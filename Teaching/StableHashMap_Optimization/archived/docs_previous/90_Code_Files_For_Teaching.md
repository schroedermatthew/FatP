# Code files to ship with the teaching documents

This manifest lists the **source code** that pairs with the teaching docs:
- the map implementations,
- the benchmark harness,
- the miss diagnostics,
- and the experimental “Option A/B” variants used to isolate the root cause.

If you want a *single download*, grab the **“Full history + variants”** zip at the bottom.

---

## 1) Core map implementations

### StableHashMap (reference-stable)
- `StableHashMap.h`  
  Main header implementing StableHashMap and its internals.

### FastHashMap (faster, non-reference-stable variants)
- `FastHashMap.h`  
  Main header implementing FastHashMap (BS/TS policies).

### Memory / allocation + SIMD plumbing
- `AllocationStrategies.h`  
  Allocation + node storage strategies used by the maps.

- `FatPSimdDetection.h`  
  SIMD backend detection + tag/empty matching helpers (AVX2/SSE paths).

---

## 2) Benchmark harness and diagnostics

### Full benchmark suite
- `benchmark_FatPHashMap.cpp`  
  Comprehensive suite (core ops, miss diagnostics, pathological erase, string heterogeneous lookup, etc.).

### Slim benchmark suite (recommended for teaching)
- `benchmark_FatPHashMap_slim.cpp`  
  “Core + Pathological + Slim MissDiag” only.

### Standalone diagnostic (microbench)
- `benchmark_EqualityComparisonsAny.cpp`  
  Focused microbench for equality comparisons / candidate checks.

### Shared benchmark utilities
- `FatPBenchmarkUtils.h`  
  Timers, CPU frequency gating / stability logic, random generators, stats.

---

## 3) Experimental variants used in the case study

### Option A
- `StableHashMap_optionA.h`
- `benchmark_FatPHashMap_optionA.cpp`

### Option B (first-empty masking optimization)
- `StableHashMap_optionB.h`
- `benchmark_FatPHashMap_optionB.cpp`

### Snapshots (“_copy”) used to preserve baselines for teaching / diffing
- `StableHashMap_copy.h`
- `benchmark_FatPHashMap_copy.cpp`
- `benchmark_EqualityComparisonsAny_copy.cpp`
- `AllocationStrategies_copy.h`
- `FatPBenchmarkUtils_copy.h`
- `FatPSimdDetection_copy.h`

---

## 4) Tests (optional, but great for the engineering narrative)
- `test_FastHashMap.h` / `test_FastHashMap.cpp`
- `test_StableHashMap.h` / `test_StableHashMap.cpp`

---

## 5) Pre-made zip bundles (pick one)

- `StableHashMap_latest_files.zip`  
  Latest “core” sources.

- `benchmark_FatPHashMap_slim_bundle.zip`  
  Slim benchmark + required headers.

- `StableHashMap_latest_and_optionB.zip`  
  Latest + Option B variant.

- `StableHashMap_all_latest_with_copies.zip`  
  Latest + snapshot “_copy” files.

- `StableHashMap_all_latest_with_copies_and_optionB.zip`  
  **FULL HISTORY + VARIANTS** (recommended teaching archive).
