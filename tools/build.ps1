$ErrorActionPreference = "Stop"

# Verify we're in project root
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Error: Run from project root (where CMakeLists.txt is located)" -ForegroundColor Red
    exit 1
}

cmake --build --preset release
ctest --preset release
