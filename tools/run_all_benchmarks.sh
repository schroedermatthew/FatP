#!/bin/bash

# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/run_all_benchmarks.sh
#   summary: "Linux/macOS script to execute all Fat-P benchmark suites."
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

# ==============================================================================
# run_all_benchmarks.sh - Execute all Fat-P benchmark suites
# ==============================================================================
# Location: tools/ (run from project root)
#
# Usage:
#   ./tools/run_all_benchmarks.sh [OPTIONS]
#
# Options:
#   -d, --dir DIR       Benchmark directory (default: build/release)
#   -o, --output DIR    Output directory for results (default: benchmark_results)
#   -f, --filter GLOB   Only run benchmarks matching pattern (e.g., "*Hash*")
#   -q, --quick         Quick mode: reduced iterations (warmup=1, measured=5)
#   -v, --verbose       Verbose output during execution
#   -s, --sequential    Run sequentially (default, for consistent results)
#   -c, --continue      Continue on error (don't stop on first failure)
#   --dry-run           Show what would be run without executing
#   -h, --help          Show this help message
#
# Environment Variables (passed to benchmarks):
#   FATP_BENCH_WARMUP_RUNS    Warmup iterations (default: 3)
#   FATP_BENCH_BATCHES        Measured batches (default: 15)
#   FATP_BENCH_SEED           RNG seed (default: 12345)
#   FATP_BENCH_NO_STABILIZE   Skip CPU stabilization (faster but less accurate)
#   FATP_BENCH_OUTPUT_CSV     CSV output path per benchmark
#   FATP_BENCH_OUTPUT_JSON    JSON output path per benchmark
#
# ==============================================================================

set -euo pipefail

# ------------------------------------------------------------------------------
# Verify we're in project root
# ------------------------------------------------------------------------------
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Run from project root (where CMakeLists.txt is located)"
    exit 1
fi

# ------------------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------------------
BENCH_DIR="build/release"
OUTPUT_DIR="benchmark_results"
FILTER=""
QUICK_MODE=false
VERBOSE=false
CONTINUE_ON_ERROR=false
DRY_RUN=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ------------------------------------------------------------------------------
# Parse Arguments
# ------------------------------------------------------------------------------
show_help() {
    head -35 "$0" | tail -32
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--dir)
            BENCH_DIR="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -f|--filter)
            FILTER="$2"
            shift 2
            ;;
        -q|--quick)
            QUICK_MODE=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -c|--continue)
            CONTINUE_ON_ERROR=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# ------------------------------------------------------------------------------
# Setup
# ------------------------------------------------------------------------------
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RESULTS_DIR="${OUTPUT_DIR}/${TIMESTAMP}"
LOG_FILE="${RESULTS_DIR}/run_all.log"

# Quick mode environment
if [ "$QUICK_MODE" = true ]; then
    export FATP_BENCH_WARMUP_RUNS=1
    export FATP_BENCH_BATCHES=5
    export FATP_BENCH_NO_STABILIZE=1
fi

# Find benchmarks
if [ -n "$FILTER" ]; then
    BENCHMARKS=$(find "$BENCH_DIR" -maxdepth 1 -type f -name "benchmark_*${FILTER}*" -executable 2>/dev/null | sort)
else
    BENCHMARKS=$(find "$BENCH_DIR" -maxdepth 1 -type f -name "benchmark_*" -executable 2>/dev/null | sort)
fi

# Handle Windows executables (.exe)
if [ -z "$BENCHMARKS" ]; then
    if [ -n "$FILTER" ]; then
        BENCHMARKS=$(find "$BENCH_DIR" -maxdepth 1 -type f -name "benchmark_*${FILTER}*.exe" 2>/dev/null | sort)
    else
        BENCHMARKS=$(find "$BENCH_DIR" -maxdepth 1 -type f -name "benchmark_*.exe" 2>/dev/null | sort)
    fi
fi

BENCH_COUNT=$(echo "$BENCHMARKS" | grep -c . || echo 0)

if [ "$BENCH_COUNT" -eq 0 ]; then
    echo -e "${RED}Error: No benchmarks found in $BENCH_DIR${NC}"
    echo "Looking for: benchmark_* executables"
    exit 1
fi

# ------------------------------------------------------------------------------
# Display Plan
# ------------------------------------------------------------------------------
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}  Fat-P Benchmark Suite Runner${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""
echo -e "Directory:    ${YELLOW}$BENCH_DIR${NC}"
echo -e "Output:       ${YELLOW}$RESULTS_DIR${NC}"
echo -e "Benchmarks:   ${YELLOW}$BENCH_COUNT${NC}"
echo -e "Quick mode:   ${YELLOW}$QUICK_MODE${NC}"
echo -e "Continue:     ${YELLOW}$CONTINUE_ON_ERROR${NC}"
echo ""
echo "Benchmarks to run:"
for bench in $BENCHMARKS; do
    echo "  - $(basename "$bench")"
done
echo ""

if [ "$DRY_RUN" = true ]; then
    echo -e "${YELLOW}[DRY RUN] Would execute the above benchmarks${NC}"
    exit 0
fi

# Create output directory
mkdir -p "$RESULTS_DIR"

# ------------------------------------------------------------------------------
# Run Benchmarks
# ------------------------------------------------------------------------------
echo -e "${BLUE}Starting benchmark execution at $(date)${NC}"
echo ""

PASSED=0
FAILED=0
SKIPPED=0
declare -a FAILED_BENCHMARKS=()
declare -a TIMINGS=()

run_benchmark() {
    local bench="$1"
    local name=$(basename "$bench" | sed 's/\.exe$//')
    local result_file="${RESULTS_DIR}/${name}.txt"
    local csv_file="${RESULTS_DIR}/${name}.csv"
    local start_time end_time duration
    
    echo -e "${BLUE}================================================================================${NC}"
    echo -e "${YELLOW}Running: $name${NC}"
    echo -e "${BLUE}================================================================================${NC}"
    
    # Set per-benchmark CSV output
    export FATP_BENCH_OUTPUT_CSV="$csv_file"
    
    start_time=$(date +%s)
    
    if [ "$VERBOSE" = true ]; then
        # Show output in real-time and capture to file
        if "$bench" 2>&1 | tee "$result_file"; then
            local status=0
        else
            local status=$?
        fi
    else
        # Capture output, show only header
        if "$bench" > "$result_file" 2>&1; then
            local status=0
            # Show just the header (first ~30 lines)
            head -30 "$result_file"
            echo "..."
            echo "[Full output saved to $result_file]"
        else
            local status=$?
        fi
    fi
    
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    TIMINGS+=("$name:$duration")
    
    if [ $status -eq 0 ]; then
        echo -e "${GREEN}[OK] $name completed in ${duration}s${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}[FAIL] $name failed (exit code: $status)${NC}"
        FAILED_BENCHMARKS+=("$name")
        ((FAILED++))
        return $status
    fi
}

# Main execution loop
for bench in $BENCHMARKS; do
    if [ "$CONTINUE_ON_ERROR" = true ]; then
        run_benchmark "$bench" || true
    else
        run_benchmark "$bench"
    fi
    echo ""
done

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}  Summary${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""
echo -e "Total:    $BENCH_COUNT"
echo -e "Passed:   ${GREEN}$PASSED${NC}"
echo -e "Failed:   ${RED}$FAILED${NC}"
echo ""

if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed benchmarks:${NC}"
    for name in "${FAILED_BENCHMARKS[@]}"; do
        echo "  - $name"
    done
    echo ""
fi

echo "Timing breakdown:"
for timing in "${TIMINGS[@]}"; do
    name="${timing%%:*}"
    secs="${timing##*:}"
    printf "  %-40s %4ds\n" "$name" "$secs"
done
echo ""

TOTAL_TIME=0
for timing in "${TIMINGS[@]}"; do
    secs="${timing##*:}"
    TOTAL_TIME=$((TOTAL_TIME + secs))
done
echo -e "Total time: ${YELLOW}${TOTAL_TIME}s${NC} ($(printf '%d:%02d' $((TOTAL_TIME/60)) $((TOTAL_TIME%60))))"
echo ""

echo "Results saved to: $RESULTS_DIR"
echo ""
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}  Completed at $(date)${NC}"
echo -e "${BLUE}================================================================================${NC}"

# Exit with failure if any benchmark failed
[ $FAILED -eq 0 ]
