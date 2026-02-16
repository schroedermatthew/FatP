---
doc_id: BR-StateMachine-001
doc_type: "Benchmark Results"
title: "StateMachine"
fatp_components: ["StateMachine"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - StateMachine

**Source:** `benchmark_StateMachine.cpp`
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
| fat_p::StateMachine<AnyToAnyPolicy> | x | x | x | x |
| fat_p::StateMachine<StrictPolicy> | x | x | x | x |
| Manual enum-switch | x | x | x | x |
| Manual fn-ptr table | x | x | x | x |
| std::variant + std::visit (C++17 baseline) | x | x | x | x |
| boost::sml | x | x | x | x |
| Boost.MSM | x | x | x | x |
| TinyFSM | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  Section 1: Core Transition Performance
================================================================================

Contract: transition() is O(1) for all implementations. All use 4 states with counting hooks.

[2026-02-15 19:59:45] CPU: 2617 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             5.87        6.13      0.72  [  5.77,   6.50] ns/op
fat_p Strict                               6.16        6.81      1.27  [  6.17,   7.45] ns/op
Manual enum-switch                         2.14        2.30      0.44  [  2.07,   2.52] ns/op
Manual fn-ptr table                        2.70        2.83      0.44  [  2.61,   3.05] ns/op
std::variant                               5.94        6.34      0.69  [  5.99,   6.69] ns/op
[Boost].SML                                6.20        6.48      0.54  [  6.20,   6.75] ns/op
TinyFSM                                    6.84        7.49      1.18  [  6.89,   8.09] ns/op
Boost.MSM                                 10.81       11.61      1.26  [ 10.97,  12.25] ns/op

[OK] Correctness validated for all 8 implementations

================================================================================
  Section 2a: Hook Overhead (Empty Hooks)
================================================================================

Contract: Baseline with empty on_entry/on_exit hooks. Measures pure dispatch overhead.

[2026-02-15 19:59:50] CPU: 2506 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (empty)                     6.37        6.66      1.04  [  6.13,   7.18] ns/op
fat_p Strict (empty)                       6.25        6.79      0.98  [  6.29,   7.29] ns/op
Manual enum-switch (empty)                 1.99        2.28      0.62  [  1.97,   2.59] ns/op
Manual fn-ptr table (empty)                2.71        2.91      0.42  [  2.70,   3.12] ns/op
std::variant (empty)                       5.67        5.97      0.58  [  5.68,   6.27] ns/op
[Boost].SML (empty)                        6.37        7.15      1.12  [  6.59,   7.72] ns/op
TinyFSM (empty)                            7.08        7.85      1.17  [  7.25,   8.44] ns/op
Boost.MSM (empty)                         11.22       11.83      1.36  [ 11.14,  12.52] ns/op

================================================================================
  Section 2b: Hook Overhead (Counting Hooks)
================================================================================

Contract: Hooks increment a counter. Measures dispatch + minimal work.

[2026-02-15 19:59:54] CPU: 3169 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             5.87        6.16      0.78  [  5.76,   6.55] ns/op
fat_p Strict                               6.24        6.73      1.05  [  6.20,   7.26] ns/op
Manual enum-switch                         2.16        2.50      0.58  [  2.20,   2.79] ns/op
Manual fn-ptr table                        2.66        2.76      0.26  [  2.63,   2.89] ns/op
std::variant                               5.95        6.56      0.94  [  6.08,   7.03] ns/op
[Boost].SML                                6.17        6.69      0.99  [  6.19,   7.20] ns/op
TinyFSM                                    6.84        8.09      1.70  [  7.23,   8.95] ns/op
Boost.MSM                                 10.79       11.74      1.39  [ 11.03,  12.44] ns/op

[OK] Correctness validated for all 8 implementations

================================================================================
  Section 3a: State Scaling (4 States)
================================================================================

Contract: 4-state machines with counting hooks. Baseline for scaling comparison.

[2026-02-15 19:59:59] CPU: 3022 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             5.89        6.64      1.06  [  6.11,   7.18] ns/op
fat_p Strict                               6.15        6.67      0.94  [  6.20,   7.15] ns/op
Manual enum-switch                         2.14        2.51      0.75  [  2.13,   2.89] ns/op
Manual fn-ptr table                        2.67        2.89      0.56  [  2.61,   3.17] ns/op
std::variant                               5.95        6.39      0.74  [  6.02,   6.77] ns/op
[Boost].SML                                6.47        7.03      1.36  [  6.34,   7.72] ns/op
TinyFSM                                    6.85        7.40      0.99  [  6.90,   7.90] ns/op
Boost.MSM                                 10.76       11.56      1.51  [ 10.79,  12.32] ns/op

[OK] Correctness validated for all 8 implementations

================================================================================
  Section 3b: State Scaling (8 States)
================================================================================

Contract: 8-state machines with counting hooks. O(1) claim: should match 4-state performance.

[2026-02-15 20:00:04] CPU: 3243 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (8-state)                   7.11        7.59      0.89  [  7.14,   8.03] ns/op
fat_p Strict (8-state)                     7.09        7.54      0.78  [  7.14,   7.93] ns/op
Manual enum-switch (8-state)               1.37        1.39      0.11  [  1.34,   1.45] ns/op
Manual fn-ptr table (8-state)              2.02        2.32      0.72  [  1.96,   2.69] ns/op
std::variant (8-state)                     6.89        7.44      1.20  [  6.83,   8.05] ns/op
[Boost].SML (8-state)                      7.40        7.78      0.64  [  7.45,   8.11] ns/op

[OK] Correctness validated for all 6 implementations

================================================================================
  Section 4: Self-Transition (No-Op) Performance
================================================================================

Contract: Self-transitions should early-exit without invoking hooks. Expected count is 1 (initial entry only).

[2026-02-15 20:00:08] CPU: 3022 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.44        0.46      0.13  [  0.40,   0.53] ns/op
fat_p Strict                               0.44        0.44      0.01  [  0.44,   0.45] ns/op
Manual enum-switch                         0.44        0.44      0.00  [  0.44,   0.44] ns/op
Manual fn-ptr table                        0.44        0.48      0.12  [  0.42,   0.54] ns/op
std::variant                               0.44        0.46      0.04  [  0.44,   0.48] ns/op
[Boost].SML                                0.44        0.47      0.10  [  0.41,   0.52] ns/op
TinyFSM                                    0.44        0.47      0.13  [  0.41,   0.54] ns/op
Boost.MSM                                  0.44        0.46      0.08  [  0.42,   0.50] ns/op

[OK] Correctness validated for all 8 implementations

================================================================================
  Section 5: State Query Performance
================================================================================

Contract: isInState<T>() / equivalent should be O(1). Query from initial state.

[2026-02-15 20:00:08] CPU: 2838 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.44        0.47      0.07  [  0.43,   0.50] ns/op
fat_p Strict                               0.44        0.44      0.00  [  0.44,   0.44] ns/op
Manual enum-switch                         0.44        0.44      0.01  [  0.44,   0.45] ns/op
Manual fn-ptr table                        0.44        0.48      0.14  [  0.41,   0.55] ns/op
std::variant                               0.44        0.46      0.06  [  0.43,   0.49] ns/op
[Boost].SML                                0.44        0.48      0.14  [  0.41,   0.55] ns/op
TinyFSM                                    0.44        0.46      0.08  [  0.42,   0.50] ns/op
Boost.MSM                                  0.44        0.48      0.14  [  0.41,   0.54] ns/op

[OK] All 8 implementations returned 5000000/5000000 true

================================================================================
  Summary
================================================================================

All benchmarks completed.

Key comparisons:
  - fat_p AnyToAny vs fat_p Strict: policy validation overhead
  - fat_p vs Manual: abstraction overhead
  - fat_p vs std::variant: type-safe alternative comparison
  - 4-state vs 8-state: O(1) scaling validation
  - Empty vs Counting hooks: hook invocation cost

Implementation characteristics:
  - fat_p AnyToAny: compile-time state set, O(1) fn-ptr dispatch, no validation
  - fat_p Strict: same + O(1) matrix lookup for transition validation
  - Manual enum-switch: runtime switch/case, inline-friendly
  - Manual fn-ptr table: constexpr table, indirect call
  - std::variant: type-safe union, visitor pattern dispatch

[INFO] CSV written to: benchmark_results\20260215_191525\benchmark_StateMachine.csv
[Sink: 2812042908]
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  Section 1: Core Transition Performance
================================================================================

Contract: transition() is O(1) for all implementations. All use 4 states with counting hooks.

[2026-02-16 03:37:52] CPU: 3244 MHz (~base: 3244)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             7.94        7.97      0.17  [  7.90,   8.04] ns/op
fat_p Strict                               7.92        7.95      0.14  [  7.90,   8.01] ns/op
Manual enum-switch                         2.37        2.37      0.07  [  2.34,   2.40] ns/op
Manual fn-ptr table                        3.12        3.16      0.09  [  3.12,   3.19] ns/op
std::variant                               6.42        6.43      0.09  [  6.39,   6.47] ns/op
[Boost].SML                                7.63        7.64      0.20  [  7.55,   7.73] ns/op
TinyFSM                                    8.30        8.30      0.04  [  8.29,   8.32] ns/op
Boost.MSM                                 11.76       11.79      0.16  [ 11.72,  11.86] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 2a: Hook Overhead (Empty Hooks)
================================================================================

Contract: Baseline with empty on_entry/on_exit hooks. Measures pure dispatch overhead.

[2026-02-16 03:37:52] CPU: 3240 MHz (~base: 3240)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (empty)                     8.29        8.26      0.19  [  8.18,   8.35] ns/op
fat_p Strict (empty)                       7.68        7.75      0.21  [  7.65,   7.84] ns/op
Manual enum-switch (empty)                 2.16        2.20      0.11  [  2.15,   2.24] ns/op
Manual fn-ptr table (empty)                3.20        3.20      0.07  [  3.17,   3.23] ns/op
std::variant (empty)                       6.19        6.20      0.12  [  6.15,   6.26] ns/op
[Boost].SML (empty)                        7.17        7.25      0.24  [  7.15,   7.36] ns/op
TinyFSM (empty)                            8.56        8.58      0.13  [  8.52,   8.63] ns/op
Boost.MSM (empty)                         12.91       12.88      0.21  [ 12.79,  12.98] ns/op
================================================================================
  Section 2b: Hook Overhead (Counting Hooks)
================================================================================

Contract: Hooks increment a counter. Measures dispatch + minimal work.

[2026-02-16 03:37:52] CPU: 3242 MHz (~base: 3242)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             8.41        8.42      0.32  [  8.28,   8.56] ns/op
fat_p Strict                               8.06        8.19      0.30  [  8.06,   8.32] ns/op
Manual enum-switch                         2.39        2.39      0.07  [  2.36,   2.42] ns/op
Manual fn-ptr table                        3.19        3.23      0.13  [  3.17,   3.28] ns/op
std::variant                               6.46        6.47      0.08  [  6.44,   6.51] ns/op
[Boost].SML                                7.68        7.69      0.16  [  7.63,   7.76] ns/op
TinyFSM                                    8.96        8.94      0.18  [  8.86,   9.02] ns/op
Boost.MSM                                 12.40       12.49      0.38  [ 12.32,  12.65] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 3a: State Scaling (4 States)
================================================================================

Contract: 4-state machines with counting hooks. Baseline for scaling comparison.

[2026-02-16 03:37:52] CPU: 3241 MHz (~base: 3241)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             8.09        8.26      0.65  [  7.97,   8.54] ns/op
fat_p Strict                               8.35        8.47      0.42  [  8.28,   8.65] ns/op
Manual enum-switch                         2.44        2.47      0.18  [  2.39,   2.55] ns/op
Manual fn-ptr table                        3.16        3.25      0.27  [  3.13,   3.37] ns/op
std::variant                               6.42        6.52      0.34  [  6.37,   6.67] ns/op
[Boost].SML                                7.70        7.79      0.57  [  7.54,   8.04] ns/op
TinyFSM                                    8.72        8.90      0.53  [  8.67,   9.14] ns/op
Boost.MSM                                 12.19       12.41      0.90  [ 12.01,  12.80] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 3b: State Scaling (8 States)
================================================================================

Contract: 8-state machines with counting hooks. O(1) claim: should match 4-state performance.

[2026-02-16 03:37:53] CPU: 3243 MHz (~base: 3243)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (8-state)                  17.09       16.51      1.04  [ 16.05,  16.96] ns/op
fat_p Strict (8-state)                    17.22       16.68      1.12  [ 16.20,  17.17] ns/op
Manual enum-switch (8-state)               2.04        1.94      0.25  [  1.83,   2.05] ns/op
Manual fn-ptr table (8-state)              3.21        2.93      0.43  [  2.74,   3.12] ns/op
std::variant (8-state)                    10.00        9.58      0.72  [  9.26,   9.89] ns/op
[Boost].SML (8-state)                     14.52       13.88      1.20  [ 13.35,  14.40] ns/op

[OK] Correctness validated for all 6 implementations
================================================================================
  Section 4: Self-Transition (No-Op) Performance
================================================================================

Contract: Self-transitions should early-exit without invoking hooks. Expected count is 1 (initial entry only).

[2026-02-16 03:37:53] CPU: 3237 MHz (~base: 3237)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.00        0.00      0.00  [  0.00,   0.00] ns/op
fat_p Strict                               0.00        0.00      0.00  [  0.00,   0.00] ns/op
Manual enum-switch                         0.00        0.00      0.00  [  0.00,   0.00] ns/op
Manual fn-ptr table                        0.00        0.00      0.00  [  0.00,   0.00] ns/op
std::variant                               0.00        0.00      0.00  [  0.00,   0.00] ns/op
[Boost].SML                                0.00        0.00      0.00  [  0.00,   0.00] ns/op
TinyFSM                                    0.00        0.00      0.00  [  0.00,   0.00] ns/op
Boost.MSM                                  0.00        0.00      0.00  [  0.00,   0.00] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 5: State Query Performance
================================================================================

Contract: isInState<T>() / equivalent should be O(1). Query from initial state.

[2026-02-16 03:37:53] CPU: 3242 MHz (~base: 3242)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.00        0.00      0.00  [  0.00,   0.00] ns/op
fat_p Strict                               0.00        0.00      0.00  [  0.00,   0.00] ns/op
Manual enum-switch                         0.00        0.00      0.00  [  0.00,   0.00] ns/op
Manual fn-ptr table                        0.00        0.00      0.00  [  0.00,   0.00] ns/op
std::variant                               0.00        0.00      0.00  [  0.00,   0.00] ns/op
[Boost].SML                                0.00        0.00      0.00  [  0.00,   0.00] ns/op
TinyFSM                                    0.00        0.00      0.00  [  0.00,   0.00] ns/op
Boost.MSM                                  0.00        0.00      0.00  [  0.00,   0.00] ns/op

[OK] All 8 implementations returned 100000/100000 true
================================================================================
  Summary
================================================================================

All benchmarks completed.

Key comparisons:
  - fat_p AnyToAny vs fat_p Strict: policy validation overhead
  - fat_p vs Manual: abstraction overhead
  - fat_p vs std::variant: type-safe alternative comparison
  - 4-state vs 8-state: O(1) scaling validation
  - Empty vs Counting hooks: hook invocation cost

Implementation characteristics:
  - fat_p AnyToAny: compile-time state set, O(1) fn-ptr dispatch, no validation
  - fat_p Strict: same + O(1) matrix lookup for transition validation
  - Manual enum-switch: runtime switch/case, inline-friendly
  - Manual fn-ptr table: constexpr table, indirect call
  - std::variant: type-safe union, visitor pattern dispatch

[INFO] CSV written to: results.csv
[Sink: 71857566]
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  Section 1: Core Transition Performance
================================================================================

Contract: transition() is O(1) for all implementations. All use 4 states with counting hooks.

[2026-02-16 04:11:20] CPU: 2871 MHz (~base: 2871)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                            13.05       13.09      0.20  [ 13.00,  13.18] ns/op
fat_p Strict                              13.68       13.69      0.09  [ 13.65,  13.73] ns/op
Manual enum-switch                         2.74        2.71      0.11  [  2.67,   2.76] ns/op
Manual fn-ptr table                        2.65        2.67      0.05  [  2.65,   2.70] ns/op
std::variant                               7.93        7.92      0.10  [  7.87,   7.96] ns/op
[Boost].SML                                7.84        7.86      0.10  [  7.82,   7.91] ns/op
TinyFSM                                   13.93       14.00      0.27  [ 13.89,  14.12] ns/op
Boost.MSM                                 15.78       15.91      0.48  [ 15.70,  16.12] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 2a: Hook Overhead (Empty Hooks)
================================================================================

Contract: Baseline with empty on_entry/on_exit hooks. Measures pure dispatch overhead.

[2026-02-16 04:11:20] CPU: 2870 MHz (~base: 2870)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (empty)                    11.31       11.37      0.39  [ 11.20,  11.53] ns/op
fat_p Strict (empty)                      11.40       11.42      0.20  [ 11.33,  11.50] ns/op
Manual enum-switch (empty)                 2.52        2.51      0.09  [  2.47,   2.55] ns/op
Manual fn-ptr table (empty)                2.51        2.50      0.06  [  2.47,   2.53] ns/op
std::variant (empty)                       7.57        7.63      0.16  [  7.57,   7.70] ns/op
[Boost].SML (empty)                        7.88        7.91      0.12  [  7.85,   7.96] ns/op
TinyFSM (empty)                           13.82       13.89      0.23  [ 13.79,  13.99] ns/op
Boost.MSM (empty)                         15.14       15.36      0.71  [ 15.05,  15.67] ns/op
================================================================================
  Section 2b: Hook Overhead (Counting Hooks)
================================================================================

Contract: Hooks increment a counter. Measures dispatch + minimal work.

[2026-02-16 04:11:20] CPU: 2870 MHz (~base: 2870)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                            13.04       13.13      0.24  [ 13.03,  13.24] ns/op
fat_p Strict                              13.71       13.72      0.12  [ 13.67,  13.78] ns/op
Manual enum-switch                         2.62        2.64      0.12  [  2.59,   2.69] ns/op
Manual fn-ptr table                        2.73        2.72      0.09  [  2.68,   2.76] ns/op
std::variant                               7.87        7.84      0.15  [  7.77,   7.90] ns/op
[Boost].SML                                7.94        7.93      0.10  [  7.88,   7.97] ns/op
TinyFSM                                   14.10       14.22      0.43  [ 14.04,  14.41] ns/op
Boost.MSM                                 15.25       15.32      0.35  [ 15.17,  15.48] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 3a: State Scaling (4 States)
================================================================================

Contract: 4-state machines with counting hooks. Baseline for scaling comparison.

[2026-02-16 04:11:20] CPU: 2878 MHz (~base: 2878)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                            12.94       12.93      0.08  [ 12.90,  12.97] ns/op
fat_p Strict                              13.34       13.38      0.12  [ 13.33,  13.43] ns/op
Manual enum-switch                         2.65        2.68      0.13  [  2.62,   2.74] ns/op
Manual fn-ptr table                        2.67        2.68      0.06  [  2.65,   2.70] ns/op
std::variant                               7.93        7.93      0.10  [  7.89,   7.98] ns/op
[Boost].SML                                7.87        7.88      0.08  [  7.84,   7.91] ns/op
TinyFSM                                   14.32       14.30      0.21  [ 14.20,  14.39] ns/op
Boost.MSM                                 14.89       14.94      0.26  [ 14.83,  15.06] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 3b: State Scaling (8 States)
================================================================================

Contract: 8-state machines with counting hooks. O(1) claim: should match 4-state performance.

[2026-02-16 04:11:20] CPU: 2596 MHz (~base: 2596)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (8-state)                  16.78       16.74      0.17  [ 16.66,  16.81] ns/op
fat_p Strict (8-state)                    14.29       14.28      0.17  [ 14.20,  14.35] ns/op
Manual enum-switch (8-state)               1.72        1.69      0.08  [  1.66,   1.72] ns/op
Manual fn-ptr table (8-state)              1.80        1.77      0.15  [  1.71,   1.84] ns/op
std::variant (8-state)                     8.91        8.91      0.07  [  8.88,   8.94] ns/op
[Boost].SML (8-state)                     11.96       11.98      0.11  [ 11.93,  12.03] ns/op

[OK] Correctness validated for all 6 implementations
================================================================================
  Section 4: Self-Transition (No-Op) Performance
================================================================================

Contract: Self-transitions should early-exit without invoking hooks. Expected count is 1 (initial entry only).

[2026-02-16 04:11:21] CPU: 2869 MHz (~base: 2869)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.00        0.00      0.00  [  0.00,   0.00] ns/op
fat_p Strict                               0.00        0.00      0.00  [  0.00,   0.00] ns/op
Manual enum-switch                         0.35        0.36      0.03  [  0.35,   0.37] ns/op
Manual fn-ptr table                        0.35        0.35      0.00  [  0.35,   0.35] ns/op
std::variant                               0.35        0.35      0.01  [  0.35,   0.35] ns/op
[Boost].SML                                0.00        0.00      0.00  [  0.00,   0.00] ns/op
TinyFSM                                    0.00        0.00      0.00  [  0.00,   0.00] ns/op
Boost.MSM                                  0.00        0.00      0.00  [  0.00,   0.00] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 5: State Query Performance
================================================================================

Contract: isInState<T>() / equivalent should be O(1). Query from initial state.

[2026-02-16 04:11:21] CPU: 2870 MHz (~base: 2870)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.00        0.00      0.00  [  0.00,   0.00] ns/op
fat_p Strict                               0.00        0.00      0.00  [  0.00,   0.00] ns/op
Manual enum-switch                         0.00        0.00      0.00  [  0.00,   0.00] ns/op
Manual fn-ptr table                        0.00        0.00      0.00  [  0.00,   0.00] ns/op
std::variant                               0.00        0.00      0.00  [  0.00,   0.00] ns/op
[Boost].SML                                0.00        0.00      0.00  [  0.00,   0.00] ns/op
TinyFSM                                    0.00        0.00      0.00  [  0.00,   0.00] ns/op
Boost.MSM                                  0.00        0.00      0.00  [  0.00,   0.00] ns/op

[OK] All 8 implementations returned 100000/100000 true
================================================================================
  Summary
================================================================================

All benchmarks completed.

Key comparisons:
  - fat_p AnyToAny vs fat_p Strict: policy validation overhead
  - fat_p vs Manual: abstraction overhead
  - fat_p vs std::variant: type-safe alternative comparison
  - 4-state vs 8-state: O(1) scaling validation
  - Empty vs Counting hooks: hook invocation cost

Implementation characteristics:
  - fat_p AnyToAny: compile-time state set, O(1) fn-ptr dispatch, no validation
  - fat_p Strict: same + O(1) matrix lookup for transition validation
  - Manual enum-switch: runtime switch/case, inline-friendly
  - Manual fn-ptr table: constexpr table, indirect call
  - std::variant: type-safe union, visitor pattern dispatch

[INFO] CSV written to: results.csv
[Sink: 71857566]
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  Section 1: Core Transition Performance
================================================================================

Contract: transition() is O(1) for all implementations. All use 4 states with counting hooks.

[2026-02-16 04:55:05] CPU: 2445 MHz (base: 2445)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             7.16        7.22      0.13  [  7.17,   7.28] ns/op
fat_p Strict                               7.53        7.59      0.14  [  7.53,   7.65] ns/op
Manual enum-switch                         2.47        2.54      0.15  [  2.48,   2.60] ns/op
Manual fn-ptr table                        3.25        3.31      0.12  [  3.26,   3.36] ns/op
std::variant                               6.86        6.92      0.16  [  6.85,   6.99] ns/op
[Boost].SML                                6.69        6.79      0.19  [  6.70,   6.87] ns/op
TinyFSM                                    9.33        9.35      0.18  [  9.28,   9.43] ns/op
Boost.MSM                                 16.67       16.64      0.14  [ 16.57,  16.70] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 2a: Hook Overhead (Empty Hooks)
================================================================================

Contract: Baseline with empty on_entry/on_exit hooks. Measures pure dispatch overhead.

[2026-02-16 04:55:05] CPU: 2445 MHz (base: 2445)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (empty)                     6.90        6.93      0.14  [  6.87,   6.99] ns/op
fat_p Strict (empty)                       7.33        7.42      0.13  [  7.36,   7.47] ns/op
Manual enum-switch (empty)                 2.31        2.34      0.08  [  2.30,   2.37] ns/op
Manual fn-ptr table (empty)                3.40        4.62      3.63  [  3.03,   6.22] ns/op
  [NOTE] High variance (stddev 3.63 > median 3.40)
std::variant (empty)                       6.68        6.75      0.12  [  6.69,   6.80] ns/op
[Boost].SML (empty)                        6.95        6.98      0.23  [  6.88,   7.08] ns/op
TinyFSM (empty)                            8.79        8.83      0.25  [  8.72,   8.94] ns/op
Boost.MSM (empty)                         17.56       17.77      0.77  [ 17.43,  18.11] ns/op
================================================================================
  Section 2b: Hook Overhead (Counting Hooks)
================================================================================

Contract: Hooks increment a counter. Measures dispatch + minimal work.

[2026-02-16 04:55:05] CPU: 2445 MHz (base: 2445)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             7.19        7.58      0.85  [  7.21,   7.96] ns/op
fat_p Strict                               7.59        7.93      0.80  [  7.58,   8.28] ns/op
Manual enum-switch                         2.47        2.56      0.22  [  2.46,   2.66] ns/op
Manual fn-ptr table                        7.11        6.91      0.57  [  6.66,   7.16] ns/op
std::variant                               6.87        7.19      0.71  [  6.88,   7.50] ns/op
[Boost].SML                                6.76        7.03      0.74  [  6.70,   7.36] ns/op
TinyFSM                                    9.41        9.91      1.11  [  9.43,  10.40] ns/op
Boost.MSM                                 17.29       18.74      3.04  [ 17.40,  20.07] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 3a: State Scaling (4 States)
================================================================================

Contract: 4-state machines with counting hooks. Baseline for scaling comparison.

[2026-02-16 04:55:05] CPU: 2445 MHz (base: 2445)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             7.18        7.27      0.25  [  7.16,   7.38] ns/op
fat_p Strict                               7.71        7.79      0.50  [  7.57,   8.01] ns/op
Manual enum-switch                         2.47        2.50      0.11  [  2.46,   2.55] ns/op
Manual fn-ptr table                        3.26        3.43      0.30  [  3.30,   3.56] ns/op
std::variant                               6.86        6.96      0.19  [  6.88,   7.05] ns/op
[Boost].SML                                6.68        6.76      0.31  [  6.63,   6.90] ns/op
TinyFSM                                    9.33        9.43      0.39  [  9.26,   9.61] ns/op
Boost.MSM                                 16.50       16.95      1.29  [ 16.38,  17.51] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 3b: State Scaling (8 States)
================================================================================

Contract: 8-state machines with counting hooks. O(1) claim: should match 4-state performance.

[2026-02-16 04:55:05] CPU: 2445 MHz (base: 2445)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (8-state)                   9.81       10.04      0.78  [  9.70,  10.38] ns/op
fat_p Strict (8-state)                     9.46        9.72      0.77  [  9.38,  10.06] ns/op
Manual enum-switch (8-state)               1.59        1.71      0.21  [  1.62,   1.81] ns/op
Manual fn-ptr table (8-state)              2.29        2.41      0.35  [  2.26,   2.56] ns/op
std::variant (8-state)                     8.67        8.96      0.76  [  8.63,   9.30] ns/op
[Boost].SML (8-state)                     10.18       10.76      1.18  [ 10.25,  11.28] ns/op

[OK] Correctness validated for all 6 implementations
================================================================================
  Section 4: Self-Transition (No-Op) Performance
================================================================================

Contract: Self-transitions should early-exit without invoking hooks. Expected count is 1 (initial entry only).

[2026-02-16 04:55:05] CPU: 2445 MHz (base: 2445)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.62        0.63      0.04  [  0.61,   0.65] ns/op
fat_p Strict                               0.62        0.63      0.03  [  0.61,   0.64] ns/op
Manual enum-switch                         0.62        0.65      0.11  [  0.60,   0.70] ns/op
Manual fn-ptr table                        0.62        0.64      0.05  [  0.61,   0.66] ns/op
std::variant                               0.62        0.62      0.01  [  0.62,   0.62] ns/op
[Boost].SML                                0.62        0.63      0.03  [  0.61,   0.64] ns/op
TinyFSM                                    0.62        0.62      0.03  [  0.61,   0.64] ns/op
Boost.MSM                                  0.62        0.62      0.03  [  0.61,   0.64] ns/op

[OK] Correctness validated for all 8 implementations
================================================================================
  Section 5: State Query Performance
================================================================================

Contract: isInState<T>() / equivalent should be O(1). Query from initial state.

[2026-02-16 04:55:05] CPU: 2445 MHz (base: 2445)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.97        0.95      0.25  [  0.84,   1.06] ns/op
fat_p Strict                               0.62        0.71      0.16  [  0.64,   0.77] ns/op
Manual enum-switch                         0.83        0.86      0.25  [  0.75,   0.97] ns/op
Manual fn-ptr table                        0.65        0.80      0.25  [  0.69,   0.91] ns/op
std::variant                               0.66        0.68      0.08  [  0.64,   0.71] ns/op
[Boost].SML                                0.67        0.67      0.05  [  0.65,   0.69] ns/op
TinyFSM                                    0.62        0.65      0.05  [  0.63,   0.67] ns/op
Boost.MSM                                  0.73        0.81      0.21  [  0.72,   0.90] ns/op

[OK] All 8 implementations returned 100000/100000 true
================================================================================
  Summary
================================================================================

All benchmarks completed.

Key comparisons:
  - fat_p AnyToAny vs fat_p Strict: policy validation overhead
  - fat_p vs Manual: abstraction overhead
  - fat_p vs std::variant: type-safe alternative comparison
  - 4-state vs 8-state: O(1) scaling validation
  - Empty vs Counting hooks: hook invocation cost

Implementation characteristics:
  - fat_p AnyToAny: compile-time state set, O(1) fn-ptr dispatch, no validation
  - fat_p Strict: same + O(1) matrix lookup for transition validation
  - Manual enum-switch: runtime switch/case, inline-friendly
  - Manual fn-ptr table: constexpr table, indirect call
  - std::variant: type-safe union, visitor pattern dispatch

[INFO] CSV written to: results.csv
[Sink: 71857566]
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
