---
doc_id: BR-FlatMapSet-001
doc_type: "Benchmark Results"
title: "FlatMapSet"
fatp_components: ["FlatMapSet"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - FlatMapSet

**Source:** `benchmark_FlatMapSet.cpp`
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
| fat_p::FlatMap | x | x | x | x |
| fat_p::FlatSet | x | x | x | x |
| std::map | x | x | x | x |
| std::set | x | x | x | x |
| boost::flat_map / boost::flat_set | x | x | x | x |
| absl::btree_map / absl::btree_set | x | x | x | x |
| folly::sorted_vector_map | — | x | x | x |
| std::flat_map / std::flat_set (C++23 not available) | — | — | — | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
[WARNING: CPU frequency still unstable after 30s]
WARNING: CPU frequency still fluctuating, results may have higher variance.

================================================================================
  SECTION 1: Core Operations
================================================================================
[2026-02-15 19:23:32] Section start CPU: 2469 MHz (base: 3686)

--- N = 1000 ---
[2026-02-15 19:23:32] CPU: 2469 MHz (base: 3686)
[Cooling: size transition] [Ready: 2101 MHz]

  Bulk Build (sorted range):
                    std::map:    22.80 ns/op (+/-  0.28, CI:[22.64,22.92])
              fat_p::FlatMap:     1.40 ns/op (+/-  0.08, CI:[1.30,1.39])
             boost::flat_map:     0.60 ns/op (+/-  0.05, CI:[0.53,0.58])
             absl::btree_map:    13.30 ns/op (+/-  0.15, CI:[13.30,13.46])

  Bulk Insert (sorted):
                    std::map:    60.60 ns/op (+/-  1.26, CI:[60.37,61.64])
              fat_p::FlatMap:     5.40 ns/op (+/-  0.09, CI:[5.35,5.44])
             boost::flat_map:     6.90 ns/op (+/-  0.74, CI:[7.10,7.85])
             absl::btree_map:    51.10 ns/op (+/-  0.52, CI:[50.77,51.30])

  Bulk Insert (random):
                    std::map:    59.40 ns/op (+/- 36.31, CI:[50.92,87.67])
              fat_p::FlatMap:    80.90 ns/op (+/- 13.86, CI:[77.52,91.55])
             boost::flat_map:    86.60 ns/op (+/- 17.94, CI:[82.08,100.24])
             absl::btree_map:    37.40 ns/op (+/- 16.31, CI:[34.40,50.91])

  Find (hit):
                    std::map:    32.70 ns/op (+/-  1.49, CI:[32.64,34.15])
              fat_p::FlatMap:    34.00 ns/op (+/-  5.05, CI:[32.78,37.90])
             boost::flat_map:    34.10 ns/op (+/-  5.11, CI:[33.78,38.95])
             absl::btree_map:    18.80 ns/op (+/-  4.70, CI:[18.09,22.84])

  Find (miss):
                    std::map:     7.80 ns/op (+/-  0.34, CI:[7.58,7.92])
              fat_p::FlatMap:     4.80 ns/op (+/- 15.04, CI:[1.15,16.37])
             boost::flat_map:     4.90 ns/op (+/-  0.17, CI:[4.81,4.98])
             absl::btree_map:     2.70 ns/op (+/-  0.27, CI:[2.61,2.89])

  Iteration:
                    std::map:     2.40 ns/op (+/-  0.07, CI:[2.37,2.43])
              fat_p::FlatMap:     1.20 ns/op (+/-  0.06, CI:[1.18,1.24])
             boost::flat_map:     1.20 ns/op (+/-  1.21, CI:[0.94,2.16])
             absl::btree_map:     2.00 ns/op (+/-  0.12, CI:[1.91,2.03])

  lower_bound:
                    std::map:    17.50 ns/op (+/-  1.28, CI:[16.60,17.90])
              fat_p::FlatMap:    20.00 ns/op (+/-  2.35, CI:[19.26,21.64])
             boost::flat_map:    21.50 ns/op (+/-  1.18, CI:[20.84,22.03])
             absl::btree_map:    14.70 ns/op (+/-  0.80, CI:[14.34,15.15])

--- N = 10000 ---
[2026-02-15 19:25:30] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 3649 MHz]

  Bulk Build (sorted range):
                    std::map:    31.08 ns/op (+/-  4.33, CI:[29.75,34.13])
              fat_p::FlatMap:     3.92 ns/op (+/-  1.37, CI:[2.39,3.77])
             boost::flat_map:     0.86 ns/op (+/-  1.21, CI:[1.03,2.25])
             absl::btree_map:    16.00 ns/op (+/-  1.93, CI:[15.13,17.08])

  Bulk Insert (sorted):
                    std::map:    27.69 ns/op (+/-  2.22, CI:[27.33,29.58])
              fat_p::FlatMap:     4.49 ns/op (+/-  1.25, CI:[2.97,4.24])
             boost::flat_map:     3.24 ns/op (+/-  1.24, CI:[3.43,4.69])
             absl::btree_map:    21.77 ns/op (+/-  1.84, CI:[21.04,22.90])

  Bulk Insert (random):
                    std::map:    91.29 ns/op (+/- 11.18, CI:[88.09,99.41])
              fat_p::FlatMap:   640.49 ns/op (+/- 17.33, CI:[626.81,644.35])
             boost::flat_map:   646.26 ns/op (+/- 19.88, CI:[634.17,654.29])
             absl::btree_map:    60.26 ns/op (+/-  8.53, CI:[57.71,66.34])

  Find (hit):
                    std::map:    59.32 ns/op (+/- 10.71, CI:[59.50,70.34])
              fat_p::FlatMap:    48.72 ns/op (+/-  0.80, CI:[48.32,49.14])
             boost::flat_map:    51.50 ns/op (+/-  9.48, CI:[50.50,60.09])
             absl::btree_map:    35.40 ns/op (+/-  9.20, CI:[33.04,42.35])

  Find (miss):
                    std::map:     4.83 ns/op (+/-  0.29, CI:[4.59,4.88])
              fat_p::FlatMap:     5.78 ns/op (+/-  0.33, CI:[5.68,6.01])
             boost::flat_map:     5.79 ns/op (+/-  0.33, CI:[5.70,6.03])
             absl::btree_map:     1.53 ns/op (+/-  0.09, CI:[1.50,1.59])

  Iteration:
                    std::map:     3.62 ns/op (+/-  0.27, CI:[3.54,3.82])
              fat_p::FlatMap:     1.20 ns/op (+/-  0.05, CI:[1.16,1.21])
             boost::flat_map:     1.17 ns/op (+/-  0.07, CI:[1.16,1.24])
             absl::btree_map:     2.00 ns/op (+/-  0.20, CI:[1.93,2.13])

  lower_bound:
                    std::map:    30.15 ns/op (+/-  2.52, CI:[29.79,32.34])
              fat_p::FlatMap:    25.77 ns/op (+/- 12.44, CI:[24.81,37.40])
             boost::flat_map:    27.17 ns/op (+/-  3.89, CI:[26.71,30.65])
             absl::btree_map:    20.22 ns/op (+/-  0.67, CI:[19.90,20.58])

--- N = 100000 ---
[2026-02-15 19:27:04] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2248 MHz]

  Bulk Build (sorted range):
                    std::map:    42.07 ns/op (+/-  2.66, CI:[41.33,44.02])
              fat_p::FlatMap:     4.04 ns/op (+/-  0.32, CI:[3.95,4.27])
             boost::flat_map:     3.33 ns/op (+/-  0.57, CI:[3.22,3.80])
             absl::btree_map:    16.17 ns/op (+/-  1.44, CI:[15.94,17.40])

  Bulk Insert (sorted):
                    std::map:    40.20 ns/op (+/-  2.92, CI:[39.83,42.78])
              fat_p::FlatMap:     4.24 ns/op (+/-  0.49, CI:[4.10,4.59])
             boost::flat_map:     5.03 ns/op (+/-  0.24, CI:[4.96,5.21])
             absl::btree_map:    21.99 ns/op (+/-  0.96, CI:[21.52,22.49])

  Bulk Insert (random):
                    std::map:   144.05 ns/op (+/-  3.82, CI:[142.17,146.04])
              fat_p::FlatMap:  8174.69 ns/op (+/- 70.25, CI:[8142.10,8213.21])
             boost::flat_map:  8146.68 ns/op (+/-106.31, CI:[8078.96,8186.56])
             absl::btree_map:    72.36 ns/op (+/-  2.58, CI:[71.08,73.69])

  Find (hit):
                    std::map:   126.24 ns/op (+/-  4.29, CI:[123.97,128.31])
              fat_p::FlatMap:    65.29 ns/op (+/-  2.17, CI:[64.76,66.96])
             boost::flat_map:    67.78 ns/op (+/-  2.63, CI:[66.88,69.54])
             absl::btree_map:    47.40 ns/op (+/-  1.94, CI:[46.79,48.75])

  Find (miss):
                    std::map:     6.59 ns/op (+/-  0.88, CI:[6.32,7.21])
              fat_p::FlatMap:     6.89 ns/op (+/-  0.73, CI:[6.70,7.44])
             boost::flat_map:     6.89 ns/op (+/-  0.72, CI:[6.65,7.38])
             absl::btree_map:     1.90 ns/op (+/-  0.06, CI:[1.86,1.92])

  Iteration:
                    std::map:    17.66 ns/op (+/-  3.04, CI:[16.29,19.37])
              fat_p::FlatMap:     1.21 ns/op (+/-  0.05, CI:[1.23,1.28])
             boost::flat_map:     1.43 ns/op (+/-  0.12, CI:[1.41,1.53])
             absl::btree_map:     2.46 ns/op (+/-  0.76, CI:[2.17,2.94])

  lower_bound:
                    std::map:    53.88 ns/op (+/-  3.14, CI:[53.91,57.08])
              fat_p::FlatMap:    34.47 ns/op (+/-  2.72, CI:[34.21,36.97])
             boost::flat_map:    38.18 ns/op (+/-  2.66, CI:[37.11,39.80])
             absl::btree_map:    26.75 ns/op (+/-  1.55, CI:[26.78,28.35])
[Cooling: before pathological insert] [Ready: 2395 MHz]

================================================================================
  SECTION 2: Pathological Random Insert (FlatMap's Weakness)
================================================================================
  This benchmark measures single random insertions into a populated container.
  For flat containers, each insert may require shifting O(n) elements.

[2026-02-15 19:29:40] Section start CPU: 2395 MHz (base: 3686)

--- Base size: 1000, inserting 100 missing keys ---
[2026-02-15 19:29:40] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2322 MHz]
  Single random insert (into populated map):
                    std::map:    37.00 ns/op (+/- 13.00, CI:[34.16,47.31])
              fat_p::FlatMap:   132.00 ns/op (+/-  7.53, CI:[131.66,139.28])
             boost::flat_map:   142.00 ns/op (+/-753.77, CI:[-13.39,749.52])
             absl::btree_map:    29.00 ns/op (+/-  8.10, CI:[26.30,34.50])

--- Base size: 5000, inserting 100 missing keys ---
[2026-02-15 19:29:46] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2322 MHz]
  Single random insert (into populated map):
                    std::map:    39.00 ns/op (+/-  8.68, CI:[35.81,44.59])
              fat_p::FlatMap:   581.00 ns/op (+/-  6.74, CI:[580.06,586.88])
             boost::flat_map:   592.00 ns/op (+/-116.81, CI:[565.82,684.05])
             absl::btree_map:    45.00 ns/op (+/- 16.62, CI:[44.32,61.14])

--- Base size: 10000, inserting 100 missing keys ---
[2026-02-15 19:30:02] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2580 MHz]
  Single random insert (into populated map):
                    std::map:    52.00 ns/op (+/- 10.53, CI:[46.01,56.66])
              fat_p::FlatMap:  1203.00 ns/op (+/- 22.92, CI:[1200.33,1223.53])
             boost::flat_map:  1210.00 ns/op (+/-545.98, CI:[1085.23,1637.84])
             absl::btree_map:    78.00 ns/op (+/- 28.28, CI:[70.55,99.18])
[Cooling: before iteration benchmark] [Ready: 2101 MHz]

================================================================================
  SECTION 3: Iteration Speed (FlatMap's Strength)
================================================================================
  FlatMap stores elements contiguously, enabling hardware prefetching.
  std::map requires pointer chasing through scattered tree nodes.

[2026-02-15 19:30:28] Section start CPU: 2101 MHz (base: 3686)

--- N = 1000 ---
[Cooling: size transition] [Ready: 2248 MHz]
  Iteration (ns/element):
                    std::map:     2.60 ns/elem (+/-0.20)
              fat_p::FlatMap:     1.20 ns/elem (+/-0.08)
             boost::flat_map:     1.30 ns/elem (+/-0.06)
             absl::btree_map:     2.10 ns/elem (+/-0.11)

--- N = 10000 ---
[Cooling: size transition] [Ready: 2174 MHz]
  Iteration (ns/element):
                    std::map:     3.73 ns/elem (+/-1.10)
              fat_p::FlatMap:     1.36 ns/elem (+/-0.01)
             boost::flat_map:     1.21 ns/elem (+/-0.08)
             absl::btree_map:     2.00 ns/elem (+/-0.03)

--- N = 100000 ---
[Cooling: size transition] [Ready: 2432 MHz]
  Iteration (ns/element):
                    std::map:     7.79 ns/elem (+/-4.57)
              fat_p::FlatMap:     1.21 ns/elem (+/-0.03)
             boost::flat_map:     1.29 ns/elem (+/-0.09)
             absl::btree_map:     1.95 ns/elem (+/-0.27)

--- N = 1000000 ---
[Cooling: size transition] [Ready: 2432 MHz]
  Iteration (ns/element):
                    std::map:    19.62 ns/elem (+/-3.51)
              fat_p::FlatMap:     1.41 ns/elem (+/-0.16)
             boost::flat_map:     1.47 ns/elem (+/-0.14)
             absl::btree_map:     3.07 ns/elem (+/-0.48)
[Cooling: before set operations] [Ready: 2285 MHz]

================================================================================
  SECTION 4: FlatSet Core Operations
================================================================================
[2026-02-15 19:31:39] Section start CPU: 2285 MHz (base: 3686)

--- N = 1000 ---
[2026-02-15 19:31:39] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2322 MHz]

  Bulk Build (sorted range):
                    std::set:    22.60 ns/op (+/-  0.95, CI:[22.36,23.32])
              fat_p::FlatSet:     0.70 ns/op (+/-  0.18, CI:[0.66,0.84])
             boost::flat_set:     0.20 ns/op (+/-  0.05, CI:[0.20,0.25])
             absl::btree_set:    12.10 ns/op (+/-  0.80, CI:[11.58,12.39])

  Bulk Insert (sorted):
                    std::set:    22.50 ns/op (+/-  0.68, CI:[22.20,22.88])
              fat_p::FlatSet:     1.30 ns/op (+/-  0.06, CI:[1.32,1.39])
             boost::flat_set:     1.80 ns/op (+/-  0.06, CI:[1.81,1.87])
             absl::btree_set:    11.40 ns/op (+/-  0.88, CI:[11.02,11.91])

  Bulk Insert (random):
                    std::set:    59.20 ns/op (+/-  2.77, CI:[58.36,61.17])
              fat_p::FlatSet:    52.20 ns/op (+/-  1.23, CI:[51.77,53.02])
             boost::flat_set:    52.90 ns/op (+/-  2.01, CI:[52.48,54.52])
             absl::btree_set:    39.00 ns/op (+/-  1.85, CI:[37.95,39.82])

  Find (hit):
                    std::set:    31.20 ns/op (+/-  0.86, CI:[31.07,31.93])
              fat_p::FlatSet:    31.50 ns/op (+/-  1.47, CI:[31.49,32.99])
             boost::flat_set:    31.30 ns/op (+/-  2.96, CI:[30.66,33.66])
             absl::btree_set:    23.60 ns/op (+/-  0.94, CI:[23.39,24.33])

  Find (miss):
                    std::set:     3.30 ns/op (+/-  0.13, CI:[3.25,3.38])
              fat_p::FlatSet:     2.80 ns/op (+/-  0.17, CI:[2.80,2.97])
             boost::flat_set:     2.90 ns/op (+/-  0.12, CI:[2.87,3.00])
             absl::btree_set:     1.20 ns/op (+/-  0.04, CI:[1.20,1.23])

  Iteration:
                    std::set:     3.10 ns/op (+/-  0.05, CI:[3.03,3.08])
              fat_p::FlatSet:     1.70 ns/op (+/-  0.05, CI:[1.63,1.69])
             boost::flat_set:     1.70 ns/op (+/-  0.05, CI:[1.67,1.72])
             absl::btree_set:     2.40 ns/op (+/-  0.11, CI:[2.28,2.40])

  lower_bound:
                    std::set:    23.90 ns/op (+/-  2.04, CI:[23.58,25.64])
              fat_p::FlatSet:    25.10 ns/op (+/-  1.38, CI:[24.57,25.97])
             boost::flat_set:    25.60 ns/op (+/-  1.63, CI:[25.46,27.11])
             absl::btree_set:    21.30 ns/op (+/-  0.87, CI:[20.78,21.66])

--- N = 10000 ---
[2026-02-15 19:32:55] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2064 MHz]

  Bulk Build (sorted range):
                    std::set:    21.75 ns/op (+/-  8.05, CI:[21.59,29.74])
              fat_p::FlatSet:     0.89 ns/op (+/-  0.13, CI:[0.83,0.96])
             boost::flat_set:     0.23 ns/op (+/-  0.05, CI:[0.22,0.27])
             absl::btree_set:    12.14 ns/op (+/-  3.24, CI:[11.30,14.58])

  Bulk Insert (sorted):
                    std::set:    21.65 ns/op (+/-  3.49, CI:[21.21,24.74])
              fat_p::FlatSet:     1.43 ns/op (+/-  0.24, CI:[1.42,1.67])
             boost::flat_set:     2.32 ns/op (+/-  0.64, CI:[2.17,2.82])
             absl::btree_set:    10.93 ns/op (+/-  1.97, CI:[10.86,12.86])

  Bulk Insert (random):
                    std::set:    76.17 ns/op (+/-  2.02, CI:[75.92,77.97])
              fat_p::FlatSet:   186.13 ns/op (+/- 11.88, CI:[185.05,197.08])
             boost::flat_set:   187.46 ns/op (+/- 12.19, CI:[187.03,199.37])
             absl::btree_set:    45.35 ns/op (+/-  1.30, CI:[45.05,46.37])

  Find (hit):
                    std::set:    55.55 ns/op (+/-  7.74, CI:[53.91,61.75])
              fat_p::FlatSet:    46.42 ns/op (+/-  1.40, CI:[46.35,47.77])
             boost::flat_set:    46.53 ns/op (+/-  4.74, CI:[45.46,50.26])
             absl::btree_set:    34.58 ns/op (+/-  1.68, CI:[33.94,35.64])

  Find (miss):
                    std::set:     4.30 ns/op (+/-  1.65, CI:[3.95,5.62])
              fat_p::FlatSet:     5.78 ns/op (+/-  0.36, CI:[5.72,6.08])
             boost::flat_set:     5.78 ns/op (+/-  0.34, CI:[5.69,6.04])
             absl::btree_set:     1.17 ns/op (+/-  0.09, CI:[1.16,1.25])

  Iteration:
                    std::set:     2.89 ns/op (+/-  0.44, CI:[2.97,3.42])
              fat_p::FlatSet:     1.14 ns/op (+/-  0.17, CI:[1.17,1.34])
             boost::flat_set:     1.15 ns/op (+/-  0.17, CI:[1.17,1.34])
             absl::btree_set:     1.62 ns/op (+/-  0.26, CI:[1.58,1.85])

  lower_bound:
                    std::set:    28.94 ns/op (+/-  6.34, CI:[27.97,34.39])
              fat_p::FlatSet:    25.06 ns/op (+/-  7.89, CI:[23.75,31.73])
             boost::flat_set:    25.49 ns/op (+/-  0.82, CI:[25.35,26.18])
             absl::btree_set:    17.95 ns/op (+/-  0.69, CI:[17.99,18.69])

--- N = 100000 ---
[2026-02-15 19:34:42] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]

  Bulk Build (sorted range):
                    std::set:    22.38 ns/op (+/-  3.23, CI:[22.02,25.29])
              fat_p::FlatSet:     0.76 ns/op (+/-  0.38, CI:[0.75,1.13])
             boost::flat_set:     0.22 ns/op (+/-  0.08, CI:[0.21,0.29])
             absl::btree_set:    11.65 ns/op (+/-  1.00, CI:[11.37,12.39])

  Bulk Insert (sorted):
                    std::set:    22.28 ns/op (+/-  3.45, CI:[22.25,25.73])
              fat_p::FlatSet:     1.41 ns/op (+/-  0.78, CI:[1.28,2.08])
             boost::flat_set:     2.06 ns/op (+/-  0.48, CI:[2.04,2.53])
             absl::btree_set:    11.00 ns/op (+/-  0.47, CI:[10.86,11.33])

  Bulk Insert (random):
                    std::set:   126.59 ns/op (+/-  4.38, CI:[123.85,128.28])
              fat_p::FlatSet:  2579.29 ns/op (+/- 44.79, CI:[2555.61,2600.94])
             boost::flat_set:  2563.33 ns/op (+/- 43.70, CI:[2551.24,2595.48])
             absl::btree_set:    61.18 ns/op (+/-  4.18, CI:[59.89,64.12])

  Find (hit):
                    std::set:   110.88 ns/op (+/-  5.19, CI:[107.47,112.72])
              fat_p::FlatSet:    61.78 ns/op (+/-  2.59, CI:[61.08,63.71])
             boost::flat_set:    61.19 ns/op (+/-  1.63, CI:[60.90,62.55])
             absl::btree_set:    45.39 ns/op (+/-  1.93, CI:[43.63,45.58])

  Find (miss):
                    std::set:     6.61 ns/op (+/-  0.79, CI:[6.47,7.27])
              fat_p::FlatSet:     6.91 ns/op (+/-  0.62, CI:[6.85,7.48])
             boost::flat_set:     6.93 ns/op (+/-  0.15, CI:[6.86,7.01])
             absl::btree_set:     1.52 ns/op (+/-  0.24, CI:[1.49,1.74])

  Iteration:
                    std::set:     4.26 ns/op (+/-  1.13, CI:[4.31,5.45])
              fat_p::FlatSet:     1.20 ns/op (+/-  0.75, CI:[1.02,1.78])
             boost::flat_set:     1.20 ns/op (+/-  0.02, CI:[1.20,1.22])
             absl::btree_set:     1.66 ns/op (+/-  0.75, CI:[1.47,2.23])

  lower_bound:
                    std::set:    45.03 ns/op (+/-  2.30, CI:[44.60,46.93])
              fat_p::FlatSet:    32.73 ns/op (+/-  1.42, CI:[32.05,33.48])
             boost::flat_set:    32.88 ns/op (+/-  1.80, CI:[32.12,33.94])
             absl::btree_set:    22.51 ns/op (+/-  0.57, CI:[21.92,22.50])

================================================================================
  Memory Usage Comparison (Theoretical)
================================================================================
  For map<int64_t, int64_t>:

  Container                   Per-Entry Overhead    Total for N=10000
  -------------------------   -------------------   -----------------
  std::map                    ~40 bytes (tree node) ~400 KB + 160 KB data
  fat_p::FlatMap              ~0 bytes              ~160 KB (data only)
  boost::container::flat_map  ~0 bytes              ~160 KB (data only)
  absl::btree_map             ~2-4 bytes (B-tree)   ~180-200 KB

  Note: FlatMap has ~2.5x better memory efficiency than std::map.

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
--- N = 1000 ---
[2026-02-16 03:38:07] CPU: 3241 MHz (~base: 3241)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::map:    46.57 ns/op (+/-  7.54, CI:[45.94,52.55])
              fat_p::FlatMap:     1.69 ns/op (+/-  0.34, CI:[1.59,1.89])
             boost::flat_map:     0.42 ns/op (+/-  0.13, CI:[0.36,0.47])
             absl::btree_map:    13.93 ns/op (+/-  2.59, CI:[14.33,16.60])
    folly::sorted_vector_map:     4.15 ns/op (+/-  0.82, CI:[3.81,4.53])

  Bulk Insert (sorted):
                    std::map:    35.33 ns/op (+/- 10.63, CI:[33.39,42.71])
              fat_p::FlatMap:     1.81 ns/op (+/-  0.32, CI:[1.69,1.97])
             boost::flat_map:     2.55 ns/op (+/-  0.45, CI:[2.41,2.80])
             absl::btree_map:    14.24 ns/op (+/-  1.66, CI:[14.40,15.86])
    folly::sorted_vector_map:     4.17 ns/op (+/-  0.61, CI:[4.00,4.53])

  Bulk Insert (random):
                    std::map:    87.57 ns/op (+/-  2.62, CI:[86.48,88.77])
              fat_p::FlatMap:   122.02 ns/op (+/-  3.17, CI:[121.70,124.49])
             boost::flat_map:    90.40 ns/op (+/-  3.78, CI:[88.68,92.00])
             absl::btree_map:    64.12 ns/op (+/-  3.57, CI:[63.07,66.20])
    folly::sorted_vector_map:   122.14 ns/op (+/-  6.58, CI:[121.31,127.08])

  Find (hit):
                    std::map:    44.14 ns/op (+/-  4.67, CI:[42.17,46.27])
              fat_p::FlatMap:    40.94 ns/op (+/-  7.04, CI:[36.56,42.74])
             boost::flat_map:    44.34 ns/op (+/-  2.61, CI:[42.63,44.92])
             absl::btree_map:    32.34 ns/op (+/-  2.72, CI:[31.31,33.70])
    folly::sorted_vector_map:    39.17 ns/op (+/-  5.35, CI:[35.87,40.56])

  Find (miss):
                    std::map:     7.39 ns/op (+/-  1.32, CI:[7.00,8.15])
              fat_p::FlatMap:     8.87 ns/op (+/-  1.41, CI:[8.29,9.53])
             boost::flat_map:     7.44 ns/op (+/-  1.88, CI:[7.47,9.12])
             absl::btree_map:    12.64 ns/op (+/-  3.29, CI:[9.05,11.93])
    folly::sorted_vector_map:     8.41 ns/op (+/-  1.28, CI:[8.13,9.25])

  Iteration:
                    std::map:     4.83 ns/op (+/-  0.51, CI:[4.77,5.22])
              fat_p::FlatMap:     0.48 ns/op (+/-  0.06, CI:[0.49,0.54])
             boost::flat_map:     0.48 ns/op (+/-  0.07, CI:[0.48,0.54])
             absl::btree_map:     1.19 ns/op (+/-  0.10, CI:[1.18,1.27])
    folly::sorted_vector_map:     0.48 ns/op (+/-  0.05, CI:[0.48,0.52])

  lower_bound:
                    std::map:    14.41 ns/op (+/-  4.43, CI:[13.93,17.82])
              fat_p::FlatMap:    15.47 ns/op (+/-  3.38, CI:[14.59,17.55])
             boost::flat_map:    16.08 ns/op (+/-  3.60, CI:[14.87,18.02])
             absl::btree_map:    19.59 ns/op (+/-  3.07, CI:[19.23,21.92])
    folly::sorted_vector_map:    15.54 ns/op (+/-  2.62, CI:[15.07,17.37])

--- N = 10000 ---
[2026-02-16 03:38:33] CPU: 3241 MHz (~base: 3241)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::map:    76.20 ns/op (+/-  5.06, CI:[74.37,78.81])
              fat_p::FlatMap:     1.37 ns/op (+/-  0.21, CI:[1.33,1.51])
             boost::flat_map:     0.39 ns/op (+/-  0.15, CI:[0.41,0.54])
             absl::btree_map:    13.88 ns/op (+/-  4.93, CI:[14.89,19.22])
    folly::sorted_vector_map:     3.33 ns/op (+/-  0.01, CI:[3.32,3.33])

  Bulk Insert (sorted):
                    std::map:    29.51 ns/op (+/-  3.21, CI:[27.47,30.28])
              fat_p::FlatMap:     1.27 ns/op (+/-  0.01, CI:[1.27,1.28])
             boost::flat_map:     1.95 ns/op (+/-  0.22, CI:[1.90,2.09])
             absl::btree_map:    14.21 ns/op (+/-  5.66, CI:[16.09,21.05])
    folly::sorted_vector_map:     3.30 ns/op (+/-  0.27, CI:[3.28,3.52])

  Bulk Insert (random):
                    std::map:   126.72 ns/op (+/-  1.28, CI:[126.42,127.54])
              fat_p::FlatMap:   855.76 ns/op (+/-  4.59, CI:[855.08,859.10])
             boost::flat_map:   638.62 ns/op (+/-  1.98, CI:[637.85,639.58])
             absl::btree_map:    80.23 ns/op (+/-  7.18, CI:[79.92,86.21])
    folly::sorted_vector_map:   855.96 ns/op (+/-  2.96, CI:[855.61,858.20])

  Find (hit):
                    std::map:    99.68 ns/op (+/-  2.04, CI:[98.99,100.78])
              fat_p::FlatMap:    70.59 ns/op (+/-  0.69, CI:[70.11,70.72])
             boost::flat_map:    74.77 ns/op (+/-  0.53, CI:[74.31,74.78])
             absl::btree_map:    50.05 ns/op (+/-  1.24, CI:[49.62,50.71])
    folly::sorted_vector_map:    69.20 ns/op (+/-  0.76, CI:[69.09,69.75])

  Find (miss):
                    std::map:     8.04 ns/op (+/-  1.02, CI:[8.06,8.95])
              fat_p::FlatMap:    10.80 ns/op (+/-  0.82, CI:[10.75,11.47])
             boost::flat_map:     9.87 ns/op (+/-  0.31, CI:[9.80,10.07])
             absl::btree_map:     7.41 ns/op (+/-  0.52, CI:[7.35,7.81])
    folly::sorted_vector_map:    10.80 ns/op (+/-  1.39, CI:[11.17,12.39])

  Iteration:
                    std::map:     6.09 ns/op (+/-  0.58, CI:[5.90,6.41])
              fat_p::FlatMap:     0.45 ns/op (+/-  0.01, CI:[0.45,0.45])
             boost::flat_map:     0.45 ns/op (+/-  0.01, CI:[0.45,0.46])
             absl::btree_map:     1.06 ns/op (+/-  0.22, CI:[1.05,1.24])
    folly::sorted_vector_map:     0.46 ns/op (+/-  0.01, CI:[0.45,0.46])

  lower_bound:
                    std::map:    45.14 ns/op (+/-  1.02, CI:[45.13,46.03])
              fat_p::FlatMap:    37.36 ns/op (+/-  0.72, CI:[37.26,37.89])
             boost::flat_map:    37.27 ns/op (+/-  0.90, CI:[37.42,38.22])
             absl::btree_map:    29.87 ns/op (+/-  0.82, CI:[29.88,30.60])
    folly::sorted_vector_map:    37.69 ns/op (+/-  0.42, CI:[37.65,38.02])

--- N = 100000 ---
[2026-02-16 03:39:00] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::map:   111.00 ns/op (+/-  1.38, CI:[110.79,111.99])
              fat_p::FlatMap:     1.45 ns/op (+/-  0.16, CI:[1.45,1.59])
             boost::flat_map:     0.81 ns/op (+/-  0.10, CI:[0.78,0.87])
             absl::btree_map:    13.90 ns/op (+/-  3.90, CI:[14.08,17.49])
    folly::sorted_vector_map:     3.31 ns/op (+/-  0.05, CI:[3.32,3.36])

  Bulk Insert (sorted):
                    std::map:    29.63 ns/op (+/-  0.39, CI:[29.45,29.78])
              fat_p::FlatMap:     1.28 ns/op (+/-  0.04, CI:[1.29,1.32])
             boost::flat_map:     1.96 ns/op (+/-  0.05, CI:[1.97,2.01])
             absl::btree_map:    14.25 ns/op (+/-  3.53, CI:[14.15,17.24])
    folly::sorted_vector_map:     3.30 ns/op (+/-  0.06, CI:[3.32,3.37])

  Bulk Insert (random):
                    std::map:   215.79 ns/op (+/- 10.18, CI:[214.12,223.04])
              fat_p::FlatMap:  8129.68 ns/op (+/- 17.65, CI:[8127.90,8143.37])
             boost::flat_map:  7158.01 ns/op (+/- 57.19, CI:[7142.59,7192.72])
             absl::btree_map:   111.35 ns/op (+/- 11.96, CI:[112.76,123.24])
    folly::sorted_vector_map:  8127.18 ns/op (+/- 14.79, CI:[8124.93,8137.89])

  Find (hit):
                    std::map:   212.40 ns/op (+/- 15.54, CI:[206.74,220.36])
              fat_p::FlatMap:   113.90 ns/op (+/-  0.56, CI:[113.83,114.32])
             boost::flat_map:   118.81 ns/op (+/-  1.00, CI:[118.64,119.52])
             absl::btree_map:    80.05 ns/op (+/-  0.76, CI:[79.85,80.52])
    folly::sorted_vector_map:   112.45 ns/op (+/-  0.37, CI:[112.40,112.72])

  Find (miss):
                    std::map:    11.91 ns/op (+/-  0.38, CI:[11.81,12.14])
              fat_p::FlatMap:    12.89 ns/op (+/-  1.14, CI:[13.12,14.11])
             boost::flat_map:    11.82 ns/op (+/-  0.15, CI:[11.76,11.89])
             absl::btree_map:     9.05 ns/op (+/-  0.32, CI:[9.00,9.28])
    folly::sorted_vector_map:    13.92 ns/op (+/-  1.24, CI:[13.49,14.58])

  Iteration:
                    std::map:     6.34 ns/op (+/-  1.54, CI:[6.32,7.67])
              fat_p::FlatMap:     0.46 ns/op (+/-  0.07, CI:[0.46,0.51])
             boost::flat_map:     0.48 ns/op (+/-  0.10, CI:[0.48,0.57])
             absl::btree_map:     1.06 ns/op (+/-  0.02, CI:[1.06,1.07])
    folly::sorted_vector_map:     0.47 ns/op (+/-  0.04, CI:[0.46,0.50])

  lower_bound:
                    std::map:   105.13 ns/op (+/-  8.27, CI:[101.81,109.06])
              fat_p::FlatMap:    54.72 ns/op (+/-  0.25, CI:[54.67,54.89])
             boost::flat_map:    54.94 ns/op (+/-  0.35, CI:[54.88,55.18])
             absl::btree_map:    43.99 ns/op (+/-  0.51, CI:[43.91,44.36])
    folly::sorted_vector_map:    54.98 ns/op (+/-  0.34, CI:[54.91,55.21])
[Cooling: before pathological insert] [Ready]

================================================================================
  SECTION 2: Pathological Random Insert (FlatMap's Weakness)
================================================================================
  This benchmark measures single random insertions into a populated container.
  For flat containers, each insert may require shifting O(n) elements.

[2026-02-16 03:40:29] Section start CPU: 2445 MHz (~base: 2445)

--- Base size: 1000, inserting 100 missing keys ---
[2026-02-16 03:40:29] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Single random insert (into populated map):
                    std::map:    43.33 ns/op (+/-  8.66, CI:[42.22,49.82])
              fat_p::FlatMap:   200.53 ns/op (+/- 12.62, CI:[200.99,212.05])
             boost::flat_map:   112.76 ns/op (+/-  9.46, CI:[111.92,120.21])
             absl::btree_map:    45.08 ns/op (+/-  5.01, CI:[44.38,48.77])

--- Base size: 5000, inserting 100 missing keys ---
[2026-02-16 03:40:32] CPU: 2598 MHz (~base: 2598)
[Cooling: size transition] [Ready]
  Single random insert (into populated map):
                    std::map:    59.76 ns/op (+/-  6.63, CI:[58.44,64.25])
              fat_p::FlatMap:   804.86 ns/op (+/- 41.07, CI:[806.41,842.41])
             boost::flat_map:   586.79 ns/op (+/- 26.41, CI:[581.31,604.47])
             absl::btree_map:    75.74 ns/op (+/- 10.24, CI:[75.03,84.00])

--- Base size: 10000, inserting 100 missing keys ---
[2026-02-16 03:40:36] CPU: 2606 MHz (~base: 2606)
[Cooling: size transition] [Ready]
  Single random insert (into populated map):
                    std::map:    95.08 ns/op (+/- 11.02, CI:[94.23,103.89])
              fat_p::FlatMap:  1531.86 ns/op (+/- 34.96, CI:[1531.39,1562.03])
             boost::flat_map:  1182.95 ns/op (+/- 37.20, CI:[1182.45,1215.06])
             absl::btree_map:   108.25 ns/op (+/- 11.72, CI:[102.36,112.63])
[Cooling: before iteration benchmark] [Ready]

================================================================================
  SECTION 3: Iteration Speed (FlatMap's Strength)
================================================================================
  FlatMap stores elements contiguously, enabling hardware prefetching.
  std::map requires pointer chasing through scattered tree nodes.

[2026-02-16 03:40:43] Section start CPU: 2892 MHz (~base: 2892)

--- N = 1000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:     5.77 ns/elem (+/-3.04)
              fat_p::FlatMap:     0.51 ns/elem (+/-0.09)
             boost::flat_map:     0.51 ns/elem (+/-0.09)
             absl::btree_map:     1.36 ns/elem (+/-0.27)

--- N = 10000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:     6.43 ns/elem (+/-0.76)
              fat_p::FlatMap:     0.45 ns/elem (+/-0.01)
             boost::flat_map:     0.45 ns/elem (+/-0.01)
             absl::btree_map:     1.05 ns/elem (+/-0.02)

--- N = 100000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:    12.38 ns/elem (+/-6.12)
              fat_p::FlatMap:     0.49 ns/elem (+/-0.04)
             boost::flat_map:     0.52 ns/elem (+/-0.05)
             absl::btree_map:     1.05 ns/elem (+/-0.06)

--- N = 1000000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:    32.18 ns/elem (+/-26.05)
              fat_p::FlatMap:     0.85 ns/elem (+/-0.04)
             boost::flat_map:     0.82 ns/elem (+/-0.05)
             absl::btree_map:     1.30 ns/elem (+/-0.03)
[Cooling: before set operations] [Ready]

================================================================================
  SECTION 4: FlatSet Core Operations
================================================================================
[2026-02-16 03:41:06] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 ---
[2026-02-16 03:41:06] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::set:    38.47 ns/op (+/-  7.37, CI:[33.15,39.61])
              fat_p::FlatSet:     1.31 ns/op (+/-  0.22, CI:[1.23,1.42])
             boost::flat_set:     0.25 ns/op (+/-  0.08, CI:[0.24,0.31])
             absl::btree_set:    14.78 ns/op (+/-  1.21, CI:[14.59,15.65])

  Bulk Insert (sorted):
                    std::set:    39.44 ns/op (+/-  7.61, CI:[39.95,46.61])
              fat_p::FlatSet:     2.26 ns/op (+/-  0.34, CI:[2.27,2.57])
             boost::flat_set:     2.44 ns/op (+/-  0.31, CI:[2.49,2.76])
             absl::btree_set:    16.76 ns/op (+/-  1.91, CI:[15.99,17.66])

  Bulk Insert (random):
                    std::set:    78.25 ns/op (+/-  3.26, CI:[77.71,80.56])
              fat_p::FlatSet:    60.07 ns/op (+/-  3.76, CI:[59.15,62.44])
             boost::flat_set:    61.44 ns/op (+/-  3.14, CI:[59.01,61.76])
             absl::btree_set:    59.20 ns/op (+/-  5.15, CI:[57.78,62.29])

  Find (hit):
                    std::set:    36.60 ns/op (+/-  6.36, CI:[35.80,41.37])
              fat_p::FlatSet:    29.69 ns/op (+/-  6.02, CI:[27.23,32.51])
             boost::flat_set:    33.03 ns/op (+/-  5.23, CI:[30.61,35.19])
             absl::btree_set:    29.95 ns/op (+/-  4.21, CI:[30.61,34.30])

  Find (miss):
                    std::set:     5.48 ns/op (+/-  0.44, CI:[5.34,5.73])
              fat_p::FlatSet:     9.17 ns/op (+/-  0.52, CI:[8.97,9.42])
             boost::flat_set:     6.19 ns/op (+/-  0.45, CI:[6.04,6.43])
             absl::btree_set:     4.25 ns/op (+/-  0.36, CI:[3.96,4.28])

  Iteration:
                    std::set:     4.58 ns/op (+/-  0.41, CI:[4.50,4.85])
              fat_p::FlatSet:     0.52 ns/op (+/-  0.05, CI:[0.51,0.55])
             boost::flat_set:     0.51 ns/op (+/-  0.03, CI:[0.50,0.53])
             absl::btree_set:     1.07 ns/op (+/-  0.12, CI:[1.02,1.13])

  lower_bound:
                    std::set:    11.95 ns/op (+/-  2.38, CI:[11.71,13.80])
              fat_p::FlatSet:    11.50 ns/op (+/-  2.20, CI:[11.30,13.22])
             boost::flat_set:    11.36 ns/op (+/-  2.58, CI:[11.18,13.44])
             absl::btree_set:    17.20 ns/op (+/-  1.30, CI:[17.12,18.25])

--- N = 10000 ---
[2026-02-16 03:41:31] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::set:    28.53 ns/op (+/-  2.88, CI:[26.48,29.00])
              fat_p::FlatSet:     0.97 ns/op (+/-  0.01, CI:[0.96,0.97])
             boost::flat_set:     0.17 ns/op (+/-  0.04, CI:[0.17,0.20])
             absl::btree_set:    14.39 ns/op (+/-  0.23, CI:[14.34,14.54])

  Bulk Insert (sorted):
                    std::set:    28.86 ns/op (+/-  3.07, CI:[26.72,29.41])
              fat_p::FlatSet:     1.25 ns/op (+/-  0.00, CI:[1.24,1.25])
             boost::flat_set:     1.44 ns/op (+/-  0.02, CI:[1.43,1.45])
             absl::btree_set:    14.52 ns/op (+/-  0.42, CI:[14.46,14.83])

  Bulk Insert (random):
                    std::set:   112.66 ns/op (+/-  0.84, CI:[112.30,113.03])
              fat_p::FlatSet:   309.87 ns/op (+/-  1.63, CI:[309.54,310.98])
             boost::flat_set:   310.73 ns/op (+/-  1.03, CI:[310.54,311.44])
             absl::btree_set:    74.19 ns/op (+/-  1.90, CI:[72.95,74.62])

  Find (hit):
                    std::set:    84.63 ns/op (+/-  1.46, CI:[83.80,85.08])
              fat_p::FlatSet:    66.14 ns/op (+/-  0.65, CI:[65.62,66.19])
             boost::flat_set:    66.18 ns/op (+/-  0.74, CI:[65.93,66.57])
             absl::btree_set:    42.93 ns/op (+/-  0.59, CI:[42.90,43.42])

  Find (miss):
                    std::set:     6.22 ns/op (+/-  0.45, CI:[6.22,6.62])
              fat_p::FlatSet:    10.79 ns/op (+/-  1.00, CI:[10.64,11.51])
             boost::flat_set:     6.17 ns/op (+/-  0.48, CI:[6.11,6.53])
             absl::btree_set:     2.78 ns/op (+/-  0.22, CI:[2.73,2.93])

  Iteration:
                    std::set:     4.80 ns/op (+/-  0.36, CI:[4.72,5.04])
              fat_p::FlatSet:     0.43 ns/op (+/-  0.02, CI:[0.42,0.44])
             boost::flat_set:     0.42 ns/op (+/-  0.00, CI:[0.42,0.42])
             absl::btree_set:     0.78 ns/op (+/-  0.08, CI:[0.78,0.85])

  lower_bound:
                    std::set:    42.95 ns/op (+/-  1.14, CI:[42.69,43.69])
              fat_p::FlatSet:    36.46 ns/op (+/-  0.80, CI:[36.03,36.73])
             boost::flat_set:    34.30 ns/op (+/-  1.30, CI:[34.06,35.20])
             absl::btree_set:    27.29 ns/op (+/-  0.58, CI:[27.17,27.68])

--- N = 100000 ---
[2026-02-16 03:41:58] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::set:    28.33 ns/op (+/-  0.30, CI:[28.17,28.44])
              fat_p::FlatSet:     0.98 ns/op (+/-  0.04, CI:[0.98,1.01])
             boost::flat_set:     0.38 ns/op (+/-  0.04, CI:[0.37,0.41])
             absl::btree_set:    14.39 ns/op (+/-  0.08, CI:[14.36,14.42])

  Bulk Insert (sorted):
                    std::set:    28.99 ns/op (+/-  2.05, CI:[27.86,29.66])
              fat_p::FlatSet:     1.25 ns/op (+/-  0.02, CI:[1.25,1.27])
             boost::flat_set:     1.43 ns/op (+/-  0.00, CI:[1.43,1.43])
             absl::btree_set:    14.50 ns/op (+/-  0.05, CI:[14.48,14.52])

  Bulk Insert (random):
                    std::set:   181.90 ns/op (+/-  5.45, CI:[182.19,186.97])
              fat_p::FlatSet:  3224.67 ns/op (+/- 26.84, CI:[3206.12,3229.65])
             boost::flat_set:  3229.19 ns/op (+/- 24.92, CI:[3206.88,3228.72])
             absl::btree_set:    89.61 ns/op (+/-  3.35, CI:[88.89,91.82])

  Find (hit):
                    std::set:   166.09 ns/op (+/- 11.74, CI:[164.59,174.88])
              fat_p::FlatSet:    93.85 ns/op (+/-  0.75, CI:[93.82,94.48])
             boost::flat_set:    94.64 ns/op (+/-  0.55, CI:[94.45,94.94])
             absl::btree_set:    66.92 ns/op (+/-  0.53, CI:[66.85,67.32])

  Find (miss):
                    std::set:     8.93 ns/op (+/-  0.16, CI:[8.86,9.00])
              fat_p::FlatSet:    12.78 ns/op (+/-  0.89, CI:[12.97,13.75])
             boost::flat_set:     7.50 ns/op (+/-  0.22, CI:[7.47,7.66])
             absl::btree_set:     3.40 ns/op (+/-  0.17, CI:[3.41,3.56])

  Iteration:
                    std::set:     4.59 ns/op (+/-  0.28, CI:[4.51,4.76])
              fat_p::FlatSet:     0.42 ns/op (+/-  0.02, CI:[0.42,0.43])
             boost::flat_set:     0.42 ns/op (+/-  0.03, CI:[0.42,0.44])
             absl::btree_set:     0.77 ns/op (+/-  0.04, CI:[0.77,0.81])

  lower_bound:
                    std::set:    76.81 ns/op (+/-  0.90, CI:[76.39,77.18])
              fat_p::FlatSet:    46.94 ns/op (+/-  0.27, CI:[46.92,47.16])
             boost::flat_set:    45.67 ns/op (+/-  0.41, CI:[45.59,45.95])
             absl::btree_set:    35.90 ns/op (+/-  0.30, CI:[35.72,35.99])

================================================================================
  Memory Usage Comparison (Theoretical)
================================================================================
  For map<int64_t, int64_t>:

  Container                   Per-Entry Overhead    Total for N=10000
  -------------------------   -------------------   -----------------
  std::map                    ~40 bytes (tree node) ~400 KB + 160 KB data
  fat_p::FlatMap              ~0 bytes              ~160 KB (data only)
  boost::container::flat_map  ~0 bytes              ~160 KB (data only)
  absl::btree_map             ~2-4 bytes (B-tree)   ~180-200 KB

  Note: FlatMap has ~2.5x better memory efficiency than std::map.

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
--- N = 1000 ---
[2026-02-16 04:11:26] CPU: 3248 MHz (~base: 3248)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::map:    72.17 ns/op (+/-  2.50, CI:[71.23,73.43])
              fat_p::FlatMap:     3.74 ns/op (+/-  0.55, CI:[3.56,4.04])
             boost::flat_map:     0.41 ns/op (+/-  0.12, CI:[0.36,0.47])
             absl::btree_map:    11.93 ns/op (+/-  3.52, CI:[12.22,15.30])
    folly::sorted_vector_map:     6.83 ns/op (+/-  0.07, CI:[6.84,6.91])

  Bulk Insert (sorted):
                    std::map:    37.51 ns/op (+/-  8.48, CI:[34.24,41.68])
              fat_p::FlatMap:     6.99 ns/op (+/-  1.36, CI:[6.25,7.44])
             boost::flat_map:     5.72 ns/op (+/-  1.18, CI:[5.03,6.07])
             absl::btree_map:    15.09 ns/op (+/-  9.47, CI:[12.86,21.15])
    folly::sorted_vector_map:     6.94 ns/op (+/-  0.69, CI:[6.94,7.55])

  Bulk Insert (random):
                    std::map:    84.53 ns/op (+/-  2.94, CI:[84.26,86.83])
              fat_p::FlatMap:   100.68 ns/op (+/-  6.65, CI:[99.27,105.10])
             boost::flat_map:    84.44 ns/op (+/-  3.06, CI:[82.99,85.68])
             absl::btree_map:    62.21 ns/op (+/-  2.97, CI:[61.72,64.32])
    folly::sorted_vector_map:   100.92 ns/op (+/-  4.42, CI:[100.14,104.01])

  Find (hit):
                    std::map:    32.95 ns/op (+/-  1.99, CI:[31.73,33.48])
              fat_p::FlatMap:    34.72 ns/op (+/-  5.19, CI:[32.31,36.85])
             boost::flat_map:    35.50 ns/op (+/-  3.16, CI:[34.23,36.99])
             absl::btree_map:    31.88 ns/op (+/-  1.21, CI:[30.96,32.02])
    folly::sorted_vector_map:    36.51 ns/op (+/-  3.57, CI:[35.10,38.23])

  Find (miss):
                    std::map:    10.30 ns/op (+/-  1.32, CI:[10.39,11.55])
              fat_p::FlatMap:     4.74 ns/op (+/-  2.51, CI:[5.08,7.27])
             boost::flat_map:     7.30 ns/op (+/-  1.39, CI:[7.50,8.72])
             absl::btree_map:     4.10 ns/op (+/-  0.87, CI:[4.36,5.12])
    folly::sorted_vector_map:     4.68 ns/op (+/-  0.97, CI:[5.07,5.92])

  Iteration:
                    std::map:     4.95 ns/op (+/-  0.37, CI:[4.87,5.20])
              fat_p::FlatMap:     0.68 ns/op (+/-  0.09, CI:[0.71,0.79])
             boost::flat_map:     0.52 ns/op (+/-  0.04, CI:[0.51,0.54])
             absl::btree_map:     1.34 ns/op (+/-  0.30, CI:[1.25,1.51])
    folly::sorted_vector_map:     0.68 ns/op (+/-  0.09, CI:[0.70,0.79])

  lower_bound:
                    std::map:    16.80 ns/op (+/-  1.07, CI:[16.53,17.47])
              fat_p::FlatMap:    17.34 ns/op (+/-  3.60, CI:[16.30,19.45])
             boost::flat_map:    13.76 ns/op (+/-  2.50, CI:[12.78,14.97])
             absl::btree_map:    16.27 ns/op (+/-  2.45, CI:[16.17,18.32])
    folly::sorted_vector_map:    17.08 ns/op (+/-  2.83, CI:[16.31,18.79])

--- N = 10000 ---
[2026-02-16 04:11:52] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::map:   125.20 ns/op (+/-  7.12, CI:[121.01,127.25])
              fat_p::FlatMap:     3.44 ns/op (+/-  0.25, CI:[3.38,3.60])
             boost::flat_map:     0.37 ns/op (+/-  0.07, CI:[0.37,0.43])
             absl::btree_map:    11.13 ns/op (+/-  4.75, CI:[11.76,15.92])
    folly::sorted_vector_map:     6.90 ns/op (+/-  0.25, CI:[6.88,7.10])

  Bulk Insert (sorted):
                    std::map:    28.83 ns/op (+/-  3.62, CI:[26.85,30.02])
              fat_p::FlatMap:     4.66 ns/op (+/-  0.67, CI:[4.52,5.10])
             boost::flat_map:     3.83 ns/op (+/-  0.01, CI:[3.83,3.84])
             absl::btree_map:    10.98 ns/op (+/-  5.49, CI:[12.63,17.44])
    folly::sorted_vector_map:     6.90 ns/op (+/-  0.32, CI:[6.89,7.17])

  Bulk Insert (random):
                    std::map:   118.31 ns/op (+/-  2.40, CI:[117.86,119.97])
              fat_p::FlatMap:   796.15 ns/op (+/-  2.45, CI:[795.80,797.94])
             boost::flat_map:   631.59 ns/op (+/-  1.42, CI:[631.02,632.27])
             absl::btree_map:    81.94 ns/op (+/-  6.65, CI:[81.09,86.92])
    folly::sorted_vector_map:   797.79 ns/op (+/-  3.14, CI:[796.72,799.47])

  Find (hit):
                    std::map:    63.47 ns/op (+/-  2.54, CI:[62.44,64.66])
              fat_p::FlatMap:    66.16 ns/op (+/-  0.82, CI:[65.87,66.58])
             boost::flat_map:    64.46 ns/op (+/-  0.86, CI:[64.06,64.82])
             absl::btree_map:    52.28 ns/op (+/-  0.69, CI:[51.94,52.54])
    folly::sorted_vector_map:    65.78 ns/op (+/-  0.71, CI:[65.38,66.00])

  Find (miss):
                    std::map:    15.16 ns/op (+/-  1.00, CI:[15.07,15.96])
              fat_p::FlatMap:     6.48 ns/op (+/-  1.15, CI:[6.50,7.51])
             boost::flat_map:    10.54 ns/op (+/-  0.44, CI:[10.60,10.98])
             absl::btree_map:     4.79 ns/op (+/-  0.29, CI:[4.71,4.96])
    folly::sorted_vector_map:     9.21 ns/op (+/-  1.43, CI:[7.57,8.83])

  Iteration:
                    std::map:     7.41 ns/op (+/-  1.05, CI:[7.02,7.94])
              fat_p::FlatMap:     0.63 ns/op (+/-  0.01, CI:[0.62,0.63])
             boost::flat_map:     0.46 ns/op (+/-  0.01, CI:[0.45,0.46])
             absl::btree_map:     1.06 ns/op (+/-  0.04, CI:[1.06,1.10])
    folly::sorted_vector_map:     0.63 ns/op (+/-  0.03, CI:[0.62,0.65])

  lower_bound:
                    std::map:    32.54 ns/op (+/-  0.88, CI:[32.38,33.16])
              fat_p::FlatMap:    36.99 ns/op (+/-  0.72, CI:[36.56,37.19])
             boost::flat_map:    34.56 ns/op (+/-  0.56, CI:[34.40,34.90])
             absl::btree_map:    28.92 ns/op (+/-  1.08, CI:[29.03,29.98])
    folly::sorted_vector_map:    36.97 ns/op (+/-  0.52, CI:[36.63,37.09])

--- N = 100000 ---
[2026-02-16 04:12:19] CPU: 3243 MHz (~base: 3243)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::map:   162.95 ns/op (+/-  0.78, CI:[162.72,163.40])
              fat_p::FlatMap:     3.60 ns/op (+/-  0.08, CI:[3.56,3.63])
             boost::flat_map:     0.60 ns/op (+/-  0.05, CI:[0.60,0.65])
             absl::btree_map:     9.70 ns/op (+/-  3.88, CI:[9.90,13.30])
    folly::sorted_vector_map:     7.01 ns/op (+/-  0.05, CI:[6.97,7.01])

  Bulk Insert (sorted):
                    std::map:    28.94 ns/op (+/-  0.46, CI:[28.88,29.28])
              fat_p::FlatMap:     4.75 ns/op (+/-  0.08, CI:[4.71,4.79])
             boost::flat_map:     3.85 ns/op (+/-  0.10, CI:[3.86,3.95])
             absl::btree_map:    10.96 ns/op (+/-  3.43, CI:[10.86,13.88])
    folly::sorted_vector_map:     7.01 ns/op (+/-  0.16, CI:[7.02,7.15])

  Bulk Insert (random):
                    std::map:   208.53 ns/op (+/- 12.56, CI:[206.00,217.01])
              fat_p::FlatMap:  7892.13 ns/op (+/- 56.97, CI:[7880.73,7930.67])
             boost::flat_map:  6523.32 ns/op (+/- 60.54, CI:[6517.23,6570.30])
             absl::btree_map:   111.34 ns/op (+/-  8.54, CI:[112.31,119.79])
    folly::sorted_vector_map:  7895.53 ns/op (+/- 31.35, CI:[7890.41,7917.88])

  Find (hit):
                    std::map:   120.19 ns/op (+/-  7.82, CI:[117.47,124.32])
              fat_p::FlatMap:   103.66 ns/op (+/-  2.89, CI:[104.00,106.54])
             boost::flat_map:   102.32 ns/op (+/-  3.06, CI:[102.76,105.43])
             absl::btree_map:    81.50 ns/op (+/-  0.83, CI:[81.13,81.86])
    folly::sorted_vector_map:   103.20 ns/op (+/-  3.13, CI:[103.89,106.64])

  Find (miss):
                    std::map:    23.73 ns/op (+/-  0.10, CI:[23.69,23.77])
              fat_p::FlatMap:     8.36 ns/op (+/-  1.23, CI:[7.99,9.07])
             boost::flat_map:    12.60 ns/op (+/-  0.19, CI:[12.53,12.69])
             absl::btree_map:     5.80 ns/op (+/-  0.11, CI:[5.74,5.84])
    folly::sorted_vector_map:     9.81 ns/op (+/-  1.39, CI:[8.79,10.00])

  Iteration:
                    std::map:     8.37 ns/op (+/-  4.46, CI:[7.44,11.35])
              fat_p::FlatMap:     0.67 ns/op (+/-  0.05, CI:[0.66,0.71])
             boost::flat_map:     0.60 ns/op (+/-  0.11, CI:[0.57,0.66])
             absl::btree_map:     1.08 ns/op (+/-  0.08, CI:[1.08,1.14])
    folly::sorted_vector_map:     0.70 ns/op (+/-  0.07, CI:[0.69,0.75])

  lower_bound:
                    std::map:    52.58 ns/op (+/-  7.45, CI:[51.38,57.91])
              fat_p::FlatMap:    50.87 ns/op (+/-  0.57, CI:[50.82,51.32])
             boost::flat_map:    48.25 ns/op (+/-  0.33, CI:[48.04,48.33])
             absl::btree_map:    42.14 ns/op (+/-  0.72, CI:[41.95,42.58])
    folly::sorted_vector_map:    51.30 ns/op (+/-  0.35, CI:[51.18,51.49])
[Cooling: before pathological insert] [Ready]

================================================================================
  SECTION 2: Pathological Random Insert (FlatMap's Weakness)
================================================================================
  This benchmark measures single random insertions into a populated container.
  For flat containers, each insert may require shifting O(n) elements.

[2026-02-16 04:13:46] Section start CPU: 2445 MHz (~base: 2445)

--- Base size: 1000, inserting 100 missing keys ---
[2026-02-16 04:13:46] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Single random insert (into populated map):
                    std::map:    65.57 ns/op (+/-  8.07, CI:[64.96,72.04])
              fat_p::FlatMap:   159.30 ns/op (+/-  5.13, CI:[158.06,162.55])
             boost::flat_map:   108.30 ns/op (+/-  5.79, CI:[107.79,112.86])
             absl::btree_map:    46.89 ns/op (+/-  3.77, CI:[46.44,49.74])

--- Base size: 5000, inserting 100 missing keys ---
[2026-02-16 04:13:50] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Single random insert (into populated map):
                    std::map:    92.67 ns/op (+/-  6.32, CI:[92.91,98.45])
              fat_p::FlatMap:   742.38 ns/op (+/- 27.67, CI:[738.59,762.85])
             boost::flat_map:   581.04 ns/op (+/-  2.52, CI:[580.01,582.21])
             absl::btree_map:    72.98 ns/op (+/-  6.59, CI:[72.41,78.18])

--- Base size: 10000, inserting 100 missing keys ---
[2026-02-16 04:13:53] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Single random insert (into populated map):
                    std::map:   131.59 ns/op (+/- 20.89, CI:[128.74,147.06])
              fat_p::FlatMap:  1460.72 ns/op (+/- 28.40, CI:[1460.71,1485.61])
             boost::flat_map:  1171.18 ns/op (+/- 33.25, CI:[1170.42,1199.57])
             absl::btree_map:    95.72 ns/op (+/- 27.64, CI:[94.06,118.29])
[Cooling: before iteration benchmark] [Ready]

================================================================================
  SECTION 3: Iteration Speed (FlatMap's Strength)
================================================================================
  FlatMap stores elements contiguously, enabling hardware prefetching.
  std::map requires pointer chasing through scattered tree nodes.

[2026-02-16 04:14:01] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:     4.86 ns/elem (+/-9.10)
              fat_p::FlatMap:     0.73 ns/elem (+/-0.03)
             boost::flat_map:     0.52 ns/elem (+/-0.02)
             absl::btree_map:     1.07 ns/elem (+/-0.28)

--- N = 10000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:     6.65 ns/elem (+/-1.54)
              fat_p::FlatMap:     0.65 ns/elem (+/-0.03)
             boost::flat_map:     0.46 ns/elem (+/-0.00)
             absl::btree_map:     1.05 ns/elem (+/-0.01)

--- N = 100000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:     9.33 ns/elem (+/-4.58)
              fat_p::FlatMap:     0.62 ns/elem (+/-0.03)
             boost::flat_map:     0.45 ns/elem (+/-0.02)
             absl::btree_map:     1.06 ns/elem (+/-0.04)

--- N = 1000000 ---
[Cooling: size transition] [Ready]
  Iteration (ns/element):
                    std::map:    28.11 ns/elem (+/-29.35)
              fat_p::FlatMap:     0.82 ns/elem (+/-0.03)
             boost::flat_map:     0.64 ns/elem (+/-0.08)
             absl::btree_map:     1.23 ns/elem (+/-0.06)
[Cooling: before set operations] [Ready]

================================================================================
  SECTION 4: FlatSet Core Operations
================================================================================
[2026-02-16 04:14:25] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 ---
[2026-02-16 04:14:25] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::set:    34.22 ns/op (+/-  4.65, CI:[30.22,34.29])
              fat_p::FlatSet:     1.92 ns/op (+/-  0.19, CI:[1.89,2.05])
             boost::flat_set:     0.14 ns/op (+/-  0.05, CI:[0.14,0.19])
             absl::btree_set:    12.71 ns/op (+/-  2.06, CI:[12.50,14.30])

  Bulk Insert (sorted):
                    std::set:    37.26 ns/op (+/-  6.71, CI:[35.99,41.87])
              fat_p::FlatSet:     4.69 ns/op (+/-  0.54, CI:[4.64,5.12])
             boost::flat_set:     4.75 ns/op (+/-  0.69, CI:[4.71,5.31])
             absl::btree_set:    13.46 ns/op (+/-  1.86, CI:[13.13,14.76])

  Bulk Insert (random):
                    std::set:    78.72 ns/op (+/-  3.11, CI:[77.95,80.68])
              fat_p::FlatSet:    63.60 ns/op (+/-  3.69, CI:[62.71,65.95])
             boost::flat_set:    60.97 ns/op (+/-  2.79, CI:[59.19,61.64])
             absl::btree_set:    56.80 ns/op (+/-  3.39, CI:[56.15,59.12])

  Find (hit):
                    std::set:    27.93 ns/op (+/-  2.74, CI:[26.83,29.24])
              fat_p::FlatSet:    16.35 ns/op (+/-  5.85, CI:[16.52,21.65])
             boost::flat_set:    16.92 ns/op (+/-  6.30, CI:[16.19,21.72])
             absl::btree_set:    29.51 ns/op (+/-  3.69, CI:[29.30,32.53])

  Find (miss):
                    std::set:    10.15 ns/op (+/-  1.24, CI:[10.52,11.60])
              fat_p::FlatSet:     5.76 ns/op (+/-  0.84, CI:[5.55,6.29])
             boost::flat_set:     8.08 ns/op (+/-  7.76, CI:[6.93,13.73])
             absl::btree_set:     4.46 ns/op (+/-  0.74, CI:[4.26,4.91])

  Iteration:
                    std::set:     4.73 ns/op (+/-  0.64, CI:[4.86,5.42])
              fat_p::FlatSet:     0.86 ns/op (+/-  0.11, CI:[0.89,0.98])
             boost::flat_set:     0.56 ns/op (+/-  0.07, CI:[0.57,0.64])
             absl::btree_set:     1.47 ns/op (+/-  0.18, CI:[1.51,1.67])

  lower_bound:
                    std::set:    16.50 ns/op (+/-  0.28, CI:[16.47,16.72])
              fat_p::FlatSet:     8.89 ns/op (+/-  2.14, CI:[8.80,10.68])
             boost::flat_set:    11.49 ns/op (+/-  1.79, CI:[11.33,12.90])
             absl::btree_set:    15.30 ns/op (+/-  2.48, CI:[15.03,17.20])

--- N = 10000 ---
[2026-02-16 04:14:51] CPU: 3234 MHz (~base: 3234)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::set:    25.89 ns/op (+/-  2.87, CI:[23.66,26.17])
              fat_p::FlatSet:     1.28 ns/op (+/-  0.02, CI:[1.27,1.29])
             boost::flat_set:     0.18 ns/op (+/-  0.05, CI:[0.18,0.22])
             absl::btree_set:     8.82 ns/op (+/-  0.32, CI:[8.77,9.05])

  Bulk Insert (sorted):
                    std::set:    28.98 ns/op (+/-  5.78, CI:[26.44,31.50])
              fat_p::FlatSet:     3.63 ns/op (+/-  0.21, CI:[3.59,3.78])
             boost::flat_set:     3.46 ns/op (+/-  0.64, CI:[3.32,3.88])
             absl::btree_set:     8.86 ns/op (+/-  0.23, CI:[8.80,9.00])

  Bulk Insert (random):
                    std::set:   105.75 ns/op (+/-  2.05, CI:[105.43,107.23])
              fat_p::FlatSet:   311.70 ns/op (+/-  2.31, CI:[311.34,313.36])
             boost::flat_set:   310.87 ns/op (+/-  0.55, CI:[310.76,311.24])
             absl::btree_set:    73.97 ns/op (+/-  0.80, CI:[73.44,74.14])

  Find (hit):
                    std::set:    55.27 ns/op (+/-  1.14, CI:[54.92,55.92])
              fat_p::FlatSet:    62.24 ns/op (+/-  0.95, CI:[61.99,62.82])
             boost::flat_set:    62.33 ns/op (+/-  0.72, CI:[61.95,62.58])
             absl::btree_set:    45.87 ns/op (+/-  1.38, CI:[45.22,46.43])

  Find (miss):
                    std::set:    15.38 ns/op (+/-  0.08, CI:[15.33,15.40])
              fat_p::FlatSet:     9.21 ns/op (+/-  0.86, CI:[8.73,9.48])
             boost::flat_set:    10.49 ns/op (+/-  0.20, CI:[10.44,10.62])
             absl::btree_set:     3.09 ns/op (+/-  0.00, CI:[3.09,3.09])

  Iteration:
                    std::set:     5.29 ns/op (+/-  0.38, CI:[5.13,5.46])
              fat_p::FlatSet:     0.62 ns/op (+/-  0.19, CI:[0.58,0.75])
             boost::flat_set:     0.36 ns/op (+/-  0.00, CI:[0.36,0.37])
             absl::btree_set:     0.98 ns/op (+/-  0.21, CI:[0.94,1.13])

  lower_bound:
                    std::set:    30.64 ns/op (+/-  1.75, CI:[30.20,31.74])
              fat_p::FlatSet:    31.50 ns/op (+/-  0.67, CI:[31.40,31.99])
             boost::flat_set:    34.89 ns/op (+/-  0.76, CI:[34.61,35.27])
             absl::btree_set:    25.96 ns/op (+/-  0.67, CI:[25.89,26.48])

--- N = 100000 ---
[2026-02-16 04:15:17] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]

  Bulk Build (sorted range):
                    std::set:    25.85 ns/op (+/-  0.16, CI:[25.77,25.92])
              fat_p::FlatSet:     1.28 ns/op (+/-  0.04, CI:[1.27,1.30])
             boost::flat_set:     0.31 ns/op (+/-  0.04, CI:[0.31,0.34])
             absl::btree_set:     8.79 ns/op (+/-  0.04, CI:[8.77,8.80])

  Bulk Insert (sorted):
                    std::set:    28.65 ns/op (+/-  1.77, CI:[27.61,29.16])
              fat_p::FlatSet:     3.64 ns/op (+/-  0.05, CI:[3.62,3.66])
             boost::flat_set:     3.46 ns/op (+/-  0.05, CI:[3.47,3.51])
             absl::btree_set:     8.83 ns/op (+/-  0.04, CI:[8.81,8.84])

  Bulk Insert (random):
                    std::set:   180.01 ns/op (+/-  2.36, CI:[179.04,181.10])
              fat_p::FlatSet:  3204.33 ns/op (+/- 53.61, CI:[3192.95,3239.93])
             boost::flat_set:  3202.87 ns/op (+/- 19.86, CI:[3199.76,3217.16])
             absl::btree_set:    93.16 ns/op (+/-  0.74, CI:[92.72,93.37])

  Find (hit):
                    std::set:    96.91 ns/op (+/-  6.60, CI:[95.16,100.94])
              fat_p::FlatSet:    90.00 ns/op (+/-  0.63, CI:[89.83,90.38])
             boost::flat_set:    89.74 ns/op (+/-  0.76, CI:[89.66,90.33])
             absl::btree_set:    69.32 ns/op (+/-  0.55, CI:[69.07,69.55])

  Find (miss):
                    std::set:    23.72 ns/op (+/-  0.10, CI:[23.69,23.78])
              fat_p::FlatSet:     9.07 ns/op (+/-  0.93, CI:[8.84,9.65])
             boost::flat_set:    12.44 ns/op (+/-  0.04, CI:[12.44,12.48])
             absl::btree_set:     3.92 ns/op (+/-  0.13, CI:[3.89,4.01])

  Iteration:
                    std::set:     4.81 ns/op (+/-  0.29, CI:[4.79,5.04])
              fat_p::FlatSet:     0.62 ns/op (+/-  0.06, CI:[0.62,0.67])
             boost::flat_set:     0.35 ns/op (+/-  0.04, CI:[0.36,0.39])
             absl::btree_set:     0.98 ns/op (+/-  0.19, CI:[0.97,1.13])

  lower_bound:
                    std::set:    40.15 ns/op (+/-  1.45, CI:[39.66,40.93])
              fat_p::FlatSet:    42.95 ns/op (+/-  0.47, CI:[42.81,43.22])
             boost::flat_set:    45.74 ns/op (+/-  0.16, CI:[45.68,45.82])
             absl::btree_set:    35.07 ns/op (+/-  0.37, CI:[34.86,35.18])

================================================================================
  Memory Usage Comparison (Theoretical)
================================================================================
  For map<int64_t, int64_t>:

  Container                   Per-Entry Overhead    Total for N=10000
  -------------------------   -------------------   -----------------
  std::map                    ~40 bytes (tree node) ~400 KB + 160 KB data
  fat_p::FlatMap              ~0 bytes              ~160 KB (data only)
  boost::container::flat_map  ~0 bytes              ~160 KB (data only)
  absl::btree_map             ~2-4 bytes (B-tree)   ~180-200 KB

  Note: FlatMap has ~2.5x better memory efficiency than std::map.

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
--- N = 1000 ---
[2026-02-16 04:55:00] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]

  Bulk Build (sorted range):
                    std::map:    49.75 ns/op (+/-  0.69, CI:[49.65,50.26])
              fat_p::FlatMap:     1.70 ns/op (+/-  0.03, CI:[1.68,1.70])
             boost::flat_map:     1.90 ns/op (+/-  0.05, CI:[1.91,1.96])
             absl::btree_map:    23.15 ns/op (+/-  4.56, CI:[22.15,26.15])
    folly::sorted_vector_map:     7.80 ns/op (+/-  0.20, CI:[7.71,7.89])

  Bulk Insert (sorted):
                    std::map:    49.90 ns/op (+/-  0.57, CI:[49.82,50.31])
              fat_p::FlatMap:     6.20 ns/op (+/-  0.14, CI:[6.13,6.25])
             boost::flat_map:     7.50 ns/op (+/-  0.09, CI:[7.46,7.54])
             absl::btree_map:    37.15 ns/op (+/-  0.45, CI:[36.87,37.27])
    folly::sorted_vector_map:     7.80 ns/op (+/-  0.04, CI:[7.76,7.80])

  Bulk Insert (random):
                    std::map:   103.70 ns/op (+/-  7.79, CI:[102.46,109.29])
              fat_p::FlatMap:   155.10 ns/op (+/-  9.72, CI:[154.93,163.45])
             boost::flat_map:   127.60 ns/op (+/-  2.36, CI:[126.33,128.39])
             absl::btree_map:    76.50 ns/op (+/-  6.84, CI:[73.63,79.62])
    folly::sorted_vector_map:   157.60 ns/op (+/-  8.18, CI:[155.73,162.91])

  Find (hit):
                    std::map:    57.55 ns/op (+/-  3.03, CI:[55.84,58.50])
              fat_p::FlatMap:    45.70 ns/op (+/-  5.41, CI:[42.52,47.27])
             boost::flat_map:    47.40 ns/op (+/-  6.15, CI:[46.61,51.99])
             absl::btree_map:    34.65 ns/op (+/-  1.62, CI:[33.73,35.14])
    folly::sorted_vector_map:    44.25 ns/op (+/-  3.39, CI:[43.79,46.76])

  Find (miss):
                    std::map:     8.80 ns/op (+/-  6.15, CI:[7.77,13.16])
              fat_p::FlatMap:    10.20 ns/op (+/-  0.03, CI:[10.20,10.22])
             boost::flat_map:     7.50 ns/op (+/-  0.05, CI:[7.44,7.48])
             absl::btree_map:    27.20 ns/op (+/-  0.36, CI:[27.11,27.43])
    folly::sorted_vector_map:     7.10 ns/op (+/-  0.04, CI:[7.11,7.14])

  Iteration:
                    std::map:     4.60 ns/op (+/-  0.08, CI:[4.53,4.61])
              fat_p::FlatMap:     0.50 ns/op (+/-  0.04, CI:[0.46,0.50])
             boost::flat_map:     0.50 ns/op (+/-  0.05, CI:[0.44,0.49])
             absl::btree_map:     2.20 ns/op (+/-  0.22, CI:[2.19,2.39])
    folly::sorted_vector_map:     0.70 ns/op (+/-  5.26, CI:[-0.46,4.15])

  lower_bound:
                    std::map:    21.85 ns/op (+/-  2.35, CI:[21.19,23.25])
              fat_p::FlatMap:    22.30 ns/op (+/-  5.27, CI:[20.79,25.40])
             boost::flat_map:    18.30 ns/op (+/-  2.57, CI:[16.74,18.99])
             absl::btree_map:    18.35 ns/op (+/-  1.25, CI:[17.81,18.90])
    folly::sorted_vector_map:    18.85 ns/op (+/-  1.73, CI:[18.01,19.52])

--- N = 10000 ---
[2026-02-16 04:55:17] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]

  Bulk Build (sorted range):
                    std::map:    59.62 ns/op (+/-  2.03, CI:[59.41,61.19])
              fat_p::FlatMap:     2.00 ns/op (+/-  1.56, CI:[2.33,3.70])
             boost::flat_map:     1.87 ns/op (+/-  0.98, CI:[1.91,2.77])
             absl::btree_map:    27.54 ns/op (+/-  2.62, CI:[25.70,27.99])
    folly::sorted_vector_map:     7.79 ns/op (+/-  1.80, CI:[7.99,9.56])

  Bulk Insert (sorted):
                    std::map:    60.83 ns/op (+/-  2.13, CI:[59.74,61.61])
              fat_p::FlatMap:     6.25 ns/op (+/-  1.90, CI:[6.44,8.10])
             boost::flat_map:     7.51 ns/op (+/-  2.17, CI:[8.33,10.23])
             absl::btree_map:    41.32 ns/op (+/-  1.62, CI:[40.12,41.54])
    folly::sorted_vector_map:     7.79 ns/op (+/-  1.67, CI:[8.21,9.68])

  Bulk Insert (random):
                    std::map:   156.62 ns/op (+/-  5.85, CI:[155.01,160.14])
              fat_p::FlatMap:  1285.17 ns/op (+/- 44.08, CI:[1278.37,1317.01])
             boost::flat_map:   873.91 ns/op (+/- 33.90, CI:[866.78,896.49])
             absl::btree_map:   106.01 ns/op (+/-  5.30, CI:[101.49,106.14])
    folly::sorted_vector_map:  1288.26 ns/op (+/- 27.61, CI:[1283.51,1307.71])

  Find (hit):
                    std::map:   108.37 ns/op (+/-  3.66, CI:[107.45,110.67])
              fat_p::FlatMap:    72.66 ns/op (+/-  1.40, CI:[72.84,74.07])
             boost::flat_map:    74.87 ns/op (+/-  8.69, CI:[73.23,80.85])
             absl::btree_map:    54.21 ns/op (+/-  2.18, CI:[53.47,55.39])
    folly::sorted_vector_map:    73.97 ns/op (+/-  1.62, CI:[73.70,75.12])

  Find (miss):
                    std::map:    11.45 ns/op (+/-  0.44, CI:[11.36,11.74])
              fat_p::FlatMap:    14.50 ns/op (+/-  1.22, CI:[14.57,15.64])
             boost::flat_map:    10.18 ns/op (+/-  0.47, CI:[10.08,10.50])
             absl::btree_map:    30.23 ns/op (+/-  1.00, CI:[30.41,31.29])
    folly::sorted_vector_map:    10.18 ns/op (+/-  1.84, CI:[10.02,11.63])

  Iteration:
                    std::map:     7.27 ns/op (+/-  0.37, CI:[7.08,7.41])
              fat_p::FlatMap:     0.44 ns/op (+/-  0.01, CI:[0.44,0.45])
             boost::flat_map:     0.41 ns/op (+/-  0.01, CI:[0.41,0.42])
             absl::btree_map:     2.36 ns/op (+/-  0.08, CI:[2.35,2.42])
    folly::sorted_vector_map:     0.63 ns/op (+/-  0.01, CI:[0.62,0.63])

  lower_bound:
                    std::map:    53.08 ns/op (+/-  1.84, CI:[52.64,54.26])
              fat_p::FlatMap:    41.89 ns/op (+/-  0.72, CI:[41.72,42.35])
             boost::flat_map:    40.02 ns/op (+/-  1.01, CI:[40.02,40.91])
             absl::btree_map:    30.64 ns/op (+/-  0.84, CI:[30.27,31.01])
    folly::sorted_vector_map:    39.75 ns/op (+/- 11.63, CI:[37.45,47.65])

--- N = 100000 ---
[2026-02-16 04:55:34] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]

  Bulk Build (sorted range):
                    std::map:    72.28 ns/op (+/-  0.69, CI:[72.00,72.60])
              fat_p::FlatMap:     4.28 ns/op (+/-  0.16, CI:[4.22,4.36])
             boost::flat_map:     4.10 ns/op (+/-  0.22, CI:[4.08,4.28])
             absl::btree_map:    27.55 ns/op (+/-  1.74, CI:[27.32,28.84])
    folly::sorted_vector_map:    11.36 ns/op (+/-  0.12, CI:[11.32,11.43])

  Bulk Insert (sorted):
                    std::map:    72.68 ns/op (+/-  0.72, CI:[72.19,72.82])
              fat_p::FlatMap:     9.99 ns/op (+/-  0.20, CI:[9.92,10.09])
             boost::flat_map:    11.40 ns/op (+/-  0.17, CI:[11.33,11.48])
             absl::btree_map:    42.15 ns/op (+/-  0.93, CI:[41.58,42.39])
    folly::sorted_vector_map:    11.44 ns/op (+/-  0.15, CI:[11.39,11.51])

  Bulk Insert (random):
                    std::map:   260.83 ns/op (+/- 21.91, CI:[257.52,276.72])
              fat_p::FlatMap: 12832.81 ns/op (+/-159.37, CI:[12823.00,12962.69])
             boost::flat_map:  8309.13 ns/op (+/-304.36, CI:[8272.09,8538.88])
             absl::btree_map:   132.44 ns/op (+/-  3.99, CI:[131.80,135.29])
    folly::sorted_vector_map: 12856.62 ns/op (+/-181.13, CI:[12863.21,13021.97])

  Find (hit):
                    std::map:   211.91 ns/op (+/-  8.83, CI:[209.37,217.11])
              fat_p::FlatMap:   114.70 ns/op (+/-  3.66, CI:[114.44,117.65])
             boost::flat_map:   115.59 ns/op (+/-  1.15, CI:[115.45,116.46])
             absl::btree_map:    83.11 ns/op (+/-  1.12, CI:[82.78,83.76])
    folly::sorted_vector_map:   114.80 ns/op (+/-  1.76, CI:[114.65,116.19])

  Find (miss):
                    std::map:    15.69 ns/op (+/-  0.46, CI:[15.59,16.00])
              fat_p::FlatMap:    17.69 ns/op (+/-  0.56, CI:[17.67,18.16])
             boost::flat_map:    12.37 ns/op (+/-  0.28, CI:[12.24,12.49])
             absl::btree_map:    31.25 ns/op (+/-  0.33, CI:[31.20,31.49])
    folly::sorted_vector_map:    12.37 ns/op (+/-  0.84, CI:[12.12,12.86])

  Iteration:
                    std::map:    10.25 ns/op (+/-  4.40, CI:[9.64,13.50])
              fat_p::FlatMap:     0.45 ns/op (+/-  0.09, CI:[0.46,0.54])
             boost::flat_map:     0.43 ns/op (+/-  0.08, CI:[0.44,0.51])
             absl::btree_map:     2.68 ns/op (+/-  0.19, CI:[2.65,2.81])
    folly::sorted_vector_map:     0.63 ns/op (+/-  0.05, CI:[0.63,0.67])

  lower_bound:
                    std::map:   101.08 ns/op (+/-  3.34, CI:[100.93,103.86])
              fat_p::FlatMap:    58.32 ns/op (+/-  2.82, CI:[58.13,60.60])
             boost::flat_map:    55.73 ns/op (+/-  2.58, CI:[55.32,57.58])
             absl::btree_map:    43.05 ns/op (+/-  0.46, CI:[42.90,43.31])
    folly::sorted_vector_map:    55.23 ns/op (+/-  0.26, CI:[55.11,55.33])
[Cooling: before pathological insert] [Ready: 2445 MHz]

================================================================================
  SECTION 2: Pathological Random Insert (FlatMap's Weakness)
================================================================================
  This benchmark measures single random insertions into a populated container.
  For flat containers, each insert may require shifting O(n) elements.

[2026-02-16 04:57:19] Section start CPU: 2445 MHz (base: 2445)

--- Base size: 1000, inserting 100 missing keys ---
[2026-02-16 04:57:19] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Single random insert (into populated map):
                    std::map:    54.50 ns/op (+/- 78.05, CI:[46.49,114.91])
              fat_p::FlatMap:   271.00 ns/op (+/-  1.62, CI:[270.39,271.81])
             boost::flat_map:   187.00 ns/op (+/-  1.96, CI:[186.34,188.06])
             absl::btree_map:    55.50 ns/op (+/-  2.63, CI:[55.05,57.35])

--- Base size: 5000, inserting 100 missing keys ---
[2026-02-16 04:57:22] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Single random insert (into populated map):
                    std::map:    90.00 ns/op (+/- 29.90, CI:[89.25,115.45])
              fat_p::FlatMap:  1217.50 ns/op (+/-169.04, CI:[1208.47,1356.63])
             boost::flat_map:   811.00 ns/op (+/- 68.48, CI:[805.54,865.56])
             absl::btree_map:   116.50 ns/op (+/- 47.29, CI:[114.73,156.17])

--- Base size: 10000, inserting 100 missing keys ---
[2026-02-16 04:57:24] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Single random insert (into populated map):
                    std::map:   107.00 ns/op (+/- 45.09, CI:[102.59,142.11])
              fat_p::FlatMap:  2367.50 ns/op (+/-113.81, CI:[2373.77,2473.53])
             boost::flat_map:  1531.00 ns/op (+/-  2.19, CI:[1530.64,1532.56])
             absl::btree_map:   140.50 ns/op (+/- 58.55, CI:[134.24,185.56])
[Cooling: before iteration benchmark] [Ready: 2445 MHz]

================================================================================
  SECTION 3: Iteration Speed (FlatMap's Strength)
================================================================================
  FlatMap stores elements contiguously, enabling hardware prefetching.
  std::map requires pointer chasing through scattered tree nodes.

[2026-02-16 04:57:31] Section start CPU: 2445 MHz (base: 2445)

--- N = 1000 ---
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration (ns/element):
                    std::map:     4.70 ns/elem (+/-0.35)
              fat_p::FlatMap:     0.50 ns/elem (+/-0.15)
             boost::flat_map:     0.50 ns/elem (+/-0.07)
             absl::btree_map:     2.30 ns/elem (+/-0.11)

--- N = 10000 ---
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration (ns/element):
                    std::map:     6.48 ns/elem (+/-0.35)
              fat_p::FlatMap:     0.44 ns/elem (+/-0.01)
             boost::flat_map:     0.41 ns/elem (+/-0.01)
             absl::btree_map:     2.31 ns/elem (+/-0.14)

--- N = 100000 ---
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration (ns/element):
                    std::map:     6.80 ns/elem (+/-1.42)
              fat_p::FlatMap:     0.44 ns/elem (+/-0.00)
             boost::flat_map:     0.42 ns/elem (+/-0.04)
             absl::btree_map:     2.56 ns/elem (+/-0.14)

--- N = 1000000 ---
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration (ns/element):
                    std::map:    23.85 ns/elem (+/-0.50)
              fat_p::FlatMap:     0.52 ns/elem (+/-0.05)
             boost::flat_map:     0.77 ns/elem (+/-0.16)
             absl::btree_map:     3.80 ns/elem (+/-0.65)
[Cooling: before set operations] [Ready: 2445 MHz]

================================================================================
  SECTION 4: FlatSet Core Operations
================================================================================
[2026-02-16 04:57:51] Section start CPU: 2445 MHz (base: 2445)

--- N = 1000 ---
[2026-02-16 04:57:51] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]

  Bulk Build (sorted range):
                    std::set:    49.55 ns/op (+/-  3.73, CI:[48.81,52.09])
              fat_p::FlatSet:     1.60 ns/op (+/-  0.05, CI:[1.62,1.66])
             boost::flat_set:     0.40 ns/op (+/-  0.12, CI:[0.38,0.49])
             absl::btree_set:    25.50 ns/op (+/-  2.25, CI:[25.02,27.00])

  Bulk Insert (sorted):
                    std::set:    49.80 ns/op (+/-  0.52, CI:[49.72,50.17])
              fat_p::FlatSet:     2.70 ns/op (+/-  0.04, CI:[2.70,2.74])
             boost::flat_set:     5.30 ns/op (+/-  5.92, CI:[4.06,9.24])
             absl::btree_set:    21.60 ns/op (+/-  0.53, CI:[21.50,21.97])

  Bulk Insert (random):
                    std::set:    99.10 ns/op (+/-  2.95, CI:[98.03,100.61])
              fat_p::FlatSet:    82.30 ns/op (+/-  4.53, CI:[81.15,85.13])
             boost::flat_set:    87.85 ns/op (+/-  5.84, CI:[85.66,90.78])
             absl::btree_set:    66.60 ns/op (+/-  3.40, CI:[65.51,68.49])

  Find (hit):
                    std::set:    49.70 ns/op (+/-  7.15, CI:[47.67,53.94])
              fat_p::FlatSet:    37.20 ns/op (+/-  6.56, CI:[34.42,40.18])
             boost::flat_set:    35.25 ns/op (+/-  3.45, CI:[33.71,36.73])
             absl::btree_set:    38.85 ns/op (+/-  0.98, CI:[38.52,39.38])

  Find (miss):
                    std::set:     8.90 ns/op (+/-  0.13, CI:[8.89,9.00])
              fat_p::FlatSet:     7.10 ns/op (+/-  0.18, CI:[7.09,7.24])
             boost::flat_set:     7.40 ns/op (+/-  0.05, CI:[7.41,7.46])
             absl::btree_set:    29.60 ns/op (+/-  0.58, CI:[29.34,29.85])

  Iteration:
                    std::set:     4.30 ns/op (+/-  0.39, CI:[4.24,4.58])
              fat_p::FlatSet:     0.70 ns/op (+/-  0.02, CI:[0.69,0.70])
             boost::flat_set:     0.70 ns/op (+/-  0.05, CI:[0.64,0.68])
             absl::btree_set:     1.60 ns/op (+/-  0.05, CI:[1.56,1.61])

  lower_bound:
                    std::set:    13.25 ns/op (+/-  2.92, CI:[13.38,15.95])
              fat_p::FlatSet:    12.30 ns/op (+/-  7.44, CI:[11.39,17.91])
             boost::flat_set:    12.45 ns/op (+/-  2.08, CI:[12.32,14.14])
             absl::btree_set:    17.60 ns/op (+/-  1.30, CI:[17.54,18.67])

--- N = 10000 ---
[2026-02-16 04:58:07] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]

  Bulk Build (sorted range):
                    std::set:    48.97 ns/op (+/-  1.03, CI:[49.05,49.96])
              fat_p::FlatSet:     1.55 ns/op (+/-  0.40, CI:[1.47,1.82])
             boost::flat_set:     0.33 ns/op (+/-  0.01, CI:[0.33,0.33])
             absl::btree_set:    25.05 ns/op (+/-  0.58, CI:[25.01,25.52])

  Bulk Insert (sorted):
                    std::set:    49.56 ns/op (+/-  2.55, CI:[49.85,52.09])
              fat_p::FlatSet:     2.67 ns/op (+/-  0.02, CI:[2.66,2.68])
             boost::flat_set:     5.31 ns/op (+/-  0.03, CI:[5.31,5.33])
             absl::btree_set:    21.38 ns/op (+/-  0.06, CI:[21.35,21.41])

  Bulk Insert (random):
                    std::set:   144.12 ns/op (+/-  3.67, CI:[143.19,146.41])
              fat_p::FlatSet:   480.54 ns/op (+/- 28.55, CI:[479.04,504.07])
             boost::flat_set:   484.65 ns/op (+/- 30.56, CI:[483.33,510.11])
             absl::btree_set:    84.39 ns/op (+/-  1.73, CI:[83.36,84.88])

  Find (hit):
                    std::set:   101.81 ns/op (+/-  2.43, CI:[100.16,102.29])
              fat_p::FlatSet:    70.39 ns/op (+/-  1.41, CI:[70.45,71.69])
             boost::flat_set:    70.94 ns/op (+/-  1.56, CI:[71.29,72.66])
             absl::btree_set:    52.58 ns/op (+/-  0.95, CI:[52.55,53.38])

  Find (miss):
                    std::set:    11.98 ns/op (+/-  0.98, CI:[11.83,12.69])
              fat_p::FlatSet:    10.18 ns/op (+/-  0.02, CI:[10.17,10.19])
             boost::flat_set:    10.18 ns/op (+/-  0.95, CI:[10.15,10.98])
             absl::btree_set:    29.61 ns/op (+/-  0.80, CI:[29.70,30.40])

  Iteration:
                    std::set:     5.25 ns/op (+/-  0.27, CI:[5.22,5.46])
              fat_p::FlatSet:     0.63 ns/op (+/-  0.01, CI:[0.62,0.63])
             boost::flat_set:     0.63 ns/op (+/-  0.77, CI:[0.54,1.22])
             absl::btree_set:     1.58 ns/op (+/-  0.26, CI:[1.53,1.75])

  lower_bound:
                    std::set:    50.71 ns/op (+/-  1.11, CI:[50.36,51.33])
              fat_p::FlatSet:    38.89 ns/op (+/-  1.61, CI:[39.01,40.42])
             boost::flat_set:    39.06 ns/op (+/-  1.24, CI:[39.14,40.23])
             absl::btree_set:    30.64 ns/op (+/-  1.24, CI:[29.46,30.54])

--- N = 100000 ---
[2026-02-16 04:58:24] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]

  Bulk Build (sorted range):
                    std::set:    49.63 ns/op (+/-  3.99, CI:[48.80,52.30])
              fat_p::FlatSet:     1.56 ns/op (+/-  0.45, CI:[1.53,1.92])
             boost::flat_set:     0.34 ns/op (+/-  0.05, CI:[0.33,0.38])
             absl::btree_set:    25.33 ns/op (+/-  0.30, CI:[25.30,25.56])

  Bulk Insert (sorted):
                    std::set:    49.97 ns/op (+/-  6.35, CI:[50.24,55.81])
              fat_p::FlatSet:     2.70 ns/op (+/-  0.65, CI:[2.70,3.27])
             boost::flat_set:     5.40 ns/op (+/-  0.52, CI:[5.31,5.77])
             absl::btree_set:    21.57 ns/op (+/-  0.80, CI:[21.58,22.28])

  Bulk Insert (random):
                    std::set:   231.42 ns/op (+/-  7.48, CI:[232.16,238.72])
              fat_p::FlatSet:  4493.01 ns/op (+/- 53.15, CI:[4485.97,4532.56])
             boost::flat_set:  4503.59 ns/op (+/- 30.86, CI:[4495.58,4522.63])
             absl::btree_set:   106.64 ns/op (+/-  1.67, CI:[106.45,107.91])

  Find (hit):
                    std::set:   203.40 ns/op (+/-  5.96, CI:[199.80,205.02])
              fat_p::FlatSet:   100.96 ns/op (+/-  0.33, CI:[100.87,101.16])
             boost::flat_set:   101.41 ns/op (+/-  1.27, CI:[101.10,102.22])
             absl::btree_set:    78.47 ns/op (+/-  2.28, CI:[78.07,80.07])

  Find (miss):
                    std::set:    16.26 ns/op (+/-  0.16, CI:[16.18,16.32])
              fat_p::FlatSet:    12.29 ns/op (+/-  0.17, CI:[12.16,12.32])
             boost::flat_set:    12.33 ns/op (+/-  0.20, CI:[12.20,12.37])
             absl::btree_set:    30.57 ns/op (+/-  0.21, CI:[30.53,30.72])

  Iteration:
                    std::set:     5.90 ns/op (+/-  1.16, CI:[5.71,6.72])
              fat_p::FlatSet:     0.62 ns/op (+/-  0.08, CI:[0.62,0.70])
             boost::flat_set:     0.62 ns/op (+/-  0.01, CI:[0.62,0.63])
             absl::btree_set:     1.70 ns/op (+/-  0.08, CI:[1.70,1.77])

  lower_bound:
                    std::set:    95.83 ns/op (+/-  1.97, CI:[95.59,97.32])
              fat_p::FlatSet:    51.72 ns/op (+/-  0.42, CI:[51.65,52.02])
             boost::flat_set:    51.93 ns/op (+/-  0.24, CI:[51.87,52.08])
             absl::btree_set:    37.97 ns/op (+/-  0.65, CI:[37.87,38.45])

================================================================================
  Memory Usage Comparison (Theoretical)
================================================================================
  For map<int64_t, int64_t>:

  Container                   Per-Entry Overhead    Total for N=10000
  -------------------------   -------------------   -----------------
  std::map                    ~40 bytes (tree node) ~400 KB + 160 KB data
  fat_p::FlatMap              ~0 bytes              ~160 KB (data only)
  boost::container::flat_map  ~0 bytes              ~160 KB (data only)
  absl::btree_map             ~2-4 bytes (B-tree)   ~180-200 KB

  Note: FlatMap has ~2.5x better memory efficiency than std::map.

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- folly::sorted_vector_map was not detected on Local.
- std::flat_map / std::flat_set (C++23 not available) was not detected on Clang.
- std::flat_map / std::flat_set (C++23 not available) was not detected on GCC.
- std::flat_map / std::flat_set (C++23 not available) was not detected on Local.
- std::flat_map / std::flat_set (C++23 not available) was not detected on MSVC CI.
