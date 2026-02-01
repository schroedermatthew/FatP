$ErrorActionPreference = "Stop"

cmake --build --preset release
ctest --preset release
