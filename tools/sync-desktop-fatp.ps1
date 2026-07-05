# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/sync-desktop-fatp.ps1
#   summary: "Fast-forward Desktop FatP clone to match origin/main."
#   api_stability: in_work
#   layer: Infrastructure
#   related:
#     docs_search: ""
#     tests: []
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false

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

Write-Host "Syncing Desktop FatP: $desktop"

git -C $desktop fetch origin main
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$localSha = git -C $desktop rev-parse HEAD
$remoteSha = git -C $desktop rev-parse origin/main

if ($localSha -eq $remoteSha) {
    $shortSha = git -C $desktop rev-parse --short HEAD
    Write-Host "Desktop FatP is at $shortSha (origin/main)."
    exit 0
}

$dirty = git -C $desktop status --porcelain
if ($dirty -and -not $Force) {
    throw @"
Desktop FatP has uncommitted changes — sync aborted.
  Path: $desktop
  Local:  $localSha
  Remote: $remoteSha
  Fix: commit or stash on Desktop, or re-run with -Force to reset to origin/main.
"@
}

if ($Force -and $dirty) {
    git -C $desktop reset --hard origin/main
}
else {
    git -C $desktop merge --ff-only origin/main
}

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$sha = git -C $desktop rev-parse --short HEAD
Write-Host "Desktop FatP is at $sha (origin/main)."