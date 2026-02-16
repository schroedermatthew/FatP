---
doc_id: BR-PolicyIterator-001
doc_type: "Benchmark Results"
title: "PolicyIterator"
fatp_components: ["PolicyIterator"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - PolicyIterator

**Source:** `benchmark_PolicyIterator.cpp`
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
| fat_p::PolicyIterator | x | x | x | x |
| Raw pointer iteration | x | x | x | x |
| Manual loop | x | x | x | x |
| Boost.Iterator | x | x | x | x |
| range-v3 | x | x | x | — |
| Eigen | x | x | x | — |
| xtensor | x | x | x | — |
| Boost.MultiArray | x | x | x | x |
| range-v3 (range/v3/view/filter.hpp) | — | — | — | — |
| Eigen (Eigen/Dense) | — | — | — | — |
| xtensor (requires C++20) | — | — | — | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
[2026-02-15 19:55:00] Benchmark start CPU: 2653 MHz (base: 3686)

================================================================================
  STANDARD ITERATION VS RAW POINTER
================================================================================

  Contract: Sequential sum accumulation, no predicate/transform

[2026-02-15 19:55:00] Start CPU: 2838 MHz (base: 3686)
  Results (ns/element):
    Raw pointer             :     0.25 ns/op  (+/-  0.00)
    PolicyIterator<Standard>:     0.25 ns/op  (+/-  0.00)

  Overhead ratio: 1.03x

================================================================================
  STRIDE POLICIES VS MANUAL LOOPS + RANGE-V3
================================================================================

  Contract: Sum with fixed compile-time stride, bounds clamping included

[2026-02-15 19:55:02] Start CPU: 2838 MHz (base: 3686)

  --- Stride 2 ---

    Manual loop             :     0.52 ns/op  (+/-  0.14)
    PolicyIterator<Stride>  :     0.58 ns/op  (+/-  0.20)
    range-v3::views::stride :     1.78 ns/op  (+/-  0.48)
    vs Manual: Policy 1.11x, range-v3 3.42x

  --- Stride 4 ---

    Manual loop             :     0.58 ns/op  (+/-  0.26)
    PolicyIterator<Stride>  :     0.63 ns/op  (+/-  0.29)
    range-v3::views::stride :     1.12 ns/op  (+/-  0.66)
    vs Manual: Policy 1.10x, range-v3 1.95x

  --- Stride 8 ---

    Manual loop             :     1.20 ns/op  (+/-  0.06)
    PolicyIterator<Stride>  :     1.20 ns/op  (+/-  0.15)
    range-v3::views::stride :     1.63 ns/op  (+/-  0.08)
    vs Manual: Policy 1.00x, range-v3 1.36x

================================================================================
  FILTER POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of even values only, predicate evaluation cost included

[2026-02-15 19:55:04] Start CPU: 2506 MHz (base: 3686)
  Match ratio: 50.0%

  Results (ns/element scanned):
    Manual if-check loop    :     3.06 ns/op  (+/-  1.18)
    PolicyIterator<Filter>  :     3.25 ns/op  (+/-  0.88)
    Boost.filter_iterator   :     3.29 ns/op  (+/-  0.75)
    range-v3::views::filter :     3.33 ns/op  (+/-  1.57)

  vs Manual:
    PolicyIterator: 1.06x
    Boost:          1.07x
    range-v3:       1.09x

================================================================================
  TRANSFORM POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of doubled values, transform cost included

[2026-02-15 19:55:07] Start CPU: 2690 MHz (base: 3686)
  Results (ns/element):
    Manual transform loop   :     0.38 ns/op  (+/-  0.01)
    PolicyIterator<Transform>:     0.38 ns/op  (+/-  0.01)
    Boost.transform_iterator:     0.38 ns/op  (+/-  0.02)
    range-v3::views::transform:     0.38 ns/op  (+/-  0.01)

  vs Manual:
    PolicyIterator: 1.01x
    Boost:          1.01x
    range-v3:       1.01x

================================================================================
  TENSOR STRIDE POLICY VS MANUAL + EIGEN + XTENSOR + BOOST
================================================================================

  Contract: Row-major matrix column iteration (strided access)

[2026-02-15 19:55:09] Start CPU: 2285 MHz (base: 3686)
  Matrix: 1000 x 1000 (1000000 elements)
  Task: Sum first column (stride = 1000)

  Results (ns/row):
    Manual r*cols indexing  :     1.00 ns/op  (+/-  0.04)
    TensorStridePolicy      :     3.90 ns/op  (+/-  0.08)
    Stride1DPolicy          :     1.00 ns/op  (+/-  0.04)
    Eigen::col(0).sum()     :     0.70 ns/op  (+/-  0.05)
    xtensor strided_view    :     4.70 ns/op  (+/-  0.11)
    Boost.MultiArray [][0]  :     1.00 ns/op  (+/-  0.04)

  vs Manual:
    TensorStridePolicy: 3.90x
    Stride1DPolicy:     1.00x
    Eigen:              0.70x
    xtensor:            4.70x
    Boost:              1.00x

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 3.90 ns
    xtensor:            4.70 ns (0.83x)

================================================================================
  TENSOR: CONTIGUOUS VS NON-CONTIGUOUS VS SPECIALIZED + XTENSOR + BOOST
================================================================================

  Contract: Compare general TensorStridePolicy vs lightweight 1D/2D policies

[2026-02-15 19:55:11] Start CPU: 2285 MHz (base: 3686)
  Matrix: 1000 x 1000 (1000000 elements)

  Full matrix iteration (ns/element):
    Manual flat loop        :     0.28 ns/op  (+/-  0.17)
    TensorStridePolicy (general):     1.67 ns/op  (+/-  0.81)
    Stride2DPolicy (specialized):     0.32 ns/op  (+/-  0.18)
    xtensor iterator        :     0.68 ns/op  (+/-  0.28)
    Boost.MultiArray data() :     0.42 ns/op  (+/-  0.17)

  Column iteration (ns/row, stride=1000):
    TensorStridePolicy (general):     3.40 ns/op  (+/-  2.94)
    Stride1DPolicy (specialized):     1.10 ns/op  (+/-  0.37)
    xtensor strided_view    :     4.50 ns/op  (+/-  3.73)

  vs Manual (full matrix):
    TensorStridePolicy: 5.90x
    Stride2DPolicy:     1.12x
    xtensor:            2.40x
    Boost:              1.47x

  Stride1D vs TensorStride (column iteration):
    TensorStridePolicy: 3.40 ns/row
    Stride1DPolicy:     1.10 ns/row (0.32x)

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 3.40 ns
    xtensor:            4.50 ns (0.76x)

================================================================================
  2D MATRIX FULL ITERATION: POLICIES VS EIGEN
================================================================================

  Contract: Sum all elements of 1000x1000 row-major matrix

[2026-02-15 19:55:13] Start CPU: 2506 MHz (base: 3686)
  Matrix: 1000 x 1000

  Results (ns/element):
    Manual flat loop        :     0.23 ns/op  (+/-  0.15)
    Stride2DPolicy          :     0.25 ns/op  (+/-  0.21)
    TensorStridePolicy 2D   :     0.91 ns/op  (+/-  0.58)
    Eigen::sum()            :     0.19 ns/op  (+/-  0.07)
    Eigen coeff (r,c) loop  :     0.24 ns/op  (+/-  0.13)

  vs Manual:
    Stride2DPolicy:     1.09x
    TensorStridePolicy: 3.95x
    Eigen::sum():       0.82x
    Eigen coeff loop:   1.03x

================================================================================
  TENSOR ITERATION COMPOSITION HELPERS
================================================================================

  Contract: Compare iterateND composition vs manual loops vs TensorStridePolicy

[2026-02-15 19:55:15] Start CPU: 2985 MHz (base: 3686)
  3D Tensor: 50 x 100 x 200 (1000000 elements)
  4D Tensor: 8 x 16 x 50 x 100 (640000 elements)

  3D Results (ns/element):
    Manual nested loop      :     0.42 ns/op  (+/-  0.18)
    TensorStridePolicy      :     1.41 ns/op  (+/-  0.59)
    iterateND (composition) :     0.86 ns/op  (+/-  0.35)

  3D vs Manual:
    TensorStridePolicy:     3.37x
    iterateND (composition): 2.06x

  4D Results (ns/element):
    Manual nested loop      :     0.42 ns/op  (+/-  0.17)
    TensorStridePolicy      :     1.47 ns/op  (+/-  0.62)
    iterateND (composition) :     0.82 ns/op  (+/-  0.34)

  4D vs Manual:
    TensorStridePolicy:     3.52x
    iterateND (composition): 1.97x

  Composition vs TensorStridePolicy:
    3D: iterateND is 1.63x faster
    4D: iterateND is 1.79x faster

================================================================================
  SIZE SCALING (CACHE EFFECTS)
================================================================================

  Contract: Standard iteration sum at different data sizes

[2026-02-15 19:55:17] Start CPU: 2727 MHz (base: 3686)

  --- L1 (4K) (512 elements) ---

    Raw pointer             :     0.78 ns/op  (+/-  0.09)
    PolicyIterator          :     0.78 ns/op  (+/-  0.09)
    Overhead ratio: 1.00x

  --- L2 (32K) (4096 elements) ---

    Raw pointer             :     0.61 ns/op  (+/-  0.01)
    PolicyIterator          :     0.61 ns/op  (+/-  0.01)
    Overhead ratio: 1.00x

  --- L3 (512K) (65536 elements) ---

    Raw pointer             :     0.60 ns/op  (+/-  0.07)
    PolicyIterator          :     0.60 ns/op  (+/-  0.14)
    Overhead ratio: 0.99x

  --- RAM (8M) (1048576 elements) ---

    Raw pointer             :     0.34 ns/op  (+/-  0.18)
    PolicyIterator          :     0.33 ns/op  (+/-  0.17)
    Overhead ratio: 0.97x

================================================================================
  BENCHMARK COMPLETE
================================================================================

[2026-02-15 19:55:17] Final state CPU: 2727 MHz (base: 3686)
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
[2026-02-16 03:37:50] Benchmark start CPU: 3237 MHz (~base: 3237)

================================================================================
  STANDARD ITERATION VS RAW POINTER
================================================================================

  Contract: Sequential sum accumulation, no predicate/transform

[2026-02-16 03:37:50] Start CPU: 3237 MHz (~base: 3237)
  Results (ns/element):
    Raw pointer             :     0.33 ns/op  (+/-  0.01)
    PolicyIterator<Standard>:     0.33 ns/op  (+/-  0.01)

  Overhead ratio: 1.00x

================================================================================
  STRIDE POLICIES VS MANUAL LOOPS + RANGE-V3
================================================================================

  Contract: Sum with fixed compile-time stride, bounds clamping included

[2026-02-16 03:37:51] Start CPU: 2445 MHz (~base: 2445)

  --- Stride 2 ---

    Manual loop             :     0.37 ns/op  (+/-  0.03)
    PolicyIterator<Stride>  :     0.45 ns/op  (+/-  0.02)
    range-v3::views::stride :     0.70 ns/op  (+/-  0.01)
    vs Manual: Policy 1.21x, range-v3 1.90x

  --- Stride 4 ---

    Manual loop             :     0.56 ns/op  (+/-  0.02)
    PolicyIterator<Stride>  :     0.63 ns/op  (+/-  0.02)
    range-v3::views::stride :     0.72 ns/op  (+/-  0.02)
    vs Manual: Policy 1.12x, range-v3 1.27x

  --- Stride 8 ---

    Manual loop             :     1.10 ns/op  (+/-  0.05)
    PolicyIterator<Stride>  :     1.10 ns/op  (+/-  0.06)
    range-v3::views::stride :     1.12 ns/op  (+/-  0.06)
    vs Manual: Policy 0.99x, range-v3 1.01x

================================================================================
  FILTER POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of even values only, predicate evaluation cost included

[2026-02-16 03:37:52] Start CPU: 3240 MHz (~base: 3240)
  Match ratio: 50.0%

  Results (ns/element scanned):
    Manual if-check loop    :     3.85 ns/op  (+/-  0.21)
    PolicyIterator<Filter>  :     4.11 ns/op  (+/-  0.32)
    Boost.filter_iterator   :     3.85 ns/op  (+/-  0.23)
    range-v3::views::filter :     3.86 ns/op  (+/-  0.24)

  vs Manual:
    PolicyIterator: 1.07x
    Boost:          1.00x
    range-v3:       1.00x

================================================================================
  TRANSFORM POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of doubled values, transform cost included

[2026-02-16 03:37:53] Start CPU: 3135 MHz (~base: 3135)
  Results (ns/element):
    Manual transform loop   :     0.37 ns/op  (+/-  0.01)
    PolicyIterator<Transform>:     0.36 ns/op  (+/-  0.01)
    Boost.transform_iterator:     0.36 ns/op  (+/-  0.01)
    range-v3::views::transform:     0.36 ns/op  (+/-  0.02)

  vs Manual:
    PolicyIterator: 0.99x
    Boost:          0.99x
    range-v3:       0.99x

================================================================================
  TENSOR STRIDE POLICY VS MANUAL + EIGEN + XTENSOR + BOOST
================================================================================

  Contract: Row-major matrix column iteration (strided access)

[2026-02-16 03:37:55] Start CPU: 3140 MHz (~base: 3140)
  Matrix: 1000 x 1000 (1000000 elements)
  Task: Sum first column (stride = 1000)

  Results (ns/row):
    Manual r*cols indexing  :     0.65 ns/op  (+/-  0.01)
    TensorStridePolicy      :     2.16 ns/op  (+/-  0.05)
    Stride1DPolicy          :     0.65 ns/op  (+/-  0.01)
    Eigen::col(0).sum()     :     0.65 ns/op  (+/-  0.01)
    xtensor strided_view    :     2.83 ns/op  (+/-  0.01)
    Boost.MultiArray [][0]  :     0.65 ns/op  (+/-  0.01)

  vs Manual:
    TensorStridePolicy: 3.32x
    Stride1DPolicy:     1.00x
    Eigen:              1.00x
    xtensor:            4.34x
    Boost:              1.00x

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 2.16 ns
    xtensor:            2.83 ns (0.77x)

================================================================================
  TENSOR: CONTIGUOUS VS NON-CONTIGUOUS VS SPECIALIZED + XTENSOR + BOOST
================================================================================

  Contract: Compare general TensorStridePolicy vs lightweight 1D/2D policies

[2026-02-16 03:37:56] Start CPU: 2445 MHz (~base: 2445)
  Matrix: 1000 x 1000 (1000000 elements)

  Full matrix iteration (ns/element):
    Manual flat loop        :     0.33 ns/op  (+/-  0.00)
    TensorStridePolicy (general):     1.25 ns/op  (+/-  0.02)
    Stride2DPolicy (specialized):     0.94 ns/op  (+/-  0.00)
    xtensor iterator        :     0.41 ns/op  (+/-  0.02)
    Boost.MultiArray data() :     0.34 ns/op  (+/-  0.01)

  Column iteration (ns/row, stride=1000):
    TensorStridePolicy (general):     3.02 ns/op  (+/-  0.22)
    Stride1DPolicy (specialized):     0.66 ns/op  (+/-  0.03)
    xtensor strided_view    :     3.18 ns/op  (+/-  0.43)

  vs Manual (full matrix):
    TensorStridePolicy: 3.77x
    Stride2DPolicy:     2.84x
    xtensor:            1.25x
    Boost:              1.03x

  Stride1D vs TensorStride (column iteration):
    TensorStridePolicy: 3.02 ns/row
    Stride1DPolicy:     0.66 ns/row (0.22x)

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 3.02 ns
    xtensor:            3.18 ns (0.95x)

================================================================================
  2D MATRIX FULL ITERATION: POLICIES VS EIGEN
================================================================================

  Contract: Sum all elements of 1000x1000 row-major matrix

[2026-02-16 03:37:57] Start CPU: 2445 MHz (~base: 2445)
  Matrix: 1000 x 1000

  Results (ns/element):
    Manual flat loop        :     0.33 ns/op  (+/-  0.01)
    Stride2DPolicy          :     0.63 ns/op  (+/-  0.01)
    TensorStridePolicy 2D   :     1.25 ns/op  (+/-  0.02)
    Eigen::sum()            :     0.18 ns/op  (+/-  0.03)
    Eigen coeff (r,c) loop  :     0.35 ns/op  (+/-  0.01)

  vs Manual:
    Stride2DPolicy:     1.90x
    TensorStridePolicy: 3.75x
    Eigen::sum():       0.55x
    Eigen coeff loop:   1.04x

================================================================================
  TENSOR ITERATION COMPOSITION HELPERS
================================================================================

  Contract: Compare iterateND composition vs manual loops vs TensorStridePolicy

[2026-02-16 03:37:58] Start CPU: 2445 MHz (~base: 2445)
  3D Tensor: 50 x 100 x 200 (1000000 elements)
  4D Tensor: 8 x 16 x 50 x 100 (640000 elements)

  3D Results (ns/element):
    Manual nested loop      :     0.41 ns/op  (+/-  0.02)
    TensorStridePolicy      :     1.25 ns/op  (+/-  0.01)
    iterateND (composition) :     0.54 ns/op  (+/-  0.01)

  3D vs Manual:
    TensorStridePolicy:     3.05x
    iterateND (composition): 1.32x

  4D Results (ns/element):
    Manual nested loop      :     0.24 ns/op  (+/-  0.04)
    TensorStridePolicy      :     1.56 ns/op  (+/-  0.12)
    iterateND (composition) :     0.51 ns/op  (+/-  0.01)

  4D vs Manual:
    TensorStridePolicy:     6.58x
    iterateND (composition): 2.14x

  Composition vs TensorStridePolicy:
    3D: iterateND is 2.31x faster
    4D: iterateND is 3.07x faster

================================================================================
  SIZE SCALING (CACHE EFFECTS)
================================================================================

  Contract: Standard iteration sum at different data sizes

[2026-02-16 03:37:59] Start CPU: 2642 MHz (~base: 2642)

  --- L1 (4K) (512 elements) ---

    Raw pointer             :     0.24 ns/op  (+/-  0.02)
    PolicyIterator          :     0.24 ns/op  (+/-  0.03)
    Overhead ratio: 1.00x

  --- L2 (32K) (4096 elements) ---

    Raw pointer             :     0.18 ns/op  (+/-  0.01)
    PolicyIterator          :     0.18 ns/op  (+/-  0.01)
    Overhead ratio: 1.01x

  --- L3 (512K) (65536 elements) ---

    Raw pointer             :     0.12 ns/op  (+/-  0.02)
    PolicyIterator          :     0.12 ns/op  (+/-  0.10)
    Overhead ratio: 1.00x

  --- RAM (8M) (1048576 elements) ---

    Raw pointer             :     0.20 ns/op  (+/-  0.03)
    PolicyIterator          :     0.19 ns/op  (+/-  0.03)
    Overhead ratio: 0.96x
================================================================================
  BENCHMARK COMPLETE
================================================================================

[2026-02-16 03:37:59] Final state CPU: 2445 MHz (~base: 2445)
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
[2026-02-16 04:11:28] Benchmark start CPU: 3240 MHz (~base: 3240)

================================================================================
  STANDARD ITERATION VS RAW POINTER
================================================================================

  Contract: Sequential sum accumulation, no predicate/transform

[2026-02-16 04:11:28] Start CPU: 3240 MHz (~base: 3240)
  Results (ns/element):
    Raw pointer             :     0.15 ns/op  (+/-  0.02)
    PolicyIterator<Standard>:     0.15 ns/op  (+/-  0.02)

  Overhead ratio: 0.99x

================================================================================
  STRIDE POLICIES VS MANUAL LOOPS + RANGE-V3
================================================================================

  Contract: Sum with fixed compile-time stride, bounds clamping included

[2026-02-16 04:11:29] Start CPU: 3233 MHz (~base: 3233)

  --- Stride 2 ---

    Manual loop             :     0.37 ns/op  (+/-  0.02)
    PolicyIterator<Stride>  :     0.95 ns/op  (+/-  0.02)
    range-v3::views::stride :     0.50 ns/op  (+/-  0.02)
    vs Manual: Policy 2.60x, range-v3 1.37x

  --- Stride 4 ---

    Manual loop             :     0.59 ns/op  (+/-  0.03)
    PolicyIterator<Stride>  :     1.01 ns/op  (+/-  0.05)
    range-v3::views::stride :     0.61 ns/op  (+/-  0.05)
    vs Manual: Policy 1.72x, range-v3 1.03x

  --- Stride 8 ---

    Manual loop             :     0.91 ns/op  (+/-  0.12)
    PolicyIterator<Stride>  :     1.15 ns/op  (+/-  0.13)
    range-v3::views::stride :     0.93 ns/op  (+/-  0.13)
    vs Manual: Policy 1.27x, range-v3 1.02x

================================================================================
  FILTER POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of even values only, predicate evaluation cost included

[2026-02-16 04:11:30] Start CPU: 2445 MHz (~base: 2445)
  Match ratio: 50.0%

  Results (ns/element scanned):
    Manual if-check loop    :     3.66 ns/op  (+/-  0.06)
    PolicyIterator<Filter>  :     4.14 ns/op  (+/-  0.01)
    Boost.filter_iterator   :     3.97 ns/op  (+/-  0.05)
    range-v3::views::filter :     3.76 ns/op  (+/-  0.01)

  vs Manual:
    PolicyIterator: 1.13x
    Boost:          1.09x
    range-v3:       1.03x

================================================================================
  TRANSFORM POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of doubled values, transform cost included

[2026-02-16 04:11:31] Start CPU: 2445 MHz (~base: 2445)
  Results (ns/element):
    Manual transform loop   :     0.12 ns/op  (+/-  0.02)
    PolicyIterator<Transform>:     0.12 ns/op  (+/-  0.02)
    Boost.transform_iterator:     0.12 ns/op  (+/-  0.02)
    range-v3::views::transform:     0.11 ns/op  (+/-  0.02)

  vs Manual:
    PolicyIterator: 1.00x
    Boost:          0.99x
    range-v3:       0.95x

================================================================================
  TENSOR STRIDE POLICY VS MANUAL + EIGEN + XTENSOR + BOOST
================================================================================

  Contract: Row-major matrix column iteration (strided access)

[2026-02-16 04:11:32] Start CPU: 2445 MHz (~base: 2445)
  Matrix: 1000 x 1000 (1000000 elements)
  Task: Sum first column (stride = 1000)

  Results (ns/row):
    Manual r*cols indexing  :     0.67 ns/op  (+/-  0.10)
    TensorStridePolicy      :     3.84 ns/op  (+/-  0.04)
    Stride1DPolicy          :     0.67 ns/op  (+/-  0.07)
    Eigen::col(0).sum()     :     0.67 ns/op  (+/-  0.10)
    xtensor strided_view    :     6.86 ns/op  (+/-  0.16)
    Boost.MultiArray [][0]  :     0.73 ns/op  (+/-  0.04)

  vs Manual:
    TensorStridePolicy: 5.71x
    Stride1DPolicy:     1.00x
    Eigen:              1.00x
    xtensor:            10.20x
    Boost:              1.09x

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 3.84 ns
    xtensor:            6.86 ns (0.56x)

================================================================================
  TENSOR: CONTIGUOUS VS NON-CONTIGUOUS VS SPECIALIZED + XTENSOR + BOOST
================================================================================

  Contract: Compare general TensorStridePolicy vs lightweight 1D/2D policies

[2026-02-16 04:11:33] Start CPU: 2445 MHz (~base: 2445)
  Matrix: 1000 x 1000 (1000000 elements)

  Full matrix iteration (ns/element):
    Manual flat loop        :     0.15 ns/op  (+/-  0.01)
    TensorStridePolicy (general):     1.34 ns/op  (+/-  0.02)
    Stride2DPolicy (specialized):     0.15 ns/op  (+/-  0.01)
    xtensor iterator        :     0.15 ns/op  (+/-  0.01)
    Boost.MultiArray data() :     0.15 ns/op  (+/-  0.00)

  Column iteration (ns/row, stride=1000):
    TensorStridePolicy (general):     2.88 ns/op  (+/-  0.06)
    Stride1DPolicy (specialized):     0.67 ns/op  (+/-  0.01)
    xtensor strided_view    :     2.86 ns/op  (+/-  0.06)

  vs Manual (full matrix):
    TensorStridePolicy: 8.77x
    Stride2DPolicy:     0.99x
    xtensor:            0.96x
    Boost:              0.96x

  Stride1D vs TensorStride (column iteration):
    TensorStridePolicy: 2.88 ns/row
    Stride1DPolicy:     0.67 ns/row (0.23x)

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 2.88 ns
    xtensor:            2.86 ns (1.01x)

================================================================================
  2D MATRIX FULL ITERATION: POLICIES VS EIGEN
================================================================================

  Contract: Sum all elements of 1000x1000 row-major matrix

[2026-02-16 04:11:34] Start CPU: 2703 MHz (~base: 2703)
  Matrix: 1000 x 1000

  Results (ns/element):
    Manual flat loop        :     0.15 ns/op  (+/-  0.00)
    Stride2DPolicy          :     0.15 ns/op  (+/-  0.01)
    TensorStridePolicy 2D   :     1.35 ns/op  (+/-  0.01)
    Eigen::sum()            :     0.15 ns/op  (+/-  0.01)
    Eigen coeff (r,c) loop  :     0.15 ns/op  (+/-  0.01)

  vs Manual:
    Stride2DPolicy:     0.98x
    TensorStridePolicy: 8.78x
    Eigen::sum():       0.96x
    Eigen coeff loop:   0.97x

================================================================================
  TENSOR ITERATION COMPOSITION HELPERS
================================================================================

  Contract: Compare iterateND composition vs manual loops vs TensorStridePolicy

[2026-02-16 04:11:35] Start CPU: 2445 MHz (~base: 2445)
  3D Tensor: 50 x 100 x 200 (1000000 elements)
  4D Tensor: 8 x 16 x 50 x 100 (640000 elements)

  3D Results (ns/element):
    Manual nested loop      :     0.17 ns/op  (+/-  0.01)
    TensorStridePolicy      :     1.29 ns/op  (+/-  0.02)
    iterateND (composition) :     1.20 ns/op  (+/-  0.01)

  3D vs Manual:
    TensorStridePolicy:     7.78x
    iterateND (composition): 7.26x

  4D Results (ns/element):
    Manual nested loop      :     0.17 ns/op  (+/-  0.02)
    TensorStridePolicy      :     1.33 ns/op  (+/-  0.04)
    iterateND (composition) :     1.21 ns/op  (+/-  0.01)

  4D vs Manual:
    TensorStridePolicy:     7.97x
    iterateND (composition): 7.23x

  Composition vs TensorStridePolicy:
    3D: iterateND is 1.07x faster
    4D: iterateND is 1.10x faster

================================================================================
  SIZE SCALING (CACHE EFFECTS)
================================================================================

  Contract: Standard iteration sum at different data sizes

[2026-02-16 04:11:37] Start CPU: 2445 MHz (~base: 2445)

  --- L1 (4K) (512 elements) ---

    Raw pointer             :     0.20 ns/op  (+/-  0.04)
    PolicyIterator          :     0.19 ns/op  (+/-  0.01)
    Overhead ratio: 0.95x

  --- L2 (32K) (4096 elements) ---

    Raw pointer             :     0.16 ns/op  (+/-  0.01)
    PolicyIterator          :     0.16 ns/op  (+/-  0.01)
    Overhead ratio: 0.98x

  --- L3 (512K) (65536 elements) ---

    Raw pointer             :     0.14 ns/op  (+/-  0.02)
    PolicyIterator          :     0.14 ns/op  (+/-  0.01)
    Overhead ratio: 1.01x

  --- RAM (8M) (1048576 elements) ---

    Raw pointer             :     0.16 ns/op  (+/-  0.02)
    PolicyIterator          :     0.16 ns/op  (+/-  0.02)
    Overhead ratio: 1.01x
================================================================================
  BENCHMARK COMPLETE
================================================================================

[2026-02-16 04:11:37] Final state CPU: 3250 MHz (~base: 3250)
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
[2026-02-16 04:54:55] Benchmark start CPU: 2445 MHz (base: 2445)

================================================================================
  STANDARD ITERATION VS RAW POINTER
================================================================================

  Contract: Sequential sum accumulation, no predicate/transform

[2026-02-16 04:54:55] Start CPU: 2445 MHz (base: 2445)
  Results (ns/element):
    Raw pointer             :     0.37 ns/op  (+/-  0.17)
    PolicyIterator<Standard>:     0.69 ns/op  (+/-  0.37)

  Overhead ratio: 1.86x

================================================================================
  STRIDE POLICIES VS MANUAL LOOPS
================================================================================

  Contract: Sum with fixed compile-time stride, bounds clamping included

[2026-02-16 04:54:57] Start CPU: 2445 MHz (base: 2445)

  --- Stride 2 ---

    Manual loop             :     0.65 ns/op  (+/-  0.08)
    PolicyIterator<Stride>  :     1.28 ns/op  (+/-  0.22)
    Overhead ratio: 1.98x

  --- Stride 4 ---

    Manual loop             :     0.70 ns/op  (+/-  0.06)
    PolicyIterator<Stride>  :     0.73 ns/op  (+/-  0.19)
    Overhead ratio: 1.04x

  --- Stride 8 ---

    Manual loop             :     1.09 ns/op  (+/-  0.07)
    PolicyIterator<Stride>  :     1.09 ns/op  (+/-  0.10)
    Overhead ratio: 1.01x

================================================================================
  FILTER POLICY VS MANUAL LOOP + BOOST
================================================================================

  Contract: Sum of even values only, predicate evaluation cost included

[2026-02-16 04:54:59] Start CPU: 2445 MHz (base: 2445)
  Match ratio: 50.0%

  Results (ns/element scanned):
    Manual if-check loop    :     3.50 ns/op  (+/-  0.09)
    PolicyIterator<Filter>  :     4.08 ns/op  (+/-  0.06)
    Boost.filter_iterator   :     3.89 ns/op  (+/-  0.03)

  vs Manual:
    PolicyIterator: 1.16x
    Boost:          1.11x

================================================================================
  TRANSFORM POLICY VS MANUAL LOOP + BOOST
================================================================================

  Contract: Sum of doubled values, transform cost included

[2026-02-16 04:55:02] Start CPU: 2445 MHz (base: 2445)
  Results (ns/element):
    Manual transform loop   :     0.31 ns/op  (+/-  0.01)
    PolicyIterator<Transform>:     0.43 ns/op  (+/-  0.05)
    Boost.transform_iterator:     0.43 ns/op  (+/-  0.03)

  vs Manual:
    PolicyIterator: 1.37x
    Boost:          1.36x

================================================================================
  TENSOR STRIDE POLICY VS MANUAL + BOOST
================================================================================

  Contract: Row-major matrix column iteration (strided access)

[2026-02-16 04:55:04] Start CPU: 2445 MHz (base: 2445)
  Matrix: 1000 x 1000 (1000000 elements)
  Task: Sum first column (stride = 1000)

  Results (ns/row):
    Manual r*cols indexing  :     0.70 ns/op  (+/-  0.05)
    TensorStridePolicy      :     2.95 ns/op  (+/-  0.05)
    Stride1DPolicy          :     0.65 ns/op  (+/-  0.05)
    Boost.MultiArray [][0]  :     0.70 ns/op  (+/-  0.05)

  vs Manual:
    TensorStridePolicy: 4.21x
    Stride1DPolicy:     0.93x
    Boost:              1.00x

================================================================================
  TENSOR: CONTIGUOUS VS NON-CONTIGUOUS VS SPECIALIZED + BOOST
================================================================================

  Contract: Compare general TensorStridePolicy vs lightweight 1D/2D policies

[2026-02-16 04:55:06] Start CPU: 2445 MHz (base: 2445)
  Matrix: 1000 x 1000 (1000000 elements)

  Full matrix iteration (ns/element):
    Manual flat loop        :     0.31 ns/op  (+/-  0.01)
    TensorStridePolicy (general):     1.89 ns/op  (+/-  0.01)
    Stride2DPolicy (specialized):     0.33 ns/op  (+/-  0.02)
    Boost.MultiArray data() :     0.63 ns/op  (+/-  0.02)

  Column iteration (ns/row, stride=1000):
    TensorStridePolicy (general):     4.00 ns/op  (+/-  0.50)
    Stride1DPolicy (specialized):     0.70 ns/op  (+/-  0.09)

  vs Manual (full matrix):
    TensorStridePolicy: 6.08x
    Stride2DPolicy:     1.06x
    Boost:              2.02x

  Stride1D vs TensorStride (column iteration):
    TensorStridePolicy: 4.00 ns/row
    Stride1DPolicy:     0.70 ns/row (0.17x)

================================================================================
  TENSOR ITERATION COMPOSITION HELPERS
================================================================================

  Contract: Compare iterateND composition vs manual loops vs TensorStridePolicy

[2026-02-16 04:55:08] Start CPU: 2445 MHz (base: 2445)
  3D Tensor: 50 x 100 x 200 (1000000 elements)
  4D Tensor: 8 x 16 x 50 x 100 (640000 elements)

  3D Results (ns/element):
    Manual nested loop      :     0.31 ns/op  (+/-  0.01)
    TensorStridePolicy      :     1.84 ns/op  (+/-  0.01)
    iterateND (composition) :     0.96 ns/op  (+/-  0.01)

  3D vs Manual:
    TensorStridePolicy:     5.91x
    iterateND (composition): 3.08x

  4D Results (ns/element):
    Manual nested loop      :     0.31 ns/op  (+/-  0.01)
    TensorStridePolicy      :     1.89 ns/op  (+/-  0.02)
    iterateND (composition) :     0.98 ns/op  (+/-  0.02)

  4D vs Manual:
    TensorStridePolicy:     6.03x
    iterateND (composition): 3.13x

  Composition vs TensorStridePolicy:
    3D: iterateND is 1.92x faster
    4D: iterateND is 1.92x faster

================================================================================
  SIZE SCALING (CACHE EFFECTS)
================================================================================

  Contract: Standard iteration sum at different data sizes

[2026-02-16 04:55:10] Start CPU: 2445 MHz (base: 2445)

  --- L1 (4K) (512 elements) ---

    Raw pointer             :     0.39 ns/op  (+/-  0.10)
    PolicyIterator          :     0.59 ns/op  (+/-  0.10)
    Overhead ratio: 1.50x

  --- L2 (32K) (4096 elements) ---

    Raw pointer             :     0.63 ns/op  (+/-  0.06)
    PolicyIterator          :     0.88 ns/op  (+/-  0.10)
    Overhead ratio: 1.38x

  --- L3 (512K) (65536 elements) ---

    Raw pointer             :     0.46 ns/op  (+/-  0.05)
    PolicyIterator          :     0.62 ns/op  (+/-  0.12)
    Overhead ratio: 1.35x

  --- RAM (8M) (1048576 elements) ---

    Raw pointer             :     0.33 ns/op  (+/-  0.01)
    PolicyIterator          :     0.64 ns/op  (+/-  0.02)
    Overhead ratio: 1.94x
================================================================================
  BENCHMARK COMPLETE
================================================================================

[2026-02-16 04:55:10] Final state CPU: 2445 MHz (base: 2445)
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- Eigen (Eigen/Dense) was not detected on MSVC CI.
- range-v3 (range/v3/view/filter.hpp) was not detected on MSVC CI.
- xtensor (requires C++20) was not detected on MSVC CI.
