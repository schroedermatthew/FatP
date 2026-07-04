# git add, commit, push, then fast-forward Desktop FatP. Use from any FatP clone/worktree.
#
# Usage (from repo root):
#   .\tools\commit-push-sync-desktop.ps1 -Message "Fix XmlLite parser"
#   .\tools\commit-push-sync-desktop.ps1 -Message "Fix XmlLite parser" -All
#   .\tools\commit-push-sync-desktop.ps1 -Message "Fix XmlLite parser" -Paths include/fat_p/XmlLite.h
#   .\tools\commit-push-sync-desktop.ps1 -Message "Fix XmlLite parser" origin main

param(
    [Parameter(Mandatory = $true)]
    [string]$Message,

    [switch]$All,

    [string[]]$Paths = @(),

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GitPushArgs = @('origin', 'main')
)

$ErrorActionPreference = 'Stop'

if ($All -and $Paths.Count -gt 0) {
    throw 'Use either -All or -Paths, not both.'
}

$addPaths = @()
foreach ($entry in $Paths) {
    $addPaths += ($entry -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }
}

if ($All) {
    git add -A
}
elseif ($addPaths.Count -gt 0) {
    git add @addPaths
}
else {
    git add -u
}

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$status = git status --porcelain
if (-not $status) {
    throw 'Nothing staged to commit. Use -All or -Paths to include new files.'
}

git commit -m $Message
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

git push @GitPushArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot 'sync-desktop-fatp.ps1')
exit $LASTEXITCODE