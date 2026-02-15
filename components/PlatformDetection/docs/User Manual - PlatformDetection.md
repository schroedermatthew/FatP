---
doc_id: UM-PLATFORMDETECTION-001
doc_type: "User Manual"
title: "PlatformDetection"
fatp_components: ["PlatformDetection"]
topics: ["compiler detection", "OS detection", "architecture detection", "NUMA", "debug mode", "FATP_COMPILER_*", "FATP_PLATFORM_*", "FATP_ARCH_*"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# User Manual - PlatformDetection

*February 2026*

---

**Scope:** Complete reference for the platform detection macros in `PlatformDetection.h`.

**Prerequisites:** C++20. Basic understanding of conditional compilation.

---

## User Manual Card

**Component:** PlatformDetection
**Primary use case:** Portable platform-conditional compilation across Fat-P
**Key API:** `FATP_COMPILER_*`, `FATP_PLATFORM_*`, `FATP_ARCH_*`, `FATP_HAS_NUMA`, `FATP_DEBUG`
**Common mistakes:** Using raw `_MSC_VER` instead of `FATP_COMPILER_MSVC`; confusing `FATP_PLATFORM_POSIX` (includes Linux and macOS) with `FATP_PLATFORM_LINUX` (Linux only)

---

## Macro Reference

### Compiler Detection

| Macro | Value | When |
|---|---|---|
| `FATP_COMPILER_CLANG` | 1 or 0 | Clang (including Apple Clang) |
| `FATP_COMPILER_GCC` | 1 or 0 | GCC (not Clang pretending to be GCC) |
| `FATP_COMPILER_MSVC` | 1 or 0 | Microsoft Visual C++ |
| `FATP_COMPILER_NAME` | String | "Clang", "GCC", "MSVC", or "Unknown" |
| `FATP_COMPILER_VERSION` | Integer | Compiler version (e.g., Clang 1500, GCC 1301, MSVC 1940) |

Detection order: Clang first (Clang defines `__GNUC__`, so GCC check must exclude Clang), then GCC, then MSVC.

### Operating System Detection

| Macro | Value | When |
|---|---|---|
| `FATP_PLATFORM_WINDOWS` | 1 or 0 | Windows (any version) |
| `FATP_PLATFORM_POSIX` | 1 or 0 | Any POSIX system (Linux, macOS, BSD) |
| `FATP_PLATFORM_LINUX` | 1 or 0 | Linux specifically |
| `FATP_PLATFORM_MACOS` | 1 or 0 | macOS specifically |

`FATP_PLATFORM_POSIX` is 1 for both Linux and macOS.

### Architecture Detection

| Macro | Value | When |
|---|---|---|
| `FATP_ARCH_X64` | 1 or 0 | x86-64 / AMD64 |
| `FATP_ARCH_X86` | 1 or 0 | 32-bit x86 |
| `FATP_ARCH_ARM64` | 1 or 0 | AArch64 / ARM64 |
| `FATP_ARCH_ARM32` | 1 or 0 | 32-bit ARM |

### Hardware and Build Configuration

| Macro | Value | When |
|---|---|---|
| `FATP_HAS_NUMA` | 1 or 0 | NUMA-capable system detected |
| `FATP_DEBUG` | 1 or 0 | Debug build (NDEBUG not defined) |

---

## Usage

```cpp
#include "PlatformDetection.h"

#if FATP_PLATFORM_WINDOWS
    #include <windows.h>
#elif FATP_PLATFORM_POSIX
    #include <unistd.h>
#endif

#if FATP_ARCH_ARM64
    // ARM-specific code
#elif FATP_ARCH_X64
    // x86-64-specific code
#endif
```

---

## Best Practices

Always use `FATP_*` macros instead of raw vendor macros. The detection logic handles edge cases (Clang-on-Windows defining `__GNUC__`, Apple Clang vs upstream Clang) that raw checks miss.

---

## Troubleshooting

### FATP_COMPILER_GCC is 1 but I am using Clang

Clang defines `__GNUC__` for compatibility. PlatformDetection checks `__clang__` first to avoid this. If you see GCC detected under Clang, the header include order may be wrong---ensure PlatformDetection.h is included before any manual `__GNUC__` checks.

### FATP_HAS_NUMA is 0 on a NUMA machine

NUMA detection checks for `<numa.h>` availability (Linux). If the NUMA development headers are not installed, detection fails. Install `libnuma-dev` (Debian/Ubuntu) or `numactl-devel` (RHEL/Fedora).

---

*PlatformDetection.h --- Fat-P Library*
