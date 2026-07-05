# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/install-git-hooks.ps1
#   summary: "Install git hook templates from tools/git-hooks into .git/hooks."
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

$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel)
if (-not $repoRoot) { throw 'Not inside a git repository.' }

$srcDir = Join-Path $repoRoot 'tools/git-hooks'
$dstDir = Join-Path $repoRoot '.git/hooks'

if (-not (Test-Path -LiteralPath $srcDir)) {
    throw "Hook templates not found: $srcDir"
}

Get-ChildItem -LiteralPath $srcDir -File | ForEach-Object {
    $dest = Join-Path $dstDir $_.Name
    Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
    Write-Host "Installed: .git/hooks/$($_.Name)"
}

Write-Host 'Git hooks installed. post-push will sync Desktop FatP after every push.'