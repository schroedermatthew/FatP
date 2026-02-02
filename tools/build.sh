#!/bin/bash
set -e

# Verify we're in project root
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Run from project root (where CMakeLists.txt is located)"
    exit 1
fi

cmake --build --preset release
ctest --preset release
