---
doc_id: BR-AllocationStrategies-001
doc_type: "Benchmark Results"
title: "AllocationStrategies"
fatp_components: ["AllocationStrategies"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - AllocationStrategies

**Source:** `benchmark_AllocationStrategies.cpp`
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
| fat_p::AllocationStrategies | x | x | x | x |
| std::pmr | x | x | x | x |
| boost::pool | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
Waiting for CPU to stabilize...

================================================================================
  SINGLE ALLOCATION/DEALLOCATION
================================================================================

[2026-02-15 19:18:49] Start CPU: 3612 MHz (base: 3686)
Contract: Measures time for one allocate() + deallocate() cycle. Allocation includes construction, deallocation includes destruction.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                18.86         19.22        0.93  [  18.71,   19.74]
fat_p::BlockAllocator                     2.23          2.39        0.42  [   2.16,    2.62]
fat_p::PoolAllocator                      2.60          2.60        0.18  [   2.51,    2.70]
--------------------------------------------------------------------------------------------
std::allocator                           18.87         19.06        0.83  [  18.60,   19.52]
std::pmr::monotonic                       2.65          2.68        0.21  [   2.56,    2.80]
std::pmr::unsync_pool                     4.18          4.48        0.85  [   4.01,    4.96]
boost::pool (raw)                         1.26          1.25        0.02  [   1.24,    1.27]

  [Correctness: PASS]
[2026-02-15 19:18:49] End CPU: 2543 MHz (base: 3686)

================================================================================
  BURST ALLOCATION (100 objects)
================================================================================

[2026-02-15 19:18:50] Start CPU: 2211 MHz (base: 3686)
Contract: Allocate 100 objects, then deallocate all. Measures bulk allocation pattern common in container growth.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator              2395.19       2399.73       31.42  [2382.37, 2417.10]
fat_p::BlockAllocator                   199.20        199.28        9.49  [ 194.04,  204.52]
fat_p::PoolAllocator                    203.70        208.31       18.34  [ 198.18,  218.45]
--------------------------------------------------------------------------------------------
std::allocator                         2383.95       2394.57       43.36  [2370.61, 2418.52]
std::pmr::monotonic                     233.11        233.19       17.28  [ 223.64,  242.74]
std::pmr::unsync_pool                   697.38        700.51       32.16  [ 682.74,  718.28]
boost::pool (raw)                       186.50        188.06        9.86  [ 182.61,  193.50]
[2026-02-15 19:18:51] End CPU: 2617 MHz (base: 3686)

================================================================================
  CHURN PATTERN (Steady-State Mixed Operations)
================================================================================

[2026-02-15 19:18:51] Start CPU: 2285 MHz (base: 3686)
Contract: Simulates container churn with interleaved alloc/dealloc. Free list reuse should show advantage for BlockAllocator/PoolAllocator.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                18.29         18.94        1.17  [  18.29,   19.59]
fat_p::BlockAllocator (warmed)            1.76          1.80        0.05  [   1.77,    1.83]
fat_p::PoolAllocator (warmed)             2.50          2.52        0.06  [   2.49,    2.56]
boost::pool (warmed)                      1.23          1.23        0.03  [   1.21,    1.25]
[2026-02-15 19:18:51] End CPU: 3059 MHz (base: 3686)

================================================================================
  SIZE SCALING (Allocation Count)
================================================================================

[2026-02-15 19:18:51] Start CPU: 2359 MHz (base: 3686)
Contract: Measures how allocation time scales with number of live objects. BlockAllocator should show constant time regardless of count.

--- N = 100 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.38         21.00        5.17  [  18.15,   23.86]
fat_p::BlockAllocator                     2.05          2.15        0.41  [   1.92,    2.38]
boost::pool                               1.26          1.24        0.06  [   1.21,    1.27]

--- N = 1000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.22         19.59        0.94  [  19.07,   20.11]
fat_p::BlockAllocator                     2.05          2.02        0.05  [   1.99,    2.05]
boost::pool                               1.25          1.22        0.09  [   1.17,    1.27]

--- N = 10000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                18.96         21.19        7.54  [  17.03,   25.36]
fat_p::BlockAllocator                     1.94          1.99        0.11  [   1.93,    2.05]
boost::pool                               1.18          1.17        0.09  [   1.13,    1.22]

--- N = 50000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.24         19.70        2.01  [  18.59,   20.81]
fat_p::BlockAllocator                     2.05          2.11        0.21  [   2.00,    2.23]
boost::pool                               1.26          1.23        0.10  [   1.18,    1.29]
[2026-02-15 19:18:51] End CPU: 2469 MHz (base: 3686)

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  SINGLE ALLOCATION/DEALLOCATION
================================================================================

[2026-02-16 03:37:46] Start CPU: 3244 MHz (~base: 3244)
Contract: Measures time for one allocate() + deallocate() cycle. Allocation includes construction, deallocation includes destruction.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.95         13.90        0.20  [  13.81,   14.00]
fat_p::BlockAllocator                     0.62          0.62        0.02  [   0.61,    0.63]
fat_p::PoolAllocator                      1.24          1.26        0.04  [   1.24,    1.28]
--------------------------------------------------------------------------------------------
std::allocator                           14.26         14.16        0.16  [  14.08,   14.23]
std::pmr::monotonic                       1.47          1.48        0.03  [   1.47,    1.50]
std::pmr::unsync_pool                    22.97         22.98        0.09  [  22.93,   23.02]
boost::pool (raw)                         0.62          0.62        0.02  [   0.61,    0.64]

  [Correctness: PASS]
[2026-02-16 03:37:46] End CPU: 3241 MHz (~base: 3241)

================================================================================
  BURST ALLOCATION (100 objects)
================================================================================

[2026-02-16 03:37:46] Start CPU: 2445 MHz (~base: 2445)
Contract: Allocate 100 objects, then deallocate all. Measures bulk allocation pattern common in container growth.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator              1630.49       1634.28        9.17  [1629.89, 1638.67]
fat_p::BlockAllocator                   174.48        175.53        3.37  [ 173.92,  177.15]
fat_p::PoolAllocator                    223.81        223.91        3.26  [ 222.34,  225.47]
--------------------------------------------------------------------------------------------
std::allocator                         1656.73       1659.90        6.79  [1656.65, 1663.15]
std::pmr::monotonic                     138.99        139.07        0.94  [ 138.62,  139.52]
std::pmr::unsync_pool                  2571.67       2574.13        9.77  [2569.45, 2578.80]
boost::pool (raw)                       323.66        324.91        3.70  [ 323.14,  326.67]
[2026-02-16 03:37:48] End CPU: 2445 MHz (~base: 2445)

================================================================================
  CHURN PATTERN (Steady-State Mixed Operations)
================================================================================

[2026-02-16 03:37:48] Start CPU: 2585 MHz (~base: 2585)
Contract: Simulates container churn with interleaved alloc/dealloc. Free list reuse should show advantage for BlockAllocator/PoolAllocator.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.34         13.43        0.23  [  13.31,   13.54]
fat_p::BlockAllocator (warmed)            0.62          0.63        0.03  [   0.62,    0.65]
fat_p::PoolAllocator (warmed)             1.24          1.24        0.02  [   1.23,    1.26]
boost::pool (warmed)                      0.62          0.63        0.03  [   0.62,    0.64]
[2026-02-16 03:37:48] End CPU: 2445 MHz (~base: 2445)

================================================================================
  SIZE SCALING (Allocation Count)
================================================================================

[2026-02-16 03:37:48] Start CPU: 2445 MHz (~base: 2445)
Contract: Measures how allocation time scales with number of live objects. BlockAllocator should show constant time regardless of count.
--- N = 100 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.57         13.76        0.52  [  13.51,   14.01]
fat_p::BlockAllocator                     0.31          0.46        0.48  [   0.23,    0.69]
  [NOTE] High variance (stddev > median)
boost::pool                               0.62          0.62        0.02  [   0.62,    0.63]

--- N = 1000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.57         13.71        0.29  [  13.57,   13.85]
fat_p::BlockAllocator                     0.31          0.31        0.00  [   0.31,    0.31]
boost::pool                               0.62          0.62        0.00  [   0.62,    0.62]

--- N = 10000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.57         13.86        1.11  [  13.33,   14.39]
fat_p::BlockAllocator                     0.31          0.31        0.00  [   0.31,    0.31]
boost::pool                               0.62          0.66        0.18  [   0.57,    0.75]

--- N = 50000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.57         13.74        0.36  [  13.56,   13.91]
fat_p::BlockAllocator                     0.31          0.31        0.00  [   0.31,    0.31]
boost::pool                               0.62          0.62        0.00  [   0.62,    0.62]
[2026-02-16 03:37:49] End CPU: 3247 MHz (~base: 3247)

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  SINGLE ALLOCATION/DEALLOCATION
================================================================================

[2026-02-16 04:11:17] Start CPU: 2948 MHz (~base: 2948)
Contract: Measures time for one allocate() + deallocate() cycle. Allocation includes construction, deallocation includes destruction.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.09         13.10        0.08  [  13.06,   13.14]
fat_p::BlockAllocator                     0.62          0.62        0.00  [   0.62,    0.62]
fat_p::PoolAllocator                      1.54          1.54        0.00  [   1.54,    1.54]
--------------------------------------------------------------------------------------------
std::allocator                           13.03         13.06        0.04  [  13.03,   13.08]
std::pmr::monotonic                       5.63          5.66        0.12  [   5.60,    5.71]
std::pmr::unsync_pool                    22.65         22.74        0.21  [  22.64,   22.83]
boost::pool (raw)                         1.23          1.26        0.05  [   1.23,    1.28]

  [Correctness: PASS]
[2026-02-16 04:11:17] End CPU: 2445 MHz (~base: 2445)

================================================================================
  BURST ALLOCATION (100 objects)
================================================================================

[2026-02-16 04:11:17] Start CPU: 2445 MHz (~base: 2445)
Contract: Allocate 100 objects, then deallocate all. Measures bulk allocation pattern common in container growth.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator              1555.52       1569.73       42.29  [1549.50, 1589.97]
fat_p::BlockAllocator                   124.03        123.89        0.49  [ 123.65,  124.12]
fat_p::PoolAllocator                    179.43        179.68        1.89  [ 178.78,  180.58]
--------------------------------------------------------------------------------------------
std::allocator                         1553.96       1556.95        8.34  [1552.96, 1560.93]
std::pmr::monotonic                     641.72        642.23        2.52  [ 641.02,  643.44]
std::pmr::unsync_pool                  2517.28       2521.47        7.33  [2517.97, 2524.98]
boost::pool (raw)                       257.59        258.13        1.81  [ 257.27,  259.00]
[2026-02-16 04:11:19] End CPU: 2445 MHz (~base: 2445)

================================================================================
  CHURN PATTERN (Steady-State Mixed Operations)
================================================================================

[2026-02-16 04:11:19] Start CPU: 2445 MHz (~base: 2445)
Contract: Simulates container churn with interleaved alloc/dealloc. Free list reuse should show advantage for BlockAllocator/PoolAllocator.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                13.04         13.06        0.05  [  13.04,   13.09]
fat_p::BlockAllocator (warmed)            0.62          0.62        0.00  [   0.62,    0.62]
fat_p::PoolAllocator (warmed)             1.54          1.55        0.03  [   1.54,    1.57]
boost::pool (warmed)                      1.23          1.24        0.03  [   1.23,    1.26]
[2026-02-16 04:11:19] End CPU: 2445 MHz (~base: 2445)

================================================================================
  SIZE SCALING (Allocation Count)
================================================================================

[2026-02-16 04:11:19] Start CPU: 2445 MHz (~base: 2445)
Contract: Measures how allocation time scales with number of live objects. BlockAllocator should show constant time regardless of count.
--- N = 100 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                12.64         12.73        0.26  [  12.61,   12.86]
fat_p::BlockAllocator                     0.62          0.62        0.00  [   0.62,    0.62]
boost::pool                               0.62          0.65        0.05  [   0.63,    0.68]

--- N = 1000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                12.64         12.73        0.22  [  12.62,   12.83]
fat_p::BlockAllocator                     0.62          0.62        0.00  [   0.62,    0.62]
boost::pool                               0.62          0.62        0.00  [   0.62,    0.62]

--- N = 10000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                12.65         12.98        0.57  [  12.70,   13.25]
fat_p::BlockAllocator                     0.62          0.62        0.01  [   0.62,    0.63]
boost::pool                               0.62          0.67        0.22  [   0.57,    0.78]

--- N = 50000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                12.65         12.79        0.34  [  12.63,   12.95]
fat_p::BlockAllocator                     0.62          0.62        0.00  [   0.62,    0.62]
boost::pool                               0.62          0.62        0.00  [   0.62,    0.62]
[2026-02-16 04:11:19] End CPU: 2445 MHz (~base: 2445)

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  SINGLE ALLOCATION/DEALLOCATION
================================================================================

[2026-02-16 04:54:04] Start CPU: 2445 MHz (base: 2445)
Contract: Measures time for one allocate() + deallocate() cycle. Allocation includes construction, deallocation includes destruction.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                37.03         37.09        0.30  [  36.95,   37.24]
fat_p::BlockAllocator                     4.01          4.07        0.16  [   3.99,    4.14]
fat_p::PoolAllocator                      4.32          4.55        0.31  [   4.41,    4.70]
--------------------------------------------------------------------------------------------
std::allocator                           36.79         36.71        0.14  [  36.64,   36.78]
std::pmr::monotonic                       5.82          5.83        0.20  [   5.74,    5.93]
std::pmr::unsync_pool                    13.86         13.86        0.17  [  13.78,   13.95]
boost::pool (raw)                         1.54          1.59        0.09  [   1.54,    1.63]

  [Correctness: PASS]
[2026-02-16 04:54:04] End CPU: 2445 MHz (base: 2445)

================================================================================
  BURST ALLOCATION (100 objects)
================================================================================

[2026-02-16 04:54:05] Start CPU: 2445 MHz (base: 2445)
Contract: Allocate 100 objects, then deallocate all. Measures bulk allocation pattern common in container growth.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator              4592.45       4601.80       29.32  [4587.77, 4615.83]
fat_p::BlockAllocator                   508.57        508.78        2.56  [ 507.56,  510.01]
fat_p::PoolAllocator                    423.66        426.98       12.79  [ 420.86,  433.10]
--------------------------------------------------------------------------------------------
std::allocator                         4630.03       4637.84       33.71  [4621.71, 4653.98]
std::pmr::monotonic                     486.90        486.71        2.40  [ 485.56,  487.85]
std::pmr::unsync_pool                  1767.42       1768.15       14.14  [1761.38, 1774.92]
boost::pool (raw)                       382.78        383.18        2.29  [ 382.08,  384.27]
[2026-02-16 04:54:08] End CPU: 2445 MHz (base: 2445)

================================================================================
  CHURN PATTERN (Steady-State Mixed Operations)
================================================================================

[2026-02-16 04:54:08] Start CPU: 2445 MHz (base: 2445)
Contract: Simulates container churn with interleaved alloc/dealloc. Free list reuse should show advantage for BlockAllocator/PoolAllocator.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                36.68         36.66        0.12  [  36.60,   36.72]
fat_p::BlockAllocator (warmed)            3.95          3.74        0.46  [   3.52,    3.96]
fat_p::PoolAllocator (warmed)             4.93          4.87        0.22  [   4.76,    4.97]
boost::pool (warmed)                      1.54          1.56        0.05  [   1.53,    1.58]
[2026-02-16 04:54:08] End CPU: 2445 MHz (base: 2445)

================================================================================
  SIZE SCALING (Allocation Count)
================================================================================

[2026-02-16 04:54:08] Start CPU: 2445 MHz (base: 2445)
Contract: Measures how allocation time scales with number of live objects. BlockAllocator should show constant time regardless of count.
--- N = 100 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                36.15         36.81        1.23  [  36.22,   37.40]
fat_p::BlockAllocator                     3.70          3.80        0.31  [   3.65,    3.95]
boost::pool                               1.54          1.54        0.01  [   1.54,    1.55]

--- N = 1000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                37.91         37.91        2.26  [  36.82,   38.99]
fat_p::BlockAllocator                     3.70          3.72        0.07  [   3.69,    3.76]
boost::pool                               1.54          1.64        0.32  [   1.49,    1.80]

--- N = 10000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                36.14         37.19        1.67  [  36.39,   37.99]
fat_p::BlockAllocator                     3.70          3.76        0.11  [   3.70,    3.81]
boost::pool                               1.54          1.55        0.01  [   1.54,    1.55]

--- N = 50000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                36.10         36.40        0.95  [  35.94,   36.85]
fat_p::BlockAllocator                     3.71          3.77        0.13  [   3.71,    3.83]
boost::pool                               1.55          1.56        0.04  [   1.54,    1.58]
[2026-02-16 04:54:08] End CPU: 2445 MHz (base: 2445)

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
