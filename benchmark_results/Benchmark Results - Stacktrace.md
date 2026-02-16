---
doc_id: BR-Stacktrace-001
doc_type: "Benchmark Results"
title: "Stacktrace"
fatp_components: ["Stacktrace"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - Stacktrace

**Source:** `benchmark_Stacktrace.cpp`
**Date:** February 2026
**Methodology:** Round-robin, randomized order, CPU-stabilized (local) / unstabilized (CI), median-primary

---

## Test Environments

| Property | Local (MSVC) | GCC CI | Clang CI | MSVC CI |
|----------|-------------|--------|----------|---------|
| OS | Windows 11 Pro | Ubuntu (Azure) | Ubuntu (Azure) | Windows (Azure) |
| Compiler | Windows-x64 MSVC-1950 | Linux-x64 GCC-14.2 | Linux-x64 Clang-17.0 | — |
| CPU | Intel Core Ultra 9 285K | Azure (shared) | Azure (shared) | Azure (shared) |
| RAM | 64 GB DDR5 | Shared tenancy | Shared tenancy | Shared tenancy |
| Measured runs | 15 | 20 | 20 | — |
| CPU stabilization | Yes | No | No | No |

**Competitors detected:**

| Library | Local | GCC | Clang | MSVC CI |
|---------|-------|-----|-------|---------|
| fat_p::Stacktrace | x | x | x | — |
| Native backend (Windows DbgHelp) | x | — | — | — |
| Native backend (execinfo) | — | x | x | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
======================================================================
  captureRaw() - Address Collection Only
======================================================================

Contract: Measures address capture without symbol resolution

captureRaw()                          0.35 us/op  (mean: 0.35, stddev: 0.00)

======================================================================
  current() - Full Capture with Symbols
======================================================================

Contract: Measures capture + symbol resolution

current()                             4.35 us/op  (mean: 4.45, stddev: 0.22)

======================================================================
  Formatting - toString() and toJson()
======================================================================

Contract: Measures output formatting from symbolized trace

toString()                            2.96 us/op  (mean: 2.97, stddev: 0.05)
toJson()                              4.99 us/op  (mean: 5.02, stddev: 0.06)

======================================================================
  Depth Scaling - captureRaw() at Various Stack Depths
======================================================================

Contract: Measures capture cost vs stack depth

Depth                               Median  us/op
--------------------------------------------------
depth=5                               0.70  us/op
depth=10                              0.98  us/op
depth=20                              1.55  us/op
depth=50                              2.03  us/op

======================================================================
  hash() - For Deduplication
======================================================================

Contract: Measures hash computation for container usage

hash()                                7.87 ns/op  (mean: 7.93, stddev: 0.18)

======================================================================
  Summary
======================================================================

All benchmarks completed.
Backend used: Windows DbgHelp
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
======================================================================
  captureRaw() - Address Collection Only
======================================================================

Contract: Measures address capture without symbol resolution

captureRaw()                          0.89 us/op  (mean: 0.89, stddev: 0.02)

======================================================================
  current() - Full Capture with Symbols
======================================================================

Contract: Measures capture + symbol resolution

current()                             1.07 us/op  (mean: 1.08, stddev: 0.02)

======================================================================
  Formatting - toString() and toJson()
======================================================================

Contract: Measures output formatting from symbolized trace

toString()                            1.08 us/op  (mean: 1.08, stddev: 0.01)
toJson()                              1.84 us/op  (mean: 1.84, stddev: 0.01)

======================================================================
  Depth Scaling - captureRaw() at Various Stack Depths
======================================================================

Contract: Measures capture cost vs stack depth

Depth                               Median  us/op
--------------------------------------------------
depth=5                               2.96  us/op
depth=10                              4.59  us/op
depth=20                              7.81  us/op
depth=50                             10.99  us/op

======================================================================
  hash() - For Deduplication
======================================================================

Contract: Measures hash computation for container usage

hash()                                0.31 ns/op  (mean: 0.31, stddev: 0.00)

======================================================================
  Summary
======================================================================

All benchmarks completed.
Backend used: execinfo
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
======================================================================
  captureRaw() - Address Collection Only
======================================================================

CPU: 3418 MHz
Contract: Measures address capture without symbol resolution

captureRaw()                          1.00 us/op  (mean: 1.00, stddev: 0.01)

======================================================================
  current() - Full Capture with Symbols
======================================================================

CPU: 3553 MHz
Contract: Measures capture + symbol resolution

current()                             0.98 us/op  (mean: 0.99, stddev: 0.02)

======================================================================
  Formatting - toString() and toJson()
======================================================================

CPU: 2800 MHz
Contract: Measures output formatting from symbolized trace

toString()                            0.44 us/op  (mean: 0.44, stddev: 0.00)
toJson()                              0.61 us/op  (mean: 0.61, stddev: 0.00)

======================================================================
  Depth Scaling - captureRaw() at Various Stack Depths
======================================================================

CPU: 2800 MHz
Contract: Measures capture cost vs stack depth

Depth                               Median  us/op
--------------------------------------------------
depth=5                               1.18  us/op
depth=10                              1.19  us/op
depth=20                              1.20  us/op
depth=50                              1.23  us/op

======================================================================
  hash() - For Deduplication
======================================================================

CPU: 3388 MHz
Contract: Measures hash computation for container usage

hash()                                0.15 ns/op  (mean: 0.15, stddev: 0.00)

======================================================================
  Summary
======================================================================

All benchmarks completed.
Backend used: execinfo
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- MSVC CI results not available for this component.
