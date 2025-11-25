# ConstexprUtilities User Manual

**Version:** 1.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Hashing](#hashing)
3. [Arithmetic Utilities](#arithmetic-utilities)
4. [String Conversion](#string-conversion)
5. [String Concatenation](#string-concatenation)
6. [Floating-Point Utilities](#floating-point-utilities)
7. [Use Cases](#use-cases)
8. [Best Practices](#best-practices)

---

## Overview

ConstexprUtilities provides compile-time utilities for hashing, arithmetic checks, and string operations that can be evaluated at compile time.

### Include

```cpp
#include "ConstexprUtilities.h"
using namespace fat_p;
```

### Key Features

- **Compile-time hashing**: FNV-1a hash (32/64-bit)
- **Power-of-two detection**: `is_power_of_two()`
- **Integer-to-string**: Compile-time conversion
- **String concatenation**: Zero-allocation concat
- **Floating-point conversion**: Runtime float-to-string

---

## Hashing

### constexpr_hash (32-bit)

FNV-1a hash for compile-time string hashing.

```cpp
// Compile-time hash
constexpr uint32_t hash = constexpr_hash("hello");
// hash = 0x4F9F2CAB

// Use in switch statements
void process(std::string_view cmd) {
    switch (constexpr_hash(cmd)) {
        case constexpr_hash("start"):
            start();
            break;
        case constexpr_hash("stop"):
            stop();
            break;
        case constexpr_hash("pause"):
            pause();
            break;
    }
}

// Template parameters
template <uint32_t Hash>
struct HashedString { };

using HelloHash = HashedString<constexpr_hash("hello")>;
```

### constexpr_hash64 (64-bit)

64-bit variant for reduced collision probability.

```cpp
constexpr uint64_t hash = constexpr_hash64("unique_identifier");

// Better for large hash tables
std::unordered_map<uint64_t, Handler> handlers;
handlers[constexpr_hash64("event_a")] = handle_a;
handlers[constexpr_hash64("event_b")] = handle_b;
```

### Hash Quality

FNV-1a properties:
- Good avalanche (1-bit change → ~50% output change)
- Fast computation
- Simple implementation
- **Not cryptographic** - don't use for security

---

## Arithmetic Utilities

### is_power_of_two

Checks if an integer is a power of two.

```cpp
static_assert(is_power_of_two(1));    // true (2^0)
static_assert(is_power_of_two(2));    // true (2^1)
static_assert(is_power_of_two(4));    // true (2^2)
static_assert(is_power_of_two(1024)); // true (2^10)

static_assert(!is_power_of_two(0));   // false
static_assert(!is_power_of_two(3));   // false
static_assert(!is_power_of_two(6));   // false
static_assert(!is_power_of_two(-4));  // false (negatives)
```

### Common Uses

```cpp
// Buffer size validation
template <size_t N>
class AlignedBuffer {
    static_assert(is_power_of_two(N), "Buffer size must be power of 2");
    // ...
};

// Alignment check
template <typename T, size_t Align>
T* aligned_alloc() {
    static_assert(is_power_of_two(Align), "Alignment must be power of 2");
    // ...
}

// Ring buffer index masking
template <size_t Capacity>
class RingBuffer {
    static_assert(is_power_of_two(Capacity), "Use power of 2 for fast modulo");
    
    size_t next_index(size_t i) {
        return (i + 1) & (Capacity - 1);  // Fast modulo
    }
};
```

---

## String Conversion

### constexpr_to_string_t

Compile-time integer-to-string conversion.

```cpp
// Create converter
constexpr constexpr_to_string_t<int> conv{42};
constexpr auto view = conv.view();  // "42"

// Negative numbers
constexpr constexpr_to_string_t<int> neg{-123};
constexpr auto neg_view = neg.view();  // "-123"

// Large numbers
constexpr constexpr_to_string_t<int64_t> big{9223372036854775807LL};
// Works with full int64_t range
```

### to_string_view

Runtime integer-to-string with buffer pooling.

```cpp
// Convert integer to string_view
std::string_view s1 = to_string_view(42);     // "42"
std::string_view s2 = to_string_view(-100);   // "-100"
std::string_view s3 = to_string_view(0);      // "0"

// Safe for multiple calls in same expression
std::cout << to_string_view(10) << " + " 
          << to_string_view(20) << " = " 
          << to_string_view(30);
// Uses rotating buffer pool (8 buffers)
```

### Thread Safety

`to_string_view` uses thread-local storage:
- ✅ Safe across threads
- ⚠️ Only 8 values per thread before buffer reuse

```cpp
// Safe: different threads
std::thread t1([]{
    auto s = to_string_view(1);  // Thread-local buffer
});

// Warning: Don't store more than 8 views
std::vector<std::string_view> views;
for (int i = 0; i < 100; ++i) {
    views.push_back(to_string_view(i));  // Earlier views invalidated!
}
```

---

## String Concatenation

### ConstexprString

Zero-allocation string view concatenation.

```cpp
// Concatenate multiple views
auto concat = constexpr_concat("Hello", " ", "World");
// concat holds views, no allocation yet

// Get total size
size_t len = concat.size();  // 11

// Convert to std::string (allocates)
std::string str = concat.to_string();  // "Hello World"

// Write to buffer (no allocation)
char buffer[64];
concat.to_array(buffer, sizeof(buffer));
// buffer = "Hello World\0"
```

### constexpr_concat

Variadic concatenation helper.

```cpp
// Multiple strings
auto msg = constexpr_concat(
    "Error at line ", 
    to_string_view(42), 
    ": ", 
    "unexpected token"
);

// Single allocation for final string
std::string error = msg.to_string();
// "Error at line 42: unexpected token"
```

### Use Cases

```cpp
// Efficient message building
void log(int code, std::string_view detail) {
    auto msg = constexpr_concat(
        "[", to_string_view(code), "] ", detail
    );
    write_log(msg.to_string());  // Single allocation
}

// Compile-time path building
constexpr auto path = constexpr_concat("/api/v", "2", "/users");
```

---

## Floating-Point Utilities

### constexpr_float_to_string_t

Float-to-string conversion with precision control.

```cpp
// Basic usage
constexpr_float_to_string_t<double> conv{3.14159, 2};
auto view = conv.view();  // "3.14"

// Different precisions
constexpr_float_to_string_t<double> p1{3.14159, 1};  // "3.1"
constexpr_float_to_string_t<double> p3{3.14159, 3};  // "3.141"
constexpr_float_to_string_t<double> p6{3.14159, 6};  // "3.141590"
```

### Special Values

```cpp
// NaN
constexpr_float_to_string_t<double> nan_conv{NAN, 2};
// view = "nan"

// Infinity
constexpr_float_to_string_t<double> inf_conv{INFINITY, 2};
// view = "inf"

// Negative infinity
constexpr_float_to_string_t<double> ninf_conv{-INFINITY, 2};
// view = "-inf"
```

---

## Use Cases

### Compile-Time String Switch

```cpp
enum class Command { Unknown, Start, Stop, Pause, Resume };

Command parse_command(std::string_view input) {
    switch (constexpr_hash(input)) {
        case constexpr_hash("start"):  return Command::Start;
        case constexpr_hash("stop"):   return Command::Stop;
        case constexpr_hash("pause"):  return Command::Pause;
        case constexpr_hash("resume"): return Command::Resume;
        default: return Command::Unknown;
    }
}
```

### Static Assertions

```cpp
template <size_t BufferSize, size_t Alignment>
class AlignedBuffer {
    static_assert(is_power_of_two(BufferSize), 
                  "Buffer size must be power of 2");
    static_assert(is_power_of_two(Alignment), 
                  "Alignment must be power of 2");
    static_assert(BufferSize >= Alignment,
                  "Buffer must be at least as large as alignment");
};
```

### Message Building

```cpp
Expected<int, std::string> parse(std::string_view input, int line) {
    if (input.empty()) {
        auto msg = constexpr_concat(
            "Parse error at line ",
            to_string_view(line),
            ": empty input"
        );
        return make_unexpected(msg.to_string());
    }
    // ...
}
```

### Hash-Based Dispatch

```cpp
using Handler = std::function<void()>;
std::unordered_map<uint64_t, Handler> dispatch_table;

void register_handler(std::string_view name, Handler h) {
    dispatch_table[constexpr_hash64(name)] = std::move(h);
}

void dispatch(std::string_view name) {
    auto it = dispatch_table.find(constexpr_hash64(name));
    if (it != dispatch_table.end()) {
        it->second();
    }
}
```

---

## Best Practices

### Do

```cpp
// ✅ Use constexpr_hash for string switches
switch (constexpr_hash(cmd)) { /* ... */ }

// ✅ Use is_power_of_two in static_assert
static_assert(is_power_of_two(BUFFER_SIZE));

// ✅ Use constexpr_concat to minimize allocations
auto msg = constexpr_concat(parts...).to_string();

// ✅ Use 64-bit hash for large tables
auto hash = constexpr_hash64(key);
```

### Don't

```cpp
// ❌ Don't use for cryptographic purposes
auto secure_hash = constexpr_hash(password);  // NOT SECURE!

// ❌ Don't store too many to_string_view results
for (int i = 0; i < 100; ++i) {
    stored_views.push_back(to_string_view(i));  // Buffer reuse!
}

// ❌ Don't assume hash uniqueness
// Always handle collisions in hash tables
```

---

## Performance

| Function | Complexity | Compile-time |
|----------|------------|--------------|
| constexpr_hash | O(n) | ✅ Yes |
| constexpr_hash64 | O(n) | ✅ Yes |
| is_power_of_two | O(1) | ✅ Yes |
| constexpr_to_string_t | O(digits) | ✅ Yes |
| to_string_view | O(digits) | ❌ Runtime |
| constexpr_concat | O(1) | ✅ Yes |
| ConstexprString::to_string | O(n) | ❌ Runtime |

---

## Related Components

- **Stringify.h**: Full-featured type-to-string conversion
- **CheckedArithmetic.h**: Safe arithmetic operations
- **EnumPlus.h**: Enum string conversion

---

**Document Version:** 1.0  
**Last Updated:** November 2025
