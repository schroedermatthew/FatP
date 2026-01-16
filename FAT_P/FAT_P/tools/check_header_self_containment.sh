#!/bin/bash
#
# check_header_self_containment.sh
#
# CI script to verify all Fat-P headers are self-contained.
# Each header must compile standalone without requiring other headers first.
#
# Usage:
#     ./check_header_self_containment.sh [OPTIONS]
#
# Options:
#     -c, --compiler CXX    C++ compiler (default: g++)
#     -s, --std STD         C++ standard (default: c++20)
#     -I, --include-dir DIR Include directory (default: fat_p)
#     -v, --verbose         Print verbose output
#     -h, --help            Show this help
#
# Exit codes:
#     0 - All headers are self-contained
#     1 - One or more headers failed to compile standalone
#

set -euo pipefail

# Defaults
COMPILER="g++"
STD="c++20"
INCLUDE_DIR="fat_p"
VERBOSE=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

usage() {
    sed -n '2,/^$/p' "$0" | sed 's/^# //' | sed 's/^#//'
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--compiler)
            COMPILER="$2"
            shift 2
            ;;
        -s|--std)
            STD="$2"
            shift 2
            ;;
        -I|--include-dir)
            INCLUDE_DIR="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Check include directory exists
if [[ ! -d "$INCLUDE_DIR" ]]; then
    echo -e "${RED}Error: Include directory '$INCLUDE_DIR' not found${NC}"
    exit 1
fi

# Check compiler exists
if ! command -v "$COMPILER" &> /dev/null; then
    echo -e "${RED}Error: Compiler '$COMPILER' not found${NC}"
    exit 1
fi

echo "Checking header self-containment"
echo "  Compiler: $COMPILER"
echo "  Standard: $STD"
echo "  Include dir: $INCLUDE_DIR"
echo ""

# Find all headers
HEADERS=$(find "$INCLUDE_DIR" -maxdepth 1 -name "*.h" -o -name "*.hpp" | sort)
HEADER_COUNT=$(echo "$HEADERS" | wc -l)

echo "Found $HEADER_COUNT headers to check"
echo ""

PASSED=0
FAILED=0
FAILED_HEADERS=""

for header in $HEADERS; do
    header_name=$(basename "$header")
    
    if [[ $VERBOSE -eq 1 ]]; then
        printf "Checking %-45s " "$header_name..."
    fi
    
    # Create test source that includes just this header
    TEST_SOURCE="#include \"$header_name\""
    
    # Compile with syntax-only
    if echo "$TEST_SOURCE" | $COMPILER -std="$STD" -fsyntax-only -Wall -Wextra -I"$INCLUDE_DIR" -x c++ - 2>/dev/null; then
        ((PASSED++))
        if [[ $VERBOSE -eq 1 ]]; then
            echo -e "${GREEN}OK${NC}"
        fi
    else
        ((FAILED++))
        FAILED_HEADERS="$FAILED_HEADERS $header_name"
        if [[ $VERBOSE -eq 1 ]]; then
            echo -e "${RED}FAILED${NC}"
        fi
    fi
done

echo ""
echo "============================================================"
echo "Results: $PASSED/$HEADER_COUNT headers passed"

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo -e "${RED}FAILED HEADERS:${NC}"
    for h in $FAILED_HEADERS; do
        echo "  - $h"
        # Show actual error
        echo "#include \"$h\"" | $COMPILER -std="$STD" -fsyntax-only -Wall -I"$INCLUDE_DIR" -x c++ - 2>&1 | head -5 | sed 's/^/      /'
    done
    echo ""
    echo -e "${RED}FAILED: Not all headers are self-contained${NC}"
    exit 1
else
    echo ""
    echo -e "${GREEN}PASSED: All headers are self-contained${NC}"
    exit 0
fi
