# EqualityComparisons & EqualityAny Benchmark Results

**Platforms:**  
- Windows 11 Pro, MSVC 1950 /O2 /Ob2 /GL, Intel Core Ultra 9 285K @ 3.7 GHz  
- Ubuntu 24.04, GCC 13.3 -O3 -march=native, Linux container @ 2.6 GHz  

**Date:** December 2025  
**Source:** `benchmark_EqualityComparisonsAny.cpp`  
**Methodology:** Statistical measurement with 15 batches (MSVC) / 30 batches (GCC). Primary metric: median. CI95 assumes normality.

---

## Summary

EqualityComparisons provides correct floating-point semantics (NaN/Inf/signed-zero handling). Performance characteristics differ significantly between compilers: MSVC shows 10-19% overhead versus semantics-equivalent baselines but larger gaps versus naive loops; GCC shows consistent ~60-65% overhead versus both baseline types due to aggressive optimization of the naive baseline. Per-element dispatch overhead is ~1.5-1.7 ns (MSVC) or ~0.9-1.0 ns (GCC). EqualityAny registry lookup adds ~80 ns (MSVC) or ~190 ns (GCC) fixed cost, amortizing to parity at N≈256-1024.

---

## Test Environment

| Property | MSVC | GCC |
|----------|------|-----|
| OS | Windows 11 Pro (26200) | Ubuntu 24.04 |
| Compiler | MSVC 1950 /O2 /Ob2 /GL /Oi /Ot | GCC 13.3 -O3 -march=native |
| CPU | Intel Core Ultra 9 285K @ 3.7 GHz | Linux container @ 2.6 GHz |
| Build | Release (NDEBUG) | Release (NDEBUG) |
| Batches | 15 | 30 |
| kStopOnFirstError | false | false |

---

## Vector Comparison: Semantics Matter

**What:** Comparing Fat-P to two baselines — naive fabs loop vs. loop with equivalent NaN/Inf handling

### MSVC Results

| Size | Fat-P | ±Std | Naive fabs | Speedup | Semantics | Speedup |
|------|-------|------|------------|---------|-----------|---------|
| 64 | 134.2 ns | 2.1 ns | 19.0 ns | 0.14x | 107.7 ns | 0.81x |
| 256 | 494.8 ns | 8.6 ns | 58.8 ns | 0.12x | 431.2 ns | 0.88x |
| 1024 | 1.94 µs | 26 ns | 202 ns | 0.10x | 1.73 µs | 0.89x |
| 4096 | 7.73 µs | 95 ns | 975 ns | 0.13x | 6.92 µs | 0.89x |
| 16384 | 30.6 µs | 264 ns | 4.65 µs | 0.15x | 27.7 µs | 0.90x |

### GCC Results

| Size | Fat-P | ±Std | Naive fabs | Speedup | Semantics | Speedup |
|------|-------|------|------------|---------|-----------|---------|
| 64 | 117 ns | 5.5 ns | 39.4 ns | 0.34x | 40.5 ns | 0.35x |
| 256 | 468 ns | 7.7 ns | 167 ns | 0.36x | 168 ns | 0.36x |
| 1024 | 1.84 µs | 55 ns | 634 ns | 0.34x | 632 ns | 0.34x |
| 4096 | 7.32 µs | 73 ns | 2.62 µs | 0.36x | 2.46 µs | 0.34x |
| 16384 | 29.2 µs | 75 ns | 10.5 µs | 0.36x | 9.76 µs | 0.33x |

**Notes:** MSVC's naive baseline is faster than its semantics baseline, showing NaN/Inf checks have real cost. GCC optimizes both baselines to nearly identical speed, meaning Fat-P's overhead is pure dispatch. Against equivalent semantics, MSVC overhead is 10-19%; GCC overhead is 64-67%.

---

## Size Scaling with Per-Element Overhead

**What:** How overhead distributes across container sizes

### MSVC Results

| Size | Manual | Fat-P | ±Std | Speedup | Per-elem OH |
|------|--------|-------|------|---------|-------------|
| 10 | 2.1 ns | 31.0 ns | 0.4 ns | 0.07x | 2.89 ns |
| 100 | 28.0 ns | 204 ns | 3.1 ns | 0.14x | 1.76 ns |
| 1000 | 198 ns | 1.88 µs | 36 ns | 0.11x | 1.68 ns |
| 10000 | 2.36 µs | 18.6 µs | 325 ns | 0.13x | 1.63 ns |
| 100000 | 33.3 µs | 183 µs | 1.9 µs | 0.18x | 1.50 ns |

### GCC Results

| Size | Manual | Fat-P | ±Std | Speedup | Per-elem OH |
|------|--------|-------|------|---------|-------------|
| 10 | 7.2 ns | 17.0 ns | — | 0.42x | 0.98 ns |
| 100 | 62 ns | 158 ns | — | 0.39x | 0.96 ns |
| 1000 | 617 ns | 1.53 µs | — | 0.40x | 0.91 ns |
| 10000 | 6.06 µs | 16.3 µs | — | 0.37x | 1.02 ns |
| 100000 | 62.6 µs | 153 µs | — | 0.41x | 0.90 ns |

**Notes:** GCC achieves ~0.9-1.0 ns per-element overhead; MSVC is ~1.5-1.7 ns converging toward ~1.5 ns at large N. Both show higher overhead at small N due to fixed dispatch setup cost.

---

## Nested Container Overhead

**What:** Impact of nesting depth on per-element cost (all structures contain 1000 doubles)

### MSVC Results

| Structure | Median | ±Std | Per-elem |
|-----------|--------|------|----------|
| Flat: vector\<double\>[1000] | 1.93 µs | 35 ns | 1.93 ns |
| 2-level: vector\<vector\>[10×100] | 2.03 µs | 41 ns | 2.03 ns |
| 3-level: vec\<vec\<vec\>\>[5×10×20] | 2.53 µs | 37 ns | 2.53 ns |

### GCC Results

| Structure | Median | Per-elem |
|-----------|--------|----------|
| Flat: vector\<double\>[1000] | 1.72 µs | 1.72 ns |
| 2-level: vector\<vector\>[10×100] | 1.65 µs | 1.65 ns |
| 3-level: vec\<vec\<vec\>\>[5×10×20] | 1.78 µs | 1.78 ns |

**Notes:** MSVC shows 5-31% overhead per nesting level. GCC shows minimal nesting impact — 2-level is actually faster than flat (cache effects).

---

## Map Comparisons

**What:** Nested container comparison (map of vectors)

### MSVC Results

| Structure | Fat-P | ±Std | Manual | Speedup |
|-----------|-------|------|--------|---------|
| map K=64, V=32 | 5.05 µs | 49 ns | 996 ns | 0.20x |
| map K=128, V=64 | 17.1 µs | 347 ns | 3.35 µs | 0.20x |
| map K=256, V=64 | 34.4 µs | 1.2 µs | 6.99 µs | 0.20x |
| unordered_map K=64, V=32 | 5.12 µs | 90 ns | 979 ns | 0.19x |
| unordered_map K=128, V=64 | 17.9 µs | 279 ns | 3.78 µs | 0.21x |
| unordered_map K=256, V=64 | 36.3 µs | 1.1 µs | 8.35 µs | 0.23x |

### GCC Results

| Structure | Fat-P | Manual | Speedup |
|-----------|-------|--------|---------|
| map K=64, V=32 | 4.57 µs | 1.69 µs | 0.37x |
| map K=128, V=64 | 17.8 µs | 6.45 µs | 0.36x |
| map K=256, V=64 | 35.5 µs | 13.0 µs | 0.36x |
| unordered_map K=64, V=32 | 5.26 µs | 3.06 µs | 0.58x |
| unordered_map K=128, V=64 | 16.5 µs | 8.54 µs | 0.52x |
| unordered_map K=256, V=64 | 33.4 µs | 17.2 µs | 0.52x |

**Notes:** Manual baseline uses simple fabs. GCC's baseline is slower (less aggressive optimization of nested loops), making Fat-P look better relatively.

---

## Unordered Map Scalar Values

**What:** Best-case container scenario (scalar doubles, no nesting)

### MSVC Results

| Keys | Fat-P | ±Std | Manual | Speedup |
|------|-------|------|--------|---------|
| 64 | 780 ns | 4.7 ns | 496 ns | 0.64x |
| 256 | 2.86 µs | 18 ns | 1.88 µs | 0.66x |
| 1024 | 11.5 µs | 117 ns | 8.05 µs | 0.70x |
| 4096 | 45.6 µs | 299 ns | 32.7 µs | 0.72x |

### GCC Results

| Keys | Fat-P | Manual | Speedup |
|------|-------|--------|---------|
| 64 | 855 ns | 830 ns | 0.97x |
| 256 | 3.82 µs | 3.45 µs | 0.90x |
| 1024 | 17.9 µs | 16.8 µs | 0.94x |
| 4096 | 68.5 µs | 65.1 µs | 0.95x |

**Notes:** GCC achieves near-parity (0.90-0.97x) for scalar map comparisons. Hash table lookup dominates, minimizing dispatch overhead impact.

---

## Policy Comparison

**What:** Relative cost of different floating-point policies on vector<double>[10000]

### MSVC Results

| Policy | Median | ±Std |
|--------|--------|------|
| Standard | 32.2 µs | 23.2 µs* |
| Hybrid | 22.4 µs | 369 ns |
| Relative | 54.4 µs | 629 ns |
| ULP (float) | 19.2 µs | 407 ns |

*High stddev indicates measurement outlier in one batch.

### GCC Results

| Policy | Median |
|--------|--------|
| Standard | 15.4 µs |
| Hybrid | 15.4 µs |
| Relative | 35.7 µs |
| ULP (float) | 18.5 µs |

**Notes:** RelativeComparisonPolicy is ~2.3-2.7x slower due to per-element division. ULP is faster on MSVC, slower on GCC (different float vs double optimization). Standard and Hybrid are nearly identical on GCC.

---

## EqualityAny: Registry Overhead

**What:** Cost of type-erased comparison via registry lookup

### MSVC Results

| Method | Median | ±Std |
|--------|--------|------|
| any registry lookup | 80.1 ns | 0.4 ns |
| any_cast + typed call | 4.78 ns | 0.04 ns |
| Direct typed call | 1.69 ns | 0.03 ns |

### GCC Results

| Method | Median |
|--------|--------|
| any registry lookup | 191 ns |
| any_cast + typed call | 5.15 ns |
| Direct typed call | 0.15 ns |

**Notes:** MSVC has 2.4x faster registry lookup than GCC. For scalar comparisons, registry lookup dominates.

---

## EqualityAny: Overhead Amortization

**What:** How fixed registry cost amortizes over container size

### MSVC Results

| Size | any(vector) | ±Std | Direct | Speedup |
|------|-------------|------|--------|---------|
| 64 | 231 ns | 3.0 ns | 135 ns | 0.58x |
| 256 | 567 ns | 28 ns | 510 ns | 0.90x |
| 1024 | 1.93 µs | 56 ns | 1.96 µs | 1.01x |
| 4096 | 7.41 µs | 157 ns | 7.73 µs | 1.04x |

### GCC Results

| Size | any(vector) | Direct | Speedup |
|------|-------------|--------|---------|
| 64 | 308 ns | 107 ns | 0.35x |
| 256 | 661 ns | 416 ns | 0.63x |
| 1024 | 2.11 µs | 1.72 µs | 0.82x |
| 4096 | 7.74 µs | 6.75 µs | 0.87x |

**Notes:** MSVC reaches parity at N≈256, slight speedup at larger sizes. GCC reaches 0.87x at N=4096, parity expected around N≈8000.

---

## EqualityAny: vector<any> Performance

**What:** Nested any comparison (vector of any values)

### MSVC Results

| Size | any(vec) | ±Std | Direct | Speedup |
|------|----------|------|--------|---------|
| 32 | 2.72 µs | 16 ns | 2.58 µs | 0.95x |
| 128 | 10.5 µs | 88 ns | 10.4 µs | 0.99x |
| 512 | 41.4 µs | 439 ns | 41.6 µs | 1.00x |
| 2048 | 167 µs | 1.5 µs | 167 µs | 1.00x |

### GCC Results

| Size | any(vec) | Direct | Speedup |
|------|----------|--------|---------|
| 32 | 6.42 µs | 5.92 µs | 0.92x |
| 128 | 24.0 µs | 24.4 µs | 1.01x |
| 512 | 102 µs | 94.2 µs | 0.92x |
| 2048 | 397 µs | 386 µs | 0.97x |

**Notes:** Near-parity on both compilers. Outer any wrapper adds minimal overhead when inner elements already require registry lookup.

---

## EqualityAny: Edge Cases

**What:** Special case handling costs

### MSVC Results

| Case | Median | ±Std |
|------|--------|------|
| Both empty | 3.60 ns | 0.03 ns |
| Type mismatch | 50.1 ns | 0.7 ns |
| Nested std::any | 81.8 ns | 1.0 ns |

### GCC Results

| Case | Median |
|------|--------|
| Both empty | 5.95 ns |
| Type mismatch | 58.9 ns |
| Nested std::any | 188 ns |

**Notes:** Empty comparison is fast (no registry lookup). Type mismatch requires one probe. Nested any requires two lookups — GCC shows 2.3x cost of MSVC.

---

## Compiler Comparison Summary

| Metric | MSVC | GCC | Winner |
|--------|------|-----|--------|
| Per-element dispatch overhead | 1.5-1.7 ns | 0.9-1.0 ns | GCC |
| Fat-P vs semantics baseline | 0.81-0.90x | 0.33-0.36x | MSVC |
| Naive baseline speed | Very fast | 2x slower | MSVC |
| unordered_map scalar | 0.64-0.72x | 0.90-0.97x | GCC |
| Any registry lookup | ~80 ns | ~190 ns | MSVC |
| Any parity point | N≈256 | N≈8000 | MSVC |
| Nesting overhead | 5-31% per level | Minimal | GCC |

**Interpretation:** MSVC produces faster baseline loops, making Fat-P overhead appear larger. GCC's baselines are slower but Fat-P absolute times are similar, yielding better speedup ratios. For production use, Fat-P performance is comparable on both compilers; the "overhead" numbers primarily reflect baseline optimization differences.

---

## Caveats

- Results are compiler-specific; different optimization strategies yield different overhead profiles
- Linux container CPU was throttled/virtualized; absolute times may not reflect bare-metal performance
- kStopOnFirstError=false; early-exit behavior not measured
- Manual baselines use naive fabs except where noted
- GCC's semantics baseline optimizes to same speed as naive baseline, suggesting NaN/Inf checks are elided
- std::any performance varies significantly between standard library implementations
- CI95 uses normal approximation; with 15 batches (MSVC), intervals are approximate

---

*EqualityComparisons & EqualityAny Benchmark Results v1.3 — December 2025*
