# Source Files (Teaching Copies)

These are frozen copies of headers used by the teaching pack's benchmarks and tests. They may differ from the current versions in `include/fat_p/` because they capture the state of the code at the time the teaching materials were written.

Do not update these to match the main library — the teaching docs and benchmarks reference specific behavior and performance characteristics of these versions.

| File | Purpose |
|------|---------|
| `StableHashMap.h` | The primary subject of the teaching pack |
| `FastHashMap.h` | Comparison implementation (Swiss Table variant) |
| `AllocationStrategies.h` | Allocator policies used by both maps |
| `FatPBenchmarkUtils.h` | Benchmark harness utilities |
| `FatPSimdDetection.h` | SIMD detection used by FastHashMap |
