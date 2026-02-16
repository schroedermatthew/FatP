---
doc_id: BR-FloatingPointComparison-001
doc_type: "Benchmark Results"
title: "FloatingPointComparison"
fatp_components: ["FloatingPointComparison"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - FloatingPointComparison

**Source:** `benchmark_FloatingPointComparison.cpp`
**Date:** February 2026
**Methodology:** Round-robin, randomized order, CPU-stabilized (local) / unstabilized (CI), median-primary

---

## Test Environments

| Property | Local (MSVC) | GCC CI | Clang CI | MSVC CI |
|----------|-------------|--------|----------|---------|
| OS | Windows 11 Pro | Ubuntu (Azure) | Ubuntu (Azure) | Windows (Azure) |
| Compiler | Windows-x64 MSVC-1950 | Linux-x64 GCC-14.2 | Linux-x64 Clang-17.0 | Windows-x64 MSVC-1944 |
| CPU | Intel Core Ultra 9 285K | Azure (shared) | Azure (shared) | Azure (shared) |
| RAM | 64 GB DDR5 | Shared tenancy | Shared tenancy | Shared tenancy |
| Measured runs | 15 | 20 | 20 | 20 |
| CPU stabilization | Yes | No | No | No |

**Competitors detected:**

| Library | Local | GCC | Clang | MSVC CI |
|---------|-------|-----|-------|---------|
| fat_p::floatEqual<StandardPolicy> | x | x | x | x |
| fat_p::approximateEqual | x | x | x | x |
| Manual absolute epsilon | x | x | x | x |
| Manual relative epsilon | x | x | x | x |
| Direct operator== | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
--- Fat-P Policies vs Manual Baseline ---
Contract: absolute/hybrid tolerance comparison semantics

[2026-02-15 19:36:02] CPU: 2211 MHz (base: 3686)
  Fat-P Standard                1.72       ns  (mean: 1.78, stddev: 0.12)  CI95: [1.71, 1.84]
  Manual absolute               0.36       ns  (mean: 0.36, stddev: 0.01)  CI95: [0.36, 0.37]
  Ratio: 4.76x

  Fat-P Hybrid                  7.16       ns  (mean: 7.17, stddev: 0.20)  CI95: [7.06, 7.28]
  Manual hybrid                 3.15       ns  (mean: 3.21, stddev: 0.16)  CI95: [3.12, 3.29]
  Ratio: 2.27x

--- Policy Comparison (Normal Values) ---
Contract: epsilon-based floating-point equality

[2026-02-15 19:36:02] CPU: 2174 MHz (base: 3686)
  Standard                      1.69       ns  (mean: 1.71, stddev: 0.11)  CI95: [1.65, 1.77]
  Relative                      2.19       ns  (mean: 2.22, stddev: 0.12)  CI95: [2.15, 2.29]
  ULP                           6.77       ns  (mean: 6.88, stddev: 0.25)  CI95: [6.74, 7.01]
  Hybrid                        7.09       ns  (mean: 7.02, stddev: 0.25)  CI95: [6.88, 7.16]

--- Special Value Handling ---
Contract: IEEE 754 NaN/Inf semantics

[2026-02-15 19:36:03] CPU: 2248 MHz (base: 3686)
  NaN                           1.55       ns  (mean: 1.58, stddev: 0.09)  CI95: [1.52, 1.63]
  Infinity                      2.05       ns  (mean: 2.03, stddev: 0.08)  CI95: [1.99, 2.07]
  Speedup vs normal: 4.6x (NaN), 3.5x (Inf)

================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
--- Fat-P Policies vs Manual Baseline ---
Contract: absolute/hybrid tolerance comparison semantics

[2026-02-16 03:37:40] CPU: 2445 MHz (~base: 2445)
  Fat-P Standard                0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Manual absolute               0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Ratio: 1.00x

  Fat-P Hybrid                  0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Manual hybrid                 0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Ratio: 1.00x

--- Policy Comparison (Normal Values) ---
Contract: epsilon-based floating-point equality

[2026-02-16 03:37:40] CPU: 2445 MHz (~base: 2445)
  Standard                      0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Relative                      0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  ULP                           0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Hybrid                        0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]

--- Special Value Handling ---
Contract: IEEE 754 NaN/Inf semantics

[2026-02-16 03:37:40] CPU: 2445 MHz (~base: 2445)
  NaN                           0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Infinity                      0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Speedup vs normal: 1.0x (NaN), 1.0x (Inf)

================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
--- Fat-P Policies vs Manual Baseline ---
Contract: absolute/hybrid tolerance comparison semantics

[2026-02-16 04:11:13] CPU: 3240 MHz (~base: 3240)
  Fat-P Standard                0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Manual absolute               0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Ratio: 1.00x

  Fat-P Hybrid                  0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Manual hybrid                 0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Ratio: 1.47x

--- Policy Comparison (Normal Values) ---
Contract: epsilon-based floating-point equality

[2026-02-16 04:11:13] CPU: 3240 MHz (~base: 3240)
  Standard                      0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Relative                      0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  ULP                           0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Hybrid                        0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]

--- Special Value Handling ---
Contract: IEEE 754 NaN/Inf semantics

[2026-02-16 04:11:13] CPU: 3240 MHz (~base: 3240)
  NaN                           0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Infinity                      0.00       ns  (mean: 0.00, stddev: 0.00)  CI95: [0.00, 0.00]
  Speedup vs normal: 1.0x (NaN), 1.0x (Inf)

================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
--- Fat-P Policies vs Manual Baseline ---
Contract: absolute/hybrid tolerance comparison semantics

[2026-02-16 04:53:33] CPU: 2445 MHz (base: 2445)
  Fat-P Standard                6.65       ns  (mean: 6.73, stddev: 0.22)  CI95: [6.63, 6.84]
  Manual absolute               0.73       ns  (mean: 0.73, stddev: 0.03)  CI95: [0.71, 0.74]
  Ratio: 9.17x

  Fat-P Hybrid                  12.60      ns  (mean: 12.61, stddev: 0.08)  CI95: [12.57, 12.65]
  Manual hybrid                 7.31       ns  (mean: 7.32, stddev: 0.02)  CI95: [7.31, 7.33]
  Ratio: 1.72x

--- Policy Comparison (Normal Values) ---
Contract: epsilon-based floating-point equality

[2026-02-16 04:53:34] CPU: 2445 MHz (base: 2445)
  Standard                      6.61       ns  (mean: 6.66, stddev: 0.13)  CI95: [6.60, 6.72]
  Relative                      8.49       ns  (mean: 8.49, stddev: 0.02)  CI95: [8.48, 8.50]
  ULP                           13.86      ns  (mean: 13.90, stddev: 0.16)  CI95: [13.82, 13.97]
  Hybrid                        12.49      ns  (mean: 12.48, stddev: 0.04)  CI95: [12.46, 12.50]

--- Special Value Handling ---
Contract: IEEE 754 NaN/Inf semantics

[2026-02-16 04:53:34] CPU: 2445 MHz (base: 2445)
  NaN                           5.04       ns  (mean: 5.05, stddev: 0.03)  CI95: [5.03, 5.06]
  Infinity                      6.31       ns  (mean: 6.66, stddev: 1.07)  CI95: [6.15, 7.17]
  Speedup vs normal: 2.5x (NaN), 2.0x (Inf)

================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
