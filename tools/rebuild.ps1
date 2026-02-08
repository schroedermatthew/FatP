# FATP_META:
#   meta_version: 1
#   component: BuildSystem
#   file_role: tooling
#   path: tools/rebuild.ps1
#   summary: "Windows PowerShell clean rebuild script."
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
