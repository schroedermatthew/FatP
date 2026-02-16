---
doc_id: BR-FatPHashMap-001
doc_type: "Benchmark Results"
title: "FatPHashMap"
fatp_components: ["FatPHashMap"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - FatPHashMap

**Source:** `benchmark_FatPHashMap.cpp`
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
| fat_p::StableHashMap | x | x | x | x |
| std::unordered_map | x | x | x | x |
| tsl::robin_map | x | x | x | x |
| ankerl::unordered_dense | x | x | x | x |
| absl::flat_hash_map | x | x | x | x |
| boost::unordered_flat_map | x | x | x | x |
| folly::F14FastMap | — | x | x | x |
| llvm::DenseMap | x | x | x | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
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

[2026-02-15 19:19:06] CPU: 2506 MHz (base: 3686)
N = 10000
[2026-02-15 19:19:06] Insert (amortized) CPU: 2506 MHz (base: 3686)
[2026-02-15 19:19:07] Find(hit) CPU: 2506 MHz (base: 3686)
[2026-02-15 19:19:09] Find(miss) CPU: 2506 MHz (base: 3686)
[2026-02-15 19:19:11] Erase (25%) CPU: 2469 MHz (base: 3686)
[2026-02-15 19:19:12] Churn CPU: 2617 MHz (base: 3686)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]      6.40      4.70      4.15     26.52     19.10
    FastHashMap[BS]+SplitMix64      4.02      2.54      2.79     20.80     14.91
               FastHashMap[TS]      6.66      4.56      4.18      4.24     21.12
    FastHashMap[TS]+SplitMix64      4.19      2.46      2.90      2.56     15.28
                 StableHashMap     25.95      6.60      4.62     18.00     32.94
      StableHashMap+SplitMix64     24.76      3.39      1.98     13.68     28.06
     StableHashMap[Block]+SM64      6.68      3.12      2.05      4.12     14.66
            std::unordered_map     36.67      6.89      8.68     22.16     27.36
                tsl::robin_map      7.15      5.38      5.97      8.56     10.03
       ankerl::unordered_dense     19.69      7.42      2.85     17.88     14.54
           absl::flat_hash_map     12.82      2.93      3.79      6.60     11.20
           absl::node_hash_map     28.92      3.32      3.84     16.44     22.91
     boost::unordered_flat_map      5.30      1.86      1.43      2.56      2.92
     boost::unordered_node_map     25.76      2.19      1.56     12.28     16.30
                llvm::DenseMap      7.01      4.77      9.15      5.28     19.48

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[BS]+SplitMix64 (9.12x), FastHashMap[TS]+SplitMix64 (8.75x), boost::unordered_flat_map (6.92x)
    Top 3 Find: boost::unordered_flat_map (3.70x), FastHashMap[TS]+SplitMix64 (2.80x), FastHashMap[BS]+SplitMix64 (2.71x)
    Top 3 Miss: boost::unordered_flat_map (6.07x), FastHashMap[BS]+SplitMix64 (3.11x), ankerl::unordered_dense (3.05x)
    Top 3 Erase: FastHashMap[TS]+SplitMix64 (8.66x), boost::unordered_flat_map (8.66x), FastHashMap[TS] (5.23x)
    Top 3 Churn: boost::unordered_flat_map (9.36x), tsl::robin_map (2.73x), absl::flat_hash_map (2.44x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (5.49x), StableHashMap+SplitMix64 (1.48x), boost::unordered_node_map (1.42x)
    Top 3 Find: boost::unordered_node_map (3.15x), StableHashMap[Block]+SM64 (2.21x), absl::node_hash_map (2.08x)
    Top 3 Miss: boost::unordered_node_map (5.56x), StableHashMap+SplitMix64 (4.38x), StableHashMap[Block]+SM64 (4.23x)
    Top 3 Erase: StableHashMap[Block]+SM64 (5.38x), boost::unordered_node_map (1.80x), StableHashMap+SplitMix64 (1.62x)
    Top 3 Churn: StableHashMap[Block]+SM64 (1.87x), boost::unordered_node_map (1.68x), absl::node_hash_map (1.19x)

  All Results:
    FastHashMap[BS]                 5.73x insert,  1.47x find,  2.09x miss,  0.84x erase,  1.43x churn
    FastHashMap[BS]+SplitMix64      9.12x insert,  2.71x find,  3.11x miss,  1.07x erase,  1.83x churn
    FastHashMap[TS]                 5.51x insert,  1.51x find,  2.08x miss,  5.23x erase,  1.30x churn
    FastHashMap[TS]+SplitMix64      8.75x insert,  2.80x find,  2.99x miss,  8.66x erase,  1.79x churn
    StableHashMap                   1.41x insert,  1.04x find,  1.88x miss,  1.23x erase,  0.83x churn
    StableHashMap+SplitMix64        1.48x insert,  2.03x find,  4.38x miss,  1.62x erase,  0.98x churn
    StableHashMap[Block]+SM64       5.49x insert,  2.21x find,  4.23x miss,  5.38x erase,  1.87x churn
    tsl::robin_map                  5.13x insert,  1.28x find,  1.45x miss,  2.59x erase,  2.73x churn
    ankerl::unordered_dense         1.86x insert,  0.93x find,  3.05x miss,  1.24x erase,  1.88x churn
    absl::flat_hash_map             2.86x insert,  2.35x find,  2.29x miss,  3.36x erase,  2.44x churn
    absl::node_hash_map             1.27x insert,  2.08x find,  2.26x miss,  1.35x erase,  1.19x churn
    boost::unordered_flat_map       6.92x insert,  3.70x find,  6.07x miss,  8.66x erase,  9.36x churn
    boost::unordered_node_map       1.42x insert,  3.15x find,  5.56x miss,  1.80x erase,  1.68x churn
    llvm::DenseMap                  5.23x insert,  1.44x find,  0.95x miss,  4.20x erase,  1.41x churn
  [NOTE] FastHashMap[TS]+SplitMix64: high variance (stddev 8.23 > median 4.19) - system noise or memory pressure
  [NOTE] ankerl::unordered_dense: high variance (stddev 7.77 > median 2.85) - system noise or memory pressure
  [NOTE] ankerl::unordered_dense: high variance (stddev 31.15 > median 17.88) - system noise or memory pressure

[Cooling: before next size][Waiting: 2395 MHz (variance: 16.9%, need <10.0%)]   
 [Ready: 2506 MHz]
[2026-02-15 19:19:15] CPU: 2506 MHz (base: 3686)
N = 100000
[2026-02-15 19:19:15] Insert (amortized) CPU: 2506 MHz (base: 3686)
[2026-02-15 19:19:17] Find(hit) CPU: 2469 MHz (base: 3686)
[2026-02-15 19:19:22] Find(miss) CPU: 2248 MHz (base: 3686)
[2026-02-15 19:19:25] Erase (25%) CPU: 2432 MHz (base: 3686)
[2026-02-15 19:19:27] Churn CPU: 2322 MHz (base: 3686)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]      7.85      6.76      7.86     38.55     30.05
    FastHashMap[BS]+SplitMix64      5.47      4.32      5.63     31.74     25.46
               FastHashMap[TS]      7.61      5.49      7.59      5.34     18.67
    FastHashMap[TS]+SplitMix64      4.79      3.42      5.41      3.46     13.35
                 StableHashMap     30.34     13.48      8.68     29.02     54.33
      StableHashMap+SplitMix64     30.72      7.92      5.15     26.62     49.15
     StableHashMap[Block]+SM64     11.26      5.70      4.76      7.27     16.01
            std::unordered_map     45.11     10.19     11.39     34.43     56.55
                tsl::robin_map     19.34      7.85      9.97     12.42     13.74
       ankerl::unordered_dense     24.39     11.09      5.93     34.75     27.28
           absl::flat_hash_map     12.90      4.02      6.39     12.13     19.43
           absl::node_hash_map     34.49      5.83      6.48     25.51     52.55
     boost::unordered_flat_map      7.79      2.99      2.44      3.51     11.08
     boost::unordered_node_map     30.84      4.11      2.86     19.71     43.97
                llvm::DenseMap     12.17      3.97      7.24      3.82      9.91

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[TS]+SplitMix64 (9.41x), FastHashMap[BS]+SplitMix64 (8.25x), FastHashMap[TS] (5.93x)
    Top 3 Find: boost::unordered_flat_map (3.40x), FastHashMap[TS]+SplitMix64 (2.98x), llvm::DenseMap (2.56x)
    Top 3 Miss: boost::unordered_flat_map (4.67x), FastHashMap[TS]+SplitMix64 (2.10x), FastHashMap[BS]+SplitMix64 (2.02x)
    Top 3 Erase: FastHashMap[TS]+SplitMix64 (9.94x), boost::unordered_flat_map (9.82x), llvm::DenseMap (9.02x)
    Top 3 Churn: llvm::DenseMap (5.71x), boost::unordered_flat_map (5.10x), FastHashMap[TS]+SplitMix64 (4.24x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (4.01x), StableHashMap (1.49x), StableHashMap+SplitMix64 (1.47x)
    Top 3 Find: boost::unordered_node_map (2.48x), StableHashMap[Block]+SM64 (1.79x), absl::node_hash_map (1.75x)
    Top 3 Miss: boost::unordered_node_map (3.98x), StableHashMap[Block]+SM64 (2.39x), StableHashMap+SplitMix64 (2.21x)
    Top 3 Erase: StableHashMap[Block]+SM64 (4.73x), boost::unordered_node_map (1.75x), absl::node_hash_map (1.35x)
    Top 3 Churn: StableHashMap[Block]+SM64 (3.53x), boost::unordered_node_map (1.29x), StableHashMap+SplitMix64 (1.15x)

  All Results:
    FastHashMap[BS]                 5.74x insert,  1.51x find,  1.45x miss,  0.89x erase,  1.88x churn
    FastHashMap[BS]+SplitMix64      8.25x insert,  2.36x find,  2.02x miss,  1.08x erase,  2.22x churn
    FastHashMap[TS]                 5.93x insert,  1.86x find,  1.50x miss,  6.45x erase,  3.03x churn
    FastHashMap[TS]+SplitMix64      9.41x insert,  2.98x find,  2.10x miss,  9.94x erase,  4.24x churn
    StableHashMap                   1.49x insert,  0.76x find,  1.31x miss,  1.19x erase,  1.04x churn
    StableHashMap+SplitMix64        1.47x insert,  1.29x find,  2.21x miss,  1.29x erase,  1.15x churn
    StableHashMap[Block]+SM64       4.01x insert,  1.79x find,  2.39x miss,  4.73x erase,  3.53x churn
    tsl::robin_map                  2.33x insert,  1.30x find,  1.14x miss,  2.77x erase,  4.12x churn
    ankerl::unordered_dense         1.85x insert,  0.92x find,  1.92x miss,  0.99x erase,  2.07x churn
    absl::flat_hash_map             3.50x insert,  2.53x find,  1.78x miss,  2.84x erase,  2.91x churn
    absl::node_hash_map             1.31x insert,  1.75x find,  1.76x miss,  1.35x erase,  1.08x churn
    boost::unordered_flat_map       5.79x insert,  3.40x find,  4.67x miss,  9.82x erase,  5.10x churn
    boost::unordered_node_map       1.46x insert,  2.48x find,  3.98x miss,  1.75x erase,  1.29x churn
    llvm::DenseMap                  3.71x insert,  2.56x find,  1.57x miss,  9.02x erase,  5.71x churn

[Cooling: before next size][CPU stable at 2182 MHz (59% of base, variance: 5.1%)]
 [Ready: 2211 MHz]
[2026-02-15 19:19:31] CPU: 2211 MHz (base: 3686)
N = 1000000
[2026-02-15 19:19:31] Insert (amortized) CPU: 2211 MHz (base: 3686)
[2026-02-15 19:19:46] Find(hit) CPU: 2248 MHz (base: 3686)
[2026-02-15 19:20:04] Find(miss) CPU: 2027 MHz (base: 3686)
[2026-02-15 19:20:20] Erase (25%) CPU: 2101 MHz (base: 3686)
[2026-02-15 19:20:37] Churn CPU: 2285 MHz (base: 3686)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     13.75     26.45      6.72     47.14     37.53
    FastHashMap[BS]+SplitMix64     10.47     19.46      4.68     35.65     28.83
               FastHashMap[TS]     13.07     21.36      5.72     17.89     21.90
    FastHashMap[TS]+SplitMix64      9.96     15.26      4.03     12.53     15.23
                 StableHashMap     40.43     21.53      6.52    105.76    124.10
      StableHashMap+SplitMix64     37.32     14.00      3.47     97.53    118.35
     StableHashMap[Block]+SM64     17.53     10.26      3.04     24.95     31.73
            std::unordered_map     90.96     27.70     34.93    127.90    212.30
                tsl::robin_map     26.80     21.82     20.74     30.38     32.51
       ankerl::unordered_dense     37.18      8.35      5.24     32.86     23.35
           absl::flat_hash_map     22.93     17.85      4.54     22.09     26.40
           absl::node_hash_map     42.62     17.75      8.66    104.09    123.34
     boost::unordered_flat_map     12.02      8.95      2.37      9.95      9.42
     boost::unordered_node_map     38.24     10.70      4.52     81.16    103.11
                llvm::DenseMap     18.87     10.27     14.62      8.96     20.20

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[TS]+SplitMix64 (9.14x), FastHashMap[BS]+SplitMix64 (8.69x), boost::unordered_flat_map (7.56x)
    Top 3 Find: ankerl::unordered_dense (3.32x), boost::unordered_flat_map (3.09x), llvm::DenseMap (2.70x)
    Top 3 Miss: boost::unordered_flat_map (14.76x), FastHashMap[TS]+SplitMix64 (8.66x), absl::flat_hash_map (7.70x)
    Top 3 Erase: llvm::DenseMap (14.28x), boost::unordered_flat_map (12.85x), FastHashMap[TS]+SplitMix64 (10.21x)
    Top 3 Churn: boost::unordered_flat_map (22.54x), FastHashMap[TS]+SplitMix64 (13.94x), llvm::DenseMap (10.51x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (5.19x), StableHashMap+SplitMix64 (2.44x), boost::unordered_node_map (2.38x)
    Top 3 Find: StableHashMap[Block]+SM64 (2.70x), boost::unordered_node_map (2.59x), StableHashMap+SplitMix64 (1.98x)
    Top 3 Miss: StableHashMap[Block]+SM64 (11.50x), StableHashMap+SplitMix64 (10.07x), boost::unordered_node_map (7.73x)
    Top 3 Erase: StableHashMap[Block]+SM64 (5.13x), boost::unordered_node_map (1.58x), StableHashMap+SplitMix64 (1.31x)
    Top 3 Churn: StableHashMap[Block]+SM64 (6.69x), boost::unordered_node_map (2.06x), StableHashMap+SplitMix64 (1.79x)

  All Results:
    FastHashMap[BS]                 6.62x insert,  1.05x find,  5.20x miss,  2.71x erase,  5.66x churn
    FastHashMap[BS]+SplitMix64      8.69x insert,  1.42x find,  7.46x miss,  3.59x erase,  7.36x churn
    FastHashMap[TS]                 6.96x insert,  1.30x find,  6.11x miss,  7.15x erase,  9.69x churn
    FastHashMap[TS]+SplitMix64      9.14x insert,  1.82x find,  8.66x miss, 10.21x erase, 13.94x churn
    StableHashMap                   2.25x insert,  1.29x find,  5.36x miss,  1.21x erase,  1.71x churn
    StableHashMap+SplitMix64        2.44x insert,  1.98x find, 10.07x miss,  1.31x erase,  1.79x churn
    StableHashMap[Block]+SM64       5.19x insert,  2.70x find, 11.50x miss,  5.13x erase,  6.69x churn
    tsl::robin_map                  3.39x insert,  1.27x find,  1.68x miss,  4.21x erase,  6.53x churn
    ankerl::unordered_dense         2.45x insert,  3.32x find,  6.67x miss,  3.89x erase,  9.09x churn
    absl::flat_hash_map             3.97x insert,  1.55x find,  7.70x miss,  5.79x erase,  8.04x churn
    absl::node_hash_map             2.13x insert,  1.56x find,  4.03x miss,  1.23x erase,  1.72x churn
    boost::unordered_flat_map       7.56x insert,  3.09x find, 14.76x miss, 12.85x erase, 22.54x churn
    boost::unordered_node_map       2.38x insert,  2.59x find,  7.73x miss,  1.58x erase,  2.06x churn
    llvm::DenseMap                  4.82x insert,  2.70x find,  2.39x miss, 14.28x erase, 10.51x churn
  [NOTE] std::unordered_map: median 127.90 ns (node-based pointer chasing / cache locality effects)
  [NOTE] std::unordered_map: median 212.30 ns (node-based pointer chasing / cache locality effects)

--- Detailed Statistics for FastHashMap[BS] at N=1000000 ---
  Insert (amortized): median=   13.75 mean=   14.08 +/-  1.21 CI95(mean)=[13.47,14.69] min=13.42 max=18.38
     Find(hit): median=   26.45 mean=   27.19 +/-  1.58 CI95(mean)=[26.39,27.99] min=25.77 max=31.50
    Find(miss): median=    6.72 mean=    6.78 +/-  0.32 CI95(mean)=[6.62,6.95] min=6.38 max=7.39
   Erase (25%): median=   47.14 mean=   47.84 +/-  1.85 CI95(mean)=[46.90,48.78] min=45.55 max=53.47
         Churn: median=   37.53 mean=   37.57 +/-  0.65 CI95(mean)=[37.24,37.90] min=36.21 max=38.72

--- Detailed Statistics for FastHashMap[BS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   10.47 mean=   10.49 +/-  0.38 CI95(mean)=[10.30,10.68] min=9.84 max=11.20
     Find(hit): median=   19.46 mean=   19.75 +/-  1.06 CI95(mean)=[19.21,20.28] min=18.65 max=22.63
    Find(miss): median=    4.68 mean=    4.83 +/-  0.48 CI95(mean)=[4.58,5.07] min=4.38 max=5.86
   Erase (25%): median=   35.65 mean=   35.79 +/-  1.40 CI95(mean)=[35.08,36.49] min=34.04 max=39.32
         Churn: median=   28.83 mean=   28.94 +/-  0.56 CI95(mean)=[28.66,29.23] min=27.89 max=29.90

--- Detailed Statistics for FastHashMap[TS] at N=1000000 ---
  Insert (amortized): median=   13.07 mean=   13.26 +/-  0.69 CI95(mean)=[12.91,13.61] min=12.53 max=15.39
     Find(hit): median=   21.36 mean=   22.08 +/-  1.51 CI95(mean)=[21.32,22.85] min=20.58 max=24.82
    Find(miss): median=    5.72 mean=    5.84 +/-  0.34 CI95(mean)=[5.67,6.02] min=5.43 max=6.71
   Erase (25%): median=   17.89 mean=   18.50 +/-  2.14 CI95(mean)=[17.42,19.58] min=16.09 max=23.64
         Churn: median=   21.90 mean=   22.39 +/-  1.33 CI95(mean)=[21.71,23.06] min=20.62 max=25.91

--- Detailed Statistics for FastHashMap[TS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=    9.96 mean=    9.98 +/-  0.28 CI95(mean)=[9.84,10.12] min=9.44 max=10.53
     Find(hit): median=   15.26 mean=   15.57 +/-  1.08 CI95(mean)=[15.03,16.12] min=14.17 max=17.98
    Find(miss): median=    4.03 mean=    4.03 +/-  0.29 CI95(mean)=[3.89,4.18] min=3.68 max=4.57
   Erase (25%): median=   12.53 mean=   12.69 +/-  1.61 CI95(mean)=[11.88,13.51] min=11.08 max=17.03
         Churn: median=   15.23 mean=   15.36 +/-  0.71 CI95(mean)=[15.00,15.72] min=14.36 max=16.72

--- Detailed Statistics for StableHashMap at N=1000000 ---
  Insert (amortized): median=   40.43 mean=   40.82 +/-  2.25 CI95(mean)=[39.68,41.96] min=37.60 max=45.29
     Find(hit): median=   21.53 mean=   22.07 +/-  1.45 CI95(mean)=[21.33,22.80] min=20.24 max=25.36
    Find(miss): median=    6.52 mean=    6.47 +/-  0.33 CI95(mean)=[6.30,6.64] min=5.99 max=7.32
   Erase (25%): median=  105.76 mean=  105.11 +/-  3.97 CI95(mean)=[103.09,107.12] min=98.67 max=112.14
         Churn: median=  124.10 mean=  123.67 +/-  6.01 CI95(mean)=[120.63,126.72] min=108.92 max=137.99

--- Detailed Statistics for StableHashMap+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   37.32 mean=   37.62 +/-  1.46 CI95(mean)=[36.89,38.36] min=36.00 max=40.88
     Find(hit): median=   14.00 mean=   14.28 +/-  0.87 CI95(mean)=[13.84,14.73] min=12.88 max=15.77
    Find(miss): median=    3.47 mean=    3.44 +/-  0.20 CI95(mean)=[3.34,3.54] min=3.12 max=3.86
   Erase (25%): median=   97.53 mean=   98.11 +/-  5.73 CI95(mean)=[95.21,101.01] min=91.13 max=111.14
         Churn: median=  118.35 mean=  118.27 +/-  3.08 CI95(mean)=[116.71,119.83] min=112.99 max=123.37

--- Detailed Statistics for StableHashMap[Block]+SM64 at N=1000000 ---
  Insert (amortized): median=   17.53 mean=   17.79 +/-  1.29 CI95(mean)=[17.14,18.45] min=16.52 max=21.08
     Find(hit): median=   10.26 mean=   10.39 +/-  0.78 CI95(mean)=[9.99,10.78] min=8.88 max=12.16
    Find(miss): median=    3.04 mean=    3.11 +/-  0.30 CI95(mean)=[2.96,3.26] min=2.79 max=3.90
   Erase (25%): median=   24.95 mean=   25.64 +/-  2.88 CI95(mean)=[24.18,27.10] min=22.24 max=33.19
         Churn: median=   31.73 mean=   31.81 +/-  1.73 CI95(mean)=[30.94,32.68] min=29.15 max=34.83

[Cooling: before miss diagnostics][Waiting: 2329 MHz (variance: 47.5%, need <10.0%)]   
 [Ready: 2395 MHz]

================================================================================
  MISS DIAGNOSTICS (Slim)
================================================================================

Random misses only (no H2-biased sets), fixed N=1,000,000.
Purpose: regression tripwire for unsuccessful lookup behavior.

[2026-02-15 19:21:27] MissDiag CPU: 2211 MHz (base: 3686)

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=1000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        3.28          0.01          1.00        1.00          7.65        0.00        0.01
StableHashMap[Block]+SM64 (counted)        3.15          0.01          1.00        1.00          7.65        0.00        0.01
boost::unordered_node_map+SM64 (counted)        4.99          0.03          1.00           -             -           -           -
[Cooling: miss reserve change][Waiting: 2204 MHz (variance: 25.1%, need <10.0%)]   
 [Ready: 2359 MHz]

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=2000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        3.85          0.00          1.00        1.00          3.81        0.00        0.00
StableHashMap[Block]+SM64 (counted)        3.72          0.00          1.00        1.00          3.81        0.00        0.00
boost::unordered_node_map+SM64 (counted)        4.87          0.02          1.00           -             -           -           -

[Cooling: before pathological erase][CPU stable at 2204 MHz (60% of base, variance: 3.3%)]
 [Ready: 2174 MHz]

================================================================================
  PATHOLOGICAL ERASE (TOMBSTONE DEGRADATION TEST)
================================================================================

Tests sustained churn on a single table without reset.
Tombstone-based maps may degrade over time.
Backward-shift maps stay stable.
Methodology: 3 warmup + 15 measured runs
             Round-robin execution with randomized order

[2026-02-15 19:21:40] Starting CPU: 2174 MHz (base: 3686)
N = 100000, Total operations = 5000000

               StableHashMap:    98.73 ns/step (+/-0.88, CI:[98.11,99.01])
    StableHashMap+SplitMix64:    89.04 ns/step (+/-0.78, CI:[88.80,89.59])
   StableHashMap[Block]+SM64:    23.08 ns/step (+/-0.21, CI:[23.02,23.24])
             FastHashMap[BS]:    30.01 ns/step (+/-0.25, CI:[29.95,30.21])
  FastHashMap[BS]+SplitMix64:    22.49 ns/step (+/-0.22, CI:[22.45,22.67])
             FastHashMap[TS]:    28.52 ns/step (+/-0.25, CI:[28.38,28.64])
  FastHashMap[TS]+SplitMix64:    18.35 ns/step (+/-0.25, CI:[18.32,18.58])
              tsl::robin_map:    16.97 ns/step (+/-0.16, CI:[16.95,17.11])
     ankerl::unordered_dense:    20.25 ns/step (+/-0.19, CI:[20.20,20.39])
         absl::flat_hash_map:    22.78 ns/step (+/-0.18, CI:[22.76,22.94])
         absl::node_hash_map:    88.30 ns/step (+/-1.41, CI:[87.81,89.23])
   boost::unordered_flat_map:     7.33 ns/step (+/-0.16, CI:[7.30,7.45])
   boost::unordered_node_map:    73.10 ns/step (+/-0.61, CI:[72.85,73.47])
              llvm::DenseMap:    21.12 ns/step (+/-0.30, CI:[21.05,21.35])
          std::unordered_map:   108.87 ns/step (+/-1.26, CI:[108.05,109.32])

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  CORE OPERATIONS BENCHMARK (Round-Robin)
================================================================================

Comparing FastHashMap and StableHashMap (std::hash vs SplitMix64Hash)
vs std::unordered_map vs tsl::robin_map vs ankerl::unordered_dense vs absl::flat_hash_map vs boost::unordered_flat_map vs folly::F14FastMap vs llvm::DenseMap

Methodology:
  - 3 warmup + 20 measured runs per test
  - Round-robin execution with randomized order per run
  - All libraries observe same distribution of machine states
  - Primary metric: median (ns/op)
  - FastHashMap SIMD backend: AVX2
  - StableHashMap SIMD backend: AVX2
  - FastHashMap policies: BackwardShift (BS), Tombstone (TS)
  - StableHashMap: Reference-stable (pointers valid across insert/reserve)

Cases (ns/op):
  Insert: insert N unique keys into empty map (after reserve)
  Find(hit): find N present keys
  Find(miss): find N absent keys
  Erase: erase 25% of present keys (random order)
  Churn: key replacement churn (erase one existing key, insert new key; size constant)

[2026-02-16 03:38:06] CPU: 3209 MHz (~base: 3209)
N = 10000
[2026-02-16 03:38:06] Insert (amortized) CPU: 3209 MHz (~base: 3209)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:09] Find(hit) CPU: 2594 MHz (~base: 2594)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:13] Find(miss) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:16] Erase (25%) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:19] Churn CPU: 3245 MHz (~base: 3245)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]      7.63      4.64      4.87     30.67     25.08
    FastHashMap[BS]+SplitMix64      7.36      4.50      4.98     31.50     24.51
               FastHashMap[TS]      7.89      4.26      5.09      5.76     18.82
    FastHashMap[TS]+SplitMix64      8.05      4.07      5.05      5.41     19.43
                 StableHashMap     20.43      4.66      2.90     24.51     35.06
      StableHashMap+SplitMix64     20.74      4.65      2.88     24.40     34.63
     StableHashMap[Block]+SM64     10.69      4.42      2.87     10.61     26.79
            std::unordered_map     32.44     11.43     16.91     46.44     37.04
                tsl::robin_map      9.36      4.35      5.03      7.68    766.43
       ankerl::unordered_dense     13.03     10.14      4.37     25.34     20.82
           absl::flat_hash_map     15.23      4.08      5.00     11.27     14.69
           absl::node_hash_map     19.87      4.34      5.29     23.53     22.93
     boost::unordered_flat_map      5.71      3.17      2.36      4.30      5.80
     boost::unordered_node_map     17.64      3.36      2.45     20.29     18.22
             folly::F14FastMap      7.86      4.09      4.64      8.77     14.38
             folly::F14NodeMap     18.99      4.28      4.61     22.42     24.10
                llvm::DenseMap     13.06      6.98     13.36      7.45     24.20

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: boost::unordered_flat_map (5.68x), FastHashMap[BS]+SplitMix64 (4.40x), FastHashMap[BS] (4.25x)
    Top 3 Find: boost::unordered_flat_map (3.61x), FastHashMap[TS]+SplitMix64 (2.81x), absl::flat_hash_map (2.80x)
    Top 3 Miss: boost::unordered_flat_map (7.16x), ankerl::unordered_dense (3.87x), folly::F14FastMap (3.65x)
    Top 3 Erase: boost::unordered_flat_map (10.80x), FastHashMap[TS]+SplitMix64 (8.58x), FastHashMap[TS] (8.06x)
    Top 3 Churn: boost::unordered_flat_map (6.38x), folly::F14FastMap (2.58x), absl::flat_hash_map (2.52x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (3.03x), boost::unordered_node_map (1.84x), folly::F14NodeMap (1.71x)
    Top 3 Find: boost::unordered_node_map (3.40x), folly::F14NodeMap (2.67x), absl::node_hash_map (2.64x)
    Top 3 Miss: boost::unordered_node_map (6.91x), StableHashMap[Block]+SM64 (5.89x), StableHashMap+SplitMix64 (5.88x)
    Top 3 Erase: StableHashMap[Block]+SM64 (4.38x), boost::unordered_node_map (2.29x), folly::F14NodeMap (2.07x)
    Top 3 Churn: boost::unordered_node_map (2.03x), absl::node_hash_map (1.62x), folly::F14NodeMap (1.54x)

  All Results:
    FastHashMap[BS]                 4.25x insert,  2.46x find,  3.47x miss,  1.51x erase,  1.48x churn
    FastHashMap[BS]+SplitMix64      4.40x insert,  2.54x find,  3.40x miss,  1.47x erase,  1.51x churn
    FastHashMap[TS]                 4.11x insert,  2.68x find,  3.32x miss,  8.06x erase,  1.97x churn
    FastHashMap[TS]+SplitMix64      4.03x insert,  2.81x find,  3.35x miss,  8.58x erase,  1.91x churn
    StableHashMap                   1.59x insert,  2.45x find,  5.82x miss,  1.89x erase,  1.06x churn
    StableHashMap+SplitMix64        1.56x insert,  2.46x find,  5.88x miss,  1.90x erase,  1.07x churn
    StableHashMap[Block]+SM64       3.03x insert,  2.58x find,  5.89x miss,  4.38x erase,  1.38x churn
    tsl::robin_map                  3.47x insert,  2.63x find,  3.36x miss,  6.05x erase,  0.05x churn
    ankerl::unordered_dense         2.49x insert,  1.13x find,  3.87x miss,  1.83x erase,  1.78x churn
    absl::flat_hash_map             2.13x insert,  2.80x find,  3.38x miss,  4.12x erase,  2.52x churn
    absl::node_hash_map             1.63x insert,  2.64x find,  3.20x miss,  1.97x erase,  1.62x churn
    boost::unordered_flat_map       5.68x insert,  3.61x find,  7.16x miss, 10.80x erase,  6.38x churn
    boost::unordered_node_map       1.84x insert,  3.40x find,  6.91x miss,  2.29x erase,  2.03x churn
    folly::F14FastMap               4.13x insert,  2.79x find,  3.65x miss,  5.30x erase,  2.58x churn
    folly::F14NodeMap               1.71x insert,  2.67x find,  3.67x miss,  2.07x erase,  1.54x churn
    llvm::DenseMap                  2.48x insert,  1.64x find,  1.27x miss,  6.24x erase,  1.53x churn

[Cooling: before next size][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]
[2026-02-16 03:38:23] CPU: 2594 MHz (~base: 2594)
N = 100000
[2026-02-16 03:38:23] Insert (amortized) CPU: 2594 MHz (~base: 2594)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:28] Find(hit) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:33] Find(miss) CPU: 2626 MHz (~base: 2626)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:37] Erase (25%) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:38:42] Churn CPU: 2445 MHz (~base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]      9.80      6.80      7.22     54.69     42.42
    FastHashMap[BS]+SplitMix64      9.73      6.56      7.19     56.00     42.80
               FastHashMap[TS]     10.37      7.14      7.49      8.04     24.19
    FastHashMap[TS]+SplitMix64     10.41      6.91      7.23      7.88     24.07
                 StableHashMap     23.78      8.08      5.35     44.48     39.22
      StableHashMap+SplitMix64     24.00      8.18      5.34     44.78     38.86
     StableHashMap[Block]+SM64     16.42      7.93      5.27     18.87     27.25
            std::unordered_map     45.71     14.21     20.71     78.31     51.20
                tsl::robin_map     12.50      5.89      7.88     11.92  14898.18
       ankerl::unordered_dense     20.06     16.57      9.11     42.43     35.87
           absl::flat_hash_map     19.63      6.51      9.27     17.96     29.60
           absl::node_hash_map     25.68      6.75      9.73     35.48     42.83
     boost::unordered_flat_map      8.57      5.57      3.97      7.00     18.19
     boost::unordered_node_map     22.29      5.71      4.38     32.49     39.54
             folly::F14FastMap     10.72      6.15      4.63     12.36     13.13
             folly::F14NodeMap     29.04      6.46      4.81     33.29     27.43
                llvm::DenseMap     13.61      5.53     10.00      5.72     16.71

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: boost::unordered_flat_map (5.33x), FastHashMap[BS]+SplitMix64 (4.70x), FastHashMap[BS] (4.66x)
    Top 3 Find: llvm::DenseMap (2.57x), boost::unordered_flat_map (2.55x), tsl::robin_map (2.41x)
    Top 3 Miss: boost::unordered_flat_map (5.21x), folly::F14FastMap (4.47x), FastHashMap[BS]+SplitMix64 (2.88x)
    Top 3 Erase: llvm::DenseMap (13.70x), boost::unordered_flat_map (11.18x), FastHashMap[TS]+SplitMix64 (9.94x)
    Top 3 Churn: folly::F14FastMap (3.90x), llvm::DenseMap (3.06x), boost::unordered_flat_map (2.81x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (2.78x), boost::unordered_node_map (2.05x), StableHashMap (1.92x)
    Top 3 Find: boost::unordered_node_map (2.49x), folly::F14NodeMap (2.20x), absl::node_hash_map (2.10x)
    Top 3 Miss: boost::unordered_node_map (4.73x), folly::F14NodeMap (4.30x), StableHashMap[Block]+SM64 (3.93x)
    Top 3 Erase: StableHashMap[Block]+SM64 (4.15x), boost::unordered_node_map (2.41x), folly::F14NodeMap (2.35x)
    Top 3 Churn: StableHashMap[Block]+SM64 (1.88x), folly::F14NodeMap (1.87x), StableHashMap+SplitMix64 (1.32x)

  All Results:
    FastHashMap[BS]                 4.66x insert,  2.09x find,  2.87x miss,  1.43x erase,  1.21x churn
    FastHashMap[BS]+SplitMix64      4.70x insert,  2.17x find,  2.88x miss,  1.40x erase,  1.20x churn
    FastHashMap[TS]                 4.41x insert,  1.99x find,  2.77x miss,  9.74x erase,  2.12x churn
    FastHashMap[TS]+SplitMix64      4.39x insert,  2.06x find,  2.87x miss,  9.94x erase,  2.13x churn
    StableHashMap                   1.92x insert,  1.76x find,  3.87x miss,  1.76x erase,  1.31x churn
    StableHashMap+SplitMix64        1.90x insert,  1.74x find,  3.88x miss,  1.75x erase,  1.32x churn
    StableHashMap[Block]+SM64       2.78x insert,  1.79x find,  3.93x miss,  4.15x erase,  1.88x churn
    tsl::robin_map                  3.66x insert,  2.41x find,  2.63x miss,  6.57x erase,  0.00x churn
    ankerl::unordered_dense         2.28x insert,  0.86x find,  2.27x miss,  1.85x erase,  1.43x churn
    absl::flat_hash_map             2.33x insert,  2.18x find,  2.23x miss,  4.36x erase,  1.73x churn
    absl::node_hash_map             1.78x insert,  2.10x find,  2.13x miss,  2.21x erase,  1.20x churn
    boost::unordered_flat_map       5.33x insert,  2.55x find,  5.21x miss, 11.18x erase,  2.81x churn
    boost::unordered_node_map       2.05x insert,  2.49x find,  4.73x miss,  2.41x erase,  1.29x churn
    folly::F14FastMap               4.26x insert,  2.31x find,  4.47x miss,  6.34x erase,  3.90x churn
    folly::F14NodeMap               1.57x insert,  2.20x find,  4.30x miss,  2.35x erase,  1.87x churn
    llvm::DenseMap                  3.36x insert,  2.57x find,  2.07x miss, 13.70x erase,  3.06x churn

[Cooling: before next size][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]
[2026-02-16 03:39:58] CPU: 2445 MHz (~base: 2445)
N = 1000000
[2026-02-16 03:39:58] Insert (amortized) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:40:28] Find(hit) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:41:02] Find(miss) CPU: 2540 MHz (~base: 2540)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:41:34] Erase (25%) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 03:42:06] Churn CPU: 2445 MHz (~base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     15.78     22.56      9.73     47.13     42.81
    FastHashMap[BS]+SplitMix64     15.84     21.31      9.37     46.03     41.34
               FastHashMap[TS]     15.44     19.50      8.87     21.61     29.39
    FastHashMap[TS]+SplitMix64     15.70     19.67      8.79     20.37     29.71
                 StableHashMap     42.51     16.07      5.38    143.56     89.37
      StableHashMap+SplitMix64     43.54     16.28      5.40    145.74     86.95
     StableHashMap[Block]+SM64     26.16     13.95      5.24     37.78     43.55
            std::unordered_map     80.57     26.96     31.80    210.77    135.38
                tsl::robin_map     32.19     14.07     14.24     24.64  16800.65
       ankerl::unordered_dense     16.95     10.03      5.44     37.74     31.67
           absl::flat_hash_map     36.97     21.30      7.81     29.13     27.62
           absl::node_hash_map     43.27     15.72     11.11    112.65     79.97
     boost::unordered_flat_map     12.27     10.11      3.67     11.96     13.81
     boost::unordered_node_map     40.03     10.30      5.83    111.85     74.81
             folly::F14FastMap     25.77     16.97     11.71     25.87     30.90
             folly::F14NodeMap     46.41      9.33      8.51    112.02     74.48
                llvm::DenseMap     33.19     10.62     18.56      9.45     29.80

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: boost::unordered_flat_map (6.57x), FastHashMap[TS] (5.22x), FastHashMap[TS]+SplitMix64 (5.13x)
    Top 3 Find: ankerl::unordered_dense (2.69x), boost::unordered_flat_map (2.67x), llvm::DenseMap (2.54x)
    Top 3 Miss: boost::unordered_flat_map (8.68x), ankerl::unordered_dense (5.85x), absl::flat_hash_map (4.07x)
    Top 3 Erase: llvm::DenseMap (22.30x), boost::unordered_flat_map (17.63x), FastHashMap[TS]+SplitMix64 (10.35x)
    Top 3 Churn: boost::unordered_flat_map (9.80x), absl::flat_hash_map (4.90x), FastHashMap[TS] (4.61x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (3.08x), boost::unordered_node_map (2.01x), StableHashMap (1.90x)
    Top 3 Find: folly::F14NodeMap (2.89x), boost::unordered_node_map (2.62x), StableHashMap[Block]+SM64 (1.93x)
    Top 3 Miss: StableHashMap[Block]+SM64 (6.06x), StableHashMap (5.91x), StableHashMap+SplitMix64 (5.88x)
    Top 3 Erase: StableHashMap[Block]+SM64 (5.58x), boost::unordered_node_map (1.88x), folly::F14NodeMap (1.88x)
    Top 3 Churn: StableHashMap[Block]+SM64 (3.11x), folly::F14NodeMap (1.82x), boost::unordered_node_map (1.81x)

  All Results:
    FastHashMap[BS]                 5.10x insert,  1.20x find,  3.27x miss,  4.47x erase,  3.16x churn
    FastHashMap[BS]+SplitMix64      5.09x insert,  1.27x find,  3.39x miss,  4.58x erase,  3.27x churn
    FastHashMap[TS]                 5.22x insert,  1.38x find,  3.59x miss,  9.76x erase,  4.61x churn
    FastHashMap[TS]+SplitMix64      5.13x insert,  1.37x find,  3.62x miss, 10.35x erase,  4.56x churn
    StableHashMap                   1.90x insert,  1.68x find,  5.91x miss,  1.47x erase,  1.51x churn
    StableHashMap+SplitMix64        1.85x insert,  1.66x find,  5.88x miss,  1.45x erase,  1.56x churn
    StableHashMap[Block]+SM64       3.08x insert,  1.93x find,  6.06x miss,  5.58x erase,  3.11x churn
    tsl::robin_map                  2.50x insert,  1.92x find,  2.23x miss,  8.55x erase,  0.01x churn
    ankerl::unordered_dense         4.75x insert,  2.69x find,  5.85x miss,  5.58x erase,  4.28x churn
    absl::flat_hash_map             2.18x insert,  1.27x find,  4.07x miss,  7.23x erase,  4.90x churn
    absl::node_hash_map             1.86x insert,  1.71x find,  2.86x miss,  1.87x erase,  1.69x churn
    boost::unordered_flat_map       6.57x insert,  2.67x find,  8.68x miss, 17.63x erase,  9.80x churn
    boost::unordered_node_map       2.01x insert,  2.62x find,  5.46x miss,  1.88x erase,  1.81x churn
    folly::F14FastMap               3.13x insert,  1.59x find,  2.72x miss,  8.15x erase,  4.38x churn
    folly::F14NodeMap               1.74x insert,  2.89x find,  3.74x miss,  1.88x erase,  1.82x churn
    llvm::DenseMap                  2.43x insert,  2.54x find,  1.71x miss, 22.30x erase,  4.54x churn
  [NOTE] std::unordered_map: median 210.77 ns (node-based pointer chasing / cache locality effects)
  [NOTE] std::unordered_map: median 135.38 ns (node-based pointer chasing / cache locality effects)

--- Detailed Statistics for FastHashMap[BS] at N=1000000 ---
  Insert (amortized): median=   15.78 mean=   15.89 +/-  0.56 CI95(mean)=[15.64,16.13] min=15.21 max=17.40
     Find(hit): median=   22.56 mean=   22.51 +/-  0.85 CI95(mean)=[22.14,22.88] min=20.93 max=24.38
    Find(miss): median=    9.73 mean=    9.78 +/-  0.62 CI95(mean)=[9.51,10.05] min=8.87 max=11.51
   Erase (25%): median=   47.13 mean=   47.49 +/-  1.86 CI95(mean)=[46.68,48.31] min=44.69 max=50.59
         Churn: median=   42.81 mean=   42.85 +/-  0.67 CI95(mean)=[42.56,43.15] min=41.80 max=44.13

--- Detailed Statistics for FastHashMap[BS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   15.84 mean=   16.03 +/-  0.58 CI95(mean)=[15.77,16.28] min=15.45 max=17.36
     Find(hit): median=   21.31 mean=   21.19 +/-  0.63 CI95(mean)=[20.92,21.47] min=20.09 max=22.31
    Find(miss): median=    9.37 mean=    9.41 +/-  0.40 CI95(mean)=[9.23,9.59] min=8.82 max=10.42
   Erase (25%): median=   46.03 mean=   46.14 +/-  1.38 CI95(mean)=[45.53,46.75] min=44.48 max=48.94
         Churn: median=   41.34 mean=   41.63 +/-  1.36 CI95(mean)=[41.03,42.22] min=40.25 max=46.58

--- Detailed Statistics for FastHashMap[TS] at N=1000000 ---
  Insert (amortized): median=   15.44 mean=   15.57 +/-  0.76 CI95(mean)=[15.24,15.90] min=14.59 max=17.79
     Find(hit): median=   19.50 mean=   19.41 +/-  1.65 CI95(mean)=[18.69,20.14] min=16.59 max=21.85
    Find(miss): median=    8.87 mean=    8.93 +/-  0.63 CI95(mean)=[8.65,9.20] min=7.92 max=10.17
   Erase (25%): median=   21.61 mean=   21.44 +/-  1.89 CI95(mean)=[20.61,22.27] min=18.43 max=25.18
         Churn: median=   29.39 mean=   29.55 +/-  1.49 CI95(mean)=[28.90,30.21] min=26.43 max=32.88

--- Detailed Statistics for FastHashMap[TS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   15.70 mean=   15.75 +/-  0.66 CI95(mean)=[15.46,16.04] min=14.88 max=17.65
     Find(hit): median=   19.67 mean=   19.99 +/-  1.64 CI95(mean)=[19.27,20.71] min=17.38 max=24.02
    Find(miss): median=    8.79 mean=    8.73 +/-  0.56 CI95(mean)=[8.49,8.98] min=7.85 max=9.83
   Erase (25%): median=   20.37 mean=   19.96 +/-  1.30 CI95(mean)=[19.39,20.53] min=17.08 max=21.61
         Churn: median=   29.71 mean=   29.94 +/-  1.49 CI95(mean)=[29.29,30.59] min=28.10 max=32.93

--- Detailed Statistics for StableHashMap at N=1000000 ---
  Insert (amortized): median=   42.51 mean=   43.53 +/-  4.56 CI95(mean)=[41.54,45.53] min=37.09 max=52.10
     Find(hit): median=   16.07 mean=   16.09 +/-  2.16 CI95(mean)=[15.15,17.04] min=12.55 max=19.99
    Find(miss): median=    5.38 mean=    5.37 +/-  0.10 CI95(mean)=[5.32,5.41] min=5.13 max=5.48
   Erase (25%): median=  143.56 mean=  143.58 +/- 12.46 CI95(mean)=[138.12,149.04] min=123.61 max=163.47
         Churn: median=   89.37 mean=   90.79 +/-  5.66 CI95(mean)=[88.31,93.27] min=83.84 max=100.72

--- Detailed Statistics for StableHashMap+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   43.54 mean=   43.24 +/-  2.94 CI95(mean)=[41.95,44.53] min=38.45 max=47.98
     Find(hit): median=   16.28 mean=   15.77 +/-  2.01 CI95(mean)=[14.89,16.65] min=12.61 max=19.75
    Find(miss): median=    5.40 mean=    5.38 +/-  0.13 CI95(mean)=[5.33,5.44] min=5.14 max=5.60
   Erase (25%): median=  145.74 mean=  144.53 +/- 12.04 CI95(mean)=[139.25,149.81] min=123.10 max=168.14
         Churn: median=   86.95 mean=   89.10 +/-  5.56 CI95(mean)=[86.66,91.54] min=83.50 max=104.17

--- Detailed Statistics for StableHashMap[Block]+SM64 at N=1000000 ---
  Insert (amortized): median=   26.16 mean=   26.70 +/-  2.33 CI95(mean)=[25.68,27.72] min=23.38 max=31.34
     Find(hit): median=   13.95 mean=   13.73 +/-  2.41 CI95(mean)=[12.68,14.79] min=9.36 max=18.68
    Find(miss): median=    5.24 mean=    5.93 +/-  2.91 CI95(mean)=[4.65,7.20] min=5.09 max=18.28
   Erase (25%): median=   37.78 mean=   38.06 +/-  3.37 CI95(mean)=[36.59,39.54] min=33.42 max=44.93
         Churn: median=   43.55 mean=   43.63 +/-  2.30 CI95(mean)=[42.62,44.64] min=39.56 max=47.37

[Cooling: before miss diagnostics][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]

================================================================================
  MISS DIAGNOSTICS (Slim)
================================================================================

Random misses only (no H2-biased sets), fixed N=1,000,000.
Purpose: regression tripwire for unsuccessful lookup behavior.

[2026-02-16 03:56:07] MissDiag CPU: 2445 MHz (~base: 2445)

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=1000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        6.51          0.01          1.00        1.00         15.26        0.00        0.01
StableHashMap[Block]+SM64 (counted)        6.34          0.01          1.00        1.00         15.26        0.00        0.01
boost::unordered_node_map+SM64 (counted)        7.56          0.03          1.00           -             -           -           -
[Cooling: miss reserve change][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=2000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        5.57          0.00          1.00        1.00          7.63        0.00        0.00
StableHashMap[Block]+SM64 (counted)        5.48          0.00          1.00        1.00          7.63        0.00        0.00
boost::unordered_node_map+SM64 (counted)        6.42          0.02          1.00           -             -           -           -

[Cooling: before pathological erase][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]

================================================================================
  PATHOLOGICAL ERASE (TOMBSTONE DEGRADATION TEST)
================================================================================

Tests sustained churn on a single table without reset.
Tombstone-based maps may degrade over time.
Backward-shift maps stay stable.
Methodology: 3 warmup + 20 measured runs
             Round-robin execution with randomized order

[2026-02-16 03:56:30] Starting CPU: 2594 MHz (~base: 2594)
N = 100000, Total operations = 5000000

               StableHashMap:    70.93 ns/step (+/-0.41, CI:[70.88,71.24])
    StableHashMap+SplitMix64:    70.72 ns/step (+/-0.26, CI:[70.67,70.90])
   StableHashMap[Block]+SM64:    50.65 ns/step (+/-0.24, CI:[50.45,50.67])
             FastHashMap[BS]:    43.71 ns/step (+/-1.37, CI:[43.80,45.00])
  FastHashMap[BS]+SplitMix64:    43.07 ns/step (+/-0.82, CI:[42.93,43.65])
             FastHashMap[TS]:    34.63 ns/step (+/-0.76, CI:[34.43,35.10])
  FastHashMap[TS]+SplitMix64:    34.62 ns/step (+/-0.71, CI:[34.47,35.09])
              tsl::robin_map:   229.80 ns/step (+/-3.51, CI:[227.27,230.35])
     ankerl::unordered_dense:    23.97 ns/step (+/-0.09, CI:[23.88,23.96])
         absl::flat_hash_map:    33.89 ns/step (+/-0.85, CI:[33.70,34.45])
         absl::node_hash_map:    56.25 ns/step (+/-0.30, CI:[56.21,56.47])
   boost::unordered_flat_map:    14.20 ns/step (+/-0.05, CI:[14.18,14.22])
   boost::unordered_node_map:    49.89 ns/step (+/-0.70, CI:[49.76,50.37])
           folly::F14FastMap:    19.33 ns/step (+/-0.08, CI:[19.30,19.38])
           folly::F14NodeMap:    50.41 ns/step (+/-0.29, CI:[50.40,50.65])
              llvm::DenseMap:    16.53 ns/step (+/-0.60, CI:[16.43,16.95])
          std::unordered_map:    92.92 ns/step (+/-0.87, CI:[92.73,93.49])

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  CORE OPERATIONS BENCHMARK (Round-Robin)
================================================================================

Comparing FastHashMap and StableHashMap (std::hash vs SplitMix64Hash)
vs std::unordered_map vs tsl::robin_map vs ankerl::unordered_dense vs absl::flat_hash_map vs boost::unordered_flat_map vs folly::F14FastMap vs llvm::DenseMap

Methodology:
  - 3 warmup + 20 measured runs per test
  - Round-robin execution with randomized order per run
  - All libraries observe same distribution of machine states
  - Primary metric: median (ns/op)
  - FastHashMap SIMD backend: AVX2
  - StableHashMap SIMD backend: AVX2
  - FastHashMap policies: BackwardShift (BS), Tombstone (TS)
  - StableHashMap: Reference-stable (pointers valid across insert/reserve)

Cases (ns/op):
  Insert: insert N unique keys into empty map (after reserve)
  Find(hit): find N present keys
  Find(miss): find N absent keys
  Erase: erase 25% of present keys (random order)
  Churn: key replacement churn (erase one existing key, insert new key; size constant)

[2026-02-16 04:11:31] CPU: 3255 MHz (~base: 3255)
N = 10000
[2026-02-16 04:11:31] Insert (amortized) CPU: 3244 MHz (~base: 3244)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:11:35] Find(hit) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:11:38] Find(miss) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:11:41] Erase (25%) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:11:45] Churn CPU: 2445 MHz (~base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]      9.96      4.65      5.21     31.64     25.14
    FastHashMap[BS]+SplitMix64      9.83      4.35      4.93     31.62     24.15
               FastHashMap[TS]     11.49      4.42      5.05      5.53     18.84
    FastHashMap[TS]+SplitMix64     11.17      3.97      4.77      5.25     18.02
                 StableHashMap     19.76      4.30      2.96     24.29     34.34
      StableHashMap+SplitMix64     19.78      4.20      2.95     24.98     34.51
     StableHashMap[Block]+SM64     10.86      4.14      2.81     13.45     27.57
            std::unordered_map     32.55     11.33     17.21     45.14     35.96
                tsl::robin_map      9.93      4.11      5.52     10.15    887.20
       ankerl::unordered_dense     21.94     10.56      4.22     26.25     22.52
           absl::flat_hash_map     10.31      3.04      3.96     11.10     14.07
           absl::node_hash_map     19.90      2.95      3.76     23.91     23.65
     boost::unordered_flat_map      6.29      3.20      2.63      4.77      6.76
     boost::unordered_node_map     18.02      3.39      2.69     19.35     17.89
             folly::F14FastMap      9.50      4.29      4.81      8.79     16.14
             folly::F14NodeMap     23.80      4.25      4.87     21.42     25.19
                llvm::DenseMap      9.39      7.01     12.70      7.70     20.50

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: boost::unordered_flat_map (5.17x), llvm::DenseMap (3.47x), folly::F14FastMap (3.43x)
    Top 3 Find: absl::flat_hash_map (3.73x), boost::unordered_flat_map (3.54x), FastHashMap[TS]+SplitMix64 (2.86x)
    Top 3 Miss: boost::unordered_flat_map (6.54x), absl::flat_hash_map (4.35x), ankerl::unordered_dense (4.07x)
    Top 3 Erase: boost::unordered_flat_map (9.46x), FastHashMap[TS]+SplitMix64 (8.60x), FastHashMap[TS] (8.16x)
    Top 3 Churn: boost::unordered_flat_map (5.32x), absl::flat_hash_map (2.55x), folly::F14FastMap (2.23x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (3.00x), boost::unordered_node_map (1.81x), StableHashMap (1.65x)
    Top 3 Find: absl::node_hash_map (3.84x), boost::unordered_node_map (3.34x), StableHashMap[Block]+SM64 (2.74x)
    Top 3 Miss: boost::unordered_node_map (6.40x), StableHashMap[Block]+SM64 (6.12x), StableHashMap+SplitMix64 (5.83x)
    Top 3 Erase: StableHashMap[Block]+SM64 (3.36x), boost::unordered_node_map (2.33x), folly::F14NodeMap (2.11x)
    Top 3 Churn: boost::unordered_node_map (2.01x), absl::node_hash_map (1.52x), folly::F14NodeMap (1.43x)

  All Results:
    FastHashMap[BS]                 3.27x insert,  2.44x find,  3.31x miss,  1.43x erase,  1.43x churn
    FastHashMap[BS]+SplitMix64      3.31x insert,  2.60x find,  3.49x miss,  1.43x erase,  1.49x churn
    FastHashMap[TS]                 2.83x insert,  2.56x find,  3.41x miss,  8.16x erase,  1.91x churn
    FastHashMap[TS]+SplitMix64      2.91x insert,  2.86x find,  3.61x miss,  8.60x erase,  2.00x churn
    StableHashMap                   1.65x insert,  2.63x find,  5.81x miss,  1.86x erase,  1.05x churn
    StableHashMap+SplitMix64        1.65x insert,  2.70x find,  5.83x miss,  1.81x erase,  1.04x churn
    StableHashMap[Block]+SM64       3.00x insert,  2.74x find,  6.12x miss,  3.36x erase,  1.30x churn
    tsl::robin_map                  3.28x insert,  2.76x find,  3.12x miss,  4.45x erase,  0.04x churn
    ankerl::unordered_dense         1.48x insert,  1.07x find,  4.07x miss,  1.72x erase,  1.60x churn
    absl::flat_hash_map             3.16x insert,  3.73x find,  4.35x miss,  4.07x erase,  2.55x churn
    absl::node_hash_map             1.64x insert,  3.84x find,  4.57x miss,  1.89x erase,  1.52x churn
    boost::unordered_flat_map       5.17x insert,  3.54x find,  6.54x miss,  9.46x erase,  5.32x churn
    boost::unordered_node_map       1.81x insert,  3.34x find,  6.40x miss,  2.33x erase,  2.01x churn
    folly::F14FastMap               3.43x insert,  2.64x find,  3.58x miss,  5.14x erase,  2.23x churn
    folly::F14NodeMap               1.37x insert,  2.67x find,  3.54x miss,  2.11x erase,  1.43x churn
    llvm::DenseMap                  3.47x insert,  1.61x find,  1.35x miss,  5.86x erase,  1.75x churn

[Cooling: before next size][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]
[2026-02-16 04:11:49] CPU: 3238 MHz (~base: 3238)
N = 100000
[2026-02-16 04:11:49] Insert (amortized) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:11:53] Find(hit) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:11:58] Find(miss) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:12:03] Erase (25%) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:12:08] Churn CPU: 2445 MHz (~base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     18.75      7.09      7.64     55.63     42.49
    FastHashMap[BS]+SplitMix64     19.26      6.55      7.17     55.35     42.64
               FastHashMap[TS]     18.99      7.18      7.37      8.10     24.61
    FastHashMap[TS]+SplitMix64     16.99      6.52      6.95      7.62     24.22
                 StableHashMap     23.51      6.93      5.34     45.87     42.34
      StableHashMap+SplitMix64     23.56      6.91      5.29     46.08     42.45
     StableHashMap[Block]+SM64     13.95      6.75      5.16     24.61     29.33
            std::unordered_map     45.34     13.88     20.66     78.07     52.28
                tsl::robin_map     17.26      5.54      7.07     13.86  17316.62
       ankerl::unordered_dense     34.59     16.87      9.03     44.82     39.40
           absl::flat_hash_map     13.66      4.61      7.87     17.49     28.31
           absl::node_hash_map     23.63      4.61      7.93     41.47     50.99
     boost::unordered_flat_map      9.37      5.56      4.44      7.75     18.22
     boost::unordered_node_map     22.13      5.83      4.70     32.76     36.57
             folly::F14FastMap     16.47      5.82      4.26      9.71     14.61
             folly::F14NodeMap     32.90      5.42      4.21     30.62     32.32
                llvm::DenseMap      9.75      5.55      9.60      5.64     13.79

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: boost::unordered_flat_map (4.84x), llvm::DenseMap (4.65x), absl::flat_hash_map (3.32x)
    Top 3 Find: absl::flat_hash_map (3.01x), tsl::robin_map (2.50x), llvm::DenseMap (2.50x)
    Top 3 Miss: folly::F14FastMap (4.85x), boost::unordered_flat_map (4.65x), FastHashMap[TS]+SplitMix64 (2.97x)
    Top 3 Erase: llvm::DenseMap (13.85x), FastHashMap[TS]+SplitMix64 (10.24x), boost::unordered_flat_map (10.08x)
    Top 3 Churn: llvm::DenseMap (3.79x), folly::F14FastMap (3.58x), boost::unordered_flat_map (2.87x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (3.25x), boost::unordered_node_map (2.05x), StableHashMap (1.93x)
    Top 3 Find: absl::node_hash_map (3.01x), folly::F14NodeMap (2.56x), boost::unordered_node_map (2.38x)
    Top 3 Miss: folly::F14NodeMap (4.91x), boost::unordered_node_map (4.40x), StableHashMap[Block]+SM64 (4.01x)
    Top 3 Erase: StableHashMap[Block]+SM64 (3.17x), folly::F14NodeMap (2.55x), boost::unordered_node_map (2.38x)
    Top 3 Churn: StableHashMap[Block]+SM64 (1.78x), folly::F14NodeMap (1.62x), boost::unordered_node_map (1.43x)

  All Results:
    FastHashMap[BS]                 2.42x insert,  1.96x find,  2.70x miss,  1.40x erase,  1.23x churn
    FastHashMap[BS]+SplitMix64      2.35x insert,  2.12x find,  2.88x miss,  1.41x erase,  1.23x churn
    FastHashMap[TS]                 2.39x insert,  1.93x find,  2.80x miss,  9.64x erase,  2.12x churn
    FastHashMap[TS]+SplitMix64      2.67x insert,  2.13x find,  2.97x miss, 10.24x erase,  2.16x churn
    StableHashMap                   1.93x insert,  2.00x find,  3.87x miss,  1.70x erase,  1.23x churn
    StableHashMap+SplitMix64        1.92x insert,  2.01x find,  3.91x miss,  1.69x erase,  1.23x churn
    StableHashMap[Block]+SM64       3.25x insert,  2.06x find,  4.01x miss,  3.17x erase,  1.78x churn
    tsl::robin_map                  2.63x insert,  2.50x find,  2.92x miss,  5.63x erase,  0.00x churn
    ankerl::unordered_dense         1.31x insert,  0.82x find,  2.29x miss,  1.74x erase,  1.33x churn
    absl::flat_hash_map             3.32x insert,  3.01x find,  2.62x miss,  4.46x erase,  1.85x churn
    absl::node_hash_map             1.92x insert,  3.01x find,  2.60x miss,  1.88x erase,  1.03x churn
    boost::unordered_flat_map       4.84x insert,  2.50x find,  4.65x miss, 10.08x erase,  2.87x churn
    boost::unordered_node_map       2.05x insert,  2.38x find,  4.40x miss,  2.38x erase,  1.43x churn
    folly::F14FastMap               2.75x insert,  2.38x find,  4.85x miss,  8.04x erase,  3.58x churn
    folly::F14NodeMap               1.38x insert,  2.56x find,  4.91x miss,  2.55x erase,  1.62x churn
    llvm::DenseMap                  4.65x insert,  2.50x find,  2.15x miss, 13.85x erase,  3.79x churn

[Cooling: before next size][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]
[2026-02-16 04:13:35] CPU: 2977 MHz (~base: 2977)
N = 1000000
[2026-02-16 04:13:35] Insert (amortized) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:14:10] Find(hit) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:14:54] Find(miss) CPU: 2598 MHz (~base: 2598)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:15:35] Erase (25%) CPU: 2445 MHz (~base: 2445)
[CPU frequency detection unavailable - using fixed cooling delay]
[2026-02-16 04:16:20] Churn CPU: 2445 MHz (~base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     26.34     24.44     11.74     53.29     48.21
    FastHashMap[BS]+SplitMix64     26.43     22.54     11.07     52.31     47.10
               FastHashMap[TS]     24.63     23.73     10.87     27.56     34.58
    FastHashMap[TS]+SplitMix64     24.48     23.17     10.01     24.64     35.31
                 StableHashMap     45.63     19.48      5.47    183.22    124.31
      StableHashMap+SplitMix64     44.95     18.55      5.57    183.58    118.96
     StableHashMap[Block]+SM64     25.26     15.95      5.28     72.37     63.30
            std::unordered_map     98.29     25.85     34.89    278.97    198.95
                tsl::robin_map     36.34     13.58     14.68     32.91  19949.44
       ankerl::unordered_dense     49.04     13.62      9.71     58.36     56.54
           absl::flat_hash_map     28.11     15.94      6.81     36.31     35.74
           absl::node_hash_map     43.66     13.81      9.26    160.23    115.03
     boost::unordered_flat_map     14.66     13.36      4.05     19.29     23.75
     boost::unordered_node_map     47.27     12.71      6.24    126.59    108.14
             folly::F14FastMap     50.64     19.94     13.22     41.38     57.04
             folly::F14NodeMap     68.38     11.87     10.92    136.67    117.05
                llvm::DenseMap     20.86     13.79     19.26     11.41     34.11

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: boost::unordered_flat_map (6.70x), llvm::DenseMap (4.71x), FastHashMap[TS]+SplitMix64 (4.01x)
    Top 3 Find: boost::unordered_flat_map (1.93x), tsl::robin_map (1.90x), ankerl::unordered_dense (1.90x)
    Top 3 Miss: boost::unordered_flat_map (8.62x), absl::flat_hash_map (5.12x), ankerl::unordered_dense (3.59x)
    Top 3 Erase: llvm::DenseMap (24.45x), boost::unordered_flat_map (14.46x), FastHashMap[TS]+SplitMix64 (11.32x)
    Top 3 Churn: boost::unordered_flat_map (8.38x), llvm::DenseMap (5.83x), FastHashMap[TS] (5.75x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (3.89x), absl::node_hash_map (2.25x), StableHashMap+SplitMix64 (2.19x)
    Top 3 Find: folly::F14NodeMap (2.18x), boost::unordered_node_map (2.03x), absl::node_hash_map (1.87x)
    Top 3 Miss: StableHashMap[Block]+SM64 (6.61x), StableHashMap (6.37x), StableHashMap+SplitMix64 (6.27x)
    Top 3 Erase: StableHashMap[Block]+SM64 (3.86x), boost::unordered_node_map (2.20x), folly::F14NodeMap (2.04x)
    Top 3 Churn: StableHashMap[Block]+SM64 (3.14x), boost::unordered_node_map (1.84x), absl::node_hash_map (1.73x)

  All Results:
    FastHashMap[BS]                 3.73x insert,  1.06x find,  2.97x miss,  5.23x erase,  4.13x churn
    FastHashMap[BS]+SplitMix64      3.72x insert,  1.15x find,  3.15x miss,  5.33x erase,  4.22x churn
    FastHashMap[TS]                 3.99x insert,  1.09x find,  3.21x miss, 10.12x erase,  5.75x churn
    FastHashMap[TS]+SplitMix64      4.01x insert,  1.12x find,  3.49x miss, 11.32x erase,  5.63x churn
    StableHashMap                   2.15x insert,  1.33x find,  6.37x miss,  1.52x erase,  1.60x churn
    StableHashMap+SplitMix64        2.19x insert,  1.39x find,  6.27x miss,  1.52x erase,  1.67x churn
    StableHashMap[Block]+SM64       3.89x insert,  1.62x find,  6.61x miss,  3.86x erase,  3.14x churn
    tsl::robin_map                  2.70x insert,  1.90x find,  2.38x miss,  8.48x erase,  0.01x churn
    ankerl::unordered_dense         2.00x insert,  1.90x find,  3.59x miss,  4.78x erase,  3.52x churn
    absl::flat_hash_map             3.50x insert,  1.62x find,  5.12x miss,  7.68x erase,  5.57x churn
    absl::node_hash_map             2.25x insert,  1.87x find,  3.77x miss,  1.74x erase,  1.73x churn
    boost::unordered_flat_map       6.70x insert,  1.93x find,  8.62x miss, 14.46x erase,  8.38x churn
    boost::unordered_node_map       2.08x insert,  2.03x find,  5.59x miss,  2.20x erase,  1.84x churn
    folly::F14FastMap               1.94x insert,  1.30x find,  2.64x miss,  6.74x erase,  3.49x churn
    folly::F14NodeMap               1.44x insert,  2.18x find,  3.20x miss,  2.04x erase,  1.70x churn
    llvm::DenseMap                  4.71x insert,  1.88x find,  1.81x miss, 24.45x erase,  5.83x churn
  [NOTE] std::unordered_map: median 278.97 ns (node-based pointer chasing / cache locality effects)
  [NOTE] std::unordered_map: median 198.95 ns (node-based pointer chasing / cache locality effects)

--- Detailed Statistics for FastHashMap[BS] at N=1000000 ---
  Insert (amortized): median=   26.34 mean=   26.52 +/-  0.79 CI95(mean)=[26.18,26.87] min=25.50 max=28.39
     Find(hit): median=   24.44 mean=   24.35 +/-  0.76 CI95(mean)=[24.02,24.69] min=22.89 max=25.86
    Find(miss): median=   11.74 mean=   11.82 +/-  0.92 CI95(mean)=[11.42,12.23] min=10.28 max=14.02
   Erase (25%): median=   53.29 mean=   53.55 +/-  1.86 CI95(mean)=[52.73,54.36] min=51.23 max=58.26
         Churn: median=   48.21 mean=   47.72 +/-  2.28 CI95(mean)=[46.72,48.72] min=43.06 max=51.62

--- Detailed Statistics for FastHashMap[BS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   26.43 mean=   26.50 +/-  0.98 CI95(mean)=[26.07,26.94] min=25.26 max=29.09
     Find(hit): median=   22.54 mean=   22.72 +/-  0.81 CI95(mean)=[22.36,23.08] min=21.32 max=24.28
    Find(miss): median=   11.07 mean=   11.20 +/-  0.56 CI95(mean)=[10.96,11.45] min=10.27 max=12.24
   Erase (25%): median=   52.31 mean=   52.24 +/-  2.20 CI95(mean)=[51.28,53.21] min=49.49 max=57.78
         Churn: median=   47.10 mean=   46.88 +/-  2.35 CI95(mean)=[45.85,47.91] min=42.33 max=51.30

--- Detailed Statistics for FastHashMap[TS] at N=1000000 ---
  Insert (amortized): median=   24.63 mean=   24.53 +/-  1.08 CI95(mean)=[24.06,25.01] min=22.97 max=26.28
     Find(hit): median=   23.73 mean=   23.66 +/-  1.35 CI95(mean)=[23.07,24.25] min=21.45 max=25.71
    Find(miss): median=   10.87 mean=   10.87 +/-  1.15 CI95(mean)=[10.37,11.38] min=8.61 max=13.17
   Erase (25%): median=   27.56 mean=   27.24 +/-  1.61 CI95(mean)=[26.54,27.95] min=23.54 max=29.59
         Churn: median=   34.58 mean=   34.37 +/-  2.38 CI95(mean)=[33.33,35.41] min=30.77 max=38.55

--- Detailed Statistics for FastHashMap[TS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   24.48 mean=   24.60 +/-  1.38 CI95(mean)=[23.99,25.20] min=22.98 max=28.92
     Find(hit): median=   23.17 mean=   23.25 +/-  0.91 CI95(mean)=[22.85,23.64] min=21.61 max=25.40
    Find(miss): median=   10.01 mean=   10.21 +/-  0.81 CI95(mean)=[9.85,10.56] min=9.19 max=12.05
   Erase (25%): median=   24.64 mean=   24.56 +/-  1.43 CI95(mean)=[23.94,25.19] min=22.53 max=28.44
         Churn: median=   35.31 mean=   34.54 +/-  2.56 CI95(mean)=[33.42,35.67] min=26.96 max=37.76

--- Detailed Statistics for StableHashMap at N=1000000 ---
  Insert (amortized): median=   45.63 mean=   45.20 +/-  3.26 CI95(mean)=[43.77,46.63] min=38.55 max=50.41
     Find(hit): median=   19.48 mean=   19.23 +/-  2.08 CI95(mean)=[18.32,20.14] min=14.82 max=23.63
    Find(miss): median=    5.47 mean=    5.53 +/-  0.22 CI95(mean)=[5.44,5.63] min=5.23 max=6.10
   Erase (25%): median=  183.22 mean=  185.95 +/- 12.40 CI95(mean)=[180.52,191.38] min=163.85 max=211.94
         Churn: median=  124.31 mean=  124.22 +/- 14.43 CI95(mean)=[117.89,130.54] min=98.54 max=150.85

--- Detailed Statistics for StableHashMap+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   44.95 mean=   44.92 +/-  3.64 CI95(mean)=[43.32,46.51] min=36.18 max=51.83
     Find(hit): median=   18.55 mean=   18.46 +/-  1.87 CI95(mean)=[17.64,19.28] min=14.39 max=22.27
    Find(miss): median=    5.57 mean=    5.65 +/-  0.31 CI95(mean)=[5.51,5.78] min=5.26 max=6.39
   Erase (25%): median=  183.58 mean=  185.71 +/-  9.67 CI95(mean)=[181.47,189.95] min=175.14 max=210.27
         Churn: median=  118.96 mean=  122.68 +/- 13.59 CI95(mean)=[116.72,128.63] min=99.36 max=146.14

--- Detailed Statistics for StableHashMap[Block]+SM64 at N=1000000 ---
  Insert (amortized): median=   25.26 mean=   25.34 +/-  1.67 CI95(mean)=[24.61,26.08] min=22.47 max=27.98
     Find(hit): median=   15.95 mean=   16.34 +/-  3.16 CI95(mean)=[14.95,17.72] min=10.24 max=21.09
    Find(miss): median=    5.28 mean=    5.36 +/-  0.32 CI95(mean)=[5.22,5.50] min=4.99 max=6.24
   Erase (25%): median=   72.37 mean=   70.29 +/- 11.29 CI95(mean)=[65.34,75.24] min=52.40 max=87.81
         Churn: median=   63.30 mean=   64.20 +/-  8.38 CI95(mean)=[60.53,67.88] min=50.73 max=78.77

[Cooling: before miss diagnostics][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]

================================================================================
  MISS DIAGNOSTICS (Slim)
================================================================================

Random misses only (no H2-biased sets), fixed N=1,000,000.
Purpose: regression tripwire for unsuccessful lookup behavior.

[2026-02-16 04:33:15] MissDiag CPU: 2445 MHz (~base: 2445)

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=1000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        8.43          0.01          1.00        1.00         15.26        0.00        0.01
StableHashMap[Block]+SM64 (counted)        8.30          0.01          1.00        1.00         15.26        0.00        0.01
boost::unordered_node_map+SM64 (counted)        8.23          0.03          1.00           -             -           -           -
[Cooling: miss reserve change][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=2000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        7.14          0.00          1.00        1.00          7.63        0.00        0.00
StableHashMap[Block]+SM64 (counted)        7.02          0.00          1.00        1.00          7.63        0.00        0.00
boost::unordered_node_map+SM64 (counted)        7.09          0.02          1.00           -             -           -           -

[Cooling: before pathological erase][CPU frequency detection unavailable - using fixed cooling delay]
 [Ready]

================================================================================
  PATHOLOGICAL ERASE (TOMBSTONE DEGRADATION TEST)
================================================================================

Tests sustained churn on a single table without reset.
Tombstone-based maps may degrade over time.
Backward-shift maps stay stable.
Methodology: 3 warmup + 20 measured runs
             Round-robin execution with randomized order

[2026-02-16 04:33:42] Starting CPU: 2876 MHz (~base: 2876)
N = 100000, Total operations = 5000000

               StableHashMap:    73.52 ns/step (+/-0.72, CI:[73.37,74.00])
    StableHashMap+SplitMix64:    73.70 ns/step (+/-1.09, CI:[73.52,74.48])
   StableHashMap[Block]+SM64:    52.81 ns/step (+/-0.28, CI:[52.73,52.98])
             FastHashMap[BS]:    42.64 ns/step (+/-0.23, CI:[42.58,42.78])
  FastHashMap[BS]+SplitMix64:    41.98 ns/step (+/-0.24, CI:[41.84,42.05])
             FastHashMap[TS]:    33.81 ns/step (+/-0.43, CI:[33.76,34.14])
  FastHashMap[TS]+SplitMix64:    32.50 ns/step (+/-0.32, CI:[32.39,32.67])
              tsl::robin_map:   260.80 ns/step (+/-4.28, CI:[257.84,261.59])
     ankerl::unordered_dense:    35.75 ns/step (+/-0.16, CI:[35.71,35.85])
         absl::flat_hash_map:    28.26 ns/step (+/-0.60, CI:[27.80,28.33])
         absl::node_hash_map:    56.08 ns/step (+/-0.77, CI:[55.75,56.43])
   boost::unordered_flat_map:    15.58 ns/step (+/-0.27, CI:[15.54,15.78])
   boost::unordered_node_map:    50.08 ns/step (+/-0.41, CI:[49.90,50.26])
           folly::F14FastMap:    21.95 ns/step (+/-0.12, CI:[21.93,22.04])
           folly::F14NodeMap:    56.84 ns/step (+/-1.42, CI:[56.54,57.79])
              llvm::DenseMap:    14.49 ns/step (+/-0.30, CI:[14.47,14.74])
          std::unordered_map:    92.30 ns/step (+/-0.20, CI:[92.27,92.44])

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  CORE OPERATIONS BENCHMARK (Round-Robin)
================================================================================

Comparing FastHashMap and StableHashMap (std::hash vs SplitMix64Hash)
vs std::unordered_map vs tsl::robin_map vs ankerl::unordered_dense vs absl::flat_hash_map vs boost::unordered_flat_map vs folly::F14FastMap

Methodology:
  - 3 warmup + 20 measured runs per test
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

[2026-02-16 04:54:30] CPU: 2445 MHz (base: 2445)
N = 10000
[2026-02-16 04:54:30] Insert (amortized) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:31] Find(hit) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:32] Find(miss) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:33] Erase (25%) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:34] Churn CPU: 2445 MHz (base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     19.54     10.95      9.14     45.80     32.77
    FastHashMap[BS]+SplitMix64     12.38      6.73      5.86     33.14     26.34
               FastHashMap[TS]     20.81     13.34     11.33     13.50     33.03
    FastHashMap[TS]+SplitMix64     13.22      6.49      6.12      6.60     24.59
                 StableHashMap     58.03     17.66     13.23     46.26     71.41
      StableHashMap+SplitMix64     47.07      9.48      5.84     34.66     64.16
     StableHashMap[Block]+SM64     14.77      8.70      5.79     11.38     28.98
            std::unordered_map     69.40     14.21     15.97     51.62     57.84
                tsl::robin_map     16.16     10.85     12.47     16.20     20.28
       ankerl::unordered_dense     32.77     12.59      6.74     30.98     25.37
           absl::flat_hash_map     49.35      6.46      6.80     26.16     24.90
           absl::node_hash_map     60.75      7.61      6.76     39.14     55.09
     boost::unordered_flat_map     19.43      4.31      2.82      5.68      7.59
     boost::unordered_node_map     46.20      5.04      2.98     28.94     39.94
             folly::F14FastMap     37.22     11.27     10.11     26.52     33.59
             folly::F14NodeMap     64.44     12.29     10.12     64.92     65.14

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[BS]+SplitMix64 (5.60x), FastHashMap[TS]+SplitMix64 (5.25x), tsl::robin_map (4.29x)
    Top 3 Find: boost::unordered_flat_map (3.29x), absl::flat_hash_map (2.20x), FastHashMap[TS]+SplitMix64 (2.19x)
    Top 3 Miss: boost::unordered_flat_map (5.66x), FastHashMap[BS]+SplitMix64 (2.73x), FastHashMap[TS]+SplitMix64 (2.61x)
    Top 3 Erase: boost::unordered_flat_map (9.09x), FastHashMap[TS]+SplitMix64 (7.82x), FastHashMap[TS] (3.82x)
    Top 3 Churn: boost::unordered_flat_map (7.62x), tsl::robin_map (2.85x), FastHashMap[TS]+SplitMix64 (2.35x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (4.70x), boost::unordered_node_map (1.50x), StableHashMap+SplitMix64 (1.47x)
    Top 3 Find: boost::unordered_node_map (2.82x), absl::node_hash_map (1.87x), StableHashMap[Block]+SM64 (1.63x)
    Top 3 Miss: boost::unordered_node_map (5.37x), StableHashMap[Block]+SM64 (2.76x), StableHashMap+SplitMix64 (2.73x)
    Top 3 Erase: StableHashMap[Block]+SM64 (4.54x), boost::unordered_node_map (1.78x), StableHashMap+SplitMix64 (1.49x)
    Top 3 Churn: StableHashMap[Block]+SM64 (2.00x), boost::unordered_node_map (1.45x), absl::node_hash_map (1.05x)

  All Results:
    FastHashMap[BS]                 3.55x insert,  1.30x find,  1.75x miss,  1.13x erase,  1.77x churn
    FastHashMap[BS]+SplitMix64      5.60x insert,  2.11x find,  2.73x miss,  1.56x erase,  2.20x churn
    FastHashMap[TS]                 3.33x insert,  1.07x find,  1.41x miss,  3.82x erase,  1.75x churn
    FastHashMap[TS]+SplitMix64      5.25x insert,  2.19x find,  2.61x miss,  7.82x erase,  2.35x churn
    StableHashMap                   1.20x insert,  0.80x find,  1.21x miss,  1.12x erase,  0.81x churn
    StableHashMap+SplitMix64        1.47x insert,  1.50x find,  2.73x miss,  1.49x erase,  0.90x churn
    StableHashMap[Block]+SM64       4.70x insert,  1.63x find,  2.76x miss,  4.54x erase,  2.00x churn
    tsl::robin_map                  4.29x insert,  1.31x find,  1.28x miss,  3.19x erase,  2.85x churn
    ankerl::unordered_dense         2.12x insert,  1.13x find,  2.37x miss,  1.67x erase,  2.28x churn
    absl::flat_hash_map             1.41x insert,  2.20x find,  2.35x miss,  1.97x erase,  2.32x churn
    absl::node_hash_map             1.14x insert,  1.87x find,  2.36x miss,  1.32x erase,  1.05x churn
    boost::unordered_flat_map       3.57x insert,  3.29x find,  5.66x miss,  9.09x erase,  7.62x churn
    boost::unordered_node_map       1.50x insert,  2.82x find,  5.37x miss,  1.78x erase,  1.45x churn
    folly::F14FastMap               1.86x insert,  1.26x find,  1.58x miss,  1.95x erase,  1.72x churn
    folly::F14NodeMap               1.08x insert,  1.16x find,  1.58x miss,  0.80x erase,  0.89x churn

[Cooling: before next size][CPU stable at 2445 MHz (100% of base, variance: 0.0%)]
 [Ready: 2445 MHz]
[2026-02-16 04:54:36] CPU: 2445 MHz (base: 2445)
N = 100000
[2026-02-16 04:54:36] Insert (amortized) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:39] Find(hit) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:42] Find(miss) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:45] Erase (25%) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:54:48] Churn CPU: 2445 MHz (base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     25.68     16.58     14.88     73.91     56.59
    FastHashMap[BS]+SplitMix64     17.56     11.25     10.36     59.29     48.83
               FastHashMap[TS]     25.78     19.50     16.95     19.10     35.06
    FastHashMap[TS]+SplitMix64     17.32     11.17     10.11     10.98     26.62
                 StableHashMap     61.17     26.36     19.23     66.39    101.61
      StableHashMap+SplitMix64     53.97     16.52     10.14     54.85     89.67
     StableHashMap[Block]+SM64     21.17     15.35      9.88     22.95     36.06
            std::unordered_map     71.09     19.57     20.46     67.14     91.76
                tsl::robin_map     21.56     14.18     16.64     22.32     24.70
       ankerl::unordered_dense     45.52     19.59     12.42     56.46     52.41
           absl::flat_hash_map     51.53      9.93     11.18     20.28     34.94
           absl::node_hash_map     72.32     11.52     11.76     60.20     97.56
     boost::unordered_flat_map     20.67      7.38      4.45      9.14     22.09
     boost::unordered_node_map     55.99      8.29      5.00     44.76     84.12
             folly::F14FastMap     35.11     15.41     11.36     29.60     34.36
             folly::F14NodeMap     73.79     16.11     11.30     87.52     95.34

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: FastHashMap[TS]+SplitMix64 (4.11x), FastHashMap[BS]+SplitMix64 (4.05x), boost::unordered_flat_map (3.44x)
    Top 3 Find: boost::unordered_flat_map (2.65x), absl::flat_hash_map (1.97x), FastHashMap[TS]+SplitMix64 (1.75x)
    Top 3 Miss: boost::unordered_flat_map (4.60x), FastHashMap[TS]+SplitMix64 (2.02x), FastHashMap[BS]+SplitMix64 (1.98x)
    Top 3 Erase: boost::unordered_flat_map (7.34x), FastHashMap[TS]+SplitMix64 (6.12x), FastHashMap[TS] (3.52x)
    Top 3 Churn: boost::unordered_flat_map (4.15x), tsl::robin_map (3.72x), FastHashMap[TS]+SplitMix64 (3.45x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (3.36x), StableHashMap+SplitMix64 (1.32x), boost::unordered_node_map (1.27x)
    Top 3 Find: boost::unordered_node_map (2.36x), absl::node_hash_map (1.70x), StableHashMap[Block]+SM64 (1.28x)
    Top 3 Miss: boost::unordered_node_map (4.09x), StableHashMap[Block]+SM64 (2.07x), StableHashMap+SplitMix64 (2.02x)
    Top 3 Erase: StableHashMap[Block]+SM64 (2.93x), boost::unordered_node_map (1.50x), StableHashMap+SplitMix64 (1.22x)
    Top 3 Churn: StableHashMap[Block]+SM64 (2.54x), boost::unordered_node_map (1.09x), StableHashMap+SplitMix64 (1.02x)

  All Results:
    FastHashMap[BS]                 2.77x insert,  1.18x find,  1.38x miss,  0.91x erase,  1.62x churn
    FastHashMap[BS]+SplitMix64      4.05x insert,  1.74x find,  1.98x miss,  1.13x erase,  1.88x churn
    FastHashMap[TS]                 2.76x insert,  1.00x find,  1.21x miss,  3.52x erase,  2.62x churn
    FastHashMap[TS]+SplitMix64      4.11x insert,  1.75x find,  2.02x miss,  6.12x erase,  3.45x churn
    StableHashMap                   1.16x insert,  0.74x find,  1.06x miss,  1.01x erase,  0.90x churn
    StableHashMap+SplitMix64        1.32x insert,  1.18x find,  2.02x miss,  1.22x erase,  1.02x churn
    StableHashMap[Block]+SM64       3.36x insert,  1.28x find,  2.07x miss,  2.93x erase,  2.54x churn
    tsl::robin_map                  3.30x insert,  1.38x find,  1.23x miss,  3.01x erase,  3.72x churn
    ankerl::unordered_dense         1.56x insert,  1.00x find,  1.65x miss,  1.19x erase,  1.75x churn
    absl::flat_hash_map             1.38x insert,  1.97x find,  1.83x miss,  3.31x erase,  2.63x churn
    absl::node_hash_map             0.98x insert,  1.70x find,  1.74x miss,  1.12x erase,  0.94x churn
    boost::unordered_flat_map       3.44x insert,  2.65x find,  4.60x miss,  7.34x erase,  4.15x churn
    boost::unordered_node_map       1.27x insert,  2.36x find,  4.09x miss,  1.50x erase,  1.09x churn
    folly::F14FastMap               2.02x insert,  1.27x find,  1.80x miss,  2.27x erase,  2.67x churn
    folly::F14NodeMap               0.96x insert,  1.22x find,  1.81x miss,  0.77x erase,  0.96x churn

[Cooling: before next size][CPU stable at 2445 MHz (100% of base, variance: 0.0%)]
 [Ready: 2445 MHz]
[2026-02-16 04:54:56] CPU: 2445 MHz (base: 2445)
N = 1000000
[2026-02-16 04:54:56] Insert (amortized) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:55:33] Find(hit) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:56:26] Find(miss) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:57:12] Erase (25%) CPU: 2445 MHz (base: 2445)
[2026-02-16 04:57:57] Churn CPU: 2445 MHz (base: 2445)
-------------------------------------------------------------------------------
                           Map    Insert      Find      Miss     Erase     Churn
-------------------------------------------------------------------------------
               FastHashMap[BS]     45.57     52.18     16.40     86.13     71.45
    FastHashMap[BS]+SplitMix64     33.79     39.71     11.73     63.37     52.43
               FastHashMap[TS]     43.09     48.38     18.15     39.48     43.79
    FastHashMap[TS]+SplitMix64     31.15     36.63     10.45     28.19     34.56
                 StableHashMap     77.00     54.26     18.79    205.66    182.49
      StableHashMap+SplitMix64     66.33     40.43      9.69    174.55    168.14
     StableHashMap[Block]+SM64     32.46     22.36      9.31     63.65     70.49
            std::unordered_map    145.05     60.28     69.41    259.92    294.29
                tsl::robin_map     50.08     39.63     40.72     54.11     59.15
       ankerl::unordered_dense     47.80     15.73      8.82     63.08     43.77
           absl::flat_hash_map     75.39     35.73     12.19     39.83     44.03
           absl::node_hash_map     87.90     37.24     16.00    212.71    176.41
     boost::unordered_flat_map     30.85     18.34      5.18     18.46     21.38
     boost::unordered_node_map     73.33     23.57      8.46    139.71    162.04
             folly::F14FastMap     58.30     37.24     28.42     54.26     61.65
             folly::F14NodeMap     85.15     33.75     19.13    216.57    191.60

Speedup vs std::unordered_map:
  Flat/Fast Maps:
    Top 3 Insert: boost::unordered_flat_map (4.70x), FastHashMap[TS]+SplitMix64 (4.66x), FastHashMap[BS]+SplitMix64 (4.29x)
    Top 3 Find: ankerl::unordered_dense (3.83x), boost::unordered_flat_map (3.29x), absl::flat_hash_map (1.69x)
    Top 3 Miss: boost::unordered_flat_map (13.40x), ankerl::unordered_dense (7.87x), FastHashMap[TS]+SplitMix64 (6.64x)
    Top 3 Erase: boost::unordered_flat_map (14.08x), FastHashMap[TS]+SplitMix64 (9.22x), FastHashMap[TS] (6.58x)
    Top 3 Churn: boost::unordered_flat_map (13.77x), FastHashMap[TS]+SplitMix64 (8.52x), ankerl::unordered_dense (6.72x)
  Node-Based Maps (reference-stable):
    Top 3 Insert: StableHashMap[Block]+SM64 (4.47x), StableHashMap+SplitMix64 (2.19x), boost::unordered_node_map (1.98x)
    Top 3 Find: StableHashMap[Block]+SM64 (2.70x), boost::unordered_node_map (2.56x), folly::F14NodeMap (1.79x)
    Top 3 Miss: boost::unordered_node_map (8.20x), StableHashMap[Block]+SM64 (7.46x), StableHashMap+SplitMix64 (7.16x)
    Top 3 Erase: StableHashMap[Block]+SM64 (4.08x), boost::unordered_node_map (1.86x), StableHashMap+SplitMix64 (1.49x)
    Top 3 Churn: StableHashMap[Block]+SM64 (4.17x), boost::unordered_node_map (1.82x), StableHashMap+SplitMix64 (1.75x)

  All Results:
    FastHashMap[BS]                 3.18x insert,  1.16x find,  4.23x miss,  3.02x erase,  4.12x churn
    FastHashMap[BS]+SplitMix64      4.29x insert,  1.52x find,  5.92x miss,  4.10x erase,  5.61x churn
    FastHashMap[TS]                 3.37x insert,  1.25x find,  3.82x miss,  6.58x erase,  6.72x churn
    FastHashMap[TS]+SplitMix64      4.66x insert,  1.65x find,  6.64x miss,  9.22x erase,  8.52x churn
    StableHashMap                   1.88x insert,  1.11x find,  3.69x miss,  1.26x erase,  1.61x churn
    StableHashMap+SplitMix64        2.19x insert,  1.49x find,  7.16x miss,  1.49x erase,  1.75x churn
    StableHashMap[Block]+SM64       4.47x insert,  2.70x find,  7.46x miss,  4.08x erase,  4.17x churn
    tsl::robin_map                  2.90x insert,  1.52x find,  1.70x miss,  4.80x erase,  4.98x churn
    ankerl::unordered_dense         3.03x insert,  3.83x find,  7.87x miss,  4.12x erase,  6.72x churn
    absl::flat_hash_map             1.92x insert,  1.69x find,  5.69x miss,  6.53x erase,  6.68x churn
    absl::node_hash_map             1.65x insert,  1.62x find,  4.34x miss,  1.22x erase,  1.67x churn
    boost::unordered_flat_map       4.70x insert,  3.29x find, 13.40x miss, 14.08x erase, 13.77x churn
    boost::unordered_node_map       1.98x insert,  2.56x find,  8.20x miss,  1.86x erase,  1.82x churn
    folly::F14FastMap               2.49x insert,  1.62x find,  2.44x miss,  4.79x erase,  4.77x churn
    folly::F14NodeMap               1.70x insert,  1.79x find,  3.63x miss,  1.20x erase,  1.54x churn
  [NOTE] std::unordered_map: median 145.05 ns (node-based pointer chasing / cache locality effects)
  [NOTE] std::unordered_map: median 259.92 ns (node-based pointer chasing / cache locality effects)
  [NOTE] std::unordered_map: median 294.29 ns (node-based pointer chasing / cache locality effects)

--- Detailed Statistics for FastHashMap[BS] at N=1000000 ---
  Insert (amortized): median=   45.57 mean=   45.80 +/-  0.91 CI95(mean)=[45.40,46.20] min=44.58 max=47.88
     Find(hit): median=   52.18 mean=   51.99 +/-  2.01 CI95(mean)=[51.11,52.87] min=48.70 max=56.05
    Find(miss): median=   16.40 mean=   17.04 +/-  2.16 CI95(mean)=[16.09,17.98] min=15.32 max=23.92
   Erase (25%): median=   86.13 mean=   95.20 +/- 19.55 CI95(mean)=[86.63,103.77] min=81.81 max=152.82
         Churn: median=   71.45 mean=   71.86 +/-  4.83 CI95(mean)=[69.74,73.97] min=66.49 max=86.50

--- Detailed Statistics for FastHashMap[BS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   33.79 mean=   34.07 +/-  0.81 CI95(mean)=[33.72,34.43] min=33.38 max=36.83
     Find(hit): median=   39.71 mean=   40.61 +/-  3.73 CI95(mean)=[38.97,42.25] min=36.34 max=50.69
    Find(miss): median=   11.73 mean=   12.38 +/-  1.79 CI95(mean)=[11.60,13.17] min=10.71 max=17.43
   Erase (25%): median=   63.37 mean=   65.21 +/-  5.38 CI95(mean)=[62.85,67.57] min=59.45 max=83.09
         Churn: median=   52.43 mean=   53.39 +/-  3.93 CI95(mean)=[51.67,55.12] min=50.51 max=67.94

--- Detailed Statistics for FastHashMap[TS] at N=1000000 ---
  Insert (amortized): median=   43.09 mean=   43.42 +/-  1.76 CI95(mean)=[42.65,44.19] min=40.70 max=46.77
     Find(hit): median=   48.38 mean=   48.36 +/-  4.69 CI95(mean)=[46.30,50.41] min=41.09 max=58.22
    Find(miss): median=   18.15 mean=   18.65 +/-  1.54 CI95(mean)=[17.97,19.33] min=17.16 max=23.45
   Erase (25%): median=   39.48 mean=   41.27 +/-  7.69 CI95(mean)=[37.90,44.64] min=34.37 max=71.03
         Churn: median=   43.79 mean=   45.47 +/-  4.44 CI95(mean)=[43.52,47.41] min=40.95 max=59.02

--- Detailed Statistics for FastHashMap[TS]+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   31.15 mean=   31.64 +/-  1.02 CI95(mean)=[31.20,32.09] min=30.56 max=34.17
     Find(hit): median=   36.63 mean=   36.42 +/-  3.08 CI95(mean)=[35.07,37.77] min=31.18 max=43.28
    Find(miss): median=   10.45 mean=   10.77 +/-  1.36 CI95(mean)=[10.18,11.37] min=9.68 max=16.03
   Erase (25%): median=   28.19 mean=   29.04 +/-  4.71 CI95(mean)=[26.98,31.10] min=24.72 max=46.52
         Churn: median=   34.56 mean=   35.14 +/-  2.45 CI95(mean)=[34.07,36.22] min=32.28 max=41.52

--- Detailed Statistics for StableHashMap at N=1000000 ---
  Insert (amortized): median=   77.00 mean=   77.81 +/-  4.21 CI95(mean)=[75.96,79.66] min=71.88 max=88.13
     Find(hit): median=   54.26 mean=   56.25 +/-  8.56 CI95(mean)=[52.50,60.00] min=47.04 max=85.95
    Find(miss): median=   18.79 mean=   19.31 +/-  1.81 CI95(mean)=[18.52,20.11] min=18.05 max=26.46
   Erase (25%): median=  205.66 mean=  217.15 +/- 24.20 CI95(mean)=[206.54,227.75] min=186.39 max=277.85
         Churn: median=  182.49 mean=  185.56 +/- 11.47 CI95(mean)=[180.54,190.59] min=174.31 max=218.96

--- Detailed Statistics for StableHashMap+SplitMix64 at N=1000000 ---
  Insert (amortized): median=   66.33 mean=   66.83 +/-  3.69 CI95(mean)=[65.22,68.45] min=61.93 max=74.82
     Find(hit): median=   40.43 mean=   41.15 +/-  4.46 CI95(mean)=[39.19,43.11] min=35.79 max=52.37
    Find(miss): median=    9.69 mean=    9.78 +/-  0.45 CI95(mean)=[9.58,9.97] min=9.25 max=11.05
   Erase (25%): median=  174.55 mean=  177.01 +/- 11.68 CI95(mean)=[171.89,182.13] min=166.22 max=216.11
         Churn: median=  168.14 mean=  171.91 +/-  8.79 CI95(mean)=[168.06,175.76] min=163.51 max=192.25

--- Detailed Statistics for StableHashMap[Block]+SM64 at N=1000000 ---
  Insert (amortized): median=   32.46 mean=   33.85 +/-  3.76 CI95(mean)=[32.20,35.50] min=28.63 max=43.60
     Find(hit): median=   22.36 mean=   23.78 +/-  5.68 CI95(mean)=[21.29,26.27] min=19.30 max=42.51
    Find(miss): median=    9.31 mean=    9.98 +/-  1.60 CI95(mean)=[9.28,10.68] min=9.02 max=15.36
   Erase (25%): median=   63.65 mean=   65.34 +/-  5.51 CI95(mean)=[62.93,67.75] min=58.95 max=79.27
         Churn: median=   70.49 mean=   71.59 +/-  4.69 CI95(mean)=[69.54,73.65] min=66.23 max=81.05

[Cooling: before miss diagnostics][CPU stable at 2445 MHz (100% of base, variance: 0.0%)]
 [Ready: 2445 MHz]

================================================================================
  MISS DIAGNOSTICS (Slim)
================================================================================

Random misses only (no H2-biased sets), fixed N=1,000,000.
Purpose: regression tripwire for unsuccessful lookup behavior.

[2026-02-16 04:59:55] MissDiag CPU: 2445 MHz (base: 2445)

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=1000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        7.11          0.01          1.00        1.00          7.65        0.00        0.01
StableHashMap[Block]+SM64 (counted)        6.92          0.01          1.00        1.00          7.65        0.00        0.01
boost::unordered_node_map+SM64 (counted)        9.20          0.03          1.00           -             -           -           -
[Cooling: miss reserve change][CPU stable at 2445 MHz (100% of base, variance: 0.0%)]
 [Ready: 2445 MHz]

--- MISS DIAGNOSTIC: Random misses ---
N=1000000 reserve=2000000
                           Library  Median(ns)       Eq/miss     Hash/miss    Grp/miss   FullSlots/m   FullGrp/m    Tag/miss
----------------------------------------------------------------------------------------------------------------------------
      StableHashMap+SM64 (counted)        6.29          0.00          1.00        1.00          3.81        0.00        0.00
StableHashMap[Block]+SM64 (counted)        6.16          0.00          1.00        1.00          3.81        0.00        0.00
boost::unordered_node_map+SM64 (counted)        7.83          0.02          1.00           -             -           -           -

[Cooling: before pathological erase][CPU stable at 2445 MHz (100% of base, variance: 0.0%)]
 [Ready: 2445 MHz]

================================================================================
  PATHOLOGICAL ERASE (TOMBSTONE DEGRADATION TEST)
================================================================================

Tests sustained churn on a single table without reset.
Tombstone-based maps may degrade over time.
Backward-shift maps stay stable.
Methodology: 3 warmup + 20 measured runs
             Round-robin execution with randomized order

[2026-02-16 05:00:20] Starting CPU: 2445 MHz (base: 2445)
N = 100000, Total operations = 5000000

               StableHashMap:   195.31 ns/step (+/-11.20, CI:[192.32,202.13])
    StableHashMap+SplitMix64:   178.95 ns/step (+/-11.19, CI:[176.26,186.07])
   StableHashMap[Block]+SM64:    61.61 ns/step (+/-1.52, CI:[60.83,62.16])
             FastHashMap[BS]:    63.19 ns/step (+/-1.93, CI:[63.13,64.83])
  FastHashMap[BS]+SplitMix64:    49.55 ns/step (+/-1.08, CI:[49.46,50.41])
             FastHashMap[TS]:    63.64 ns/step (+/-1.44, CI:[62.91,64.17])
  FastHashMap[TS]+SplitMix64:    46.60 ns/step (+/-3.46, CI:[45.91,48.95])
              tsl::robin_map:    35.07 ns/step (+/-3.46, CI:[33.78,36.81])
     ankerl::unordered_dense:    43.99 ns/step (+/-2.01, CI:[43.86,45.62])
         absl::flat_hash_map:    53.74 ns/step (+/-1.33, CI:[53.62,54.78])
         absl::node_hash_map:   178.05 ns/step (+/-5.85, CI:[177.04,182.16])
   boost::unordered_flat_map:    20.07 ns/step (+/-0.59, CI:[20.07,20.59])
   boost::unordered_node_map:   144.79 ns/step (+/-12.52, CI:[142.64,153.61])
           folly::F14FastMap:    70.30 ns/step (+/-1.93, CI:[69.76,71.45])
           folly::F14NodeMap:   200.09 ns/step (+/-12.63, CI:[197.01,208.08])
          std::unordered_map:   190.25 ns/step (+/-7.39, CI:[186.74,193.22])

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- folly::F14FastMap was not detected on Local.
- llvm::DenseMap was not detected on MSVC CI.
