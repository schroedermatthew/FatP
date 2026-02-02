# FAT-P Library Complete Benchmark Results

**Run ID:** 20260201_184451  
**Date:** 2026-02-01  
**Platform:** Windows-x64, MSVC 19.50  
**CPU:** 24 threads, 3686 MHz base frequency  
**Methodology:** warmup=3, measured=15, seed=12345, round-robin execution

---


---

# AlignedVector

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::AlignedVector Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::AlignedVector (primary)
  [x] std::vector (baseline)
  [x] boost::alignment::aligned_allocator
  [ ] Eigen (not detected)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement

Cooling: section=500ms size=100ms case=50ms

Checking initial CPU state...
[2026-02-01 18:44:52] Initial CPU: 3133 MHz (base: 3686)
Waiting for CPU to stabilize...
  [CPU stable at 2159 MHz, variance 4.5%]


================================================================================
  ALIGNMENT VERIFICATION
================================================================================

Contract: Verifies alignment guarantees across multiple allocations. All allocations must meet specified alignment.

[2026-02-01 18:44:53] Alignment Verification CPU: 2211 MHz (base: 3686)

--- Alignment = 16 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 32 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 64 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 128 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

--- Alignment = 256 bytes ---

  Allocations: 1000, Violations: 0 [PASS]

================================================================================
  SEQUENTIAL ITERATION (Sum)
================================================================================

Contract: Measures cache-line alignment benefit for SIMD auto-vectorization. assume_aligned() provides compiler hints for aligned loads.

[2026-02-01 18:44:53] Sequential Iteration CPU: 2690 MHz (base: 3686)

--- N = 10000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector        0.37 ns/elem        0.40 ns/elem      0.09  [0.35, 0.45]
             AlignedVector<64>        0.37 ns/elem        0.39 ns/elem      0.04  [0.37, 0.41]
AlignedVector<64>::assume_aligned        0.37 ns/elem        0.37 ns/elem      0.01  [0.37, 0.38]
            AlignedVector<128>        0.37 ns/elem        0.37 ns/elem      0.01  [0.37, 0.38]
  boost::aligned_allocator<64>        0.37 ns/elem        0.40 ns/elem      0.07  [0.36, 0.43]

--- N = 100000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector        0.37 ns/elem        0.37 ns/elem      0.01  [0.37, 0.38]
             AlignedVector<64>        0.37 ns/elem        0.37 ns/elem      0.01  [0.36, 0.38]
AlignedVector<64>::assume_aligned        0.37 ns/elem        0.37 ns/elem      0.01  [0.37, 0.37]
            AlignedVector<128>        0.37 ns/elem        0.38 ns/elem      0.01  [0.37, 0.38]
  boost::aligned_allocator<64>        0.37 ns/elem        0.37 ns/elem      0.01  [0.37, 0.38]

================================================================================
  SIMD DOT PRODUCT
================================================================================

Contract: Tests fused multiply-add vectorization potential. assume_aligned() enables compiler to use aligned SIMD loads.

[2026-02-01 18:44:54] Dot Product CPU: 2137 MHz (base: 3686)

--- N = 10000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector        0.37 ns/elem        0.37 ns/elem      0.04  [0.35, 0.39]
             AlignedVector<64>        0.37 ns/elem        0.36 ns/elem      0.01  [0.36, 0.37]
AlignedVector<64>::assume_aligned        0.36 ns/elem        0.38 ns/elem      0.07  [0.34, 0.41]

--- N = 100000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector        0.36 ns/elem        0.37 ns/elem      0.01  [0.36, 0.37]
             AlignedVector<64>        0.37 ns/elem        0.36 ns/elem      0.01  [0.36, 0.37]
AlignedVector<64>::assume_aligned        0.36 ns/elem        0.37 ns/elem      0.02  [0.36, 0.37]

================================================================================
  SIMD SAXPY (Explicit AVX2)
================================================================================

Contract: Skipped: AVX2 not enabled at compile time. Build with -mavx2 or -march=native (or /arch:AVX2 on MSVC). 


================================================================================
  RANDOM ACCESS
================================================================================

Contract: Measures cache behavior with non-sequential access. Alignment has less impact here; tests baseline overhead.

[2026-02-01 18:44:56] Random Access CPU: 2359 MHz (base: 3686)

--- N = 10000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector        0.58 ns/elem        0.55 ns/elem      0.08  [0.51, 0.59]
             AlignedVector<64>        0.58 ns/elem        0.54 ns/elem      0.08  [0.50, 0.58]

--- N = 100000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector        0.40 ns/elem        0.40 ns/elem      0.01  [0.39, 0.40]
             AlignedVector<64>        0.40 ns/elem        0.40 ns/elem      0.02  [0.39, 0.41]

================================================================================
  PUSH_BACK GROWTH
================================================================================

Contract: Measures amortized insertion cost. 'reserved' variant pre-allocates to isolate allocation overhead from element construction.

[2026-02-01 18:44:56] Push Back CPU: 2137 MHz (base: 3686)

--- N = 1000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
            std::vector (grow)        1.71 ns/elem        1.62 ns/elem      0.33  [1.44, 1.79]
        std::vector (reserved)        0.60 ns/elem        0.92 ns/elem      0.90  [0.46, 1.39]
  [NOTE] High variance (stddev > median)
      AlignedVector<64> (grow)        0.73 ns/elem        0.82 ns/elem      0.17  [0.73, 0.92]
  AlignedVector<64> (reserved)        0.42 ns/elem        0.48 ns/elem      0.10  [0.43, 0.54]

--- N = 10000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
            std::vector (grow)        0.87 ns/elem        0.91 ns/elem      0.11  [0.85, 0.97]
        std::vector (reserved)        0.58 ns/elem        0.58 ns/elem      0.04  [0.56, 0.61]
      AlignedVector<64> (grow)        0.55 ns/elem        0.56 ns/elem      0.04  [0.54, 0.58]
  AlignedVector<64> (reserved)        0.39 ns/elem        0.38 ns/elem      0.01  [0.38, 0.39]

================================================================================
  INSERT AT MIDDLE
================================================================================

Contract: Measures O(n) insertion at middle position. Tests element shifting performance.

[2026-02-01 18:44:57] Insert CPU: 2617 MHz (base: 3686)

--- Initial size = 1000, inserting 100 elements ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector       17.46 ns/insert       20.97 ns/insert      7.79  [16.94, 24.99]
             AlignedVector<64>       17.84 ns/insert       19.16 ns/insert      3.98  [17.11, 21.22]

--- Initial size = 10000, inserting 100 elements ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
                   std::vector      128.88 ns/insert      130.44 ns/insert      5.06  [127.82, 133.05]
             AlignedVector<64>      129.44 ns/insert      132.36 ns/insert     10.85  [126.75, 137.96]

================================================================================
  SHIFT MICROBENCH (memmove fast-path)
================================================================================

Contract: Isolates the cost of shifting elements during insert/erase range operations. Compares a trivially copyable element type (int) that can use bulk memmove shifts against an equivalent-size non-trivially copyable wrapper that forces element-wise moves. Reported as ns per shifted element.

[2026-02-01 18:44:58] Shift/memmove CPU: 2911 MHz (base: 3686)

--- N = 10000, range K = 32 at middle ---


--- insert(pos, first, last)   [shift right] ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
AlignedVector<int,64> (trivial => memmove shift)        0.03 ns/shifted-elem        0.03 ns/shifted-elem      0.00  [0.03, 0.03]
AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)        0.26 ns/shifted-elem        0.25 ns/shifted-elem      0.03  [0.24, 0.27]

--- erase(first, last)        [shift left] ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
AlignedVector<int,64> (trivial => memmove shift)        0.02 ns/shifted-elem        0.02 ns/shifted-elem      0.00  [0.02, 0.02]
AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)        0.19 ns/shifted-elem        0.20 ns/shifted-elem      0.03  [0.18, 0.21]

--- N = 100000, range K = 32 at middle ---


--- insert(pos, first, last)   [shift right] ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
AlignedVector<int,64> (trivial => memmove shift)        0.04 ns/shifted-elem        0.05 ns/shifted-elem      0.02  [0.04, 0.05]
AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)        0.19 ns/shifted-elem        0.19 ns/shifted-elem      0.01  [0.19, 0.20]

--- erase(first, last)        [shift left] ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
AlignedVector<int,64> (trivial => memmove shift)        0.03 ns/shifted-elem        0.03 ns/shifted-elem      0.00  [0.03, 0.03]
AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)        0.19 ns/shifted-elem        0.19 ns/shifted-elem      0.01  [0.19, 0.20]

================================================================================
  COPY AND MOVE OPERATIONS
================================================================================

Contract: Copy construction should be O(n). Move construction/assignment should be O(1). This section reports:
  1) copy-ctor (bulk copy)
  2) move-ctor (rotation; no allocation)
  3) move-assign (rotation; no allocation)
  4) construct+move (includes allocation+fill; O(n), not a pure move test)

[2026-02-01 18:44:59] Copy/Move CPU: 2137 MHz (base: 3686)

--- N = 10000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
         std::vector copy-ctor     1083.20 ns/op     1067.79 ns/op     25.97  [1054.38, 1081.20]
std::vector move-ctor (rotation)        0.50 ns/op        0.49 ns/op      0.02  [0.48, 0.51]
std::vector move-assign (rotation)        0.42 ns/op        0.42 ns/op      0.01  [0.41, 0.43]
std::vector construct+move (alloc+fill)     2596.00 ns/op     2644.73 ns/op    297.13  [2491.30, 2798.17]
   AlignedVector<64> copy-ctor      986.20 ns/op      983.59 ns/op     32.74  [966.68, 1000.50]
AlignedVector<64> move-ctor (rotation)        0.44 ns/op        0.45 ns/op      0.03  [0.43, 0.46]
AlignedVector<64> move-assign (rotation)        0.41 ns/op        0.40 ns/op      0.01  [0.39, 0.40]
AlignedVector<64> construct+move (alloc+fill)     4748.00 ns/op     4755.13 ns/op    335.84  [4581.71, 4928.56]

--- N = 100000 ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
         std::vector copy-ctor    29272.00 ns/op    28976.43 ns/op   1483.91  [28210.14, 29742.72]
std::vector move-ctor (rotation)        0.50 ns/op        0.49 ns/op      0.01  [0.49, 0.50]
std::vector move-assign (rotation)        0.41 ns/op        0.41 ns/op      0.01  [0.40, 0.41]
std::vector construct+move (alloc+fill)    39900.00 ns/op    40301.00 ns/op   1318.26  [39620.25, 40981.75]
   AlignedVector<64> copy-ctor    28590.50 ns/op    28584.77 ns/op   1470.10  [27825.61, 29343.92]
AlignedVector<64> move-ctor (rotation)        0.45 ns/op        0.46 ns/op      0.04  [0.44, 0.48]
AlignedVector<64> move-assign (rotation)        0.39 ns/op        0.39 ns/op      0.01  [0.39, 0.40]
AlignedVector<64> construct+move (alloc+fill)    63534.00 ns/op    64098.07 ns/op   2142.65  [62991.61, 65204.53]

================================================================================
  CORNER CASES
================================================================================

Contract: Tests edge cases that may trigger different code paths. Empty vector operations, single element, capacity boundary.

[2026-02-01 18:45:00] Corner Cases CPU: 2322 MHz (base: 3686)

--- Empty Vector Operations ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
   std::vector empty begin/end        2.34 ns/op        2.35 ns/op      0.04  [2.33, 2.37]
 AlignedVector empty begin/end        3.57 ns/op        3.54 ns/op      0.08  [3.50, 3.58]

--- Single Element Push/Pop Cycle ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
          std::vector push/pop        1.38 ns/cycle        1.49 ns/cycle      0.14  [1.42, 1.56]
        AlignedVector push/pop        0.94 ns/cycle        1.10 ns/cycle      0.41  [0.89, 1.31]

--- Insert at Capacity (Reallocation) ---

                     Container        Median          Mean      Stddev  CI95
-------------------------------------------------------------------------------
           std::vector realloc      656.25 ns/realloc      679.19 ns/realloc    170.18  [591.31, 767.07]
         AlignedVector realloc     1035.55 ns/realloc     1054.17 ns/realloc    244.83  [927.74, 1180.59]

================================================================================
  Benchmark Complete
================================================================================
[2026-02-01 18:45:00] Final CPU: 2359 MHz (base: 3686)
```


---

# AllocationStrategies

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::AllocationStrategies Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::AllocationStrategies (primary)
  [x] std::pmr (baseline)
  [x] boost::pool

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement

Checking initial CPU state...
[2026-02-01 18:45:01] Initial CPU: 3169 MHz (base: 3686)
  Waiting for CPU to stabilize...
[Waiting: 3169/3686 MHz (14% below base)]
[Ready: 3575/3686 MHz]


================================================================================
  SINGLE ALLOCATION/DEALLOCATION
================================================================================

[2026-02-01 18:45:02] Start CPU: 3575 MHz (base: 3686)
Contract: Measures time for one allocate() + deallocate() cycle. Allocation includes construction, deallocation includes destruction.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.34         19.84        1.22  [  19.17,   20.52]
fat_p::BlockAllocator                     1.11          1.13        0.04  [   1.11,    1.16]
fat_p::PoolAllocator                      1.27          1.47        0.73  [   1.07,    1.88]
--------------------------------------------------------------------------------------------
std::allocator                           19.47         20.17        1.37  [  19.41,   20.92]
std::pmr::monotonic                       2.71          2.72        0.30  [   2.55,    2.88]
std::pmr::unsync_pool                     4.24          4.58        0.82  [   4.12,    5.03]
boost::pool (raw)                         1.27          1.32        0.13  [   1.25,    1.40]

  [Correctness: PASS]
[2026-02-01 18:45:02] End CPU: 3022 MHz (base: 3686)

================================================================================
  BURST ALLOCATION (100 objects)
================================================================================

[2026-02-01 18:45:03] Start CPU: 2359 MHz (base: 3686)
Contract: Allocate 100 objects, then deallocate all. Measures bulk allocation pattern common in container growth.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator              2440.20       2448.98       29.74  [2432.54, 2465.41]
fat_p::BlockAllocator                    98.63        101.57        9.70  [  96.21,  106.93]
fat_p::PoolAllocator                    138.42        146.97       26.73  [ 132.20,  161.74]
--------------------------------------------------------------------------------------------
std::allocator                         2426.17       2440.59       46.55  [2414.87, 2466.31]
std::pmr::monotonic                     206.57        212.51       12.55  [ 205.58,  219.45]
std::pmr::unsync_pool                   813.99        808.65       48.45  [ 781.88,  835.42]
boost::pool (raw)                       190.17        196.78       22.35  [ 184.43,  209.12]
[2026-02-01 18:45:04] End CPU: 2469 MHz (base: 3686)

================================================================================
  CHURN PATTERN (Steady-State Mixed Operations)
================================================================================

[2026-02-01 18:45:04] Start CPU: 2469 MHz (base: 3686)
Contract: Simulates container churn with interleaved alloc/dealloc. Free list reuse should show advantage for BlockAllocator/PoolAllocator.

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.37         20.31        2.20  [  19.09,   21.53]
fat_p::BlockAllocator (warmed)            1.21          1.30        0.39  [   1.09,    1.51]
fat_p::PoolAllocator (warmed)             1.24          1.26        0.17  [   1.17,    1.36]
boost::pool (warmed)                      1.27          1.32        0.35  [   1.13,    1.52]
[2026-02-01 18:45:04] End CPU: 2727 MHz (base: 3686)

================================================================================
  SIZE SCALING (Allocation Count)
================================================================================

[2026-02-01 18:45:04] Start CPU: 2727 MHz (base: 3686)
Contract: Measures how allocation time scales with number of live objects. BlockAllocator should show constant time regardless of count.


--- N = 100 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.66         25.44        6.77  [  21.70,   29.18]
fat_p::BlockAllocator                     1.29          1.70        0.55  [   1.39,    2.00]
boost::pool                               1.28          1.57        0.43  [   1.33,    1.81]

--- N = 1000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.20         19.23        0.26  [  19.09,   19.37]
fat_p::BlockAllocator                     1.28          1.32        0.17  [   1.23,    1.42]
boost::pool                               1.25          1.24        0.05  [   1.21,    1.26]

--- N = 10000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.01         19.66        0.88  [  19.18,   20.15]
fat_p::BlockAllocator                     1.27          1.35        0.23  [   1.22,    1.47]
boost::pool                               1.30          1.31        0.07  [   1.27,    1.35]

--- N = 50000 ---

Allocator                          Median (ns)     Mean (ns)      Stddev                CI95
--------------------------------------------------------------------------------------------
fat_p::NewDeleteAllocator                19.11         19.55        1.12  [  18.92,   20.17]
fat_p::BlockAllocator                     1.32          1.44        0.28  [   1.28,    1.60]
boost::pool                               1.28          1.28        0.08  [   1.24,    1.33]
[2026-02-01 18:45:04] End CPU: 3169 MHz (base: 3686)

================================================================================
  Benchmark Complete
================================================================================
[2026-02-01 18:45:04] Final CPU: 3169 MHz (base: 3686)
```


---

# BitSet

```
================================================================================
  fat_p::BitSet Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::BitSet (primary)
  [x] std::bitset (baseline)
  [x] boost::dynamic_bitset
  [x] llvm::BitVector
  [x] llvm::SmallBitVector
  [x] roaring::Roaring (CRoaring)
  [x] bm::bvector (BitMagic)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement


======================================================================
  Section 1: Single-Bit Operations (N=1024)
======================================================================

--- Set Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.47          0.48        0.02
                 std::bitset          0.33          0.43        0.17
       boost::dynamic_bitset          0.29          0.29        0.01
             llvm::BitVector          0.32          0.32        0.01
            roaring::Roaring         42.59         44.94        5.28
               bm::bvector<>          4.64          4.47        0.25

======================================================================
  Section 2: Population Count (N=1024)
======================================================================

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          2.46          2.47        0.02
                 std::bitset          3.38          3.40        0.03
       boost::dynamic_bitset         48.40         58.84       18.58
             llvm::BitVector         15.27         16.48        3.25
   roaring::Roaring (cached)          1.96          2.00        0.15
               bm::bvector<>       1364.63       1387.65      124.76

======================================================================
  Section 3: Find Operations (N=1024, k=100)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.87          0.88        0.01
   std::bitset (manual scan)          2.84          2.85        0.02
       boost::dynamic_bitset          1.38          1.38        0.01
             llvm::BitVector          1.09          1.09        0.01
            roaring::Roaring          1.52          1.53        0.01
               bm::bvector<>          2.62          2.62        0.01

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet        296.50        298.21        3.55
     std::bitset (O(N) scan)        525.60        526.71        2.72
       boost::dynamic_bitset        387.80        388.42        1.29
             llvm::BitVector        247.90        334.44      149.82
            roaring::Roaring        425.60        427.12        4.15
               bm::bvector<>       1215.60       1642.05      526.53

======================================================================
  Section 3: Find Operations (N=10000, k=1000)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.93          1.22        0.48
   std::bitset (manual scan)          4.14          4.35        0.79
       boost::dynamic_bitset          2.54          2.68        0.38
             llvm::BitVector          1.79          1.65        0.22
            roaring::Roaring          2.19          2.10        0.25
               bm::bvector<>          2.62          2.70        0.14

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet       3077.00       3143.89      164.98
     std::bitset (O(N) scan)       5109.90       5501.70     1049.17
       boost::dynamic_bitset       4569.60       5930.13     2144.30
             llvm::BitVector       4125.20       4400.85     1580.96
            roaring::Roaring       4667.90       5035.14      815.09
               bm::bvector<>       3434.90       4781.29     1811.69

======================================================================
  Section 4: Sparse Iteration (N=1024, k=50)
======================================================================

Density: 4.88%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        149.02        162.30       25.69
     std::bitset (O(N) scan)        527.65        554.61       63.34
            roaring::Roaring        239.78        299.68      107.84
               bm::bvector<>       1149.38       1277.66      356.03

======================================================================
  Section 4: Sparse Iteration (N=10000, k=100)
======================================================================

Density: 1.00%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        328.90        346.39       55.39
     std::bitset (O(N) scan)       6338.24       6258.03      912.24
            roaring::Roaring        427.11        571.23      271.01
               bm::bvector<>       1058.03       1231.22      515.37

======================================================================
  Section 5: Bitwise Operations (N=1024)
======================================================================

--- AND Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          3.84          3.89        0.19
                 std::bitset          4.56          4.56        0.01
            roaring::Roaring        291.76        372.97      176.49
               bm::bvector<>       2179.68       2634.84      805.84

======================================================================
  Section 6: Range Operations (N=1024, range=100)
======================================================================

Fat-P has native set_range. Others require loops.

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
      fat_p::BitSet (native)          3.29          3.29        0.00
          std::bitset (loop)         93.24        119.43       48.44
    llvm::BitVector (native)        118.46        120.31        3.89
   roaring::Roaring (native)        207.12        185.66       40.70
      bm::bvector<> (native)        301.48        400.31      148.65

======================================================================
  Section 7: Object Sizes (bytes)
======================================================================

                          Type |        Size
---------------------------------------------
             fat_p::BitSet<64> |            8
           fat_p::BitSet<1024> |          128
               std::bitset<64> |            8
             std::bitset<1024> |          128
  boost::dynamic_bitset (base) |           32
        llvm::BitVector (base) |           72
          llvm::SmallBitVector |            8
       roaring::Roaring (base) |           40
          bm::bvector<> (base) |           72

======================================================================
  Benchmark Complete
======================================================================
```


---

# CircularBuffer

```
================================================================================
  fat_p::CircularBuffer Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=50 seed=12345

Competitors:
  [x] fat_p::CircularBuffer (primary)
  [x] fat_p::LockFreeRingBuffer (sibling SPSC)
  [x] std::mutex + std::deque (baseline)
  [x] boost::lockfree::spsc_queue
  [x] moodycamel::BlockingReaderWriterCircularBuffer

Configuration:
  Target work:    1000000 ops/batch
  Min batch ms:   50
  Scope:          ON
  Stabilize:      ON
  Cooldown:       ON

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement

Configuration:
  Warmup runs:    3
  Measured runs:  15
  Seed:           12345
  Target work:    1000000
  Min batch ms:   50
  Scope:          enabled
  Stabilize:      enabled
  Cooldown:       enabled

[2026-02-01 18:45:09] INIT CPU: 2580 MHz (base: 3686)

Correctness verification:
  [PASS] CircularBuffer FIFO ordering
  [PASS] CircularBuffer capacity enforcement
  [PASS] CircularBuffer SPSC thread safety

[BenchmarkScope] High priority, CPU non-0 affinity

================================================================================
  Single-Threaded Throughput (push + pop cycle)
================================================================================

[2026-02-01 18:45:09] CPU: 2580 MHz (base: 3686)
Contract note: Uncontended SPSC overhead. Measures push+pop per op. Capacity: 4096.

Warmup (3 runs)...
Measured runs (15 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           1.00        1.11      0.35  [0.93, 1.29] ns/op
fat_p::LockFreeRingBuffer (SPSC)                1.15        1.27      0.42  [1.06, 1.48] ns/op
std::mutex + std::deque (baseline)             22.46       24.78      6.23  [21.63, 27.93] ns/op
boost::lockfree::spsc_queue                     1.25        1.38      0.45  [1.15, 1.60] ns/op
moodycamel::BlockingRWCircularBuffer           20.68       23.25      7.42  [19.50, 27.01] ns/op

[2026-02-01 18:45:09] END CPU: 2580 MHz (base: 3686)

================================================================================
  SPSC Throughput (dedicated producer/consumer threads)
================================================================================

[2026-02-01 18:45:09] CPU: 2580 MHz (base: 3686)
Contract note: True SPSC pattern. Producer and consumer on separate threads. Capacity: 4096.

Warmup (3 runs)...
Measured runs (15 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                          34.05       36.24      7.47  [32.45, 40.02] ns/op
fat_p::LockFreeRingBuffer (SPSC)                8.16        9.58      4.67  [7.22, 11.94] ns/op
std::mutex + std::deque (baseline)             95.82      114.39     38.02  [95.15, 133.63] ns/op
boost::lockfree::spsc_queue                    21.43       22.41      6.34  [19.20, 25.62] ns/op
moodycamel::BlockingRWCircularBuffer           67.85       79.06     28.94  [64.41, 93.70] ns/op

[2026-02-01 18:45:14] END CPU: 3096 MHz (base: 3686)

================================================================================
  Burst Pattern (fill then drain cycles)
================================================================================

[2026-02-01 18:45:14] CPU: 3096 MHz (base: 3686)
Contract note: Simulates batched workloads. Burst size: 1024, bursts: 1000.

Warmup (3 runs)...
Measured runs (15 batches, round-robin)...

Library                                       Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::CircularBuffer                           0.89        0.97      0.17  [0.89, 1.06] ns/op
fat_p::LockFreeRingBuffer (SPSC)                0.96        0.96      0.03  [0.95, 0.98] ns/op
std::mutex + std::deque (baseline)              1.96        2.01      0.17  [1.92, 2.10] ns/op
boost::lockfree::spsc_queue                     0.89        0.91      0.06  [0.88, 0.94] ns/op
moodycamel::BlockingRWCircularBuffer           16.29       16.76      0.96  [16.27, 17.24] ns/op

[2026-02-01 18:45:15] END CPU: 2580 MHz (base: 3686)

================================================================================
  Capacity Sensitivity (SPSC throughput vs buffer size)
================================================================================

[2026-02-01 18:45:15] CPU: 2580 MHz (base: 3686)
Contract note: Fixed work per test. Smaller buffers may cause more contention.

       Capacity   Median ns/op     Throughput
--------------------------------------------------
             64           9.79       102.2 Mops/s
             1K           8.22       121.6 Mops/s
             4K          10.70        93.4 Mops/s
            64K           8.10       123.5 Mops/s

[2026-02-01 18:45:16] END CPU: 2432 MHz (base: 3686)

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
[2026-02-01 18:45:16] FINAL CPU: 2432 MHz (base: 3686)
```


---

# FatPHashMap

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::StableHashMap Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::StableHashMap (primary)
  [x] std::unordered_map (baseline)
  [x] tsl::robin_map
  [x] ankerl::unordered_dense
  [x] absl::flat_hash_map
  [x] boost::unordered_flat_map
  [ ] folly::F14FastMap (not detected)
  [x] llvm::DenseMap

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement

Cooling: section=2000ms size=1000ms case=300ms

Checking initial CPU state...
[2026-02-01 18:45:16] Initial CPU: 2469 MHz (base: 3686)
Waiting for CPU to stabilize before benchmarks...
[Waiting: 2786 MHz (variance: 22.5%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 16.3%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 16.7%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 16.6%, need <10.0%)]   
[Waiting: 2558 MHz (variance: 21.6%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 22.2%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 21.9%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 21.7%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 12.0%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 11.7%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 14.9%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 20.4%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 20.0%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 19.5%, need <10.0%)]   
[Waiting: 2698 MHz (variance: 23.2%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 21.8%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 22.5%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 22.8%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 22.7%, need <10.0%)]   
[Waiting: 2698 MHz (variance: 41.0%, need <10.0%)]   
[Waiting: 2852 MHz (variance: 37.5%, need <10.0%)]   
[Waiting: 3052 MHz (variance: 31.4%, need <10.0%)]   
[Waiting: 3125 MHz (variance: 22.4%, need <10.0%)]   
[Waiting: 3103 MHz (variance: 26.1%, need <10.0%)]   
[Waiting: 2956 MHz (variance: 24.9%, need <10.0%)]   
[Waiting: 2867 MHz (variance: 25.7%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 25.2%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 26.5%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 27.1%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 27.7%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 28.5%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 23.3%, need <10.0%)]   
[Waiting: 2410 MHz (variance: 10.7%, need <10.0%)]   
[Waiting: 2395 MHz (variance: 10.8%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 10.6%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 10.6%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 13.6%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 11.7%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 11.7%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 10.2%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 15.3%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 15.4%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 15.5%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 13.8%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 12.5%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 12.7%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 15.6%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 25.4%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 25.1%, need <10.0%)]   
[Waiting: 2749 MHz (variance: 26.8%, need <10.0%)]   
[Waiting: 2860 MHz (variance: 25.8%, need <10.0%)]   
[Waiting: 2875 MHz (variance: 16.7%, need <10.0%)]   
[Waiting: 2830 MHz (variance: 16.9%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 24.8%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 19.7%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 18.6%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 18.6%, need <10.0%)]   
[Waiting: 2690 MHz (variance: 16.4%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 16.6%, need <10.0%)]   
[Waiting: 2727 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 18.1%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 21.5%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 18.0%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 17.6%, need <10.0%)]   
[Waiting: 2440 MHz (variance: 16.6%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 16.5%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 16.2%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 11.6%, need <10.0%)]   
[CPU stable at 2425 MHz (66% of base, variance: 9.1%)]


================================================================================
  CORE OPERATIONS BENCHMARK (Round-Robin)
================================================================================

Comparing FastHashMap and StableHashMap (std::hash vs SplitMix64Hash)
vs std::unordered_map vs tsl::robin_map vs ankerl::unordered_dense vs absl::flat_hash_map vs boost::unordered_flat_map vs llvm::DenseMap

Methodology:
  - 3 warmup + 15 measured runs per test
  - Round-robin execution with randomized order per run
  - All libraries observe same distribution of machine states
  - Primary metric: median (ns/op)
  - FastHashMap SIMD backend: SSE2
  - StableHashMap SIMD backend: SSE2
  - FastHashMap policies: BackwardShift (BS), Tombstone (TS)
  - StableHashMap: Reference-stable (pointers valid across insert/reserve)

Cases (ns/op):
  Insert: insert N unique keys into empty map (after reserve)
  Find(hit): find N present keys
  Find(miss): find N absent keys
  Erase: erase 25% of present keys (random order)
  Churn: key replacement churn (erase one existing key, insert new key; size constant)

[2026-02-01 18:45:21] CPU: 2322 MHz (base: 3686)
N = 10000
[2026-02-01 18:45:21] Insert (amortized) CPU: 2322 MHz (base: 3686)
[Waiting: 2454 MHz (variance: 18.0%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 17.5%, need <10.0%)]   
[Waiting: 2558 MHz (variance: 10.1%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 10.2%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 10.2%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 10.3%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 11.9%, need <10.0%)]   
[CPU stable at 2410 MHz (65% of base, variance: 4.6%)]
[2026-02-01 18:45:22] Find(hit) CPU: 2432 MHz (base: 3686)
[Waiting: 2609 MHz (variance: 18.4%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 19.0%, need <10.0%)]   
[Waiting: 2418 MHz (variance: 12.2%, need <10.0%)]   
[CPU stable at 2381 MHz (65% of base, variance: 4.6%)]
[2026-02-01 18:45:23] Find(miss) CPU: 2432 MHz (base: 3686)
[Waiting: 3015 MHz (variance: 34.2%, need <10.0%)]   
[Waiting: 3022 MHz (variance: 34.1%, need <10.0%)]   
[Waiting: 2993 MHz (variance: 39.4%, need <10.0%)]   
[Waiting: 2830 MHz (variance: 44.3%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 48.0%, need <10.0%)]   
[Waiting: 2388 MHz (variance: 26.2%, need <10.0%)]   
[Waiting: 2366 MHz (variance: 18.7%, need <10.0%)]   
[Waiting: 2410 MHz (variance: 16.8%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 12.0%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 12.0%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 26.6%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 25.7%, need <10.0%)]   
[Waiting: 2727 MHz (variance: 25.7%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 24.3%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 25.1%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 19.1%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 13.7%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 13.6%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 17.7%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 18.0%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 18.0%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 13.4%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 13.0%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 13.0%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 12.7%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 12.8%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 12.8%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 16.1%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 19.0%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 19.4%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 19.2%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 26.7%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 22.0%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 22.5%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 21.1%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 24.1%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 13.1%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 13.3%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 13.4%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 13.4%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 13.4%, need <10.0%)]   
[CPU stable at 2447 MHz (66% of base, variance: 3.0%)]
[2026-02-01 18:45:26] Erase (25%) CPU: 2506 MHz (base: 3686)
[Waiting: 2565 MHz (variance: 18.7%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 19.0%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 19.1%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 19.6%, need <10.0%)]   
[CPU stable at 2373 MHz (64% of base, variance: 9.3%)]
[2026-02-01 18:45:27] Churn CPU: 2506 MHz (base: 3686)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]      6.44      4.69      4.33     27.64     19.29
    FastHashMap[BS]+SplitMix64      3.98      2.54      2.79     21.16     15.26
               FastHashMap[TS]      6.67      4.57      4.40      4.68     19.24
    FastHashMap[TS]+SplitMix64      4.15      2.47      2.78      2.80     14.60
                 StableHashMap     25.98      6.59      4.75     18.00     32.25
      StableHashMap+SplitMix64     24.58      3.37      2.06     13.56     27.11
     StableHashMap[Block]+SM64      5.80      3.11      2.05      4.12     15.12
            std::unordered_map     32.28      6.83      8.55     22.12     27.23
                tsl::robin_map      7.26      5.44      6.10      8.64     10.06
       ankerl::unordered_dense     18.36      7.48      2.87     17.88     14.43
           absl::flat_hash_map     11.64      2.94      3.88      6.72     11.02
           absl::node_hash_map     29.83      3.30      3.85     16.48     22.90
     boost::unordered_flat_map      6.60      1.94      1.43      2.48      2.87
     boost::unordered_node_map     24.33      2.22      1.47     12.72     17.23
                llvm::DenseMap      7.06      4.78      9.22      5.36     20.00

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[BS]+SplitMix64 (8.11x), FastHashMap[TS]+SplitMix64 (7.78x), FastHashMap[BS] (5.01x)
    Top 3 Find: boost::unordered_flat_map (3.52x), FastHashMap[TS]+SplitMix64 (2.77x), FastHashMap[BS]+SplitMix64 (2.69x)
    Top 3 Miss: boost::unordered_flat_map (5.98x), FastHashMap[TS]+SplitMix64 (3.08x), FastHashMap[BS]+SplitMix64 (3.06x)
    Top 3 Erase: boost::unordered_flat_map (8.92x), FastHashMap[TS]+SplitMix64 (7.90x), FastHashMap[TS] (4.73x)
    Top 3 Churn: boost::unordered_flat_map (9.49x), tsl::robin_map (2.71x), absl::flat_hash_map (2.47x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (5.57x), boost::unordered_node_map (1.33x), StableHashMap+SplitMix64 (1.31x)
    Top 3 Find: boost::unordered_node_map (3.08x), StableHashMap[Block]+SM64 (2.20x), absl::node_hash_map (2.07x)
    Top 3 Miss: boost::unordered_node_map (5.82x), StableHashMap[Block]+SM64 (4.17x), StableHashMap+SplitMix64 (4.15x)
    Top 3 Erase: StableHashMap[Block]+SM64 (5.37x), boost::unordered_node_map (1.74x), StableHashMap+SplitMix64 (1.63x)
    Top 3 Churn: StableHashMap[Block]+SM64 (1.80x), boost::unordered_node_map (1.58x), absl::node_hash_map (1.19x)

  All Results:
    FastHashMap[BS]                 5.01x insert,  1.46x find,  1.97x miss,  0.80x erase,  1.41x churn
    FastHashMap[BS]+SplitMix64      8.11x insert,  2.69x find,  3.06x miss,  1.05x erase,  1.79x churn
    FastHashMap[TS]                 4.84x insert,  1.49x find,  1.94x miss,  4.73x erase,  1.42x churn
    FastHashMap[TS]+SplitMix64      7.78x insert,  2.77x find,  3.08x miss,  7.90x erase,  1.87x churn
    StableHashMap                   1.24x insert,  1.04x find,  1.80x miss,  1.23x erase,  0.84x churn
    StableHashMap+SplitMix64        1.31x insert,  2.03x find,  4.15x miss,  1.63x erase,  1.00x churn
    StableHashMap[Block]+SM64       5.57x insert,  2.20x find,  4.17x miss,  5.37x erase,  1.80x churn
    tsl::robin_map                  4.45x insert,  1.26x find,  1.40x miss,  2.56x erase,  2.71x churn
    ankerl::unordered_dense         1.76x insert,  0.91x find,  2.98x miss,  1.24x erase,  1.89x churn
    absl::flat_hash_map             2.77x insert,  2.32x find,  2.20x miss,  3.29x erase,  2.47x churn
    absl::node_hash_map             1.08x insert,  2.07x find,  2.22x miss,  1.34x erase,  1.19x churn
    boost::unordered_flat_map       4.89x insert,  3.52x find,  5.98x miss,  8.92x erase,  9.49x churn
    boost::unordered_node_map       1.33x insert,  3.08x find,  5.82x miss,  1.74x erase,  1.58x churn
    llvm::DenseMap                  4.57x insert,  1.43x find,  0.93x miss,  4.13x erase,  1.36x churn
  [NOTE] FastHashMap[TS]: high variance (stddev 7.86 > median 6.67) - system noise or memory pressure
  [NOTE] FastHashMap[TS]+SplitMix64: high variance (stddev 8.12 > median 4.15) - system noise or memory pressure
  [NOTE] StableHashMap: high variance (stddev 9.68 > median 6.59) - system noise or memory pressure
  [NOTE] tsl::robin_map: high variance (stddev 7.46 > median 5.44) - system noise or memory pressure
  [NOTE] absl::node_hash_map: high variance (stddev 6.62 > median 3.85) - system noise or memory pressure
  [NOTE] llvm::DenseMap: high variance (stddev 5.65 > median 5.36) - system noise or memory pressure

[Cooling: before next size][Waiting: 2432 MHz (variance: 15.2%, need <10.0%)]   
[Waiting: 2359 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2329 MHz (variance: 12.7%, need <10.0%)]   
[Waiting: 2418 MHz (variance: 22.9%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 22.4%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 22.0%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 14.5%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 16.1%, need <10.0%)]   
[CPU stable at 2425 MHz (66% of base, variance: 9.1%)]
 [Ready: 2580 MHz]
[2026-02-01 18:45:29] CPU: 2580 MHz (base: 3686)
N = 100000
[2026-02-01 18:45:29] Insert (amortized) CPU: 2580 MHz (base: 3686)
[Waiting: 2786 MHz (variance: 13.2%, need <10.0%)]   
[Waiting: 2793 MHz (variance: 11.9%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 19.3%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 19.7%, need <10.0%)]   
[Waiting: 2558 MHz (variance: 17.3%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 11.8%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 14.7%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 15.4%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 15.4%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 15.4%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 15.4%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 15.7%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 14.9%, need <10.0%)]   
[Waiting: 2440 MHz (variance: 15.1%, need <10.0%)]   
[Waiting: 2418 MHz (variance: 15.2%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 25.4%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 23.4%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 22.7%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 19.6%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 15.2%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 12.9%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 12.9%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 13.0%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 13.1%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 13.0%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 16.0%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 16.1%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 16.1%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 19.6%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 19.2%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 13.6%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 13.4%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 13.3%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 35.2%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 35.7%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 30.0%, need <10.0%)]   
[Waiting: 2779 MHz (variance: 29.2%, need <10.0%)]   
[Waiting: 2845 MHz (variance: 27.2%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 19.0%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 21.8%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 22.7%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 17.4%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 17.4%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 18.9%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 11.7%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 11.7%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 11.9%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 10.1%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 12.9%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 13.0%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 16.8%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 21.2%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 21.7%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 21.7%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 21.7%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 17.6%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 15.7%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 14.5%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 14.3%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 19.8%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 19.9%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 15.5%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 15.6%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 15.7%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 10.1%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 16.9%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 14.0%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 14.0%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 14.0%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 14.1%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 14.6%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 34.7%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 34.5%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 35.1%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 35.1%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 28.9%, need <10.0%)]   
[CPU stable at 2506 MHz (68% of base, variance: 8.8%)]
[2026-02-01 18:45:35] Find(hit) CPU: 2543 MHz (base: 3686)
[Waiting: 2705 MHz (variance: 46.3%, need <10.0%)]   
[Waiting: 2779 MHz (variance: 38.5%, need <10.0%)]   
[Waiting: 2845 MHz (variance: 36.3%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 12.3%, need <10.0%)]   
[Waiting: 2712 MHz (variance: 12.2%, need <10.0%)]   
[Waiting: 2698 MHz (variance: 15.0%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 16.7%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 20.2%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 11.9%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 11.8%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 12.6%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 11.2%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 11.1%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 11.0%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 26.0%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 26.7%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 26.6%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 25.2%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 30.5%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 22.2%, need <10.0%)]   
[Waiting: 2727 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2845 MHz (variance: 16.8%, need <10.0%)]   
[Waiting: 2911 MHz (variance: 16.5%, need <10.0%)]   
[Waiting: 2845 MHz (variance: 19.4%, need <10.0%)]   
[Waiting: 2808 MHz (variance: 19.7%, need <10.0%)]   
[Waiting: 2764 MHz (variance: 25.3%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 25.3%, need <10.0%)]   
[CPU stable at 2476 MHz (67% of base, variance: 7.4%)]
[2026-02-01 18:45:38] Find(miss) CPU: 2543 MHz (base: 3686)
[Waiting: 2499 MHz (variance: 10.3%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 21.4%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 21.0%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 20.4%, need <10.0%)]   
[Waiting: 2698 MHz (variance: 21.9%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 22.1%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 19.6%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 16.9%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 18.0%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 14.1%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 10.3%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 18.1%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 15.2%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 26.3%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 32.0%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 31.7%, need <10.0%)]   
[Waiting: 2808 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2764 MHz (variance: 25.3%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 26.2%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 19.9%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 20.1%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 11.8%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 24.9%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 24.9%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 26.4%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 28.1%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 27.9%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 20.4%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 20.4%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 20.1%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 19.8%, need <10.0%)]   
[Waiting: 2690 MHz (variance: 16.4%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 14.2%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 14.2%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 14.5%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 14.8%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 10.3%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 10.3%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 10.4%, need <10.0%)]   
[CPU stable at 2447 MHz (66% of base, variance: 4.5%)]
[2026-02-01 18:45:43] Erase (25%) CPU: 2469 MHz (base: 3686)
[Waiting: 2676 MHz (variance: 16.5%, need <10.0%)]   
[Waiting: 2690 MHz (variance: 13.7%, need <10.0%)]   
[CPU stable at 2587 MHz (70% of base, variance: 7.1%)]
[2026-02-01 18:45:44] Churn CPU: 2653 MHz (base: 3686)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]      7.82      6.67      8.01     40.48     30.68
    FastHashMap[BS]+SplitMix64      5.18      4.28      5.83     31.80     25.82
               FastHashMap[TS]      7.61      5.60      7.64      5.26     19.08
    FastHashMap[TS]+SplitMix64      4.86      3.38      5.63      3.30     13.82
                 StableHashMap     29.33     11.35      8.76     28.82     54.35
      StableHashMap+SplitMix64     27.65      7.34      4.97     22.92     48.58
     StableHashMap[Block]+SM64      9.33      5.64      4.74      7.13     14.81
            std::unordered_map     41.33     10.15     11.53     34.71     57.87
                tsl::robin_map     14.11      7.88      9.62     12.32     13.53
       ankerl::unordered_dense     24.15     11.08      6.04     30.24     26.20
           absl::flat_hash_map     12.65      3.87      6.52     11.92     19.66
           absl::node_hash_map     33.55      5.00      6.50     26.19     53.15
     boost::unordered_flat_map      7.38      2.96      2.44      3.55     10.61
     boost::unordered_node_map     29.52      3.99      2.63     20.21     43.26
                llvm::DenseMap      8.28      3.85      7.12      3.79     10.19

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[TS]+SplitMix64 (8.51x), FastHashMap[BS]+SplitMix64 (7.98x), boost::unordered_flat_map (5.60x)
    Top 3 Find: boost::unordered_flat_map (3.43x), FastHashMap[TS]+SplitMix64 (3.00x), llvm::DenseMap (2.64x)
    Top 3 Miss: boost::unordered_flat_map (4.72x), FastHashMap[TS]+SplitMix64 (2.05x), FastHashMap[BS]+SplitMix64 (1.98x)
    Top 3 Erase: FastHashMap[TS]+SplitMix64 (10.50x), boost::unordered_flat_map (9.78x), llvm::DenseMap (9.15x)
    Top 3 Churn: llvm::DenseMap (5.68x), boost::unordered_flat_map (5.45x), tsl::robin_map (4.28x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (4.43x), StableHashMap+SplitMix64 (1.49x), StableHashMap (1.41x)
    Top 3 Find: boost::unordered_node_map (2.54x), absl::node_hash_map (2.03x), StableHashMap[Block]+SM64 (1.80x)
    Top 3 Miss: boost::unordered_node_map (4.39x), StableHashMap[Block]+SM64 (2.43x), StableHashMap+SplitMix64 (2.32x)
    Top 3 Erase: StableHashMap[Block]+SM64 (4.87x), boost::unordered_node_map (1.72x), StableHashMap+SplitMix64 (1.51x)
    Top 3 Churn: StableHashMap[Block]+SM64 (3.91x), boost::unordered_node_map (1.34x), StableHashMap+SplitMix64 (1.19x)

  All Results:
    FastHashMap[BS]                 5.28x insert,  1.52x find,  1.44x miss,  0.86x erase,  1.89x churn
    FastHashMap[BS]+SplitMix64      7.98x insert,  2.37x find,  1.98x miss,  1.09x erase,  2.24x churn
    FastHashMap[TS]                 5.43x insert,  1.81x find,  1.51x miss,  6.60x erase,  3.03x churn
    FastHashMap[TS]+SplitMix64      8.51x insert,  3.00x find,  2.05x miss, 10.50x erase,  4.19x churn
    StableHashMap                   1.41x insert,  0.89x find,  1.32x miss,  1.20x erase,  1.06x churn
    StableHashMap+SplitMix64        1.49x insert,  1.38x find,  2.32x miss,  1.51x erase,  1.19x churn
    StableHashMap[Block]+SM64       4.43x insert,  1.80x find,  2.43x miss,  4.87x erase,  3.91x churn
    tsl::robin_map                  2.93x insert,  1.29x find,  1.20x miss,  2.82x erase,  4.28x churn
    ankerl::unordered_dense         1.71x insert,  0.92x find,  1.91x miss,  1.15x erase,  2.21x churn
    absl::flat_hash_map             3.27x insert,  2.62x find,  1.77x miss,  2.91x erase,  2.94x churn
    absl::node_hash_map             1.23x insert,  2.03x find,  1.77x miss,  1.33x erase,  1.09x churn
    boost::unordered_flat_map       5.60x insert,  3.43x find,  4.72x miss,  9.78x erase,  5.45x churn
    boost::unordered_node_map       1.40x insert,  2.54x find,  4.39x miss,  1.72x erase,  1.34x churn
    llvm::DenseMap                  4.99x insert,  2.64x find,  1.62x miss,  9.15x erase,  5.68x churn

[Cooling: before next size][Waiting: 2440 MHz (variance: 10.6%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 14.7%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 15.0%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 14.5%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 14.1%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 14.0%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 14.4%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 12.9%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 16.3%, need <10.0%)]   
[CPU stable at 2432 MHz (66% of base, variance: 9.1%)]
 [Ready: 2580 MHz]
[2026-02-01 18:45:49] CPU: 2580 MHz (base: 3686)
N = 1000000
[2026-02-01 18:45:49] Insert (amortized) CPU: 2395 MHz (base: 3686)
[Waiting: 2403 MHz (variance: 12.3%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2558 MHz (variance: 13.0%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 14.6%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 14.6%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 14.5%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 19.0%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 14.9%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 14.8%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 14.9%, need <10.0%)]   
[Waiting: 2410 MHz (variance: 10.7%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 13.6%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 19.1%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 19.1%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 19.5%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 19.5%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 16.0%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 11.8%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 11.5%, need <10.0%)]   
[CPU stable at 2661 MHz (72% of base, variance: 9.7%)]
[2026-02-01 18:46:04] Find(hit) CPU: 2801 MHz (base: 3686)
[Waiting: 2034 MHz (variance: 12.7%, need <10.0%)]   
[Waiting: 2086 MHz (variance: 14.1%, need <10.0%)]   
[Waiting: 2307 MHz (variance: 51.1%, need <10.0%)]   
[Waiting: 2366 MHz (variance: 48.3%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 35.9%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 33.2%, need <10.0%)]   
[Waiting: 2749 MHz (variance: 32.2%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 28.7%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 28.6%, need <10.0%)]   
[Waiting: 2440 MHz (variance: 18.1%, need <10.0%)]   
[Waiting: 2336 MHz (variance: 22.1%, need <10.0%)]   
[Waiting: 2344 MHz (variance: 20.4%, need <10.0%)]   
[Waiting: 2366 MHz (variance: 20.2%, need <10.0%)]   
[Waiting: 2410 MHz (variance: 15.3%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 10.4%, need <10.0%)]   
[CPU stable at 2462 MHz (67% of base, variance: 3.0%)]
[2026-02-01 18:46:24] Find(miss) CPU: 2469 MHz (base: 3686)
[Waiting: 2307 MHz (variance: 38.3%, need <10.0%)]   
[Waiting: 2381 MHz (variance: 52.6%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 49.7%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 46.3%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 46.5%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 38.8%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 22.6%, need <10.0%)]   
[Waiting: 2336 MHz (variance: 14.2%, need <10.0%)]   
[CPU stable at 2270 MHz (62% of base, variance: 3.2%)]
[2026-02-01 18:46:41] Erase (25%) CPU: 2322 MHz (base: 3686)
[Waiting: 2359 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2395 MHz (variance: 10.8%, need <10.0%)]   
[Waiting: 2395 MHz (variance: 10.8%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 25.3%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 45.5%, need <10.0%)]   
[Waiting: 2816 MHz (variance: 43.2%, need <10.0%)]   
[Waiting: 2978 MHz (variance: 33.4%, need <10.0%)]   
[Waiting: 3000 MHz (variance: 29.5%, need <10.0%)]   
[Waiting: 2934 MHz (variance: 31.4%, need <10.0%)]   
[Waiting: 2779 MHz (variance: 23.9%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 19.1%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 11.1%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 11.1%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 11.1%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 14.1%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 12.9%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 11.9%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 15.2%, need <10.0%)]   
[Waiting: 2381 MHz (variance: 15.5%, need <10.0%)]   
[Waiting: 2344 MHz (variance: 15.7%, need <10.0%)]   
[Waiting: 2329 MHz (variance: 12.7%, need <10.0%)]   
[Waiting: 2410 MHz (variance: 19.9%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 19.8%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 19.5%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 16.3%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 16.1%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 13.6%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 24.2%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 24.2%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 25.8%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 25.6%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 25.2%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 15.9%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 32.7%, need <10.0%)]   
[Waiting: 2816 MHz (variance: 23.6%, need <10.0%)]   
[Waiting: 2793 MHz (variance: 23.7%, need <10.0%)]   
[Waiting: 2875 MHz (variance: 23.1%, need <10.0%)]   
[Waiting: 2845 MHz (variance: 28.5%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 20.6%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 20.8%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 21.1%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 20.9%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 20.3%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 20.5%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 20.7%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 37.6%, need <10.0%)]   
[Waiting: 2904 MHz (variance: 40.6%, need <10.0%)]   
[Waiting: 3022 MHz (variance: 39.0%, need <10.0%)]   
[Waiting: 3022 MHz (variance: 39.0%, need <10.0%)]   
[Waiting: 2978 MHz (variance: 39.6%, need <10.0%)]   
[Waiting: 2852 MHz (variance: 41.3%, need <10.0%)]   
[Waiting: 2690 MHz (variance: 37.0%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 15.3%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 16.8%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 13.3%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 16.2%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 16.3%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 14.5%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 16.0%, need <10.0%)]   
[Waiting: 2565 MHz (variance: 15.8%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 17.0%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 13.7%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 18.1%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 17.9%, need <10.0%)]   
[Waiting: 2712 MHz (variance: 17.7%, need <10.0%)]   
[Waiting: 2712 MHz (variance: 17.7%, need <10.0%)]   
[Waiting: 2690 MHz (variance: 17.8%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 12.2%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 16.7%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 17.0%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 24.1%, need <10.0%)]   
[Waiting: 2381 MHz (variance: 18.6%, need <10.0%)]   
[Waiting: 2344 MHz (variance: 15.7%, need <10.0%)]   
[Waiting: 2381 MHz (variance: 21.7%, need <10.0%)]   
[Waiting: 2359 MHz (variance: 21.9%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 42.6%, need <10.0%)]   
[Waiting: 2793 MHz (variance: 40.9%, need <10.0%)]   
[Waiting: 2867 MHz (variance: 39.8%, need <10.0%)]   
[Waiting: 2919 MHz (variance: 39.1%, need <10.0%)]   
[Waiting: 2934 MHz (variance: 36.4%, need <10.0%)]   
[Waiting: 2771 MHz (variance: 22.6%, need <10.0%)]   
[Waiting: 2742 MHz (variance: 17.5%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 17.6%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 17.5%, need <10.0%)]   
[Waiting: 2749 MHz (variance: 14.7%, need <10.0%)]   
[Waiting: 2808 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2808 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2727 MHz (variance: 23.0%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 29.4%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 29.8%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 28.3%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 18.2%, need <10.0%)]   
[Waiting: 2403 MHz (variance: 18.4%, need <10.0%)]   
[Waiting: 2395 MHz (variance: 20.0%, need <10.0%)]   
[Waiting: 2373 MHz (variance: 20.2%, need <10.0%)]   
[Waiting: 2336 MHz (variance: 20.5%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 49.0%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 46.1%, need <10.0%)]   
[Waiting: 2727 MHz (variance: 43.2%, need <10.0%)]   
[Waiting: 2779 MHz (variance: 42.4%, need <10.0%)]   
[Waiting: 2816 MHz (variance: 35.3%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 23.9%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 22.8%, need <10.0%)]   
[Waiting: 2727 MHz (variance: 28.4%, need <10.0%)]   
[Waiting: 2771 MHz (variance: 26.6%, need <10.0%)]   
[Waiting: 2757 MHz (variance: 29.4%, need <10.0%)]   
[Waiting: 2793 MHz (variance: 29.0%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 33.3%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 14.8%, need <10.0%)]   
[Waiting: 2440 MHz (variance: 15.1%, need <10.0%)]   
[Waiting: 2440 MHz (variance: 15.1%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 22.3%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 18.5%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 17.9%, need <10.0%)]   
[Waiting: 2698 MHz (variance: 17.8%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 14.9%, need <10.0%)]   
[Waiting: 2823 MHz (variance: 32.6%, need <10.0%)]   
[Waiting: 2808 MHz (variance: 32.8%, need <10.0%)]   
[Waiting: 2786 MHz (variance: 33.1%, need <10.0%)]   
[Waiting: 2779 MHz (variance: 34.5%, need <10.0%)]   
[Waiting: 2830 MHz (variance: 33.9%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 13.9%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 12.6%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 12.9%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 15.7%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 15.9%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 16.0%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 18.1%, need <10.0%)]   
[Waiting: 2410 MHz (variance: 13.8%, need <10.0%)]   
[Waiting: 2410 MHz (variance: 13.8%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 12.2%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 10.6%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 10.6%, need <10.0%)]   
[Waiting: 2535 MHz (variance: 30.5%, need <10.0%)]   
[Waiting: 2617 MHz (variance: 29.6%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 29.5%, need <10.0%)]   
[Waiting: 2712 MHz (variance: 27.2%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 27.2%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2528 MHz (variance: 19.0%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 16.5%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 16.3%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 40.7%, need <10.0%)]   
[Waiting: 2867 MHz (variance: 38.6%, need <10.0%)]   
[Waiting: 2970 MHz (variance: 37.2%, need <10.0%)]   
[Waiting: 3081 MHz (variance: 32.3%, need <10.0%)]   
[Waiting: 3147 MHz (variance: 21.1%, need <10.0%)]   
[Waiting: 2978 MHz (variance: 27.2%, need <10.0%)]   
[Waiting: 2816 MHz (variance: 11.8%, need <10.0%)]   
[Waiting: 2830 MHz (variance: 14.3%, need <10.0%)]   
[Waiting: 2779 MHz (variance: 14.6%, need <10.0%)]   
[Waiting: 2749 MHz (variance: 14.7%, need <10.0%)]   
[Waiting: 2705 MHz (variance: 23.2%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 22.9%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 13.9%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 14.2%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 14.2%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 14.2%, need <10.0%)]   
[Waiting: 2558 MHz (variance: 14.4%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 14.3%, need <10.0%)]   
[Waiting: 2506 MHz (variance: 19.1%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 18.8%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 21.0%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 20.9%, need <10.0%)]   
[Waiting: 2771 MHz (variance: 29.3%, need <10.0%)]   
[Waiting: 3044 MHz (variance: 33.9%, need <10.0%)]   
[Waiting: 3177 MHz (variance: 25.5%, need <10.0%)]   
[Waiting: 3125 MHz (variance: 34.2%, need <10.0%)]   
[Waiting: 3110 MHz (variance: 34.4%, need <10.0%)]   
[Waiting: 3103 MHz (variance: 34.4%, need <10.0%)]   
[Waiting: 2970 MHz (variance: 23.6%, need <10.0%)]   
[Waiting: 2875 MHz (variance: 16.7%, need <10.0%)]   
[Waiting: 2889 MHz (variance: 14.0%, need <10.0%)]   
[Waiting: 2941 MHz (variance: 13.8%, need <10.0%)]   
[Waiting: 2919 MHz (variance: 12.6%, need <10.0%)]   
[Waiting: 2934 MHz (variance: 13.8%, need <10.0%)]   
[Waiting: 2897 MHz (variance: 15.3%, need <10.0%)]   
[Waiting: 2911 MHz (variance: 15.2%, need <10.0%)]   
[Waiting: 2823 MHz (variance: 17.0%, need <10.0%)]   
[Waiting: 2771 MHz (variance: 17.3%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 11.0%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 11.1%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 11.0%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 12.6%, need <10.0%)]   
[Waiting: 2631 MHz (variance: 14.0%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 14.0%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 17.6%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 17.7%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 34.3%, need <10.0%)]   
[Waiting: 2646 MHz (variance: 33.4%, need <10.0%)]   
[Waiting: 2712 MHz (variance: 32.6%, need <10.0%)]   
[Waiting: 2786 MHz (variance: 30.4%, need <10.0%)]   
[Waiting: 2860 MHz (variance: 19.3%, need <10.0%)]   
[Waiting: 2720 MHz (variance: 12.2%, need <10.0%)]   
[Waiting: 2757 MHz (variance: 14.7%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 19.2%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 19.2%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 19.3%, need <10.0%)]   
[Waiting: 2639 MHz (variance: 22.3%, need <10.0%)]   
[Waiting: 2572 MHz (variance: 20.1%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 20.0%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 14.6%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 11.6%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 13.3%, need <10.0%)]   
[Waiting: 2484 MHz (variance: 13.4%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 13.3%, need <10.0%)]   
[Waiting: 2513 MHz (variance: 17.6%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 17.1%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 17.0%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 13.7%, need <10.0%)]   
[Waiting: 2764 MHz (variance: 14.7%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 14.8%, need <10.0%)]   
[Waiting: 2668 MHz (variance: 15.2%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 18.1%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 21.7%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 14.9%, need <10.0%)]   
[Waiting: 2388 MHz (variance: 12.3%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 12.1%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 15.0%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 15.0%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 19.2%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 19.7%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 19.8%, need <10.0%)]   
[Waiting: 2683 MHz (variance: 24.7%, need <10.0%)]   
[Waiting: 2801 MHz (variance: 21.1%, need <10.0%)]   
[Waiting: 2808 MHz (variance: 21.0%, need <10.0%)]   
[Waiting: 2764 MHz (variance: 21.3%, need <10.0%)]   
[Waiting: 2786 MHz (variance: 17.2%, need <10.0%)]   
[Waiting: 2749 MHz (variance: 14.7%, need <10.0%)]   
[Waiting: 2742 MHz (variance: 13.4%, need <10.0%)]   
[Waiting: 2764 MHz (variance: 13.3%, need <10.0%)]   
[Waiting: 2801 MHz (variance: 13.2%, need <10.0%)]   
[Waiting: 2727 MHz (variance: 27.0%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 28.3%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 28.1%, need <10.0%)]   
[Waiting: 2388 MHz (variance: 27.8%, need <10.0%)]   
[CPU stable at 2410 MHz (65% of base, variance: 6.1%)]
[2026-02-01 18:47:11] Churn CPU: 2506 MHz (base: 3686)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     14.51     28.60      7.55     50.81     39.93
    FastHashMap[BS]+SplitMix64     11.11     20.17      4.66     38.55     30.39
               FastHashMap[TS]     13.88     23.78      6.44     20.31     25.40
    FastHashMap[TS]+SplitMix64     10.06     16.07      4.23     13.22     16.38
                 StableHashMap     39.58     25.46      7.00    115.07    135.93
      StableHashMap+SplitMix64     36.93     15.87      3.79    103.11    134.84
     StableHashMap[Block]+SM64     16.03     10.65      3.39     25.06     32.14
            std::unordered_map     91.40     31.06     37.36    142.25    233.76
                tsl::robin_map     28.20     23.11     21.81     31.75     34.32
       ankerl::unordered_dense     39.52      9.09      5.77     36.60     25.11
           absl::flat_hash_map     23.95     19.32      5.47     22.72     28.79
           absl::node_hash_map     44.47     17.31      9.56    107.96    140.17
     boost::unordered_flat_map     12.35      9.82      2.67     10.58     10.71
     boost::unordered_node_map     38.31     11.54      4.90     95.42    114.21
                llvm::DenseMap     18.95     11.34     15.63      9.80     21.48

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[TS]+SplitMix64 (9.09x), FastHashMap[BS]+SplitMix64 (8.22x), boost::unordered_flat_map (7.40x)
    Top 3 Find: ankerl::unordered_dense (3.42x), boost::unordered_flat_map (3.16x), llvm::DenseMap (2.74x)
    Top 3 Miss: boost::unordered_flat_map (14.00x), FastHashMap[TS]+SplitMix64 (8.82x), FastHashMap[BS]+SplitMix64 (8.02x)
    Top 3 Erase: llvm::DenseMap (14.51x), boost::unordered_flat_map (13.45x), FastHashMap[TS]+SplitMix64 (10.76x)
    Top 3 Churn: boost::unordered_flat_map (21.83x), FastHashMap[TS]+SplitMix64 (14.27x), llvm::DenseMap (10.88x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (5.70x), StableHashMap+SplitMix64 (2.48x), boost::unordered_node_map (2.39x)
    Top 3 Find: StableHashMap[Block]+SM64 (2.92x), boost::unordered_node_map (2.69x), StableHashMap+SplitMix64 (1.96x)
    Top 3 Miss: StableHashMap[Block]+SM64 (11.01x), StableHashMap+SplitMix64 (9.85x), boost::unordered_node_map (7.62x)
    Top 3 Erase: StableHashMap[Block]+SM64 (5.68x), boost::unordered_node_map (1.49x), StableHashMap+SplitMix64 (1.38x)
    Top 3 Churn: StableHashMap[Block]+SM64 (7.27x), boost::unordered_node_map (2.05x), StableHashMap+SplitMix64 (1.73x)

  All Results:
    FastHashMap[BS]                 6.30x insert,  1.09x find,  4.95x miss,  2.80x erase,  5.85x churn
    FastHashMap[BS]+SplitMix64      8.22x insert,  1.54x find,  8.02x miss,  3.69x erase,  7.69x churn
    FastHashMap[TS]                 6.58x insert,  1.31x find,  5.80x miss,  7.00x erase,  9.20x churn
    FastHashMap[TS]+SplitMix64      9.09x insert,  1.93x find,  8.82x miss, 10.76x erase, 14.27x churn
    StableHashMap                   2.31x insert,  1.22x find,  5.34x miss,  1.24x erase,  1.72x churn
    StableHashMap+SplitMix64        2.48x insert,  1.96x find,  9.85x miss,  1.38x erase,  1.73x churn
    StableHashMap[Block]+SM64       5.70x insert,  2.92x find, 11.01x miss,  5.68x erase,  7.27x churn
    tsl::robin_map                  3.24x insert,  1.34x find,  1.71x miss,  4.48x erase,  6.81x churn
    ankerl::unordered_dense         2.31x insert,  3.42x find,  6.47x miss,  3.89x erase,  9.31x churn
    absl::flat_hash_map             3.82x insert,  1.61x find,  6.83x miss,  6.26x erase,  8.12x churn
    absl::node_hash_map             2.06x insert,  1.79x find,  3.91x miss,  1.32x erase,  1.67x churn
    boost::unordered_flat_map       7.40x insert,  3.16x find, 14.00x miss, 13.45x erase, 21.83x churn
    boost::unordered_node_map       2.39x insert,  2.69x find,  7.62x miss,  1.49x erase,  2.05x churn
    llvm::DenseMap                  4.82x insert,  2.74x find,  2.39x miss, 14.51x erase, 10.88x churn
  [NOTE] std::unordered_map: median 142.25 ns (node-based pointer chasing / cache locality effects)
  [NOTE] std::unordered_map: median 233.76 ns (node-based pointer chasing / cache locality effects)

--- Detailed Statistics for FastHashMap[BS] at N=1000000 ---
  Insert (amortized): median=   14.51 mean=   14.56 +/-  0.35 CI95(mean)=[14.39,14.74] min=14.07 max=15.39
     Find(hit): median=   28.60 mean=   28.64 +/-  0.62 CI95(mean)=[28.33,28.95] min=27.53 max=29.73
    Find(miss): median=    7.55 mean=    7.51 +/-  0.66 CI95(mean)=[7.17,7.84] min=6.70 max=8.90
   Erase (25%): median=   50.81 mean=   51.47 +/-  1.85 CI95(mean)=[50.53,52.40] min=49.11 max=54.66
         Churn: median=   39.93 mean=   39.94 +/-  0.63 CI95(mean)=[39.62,40.26] min=39.11 max=41.46

--- Detailed Statistics for FastHashMap[BS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   11.11 mean=   11.13 +/-  0.38 CI95(mean)=[10.94,11.33] min=10.37 max=11.79
     Find(hit): median=   20.17 mean=   20.42 +/-  0.69 CI95(mean)=[20.07,20.77] min=19.90 max=22.17
    Find(miss): median=    4.66 mean=    5.02 +/-  0.65 CI95(mean)=[4.69,5.34] min=4.44 max=6.52
   Erase (25%): median=   38.55 mean=   38.19 +/-  2.28 CI95(mean)=[37.04,39.34] min=35.64 max=42.67
         Churn: median=   30.39 mean=   30.47 +/-  0.56 CI95(mean)=[30.19,30.76] min=29.48 max=31.73

--- Detailed Statistics for FastHashMap[TS] at N=1000000 ---
  Insert (amortized): median=   13.88 mean=   13.85 +/-  0.29 CI95(mean)=[13.70,14.00] min=13.08 max=14.29
     Find(hit): median=   23.78 mean=   23.75 +/-  0.85 CI95(mean)=[23.32,24.19] min=22.05 max=25.52
    Find(miss): median=    6.44 mean=    6.42 +/-  0.55 CI95(mean)=[6.14,6.69] min=5.62 max=7.60
   Erase (25%): median=   20.31 mean=   19.91 +/-  2.25 CI95(mean)=[18.77,21.05] min=16.91 max=23.29
         Churn: median=   25.40 mean=   25.30 +/-  1.60 CI95(mean)=[24.49,26.10] min=22.84 max=27.87

--- Detailed Statistics for FastHashMap[TS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   10.06 mean=   10.20 +/-  0.45 CI95(mean)=[9.97,10.42] min=9.59 max=11.07
     Find(hit): median=   16.07 mean=   16.27 +/-  0.71 CI95(mean)=[15.91,16.63] min=15.23 max=17.33
    Find(miss): median=    4.23 mean=    4.16 +/-  0.48 CI95(mean)=[3.92,4.41] min=3.59 max=5.13
   Erase (25%): median=   13.22 mean=   13.25 +/-  1.47 CI95(mean)=[12.50,13.99] min=11.67 max=16.45
         Churn: median=   16.38 mean=   16.66 +/-  1.04 CI95(mean)=[16.13,17.19] min=14.68 max=18.42

--- Detailed Statistics for StableHashMap at N=1000000 ---
  Insert (amortized): median=   39.58 mean=   39.99 +/-  2.44 CI95(mean)=[38.75,41.22] min=37.18 max=45.95
     Find(hit): median=   25.46 mean=   25.58 +/-  1.86 CI95(mean)=[24.64,26.52] min=22.80 max=29.81
    Find(miss): median=    7.00 mean=    7.10 +/-  0.55 CI95(mean)=[6.82,7.38] min=6.45 max=8.03
   Erase (25%): median=  115.07 mean=  115.82 +/-  5.60 CI95(mean)=[112.99,118.66] min=104.64 max=123.97
         Churn: median=  135.93 mean=  136.95 +/-  3.48 CI95(mean)=[135.19,138.71] min=132.53 max=145.75

--- Detailed Statistics for StableHashMap+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   36.93 mean=   36.88 +/-  1.39 CI95(mean)=[36.18,37.59] min=34.93 max=39.41
     Find(hit): median=   15.87 mean=   16.01 +/-  1.22 CI95(mean)=[15.40,16.63] min=14.53 max=18.87
    Find(miss): median=    3.79 mean=    3.85 +/-  0.27 CI95(mean)=[3.71,3.99] min=3.50 max=4.36
   Erase (25%): median=  103.11 mean=  103.82 +/-  4.12 CI95(mean)=[101.74,105.91] min=97.36 max=114.90
         Churn: median=  134.84 mean=  134.70 +/-  5.98 CI95(mean)=[131.68,137.73] min=126.70 max=146.76

--- Detailed Statistics for StableHashMap[Block]+SM64 at N=1000000 ---
  Insert (amortized): median=   16.03 mean=   16.21 +/-  0.75 CI95(mean)=[15.84,16.59] min=15.43 max=17.76
     Find(hit): median=   10.65 mean=   10.83 +/-  0.96 CI95(mean)=[10.34,11.31] min=9.65 max=13.01
    Find(miss): median=    3.39 mean=    3.53 +/-  0.28 CI95(mean)=[3.38,3.67] min=3.16 max=4.09
   Erase (25%): median=   25.06 mean=   26.90 +/-  3.05 CI95(mean)=[25.35,28.44] min=24.14 max=32.45
         Churn: median=   32.14 mean=   33.13 +/-  2.17 CI95(mean)=[32.03,34.22] min=31.30 max=38.38

[Cooling: before miss diagnostics][Waiting: 2388 MHz (variance: 47.8%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 46.7%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 41.3%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 37.1%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 33.9%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 10.6%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 13.4%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 13.5%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 13.7%, need <10.0%)]   
[Waiting: 2432 MHz (variance: 13.6%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 30.7%, need <10.0%)]   
[Waiting: 2580 MHz (variance: 30.0%, need <10.0%)]   
[Waiting: 2676 MHz (variance: 28.9%, need <10.0%)]   
[Waiting: 2712 MHz (variance: 27.2%, need <10.0%)]   
[Waiting: 2690 MHz (variance: 31.5%, need <10.0%)]   
[Waiting: 2543 MHz (variance: 21.7%, need <10.0%)]   
[Waiting: 2491 MHz (variance: 22.2%, need <10.0%)]   
[Waiting: 2476 MHz (variance: 19.3%, need <10.0%)]   
[Waiting: 2499 MHz (variance: 19.2%, need <10.0%)]   
[Waiting: 2609 MHz (variance: 16.9%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 16.6%, need <10.0%)]   
[Waiting: 2661 MHz (variance: 16.6%, need <10.0%)]   
[Waiting: 2594 MHz (variance: 17.0%, need <10.0%)]   
[Waiting: 2550 MHz (variance: 17.3%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2469 MHz (variance: 10.4%, need <10.0%)]   
[Waiting: 2462 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2454 MHz (variance: 10.5%, need <10.0%)]   
[Waiting: 2447 MHz (variance: 10.5%, need <10.0%)]   
[CPU stable at 2425 MHz (66% of base, variance: 6.1%)]
 [Ready: 2543 MHz]

================================================================================
  MISS DIAGNOSTICS (Slim)
================================================================================

Random misses only (no H2-biased sets), fixed N=1,000,000.
Purpose: regression tripwire for unsuccessful lookup behavior.

[2026-02-01 18:48:07] MissDiag CPU: 2653 MHz (base: 3686)

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=1000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        4.18          0.01          1.00        1.00          7.65        0.00        0.01
StableHashMap[Block]+SM64 (counted)        3.69          0.01          1.00        1.00          7.65        0.00        0.01
boost::unordered_node_map+SM64 (counted)        5.18          0.03          1.00           -             -           -           -
[Cooling: miss reserve change][Waiting: 2491 MHz (variance: 20.7%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 19.8%, need <10.0%)]   
[Waiting: 2373 MHz (variance: 10.9%, need <10.0%)]   
[Waiting: 2388 MHz (variance: 10.8%, need <10.0%)]   
[Waiting: 2395 MHz (variance: 10.8%, need <10.0%)]   
[Waiting: 2440 MHz (variance: 16.6%, need <10.0%)]   
[Waiting: 2425 MHz (variance: 19.8%, need <10.0%)]   
[Waiting: 2403 MHz (variance: 19.9%, need <10.0%)]   
[Waiting: 2381 MHz (variance: 20.1%, need <10.0%)]   
[Waiting: 2388 MHz (variance: 20.1%, need <10.0%)]   
[Waiting: 2521 MHz (variance: 45.3%, need <10.0%)]   
[Waiting: 2653 MHz (variance: 41.7%, need <10.0%)]   
[Waiting: 2690 MHz (variance: 41.1%, need <10.0%)]   
[Waiting: 2735 MHz (variance: 36.4%, need <10.0%)]   
[Waiting: 2757 MHz (variance: 32.1%, need <10.0%)]   
[Waiting: 2602 MHz (variance: 15.6%, need <10.0%)]   
[Waiting: 2587 MHz (variance: 12.8%, need <10.0%)]   
[Waiting: 2624 MHz (variance: 12.6%, need <10.0%)]   
[CPU stable at 2661 MHz (72% of base, variance: 6.9%)]
 [Ready: 2617 MHz]

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=2000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        3.91          0.00          1.00        1.00          3.81        0.00        0.00
StableHashMap[Block]+SM64 (counted)        3.90          0.00          1.00        1.00          3.81        0.00        0.00
boost::unordered_node_map+SM64 (counted)        5.09          0.02          1.00           -             -           -           -

[Cooling: before pathological erase][CPU stable at 2447 MHz (66% of base, variance: 4.5%)]
 [Ready: 2395 MHz]

================================================================================
  PATHOLOGICAL ERASE (TOMBSTONE DEGRADATION TEST)
================================================================================

Tests sustained churn on a single table without reset.
Tombstone-based maps may degrade over time.
Backward-shift maps stay stable.
Methodology: 3 warmup + 15 measured runs
             Round-robin execution with randomized order

[2026-02-01 18:48:20] Starting CPU: 2395 MHz (base: 3686)
N = 100000, Total operations = 5000000

               StableHashMap:   105.65 ns/step (+/-0.78, CI:[105.05,105.84])
    StableHashMap+SplitMix64:    96.17 ns/step (+/-1.01, CI:[95.58,96.60])
   StableHashMap[Block]+SM64:    24.64 ns/step (+/-0.44, CI:[24.41,24.86])
             FastHashMap[BS]:    31.85 ns/step (+/-0.51, CI:[31.72,32.24])
  FastHashMap[BS]+SplitMix64:    23.94 ns/step (+/-0.15, CI:[23.89,24.05])
             FastHashMap[TS]:    30.39 ns/step (+/-0.36, CI:[30.14,30.51])
  FastHashMap[TS]+SplitMix64:    19.87 ns/step (+/-0.26, CI:[19.74,20.00])
              tsl::robin_map:    17.83 ns/step (+/-0.21, CI:[17.77,17.98])
     ankerl::unordered_dense:    22.98 ns/step (+/-0.95, CI:[22.37,23.33])
         absl::flat_hash_map:    23.93 ns/step (+/-0.16, CI:[23.88,24.04])
         absl::node_hash_map:    92.83 ns/step (+/-1.05, CI:[92.43,93.49])
   boost::unordered_flat_map:     7.80 ns/step (+/-0.11, CI:[7.71,7.83])
   boost::unordered_node_map:    77.82 ns/step (+/-1.16, CI:[77.09,78.27])
              llvm::DenseMap:    21.64 ns/step (+/-0.31, CI:[21.47,21.79])
          std::unordered_map:   113.33 ns/step (+/-0.66, CI:[113.18,113.85])

================================================================================
  Benchmark Complete
================================================================================
```


---

# FeatureManager

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::FeatureManager Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::FeatureManager (primary)
  [x] std::map<string, bool> (baseline)
  [x] Manual if/else chain (baseline)

Configuration:
  Min batch ms:   50
  Scope:          ON
  Stabilize:      ON
  Cooldown:       ON

Design Invariants:
  1. Setup/teardown outside timed regions
  2. Medians are the primary reported statistic
  3. Correctness verified after each benchmark
  4. CPU frequency stabilized before measurement

  Waiting for CPU to stabilize...
[Waiting: 2764/3686 MHz (25% below base)]
[Waiting: 2690/3686 MHz (27% below base)]
[Waiting: 2174/3686 MHz (41% below base)]
[Waiting: 2285/3686 MHz (38% below base)]
[Waiting: 2174/3686 MHz (41% below base)]
[Waiting: 2211/3686 MHz (40% below base)]
[WARNING: CPU still 34% below base after 30s - 2432/3686 MHz]

================================================================================
  FIXTURE SETUP
================================================================================

[2026-02-01 18:49:56] Creating fixtures CPU: 2432 MHz (base: 3686)
  Verifying fixtures...
    [OK] All fixture validations passed


================================================================================
  LOOKUP OPERATIONS
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: is_enabled() is O(log n) lookup in std::map<string, FeatureNode>


================================================================================
  VALIDATION OPERATIONS
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: validate() traverses full dependency graph, O(n * d * log n)


================================================================================
  ENABLE/DISABLE OPERATIONS
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: enable() recursively enables dependencies with rollback on failure


================================================================================
  OBSERVER OVERHEAD
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: Observers are called synchronously on state change


================================================================================
  SERIALIZATION
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: JSON round-trip must preserve enabled state and relationships


================================================================================
  GRAPH CONSTRUCTION
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: add_feature() is O(log n) map insertion


================================================================================
  SYNCHRONIZATION POLICY OVERHEAD
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: Comparing SingleThreadedPolicy vs MutexSynchronizationPolicy


================================================================================
  GROUP OPERATIONS
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: Group state computed from member feature states


================================================================================
  BATCH OPERATIONS SCALING
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: batch_enable atomically enables multiple features with rollback


================================================================================
  DENSE GRAPH OPERATIONS
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: Performance with 1000+ relationships for scaling analysis


================================================================================
  SCOPED FEATURE CHANGE (RAII)
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: ScopedFeatureChange provides temporary state with auto-rollback


================================================================================
  CUSTOM STATE COMPUTER
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: User-provided state computation logic for groups


================================================================================
  MEMORY & CONSTRUCTION SCALING
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: Construction cost scaling with features and relationships


================================================================================
  MUTUALLY EXCLUSIVE GROUPS
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
  Contract: add_mutually_exclusive_group creates O(n^2) conflict relationships


================================================================================
  RUNNING BENCHMARKS
================================================================================

[2026-02-01 18:49:56] Section start CPU: 3243 MHz (base: 3686)
[2026-02-01 18:49:56] Starting benchmark execution CPU: 3243 MHz (base: 3686)
    is_enabled: enabled hit (10k features):   133.33 ns/op  (+/- 61.72)  CI95=[99.23, 167.44]
    is_enabled: disabled hit (10k features):   120.00 ns/op  (+/- 41.40)  CI95=[97.12, 142.88]
    is_enabled: missing feature (10k features):   300.00 ns/op  (+/- 84.52)  CI95=[253.30, 346.70]
    validate: requires-chain depth 50 (all enabled): 27673.33 ns/op  (+/-40109.02)  CI95=[5511.27, 49835.40]
    validate: flat graph 10k (no dependencies): 1880660.00 ns/op  (+/-423278.78)  CI95=[1646779.16, 2114540.84]
    validate: conflict graph 100 features: 344433.33 ns/op  (+/-119302.64)  CI95=[278513.18, 410353.48]
    batch_enable + batch_disable: chain depth 50 (cold): 112400.00 ns/op  (+/-2172.88)  CI95=[111199.38, 113600.62]
    enable + disable: single feature (no deps):  1366.67 ns/op  (+/-129.10)  CI95=[1295.33, 1438.00]
    enable: conflict detection (100 conflicts): 973373.33 ns/op  (+/-128709.84)  CI95=[902255.27, 1044491.39]
    enable/disable: 0 observers: 517013.33 ns/op  (+/-260153.61)  CI95=[373266.59, 660760.07]
    enable/disable: 1 observer: 519820.00 ns/op  (+/-221298.66)  CI95=[397542.39, 642097.61]
    enable/disable: 10 observers: 395860.00 ns/op  (+/-70110.67)  CI95=[357120.66, 434599.34]
    to_json: 10k features, no relationships: 15161073.33 ns/op  (+/-605490.66)  CI95=[14826512.12, 15495634.55]
    from_json: 10k features, no relationships: 10798306.67 ns/op  (+/-361799.70)  CI95=[10598395.82, 10998217.51]
    to_dot: 100 features, 50 relationships: 28433.33 ns/op  (+/-1856.14)  CI95=[27407.73, 29458.93]
    from_dot: 100 features, 50 relationships: 1091493.33 ns/op  (+/-244347.58)  CI95=[956480.15, 1226506.52]
    to_dot: 10k features, no relationships: 1763406.67 ns/op  (+/-522344.86)  CI95=[1474787.30, 2052026.03]
    add_feature: build 100 features: 17326.67 ns/op  (+/-1190.72)  CI95=[16668.74, 17984.59]
    add_feature: build 1000 features: 393840.00 ns/op  (+/-297769.74)  CI95=[229308.63, 558371.37]
    add_relationship: 100 Requires edges: 77000.00 ns/op  (+/-1126.31)  CI95=[76377.66, 77622.34]
    is_enabled: SingleThreadedPolicy (10k):   153.33 ns/op  (+/- 63.99)  CI95=[117.97, 188.69]
    is_enabled: MutexSynchronizationPolicy (10k):   146.67 ns/op  (+/- 51.64)  CI95=[118.13, 175.20]
    get_group_state: 20-member group:  1153.33 ns/op  (+/- 51.64)  CI95=[1124.80, 1181.87]
    batch_enable: 10 features (no deps):  7293.33 ns/op  (+/-319.52)  CI95=[7116.78, 7469.88]
    batch_enable: 100 features (no deps): 47000.00 ns/op  (+/-33829.91)  CI95=[28307.43, 65692.57]
    batch_enable: 1000 features (no deps): 716480.00 ns/op  (+/-190053.49)  CI95=[611466.78, 821493.22]
    batch_disable: 100 features (no deps): 193013.33 ns/op  (+/-115690.43)  CI95=[129089.09, 256937.57]
    validate: dense graph (200 nodes, ~1000 edges): 147873.33 ns/op  (+/-75288.51)  CI95=[106273.00, 189473.67]
    to_json: dense graph (200 nodes, ~1000 edges): 1359166.67 ns/op  (+/-196221.67)  CI95=[1250745.24, 1467588.09]
    from_json: dense graph (200 nodes, ~1000 edges): 871840.00 ns/op  (+/-147485.98)  CI95=[790347.27, 953332.73]
    validate: very dense graph (500 nodes, ~5000 edges): 485800.00 ns/op  (+/-42147.55)  CI95=[462511.56, 509088.44]
    validate: tree graph (depth 5, branching 3): 217113.33 ns/op  (+/-120298.98)  CI95=[150642.66, 283584.01]
    enable: tree root (cascades to 364 nodes): 276340.00 ns/op  (+/-29791.46)  CI95=[259878.86, 292801.14]
    ScopedFeatureChange: enable then auto-restore:   720.00 ns/op  (+/-142.43)  CI95=[641.30, 798.70]
    ScopedFeatureChange: disable then auto-restore:  1306.67 ns/op  (+/-218.65)  CI95=[1185.85, 1427.48]
    ScopedFeatureChange: nested scopes (3 deep):  3740.00 ns/op  (+/-1600.36)  CI95=[2855.73, 4624.27]
    get_group_state: default computer (50 features): 16373.33 ns/op  (+/-5185.35)  CI95=[13508.19, 19238.47]
    get_group_state: custom computer (50 features): 15426.67 ns/op  (+/-122.28)  CI95=[15359.10, 15494.23]
    get_group_state: default computer (200 features): 146746.67 ns/op  (+/-87822.10)  CI95=[98220.95, 195272.38]
    construct: 100 features + 50 relationships: 54906.67 ns/op  (+/-48375.50)  CI95=[28176.99, 81636.34]
    construct: 1000 features + 500 relationships: 407573.33 ns/op  (+/-70069.34)  CI95=[368856.83, 446289.84]
    construct: 5000 features + 2500 relationships: 2144580.00 ns/op  (+/-175039.11)  CI95=[2047862.91, 2241297.09]
    move: 1000-feature graph: 290900.00 ns/op  (+/-73012.20)  CI95=[250557.43, 331242.57]
    clear: 1000-feature graph: 268960.00 ns/op  (+/-61232.13)  CI95=[235126.45, 302793.55]
    add_mutually_exclusive_group: 10 features: 18840.00 ns/op  (+/-2037.44)  CI95=[17714.22, 19965.78]
    add_mutually_exclusive_group: 50 features: 310173.33 ns/op  (+/-118648.55)  CI95=[244614.60, 375732.07]
    validate: mutually exclusive group (20 features): 59260.00 ns/op  (+/-74826.07)  CI95=[17915.18, 100604.82]
    enable: conflict in mutually exclusive group:  3100.00 ns/op  (+/-1247.28)  CI95=[2410.82, 3789.18]
[2026-02-01 18:50:11] Benchmark execution complete CPU: 2617 MHz (base: 3686)

================================================================================
  CONCURRENT ACCESS PATTERNS
================================================================================

  Contract: Multi-threaded read/write contention with MutexSynchronizationPolicy

[2026-02-01 18:50:11] Concurrent benchmarks CPU: 2617 MHz (base: 3686)
  Hardware threads: 24, using up to 8
  Pinning policy: ON (Windows only)

  Thread Scaling (read-only, 300ms warmup + 500ms measured):
    Threads  |  Throughput (ops/sec)  |  Per-Thread
    ---------+------------------------+-------------
          1  |               9154356  |      9154356
          2  |               8244144  |      4122072
          4  |               8261847  |      2065462
          8  |               4761397  |       595175

  Mixed read-write (6 readers, 2 writers, with barrier):
    Total reads:   6
    Total writes:  6066 (failed: 0)
    Read throughput:  12 ops/sec
    Write throughput: 12043 ops/sec


================================================================================
  Benchmark Complete
================================================================================
  CSV exported: benchmark_results\20260201_184451\benchmark_FeatureManager.csv
[2026-02-01 18:50:15] Benchmark complete CPU: 3059 MHz (base: 3686)
```


---

# FlatMapSet

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::FlatMap/FlatSet Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::FlatMap (primary)
  [x] fat_p::FlatSet (primary)
  [x] std::map (baseline)
  [x] std::set (baseline)
  [x] boost::flat_map / boost::flat_set
  [x] absl::btree_map / absl::btree_set
  [ ] folly::sorted_vector_map (not detected)
  [ ] std::flat_map / std::flat_set (C++23 not available)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement

Expected Results:
  - FlatMap excels at: iteration, find, bulk insert (sorted)
  - FlatMap struggles at: random insert, erase (O(n) operations)
  - std::map has consistent O(log n) for all operations
  - absl::btree_map balances between tree and flat characteristics

Checking initial CPU state...
[2026-02-01 18:50:16] Initial CPU: 2543 MHz (base: 3686)
Waiting for CPU to stabilize...
[CPU stable at 2314 MHz (63% of base)]


================================================================================
  SECTION 1: Core Operations
================================================================================
[2026-02-01 18:50:27] Section start CPU: 2248 MHz (base: 3686)

--- N = 1000 ---
[2026-02-01 18:50:27] CPU: 2248 MHz (base: 3686)
[Cooling: size transition] [Ready: 2359 MHz]

  Bulk Build (sorted range):
                    std::map:    32.80 ns/op (+/-  3.69, CI:[30.68,34.41])
              fat_p::FlatMap:     1.90 ns/op (+/-  0.21, CI:[1.78,1.99])
             boost::flat_map:     0.70 ns/op (+/-  0.13, CI:[0.68,0.81])
             absl::btree_map:    17.10 ns/op (+/-  1.94, CI:[15.98,17.94])

  Bulk Insert (sorted):
                    std::map:    22.90 ns/op (+/-  0.74, CI:[22.73,23.48])
              fat_p::FlatMap:     1.90 ns/op (+/-  0.11, CI:[1.90,2.01])
             boost::flat_map:     3.00 ns/op (+/-  0.12, CI:[3.00,3.12])
             absl::btree_map:    20.00 ns/op (+/-  0.74, CI:[19.91,20.66])

  Bulk Insert (random):
                    std::map:    58.20 ns/op (+/- 78.66, CI:[49.67,129.29])
              fat_p::FlatMap:    82.50 ns/op (+/- 43.99, CI:[76.78,121.30])
             boost::flat_map:    87.80 ns/op (+/- 20.23, CI:[89.08,109.55])
             absl::btree_map:    41.00 ns/op (+/-  1.41, CI:[40.63,42.06])

  Find (hit):
                    std::map:    31.80 ns/op (+/-  1.10, CI:[31.22,32.33])
              fat_p::FlatMap:    32.40 ns/op (+/-  1.45, CI:[32.31,33.77])
             boost::flat_map:    32.30 ns/op (+/-  1.29, CI:[31.49,32.79])
             absl::btree_map:    19.70 ns/op (+/- 12.01, CI:[16.57,28.73])

  Find (miss):
                    std::map:     3.40 ns/op (+/-  0.15, CI:[3.39,3.53])
              fat_p::FlatMap:     2.20 ns/op (+/-  0.21, CI:[2.16,2.37])
             boost::flat_map:     2.20 ns/op (+/-  0.11, CI:[2.16,2.27])
             absl::btree_map:     1.20 ns/op (+/-  0.05, CI:[1.20,1.25])

  Iteration:
                    std::map:     2.50 ns/op (+/-  0.12, CI:[2.50,2.62])
              fat_p::FlatMap:     1.20 ns/op (+/-  0.07, CI:[1.19,1.25])
             boost::flat_map:     1.20 ns/op (+/-  0.08, CI:[1.18,1.26])
             absl::btree_map:     2.00 ns/op (+/-  0.09, CI:[1.98,2.06])

  lower_bound:
                    std::map:    14.70 ns/op (+/-  0.83, CI:[14.53,15.37])
              fat_p::FlatMap:    16.20 ns/op (+/-  0.97, CI:[16.05,17.03])
             boost::flat_map:    17.70 ns/op (+/-  0.70, CI:[17.15,17.86])
             absl::btree_map:    12.00 ns/op (+/- 30.56, CI:[4.47,35.41])

--- N = 10000 ---
[2026-02-01 18:51:34] CPU: 2469 MHz (base: 3686)
[Cooling: size transition] [Ready: 2727 MHz]

  Bulk Build (sorted range):
                    std::map:    34.79 ns/op (+/-  0.92, CI:[34.13,35.06])
              fat_p::FlatMap:     3.36 ns/op (+/-  1.04, CI:[2.19,3.24])
             boost::flat_map:     0.77 ns/op (+/-  1.31, CI:[1.27,2.59])
             absl::btree_map:    16.07 ns/op (+/-  0.33, CI:[15.97,16.30])

  Bulk Insert (sorted):
                    std::map:    33.20 ns/op (+/-  2.55, CI:[30.98,33.56])
              fat_p::FlatMap:     4.45 ns/op (+/-  1.34, CI:[3.08,4.43])
             boost::flat_map:     4.86 ns/op (+/-  1.34, CI:[3.69,5.04])
             absl::btree_map:    23.82 ns/op (+/-  1.78, CI:[22.23,24.03])

  Bulk Insert (random):
                    std::map:    91.84 ns/op (+/- 10.97, CI:[89.99,101.09])
              fat_p::FlatMap:   657.02 ns/op (+/- 30.60, CI:[646.54,677.51])
             boost::flat_map:   652.77 ns/op (+/- 24.08, CI:[644.29,668.66])
             absl::btree_map:    60.98 ns/op (+/- 14.34, CI:[59.36,73.87])

  Find (hit):
                    std::map:    58.85 ns/op (+/-  8.98, CI:[56.63,65.72])
              fat_p::FlatMap:    49.00 ns/op (+/-  1.68, CI:[48.59,50.29])
             boost::flat_map:    50.97 ns/op (+/- 13.91, CI:[48.51,62.59])
             absl::btree_map:    35.18 ns/op (+/-  1.65, CI:[34.48,36.15])

  Find (miss):
                    std::map:     4.83 ns/op (+/-  0.13, CI:[4.70,4.83])
              fat_p::FlatMap:     5.80 ns/op (+/-  0.23, CI:[5.82,6.05])
             boost::flat_map:     5.79 ns/op (+/-  0.17, CI:[5.73,5.90])
             absl::btree_map:     1.53 ns/op (+/-  0.15, CI:[1.51,1.67])

  Iteration:
                    std::map:     4.50 ns/op (+/-  0.78, CI:[4.13,4.91])
              fat_p::FlatMap:     1.21 ns/op (+/-  0.04, CI:[1.20,1.24])
             boost::flat_map:     1.21 ns/op (+/-  0.05, CI:[1.20,1.25])
             absl::btree_map:     2.00 ns/op (+/-  0.07, CI:[1.96,2.03])

  lower_bound:
                    std::map:    30.10 ns/op (+/-  7.37, CI:[29.95,37.41])
              fat_p::FlatMap:    25.32 ns/op (+/-  6.00, CI:[24.71,30.78])
             boost::flat_map:    27.97 ns/op (+/-  5.57, CI:[26.80,32.44])
             absl::btree_map:    20.15 ns/op (+/-  5.48, CI:[20.15,25.70])

--- N = 100000 ---
[2026-02-01 18:53:05] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]

  Bulk Build (sorted range):
                    std::map:    41.41 ns/op (+/-  2.48, CI:[41.07,43.58])
              fat_p::FlatMap:     3.70 ns/op (+/-  1.17, CI:[3.66,4.85])
             boost::flat_map:     3.21 ns/op (+/-  0.29, CI:[3.10,3.40])
             absl::btree_map:    15.61 ns/op (+/-  1.74, CI:[15.35,17.11])

  Bulk Insert (sorted):
                    std::map:    42.75 ns/op (+/-  6.11, CI:[40.59,46.77])
              fat_p::FlatMap:     4.31 ns/op (+/-  0.26, CI:[4.14,4.41])
             boost::flat_map:     5.42 ns/op (+/-  0.58, CI:[5.20,5.79])
             absl::btree_map:    22.87 ns/op (+/-  2.51, CI:[22.79,25.33])

  Bulk Insert (random):
                    std::map:   149.94 ns/op (+/-  4.45, CI:[146.35,150.85])
              fat_p::FlatMap:  8344.91 ns/op (+/-132.71, CI:[8267.16,8401.48])
             boost::flat_map:  8334.42 ns/op (+/-177.60, CI:[8237.34,8417.10])
             absl::btree_map:    73.97 ns/op (+/-  3.00, CI:[73.58,76.62])

  Find (hit):
                    std::map:   138.06 ns/op (+/-  4.59, CI:[135.03,139.67])
              fat_p::FlatMap:    67.27 ns/op (+/-  2.90, CI:[66.88,69.82])
             boost::flat_map:    74.54 ns/op (+/-  3.45, CI:[71.82,75.31])
             absl::btree_map:    48.60 ns/op (+/-  3.02, CI:[48.81,51.87])

  Find (miss):
                    std::map:     6.59 ns/op (+/-  0.07, CI:[6.57,6.64])
              fat_p::FlatMap:     6.94 ns/op (+/-  0.78, CI:[6.80,7.59])
             boost::flat_map:     6.90 ns/op (+/-  1.19, CI:[6.81,8.02])
             absl::btree_map:     1.96 ns/op (+/-  0.02, CI:[1.94,1.97])

  Iteration:
                    std::map:     9.43 ns/op (+/-  3.46, CI:[8.63,12.13])
              fat_p::FlatMap:     1.21 ns/op (+/-  0.10, CI:[1.22,1.33])
             boost::flat_map:     1.30 ns/op (+/-  0.34, CI:[1.22,1.57])
             absl::btree_map:     1.98 ns/op (+/-  0.14, CI:[1.96,2.10])

  lower_bound:
                    std::map:    53.51 ns/op (+/-  3.65, CI:[51.58,55.27])
              fat_p::FlatMap:    34.58 ns/op (+/-  2.67, CI:[34.35,37.04])
             boost::flat_map:    35.89 ns/op (+/-  3.53, CI:[35.41,38.98])
             absl::btree_map:    26.60 ns/op (+/-  2.37, CI:[26.65,29.04])
[Cooling: before pathological insert] [Ready: 2506 MHz]

================================================================================
  SECTION 2: Pathological Random Insert (FlatMap's Weakness)
================================================================================
  This benchmark measures single random insertions into a populated container.
  For flat containers, each insert may require shifting O(n) elements.

[2026-02-01 18:55:16] Section start CPU: 2506 MHz (base: 3686)

--- Base size: 1000, inserting 100 missing keys ---
[2026-02-01 18:55:16] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2506 MHz]
  Single random insert (into populated map):
                    std::map:    43.00 ns/op (+/- 30.20, CI:[44.85,75.42])
              fat_p::FlatMap:   149.00 ns/op (+/-109.79, CI:[184.77,295.89])
             boost::flat_map:   155.00 ns/op (+/-124.35, CI:[181.67,307.53])
             absl::btree_map:    25.00 ns/op (+/- 20.45, CI:[29.45,50.15])

--- Base size: 5000, inserting 100 missing keys ---
[2026-02-01 18:55:32] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2543 MHz]
  Single random insert (into populated map):
                    std::map:    50.00 ns/op (+/-  7.28, CI:[46.72,54.08])
              fat_p::FlatMap:   619.00 ns/op (+/-172.06, CI:[622.79,796.94])
             boost::flat_map:   652.00 ns/op (+/-188.99, CI:[638.36,829.64])
             absl::btree_map:    59.00 ns/op (+/- 16.95, CI:[52.42,69.58])

--- Base size: 10000, inserting 100 missing keys ---
[2026-02-01 18:55:48] CPU: 2801 MHz (base: 3686)
[Cooling: size transition] [Ready: 2469 MHz]
  Single random insert (into populated map):
                    std::map:    57.00 ns/op (+/- 14.43, CI:[50.90,65.50])
              fat_p::FlatMap:  1281.00 ns/op (+/-167.62, CI:[1249.91,1419.56])
             boost::flat_map:  1284.00 ns/op (+/-215.18, CI:[1252.44,1470.23])
             absl::btree_map:    75.00 ns/op (+/- 37.36, CI:[68.50,106.30])
[Cooling: before iteration benchmark] [Ready: 2432 MHz]

================================================================================
  SECTION 3: Iteration Speed (FlatMap's Strength)
================================================================================
  FlatMap stores elements contiguously, enabling hardware prefetching.
  std::map requires pointer chasing through scattered tree nodes.

[2026-02-01 18:56:08] Section start CPU: 2432 MHz (base: 3686)

--- N = 1000 ---
[Cooling: size transition] [Ready: 2543 MHz]
  Iteration (ns/element):
                    std::map:     2.40 ns/elem (+/-0.23)
              fat_p::FlatMap:     1.20 ns/elem (+/-0.06)
             boost::flat_map:     1.20 ns/elem (+/-0.06)
             absl::btree_map:     2.10 ns/elem (+/-0.11)

--- N = 10000 ---
[Cooling: size transition] [Ready: 2543 MHz]
  Iteration (ns/element):
                    std::map:     3.51 ns/elem (+/-2.20)
              fat_p::FlatMap:     1.20 ns/elem (+/-0.01)
             boost::flat_map:     1.21 ns/elem (+/-0.12)
             absl::btree_map:     2.00 ns/elem (+/-0.03)

--- N = 100000 ---
[Cooling: size transition] [Ready: 2580 MHz]
  Iteration (ns/element):
                    std::map:     9.01 ns/elem (+/-1.88)
              fat_p::FlatMap:     1.21 ns/elem (+/-0.78)
             boost::flat_map:     1.20 ns/elem (+/-0.03)
             absl::btree_map:     1.97 ns/elem (+/-0.37)

--- N = 1000000 ---
[Cooling: size transition] [Ready: 2395 MHz]
  Iteration (ns/element):
                    std::map:    12.50 ns/elem (+/-2.23)
              fat_p::FlatMap:     1.37 ns/elem (+/-0.21)
             boost::flat_map:     1.45 ns/elem (+/-0.16)
             absl::btree_map:     2.95 ns/elem (+/-0.49)
[Cooling: before set operations] [Ready: 2322 MHz]

================================================================================
  SECTION 4: FlatSet Core Operations
================================================================================
[2026-02-01 18:57:07] Section start CPU: 2322 MHz (base: 3686)

--- N = 1000 ---
[2026-02-01 18:57:07] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]

  Bulk Build (sorted range):
                    std::set:    55.10 ns/op (+/-  4.55, CI:[49.84,54.45])
              fat_p::FlatSet:     1.50 ns/op (+/-  0.24, CI:[1.46,1.70])
             boost::flat_set:     0.50 ns/op (+/-  0.06, CI:[0.45,0.52])
             absl::btree_set:    27.80 ns/op (+/-  5.08, CI:[24.58,29.72])

  Bulk Insert (sorted):
                    std::set:    41.90 ns/op (+/- 21.77, CI:[36.26,58.29])
              fat_p::FlatSet:     2.20 ns/op (+/-  0.30, CI:[1.98,2.29])
             boost::flat_set:     3.30 ns/op (+/-  0.48, CI:[3.08,3.56])
             absl::btree_set:    19.50 ns/op (+/-113.09, CI:[-1.53,112.93])

  Bulk Insert (random):
                    std::set:    59.90 ns/op (+/- 15.16, CI:[58.07,73.42])
              fat_p::FlatSet:    53.20 ns/op (+/- 13.90, CI:[52.17,66.24])
             boost::flat_set:    54.80 ns/op (+/- 16.07, CI:[53.75,70.01])
             absl::btree_set:    37.60 ns/op (+/- 12.20, CI:[36.88,49.22])

  Find (hit):
                    std::set:    31.70 ns/op (+/-  0.59, CI:[31.44,32.04])
              fat_p::FlatSet:    32.00 ns/op (+/-  0.82, CI:[31.68,32.51])
             boost::flat_set:    31.00 ns/op (+/-  0.67, CI:[30.84,31.52])
             absl::btree_set:    23.50 ns/op (+/-  1.05, CI:[23.28,24.34])

  Find (miss):
                    std::set:     3.30 ns/op (+/-  0.15, CI:[3.25,3.40])
              fat_p::FlatSet:     2.90 ns/op (+/-  0.13, CI:[2.83,2.96])
             boost::flat_set:     2.90 ns/op (+/-  0.15, CI:[2.87,3.02])
             absl::btree_set:     1.20 ns/op (+/-  0.07, CI:[1.20,1.28])

  Iteration:
                    std::set:     2.10 ns/op (+/-  0.05, CI:[2.10,2.15])
              fat_p::FlatSet:     1.20 ns/op (+/-  0.04, CI:[1.20,1.23])
             boost::flat_set:     1.20 ns/op (+/-  0.03, CI:[1.18,1.21])
             absl::btree_set:     1.70 ns/op (+/-  0.06, CI:[1.69,1.75])

  lower_bound:
                    std::set:    19.20 ns/op (+/-  2.81, CI:[18.92,21.77])
              fat_p::FlatSet:    20.90 ns/op (+/-  2.42, CI:[20.68,23.12])
             boost::flat_set:    22.20 ns/op (+/-  1.17, CI:[22.04,23.23])
             absl::btree_set:    18.70 ns/op (+/-  1.02, CI:[18.50,19.53])

--- N = 10000 ---
[2026-02-01 18:59:01] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2801 MHz]

  Bulk Build (sorted range):
                    std::set:    22.21 ns/op (+/-  6.10, CI:[22.24,28.41])
              fat_p::FlatSet:     0.76 ns/op (+/-  0.22, CI:[0.72,0.94])
             boost::flat_set:     0.22 ns/op (+/-  0.05, CI:[0.20,0.25])
             absl::btree_set:    11.38 ns/op (+/- 12.96, CI:[9.11,22.23])

  Bulk Insert (sorted):
                    std::set:    21.97 ns/op (+/-  1.58, CI:[21.62,23.22])
              fat_p::FlatSet:     1.17 ns/op (+/-  0.12, CI:[1.16,1.27])
             boost::flat_set:     1.90 ns/op (+/-  0.38, CI:[1.95,2.34])
             absl::btree_set:    10.55 ns/op (+/-  0.88, CI:[10.51,11.41])

  Bulk Insert (random):
                    std::set:    80.37 ns/op (+/- 16.99, CI:[76.41,93.60])
              fat_p::FlatSet:   196.14 ns/op (+/- 19.85, CI:[200.29,220.38])
             boost::flat_set:   197.85 ns/op (+/-  9.74, CI:[195.16,205.02])
             absl::btree_set:    47.40 ns/op (+/-  5.53, CI:[46.55,52.15])

  Find (hit):
                    std::set:    55.56 ns/op (+/-  8.58, CI:[54.10,62.78])
              fat_p::FlatSet:    46.38 ns/op (+/-  1.39, CI:[46.03,47.44])
             boost::flat_set:    46.86 ns/op (+/-  7.77, CI:[46.70,54.57])
             absl::btree_set:    34.08 ns/op (+/-  4.48, CI:[33.14,37.67])

  Find (miss):
                    std::set:     4.41 ns/op (+/-  1.15, CI:[4.22,5.39])
              fat_p::FlatSet:     5.78 ns/op (+/-  5.35, CI:[4.83,10.25])
             boost::flat_set:     5.96 ns/op (+/-  2.35, CI:[5.56,7.93])
             absl::btree_set:     1.17 ns/op (+/-  0.13, CI:[1.14,1.27])

  Iteration:
                    std::set:     3.02 ns/op (+/-  0.14, CI:[3.01,3.15])
              fat_p::FlatSet:     1.20 ns/op (+/-  0.05, CI:[1.20,1.25])
             boost::flat_set:     1.20 ns/op (+/-  0.04, CI:[1.20,1.24])
             absl::btree_set:     1.71 ns/op (+/-  0.15, CI:[1.64,1.79])

  lower_bound:
                    std::set:    29.08 ns/op (+/-  8.55, CI:[27.43,36.08])
              fat_p::FlatSet:    25.16 ns/op (+/-  6.44, CI:[24.00,30.52])
             boost::flat_set:    25.40 ns/op (+/-  1.49, CI:[25.00,26.50])
             absl::btree_set:    18.05 ns/op (+/-  3.87, CI:[17.30,21.22])

--- N = 100000 ---
[2026-02-01 19:01:02] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2174 MHz]

  Bulk Build (sorted range):
                    std::set:    23.04 ns/op (+/-  4.90, CI:[22.74,27.69])
              fat_p::FlatSet:     0.81 ns/op (+/-  0.13, CI:[0.83,0.96])
             boost::flat_set:     0.23 ns/op (+/-  0.45, CI:[0.19,0.65])
             absl::btree_set:    12.01 ns/op (+/-  1.72, CI:[12.06,13.80])

  Bulk Insert (sorted):
                    std::set:    22.71 ns/op (+/-  5.03, CI:[22.72,27.81])
              fat_p::FlatSet:     1.22 ns/op (+/-  0.33, CI:[1.13,1.47])
             boost::flat_set:     1.91 ns/op (+/-  0.82, CI:[1.92,2.76])
             absl::btree_set:    10.98 ns/op (+/-  1.15, CI:[10.85,12.02])

  Bulk Insert (random):
                    std::set:   124.55 ns/op (+/-  6.33, CI:[122.87,129.27])
              fat_p::FlatSet:  2610.81 ns/op (+/- 19.95, CI:[2591.94,2612.13])
             boost::flat_set:  2606.59 ns/op (+/- 18.46, CI:[2591.72,2610.41])
             absl::btree_set:    61.33 ns/op (+/-  3.23, CI:[61.01,64.29])

  Find (hit):
                    std::set:   112.68 ns/op (+/-  4.81, CI:[111.58,116.44])
              fat_p::FlatSet:    63.53 ns/op (+/-  2.16, CI:[63.62,65.81])
             boost::flat_set:    63.72 ns/op (+/-  2.22, CI:[64.10,66.34])
             absl::btree_set:    45.33 ns/op (+/-  1.79, CI:[45.58,47.39])

  Find (miss):
                    std::set:     6.59 ns/op (+/-  0.69, CI:[6.42,7.11])
              fat_p::FlatSet:     6.92 ns/op (+/-  0.71, CI:[6.75,7.47])
             boost::flat_set:     6.89 ns/op (+/-  0.16, CI:[6.91,7.07])
             absl::btree_set:     1.50 ns/op (+/-  0.77, CI:[1.34,2.12])

  Iteration:
                    std::set:     5.82 ns/op (+/-  2.58, CI:[5.03,7.65])
              fat_p::FlatSet:     1.20 ns/op (+/-  0.03, CI:[1.20,1.23])
             boost::flat_set:     1.20 ns/op (+/-  0.01, CI:[1.20,1.21])
             absl::btree_set:     1.65 ns/op (+/-  0.14, CI:[1.53,1.67])

  lower_bound:
                    std::set:    42.97 ns/op (+/-  1.11, CI:[43.09,44.22])
              fat_p::FlatSet:    31.47 ns/op (+/-  2.17, CI:[31.71,33.90])
             boost::flat_set:    31.33 ns/op (+/-  0.91, CI:[31.27,32.20])
             absl::btree_set:    21.65 ns/op (+/-  0.82, CI:[21.50,22.33])

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
  Benchmark Complete
================================================================================
```


---

# FloatingPointComparison

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::FloatingPointComparison Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::floatEqual<StandardPolicy> (primary)
  [x] fat_p::approximateEqual (primary)
  [x] Manual absolute epsilon (baseline)
  [x] Manual relative epsilon (baseline)
  [x] Direct operator== (baseline)

Design Invariants:
  1. Setup/teardown outside timed regions
  2. Medians are the primary reported statistic
  3. CPU frequency stabilized before measurement

Operations per batch: 1000000

--- Fat-P Policies vs Manual Baseline ---
Contract: absolute/hybrid tolerance comparison semantics

[2026-02-01 19:02:49] CPU: 3575 MHz (base: 3686)
  Fat-P Standard                1.68       ns  (mean: 1.69, stddev: 0.07)  CI95: [1.65, 1.72]
  Manual absolute               0.36       ns  (mean: 0.37, stddev: 0.02)  CI95: [0.36, 0.38]
  Ratio: 4.63x

  Fat-P Hybrid                  6.76       ns  (mean: 6.90, stddev: 0.24)  CI95: [6.77, 7.04]
  Manual hybrid                 3.22       ns  (mean: 3.20, stddev: 0.10)  CI95: [3.15, 3.25]
  Ratio: 2.10x

--- Policy Comparison (Normal Values) ---
Contract: epsilon-based floating-point equality

[2026-02-01 19:02:50] CPU: 2875 MHz (base: 3686)
  Standard                      1.68       ns  (mean: 1.74, stddev: 0.18)  CI95: [1.64, 1.84]
  Relative                      2.24       ns  (mean: 2.26, stddev: 0.15)  CI95: [2.17, 2.34]
  ULP                           6.63       ns  (mean: 6.73, stddev: 0.28)  CI95: [6.58, 6.89]
  Hybrid                        6.71       ns  (mean: 6.76, stddev: 0.14)  CI95: [6.68, 6.84]

--- Special Value Handling ---
Contract: IEEE 754 NaN/Inf semantics

[2026-02-01 19:02:50] CPU: 2543 MHz (base: 3686)
  NaN                           1.51       ns  (mean: 1.53, stddev: 0.06)  CI95: [1.50, 1.56]
  Infinity                      1.96       ns  (mean: 1.99, stddev: 0.06)  CI95: [1.96, 2.02]
  Speedup vs normal: 4.4x (NaN), 3.4x (Inf)

================================================================
  Benchmark Complete
================================================================
```


---

# IntrusiveList

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::IntrusiveList Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::IntrusiveList<FastPolicy> (primary)
  [x] fat_p::IntrusiveList<SafePolicy> (primary)
  [x] std::list<T*> (baseline)
  [x] boost::intrusive::list
  [x] eastl::intrusive_list
  [x] llvm::simple_ilist
  [x] etl::intrusive_list

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement


================================================================================
  PUSH_BACK PERFORMANCE (N=10000)
================================================================================

[2026-02-01 19:02:50] CPU: 2801 MHz (base: 3686)
Contract: Zero allocation for IntrusiveList vs heap allocation for std::list

Measuring time to push_back 10000 pre-existing nodes.
IntrusiveList: no allocation (nodes pre-exist)
std::list: allocates node wrapper for each push

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           1.75        1.78      0.05  [1.75, 1.80] ns/op
fat_p::IntrusiveList (safe)           2.35        2.39      0.09  [2.35, 2.44] ns/op
std::list<T*>                        17.63       18.08      1.51  [17.32, 18.84] ns/op
boost::intrusive::list                1.91        1.93      0.05  [1.90, 1.95] ns/op
eastl::intrusive_list                 1.81        1.83      0.06  [1.80, 1.86] ns/op
etl::intrusive_list [!]               1.37        1.41      0.12  [1.34, 1.47] ns/op
llvm::simple_ilist                    1.89        1.90      0.06  [1.87, 1.93] ns/op

Speedup (IntrusiveList vs std::list): 7.5x

================================================================================
  REMOVE PERFORMANCE (N=10000)
================================================================================

[2026-02-01 19:02:50] CPU: 3501 MHz (base: 3686)
Contract: O(1) removal with known node reference - except ETL which is O(N)

Measuring time to remove 10000 nodes in random order.
Most intrusive lists: O(1) removal via node reference or iterator_to()
ETL: O(N) removal - searches the list (API limitation)

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           5.25        5.19      0.20  [5.09, 5.29] ns/op
fat_p::IntrusiveList (safe)           6.18        6.25      0.28  [6.11, 6.40] ns/op
std::list<T*>                        30.32       29.97      1.53  [29.19, 30.74] ns/op
boost::intrusive::list                5.26        5.23      0.21  [5.12, 5.34] ns/op
eastl::intrusive_list                 4.42        4.46      0.14  [4.38, 4.53] ns/op
etl::intrusive_list [!]           14663.73    14771.85    303.69  [14618.17, 14925.54] ns/op
llvm::simple_ilist                    5.17        5.13      0.22  [5.02, 5.25] ns/op

Speedup (IntrusiveList vs std::list): 4.9x

================================================================================
  ITERATION PERFORMANCE (N=10000)
================================================================================

[2026-02-01 19:02:53] CPU: 2395 MHz (base: 3686)
Contract: Sequential traversal with equivalent memory layout (std::deque storage)

Measuring time to iterate and sum 10000 elements.
All competitors use std::deque for node storage.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       63700.00    66473.33   7594.03  [62630.23, 70316.44] ns/iter
fat_p::IntrusiveList (safe)       73700.00    76113.33   9781.53  [71163.19, 81063.47] ns/iter
std::list<T*>                     32600.00    33893.33   5685.88  [31015.88, 36770.79] ns/iter
boost::intrusive::list            68600.00    69926.67   8309.59  [65721.44, 74131.90] ns/iter
eastl::intrusive_list             66200.00    65386.67  10076.84  [60287.08, 70486.25] ns/iter
etl::intrusive_list [!]           69100.00    66846.67  12503.71  [60518.92, 73174.42] ns/iter
llvm::simple_ilist                68000.00    64420.00  15241.03  [56706.98, 72133.02] ns/iter

Per-element: 6.37 ns/element

================================================================================
  SPLICE PERFORMANCE (N=10000)
================================================================================

[2026-02-01 19:02:53] CPU: 2395 MHz (base: 3686)
Contract: Build source list + splice to dest (measures total transfer cost)

Measuring time to build source list and splice 10000 elements.
std::list: N allocating push_backs + O(1) splice
Intrusive: N non-allocating links + O(1) splice
fat_p: fast policy splices in O(1); safe policy is O(N) due to owner updates.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       49000.00    50186.67   1534.77  [49409.97, 50963.37] ns/splice
fat_p::IntrusiveList (safe)       87400.00    86820.00   3905.71  [84843.44, 88796.56] ns/splice
std::list<T*>                    269200.00   275520.00  18426.89  [266194.71, 284845.29] ns/splice
boost::intrusive::list            50700.00    49973.33   1305.74  [49312.54, 50634.13] ns/splice
eastl::intrusive_list             18200.00    18493.33    493.48  [18243.60, 18743.07] ns/splice
etl::intrusive_list [!]           47400.00    46993.33   1831.65  [46066.39, 47920.28] ns/splice
llvm::simple_ilist                19500.00    19393.33    554.81  [19112.56, 19674.10] ns/splice

Note: Results include cost of building source list (N push_backs).
std::list is slowest due to N allocations during setup.

================================================================================
  MEMORY OVERHEAD COMPARISON
================================================================================

Per-node memory overhead for different list implementations:

Structure                               sizeof (bytes)
-------------------------------------------------------
Raw user data (int64_t + 7 padding)                  64
fat_p::IntrusiveList (fast)                          80
fat_p::IntrusiveList (safe)                          88
std::list<T*>                                        88
boost::intrusive::list                               80
eastl::intrusive_list                                80
etl::intrusive_list [!]                              80
llvm::simple_ilist                                   80

Analysis:
- IntrusiveList adds 24 bytes per node (prev + next + owner)
- std::list adds 16 bytes per node PLUS allocator overhead (~8-32 bytes)
- Other intrusive lists add 16 bytes (prev + next only)
- For 1M nodes, IntrusiveList saves ~24-48 MB vs std::list

================================================================================
  FREE LIST PATTERN (Pool=1000, Ops=100000)
================================================================================

[2026-02-01 19:02:53] CPU: 2727 MHz (base: 3686)
Contract: Allocate/deallocate pattern using list as free list

Simulating object pool: allocate (pop_front) and deallocate (push_back)
Pattern: 70% allocate, 30% deallocate

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           2.90        3.06      0.50  [2.80, 3.31] ns/op
fat_p::IntrusiveList (safe)           2.94        2.93      0.06  [2.90, 2.96] ns/op
std::list<T*>                         7.95        8.04      0.31  [7.89, 8.20] ns/op
boost::intrusive::list                3.02        3.03      0.05  [3.01, 3.06] ns/op
eastl::intrusive_list                 3.14        3.09      0.18  [3.00, 3.18] ns/op
etl::intrusive_list [!]               3.19        3.23      0.12  [3.17, 3.29] ns/op
llvm::simple_ilist                    2.96        3.02      0.10  [2.97, 3.08] ns/op

Speedup: 2.7x

================================================================================
  IS_LINKED CHECK PERFORMANCE (N=1000)
================================================================================

[2026-02-01 19:02:53] CPU: 2395 MHz (base: 3686)
Contract: Check if each node is in a list - O(1) vs O(N) depending on library

Measuring time to check membership for 1000 nodes (half linked).
fat_p/Boost/EASTL/ETL: O(1) via link state or owner pointers
LLVM/std::list: O(N) search required - no public is_linked()/isLinked() API
Note: N kept small because O(N) search per node = O(N^2) total.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           0.50        0.51      0.07  [0.47, 0.54] ns/op
fat_p::IntrusiveList (safe)           0.50        0.50      0.05  [0.47, 0.53] ns/op
std::list<T*>                       283.70      277.94      7.79  [274.00, 281.88] ns/op
boost::intrusive::list                0.70        0.72      0.07  [0.69, 0.75] ns/op
eastl::intrusive_list                 0.50        0.49      0.07  [0.46, 0.53] ns/op
etl::intrusive_list [!]               0.50        0.51      0.05  [0.49, 0.54] ns/op
llvm::simple_ilist                  283.90      278.45      7.83  [274.49, 282.41] ns/op

Note: Libraries with O(1) membership: fat_p, Boost, EASTL, ETL.
Libraries requiring O(N) search: LLVM, std::list.
At N=100000, O(N) search would be ~10000x slower than O(1).
fat_p safe policy: owner pointer can identify WHICH list owns the node.

================================================================================
  Benchmark Complete
================================================================================
```


---

# LockFreeQueue

```
================================================================================
  fat_p::LockFreeQueue Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::LockFreeQueue (primary)
  [x] fat_p::LockFreeRingBuffer (primary)
  [x] std::mutex + std::queue (baseline)
  [x] moodycamel::ConcurrentQueue
  [x] boost::lockfree::queue

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement

Configuration:
  Warmup runs:    3
  Measured runs:  15
  Seed:           12345
  Target work:    1000000
  Min batch ms:   50
  Scope:          enabled
  Stabilize:      enabled
  Cooldown:       enabled
  CSV output:     benchmark_results\20260201_184451\benchmark_LockFreeQueue.csv

[INIT 19:02:53] CPU: 3686 MHz (base: 3686)

Correctness verification:
  [PASS] LockFreeQueue FIFO ordering
  [PASS] LockFreeRingBuffer (SPSC) FIFO ordering
  [PASS] LockFreeRingBufferMPMC FIFO ordering

[BenchmarkScope] High priority, CPU non-0 affinity
  [NOTE] Single-thread target work clamped from 1000000 to 131072 to avoid capacity overflow.

================================================================================
  Single-Threaded Throughput (enqueue + dequeue cycle)
================================================================================

[START 19:02:53] CPU: 3686 MHz (base: 3686)
Contract: Pure queue overhead without contention. Measures enqueue+dequeue per op.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       8.07        8.30      0.60  [    7.99,     8.60]  ns/op
fat_p::WorkQueue (sharded)                 8.64        8.59      0.34  [    8.42,     8.76]  ns/op
fat_p::LockFreeRingBuffer (SPSC)           0.54        0.54      0.02  [    0.53,     0.55]  ns/op
fat_p::LockFreeRingBufferMPMC              9.04        8.92      0.45  [    8.69,     9.14]  ns/op
std::mutex + std::queue (baseline)        16.81       16.94      0.55  [   16.67,    17.22]  ns/op
moodycamel::ConcurrentQueue                8.12        8.20      0.25  [    8.07,     8.32]  ns/op
boost::lockfree::queue                    39.69       39.17      4.87  [   36.70,    41.63]  ns/op

[END 19:02:54] CPU: 3686 MHz (base: 3686)

================================================================================
  SPSC Throughput (1 producer, 1 consumer threads)
================================================================================

[START 19:02:54] CPU: 3686 MHz (base: 3686)
Contract: Dedicated producer and consumer threads. Native SPSC use case.

Library                                  Median        Mean    Stddev  CI95
------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                      11.87       13.30      4.23  [   11.16,    15.44]  ns/op
fat_p::LockFreeRingBuffer (SPSC)          29.13       33.56     15.66  [   25.63,    41.48]  ns/op
std::mutex + std::queue (baseline)        19.19       19.98      2.07  [   18.93,    21.03]  ns/op
moodycamel::ConcurrentQueue               16.14       17.34      3.44  [   15.60,    19.08]  ns/op
boost::lockfree::queue                   130.43      137.92     13.09  [  131.30,   144.55]  ns/op

[END 19:03:02] CPU: 3686 MHz (base: 3686)

================================================================================
  MPMC Scaling (N producers, N consumers)
================================================================================

[START 19:03:02] CPU: 3686 MHz (base: 3686)
Contract: Equal producer and consumer threads. Tests lock-free scaling.

Thread counts: 1 2 4 8 12 16 

Library                                      1T          2T          4T          8T         12T         16T
-----------------------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                        6.0        21.7        60.1        80.3        87.2        88.6 ns/op
fat_p::WorkQueue (sharded)                 12.7        12.0        20.6        27.2        57.0        32.7 ns/op
fat_p::WorkQueue (round-robin)             17.4        20.0        48.0        69.7        71.4        74.7 ns/op
fat_p::WorkQueue (stride-3)                10.2        22.5        47.6        71.0        72.9        69.5 ns/op
fat_p::LockFreeRingBufferMPMC              11.6        21.4        71.7        85.6        88.3        93.5 ns/op
std::mutex + std::queue (baseline)         24.3        18.5        46.0       200.2       314.9       240.7 ns/op
moodycamel::ConcurrentQueue                20.4        35.8        50.2        48.8        44.2        41.0 ns/op
boost::lockfree::queue                     84.3       108.0       278.2       254.3       274.0       314.2 ns/op

[END 19:05:21] CPU: 3686 MHz (base: 3686)

================================================================================
  Asymmetric MPMC (MPSC, SPMC, Unbalanced)
================================================================================

[START 19:05:21] CPU: 3686 MHz (base: 3686)
Contract: Tests non-symmetric producer/consumer ratios.


Library                                   8P:1C     4P:1C     1P:8C     1P:4C     8P:2C     2P:8C
-------------------------------------------------------------------------------------------------
fat_p::LockFreeQueue                       47.9      40.8      74.7      45.7      62.4      80.4 ns/op
fat_p::WorkQueue (sharded)                 22.0      16.6      23.7      20.8      23.8      23.6 ns/op
fat_p::LockFreeRingBufferMPMC              49.0      43.8      76.2      49.6      65.3      80.5 ns/op
std::mutex + std::queue (baseline)         36.7      19.6     181.7      28.9      59.4     147.9 ns/op
moodycamel::ConcurrentQueue                15.5      15.9      78.4      75.2      64.6      49.7 ns/op
boost::lockfree::queue                    149.4     148.7     285.2     212.1     245.4     259.5 ns/op

[END 19:07:11] CPU: 3686 MHz (base: 3686)

================================================================================
  Summary
================================================================================

Benchmarks completed.
Hardware concurrency: 24 threads
[FINAL 19:07:11] CPU: 3686 MHz (base: 3686)
```


---

# ObjectPool

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::ObjectPool Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::ObjectPool (primary)
  [x] boost::object_pool
  [x] foonathan::memory_pool
  [x] EASTL::fixed_pool (fixed-capacity, no auto-grow)
  [x] std::pmr::unsynchronized_pool_resource (C++17)
  [x] std::pmr::synchronized_pool_resource (C++17 thread-safe)
  [x] new/delete (baseline)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement

Checking initial CPU state...
[2026-02-01 19:07:11] Initial CPU: 2174 MHz (base: 3686)
Waiting for CPU to stabilize...
[CPU stable at 2314 MHz (63% of base)]


================================================================================
  Acquire + Release Cycle (SmallTrivial 16B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth


--- N = 1000 ops ---
[2026-02-01 19:07:27] CPU: 2359 MHz (base: 3686)
[Cooling: size transition] [Ready: 2211 MHz]
           fat_p::ObjectPool: median=    5.60 ns/op  mean=    5.82 +/-  0.49
          boost::object_pool: median=    6.20 ns/op  mean=    6.29 +/-  0.43
      foonathan::memory_pool: median=    8.70 ns/op  mean=    9.18 +/-  1.11
   EASTL::fixed_pool [!grow]: median=    5.20 ns/op  mean=    5.59 +/-  1.21
       std::pmr::unsync_pool: median=   15.50 ns/op  mean=   16.33 +/-  1.48
                  new/delete: median=   49.00 ns/op  mean=   73.13 +/- 91.99
  Speedup vs new/delete: 8.8x

--- N = 10000 ops ---
[2026-02-01 19:07:43] CPU: 2211 MHz (base: 3686)
[Cooling: size transition] [Ready: 2617 MHz]
           fat_p::ObjectPool: median=    2.24 ns/op  mean=    2.28 +/-  0.16
          boost::object_pool: median=    2.45 ns/op  mean=    2.46 +/-  0.01
      foonathan::memory_pool: median=    3.03 ns/op  mean=    3.04 +/-  0.08
   EASTL::fixed_pool [!grow]: median=    2.11 ns/op  mean=    2.16 +/-  0.10
       std::pmr::unsync_pool: median=    6.42 ns/op  mean=    6.56 +/-  0.42
                  new/delete: median=   19.93 ns/op  mean=   20.05 +/-  0.44
  Speedup vs new/delete: 8.9x

--- N = 100000 ops ---
[2026-02-01 19:07:59] CPU: 2617 MHz (base: 3686)
[Cooling: size transition] [Ready: 2432 MHz]
           fat_p::ObjectPool: median=    2.23 ns/op  mean=    2.37 +/-  0.48
          boost::object_pool: median=    2.49 ns/op  mean=    2.48 +/-  0.10
      foonathan::memory_pool: median=    2.98 ns/op  mean=    3.19 +/-  0.76
   EASTL::fixed_pool [!grow]: median=    2.06 ns/op  mean=    2.12 +/-  0.23
       std::pmr::unsync_pool: median=    6.21 ns/op  mean=    6.29 +/-  0.29
                  new/delete: median=   19.41 ns/op  mean=   19.47 +/-  0.54
  Speedup vs new/delete: 8.7x
[Cooling: before bulk acquire] [Ready: 2359 MHz]

================================================================================
  Bulk Acquire (SmallTrivial 16B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput


--- N = 1000 objects ---
[2026-02-01 19:08:33] CPU: 2359 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    1.90 ns/op  mean=    1.89 +/-  0.08
          boost::object_pool: median=    2.40 ns/op  mean=    2.45 +/-  0.16
      foonathan::memory_pool: median=    2.00 ns/op  mean=    1.98 +/-  0.20
   EASTL::fixed_pool [!grow]: median=    1.50 ns/op  mean=    1.54 +/-  0.05
       std::pmr::unsync_pool: median=    4.90 ns/op  mean=    5.01 +/-  0.48
                  new/delete: median=   14.30 ns/op  mean=   14.34 +/-  0.25

--- N = 10000 objects ---
[2026-02-01 19:08:44] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2322 MHz]
           fat_p::ObjectPool: median=    2.09 ns/op  mean=    2.23 +/-  0.31
          boost::object_pool: median=    9.16 ns/op  mean=    8.80 +/-  1.45
      foonathan::memory_pool: median=    1.98 ns/op  mean=    2.01 +/-  0.23
   EASTL::fixed_pool [!grow]: median=    1.70 ns/op  mean=    1.81 +/-  0.23
       std::pmr::unsync_pool: median=    9.23 ns/op  mean=    9.08 +/-  1.27
                  new/delete: median=   20.17 ns/op  mean=   20.63 +/-  1.97

--- N = 100000 objects ---
[2026-02-01 19:08:53] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2506 MHz]
           fat_p::ObjectPool: median=    2.43 ns/op  mean=    2.54 +/-  0.45
          boost::object_pool: median=    8.04 ns/op  mean=    8.04 +/-  0.36
      foonathan::memory_pool: median=    2.14 ns/op  mean=    2.34 +/-  0.41
   EASTL::fixed_pool [!grow]: median=    2.00 ns/op  mean=    2.09 +/-  0.35
       std::pmr::unsync_pool: median=    8.68 ns/op  mean=    8.70 +/-  0.66
                  new/delete: median=   23.45 ns/op  mean=   23.30 +/-  1.14
[Cooling: before interleaved] [Ready: 2211 MHz]

================================================================================
  Interleaved Acquire/Release (SmallTrivial 16B)
================================================================================
Contract: Realistic workload with interleaved operations
          50% acquire, 50% release (steady-state simulation)


--- N = 1000 operations ---
[2026-02-01 19:10:00] CPU: 2211 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
           fat_p::ObjectPool: median=    8.70 ns/op  mean=    8.83 +/-  0.76
          boost::object_pool: median=   11.20 ns/op  mean=   11.57 +/-  1.34
      foonathan::memory_pool: median=    8.70 ns/op  mean=    8.73 +/-  0.26
   EASTL::fixed_pool [!grow]: median=    7.70 ns/op  mean=    7.82 +/-  0.32
       std::pmr::unsync_pool: median=   12.00 ns/op  mean=   11.95 +/-  0.73
                  new/delete: median=   18.10 ns/op  mean=   18.45 +/-  1.00

--- N = 10000 operations ---
[2026-02-01 19:10:03] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2506 MHz]
           fat_p::ObjectPool: median=    7.95 ns/op  mean=    9.42 +/-  2.77
          boost::object_pool: median=   12.51 ns/op  mean=   15.13 +/-  7.66
      foonathan::memory_pool: median=    8.56 ns/op  mean=    9.26 +/-  1.74
   EASTL::fixed_pool [!grow]: median=    7.73 ns/op  mean=    8.42 +/-  1.30
       std::pmr::unsync_pool: median=   12.51 ns/op  mean=   12.84 +/-  1.93
                  new/delete: median=   18.15 ns/op  mean=   19.40 +/-  2.83

--- N = 100000 operations ---
[2026-02-01 19:10:20] CPU: 2211 MHz (base: 3686)
[Cooling: size transition] [Ready: 2580 MHz]
           fat_p::ObjectPool: median=    8.24 ns/op  mean=    8.28 +/-  0.37
          boost::object_pool: median=   13.05 ns/op  mean=   13.43 +/-  1.11
      foonathan::memory_pool: median=    8.74 ns/op  mean=    8.87 +/-  0.59
   EASTL::fixed_pool [!grow]: median=    7.71 ns/op  mean=    8.02 +/-  0.77
       std::pmr::unsync_pool: median=   10.59 ns/op  mean=   10.99 +/-  1.07
                  new/delete: median=   19.29 ns/op  mean=   19.70 +/-  1.34
[Cooling: before pool reuse] [Ready: 2580 MHz]

================================================================================
  Pool Reuse / Free List Efficiency (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all in random order, acquire N again
          Tests free list traversal and memory reuse


--- N = 1000 objects ---
[2026-02-01 19:10:50] CPU: 2580 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    1.90 ns/op  mean=    1.93 +/-  0.19
          boost::object_pool: median=    2.10 ns/op  mean=    2.06 +/-  0.06
      foonathan::memory_pool: median=    1.90 ns/op  mean=    1.92 +/-  0.14
   EASTL::fixed_pool [!grow]: median=    1.40 ns/op  mean=    1.48 +/-  0.20
       std::pmr::unsync_pool: median=    4.40 ns/op  mean=    4.50 +/-  0.35
                  new/delete: median=   14.80 ns/op  mean=   15.35 +/-  0.93

--- N = 10000 objects ---
[2026-02-01 19:10:53] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2690 MHz]
           fat_p::ObjectPool: median=    2.73 ns/op  mean=    2.79 +/-  0.15
          boost::object_pool: median=    2.16 ns/op  mean=    2.29 +/-  0.41
      foonathan::memory_pool: median=    2.44 ns/op  mean=    2.82 +/-  0.68
   EASTL::fixed_pool [!grow]: median=    2.14 ns/op  mean=    2.12 +/-  0.10
       std::pmr::unsync_pool: median=    8.44 ns/op  mean=    8.11 +/-  1.01
                  new/delete: median=   18.70 ns/op  mean=   19.87 +/-  4.90

--- N = 100000 objects ---
[2026-02-01 19:11:02] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2248 MHz]
           fat_p::ObjectPool: median=    9.97 ns/op  mean=   10.30 +/-  2.24
          boost::object_pool: median=    2.40 ns/op  mean=    2.44 +/-  0.13
      foonathan::memory_pool: median=    5.84 ns/op  mean=    6.64 +/-  2.26
   EASTL::fixed_pool [!grow]: median=    6.10 ns/op  mean=    6.41 +/-  1.61
       std::pmr::unsync_pool: median=    8.16 ns/op  mean=    8.43 +/-  1.15
                  new/delete: median=   22.51 ns/op  mean=   22.14 +/-  1.25
[Cooling: before pool reuse with compact] [Ready: 2285 MHz]

================================================================================
  Pool Reuse With Compaction (SmallTrivial 16B)
================================================================================
Contract: Acquire N, release all randomly, COMPACT, then acquire N again
          Tests whether compaction recovers allocation locality


--- N = 1000 objects ---
[2026-02-01 19:13:16] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2211 MHz]
           fat_p::ObjectPool: median=    2.30 ns/op  mean=    2.34 +/-  0.51
          boost::object_pool: median=    1.90 ns/op  mean=    2.03 +/-  0.31
      foonathan::memory_pool: median=    2.70 ns/op  mean=    2.64 +/-  0.76
   EASTL::fixed_pool [!grow]: median=    1.70 ns/op  mean=    1.67 +/-  0.43
       std::pmr::unsync_pool: median=    5.40 ns/op  mean=    5.69 +/-  1.17
                  new/delete: median=   15.40 ns/op  mean=   16.72 +/-  2.77

--- N = 10000 objects ---
[2026-02-01 19:13:29] CPU: 2211 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
           fat_p::ObjectPool: median=    2.16 ns/op  mean=    3.44 +/-  4.69
          boost::object_pool: median=    2.18 ns/op  mean=    2.52 +/-  1.07
      foonathan::memory_pool: median=    2.48 ns/op  mean=    2.58 +/-  0.28
   EASTL::fixed_pool [!grow]: median=    2.15 ns/op  mean=    2.35 +/-  0.95
       std::pmr::unsync_pool: median=    8.50 ns/op  mean=    8.69 +/-  1.96
                  new/delete: median=   18.39 ns/op  mean=   18.04 +/-  2.51

--- N = 100000 objects ---
[2026-02-01 19:13:34] CPU: 2469 MHz (base: 3686)
[Cooling: size transition] [Ready: 2248 MHz]
           fat_p::ObjectPool: median=    2.22 ns/op  mean=    2.24 +/-  0.12
          boost::object_pool: median=    2.36 ns/op  mean=    2.53 +/-  0.27
      foonathan::memory_pool: median=    4.97 ns/op  mean=    5.83 +/-  2.08
   EASTL::fixed_pool [!grow]: median=    5.81 ns/op  mean=    5.75 +/-  1.21
       std::pmr::unsync_pool: median=    7.75 ns/op  mean=    7.91 +/-  0.73
                  new/delete: median=   23.06 ns/op  mean=   22.42 +/-  1.83
[Cooling: before pool reuse full cycle] [Ready: 2285 MHz]

================================================================================
  Pool Reuse Full Cycle (SmallTrivial 16B)
================================================================================
Contract: Acquire N, then TIME [release all randomly + compact + reacquire all]
          True cost of 'flush and refill' pattern (no hidden work)
          Note: Boost's ordered_free O(N) cost is now visible


--- N = 1000 objects ---
[2026-02-01 19:15:35] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    4.80 ns/op  mean=    4.85 +/-  0.59
          boost::object_pool: median=  967.70 ns/op  mean= 1101.88 +/-355.86
      foonathan::memory_pool: median=    3.80 ns/op  mean=    3.90 +/-  0.31
   EASTL::fixed_pool [!grow]: median=    2.60 ns/op  mean=    2.75 +/-  0.36
       std::pmr::unsync_pool: median=    8.60 ns/op  mean=    8.84 +/-  0.86
                  new/delete: median=   25.20 ns/op  mean=   25.97 +/-  2.28

--- N = 10000 objects ---
[2026-02-01 19:15:51] CPU: 2432 MHz (base: 3686)
[Cooling: size transition] [Ready: 2174 MHz]
           fat_p::ObjectPool: median=    5.80 ns/op  mean=    5.91 +/-  0.38
          boost::object_pool: median= 2478.47 ns/op  mean= 3021.57 +/-947.02
      foonathan::memory_pool: median=    4.75 ns/op  mean=    4.97 +/-  0.58
   EASTL::fixed_pool [!grow]: median=    3.89 ns/op  mean=    4.14 +/-  0.54
       std::pmr::unsync_pool: median=   12.66 ns/op  mean=   13.78 +/-  3.39
                  new/delete: median=   22.51 ns/op  mean=   25.24 +/-  5.31

--- N = 100000 objects ---
[2026-02-01 19:15:57] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2395 MHz]
           fat_p::ObjectPool: median=    7.26 ns/op  mean=    7.59 +/-  0.66
          boost::object_pool: median=30381.57 ns/op  mean=30269.21 +/-372.46
      foonathan::memory_pool: median=    8.42 ns/op  mean=    8.77 +/-  1.18
   EASTL::fixed_pool [!grow]: median=    8.56 ns/op  mean=    8.75 +/-  1.70
       std::pmr::unsync_pool: median=   18.20 ns/op  mean=   17.78 +/-  2.54
                  new/delete: median=   44.05 ns/op  mean=   43.86 +/-  3.77
[Cooling: before medium object benchmarks] [Ready: 2395 MHz]

================================================================================
  Acquire + Release Cycle (MediumObject 64B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth


--- N = 1000 ops ---
[2026-02-01 19:17:53] CPU: 2395 MHz (base: 3686)
[Cooling: size transition] [Ready: 2875 MHz]
           fat_p::ObjectPool: median=    3.50 ns/op  mean=    3.93 +/-  0.90
          boost::object_pool: median=    4.00 ns/op  mean=    4.48 +/-  1.06
      foonathan::memory_pool: median=    4.60 ns/op  mean=    5.04 +/-  1.01
   EASTL::fixed_pool [!grow]: median=    3.00 ns/op  mean=    3.28 +/-  0.65
       std::pmr::unsync_pool: median=    8.80 ns/op  mean=   10.05 +/-  2.30
                  new/delete: median=   27.00 ns/op  mean=   29.69 +/-  5.98
  Speedup vs new/delete: 7.7x

--- N = 10000 ops ---
[2026-02-01 19:18:09] CPU: 2875 MHz (base: 3686)
[Cooling: size transition] [Ready: 2211 MHz]
           fat_p::ObjectPool: median=    2.65 ns/op  mean=    2.89 +/-  0.40
          boost::object_pool: median=    3.01 ns/op  mean=    3.23 +/-  0.39
      foonathan::memory_pool: median=    3.23 ns/op  mean=    3.45 +/-  0.41
   EASTL::fixed_pool [!grow]: median=    2.38 ns/op  mean=    2.48 +/-  0.28
       std::pmr::unsync_pool: median=    6.32 ns/op  mean=    6.71 +/-  0.75
                  new/delete: median=   21.02 ns/op  mean=   22.37 +/-  2.37
  Speedup vs new/delete: 7.9x

--- N = 50000 ops ---
[2026-02-01 19:18:25] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2543 MHz]
           fat_p::ObjectPool: median=    2.97 ns/op  mean=    3.22 +/-  0.74
          boost::object_pool: median=    3.12 ns/op  mean=    3.18 +/-  0.23
      foonathan::memory_pool: median=    3.24 ns/op  mean=    3.34 +/-  0.18
   EASTL::fixed_pool [!grow]: median=    2.35 ns/op  mean=    2.98 +/-  1.82
       std::pmr::unsync_pool: median=    6.69 ns/op  mean=    6.70 +/-  0.36
                  new/delete: median=   21.13 ns/op  mean=   22.15 +/-  2.23
  Speedup vs new/delete: 7.1x
[Cooling: before bulk acquire] [Ready: 2211 MHz]

================================================================================
  Bulk Acquire (MediumObject 64B)
================================================================================
Contract: Acquire N objects, then release all
          Tests sustained allocation throughput


--- N = 1000 objects ---
[2026-02-01 19:18:58] CPU: 2211 MHz (base: 3686)
[Cooling: size transition] [Ready: 2580 MHz]
           fat_p::ObjectPool: median=    2.80 ns/op  mean=    2.81 +/-  0.06
          boost::object_pool: median=    4.50 ns/op  mean=    4.57 +/-  0.44
      foonathan::memory_pool: median=    2.60 ns/op  mean=    2.67 +/-  0.12
   EASTL::fixed_pool [!grow]: median=    3.20 ns/op  mean=    3.27 +/-  0.14
       std::pmr::unsync_pool: median=    6.00 ns/op  mean=    6.33 +/-  0.63
                  new/delete: median=   16.00 ns/op  mean=   16.31 +/-  0.64

--- N = 10000 objects ---
[2026-02-01 19:19:14] CPU: 2580 MHz (base: 3686)
[Cooling: size transition] [Ready: 2174 MHz]
           fat_p::ObjectPool: median=    3.05 ns/op  mean=    3.10 +/-  0.26
          boost::object_pool: median=    5.06 ns/op  mean=    8.47 +/-  6.71
      foonathan::memory_pool: median=    3.02 ns/op  mean=    3.56 +/-  0.94
   EASTL::fixed_pool [!grow]: median=    2.87 ns/op  mean=    2.87 +/-  0.13
       std::pmr::unsync_pool: median=   20.71 ns/op  mean=   18.82 +/-  5.80
                  new/delete: median=   25.48 ns/op  mean=   28.43 +/-  8.90

--- N = 50000 objects ---
[2026-02-01 19:19:30] CPU: 2322 MHz (base: 3686)
[Cooling: size transition] [Ready: 2174 MHz]
           fat_p::ObjectPool: median=    3.33 ns/op  mean=    3.38 +/-  0.18
          boost::object_pool: median=   17.80 ns/op  mean=   18.61 +/-  2.09
      foonathan::memory_pool: median=    4.84 ns/op  mean=    5.38 +/-  1.21
   EASTL::fixed_pool [!grow]: median=    3.58 ns/op  mean=    3.85 +/-  0.86
       std::pmr::unsync_pool: median=   19.08 ns/op  mean=   20.47 +/-  2.76
                  new/delete: median=   20.17 ns/op  mean=   25.20 +/-  7.00
[Cooling: before large object benchmarks] [Ready: 2174 MHz]

================================================================================
  Acquire + Release Cycle (LargeObject 256B)
================================================================================
Contract: Single acquire followed by immediate release
          Measures raw pool overhead without allocation growth


--- N = 1000 ops ---
[2026-02-01 19:20:23] CPU: 2174 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
           fat_p::ObjectPool: median=    5.60 ns/op  mean=    5.66 +/-  0.11
          boost::object_pool: median=    5.70 ns/op  mean=    5.80 +/-  0.18
      foonathan::memory_pool: median=    5.80 ns/op  mean=    5.83 +/-  0.15
   EASTL::fixed_pool [!grow]: median=    5.30 ns/op  mean=    5.34 +/-  0.14
       std::pmr::unsync_pool: median=    7.90 ns/op  mean=    7.99 +/-  0.20
                  new/delete: median=   20.60 ns/op  mean=   20.88 +/-  0.56
  Speedup vs new/delete: 3.7x

--- N = 10000 ops ---
[2026-02-01 19:20:39] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
           fat_p::ObjectPool: median=    5.74 ns/op  mean=    5.81 +/-  0.39
          boost::object_pool: median=    6.13 ns/op  mean=    6.11 +/-  0.31
      foonathan::memory_pool: median=    6.12 ns/op  mean=    6.10 +/-  0.21
   EASTL::fixed_pool [!grow]: median=    5.48 ns/op  mean=    5.57 +/-  0.30
       std::pmr::unsync_pool: median=    8.96 ns/op  mean=    9.57 +/-  1.32
                  new/delete: median=   21.97 ns/op  mean=   23.14 +/-  1.93
  Speedup vs new/delete: 3.8x
[Cooling: before specialized acquire] [Ready: 3243 MHz]

================================================================================
  Specialized Acquire (SmallTrivial - fat_p only)
================================================================================
Contract: Compare acquire() vs acquire_uninitialized() vs acquire_zeroed()
          Shows overhead of zero-initialization and default construction

[2026-02-01 19:21:08] CPU: 3243 MHz (base: 3686)

--- N = 1000 objects ---
[Cooling: size transition] [Ready: 2359 MHz]
              acquire(value): median=    1.90 ns/op
     acquire_uninitialized(): median=    1.50 ns/op  (1.3x faster)
            acquire_zeroed(): median=     1.4 ns/op

--- N = 10000 objects ---
[Cooling: size transition] [Ready: 2469 MHz]
              acquire(value): median=    2.70 ns/op
     acquire_uninitialized(): median=    1.64 ns/op  (1.6x faster)
            acquire_zeroed(): median=     1.7 ns/op

--- N = 100000 objects ---
[Cooling: size transition] [Ready: 2285 MHz]
              acquire(value): median=    2.03 ns/op
     acquire_uninitialized(): median=    1.28 ns/op  (1.6x faster)
            acquire_zeroed(): median=     1.5 ns/op
[Cooling: before multithreaded] [Ready: 2322 MHz]

================================================================================
  Multi-threaded Contention (Thread-Safe Pools)
================================================================================
Contract: Concurrent acquire/release from multiple threads
          Tests lock contention and scalability
          Comparing: fat_p::ThreadSafeObjectPool vs std::pmr::synchronized_pool_resource

[2026-02-01 19:22:06] CPU: 2322 MHz (base: 3686)

--- 1 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2211 MHz]
       fat_p::ThreadSafePool: median=   95.38 ns/op  throughput=  10484378 ops/sec
         std::pmr::sync_pool: median=   78.29 ns/op  throughput=  12773023 ops/sec

--- 2 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2285 MHz]
       fat_p::ThreadSafePool: median=   96.23 ns/op  throughput=  10391230 ops/sec
         std::pmr::sync_pool: median=   59.15 ns/op  throughput=  16907600 ops/sec

--- 4 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2322 MHz]
       fat_p::ThreadSafePool: median=   57.80 ns/op  throughput=  17301038 ops/sec
         std::pmr::sync_pool: median=   31.08 ns/op  throughput=  32177620 ops/sec

--- 8 threads, 10000 ops/thread ---
[Cooling: thread count transition] [Ready: 2174 MHz]
       fat_p::ThreadSafePool: median=   73.64 ns/op  throughput=  13578654 ops/sec
         std::pmr::sync_pool: median=   79.08 ns/op  throughput=  12645422 ops/sec

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
  Benchmark Complete
================================================================================
```


---

# PolicyIterator

```
================================================================================
  fat_p::PolicyIterator Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::PolicyIterator (primary)
  [x] Raw pointer iteration (baseline)
  [x] Manual loop (baseline)
  [x] Boost.Iterator
  [x] range-v3
  [x] Eigen
  [x] xtensor
  [x] Boost.MultiArray

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement


Initial CPU state:
[2026-02-01 19:22:57] Benchmark start CPU: 2617 MHz (base: 3686)

================================================================================
  STANDARD ITERATION VS RAW POINTER
================================================================================

  Contract: Sequential sum accumulation, no predicate/transform

[2026-02-01 19:22:57] Start CPU: 2617 MHz (base: 3686)
  Results (ns/element):
    Raw pointer             :     0.59 ns/op  (+/-  0.02)
    PolicyIterator<Standard>:     0.60 ns/op  (+/-  0.01)

  Overhead ratio: 1.00x

================================================================================
  STRIDE POLICIES VS MANUAL LOOPS + RANGE-V3
================================================================================

  Contract: Sum with fixed compile-time stride, bounds clamping included

[2026-02-01 19:22:59] Start CPU: 2064 MHz (base: 3686)

  --- Stride 2 ---

    Manual loop             :     0.63 ns/op  (+/-  0.26)
    PolicyIterator<Stride>  :     0.74 ns/op  (+/-  0.18)
    range-v3::views::stride :     2.44 ns/op  (+/-  0.78)
    vs Manual: Policy 1.17x, range-v3 3.85x

  --- Stride 4 ---

    Manual loop             :     1.10 ns/op  (+/-  0.12)
    PolicyIterator<Stride>  :     1.23 ns/op  (+/-  0.34)
    range-v3::views::stride :     2.79 ns/op  (+/-  0.47)
    vs Manual: Policy 1.12x, range-v3 2.54x

  --- Stride 8 ---

    Manual loop             :     1.65 ns/op  (+/-  0.35)
    PolicyIterator<Stride>  :     1.72 ns/op  (+/-  0.36)
    range-v3::views::stride :     2.79 ns/op  (+/-  0.75)
    vs Manual: Policy 1.04x, range-v3 1.68x

================================================================================
  FILTER POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of even values only, predicate evaluation cost included

[2026-02-01 19:23:01] Start CPU: 2137 MHz (base: 3686)
  Match ratio: 50.0%

  Results (ns/element scanned):
    Manual if-check loop    :     3.05 ns/op  (+/-  1.14)
    PolicyIterator<Filter>  :     3.30 ns/op  (+/-  0.97)
    Boost.filter_iterator   :     3.25 ns/op  (+/-  0.90)
    range-v3::views::filter :     3.31 ns/op  (+/-  0.60)

  vs Manual:
    PolicyIterator: 1.08x
    Boost:          1.06x
    range-v3:       1.08x

================================================================================
  TRANSFORM POLICY VS MANUAL LOOP + BOOST + RANGE-V3
================================================================================

  Contract: Sum of doubled values, transform cost included

[2026-02-01 19:23:03] Start CPU: 3059 MHz (base: 3686)
  Results (ns/element):
    Manual transform loop   :     0.26 ns/op  (+/-  0.00)
    PolicyIterator<Transform>:     0.27 ns/op  (+/-  0.01)
    Boost.transform_iterator:     0.27 ns/op  (+/-  0.01)
    range-v3::views::transform:     0.26 ns/op  (+/-  0.00)

  vs Manual:
    PolicyIterator: 1.03x
    Boost:          1.02x
    range-v3:       1.01x

================================================================================
  TENSOR STRIDE POLICY VS MANUAL + EIGEN + XTENSOR + BOOST
================================================================================

  Contract: Row-major matrix column iteration (strided access)

[2026-02-01 19:23:05] Start CPU: 2211 MHz (base: 3686)
  Matrix: 1000 x 1000 (1000000 elements)
  Task: Sum first column (stride = 1000)

  Results (ns/row):
    Manual r*cols indexing  :     0.40 ns/op  (+/-  0.05)
    TensorStridePolicy      :     1.70 ns/op  (+/-  0.06)
    Stride1DPolicy          :     0.40 ns/op  (+/-  0.05)
    Eigen::col(0).sum()     :     0.30 ns/op  (+/-  0.05)
    xtensor strided_view    :     2.20 ns/op  (+/-  0.04)
    Boost.MultiArray [][0]  :     0.40 ns/op  (+/-  0.04)

  vs Manual:
    TensorStridePolicy: 4.25x
    Stride1DPolicy:     1.00x
    Eigen:              0.75x
    xtensor:            5.50x
    Boost:              1.00x

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 1.70 ns
    xtensor:            2.20 ns (0.77x)

================================================================================
  TENSOR: CONTIGUOUS VS NON-CONTIGUOUS VS SPECIALIZED + XTENSOR + BOOST
================================================================================

  Contract: Compare general TensorStridePolicy vs lightweight 1D/2D policies

[2026-02-01 19:23:07] Start CPU: 2174 MHz (base: 3686)
  Matrix: 1000 x 1000 (1000000 elements)

  Full matrix iteration (ns/element):
    Manual flat loop        :     0.25 ns/op  (+/-  0.11)
    TensorStridePolicy (general):     0.94 ns/op  (+/-  0.40)
    Stride2DPolicy (specialized):     0.26 ns/op  (+/-  0.11)
    xtensor iterator        :     0.38 ns/op  (+/-  0.18)
    Boost.MultiArray data() :     0.27 ns/op  (+/-  0.12)

  Column iteration (ns/row, stride=1000):
    TensorStridePolicy (general):     3.40 ns/op  (+/-  1.07)
    Stride1DPolicy (specialized):     0.50 ns/op  (+/-  0.23)
    xtensor strided_view    :     4.50 ns/op  (+/-  1.41)

  vs Manual (full matrix):
    TensorStridePolicy: 3.78x
    Stride2DPolicy:     1.06x
    xtensor:            1.53x
    Boost:              1.09x

  Stride1D vs TensorStride (column iteration):
    TensorStridePolicy: 3.40 ns/row
    Stride1DPolicy:     0.50 ns/row (0.15x)

  TensorStridePolicy vs xtensor (strided iteration):
    TensorStridePolicy: 3.40 ns
    xtensor:            4.50 ns (0.76x)

================================================================================
  2D MATRIX FULL ITERATION: POLICIES VS EIGEN
================================================================================

  Contract: Sum all elements of 1000x1000 row-major matrix

[2026-02-01 19:23:09] Start CPU: 2359 MHz (base: 3686)
  Matrix: 1000 x 1000

  Results (ns/element):
    Manual flat loop        :     0.50 ns/op  (+/-  0.13)
    Stride2DPolicy          :     0.48 ns/op  (+/-  0.12)
    TensorStridePolicy 2D   :     1.90 ns/op  (+/-  0.45)
    Eigen::sum()            :     0.31 ns/op  (+/-  0.08)
    Eigen coeff (r,c) loop  :     0.46 ns/op  (+/-  0.12)

  vs Manual:
    Stride2DPolicy:     0.97x
    TensorStridePolicy: 3.80x
    Eigen::sum():       0.63x
    Eigen coeff loop:   0.92x

================================================================================
  TENSOR ITERATION COMPOSITION HELPERS
================================================================================

  Contract: Compare iterateND composition vs manual loops vs TensorStridePolicy

[2026-02-01 19:23:11] Start CPU: 2248 MHz (base: 3686)
  3D Tensor: 50 x 100 x 200 (1000000 elements)
  4D Tensor: 8 x 16 x 50 x 100 (640000 elements)

  3D Results (ns/element):
    Manual nested loop      :     0.24 ns/op  (+/-  0.03)
    TensorStridePolicy      :     1.32 ns/op  (+/-  0.32)
    iterateND (composition) :     0.49 ns/op  (+/-  0.15)

  3D vs Manual:
    TensorStridePolicy:     5.58x
    iterateND (composition): 2.06x

  4D Results (ns/element):
    Manual nested loop      :     0.25 ns/op  (+/-  0.07)
    TensorStridePolicy      :     0.90 ns/op  (+/-  0.14)
    iterateND (composition) :     0.52 ns/op  (+/-  0.14)

  4D vs Manual:
    TensorStridePolicy:     3.60x
    iterateND (composition): 2.06x

  Composition vs TensorStridePolicy:
    3D: iterateND is 2.71x faster
    4D: iterateND is 1.74x faster

================================================================================
  SIZE SCALING (CACHE EFFECTS)
================================================================================

  Contract: Standard iteration sum at different data sizes

[2026-02-01 19:23:13] Start CPU: 2543 MHz (base: 3686)

  --- L1 (4K) (512 elements) ---

    Raw pointer             :     0.20 ns/op  (+/-  0.07)
    PolicyIterator          :     0.20 ns/op  (+/-  0.07)
    Overhead ratio: 1.00x

  --- L2 (32K) (4096 elements) ---

    Raw pointer             :     0.20 ns/op  (+/-  0.01)
    PolicyIterator          :     0.20 ns/op  (+/-  0.01)
    Overhead ratio: 1.00x

  --- L3 (512K) (65536 elements) ---

    Raw pointer             :     0.19 ns/op  (+/-  0.00)
    PolicyIterator          :     0.19 ns/op  (+/-  0.00)
    Overhead ratio: 1.00x

  --- RAM (8M) (1048576 elements) ---

    Raw pointer             :     0.25 ns/op  (+/-  0.02)
    PolicyIterator          :     0.26 ns/op  (+/-  0.01)
    Overhead ratio: 1.01x


================================================================================
  BENCHMARK COMPLETE
================================================================================

[2026-02-01 19:23:13] Final state CPU: 2911 MHz (base: 3686)
```


---

# ServiceLocator

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::ServiceLocator Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::DefaultServiceLocator (primary)
  [x] fat_p::ThreadSafeServiceLocator (primary)
  [x] std::unordered_map<type_index> (baseline)
  [x] Direct pointer (baseline)
  [ ] entt::locator (vcpkg install entt)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement

Verifying adapters...
  [OK] All adapters verified


================================================================================
  SINGLE-TYPE RESOLUTION
================================================================================

Contract: Resolve one service type. O(1) hash lookup for map-based, O(1) static access for EnTT.

[2026-02-01 19:23:13] Starting CPU: 2948 MHz (base: 3686)
  fat_p::DefaultServiceLocator (no stats): median=    2.87 mean=    2.89 +/-  0.07 [    2.85,     2.93]
  fat_p::DefaultServiceLocator (atomic stats): median=    5.20 mean=    5.28 +/-  0.28 [    5.14,     5.43]
  fat_p::ThreadSafeServiceLocator (no stats): median=   10.91 mean=   11.58 +/-  1.13 [   11.01,    12.15]
  fat_p::ThreadSafeServiceLocator (atomic stats): median=   13.98 mean=   14.81 +/-  1.55 [   14.03,    15.59]
  entt::locator (static global)      : median=    1.20 mean=    1.24 +/-  0.12 [    1.18,     1.31]
  std::unordered_map<type_index>     : median=   11.99 mean=   11.99 +/-  0.02 [   11.98,    12.00]
  Direct pointers (baseline)         : median=    1.18 mean=    1.20 +/-  0.04 [    1.18,     1.22]


================================================================================
  MULTI-TYPE RESOLUTION (5 types)
================================================================================

Contract: Resolve 5 different service types per iteration. Measures cumulative lookup cost.

[2026-02-01 19:23:14] Starting CPU: 3317 MHz (base: 3686)
  fat_p::DefaultServiceLocator (no stats): median=    2.83 mean=    2.95 +/-  0.74 [    2.57,     3.33]
  fat_p::DefaultServiceLocator (atomic stats): median=    5.22 mean=    6.97 +/-  2.85 [    5.53,     8.41]
  fat_p::ThreadSafeServiceLocator (no stats): median=   10.88 mean=   11.58 +/-  1.45 [   10.85,    12.31]
  fat_p::ThreadSafeServiceLocator (atomic stats): median=   13.98 mean=   14.38 +/-  0.87 [   13.94,    14.82]
  entt::locator (static global)      : median=    1.14 mean=    1.15 +/-  0.03 [    1.13,     1.16]
  std::unordered_map<type_index>     : median=    8.79 mean=    8.92 +/-  0.32 [    8.76,     9.08]
  Direct pointers (baseline)         : median=    1.14 mean=    1.20 +/-  0.17 [    1.12,     1.29]


================================================================================
  NAMED SERVICES (fat_p exclusive)
================================================================================

Contract: Resolve services by type+name composite key. Competitors do not support named services.

[2026-02-01 19:23:14] Starting CPU: 2875 MHz (base: 3686)
  resolve<ILogger>() (default)       : median=    3.08 mean=    3.08 +/-  0.16 [    3.00,     3.16]
  resolve<ILogger>("file")           : median=    6.78 mean=    6.96 +/-  0.59 [    6.66,     7.25]
  resolve 3 named variants           : median=    5.63 mean=    5.91 +/-  0.57 [    5.62,     6.20]


================================================================================
  SCOPED RESOLUTION (fat_p exclusive)
================================================================================

Contract: Child scope overrides parent. Measures lookup with parent chain traversal.

[2026-02-01 19:23:14] Starting CPU: 2801 MHz (base: 3686)
  resolve (child override)           : median=    1.30 mean=    1.29 +/-  0.03 [    1.28,     1.31]
  resolve (parent inheritance)       : median=    3.47 mean=    3.47 +/-  0.06 [    3.44,     3.50]


================================================================================
  REGISTRATION PERFORMANCE
================================================================================

Contract: Register 5 services including hash map insertion. Measures setup cost.

[2026-02-01 19:23:14] Starting CPU: 2801 MHz (base: 3686)
  fat_p::DefaultServiceLocator       : median=   41.40 mean=   42.65 +/-  2.51 [   41.38,    43.92]
  std::unordered_map<type_index>     : median=   34.03 mean=   35.48 +/-  2.39 [   34.27,    36.69]


================================================================================
  SIZE SENSITIVITY
================================================================================

Contract: Measure resolve performance as number of registered services scales. Split into unnamed (type-only) and named (type+name) variants.

[2026-02-01 19:23:14] Starting CPU: 2359 MHz (base: 3686)
UNNAMED (TYPE-ONLY)
Services | fat_p median (ns/op) | unordered_map<void*> median (ns/op) | Ratio (unordered_map / fat_p)
---------|----------------------|-----------------------------------|---------------------------
       1 |                 3.19 |                              3.09 |  0.97x
       5 |                 3.06 |                              3.08 |  1.01x
      10 |                 3.08 |                              3.09 |  1.00x
      25 |                 3.09 |                              3.10 |  1.01x
      50 |                 3.33 |                              3.10 |  0.93x
     100 |                 3.76 |                              3.11 |  0.83x

NAMED (TYPE+NAME)
Note: The string-only unordered_map is a name-only key, not a composite (type+name) key.
      The composite-key unordered_map is the apples-to-apples comparator.
Services | fat_p median (ns/op) | unordered_map<string> median (ns/op) | unordered_map<composite> median (ns/op) | Ratio (composite / fat_p)
---------|----------------------|----------------------------------|----------------------------------|---------------------------
       1 |                21.46 |                             4.36 |                             5.50 |  0.26x
       5 |                21.05 |                             4.26 |                             5.50 |  0.26x
      10 |                20.83 |                             4.34 |                             5.52 |  0.27x
      25 |                21.04 |                             6.55 |                             7.28 |  0.35x
      50 |                21.43 |                             7.31 |                             7.72 |  0.36x
     100 |                23.36 |                             8.53 |                             8.44 |  0.36x


================================================================================
  MRU RESOLVE CACHE LOCALITY (fat_p optional)
================================================================================

Contract: Hot-loop repeated resolves after startup registration. Measures steady-state resolve cost for
type-only (unnamed) services while varying the total number of registered services.
Patterns:
  1) Repeat A: tryResolve<A>() repeatedly.
  2) Alternate A/B: tryResolve<A>(), tryResolve<B>(), ...
Comparator: DefaultServiceLocator (no cache) vs HotLoopServiceLocator (MRU2).


[2026-02-01 19:23:14] Starting CPU: 2432 MHz (base: 3686)
REPEAT A (AAAA...)
Services | Default median (ns/op) | MRU2 median (ns/op) | Speedup (Default/MRU2)
---------|------------------------|--------------------|----------------------
       2 |                   2.59 |               1.52 |                 1.71
       5 |                   2.63 |               1.52 |                 1.74
      10 |                   2.60 |               1.52 |                 1.71
      25 |                   2.61 |               1.51 |                 1.73
      50 |                   2.47 |               1.53 |                 1.61
     100 |                   2.61 |               1.51 |                 1.73

ALTERNATE A/B (ABAB...)
Services | Default median (ns/op) | MRU2 median (ns/op) | Speedup (Default/MRU2)
---------|------------------------|--------------------|----------------------
       2 |                   2.59 |               2.07 |                 1.25
       5 |                   2.58 |               2.09 |                 1.24
      10 |                   2.58 |               2.09 |                 1.24
      25 |                   2.59 |               2.08 |                 1.24
      50 |                   2.39 |               2.07 |                 1.16
     100 |                   2.52 |               2.07 |                 1.22

CYCLE A/B/C (ABCABC...)
Services | Default median (ns/op) | MRU2 median (ns/op) | Speedup (Default/MRU2)
---------|------------------------|--------------------|----------------------
       3 |                   2.58 |               3.42 |                 0.75
       5 |                   2.58 |               3.44 |                 0.75
      10 |                   2.60 |               3.44 |                 0.75
      25 |                   2.58 |               3.42 |                 0.76
      50 |                   2.35 |               3.32 |                 0.71
     100 |                   2.50 |               3.35 |                 0.75


================================================================================
  STRING KEY HOT LOOP (named services)
================================================================================

Contract: Hot-loop resolves by type+name (string key) after startup registration.
Measures steady-state cost while varying:
  A) Name length (bytes) at fixed named-variant count.
  B) Named-variant count at fixed name length.
Comparator: DefaultServiceLocator vs HotLoopServiceLocator (MRU2).
Note: The MRU cache is type-only; named lookups should not benefit (this is intentional to measure).


[2026-02-01 19:23:15] Starting CPU: 2690 MHz (base: 3686)
NAME LENGTH SWEEP (variants=100)
Len | Default AAAA | MRU2 AAAA | Default ABAB | MRU2 ABAB
----|--------------|----------|--------------|----------
  4 |         6.76 |     6.92 |        12.37 |     6.94
  8 |         7.85 |     7.86 |        13.04 |     7.88
 16 |        11.51 |    11.48 |        17.33 |    11.64
 32 |        20.49 |    21.51 |        28.39 |    22.03
 64 |        38.09 |    39.16 |        52.93 |    39.14
128 |        90.20 |    89.60 |       104.29 |    89.63

VARIANT COUNT SWEEP (len=16)
N   | Default AAAA | MRU2 AAAA | Default ABAB | MRU2 ABAB
----|--------------|----------|--------------|----------
  1 |        11.28 |    11.43 |        17.28 |    11.62
  5 |        11.74 |    11.73 |        17.70 |    11.81
 10 |        11.71 |    11.79 |        17.63 |    12.06
 25 |        11.28 |    11.32 |        17.35 |    12.07
 50 |        11.21 |    11.27 |        17.27 |    11.30
100 |        11.38 |    11.10 |        17.34 |    12.19
500 |        11.45 |    11.49 |        17.89 |    11.88


================================================================================
  CONST RESOLVE (T vs const T)
================================================================================

Contract: Measures steady-state cost of tryResolve<T>() vs tryResolve<const T>() for unnamed services.
TypeKeyPolicy removes cv-qualifiers, so both resolve paths share the same type id.
Comparator: DefaultServiceLocator (no cache) vs HotLoopServiceLocator (MRU2).


[2026-02-01 19:23:21] Starting CPU: 2653 MHz (base: 3686)
Locator | tryResolve<T> median (ns/op) | tryResolve<const T> median (ns/op) | Ratio
--------|----------------------------|----------------------------------|------
Default |                       2.56 |                             2.65 | 0.96x
HotLoop |                       1.26 |                             1.26 | 1.00x


================================================================================
  MUTATION COST (unregister / clear)
================================================================================

Contract: Measures registry mutation cost after services have been registered.
  A) unregister N distinct service types (ns/op).
  B) clear N service types (ns/op per entry).
Comparator: DefaultServiceLocator vs HotLoopServiceLocator.


[2026-02-01 19:23:21] Starting CPU: 2432 MHz (base: 3686)
UNREGISTER (ns/op)
N   | Default median | HotLoop median
----|----------------|--------------
  1 |           0.00 |         0.00
  5 |          20.00 |        20.00
 10 |          30.00 |        20.00
 25 |          20.00 |        20.00
 50 |          18.00 |        18.00
100 |          16.00 |        15.00

CLEAR (ns/op per entry)
N   | Default median | HotLoop median
----|----------------|--------------
  1 |           0.00 |         0.00
  5 |          20.00 |        20.00
 10 |          20.00 |        20.00
 25 |          12.00 |        12.00
 50 |          10.00 |        10.00
100 |          10.00 |        10.00


================================================================================
  OVERHEAD ISOLATION MICRO-BENCHMARKS
================================================================================

Contract: Isolate individual overhead components to identify optimization targets.

[2026-02-01 19:23:21] Starting CPU: 2432 MHz (base: 3686)
Measuring individual overhead components...

  1. Direct pointer access (baseline)          : 1.18 ns/op
  2. std::type_index construction              : 9.93 ns/op
  3. fat_p TypeKeyPolicy (static addr)         : 1.14 ns/op
  4. std::string construction (empty)          : 1.49 ns/op
  5. std::string from string_view (empty)      : 2.49 ns/op
  6. ServiceKey-like struct construction       : 7.75 ns/op
  7. ServiceKeyHash computation                : 1.99 ns/op
  8. unordered_map<type_index>.find()          : 7.58 ns/op
  9. unordered_map<void*>.find() (optimal)     : 2.55 ns/op
  10. ServiceKey map find (with makeKey)       : 9.35 ns/op
  11. std::optional construction + check       : 1.42 ns/op
  12. fat_p::DefaultServiceLocator.tryResolve  : 2.65 ns/op

Analysis:
  - Items 6+10 show ServiceKey construction + lookup overhead
  - Compare item 9 (optimal) vs item 10 (current) for improvement potential
  - Item 4-5 show std::string allocation overhead (even for empty strings)

Detailed gap analysis (tryResolve breakdown)...

  13. makeKey<T>() simulation                  : 1.19 ns/op
  14. SingleThreadedPolicy lock_shared()       : 1.17 ns/op
  15. SharedMutexPolicy lock_shared()          : 11.43 ns/op
  16. std::function copy                       : 1.83 ns/op
  17. std::shared_ptr<void> copy               : 8.93 ns/op
  18. std::optional<SnapLike> construction     : 1.15 ns/op
  19. FullSnapshot copy (Instance path)        : 1.14 ns/op
  20. FullSnapshot copy (Factory path)         : 2.05 ns/op
  21. Expected<ref_wrapper> construction       : 1.30 ns/op
  22. resolveEntryForRead simulation           : 7.24 ns/op
  23. tryResolve simulation (no snapshot)      : 2.57 ns/op
  24. fat_p tryResolve (actual)                : 2.63 ns/op

Gap Analysis Summary:
  Compare items 22-24 to identify where overhead accumulates.
  Item 23 vs 24 shows cost of snapshot copy + Expected wrapper.
  Item 16 (std::function copy) is often the hidden culprit.

StableHashMap comparison (reference stability)...

  25. StableHashMap<void*> find (no copy, SM64): 1.26 ns/op
  26. StableHashMap<ServiceKey> find           : 7.22 ns/op
  27. Optimal tryResolve (StableHashMap)       : 1.18 ns/op
  28. Current fat_p tryResolve                 : 2.69 ns/op

StableHashMap Advantage:
  - Reference stability eliminates snapshot copy (~10ns saved)
  - SIMD-accelerated probing (faster than std::unordered_map)
  - No shared_ptr atomic refcount overhead on resolve
  - Compare #27 vs #28 for potential improvement

Zero-cost abstraction verification...

  29. Raw StableHashMap (no lock)              : 1.20 ns/op
  30. With SingleThreadedPolicy lock           : 1.19 ns/op
  31. ServiceLocator tryResolve (optimized)    : 2.74 ns/op
  32. Minimal resolve (just hash lookup)       : 1.25 ns/op
  33. Static global (EnTT-style)               : 1.17 ns/op

If #29 == #30, SingleThreadedPolicy is truly zero-cost.
Gap between #32 and #33 is the irreducible hash lookup cost.


================================================================================
  ALTERNATIVE KEY STRATEGIES
================================================================================

Contract: Compare different key designs to identify optimization opportunities.

[2026-02-01 19:23:21] Starting CPU: 2690 MHz (base: 3686)
Testing alternative ServiceKey designs...

  Strategy 1: void* + std::string (current)         : 3.46 ns/op
  Strategy 2: void* + string_view (zero-alloc)      : 2.37 ns/op
  Strategy 3: void* only (no names)                 : 1.25 ns/op
  Strategy 4: Cached hash (still allocates string)  : 2.65 ns/op
  Strategy 5: Two-level map (unnamed fast path)     : 1.29 ns/op
  Strategy 6: std::type_index (no names)            : 7.80 ns/op

Recommendations:
  - Strategy 3/5 show potential for unnamed services (~2x faster)
  - Strategy 2 eliminates allocation but requires API changes
  - Consider two-tier storage: fast path for type-only, slow path for named


================================================================================
  CONCURRENT RESOLUTION
================================================================================

Contract: Multi-threaded read-only resolution. Thread-safe variants only.

[2026-02-01 19:23:21] Starting CPU: 2690 MHz (base: 3686)
Thread count: 24, ops/thread: 100000

  fat_p::ThreadSafeServiceLocator (no stats): median=  147.40 mean=  151.66 +/- 11.95 [  145.61,   157.71]
  fat_p::ThreadSafeServiceLocator (atomic stats): median=  161.14 mean=  160.87 +/-  7.62 [  157.02,   164.73]
  unordered_map<void*> + shared_mutex (type key): median=  123.76 mean=  129.13 +/- 12.09 [  123.01,   135.25]
  StableHashMap<void*> + shared_mutex (type key, SM64): median=  104.99 mean=  105.92 +/-  5.30 [  103.24,   108.60]
  unordered_map<type_index> + shared_mutex (precomputed key): median=  137.10 mean=  142.02 +/- 14.75 [  134.55,   149.49]

================================================================================
  Benchmark Complete
================================================================================
```


---

# SlotMap

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::SlotMap Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::SlotMap (primary)
  [x] entt::registry
  [x] plf::hive
  [x] sg14::slot_map
  [x] std::unordered_map (baseline)
  [x] std::map (baseline)
  [x] std::vector (raw) (baseline)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement

Cooling: section=2000ms size=1000ms case=300ms

Expected Results:
  - SlotMap excels at: iteration (dense storage), O(1) operations, ABA safety
  - EnTT: similar design, optimized for ECS patterns
  - plf::hive: stable pointers, good iteration, no handle safety
  - unordered_map: fast lookup but scattered iteration
  - std::map: consistent O(log n) but slowest overall
  - std::vector: fastest raw access but no handle safety

Checking initial CPU state...
[2026-02-01 19:23:51] Initial CPU: 3022 MHz (base: 3686)
Waiting for CPU to stabilize...
[WARNING: CPU frequency still unstable after 30s]
WARNING: CPU frequency still fluctuating, results may have higher variance.


================================================================================
  SECTION 1: Core Operations (Insert, Access, Erase)
================================================================================
[2026-02-01 19:24:21] Section start CPU: 2506 MHz (base: 3686)

--- N = 1000 ---
[2026-02-01 19:24:21] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2432 MHz]
  Sequential Insert:
              fat_p::SlotMap:    10.40 ns/op (+/-  0.09)
              entt::registry:    17.60 ns/op (+/-  1.46)
                   plf::hive:     6.70 ns/op (+/-  0.15)
              sg14::slot_map:    13.40 ns/op (+/-  0.11)
          std::unordered_map:    20.20 ns/op (+/-  0.19)
                    std::map:    32.50 ns/op (+/-  1.24)
           std::vector (raw):     8.00 ns/op (+/-  0.05)
  Random Access (valid):
              fat_p::SlotMap:     1.90 ns/op (+/-  0.05)
              entt::registry:     5.00 ns/op (+/-  0.67)
                   plf::hive:     1.60 ns/op (+/-  0.05)
              sg14::slot_map:     2.10 ns/op (+/-  0.04)
          std::unordered_map:     5.60 ns/op (+/-  0.21)
                    std::map:    23.50 ns/op (+/-  2.21)
           std::vector (raw):     1.60 ns/op (+/-  0.04)
  Erase (25%):
              fat_p::SlotMap:     6.00 ns/op (+/-  0.22)
              entt::registry:    34.00 ns/op (+/-  1.36)
                   plf::hive:     8.00 ns/op (+/-  0.71)
              sg14::slot_map:     6.00 ns/op (+/-  0.80)
          std::unordered_map:    18.00 ns/op (+/- 46.93)
                    std::map:    44.00 ns/op (+/-304.39)
           std::vector (raw):     2.00 ns/op (+/-  0.21)

--- N = 10000 ---
[2026-02-01 19:25:03] CPU: 2359 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
  Sequential Insert:
              fat_p::SlotMap:    15.01 ns/op (+/-  1.99)
              entt::registry:    30.02 ns/op (+/-  1.15)
                   plf::hive:    12.03 ns/op (+/-  1.95)
              sg14::slot_map:    18.33 ns/op (+/- 10.21)
          std::unordered_map:    42.11 ns/op (+/-  6.13)
                    std::map:    66.93 ns/op (+/-  8.38)
           std::vector (raw):    12.22 ns/op (+/-  0.47)
  Random Access (valid):
              fat_p::SlotMap:     2.69 ns/op (+/-  0.14)
              entt::registry:     6.07 ns/op (+/-  0.04)
                   plf::hive:     2.03 ns/op (+/-  0.09)
              sg14::slot_map:     2.81 ns/op (+/-  0.11)
          std::unordered_map:    13.23 ns/op (+/-  0.70)
                    std::map:    67.36 ns/op (+/-  8.42)
           std::vector (raw):     1.71 ns/op (+/-  0.08)
  Erase (25%):
              fat_p::SlotMap:     7.44 ns/op (+/-  1.84)
              entt::registry:    38.20 ns/op (+/-  6.93)
                   plf::hive:     9.52 ns/op (+/- 30.24)
              sg14::slot_map:     6.16 ns/op (+/-  0.23)
          std::unordered_map:    29.68 ns/op (+/- 32.12)
                    std::map:   143.40 ns/op (+/- 18.32)
           std::vector (raw):     0.60 ns/op (+/-  0.03)

--- N = 100000 ---
[2026-02-01 19:25:53] CPU: 2359 MHz (base: 3686)
[Cooling: size transition] [Ready: 2543 MHz]
  Sequential Insert:
              fat_p::SlotMap:    15.46 ns/op (+/-  0.87)
              entt::registry:    29.38 ns/op (+/-  2.29)
                   plf::hive:    11.53 ns/op (+/-  0.89)
              sg14::slot_map:    17.84 ns/op (+/-  3.04)
          std::unordered_map:    49.72 ns/op (+/-  4.28)
                    std::map:    77.93 ns/op (+/-  3.73)
           std::vector (raw):    12.11 ns/op (+/-  1.45)
  Random Access (valid):
              fat_p::SlotMap:    11.62 ns/op (+/-  3.55)
              entt::registry:    15.29 ns/op (+/-  3.53)
                   plf::hive:     7.88 ns/op (+/-  2.57)
              sg14::slot_map:     7.76 ns/op (+/-  2.40)
          std::unordered_map:    17.59 ns/op (+/-  3.86)
                    std::map:   169.11 ns/op (+/-  4.63)
           std::vector (raw):     4.39 ns/op (+/-  1.95)
  Erase (25%):
              fat_p::SlotMap:    30.05 ns/op (+/-  7.98)
              entt::registry:    61.23 ns/op (+/-  8.57)
                   plf::hive:    20.77 ns/op (+/-  6.16)
              sg14::slot_map:    15.48 ns/op (+/-  3.07)
          std::unordered_map:    43.63 ns/op (+/- 10.77)
                    std::map:   280.60 ns/op (+/- 17.66)
           std::vector (raw):     0.50 ns/op (+/-  0.41)
[Cooling: before ABA safety test] [Ready: 3059 MHz]

================================================================================
  SECTION 2: ABA Safety Test (Generational Index Validation)
================================================================================
[2026-02-01 19:27:13] Section start CPU: 3059 MHz (base: 3686)

  Testing fat_p::SlotMap ABA protection:
    Erased handles correctly invalidated: 5000/5000 [PASS]
    Old handles invalid after slot reuse: 5000/5000 [PASS]
    New handles valid: 10000/10000 [PASS]

  Testing entt::registry ABA protection:
    Destroyed entities correctly invalidated: 5000/5000 [PASS]
[Cooling: before iteration benchmark] [Ready: 2248 MHz]

================================================================================
  SECTION 3: Iteration Speed (Dense Storage Advantage)
================================================================================
[2026-02-01 19:27:24] Section start CPU: 2248 MHz (base: 3686)

--- N = 1000 ---
[2026-02-01 19:27:24] CPU: 2248 MHz (base: 3686)
[Cooling: size transition] [Ready: 2359 MHz]
  Iteration:
              fat_p::SlotMap:     1.90 ns/op (+/-  0.44)
              entt::registry:     2.10 ns/op (+/-  0.55)
                   plf::hive:     2.20 ns/op (+/-  0.58)
          std::unordered_map:     2.70 ns/op (+/-  0.71)
                    std::map:     4.10 ns/op (+/-  1.09)
           std::vector (raw):     1.90 ns/op (+/-  0.50)

--- N = 10000 ---
[2026-02-01 19:27:40] CPU: 2359 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
  Iteration:
              fat_p::SlotMap:     1.22 ns/op (+/-  0.03)
              entt::registry:     1.40 ns/op (+/-  0.04)
                   plf::hive:     1.45 ns/op (+/-  0.07)
          std::unordered_map:     2.90 ns/op (+/-  0.05)
                    std::map:     3.97 ns/op (+/-  7.76)
           std::vector (raw):     1.22 ns/op (+/-  0.04)

--- N = 100000 ---
[2026-02-01 19:27:52] CPU: 2248 MHz (base: 3686)
[Cooling: size transition] [Ready: 2359 MHz]
  Iteration:
              fat_p::SlotMap:     2.03 ns/op (+/-  0.53)
              entt::registry:     1.60 ns/op (+/-  0.83)
                   plf::hive:     2.30 ns/op (+/-  0.49)
          std::unordered_map:     6.52 ns/op (+/-  0.92)
                    std::map:     8.02 ns/op (+/-  6.31)
           std::vector (raw):     2.50 ns/op (+/-  0.36)

--- N = 500000 ---
[2026-02-01 19:28:07] CPU: 2432 MHz (base: 3686)
[Cooling: size transition] [Ready: 2285 MHz]
  Iteration:
              fat_p::SlotMap:     2.22 ns/op (+/-  0.79)
              entt::registry:     3.45 ns/op (+/-  0.55)
                   plf::hive:     3.56 ns/op (+/-  0.80)
          std::unordered_map:    26.30 ns/op (+/-  4.42)
                    std::map:    20.10 ns/op (+/-  3.79)
           std::vector (raw):     3.34 ns/op (+/-  0.93)
[Cooling: before mixed workload] [Ready: 2617 MHz]

================================================================================
  SECTION 4: Mixed Workload (Insert/Access/Erase Interleaved)
================================================================================
[2026-02-01 19:28:32] Section start CPU: 2617 MHz (base: 3686)

--- N = 1000 ---
[2026-02-01 19:28:32] CPU: 2617 MHz (base: 3686)
[Cooling: size transition] [Ready: 2506 MHz]
  Mixed Workload:
              fat_p::SlotMap:     8.80 ns/op (+/-  0.50)
              entt::registry:    26.40 ns/op (+/-  1.95)
                   plf::hive:     7.60 ns/op (+/-  0.38)
          std::unordered_map:    16.00 ns/op (+/-  0.80)
                    std::map:    22.00 ns/op (+/-  2.35)
           std::vector (raw):     7.20 ns/op (+/-  4.71)

--- N = 10000 ---
[2026-02-01 19:28:35] CPU: 2506 MHz (base: 3686)
[Cooling: size transition] [Ready: 2432 MHz]
  Mixed Workload:
              fat_p::SlotMap:     7.12 ns/op (+/-  0.38)
              entt::registry:    16.88 ns/op (+/-  0.54)
                   plf::hive:     6.52 ns/op (+/-  0.37)
          std::unordered_map:    16.00 ns/op (+/-  0.68)
                    std::map:    46.72 ns/op (+/-  1.33)
           std::vector (raw):     5.56 ns/op (+/-  0.04)

--- N = 50000 ---
[2026-02-01 19:28:41] CPU: 2432 MHz (base: 3686)
[Cooling: size transition] [Ready: 2653 MHz]
  Mixed Workload:
              fat_p::SlotMap:     9.59 ns/op (+/-  1.02)
              entt::registry:    17.04 ns/op (+/-  1.57)
                   plf::hive:     9.62 ns/op (+/-  1.41)
          std::unordered_map:    17.26 ns/op (+/-  1.26)
                    std::map:    65.06 ns/op (+/- 12.94)
           std::vector (raw):     7.58 ns/op (+/-  6.35)

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
  Benchmark Complete
================================================================================
```


---

# SmallVector

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::SmallVector Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::SmallVector (primary)
  [x] std::vector (baseline)
  [ ] boost::container::small_vector (vcpkg install boost-container)
  [ ] llvm::SmallVector (apt install llvm-dev)
  [ ] absl::InlinedVector (vcpkg install abseil)
  [ ] folly::small_vector (not detected)
  [ ] ankerl::svector (not detected)
  [ ] eastl::fixed_vector (vcpkg install eastl)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement


================================================================================
  CORE OPERATIONS
================================================================================

Comparing fundamental vector operations.
All containers pre-reserved to target size.


--- N = 100 ---

[2026-02-01 19:28:56] Starting CPU: 2322 MHz (base: 3686)
push_back:
       SmallVector<16>: median=    1.00 mean=    0.87 +/-  0.35 min=0.00 max=1.00
           std::vector: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    0.60 +/-  0.51 min=0.00 max=1.00
  llvm::SmallVector<16>: median=    1.00 mean=    1.00 +/-  0.65 min=0.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    0.87 +/-  0.35 min=0.00 max=1.00
   ankerl::svector<16>: median=    1.00 mean=    1.07 +/-  0.26 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00

emplace_back:
       SmallVector<16>: median=    1.00 mean=    0.67 +/-  0.49 min=0.00 max=1.00
           std::vector: median=    1.00 mean=    0.60 +/-  0.51 min=0.00 max=1.00
  boost::small_vector<16>: median=    1.00 mean=    0.53 +/-  0.52 min=0.00 max=1.00
  llvm::SmallVector<16>: median=    1.00 mean=    0.87 +/-  0.35 min=0.00 max=1.00
  absl::InlinedVector<16>: median=    1.00 mean=    0.87 +/-  0.35 min=0.00 max=1.00
   ankerl::svector<16>: median=    1.00 mean=    1.00 +/-  0.38 min=0.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    0.53 +/-  0.52 min=0.00 max=1.00

operator[]:
       SmallVector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.40 +/-  0.51 min=1.00 max=2.00
  llvm::SmallVector<16>: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.33 +/-  0.49 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    1.20 +/-  0.41 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00

iteration:
       SmallVector<16>: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.27 +/-  0.46 min=1.00 max=2.00
  llvm::SmallVector<16>: median=    1.00 mean=    1.47 +/-  0.52 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.47 +/-  0.52 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.47 +/-  0.52 min=1.00 max=2.00

copy ctor:
       SmallVector<16>: median=    1.00 mean=    1.20 +/-  0.41 min=1.00 max=2.00
           std::vector: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
  boost::small_vector<16>: median=    1.00 mean=    1.00 +/-  0.00 min=1.00 max=1.00
  llvm::SmallVector<16>: median=    1.00 mean=    1.07 +/-  0.26 min=1.00 max=2.00
  absl::InlinedVector<16>: median=    1.00 mean=    1.20 +/-  0.41 min=1.00 max=2.00
   ankerl::svector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00
  eastl::fixed_vector<16>: median=    1.00 mean=    1.13 +/-  0.35 min=1.00 max=2.00

move ctor:
       SmallVector<16>: median=    0.00 mean=    0.20 +/-  0.41 min=0.00 max=1.00
           std::vector: median=    0.00 mean=    0.13 +/-  0.35 min=0.00 max=1.00
  boost::small_vector<16>: median=    0.00 mean=    0.27 +/-  0.46 min=0.00 max=1.00
  llvm::SmallVector<16>: median=    0.00 mean=    0.40 +/-  0.51 min=0.00 max=1.00
  absl::InlinedVector<16>: median=    0.00 mean=    0.13 +/-  0.35 min=0.00 max=1.00
   ankerl::svector<16>: median=    0.00 mean=    0.20 +/-  0.41 min=0.00 max=1.00
  eastl::fixed_vector<16>: median=    2.00 mean=    1.60 +/-  0.51 min=1.00 max=2.00


--- N = 1000 ---

[2026-02-01 19:28:56] Starting CPU: 2322 MHz (base: 3686)
push_back:
       SmallVector<16>: median=    0.60 mean=    0.57 +/-  0.08 min=0.50 max=0.70
           std::vector: median=    0.90 mean=    0.93 +/-  0.05 min=0.90 max=1.00
  boost::small_vector<16>: median=    0.60 mean=    0.57 +/-  0.05 min=0.50 max=0.60
  llvm::SmallVector<16>: median=    0.60 mean=    0.59 +/-  0.04 min=0.50 max=0.60
  absl::InlinedVector<16>: median=    0.60 mean=    0.61 +/-  0.12 min=0.50 max=1.00
   ankerl::svector<16>: median=    0.60 mean=    0.63 +/-  0.11 min=0.50 max=0.80
  eastl::fixed_vector<16>: median=    1.10 mean=    1.12 +/-  0.04 min=1.10 max=1.20

emplace_back:
       SmallVector<16>: median=    0.60 mean=    0.57 +/-  0.05 min=0.50 max=0.60
           std::vector: median=    0.60 mean=    0.57 +/-  0.05 min=0.50 max=0.60
  boost::small_vector<16>: median=    0.60 mean=    0.63 +/-  0.10 min=0.50 max=0.90
  llvm::SmallVector<16>: median=    0.40 mean=    0.43 +/-  0.06 min=0.40 max=0.60
  absl::InlinedVector<16>: median=    0.60 mean=    0.65 +/-  0.14 min=0.50 max=1.10
   ankerl::svector<16>: median=    0.70 mean=    0.66 +/-  0.11 min=0.50 max=0.80
  eastl::fixed_vector<16>: median=    0.40 mean=    0.43 +/-  0.06 min=0.40 max=0.60

operator[]:
       SmallVector<16>: median=    1.20 mean=    1.21 +/-  0.04 min=1.20 max=1.30
           std::vector: median=    1.20 mean=    1.23 +/-  0.05 min=1.20 max=1.30
  boost::small_vector<16>: median=    1.20 mean=    1.19 +/-  0.04 min=1.10 max=1.20
  llvm::SmallVector<16>: median=    1.20 mean=    1.18 +/-  0.06 min=1.10 max=1.30
  absl::InlinedVector<16>: median=    1.20 mean=    1.19 +/-  0.07 min=1.10 max=1.30
   ankerl::svector<16>: median=    1.10 mean=    1.16 +/-  0.07 min=1.10 max=1.30
  eastl::fixed_vector<16>: median=    1.20 mean=    1.23 +/-  0.05 min=1.20 max=1.30

iteration:
       SmallVector<16>: median=    1.20 mean=    1.22 +/-  0.04 min=1.20 max=1.30
           std::vector: median=    1.20 mean=    1.20 +/-  0.00 min=1.20 max=1.20
  boost::small_vector<16>: median=    1.20 mean=    1.23 +/-  0.05 min=1.20 max=1.30
  llvm::SmallVector<16>: median=    1.20 mean=    1.24 +/-  0.05 min=1.20 max=1.30
  absl::InlinedVector<16>: median=    1.20 mean=    1.21 +/-  0.05 min=1.10 max=1.30
   ankerl::svector<16>: median=    1.20 mean=    1.23 +/-  0.05 min=1.20 max=1.30
  eastl::fixed_vector<16>: median=    1.20 mean=    1.20 +/-  0.04 min=1.10 max=1.30

copy ctor:
       SmallVector<16>: median=    0.30 mean=    0.29 +/-  0.03 min=0.20 max=0.30
           std::vector: median=    0.10 mean=    0.12 +/-  0.04 min=0.10 max=0.20
  boost::small_vector<16>: median=    0.20 mean=    0.15 +/-  0.05 min=0.10 max=0.20
  llvm::SmallVector<16>: median=    0.10 mean=    0.14 +/-  0.05 min=0.10 max=0.20
  absl::InlinedVector<16>: median=    0.10 mean=    0.13 +/-  0.05 min=0.10 max=0.20
   ankerl::svector<16>: median=    0.10 mean=    0.13 +/-  0.05 min=0.10 max=0.20
  eastl::fixed_vector<16>: median=    0.10 mean=    0.15 +/-  0.05 min=0.10 max=0.20

move ctor:
       SmallVector<16>: median=    0.00 mean=    0.02 +/-  0.04 min=0.00 max=0.10
           std::vector: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.00
  boost::small_vector<16>: median=    0.00 mean=    0.02 +/-  0.04 min=0.00 max=0.10
  llvm::SmallVector<16>: median=    0.00 mean=    0.03 +/-  0.05 min=0.00 max=0.10
  absl::InlinedVector<16>: median=    0.00 mean=    0.03 +/-  0.05 min=0.00 max=0.10
   ankerl::svector<16>: median=    0.00 mean=    0.02 +/-  0.04 min=0.00 max=0.10
  eastl::fixed_vector<16>: median=    0.40 mean=    0.43 +/-  0.05 min=0.40 max=0.50


--- N = 10000 ---

[2026-02-01 19:28:56] Starting CPU: 2322 MHz (base: 3686)
push_back:
       SmallVector<16>: median=    0.74 mean=    0.94 +/-  0.55 min=0.73 max=2.67
           std::vector: median=    1.15 mean=    1.15 +/-  0.01 min=1.14 max=1.16
  boost::small_vector<16>: median=    0.73 mean=    0.75 +/-  0.04 min=0.72 max=0.83
  llvm::SmallVector<16>: median=    0.59 mean=    0.59 +/-  0.02 min=0.57 max=0.62
  absl::InlinedVector<16>: median=    0.74 mean=    0.74 +/-  0.01 min=0.72 max=0.75
   ankerl::svector<16>: median=    0.57 mean=    0.59 +/-  0.04 min=0.55 max=0.67
  eastl::fixed_vector<16>: median=    1.29 mean=    1.29 +/-  0.01 min=1.28 max=1.31

emplace_back:
       SmallVector<16>: median=    0.75 mean=    0.76 +/-  0.04 min=0.73 max=0.88
           std::vector: median=    0.73 mean=    0.73 +/-  0.01 min=0.72 max=0.74
  boost::small_vector<16>: median=    0.84 mean=    0.91 +/-  0.16 min=0.77 max=1.24
  llvm::SmallVector<16>: median=    0.72 mean=    0.77 +/-  0.20 min=0.53 max=1.34
  absl::InlinedVector<16>: median=    0.74 mean=    0.74 +/-  0.01 min=0.73 max=0.76
   ankerl::svector<16>: median=    0.61 mean=    0.60 +/-  0.04 min=0.55 max=0.69
  eastl::fixed_vector<16>: median=    0.53 mean=    0.53 +/-  0.01 min=0.51 max=0.56

operator[]:
       SmallVector<16>: median=    1.21 mean=    1.23 +/-  0.05 min=1.20 max=1.34
           std::vector: median=    1.21 mean=    1.25 +/-  0.09 min=1.20 max=1.51
  boost::small_vector<16>: median=    1.21 mean=    1.23 +/-  0.05 min=1.20 max=1.33
  llvm::SmallVector<16>: median=    1.21 mean=    1.23 +/-  0.05 min=1.20 max=1.33
  absl::InlinedVector<16>: median=    1.28 mean=    1.27 +/-  0.10 min=1.16 max=1.43
   ankerl::svector<16>: median=    1.33 mean=    1.37 +/-  0.14 min=1.21 max=1.72
  eastl::fixed_vector<16>: median=    1.21 mean=    1.23 +/-  0.05 min=1.20 max=1.33

iteration:
       SmallVector<16>: median=    1.22 mean=    1.22 +/-  0.01 min=1.20 max=1.24
           std::vector: median=    1.20 mean=    1.20 +/-  0.01 min=1.19 max=1.22
  boost::small_vector<16>: median=    1.22 mean=    1.42 +/-  0.75 min=1.20 max=4.12
  llvm::SmallVector<16>: median=    1.23 mean=    1.22 +/-  0.01 min=1.21 max=1.23
  absl::InlinedVector<16>: median=    1.22 mean=    1.22 +/-  0.01 min=1.20 max=1.25
   ankerl::svector<16>: median=    1.22 mean=    1.22 +/-  0.01 min=1.20 max=1.24
  eastl::fixed_vector<16>: median=    1.20 mean=    1.20 +/-  0.01 min=1.19 max=1.21

copy ctor:
       SmallVector<16>: median=    0.24 mean=    0.82 +/-  0.87 min=0.22 max=2.08
           std::vector: median=    0.09 mean=    0.98 +/-  1.30 min=0.08 max=2.82
  boost::small_vector<16>: median=    0.10 mean=    1.00 +/-  1.32 min=0.09 max=2.85
  llvm::SmallVector<16>: median=    0.10 mean=    1.02 +/-  1.36 min=0.09 max=3.12
  absl::InlinedVector<16>: median=    0.10 mean=    1.37 +/-  1.42 min=0.08 max=3.23
   ankerl::svector<16>: median=    2.08 mean=    1.94 +/-  0.51 min=0.10 max=2.13
  eastl::fixed_vector<16>: median=    0.10 mean=    0.81 +/-  1.22 min=0.09 max=2.87

move ctor:
       SmallVector<16>: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
           std::vector: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
  boost::small_vector<16>: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
  llvm::SmallVector<16>: median=    0.01 mean=    0.01 +/-  0.01 min=0.00 max=0.01
  absl::InlinedVector<16>: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
   ankerl::svector<16>: median=    0.00 mean=    0.00 +/-  0.00 min=0.00 max=0.01
  eastl::fixed_vector<16>: median=    0.38 mean=    0.51 +/-  0.51 min=0.35 max=2.37


================================================================================
  INLINE VS HEAP PERFORMANCE
================================================================================

The key SmallVector advantage: zero allocations for small sizes.
Testing operations at various sizes relative to inline capacity.

[2026-02-01 19:28:56] Starting CPU: 2248 MHz (base: 3686)
Size | SmallVector push_back (ns) | std::vector push_back (ns) | Ratio
-----|----------------------------|----------------------------|------
   4 (inline) |                       3.97 |                      23.45 |  5.91x
   8 (inline) |                       2.08 |                      18.27 |  8.77x
  12 (inline) |                       1.27 |                      15.71 | 12.32x
  16 (inline) |                       1.06 |                      12.88 | 12.19x
  20 (heap) |                       2.47 |                      10.59 |  4.28x
  32 (heap) |                       1.87 |                       7.43 |  3.97x
  64 (heap) |                       1.89 |                       5.84 |  3.09x
 128 (heap) |                       1.91 |                       3.32 |  1.73x

================================================================================
  ALLOCATION COUNT COMPARISON
================================================================================

Counting heap allocations for various scenarios.
SmallVector should have zero allocations when size <= InlineCapacity.

[2026-02-01 19:28:56] Starting CPU: 2653 MHz (base: 3686)
Scenario: push_back N elements (no reserve)

    N | SmallVector allocs | std::vector allocs
------|--------------------|-----------------
    1 |                  0 |                 1
    8 |                  0 |                 6
   16 |                  0 |                 8
   17 |                  1 |                 8
   32 |                  1 |                10
  100 |                  3 |                13
 1000 |                  6 |                18

Note: SmallVector<16> uses inline storage for N <= 16

================================================================================
  INSERT/ERASE OPERATIONS
================================================================================

Comparing insert and erase at various positions.

[2026-02-01 19:28:56] Starting CPU: 2653 MHz (base: 3686)
Insert single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |               63.00 |               56.87 |  0.90x
  middle |               35.07 |               33.20 |  0.95x
    back |               10.67 |                9.00 |  0.84x

Erase single element (N=1000, 100 ops each):

Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio
---------|---------------------|---------------------|------
   front |               36.73 |               35.60 |  0.97x
  middle |               21.40 |               20.13 |  0.94x
    back |                6.80 |                6.87 |  1.01x

================================================================================
  INLINE CAPACITY SENSITIVITY
================================================================================

How does inline capacity choice affect performance?
Testing push_back of N elements with various inline capacities.

[2026-02-01 19:28:56] Starting CPU: 2653 MHz (base: 3686)
Target size: 32 elements

InlineCapacity | Time (ns/op) | Allocations | Notes
---------------|--------------|-------------|------
  0 (std::vec) |         7.15 |          10 | always heap
             8 |         2.54 |           2 | transitions
            16 |         1.74 |           1 | transitions
            32 |         0.97 |           0 | all inline
            64 |         0.84 |           0 | all inline

================================================================================
  FAST PATH THROUGHPUT (ISOLATION BENCHMARK)
================================================================================

Measuring pure fast-path performance with zero allocations.
This isolates the emplace_back fast-path optimization from allocation benefits.

[2026-02-01 19:28:56] Starting CPU: 2764 MHz (base: 3686)

--- emplace_back Throughput (100% Fast Path) ---

InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio
----------|---------------------|----------------------|------
* std::vector is pre-reserved to avoid allocation

        4 |                3.33 |                 3.29 |  0.99x
        8 |                1.96 |                 2.17 |  1.11x
       16 |                1.17 |                 1.14 |  0.97x
       32 |                0.99 |                 0.87 |  0.88x

--- push_back vs emplace_back (InlineCap=16) ---

Operation   | SmallVector (ns/op) | std::vector* (ns/op)
------------|---------------------|---------------------
push_back   |                1.18 |                1.17
emplace_back|                1.06 |                1.06

--- Pairwise Fast-Path Comparisons (InlineCap=16, N=16) ---
Each pair tested in isolated function call (noinline).

fat_p= 1.04  std::vec= 1.20
fat_p= 1.17  boost= 1.12
fat_p= 1.30  llvm= 1.22
fat_p= 1.12  absl= 1.70
fat_p= 1.18  ankerl= 4.30
fat_p= 1.17  eastl= 1.00

--- Fill/Clear Cycle (1000 cycles, InlineCap=16) ---
Tests repeated use of same SmallVector (cache-warm scenario)

SmallVector (reused): 0.69 ns/op
std::vector (reused): 0.68 ns/op

Note: This benchmark measures the optimized fast-path code.
The main SmallVector advantage (7-8x for small N) comes from
avoiding heap allocation entirely, which is tested in
benchmark_inline_vs_heap().

================================================================================
  OBJECT SIZE IMPACT
================================================================================

How does sizeof(SmallVector) compare to std::vector?
Larger inline capacity = larger object size.

Container                        | sizeof (bytes)
---------------------------------|---------------
            std::vector<int64_t> |             24
         SmallVector<int64_t, 4> |             64
         SmallVector<int64_t, 8> |             96
        SmallVector<int64_t, 16> |            160
        SmallVector<int64_t, 32> |            288
        SmallVector<int64_t, 64> |            544

Boost comparison:
boost::small_vector<int64_t, 16> |            152

LLVM comparison:
  llvm::SmallVector<int64_t, 16> |            144

Abseil comparison:
absl::InlinedVector<int64_t, 16> |            136

ankerl comparison:
    ankerl::svector<int64_t, 16> |            136
     ankerl::svector<int64_t, 1> |             16

EASTL comparison:
 eastl::fixed_vector<int64_t,16> |            168

Note: SmallVector<T, N> size Γëê 3*sizeof(void*) + N*sizeof(T)

================================================================================
  Benchmark Complete
================================================================================
```


---

# SparseSet

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::SparseSet Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::SparseSet<8> (primary)
  [x] fat_p::SparseSet<32> (primary)
  [x] fat_p::FlatSet (sibling)
  [x] std::unordered_set (baseline)
  [x] std::set (baseline)
  [x] entt::sparse_set

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement

Cooling: section=1000ms size=500ms case=200ms

Expected Results:
  - SparseSet excels at: insert/erase churn, iteration
  - FlatSet excels at: sorted access, iteration, lookup after build
  - Hash sets: fast point operations, scattered iteration
  - std::set: consistent O(log n), slowest overall

Checking initial CPU state...
[2026-02-01 19:28:57] Initial CPU: 2580 MHz (base: 3686)
Waiting for CPU to stabilize...
[Waiting: 2580/3686 MHz (30% below base)]
[Waiting: 2395/3686 MHz (35% below base)]
[WARNING: CPU still 41% below base after 6s - 2174/3686 MHz]
WARNING: CPU frequency still fluctuating, results may have higher variance.


================================================================================
  SECTION 1: Core Operations
================================================================================

[2026-02-01 19:29:03] Section start CPU: 2174 MHz (base: 3686)
Contract Note: O(1) insert/erase/contains; dense iteration; unstable erase order
              Insert excludes allocation (reserve performed in setup)


--- N = 1000 ---
[2026-02-01 19:29:03] CPU: 2174 MHz (base: 3686)
[Cooling: size transition] [Ready: 2211 MHz]

  Insert:
     fat_p::SparseSet<8>:       1.90 ns/op (+/-   0.48, CI:[    1.81,    2.34])
    fat_p::SparseSet<32>:       1.00 ns/op (+/-   0.30, CI:[    0.98,    1.30])
          fat_p::FlatSet:      43.90 ns/op (+/-  11.70, CI:[   42.15,   55.09])
      std::unordered_set:      28.40 ns/op (+/-   8.13, CI:[   28.13,   37.11])
     absl::flat_hash_set:      17.60 ns/op (+/-   4.58, CI:[   16.70,   21.76])
      llvm::SparseSet<8>:       1.10 ns/op (+/-   0.33, CI:[    1.03,    1.39])
     llvm::SparseSet<32>:       1.30 ns/op (+/-   0.31, CI:[    1.05,    1.39])
        entt::sparse_set:       2.90 ns/op (+/-   0.81, CI:[    2.76,    3.66])
                std::set:      69.30 ns/op (+/-  17.92, CI:[   65.99,   85.79])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.50 ns/op (+/-   0.07, CI:[    0.49,    0.57])
    fat_p::SparseSet<32>:       3.50 ns/op (+/-   0.97, CI:[    2.53,    3.59])
          fat_p::FlatSet:      43.10 ns/op (+/-   7.65, CI:[   41.23,   49.69])
      std::unordered_set:       7.60 ns/op (+/-   2.14, CI:[    6.29,    8.66])
     absl::flat_hash_set:       7.60 ns/op (+/-   1.20, CI:[    7.42,    8.75])
      llvm::SparseSet<8>:       5.10 ns/op (+/-   0.79, CI:[    4.71,    5.58])
     llvm::SparseSet<32>:       2.60 ns/op (+/-   1.32, CI:[    2.19,    3.65])
        entt::sparse_set:       3.10 ns/op (+/-   0.76, CI:[    2.70,    3.53])
                std::set:      37.70 ns/op (+/-   4.49, CI:[   36.55,   41.51])

  Erase:
     fat_p::SparseSet<8>:       2.40 ns/op (+/-   0.74, CI:[    2.19,    3.01])
    fat_p::SparseSet<32>:       0.80 ns/op (+/-   0.13, CI:[    0.84,    0.98])
          fat_p::FlatSet:      44.20 ns/op (+/-   2.28, CI:[   43.51,   46.03])
      std::unordered_set:      15.00 ns/op (+/-   0.85, CI:[   14.33,   15.27])
     absl::flat_hash_set:       5.00 ns/op (+/-   0.30, CI:[    4.70,    5.03])
      llvm::SparseSet<8>:       4.00 ns/op (+/-   0.27, CI:[    3.93,    4.23])
     llvm::SparseSet<32>:       1.00 ns/op (+/-   0.05, CI:[    0.98,    1.04])
        entt::sparse_set:       3.00 ns/op (+/-   0.10, CI:[    3.02,    3.14])
                std::set:      83.60 ns/op (+/-   4.03, CI:[   82.77,   87.23])

  Iteration:
     fat_p::SparseSet<8>:       0.40 ns/op (+/-   0.20, CI:[    0.16,    0.37])
    fat_p::SparseSet<32>:       0.20 ns/op (+/-   0.03, CI:[    0.19,    0.22])
          fat_p::FlatSet:       0.20 ns/op (+/-   0.03, CI:[    0.19,    0.22])
      std::unordered_set:       0.90 ns/op (+/-   0.05, CI:[    0.90,    0.95])
     absl::flat_hash_set:       3.00 ns/op (+/-   0.42, CI:[    2.91,    3.38])
      llvm::SparseSet<8>:       0.20 ns/op (+/-   0.00, CI:[    0.20,    0.20])
     llvm::SparseSet<32>:       0.20 ns/op (+/-   0.04, CI:[    0.20,    0.24])
        entt::sparse_set:       0.20 ns/op (+/-   0.05, CI:[    0.22,    0.28])
                std::set:       4.20 ns/op (+/-   0.33, CI:[    4.05,    4.41])

--- N = 10000 ---
[2026-02-01 19:29:03] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2653 MHz]

  Insert:
     fat_p::SparseSet<8>:       0.73 ns/op (+/-   0.04, CI:[    0.72,    0.76])
    fat_p::SparseSet<32>:       1.18 ns/op (+/-   0.06, CI:[    1.17,    1.23])
          fat_p::FlatSet:     110.16 ns/op (+/-   9.45, CI:[  108.62,  119.07])
      std::unordered_set:      35.84 ns/op (+/-   9.13, CI:[   33.25,   43.34])
     absl::flat_hash_set:      16.24 ns/op (+/-   1.41, CI:[   15.55,   17.10])
      llvm::SparseSet<8>:       5.96 ns/op (+/-   0.18, CI:[    5.94,    6.14])
     llvm::SparseSet<32>:       1.13 ns/op (+/-   0.09, CI:[    1.12,    1.23])
        entt::sparse_set:       5.03 ns/op (+/-   0.38, CI:[    4.76,    5.17])
                std::set:     112.51 ns/op (+/-  18.15, CI:[  110.72,  130.78])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.35 ns/op (+/-   0.02, CI:[    0.35,    0.37])
    fat_p::SparseSet<32>:       3.82 ns/op (+/-   0.12, CI:[    3.77,    3.90])
          fat_p::FlatSet:      48.40 ns/op (+/-   4.06, CI:[   48.04,   52.52])
      std::unordered_set:       8.06 ns/op (+/-   0.12, CI:[    8.04,    8.17])
     absl::flat_hash_set:       7.42 ns/op (+/-   0.36, CI:[    7.39,    7.79])
      llvm::SparseSet<8>:      12.26 ns/op (+/-   5.12, CI:[   10.80,   16.46])
     llvm::SparseSet<32>:       3.91 ns/op (+/-   0.22, CI:[    3.90,    4.15])
        entt::sparse_set:       4.33 ns/op (+/-   0.15, CI:[    4.28,    4.44])
                std::set:      55.94 ns/op (+/-  12.24, CI:[   54.97,   68.50])

  Erase:
     fat_p::SparseSet<8>:       0.94 ns/op (+/-   0.15, CI:[    0.88,    1.04])
    fat_p::SparseSet<32>:       2.04 ns/op (+/-   0.08, CI:[    2.02,    2.10])
          fat_p::FlatSet:     114.78 ns/op (+/-   4.68, CI:[  110.66,  115.83])
      std::unordered_set:      15.54 ns/op (+/-   2.46, CI:[   14.77,   17.48])
     absl::flat_hash_set:       6.46 ns/op (+/-   0.47, CI:[    6.41,    6.93])
      llvm::SparseSet<8>:      10.72 ns/op (+/-   0.53, CI:[   10.52,   11.11])
     llvm::SparseSet<32>:       1.98 ns/op (+/-   0.05, CI:[    1.97,    2.02])
        entt::sparse_set:       3.66 ns/op (+/-  15.17, CI:[   -0.77,   16.00])
                std::set:     125.70 ns/op (+/-  14.44, CI:[  122.19,  138.14])

  Iteration:
     fat_p::SparseSet<8>:       0.39 ns/op (+/-   0.14, CI:[    0.37,    0.52])
    fat_p::SparseSet<32>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
          fat_p::FlatSet:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
      std::unordered_set:       2.18 ns/op (+/-   0.12, CI:[    2.14,    2.28])
     absl::flat_hash_set:       2.85 ns/op (+/-   0.18, CI:[    2.80,    2.99])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
     llvm::SparseSet<32>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.19])
        entt::sparse_set:       0.20 ns/op (+/-   0.01, CI:[    0.20,    0.21])
                std::set:       6.34 ns/op (+/-   0.17, CI:[    6.20,    6.39])

--- N = 100000 ---
[2026-02-01 19:29:04] CPU: 2248 MHz (base: 3686)
[Cooling: size transition] [Ready: 2432 MHz]

  Insert:
     fat_p::SparseSet<8>:       0.60 ns/op (+/-   0.03, CI:[    0.60,    0.63])
    fat_p::SparseSet<32>:       2.16 ns/op (+/-   0.28, CI:[    2.10,    2.40])
          fat_p::FlatSet:    1229.44 ns/op (+/-   6.36, CI:[ 1226.18, 1233.21])
      std::unordered_set:      43.83 ns/op (+/-   4.25, CI:[   43.27,   47.97])
     absl::flat_hash_set:      14.09 ns/op (+/-   1.44, CI:[   13.59,   15.19])
      llvm::SparseSet<8>:     108.02 ns/op (+/-   3.91, CI:[  105.55,  109.87])
     llvm::SparseSet<32>:       1.85 ns/op (+/-   0.20, CI:[    1.67,    1.89])
        entt::sparse_set:       6.64 ns/op (+/-   0.17, CI:[    6.55,    6.74])
                std::set:     158.70 ns/op (+/-   4.15, CI:[  156.03,  160.61])

  Contains (50% hit):
     fat_p::SparseSet<8>:       0.40 ns/op (+/-   0.05, CI:[    0.38,    0.44])
    fat_p::SparseSet<32>:       4.89 ns/op (+/-   0.83, CI:[    4.73,    5.65])
          fat_p::FlatSet:      63.61 ns/op (+/-   2.63, CI:[   63.20,   66.10])
      std::unordered_set:      11.08 ns/op (+/-   1.71, CI:[   11.04,   12.93])
     absl::flat_hash_set:       8.62 ns/op (+/-   1.37, CI:[    8.68,   10.20])
      llvm::SparseSet<8>:     140.20 ns/op (+/-   2.86, CI:[  137.65,  140.81])
     llvm::SparseSet<32>:       5.28 ns/op (+/-   0.17, CI:[    5.19,    5.38])
        entt::sparse_set:       5.76 ns/op (+/-   0.84, CI:[    5.59,    6.53])
                std::set:     109.90 ns/op (+/-   3.67, CI:[  107.06,  111.11])

  Erase:
     fat_p::SparseSet<8>:       0.50 ns/op (+/-   0.10, CI:[    0.47,    0.58])
    fat_p::SparseSet<32>:       3.22 ns/op (+/-   0.27, CI:[    3.13,    3.43])
          fat_p::FlatSet:    1451.37 ns/op (+/-  10.80, CI:[ 1441.62, 1453.55])
      std::unordered_set:      25.87 ns/op (+/-   2.72, CI:[   24.83,   27.84])
     absl::flat_hash_set:      11.09 ns/op (+/-   0.65, CI:[   11.01,   11.72])
      llvm::SparseSet<8>:      53.13 ns/op (+/-   4.51, CI:[   53.06,   58.04])
     llvm::SparseSet<32>:       3.20 ns/op (+/-   0.63, CI:[    3.04,    3.74])
        entt::sparse_set:       6.05 ns/op (+/-   0.40, CI:[    6.00,    6.44])
                std::set:     221.38 ns/op (+/-   8.99, CI:[  214.62,  224.56])

  Iteration:
     fat_p::SparseSet<8>:       0.78 ns/op (+/-   0.20, CI:[    0.49,    0.71])
    fat_p::SparseSet<32>:       0.19 ns/op (+/-   0.00, CI:[    0.19,    0.19])
          fat_p::FlatSet:       0.19 ns/op (+/-   0.00, CI:[    0.19,    0.19])
      std::unordered_set:       4.67 ns/op (+/-   0.23, CI:[    4.58,    4.84])
     absl::flat_hash_set:       1.78 ns/op (+/-   0.01, CI:[    1.77,    1.78])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.13, CI:[    0.15,    0.29])
     llvm::SparseSet<32>:       0.20 ns/op (+/-   0.01, CI:[    0.20,    0.21])
        entt::sparse_set:       0.22 ns/op (+/-   0.02, CI:[    0.21,    0.23])
                std::set:      12.27 ns/op (+/-   1.23, CI:[   12.21,   13.56])
[Cooling: before iteration benchmark] [Ready: 2174 MHz]

================================================================================
  SECTION 2: Dense Iteration (SparseSet Key Advantage)
================================================================================

[2026-02-01 19:29:23] Section start CPU: 2174 MHz (base: 3686)
Contract Note: Dense iterationΓÇöSparseSet/FlatSet expected to outperform hash sets

--- N = 10000 ---
[2026-02-01 19:29:23] CPU: 2174 MHz (base: 3686)
[Cooling: size transition] [Ready: 2322 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.39 ns/op (+/-   0.10, CI:[    0.31,    0.42])
    fat_p::SparseSet<32>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
          fat_p::FlatSet:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.19])
      std::unordered_set:       2.13 ns/op (+/-   0.15, CI:[    2.08,    2.24])
     absl::flat_hash_set:       2.84 ns/op (+/-   0.17, CI:[    2.77,    2.97])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
     llvm::SparseSet<32>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
        entt::sparse_set:       0.21 ns/op (+/-   0.01, CI:[    0.20,    0.22])
                std::set:       6.36 ns/op (+/-   0.18, CI:[    6.19,    6.39])

--- N = 100000 ---
[2026-02-01 19:29:23] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 2137 MHz]
  Iteration (sum all elements):
     fat_p::SparseSet<8>:       0.78 ns/op (+/-   0.26, CI:[    0.64,    0.92])
    fat_p::SparseSet<32>:       0.19 ns/op (+/-   0.01, CI:[    0.19,    0.20])
          fat_p::FlatSet:       0.19 ns/op (+/-   0.00, CI:[    0.19,    0.19])
      std::unordered_set:       4.80 ns/op (+/-   1.09, CI:[    4.58,    5.78])
     absl::flat_hash_set:       1.77 ns/op (+/-   0.03, CI:[    1.77,    1.80])
      llvm::SparseSet<8>:       0.19 ns/op (+/-   0.00, CI:[    0.19,    0.19])
     llvm::SparseSet<32>:       0.20 ns/op (+/-   0.01, CI:[    0.20,    0.21])
        entt::sparse_set:       0.21 ns/op (+/-   0.02, CI:[    0.21,    0.23])
                std::set:      12.14 ns/op (+/-   1.52, CI:[   11.80,   13.48])

[Cooling: before mixed workload] [Ready: 2506 MHz]

================================================================================
  SECTION 3: Mixed Workload (Insert/Erase Churn)
================================================================================

[2026-02-01 19:29:28] Section start CPU: 2506 MHz (base: 3686)
Contract Note: Random insert/erase churnΓÇötests swap-with-back erase efficiency

--- N = 10000 (50% insert/erase cycles) ---
[2026-02-01 19:29:28] CPU: 2506 MHz (base: 3686)
  Mixed Workload:
     fat_p::SparseSet<8>:       1.26 ns/op (+/-   0.08, CI:[    1.26,    1.34])
    fat_p::SparseSet<32>:       2.20 ns/op (+/-   0.08, CI:[    2.19,    2.28])
          fat_p::FlatSet:     153.49 ns/op (+/-  14.74, CI:[  152.89,  169.18])
      std::unordered_set:      20.67 ns/op (+/-   5.32, CI:[   19.00,   24.88])
     absl::flat_hash_set:      10.55 ns/op (+/-   0.46, CI:[   10.50,   11.00])
      llvm::SparseSet<8>:      15.83 ns/op (+/-   7.19, CI:[   14.43,   22.38])
     llvm::SparseSet<32>:       2.15 ns/op (+/-   0.11, CI:[    2.16,    2.28])
        entt::sparse_set:       3.63 ns/op (+/-   0.33, CI:[    3.63,    4.00])
                std::set:     101.65 ns/op (+/-  12.07, CI:[  100.95,  114.29])

================================================================================
  Feature Comparison Summary
================================================================================

  Container                 Insert    Contains  Erase     Iteration  Order
  --------------------------------------------------------------------------
  fat_p::SparseSet<8>       O(1)*     O(1)      O(1)      Dense      Unstable  (max 256)
  fat_p::SparseSet<32>      O(1)*     O(1)      O(1)      Dense      Unstable  (handles large N)
  fat_p::FlatSet            O(n)      O(log n)  O(n)      Dense      Sorted
  std::unordered_set        O(1)*     O(1)*     O(1)*     Scattered  Unordered
  absl::flat_hash_set       O(1)*     O(1)*     O(1)*     Scattered  Unordered
  llvm::SparseSet<8>        O(1)*     O(1)      O(1)      Dense      Unstable  (default, max 256)
  llvm::SparseSet<32>       O(1)*     O(1)      O(1)      Dense      Unstable  (configured)
  entt::sparse_set          O(1)*     O(1)      O(1)      Dense      Unstable
  std::set                  O(log n)  O(log n)  O(log n)  In-order   Sorted

  * = amortized
  Note: <8> variants use uint8_t value type (max 256 elements)
        <32> variants use uint32_t value type (handles large N)

  When to use SparseSet:
    - Integer keys in a bounded range
    - Frequent insert/erase churn
    - Iteration performance matters
    - Order doesn't matter

  When to use FlatSet:
    - Need sorted order
    - Mostly lookups after initial build
    - Binary search semantics (lower_bound, etc.)

================================================================================
  Benchmark Complete
================================================================================
```


---

# Stacktrace

```
================================================================================
  fat_p::Stacktrace Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::Stacktrace (primary)
  [x] Native backend (Windows DbgHelp) (baseline)

Design Invariants:
  1. Setup/teardown outside timed regions
  2. Medians are the primary reported statistic

Backend info:
  Backend:       Windows DbgHelp
  Real backend:  yes


======================================================================
  captureRaw() - Address Collection Only
======================================================================

Contract: Measures address capture without symbol resolution

captureRaw()                          0.36 us/op  (mean: 0.37, stddev: 0.00)

======================================================================
  current() - Full Capture with Symbols
======================================================================

Contract: Measures capture + symbol resolution

current()                             4.26 us/op  (mean: 4.32, stddev: 0.19)

======================================================================
  Formatting - toString() and toJson()
======================================================================

Contract: Measures output formatting from symbolized trace

toString()                            2.96 us/op  (mean: 2.96, stddev: 0.05)
toJson()                              5.01 us/op  (mean: 5.06, stddev: 0.09)

======================================================================
  Depth Scaling - captureRaw() at Various Stack Depths
======================================================================

Contract: Measures capture cost vs stack depth

Depth                               Median  us/op
--------------------------------------------------
depth=5                               0.71  us/op
depth=10                              1.01  us/op
depth=20                              1.56  us/op
depth=50                              2.01  us/op

======================================================================
  hash() - For Deduplication
======================================================================

Contract: Measures hash computation for container usage

hash()                                7.87 ns/op  (mean: 7.89, stddev: 0.10)

======================================================================
  Summary
======================================================================

All benchmarks completed.
Backend used: Windows DbgHelp

```


---

# StateMachine

```
================================================================================
  fat_p::StateMachine Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::StateMachine<AnyToAnyPolicy> (primary)
  [x] fat_p::StateMachine<StrictPolicy> (primary)
  [x] Manual enum-switch (baseline)
  [x] Manual fn-ptr table (baseline)
  [x] std::variant + std::visit (C++17 baseline)
  [x] boost::sml
  [x] Boost.MSM
  [x] TinyFSM

Configuration:
  Target work:    5000000 ops/batch
  Min batch ms:   50
  Scope:          ON
  Stabilize:      ON
  Cooldown:       ON

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
  6. CPU frequency stabilized before measurement


================================================================================
  Section 1: Core Transition Performance
================================================================================

Contract: transition() is O(1) for all implementations. All use 4 states with counting hooks.

[2026-02-01 19:29:29] CPU: 2875 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             7.64        7.33      1.22  [  6.71,   7.94] ns/op
fat_p Strict                               7.58        7.35      1.15  [  6.77,   7.93] ns/op
Manual enum-switch                         2.27        2.54      0.60  [  2.24,   2.85] ns/op
Manual fn-ptr table                        3.40        3.65      0.87  [  3.21,   4.09] ns/op
std::variant                               7.06        7.37      1.28  [  6.73,   8.02] ns/op
[Boost].SML                                6.68        7.02      1.00  [  6.51,   7.53] ns/op
TinyFSM                                    7.68        8.24      1.73  [  7.36,   9.11] ns/op
Boost.MSM                                 11.61       12.23      1.73  [ 11.35,  13.10] ns/op

[OK] Correctness validated for all 8 implementations


================================================================================
  Section 2a: Hook Overhead (Empty Hooks)
================================================================================

Contract: Baseline with empty on_entry/on_exit hooks. Measures pure dispatch overhead.

[2026-02-01 19:29:34] CPU: 3538 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (empty)                     5.42        5.94      1.18  [  5.34,   6.53] ns/op
fat_p Strict (empty)                       6.10        6.34      1.54  [  5.56,   7.11] ns/op
Manual enum-switch (empty)                 2.17        2.12      0.10  [  2.06,   2.17] ns/op
Manual fn-ptr table (empty)                2.94        3.05      0.48  [  2.81,   3.29] ns/op
std::variant (empty)                       5.45        5.80      1.43  [  5.08,   6.53] ns/op
[Boost].SML (empty)                        6.21        6.47      1.38  [  5.77,   7.17] ns/op
TinyFSM (empty)                            6.68        7.07      1.26  [  6.44,   7.71] ns/op
Boost.MSM (empty)                         10.98       11.04      2.36  [  9.85,  12.24] ns/op


================================================================================
  Section 2b: Hook Overhead (Counting Hooks)
================================================================================

Contract: Hooks increment a counter. Measures dispatch + minimal work.

[2026-02-01 19:29:39] CPU: 2395 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             4.79        4.86      0.18  [  4.76,   4.95] ns/op
fat_p Strict                               6.05        6.04      0.14  [  5.97,   6.11] ns/op
Manual enum-switch                         2.34        2.38      0.14  [  2.31,   2.45] ns/op
Manual fn-ptr table                        2.83        2.81      0.11  [  2.75,   2.86] ns/op
std::variant                               5.41        5.44      0.19  [  5.34,   5.53] ns/op
[Boost].SML                                5.24        5.25      0.15  [  5.17,   5.32] ns/op
TinyFSM                                    6.32        6.30      0.19  [  6.20,   6.40] ns/op
Boost.MSM                                  8.64        8.63      0.18  [  8.54,   8.72] ns/op

[OK] Correctness validated for all 8 implementations


================================================================================
  Section 3a: State Scaling (4 States)
================================================================================

Contract: 4-state machines with counting hooks. Baseline for scaling comparison.

[2026-02-01 19:29:43] CPU: 3022 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             4.92        5.58      1.03  [  5.06,   6.10] ns/op
fat_p Strict                               6.12        6.47      0.95  [  5.99,   6.95] ns/op
Manual enum-switch                         2.38        2.36      0.13  [  2.30,   2.42] ns/op
Manual fn-ptr table                        2.81        3.18      0.76  [  2.80,   3.56] ns/op
std::variant                               5.67        6.14      1.22  [  5.52,   6.76] ns/op
[Boost].SML                                5.42        6.39      1.82  [  5.47,   7.32] ns/op
TinyFSM                                    6.60        6.80      0.66  [  6.47,   7.14] ns/op
Boost.MSM                                  9.06       10.82      3.14  [  9.23,  12.41] ns/op

[OK] Correctness validated for all 8 implementations


================================================================================
  Section 3b: State Scaling (8 States)
================================================================================

Contract: 8-state machines with counting hooks. O(1) claim: should match 4-state performance.

[2026-02-01 19:29:47] CPU: 3317 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny (8-state)                   7.89        8.54      1.81  [  7.62,   9.46] ns/op
fat_p Strict (8-state)                     7.88        8.11      1.08  [  7.56,   8.66] ns/op
Manual enum-switch (8-state)               1.37        1.60      0.42  [  1.39,   1.81] ns/op
Manual fn-ptr table (8-state)              1.99        2.08      0.20  [  1.97,   2.18] ns/op
std::variant (8-state)                     7.17        7.53      1.06  [  6.99,   8.06] ns/op
[Boost].SML (8-state)                      8.08        8.49      1.13  [  7.92,   9.06] ns/op

[OK] Correctness validated for all 6 implementations


================================================================================
  Section 4: Self-Transition (No-Op) Performance
================================================================================

Contract: Self-transitions should early-exit without invoking hooks. Expected count is 1 (initial entry only).

[2026-02-01 19:29:51] CPU: 3059 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.44        0.56      0.19  [  0.47,   0.66] ns/op
fat_p Strict                               0.44        0.54      0.16  [  0.45,   0.62] ns/op
Manual enum-switch                         0.45        0.47      0.11  [  0.41,   0.52] ns/op
Manual fn-ptr table                        0.44        0.53      0.17  [  0.45,   0.62] ns/op
std::variant                               0.44        0.48      0.09  [  0.43,   0.52] ns/op
[Boost].SML                                0.44        0.49      0.10  [  0.44,   0.54] ns/op
TinyFSM                                    0.44        0.52      0.17  [  0.43,   0.60] ns/op
Boost.MSM                                  0.45        0.62      0.22  [  0.51,   0.74] ns/op

[OK] Correctness validated for all 8 implementations


================================================================================
  Section 5: State Query Performance
================================================================================

Contract: isInState<T>() / equivalent should be O(1). Query from initial state.

[2026-02-01 19:29:51] CPU: 2875 MHz (base: 3686)
Implementation                           Median        Mean    Stddev   CI95
-------------------------------------------------------------------------------
fat_p AnyToAny                             0.44        0.49      0.09  [  0.44,   0.54] ns/op
fat_p Strict                               0.44        0.48      0.11  [  0.42,   0.53] ns/op
Manual enum-switch                         0.44        0.50      0.13  [  0.43,   0.57] ns/op
Manual fn-ptr table                        0.46        0.57      0.17  [  0.48,   0.65] ns/op
std::variant                               0.44        0.49      0.09  [  0.44,   0.54] ns/op
[Boost].SML                                0.44        0.48      0.07  [  0.45,   0.52] ns/op
TinyFSM                                    0.44        0.48      0.13  [  0.41,   0.55] ns/op
Boost.MSM                                  0.44        0.51      0.11  [  0.45,   0.56] ns/op

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

[INFO] CSV written to: benchmark_results\20260201_184451\benchmark_StateMachine.csv
[Sink: 2812042908]
```


---

# Stringify

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::Stringify Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::stringify (primary)
  [x] std::to_string (baseline)
  [x] std::ostringstream (baseline)
  [x] std::format (C++20)
  [x] fmt::format

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement

--- Section 1: Integer Stringification ---
Contract: Convert int to std::string. No locale formatting.

[2026-02-01 19:29:52] CPU: 3354 MHz (base: 3686)
  std::to_string (baseline)     7.36       ns  (mean: 7.78, stddev: 1.27)  CI95: [7.07, 8.48]
  fat_p::toString               8.62       ns  (mean: 9.08, stddev: 1.15)  CI95: [8.45, 9.72]
    -> fat_p::toString: 0.85x SLOWER than baseline
  std::format                   39.37      ns  (mean: 40.91, stddev: 3.26)  CI95: [39.10, 42.71]
    -> std::format: 0.19x SLOWER than baseline
  std::ostringstream            267.27     ns  (mean: 267.95, stddev: 3.39)  CI95: [266.08, 269.83]
    -> std::ostringstream: 0.03x SLOWER than baseline
  fmt::format                   19.04      ns  (mean: 19.50, stddev: 1.32)  CI95: [18.77, 20.23]
    -> fmt::format: 0.39x SLOWER than baseline

--- Section 2: Floating-Point Stringification ---
Contract: Convert double to std::string. Default precision.

[2026-02-01 19:29:53] CPU: 2653 MHz (base: 3686)
  std::to_string (baseline)     196.13     ns  (mean: 197.79, stddev: 3.40)  CI95: [195.91, 199.67]
  fat_p::toString               83.11      ns  (mean: 82.08, stddev: 3.14)  CI95: [80.34, 83.81]
    -> fat_p::toString: 2.36x FASTER than baseline
  std::format                   85.33      ns  (mean: 84.51, stddev: 3.66)  CI95: [82.49, 86.53]
    -> std::format: 2.30x FASTER than baseline
  std::ostringstream            406.62     ns  (mean: 407.37, stddev: 3.44)  CI95: [405.48, 409.27]
    -> std::ostringstream: 0.48x SLOWER than baseline
  fmt::format                   85.83      ns  (mean: 83.57, stddev: 3.66)  CI95: [81.55, 85.60]
    -> fmt::format: 2.29x FASTER than baseline

--- Section 3: Boolean Stringification ---
Contract: Convert bool to 'true'/'false' string.

[2026-02-01 19:29:54] CPU: 2359 MHz (base: 3686)
  ternary (baseline)            4.68       ns  (mean: 4.79, stddev: 0.42)  CI95: [4.56, 5.02]
  fat_p::toString               4.69       ns  (mean: 5.24, stddev: 0.97)  CI95: [4.71, 5.78]
    -> fat_p::toString: 1.00x SAME than baseline

--- Section 4: Container Stringification ---
Contract: Convert vector<int> to '[elem, ...]' format. Fat-P unique.

[2026-02-01 19:29:54] CPU: 2911 MHz (base: 3686)
  manual loop (baseline)        110.40     ns  (mean: 110.41, stddev: 0.26)  CI95: [110.27, 110.56]
  fat_p::toString               111.30     ns  (mean: 111.70, stddev: 0.72)  CI95: [111.30, 112.10]
    -> fat_p::toString: 0.99x SAME than baseline

--- Section 5: Map Stringification ---
Contract: Convert map<string,int> to '{k: v, ...}' format. Fat-P unique.

[2026-02-01 19:29:54] CPU: 2911 MHz (base: 3686)
  manual loop (baseline)        66.30      ns  (mean: 66.39, stddev: 0.29)  CI95: [66.23, 66.55]
  fat_p::toString               59.40      ns  (mean: 59.81, stddev: 1.06)  CI95: [59.22, 60.39]
    -> fat_p::toString: 1.12x FASTER than baseline

--- Section 6: Tuple/Pair Stringification ---
Contract: Convert tuple to '(a, b, c)' format. Fat-P unique.

[2026-02-01 19:29:54] CPU: 2911 MHz (base: 3686)
  fat_p::toString(tuple)        109.42     ns  (mean: 115.65, stddev: 15.73)  CI95: [106.96, 124.34]
  fat_p::toString(pair)         41.00      ns  (mean: 41.15, stddev: 0.44)  CI95: [40.91, 41.39]

--- Section 7: Optional Stringification ---
Contract: Convert optional<int> to value or 'nullopt'.

[2026-02-01 19:29:54] CPU: 2948 MHz (base: 3686)
  fat_p::toString(has_value)    9.57       ns  (mean: 9.58, stddev: 0.06)  CI95: [9.55, 9.61]
  fat_p::toString(nullopt)      5.57       ns  (mean: 5.61, stddev: 0.09)  CI95: [5.56, 5.66]

--- Section 8: String Concatenation (toStringConcat) ---
Contract: Concatenate multiple values into single string.

[2026-02-01 19:29:54] CPU: 2948 MHz (base: 3686)
  manual + (baseline)           211.85     ns  (mean: 219.78, stddev: 16.78)  CI95: [210.51, 229.05]
  fat_p::toStringConcat         97.81      ns  (mean: 103.87, stddev: 21.42)  CI95: [92.03, 115.70]
    -> fat_p::toStringConcat: 2.17x FASTER than baseline
  std::format                   106.12     ns  (mean: 110.02, stddev: 12.56)  CI95: [103.08, 116.96]
    -> std::format: 2.00x FASTER than baseline

--- Correctness Verification ---
  Integer: PASS
  Container: PASS

================================================================================
  Benchmark Complete
================================================================================
```


---

# StrongId

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::StrongId Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1950 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::StrongId<Checked> (primary)
  [x] fat_p::StrongId<Unchecked> (primary)
  [x] fluent::NamedType
  [x] ts::strong_typedef (type_safe)
  [x] strong::type (rollbear)
  [x] boost::strong_typedef
  [x] enum class (built-in)
  [x] Manual wrapper struct (baseline)
  [x] Raw int (baseline)

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. CPU frequency stabilized before measurement

Operations per run: 1000000


------------------------------------------------------------------------
  CONSTRUCTION (from int)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.09       ns  (stddev: 0.01  )  1.00 x
  fat_p::StrongId (Unchecked)   0.10       ns  (stddev: 0.09  )  1.01 x
  fluent::NamedType             0.10       ns  (stddev: 0.01  )  1.03 x
  ts::strong_typedef            0.10       ns  (stddev: 0.02  )  1.02 x
  strong::type (rollbear)       0.20       ns  (stddev: 0.07  )  2.09 x
  boost::strong_typedef         0.09       ns  (stddev: 0.00  )  0.97 x
  enum class                    0.09       ns  (stddev: 0.01  )  0.95 x
  Manual wrapper struct         0.10       ns  (stddev: 0.01  )  1.01 x
  Raw int (baseline)            0.09       ns  (stddev: 0.00  )  1.00 x

------------------------------------------------------------------------
  COMPARISON (operator<)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.37       ns  (stddev: 0.04  )  0.99 x
  fat_p::StrongId (Unchecked)   0.37       ns  (stddev: 0.01  )  0.99 x
  fluent::NamedType             0.37       ns  (stddev: 0.01  )  0.99 x
  ts::strong_typedef            0.37       ns  (stddev: 0.02  )  0.99 x
  strong::type (rollbear)       0.38       ns  (stddev: 0.05  )  1.00 x
  boost::strong_typedef         0.37       ns  (stddev: 0.08  )  0.99 x
  enum class                    0.37       ns  (stddev: 0.02  )  0.99 x
  Manual wrapper struct         0.37       ns  (stddev: 0.12  )  0.99 x
  Raw int (baseline)            0.38       ns  (stddev: 0.09  )  1.00 x

------------------------------------------------------------------------
  ADDITION (compound +=)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     1.39       ns  (stddev: 0.20  )  1.49 x
  fat_p::StrongId (Unchecked)   0.93       ns  (stddev: 0.13  )  1.00 x
  fluent::NamedType             0.93       ns  (stddev: 0.16  )  1.00 x
  ts::strong_typedef            0.93       ns  (stddev: 0.17  )  1.00 x
  strong::type (rollbear)       0.93       ns  (stddev: 0.18  )  1.00 x
  boost::strong_typedef         0.93       ns  (stddev: 0.02  )  1.00 x
  enum class                    0.93       ns  (stddev: 0.02  )  1.00 x
  Manual wrapper struct         0.93       ns  (stddev: 0.01  )  0.99 x
  Raw int (baseline)            0.93       ns  (stddev: 0.05  )  1.00 x

------------------------------------------------------------------------
  INCREMENT (prefix ++)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.20       ns  (stddev: 0.12  )  1.05 x
  fat_p::StrongId (Unchecked)   0.19       ns  (stddev: 0.01  )  1.00 x
  fluent::NamedType             0.19       ns  (stddev: 0.04  )  1.00 x
  ts::strong_typedef            0.19       ns  (stddev: 0.00  )  1.00 x
  strong::type (rollbear)       0.19       ns  (stddev: 0.01  )  1.00 x
  boost::strong_typedef         0.19       ns  (stddev: 0.03  )  1.00 x
  enum class                    0.19       ns  (stddev: 0.03  )  1.00 x
  Manual wrapper struct         0.19       ns  (stddev: 0.04  )  1.00 x
  Raw int (baseline)            0.19       ns  (stddev: 0.01  )  1.00 x

------------------------------------------------------------------------
  HASH (std::hash)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.59       ns  (stddev: 0.07  )  1.00 x
  fat_p::StrongId (Unchecked)   0.59       ns  (stddev: 0.05  )  1.00 x
  fluent::NamedType             0.59       ns  (stddev: 0.04  )  1.00 x
  ts::strong_typedef            0.59       ns  (stddev: 0.02  )  1.00 x
  strong::type (rollbear)       0.61       ns  (stddev: 0.12  )  1.02 x
  boost::strong_typedef         0.59       ns  (stddev: 0.15  )  1.00 x
  enum class                    0.59       ns  (stddev: 0.09  )  1.00 x
  Manual wrapper struct         0.59       ns  (stddev: 0.02  )  1.00 x
  Raw int (baseline)            0.59       ns  (stddev: 0.06  )  1.00 x

========================================================================
  INTERPRETATION GUIDE
========================================================================

  1.00x = Zero overhead (identical to raw int)
  <1.5x = Negligible overhead for most applications
  >2.0x = Measurable overhead, consider for hot paths

  Key findings:
  - fat_p::StrongId (Unchecked) should match raw int exactly
  - fat_p::StrongId (Checked) has overflow detection cost
  - enum class lacks arithmetic/increment operators
  - Manual wrapper validates zero-overhead design pattern

========================================================================
  Benchmark Complete
========================================================================
```

