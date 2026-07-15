# SimdVector: A Fat-P Library Showcase

## Executive Summary

SimdVector is a **portable SIMD abstraction layer** that eliminates architecture-specific intrinsics from your source code. Unlike raw intrinsics (platform-locked, verbose, error-prone) or `std::simd` (C++26, unavailable today), SimdVector provides **lane-width-agnostic operations** that compile to optimal instructions on SSE2, AVX, AVX-512, and ARM NEON. The same code runs on a Raspberry Pi and a Xeon server—the compiler generates the right instructions, and `SimdVector<float>::width` tells you how many elements process in parallel.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The intrinsics nightmare: platform-locked, verbose, fragile
#ifdef __AVX__
    __m256 va = _mm256_loadu_ps(a);
    __m256 vb = _mm256_loadu_ps(b);
    __m256 vc = _mm256_add_ps(va, vb);
    _mm256_storeu_ps(result, vc);
#elif defined(__SSE2__)
    __m128 va = _mm_loadu_ps(a);
    __m128 vb = _mm_loadu_ps(b);
    __m128 vc = _mm_add_ps(va, vb);
    _mm_storeu_ps(result, vc);
#elif defined(__ARM_NEON)
    float32x4_t va = vld1q_f32(a);
    float32x4_t vb = vld1q_f32(b);
    float32x4_t vc = vaddq_f32(va, vb);
    vst1q_f32(result, vc);
#else
    for (int i = 0; i < 4; ++i) result[i] = a[i] + b[i];
#endif
// 15 lines for a vector add. Repeat for every operation.
```

| Issue | HPC Impact |
|-------|------------|
| Platform-specific intrinsics | Code locked to one architecture; cross-platform requires `#ifdef` forests |
| Manual width management | AVX processes 8 floats, SSE processes 4—you track the difference |
| Type unsafety | Intrinsics accept raw pointers; nothing prevents passing misaligned data |
| No mask abstraction | Comparisons return opaque bitmasks; blend/select requires more intrinsics |
| Verbose scalar tails | Remainder loops after SIMD section need separate code |

### The Standard's Limitation

C++26 proposes `std::simd`, which solves many of these problems—**but it doesn't exist today**.

```cpp
// C++26 std::simd (draft)
std::simd<float> va, vb;
va.copy_from(a, std::element_aligned);
vb.copy_from(b, std::element_aligned);
auto vc = va + vb;
vc.copy_to(result, std::element_aligned);
```

**When you'll get this:**
- Clang: Experimental in libc++ trunk
- GCC: Partial in GCC 13+
- MSVC: No timeline announced
- Production HPC clusters: Years away (RHEL 7/8, GCC 7.x for CUDA compatibility)

**What `std::simd` won't provide:** Integration with checked arithmetic. When your simulation needs to detect NaN propagation or overflow in SIMD code, `std::simd` offers no policy hooks. Fat-P's SimdVector integrates with `CheckedArithmetic` for safety-critical vector operations.

---

## Architecture: Lane-Width Abstraction

### The Mechanism: Compile-Time Width Resolution

```cpp
template<typename T>
class SimdVector {
public:
    // Width determined by architecture at compile time
    static constexpr size_t width = /* 16 for AVX-512 float, 8 for AVX, 4 for SSE/NEON */;
    static constexpr size_t alignment = /* 64 for AVX-512, 32 for AVX, 16 for SSE/NEON */;
    
private:
    // Native register type selected at compile time
#if defined(SIMD_AVX512)
    __m512 data_;  // 16 floats
#elif defined(SIMD_AVX)
    __m256 data_;  // 8 floats
#elif defined(SIMD_SSE2)
    __m128 data_;  // 4 floats
#elif defined(SIMD_NEON)
    float32x4_t data_;  // 4 floats
#else
    std::array<T, 4> data_;  // Scalar fallback
#endif
};
```

**Why this works:**

1. **Single source code:** You write `SimdVector<float> v;` once. The compiler instantiates the right type.

2. **Width-agnostic loops:** Use `SimdVector<float>::width` to stride through data:
   ```cpp
   for (size_t i = 0; i < n; i += SimdVector<float>::width) {
       auto v = SimdVector<float>::load_aligned(data + i);
       // Process...
   }
   ```

3. **Optimal codegen:** The abstraction compiles away. `v + w` becomes `_mm256_add_ps` on AVX, `vaddq_f32` on NEON.

### Backend Selection Table

| Architecture | Instruction Set | float width | double width | Alignment |
|--------------|-----------------|-------------|--------------|-----------|
| x86-64 + AVX-512 | AVX-512F | 16 | 8 | 64 bytes |
| x86-64 + AVX/AVX2 | AVX | 8 | 4 | 32 bytes |
| x86-64 baseline | SSE2 | 4 | 2 | 16 bytes |
| AArch64 (64-bit ARM) | NEON | 4 | 2 | 16 bytes |
| ARMv7 (32-bit ARM) | NEON | 4 | 1 (scalar) | 16 bytes |
| Scalar fallback | — | 1 | 1 | — |

**Note:** Double-precision NEON requires AArch64. On 32-bit ARM, `SimdVector<double>::width == 1` (scalar).

---

## Feature Inventory

### 1. Portable Load/Store Operations

```cpp
fat_p::AlignedVector<float, 64> data(1024);
const float* ptr = data.assume_aligned();

// Aligned load (pointer must be aligned to SimdVector<float>::alignment)
auto v = SimdVector<float>::load_aligned(ptr);

// Unaligned load (works with any pointer)
auto v2 = SimdVector<float>::load_unaligned(some_pointer);

// Partial load (fewer than width elements remaining)
auto v3 = SimdVector<float>::load_partial(ptr, 3);  // Load 3 elements, zero rest

// Store operations
v.store_aligned(ptr);
v.store_unaligned(some_pointer);
v.store_partial(ptr, 3);  // Store only first 3 elements
```

**Why aligned loads matter:** On x86, `vmovaps` (aligned) is not faster than `vmovups` (unaligned) on modern CPUs—**unless the compiler knows about alignment**. The aligned load tells the compiler "this pointer is aligned," enabling further optimizations (no peeling loops, better instruction scheduling).

### 2. Arithmetic Operations

```cpp
SimdVector<float> a = SimdVector<float>::load_aligned(data_a);
SimdVector<float> b = SimdVector<float>::load_aligned(data_b);

// Element-wise arithmetic
auto sum = a + b;
auto diff = a - b;
auto prod = a * b;
auto quot = a / b;

// Compound assignment
a += b;
a *= SimdVector<float>(2.0f);  // Broadcast constructor

// Unary operations
auto neg = -a;
auto sq = a.sqrt();
auto ab = a.abs();
```

### 3. Fused Multiply-Add (FMA)

```cpp
// FMA: a * b + c with single rounding (more accurate than separate mul + add)
auto result = SimdVector<float>::fma(a, b, c);  // a*b + c

// FMS: a * b - c
auto result2 = SimdVector<float>::fms(a, b, c);  // a*b - c
```

**Why FMA matters:**
1. **Single rounding error** instead of two (mul then add)
2. **Higher throughput** on modern CPUs (one instruction instead of two)
3. **Numerical stability** for dot products, polynomial evaluation

### 4. Comparison and Masking

```cpp
SimdVector<float> a, b;

// Comparisons return SimdMask<float>
SimdMask<float> mask = (a > b);

// Mask queries
bool any_greater = mask.any();   // True if any lane is true
bool all_greater = mask.all();   // True if all lanes are true
bool none_greater = mask.none(); // True if no lane is true
int count = mask.popcount();     // Number of true lanes

// Conditional select (branchless)
auto result = SimdVector<float>::select(mask, a, b);  // mask ? a : b per lane
```

**The select pattern:** This is the SIMD equivalent of `if/else`—no branches, no pipeline stalls:

```cpp
// Scalar (branchy, slow)
for (size_t i = 0; i < n; ++i) {
    result[i] = (a[i] > b[i]) ? a[i] : b[i];  // Branch per element
}

// SIMD (branchless, fast)
auto va = SimdVector<float>::load_aligned(a);
auto vb = SimdVector<float>::load_aligned(b);
auto result = SimdVector<float>::select(va > vb, va, vb);  // No branches
```

### 5. Horizontal Reductions

```cpp
SimdVector<float> v = /* ... */;

float sum = v.horizontal_sum();  // Sum all lanes
float max = v.horizontal_max();  // Maximum across lanes
float min = v.horizontal_min();  // Minimum across lanes
```

**Performance note:** Horizontal operations are expensive (they cross lanes). Prefer vertical operations when possible. Use horizontal reductions only for final accumulation.

### 6. Special Value Detection (CheckedArithmetic Integration)

```cpp
SimdVector<float> v = /* ... */;

bool has_nan = v.has_nan();    // Any lane is NaN?
bool has_inf = v.has_inf();    // Any lane is ±Inf?
bool finite = v.all_finite();  // All lanes finite?
```

**Use case:** Detect numerical issues in SIMD code without per-element checks:

```cpp
auto result = SimdVector<float>::fma(a, b, c);
if (result.has_nan()) {
    // Handle numerical failure
}
```

### 7. Factory Functions

```cpp
// Broadcast single value to all lanes (explicit constructor)
auto v = SimdVector<float>(3.14f);             // [3.14, 3.14, 3.14, ...]

// Special values
auto zeros = SimdVector<float>::zero();        // [0, 0, 0, ...]
auto ones = SimdVector<float>::ones();         // [1, 1, 1, ...]
auto inf = SimdVector<float>::infinity();      // [+Inf, +Inf, ...]
auto neg_inf = SimdVector<float>::neg_infinity();
```

---

## Why Not Alternatives?

| If You Need... | Why Not Raw Intrinsics | Why Not Vc/Highway | Why Not std::simd | Fat-P Advantage |
|----------------|------------------------|--------------------|--------------------|-----------------|
| Portable code | Platform-locked | External dependency | C++26, unavailable | Single header, works today |
| Mask abstraction | Opaque bitmasks | Yes, but heavy | Yes | Clean SimdMask type |
| NaN/Inf detection | Manual per-element | Not integrated | Not integrated | `has_nan()`, `all_finite()` |
| CheckedArithmetic | Not available | Not available | Not available | Policy integration |
| Zero dependencies | N/A | Requires Vc/Highway | Requires C++26 stdlib | Standard library only |

**The Sweet Spot:** SimdVector provides portable SIMD with clean masking, special value detection, and CheckedArithmetic integration—all in a single header with zero dependencies.

---

## The "Forever Stuck" Reality

**Compiler Reality Check:** `std::simd` is C++26 at the earliest. Even then:
- It won't integrate with checked arithmetic
- It won't detect NaN/Inf at the vector level
- HPC clusters typically lag years behind the newest standard for CUDA/driver compatibility, putting C++26 far away

SimdVector provides portable SIMD **today**, with safety features the standard will never mandate.

**Integer SIMD:** SimdVector deliberately supports **only float and double**. Integer overflow detection lacks universal hardware semantics—different CPUs handle it differently. For checked integer vectors, use `CheckedArithmetic.h` which provides `checked_add_vec`, `checked_mul_vec` with AVX2 acceleration and per-element overflow detection.

---

## Performance Characteristics

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| `load_aligned` | O(1) | Single `vmovaps`/`vld1q` instruction |
| `load_unaligned` | O(1) | Single `vmovups`/`vld1q` instruction |
| `load_partial` | O(width) | Masked load or element-by-element |
| Arithmetic (+, -, *, /) | O(1) | Single SIMD instruction |
| `fma` | O(1) | Single FMA instruction (where available) |
| Comparison | O(1) | Single compare instruction |
| `select` | O(1) | Single blend instruction |
| `horizontal_sum` | O(log width) | Shuffle + add sequence |

### Where Fat-P Wins

**Cross-platform code:** Write once, run on x86 and ARM with optimal codegen.

**Numerical validation:** Detect NaN/Inf propagation in SIMD code without per-element overhead.

**Integration:** Works seamlessly with `AlignedVector::assume_aligned()` for end-to-end optimization.

### Where Fat-P Loses (Honesty Builds Trust)

**Advanced intrinsics:** SimdVector doesn't expose gather/scatter, permute, or other advanced instructions. For those, you need raw intrinsics.

**Integer SIMD:** SimdVector is FP-only. Use CheckedArithmetic for integer vectors with overflow detection.

**Exotic architectures:** Only x86 (SSE2/AVX/AVX-512) and ARM (NEON) are supported. No RISC-V, no IBM POWER.

**Maximum performance:** For the last 5% of performance, expert hand-tuning with intrinsics may still win.

---

## Integration Points

```
SimdVector.h
    ↓ uses
enforce.h              (Bounds checking on partial load/store)
    ↓ integrates with
AlignedVector.h        (assume_aligned() provides aligned pointers)
HpcVector.h            (NUMA-local aligned storage for SIMD)
CheckedArithmetic.h    (Integer SIMD with overflow detection)
```

**Typical usage pattern:**

```cpp
fat_p::HpcVector<float> data(1000000);  // NUMA-local, 64-byte aligned

const float* ptr = data.assume_aligned();
for (size_t i = 0; i < data.size(); i += SimdVector<float>::width) {
    auto v = SimdVector<float>::load_aligned(ptr + i);
    auto result = SimdVector<float>::fma(v, scale, offset);
    if (result.has_nan()) {
        // Handle error
    }
    result.store_aligned(ptr + i);
}
```

---

## Final Assessment

SimdVector delivers on the fat_p promise through three pillars:

### 1. Permanence
`std::simd` is years away for production HPC clusters. SimdVector provides portable SIMD today, and its NaN/Inf detection and CheckedArithmetic integration will remain valuable even after the standard arrives.

### 2. Specialization
FP-only design enables clean semantics. Integer overflow detection has different requirements (see CheckedArithmetic). The `has_nan()`, `has_inf()`, and `all_finite()` methods address numerical validation that generic SIMD libraries ignore.

### 3. Control
Explicit aligned vs. unaligned loads, partial load/store for tail handling, and mask-based selection give you precise control over memory access and conditional logic without platform-specific code.

**Architectural Verdict:** SimdVector transforms SIMD programming from **platform-specific intrinsic soup** to **portable, type-safe vector operations**—with numerical validation that makes it suitable for safety-critical HPC code.

---

*SimdVector.h (1242 lines) — Fat-P Library*
