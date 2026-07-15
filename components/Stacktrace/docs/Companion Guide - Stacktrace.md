---
doc_id: CG-STACKTRACE-001
doc_type: "Companion Guide"
title: "Stacktrace"
fatp_components: ["Stacktrace", "StackFrame"]
topics: ["stack trace design", "backend architecture", "async-signal-safety", "symbol resolution", "cross-platform abstraction", "crash diagnostics", "exception debugging", "unwinding algorithms"]
constraints: ["async-signal-safety", "symbol resolution performance", "debug info availability", "platform API differences", "signal handler restrictions", "memory allocation constraints"]
cxx_standard: "C++20"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-18"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# **The Call Chain Witness**

### *A Companion Guide to FAT-P's Stacktrace*

---

**Scope:** This guide covers the design philosophy, architectural decisions, and implementation rationale behind Stacktrace—FAT-P's portable stack trace capture facility with multi-backend support and async-signal-safe operation. It explains why stack trace capture is harder than it appears, how the backend selection mechanism achieves zero-overhead abstraction, the subtle constraints that make signal handler capture treacherous, and when the design's tradeoffs work against you. Other FAT-P diagnostic components (DiagnosticLogger, ContractException) are documented separately.

**Not covered:**
- API reference and usage recipes (see User Manual - Stacktrace)
- Platform-specific backend implementations (see source code)
- General signal handling concepts (see operating system documentation)
- DWARF debug format details (see DWARF specification)

**Prerequisites:**
- Understanding of the call stack concept (return addresses, stack frames)
- Familiarity with signal handlers and their restrictions
- Awareness of symbol tables and debug information
- Basic knowledge of C++ name mangling

---

## Companion Guide Card

**Component:** Stacktrace  
**Design question:** How do you capture stack traces portably with async-signal-safe support?  
**Key tradeoff:** Immediate symbol resolution (convenient) vs. deferred resolution (async-signal-safe)  
**Decision made:** Provide both: `current()` for convenience, `captureRaw()` for signal handlers  
**Rejected alternatives:** Virtual backend dispatch (runtime overhead), callback-based API (complex), single-platform focus (not portable)  
**Historical context:** Platform fragmentation meets C++23 standardization timeline gap

---

# **Table of Contents**

**[Introduction: Why This Component Exists](#introduction-why-this-component-exists)**

## Part I — The Problems

1. [The Platform Fragmentation Problem](#chapter-1--the-platform-fragmentation-problem)
2. [The Symbol Resolution Challenge](#chapter-2--the-symbol-resolution-challenge)
3. [The Signal Handler Trap](#chapter-3--the-signal-handler-trap)
4. [The C++23 Timing Gap](#chapter-4--the-c23-timing-gap)
5. [The Debug Info Dependency](#chapter-5--the-debug-info-dependency)

## Part II — The Solutions

6. [Architecture Overview](#chapter-6--architecture-overview)
7. [Backend Selection: Compile-Time Polymorphism](#chapter-7--backend-selection-compile-time-polymorphism)
8. [Two-Phase Capture: Address Collection and Symbol Resolution](#chapter-8--two-phase-capture-address-collection-and-symbol-resolution)
9. [The StackFrame Contract: Platform-Unified Data](#chapter-9--the-stackframe-contract-platform-unified-data)
10. [Demangling: From _ZN3foo3barEi to foo::bar(int)](#chapter-10--demangling-from-_zn3foo3barei-to-foobarint)
11. [Hash and Equality: Enabling Deduplication](#chapter-11--hash-and-equality-enabling-deduplication)

## Part III — Putting It Together

12. [Case Study: Contract Violation Diagnostics](#chapter-12--case-study-contract-violation-diagnostics)
13. [Case Study: Production Crash Handler](#chapter-13--case-study-production-crash-handler)
14. [Case Study: Log Aggregation and Analysis](#chapter-14--case-study-log-aggregation-and-analysis)
15. [Case Study: Performance Sampling Profiler](#chapter-15--case-study-performance-sampling-profiler)
16. [Choosing the Right Capture Strategy](#chapter-16--choosing-the-right-capture-strategy)

## Part IV — Foundations

- [Appendix A — A History of Stack Unwinding](#appendix-a--a-history-of-stack-unwinding)
- [Appendix B — Why Each Backend Exists](#appendix-b--why-each-backend-exists)
- [Appendix C — Design Constraints and Rejected Alternatives](#appendix-c--design-constraints-and-rejected-alternatives)
- [Appendix D — Where Stacktrace Loses](#appendix-d--where-stacktrace-loses)
- [Appendix E — The std::stacktrace Gap](#appendix-e--the-stdstacktrace-gap)
- [Appendix F — Further Reading](#appendix-f--further-reading)

---

# **Introduction: Why This Component Exists**

You're building a numerical library. Your matrix multiplication function validates dimensions:

```cpp
Matrix multiply(const Matrix& a, const Matrix& b) {
    if (a.cols() != b.rows()) {
        throw std::invalid_argument(
            "Matrix dimension mismatch: " + 
            std::to_string(a.cols()) + " != " + std::to_string(b.rows())
        );
    }
    // ...
}
```

A user reports: "I got 'Matrix dimension mismatch: 3 != 4' but I can't figure out where." The exception message tells them *what* went wrong but not *where*. Was it in their mesh generation code? Their physics solver? A bug in their data loader? Without a stack trace, they're reduced to sprinkling `std::cout` statements through their code until they find the call site.

Or consider a server application that crashes with SIGSEGV in production. The process terminates, leaving no evidence except "segmentation fault." Even if core dumps are enabled, they may not be collected due to security policies or disk space limits. A stack trace captured at crash time would identify the crashing function and its callers—enough to diagnose most issues without reproducing the crash.

Or this scenario: you're writing a logging library that captures stack traces for error events. You need the traces in JSON format for your log aggregation system. Each platform's native format is different—Linux's backtrace_symbols produces one format, Windows DbgHelp another. You need consistent structured output regardless of platform.

These aren't edge cases. They're the daily reality of C++ systems programming:

- **Exceptions lack context** because C++ doesn't automatically attach stack traces
- **Crashes lose evidence** because signal handlers can't safely use most capture code
- **Platform APIs diverge** because there's no standard (until C++23, and migration takes years)
- **Symbol resolution has hidden costs** that hurt performance-sensitive logging

Stacktrace exists for engineers who need all of these solved simultaneously:

- **Multi-backend selection** automatically uses the best available platform API
- **Two-phase capture** separates async-signal-safe address collection from symbol resolution
- **Platform-unified output** provides consistent StackFrame structure everywhere
- **Structured serialization** via toJson() for log aggregation systems

This guide explains the problems in depth and how Stacktrace addresses them.

---

# **PART I — THE PROBLEMS**

Stack trace capture seems simple: walk the call stack, collect return addresses, map them to function names. The complications arise from platform diversity (every OS does it differently), signal safety (most code is unsafe in handlers), symbol availability (debug info may be absent), and performance (symbol resolution is expensive). Understanding these forces is essential for understanding why Stacktrace is designed as it is.

---

# **CHAPTER 1 — The Platform Fragmentation Problem**

### The Unix Heritage

Unix systems traditionally provided no standard stack trace API. The runtime maintained a call stack, but accessing it programmatically required assembly or debugger-style /proc examination.

In the 1990s, glibc added `backtrace()` and `backtrace_symbols()` to `<execinfo.h>`. These became the de facto Unix standard, but they have limitations:

- **Format varies by platform.** Linux glibc, macOS, and BSD produce different symbol string formats.
- **Not async-signal-safe.** `backtrace_symbols()` calls `malloc()`.
- **Requires frame pointers.** Without `-fno-omit-frame-pointer`, unwinding may fail.

### The libunwind Alternative

libunwind emerged as a more robust solution. It uses exception handling metadata (DWARF CFI) to unwind stacks even without frame pointers. It's available on Linux and macOS, and parts of it were standardized as the Itanium C++ ABI unwinding interface.

libunwind provides:
- **Reliable unwinding** even with optimization
- **Cursor-based iteration** over stack frames
- **Async-signal-safe capture** (the core unwinding, not symbol resolution)

But libunwind isn't universally available—it's a separate library that must be installed.

### The Windows World

Windows has a completely different stack trace infrastructure:

- **CaptureStackBackTrace()** collects return addresses. It's the equivalent of `backtrace()`.
- **DbgHelp library** provides symbol resolution via `SymFromAddr()`, `SymGetLineFromAddr64()`.
- **Initialization required.** `SymInitialize()` must be called before symbol resolution.
- **Thread safety concerns.** DbgHelp operations require careful serialization.

Windows symbols work differently from Unix—PDB files contain debug information separately from the executable.

### The C++23 Future

C++23 introduced `std::stacktrace`, finally providing a portable standard API. But:

- Most production codebases aren't on C++23 yet
- `std::stacktrace::current()` performs immediate symbol resolution (no async-signal-safe mode)
- Implementation quality varies across compilers

### What This Means for Application Code

Without abstraction, supporting multiple platforms requires:

```cpp
#if defined(__linux__) || defined(__APPLE__)
    #if HAS_LIBUNWIND
        // libunwind path
    #else
        // execinfo path
    #endif
#elif defined(_WIN32)
    // DbgHelp path
#elif __cplusplus >= 202302L
    // std::stacktrace path
#endif
```

This conditional compilation, repeated in every project that needs stack traces, is the platform fragmentation problem.

---

# **CHAPTER 2 — The Symbol Resolution Challenge**

### From Address to Name

A stack trace without symbols is a list of hexadecimal numbers:

```
0x55f8a2c01234
0x55f8a2c01567
0x55f8a2c01890
```

Converting to useful output requires several steps:

1. **Find the containing module.** Which executable or shared library contains this address?
2. **Locate the symbol table.** Read the ELF symbol table (Unix) or PDB file (Windows).
3. **Find the enclosing function.** Binary search the symbol addresses.
4. **Demangle the name.** C++ names are mangled: `_ZN3foo3barEi` → `foo::bar(int)`.
5. **Find source location.** DWARF debug info (Unix) or PDB (Windows) maps addresses to file:line.

### The Demangling Minefield

C++ compilers mangle function names to encode overloading, namespaces, and templates:

| C++ Name | Mangled (GCC/Clang) |
|----------|---------------------|
| `foo()` | `_Z3foov` |
| `foo::bar(int)` | `_ZN3foo3barEi` |
| `std::vector<int>::push_back(int)` | `_ZNSt6vectorIiSaIiEE9push_backEi` |
| `template<typename T> T max(T, T)` | Complex encoding with template arguments |

The `abi::__cxa_demangle()` function reverses this, but:

- It allocates memory (caller must `free()`)
- It can fail on malformed input
- MSVC uses a different mangling scheme (handled by DbgHelp's `SYMOPT_UNDNAME`)

### The Debug Info Dependency

Source file and line information requires debug symbols:

| Build | Function Name | File:Line |
|-------|---------------|-----------|
| `g++ -g` | ✓ Available | ✓ Available |
| `g++ -g0` | ✓ Available (symbol table) | ✗ Not available |
| `g++ -s` (stripped) | ✗ Not available | ✗ Not available |
| `g++ -g` then `strip` | ✗ Not available | ✗ Not available |

Release builds often strip debug info for size. Crash reports from production may have only addresses.

### The Performance Cost

Symbol resolution is expensive:

| Operation | Typical Cost |
|-----------|--------------|
| Address capture (10 frames) | 5-10 µs |
| Symbol resolution (10 frames) | 20-50 µs |
| File/line lookup (10 frames) | 10-30 µs additional |

For crash handlers (one-time operation), this is fine. For logging every error, it may be excessive.

---

# **CHAPTER 3 — The Signal Handler Trap**

### Why Signal Handlers Are Dangerous

When the operating system delivers a signal (SIGSEGV, SIGABRT, etc.), it invokes your signal handler in a special context:

1. **The interrupted code may hold locks.** If your handler tries to acquire the same lock, deadlock.
2. **The heap may be corrupted.** If the crash was due to heap corruption, `malloc()` may infinite loop or crash again.
3. **stdio may be in an inconsistent state.** `printf()` uses internal buffers and locks.

The POSIX standard defines a small set of "async-signal-safe" functions that are safe to call from signal handlers. Most useful functions—including `malloc()`, `free()`, `printf()`, and anything that allocates memory—are NOT on this list.

### What Most Stack Trace Code Does Wrong

```cpp
void naive_crash_handler(int sig) {
    // WRONG: backtrace_symbols() calls malloc()
    void* addresses[64];
    int depth = backtrace(addresses, 64);
    char** symbols = backtrace_symbols(addresses, depth);  // malloc!
    
    // WRONG: fprintf() calls malloc() and isn't async-signal-safe
    for (int i = 0; i < depth; ++i) {
        fprintf(stderr, "%s\n", symbols[i]);  // Not safe!
    }
    
    free(symbols);  // malloc/free not safe!
}
```

This code works most of the time. Occasionally, it deadlocks when the crash happened while the heap allocator held its lock. Or it corrupts memory when the heap was already corrupted. These are the worst bugs—intermittent, unreproducible, and they corrupt the crash evidence.

### The Async-Signal-Safe Path

Correct crash handling separates capture from resolution:

```cpp
void safe_crash_handler(int sig) {
    // Phase 1: Capture addresses only (no malloc)
    static void* addresses[64];  // Static allocation
    int depth = backtrace(addresses, 64);  // backtrace() is async-signal-safe
    
    // Phase 2: Write raw addresses using write() (async-signal-safe)
    // to a pre-opened file descriptor
    write(crash_fd, addresses, depth * sizeof(void*));
    
    // Re-raise for default handling
    signal(sig, SIG_DFL);
    raise(sig);
}
```

Symbol resolution happens later—either in a crash dump processor or after returning to normal execution (for non-fatal signals).

---

# **CHAPTER 4 — The C++23 Timing Gap**

### When Standards Arrive vs. When Code Migrates

C++23 introduced `std::stacktrace`, finally standardizing what should have been standard decades ago. But standardization and adoption are different things.

**Compiler support timeline:**
- GCC 12 (2022): Basic support
- GCC 13 (2023): Improved support
- Clang 16 (2023): Support with limitations
- MSVC 19.34 (2022): Support

**Production codebase migration:**
- Most enterprise C++ is still on C++20 or earlier
- Compiler upgrades require testing, ABI validation, build system changes
- Security-critical codebases move slowly
- Scientific computing clusters run old compilers for driver compatibility

A component that requires C++23 excludes the majority of production C++.

### What std::stacktrace Provides

```cpp
#include <stacktrace>

void print_trace() {
    auto st = std::stacktrace::current();
    std::cout << std::to_string(st);
}
```

Clean, portable, standard. But:

- **Immediate resolution.** No way to defer symbol lookup.
- **No async-signal-safe mode.** `current()` may allocate.
- **Implementation-defined format.** `to_string()` output varies by compiler.
- **No JSON output.** Structured logging requires parsing the string.

### The Gap Stacktrace Fills

For C++20 codebases that need:
- Stack traces today, not when C++23 migration completes
- Async-signal-safe capture for crash handlers
- Consistent structured output (JSON) for log aggregation
- Control over when symbol resolution happens

No standard solution exists. This gap will persist for years.

---

# **CHAPTER 5 — The Debug Info Dependency**

### What Debug Info Contains

Debug information maps compiled code back to source:

| Information | Format | Required For |
|-------------|--------|--------------|
| Symbol table | ELF .symtab / PE exports | Function names |
| DWARF info | .debug_* sections | File, line, variable info |
| PDB files | Separate .pdb | Windows source mapping |

### Build Configuration Tradeoffs

| Configuration | Binary Size | Stack Trace Quality |
|---------------|-------------|---------------------|
| `-g -O0` | Largest | Best: full symbols, file:line |
| `-g -O2` | Large | Good: symbols, file:line (some inlining) |
| `-O2` | Medium | Limited: symbols only |
| `-O2 -s` | Smallest | Minimal: addresses only |

Production often uses the smallest builds. Debugging uses the largest. A common practice is to build with debug info, then strip it into separate files:

```bash
objcopy --only-keep-debug app app.debug
strip app
objcopy --add-gnu-debuglink=app.debug app
```

### When Symbols Aren't Available

Without debug info, Stacktrace still captures addresses. These can be:

1. **Resolved later** if debug info is available separately
2. **Cross-referenced** against builds that have symbols
3. **Used for deduplication** even without function names

A stack trace with only addresses is still useful for identifying unique crash sites.

---

# **PART II — THE SOLUTIONS**

Stacktrace addresses platform fragmentation through compile-time backend selection, signal safety through two-phase capture, and output consistency through the StackFrame structure. These mechanisms work together to provide a unified API without runtime overhead.

---

# **CHAPTER 6 — Architecture Overview**

### The Three-Layer Design

```
┌─────────────────────────────────────────────────┐
│              Application Code                   │
│   auto st = Stacktrace::current();              │
└─────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────┐
│               Stacktrace Class                  │
│   - current() / captureRaw()                    │
│   - resolveSymbols()                            │
│   - toString() / toJson()                       │
│   - Iteration, comparison, hashing              │
└─────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────┐
│          Backend (compile-time selected)        │
│   ┌─────────────┬────────────┬────────────┐     │
│   │   C++23     │  libunwind │  execinfo  │     │
│   │ std::stack  │            │ backtrace  │     │
│   │   trace     │            │            │     │
│   └─────────────┴────────────┴────────────┘     │
│   ┌─────────────┬────────────┐                  │
│   │  Windows    │   Stub     │                  │
│   │  DbgHelp    │ (fallback) │                  │
│   └─────────────┴────────────┘                  │
└─────────────────────────────────────────────────┘
```

**Application Code** uses the `Stacktrace` class without knowing which backend is active.

**Stacktrace Class** provides the public API: capture, resolution, formatting, comparison.

**Backend** is a compile-time selected type alias that implements `capture()`, `captureRaw()`, and `resolveSymbols()`.

### Zero-Overhead Backend Selection

The backend is a type alias, not a virtual base class:

```cpp
namespace detail {
    #if FATP_HAS_STD_STACKTRACE
        using StacktraceBackend = Cpp23StacktraceBackend;
    #elif FATP_HAS_LIBUNWIND
        using StacktraceBackend = LibunwindBackend;
    #elif FATP_HAS_EXECINFO
        using StacktraceBackend = ExecinfoBackend;
    #elif FATP_HAS_DBGHELP
        using StacktraceBackend = DbgHelpBackend;
    #else
        using StacktraceBackend = StubBackend;
    #endif
}
```

No virtual function calls. No runtime dispatch. The compiler sees through the alias and optimizes as if the backend were used directly.

---

# **CHAPTER 7 — Backend Selection: Compile-Time Polymorphism**

### Detection Macros

Stacktrace defines detection macros based on platform and available headers:

```cpp
// C++23 detection
#if FATP_CPP23_OR_LATER && __has_include(<stacktrace>)
    #define FATP_HAS_STD_STACKTRACE 1
#endif

// libunwind detection
#if FATP_PLATFORM_POSIX && __has_include(<libunwind.h>)
    #define FATP_HAS_LIBUNWIND 1
#endif

// execinfo detection (glibc)
#if FATP_PLATFORM_POSIX && __has_include(<execinfo.h>)
    #define FATP_HAS_EXECINFO 1
#endif

// Windows DbgHelp (always available on Windows)
#if FATP_PLATFORM_WINDOWS
    #define FATP_HAS_DBGHELP 1
#endif
```

### Priority Order

Multiple backends may be available. Stacktrace selects the best one:

1. **C++23 std::stacktrace** — Standard, portable, well-integrated
2. **libunwind** — Robust unwinding, handles optimized code
3. **execinfo** — Widely available on Unix, simpler
4. **DbgHelp** — Windows-native
5. **Stub** — Always available, provides placeholder

The priority reflects reliability and feature completeness. C++23 is preferred when available because it's standard and future-proof. libunwind beats execinfo because it handles frame-pointer-omitting code better.

### Backend Interface Contract

Each backend implements three static methods:

```cpp
class Backend {
public:
    // Capture with immediate symbol resolution
    static std::vector<StackFrame> capture(size_t skip, size_t maxDepth);
    
    // Capture addresses only (async-signal-safe on POSIX)
    static std::vector<StackFrame> captureRaw(size_t skip, size_t maxDepth);
    
    // Resolve symbols for previously captured frames
    static void resolveSymbols(std::vector<StackFrame>& frames);
};
```

This contract enables the Stacktrace class to work identically regardless of which backend is active.

---

# **CHAPTER 8 — Two-Phase Capture: Address Collection and Symbol Resolution**

### The Key Insight

Stack trace capture has two distinct phases with different constraints:

| Phase | Operations | Allocates? | Async-Signal-Safe? |
|-------|------------|------------|-------------------|
| Address collection | Walk stack, read return addresses | Minimal | Yes (on POSIX) |
| Symbol resolution | Load symbol tables, demangle names, lookup lines | Heavy | No |

Separating these phases enables async-signal-safe crash handlers.

### Phase 1: captureRaw()

```cpp
static std::vector<StackFrame> captureRaw(size_t skip, size_t maxDepth) {
    std::vector<StackFrame> frames;
    // Walk stack, collect addresses
    // Do NOT resolve symbols
    return frames;
}
```

The returned frames have `address` populated but `function`, `file`, `line` empty.

On POSIX with libunwind or execinfo, the core stack walking is async-signal-safe. The `std::vector` allocation is technically not async-signal-safe, but in practice:
- Pre-allocated static storage can be used in signal handlers
- The allocation happens once at the start, not during walking

### Phase 2: resolveSymbols()

```cpp
static void resolveSymbols(std::vector<StackFrame>& frames) {
    for (auto& frame : frames) {
        if (frame.symbolized) continue;
        
        // Load symbol tables
        // Find function name
        // Demangle
        // Find file/line if available
        
        frame.symbolized = true;
    }
}
```

This phase allocates freely: string storage for names, calls to demangling, file I/O for symbol tables.

### The User-Facing API

```cpp
// Convenient: capture and resolve in one call
auto st = Stacktrace::current();

// Async-signal-safe: capture addresses only
auto raw = Stacktrace::captureRaw();
// Later, when safe:
raw.resolveSymbols();
```

Both produce equivalent results; the difference is when symbol resolution happens.

---

# **CHAPTER 9 — The StackFrame Contract: Platform-Unified Data**

### The Problem: Platform Format Chaos

Each platform's native output format differs:

**Linux execinfo:**
```
./myapp(_ZN3foo3barEi+0x23) [0x400abc]
```

**macOS execinfo:**
```
0   myapp                       0x0000000100001234 _ZN3foo3barEi + 35
```

**Windows DbgHelp:**
```
foo::bar() + 0x23 [myapp.exe @ 0x00401234]
```

Parsing these formats is fragile—they change between OS versions.

### The Solution: Unified StackFrame

Stacktrace normalizes all backends to a common structure:

```cpp
struct StackFrame {
    void* address;           // Instruction pointer
    std::string function;    // Demangled function name
    std::string file;        // Source file path
    std::size_t line;        // Line number
    std::size_t column;      // Column number (rarely available)
    std::string module;      // Executable/library name
    void* moduleBase;        // Module load address
    std::size_t offset;      // Bytes from function start
    bool symbolized;         // Resolution attempted?
};
```

Every backend populates the same fields. Missing information is left empty/zero, not represented differently.

### Field Availability by Backend

| Field | C++23 | libunwind | execinfo | DbgHelp |
|-------|-------|-----------|----------|---------|
| address | ✓ | ✓ | ✓ | ✓ |
| function | ✓ | ✓ | ✓ | ✓ |
| file | ✓ | ✗ | ✗ | ✓ |
| line | ✓ | ✗ | ✗ | ✓ |
| module | ✗ | ✓ | ✓ | ✓ |
| offset | ✗ | ✓ | ✓ | ✓ |

libunwind and execinfo use `dladdr()` which doesn't provide file/line. DbgHelp and C++23 have full information when debug info is available.

---

# **CHAPTER 10 — Demangling: From _ZN3foo3barEi to foo::bar(int)**

### Why Demangling Matters

C++ compilers mangle function names to handle:
- Namespaces: `foo::bar` → `_ZN3foo3barE`
- Overloading: `f(int)` vs `f(double)` → different manglings
- Templates: `vector<int>` → encoded template arguments
- Calling conventions, const-ness, etc.

Without demangling, stack traces are unreadable:

```
#0 _ZNSt6vectorIiSaIiEE9push_backEi [0x400abc]
#1 _ZN7MyClass7processERKSt6vectorIiSaIiEE [0x400def]
```

With demangling:

```
#0 std::vector<int>::push_back(int) [0x400abc]
#1 MyClass::process(std::vector<int> const&) [0x400def]
```

### The Demangling Implementation

On POSIX (GCC/Clang ABI):

```cpp
std::string demangle(const char* mangled) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    
    if (status == 0 && demangled) {
        std::string result(demangled);
        free(demangled);
        return result;
    }
    
    return mangled;  // Return original if demangling fails
}
```

On Windows, DbgHelp's `SYMOPT_UNDNAME` option handles demangling automatically.

### Edge Cases

Demangling can fail for:
- C functions (not mangled)
- Symbols from other languages (Rust, D)
- Corrupt symbol data
- Extremely long template names (buffer limits)

Stacktrace always returns *something*—either the demangled name or the original mangled name.

---

# **CHAPTER 11 — Hash and Equality: Enabling Deduplication**

### The Deduplication Use Case

Applications often encounter the same error repeatedly:

```cpp
void log_error(const std::string& msg) {
    auto st = Stacktrace::current();
    
    // Without deduplication: logs same trace thousands of times
    logger.error(msg, st.toString());
}
```

With hash/equality support:

```cpp
std::unordered_set<Stacktrace> seen;

void log_error(const std::string& msg) {
    auto st = Stacktrace::current();
    
    if (seen.insert(st).second) {
        // First occurrence of this trace
        logger.error(msg, st.toString());
    } else {
        // Duplicate - just count it
        error_counts[st.hash()]++;
    }
}
```

### Equality Semantics

Two Stacktraces are equal if they have the same addresses in the same order:

```cpp
bool operator==(const Stacktrace& other) const {
    if (mFrames.size() != other.mFrames.size()) {
        return false;
    }
    for (size_t i = 0; i < mFrames.size(); ++i) {
        if (mFrames[i].address != other.mFrames[i].address) {
            return false;
        }
    }
    return true;
}
```

Function names are ignored for equality—they're derived from addresses anyway. This means unsymbolized and symbolized traces from the same call site are equal.

### Hash Implementation

```cpp
std::size_t hash() const noexcept {
    std::size_t h = 0;
    for (const auto& frame : mFrames) {
        // Combine addresses using a standard hash mixing function
        h ^= std::hash<void*>{}(frame.address) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}
```

The `0x9e3779b9` constant and bit mixing are standard hash combination techniques that reduce collisions.

---

# **PART III — PUTTING IT TOGETHER**

Theory meets practice. These case studies show how Stacktrace's features combine to solve real diagnostic problems.

---

# **CHAPTER 12 — Case Study: Contract Violation Diagnostics**

### The Problem

Contract violations throw exceptions, but the exception message lacks location context:

```cpp
void set_temperature(double kelvin) {
    if (kelvin < 0.0) {
        throw std::domain_error("Temperature cannot be negative");
    }
}
```

### The Solution

Attach a Stacktrace to the exception:

```cpp
class ContractViolationError : public std::logic_error {
public:
    explicit ContractViolationError(const std::string& msg)
        : std::logic_error(msg)
        , mStacktrace(Stacktrace::current(2))  // Skip constructor frames
    {}
    
    const Stacktrace& stacktrace() const { return mStacktrace; }
    
    std::string fullMessage() const {
        return std::string(what()) + "\n\nStack trace:\n" + mStacktrace.toString();
    }
    
private:
    Stacktrace mStacktrace;
};
```

The skip count of 2 removes:
1. `Stacktrace::current()` itself
2. `ContractViolationError` constructor

What remains is the code that violated the contract.

### Integration with FATP_ENFORCE

```cpp
#define FATP_ENFORCE(condition, msg) \
    do { \
        if (!(condition)) { \
            throw ContractViolationError(msg); \
        } \
    } while (false)

// Usage
void set_temperature(double kelvin) {
    FATP_ENFORCE(kelvin >= 0.0, "Temperature cannot be negative");
    mKelvin = kelvin;
}
```

---

# **CHAPTER 13 — Case Study: Production Crash Handler**

### The Problem

A server crashes with SIGSEGV. No debugger attached. Core dumps disabled by policy. Need to capture evidence at crash time.

### The Solution

```cpp
static Stacktrace g_crashTrace;

void crash_handler(int sig) {
    // Phase 1: Async-signal-safe capture
    g_crashTrace = Stacktrace::captureRaw();
    
    // Write raw addresses to pre-opened file descriptor
    int fd = g_crashLogFd;
    for (const auto& frame : g_crashTrace) {
        // Format address manually (no snprintf)
        char buf[32];
        format_address(frame.address, buf);
        write(fd, buf, strlen(buf));
        write(fd, "\n", 1);
    }
    
    // Re-raise for default handling
    signal(sig, SIG_DFL);
    raise(sig);
}

void install_crash_handler() {
    g_crashLogFd = open("/var/log/myapp/crash.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
}
```

### Post-Crash Analysis

A separate tool reads the crash log and resolves symbols:

```cpp
void analyze_crash_log(const std::string& path) {
    auto addresses = load_addresses(path);
    auto trace = create_trace_from_addresses(addresses);
    trace.resolveSymbols();
    std::cout << trace.toString() << std::endl;
}
```

This separation ensures the crash handler never deadlocks, while still providing full symbols in analysis.

---

# **CHAPTER 14 — Case Study: Log Aggregation and Analysis**

### The Problem

A distributed system generates millions of error events. Each needs a stack trace for diagnosis. Traces must be in structured format for the log aggregation pipeline.

### The Solution

```cpp
void log_error(const std::string& error_id, const std::string& message) {
    auto st = Stacktrace::current();
    
    json event = {
        {"timestamp", current_iso_timestamp()},
        {"error_id", error_id},
        {"message", message},
        {"stacktrace", json::parse(st.toJson())},
        {"host", hostname()},
        {"process", getpid()}
    };
    
    kafka_producer.send(event.dump());
}
```

### Deduplication at Aggregation Layer

The log aggregator can group by stack trace hash:

```sql
SELECT 
    stacktrace_hash,
    COUNT(*) as occurrences,
    MIN(timestamp) as first_seen,
    MAX(timestamp) as last_seen,
    ANY_VALUE(stacktrace) as example_trace
FROM error_events
WHERE timestamp > NOW() - INTERVAL 1 HOUR
GROUP BY stacktrace_hash
ORDER BY occurrences DESC;
```

Instead of seeing millions of individual events, operators see "Error X occurred 50,000 times from call site Y."

---

# **CHAPTER 15 — Case Study: Performance Sampling Profiler**

### The Problem

A sampling profiler captures stack traces at 1000 Hz. Symbol resolution at 30 µs per trace would consume 30 ms/second—3% CPU just for symbolization.

### The Solution

Capture raw, deduplicate by address, then resolve only unique traces:

```cpp
class SamplingProfiler {
    std::unordered_map<std::size_t, std::pair<Stacktrace, int>> samples;
    
public:
    void sample() {
        auto raw = Stacktrace::captureRaw();  // 6 µs
        std::size_t hash = raw.hash();
        
        auto& entry = samples[hash];
        if (entry.second == 0) {
            entry.first = std::move(raw);
        }
        entry.second++;
    }
    
    void report() {
        for (auto& [hash, entry] : samples) {
            entry.first.resolveSymbols();  // Resolve once per unique trace
            std::cout << "Samples: " << entry.second << "\n";
            std::cout << entry.first.toString() << "\n\n";
        }
    }
};
```

If 1000 samples collapse to 50 unique call stacks, we resolve 50 traces instead of 1000—a 20x reduction in resolution cost.

---

# **CHAPTER 16 — Choosing the Right Capture Strategy**

### Decision Matrix

| Scenario | Method | Reasoning |
|----------|--------|-----------|
| Exception with stack trace | `current()` | One-time capture, need symbols immediately |
| Crash handler | `captureRaw()` | Async-signal-safety required |
| Debug logging | `current()` | Convenience over performance |
| High-frequency sampling | `captureRaw()` + dedupe | Defer resolution cost |
| Log aggregation | `current()` + `toJson()` | Structured output needed |
| Performance-critical path | Avoid capture | Microsecond cost may matter |

### Performance Guidelines

| Operation | Cost | Acceptable In |
|-----------|------|---------------|
| `captureRaw()` | ~6 µs | Crash handlers, sampling |
| `current()` | ~30 µs | Error paths, infrequent logging |
| `toJson()` | ~10 µs | Structured logging |
| `toString()` | ~3 µs | Debug output |

For code that runs millions of times per second, even 6 µs is too expensive. Don't capture stack traces in hot loops.

---

# **PART IV — FOUNDATIONS**

---

# **APPENDIX A — A History of Stack Unwinding**

### The 1960s: Manual Tracing

Before stack unwinding tools existed, debugging meant reading memory dumps—hexadecimal listings of memory contents. Experienced programmers recognized return address patterns and traced calls manually.

### The 1970s-80s: Debugger Commands

Interactive debuggers like DBX and GDB introduced the `backtrace` command. The debugger walked the stack by following frame pointers, stopping at each return address to look up symbols.

### The 1990s: Runtime Capture

Applications needed stack traces without debuggers—for crash reporting, logging, profiling. Unix systems added `backtrace()` to glibc. Windows provided stack walking APIs in the debugging toolkit.

### The 2000s: Exception Handling Integration

C++ exception handling required stack unwinding for destructor calls. The Itanium C++ ABI formalized unwinding, and libunwind emerged to provide portable access to this machinery.

### The 2010s: Standard Library Proposals

Multiple proposals for `std::stacktrace` worked through the C++ committee. The feature required compiler cooperation for reliable unwinding.

### The 2020s: Standardization

C++23 finally standardized `std::stacktrace`. Implementation varies by compiler, but the interface is portable.

---

# **APPENDIX B — Why Each Backend Exists**

### C++23 std::stacktrace

**Why it exists:** Standard, portable, future-proof. When available, it should be used.

**Limitations:** Requires C++23. Immediate symbol resolution only. No async-signal-safe mode.

### libunwind

**Why it exists:** Reliable unwinding even without frame pointers. Uses exception handling metadata (DWARF CFI) instead of frame pointer walking.

**Limitations:** External library dependency. Not available on all Unix systems.

### execinfo backtrace()

**Why it exists:** Part of glibc, universally available on Linux. Simple API.

**Limitations:** Less reliable without frame pointers. `backtrace_symbols()` allocates memory.

### Windows DbgHelp

**Why it exists:** Microsoft's official debugging infrastructure. Provides symbols, line numbers, everything.

**Limitations:** Windows-only. Requires initialization. Thread safety concerns.

### Stub

**Why it exists:** Stacktrace must compile everywhere. On platforms without any backend, the stub provides placeholder output rather than compilation failure.

**Limitations:** No actual stack trace. Only useful to confirm the code compiles.

---

# **APPENDIX C — Design Constraints and Rejected Alternatives**

### Hard Constraints

1. **Header-only.** No separate compilation, no link-time requirements.
2. **Zero external dependencies.** STL only; no Boost, no separate libraries.
3. **Async-signal-safe capture mode.** Must work in crash handlers.
4. **C++20 minimum.** No C++23 requirements for core functionality.

### Rejected Alternatives

| Alternative | Why Rejected |
|-------------|--------------|
| Virtual backend dispatch | Runtime overhead; doesn't enable compile-time optimization |
| Callback-based API | Complex; doesn't fit C++ idioms |
| Platform-specific interfaces | Defeats portability goal |
| Required libunwind | Not universally available |
| Lazy symbol resolution | Complex lifetime management |
| Mutable StackFrame | Thread safety concerns |

### Accepted Tradeoffs

| Tradeoff | Rationale |
|----------|-----------|
| `std::vector` in captureRaw() | Technically not async-signal-safe, but single allocation; can be worked around with static storage |
| Symbol resolution allocates | Unavoidable for demangling and string storage |
| Per-platform symbol quality varies | Can't control what platforms provide |
| No file/line on some backends | Requires DWARF parsing that libunwind/execinfo don't do |

---

# **APPENDIX D — Where Stacktrace Loses**

### Hot Paths

Even 6 µs for `captureRaw()` is too expensive for code running millions of times per second. Don't capture stack traces in inner loops.

### Embedded Systems

Small embedded targets may lack stack unwinding infrastructure entirely. The stub backend provides nothing useful.

### Stripped Binaries

Without symbols, only addresses are available. The traces work for deduplication but not diagnosis.

### Cross-Compilation Scenarios

Debug info for the target may not be available on the build host. Symbol resolution fails.

### JIT-Compiled Code

JIT code (V8, LLVM JIT) has no debug info in the normal sense. Addresses won't resolve to function names.

---

# **APPENDIX E — The std::stacktrace Gap**

### What C++23 Provides

- Portable stack capture via `std::stacktrace::current()`
- Iterator access to `std::stacktrace_entry` objects
- `std::to_string()` for formatting
- Comparison and hashing

### What C++23 Doesn't Provide

- **Async-signal-safe capture.** `current()` may allocate.
- **Deferred symbol resolution.** Symbols are resolved immediately.
- **JSON output.** `to_string()` produces human-readable text, not structured data.
- **Custom formatting.** Take what the implementation gives you.
- **C++20 availability.** Requires C++23.

### Why These Gaps Matter

Crash handlers need async-signal-safe capture. Log aggregation needs JSON. Most production codebases aren't on C++23 yet. Stacktrace fills these gaps for real-world deployment scenarios.

---

# **APPENDIX F — Further Reading**

**libunwind Documentation**  
https://www.nongnu.org/libunwind/  

**GCC Unwinding Implementation**  
https://gcc.gnu.org/wiki/Dwarf2EHNewbiesHowto

**Windows DbgHelp Reference**  
https://docs.microsoft.com/en-us/windows/win32/debug/debug-help-library

**C++ Name Mangling (Itanium ABI)**  
https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling

**DWARF Debugging Format**  
https://dwarfstd.org/

**Boost.Stacktrace Documentation**  
https://www.boost.org/doc/libs/release/doc/html/stacktrace.html

**C++23 std::stacktrace Paper**  
P0881R7: A Proposal to add stacktrace library

**Signal Safety in POSIX**  
https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_04

---

*End of Companion Guide*

*Stacktrace.h — Fat-P Library v3.2*
