---
doc_id: BR-SlotMap-001
doc_type: "Benchmark Results"
title: "SlotMap"
fatp_components: ["SlotMap"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - SlotMap

**Source:** `benchmark_SlotMap.cpp`
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
| fat_p::SlotMap | x | x | x | x |
| entt::registry | x | x | x | x |
| plf::hive | x | x | x | — |
| sg14::slot_map | x | x | x | x |
| std::unordered_map | x | x | x | x |
| std::map | x | x | x | x |
| std::vector (raw) | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  SECTION 1: Core Operations (Insert, Access, Erase)
================================================================================
[2026-02-15 19:55:45] Section start CPU: 2469 MHz (base: 3686)

--- N = 1000 ---
[2026-02-15 19:55:45] CPU: 2469 MHz (base: 3686)
[Cooling: size transition] [Ready: 2580 MHz]
  Sequential Insert:
              fat_p::SlotMap:    10.30 ns/op (+/-  0.10)
              entt::registry:    18.40 ns/op (+/-  1.21)
                   plf::hive:     6.60 ns/op (+/- 73.68)
              sg14::slot_map:    13.40 ns/op (+/-  1.76)
          std::unordered_map:    19.60 ns/op (+/-  0.67)
                    std::map:    32.40 ns/op (+/-  0.84)
           std::vector (raw):     8.00 ns/op (+/-  0.07)
  Random Access (valid):
              fat_p::SlotMap:     2.00 ns/op (+/-  0.05)
              entt::registry:     5.00 ns/op (+/-  0.05)
                   plf::hive:     1.70 ns/op (+/-  0.05)
              sg14::slot_map:     2.10 ns/op (+/-  0.04)
          std::unordered_map:     5.70 ns/op (+/-  0.31)
                    std::map:    33.10 ns/op (+/-  4.26)
           std::vector (raw):     1.60 ns/op (+/-  0.06)
  Erase (25%):
              fat_p::SlotMap:     5.60 ns/op (+/-  0.42)
              entt::registry:    32.40 ns/op (+/-  0.28)
                   plf::hive:     7.20 ns/op (+/-  0.58)
              sg14::slot_map:     5.20 ns/op (+/-  0.28)
          std::unordered_map:    16.40 ns/op (+/-  0.72)
                    std::map:    48.40 ns/op (+/-  7.89)
           std::vector (raw):     1.60 ns/op (+/-  0.26)

--- N = 10000 ---
[2026-02-15 19:56:32] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
  Sequential Insert:
              fat_p::SlotMap:    14.53 ns/op (+/-  3.01)
              entt::registry:    32.21 ns/op (+/-  9.65)
                   plf::hive:    10.80 ns/op (+/-  2.37)
              sg14::slot_map:    17.83 ns/op (+/-  2.06)
          std::unordered_map:    43.96 ns/op (+/-  2.73)
                    std::map:    65.08 ns/op (+/-  9.45)
           std::vector (raw):    12.25 ns/op (+/-  4.97)
  Random Access (valid):
              fat_p::SlotMap:     2.68 ns/op (+/-  0.14)
              entt::registry:     6.06 ns/op (+/-  0.16)
                   plf::hive:     2.04 ns/op (+/-  0.34)
              sg14::slot_map:     2.81 ns/op (+/-  0.21)
          std::unordered_map:    13.00 ns/op (+/-  0.40)
                    std::map:    69.02 ns/op (+/-  7.16)
           std::vector (raw):     1.71 ns/op (+/-  0.01)
  Erase (25%):
              fat_p::SlotMap:     7.20 ns/op (+/-  0.83)
              entt::registry:    39.04 ns/op (+/- 17.40)
                   plf::hive:     9.56 ns/op (+/- 26.96)
              sg14::slot_map:     6.04 ns/op (+/-  0.37)
          std::unordered_map:    28.64 ns/op (+/-  1.69)
                    std::map:   146.24 ns/op (+/- 10.35)
           std::vector (raw):     0.52 ns/op (+/-  0.06)

--- N = 100000 ---
[2026-02-15 19:56:59] CPU: 2432 MHz (base: 3686)
[Cooling: size transition] [Ready: 2764 MHz]
  Sequential Insert:
              fat_p::SlotMap:    14.65 ns/op (+/-  1.26)
              entt::registry:    30.28 ns/op (+/-  2.12)
                   plf::hive:    11.11 ns/op (+/-  0.93)
              sg14::slot_map:    17.58 ns/op (+/-  0.73)
          std::unordered_map:    54.05 ns/op (+/-  3.26)
                    std::map:    78.38 ns/op (+/-  2.74)
           std::vector (raw):    11.96 ns/op (+/-  1.27)
  Random Access (valid):
              fat_p::SlotMap:    11.57 ns/op (+/-  1.89)
              entt::registry:    16.51 ns/op (+/-  2.22)
                   plf::hive:    10.13 ns/op (+/-  1.57)
              sg14::slot_map:     9.01 ns/op (+/-  1.82)
          std::unordered_map:    17.84 ns/op (+/-  0.92)
                    std::map:   170.96 ns/op (+/-  5.71)
           std::vector (raw):     6.50 ns/op (+/-  1.34)
  Erase (25%):
              fat_p::SlotMap:    32.19 ns/op (+/-  5.79)
              entt::registry:    66.69 ns/op (+/-  5.28)
                   plf::hive:    26.58 ns/op (+/-  8.41)
              sg14::slot_map:    20.26 ns/op (+/-  5.01)
          std::unordered_map:    52.81 ns/op (+/-  6.18)
                    std::map:   290.85 ns/op (+/- 18.17)
           std::vector (raw):     0.50 ns/op (+/-  0.16)
[Cooling: before ABA safety test] [Ready: 2395 MHz]

================================================================================
  SECTION 2: ABA Safety Test (Generational Index Validation)
================================================================================
[2026-02-15 19:57:44] Section start CPU: 2395 MHz (base: 3686)

  Testing fat_p::SlotMap ABA protection:
    Erased handles correctly invalidated: 5000/5000 [PASS]
    Old handles invalid after slot reuse: 5000/5000 [PASS]
    New handles valid: 10000/10000 [PASS]

  Testing entt::registry ABA protection:
    Destroyed entities correctly invalidated: 5000/5000 [PASS]
[Cooling: before iteration benchmark] [Ready: 2395 MHz]

================================================================================
  SECTION 3: Iteration Speed (Dense Storage Advantage)
================================================================================
[2026-02-15 19:58:01] Section start CPU: 2395 MHz (base: 3686)

--- N = 1000 ---
[2026-02-15 19:58:01] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2322 MHz]
  Iteration:
              fat_p::SlotMap:     1.50 ns/op (+/-  2.15)
              entt::registry:     1.70 ns/op (+/-  0.14)
                   plf::hive:     1.80 ns/op (+/-  0.11)
          std::unordered_map:     2.20 ns/op (+/-  0.22)
                    std::map:     3.30 ns/op (+/-  0.22)
           std::vector (raw):     1.50 ns/op (+/-  0.11)

--- N = 10000 ---
[2026-02-15 19:58:06] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2469 MHz]
  Iteration:
              fat_p::SlotMap:     1.22 ns/op (+/-  0.16)
              entt::registry:     1.40 ns/op (+/-  0.17)
                   plf::hive:     1.45 ns/op (+/-  0.15)
          std::unordered_map:     2.92 ns/op (+/-  0.21)
                    std::map:     3.97 ns/op (+/-  0.15)
           std::vector (raw):     1.22 ns/op (+/-  0.25)

--- N = 100000 ---
[2026-02-15 19:58:13] CPU: 2432 MHz (base: 3686)
[Cooling: size transition] [Ready: 2469 MHz]
  Iteration:
              fat_p::SlotMap:     1.97 ns/op (+/-  0.33)
              entt::registry:     1.99 ns/op (+/-  0.84)
                   plf::hive:     2.26 ns/op (+/-  1.20)
          std::unordered_map:     6.31 ns/op (+/-  1.04)
                    std::map:    10.42 ns/op (+/-  5.21)
           std::vector (raw):     2.29 ns/op (+/-  0.78)

--- N = 500000 ---
[2026-02-15 19:58:17] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2432 MHz]
  Iteration:
              fat_p::SlotMap:     1.90 ns/op (+/-  0.34)
              entt::registry:     3.00 ns/op (+/-  0.70)
                   plf::hive:     2.87 ns/op (+/-  0.76)
          std::unordered_map:    24.26 ns/op (+/-  2.98)
                    std::map:    18.89 ns/op (+/-  3.98)
           std::vector (raw):     2.63 ns/op (+/-  0.66)
[Cooling: before mixed workload] [Ready: 2359 MHz]

================================================================================
  SECTION 4: Mixed Workload (Insert/Access/Erase Interleaved)
================================================================================
[2026-02-15 19:58:52] Section start CPU: 2359 MHz (base: 3686)

--- N = 1000 ---
[2026-02-15 19:58:52] CPU: 2359 MHz (base: 3686)
[Cooling: size transition] [Ready: 2248 MHz]
  Mixed Workload:
              fat_p::SlotMap:     9.20 ns/op (+/-  7.96)
              entt::registry:    30.00 ns/op (+/-  3.34)
                   plf::hive:     7.60 ns/op (+/-  0.62)
          std::unordered_map:    17.20 ns/op (+/-  2.29)
                    std::map:    22.80 ns/op (+/- 24.82)
           std::vector (raw):     8.00 ns/op (+/-  0.58)

--- N = 10000 ---
[2026-02-15 19:58:59] CPU: 2248 MHz (base: 3686)
[Cooling: size transition] [Ready: 2580 MHz]
  Mixed Workload:
              fat_p::SlotMap:     7.28 ns/op (+/-  0.67)
              entt::registry:    16.64 ns/op (+/-  0.82)
                   plf::hive:     6.44 ns/op (+/-  0.27)
          std::unordered_map:    16.16 ns/op (+/- 16.54)
                    std::map:    48.20 ns/op (+/- 10.93)
           std::vector (raw):     5.80 ns/op (+/-  0.76)

--- N = 50000 ---
[2026-02-15 19:59:08] CPU: 2653 MHz (base: 3686)
[Cooling: size transition] [Ready: 2764 MHz]
  Mixed Workload:
              fat_p::SlotMap:     9.74 ns/op (+/-  0.40)
              entt::registry:    18.65 ns/op (+/-  3.92)
                   plf::hive:     9.72 ns/op (+/-  2.34)
          std::unordered_map:    17.36 ns/op (+/-  0.74)
                    std::map:    65.95 ns/op (+/-  5.75)
           std::vector (raw):     8.34 ns/op (+/-  0.46)

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-element overhead (excluding value storage):
    fat_p::SlotMap:      ~12 bytes (slot: 8B + erase_map entry: 4B)
    entt::registry:      ~8-12 bytes (sparse set + component pool)
    plf::hive:           ~0-8 bytes (skipfield metadata, amortized)
    sg14::slot_map:      ~12 bytes (similar to fat_p)
    std::unordered_map:  ~8-16 bytes (bucket pointer + next pointer)
    std::map:            ~32-40 bytes (RB-tree node: color + 3 pointers)
    std::vector:         ~0 bytes (dense, but no handle safety)

  Feature comparison:
    Container             ABA-Safe  O(1) Access  Dense Iter  Stable Ptr
    -----------------------------------------------------------------------
    fat_p::SlotMap        Yes       Yes          Yes         No
    entt::registry        Yes       Yes          Yes         No
    plf::hive             No        No           Yes         Yes
    sg14::slot_map        Yes       Yes          Yes         No
    std::unordered_map    No        Yes*         No          Yes
    std::map              No        No           No          Yes
    std::vector           No        Yes          Yes         No
    (* average case)

  fat_p::SlotMap<TestValue> with 10000 elements:
    size():        10000
    capacity():    12138
    slot_count():  10000
    sizeof(TestValue): 40 bytes

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  SECTION 1: Core Operations (Insert, Access, Erase)
================================================================================
[2026-02-16 03:37:50] Section start CPU: 3247 MHz (~base: 3247)

--- N = 1000 ---
[2026-02-16 03:37:50] CPU: 3247 MHz (~base: 3247)
[Cooling: size transition] [Ready]
  Sequential Insert:
              fat_p::SlotMap:     9.96 ns/op (+/-  0.68)
              entt::registry:    31.01 ns/op (+/-  5.45)
                   plf::hive:     9.81 ns/op (+/-  4.02)
              sg14::slot_map:     9.44 ns/op (+/-  0.05)
          std::unordered_map:    22.82 ns/op (+/-  3.29)
                    std::map:    40.70 ns/op (+/-  3.85)
           std::vector (raw):     3.22 ns/op (+/-  0.53)
  Random Access (valid):
              fat_p::SlotMap:     4.13 ns/op (+/-  0.29)
              entt::registry:     9.79 ns/op (+/-  3.62)
                   plf::hive:     2.96 ns/op (+/-  0.16)
              sg14::slot_map:     4.39 ns/op (+/-  0.77)
          std::unordered_map:     5.63 ns/op (+/-  0.96)
                    std::map:    13.20 ns/op (+/-  4.85)
           std::vector (raw):     3.21 ns/op (+/-  0.19)
  Erase (25%):
              fat_p::SlotMap:    11.50 ns/op (+/-  0.52)
              entt::registry:    32.92 ns/op (+/-  1.64)
                   plf::hive:     9.72 ns/op (+/-  1.10)
              sg14::slot_map:    15.21 ns/op (+/-  0.56)
          std::unordered_map:    33.84 ns/op (+/- 11.13)
                    std::map:    44.68 ns/op (+/- 11.78)
           std::vector (raw):     3.73 ns/op (+/-  0.08)

--- N = 10000 ---
[2026-02-16 03:38:03] CPU: 2838 MHz (~base: 2838)
[Cooling: size transition] [Ready]
  Sequential Insert:
              fat_p::SlotMap:     9.53 ns/op (+/-  0.64)
              entt::registry:    25.78 ns/op (+/-  3.83)
                   plf::hive:     9.27 ns/op (+/-  0.52)
              sg14::slot_map:     8.96 ns/op (+/-  0.31)
          std::unordered_map:    22.12 ns/op (+/-  0.53)
                    std::map:    50.52 ns/op (+/-  2.61)
           std::vector (raw):     2.67 ns/op (+/-  0.01)
  Random Access (valid):
              fat_p::SlotMap:     6.53 ns/op (+/-  0.48)
              entt::registry:    14.06 ns/op (+/-  0.80)
                   plf::hive:     4.31 ns/op (+/-  0.43)
              sg14::slot_map:     7.34 ns/op (+/-  0.47)
          std::unordered_map:     9.21 ns/op (+/-  0.49)
                    std::map:   115.69 ns/op (+/-  1.84)
           std::vector (raw):     3.50 ns/op (+/-  0.03)
  Erase (25%):
              fat_p::SlotMap:    14.79 ns/op (+/-  0.49)
              entt::registry:    40.63 ns/op (+/-  2.12)
                   plf::hive:    12.82 ns/op (+/-  0.80)
              sg14::slot_map:    20.08 ns/op (+/-  1.10)
          std::unordered_map:    52.42 ns/op (+/-  1.56)
                    std::map:   160.99 ns/op (+/-  2.76)
           std::vector (raw):     1.54 ns/op (+/-  0.01)

--- N = 100000 ---
[2026-02-16 03:38:16] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Sequential Insert:
              fat_p::SlotMap:     9.74 ns/op (+/-  0.22)
              entt::registry:    25.38 ns/op (+/-  2.34)
                   plf::hive:     9.36 ns/op (+/-  0.19)
              sg14::slot_map:     9.01 ns/op (+/-  0.17)
          std::unordered_map:    22.27 ns/op (+/-  1.03)
                    std::map:    94.40 ns/op (+/-  2.04)
           std::vector (raw):     2.77 ns/op (+/-  0.21)
  Random Access (valid):
              fat_p::SlotMap:    11.78 ns/op (+/-  2.61)
              entt::registry:    21.24 ns/op (+/-  3.16)
                   plf::hive:     7.03 ns/op (+/-  1.96)
              sg14::slot_map:    13.44 ns/op (+/-  5.32)
          std::unordered_map:    15.92 ns/op (+/-  1.77)
                    std::map:   230.69 ns/op (+/- 33.29)
           std::vector (raw):     5.12 ns/op (+/-  3.29)
  Erase (25%):
              fat_p::SlotMap:    32.43 ns/op (+/-  5.72)
              entt::registry:    73.32 ns/op (+/-  5.03)
                   plf::hive:    20.81 ns/op (+/-  6.03)
              sg14::slot_map:    38.94 ns/op (+/-  4.90)
          std::unordered_map:    84.33 ns/op (+/-  5.92)
                    std::map:   297.83 ns/op (+/- 31.36)
           std::vector (raw):     1.31 ns/op (+/-  0.13)
[Cooling: before ABA safety test] [Ready]

================================================================================
  SECTION 2: ABA Safety Test (Generational Index Validation)
================================================================================
[2026-02-16 03:38:36] Section start CPU: 2621 MHz (~base: 2621)

  Testing fat_p::SlotMap ABA protection:
    Erased handles correctly invalidated: 5000/5000 [PASS]
    Old handles invalid after slot reuse: 5000/5000 [PASS]
    New handles valid: 10000/10000 [PASS]

  Testing entt::registry ABA protection:
    Destroyed entities correctly invalidated: 5000/5000 [PASS]
[Cooling: before iteration benchmark] [Ready]

================================================================================
  SECTION 3: Iteration Speed (Dense Storage Advantage)
================================================================================
[2026-02-16 03:38:40] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 ---
[2026-02-16 03:38:40] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     1.85 ns/op (+/-  0.06)
              entt::registry:     3.57 ns/op (+/-  0.02)
                   plf::hive:     2.56 ns/op (+/-  0.02)
          std::unordered_map:     2.98 ns/op (+/-  0.04)
                    std::map:     5.57 ns/op (+/-  0.26)
           std::vector (raw):     2.47 ns/op (+/-  0.13)

--- N = 10000 ---
[2026-02-16 03:38:43] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     1.34 ns/op (+/-  0.03)
              entt::registry:     2.97 ns/op (+/-  0.01)
                   plf::hive:     2.01 ns/op (+/-  0.04)
          std::unordered_map:     2.34 ns/op (+/-  0.21)
                    std::map:     5.52 ns/op (+/-  0.45)
           std::vector (raw):     1.93 ns/op (+/-  0.21)

--- N = 100000 ---
[2026-02-16 03:38:47] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     1.41 ns/op (+/-  0.19)
              entt::registry:     2.97 ns/op (+/-  0.07)
                   plf::hive:     1.95 ns/op (+/-  0.05)
          std::unordered_map:     2.30 ns/op (+/-  0.09)
                    std::map:     6.91 ns/op (+/-  1.32)
           std::vector (raw):     1.91 ns/op (+/-  0.08)

--- N = 500000 ---
[2026-02-16 03:38:51] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     2.15 ns/op (+/-  0.24)
              entt::registry:     3.19 ns/op (+/-  0.06)
                   plf::hive:     2.38 ns/op (+/-  0.09)
          std::unordered_map:     3.64 ns/op (+/-  0.15)
                    std::map:    12.93 ns/op (+/-  1.06)
           std::vector (raw):     2.29 ns/op (+/-  0.18)
[Cooling: before mixed workload] [Ready]

================================================================================
  SECTION 4: Mixed Workload (Insert/Access/Erase Interleaved)
================================================================================
[2026-02-16 03:39:02] Section start CPU: 2596 MHz (~base: 2596)

--- N = 1000 ---
[2026-02-16 03:39:02] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
  Mixed Workload:
              fat_p::SlotMap:    14.47 ns/op (+/-  1.11)
              entt::registry:    37.53 ns/op (+/-  5.34)
                   plf::hive:    13.68 ns/op (+/-  1.22)
          std::unordered_map:    31.10 ns/op (+/- 26.81)
                    std::map:    42.04 ns/op (+/-  6.45)
           std::vector (raw):     9.14 ns/op (+/-  0.62)

--- N = 10000 ---
[2026-02-16 03:39:05] CPU: 2606 MHz (~base: 2606)
[Cooling: size transition] [Ready]
  Mixed Workload:
              fat_p::SlotMap:     9.41 ns/op (+/-  0.07)
              entt::registry:    23.65 ns/op (+/-  2.75)
                   plf::hive:     9.75 ns/op (+/-  0.80)
          std::unordered_map:    21.59 ns/op (+/-  1.55)
                    std::map:    44.16 ns/op (+/-  4.97)
           std::vector (raw):     4.68 ns/op (+/-  0.05)

--- N = 50000 ---
[2026-02-16 03:39:09] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Mixed Workload:
              fat_p::SlotMap:    10.47 ns/op (+/-  0.58)
              entt::registry:    25.64 ns/op (+/-  0.67)
                   plf::hive:    11.01 ns/op (+/-  0.40)
          std::unordered_map:    23.14 ns/op (+/-  0.49)
                    std::map:    83.70 ns/op (+/-  0.61)
           std::vector (raw):     4.57 ns/op (+/-  0.18)

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-element overhead (excluding value storage):
    fat_p::SlotMap:      ~12 bytes (slot: 8B + erase_map entry: 4B)
    entt::registry:      ~8-12 bytes (sparse set + component pool)
    plf::hive:           ~0-8 bytes (skipfield metadata, amortized)
    sg14::slot_map:      ~12 bytes (similar to fat_p)
    std::unordered_map:  ~8-16 bytes (bucket pointer + next pointer)
    std::map:            ~32-40 bytes (RB-tree node: color + 3 pointers)
    std::vector:         ~0 bytes (dense, but no handle safety)

  Feature comparison:
    Container             ABA-Safe  O(1) Access  Dense Iter  Stable Ptr
    -----------------------------------------------------------------------
    fat_p::SlotMap        Yes       Yes          Yes         No
    entt::registry        Yes       Yes          Yes         No
    plf::hive             No        No           Yes         Yes
    sg14::slot_map        Yes       Yes          Yes         No
    std::unordered_map    No        Yes*         No          Yes
    std::map              No        No           No          Yes
    std::vector           No        Yes          Yes         No
    (* average case)

  fat_p::SlotMap<TestValue> with 10000 elements:
    size():        10000
    capacity():    16384
    slot_count():  10000
    sizeof(TestValue): 40 bytes

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  SECTION 1: Core Operations (Insert, Access, Erase)
================================================================================
[2026-02-16 04:11:20] Section start CPU: 3240 MHz (~base: 3240)

--- N = 1000 ---
[2026-02-16 04:11:20] CPU: 3240 MHz (~base: 3240)
[Cooling: size transition] [Ready]
  Sequential Insert:
              fat_p::SlotMap:     9.38 ns/op (+/-  0.16)
              entt::registry:    31.51 ns/op (+/-  5.89)
                   plf::hive:     8.82 ns/op (+/-  0.32)
              sg14::slot_map:     9.49 ns/op (+/-  0.06)
          std::unordered_map:    22.49 ns/op (+/-  2.52)
                    std::map:    71.87 ns/op (+/-  4.52)
           std::vector (raw):     3.16 ns/op (+/-  0.28)
  Random Access (valid):
              fat_p::SlotMap:     3.77 ns/op (+/-  0.02)
              entt::registry:     8.38 ns/op (+/-  0.03)
                   plf::hive:     2.83 ns/op (+/-  0.02)
              sg14::slot_map:     3.92 ns/op (+/-  0.05)
          std::unordered_map:     4.97 ns/op (+/-  0.04)
                    std::map:    25.59 ns/op (+/-  3.84)
           std::vector (raw):     3.16 ns/op (+/-  0.02)
  Erase (25%):
              fat_p::SlotMap:    13.24 ns/op (+/-  0.47)
              entt::registry:    39.93 ns/op (+/-  0.84)
                   plf::hive:    13.73 ns/op (+/-  1.53)
              sg14::slot_map:    13.37 ns/op (+/-  0.24)
          std::unordered_map:    33.16 ns/op (+/-  4.09)
                    std::map:    55.73 ns/op (+/- 25.93)
           std::vector (raw):     3.97 ns/op (+/-  0.04)

--- N = 10000 ---
[2026-02-16 04:11:34] CPU: 3246 MHz (~base: 3246)
[Cooling: size transition] [Ready]
  Sequential Insert:
              fat_p::SlotMap:     9.03 ns/op (+/-  0.26)
              entt::registry:    28.06 ns/op (+/-  3.55)
                   plf::hive:     8.32 ns/op (+/-  0.70)
              sg14::slot_map:     9.02 ns/op (+/-  0.31)
          std::unordered_map:    21.87 ns/op (+/-  0.32)
                    std::map:    96.82 ns/op (+/-  2.12)
           std::vector (raw):     2.65 ns/op (+/-  0.58)
  Random Access (valid):
              fat_p::SlotMap:     6.06 ns/op (+/-  0.19)
              entt::registry:    11.44 ns/op (+/-  0.33)
                   plf::hive:     4.24 ns/op (+/-  0.32)
              sg14::slot_map:     6.56 ns/op (+/-  0.43)
          std::unordered_map:     8.12 ns/op (+/-  0.15)
                    std::map:    77.58 ns/op (+/-  6.60)
           std::vector (raw):     3.61 ns/op (+/-  0.23)
  Erase (25%):
              fat_p::SlotMap:    18.27 ns/op (+/-  2.72)
              entt::registry:    50.15 ns/op (+/-  2.97)
                   plf::hive:    17.65 ns/op (+/-  1.00)
              sg14::slot_map:    18.33 ns/op (+/-  0.38)
          std::unordered_map:    52.48 ns/op (+/-  2.17)
                    std::map:   168.50 ns/op (+/-  4.46)
           std::vector (raw):     1.80 ns/op (+/-  0.01)

--- N = 100000 ---
[2026-02-16 04:11:47] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Sequential Insert:
              fat_p::SlotMap:     9.12 ns/op (+/-  0.10)
              entt::registry:    26.87 ns/op (+/-  2.23)
                   plf::hive:     8.41 ns/op (+/-  0.11)
              sg14::slot_map:     9.08 ns/op (+/-  0.06)
          std::unordered_map:    21.83 ns/op (+/-  0.16)
                    std::map:   153.28 ns/op (+/-  6.81)
           std::vector (raw):     2.75 ns/op (+/-  0.14)
  Random Access (valid):
              fat_p::SlotMap:     9.89 ns/op (+/-  0.92)
              entt::registry:    18.39 ns/op (+/-  1.22)
                   plf::hive:     7.20 ns/op (+/-  2.33)
              sg14::slot_map:    11.30 ns/op (+/-  2.86)
          std::unordered_map:    13.42 ns/op (+/-  0.82)
                    std::map:   141.83 ns/op (+/- 14.95)
           std::vector (raw):     4.34 ns/op (+/-  0.61)
  Erase (25%):
              fat_p::SlotMap:    38.28 ns/op (+/-  6.51)
              entt::registry:    82.06 ns/op (+/-  1.96)
                   plf::hive:    23.31 ns/op (+/-  1.36)
              sg14::slot_map:    35.72 ns/op (+/-  1.15)
          std::unordered_map:    77.90 ns/op (+/-  2.03)
                    std::map:   292.87 ns/op (+/- 16.39)
           std::vector (raw):     1.58 ns/op (+/-  0.01)
[Cooling: before ABA safety test] [Ready]

================================================================================
  SECTION 2: ABA Safety Test (Generational Index Validation)
================================================================================
[2026-02-16 04:12:07] Section start CPU: 2445 MHz (~base: 2445)

  Testing fat_p::SlotMap ABA protection:
    Erased handles correctly invalidated: 5000/5000 [PASS]
    Old handles invalid after slot reuse: 5000/5000 [PASS]
    New handles valid: 10000/10000 [PASS]

  Testing entt::registry ABA protection:
    Destroyed entities correctly invalidated: 5000/5000 [PASS]
[Cooling: before iteration benchmark] [Ready]

================================================================================
  SECTION 3: Iteration Speed (Dense Storage Advantage)
================================================================================
[2026-02-16 04:12:11] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 ---
[2026-02-16 04:12:11] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     1.67 ns/op (+/-  0.22)
              entt::registry:     3.26 ns/op (+/-  0.02)
                   plf::hive:     2.62 ns/op (+/-  0.15)
          std::unordered_map:     2.78 ns/op (+/-  0.03)
                    std::map:     5.75 ns/op (+/-  1.83)
           std::vector (raw):     2.67 ns/op (+/-  0.05)

--- N = 10000 ---
[2026-02-16 04:12:14] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     1.08 ns/op (+/-  0.03)
              entt::registry:     2.64 ns/op (+/-  0.20)
                   plf::hive:     2.06 ns/op (+/-  0.28)
          std::unordered_map:     2.22 ns/op (+/-  0.04)
                    std::map:     5.83 ns/op (+/-  0.45)
           std::vector (raw):     2.13 ns/op (+/-  0.21)

--- N = 100000 ---
[2026-02-16 04:12:18] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     1.16 ns/op (+/-  0.25)
              entt::registry:     2.63 ns/op (+/-  0.04)
                   plf::hive:     2.05 ns/op (+/-  0.07)
          std::unordered_map:     2.10 ns/op (+/-  0.07)
                    std::map:     6.57 ns/op (+/-  1.02)
           std::vector (raw):     2.10 ns/op (+/-  0.08)

--- N = 500000 ---
[2026-02-16 04:12:22] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Iteration:
              fat_p::SlotMap:     2.19 ns/op (+/-  0.25)
              entt::registry:     3.09 ns/op (+/-  0.10)
                   plf::hive:     2.66 ns/op (+/-  0.13)
          std::unordered_map:     3.88 ns/op (+/-  0.20)
                    std::map:    13.56 ns/op (+/-  0.55)
           std::vector (raw):     2.46 ns/op (+/-  0.19)
[Cooling: before mixed workload] [Ready]

================================================================================
  SECTION 4: Mixed Workload (Insert/Access/Erase Interleaved)
================================================================================
[2026-02-16 04:12:33] Section start CPU: 2629 MHz (~base: 2629)

--- N = 1000 ---
[2026-02-16 04:12:33] CPU: 2629 MHz (~base: 2629)
[Cooling: size transition] [Ready]
  Mixed Workload:
              fat_p::SlotMap:    15.17 ns/op (+/-  1.45)
              entt::registry:    42.32 ns/op (+/- 11.41)
                   plf::hive:    13.98 ns/op (+/-  1.30)
          std::unordered_map:    33.00 ns/op (+/-  3.48)
                    std::map:    49.51 ns/op (+/-  7.13)
           std::vector (raw):     9.76 ns/op (+/-  0.94)

--- N = 10000 ---
[2026-02-16 04:12:37] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Mixed Workload:
              fat_p::SlotMap:    10.41 ns/op (+/-  2.15)
              entt::registry:    24.81 ns/op (+/-  1.60)
                   plf::hive:    10.25 ns/op (+/-  1.09)
          std::unordered_map:    20.87 ns/op (+/-  3.71)
                    std::map:    59.52 ns/op (+/-  4.16)
           std::vector (raw):     5.14 ns/op (+/-  0.03)

--- N = 50000 ---
[2026-02-16 04:12:40] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
  Mixed Workload:
              fat_p::SlotMap:    11.85 ns/op (+/-  0.41)
              entt::registry:    26.41 ns/op (+/-  0.53)
                   plf::hive:    11.55 ns/op (+/-  0.59)
          std::unordered_map:    22.26 ns/op (+/-  0.53)
                    std::map:    91.35 ns/op (+/-  2.06)
           std::vector (raw):     5.06 ns/op (+/-  0.53)

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-element overhead (excluding value storage):
    fat_p::SlotMap:      ~12 bytes (slot: 8B + erase_map entry: 4B)
    entt::registry:      ~8-12 bytes (sparse set + component pool)
    plf::hive:           ~0-8 bytes (skipfield metadata, amortized)
    sg14::slot_map:      ~12 bytes (similar to fat_p)
    std::unordered_map:  ~8-16 bytes (bucket pointer + next pointer)
    std::map:            ~32-40 bytes (RB-tree node: color + 3 pointers)
    std::vector:         ~0 bytes (dense, but no handle safety)

  Feature comparison:
    Container             ABA-Safe  O(1) Access  Dense Iter  Stable Ptr
    -----------------------------------------------------------------------
    fat_p::SlotMap        Yes       Yes          Yes         No
    entt::registry        Yes       Yes          Yes         No
    plf::hive             No        No           Yes         Yes
    sg14::slot_map        Yes       Yes          Yes         No
    std::unordered_map    No        Yes*         No          Yes
    std::map              No        No           No          Yes
    std::vector           No        Yes          Yes         No
    (* average case)

  fat_p::SlotMap<TestValue> with 10000 elements:
    size():        10000
    capacity():    16384
    slot_count():  10000
    sizeof(TestValue): 40 bytes

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  SECTION 1: Core Operations (Insert, Access, Erase)
================================================================================
[2026-02-16 04:54:13] Section start CPU: 2445 MHz (base: 2445)

--- N = 1000 ---
[2026-02-16 04:54:13] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Sequential Insert:
              fat_p::SlotMap:    15.95 ns/op (+/-  0.18)
              entt::registry:    43.35 ns/op (+/-  0.96)
              sg14::slot_map:    22.80 ns/op (+/-  4.93)
          std::unordered_map:    45.20 ns/op (+/-  7.22)
                    std::map:    70.30 ns/op (+/-  1.42)
           std::vector (raw):    12.50 ns/op (+/-  0.18)
  Random Access (valid):
              fat_p::SlotMap:     4.30 ns/op (+/-  0.79)
              entt::registry:    13.70 ns/op (+/-  1.05)
              sg14::slot_map:     4.50 ns/op (+/-  0.06)
          std::unordered_map:    10.05 ns/op (+/-  0.81)
                    std::map:    19.50 ns/op (+/-  5.01)
           std::vector (raw):     3.40 ns/op (+/-  0.07)
  Erase (25%):
              fat_p::SlotMap:    12.40 ns/op (+/-  0.66)
              entt::registry:    88.00 ns/op (+/-  1.49)
              sg14::slot_map:    17.20 ns/op (+/-  0.59)
          std::unordered_map:    35.60 ns/op (+/-  5.46)
                    std::map:    63.00 ns/op (+/- 23.29)
           std::vector (raw):     3.60 ns/op (+/-  0.26)

--- N = 10000 ---
[2026-02-16 04:54:22] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Sequential Insert:
              fat_p::SlotMap:    23.77 ns/op (+/-  3.88)
              entt::registry:    59.60 ns/op (+/- 16.30)
              sg14::slot_map:    30.50 ns/op (+/-  2.15)
          std::unordered_map:    80.30 ns/op (+/-  4.45)
                    std::map:   113.96 ns/op (+/-  6.27)
           std::vector (raw):    19.76 ns/op (+/-  3.66)
  Random Access (valid):
              fat_p::SlotMap:     7.07 ns/op (+/-  0.31)
              entt::registry:    17.45 ns/op (+/-  1.46)
              sg14::slot_map:     7.76 ns/op (+/-  0.71)
          std::unordered_map:    27.80 ns/op (+/-  1.67)
                    std::map:   123.50 ns/op (+/-  2.23)
           std::vector (raw):     3.79 ns/op (+/-  0.79)
  Erase (25%):
              fat_p::SlotMap:    20.84 ns/op (+/-  1.22)
              entt::registry:   100.28 ns/op (+/-  4.98)
              sg14::slot_map:    23.58 ns/op (+/-  2.36)
          std::unordered_map:    69.62 ns/op (+/-  1.78)
                    std::map:   253.14 ns/op (+/-  9.89)
           std::vector (raw):     1.24 ns/op (+/-  0.03)

--- N = 100000 ---
[2026-02-16 04:54:31] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Sequential Insert:
              fat_p::SlotMap:    24.84 ns/op (+/-  1.37)
              entt::registry:    59.25 ns/op (+/-  4.12)
              sg14::slot_map:    29.80 ns/op (+/-  0.76)
          std::unordered_map:    92.73 ns/op (+/- 10.08)
                    std::map:   125.85 ns/op (+/-  4.59)
           std::vector (raw):    19.24 ns/op (+/-  0.57)
  Random Access (valid):
              fat_p::SlotMap:    16.75 ns/op (+/-  3.34)
              entt::registry:    31.21 ns/op (+/-  4.85)
              sg14::slot_map:    18.27 ns/op (+/-  2.57)
          std::unordered_map:    40.85 ns/op (+/-  7.70)
                    std::map:   282.37 ns/op (+/- 44.20)
           std::vector (raw):     9.03 ns/op (+/-  2.22)
  Erase (25%):
              fat_p::SlotMap:    46.43 ns/op (+/-  9.94)
              entt::registry:   153.86 ns/op (+/- 28.75)
              sg14::slot_map:    48.70 ns/op (+/-  6.68)
          std::unordered_map:   131.73 ns/op (+/- 43.87)
                    std::map:   503.99 ns/op (+/- 97.69)
           std::vector (raw):     1.06 ns/op (+/-  0.03)
[Cooling: before ABA safety test] [Ready: 2445 MHz]

================================================================================
  SECTION 2: ABA Safety Test (Generational Index Validation)
================================================================================
[2026-02-16 04:54:47] Section start CPU: 2445 MHz (base: 2445)

  Testing fat_p::SlotMap ABA protection:
    Erased handles correctly invalidated: 5000/5000 [PASS]
    Old handles invalid after slot reuse: 5000/5000 [PASS]
    New handles valid: 10000/10000 [PASS]

  Testing entt::registry ABA protection:
    Destroyed entities correctly invalidated: 5000/5000 [PASS]
[Cooling: before iteration benchmark] [Ready: 2445 MHz]

================================================================================
  SECTION 3: Iteration Speed (Dense Storage Advantage)
================================================================================
[2026-02-16 04:54:51] Section start CPU: 2445 MHz (base: 2445)

--- N = 1000 ---
[2026-02-16 04:54:51] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration:
              fat_p::SlotMap:     1.80 ns/op (+/-  0.19)
              entt::registry:     3.90 ns/op (+/-  0.05)
          std::unordered_map:     4.30 ns/op (+/-  0.97)
                    std::map:     5.80 ns/op (+/-  0.18)
           std::vector (raw):     3.10 ns/op (+/-  0.04)

--- N = 10000 ---
[2026-02-16 04:54:54] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration:
              fat_p::SlotMap:     1.19 ns/op (+/-  0.15)
              entt::registry:     3.34 ns/op (+/-  0.02)
          std::unordered_map:     9.68 ns/op (+/-  0.59)
                    std::map:     8.23 ns/op (+/-  0.27)
           std::vector (raw):     2.52 ns/op (+/-  0.01)

--- N = 100000 ---
[2026-02-16 04:54:56] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration:
              fat_p::SlotMap:     1.92 ns/op (+/-  0.23)
              entt::registry:     3.47 ns/op (+/-  0.16)
          std::unordered_map:    24.45 ns/op (+/-  7.91)
                    std::map:    30.95 ns/op (+/-  5.61)
           std::vector (raw):     2.68 ns/op (+/-  0.15)

--- N = 500000 ---
[2026-02-16 04:55:00] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Iteration:
              fat_p::SlotMap:     2.50 ns/op (+/-  0.10)
              entt::registry:     3.75 ns/op (+/-  0.40)
          std::unordered_map:    63.41 ns/op (+/-  2.48)
                    std::map:    38.87 ns/op (+/-  1.09)
           std::vector (raw):     3.11 ns/op (+/-  0.33)
[Cooling: before mixed workload] [Ready: 2445 MHz]

================================================================================
  SECTION 4: Mixed Workload (Insert/Access/Erase Interleaved)
================================================================================
[2026-02-16 04:55:14] Section start CPU: 2445 MHz (base: 2445)

--- N = 1000 ---
[2026-02-16 04:55:14] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Mixed Workload:
              fat_p::SlotMap:    15.20 ns/op (+/-  0.22)
              entt::registry:    63.20 ns/op (+/-  0.78)
          std::unordered_map:    34.00 ns/op (+/-  0.30)
                    std::map:    44.80 ns/op (+/- 34.54)
           std::vector (raw):    12.40 ns/op (+/-  0.22)

--- N = 10000 ---
[2026-02-16 04:55:17] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Mixed Workload:
              fat_p::SlotMap:    12.92 ns/op (+/-  0.08)
              entt::registry:    43.60 ns/op (+/-  3.10)
          std::unordered_map:    35.98 ns/op (+/-  3.28)
                    std::map:    74.86 ns/op (+/-  7.21)
           std::vector (raw):     9.32 ns/op (+/-  0.03)

--- N = 50000 ---
[2026-02-16 04:55:19] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
  Mixed Workload:
              fat_p::SlotMap:    17.81 ns/op (+/-  1.21)
              entt::registry:    43.89 ns/op (+/-  1.69)
          std::unordered_map:    43.34 ns/op (+/-  2.44)
                    std::map:   119.75 ns/op (+/-  2.34)
           std::vector (raw):    13.54 ns/op (+/-  1.10)

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-element overhead (excluding value storage):
    fat_p::SlotMap:      ~12 bytes (slot: 8B + erase_map entry: 4B)
    entt::registry:      ~8-12 bytes (sparse set + component pool)
    sg14::slot_map:      ~12 bytes (similar to fat_p)
    std::unordered_map:  ~8-16 bytes (bucket pointer + next pointer)
    std::map:            ~32-40 bytes (RB-tree node: color + 3 pointers)
    std::vector:         ~0 bytes (dense, but no handle safety)

  Feature comparison:
    Container             ABA-Safe  O(1) Access  Dense Iter  Stable Ptr
    -----------------------------------------------------------------------
    fat_p::SlotMap        Yes       Yes          Yes         No
    entt::registry        Yes       Yes          Yes         No
    sg14::slot_map        Yes       Yes          Yes         No
    std::unordered_map    No        Yes*         No          Yes
    std::map              No        No           No          Yes
    std::vector           No        Yes          Yes         No
    (* average case)

  fat_p::SlotMap<TestValue> with 10000 elements:
    size():        10000
    capacity():    12138
    slot_count():  10000
    sizeof(TestValue): 40 bytes

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- plf::hive was not detected on MSVC CI.
