---
doc_id: BR-LockFreeQueue-001
doc_type: "Benchmark Results"
title: "LockFreeQueue"
fatp_components: ["LockFreeQueue"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - LockFreeQueue

**Source:** `benchmark_LockFreeQueue.cpp`
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
| Measured runs | 15 | 15 | 15 | 15 |
| CPU stabilization | Yes | No | No | No |

**Competitors detected:**

| Library | Local | GCC | Clang | MSVC CI |
|---------|-------|-----|-------|---------|
| fat_p::LockFreeQueue | x | x | x | x |
| fat_p::LockFreeRingBuffer | x | x | x | x |
| std::mutex + std::queue | x | x | x | x |
| moodycamel::ConcurrentQueue | x | x | x | x |
| boost::lockfree::queue | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
[INIT 19:36:07] CPU: 3686 MHz (base: 3686)

Correctness verification:
  [PASS] LockFreeQueue FIFO ordering
  [PASS] LockFreeRingBuffer (SPSC) FIFO ordering
  [PASS] LockFreeRingBufferMPMC FIFO ordering

[BenchmarkScope] High priority, CPU non-0 affinity
  [NOTE] Single-thread target work clamped from 1000000 to 131072 to avoid capacity overflow.

================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 19:36:07] CPU: 3686 MHz (base: 3686)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       8.11        8.36      0.50  [    8.11,     8.62]  ns/op
fat_p::WorkQueue (sharded)                 8.89        9.05      0.59  [    8.76,     9.35]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           0.80        0.79      0.13  [    0.73,     0.86]  ns/op
fat_p::LockFreeRingBufferMPMC              9.16        9.38      0.74  [    9.00,     9.75]  ns/op
std::mutex + std::queue (baseline)        15.75       15.91      0.48  [   15.66,    16.15]  ns/op
moodycamel::ConcurrentQueue                8.08        8.21      0.47  [    7.97,     8.44]  ns/op
boost::lockfree::queue                    36.19       38.25      4.33  [   36.06,    40.44]  ns/op

[END 19:36:08] CPU: 3686 MHz (base: 3686)

================================================================================
  SPSC Throughput (1 producer, 1 consumer threads)
================================================================================

[START 19:36:08] CPU: 3686 MHz (base: 3686)
Contract: Dedicated producer and consumer threads. Native SPSC use case.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       8.91       11.27      6.90  [    7.78,    14.77]  ns/op
fat_p::LockFreeRingBuffer (SPSC)          30.48       30.69     10.74  [   25.25,    36.12]  ns/op
std::mutex + std::queue (baseline)        21.46       22.01      2.34  [   20.83,    23.20]  ns/op
moodycamel::ConcurrentQueue               18.37       20.35      3.74  [   18.46,    22.24]  ns/op
boost::lockfree::queue                   152.54      149.48     13.65  [  142.57,   156.39]  ns/op

[END 19:36:16] CPU: 3686 MHz (base: 3686)

================================================================================
  MPMC Scaling (N producers, N consumers)
================================================================================

[START 19:36:16] CPU: 3686 MHz (base: 3686)
Contract: Equal producer and consumer threads. Tests lock-free scaling.

Thread counts: 1 2 4 8 12 16 

Library                                      1T          2T          4T          8T         12T         16T
-----------------------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                        6.8        21.5        68.5        83.5        86.2        93.8 ns/op
fat_p::WorkQueue (sharded)                  9.8        20.5        29.8        26.5        37.5        30.1 ns/op
fat_p::WorkQueue (round-robin)              9.5        19.5        46.3        83.0        69.1        78.2 ns/op
fat_p::WorkQueue (stride-3)                10.8        19.6        48.7        78.4        73.8        73.6 ns/op
fat_p::LockFreeRingBufferMPMC               5.7        25.8        64.0        86.2        89.7        92.9 ns/op
std::mutex + std::queue (baseline)         24.0        20.0        52.2       195.4       312.7       247.9 ns/op
moodycamel::ConcurrentQueue                20.2        31.7        49.1        45.0        43.3        40.2 ns/op
boost::lockfree::queue                     58.3        57.2       203.5       245.5       284.1       310.0 ns/op

[END 19:38:34] CPU: 3686 MHz (base: 3686)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 19:38:34] CPU: 3686 MHz (base: 3686)
Contract: Tests non-symmetric producer/consumer ratios.

Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       43.4      29.9      71.2      46.2      52.1      73.7 ns/op
fat_p::WorkQueue (sharded)                 18.3      21.9      23.8      24.8      25.9      24.1 ns/op
fat_p::LockFreeRingBufferMPMC              44.1      30.9      72.1      47.3      50.7      73.5 ns/op
std::mutex + std::queue (baseline)         38.6      21.6     183.6      31.4      48.7     133.5 ns/op
moodycamel::ConcurrentQueue                13.1      12.7      55.5      48.9      40.3      39.6 ns/op
boost::lockfree::queue                    129.9      96.5     233.0     179.9     233.0     265.1 ns/op

[END 19:40:10] CPU: 3686 MHz (base: 3686)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 24 threads
[FINAL 19:40:10] CPU: 3686 MHz (base: 3686)
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=15 | seed=12345

```
[INIT 03:37:48] CPU: (frequency unavailable)

Correctness verification:
  [PASS] LockFreeQueue FIFO ordering
  [PASS] LockFreeRingBuffer (SPSC) FIFO ordering
  [PASS] LockFreeRingBufferMPMC FIFO ordering
================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 03:37:48] CPU: (frequency unavailable)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       4.48        4.55      0.26  [    4.44,     4.67]  ns/op
fat_p::WorkQueue (sharded)                 4.43        4.42      0.05  [    4.40,     4.44]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           1.36        1.37      0.03  [    1.35,     1.38]  ns/op
fat_p::LockFreeRingBufferMPMC              7.42        7.43      0.04  [    7.41,     7.44]  ns/op
std::mutex + std::queue (baseline)         7.06        8.33      3.15  [    6.95,     9.71]  ns/op
moodycamel::ConcurrentQueue               10.41        9.92      0.78  [    9.58,    10.26]  ns/op
boost::lockfree::queue                     8.74        8.80      0.18  [    8.72,     8.88]  ns/op

[END 03:37:48] CPU: (frequency unavailable)

================================================================================
  SPSC Throughput (1 producer, 1 consumer threads)
================================================================================

[START 03:37:48] CPU: (frequency unavailable)
Contract: Dedicated producer and consumer threads. Native SPSC use case.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       3.82        3.82      0.07  [    3.79,     3.85]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           1.56        1.62      0.13  [    1.56,     1.68]  ns/op
std::mutex + std::queue (baseline)        31.39       35.36      8.90  [   31.46,    39.26]  ns/op
moodycamel::ConcurrentQueue               22.02       21.08      2.30  [   20.07,    22.09]  ns/op
boost::lockfree::queue                    86.04       83.50     11.19  [   78.60,    88.40]  ns/op

[END 03:37:49] CPU: (frequency unavailable)

================================================================================
  MPMC Scaling (N producers, N consumers)
================================================================================

[START 03:37:49] CPU: (frequency unavailable)
Contract: Equal producer and consumer threads. Tests lock-free scaling.

Thread counts: 1 2 4 

Library                                      1T          2T          4T
-----------------------------------------------------------------------
fat_p::LockFreeQueue                        6.8        41.4        48.7 ns/op
fat_p::WorkQueue (sharded)                  7.1        33.8        24.1 ns/op
fat_p::WorkQueue (round-robin)              7.2        30.2        44.2 ns/op
fat_p::WorkQueue (stride-3)                 7.0        16.1        31.2 ns/op
fat_p::LockFreeRingBufferMPMC               7.5        43.5        38.6 ns/op
std::mutex + std::queue (baseline)         32.5        55.9        64.1 ns/op
moodycamel::ConcurrentQueue                25.5        59.4        47.2 ns/op
boost::lockfree::queue                     89.2       147.0       151.5 ns/op

[END 03:37:51] CPU: (frequency unavailable)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 03:37:51] CPU: (frequency unavailable)
Contract: Tests non-symmetric producer/consumer ratios.
Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       33.4      34.7      42.3      41.3      37.8      42.1 ns/op
fat_p::WorkQueue (sharded)                  6.8       7.3      19.7      20.7      11.7      16.5 ns/op
fat_p::LockFreeRingBufferMPMC              33.0      33.0      42.1      39.3      33.5      41.4 ns/op
std::mutex + std::queue (baseline)         39.0      35.9     254.0     111.0      56.2     160.9 ns/op
moodycamel::ConcurrentQueue                12.5      16.4      94.7      48.4      54.3      74.7 ns/op
boost::lockfree::queue                     87.7      85.3     135.3     120.9      96.9     209.0 ns/op

[END 03:38:04] CPU: (frequency unavailable)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
[FINAL 03:38:04] CPU: (frequency unavailable)
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=15 | seed=12345

```
[INIT 04:11:19] CPU: (frequency unavailable)

Correctness verification:
  [PASS] LockFreeQueue FIFO ordering
  [PASS] LockFreeRingBuffer (SPSC) FIFO ordering
  [PASS] LockFreeRingBufferMPMC FIFO ordering
================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 04:11:19] CPU: (frequency unavailable)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       4.48        4.54      0.23  [    4.43,     4.64]  ns/op
fat_p::WorkQueue (sharded)                 4.76        4.87      0.48  [    4.65,     5.08]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           1.29        1.31      0.03  [    1.30,     1.32]  ns/op
fat_p::LockFreeRingBufferMPMC              4.45        4.45      0.04  [    4.43,     4.47]  ns/op
std::mutex + std::queue (baseline)         7.03        8.31      3.09  [    6.95,     9.66]  ns/op
moodycamel::ConcurrentQueue               11.36       11.32      0.20  [   11.23,    11.40]  ns/op
boost::lockfree::queue                     8.57        8.70      0.59  [    8.44,     8.95]  ns/op

[END 04:11:20] CPU: (frequency unavailable)

================================================================================
  SPSC Throughput (1 producer, 1 consumer threads)
================================================================================

[START 04:11:20] CPU: (frequency unavailable)
Contract: Dedicated producer and consumer threads. Native SPSC use case.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       3.88        3.87      0.09  [    3.83,     3.91]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           1.75        1.75      0.09  [    1.71,     1.79]  ns/op
std::mutex + std::queue (baseline)        29.21       32.51      4.92  [   30.36,    34.67]  ns/op
moodycamel::ConcurrentQueue               28.49       28.58      0.60  [   28.32,    28.84]  ns/op
boost::lockfree::queue                   130.97      127.99     18.12  [  120.05,   135.94]  ns/op

[END 04:11:21] CPU: (frequency unavailable)

================================================================================
  MPMC Scaling (N producers, N consumers)
================================================================================

[START 04:11:21] CPU: (frequency unavailable)
Contract: Equal producer and consumer threads. Tests lock-free scaling.

Thread counts: 1 2 4 

Library                                      1T          2T          4T
-----------------------------------------------------------------------
fat_p::LockFreeQueue                        7.2        40.8        43.9 ns/op
fat_p::WorkQueue (sharded)                  7.5        31.2        22.0 ns/op
fat_p::WorkQueue (round-robin)              7.2        37.0        30.8 ns/op
fat_p::WorkQueue (stride-3)                 7.3        27.6        29.6 ns/op
fat_p::LockFreeRingBufferMPMC               7.3        42.2        46.1 ns/op
std::mutex + std::queue (baseline)         31.4        56.1        59.2 ns/op
moodycamel::ConcurrentQueue                24.6        58.1        44.3 ns/op
boost::lockfree::queue                     83.2       126.3       146.5 ns/op

[END 04:11:23] CPU: (frequency unavailable)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 04:11:23] CPU: (frequency unavailable)
Contract: Tests non-symmetric producer/consumer ratios.
Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       31.7      32.4      42.1      41.3      34.6      47.9 ns/op
fat_p::WorkQueue (sharded)                  6.0       6.0      19.4      16.7      11.1      17.6 ns/op
fat_p::LockFreeRingBufferMPMC              36.5      35.2      40.6      38.0      38.0      41.5 ns/op
std::mutex + std::queue (baseline)         37.4      36.4     262.4     129.0      58.6     158.6 ns/op
moodycamel::ConcurrentQueue                15.9      20.0      94.8      47.0      53.5      77.1 ns/op
boost::lockfree::queue                     92.0      88.5     150.0     120.2     100.1     210.9 ns/op

[END 04:11:36] CPU: (frequency unavailable)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
[FINAL 04:11:36] CPU: (frequency unavailable)
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=15 | seed=12345

```
[INIT 04:54:29] CPU: 2445 MHz (base: 2445)

Correctness verification:
  [PASS] LockFreeQueue FIFO ordering
  [PASS] LockFreeRingBuffer (SPSC) FIFO ordering
  [PASS] LockFreeRingBufferMPMC FIFO ordering

[BenchmarkScope] High priority, CPU non-0 affinity

================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 04:54:29] CPU: 2445 MHz (base: 2445)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       4.42        4.43      0.07  [    4.39,     4.46]  ns/op
fat_p::WorkQueue (sharded)                 4.93        5.25      0.87  [    4.87,     5.63]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           1.80        1.85      0.09  [    1.81,     1.89]  ns/op
fat_p::LockFreeRingBufferMPMC              8.05        8.06      0.07  [    8.03,     8.09]  ns/op
std::mutex + std::queue (baseline)        20.81       21.09      1.19  [   20.57,    21.61]  ns/op
moodycamel::ConcurrentQueue               10.71       10.73      0.61  [   10.46,    11.00]  ns/op
boost::lockfree::queue                    62.07       61.60      5.34  [   59.26,    63.94]  ns/op

[END 04:54:31] CPU: 2445 MHz (base: 2445)

================================================================================
  SPSC Throughput (1 producer, 1 consumer threads)
================================================================================

[START 04:54:31] CPU: 2445 MHz (base: 2445)
Contract: Dedicated producer and consumer threads. Native SPSC use case.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       5.13        5.16      0.44  [    4.96,     5.35]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           2.42        3.00      1.63  [    2.28,     3.71]  ns/op
std::mutex + std::queue (baseline)        24.16       24.29      1.57  [   23.61,    24.98]  ns/op
moodycamel::ConcurrentQueue               22.51       22.54      2.12  [   21.61,    23.47]  ns/op
boost::lockfree::queue                   126.44      126.61      1.15  [  126.11,   127.12]  ns/op

[END 04:54:32] CPU: 2445 MHz (base: 2445)

================================================================================
  MPMC Scaling (N producers, N consumers)
================================================================================

[START 04:54:32] CPU: 2445 MHz (base: 2445)
Contract: Equal producer and consumer threads. Tests lock-free scaling.

Thread counts: 1 2 4 

Library                                      1T          2T          4T
-----------------------------------------------------------------------
fat_p::LockFreeQueue                       17.9        22.2        42.6 ns/op
fat_p::WorkQueue (sharded)                 21.4        21.7        25.7 ns/op
fat_p::WorkQueue (round-robin)             17.3        23.0        25.4 ns/op
fat_p::WorkQueue (stride-3)                17.6        28.2        25.0 ns/op
fat_p::LockFreeRingBufferMPMC              16.5        23.2        50.8 ns/op
std::mutex + std::queue (baseline)         34.0        39.5        41.7 ns/op
moodycamel::ConcurrentQueue                25.2        36.0       453.1 ns/op
boost::lockfree::queue                    108.6       104.3       511.5 ns/op

[END 04:54:52] CPU: 2445 MHz (base: 2445)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 04:54:52] CPU: 2445 MHz (base: 2445)
Contract: Tests non-symmetric producer/consumer ratios.
Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                      192.7      29.7     201.4      34.2     213.7     188.8 ns/op
fat_p::WorkQueue (sharded)                  8.1       6.8      19.8      18.0      17.6      19.4 ns/op
fat_p::LockFreeRingBufferMPMC              35.8      38.6      39.3      35.2      43.9      39.0 ns/op
std::mutex + std::queue (baseline)         31.7      30.4     100.1      71.3      35.4      42.5 ns/op
moodycamel::ConcurrentQueue                15.7      13.6      53.3      50.9      50.2      48.6 ns/op
boost::lockfree::queue                    115.4     110.1     131.2     125.7     147.2     295.1 ns/op

[END 04:55:13] CPU: 2445 MHz (base: 2445)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
[FINAL 04:55:13] CPU: 2445 MHz (base: 2445)
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
