# TensorRanked implementation baseline

Date: 2026-09-04. Local pre-publication evidence from the uncommitted Phase 12
working tree based on revision `8bddc68f7be2`.

## Protocol

Hardware: Intel Core Ultra 9 285K, Windows x64, Balanced power plan. The run
reported a 3686 MHz registry reference and approximately 2359 MHz at capture
(36% below that reference). No affinity or priority override was used, CPU
stabilization and cooldown were disabled, and concurrent local review work was
active. These measurements are implementation evidence, not portable speed or
ABI promises.

Compiler: Clang 22.1, C++20. Build command:

```powershell
clang++ -std=c++20 -O3 -DNDEBUG -march=native -iquote include/fat_p \
  -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion -Werror \
  components/Tensor/benchmarks/benchmark_TensorRanked.cpp \
  -o benchmark_TensorRanked.exe
```

Configuration: two warmups, nine measured batches, seed 12345, 10 ms requested
minimum batch, verbose statistics, and no scope/stabilization/cooldown. Each
benchmark callback performs 2,000 owner operations, adaptations, or 16-element
kernel operations as documented by the benchmark source. JSON retains all nine
raw ns/op samples in measurement order; CSV and console preserve the aggregate
statistics and environment.

Files:

- [Clang JSON](clang.json) — configuration, dispersion, and raw samples.
- [Clang CSV](clang.csv) — tabular aggregate statistics.

## Recorded observations

Object sizes in bytes for rank 0 through 8 were: extents
16/16/24/32/40/48/56/64/72, layout 72/80/96/112/128/144/160/176/192,
borrowed view 104/112/128/144/160/176/192/208/224, and owner
112/120/136/152/168/184/200/216/232. The dynamic objects were 56, 160, 192,
and 200 bytes respectively. These are compiler-specific observations.

Selected medians (ns/op; standard deviations in parentheses):

| Case | Ranked | Dynamic | Static |
|---|---:|---:|---:|
| Construct 4x4 | 95.10 (49.61) | 132.30 (13.45) | 1.20 (0.03) |
| Copy 4x4 | 92.05 (0.79) | 93.95 (1.46) | 1.20 (0.00) |
| Index | 3.17 (0.11) | 3.83 (0.08) | 0.37 (0.00) |
| Add 4x4 | 148.60 (0.83) | 282.90 (46.00) | 1.20 (0.03) |

Adapter medians were 40.05 ns ranked-2 to dynamic, 15.45 ns dynamic-2 to
ranked, 118.50 ns ranked-8 to dynamic, and 28.00 ns dynamic-8 to ranked. The
rank-8 ranked-to-dynamic path includes the documented dynamic metadata fallback
allocation. The data support the intended metadata/traversal design on this
build but do not establish general performance superiority.

## Source identities

| File | SHA-256 |
|---|---|
| `benchmark_TensorRanked.cpp` | `AFFEB80CA3A2D299286968D8DC7F6578CA1D0329491B97F2B03269A25058B0D2` |
| `tensor/TensorRanked.h` | `DA67D558FDAD7C3BCE16EE81E5E90C586CE14F2FAD352AC23198F7C1846B1DB3` |
| `tensor/TensorIterationPlan.h` | `BD15E02EDB81CFEBA68FE269F53C73D656FF6B6D2F16DEC49EE47656BCC4F620` |
