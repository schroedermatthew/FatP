#!/bin/bash
set -e

rm -rf build
cmake --preset default
cmake --build --preset release
ctest --preset release
