# DebugOnly User Manual

**Version:** 1.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17 (C++20 enhanced)  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [API Reference](#api-reference)
4. [C++20 Zero-Overhead](#cpp20-zero-overhead)
5. [Use Cases](#use-cases)
6. [Best Practices](#best-practices)

---

## Overview

`DebugOnly<T>` is an RAII wrapper that stores a value in debug builds but becomes an empty no-op in release builds, providing zero (or near-zero) overhead for debug-only data.

### Include

```cpp
#include "DebugOnly.h"
using namespace fat_p;
```

### Key Features

- **Zero overhead in release**: Empty class, all operations compile to nothing
- **Full functionality in debug**: Store, access, modify values normally
- **C++20 enhancement**: True zero-size with `[[no_unique_address]]`
- **Type-safe**: Preserves type information even when empty

---

## Quick Start

### Basic Usage

```cpp
struct Widget {
    int id;
    DebugOnly<std::string> debug_name;  // Only in debug builds
    
    Widget(int id, std::string name) 
        : id(id), debug_name(std::move(name)) {}
};

void process(Widget& w) {
#ifndef NDEBUG
    std::cout << "Processing: " << w.debug_name.get() << "\n";
#endif
    // ... actual processing
}
```

### Debug Logging

```cpp
class Connection {
    Socket socket_;
    DebugOnly<std::chrono::steady_clock::time_point> created_at_;
    
public:
    Connection(Socket s) 
        : socket_(std::move(s))
        , created_at_(std::chrono::steady_clock::now()) 
    {}
    
    void log_lifetime() {
#ifndef NDEBUG
        auto now = std::chrono::steady_clock::now();
        auto duration = now - created_at_.get();
        std::cout << "Connection alive for " 
                  << std::chrono::duration_cast<std::chrono::seconds>(duration).count()
                  << "s\n";
#endif
    }
};
```

---

## API Reference

### In Debug Builds (NDEBUG not defined)

```cpp
template <typename T>
struct DebugOnly {
    T value;
    
    // Constructors
    DebugOnly();                    // Default-initialize value
    DebugOnly(const T& val);        // Copy value
    DebugOnly(T&& val);             // Move value
    
    // Assignment
    DebugOnly& operator=(const T& val);
    DebugOnly& operator=(T&& val);
    
    // Access
    operator T&();                  // Implicit conversion
    operator const T&() const;
    T& get();                       // Explicit access
    const T& get() const;
    T* operator->();                // Pointer-like access
    const T* operator->() const;
    T& operator*();                 // Dereference
    const T& operator*() const;
};
```

### In Release Builds (NDEBUG defined)

```cpp
template <typename T>
struct DebugOnly {
    // Empty - no storage
    
    // Constructors (all no-ops)
    constexpr DebugOnly() noexcept = default;
    constexpr DebugOnly(const T&) noexcept {}
    constexpr DebugOnly(T&&) noexcept {}
    
    // Assignment (all no-ops, return *this)
    constexpr DebugOnly& operator=(const T&) noexcept { return *this; }
    constexpr DebugOnly& operator=(T&&) noexcept { return *this; }
    
    // No accessors - accessing in release is a compile error
};
```

---

## C++20 Zero-Overhead

### The Problem in C++17

In C++17, empty classes still take at least 1 byte as members:

```cpp
struct MyClass {
    int data;                        // 4 bytes
    DebugOnly<std::string> debug;    // 1 byte (empty in release)
    // Total: 8 bytes (with padding)
};
```

### The Solution in C++20

Use `[[no_unique_address]]` for true zero overhead:

```cpp
struct MyClass {
    int data;                        // 4 bytes
    
#if FATP_HAS_CPP20
    [[no_unique_address]]
#endif
    DebugOnly<std::string> debug;    // 0 bytes in C++20!
    // Total: 4 bytes
};
```

### Recommended Pattern

```cpp
struct OptimalClass {
    // Production data
    int id;
    Data payload;
    
    // Debug-only data (zero overhead in C++20 release)
#if FATP_HAS_CPP20
    [[no_unique_address]]
#endif
    DebugOnly<std::string> debug_label;
    
#if FATP_HAS_CPP20
    [[no_unique_address]]
#endif
    DebugOnly<SourceLocation> created_at;
};
```

---

## Use Cases

### Debug Labels

```cpp
class Entity {
    EntityId id_;
    DebugOnly<std::string> name_;  // For debugging only
    
public:
    Entity(EntityId id, std::string name) 
        : id_(id), name_(std::move(name)) {}
    
    void dump() const {
#ifndef NDEBUG
        std::cout << "Entity[" << id_ << "]: " << name_.get() << "\n";
#endif
    }
};
```

### Performance Counters

```cpp
class Algorithm {
    DebugOnly<size_t> iterations_;
    DebugOnly<size_t> cache_hits_;
    DebugOnly<size_t> cache_misses_;
    
public:
    void run() {
        // In release: these assignments compile to nothing
        iterations_ = 0;
        cache_hits_ = 0;
        cache_misses_ = 0;
        
        while (condition) {
#ifndef NDEBUG
            ++iterations_.get();
#endif
            // ... algorithm ...
        }
    }
    
    void report() const {
#ifndef NDEBUG
        std::cout << "Iterations: " << iterations_.get() << "\n";
        std::cout << "Cache hit rate: " 
                  << (100.0 * cache_hits_.get() / (cache_hits_.get() + cache_misses_.get()))
                  << "%\n";
#endif
    }
};
```

### Creation Tracking

```cpp
class Resource {
    Handle handle_;
    DebugOnly<std::string> creator_location_;
    DebugOnly<std::thread::id> creator_thread_;
    
public:
    Resource(Handle h, const char* file, int line) 
        : handle_(h)
        , creator_location_(std::string(file) + ":" + std::to_string(line))
        , creator_thread_(std::this_thread::get_id())
    {}
    
    void validate_thread() const {
#ifndef NDEBUG
        if (creator_thread_.get() != std::this_thread::get_id()) {
            std::cerr << "Warning: Resource created at " 
                      << creator_location_.get()
                      << " accessed from different thread\n";
        }
#endif
    }
};

#define MAKE_RESOURCE(h) Resource(h, __FILE__, __LINE__)
```

### Expensive Invariant Checks

```cpp
class SortedContainer {
    std::vector<int> data_;
    DebugOnly<bool> verified_sorted_;
    
public:
    void insert(int value) {
        // ... insert maintaining sorted order ...
        
#ifndef NDEBUG
        // Expensive O(n) check only in debug
        verified_sorted_ = std::is_sorted(data_.begin(), data_.end());
        assert(verified_sorted_.get());
#endif
    }
};
```

---

## Best Practices

### Do

```cpp
// ✅ Use for debug labels
DebugOnly<std::string> debug_name;

// ✅ Use for performance counters
DebugOnly<size_t> operation_count;

// ✅ Use [[no_unique_address]] in C++20
#if FATP_HAS_CPP20
[[no_unique_address]]
#endif
DebugOnly<Metadata> debug_info;

// ✅ Guard debug-only access with #ifndef NDEBUG
#ifndef NDEBUG
std::cout << debug_value.get();
#endif

// ✅ Use for expensive validation
DebugOnly<bool> invariant_checked;
```

### Don't

```cpp
// ❌ Don't use for production data
DebugOnly<ImportantData> data;  // Lost in release!

// ❌ Don't access without guards in portable code
std::cout << debug_value.get();  // Won't compile in release

// ❌ Don't forget the value is gone in release
void save(const Widget& w) {
    file << w.debug_name.get();  // Crashes in release!
}
```

---

## Size Comparison

| Configuration | DebugOnly Size | With `[[no_unique_address]]` |
|---------------|----------------|------------------------------|
| Debug (any) | sizeof(T) | sizeof(T) |
| Release C++17 | 1 byte | 1 byte |
| Release C++20 | 1 byte | **0 bytes** |

---

## Related Components

- **enforce.h**: Debug-only assertions
- **DiagnosticLogger.h**: Debug logging utilities

---

**Document Version:** 1.0  
**Last Updated:** November 2025
