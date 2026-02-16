---
doc_id: BR-CircularBuffer-001
doc_type: "Benchmark Results"
title: "CircularBuffer"
fatp_components: ["CircularBuffer"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - CircularBuffer

**Source:** `benchmark_CircularBuffer.cpp`
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
| Measured runs | 50 | 50 | 50 | 50 |
| CPU stabilization | Yes | No | No | No |

**Competitors detected:**

| Library | Local | GCC | Clang | MSVC CI |
|---------|-------|-----|-------|---------|
| fat_p::CircularBuffer | x | x | x | x |
| fat_p::LockFreeRingBuffer (sibling SPSC) | x | x | x | x |
| std::mutex + std::deque | x | x | x | x |
| boost::lockfree::spsc_queue | x | x | x | x |
| moodycamel::BlockingReaderWriterCircularBuffer | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=50 | seed=12345

```
[2026-02-15 19:18:56] INIT CPU: 2948 MHz (base: 3686)

Correctness verification:
  [PASS] CircularBuffer FIFO ordering
  [PASS] CircularBuffer capacity enforcement
  [PASS] CircularBuffer SPSC thread safety

[BenchmarkScope] High priority, CPU non-0 affinity

================================================================================
  Single-Threaded Throughput (push + pop cycle)
================================================================================

[2026-02-15 19:18:56] CPU: 2948 MHz (base: 3686)
Contract note: Uncontended SPSC overhead. Measures push+pop per op. Capacity: 4096.

Warmup (3 runs)...
Measured runs (15 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           0.95        0.97      0.02  [0.96, 0.97] ns/op
fat_p::LockFreeRingBuffer (SPSC)                1.17        1.17      0.01  [1.16, 1.17] ns/op
std::mutex + std::deque (baseline)             22.44       22.73      0.89  [22.28, 23.18] ns/op
boost::lockfree::spsc_queue                     1.25        1.25      0.02  [1.24, 1.26] ns/op
moodycamel::BlockingRWCircularBuffer           20.63       20.89      0.65  [20.56, 21.22] ns/op

[2026-02-15 19:18:56] END CPU: 2948 MHz (base: 3686)

================================================================================
  SPSC Throughput (dedicated producer/consumer threads)
================================================================================

[2026-02-15 19:18:56] CPU: 2948 MHz (base: 3686)
Contract note: True SPSC pattern. Producer and consumer on separate threads. Capacity: 4096.

Warmup (3 runs)...
Measured runs (15 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                          42.61       53.10     20.37  [42.80, 63.41] ns/op
fat_p::LockFreeRingBuffer (SPSC)                9.14       10.69      3.96  [8.69, 12.70] ns/op
std::mutex + std::deque (baseline)             88.66      109.43     36.06  [91.18, 127.68] ns/op
boost::lockfree::spsc_queue                    21.04       28.05     20.01  [17.93, 38.18] ns/op
moodycamel::BlockingRWCircularBuffer           66.72       90.13     63.02  [58.23, 122.02] ns/op

[2026-02-15 19:19:01] END CPU: 2617 MHz (base: 3686)

================================================================================
  Burst Pattern (fill then drain cycles)
================================================================================

[2026-02-15 19:19:01] CPU: 2617 MHz (base: 3686)
Contract note: Simulates batched workloads. Burst size: 1024, bursts: 1000.

Warmup (3 runs)...
Measured runs (15 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           0.86        0.90      0.10  [0.85, 0.95] ns/op
fat_p::LockFreeRingBuffer (SPSC)                0.93        0.91      0.03  [0.90, 0.93] ns/op
std::mutex + std::deque (baseline)              3.70        3.61      0.44  [3.39, 3.83] ns/op
boost::lockfree::spsc_queue                     0.89        0.91      0.08  [0.87, 0.95] ns/op
moodycamel::BlockingRWCircularBuffer           15.93       16.37      0.90  [15.92, 16.83] ns/op

[2026-02-15 19:19:02] END CPU: 2359 MHz (base: 3686)

================================================================================
  Capacity Sensitivity (SPSC throughput vs buffer size)
================================================================================

[2026-02-15 19:19:02] CPU: 2359 MHz (base: 3686)
Contract note: Fixed work per test. Smaller buffers may cause more contention.

       Capacity   Median ns/op     Throughput
--------------------------------------------------
             64           9.30       107.6 Mops/s
             1K           9.01       111.0 Mops/s
             4K           8.84       113.1 Mops/s
            64K           8.33       120.1 Mops/s

[2026-02-15 19:19:03] END CPU: 2948 MHz (base: 3686)

================================================================================
  Object Size Impact
================================================================================

CircularBuffer object sizes (includes inline metadata):

Type                                    sizeof (bytes)
------------------------------------------------------------
CircularBuffer<int64_t, 64>                         320
CircularBuffer<int64_t, 1024>                       320
CircularBuffer<int64_t, 4096>                       320
LockFreeRingBuffer<int64_t>                         320

Boost comparison:
boost::lockfree::spsc_queue<int64_t>                 88

Note: CircularBuffer uses cache-line aligned indices for false sharing prevention.
Actual buffer storage is heap-allocated via unique_ptr.

================================================================================
  Summary
================================================================================

CircularBuffer benchmark suite completed.
Hardware concurrency: 24 threads
[2026-02-15 19:19:03] FINAL CPU: 2948 MHz (base: 3686)
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=50 | seed=12345

```
[2026-02-16 03:37:47] INIT CPU: 3242 MHz (~base: 3242)

Correctness verification:
  [PASS] CircularBuffer FIFO ordering
  [PASS] CircularBuffer capacity enforcement
  [PASS] CircularBuffer SPSC thread safety
================================================================================
  Single-Threaded Throughput (push + pop cycle)
================================================================================

[2026-02-16 03:37:47] CPU: 3243 MHz (~base: 3243)
Contract note: Uncontended SPSC overhead. Measures push+pop per op. Capacity: 4096.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           2.06        2.13      0.16  [2.06, 2.20] ns/op
fat_p::LockFreeRingBuffer (SPSC)                2.44        2.94      1.84  [2.13, 3.75] ns/op
std::mutex + std::deque (baseline)             16.15       16.47      1.30  [15.90, 17.04] ns/op
boost::lockfree::spsc_queue                     2.16        2.20      0.09  [2.16, 2.24] ns/op
moodycamel::BlockingRWCircularBuffer           10.23       10.60      1.20  [10.07, 11.13] ns/op

[2026-02-16 03:37:47] END CPU: 3241 MHz (~base: 3241)

================================================================================
  SPSC Throughput (dedicated producer/consumer threads)
================================================================================

[2026-02-16 03:37:47] CPU: 3241 MHz (~base: 3241)
Contract note: True SPSC pattern. Producer and consumer on separate threads. Capacity: 4096.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                          65.53       64.31     21.88  [54.73, 73.90] ns/op
fat_p::LockFreeRingBuffer (SPSC)                8.88        9.02      0.51  [8.80, 9.25] ns/op
std::mutex + std::deque (baseline)            208.58      211.07     40.82  [193.19, 228.96] ns/op
boost::lockfree::spsc_queue                    26.24       25.09      4.34  [23.19, 26.99] ns/op
moodycamel::BlockingRWCircularBuffer          101.34      106.81     18.85  [98.55, 115.07] ns/op

[2026-02-16 03:37:48] END CPU: 2445 MHz (~base: 2445)

================================================================================
  Burst Pattern (fill then drain cycles)
================================================================================

[2026-02-16 03:37:48] CPU: 2445 MHz (~base: 2445)
Contract note: Simulates batched workloads. Burst size: 1024, bursts: 1000.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           2.12        2.13      0.04  [2.11, 2.15] ns/op
fat_p::LockFreeRingBuffer (SPSC)                2.33        2.35      0.08  [2.31, 2.38] ns/op
std::mutex + std::deque (baseline)              2.20        2.19      0.13  [2.13, 2.25] ns/op
boost::lockfree::spsc_queue                     2.04        2.05      0.03  [2.04, 2.07] ns/op
moodycamel::BlockingRWCircularBuffer           10.36       10.39      0.09  [10.35, 10.43] ns/op

[2026-02-16 03:37:48] END CPU: 3246 MHz (~base: 3246)

================================================================================
  Capacity Sensitivity (SPSC throughput vs buffer size)
================================================================================

[2026-02-16 03:37:48] CPU: 3240 MHz (~base: 3240)
Contract note: Fixed work per test. Smaller buffers may cause more contention.

       Capacity   Median ns/op     Throughput
--------------------------------------------------
             64          21.01        47.6 Mops/s
             1K           8.50       117.6 Mops/s
             4K           4.94       202.6 Mops/s
            64K           3.46       289.2 Mops/s

[2026-02-16 03:37:48] END CPU: 3243 MHz (~base: 3243)

================================================================================
  Object Size Impact
================================================================================

CircularBuffer object sizes (includes inline metadata):

Type                                    sizeof (bytes)
------------------------------------------------------------
CircularBuffer<int64_t, 64>                         320
CircularBuffer<int64_t, 1024>                       320
CircularBuffer<int64_t, 4096>                       320
LockFreeRingBuffer<int64_t>                         320

Boost comparison:
boost::lockfree::spsc_queue<int64_t>                 88

Note: CircularBuffer uses cache-line aligned indices for false sharing prevention.
Actual buffer storage is heap-allocated via unique_ptr.

================================================================================
  Summary
================================================================================

CircularBuffer benchmark suite completed.
Hardware concurrency: 4 threads
[2026-02-16 03:37:48] FINAL CPU: 3243 MHz (~base: 3243)
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=50 | seed=12345

```
[2026-02-16 04:11:20] INIT CPU: 3491 MHz (max: 2800) [TURBO]

Correctness verification:
  [PASS] CircularBuffer FIFO ordering
  [PASS] CircularBuffer capacity enforcement
  [PASS] CircularBuffer SPSC thread safety
================================================================================
  Single-Threaded Throughput (push + pop cycle)
================================================================================

[2026-02-16 04:11:20] CPU: 3492 MHz (max: 2800) [TURBO]
Contract note: Uncontended SPSC overhead. Measures push+pop per op. Capacity: 4096.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           2.36        2.36      0.01  [2.35, 2.36] ns/op
fat_p::LockFreeRingBuffer (SPSC)                2.55        2.55      0.04  [2.54, 2.57] ns/op
std::mutex + std::deque (baseline)             36.93       37.36      1.81  [36.56, 38.15] ns/op
boost::lockfree::spsc_queue                     2.47        2.47      0.01  [2.46, 2.47] ns/op
moodycamel::BlockingRWCircularBuffer           34.44       35.19      1.31  [34.62, 35.77] ns/op

[2026-02-16 04:11:20] END CPU: 3488 MHz (max: 2800) [TURBO]

================================================================================
  SPSC Throughput (dedicated producer/consumer threads)
================================================================================

[2026-02-16 04:11:20] CPU: 3488 MHz (max: 2800) [TURBO]
Contract note: True SPSC pattern. Producer and consumer on separate threads. Capacity: 4096.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                         186.96      185.68      6.28  [182.92, 188.43] ns/op
fat_p::LockFreeRingBuffer (SPSC)               42.14       42.46      2.64  [41.31, 43.62] ns/op
std::mutex + std::deque (baseline)            149.06      152.27     28.92  [139.59, 164.94] ns/op
boost::lockfree::spsc_queue                    39.02       38.94      2.72  [37.75, 40.13] ns/op
moodycamel::BlockingRWCircularBuffer          140.80      148.02     14.74  [141.56, 154.48] ns/op

[2026-02-16 04:11:22] END CPU: 3491 MHz (max: 2800) [TURBO]

================================================================================
  Burst Pattern (fill then drain cycles)
================================================================================

[2026-02-16 04:11:22] CPU: 3491 MHz (max: 2800) [TURBO]
Contract note: Simulates batched workloads. Burst size: 1024, bursts: 1000.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           4.02        4.00      0.05  [3.98, 4.03] ns/op
fat_p::LockFreeRingBuffer (SPSC)                4.08        4.10      0.08  [4.07, 4.14] ns/op
std::mutex + std::deque (baseline)              4.33        4.33      0.04  [4.31, 4.35] ns/op
boost::lockfree::spsc_queue                     4.48        4.48      0.02  [4.48, 4.49] ns/op
moodycamel::BlockingRWCircularBuffer           25.12       25.13      0.06  [25.10, 25.16] ns/op

[2026-02-16 04:11:23] END CPU: 3491 MHz (max: 2800) [TURBO]

================================================================================
  Capacity Sensitivity (SPSC throughput vs buffer size)
================================================================================

[2026-02-16 04:11:23] CPU: 3491 MHz (max: 2800) [TURBO]
Contract note: Fixed work per test. Smaller buffers may cause more contention.

       Capacity   Median ns/op     Throughput
--------------------------------------------------
             64          30.11        33.2 Mops/s
             1K          28.48        35.1 Mops/s
             4K          27.25        36.7 Mops/s
            64K          14.49        69.0 Mops/s

[2026-02-16 04:11:23] END CPU: 3495 MHz (max: 2800) [TURBO]

================================================================================
  Object Size Impact
================================================================================

CircularBuffer object sizes (includes inline metadata):

Type                                    sizeof (bytes)
------------------------------------------------------------
CircularBuffer<int64_t, 64>                         320
CircularBuffer<int64_t, 1024>                       320
CircularBuffer<int64_t, 4096>                       320
LockFreeRingBuffer<int64_t>                         320

Boost comparison:
boost::lockfree::spsc_queue<int64_t>                 88

Note: CircularBuffer uses cache-line aligned indices for false sharing prevention.
Actual buffer storage is heap-allocated via unique_ptr.

================================================================================
  Summary
================================================================================

CircularBuffer benchmark suite completed.
Hardware concurrency: 4 threads
[2026-02-16 04:11:23] FINAL CPU: 3495 MHz (max: 2800) [TURBO]
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=50 | seed=12345

```
[2026-02-16 04:54:19] INIT CPU: 2445 MHz (base: 2445)

Correctness verification:
  [PASS] CircularBuffer FIFO ordering
  [PASS] CircularBuffer capacity enforcement
  [PASS] CircularBuffer SPSC thread safety

[BenchmarkScope] High priority, CPU non-0 affinity

================================================================================
  Single-Threaded Throughput (push + pop cycle)
================================================================================

[2026-02-16 04:54:19] CPU: 2445 MHz (base: 2445)
Contract note: Uncontended SPSC overhead. Measures push+pop per op. Capacity: 4096.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           2.39        2.39      0.01  [2.38, 2.39] ns/op
fat_p::LockFreeRingBuffer (SPSC)                2.76        2.77      0.02  [2.76, 2.78] ns/op
std::mutex + std::deque (baseline)             28.10       28.55      1.23  [28.01, 29.09] ns/op
boost::lockfree::spsc_queue                     2.49        2.48      0.02  [2.48, 2.49] ns/op
moodycamel::BlockingRWCircularBuffer           10.80       11.07      0.90  [10.67, 11.47] ns/op

[2026-02-16 04:54:19] END CPU: 2445 MHz (base: 2445)

================================================================================
  SPSC Throughput (dedicated producer/consumer threads)
================================================================================

[2026-02-16 04:54:19] CPU: 2445 MHz (base: 2445)
Contract note: True SPSC pattern. Producer and consumer on separate threads. Capacity: 4096.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                          62.20       55.76     12.76  [50.17, 61.35] ns/op
fat_p::LockFreeRingBuffer (SPSC)               18.01       16.59      2.65  [15.43, 17.76] ns/op
std::mutex + std::deque (baseline)            111.36      110.76     14.91  [104.22, 117.29] ns/op
boost::lockfree::spsc_queue                    24.74       23.47      4.69  [21.41, 25.52] ns/op
moodycamel::BlockingRWCircularBuffer          119.80       97.28     37.07  [81.03, 113.53] ns/op

[2026-02-16 04:54:19] END CPU: 2445 MHz (base: 2445)

================================================================================
  Burst Pattern (fill then drain cycles)
================================================================================

[2026-02-16 04:54:19] CPU: 2445 MHz (base: 2445)
Contract note: Simulates batched workloads. Burst size: 1024, bursts: 1000.

Warmup (3 runs)...
Measured runs (20 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           2.16        2.23      0.19  [2.15, 2.31] ns/op
fat_p::LockFreeRingBuffer (SPSC)                2.66        2.69      0.08  [2.66, 2.73] ns/op
std::mutex + std::deque (baseline)              4.96        5.05      0.20  [4.96, 5.14] ns/op
boost::lockfree::spsc_queue                     2.36        2.40      0.13  [2.35, 2.46] ns/op
moodycamel::BlockingRWCircularBuffer           10.31       10.52      0.44  [10.33, 10.72] ns/op

[2026-02-16 04:54:20] END CPU: 2445 MHz (base: 2445)

================================================================================
  Capacity Sensitivity (SPSC throughput vs buffer size)
================================================================================

[2026-02-16 04:54:20] CPU: 2445 MHz (base: 2445)
Contract note: Fixed work per test. Smaller buffers may cause more contention.

       Capacity   Median ns/op     Throughput
--------------------------------------------------
             64          11.47        87.2 Mops/s
             1K          11.03        90.7 Mops/s
             4K           9.29       107.7 Mops/s
            64K           5.85       171.0 Mops/s

[2026-02-16 04:54:20] END CPU: 2445 MHz (base: 2445)

================================================================================
  Object Size Impact
================================================================================

CircularBuffer object sizes (includes inline metadata):

Type                                    sizeof (bytes)
------------------------------------------------------------
CircularBuffer<int64_t, 64>                         320
CircularBuffer<int64_t, 1024>                       320
CircularBuffer<int64_t, 4096>                       320
LockFreeRingBuffer<int64_t>                         320

Boost comparison:
boost::lockfree::spsc_queue<int64_t>                 88

Note: CircularBuffer uses cache-line aligned indices for false sharing prevention.
Actual buffer storage is heap-allocated via unique_ptr.

================================================================================
  Summary
================================================================================

CircularBuffer benchmark suite completed.
Hardware concurrency: 4 threads
[2026-02-16 04:54:20] FINAL CPU: 2445 MHz (base: 2445)
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
