$ErrorActionPreference = "Stop"

Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake --preset default
cmake --build --preset release
ctest --preset release
