---
doc_id: UM-STACKTRACE-001
doc_type: "User Manual"
title: "Stacktrace"
fatp_components: ["Stacktrace", "StackFrame"]
topics: ["stack trace capture", "symbol resolution", "crash diagnostics", "async-signal-safe capture", "demangling", "debug information", "cross-platform diagnostics", "exception debugging"]
constraints: ["async-signal-safety requirements", "symbol resolution cost", "platform backend availability", "debug info dependency", "signal handler restrictions", "memory allocation in handlers"]
cxx_standard: "C++20"
std_equivalent: "std::stacktrace"
std_since: "C++23"
boost_equivalent: "Boost.Stacktrace"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-18"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# User Manual - Stacktrace

*Updated January 2026*

---



**Scope:** Complete usage guide for `fat_p::Stacktrace`: stack frame capture, symbol resolution, demangling, formatting, integration with error handling and logging, and platform-specific behavior.

**Not covered:**
- Post-mortem debugging with core dumps
- Profiler integration
- Sampling-based stack collection

**Prerequisites:** C++20; understanding of call stacks and stack frames; awareness that debug symbols are needed for symbol resolution

---

## User Manual Card

**Component:** Stacktrace
**Primary use case:** Capture and format stack traces at runtime for error reporting, logging, and diagnostic purposes
**Integration pattern:** Call `Stacktrace::capture()` at the point of interest, format with `.toString()` or iterate frames, embed in exception messages or log entries
**Key API:** `Stacktrace::capture()`, `.frames()`, `.toString()`, `StackFrame`, `.functionName()`, `.fileName()`, `.lineNumber()`
**std equivalent:** std::stacktrace (C++23)
**Common mistakes:** Capturing stack traces in hot paths (expensive operation); expecting symbol resolution without debug symbols; assuming stack depth is unlimited (default capture depth is bounded)
**Performance notes:** Capture is expensive (system call + frame walking). Use selectively in error paths, not in hot loops. See `components/Stacktrace/results/` for current data

---
## Table of Contents

1. [The Stack Trace Story](#the-stack-trace-story)
2. [Understanding Why Diagnostics Matter](#understanding-why-diagnostics-matter)
3. [The Platform Portability Problem](#the-platform-portability-problem)
4. [The Signal Handler Problem](#the-signal-handler-problem)
5. [Getting Started](#getting-started)
6. [Capture Methods: current() vs captureRaw()](#capture-methods-current-vs-captureraw)
7. [Frame Skipping and Depth Control](#frame-skipping-and-depth-control)
8. [Symbol Resolution: When and How](#symbol-resolution-when-and-how)
9. [Output Formatting: toString() and toJson()](#output-formatting-tostring-and-tojson)
10. [Working with StackFrame](#working-with-stackframe)
11. [Iterator and Container Interface](#iterator-and-container-interface)
12. [Crash Handler Integration](#crash-handler-integration)
13. [Exception Integration](#exception-integration)
14. [Backend Selection and Detection](#backend-selection-and-detection)
15. [Debug Info Requirements](#debug-info-requirements)
16. [Troubleshooting](#troubleshooting)
17. [API Reference](#api-reference)
18. [Summary](#summary)

---

## The Stack Trace Story

### The Oldest Debugging Technique

Stack traces are as old as subroutine calls. When FORTRAN introduced the `CALL` statement in 1957, debugging became harder—errors could occur deep in call chains, and understanding "how did we get here?" required tracing execution manually.

Early debuggers automated this with the "backtrace" command. GDB's `bt` command, introduced in the 1980s, became the canonical way to answer "what functions called this function?" The operating system maintains a call stack—a chain of return addresses—and the debugger walks this chain, converting addresses to function names.

But debuggers require stopping the program. For production systems—servers handling thousands of requests, games running at 60 fps, scientific simulations on supercomputers—stopping is unacceptable. These systems needed runtime stack capture: the ability to snapshot the call stack programmatically, without a debugger, while the program continues running.

### The Platform Fragmentation

Each operating system developed its own mechanism:

**Unix/Linux** provided `backtrace()` in glibc's `<execinfo.h>`. It captures addresses but returns symbols in a format that varies between glibc versions and requires manual parsing.

**macOS** inherited the BSD `backtrace()` interface but uses a different symbol format and has additional facilities in the `<execinfo.h>` header.

**Windows** provided `CaptureStackBackTrace()` for addresses and the DbgHelp library (`SymFromAddr`, `SymGetLineFromAddr64`) for symbol resolution. Initialization is required, and thread safety requires careful handling.

**libunwind** emerged as a portable solution for POSIX systems, providing reliable unwinding even in the presence of exceptions and optimized code.

**C++23** finally standardized stack traces with `std::stacktrace`, but adoption will take years.

### The Symbol Resolution Challenge

A stack trace without symbols is just a list of numbers:

```
0x55f8a2c01234
0x55f8a2c01567
0x55f8a2c01890
```

Converting addresses to function names requires:

1. **Debug information.** The compiler must emit symbol tables (`-g` flag).
2. **Symbol table access.** The runtime must read the executable's symbol table or external debug files.
3. **Demangling.** C++ mangles names (`_ZN3foo3barEi` → `foo::bar(int)`). The `abi::__cxa_demangle` function reverses this.
4. **Source mapping.** Line number information requires DWARF debug info.

Each step can fail: symbols stripped, debug info unavailable, demangling crashes on malformed input. Robust stack trace capture must handle all these failure modes gracefully.

---

## Understanding Why Diagnostics Matter

### The Exception Without Context

Consider this exception handler:

```cpp
try {
    process_request(request);
} catch (const std::exception& e) {
    log_error(e.what());  // "Invalid matrix dimensions"
}
```

The message tells you *what* went wrong but not *where* or *how*. Was it in the JSON parser? The matrix library? The user's callback? Without a stack trace, diagnosis requires reproducing the error under a debugger—often impossible with production bugs reported by users.

### The Crash Without Evidence

When a program crashes from SIGSEGV, the default behavior is to dump core (if enabled) and terminate. In production, core dumps may be disabled for security or size reasons. Without a stack trace captured at crash time, the only evidence is "it crashed"—insufficient for diagnosis.

### The Contract Violation Without Location

Contract-checking code often reports violations without location:

```cpp
void set_temperature(double kelvin) {
    if (kelvin < 0.0) {
        throw std::domain_error("Temperature cannot be negative");
    }
}
```

When this throws, you know a negative temperature was passed, but not by whom. Adding a stack trace to the exception transforms "temperature cannot be negative" into "temperature cannot be negative, called from PhysicsEngine::step() at physics.cpp:234, called from SimulationLoop::tick() at main.cpp:89."

---

## The Platform Portability Problem

### The Preprocessor Maze

Writing portable stack trace code without abstraction looks like this:

```cpp
#if defined(__linux__) || defined(__APPLE__)
    #include <execinfo.h>
    #include <cxxabi.h>
    
    void capture_trace() {
        void* addresses[64];
        int depth = backtrace(addresses, 64);
        char** symbols = backtrace_symbols(addresses, depth);
        for (int i = 0; i < depth; ++i) {
            // Parse symbols[i] - format varies by platform
            // Demangle C++ names
        }
        free(symbols);
    }
    
#elif defined(_WIN32)
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <DbgHelp.h>
    #pragma comment(lib, "dbghelp.lib")
    
    void capture_trace() {
        static bool initialized = false;
        if (!initialized) {
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
            SymInitialize(GetCurrentProcess(), nullptr, TRUE);
            initialized = true;
        }
        
        void* addresses[64];
        USHORT depth = CaptureStackBackTrace(0, 64, addresses, nullptr);
        
        char buffer[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        
        for (USHORT i = 0; i < depth; ++i) {
            DWORD64 displacement;
            if (SymFromAddr(GetCurrentProcess(), 
                           reinterpret_cast<DWORD64>(addresses[i]),
                           &displacement, symbol)) {
                // symbol->Name contains the function name
            }
        }
    }
#else
    #error "Unsupported platform"
#endif
```

This code is duplicated across every project that needs diagnostics. It's fragile—symbol parsing differs between Linux distributions, DbgHelp initialization has subtle thread safety requirements, demangling has edge cases with templates.

### The Stacktrace Solution

FAT-P Stacktrace encapsulates all platform complexity:

```cpp
#include <fat_p/Stacktrace.h>

void capture_trace() {
    auto st = fat_p::Stacktrace::current();
    std::cout << st.toString();
}
```

One line. Works on Windows, Linux, macOS. Handles demangling. Formats output consistently.

---

## The Signal Handler Problem

### Why Signal Handlers Are Special

When a program crashes (SIGSEGV, SIGABRT, etc.), the operating system invokes a signal handler. Signal handlers execute in a restricted context:

1. **No malloc/free.** The heap may be corrupted (if the crash was heap corruption).
2. **No stdio.** `printf` calls `malloc` internally.
3. **No complex data structures.** `std::string`, `std::vector` allocate memory.
4. **Limited system calls.** Only "async-signal-safe" functions are permitted.

Most stack trace code violates these constraints. `backtrace_symbols()` allocates memory. `std::string` allocates memory. Writing a full stack trace from a signal handler with most libraries is undefined behavior that often works but sometimes deadlocks or corrupts memory.

### The Two-Phase Solution

Stacktrace provides async-signal-safe capture:

```cpp
void crash_handler(int signal) {
    // Phase 1: Capture addresses only (no allocation)
    static fat_p::Stacktrace raw;
    raw = fat_p::Stacktrace::captureRaw();
    
    // Write raw addresses to pre-allocated buffer or file descriptor
    // using async-signal-safe I/O (write(), not fprintf())
    
    // Then either:
    // - Exit and let crash reporter process the dump
    // - Re-raise the signal for default handling
    
    raise(signal);
}

// Later, in crash reporter or dump processor:
void process_crash_dump(fat_p::Stacktrace& raw) {
    raw.resolveSymbols();  // Now safe to allocate
    std::cout << raw.toString();
}
```

`captureRaw()` only collects instruction pointer addresses. On POSIX with libunwind or execinfo, this is async-signal-safe. `resolveSymbols()` performs the memory-allocating symbol lookup, but is called later when it's safe.

---

## Getting Started

### Installation

Stacktrace is a header-only component. Include and use:

```cpp
#include <fat_p/Stacktrace.h>
```

Ensure `CppStandardDetection.h` is also accessible (Stacktrace includes it for platform detection).

### Basic Capture

```cpp
#include <fat_p/Stacktrace.h>
#include <iostream>

void inner_function() {
    auto st = fat_p::Stacktrace::current();
    std::cout << st.toString();
}

void outer_function() {
    inner_function();
}

int main() {
    outer_function();
}
```

Output (with debug info):
```
#0 inner_function()+0x23 at example.cpp:5 [0x55f8a2c01234]
#1 outer_function()+0x15 at example.cpp:10 [0x55f8a2c01456]
#2 main+0x9 at example.cpp:14 [0x55f8a2c01678]
```

### Compilation Requirements

Debug symbols provide the best output:

```bash
# GCC/Clang: Debug info enabled
g++ -g -O2 example.cpp -o example

# MSVC: Generate debug info
cl /Zi /O2 example.cpp
```

Without debug info, file/line information will be unavailable, but function names typically remain (unless symbols are stripped).

---

## Capture Methods: current() vs captureRaw()

### current() — Full Capture with Symbols

`Stacktrace::current()` captures the stack and immediately resolves symbols:

```cpp
auto st = fat_p::Stacktrace::current();
// st.isSymbolized() == true
// Function names, file, line available immediately
```

Use `current()` when:
- You need the stack trace immediately (exception handling, logging)
- Memory allocation is permitted
- You're not in a signal handler

### captureRaw() — Addresses Only

`Stacktrace::captureRaw()` captures only instruction pointer addresses:

```cpp
auto raw = fat_p::Stacktrace::captureRaw();
// raw.isSymbolized() == false
// Only addresses available

raw.resolveSymbols();
// Now symbolized
```

Use `captureRaw()` when:
- You're in a signal handler (async-signal-safe requirement)
- You want to defer symbol resolution cost
- You're capturing many traces and only resolving some

### Performance Comparison

```
captureRaw():      ~6 µs
current():         ~32 µs
resolveSymbols():  ~21 µs
```

For batch operations where you might discard most traces (e.g., sampling profiler), capture raw and resolve only the ones you keep.

---

## Frame Skipping and Depth Control

### Skipping Wrapper Frames

When capturing from a wrapper function, skip the wrapper:

```cpp
class ErrorLogger {
public:
    void logError(const std::string& message) {
        // Skip: logError() and current()
        auto st = Stacktrace::current(2);
        log(message, st);
    }
};
```

The default `skip=1` removes `current()` itself. Additional skips remove your wrapper frames.

### Limiting Depth

For shallow traces or performance-sensitive capture:

```cpp
// Only capture top 5 frames
auto shallow = Stacktrace::current(1, 5);

// Deep trace for full diagnosis
auto deep = Stacktrace::current(1, 128);
```

### Combining Skip and Depth

```cpp
// Skip 3 wrapper frames, capture at most 20 relevant frames
auto st = Stacktrace::current(3, 20);
```

---

## Symbol Resolution: When and How

### Automatic Resolution with current()

`Stacktrace::current()` resolves symbols automatically:

```cpp
auto st = Stacktrace::current();
assert(st.isSymbolized());

for (const auto& frame : st) {
    std::cout << frame.function << "\n";  // Demangled name
}
```

### Deferred Resolution with captureRaw()

`captureRaw()` defers symbol resolution:

```cpp
auto raw = Stacktrace::captureRaw();
assert(!raw.isSymbolized());

// ... do other work ...

raw.resolveSymbols();  // Now resolve
assert(raw.isSymbolized());
```

### Idempotent Resolution

`resolveSymbols()` is safe to call multiple times:

```cpp
auto st = Stacktrace::current();
st.resolveSymbols();  // No-op, already symbolized
st.resolveSymbols();  // Still no-op
```

### What Resolution Provides

| Field | Before Resolution | After Resolution |
|-------|-------------------|------------------|
| `address` | ✓ Available | ✓ Available |
| `function` | Empty | Demangled name (if available) |
| `file` | Empty | Source path (if debug info) |
| `line` | 0 | Line number (if debug info) |
| `column` | 0 | Column (rarely available) |
| `module` | Empty | Library/executable name |
| `offset` | 0 | Bytes from function start |
| `symbolized` | false | true |

---

## Output Formatting: toString() and toJson()

### Human-Readable Output: toString()

```cpp
auto st = Stacktrace::current();
std::cout << st.toString();
```

```
#0 process_request(Request const&)+0x156 at server.cpp:203 [0x55f8a2c01234]
#1 handle_connection(int)+0x89 at server.cpp:145 [0x55f8a2c01567]
#2 main+0x42 at main.cpp:28 [0x55f8a2c01890]
```

### Limiting Output

```cpp
// Show only top 5 frames
std::cout << st.toString(5);
```

```
#0 process_request(Request const&)+0x156 at server.cpp:203 [0x55f8a2c01234]
#1 handle_connection(int)+0x89 at server.cpp:145 [0x55f8a2c01567]
#2 worker_thread()+0x23 at thread.cpp:67 [0x55f8a2c01abc]
#3 std::thread::_Invoker<...>+0x15 at thread:234 [0x55f8a2c01def]
#4 start_thread+0x42 [0x7f8a2c001234]
... (5 more frames)
```

### Machine-Readable Output: toJson()

```cpp
std::string json = st.toJson();
```

```json
[
  {
    "index": 0,
    "address": "0x55f8a2c01234",
    "function": "process_request(Request const&)",
    "file": "server.cpp",
    "line": 203,
    "column": 0,
    "module": "myserver",
    "offset": 342,
    "symbolized": true
  },
  {
    "index": 1,
    "address": "0x55f8a2c01567",
    "function": "handle_connection(int)",
    "file": "server.cpp",
    "line": 145,
    "column": 0,
    "module": "myserver",
    "offset": 137,
    "symbolized": true
  }
]
```

Use JSON for:
- Structured logging systems
- Crash reporting services
- Log aggregation pipelines
- Automated analysis tools

### Stream Output

```cpp
std::cout << st;  // Equivalent to st.toString()
```

---

## Working with StackFrame

### StackFrame Structure

```cpp
struct StackFrame {
    void* address;           // Always available
    std::string function;    // Demangled function name
    std::string file;        // Source file path
    std::size_t line;        // Line number (0 if unknown)
    std::size_t column;      // Column (0 if unknown)
    std::string module;      // Library/executable name
    void* moduleBase;        // Module base address
    std::size_t offset;      // Offset from function start
    bool symbolized;         // Resolution attempted?
};
```

### Frame Formatting

```cpp
StackFrame frame = st[0];

// Full format
std::cout << frame.toString();
// "process_request(Request const&)+0x156 at server.cpp:203 [0x55f8a2c01234]"

// Short format (function only)
std::cout << frame.toStringShort();
// "process_request(Request const&)+0x156"
```

### Frame Comparison

Frames are compared by address:

```cpp
StackFrame a, b;
a.address = reinterpret_cast<void*>(0x1000);
b.address = reinterpret_cast<void*>(0x1000);

assert(a == b);  // Same address
```

---

## Iterator and Container Interface

### Range-For Iteration

```cpp
for (const auto& frame : st) {
    if (frame.function.find("my_module") != std::string::npos) {
        std::cout << "Found: " << frame.toString() << "\n";
    }
}
```

### Standard Iterators

```cpp
auto it = st.begin();
auto end = st.end();

// Reverse iteration
for (auto rit = st.rbegin(); rit != st.rend(); ++rit) {
    std::cout << rit->function << "\n";
}
```

### Indexed Access

```cpp
// Unchecked access (undefined behavior if out of bounds)
const StackFrame& frame = st[0];

// Bounds-checked access (throws std::out_of_range)
const StackFrame& safe = st.at(0);
```

### Size and Emptiness

```cpp
if (!st.empty()) {
    std::cout << "Captured " << st.size() << " frames\n";
}
```

### Hash and Equality for Containers

```cpp
std::unordered_set<fat_p::Stacktrace> unique_traces;
std::unordered_map<fat_p::Stacktrace, int> trace_counts;

auto st = Stacktrace::current();
unique_traces.insert(st);
trace_counts[st]++;
```

---

## Crash Handler Integration

### Basic Crash Handler

```cpp
#include <csignal>
#include <fat_p/Stacktrace.h>

// Pre-allocated storage for async-signal-safe capture
static fat_p::Stacktrace g_crash_trace;

void crash_handler(int signal) {
    // Capture raw (async-signal-safe on POSIX)
    g_crash_trace = fat_p::Stacktrace::captureRaw();
    
    // Write addresses to stderr using write() (async-signal-safe)
    // Or save to pre-opened file descriptor
    
    // Re-raise for default handling (core dump, etc.)
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

void install_crash_handler() {
    std::signal(SIGSEGV, crash_handler);
    std::signal(SIGABRT, crash_handler);
    std::signal(SIGFPE, crash_handler);
    std::signal(SIGILL, crash_handler);
}
```

### Writing Raw Addresses (Async-Signal-Safe)

```cpp
void crash_handler(int signal) {
    g_crash_trace = fat_p::Stacktrace::captureRaw();
    
    // Format addresses manually (no memory allocation)
    char buffer[2048];
    char* ptr = buffer;
    
    for (const auto& frame : g_crash_trace) {
        // Format address as hex (manual, no sprintf)
        ptr += format_hex(frame.address, ptr);
        *ptr++ = '\n';
    }
    
    // write() is async-signal-safe
    write(STDERR_FILENO, buffer, ptr - buffer);
    
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}
```

### Post-Crash Symbol Resolution

If the crash handler saves addresses to a file, a separate tool can resolve them:

```cpp
void process_crash_log(const std::string& log_file) {
    auto trace = load_addresses_from_file(log_file);
    trace.resolveSymbols();
    std::cout << trace.toString();
}
```

---

## Exception Integration

### Adding Stack Traces to Exceptions

```cpp
#include <fat_p/Stacktrace.h>
#include <stdexcept>

class TracedException : public std::runtime_error {
public:
    explicit TracedException(const std::string& message)
        : std::runtime_error(message)
        , mStacktrace(fat_p::Stacktrace::current(2))  // Skip constructor
    {}
    
    const fat_p::Stacktrace& stacktrace() const noexcept {
        return mStacktrace;
    }
    
    std::string fullMessage() const {
        std::string result = what();
        result += "\n\nStack trace:\n";
        result += mStacktrace.toString();
        return result;
    }
    
private:
    fat_p::Stacktrace mStacktrace;
};
```

### Usage

```cpp
void validate(int value) {
    if (value < 0) {
        throw TracedException("Value must be non-negative");
    }
}

try {
    validate(-1);
} catch (const TracedException& e) {
    std::cerr << e.fullMessage() << std::endl;
}
```

Output:
```
Value must be non-negative

Stack trace:
#0 validate(int)+0x45 at validation.cpp:4 [0x55f8a2c01234]
#1 process(Data const&)+0x89 at processor.cpp:56 [0x55f8a2c01567]
#2 main+0x23 at main.cpp:12 [0x55f8a2c01890]
```

---

## Backend Selection and Detection

### Automatic Backend Selection

Stacktrace automatically selects the best available backend at compile time:

```
Priority:
1. C++23 std::stacktrace    (FATP_HAS_STD_STACKTRACE)
2. libunwind               (FATP_HAS_LIBUNWIND)
3. execinfo backtrace()    (FATP_HAS_EXECINFO)
4. Windows DbgHelp         (FATP_HAS_DBGHELP)
5. Stub fallback           (always available)
```

### Querying Backend at Runtime

```cpp
std::cout << "Backend: " << fat_p::Stacktrace::backendName() << "\n";
// e.g., "libunwind", "execinfo", "Windows DbgHelp", "stub"

if (fat_p::Stacktrace::hasRealBackend()) {
    // Real stack traces available
} else {
    // Stub backend - traces will be placeholder only
}
```

### Backend Detection Macros

```cpp
#if FATP_HAS_STD_STACKTRACE
    // Using C++23 std::stacktrace
#elif FATP_HAS_LIBUNWIND
    // Using libunwind
#elif FATP_HAS_EXECINFO
    // Using execinfo backtrace()
#elif FATP_HAS_DBGHELP
    // Using Windows DbgHelp
#else
    // Stub fallback
#endif
```

---

## Debug Info Requirements

### Compilation Flags

| Compiler | Debug Symbols | Optimization | Result |
|----------|--------------|--------------|--------|
| GCC/Clang | `-g` | Any | Full symbols, file/line |
| GCC/Clang | None | Any | Function names only |
| MSVC | `/Zi` | Any | Full symbols, file/line |
| MSVC | None | Any | Exported symbols only |

### Release Builds with Debug Info

For production crash diagnostics, compile with both optimization and debug info:

```bash
# GCC/Clang
g++ -O2 -g myapp.cpp -o myapp

# MSVC
cl /O2 /Zi myapp.cpp
```

The debug info can be stripped into separate files for deployment:

```bash
# Extract debug info to separate file
objcopy --only-keep-debug myapp myapp.debug
objcopy --strip-debug myapp
objcopy --add-gnu-debuglink=myapp.debug myapp
```

### What's Available Without Debug Info

| Information | With `-g` | Without `-g` |
|-------------|-----------|--------------|
| Addresses | ✓ | ✓ |
| Function names | ✓ | ✓ (if not stripped) |
| File paths | ✓ | ✗ |
| Line numbers | ✓ | ✗ |
| Column numbers | Sometimes | ✗ |

---

## Troubleshooting

### Empty or Short Stack Traces

**Symptom:** Stack trace has fewer frames than expected.

**Causes:**
1. **Frame pointer optimization.** `-fomit-frame-pointer` (default at `-O2`) can prevent unwinding. libunwind handles this; execinfo may not.
2. **Inline functions.** Inlined functions don't appear in stack traces.
3. **Depth limit reached.** Increase `maxDepth` parameter.

**Solution:**
```bash
# Preserve frame pointers for better traces
g++ -fno-omit-frame-pointer -O2 -g myapp.cpp
```

### Missing Function Names

**Symptom:** Frames show only addresses, not function names.

**Causes:**
1. **No debug symbols.** Compile with `-g`.
2. **Symbols stripped.** Don't run `strip` on the binary.
3. **Dynamic library not loaded.** Symbol resolution may fail for unloaded libraries.

**Solution:**
```bash
# Check if symbols are present
nm myapp | head
readelf -s myapp | head
```

### Mangled Names in Output

**Symptom:** Function names look like `_ZN3foo3barEi` instead of `foo::bar(int)`.

**Cause:** Demangling failed.

**Solution:** This is rare with FAT-P's built-in demangling. If it occurs, the mangled name is returned as-is. You can demangle manually:

```bash
c++filt _ZN3foo3barEi
# Output: foo::bar(int)
```

### Crash in Signal Handler

**Symptom:** Deadlock or double-fault when signal handler captures stack.

**Cause:** Using `current()` (which allocates) instead of `captureRaw()` in signal handler.

**Solution:** Always use `captureRaw()` in signal handlers:

```cpp
void handler(int sig) {
    auto raw = Stacktrace::captureRaw();  // Safe
    // NOT: auto st = Stacktrace::current();  // Unsafe!
}
```

### Stub Backend Active

**Symptom:** `backendName()` returns `"stub"` and traces are placeholders.

**Cause:** No stack trace backend was detected at compile time.

**Solution:**
- **Linux/macOS:** Install libunwind-dev or ensure glibc provides execinfo
- **Windows:** Ensure Windows SDK is installed
- **C++23:** Ensure compiler supports `<stacktrace>` header

---

---

## Use Case: Enhanced Exception Messages

Capture a stack trace when throwing an exception:

```cpp
class TracedException : public std::runtime_error
{
public:
    TracedException(const std::string& msg)
        : std::runtime_error(msg)
        , trace_(fat_p::Stacktrace::capture())
    {}

    const fat_p::Stacktrace& trace() const { return trace_; }

private:
    fat_p::Stacktrace trace_;
};

// In catch handler:
catch (const TracedException& e)
{
    log_error("{}\nStack trace:\n{}", e.what(), e.trace().to_string());
}
```

## Use Case: Memory Leak Tracker

Record allocation stack traces for leak detection:

```cpp
struct AllocRecord
{
    void* ptr;
    size_t size;
    fat_p::Stacktrace trace;
};

std::unordered_map<void*, AllocRecord> live_allocations;

void* tracked_alloc(size_t size)
{
    void* ptr = malloc(size);
    live_allocations[ptr] = {ptr, size, fat_p::Stacktrace::capture()};
    return ptr;
}

void report_leaks()
{
    for (auto& [ptr, record] : live_allocations)
    {
        log_warning("Leak: {} bytes at {}\n{}", record.size, ptr,
                    record.trace.to_string());
    }
}
```

## Use Case: Assertion Failure Diagnostics

Include stack traces in enforce/assertion failures:

```cpp
void custom_enforce_handler(const char* expr, const char* file, int line)
{
    auto trace = fat_p::Stacktrace::capture();
    log_fatal("Assertion failed: {} at {}:{}\n{}", expr, file, line,
              trace.to_string());
    std::abort();
}
```

## Best Practices

**Capture sparingly in hot paths.** Stack trace capture is expensive (~1-50 μs depending on depth and backend). Capture on error paths, not on every function call.

**Use skip parameter to omit framework frames.** `capture(skip=2)` omits the capture function itself and its caller, showing the user's code first.

**Prefer to_string() for logging, frames() for programmatic access.** `to_string()` produces human-readable output. `frames()` returns structured data for filtering or serialization.

## Expanded Troubleshooting

### Stack trace shows only addresses, no function names

Symbol information is not available. On Linux, compile with `-g` and link with `-rdynamic`. On Windows, ensure PDB files are present. On macOS, use `dsymutil` to generate debug symbols.

### capture() returns empty trace

The backend may not be available. Check `Stacktrace::backend()` to see which backend is active. If "none", no stack trace support was detected at compile time.

### Function names are mangled

The backend returns raw symbol names. Use `to_string()` which runs demangling automatically. For programmatic access, use `abi::__cxa_demangle` (GCC/Clang) or `UnDecorateSymbolName` (MSVC).

---

*Tier C Manual Expansions — Fat-P Library*

---

## API Reference

### Stacktrace Class

#### Static Factory Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `current(skip=1, maxDepth=64)` | `Stacktrace` | Capture with immediate symbol resolution |
| `captureRaw(skip=1, maxDepth=64)` | `Stacktrace` | Capture addresses only (async-signal-safe) |

#### Instance Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `frames()` | `const std::vector<StackFrame>&` | Access all frames |
| `size()` | `std::size_t` | Number of frames |
| `empty()` | `bool` | True if no frames |
| `operator[](index)` | `const StackFrame&` | Unchecked frame access |
| `at(index)` | `const StackFrame&` | Bounds-checked access |
| `begin()` / `end()` | Iterator | Range iteration |
| `rbegin()` / `rend()` | Iterator | Reverse iteration |
| `resolveSymbols()` | `void` | Resolve symbols (idempotent) |
| `isSymbolized()` | `bool` | True if symbols resolved |
| `toString(maxFrames=0)` | `std::string` | Human-readable format |
| `toJson()` | `std::string` | JSON array format |
| `hash()` | `std::size_t` | Hash for containers |

#### Static Information

| Method | Returns | Description |
|--------|---------|-------------|
| `backendName()` | `const char*` | Active backend identifier |
| `hasRealBackend()` | `bool` | True if real traces available |

### StackFrame Structure

| Field | Type | Description |
|-------|------|-------------|
| `address` | `void*` | Instruction pointer |
| `function` | `std::string` | Demangled function name |
| `file` | `std::string` | Source file path |
| `line` | `std::size_t` | Line number (0 if unknown) |
| `column` | `std::size_t` | Column (0 if unknown) |
| `module` | `std::string` | Library/executable name |
| `moduleBase` | `void*` | Module load address |
| `offset` | `std::size_t` | Offset from function start |
| `symbolized` | `bool` | Resolution attempted |

| Method | Returns | Description |
|--------|---------|-------------|
| `toString()` | `std::string` | Full frame description |
| `toStringShort()` | `std::string` | Function + offset only |

---

## Summary

Stacktrace provides portable stack trace capture with three key capabilities:

**Multi-backend abstraction.** One API works across C++23, libunwind, execinfo, and Windows DbgHelp. No platform-specific code in your application.

**Two-phase capture.** `captureRaw()` is async-signal-safe for crash handlers. `resolveSymbols()` defers the memory-allocating symbol lookup until it's safe.

**Structured output.** `toString()` for human-readable logs, `toJson()` for structured logging systems, hash/equality for deduplication.

Use Stacktrace for exception diagnostics, crash handlers, and debug logging. Compile with `-g` for full symbol information. Use `captureRaw()` in signal handlers.

---

## Read Next

- **[Overview - Stacktrace](Overview_-_Stacktrace.md)** — Executive summary and positioning
- **[Companion Guide - Stacktrace](Companion_Guide_-_Stacktrace.md)** — Design rationale and case studies
- **CppStandardDetection.h** — Platform detection macros used by Stacktrace

---

*Stacktrace.h — Fat-P Library v3.2*
