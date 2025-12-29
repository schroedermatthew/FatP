# TypeTraits: A Fat-P Library Showcase

## Executive Summary

TypeTraits is a **unified C++ standard detection and compatibility layer** that normalizes compiler-specific version macros, feature detection, and attribute support into consistent, portable macros. Unlike checking `__cplusplus` directly (broken on MSVC), scattered `#ifdef` checks (error-prone and verbose), or Boost.Config (heavyweight dependency), TypeTraits provides **single-header detection** for C++17/20/23 features, compiler-specific attributes, and platform characteristics with **zero runtime overhead**.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The MSVC trap: __cplusplus lies!
#if __cplusplus >= 202002L  // C++20
    // MSVC reports 199711L by default even in C++20 mode!
    // Must check _MSVC_LANG instead
#endif

// The attribute minefield
#if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
        #define NODISCARD [[nodiscard]]
    #else
        #define NODISCARD
    #endif
#else
    #define NODISCARD
#endif
// Repeat for likely, unlikely, no_unique_address, ...

// The feature detection sprawl
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    // Has concepts
#elif defined(__cpp_lib_concepts)
    // Library concepts
#else
    // No concepts
#endif
// Scattered throughout codebase, inconsistent
```

| Issue | HPC Impact |
|-------|------------|
| `__cplusplus` unreliable | MSVC requires `/Zc:__cplusplus` or `_MSVC_LANG` check |
| Scattered feature checks | Inconsistent, duplicated, error-prone |
| Attribute portability | `[[likely]]` doesn't exist pre-C++20 |
| Platform detection | Windows/Linux/ARM64/x64 checks everywhere |

### The Standard's Limitation

The C++ standard defines feature-test macros (`__cpp_concepts`, `__cpp_lib_ranges`, etc.) but:
- MSVC's `__cplusplus` is broken by default
- Attribute detection requires `__has_cpp_attribute` which may not exist
- No unified "is this C++20?" check that works everywhere

---

## Architecture: Normalized Detection Macros

### The Mechanism: Compiler Normalization

```cpp
// Step 1: Detect actual C++ version (MSVC workaround)
#if defined(_MSVC_LANG)
    #define FATP_CPLUSPLUS _MSVC_LANG
#else
    #define FATP_CPLUSPLUS __cplusplus
#endif

// Step 2: Version macros
#define FATP_HAS_CPP17 (FATP_CPLUSPLUS >= 201703L)
#define FATP_HAS_CPP20 (FATP_CPLUSPLUS >= 202002L)
#define FATP_HAS_CPP23 (FATP_CPLUSPLUS >= 202302L)

// Step 3: Feature detection with fallbacks
#if defined(__cpp_concepts) && FATP_HAS_CPP20
    #define FATP_HAS_CONCEPTS 1
#else
    #define FATP_HAS_CONCEPTS 0
#endif
```

**Why this matters:** A single `#if FATP_HAS_CPP20` works on GCC, Clang, and MSVC without special flags or workarounds.

---

## Feature Inventory

### 1. C++ Standard Version Detection

```cpp
#if FATP_HAS_CPP23
    // Use C++23 features
#elif FATP_HAS_CPP20
    // Use C++20 features
#elif FATP_HAS_CPP17
    // Baseline: C++17
#else
    #error "Requires C++17 or later"
#endif
```

**Macro list:**
- `FATP_HAS_CPP17` — C++17 or later
- `FATP_HAS_CPP20` — C++20 or later
- `FATP_HAS_CPP23` — C++23 or later

### 2. Feature-Specific Detection

```cpp
// Concepts
#if FATP_HAS_CONCEPTS
template<std::integral T>
void process(T value);
#else
template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void process(T value);
#endif

// Ranges
#if FATP_HAS_RANGES
    auto result = data | std::views::filter(pred) | std::views::transform(func);
#endif

// Three-way comparison
#if FATP_HAS_SPACESHIP
    auto operator<=>(const Type& other) const = default;
#endif

// std::format
#if FATP_HAS_FORMAT
    auto s = std::format("Value: {}", value);
#endif
```

**Macro list:**
- `FATP_HAS_CONCEPTS` — C++20 concepts
- `FATP_HAS_RANGES` — C++20 ranges library
- `FATP_HAS_SPACESHIP` — Three-way comparison (`<=>`)
- `FATP_HAS_FORMAT` — `std::format`
- `FATP_HAS_COROUTINES` — C++20 coroutines
- `FATP_HAS_MODULES` — C++20 modules

### 3. Attribute Compatibility Macros

```cpp
// Works on all compilers, degrades gracefully
FATP_NODISCARD int compute();              // [[nodiscard]] or nothing
FATP_LIKELY(condition)                      // [[likely]] or nothing
FATP_UNLIKELY(condition)                    // [[unlikely]] or nothing
FATP_NO_UNIQUE_ADDRESS Allocator alloc_;   // [[no_unique_address]] or nothing
FATP_FALLTHROUGH;                           // [[fallthrough]] or nothing
```

**Implementation:**
```cpp
#if FATP_HAS_CPP17
    #define FATP_NODISCARD [[nodiscard]]
    #define FATP_FALLTHROUGH [[fallthrough]]
#else
    #define FATP_NODISCARD
    #define FATP_FALLTHROUGH
#endif

#if FATP_HAS_CPP20
    #define FATP_LIKELY(x) (x) [[likely]]
    #define FATP_UNLIKELY(x) (x) [[unlikely]]
    #define FATP_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
    #define FATP_LIKELY(x) (x)
    #define FATP_UNLIKELY(x) (x)
    #define FATP_NO_UNIQUE_ADDRESS
#endif
```

### 4. constexpr Evolution Support

```cpp
// constexpr levels through C++ versions
FATP_CONSTEXPR_14 int helper();      // constexpr in C++14+
FATP_CONSTEXPR_17 auto func();       // constexpr in C++17+
FATP_CONSTEXPR_20 void method();     // constexpr in C++20+
FATP_CONSTEXPR_ALLOC Container();    // constexpr with dynamic allocation (C++20)
FATP_CONSTEVAL int must_be_const();  // consteval in C++20, constexpr before
```

### 5. Compiler Detection

```cpp
#if FATP_COMPILER_CLANG
    // Clang-specific optimizations or workarounds
#elif FATP_COMPILER_GCC
    // GCC-specific
#elif FATP_COMPILER_MSVC
    // MSVC-specific
#endif

// Version checks
#if FATP_COMPILER_GCC && __GNUC__ >= 10
    // GCC 10+ feature
#endif
```

### 6. Platform Detection

```cpp
#if FATP_PLATFORM_WINDOWS
    #include <windows.h>
#elif FATP_PLATFORM_LINUX
    #include <unistd.h>
#elif FATP_PLATFORM_MACOS
    #include <mach/mach.h>
#endif

#if FATP_ARCH_X64
    // x86-64 SIMD available
#elif FATP_ARCH_ARM64
    // ARM NEON available
#endif
```

---

## Why Not Alternatives?

| If You Need... | Why Not Manual Checks | Why Not Boost.Config | Why Not feature-test macros | Fat-P Advantage |
|----------------|----------------------|---------------------|----------------------------|-----------------|
| MSVC compatibility | `__cplusplus` lies | ✅ Handles it | Manual `_MSVC_LANG` | ✅ Automatic |
| Unified version check | Scattered `#ifdef` | ✅ Unified | No unified macro | ✅ `FATP_HAS_CPP20` |
| Zero dependencies | ✅ Works | ❌ Requires Boost | ✅ Works | ✅ Single header |
| Attribute degradation | Manual per attribute | ✅ Provides | Not covered | ✅ `FATP_NODISCARD` |

**The Sweet Spot:** TypeTraits is the only option combining:
- ✅ MSVC `__cplusplus` workaround built-in
- ✅ Unified version detection (`FATP_HAS_CPP20`)
- ✅ Feature-specific macros (`FATP_HAS_CONCEPTS`)
- ✅ Attribute compatibility (`FATP_NODISCARD`, `FATP_LIKELY`)
- ✅ Zero external dependencies

---

## The "Forever Stuck" Reality

**Standard Reality:** Feature-test macros are standardized, but:
- MSVC requires special flags to report `__cplusplus` correctly
- No standard "unified C++ version" macro
- Attribute availability isn't covered by feature-test macros

TypeTraits normalizes these inconsistencies into a single header that "just works" across all major compilers.

---

## Performance Characteristics

### Compile-Time Only

TypeTraits is **entirely compile-time**:
- Zero runtime overhead
- No code generation
- Pure preprocessor macros

### Usage Pattern

```cpp
#include "TypeTraits.h"

// All checks resolve at compile time
#if FATP_HAS_CPP20
    // C++20 code path
#else
    // C++17 fallback
#endif
```

### Where Fat-P Wins

- **Cross-compiler libraries:** Write once, compile everywhere
- **Progressive enhancement:** Use C++20 when available, fall back gracefully
- **MSVC compatibility:** Automatic `_MSVC_LANG` handling

### Where Fat-P Loses (Honesty Builds Trust)

- **Non-standard compilers:** Only GCC, Clang, MSVC tested
- **Embedded compilers:** May need additions for proprietary toolchains
- **Bleeding edge:** New C++26 features need manual additions

---

## Integration Points

```
TypeTraits.h (CppStandardDetection.h)
    ↓ used by
Every fat_p header    (conditional compilation)
CheckedArithmetic.h   (constexpr levels)
Expected.h            (concepts when available)
SmallVector.h         ([[no_unique_address]])
```

---

## Final Assessment

TypeTraits delivers on the fat_p promise through three pillars:

### 1. Permanence
Compiler inconsistencies (MSVC `__cplusplus`) will persist for years due to backwards compatibility. TypeTraits provides a permanent abstraction layer that handles these quirks.

### 2. Specialization
HPC code often targets multiple compilers and standards. TypeTraits enables a single codebase that progressively uses available features (concepts, ranges, attributes) without #ifdef sprawl.

### 3. Control
Each feature has an independent macro. You check exactly what you need (`FATP_HAS_CONCEPTS`) without testing the entire standard version. Attribute macros degrade gracefully.

**Architectural Verdict:** TypeTraits transforms compiler detection from **scattered, error-prone #ifdef checks** to **unified, tested macros**. It's the foundation that enables all other fat_p headers to be portable across GCC, Clang, and MSVC with C++17/20/23.

---

*TypeTraits.h / CppStandardDetection.h — Fat-P Library*
