# CheckedArithmetic User Manual

**Version:** 2.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17 (C++20 concepts supported)  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Operations](#operations)
4. [Policies](#policies)
5. [Error Handling](#error-handling)
6. [Floating-Point Support](#floating-point-support)
7. [SIMD Vectorization](#simd-vectorization)
8. [Compile-Time Checking](#compile-time-checking)
9. [Performance](#performance)
10. [Best Practices](#best-practices)

---

## Overview

CheckedArithmetic provides overflow/underflow-safe arithmetic operations with policy-based error handling.

### Key Features

- **Integer operations**: add, subtract, multiply, divide, modulo
- **Bitwise operations**: and, or, xor, shifts
- **Floating-point**: overflow, NaN, Inf detection
- **Policy-based**: throw, return Expected, saturate
- **SIMD optimized**: AVX2 vectorized variants
- **Compile-time**: constexpr checking with static_assert

### Include

```cpp
#include "CheckedArithmetic.h"
using namespace fat_p;
```

---

## Quick Start

### Basic Checked Operations

```cpp
// Throws on overflow
int result = checked_add(INT_MAX, 1);  // throws OverflowContractError

// Safe alternatives
int a = checked_add(100, 200);     // OK: 300
int b = checked_sub(50, 30);       // OK: 20
int c = checked_mul(10, 20);       // OK: 200
int d = checked_div(100, 5);       // OK: 20
int e = checked_mod(17, 5);        // OK: 2
```

### Expected-Based (No Exceptions)

```cpp
auto result = safe_add<ReturnExpectedPolicy>(INT_MAX, 1);
if (!result) {
    std::cerr << "Error: " << result.error() << "\n";  // "Overflow"
} else {
    use(*result);
}
```

### Saturating Arithmetic

```cpp
// Clamps to max/min instead of overflow
int x = saturating_add(INT_MAX, 100);  // Returns INT_MAX
int y = saturating_sub(INT_MIN, 100);  // Returns INT_MIN
```

---

## Operations

### Integer Arithmetic

| Function | Operation | Checks |
|----------|-----------|--------|
| `checked_add(a, b)` | a + b | Overflow, underflow |
| `checked_sub(a, b)` | a - b | Overflow, underflow |
| `checked_mul(a, b)` | a × b | Overflow, underflow |
| `checked_div(a, b)` | a ÷ b | Division by zero, INT_MIN/-1 |
| `checked_mod(a, b)` | a % b | Division by zero |
| `checked_neg(a)` | -a | INT_MIN overflow |
| `checked_abs(a)` | \|a\| | INT_MIN overflow |

### Bitwise Operations

| Function | Operation | Checks |
|----------|-----------|--------|
| `checked_and(a, b)` | a & b | Type compatibility |
| `checked_or(a, b)` | a \| b | Type compatibility |
| `checked_xor(a, b)` | a ^ b | Type compatibility |
| `checked_not(a)` | ~a | None (always safe) |
| `checked_left_shift(a, n)` | a << n | Shift amount, overflow |
| `checked_right_shift(a, n)` | a >> n | Shift amount |

### Compound Operations

```cpp
// Fused multiply-add: a * b + c
int result = checked_fma(10, 20, 5);  // 205

// Absolute difference: |a - b|
unsigned diff = checked_abs_diff(100, 30);  // 70

// Power: a^n
int power = checked_pow(2, 10);  // 1024
```

---

## Policies

### ThrowOnErrorPolicy (Default)

Throws typed exceptions on error.

```cpp
try {
    int result = checked_add(INT_MAX, 1);
} catch (const OverflowContractError& e) {
    // Handle overflow
}
```

### ReturnExpectedPolicy

Returns `Expected<T, MathError>` for no-throw code.

```cpp
auto result = safe_add<ReturnExpectedPolicy>(a, b);
if (!result) {
    switch (result.error()) {
        case MathError::Overflow: /* ... */ break;
        case MathError::DivByZero: /* ... */ break;
    }
}
```

### SaturatingPolicy

Clamps to type limits instead of erroring.

```cpp
int x = saturating_add(INT_MAX, 100);  // INT_MAX
int y = saturating_mul(INT_MAX, 2);    // INT_MAX
int z = saturating_sub(INT_MIN, 1);    // INT_MIN
```

### InfTolerantPolicy

For floating-point: allows infinity but rejects NaN.

```cpp
double x = fp_add<InfTolerantPolicy>(DBL_MAX, DBL_MAX);  // Returns Inf
double y = fp_div<InfTolerantPolicy>(0.0, 0.0);          // Throws (NaN)
```

---

## Error Handling

### MathError Enum

```cpp
enum class MathError {
    Overflow,        // Result too large
    Underflow,       // Result too small
    DivByZero,       // Division by zero
    NaN,             // Not a number result
    Inf,             // Infinite result
    InvalidArgument  // Bad input
};
```

### Exception Types

| Error | Exception Type |
|-------|---------------|
| Overflow | `OverflowContractError` |
| Underflow | `UnderflowContractError` |
| DivByZero | `DomainContractError` |
| NaN/Inf | `DomainContractError` |

### Pattern: Mixed Error Handling

```cpp
Expected<int, MathError> compute(int a, int b) {
    // Use Expected internally
    auto sum = safe_add<ReturnExpectedPolicy>(a, b);
    if (!sum) return sum;
    
    auto product = safe_mul<ReturnExpectedPolicy>(*sum, 2);
    return product;
}

void caller() {
    auto result = compute(x, y);
    if (!result) {
        // Convert to exception if needed
        throw OverflowContractError("Computation failed");
    }
}
```

---

## Floating-Point Support

### Checked FP Operations

```cpp
double a = fp_add(1e308, 1e308);  // Throws: Inf
double b = fp_div(0.0, 0.0);      // Throws: NaN
double c = fp_mul(1e-308, 1e-308); // Throws: Underflow to zero

// With policy
auto result = fp_add<ReturnExpectedPolicy>(x, y);
```

### Special Value Handling

```cpp
// Check before operations
if (std::isnan(x) || std::isinf(x)) {
    return make_unexpected(MathError::InvalidArgument);
}

// Or use checked operations that verify results
double safe = fp_add(a, b);  // Throws if result is NaN/Inf
```

### InfTolerantPolicy for Scientific Computing

```cpp
// Allow infinity (useful for limits)
double ratio = fp_div<InfTolerantPolicy>(1.0, 0.0);  // Returns Inf

// Still rejects NaN
double bad = fp_div<InfTolerantPolicy>(0.0, 0.0);   // Throws
```

---

## SIMD Vectorization

### AVX2 Support

When compiled with AVX2, vector operations are optimized:

```cpp
#ifdef __AVX2__
std::vector<int32_t> a = {1, 2, 3, 4, 5, 6, 7, 8};
std::vector<int32_t> b = {1, 1, 1, 1, 1, 1, 1, 1};

// Vectorized add (8 ints at a time)
auto result = checked_add_vector(a, b);
// result = {2, 3, 4, 5, 6, 7, 8, 9}
#endif
```

### Vector Operations

```cpp
// All return std::vector<T>
checked_add_vector(vec_a, vec_b);
checked_sub_vector(vec_a, vec_b);
checked_mul_vector(vec_a, vec_b);

// Scalar broadcast
checked_add_scalar(vec, scalar);
checked_mul_scalar(vec, scalar);
```

### Performance

| Operation | Scalar | AVX2 (8-wide) |
|-----------|--------|---------------|
| add | ~5ns | ~0.8ns/elem |
| mul | ~8ns | ~1ns/elem |
| div | ~20ns | ~3ns/elem |

---

## Compile-Time Checking

### constexpr Operations

```cpp
// Compile-time overflow detection
constexpr int a = constexpr_checked_add(100, 200);  // OK
// constexpr int b = constexpr_checked_add(INT_MAX, 1);  // Compile error!

// Use in template parameters
template <int N = constexpr_checked_mul(10, 10)>
struct Array { int data[N]; };
```

### static_assert Integration

```cpp
static_assert(
    constexpr_checked_add(INT_MAX/2, INT_MAX/2) < INT_MAX,
    "Overflow in computation"
);
```

---

## Performance

### Builtin Detection

GCC/Clang builtins are used when available:

```cpp
// Uses __builtin_add_overflow on GCC/Clang
// Falls back to manual checks on MSVC
int result = checked_add(a, b);
```

### Benchmark Results (Intel i7)

| Operation | Builtin | Fallback |
|-----------|---------|----------|
| checked_add | 2ns | 8ns |
| checked_mul | 3ns | 15ns |
| checked_div | 20ns | 25ns |
| saturating_add | 2ns | 5ns |

### Optimization Tips

```cpp
// Batch operations with vectors
auto results = checked_add_vector(vec_a, vec_b);  // Faster than loop

// Use saturating for performance-critical paths
int x = saturating_add(a, b);  // No branch for error handling

// Expected policy avoids exception overhead
auto r = safe_add<ReturnExpectedPolicy>(a, b);  // No try/catch needed
```

---

## Best Practices

### Do

```cpp
// ✅ Use for untrusted input
int user_count = checked_add(current_count, user_input);

// ✅ Use Expected for recoverable errors
auto result = safe_mul<ReturnExpectedPolicy>(a, b);
if (!result) return fallback_value;

// ✅ Use saturating for clamped values
int volume = saturating_add(current_volume, delta);
volume = saturating_sub(volume, 0);  // Clamp to 0

// ✅ Check floating-point results
double ratio = fp_div(numerator, denominator);
```

### Don't

```cpp
// ❌ Don't use for already-validated internal math
checked_add(constant_a, constant_b);  // Unnecessary overhead

// ❌ Don't ignore Expected results
safe_add<ReturnExpectedPolicy>(a, b);  // Result discarded!

// ❌ Don't mix signed/unsigned carelessly
checked_add(signed_val, unsigned_val);  // May need explicit cast
```

---

## Related Components

- **enforce.h**: `enforce()` macros for preconditions
- **Expected.h**: Return type for `ReturnExpectedPolicy`
- **ContractException.h**: Exception types
- **StrongId.h**: Uses checked arithmetic internally

---

**Document Version:** 1.0  
**Last Updated:** November 2025
