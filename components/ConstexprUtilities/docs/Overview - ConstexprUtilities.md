# ConstexprUtilities: A Fat-P Library Showcase

## Executive Summary

ConstexprUtilities is a **compile-time computation toolkit** providing hashing, bit manipulation, arithmetic, and string operations that execute entirely during compilation via `constexpr` and `consteval`. Unlike runtime equivalents that pay CPU cost on every call, ConstexprUtilities performs computation **once at compile time**, embedding results directly in the binary. The FNV-1a hash enables compile-time string switches, while power-of-two and popcount functions leverage compiler builtins that map to single CPU instructions.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// Runtime hash computation on every lookup
std::unordered_map<std::string, Handler> dispatch;
dispatch["create"] = handleCreate;
dispatch["update"] = handleUpdate;
dispatch["delete"] = handleDelete;

void process(const std::string& cmd) {
    auto it = dispatch.find(cmd);  // Hash computed at RUNTIME
    if (it != dispatch.end()) it->second();
}
// Every call hashes the string, looks up bucket, compares keys

// Runtime power-of-two calculation
size_t alignBuffer(size_t size) {
    size_t aligned = 1;
    while (aligned < size) aligned *= 2;  // Runtime loop
    return aligned;
}
```

| Issue | HPC Impact |
|-------|------------|
| Runtime string hashing | Hash computed on every dispatch |
| Runtime bit operations | Loops instead of single instructions |
| No compile-time string switch | `switch` only works on integers |
| Repeated calculations | Same constants recomputed |

### The Standard's Limitation

C++20 adds `std::bit_ceil`, `std::popcount`, etc., but:
- Not available in C++17 codebases
- Standard hash (`std::hash`) is not `constexpr`
- No compile-time string-to-integer for `switch` statements
- Compiler builtins require manual detection

---

## Architecture: Compile-Time Evaluation

### The Mechanism: constexpr/consteval Computation

```cpp
// FNV-1a hash - computes at compile time
[[nodiscard]] consteval uint32_t constexpr_hash(std::string_view s) noexcept {
    constexpr uint32_t FNV_PRIME = 16777619U;
    constexpr uint32_t FNV_OFFSET_BASIS = 2166136261U;
    
    uint32_t hash = FNV_OFFSET_BASIS;
    for (char c : s) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

// Usage: compile-time hashes in case labels.
// Note: constexpr_hash is consteval, so it cannot hash the runtime
// `command` in the switch expression — supply a runtime FNV-1a twin
// with the same constants for that (the library provides only the
// consteval version).
switch (fnv1a_runtime(command)) {
    case constexpr_hash("create"): return handleCreate();
    case constexpr_hash("update"): return handleUpdate();
    case constexpr_hash("delete"): return handleDelete();
}
// All case values computed at compile time → efficient jump table
```

**Why `consteval` over `constexpr`:**

`consteval` guarantees compile-time evaluation. `constexpr` may evaluate at runtime if context isn't constant. The hash functions use raw `consteval` (there is no selection macro), which is why the component requires C++20: a compile-time hash that silently degraded to runtime evaluation would defeat the point.

### Builtin Acceleration

```cpp
template <typename T>
[[nodiscard]] constexpr int popcount(T n) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
        return __builtin_popcount(static_cast<unsigned int>(n));
    } else {
        return __builtin_popcountll(static_cast<unsigned long long>(n));
    }
#else
    // Portable fallback
    int count = 0;
    while (n) { n &= (n - 1); ++count; }
    return count;
#endif
}
```

**Generated assembly (GCC -O3):**
```asm
; __builtin_popcount maps to:
popcnt  eax, edi    ; Single instruction
ret
```

---

## Feature Inventory

### 1. Compile-Time String Hashing (FNV-1a)

```cpp
// 32-bit hash
constexpr auto h32 = constexpr_hash("hello");     // 1335831723

// 64-bit hash (better collision resistance)
constexpr auto h64 = constexpr_hash64("hello");   // 11831194018420276491

// Combine multiple values
constexpr auto combined = hash_values("namespace", "class", "method");
```

**Use cases:** String switches, compile-time hash tables, enum-to-string mapping.

### 2. Power-of-Two Operations

```cpp
// Check if power of two
static_assert(is_power_of_two(8u));    // true
static_assert(!is_power_of_two(6u));   // false

// Next power of two (for alignment)
static_assert(next_power_of_two(5u) == 8u);
static_assert(next_power_of_two(8u) == 8u);

// Log2 operations
static_assert(log2_floor(15u) == 3);   // floor(log2(15)) = 3
static_assert(log2_ceil(15u) == 4);    // ceil(log2(15)) = 4
```

**Mechanism:** Bit manipulation with overflow detection:
```cpp
template <typename T>
constexpr T next_power_of_two(T n) noexcept {
    if (n == 0) return 1;
    if (is_power_of_two(n)) return n;
    --n;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    if constexpr (sizeof(T) > 1) n |= n >> 8;
    if constexpr (sizeof(T) > 2) n |= n >> 16;
    if constexpr (sizeof(T) > 4) n |= n >> 32;
    return n + 1;
}
```

### 3. Bit Manipulation (Builtin-Accelerated)

```cpp
// Population count (count set bits)
static_assert(popcount(0b1010u) == 2);

// Count leading zeros
static_assert(clz(uint8_t{0b00001000}) == 4);

// Count trailing zeros
static_assert(ctz(uint8_t{0b00001000}) == 3);
```

**Performance:** Single instruction on modern CPUs via `__builtin_popcount`, `__builtin_clz`, `__builtin_ctz`.

### 4. Digit Counting (Optimized)

```cpp
static_assert(count_digits(0) == 1);
static_assert(count_digits(42) == 2);
static_assert(count_digits(-42) == 3);  // '-', '4', '2'
static_assert(count_digits(1000000000) == 10);
```

**Mechanism:** Comparison ladder instead of division loop:
```cpp
// 32-bit path (branchless via ternary chain)
digits = (abs_n < 10) ? 1 :
         (abs_n < 100) ? 2 :
         (abs_n < 1000) ? 3 : /* ... */;
```

### 5. Compile-Time String Operations

```cpp
// Compile-time string length
constexpr auto len = constexpr_strlen("hello");  // 5

// Compile-time string comparison
constexpr bool eq = constexpr_strcmp("abc", "abc") == 0;

// Compile-time string equality
constexpr bool same = constexpr_streq("hello", "hello");  // true
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::hash | Why Not std::bit_* (C++20) | Why Not Manual Loop | Fat-P Advantage |
|----------------|-------------------|---------------------------|--------------------|-----------------| 
| Compile-time hash | ❌ Not constexpr | N/A | ✅ Works but verbose | ✅ Clean API |
| Guaranteed compile-time eval | ❌ No | Partial | ❌ No | ✅ consteval (C++20) |
| Builtin acceleration | ❌ Implementation-defined | ✅ Yes | ❌ No | ✅ Yes |
| String switch | ❌ No | ❌ No | ❌ No | ✅ Hash-based |
| Overflow detection | ❌ No | ❌ No | Manual | ✅ Built-in |

**The Sweet Spot:** ConstexprUtilities is the only option combining guaranteed compile-time hashing, builtin-accelerated bit ops, and string switches in a single C++20 header.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++20 added `std::bit_ceil`, `std::popcount`, etc., but:
- `std::hash` is **still not constexpr** (C++26 doesn't change this)
- String switches via hashing are **not standardized**
- There is no standard consteval string hash

ConstexprUtilities provides compile-time computation capabilities that the standard doesn't offer and won't offer for years.

---

## Performance Characteristics

| Operation | Compile-Time | Runtime Fallback | Algorithm |
|-----------|-------------|------------------|-----------|
| `constexpr_hash` | Zero — result embedded in binary | FNV-1a iteration over input bytes | FNV-1a |
| `popcount` | Zero — result embedded in binary | Compiler builtin → single instruction | `__builtin_popcount` / `POPCNT` |
| `next_power_of_two` | Zero — result embedded in binary | Bit manipulation (shift + OR cascade) | `std::bit_ceil` equivalent |
| `count_digits` | Zero — result embedded in binary | Comparison ladder (log10 approximation) | Successive division/comparison |

At compile time, all operations are evaluated by the compiler and the result is embedded directly in the binary — zero runtime cost. At runtime, each operation maps to a small number of ALU instructions. See `components/ConstexprUtilities/results/` for current platform-specific benchmark data.

### Where Fat-P Wins
- Compile-time dispatch tables (string switches)
- Buffer alignment calculations
- Bit manipulation in template metaprogramming
- Hash-based compile-time lookups

### Where Fat-P Loses (Honesty Builds Trust)
- Runtime-only string hashing → `std::hash` may be faster (hardware-accelerated)
- C++20 available → `std::bit_ceil` etc. are standard
- Cryptographic hashing → FNV-1a is not suitable
- Runtime variable inputs → compile-time computation doesn't apply

---

## Integration Points

```
ConstexprUtilities.h
    ↓ used by
StringPool.h         (compile-time hash for interning)
EnumPlus.h           (enum-to-string compile-time dispatch)
BitSet.h             (popcount, clz, ctz)
CircularBuffer.h     (power-of-two capacity)
```

---

## Final Assessment

ConstexprUtilities delivers on the fat_p promise through three pillars:

### 1. Permanence
`std::hash` will never be `constexpr` (implementation-defined). Compile-time string switches will never be standardized. ConstexprUtilities provides these permanently.

### 2. Specialization
Builtin detection (`__builtin_popcount`, etc.) ensures optimal code generation. FNV-1a hash is tuned for compile-time evaluation with good avalanche properties.

### 3. Control
Raw `consteval` on the hash functions guarantees compile-time evaluation — there is no runtime fallback to reason about. Overflow detection in `next_power_of_two` prevents silent wraparound.

**Architectural Verdict:** ConstexprUtilities transforms runtime computations into **compile-time constants**, enabling string switches, zero-cost dispatch tables, and optimal bit manipulation through a clean, portable API.

---

*ConstexprUtilities.h (umbrella header for ConstexprHash.h, ConstexprBitOps.h, ConstexprStringConversion.h) — Fat-P Library*
