# git push then fast-forward Desktop FatP. Use from any FatP clone/worktree.
#
# Usage (from repo root):
#   .\tools\push-and-sync-desktop.ps1
#   .\tools\push-and-sync-desktop.ps1 origin main

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GitArgs = @('origin', 'main')
)

$ErrorActionPreference = 'Stop'

git push @GitArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot 'sync-desktop-fatp.ps1')
exit $LASTEXITCODE