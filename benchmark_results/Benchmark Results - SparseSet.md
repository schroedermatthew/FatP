---
doc_id: BR-SparseSet-001
doc_type: "Benchmark Results"
title: "SparseSet"
fatp_components: ["SparseSet"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - SparseSet

**Source:** `benchmark_SparseSet.cpp`
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
| fat_p::SparseSet<8> | x | x | x | x |
| fat_p::SparseSet<32> | x | x | x | x |
| fat_p::FlatSet (sibling) | x | x | x | x |
| std::unordered_set | x | x | x | x |
| std::set | x | x | x | x |
| entt::sparse_set | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
[WARNING: CPU still 32% below base after 6s - 2506/3686 MHz]
WARNING: CPU frequency still fluctuating, results may have higher variance.

================================================================================
  SECTION 1: Core Operations
================================================================================

[2026-02-15 19:59:19] Section start CPU: 2506 MHz (base: 3686)
Contract Note: O(1) insert/erase/contains; dense iteration; unstable erase order
              Insert excludes allocation (reserve performed in setup)

--- N = 1000 ---
[2026-02-15 19:59:19] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2506 MHz]

  Insert:
     fat_p::SparseSet<8>:       1.90 ns/op (+/-   0.10, CI:[    1.86,    1.97])
    fat_p::SparseSet<32>:       1.00 ns/op (+/-   0.05, CI:[    1.00,    1.05])
          fat_p::FlatSet:      44.90 ns/op (+/-   2.63, CI:[   44.56,   47.47])
      std::unordered_set:      28.30 ns/op (+/-   3.34, CI:[   27.33,   31.02])
     absl::flat_hash_set:      18.10 ns/op (+/-   2.10, CI:[   17.50,   19.81])
      llvm::SparseSet<8>:       1.10 ns/op (+/-   0.04, CI:[    1.06,    1.10])
     llvm::SparseSet<32>:       0.80 ns/op (+/-   0.03, CI:[    0.79,    0.82])
        entt::sparse_set:       3.10 ns/op (+/-   0.51, CI:[    3.02,    3.59])
                std::set:      69.80 ns/op (+/-  15.76, CI:[   66.41,   83.83])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.40 ns/op (+/-   0.04, CI:[    0.40,    0.44])
    fat_p::SparseSet<32>:       2.30 ns/op (+/-   0.51, CI:[    1.93,    2.49])
          fat_p::FlatSet:      34.00 ns/op (+/-   2.23, CI:[   33.56,   36.02])
      std::unordered_set:       6.20 ns/op (+/-   1.16, CI:[    4.86,    6.14])
     absl::flat_hash_set:       6.50 ns/op (+/-   0.53, CI:[    6.40,    6.99])
      llvm::SparseSet<8>:       4.20 ns/op (+/-   0.43, CI:[    3.94,    4.41])
     llvm::SparseSet<32>:       2.10 ns/op (+/-   0.83, CI:[    1.81,    2.73])
        entt::sparse_set:       2.70 ns/op (+/-   0.57, CI:[    2.29,    2.92])
                std::set:      33.10 ns/op (+/-   1.22, CI:[   32.36,   33.71])

  Erase:
     fat_p::SparseSet<8>:       2.80 ns/op (+/-   0.62, CI:[    2.59,    3.28])
    fat_p::SparseSet<32>:       1.00 ns/op (+/-   0.07, CI:[    0.93,    1.01])
          fat_p::FlatSet:      43.60 ns/op (+/-   1.69, CI:[   43.08,   44.94])
      std::unordered_set:      14.80 ns/op (+/-   0.90, CI:[   13.94,   14.94])
     absl::flat_hash_set:       5.20 ns/op (+/-   0.19, CI:[    5.11,    5.32])
      llvm::SparseSet<8>:       4.20 ns/op (+/-   0.18, CI:[    3.97,    4.17])
     llvm::SparseSet<32>:       1.00 ns/op (+/-   0.14, CI:[    0.91,    1.06])
        entt::sparse_set:       2.80 ns/op (+/-   3.50, CI:[    1.83,    5.69])
                std::set:      80.60 ns/op (+/-   3.40, CI:[   79.09,   82.85])

  Iteration:
     fat_p::SparseSet<8>:       0.40 ns/op (+/-   0.17, CI:[    0.23,    0.41])
    fat_p::SparseSet<32>:       0.20 ns/op (+/-   0.04, CI:[    0.19,    0.23])
          fat_p::FlatSet:       0.20 ns/op (+/-   0.04, CI:[    0.20,    0.24])
      std::unordered_set:       0.90 ns/op (+/-   0.04, CI:[    0.90,    0.94])
     absl::flat_hash_set:       2.70 ns/op (+/-   1.63, CI:[    2.17,    3.98])
      llvm::SparseSet<8>:       0.20 ns/op (+/-   0.04, CI:[    0.19,    0.23])
     llvm::SparseSet<32>:       0.20 ns/op (+/-   0.04, CI:[    0.19,    0.23])
        entt::sparse_set:       0.20 ns/op (+/-   0.05, CI:[    0.21,    0.27])
                std::set:       4.20 ns/op (+/-   0.17, CI:[    4.13,    4.32])

--- N = 10000 ---
[2026-02-15 19:59:20] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]

  Insert:
     fat_p::SparseSet<8>:       0.72 ns/op (+/-   0.90, CI:[    0.47,    1.46])
    fat_p::SparseSet<32>:       1.19 ns/op (+/-   0.04, CI:[    1.16,    1.21])
          fat_p::FlatSet:     110.39 ns/op (+/-   5.38, CI:[  109.67,  115.61])
      std::unordered_set:      35.96 ns/op (+/-   2.31, CI:[   35.32,   37.87])
     absl::flat_hash_set:      16.54 ns/op (+/-   8.40, CI:[   14.44,   23.73])
      llvm::SparseSet<8>:       6.02 ns/op (+/-   5.61, CI:[    4.66,   10.87])
     llvm::SparseSet<32>:       1.12 ns/op (+/-   0.05, CI:[    1.12,    1.18])
        entt::sparse_set:       4.93 ns/op (+/-   7.93, CI:[    2.76,   11.53])
                std::set:     113.03 ns/op (+/-  11.72, CI:[  111.59,  124.54])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.35 ns/op (+/-   0.02, CI:[    0.34,    0.37])
    fat_p::SparseSet<32>:       3.79 ns/op (+/-   3.39, CI:[    2.87,    6.61])
          fat_p::FlatSet:      47.09 ns/op (+/-   2.48, CI:[   46.64,   49.37])
      std::unordered_set:       8.11 ns/op (+/-   0.21, CI:[    8.02,    8.25])
     absl::flat_hash_set:       7.43 ns/op (+/-   0.40, CI:[    7.45,    7.89])
      llvm::SparseSet<8>:      11.76 ns/op (+/-   0.54, CI:[   11.72,   12.32])
     llvm::SparseSet<32>:       3.90 ns/op (+/-   0.22, CI:[    3.89,    4.13])
        entt::sparse_set:       4.29 ns/op (+/-   0.19, CI:[    4.22,    4.43])
                std::set:      56.22 ns/op (+/-   1.11, CI:[   55.75,   56.98])

  Erase:
     fat_p::SparseSet<8>:       0.96 ns/op (+/-   0.09, CI:[    0.90,    1.00])
    fat_p::SparseSet<32>:       2.02 ns/op (+/-   0.08, CI:[    1.99,    2.07])
          fat_p::FlatSet:     110.70 ns/op (+/-  15.83, CI:[  106.59,  124.08])
      std::unordered_set:      15.36 ns/op (+/-   0.56, CI:[   15.18,   15.80])
     absl::flat_hash_set:       6.30 ns/op (+/-   0.72, CI:[    6.20,    7.00])
      llvm::SparseSet<8>:      10.72 ns/op (+/-   2.04, CI:[   10.12,   12.37])
     llvm::SparseSet<32>:       1.98 ns/op (+/-   2.64, CI:[    1.22,    4.13])
        entt::sparse_set:       3.60 ns/op (+/-   0.11, CI:[    3.57,    3.69])
                std::set:     124.90 ns/op (+/-   3.63, CI:[  123.78,  127.79])

  Iteration:
     fat_p::SparseSet<8>:       0.39 ns/op (+/-   0.15, CI:[    0.31,    0.47])
    fat_p::SparseSet<32>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
          fat_p::FlatSet:       0.19 ns/op (+/-   0.00, CI:[    0.19,    0.20])
      std::unordered_set:       2.33 ns/op (+/-   0.19, CI:[    2.29,    2.49])
     absl::flat_hash_set:       2.78 ns/op (+/-   0.14, CI:[    2.79,    2.95])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
     llvm::SparseSet<32>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
        entt::sparse_set:       0.21 ns/op (+/-   0.01, CI:[    0.21,    0.22])
                std::set:       6.38 ns/op (+/-   0.32, CI:[    6.31,    6.67])

--- N = 100000 ---
[2026-02-15 19:59:21] CPU: 2580 MHz (base: 3686)
[Cooling: size transition] [Ready: 2985 MHz]

  Insert:
     fat_p::SparseSet<8>:       0.60 ns/op (+/-   0.03, CI:[    0.61,    0.64])
    fat_p::SparseSet<32>:       2.18 ns/op (+/-   0.46, CI:[    2.09,    2.60])
          fat_p::FlatSet:    1233.00 ns/op (+/-  26.18, CI:[ 1210.39, 1239.32])
      std::unordered_set:      44.59 ns/op (+/-   2.94, CI:[   43.39,   46.64])
     absl::flat_hash_set:      14.34 ns/op (+/-   0.58, CI:[   13.85,   14.50])
      llvm::SparseSet<8>:     108.64 ns/op (+/-   4.79, CI:[  106.86,  112.15])
     llvm::SparseSet<32>:       1.73 ns/op (+/-   0.32, CI:[    1.61,    1.96])
        entt::sparse_set:       7.25 ns/op (+/-   0.92, CI:[    6.99,    8.01])
                std::set:     160.91 ns/op (+/-   4.63, CI:[  159.15,  164.26])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.40 ns/op (+/-   0.12, CI:[    0.38,    0.51])
    fat_p::SparseSet<32>:       5.07 ns/op (+/-   1.38, CI:[    4.80,    6.32])
          fat_p::FlatSet:      61.46 ns/op (+/-   2.71, CI:[   61.13,   64.12])
      std::unordered_set:      11.66 ns/op (+/-   1.21, CI:[   11.33,   12.66])
     absl::flat_hash_set:       8.56 ns/op (+/-   0.21, CI:[    8.47,    8.70])
      llvm::SparseSet<8>:     139.02 ns/op (+/-   4.49, CI:[  136.30,  141.26])
     llvm::SparseSet<32>:       5.32 ns/op (+/-   1.17, CI:[    5.14,    6.43])
        entt::sparse_set:       5.68 ns/op (+/-   1.17, CI:[    5.28,    6.58])
                std::set:     107.05 ns/op (+/-   3.48, CI:[  105.54,  109.38])

  Erase:
     fat_p::SparseSet<8>:       0.46 ns/op (+/-   0.05, CI:[    0.45,    0.50])
    fat_p::SparseSet<32>:       3.29 ns/op (+/-   0.38, CI:[    3.20,    3.61])
          fat_p::FlatSet:    1402.50 ns/op (+/-  32.41, CI:[ 1383.37, 1419.19])
      std::unordered_set:      25.00 ns/op (+/-   2.48, CI:[   24.37,   27.12])
     absl::flat_hash_set:      11.23 ns/op (+/-   0.82, CI:[   11.15,   12.05])
      llvm::SparseSet<8>:      54.21 ns/op (+/-   4.90, CI:[   52.83,   58.24])
     llvm::SparseSet<32>:       3.07 ns/op (+/-   0.30, CI:[    2.89,    3.22])
        entt::sparse_set:       6.24 ns/op (+/-   0.93, CI:[    6.08,    7.10])
                std::set:     209.89 ns/op (+/-   8.64, CI:[  206.01,  215.56])

  Iteration:
     fat_p::SparseSet<8>:       0.78 ns/op (+/-   0.35, CI:[    0.49,    0.87])
    fat_p::SparseSet<32>:       0.18 ns/op (+/-   0.01, CI:[    0.18,    0.19])
          fat_p::FlatSet:       0.18 ns/op (+/-   0.01, CI:[    0.18,    0.20])
      std::unordered_set:       4.54 ns/op (+/-   0.21, CI:[    4.53,    4.75])
     absl::flat_hash_set:       1.71 ns/op (+/-   0.21, CI:[    1.62,    1.85])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.01, CI:[    0.18,    0.19])
     llvm::SparseSet<32>:       0.19 ns/op (+/-   0.02, CI:[    0.19,    0.21])
        entt::sparse_set:       0.20 ns/op (+/-   0.01, CI:[    0.20,    0.21])
                std::set:      12.08 ns/op (+/-   0.40, CI:[   11.71,   12.15])
[Cooling: before iteration benchmark] [Ready: 2764 MHz]

================================================================================
  SECTION 2: Dense Iteration (SparseSet Key Advantage)
================================================================================

[2026-02-15 19:59:39] Section start CPU: 2764 MHz (base: 3686)
Contract Note: Dense iterationΓÇöSparseSet/FlatSet expected to outperform hash sets

--- N = 10000 ---
[2026-02-15 19:59:39] CPU: 2764 MHz (base: 3686)
[Cooling: size transition] [Ready: 2174 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.39 ns/op (+/-   0.16, CI:[    0.22,    0.40])
    fat_p::SparseSet<32>:       0.18 ns/op (+/-   0.01, CI:[    0.18,    0.19])
          fat_p::FlatSet:       0.18 ns/op (+/-   0.00, CI:[    0.18,    0.19])
      std::unordered_set:       2.01 ns/op (+/-   0.15, CI:[    1.98,    2.15])
     absl::flat_hash_set:       2.64 ns/op (+/-   0.09, CI:[    2.60,    2.71])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.80, CI:[   -0.05,    0.83])
     llvm::SparseSet<32>:       0.18 ns/op (+/-   0.01, CI:[    0.18,    0.19])
        entt::sparse_set:       0.20 ns/op (+/-   0.08, CI:[    0.17,    0.27])
                std::set:       6.04 ns/op (+/-   0.25, CI:[    5.96,    6.24])

--- N = 100000 ---
[2026-02-15 19:59:39] CPU: 2248 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.78 ns/op (+/-   0.28, CI:[    0.49,    0.81])
    fat_p::SparseSet<32>:       0.19 ns/op (+/-   0.07, CI:[    0.17,    0.25])
          fat_p::FlatSet:       0.18 ns/op (+/-   0.01, CI:[    0.18,    0.19])
      std::unordered_set:       4.59 ns/op (+/-   0.18, CI:[    4.53,    4.73])
     absl::flat_hash_set:       1.71 ns/op (+/-   0.06, CI:[    1.67,    1.74])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.01, CI:[    0.18,    0.19])
     llvm::SparseSet<32>:       0.20 ns/op (+/-   0.14, CI:[    0.16,    0.31])
        entt::sparse_set:       0.20 ns/op (+/-   0.02, CI:[    0.19,    0.22])
                std::set:      12.16 ns/op (+/-   0.92, CI:[   11.74,   12.76])

[Cooling: before mixed workload] [Ready: 2432 MHz]

================================================================================
  SECTION 3: Mixed Workload (Insert/Erase Churn)
================================================================================

[2026-02-15 19:59:44] Section start CPU: 2432 MHz (base: 3686)
Contract Note: Random insert/erase churnΓÇötests swap-with-back erase efficiency

--- N = 10000 (50% insert/erase cycles) ---
[2026-02-15 19:59:44] CPU: 2432 MHz (base: 3686)
  Mixed Workload:
     fat_p::SparseSet<8>:       1.23 ns/op (+/-   0.12, CI:[    1.18,    1.32])
    fat_p::SparseSet<32>:       2.08 ns/op (+/-   0.13, CI:[    2.07,    2.21])
          fat_p::FlatSet:     146.87 ns/op (+/-   2.92, CI:[  146.22,  149.45])
      std::unordered_set:      19.40 ns/op (+/-   0.58, CI:[   19.20,   19.84])
     absl::flat_hash_set:      10.64 ns/op (+/-   0.61, CI:[   10.42,   11.10])
      llvm::SparseSet<8>:      14.82 ns/op (+/-   0.63, CI:[   14.47,   15.16])
     llvm::SparseSet<32>:       2.19 ns/op (+/-   0.20, CI:[    2.12,    2.34])
        entt::sparse_set:       3.54 ns/op (+/-   0.36, CI:[    3.44,    3.84])
                std::set:      96.62 ns/op (+/-   2.09, CI:[   96.33,   98.65])

================================================================================
  Feature Comparison Summary
================================================================================

  Container                 Insert    Contains  Erase     Iteration  Order
  --------------------------------------------------------------------------
  fat_p::SparseSet<8>       O(1)*     O(1)      O(1)      Dense      Unstable  (max 256)
  fat_p::SparseSet<32>      O(1)*     O(1)      O(1)      Dense      Unstable  (handles large N)
  fat_p::FlatSet            O(n)      O(log n)  O(n)      Dense      Sorted
  std::unordered_set        O(1)*     O(1)*     O(1)*     Scattered  Unordered
  absl::flat_hash_set       O(1)*     O(1)*     O(1)*     Scattered  Unordered
  llvm::SparseSet<8>        O(1)*     O(1)      O(1)      Dense      Unstable  (default, max 256)
  llvm::SparseSet<32>       O(1)*     O(1)      O(1)      Dense      Unstable  (configured)
  entt::sparse_set          O(1)*     O(1)      O(1)      Dense      Unstable
  std::set                  O(log n)  O(log n)  O(log n)  In-order   Sorted

  * = amortized
  Note: <8> variants use uint8_t value type (max 256 elements)
        <32> variants use uint32_t value type (handles large N)

  When to use SparseSet:
    - Integer keys in a bounded range
    - Frequent insert/erase churn
    - Iteration performance matters
    - Order doesn't matter

  When to use FlatSet:
    - Need sorted order
    - Mostly lookups after initial build
    - Binary search semantics (lower_bound, etc.)

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
[2026-02-16 03:37:48] Section start CPU: 3491 MHz (max: 2800) [TURBO]
Contract Note: O(1) insert/erase/contains; dense iteration; unstable erase order
              Insert excludes allocation (reserve performed in setup)
--- N = 1000 ---
[2026-02-16 03:37:48] CPU: 3491 MHz (max: 2800) [TURBO]
[Cooling: size transition] [Ready: 2800 MHz]

  Insert:
     fat_p::SparseSet<8>:       4.18 ns/op (+/-   0.30, CI:[    4.03,    4.31])
    fat_p::SparseSet<32>:       3.15 ns/op (+/-   1.14, CI:[    2.85,    3.94])
          fat_p::FlatSet:      61.62 ns/op (+/-   1.57, CI:[   60.69,   62.20])
      std::unordered_set:      30.26 ns/op (+/-   0.70, CI:[   29.98,   30.65])
     absl::flat_hash_set:      17.20 ns/op (+/-   0.66, CI:[   16.88,   17.51])
      llvm::SparseSet<8>:       2.66 ns/op (+/-   0.03, CI:[    2.65,    2.68])
     llvm::SparseSet<32>:       2.04 ns/op (+/-   0.32, CI:[    1.78,    2.09])
        entt::sparse_set:       4.96 ns/op (+/-   0.15, CI:[    4.89,    5.04])
                std::set:      98.03 ns/op (+/-   2.68, CI:[   97.37,   99.94])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.90 ns/op (+/-   0.00, CI:[    0.90,    0.91])
    fat_p::SparseSet<32>:       1.19 ns/op (+/-   0.07, CI:[    1.11,    1.18])
          fat_p::FlatSet:      50.86 ns/op (+/-   1.90, CI:[   50.43,   52.25])
      std::unordered_set:      15.60 ns/op (+/-   2.96, CI:[   13.34,   16.18])
     absl::flat_hash_set:       8.13 ns/op (+/-   0.40, CI:[    7.95,    8.34])
      llvm::SparseSet<8>:       7.80 ns/op (+/-   0.56, CI:[    7.30,    7.84])
     llvm::SparseSet<32>:       5.19 ns/op (+/-   0.62, CI:[    4.69,    5.29])
        entt::sparse_set:       1.14 ns/op (+/-   0.04, CI:[    1.12,    1.16])
                std::set:      58.51 ns/op (+/-   2.08, CI:[   57.83,   59.82])

  Erase:
     fat_p::SparseSet<8>:       4.78 ns/op (+/-   0.90, CI:[    4.12,    4.98])
    fat_p::SparseSet<32>:       1.27 ns/op (+/-   0.04, CI:[    1.26,    1.29])
          fat_p::FlatSet:      71.80 ns/op (+/-   4.14, CI:[   70.34,   74.30])
      std::unordered_set:      25.60 ns/op (+/-   1.31, CI:[   24.58,   25.84])
     absl::flat_hash_set:       7.89 ns/op (+/-   0.65, CI:[    7.65,    8.26])
      llvm::SparseSet<8>:       6.60 ns/op (+/-   1.30, CI:[    5.80,    7.04])
     llvm::SparseSet<32>:       2.09 ns/op (+/-   0.17, CI:[    2.10,    2.26])
        entt::sparse_set:       6.01 ns/op (+/-   0.24, CI:[    5.88,    6.11])
                std::set:      88.29 ns/op (+/-   7.43, CI:[   85.91,   93.02])

  Iteration:
     fat_p::SparseSet<8>:       0.28 ns/op (+/-   0.02, CI:[    0.28,    0.30])
    fat_p::SparseSet<32>:       0.13 ns/op (+/-   0.00, CI:[    0.13,    0.13])
          fat_p::FlatSet:       0.13 ns/op (+/-   0.01, CI:[    0.13,    0.14])
      std::unordered_set:       1.87 ns/op (+/-   0.06, CI:[    1.84,    1.90])
     absl::flat_hash_set:       7.57 ns/op (+/-   0.14, CI:[    7.49,    7.62])
      llvm::SparseSet<8>:       0.14 ns/op (+/-   0.01, CI:[    0.14,    0.14])
     llvm::SparseSet<32>:       0.13 ns/op (+/-   0.01, CI:[    0.13,    0.14])
        entt::sparse_set:       0.17 ns/op (+/-   0.00, CI:[    0.17,    0.17])
                std::set:       7.07 ns/op (+/-   0.29, CI:[    6.91,    7.18])

--- N = 10000 ---
[2026-02-16 03:37:48] CPU: 3491 MHz (max: 2800) [TURBO]
[Cooling: size transition] [Ready: 2800 MHz]

  Insert:
     fat_p::SparseSet<8>:       2.74 ns/op (+/-   0.34, CI:[    2.71,    3.04])
    fat_p::SparseSet<32>:       3.12 ns/op (+/-   0.09, CI:[    3.11,    3.20])
          fat_p::FlatSet:     158.47 ns/op (+/-   0.63, CI:[  158.32,  158.92])
      std::unordered_set:      33.77 ns/op (+/-   0.49, CI:[   33.63,   34.09])
     absl::flat_hash_set:      12.82 ns/op (+/-   1.28, CI:[   12.63,   13.86])
      llvm::SparseSet<8>:      10.63 ns/op (+/-   0.32, CI:[   10.62,   10.92])
     llvm::SparseSet<32>:       2.23 ns/op (+/-   0.21, CI:[    2.14,    2.33])
        entt::sparse_set:       5.68 ns/op (+/-   0.29, CI:[    5.60,    5.87])
                std::set:     147.61 ns/op (+/-   1.07, CI:[  147.09,  148.11])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.88 ns/op (+/-   0.14, CI:[    0.85,    0.98])
    fat_p::SparseSet<32>:       1.20 ns/op (+/-   0.05, CI:[    1.19,    1.24])
          fat_p::FlatSet:      67.49 ns/op (+/-   0.44, CI:[   67.31,   67.73])
      std::unordered_set:      22.42 ns/op (+/-   0.29, CI:[   22.40,   22.68])
     absl::flat_hash_set:       8.50 ns/op (+/-   0.14, CI:[    8.43,    8.56])
      llvm::SparseSet<8>:      20.00 ns/op (+/-   0.53, CI:[   19.78,   20.29])
     llvm::SparseSet<32>:       7.20 ns/op (+/-   0.59, CI:[    6.96,    7.53])
        entt::sparse_set:       3.32 ns/op (+/-   0.52, CI:[    2.99,    3.48])
                std::set:      98.45 ns/op (+/-   3.00, CI:[   97.89,  100.76])

  Erase:
     fat_p::SparseSet<8>:       1.81 ns/op (+/-   0.04, CI:[    1.79,    1.82])
    fat_p::SparseSet<32>:       3.12 ns/op (+/-   0.02, CI:[    3.11,    3.13])
          fat_p::FlatSet:     217.42 ns/op (+/-   0.53, CI:[  217.19,  217.70])
      std::unordered_set:      32.05 ns/op (+/-   0.34, CI:[   32.00,   32.33])
     absl::flat_hash_set:       9.47 ns/op (+/-   0.41, CI:[    9.32,    9.71])
      llvm::SparseSet<8>:      18.38 ns/op (+/-   0.32, CI:[   18.29,   18.60])
     llvm::SparseSet<32>:       2.85 ns/op (+/-   0.17, CI:[    2.83,    3.00])
        entt::sparse_set:       7.23 ns/op (+/-   0.44, CI:[    7.03,    7.46])
                std::set:     143.42 ns/op (+/-   2.04, CI:[  142.21,  144.15])

  Iteration:
     fat_p::SparseSet<8>:       0.97 ns/op (+/-   0.55, CI:[    0.79,    1.32])
    fat_p::SparseSet<32>:       0.13 ns/op (+/-   0.01, CI:[    0.13,    0.14])
          fat_p::FlatSet:       0.13 ns/op (+/-   0.02, CI:[    0.12,    0.14])
      std::unordered_set:       4.10 ns/op (+/-   0.12, CI:[    4.07,    4.18])
     absl::flat_hash_set:       5.94 ns/op (+/-   0.25, CI:[    5.92,    6.16])
      llvm::SparseSet<8>:       0.13 ns/op (+/-   0.11, CI:[    0.11,    0.22])
     llvm::SparseSet<32>:       0.13 ns/op (+/-   0.01, CI:[    0.12,    0.13])
        entt::sparse_set:       0.16 ns/op (+/-   0.11, CI:[    0.13,    0.24])
                std::set:      11.08 ns/op (+/-   0.39, CI:[   11.02,   11.39])

--- N = 100000 ---
[2026-02-16 03:37:49] CPU: 3491 MHz (max: 2800) [TURBO]
[Cooling: size transition] [Ready: 2800 MHz]

  Insert:
     fat_p::SparseSet<8>:       2.51 ns/op (+/-   0.09, CI:[    2.52,    2.60])
    fat_p::SparseSet<32>:       7.85 ns/op (+/-   0.76, CI:[    7.78,    8.51])
          fat_p::FlatSet:    1567.52 ns/op (+/-   1.17, CI:[ 1566.88, 1568.00])
      std::unordered_set:      54.57 ns/op (+/-   5.00, CI:[   53.76,   58.54])
     absl::flat_hash_set:      14.44 ns/op (+/-   0.39, CI:[   14.30,   14.67])
      llvm::SparseSet<8>:     147.78 ns/op (+/-   1.78, CI:[  147.53,  149.23])
     llvm::SparseSet<32>:       4.06 ns/op (+/-   0.38, CI:[    3.90,    4.26])
        entt::sparse_set:      12.12 ns/op (+/-   0.84, CI:[   11.78,   12.58])
                std::set:     289.92 ns/op (+/-  17.42, CI:[  281.30,  297.97])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.90 ns/op (+/-   0.04, CI:[    0.89,    0.92])
    fat_p::SparseSet<32>:       5.01 ns/op (+/-   0.71, CI:[    4.48,    5.16])
          fat_p::FlatSet:      90.60 ns/op (+/-   0.64, CI:[   90.09,   90.70])
      std::unordered_set:      33.44 ns/op (+/-   0.82, CI:[   33.23,   34.01])
     absl::flat_hash_set:      11.05 ns/op (+/-   0.08, CI:[   11.03,   11.10])
      llvm::SparseSet<8>:     176.41 ns/op (+/-   3.73, CI:[  175.57,  179.14])
     llvm::SparseSet<32>:       9.31 ns/op (+/-   0.21, CI:[    9.28,    9.48])
        entt::sparse_set:       6.14 ns/op (+/-   0.62, CI:[    5.70,    6.29])
                std::set:     256.05 ns/op (+/-  17.91, CI:[  246.88,  264.02])

  Erase:
     fat_p::SparseSet<8>:       1.00 ns/op (+/-   0.29, CI:[    0.92,    1.20])
    fat_p::SparseSet<32>:       8.65 ns/op (+/-   0.20, CI:[    8.56,    8.76])
          fat_p::FlatSet:    2371.91 ns/op (+/-  38.66, CI:[ 2365.21, 2402.21])
      std::unordered_set:      60.12 ns/op (+/-   5.93, CI:[   59.25,   64.92])
     absl::flat_hash_set:      14.16 ns/op (+/-   1.00, CI:[   14.01,   14.97])
      llvm::SparseSet<8>:      93.40 ns/op (+/-   6.85, CI:[   92.43,   98.99])
     llvm::SparseSet<32>:       8.18 ns/op (+/-   1.30, CI:[    7.94,    9.18])
        entt::sparse_set:      19.16 ns/op (+/-   2.34, CI:[   17.42,   19.66])
                std::set:     282.55 ns/op (+/-  13.16, CI:[  274.25,  286.85])

  Iteration:
     fat_p::SparseSet<8>:       1.48 ns/op (+/-   0.42, CI:[    1.33,    1.72])
    fat_p::SparseSet<32>:       0.16 ns/op (+/-   0.01, CI:[    0.16,    0.16])
          fat_p::FlatSet:       0.12 ns/op (+/-   0.00, CI:[    0.12,    0.12])
      std::unordered_set:       8.73 ns/op (+/-   2.11, CI:[    8.31,   10.33])
     absl::flat_hash_set:       3.79 ns/op (+/-   0.03, CI:[    3.80,    3.83])
      llvm::SparseSet<8>:       0.49 ns/op (+/-   0.02, CI:[    0.49,    0.50])
     llvm::SparseSet<32>:       0.50 ns/op (+/-   0.01, CI:[    0.50,    0.50])
        entt::sparse_set:       0.66 ns/op (+/-   0.02, CI:[    0.66,    0.67])
                std::set:      34.89 ns/op (+/-   2.90, CI:[   34.66,   37.44])
[Cooling: before iteration benchmark] [Ready: 2800 MHz]

================================================================================
  SECTION 2: Dense Iteration (SparseSet Key Advantage)
================================================================================

[2026-02-16 03:38:21] Section start CPU: 2800 MHz (max: 2800)
Contract Note: Dense iteration—SparseSet/FlatSet expected to outperform hash sets

--- N = 10000 ---
[2026-02-16 03:38:21] CPU: 2800 MHz (max: 2800)
[Cooling: size transition] [Ready: 2800 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.42 ns/op (+/-   0.11, CI:[    0.37,    0.47])
    fat_p::SparseSet<32>:       0.11 ns/op (+/-   0.00, CI:[    0.11,    0.12])
          fat_p::FlatSet:       0.11 ns/op (+/-   0.00, CI:[    0.11,    0.11])
      std::unordered_set:       4.05 ns/op (+/-   0.16, CI:[    4.03,    4.19])
     absl::flat_hash_set:       5.93 ns/op (+/-   0.05, CI:[    5.90,    5.95])
      llvm::SparseSet<8>:       0.11 ns/op (+/-   0.00, CI:[    0.11,    0.12])
     llvm::SparseSet<32>:       0.11 ns/op (+/-   0.08, CI:[    0.09,    0.17])
        entt::sparse_set:       0.15 ns/op (+/-   0.11, CI:[    0.12,    0.23])
                std::set:      10.09 ns/op (+/-   0.23, CI:[   10.06,   10.27])

--- N = 100000 ---
[2026-02-16 03:38:21] CPU: 2800 MHz (max: 2800)
[Cooling: size transition] [Ready: 2800 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       1.96 ns/op (+/-   0.57, CI:[    1.73,    2.27])
    fat_p::SparseSet<32>:       0.15 ns/op (+/-   0.02, CI:[    0.15,    0.16])
          fat_p::FlatSet:       0.12 ns/op (+/-   0.02, CI:[    0.11,    0.13])
      std::unordered_set:       8.56 ns/op (+/-   1.77, CI:[    8.21,    9.91])
     absl::flat_hash_set:       3.80 ns/op (+/-   0.03, CI:[    3.79,    3.83])
      llvm::SparseSet<8>:       0.49 ns/op (+/-   0.01, CI:[    0.49,    0.50])
     llvm::SparseSet<32>:       0.50 ns/op (+/-   0.08, CI:[    0.45,    0.52])
        entt::sparse_set:       0.66 ns/op (+/-   0.02, CI:[    0.65,    0.67])
                std::set:      32.74 ns/op (+/-   3.03, CI:[   31.91,   34.81])

[Cooling: before mixed workload] [Ready: 2800 MHz]

================================================================================
  SECTION 3: Mixed Workload (Insert/Erase Churn)
================================================================================

[2026-02-16 03:38:28] Section start CPU: 2800 MHz (max: 2800)
Contract Note: Random insert/erase churn—tests swap-with-back erase efficiency

--- N = 10000 (50% insert/erase cycles) ---
[2026-02-16 03:38:28] CPU: 2800 MHz (max: 2800)
  Mixed Workload:
     fat_p::SparseSet<8>:       2.86 ns/op (+/-   0.17, CI:[    2.82,    2.98])
    fat_p::SparseSet<32>:       3.72 ns/op (+/-   0.08, CI:[    3.69,    3.77])
          fat_p::FlatSet:     260.92 ns/op (+/-   2.41, CI:[  260.47,  262.77])
      std::unordered_set:      29.00 ns/op (+/-   0.60, CI:[   28.89,   29.47])
     absl::flat_hash_set:      12.05 ns/op (+/-   0.19, CI:[   12.00,   12.18])
      llvm::SparseSet<8>:      27.68 ns/op (+/-   0.59, CI:[   27.53,   28.10])
     llvm::SparseSet<32>:       2.82 ns/op (+/-   0.16, CI:[    2.78,    2.94])
        entt::sparse_set:       5.32 ns/op (+/-   0.22, CI:[    5.21,    5.42])
                std::set:     120.74 ns/op (+/-   1.59, CI:[  120.70,  122.22])

================================================================================
  Feature Comparison Summary
================================================================================

  Container                 Insert    Contains  Erase     Iteration  Order
  --------------------------------------------------------------------------
  fat_p::SparseSet<8>       O(1)*     O(1)      O(1)      Dense      Unstable  (max 256)
  fat_p::SparseSet<32>      O(1)*     O(1)      O(1)      Dense      Unstable  (handles large N)
  fat_p::FlatSet            O(n)      O(log n)  O(n)      Dense      Sorted
  std::unordered_set        O(1)*     O(1)*     O(1)*     Scattered  Unordered
  absl::flat_hash_set       O(1)*     O(1)*     O(1)*     Scattered  Unordered
  llvm::SparseSet<8>        O(1)*     O(1)      O(1)      Dense      Unstable  (default, max 256)
  llvm::SparseSet<32>       O(1)*     O(1)      O(1)      Dense      Unstable  (configured)
  entt::sparse_set          O(1)*     O(1)      O(1)      Dense      Unstable
  std::set                  O(log n)  O(log n)  O(log n)  In-order   Sorted

  * = amortized
  Note: <8> variants use uint8_t value type (max 256 elements)
        <32> variants use uint32_t value type (handles large N)

  When to use SparseSet:
    - Integer keys in a bounded range
    - Frequent insert/erase churn
    - Iteration performance matters
    - Order doesn't matter

  When to use FlatSet:
    - Need sorted order
    - Mostly lookups after initial build
    - Binary search semantics (lower_bound, etc.)

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
[2026-02-16 04:11:17] Section start CPU: 3397 MHz (max: 2800) [TURBO]
Contract Note: O(1) insert/erase/contains; dense iteration; unstable erase order
              Insert excludes allocation (reserve performed in setup)
--- N = 1000 ---
[2026-02-16 04:11:17] CPU: 3397 MHz (max: 2800) [TURBO]
[Cooling: size transition] [Ready: 2013 MHz]

  Insert:
     fat_p::SparseSet<8>:       4.40 ns/op (+/-   0.16, CI:[    4.31,    4.47])
    fat_p::SparseSet<32>:       2.97 ns/op (+/-   0.13, CI:[    2.99,    3.11])
          fat_p::FlatSet:      64.06 ns/op (+/-   3.01, CI:[   62.69,   65.57])
      std::unordered_set:      30.71 ns/op (+/-   1.78, CI:[   30.44,   32.15])
     absl::flat_hash_set:      14.38 ns/op (+/-   0.47, CI:[   14.20,   14.65])
      llvm::SparseSet<8>:       1.94 ns/op (+/-   0.06, CI:[    1.92,    1.98])
     llvm::SparseSet<32>:       2.12 ns/op (+/-   0.07, CI:[    2.11,    2.18])
        entt::sparse_set:       4.98 ns/op (+/-   0.15, CI:[    4.96,    5.11])
                std::set:      83.58 ns/op (+/-   3.94, CI:[   82.12,   85.88])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.90 ns/op (+/-   0.01, CI:[    0.90,    0.90])
    fat_p::SparseSet<32>:       3.91 ns/op (+/-   0.02, CI:[    3.90,    3.92])
          fat_p::FlatSet:      52.30 ns/op (+/-   1.33, CI:[   51.60,   52.87])
      std::unordered_set:      13.11 ns/op (+/-   2.30, CI:[   11.96,   14.16])
     absl::flat_hash_set:       8.01 ns/op (+/-   0.39, CI:[    7.83,    8.20])
      llvm::SparseSet<8>:       7.53 ns/op (+/-   1.60, CI:[    6.99,    8.52])
     llvm::SparseSet<32>:       4.03 ns/op (+/-   0.02, CI:[    4.02,    4.04])
        entt::sparse_set:       3.88 ns/op (+/-   0.02, CI:[    3.87,    3.88])
                std::set:      30.84 ns/op (+/-   3.29, CI:[   30.13,   33.28])

  Erase:
     fat_p::SparseSet<8>:       3.64 ns/op (+/-   0.72, CI:[    3.37,    4.05])
    fat_p::SparseSet<32>:       1.25 ns/op (+/-   0.11, CI:[    1.25,    1.35])
          fat_p::FlatSet:      67.79 ns/op (+/-   1.62, CI:[   67.03,   68.58])
      std::unordered_set:      25.26 ns/op (+/-   1.37, CI:[   24.60,   25.92])
     absl::flat_hash_set:       6.57 ns/op (+/-   0.40, CI:[    6.39,    6.77])
      llvm::SparseSet<8>:       5.62 ns/op (+/-   0.83, CI:[    5.10,    5.89])
     llvm::SparseSet<32>:       2.34 ns/op (+/-   0.13, CI:[    2.24,    2.36])
        entt::sparse_set:       5.24 ns/op (+/-   0.19, CI:[    5.16,    5.34])
                std::set:      93.03 ns/op (+/-   4.72, CI:[   91.37,   95.89])

  Iteration:
     fat_p::SparseSet<8>:       0.22 ns/op (+/-   0.02, CI:[    0.21,    0.23])
    fat_p::SparseSet<32>:       0.10 ns/op (+/-   0.01, CI:[    0.10,    0.11])
          fat_p::FlatSet:       0.10 ns/op (+/-   0.01, CI:[    0.10,    0.10])
      std::unordered_set:       1.90 ns/op (+/-   0.06, CI:[    1.86,    1.92])
     absl::flat_hash_set:       7.36 ns/op (+/-   0.20, CI:[    7.23,    7.42])
      llvm::SparseSet<8>:       0.10 ns/op (+/-   0.01, CI:[    0.10,    0.10])
     llvm::SparseSet<32>:       0.10 ns/op (+/-   0.01, CI:[    0.10,    0.11])
        entt::sparse_set:       0.12 ns/op (+/-   0.01, CI:[    0.12,    0.13])
                std::set:       7.52 ns/op (+/-   0.65, CI:[    7.22,    7.83])

--- N = 10000 ---
[2026-02-16 04:11:18] CPU: 2800 MHz (max: 2800)
[Cooling: size transition] [Ready: 2800 MHz]

  Insert:
     fat_p::SparseSet<8>:       2.54 ns/op (+/-   0.16, CI:[    2.58,    2.74])
    fat_p::SparseSet<32>:       3.10 ns/op (+/-   0.11, CI:[    3.04,    3.14])
          fat_p::FlatSet:     159.90 ns/op (+/-   2.03, CI:[  159.52,  161.46])
      std::unordered_set:      33.70 ns/op (+/-   0.75, CI:[   33.61,   34.33])
     absl::flat_hash_set:      10.09 ns/op (+/-   1.12, CI:[    9.83,   10.90])
      llvm::SparseSet<8>:       8.02 ns/op (+/-   0.27, CI:[    7.96,    8.22])
     llvm::SparseSet<32>:       2.20 ns/op (+/-   0.01, CI:[    2.19,    2.20])
        entt::sparse_set:       5.98 ns/op (+/-   0.20, CI:[    5.91,    6.10])
                std::set:     123.50 ns/op (+/-   4.34, CI:[  122.32,  126.48])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.87 ns/op (+/-   0.00, CI:[    0.87,    0.87])
    fat_p::SparseSet<32>:       4.24 ns/op (+/-   0.42, CI:[    4.06,    4.47])
          fat_p::FlatSet:      69.04 ns/op (+/-   1.38, CI:[   68.70,   70.02])
      std::unordered_set:      22.56 ns/op (+/-   0.36, CI:[   22.54,   22.89])
     absl::flat_hash_set:       8.10 ns/op (+/-   0.13, CI:[    8.06,    8.18])
      llvm::SparseSet<8>:      22.35 ns/op (+/-   0.17, CI:[   22.35,   22.51])
     llvm::SparseSet<32>:       4.04 ns/op (+/-   0.41, CI:[    4.12,    4.51])
        entt::sparse_set:       4.01 ns/op (+/-   0.33, CI:[    4.05,    4.36])
                std::set:      49.26 ns/op (+/-   0.68, CI:[   49.04,   49.69])

  Erase:
     fat_p::SparseSet<8>:       1.72 ns/op (+/-   0.04, CI:[    1.69,    1.73])
    fat_p::SparseSet<32>:       3.01 ns/op (+/-   1.89, CI:[    2.53,    4.33])
          fat_p::FlatSet:     208.60 ns/op (+/-   1.47, CI:[  208.37,  209.78])
      std::unordered_set:      33.96 ns/op (+/-   0.59, CI:[   33.79,   34.35])
     absl::flat_hash_set:       8.59 ns/op (+/-   0.33, CI:[    8.47,    8.78])
      llvm::SparseSet<8>:      17.87 ns/op (+/-   0.85, CI:[   17.76,   18.57])
     llvm::SparseSet<32>:       2.73 ns/op (+/-   0.03, CI:[    2.72,    2.74])
        entt::sparse_set:       6.45 ns/op (+/-   0.37, CI:[    6.35,    6.71])
                std::set:     147.78 ns/op (+/-   4.70, CI:[  146.34,  150.84])

  Iteration:
     fat_p::SparseSet<8>:       0.33 ns/op (+/-   0.09, CI:[    0.32,    0.41])
    fat_p::SparseSet<32>:       0.08 ns/op (+/-   0.06, CI:[    0.07,    0.12])
          fat_p::FlatSet:       0.08 ns/op (+/-   0.00, CI:[    0.08,    0.08])
      std::unordered_set:       4.09 ns/op (+/-   0.01, CI:[    4.08,    4.10])
     absl::flat_hash_set:       5.50 ns/op (+/-   0.28, CI:[    5.46,    5.72])
      llvm::SparseSet<8>:       0.08 ns/op (+/-   0.08, CI:[    0.07,    0.14])
     llvm::SparseSet<32>:       0.08 ns/op (+/-   0.08, CI:[    0.07,    0.14])
        entt::sparse_set:       0.10 ns/op (+/-   0.08, CI:[    0.09,    0.17])
                std::set:      11.34 ns/op (+/-   0.22, CI:[   11.27,   11.48])

--- N = 100000 ---
[2026-02-16 04:11:19] CPU: 3492 MHz (max: 2800) [TURBO]
[Cooling: size transition] [Ready: 2800 MHz]

  Insert:
     fat_p::SparseSet<8>:       2.58 ns/op (+/-   0.15, CI:[    2.44,    2.59])
    fat_p::SparseSet<32>:       6.67 ns/op (+/-   0.55, CI:[    6.55,    7.08])
          fat_p::FlatSet:    1567.53 ns/op (+/-   6.13, CI:[ 1566.26, 1572.14])
      std::unordered_set:      50.00 ns/op (+/-   4.36, CI:[   49.31,   53.48])
     absl::flat_hash_set:      10.57 ns/op (+/-   0.19, CI:[   10.51,   10.69])
      llvm::SparseSet<8>:     141.01 ns/op (+/-  12.57, CI:[  137.79,  149.82])
     llvm::SparseSet<32>:       3.55 ns/op (+/-   0.34, CI:[    3.39,    3.72])
        entt::sparse_set:      11.48 ns/op (+/-   0.44, CI:[   11.32,   11.74])
                std::set:     204.35 ns/op (+/-   1.70, CI:[  204.09,  205.72])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.87 ns/op (+/-   0.03, CI:[    0.87,    0.90])
    fat_p::SparseSet<32>:       4.44 ns/op (+/-   0.21, CI:[    4.37,    4.57])
          fat_p::FlatSet:      91.54 ns/op (+/-   0.20, CI:[   91.42,   91.61])
      std::unordered_set:      32.50 ns/op (+/-   0.43, CI:[   32.26,   32.67])
     absl::flat_hash_set:      10.56 ns/op (+/-   0.09, CI:[   10.53,   10.62])
      llvm::SparseSet<8>:     182.45 ns/op (+/-   1.54, CI:[  181.99,  183.46])
     llvm::SparseSet<32>:       5.85 ns/op (+/-   0.16, CI:[    5.79,    5.94])
        entt::sparse_set:       8.85 ns/op (+/-   0.63, CI:[    8.39,    8.99])
                std::set:      96.43 ns/op (+/-   2.47, CI:[   95.92,   98.28])

  Erase:
     fat_p::SparseSet<8>:       0.99 ns/op (+/-   0.10, CI:[    0.98,    1.08])
    fat_p::SparseSet<32>:       7.77 ns/op (+/-   0.19, CI:[    7.72,    7.90])
          fat_p::FlatSet:    2354.54 ns/op (+/-  15.39, CI:[ 2352.74, 2367.47])
      std::unordered_set:      54.94 ns/op (+/-   0.83, CI:[   54.94,   55.73])
     absl::flat_hash_set:      14.79 ns/op (+/-   0.40, CI:[   14.63,   15.02])
      llvm::SparseSet<8>:      88.92 ns/op (+/-   3.68, CI:[   88.15,   91.67])
     llvm::SparseSet<32>:       7.47 ns/op (+/-   0.57, CI:[    7.42,    7.96])
        entt::sparse_set:      15.24 ns/op (+/-   1.73, CI:[   14.47,   16.13])
                std::set:     263.09 ns/op (+/-   4.71, CI:[  262.26,  266.76])

  Iteration:
     fat_p::SparseSet<8>:       0.93 ns/op (+/-   0.36, CI:[    0.83,    1.17])
    fat_p::SparseSet<32>:       0.13 ns/op (+/-   0.01, CI:[    0.13,    0.13])
          fat_p::FlatSet:       0.08 ns/op (+/-   0.00, CI:[    0.08,    0.08])
      std::unordered_set:       7.39 ns/op (+/-   1.91, CI:[    7.13,    8.96])
     absl::flat_hash_set:       3.16 ns/op (+/-   0.07, CI:[    3.13,    3.19])
      llvm::SparseSet<8>:       0.32 ns/op (+/-   0.00, CI:[    0.32,    0.33])
     llvm::SparseSet<32>:       0.34 ns/op (+/-   0.07, CI:[    0.29,    0.36])
        entt::sparse_set:       0.39 ns/op (+/-   0.01, CI:[    0.38,    0.39])
                std::set:      29.36 ns/op (+/-   1.43, CI:[   28.94,   30.31])
[Cooling: before iteration benchmark] [Ready: 3486 MHz]

================================================================================
  SECTION 2: Dense Iteration (SparseSet Key Advantage)
================================================================================

[2026-02-16 04:11:49] Section start CPU: 3486 MHz (max: 2800) [TURBO]
Contract Note: Dense iteration—SparseSet/FlatSet expected to outperform hash sets

--- N = 10000 ---
[2026-02-16 04:11:49] CPU: 3486 MHz (max: 2800) [TURBO]
[Cooling: size transition] [Ready: 2800 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.34 ns/op (+/-   0.07, CI:[    0.30,    0.37])
    fat_p::SparseSet<32>:       0.08 ns/op (+/-   0.06, CI:[    0.07,    0.12])
          fat_p::FlatSet:       0.08 ns/op (+/-   0.00, CI:[    0.08,    0.08])
      std::unordered_set:       4.09 ns/op (+/-   0.19, CI:[    4.06,    4.24])
     absl::flat_hash_set:       5.49 ns/op (+/-   0.06, CI:[    5.46,    5.52])
      llvm::SparseSet<8>:       0.08 ns/op (+/-   0.06, CI:[    0.06,    0.12])
     llvm::SparseSet<32>:       0.09 ns/op (+/-   0.06, CI:[    0.07,    0.13])
        entt::sparse_set:       0.10 ns/op (+/-   0.06, CI:[    0.09,    0.14])
                std::set:      11.40 ns/op (+/-   0.28, CI:[   11.34,   11.61])

--- N = 100000 ---
[2026-02-16 04:11:49] CPU: 2800 MHz (max: 2800)
[Cooling: size transition] [Ready: 3412 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.93 ns/op (+/-   0.39, CI:[    0.83,    1.20])
    fat_p::SparseSet<32>:       0.13 ns/op (+/-   0.01, CI:[    0.13,    0.14])
          fat_p::FlatSet:       0.08 ns/op (+/-   0.00, CI:[    0.08,    0.08])
      std::unordered_set:       7.68 ns/op (+/-   1.79, CI:[    7.38,    9.09])
     absl::flat_hash_set:       3.17 ns/op (+/-   0.06, CI:[    3.12,    3.18])
      llvm::SparseSet<8>:       0.32 ns/op (+/-   0.00, CI:[    0.32,    0.32])
     llvm::SparseSet<32>:       0.34 ns/op (+/-   0.06, CI:[    0.31,    0.36])
        entt::sparse_set:       0.39 ns/op (+/-   0.01, CI:[    0.38,    0.39])
                std::set:      28.51 ns/op (+/-   0.73, CI:[   28.29,   28.99])

[Cooling: before mixed workload] [Ready: 2800 MHz]

================================================================================
  SECTION 3: Mixed Workload (Insert/Erase Churn)
================================================================================

[2026-02-16 04:11:56] Section start CPU: 2800 MHz (max: 2800)
Contract Note: Random insert/erase churn—tests swap-with-back erase efficiency

--- N = 10000 (50% insert/erase cycles) ---
[2026-02-16 04:11:56] CPU: 2800 MHz (max: 2800)
  Mixed Workload:
     fat_p::SparseSet<8>:       2.85 ns/op (+/-   0.04, CI:[    2.84,    2.88])
    fat_p::SparseSet<32>:       3.75 ns/op (+/-   0.17, CI:[    3.71,    3.87])
          fat_p::FlatSet:     260.32 ns/op (+/-   0.71, CI:[  260.01,  260.69])
      std::unordered_set:      29.13 ns/op (+/-   0.55, CI:[   29.05,   29.58])
     absl::flat_hash_set:      10.00 ns/op (+/-   0.30, CI:[    9.94,   10.22])
      llvm::SparseSet<8>:      25.07 ns/op (+/-   0.41, CI:[   25.06,   25.45])
     llvm::SparseSet<32>:       2.87 ns/op (+/-   0.10, CI:[    2.84,    2.94])
        entt::sparse_set:       6.73 ns/op (+/-   0.36, CI:[    6.65,    6.99])
                std::set:     109.85 ns/op (+/-   0.79, CI:[  109.62,  110.38])

================================================================================
  Feature Comparison Summary
================================================================================

  Container                 Insert    Contains  Erase     Iteration  Order
  --------------------------------------------------------------------------
  fat_p::SparseSet<8>       O(1)*     O(1)      O(1)      Dense      Unstable  (max 256)
  fat_p::SparseSet<32>      O(1)*     O(1)      O(1)      Dense      Unstable  (handles large N)
  fat_p::FlatSet            O(n)      O(log n)  O(n)      Dense      Sorted
  std::unordered_set        O(1)*     O(1)*     O(1)*     Scattered  Unordered
  absl::flat_hash_set       O(1)*     O(1)*     O(1)*     Scattered  Unordered
  llvm::SparseSet<8>        O(1)*     O(1)      O(1)      Dense      Unstable  (default, max 256)
  llvm::SparseSet<32>       O(1)*     O(1)      O(1)      Dense      Unstable  (configured)
  entt::sparse_set          O(1)*     O(1)      O(1)      Dense      Unstable
  std::set                  O(log n)  O(log n)  O(log n)  In-order   Sorted

  * = amortized
  Note: <8> variants use uint8_t value type (max 256 elements)
        <32> variants use uint32_t value type (handles large N)

  When to use SparseSet:
    - Integer keys in a bounded range
    - Frequent insert/erase churn
    - Iteration performance matters
    - Order doesn't matter

  When to use FlatSet:
    - Need sorted order
    - Mostly lookups after initial build
    - Binary search semantics (lower_bound, etc.)

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
[2026-02-16 04:55:01] Section start CPU: 2596 MHz (base: 2596)
Contract Note: O(1) insert/erase/contains; dense iteration; unstable erase order
              Insert excludes allocation (reserve performed in setup)
--- N = 1000 ---
[2026-02-16 04:55:01] CPU: 2596 MHz (base: 2596)
[Cooling: size transition] [Ready: 2596 MHz]

  Insert:
     fat_p::SparseSet<8>:       3.20 ns/op (+/-   0.36, CI:[    2.96,    3.30])
    fat_p::SparseSet<32>:       2.40 ns/op (+/-   0.07, CI:[    2.34,    2.41])
          fat_p::FlatSet:      64.75 ns/op (+/-  16.69, CI:[   60.18,   76.15])
      std::unordered_set:      59.10 ns/op (+/-   5.60, CI:[   58.39,   63.75])
     absl::flat_hash_set:      50.50 ns/op (+/-   6.24, CI:[   49.11,   55.08])
        entt::sparse_set:       7.80 ns/op (+/-   0.06, CI:[    7.81,    7.86])
                std::set:     123.65 ns/op (+/-   9.63, CI:[  116.74,  125.96])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.90 ns/op (+/-   0.04, CI:[    0.90,    0.94])
    fat_p::SparseSet<32>:       2.95 ns/op (+/-   1.33, CI:[    2.60,    3.88])
          fat_p::FlatSet:      27.90 ns/op (+/-   9.57, CI:[   25.22,   34.38])
      std::unordered_set:       9.20 ns/op (+/-   3.07, CI:[    7.91,   10.85])
     absl::flat_hash_set:      11.60 ns/op (+/-   1.21, CI:[   11.31,   12.48])
        entt::sparse_set:       3.15 ns/op (+/-   1.16, CI:[    2.83,    3.95])
                std::set:      38.70 ns/op (+/-  13.09, CI:[   35.76,   48.29])

  Erase:
     fat_p::SparseSet<8>:       3.20 ns/op (+/-   1.18, CI:[    2.72,    3.86])
    fat_p::SparseSet<32>:       2.40 ns/op (+/-   0.13, CI:[    2.34,    2.46])
          fat_p::FlatSet:      43.30 ns/op (+/-  12.35, CI:[   41.97,   53.79])
      std::unordered_set:      30.30 ns/op (+/-   2.07, CI:[   28.87,   30.85])
     absl::flat_hash_set:      10.40 ns/op (+/-   2.90, CI:[    9.79,   12.57])
        entt::sparse_set:       9.40 ns/op (+/-   0.09, CI:[    9.34,    9.42])
                std::set:     100.90 ns/op (+/-  19.55, CI:[   91.08,  109.80])

  Iteration:
     fat_p::SparseSet<8>:       0.80 ns/op (+/-   0.20, CI:[    0.52,    0.72])
    fat_p::SparseSet<32>:       0.50 ns/op (+/-   0.07, CI:[    0.45,    0.51])
          fat_p::FlatSet:       0.50 ns/op (+/-   0.07, CI:[    0.45,    0.52])
      std::unordered_set:       2.30 ns/op (+/-   0.08, CI:[    2.31,    2.38])
     absl::flat_hash_set:       7.70 ns/op (+/-   3.17, CI:[    6.82,    9.86])
        entt::sparse_set:       0.50 ns/op (+/-   0.06, CI:[    0.49,    0.54])
                std::set:       5.80 ns/op (+/-   0.29, CI:[    5.79,    6.07])

--- N = 10000 ---
[2026-02-16 04:55:02] CPU: 2596 MHz (base: 2596)
[Cooling: size transition] [Ready: 2596 MHz]

  Insert:
     fat_p::SparseSet<8>:       1.64 ns/op (+/-   0.04, CI:[    1.61,    1.65])
    fat_p::SparseSet<32>:       2.48 ns/op (+/-   0.57, CI:[    2.40,    2.95])
          fat_p::FlatSet:     293.15 ns/op (+/-   2.24, CI:[  292.43,  294.57])
      std::unordered_set:      70.45 ns/op (+/-   2.04, CI:[   69.92,   71.87])
     absl::flat_hash_set:      50.59 ns/op (+/-   1.59, CI:[   49.97,   51.49])
        entt::sparse_set:       8.10 ns/op (+/-   0.45, CI:[    8.07,    8.50])
                std::set:     206.15 ns/op (+/-   2.25, CI:[  205.17,  207.33])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.73 ns/op (+/-   0.01, CI:[    0.72,    0.73])
    fat_p::SparseSet<32>:       6.77 ns/op (+/-   1.38, CI:[    6.35,    7.67])
          fat_p::FlatSet:      77.52 ns/op (+/-   0.90, CI:[   77.44,   78.31])
      std::unordered_set:      16.18 ns/op (+/-   0.93, CI:[   16.04,   16.93])
     absl::flat_hash_set:      12.98 ns/op (+/-   0.85, CI:[   12.90,   13.71])
        entt::sparse_set:       7.66 ns/op (+/-   0.52, CI:[    7.40,    7.90])
                std::set:     110.09 ns/op (+/-   1.66, CI:[  109.51,  111.09])

  Erase:
     fat_p::SparseSet<8>:       1.92 ns/op (+/-   0.13, CI:[    1.80,    1.93])
    fat_p::SparseSet<32>:       2.92 ns/op (+/-   0.14, CI:[    2.89,    3.03])
          fat_p::FlatSet:     256.54 ns/op (+/-  23.14, CI:[  250.48,  272.63])
      std::unordered_set:      35.03 ns/op (+/-   1.86, CI:[   34.84,   36.61])
     absl::flat_hash_set:      12.24 ns/op (+/-   1.35, CI:[   11.95,   13.24])
        entt::sparse_set:      11.15 ns/op (+/-   0.51, CI:[   11.04,   11.52])
                std::set:     238.69 ns/op (+/-   3.86, CI:[  235.76,  239.46])

  Iteration:
     fat_p::SparseSet<8>:       0.78 ns/op (+/-   0.25, CI:[    0.66,    0.90])
    fat_p::SparseSet<32>:       0.46 ns/op (+/-   0.04, CI:[    0.43,    0.47])
          fat_p::FlatSet:       0.45 ns/op (+/-   0.06, CI:[    0.42,    0.47])
      std::unordered_set:       3.69 ns/op (+/-   0.30, CI:[    3.62,    3.91])
     absl::flat_hash_set:       5.62 ns/op (+/-   0.47, CI:[    5.49,    5.94])
        entt::sparse_set:       0.44 ns/op (+/-   0.01, CI:[    0.44,    0.45])
                std::set:      12.19 ns/op (+/-   0.90, CI:[   11.97,   12.84])

--- N = 100000 ---
[2026-02-16 04:55:03] CPU: 2596 MHz (base: 2596)
[Cooling: size transition] [Ready: 2596 MHz]

  Insert:
     fat_p::SparseSet<8>:       1.42 ns/op (+/-   0.04, CI:[    1.41,    1.45])
    fat_p::SparseSet<32>:       4.34 ns/op (+/-   0.33, CI:[    4.13,    4.45])
          fat_p::FlatSet:    2332.17 ns/op (+/-   5.77, CI:[ 2329.90, 2335.42])
      std::unordered_set:      89.68 ns/op (+/-   4.22, CI:[   88.99,   93.03])
     absl::flat_hash_set:      48.42 ns/op (+/-   1.43, CI:[   47.54,   48.91])
        entt::sparse_set:       9.93 ns/op (+/-   0.30, CI:[    9.81,   10.10])
                std::set:     302.61 ns/op (+/-  10.76, CI:[  301.84,  312.14])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.72 ns/op (+/-   0.01, CI:[    0.72,    0.72])
    fat_p::SparseSet<32>:       8.43 ns/op (+/-   0.23, CI:[    8.39,    8.61])
          fat_p::FlatSet:     106.59 ns/op (+/-   0.22, CI:[  106.48,  106.69])
      std::unordered_set:      22.72 ns/op (+/-   0.73, CI:[   22.53,   23.24])
     absl::flat_hash_set:      15.87 ns/op (+/-   1.11, CI:[   15.59,   16.65])
        entt::sparse_set:      10.19 ns/op (+/-   1.06, CI:[    9.95,   10.97])
                std::set:     219.51 ns/op (+/-   4.11, CI:[  218.43,  222.36])

  Erase:
     fat_p::SparseSet<8>:       1.16 ns/op (+/-   0.08, CI:[    1.12,    1.19])
    fat_p::SparseSet<32>:       5.34 ns/op (+/-   0.35, CI:[    5.24,    5.58])
          fat_p::FlatSet:    1931.67 ns/op (+/-  29.73, CI:[ 1926.36, 1954.81])
      std::unordered_set:      54.04 ns/op (+/-   1.54, CI:[   53.49,   54.97])
     absl::flat_hash_set:      18.26 ns/op (+/-   0.40, CI:[   18.22,   18.60])
        entt::sparse_set:      22.02 ns/op (+/-   2.38, CI:[   21.39,   23.67])
                std::set:     403.16 ns/op (+/-   3.88, CI:[  401.01,  404.73])

  Iteration:
     fat_p::SparseSet<8>:       1.17 ns/op (+/-   0.32, CI:[    1.00,    1.31])
    fat_p::SparseSet<32>:       0.43 ns/op (+/-   0.00, CI:[    0.42,    0.43])
          fat_p::FlatSet:       0.43 ns/op (+/-   0.01, CI:[    0.42,    0.43])
      std::unordered_set:       9.09 ns/op (+/-   0.84, CI:[    8.91,    9.72])
     absl::flat_hash_set:       3.41 ns/op (+/-   0.09, CI:[    3.42,    3.51])
        entt::sparse_set:       0.42 ns/op (+/-   0.02, CI:[    0.40,    0.42])
                std::set:      22.61 ns/op (+/-   2.35, CI:[   21.90,   24.15])
[Cooling: before iteration benchmark] [Ready: 2596 MHz]

================================================================================
  SECTION 2: Dense Iteration (SparseSet Key Advantage)
================================================================================

[2026-02-16 04:55:41] Section start CPU: 2596 MHz (base: 2596)
Contract Note: Dense iteration—SparseSet/FlatSet expected to outperform hash sets

--- N = 10000 ---
[2026-02-16 04:55:41] CPU: 2596 MHz (base: 2596)
[Cooling: size transition] [Ready: 2596 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.78 ns/op (+/-   0.18, CI:[    0.70,    0.87])
    fat_p::SparseSet<32>:       0.46 ns/op (+/-   0.04, CI:[    0.41,    0.45])
          fat_p::FlatSet:       0.45 ns/op (+/-   0.01, CI:[    0.44,    0.45])
      std::unordered_set:       3.46 ns/op (+/-   0.22, CI:[    3.44,    3.65])
     absl::flat_hash_set:       5.62 ns/op (+/-   0.59, CI:[    5.57,    6.14])
        entt::sparse_set:       0.44 ns/op (+/-   0.01, CI:[    0.44,    0.44])
                std::set:      11.80 ns/op (+/-   0.55, CI:[   11.68,   12.20])

--- N = 100000 ---
[2026-02-16 04:55:42] CPU: 2596 MHz (base: 2596)
[Cooling: size transition] [Ready: 2596 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       1.56 ns/op (+/-   0.35, CI:[    1.18,    1.51])
    fat_p::SparseSet<32>:       0.43 ns/op (+/-   0.05, CI:[    0.42,    0.47])
          fat_p::FlatSet:       0.43 ns/op (+/-   0.03, CI:[    0.43,    0.45])
      std::unordered_set:       9.26 ns/op (+/-   0.39, CI:[    9.11,    9.48])
     absl::flat_hash_set:       3.42 ns/op (+/-   0.13, CI:[    3.43,    3.55])
        entt::sparse_set:       0.42 ns/op (+/-   0.01, CI:[    0.41,    0.42])
                std::set:      22.36 ns/op (+/-   0.60, CI:[   22.08,   22.65])

[Cooling: before mixed workload] [Ready: 2596 MHz]

================================================================================
  SECTION 3: Mixed Workload (Insert/Erase Churn)
================================================================================

[2026-02-16 04:55:50] Section start CPU: 2596 MHz (base: 2596)
Contract Note: Random insert/erase churn—tests swap-with-back erase efficiency

--- N = 10000 (50% insert/erase cycles) ---
[2026-02-16 04:55:50] CPU: 2596 MHz (base: 2596)
  Mixed Workload:
     fat_p::SparseSet<8>:       3.10 ns/op (+/-   0.89, CI:[    3.02,    3.86])
    fat_p::SparseSet<32>:       4.54 ns/op (+/-   0.43, CI:[    4.47,    4.88])
          fat_p::FlatSet:     420.45 ns/op (+/-   2.88, CI:[  419.95,  422.71])
      std::unordered_set:      42.83 ns/op (+/-   1.42, CI:[   42.47,   43.83])
     absl::flat_hash_set:      22.23 ns/op (+/-   1.04, CI:[   22.33,   23.33])
        entt::sparse_set:      11.86 ns/op (+/-   0.83, CI:[   11.84,   12.64])
                std::set:     186.98 ns/op (+/-   0.91, CI:[  186.70,  187.57])

================================================================================
  Feature Comparison Summary
================================================================================

  Container                 Insert    Contains  Erase     Iteration  Order
  --------------------------------------------------------------------------
  fat_p::SparseSet<8>       O(1)*     O(1)      O(1)      Dense      Unstable  (max 256)
  fat_p::SparseSet<32>      O(1)*     O(1)      O(1)      Dense      Unstable  (handles large N)
  fat_p::FlatSet            O(n)      O(log n)  O(n)      Dense      Sorted
  std::unordered_set        O(1)*     O(1)*     O(1)*     Scattered  Unordered
  absl::flat_hash_set       O(1)*     O(1)*     O(1)*     Scattered  Unordered
  entt::sparse_set          O(1)*     O(1)      O(1)      Dense      Unstable
  std::set                  O(log n)  O(log n)  O(log n)  In-order   Sorted

  * = amortized
  Note: <8> variants use uint8_t value type (max 256 elements)
        <32> variants use uint32_t value type (handles large N)

  When to use SparseSet:
    - Integer keys in a bounded range
    - Frequent insert/erase churn
    - Iteration performance matters
    - Order doesn't matter

  When to use FlatSet:
    - Need sorted order
    - Mostly lookups after initial build
    - Binary search semantics (lower_bound, etc.)

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
