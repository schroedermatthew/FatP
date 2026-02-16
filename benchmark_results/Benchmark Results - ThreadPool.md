---
doc_id: BR-ThreadPool-001
doc_type: "Benchmark Results"
title: "ThreadPool"
fatp_components: ["ThreadPool"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - ThreadPool

**Source:** `benchmark_ThreadPool.cpp`
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
| fat_p::ThreadPool | x | x | x | x |
| std::async | x | x | x | x |
| MutexPool (hand-rolled) | x | x | x | x |
| Intel TBB task_group (not found) | — | — | — | — |
| Boost.Asio thread_pool | x | x | x | x |
| Intel TBB task_group | — | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
[PASS] Basic submit+get
  [PASS] No lost tasks (10000/10000)
  [PASS] Priority ordering (Critical before Low)
  [PASS] submit_batch (5000/5000)
  [PASS] Exception propagation
  [PASS] Concurrent submission (8000/8000)

[BenchmarkScope] High priority, CPU non-0 affinity

================================================================================
  Section 1: Submission Overhead
================================================================================

Contract: submit() is O(1) amortized. submit_batch() amortizes lock + notify.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
submit() [empty lambda]                  294.70      300.55     21.25  [  289.80,   311.31]  ns/op
submit_priority(Critical)                311.81      316.20     24.76  [  303.67,   328.73]  ns/op
submit_batch() per task                   89.55       90.44      5.01  [   87.90,    92.97]  ns/op
MutexPool submit()                       359.29      359.53     43.88  [  337.33,   381.74]  ns/op
Boost.Asio post()                        378.32      373.66     25.16  [  360.92,   386.39]  ns/op

================================================================================
  Section 2: Throughput Scaling (tasks/sec)
================================================================================

Contract: Throughput scales sublinearly due to queue contention.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [1 worker]                1060050.82  1088852.24  69332.80  [1053765.00, 1123939.48]  tasks/s
ThreadPool [2 workers]               1373937.77  1363636.96  81011.24  [1322639.61, 1404634.31]  tasks/s
ThreadPool [4 workers]               1215184.95  1208127.54  27207.17  [1194358.82, 1221896.27]  tasks/s
ThreadPool [8 workers]               1158716.33  1159308.04  53566.13  [1132199.84, 1186416.24]  tasks/s
ThreadPool [24 workers]              1827872.87  1828437.12  14366.31  [1821166.77, 1835707.48]  tasks/s
MutexPool [4 workers]                1277054.40  1271573.40  17757.91  [1262586.66, 1280560.14]  tasks/s
std::async [10000 tasks]             1075951.41  1078536.46 104758.52  [1025521.33, 1131551.59]  tasks/s
Boost.Asio [4 workers]               1371516.35  1337351.32  91744.45  [1290922.22, 1383780.42]  tasks/s

================================================================================
  Section 3: Latency Distribution (submit-to-execute)
================================================================================

Contract: Latency depends on spin config. Higher spin = lower p50/p90, higher CPU.

  spin=0 us (sleep only)     p50=     1.0 us  p90=     2.5 us  p99=    55.6 us  p99.9=   473.3 us  (n=750000)
  spin=1000 us               p50=     1.0 us  p90=     2.1 us  p99=    17.5 us  p99.9=   112.5 us  (n=750000)
  spin=2000 us (default)     p50=     1.0 us  p90=     2.0 us  p99=    13.9 us  p99.9=    84.1 us  (n=750000)
  spin=5000 us               p50=     1.0 us  p90=     2.0 us  p99=    23.9 us  p99.9=   324.2 us  (n=750000)
  spin=10000 us              p50=     1.0 us  p90=     2.1 us  p99=    23.6 us  p99.9=   511.6 us  (n=750000)

================================================================================
  Section 4: Priority Scheduling Effectiveness
================================================================================

Contract: Critical/High tasks use the global queue; Normal/Low use local queues.

High/Critical tasks executing in first 50% of execution order:
  Median: 99.8%  (ideal: >75% for 50/50 mix)
  Mean:   99.6%  CI95: [99.3%, 99.8%]
  (Random scheduling would yield ~50%; priority should push this higher)

================================================================================
  Section 5: Work Stealing Effectiveness
================================================================================

Contract: Fisher-Yates victim selection ensures starvation-free stealing.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
Balanced (round-robin)                    40.76       41.12      0.89  [   40.67,    41.57]  ms
Batch (single notify)                     15.49       15.65      1.25  [   15.01,    16.28]  ms
Sequential (no pool)                       1.61        2.05      0.70  [    1.70,     2.40]  ms

================================================================================
  Section 6: Head-to-Head vs std::async
================================================================================

Contract: std::async may create a thread per call (libstdc++) or serialize (MSVC).

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [100 tasks]                     0.23        0.25      0.06  [    0.22,     0.28]  ms
std::async  [100 tasks]                    0.13        0.13      0.04  [    0.11,     0.15]  ms
MutexPool   [100 tasks]                    0.08        0.09      0.03  [    0.07,     0.10]  ms
Boost.Asio  [100 tasks]                    0.08        0.08      0.01  [    0.07,     0.08]  ms

ThreadPool [1000 tasks]                    0.60        0.59      0.05  [    0.56,     0.62]  ms
std::async  [1000 tasks]                   0.83        0.85      0.09  [    0.81,     0.89]  ms
MutexPool   [1000 tasks]                   0.56        0.56      0.09  [    0.52,     0.61]  ms
Boost.Asio  [1000 tasks]                   0.69        0.74      0.12  [    0.68,     0.80]  ms

ThreadPool [10000 tasks]                   7.19        7.11      0.48  [    6.86,     7.35]  ms
std::async  [10000 tasks]                  8.41        8.52      0.59  [    8.23,     8.82]  ms
MutexPool   [10000 tasks]                  7.75        7.73      0.51  [    7.47,     7.99]  ms
Boost.Asio  [10000 tasks]                  6.95        7.04      0.57  [    6.75,     7.33]  ms

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 24 threads
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=15 | seed=12345

```
[PASS] Basic submit+get
  [PASS] No lost tasks (10000/10000)
  [PASS] Priority ordering (Critical before Low)
  [PASS] submit_batch (5000/5000)
  [PASS] Exception propagation
  [PASS] Concurrent submission (8000/8000)
================================================================================
  Section 1: Submission Overhead
================================================================================

Contract: submit() is O(1) amortized. submit_batch() amortizes lock + notify.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
submit() [empty lambda]                  338.65      357.85     89.42  [  318.66,   397.04]  ns/op
submit_priority(Critical)                330.95      396.86    134.06  [  338.11,   455.62]  ns/op
submit_batch() per task                  143.69      126.53     34.33  [  111.49,   141.58]  ns/op
MutexPool submit()                       179.72      197.94     54.36  [  174.12,   221.77]  ns/op
TBB task_group submit()                  220.57      218.65      7.52  [  215.35,   221.95]  ns/op
Boost.Asio post()                        464.01      421.88    177.49  [  344.09,   499.67]  ns/op
================================================================================
  Section 2: Throughput Scaling (tasks/sec)
================================================================================

Contract: Throughput scales sublinearly due to queue contention.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [1 worker]                 971348.27   967273.22  13675.51  [961279.66, 973266.78]  tasks/s
ThreadPool [2 workers]               1499340.39  1490456.45  28667.88  [1477892.20, 1503020.70]  tasks/s
ThreadPool [4 workers]                590529.11   587350.32  95567.84  [545465.87, 629234.77]  tasks/s
ThreadPool [8 workers]                115317.63   114041.14  15955.57  [107048.30, 121033.97]  tasks/s
MutexPool [4 workers]                 487636.96   485304.47  14746.57  [478841.50, 491767.44]  tasks/s
std::async [10000 tasks]               21328.02    21307.98    141.49  [21245.96, 21369.99]  tasks/s
TBB task_arena [4 workers]           1736719.76  1724739.65  51414.81  [1702206.11, 1747273.18]  tasks/s
Boost.Asio [4 workers]                948196.77   972813.48  70524.27  [941904.86, 1003722.10]  tasks/s
================================================================================
  Section 3: Latency Distribution (submit-to-execute)
================================================================================

Contract: Latency depends on spin config. Higher spin = lower p50/p90, higher CPU.

  spin=0 us (sleep only)     p50=     2.2 us  p90=    21.0 us  p99=   982.0 us  p99.9=  3675.1 us  (n=1000000)
  spin=1000 us               p50=     1.5 us  p90=   424.7 us  p99=  4133.2 us  p99.9=  6299.1 us  (n=1000000)
  spin=2000 us (default)     p50=     1.5 us  p90=   105.2 us  p99=  3428.6 us  p99.9=  6627.8 us  (n=1000000)
  spin=5000 us               p50=     1.5 us  p90=    18.4 us  p99=  2121.7 us  p99.9=  3703.4 us  (n=1000000)
  spin=10000 us              p50=     1.5 us  p90=    31.0 us  p99=  3025.6 us  p99.9=  4111.6 us  (n=1000000)
================================================================================
  Section 4: Priority Scheduling Effectiveness
================================================================================

Contract: Critical/High tasks use the global queue; Normal/Low use local queues.

High/Critical tasks executing in first 50% of execution order:
  Median: 99.8%  (ideal: >75% for 50/50 mix)
  Mean:   99.6%  CI95: [99.3%, 99.8%]
  (Random scheduling would yield ~50%; priority should push this higher)
================================================================================
  Section 5: Work Stealing Effectiveness
================================================================================

Contract: Fisher-Yates victim selection ensures starvation-free stealing.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
Balanced (round-robin)                    89.50       87.87     17.21  [   80.33,    95.42]  ms
Batch (single notify)                     39.53       39.50      0.40  [   39.32,    39.68]  ms
Sequential (no pool)                       3.26        3.26      0.06  [    3.23,     3.29]  ms
================================================================================
  Section 6: Head-to-Head vs std::async
================================================================================

Contract: std::async may create a thread per call (libstdc++) or serialize (MSVC).

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [100 tasks]                     0.10        0.14      0.19  [    0.06,     0.23]  ms
  [NOTE] high variance (stddev 0.19 > median 0.10)
std::async  [100 tasks]                    4.58        4.60      0.30  [    4.46,     4.73]  ms
MutexPool   [100 tasks]                    0.27        0.35      0.22  [    0.26,     0.45]  ms
TBB         [100 tasks]                    0.13        0.13      0.01  [    0.12,     0.14]  ms
Boost.Asio  [100 tasks]                    0.14        0.14      0.03  [    0.13,     0.15]  ms

ThreadPool [1000 tasks]                    0.65        0.81      0.37  [    0.64,     0.97]  ms
std::async  [1000 tasks]                  45.90       45.87      0.87  [   45.49,    46.25]  ms
MutexPool   [1000 tasks]                   0.92        0.97      0.19  [    0.89,     1.06]  ms
TBB         [1000 tasks]                   1.03        1.03      0.02  [    1.02,     1.04]  ms
Boost.Asio  [1000 tasks]                   0.67        0.69      0.09  [    0.65,     0.73]  ms

ThreadPool [10000 tasks]                  12.55       12.91      3.03  [   11.58,    14.24]  ms
std::async  [10000 tasks]                472.15      472.36      3.94  [  470.63,   474.08]  ms
MutexPool   [10000 tasks]                  9.44        9.52      0.80  [    9.17,     9.88]  ms
TBB         [10000 tasks]                  9.94        9.94      0.09  [    9.90,     9.98]  ms
Boost.Asio  [10000 tasks]                  5.68        5.69      0.11  [    5.64,     5.74]  ms
================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=15 | seed=12345

```
[PASS] Basic submit+get
  [PASS] No lost tasks (10000/10000)
  [PASS] Priority ordering (Critical before Low)
  [PASS] submit_batch (5000/5000)
  [PASS] Exception propagation
  [PASS] Concurrent submission (8000/8000)
================================================================================
  Section 1: Submission Overhead
================================================================================

Contract: submit() is O(1) amortized. submit_batch() amortizes lock + notify.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
submit() [empty lambda]                  338.31      467.96    244.62  [  360.75,   575.17]  ns/op
submit_priority(Critical)                292.65      306.11     60.68  [  279.52,   332.70]  ns/op
submit_batch() per task                   87.56       80.80     25.69  [   69.55,    92.06]  ns/op
MutexPool submit()                       163.13      163.51      0.89  [  163.12,   163.90]  ns/op
TBB task_group submit()                  297.04      295.38     86.08  [  257.66,   333.11]  ns/op
Boost.Asio post()                        624.52      644.18    246.27  [  536.25,   752.12]  ns/op
================================================================================
  Section 2: Throughput Scaling (tasks/sec)
================================================================================

Contract: Throughput scales sublinearly due to queue contention.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [1 worker]                 960373.87   958920.91  18958.64  [950611.92, 967229.90]  tasks/s
ThreadPool [2 workers]               1457597.07  1437126.89  63456.04  [1409316.06, 1464937.73]  tasks/s
ThreadPool [4 workers]                437051.73   436691.66  86956.64  [398581.23, 474802.08]  tasks/s
ThreadPool [8 workers]                 83589.11    83989.60   9608.21  [79778.62, 88200.58]  tasks/s
MutexPool [4 workers]                 516865.28   519281.89  12938.34  [513611.41, 524952.36]  tasks/s
std::async [10000 tasks]               21184.69    21212.21    305.71  [21078.23, 21346.20]  tasks/s
TBB task_arena [4 workers]           1674150.77  1635039.11 102737.72  [1590012.32, 1680065.91]  tasks/s
Boost.Asio [4 workers]               1036531.56  1047694.26  66867.78  [1018388.16, 1077000.36]  tasks/s
================================================================================
  Section 3: Latency Distribution (submit-to-execute)
================================================================================

Contract: Latency depends on spin config. Higher spin = lower p50/p90, higher CPU.

  spin=0 us (sleep only)     p50=     1.9 us  p90=    16.4 us  p99=   125.0 us  p99.9=  1148.6 us  (n=1000000)
  spin=1000 us               p50=     1.7 us  p90=  1162.1 us  p99=  6375.6 us  p99.9=  9217.8 us  (n=1000000)
  spin=2000 us (default)     p50=     1.6 us  p90=   762.9 us  p99=  3713.8 us  p99.9=  5635.1 us  (n=1000000)
  spin=5000 us               p50=     1.7 us  p90=  1237.5 us  p99=  5193.1 us  p99.9=  6140.2 us  (n=1000000)
  spin=10000 us              p50=     1.6 us  p90=   754.4 us  p99=  5579.0 us  p99.9=  7650.1 us  (n=1000000)
================================================================================
  Section 4: Priority Scheduling Effectiveness
================================================================================

Contract: Critical/High tasks use the global queue; Normal/Low use local queues.

High/Critical tasks executing in first 50% of execution order:
  Median: 99.8%  (ideal: >75% for 50/50 mix)
  Mean:   99.6%  CI95: [99.3%, 99.8%]
  (Random scheduling would yield ~50%; priority should push this higher)
================================================================================
  Section 5: Work Stealing Effectiveness
================================================================================

Contract: Fisher-Yates victim selection ensures starvation-free stealing.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
Balanced (round-robin)                    92.50       94.91     21.34  [   85.55,   104.26]  ms
Batch (single notify)                     39.71       39.76      1.00  [   39.32,    40.20]  ms
Sequential (no pool)                       4.10        4.12      0.06  [    4.09,     4.14]  ms
================================================================================
  Section 6: Head-to-Head vs std::async
================================================================================

Contract: std::async may create a thread per call (libstdc++) or serialize (MSVC).

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [100 tasks]                     0.05        0.05      0.01  [    0.05,     0.06]  ms
std::async  [100 tasks]                    4.54        4.55      0.27  [    4.44,     4.67]  ms
MutexPool   [100 tasks]                    0.71        0.69      0.22  [    0.60,     0.79]  ms
TBB         [100 tasks]                    0.10        0.10      0.01  [    0.10,     0.10]  ms
Boost.Asio  [100 tasks]                    0.14        0.16      0.07  [    0.13,     0.19]  ms

ThreadPool [1000 tasks]                    0.59        1.45      1.56  [    0.76,     2.13]  ms
  [NOTE] high variance (stddev 1.56 > median 0.59)
std::async  [1000 tasks]                  45.24       45.76      1.96  [   44.90,    46.62]  ms
MutexPool   [1000 tasks]                   4.86        5.05      1.00  [    4.61,     5.49]  ms
TBB         [1000 tasks]                   0.78        0.78      0.02  [    0.77,     0.79]  ms
Boost.Asio  [1000 tasks]                   1.22        1.18      0.30  [    1.04,     1.31]  ms

ThreadPool [10000 tasks]                  15.78       16.42      5.14  [   14.17,    18.67]  ms
std::async  [10000 tasks]                473.73      473.09      6.16  [  470.39,   475.79]  ms
MutexPool   [10000 tasks]                 22.57       22.54      1.96  [   21.68,    23.39]  ms
TBB         [10000 tasks]                  7.54        7.53      0.08  [    7.50,     7.56]  ms
Boost.Asio  [10000 tasks]                  9.90        9.52      1.75  [    8.75,    10.29]  ms
================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=15 | seed=12345

```
[PASS] Basic submit+get
  [PASS] No lost tasks (10000/10000)
  [PASS] Priority ordering (Critical before Low)
  [PASS] submit_batch (5000/5000)
  [PASS] Exception propagation
  [PASS] Concurrent submission (8000/8000)

[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  Section 1: Submission Overhead
================================================================================

Contract: submit() is O(1) amortized. submit_batch() amortizes lock + notify.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
submit() [empty lambda]                  599.93      598.58      6.70  [  595.64,   601.51]  ns/op
submit_priority(Critical)                660.25      665.34     16.66  [  658.03,   672.64]  ns/op
submit_batch() per task                  172.33      174.34      6.64  [  171.43,   177.25]  ns/op
MutexPool submit()                       587.88      585.67     16.60  [  578.40,   592.95]  ns/op
TBB task_group submit()                  621.14      622.51     22.20  [  612.78,   632.24]  ns/op
Boost.Asio post()                        613.09      613.52     10.60  [  608.88,   618.17]  ns/op
================================================================================
  Section 2: Throughput Scaling (tasks/sec)
================================================================================

Contract: Throughput scales sublinearly due to queue contention.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [1 worker]                1461994.61  1404547.53 366693.05  [1243837.22, 1565257.85]  tasks/s
ThreadPool [2 workers]               1147067.32  1153811.47 230458.33  [1052808.66, 1254814.29]  tasks/s
ThreadPool [4 workers]               1001380.45   980190.32 161772.45  [909290.42, 1051090.22]  tasks/s
ThreadPool [8 workers]                879943.01   889525.44 311700.35  [752916.74, 1026134.14]  tasks/s
MutexPool [4 workers]                 917379.72   934343.88  48274.45  [913186.67, 955501.09]  tasks/s
std::async [10000 tasks]              551030.34   542841.40  55574.03  [518485.01, 567197.79]  tasks/s
TBB task_arena [4 workers]           1450991.96  1459683.74  43769.96  [1440500.71, 1478866.77]  tasks/s
Boost.Asio [4 workers]                152168.52   152069.14   9840.60  [147756.30, 156381.97]  tasks/s
================================================================================
  Section 3: Latency Distribution (submit-to-execute)
================================================================================

Contract: Latency depends on spin config. Higher spin = lower p50/p90, higher CPU.

  spin=0 us (sleep only)     p50=     5.4 us  p90=    15.2 us  p99=    37.8 us  p99.9=  5223.7 us  (n=1000000)
  spin=1000 us               p50=     1.4 us  p90=     1.9 us  p99=     7.8 us  p99.9=    43.5 us  (n=1000000)
  spin=2000 us (default)     p50=     1.4 us  p90=     2.2 us  p99=    12.7 us  p99.9=    39.0 us  (n=1000000)
  spin=5000 us               p50=     1.4 us  p90=     1.9 us  p99=     8.1 us  p99.9=    43.1 us  (n=1000000)
  spin=10000 us              p50=     1.4 us  p90=     1.9 us  p99=     7.4 us  p99.9=    39.9 us  (n=1000000)
================================================================================
  Section 4: Priority Scheduling Effectiveness
================================================================================

Contract: Critical/High tasks use the global queue; Normal/Low use local queues.

High/Critical tasks executing in first 50% of execution order:
  Median: 99.8%  (ideal: >75% for 50/50 mix)
  Mean:   99.6%  CI95: [99.3%, 99.8%]
  (Random scheduling would yield ~50%; priority should push this higher)
================================================================================
  Section 5: Work Stealing Effectiveness
================================================================================

Contract: Fisher-Yates victim selection ensures starvation-free stealing.

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
Balanced (round-robin)                    48.59       51.64     11.80  [   46.47,    56.81]  ms
Batch (single notify)                     41.08       41.08      0.63  [   40.81,    41.36]  ms
Sequential (no pool)                      10.04       10.12      0.23  [   10.02,    10.22]  ms
================================================================================
  Section 6: Head-to-Head vs std::async
================================================================================

Contract: std::async may create a thread per call (libstdc++) or serialize (MSVC).

Benchmark                                Median        Mean    StdDev  [    CI95_lo,   CI95_hi]  Unit
-----------------------------------------------------------------------------------------------
ThreadPool [100 tasks]                     0.08        0.08      0.01  [    0.08,     0.09]  ms
std::async  [100 tasks]                    0.17        0.17      0.02  [    0.17,     0.18]  ms
MutexPool   [100 tasks]                    0.11        0.12      0.02  [    0.11,     0.13]  ms
TBB         [100 tasks]                    0.11        0.11      0.01  [    0.11,     0.12]  ms
Boost.Asio  [100 tasks]                    0.21        0.22      0.08  [    0.19,     0.26]  ms

ThreadPool [1000 tasks]                    0.73        1.15      1.79  [    0.36,     1.94]  ms
  [NOTE] high variance (stddev 1.79 > median 0.73)
std::async  [1000 tasks]                   1.50        1.50      0.06  [    1.48,     1.53]  ms
MutexPool   [1000 tasks]                   0.90        0.90      0.06  [    0.87,     0.92]  ms
TBB         [1000 tasks]                   1.15        1.12      0.12  [    1.06,     1.17]  ms
Boost.Asio  [1000 tasks]                   3.00        2.99      0.41  [    2.81,     3.17]  ms

ThreadPool [10000 tasks]                   8.17       11.79      8.47  [    8.08,    15.50]  ms
  [NOTE] high variance (stddev 8.47 > median 8.17)
std::async  [10000 tasks]                 16.22       16.97      2.35  [   15.94,    18.00]  ms
MutexPool   [10000 tasks]                  9.83       10.21      1.53  [    9.54,    10.88]  ms
TBB         [10000 tasks]                 11.35       11.38      0.29  [   11.25,    11.51]  ms
Boost.Asio  [10000 tasks]                 42.53       46.90     14.17  [   40.69,    53.12]  ms
================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 4 threads
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- Intel TBB task_group (not found) was not detected on Local.
