# Batched Small-Matrix GPU Offload — Training Packet

## The Problem

65,536 independent Cholesky solves, each ≤ 32×32.  Written serially in legacy C
with hand-rolled Numerical Recipes factorization.  Needs to run on GPU.

The application is **moving-window AR spectral estimation**: a non-stationary
signal is chopped into overlapping windows, each window produces a Yule-Walker
system (Toeplitz autocovariance matrix), and the AR coefficients define the
local power spectral density.  The output is a time-frequency spectrogram.

The mathematical and physical development is in `ar_spectral_estimation_theory.md`.


## Start Here

**`00_pattern_recognition.md`** answers the question *"does my problem have this
structure?"*  It connects to the GPU Offload Triage Training course, states the
four diagnostic criteria for the batched-small-solve pattern, describes the
six-stage pipeline skeleton, and maps six application domains onto it.  Read
this first — if your problem doesn't pass the four diagnostic questions, the
rest of the packet doesn't apply.


## File Progression

The files are ordered by migration stage.  Each one compiles and runs
independently.  The same test signal (chirp + transient + noise) and
verification (peak-frequency tracking) appear in all versions for
direct comparison.

### Stage 0 — The Code You Inherit

**`ar_spectral_original.c`** — Pure C, `gcc -O2 -fopenmp … -lm`

Hand-rolled Cholesky from Numerical Recipes (2007).  Non-contiguous `double**`
row-pointer storage.  OpenMP bolted on in 2014 with `#pragma omp critical`
serializing the output write.  Per-window `mat_alloc`/`mat_free` inside the
loop.  Five archaeological layers from five different developers.

This is the code that exists before anyone says "GPU."  The migration
obstacles are structural:

  - `double**` rows are scattered across the heap (cannot `cudaMemcpy`)
  - In-place factorization destroys the matrix (cannot batch)
  - No separation of assembly from solve (no seam for a library call)
  - Scratch sharing via globals / `omp critical` (breaks parallelism)


### Stage 1 — "Just Add CUDA" (Anti-Pattern)

**`ar_spectral_legacy.cu`** — C + CUDA, `nvcc -O2 -lcusolver`

Someone wraps a single cuSOLVER call in a function and calls it 65,536 times.
Each call creates a cuSOLVER handle, `cudaMalloc`s four buffers, copies one
32×32 matrix to the device, solves, copies back, frees everything.

This version is **slower than the pure CPU version** because PCIe round-trip
latency (~10 μs per `cudaMemcpy`) exceeds the useful GPU compute time
(~1.5 μs for a 32×32 Cholesky).  Approximately one million CUDA API calls
for ~0.1 ms of useful arithmetic.


### Stage 2 — Batched Solve (The First Real Win)

**`batched_cholesky_solve.cu`** — CUDA, `nvcc -O2 -lcusolver -lcublas`

Three paths timed side by side:

  1. CPU serial — LAPACK `dpotrf`/`dpotrs`, one system at a time
  2. GPU batched — `cusolverDnDpotrfBatched` / `DpotrsBatched`, two kernel launches
  3. GPU batched + two-stream overlap — hides H2D behind compute

Demonstrates contiguous matrix storage, device pointer arrays, and the
fundamental restructuring: accumulate all matrices, then solve all at once.
Transfer dominates because the matrices are assembled on the host.

Links LAPACK for the CPU baseline (pass `-DSKIP_CPU` to omit).


### Stage 3 — GPU-Resident Pipeline (Eliminate Transfer)

**`gpu_resident_pipeline.cu`** — CUDA, `nvcc -O2 -lcusolver -lcublas`

The generic pipeline pattern: raw values → transcendental transform → matrix
formation → batched solve → scatter/average.  Data touches the host exactly
twice (H2D at start, D2H at end).  All intermediate stages are device-only.

The matrices are born on the device from a transcendental transform of the
input data.  Transfer is no longer the bottleneck because you only pay it once.


### Stage 4 — AR Spectral Estimation (The Real Application)

**`ar_spectral_pipeline.cu`** — CUDA, `nvcc -O2 -lcusolver`

The moving-window AR spectral estimation pipeline with six stages:

  1. H2D: raw signal
  2. Kernel: Hann windowing (transcendental: cos)
  3. Kernel: autocovariance + Toeplitz assembly (shared memory)
  4. cuSOLVER: batched Cholesky factorize + solve
  5. Kernel: PSD evaluation (16.8M complex polynomial evaluations)
  6. D2H: time-frequency matrix

Custom CUDA kernels for stages 2, 3, 5.  Verification via chirp
peak-frequency tracking.

See `ar_spectral_estimation_theory.md` for the physics and mathematics.


### Stage 5 — Kokkos Port

**`ar_spectral_kokkos.cpp`** — Kokkos + CUDA, `nvcc_wrapper -O2 -lcusolver`

Same pipeline expressed in Kokkos vocabulary:

  - Explicit right-iteration `MDRangePolicy` for Hann window and PSD evaluation
  - `TeamPolicy` with team scratch for autocovariance
  - `RangePolicy` for innovation variance
  - Raw pointer extraction (`.data()`) for cuSOLVER interop
  - `Kokkos::fence()` at the Kokkos↔cuSOLVER boundary

Input accepted as `const double*`, wrapped in unmanaged `Kokkos::View`.


### Stage 6 — ulib Integration

**`ar_spectral_ulib.cpp`** — ulib + Kokkos + CUDA, `nvcc_wrapper -O2 -lcusolver`

Same pipeline using the Fat-P four-layer stack at the host↔device boundary:

  - `LifetimeToken` — guards the caller's signal buffer (debug builds)
  - `ArrayView<const double, dynamic_extent>` — wraps raw `double*` input
  - `DeviceInput` — one-way H2D staging (signal, read-only)
  - `DeviceOutput` — device allocation + `copyTo()` (PSD result)
  - `ULIB_SCOPE_EXIT` — RAII cleanup of cuSOLVER handle + `cudaMalloc`

Device-only intermediates stay as `Kokkos::View` — DeviceStaging is not
used for buffers with no host counterpart.

Requires: `ArrayView.h`, `DeviceStaging.h`, `LifetimeToken.h`, `ScopeGuard.h`


## Theory Document

**`ar_spectral_estimation_theory.md`** — Physics, mathematics, and pipeline mapping.

Covers: non-stationary spectral analysis motivation, the AR model, Yule-Walker
equations, why batched Cholesky beats Levinson-Durbin on GPU, windowing,
the six-stage pipeline, PSD evaluation, and verification strategy.


## Layout Analysis

**`kokkos_layout_analysis.md`** — Why LayoutLeft is wrong for this pipeline.

A worked error analysis: the wrong layout choice, the reasoning that leads to
it, why it produces fully uncoalesced access (stride of 65,536 between adjacent
thread writes), and the correct layout derivation from first principles.  Covers
MDRangePolicy thread mapping, TeamPolicy access patterns, the cuSOLVER flat
buffer exception, and a general procedure for checking your own kernels.

Read this if you are writing Kokkos GPU code with 2D views and have been
choosing LayoutLeft by default.


## Build Quick Reference

All targets assume `sm_70` (V100).  Adjust `-arch` for your device.

```bash
# Stage 0 — Original C
gcc -O2 -fopenmp ar_spectral_original.c -lm -o ar_original

# Stage 1 — Legacy CUDA (anti-pattern)
nvcc -O2 -arch=sm_70 ar_spectral_legacy.cu -lcusolver -o ar_legacy

# Stage 2 — Batched solve (with MKL CPU baseline)
nvcc -O2 -arch=sm_70 batched_cholesky_solve.cu \
     -lcusolver -lcublas \
     -L${MKLROOT}/lib/intel64 -lmkl_intel_lp64 -lmkl_sequential -lmkl_core \
     -o batched_solve

# Stage 2 — Batched solve (skip CPU baseline)
nvcc -O2 -arch=sm_70 -DSKIP_CPU batched_cholesky_solve.cu \
     -lcusolver -lcublas -o batched_solve

# Stage 3 — GPU-resident pipeline
nvcc -O2 -arch=sm_70 gpu_resident_pipeline.cu \
     -lcusolver -lcublas -o gpu_pipeline

# Stage 4 — AR spectral (raw CUDA)
nvcc -O2 -arch=sm_70 ar_spectral_pipeline.cu -lcusolver -o ar_cuda

# Stage 5 — Kokkos port (requires C++20)
${KOKKOS_PATH}/bin/nvcc_wrapper -std=c++20 -O2 -arch=sm_70 \
    -I${KOKKOS_PATH}/include \
    -L${KOKKOS_PATH}/lib -lkokkoscore -lkokkoscontainers \
    -lcusolver \
    ar_spectral_kokkos.cpp -o ar_kokkos

# Stage 6 — ulib integration (requires C++20)
${KOKKOS_PATH}/bin/nvcc_wrapper -std=c++20 -O2 -arch=sm_70 \
    -I${KOKKOS_PATH}/include -I${ULIB_PATH}/include \
    -L${KOKKOS_PATH}/lib -lkokkoscore -lkokkoscontainers \
    -lcusolver \
    ar_spectral_ulib.cpp -o ar_ulib
```


### C++17 Kokkos Builds

Stages 5 and 6 use `std::numbers::pi` (`<numbers>` header), which requires
C++20.  If your Kokkos installation targets C++17, replace the two lines in
each file:

```cpp
#include <numbers>
inline constexpr double PI = std::numbers::pi;
```

with:

```cpp
inline constexpr double PI = 3.14159265358979323846;
```

No other C++20 features are used in these files.  Everything else — Kokkos
views, MDRangePolicy with explicit `Iterate::Right`, TeamPolicy, and the
cuSOLVER interop — works under C++17.


## Expected Timing (V100, approximate)

| Stage | Version | Time | Notes |
|-------|---------|------|-------|
| 0 | Original C (8 threads) | 5–15 s | CPU-bound, `omp critical` hurts |
| 1 | Legacy CUDA | 30–120 s | ~1M CUDA API calls, transfer-dominated |
| 2 | Batched solve | 20–50 ms | Transfer-dominated (matrices from host) |
| 3 | GPU-resident | 10–30 ms | Compute-dominated (matrices born on device) |
| 4 | AR spectral (CUDA) | 50–100 ms | Full pipeline, six stages |
| 5 | AR spectral (Kokkos) | 50–100 ms | Same pipeline, portable dispatch |
| 6 | AR spectral (ulib) | 50–100 ms | Same pipeline, ulib at boundary |

Stages 4–6 produce identical results.  The timing difference between
them is negligible — the abstraction layers add no measurable overhead.
The three-order-of-magnitude improvement (Stage 1 → Stage 4) comes
entirely from restructuring the data flow, not from the GPU being fast
at arithmetic.
