# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/rebuild.ps1
#   summary: "Windows PowerShell clean rebuild script."
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

$ErrorActionPreference = "Stop"

# Verify we're in project root
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Error: Run from project root (where CMakeLists.txt is located)" -ForegroundColor Red
    exit 1
}

Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake --preset default
cmake --build --preset release
ctest --preset release
