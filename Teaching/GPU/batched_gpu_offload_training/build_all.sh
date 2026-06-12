#!/bin/bash
# build_all.sh — Build all stages of the batched GPU offload training packet
#
# Usage:
#   ./build_all.sh              # build what's available
#   ./build_all.sh --stage 0    # build only stage 0
#   ./build_all.sh --clean      # remove binaries
#
# Prerequisites:
#   Stage 0:   gcc with OpenMP
#   Stage 1-4: nvcc, cuSOLVER
#   Stage 2:   LAPACK/MKL (optional, use SKIP_CPU=1 to omit)
#   Stage 5:   Kokkos (set KOKKOS_PATH)
#   Stage 6:   Kokkos + ulib headers (set KOKKOS_PATH, ULIB_PATH)
#
# GPU architecture defaults to sm_70 (V100).  Override with:
#   ARCH=sm_80 ./build_all.sh

set -e

ARCH=${ARCH:-sm_70}
SKIP_CPU=${SKIP_CPU:-0}
KOKKOS_PATH=${KOKKOS_PATH:-}
ULIB_PATH=${ULIB_PATH:-}

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

ok()   { echo -e "  ${GREEN}✓${NC} $1"; }
skip() { echo -e "  ${YELLOW}⊘${NC} $1 — $2"; }
fail() { echo -e "  ${RED}✗${NC} $1 — $2"; }

if [ "$1" = "--clean" ]; then
    echo "Cleaning..."
    rm -f ar_original ar_legacy batched_solve gpu_pipeline \
          ar_cuda ar_kokkos ar_ulib
    echo "Done."
    exit 0
fi

TARGET_STAGE=${2:-all}

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  Batched GPU Offload Training Packet — Build"
echo "  Architecture: ${ARCH}"
echo "══════════════════════════════════════════════════════════════"
echo ""

# ── Stage 0: Original C ───────────────────────────────────────────

if [ "$TARGET_STAGE" = "all" ] || [ "$TARGET_STAGE" = "0" ]; then
    if command -v gcc &>/dev/null; then
        gcc -O2 -fopenmp ar_spectral_original.c -lm -o ar_original 2>/dev/null \
            && ok "Stage 0: ar_original (gcc + OpenMP)" \
            || fail "Stage 0" "gcc compilation failed"
    else
        skip "Stage 0" "gcc not found"
    fi
fi

# ── Stage 1: Legacy CUDA ──────────────────────────────────────────

if [ "$TARGET_STAGE" = "all" ] || [ "$TARGET_STAGE" = "1" ]; then
    if command -v nvcc &>/dev/null; then
        nvcc -O2 -arch=${ARCH} ar_spectral_legacy.cu \
             -lcusolver -o ar_legacy 2>/dev/null \
            && ok "Stage 1: ar_legacy (nvcc + cuSOLVER)" \
            || fail "Stage 1" "nvcc compilation failed"
    else
        skip "Stage 1" "nvcc not found"
    fi
fi

# ── Stage 2: Batched solve ────────────────────────────────────────

if [ "$TARGET_STAGE" = "all" ] || [ "$TARGET_STAGE" = "2" ]; then
    if command -v nvcc &>/dev/null; then
        EXTRA=""
        if [ "$SKIP_CPU" = "1" ]; then
            EXTRA="-DSKIP_CPU"
        elif [ -n "$MKLROOT" ]; then
            EXTRA="-L${MKLROOT}/lib/intel64 -lmkl_intel_lp64 -lmkl_sequential -lmkl_core"
        else
            EXTRA="-llapack -lblas"
        fi

        if [ "$SKIP_CPU" = "1" ]; then
            nvcc -O2 -arch=${ARCH} -DSKIP_CPU batched_cholesky_solve.cu \
                 -lcusolver -lcublas -o batched_solve 2>/dev/null \
                && ok "Stage 2: batched_solve (no CPU baseline)" \
                || fail "Stage 2" "compilation failed"
        else
            nvcc -O2 -arch=${ARCH} batched_cholesky_solve.cu \
                 -lcusolver -lcublas ${EXTRA} -o batched_solve 2>/dev/null \
                && ok "Stage 2: batched_solve (with CPU baseline)" \
                || { nvcc -O2 -arch=${ARCH} -DSKIP_CPU batched_cholesky_solve.cu \
                     -lcusolver -lcublas -o batched_solve 2>/dev/null \
                    && ok "Stage 2: batched_solve (LAPACK not found, CPU baseline skipped)" \
                    || fail "Stage 2" "compilation failed"; }
        fi
    else
        skip "Stage 2" "nvcc not found"
    fi
fi

# ── Stage 3: GPU-resident pipeline ────────────────────────────────

if [ "$TARGET_STAGE" = "all" ] || [ "$TARGET_STAGE" = "3" ]; then
    if command -v nvcc &>/dev/null; then
        nvcc -O2 -arch=${ARCH} gpu_resident_pipeline.cu \
             -lcusolver -lcublas -o gpu_pipeline 2>/dev/null \
            && ok "Stage 3: gpu_pipeline" \
            || fail "Stage 3" "compilation failed"
    else
        skip "Stage 3" "nvcc not found"
    fi
fi

# ── Stage 4: AR spectral (raw CUDA) ──────────────────────────────

if [ "$TARGET_STAGE" = "all" ] || [ "$TARGET_STAGE" = "4" ]; then
    if command -v nvcc &>/dev/null; then
        nvcc -O2 -arch=${ARCH} ar_spectral_pipeline.cu \
             -lcusolver -o ar_cuda 2>/dev/null \
            && ok "Stage 4: ar_cuda" \
            || fail "Stage 4" "compilation failed"
    else
        skip "Stage 4" "nvcc not found"
    fi
fi

# ── Stage 5: Kokkos port ─────────────────────────────────────────

if [ "$TARGET_STAGE" = "all" ] || [ "$TARGET_STAGE" = "5" ]; then
    if [ -n "$KOKKOS_PATH" ] && [ -f "${KOKKOS_PATH}/bin/nvcc_wrapper" ]; then
        ${KOKKOS_PATH}/bin/nvcc_wrapper -std=c++20 -O2 -arch=${ARCH} \
            -I${KOKKOS_PATH}/include \
            -L${KOKKOS_PATH}/lib -lkokkoscore -lkokkoscontainers \
            -lcusolver \
            ar_spectral_kokkos.cpp -o ar_kokkos 2>/dev/null \
            && ok "Stage 5: ar_kokkos" \
            || fail "Stage 5" "compilation failed"
    else
        skip "Stage 5" "KOKKOS_PATH not set or nvcc_wrapper not found"
    fi
fi

# ── Stage 6: ulib integration ────────────────────────────────────

if [ "$TARGET_STAGE" = "all" ] || [ "$TARGET_STAGE" = "6" ]; then
    if [ -n "$KOKKOS_PATH" ] && [ -n "$ULIB_PATH" ] \
       && [ -f "${KOKKOS_PATH}/bin/nvcc_wrapper" ]; then
        ${KOKKOS_PATH}/bin/nvcc_wrapper -std=c++20 -O2 -arch=${ARCH} \
            -I${KOKKOS_PATH}/include -I${ULIB_PATH}/include \
            -L${KOKKOS_PATH}/lib -lkokkoscore -lkokkoscontainers \
            -lcusolver \
            ar_spectral_ulib.cpp -o ar_ulib 2>/dev/null \
            && ok "Stage 6: ar_ulib" \
            || fail "Stage 6" "compilation failed"
    else
        skip "Stage 6" "KOKKOS_PATH or ULIB_PATH not set"
    fi
fi

echo ""
echo "Done."
