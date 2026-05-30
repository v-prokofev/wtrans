# =============================================================================
# install_hooks.ps1  —  Install git hooks for WindTrans project
#
# Run once after cloning the repo:
#   cd c:\src\VNIIM\wtrans
#   .\scripts\install_hooks.ps1
# =============================================================================

$RepoRoot  = Split-Path $PSScriptRoot -Parent
$HooksSrc  = Join-Path $PSScriptRoot "hooks"
$HooksDest = Join-Path $RepoRoot ".git\hooks"

Write-Host "Installing git hooks from $HooksSrc -> $HooksDest" -ForegroundColor Cyan

if (-not (Test-Path $HooksDest)) {
    Write-Error ".git\hooks directory not found. Are you in a git repository?"
    exit 1
}

foreach ($hook in Get-ChildItem -Path $HooksSrc -File) {
    $dest = Join-Path $HooksDest $hook.Name
    Copy-Item $hook.FullName $dest -Force
    Write-Host "  Installed: $($hook.Name)" -ForegroundColor Green
}

# Set executable bit via git (needed for Git Bash on Windows)
Push-Location $RepoRoot
try {
    git update-index --chmod=+x "scripts/hooks/pre-commit" 2>$null
} catch {}
Pop-Location

Write-Host ""
Write-Host "Done. Hooks installed successfully." -ForegroundColor Green
Write-Host "FW_VERSION_BUILD will auto-increment on every commit." -ForegroundColor Yellow
