---
doc_id: BR-WorkQueue-001
doc_type: "Benchmark Results"
title: "WorkQueue"
fatp_components: ["WorkQueue"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - WorkQueue

**Source:** `benchmark_WorkQueue.cpp`
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
| fat_p::WorkQueue | x | x | x | x |
| fat_p::LockFreeQueue (sibling) | x | x | x | x |
| std::mutex + std::queue | x | x | x | x |
| moodycamel::ConcurrentQueue | x | x | x | x |
| boost::lockfree::queue | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
[PASS] WorkQueue round-trip (100 elements)
  [PASS] WorkQueue capacity enforcement (accepted 32 / 32)
  [PASS] WorkQueue MPMC exactly-once (4P:4C, 4000 elements)
  [PASS] LockFreeQueue FIFO ordering

Expected Results:
  - fat_p::WorkQueue excels at: high-contention MPMC (>= 4 threads)
  - fat_p::LockFreeQueue: lower overhead at low contention (1-2 threads)
  - std::mutex + std::queue: correct but slow under contention
  - WorkQueue pays shard-routing overhead in single-threaded/SPSC

[BenchmarkScope] High priority, CPU non-0 affinity
  [NOTE] Single-thread target work clamped from 1000000 to 131072 to avoid capacity overflow.

================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 20:00:44] CPU: 3686 MHz (base: 3686)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op. WorkQueue pays shard-routing cost even single-threaded; this quantifies that overhead.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               9.07        9.14      0.54  [    8.87,     9.42]  ns/op
fat_p::LockFreeQueue                       8.24        8.52      0.60  [    8.22,     8.83]  ns/op
std::mutex + std::queue (baseline)        17.23       17.63      1.30  [   16.98,    18.29]  ns/op
moodycamel::ConcurrentQueue                5.40        5.55      0.41  [    5.34,     5.76]  ns/op
boost::lockfree::queue                    36.73       38.01      3.02  [   36.48,    39.54]  ns/op

[END 20:00:45] CPU: 3686 MHz (base: 3686)

================================================================================
  SPSC Throughput (1 producer, 1 consumer)
================================================================================

[START 20:00:45] CPU: 3686 MHz (base: 3686)
Contract: Dedicated producer and consumer threads. Start barrier ensures simultaneous launch. WorkQueue is not optimized for SPSC; this measures the baseline cost of shard routing when contention is absent.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               4.16        4.19      0.39  [    3.99,     4.38]  ns/op
fat_p::LockFreeQueue                       5.15        5.50      1.82  [    4.58,     6.42]  ns/op
std::mutex + std::queue (baseline)        19.80       19.48      3.12  [   17.90,    21.06]  ns/op
moodycamel::ConcurrentQueue               16.25       17.59      4.56  [   15.28,    19.90]  ns/op
boost::lockfree::queue                    97.16      100.31     43.83  [   78.13,   122.50]  ns/op

[END 20:00:50] CPU: 3686 MHz (base: 3686)

================================================================================
  MPMC Symmetric Scaling (N producers, N consumers)
================================================================================

[START 20:00:51] CPU: 3686 MHz (base: 3686)
Contract: Equal producer and consumer threads. Start barrier ensures simultaneous launch. Tests lock-free scaling under increasing contention. WorkQueue should dominate at >= 4 threads where CAS contention matters.

Thread counts: 1 2 4 8 12 16 

Library                                      1T          2T          4T          8T         12T         16T
-----------------------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               16.1        13.3        27.8        26.1        24.9        29.4 ns/op
fat_p::LockFreeQueue                        7.1        24.9        73.7        83.8        87.9        87.9 ns/op
std::mutex + std::queue (baseline)         19.2        19.9        50.0       181.1       250.4       222.0 ns/op
moodycamel::ConcurrentQueue                14.6        20.2        32.9        31.3        27.3        25.8 ns/op
boost::lockfree::queue                    111.9       120.0       274.1       241.9       263.8       307.3 ns/op

[END 20:02:27] CPU: 3686 MHz (base: 3686)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 20:02:27] CPU: 3686 MHz (base: 3686)
Contract: Non-symmetric producer/consumer ratios. Tests WorkQueue under real-world dispatch patterns: many-producer/single-consumer (task submission), single-producer/many-consumer (fan-out), and unbalanced ratios.

Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               23.3      25.2      27.4      23.1      25.4      25.8 ns/op
fat_p::LockFreeQueue                       53.2      42.1      78.8      50.7      58.8      83.3 ns/op
std::mutex + std::queue (baseline)         40.0      20.7     184.0      27.1      63.2     145.3 ns/op
moodycamel::ConcurrentQueue                17.8      13.6      64.2      45.3      13.6      36.1 ns/op
boost::lockfree::queue                    194.0     135.2     225.7     201.0     233.6     234.6 ns/op

[END 20:03:59] CPU: 3686 MHz (base: 3686)

================================================================================
  Burst/Drain (single-threaded fill then drain)
================================================================================

[START 20:04:00] CPU: 3686 MHz (base: 3686)
Contract: Fill the queue to a target burst size, then drain completely. Single-threaded, no contention. Measures throughput under bursty access where the queue transitions between near-full and empty states. Allocation is excluded (reserve performed).

Library                                  1024 ops      8192 ops     65536 ops
-----------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                  8.5           8.9           9.0 ns/op
fat_p::LockFreeQueue                          8.1           8.5           8.2 ns/op
std::mutex + std::queue (baseline)           12.7          13.3          13.3 ns/op

[END 20:04:00] CPU: 3686 MHz (base: 3686)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 24 threads
[FINAL 20:04:00] CPU: 3686 MHz (base: 3686)
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
[PASS] WorkQueue round-trip (100 elements)
  [PASS] WorkQueue capacity enforcement (accepted 32 / 32)
  [PASS] WorkQueue MPMC exactly-once (4P:4C, 4000 elements)
  [PASS] LockFreeQueue FIFO ordering

Expected Results:
  - fat_p::WorkQueue excels at: high-contention MPMC (>= 4 threads)
  - fat_p::LockFreeQueue: lower overhead at low contention (1-2 threads)
  - std::mutex + std::queue: correct but slow under contention
  - WorkQueue pays shard-routing overhead in single-threaded/SPSC
================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 03:37:49] CPU: (frequency unavailable)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op. WorkQueue pays shard-routing cost even single-threaded; this quantifies that overhead.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               4.46        4.51      0.16  [    4.44,     4.59]  ns/op
fat_p::LockFreeQueue                       4.46        4.49      0.11  [    4.44,     4.54]  ns/op
std::mutex + std::queue (baseline)         7.68        9.87      3.28  [    8.43,    11.30]  ns/op
moodycamel::ConcurrentQueue                6.41        6.65      0.68  [    6.35,     6.95]  ns/op
boost::lockfree::queue                     8.49        8.77      0.71  [    8.45,     9.08]  ns/op

[END 03:37:49] CPU: (frequency unavailable)

================================================================================
  SPSC Throughput (1 producer, 1 consumer)
================================================================================

[START 03:37:49] CPU: (frequency unavailable)
Contract: Dedicated producer and consumer threads. Start barrier ensures simultaneous launch. WorkQueue is not optimized for SPSC; this measures the baseline cost of shard routing when contention is absent.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               2.66        2.77      0.18  [    2.69,     2.85]  ns/op
fat_p::LockFreeQueue                       2.78        2.79      0.06  [    2.76,     2.81]  ns/op
std::mutex + std::queue (baseline)        28.53       32.95     12.87  [   27.31,    38.59]  ns/op
moodycamel::ConcurrentQueue               15.86       16.11      1.18  [   15.59,    16.63]  ns/op
boost::lockfree::queue                    44.17       44.23      0.21  [   44.14,    44.32]  ns/op

[END 03:37:50] CPU: (frequency unavailable)

================================================================================
  MPMC Symmetric Scaling (N producers, N consumers)
================================================================================

[START 03:37:50] CPU: (frequency unavailable)
Contract: Equal producer and consumer threads. Start barrier ensures simultaneous launch. Tests lock-free scaling under increasing contention. WorkQueue should dominate at >= 4 threads where CAS contention matters.

Thread counts: 1 2 4 

Library                                      1T          2T          4T
-----------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                7.3        38.6        22.5 ns/op
fat_p::LockFreeQueue                        6.8        42.7        45.8 ns/op
std::mutex + std::queue (baseline)         30.3        52.7        59.1 ns/op
moodycamel::ConcurrentQueue                20.8        20.2        16.5 ns/op
boost::lockfree::queue                     47.8       132.4       122.1 ns/op

[END 03:37:52] CPU: (frequency unavailable)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 03:37:52] CPU: (frequency unavailable)
Contract: Non-symmetric producer/consumer ratios. Tests WorkQueue under real-world dispatch patterns: many-producer/single-consumer (task submission), single-producer/many-consumer (fan-out), and unbalanced ratios.
Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                6.1       5.5      17.3      18.2      10.6      22.6 ns/op
fat_p::LockFreeQueue                       35.8      35.5      38.6      37.9      37.6      45.8 ns/op
std::mutex + std::queue (baseline)         35.4      33.2     109.5      67.7      50.6      97.5 ns/op
moodycamel::ConcurrentQueue                 8.3       7.4      52.2      45.3      16.7      27.2 ns/op
boost::lockfree::queue                     93.6      90.1     102.3      79.1      97.6     160.2 ns/op

[END 03:38:01] CPU: (frequency unavailable)

================================================================================
  Burst/Drain (single-threaded fill then drain)
================================================================================

[START 03:38:01] CPU: (frequency unavailable)
Contract: Fill the queue to a target burst size, then drain completely. Single-threaded, no contention. Measures throughput under bursty access where the queue transitions between near-full and empty states. Allocation is excluded (reserve performed).

Library                                  1024 ops      8192 ops     65536 ops
-----------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                  4.4           4.3           4.3 ns/op
fat_p::LockFreeQueue                          4.3           4.3           4.4 ns/op
std::mutex + std::queue (baseline)            7.1           7.2           7.2 ns/op

[END 03:38:01] CPU: (frequency unavailable)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
[FINAL 03:38:01] CPU: (frequency unavailable)
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
[PASS] WorkQueue round-trip (100 elements)
  [PASS] WorkQueue capacity enforcement (accepted 32 / 32)
  [PASS] WorkQueue MPMC exactly-once (4P:4C, 4000 elements)
  [PASS] LockFreeQueue FIFO ordering

Expected Results:
  - fat_p::WorkQueue excels at: high-contention MPMC (>= 4 threads)
  - fat_p::LockFreeQueue: lower overhead at low contention (1-2 threads)
  - std::mutex + std::queue: correct but slow under contention
  - WorkQueue pays shard-routing overhead in single-threaded/SPSC
================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 04:11:20] CPU: (frequency unavailable)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op. WorkQueue pays shard-routing cost even single-threaded; this quantifies that overhead.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               4.74        4.77      0.08  [    4.73,     4.80]  ns/op
fat_p::LockFreeQueue                       4.49        4.50      0.04  [    4.48,     4.52]  ns/op
std::mutex + std::queue (baseline)         7.70        9.70      3.30  [    8.26,    11.15]  ns/op
moodycamel::ConcurrentQueue                6.93        7.21      0.59  [    6.96,     7.47]  ns/op
boost::lockfree::queue                     8.63        8.73      0.31  [    8.59,     8.86]  ns/op

[END 04:11:21] CPU: (frequency unavailable)

================================================================================
  SPSC Throughput (1 producer, 1 consumer)
================================================================================

[START 04:11:21] CPU: (frequency unavailable)
Contract: Dedicated producer and consumer threads. Start barrier ensures simultaneous launch. WorkQueue is not optimized for SPSC; this measures the baseline cost of shard routing when contention is absent.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               2.93        2.96      0.09  [    2.93,     3.00]  ns/op
fat_p::LockFreeQueue                       3.01        3.13      0.42  [    2.94,     3.31]  ns/op
std::mutex + std::queue (baseline)        26.11       27.63      7.28  [   24.44,    30.82]  ns/op
moodycamel::ConcurrentQueue               17.84       17.51      0.81  [   17.16,    17.86]  ns/op
boost::lockfree::queue                    45.23       45.19      0.28  [   45.07,    45.31]  ns/op

[END 04:11:22] CPU: (frequency unavailable)

================================================================================
  MPMC Symmetric Scaling (N producers, N consumers)
================================================================================

[START 04:11:22] CPU: (frequency unavailable)
Contract: Equal producer and consumer threads. Start barrier ensures simultaneous launch. Tests lock-free scaling under increasing contention. WorkQueue should dominate at >= 4 threads where CAS contention matters.

Thread counts: 1 2 4 

Library                                      1T          2T          4T
-----------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                7.9        17.0        20.4 ns/op
fat_p::LockFreeQueue                        8.1        41.7        50.6 ns/op
std::mutex + std::queue (baseline)         31.8        55.0        58.2 ns/op
moodycamel::ConcurrentQueue                19.5        20.4        16.3 ns/op
boost::lockfree::queue                     50.5       127.9       118.3 ns/op

[END 04:11:23] CPU: (frequency unavailable)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 04:11:24] CPU: (frequency unavailable)
Contract: Non-symmetric producer/consumer ratios. Tests WorkQueue under real-world dispatch patterns: many-producer/single-consumer (task submission), single-producer/many-consumer (fan-out), and unbalanced ratios.
Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                6.6       5.7      18.2      18.2      10.8      18.8 ns/op
fat_p::LockFreeQueue                       33.6      33.6      40.5      39.5      34.4      48.4 ns/op
std::mutex + std::queue (baseline)         35.6      34.8     104.5      70.4      50.8      94.8 ns/op
moodycamel::ConcurrentQueue                 9.2       8.3      52.5      46.5      16.7      35.8 ns/op
boost::lockfree::queue                     91.4      91.6     120.5      81.3      99.9     147.7 ns/op

[END 04:11:33] CPU: (frequency unavailable)

================================================================================
  Burst/Drain (single-threaded fill then drain)
================================================================================

[START 04:11:33] CPU: (frequency unavailable)
Contract: Fill the queue to a target burst size, then drain completely. Single-threaded, no contention. Measures throughput under bursty access where the queue transitions between near-full and empty states. Allocation is excluded (reserve performed).

Library                                  1024 ops      8192 ops     65536 ops
-----------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                  4.7           4.6           4.7 ns/op
fat_p::LockFreeQueue                          4.4           4.3           4.4 ns/op
std::mutex + std::queue (baseline)            7.4           7.5           7.5 ns/op

[END 04:11:33] CPU: (frequency unavailable)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
[FINAL 04:11:33] CPU: (frequency unavailable)
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
[PASS] WorkQueue round-trip (100 elements)
  [PASS] WorkQueue capacity enforcement (accepted 32 / 32)
  [PASS] WorkQueue MPMC exactly-once (4P:4C, 4000 elements)
  [PASS] LockFreeQueue FIFO ordering

Expected Results:
  - fat_p::WorkQueue excels at: high-contention MPMC (>= 4 threads)
  - fat_p::LockFreeQueue: lower overhead at low contention (1-2 threads)
  - std::mutex + std::queue: correct but slow under contention
  - WorkQueue pays shard-routing overhead in single-threaded/SPSC

[BenchmarkScope] High priority, CPU non-0 affinity

================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 04:54:51] CPU: 2445 MHz (base: 2445)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op. WorkQueue pays shard-routing cost even single-threaded; this quantifies that overhead.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               4.88        4.87      0.08  [    4.83,     4.91]  ns/op
fat_p::LockFreeQueue                       4.42        4.41      0.07  [    4.38,     4.44]  ns/op
std::mutex + std::queue (baseline)        19.84       19.94      0.40  [   19.77,    20.12]  ns/op
moodycamel::ConcurrentQueue                7.20        7.36      0.39  [    7.20,     7.53]  ns/op
boost::lockfree::queue                    34.02       35.32      2.51  [   34.22,    36.42]  ns/op

[END 04:54:52] CPU: 2445 MHz (base: 2445)

================================================================================
  SPSC Throughput (1 producer, 1 consumer)
================================================================================

[START 04:54:52] CPU: 2445 MHz (base: 2445)
Contract: Dedicated producer and consumer threads. Start barrier ensures simultaneous launch. WorkQueue is not optimized for SPSC; this measures the baseline cost of shard routing when contention is absent.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               4.47        4.81      0.95  [    4.39,     5.22]  ns/op
fat_p::LockFreeQueue                       3.56        3.70      0.41  [    3.52,     3.88]  ns/op
std::mutex + std::queue (baseline)        22.07       22.38      1.41  [   21.76,    23.00]  ns/op
moodycamel::ConcurrentQueue               15.90       15.92      2.13  [   14.99,    16.86]  ns/op
boost::lockfree::queue                    81.47       76.74     10.83  [   71.99,    81.49]  ns/op

[END 04:54:53] CPU: 2445 MHz (base: 2445)

================================================================================
  MPMC Symmetric Scaling (N producers, N consumers)
================================================================================

[START 04:54:53] CPU: 2445 MHz (base: 2445)
Contract: Equal producer and consumer threads. Start barrier ensures simultaneous launch. Tests lock-free scaling under increasing contention. WorkQueue should dominate at >= 4 threads where CAS contention matters.

Thread counts: 1 2 4 

Library                                      1T          2T          4T
-----------------------------------------------------------------------
fat_p::WorkQueue (16 shards)               17.9        21.6        21.1 ns/op
fat_p::LockFreeQueue                       17.1        21.8        32.1 ns/op
std::mutex + std::queue (baseline)         46.0        43.5        43.3 ns/op
moodycamel::ConcurrentQueue                19.3        24.3        24.9 ns/op
boost::lockfree::queue                     91.8        98.2        99.9 ns/op

[END 04:54:55] CPU: 2445 MHz (base: 2445)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 04:54:56] CPU: 2445 MHz (base: 2445)
Contract: Non-symmetric producer/consumer ratios. Tests WorkQueue under real-world dispatch patterns: many-producer/single-consumer (task submission), single-producer/many-consumer (fan-out), and unbalanced ratios.
Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                7.9       6.4      20.4      17.5      16.9      17.2 ns/op
fat_p::LockFreeQueue                       33.7      35.3      38.3      34.5      42.3      27.5 ns/op
std::mutex + std::queue (baseline)        198.5      31.8     198.7      35.1     198.1     197.2 ns/op
moodycamel::ConcurrentQueue               134.9       9.3      51.6      47.9      23.7     186.3 ns/op
boost::lockfree::queue                    261.5      82.7     164.9      74.3     285.2      98.3 ns/op

[END 04:55:23] CPU: 2445 MHz (base: 2445)

================================================================================
  Burst/Drain (single-threaded fill then drain)
================================================================================

[START 04:55:23] CPU: 2445 MHz (base: 2445)
Contract: Fill the queue to a target burst size, then drain completely. Single-threaded, no contention. Measures throughput under bursty access where the queue transitions between near-full and empty states. Allocation is excluded (reserve performed).

Library                                  1024 ops      8192 ops     65536 ops
-----------------------------------------------------------------------------
fat_p::WorkQueue (16 shards)                  5.2           4.8           4.9 ns/op
fat_p::LockFreeQueue                          4.7           4.3           4.5 ns/op
std::mutex + std::queue (baseline)           17.5          17.3          17.6 ns/op

[END 04:55:23] CPU: 2445 MHz (base: 2445)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
[FINAL 04:55:23] CPU: 2445 MHz (base: 2445)
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
