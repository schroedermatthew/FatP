# Building (minimal guidance)

This project is deliberately not prescriptive about build tooling.

## Visual Studio (quick)

1. Create an empty Console App.
2. Add these files:
   - `bench/benchmark_FatPHashMap_slim.cpp`
   - everything in `src/include/` (as headers)
3. Ensure `/std:c++17` (or newer).
4. Enable AVX2 if you want the AVX2 backend.

## clang / gcc (quick)

Example (adjust include paths):

```bash
c++ -O3 -std=c++20 -I./src/include bench/benchmark_FatPHashMap_slim.cpp -o bench_slim
```

Competitor libraries (absl/boost/tsl/ankerl/llvm DenseMap) are optional; if missing, you can disable them in the benchmark.
