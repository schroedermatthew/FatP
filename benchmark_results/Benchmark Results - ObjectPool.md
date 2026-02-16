---
doc_id: BR-ObjectPool-001
doc_type: "Benchmark Results"
title: "ObjectPool"
fatp_components: ["ObjectPool"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - ObjectPool

**Source:** `benchmark_ObjectPool.cpp`
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
| fat_p::ObjectPool | x | x | x | x |
| boost::object_pool | x | x | x | x |
| foonathan::memory_pool | x | x | x | — |
| EASTL::fixed_pool (fixed-capacity, no auto-grow) | x | x | x | x |
| std::pmr::unsynchronized_pool_resource (C++17) | x | x | x | x |
| std::pmr::synchronized_pool_resource (C++17 thread-safe) | x | x | x | x |
| new/delete | x | x | x | x |
| foonathan::memory_pool (vcpkg install foonathan-memory) | — | — | — | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  Acquire + Release Cycle (SmallTrivial 16B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth

--- N = 1000 ops ---
[2026-02-15 19:40:15] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2543 MHz]
           fat_p::ObjectPool: median=    2.30 ns/op  mean=    2.27 +/-  0.05
          boost::object_pool: median=    2.30 ns/op  mean=    2.35 +/-  0.06
      foonathan::memory_pool: median=    3.40 ns/op  mean=    3.42 +/-  0.15
   EASTL::fixed_pool [!grow]: median=    2.10 ns/op  mean=    2.13 +/-  0.05
       std::pmr::unsync_pool: median=    6.30 ns/op  mean=    6.25 +/-  0.07
                  new/delete: median=   20.60 ns/op  mean=   20.56 +/-  0.20
  Speedup vs new/delete: 9.0x

--- N = 10000 ops ---
[2026-02-15 19:40:25] CPU: 2543 MHz (base: 3686)
[Cooling: size transition] [Ready: 2432 MHz]
           fat_p::ObjectPool: median=    2.23 ns/op  mean=    2.41 +/-  0.47
          boost::object_pool: median=    2.29 ns/op  mean=    2.32 +/-  0.15
      foonathan::memory_pool: median=    3.04 ns/op  mean=    3.04 +/-  0.14
   EASTL::fixed_pool [!grow]: median=    2.10 ns/op  mean=    2.18 +/-  0.25
       std::pmr::unsync_pool: median=    6.61 ns/op  mean=    6.93 +/-  1.30
                  new/delete: median=   20.59 ns/op  mean=   21.33 +/-  1.58
  Speedup vs new/delete: 9.2x

--- N = 100000 ops ---
[2026-02-15 19:40:32] CPU: 2432 MHz (base: 3686)
[Cooling: size transition] [Ready: 2469 MHz]
           fat_p::ObjectPool: median=    2.31 ns/op  mean=    2.29 +/-  0.18
          boost::object_pool: median=    2.30 ns/op  mean=    2.34 +/-  0.17
      foonathan::memory_pool: median=    2.89 ns/op  mean=    2.97 +/-  0.22
   EASTL::fixed_pool [!grow]: median=    2.10 ns/op  mean=    2.16 +/-  0.23
       std::pmr::unsync_pool: median=    6.21 ns/op  mean=    6.38 +/-  0.43
                  new/delete: median=   19.60 ns/op  mean=   20.15 +/-  1.54
  Speedup vs new/delete: 8.5x
[Cooling: before bulk acquire] [Ready: 2690 MHz]

================================================================================
  Bulk Acquire (SmallTrivial 16B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput

--- N = 1000 objects ---
[2026-02-15 19:40:46] CPU: 2690 MHz (base: 3686)
[Cooling: size transition] [Ready: 2948 MHz]
           fat_p::ObjectPool: median=    2.00 ns/op  mean=    2.06 +/-  0.10
          boost::object_pool: median=    3.00 ns/op  mean=    2.93 +/-  0.19
      foonathan::memory_pool: median=    2.20 ns/op  mean=    2.17 +/-  0.24
   EASTL::fixed_pool [!grow]: median=    1.60 ns/op  mean=    1.66 +/-  0.08
       std::pmr::unsync_pool: median=    5.30 ns/op  mean=    5.39 +/-  0.43
                  new/delete: median=   15.30 ns/op  mean=   15.10 +/-  0.54

--- N = 10000 objects ---
[2026-02-15 19:41:02] CPU: 2948 MHz (base: 3686)
[Cooling: size transition] [Ready: 2690 MHz]
           fat_p::ObjectPool: median=    2.17 ns/op  mean=    2.19 +/-  0.06
          boost::object_pool: median=    9.82 ns/op  mean=    9.49 +/-  1.84
      foonathan::memory_pool: median=    1.90 ns/op  mean=    2.14 +/-  0.40
   EASTL::fixed_pool [!grow]: median=    1.69 ns/op  mean=    1.73 +/-  0.23
       std::pmr::unsync_pool: median=   10.22 ns/op  mean=   10.15 +/-  1.53
                  new/delete: median=   21.63 ns/op  mean=   23.04 +/-  5.17

--- N = 100000 objects ---
[2026-02-15 19:41:19] CPU: 2543 MHz (base: 3686)
[Cooling: size transition] [Ready: 2580 MHz]
           fat_p::ObjectPool: median=    2.42 ns/op  mean=    2.56 +/-  0.38
          boost::object_pool: median=    8.62 ns/op  mean=    8.87 +/-  0.87
      foonathan::memory_pool: median=    2.42 ns/op  mean=    2.47 +/-  0.29
   EASTL::fixed_pool [!grow]: median=    2.25 ns/op  mean=    2.43 +/-  0.66
       std::pmr::unsync_pool: median=    8.97 ns/op  mean=    9.44 +/-  1.40
                  new/delete: median=   23.83 ns/op  mean=   24.04 +/-  1.21
[Cooling: before interleaved] [Ready: 2543 MHz]

================================================================================
  Interleaved Acquire/Release (SmallTrivial 16B)
================================================================================
Contract: Realistic workload with interleaved operations
          50% acquire, 50% release (steady-state simulation)

--- N = 1000 operations ---
[2026-02-15 19:42:39] CPU: 2543 MHz (base: 3686)
[Cooling: size transition] [Ready: 2653 MHz]
           fat_p::ObjectPool: median=    9.40 ns/op  mean=    9.44 +/-  0.40
          boost::object_pool: median=   14.70 ns/op  mean=   15.45 +/-  2.83
      foonathan::memory_pool: median=    9.90 ns/op  mean=   10.01 +/-  0.43
   EASTL::fixed_pool [!grow]: median=    9.30 ns/op  mean=    9.41 +/-  0.50
       std::pmr::unsync_pool: median=   13.50 ns/op  mean=   13.94 +/-  1.52
                  new/delete: median=   21.60 ns/op  mean=   22.39 +/-  2.72

--- N = 10000 operations ---
[2026-02-15 19:42:55] CPU: 2653 MHz (base: 3686)
[Cooling: size transition] [Ready: 2543 MHz]
           fat_p::ObjectPool: median=    7.71 ns/op  mean=    8.23 +/-  1.05
          boost::object_pool: median=   12.45 ns/op  mean=   12.65 +/-  0.96
      foonathan::memory_pool: median=    8.35 ns/op  mean=    8.62 +/-  0.88
   EASTL::fixed_pool [!grow]: median=    7.46 ns/op  mean=    7.45 +/-  0.38
       std::pmr::unsync_pool: median=   10.43 ns/op  mean=   10.97 +/-  1.22
                  new/delete: median=   18.26 ns/op  mean=   17.90 +/-  0.84

--- N = 100000 operations ---
[2026-02-15 19:43:03] CPU: 2543 MHz (base: 3686)
[Cooling: size transition] [Ready: 2432 MHz]
           fat_p::ObjectPool: median=    8.75 ns/op  mean=    9.06 +/-  1.35
          boost::object_pool: median=   12.68 ns/op  mean=   12.96 +/-  1.20
      foonathan::memory_pool: median=    8.55 ns/op  mean=    8.53 +/-  0.29
   EASTL::fixed_pool [!grow]: median=    8.59 ns/op  mean=    8.62 +/-  1.18
       std::pmr::unsync_pool: median=   11.37 ns/op  mean=   11.63 +/-  1.12
                  new/delete: median=   18.65 ns/op  mean=   19.20 +/-  1.23
[Cooling: before pool reuse] [Ready: 2322 MHz]

================================================================================
  Pool Reuse / Free List Efficiency (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all in random order, acquire N again
          Tests free list traversal and memory reuse

--- N = 1000 objects ---
[2026-02-15 19:43:34] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2617 MHz]
           fat_p::ObjectPool: median=    2.00 ns/op  mean=    2.39 +/-  0.70
          boost::object_pool: median=    2.10 ns/op  mean=    2.23 +/-  0.39
      foonathan::memory_pool: median=    2.40 ns/op  mean=    2.71 +/-  0.93
   EASTL::fixed_pool [!grow]: median=    1.40 ns/op  mean=    1.61 +/-  0.56
       std::pmr::unsync_pool: median=    4.60 ns/op  mean=    5.31 +/-  1.78
                  new/delete: median=   15.20 ns/op  mean=   16.97 +/-  4.08

--- N = 10000 objects ---
[2026-02-15 19:43:45] CPU: 2359 MHz (base: 3686)
[Cooling: size transition] [Ready: 2617 MHz]
           fat_p::ObjectPool: median=    2.69 ns/op  mean=    2.80 +/-  0.19
          boost::object_pool: median=    2.18 ns/op  mean=    2.27 +/-  0.19
      foonathan::memory_pool: median=    2.60 ns/op  mean=    3.36 +/-  1.99
   EASTL::fixed_pool [!grow]: median=    2.03 ns/op  mean=    2.19 +/-  0.52
       std::pmr::unsync_pool: median=    8.27 ns/op  mean=    8.36 +/-  0.59
                  new/delete: median=   18.37 ns/op  mean=   20.03 +/-  4.63

--- N = 100000 objects ---
[2026-02-15 19:44:02] CPU: 2543 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
           fat_p::ObjectPool: median=    5.87 ns/op  mean=    6.18 +/-  1.06
          boost::object_pool: median=    2.48 ns/op  mean=    2.68 +/-  0.48
      foonathan::memory_pool: median=    4.64 ns/op  mean=    4.63 +/-  0.25
   EASTL::fixed_pool [!grow]: median=    4.48 ns/op  mean=    4.94 +/-  1.86
       std::pmr::unsync_pool: median=    7.47 ns/op  mean=    7.51 +/-  0.44
                  new/delete: median=   23.90 ns/op  mean=   23.68 +/-  1.24
[Cooling: before pool reuse with compact] [Ready: 2617 MHz]

================================================================================
  Pool Reuse With Compaction (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all randomly, COMPACT, then acquire N again
          Tests whether compaction recovers allocation locality

--- N = 1000 objects ---
[2026-02-15 19:46:16] CPU: 2617 MHz (base: 3686)
[Cooling: size transition] [Ready: 2690 MHz]
           fat_p::ObjectPool: median=    2.10 ns/op  mean=    2.23 +/-  0.45
          boost::object_pool: median=    2.00 ns/op  mean=    2.04 +/-  0.07
      foonathan::memory_pool: median=    2.00 ns/op  mean=    2.03 +/-  0.22
   EASTL::fixed_pool [!grow]: median=    1.40 ns/op  mean=    1.45 +/-  0.18
       std::pmr::unsync_pool: median=    4.60 ns/op  mean=    4.57 +/-  0.49
                  new/delete: median=   15.00 ns/op  mean=   15.52 +/-  0.98

--- N = 10000 objects ---
[2026-02-15 19:46:23] CPU: 2764 MHz (base: 3686)
[Cooling: size transition] [Ready: 2617 MHz]
           fat_p::ObjectPool: median=    2.17 ns/op  mean=    2.40 +/-  0.69
          boost::object_pool: median=    2.19 ns/op  mean=    2.60 +/-  1.31
      foonathan::memory_pool: median=    2.65 ns/op  mean=    2.81 +/-  0.56
   EASTL::fixed_pool [!grow]: median=    2.04 ns/op  mean=    2.12 +/-  0.21
       std::pmr::unsync_pool: median=    8.24 ns/op  mean=   10.37 +/-  7.59
                  new/delete: median=   19.19 ns/op  mean=   20.72 +/-  5.47

--- N = 100000 objects ---
[2026-02-15 19:46:38] CPU: 2617 MHz (base: 3686)
[Cooling: size transition] [Ready: 2580 MHz]
           fat_p::ObjectPool: median=    2.21 ns/op  mean=    2.33 +/-  0.25
          boost::object_pool: median=    2.42 ns/op  mean=    2.55 +/-  0.36
      foonathan::memory_pool: median=    4.64 ns/op  mean=    4.72 +/-  0.41
   EASTL::fixed_pool [!grow]: median=    4.49 ns/op  mean=    4.73 +/-  1.01
       std::pmr::unsync_pool: median=    7.65 ns/op  mean=    7.74 +/-  0.91
                  new/delete: median=   23.12 ns/op  mean=   22.93 +/-  0.82
[Cooling: before pool reuse full cycle] [Ready: 2322 MHz]

================================================================================
  Pool Reuse Full Cycle (SmallTrivial 16B)
================================================================================
Contract: Acquire N, then TIME [release all randomly + compact + reacquire all]
          True cost of 'flush and refill' pattern (no hidden work)
          Note: Boost's ordered_free O(N) cost is now visible

--- N = 1000 objects ---
[2026-02-15 19:48:43] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2727 MHz]
           fat_p::ObjectPool: median=    4.70 ns/op  mean=    4.68 +/-  0.44
          boost::object_pool: median=  904.90 ns/op  mean=  908.49 +/- 15.25
      foonathan::memory_pool: median=    3.80 ns/op  mean=    3.78 +/-  0.31
   EASTL::fixed_pool [!grow]: median=    2.70 ns/op  mean=    2.73 +/-  0.32
       std::pmr::unsync_pool: median=    8.20 ns/op  mean=    8.47 +/-  1.06
                  new/delete: median=   25.50 ns/op  mean=   24.81 +/-  1.16

--- N = 10000 objects ---
[2026-02-15 19:48:56] CPU: 2727 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    5.81 ns/op  mean=    6.02 +/-  0.54
          boost::object_pool: median= 2347.28 ns/op  mean= 2341.33 +/- 57.51
      foonathan::memory_pool: median=    5.02 ns/op  mean=    5.22 +/-  0.63
   EASTL::fixed_pool [!grow]: median=    4.05 ns/op  mean=    4.14 +/-  0.50
       std::pmr::unsync_pool: median=   11.32 ns/op  mean=   11.90 +/-  2.65
                  new/delete: median=   31.46 ns/op  mean=   29.61 +/-  4.72

--- N = 100000 objects ---
[2026-02-15 19:49:12] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2469 MHz]
           fat_p::ObjectPool: median=    7.36 ns/op  mean=    7.55 +/-  0.81
          boost::object_pool: median=31479.42 ns/op  mean=31541.58 +/-760.00
      foonathan::memory_pool: median=    7.28 ns/op  mean=    8.04 +/-  2.33
   EASTL::fixed_pool [!grow]: median=    6.82 ns/op  mean=    6.96 +/-  0.60
       std::pmr::unsync_pool: median=   19.14 ns/op  mean=   20.24 +/-  2.57
                  new/delete: median=   46.37 ns/op  mean=   47.24 +/-  3.64
[Cooling: before medium object benchmarks] [Ready: 2727 MHz]

================================================================================
  Acquire + Release Cycle (MediumObject 64B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth

--- N = 1000 ops ---
[2026-02-15 19:51:19] CPU: 2727 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    2.60 ns/op  mean=    2.65 +/-  0.16
          boost::object_pool: median=    2.90 ns/op  mean=    3.00 +/-  0.16
      foonathan::memory_pool: median=    3.60 ns/op  mean=    3.62 +/-  0.12
   EASTL::fixed_pool [!grow]: median=    2.50 ns/op  mean=    2.55 +/-  0.11
       std::pmr::unsync_pool: median=    6.80 ns/op  mean=    7.43 +/-  1.93
                  new/delete: median=   20.10 ns/op  mean=   20.62 +/-  0.87
  Speedup vs new/delete: 7.7x

--- N = 10000 ops ---
[2026-02-15 19:51:22] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
           fat_p::ObjectPool: median=    2.44 ns/op  mean=    2.51 +/-  0.24
          boost::object_pool: median=    2.84 ns/op  mean=    2.97 +/-  0.61
      foonathan::memory_pool: median=    3.03 ns/op  mean=    3.16 +/-  0.29
   EASTL::fixed_pool [!grow]: median=    2.31 ns/op  mean=    2.30 +/-  0.10
       std::pmr::unsync_pool: median=    6.38 ns/op  mean=    6.43 +/-  0.47
                  new/delete: median=   19.95 ns/op  mean=   19.74 +/-  0.52
  Speedup vs new/delete: 8.2x

--- N = 50000 ops ---
[2026-02-15 19:51:38] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2469 MHz]
           fat_p::ObjectPool: median=    2.83 ns/op  mean=    3.37 +/-  1.73
          boost::object_pool: median=    2.89 ns/op  mean=    2.93 +/-  0.10
      foonathan::memory_pool: median=    3.30 ns/op  mean=    3.36 +/-  0.19
   EASTL::fixed_pool [!grow]: median=    2.37 ns/op  mean=    2.52 +/-  0.35
       std::pmr::unsync_pool: median=    6.80 ns/op  mean=    6.86 +/-  0.40
                  new/delete: median=   20.31 ns/op  mean=   21.16 +/-  2.12
  Speedup vs new/delete: 7.2x
[Cooling: before bulk acquire] [Ready: 2543 MHz]

================================================================================
  Bulk Acquire (MediumObject 64B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput

--- N = 1000 objects ---
[2026-02-15 19:52:10] CPU: 2543 MHz (base: 3686)
[Cooling: size transition] [Ready: 2543 MHz]
           fat_p::ObjectPool: median=    2.80 ns/op  mean=    2.92 +/-  0.36
          boost::object_pool: median=    4.30 ns/op  mean=    4.51 +/-  0.78
      foonathan::memory_pool: median=    2.40 ns/op  mean=    2.49 +/-  0.30
   EASTL::fixed_pool [!grow]: median=    2.90 ns/op  mean=    2.99 +/-  0.29
       std::pmr::unsync_pool: median=    6.10 ns/op  mean=    7.86 +/-  6.12
                  new/delete: median=   14.60 ns/op  mean=   16.92 +/-  4.56

--- N = 10000 objects ---
[2026-02-15 19:52:26] CPU: 2543 MHz (base: 3686)
[Cooling: size transition] [Ready: 2469 MHz]
           fat_p::ObjectPool: median=    2.98 ns/op  mean=    3.02 +/-  0.18
          boost::object_pool: median=   25.41 ns/op  mean=   19.39 +/- 13.17
      foonathan::memory_pool: median=    4.42 ns/op  mean=    4.29 +/-  1.11
   EASTL::fixed_pool [!grow]: median=    2.96 ns/op  mean=    3.05 +/-  0.31
       std::pmr::unsync_pool: median=   21.90 ns/op  mean=   23.55 +/-  8.57
                  new/delete: median=   26.39 ns/op  mean=   27.34 +/-  6.95

--- N = 50000 objects ---
[2026-02-15 19:52:31] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    3.57 ns/op  mean=    4.01 +/-  1.65
          boost::object_pool: median=   23.09 ns/op  mean=   23.10 +/-  3.94
      foonathan::memory_pool: median=    4.98 ns/op  mean=    5.62 +/-  1.29
   EASTL::fixed_pool [!grow]: median=    4.51 ns/op  mean=    4.61 +/-  0.94
       std::pmr::unsync_pool: median=   21.44 ns/op  mean=   21.60 +/-  2.13
                  new/delete: median=   33.37 ns/op  mean=   34.40 +/-  2.71
[Cooling: before large object benchmarks] [Ready: 2322 MHz]

================================================================================
  Acquire + Release Cycle (LargeObject 256B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth

--- N = 1000 ops ---
[2026-02-15 19:53:05] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2690 MHz]
           fat_p::ObjectPool: median=    6.90 ns/op  mean=    6.97 +/-  0.32
          boost::object_pool: median=    7.10 ns/op  mean=    7.19 +/-  0.35
      foonathan::memory_pool: median=    7.40 ns/op  mean=    7.39 +/-  0.17
   EASTL::fixed_pool [!grow]: median=    6.60 ns/op  mean=    6.64 +/-  0.28
       std::pmr::unsync_pool: median=   10.90 ns/op  mean=   11.32 +/-  1.21
                  new/delete: median=   29.50 ns/op  mean=   29.48 +/-  1.38
  Speedup vs new/delete: 4.3x

--- N = 10000 ops ---
[2026-02-15 19:53:21] CPU: 2690 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    5.74 ns/op  mean=    5.79 +/-  0.42
          boost::object_pool: median=    5.96 ns/op  mean=    6.32 +/-  1.41
      foonathan::memory_pool: median=    6.08 ns/op  mean=    6.34 +/-  1.31
   EASTL::fixed_pool [!grow]: median=    5.42 ns/op  mean=    5.68 +/-  1.24
       std::pmr::unsync_pool: median=    8.80 ns/op  mean=    9.55 +/-  2.14
                  new/delete: median=   24.44 ns/op  mean=   25.97 +/-  7.24
  Speedup vs new/delete: 4.3x
[Cooling: before specialized acquire] [Ready: 2395 MHz]

================================================================================
  Specialized Acquire (SmallTrivial - fat_p only)
================================================================================
Contract: Compare acquire() vs acquire_uninitialized() vs acquire_zeroed()
          Shows overhead of zero-initialization and default construction

[2026-02-15 19:53:40] CPU: 2395 MHz (base: 3686)

--- N = 1000 objects ---
[Cooling: size transition] [Ready: 2285 MHz]
              acquire(value): median=    1.50 ns/op
     acquire_uninitialized(): median=    1.40 ns/op  (1.1x faster)
            acquire_zeroed(): median=     1.2 ns/op

--- N = 10000 objects ---
[Cooling: size transition] [Ready: 2395 MHz]
              acquire(value): median=    1.82 ns/op
     acquire_uninitialized(): median=    1.29 ns/op  (1.4x faster)
            acquire_zeroed(): median=     1.2 ns/op

--- N = 100000 objects ---
[Cooling: size transition] [Ready: 2469 MHz]
              acquire(value): median=    1.88 ns/op
     acquire_uninitialized(): median=    1.32 ns/op  (1.4x faster)
            acquire_zeroed(): median=     1.3 ns/op
[Cooling: before multithreaded] [Ready: 2617 MHz]

================================================================================
  Multi-threaded Contention (Thread-Safe Pools)
================================================================================
Contract: Concurrent acquire/release from multiple threads
          Tests lock contention and scalability
          Comparing: fat_p::ThreadSafeObjectPool vs std::pmr::synchronized_pool_resource

[2026-02-15 19:54:07] CPU: 2617 MHz (base: 3686)

--- 1 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2653 MHz]
       fat_p::ThreadSafePool: median=  104.62 ns/op  throughput=   9558402 ops/sec
         std::pmr::sync_pool: median=   89.67 ns/op  throughput=  11152002 ops/sec

--- 2 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2690 MHz]
       fat_p::ThreadSafePool: median=   81.32 ns/op  throughput=  12297098 ops/sec
         std::pmr::sync_pool: median=   52.33 ns/op  throughput=  19109497 ops/sec

--- 4 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2875 MHz]
       fat_p::ThreadSafePool: median=   43.72 ns/op  throughput=  22874135 ops/sec
         std::pmr::sync_pool: median=   41.73 ns/op  throughput=  23960704 ops/sec

--- 8 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2469 MHz]
       fat_p::ThreadSafePool: median=   65.69 ns/op  throughput=  15222148 ops/sec
         std::pmr::sync_pool: median=   90.40 ns/op  throughput=  11061641 ops/sec

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-object overhead (in addition to object storage):
    fat_p::ObjectPool:    8 bytes (next pointer in free list)
    boost::object_pool:   8 bytes (chunk linkage)
    foonathan::memory:    8 bytes (node linkage)
    EASTL::fixed_pool:    8 bytes (free list pointer)
    std::pmr::pool:       8-16 bytes (block headers)
    new/delete:           8-24 bytes (malloc metadata)

  Feature comparison:
    Allocator               O(1)   Thread-Safe  Auto-Grow  RAII Wrapper  try_acquire
    ---------------------------------------------------------------------------------
    fat_p::ObjectPool       Yes    Optional     Yes        Yes           Yes
    boost::object_pool      Yes    No           Yes        No            No
    foonathan::memory       Yes    Optional     Yes        No            No
    EASTL::fixed_pool [!]   Yes    No           NO         No            Yes*
    std::pmr::unsync_pool   Yes    No           Yes        No            No
    std::pmr::sync_pool     Yes    Yes          Yes        No            No
    new/delete              No**   Yes          N/A        No            No

    [!] EASTL::fixed_pool is FIXED-CAPACITY (no auto-grow) - different contract
    *   EASTL returns nullptr when exhausted
    **  malloc may have O(1) fast path but can degrade

  fat_p::ObjectPool<MediumObject> with capacity 10000:
    sizeof(MediumObject): 64 bytes
    total_capacity:       10000 objects
    num_blocks:           1
    block_size:           10000 objects

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  Acquire + Release Cycle (SmallTrivial 16B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 03:37:50] CPU: 2870 MHz (~base: 2870)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.97 ns/op  mean=    5.96 +/-  0.03
          boost::object_pool: median=    6.68 ns/op  mean=    6.67 +/-  0.05
      foonathan::memory_pool: median=    9.71 ns/op  mean=   10.75 +/-  4.66
   EASTL::fixed_pool [!grow]: median=    4.58 ns/op  mean=    4.59 +/-  0.04
       std::pmr::unsync_pool: median=   24.82 ns/op  mean=   24.85 +/-  0.11
                  new/delete: median=   20.20 ns/op  mean=   20.09 +/-  0.17
  Speedup vs new/delete: 3.4x

--- N = 10000 ops ---
[2026-02-16 03:37:54] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.92 ns/op  mean=    5.97 +/-  0.21
          boost::object_pool: median=    5.24 ns/op  mean=    5.37 +/-  0.18
      foonathan::memory_pool: median=   10.55 ns/op  mean=   10.83 +/-  2.40
   EASTL::fixed_pool [!grow]: median=    5.58 ns/op  mean=    5.58 +/-  0.00
       std::pmr::unsync_pool: median=   24.75 ns/op  mean=   24.83 +/-  0.38
                  new/delete: median=   20.59 ns/op  mean=   20.96 +/-  0.59
  Speedup vs new/delete: 3.5x

--- N = 100000 ops ---
[2026-02-16 03:37:57] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.96 ns/op  mean=    5.99 +/-  0.07
          boost::object_pool: median=    5.52 ns/op  mean=    5.48 +/-  0.23
      foonathan::memory_pool: median=    9.97 ns/op  mean=   10.01 +/-  0.16
   EASTL::fixed_pool [!grow]: median=    5.70 ns/op  mean=    5.69 +/-  0.14
       std::pmr::unsync_pool: median=   25.06 ns/op  mean=   25.05 +/-  0.19
                  new/delete: median=   19.14 ns/op  mean=   19.18 +/-  0.24
  Speedup vs new/delete: 3.2x
[Cooling: before bulk acquire] [Ready]

================================================================================
  Bulk Acquire (SmallTrivial 16B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput
--- N = 1000 objects ---
[2026-02-16 03:38:05] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.52 ns/op  mean=    3.53 +/-  0.02
          boost::object_pool: median=    4.61 ns/op  mean=    5.58 +/-  1.38
      foonathan::memory_pool: median=    5.61 ns/op  mean=    5.58 +/-  0.11
   EASTL::fixed_pool [!grow]: median=    3.04 ns/op  mean=    3.11 +/-  0.16
       std::pmr::unsync_pool: median=   14.43 ns/op  mean=   14.87 +/-  1.08
                  new/delete: median=   17.92 ns/op  mean=   18.41 +/-  1.86

--- N = 10000 objects ---
[2026-02-16 03:38:09] CPU: 2865 MHz (~base: 2865)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.49 ns/op  mean=    3.42 +/-  0.11
          boost::object_pool: median=    4.27 ns/op  mean=    5.12 +/-  1.23
      foonathan::memory_pool: median=    5.62 ns/op  mean=    5.81 +/-  0.50
   EASTL::fixed_pool [!grow]: median=    3.52 ns/op  mean=    3.47 +/-  0.15
       std::pmr::unsync_pool: median=   15.33 ns/op  mean=   15.76 +/-  0.88
                  new/delete: median=   18.23 ns/op  mean=   18.24 +/-  0.89

--- N = 100000 objects ---
[2026-02-16 03:38:14] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.52 ns/op  mean=    3.56 +/-  0.17
          boost::object_pool: median=    4.10 ns/op  mean=    4.69 +/-  1.10
      foonathan::memory_pool: median=    5.49 ns/op  mean=    5.53 +/-  0.13
   EASTL::fixed_pool [!grow]: median=    3.54 ns/op  mean=    3.51 +/-  0.28
       std::pmr::unsync_pool: median=   14.87 ns/op  mean=   15.27 +/-  0.98
                  new/delete: median=   18.26 ns/op  mean=   17.76 +/-  0.89
[Cooling: before interleaved] [Ready]

================================================================================
  Interleaved Acquire/Release (SmallTrivial 16B)
================================================================================
Contract: Realistic workload with interleaved operations
          50% acquire, 50% release (steady-state simulation)
--- N = 1000 operations ---
[2026-02-16 03:41:04] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   12.86 ns/op  mean=   13.49 +/-  2.50
          boost::object_pool: median=   18.39 ns/op  mean=   18.66 +/-  1.60
      foonathan::memory_pool: median=   14.51 ns/op  mean=   14.57 +/-  0.25
   EASTL::fixed_pool [!grow]: median=   12.60 ns/op  mean=   12.66 +/-  0.24
       std::pmr::unsync_pool: median=   22.41 ns/op  mean=   22.96 +/-  2.01
                  new/delete: median=   22.21 ns/op  mean=   22.18 +/-  0.50

--- N = 10000 operations ---
[2026-02-16 03:41:08] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   12.70 ns/op  mean=   12.74 +/-  0.19
          boost::object_pool: median=   18.69 ns/op  mean=   18.78 +/-  0.62
      foonathan::memory_pool: median=   14.52 ns/op  mean=   14.61 +/-  0.30
   EASTL::fixed_pool [!grow]: median=   12.69 ns/op  mean=   12.88 +/-  0.37
       std::pmr::unsync_pool: median=   23.87 ns/op  mean=   24.33 +/-  1.26
                  new/delete: median=   21.75 ns/op  mean=   22.31 +/-  1.52

--- N = 100000 operations ---
[2026-02-16 03:41:11] CPU: 2869 MHz (~base: 2869)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   13.12 ns/op  mean=   13.14 +/-  0.11
          boost::object_pool: median=   19.19 ns/op  mean=   19.26 +/-  0.35
      foonathan::memory_pool: median=   14.64 ns/op  mean=   14.65 +/-  0.10
   EASTL::fixed_pool [!grow]: median=   12.72 ns/op  mean=   12.70 +/-  0.06
       std::pmr::unsync_pool: median=   26.82 ns/op  mean=   27.02 +/-  1.93
                  new/delete: median=   21.63 ns/op  mean=   21.66 +/-  0.17
[Cooling: before pool reuse] [Ready]

================================================================================
  Pool Reuse / Free List Efficiency (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all in random order, acquire N again
          Tests free list traversal and memory reuse
--- N = 1000 objects ---
[2026-02-16 03:41:19] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.50 ns/op  mean=    3.51 +/-  0.20
          boost::object_pool: median=    3.52 ns/op  mean=    3.42 +/-  0.31
      foonathan::memory_pool: median=    5.63 ns/op  mean=    5.69 +/-  0.26
   EASTL::fixed_pool [!grow]: median=    3.15 ns/op  mean=    3.07 +/-  0.34
       std::pmr::unsync_pool: median=   13.86 ns/op  mean=   13.87 +/-  0.17
                  new/delete: median=   10.99 ns/op  mean=   11.12 +/-  1.79

--- N = 10000 objects ---
[2026-02-16 03:41:23] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    4.91 ns/op  mean=    4.97 +/-  0.29
          boost::object_pool: median=    3.07 ns/op  mean=    3.10 +/-  0.11
      foonathan::memory_pool: median=    5.59 ns/op  mean=    5.66 +/-  0.51
   EASTL::fixed_pool [!grow]: median=    4.65 ns/op  mean=    4.65 +/-  0.01
       std::pmr::unsync_pool: median=   14.67 ns/op  mean=   14.76 +/-  0.34
                  new/delete: median=   12.41 ns/op  mean=   12.68 +/-  0.59

--- N = 100000 objects ---
[2026-02-16 03:41:28] CPU: 2870 MHz (~base: 2870)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   14.23 ns/op  mean=   14.31 +/-  0.41
          boost::object_pool: median=    3.04 ns/op  mean=    3.06 +/-  0.05
      foonathan::memory_pool: median=   11.88 ns/op  mean=   12.69 +/-  1.24
   EASTL::fixed_pool [!grow]: median=   12.33 ns/op  mean=   13.11 +/-  1.22
       std::pmr::unsync_pool: median=   16.13 ns/op  mean=   16.17 +/-  0.21
                  new/delete: median=   25.41 ns/op  mean=   25.54 +/-  1.15
[Cooling: before pool reuse with compact] [Ready]

================================================================================
  Pool Reuse With Compaction (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all randomly, COMPACT, then acquire N again
          Tests whether compaction recovers allocation locality
--- N = 1000 objects ---
[2026-02-16 03:45:17] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.92 ns/op  mean=    3.91 +/-  0.42
          boost::object_pool: median=    3.18 ns/op  mean=    3.33 +/-  0.32
      foonathan::memory_pool: median=    5.97 ns/op  mean=    6.70 +/-  2.94
   EASTL::fixed_pool [!grow]: median=    3.56 ns/op  mean=    3.50 +/-  0.22
       std::pmr::unsync_pool: median=   14.44 ns/op  mean=   14.47 +/-  0.22
                  new/delete: median=   11.49 ns/op  mean=   11.78 +/-  1.59

--- N = 10000 objects ---
[2026-02-16 03:45:20] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.50 ns/op  mean=    3.47 +/-  0.21
          boost::object_pool: median=    3.19 ns/op  mean=    3.28 +/-  0.25
      foonathan::memory_pool: median=    5.59 ns/op  mean=    5.64 +/-  0.24
   EASTL::fixed_pool [!grow]: median=    4.64 ns/op  mean=    4.81 +/-  0.44
       std::pmr::unsync_pool: median=   15.18 ns/op  mean=   15.31 +/-  0.48
                  new/delete: median=   12.39 ns/op  mean=   12.54 +/-  0.48

--- N = 100000 objects ---
[2026-02-16 03:45:26] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.45 ns/op  mean=    3.46 +/-  0.12
          boost::object_pool: median=    3.20 ns/op  mean=    3.22 +/-  0.08
      foonathan::memory_pool: median=   14.29 ns/op  mean=   13.39 +/-  1.38
   EASTL::fixed_pool [!grow]: median=   13.45 ns/op  mean=   13.07 +/-  1.35
       std::pmr::unsync_pool: median=   16.28 ns/op  mean=   16.47 +/-  0.71
                  new/delete: median=   27.86 ns/op  mean=   27.31 +/-  1.95
[Cooling: before pool reuse full cycle] [Ready]

================================================================================
  Pool Reuse Full Cycle (SmallTrivial 16B)
================================================================================
Contract: Acquire N, then TIME [release all randomly + compact + reacquire all]
          True cost of 'flush and refill' pattern (no hidden work)
          Note: Boost's ordered_free O(N) cost is now visible
--- N = 1000 objects ---
[2026-02-16 03:49:16] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    7.48 ns/op  mean=    7.47 +/-  0.30
          boost::object_pool: median=  359.10 ns/op  mean=  380.07 +/- 60.33
      foonathan::memory_pool: median=    9.16 ns/op  mean=    9.49 +/-  0.60
   EASTL::fixed_pool [!grow]: median=    6.01 ns/op  mean=    6.07 +/-  0.26
       std::pmr::unsync_pool: median=   30.08 ns/op  mean=   32.18 +/-  4.60
                  new/delete: median=   22.76 ns/op  mean=   23.14 +/-  1.98

--- N = 10000 objects ---
[2026-02-16 03:49:19] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    7.06 ns/op  mean=    7.01 +/-  0.56
          boost::object_pool: median= 3901.12 ns/op  mean= 3899.74 +/- 14.06
      foonathan::memory_pool: median=   10.17 ns/op  mean=    9.97 +/-  0.67
   EASTL::fixed_pool [!grow]: median=    7.81 ns/op  mean=    7.77 +/-  0.55
       std::pmr::unsync_pool: median=   39.18 ns/op  mean=   39.24 +/-  0.98
                  new/delete: median=   22.53 ns/op  mean=   22.23 +/-  0.69

--- N = 100000 objects ---
[2026-02-16 03:49:25] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    8.02 ns/op  mean=    8.16 +/-  0.41
          boost::object_pool: median=39137.69 ns/op  mean=39228.47 +/-284.22
      foonathan::memory_pool: median=   17.09 ns/op  mean=   17.39 +/-  0.72
   EASTL::fixed_pool [!grow]: median=   15.83 ns/op  mean=   16.02 +/-  0.46
       std::pmr::unsync_pool: median=   41.28 ns/op  mean=   41.10 +/-  0.85
                  new/delete: median=   45.27 ns/op  mean=   45.67 +/-  2.52
[Cooling: before medium object benchmarks] [Ready]

================================================================================
  Acquire + Release Cycle (MediumObject 64B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 03:53:13] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    6.05 ns/op  mean=    6.62 +/-  2.54
          boost::object_pool: median=    6.71 ns/op  mean=    6.71 +/-  0.05
      foonathan::memory_pool: median=   10.50 ns/op  mean=   10.55 +/-  0.14
   EASTL::fixed_pool [!grow]: median=    5.56 ns/op  mean=    5.58 +/-  0.05
       std::pmr::unsync_pool: median=   27.37 ns/op  mean=   27.38 +/-  0.07
                  new/delete: median=   19.90 ns/op  mean=   19.92 +/-  0.07
  Speedup vs new/delete: 3.3x

--- N = 10000 ops ---
[2026-02-16 03:53:17] CPU: 2871 MHz (~base: 2871)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.93 ns/op  mean=    6.00 +/-  0.24
          boost::object_pool: median=    5.82 ns/op  mean=    5.93 +/-  0.32
      foonathan::memory_pool: median=   10.48 ns/op  mean=   10.80 +/-  0.53
   EASTL::fixed_pool [!grow]: median=    5.93 ns/op  mean=    5.94 +/-  0.01
       std::pmr::unsync_pool: median=   27.37 ns/op  mean=   27.70 +/-  0.56
                  new/delete: median=   19.86 ns/op  mean=   20.03 +/-  0.54
  Speedup vs new/delete: 3.3x

--- N = 50000 ops ---
[2026-02-16 03:53:20] CPU: 2870 MHz (~base: 2870)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.93 ns/op  mean=    5.99 +/-  0.09
          boost::object_pool: median=    5.93 ns/op  mean=    5.93 +/-  0.11
      foonathan::memory_pool: median=   11.60 ns/op  mean=   11.61 +/-  0.10
   EASTL::fixed_pool [!grow]: median=    6.00 ns/op  mean=    6.03 +/-  0.10
       std::pmr::unsync_pool: median=   27.58 ns/op  mean=   27.59 +/-  0.11
                  new/delete: median=   19.00 ns/op  mean=   19.00 +/-  0.09
  Speedup vs new/delete: 3.2x
[Cooling: before bulk acquire] [Ready]

================================================================================
  Bulk Acquire (MediumObject 64B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput
--- N = 1000 objects ---
[2026-02-16 03:53:28] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    6.07 ns/op  mean=    5.85 +/-  0.58
          boost::object_pool: median=    6.70 ns/op  mean=   10.52 +/-  5.66
      foonathan::memory_pool: median=    8.66 ns/op  mean=    9.03 +/-  1.89
   EASTL::fixed_pool [!grow]: median=    4.55 ns/op  mean=    4.54 +/-  0.05
       std::pmr::unsync_pool: median=   16.87 ns/op  mean=   18.60 +/-  4.22
                  new/delete: median=   18.33 ns/op  mean=   18.23 +/-  0.40

--- N = 10000 objects ---
[2026-02-16 03:53:31] CPU: 2871 MHz (~base: 2871)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.79 ns/op  mean=    5.84 +/-  0.57
          boost::object_pool: median=    6.62 ns/op  mean=   10.52 +/-  5.66
      foonathan::memory_pool: median=    8.02 ns/op  mean=    8.20 +/-  0.37
   EASTL::fixed_pool [!grow]: median=    4.38 ns/op  mean=    4.41 +/-  0.09
       std::pmr::unsync_pool: median=   16.45 ns/op  mean=   18.46 +/-  4.24
                  new/delete: median=   18.36 ns/op  mean=   18.49 +/-  0.56

--- N = 50000 objects ---
[2026-02-16 03:53:38] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.98 ns/op  mean=    6.00 +/-  0.16
          boost::object_pool: median=    6.43 ns/op  mean=    9.44 +/-  4.86
      foonathan::memory_pool: median=    8.50 ns/op  mean=    8.30 +/-  0.43
   EASTL::fixed_pool [!grow]: median=    4.34 ns/op  mean=    4.37 +/-  0.09
       std::pmr::unsync_pool: median=   16.45 ns/op  mean=   18.36 +/-  4.01
                  new/delete: median=   18.09 ns/op  mean=   17.89 +/-  0.82
[Cooling: before large object benchmarks] [Ready]

================================================================================
  Acquire + Release Cycle (LargeObject 256B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 03:54:27] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   11.74 ns/op  mean=   12.42 +/-  3.04
          boost::object_pool: median=   12.33 ns/op  mean=   12.35 +/-  0.05
      foonathan::memory_pool: median=   14.00 ns/op  mean=   14.07 +/-  0.13
   EASTL::fixed_pool [!grow]: median=   11.89 ns/op  mean=   11.90 +/-  0.03
       std::pmr::unsync_pool: median=   35.24 ns/op  mean=   35.28 +/-  0.19
                  new/delete: median=   21.64 ns/op  mean=   21.66 +/-  0.05
  Speedup vs new/delete: 1.8x

--- N = 10000 ops ---
[2026-02-16 03:54:31] CPU: 2596 MHz (~base: 2596)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   11.69 ns/op  mean=   11.97 +/-  0.49
          boost::object_pool: median=   12.26 ns/op  mean=   12.31 +/-  0.23
      foonathan::memory_pool: median=   13.95 ns/op  mean=   14.10 +/-  0.32
   EASTL::fixed_pool [!grow]: median=   11.88 ns/op  mean=   11.93 +/-  0.23
       std::pmr::unsync_pool: median=   35.60 ns/op  mean=   35.55 +/-  0.61
                  new/delete: median=   21.61 ns/op  mean=   21.71 +/-  0.31
  Speedup vs new/delete: 1.8x
[Cooling: before specialized acquire] [Ready]

================================================================================
  Specialized Acquire (SmallTrivial - fat_p only)
================================================================================
Contract: Compare acquire() vs acquire_uninitialized() vs acquire_zeroed()
          Shows overhead of zero-initialization and default construction

[2026-02-16 03:54:38] CPU: 2875 MHz (~base: 2875)

--- N = 1000 objects ---
[Cooling: size transition] [Ready]
              acquire(value): median=    1.57 ns/op
     acquire_uninitialized(): median=    1.58 ns/op  (1.0x faster)
            acquire_zeroed(): median=     2.1 ns/op

--- N = 10000 objects ---
[Cooling: size transition] [Ready]
              acquire(value): median=    1.67 ns/op
     acquire_uninitialized(): median=    1.67 ns/op  (1.0x faster)
            acquire_zeroed(): median=     2.2 ns/op

--- N = 100000 objects ---
[Cooling: size transition] [Ready]
              acquire(value): median=    1.70 ns/op
     acquire_uninitialized(): median=    1.70 ns/op  (1.0x faster)
            acquire_zeroed(): median=     2.2 ns/op
[Cooling: before multithreaded] [Ready]

================================================================================
  Multi-threaded Contention (Thread-Safe Pools)
================================================================================
Contract: Concurrent acquire/release from multiple threads
          Tests lock contention and scalability
          Comparing: fat_p::ThreadSafeObjectPool vs std::pmr::synchronized_pool_resource

[2026-02-16 03:54:53] CPU: 2880 MHz (~base: 2880)

--- 1 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready]
       fat_p::ThreadSafePool: median=   18.64 ns/op  throughput=  53635696 ops/sec
         std::pmr::sync_pool: median=   42.56 ns/op  throughput=  23494667 ops/sec

--- 2 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready]
       fat_p::ThreadSafePool: median=  102.37 ns/op  throughput=   9768735 ops/sec
         std::pmr::sync_pool: median=  159.34 ns/op  throughput=   6275941 ops/sec

--- 4 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready]
       fat_p::ThreadSafePool: median=  105.74 ns/op  throughput=   9456795 ops/sec
         std::pmr::sync_pool: median=  185.60 ns/op  throughput=   5387953 ops/sec

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-object overhead (in addition to object storage):
    fat_p::ObjectPool:    8 bytes (next pointer in free list)
    boost::object_pool:   8 bytes (chunk linkage)
    foonathan::memory:    8 bytes (node linkage)
    EASTL::fixed_pool:    8 bytes (free list pointer)
    std::pmr::pool:       8-16 bytes (block headers)
    new/delete:           8-24 bytes (malloc metadata)

  Feature comparison:
    Allocator               O(1)   Thread-Safe  Auto-Grow  RAII Wrapper  try_acquire
    ---------------------------------------------------------------------------------
    fat_p::ObjectPool       Yes    Optional     Yes        Yes           Yes
    boost::object_pool      Yes    No           Yes        No            No
    foonathan::memory       Yes    Optional     Yes        No            No
    EASTL::fixed_pool [!]   Yes    No           NO         No            Yes*
    std::pmr::unsync_pool   Yes    No           Yes        No            No
    std::pmr::sync_pool     Yes    Yes          Yes        No            No
    new/delete              No**   Yes          N/A        No            No

    [!] EASTL::fixed_pool is FIXED-CAPACITY (no auto-grow) - different contract
    *   EASTL returns nullptr when exhausted
    **  malloc may have O(1) fast path but can degrade

  fat_p::ObjectPool<MediumObject> with capacity 10000:
    sizeof(MediumObject): 64 bytes
    total_capacity:       10000 objects
    num_blocks:           1
    block_size:           10000 objects

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  Acquire + Release Cycle (SmallTrivial 16B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 04:11:24] CPU: 3240 MHz (~base: 3240)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    6.54 ns/op  mean=    6.57 +/-  0.11
          boost::object_pool: median=    6.76 ns/op  mean=    6.81 +/-  0.14
      foonathan::memory_pool: median=    8.88 ns/op  mean=    9.11 +/-  1.01
   EASTL::fixed_pool [!grow]: median=    5.77 ns/op  mean=    5.77 +/-  0.06
       std::pmr::unsync_pool: median=   27.91 ns/op  mean=   29.24 +/-  3.84
                  new/delete: median=   20.57 ns/op  mean=   21.84 +/-  4.72
  Speedup vs new/delete: 3.1x

--- N = 10000 ops ---
[2026-02-16 04:11:28] CPU: 3242 MHz (~base: 3242)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.62 ns/op  mean=    5.62 +/-  0.02
          boost::object_pool: median=    6.17 ns/op  mean=    6.15 +/-  0.14
      foonathan::memory_pool: median=    6.72 ns/op  mean=    6.76 +/-  0.06
   EASTL::fixed_pool [!grow]: median=    4.94 ns/op  mean=    5.14 +/-  0.42
       std::pmr::unsync_pool: median=   23.45 ns/op  mean=   23.67 +/-  0.41
                  new/delete: median=   16.96 ns/op  mean=   17.45 +/-  1.08
  Speedup vs new/delete: 3.0x

--- N = 100000 ops ---
[2026-02-16 04:11:31] CPU: 3241 MHz (~base: 3241)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.68 ns/op  mean=    5.68 +/-  0.06
          boost::object_pool: median=    4.80 ns/op  mean=    5.25 +/-  0.74
      foonathan::memory_pool: median=    8.12 ns/op  mean=    8.11 +/-  0.05
   EASTL::fixed_pool [!grow]: median=    5.26 ns/op  mean=    5.27 +/-  0.12
       std::pmr::unsync_pool: median=   23.67 ns/op  mean=   23.73 +/-  0.23
                  new/delete: median=   17.09 ns/op  mean=   16.44 +/-  0.95
  Speedup vs new/delete: 3.0x
[Cooling: before bulk acquire] [Ready]

================================================================================
  Bulk Acquire (SmallTrivial 16B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput
--- N = 1000 objects ---
[2026-02-16 04:11:39] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.69 ns/op  mean=    4.33 +/-  2.12
          boost::object_pool: median=    5.00 ns/op  mean=    5.81 +/-  1.19
      foonathan::memory_pool: median=    5.79 ns/op  mean=    5.83 +/-  0.33
   EASTL::fixed_pool [!grow]: median=    3.99 ns/op  mean=    4.01 +/-  0.14
       std::pmr::unsync_pool: median=   13.73 ns/op  mean=   14.20 +/-  0.91
                  new/delete: median=   18.61 ns/op  mean=   19.28 +/-  2.81

--- N = 10000 objects ---
[2026-02-16 04:11:42] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.94 ns/op  mean=    3.96 +/-  0.15
          boost::object_pool: median=    4.96 ns/op  mean=    5.38 +/-  0.88
      foonathan::memory_pool: median=    6.03 ns/op  mean=    6.03 +/-  0.13
   EASTL::fixed_pool [!grow]: median=    4.07 ns/op  mean=    4.12 +/-  0.39
       std::pmr::unsync_pool: median=   14.51 ns/op  mean=   14.73 +/-  0.81
                  new/delete: median=   19.03 ns/op  mean=   19.37 +/-  1.30

--- N = 100000 objects ---
[2026-02-16 04:11:47] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.93 ns/op  mean=    3.95 +/-  0.10
          boost::object_pool: median=    4.22 ns/op  mean=    4.77 +/-  0.95
      foonathan::memory_pool: median=    6.14 ns/op  mean=    6.14 +/-  0.12
   EASTL::fixed_pool [!grow]: median=    3.83 ns/op  mean=    3.78 +/-  0.18
       std::pmr::unsync_pool: median=   13.82 ns/op  mean=   14.21 +/-  0.84
                  new/delete: median=   17.51 ns/op  mean=   17.24 +/-  0.44
[Cooling: before interleaved] [Ready]

================================================================================
  Interleaved Acquire/Release (SmallTrivial 16B)
================================================================================
Contract: Realistic workload with interleaved operations
          50% acquire, 50% release (steady-state simulation)
--- N = 1000 operations ---
[2026-02-16 04:14:19] CPU: 2846 MHz (~base: 2846)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   12.14 ns/op  mean=   12.89 +/-  1.86
          boost::object_pool: median=   17.62 ns/op  mean=   17.54 +/-  2.04
      foonathan::memory_pool: median=   12.57 ns/op  mean=   13.43 +/-  2.07
   EASTL::fixed_pool [!grow]: median=   11.30 ns/op  mean=   12.13 +/-  1.84
       std::pmr::unsync_pool: median=   21.14 ns/op  mean=   23.38 +/-  3.69
                  new/delete: median=   19.91 ns/op  mean=   21.04 +/-  3.06

--- N = 10000 operations ---
[2026-02-16 04:14:23] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   11.55 ns/op  mean=   11.62 +/-  0.19
          boost::object_pool: median=   16.98 ns/op  mean=   17.13 +/-  0.69
      foonathan::memory_pool: median=   12.10 ns/op  mean=   12.15 +/-  0.21
   EASTL::fixed_pool [!grow]: median=   11.06 ns/op  mean=   11.18 +/-  0.32
       std::pmr::unsync_pool: median=   22.15 ns/op  mean=   22.53 +/-  1.37
                  new/delete: median=   18.58 ns/op  mean=   18.67 +/-  0.36

--- N = 100000 operations ---
[2026-02-16 04:14:26] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   11.57 ns/op  mean=   11.59 +/-  0.07
          boost::object_pool: median=   17.44 ns/op  mean=   17.49 +/-  0.26
      foonathan::memory_pool: median=   12.62 ns/op  mean=   12.64 +/-  0.15
   EASTL::fixed_pool [!grow]: median=   11.10 ns/op  mean=   11.09 +/-  0.15
       std::pmr::unsync_pool: median=   24.52 ns/op  mean=   24.63 +/-  1.82
                  new/delete: median=   17.72 ns/op  mean=   17.73 +/-  0.08
[Cooling: before pool reuse] [Ready]

================================================================================
  Pool Reuse / Free List Efficiency (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all in random order, acquire N again
          Tests free list traversal and memory reuse
--- N = 1000 objects ---
[2026-02-16 04:14:34] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.27 ns/op  mean=    3.29 +/-  0.13
          boost::object_pool: median=    3.26 ns/op  mean=    3.37 +/-  0.51
      foonathan::memory_pool: median=    4.50 ns/op  mean=    4.66 +/-  0.74
   EASTL::fixed_pool [!grow]: median=    2.68 ns/op  mean=    2.79 +/-  0.49
       std::pmr::unsync_pool: median=   12.54 ns/op  mean=   13.13 +/-  2.14
                  new/delete: median=   10.02 ns/op  mean=   10.56 +/-  1.93

--- N = 10000 objects ---
[2026-02-16 04:14:37] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    4.06 ns/op  mean=    4.06 +/-  0.04
          boost::object_pool: median=    3.40 ns/op  mean=    3.44 +/-  0.17
      foonathan::memory_pool: median=    5.28 ns/op  mean=    5.36 +/-  0.36
   EASTL::fixed_pool [!grow]: median=    4.47 ns/op  mean=    4.69 +/-  0.73
       std::pmr::unsync_pool: median=   13.72 ns/op  mean=   13.86 +/-  0.54
                  new/delete: median=   12.98 ns/op  mean=   13.15 +/-  0.54

--- N = 100000 objects ---
[2026-02-16 04:14:43] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   13.14 ns/op  mean=   13.10 +/-  0.19
          boost::object_pool: median=    3.11 ns/op  mean=    3.13 +/-  0.12
      foonathan::memory_pool: median=   13.13 ns/op  mean=   13.13 +/-  0.90
   EASTL::fixed_pool [!grow]: median=   13.00 ns/op  mean=   12.92 +/-  0.81
       std::pmr::unsync_pool: median=   14.96 ns/op  mean=   15.09 +/-  0.54
                  new/delete: median=   20.12 ns/op  mean=   20.10 +/-  0.22
[Cooling: before pool reuse with compact] [Ready]

================================================================================
  Pool Reuse With Compaction (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all randomly, COMPACT, then acquire N again
          Tests whether compaction recovers allocation locality
--- N = 1000 objects ---
[2026-02-16 04:18:06] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.71 ns/op  mean=    4.03 +/-  1.71
          boost::object_pool: median=    3.67 ns/op  mean=    3.67 +/-  0.28
      foonathan::memory_pool: median=    4.84 ns/op  mean=    4.88 +/-  0.40
   EASTL::fixed_pool [!grow]: median=    2.80 ns/op  mean=    2.84 +/-  0.25
       std::pmr::unsync_pool: median=   14.00 ns/op  mean=   14.08 +/-  0.90
                  new/delete: median=   11.53 ns/op  mean=   11.59 +/-  0.83

--- N = 10000 objects ---
[2026-02-16 04:18:10] CPU: 2621 MHz (~base: 2621)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.46 ns/op  mean=    3.49 +/-  0.24
          boost::object_pool: median=    3.48 ns/op  mean=    3.56 +/-  0.19
      foonathan::memory_pool: median=    5.14 ns/op  mean=    5.50 +/-  1.28
   EASTL::fixed_pool [!grow]: median=    4.27 ns/op  mean=    4.26 +/-  0.21
       std::pmr::unsync_pool: median=   13.84 ns/op  mean=   14.32 +/-  0.93
                  new/delete: median=   13.08 ns/op  mean=   13.66 +/-  1.44

--- N = 100000 objects ---
[2026-02-16 04:18:15] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    3.28 ns/op  mean=    3.29 +/-  0.04
          boost::object_pool: median=    3.11 ns/op  mean=    3.15 +/-  0.14
      foonathan::memory_pool: median=   14.09 ns/op  mean=   13.52 +/-  1.27
   EASTL::fixed_pool [!grow]: median=   13.99 ns/op  mean=   13.40 +/-  1.02
       std::pmr::unsync_pool: median=   15.06 ns/op  mean=   15.36 +/-  0.82
                  new/delete: median=   21.78 ns/op  mean=   21.53 +/-  0.99
[Cooling: before pool reuse full cycle] [Ready]

================================================================================
  Pool Reuse Full Cycle (SmallTrivial 16B)
================================================================================
Contract: Acquire N, then TIME [release all randomly + compact + reacquire all]
          True cost of 'flush and refill' pattern (no hidden work)
          Note: Boost's ordered_free O(N) cost is now visible
--- N = 1000 objects ---
[2026-02-16 04:21:40] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    7.14 ns/op  mean=    7.44 +/-  1.71
          boost::object_pool: median=  319.21 ns/op  mean=  339.29 +/- 53.99
      foonathan::memory_pool: median=    8.07 ns/op  mean=    8.17 +/-  0.62
   EASTL::fixed_pool [!grow]: median=    5.52 ns/op  mean=    5.60 +/-  0.37
       std::pmr::unsync_pool: median=   30.90 ns/op  mean=   32.08 +/-  4.03
                  new/delete: median=   21.22 ns/op  mean=   21.31 +/-  2.60

--- N = 10000 objects ---
[2026-02-16 04:21:43] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    7.48 ns/op  mean=    7.55 +/-  0.66
          boost::object_pool: median= 3396.79 ns/op  mean= 3397.10 +/- 15.16
      foonathan::memory_pool: median=    8.76 ns/op  mean=    8.92 +/-  0.70
   EASTL::fixed_pool [!grow]: median=    6.56 ns/op  mean=    6.60 +/-  0.33
       std::pmr::unsync_pool: median=   35.03 ns/op  mean=   35.84 +/-  1.66
                  new/delete: median=   21.02 ns/op  mean=   21.94 +/-  1.66

--- N = 100000 objects ---
[2026-02-16 04:21:49] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    8.43 ns/op  mean=    8.50 +/-  0.35
          boost::object_pool: median=34860.61 ns/op  mean=34890.44 +/-120.83
      foonathan::memory_pool: median=   17.50 ns/op  mean=   17.56 +/-  0.21
   EASTL::fixed_pool [!grow]: median=   16.67 ns/op  mean=   16.74 +/-  0.56
       std::pmr::unsync_pool: median=   37.51 ns/op  mean=   37.34 +/-  0.61
                  new/delete: median=   40.46 ns/op  mean=   40.59 +/-  0.54
[Cooling: before medium object benchmarks] [Ready]

================================================================================
  Acquire + Release Cycle (MediumObject 64B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 04:25:12] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    6.37 ns/op  mean=    6.16 +/-  0.38
          boost::object_pool: median=    6.81 ns/op  mean=    6.74 +/-  0.13
      foonathan::memory_pool: median=    8.63 ns/op  mean=    8.31 +/-  0.88
   EASTL::fixed_pool [!grow]: median=    6.16 ns/op  mean=    6.60 +/-  2.32
       std::pmr::unsync_pool: median=   24.23 ns/op  mean=   24.02 +/-  0.43
                  new/delete: median=   18.03 ns/op  mean=   17.97 +/-  0.61
  Speedup vs new/delete: 2.8x

--- N = 10000 ops ---
[2026-02-16 04:25:16] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.57 ns/op  mean=    5.72 +/-  0.37
          boost::object_pool: median=    6.50 ns/op  mean=    6.43 +/-  0.29
      foonathan::memory_pool: median=    7.17 ns/op  mean=    7.28 +/-  0.30
   EASTL::fixed_pool [!grow]: median=    5.56 ns/op  mean=    5.66 +/-  0.43
       std::pmr::unsync_pool: median=   23.50 ns/op  mean=   23.89 +/-  1.02
                  new/delete: median=   17.28 ns/op  mean=   17.53 +/-  0.52
  Speedup vs new/delete: 3.1x

--- N = 50000 ops ---
[2026-02-16 04:25:19] CPU: 3243 MHz (~base: 3243)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.57 ns/op  mean=    5.65 +/-  0.13
          boost::object_pool: median=    6.49 ns/op  mean=    6.46 +/-  0.37
      foonathan::memory_pool: median=    9.14 ns/op  mean=    9.09 +/-  0.08
   EASTL::fixed_pool [!grow]: median=    5.56 ns/op  mean=    5.61 +/-  0.08
       std::pmr::unsync_pool: median=   23.63 ns/op  mean=   23.65 +/-  0.07
                  new/delete: median=   15.62 ns/op  mean=   15.63 +/-  0.10
  Speedup vs new/delete: 2.8x
[Cooling: before bulk acquire] [Ready]

================================================================================
  Bulk Acquire (MediumObject 64B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput
--- N = 1000 objects ---
[2026-02-16 04:25:27] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.42 ns/op  mean=    5.44 +/-  0.17
          boost::object_pool: median=    7.12 ns/op  mean=   10.31 +/-  4.77
      foonathan::memory_pool: median=    7.78 ns/op  mean=    7.78 +/-  0.09
   EASTL::fixed_pool [!grow]: median=    4.62 ns/op  mean=    4.66 +/-  0.27
       std::pmr::unsync_pool: median=   15.55 ns/op  mean=   17.30 +/-  3.68
                  new/delete: median=   18.10 ns/op  mean=   18.82 +/-  1.85

--- N = 10000 objects ---
[2026-02-16 04:25:30] CPU: 3243 MHz (~base: 3243)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.75 ns/op  mean=    5.75 +/-  0.04
          boost::object_pool: median=    7.87 ns/op  mean=   11.20 +/-  4.84
      foonathan::memory_pool: median=    7.91 ns/op  mean=    7.94 +/-  0.10
   EASTL::fixed_pool [!grow]: median=    4.42 ns/op  mean=    4.51 +/-  0.30
       std::pmr::unsync_pool: median=   15.78 ns/op  mean=   17.28 +/-  3.46
                  new/delete: median=   16.90 ns/op  mean=   16.68 +/-  0.50

--- N = 50000 objects ---
[2026-02-16 04:25:36] CPU: 3244 MHz (~base: 3244)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=    5.82 ns/op  mean=    5.88 +/-  0.10
          boost::object_pool: median=    7.33 ns/op  mean=    9.66 +/-  3.77
      foonathan::memory_pool: median=    7.90 ns/op  mean=    7.96 +/-  0.15
   EASTL::fixed_pool [!grow]: median=    4.37 ns/op  mean=    4.38 +/-  0.09
       std::pmr::unsync_pool: median=   15.73 ns/op  mean=   17.34 +/-  3.24
                  new/delete: median=   17.31 ns/op  mean=   16.99 +/-  0.54
[Cooling: before large object benchmarks] [Ready]

================================================================================
  Acquire + Release Cycle (LargeObject 256B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 04:26:19] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   10.89 ns/op  mean=   10.85 +/-  0.80
          boost::object_pool: median=   10.62 ns/op  mean=   10.77 +/-  0.64
      foonathan::memory_pool: median=   11.30 ns/op  mean=   11.27 +/-  0.85
   EASTL::fixed_pool [!grow]: median=    9.95 ns/op  mean=   11.08 +/-  3.07
       std::pmr::unsync_pool: median=   29.51 ns/op  mean=   29.93 +/-  1.68
                  new/delete: median=   20.99 ns/op  mean=   20.96 +/-  2.15
  Speedup vs new/delete: 1.9x

--- N = 10000 ops ---
[2026-02-16 04:26:23] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
           fat_p::ObjectPool: median=   10.21 ns/op  mean=   10.27 +/-  0.21
          boost::object_pool: median=   11.07 ns/op  mean=   11.23 +/-  0.36
      foonathan::memory_pool: median=   10.93 ns/op  mean=   11.21 +/-  0.44
   EASTL::fixed_pool [!grow]: median=    9.92 ns/op  mean=    9.93 +/-  0.03
       std::pmr::unsync_pool: median=   29.53 ns/op  mean=   29.60 +/-  0.36
                  new/delete: median=   19.50 ns/op  mean=   19.67 +/-  0.36
  Speedup vs new/delete: 1.9x
[Cooling: before specialized acquire] [Ready]

================================================================================
  Specialized Acquire (SmallTrivial - fat_p only)
================================================================================
Contract: Compare acquire() vs acquire_uninitialized() vs acquire_zeroed()
          Shows overhead of zero-initialization and default construction

[2026-02-16 04:26:30] CPU: 2445 MHz (~base: 2445)

--- N = 1000 objects ---
[Cooling: size transition] [Ready]
              acquire(value): median=    1.77 ns/op
     acquire_uninitialized(): median=    1.70 ns/op  (1.0x faster)
            acquire_zeroed(): median=     2.8 ns/op

--- N = 10000 objects ---
[Cooling: size transition] [Ready]
              acquire(value): median=    1.82 ns/op
     acquire_uninitialized(): median=    2.38 ns/op  (0.8x faster)
            acquire_zeroed(): median=     2.4 ns/op

--- N = 100000 objects ---
[Cooling: size transition] [Ready]
              acquire(value): median=    1.48 ns/op
     acquire_uninitialized(): median=    1.93 ns/op  (0.8x faster)
            acquire_zeroed(): median=     2.5 ns/op
[Cooling: before multithreaded] [Ready]

================================================================================
  Multi-threaded Contention (Thread-Safe Pools)
================================================================================
Contract: Concurrent acquire/release from multiple threads
          Tests lock contention and scalability
          Comparing: fat_p::ThreadSafeObjectPool vs std::pmr::synchronized_pool_resource

[2026-02-16 04:26:45] CPU: 2445 MHz (~base: 2445)

--- 1 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready]
       fat_p::ThreadSafePool: median=   19.40 ns/op  throughput=  51540414 ops/sec
         std::pmr::sync_pool: median=   40.57 ns/op  throughput=  24647996 ops/sec

--- 2 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready]
       fat_p::ThreadSafePool: median=   75.85 ns/op  throughput=  13183107 ops/sec
         std::pmr::sync_pool: median=  123.00 ns/op  throughput=   8130237 ops/sec

--- 4 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready]
       fat_p::ThreadSafePool: median=  105.85 ns/op  throughput=   9446953 ops/sec
         std::pmr::sync_pool: median=  172.21 ns/op  throughput=   5806723 ops/sec

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-object overhead (in addition to object storage):
    fat_p::ObjectPool:    8 bytes (next pointer in free list)
    boost::object_pool:   8 bytes (chunk linkage)
    foonathan::memory:    8 bytes (node linkage)
    EASTL::fixed_pool:    8 bytes (free list pointer)
    std::pmr::pool:       8-16 bytes (block headers)
    new/delete:           8-24 bytes (malloc metadata)

  Feature comparison:
    Allocator               O(1)   Thread-Safe  Auto-Grow  RAII Wrapper  try_acquire
    ---------------------------------------------------------------------------------
    fat_p::ObjectPool       Yes    Optional     Yes        Yes           Yes
    boost::object_pool      Yes    No           Yes        No            No
    foonathan::memory       Yes    Optional     Yes        No            No
    EASTL::fixed_pool [!]   Yes    No           NO         No            Yes*
    std::pmr::unsync_pool   Yes    No           Yes        No            No
    std::pmr::sync_pool     Yes    Yes          Yes        No            No
    new/delete              No**   Yes          N/A        No            No

    [!] EASTL::fixed_pool is FIXED-CAPACITY (no auto-grow) - different contract
    *   EASTL returns nullptr when exhausted
    **  malloc may have O(1) fast path but can degrade

  fat_p::ObjectPool<MediumObject> with capacity 10000:
    sizeof(MediumObject): 64 bytes
    total_capacity:       10000 objects
    num_blocks:           1
    block_size:           10000 objects

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  Acquire + Release Cycle (SmallTrivial 16B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 04:54:22] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    6.80 ns/op  mean=    6.82 +/-  0.05
          boost::object_pool: median=    7.50 ns/op  mean=    7.44 +/-  0.39
   EASTL::fixed_pool [!grow]: median=    5.90 ns/op  mean=    5.90 +/-  0.09
       std::pmr::unsync_pool: median=   16.50 ns/op  mean=   16.59 +/-  0.19
                  new/delete: median=   39.80 ns/op  mean=   42.01 +/-  9.77
  Speedup vs new/delete: 5.9x

--- N = 10000 ops ---
[2026-02-16 04:54:24] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    6.79 ns/op  mean=    6.95 +/-  1.03
          boost::object_pool: median=    7.42 ns/op  mean=    7.63 +/-  0.72
   EASTL::fixed_pool [!grow]: median=    4.45 ns/op  mean=    7.52 +/-  6.36
       std::pmr::unsync_pool: median=   16.38 ns/op  mean=   16.81 +/-  1.04
                  new/delete: median=   39.92 ns/op  mean=   40.89 +/-  1.61
  Speedup vs new/delete: 5.9x

--- N = 100000 ops ---
[2026-02-16 04:54:27] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    6.18 ns/op  mean=    6.31 +/-  0.59
          boost::object_pool: median=    7.42 ns/op  mean=    7.46 +/-  1.07
   EASTL::fixed_pool [!grow]: median=    5.86 ns/op  mean=    5.70 +/-  0.50
       std::pmr::unsync_pool: median=   16.70 ns/op  mean=   16.67 +/-  0.17
                  new/delete: median=   40.52 ns/op  mean=   40.80 +/-  0.69
  Speedup vs new/delete: 6.6x
[Cooling: before bulk acquire] [Ready: 2445 MHz]

================================================================================
  Bulk Acquire (SmallTrivial 16B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput
--- N = 1000 objects ---
[2026-02-16 04:54:34] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    4.80 ns/op  mean=    4.88 +/-  0.16
          boost::object_pool: median=    6.30 ns/op  mean=    7.08 +/-  3.09
   EASTL::fixed_pool [!grow]: median=    3.50 ns/op  mean=    3.69 +/-  0.52
       std::pmr::unsync_pool: median=   12.90 ns/op  mean=   13.24 +/-  1.51
                  new/delete: median=   31.15 ns/op  mean=   31.90 +/-  2.86

--- N = 10000 objects ---
[2026-02-16 04:54:36] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    6.05 ns/op  mean=    5.85 +/-  0.92
          boost::object_pool: median=   14.71 ns/op  mean=   14.91 +/-  1.36
   EASTL::fixed_pool [!grow]: median=    4.38 ns/op  mean=    4.47 +/-  1.03
       std::pmr::unsync_pool: median=   25.93 ns/op  mean=   25.77 +/-  2.87
                  new/delete: median=   45.31 ns/op  mean=   45.50 +/-  4.05

--- N = 100000 objects ---
[2026-02-16 04:54:40] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    4.81 ns/op  mean=    4.89 +/-  0.72
          boost::object_pool: median=   11.62 ns/op  mean=   11.63 +/-  0.60
   EASTL::fixed_pool [!grow]: median=    3.49 ns/op  mean=    3.55 +/-  0.47
       std::pmr::unsync_pool: median=   17.01 ns/op  mean=   17.11 +/-  0.72
                  new/delete: median=   39.24 ns/op  mean=   39.20 +/-  0.66
[Cooling: before interleaved] [Ready: 2445 MHz]

================================================================================
  Interleaved Acquire/Release (SmallTrivial 16B)
================================================================================
Contract: Realistic workload with interleaved operations
          50% acquire, 50% release (steady-state simulation)
--- N = 1000 operations ---
[2026-02-16 04:56:55] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=   15.65 ns/op  mean=   15.66 +/-  0.18
          boost::object_pool: median=   19.70 ns/op  mean=   20.02 +/-  1.25
   EASTL::fixed_pool [!grow]: median=   15.00 ns/op  mean=   15.65 +/-  2.70
       std::pmr::unsync_pool: median=   22.60 ns/op  mean=   25.33 +/- 11.88
                  new/delete: median=   37.15 ns/op  mean=   37.69 +/-  1.98

--- N = 10000 operations ---
[2026-02-16 04:56:57] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=   15.59 ns/op  mean=   15.77 +/-  0.77
          boost::object_pool: median=   20.84 ns/op  mean=   21.51 +/-  1.95
   EASTL::fixed_pool [!grow]: median=   14.33 ns/op  mean=   14.39 +/-  0.16
       std::pmr::unsync_pool: median=   21.64 ns/op  mean=   22.06 +/-  1.11
                  new/delete: median=   36.65 ns/op  mean=   37.35 +/-  1.40

--- N = 100000 operations ---
[2026-02-16 04:57:00] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=   15.68 ns/op  mean=   15.64 +/-  0.31
          boost::object_pool: median=   21.83 ns/op  mean=   21.93 +/-  0.62
   EASTL::fixed_pool [!grow]: median=   14.98 ns/op  mean=   14.98 +/-  0.19
       std::pmr::unsync_pool: median=   21.34 ns/op  mean=   21.35 +/-  0.23
                  new/delete: median=   37.32 ns/op  mean=   37.44 +/-  0.65
[Cooling: before pool reuse] [Ready: 2445 MHz]

================================================================================
  Pool Reuse / Free List Efficiency (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all in random order, acquire N again
          Tests free list traversal and memory reuse
--- N = 1000 objects ---
[2026-02-16 04:57:07] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    5.55 ns/op  mean=    5.71 +/-  1.03
          boost::object_pool: median=    5.10 ns/op  mean=    5.11 +/-  0.50
   EASTL::fixed_pool [!grow]: median=    3.80 ns/op  mean=    4.47 +/-  2.99
       std::pmr::unsync_pool: median=   12.30 ns/op  mean=   13.06 +/-  3.17
                  new/delete: median=   33.90 ns/op  mean=   34.66 +/-  4.73

--- N = 10000 objects ---
[2026-02-16 04:57:09] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    4.82 ns/op  mean=    5.80 +/-  1.39
          boost::object_pool: median=    5.70 ns/op  mean=    5.96 +/-  0.63
   EASTL::fixed_pool [!grow]: median=    3.80 ns/op  mean=    4.19 +/-  0.65
       std::pmr::unsync_pool: median=   16.73 ns/op  mean=   17.68 +/-  1.58
                  new/delete: median=   39.12 ns/op  mean=   40.13 +/-  5.14

--- N = 100000 objects ---
[2026-02-16 04:57:15] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=   14.71 ns/op  mean=   14.69 +/-  0.25
          boost::object_pool: median=    5.16 ns/op  mean=    5.19 +/-  0.72
   EASTL::fixed_pool [!grow]: median=   13.35 ns/op  mean=   13.35 +/-  0.21
       std::pmr::unsync_pool: median=   15.97 ns/op  mean=   15.88 +/-  0.45
                  new/delete: median=   38.16 ns/op  mean=   38.63 +/-  0.99
[Cooling: before pool reuse with compact] [Ready: 2445 MHz]

================================================================================
  Pool Reuse With Compaction (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all randomly, COMPACT, then acquire N again
          Tests whether compaction recovers allocation locality
--- N = 1000 objects ---
[2026-02-16 05:00:40] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    6.60 ns/op  mean=    6.15 +/-  0.87
          boost::object_pool: median=    6.20 ns/op  mean=    5.72 +/-  0.75
   EASTL::fixed_pool [!grow]: median=    4.50 ns/op  mean=    4.63 +/-  1.52
       std::pmr::unsync_pool: median=   15.05 ns/op  mean=   14.78 +/-  2.72
                  new/delete: median=   40.80 ns/op  mean=   39.33 +/-  6.42

--- N = 10000 objects ---
[2026-02-16 05:00:42] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    4.79 ns/op  mean=    5.22 +/-  1.35
          boost::object_pool: median=    5.77 ns/op  mean=    5.90 +/-  0.40
   EASTL::fixed_pool [!grow]: median=    3.71 ns/op  mean=    4.01 +/-  0.52
       std::pmr::unsync_pool: median=   16.77 ns/op  mean=   17.20 +/-  2.85
                  new/delete: median=   38.39 ns/op  mean=   37.81 +/-  5.35

--- N = 100000 objects ---
[2026-02-16 05:00:47] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    4.09 ns/op  mean=    4.59 +/-  1.05
          boost::object_pool: median=    5.71 ns/op  mean=    5.69 +/-  0.71
   EASTL::fixed_pool [!grow]: median=   13.44 ns/op  mean=   13.51 +/-  0.60
       std::pmr::unsync_pool: median=   16.07 ns/op  mean=   17.20 +/-  2.92
                  new/delete: median=   39.76 ns/op  mean=   42.12 +/-  6.59
[Cooling: before pool reuse full cycle] [Ready: 2445 MHz]

================================================================================
  Pool Reuse Full Cycle (SmallTrivial 16B)
================================================================================
Contract: Acquire N, then TIME [release all randomly + compact + reacquire all]
          True cost of 'flush and refill' pattern (no hidden work)
          Note: Boost's ordered_free O(N) cost is now visible
--- N = 1000 objects ---
[2026-02-16 05:04:14] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    9.90 ns/op  mean=    9.51 +/-  0.87
          boost::object_pool: median=  667.45 ns/op  mean=  674.85 +/- 15.31
   EASTL::fixed_pool [!grow]: median=    6.25 ns/op  mean=    5.89 +/-  0.77
       std::pmr::unsync_pool: median=   21.10 ns/op  mean=   20.77 +/-  2.16
                  new/delete: median=   50.60 ns/op  mean=   49.28 +/-  2.63

--- N = 10000 objects ---
[2026-02-16 05:04:16] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    8.57 ns/op  mean=    9.62 +/-  1.95
          boost::object_pool: median=10045.66 ns/op  mean=10044.60 +/- 44.46
   EASTL::fixed_pool [!grow]: median=    6.62 ns/op  mean=    7.27 +/-  1.21
       std::pmr::unsync_pool: median=   26.93 ns/op  mean=   26.19 +/-  3.05
                  new/delete: median=   54.62 ns/op  mean=   55.85 +/-  7.00

--- N = 100000 objects ---
[2026-02-16 05:04:22] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=   10.41 ns/op  mean=   10.70 +/-  1.52
          boost::object_pool: median=35406.06 ns/op  mean=35423.87 +/- 96.92
   EASTL::fixed_pool [!grow]: median=   18.25 ns/op  mean=   18.73 +/-  1.14
       std::pmr::unsync_pool: median=   32.34 ns/op  mean=   34.39 +/-  6.59
                  new/delete: median=   73.94 ns/op  mean=   74.20 +/-  7.16
[Cooling: before medium object benchmarks] [Ready: 2445 MHz]

================================================================================
  Acquire + Release Cycle (MediumObject 64B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 05:07:47] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    7.80 ns/op  mean=    7.77 +/-  0.05
          boost::object_pool: median=    7.60 ns/op  mean=    7.56 +/-  0.23
   EASTL::fixed_pool [!grow]: median=    5.90 ns/op  mean=    5.92 +/-  0.08
       std::pmr::unsync_pool: median=   17.20 ns/op  mean=   17.18 +/-  0.08
                  new/delete: median=   40.40 ns/op  mean=   42.58 +/-  9.61
  Speedup vs new/delete: 5.2x

--- N = 10000 ops ---
[2026-02-16 05:07:49] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    7.73 ns/op  mean=    7.79 +/-  0.75
          boost::object_pool: median=    7.45 ns/op  mean=    7.59 +/-  0.57
   EASTL::fixed_pool [!grow]: median=    4.79 ns/op  mean=    4.80 +/-  0.06
       std::pmr::unsync_pool: median=   17.04 ns/op  mean=   17.46 +/-  1.27
                  new/delete: median=   40.36 ns/op  mean=   41.06 +/-  1.49
  Speedup vs new/delete: 5.2x

--- N = 50000 ops ---
[2026-02-16 05:07:52] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    7.72 ns/op  mean=    7.53 +/-  1.24
          boost::object_pool: median=    7.43 ns/op  mean=    7.71 +/-  0.80
   EASTL::fixed_pool [!grow]: median=    5.87 ns/op  mean=    5.88 +/-  0.30
       std::pmr::unsync_pool: median=   17.16 ns/op  mean=   17.76 +/-  1.30
                  new/delete: median=   41.05 ns/op  mean=   42.21 +/-  3.68
  Speedup vs new/delete: 5.3x
[Cooling: before bulk acquire] [Ready: 2445 MHz]

================================================================================
  Bulk Acquire (MediumObject 64B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput
--- N = 1000 objects ---
[2026-02-16 05:07:58] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    6.50 ns/op  mean=    6.81 +/-  1.53
          boost::object_pool: median=    8.65 ns/op  mean=   10.23 +/-  5.89
   EASTL::fixed_pool [!grow]: median=    4.70 ns/op  mean=    5.00 +/-  1.18
       std::pmr::unsync_pool: median=   14.30 ns/op  mean=   15.66 +/-  3.93
                  new/delete: median=   32.95 ns/op  mean=   35.45 +/-  7.98

--- N = 10000 objects ---
[2026-02-16 05:08:01] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    5.22 ns/op  mean=    5.38 +/-  0.46
          boost::object_pool: median=   11.88 ns/op  mean=   18.45 +/- 12.87
   EASTL::fixed_pool [!grow]: median=    4.29 ns/op  mean=    4.40 +/-  0.49
       std::pmr::unsync_pool: median=   21.32 ns/op  mean=   23.99 +/-  6.92
                  new/delete: median=   37.83 ns/op  mean=   39.76 +/-  7.58

--- N = 50000 objects ---
[2026-02-16 05:08:05] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=    5.35 ns/op  mean=    5.67 +/-  0.98
          boost::object_pool: median=   26.20 ns/op  mean=   26.81 +/-  2.77
   EASTL::fixed_pool [!grow]: median=    4.67 ns/op  mean=    4.67 +/-  0.50
       std::pmr::unsync_pool: median=   31.86 ns/op  mean=   32.95 +/-  5.09
                  new/delete: median=   46.41 ns/op  mean=   46.06 +/- 12.50
[Cooling: before large object benchmarks] [Ready: 2445 MHz]

================================================================================
  Acquire + Release Cycle (LargeObject 256B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth
--- N = 1000 ops ---
[2026-02-16 05:08:51] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=   11.50 ns/op  mean=   13.76 +/-  3.20
          boost::object_pool: median=   11.70 ns/op  mean=   12.26 +/-  2.58
   EASTL::fixed_pool [!grow]: median=   10.90 ns/op  mean=   11.88 +/-  2.18
       std::pmr::unsync_pool: median=   23.40 ns/op  mean=   24.21 +/-  3.28
                  new/delete: median=   46.65 ns/op  mean=   48.84 +/-  9.55
  Speedup vs new/delete: 4.1x

--- N = 10000 ops ---
[2026-02-16 05:08:54] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
           fat_p::ObjectPool: median=   11.47 ns/op  mean=   12.93 +/-  2.24
          boost::object_pool: median=   11.57 ns/op  mean=   11.75 +/-  0.73
   EASTL::fixed_pool [!grow]: median=   10.83 ns/op  mean=   10.91 +/-  0.17
       std::pmr::unsync_pool: median=   23.26 ns/op  mean=   23.74 +/-  1.14
                  new/delete: median=   46.66 ns/op  mean=   47.19 +/-  1.33
  Speedup vs new/delete: 4.1x
[Cooling: before specialized acquire] [Ready: 2445 MHz]

================================================================================
  Specialized Acquire (SmallTrivial - fat_p only)
================================================================================
Contract: Compare acquire() vs acquire_uninitialized() vs acquire_zeroed()
          Shows overhead of zero-initialization and default construction

[2026-02-16 05:09:00] CPU: 2445 MHz (base: 2445)

--- N = 1000 objects ---
[Cooling: size transition] [Ready: 2445 MHz]
              acquire(value): median=    3.20 ns/op
     acquire_uninitialized(): median=    1.70 ns/op  (1.9x faster)
            acquire_zeroed(): median=     2.9 ns/op

--- N = 10000 objects ---
[Cooling: size transition] [Ready: 2445 MHz]
              acquire(value): median=    3.26 ns/op
     acquire_uninitialized(): median=    2.08 ns/op  (1.6x faster)
            acquire_zeroed(): median=     2.9 ns/op

--- N = 100000 objects ---
[Cooling: size transition] [Ready: 2445 MHz]
              acquire(value): median=    3.28 ns/op
     acquire_uninitialized(): median=    1.98 ns/op  (1.7x faster)
            acquire_zeroed(): median=     3.0 ns/op
[Cooling: before multithreaded] [Ready: 2445 MHz]

================================================================================
  Multi-threaded Contention (Thread-Safe Pools)
================================================================================
Contract: Concurrent acquire/release from multiple threads
          Tests lock contention and scalability
          Comparing: fat_p::ThreadSafeObjectPool vs std::pmr::synchronized_pool_resource

[2026-02-16 05:09:12] CPU: 2445 MHz (base: 2445)

--- 1 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2445 MHz]
       fat_p::ThreadSafePool: median=   40.86 ns/op  throughput=  24473813 ops/sec
         std::pmr::sync_pool: median=   52.86 ns/op  throughput=  18917896 ops/sec

--- 2 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2445 MHz]
       fat_p::ThreadSafePool: median=   52.20 ns/op  throughput=  19157088 ops/sec
         std::pmr::sync_pool: median=   63.29 ns/op  throughput=  15799660 ops/sec

--- 4 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2445 MHz]
       fat_p::ThreadSafePool: median=   67.68 ns/op  throughput=  14775687 ops/sec
         std::pmr::sync_pool: median=   75.87 ns/op  throughput=  13180657 ops/sec

================================================================================
  Memory Overhead Comparison (Theoretical)
================================================================================

  Per-object overhead (in addition to object storage):
    fat_p::ObjectPool:    8 bytes (next pointer in free list)
    boost::object_pool:   8 bytes (chunk linkage)
    EASTL::fixed_pool:    8 bytes (free list pointer)
    std::pmr::pool:       8-16 bytes (block headers)
    new/delete:           8-24 bytes (malloc metadata)

  Feature comparison:
    Allocator               O(1)   Thread-Safe  Auto-Grow  RAII Wrapper  try_acquire
    ---------------------------------------------------------------------------------
    fat_p::ObjectPool       Yes    Optional     Yes        Yes           Yes
    boost::object_pool      Yes    No           Yes        No            No
    EASTL::fixed_pool [!]   Yes    No           NO         No            Yes*
    std::pmr::unsync_pool   Yes    No           Yes        No            No
    std::pmr::sync_pool     Yes    Yes          Yes        No            No
    new/delete              No**   Yes          N/A        No            No

    [!] EASTL::fixed_pool is FIXED-CAPACITY (no auto-grow) - different contract
    *   EASTL returns nullptr when exhausted
    **  malloc may have O(1) fast path but can degrade

  fat_p::ObjectPool<MediumObject> with capacity 10000:
    sizeof(MediumObject): 64 bytes
    total_capacity:       10000 objects
    num_blocks:           1
    block_size:           10000 objects

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- foonathan::memory_pool (vcpkg install foonathan-memory) was not detected on MSVC CI.
