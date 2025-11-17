# FatPJsonLite User Manual

## Table of Contents

1. [What is FatPJsonLite and Why Use It?](#what-is-fatpjsonlite-and-why-use-it)
   - [Understanding FatPJsonLite](#understanding-fatpjsonlite)
   - [The Performance-Safety Continuum](#the-performance-safety-continuum)
   - [Where FatPJsonLite Fits](#where-fatpjsonlite-fits)
2. [Core Architecture](#core-architecture)
   - [The Expected-Based Type System](#the-expected-based-type-system)
   - [Optimized Data Structures](#optimized-data-structures)
   - [Design Philosophy](#design-philosophy)
3. [Getting Started](#getting-started)
   - [Prerequisites](#prerequisites)
   - [Integration](#integration)
   - [Compilation](#compilation)
   - [First Program](#first-program)
4. [Expected-Based Error Handling](#expected-based-error-handling)
   - [Why Expected Over Exceptions](#why-expected-over-exceptions)
   - [JsonError Structure](#jsonerror-structure)
   - [Error Codes Reference](#error-codes-reference)
   - [Error Handling Patterns](#error-handling-patterns)
   - [Monadic Operations](#monadic-operations)
5. [Parsing and Serialization](#parsing-and-serialization)
   - [Exception-Free Parsing](#exception-free-parsing)
   - [Safe Numeric Conversions](#safe-numeric-conversions)
   - [Safe Type Conversions](#safe-type-conversions)
   - [Converting Between Types](#converting-between-types)
6. [Enum Serialization](#enum-serialization)
   - [EnumPlus Integration](#enumplus-integration)
   - [Enum to JSON](#enum-to-json)
   - [JSON to Enum](#json-to-enum)
   - [Struct Fields with Enums](#struct-fields-with-enums)
   - [Error Handling](#error-handling)
7. [Optimized Data Structures](#optimized-data-structures-1)
   - [FatPJsonArray (SmallVector)](#fatpjsonarray-smallvector)
   - [FatPJsonObject (FlatMap)](#fatpjsonobject-flatmap)
   - [Performance Characteristics](#performance-characteristics)
8. [String Pool and Memory Optimization](#string-pool-and-memory-optimization)
   - [Understanding StringPool](#understanding-stringpool)
   - [PooledJsonObject Usage](#pooledjsonobject-usage)
   - [Memory Savings Analysis](#memory-savings-analysis)
   - [When to Use String Pools](#when-to-use-string-pools)
9. [File I/O Operations](#file-io-operations)
   - [Standard File Operations](#standard-file-operations)
   - [Atomic Save Operations](#atomic-save-operations)
   - [Memory-Mapped File I/O](#memory-mapped-file-io)
   - [When to Use Memory Mapping](#when-to-use-memory-mapping)
10. [JSON Pointer with Expected](#json-pointer-with-expected)
    - [Exception-Free Navigation](#exception-free-navigation)
    - [Type-Safe Queries](#type-safe-queries)
    - [Mutable Access](#mutable-access)
    - [Error Handling](#error-handling-1)
    - [Comparison with JsonLite](#comparison-with-jsonlite)
11. [Advanced Features](#advanced-features)
    - [Batch Parsing](#batch-parsing)
    - [Numeric Overflow Detection](#numeric-overflow-detection)
    - [Thread Safety Considerations](#thread-safety-considerations)
12. [Performance Benchmarks](#performance-benchmarks)
    - [Parsing Performance](#parsing-performance)
    - [Data Structure Performance](#data-structure-performance)
    - [Error Handling Performance](#error-handling-performance)
    - [Memory Usage](#memory-usage)
13. [Migration Guide](#migration-guide)
    - [From JsonLite to FatPJsonLite](#from-jsonlite-to-fatpjsonlite)
    - [From Exception-Based to Expected-Based](#from-exception-based-to-expected-based)
    - [When to Stay with JsonLite](#when-to-stay-with-jsonlite)
14. [Comparison with JsonLite](#comparison-with-jsonlite-1)
    - [Feature Comparison](#feature-comparison)
    - [Performance Comparison](#performance-comparison)
    - [Use Case Matrix](#use-case-matrix)
15. [Use Case Guide](#use-case-guide)
    - [High-Performance Services](#high-performance-services)
    - [Exception-Free Environments](#exception-free-environments)
    - [Large Dataset Processing](#large-dataset-processing)
    - [Real-Time Systems](#real-time-systems)
    - [Memory-Constrained Environments](#memory-constrained-environments)
16. [Best Practices](#best-practices)
    - [Error Handling Patterns](#error-handling-patterns-1)
    - [Memory Management](#memory-management)
    - [Performance Optimization](#performance-optimization)
    - [Testing Strategies](#testing-strategies)
17. [API Reference](#api-reference)
    - [Parsing Functions](#parsing-functions)
    - [File I/O Functions](#file-io-functions)
    - [Conversion Functions](#conversion-functions)
    - [Utility Functions](#utility-functions)
18. [Compiler Requirements](#compiler-requirements)
    - [Minimum Version](#minimum-version)
    - [Required Dependencies](#required-dependencies)
    - [Compilation Flags](#compilation-flags)
19. [Summary](#summary)

---

## What is FatPJsonLite and Why Use It?

### Understanding FatPJsonLite

FatPJsonLite is an enhanced JSON library that extends JsonLite with powerful components from the 
fat_p ecosystem. It's designed for applications that need both **high performance** and 
**bulletproof safety** without the overhead of exceptions.

While JsonLite prioritizes safety and simplicity, FatPJsonLite adds:
- **Expected-based error handling**: Zero-overhead error propagation without exceptions
- **Optimized data structures**: 2-5x faster operations with FlatMap and SmallVector
- **Memory optimization**: 30-50% memory savings with StringPool deduplication
- **Large file support**: Memory-mapped I/O for files >10MB
- **Enhanced safety**: Overflow detection with CheckedArithmetic
- **JSON Pointer with Expected**: Exception-free RFC 6901 navigation
- **Atomic saves**: Crash-safe file operations with temp files
- **Enum serialization**: First-class support via EnumPlus integration

### The Performance-Safety Continuum

The C++ JSON landscape spans a spectrum:

```
Maximum Speed                                      Maximum Safety
    |                                                     |
simdjson ---- RapidJSON ---- FatPJsonLite ---- JsonLite ---- Boost.PropertyTree
(read-only)   (raw speed)   (balanced)        (simple)      (multi-format)
```

FatPJsonLite occupies the **sweet spot**: production-grade performance with compile-time and 
runtime safety guarantees.

### Where FatPJsonLite Fits

Choose FatPJsonLite when you need:

✓ **Exception-free code paths**: Embedded systems, game engines, real-time systems  
✓ **High-performance parsing**: 100-500 MB/s throughput (similar to JsonLite)  
✓ **Memory efficiency**: Large datasets with repeated keys (30-50% savings)  
✓ **Large file support**: Files from 10MB to several GB with memory mapping  
✓ **Type safety**: All the safety of JsonLite, with Expected error handling  
✓ **Atomic operations**: Crash-safe file saves with temp file pattern  
✓ **Enum support**: Type-safe enum serialization with EnumPlus

FatPJsonLite makes trade-offs that JsonLite doesn't:
- ✓ Requires additional fat_p headers (Expected.h, FlatMap.h, SmallVector.h, etc.)
- ✓ Slightly more complex API (Expected vs direct values)
- ✓ Larger binary size (~20-30% more than JsonLite)
- ✓ Requires understanding of Expected-based error handling

---

## Core Architecture

### The Expected-Based Type System

FatPJsonLite uses `Expected<T, JsonError>` for all operations that can fail:

```cpp
// JsonLite - throws exceptions
try {
    JsonValue val = parse_json(json_str);  // May throw
    std::string name = from_json<std::string>(val["name"]);  // May throw
} catch (const std::exception& e) {
    // Handle error
}

// FatPJsonLite - returns Expected
auto result = try_parse_json(json_str);
if (!result) {
    std::cerr << "Parse failed: " << result.error().message << "\n";
    return;
}

auto name = safe_from_json<std::string>((*result)["name"]);
if (!name) {
    std::cerr << "Conversion failed: " << name.error().message << "\n";
    return;
}
```

This provides:
1. ✓ **Zero-overhead error handling**: No stack unwinding, no RTTI
2. ✓ **Explicit error paths**: Impossible to ignore errors
3. ✓ **Composable**: Expected values chain naturally with monadic operations
4. ✓ **Debuggable**: Rich error context at point of failure

### Optimized Data Structures

FatPJsonLite replaces standard containers with optimized alternatives:

| JsonLite | FatPJsonLite | Benefit |
|----------|--------------|---------|
| `std::vector<JsonValue>` | `SmallVector<JsonValue, 8>` | No heap allocation for ≤8 elements |
| `std::map<string, JsonValue>` | `FlatMap<string, JsonValue>` | Contiguous storage, better cache |
| Individual strings | `StringPool` | Deduplicate repeated keys |

From benchmark results:
- **Array operations**: 13.5x faster for small arrays (≤8 elements)
- **Object operations**: Varies by use case (FlatMap is 0.62x for large objects but faster for 
  iteration)
- **Memory**: 99.9% memory savings with StringPool for repeated keys

These optimizations are **transparent**: you can still use `JsonArray` and `JsonObject` if 
needed, or opt into the optimized versions.

### Design Philosophy

FatPJsonLite follows three core principles:

1. **Performance Without Sacrifice**: Fast code should also be safe code
2. **Explicit Over Implicit**: Errors are values, not exceptions
3. **Composability**: Functions return Expected for easy chaining with monadic operations

```cpp
// Composable error handling with and_then()
auto load_config() -> Expected<Config, JsonError> {
    return try_load_json("config.json")
        .and_then([](JsonValue v) { 
            return safe_from_json<Config>(v); 
        })
        .or_else([](JsonError e) { 
            log_error(e);
            return Config{};  // Return default
        });
}
```

---

## Getting Started

### Prerequisites

- C++17 or later compiler
- Standard library with `<variant>`, `<optional>`, `<string_view>`
- fat_p headers: Expected.h, FlatMap.h, SmallVector.h, StringPool.h, CheckedArithmetic.h, 
  MemoryMappedFile.h, enforce.h, ConcurrencyPolicies.h, ScopeGuard.h, EnumPlus.h

### Integration

FatPJsonLite is header-only. Copy the required headers to your project:

```bash
# Required headers
JsonLite.h
FatPJsonLite.h
Expected.h
FlatMap.h
SmallVector.h
StringPool.h
CheckedArithmetic.h
MemoryMappedFile.h
enforce.h
ConcurrencyPolicies.h
ScopeGuard.h
EnumPlus.h
```

Include and use:

```cpp
#include "FatPJsonLite.h"
USING_FATP_JSON_LITE()
```

The `USING_FATP_JSON_LITE()` macro expands to:

```cpp
using namespace fat_p;
using fat_p::try_parse_json;
using fat_p::try_load_json;
using fat_p::load_json_mmap;
using fat_p::try_save_json;
using fat_p::try_save_atomic;
using fat_p::safe_from_json;
using fat_p::safe_from_json_numeric;
using fat_p::safe_from_json_enum;
using fat_p::try_query_json_pointer;
using fat_p::try_query_json_as;
```

### Compilation

**Minimum flags:**
```bash
g++ -std=c++17 -I/path/to/headers main.cpp -o app
```

**Recommended flags:**
```bash
g++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -I/path/to/headers main.cpp -o app
```

**Debug with sanitizers:**
```bash
g++ -std=c++17 -g -fsanitize=address,undefined -I/path/to/headers main.cpp -o app
```

### First Program

```cpp
#include "FatPJsonLite.h"
USING_FATP_JSON_LITE()

#include <iostream>

struct Config
{
    int port;
    std::string host;
    std::optional<int> timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, timeout)

int main()
{
    // Parse JSON without exceptions
    std::string json = R"({"port": 8080, "host": "localhost", "timeout": 30})";
    auto result = try_parse_json(json);
    
    if (!result)
    {
        std::cerr << "Parse error: " << result.error().message << "\n";
        return 1;
    }
    
    // Convert to struct safely
    auto config = safe_from_json<Config>(*result);
    if (!config)
    {
        std::cerr << "Conversion error: " << config.error().message << "\n";
        return 1;
    }
    
    std::cout << "Port: " << config->port << "\n";
    std::cout << "Host: " << config->host << "\n";
    if (config->timeout)
    {
        std::cout << "Timeout: " << *config->timeout << "\n";
    }
    
    // Save atomically (crash-safe)
    auto save_result = try_save_atomic<PrettyJsonPolicy>("config.json", *result);
    if (!save_result)
    {
        std::cerr << "Save error: " << save_result.error().message << "\n";
        return 1;
    }
    
    return 0;
}
```

---

## Expected-Based Error Handling

### Why Expected Over Exceptions

Exceptions have several drawbacks in performance-critical and embedded systems:

**Exception problems:**
- ❌ Unpredictable performance (stack unwinding)
- ❌ Binary size overhead (RTTI, exception tables)
- ❌ Cannot be used in exception-disabled environments
- ❌ Hidden control flow
- ❌ Difficult to reason about error paths

**Expected advantages:**
- ✅ Zero-overhead error handling
- ✅ Explicit error paths
- ✅ Composable with monadic operations
- ✅ Works in `-fno-exceptions` environments
- ✅ Clear control flow

### JsonError Structure

All FatPJsonLite functions return `Expected<T, JsonError>`:

```cpp
struct JsonError
{
    JsonErrorCode code;      // Specific error type
    std::string message;     // Human-readable description
    size_t position;         // Position in source (for parse errors)
    std::string context;     // Additional context
    
    operator bool() const noexcept;  // Check if error exists
    std::string to_string() const;   // Full error description
};
```

**Example error:**
```cpp
auto result = try_parse_json("invalid json");
if (!result)
{
    const JsonError& err = result.error();
    std::cout << "Code: " << err.code << "\n";
    std::cout << "Message: " << err.message << "\n";
    std::cout << "Position: " << err.position << "\n";
    std::cout << "Full: " << err.to_string() << "\n";
}

// Output:
// Code: ParseError
// Message: JSON parse error: unexpected token
// Position: 8
// Full: JsonError[ParseError]: JSON parse error: unexpected token at position 8
```

### Error Codes Reference

```cpp
enum class JsonErrorCode
{
    Success,           // No error (should not appear in Expected<T, JsonError>)
    ParseError,        // Syntax error in JSON
    TypeError,         // Type mismatch during conversion
    RangeError,        // Deprecated (use NumericOverflow)
    FileError,         // File I/O failure
    DepthExceeded,     // Nesting depth limit exceeded
    MemoryError,       // Memory allocation failure
    InvalidUtf8,       // Invalid UTF-8 sequence
    NumericOverflow,   // Numeric value out of range for target type
    MissingField,      // Required struct field missing
    ExtraData          // Extra data after valid JSON
};
```

**Error code semantics:**

- **ParseError**: Malformed JSON syntax (unclosed braces, invalid tokens, etc.)
- **TypeError**: Attempting to convert value to incompatible type
- **FileError**: Cannot open, read, or write file
- **DepthExceeded**: JSON nesting deeper than allowed (default: 100 levels)
- **NumericOverflow**: Value doesn't fit in target numeric type
- **MissingField**: Struct deserialization missing required field
- **ExtraData**: Additional text after valid JSON document
- **InvalidUtf8**: Invalid UTF-8 byte sequence in string

### Error Handling Patterns

**Pattern 1: Early return on error**

```cpp
Expected<Config, JsonError> load_config(const std::string& filename)
{
    auto json_result = try_load_json(filename);
    if (!json_result)
    {
        return unexpected(json_result.error());
    }
    
    return safe_from_json<Config>(*json_result);
}
```

**Pattern 2: Provide defaults on error**

```cpp
Config load_config_or_default(const std::string& filename)
{
    auto result = try_load_json(filename)
        .and_then([](JsonValue v) { return safe_from_json<Config>(v); });
    
    if (result)
    {
        return *result;
    }
    else
    {
        std::cerr << "Using defaults: " << result.error().message << "\n";
        return Config{};  // Default config
    }
}
```

**Pattern 3: Error logging and recovery**

```cpp
void process_config_files(const std::vector<std::string>& files)
{
    for (const auto& file : files)
    {
        auto result = try_load_json(file);
        if (!result)
        {
            log_error("Failed to load", file, result.error());
            continue;  // Skip this file, process others
        }
        
        process(*result);
    }
}
```

**Pattern 4: Validation with custom errors**

```cpp
Expected<Config, JsonError> load_and_validate(const std::string& filename)
{
    auto config = try_load_json(filename)
        .and_then([](JsonValue v) { return safe_from_json<Config>(v); });
    
    if (!config)
    {
        return unexpected(config.error());
    }
    
    // Custom validation
    if (config->port < 1024 || config->port > 65535)
    {
        return unexpected(JsonError{
            JsonErrorCode::RangeError,
            "Port must be 1024-65535",
            0,
            "port=" + std::to_string(config->port)
        });
    }
    
    return config;
}
```

### Monadic Operations

Expected supports monadic operations for composable error handling:

**and_then()** - Chain operations that return Expected:

```cpp
auto result = try_parse_json(json_str)
    .and_then([](JsonValue v) {
        return safe_from_json<Config>(v);
    })
    .and_then([](Config cfg) -> Expected<Config, JsonError> {
        if (cfg.port < 1024)
        {
            return unexpected(JsonError{
                JsonErrorCode::RangeError,
                "Invalid port",
                0,
                "port=" + std::to_string(cfg.port)
            });
        }
        return cfg;
    });

if (result)
{
    std::cout << "Valid config: " << result->port << "\n";
}
```

**or_else()** - Provide fallback on error:

```cpp
auto config = try_load_json("config.json")
    .and_then([](JsonValue v) { return safe_from_json<Config>(v); })
    .or_else([](JsonError e) -> Expected<Config, JsonError> {
        std::cerr << "Error: " << e.message << ", using defaults\n";
        return Config{8080, "localhost"};  // Default config
    });

// config is always valid here (either loaded or default)
std::cout << "Using port: " << config->port << "\n";
```

**transform()** - Apply function to success value:

```cpp
auto port = try_load_json("config.json")
    .and_then([](JsonValue v) { return safe_from_json<Config>(v); })
    .transform([](const Config& cfg) { return cfg.port; });

if (port)
{
    std::cout << "Port: " << *port << "\n";
}
```

**Chaining multiple operations:**

```cpp
auto load_and_process() -> Expected<ProcessedData, JsonError>
{
    return try_load_json("data.json")
        .and_then([](JsonValue v) { 
            return safe_from_json<RawData>(v); 
        })
        .and_then([](RawData raw) { 
            return validate(raw); 
        })
        .and_then([](RawData raw) { 
            return process(raw); 
        });
}
```

---

## Parsing and Serialization

### Exception-Free Parsing

**try_parse_json()** - Parse without throwing:

```cpp
std::string json = R"({"name": "Alice", "age": 30})";
auto result = try_parse_json(json);

if (!result)
{
    std::cerr << "Parse failed: " << result.error().message << "\n";
    std::cerr << "At position: " << result.error().position << "\n";
    return;
}

JsonValue val = *result;  // Extract value
```

**With policies:**

```cpp
// Pretty printing
auto result = try_parse_json<PrettyJsonPolicy>(json);

// JSONC comments
std::string jsonc = R"(
{
    // Comment
    "key": "value"
}
)";
auto result2 = try_parse_json<ConfigJsonPolicy>(jsonc);

// NaN/Infinity support
auto result3 = try_parse_json<CompatJsonPolicy>("NaN");
```

### Safe Numeric Conversions

**safe_from_json_numeric<T>()** - Convert with overflow detection:

```cpp
JsonValue j_int = to_json(42);
JsonValue j_big = to_json(INT64_MAX);
JsonValue j_float = to_json(3.14);

// Safe conversions
auto int_val = safe_from_json_numeric<int>(j_int);
if (int_val)
{
    std::cout << "Value: " << *int_val << "\n";
}

// Overflow detection
auto small = safe_from_json_numeric<int8_t>(j_big);
if (!small)
{
    std::cerr << "Overflow: " << small.error().message << "\n";
    // Error: Numeric value 9223372036854775807 out of range for target type
}

// Fractional detection
auto int_from_float = safe_from_json_numeric<int>(j_float);
if (!int_from_float)
{
    std::cerr << "Has fraction: " << int_from_float.error().message << "\n";
    // Error: Cannot convert double with fractional part to integer
}

// Negative to unsigned
JsonValue j_neg = to_json(-42);
auto unsigned_val = safe_from_json_numeric<unsigned int>(j_neg);
if (!unsigned_val)
{
    std::cerr << "Negative: " << unsigned_val.error().message << "\n";
}
```

**Supported types:**
- All signed integers: int8_t, int16_t, int32_t, int64_t, int, long, etc.
- All unsigned integers: uint8_t, uint16_t, uint32_t, uint64_t, unsigned, etc.
- Floating-point: float, double

**Safety checks:**
- Overflow/underflow for target type
- Fractional part when converting float→int
- Negative value when converting to unsigned
- Full int64_t and double range (no artificial margins)

### Safe Type Conversions

**safe_from_json<T>()** - Generic safe conversion:

```cpp
JsonValue j = parse_json(R"({"name": "Alice", "age": 30})");

// Convert to specific types
auto name = safe_from_json<std::string>(j["name"]);
auto age = safe_from_json<int>(j["age"]);

if (name && age)
{
    std::cout << *name << " is " << *age << " years old\n";
}

// Convert to struct
struct Person { std::string name; int age; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age)

auto person = safe_from_json<Person>(j);
if (person)
{
    std::cout << person->name << "\n";
}
```

**Containers:**

```cpp
JsonValue j_arr = parse_json("[1, 2, 3, 4, 5]");
auto vec = safe_from_json<std::vector<int>>(j_arr);

JsonValue j_obj = parse_json(R"({"a": 1, "b": 2})");
auto map = safe_from_json<std::map<std::string, int>>(j_obj);

JsonValue j_opt = parse_json("null");
auto opt = safe_from_json<std::optional<int>>(j_opt);  // nullopt
```

### Converting Between Types

**to_json_array() / from_json_array()** - Convert between JsonArray and FatPJsonArray:

```cpp
// JsonArray to FatPJsonArray (optimized)
JsonArray standard = {to_json(1), to_json(2), to_json(3)};
FatPJsonArray optimized = from_json_array(standard);

// FatPJsonArray to JsonArray (standard)
FatPJsonArray arr;
arr.push_back(to_json(10));
arr.push_back(to_json(20));
JsonArray standard2 = to_json_array(arr);
```

**to_json_object() / from_json_object()** - Convert between JsonObject and FatPJsonObject:

```cpp
// JsonObject to FatPJsonObject
JsonObject standard;
standard["key"] = to_json("value");
FatPJsonObject<> optimized = from_json_object(standard);

// FatPJsonObject to JsonObject
FatPJsonObject<> obj;
obj["port"] = to_json(8080);
JsonObject standard2 = to_json_object(obj);
```

**Use cases:**
- Interfacing with JsonLite code
- Optimizing hot paths with FatPJsonArray/Object
- Converting file I/O results to optimized structures

---

## Enum Serialization

FatPJsonLite provides first-class support for enum serialization through integration with 
EnumPlus. Enums are serialized as strings, making JSON human-readable and robust to enum value 
changes.

### EnumPlus Integration

First, define your enum with EnumPlus macros:

```cpp
#include "EnumPlus.h"

enum class Priority
{
    Low,
    Medium,
    High,
    Critical
};
DEFINE_ENUM_WITH_STRING_CONVERSIONS(Priority, Low, Medium, High, Critical)

enum class Color
{
    Red,
    Green,
    Blue
};
DEFINE_ENUM_WITH_STRING_CONVERSIONS(Color, Red, Green, Blue)
```

### Enum to JSON

Enums automatically serialize to JSON strings:

```cpp
Priority p = Priority::High;
JsonValue j = to_json(p);

std::string json_str = to_json_string(j);
// "High"

Color c = Color::Blue;
JsonValue j_color = to_json(c);
// "Blue"
```

### JSON to Enum

**safe_from_json_enum<E>()** - Convert string to enum with error handling:

```cpp
JsonValue j = parse_json(R"("High")");

// Safe conversion
auto priority = safe_from_json_enum<Priority>(j);
if (priority)
{
    std::cout << "Priority: " << to_string(*priority) << "\n";
}

// Invalid enum value
JsonValue j_invalid = parse_json(R"("SuperHigh")");
auto bad_priority = safe_from_json_enum<Priority>(j_invalid);
if (!bad_priority)
{
    std::cerr << "Error: " << bad_priority.error().message << "\n";
    // Error: Unknown enum value: SuperHigh
}

// Type mismatch
JsonValue j_wrong_type = parse_json("42");
auto wrong = safe_from_json_enum<Priority>(j_wrong_type);
if (!wrong)
{
    std::cerr << "Error: " << wrong.error().message << "\n";
    // Error: Expected string for enum, got integer
}
```

### Struct Fields with Enums

Enums work seamlessly in structs:

```cpp
struct Task
{
    std::string name;
    Priority priority;
    Color tag_color;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Task, name, priority, tag_color)

// Serialize
Task task{"Fix bug", Priority::Critical, Color::Red};
JsonValue j = to_json(task);
std::string json = to_json_string<PrettyJsonPolicy>(j);
// {
//   "name": "Fix bug",
//   "priority": "Critical",
//   "tag_color": "Red"
// }

// Deserialize
std::string json_input = R"({
    "name": "Write docs",
    "priority": "Medium",
    "tag_color": "Blue"
})";
auto result = try_parse_json(json_input);
if (result)
{
    auto task_result = safe_from_json<Task>(*result);
    if (task_result)
    {
        std::cout << "Task: " << task_result->name << "\n";
        std::cout << "Priority: " << to_string(task_result->priority) << "\n";
    }
}
```

### Error Handling

Enum conversion errors are reported with clear messages:

```cpp
// Test all enum values
for (auto p : {Priority::Low, Priority::Medium, Priority::High, Priority::Critical})
{
    JsonValue j = to_json(p);
    auto restored = safe_from_json_enum<Priority>(j);
    
    if (!restored)
    {
        std::cerr << "Failed: " << restored.error().message << "\n";
    }
    else if (*restored != p)
    {
        std::cerr << "Mismatch: expected " << to_string(p) 
                  << ", got " << to_string(*restored) << "\n";
    }
}

// Invalid values
std::vector<std::string> invalid_values = {"Invalid", "CRITICAL", "high", ""};
for (const auto& val : invalid_values)
{
    JsonValue j = parse_json(R"(")" + val + R"(")");
    auto result = safe_from_json_enum<Priority>(j);
    
    if (!result)
    {
        std::cout << "Correctly rejected: " << val << "\n";
        std::cout << "  Reason: " << result.error().message << "\n";
    }
}
```

**Error codes for enum conversions:**
- **TypeError**: Value is not a string (e.g., integer, object, array)
- **RangeError**: String value doesn't match any enum constant

**Benefits:**
- Human-readable JSON (strings instead of integers)
- Robust to enum value reordering
- Compiler-checked enum names
- Clear error messages for invalid values
- Works seamlessly with structs

---

## Optimized Data Structures

FatPJsonLite provides optimized alternatives to standard containers for improved performance and 
memory efficiency.

### FatPJsonArray (SmallVector)

`FatPJsonArray` uses SmallVector with inline storage for small arrays:

```cpp
using FatPJsonArray = SmallVector<JsonValue, 8>;
```

**Benefits:**
- No heap allocation for arrays with ≤8 elements
- 13.5x faster than std::vector for small arrays (from benchmark)
- Contiguous storage for better cache locality

**Usage:**

```cpp
// Create optimized array
FatPJsonArray arr;
arr.push_back(to_json(1));
arr.push_back(to_json(2));
arr.push_back(to_json(3));

// Access like std::vector
for (const auto& elem : arr)
{
    std::cout << from_json<int>(elem) << "\n";
}

// Convert from standard JsonArray
JsonArray standard = {to_json(10), to_json(20)};
FatPJsonArray optimized = from_json_array(standard);
```

**When to use:**
- Small JSON arrays (≤8 elements)
- Performance-critical paths
- Minimizing allocations
- Arrays created and destroyed frequently

**When to use standard JsonArray:**
- Large arrays (>8 elements) - no benefit from inline storage
- Interfacing with JsonLite code
- Memory usage more important than speed

### FatPJsonObject (FlatMap)

`FatPJsonObject` uses FlatMap for object storage:

```cpp
template<typename Compare = std::less<std::string>, 
         typename Allocator = std::allocator<std::pair<const std::string, JsonValue>>>
using FatPJsonObject = FlatMap<std::string, JsonValue, Compare, Allocator>;
```

**Benefits:**
- Contiguous storage (better cache locality)
- Faster iteration than std::map
- Lower memory overhead per element
- Performance varies by use case (see benchmarks)

**Usage:**

```cpp
// Create optimized object
FatPJsonObject<> obj;
obj["name"] = to_json(std::string("Alice"));
obj["age"] = to_json(30);
obj["active"] = to_json(true);

// Access by key
if (obj.find("name") != obj.end())
{
    std::string name = from_json<std::string>(obj["name"]);
}

// Iterate
for (const auto& [key, value] : obj)
{
    std::cout << key << ": " << to_json_string(value) << "\n";
}

// Convert from standard JsonObject
JsonObject standard;
standard["key"] = to_json("value");
FatPJsonObject<> optimized = from_json_object(standard);
```

**When to use:**
- Iterating over all keys frequently
- Many small objects
- Performance-critical paths

**When to use standard JsonObject:**
- Very large objects with frequent insertions
- Need guaranteed O(log n) lookup (std::map)
- Interfacing with JsonLite code

### Performance Characteristics

From the benchmark results (Intel Core i7-8850H @ 2.60GHz):

**Parsing Performance:**
- Small JSON (<1KB): 1.04x slower than JsonLite (1.376µs vs 1.322µs)
- Large arrays (10K elements): 1.06x slower than JsonLite (1.939ms vs 1.829ms)

**Data Structure Performance:**
- **Array operations**: 13.54x faster with FatPJsonArray vs JsonArray
  - JsonArray (std::vector): 613.2 ns
  - FatPJsonArray (SmallVector): 45.3 ns
  
- **Object operations**: 0.62x slower with FatPJsonObject vs JsonObject for this test
  - JsonObject (std::map): 1.178 µs
  - FatPJsonObject (FlatMap): 1.894 µs
  - Note: FlatMap is faster for iteration but slower for individual insertions

**Error Handling:**
- Expected vs exceptions: 1.1x faster on error path (7.675µs vs 8.153µs)

**Memory:**
- StringPool savings: 99.9% for repeated keys (16KB → 16 bytes for 1000 objects)
- Memory-mapped I/O: 1.32x faster than regular I/O (8.924ms vs 11.746ms)

**Recommendations:**
- Use FatPJsonArray for small arrays (≤8 elements) - massive speedup
- Use FatPJsonObject when iteration is more common than insertion
- Use PooledJsonObject for large datasets with repeated keys
- Use memory-mapped I/O for files >10MB

---

## String Pool and Memory Optimization

### Understanding StringPool

StringPool provides string deduplication for JSON objects with repeated keys:

```cpp
// Without pool: each object has its own "name", "age", "city" strings
// With pool: one shared copy of each string, referenced by all objects
```

**Benefits:**
- 30-50% memory savings for large datasets
- 99.9% savings for repeated keys (from benchmark: 16KB → 16 bytes)
- Faster string comparisons (pointer equality)
- Cache-friendly memory layout

### PooledJsonObject Usage

```cpp
#include "StringPool.h"

// Create a string pool
StringPool pool;

// Create pooled object
PooledJsonObject pooled_obj(pool);
pooled_obj.insert("name", to_json(std::string("Alice")));
pooled_obj.insert("age", to_json(30));

// "name" and "age" strings are deduplicated in the pool

// Create another object with same keys
PooledJsonObject pooled_obj2(pool);
pooled_obj2.insert("name", to_json(std::string("Bob")));  // "name" reused
pooled_obj2.insert("age", to_json(25));                   // "age" reused

// Convert to standard JsonObject if needed
JsonObject standard_obj = pooled_obj.to_json_object();

// Convert from standard JsonObject
JsonObject input;
input["city"] = to_json(std::string("New York"));
PooledJsonObject pooled_from_standard(pool);
pooled_from_standard.from_json_object(input);

// Clear pool when done
pool.clear();  // Removes all strings
```

### Memory Savings Analysis

From benchmark (1000 objects with same keys):

**Without StringPool:**
- Each object stores its own key strings
- "name", "age", "city" repeated 1000 times
- Total memory: ~16,000 bytes

**With StringPool:**
- Keys stored once in pool, referenced by objects
- "name", "age", "city" stored once
- Total memory: ~16 bytes
- **Savings: 99.9%**

**Real-world example:**

```cpp
// Processing 10,000 user records
std::vector<JsonObject> users_standard;
std::vector<PooledJsonObject> users_pooled;

StringPool pool;

for (int i = 0; i < 10000; ++i)
{
    // Standard: allocates "name", "email", "age" 10,000 times
    JsonObject user_std;
    user_std["name"] = to_json("User" + std::to_string(i));
    user_std["email"] = to_json("user" + std::to_string(i) + "@example.com");
    user_std["age"] = to_json(20 + i % 50);
    users_standard.push_back(user_std);
    
    // Pooled: shares "name", "email", "age" across all objects
    PooledJsonObject user_pooled(pool);
    user_pooled.insert("name", to_json("User" + std::to_string(i)));
    user_pooled.insert("email", to_json("user" + std::to_string(i) + "@example.com"));
    user_pooled.insert("age", to_json(20 + i % 50));
    users_pooled.push_back(user_pooled);
}

// Memory savings: ~300 KB with string pool
```

### When to Use String Pools

**✓ Use StringPool when:**
- Processing large JSON arrays with uniform structure
- Many objects with same key names
- Memory is limited
- Keys are repeated across many objects

**✗ Don't use StringPool when:**
- Objects have unique keys
- Processing small JSON documents
- Short-lived objects
- Memory is abundant

**Thread safety:**
- StringPool is thread-safe by default (uses thread-local storage)
- Multiple threads can use the same pool
- No explicit synchronization needed

---

## File I/O Operations

### Standard File Operations

**try_load_json()** - Load and parse file:

```cpp
auto result = try_load_json("config.json");
if (!result)
{
    std::cerr << "Failed: " << result.error().message << "\n";
    return;
}

JsonValue config = *result;
```

**try_save_json()** - Save to file:

```cpp
JsonValue data = to_json(my_struct);

// Compact format
auto result = try_save_json("data.json", data);

// Pretty format
auto result2 = try_save_json<PrettyJsonPolicy>("data.json", data);

if (!result2)
{
    std::cerr << "Save failed: " << result2.error().message << "\n";
}
```

### Atomic Save Operations

**try_save_atomic()** - Crash-safe file saving:

FatPJsonLite provides atomic save operations that protect against data loss during crashes:

```cpp
JsonValue config = to_json(my_config);

// Atomic save (crash-safe)
auto result = try_save_atomic("config.json", config, true);  // true = pretty print

if (!result)
{
    std::cerr << "Atomic save failed: " << result.error().message << "\n";
}
```

**How it works:**
1. Write data to temporary file (`config.json.tmp`)
2. Flush all buffers to disk
3. Atomically rename temp file to target (platform-specific syscall)
4. If crash occurs during step 1-2, original file is unchanged
5. If crash occurs during step 3, filesystem guarantees atomicity

**Benefits over try_save_json():**
- ✅ Original file never partially written
- ✅ Either old file or new file exists (never corrupted)
- ✅ Safe for critical data (user preferences, save games, etc.)
- ✅ Works even if process crashes or machine loses power

**Performance:**
- Slightly slower than try_save_json() due to additional fsync()
- Difference negligible for small files (<1MB)
- Worth the cost for any data that shouldn't be lost

**Comparison with save_params_with_backup():**

| Feature | save_params_with_backup() | try_save_atomic() |
|---------|---------------------------|-------------------|
| **Safety** | Backup preserved on crash | Atomic rename |
| **Performance** | Extra copy operation | Extra fsync |
| **Old data** | Kept in .bak file | Lost after rename |
| **Atomicity** | No (partial writes possible) | Yes (rename is atomic) |
| **Best for** | Versioned backups | Crash-safety |

**Example:**

```cpp
struct SaveGame
{
    int level;
    int health;
    std::vector<std::string> inventory;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(SaveGame, level, health, inventory)

void save_game(const SaveGame& game)
{
    JsonValue j = to_json(game);
    
    // Atomic save with pretty printing
    auto result = try_save_atomic<PrettyJsonPolicy>("save.json", j);
    
    if (!result)
    {
        show_error_dialog("Failed to save game: " + result.error().message);
    }
    else
    {
        show_message("Game saved successfully");
    }
}
```

### Memory-Mapped File I/O

**load_json_mmap()** - Load large files efficiently:

For files >10MB, memory-mapped I/O provides significant performance benefits:

```cpp
// Regular I/O
auto result1 = try_load_json("large_data.json");  // 11.746ms

// Memory-mapped I/O
auto result2 = load_json_mmap("large_data.json");  // 8.924ms (1.32x faster)

if (result2)
{
    JsonValue data = *result2;
    // Process data...
}
```

**Benefits:**
- 1.32x faster than regular I/O (from benchmark)
- Operating system manages memory
- Efficient for very large files (>100MB)
- Lazy loading (only accessed pages loaded)

**Limitations:**
- Only for reading (no mmap for writing)
- Requires enough virtual address space
- Not beneficial for small files (<10MB)

### When to Use Memory Mapping

**✓ Use load_json_mmap() when:**
- File size >10MB
- Processing very large datasets
- Limited RAM but large virtual address space
- Random access to large file

**✗ Use try_load_json() when:**
- File size <10MB
- Sequential access
- Need to modify file
- Portability concerns

**Example:**

```cpp
auto load_large_dataset(const std::string& filename) -> Expected<Dataset, JsonError>
{
    // Check file size
    std::filesystem::path p(filename);
    auto size = std::filesystem::file_size(p);
    
    // Use memory mapping for large files
    if (size > 10'000'000)  // 10MB
    {
        return load_json_mmap(filename)
            .and_then([](JsonValue v) { return safe_from_json<Dataset>(v); });
    }
    else
    {
        return try_load_json(filename)
            .and_then([](JsonValue v) { return safe_from_json<Dataset>(v); });
    }
}
```

---

## JSON Pointer with Expected

FatPJsonLite provides exception-free JSON Pointer operations using Expected for error handling. 
This allows RFC 6901 compliant navigation without exceptions.

### Exception-Free Navigation

**try_query_json_pointer()** - Navigate without exceptions:

```cpp
JsonValue config = *try_parse_json(R"({
    "database": {
        "host": "localhost",
        "port": 5432,
        "credentials": {
            "username": "admin"
        }
    },
    "servers": ["web1", "web2"]
})");

// Navigate to nested value
auto host_result = try_query_json_pointer(config, "/database/host");
if (!host_result)
{
    std::cerr << "Navigation failed: " << host_result.error().message << "\n";
    return;
}

const JsonValue* host_ptr = *host_result;
auto host = safe_from_json<std::string>(*host_ptr);
if (host)
{
    std::cout << "Host: " << *host << "\n";
}

// Array access
auto server_result = try_query_json_pointer(config, "/servers/0");
if (server_result)
{
    auto server = safe_from_json<std::string>(**server_result);
    std::cout << "Server: " << *server << "\n";
}
```

**Error handling:**

```cpp
// Key not found
auto bad_key = try_query_json_pointer(config, "/database/nonexistent");
if (!bad_key)
{
    std::cout << "Error: " << bad_key.error().message << "\n";
    std::cout << "Code: " << bad_key.error().code << "\n";
    // Error: JSON Pointer query failed
    // Code: TypeError
}

// Array index out of bounds
auto bad_index = try_query_json_pointer(config, "/servers/10");
if (!bad_index)
{
    std::cout << "Error: " << bad_index.error().message << "\n";
}

// Invalid pointer format
auto bad_format = try_query_json_pointer(config, "database/host");  // Missing /
if (!bad_format)
{
    std::cout << "Error: " << bad_format.error().message << "\n";
}
```

### Type-Safe Queries

**try_query_json_as<T>()** - Navigate and convert in one call:

```cpp
JsonValue config = *try_parse_json(R"({
    "server": {
        "port": 8080,
        "host": "localhost",
        "timeout": 30
    },
    "features": {
        "logging": true
    }
})");

// Extract primitives with type safety
auto port = try_query_json_as<int>(config, "/server/port");
if (port)
{
    std::cout << "Port: " << *port << "\n";
}

auto host = try_query_json_as<std::string>(config, "/server/host");
auto logging = try_query_json_as<bool>(config, "/features/logging");

// Extract optionals
auto timeout = try_query_json_as<std::optional<int>>(config, "/server/timeout");
if (timeout && *timeout)
{
    std::cout << "Timeout: " << **timeout << " seconds\n";
}

// Extract arrays
JsonValue arr_doc = *try_parse_json(R"({"servers": ["web1", "web2"]})");
auto servers = try_query_json_as<std::vector<std::string>>(arr_doc, "/servers");
if (servers)
{
    for (const auto& s : *servers)
    {
        std::cout << "Server: " << s << "\n";
    }
}

// Extract custom structs
struct DatabaseConfig
{
    std::string host;
    int port;
    int timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(DatabaseConfig, host, port, timeout)

auto db_result = try_query_json_as<DatabaseConfig>(config, "/server");
if (db_result)
{
    std::cout << "DB: " << db_result->host << ":" << db_result->port << "\n";
}
```

### Mutable Access

Mutable pointer operations return `Expected<JsonValue*, JsonError>`:

```cpp
auto config_result = try_parse_json(R"({
    "server": {
        "port": 8080,
        "host": "localhost"
    }
})");

if (!config_result)
{
    return;
}

JsonValue config = *config_result;

// Get mutable pointer
auto port_ptr = try_query_json_pointer(config, "/server/port");
if (port_ptr)
{
    **port_ptr = to_json(9000);  // Change port
}

auto host_ptr = try_query_json_pointer(config, "/server/host");
if (host_ptr)
{
    **host_ptr = to_json(std::string("0.0.0.0"));  // Change host
}

// Save modified config
auto save_result = try_save_atomic<PrettyJsonPolicy>("config.json", config);
```

### Error Handling

JSON Pointer operations can fail for various reasons:

```cpp
JsonValue doc = *try_parse_json(R"({
    "array": [1, 2, 3],
    "object": {"key": "value"}
})");

// Test various error conditions
struct Test
{
    std::string description;
    std::string pointer;
    JsonErrorCode expected_code;
};

std::vector<Test> tests = {
    {"Invalid format", "invalid", JsonErrorCode::TypeError},
    {"Key not found", "/nonexistent", JsonErrorCode::TypeError},
    {"Array out of bounds", "/array/10", JsonErrorCode::TypeError},
    {"Type mismatch", "/object/0", JsonErrorCode::TypeError},
};

for (const auto& test : tests)
{
    auto result = try_query_json_pointer(doc, test.pointer);
    
    if (!result)
    {
        std::cout << test.description << ":\n";
        std::cout << "  Error: " << result.error().message << "\n";
        std::cout << "  Code: " << result.error().code << "\n";
        
        if (result.error().code == test.expected_code)
        {
            std::cout << "  ✓ Correct error code\n";
        }
    }
}
```

**Composable error handling:**

```cpp
auto load_and_extract(const std::string& filename, const std::string& pointer)
    -> Expected<int, JsonError>
{
    return try_load_json(filename)
        .and_then([pointer](JsonValue v) {
            return try_query_json_as<int>(v, pointer);
        });
}

// Usage
auto port = load_and_extract("config.json", "/database/port");
if (port)
{
    std::cout << "Port: " << *port << "\n";
}
else
{
    std::cerr << "Failed: " << port.error().message << "\n";
}
```

### Comparison with JsonLite

| Feature | JsonLite | FatPJsonLite |
|---------|----------|--------------|
| **Navigation** | `query_json_pointer()` | `try_query_json_pointer()` |
| **Type-safe** | `query_json_as<T>()` | `try_query_json_as<T>()` |
| **Error handling** | Exceptions | Expected<T, JsonError> |
| **Error info** | std::runtime_error message | JsonError with code + context |
| **Composable** | try-catch blocks | Monadic operations |
| **Performance** | Slightly faster (no Expected) | Minimal overhead |

**When to use FatPJsonLite JSON Pointer:**
- Exception-free environments
- Need composable error handling
- Want structured error codes
- Building error-handling chains

**When to use JsonLite JSON Pointer:**
- Exceptions are acceptable
- Simpler error handling sufficient
- Interfacing with exception-based code

---

## Advanced Features

### Batch Parsing

Parse multiple JSON strings in one call:

```cpp
std::vector<std::string> json_strings = {
    R"({"name": "Alice", "age": 30})",
    R"({"name": "Bob", "age": 25})",
    R"({"name": "Charlie", "age": 35})"
};

// Fail-fast mode: stop at first error
auto results = batch_parse_json(json_strings, true);

if (results)
{
    for (const auto& value : *results)
    {
        auto person = safe_from_json<Person>(value);
        // Process person...
    }
}
else
{
    for (const auto& error : results.error())
    {
        std::cerr << "Parse error: " << error.message << "\n";
    }
}

// Collect all errors mode
auto results2 = batch_parse_json(json_strings, false);
if (!results2)
{
    std::cout << "Failed to parse " << results2.error().size() << " documents\n";
    for (size_t i = 0; i < results2.error().size(); ++i)
    {
        std::cout << "Document " << i << ": " << results2.error()[i].message << "\n";
    }
}
```

**Use cases:**
- Processing multiple API responses
- Batch import of JSON data
- Testing with multiple test cases

### Numeric Overflow Detection

FatPJsonLite uses CheckedArithmetic for enhanced numeric safety:

```cpp
// All numeric conversions check for overflow
JsonValue j_big = to_json(999999999);

auto int8_result = safe_from_json_numeric<int8_t>(j_big);
if (!int8_result)
{
    std::cout << "Overflow detected: " << int8_result.error().message << "\n";
    // Numeric value 999999999 out of range for target type (max: 127)
}

// Fractional part detection
JsonValue j_float = to_json(3.7);
auto int_result = safe_from_json_numeric<int>(j_float);
if (!int_result)
{
    std::cout << "Has fraction: " << int_result.error().message << "\n";
    // Cannot convert double with fractional part to integer type
}

// Negative to unsigned
JsonValue j_neg = to_json(-42);
auto unsigned_result = safe_from_json_numeric<unsigned int>(j_neg);
if (!unsigned_result)
{
    std::cout << "Negative value: " << unsigned_result.error().message << "\n";
    // Cannot convert negative value to unsigned type
}
```

**No artificial margins:**
- Full int64_t range supported (no reduction for safety margins)
- Full double range supported
- Exact boundary checking at type limits

### Thread Safety Considerations

**StringPool:**
- Thread-safe by default (uses thread-local storage)
- Each thread has its own pool
- No explicit synchronization needed

**Expected:**
- Thread-safe (immutable after construction)
- Can be safely passed between threads
- No shared state

**JsonValue:**
- Not thread-safe for mutation
- Read-only access is safe
- Use mutex for concurrent writes

**Best practices:**

```cpp
// Safe: each thread has own StringPool
void worker_thread(const std::vector<std::string>& json_strings)
{
    StringPool pool;  // Thread-local
    
    for (const auto& json_str : json_strings)
    {
        auto result = try_parse_json(json_str);
        if (result)
        {
            // Process with pooled objects
            PooledJsonObject obj(pool);
            // ...
        }
    }
}

// Safe: read-only access
void parallel_search(const JsonValue& data, const std::string& key)
{
    #pragma omp parallel for
    for (int i = 0; i < 1000; ++i)
    {
        auto result = try_query_json_as<std::string>(data, "/items/" + 
                                                      std::to_string(i) + "/" + key);
        if (result)
        {
            process(*result);
        }
    }
}

// Unsafe without mutex: concurrent writes
JsonObject shared_config;  // Needs mutex!

void update_config(const std::string& key, const JsonValue& value)
{
    std::lock_guard<std::mutex> lock(config_mutex);
    shared_config[key] = value;
}
```

---

## Performance Benchmarks

### Parsing Performance

Measured on Intel Core i7-8850H @ 2.60GHz, 32GB RAM:

**Small JSON (<1KB):**
- JsonLite: 1.322 µs
- FatPJsonLite: 1.376 µs
- Ratio: 1.04x (4% slower due to Expected overhead)

**Large Array (10,000 elements):**
- JsonLite: 1.829 ms
- FatPJsonLite: 1.939 ms
- Ratio: 1.06x (6% slower due to Expected overhead)

**Conclusion**: FatPJsonLite has minimal parsing overhead (<10%) compared to JsonLite.

### Data Structure Performance

**Array Operations:**
- JsonArray (std::vector): 613.2 ns
- FatPJsonArray (SmallVector): 45.3 ns
- **Speedup: 13.54x faster** for small arrays

**Object Operations:**
- JsonObject (std::map): 1.178 µs
- FatPJsonObject (FlatMap): 1.894 µs
- Ratio: 0.62x (slower for this specific test)

**Note**: FlatMap performance varies by workload:
- Faster for iteration (contiguous storage)
- Slower for random insertions (must maintain sorted order)
- Better cache locality overall

**StringPool Memory Savings:**
- Standard memory (1000 objects): 16,000 bytes
- Pooled memory (unique keys): 16 bytes
- **Memory savings: 99.9%**

### Error Handling Performance

**Exception vs Expected (on error path):**
- Exception-based (JsonLite): 8.153 µs
- Expected-based (FatPJsonLite): 7.675 µs
- **Speedup: 1.06x faster** (6% faster on error path)

**Note**: On success path, difference is negligible. Expected shines on error paths where 
exceptions cause stack unwinding.

### Memory Usage

**Memory-Mapped I/O vs Regular I/O:**
- Regular I/O: 11.746 ms
- Memory-mapped: 8.924 ms
- **Speedup: 1.32x faster**

**Recommendations:**
- Use FatPJsonArray for arrays ≤8 elements (13x faster!)
- Use PooledJsonObject for large datasets with repeated keys (99.9% savings!)
- Use memory-mapped I/O for files >10MB (1.3x faster)
- Expected has minimal overhead (<10%) with better error handling

---

## Migration Guide

### From JsonLite to FatPJsonLite

**1. Change includes:**

```cpp
// Before (JsonLite)
#include "JsonLite.h"
USING_JSON_LITE()

// After (FatPJsonLite)
#include "FatPJsonLite.h"
USING_FATP_JSON_LITE()
```

**2. Update parsing:**

```cpp
// Before (exception-based)
try {
    JsonValue val = parse_json(json_str);
} catch (const std::runtime_error& e) {
    std::cerr << "Error: " << e.what() << "\n";
}

// After (Expected-based)
auto result = try_parse_json(json_str);
if (!result) {
    std::cerr << "Error: " << result.error().message << "\n";
}
JsonValue val = *result;
```

**3. Update conversions:**

```cpp
// Before
try {
    int port = from_json<int>(j["port"]);
} catch (const std::runtime_error& e) {
    // Handle error
}

// After
auto port = safe_from_json<int>(j["port"]);
if (!port) {
    // Handle error: port.error()
}
```

**4. Update file I/O:**

```cpp
// Before
try {
    JsonValue config = load_json_from_file("config.json");
} catch (const std::runtime_error& e) {
    // Handle error
}

// After
auto config = try_load_json("config.json");
if (!config) {
    // Handle error: config.error()
}
```

**5. Update JSON Pointer:**

```cpp
// Before
try {
    const JsonValue& val = query_json_pointer(doc, "/database/port");
    int port = from_json<int>(val);
} catch (const std::runtime_error& e) {
    // Handle error
}

// After
auto port = try_query_json_as<int>(doc, "/database/port");
if (!port) {
    // Handle error: port.error()
}
```

### From Exception-Based to Expected-Based

**Pattern 1: Early return on error**

```cpp
// Before
Config load_config() {
    try {
        JsonValue j = load_json_from_file("config.json");
        return from_json<Config>(j);
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return Config{};  // Default
    }
}

// After
Expected<Config, JsonError> load_config() {
    auto j = try_load_json("config.json");
    if (!j) {
        return unexpected(j.error());
    }
    
    return safe_from_json<Config>(*j);
}
```

**Pattern 2: Chaining with and_then()**

```cpp
// Before
Config load_and_validate() {
    try {
        JsonValue j = load_json_from_file("config.json");
        Config cfg = from_json<Config>(j);
        
        if (cfg.port < 1024) {
            throw std::runtime_error("Invalid port");
        }
        
        return cfg;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return Config{};
    }
}

// After
Expected<Config, JsonError> load_and_validate() {
    return try_load_json("config.json")
        .and_then([](JsonValue v) { return safe_from_json<Config>(v); })
        .and_then([](Config cfg) -> Expected<Config, JsonError> {
            if (cfg.port < 1024) {
                return unexpected(JsonError{JsonErrorCode::RangeError, 
                                           "Invalid port", 0, 
                                           "port=" + std::to_string(cfg.port)});
            }
            return cfg;
        });
}
```

### When to Stay with JsonLite

Stay with JsonLite if:

**1. Exceptions are acceptable** - Your application uses exceptions throughout
**2. Simplicity matters** - You don't need Expected-based error handling
**3. Small codebase** - Binary size matters more than features
**4. No exception-free requirement** - Not embedded or real-time system
**5. Minimal dependencies** - Don't want additional fat_p headers

Migrate to FatPJsonLite if:

**1. Exception-free code required** - Embedded systems, game engines, real-time
**2. Performance critical** - Need optimized data structures (SmallVector, FlatMap)
**3. Large datasets** - Benefit from StringPool and memory-mapped I/O
**4. Better error handling** - Want structured error codes and composable errors
**5. Atomic operations** - Need crash-safe file saves

---

## Comparison with JsonLite

### Feature Comparison

| Feature | JsonLite | FatPJsonLite |
|---------|----------|--------------|
| **C++ Version** | C++17 | C++17 |
| **Error Handling** | Exceptions | Expected<T, E> |
| **Data Structures** | std::vector/map | SmallVector/FlatMap |
| **Memory Optimization** | None | StringPool |
| **Large Files** | Standard I/O | Memory-mapped I/O |
| **Atomic Saves** | Backup | Atomic rename |
| **JSON Pointer** | Exception-based | Expected-based |
| **Enum Support** | Manual | EnumPlus integration |
| **Batch Parsing** | No | Yes |
| **Dependencies** | Zero | fat_p components |
| **Binary Size** | ~30-50KB | ~40-65KB (+20-30%) |
| **Parsing Speed** | Baseline | 1.04-1.06x slower |
| **Array Performance** | Baseline | 13.5x faster (small) |
| **Object Performance** | Baseline | 0.62-2x varies |
| **Error Path** | Baseline | 1.1x faster |
| **Thread Safety** | Manual | StringPool auto |

### Performance Comparison

From benchmarks (Intel Core i7-8850H @ 2.60GHz):

**Parsing:**
- Small JSON: FatPJsonLite 1.04x slower (4% overhead from Expected)
- Large JSON: FatPJsonLite 1.06x slower (6% overhead from Expected)

**Data Structures:**
- Small arrays: FatPJsonLite 13.54x faster (SmallVector inline storage)
- Objects: Varies by use case (FlatMap better for iteration, worse for insertion)

**Memory:**
- StringPool: 99.9% savings for repeated keys
- Memory-mapped I/O: 1.32x faster for large files

**Error Handling:**
- Error path: FatPJsonLite 1.1x faster (no exception unwinding)

### Use Case Matrix

| Use Case | JsonLite | FatPJsonLite |
|----------|----------|--------------|
| **Config files** | ✓ Excellent | ✓ Excellent |
| **Embedded systems** | ○ Possible | ✓ Better (no exceptions) |
| **Game engines** | ○ Acceptable | ✓ Preferred (exception-free) |
| **REST APIs** | ○ Moderate | ✓ Good (faster error paths) |
| **Large datasets** | ○ Limited | ✓ Excellent (StringPool + mmap) |
| **Real-time systems** | ✗ Not suitable | ○ Possible (exception-free) |
| **Minimal dependencies** | ✓ Best choice | ○ Requires fat_p |
| **Small codebase** | ✓ Smaller binary | ○ Larger binary |

**Legend:**
- ✓ Excellent fit
- ○ Acceptable, with caveats
- ✗ Not recommended

---

## Use Case Guide

### High-Performance Services

**✓ FatPJsonLite is excellent for high-performance services:**

```cpp
// REST API endpoint
Expected<Response, JsonError> handle_request(const std::string& json_body)
{
    return try_parse_json(json_body)
        .and_then([](JsonValue v) { 
            return safe_from_json<Request>(v); 
        })
        .and_then([](Request req) {
            return process_request(req);
        })
        .and_then([](Result res) {
            return to_response(res);
        });
}
```

**Benefits:**
- No exception overhead on error paths
- Composable error handling
- Fast small array operations
- Memory-efficient with StringPool

### Exception-Free Environments

**✓ FatPJsonLite works in `-fno-exceptions` environments:**

```cpp
// Compile with -fno-exceptions
Expected<Config, JsonError> load_config()
{
    // No exceptions thrown, ever
    return try_load_json("config.json")
        .and_then([](JsonValue v) { return safe_from_json<Config>(v); });
}

void main_loop()
{
    auto config = load_config();
    if (!config)
    {
        // Handle error without exceptions
        log_error(config.error());
        use_defaults();
    }
    else
    {
        run_with_config(*config);
    }
}
```

### Large Dataset Processing

**✓ FatPJsonLite excels with large datasets:**

```cpp
// Process 1000+ JSON objects with repeated keys
Expected<std::vector<Record>, JsonError> process_large_dataset(
    const std::string& filename)
{
    // Memory-mapped I/O for large file
    auto json = load_json_mmap(filename);
    if (!json) {
        return unexpected(json.error());
    }
    
    // Use StringPool for memory savings
    StringPool pool;
    std::vector<Record> records;
    
    auto arr = safe_from_json<std::vector<JsonValue>>(*json);
    if (!arr) {
        return unexpected(arr.error());
    }
    
    for (const auto& item : *arr)
    {
        // PooledJsonObject deduplicates keys
        PooledJsonObject obj(pool);
        // ... process with memory savings
    }
    
    return records;
}
```

**Benefits:**
- Memory-mapped I/O (1.32x faster)
- StringPool (99.9% memory savings)
- Exception-free error handling

### Real-Time Systems

**○ FatPJsonLite is acceptable for real-time with caveats:**

```cpp
// Real-time system with predictable performance
Expected<SensorData, JsonError> parse_sensor_data(const char* json_str)
{
    // No exceptions = no unpredictable stack unwinding
    auto result = try_parse_json(std::string_view(json_str));
    if (!result) {
        // Error path is fast, predictable
        return unexpected(result.error());
    }
    
    return safe_from_json<SensorData>(*result);
}
```

**Caveats:**
- Still uses dynamic allocation (std::string, std::vector)
- Parsing is not constant-time
- Not hard real-time suitable

**Better for soft real-time:**
- Game engines
- Audio processing (non-critical paths)
- Control systems with relaxed timing

### Memory-Constrained Environments

**✓ FatPJsonLite helps in memory-constrained environments:**

```cpp
// Embedded system with limited RAM
Expected<Config, JsonError> load_minimal_memory(const std::string& filename)
{
    // SmallVector avoids heap for small arrays
    FatPJsonArray arr;
    
    // StringPool deduplicates keys
    StringPool pool;
    
    auto json = try_load_json(filename);
    if (!json) {
        return unexpected(json.error());
    }
    
    // Process with minimal allocations
    PooledJsonObject config(pool);
    
    return safe_from_json<Config>(*json);
}
```

**Memory savings:**
- SmallVector: No heap for ≤8 elements
- StringPool: 99.9% savings for repeated keys
- FlatMap: Lower per-element overhead than std::map

---

## Best Practices

### Error Handling Patterns

**1. Use monadic chaining for complex operations:**

```cpp
auto load_and_process() -> Expected<Result, JsonError>
{
    return try_load_json("data.json")
        .and_then([](JsonValue v) { return safe_from_json<Input>(v); })
        .and_then([](Input in) { return validate(in); })
        .and_then([](Input in) { return process(in); });
}
```

**2. Provide meaningful defaults with or_else():**

```cpp
Config get_config()
{
    return try_load_json("config.json")
        .and_then([](JsonValue v) { return safe_from_json<Config>(v); })
        .or_else([](JsonError e) {
            log_warning("Using defaults", e);
            return Config{};  // Safe default
        })
        .value();  // Guaranteed to have value
}
```

**3. Log errors with context:**

```cpp
void log_error(const JsonError& err)
{
    std::cerr << "Error: " << err.to_string() << "\n";
    std::cerr << "  Code: " << err.code << "\n";
    if (err.position > 0)
    {
        std::cerr << "  Position: " << err.position << "\n";
    }
    if (!err.context.empty())
    {
        std::cerr << "  Context: " << err.context << "\n";
    }
}
```

### Memory Management

**1. Reuse StringPool for related operations:**

```cpp
void process_batch(const std::vector<std::string>& json_strings)
{
    StringPool pool;  // Shared across all objects
    
    for (const auto& json_str : json_strings)
    {
        auto result = try_parse_json(json_str);
        if (result)
        {
            PooledJsonObject obj(pool);  // Reuses pool
            // Keys deduplicated across all objects
        }
    }
    
    // Clear pool when done
    pool.clear();
}
```

**2. Use FatPJsonArray for small arrays:**

```cpp
// Instead of JsonArray
JsonArray standard = {to_json(1), to_json(2), to_json(3)};  // 3 heap allocations

// Use FatPJsonArray
FatPJsonArray optimized;  // 0 heap allocations for ≤8 elements
optimized.push_back(to_json(1));
optimized.push_back(to_json(2));
optimized.push_back(to_json(3));
```

**3. Memory-map large files:**

```cpp
auto load_file(const std::string& filename) -> Expected<JsonValue, JsonError>
{
    auto size = std::filesystem::file_size(filename);
    
    if (size > 10'000'000)  // 10MB
    {
        return load_json_mmap(filename);  // Memory-mapped
    }
    else
    {
        return try_load_json(filename);  // Regular I/O
    }
}
```

### Performance Optimization

**1. Profile before optimizing:**

```cpp
// Measure before deciding on optimizations
auto start = std::chrono::high_resolution_clock::now();

auto result = try_parse_json(large_json);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

if (duration.count() > 100)  // >100ms
{
    // Consider memory-mapped I/O or streaming
}
```

**2. Use optimized structures for hot paths:**

```cpp
// Hot path: called millions of times
void process_small_array(const FatPJsonArray& arr)  // Not JsonArray
{
    for (const auto& elem : arr)  // 13x faster for small arrays
    {
        process_element(elem);
    }
}
```

**3. Atomic saves for critical data only:**

```cpp
// Critical data: use atomic save
void save_user_progress(const SaveData& data)
{
    try_save_atomic<PrettyJsonPolicy>("save.json", to_json(data));
}

// Non-critical data: use regular save
void save_cache(const CacheData& data)
{
    try_save_json("cache.json", to_json(data));  // Faster
}
```

### Testing Strategies

**1. Test error paths explicitly:**

```cpp
void test_error_handling()
{
    // Test parse error
    auto bad_json = try_parse_json("invalid json");
    ASSERT_FALSE(bad_json.has_value());
    ASSERT_EQ(bad_json.error().code, JsonErrorCode::ParseError);
    
    // Test type error
    JsonValue j = to_json(42);
    auto str_result = safe_from_json<std::string>(j);
    ASSERT_FALSE(str_result.has_value());
    ASSERT_EQ(str_result.error().code, JsonErrorCode::TypeError);
    
    // Test overflow
    JsonValue big = to_json(INT64_MAX);
    auto small_result = safe_from_json_numeric<int8_t>(big);
    ASSERT_FALSE(small_result.has_value());
    ASSERT_EQ(small_result.error().code, JsonErrorCode::NumericOverflow);
}
```

**2. Test round-trip with Expected:**

```cpp
void test_round_trip()
{
    Config original{8080, "localhost"};
    
    JsonValue j = to_json(original);
    std::string json_str = to_json_string(j);
    
    auto parsed = try_parse_json(json_str);
    ASSERT_TRUE(parsed.has_value());
    
    auto restored = safe_from_json<Config>(*parsed);
    ASSERT_TRUE(restored.has_value());
    
    ASSERT_EQ(original.port, restored->port);
    ASSERT_EQ(original.host, restored->host);
}
```

**3. Test enum serialization:**

```cpp
void test_enum_round_trip()
{
    for (auto p : {Priority::Low, Priority::Medium, Priority::High})
    {
        JsonValue j = to_json(p);
        auto restored = safe_from_json_enum<Priority>(j);
        
        ASSERT_TRUE(restored.has_value());
        ASSERT_EQ(p, *restored);
    }
}
```

---

## API Reference

### Parsing Functions

**try_parse_json<Policy>(json_string)**

```cpp
template <typename Policy = StandardJsonPolicy>
Expected<JsonValue, JsonError> try_parse_json(std::string_view json) noexcept;
```

Parse JSON string without throwing exceptions.

- **Parameters:** JSON string to parse
- **Returns:** `Expected<JsonValue, JsonError>`
- **Errors:** `ParseError`, `ExtraData`, `DepthExceeded`, `InvalidUtf8`
- **Policies:** StandardJsonPolicy, PrettyJsonPolicy, CompatJsonPolicy, ConfigJsonPolicy

**Example:**
```cpp
auto result = try_parse_json(R"({"key": "value"})");
if (result)
{
    JsonValue val = *result;
}
else
{
    std::cerr << "Error: " << result.error().message << "\n";
}
```

---

### File I/O Functions

**try_load_json(filename)**

```cpp
Expected<JsonValue, JsonError> try_load_json(const std::string& filename) noexcept;
```

Load and parse JSON from file.

- **Parameters:** Filename path
- **Returns:** `Expected<JsonValue, JsonError>`
- **Errors:** `FileError`, `ParseError`

**Example:**
```cpp
auto result = try_load_json("config.json");
if (!result)
{
    std::cerr << "Error: " << result.error().message << "\n";
}
```

---

**try_save_json<Policy>(filename, value)**

```cpp
template <typename Policy = StandardJsonPolicy>
Expected<void, JsonError> try_save_json(
    const std::string& filename, 
    const JsonValue& value) noexcept;
```

Save JSON to file.

- **Parameters:** Filename path, JsonValue to save
- **Returns:** `Expected<void, JsonError>`
- **Errors:** `FileError`
- **Policies:** StandardJsonPolicy, PrettyJsonPolicy

**Example:**
```cpp
auto result = try_save_json<PrettyJsonPolicy>("output.json", json_value);
if (!result)
{
    std::cerr << "Save failed: " << result.error().message << "\n";
}
```

---

**try_save_atomic<Policy>(filename, value, pretty)**

```cpp
template<typename Policy = StandardJsonPolicy>
Expected<void, JsonError> try_save_atomic(
    const std::string& filename, 
    const JsonValue& value,
    bool pretty_print = false) noexcept;
```

Atomically save JSON to file using temp file and rename.

- **Parameters:** Filename path, JsonValue to save, pretty print flag
- **Returns:** `Expected<void, JsonError>`
- **Errors:** `FileError`
- **Safety:** Crash-safe, uses atomic rename

**Example:**
```cpp
auto result = try_save_atomic<PrettyJsonPolicy>("config.json", config_json);
if (!result)
{
    std::cerr << "Atomic save failed: " << result.error().message << "\n";
}
```

---

**load_json_mmap(filename)**

```cpp
Expected<JsonValue, JsonError> load_json_mmap(const std::string& filename) noexcept;
```

Load large JSON file using memory-mapped I/O.

- **Parameters:** Filename path
- **Returns:** `Expected<JsonValue, JsonError>`
- **Errors:** `FileError`, `ParseError`, `MemoryError`
- **Recommended for:** Files >10 MB

**Example:**
```cpp
auto result = load_json_mmap("large_data.json");
if (result)
{
    // 1.32x faster than regular I/O
}
```

---

### Conversion Functions

**safe_from_json_numeric<T>(value)**

```cpp
template<typename T>
Expected<T, JsonError> safe_from_json_numeric(const JsonValue& j) noexcept;
```

Convert JsonValue to numeric type with overflow detection.

- **Parameters:** JsonValue to convert
- **Returns:** `Expected<T, JsonError>`
- **Errors:** `TypeError`, `NumericOverflow`
- **Supported types:** All integral and floating-point types

**Example:**
```cpp
auto int_val = safe_from_json_numeric<int>(json_value);
if (int_val)
{
    std::cout << "Value: " << *int_val << "\n";
}
else
{
    std::cerr << "Error: " << int_val.error().message << "\n";
}
```

---

**safe_from_json<T>(value)**

```cpp
template<typename T>
Expected<T, JsonError> safe_from_json(const JsonValue& j) noexcept;
```

Generic safe deserialization with Expected return.

- **Parameters:** JsonValue to convert
- **Returns:** `Expected<T, JsonError>`
- **Errors:** `TypeError`, `MissingField`, `NumericOverflow`

**Example:**
```cpp
auto str_val = safe_from_json<std::string>(json_value);
auto person = safe_from_json<Person>(json_value);
```

---

**safe_from_json_enum<E>(value)**

```cpp
template<typename E>
Expected<E, JsonError> safe_from_json_enum(const JsonValue& j) noexcept;
```

Convert JSON string to enum value with error handling.

- **Parameters:** JsonValue (must be string)
- **Returns:** `Expected<E, JsonError>`
- **Errors:** `TypeError`, `RangeError`
- **Requires:** Enum defined with `DEFINE_ENUM_WITH_STRING_CONVERSIONS`

**Example:**
```cpp
auto priority = safe_from_json_enum<Priority>(json_value);
if (priority)
{
    std::cout << "Priority: " << to_string(*priority) << "\n";
}
```

---

**try_query_json_pointer(root, pointer)**

```cpp
Expected<const JsonValue*, JsonError> try_query_json_pointer(
    const JsonValue& root, 
    std::string_view pointer) noexcept;

Expected<JsonValue*, JsonError> try_query_json_pointer(
    JsonValue& root, 
    std::string_view pointer) noexcept;
```

Exception-free JSON Pointer navigation (RFC 6901).

- **Parameters:** Root value, JSON Pointer path
- **Returns:** `Expected<JsonValue*, JsonError>` (pointer to value)
- **Errors:** `TypeError` (invalid path, not found, type mismatch)

**Example:**
```cpp
auto port_ptr = try_query_json_pointer(config, "/database/port");
if (port_ptr)
{
    auto port = safe_from_json<int>(**port_ptr);
}
```

---

**try_query_json_as<T>(root, pointer)**

```cpp
template<typename T>
Expected<T, JsonError> try_query_json_as(
    const JsonValue& root, 
    std::string_view pointer) noexcept;
```

Type-safe JSON Pointer query with automatic conversion.

- **Parameters:** Root value, JSON Pointer path
- **Returns:** `Expected<T, JsonError>`
- **Errors:** `TypeError` (navigation or conversion failure)

**Example:**
```cpp
auto port = try_query_json_as<int>(config, "/database/port");
if (port)
{
    std::cout << "Port: " << *port << "\n";
}
```

---

### Utility Functions

**batch_parse_json(json_strings, fail_fast)**

```cpp
Expected<std::vector<JsonValue>, std::vector<JsonError>> 
batch_parse_json(
    const std::vector<std::string>& json_strings, 
    bool fail_fast = false) noexcept;
```

Parse multiple JSON strings in one call.

- **Parameters:** Vector of JSON strings, fail-fast flag
- **Returns:** `Expected<vector<JsonValue>, vector<JsonError>>`
- **fail_fast:** Stop at first error if true, collect all errors if false

**Example:**
```cpp
std::vector<std::string> jsons = {R"({"a":1})", R"({"b":2})"};
auto results = batch_parse_json(jsons, false);
if (results)
{
    for (const auto& val : *results)
    {
        // Process each value
    }
}
```

---

**to_json_array(arr) / from_json_array(arr)**

```cpp
JsonArray to_json_array(const FatPJsonArray& arr);
FatPJsonArray from_json_array(const JsonArray& arr);
```

Convert between JsonArray and FatPJsonArray.

---

**to_json_object(obj) / from_json_object(obj)**

```cpp
JsonObject to_json_object(const FatPJsonObject<>& obj);
FatPJsonObject<> from_json_object(const JsonObject& obj);
```

Convert between JsonObject and FatPJsonObject.

---

## Compiler Requirements

### Minimum Version

FatPJsonLite requires C++17 or later.

### Required Dependencies

FatPJsonLite requires the following fat_p headers:

```
JsonLite.h          - Base JSON functionality
Expected.h          - Expected<T, E> type
FlatMap.h           - Optimized object storage
SmallVector.h       - Optimized array storage
StringPool.h        - String deduplication
CheckedArithmetic.h - Overflow detection
MemoryMappedFile.h  - Large file support
enforce.h           - Enhanced error messages
ConcurrencyPolicies.h - Thread safety policies
ScopeGuard.h        - RAII helpers
EnumPlus.h          - Enum string conversions
```

All headers are included when you include `FatPJsonLite.h`.

### Compilation Flags

**Required:**
```bash
-std=c++17
```

**Recommended for release:**
```bash
-std=c++17 -O3 -DNDEBUG -march=native
```

**Recommended for debug:**
```bash
-std=c++17 -g -fsanitize=address,undefined -Wall -Wextra
```

---

## Summary

FatPJsonLite is a **high-performance, exception-free JSON library** designed for production 
systems that demand both speed and safety.

**Key Characteristics:**
- ✓ Expected-based error handling (zero-overhead, composable)
- ✓ Optimized data structures (13.5x faster small arrays, memory-efficient objects)
- ✓ Memory efficiency (99.9% savings with StringPool for repeated keys)
- ✓ Large file support (memory-mapped I/O, 1.32x faster)
- ✓ Atomic saves (crash-safe file operations)
- ✓ Enhanced numeric safety (overflow detection, full range)
- ✓ JSON Pointer with Expected (exception-free RFC 6901)
- ✓ Enum serialization (first-class support via EnumPlus)
- ✓ Zero external dependencies (fat_p ecosystem only)

**Best For:**
- ✓ High-performance services (REST APIs, microservices)
- ✓ Exception-free environments (embedded, real-time, game engines)
- ✓ Large dataset processing (>10 MB files)
- ✓ Memory-constrained systems (IoT, mobile)
- ✓ Applications requiring predictable performance
- ✓ Critical data operations (atomic saves)

**Not Ideal For:**
- ✗ Simple applications where JsonLite suffices
- ✗ Teams unfamiliar with Expected-based error handling
- ✗ Minimizing binary size (FatPJsonLite is 20-30% larger)
- ✗ C++11 compatibility (requires C++17)

**Choose FatPJsonLite when:**
- Performance is critical
- Exceptions are unacceptable
- Large files need processing
- Memory efficiency matters
- Error paths are frequently exercised
- Need crash-safe file operations
- Want type-safe enum serialization

**Choose JsonLite when:**
- Simplicity is paramount
- Exception-based error handling is preferred
- Binary size is critical
- Performance is adequate
- Don't need fat_p dependencies

FatPJsonLite extends JsonLite with the power of the fat_p ecosystem, providing production-grade 
performance while maintaining the safety guarantees that make JsonLite reliable. It's the right 
choice when your application needs JSON handling that's both fast and safe, with modern error 
handling patterns and optimized data structures.
