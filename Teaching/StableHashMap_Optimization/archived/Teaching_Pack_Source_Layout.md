# Teaching Pack Source Layout (StableHashMap)

This is a recommended *book-like* layout so the repo/zip reads like a case-study project: docs first, then the code that backs each chapter.

## What you get
- A clean folder structure (docs → code → benchmarks → variants → snapshots)
- Your current “latest” files + Option A/B variants + “copy” snapshots
- A place to store future benchmark output (`results/`) without mixing it into sources

## Folder map

- `README.md` — **start here**; reading order + how to navigate

- `docs/` — the teaching narrative and supporting chapters (ordered):
  - `00_*` getting started / overview
  - `01_*` main narrative
  - `02_*` the MissDiag/first-empty-mask case study
  - `03_*` “Chasing Speed” + background
  - `04_*` design history
  - `05_*` code-file index

- `src/include/` — header-only implementation building blocks:
  - `StableHashMap.h`, `FastHashMap.h`
  - `AllocationStrategies.h`, `FatPSimdDetection.h`, `FatPBenchmarkUtils.h`

- `bench/` — benchmarks you can compile/run:
  - `benchmark_FatPHashMap_slim.cpp` (core + pathological + slim MissDiag)
  - `benchmark_FatPHashMap.cpp` (full suite)
  - `benchmark_EqualityComparisonsAny.cpp`

- `variants/` — experimental forks and the matching benchmark entrypoints:
  - `StableHashMap_optionA.h`, `benchmark_FatPHashMap_optionA.cpp`
  - `StableHashMap_optionB.h`, `benchmark_FatPHashMap_optionB.cpp`

- `snapshots/` — “frozen” copies (useful for teaching diffs):
  - `*_copy.*`

- `tests/` — small correctness/regression tests:
  - `test_FastHashMap.*`, `test_StableHashMap.*`

- `results/` — empty placeholder for saving future run logs / CSVs / plots

- `archives/` — previous zips, preserved verbatim for provenance

## Why this layout works for teaching
- Readers can start in `docs/` without seeing a wall of code.
- Every performance claim in the story has a nearby benchmark file.
- Variants and snapshots make it easy to show *“what changed”* and *“why it mattered”*.

