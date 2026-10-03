# Optional regression tests. PowerShell 5.1/7; no Pester/downloads required.
# Runs helpers from the local build script, NOT the compiler or game.
[CmdletBinding()]
param(
    [string]$BuildScript = (Join-Path $PSScriptRoot '..\build.ps1')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$BuildScript = [IO.Path]::GetFullPath($BuildScript)
$ProjectRoot = Split-Path -Parent $BuildScript
$Tokens = $null
$ParseErrors = $null
$Ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $BuildScript, [ref]$Tokens, [ref]$ParseErrors)
if (@($ParseErrors).Count -gt 0) {
    throw ('PowerShell parse errors: ' + (($ParseErrors | ForEach-Object { $_.Message }) -join '; '))
}
# Load only these trusted function definitions. Never run the build main block.
foreach ($Name in @('Read-BuildSettings','Assert-SourceTree','Get-VSBuildProfile')) {
    $Matches = @($Ast.FindAll({
        param($Node)
        $Node -is [System.Management.Automation.Language.FunctionDefinitionAst]
    }, $true) | Where-Object { $_.Name -eq $Name })
    if ($Matches.Count -ne 1) { throw ('Expected one helper: ' + $Name) }
    . ([ScriptBlock]::Create($Matches[0].Extent.Text))
}

$TempRoot = Join-Path ([IO.Path]::GetTempPath()) ('fc-settings-tests-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($TempRoot) | Out-Null
$script:Results = New-Object System.Collections.ArrayList
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Write-Fixture([string]$Folder, [string]$Text) {
    [IO.File]::WriteAllText((Join-Path $Folder 'build-settings.json'), $Text,
        (New-Object Text.UTF8Encoding($false)))
}
function Expect-Failure([ScriptBlock]$Body, [string]$Pattern) {
    $Failed = $false
    try { & $Body } catch {
        $Failed = $true
        Assert-True ($_.Exception.Message -like $Pattern) ('Unexpected error: ' + $_.Exception.Message)
    }
    Assert-True $Failed 'Expected a failure, but the operation succeeded.'
}
function Test-Case([string]$Name, [ScriptBlock]$Body) {
    $CaseRoot = Join-Path $TempRoot ([Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($CaseRoot) | Out-Null
    try {
        & $Body $CaseRoot
        [void]$script:Results.Add([PSCustomObject]@{name=$Name; passed=$true})
        Write-Host ('PASS: ' + $Name)
    } catch {
        [void]$script:Results.Add([PSCustomObject]@{name=$Name; passed=$false})
        Write-Host ('FAIL: ' + $Name + ' / ' + $_.Exception.Message)
    }
}
try {
    Test-Case 'Missing settings created without a template' {
        param($Dir)
        $S = Read-BuildSettings $Dir
        Assert-True ($S.CommonLibPath -eq '..\CommonLibSSE-NG') 'Incorrect CommonLib default.'
        Assert-True ($S.Jobs -eq 4) 'Incorrect Jobs default.'
        Assert-True ($S.VisualStudioPath -eq '' -and $S.CMakePath -eq '' -and $S.VcpkgRoot -eq '') 'Auto-detection defaults missing.'
        $File = Join-Path $Dir 'build-settings.json'
        Assert-True (Test-Path -LiteralPath $File -PathType Leaf) 'Settings not created.'
        $Data = [IO.File]::ReadAllBytes($File)
        Assert-True (@($Data | Where-Object { $_ -gt 127 }).Count -eq 0) 'Default file is not ASCII.'
        $Text = [IO.File]::ReadAllText($File)
        Assert-True (-not $Text.Replace("`r`n",'').Contains("`n")) 'Expected CRLF.'
    }
    Test-Case 'Existing complete settings are unchanged on repeated reads' {
        param($Dir)
        $null = Read-BuildSettings $Dir
        $File = Join-Path $Dir 'build-settings.json'
        $Before = [Convert]::ToBase64String([IO.File]::ReadAllBytes($File))
        $Stamp = [IO.File]::GetLastWriteTimeUtc($File)
        $null = Read-BuildSettings $Dir
        Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($File)) -eq $Before) 'Existing bytes changed.'
        Assert-True ([IO.File]::GetLastWriteTimeUtc($File) -eq $Stamp) 'Existing file was rewritten.'
    }
    Test-Case 'Partial old file inherits missing properties without rewrite' {
        param($Dir)
        $Content = '{"VcpkgRoot":"D:\\My Tools\\vcpkg"}'
        Write-Fixture $Dir $Content
        $S = Read-BuildSettings $Dir
        Assert-True ($S.VcpkgRoot -eq 'D:\My Tools\vcpkg') 'Custom path lost.'
        Assert-True ($S.CMakePath -eq '' -and $S.VisualStudioPath -eq '' -and $S.Jobs -eq 4) 'Missing fields not inherited.'
        Assert-True ([IO.File]::ReadAllText((Join-Path $Dir 'build-settings.json')) -eq $Content) 'Partial settings overwritten.'
    }
    Test-Case 'Empty file uses defaults without rewrite' {
        param($Dir)
        Write-Fixture $Dir ''
        $S = Read-BuildSettings $Dir
        Assert-True ($S.Jobs -eq 4) 'Empty-file fallback missing.'
        Assert-True ((Get-Item -LiteralPath (Join-Path $Dir 'build-settings.json')).Length -eq 0) 'Empty file changed.'
    }
    Test-Case 'Whitespace-only file uses defaults' {
        param($Dir)
        Write-Fixture $Dir " `r`n`t "
        Assert-True ((Read-BuildSettings $Dir).Jobs -eq 4) 'Whitespace fallback missing.'
    }
    Test-Case 'Empty JSON object uses defaults' {
        param($Dir)
        Write-Fixture $Dir '{}'
        Assert-True ((Read-BuildSettings $Dir).CommonLibPath -eq '..\CommonLibSSE-NG') 'Default missing.'
    }
    Test-Case 'Null properties use defaults' {
        param($Dir)
        Write-Fixture $Dir '{"CommonLibPath":null,"VisualStudioPath":null,"Jobs":null}'
        $S = Read-BuildSettings $Dir
        Assert-True ($S.Jobs -eq 4 -and $S.VisualStudioPath -eq '' -and $S.CommonLibPath -eq '..\CommonLibSSE-NG') 'Null defaults failed.'
    }
    Test-Case 'Blank CommonLib path uses sibling directory' {
        param($Dir)
        Write-Fixture $Dir '{"CommonLibPath":"   "}'
        Assert-True ((Read-BuildSettings $Dir).CommonLibPath -eq '..\CommonLibSSE-NG') 'Blank path failed.'
    }
    Test-Case 'Integer string Jobs accepted' {
        param($Dir)
        Write-Fixture $Dir '{"Jobs":"6"}'
        Assert-True ((Read-BuildSettings $Dir).Jobs -eq 6) 'Integer string Jobs failed.'
    }
    Test-Case 'Jobs clamped low' {
        param($Dir)
        Write-Fixture $Dir '{"Jobs":0}'
        Assert-True ((Read-BuildSettings $Dir).Jobs -eq 1) 'Lower bound failed.'
    }
    Test-Case 'Jobs clamped high' {
        param($Dir)
        Write-Fixture $Dir '{"Jobs":1000}'
        Assert-True ((Read-BuildSettings $Dir).Jobs -eq 8) 'Upper bound failed.'
    }
    Test-Case 'Invalid Jobs rejected' {
        param($Dir)
        Write-Fixture $Dir '{"Jobs":"all"}'
        Expect-Failure { Read-BuildSettings $Dir } '*Jobs must be an integer*'
    }
    Test-Case 'Non-string path rejected' {
        param($Dir)
        Write-Fixture $Dir '{"CommonLibPath":42}'
        Expect-Failure { Read-BuildSettings $Dir } '*must be a path string*'
    }
    Test-Case 'Invalid JSON preserved and explained' {
        param($Dir)
        $Content = '{"Jobs":'
        Write-Fixture $Dir $Content
        Expect-Failure { Read-BuildSettings $Dir } '*Invalid JSON*Nothing was overwritten*'
        Assert-True ([IO.File]::ReadAllText((Join-Path $Dir 'build-settings.json')) -eq $Content) 'Malformed settings were overwritten.'
    }
    foreach ($JsonValue in @('null','[]','[{}]','42','"text"')) {
        $script:JsonValueForTest = $JsonValue
        Test-Case ('Non-object root rejected: ' + $JsonValue) {
            param($Dir)
            Write-Fixture $Dir $script:JsonValueForTest
            Expect-Failure { Read-BuildSettings $Dir } '*must contain a JSON object*'
        }
    }
    Test-Case 'Settings path occupied by a directory' {
        param($Dir)
        [IO.Directory]::CreateDirectory((Join-Path $Dir 'build-settings.json')) | Out-Null
        Expect-Failure { Read-BuildSettings $Dir } '*Expected a settings FILE*'
    }
    Test-Case 'Literal paths: spaces, brackets, punctuation, Unicode' {
        param($Dir)
        $Special = Join-Path $Dir ('source [test] ! & (x) % $ # ' + [char]0x4E2D + [char]0x6587)
        [IO.Directory]::CreateDirectory($Special) | Out-Null
        $S = Read-BuildSettings $Special
        Assert-True ($S.Jobs -eq 4) 'Special directory default failed.'
        $Custom = Join-Path $Special 'My vcpkg [2]'
        Write-Fixture $Special (@{VcpkgRoot=$Custom} | ConvertTo-Json)
        Assert-True ((Read-BuildSettings $Special).VcpkgRoot -eq $Custom) 'Unicode/custom path not preserved.'
    }
    Test-Case 'Incomplete source tree rejected before tool discovery' {
        param($Dir)
        Expect-Failure { Assert-SourceTree $Dir } '*Incomplete SOURCE folder*FULL FreedomControl source ZIP*'
    }
    Test-Case 'Full delivered source tree passes file preflight' {
        param($Dir)
        Assert-SourceTree $ProjectRoot
    }
    Test-Case 'VS2026 generator profile' {
        param($Dir)
        $P = Get-VSBuildProfile 18
        Assert-True ($P.Generator -eq 'Visual Studio 18 2026') 'Wrong VS2026 generator.'
        Assert-True ($P.MinimumCMake -eq [Version]'4.2.0') 'Wrong minimum CMake.'
    }
    Test-Case 'VS2022 generator profile' {
        param($Dir)
        Assert-True ((Get-VSBuildProfile 17).Generator -eq 'Visual Studio 17 2022') 'Wrong VS2022 generator.'
    }
    Test-Case 'Unsupported Visual Studio major rejected' {
        param($Dir)
        Expect-Failure { Get-VSBuildProfile 16 } '*Unsupported Visual Studio major version*'
    }
} finally {
    # Remove only the unique temporary fixture directory created above.
    if (Test-Path -LiteralPath $TempRoot -PathType Container) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
}
$Failed = @($script:Results | Where-Object { -not $_.passed }).Count
Write-Host ('Settings/helper tests: ' + ($script:Results.Count - $Failed) + '/' + $script:Results.Count + ' passed.')
Write-Host 'These tests do NOT execute BUILD.bat, MSVC, native VS discovery, or Skyrim.'
if ($Failed -gt 0) { exit 1 }
exit 0
