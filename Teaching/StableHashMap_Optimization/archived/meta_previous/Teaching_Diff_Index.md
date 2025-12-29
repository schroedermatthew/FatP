# Teaching Diff Index (what to compare)

The most teachable diffs in this project:

## 1) Baseline vs Option B (first-empty mask)
- Compare:
  - `src/include/StableHashMap.h`
  - `variants/StableHashMap_optionB.h`

Focus questions:
- Why is it safe to ignore tag matches after the first empty?
- What does that do to **Tag/miss**, **Eq/miss**, and miss latency?

## 2) Benchmark harness evolution
- Compare:
  - `bench/benchmark_FatPHashMap_full.cpp`
  - `bench/benchmark_FatPHashMap_slim.cpp`

Focus questions:
- What sources of noise are you controlling?
- Why is a slimmer benchmark sometimes a *better* benchmark?

## 3) Snapshots vs current
- `snapshots/*` vs their equivalents in `src/include/` and `bench/`

Focus questions:
- What changed?
- Did it improve the metric you cared about *without* breaking invariants?
