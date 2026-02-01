---
doc_id: OV-STACKTRACE-001
doc_type: "Overview"
title: "Stacktrace"
fatp_components: ["Stacktrace", "StackFrame"]
topics: ["stack trace capture", "symbol resolution", "crash diagnostics", "async-signal-safe", "demangling", "debug information", "cross-platform diagnostics"]
constraints: ["async-signal-safety requirements", "symbol resolution cost", "platform backend availability", "debug info dependency", "signal handler restrictions"]
cxx_standard: "C++17"
std_equivalent: "std::stacktrace"
std_since: "C++23"
boost_equivalent: "Boost.Stacktrace"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-18"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# Overview - Stacktrace

*Fat-P Library — January 2026*

---

## Executive Summary

Stacktrace is a portable stack trace capture facility with automatic backend selection, designed for debugging, crash reporting, and contract violation diagnostics. Unlike raw platform APIs that expose platform-specific details and leave symbol resolution as an exercise for the caller, Stacktrace provides a unified interface that automatically selects the best available backend—C++23 `std::stacktrace`, libunwind, execinfo, Windows DbgHelp, or a stub fallback—and handles symbol demangling transparently. The combination of async-signal-safe raw capture for crash handlers, deferred symbol resolution for performance-sensitive paths, and JSON/string output formatting transforms diagnostic capture from platform-specific boilerplate into a single-line operation.

---

## Overview Card

**Component:** Stacktrace  
**Problem solved:** Cross-platform stack trace capture with symbol resolution and async-signal-safe operation  
**When to use:** Contract violation exceptions; crash handlers; debug logging; performance profiling callsite identification  
**When NOT to use:** Hot loops (capture has microsecond-scale cost); production code where debug info won't be deployed; platforms without any backend support  
**Key guarantee:** Async-signal-safe raw capture via `captureRaw()`; deferred symbolization  
**std equivalent:** `std::stacktrace` (C++23)  
**Boost equivalent:** Boost.Stacktrace  
**Other alternatives:** libbacktrace, backward-cpp, gperftools  
**Read next:** User Manual - Stacktrace, Companion Guide - Stacktrace

---

## The Problem Domain

### What Goes Wrong Without It

Consider a scientific computing application that validates matrix dimensions before operations:

```cpp
void matrix_multiply(const Matrix& a, const Matrix& b, Matrix& result) {
    if (a.cols() != b.rows()) {
        throw std::invalid_argument("Dimension mismatch");
    }
    // ... compute ...
}
```

When this throws, the exception message says "Dimension mismatch" but nothing about where. The user sees the error but not the call chain that led to it. Was it the physics engine? The mesh generator? The user's callback? The exception provides no context.

Now consider adding manual stack capture:

```cpp
#ifdef __linux__
#include <execinfo.h>
#include <cxxabi.h>
// 50 lines of backtrace capture, symbol parsing, demangling
#elif defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
// 80 lines of CaptureStackBackTrace, SymFromAddr, SymGetLineFromAddr64
#pragma comment(lib, "dbghelp.lib")
#endif
```

Every project that needs diagnostics replicates this platform-specific boilerplate. The code is fragile—symbol parsing formats differ between glibc versions, DbgHelp requires careful initialization, demangling has edge cases with templates and lambdas.

Crash handlers face an additional constraint: signal handlers cannot safely allocate memory or call non-reentrant functions. Most stack capture code violates these constraints, corrupting state or deadlocking when invoked from a signal handler.

| Issue | Impact |
|-------|--------|
| Platform-specific APIs | Duplicated code across projects; maintenance burden |
| Symbol format parsing | Fragile; breaks across OS/compiler versions |
| Demangling complexity | Template names become unreadable without proper handling |
| Async-signal-safety | Most capture code is unsafe in crash handlers |
| Debug info dependency | Symbols unavailable without `-g`; deployment complexity |

### The Standard's Limitation

C++23 introduced `std::stacktrace`, which solves the portability problem—if you can use C++23. For the vast majority of production codebases locked to C++17 or C++20 for compiler availability, ABI stability, or policy reasons, this component doesn't exist.

Even when available, `std::stacktrace::current()` performs immediate symbol resolution. There's no standard facility for async-signal-safe capture with deferred resolution.

```cpp
// C++23: Works but not available everywhere
auto trace = std::stacktrace::current();
std::cout << std::to_string(trace);

// C++17: You're on your own
```

No C++17 or C++20 standard component provides cross-platform stack trace capture. This gap will persist for years as codebases migrate.

---

## Architecture: Multi-Backend Capture with Deferred Resolution

Stacktrace combines three mechanisms:

### Automatic Backend Selection

At compile time, preprocessor detection selects the best available backend:

```
Backend Priority:
1. C++23 std::stacktrace    (FATP_HAS_STD_STACKTRACE)
2. libunwind               (FATP_HAS_LIBUNWIND)
3. execinfo backtrace()    (FATP_HAS_EXECINFO)
4. Windows DbgHelp         (FATP_HAS_DBGHELP)
5. Stub fallback           (always available)
```

```cpp
// Backend is selected at compile time
auto st = fat_p::Stacktrace::current();
std::cout << Stacktrace::backendName();  // e.g., "libunwind"
```

No runtime dispatch. No virtual functions. The backend is a compile-time type alias.

### Two-Phase Capture

Capture is separated into address collection and symbol resolution:

```cpp
// Phase 1: Collect addresses only (fast, async-signal-safe on POSIX)
auto raw = Stacktrace::captureRaw();  // ~6 µs

// Phase 2: Resolve symbols (allocates memory, calls system APIs)
raw.resolveSymbols();                  // ~20 µs additional

// Or: Single-phase capture with immediate resolution
auto st = Stacktrace::current();       // ~30 µs total
```

This separation enables async-signal-safe crash handlers: capture raw addresses in the signal handler, then resolve symbols after returning to normal execution (or in a crash dump processor).

### Portable StackFrame Structure

Each frame contains platform-unified information:

```cpp
struct StackFrame {
    void* address;           // Instruction pointer (always available)
    std::string function;    // Demangled name (after resolution)
    std::string file;        // Source file (if debug info available)
    std::size_t line;        // Line number (if available)
    std::size_t column;      // Column (if available)
    std::string module;      // Shared library / executable
    void* moduleBase;        // Module load address
    std::size_t offset;      // Offset from function start
    bool symbolized;         // Resolution attempted?
};
```

The same structure on all platforms. Backend differences are hidden.

---

## Feature Inventory

### 1. Single-Line Capture

The primary interface is a static factory method:

```cpp
auto st = Stacktrace::current();
std::cout << st.toString();
```

Output:
```
#0 validate_input(int)+0x23 at validation.cpp:47 [0x55f8a2c01234]
#1 process_request(Request const&)+0x156 at server.cpp:203 [0x55f8a2c01567]
#2 main+0x89 at main.cpp:12 [0x55f8a2c01890]
```

### 2. Frame Skipping and Depth Control

Control what gets captured:

```cpp
// Skip wrapper frames (default skip=1 removes current())
auto st = Stacktrace::current(3);        // Skip 3 frames

// Limit depth for shallow traces
auto st = Stacktrace::current(1, 10);    // Max 10 frames

// Full depth with custom skip
auto st = Stacktrace::current(2, 128);   // Skip 2, max 128
```

### 3. Async-Signal-Safe Raw Capture

For crash handlers:

```cpp
// In signal handler - only addresses, no allocation
void crash_handler(int sig) {
    static Stacktrace raw;  // Pre-allocated storage
    raw = Stacktrace::captureRaw();
    // Write raw addresses to crash log file
}

// Later, outside signal handler
raw.resolveSymbols();
std::cout << raw.toString();
```

### 4. JSON Output for Structured Logging

Machine-readable format for log aggregation:

```cpp
auto st = Stacktrace::current();
std::string json = st.toJson();
```

```json
[
  {
    "index": 0,
    "address": "0x55f8a2c01234",
    "function": "validate_input(int)",
    "file": "validation.cpp",
    "line": 47,
    "module": "myapp",
    "offset": 35,
    "symbolized": true
  }
]
```

### 5. STL Container Integration

Stacktrace is hashable and equality-comparable for deduplication:

```cpp
std::unordered_set<Stacktrace> seen_traces;

auto st = Stacktrace::current();
if (seen_traces.insert(st).second) {
    // New unique trace
    log_trace(st);
}
```

### 6. Range-For Iteration

Standard container interface:

```cpp
for (const auto& frame : st) {
    if (frame.function.find("my_module") != std::string::npos) {
        std::cout << "Found in: " << frame.toString() << "\n";
    }
}
```

---

## Why Not Alternatives?

### std::stacktrace (C++23)

| Aspect | std::stacktrace | FAT-P Stacktrace |
|--------|-----------------|------------------|
| **Availability** | C++23 only | C++17+ |
| **Async-signal-safe** | No | Yes (captureRaw) |
| **Deferred resolution** | No | Yes |
| **JSON output** | No | Built-in |
| **Hash/equality** | Implementation-defined | Guaranteed |

**When to use std::stacktrace:** C++23 is available and async-signal-safety is not needed.

**When to use FAT-P Stacktrace:** C++17/C++20 codebase; crash handlers need async-signal-safe capture; need JSON output or deduplication.

### Boost.Stacktrace

| Aspect | Boost.Stacktrace | FAT-P Stacktrace |
|--------|------------------|------------------|
| **Dependencies** | Boost headers | None (STL only) |
| **Async-signal-safe** | Via boost::stacktrace::safe_dump_to | Via captureRaw() |
| **Symbol resolution** | Immediate or never | Deferred optional |
| **Build configuration** | Link options required | Header-only |
| **JSON output** | No | Built-in |

**When to use Boost.Stacktrace:** Already using Boost; need Boost ecosystem integration.

**When to use FAT-P Stacktrace:** Zero external dependencies; need deferred resolution API; want header-only integration.

### libbacktrace

| Aspect | libbacktrace | FAT-P Stacktrace |
|--------|--------------|------------------|
| **Windows support** | No | Yes (DbgHelp) |
| **Header-only** | No (requires build) | Yes |
| **Dependencies** | Library to build | None |
| **API style** | C callbacks | C++ value types |

**When to use libbacktrace:** Unix-only deployment; need libbacktrace's DWARF parsing.

**When to use FAT-P Stacktrace:** Cross-platform requirement; header-only deployment; C++ API preferred.

### The Exclusionary Argument

| If You Need... | Why Not std:: | Why Not Boost | FAT-P Advantage |
|----------------|---------------|---------------|-----------------|
| C++17 support | Unavailable | Heavy dependency | Header-only, no deps |
| Async-signal-safe | Not provided | Complex API | Simple captureRaw() |
| Deferred resolution | Not supported | Not supported | Built-in |
| JSON output | Not provided | Not provided | toJson() method |
| Hash/equality | Implementation-defined | Implementation-defined | Guaranteed |

---

## The "Forever Stuck" Reality

C++23's `std::stacktrace` addresses portability, but:

1. **Most codebases cannot use C++23 yet.** Enterprise C++ moves slowly. Compiler availability, ABI concerns, and testing overhead mean C++23 adoption will take years.

2. **std::stacktrace lacks async-signal-safe capture.** The standard provides no mechanism for two-phase capture. This gap is unlikely to be addressed because signal handling is inherently platform-specific.

3. **No standard JSON serialization.** `std::to_string(std::stacktrace)` produces implementation-defined output, not structured data for log aggregation.

For C++17/C++20 codebases that need crash handlers, structured logging, or deferred resolution, no standard solution exists or is planned.

---

## Performance Characteristics

Benchmarks on Linux x86-64, GCC 13, execinfo backend (median of 15 runs):

| Operation | Time | Notes |
|-----------|------|-------|
| `captureRaw()` (10 frames) | **6.4 µs** | Address collection only |
| `current()` (10 frames) | **31.7 µs** | Capture + symbolize |
| `resolveSymbols()` | **20.7 µs** | Symbol resolution phase |
| `toString()` | **3.0 µs** | String formatting |
| `toJson()` | **11.3 µs** | JSON formatting |

### Where FAT-P Wins

**Contract violation exceptions.** Capture at throw site, format on catch. One-time cost per exception.

**Crash handlers.** Async-signal-safe `captureRaw()` in signal handler; resolve symbols in crash reporter.

**Debug logging.** Structured JSON output for log aggregation and analysis.

**Deduplication.** Hash-based storage to avoid logging duplicate traces.

### Where FAT-P Loses

**Hot loops.** Microsecond capture cost is unacceptable for per-iteration tracing.

**Production builds without debug info.** Symbols will be missing; only addresses available.

**Platforms without backends.** Stub fallback provides no useful information.

---

## Integration Points

```
Stacktrace.h
    → uses: CppStandardDetection.h (platform/compiler detection macros)
    → used by: ContractException.h (violation diagnostics)
    → used by: Enforce.h (assertion stack traces)
    → used by: DiagnosticLogger.h (structured crash logs)
```

---

## Final Assessment

Stacktrace delivers on the FAT-P promise:

**Permanence.** C++17/C++20 codebases cannot use `std::stacktrace`. The async-signal-safe two-phase capture pattern has no standard equivalent. This component fills a gap that will persist for years.

**Specialization.** Multi-backend automatic selection, deferred symbol resolution, JSON output, and hash/equality support address real diagnostic requirements that platform APIs and even `std::stacktrace` leave unsolved.

**Control.** Frame skipping, depth limits, and separate capture/resolve phases give precise control over when costs are incurred. Header-only deployment with zero dependencies enables integration anywhere.

For crash handlers, contract violations, and diagnostic logging in C++17/C++20 codebases, Stacktrace transforms platform-specific boilerplate into portable, async-signal-safe, structured diagnostics—without external dependencies.

---

*Stacktrace.h — Fat-P Library v3.2*
