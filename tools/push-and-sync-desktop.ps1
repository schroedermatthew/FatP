# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/push-and-sync-desktop.ps1
#   summary: "git push then sync Desktop FatP clone."
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
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GitArgs = @('origin', 'main')
)

$ErrorActionPreference = 'Stop'

git push @GitArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot 'sync-desktop-fatp.ps1')
exit $LASTEXITCODE