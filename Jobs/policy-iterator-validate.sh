#!/bin/bash
# =============================================================================
# validate.sh - PolicyIterator validation script
# =============================================================================
# Run from repo root: ./validate.sh [--quick|--full|--bench]
#
# Options:
#   --quick  Header checks + debug tests only (fastest)
#   --full   All checks including sanitizers (default)
#   --bench  Full checks + benchmark run
#
# Directory structure:
#   Headers:    FAT_P/FAT_P/fat_p/
#   Tests:      FAT_P/FAT_P/tests/
#   Benchmarks: FAT_P/FAT_P/benchmarks/
# =============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

MODE="${1:---full}"
INCLUDE_DIR="./FAT_P/FAT_P/fat_p"
TEST_SRC="./FAT_P/FAT_P/tests/test_PolicyIterator.cpp"
BENCH_SRC="./FAT_P/FAT_P/benchmarks/benchmark_PolicyIterator.cpp"

pass() { echo -e "${GREEN}✓ $1${NC}"; }
fail() { echo -e "${RED}✗ $1${NC}"; exit 1; }
section() { echo -e "\n${CYAN}=== $1 ===${NC}"; }

# Cleanup on exit
cleanup() {
    rm -f test_debug test_release test_san test_strict include_stress benchmark_run 2>/dev/null || true
}
trap cleanup EXIT

# =============================================================================
section "Header Self-Containment"
# =============================================================================

echo '#include "PolicyIterator.h"' | g++ -std=c++17 -fsyntax-only -I${INCLUDE_DIR} -x c++ - \
    && pass "PolicyIterator.h compiles standalone" \
    || fail "PolicyIterator.h not self-contained"

echo '#include "TensorStridePolicy.h"' | g++ -std=c++17 -fsyntax-only -I${INCLUDE_DIR} -x c++ - \
    && pass "TensorStridePolicy.h compiles standalone" \
    || fail "TensorStridePolicy.h not self-contained"

# =============================================================================
section "Include-Order Stress Test"
# =============================================================================

cat > /tmp/include_stress_$$.cpp << 'EOF'
#include <map>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <functional>
#include <memory>
#include <sstream>
#include <random>
#include "PolicyIterator.h"
#include "TensorStridePolicy.h"
#include <set>
#include <list>
int main() {
    std::vector<int> d = {1,2,3};
    fat_p::iterator::PolicyIterator<int> it(d.data(), d.data()+3);
    return (*it == 1) ? 0 : 1;
}
EOF

g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I${INCLUDE_DIR} -o /tmp/include_stress_$$ /tmp/include_stress_$$.cpp \
    && /tmp/include_stress_$$ \
    && pass "Include-order stress test" \
    || fail "Include-order stress test failed"

rm -f /tmp/include_stress_$$.cpp /tmp/include_stress_$$

# =============================================================================
section "Strict Warnings Compile"
# =============================================================================

g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2 -Werror \
    -DENABLE_TEST_APPLICATION -I${INCLUDE_DIR} -o test_strict ${TEST_SRC} \
    && pass "Compiles with strict warnings" \
    || fail "Strict warnings compile failed"

# =============================================================================
section "Debug Build + Tests"
# =============================================================================

g++ -std=c++17 -g -DENABLE_TEST_APPLICATION -I${INCLUDE_DIR} -o test_debug ${TEST_SRC} \
    && pass "Debug build" \
    || fail "Debug build failed"

./test_debug > /tmp/test_debug_$$.log 2>&1
if grep -q "Failed: 0" /tmp/test_debug_$$.log; then
    PASSED=$(grep "Passed:" /tmp/test_debug_$$.log | awk '{print $2}')
    pass "Debug tests ($PASSED passed)"
else
    cat /tmp/test_debug_$$.log
    fail "Debug tests failed"
fi
rm -f /tmp/test_debug_$$.log

# =============================================================================
section "Release Build + Tests"
# =============================================================================

g++ -std=c++17 -O3 -DNDEBUG -DENABLE_TEST_APPLICATION -I${INCLUDE_DIR} -o test_release ${TEST_SRC} \
    && pass "Release build" \
    || fail "Release build failed"

./test_release > /tmp/test_release_$$.log 2>&1
if grep -q "Failed: 0" /tmp/test_release_$$.log; then
    PASSED=$(grep "Passed:" /tmp/test_release_$$.log | awk '{print $2}')
    pass "Release tests ($PASSED passed)"
else
    cat /tmp/test_release_$$.log
    fail "Release tests failed"
fi
rm -f /tmp/test_release_$$.log

if [[ "$MODE" == "--quick" ]]; then
    echo -e "\n${GREEN}=== QUICK VALIDATION PASSED ===${NC}\n"
    exit 0
fi

# =============================================================================
section "Sanitizers (ASan + UBSan)"
# =============================================================================

g++ -std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -DENABLE_TEST_APPLICATION -I${INCLUDE_DIR} -o test_san ${TEST_SRC} 2>/dev/null \
    && pass "Sanitizer build" \
    || fail "Sanitizer build failed"

./test_san > /tmp/test_san_$$.log 2>&1
if grep -q "Failed: 0" /tmp/test_san_$$.log && ! grep -qi "error\|runtime error\|leak" /tmp/test_san_$$.log; then
    pass "Sanitizer tests clean"
else
    cat /tmp/test_san_$$.log
    fail "Sanitizer tests failed or found issues"
fi
rm -f /tmp/test_san_$$.log

if [[ "$MODE" != "--bench" ]]; then
    echo -e "\n${GREEN}=== FULL VALIDATION PASSED ===${NC}\n"
    exit 0
fi

# =============================================================================
section "Benchmark Build + Run"
# =============================================================================

g++ -std=c++17 -O3 -DNDEBUG -march=native -I${INCLUDE_DIR} -o benchmark_run ${BENCH_SRC} \
    && pass "Benchmark build" \
    || fail "Benchmark build failed"

echo "Running benchmark (this may take a moment)..."
./benchmark_run

echo -e "\n${GREEN}=== ALL VALIDATION PASSED ===${NC}\n"
