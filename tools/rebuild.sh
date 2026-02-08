#!/bin/bash
# FATP_META:
#   meta_version: 1
#   component: BuildSystem
#   file_role: tooling
#   path: tools/rebuild.sh
#   summary: "Linux/macOS clean rebuild script."
set -e

# Verify we're in project root
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Run from project root (where CMakeLists.txt is located)"
    exit 1
fi

rm -rf build
cmake --preset default
cmake --build --preset release
ctest --preset release
