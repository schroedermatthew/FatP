# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/build.ps1
#   summary: "Windows PowerShell build script for CMake configuration and compilation."
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

cmake --build --preset release
ctest --preset release
