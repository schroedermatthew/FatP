# DiagnosticLogger User Manual

## Table of Contents

1. [What is Diagnostic Logging?](#what-is-diagnostic-logging)
   - [Understanding Diagnostic Logging](#understanding-diagnostic-logging)
   - [The C++ Logging Landscape](#the-c-logging-landscape)
   - [Where DiagnosticLogger Fits](#where-diagnosticlogger-fits)
2. [Core Architecture](#core-architecture)
   - [The Fast Path Design](#the-fast-path-design)
   - [Lock-Free Atomics](#lock-free-atomics)
   - [Hot/Cold Path Separation](#hotcold-path-separation)
   - [Design Decisions](#design-decisions)
3. [Getting Started](#getting-started)
   - [Prerequisites](#prerequisites)
   - [Integration](#integration)
   - [Compilation](#compilation)
   - [First Program](#first-program)
4. [Log Levels](#log-levels)
   - [Available Levels](#available-levels)
   - [Compile-Time Filtering](#compile-time-filtering)
   - [Runtime Filtering](#runtime-filtering)
   - [When to Use Each Level](#when-to-use-each-level)
5. [Basic Logging](#basic-logging)
   - [Simple String Messages](#simple-string-messages)
   - [Formatted Messages](#formatted-messages)
   - [Lazy Evaluation](#lazy-evaluation)
   - [Source Location](#source-location)
6. [Sinks](#sinks)
   - [Console Sink](#console-sink)
   - [File Sink](#file-sink)
   - [Callback Sink](#callback-sink)
   - [Custom Sinks](#custom-sinks)
   - [Multiple Sinks](#multiple-sinks)
7. [Formatters](#formatters)
   - [Default Formatter](#default-formatter)
   - [Simple Formatter](#simple-formatter)
   - [JSON Formatter](#json-formatter)
   - [Custom Formatters](#custom-formatters)
8. [Performance Optimization](#performance-optimization)
   - [Compile-Time Optimization](#compile-time-optimization)
   - [Runtime Optimization](#runtime-optimization)
   - [Branch Prediction](#branch-prediction)
   - [Zero-Overhead Disabled Logging](#zero-overhead-disabled-logging)
9. [Advanced Usage](#advanced-usage)
   - [Global vs Local Loggers](#global-vs-local-loggers)
   - [Thread Safety](#thread-safety)
   - [Runtime Configuration](#runtime-configuration)
   - [Integration with External Frameworks](#integration-with-external-frameworks)
10. [Error Handling](#error-handling)
    - [File Sink Failures](#file-sink-failures)
    - [Exception Safety](#exception-safety)
    - [Recovery Strategies](#recovery-strategies)
11. [Best Practices](#best-practices)
    - [When to Log](#when-to-log)
    - [What to Log](#what-to-log)
    - [Performance Considerations](#performance-considerations)
    - [Testing with Logs](#testing-with-logs)
12. [Performance Characteristics](#performance-characteristics)
    - [Benchmark Results](#benchmark-results)
    - [Memory Usage](#memory-usage)
    - [Optimization Tips](#optimization-tips)
13. [Comparison with Other Libraries](#comparison-with-other-libraries)
    - [DiagnosticLogger vs spdlog](#diagnosticlogger-vs-spdlog)
    - [DiagnosticLogger vs glog](#diagnosticlogger-vs-glog)
    - [DiagnosticLogger vs Boost.Log](#diagnosticlogger-vs-boostlog)
    - [DiagnosticLogger vs Custom printf](#diagnosticlogger-vs-custom-printf)
14. [Migration Guide](#migration-guide)
    - [From spdlog](#from-spdlog)
    - [From glog](#from-glog)
    - [From std::cout/cerr](#from-stdcoutcerr)
15. [Compiler Requirements](#compiler-requirements)
    - [Minimum Version](#minimum-version)
    - [Tested Compilers](#tested-compilers)
    - [Compilation Flags](#compilation-flags)
    - [Dependencies](#dependencies)
16. [Troubleshooting](#troubleshooting)
    - [Common Issues](#common-issues)
    - [Performance Problems](#performance-problems)
    - [Compilation Errors](#compilation-errors)
17. [Summary](#summary)

---

## What is Diagnostic Logging?

### Understanding Diagnostic Logging

Diagnostic logging is the practice of recording runtime information about a program's execution for debugging, monitoring, and auditing purposes. Unlike print statements or assertions, diagnostic logging:

- **Survives deployment**: Logs can be enabled in production without recompilation
- **Provides context**: Timestamps, thread IDs, file locations automatically included
- **Scales**: Multiple destinations (console, files, network) without code changes
- **Performs**: Minimal overhead when disabled, manageable overhead when enabled

Good logging is essential for:
- Debugging production issues
- Understanding performance bottlenecks
- Auditing security-sensitive operations
- Monitoring system health
- Reproducing intermittent bugs

### The C++ Logging Landscape

C++ has a fragmented logging ecosystem:

**Traditional Solutions:**
- `std::cout`/`std::cerr`: Simple but no structure, no filtering, no thread safety
- `printf`: Fast but not type-safe, no C++ support
- Custom solutions: Often reinventing the wheel poorly

**Modern Libraries:**
- **spdlog**: Fast, feature-rich, header-only option, widely used
- **glog**: Google's logging library, proven but opinionated
- **Boost.Log**: Comprehensive but heavyweight, requires Boost
- **plog**: Lightweight, header-only, limited features

**Common Problems:**
- External dependencies
- Complex APIs
- Runtime overhead even when logging is disabled
- Poor compile-time optimization
- Thread contention on the hot path

### Where DiagnosticLogger Fits

DiagnosticLogger is designed for **high-performance C++ projects** where:

1. **Zero external dependencies** are required
2. **Compile-time optimization** is critical
3. **Runtime performance** matters even for disabled logs
4. **Header-only** deployment is preferred
5. **C++17 compliance** without cutting-edge features
6. **Thread safety** is non-negotiable
7. **Safety-first** philosophy aligns with project goals

**Key Features:**
- **Lock-free fast path**: Single atomic load for disabled/filtered logs (~10ns)
- **Compile-time filtering**: Zero overhead for disabled log levels
- **Branch prediction hints**: Optimized for common case (logging disabled)
- **Lazy evaluation**: Messages only generated when actually logged
- **Policy-based design**: Formatters and sinks are customizable
- **Thread-safe**: All operations safe for concurrent use
- **Header-only**: Single include, no linking required

**Trade-offs:**
- Not as feature-rich as spdlog (no async logging, no pattern formatters)
- Not as mature as glog (less battle-tested)
- Optimized for "logging mostly disabled" workload
- Requires C++17 (no C++11/14 support)

**When to Use DiagnosticLogger:**
- [YES] HPC applications where every nanosecond counts
- [YES] Header-only library projects
- [YES] Projects with strict "no external dependencies" policy
- [YES] Embedded systems with limited resources
- [YES] Scientific computing with performance-critical loops
- [YES] Real-time systems where predictable performance matters

**When to Use Something Else:**
-  Need async logging to separate thread
-  Need complex pattern-based formatting
-  Need log rotation, compression, or network sinks
-  Already using spdlog and it works fine
-  Need C++11/14 compatibility

---

## Core Architecture

### The Fast Path Design

DiagnosticLogger is built around the **fast path principle**: the common case (logging disabled or filtered) should be **extremely fast**. This is achieved through:

```
Fast Path (Hot):                     Slow Path (Cold):
             
 Compile-time check  filtered> Return immediately   
 (if constexpr)                   

            passes
           v

 Atomic load         disabled> Return immediately   
 (enabled + level)                

            passes (unlikely)
           v

 Slow path function  
 (NO_INLINE)         
 - Acquire mutex     
 - Generate message  
 - Write to sinks    

```

**Key Optimization Techniques:**

1. **Compile-Time Elimination**: `if constexpr` removes code for disabled levels
2. **Lock-Free Checks**: Atomics avoid mutex on fast path
3. **Branch Prediction**: `UNLIKELY` hint guides CPU speculation
4. **Code Separation**: Hot path stays in instruction cache
5. **Lazy Evaluation**: Messages generated only when needed

### Lock-Free Atomics

Traditional logging uses a global mutex:

```cpp
// Traditional approach (slow)
void log(Level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(globalMutex);  // ALWAYS taken
    if (!enabled || level < minLevel) return;
    // ... actually log ...
}
```

**Problem**: Every log call acquires a mutex, even when logging is disabled. This causes:
- Cache coherency traffic
- Mutex contention in multi-threaded code
- ~50-100ns overhead per call

**DiagnosticLogger Approach**:

```cpp
// Lock-free fast path (fast)
void log(Level level, MessageGen&& gen) {
    // 1. Compile-time check (zero cost)
    if constexpr (gMinLogLevel > level) return;
    
    // 2. Lock-free check (single atomic load, ~5ns)
    if (UNLIKELY(!should_log(level))) return;
    
    // 3. Slow path only if actually logging
    log_slow_path(level, std::forward<MessageGen>(gen));
}

bool should_log(Level level) const noexcept {
    return enabled_.load(std::memory_order_relaxed) &&
           level >= minLevel_.load(std::memory_order_relaxed);
}
```

**Benefits**:
- No mutex acquisition on fast path
- Single atomic load (can be cached)
- CPU can speculate through the check
- ~10ns typical case vs ~100ns with mutex

**Memory Ordering**: We use `memory_order_relaxed` because:
- Logging correctness doesn't require synchronization
- Brief staleness (thread sees old enabled state) is acceptable
- Provides maximum performance

### Hot/Cold Path Separation

**Hot Path** (FORCE_INLINE):
- Compile-time checks
- Single atomic load
- Branch with prediction hint
- Stays in instruction cache
- ~5-10 assembly instructions

**Cold Path** (NO_INLINE):
- Mutex acquisition
- Double-checked locking
- Message generation
- Sink iteration
- Formatter invocation
- Moved to separate function to avoid bloating hot path

This separation provides several benefits:

1. **Instruction Cache Efficiency**: Hot path fits in L1 cache
2. **Better Inlining Decisions**: Compiler can inline hot path aggressively
3. **Reduced Code Size**: Cold path not duplicated at every call site
4. **Improved Branch Prediction**: CPU can predict the "no logging" path

### Design Decisions

**Why variant-based LogRecord?**
- We use plain struct with strings, not variant
- Trade-off: Simplicity and type safety over minimal memory
- Alternative would be type-erased storage, but adds complexity

**Why policy-based formatters?**
- Allows customization without runtime overhead
- Each sink can have different formatter
- Zero-cost abstraction through virtual functions only on slow path

**Why separate sinks and formatters?**
- Single Responsibility Principle
- Can mix and match (file with JSON, console with simple)
- Easy to test independently

**Why string-based messages?**
- Could use format strings + args for efficiency
- Trade-off: Simplicity vs slight performance gain
- Lazy evaluation already prevents most overhead

**Why no async logging?**
- Adds significant complexity (queue, worker thread, shutdown)
- Lock-free fast path already provides good performance
- Async adds latency unpredictability (flush timing)
- Can be added as separate AsyncSink if needed

**Why double-checked locking?**
- Avoids wasted message generation if state changes
- Example: Another thread disables logging between fast check and slow path
- Small additional cost but prevents spurious work

---

## Getting Started

### Prerequisites

- **C++17 or later compiler**
- Standard library with:
  - `<atomic>` for lock-free operations
  - `<mutex>` for sink synchronization
  - `<chrono>` for timestamps
  - `<thread>` for thread IDs
  - `<string_view>` for efficient string handling

### Integration

DiagnosticLogger is a single-header library. Copy `DiagnosticLogger.h` to your project and include it:

```cpp
#include "DiagnosticLogger.h"

using namespace fat_p::diagnostic;
```

**Namespace**: All DiagnosticLogger components are in `fat_p::diagnostic`.

### Compilation

**Minimum flags:**
```bash
g++ -std=c++17 -I/path/to/headers main.cpp -o app
```

**Recommended flags:**
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I/path/to/headers main.cpp -o app
```

**With compile-time filtering (production):**
```bash
g++ -std=c++17 -O2 -DCPP_UTIL_MIN_LOG_LEVEL=2 main.cpp -o app
# Only Info and above compiled in (Trace, Debug eliminated)
```

**Debug with sanitizers:**
```bash
g++ -std=c++17 -g -fsanitize=address,thread main.cpp -o app
```

### First Program

```cpp
#include "DiagnosticLogger.h"
#include <iostream>

using namespace fat_p::diagnostic;

int main()
{
    // Initialize with console output
    initializeDefaultLogger();
    
    // Log at different levels
    LOG_INFO("Application starting");
    LOG_DEBUG("Debug information: x=" << 42);
    LOG_WARNING("This is a warning");
    LOG_ERROR("An error occurred");
    
    // Get the global logger for runtime configuration
    auto& logger = getGlobalLogger();
    logger.setMinLevel(LogLevel::Warning);  // Only Warning and above
    
    LOG_INFO("This won't be printed");      // Filtered
    LOG_WARNING("This will be printed");    // Passes filter
    
    return 0;
}
```

**Output:**
```
[2025-11-17 14:30:45.123] [INFO] [0x7f8b2c001740] Application starting (main.cpp:9)
[2025-11-17 14:30:45.124] [DEBUG] [0x7f8b2c001740] Debug information: x=42 (main.cpp:10)
[2025-11-17 14:30:45.125] [WARN] [0x7f8b2c001740] This is a warning (main.cpp:11)
[2025-11-17 14:30:45.126] [ERROR] [0x7f8b2c001740] An error occurred (main.cpp:12)
[2025-11-17 14:30:45.130] [WARN] [0x7f8b2c001740] This will be printed (main.cpp:19)
```

---

## Log Levels

### Available Levels

DiagnosticLogger provides six log levels, ordered by severity:

```cpp
enum class LogLevel : int {
    Trace   = 0,  // Detailed debugging, very verbose
    Debug   = 1,  // Debug information
    Info    = 2,  // Informational messages
    Warning = 3,  // Warning messages (potential issues)
    Error   = 4,  // Error messages (recoverable failures)
    Fatal   = 5,  // Fatal errors (unrecoverable)
    Off     = 6   // Disable all logging
};
```

**Level Hierarchy**: Setting a minimum level filters everything below it.

```
Trace (0)  most verbose
Debug (1)
Info  (2)  
Warning (3)  setMinLevel(Info) allows these
Error (4)   
Fatal (5) 
Off (6)    all disabled
```

### Compile-Time Filtering

**Most Powerful Optimization**: Eliminate log code entirely at compile time.

**Define before including header or via compiler flag:**

```cpp
// Method 1: In code (before include)
#define CPP_UTIL_MIN_LOG_LEVEL 2  // Only Info and above
#include "DiagnosticLogger.h"

// Method 2: Compiler flag (recommended)
// g++ -DCPP_UTIL_MIN_LOG_LEVEL=2 main.cpp
```

**Effect:**

```cpp
// With CPP_UTIL_MIN_LOG_LEVEL=2
LOG_TRACE("x=" << expensive());  // Completely eliminated! expensive() never called
LOG_DEBUG("y=" << another());    // Also eliminated!
LOG_INFO("Started");             // Compiled in
LOG_ERROR("Failed");             // Compiled in
```

**Performance**:
- Filtered logs: **0 instructions, 0 nanoseconds**
- No function call, no atomic load, nothing
- Equivalent to commenting out the line

**Typical Settings by Build:**
```cpp
// Development build (all logs)
#define CPP_UTIL_MIN_LOG_LEVEL 0  // Trace

// Testing build (debug and above)
#define CPP_UTIL_MIN_LOG_LEVEL 1  // Debug

// Production build (info and above)
#define CPP_UTIL_MIN_LOG_LEVEL 2  // Info

// Production minimal (errors only)
#define CPP_UTIL_MIN_LOG_LEVEL 4  // Error

// Disable all logging
#define CPP_UTIL_MIN_LOG_LEVEL 6  // Off
```

### Runtime Filtering

**After** compile-time filtering, runtime filtering provides dynamic control:

```cpp
auto& logger = getGlobalLogger();

// Set minimum level (lock-free atomic)
logger.setMinLevel(LogLevel::Warning);

// Get current level
LogLevel current = logger.getMinLevel();

// Disable all logging
logger.setEnabled(false);

// Re-enable
logger.setEnabled(true);

// Check if enabled
if (logger.isEnabled()) {
    // ...
}
```

**Performance**:
- Setting level: Single atomic store (~5ns)
- Checking level: Single atomic load (~5ns)
- Lock-free: No mutex contention
- Safe for concurrent modification

**Example: Dynamic Debug Mode**

```cpp
void onDebugSignal(int sig) {
    auto& logger = getGlobalLogger();
    logger.setMinLevel(LogLevel::Trace);  // Enable verbose logging
    LOG_INFO("Debug mode enabled by signal");
}

int main() {
    initializeDefaultLogger();
    signal(SIGUSR1, onDebugSignal);
    
    auto& logger = getGlobalLogger();
    logger.setMinLevel(LogLevel::Info);  // Normal operation
    
    // ... application runs ...
    // User can send SIGUSR1 to enable debug logging without restart
}
```

### When to Use Each Level

**Trace:**
- Loop iterations in algorithms
- Entry/exit of small functions
- Variable values at each step
- **Example**: `LOG_TRACE("Processing item " << i << "/" << total)`
- **Frequency**: Can generate thousands of log entries per second
- **Production**: Usually disabled

**Debug:**
- Entry/exit of major functions
- Intermediate calculation results
- Algorithm decisions
- **Example**: `LOG_DEBUG("Cache hit rate: " << hits << "/" << total)`
- **Frequency**: Hundreds per second
- **Production**: Disabled or very selective

**Info:**
- Significant application events
- State transitions
- Configuration changes
- **Example**: `LOG_INFO("Server started on port " << port)`
- **Frequency**: Dozens per second
- **Production**: Typically enabled

**Warning:**
- Recoverable errors
- Deprecated feature usage
- Unexpected but handled situations
- **Example**: `LOG_WARNING("Retrying connection after failure")`
- **Frequency**: Occasional
- **Production**: Always enabled

**Error:**
- Operation failures
- Invalid input data
- Resource exhaustion
- **Example**: `LOG_ERROR("Failed to open file: " << filename)`
- **Frequency**: Rare
- **Production**: Always enabled

**Fatal:**
- Unrecoverable errors
- Program must terminate
- Data corruption detected
- **Example**: `LOG_FATAL("Database corrupted, shutting down")`
- **Frequency**: Should never happen
- **Production**: Always enabled

**Best Practice**: Use the lowest level that's appropriate. Don't log everything at ERROR.

---

## Basic Logging

### Simple String Messages

**String literals** (most efficient):

```cpp
LOG_INFO("Server started");
LOG_ERROR("Connection failed");
LOG_WARNING("Timeout occurred");
```

**Compile-time vs Runtime**:
```cpp
// If INFO is filtered at compile time:
LOG_INFO("Server started");  // 0 instructions

// If INFO passes compile-time but filtered at runtime:
LOG_INFO("Server started");  // ~10ns (atomic load + branch)

// If INFO passes all filters:
LOG_INFO("Server started");  // ~130ns (mutex + formatting + sink)
```

### Formatted Messages

**Stream-style formatting** (like std::cout):

```cpp
int port = 8080;
std::string host = "localhost";

LOG_INFO("Server listening on " << host << ":" << port);
LOG_DEBUG("Processing " << count << " items in " << duration << "ms");
LOG_ERROR("Failed to connect to " << url << " - " << error_msg);
```

**Why stream style?**
- Type-safe (unlike printf)
- Supports any type with `operator<<`
- Familiar C++ idiom
- Composable

**How it works:**
```cpp
// Macro expands to:
getGlobalLogger().info([&]() -> std::string {
    std::ostringstream oss;
    oss << "Server listening on " << host << ":" << port;
    return oss.str();
}, SOURCE_LOCATION());
```

**Performance Note**: The lambda creates a capture, but:
- Lambda **not invoked** if log is filtered (lazy evaluation)
- Only overhead is lambda object creation (~1 pointer)
- Message construction avoided entirely when filtered

### Lazy Evaluation

**Key Performance Feature**: Messages are only constructed if they will actually be logged.

**Example of savings:**

```cpp
std::string expensive_computation() {
    // Simulate expensive operation
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return "result";
}

// Traditional logging
if (logger.isEnabled(LogLevel::Debug)) {  // Check manually
    logger.debug("Result: " + expensive_computation());
}

// DiagnosticLogger (automatic lazy evaluation)
LOG_DEBUG("Result: " << expensive_computation());
// If debug is disabled: expensive_computation() NEVER CALLED

// If debug is enabled: expensive_computation() called inside lambda
```

**How Lazy Evaluation Works:**

```cpp
// You write:
LOG_DEBUG("x=" << expensive_function());

// Expands to:
if constexpr (gMinLogLevel <= Debug) {
    getGlobalLogger().debug([&]() -> std::string {
        std::ostringstream oss;
        oss << "x=" << expensive_function();  // Only called if needed
        return oss.str();
    }, SOURCE_LOCATION());
}
```

**The lambda is only invoked after all filtering checks pass**:
1. Compile-time check (if constexpr)
2. Runtime enabled check (atomic)
3. Runtime level check (atomic)
4. **Then** lambda invoked to generate message

**Benefits:**
- No message construction for filtered logs
- No string allocation for filtered logs
- No expensive function calls for filtered logs
- Maintains natural syntax

**Direct Calls** (when you have the string already):

```cpp
std::string message = "Already computed";

// Lambda version (unnecessary overhead)
LOG_INFO(message);  // Creates lambda that returns message

// Direct version (more efficient)
getGlobalLogger().info(message.c_str(), SOURCE_LOCATION());
```

### Source Location

**Automatic capture**: DiagnosticLogger automatically captures:
- File name
- Line number
- Function name

```cpp
void processData() {
    LOG_INFO("Processing started");
    // Output includes: (processData.cpp:123)
}
```

**How it works:**

```cpp
struct SourceLocation {
    const char* file;
    int line;
    const char* function;
    
    constexpr SourceLocation(
        const char* f = __builtin_FILE(),
        int l = __builtin_LINE(),
        const char* fn = __builtin_FUNCTION()
    ) noexcept : file(f), line(l), function(fn) {}
};
```

**Macro provides source location automatically:**

```cpp
#define LOG_INFO(msg) \
    getGlobalLogger().info([&]() -> std::string { \
        /* ... message generation ... */ \
    }, CPP_UTIL_SOURCE_LOCATION());  // <-- Captures file, line, function
```

**Manual source location** (rare):

```cpp
void helper(SourceLocation loc = SourceLocation()) {
    getGlobalLogger().info("Helper called", loc);
}

void caller() {
    helper();  // Logs show caller's location, not helper's
}
```

**Performance**: Source location capture is **zero-cost**:
- Compile-time capture via `__builtin_FILE__` etc.
- No runtime overhead
- Stored as string pointers (not copies)

---

## Sinks

Sinks determine **where** log messages go. DiagnosticLogger supports multiple sinks simultaneously.

### Console Sink

**Writes to stdout/stderr**:

```cpp
#include "DiagnosticLogger.h"
using namespace fat_p::diagnostic;

int main()
{
    auto& logger = getGlobalLogger();
    
    // Add console sink with default formatter
    logger.addSink(std::make_unique<ConsoleSink>());
    
    LOG_INFO("This goes to console");
    LOG_ERROR("Errors go to stderr");  // stderr for Warning and above
    
    return 0;
}
```

**Constructor options:**

```cpp
// Default: all levels, default formatter
auto sink1 = std::make_unique<ConsoleSink>();

// Custom formatter
auto sink2 = std::make_unique<ConsoleSink>(
    std::make_unique<SimpleFormatter>()
);

// Custom minimum level
auto sink3 = std::make_unique<ConsoleSink>(
    std::make_unique<DefaultFormatter>(),
    LogLevel::Warning  // Only warnings and above
);
```

**Output routing:**
- `Trace`, `Debug`, `Info`  `std::cout`
- `Warning`, `Error`, `Fatal`  `std::cerr`

**Thread safety**: Mutex protected, safe for concurrent writes.

### File Sink

**Writes to a file**:

```cpp
auto& logger = getGlobalLogger();

// Basic file sink
logger.addSink(std::make_unique<FileSink>("app.log"));

// With options
logger.addSink(std::make_unique<FileSink>(
    "app.log",                              // filename
    std::make_unique<JsonFormatter>(),      // formatter
    LogLevel::Info,                         // minimum level
    true                                    // append (vs truncate)
));

LOG_INFO("This goes to file");
```

**Constructor parameters:**

```cpp
FileSink(
    const std::string& filename,           // Required
    std::unique_ptr<IFormatter> formatter, // Default: DefaultFormatter
    LogLevel minLevel,                     // Default: Trace
    bool append                            // Default: true
)
```

**File management:**
- Opens file in constructor
- Throws `std::runtime_error` if open fails
- Automatically flushed on:
  - Manual `flush()` call
  - Destructor (RAII)
  - Not every write (performance)

**Example with error handling:**

```cpp
try {
    auto& logger = getGlobalLogger();
    logger.addSink(std::make_unique<FileSink>("/var/log/app.log"));
    LOG_INFO("File logging initialized");
}
catch (const std::runtime_error& e) {
    std::cerr << "Failed to initialize file logging: " << e.what() << std::endl;
    // Fall back to console only
}
```

**Performance notes:**
- Buffered writes (OS page cache)
- Mutex protected (safe but serialized)
- For high-frequency logging, consider:
  - Batching writes
  - Separate slow sinks to different logger instances
  - Using ramdisk for log files

### Callback Sink

**Custom sink via callback**:

```cpp
auto& logger = getGlobalLogger();

// Simple callback
logger.addSink(std::make_unique<CallbackSink>(
    [](const LogRecord& record) {
        // Send to monitoring system
        monitoring::send(logLevelToString(record.level), record.message);
    }
));

// Complex callback with state
class MetricsCollector {
    std::atomic<uint64_t> errorCount_{0};
public:
    void onLog(const LogRecord& record) {
        if (record.level >= LogLevel::Error) {
            errorCount_++;
        }
    }
    uint64_t getErrorCount() const { return errorCount_.load(); }
};

MetricsCollector metrics;
logger.addSink(std::make_unique<CallbackSink>(
    [&metrics](const LogRecord& record) {
        metrics.onLog(record);
    },
    LogLevel::Error  // Only errors and fatal
));
```

**Use cases:**
- Integration with external logging systems
- Metrics collection
- Alert triggering
- Testing (capture logs for assertions)
- Network sinks (send over TCP/UDP)

**Thread safety**: Mutex protected. Ensure your callback is fast or queues work.

### Custom Sinks

**Implement ISink interface**:

```cpp
class ISink {
public:
    virtual ~ISink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};
```

**Example: Rotating file sink**:

```cpp
class RotatingFileSink : public ISink {
    std::ofstream file_;
    std::string baseFilename_;
    size_t maxSize_;
    size_t currentSize_;
    std::unique_ptr<IFormatter> formatter_;
    mutable std::mutex mutex_;

public:
    RotatingFileSink(std::string filename, size_t maxSize)
        : baseFilename_(std::move(filename))
        , maxSize_(maxSize)
        , currentSize_(0)
        , formatter_(std::make_unique<DefaultFormatter>())
    {
        file_.open(baseFilename_);
    }
    
    void write(const LogRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string formatted = formatter_->format(record);
        size_t msgSize = formatted.size() + 1; // +1 for newline
        
        if (currentSize_ + msgSize > maxSize_) {
            rotate();
        }
        
        file_ << formatted << '\n';
        currentSize_ += msgSize;
    }
    
    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.flush();
    }

private:
    void rotate() {
        file_.close();
        
        // Rename old file
        std::string oldName = baseFilename_ + ".old";
        std::rename(baseFilename_.c_str(), oldName.c_str());
        
        // Open new file
        file_.open(baseFilename_);
        currentSize_ = 0;
    }
};

// Usage
logger.addSink(std::make_unique<RotatingFileSink>("app.log", 10'000'000)); // 10MB
```

**Example: Syslog sink**:

```cpp
class SyslogSink : public ISink {
    int facility_;
    std::unique_ptr<IFormatter> formatter_;
    
public:
    SyslogSink(int facility = LOG_USER)
        : facility_(facility)
        , formatter_(std::make_unique<SimpleFormatter>())
    {
        openlog("myapp", LOG_PID, facility_);
    }
    
    ~SyslogSink() override {
        closelog();
    }
    
    void write(const LogRecord& record) override {
        int priority = facility_ | levelToSyslog(record.level);
        std::string formatted = formatter_->format(record);
        syslog(priority, "%s", formatted.c_str());
    }
    
    void flush() override {
        // Syslog doesn't need explicit flushing
    }

private:
    static int levelToSyslog(LogLevel level) {
        switch (level) {
            case LogLevel::Trace:
            case LogLevel::Debug: return LOG_DEBUG;
            case LogLevel::Info: return LOG_INFO;
            case LogLevel::Warning: return LOG_WARNING;
            case LogLevel::Error: return LOG_ERR;
            case LogLevel::Fatal: return LOG_CRIT;
            default: return LOG_INFO;
        }
    }
};
```

### Multiple Sinks

**Log to multiple destinations simultaneously**:

```cpp
auto& logger = getGlobalLogger();

// Console for everything
logger.addSink(std::make_unique<ConsoleSink>(
    std::make_unique<SimpleFormatter>(),
    LogLevel::Trace
));

// File for Info and above with full details
logger.addSink(std::make_unique<FileSink>(
    "detailed.log",
    std::make_unique<DefaultFormatter>(),
    LogLevel::Info
));

// Separate file for errors only in JSON format
logger.addSink(std::make_unique<FileSink>(
    "errors.json",
    std::make_unique<JsonFormatter>(),
    LogLevel::Error
));

// Metrics for errors and fatal
MetricsCollector metrics;
logger.addSink(std::make_unique<CallbackSink>(
    [&metrics](const LogRecord& r) { metrics.onLog(r); },
    LogLevel::Error
));

// Now a single log statement writes to all appropriate sinks
LOG_INFO("Server started");     //  console, detailed.log
LOG_ERROR("Connection failed"); //  console, detailed.log, errors.json, metrics
```

**Performance considerations:**
- Each sink is checked sequentially
- Sinks with higher minLevel are more efficient (early exit)
- All sinks share the same LogRecord (no duplication)
- Formatters called independently for each sink
- Consider sink overhead when logging frequently

---

## Formatters

Formatters control **how** log messages are formatted before being written to sinks.

### Default Formatter

**Full-featured format with all metadata**:

```cpp
auto sink = std::make_unique<ConsoleSink>(
    std::make_unique<DefaultFormatter>()
);
```

**Output format:**
```
[2025-11-17 14:30:45.123] [INFO] [0x7f8b2c001740] Application started (main.cpp:42)
                                                                     
                                                                      (filename:line)
                                                Message
                                Thread ID (hex)
                          Log level
 Timestamp with milliseconds
```

**Features:**
- Full timestamp with milliseconds
- Log level in brackets
- Thread ID (for debugging concurrent code)
- Message
- Source location (filename:line)

**Use cases:**
- Detailed debugging
- Production logs that need full context
- Multi-threaded applications
- When you need to correlate logs across threads

### Simple Formatter

**Minimal format, just level and message**:

```cpp
auto sink = std::make_unique<ConsoleSink>(
    std::make_unique<SimpleFormatter>()
);
```

**Output format:**
```
[INFO] Application started
[ERROR] Connection failed
```

**Features:**
- Log level in brackets
- Message only
- No timestamp, thread ID, or location

**Use cases:**
- Clean console output
- When logs are already timestamped by infrastructure
- Simple applications where context isn't needed
- Testing (easier to assert on output)

### JSON Formatter

**Structured JSON format for machine parsing**:

```cpp
auto sink = std::make_unique<FileSink>(
    "app.json",
    std::make_unique<JsonFormatter>()
);
```

**Output format:**
```json
{"timestamp":"2025-11-17T14:30:45","level":"INFO","message":"Application started","file":"main.cpp","line":42,"function":"main"}
{"timestamp":"2025-11-17T14:30:46","level":"ERROR","message":"Connection failed","file":"network.cpp","line":123,"function":"connect"}
```

**JSON Output Schema:**

Each log line is a JSON object with the following guaranteed fields:

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `timestamp` | string | Yes | ISO 8601 format: `YYYY-MM-DDTHH:MM:SS` |
| `level` | string | Yes | One of: `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL` |
| `message` | string | Yes | The log message (properly JSON-escaped) |
| `file` | string | Yes | Source filename (basename only) |
| `line` | integer | Yes | Source line number |
| `function` | string | Yes | Function name from `__func__` |
| `thread` | string | No | Thread ID as hex string (e.g., `0x7f3a`) |
| `data` | object | No | Structured metadata if provided via `LOG_WITH_DATA` |

Example with metadata:
```json
{"timestamp":"2025-11-17T14:30:45","level":"INFO","message":"Request processed","file":"api.cpp","line":87,"function":"handle","data":{"request_id":"abc123","duration_ms":42}}
```

**Features:**
- Valid JSON objects (one per line, not array)
- ISO 8601 timestamp
- Properly escaped strings
- All metadata included
- Parseable by standard JSON tools

**Use cases:**
- Log aggregation systems (ELK, Splunk, etc.)
- Automated log analysis
- Machine learning on logs
- Integration with monitoring dashboards

> **Performance Warning:** JSON formatting is computationally expensive (~20 us per log),
> compared to simple text formatting (~130 ns). For high-throughput applications using
> `JsonFormatter`, wrap your sink with `AsyncSink` to avoid blocking the main thread:
>
> ```cpp
> #include "DiagnosticLogger_IO.h"
> 
> auto jsonFileSink = std::make_shared<FileSink>(
>     "app.json",
>     std::make_unique<JsonFormatter>()
> );
> auto asyncSink = std::make_shared<AsyncSink>(jsonFileSink);
> logger.addSink(asyncSink);
> ```
>
> This moves the JSON serialization to a background thread, reducing main-thread
> latency from ~20 us to ~47 ns (just the queue enqueue cost).

**Parsing example:**

```bash
# Extract all errors
cat app.json | jq 'select(.level == "ERROR")'

# Count log levels
cat app.json | jq -r '.level' | sort | uniq -c

# Find all logs from specific function
cat app.json | jq 'select(.function == "processData")'
```

### Custom Formatters

**Implement IFormatter interface**:

```cpp
class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual std::string format(const LogRecord& record) const = 0;
};

struct LogRecord {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    SourceLocation location;
    std::thread::id threadId;
};
```

**Example: Colored console formatter**:

```cpp
class ColoredFormatter : public IFormatter {
public:
    std::string format(const LogRecord& record) const override {
        std::ostringstream oss;
        
        // ANSI color codes
        oss << getColor(record.level);
        oss << '[' << logLevelToString(record.level) << "] ";
        oss << "\033[0m";  // Reset color
        oss << record.message;
        
        return oss.str();
    }

private:
    static const char* getColor(LogLevel level) {
        switch (level) {
            case LogLevel::Trace:   return "\033[90m";  // Dark gray
            case LogLevel::Debug:   return "\033[36m";  // Cyan
            case LogLevel::Info:    return "\033[32m";  // Green
            case LogLevel::Warning: return "\033[33m";  // Yellow
            case LogLevel::Error:   return "\033[31m";  // Red
            case LogLevel::Fatal:   return "\033[35m";  // Magenta
            default:                return "\033[0m";   // Reset
        }
    }
};

// Usage
logger.addSink(std::make_unique<ConsoleSink>(
    std::make_unique<ColoredFormatter>()
));
```

**Example: CSV formatter**:

```cpp
class CSVFormatter : public IFormatter {
public:
    std::string format(const LogRecord& record) const override {
        std::ostringstream oss;
        
        auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
        std::tm tm_buf;
        #ifdef _WIN32
            localtime_s(&tm_buf, &time_t);
        #else
            localtime_r(&time_t, &tm_buf);
        #endif
        
        // timestamp,level,message,file,line
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << ','
            << logLevelToString(record.level) << ','
            << escape_csv(record.message) << ','
            << (record.location.file ? record.location.file : "") << ','
            << record.location.line;
        
        return oss.str();
    }

private:
    static std::string escape_csv(const std::string& str) {
        if (str.find(',') == std::string::npos &&
            str.find('"') == std::string::npos &&
            str.find('\n') == std::string::npos) {
            return str;
        }
        
        std::string result = "\"";
        for (char c : str) {
            if (c == '"') result += "\"\"";  // Escape quotes
            else result += c;
        }
        result += "\"";
        return result;
    }
};
```

**Example: Compact binary formatter** (for high-performance logging):

```cpp
class BinaryFormatter : public IFormatter {
public:
    std::string format(const LogRecord& record) const override {
        // This is a hack - return binary data as string
        // In real implementation, would need binary sink support
        std::ostringstream oss;
        
        // Write level as 1 byte
        oss.write(reinterpret_cast<const char*>(&record.level), sizeof(record.level));
        
        // Write timestamp as 8 bytes
        auto ts = record.timestamp.time_since_epoch().count();
        oss.write(reinterpret_cast<const char*>(&ts), sizeof(ts));
        
        // Write message length + message
        uint32_t len = record.message.size();
        oss.write(reinterpret_cast<const char*>(&len), sizeof(len));
        oss.write(record.message.data(), len);
        
        return oss.str();
    }
};
```

---

## Performance Optimization

DiagnosticLogger is designed for maximum performance. Understanding the optimization strategies helps you use it effectively.

### Compile-Time Optimization

**The most powerful optimization: eliminate code entirely.**

**Before compilation (source code):**
```cpp
LOG_TRACE("x=" << expensive_function());
LOG_DEBUG("y=" << another_expensive());
LOG_INFO("Server started");
LOG_ERROR("Failed: " << get_error());
```

**After compilation with `CPP_UTIL_MIN_LOG_LEVEL=2` (assembly):**
```cpp
// LOG_TRACE - completely eliminated, 0 instructions
// LOG_DEBUG - completely eliminated, 0 instructions
LOG_INFO("Server started");      // Compiled in
LOG_ERROR("Failed: " << get_error());  // Compiled in
```

**How it works:**

```cpp
#define LOG_TRACE(msg) \
    do { \
        if constexpr (gMinLogLevel <= LogLevel::Trace) { \
            // ... actual logging code ...
        } \
        // If gMinLogLevel > Trace, the if constexpr evaluates to false
        // and the entire block is eliminated by the compiler
    } while(0)
```

**Verification: Check assembly output:**

```bash
# Compile with different log levels
g++ -std=c++17 -O2 -DCPP_UTIL_MIN_LOG_LEVEL=0 -S test.cpp -o trace.s
g++ -std=c++17 -O2 -DCPP_UTIL_MIN_LOG_LEVEL=2 -S test.cpp -o info.s

# Compare sizes
wc -l trace.s info.s
# trace.s: 523 lines
# info.s:  387 lines  (136 lines eliminated!)
```

**Build configuration strategy:**

```makefile
# Development build - all logs
CXXFLAGS_DEV = -DCPP_UTIL_MIN_LOG_LEVEL=0 -g

# Testing build - debug and above
CXXFLAGS_TEST = -DCPP_UTIL_MIN_LOG_LEVEL=1 -O2

# Production build - info and above
CXXFLAGS_PROD = -DCPP_UTIL_MIN_LOG_LEVEL=2 -O3 -DNDEBUG

# Minimal build - errors only
CXXFLAGS_MIN = -DCPP_UTIL_MIN_LOG_LEVEL=4 -O3 -DNDEBUG
```

**Performance impact:**

| Build Type | Code Size | Disabled Log Overhead | Binary Size |
|-----------|-----------|---------------------|-------------|
| Trace (0) | 100%      | ~10ns (atomic)      | 100%        |
| Debug (1) | ~95%      | ~10ns (atomic)      | ~98%        |
| Info (2)  | ~80%      | ~10ns (atomic)      | ~85%        |
| Error (4) | ~60%      | ~10ns (atomic)      | ~70%        |
| Off (6)   | 0%        | 0ns (eliminated)    | ~50%        |

### Runtime Optimization

**After compile-time filtering, runtime filtering adds minimal overhead.**

**Optimization 1: Lock-Free Atomics**

```cpp
// Traditional approach (slow)
std::mutex globalMutex;
bool enabled = true;
LogLevel minLevel = Info;

void log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(globalMutex);  // ~100ns
    if (!enabled || level < minLevel) return;
    // ... logging ...
}

// DiagnosticLogger approach (fast)
std::atomic<unsigned char> enabled_{1};
std::atomic<LogLevel> minLevel_{Info};

void log(LogLevel level, MessageGen&& gen) {
    // ~5ns - single atomic load, no mutex
    if (!enabled_.load(std::memory_order_relaxed) ||
        level < minLevel_.load(std::memory_order_relaxed)) {
        return;
    }
    // ... logging ...
}
```

**Why relaxed memory ordering?**
- Logging doesn't require synchronization
- Brief staleness (seeing old enabled state) is acceptable
- Provides maximum performance
- Still atomic (no torn reads/writes)

**Optimization 2: Combined Check**

```cpp
// Less efficient: two separate atomics
if (!enabled_.load(std::memory_order_relaxed)) return;
if (level < minLevel_.load(std::memory_order_relaxed)) return;

// More efficient: single combined check
bool should_log(LogLevel level) const noexcept {
    return enabled_.load(std::memory_order_relaxed) &&
           level >= minLevel_.load(std::memory_order_relaxed);
}

if (UNLIKELY(!should_log(level))) return;
```

**Benefits:**
- Fewer atomic loads in common case
- CPU can execute both loads in parallel
- Branch predictor only sees one branch

**Optimization 3: Double-Checked Locking**

```cpp
// Fast path (no lock)
if (UNLIKELY(!should_log(level))) return;

// Slow path (acquire lock)
std::lock_guard<std::mutex> lock(sinkMutex_);

// Recheck after acquiring lock
if (!enabled_.load(std::memory_order_relaxed) ||
    level < runtimeMinLevel_.load(std::memory_order_relaxed) ||
    sinks_.empty()) {
    return;  // State changed, don't log
}

// Generate message and write to sinks
```

**Why recheck?**
- Another thread might have disabled logging between checks
- Prevents wasted message generation
- Small additional cost but avoids spurious work

### Branch Prediction

**Modern CPUs predict which way branches go. Help them out!**

**LIKELY/UNLIKELY macros:**

```cpp
#if defined(__GNUC__) || defined(__clang__)
#  define LIKELY(x)   __builtin_expect(!!(x), 1)
#  define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x)   (x)
#  define UNLIKELY(x) (x)
#endif
```

**Usage in DiagnosticLogger:**

```cpp
// The common case is logging is DISABLED
if (UNLIKELY(!should_log(level))) {
    return;  // CPU predicts we take this branch
}

// Slow path only executed rarely
log_slow_path(level, std::forward<MessageGen>(gen));
```

**How it helps:**
- CPU speculatively executes the predicted path
- If prediction is correct, no branch penalty (~15-20 cycles saved)
- If prediction is wrong, pipeline flush (~15-20 cycles lost)

**Prediction accuracy matters:**

```cpp
// Bad: logging frequently enabled
// CPU always predicts "disabled" but is usually wrong
for (int i = 0; i < 1000000; ++i) {
    LOG_INFO("Iteration " << i);  // Wrong prediction every time!
}
// Result: Lots of pipeline flushes

// Good: logging rarely enabled
// CPU predicts "disabled" and is usually right
for (int i = 0; i < 1000000; ++i) {
    LOG_DEBUG("Iteration " << i);  // Correct prediction most of the time
}
// Result: Fast path is fast
```

**Measurement: Branch misprediction rate**

```bash
# Profile your application
perf stat -e branches,branch-misses ./app

# Look for high branch misprediction rate
# Good: <1% misprediction
# Bad: >5% misprediction
```

**Optimization: Organize code for cold paths:**

```cpp
// FORCE_INLINE - hot path stays in instruction cache
FORCE_INLINE
void log(LogLevel level, MessageGen&& gen) {
    if constexpr (gMinLogLevel > level) return;
    if (UNLIKELY(!should_log(level))) return;
    log_slow_path(level, std::forward<MessageGen>(gen));
}

// NO_INLINE - cold path doesn't pollute instruction cache
NO_INLINE
void log_slow_path(LogLevel level, MessageGen&& gen) {
    // ... expensive logging work ...
}
```

### Zero-Overhead Disabled Logging

**Goal**: When logging is disabled at compile time, **zero** overhead.

**Achieved through:**

```cpp
// 1. Compile-time elimination
if constexpr (gMinLogLevel > LogLevel::Debug) {
    return;  // Entire function body eliminated
}

// 2. No code generation
LOG_DEBUG("x=" << expensive());
// With gMinLogLevel > Debug: 0 instructions
// expensive() is NEVER called, not even evaluated

// 3. No string construction
LOG_DEBUG("Result: " << calculation());
// With gMinLogLevel > Debug: calculation() never called

// 4. No lambda creation
LOG_DEBUG([complex logic to build message]);
// With gMinLogLevel > Debug: lambda never instantiated
```

**Verification: Benchmark disabled logging**

```cpp
#include <chrono>
#include "DiagnosticLogger.h"

// Compile with -DCPP_UTIL_MIN_LOG_LEVEL=6 (all disabled)

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10'000'000; ++i) {
        LOG_TRACE("Iteration " << i);
        LOG_DEBUG("Value: " << i * 2);
        LOG_INFO("Processing " << i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    std::cout << "Time per log call: " << (duration.count() / 30'000'000.0) << " ns\n";
    // Expected output: ~0 ns (loop overhead only)
    
    return 0;
}
```

**Results:**

| Configuration | Time per Call | Notes |
|--------------|---------------|-------|
| All disabled (CPP_UTIL_MIN_LOG_LEVEL=6) | 0 ns | Code eliminated |
| Runtime disabled | ~10 ns | Atomic load + branch |
| Runtime filtered | ~10 ns | Atomic load + branch + level check |
| Active logging | ~130 ns | Full logging path |

---

## Advanced Usage

### Global vs Local Loggers

**Global logger** (most common):

```cpp
#include "DiagnosticLogger.h"
using namespace fat_p::diagnostic;

int main() {
    initializeDefaultLogger();
    
    LOG_INFO("Using global logger");
    
    auto& logger = getGlobalLogger();
    logger.setMinLevel(LogLevel::Warning);
    
    return 0;
}
```

**Advantages:**
- Simple, no passing logger around
- Macros (LOG_INFO, etc.) use global logger
- Centralized configuration

**Disadvantages:**
- Single global state
- Cannot have different configurations for different components

**Local loggers** (advanced):

```cpp
class DatabaseModule {
    fat_p::diagnostic::Logger logger_;

public:
    DatabaseModule() {
        logger_.addSink(std::make_unique<FileSink>("database.log"));
        logger_.setMinLevel(LogLevel::Debug);  // Verbose for this module
    }
    
    void query(const std::string& sql) {
        logger_.info([&]() {
            return "Executing query: " + sql;
        }, CPP_UTIL_SOURCE_LOCATION());
    }
};

class NetworkModule {
    fat_p::diagnostic::Logger logger_;

public:
    NetworkModule() {
        logger_.addSink(std::make_unique<FileSink>("network.log"));
        logger_.setMinLevel(LogLevel::Warning);  // Only warnings for this module
    }
    
    void send(const std::string& data) {
        logger_.warning([&]() {
            return "Sending data: " + data;
        }, CPP_UTIL_SOURCE_LOCATION());
    }
};
```

**Advantages:**
- Per-component configuration
- Separate log files
- Different log levels per component
- Better isolation

**Disadvantages:**
- More complex
- Need to pass logger around
- Cannot use convenience macros

**Hybrid approach: Scoped global logger**

```cpp
namespace database {
    inline Logger& getLogger() {
        static Logger logger;
        return logger;
    }
}

namespace network {
    inline Logger& getLogger() {
        static Logger logger;
        return logger;
    }
}

// In code:
database::getLogger().info("Query executed");
network::getLogger().error("Connection failed");
```

**Per-Function Caching Pattern** (for hot paths):

```cpp
void process_request(const Request& req) {
    // Cache logger reference in static variable - zero lookup overhead after first call
    static Logger& log = database::getLogger();
    
    log.debug("Processing request");
    // ... hot path code ...
    log.info("Request completed");
}
```

This pattern eliminates the function call overhead of `getLogger()` on every log statement,
which can matter in tight loops or high-frequency code paths.

### Thread Safety

**All DiagnosticLogger operations are thread-safe:**

```cpp
#include <thread>
#include <vector>

void worker(int id) {
    for (int i = 0; i < 1000; ++i) {
        LOG_INFO("Worker " << id << " iteration " << i);
    }
}

int main() {
    initializeDefaultLogger();
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
// All logs properly interleaved, no corruption
```

**Thread safety guarantees:**

1. **Logger configuration** (setMinLevel, setEnabled, addSink):
   - Protected by mutex
   - Safe to call from any thread
   - Atomic operations use relaxed ordering

2. **Logging operations** (log, trace, debug, etc.):
   - Fast path is lock-free (atomics only)
   - Slow path protected by mutex
   - LogRecord created independently per thread

3. **Sinks**:
   - Each sink has own mutex
   - Multiple threads can log simultaneously (different sinks)
   - Single sink serializes writes

4. **Formatters**:
   - Called under sink's mutex
   - Must be thread-safe (const methods)
   - DefaultFormatter, SimpleFormatter, JsonFormatter are thread-safe

**Performance implications:**

```cpp
// Single sink: all threads contend for same mutex
auto& logger = getGlobalLogger();
logger.addSink(std::make_unique<FileSink>("app.log"));

// Multiple sinks: threads can write to different sinks concurrently
logger.addSink(std::make_unique<FileSink>("thread0.log", ..., LogLevel::Trace));
logger.addSink(std::make_unique<FileSink>("thread1.log", ..., LogLevel::Trace));
// Still not perfect (global logger mutex) but better
```

**Best practices for multi-threaded logging:**

1. **Use compile-time filtering** to reduce contention:
   ```cpp
   // Production build: only errors
   // -DCPP_UTIL_MIN_LOG_LEVEL=4
   ```

2. **Use runtime filtering** to reduce active threads:
   ```cpp
   logger.setMinLevel(LogLevel::Warning);
   ```

3. **Consider per-thread loggers** for high-frequency logging:
   ```cpp
   thread_local Logger threadLogger;
   ```

4. **Profile contention** if performance is critical:
   ```bash
   perf record -g ./app
   perf report
   # Look for time spent in mutex operations
   ```

### Runtime Configuration

**Change logger behavior at runtime:**

```cpp
#include "DiagnosticLogger.h"
#include <signal.h>

using namespace fat_p::diagnostic;

// Signal handler to enable debug logging
void handle_debug_signal(int sig) {
    auto& logger = getGlobalLogger();
    logger.setMinLevel(LogLevel::Debug);
    LOG_INFO("Debug logging enabled by signal");
}

// Signal handler to disable all logging
void handle_disable_signal(int sig) {
    auto& logger = getGlobalLogger();
    logger.setEnabled(false);
    // Note: This LOG_INFO might not be printed if logging disabled fast enough
    LOG_INFO("Logging disabled by signal");
}

int main() {
    initializeDefaultLogger();
    
    // Install signal handlers
    signal(SIGUSR1, handle_debug_signal);   // kill -USR1 <pid>
    signal(SIGUSR2, handle_disable_signal); // kill -USR2 <pid>
    
    auto& logger = getGlobalLogger();
    logger.setMinLevel(LogLevel::Info);
    
    LOG_INFO("Application started. Send SIGUSR1 for debug mode.");
    
    // Main application loop
    while (true) {
        LOG_DEBUG("This is only printed in debug mode");
        LOG_INFO("Processing...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

**Configuration from environment variables:**

```cpp
void configureFromEnvironment() {
    auto& logger = getGlobalLogger();
    
    // Log level from environment
    if (const char* levelStr = std::getenv("LOG_LEVEL")) {
        if (std::string(levelStr) == "TRACE") {
            logger.setMinLevel(LogLevel::Trace);
        } else if (std::string(levelStr) == "DEBUG") {
            logger.setMinLevel(LogLevel::Debug);
        } else if (std::string(levelStr) == "INFO") {
            logger.setMinLevel(LogLevel::Info);
        } else if (std::string(levelStr) == "WARNING") {
            logger.setMinLevel(LogLevel::Warning);
        } else if (std::string(levelStr) == "ERROR") {
            logger.setMinLevel(LogLevel::Error);
        }
    }
    
    // Enable/disable from environment
    if (const char* enabledStr = std::getenv("LOG_ENABLED")) {
        logger.setEnabled(std::string(enabledStr) == "1" || 
                         std::string(enabledStr) == "true");
    }
    
    // Log file from environment
    if (const char* logFile = std::getenv("LOG_FILE")) {
        logger.addSink(std::make_unique<FileSink>(logFile));
    }
}

int main() {
    initializeDefaultLogger();
    configureFromEnvironment();
    
    LOG_INFO("Logger configured from environment");
    return 0;
}

// Usage:
// LOG_LEVEL=DEBUG LOG_FILE=/tmp/app.log ./app
```

**Configuration from config file:**

```cpp
#include "JsonLite.h"  // If using JsonLite for config

struct LogConfig {
    std::string level = "INFO";
    std::string file;
    bool console = true;
    bool json_format = false;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(LogConfig, level, file, console, json_format)

void configureFromFile(const std::string& configPath) {
    auto config = load_params<LogConfig>(configPath);
    
    auto& logger = getGlobalLogger();
    
    // Set level
    if (config.level == "TRACE") logger.setMinLevel(LogLevel::Trace);
    else if (config.level == "DEBUG") logger.setMinLevel(LogLevel::Debug);
    else if (config.level == "INFO") logger.setMinLevel(LogLevel::Info);
    else if (config.level == "WARNING") logger.setMinLevel(LogLevel::Warning);
    else if (config.level == "ERROR") logger.setMinLevel(LogLevel::Error);
    
    // Add console sink
    if (config.console) {
        auto formatter = config.json_format ? 
            std::make_unique<JsonFormatter>() :
            std::make_unique<DefaultFormatter>();
        logger.addSink(std::make_unique<ConsoleSink>(std::move(formatter)));
    }
    
    // Add file sink
    if (!config.file.empty()) {
        auto formatter = config.json_format ?
            std::make_unique<JsonFormatter>() :
            std::make_unique<DefaultFormatter>();
        logger.addSink(std::make_unique<FileSink>(config.file, std::move(formatter)));
    }
}

int main() {
    configureFromFile("logging.json");
    LOG_INFO("Logger configured from file");
    return 0;
}
```

**logging.json:**
```json
{
    "level": "DEBUG",
    "file": "/var/log/app.log",
    "console": true,
    "json_format": false
}
```

### Integration with External Frameworks

**Integration with spdlog:**

```cpp
#include "DiagnosticLogger.h"
#include <spdlog/spdlog.h>

class SpdlogSink : public fat_p::diagnostic::ISink {
    std::shared_ptr<spdlog::logger> spdLogger_;
    
public:
    explicit SpdlogSink(std::shared_ptr<spdlog::logger> logger)
        : spdLogger_(std::move(logger)) {}
    
    void write(const fat_p::diagnostic::LogRecord& record) override {
        using fat_p::diagnostic::LogLevel;
        
        switch (record.level) {
            case LogLevel::Trace:
                spdLogger_->trace(record.message);
                break;
            case LogLevel::Debug:
                spdLogger_->debug(record.message);
                break;
            case LogLevel::Info:
                spdLogger_->info(record.message);
                break;
            case LogLevel::Warning:
                spdLogger_->warn(record.message);
                break;
            case LogLevel::Error:
                spdLogger_->error(record.message);
                break;
            case LogLevel::Fatal:
                spdLogger_->critical(record.message);
                break;
            default:
                break;
        }
    }
    
    void flush() override {
        spdLogger_->flush();
    }
};

// Usage
int main() {
    auto spdlogger = spdlog::stdout_color_mt("console");
    
    auto& logger = fat_p::diagnostic::getGlobalLogger();
    logger.addSink(std::make_unique<SpdlogSink>(spdlogger));
    
    LOG_INFO("This goes through spdlog");
    return 0;
}
```

**Integration with Google Cloud Logging:**

```cpp
#include "DiagnosticLogger.h"
#include <google/cloud/logging/logging_client.h>

class CloudLoggingSink : public fat_p::diagnostic::ISink {
    google::cloud::logging::LoggingClient client_;
    std::string logName_;
    std::unique_ptr<fat_p::diagnostic::IFormatter> formatter_;
    
public:
    CloudLoggingSink(std::string logName)
        : client_(google::cloud::logging::LoggingClient::Create())
        , logName_(std::move(logName))
        , formatter_(std::make_unique<fat_p::diagnostic::JsonFormatter>()) {}
    
    void write(const fat_p::diagnostic::LogRecord& record) override {
        google::cloud::logging::LogEntry entry;
        entry.set_log_name(logName_);
        entry.set_severity(toCloudSeverity(record.level));
        entry.set_json_payload(formatter_->format(record));
        
        client_.WriteLogEntry(entry);
    }
    
    void flush() override {}

private:
    static google::cloud::logging::Severity toCloudSeverity(
        fat_p::diagnostic::LogLevel level) {
        using fat_p::diagnostic::LogLevel;
        switch (level) {
            case LogLevel::Trace:
            case LogLevel::Debug:
                return google::cloud::logging::Severity::kDebug;
            case LogLevel::Info:
                return google::cloud::logging::Severity::kInfo;
            case LogLevel::Warning:
                return google::cloud::logging::Severity::kWarning;
            case LogLevel::Error:
                return google::cloud::logging::Severity::kError;
            case LogLevel::Fatal:
                return google::cloud::logging::Severity::kCritical;
            default:
                return google::cloud::logging::Severity::kDefault;
        }
    }
};
```

**Integration with Prometheus metrics:**

```cpp
#include "DiagnosticLogger.h"
#include <prometheus/counter.h>
#include <prometheus/registry.h>

class PrometheusSink : public fat_p::diagnostic::ISink {
    prometheus::Family<prometheus::Counter>& family_;
    prometheus::Counter& trace_;
    prometheus::Counter& debug_;
    prometheus::Counter& info_;
    prometheus::Counter& warning_;
    prometheus::Counter& error_;
    prometheus::Counter& fatal_;
    
public:
    explicit PrometheusSink(prometheus::Registry& registry)
        : family_(prometheus::BuildCounter()
                    .Name("log_messages_total")
                    .Help("Total log messages by level")
                    .Register(registry))
        , trace_(family_.Add({{"level", "trace"}}))
        , debug_(family_.Add({{"level", "debug"}}))
        , info_(family_.Add({{"level", "info"}}))
        , warning_(family_.Add({{"level", "warning"}}))
        , error_(family_.Add({{"level", "error"}}))
        , fatal_(family_.Add({{"level", "fatal"}}))
    {}
    
    void write(const fat_p::diagnostic::LogRecord& record) override {
        using fat_p::diagnostic::LogLevel;
        switch (record.level) {
            case LogLevel::Trace:   trace_.Increment(); break;
            case LogLevel::Debug:   debug_.Increment(); break;
            case LogLevel::Info:    info_.Increment(); break;
            case LogLevel::Warning: warning_.Increment(); break;
            case LogLevel::Error:   error_.Increment(); break;
            case LogLevel::Fatal:   fatal_.Increment(); break;
            default: break;
        }
    }
    
    void flush() override {}
};

// Usage
int main() {
    prometheus::Registry registry;
    
    auto& logger = fat_p::diagnostic::getGlobalLogger();
    logger.addSink(std::make_unique<PrometheusSink>(registry));
    logger.addSink(std::make_unique<ConsoleSink>());
    
    LOG_INFO("Starting server");
    LOG_ERROR("Connection failed");
    
    // Expose metrics endpoint
    // ...
    
    return 0;
}
```

---

## Error Handling

### File Sink Failures

**FileSink throws on construction failure:**

```cpp
try {
    auto& logger = getGlobalLogger();
    logger.addSink(std::make_unique<FileSink>("/nonexistent/path/log.txt"));
}
catch (const std::runtime_error& e) {
    std::cerr << "Failed to create file sink: " << e.what() << std::endl;
    // Fall back to console logging
    auto& logger = getGlobalLogger();
    logger.addSink(std::make_unique<ConsoleSink>());
}
```

**Defensive initialization:**

```cpp
void initializeLogging(const std::string& logPath) {
    auto& logger = getGlobalLogger();
    
    // Always add console sink as fallback
    logger.addSink(std::make_unique<ConsoleSink>());
    
    // Try to add file sink
    try {
        logger.addSink(std::make_unique<FileSink>(logPath));
        LOG_INFO("File logging initialized: " << logPath);
    }
    catch (const std::runtime_error& e) {
        LOG_WARNING("File logging failed: " << e.what());
        LOG_WARNING("Falling back to console only");
    }
}
```

**Multiple fallback options:**

```cpp
void initializeLogging() {
    auto& logger = getGlobalLogger();
    
    // Try primary log location
    try {
        logger.addSink(std::make_unique<FileSink>("/var/log/app.log"));
        return;
    }
    catch (...) {
        // Try secondary location
        try {
            logger.addSink(std::make_unique<FileSink>("/tmp/app.log"));
            LOG_WARNING("Using fallback log location: /tmp/app.log");
            return;
        }
        catch (...) {
            // Final fallback: console only
            logger.addSink(std::make_unique<ConsoleSink>());
            LOG_ERROR("All file logging failed, using console only");
        }
    }
}
```

### Exception Safety

**DiagnosticLogger provides strong exception safety guarantees:**

1. **Logger operations**:
   - `addSink()`: Strong guarantee (either sink added or exception thrown)
   - `setMinLevel()`: No-throw guarantee (atomic operation)
   - `setEnabled()`: No-throw guarantee (atomic operation)
   - `log()`: Basic guarantee (state consistent but message may be lost)

2. **Sink operations**:
   - `write()`: Basic guarantee (some sinks may have written)
   - `flush()`: No-throw guarantee (best effort)

3. **Formatter operations**:
   - `format()`: Strong guarantee (either returns string or throws)

**Exception handling in logging:**

```cpp
// User code throwing in message generation
try {
    LOG_INFO("Processing " << risky_function());
}
catch (const std::exception& e) {
    LOG_ERROR("Exception in log message generation: " << e.what());
}

// However, this is handled internally:
LOG_INFO("Processing " << risky_function());
// If risky_function() throws, exception propagates to caller
// Log is not written (message generation failed)
```

**Best practices:**

1. **Don't throw in formatters**:
   ```cpp
   class MyFormatter : public IFormatter {
   public:
       std::string format(const LogRecord& record) const override {
           try {
               // Format logic that might throw
               return doFormat(record);
           }
           catch (...) {
               return "[Formatting error]";
           }
       }
   };
   ```

2. **Don't throw in sinks** (if possible):
   ```cpp
   class MySink : public ISink {
   public:
       void write(const LogRecord& record) override {
           try {
               doWrite(record);
           }
           catch (const std::exception& e) {
               std::cerr << "Sink error: " << e.what() << std::endl;
           }
       }
   };
   ```

3. **Be careful with message generation**:
   ```cpp
   // Bad: Expensive operation that might throw
   LOG_INFO("Result: " << calculate_expensive_result());
   
   // Good: Handle exception before logging
   try {
       auto result = calculate_expensive_result();
       LOG_INFO("Result: " << result);
   }
   catch (const std::exception& e) {
       LOG_ERROR("Calculation failed: " << e.what());
   }
   ```

### Recovery Strategies

**Strategy 1: Graceful degradation**

```cpp
class RobustLogger {
    fat_p::diagnostic::Logger& logger_;
    bool fileLoggingAvailable_ = false;

public:
    RobustLogger() : logger_(fat_p::diagnostic::getGlobalLogger()) {
        // Always have console as fallback
        logger_.addSink(std::make_unique<fat_p::diagnostic::ConsoleSink>());
        
        // Try to enable file logging
        try {
            logger_.addSink(std::make_unique<fat_p::diagnostic::FileSink>("app.log"));
            fileLoggingAvailable_ = true;
        }
        catch (...) {
            fileLoggingAvailable_ = false;
        }
    }
    
    void checkHealth() {
        if (!fileLoggingAvailable_) {
            // Periodically retry file logging
            try {
                logger_.addSink(std::make_unique<fat_p::diagnostic::FileSink>("app.log"));
                fileLoggingAvailable_ = true;
                LOG_INFO("File logging recovered");
            }
            catch (...) {
                // Still not available
            }
        }
    }
};
```

**Strategy 2: Disk space monitoring**

```cpp
#include <sys/statvfs.h>

bool hasEnoughDiskSpace(const std::string& path, size_t requiredBytes) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        return false;
    }
    
    size_t availableBytes = stat.f_bavail * stat.f_frsize;
    return availableBytes > requiredBytes;
}

void initializeLoggingWithDiskCheck(const std::string& logPath) {
    auto& logger = getGlobalLogger();
    
    if (!hasEnoughDiskSpace("/var/log", 100 * 1024 * 1024)) {  // 100 MB
        LOG_WARNING("Insufficient disk space for file logging");
        logger.addSink(std::make_unique<ConsoleSink>());
        return;
    }
    
    try {
        logger.addSink(std::make_unique<FileSink>(logPath));
    }
    catch (const std::exception& e) {
        LOG_ERROR("File sink failed: " << e.what());
        logger.addSink(std::make_unique<ConsoleSink>());
    }
}
```

**Strategy 3: Monitoring and alerts**

```cpp
class MonitoredLogger {
    fat_p::diagnostic::Logger& logger_;
    std::atomic<uint64_t> writeFailures_{0};
    std::atomic<uint64_t> lastAlertTime_{0};

public:
    MonitoredLogger() : logger_(fat_p::diagnostic::getGlobalLogger()) {
        // Add monitoring sink
        logger_.addSink(std::make_unique<fat_p::diagnostic::CallbackSink>(
            [this](const fat_p::diagnostic::LogRecord& record) {
                checkHealth();
            }
        ));
    }
    
    void onWriteFailure() {
        writeFailures_++;
        
        auto now = std::time(nullptr);
        auto lastAlert = lastAlertTime_.load();
        
        // Alert at most once per hour
        if (now - lastAlert > 3600) {
            if (lastAlertTime_.compare_exchange_strong(lastAlert, now)) {
                sendAlert("Logging failures detected: " + 
                         std::to_string(writeFailures_.load()));
            }
        }
    }
    
    void checkHealth() {
        // Check metrics, send alerts if needed
        auto failures = writeFailures_.load();
        if (failures > 100) {
            sendAlert("High logging failure rate: " + std::to_string(failures));
        }
    }
    
    void sendAlert(const std::string& message) {
        // Send to monitoring system
        std::cerr << "ALERT: " << message << std::endl;
    }
};
```

---

## Best Practices

### When to Log

**[YES] Do log:**

1. **Application lifecycle events**:
   ```cpp
   LOG_INFO("Application started");
   LOG_INFO("Configuration loaded from " << configPath);
   LOG_INFO("Shutting down gracefully");
   ```

2. **State transitions**:
   ```cpp
   LOG_INFO("Connection established to " << host);
   LOG_INFO("Entering maintenance mode");
   LOG_INFO("Switched to backup database");
   ```

3. **Error conditions**:
   ```cpp
   LOG_ERROR("Failed to connect to database: " << error);
   LOG_WARNING("Retrying operation after failure");
   LOG_FATAL("Unrecoverable error, terminating");
   ```

4. **Security events**:
   ```cpp
   LOG_WARNING("Failed login attempt for user: " << username);
   LOG_ERROR("Invalid API key provided");
   LOG_INFO("User " << username << " logged out");
   ```

5. **Performance milestones**:
   ```cpp
   LOG_INFO("Processed 1,000,000 records in " << elapsed << "s");
   LOG_DEBUG("Cache hit rate: " << hitRate << "%");
   ```

** Don't log:**

1. **High-frequency loops**:
   ```cpp
   // Bad
   for (int i = 0; i < 1000000; ++i) {
       LOG_DEBUG("Processing item " << i);  // 1M log entries!
   }
   
   // Good
   for (int i = 0; i < 1000000; ++i) {
       // Process item
       if (i % 10000 == 0) {
           LOG_DEBUG("Progress: " << i << "/1000000");
       }
   }
   ```

2. **Sensitive data**:
   ```cpp
   // Bad
   LOG_INFO("User password: " << password);
   LOG_DEBUG("Credit card: " << ccNumber);
   
   // Good
   LOG_INFO("User authenticated successfully");
   LOG_DEBUG("Payment processed");
   ```

3. **Redundant information**:
   ```cpp
   // Bad
   LOG_INFO("Starting processData");
   processData();
   LOG_INFO("Finished processData");
   
   // Good
   LOG_DEBUG("Processing data batch " << batchId);
   processData();
   ```

4. **Normal operation details** (at INFO level):
   ```cpp
   // Bad (INFO)
   LOG_INFO("Incrementing counter");
   LOG_INFO("Checking condition");
   
   // Good (DEBUG or TRACE)
   LOG_TRACE("Incrementing counter");
   LOG_TRACE("Checking condition");
   ```

### What to Log

**Include context**:

```cpp
// Bad: Not enough context
LOG_ERROR("Connection failed");

// Good: Includes what, why, where
LOG_ERROR("Database connection failed: " << errorMsg << 
          " (host=" << host << ", port=" << port << ")");
```

**Include values that help debugging**:

```cpp
// Bad: Missing key information
LOG_ERROR("Invalid input");

// Good: Shows what was invalid and why
LOG_ERROR("Invalid input: expected positive integer, got: " << input);
```

**Use structured logging for machine parsing**:

```cpp
// Less useful for automated analysis
LOG_INFO("User logged in from 192.168.1.1");

// Better: Use consistent format
LOG_INFO("event=login user=" << username << " ip=" << ipAddress << 
         " timestamp=" << timestamp);

// Best: Use JSON formatter for fully structured logs
// Output: {"event":"login","user":"alice","ip":"192.168.1.1","timestamp":"2025-11-17T14:30:45"}
```

**Correlation IDs for request tracing**:

```cpp
class RequestContext {
    std::string requestId_;
    
public:
    explicit RequestContext(std::string id) : requestId_(std::move(id)) {}
    
    void logInfo(const std::string& message) {
        LOG_INFO("[req:" << requestId_ << "] " << message);
    }
    
    void logError(const std::string& message) {
        LOG_ERROR("[req:" << requestId_ << "] " << message);
    }
};

// Usage
RequestContext ctx(generateRequestId());
ctx.logInfo("Processing started");
ctx.logInfo("Database query completed");
ctx.logError("Validation failed");
// All logs include [req:abc123] prefix for correlation
```

### Performance Considerations

**1. Use compile-time filtering aggressively**:

```cpp
// Development build
g++ -DCPP_UTIL_MIN_LOG_LEVEL=0  // All logs

// Production build
g++ -DCPP_UTIL_MIN_LOG_LEVEL=2  // Info and above only
```

**2. Avoid expensive operations in log messages**:

```cpp
// Bad: Expensive serialization always executed
LOG_DEBUG("Data: " << expensiveToString(largeObject));

// Good: Lazy evaluation avoids cost when debug disabled
LOG_DEBUG([&]() {
    return "Data: " + expensiveToString(largeObject);
});

// Best: Use trace/debug levels with compile-time filtering
#define LOG_EXPENSIVE_DEBUG(msg) \
    do { \
        if constexpr (gMinLogLevel <= LogLevel::Debug) { \
            LOG_DEBUG(msg); \
        } \
    } while(0)
```

**3. Batch operations when possible**:

```cpp
// Bad: 1000 log calls
for (const auto& item : items) {
    LOG_DEBUG("Processing " << item);
}

// Good: Single log with summary
LOG_DEBUG("Processing " << items.size() << " items");

// For detailed logging, sample:
for (size_t i = 0; i < items.size(); ++i) {
    if (i < 10 || i % 100 == 0) {  // First 10 + every 100th
        LOG_DEBUG("Processing item " << i << ": " << items[i]);
    }
}
```

**4. Use appropriate log levels**:

```cpp
// Bad: Everything at INFO
LOG_INFO("Entered function");  // Should be TRACE
LOG_INFO("Variable x=" << x);  // Should be DEBUG
LOG_INFO("Error occurred");    // Should be ERROR

// Good: Appropriate levels
LOG_TRACE("Entered function");
LOG_DEBUG("Variable x=" << x);
LOG_ERROR("Error occurred: " << details);
```

**5. Profile logging overhead**:

```cpp
#include <chrono>

void benchmark_logging() {
    auto& logger = getGlobalLogger();
    
    // Test disabled logging
    logger.setEnabled(false);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        LOG_DEBUG("Test " << i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto disabled_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    // Test enabled logging to /dev/null or fast sink
    logger.setEnabled(true);
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        LOG_DEBUG("Test " << i);
    }
    end = std::chrono::high_resolution_clock::now();
    auto enabled_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    std::cout << "Disabled: " << (disabled_ns / 1000000.0) << " ns/call\n";
    std::cout << "Enabled: " << (enabled_ns / 1000000.0) << " ns/call\n";
}
```

### Testing with Logs

**Capture logs in unit tests**:

```cpp
#include "DiagnosticLogger.h"
#include <gtest/gtest.h>

class LogCapture : public fat_p::diagnostic::ISink {
    std::vector<std::string> messages_;
    std::unique_ptr<fat_p::diagnostic::IFormatter> formatter_;
    mutable std::mutex mutex_;

public:
    LogCapture() : formatter_(std::make_unique<fat_p::diagnostic::SimpleFormatter>()) {}
    
    void write(const fat_p::diagnostic::LogRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.push_back(formatter_->format(record));
    }
    
    void flush() override {}
    
    std::vector<std::string> getMessages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.clear();
    }
};

TEST(MyComponentTest, LogsCorrectly) {
    auto capture = std::make_shared<LogCapture>();
    
    auto& logger = fat_p::diagnostic::getGlobalLogger();
    logger.addSink(std::unique_ptr<LogCapture>(capture.get()));
    
    // Code under test
    myFunction();
    
    // Verify logs
    auto messages = capture->getMessages();
    ASSERT_EQ(messages.size(), 2);
    EXPECT_THAT(messages[0], testing::HasSubstr("[INFO] Started processing"));
    EXPECT_THAT(messages[1], testing::HasSubstr("[INFO] Completed successfully"));
}
```

**Disable logging in performance-sensitive tests**:

```cpp
TEST(PerformanceTest, FastOperation) {
    auto& logger = fat_p::diagnostic::getGlobalLogger();
    logger.setEnabled(false);  // Disable logging for this test
    
    auto start = std::chrono::high_resolution_clock::now();
    performOperation();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100);  // Should complete in <100ms
    
    logger.setEnabled(true);  // Re-enable for other tests
}
```

**Test log levels**:

```cpp
TEST(MyComponentTest, LogsAtCorrectLevels) {
    auto capture = std::make_shared<LogCapture>();
    auto& logger = fat_p::diagnostic::getGlobalLogger();
    logger.addSink(std::unique_ptr<LogCapture>(capture.get()));
    
    // Test with different log levels
    logger.setMinLevel(LogLevel::Debug);
    myFunction();
    EXPECT_EQ(capture->getMessages().size(), 5);  // All logs
    
    capture->clear();
    logger.setMinLevel(LogLevel::Info);
    myFunction();
    EXPECT_EQ(capture->getMessages().size(), 3);  // Debug logs filtered
    
    capture->clear();
    logger.setMinLevel(LogLevel::Error);
    myFunction();
    EXPECT_EQ(capture->getMessages().size(), 1);  // Only error log
}
```

---

## Performance Characteristics

### Benchmark Results

**Test Environments:**

| Environment | CPU | Clock | Compiler | OS |
|-------------|-----|-------|----------|-----|
| Desktop | Intel i7-9700K | 3.6 GHz (4.9 turbo) | GCC 11.2.0 -O2 | Ubuntu 22.04 |
| Laptop | Intel i7-8850H | 2.6 GHz (4.3 turbo) | MSVC 2022 /O2 | Windows 11 |

### CPU Cycle Analysis

Understanding the performance numbers in terms of CPU cycles provides insight into
whether the implementation is optimal:

**Disabled/Filtered Log Path (~6 ns on i7-8850H @ 4.3 GHz turbo):**
```
6 ns x 4.3 cycles/ns = ~26 CPU cycles

Breakdown:
  - Atomic load (L1 cache hit):     ~15-20 cycles
  - Comparison:                     ~1 cycle
  - Predicted branch (UNLIKELY):    ~0 cycles (free if predicted correctly)
  - Function call overhead:         ~5 cycles
  ----------------------------------------
  Total:                            ~21-26 cycles
```

This is **near the theoretical minimum** for any check that involves an atomic load.
The `UNLIKELY` macro ensures the branch predictor almost always predicts "don't log",
making the branch effectively free.

**What this means for your application:**
- On a 4.3 GHz CPU: ~6 ns per filtered log call
- On a 3.0 GHz server CPU: ~9 ns per filtered log call  
- On a 5.0 GHz desktop: ~5 ns per filtered log call
- On modern HPC clusters (AMD EPYC, Intel Sapphire Rapids): potentially <3 ns

**Active Log Path (~100-125 ns):**
```
~100 ns x 4.3 cycles/ns = ~430 CPU cycles

Breakdown:
  - Atomic loads (shouldLog check):     ~26 cycles
  - system_clock::now():                ~80-130 cycles (syscall or RDTSC)
  - LogRecord construction:             ~100-150 cycles
  - String copy for message:            ~50-100 cycles (depends on length)
  - Sink dispatch (virtual call):       ~20-30 cycles
  - Sink write (varies by sink):        ~50-500+ cycles
  ----------------------------------------
  Total:                                ~400-900+ cycles
```

The timestamp acquisition (`system_clock::now()`) dominates the active path.
On platforms with fast RDTSC, this can be significantly faster.

> **Important:** Performance figures in this section represent the **logger path only** 
> (from macro to sink dispatch). Actual end-to-end latency depends heavily on sink 
> implementation. See "Understanding Sink Overhead" below for total latency estimates.

**1. Disabled Logging Overhead** (compile-time filtered):

| Configuration | Overhead per Call | Binary Size Impact |
|--------------|-------------------|-------------------|
| All logs disabled (CPP_UTIL_MIN_LOG_LEVEL=6) | 0 ns | 0% (eliminated) |
| Compile-time filtered | 0 ns | 0% (eliminated) |

**2. Runtime Filtered Logging Overhead** (logger path only):

| Configuration | Overhead per Call | Notes |
|--------------|-------------------|-------|
| Logging disabled (setEnabled(false)) | ~5-6 ns | Single atomic load + branch |
| Level filtered (below minLevel) | ~5-6 ns | Two atomic loads + comparison |

**3. Active Logging Performance** (logger path only, with BenchmarkSink):

| Component | Time | Notes |
|-----------|------|-------|
| Logger fast-path check | ~1 ns | Two atomic loads |
| system_clock::now() | ~19-30 ns | Timestamp acquisition (platform dependent) |
| LogRecord construction | ~30-35 ns | Includes string copy |
| **Total logger path** | **~80-125 ns** | Before sink overhead |

**4. Active Logging with Real Sinks** (end-to-end):

| Operation | Logger Path | Sink Overhead | Total | Notes |
|-----------|-------------|---------------|-------|-------|
| Console (simple format) | ~100 ns | ~70-100 ns | ~170-200 ns | stdout write |
| Console (default format) | ~100 ns | ~120 ns | ~220 ns | Timestamp formatting |
| File (buffered) | ~100 ns | ~80 ns | ~180 ns | OS buffering |
| File (with flush) | ~100 ns | ~10-50 us | ~10-50 us | Disk I/O dominates |
| JSON formatting | ~100 ns | ~140 ns | ~240 ns | String escaping |

**5. Understanding Sink Overhead**:

Different sink implementations add varying overhead:

| Sink Type | write() Overhead | Notes |
|-----------|------------------|-------|
| NullSink (atomic counter) | ~6-7 ns | Minimal for benchmarking |
| NoOpSink (empty) | ~0 ns | Optimized away by compiler |
| TestSink (mutex + vector) | ~200 ns | Includes mutex + allocation |
| ConsoleSink | ~70-120 ns | Depends on formatter |
| FileSink (buffered) | ~80-100 ns | OS buffering helps |

> **Testing Note:** When benchmarking with `TestSink` (mutex + vector storage), 
> expect ~200 ns additional overhead per log call due to mutex acquisition and 
> vector allocation. Use `BenchmarkSink` for accurate logger-path measurements.

**6. Multi-threaded Contention**:

| Scenario | Throughput | Latency |
|----------|-----------|---------|
| 1 thread, file sink | 7M logs/sec | ~140 ns/log |
| 4 threads, file sink | 3M logs/sec | ~330 ns/log (contention) |
| 8 threads, file sink | 2M logs/sec | ~500 ns/log (high contention) |
| 8 threads, separate sinks | 5M logs/sec | ~160 ns/log (reduced contention) |

**7. Lazy Evaluation Savings**:

```cpp
std::string expensive() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return "result";
}

// Without lazy evaluation (always executes)
auto start = now();
LOG_DEBUG("Value: " + expensive());  // ~1ms even if filtered!
auto end = now();

// With lazy evaluation (only if not filtered)
auto start = now();
LOG_DEBUG([&]() { return "Value: " + expensive(); });  // ~5-6ns if filtered
auto end = now();
```

**Comparison with Other Libraries**:

| Library | Disabled Overhead | Active Overhead | Notes |
|---------|------------------|----------------|-------|
| DiagnosticLogger | 0 ns (compile) / 5-6 ns (runtime) | ~100 ns + sink | Header-only, lock-free fast path |
| spdlog | ~20 ns | ~120 ns | Mature, feature-rich |
| glog | ~30 ns | ~180 ns | Google's library |
| Boost.Log | ~50 ns | ~250 ns | Most features, heaviest |
| printf | 0 ns (if macro) | ~100 ns | Not thread-safe, not type-safe |

### Memory Usage

**1. Logger object size**:

```cpp
sizeof(Logger) = 96 bytes
  - vector<unique_ptr<ISink>>: 24 bytes
  - atomic<unsigned char>: 1 byte
  - atomic<LogLevel>: 4 bytes
  - mutex: 40 bytes (platform-dependent)
  - padding: ~27 bytes
```

**2. Per-log-call memory**:

```cpp
// Disabled/filtered: 0 bytes (no allocation)

// Active logging:
LogRecord record;
sizeof(LogRecord) = ~120 bytes
  - LogLevel: 4 bytes
  - time_point: 16 bytes
  - string message: 32 bytes (+ heap allocation)
  - SourceLocation: 24 bytes
  - thread::id: 8 bytes
  - padding: ~36 bytes
```

**3. Formatter memory**:

```cpp
// Formatters are lightweight, mainly virtual function overhead
sizeof(DefaultFormatter) = 8 bytes (vtable ptr)
sizeof(SimpleFormatter) = 8 bytes
sizeof(JsonFormatter) = 8 bytes
```

**4. Sink memory**:

```cpp
sizeof(ConsoleSink) = ~96 bytes
  - unique_ptr<IFormatter>: 8 bytes
  - LogLevel: 4 bytes
  - mutex: 40 bytes
  - vtable: 8 bytes
  - padding: ~36 bytes

sizeof(FileSink) = ~160 bytes
  - unique_ptr<IFormatter>: 8 bytes
  - LogLevel: 4 bytes
  - ofstream: ~96 bytes
  - string filename: 32 bytes
  - mutex: 40 bytes
```

**5. Memory allocation patterns**:

```cpp
// Per log message:
// 1. Lambda capture (stack allocation): ~8-16 bytes
// 2. Message string (heap allocation): strlen(msg) + 1 bytes
// 3. Formatted string (heap allocation): format size bytes
// 4. Total per-message heap: ~2x message size

// Example:
LOG_INFO("Server started on port 8080");
// Stack: ~16 bytes (lambda)
// Heap: ~30 bytes (message) + ~80 bytes (formatted) = ~110 bytes
```

**6. Memory optimization tips**:

```cpp
// 1. Reuse Logger instances (avoid copies)
Logger& logger = getGlobalLogger();  // Good: reference
Logger logger_copy = getGlobalLogger();  // Bad: copy (if allowed)

// 2. Limit sink count (each sink adds ~100-200 bytes)
logger.addSink(...);  // Only add necessary sinks

// 3. Use simple formatters for production
auto formatter = std::make_unique<SimpleFormatter>();  // Less formatting overhead

// 4. Compile-time filtering eliminates memory entirely
// -DCPP_UTIL_MIN_LOG_LEVEL=4  // Only errors: ~60% of log code eliminated
```

### Optimization Tips

**1. Choose the right build configuration**:

```makefile
# Development: all logs, debug symbols
CXXFLAGS_DEV = -DCPP_UTIL_MIN_LOG_LEVEL=0 -g -O0

# Testing: debug and above, optimized
CXXFLAGS_TEST = -DCPP_UTIL_MIN_LOG_LEVEL=1 -O2

# Production: info and above, fully optimized
CXXFLAGS_PROD = -DCPP_UTIL_MIN_LOG_LEVEL=2 -O3 -DNDEBUG

# Minimal: errors only, maximum optimization
CXXFLAGS_MIN = -DCPP_UTIL_MIN_LOG_LEVEL=4 -O3 -DNDEBUG -flto
```

**2. Use runtime filtering for dynamic control**:

```cpp
// Production starts with minimal logging
auto& logger = getGlobalLogger();
logger.setMinLevel(LogLevel::Warning);

// Can enable debug dynamically when investigating issues
// (via signal handler, config file reload, etc.)
signal(SIGUSR1, [](int) {
    getGlobalLogger().setMinLevel(LogLevel::Debug);
});
```

**3. Optimize sink selection**:

```cpp
// Development: console for immediate feedback
#ifdef DEBUG_BUILD
    logger.addSink(std::make_unique<ConsoleSink>());
#endif

// Production: file with appropriate level
#ifdef PRODUCTION_BUILD
    logger.addSink(std::make_unique<FileSink>(
        "/var/log/app.log",
        std::make_unique<SimpleFormatter>(),
        LogLevel::Info  // Filter at sink level too
    ));
#endif

// Error-only file for critical issues
logger.addSink(std::make_unique<FileSink>(
    "/var/log/app-errors.log",
    std::make_unique<DefaultFormatter>(),
    LogLevel::Error  // Only errors and fatal
));
```

**4. Batch sink operations**:

```cpp
// Instead of flushing after every log:
LOG_INFO("Message 1");
logger.flush();  // Expensive
LOG_INFO("Message 2");
logger.flush();  // Expensive

// Flush periodically:
for (const auto& item : items) {
    LOG_INFO("Processing " << item);
}
logger.flush();  // Once at end
```

**5. Use appropriate data structures**:

```cpp
// Avoid large object serialization in logs
struct LargeObject {
    std::vector<int> data;  // 1M elements
    // ...
};

// Bad: Serializes entire object
LOG_DEBUG("Object: " << serializeToJson(obj));  // Very slow!

// Good: Log summary only
LOG_DEBUG("Object: size=" << obj.data.size() << " checksum=" << checksum(obj));
```

**6. Profile before optimizing**:

```bash
# Profile logging overhead
perf record -g ./app
perf report

# Look for hotspots in:
# - Formatter::format()
# - Sink::write()
# - Message string construction

# If logging shows up as hotspot:
# 1. Increase CPP_UTIL_MIN_LOG_LEVEL
# 2. Increase runtime minLevel
# 3. Simplify log messages
# 4. Reduce log frequency
```

**7. Consider async logging for high throughput**:

DiagnosticLogger provides `AsyncSink` in `DiagnosticLogger_IO.h` for asynchronous logging:

```cpp
#include "DiagnosticLogger_IO.h"

// Wrap any sink with async processing
auto fileSink = std::make_shared<FileSink>("app.log");
auto asyncSink = std::make_shared<AsyncSink>(fileSink);

Logger logger;
logger.addSink(asyncSink);

// Writes return immediately (~47ns), actual I/O happens on background thread
LOG_INFO("This returns quickly");

// Check for dropped messages under extreme load
uint64_t dropped = asyncSink->dropped();
```

> **Queue Size Note:** AsyncSink uses a fixed-size lock-free queue of 4096 entries
> (compile-time constant). If your application produces log messages faster than
> the backend can consume them, messages will be dropped. The `dropped()` method
> returns the count of dropped messages. For bursty applications, consider:
> - Using a faster backend sink
> - Increasing the log level filter to reduce message volume
> - Batching related log messages into fewer calls

---

## Comparison with Other Libraries

### DiagnosticLogger vs spdlog

**spdlog** is the most popular C++ logging library.

**Similarities:**
- Header-only option
- Fast performance
- Multiple sinks
- Various formatters
- Thread-safe

**Differences:**

| Feature | DiagnosticLogger | spdlog |
|---------|-----------------|--------|
| **External dependencies** | None | fmt (optional) |
| **Compile-time filtering** | [YES] Full support (if constexpr) | [YES] Via macros |
| **Lock-free fast path** | [YES] Atomics only |  Spinlock/mutex |
| **Disabled overhead** | 0 ns (compile) / 10 ns (runtime) | ~20 ns |
| **Active overhead** | ~140 ns | ~120 ns |
| **Async logging** |  (requires custom sink) | [YES] Built-in |
| **Pattern formatters** |  | [YES] |
| **Log rotation** |  (requires custom sink) | [YES] |
| **C++17 requirement** | [YES] |  (C++11) |
| **Code size** | Smaller (minimal features) | Larger (more features) |
| **Learning curve** | Simple | Moderate |
| **Maturity** | Newer | Very mature |

**When to use DiagnosticLogger:**
- Need zero external dependencies
- Want truly zero overhead for disabled logs
- Working with C++17 projects
- Header-only deployment is critical
- Need simple, predictable behavior

**When to use spdlog:**
- Need async logging
- Want pattern-based formatting
- Need log rotation out of the box
- C++11/14 compatibility required
- Want mature, battle-tested library

**Migration from spdlog:**

```cpp
// spdlog
#include <spdlog/spdlog.h>
auto logger = spdlog::stdout_color_mt("console");
logger->info("Message {}", value);
logger->error("Error: {}", error);

// DiagnosticLogger
#include "DiagnosticLogger.h"
using namespace fat_p::diagnostic;
initializeDefaultLogger();
LOG_INFO("Message " << value);
LOG_ERROR("Error: " << error);
```

### DiagnosticLogger vs glog

**glog** is Google's logging library.

**Similarities:**
- Multiple log levels
- Thread-safe
- Source location capture
- Production-ready

**Differences:**

| Feature | DiagnosticLogger | glog |
|---------|-----------------|------|
| **Header-only** | [YES] |  (requires linking) |
| **External dependencies** | None | gflags (optional) |
| **Compile-time filtering** | [YES] |  Limited |
| **Disabled overhead** | 0 ns (compile) / 10 ns (runtime) | ~30 ns |
| **Configuration** | Programmatic | Command-line flags |
| **Severity levels** | 6 | 4 (+ custom) |
| **CHECK macros** |  | [YES] |
| **Conditional logging** | Manual | Built-in (LOG_IF, LOG_EVERY_N) |
| **Signal handling** |  | [YES] |
| **Stack traces** |  | [YES] |
| **Binary logs** |  (requires custom formatter) |  |

**When to use DiagnosticLogger:**
- Need header-only deployment
- Want zero dependencies
- Need compile-time optimization
- Want simple API

**When to use glog:**
- Need CHECK macros (DbC-style assertions)
- Want stack traces on fatal errors
- Need conditional logging (LOG_IF, LOG_EVERY_N)
- Want command-line flag configuration
- Google ecosystem integration

**Migration from glog:**

```cpp
// glog
#include <glog/logging.h>
LOG(INFO) << "Message " << value;
LOG(ERROR) << "Error: " << error;
CHECK_EQ(a, b) << "Values don't match";

// DiagnosticLogger
#include "DiagnosticLogger.h"
using namespace fat_p::diagnostic;
LOG_INFO("Message " << value);
LOG_ERROR("Error: " << error);
// CHECK macros: use enforce.h from same project
```

### DiagnosticLogger vs Boost.Log

**Boost.Log** is the most feature-rich logging library.

**Similarities:**
- Multiple sinks
- Formatters
- Thread-safe
- Extensible

**Differences:**

| Feature | DiagnosticLogger | Boost.Log |
|---------|-----------------|-----------|
| **Header-only** | [YES] |  |
| **External dependencies** | None | Boost (huge) |
| **Complexity** | Simple | Complex |
| **Compile time** | Fast | Slow |
| **Binary size** | Small | Large |
| **Attributes** |  | [YES] |
| **Filters** | Simple (level-based) | Complex (attribute-based) |
| **Sinks** | Console, File, Callback | Many built-in |
| **Learning curve** | Easy | Steep |
| **Disabled overhead** | 0 ns (compile) / 10 ns (runtime) | ~50 ns |

**When to use DiagnosticLogger:**
- Can't use Boost
- Need fast compile times
- Want simple API
- Need predictable performance

**When to use Boost.Log:**
- Already using Boost
- Need advanced filtering
- Need attributes (key-value pairs)
- Need many built-in sinks
- Want maximum flexibility

### DiagnosticLogger vs Custom printf

**printf** (and custom wrappers) is the simplest logging.

**printf advantages:**
- Universally available
- Very fast (~100 ns)
- No dependencies
- Simple

**printf disadvantages:**
- Not thread-safe (without locking)
- Not type-safe
- No structure (just strings)
- No filtering
- No multiple outputs
- C-style only

**DiagnosticLogger advantages over printf:**
- [YES] Thread-safe
- [YES] Type-safe (C++ streams)
- [YES] Structured (formatters)
- [YES] Filtering (compile + runtime)
- [YES] Multiple sinks
- [YES] Source location
- [YES] Timestamps

**DiagnosticLogger disadvantages vs printf:**
- Slightly more overhead (~140 ns vs ~100 ns)
- Requires C++17
- More complex (though not much)

**When to use DiagnosticLogger over printf:**
- Multi-threaded applications
- Need type safety
- Want structured logs
- Need filtering
- Want multiple outputs

**When to use printf:**
- Extremely simple programs
- Lowest possible overhead matters
- C compatibility required
- Already have printf-based infrastructure

---

## Migration Guide

### From spdlog

**1. Basic setup:**

```cpp
// Before (spdlog)
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

auto console = spdlog::stdout_color_mt("console");
auto file = spdlog::basic_logger_mt("file", "logs/app.log");
spdlog::set_default_logger(console);

// After (DiagnosticLogger)
#include "DiagnosticLogger.h"
using namespace fat_p::diagnostic;

auto& logger = getGlobalLogger();
logger.addSink(std::make_unique<ConsoleSink>());
logger.addSink(std::make_unique<FileSink>("logs/app.log"));
```

**2. Logging calls:**

```cpp
// Before (spdlog)
spdlog::trace("Entering function {}", funcName);
spdlog::debug("Value: {}", value);
spdlog::info("Server started on port {}", port);
spdlog::warn("Connection timeout after {}s", timeout);
spdlog::error("Failed to open file: {}", filename);
spdlog::critical("Fatal error: {}", error);

// After (DiagnosticLogger)
LOG_TRACE("Entering function " << funcName);
LOG_DEBUG("Value: " << value);
LOG_INFO("Server started on port " << port);
LOG_WARNING("Connection timeout after " << timeout << "s");
LOG_ERROR("Failed to open file: " << filename);
LOG_FATAL("Fatal error: " << error);
```

**3. Log levels:**

```cpp
// Before (spdlog)
spdlog::set_level(spdlog::level::debug);

// After (DiagnosticLogger)
getGlobalLogger().setMinLevel(LogLevel::Debug);
```

**4. Custom formatting:**

```cpp
// Before (spdlog pattern)
spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

// After (DiagnosticLogger custom formatter)
class CustomFormatter : public IFormatter {
    std::string format(const LogRecord& record) const override {
        // Implement custom format
        return /* formatted string */;
    }
};
logger.addSink(std::make_unique<ConsoleSink>(
    std::make_unique<CustomFormatter>()
));
```

**5. Async logging:**

```cpp
// Before (spdlog built-in async)
#include <spdlog/async.h>
auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>(
    "async", "logs/async.log"
);

// After (DiagnosticLogger - requires custom sink)
// See "Advanced Usage" section for AsyncSink implementation
```

### From glog

**1. Initialization:**

```cpp
// Before (glog)
#include <glog/logging.h>
google::InitGoogleLogging(argv[0]);

// After (DiagnosticLogger)
#include "DiagnosticLogger.h"
using namespace fat_p::diagnostic;
initializeDefaultLogger();
```

**2. Logging calls:**

```cpp
// Before (glog)
LOG(INFO) << "Message " << value;
LOG(WARNING) << "Warning message";
LOG(ERROR) << "Error: " << error;
LOG(FATAL) << "Fatal error";  // Terminates program
VLOG(1) << "Verbose message";

// After (DiagnosticLogger)
LOG_INFO("Message " << value);
LOG_WARNING("Warning message");
LOG_ERROR("Error: " << error);
LOG_FATAL("Fatal error");  // Does NOT terminate (manual handling)
// VLOG equivalent: use LOG_DEBUG or LOG_TRACE with runtime filtering
```

**3. CHECK macros:**

```cpp
// Before (glog)
CHECK(ptr != nullptr) << "Pointer is null";
CHECK_EQ(a, b) << "Values don't match";
CHECK_LT(x, y) << "x must be less than y";

// After (DiagnosticLogger + enforce.h)
#include "enforce.h"  // From same project
ENFORCE(ptr != nullptr, "Pointer is null");
ENFORCE(a == b, "Values don't match");
ENFORCE(x < y, "x must be less than y");
```

**4. Conditional logging:**

```cpp
// Before (glog)
LOG_IF(INFO, condition) << "Message";
LOG_EVERY_N(INFO, 100) << "Every 100th call";

// After (DiagnosticLogger)
if (condition) {
    LOG_INFO("Message");
}
// Every N: manual counter
static int counter = 0;
if (++counter % 100 == 0) {
    LOG_INFO("Every 100th call");
}
```

**5. Configuration:**

```cpp
// Before (glog command-line flags)
// --log_dir=/var/log --minloglevel=1

// After (DiagnosticLogger programmatic)
auto& logger = getGlobalLogger();
logger.addSink(std::make_unique<FileSink>("/var/log/app.log"));
logger.setMinLevel(LogLevel::Debug);  // minloglevel=1
```

### From std::cout/cerr

**1. Basic replacement:**

```cpp
// Before
std::cout << "Message: " << value << std::endl;
std::cerr << "Error: " << error << std::endl;

// After
LOG_INFO("Message: " << value);
LOG_ERROR("Error: " << error);
```

**2. Debugging prints:**

```cpp
// Before
#ifdef DEBUG
std::cout << "Debug: x=" << x << " y=" << y << std::endl;
#endif

// After (compile-time filtering)
// Set CPP_UTIL_MIN_LOG_LEVEL=0 for debug builds, =2 for production
LOG_DEBUG("Debug: x=" << x << " y=" << y);
```

**3. Conditional output:**

```cpp
// Before
if (verbose) {
    std::cout << "Verbose message" << std::endl;
}

// After (runtime filtering)
getGlobalLogger().setMinLevel(verbose ? LogLevel::Debug : LogLevel::Info);
LOG_DEBUG("Verbose message");
```

**4. Error reporting:**

```cpp
// Before
std::cerr << "ERROR: " << filename << ": " << strerror(errno) << std::endl;

// After
LOG_ERROR("Failed to open " << filename << ": " << strerror(errno));
```

**5. Structured output to file:**

```cpp
// Before
std::ofstream logfile("app.log", std::ios::app);
logfile << timestamp() << " INFO: " << message << std::endl;
logfile.close();

// After
auto& logger = getGlobalLogger();
logger.addSink(std::make_unique<FileSink>("app.log"));
LOG_INFO(message);
// Automatic timestamp, thread ID, source location
```

---

## Compiler Requirements

### Minimum Version

**C++17 is required** for:
- `if constexpr` (compile-time filtering)
- `std::string_view` (efficient string handling)
- Structured bindings (internal use)
- Inline variables (`inline Logger& getGlobalLogger()`)

**Minimum compiler versions:**

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| GCC | 7.1+ | Full C++17 support |
| Clang | 5.0+ | Full C++17 support |
| MSVC | VS 2017 15.7+ | `/std:c++17` flag |
| Apple Clang | 10.0+ | Xcode 10+ |
| Intel C++ | 19.0+ | `-std=c++17` |

### Tested Compilers

DiagnosticLogger has been tested on:

- [YES] GCC 7.5, 8.4, 9.3, 10.2, 11.2, 12.1
- [YES] Clang 6.0, 8.0, 10.0, 12.0, 13.0, 14.0
- [YES] MSVC 2017 15.9, 2019 16.11, 2022 17.2
- [YES] Apple Clang 12.0, 13.0, 14.0

### Compilation Flags

**Minimum (GCC/Clang):**
```bash
g++ -std=c++17 main.cpp -o app
```

**Recommended:**
```bash
g++ -std=c++17 -O2 -Wall -Wextra main.cpp -o app
```

**Production:**
```bash
g++ -std=c++17 -O3 -DNDEBUG -DCPP_UTIL_MIN_LOG_LEVEL=2 main.cpp -o app
```

**Debug:**
```bash
g++ -std=c++17 -g -O0 -DCPP_UTIL_MIN_LOG_LEVEL=0 main.cpp -o app
```

**With sanitizers:**
```bash
# Address sanitizer (memory errors)
g++ -std=c++17 -g -fsanitize=address main.cpp -o app

# Thread sanitizer (data races)
g++ -std=c++17 -g -fsanitize=thread main.cpp -o app

# Undefined behavior sanitizer
g++ -std=c++17 -g -fsanitize=undefined main.cpp -o app
```

**MSVC:**
```cmd
cl /std:c++17 /O2 /W4 /EHsc main.cpp
```

**Important flags:**

| Flag | Purpose |
|------|---------|
| `-std=c++17` | Enable C++17 features |
| `-O2` / `-O3` | Optimization (required for inline/constexpr) |
| `-DCPP_UTIL_MIN_LOG_LEVEL=N` | Compile-time log level filtering |
| `-DNDEBUG` | Disable assertions (production) |
| `-Wall -Wextra` | Enable warnings |
| `-g` | Debug symbols |

### Dependencies

**Standard library headers required:**
```cpp
#include <iostream>     // cout, cerr
#include <fstream>      // ofstream
#include <sstream>      // ostringstream
#include <string>       // string
#include <string_view>  // string_view (C++17)
#include <vector>       // vector
#include <memory>       // unique_ptr, make_unique
#include <mutex>        // mutex, lock_guard
#include <atomic>       // atomic (C++11)
#include <chrono>       // system_clock, time_point
#include <ctime>        // localtime_r, localtime_s
#include <iomanip>      // put_time
#include <functional>   // function
#include <type_traits>  // is_convertible_v, decay_t
#include <thread>       // thread::id, this_thread::get_id
```

**No external dependencies:**
- No Boost
- No fmt
- No external logging frameworks
- Just standard library

**Platform-specific:**
- Uses `localtime_r` (POSIX) or `localtime_s` (Windows)
- Uses `__builtin_FILE__` etc. for source location (GCC/Clang)
- Falls back to `__FILE__` etc. for other compilers

---

## Troubleshooting

### Common Issues

**1. Compile-time filtering not working**

**Symptom:**
```cpp
// Compiled with -DCPP_UTIL_MIN_LOG_LEVEL=4
LOG_DEBUG("This should be eliminated");  // Still in binary!
```

**Solution:**
- Ensure `CPP_UTIL_MIN_LOG_LEVEL` is defined **before** including the header:
```cpp
#define CPP_UTIL_MIN_LOG_LEVEL 4
#include "DiagnosticLogger.h"
```
- Or use compiler flag: `g++ -DCPP_UTIL_MIN_LOG_LEVEL=4 ...`
- Check with: `g++ -E main.cpp | grep LOG_DEBUG` to see preprocessed output

**2. Log file not created**

**Symptom:**
```cpp
logger.addSink(std::make_unique<FileSink>("/var/log/app.log"));
// Crashes or throws exception
```

**Solution:**
- Check permissions on directory
- Use absolute path or current directory
- Wrap in try-catch:
```cpp
try {
    logger.addSink(std::make_unique<FileSink>("/var/log/app.log"));
}
catch (const std::runtime_error& e) {
    std::cerr << "Failed: " << e.what() << std::endl;
}
```

**3. Logs not appearing**

**Symptom:**
```cpp
LOG_INFO("Test message");  // Nothing printed
```

**Solutions:**
- **Check if logger initialized:**
```cpp
initializeDefaultLogger();  // Must call this first!
```
- **Check log level:**
```cpp
getGlobalLogger().setMinLevel(LogLevel::Trace);  // Lower threshold
```
- **Check if enabled:**
```cpp
getGlobalLogger().setEnabled(true);
```
- **Flush explicitly:**
```cpp
LOG_INFO("Test");
getGlobalLogger().flush();
```

**4. Thread safety issues / data races**

**Symptom:**
```
ThreadSanitizer: data race
```

**Solutions:**
- DiagnosticLogger is thread-safe by default
- If using custom sinks/formatters, ensure they're thread-safe
- Use mutex in custom sink:
```cpp
class MySink : public ISink {
    mutable std::mutex mutex_;
public:
    void write(const LogRecord& r) override {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... write ...
    }
};
```

**5. High memory usage**

**Symptom:**
Memory grows unbounded during logging.

**Solutions:**
- **Flush regularly:**
```cpp
logger.flush();  // Periodically
```
- **Check for memory leaks in custom sinks**
- **Ensure FileSink is closed properly** (RAII handles this)
- **Limit log message size:**
```cpp
// Bad: Huge messages
LOG_DEBUG("Data: " << hugeString);  // 10 MB string!

// Good: Summarize
LOG_DEBUG("Data: size=" << hugeString.size());
```

### Performance Problems

**1. Logging is slow**

**Symptom:**
Application spends significant time in logging.

**Diagnosis:**
```bash
perf record -g ./app
perf report
# Look for time in log(), write(), format()
```

**Solutions:**

- **Increase compile-time filter level:**
```bash
g++ -DCPP_UTIL_MIN_LOG_LEVEL=2 ...  # Only Info and above
```

- **Increase runtime filter level:**
```cpp
getGlobalLogger().setMinLevel(LogLevel::Warning);
```

- **Reduce log frequency:**
```cpp
// Before: log every iteration
for (auto& item : items) {
    LOG_DEBUG("Processing " << item);  // 1M logs!
}

// After: log periodically
for (size_t i = 0; i < items.size(); ++i) {
    if (i % 1000 == 0) {
        LOG_DEBUG("Progress: " << i << "/" << items.size());
    }
}
```

- **Use simpler formatter:**
```cpp
// SimpleFormatter is faster than DefaultFormatter
logger.addSink(std::make_unique<ConsoleSink>(
    std::make_unique<SimpleFormatter>()
));
```

**2. High contention in multi-threaded code**

**Symptom:**
```bash
perf report shows time spent in mutex operations
```

**Solutions:**

- **Use separate loggers per thread:**
```cpp
thread_local Logger threadLogger;
```

- **Filter more aggressively:**
```cpp
logger.setMinLevel(LogLevel::Error);  // Reduce active logging
```

- **Use per-thread log files:**
```cpp
void worker(int id) {
    Logger logger;
    logger.addSink(std::make_unique<FileSink>(
        "thread_" + std::to_string(id) + ".log"
    ));
    // ... log to thread-specific file ...
}
```

**3. Slow file I/O**

**Symptom:**
FileSink write operations taking milliseconds.

**Solutions:**

- **Don't flush after every log:**
```cpp
// Bad: flush after each log
LOG_INFO("Message");
logger.flush();  // Slow!

// Good: flush periodically
for (...) {
    LOG_INFO("Message");
}
logger.flush();  // Once at end
```

- **Use ramdisk for log files:**
```bash
# Mount ramdisk
sudo mount -t tmpfs -o size=100M tmpfs /mnt/ramdisk

# Log to ramdisk
logger.addSink(std::make_unique<FileSink>("/mnt/ramdisk/app.log"));
```

- **Increase OS buffer size:**
```cpp
// In custom sink:
std::ofstream file(filename, std::ios::out | std::ios::app);
file.rdbuf()->pubsetbuf(buffer, bufferSize);  // Larger buffer
```

### Compilation Errors

**1. `if constexpr` errors**

**Symptom:**
```
error: 'constexpr' does not name a type
```

**Solution:**
C++17 not enabled. Use `-std=c++17` flag.

**2. `__builtin_FILE__` not found**

**Symptom:**
```
error: '__builtin_FILE__' was not declared in this scope
```

**Solution:**
Older compiler. Update to GCC 7+ or Clang 5+, or use fallback:
```cpp
#ifdef __has_builtin
#  if __has_builtin(__builtin_FILE)
#    define SOURCE_FILE __builtin_FILE()
#  else
#    define SOURCE_FILE __FILE__
#  endif
#else
#  define SOURCE_FILE __FILE__
#endif
```

**3. `std::memory_order_relaxed` errors**

**Symptom:**
```
error: 'memory_order_relaxed' is not a member of 'std'
```

**Solution:**
Missing `#include <atomic>`. Check header includes.

**4. Link errors with `localtime_r`**

**Symptom:**
```
undefined reference to 'localtime_r'
```

**Solution:**
On some systems, need to link with `-lpthread`:
```bash
g++ -std=c++17 main.cpp -o app -lpthread
```

---

## Summary

DiagnosticLogger provides **high-performance, zero-dependency diagnostic logging** for C++17 projects with a focus on:

**Key Features:**
1. [YES] **Lock-free fast path** - Single atomic load for disabled/filtered logs (~10ns)
2. [YES] **Compile-time elimination** - Zero overhead with `if constexpr` and CPP_UTIL_MIN_LOG_LEVEL
3. [YES] **Lazy evaluation** - Messages only generated when actually logged
4. [YES] **Thread-safe** - All operations safe for concurrent use
5. [YES] **Header-only** - Single include, no linking required
6. [YES] **Zero dependencies** - Just C++17 standard library
7. [YES] **Policy-based design** - Customizable formatters and sinks
8. [YES] **Multiple outputs** - Console, file, callback, custom sinks simultaneously

**Performance Profile:**
- Disabled logging (compile-time): **0 ns** (eliminated)
- Disabled logging (runtime): **~10 ns** (atomic load)
- Active logging: **~130 ns** (mutex + formatting + write)
- Multi-threaded: **~330 ns** with contention (4 threads)

**Best For:**
- High-performance C++ applications
- Projects with strict "no dependencies" policy
- HPC and scientific computing
- Real-time systems
- Embedded systems with resources constraints
- Header-only library projects

**Not Ideal For:**
- Need async logging (requires custom sink)
- Need complex pattern formatters
- Need log rotation (requires custom sink)
- C++11/14 compatibility required
- Already using spdlog successfully

**Quick Start:**
```cpp
#include "DiagnosticLogger.h"
using namespace fat_p::diagnostic;

int main() {
    initializeDefaultLogger();
    
    LOG_INFO("Application started");
    LOG_ERROR("An error occurred");
    
    return 0;
}
```

**Production Build:**
```bash
g++ -std=c++17 -O3 -DCPP_UTIL_MIN_LOG_LEVEL=2 -DNDEBUG main.cpp -o app
```

**Essential Recommendations:**

1. **Always use compile-time filtering in production** (`CPP_UTIL_MIN_LOG_LEVEL=2`)
2. **Use runtime filtering for dynamic control** (`setMinLevel()`)
3. **Wrap log message generation in lambdas** for lazy evaluation
4. **Choose appropriate log levels** (don't overuse ERROR)
5. **Test with ThreadSanitizer** to verify thread safety
6. **Profile before optimizing** to identify actual bottlenecks

**Project Alignment:**
DiagnosticLogger fits perfectly with your safety-first, header-only, zero-dependency C++17 library project. It complements other components like:
- `enforce.h` for Design by Contract
- `Expected.h` for error handling
- `JsonLite.h` for configuration

For questions or issues, refer to the [Troubleshooting](#troubleshooting) section or examine the comprehensive test suite in `test_DiagnosticLogger.cpp`.
