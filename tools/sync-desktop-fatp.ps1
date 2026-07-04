# Fast-forward Desktop FatP clone to match origin/main (after agent worktree pushes).
#
# Usage (from any FatP clone/worktree):
#   .\tools\sync-desktop-fatp.ps1
#   .\tools\sync-desktop-fatp.ps1 -Force   # discard Desktop local edits
#
# Override path: $env:FATP_DESKTOP_PATH = 'D:\other\FatP'

param(
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$desktop = $env:FATP_DESKTOP_PATH
if (-not $desktop) {
    $desktop = 'C:\Users\mtthw\Desktop\AI Projects\FatP'
}

if (-not (Test-Path -LiteralPath $desktop)) {
    Write-Warning "Desktop FatP not found (skipped): $desktop"
    exit 0
}

if (-not (Test-Path -LiteralPath (Join-Path $desktop '.git'))) {
    Write-Warning "Not a git repo (skipped): $desktop"
    exit 0
}

$dirty = git -C $desktop status --porcelain
if ($dirty -and -not $Force) {
    throw @"
Desktop FatP has uncommitted changes — sync aborted.
  Path: $desktop
  Fix: commit or stash on Desktop, or re-run with -Force to reset to origin/main.
"@
}

Write-Host "Syncing Desktop FatP: $desktop"

git -C $desktop fetch origin main
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Force -and $dirty) {
    git -C $desktop reset --hard origin/main
}
else {
    git -C $desktop merge --ff-only origin/main
}

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$sha = git -C $desktop rev-parse --short HEAD
Write-Host "Desktop FatP is at $sha (origin/main)."