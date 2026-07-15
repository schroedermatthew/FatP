---
doc_id: UM-CPPFEATUREDETECTION-001
doc_type: "User Manual"
title: "CppFeatureDetection"
fatp_components: ["CppFeatureDetection"]
topics: ["C++ standard detection", "feature test macros", "MSVC __cplusplus", "library feature probing"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# User Manual - CppFeatureDetection

*February 2026*

---

**Scope:** Reference for the C++ standard and library feature detection macros in `CppFeatureDetection.h`.

**Not covered:**
- Compiler and OS detection (see User Manual - PlatformDetection)
- SIMD instruction set detection (see User Manual - SimdDetection)
- Portability attribute macros and library configuration (see User Manual - FatPConfig)
- How individual Fat-P components use these macros internally

**Prerequisites:** C++20.

---

## User Manual Card

**Component:** CppFeatureDetection
**Primary use case:** Gate code on actual C++ library feature availability, not just language standard
**Key API:** `FATP_CPP20_OR_LATER`, `FATP_CPP23_OR_LATER`, `FATP_HAS_FORMAT`, `FATP_HAS_COROUTINES`, `FATP_HAS_JTHREAD`, `FATP_HAS_ATOMIC_WAIT`, `FATP_HAS_LATCH`, `FATP_HAS_BARRIER`, `FATP_HAS_SEMAPHORE`, `FATP_HAS_MODULES`, `FATP_HAS_EXPECTED`, `FATP_HAS_STACKTRACE`, `FATP_HAS_MDSPAN`, `FATP_HAS_PRINT`
**Common mistakes:** Checking `__cplusplus` directly (wrong on MSVC without `/Zc:__cplusplus`); assuming C++20 mode means `<format>` is available (it may not be on older libstdc++)

---

## Macro Reference

### Standard Version

| Macro | Value | When |
|---|---|---|
| `FATP_CPLUSPLUS` | Integer | Normalized `__cplusplus` (uses `_MSVC_LANG` on MSVC) |
| `FATP_CPP20_OR_LATER` | 1 or 0 | C++20 or newer |
| `FATP_CPP23_OR_LATER` | 1 or 0 | C++23 or newer |
| `FATP_CPP26_OR_LATER` | 1 or 0 | C++26 or newer |

Fat-P requires C++20. `FATP_CPP20_OR_LATER` is always 1 in a valid Fat-P build.

### C++20 Library Features

| Macro | Value | What it detects |
|---|---|---|
| `FATP_HAS_FORMAT` | 1 or 0 | `<format>` header (std::format) |
| `FATP_HAS_JTHREAD` | 1 or 0 | `std::jthread` (C++20 cooperative threading) |
| `FATP_HAS_COROUTINES` | 1 or 0 | `<coroutine>` header and compiler support |
| `FATP_HAS_MODULES` | 1 or 0 | C++20 modules support |
| `FATP_HAS_ATOMIC_WAIT` | 1 or 0 | `std::atomic::wait()` (C++20) |
| `FATP_HAS_LATCH` | 1 or 0 | `std::latch` (C++20) |
| `FATP_HAS_BARRIER` | 1 or 0 | `std::barrier` (C++20) |
| `FATP_HAS_SEMAPHORE` | 1 or 0 | `std::counting_semaphore` (C++20) |
| `FATP_HAS_ATOMIC_SHARED_PTR` | 1 or 0 | `std::atomic<std::shared_ptr>` (C++20) |

### C++23 Library Features

| Macro | Value | What it detects |
|---|---|---|
| `FATP_HAS_EXPECTED` | 1 or 0 | `std::expected` (C++23) |
| `FATP_HAS_STACKTRACE` | 1 or 0 | `<stacktrace>` (C++23) |
| `FATP_HAS_MDSPAN` | 1 or 0 | `std::mdspan` (C++23) |
| `FATP_HAS_PRINT` | 1 or 0 | `std::print` (C++23) |

### C++26 Library Features

| Macro | Value | What it detects |
|---|---|---|
| `FATP_HAS_REFLECTION` | 1 or 0 | Reflection (`__cpp_lib_reflection`, C++26) |

### Namespace Utilities

| Symbol | Type | Purpose |
|---|---|---|
| `fat_p::cplusplus_value` | `constexpr long` | The normalized `FATP_CPLUSPLUS` value as a constant |
| `fat_p::cplusplus_string()` | `constexpr const char*` | Human-readable standard name: `"C++20"`, `"C++23"`, or `"C++26"` |

---

## Usage

```cpp
#include "CppFeatureDetection.h"

#if FATP_HAS_FORMAT
    #include <format>
    std::string msg = std::format("value: {}", 42);
#else
    // Fallback to snprintf or fmt
#endif

#if FATP_HAS_COROUTINES
    #include <coroutine>
    // Use CoroutineTask
#endif
```

---

## The MSVC __cplusplus Problem

MSVC reports `__cplusplus` as `199711L` (C++98) by default, regardless of the actual language mode. The actual mode is in `_MSVC_LANG`. CppFeatureDetection normalizes this:

```cpp
#if defined(_MSVC_LANG)
    #define FATP_CPLUSPLUS _MSVC_LANG
#else
    #define FATP_CPLUSPLUS __cplusplus
#endif
```

Always use `FATP_CPLUSPLUS` instead of `__cplusplus`.

---

## Troubleshooting

### FATP_HAS_FORMAT is 0 on GCC 12 in C++20 mode

GCC 12 has partial `<format>` support. The feature-test macro `__cpp_lib_format` may not be defined. CppFeatureDetection checks for the macro, not the header. Upgrade to GCC 13+ for full support.

### FATP_HAS_COROUTINES is 0 on Clang 13

Clang 13 requires `-fcoroutines-ts` for coroutine support. Clang 14+ supports coroutines without extra flags in C++20 mode.

---

*CppFeatureDetection.h --- Fat-P Library*
