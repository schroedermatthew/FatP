# FloatingPointComparison.h User Manual

## Table of Contents
1. [Overview](#overview)
2. [Why This Library Exists](#why-this-library-exists)
3. [Quick Start Guide](#quick-start-guide)
4. [The Floating-Point Comparison Problem](#the-floating-point-comparison-problem)
5. [Comparison Policies Explained](#comparison-policies-explained)
6. [API Reference](#api-reference)
7. [Usage Examples](#usage-examples)
8. [Best Practices](#best-practices)
9. [Advanced Topics](#advanced-topics)
10. [Common Pitfalls](#common-pitfalls)

---

## Overview

**FloatingPointComparison.h** is a header-only C++17 library providing robust, policy-based floating-point comparison utilities. It solves the fundamental problem that direct equality comparison (`==`) is unreliable for floating-point numbers due to representation errors, rounding, and accumulated computational drift.

### Key Features
- **Four comparison policies** for different use cases and scales
- **Type-safe** with compile-time policy selection
- **Zero dependencies** (header-only, uses only standard library)
- **Special value handling** (NaN, infinity, subnormals)
- **Sign consistency enforcement** (prevents -ε == +ε)
- **Diagnostic support** via conditional error logging
- **C++20 ready** with `std::bit_cast` support for ULP comparisons

### Philosophy
The library follows a safety-first design philosophy:
- Explicit comparison policies make behavior predictable
- No silent failures or surprising edge cases
- Special values are handled consistently across all policies
- Sign mismatches are never considered equal (except ±0.0)

---

## Why This Library Exists

### The Fundamental Problem

Floating-point numbers cannot exactly represent most decimal values. This leads to surprising behavior:

```cpp
// Classic example: Why does this fail?
double a = 0.1 + 0.2;
double b = 0.3;
assert(a == b);  // FAILS! a ≈ 0.30000000000000004
```

The problem stems from binary representation limitations:
- Decimal 0.1 has no exact binary representation (like 1/3 in decimal)
- Arithmetic operations compound rounding errors
- Different computation orders yield different results
- Values at different scales need different comparison strategies

### Real-World Consequences

Direct equality comparison leads to:
1. **Test brittleness**: Unit tests fail on legitimate implementations
2. **Algorithm failures**: Iterative methods never converge
3. **Data structure corruption**: Hash maps and sets behave incorrectly
4. **Silent bugs**: Conditional logic makes wrong decisions

### Why Multiple Policies?

There is no one-size-fits-all solution because floating-point values span vastly different contexts:

| Use Case | Challenge | Solution |
|----------|-----------|----------|
| Near-zero calculations | Small absolute differences matter | Absolute tolerance |
| Multi-scale data (1e-10 to 1e10) | Same epsilon fails everywhere | Relative tolerance |
| Bit-exact algorithm testing | Need exact representation match | ULP comparison |
| Production robustness | Must work across all scales | Hybrid approach |

**This library provides the right tool for each job.**

---

## Quick Start Guide

### Basic Usage (Recommended)

For most cases, use `approximateEqual()` with sensible defaults:

```cpp
#include "FloatingPointComparison.h"
using namespace fat_p;

// Simple comparison (uses HybridComparisonPolicy)
double x = 1.0;
double y = 1.0 + 1e-10;
bool equal = approximateEqual(x, y);  // true
```

### Custom Tolerances

```cpp
// Specify custom relative and absolute tolerances
double a = 1000.0;
double b = 1000.1;
bool equal = approximateEqual(a, b, 1e-3, 0.2);  // true
//                                   ^^^^^  ^^^
//                                   rel    abs
```

### Policy Selection

```cpp
// Use absolute tolerance only
bool eq1 = floatEqual(1.0, 1.00001, 1e-4);  // StandardComparisonPolicy (default)

// Use ULP (bit-exact) comparison
bool eq2 = floatEqual<double, UlpComparisonPolicy>(1.0, nextafter(1.0, 2.0), 1.0);

// Use relative tolerance
bool eq3 = floatEqual<double, RelativeComparisonPolicy>(1e6, 1e6 + 0.1, 1e-5);

// Use hybrid (recommended for robust code)
bool eq4 = floatEqual<double, HybridComparisonPolicy>(x, y, 1e-5, 1e-8);
```

---

## The Floating-Point Comparison Problem

### Why Equality Fails

Floating-point numbers use a finite binary representation based on the IEEE 754 standard:

```
sign × mantissa × 2^exponent
```

This representation has fundamental limitations:

#### 1. Representation Error
Many decimal values have no exact binary representation:

```cpp
float x = 0.1f;  // Actually stores: 0.100000001490116119384765625
double y = 0.1;  // Actually stores: 0.1000000000000000055511151231257827...
```

#### 2. Rounding Error
Every arithmetic operation may introduce rounding:

```cpp
double sum = 0.0;
for (int i = 0; i < 10; ++i) {
    sum += 0.1;  // Accumulates rounding error
}
// sum ≈ 0.9999999999999999, not exactly 1.0
```

#### 3. Catastrophic Cancellation
Subtracting nearly equal values loses precision:

```cpp
double a = 1.0 + 1e-15;
double b = 1.0;
double diff = a - b;  // Expected: 1e-15, Actual: varies by implementation
```

#### 4. Scale-Dependent Precision
The same epsilon fails at different scales:

```cpp
// Near zero: 1e-10 is significant
double small_a = 1e-10;
double small_b = 2e-10;
assert(abs(small_a - small_b) <= 1e-10);  // PASSES (probably wrong!)

// At large scale: 1e-10 is noise
double large_a = 1e10;
double large_b = 1e10 + 1e-10;
assert(abs(large_a - large_b) <= 1e-10);  // PASSES (correct, within precision)
```

### Special Values Complicate Matters

IEEE 754 defines special values requiring careful handling:

```cpp
// NaN (Not a Number) - result of undefined operations
double nan = 0.0 / 0.0;
assert(nan == nan);  // ALWAYS FALSE (NaN != NaN by definition)

// Infinity - result of overflow
double inf = 1.0 / 0.0;
assert(inf == inf);  // true (same-sign infinities are equal)
assert(inf == -inf);  // false (opposite-sign infinities differ)

// Signed zero - positive and negative zero
double pos_zero = +0.0;
double neg_zero = -0.0;
assert(pos_zero == neg_zero);  // true (IEEE 754 defines -0 == +0)
```

---

## Comparison Policies Explained

Each policy solves a specific class of comparison problems.

### StandardComparisonPolicy (Absolute Tolerance)

**Problem Solved:** Simple comparisons where values are at a known, consistent scale.

**Formula:**
```
|a - b| ≤ epsilon
```

**When to Use:**
- Values near a known reference point (e.g., all near zero, or all near 1.0)
- Simple unit tests with controlled inputs
- When you need predictable, easy-to-understand behavior

**Strengths:**
- Simple and intuitive
- Works well near zero
- Predictable behavior

**Weaknesses:**
- Fails across different scales (epsilon too large or too small)
- Not suitable for values spanning orders of magnitude

**Example:**

```cpp
// Good use case: Values at similar scale
double temperature1 = 98.6;
double temperature2 = 98.7;
bool same = floatEqual(temperature1, temperature2, 0.2);  // true

// Bad use case: Different scales
double tiny = 1e-10;
double large = 1e10;
bool same2 = floatEqual(tiny, tiny + 1e-5, 1e-4);  // false (too strict)
bool same3 = floatEqual(large, large + 1e5, 1e-4);  // true (too loose!)
```

**Default Epsilon Values:**
- `float`: `1e-5f` (100 × `FLT_EPSILON`)
- `double`: `1e-9` (100 × `DBL_EPSILON`)  
- `long double`: `100 × LDBL_EPSILON`

---

### RelativeComparisonPolicy (Scale-Independent)

**Problem Solved:** Comparing values that span many orders of magnitude, but are bounded away from zero.

**Formula:**
```
|a - b| ≤ epsilon × max(|a|, |b|)
```

**When to Use:**
- Scientific calculations with large dynamic range
- Financial calculations (percentages, not absolute amounts)
- When values may be very large or very small (but not zero)

**Strengths:**
- Scale-independent (works for 1e-10 and 1e10 equally well)
- Natural for percentage-based comparisons
- Adapts automatically to value magnitude

**Weaknesses:**
- **Breaks down near zero** (relative error becomes meaningless)
- Can give surprising results for mixed near-zero/large comparisons

**Example:**

```cpp
// Good use case: Large values
double price1 = 1e6;      // $1 million
double price2 = 1e6 + 100; // $1 million + $100
bool same = floatEqual<double, RelativeComparisonPolicy>(price1, price2, 1e-3);
// true (within 0.1% relative tolerance)

// Bad use case: Near zero
double nearly_zero_a = 1e-15;
double nearly_zero_b = 2e-15;
bool same2 = floatEqual<double, RelativeComparisonPolicy>(nearly_zero_a, nearly_zero_b, 0.5);
// false! Relative difference is 100%, even though absolute difference is tiny
// ⚠️ Use HybridComparisonPolicy instead!
```

**Critical Warning:**
```cpp
// Near zero, relative comparison fails catastrophically
double a = 0.0;
double b = 1e-100;
// max(|a|, |b|) = 1e-100
// Tolerance = epsilon × 1e-100 (ridiculously small!)
// Result: Will almost always fail for any reasonable epsilon
```

---

### UlpComparisonPolicy (Bit-Exact)

**Problem Solved:** Verifying numerical algorithm correctness with exact bit-level precision requirements.

**Formula:**
```
Distance in binary representation ≤ maxUlps
```

**ULP (Unit in Last Place):** The spacing between adjacent floating-point numbers in their binary representation.

**When to Use:**
- Testing mathematical libraries (sin, cos, sqrt)
- Verifying compiler optimization correctness
- When you need to know if values are bit-for-bit identical (within tolerance)
- Cross-platform numerical reproducibility testing

**Strengths:**
- Directly measures representation distance
- Scale-independent (like relative, but works better near zero for normalized values)
- Deterministic and reproducible
- Exposes actual floating-point behavior

**Weaknesses:**
- **Only supports `float` (32-bit) and `double` (64-bit)** - not `long double`
- Complex behavior near zero and in subnormal range
- Requires understanding of floating-point representation
- Default 4 ULPs may be too strict for many applications

**Technical Details:**

ULP distance measures how many representable floating-point values lie between two numbers:

```cpp
double x = 1.0;
double next = std::nextafter(x, 2.0);  // Next representable value after 1.0
// ULP distance between x and next = 1

double x = 1.0;
double y = 1.0 + DBL_EPSILON;  // Roughly 1 ULP away
bool eq = floatEqual<double, UlpComparisonPolicy>(x, y, 1.0);  // true (within 1 ULP)
```

**Subnormal Handling:**

The library uses hybrid tolerance for subnormal numbers (very close to zero):
- Float subnormals (< ~1.4e-45): absolute tolerance of 1e-6
- Double subnormals (< ~5e-324): absolute tolerance of 1e-12

This prevents ULP comparison from breaking down where exponent bits are fixed.

**Example:**

```cpp
// Good use case: Algorithm verification
double computed = my_sqrt(2.0);
double expected = 1.4142135623730951;  // Correct to many digits
bool accurate = floatEqual<double, UlpComparisonPolicy>(computed, expected, 4.0);
// Ensures computed value is within 4 ULPs of mathematically correct result

// Detecting subtle bugs
double result1 = (a + b) + c;
double result2 = a + (b + c);
// These may differ by a few ULPs due to rounding order
bool same = floatEqual<double, UlpComparisonPolicy>(result1, result2, 2.0);
```

**Default ULP Tolerance:** 4 ULPs (conservative for most numerical algorithms)

---

### HybridComparisonPolicy (Recommended)

**Problem Solved:** Robust comparison that works correctly across all scales, from near-zero to very large values.

**Formula:**
```
PASS if either condition is met:
1. |a - b| ≤ absEps  (absolute tolerance)
   OR
2. |a - b| ≤ relEps × max(|a|, |b|)  (relative tolerance)
```

**When to Use:**
- **Production code (RECOMMENDED)**
- When input ranges are unknown or dynamic
- When you need robustness without thinking about scales
- General-purpose approximate equality

**Strengths:**
- Handles near-zero values (via absolute tolerance)
- Handles large values (via relative tolerance)
- Most forgiving and robust policy
- Combines benefits of Standard and Relative policies

**Weaknesses:**
- Requires understanding two tolerance parameters
- May be "too forgiving" if strictness is required
- Slightly more complex to reason about than single-tolerance policies

**Example:**

```cpp
// Near zero: absolute tolerance dominates
double tiny_a = 1e-10;
double tiny_b = 2e-10;
bool eq1 = floatEqual<double, HybridComparisonPolicy>(
    tiny_a, tiny_b,
    1e-5,    // relEps (relative tolerance)
    1e-8     // absEps (absolute tolerance)
);
// Check 1: |1e-10 - 2e-10| = 1e-10 ≤ 1e-8? YES → PASS

// Large values: relative tolerance dominates
double large_a = 1e6;
double large_b = 1e6 + 0.01;
bool eq2 = floatEqual<double, HybridComparisonPolicy>(
    large_a, large_b,
    1e-5,    // relEps
    1e-8     // absEps
);
// Check 1: |0.01| ≤ 1e-8? NO
// Check 2: |0.01| ≤ 1e-5 × 1e6 = 10? YES → PASS

// Mixed scales: handles both
double vals[] = {1e-9, 1.0, 1e9};
// Hybrid policy works correctly for all pairwise comparisons
```

**This is the policy used by `approximateEqual()`** - the recommended high-level function.

---

## API Reference

### Type Requirements

All functions require floating-point types:
```cpp
static_assert(std::is_floating_point_v<T>);
// Valid: float, double, long double
// Invalid: int, custom types, etc.
```

### Primary Functions

#### `approximateEqual()` (Recommended)

```cpp
template <typename T>
bool approximateEqual(
    const T& a, 
    const T& b,
    T relEps = getDefaultEpsilon<T>(),  // Relative tolerance
    T absEps = getDefaultEpsilon<T>()   // Absolute tolerance
);
```

**Description:** High-level interface using `HybridComparisonPolicy` with sensible defaults.

**Parameters:**
- `a`, `b`: Values to compare
- `relEps`: Relative tolerance (default: type-specific epsilon)
- `absEps`: Absolute tolerance (default: same as `relEps`)

**Returns:** `true` if values are approximately equal

**Example:**
```cpp
// Use defaults
bool eq1 = approximateEqual(1.0, 1.0 + 1e-10);

// Custom tolerances
bool eq2 = approximateEqual(1.0, 1.001, 1e-2, 1e-3);
```

---

#### `floatEqual()` (Policy-Based)

```cpp
template <typename T, typename Policy = StandardComparisonPolicy, typename... EpsParams>
bool floatEqual(const T& a, const T& b, EpsParams... eps);
```

**Description:** Flexible comparison function with explicit policy selection.

**Template Parameters:**
- `T`: Floating-point type (float, double, long double)
- `Policy`: Comparison policy (default: `StandardComparisonPolicy`)
- `EpsParams`: Policy-specific tolerance parameter(s)

**Parameters:**
- `a`, `b`: Values to compare
- `eps`: Tolerance value(s) (policy-dependent, uses defaults if omitted)

**Returns:** `true` if values are equal according to the policy

**Policy-Specific Parameters:**

| Policy | Parameters | Defaults |
|--------|------------|----------|
| `StandardComparisonPolicy` | `epsilon` | Type-specific epsilon |
| `RelativeComparisonPolicy` | `epsilon` | Type-specific epsilon |
| `UlpComparisonPolicy` | `maxUlps` | 4.0 |
| `HybridComparisonPolicy` | `relEps, absEps` | Type-specific epsilon for both |

**Examples:**
```cpp
// Standard (absolute) with default epsilon
bool eq1 = floatEqual(1.0, 1.0001);

// Standard with custom epsilon
bool eq2 = floatEqual(1.0, 1.1, 0.2);

// Relative policy with custom epsilon
bool eq3 = floatEqual<double, RelativeComparisonPolicy>(1e6, 1e6 + 100, 1e-4);

// ULP policy with custom max ULPs
bool eq4 = floatEqual<float, UlpComparisonPolicy>(1.0f, nextafter(1.0f, 2.0f), 2.0f);

// Hybrid with custom tolerances
bool eq5 = floatEqual<double, HybridComparisonPolicy>(0.1, 0.1001, 1e-3, 1e-4);
```

---

### Helper Functions

#### `getDefaultEpsilon<T>()`

```cpp
template <typename T>
constexpr T getDefaultEpsilon();
```

**Description:** Returns the type-appropriate default epsilon value.

**Returns:**
- `float`: `1e-5f`
- `double`: `1e-9`
- `long double`: `100 × LDBL_EPSILON`

**Example:**
```cpp
double eps = getDefaultEpsilon<double>();  // 1e-9
```

---

## Usage Examples

### Example 1: Unit Testing

```cpp
#include "FloatingPointComparison.h"
#include <cmath>

// Testing a mathematical function
double my_sqrt(double x) {
    // Some sqrt implementation
    return std::sqrt(x);  // Simplified
}

void test_sqrt() {
    using namespace fat_p;
    
    // Use absolute tolerance for values near 1
    assert(floatEqual(my_sqrt(4.0), 2.0, 1e-10));
    
    // Use relative tolerance for large values
    double large = 1e10;
    assert(floatEqual<double, RelativeComparisonPolicy>(
        my_sqrt(large * large), large, 1e-8));
    
    // Use ULP for bit-exact verification
    assert(floatEqual<double, UlpComparisonPolicy>(
        my_sqrt(2.0), 1.4142135623730951, 4.0));
}
```

### Example 2: Iterative Algorithm Convergence

```cpp
#include "FloatingPointComparison.h"

double newton_raphson_sqrt(double x) {
    using namespace fat_p;
    
    double guess = x / 2.0;
    double prev_guess;
    
    do {
        prev_guess = guess;
        guess = (guess + x / guess) / 2.0;
        
        // Use hybrid comparison for robust convergence check
    } while (!approximateEqual(guess, prev_guess, 1e-10, 1e-15));
    
    return guess;
}
```

### Example 3: Comparing Arrays

```cpp
#include "FloatingPointComparison.h"
#include <vector>
#include <algorithm>

template <typename T>
bool arrays_equal(const std::vector<T>& a, const std::vector<T>& b) {
    using namespace fat_p;
    
    if (a.size() != b.size()) return false;
    
    return std::equal(a.begin(), a.end(), b.begin(),
        [](T x, T y) { return approximateEqual(x, y); });
}

// Usage
std::vector<double> result = compute_something();
std::vector<double> expected = {1.0, 2.0, 3.0};
assert(arrays_equal(result, expected));
```

### Example 4: Configuration Tolerance

```cpp
#include "FloatingPointComparison.h"

class SimulationConfig {
    double position_tolerance = 1e-6;  // meters
    double velocity_tolerance = 1e-3;  // m/s
    
public:
    bool positions_match(double p1, double p2) const {
        using namespace fat_p;
        return floatEqual(p1, p2, position_tolerance);
    }
    
    bool velocities_match(double v1, double v2) const {
        using namespace fat_p;
        // Use relative comparison for velocities (may span large range)
        return floatEqual<double, RelativeComparisonPolicy>(
            v1, v2, velocity_tolerance);
    }
};
```

### Example 5: Special Value Handling

```cpp
#include "FloatingPointComparison.h"
#include <limits>
#include <cmath>

void test_special_values() {
    using namespace fat_p;
    
    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();
    
    // NaN never equals anything (including itself)
    assert(!approximateEqual(nan, nan));
    assert(!approximateEqual(nan, 0.0));
    
    // Same-sign infinities are equal
    assert(approximateEqual(inf, inf));
    assert(approximateEqual(-inf, -inf));
    
    // Opposite-sign infinities are not equal
    assert(!approximateEqual(inf, -inf));
    
    // Signed zeros are equal
    assert(approximateEqual(+0.0, -0.0));
}
```

### Example 6: Cross-Platform Testing

```cpp
#include "FloatingPointComparison.h"

// Verify numerical reproducibility across platforms
void test_cross_platform() {
    using namespace fat_p;
    
    // Compute same value on different platforms
    double result = complex_computation();
    
    // Use ULP comparison for bit-exact verification
    // (Useful when compiler optimizations must be deterministic)
    double expected = 3.141592653589793;
    assert(floatEqual<double, UlpComparisonPolicy>(result, expected, 8.0));
    // Within 8 ULPs is acceptable for cross-platform consistency
}
```

---

## Best Practices

### 1. Choose the Right Policy

**Decision Tree:**

```
Is this production code with unknown input ranges?
├─ YES → Use approximateEqual() (HybridComparisonPolicy)
└─ NO → Continue...

Do you need bit-exact verification?
├─ YES → Use UlpComparisonPolicy
└─ NO → Continue...

Are your values all at a similar scale?
├─ YES → Use StandardComparisonPolicy (absolute)
└─ NO → Use RelativeComparisonPolicy or HybridComparisonPolicy
```

### 2. Set Tolerances Appropriately

```cpp
// BAD: Tolerance too tight (test will be brittle)
assert(floatEqual(compute(), expected, 1e-16));  // Machine epsilon territory!

// GOOD: Allow for reasonable rounding error
assert(floatEqual(compute(), expected, 1e-10));

// BAD: Tolerance too loose (test is meaningless)
assert(floatEqual(1.0, 2.0, 10.0));  // Will always pass!

// GOOD: Tolerance based on problem domain
assert(floatEqual(measured_meters, expected_meters, 0.001));  // 1mm tolerance
```

### 3. Document Your Choice

```cpp
// GOOD: Explain why this tolerance is appropriate
// Tolerance of 1e-6 accounts for:
// - Accumulated rounding in 1000 iteration loop
// - Initial condition precision of input data (1e-8)
// - Expected convergence rate (quadratic)
bool converged = floatEqual(x_new, x_old, 1e-6);
```

### 4. Avoid Floating-Point in Keys

```cpp
// BAD: Using floats as map keys or set elements
std::map<double, std::string> bad_map;
bad_map[0.1 + 0.2] = "sum";
assert(bad_map.count(0.3) == 1);  // May fail!

// GOOD: Use integers, strings, or custom comparison
struct ApproxDouble {
    double value;
    bool operator<(const ApproxDouble& other) const {
        // Define stable ordering using approximateEqual
        return value < other.value && !approximateEqual(value, other.value);
    }
};
```

### 5. Be Consistent

```cpp
// BAD: Mixing comparison methods
if (a == b) {  // Direct equality
    // ...
} else if (floatEqual(a, c)) {  // Approximate equality
    // ...
}

// GOOD: Use consistent comparison throughout
if (approximateEqual(a, b)) {
    // ...
} else if (approximateEqual(a, c)) {
    // ...
}
```

### 6. Test Edge Cases

Always test your comparisons with:
```cpp
// Zero and near-zero
approximateEqual(0.0, 1e-100);
approximateEqual(1e-10, 2e-10);

// Large values
approximateEqual(1e10, 1e10 + 1.0);

// Opposite signs
approximateEqual(-1.0, 1.0);  // Should be false

// Special values
approximateEqual(inf, inf);
approximateEqual(nan, nan);  // Should be false
approximateEqual(+0.0, -0.0);  // Should be true
```

---

## Advanced Topics

### Special Value Handling

The library implements IEEE 754 compliant special value handling:

#### NaN (Not a Number)
```cpp
// NaN is NEVER equal to anything (including itself)
double nan = 0.0 / 0.0;
assert(!approximateEqual(nan, nan));      // false
assert(!approximateEqual(nan, 0.0));      // false
assert(!approximateEqual(nan, INFINITY)); // false

// This is IEEE 754 mandated behavior
// Rationale: NaN represents "invalid computation", not a specific value
```

#### Infinity
```cpp
double inf = 1.0 / 0.0;

// Same-sign infinities are equal
assert(approximateEqual(inf, inf));       // true
assert(approximateEqual(-inf, -inf));     // true

// Opposite-sign infinities are not equal
assert(!approximateEqual(inf, -inf));     // false

// Infinity vs finite: not equal
assert(!approximateEqual(inf, 1e308));    // false (even very large finite)
```

#### Signed Zero
```cpp
// IEEE 754 defines -0.0 == +0.0
assert(approximateEqual(+0.0, -0.0));     // true

// But they have different bit representations
assert(std::signbit(+0.0) != std::signbit(-0.0));  // true

// The library treats them as equal for comparison purposes
```

### Sign Consistency Enforcement

All policies enforce sign consistency (except for ±0.0):

```cpp
// Opposite signs are NEVER considered equal (unless both are zero)
assert(!approximateEqual(-1e-100, +1e-100));   // false
assert(!approximateEqual(-1.0, +1.0));         // false

// Even with very large tolerance
assert(!floatEqual(-0.5, +0.5, 10.0));         // false

// This prevents nonsensical results like -ε ≈ +ε
```

Rationale: Values with opposite signs represent fundamentally different physical or mathematical quantities. Allowing them to be "approximately equal" leads to logical errors.

### Diagnostic Logging

When comparisons fail, the library can log diagnostic information:

```cpp
// Enable diagnostic logging (implementation-specific)
// The library uses fat_p::diagnostic::conditionalPrintError()

double a = 1.0;
double b = 1.1;
bool eq = floatEqual(a, b, 0.05);  // false

// May log: "Equality check failed: 1.0 and 1.1 differ by more than 0.05"
```

This is particularly useful during debugging and test development.

### Subnormal Number Handling

**Subnormal numbers** (also called denormalized numbers) are very small floating-point values near zero where the exponent is at its minimum.

#### Why Subnormals Are Tricky

For normal floating-point numbers, ULP distance is meaningful. But subnormals have:
- Fixed exponent (at minimum value)
- Only mantissa varies
- ULP distance becomes unreliable

#### Library Solution

`UlpComparisonPolicy` uses **hybrid handling** for subnormals:
```cpp
// For subnormal values, switch to absolute tolerance:
// - Float: |a - b| ≤ 1e-6
// - Double: |a - b| ≤ 1e-12

float tiny_a = 1e-40f;  // Subnormal
float tiny_b = 2e-40f;  // Subnormal
bool eq = floatEqual<float, UlpComparisonPolicy>(tiny_a, tiny_b, 10.0);
// Uses absolute tolerance (1e-6), not ULP distance
```

This ensures robust behavior across the entire floating-point range.

### Performance Considerations

#### Comparison Costs

Relative cost of each policy (measured in CPU cycles):

| Policy | Typical Cost | Why |
|--------|--------------|-----|
| Standard | ~10 cycles | Simple subtraction + comparison |
| Relative | ~20 cycles | Additional division + max() |
| ULP | ~30 cycles | Bit manipulation + integer arithmetic |
| Hybrid | ~30 cycles | Two comparisons (short-circuit) |

**Recommendation:** For performance-critical code, profile before optimizing. The cost difference is usually negligible compared to the floating-point operations being compared.

#### Branch Prediction

All policies are branch-prediction friendly:
```cpp
// Short-circuit evaluation helps branch predictor
if (diff <= absEps) return true;  // Most common case first
if (diff <= relEps * maxAbs) return true;
```

---

## Common Pitfalls

### Pitfall 1: Using `operator==` Directly

```cpp
// ❌ WRONG: Direct comparison
double result = compute();
if (result == 0.0) {  // Almost never true for computed values!
    // ...
}

// ✅ CORRECT: Approximate comparison
if (approximateEqual(result, 0.0, 1e-10, 1e-12)) {
    // ...
}
```

### Pitfall 2: Wrong Policy for the Scale

```cpp
// ❌ WRONG: Absolute tolerance for large values
double big_a = 1e10;
double big_b = 1e10 + 1.0;
if (floatEqual(big_a, big_b, 1e-9)) {  // Will fail (epsilon too small)!
    // ...
}

// ✅ CORRECT: Relative tolerance for large values
if (floatEqual<double, RelativeComparisonPolicy>(big_a, big_b, 1e-9)) {
    // ...
}

// ✅ BETTER: Hybrid handles both cases
if (approximateEqual(big_a, big_b, 1e-9, 1e-9)) {
    // ...
}
```

### Pitfall 3: Relative Comparison Near Zero

```cpp
// ❌ WRONG: Relative tolerance near zero
double tiny_a = 1e-100;
double tiny_b = 2e-100;
if (floatEqual<double, RelativeComparisonPolicy>(tiny_a, tiny_b, 0.5)) {
    // Will fail! Relative difference is 100%
}

// ✅ CORRECT: Use Hybrid or Absolute
if (approximateEqual(tiny_a, tiny_b, 0.5, 1e-99)) {
    // Absolute tolerance (1e-99) handles this correctly
}
```

### Pitfall 4: Ignoring Accumulation

```cpp
// ❌ WRONG: Expecting exact sum
double sum = 0.0;
for (int i = 0; i < 1000; ++i) {
    sum += 0.001;
}
assert(sum == 1.0);  // May fail due to accumulated error!

// ✅ CORRECT: Account for error accumulation
assert(approximateEqual(sum, 1.0, 1e-10, 1e-10));
```

### Pitfall 5: Transitive Equality Assumption

```cpp
// Approximate equality is NOT transitive!
double a = 1.0;
double b = 1.0 + 1e-9;
double c = 1.0 + 2e-9;

assert(approximateEqual(a, b, 1e-8, 1e-8));  // true
assert(approximateEqual(b, c, 1e-8, 1e-8));  // true
assert(approximateEqual(a, c, 1e-8, 1e-8));  // true (but barely!)

// ⚠️ WARNING: Do not rely on transitivity
// If a ≈ b and b ≈ c, it does NOT guarantee a ≈ c
// (Though it may be true in practice with appropriate tolerance)
```

### Pitfall 6: Comparing Floats in Sorted Containers

```cpp
// ❌ WRONG: Sorting floats directly
std::set<double> values;
values.insert(0.1 + 0.2);  // Inserts 0.30000000000000004
assert(values.count(0.3) == 1);  // May be 0!

// ✅ CORRECT: Custom comparator with approximate equality
struct ApproxCompare {
    bool operator()(double a, double b) const {
        if (approximateEqual(a, b)) return false;  // Equal, not less
        return a < b;
    }
};
std::set<double, ApproxCompare> safe_values;
```

### Pitfall 7: Comparing Computed vs Literal Values

```cpp
// ❌ PROBLEMATIC: Comparing computed result to literal
double computed = std::sin(M_PI);  // Should be 0, actually ~1e-16
assert(computed == 0.0);  // Fails!

// ✅ CORRECT: Account for computation error
assert(approximateEqual(computed, 0.0, 1e-10, 1e-14));
```

---

## Integration with Other Components

### DiagnosticLogger Integration

The library integrates with `DiagnosticLogger.h` for conditional error reporting:

```cpp
// When a comparison fails, diagnostic information is logged (if enabled)
bool eq = floatEqual(1.0, 1.1, 0.05);
// May output: "Equality check failed: 1.0 and 1.1 differ by more than 0.05"
```

### Stringify Integration

The library uses `Stringify.h` for human-readable value formatting in error messages:

```cpp
// Error messages automatically format values appropriately
double a = 1.234567890123456;
double b = 1.234567890123457;
// Error message will show full precision: "1.234567890123456 vs 1.234567890123457"
```

### ComparisonTolerances Integration

Default epsilon values come from `ComparisonTolerances.h`:
```cpp
constexpr double kDefaultDoubleEpsilon = 1e-9;
constexpr float kDefaultFloatEpsilon = 1e-5f;
```

---

## Comparison Table: All Policies

| Feature | Standard | Relative | ULP | Hybrid |
|---------|----------|----------|-----|--------|
| **Formula** | \|a-b\| ≤ ε | \|a-b\| ≤ ε·max(\|a\|,\|b\|) | ULP distance ≤ N | Both Standard AND Relative |
| **Near-zero handling** | ✅ Excellent | ❌ Poor | ⚠️ Complex | ✅ Excellent |
| **Large values** | ❌ Poor | ✅ Excellent | ✅ Good | ✅ Excellent |
| **Multi-scale robustness** | ❌ Poor | ⚠️ Moderate | ✅ Good | ✅ Excellent |
| **Simplicity** | ✅ Very simple | ✅ Simple | ❌ Complex | ⚠️ Moderate |
| **Type support** | All types | All types | float/double only | All types |
| **Recommended for** | Known scale | Large ranges (>0) | Bit-exact tests | Production code |
| **Default tolerance** | Type-specific ε | Type-specific ε | 4 ULPs | Type-specific ε |

---

## FAQ

### Q: Which policy should I use?

**A:** For most production code, use `approximateEqual()` (which uses `HybridComparisonPolicy`). For testing numerical algorithms, consider `UlpComparisonPolicy`. For simple cases with known scale, `StandardComparisonPolicy` is fine.

### Q: Why is NaN never equal to itself?

**A:** This is mandated by IEEE 754. NaN represents "undefined result", not a specific value. Testing `if (x != x)` is the standard way to check for NaN.

### Q: How do I choose appropriate tolerances?

**A:** Consider:
1. Expected error in your algorithm (rounding, convergence criteria)
2. Precision requirements of your problem domain
3. Input data precision
4. Number of operations (accumulation)

Generally: `1e-10` for double is a good starting point for absolute tolerance.

### Q: Can I use this with `std::complex<double>`?

**A:** Not directly. You'll need to compare real and imaginary parts separately:
```cpp
bool complex_equal(std::complex<double> a, std::complex<double> b) {
    return approximateEqual(a.real(), b.real()) &&
           approximateEqual(a.imag(), b.imag());
}
```

### Q: What about `long double`?

**A:** All policies except `UlpComparisonPolicy` support `long double`. ULP comparison requires fixed-size types (32-bit float, 64-bit double).

### Q: Why doesn't ULP comparison work for `long double`?

**A:** `long double` size is platform-dependent (64, 80, or 128 bits). Bit manipulation requires knowing the exact format. Use `HybridComparisonPolicy` for `long double`.

### Q: Is approximate equality transitive?

**A:** No! If `a ≈ b` and `b ≈ c`, it does NOT guarantee `a ≈ c`. Be careful when chaining comparisons.

### Q: How does this compare to Google Test's `EXPECT_NEAR`?

**A:** `EXPECT_NEAR` uses absolute tolerance (like `StandardComparisonPolicy`). This library provides more flexible policies and better handles multi-scale data.

### Q: Can I use this in embedded systems?

**A:** Yes! The library is header-only with no dependencies. However, be aware of potential increased code size due to template instantiation.

---

## Summary

**FloatingPointComparison.h** solves the fundamental problem that direct equality comparison fails for floating-point numbers. By providing four carefully designed policies, it gives developers the right tool for each comparison scenario:

- **StandardComparisonPolicy**: Simple absolute tolerance
- **RelativeComparisonPolicy**: Scale-independent (but not near zero)
- **UlpComparisonPolicy**: Bit-exact verification
- **HybridComparisonPolicy**: Robust across all scales (recommended)

**Key Takeaways:**

1. Never use `operator==` for computed floating-point values
2. Default to `approximateEqual()` for production code
3. Choose policies based on your value scales and requirements
4. Set tolerances based on problem domain, not arbitrary numbers
5. Test edge cases (zero, infinity, NaN, opposite signs)
6. Remember: approximate equality is not transitive

**Quick Reference:**

```cpp
// Most common usage (recommended)
bool eq = approximateEqual(a, b);

// With custom tolerances
bool eq = approximateEqual(a, b, 1e-5, 1e-8);

// Explicit policy selection
bool eq = floatEqual<double, UlpComparisonPolicy>(a, b, 4.0);
```

By understanding the strengths and limitations of each policy, you can write robust numerical code that behaves correctly across all scales and edge cases.

---

**End of Manual**
