---
doc_id: BR-SmallVector-001
doc_type: "Benchmark Results"
title: "SmallVector"
fatp_components: ["SmallVector"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - SmallVector

**Source:** `benchmark_SmallVector.cpp`
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
| fat_p::SmallVector | x | x | x | x |
| std::vector | x | x | x | x |
| boost::container::small_vector | x | x | x | x |
| llvm::SmallVector | x | x | x | — |
| absl::InlinedVector | x | x | x | x |
| folly::small_vector | — | x | x | x |
| ankerl::svector | x | x | x | x |
| eastl::fixed_vector | x | x | x | x |
| llvm::SmallVector (apt install llvm-dev) | — | — | — | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  CORE OPERATIONS
================================================================================

Comparing fundamental vector operations.
All containers pre-reserved to target size.

--- N = 100 ---

[2026-02-15 19:59:12] Starting CPU: 2653 MHz (base: 3686)
push_back:
       SmallVector<16>: median=    1.00 mean=    0.93 +/-  0.26 min=0.00 max=1.00
           std::vector: median=    1.00 mean=    1.07 +/-  0.26 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    0.73 +/-  0.59 min=0.00 max=2.00
  llvm::SmallVector<16>: median=    1.00 mean=    0.93 +/-  0.46 min=0.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.00 +/-  0.38 min=0.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    1.00 +/-  0.53 min=0.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.00 +/-  0.00 min=1.00 max=1.00

emplace_back:
       SmallVector<16>: median=    1.00 mean=    1.00 +/-  0.38 min=0.00 max=2.00
           std::vector: median=    1.00 mean=    0.87 +/-  0.52 min=0.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    0.93 +/-  0.70 min=0.00 max=2.00
  llvm::SmallVector<16>: median=    1.00 mean=    0.93 +/-  0.59 min=0.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.20 +/-  0.41 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    0.93 +/-  0.26 min=0.00 max=1.00
  eastl::fixed_vector<16>: median=    1.00 mean=    0.80 +/-  0.41 min=0.00 max=1.00

operator[]:
       SmallVector<16>: median=    1.00 mean=    1.40 +/-  0.51 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  llvm::SmallVector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.33 +/-  0.49 min=1.00 max=2.00
   ankerl::svector<16>: median=    2.00 mean=    1.53 +/-  0.52 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.40 +/-  0.51 min=1.00 max=2.00

iteration:
       SmallVector<16>: median=    1.00 mean=    1.47 +/-  0.52 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.33 +/-  0.49 min=1.00 max=2.00
  llvm::SmallVector<16>: median=    1.00 mean=    1.20 +/-  0.41 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.40 +/-  0.51 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00

copy ctor:
       SmallVector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.00 +/-  0.00 min=1.00 max=1.00
  llvm::SmallVector<16>: median=    1.00 mean=    1.07 +/-  0.26 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.07 +/-  0.26 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00

move ctor:
       SmallVector<16>: median=    0.00 mean=    0.07 +/-  0.26 min=0.00 max=1.00
           std::vector: median=    0.00 mean=    0.20 +/-  0.41 min=0.00 max=1.00
  boost::small_vector<16>: median=    0.00 mean=    0.47 +/-  0.52 min=0.00 max=1.00
  llvm::SmallVector<16>: median=    0.00 mean=    0.33 +/-  0.49 min=0.00 max=1.00
  absl::InlinedVector<16>: median=    0.00 mean=    0.13 +/-  0.35 min=0.00 max=1.00
   ankerl::svector<16>: median=    0.00 mean=    0.20 +/-  0.41 min=0.00 max=1.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.40 +/-  0.51 min=1.00 max=2.00

--- N = 1000 ---

[2026-02-15 19:59:12] Starting CPU: 2653 MHz (base: 3686)
push_back:
       SmallVector<16>: median=    0.60 mean=    0.59 +/-  0.06 min=0.50 max=0.70
           std::vector: median=    1.00 mean=    0.96 +/-  0.05 min=0.90 max=1.00
  boost::small_vector<16>: median=    0.60 mean=    0.59 +/-  0.05 min=0.50 max=0.70
  llvm::SmallVector<16>: median=    0.60 mean=    0.60 +/-  0.07 min=0.50 max=0.70
  absl::InlinedVector<16>: median=    0.60 mean=    0.63 +/-  0.05 min=0.60 max=0.70
   ankerl::svector<16>: median=    0.60 mean=    0.57 +/-  0.06 min=0.50 max=0.70
  eastl::fixed_vector<16>: median=    0.70 mean=    0.75 +/-  0.05 min=0.70 max=0.80

emplace_back:
       SmallVector<16>: median=    0.60 mean=    0.60 +/-  0.04 min=0.50 max=0.70
           std::vector: median=    0.60 mean=    0.57 +/-  0.06 min=0.50 max=0.70
  boost::small_vector<16>: median=    0.60 mean=    0.57 +/-  0.07 min=0.50 max=0.70
  llvm::SmallVector<16>: median=    0.50 mean=    0.47 +/-  0.07 min=0.40 max=0.60
  absl::InlinedVector<16>: median=    0.60 mean=    0.65 +/-  0.08 min=0.50 max=0.80
   ankerl::svector<16>: median=    0.60 mean=    0.61 +/-  0.08 min=0.50 max=0.80
  eastl::fixed_vector<16>: median=    0.40 mean=    0.49 +/-  0.20 min=0.40 max=1.20

operator[]:
       SmallVector<16>: median=    1.20 mean=    1.19 +/-  0.04 min=1.10 max=1.20
           std::vector: median=    1.20 mean=    1.20 +/-  0.04 min=1.10 max=1.30
  boost::small_vector<16>: median=    1.20 mean=    1.19 +/-  0.05 min=1.10 max=1.30
  llvm::SmallVector<16>: median=    1.20 mean=    1.22 +/-  0.04 min=1.20 max=1.30
  absl::InlinedVector<16>: median=    1.20 mean=    1.17 +/-  0.06 min=1.10 max=1.30
   ankerl::svector<16>: median=    1.20 mean=    1.21 +/-  0.11 min=1.10 max=1.50
  eastl::fixed_vector<16>: median=    1.20 mean=    1.19 +/-  0.04 min=1.10 max=1.20

iteration:
       SmallVector<16>: median=    1.20 mean=    1.23 +/-  0.05 min=1.20 max=1.30
           std::vector: median=    1.20 mean=    1.21 +/-  0.03 min=1.20 max=1.30
  boost::small_vector<16>: median=    1.20 mean=    1.25 +/-  0.05 min=1.20 max=1.30
  llvm::SmallVector<16>: median=    1.20 mean=    1.23 +/-  0.05 min=1.20 max=1.30
  absl::InlinedVector<16>: median=    1.20 mean=    1.23 +/-  0.05 min=1.20 max=1.30
   ankerl::svector<16>: median=    1.20 mean=    1.25 +/-  0.05 min=1.20 max=1.30
  eastl::fixed_vector<16>: median=    1.20 mean=    1.21 +/-  0.04 min=1.20 max=1.30

copy ctor:
       SmallVector<16>: median=    0.30 mean=    0.28 +/-  0.04 min=0.20 max=0.30
           std::vector: median=    0.10 mean=    0.12 +/-  0.04 min=0.10 max=0.20
  boost::small_vector<16>: median=    0.20 mean=    0.15 +/-  0.05 min=0.10 max=0.20
  llvm::SmallVector<16>: median=    0.10 mean=    0.43 +/-  1.13 min=0.10 max=4.50
  absl::InlinedVector<16>: median=    0.10 mean=    0.13 +/-  0.05 min=0.10 max=0.20
   ankerl::svector<16>: median=    0.10 mean=    0.12 +/-  0.04 min=0.10 max=0.20
  eastl::fixed_vector<16>: median=    0.10 mean=    0.15 +/-  0.05 min=0.10 max=0.20

move ctor:
       SmallVector<16>: median=    0.00 mean=    0.03 +/-  0.05 min=0.00 max=0.10
           std::vector: median=    0.00 mean=    0.01 +/-  0.04 min=0.00 max=0.10
  boost::small_vector<16>: median=    0.00 mean=    0.02 +/-  0.04 min=0.00 max=0.10
  llvm::SmallVector<16>: median=    0.00 mean=    0.03 +/-  0.05 min=0.00 max=0.10
  absl::InlinedVector<16>: median=    0.00 mean=    0.03 +/-  0.05 min=0.00 max=0.10
   ankerl::svector<16>: median=    0.00 mean=    0.03 +/-  0.05 min=0.00 max=0.10
  eastl::fixed_vector<16>: median=    0.40 mean=    0.42 +/-  0.04 min=0.40 max=0.50

--- N = 10000 ---

[2026-02-15 19:59:12] Starting CPU: 2653 MHz (base: 3686)
push_back:
       SmallVector<16>: median=    0.74 mean=    0.74 +/-  0.01 min=0.72 max=0.78
           std::vector: median=    1.15 mean=    1.15 +/-  0.01 min=1.14 max=1.16
  boost::small_vector<16>: median=    0.73 mean=    0.73 +/-  0.02 min=0.72 max=0.80
  llvm::SmallVector<16>: median=    0.59 mean=    0.63 +/-  0.19 min=0.56 max=1.33
  absl::InlinedVector<16>: median=    0.75 mean=    0.75 +/-  0.01 min=0.74 max=0.79
   ankerl::svector<16>: median=    0.56 mean=    0.57 +/-  0.02 min=0.55 max=0.63
  eastl::fixed_vector<16>: median=    0.89 mean=    0.89 +/-  0.00 min=0.89 max=0.90

emplace_back:
       SmallVector<16>: median=    0.75 mean=    0.84 +/-  0.20 min=0.73 max=1.47
           std::vector: median=    0.74 mean=    0.74 +/-  0.01 min=0.73 max=0.75
  boost::small_vector<16>: median=    0.73 mean=    0.74 +/-  0.02 min=0.73 max=0.80
  llvm::SmallVector<16>: median=    0.52 mean=    0.58 +/-  0.21 min=0.50 max=1.34
  absl::InlinedVector<16>: median=    0.74 mean=    0.74 +/-  0.01 min=0.74 max=0.75
   ankerl::svector<16>: median=    0.56 mean=    0.56 +/-  0.01 min=0.55 max=0.58
  eastl::fixed_vector<16>: median=    0.51 mean=    2.57 +/-  7.97 min=0.50 max=31.39

operator[]:
       SmallVector<16>: median=    1.21 mean=    1.26 +/-  0.14 min=1.20 max=1.75
           std::vector: median=    1.21 mean=    1.25 +/-  0.06 min=1.20 max=1.34
  boost::small_vector<16>: median=    1.21 mean=    1.23 +/-  0.05 min=1.19 max=1.32
  llvm::SmallVector<16>: median=    1.21 mean=    1.24 +/-  0.06 min=1.19 max=1.34
  absl::InlinedVector<16>: median=    1.24 mean=    1.28 +/-  0.11 min=1.16 max=1.60
   ankerl::svector<16>: median=    1.34 mean=    1.36 +/-  0.13 min=1.18 max=1.57
  eastl::fixed_vector<16>: median=    1.21 mean=    1.27 +/-  0.14 min=1.19 max=1.75

iteration:
       SmallVector<16>: median=    1.23 mean=    1.24 +/-  0.03 min=1.22 max=1.34
           std::vector: median=    1.20 mean=    1.21 +/-  0.03 min=1.19 max=1.33
  boost::small_vector<16>: median=    1.23 mean=    1.25 +/-  0.05 min=1.21 max=1.39
  llvm::SmallVector<16>: median=    1.23 mean=    1.24 +/-  0.05 min=1.21 max=1.36
  absl::InlinedVector<16>: median=    1.23 mean=    1.27 +/-  0.14 min=1.21 max=1.74
   ankerl::svector<16>: median=    1.22 mean=    1.23 +/-  0.03 min=1.21 max=1.34
  eastl::fixed_vector<16>: median=    1.20 mean=    1.25 +/-  0.14 min=1.19 max=1.76

copy ctor:
       SmallVector<16>: median=    2.13 mean=    2.27 +/-  0.41 min=2.07 max=3.42
           std::vector: median=    3.07 mean=    3.19 +/-  0.65 min=2.82 max=5.42
  boost::small_vector<16>: median=    2.80 mean=    3.31 +/-  0.89 min=2.75 max=5.75
  llvm::SmallVector<16>: median=    2.88 mean=    3.21 +/-  0.61 min=2.83 max=4.53
  absl::InlinedVector<16>: median=    2.78 mean=    2.90 +/-  0.34 min=2.71 max=4.07
   ankerl::svector<16>: median=    2.15 mean=    2.21 +/-  0.17 min=2.09 max=2.73
  eastl::fixed_vector<16>: median=    2.95 mean=    3.38 +/-  0.78 min=2.88 max=5.55

move ctor:
       SmallVector<16>: median=    0.01 mean=    0.01 +/-  0.01 min=0.00 max=0.01
           std::vector: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
  boost::small_vector<16>: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
  llvm::SmallVector<16>: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
  absl::InlinedVector<16>: median=    0.01 mean=    0.01 +/-  0.01 min=0.00 max=0.02
   ankerl::svector<16>: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
  eastl::fixed_vector<16>: median=    2.32 mean=    2.67 +/-  0.68 min=2.04 max=3.93

================================================================================
  INLINE VS HEAP PERFORMANCE
================================================================================

The key SmallVector advantage: zero allocations for small sizes.
Testing operations at various sizes relative to inline capacity.

[2026-02-15 19:59:12] Starting CPU: 2838 MHz (base: 3686)
Size | SmallVector push_back (ns) | std::vector push_back (ns) | Ratio
-----|----------------------------|----------------------------|------
   4 (inline) |                       3.98 |                      30.76 |  7.72x
   8 (inline) |                       1.95 |                      16.59 |  8.51x
  12 (inline) |                       1.54 |                      12.86 |  8.34x
  16 (inline) |                       1.16 |                      11.28 |  9.69x
  20 (heap) |                       2.55 |                      10.17 |  4.00x
  32 (heap) |                       1.94 |                       7.22 |  3.72x
  64 (heap) |                       2.00 |                       5.64 |  2.82x
 128 (heap) |                       2.02 |                       3.52 |  1.74x

================================================================================
  ALLOCATION COUNT COMPARISON
================================================================================

Counting heap allocations for various scenarios.
SmallVector should have zero allocations when size <= InlineCapacity.

[2026-02-15 19:59:12] Starting CPU: 2838 MHz (base: 3686)
Scenario: push_back N elements (no reserve)

    N | SmallVector allocs | std::vector allocs
------|--------------------|-----------------
    1 |                  0 |                 1
    8 |                  0 |                 6
   16 |                  0 |                 8
   17 |                  1 |                 8
   32 |                  1 |                10
  100 |                  3 |                13
 1000 |                  6 |                18

Note: SmallVector<16> uses inline storage for N <= 16

================================================================================
  INSERT/ERASE OPERATIONS
================================================================================

Comparing insert and erase at various positions.

[2026-02-15 19:59:12] Starting CPU: 2838 MHz (base: 3686)
Insert single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |               60.13 |               55.67 |  0.93x
  middle |               40.93 |               31.87 |  0.78x
    back |               10.40 |                8.47 |  0.81x

Erase single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |               37.93 |               35.40 |  0.93x
  middle |               20.53 |               20.07 |  0.98x
    back |                6.40 |                6.60 |  1.03x

================================================================================
  INLINE CAPACITY SENSITIVITY
================================================================================

How does inline capacity choice affect performance?
Testing push_back of N elements with various inline capacities.

[2026-02-15 19:59:12] Starting CPU: 2838 MHz (base: 3686)
Target size: 32 elements

InlineCapacity | Time (ns/op) | Allocations | Notes
---------------|--------------|-------------|------
  0 (std::vec) |         7.10 |          10 | always heap
             8 |         2.44 |           2 | transitions
            16 |         1.36 |           1 | transitions
            32 |         1.03 |           0 | all inline
            64 |         0.97 |           0 | all inline

================================================================================
  FAST PATH THROUGHPUT (ISOLATION BENCHMARK)
================================================================================

Measuring pure fast-path performance with zero allocations.
This isolates the emplace_back fast-path optimization from allocation benefits.

[2026-02-15 19:59:12] Starting CPU: 2764 MHz (base: 3686)

--- emplace_back Throughput (100% Fast Path) ---

InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio
----------|---------------------|----------------------|------
* std::vector is pre-reserved to avoid allocation

        4 |                3.42 |                 3.35 |  0.98x
        8 |                1.84 |                 2.35 |  1.28x
       16 |                1.08 |                 1.11 |  1.03x
       32 |                0.85 |                 0.83 |  0.98x

--- push_back vs emplace_back (InlineCap=16) ---

Operation   | SmallVector (ns/op) | std::vector* (ns/op)
------------|---------------------|---------------------
push_back   |                1.06 |                1.07
emplace_back|                1.18 |                1.05

--- Pairwise Fast-Path Comparisons (InlineCap=16, N=16) ---
Each pair tested in isolated function call (noinline).

fat_p= 1.14  std::vec= 1.10
fat_p= 1.12  boost= 1.38
fat_p= 1.10  llvm= 1.07
fat_p= 1.20  absl= 1.88
fat_p= 1.39  ankerl= 4.20
fat_p= 1.09  eastl= 1.02

--- Fill/Clear Cycle (1000 cycles, InlineCap=16) ---
Tests repeated use of same SmallVector (cache-warm scenario)

SmallVector (reused): 0.56 ns/op
std::vector (reused): 0.49 ns/op

Note: This benchmark measures the optimized fast-path code.
The main SmallVector advantage (7-8x for small N) comes from
avoiding heap allocation entirely, which is tested in
benchmark_inline_vs_heap().

================================================================================
  OBJECT SIZE IMPACT
================================================================================

How does sizeof(SmallVector) compare to std::vector?
Larger inline capacity = larger object size.

Container                        | sizeof (bytes)
---------------------------------|---------------
            std::vector<int64_t> |             24
         SmallVector<int64_t, 4> |             64
         SmallVector<int64_t, 8> |             96
        SmallVector<int64_t, 16> |            160
        SmallVector<int64_t, 32> |            288
        SmallVector<int64_t, 64> |            544

Boost comparison:
boost::small_vector<int64_t, 16> |            152

LLVM comparison:
  llvm::SmallVector<int64_t, 16> |            144

Abseil comparison:
absl::InlinedVector<int64_t, 16> |            136

ankerl comparison:
    ankerl::svector<int64_t, 16> |            136
     ankerl::svector<int64_t, 1> |             16

EASTL comparison:
 eastl::fixed_vector<int64_t,16> |            168

Note: SmallVector<T, N> size Γëê 3*sizeof(void*) + N*sizeof(T)

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  CORE OPERATIONS
================================================================================

Comparing fundamental vector operations.
All containers pre-reserved to target size.
--- N = 100 ---

[2026-02-16 03:37:58] Starting CPU: 3245 MHz (~base: 3245)
push_back:
       SmallVector<16>: median=    1.30 mean=    1.31 +/-  0.08 min=1.20 max=1.50
           std::vector: median=    0.90 mean=    0.93 +/-  0.07 min=0.89 max=1.21
  boost::small_vector<16>: median=    1.40 mean=    1.38 +/-  0.11 min=1.11 max=1.60
  llvm::SmallVector<16>: median=    1.60 mean=    1.63 +/-  0.12 min=1.40 max=2.00
  folly::small_vector<16>: median=    1.50 mean=    1.51 +/-  0.11 min=1.30 max=1.71
  absl::InlinedVector<16>: median=    1.40 mean=    1.39 +/-  0.09 min=1.20 max=1.61
   ankerl::svector<16>: median=    1.46 mean=    1.47 +/-  0.12 min=1.20 max=1.80
  eastl::fixed_vector<16>: median=    1.00 mean=    0.97 +/-  0.09 min=0.80 max=1.10

emplace_back:
       SmallVector<16>: median=    1.35 mean=    1.47 +/-  0.26 min=1.11 max=2.01
           std::vector: median=    0.90 mean=    1.07 +/-  0.25 min=0.80 max=1.51
  boost::small_vector<16>: median=    1.45 mean=    1.57 +/-  0.26 min=1.20 max=2.01
  llvm::SmallVector<16>: median=    0.90 mean=    1.17 +/-  0.55 min=0.80 max=3.21
  folly::small_vector<16>: median=    1.50 mean=    1.92 +/-  0.73 min=1.30 max=3.81
  absl::InlinedVector<16>: median=    1.35 mean=    1.65 +/-  0.44 min=1.20 max=2.30
   ankerl::svector<16>: median=    1.50 mean=    1.74 +/-  0.45 min=1.20 max=2.41
  eastl::fixed_vector<16>: median=    1.00 mean=    1.16 +/-  0.27 min=0.90 max=1.70

operator[]:
       SmallVector<16>: median=    1.16 mean=    1.16 +/-  0.09 min=0.91 max=1.30
           std::vector: median=    1.10 mean=    1.10 +/-  0.16 min=0.70 max=1.40
  boost::small_vector<16>: median=    1.20 mean=    1.18 +/-  0.08 min=1.10 max=1.40
  llvm::SmallVector<16>: median=    1.10 mean=    1.15 +/-  0.11 min=1.00 max=1.41
  folly::small_vector<16>: median=    1.06 mean=    1.07 +/-  0.12 min=0.80 max=1.30
  absl::InlinedVector<16>: median=    1.10 mean=    1.14 +/-  0.12 min=0.80 max=1.41
   ankerl::svector<16>: median=    1.20 mean=    1.19 +/-  0.06 min=1.10 max=1.30
  eastl::fixed_vector<16>: median=    1.10 mean=    1.10 +/-  0.14 min=0.70 max=1.40

iteration:
       SmallVector<16>: median=    1.10 mean=    1.13 +/-  0.15 min=0.80 max=1.50
           std::vector: median=    1.10 mean=    1.04 +/-  0.17 min=0.60 max=1.21
  boost::small_vector<16>: median=    1.20 mean=    1.22 +/-  0.05 min=1.10 max=1.31
  llvm::SmallVector<16>: median=    1.20 mean=    1.18 +/-  0.11 min=0.81 max=1.40
  folly::small_vector<16>: median=    1.20 mean=    1.13 +/-  0.20 min=0.70 max=1.41
  absl::InlinedVector<16>: median=    1.10 mean=    1.08 +/-  0.14 min=0.70 max=1.30
   ankerl::svector<16>: median=    1.20 mean=    1.15 +/-  0.09 min=0.90 max=1.30
  eastl::fixed_vector<16>: median=    1.10 mean=    1.12 +/-  0.15 min=0.60 max=1.41

copy ctor:
       SmallVector<16>: median=    0.60 mean=    0.63 +/-  0.04 min=0.60 max=0.71
           std::vector: median=    0.50 mean=    0.52 +/-  0.06 min=0.40 max=0.61
  boost::small_vector<16>: median=    0.55 mean=    0.56 +/-  0.06 min=0.50 max=0.70
  llvm::SmallVector<16>: median=    0.60 mean=    0.59 +/-  0.06 min=0.50 max=0.71
  folly::small_vector<16>: median=    0.60 mean=    0.57 +/-  0.08 min=0.40 max=0.80
  absl::InlinedVector<16>: median=    0.60 mean=    0.59 +/-  0.09 min=0.40 max=0.80
   ankerl::svector<16>: median=    0.90 mean=    0.87 +/-  0.10 min=0.60 max=1.01
  eastl::fixed_vector<16>: median=    0.50 mean=    0.47 +/-  0.07 min=0.30 max=0.60

move ctor:
       SmallVector<16>: median=    0.40 mean=    0.45 +/-  0.12 min=0.29 max=0.80
           std::vector: median=    0.40 mean=    0.44 +/-  0.09 min=0.30 max=0.60
  boost::small_vector<16>: median=    0.50 mean=    0.49 +/-  0.11 min=0.31 max=0.80
  llvm::SmallVector<16>: median=    0.60 mean=    0.58 +/-  0.10 min=0.40 max=0.80
  folly::small_vector<16>: median=    0.50 mean=    0.48 +/-  0.08 min=0.30 max=0.70
  absl::InlinedVector<16>: median=    0.40 mean=    0.46 +/-  0.15 min=0.29 max=0.80
   ankerl::svector<16>: median=    0.50 mean=    0.49 +/-  0.11 min=0.30 max=0.70
  eastl::fixed_vector<16>: median=    1.25 mean=    1.34 +/-  0.28 min=1.01 max=2.10
--- N = 1000 ---

[2026-02-16 03:37:58] Starting CPU: 3268 MHz (~base: 3268)
push_back:
       SmallVector<16>: median=    0.97 mean=    0.97 +/-  0.02 min=0.94 max=1.00
           std::vector: median=    0.65 mean=    0.65 +/-  0.01 min=0.64 max=0.67
  boost::small_vector<16>: median=    0.97 mean=    0.97 +/-  0.02 min=0.94 max=1.05
  llvm::SmallVector<16>: median=    1.19 mean=    1.19 +/-  0.02 min=1.16 max=1.21
  folly::small_vector<16>: median=    1.12 mean=    1.12 +/-  0.01 min=1.10 max=1.14
  absl::InlinedVector<16>: median=    0.97 mean=    0.98 +/-  0.02 min=0.95 max=1.03
   ankerl::svector<16>: median=    0.99 mean=    0.99 +/-  0.01 min=0.97 max=1.01
  eastl::fixed_vector<16>: median=    0.64 mean=    0.64 +/-  0.01 min=0.64 max=0.67

emplace_back:
       SmallVector<16>: median=    0.96 mean=    0.97 +/-  0.02 min=0.94 max=1.03
           std::vector: median=    0.64 mean=    0.65 +/-  0.00 min=0.64 max=0.65
  boost::small_vector<16>: median=    0.97 mean=    0.97 +/-  0.01 min=0.95 max=1.00
  llvm::SmallVector<16>: median=    0.66 mean=    0.66 +/-  0.01 min=0.64 max=0.68
  folly::small_vector<16>: median=    1.11 mean=    1.12 +/-  0.01 min=1.10 max=1.13
  absl::InlinedVector<16>: median=    0.97 mean=    0.97 +/-  0.01 min=0.95 max=0.98
   ankerl::svector<16>: median=    0.97 mean=    0.97 +/-  0.01 min=0.95 max=0.99
  eastl::fixed_vector<16>: median=    0.65 mean=    0.65 +/-  0.00 min=0.64 max=0.65

operator[]:
       SmallVector<16>: median=    0.59 mean=    0.59 +/-  0.01 min=0.57 max=0.61
           std::vector: median=    0.59 mean=    0.59 +/-  0.01 min=0.56 max=0.62
  boost::small_vector<16>: median=    0.58 mean=    0.58 +/-  0.01 min=0.57 max=0.60
  llvm::SmallVector<16>: median=    0.60 mean=    0.60 +/-  0.01 min=0.57 max=0.62
  folly::small_vector<16>: median=    0.57 mean=    0.60 +/-  0.13 min=0.55 max=1.15
  absl::InlinedVector<16>: median=    0.60 mean=    0.60 +/-  0.01 min=0.58 max=0.62
   ankerl::svector<16>: median=    0.58 mean=    0.58 +/-  0.01 min=0.57 max=0.59
  eastl::fixed_vector<16>: median=    0.60 mean=    0.60 +/-  0.01 min=0.56 max=0.62

iteration:
       SmallVector<16>: median=    0.46 mean=    0.46 +/-  0.01 min=0.44 max=0.49
           std::vector: median=    0.47 mean=    0.47 +/-  0.02 min=0.42 max=0.49
  boost::small_vector<16>: median=    0.48 mean=    0.48 +/-  0.01 min=0.46 max=0.50
  llvm::SmallVector<16>: median=    0.48 mean=    0.48 +/-  0.01 min=0.44 max=0.52
  folly::small_vector<16>: median=    0.48 mean=    0.47 +/-  0.02 min=0.43 max=0.49
  absl::InlinedVector<16>: median=    0.47 mean=    0.47 +/-  0.01 min=0.43 max=0.49
   ankerl::svector<16>: median=    0.49 mean=    0.48 +/-  0.02 min=0.44 max=0.50
  eastl::fixed_vector<16>: median=    0.48 mean=    0.48 +/-  0.02 min=0.42 max=0.51

copy ctor:
       SmallVector<16>: median=    0.17 mean=    0.17 +/-  0.02 min=0.16 max=0.23
           std::vector: median=    0.19 mean=    0.18 +/-  0.02 min=0.16 max=0.22
  boost::small_vector<16>: median=    0.19 mean=    0.19 +/-  0.02 min=0.16 max=0.24
  llvm::SmallVector<16>: median=    0.19 mean=    0.19 +/-  0.01 min=0.16 max=0.22
  folly::small_vector<16>: median=    0.19 mean=    0.19 +/-  0.01 min=0.16 max=0.20
  absl::InlinedVector<16>: median=    0.19 mean=    0.19 +/-  0.01 min=0.16 max=0.23
   ankerl::svector<16>: median=    0.17 mean=    0.39 +/-  0.90 min=0.16 max=4.19
  eastl::fixed_vector<16>: median=    0.06 mean=    0.06 +/-  0.01 min=0.05 max=0.07

move ctor:
       SmallVector<16>: median=    0.06 mean=    0.06 +/-  0.02 min=0.03 max=0.11
           std::vector: median=    0.05 mean=    0.05 +/-  0.01 min=0.03 max=0.07
  boost::small_vector<16>: median=    0.06 mean=    0.06 +/-  0.02 min=0.04 max=0.13
  llvm::SmallVector<16>: median=    0.07 mean=    0.08 +/-  0.02 min=0.06 max=0.12
  folly::small_vector<16>: median=    0.07 mean=    0.07 +/-  0.01 min=0.04 max=0.09
  absl::InlinedVector<16>: median=    0.06 mean=    0.06 +/-  0.01 min=0.03 max=0.09
   ankerl::svector<16>: median=    0.05 mean=    0.06 +/-  0.02 min=0.03 max=0.11
  eastl::fixed_vector<16>: median=    0.55 mean=    1.35 +/-  3.54 min=0.49 max=16.41
--- N = 10000 ---

[2026-02-16 03:37:58] Starting CPU: 3243 MHz (~base: 3243)
push_back:
       SmallVector<16>: median=    1.01 mean=    1.15 +/-  0.48 min=0.99 max=3.13
           std::vector: median=    0.65 mean=    0.80 +/-  0.58 min=0.64 max=3.23
  boost::small_vector<16>: median=    1.00 mean=    1.02 +/-  0.08 min=0.98 max=1.33
  llvm::SmallVector<16>: median=    1.20 mean=    1.24 +/-  0.10 min=1.20 max=1.59
  folly::small_vector<16>: median=    1.09 mean=    1.14 +/-  0.20 min=1.09 max=1.99
  absl::InlinedVector<16>: median=    1.00 mean=    1.07 +/-  0.22 min=0.98 max=1.73
   ankerl::svector<16>: median=    0.94 mean=    0.98 +/-  0.19 min=0.93 max=1.79
  eastl::fixed_vector<16>: median=    0.66 mean=    0.69 +/-  0.09 min=0.66 max=0.97

emplace_back:
       SmallVector<16>: median=    1.01 mean=    1.01 +/-  0.01 min=0.99 max=1.02
           std::vector: median=    0.66 mean=    0.66 +/-  0.00 min=0.65 max=0.66
  boost::small_vector<16>: median=    1.01 mean=    1.00 +/-  0.01 min=0.98 max=1.02
  llvm::SmallVector<16>: median=    0.75 mean=    0.75 +/-  0.01 min=0.73 max=0.76
  folly::small_vector<16>: median=    1.09 mean=    1.14 +/-  0.22 min=1.09 max=2.08
  absl::InlinedVector<16>: median=    0.98 mean=    0.99 +/-  0.02 min=0.98 max=1.07
   ankerl::svector<16>: median=    0.94 mean=    0.94 +/-  0.00 min=0.93 max=0.94
  eastl::fixed_vector<16>: median=    0.67 mean=    0.82 +/-  0.67 min=0.66 max=3.66

operator[]:
       SmallVector<16>: median=    0.59 mean=    0.59 +/-  0.01 min=0.58 max=0.63
           std::vector: median=    0.58 mean=    0.59 +/-  0.01 min=0.58 max=0.61
  boost::small_vector<16>: median=    0.57 mean=    0.64 +/-  0.32 min=0.56 max=2.00
  llvm::SmallVector<16>: median=    0.59 mean=    0.68 +/-  0.35 min=0.58 max=2.14
  folly::small_vector<16>: median=    0.57 mean=    0.57 +/-  0.01 min=0.56 max=0.60
  absl::InlinedVector<16>: median=    0.59 mean=    0.59 +/-  0.01 min=0.58 max=0.61
   ankerl::svector<16>: median=    0.57 mean=    0.57 +/-  0.01 min=0.56 max=0.59
  eastl::fixed_vector<16>: median=    0.58 mean=    0.59 +/-  0.02 min=0.58 max=0.66

iteration:
       SmallVector<16>: median=    0.42 mean=    0.70 +/-  1.13 min=0.42 max=5.51
           std::vector: median=    0.42 mean=    0.42 +/-  0.00 min=0.42 max=0.42
  boost::small_vector<16>: median=    0.42 mean=    0.57 +/-  0.62 min=0.42 max=3.19
  llvm::SmallVector<16>: median=    0.42 mean=    0.42 +/-  0.00 min=0.42 max=0.43
  folly::small_vector<16>: median=    0.42 mean=    0.42 +/-  0.00 min=0.42 max=0.43
  absl::InlinedVector<16>: median=    0.42 mean=    0.42 +/-  0.00 min=0.42 max=0.43
   ankerl::svector<16>: median=    0.42 mean=    0.44 +/-  0.08 min=0.42 max=0.76
  eastl::fixed_vector<16>: median=    0.42 mean=    0.59 +/-  0.73 min=0.42 max=3.70

copy ctor:
       SmallVector<16>: median=    1.45 mean=    1.77 +/-  0.82 min=1.40 max=4.81
           std::vector: median=    1.55 mean=    1.78 +/-  0.33 min=1.53 max=2.28
  boost::small_vector<16>: median=    1.61 mean=    1.84 +/-  0.34 min=1.52 max=2.38
  llvm::SmallVector<16>: median=    1.58 mean=    2.07 +/-  0.81 min=1.51 max=4.63
  folly::small_vector<16>: median=    1.56 mean=    1.82 +/-  0.46 min=1.52 max=3.13
  absl::InlinedVector<16>: median=    1.57 mean=    1.76 +/-  0.33 min=1.52 max=2.37
   ankerl::svector<16>: median=    4.40 mean=    4.46 +/-  0.71 min=3.86 max=6.98
  eastl::fixed_vector<16>: median=    0.34 mean=    0.39 +/-  0.14 min=0.33 max=0.96

move ctor:
       SmallVector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.01 max=0.01
           std::vector: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.01
  boost::small_vector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.01 max=0.01
  llvm::SmallVector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.01 max=0.01
  folly::small_vector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.01 max=0.01
  absl::InlinedVector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.01
   ankerl::svector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.02
  eastl::fixed_vector<16>: median=    2.55 mean=    2.80 +/-  0.48 min=2.43 max=3.68
================================================================================
  INLINE VS HEAP PERFORMANCE
================================================================================

The key SmallVector advantage: zero allocations for small sizes.
Testing operations at various sizes relative to inline capacity.

[2026-02-16 03:37:58] Starting CPU: 3242 MHz (~base: 3242)
Size | SmallVector push_back (ns) | std::vector push_back (ns) | Ratio
-----|----------------------------|----------------------------|------
   4 (inline) |                       7.38 |                      14.86 |  2.01x
   8 (inline) |                       3.86 |                       9.43 |  2.44x
  12 (inline) |                       2.84 |                       7.89 |  2.78x
  16 (inline) |                       2.27 |                       5.93 |  2.62x
  20 (heap) |                       3.86 |                       6.03 |  1.56x
  32 (heap) |                       2.95 |                       3.70 |  1.25x
  64 (heap) |                       2.28 |                       2.28 |  1.00x
 128 (heap) |                       2.02 |                       1.55 |  0.77x

================================================================================
  ALLOCATION COUNT COMPARISON
================================================================================

Counting heap allocations for various scenarios.
SmallVector should have zero allocations when size <= InlineCapacity.

[2026-02-16 03:37:58] Starting CPU: 3256 MHz (~base: 3256)
Scenario: push_back N elements (no reserve)

    N | SmallVector allocs | std::vector allocs
------|--------------------|-----------------
    1 |                  0 |                 1
    8 |                  0 |                 4
   16 |                  0 |                 5
   17 |                  1 |                 6
   32 |                  1 |                 6
  100 |                  3 |                 8
 1000 |                  6 |                11

Note: SmallVector<16> uses inline storage for N <= 16

================================================================================
  INSERT/ERASE OPERATIONS
================================================================================

Comparing insert and erase at various positions.

[2026-02-16 03:37:58] Starting CPU: 3256 MHz (~base: 3256)
Insert single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |               93.54 |              119.03 |  1.27x
  middle |               54.69 |               86.68 |  1.58x
    back |               17.18 |               38.55 |  2.24x

Erase single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |              153.16 |              136.10 |  0.89x
  middle |               73.65 |               73.38 |  1.00x
    back |               12.89 |               12.90 |  1.00x

================================================================================
  INLINE CAPACITY SENSITIVITY
================================================================================

How does inline capacity choice affect performance?
Testing push_back of N elements with various inline capacities.

[2026-02-16 03:37:58] Starting CPU: 3256 MHz (~base: 3256)
Target size: 32 elements

InlineCapacity | Time (ns/op) | Allocations | Notes
---------------|--------------|-------------|------
  0 (std::vec) |         3.71 |           6 | always heap
             8 |         2.59 |           2 | transitions
            16 |         2.17 |           1 | transitions
            32 |         1.65 |           0 | all inline
            64 |         1.62 |           0 | all inline

================================================================================
  FAST PATH THROUGHPUT (ISOLATION BENCHMARK)
================================================================================

Measuring pure fast-path performance with zero allocations.
This isolates the emplace_back fast-path optimization from allocation benefits.

[2026-02-16 03:37:58] Starting CPU: 2791 MHz (~base: 2791)

--- emplace_back Throughput (100% Fast Path) ---

InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio
----------|---------------------|----------------------|------
* std::vector is pre-reserved to avoid allocation

        4 |                8.73 |                 8.90 |  1.02x
        8 |                5.03 |                 4.28 |  0.85x
       16 |                3.24 |                 1.98 |  0.61x
       32 |                1.65 |                 1.13 |  0.68x

--- push_back vs emplace_back (InlineCap=16) ---

Operation   | SmallVector (ns/op) | std::vector* (ns/op)
------------|---------------------|---------------------
push_back   |                3.15 |                1.97
emplace_back|                4.05 |                2.24

--- Pairwise Fast-Path Comparisons (InlineCap=16, N=16) ---
Each pair tested in isolated function call (noinline).

fat_p= 4.00  std::vec= 3.20
fat_p= 2.26  boost= 2.36
fat_p= 2.24  llvm= 2.11
fat_p= 2.24  folly= 2.04
fat_p= 2.26  absl= 2.01
fat_p= 2.24  ankerl= 4.46
fat_p= 3.39  eastl= 2.13

--- Fill/Clear Cycle (1000 cycles, InlineCap=16) ---
Tests repeated use of same SmallVector (cache-warm scenario)

SmallVector (reused): 0.94 ns/op
std::vector (reused): 0.45 ns/op

Note: This benchmark measures the optimized fast-path code.
The main SmallVector advantage (7-8x for small N) comes from
avoiding heap allocation entirely, which is tested in
benchmark_inline_vs_heap().

================================================================================
  OBJECT SIZE IMPACT
================================================================================

How does sizeof(SmallVector) compare to std::vector?
Larger inline capacity = larger object size.

Container                        | sizeof (bytes)
---------------------------------|---------------
            std::vector<int64_t> |             24
         SmallVector<int64_t, 4> |             56
         SmallVector<int64_t, 8> |             88
        SmallVector<int64_t, 16> |            152
        SmallVector<int64_t, 32> |            280
        SmallVector<int64_t, 64> |            536

Boost comparison:
boost::small_vector<int64_t, 16> |            152

Folly comparison:
folly::small_vector<int64_t, 16> |            136

LLVM comparison:
  llvm::SmallVector<int64_t, 16> |            144

Abseil comparison:
absl::InlinedVector<int64_t, 16> |            136

ankerl comparison:
    ankerl::svector<int64_t, 16> |            136
     ankerl::svector<int64_t, 1> |             16

EASTL comparison:
 eastl::fixed_vector<int64_t,16> |            168

Note: SmallVector<T, N> size ≈ 3*sizeof(void*) + N*sizeof(T)

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  CORE OPERATIONS
================================================================================

Comparing fundamental vector operations.
All containers pre-reserved to target size.
--- N = 100 ---

[2026-02-16 04:11:22] Starting CPU: 3256 MHz (~base: 3256)
push_back:
       SmallVector<16>: median=    1.60 mean=    1.65 +/-  0.11 min=1.50 max=1.91
           std::vector: median=    1.21 mean=    1.22 +/-  0.11 min=1.00 max=1.50
  boost::small_vector<16>: median=    1.71 mean=    1.75 +/-  0.09 min=1.60 max=1.90
  llvm::SmallVector<16>: median=    1.70 mean=    1.68 +/-  0.13 min=1.50 max=2.00
  folly::small_vector<16>: median=    1.61 mean=    1.65 +/-  0.08 min=1.50 max=1.80
  absl::InlinedVector<16>: median=    1.71 mean=    1.71 +/-  0.12 min=1.50 max=1.91
   ankerl::svector<16>: median=    3.11 mean=    3.22 +/-  0.30 min=3.00 max=4.11
  eastl::fixed_vector<16>: median=    1.30 mean=    1.26 +/-  0.08 min=1.01 max=1.40

emplace_back:
       SmallVector<16>: median=    1.60 mean=    1.61 +/-  0.08 min=1.50 max=1.80
           std::vector: median=    1.20 mean=    1.19 +/-  0.08 min=1.00 max=1.31
  boost::small_vector<16>: median=    1.70 mean=    1.72 +/-  0.08 min=1.60 max=1.91
  llvm::SmallVector<16>: median=    1.50 mean=    1.47 +/-  0.07 min=1.40 max=1.61
  folly::small_vector<16>: median=    1.66 mean=    1.65 +/-  0.05 min=1.60 max=1.71
  absl::InlinedVector<16>: median=    1.70 mean=    1.66 +/-  0.08 min=1.50 max=1.80
   ankerl::svector<16>: median=    2.71 mean=    2.80 +/-  0.16 min=2.60 max=3.31
  eastl::fixed_vector<16>: median=    1.21 mean=    1.24 +/-  0.10 min=0.90 max=1.40

operator[]:
       SmallVector<16>: median=    1.10 mean=    1.10 +/-  0.09 min=1.00 max=1.31
           std::vector: median=    1.00 mean=    0.99 +/-  0.12 min=0.71 max=1.20
  boost::small_vector<16>: median=    1.10 mean=    1.08 +/-  0.07 min=1.00 max=1.21
  llvm::SmallVector<16>: median=    1.10 mean=    1.14 +/-  0.07 min=1.10 max=1.30
  folly::small_vector<16>: median=    1.00 mean=    1.04 +/-  0.11 min=0.90 max=1.40
  absl::InlinedVector<16>: median=    1.10 mean=    1.14 +/-  0.08 min=1.00 max=1.31
   ankerl::svector<16>: median=    1.10 mean=    1.07 +/-  0.10 min=0.80 max=1.20
  eastl::fixed_vector<16>: median=    1.00 mean=    1.01 +/-  0.14 min=0.80 max=1.41

iteration:
       SmallVector<16>: median=    1.11 mean=    1.14 +/-  0.07 min=1.00 max=1.30
           std::vector: median=    1.00 mean=    0.98 +/-  0.15 min=0.70 max=1.20
  boost::small_vector<16>: median=    1.10 mean=    1.11 +/-  0.13 min=0.90 max=1.40
  llvm::SmallVector<16>: median=    1.10 mean=    1.12 +/-  0.05 min=1.10 max=1.30
  folly::small_vector<16>: median=    1.00 mean=    1.01 +/-  0.15 min=0.80 max=1.41
  absl::InlinedVector<16>: median=    1.00 mean=    1.04 +/-  0.08 min=0.80 max=1.20
   ankerl::svector<16>: median=    1.00 mean=    1.00 +/-  0.15 min=0.70 max=1.30
  eastl::fixed_vector<16>: median=    1.10 mean=    1.06 +/-  0.18 min=0.60 max=1.40

copy ctor:
       SmallVector<16>: median=    0.60 mean=    0.60 +/-  0.08 min=0.40 max=0.80
           std::vector: median=    0.50 mean=    0.53 +/-  0.08 min=0.40 max=0.70
  boost::small_vector<16>: median=    0.60 mean=    0.60 +/-  0.05 min=0.50 max=0.70
  llvm::SmallVector<16>: median=    0.60 mean=    0.62 +/-  0.07 min=0.50 max=0.81
  folly::small_vector<16>: median=    0.51 mean=    0.55 +/-  0.08 min=0.40 max=0.70
  absl::InlinedVector<16>: median=    0.60 mean=    0.60 +/-  0.07 min=0.50 max=0.81
   ankerl::svector<16>: median=    0.90 mean=    0.89 +/-  0.10 min=0.80 max=1.20
  eastl::fixed_vector<16>: median=    0.50 mean=    0.52 +/-  0.07 min=0.40 max=0.60

move ctor:
       SmallVector<16>: median=    0.49 mean=    0.47 +/-  0.10 min=0.30 max=0.61
           std::vector: median=    0.40 mean=    2.09 +/-  7.62 min=0.30 max=34.46
  boost::small_vector<16>: median=    0.50 mean=    0.54 +/-  0.15 min=0.40 max=0.90
  llvm::SmallVector<16>: median=    0.60 mean=    0.59 +/-  0.14 min=0.40 max=0.90
  folly::small_vector<16>: median=    0.50 mean=    0.52 +/-  0.12 min=0.30 max=0.80
  absl::InlinedVector<16>: median=    0.49 mean=    0.48 +/-  0.11 min=0.30 max=0.80
   ankerl::svector<16>: median=    0.51 mean=    0.58 +/-  0.15 min=0.40 max=1.00
  eastl::fixed_vector<16>: median=    1.30 mean=    1.48 +/-  0.86 min=0.90 max=4.91
--- N = 1000 ---

[2026-02-16 04:11:22] Starting CPU: 3256 MHz (~base: 3256)
push_back:
       SmallVector<16>: median=    1.19 mean=    1.20 +/-  0.02 min=1.17 max=1.24
           std::vector: median=    0.82 mean=    0.82 +/-  0.03 min=0.78 max=0.86
  boost::small_vector<16>: median=    1.30 mean=    1.29 +/-  0.02 min=1.25 max=1.32
  llvm::SmallVector<16>: median=    1.24 mean=    1.25 +/-  0.02 min=1.22 max=1.27
  folly::small_vector<16>: median=    1.28 mean=    1.29 +/-  0.02 min=1.27 max=1.35
  absl::InlinedVector<16>: median=    1.28 mean=    1.28 +/-  0.02 min=1.26 max=1.34
   ankerl::svector<16>: median=    2.88 mean=    2.90 +/-  0.03 min=2.86 max=2.95
  eastl::fixed_vector<16>: median=    0.80 mean=    0.80 +/-  0.01 min=0.78 max=0.81

emplace_back:
       SmallVector<16>: median=    1.20 mean=    1.20 +/-  0.02 min=1.17 max=1.23
           std::vector: median=    0.79 mean=    0.80 +/-  0.02 min=0.78 max=0.84
  boost::small_vector<16>: median=    1.29 mean=    1.29 +/-  0.01 min=1.26 max=1.30
  llvm::SmallVector<16>: median=    1.11 mean=    1.11 +/-  0.02 min=1.09 max=1.16
  folly::small_vector<16>: median=    1.29 mean=    1.30 +/-  0.03 min=1.26 max=1.36
  absl::InlinedVector<16>: median=    1.28 mean=    1.29 +/-  0.03 min=1.27 max=1.35
   ankerl::svector<16>: median=    2.50 mean=    2.50 +/-  0.03 min=2.46 max=2.56
  eastl::fixed_vector<16>: median=    0.79 mean=    0.79 +/-  0.01 min=0.78 max=0.82

operator[]:
       SmallVector<16>: median=    0.66 mean=    0.67 +/-  0.01 min=0.66 max=0.68
           std::vector: median=    0.54 mean=    0.54 +/-  0.01 min=0.51 max=0.57
  boost::small_vector<16>: median=    0.54 mean=    0.54 +/-  0.01 min=0.53 max=0.56
  llvm::SmallVector<16>: median=    0.67 mean=    0.67 +/-  0.01 min=0.66 max=0.69
  folly::small_vector<16>: median=    0.54 mean=    0.54 +/-  0.01 min=0.52 max=0.56
  absl::InlinedVector<16>: median=    0.55 mean=    0.55 +/-  0.01 min=0.53 max=0.56
   ankerl::svector<16>: median=    0.55 mean=    0.55 +/-  0.01 min=0.53 max=0.56
  eastl::fixed_vector<16>: median=    0.54 mean=    0.55 +/-  0.02 min=0.51 max=0.58

iteration:
       SmallVector<16>: median=    0.66 mean=    0.66 +/-  0.01 min=0.64 max=0.68
           std::vector: median=    0.51 mean=    0.49 +/-  0.05 min=0.36 max=0.53
  boost::small_vector<16>: median=    0.39 mean=    0.40 +/-  0.02 min=0.38 max=0.44
  llvm::SmallVector<16>: median=    0.67 mean=    0.67 +/-  0.01 min=0.66 max=0.68
  folly::small_vector<16>: median=    0.42 mean=    0.41 +/-  0.02 min=0.37 max=0.44
  absl::InlinedVector<16>: median=    0.39 mean=    0.40 +/-  0.02 min=0.37 max=0.44
   ankerl::svector<16>: median=    0.50 mean=    0.50 +/-  0.01 min=0.49 max=0.52
  eastl::fixed_vector<16>: median=    0.51 mean=    0.51 +/-  0.02 min=0.47 max=0.52

copy ctor:
       SmallVector<16>: median=    0.14 mean=    0.15 +/-  0.02 min=0.14 max=0.22
           std::vector: median=    0.18 mean=    0.18 +/-  0.01 min=0.16 max=0.20
  boost::small_vector<16>: median=    0.19 mean=    0.18 +/-  0.01 min=0.16 max=0.19
  llvm::SmallVector<16>: median=    0.19 mean=    0.18 +/-  0.01 min=0.16 max=0.20
  folly::small_vector<16>: median=    0.18 mean=    0.18 +/-  0.02 min=0.15 max=0.21
  absl::InlinedVector<16>: median=    0.19 mean=    0.19 +/-  0.01 min=0.15 max=0.21
   ankerl::svector<16>: median=    0.17 mean=    0.37 +/-  0.88 min=0.16 max=4.12
  eastl::fixed_vector<16>: median=    0.18 mean=    0.18 +/-  0.01 min=0.15 max=0.20

move ctor:
       SmallVector<16>: median=    0.04 mean=    0.04 +/-  0.02 min=0.03 max=0.10
           std::vector: median=    0.04 mean=    0.04 +/-  0.01 min=0.03 max=0.06
  boost::small_vector<16>: median=    0.04 mean=    0.04 +/-  0.01 min=0.03 max=0.08
  llvm::SmallVector<16>: median=    0.04 mean=    0.04 +/-  0.01 min=0.03 max=0.07
  folly::small_vector<16>: median=    0.04 mean=    0.04 +/-  0.01 min=0.03 max=0.08
  absl::InlinedVector<16>: median=    0.04 mean=    0.04 +/-  0.01 min=0.03 max=0.06
   ankerl::svector<16>: median=    0.04 mean=    0.04 +/-  0.02 min=0.03 max=0.11
  eastl::fixed_vector<16>: median=    0.26 mean=    0.27 +/-  0.05 min=0.24 max=0.46
--- N = 10000 ---

[2026-02-16 04:11:22] Starting CPU: 3244 MHz (~base: 3244)
push_back:
       SmallVector<16>: median=    1.24 mean=    1.24 +/-  0.01 min=1.23 max=1.26
           std::vector: median=    0.79 mean=    0.79 +/-  0.00 min=0.78 max=0.80
  boost::small_vector<16>: median=    1.30 mean=    1.30 +/-  0.01 min=1.26 max=1.31
  llvm::SmallVector<16>: median=    1.25 mean=    1.37 +/-  0.48 min=1.24 max=3.40
  folly::small_vector<16>: median=    1.28 mean=    1.50 +/-  0.96 min=1.27 max=5.58
  absl::InlinedVector<16>: median=    1.27 mean=    1.28 +/-  0.02 min=1.26 max=1.32
   ankerl::svector<16>: median=    2.80 mean=    2.80 +/-  0.01 min=2.79 max=2.81
  eastl::fixed_vector<16>: median=    0.79 mean=    0.79 +/-  0.00 min=0.79 max=0.79

emplace_back:
       SmallVector<16>: median=    1.25 mean=    1.30 +/-  0.26 min=1.23 max=2.39
           std::vector: median=    0.79 mean=    0.79 +/-  0.01 min=0.78 max=0.80
  boost::small_vector<16>: median=    1.31 mean=    1.31 +/-  0.01 min=1.30 max=1.32
  llvm::SmallVector<16>: median=    1.22 mean=    1.22 +/-  0.01 min=1.21 max=1.24
  folly::small_vector<16>: median=    1.29 mean=    1.30 +/-  0.02 min=1.28 max=1.36
  absl::InlinedVector<16>: median=    1.27 mean=    1.27 +/-  0.01 min=1.26 max=1.32
   ankerl::svector<16>: median=    2.45 mean=    2.51 +/-  0.26 min=2.43 max=3.63
  eastl::fixed_vector<16>: median=    0.79 mean=    0.79 +/-  0.00 min=0.78 max=0.80

operator[]:
       SmallVector<16>: median=    0.63 mean=    0.63 +/-  0.00 min=0.62 max=0.63
           std::vector: median=    0.55 mean=    0.55 +/-  0.00 min=0.54 max=0.55
  boost::small_vector<16>: median=    0.55 mean=    0.55 +/-  0.00 min=0.54 max=0.55
  llvm::SmallVector<16>: median=    0.63 mean=    0.71 +/-  0.26 min=0.62 max=1.55
  folly::small_vector<16>: median=    0.55 mean=    0.55 +/-  0.00 min=0.54 max=0.55
  absl::InlinedVector<16>: median=    0.55 mean=    0.55 +/-  0.00 min=0.54 max=0.55
   ankerl::svector<16>: median=    0.55 mean=    0.55 +/-  0.00 min=0.54 max=0.55
  eastl::fixed_vector<16>: median=    0.55 mean=    0.59 +/-  0.17 min=0.54 max=1.32

iteration:
       SmallVector<16>: median=    0.62 mean=    0.62 +/-  0.00 min=0.62 max=0.63
           std::vector: median=    0.45 mean=    0.44 +/-  0.03 min=0.36 max=0.46
  boost::small_vector<16>: median=    0.36 mean=    0.36 +/-  0.00 min=0.36 max=0.36
  llvm::SmallVector<16>: median=    0.62 mean=    0.62 +/-  0.00 min=0.62 max=0.63
  folly::small_vector<16>: median=    0.36 mean=    0.36 +/-  0.00 min=0.36 max=0.36
  absl::InlinedVector<16>: median=    0.36 mean=    0.36 +/-  0.00 min=0.36 max=0.37
   ankerl::svector<16>: median=    0.45 mean=    0.51 +/-  0.24 min=0.45 max=1.52
  eastl::fixed_vector<16>: median=    0.45 mean=    0.45 +/-  0.00 min=0.45 max=0.46

copy ctor:
       SmallVector<16>: median=    1.42 mean=    1.54 +/-  0.27 min=1.40 max=2.10
           std::vector: median=    1.54 mean=    1.75 +/-  0.34 min=1.52 max=2.48
  boost::small_vector<16>: median=    1.55 mean=    1.74 +/-  0.30 min=1.53 max=2.23
  llvm::SmallVector<16>: median=    1.55 mean=    1.71 +/-  0.30 min=1.52 max=2.24
  folly::small_vector<16>: median=    1.54 mean=    1.71 +/-  0.30 min=1.52 max=2.26
  absl::InlinedVector<16>: median=    1.55 mean=    1.72 +/-  0.30 min=1.52 max=2.28
   ankerl::svector<16>: median=    4.37 mean=    4.24 +/-  0.29 min=3.85 max=4.61
  eastl::fixed_vector<16>: median=    1.53 mean=    1.66 +/-  0.26 min=1.52 max=2.24

move ctor:
       SmallVector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.01
           std::vector: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.01
  boost::small_vector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.01
  llvm::SmallVector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.01 max=0.01
  folly::small_vector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.01
  absl::InlinedVector<16>: median=    0.01 mean=    0.00 +/-  0.00 min=0.00 max=0.01
   ankerl::svector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.01
  eastl::fixed_vector<16>: median=    1.72 mean=    2.09 +/-  0.73 min=1.69 max=4.60
================================================================================
  INLINE VS HEAP PERFORMANCE
================================================================================

The key SmallVector advantage: zero allocations for small sizes.
Testing operations at various sizes relative to inline capacity.

[2026-02-16 04:11:22] Starting CPU: 3243 MHz (~base: 3243)
Size | SmallVector push_back (ns) | std::vector push_back (ns) | Ratio
-----|----------------------------|----------------------------|------
   4 (inline) |                       7.24 |                      13.48 |  1.86x
   8 (inline) |                       3.66 |                       9.23 |  2.52x
  12 (inline) |                       2.73 |                       7.79 |  2.86x
  16 (inline) |                       2.19 |                       6.09 |  2.78x
  20 (heap) |                       2.95 |                       5.83 |  1.97x
  32 (heap) |                       2.04 |                       3.83 |  1.88x
  64 (heap) |                       1.84 |                       2.63 |  1.43x
 128 (heap) |                       1.63 |                       1.76 |  1.08x

================================================================================
  ALLOCATION COUNT COMPARISON
================================================================================

Counting heap allocations for various scenarios.
SmallVector should have zero allocations when size <= InlineCapacity.

[2026-02-16 04:11:22] Starting CPU: 2445 MHz (~base: 2445)
Scenario: push_back N elements (no reserve)

    N | SmallVector allocs | std::vector allocs
------|--------------------|-----------------
    1 |                  0 |                 1
    8 |                  0 |                 4
   16 |                  0 |                 5
   17 |                  1 |                 6
   32 |                  1 |                 6
  100 |                  3 |                 8
 1000 |                  6 |                11

Note: SmallVector<16> uses inline storage for N <= 16

================================================================================
  INSERT/ERASE OPERATIONS
================================================================================

Comparing insert and erase at various positions.

[2026-02-16 04:11:22] Starting CPU: 2445 MHz (~base: 2445)
Insert single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |               90.05 |              118.14 |  1.31x
  middle |               57.55 |               79.91 |  1.39x
    back |               13.34 |               38.14 |  2.86x

Erase single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |              142.49 |              136.12 |  0.96x
  middle |               76.68 |               73.53 |  0.96x
    back |               12.89 |               12.83 |  1.00x

================================================================================
  INLINE CAPACITY SENSITIVITY
================================================================================

How does inline capacity choice affect performance?
Testing push_back of N elements with various inline capacities.

[2026-02-16 04:11:22] Starting CPU: 2948 MHz (~base: 2948)
Target size: 32 elements

InlineCapacity | Time (ns/op) | Allocations | Notes
---------------|--------------|-------------|------
  0 (std::vec) |         3.81 |           6 | always heap
             8 |         2.43 |           2 | transitions
            16 |         1.88 |           1 | transitions
            32 |         1.61 |           0 | all inline
            64 |         1.58 |           0 | all inline

================================================================================
  FAST PATH THROUGHPUT (ISOLATION BENCHMARK)
================================================================================

Measuring pure fast-path performance with zero allocations.
This isolates the emplace_back fast-path optimization from allocation benefits.

[2026-02-16 04:11:22] Starting CPU: 2445 MHz (~base: 2445)

--- emplace_back Throughput (100% Fast Path) ---

InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio
----------|---------------------|----------------------|------
* std::vector is pre-reserved to avoid allocation

        4 |                7.27 |                 8.07 |  1.11x
        8 |                3.72 |                 4.04 |  1.09x
       16 |                2.22 |                 1.91 |  0.86x
       32 |                1.57 |                 1.10 |  0.70x

--- push_back vs emplace_back (InlineCap=16) ---

Operation   | SmallVector (ns/op) | std::vector* (ns/op)
------------|---------------------|---------------------
push_back   |                2.19 |                1.89
emplace_back|                2.19 |                1.88

--- Pairwise Fast-Path Comparisons (InlineCap=16, N=16) ---
Each pair tested in isolated function call (noinline).

fat_p= 2.20  std::vec= 1.88
fat_p= 2.23  boost= 2.36
fat_p= 2.24  llvm= 2.08
fat_p= 2.24  folly= 2.18
fat_p= 2.23  absl= 2.61
fat_p= 2.21  ankerl= 3.85
fat_p= 2.17  eastl= 1.94

--- Fill/Clear Cycle (1000 cycles, InlineCap=16) ---
Tests repeated use of same SmallVector (cache-warm scenario)

SmallVector (reused): 0.93 ns/op
std::vector (reused): 0.42 ns/op

Note: This benchmark measures the optimized fast-path code.
The main SmallVector advantage (7-8x for small N) comes from
avoiding heap allocation entirely, which is tested in
benchmark_inline_vs_heap().

================================================================================
  OBJECT SIZE IMPACT
================================================================================

How does sizeof(SmallVector) compare to std::vector?
Larger inline capacity = larger object size.

Container                        | sizeof (bytes)
---------------------------------|---------------
            std::vector<int64_t> |             24
         SmallVector<int64_t, 4> |             56
         SmallVector<int64_t, 8> |             88
        SmallVector<int64_t, 16> |            152
        SmallVector<int64_t, 32> |            280
        SmallVector<int64_t, 64> |            536

Boost comparison:
boost::small_vector<int64_t, 16> |            152

Folly comparison:
folly::small_vector<int64_t, 16> |            136

LLVM comparison:
  llvm::SmallVector<int64_t, 16> |            144

Abseil comparison:
absl::InlinedVector<int64_t, 16> |            136

ankerl comparison:
    ankerl::svector<int64_t, 16> |            136
     ankerl::svector<int64_t, 1> |             16

EASTL comparison:
 eastl::fixed_vector<int64_t,16> |            168

Note: SmallVector<T, N> size ≈ 3*sizeof(void*) + N*sizeof(T)

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  CORE OPERATIONS
================================================================================

Comparing fundamental vector operations.
All containers pre-reserved to target size.
--- N = 100 ---

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)
push_back:
       SmallVector<16>: median=    2.00 mean=    2.40 +/-  1.85 min=1.00 max=6.00
           std::vector: median=    2.00 mean=    2.45 +/-  0.51 min=2.00 max=3.00
  boost::small_vector<16>: median=    2.00 mean=    2.00 +/-  0.46 min=1.00 max=3.00
  folly::small_vector<16>: median=    2.00 mean=    2.25 +/-  0.44 min=2.00 max=3.00
  absl::InlinedVector<16>: median=    2.00 mean=    1.75 +/-  0.44 min=1.00 max=2.00
   ankerl::svector<16>: median=    2.00 mean=    1.90 +/-  0.31 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    3.00 mean=    3.00 +/-  0.73 min=2.00 max=5.00

emplace_back:
       SmallVector<16>: median=    2.00 mean=    2.65 +/-  1.57 min=1.00 max=5.00
           std::vector: median=    2.00 mean=    1.65 +/-  0.49 min=1.00 max=2.00
  boost::small_vector<16>: median=    2.00 mean=    1.75 +/-  0.44 min=1.00 max=2.00
  folly::small_vector<16>: median=    2.00 mean=    1.90 +/-  0.31 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    2.00 mean=    1.90 +/-  0.45 min=1.00 max=3.00
   ankerl::svector<16>: median=    2.00 mean=    2.35 +/-  0.49 min=2.00 max=3.00
  eastl::fixed_vector<16>: median=    2.00 mean=    2.20 +/-  0.95 min=1.00 max=4.00

operator[]:
       SmallVector<16>: median=    1.50 mean=    1.50 +/-  0.51 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.45 +/-  0.51 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.35 +/-  0.49 min=1.00 max=2.00
  folly::small_vector<16>: median=    1.00 mean=    1.25 +/-  0.44 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.50 mean=    1.50 +/-  0.51 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.50 mean=    1.50 +/-  0.51 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.35 +/-  0.49 min=1.00 max=2.00

iteration:
       SmallVector<16>: median=    1.00 mean=    1.25 +/-  0.44 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.10 +/-  0.31 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.20 +/-  0.41 min=1.00 max=2.00
  folly::small_vector<16>: median=    1.00 mean=    1.20 +/-  0.41 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.15 +/-  0.37 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    1.15 +/-  0.37 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.10 +/-  0.31 min=1.00 max=2.00

copy ctor:
       SmallVector<16>: median=    3.00 mean=    2.90 +/-  0.31 min=2.00 max=3.00
           std::vector: median=    2.00 mean=    2.40 +/-  0.50 min=2.00 max=3.00
  boost::small_vector<16>: median=    2.00 mean=    2.45 +/-  0.51 min=2.00 max=3.00
  folly::small_vector<16>: median=    2.50 mean=    2.50 +/-  0.51 min=2.00 max=3.00
  absl::InlinedVector<16>: median=    2.00 mean=    2.45 +/-  0.51 min=2.00 max=3.00
   ankerl::svector<16>: median=    3.00 mean=    2.75 +/-  0.44 min=2.00 max=3.00
  eastl::fixed_vector<16>: median=    2.00 mean=    2.45 +/-  0.51 min=2.00 max=3.00

move ctor:
       SmallVector<16>: median=    0.50 mean=    0.50 +/-  0.51 min=0.00 max=1.00
           std::vector: median=    0.00 mean=    0.30 +/-  0.47 min=0.00 max=1.00
  boost::small_vector<16>: median=    0.00 mean=    0.40 +/-  0.50 min=0.00 max=1.00
  folly::small_vector<16>: median=    0.00 mean=    0.35 +/-  0.49 min=0.00 max=1.00
  absl::InlinedVector<16>: median=    1.00 mean=    0.60 +/-  0.50 min=0.00 max=1.00
   ankerl::svector<16>: median=    1.00 mean=    0.60 +/-  0.50 min=0.00 max=1.00
  eastl::fixed_vector<16>: median=    3.00 mean=    3.20 +/-  0.41 min=3.00 max=4.00
--- N = 1000 ---

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)
push_back:
       SmallVector<16>: median=    1.30 mean=    1.27 +/-  0.05 min=1.20 max=1.30
           std::vector: median=    2.10 mean=    2.07 +/-  0.05 min=2.00 max=2.10
  boost::small_vector<16>: median=    1.30 mean=    1.33 +/-  0.05 min=1.30 max=1.40
  folly::small_vector<16>: median=    1.90 mean=    1.91 +/-  0.04 min=1.90 max=2.00
  absl::InlinedVector<16>: median=    1.50 mean=    1.50 +/-  0.04 min=1.40 max=1.60
   ankerl::svector<16>: median=    1.60 mean=    1.61 +/-  0.02 min=1.60 max=1.70
  eastl::fixed_vector<16>: median=    2.30 mean=    2.34 +/-  0.05 min=2.30 max=2.40

emplace_back:
       SmallVector<16>: median=    2.05 mean=    1.83 +/-  0.56 min=1.20 max=2.60
           std::vector: median=    1.60 mean=    1.77 +/-  0.68 min=1.10 max=2.80
  boost::small_vector<16>: median=    1.70 mean=    1.65 +/-  0.31 min=1.30 max=2.40
  folly::small_vector<16>: median=    2.65 mean=    2.75 +/-  1.06 min=1.70 max=4.00
  absl::InlinedVector<16>: median=    2.00 mean=    2.27 +/-  0.80 min=1.50 max=3.50
   ankerl::svector<16>: median=    4.05 mean=    3.35 +/-  1.32 min=1.90 max=5.00
  eastl::fixed_vector<16>: median=    1.50 mean=    1.50 +/-  0.42 min=1.10 max=2.40

operator[]:
       SmallVector<16>: median=    0.85 mean=    0.85 +/-  0.05 min=0.80 max=0.90
           std::vector: median=    0.80 mean=    0.82 +/-  0.04 min=0.80 max=0.90
  boost::small_vector<16>: median=    0.80 mean=    0.83 +/-  0.05 min=0.80 max=0.90
  folly::small_vector<16>: median=    1.00 mean=    1.01 +/-  0.02 min=1.00 max=1.10
  absl::InlinedVector<16>: median=    1.00 mean=    2.12 +/-  4.94 min=1.00 max=23.10
   ankerl::svector<16>: median=    1.00 mean=    1.01 +/-  0.04 min=0.90 max=1.10
  eastl::fixed_vector<16>: median=    0.90 mean=    0.86 +/-  0.06 min=0.80 max=1.00

iteration:
       SmallVector<16>: median=    0.50 mean=    0.52 +/-  0.04 min=0.50 max=0.60
           std::vector: median=    0.50 mean=    0.48 +/-  0.04 min=0.40 max=0.50
  boost::small_vector<16>: median=    0.50 mean=    0.53 +/-  0.05 min=0.50 max=0.60
  folly::small_vector<16>: median=    0.50 mean=    0.51 +/-  0.02 min=0.50 max=0.60
  absl::InlinedVector<16>: median=    0.70 mean=    0.68 +/-  0.04 min=0.60 max=0.70
   ankerl::svector<16>: median=    0.50 mean=    0.51 +/-  0.02 min=0.50 max=0.60
  eastl::fixed_vector<16>: median=    0.50 mean=    0.47 +/-  0.04 min=0.40 max=0.50

copy ctor:
       SmallVector<16>: median=    0.70 mean=    0.73 +/-  0.04 min=0.70 max=0.80
           std::vector: median=    0.30 mean=    0.28 +/-  0.04 min=0.20 max=0.30
  boost::small_vector<16>: median=    0.30 mean=    0.31 +/-  0.03 min=0.30 max=0.40
  folly::small_vector<16>: median=    0.30 mean=    0.30 +/-  0.03 min=0.20 max=0.40
  absl::InlinedVector<16>: median=    0.30 mean=    0.29 +/-  0.02 min=0.20 max=0.30
   ankerl::svector<16>: median=    0.30 mean=    0.32 +/-  0.05 min=0.20 max=0.40
  eastl::fixed_vector<16>: median=    0.30 mean=    0.32 +/-  0.04 min=0.30 max=0.40

move ctor:
       SmallVector<16>: median=    0.00 mean=    0.05 +/-  0.06 min=0.00 max=0.20
           std::vector: median=    0.10 mean=    0.05 +/-  0.05 min=0.00 max=0.10
  boost::small_vector<16>: median=    0.10 mean=    0.05 +/-  0.05 min=0.00 max=0.10
  folly::small_vector<16>: median=    0.00 mean=    0.04 +/-  0.05 min=0.00 max=0.10
  absl::InlinedVector<16>: median=    0.00 mean=    0.04 +/-  0.05 min=0.00 max=0.10
   ankerl::svector<16>: median=    0.10 mean=    0.06 +/-  0.05 min=0.00 max=0.10
  eastl::fixed_vector<16>: median=    0.90 mean=    1.05 +/-  0.53 min=0.80 max=2.70
--- N = 10000 ---

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)
push_back:
       SmallVector<16>: median=    1.21 mean=    1.32 +/-  0.36 min=1.20 max=2.58
           std::vector: median=    2.02 mean=    2.03 +/-  0.06 min=2.02 max=2.28
  boost::small_vector<16>: median=    1.37 mean=    1.39 +/-  0.09 min=1.34 max=1.77
  folly::small_vector<16>: median=    1.89 mean=    2.02 +/-  0.59 min=1.88 max=4.54
  absl::InlinedVector<16>: median=    1.29 mean=    1.29 +/-  0.01 min=1.28 max=1.30
   ankerl::svector<16>: median=    1.29 mean=    1.34 +/-  0.21 min=1.29 max=2.25
  eastl::fixed_vector<16>: median=    2.30 mean=    2.53 +/-  0.58 min=2.30 max=4.21

emplace_back:
       SmallVector<16>: median=    1.21 mean=    1.21 +/-  0.01 min=1.20 max=1.22
           std::vector: median=    1.07 mean=    1.13 +/-  0.27 min=1.06 max=2.27
  boost::small_vector<16>: median=    1.37 mean=    1.38 +/-  0.06 min=1.35 max=1.64
  folly::small_vector<16>: median=    1.59 mean=    1.66 +/-  0.32 min=1.57 max=3.02
  absl::InlinedVector<16>: median=    1.29 mean=    1.41 +/-  0.56 min=1.28 max=3.79
   ankerl::svector<16>: median=    1.86 mean=    1.86 +/-  0.01 min=1.86 max=1.88
  eastl::fixed_vector<16>: median=    1.06 mean=    1.06 +/-  0.01 min=1.06 max=1.08

operator[]:
       SmallVector<16>: median=    0.82 mean=    0.90 +/-  0.35 min=0.81 max=2.37
           std::vector: median=    0.86 mean=    0.86 +/-  0.05 min=0.81 max=1.03
  boost::small_vector<16>: median=    0.83 mean=    0.83 +/-  0.01 min=0.82 max=0.83
  folly::small_vector<16>: median=    1.02 mean=    1.02 +/-  0.01 min=1.01 max=1.06
  absl::InlinedVector<16>: median=    1.02 mean=    1.03 +/-  0.04 min=1.01 max=1.18
   ankerl::svector<16>: median=    1.01 mean=    1.01 +/-  0.01 min=1.01 max=1.02
  eastl::fixed_vector<16>: median=    0.86 mean=    0.88 +/-  0.11 min=0.82 max=1.33

iteration:
       SmallVector<16>: median=    0.45 mean=    0.49 +/-  0.10 min=0.44 max=0.78
           std::vector: median=    0.42 mean=    0.45 +/-  0.10 min=0.42 max=0.80
  boost::small_vector<16>: median=    0.45 mean=    0.48 +/-  0.09 min=0.44 max=0.80
  folly::small_vector<16>: median=    0.45 mean=    0.48 +/-  0.09 min=0.44 max=0.77
  absl::InlinedVector<16>: median=    0.63 mean=    0.91 +/-  0.79 min=0.62 max=4.03
   ankerl::svector<16>: median=    0.45 mean=    0.61 +/-  0.39 min=0.44 max=2.18
  eastl::fixed_vector<16>: median=    0.42 mean=    0.47 +/-  0.11 min=0.41 max=0.81

copy ctor:
       SmallVector<16>: median=    0.59 mean=    0.73 +/-  0.32 min=0.56 max=1.99
           std::vector: median=    0.21 mean=    0.23 +/-  0.05 min=0.18 max=0.33
  boost::small_vector<16>: median=    0.29 mean=    0.46 +/-  0.59 min=0.20 max=2.47
  folly::small_vector<16>: median=    0.21 mean=    0.32 +/-  0.36 min=0.19 max=1.84
  absl::InlinedVector<16>: median=    0.20 mean=    0.22 +/-  0.05 min=0.18 max=0.32
   ankerl::svector<16>: median=    0.28 mean=    1.28 +/-  1.80 min=0.20 max=5.16
  eastl::fixed_vector<16>: median=    0.21 mean=    0.34 +/-  0.49 min=0.20 max=2.42

move ctor:
       SmallVector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.02
           std::vector: median=    0.01 mean=    0.01 +/-  0.01 min=0.00 max=0.02
  boost::small_vector<16>: median=    0.01 mean=    0.01 +/-  0.01 min=0.00 max=0.03
  folly::small_vector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.02
  absl::InlinedVector<16>: median=    0.01 mean=    0.01 +/-  0.00 min=0.00 max=0.02
   ankerl::svector<16>: median=    0.02 mean=    0.02 +/-  0.01 min=0.00 max=0.04
  eastl::fixed_vector<16>: median=    0.66 mean=    0.89 +/-  0.38 min=0.65 max=1.60
================================================================================
  INLINE VS HEAP PERFORMANCE
================================================================================

The key SmallVector advantage: zero allocations for small sizes.
Testing operations at various sizes relative to inline capacity.

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)
Size | SmallVector push_back (ns) | std::vector push_back (ns) | Ratio
-----|----------------------------|----------------------------|------
   4 (inline) |                       7.67 |                      65.63 |  8.56x
   8 (inline) |                       4.56 |                      51.31 | 11.26x
  12 (inline) |                       3.26 |                      38.63 | 11.85x
  16 (inline) |                       2.92 |                      35.40 | 12.11x
  20 (heap) |                       5.47 |                      30.65 |  5.60x
  32 (heap) |                       3.77 |                      22.07 |  5.85x
  64 (heap) |                       3.04 |                      10.98 |  3.62x
 128 (heap) |                       2.75 |                       6.05 |  2.20x

================================================================================
  ALLOCATION COUNT COMPARISON
================================================================================

Counting heap allocations for various scenarios.
SmallVector should have zero allocations when size <= InlineCapacity.

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)
Scenario: push_back N elements (no reserve)

    N | SmallVector allocs | std::vector allocs
------|--------------------|-----------------
    1 |                  0 |                 1
    8 |                  0 |                 6
   16 |                  0 |                 8
   17 |                  1 |                 8
   32 |                  1 |                10
  100 |                  3 |                13
 1000 |                  6 |                18

Note: SmallVector<16> uses inline storage for N <= 16

================================================================================
  INSERT/ERASE OPERATIONS
================================================================================

Comparing insert and erase at various positions.

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)
Insert single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |              174.65 |              167.00 |  0.96x
  middle |               93.55 |               88.05 |  0.94x
    back |               21.25 |               14.85 |  0.70x

Erase single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |               89.75 |               90.85 |  1.01x
  middle |               50.20 |               50.25 |  1.00x
    back |               13.20 |               11.80 |  0.89x

================================================================================
  INLINE CAPACITY SENSITIVITY
================================================================================

How does inline capacity choice affect performance?
Testing push_back of N elements with various inline capacities.

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)
Target size: 32 elements

InlineCapacity | Time (ns/op) | Allocations | Notes
---------------|--------------|-------------|------
  0 (std::vec) |        15.15 |          10 | always heap
             8 |         4.27 |           2 | transitions
            16 |         2.85 |           1 | transitions
            32 |         1.62 |           0 | all inline
            64 |         1.59 |           0 | all inline

================================================================================
  FAST PATH THROUGHPUT (ISOLATION BENCHMARK)
================================================================================

Measuring pure fast-path performance with zero allocations.
This isolates the emplace_back fast-path optimization from allocation benefits.

[2026-02-16 04:54:29] Starting CPU: 2445 MHz (base: 2445)

--- emplace_back Throughput (100% Fast Path) ---

InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio
----------|---------------------|----------------------|------
* std::vector is pre-reserved to avoid allocation

        4 |                7.63 |                 7.77 |  1.02x
        8 |                3.92 |                 4.17 |  1.06x
       16 |                2.24 |                 2.27 |  1.02x
       32 |                2.45 |                 1.91 |  0.78x

--- push_back vs emplace_back (InlineCap=16) ---

Operation   | SmallVector (ns/op) | std::vector* (ns/op)
------------|---------------------|---------------------
push_back   |                2.71 |                2.53
emplace_back|                2.77 |                2.75

--- Pairwise Fast-Path Comparisons (InlineCap=16, N=16) ---
Each pair tested in isolated function call (noinline).

fat_p= 2.24  std::vec= 2.18
fat_p= 2.26  boost= 2.24
fat_p= 2.35  folly= 2.29
fat_p= 3.64  absl= 3.85
fat_p= 2.99  ankerl= 9.73
fat_p= 2.35  eastl= 2.13

--- Fill/Clear Cycle (1000 cycles, InlineCap=16) ---
Tests repeated use of same SmallVector (cache-warm scenario)

SmallVector (reused): 3.02 ns/op
std::vector (reused): 1.50 ns/op

Note: This benchmark measures the optimized fast-path code.
The main SmallVector advantage (7-8x for small N) comes from
avoiding heap allocation entirely, which is tested in
benchmark_inline_vs_heap().

================================================================================
  OBJECT SIZE IMPACT
================================================================================

How does sizeof(SmallVector) compare to std::vector?
Larger inline capacity = larger object size.

Container                        | sizeof (bytes)
---------------------------------|---------------
            std::vector<int64_t> |             24
         SmallVector<int64_t, 4> |             64
         SmallVector<int64_t, 8> |             96
        SmallVector<int64_t, 16> |            160
        SmallVector<int64_t, 32> |            288
        SmallVector<int64_t, 64> |            544

Boost comparison:
boost::small_vector<int64_t, 16> |            152

Folly comparison:
folly::small_vector<int64_t, 16> |            136

Abseil comparison:
absl::InlinedVector<int64_t, 16> |            136

ankerl comparison:
    ankerl::svector<int64_t, 16> |            136
     ankerl::svector<int64_t, 1> |             16

EASTL comparison:
 eastl::fixed_vector<int64_t,16> |            168

Note: SmallVector<T, N> size ≈ 3*sizeof(void*) + N*sizeof(T)

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- folly::small_vector was not detected on Local.
- llvm::SmallVector (apt install llvm-dev) was not detected on MSVC CI.
