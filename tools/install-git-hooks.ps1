# Copy repo git hook templates into .git/hooks for this clone/worktree.
#
# Usage (from repo root):
#   .\tools\install-git-hooks.ps1

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