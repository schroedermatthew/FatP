# DiagnosticLogger: A Fat-P Library Showcase

## Executive Summary

DiagnosticLogger is a **policy-based logging system** with compile-time level filtering, pluggable sinks, and structured JSON output. Unlike runtime-filtered loggers (branch on every call) or macro-only solutions (limited composability), DiagnosticLogger uses **`if constexpr` level elimination** to completely remove disabled log statements from the binary. The sink abstraction supports console, file, rotating file, and custom outputs, while JSON formatting enables structured log analysis without string parsing.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The runtime branch tax
void hotPath() {
    logger.debug("Entering hotPath");  // Branch even if DEBUG disabled
    for (int i = 0; i < 1000000; ++i) {
        logger.trace("Iteration ", i);  // 1M branches in release!
        process(i);
    }
}

// The scattered std::cout
void riskyOperation() {
    std::cout << "Starting risky operation" << std::endl;  // Always runs
    // ...
    std::cout << "Step 1 complete" << std::endl;  // No level control
    // ...
    if (error) {
        std::cerr << "Error: " << error.message() << std::endl;  // Different stream
    }
}
// No structured output, no level filtering, no sink control
```

| Issue | HPC Impact |
|-------|------------|
| Runtime level checks | Branch misprediction in hot paths |
| String formatting overhead | Allocations even for disabled levels |
| Unstructured output | Can't machine-parse logs |
| Single sink | Can't log to file AND console |

### The Standard's Limitation

C++ has no standard logging facility:
- `std::clog`/`std::cerr` have no level concept
- No structured output format
- No sink abstraction
- No compile-time filtering

---

## Architecture: Compile-Time Level Elimination

### The Mechanism: `if constexpr` Level Dispatch

```cpp
template<LogLevel CompileTimeLevel>
class DiagnosticLogger {
public:
    template<LogLevel MsgLevel, typename... Args>
    void log(Args&&... args) {
        if constexpr (MsgLevel >= CompileTimeLevel) {
            // Format and emit
            doLog(MsgLevel, format(std::forward<Args>(args)...));
        }
        // When MsgLevel < CompileTimeLevel: function body is EMPTY
    }
    
    template<typename... Args>
    void debug(Args&&... args) { log<LogLevel::Debug>(std::forward<Args>(args)...); }
    
    template<typename... Args>
    void info(Args&&... args) { log<LogLevel::Info>(std::forward<Args>(args)...); }
    
    // etc.
};

// Usage
using ReleaseLogger = DiagnosticLogger<LogLevel::Warning>;
ReleaseLogger logger;
logger.debug("This compiles to nothing");  // Eliminated
logger.warning("This emits");              // Kept
```

**Generated code for disabled levels:** Literally nothing. The `if constexpr (false)` branch is not compiled.

### Sink Abstraction

```cpp
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};

class ConsoleSink : public LogSink { /* ... */ };
class FileSink : public LogSink { /* ... */ };
class RotatingFileSink : public LogSink { /* ... */ };
class JsonFileSink : public LogSink { /* ... */ };

// Compose multiple sinks
logger.addSink(std::make_unique<ConsoleSink>());
logger.addSink(std::make_unique<RotatingFileSink>("app.log", 10_MB, 5));
```

---

## Feature Inventory

### 1. Compile-Time Level Filtering

```cpp
// Configure at compile time via template parameter
using DebugLogger = DiagnosticLogger<LogLevel::Trace>;     // All messages
using ReleaseLogger = DiagnosticLogger<LogLevel::Warning>; // Warning and above

ReleaseLogger logger;
logger.trace("Never compiled");   // Zero cost
logger.debug("Never compiled");   // Zero cost  
logger.info("Never compiled");    // Zero cost
logger.warning("Compiled");       // Emits
logger.error("Compiled");         // Emits
```

### 2. Structured JSON Output

```cpp
JsonFileSink jsonSink("logs/app.json");
logger.addSink(&jsonSink);

logger.info("User logged in", 
    LogField("user_id", 12345),
    LogField("ip", "192.168.1.1"),
    LogField("duration_ms", 42));

// Output:
// {"timestamp":"2024-01-15T10:30:00Z","level":"INFO",
//  "message":"User logged in","user_id":12345,
//  "ip":"192.168.1.1","duration_ms":42}
```

### 3. Source Location Capture (C++20)

```cpp
#if FATP_HAS_CPP20
logger.error("Something failed");
// Automatically captures: file, line, function
// Output: [ERROR] main.cpp:42 (processData): Something failed
#endif
```

### 4. Rotating File Sink

```cpp
// Rotate when file exceeds 10MB, keep 5 backups
auto sink = std::make_unique<RotatingFileSink>(
    "logs/app.log",
    10 * 1024 * 1024,  // 10 MB max size
    5                   // Keep 5 rotated files
);
logger.addSink(std::move(sink));

// Creates: app.log, app.log.1, app.log.2, ... app.log.5
```

### 5. Thread-Safe Emission

```cpp
// Thread-safe by default via mutex
logger.info("From thread 1");  // Safe
logger.info("From thread 2");  // Safe

// High-performance mode with lock-free queue
using FastLogger = DiagnosticLogger<LogLevel::Info, LockFreePolicy>;
```

### 6. Scoped Context

```cpp
{
    auto scope = logger.pushContext("RequestHandler");
    logger.info("Processing request");  // [INFO] [RequestHandler] Processing request
    
    {
        auto inner = logger.pushContext("Validation");
        logger.debug("Validating input");  // [DEBUG] [RequestHandler.Validation] Validating input
    }
}
// Context automatically popped
```

### 7. Conditional Logging

```cpp
// Only format if level is enabled
logger.info_if(expensive_condition(), "Expensive message: ", compute_details());

// With lazy evaluation
logger.info_lazy([&] { 
    return "Expensive to compute: " + expensive_computation(); 
});
```

---

## Why Not Alternatives?

| If You Need... | Why Not spdlog | Why Not glog | Why Not printf | Fat-P Advantage |
|----------------|---------------|--------------|----------------|-----------------|
| Compile-time filtering | ❌ Runtime only | ❌ Runtime only | ❌ No levels | ✅ `if constexpr` |
| Zero dependencies | ❌ fmt library | ❌ gflags | ✅ Standard | ✅ Header-only |
| JSON output | Plugin needed | ❌ No | ❌ No | ✅ Built-in |
| Header-only | ✅ Yes | ❌ No | N/A | ✅ Yes |
| Fat-P integration | ❌ No | ❌ No | ❌ No | ✅ Uses Expected, ScopeGuard |

**The Sweet Spot:** DiagnosticLogger is the only option combining compile-time level elimination, built-in JSON output, header-only distribution, and fat_p ecosystem integration.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ will never standardize logging:
- No consensus on level semantics
- No consensus on sink abstraction
- No consensus on structured output format
- Too domain-specific for the standard

DiagnosticLogger provides a complete logging solution permanently, without waiting for a standard that won't come.

---

## Performance Characteristics

| Scenario | Cost | Notes |
|----------|------|-------|
| Disabled level | 0 ns | Compiled out via `if constexpr` |
| Enabled level (no formatting) | ~5-10 ns | Level check + sink dispatch |
| Enabled level (formatting) | ~50-200 ns | String formatting dominates |
| JSON formatting | ~100-500 ns | Field serialization |
| File write | ~1-10 μs | I/O dominates |

### Where Fat-P Wins
- Hot paths needing trace logging in debug only
- Production systems needing structured JSON logs
- Header-only projects avoiding library dependencies

### Where Fat-P Loses (Honesty Builds Trust)
- Extreme throughput (>1M logs/sec) → async logging libraries
- Complex formatting → spdlog/fmt integration
- Existing spdlog/glog investment → migration cost

---

## Integration Points

```
DiagnosticLogger_Core.h
DiagnosticLogger_Sinks.h
DiagnosticLogger_Json.h
DiagnosticLogger_IO.h
    ↓ uses
Expected.h              (Error handling in sink operations)
ScopeGuard.h            (RAII context management)
JsonLite.h              (JSON serialization)
ConcurrencyPolicies.h   (Thread-safety policies)
    ↓ used by
All fat_p components    (Internal diagnostics)
```

---

## Final Assessment

DiagnosticLogger delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ will never standardize logging—too many competing requirements. DiagnosticLogger provides a complete solution permanently.

### 2. Specialization
Compile-time level filtering via `if constexpr` eliminates disabled logs entirely—no runtime branch, no string formatting, zero cost. This HPC-critical optimization isn't available in runtime-filtered loggers.

### 3. Control
Template-based level selection, pluggable sinks, and threading policies let you configure exactly the logging behavior you need. Debug builds get trace logs; release builds get warnings only—same source code.

**Architectural Verdict:** DiagnosticLogger transforms logging from **runtime-filtered overhead** to **compile-time-eliminated diagnostics**. Disabled levels contribute zero instructions to the binary.

---

*DiagnosticLogger (Core, Sinks, Json, IO — ~2000 lines total) — Fat-P Library*
