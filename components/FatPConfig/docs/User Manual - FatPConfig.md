---
doc_id: UM-FATPCONFIG-001
doc_type: "User Manual"
title: "FatPConfig"
fatp_components: ["FatPConfig"]
topics: ["configuration macros", "portability", "FATP_NO_UNIQUE_ADDRESS", "FATP_LIKELY", "FATP_UNLIKELY", "FATP_FORCEINLINE", "FATP_NOINLINE", "cache line size"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# User Manual - FatPConfig

*February 2026*

---

**Scope:** Reference for the portability and configuration macros in `FatPConfig.h`.

**Not covered:**
- C++ standard and library feature detection (see User Manual - CppFeatureDetection)
- Compiler, OS, and architecture detection (see User Manual - PlatformDetection)
- SIMD instruction set detection (see User Manual - SimdDetection)
- Build system integration and CMake configuration

**Prerequisites:** C++20.

---

## User Manual Card

**Component:** FatPConfig
**Primary use case:** Portable compiler-specific attributes and library-wide configuration
**Key API:** `FATP_NO_UNIQUE_ADDRESS`, `FATP_LIKELY(x)`, `FATP_UNLIKELY(x)`, `FATP_FORCEINLINE`, `FATP_NOINLINE`, `FATP_CACHE_LINE_SIZE`, `FATP_ENABLE_IOSTREAM`, `fat_p::config::cache_line_size`
**Common mistakes:** Using raw `[[no_unique_address]]` on MSVC (requires `[[msvc::no_unique_address]]`); assuming `FATP_FORCEINLINE` guarantees inlining (it is a hint, not a guarantee)

---

## Macro Reference

### Attribute Portability

| Macro | GCC/Clang | MSVC | Fallback |
|---|---|---|---|
| `FATP_NO_UNIQUE_ADDRESS` | `[[no_unique_address]]` | `[[msvc::no_unique_address]]` | empty |
| `FATP_LIKELY(x)` | `__builtin_expect(!!(x), 1)` | `(x)` | `(x)` |
| `FATP_UNLIKELY(x)` | `__builtin_expect(!!(x), 0)` | `(x)` | `(x)` |
| `FATP_FORCEINLINE` | `inline __attribute__((always_inline))` | `__forceinline` | `inline` |
| `FATP_NOINLINE` | `__attribute__((noinline, cold))` | `__declspec(noinline)` | empty |

### Configuration Constants

| Macro / Constant | Default | Description |
|---|---|---|
| `FATP_CACHE_LINE_SIZE` | 64 | Cache line size in bytes. Override before including FatPConfig.h if your target has a different cache line size (e.g., 128 on Apple M-series). |
| `fat_p::config::cache_line_size` | 64 | constexpr version of the above, in namespace `fat_p::config`. |
| `FATP_ENABLE_IOSTREAM` | 1 | Controls whether Fat-P headers include `<iostream>`. Set to 0 to disable for embedded or freestanding environments. |

---

## Usage

```cpp
#include "FatPConfig.h"

struct Compressed
{
    FATP_NO_UNIQUE_ADDRESS EmptyTag tag;  // Zero bytes on supporting compilers
    int value;
};

FATP_FORCEINLINE int fast_path(int x)
{
    if (FATP_LIKELY(x > 0))
        return x * 2;
    return slow_path(x);
}
```

---

## Overriding Cache Line Size

Apple M-series CPUs use 128-byte cache lines. Define `FATP_CACHE_LINE_SIZE` before including any Fat-P header:

```cpp
#define FATP_CACHE_LINE_SIZE 128
#include "FatPConfig.h"
```

This affects `alignas` usage in cache-line-padded structures throughout the library (e.g., `SpinBarrier`, `LockFreeQueue` padding).

---

## Troubleshooting

### MSVC warning about [[no_unique_address]]

MSVC versions before 19.29 do not support `[[msvc::no_unique_address]]`. FatPConfig falls back to an empty macro, meaning empty base optimization is not applied and sizeof may be larger than expected.

### FATP_LIKELY has no effect on MSVC

MSVC does not support `__builtin_expect`. The macro expands to a plain expression. Branch prediction relies on the CPU's hardware predictor instead.

---

*FatPConfig.h --- Fat-P Library*
