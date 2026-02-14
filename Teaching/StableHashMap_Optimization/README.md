# StableHashMap Teaching Pack

This folder is laid out as a self-contained "book + code" project for engineers and students interested in hash map design, benchmarking methodology, and performance engineering.

## Start here (docs/)

1. **Companion Guide — The Hash Map Wars**
   `docs/Companion Guide - The Hash Map Wars.md`
   Design philosophy, failed experiments, and architectural decisions behind both FastHashMap and StableHashMap.

2. **Case Study — The Slow Miss**
   `docs/Case Study - The Slow Miss.md`
   The miss-path bug investigation — how a 3.6x regression was diagnosed and what it taught us about benchmarking honestly.

For broader performance engineering methodology beyond hash maps, see the project-wide handbook:
`../Handbook - Performance Engineering Methodology.md`

## Code

- `src/include/` — Frozen copies of the headers at a teaching-relevant point in development (see `src/include/README.md`)
- `variants/StableHashMap_optionA.h` — Option A: block allocator with stable-node improvements
- `variants/StableHashMap_optionB.h` — Option B: stop tag matches after first empty

## Benchmarks and tests

- `bench/benchmark_FatPHashMap_slim.cpp` — Core + Pathological + Slim MissDiag
- `bench/benchmark_FatPHashMap_full.cpp` — Full benchmark harness
- `tests/` — API and correctness tests

## Archived material

`archived/` contains earlier drafts, old exports, intermediate artifacts, and prior benchmark logs. The current canonical docs are in `docs/`.
