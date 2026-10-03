# Windows PowerShell 5.1 or PowerShell 7. ASCII source; never build commands with Invoke-Expression.
[CmdletBinding()]
param([switch]$RebuildPlugin)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$TranscriptStarted = $false
$ExitCode = 1
$Phase = 'preflight'

function Full-Path([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return '' }
    $expanded = [Environment]::ExpandEnvironmentVariables($Value)
    if (-not [IO.Path]::IsPathRooted($expanded)) { $expanded = Join-Path $Root $expanded }
    return [IO.Path]::GetFullPath($expanded)
}
function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    Write-Host ('RUN: ' + $Executable)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw ('Native command failed with exit code ' + $LASTEXITCODE + ': ' + $Executable) }
}
function Find-Application([string]$Name) {
    $found = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $found) { return $found.Source }
    return ''
}


function Get-VSBuildProfile([int]$Major) {
    switch ($Major) {
        17 { return [PSCustomObject]@{ Label = 'Visual Studio 2022'; Generator = 'Visual Studio 17 2022'; MinimumCMake = [Version]'3.25.0'; Folder = 'vs2022' } }
        18 { return [PSCustomObject]@{ Label = 'Visual Studio 2026'; Generator = 'Visual Studio 18 2026'; MinimumCMake = [Version]'4.2.0'; Folder = 'vs2026' } }
        default { throw ('Unsupported Visual Studio major version: ' + $Major + '. Expected VS 2022 (17.x) or VS 2026 (18.x). Check VisualStudioPath in build-settings.json.') }
    }
}


function Read-BuildSettings([string]$ProjectRoot) {
    # This is an OPTIONAL override file. These defaults are part of the script,
    # not a second file that can be left out of a patch/archive.
    $Defaults = [ordered]@{
        CommonLibPath = '..\CommonLibSSE-NG'
        VcpkgRoot = ''
        CMakePath = ''
        VisualStudioPath = ''
        Jobs = 4
    }
    $File = Join-Path $ProjectRoot 'build-settings.json'
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
        if (Test-Path -LiteralPath $File) {
            throw ('Expected a settings FILE, but a directory exists at: ' + $File)
        }
        # CreateNew prevents overwriting an existing file, including a concurrent
        # build's settings. ASCII defaults are also valid UTF-8, with no BOM.
        $Json = ($Defaults | ConvertTo-Json -Depth 3)
        $Json = $Json.Replace("`r`n", "`n").Replace("`n", "`r`n") + "`r`n"
        $Bytes = [Text.Encoding]::ASCII.GetBytes($Json)
        $Stream = $null
        try {
            $Stream = [IO.File]::Open($File, [IO.FileMode]::CreateNew,
                [IO.FileAccess]::Write, [IO.FileShare]::Read)
            $Stream.Write($Bytes, 0, $Bytes.Length)
        } catch {
            throw ('Could not create default build-settings.json at ' + $File + ': ' + $_.Exception.Message)
        } finally {
            if ($null -ne $Stream) { $Stream.Dispose() }
        }
        Write-Host ('Created default settings: ' + $File)
    }

    $Text = Get-Content -LiteralPath $File -Raw -Encoding UTF8
    if ([string]::IsNullOrWhiteSpace($Text)) {
        Write-Warning 'build-settings.json is empty. Using built-in defaults; the existing file is unchanged.'
        return [PSCustomObject]$Defaults
    }
    try {
        $Overrides = $Text | ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw ('Invalid JSON in ' + $File + '. Nothing was overwritten. Correct the JSON, or rename this file to use defaults. Details: ' + $_.Exception.Message)
    }
    # Test the text as well: PowerShell can unwrap a one-item JSON array.
    if (-not $Text.TrimStart().StartsWith('{') -or $null -eq $Overrides -or
        $Overrides -isnot [PSCustomObject]) {
        throw ('The settings file must contain a JSON object: ' + $File + '. Nothing was overwritten.')
    }
    foreach ($Name in @($Defaults.Keys)) {
        # Do not access a missing property under Set-StrictMode. Old/partial
        # settings files are valid and inherit every omitted field.
        $Property = $Overrides.PSObject.Properties[$Name]
        if ($null -eq $Property -or $null -eq $Property.Value) { continue }
        if ($Name -eq 'Jobs') {
            $ParsedJobs = 0
            if (-not [int]::TryParse([string]$Property.Value, [ref]$ParsedJobs)) {
                throw ('Jobs must be an integer in ' + $File + '. Example: "Jobs": 4')
            }
            $Defaults[$Name] = [Math]::Max(1, [Math]::Min(8, $ParsedJobs))
        } else {
            if ($Property.Value -isnot [string]) {
                throw ($Name + ' must be a path string or an empty string in ' + $File)
            }
            $Value = ([string]$Property.Value).Trim().Trim('"')
            if (-not [string]::IsNullOrWhiteSpace($Value)) { $Defaults[$Name] = $Value }
        }
    }
    return [PSCustomObject]$Defaults
}

function Assert-SourceTree([string]$ProjectRoot) {
    $RequiredFiles = @(
        'CMakeLists.txt', 'vcpkg.json', 'include\fc\Core.hpp',
        'src\Plugin.cpp', 'src\PCH.h', 'src\Engine.cpp', 'src\Engine.hpp',
        'src\Overlay.cpp', 'src\Overlay.hpp', 'src\Core.cpp', 'src\Legion.cpp', 'include\fc\LegionPolicy.hpp',
        'tests\CMakeLists.txt', 'tests\core_tests.cpp', 'tests\runtime_tests.cpp', 'tests\legion_tests.cpp',
        'include\fc\HotkeyState.hpp', 'include\fc\RuntimePolicy.hpp',
        'package\SKSE\Plugins\FreedomControl.ini',
        'README_zh-CN.md', 'FEATURES_zh-CN.md', 'BUILD_STATUS.md', 'LICENSE.txt',
        'docs\API_SOURCES.md', 'docs\TESTING_zh-CN.md',
        'cmake\PackageRuntime.cmake', 'cmake\SourceIdentity.cmake',
        'cmake\ResetPluginCache.cmake', 'LATEST_BUILD_ID.txt',
        'src\BuildIdentity.cpp', 'src\BuildIdentity.hpp',
        'src\Follower10.cpp', 'src\Spawner10.cpp', 'src\WorldFreedom10.cpp', 'src\Runtime10.hpp',
        'include\fc\FreedomPolicy.hpp', 'tests\freedom_tests.cpp',
        'src\Kernel11.cpp', 'src\PlayerFreedom14.cpp', 'include\fc\PlayerFreedomPolicy14.hpp',
        'include\fc\CreatureFollowPolicy.hpp', 'tests\player_freedom_tests.cpp',
        'src\PlayerCustomization.cpp', 'include\fc\PlayerCustomization.hpp',
        'src\Input13.cpp', 'src\Input13.hpp', 'src\GameInput13.cpp', 'include\fc\InputPolicy13.hpp', 'tests\input_policy_tests.cpp',
        'src\QuestCenter11.cpp', 'src\KernelSave11.cpp', 'src\SexLab16.cpp',
        'src\CrimeHooks11.cpp', 'src\CrimeHooks11.hpp', 'include\fc\KernelPolicy.hpp', 'tests\kernel_tests.cpp',
        'package\FreedomControlRuntime.esp', 'package\SKSE\Plugins\FreedomControl.creatures.json'
    )
    $Missing = @($RequiredFiles | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $ProjectRoot $_) -PathType Leaf)
    })
    if ($Missing.Count -gt 0) {
        throw ('Incomplete SOURCE folder: ' + $ProjectRoot + '. Missing: ' + ($Missing -join ', ') +
            '. Extract the FULL FreedomControl source ZIP into the parent folder and merge/replace files; do not use a build-only patch as the whole project.')
    }
}

try {
    Set-Location -LiteralPath $Root
    Start-Transcript -LiteralPath (Join-Path $Root 'build.log') -Force | Out-Null
    $TranscriptStarted = $true
    Write-Host 'FreedomControl 0.5.5 VMARGS18 - Windows x64 / Skyrim 1.6.1170'
    Write-Host 'Build-script revision: VMARGS18 + COMPILE17 + SexFast16 + Repair15 + Buildfix8 (VS 2022 / VS 2026)'
    Write-Host 'This builds SOURCE. It does not install anything into your game.'
    Write-Host 'The first build downloads vcpkg dependencies and compiles CommonLib from source.'
    Write-Host ''
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
    if (-not [Environment]::Is64BitProcess) { throw 'Run BUILD.bat from normal 64-bit Windows Explorer.' }

    Assert-SourceTree $Root
    $Settings = Read-BuildSettings $Root
    $CommonLib = Full-Path ([string]$Settings.CommonLibPath)
    if (-not (Test-Path -LiteralPath (Join-Path $CommonLib 'CMakeLists.txt') -PathType Leaf)) {
        throw ('CommonLib checkout not found: ' + $CommonLib + '. Finish the Git clone, or set CommonLibPath in build-settings.json.')
    }
    if (-not (Test-Path -LiteralPath (Join-Path $CommonLib 'extern\openvr\headers\openvr.h') -PathType Leaf)) {
        throw 'OpenVR submodule headers are missing. Let your existing recursive Git clone finish, then run BUILD.bat again.'
    }

    # Match the actual VS major version, not the year embedded in a folder name.
    # vswhere includes Community, Professional, Enterprise and standalone Build Tools.
    $VsPath = Full-Path ([string]$Settings.VisualStudioPath)
    $VsWhere = ''
    if (${env:ProgramFiles(x86)}) {
        $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    }
    if (-not $VsWhere -or -not (Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
        $VsWhere = Find-Application 'vswhere.exe'
    }
    $VsMajor = 0
    $VsLabel = ''
    if ($VsWhere) {
        $VsArgs = @('-products','*','-version','[17.0,19.0)','-prerelease','-sort',
            '-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-format','json','-utf8')
        # vswhere emits UTF-8 JSON; decode it explicitly even in Windows PowerShell 5.1.
        $SavedOutputEncoding = [Console]::OutputEncoding
        try {
            [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
            $VsJson = @(& $VsWhere @VsArgs)
            if ($LASTEXITCODE -ne 0) { throw 'vswhere failed while enumerating C++ installations.' }
        } finally {
            [Console]::OutputEncoding = $SavedOutputEncoding
        }
        $Instances = @(($VsJson -join [Environment]::NewLine) | ConvertFrom-Json | ForEach-Object { $_ })
        $ChosenVs = $null
        if ($VsPath) {
            foreach ($Instance in $Instances) {
                $InstalledPath = Full-Path ([string]$Instance.installationPath)
                if ($InstalledPath.TrimEnd('\') -ieq $VsPath.TrimEnd('\')) {
                    $ChosenVs = $Instance
                    break
                }
            }
        } else {
            # Prefer stable installations; use an Insiders/Preview installation if needed.
            $Stable = @($Instances | Where-Object { -not $_.isPrerelease })
            if ($Stable.Count -gt 0) { $ChosenVs = $Stable[0] }
            elseif ($Instances.Count -gt 0) { $ChosenVs = $Instances[0] }
        }
        if ($null -ne $ChosenVs) {
            $VsPath = Full-Path ([string]$ChosenVs.installationPath)
            $VsMajor = ([Version]([string]$ChosenVs.installationVersion)).Major
            $VsLabel = [string]$ChosenVs.displayName
        }
    }
    if (-not $VsPath) {
        throw 'No VS 2022/2026 installation with C++ x64 tools was found. In Visual Studio Installer, check Desktop development with C++ and the Windows SDK. Or set VisualStudioPath in build-settings.json.'
    }
    # Permit an explicit install path even when vswhere is unavailable or its component
    # metadata is incomplete. Read the executable version; never guess from the path.
    if ($VsMajor -eq 0) {
        foreach ($Relative in @('MSBuild\Current\Bin\MSBuild.exe','Common7\IDE\devenv.exe')) {
            $VersionFile = Join-Path $VsPath $Relative
            if (Test-Path -LiteralPath $VersionFile -PathType Leaf) {
                $FileMajor = [Diagnostics.FileVersionInfo]::GetVersionInfo($VersionFile).FileMajorPart
                if ($FileMajor -in @(17,18)) { $VsMajor = $FileMajor; break }
            }
        }
    }
    $VsProfile = Get-VSBuildProfile $VsMajor
    $Generator = $VsProfile.Generator
    $MinCMake = $VsProfile.MinimumCMake
    if (-not $VsLabel) { $VsLabel = $VsProfile.Label }
    $ToolsetRoot = Join-Path $VsPath 'VC\Tools\MSVC'
    $HasX64Compiler = $false
    if (Test-Path -LiteralPath $ToolsetRoot -PathType Container) {
        foreach ($ToolsetDirectory in @(Get-ChildItem -LiteralPath $ToolsetRoot -Directory)) {
            if (Test-Path -LiteralPath (Join-Path $ToolsetDirectory.FullName 'bin\Hostx64\x64\cl.exe') -PathType Leaf) {
                $HasX64Compiler = $true
                break
            }
        }
    }
    if (-not $HasX64Compiler) {
        throw ('No x64 MSVC compiler was found under: ' + $VsPath + '. Add the C++ desktop tools to THIS Visual Studio installation.')
    }

    # VS 2026 requires the VS 18 generator, introduced in CMake 4.2. Do not select an
    # older PATH copy merely because it exists. A user-configured CMakePath is explicit.
    $CMakeOverride = Full-Path ([string]$Settings.CMakePath)
    $CMakeCandidates = @()
    if ($CMakeOverride) {
        $CMakeCandidates += $CMakeOverride
    } else {
        $CMakeCandidates += (Join-Path $VsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
        $CMakeCommands = @(Get-Command 'cmake.exe' -All -CommandType Application -ErrorAction SilentlyContinue)
        foreach ($Command in $CMakeCommands) { $CMakeCandidates += $Command.Source }
        if ($env:ProgramFiles) { $CMakeCandidates += (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe') }
    }
    $CMake = ''
    $CTest = ''
    $CMakeVersion = ''
    foreach ($Candidate in @($CMakeCandidates | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) { continue }
        $Capabilities = $null
        try {
            $CapsOutput = @(& $Candidate -E capabilities)
            if ($LASTEXITCODE -ne 0) { throw 'CMake capability query failed.' }
            $Capabilities = ($CapsOutput -join [Environment]::NewLine) | ConvertFrom-Json
            $CandidateVersion = [Version]('{0}.{1}.{2}' -f $Capabilities.version.major,$Capabilities.version.minor,$Capabilities.version.patch)
            $CandidateCTest = Join-Path (Split-Path -Parent $Candidate) 'ctest.exe'
            $SupportsGenerator = @($Capabilities.generators | Where-Object { $_.name -eq $Generator }).Count -gt 0
            if ($CandidateVersion -lt $MinCMake -or -not $SupportsGenerator) {
                Write-Host ('SKIP CMake ' + $CandidateVersion + ' (needs ' + $Generator + ' / CMake >= ' + $MinCMake + '): ' + $Candidate)
                continue
            }
            if (-not (Test-Path -LiteralPath $CandidateCTest -PathType Leaf)) {
                Write-Host ('SKIP CMake without ctest.exe: ' + $Candidate)
                continue
            }
            $CMake = $Candidate
            $CTest = $CandidateCTest
            $CMakeVersion = [string]$Capabilities.version.string
            break
        } catch {
            Write-Host ('SKIP unusable CMake: ' + $Candidate + ' (' + $_.Exception.Message + ')')
        }
    }
    if (-not $CMake) {
        throw ('No compatible CMake/CTest pair was found. ' + $VsProfile.Label + ' needs CMake >= ' + $MinCMake + ' with generator "' + $Generator + '". Update C++ CMake tools for Windows in Visual Studio Installer, or set CMakePath to a newer cmake.exe. No VS downgrade is required.')
    }

    $Vcpkg = Full-Path ([string]$Settings.VcpkgRoot)
    if (-not $Vcpkg) {
        $Candidates = @()
        if ($env:VCPKG_ROOT) { $Candidates += $env:VCPKG_ROOT }
        $VcpkgExe = Find-Application 'vcpkg.exe'
        if ($VcpkgExe) { $Candidates += (Split-Path -Parent $VcpkgExe) }
        $Candidates += @((Join-Path $VsPath 'VC\vcpkg'), (Join-Path (Split-Path -Parent $Root) 'vcpkg'), 'C:\vcpkg', 'D:\vcpkg', 'C:\src\vcpkg', 'D:\src\vcpkg')
        foreach ($Candidate in $Candidates) {
            if (Test-Path -LiteralPath (Join-Path $Candidate 'scripts\buildsystems\vcpkg.cmake') -PathType Leaf) { $Vcpkg = Full-Path $Candidate; break }
        }
    }
    if (-not $Vcpkg) {
        Write-Host 'Enter the existing vcpkg ROOT folder. Do not enter vcpkg.exe.'
        $Vcpkg = Full-Path ((Read-Host 'VcpkgRoot').Trim().Trim('"'))
    }
    if (-not $Vcpkg) { throw 'No vcpkg path supplied.' }
    $Toolchain = Join-Path $Vcpkg 'scripts\buildsystems\vcpkg.cmake'
    if (-not (Test-Path -LiteralPath $Toolchain -PathType Leaf)) { throw ('Invalid vcpkg root: ' + $Vcpkg) }
    $env:VCPKG_ROOT = $Vcpkg
    $env:VCPKG_VISUAL_STUDIO_PATH = $VsPath
    $env:VCPKG_DISABLE_METRICS = '1'
    $Jobs = [Math]::Max(1,[Math]::Min(8,[int]$Settings.Jobs))
    # Separate caches by generator; leave any old VS2022 build directory untouched.
    $Build = Join-Path (Join-Path $Root 'build') $VsProfile.Folder
    $Stage = Join-Path $Root 'dist\MO2'
    $Dist = Join-Path $Root 'dist'
    $Zip = Join-Path $Dist 'FreedomControl-0.5.5-VMARGS18-MO2.zip'

    Write-Host ('Source      : ' + $Root)
    Write-Host ('CommonLib   : ' + $CommonLib)
    Write-Host ('VisualStudio: ' + $VsLabel + ' / ' + $VsPath)
    Write-Host ('Generator   : ' + $Generator + ' / x64 / native default MSVC toolset')
    Write-Host ('Build folder: ' + $Build)
    Write-Host ('CMake       : ' + $CMakeVersion + ' / ' + $CMake)
    Write-Host ('vcpkg       : ' + $Vcpkg)
    Invoke-Checked $CMake @('--version')

    $Git = Find-Application 'git.exe'
    if (-not $Git) { throw 'Git is not on PATH. Reopen Windows after installing Git, or add its cmd folder to PATH.' }
    $Revision = @(& $Git -C $CommonLib rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $Revision.Count -eq 0) { throw 'Cannot read the CommonLib Git revision. Wait for the clone to complete.' }
    $Commit = ([string]$Revision[0]).Trim()
    $Changes = @(& $Git -C $CommonLib status --porcelain --untracked-files=no)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the CommonLib working tree.' }
    if ($Changes.Count -gt 0) { Write-Warning 'CommonLib has local modifications; its commit ID alone does not identify the exact sources.' }
    $LockPath = Join-Path $Root 'commonlib.lock.txt'
    if (Test-Path -LiteralPath $LockPath) {
        $LockedCommit = (Get-Content -LiteralPath $LockPath -Raw).Trim()
        if ($LockedCommit -ne $Commit) { throw 'CommonLib revision changed since the first build. Review the change; remove commonlib.lock.txt and the build folder only when intentionally rebuilding against new sources.' }
    } else { [IO.File]::WriteAllText($LockPath,$Commit+[Environment]::NewLine,[Text.Encoding]::ASCII) }

    # Each -D definition is ONE argument. No shell-expanded command strings, CALL, or codepage changes.
    # Use this VS installation's native default toolset (v145 on VS 2026,
    # v143 on VS 2022). Do not require v143 to be installed in VS 2026.
    $Configure = @('-S',$Root,'-B',$Build,'-G',$Generator,'-A','x64','-T','host=x64',
        ('-DCMAKE_GENERATOR_INSTANCE='+$VsPath),('-DCMAKE_TOOLCHAIN_FILE='+$Toolchain),
        ('-DCOMMONLIBSSE_PATH='+$CommonLib),'-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',
        '-DVCPKG_HOST_TRIPLET=x64-windows','-DVCPKG_MANIFEST_MODE=ON','-DFC_BUILD_PORTABLE_TESTS=ON')
    $Phase = 'configure / dependencies'
    Invoke-Checked $CMake $Configure
    if ($RebuildPlugin) {
        $Phase = 'plugin-only incremental-cache repair'
        Invoke-Checked $CMake @(('-DFC_ROOT='+$Root),('-DFC_BUILD='+$Build),
            '-P',(Join-Path $Root 'cmake\ResetPluginCache.cmake'))
    }
    $Phase = 'C++ compile / link'
    Invoke-Checked $CMake @('--build',$Build,'--config','Release','--target','FreedomControl','fc_core_tests','fc_runtime_tests','fc_legion_tests','fc_freedom_tests','fc_kernel_tests','fc_input_tests','fc_player_freedom_tests','--parallel',([string]$Jobs))
    $Phase = 'portable unit tests'
    Invoke-Checked $CTest @('--test-dir',$Build,'-C','Release','--output-on-failure','-R','^fc_(core|runtime|legion|freedom|kernel|input|player_freedom)$')

    # All packaging is done by the same independently testable CMake script as
    # PACKAGE_ONLY.bat. No Get-FileHash or optional PowerShell compression modules.
    $Phase = 'packaging (DLL and CTest already succeeded)'
    Invoke-Checked $CMake @(('-DFC_ROOT='+$Root),('-DFC_BUILD='+$Build),
        '-DFC_TEST_STATUS=PASSED_IN_THIS_BUILD: fc_core + fc_runtime + fc_legion + fc_freedom + fc_kernel + fc_input + fc_player_freedom (engine-independent only)',
        '-P',(Join-Path $Root 'cmake\PackageRuntime.cmake'))
    Write-Host ''
    Write-Host 'BUILD COMPLETE. Install THIS ZIP with MO2:' -ForegroundColor Green
    Write-Host $Zip
    Write-Host 'Enable FreedomControlRuntime.esp in the MO2 right-side Plugins tab.'
    Write-Host 'Start matching SKSE through MO2. Default menu key: F8.'
    Write-Host 'Source compile success does not establish in-game compatibility.'
    $ExitCode = 0
} catch {
    Write-Host ''
    Write-Host ('FAILED during ' + $Phase + ': ' + $_.Exception.Message) -ForegroundColor Red
    if ($null -ne $_.InvocationInfo) { Write-Host ('Script line: ' + $_.InvocationInfo.ScriptLineNumber) }
    if ($_.ScriptStackTrace) { Write-Host $_.ScriptStackTrace }
    if ($Phase -like 'packaging*') {
        Write-Host 'The DLL and unit tests succeeded. Do not delete build or dependency caches.'
        Write-Host 'For FC_IDENTITY_MISMATCH use REPAIR_BUILD8.bat. PACKAGE_ONLY cannot repair a stale DLL.'
    }
    Write-Host 'Read build.log for the failing stage and its first error.'
    Write-Host 'No game files have been modified by this script.'
    $ExitCode = 1
} finally {
    if ($TranscriptStarted) {
        try { Stop-Transcript | Out-Null } catch { Write-Warning 'Could not close the build transcript.' }
    }
}
exit $ExitCode
