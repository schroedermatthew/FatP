# Benchmark Results

## Start Here

**[FAT-P_Benchmark_Analysis.md](FAT-P_Benchmark_Analysis.md)** — Competitive analysis of 23 FAT-P components against 50+ implementations from Boost, Abseil, LLVM, EASTL, moodycamel, folly, entt, and the C++ standard library. Tiered rankings, cross-platform observations, honest about where FAT-P loses.

**Per-component results** are in the `Benchmark Results - {Component}.md` files. Each contains structured data from all four test platforms with competitor detection, methodology, and caveats.

## What's In This Directory

```
benchmark_results/
├── FAT-P_Benchmark_Analysis.md          # Start here — competitive analysis
├── Benchmark Results - SmallVector.md   # Per-component results (23 files)
├── Benchmark Results - FatPHashMap.md   #   All 4 platforms, all competitors,
├── Benchmark Results - ....md           #   all sizes and operations
└── logs/                                # Raw benchmark output (zips)
    ├── *.zip                            #   CI logs and local PC output
    └── ...                              #   Per-component and per-platform
```

The `logs/` directory contains raw benchmark output: local development machine captures (Intel Core Ultra 9 285K, 64 GB DDR5, Windows 11, MSVC 19.50) and full CI logs from GitHub Actions runners (GCC, Clang, MSVC). Contents change as benchmarks are re-run or new components are added.

## Test Platforms

| Platform | Compiler | CPU | Stabilization | Measured Runs |
|----------|----------|-----|---------------|---------------|
| Local PC | MSVC 19.50 | Intel Core Ultra 9 285K | Yes (< 10% variance) | 15 |
| GCC CI | GCC 14.2 | Azure (shared tenancy) | No | 20 |
| Clang CI | Clang 17.0 | Azure (shared tenancy) | No | 20 |
| MSVC CI | MSVC 19.44 | Azure (shared tenancy) | No | 20 |

**Competitor availability varies by platform.** LLVM is excluded from MSVC CI because it takes too long to build in the CI environment. Folly is excluded from the local PC because its Windows build configuration is unreliable. Both are available on GCC and Clang CI where vcpkg handles them cleanly.

## Methodology

Every FAT-P benchmark follows the same protocol:

- **Round-robin execution** with randomized library order per run — no library gets a systematic cache advantage
- **CPU frequency stabilization** before measurement (local PC only) — waits for variance < 10%
- **Median-primary reporting** — median is the primary statistic, with mean, stddev, and CI95 also reported
- **Correctness verification** after each benchmark — confirms all libraries produce identical results
- **Setup/teardown outside timed regions** — only the operation under test is measured

The full methodology is documented in [Benchmark Code Style Guide](../Read_Me/FatP_Benchmark_Code_Style_Guide.md).

## Reproducing

Build with benchmarks enabled:

```bash
cmake -B build -DFATP_BUILD_TESTS=OFF -DFATP_BUILD_BENCHMARKS=ON
cmake --build build --config Release
```

Run all benchmarks:

```bash
# Linux / macOS
./tools/run_all_benchmarks.sh

# Windows (PowerShell)
.\tools\run_all_benchmarks.ps1

# Single component
./build/release/benchmark_SmallVector
```

Competitor libraries (Boost, Abseil, LLVM, etc.) are resolved through vcpkg. The benchmarks compile and run without competitors — they just skip those columns.

## Reading the Raw Output

Each `.txt` file follows the same structure:

1. **Header** — platform, compiler, competitor detection, methodology notes
2. **Results by size** (N=100, N=1K, N=10K, N=100K, N=1M) — each operation shows median, mean, stddev, min, max in nanoseconds per operation for every competitor
3. **Diagnostics** (where applicable) — probe distances, cache miss rates, allocation counts

The numbers are nanoseconds per operation unless otherwise labeled. Lower is better.

## Caveats

- **Local PC CPU frequency instability:** The Windows test machine shows frequency variance of 30-70% during stabilization. Absolute ns/op values are approximate; relative rankings within each benchmark section are reliable.
- **CI runners are shared-tenancy:** GCC and Clang results come from Azure cloud runners with potential neighbor noise. Results are reproducible across runs but may differ from dedicated hardware.
- **Not all components are benchmarked yet.** See coverage below.

## Coverage

23 of 62 components have competitive benchmarks against external libraries. The remaining 39 were analyzed in [FAT-P_Benchmark_Gap_Analysis.md](../Plans/FAT-P_Benchmark_Gap_Analysis.md) and fall into four categories:

**High priority (pending):** CSRMatrix, Tensor, SimdVector, Signal — the HPC core and most commonly benchmarked utility patterns. These are the components most likely to drive adoption decisions and will be benchmarked next.

**Medium priority (pending):** CheckedArithmetic, IdGenerator, RateLimiter, Factory, HpcVector — worth benchmarking to validate design decisions but less likely to be the deciding factor for adoption.

**Borderline (7):** Expected, ConcurrencyPolicies, CacheUtilities, and others where benchmarking value is unclear (dominated by I/O, should be zero-cost by design, or thin competitive field).

**Not benchmarkable (23):** Compile-time constructs (Concepts, ConstexprUtilities), debug-only infrastructure (DebugOnly, EnforcedInit), tooling (FatPTest, FatPConfig), and hardware-dependent components (NumaAllocator requires NUMA hardware CI runners don't have).
