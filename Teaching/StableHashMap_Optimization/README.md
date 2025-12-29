# StableHashMap Teaching Pack (Curated)

This folder is laid out like a small “book + code” project for engineers and students.

## Start here (docs/)

Read in this order:

1. **Companion Guide — The Hash Map Wars**
   - `docs/Companion Guide - The Hash Map Wars.md`

2. **Case Study — The Slow Miss**
   - `docs/Case Study - The Slow Miss.md`

3. **Handbook — Performance Engineering Methodology**
   - `docs/Handbook - Performance Engineering Methodology.md`

4. **Teaching Pack README / TOC**
   - `docs/README_Teaching_Pack.md`

## Code you’ll point to from the docs

- `src/include/StableHashMap.h` — “latest” StableHashMap implementation
- `variants/StableHashMap_optionA.h` — Option A (block allocator / stable-node improvements)
- `variants/StableHashMap_optionB.h` — Option B (stop tag matches after first empty)

## Benchmarks and tests

- `bench/benchmark_FatPHashMap_slim.cpp` — Core + Pathological + Slim MissDiag
- `bench/benchmark_FatPHashMap.cpp` — full benchmark harness (if you want the extras)
- `tests/` — sanity / API tests

## History / extra material

Everything that is **not** part of the current “front-of-book” docs has been moved into:

- `archived/`

That includes earlier draft narratives, older exports, bundles, and intermediate artifacts.
