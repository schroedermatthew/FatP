---
doc_id: BR-BitSet-001
doc_type: "Benchmark Results"
title: "BitSet"
fatp_components: ["BitSet"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - BitSet

**Source:** `benchmark_BitSet.cpp`
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
| fat_p::BitSet | x | x | x | x |
| std::bitset | x | x | x | x |
| boost::dynamic_bitset | x | x | x | x |
| llvm::BitVector | x | x | x | — |
| llvm::SmallBitVector | x | x | x | — |
| roaring::Roaring (CRoaring) | x | x | x | — |
| bm::bvector (BitMagic) | x | x | x | — |
| roaring::Roaring | — | — | — | — |
| bm::bvector | — | — | — | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
======================================================================
  Section 1: Single-Bit Operations (N=1024)
======================================================================

--- Set Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          1.28          0.92        0.41
                 std::bitset          0.48          0.48        0.01
       boost::dynamic_bitset          0.43          0.43        0.00
             llvm::BitVector          0.45          0.45        0.00
            roaring::Roaring         42.08         42.23        0.48
               bm::bvector<>          4.14          3.54        0.74

======================================================================
  Section 2: Population Count (N=1024)
======================================================================

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.19          0.19        0.00
                 std::bitset          3.42          3.63        0.62
       boost::dynamic_bitset         45.45         45.59        0.31
             llvm::BitVector         15.30         15.37        0.17
   roaring::Roaring (cached)          1.96          2.01        0.17
               bm::bvector<>       1220.12       1218.56       80.18

======================================================================
  Section 3: Find Operations (N=1024, k=100)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.87          0.87        0.00
   std::bitset (manual scan)          1.96          2.05        0.24
       boost::dynamic_bitset          1.38          1.38        0.01
             llvm::BitVector          1.09          1.10        0.01
            roaring::Roaring          1.52          1.53        0.01
               bm::bvector<>          2.62          2.62        0.01

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet        318.30        319.33        2.34
     std::bitset (O(N) scan)        524.40        525.15        1.45
       boost::dynamic_bitset        387.60        390.89        9.04
             llvm::BitVector        247.80        248.35        1.31
            roaring::Roaring        426.00        427.29        2.61
               bm::bvector<>       1181.70       1184.01       13.86

======================================================================
  Section 3: Find Operations (N=10000, k=1000)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.87          0.87        0.01
   std::bitset (manual scan)          3.34          3.39        0.15
       boost::dynamic_bitset          1.38          1.38        0.01
             llvm::BitVector          1.31          1.31        0.01
            roaring::Roaring          1.53          1.53        0.02
               bm::bvector<>          2.62          2.62        0.01

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet       3278.70       3281.84       10.43
     std::bitset (O(N) scan)       5092.20       5095.81       11.72
       boost::dynamic_bitset       5042.60       5170.58      428.05
             llvm::BitVector       2946.30       2947.36       20.93
            roaring::Roaring       4226.90       5561.60     2309.85
               bm::bvector<>       3699.60       3701.83       61.52

======================================================================
  Section 4: Sparse Iteration (N=1024, k=50)
======================================================================

Density: 4.88%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        160.43        160.67        0.69
     std::bitset (O(N) scan)        526.66        549.62       52.71
            roaring::Roaring        215.81        230.02       40.01
               bm::bvector<>       1032.67       1089.81      127.21

======================================================================
  Section 4: Sparse Iteration (N=10000, k=100)
======================================================================

Density: 1.00%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        350.51        428.72      145.28
     std::bitset (O(N) scan)       5103.55       5261.08      349.19
            roaring::Roaring        457.57        484.85       84.37
               bm::bvector<>       1121.17       1178.93       98.04

======================================================================
  Section 5: Bitwise Operations (N=1024)
======================================================================

--- AND Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          4.38          4.41        0.10
                 std::bitset          4.53          4.53        0.00
            roaring::Roaring        284.45        288.88        9.56
               bm::bvector<>       2050.24       2049.77       73.09

======================================================================
  Section 6: Range Operations (N=1024, range=100)
======================================================================

Fat-P has native setRange. Others require loops.

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
      fat_p::BitSet (native)          4.11          4.10        0.02
          std::bitset (loop)         91.79         91.98        0.45
    llvm::BitVector (native)         50.93         51.09        0.24
   roaring::Roaring (native)         89.54         90.04        1.16
      bm::bvector<> (native)        298.42        317.96      121.19

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
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
======================================================================
  Section 1: Single-Bit Operations (N=1024)
======================================================================

--- Set Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.54          0.55        0.02
                 std::bitset          1.04          1.06        0.07
       boost::dynamic_bitset          0.54          0.57        0.08
             llvm::BitVector          0.54          0.59        0.09
            roaring::Roaring         51.84         51.96        0.31
               bm::bvector<>          6.93          6.96        0.10

======================================================================
  Section 2: Population Count (N=1024)
======================================================================

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.54          0.56        0.06
                 std::bitset          0.54          0.55        0.02
       boost::dynamic_bitset          0.15          0.15        0.00
             llvm::BitVector          5.31          5.35        0.20
   roaring::Roaring (cached)          2.47          2.49        0.03
               bm::bvector<>          0.02          0.02        0.00

======================================================================
  Section 3: Find Operations (N=1024, k=100)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.15          0.15        0.00
   std::bitset (manual scan)          1.85          1.90        0.12
       boost::dynamic_bitset          0.15          0.15        0.00
             llvm::BitVector          1.54          1.56        0.03
            roaring::Roaring          2.78          2.80        0.03
               bm::bvector<>          3.71          3.73        0.04

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet        318.34        319.97        3.17
     std::bitset (O(N) scan)        644.33        643.89        6.64
       boost::dynamic_bitset        361.56        365.00       12.13
             llvm::BitVector        454.79        454.72       15.14
            roaring::Roaring        283.96        288.12       10.32
               bm::bvector<>       1604.73       1605.29       11.49

======================================================================
  Section 3: Find Operations (N=10000, k=1000)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.93          0.95        0.07
   std::bitset (manual scan)          6.55          6.70        0.46
       boost::dynamic_bitset          0.15          0.15        0.00
             llvm::BitVector          1.54          1.56        0.06
            roaring::Roaring          3.08          3.11        0.05
               bm::bvector<>          3.72          3.78        0.18

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet       4025.46       4029.65        9.39
     std::bitset (O(N) scan)       6224.83       6231.04       17.73
       boost::dynamic_bitset       3452.50       3458.15       23.89
             llvm::BitVector       4468.51       4470.97        9.30
            roaring::Roaring       2708.87       2721.19       32.70
               bm::bvector<>       5613.25       5611.16       54.05

======================================================================
  Section 4: Sparse Iteration (N=1024, k=50)
======================================================================

Density: 4.88%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        153.63        153.56        0.58
     std::bitset (O(N) scan)        643.50        646.43        5.73
            roaring::Roaring        141.18        141.72        1.81
               bm::bvector<>       1350.35       1350.93        4.51

======================================================================
  Section 4: Sparse Iteration (N=10000, k=100)
======================================================================

Density: 1.00%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        432.62        432.68        0.99
     std::bitset (O(N) scan)       6226.06       6228.10        7.71
            roaring::Roaring        264.29        264.80        1.14
               bm::bvector<>       1582.84       1584.75        5.50

======================================================================
  Section 5: Bitwise Operations (N=1024)
======================================================================

--- AND Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.54          0.55        0.01
                 std::bitset          0.54          0.59        0.20
            roaring::Roaring        154.96        155.27        1.91
               bm::bvector<>       3251.28       3252.44        8.54

======================================================================
  Section 6: Range Operations (N=1024, range=100)
======================================================================

Fat-P has native setRange. Others require loops.

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
      fat_p::BitSet (native)          6.71          6.78        0.24
          std::bitset (loop)        108.21        108.42        0.67
    llvm::BitVector (native)         24.00         24.21        0.41
   roaring::Roaring (native)         71.83         72.00        0.69
      bm::bvector<> (native)        292.81        294.95        9.49

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
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
======================================================================
  Section 1: Single-Bit Operations (N=1024)
======================================================================

--- Set Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.58          0.58        0.11
                 std::bitset          0.64          0.65        0.05
       boost::dynamic_bitset          0.47          0.48        0.02
             llvm::BitVector          0.47          0.48        0.02
            roaring::Roaring         52.19         52.21        0.19
               bm::bvector<>          3.33          3.36        0.06

======================================================================
  Section 2: Population Count (N=1024)
======================================================================

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.00          0.00        0.00
                 std::bitset          0.00          0.00        0.00
       boost::dynamic_bitset          0.02          0.02        0.00
             llvm::BitVector          4.98          4.99        0.06
   roaring::Roaring (cached)          3.08          3.11        0.04
               bm::bvector<>       1819.97       1822.06        6.93

======================================================================
  Section 3: Find Operations (N=1024, k=100)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.00          0.00        0.00
   std::bitset (manual scan)          2.04          2.06        0.03
       boost::dynamic_bitset          0.00          0.00        0.00
             llvm::BitVector          0.00          0.00        0.00
            roaring::Roaring          2.47          2.49        0.03
               bm::bvector<>          3.40          3.42        0.04

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet        385.79        388.70        4.36
     std::bitset (O(N) scan)        149.41        150.75        2.86
       boost::dynamic_bitset        336.51        339.36        4.01
             llvm::BitVector        373.20        376.20        4.31
            roaring::Roaring        323.14        325.02        3.62
               bm::bvector<>       1765.80       1765.97        7.79

======================================================================
  Section 3: Find Operations (N=10000, k=1000)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.93          0.94        0.05
   std::bitset (manual scan)          3.78          4.78        1.56
       boost::dynamic_bitset          0.00          0.00        0.00
             llvm::BitVector          0.00          0.00        0.00
            roaring::Roaring          2.47          2.49        0.03
               bm::bvector<>          3.24          3.27        0.04

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet       4509.86       4515.11       17.52
     std::bitset (O(N) scan)       1468.98       1469.44        8.71
       boost::dynamic_bitset       3503.42       3504.25       10.66
             llvm::BitVector       3818.96       3819.54        3.58
            roaring::Roaring       3127.64       3139.22       25.69
               bm::bvector<>       5316.14       5350.57       72.74

======================================================================
  Section 4: Sparse Iteration (N=1024, k=50)
======================================================================

Density: 4.88%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        168.30        168.22        0.48
     std::bitset (O(N) scan)        150.62        150.62        0.48
            roaring::Roaring        160.00        159.85        0.52
               bm::bvector<>       1546.02       1552.98       23.64

======================================================================
  Section 4: Sparse Iteration (N=10000, k=100)
======================================================================

Density: 1.00%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        434.52        434.77        0.95
     std::bitset (O(N) scan)       1458.56       1459.28        1.81
            roaring::Roaring        324.97        324.80        0.94
               bm::bvector<>       1700.49       1802.08      166.75

======================================================================
  Section 5: Bitwise Operations (N=1024)
======================================================================

--- AND Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.00          0.00        0.00
                 std::bitset          0.00          0.00        0.00
            roaring::Roaring        176.67        186.79       35.26
               bm::bvector<>       2938.31       2993.12      419.54

======================================================================
  Section 6: Range Operations (N=1024, range=100)
======================================================================

Fat-P has native setRange. Others require loops.

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
      fat_p::BitSet (native)         11.79         11.93        0.51
          std::bitset (loop)        180.20        183.45        6.01
    llvm::BitVector (native)         25.73         25.77        1.14
   roaring::Roaring (native)         69.72         70.63        3.59
      bm::bvector<> (native)        263.11        264.94        5.48

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
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
======================================================================
  Section 1: Single-Bit Operations (N=1024)
======================================================================

--- Set Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.81          0.83        0.08
                 std::bitset          0.84          0.86        0.08
       boost::dynamic_bitset          0.78          0.80        0.04

======================================================================
  Section 2: Population Count (N=1024)
======================================================================

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          0.31          0.31        0.01
                 std::bitset          4.63          4.69        0.13
       boost::dynamic_bitset         30.51         30.97        1.34

======================================================================
  Section 3: Find Operations (N=1024, k=100)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          1.54          1.56        0.09
   std::bitset (manual scan)          2.47          2.52        0.14
       boost::dynamic_bitset          1.54          1.56        0.06

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet        447.40        454.52       12.67
     std::bitset (O(N) scan)        638.30        647.54       16.34
       boost::dynamic_bitset        467.45        472.66       11.51

======================================================================
  Section 3: Find Operations (N=10000, k=1000)
======================================================================

std::bitset has NO find_first/find_next - requires O(N) scan.

--- find_first ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          1.54          1.58        0.11
   std::bitset (manual scan)          6.78          6.84        0.13
       boost::dynamic_bitset          1.54          1.58        0.10

--- Iterate All Set Bits ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet       4673.60       4682.71       33.98
     std::bitset (O(N) scan)      10747.25      10955.76      499.97
       boost::dynamic_bitset       4825.45       4867.50      102.40

======================================================================
  Section 4: Sparse Iteration (N=1024, k=50)
======================================================================

Density: 4.88%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        241.32        245.13       12.17
     std::bitset (O(N) scan)       1104.97       1124.44       47.87

======================================================================
  Section 4: Sparse Iteration (N=10000, k=100)
======================================================================

Density: 1.00%

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
    fat_p::BitSet (iterator)        525.91        528.78       10.85
     std::bitset (O(N) scan)      10758.64      10775.84       52.21

======================================================================
  Section 5: Bitwise Operations (N=1024)
======================================================================

--- AND Operation ---
                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
               fat_p::BitSet          9.47          9.79        0.82
                 std::bitset         11.26         11.67        1.31

======================================================================
  Section 6: Range Operations (N=1024, range=100)
======================================================================

Fat-P has native setRange. Others require loops.

                     Library    Median(ns)      Mean(ns)     Stddev
--------------------------------------------------------------------
      fat_p::BitSet (native)          7.58          7.57        0.03
          std::bitset (loop)        122.70        122.00        1.65

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

======================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- bm::bvector was not detected on MSVC CI.
- llvm::BitVector was not detected on MSVC CI.
- roaring::Roaring was not detected on MSVC CI.
