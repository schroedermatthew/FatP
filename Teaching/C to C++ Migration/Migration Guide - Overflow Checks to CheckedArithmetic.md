---
doc_id: MG-CHECKEDARITH-001
doc_type: "Migration Guide"
title: "Manual Overflow Checks to CheckedArithmetic"
from_pattern: "Manual overflow detection, compiler builtins, unchecked math"
to_component: "CheckedArithmetic"
fatp_version: "1.0"
cxx_standard: "C++20"
migration_complexity: "Low-Medium"
breaking_changes: false
last_verified: "2025-01-08"
---

# Migration Guide - Manual Overflow Checks to CheckedArithmetic

### *From Undefined Behavior to Policy-Controlled Safe Arithmetic*

*FAT-P Library — January 2025*

---

## Migration Guide Card

**From:** Manual overflow checks, compiler builtins (`__builtin_add_overflow`), unchecked arithmetic  
**To:** `CheckedArithmetic<T, Policy>` with policy-selected overflow behavior  
**Why migrate:** Integer overflow is undefined behavior in C/C++; manual checks are error-prone and inconsistently applied  
**Compatibility strategy:** Incremental — wrap arithmetic types one at a time; explicit conversion to/from underlying type  
**Mechanical steps:**
1. Identify arithmetic operations on security-sensitive or safety-critical values.
2. Replace integer types with `CheckedArithmetic<T, Policy>`.
3. Select overflow policy (throw, saturate, or return `Expected`).
4. Fix call sites that rely on implicit conversion.
**Behavioral equivalence:** Same arithmetic results when no overflow occurs  
**Intentional differences:** Overflow is detected and handled per policy rather than silently producing undefined behavior  
**Failure model:** Overflow → policy-determined response (throw, saturate, `Expected` error)  
**Threading model:** Unchanged — value type with no synchronization requirements  
**Lifetime model:** Value semantics; same lifetime as underlying integer  
**Alternatives:** Compiler builtins (`__builtin_*_overflow`), SafeInt, Boost.SafeNumerics  
**Verification:** Unit tests for overflow detection at boundary values; sanitizer runs (`-fsanitize=integer`)  
**Rollback plan:** Replace `CheckedArithmetic<T>` with raw integer types; restore manual overflow checks

---

## Table of Contents

1. [The Problem with Integer Overflow](#the-problem-with-integer-overflow)
2. [Real-World Overflow Disasters](#real-world-overflow-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The CheckedArithmetic Solution](#the-checkedarithmetic-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When CheckedArithmetic Loses](#when-checkedarithmetic-loses)

---

## The Problem with Integer Overflow

Signed integer overflow is **undefined behavior** in C and C++. The compiler is allowed to assume it never happens:

```cpp
bool check_overflow(int x) {
    return x + 1 > x;  // Compiler may optimize to "return true"
}
// Because if x + 1 overflows, it's UB, so compiler assumes x + 1 > x always
```

Unsigned overflow is **defined** (wraps around), but often still wrong:

```cpp
size_t compute_size(size_t count, size_t elem_size) {
    return count * elem_size;  // Wraps silently on overflow!
}
// compute_size(SIZE_MAX, 2) returns a small number, not an error
```

**Why this matters:**

| Problem | Consequence |
|---------|-------------|
| **Security vulnerabilities** | Buffer overflows from size miscalculation |
| **Silent data corruption** | Financial calculations wrap to wrong values |
| **Undefined behavior** | Compiler optimizes away "impossible" checks |
| **Platform variance** | Works on 64-bit, fails on 32-bit |

---

## Real-World Overflow Disasters

### The OpenSSH Integer Overflow (CVE-2002-0639)

```c
/* Simplified from original vulnerability */
int nresp = packet_get_int();
if (nresp > 0) {
    response = malloc(nresp * sizeof(char*));  /* OVERFLOW! */
    /* If nresp = 1073741824, nresp * 4 = 0 on 32-bit */
    /* malloc(0) succeeds, then buffer overflow on write */
}
```

### The Flash Player Vulnerability (CVE-2015-3043)

```c
int calculate_buffer_size(int width, int height, int components) {
    return width * height * components;  /* Can overflow */
}
/* 65536 × 65536 × 4 = 0 on 32-bit → heap overflow */
```

### The Apple goto fail (Context)

While not directly an overflow bug, demonstrates how subtle arithmetic issues cause security failures. Many overflow bugs similarly hide in plain sight.

### The Ariane 5 Explosion

A 64-bit floating point to 16-bit integer conversion overflow caused a $370 million rocket to self-destruct 37 seconds after launch.

```c
/* Simplified logic */
int16_t horizontal_velocity = (int16_t)computed_velocity;
/* computed_velocity exceeded INT16_MAX → undefined behavior → crash */
```

---

## The C Patterns

### Pattern 1: No Checking (Pray)

```c
size_t total = count * element_size;
void* buffer = malloc(total);
/* If overflow occurred, buffer is too small → heap corruption */
```

**Problems:** Silent corruption, security vulnerabilities.

### Pattern 2: Manual Pre-Check

```c
size_t safe_multiply(size_t a, size_t b) {
    if (a > 0 && b > SIZE_MAX / a) {
        return 0;  /* Overflow would occur */
    }
    return a * b;
}
```

**Problems:** 
- Division is expensive
- Easy to get the check wrong
- Must remember to use it every time
- Doesn't handle signed overflow (UB before you can check)

### Pattern 3: Compiler Builtins

```c
/* GCC/Clang specific */
size_t safe_add(size_t a, size_t b, int* overflow) {
    size_t result;
    *overflow = __builtin_add_overflow(a, b, &result);
    return result;
}
```

**Problems:**
- Non-portable (compiler-specific)
- Must check return value manually
- Awkward API (out parameter)

### Pattern 4: Post-Check (Too Late for Signed)

```c
int a = get_value();
int b = get_value();
int result = a + b;  /* UB if overflow! */

/* This check is meaningless - UB already happened */
if ((b > 0 && result < a) || (b < 0 && result > a)) {
    handle_overflow();
}
```

**Problems:** The overflow already occurred—check is optimized away.

---

## The CheckedArithmetic Solution

### Core Concept

`CheckedArithmetic<T, Policy>` wraps an integer type and checks every operation for overflow. The policy determines what happens on overflow.

```cpp
#include "CheckedArithmetic.h"
using namespace fat_p;

// Different policies for different needs
using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;
using SatInt = CheckedArithmetic<int, SaturateOnOverflow>;
using WrapInt = CheckedArithmetic<int, WrapOnOverflow>;

SafeInt a = 1000000000;
SafeInt b = 2000000000;

try {
    SafeInt c = a + b;  // Throws: overflow detected
} catch (const overflow_error& e) {
    std::cerr << e.what() << "\n";
}

SatInt x = INT_MAX;
SatInt y = x + 1;  // y == INT_MAX (saturated)

WrapInt w = INT_MAX;
WrapInt z = w + 1;  // z == INT_MIN (wrapped, but defined)
```

### Overflow Policies

| Policy | Behavior | Use Case |
|--------|----------|----------|
| **ThrowOnOverflow** | Throws `overflow_error` | Security-critical code |
| **SaturateOnOverflow** | Clamps to min/max | Signal processing, graphics |
| **WrapOnOverflow** | Wraps (defined behavior) | Intentional modular arithmetic |
| **TrapOnOverflow** | Calls `std::abort()` | Debug/testing |
| **ExpectedOnOverflow** | Returns `Expected<T, E>` | Functional error handling |

### Key Features

| Feature | Benefit |
|---------|---------|
| **Policy-based** | Choose behavior at compile time |
| **Type-safe** | Can't accidentally mix with raw integers |
| **Operator overloading** | Natural arithmetic syntax |
| **SIMD optimized** | Batch operations use vectorization |
| **Zero overhead option** | `WrapOnOverflow` with unsigned matches raw int |

### API Overview

```cpp
template<typename T, typename Policy = ThrowOnOverflow>
class CheckedArithmetic {
public:
    // Construction
    CheckedArithmetic() = default;
    explicit CheckedArithmetic(T value);
    
    // Arithmetic operators (all check for overflow)
    CheckedArithmetic operator+(CheckedArithmetic rhs) const;
    CheckedArithmetic operator-(CheckedArithmetic rhs) const;
    CheckedArithmetic operator*(CheckedArithmetic rhs) const;
    CheckedArithmetic operator/(CheckedArithmetic rhs) const;
    CheckedArithmetic operator%(CheckedArithmetic rhs) const;
    
    // Unary
    CheckedArithmetic operator-() const;
    CheckedArithmetic& operator++();
    CheckedArithmetic& operator--();
    
    // Compound assignment
    CheckedArithmetic& operator+=(CheckedArithmetic rhs);
    CheckedArithmetic& operator-=(CheckedArithmetic rhs);
    CheckedArithmetic& operator*=(CheckedArithmetic rhs);
    CheckedArithmetic& operator/=(CheckedArithmetic rhs);
    
    // Comparisons
    bool operator==(CheckedArithmetic rhs) const;
    bool operator<(CheckedArithmetic rhs) const;
    // ... etc
    
    // Access underlying value
    T value() const noexcept;
    explicit operator T() const noexcept;
    
    // Static checked operations
    static CheckedArithmetic add(CheckedArithmetic a, CheckedArithmetic b);
    static CheckedArithmetic multiply(CheckedArithmetic a, CheckedArithmetic b);
};
```

---

## Migration Steps

### Step 1: Identify Overflow-Sensitive Code

Look for:

```bash
# Size calculations
grep -rn "malloc\|calloc\|new\[" src/

# Arithmetic on external input
grep -rn "get.*int\|read.*int\|parse.*int" src/

# Financial/critical calculations
grep -rn "price\|amount\|balance\|total" src/
```

### Step 2: Choose Policy Based on Context

| Context | Recommended Policy |
|---------|-------------------|
| Security-critical (sizes, indices) | `ThrowOnOverflow` or `ExpectedOnOverflow` |
| Signal processing, graphics | `SaturateOnOverflow` |
| Cryptography, hash functions | `WrapOnOverflow` |
| Debug builds | `TrapOnOverflow` |
| Hot inner loops | Raw integers with bounds verified at boundaries |

### Step 3: Replace Types

**Before:**
```cpp
int compute_buffer_size(int width, int height, int channels) {
    return width * height * channels;
}
```

**After:**
```cpp
using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;

SafeInt compute_buffer_size(SafeInt width, SafeInt height, SafeInt channels) {
    return width * height * channels;  // Throws on overflow
}

// Or return Expected for non-throwing code:
using CheckedInt = CheckedArithmetic<int, ExpectedOnOverflow>;

Expected<int, std::string> compute_buffer_size(int w, int h, int c) {
    CheckedInt width(w), height(h), channels(c);
    auto result = width * height * channels;
    if (!result.has_value()) {
        return unexpected("Buffer size overflow");
    }
    return result.value().value();
}
```

### Step 4: Handle Boundaries

At API boundaries, convert explicitly:

```cpp
// Public API takes raw integers for compatibility
size_t allocate_buffer(size_t width, size_t height) {
    using SafeSize = CheckedArithmetic<size_t, ThrowOnOverflow>;
    
    SafeSize w(width), h(height);
    SafeSize size = w * h * SafeSize(4);  // 4 bytes per pixel
    
    return size.value();  // Convert back to raw
}
```

### Step 5: Batch Operations for Performance

For large arrays, use SIMD-optimized batch operations:

```cpp
void scale_values(std::span<int> values, int factor) {
    // SIMD-optimized batch multiply with overflow checking
    checked_multiply_array(values.data(), values.size(), factor);
}
```

---

## Before/After Examples

### Example 1: Buffer Allocation

**Before (vulnerable):**
```cpp
void* allocate_image(int width, int height, int bytes_per_pixel) {
    size_t size = width * height * bytes_per_pixel;
    // If width = 65536, height = 65536, bpp = 4:
    // 65536 * 65536 * 4 = 17179869184, but on 32-bit:
    // Wraps to 0 → malloc(0) succeeds → heap overflow
    return malloc(size);
}
```

**After (safe):**
```cpp
void* allocate_image(int width, int height, int bytes_per_pixel) {
    using SafeSize = CheckedArithmetic<size_t, ThrowOnOverflow>;
    
    if (width <= 0 || height <= 0 || bytes_per_pixel <= 0) {
        throw std::invalid_argument("Invalid dimensions");
    }
    
    SafeSize w(width), h(height), bpp(bytes_per_pixel);
    SafeSize size = w * h * bpp;  // Throws on overflow
    
    return malloc(size.value());
}
```

### Example 2: Array Index Calculation

**Before:**
```cpp
int get_pixel(const Image& img, int x, int y) {
    int index = y * img.width + x;  // Can overflow
    return img.pixels[index];        // Out of bounds if overflow
}
```

**After:**
```cpp
int get_pixel(const Image& img, int x, int y) {
    using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;
    
    SafeInt index = SafeInt(y) * SafeInt(img.width) + SafeInt(x);
    
    if (index.value() < 0 || index.value() >= img.width * img.height) {
        throw std::out_of_range("Pixel index out of bounds");
    }
    
    return img.pixels[index.value()];
}
```

### Example 3: Financial Calculation

**Before:**
```cpp
long long calculate_total(const std::vector<Item>& items) {
    long long total = 0;
    for (const auto& item : items) {
        total += item.price * item.quantity;  // Can overflow
    }
    return total;
}
```

**After:**
```cpp
Expected<long long, std::string> calculate_total(const std::vector<Item>& items) {
    using SafeMoney = CheckedArithmetic<long long, ExpectedOnOverflow>;
    
    SafeMoney total(0);
    for (const auto& item : items) {
        auto line_total = SafeMoney(item.price) * SafeMoney(item.quantity);
        if (!line_total.has_value()) {
            return unexpected("Line item overflow");
        }
        
        auto new_total = total + line_total.value();
        if (!new_total.has_value()) {
            return unexpected("Total overflow");
        }
        total = new_total.value();
    }
    
    return total.value();
}
```

### Example 4: Saturating Audio

**Before (clipping):**
```cpp
int16_t mix_audio(int16_t a, int16_t b) {
    int sum = a + b;
    // Manual clipping
    if (sum > INT16_MAX) return INT16_MAX;
    if (sum < INT16_MIN) return INT16_MIN;
    return sum;
}
```

**After (automatic saturation):**
```cpp
int16_t mix_audio(int16_t a, int16_t b) {
    using SatInt16 = CheckedArithmetic<int16_t, SaturateOnOverflow>;
    return (SatInt16(a) + SatInt16(b)).value();
}
```

---

## Advanced Patterns

### Pattern: Overflow Budget

For hot loops, check at boundaries instead of every operation:

```cpp
void process_batch(std::span<int> data, int multiplier) {
    // Pre-check: verify no element can overflow
    int max_val = *std::max_element(data.begin(), data.end());
    int min_val = *std::min_element(data.begin(), data.end());
    
    using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;
    SafeInt(max_val) * SafeInt(multiplier);  // Throws if would overflow
    SafeInt(min_val) * SafeInt(multiplier);  // Check negative too
    
    // Now safe to use unchecked arithmetic in loop
    for (int& val : data) {
        val *= multiplier;
    }
}
```

### Pattern: Type-Safe Conversions

```cpp
template<typename To, typename From>
Expected<To, std::string> safe_cast(From value) {
    if (value < std::numeric_limits<To>::min() ||
        value > std::numeric_limits<To>::max()) {
        return unexpected("Value out of range for target type");
    }
    return static_cast<To>(value);
}

// Usage
int64_t big_value = get_big_value();
auto small = safe_cast<int32_t>(big_value);
if (!small) {
    handle_error(small.error());
}
```

### Pattern: Debug vs Release

```cpp
#ifdef NDEBUG
    using Int = int;  // Raw int in Release for performance
#else
    using Int = CheckedArithmetic<int, TrapOnOverflow>;  // Trap in Debug
#endif

Int compute(Int a, Int b) {
    return a * b + a;  // Checked in Debug, unchecked in Release
}
```

### Pattern: SIMD Batch Checking

```cpp
// Check array for potential overflow before batch operation
bool will_overflow_add(const int* a, const int* b, size_t n) {
    // SIMD implementation checks many elements in parallel
    return checked_add_overflow_test_simd(a, b, n);
}

void batch_add(int* dst, const int* a, const int* b, size_t n) {
    if (will_overflow_add(a, b, n)) {
        throw overflow_error("Batch add would overflow");
    }
    
    // Safe to use unchecked SIMD add
    simd_add(dst, a, b, n);
}
```

---

## Verification

### Compile-Time Guarantees

```cpp
using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;

SafeInt a(100);
int b = 200;

// Cannot accidentally mix types
// SafeInt c = a + b;  // ERROR: no implicit conversion

// Must be explicit
SafeInt c = a + SafeInt(b);  // OK
```

### Runtime Tests

```cpp
TEST_CASE(overflow_throws) {
    using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;
    
    SafeInt a(INT_MAX);
    ASSERT_THROWS(a + SafeInt(1), overflow_error);
}

TEST_CASE(saturation_clamps) {
    using SatInt = CheckedArithmetic<int, SaturateOnOverflow>;
    
    SatInt a(INT_MAX);
    SatInt b = a + SatInt(1);
    ASSERT_EQ(b.value(), INT_MAX);
    
    SatInt c(INT_MIN);
    SatInt d = c - SatInt(1);
    ASSERT_EQ(d.value(), INT_MIN);
}

TEST_CASE(multiplication_overflow) {
    using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;
    
    SafeInt a(100000);
    SafeInt b(100000);
    ASSERT_THROWS(a * b, overflow_error);  // 10^10 > INT_MAX
}

TEST_CASE(division_by_zero) {
    using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;
    
    SafeInt a(42);
    SafeInt b(0);
    ASSERT_THROWS(a / b, std::domain_error);
}
```

### Fuzzing

```cpp
// Fuzz test: compare checked vs builtin
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) return 0;
    
    int a, b;
    memcpy(&a, data, 4);
    memcpy(&b, data + 4, 4);
    
    using SafeInt = CheckedArithmetic<int, ThrowOnOverflow>;
    
    bool builtin_overflow = false;
    int builtin_result;
    builtin_overflow = __builtin_add_overflow(a, b, &builtin_result);
    
    bool checked_overflow = false;
    int checked_result;
    try {
        checked_result = (SafeInt(a) + SafeInt(b)).value();
    } catch (const overflow_error&) {
        checked_overflow = true;
    }
    
    assert(builtin_overflow == checked_overflow);
    if (!builtin_overflow) {
        assert(builtin_result == checked_result);
    }
    
    return 0;
}
```

---

## When CheckedArithmetic Loses

### 1. Hot Inner Loops

Checking every operation adds ~0.5-2ns overhead:

```cpp
// Don't use CheckedArithmetic here:
for (size_t i = 0; i < 1000000000; i++) {
    sum += data[i];  // 1 billion checks = 0.5-2 seconds overhead
}

// Instead: validate bounds once, use raw arithmetic
if (data.size() > SIZE_MAX / sizeof(int)) {
    throw overflow_error("Array too large");
}
for (size_t i = 0; i < data.size(); i++) {
    sum += data[i];
}
```

### 2. Known-Safe Ranges

When you can prove values won't overflow:

```cpp
// x and y are both in [0, 100] from validation
// No need for checked arithmetic
int index = y * 100 + x;  // Max value: 100 * 100 + 100 = 10100, fits in int
```

### 3. Intentional Wraparound

Hash functions, RNGs, and crypto rely on defined wraparound:

```cpp
// Use WrapOnOverflow or raw unsigned
uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x85ebca6b;  // Intentional overflow
    x ^= x >> 13;
    x *= 0xc2b2ae35;
    x ^= x >> 16;
    return x;
}
```

### 4. Embedded Systems

Where exceptions aren't available, use different policies:

```cpp
// No exceptions in embedded
using SafeInt = CheckedArithmetic<int, SaturateOnOverflow>;
// or
using SafeInt = CheckedArithmetic<int, ExpectedOnOverflow>;
```

---

## Summary

| Aspect | C Pattern | CheckedArithmetic |
|--------|-----------|-------------------|
| Overflow detection | Manual (error-prone) | Automatic |
| Signed overflow | Undefined behavior | Defined, policy-controlled |
| Policy flexibility | None | Throw, saturate, wrap, trap, Expected |
| Performance | Zero overhead | ~0.5-2ns per operation |
| Type safety | None | Strong typing prevents mixing |

**Security Impact:**
- **Immediate:** Prevents buffer overflow vulnerabilities
- **Short-term:** Catches arithmetic bugs in testing
- **Long-term:** Documents intent (saturate vs throw)

---

## References

- [CWE-190: Integer Overflow](https://cwe.mitre.org/data/definitions/190.html)
- [SEI CERT C: INT30-C](https://wiki.sei.cmu.edu/confluence/display/c/INT30-C.+Ensure+that+unsigned+integer+operations+do+not+wrap)
- [Safe Integer Library Comparison](https://www.boost.org/doc/libs/release/libs/safe_numerics/)
- Fat-P User Manual: CheckedArithmetic — Complete API reference

---

*FAT-P Library Documentation — January 2025*
