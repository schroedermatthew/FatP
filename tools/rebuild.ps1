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
