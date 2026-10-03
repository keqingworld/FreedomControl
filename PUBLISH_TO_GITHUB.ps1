param(
    [string]$RepoUrl = "https://github.com/keqingworld/FreedomControl.git"
)

$ErrorActionPreference = "Stop"
$Source = Split-Path -Parent $MyInvocation.MyCommand.Path
$Temp = Join-Path $env:TEMP ("FreedomControlPublish-" + [guid]::NewGuid().ToString("N"))

function Invoke-Git {
    # Important: use PowerShell's automatic $args variable.
    # This prevents git switches such as "-A" from being parsed as PowerShell function parameters.
    & git @args
    if ($LASTEXITCODE -ne 0) {
        throw "git $($args -join ' ') failed with exit code $LASTEXITCODE"
    }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git was not found in PATH. Install Git for Windows first."
}

# If GitHub CLI is available, use it to configure Git authentication.
if (Get-Command gh -ErrorAction SilentlyContinue) {
    & gh auth status *> $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "GitHub authorization is required. A browser window may open..." -ForegroundColor Yellow
        & gh auth login --web --git-protocol https
        if ($LASTEXITCODE -ne 0) {
            throw "gh auth login failed."
        }
    }

    & gh auth setup-git
    if ($LASTEXITCODE -ne 0) {
        throw "gh auth setup-git failed."
    }
}

Write-Host "[1/5] Cloning $RepoUrl" -ForegroundColor Cyan
Invoke-Git clone --depth 1 $RepoUrl $Temp

Write-Host "[2/5] Replacing repository tree with clean source..." -ForegroundColor Cyan
Get-ChildItem -LiteralPath $Temp -Force |
    Where-Object { $_.Name -ne ".git" } |
    Remove-Item -Recurse -Force

Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $Temp -Recurse -Force
}

Push-Location $Temp
try {
    # Use GitHub noreply identity to avoid exposing a private email in commit metadata.
    Invoke-Git config user.name "莱茵多特"
    Invoke-Git config user.email "250785283+keqingworld@users.noreply.github.com"

    Write-Host "[3/5] Staging full clean source..." -ForegroundColor Cyan
    Invoke-Git add --all

    & git diff --cached --quiet
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Repository is already identical. Nothing to upload." -ForegroundColor Green
        $NoChanges = $true
    } else {
        $NoChanges = $false
    }

    if (-not $NoChanges) {
        Write-Host "[4/5] Creating commit..." -ForegroundColor Cyan
        Invoke-Git commit -m "Publish FreedomControl 0.5.5 VMARGS18 clean full source"

        Write-Host "[5/5] Pushing to main..." -ForegroundColor Cyan
        Invoke-Git push origin HEAD:main

        Write-Host ""
        Write-Host "DONE: FreedomControl 0.5.5 full clean source is on GitHub." -ForegroundColor Green
        Write-Host "https://github.com/keqingworld/FreedomControl"
    }
}
finally {
    Pop-Location
    if (Test-Path -LiteralPath $Temp) {
        Remove-Item -LiteralPath $Temp -Recurse -Force -ErrorAction SilentlyContinue
    }
}
