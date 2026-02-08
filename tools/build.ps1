# FATP_META:
#   meta_version: 1
#   component: BuildSystem
#   file_role: tooling
#   path: tools/build.ps1
#   summary: "Windows PowerShell build script for CMake configuration and compilation."
$ErrorActionPreference = "Stop"

# Verify we're in project root
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Error: Run from project root (where CMakeLists.txt is located)" -ForegroundColor Red
    exit 1
}

cmake --build --preset release
ctest --preset release
