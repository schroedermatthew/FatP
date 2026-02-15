# CheckedArithmetic: A Fat-P Library Showcase

## Executive Summary

CheckedArithmetic is a **compile-time policy-resolved** arithmetic safety system that detects overflow, underflow, division-by-zero, and floating-point anomalies through compiler builtins that map directly to CPU flags. Unlike C++26's `std::add_sat` (saturation only) or Boost.SafeNumerics (exception only), CheckedArithmetic lets you select error handling—throw, return Expected, saturate, or tolerate infinities—at **compile time with zero virtual dispatch**. The `if constexpr` chains resolve during compilation, generating assembly identical to hand-tuned overflow checks while providing architectural flexibility the standard will never offer.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// Silent corruption: the billion-dollar bug
int32_t balance = 2'000'000'000;
int32_t deposit = 1'000'000'000;
int32_t new_balance = balance + deposit;  // -1,294,967,296 (wrapped!)
// Customer just lost $3 billion due to silent overflow

// Undefined behavior in "safe" code
int32_t a = INT32_MIN;
int32_t b = -1;
int32_t result = a / b;  // UB: INT32_MIN / -1 overflows
// Compiler may generate crashing code, wrong value, or "works"
```

| Issue | HPC Impact |
|-------|------------|
| Silent integer overflow | Simulation produces wrong results without error signal |
| Division UB | `INT_MIN / -1` is undefined even with valid operands |
| Floating-point NaN propagation | One bad value silently corrupts entire computation |
| No policy flexibility | `std::add_sat` only saturates—can't throw or return Expected |

### The Standard's Limitation

C++26 adds `std::add_sat`, `std::sub_sat`, etc.—but these **only saturate**. You cannot:
- Choose to throw an exception instead
- Return `Expected<T, Error>` for no-throw contexts
- Log the error and continue
- Apply different policies to different call sites

The standard provides **one hardcoded behavior**. Fat-P provides **architectural control**.

---

## Architecture: Compile-Time Policy Resolution

### The Mechanism: `if constexpr` Dispatch

```cpp
// Four policies—resolved entirely at compile time
struct ThrowOnErrorPolicy {};      // Throws via always_enforce
struct ReturnExpectedPolicy {};    // Returns Expected<T, MathError>
struct SaturatingPolicy {};        // Clamps to min/max (like std::add_sat)
struct InfTolerantPolicy {};       // For FP: allows Inf, rejects NaN

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] constexpr auto checked_add(T a, T b) {
    if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>) {
        T result;
        if (__builtin_add_overflow(a, b, &result)) {
            always_enforce(false, "Integer overflow");
        }
        return result;
    }
    else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>) {
        T result;
        if (__builtin_add_overflow(a, b, &result)) {
            return Expected<T, MathError>{unexpect, MathError::Overflow};
        }
        return Expected<T, MathError>{result};
    }
    else if constexpr (std::is_same_v<Policy, SaturatingPolicy>) {
        T result;
        if (__builtin_add_overflow(a, b, &result)) {
            return (b > 0) ? std::numeric_limits<T>::max() 
                           : std::numeric_limits<T>::lowest();
        }
        return result;
    }
}
```

**Why this compiles to optimal code:**

The compiler sees only ONE branch after template instantiation. For `checked_add<SaturatingPolicy>(a, b)`:

```asm
; x86-64 with GCC -O3
add     eax, edx          ; a + b
jo      .saturate         ; Jump if overflow flag set
ret
.saturate:
mov     eax, 0x7FFFFFFF   ; Return INT_MAX
ret
```

The `jo` instruction checks the CPU's overflow flag—**zero overhead** compared to manual overflow checking.

### Compiler Builtin Acceleration

```cpp
#if HAS_BUILTIN_OVERFLOW
    T result;
    if (__builtin_add_overflow(a, b, &result)) {
        // Handle overflow - CPU flag already set
    }
#else
    // Portable fallback: pre-check before operation
    bool overflow = (b > 0 && a > max - b) || (b < 0 && a < min - b);
    T result = a + b;
#endif
```

| Path | Mechanism | Performance |
|------|-----------|-------------|
| Builtin (GCC/Clang) | Single `jo` instruction | 2-5 ns |
| Fallback | Pre-computation check | 10-20 ns |
| Unchecked | Raw arithmetic | ~1 ns |

---

## Feature Inventory

### 1. Policy-Based Error Handling (The Core Differentiator)

Same operation, four behaviors—all zero-overhead:

```cpp
int32_t a = INT32_MAX, b = 1;

// Policy 1: Throw on error (default)
try {
    auto result = checked_add(a, b);  // Throws
} catch (...) { /* handle */ }

// Policy 2: Return Expected (no-throw, monadic)
auto result = checked_add<ReturnExpectedPolicy>(a, b);
if (!result) {
    // result.error() == MathError::Overflow
}

// Policy 3: Saturate (DSP/audio workloads)
auto clamped = checked_add<SaturatingPolicy>(a, b);  // INT32_MAX

// Policy 4: Tolerate infinity (scientific computing)
double x = DBL_MAX, y = DBL_MAX;
auto inf_ok = checked_add_fp<InfTolerantPolicy>(x, y);  // +Inf (not error)
```

**Mechanism:** The `noexcept` specifier is **conditional**—`ReturnExpectedPolicy` and `SaturatingPolicy` are `noexcept(true)`, enabling different optimization paths.

### 2. SIMD-Vectorized Checked Operations

CheckedArithmetic provides batch operations for both integer and floating-point vectors, with separate function families for type safety:

#### Integer Vectors (AVX2-Accelerated)

```cpp
#include "CheckedArithmeticInt.h"

std::vector<int32_t> a = {1, 2, INT_MAX, 4, 5, 6, 7, 8};
std::vector<int32_t> b = {1, 1, 1, 1, 1, 1, 1, 1};

// Integer vector operations: checked_add_vec, checked_sub_vec, checked_mul_vec
auto result = checked_add_vec<ThrowOnErrorPolicy>(a, b);  // Throws at element 2

auto result2 = checked_add_vec<ReturnExpectedPolicy>(a, b);
// result2.error() == MathError::Overflow
```

**Mechanism:** AVX2 intrinsics with overflow detection via sign-bit XOR:
```cpp
__m256i sum = _mm256_add_epi32(va, vb);
__m256i overflow = _mm256_cmpgt_epi32(_mm256_xor_si256(va, sum), 
                                       _mm256_xor_si256(va, vb));
```

#### Floating-Point Vectors (NaN/Inf Detection)

```cpp
#include "CheckedArithmeticFP.h"

std::vector<double> a = {1.0, 2.0, DBL_MAX, 4.0};
std::vector<double> b = {1.0, 1.0, DBL_MAX, 1.0};

// FP vector operations: checked_add_vec_fp, checked_sub_vec_fp, 
//                       checked_mul_vec_fp, checked_div_vec_fp
auto result = checked_add_vec_fp<ThrowOnErrorPolicy>(a, b);  // Throws (Inf at element 2)

auto result2 = checked_mul_vec_fp<ReturnExpectedPolicy>(a, b);
// Detects NaN/Inf in results
```

#### Aligned Vector Variants (For HpcVector/AlignedVector)

```cpp
#include "CheckedArithmeticFP.h"

fat_p::HpcVector<double> a(1000), b(1000);
// ... fill vectors ...

// Aligned variants preserve container type and work with custom allocators
auto result = checked_add_vec_fp_aligned<SaturatingPolicy, double, 
                                          fat_p::HpcVector<double>>(a, b);
// result is HpcVector<double>, maintains NUMA locality
```

**Vector Function Summary:**

| Function | Type | SIMD Backend | Use Case |
|----------|------|--------------|----------|
| `checked_add_vec` | Integer | AVX2 | Batch integer arithmetic |
| `checked_sub_vec` | Integer | AVX2 | Batch integer arithmetic |
| `checked_mul_vec` | Integer | AVX2 | Batch integer arithmetic |
| `checked_add_vec_fp` | Float/Double | Scalar + NaN/Inf check | Batch FP with std::vector |
| `checked_sub_vec_fp` | Float/Double | Scalar + NaN/Inf check | Batch FP with std::vector |
| `checked_mul_vec_fp` | Float/Double | Scalar + NaN/Inf check | Batch FP with std::vector |
| `checked_div_vec_fp` | Float/Double | Scalar + div-zero check | Batch FP division |
| `checked_*_vec_fp_aligned` | Float/Double | Scalar + checks | AlignedVector/HpcVector |

**Performance:** Integer SIMD paths process multiple elements per instruction via packed arithmetic, providing significant throughput over scalar one-at-a-time checking. FP operations include per-element NaN/Inf validation. See `components/CheckedArithmetic/results/` for current platform-specific benchmark data.

### 3. Floating-Point Anomaly Detection

Beyond integer overflow—detect NaN, Inf, and invalid operations:

```cpp
double a = 1.0, b = 0.0;

// Division by zero handling
auto result = checked_div_fp<ReturnExpectedPolicy>(a, b);
// result.error() == MathError::DivByZero

// NaN input detection
double nan = std::nan("");
auto bad = checked_add_fp<ReturnExpectedPolicy>(nan, 1.0);
// bad.error() == MathError::NaN

// Inf ± Inf = NaN detection
double inf = std::numeric_limits<double>::infinity();
auto undefined = checked_sub_fp<ReturnExpectedPolicy>(inf, inf);
// undefined.error() == MathError::NaN
```

### 4. Comprehensive Operation Coverage

```cpp
// Integer operations (CheckedArithmeticInt.h)
checked_add<Policy>(a, b);        // Integer addition with overflow detection
checked_sub<Policy>(a, b);        // Integer subtraction
checked_mul<Policy>(a, b);        // Integer multiplication
checked_div<Policy>(a, b);        // Catches b==0, INT_MIN/-1
checked_shl<Policy>(val, shift);  // Catches shift >= bit_width
checked_abs<Policy>(val);         // Catches abs(INT_MIN) overflow
checked_neg<Policy>(val);         // Catches -INT_MIN overflow

// Floating-point operations (CheckedArithmeticFP.h)
checked_add_fp<Policy>(a, b);     // FP addition with NaN/Inf detection
checked_sub_fp<Policy>(a, b);     // FP subtraction
checked_mul_fp<Policy>(a, b);     // FP multiplication
checked_div_fp<Policy>(a, b);     // FP division with div-zero detection
```

### 5. Constexpr Compile-Time Checking

```cpp
constexpr auto safe = checked_add(100, 200);       // OK: 300
constexpr auto overflow = checked_add(INT_MAX, 1); // Compile error!
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::add_sat (C++26) | Why Not Boost.SafeNumerics | Fat-P Advantage |
|----------------|------------------------------|----------------------------|-----------------|
| Policy flexibility | ❌ Only saturates | ❌ Only throws | ✅ Four policies, compile-time |
| Expected returns | ❌ Not available | ❌ Not available | ✅ Full Expected integration |
| SIMD vectors | ❌ Not available | ❌ Not available | ✅ AVX2-optimized integer batch |
| Floating-point | ❌ Not applicable | ❌ Limited | ✅ Full NaN/Inf handling |
| Aligned containers | ❌ Not available | ❌ Not available | ✅ `_aligned` variants for HpcVector |
| C++17 support | ❌ C++26 required | ✅ C++14+ | ✅ C++17 with C++20 concepts |

**The Sweet Spot:** Fat-P CheckedArithmetic is the only option combining policy-based error handling, Expected integration, SIMD acceleration, floating-point coverage, and aligned container support—all with zero external dependencies.

---

## The "Forever Stuck" Reality

**Compiler Reality Check:** Even when C++26 ships `std::add_sat`, your codebase gets **one behavior**: saturation. If your trading system needs to throw on overflow (regulatory requirement) while your audio DSP needs to saturate (artistic requirement), the standard forces you to write two codepaths.

CheckedArithmetic provides **architectural control** that the standard committee cannot mandate because different domains need different policies. This isn't waiting for the standard—it's providing what the standard will never offer.

---

## Performance Characteristics

| Operation | Builtin Path | Fallback Path | Unchecked | Mechanism |
|-----------|-------------|---------------|-----------|-----------|
| `checked_add` | 2-5 ns | 10-20 ns | ~1 ns | `__builtin_add_overflow` → `jo` |
| `checked_mul` | 3-7 ns | 15-30 ns | ~1 ns | `__builtin_mul_overflow` → flags |
| `checked_div` | 15-25 ns | 20-35 ns | ~15 ns | Pre-check + division |
| `checked_add_vec` (×8 int) | 4-8 ns | N/A | ~2 ns | AVX2 + overflow detect |
| `checked_add_vec_fp` (×8 double) | 15-25 ns | N/A | ~8 ns | Scalar + NaN/Inf check |

### Where Fat-P Wins
- Mixed-policy codebases (throw here, saturate there)
- No-throw contexts requiring Expected integration
- Batch integer processing with AVX2 acceleration
- Floating-point scientific computing with NaN/Inf handling
- HPC containers needing aligned vector operations

### Where Fat-P Loses (Honesty Builds Trust)
- Single-policy codebases where `std::add_sat` suffices
- Performance-critical inner loops where 2-5 ns overhead matters
- Codebases already using Boost.SafeNumerics with acceptable behavior
- FP vector operations without SIMD (scalar with per-element checks)

---

## Integration Points

```
CheckedArithmetic.h (umbrella header)
    ↓ includes
CheckedArithmeticInt.h    (Integer operations + SIMD vectors)
CheckedArithmeticFP.h     (FP operations + aligned variants)
CheckedArithmetic_IntSimd.h (AVX2/SSE2/NEON backends)
    ↓ uses
enforce.h                 (ThrowOnErrorPolicy implementation)
Expected.h                (ReturnExpectedPolicy return type)
CppStandardDetection.h    (C++20 concepts conditional)
    ↓ used by
SmallVector.h             (overflow-safe capacity growth)
StrongId.h                (checked ID arithmetic)
IdGenerator.h             (safe counter increment)
SimdVector.h              (has_nan/has_inf integration)
```

---

## Final Assessment

CheckedArithmetic delivers on the fat_p promise through three pillars:

### 1. Permanence
C++26's `std::add_sat` provides one behavior. CheckedArithmetic provides four, selected at compile time. This architectural flexibility will **never** be in the standard because different domains need different policies.

### 2. Specialization
SIMD-accelerated integer batch operations, floating-point anomaly detection, aligned container variants, and `constexpr` compile-time checking address HPC needs that generic safety libraries ignore. The builtin path generates assembly identical to hand-tuned overflow checks.

### 3. Control
The policy template parameter puts error handling decisions in the architect's hands—not the library author's. Throw for debug builds, return Expected for production, saturate for DSP—same code, different instantiations. Separate `_vec` (integer) and `_vec_fp` (floating-point) families provide type-safe batch operations.

**Architectural Verdict:** CheckedArithmetic transforms arithmetic safety from **single-behavior wrappers** to a **compile-time policy framework**. The generated assembly is identical to hand-written overflow checks, but the source code expresses intent through policy selection rather than scattered conditionals.

---

*CheckedArithmetic.h + CheckedArithmeticInt.h + CheckedArithmeticFP.h — Fat-P Library*
