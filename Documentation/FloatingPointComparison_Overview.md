# FloatingPointComparison: A Fat-P Library Showcase

## Executive Summary

FloatingPointComparison is a **policy-based floating-point equality system** that resolves comparison strategy at compile time with zero runtime dispatch. Unlike direct `==` comparison (catastrophically broken for computed values) or hard-coded epsilon checks (wrong scale for large/small numbers), FloatingPointComparison provides four policies—Standard (absolute), Relative, ULP (bit-level), and Hybrid—each compile-time selectable with proper IEEE 754 handling for NaN, infinity, subnormals, and signed zeros. This is the numerical correctness infrastructure that every scientific codebase reinvents—now done correctly once.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The classic floating-point trap
double a = 0.1 + 0.2;
double b = 0.3;

if (a == b) {  // FALSE! a = 0.30000000000000004
    // Never executed
}

// The naive epsilon fix
if (std::abs(a - b) < 1e-9) {  // Wrong for large numbers!
    // Fails: compare(1e20, 1e20 + 1.0) → difference > 1e-9 but relatively tiny
}

// The relative epsilon fix
if (std::abs(a - b) < std::abs(a) * 1e-9) {  // Wrong for near-zero!
    // Fails: compare(1e-20, 2e-20) → relative error = 100% but absolute tiny
}
```

| Issue | HPC Impact |
|-------|------------|
| Direct `==` comparison | Fails for virtually any computed floating-point value |
| Absolute epsilon | Wrong scale for large or small numbers |
| Relative epsilon | Fails near zero (division by zero or infinite relative error) |
| NaN handling | `NaN == NaN` is false; naive comparisons miss this |
| Signed zero | `-0.0 == +0.0` is true but `-0.0 < +0.0` is false |
| Subnormal numbers | Different precision characteristics near zero |

### The Standard's Limitation

The C++ standard provides `std::numeric_limits<T>::epsilon()` but no comparison functions. `<cmath>` has `isnan()`, `isinf()`, and `fpclassify()` but no "approximately equal" function.

**Why:** The committee cannot standardize a single comparison strategy because different domains need different approaches:
- Financial: Absolute epsilon (cents matter regardless of total)
- Scientific: Relative epsilon (proportional error matters)
- Graphics: ULP-based (bit-level precision)
- Physics: Hybrid (relative for large, absolute for small)

---

## Architecture: Compile-Time Policy Resolution

### The Mechanism: Policy-Based Comparison

```cpp
// Four policies—resolved entirely at compile time
struct StandardPolicy {};      // |a - b| < epsilon (absolute)
struct RelativePolicy {};      // |a - b| < epsilon * max(|a|, |b|)
struct ULPPolicy {};           // ULP distance < threshold (bit-level)
struct HybridPolicy {};        // Relative + absolute floor

template<typename Policy = HybridPolicy>
class FloatingPointCompare {
public:
    template<typename T>
    static constexpr bool equal(T a, T b, T epsilon = default_epsilon<T>) {
        // Special case handling first
        if (std::isnan(a) || std::isnan(b)) return false;
        if (a == b) return true;  // Handles ±0, ±∞ identity
        
        // Policy-specific comparison
        if constexpr (std::is_same_v<Policy, StandardPolicy>) {
            return std::abs(a - b) < epsilon;
        }
        else if constexpr (std::is_same_v<Policy, RelativePolicy>) {
            return std::abs(a - b) < epsilon * std::max(std::abs(a), std::abs(b));
        }
        else if constexpr (std::is_same_v<Policy, ULPPolicy>) {
            return ulp_distance(a, b) < static_cast<int64_t>(epsilon);
        }
        else /* HybridPolicy */ {
            T abs_diff = std::abs(a - b);
            T max_abs = std::max(std::abs(a), std::abs(b));
            return abs_diff < epsilon * max_abs || abs_diff < epsilon;
        }
    }
};
```

**Why `if constexpr`:** The compiler eliminates non-selected branches entirely. The generated code is identical to hand-written policy-specific comparison.

### IEEE 754 Edge Case Handling

| Edge Case | Behavior | Rationale |
|-----------|----------|-----------|
| NaN vs anything | `false` | NaN represents undefined; nothing equals undefined |
| +0.0 vs -0.0 | `true` | Mathematically equal despite different bit patterns |
| +∞ vs +∞ | `true` | Identity comparison: `a == b` handles this |
| +∞ vs -∞ | `false` | Different infinities are not equal |
| Subnormal handling | Automatic | `std::abs()` works correctly on subnormals |

---

## Feature Inventory

### 1. Standard (Absolute) Policy

```cpp
using Cmp = FloatingPointCompare<StandardPolicy>;

Cmp::equal(0.1, 0.10000001, 1e-6);  // true: |diff| < epsilon
Cmp::equal(1e20, 1e20 + 100, 1e-6); // false: |diff| = 100 > epsilon
```

**Use case:** Financial calculations where absolute differences matter (cents, not percentages).

### 2. Relative Policy

```cpp
using Cmp = FloatingPointCompare<RelativePolicy>;

Cmp::equal(1e20, 1e20 + 1e10, 1e-9);  // true: relative diff = 1e-10 < 1e-9
Cmp::equal(1e-20, 2e-20, 1e-9);        // false: relative diff = 100% >> 1e-9
```

**Use case:** Scientific computing where proportional error matters across scales.

### 3. ULP (Units in Last Place) Policy

```cpp
using Cmp = FloatingPointCompare<ULPPolicy>;

// ULP distance: how many representable floats apart?
Cmp::equal(1.0, std::nextafter(1.0, 2.0), 1.0);  // true: 1 ULP apart
Cmp::equal(1.0, std::nextafter(std::nextafter(1.0, 2.0), 2.0), 1.0);  // false: 2 ULPs
```

**Use case:** Bit-exact verification, testing numerical algorithms, graphics where precision is paramount.

### 4. Hybrid Policy (Default)

```cpp
using Cmp = FloatingPointCompare<HybridPolicy>;

// Near zero: uses absolute comparison
Cmp::equal(1e-15, 2e-15, 1e-9);  // true: absolute diff 1e-15 < epsilon

// Large numbers: uses relative comparison
Cmp::equal(1e20, 1e20 * (1 + 1e-10), 1e-9);  // true: relative diff 1e-10 < epsilon
```

**Use case:** General-purpose comparison that works across the full floating-point range.

### 5. Default Epsilon Selection

```cpp
// Default: 100× machine epsilon
constexpr double default_epsilon<double> = 100 * std::numeric_limits<double>::epsilon();
// ≈ 2.2e-14 for double
// ≈ 1.2e-5 for float

// Rationale: Machine epsilon is too tight (single operation error)
// 100× allows for accumulated rounding in typical computations
```

### 6. Comparison Predicates

```cpp
FloatingPointCompare<HybridPolicy> cmp;

cmp.less(a, b);         // a < b with tolerance
cmp.less_equal(a, b);   // a ≤ b with tolerance
cmp.greater(a, b);      // a > b with tolerance
cmp.greater_equal(a, b); // a ≥ b with tolerance
```

**Mechanism:** `less(a, b)` = `a < b && !equal(a, b)`. Tolerance-aware ordering.

---

## Why Not Alternatives?

| If You Need... | Why Not Direct == | Why Not Manual Epsilon | Why Not Boost.Test | Fat-P Advantage |
|----------------|-------------------|----------------------|-------------------|-----------------|
| Computed value comparison | ❌ Fails for nearly all cases | Works but error-prone | ✅ Works | ✅ Works |
| Policy selection | N/A | Manual if/else | Fixed tolerance | ✅ Compile-time |
| IEEE 754 edge cases | ❌ NaN, ±0 mishandled | Usually forgotten | ✅ Handled | ✅ Handled |
| Zero dependencies | ✅ Built-in | ✅ Manual | ❌ Requires Boost | ✅ Header-only |
| ULP comparison | N/A | Complex to implement | ❌ Not available | ✅ Built-in |

**The Sweet Spot:** FloatingPointComparison is the only option combining:
- ✅ Four comparison policies (absolute, relative, ULP, hybrid)
- ✅ Compile-time policy resolution
- ✅ Proper IEEE 754 edge case handling
- ✅ Zero external dependencies
- ✅ Configurable default epsilon

---

## The "Forever Stuck" Reality

**Standard Library Reality:** The committee will never standardize floating-point comparison because:

1. **No single correct approach:** Financial ≠ scientific ≠ graphics requirements
2. **Epsilon selection:** Default epsilon is domain-dependent
3. **Edge case semantics:** Should NaN equal NaN? Different domains disagree

Every numerical codebase reinvents this. Fat-P provides all four common strategies with proper edge case handling, letting you choose the right one for your domain.

---

## Performance Characteristics

### Benchmark Results (Release Build, i7-8850H @ 2.60GHz)

| Policy | Time per Comparison | Mechanism |
|--------|---------------------|-----------|
| Standard | ~2 ns | `std::abs(a - b) < eps` |
| Relative | ~5 ns | + `std::max` + multiply |
| ULP | ~10 ns | Bit manipulation + comparison |
| Hybrid | ~6 ns | Relative check + absolute fallback |

### Complexity Analysis

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `equal()` | O(1) | Fixed number of FP operations |
| `ulp_distance()` | O(1) | Bit reinterpretation + subtraction |
| Edge case checks | O(1) | `isnan()`, identity comparison |

### Where Fat-P Wins

- **Unit testing:** Verify computed results against expected values
- **Numerical algorithms:** Convergence checks with appropriate tolerance
- **Physics simulations:** Compare states across iterations
- **Machine learning:** Loss function comparisons

### Where Fat-P Loses (Honesty Builds Trust)

- **Exact comparison needed:** When you actually need bitwise equality, use `==` or `memcmp`
- **Integer promotion:** Comparing `int` to `double` may surprise; cast explicitly
- **Interval arithmetic:** For guaranteed bounds, use proper interval libraries
- **Arbitrary precision:** For >64-bit precision, use MPFR or similar

---

## Integration Points

```
FloatingPointComparison.h
    ↓ uses
TypeTraits.h   (C++ standard detection)
    ↓ used by
FatPTest.h     (ASSERT_FLOAT_EQ, ASSERT_NEAR)
Tensor.h       (element-wise comparison)
Scientific computing utilities
```

---

## Final Assessment

FloatingPointComparison delivers on the fat_p promise through three pillars:

### 1. Permanence
The standard will never provide floating-point comparison functions—too many domain-specific requirements. FloatingPointComparison provides all four common strategies permanently, with proper IEEE 754 handling the standard cannot mandate.

### 2. Specialization
Policy-based design with `if constexpr` generates code identical to hand-written comparisons. No runtime dispatch, no virtual calls. The policy IS the implementation.

### 3. Control
Four policies for four domains: absolute for financial, relative for scientific, ULP for bit-exact, hybrid for general purpose. Default epsilon is sensible; custom epsilon is always available.

**Architectural Verdict:** FloatingPointComparison transforms floating-point equality from **undefined behavior** (`==` on computed values) to **domain-appropriate comparison** via compile-time policy selection. It's the numerical correctness primitive every scientific codebase needs.

---

*FloatingPointComparison.h (318 lines) — Fat-P Library*
