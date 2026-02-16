---
doc_id: BR-FeatureManager-001
doc_type: "Benchmark Results"
title: "FeatureManager"
fatp_components: ["FeatureManager"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - FeatureManager

**Source:** `benchmark_FeatureManager.cpp`
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
| fat_p::feature::FeatureManager | x | x | x | x |
| std::map<string, bool> | x | x | x | x |
| Manual if/else chain | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  FIXTURE SETUP
================================================================================

[2026-02-15 19:22:42] Creating fixtures CPU: 3391 MHz (base: 3686)
  Verifying fixtures...
    [OK] All fixture validations passed

================================================================================
  LOOKUP OPERATIONS
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: isEnabled() is O(log n) lookup in std::map<string, FeatureNode>

================================================================================
  VALIDATION OPERATIONS
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: validate() traverses full dependency graph, O(n * d * log n)

================================================================================
  ENABLE/DISABLE OPERATIONS
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: enable() recursively enables dependencies with rollback on failure

================================================================================
  OBSERVER OVERHEAD
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: Observers are called synchronously on state change

================================================================================
  SERIALIZATION
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: JSON round-trip must preserve enabled state and relationships

================================================================================
  GRAPH CONSTRUCTION
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: addFeature() is O(log n) map insertion

================================================================================
  SYNCHRONIZATION POLICY OVERHEAD
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: Comparing SingleThreadedPolicy vs MutexSynchronizationPolicy

================================================================================
  GROUP OPERATIONS
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: Group state computed from member feature states

================================================================================
  BATCH OPERATIONS SCALING
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: batchEnable atomically enables multiple features with rollback

================================================================================
  DENSE GRAPH OPERATIONS
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: Performance with 1000+ relationships for scaling analysis

================================================================================
  SCOPED FEATURE CHANGE (RAII)
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: ScopedFeatureChange provides temporary state with auto-rollback

================================================================================
  CUSTOM STATE COMPUTER
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: User-provided state computation logic for groups

================================================================================
  MEMORY & CONSTRUCTION SCALING
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: Construction cost scaling with features and relationships

================================================================================
  MUTUALLY EXCLUSIVE GROUPS
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
  Contract: addMutuallyExclusiveGroup creates O(n^2) conflict relationships

================================================================================
  RUNNING BENCHMARKS
================================================================================

[2026-02-15 19:22:42] Section start CPU: 3391 MHz (base: 3686)
[2026-02-15 19:22:42] Starting benchmark execution CPU: 3391 MHz (base: 3686)
    isEnabled: enabled hit (10k features):    40.00 ns/op  (+/- 50.71)  CI95=[11.98, 68.02]
    isEnabled: disabled hit (10k features):   106.67 ns/op  (+/- 25.82)  CI95=[92.40, 120.93]
    isEnabled: missing feature (10k features):   133.33 ns/op  (+/- 61.72)  CI95=[99.23, 167.44]
    validate: requires-chain depth 50 (all enabled): 10266.67 ns/op  (+/-649.91)  CI95=[9907.56, 10625.77]
    validate: flat graph 10k (no dependencies): 1385053.33 ns/op  (+/-472150.68)  CI95=[1124168.54, 1645938.13]
    validate: conflict graph 100 features: 148806.67 ns/op  (+/-3769.32)  CI95=[146723.94, 150889.39]
    batchEnable + batchDisable: chain depth 50 (cold): 84300.00 ns/op  (+/-2914.99)  CI95=[82689.34, 85910.66]
    enable + disable: single feature (no deps):  2066.67 ns/op  (+/-202.37)  CI95=[1954.85, 2178.48]
    enable: conflict detection (100 conflicts): 853813.33 ns/op  (+/-151728.61)  CI95=[769976.36, 937650.31]
    enable/disable: 0 observers: 591393.33 ns/op  (+/-90231.82)  CI95=[541536.14, 641250.53]
    enable/disable: 1 observer: 502466.67 ns/op  (+/-44392.79)  CI95=[477937.63, 526995.71]
    enable/disable: 10 observers: 505653.33 ns/op  (+/-75423.77)  CI95=[463978.26, 547328.41]
    toJson: 10k features, no relationships: 14844573.33 ns/op  (+/-263242.68)  CI95=[14699119.74, 14990026.92]
    fromJson: 10k features, no relationships: 11583386.67 ns/op  (+/-743242.48)  CI95=[11172711.29, 11994062.05]
    toDot: 100 features, 50 relationships: 72373.33 ns/op  (+/-12846.93)  CI95=[65274.82, 79471.85]
    toDot: 10k features, no relationships: 1635973.33 ns/op  (+/-426012.07)  CI95=[1400582.23, 1871364.44]
    addFeature: build 100 features: 30533.33 ns/op  (+/-13119.05)  CI95=[23284.46, 37782.21]
    addFeature: build 1000 features: 522080.00 ns/op  (+/-108918.24)  CI95=[461897.70, 582262.30]
    addRelationship: 100 Requires edges: 71893.33 ns/op  (+/-104701.95)  CI95=[14040.73, 129745.94]
    isEnabled: SingleThreadedPolicy (10k):   100.00 ns/op  (+/-  0.00)  CI95=[100.00, 100.00]
    isEnabled: MutexSynchronizationPolicy (10k):   106.67 ns/op  (+/- 25.82)  CI95=[92.40, 120.93]
    getGroupState: 20-member group:  2720.00 ns/op  (+/-108.23)  CI95=[2660.20, 2779.80]
    batchEnable: 10 features (no deps):  3333.33 ns/op  (+/-198.81)  CI95=[3223.48, 3443.18]
    batchEnable: 100 features (no deps): 72920.00 ns/op  (+/-15745.53)  CI95=[64219.88, 81620.12]
    batchEnable: 1000 features (no deps): 725020.00 ns/op  (+/-66699.92)  CI95=[688165.25, 761874.75]
    batchDisable: 100 features (no deps): 88066.67 ns/op  (+/-5813.74)  CI95=[84854.31, 91279.02]
    validate: dense graph (200 nodes, ~1000 edges): 33913.33 ns/op  (+/-3764.66)  CI95=[31833.19, 35993.48]
    toJson: dense graph (200 nodes, ~1000 edges): 1259500.00 ns/op  (+/-110450.32)  CI95=[1198471.16, 1320528.84]
    fromJson: dense graph (200 nodes, ~1000 edges): 517386.67 ns/op  (+/-188712.68)  CI95=[413114.30, 621659.03]
    validate: very dense graph (500 nodes, ~5000 edges): 86400.00 ns/op  (+/-83940.40)  CI95=[40019.10, 132780.90]
    validate: tree graph (depth 5, branching 3): 77533.33 ns/op  (+/-9118.40)  CI95=[72495.00, 82571.67]
    enable: tree root (cascades to 364 nodes): 163906.67 ns/op  (+/-52380.88)  CI95=[134963.84, 192849.49]
    ScopedFeatureChange: enable then auto-restore:  1920.00 ns/op  (+/-156.75)  CI95=[1833.39, 2006.61]
    ScopedFeatureChange: disable then auto-restore:  4866.67 ns/op  (+/-6156.72)  CI95=[1464.80, 8268.54]
    ScopedFeatureChange: nested scopes (3 deep):  6133.33 ns/op  (+/-202.37)  CI95=[6021.52, 6245.15]
    getGroupState: default computer (50 features): 10946.67 ns/op  (+/-1378.85)  CI95=[10184.79, 11708.55]
    getGroupState: custom computer (50 features): 10413.33 ns/op  (+/-140.75)  CI95=[10335.56, 10491.10]
    getGroupState: default computer (200 features): 90133.33 ns/op  (+/-7252.16)  CI95=[86126.18, 94140.48]
    construct: 100 features + 50 relationships: 59233.33 ns/op  (+/-104533.81)  CI95=[1473.64, 116993.03]
    construct: 1000 features + 500 relationships: 324746.67 ns/op  (+/-283686.45)  CI95=[167996.96, 481496.37]
    construct: 5000 features + 2500 relationships: 1707740.00 ns/op  (+/-147227.76)  CI95=[1626389.95, 1789090.05]
    move: 1000-feature graph: 296080.00 ns/op  (+/-154980.41)  CI95=[210446.26, 381713.74]
    clear: 1000-feature graph: 318806.67 ns/op  (+/-187543.03)  CI95=[215180.58, 422432.75]
    addMutuallyExclusiveGroup: 10 features:  9046.67 ns/op  (+/-318.18)  CI95=[8870.86, 9222.48]
    addMutuallyExclusiveGroup: 50 features: 225480.00 ns/op  (+/-70653.94)  CI95=[186440.48, 264519.52]
    validate: mutually exclusive group (20 features):  8433.33 ns/op  (+/-129.10)  CI95=[8362.00, 8504.67]
    enable: conflict in mutually exclusive group:  1820.00 ns/op  (+/-185.93)  CI95=[1717.26, 1922.74]
[2026-02-15 19:22:58] Benchmark execution complete CPU: 2027 MHz (base: 3686)

================================================================================
  CONCURRENT ACCESS PATTERNS
================================================================================

  Contract: Multi-threaded read/write contention with MutexSynchronizationPolicy

[2026-02-15 19:22:58] Concurrent benchmarks CPU: 2027 MHz (base: 3686)
  Hardware threads: 24, using up to 8
  Pinning policy: ON (Windows only)

  Thread Scaling (read-only, 300ms warmup + 500ms measured):
    Threads  |  Throughput (ops/sec)  |  Per-Thread
    ---------+------------------------+-------------
          1  |              25988980  |     25988980
          2  |              23284932  |     11642466
          4  |              21315243  |      5328811
          8  |               8519999  |      1065000

  Mixed read-write (6 readers, 2 writers, with barrier):
    Total reads:   43904
    Total writes:  17363 (failed: 0)
    Read throughput:  87715 ops/sec
    Write throughput: 34689 ops/sec

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
[CPU stabilization disabled]

================================================================================
  FIXTURE SETUP
================================================================================

[2026-02-16 03:37:48] Creating fixtures CPU: 3285 MHz (~base: 3285)
  Verifying fixtures...
    [OK] All fixture validations passed
================================================================================
  LOOKUP OPERATIONS
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: isEnabled() is O(log n) lookup in std::map<string, FeatureNode>
================================================================================
  VALIDATION OPERATIONS
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: validate() traverses full dependency graph, O(n * d * log n)
================================================================================
  ENABLE/DISABLE OPERATIONS
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: enable() recursively enables dependencies with rollback on failure
================================================================================
  OBSERVER OVERHEAD
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: Observers are called synchronously on state change
================================================================================
  SERIALIZATION
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: JSON round-trip must preserve enabled state and relationships
================================================================================
  GRAPH CONSTRUCTION
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: addFeature() is O(log n) map insertion
================================================================================
  SYNCHRONIZATION POLICY OVERHEAD
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: Comparing SingleThreadedPolicy vs MutexSynchronizationPolicy
================================================================================
  GROUP OPERATIONS
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: Group state computed from member feature states
================================================================================
  BATCH OPERATIONS SCALING
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: batchEnable atomically enables multiple features with rollback
================================================================================
  DENSE GRAPH OPERATIONS
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3240 MHz (~base: 3240)
  Contract: Performance with 1000+ relationships for scaling analysis
================================================================================
  SCOPED FEATURE CHANGE (RAII)
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3236 MHz (~base: 3236)
  Contract: ScopedFeatureChange provides temporary state with auto-rollback
================================================================================
  CUSTOM STATE COMPUTER
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3236 MHz (~base: 3236)
  Contract: User-provided state computation logic for groups
================================================================================
  MEMORY & CONSTRUCTION SCALING
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3236 MHz (~base: 3236)
  Contract: Construction cost scaling with features and relationships
================================================================================
  MUTUALLY EXCLUSIVE GROUPS
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3236 MHz (~base: 3236)
  Contract: addMutuallyExclusiveGroup creates O(n^2) conflict relationships
================================================================================
  RUNNING BENCHMARKS
================================================================================

[2026-02-16 03:37:48] Section start CPU: 3236 MHz (~base: 3236)
[2026-02-16 03:37:48] Starting benchmark execution CPU: 3236 MHz (~base: 3236)
    isEnabled: enabled hit (10k features):    68.25 ns/op  (+/- 14.37)  CI95=[61.37, 75.13]
    isEnabled: disabled hit (10k features):    52.20 ns/op  (+/-  8.91)  CI95=[47.94, 56.46]
    isEnabled: missing feature (10k features):    90.60 ns/op  (+/- 20.62)  CI95=[80.73, 100.47]
    validate: requires-chain depth 50 (all enabled): 15053.15 ns/op  (+/-505.49)  CI95=[14811.26, 15295.04]
    validate: flat graph 10k (no dependencies): 1696733.15 ns/op  (+/-16719.00)  CI95=[1688732.80, 1704733.50]
    validate: conflict graph 100 features: 173919.10 ns/op  (+/-7307.37)  CI95=[170422.39, 177415.81]
    batchEnable + batchDisable: chain depth 50 (cold): 73531.15 ns/op  (+/-7359.39)  CI95=[70009.55, 77052.75]
    enable + disable: single feature (no deps):  1114.15 ns/op  (+/- 85.94)  CI95=[1073.03, 1155.27]
    enable: conflict detection (100 conflicts): 1271872.25 ns/op  (+/-6981.33)  CI95=[1268531.56, 1275212.94]
    enable/disable: 0 observers: 273699.50 ns/op  (+/-5203.60)  CI95=[271209.48, 276189.52]
    enable/disable: 1 observer: 279175.80 ns/op  (+/-13645.33)  CI95=[272646.25, 285705.35]
    enable/disable: 10 observers: 277436.60 ns/op  (+/-7041.21)  CI95=[274067.25, 280805.95]
    toJson: 10k features, no relationships: 9244691.30 ns/op  (+/-33076.67)  CI95=[9228863.50, 9260519.10]
    fromJson: 10k features, no relationships: 11743325.90 ns/op  (+/-1050418.06)  CI95=[11240681.38, 12245970.42]
    toDot: 100 features, 50 relationships: 28818.80 ns/op  (+/-2058.94)  CI95=[27833.56, 29804.04]
    toDot: 10k features, no relationships: 1154812.75 ns/op  (+/-6458.14)  CI95=[1151722.41, 1157903.09]
    addFeature: build 100 features: 17567.30 ns/op  (+/- 60.58)  CI95=[17538.31, 17596.29]
    addFeature: build 1000 features: 194015.60 ns/op  (+/-6698.98)  CI95=[190810.01, 197221.19]
    addRelationship: 100 Requires edges: 35346.25 ns/op  (+/-7956.02)  CI95=[31539.15, 39153.35]
    isEnabled: SingleThreadedPolicy (10k):    72.15 ns/op  (+/- 14.71)  CI95=[65.11, 79.19]
    isEnabled: MutexSynchronizationPolicy (10k):    69.20 ns/op  (+/- 17.31)  CI95=[60.92, 77.48]
    getGroupState: 20-member group:  1883.00 ns/op  (+/- 24.99)  CI95=[1871.04, 1894.96]
    batchEnable: 10 features (no deps):  4496.90 ns/op  (+/- 65.52)  CI95=[4465.55, 4528.25]
    batchEnable: 100 features (no deps): 42887.90 ns/op  (+/-149.10)  CI95=[42816.55, 42959.25]
    batchEnable: 1000 features (no deps): 442013.75 ns/op  (+/-7177.27)  CI95=[438579.30, 445448.20]
    batchDisable: 100 features (no deps): 49724.75 ns/op  (+/-5609.42)  CI95=[47040.54, 52408.96]
    validate: dense graph (200 nodes, ~1000 edges): 28549.80 ns/op  (+/-5325.27)  CI95=[26001.56, 31098.04]
    toJson: dense graph (200 nodes, ~1000 edges): 715959.15 ns/op  (+/-18362.49)  CI95=[707172.36, 724745.94]
    fromJson: dense graph (200 nodes, ~1000 edges): 714169.05 ns/op  (+/-28441.61)  CI95=[700559.21, 727778.89]
    validate: very dense graph (500 nodes, ~5000 edges): 157557.65 ns/op  (+/-6074.19)  CI95=[154651.04, 160464.26]
    validate: tree graph (depth 5, branching 3): 79967.10 ns/op  (+/-12409.94)  CI95=[74028.71, 85905.49]
    enable: tree root (cascades to 364 nodes): 187517.00 ns/op  (+/-20293.12)  CI95=[177806.36, 197227.64]
    ScopedFeatureChange: enable then auto-restore:  3238.00 ns/op  (+/-6027.21)  CI95=[353.87, 6122.13]
    ScopedFeatureChange: disable then auto-restore:  1905.50 ns/op  (+/- 59.77)  CI95=[1876.90, 1934.10]
    ScopedFeatureChange: nested scopes (3 deep):  4716.35 ns/op  (+/- 54.16)  CI95=[4690.44, 4742.26]
    getGroupState: default computer (50 features): 10409.90 ns/op  (+/- 42.17)  CI95=[10389.72, 10430.08]
    getGroupState: custom computer (50 features): 10157.50 ns/op  (+/- 32.07)  CI95=[10142.15, 10172.85]
    getGroupState: default computer (200 features): 106213.90 ns/op  (+/-16149.44)  CI95=[98486.10, 113941.70]
    construct: 100 features + 50 relationships: 25505.55 ns/op  (+/-263.17)  CI95=[25379.62, 25631.48]
    construct: 1000 features + 500 relationships: 279367.10 ns/op  (+/-8056.00)  CI95=[275512.15, 283222.05]
    construct: 5000 features + 2500 relationships: 1448418.70 ns/op  (+/-6001.90)  CI95=[1445546.68, 1451290.72]
    move: 1000-feature graph: 203305.45 ns/op  (+/-17615.58)  CI95=[194876.07, 211734.83]
    clear: 1000-feature graph: 205986.50 ns/op  (+/-21772.48)  CI95=[195567.96, 216405.04]
    addMutuallyExclusiveGroup: 10 features:  9930.55 ns/op  (+/-474.21)  CI95=[9703.63, 10157.47]
    addMutuallyExclusiveGroup: 50 features: 293456.45 ns/op  (+/-12657.78)  CI95=[287399.47, 299513.43]
    validate: mutually exclusive group (20 features): 11106.25 ns/op  (+/-332.37)  CI95=[10947.20, 11265.30]
    enable: conflict in mutually exclusive group:  1816.90 ns/op  (+/- 80.85)  CI95=[1778.21, 1855.59]
[2026-02-16 03:37:58] Benchmark execution complete CPU: 2445 MHz (~base: 2445)

================================================================================
  CONCURRENT ACCESS PATTERNS
================================================================================

  Contract: Multi-threaded read/write contention with MutexSynchronizationPolicy

[2026-02-16 03:37:58] Concurrent benchmarks CPU: 2445 MHz (~base: 2445)
  Hardware threads: 4, using up to 4
  Pinning policy: ON (Windows only)

  Thread Scaling (read-only, 300ms warmup + 500ms measured):
    Threads  |  Throughput (ops/sec)  |  Per-Thread
    ---------+------------------------+-------------
          1  |              27148049  |     27148049
          2  |               8168154  |      4084077
          4  |               6668045  |      1667011

  Mixed read-write (3 readers, 1 writers, with barrier):
    Total reads:   2770
    Total writes:  12001 (failed: 0)
    Read throughput:  5538 ops/sec
    Write throughput: 23993 ops/sec
================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
[CPU stabilization disabled]

================================================================================
  FIXTURE SETUP
================================================================================

[2026-02-16 04:11:24] Creating fixtures CPU: 3239 MHz (~base: 3239)
  Verifying fixtures...
    [OK] All fixture validations passed
================================================================================
  LOOKUP OPERATIONS
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: isEnabled() is O(log n) lookup in std::map<string, FeatureNode>
================================================================================
  VALIDATION OPERATIONS
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: validate() traverses full dependency graph, O(n * d * log n)
================================================================================
  ENABLE/DISABLE OPERATIONS
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: enable() recursively enables dependencies with rollback on failure
================================================================================
  OBSERVER OVERHEAD
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: Observers are called synchronously on state change
================================================================================
  SERIALIZATION
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: JSON round-trip must preserve enabled state and relationships
================================================================================
  GRAPH CONSTRUCTION
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: addFeature() is O(log n) map insertion
================================================================================
  SYNCHRONIZATION POLICY OVERHEAD
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: Comparing SingleThreadedPolicy vs MutexSynchronizationPolicy
================================================================================
  GROUP OPERATIONS
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: Group state computed from member feature states
================================================================================
  BATCH OPERATIONS SCALING
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: batchEnable atomically enables multiple features with rollback
================================================================================
  DENSE GRAPH OPERATIONS
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3196 MHz (~base: 3196)
  Contract: Performance with 1000+ relationships for scaling analysis
================================================================================
  SCOPED FEATURE CHANGE (RAII)
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3242 MHz (~base: 3242)
  Contract: ScopedFeatureChange provides temporary state with auto-rollback
================================================================================
  CUSTOM STATE COMPUTER
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3242 MHz (~base: 3242)
  Contract: User-provided state computation logic for groups
================================================================================
  MEMORY & CONSTRUCTION SCALING
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3241 MHz (~base: 3241)
  Contract: Construction cost scaling with features and relationships
================================================================================
  MUTUALLY EXCLUSIVE GROUPS
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3241 MHz (~base: 3241)
  Contract: addMutuallyExclusiveGroup creates O(n^2) conflict relationships
================================================================================
  RUNNING BENCHMARKS
================================================================================

[2026-02-16 04:11:24] Section start CPU: 3241 MHz (~base: 3241)
[2026-02-16 04:11:24] Starting benchmark execution CPU: 3241 MHz (~base: 3241)
    isEnabled: enabled hit (10k features):    63.00 ns/op  (+/- 14.94)  CI95=[55.85, 70.15]
    isEnabled: disabled hit (10k features):    48.65 ns/op  (+/- 10.87)  CI95=[43.45, 53.85]
    isEnabled: missing feature (10k features):    67.65 ns/op  (+/- 12.60)  CI95=[61.62, 73.68]
    validate: requires-chain depth 50 (all enabled): 15089.80 ns/op  (+/-5204.71)  CI95=[12599.25, 17580.35]
    validate: flat graph 10k (no dependencies): 1601064.60 ns/op  (+/-15551.83)  CI95=[1593622.76, 1608506.44]
    validate: conflict graph 100 features: 161137.90 ns/op  (+/-14042.54)  CI95=[154418.29, 167857.51]
    batchEnable + batchDisable: chain depth 50 (cold): 70401.90 ns/op  (+/-10390.66)  CI95=[65429.78, 75374.02]
    enable + disable: single feature (no deps):  1247.40 ns/op  (+/- 56.01)  CI95=[1220.60, 1274.20]
    enable: conflict detection (100 conflicts): 1108639.60 ns/op  (+/-6575.39)  CI95=[1105493.15, 1111786.05]
    enable/disable: 0 observers: 253545.00 ns/op  (+/-6752.09)  CI95=[250314.00, 256776.00]
    enable/disable: 1 observer: 257220.60 ns/op  (+/-9680.37)  CI95=[252588.37, 261852.83]
    enable/disable: 10 observers: 250803.45 ns/op  (+/-6361.39)  CI95=[247759.41, 253847.49]
    toJson: 10k features, no relationships: 8875627.65 ns/op  (+/-70707.19)  CI95=[8841792.95, 8909462.35]
    fromJson: 10k features, no relationships: 12145130.45 ns/op  (+/-1086612.34)  CI95=[11625166.29, 12665094.61]
    toDot: 100 features, 50 relationships: 25410.45 ns/op  (+/-7132.18)  CI95=[21997.57, 28823.33]
    toDot: 10k features, no relationships: 1049966.95 ns/op  (+/-19780.48)  CI95=[1040501.62, 1059432.28]
    addFeature: build 100 features: 14154.90 ns/op  (+/- 65.46)  CI95=[14123.57, 14186.23]
    addFeature: build 1000 features: 181077.65 ns/op  (+/-9674.33)  CI95=[176448.30, 185707.00]
    addRelationship: 100 Requires edges: 28372.35 ns/op  (+/-5418.37)  CI95=[25779.56, 30965.14]
    isEnabled: SingleThreadedPolicy (10k):    58.00 ns/op  (+/- 13.27)  CI95=[51.65, 64.35]
    isEnabled: MutexSynchronizationPolicy (10k):    66.70 ns/op  (+/- 11.35)  CI95=[61.27, 72.13]
    getGroupState: 20-member group:  3158.35 ns/op  (+/-6439.62)  CI95=[76.87, 6239.83]
    batchEnable: 10 features (no deps):  3918.30 ns/op  (+/- 57.25)  CI95=[3890.91, 3945.69]
    batchEnable: 100 features (no deps): 39611.60 ns/op  (+/-5182.74)  CI95=[37131.57, 42091.63]
    batchEnable: 1000 features (no deps): 405967.60 ns/op  (+/-6111.28)  CI95=[403043.24, 408891.96]
    batchDisable: 100 features (no deps): 49858.70 ns/op  (+/-7989.30)  CI95=[46035.67, 53681.73]
    validate: dense graph (200 nodes, ~1000 edges): 31291.30 ns/op  (+/-7204.50)  CI95=[27843.81, 34738.79]
    toJson: dense graph (200 nodes, ~1000 edges): 687968.15 ns/op  (+/-6493.21)  CI95=[684861.03, 691075.27]
    fromJson: dense graph (200 nodes, ~1000 edges): 698312.95 ns/op  (+/-25578.42)  CI95=[686073.20, 710552.70]
    validate: very dense graph (500 nodes, ~5000 edges): 138513.55 ns/op  (+/-6550.32)  CI95=[135379.10, 141648.00]
    validate: tree graph (depth 5, branching 3): 67953.90 ns/op  (+/-8704.78)  CI95=[63788.50, 72119.30]
    enable: tree root (cascades to 364 nodes): 166816.05 ns/op  (+/-9451.39)  CI95=[162293.38, 171338.72]
    ScopedFeatureChange: enable then auto-restore:  1586.45 ns/op  (+/- 46.76)  CI95=[1564.07, 1608.83]
    ScopedFeatureChange: disable then auto-restore:  1803.95 ns/op  (+/- 37.58)  CI95=[1785.97, 1821.93]
    ScopedFeatureChange: nested scopes (3 deep):  3906.30 ns/op  (+/-3537.57)  CI95=[2213.51, 5599.09]
    getGroupState: default computer (50 features):  8800.85 ns/op  (+/- 37.97)  CI95=[8782.68, 8819.02]
    getGroupState: custom computer (50 features):  8717.35 ns/op  (+/- 32.79)  CI95=[8701.66, 8733.04]
    getGroupState: default computer (200 features): 89081.75 ns/op  (+/-16722.96)  CI95=[81079.50, 97084.00]
    construct: 100 features + 50 relationships: 22336.70 ns/op  (+/-4403.94)  CI95=[20229.34, 24444.06]
    construct: 1000 features + 500 relationships: 247844.35 ns/op  (+/-5228.12)  CI95=[245342.60, 250346.10]
    construct: 5000 features + 2500 relationships: 1263165.65 ns/op  (+/-13818.33)  CI95=[1256553.32, 1269777.98]
    move: 1000-feature graph: 176879.35 ns/op  (+/-4029.47)  CI95=[174951.17, 178807.53]
    clear: 1000-feature graph: 183840.80 ns/op  (+/-20861.50)  CI95=[173858.19, 193823.41]
    addMutuallyExclusiveGroup: 10 features:  9466.05 ns/op  (+/-500.27)  CI95=[9226.66, 9705.44]
    addMutuallyExclusiveGroup: 50 features: 273241.15 ns/op  (+/-23988.04)  CI95=[261762.43, 284719.87]
    validate: mutually exclusive group (20 features): 11811.05 ns/op  (+/-7323.91)  CI95=[8306.42, 15315.68]
    enable: conflict in mutually exclusive group:  1556.00 ns/op  (+/- 40.79)  CI95=[1536.48, 1575.52]
[2026-02-16 04:11:35] Benchmark execution complete CPU: 2445 MHz (~base: 2445)

================================================================================
  CONCURRENT ACCESS PATTERNS
================================================================================

  Contract: Multi-threaded read/write contention with MutexSynchronizationPolicy

[2026-02-16 04:11:35] Concurrent benchmarks CPU: 2445 MHz (~base: 2445)
  Hardware threads: 4, using up to 4
  Pinning policy: ON (Windows only)

  Thread Scaling (read-only, 300ms warmup + 500ms measured):
    Threads  |  Throughput (ops/sec)  |  Per-Thread
    ---------+------------------------+-------------
          1  |              26173221  |     26173221
          2  |               8194236  |      4097118
          4  |               6620304  |      1655076

  Mixed read-write (3 readers, 1 writers, with barrier):
    Total reads:   3825
    Total writes:  11832 (failed: 0)
    Read throughput:  7646 ops/sec
    Write throughput: 23652 ops/sec
================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
[CPU stabilization disabled]

================================================================================
  FIXTURE SETUP
================================================================================

[2026-02-16 04:53:40] Creating fixtures CPU: 2445 MHz (base: 2445)
  Verifying fixtures...
    [OK] All fixture validations passed
================================================================================
  LOOKUP OPERATIONS
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: isEnabled() is O(log n) lookup in std::map<string, FeatureNode>
================================================================================
  VALIDATION OPERATIONS
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: validate() traverses full dependency graph, O(n * d * log n)
================================================================================
  ENABLE/DISABLE OPERATIONS
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: enable() recursively enables dependencies with rollback on failure
================================================================================
  OBSERVER OVERHEAD
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: Observers are called synchronously on state change
================================================================================
  SERIALIZATION
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: JSON round-trip must preserve enabled state and relationships
================================================================================
  GRAPH CONSTRUCTION
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: addFeature() is O(log n) map insertion
================================================================================
  SYNCHRONIZATION POLICY OVERHEAD
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: Comparing SingleThreadedPolicy vs MutexSynchronizationPolicy
================================================================================
  GROUP OPERATIONS
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: Group state computed from member feature states
================================================================================
  BATCH OPERATIONS SCALING
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: batchEnable atomically enables multiple features with rollback
================================================================================
  DENSE GRAPH OPERATIONS
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: Performance with 1000+ relationships for scaling analysis
================================================================================
  SCOPED FEATURE CHANGE (RAII)
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: ScopedFeatureChange provides temporary state with auto-rollback
================================================================================
  CUSTOM STATE COMPUTER
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: User-provided state computation logic for groups
================================================================================
  MEMORY & CONSTRUCTION SCALING
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: Construction cost scaling with features and relationships
================================================================================
  MUTUALLY EXCLUSIVE GROUPS
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
  Contract: addMutuallyExclusiveGroup creates O(n^2) conflict relationships
================================================================================
  RUNNING BENCHMARKS
================================================================================

[2026-02-16 04:53:40] Section start CPU: 2445 MHz (base: 2445)
[2026-02-16 04:53:40] Starting benchmark execution CPU: 2445 MHz (base: 2445)
    isEnabled: enabled hit (10k features):    55.00 ns/op  (+/- 51.04)  CI95=[30.58, 79.42]
    isEnabled: disabled hit (10k features):    70.00 ns/op  (+/- 47.02)  CI95=[47.50, 92.50]
    isEnabled: missing feature (10k features):   160.00 ns/op  (+/- 59.82)  CI95=[131.37, 188.63]
    validate: requires-chain depth 50 (all enabled):  8950.00 ns/op  (+/-157.28)  CI95=[8874.74, 9025.26]
    validate: flat graph 10k (no dependencies): 1968750.00 ns/op  (+/-49647.06)  CI95=[1944992.96, 1992507.04]
    validate: conflict graph 100 features: 149160.00 ns/op  (+/-13709.98)  CI95=[142599.52, 155720.48]
    batchEnable + batchDisable: chain depth 50 (cold): 77055.00 ns/op  (+/-2373.87)  CI95=[75919.06, 78190.94]
    enable + disable: single feature (no deps):  2710.00 ns/op  (+/-954.16)  CI95=[2253.42, 3166.58]
    enable: conflict detection (100 conflicts): 1392005.00 ns/op  (+/-60917.19)  CI95=[1362855.00, 1421155.00]
    enable/disable: 0 observers: 721500.00 ns/op  (+/-14656.70)  CI95=[714486.50, 728513.50]
    enable/disable: 1 observer: 733310.00 ns/op  (+/-56732.96)  CI95=[706162.23, 760457.77]
    enable/disable: 10 observers: 740755.00 ns/op  (+/-56749.88)  CI95=[713599.13, 767910.87]
    toJson: 10k features, no relationships: 28309530.00 ns/op  (+/-476415.50)  CI95=[28081556.35, 28537503.65]
    fromJson: 10k features, no relationships: 18956180.00 ns/op  (+/-484922.02)  CI95=[18724135.82, 19188224.18]
    toDot: 100 features, 50 relationships: 59115.00 ns/op  (+/-8912.81)  CI95=[54850.06, 63379.94]
    toDot: 10k features, no relationships: 2706735.00 ns/op  (+/-128941.67)  CI95=[2645034.02, 2768435.98]
    addFeature: build 100 features: 27890.00 ns/op  (+/-1754.36)  CI95=[27050.50, 28729.50]
    addFeature: build 1000 features: 574130.00 ns/op  (+/-15380.61)  CI95=[566770.09, 581489.91]
    addRelationship: 100 Requires edges: 36935.00 ns/op  (+/-236.81)  CI95=[36821.68, 37048.32]
    isEnabled: SingleThreadedPolicy (10k):    75.00 ns/op  (+/- 44.43)  CI95=[53.74, 96.26]
    isEnabled: MutexSynchronizationPolicy (10k):    75.00 ns/op  (+/- 44.43)  CI95=[53.74, 96.26]
    getGroupState: 20-member group:  1530.00 ns/op  (+/- 47.02)  CI95=[1507.50, 1552.50]
    batchEnable: 10 features (no deps):  7155.00 ns/op  (+/-716.33)  CI95=[6812.22, 7497.78]
    batchEnable: 100 features (no deps): 97945.00 ns/op  (+/-24607.67)  CI95=[86169.77, 109720.23]
    batchEnable: 1000 features (no deps): 973365.00 ns/op  (+/-21645.13)  CI95=[963007.40, 983722.60]
    batchDisable: 100 features (no deps): 77510.00 ns/op  (+/-7105.96)  CI95=[74109.67, 80910.33]
    validate: dense graph (200 nodes, ~1000 edges): 30340.00 ns/op  (+/-3826.01)  CI95=[28509.19, 32170.81]
    toJson: dense graph (200 nodes, ~1000 edges): 2518285.00 ns/op  (+/-51496.42)  CI95=[2493643.01, 2542926.99]
    fromJson: dense graph (200 nodes, ~1000 edges): 881725.00 ns/op  (+/-24672.14)  CI95=[869918.92, 893531.08]
    validate: very dense graph (500 nodes, ~5000 edges): 140920.00 ns/op  (+/-9117.28)  CI95=[136557.21, 145282.79]
    validate: tree graph (depth 5, branching 3): 72305.00 ns/op  (+/-5259.22)  CI95=[69788.36, 74821.64]
    enable: tree root (cascades to 364 nodes): 228250.00 ns/op  (+/-11088.14)  CI95=[222944.12, 233555.88]
    ScopedFeatureChange: enable then auto-restore:  2060.00 ns/op  (+/-203.65)  CI95=[1962.55, 2157.45]
    ScopedFeatureChange: disable then auto-restore:  2595.00 ns/op  (+/-248.10)  CI95=[2476.28, 2713.72]
    ScopedFeatureChange: nested scopes (3 deep):  5960.00 ns/op  (+/-454.68)  CI95=[5742.43, 6177.57]
    getGroupState: default computer (50 features):  9840.00 ns/op  (+/-311.87)  CI95=[9690.76, 9989.24]
    getGroupState: custom computer (50 features):  9565.00 ns/op  (+/-108.94)  CI95=[9512.87, 9617.13]
    getGroupState: default computer (200 features): 96960.00 ns/op  (+/-6559.24)  CI95=[93821.28, 100098.72]
    construct: 100 features + 50 relationships: 36825.00 ns/op  (+/-693.48)  CI95=[36493.15, 37156.85]
    construct: 1000 features + 500 relationships: 459145.00 ns/op  (+/-95308.38)  CI95=[413538.17, 504751.83]
    construct: 5000 features + 2500 relationships: 2519445.00 ns/op  (+/-90696.90)  CI95=[2476044.85, 2562845.15]
    move: 1000-feature graph: 393920.00 ns/op  (+/-108459.46)  CI95=[342020.13, 445819.87]
    clear: 1000-feature graph: 374215.00 ns/op  (+/-99305.36)  CI95=[326695.54, 421734.46]
    addMutuallyExclusiveGroup: 10 features: 13890.00 ns/op  (+/-645.55)  CI95=[13581.09, 14198.91]
    addMutuallyExclusiveGroup: 50 features: 271765.00 ns/op  (+/-18350.11)  CI95=[262984.13, 280545.87]
    validate: mutually exclusive group (20 features):  8980.00 ns/op  (+/-110.50)  CI95=[8927.12, 9032.88]
    enable: conflict in mutually exclusive group:  2035.00 ns/op  (+/-205.90)  CI95=[1936.47, 2133.53]
[2026-02-16 04:53:55] Benchmark execution complete CPU: 2445 MHz (base: 2445)

================================================================================
  CONCURRENT ACCESS PATTERNS
================================================================================

  Contract: Multi-threaded read/write contention with MutexSynchronizationPolicy

[2026-02-16 04:53:55] Concurrent benchmarks CPU: 2445 MHz (base: 2445)
  Hardware threads: 4, using up to 4
  Pinning policy: ON (Windows only)

  Thread Scaling (read-only, 300ms warmup + 500ms measured):
    Threads  |  Throughput (ops/sec)  |  Per-Thread
    ---------+------------------------+-------------
          1  |              21340075  |     21340075
          2  |              20280385  |     10140193
          4  |              12576499  |      3144125

  Mixed read-write (3 readers, 1 writers, with barrier):
    Total reads:   99689
    Total writes:  11151 (failed: 0)
    Read throughput:  195980 ops/sec
    Write throughput: 21922 ops/sec
================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
