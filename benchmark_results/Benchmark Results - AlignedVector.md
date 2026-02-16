---
doc_id: BR-AlignedVector-001
doc_type: "Benchmark Results"
title: "AlignedVector"
fatp_components: ["AlignedVector"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - AlignedVector

**Source:** `benchmark_AlignedVector.cpp`
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
| fat_p::AlignedVector | x | x | x | x |
| std::vector | x | x | x | x |
| boost::alignment::aligned_allocator | x | x | x | x |
| Eigen | — | — | — | — |
| Eigen::aligned_allocator | — | x | x | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  ALIGNMENT VERIFICATION
================================================================================

Contract: Verifies alignment guarantees across multiple allocations. All allocations must meet specified alignment.

[2026-02-15 19:15:30] Alignment Verification CPU: 2469 MHz (base: 3686)

--- Alignment = 16 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 32 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 64 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 128 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 256 bytes ---

  Allocations: 1000, Violations: 0 [PASS]
[Cooling: before sequential iteration] [Ready: 2395 MHz]

================================================================================
  SEQUENTIAL ITERATION (Sum)
================================================================================

Contract: Measures cache-line alignment benefit for SIMD auto-vectorization.

[2026-02-15 19:15:33] Sequential Iteration CPU: 2395 MHz (base: 3686)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.35 ns/elem      0.36 ns/elem      0.01  [0.35, 0.36]
                       AlignedVector<64>      0.35 ns/elem      0.36 ns/elem      0.01  [0.35, 0.36]
                      AlignedVector<128>      0.36 ns/elem      0.36 ns/elem      0.01  [0.36, 0.36]
            boost::aligned_allocator<64>      0.35 ns/elem      0.36 ns/elem      0.01  [0.35, 0.37]
[Cooling: between sizes] [Ready: 2469 MHz]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.36 ns/elem      0.37 ns/elem      0.02  [0.36, 0.38]
                       AlignedVector<64>      0.36 ns/elem      0.36 ns/elem      0.01  [0.35, 0.36]
                      AlignedVector<128>      0.35 ns/elem      0.36 ns/elem      0.01  [0.35, 0.36]
            boost::aligned_allocator<64>      0.36 ns/elem      0.36 ns/elem      0.01  [0.36, 0.37]
[Cooling: between sizes] [Ready: 2285 MHz]
[Cooling: before SIMD dot product] [Ready: 2248 MHz]

================================================================================
  SIMD DOT PRODUCT
================================================================================

Contract: Tests fused multiply-add vectorization potential. assume_aligned() enables compiler to use aligned SIMD loads.

[2026-02-15 19:15:58] Dot Product CPU: 2248 MHz (base: 3686)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.35 ns/elem      0.35 ns/elem      0.01  [0.35, 0.36]
                       AlignedVector<64>      0.35 ns/elem      0.36 ns/elem      0.02  [0.35, 0.37]
                      AlignedVector<128>      0.35 ns/elem      0.35 ns/elem      0.01  [0.35, 0.35]
            boost::aligned_allocator<64>      0.35 ns/elem      0.37 ns/elem      0.07  [0.34, 0.41]
[Cooling: between sizes] [Ready: 2838 MHz]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.38 ns/elem      0.38 ns/elem      0.01  [0.38, 0.39]
                       AlignedVector<64>      0.38 ns/elem      0.38 ns/elem      0.01  [0.38, 0.39]
                      AlignedVector<128>      0.39 ns/elem      0.39 ns/elem      0.02  [0.38, 0.40]
            boost::aligned_allocator<64>      0.37 ns/elem      0.38 ns/elem      0.02  [0.37, 0.39]
[Cooling: between sizes] [Ready: 2690 MHz]
[Cooling: before SIMD SAXPY] [Ready: 3133 MHz]

================================================================================
  SIMD SAXPY (Explicit AVX2)
================================================================================

[SKIPPED] AVX2 not available on this platform
[Cooling: before random access] [Ready: 2322 MHz]

================================================================================
  RANDOM ACCESS
================================================================================

Contract: Measures cache behavior with non-sequential access patterns. Alignment has less impact here; tests baseline overhead.

[2026-02-15 19:16:44] Random Access CPU: 2322 MHz (base: 3686)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.55 ns/elem      0.55 ns/elem      0.03  [0.54, 0.57]
                       AlignedVector<64>      0.53 ns/elem      0.55 ns/elem      0.02  [0.53, 0.56]
                      AlignedVector<128>      0.53 ns/elem      0.54 ns/elem      0.02  [0.54, 0.55]
            boost::aligned_allocator<64>      0.56 ns/elem      0.56 ns/elem      0.06  [0.53, 0.59]
[Cooling: between sizes] [Ready: 2322 MHz]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.60 ns/elem      0.63 ns/elem      0.07  [0.59, 0.67]
                       AlignedVector<64>      0.60 ns/elem      0.61 ns/elem      0.01  [0.60, 0.62]
                      AlignedVector<128>      0.60 ns/elem      0.65 ns/elem      0.10  [0.60, 0.70]
            boost::aligned_allocator<64>      0.60 ns/elem      0.61 ns/elem      0.02  [0.60, 0.62]
[Cooling: between sizes] [Ready: 2395 MHz]
[Cooling: before push_back] [Ready: 2248 MHz]

================================================================================
  PUSH_BACK GROWTH
================================================================================

Contract: Tests amortized O(1) push_back with geometric growth. Compares growing vs pre-reserved scenarios.

[2026-02-15 19:17:02] Push Back CPU: 2248 MHz (base: 3686)

--- N = 1000 ---

  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      1.70 ns/elem      1.71 ns/elem      0.14  [1.64, 1.78]
                AlignedVector<64> (grow)      1.80 ns/elem      1.81 ns/elem      0.24  [1.69, 1.94]
               AlignedVector<128> (grow)      2.10 ns/elem      2.03 ns/elem      0.35  [1.85, 2.20]
     boost::aligned_allocator<64> (grow)      2.30 ns/elem      2.40 ns/elem      0.42  [2.19, 2.61]

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      1.00 ns/elem      1.15 ns/elem      0.27  [1.01, 1.28]
            AlignedVector<64> (reserved)      1.70 ns/elem      1.67 ns/elem      0.05  [1.65, 1.70]
           AlignedVector<128> (reserved)      1.30 ns/elem      1.33 ns/elem      0.12  [1.27, 1.39]
 boost::aligned_allocator<64> (reserved)      1.20 ns/elem      1.13 ns/elem      0.18  [1.03, 1.22]
[Cooling: between sizes] [Ready: 2359 MHz]

--- N = 10000 ---

  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      1.41 ns/elem      1.37 ns/elem      0.45  [1.14, 1.60]
                AlignedVector<64> (grow)      1.37 ns/elem      1.39 ns/elem      0.14  [1.32, 1.45]
               AlignedVector<128> (grow)      1.39 ns/elem      1.47 ns/elem      0.28  [1.33, 1.61]
     boost::aligned_allocator<64> (grow)      1.33 ns/elem      3.44 ns/elem      7.39  [-0.30, 7.18]
  [NOTE] High variance (stddev > median)

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      0.87 ns/elem      0.92 ns/elem      0.27  [0.78, 1.06]
            AlignedVector<64> (reserved)      1.27 ns/elem      1.28 ns/elem      0.05  [1.26, 1.31]
           AlignedVector<128> (reserved)      1.27 ns/elem      1.27 ns/elem      0.03  [1.26, 1.29]
 boost::aligned_allocator<64> (reserved)      1.30 ns/elem      1.09 ns/elem      0.30  [0.94, 1.24]
[Cooling: between sizes] [Ready: 2469 MHz]
[Cooling: before insert] [Ready: 2395 MHz]

================================================================================
  INSERT AT MIDDLE
================================================================================

Contract: Measures O(n) insertion at middle position. Tests element shifting performance.

[2026-02-15 19:17:21] Insert CPU: 2395 MHz (base: 3686)

--- Initial size = 1000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector     18.00 ns/insert     18.33 ns/insert      0.49  [18.09, 18.58]
                       AlignedVector<64>     19.00 ns/insert     18.80 ns/insert      0.41  [18.59, 19.01]
                      AlignedVector<128>     19.00 ns/insert     18.67 ns/insert      0.49  [18.42, 18.91]
            boost::aligned_allocator<64>     20.00 ns/insert     20.27 ns/insert      0.46  [20.04, 20.50]
[Cooling: between sizes] [Ready: 2432 MHz]

--- Initial size = 10000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector    123.00 ns/insert    124.27 ns/insert      3.26  [122.62, 125.92]
                       AlignedVector<64>    124.00 ns/insert    128.20 ns/insert      8.51  [123.89, 132.51]
                      AlignedVector<128>    125.00 ns/insert    127.53 ns/insert      6.69  [124.15, 130.92]
            boost::aligned_allocator<64>    135.00 ns/insert    137.67 ns/insert      6.15  [134.55, 140.78]
[Cooling: between sizes] [Ready: 2727 MHz]
[Cooling: before copy/move] [Ready: 2174 MHz]

================================================================================
  COPY AND MOVE OPERATIONS
================================================================================

Contract: Copy construction should be O(n). Move construction/assignment should be O(1). This section reports: copy-ctor, move-ctor (rotation), move-assign (rotation).

[2026-02-15 19:18:08] Copy/Move CPU: 2174 MHz (base: 3686)

--- N = 10000 ---

  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor   1500.00 ns/op   1540.00 ns/op     82.81  [1498.09, 1581.91]
             AlignedVector<64> copy-ctor   1200.00 ns/op   1220.00 ns/op     56.06  [1191.63, 1248.37]
            AlignedVector<128> copy-ctor   1200.00 ns/op   1226.67 ns/op     45.77  [1203.50, 1249.83]
  boost::aligned_allocator<64> copy-ctor   4800.00 ns/op   4846.67 ns/op     51.64  [4820.53, 4872.80]

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor     10.97 ns/op     10.99 ns/op      0.06  [10.96, 11.02]
             AlignedVector<64> move-ctor     12.53 ns/op     12.76 ns/op      0.53  [12.49, 13.03]
            AlignedVector<128> move-ctor     12.70 ns/op     12.82 ns/op      0.50  [12.57, 13.08]
  boost::aligned_allocator<64> move-ctor     10.90 ns/op     11.22 ns/op      1.15  [10.63, 11.80]
[Cooling: between sizes] [Ready: 2285 MHz]

--- N = 100000 ---

  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor   8000.00 ns/op  15786.67 ns/op  35394.57  [-2125.46, 33698.79]
  [NOTE] High variance (stddev > median)
             AlignedVector<64> copy-ctor   8200.00 ns/op  37566.67 ns/op  53613.45  [10434.52, 64698.82]
  [NOTE] High variance (stddev > median)
            AlignedVector<128> copy-ctor   8200.00 ns/op  19360.00 ns/op  33287.40  [2514.25, 36205.75]
  [NOTE] High variance (stddev > median)
  boost::aligned_allocator<64> copy-ctor  31400.00 ns/op  82906.67 ns/op 110124.51  [27175.98, 138637.36]
  [NOTE] High variance (stddev > median)

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor      4.57 ns/op      4.98 ns/op      1.13  [4.41, 5.56]
             AlignedVector<64> move-ctor      5.17 ns/op      5.79 ns/op      1.21  [5.18, 6.40]
            AlignedVector<128> move-ctor      5.63 ns/op      6.04 ns/op      1.39  [5.34, 6.75]
  boost::aligned_allocator<64> move-ctor      4.43 ns/op      4.88 ns/op      0.96  [4.39, 5.36]
[Cooling: between sizes] [Ready: 2432 MHz]
[Cooling: before corner cases] [Ready: 2432 MHz]

================================================================================
  CORNER CASES
================================================================================

Contract: Tests edge cases: empty vector operations, single element, capacity boundary.

[2026-02-15 19:18:41] Corner Cases CPU: 2432 MHz (base: 3686)

--- Empty Vector Operations ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
             std::vector empty begin/end      2.66 ns/op      2.62 ns/op      0.07  [2.58, 2.66]
       AlignedVector<64> empty begin/end      2.83 ns/op      2.87 ns/op      0.29  [2.72, 3.02]
      AlignedVector<128> empty begin/end      2.81 ns/op      2.95 ns/op      0.67  [2.61, 3.29]
boost::aligned_allocator<64> empty begin/end      2.66 ns/op      2.82 ns/op      0.70  [2.47, 3.18]

--- Single Element Push/Pop Cycle ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                    std::vector push/pop      1.18 ns/cycle      1.20 ns/cycle      0.06  [1.17, 1.23]
              AlignedVector<64> push/pop      1.51 ns/cycle      1.55 ns/cycle      0.08  [1.51, 1.59]
             AlignedVector<128> push/pop      1.52 ns/cycle      1.56 ns/cycle      0.08  [1.52, 1.59]
   boost::aligned_allocator<64> push/pop      1.17 ns/cycle      1.18 ns/cycle      0.07  [1.15, 1.22]

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  ALIGNMENT VERIFICATION
================================================================================

Contract: Verifies alignment guarantees across multiple allocations. All allocations must meet specified alignment.

[2026-02-16 03:37:47] Alignment Verification CPU: 3245 MHz (~base: 3245)

--- Alignment = 16 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 32 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 64 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 128 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 256 bytes ---

  Allocations: 1000, Violations: 0 [PASS]
[Cooling: before sequential iteration] [Ready]

================================================================================
  SEQUENTIAL ITERATION (Sum)
================================================================================

Contract: Measures cache-line alignment benefit for SIMD auto-vectorization.

[2026-02-16 03:37:51] Sequential Iteration CPU: 2445 MHz (~base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.94]
                       AlignedVector<64>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.94]
                      AlignedVector<128>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
            boost::aligned_allocator<64>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
                Eigen::aligned_allocator      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
[Cooling: between sizes] [Ready]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.93 ns/elem      0.94 ns/elem      0.00  [0.93, 0.94]
                       AlignedVector<64>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.94]
                      AlignedVector<128>      0.93 ns/elem      0.94 ns/elem      0.00  [0.93, 0.94]
            boost::aligned_allocator<64>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.94]
                Eigen::aligned_allocator      0.93 ns/elem      0.94 ns/elem      0.01  [0.93, 0.94]
[Cooling: between sizes] [Ready]
[Cooling: before SIMD dot product] [Ready]

================================================================================
  SIMD DOT PRODUCT
================================================================================

Contract: Tests fused multiply-add vectorization potential. assume_aligned() enables compiler to use aligned SIMD loads.

[2026-02-16 03:38:03] Dot Product CPU: 2445 MHz (~base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.93 ns/elem      0.93 ns/elem      0.01  [0.93, 0.93]
                       AlignedVector<64>      0.93 ns/elem      0.93 ns/elem      0.01  [0.93, 0.93]
                      AlignedVector<128>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
            boost::aligned_allocator<64>      0.93 ns/elem      0.93 ns/elem      0.01  [0.93, 0.93]
                Eigen::aligned_allocator      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
[Cooling: between sizes] [Ready]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.94 ns/elem      0.94 ns/elem      0.02  [0.93, 0.95]
                       AlignedVector<64>      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
                      AlignedVector<128>      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
            boost::aligned_allocator<64>      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
                Eigen::aligned_allocator      0.94 ns/elem      0.94 ns/elem      0.01  [0.94, 0.95]
[Cooling: between sizes] [Ready]
[Cooling: before SIMD SAXPY] [Ready]

================================================================================
  SIMD SAXPY (Explicit AVX2)
================================================================================

Contract: Demonstrates alignment impact with explicit AVX2 loads/stores. Includes a deliberately misaligned-pointer case.

[2026-02-16 03:38:15] SAXPY CPU: 2808 MHz (~base: 2808)

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
         AVX2 aligned (load_ps/store_ps)      0.12 ns/elem      0.12 ns/elem      0.00  [0.12, 0.12]
AVX2 unaligned (loadu/storeu on aligned ptr)      0.12 ns/elem      0.12 ns/elem      0.00  [0.12, 0.12]
AVX2 unaligned (loadu/storeu on misaligned ptr)      0.06 ns/elem      0.06 ns/elem      0.00  [0.06, 0.06]
[Cooling: between sizes] [Ready]

--- N = 1000000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
         AVX2 aligned (load_ps/store_ps)      0.13 ns/elem      0.13 ns/elem      0.00  [0.13, 0.13]
AVX2 unaligned (loadu/storeu on aligned ptr)      0.13 ns/elem      0.13 ns/elem      0.00  [0.13, 0.13]
AVX2 unaligned (loadu/storeu on misaligned ptr)      0.07 ns/elem      0.07 ns/elem      0.00  [0.07, 0.07]
[Cooling: between sizes] [Ready]
[Cooling: before random access] [Ready]

================================================================================
  RANDOM ACCESS
================================================================================

Contract: Measures cache behavior with non-sequential access patterns. Alignment has less impact here; tests baseline overhead.

[2026-02-16 03:38:27] Random Access CPU: 2445 MHz (~base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      3.13 ns/elem      3.14 ns/elem      0.07  [3.11, 3.18]
                       AlignedVector<64>      3.09 ns/elem      3.10 ns/elem      0.03  [3.09, 3.11]
                      AlignedVector<128>      3.09 ns/elem      3.11 ns/elem      0.04  [3.09, 3.13]
            boost::aligned_allocator<64>      3.11 ns/elem      3.12 ns/elem      0.04  [3.11, 3.14]
                Eigen::aligned_allocator      3.10 ns/elem      3.12 ns/elem      0.04  [3.10, 3.14]
[Cooling: between sizes] [Ready]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      3.14 ns/elem      3.15 ns/elem      0.03  [3.14, 3.16]
                       AlignedVector<64>      3.17 ns/elem      3.18 ns/elem      0.02  [3.16, 3.19]
                      AlignedVector<128>      3.16 ns/elem      3.16 ns/elem      0.02  [3.15, 3.17]
            boost::aligned_allocator<64>      3.16 ns/elem      3.17 ns/elem      0.03  [3.16, 3.19]
                Eigen::aligned_allocator      3.16 ns/elem      3.18 ns/elem      0.05  [3.16, 3.20]
[Cooling: between sizes] [Ready]
[Cooling: before push_back] [Ready]

================================================================================
  PUSH_BACK GROWTH
================================================================================

Contract: Tests amortized O(1) push_back with geometric growth. Compares growing vs pre-reserved scenarios.

[2026-02-16 03:38:39] Push Back CPU: 2445 MHz (~base: 2445)

--- N = 1000 ---
  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      3.70 ns/elem      3.70 ns/elem      0.07  [3.67, 3.73]
                AlignedVector<64> (grow)      3.52 ns/elem      3.51 ns/elem      0.16  [3.44, 3.58]
               AlignedVector<128> (grow)      3.85 ns/elem      3.85 ns/elem      0.18  [3.77, 3.93]
     boost::aligned_allocator<64> (grow)      4.42 ns/elem      5.51 ns/elem      4.67  [3.46, 7.56]
  [NOTE] High variance (stddev > median)
         Eigen::aligned_allocator (grow)      3.92 ns/elem      3.97 ns/elem      0.23  [3.86, 4.07]

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      3.37 ns/elem      3.36 ns/elem      0.01  [3.36, 3.37]
            AlignedVector<64> (reserved)      2.89 ns/elem      2.87 ns/elem      0.04  [2.86, 2.89]
           AlignedVector<128> (reserved)      2.93 ns/elem      2.92 ns/elem      0.03  [2.90, 2.93]
 boost::aligned_allocator<64> (reserved)      3.75 ns/elem      3.75 ns/elem      0.03  [3.74, 3.77]
     Eigen::aligned_allocator (reserved)      3.51 ns/elem      3.49 ns/elem      0.03  [3.48, 3.50]
[Cooling: between sizes] [Ready]

--- N = 10000 ---
  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      4.98 ns/elem      5.09 ns/elem      0.34  [4.94, 5.24]
                AlignedVector<64> (grow)      2.54 ns/elem      2.57 ns/elem      0.15  [2.51, 2.64]
               AlignedVector<128> (grow)      3.03 ns/elem      3.11 ns/elem      0.23  [3.01, 3.21]
     boost::aligned_allocator<64> (grow)      3.96 ns/elem      3.98 ns/elem      0.08  [3.94, 4.01]
         Eigen::aligned_allocator (grow)      3.74 ns/elem      3.77 ns/elem      0.18  [3.69, 3.84]

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      2.88 ns/elem      2.91 ns/elem      0.16  [2.84, 2.98]
            AlignedVector<64> (reserved)      2.80 ns/elem      2.84 ns/elem      0.18  [2.76, 2.92]
           AlignedVector<128> (reserved)      2.80 ns/elem      2.80 ns/elem      0.01  [2.80, 2.81]
 boost::aligned_allocator<64> (reserved)      3.79 ns/elem      3.80 ns/elem      0.07  [3.77, 3.83]
     Eigen::aligned_allocator (reserved)      3.61 ns/elem      3.64 ns/elem      0.18  [3.56, 3.72]
[Cooling: between sizes] [Ready]
[Cooling: before insert] [Ready]

================================================================================
  INSERT AT MIDDLE
================================================================================

Contract: Measures O(n) insertion at middle position. Tests element shifting performance.

[2026-02-16 03:38:50] Insert CPU: 2604 MHz (~base: 2604)

--- Initial size = 1000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector     30.46 ns/insert     33.16 ns/insert      3.87  [31.47, 34.86]
                       AlignedVector<64>     30.86 ns/insert     33.61 ns/insert      4.62  [31.59, 35.64]
                      AlignedVector<128>     34.66 ns/insert     37.11 ns/insert      4.40  [35.19, 39.04]
            boost::aligned_allocator<64>     33.31 ns/insert     35.78 ns/insert      4.62  [33.75, 37.80]
                Eigen::aligned_allocator     32.16 ns/insert     34.28 ns/insert      3.93  [32.55, 36.00]
[Cooling: between sizes] [Ready]

--- Initial size = 10000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector    211.19 ns/insert    214.20 ns/insert     11.90  [208.98, 219.42]
                       AlignedVector<64>    213.25 ns/insert    230.44 ns/insert     48.37  [209.24, 251.63]
                      AlignedVector<128>    219.31 ns/insert    222.10 ns/insert     12.41  [216.66, 227.54]
            boost::aligned_allocator<64>    213.50 ns/insert    218.84 ns/insert     18.62  [210.68, 227.00]
                Eigen::aligned_allocator    211.69 ns/insert    214.36 ns/insert     12.08  [209.07, 219.66]
[Cooling: between sizes] [Ready]
[Cooling: before copy/move] [Ready]

================================================================================
  COPY AND MOVE OPERATIONS
================================================================================

Contract: Copy construction should be O(n). Move construction/assignment should be O(1). This section reports: copy-ctor, move-ctor (rotation), move-assign (rotation).

[2026-02-16 03:39:01] Copy/Move CPU: 2445 MHz (~base: 2445)

--- N = 10000 ---
  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor   1272.00 ns/op   1285.90 ns/op     96.15  [1243.76, 1328.04]
             AlignedVector<64> copy-ctor   1122.00 ns/op   1129.60 ns/op     48.02  [1108.56, 1150.64]
            AlignedVector<128> copy-ctor   1182.00 ns/op   1177.80 ns/op     50.32  [1155.75, 1199.85]
  boost::aligned_allocator<64> copy-ctor   1503.00 ns/op   1507.35 ns/op     28.58  [1494.82, 1519.88]
      Eigen::aligned_allocator copy-ctor   1442.50 ns/op   1441.15 ns/op     17.92  [1433.30, 1449.00]

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor      6.39 ns/op      6.66 ns/op      0.49  [6.45, 6.88]
             AlignedVector<64> move-ctor      7.84 ns/op      8.32 ns/op      0.89  [7.93, 8.71]
            AlignedVector<128> move-ctor      7.74 ns/op      8.32 ns/op      1.34  [7.73, 8.91]
  boost::aligned_allocator<64> move-ctor      6.10 ns/op      6.33 ns/op      0.54  [6.09, 6.56]
      Eigen::aligned_allocator move-ctor      6.08 ns/op      6.34 ns/op      0.47  [6.14, 6.55]
[Cooling: between sizes] [Ready]

--- N = 100000 ---
  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor  15388.50 ns/op  15778.35 ns/op   1503.39  [15119.46, 16437.24]
             AlignedVector<64> copy-ctor  16200.00 ns/op  16834.30 ns/op   2809.22  [15603.11, 18065.49]
            AlignedVector<128> copy-ctor  15444.00 ns/op  16087.15 ns/op   1737.01  [15325.87, 16848.43]
  boost::aligned_allocator<64> copy-ctor  15759.50 ns/op  15686.70 ns/op   1302.94  [15115.66, 16257.74]
      Eigen::aligned_allocator copy-ctor  15914.00 ns/op  16458.55 ns/op   2708.81  [15271.36, 17645.74]

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor      6.40 ns/op      6.56 ns/op      0.67  [6.27, 6.86]
             AlignedVector<64> move-ctor      7.43 ns/op      7.44 ns/op      0.06  [7.42, 7.47]
            AlignedVector<128> move-ctor      7.20 ns/op      7.35 ns/op      0.68  [7.05, 7.65]
  boost::aligned_allocator<64> move-ctor      6.20 ns/op      6.26 ns/op      0.21  [6.16, 6.35]
      Eigen::aligned_allocator move-ctor      6.09 ns/op      6.10 ns/op      0.01  [6.09, 6.10]
[Cooling: between sizes] [Ready]
[Cooling: before corner cases] [Ready]

================================================================================
  CORNER CASES
================================================================================

Contract: Tests edge cases: empty vector operations, single element, capacity boundary.

[2026-02-16 03:39:12] Corner Cases CPU: 2445 MHz (~base: 2445)

--- Empty Vector Operations ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
             std::vector empty begin/end      5.31 ns/op      5.29 ns/op      0.04  [5.27, 5.30]
       AlignedVector<64> empty begin/end      3.08 ns/op      3.12 ns/op      0.08  [3.09, 3.16]
      AlignedVector<128> empty begin/end      5.00 ns/op      4.99 ns/op      0.06  [4.96, 5.01]
boost::aligned_allocator<64> empty begin/end      4.98 ns/op      4.97 ns/op      0.04  [4.96, 4.99]
Eigen::aligned_allocator empty begin/end      4.95 ns/op      4.97 ns/op      0.04  [4.95, 4.99]

--- Single Element Push/Pop Cycle ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                    std::vector push/pop      5.25 ns/cycle      5.25 ns/cycle      0.00  [5.25, 5.25]
              AlignedVector<64> push/pop      5.24 ns/cycle      5.32 ns/cycle      0.23  [5.22, 5.42]
             AlignedVector<128> push/pop      5.24 ns/cycle      5.36 ns/cycle      0.28  [5.24, 5.48]
   boost::aligned_allocator<64> push/pop     17.99 ns/cycle     17.42 ns/cycle      3.02  [16.10, 18.74]
       Eigen::aligned_allocator push/pop      5.25 ns/cycle      6.50 ns/cycle      3.73  [4.86, 8.13]

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  ALIGNMENT VERIFICATION
================================================================================

Contract: Verifies alignment guarantees across multiple allocations. All allocations must meet specified alignment.

[2026-02-16 04:11:21] Alignment Verification CPU: 3253 MHz (~base: 3253)

--- Alignment = 16 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 32 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 64 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 128 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 256 bytes ---

  Allocations: 1000, Violations: 0 [PASS]
[Cooling: before sequential iteration] [Ready]

================================================================================
  SEQUENTIAL ITERATION (Sum)
================================================================================

Contract: Measures cache-line alignment benefit for SIMD auto-vectorization.

[2026-02-16 04:11:25] Sequential Iteration CPU: 2445 MHz (~base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.94]
                       AlignedVector<64>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
                      AlignedVector<128>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
            boost::aligned_allocator<64>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
                Eigen::aligned_allocator      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.93]
[Cooling: between sizes] [Ready]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.94]
                       AlignedVector<64>      0.94 ns/elem      0.94 ns/elem      0.00  [0.93, 0.94]
                      AlignedVector<128>      0.93 ns/elem      0.93 ns/elem      0.00  [0.93, 0.94]
            boost::aligned_allocator<64>      0.93 ns/elem      0.94 ns/elem      0.00  [0.93, 0.94]
                Eigen::aligned_allocator      0.93 ns/elem      0.94 ns/elem      0.00  [0.93, 0.94]
[Cooling: between sizes] [Ready]
[Cooling: before SIMD dot product] [Ready]

================================================================================
  SIMD DOT PRODUCT
================================================================================

Contract: Tests fused multiply-add vectorization potential. assume_aligned() enables compiler to use aligned SIMD loads.

[2026-02-16 04:11:37] Dot Product CPU: 2445 MHz (~base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      1.24 ns/elem      1.24 ns/elem      0.01  [1.24, 1.24]
                       AlignedVector<64>      1.24 ns/elem      1.24 ns/elem      0.00  [1.24, 1.24]
                      AlignedVector<128>      1.24 ns/elem      1.24 ns/elem      0.00  [1.24, 1.24]
            boost::aligned_allocator<64>      1.24 ns/elem      1.24 ns/elem      0.00  [1.24, 1.24]
                Eigen::aligned_allocator      1.24 ns/elem      1.24 ns/elem      0.00  [1.24, 1.24]
[Cooling: between sizes] [Ready]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      1.25 ns/elem      1.25 ns/elem      0.00  [1.25, 1.25]
                       AlignedVector<64>      1.25 ns/elem      1.25 ns/elem      0.00  [1.25, 1.25]
                      AlignedVector<128>      1.25 ns/elem      1.25 ns/elem      0.00  [1.25, 1.25]
            boost::aligned_allocator<64>      1.25 ns/elem      1.25 ns/elem      0.00  [1.25, 1.25]
                Eigen::aligned_allocator      1.25 ns/elem      1.25 ns/elem      0.00  [1.25, 1.25]
[Cooling: between sizes] [Ready]
[Cooling: before SIMD SAXPY] [Ready]

================================================================================
  SIMD SAXPY (Explicit AVX2)
================================================================================

Contract: Demonstrates alignment impact with explicit AVX2 loads/stores. Includes a deliberately misaligned-pointer case.

[2026-02-16 04:11:50] SAXPY CPU: 2851 MHz (~base: 2851)

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
         AVX2 aligned (load_ps/store_ps)      0.09 ns/elem      0.09 ns/elem      0.00  [0.09, 0.10]
AVX2 unaligned (loadu/storeu on aligned ptr)      0.09 ns/elem      0.09 ns/elem      0.00  [0.09, 0.09]
AVX2 unaligned (loadu/storeu on misaligned ptr)      0.11 ns/elem      0.11 ns/elem      0.00  [0.11, 0.11]
[Cooling: between sizes] [Ready]

--- N = 1000000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
         AVX2 aligned (load_ps/store_ps)      0.10 ns/elem      0.10 ns/elem      0.00  [0.10, 0.10]
AVX2 unaligned (loadu/storeu on aligned ptr)      0.10 ns/elem      0.10 ns/elem      0.00  [0.10, 0.10]
AVX2 unaligned (loadu/storeu on misaligned ptr)      0.12 ns/elem      0.12 ns/elem      0.00  [0.12, 0.12]
[Cooling: between sizes] [Ready]
[Cooling: before random access] [Ready]

================================================================================
  RANDOM ACCESS
================================================================================

Contract: Measures cache behavior with non-sequential access patterns. Alignment has less impact here; tests baseline overhead.

[2026-02-16 04:12:02] Random Access CPU: 2631 MHz (~base: 2631)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      3.71 ns/elem      3.74 ns/elem      0.04  [3.73, 3.76]
                       AlignedVector<64>      3.76 ns/elem      3.75 ns/elem      0.04  [3.73, 3.76]
                      AlignedVector<128>      3.75 ns/elem      3.74 ns/elem      0.04  [3.73, 3.76]
            boost::aligned_allocator<64>      3.87 ns/elem      3.88 ns/elem      0.03  [3.87, 3.89]
                Eigen::aligned_allocator      3.71 ns/elem      3.73 ns/elem      0.04  [3.72, 3.75]
[Cooling: between sizes] [Ready]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      3.76 ns/elem      3.77 ns/elem      0.02  [3.76, 3.78]
                       AlignedVector<64>      3.77 ns/elem      3.77 ns/elem      0.01  [3.77, 3.78]
                      AlignedVector<128>      3.78 ns/elem      3.78 ns/elem      0.02  [3.77, 3.79]
            boost::aligned_allocator<64>      3.76 ns/elem      3.76 ns/elem      0.02  [3.75, 3.77]
                Eigen::aligned_allocator      3.77 ns/elem      3.78 ns/elem      0.03  [3.77, 3.79]
[Cooling: between sizes] [Ready]
[Cooling: before push_back] [Ready]

================================================================================
  PUSH_BACK GROWTH
================================================================================

Contract: Tests amortized O(1) push_back with geometric growth. Compares growing vs pre-reserved scenarios.

[2026-02-16 04:12:13] Push Back CPU: 2445 MHz (~base: 2445)

--- N = 1000 ---
  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      3.99 ns/elem      4.10 ns/elem      0.56  [3.85, 4.35]
                AlignedVector<64> (grow)      3.75 ns/elem      3.79 ns/elem      0.31  [3.66, 3.93]
               AlignedVector<128> (grow)      4.07 ns/elem      5.29 ns/elem      5.39  [2.93, 7.65]
  [NOTE] High variance (stddev > median)
     boost::aligned_allocator<64> (grow)      3.20 ns/elem      3.23 ns/elem      0.13  [3.18, 3.29]
         Eigen::aligned_allocator (grow)      3.18 ns/elem      3.30 ns/elem      0.36  [3.14, 3.46]

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      3.65 ns/elem      3.65 ns/elem      0.01  [3.65, 3.65]
            AlignedVector<64> (reserved)      3.18 ns/elem      3.16 ns/elem      0.05  [3.14, 3.18]
           AlignedVector<128> (reserved)      3.19 ns/elem      3.17 ns/elem      0.04  [3.15, 3.19]
 boost::aligned_allocator<64> (reserved)      1.86 ns/elem      1.87 ns/elem      0.01  [1.86, 1.87]
     Eigen::aligned_allocator (reserved)      2.81 ns/elem      2.80 ns/elem      0.02  [2.80, 2.81]
[Cooling: between sizes] [Ready]

--- N = 10000 ---
  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      5.28 ns/elem      5.37 ns/elem      0.26  [5.26, 5.48]
                AlignedVector<64> (grow)      2.43 ns/elem      2.52 ns/elem      0.22  [2.42, 2.61]
               AlignedVector<128> (grow)      3.34 ns/elem      3.34 ns/elem      0.02  [3.33, 3.35]
     boost::aligned_allocator<64> (grow)      2.67 ns/elem      2.68 ns/elem      0.01  [2.67, 2.68]
         Eigen::aligned_allocator (grow)      2.95 ns/elem      2.96 ns/elem      0.01  [2.95, 2.96]

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      3.18 ns/elem      3.26 ns/elem      0.23  [3.16, 3.36]
            AlignedVector<64> (reserved)      3.11 ns/elem      3.11 ns/elem      0.02  [3.10, 3.12]
           AlignedVector<128> (reserved)      3.11 ns/elem      3.12 ns/elem      0.05  [3.10, 3.14]
 boost::aligned_allocator<64> (reserved)      1.55 ns/elem      1.59 ns/elem      0.17  [1.51, 1.66]
     Eigen::aligned_allocator (reserved)      2.78 ns/elem      2.78 ns/elem      0.00  [2.78, 2.78]
[Cooling: between sizes] [Ready]
[Cooling: before insert] [Ready]

================================================================================
  INSERT AT MIDDLE
================================================================================

Contract: Measures O(n) insertion at middle position. Tests element shifting performance.

[2026-02-16 04:12:24] Insert CPU: 2638 MHz (~base: 2638)

--- Initial size = 1000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector     31.41 ns/insert     34.65 ns/insert      3.97  [32.91, 36.39]
                       AlignedVector<64>     31.95 ns/insert     34.62 ns/insert      3.94  [32.90, 36.35]
                      AlignedVector<128>     32.36 ns/insert     34.86 ns/insert      4.33  [32.97, 36.76]
            boost::aligned_allocator<64>     30.70 ns/insert     33.51 ns/insert      3.68  [31.89, 35.12]
                Eigen::aligned_allocator     30.36 ns/insert     32.77 ns/insert      3.57  [31.20, 34.33]
[Cooling: between sizes] [Ready]

--- Initial size = 10000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector    211.49 ns/insert    214.75 ns/insert     14.59  [208.35, 221.14]
                       AlignedVector<64>    213.94 ns/insert    217.64 ns/insert     14.79  [211.16, 224.12]
                      AlignedVector<128>    213.25 ns/insert    220.92 ns/insert     23.49  [210.62, 231.21]
            boost::aligned_allocator<64>    210.14 ns/insert    236.62 ns/insert     95.08  [194.95, 278.29]
                Eigen::aligned_allocator    210.59 ns/insert    213.85 ns/insert     14.57  [207.47, 220.24]
[Cooling: between sizes] [Ready]
[Cooling: before copy/move] [Ready]

================================================================================
  COPY AND MOVE OPERATIONS
================================================================================

Contract: Copy construction should be O(n). Move construction/assignment should be O(1). This section reports: copy-ctor, move-ctor (rotation), move-assign (rotation).

[2026-02-16 04:12:35] Copy/Move CPU: 2445 MHz (~base: 2445)

--- N = 10000 ---
  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor   1037.00 ns/op   1248.40 ns/op    296.52  [1118.44, 1378.36]
             AlignedVector<64> copy-ctor    996.50 ns/op   1098.05 ns/op    246.53  [990.00, 1206.10]
            AlignedVector<128> copy-ctor   1012.00 ns/op   1294.55 ns/op    741.23  [969.69, 1619.41]
  boost::aligned_allocator<64> copy-ctor    942.00 ns/op   1014.50 ns/op    119.92  [961.94, 1067.06]
      Eigen::aligned_allocator copy-ctor    957.00 ns/op   1199.70 ns/op    421.93  [1014.78, 1384.62]

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor      3.01 ns/op      3.01 ns/op      0.02  [3.00, 3.02]
             AlignedVector<64> move-ctor      3.00 ns/op      3.00 ns/op      0.01  [2.99, 3.00]
            AlignedVector<128> move-ctor      3.00 ns/op      3.00 ns/op      0.01  [2.99, 3.00]
  boost::aligned_allocator<64> move-ctor      3.01 ns/op      2.99 ns/op      0.10  [2.94, 3.04]
      Eigen::aligned_allocator move-ctor      3.01 ns/op      3.01 ns/op      0.01  [3.00, 3.01]
[Cooling: between sizes] [Ready]

--- N = 100000 ---
  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor  14382.00 ns/op  14718.00 ns/op   1000.25  [14279.62, 15156.38]
             AlignedVector<64> copy-ctor  14367.00 ns/op  14861.70 ns/op   1422.23  [14238.38, 15485.02]
            AlignedVector<128> copy-ctor  14452.00 ns/op  14760.60 ns/op   1072.58  [14290.52, 15230.68]
  boost::aligned_allocator<64> copy-ctor  14216.50 ns/op  14476.05 ns/op    736.05  [14153.46, 14798.64]
      Eigen::aligned_allocator copy-ctor  14342.00 ns/op  15187.80 ns/op   2445.09  [14116.19, 16259.41]

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor      3.11 ns/op      3.12 ns/op      0.11  [3.07, 3.16]
             AlignedVector<64> move-ctor      3.01 ns/op      3.01 ns/op      0.01  [3.00, 3.01]
            AlignedVector<128> move-ctor      3.00 ns/op      3.00 ns/op      0.01  [3.00, 3.01]
  boost::aligned_allocator<64> move-ctor      3.22 ns/op      3.50 ns/op      0.94  [3.08, 3.91]
      Eigen::aligned_allocator move-ctor      3.01 ns/op      3.06 ns/op      0.09  [3.02, 3.10]
[Cooling: between sizes] [Ready]
[Cooling: before corner cases] [Ready]

================================================================================
  CORNER CASES
================================================================================

Contract: Tests edge cases: empty vector operations, single element, capacity boundary.

[2026-02-16 04:12:46] Corner Cases CPU: 2635 MHz (~base: 2635)

--- Empty Vector Operations ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
             std::vector empty begin/end      4.63 ns/op      4.67 ns/op      0.07  [4.64, 4.70]
       AlignedVector<64> empty begin/end      2.78 ns/op      2.85 ns/op      0.19  [2.76, 2.93]
      AlignedVector<128> empty begin/end      4.63 ns/op      4.67 ns/op      0.05  [4.64, 4.69]
boost::aligned_allocator<64> empty begin/end      4.70 ns/op      4.68 ns/op      0.05  [4.66, 4.70]
Eigen::aligned_allocator empty begin/end      4.63 ns/op      4.66 ns/op      0.04  [4.64, 4.67]

--- Single Element Push/Pop Cycle ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                    std::vector push/pop      5.86 ns/cycle      5.94 ns/cycle      0.22  [5.84, 6.04]
              AlignedVector<64> push/pop      5.86 ns/cycle      5.94 ns/cycle      0.22  [5.84, 6.03]
             AlignedVector<128> push/pop      5.87 ns/cycle      5.86 ns/cycle      0.00  [5.86, 5.87]
   boost::aligned_allocator<64> push/pop      3.40 ns/cycle      3.44 ns/cycle      0.17  [3.36, 3.51]
       Eigen::aligned_allocator push/pop      5.56 ns/cycle      5.59 ns/cycle      0.16  [5.52, 5.67]

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  ALIGNMENT VERIFICATION
================================================================================

Contract: Verifies alignment guarantees across multiple allocations. All allocations must meet specified alignment.

[2026-02-16 04:54:05] Alignment Verification CPU: 2445 MHz (base: 2445)

--- Alignment = 16 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 32 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 64 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 128 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 256 bytes ---

  Allocations: 1000, Violations: 0 [PASS]
[Cooling: before sequential iteration] [Ready: 2445 MHz]

================================================================================
  SEQUENTIAL ITERATION (Sum)
================================================================================

Contract: Measures cache-line alignment benefit for SIMD auto-vectorization.

[2026-02-16 04:54:09] Sequential Iteration CPU: 2445 MHz (base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.93 ns/elem      0.93 ns/elem      0.01  [0.93, 0.94]
                       AlignedVector<64>      0.93 ns/elem      0.93 ns/elem      0.01  [0.93, 0.94]
                      AlignedVector<128>      0.94 ns/elem      0.94 ns/elem      0.01  [0.94, 0.94]
            boost::aligned_allocator<64>      0.94 ns/elem      0.93 ns/elem      0.01  [0.93, 0.94]
[Cooling: between sizes] [Ready: 2445 MHz]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
                       AlignedVector<64>      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
                      AlignedVector<128>      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
            boost::aligned_allocator<64>      0.94 ns/elem      0.94 ns/elem      0.01  [0.94, 0.95]
[Cooling: between sizes] [Ready: 2445 MHz]
[Cooling: before SIMD dot product] [Ready: 2445 MHz]

================================================================================
  SIMD DOT PRODUCT
================================================================================

Contract: Tests fused multiply-add vectorization potential. assume_aligned() enables compiler to use aligned SIMD loads.

[2026-02-16 04:54:19] Dot Product CPU: 2445 MHz (base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.94 ns/elem      0.94 ns/elem      0.01  [0.93, 0.94]
                       AlignedVector<64>      0.93 ns/elem      0.93 ns/elem      0.01  [0.93, 0.94]
                      AlignedVector<128>      0.93 ns/elem      0.94 ns/elem      0.01  [0.93, 0.94]
            boost::aligned_allocator<64>      0.93 ns/elem      0.93 ns/elem      0.01  [0.93, 0.94]
[Cooling: between sizes] [Ready: 2445 MHz]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
                       AlignedVector<64>      0.94 ns/elem      0.94 ns/elem      0.01  [0.94, 0.95]
                      AlignedVector<128>      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
            boost::aligned_allocator<64>      0.94 ns/elem      0.94 ns/elem      0.00  [0.94, 0.94]
[Cooling: between sizes] [Ready: 2445 MHz]
[Cooling: before SIMD SAXPY] [Ready: 2445 MHz]

================================================================================
  SIMD SAXPY (Explicit AVX2)
================================================================================

[SKIPPED] AVX2 not available on this platform
[Cooling: before random access] [Ready: 2445 MHz]

================================================================================
  RANDOM ACCESS
================================================================================

Contract: Measures cache behavior with non-sequential access patterns. Alignment has less impact here; tests baseline overhead.

[2026-02-16 04:54:32] Random Access CPU: 2445 MHz (base: 2445)

--- N = 10000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      1.55 ns/elem      1.59 ns/elem      0.13  [1.53, 1.65]
                       AlignedVector<64>      2.47 ns/elem      2.50 ns/elem      0.07  [2.47, 2.53]
                      AlignedVector<128>      2.47 ns/elem      2.52 ns/elem      0.11  [2.47, 2.57]
            boost::aligned_allocator<64>      1.55 ns/elem      1.56 ns/elem      0.03  [1.54, 1.57]
[Cooling: between sizes] [Ready: 2445 MHz]

--- N = 100000 ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector      1.62 ns/elem      1.64 ns/elem      0.05  [1.62, 1.66]
                       AlignedVector<64>      2.55 ns/elem      2.55 ns/elem      0.03  [2.54, 2.56]
                      AlignedVector<128>      2.54 ns/elem      2.55 ns/elem      0.03  [2.54, 2.56]
            boost::aligned_allocator<64>      1.64 ns/elem      1.64 ns/elem      0.04  [1.63, 1.66]
[Cooling: between sizes] [Ready: 2445 MHz]
[Cooling: before push_back] [Ready: 2445 MHz]

================================================================================
  PUSH_BACK GROWTH
================================================================================

Contract: Tests amortized O(1) push_back with geometric growth. Compares growing vs pre-reserved scenarios.

[2026-02-16 04:54:41] Push Back CPU: 2445 MHz (base: 2445)

--- N = 1000 ---
  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      4.40 ns/elem      4.89 ns/elem      1.06  [4.43, 5.35]
                AlignedVector<64> (grow)      5.05 ns/elem      5.67 ns/elem      1.66  [4.94, 6.39]
               AlignedVector<128> (grow)      4.50 ns/elem      4.69 ns/elem      0.86  [4.31, 5.07]
     boost::aligned_allocator<64> (grow)      4.40 ns/elem      5.06 ns/elem      1.75  [4.30, 5.83]

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      2.90 ns/elem      2.88 ns/elem      0.07  [2.86, 2.91]
            AlignedVector<64> (reserved)      3.05 ns/elem      3.04 ns/elem      0.09  [3.00, 3.07]
           AlignedVector<128> (reserved)      3.50 ns/elem      3.49 ns/elem      0.04  [3.47, 3.51]
 boost::aligned_allocator<64> (reserved)      2.80 ns/elem      2.78 ns/elem      0.06  [2.76, 2.81]
[Cooling: between sizes] [Ready: 2445 MHz]

--- N = 10000 ---
  [Growing - no reserve]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                      std::vector (grow)      3.01 ns/elem      3.43 ns/elem      1.12  [2.94, 3.92]
                AlignedVector<64> (grow)      3.75 ns/elem      3.79 ns/elem      0.14  [3.72, 3.85]
               AlignedVector<128> (grow)      3.47 ns/elem      3.54 ns/elem      0.26  [3.43, 3.66]
     boost::aligned_allocator<64> (grow)      2.66 ns/elem      2.67 ns/elem      0.02  [2.65, 2.68]

  [Pre-reserved]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                  std::vector (reserved)      2.47 ns/elem      2.57 ns/elem      0.45  [2.38, 2.77]
            AlignedVector<64> (reserved)      2.53 ns/elem      2.53 ns/elem      0.01  [2.52, 2.53]
           AlignedVector<128> (reserved)      3.10 ns/elem      3.10 ns/elem      0.01  [3.09, 3.10]
 boost::aligned_allocator<64> (reserved)      2.47 ns/elem      2.49 ns/elem      0.08  [2.45, 2.52]
[Cooling: between sizes] [Ready: 2445 MHz]
[Cooling: before insert] [Ready: 2445 MHz]

================================================================================
  INSERT AT MIDDLE
================================================================================

Contract: Measures O(n) insertion at middle position. Tests element shifting performance.

[2026-02-16 04:54:50] Insert CPU: 2445 MHz (base: 2445)

--- Initial size = 1000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector     46.00 ns/insert     50.35 ns/insert     10.33  [45.82, 54.88]
                       AlignedVector<64>     48.00 ns/insert     52.75 ns/insert     15.88  [45.79, 59.71]
                      AlignedVector<128>     48.50 ns/insert     51.00 ns/insert      9.71  [46.75, 55.25]
            boost::aligned_allocator<64>     51.00 ns/insert     57.30 ns/insert     15.89  [50.33, 64.27]
[Cooling: between sizes] [Ready: 2445 MHz]

--- Initial size = 10000, inserting 100 elements ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                             std::vector    405.00 ns/insert    404.90 ns/insert      2.05  [404.00, 405.80]
                       AlignedVector<64>    411.00 ns/insert    424.70 ns/insert     60.83  [398.04, 451.36]
                      AlignedVector<128>    413.00 ns/insert    427.90 ns/insert     67.38  [398.37, 457.43]
            boost::aligned_allocator<64>    442.00 ns/insert    448.70 ns/insert     29.32  [435.85, 461.55]
[Cooling: between sizes] [Ready: 2445 MHz]
[Cooling: before copy/move] [Ready: 2445 MHz]

================================================================================
  COPY AND MOVE OPERATIONS
================================================================================

Contract: Copy construction should be O(n). Move construction/assignment should be O(1). This section reports: copy-ctor, move-ctor (rotation), move-assign (rotation).

[2026-02-16 04:54:59] Copy/Move CPU: 2445 MHz (base: 2445)

--- N = 10000 ---
  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor   1000.00 ns/op    980.00 ns/op     61.56  [953.02, 1006.98]
             AlignedVector<64> copy-ctor   1000.00 ns/op    990.00 ns/op     96.79  [947.58, 1032.42]
            AlignedVector<128> copy-ctor   1000.00 ns/op    980.00 ns/op     69.59  [949.50, 1010.50]
  boost::aligned_allocator<64> copy-ctor   3200.00 ns/op   3245.00 ns/op     51.04  [3222.63, 3267.37]

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor     11.27 ns/op     11.26 ns/op      0.55  [11.02, 11.50]
             AlignedVector<64> move-ctor     12.67 ns/op     13.52 ns/op      2.68  [12.35, 14.69]
            AlignedVector<128> move-ctor     12.77 ns/op     13.36 ns/op      1.76  [12.59, 14.13]
  boost::aligned_allocator<64> move-ctor     11.17 ns/op     11.18 ns/op      0.37  [11.02, 11.34]
[Cooling: between sizes] [Ready: 2445 MHz]

--- N = 100000 ---
  [Copy Construction]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector copy-ctor  16850.00 ns/op  25565.00 ns/op  26882.50  [13783.23, 37346.77]
  [NOTE] High variance (stddev > median)
             AlignedVector<64> copy-ctor  16800.00 ns/op  36490.00 ns/op  35830.67  [20786.52, 52193.48]
  [NOTE] High variance (stddev > median)
            AlignedVector<128> copy-ctor  17050.00 ns/op  48650.00 ns/op  41370.85  [30518.43, 66781.57]
  [NOTE] High variance (stddev > median)
  boost::aligned_allocator<64> copy-ctor 107050.00 ns/op  76370.00 ns/op  42429.73  [57774.35, 94965.65]

  [Move Construction (rotation)]

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector move-ctor     11.12 ns/op     11.13 ns/op      0.57  [10.88, 11.38]
             AlignedVector<64> move-ctor     12.67 ns/op     12.70 ns/op      0.11  [12.65, 12.74]
            AlignedVector<128> move-ctor     12.77 ns/op     13.48 ns/op      1.90  [12.64, 14.31]
  boost::aligned_allocator<64> move-ctor     11.13 ns/op     11.23 ns/op      0.32  [11.09, 11.37]
[Cooling: between sizes] [Ready: 2445 MHz]
[Cooling: before corner cases] [Ready: 2445 MHz]

================================================================================
  CORNER CASES
================================================================================

Contract: Tests edge cases: empty vector operations, single element, capacity boundary.

[2026-02-16 04:55:09] Corner Cases CPU: 2445 MHz (base: 2445)

--- Empty Vector Operations ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
             std::vector empty begin/end      3.08 ns/op      3.11 ns/op      0.05  [3.09, 3.14]
       AlignedVector<64> empty begin/end      5.05 ns/op      5.04 ns/op      0.12  [4.99, 5.10]
      AlignedVector<128> empty begin/end      4.93 ns/op      4.98 ns/op      0.09  [4.94, 5.02]
boost::aligned_allocator<64> empty begin/end      3.09 ns/op      3.17 ns/op      0.12  [3.12, 3.22]

--- Single Element Push/Pop Cycle ---

                               Container          Median            Mean    Stddev  CI95
-------------------------------------------------------------------------------
                    std::vector push/pop      5.56 ns/cycle      5.68 ns/cycle      0.35  [5.52, 5.83]
              AlignedVector<64> push/pop      4.64 ns/cycle      4.69 ns/cycle      0.24  [4.59, 4.80]
             AlignedVector<128> push/pop      5.25 ns/cycle      5.32 ns/cycle      0.28  [5.19, 5.44]
   boost::aligned_allocator<64> push/pop      5.56 ns/cycle      5.62 ns/cycle      0.20  [5.53, 5.71]

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- Eigen was not detected on Local.
- Eigen was not detected on MSVC CI.
