# FloatingPointComparison Benchmark Results

**Platforms:** Linux (GCC -O3), Windows (MSVC /O2)  
**Date:** December 2025  
**Source:** `benchmark_FloatingPointComparison.cpp`

---

## Summary

FloatingPointComparison provides complete IEEE 754 correctness at 1.2-4× the cost of unsafe manual checks. Standard policy achieves sub-2ns latency on both platforms; Hybrid policy runs at ~6.5ns with full special-value handling. MSVC optimizes Relative and ULP policies significantly better than GCC. Special value early-exit provides 3-10× speedup.

---

## Test Environment

| Property | Linux | Windows |
|----------|-------|---------|
| OS | Linux 64-bit | Windows 11 Pro (26200) |
| Compiler | GCC -O3 -DNDEBUG | MSVC 19.45 /O2 /Ob2 /GL /Oi |
| CPU | — | Intel Core Ultra 9 285K |
| CPU Base Clock | — | 3686 MHz |
| RAM | — | 64 GB DDR5 |
| C++ Standard | C++17 | C++17 |
| Ops per batch | 1,000,000 | 1,000,000 |
| Measured runs | 50 | 15 |
| Warmup runs | 3 | 3 |

**Windows Compiler Flags:**
```
/O2 /Ob2 /GL /Gy /Oi /fp:precise /DNDEBUG /std:c++17 /EHsc
```
- `/O2` — Maximum optimization
- `/Ob2` — Inline expansion
- `/GL` — Whole program optimization (link-time)
- `/Oi` — Intrinsic functions enabled

---

## Fat-P vs Manual Baseline

**What:** Overhead of Fat-P's IEEE 754 correctness vs unsafe manual checks

### Standard Policy vs Manual Absolute

| Platform | Fat-P (ns) | Manual (ns) | Ratio | Notes |
|----------|------------|-------------|-------|-------|
| Linux/GCC | 1.70 | 0.54 | 3.1× | |
| Windows/MSVC | 1.71 | 0.43 | **4.0×** | MSVC baseline faster |

### Hybrid Policy vs Manual Hybrid

| Platform | Fat-P (ns) | Manual (ns) | Ratio | Notes |
|----------|------------|-------------|-------|-------|
| Linux/GCC | 6.70 | 5.48 | 1.2× | |
| Windows/MSVC | 6.49 | 2.93 | **2.2×** | MSVC baseline faster |

**Notes:** The overhead is the cost of correctness—handling NaN, infinity, signed zero, and subnormals. MSVC produces faster manual baselines, so the ratio appears higher, but Fat-P absolute performance is nearly identical across platforms.

---

## Policy Comparison (Normal Values)

**What:** Per-operation latency for each comparison policy

| Policy | Linux/GCC (ns) | Windows/MSVC (ns) | MSVC vs GCC |
|--------|----------------|-------------------|-------------|
| Standard | 1.88 | 1.79 | 1.05× faster |
| Relative | 6.43 | **2.27** | **2.8× faster** |
| ULP | 12.70 | **7.75** | **1.6× faster** |
| Hybrid | 6.54 | 6.86 | 0.95× (similar) |

**Key Finding:** MSVC optimizes Relative and ULP policies significantly better than GCC:
- Relative: MSVC likely inlines `std::max()` more aggressively
- ULP: MSVC may use faster bit manipulation intrinsics

Hybrid performance is consistent across platforms because it's dominated by the special-value checks and absolute comparison, which both compilers handle similarly.

---

## Special Value Handling

**What:** Early-exit optimization for IEEE 754 special values

| Value | Linux/GCC (ns) | Windows/MSVC (ns) | Notes |
|-------|----------------|-------------------|-------|
| NaN | **0.67** | 1.60 | GCC 2.4× faster |
| Infinity | 2.17 | 2.13 | Similar |

| Platform | NaN Speedup | Infinity Speedup |
|----------|-------------|------------------|
| Linux/GCC | 9.8× vs Hybrid | 3.0× vs Hybrid |
| Windows/MSVC | 4.3× vs Hybrid | 3.2× vs Hybrid |

**Notes:** GCC's NaN early-exit is significantly faster, possibly due to better branch prediction or `std::isnan()` implementation. Both platforms show substantial speedup for special values.

---

## Cross-Platform Analysis

### Where GCC Wins
- **NaN detection:** 0.67 ns vs 1.60 ns (2.4× faster)
- More consistent policy performance (less variance between policies)

### Where MSVC Wins
- **Relative policy:** 2.27 ns vs 6.43 ns (2.8× faster)
- **ULP policy:** 7.75 ns vs 12.70 ns (1.6× faster)
- Faster manual baselines (better simple-case optimization)

### Platform-Independent
- **Standard policy:** ~1.8 ns on both
- **Hybrid policy:** ~6.5-6.9 ns on both
- **Infinity handling:** ~2.1 ns on both

---

## Interpretation

### The "Overhead" Is Correctness

Fat-P is **not** zero-overhead compared to unsafe manual checks:

| Policy | Linux Ratio | Windows Ratio |
|--------|-------------|---------------|
| Standard | 3.1× | 4.0× |
| Hybrid | 1.2× | 2.2× |

This overhead buys: NaN detection, infinity handling, signed-zero correctness, subnormal awareness. Manual implementations fail silently on these cases.

### Policy Selection Guidance

| Policy | Typical Latency | Use When |
|--------|-----------------|----------|
| Standard | 1.8 ns | Fastest; noise floor semantics; values known normal |
| Relative | 2-6 ns | Scale-independent; values bounded away from zero |
| Hybrid | 6-7 ns | General purpose; **recommended default** |
| ULP | 8-13 ns | Algorithm testing; bit-exact verification |

### Production Readiness

At 2-13 ns per comparison, all policies are production-ready on both platforms. A million comparisons take 2-13 ms. Choose based on **correctness requirements**, not performance.

---

## Caveats

- Linux environment details not recorded; GCC version unspecified
- MSVC used whole-program optimization (`/GL`); GCC may not have LTO enabled
- `long double` not benchmarked (ULP unsupported)
- No Boost.Math or Google Test comparison (different APIs)
- MSVC results from 15 runs per Style Guide recommendation for Windows

---

*FloatingPointComparison Benchmark Results — December 2025*
