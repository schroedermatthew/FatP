#!/bin/bash
# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/build.sh
#   summary: "Linux/macOS build script for CMake configuration and compilation."
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
set -e

# Verify we're in project root
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Run from project root (where CMakeLists.txt is located)"
    exit 1
fi

cmake --build --preset release
ctest --preset release
