# DebugOnly: A Fat-P Library Showcase

## Executive Summary

DebugOnly is a **zero-overhead conditional storage wrapper** that holds values in debug builds and compiles to literally nothing in release builds via `NDEBUG` detection. Unlike `#ifdef` scattered throughout code (messy, error-prone) or always-present debug fields (wastes memory), DebugOnly provides **type-safe debug storage** where `sizeof(DebugOnly<T>)` is `sizeof(T)` in debug and `1` byte (empty class) in release. The identical API in both modes means debug code compiles and type-checks even in release—catching errors without paying runtime cost.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The #ifdef spaghetti
class Entity {
#ifdef DEBUG
    std::string debug_name_;
    uint64_t creation_time_;
    int operation_count_;
#endif

public:
    void process() {
#ifdef DEBUG
        ++operation_count_;
        log(debug_name_, " processed, count=", operation_count_);
#endif
        // actual work
    }
};
// Problem: #ifdef scattered everywhere, easy to mismatch, clutters code

// The always-present overhead
class Entity {
    std::string debug_name_;  // 32 bytes in release builds too!
    uint64_t creation_time_;  // 8 bytes wasted
    int operation_count_;     // 4 bytes wasted
};
// 44+ bytes overhead per entity, even in production
```

| Issue | HPC Impact |
|-------|------------|
| `#ifdef` pollution | Code becomes unreadable, hard to maintain |
| Mismatched `#ifdef` | Debug code may not compile in release |
| Always-present fields | Memory overhead in production |
| Type-unsafe | `#ifdef` around declarations can cause ODR violations |

### The Standard's Limitation

C++ has no built-in "debug-only" storage concept:
- `assert()` is debug-only but for checking, not storage
- `[[no_unique_address]]` (C++20) helps but doesn't eliminate storage
- No standard way to conditionally include members

---

## Architecture: Conditional Compilation via Specialization

### The Mechanism: Two Complete Implementations

```cpp
#ifndef NDEBUG
// Debug build: full storage
template <typename T>
struct DebugOnly {
    static constexpr bool is_active = true;
    T value_;
    
    constexpr DebugOnly() : value_() {}
    constexpr DebugOnly(const T& val) : value_(val) {}
    
    constexpr T& get() noexcept { return value_; }
    constexpr const T& get() const noexcept { return value_; }
    constexpr T* operator->() noexcept { return &value_; }
    
    template<typename F>
    void if_debug(F&& func) { func(value_); }
};

#else
// Release build: empty shell
template <typename T>
struct DebugOnly {
    static constexpr bool is_active = false;
    
    constexpr DebugOnly() noexcept {}
    constexpr DebugOnly(const T&) noexcept {}  // Ignores argument
    
    // Returns default-constructed temporary (likely optimized away)
    constexpr T get() const noexcept { return T{}; }
    
    template<typename F>
    void if_debug(F&&) noexcept {}  // Does nothing
};
#endif
```

**Why two implementations instead of `if constexpr`:**

Empty Base Optimization (EBO) requires the type to BE empty, not just have empty members. The release `DebugOnly<T>` has no data members, so:

```cpp
struct Entity {
    DebugOnly<std::string> name;  // 0 bytes effective in release (EBO)
    int id;
};
// Debug:  sizeof(Entity) = 32 + 4 + padding = 40 bytes
// Release: sizeof(Entity) = 4 bytes (just id)
```

### API Consistency

Both implementations have identical APIs—code compiles in both modes:

```cpp
void process(Entity& e) {
    e.debug_name.if_debug([](auto& name) {
        log("Processing: ", name);
    });
    // Release: if_debug is empty, lambda never called
    // Debug: lambda executes, logs name
}
```

---

## Feature Inventory

### 1. Debug Labels

```cpp
class Texture {
    DebugOnly<std::string> name_;
    GLuint handle_;
    
public:
    Texture(const std::string& path) 
        : name_(path), handle_(loadTexture(path)) {}
    
    void bind() {
        name_.if_debug([](auto& n) { 
            log("Binding texture: ", n); 
        });
        glBindTexture(GL_TEXTURE_2D, handle_);
    }
};
// Release: name_ is 0 bytes, if_debug is no-op
```

### 2. Performance Counters

```cpp
class Cache {
    DebugOnly<uint64_t> hits_{0};
    DebugOnly<uint64_t> misses_{0};
    std::unordered_map<Key, Value> data_;
    
public:
    Value* find(const Key& k) {
        auto it = data_.find(k);
        if (it != data_.end()) {
            hits_.if_debug([](auto& h) { ++h; });
            return &it->second;
        }
        misses_.if_debug([](auto& m) { ++m; });
        return nullptr;
    }
    
    void dumpStats() {
        if constexpr (DebugOnly<int>::is_active) {
            log("Cache hits: ", hits_.get(), " misses: ", misses_.get());
        }
    }
};
```

### 3. Creation Tracking

```cpp
class Object {
    DebugOnly<std::chrono::steady_clock::time_point> created_;
    DebugOnly<std::thread::id> creator_thread_;
    
public:
    Object() 
        : created_(std::chrono::steady_clock::now())
        , creator_thread_(std::this_thread::get_id()) {}
    
    void validate() {
        creator_thread_.if_debug([](auto& tid) {
            if (tid != std::this_thread::get_id()) {
                log("Warning: Object used from different thread");
            }
        });
    }
};
```

### 4. Invariant Verification

```cpp
class SortedContainer {
    std::vector<int> data_;
    DebugOnly<bool> verified_{false};
    
public:
    void insert(int val) {
        auto it = std::lower_bound(data_.begin(), data_.end(), val);
        data_.insert(it, val);
        verified_.if_debug([](auto& v) { v = false; });
    }
    
    void verify() {
        verified_.if_debug([this](auto& v) {
            bool sorted = std::is_sorted(data_.begin(), data_.end());
            always_enforce(sorted, "Container invariant violated");
            v = true;
        });
    }
};
```

### 5. Compile-Time Mode Detection

```cpp
template<typename T>
void algorithm(const std::vector<T>& data) {
    if constexpr (DebugOnly<int>::is_active) {
        // Expensive validation only in debug
        validate_input(data);
    }
    // Actual algorithm
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not #ifdef | Why Not Always-Present | Why Not std::optional | Fat-P Advantage |
|----------------|---------------|----------------------|---------------------|-----------------|
| Zero release overhead | ✅ Works | ❌ Memory overhead | ❌ 1 byte + alignment | ✅ Empty class |
| Type-safe API | ❌ Preprocessor | ✅ Works | ✅ Works | ✅ Works |
| Compiles in release | ❌ Code hidden | ✅ Works | ✅ Works | ✅ Works |
| Clean syntax | ❌ Scattered `#ifdef` | ✅ Works | ✅ Works | ✅ Works |
| EBO compatible | ❌ N/A | ❌ Always present | ❌ 1+ bytes | ✅ 0 bytes |

**The Sweet Spot:** DebugOnly is the only option providing zero release overhead, type-safe API that compiles in both modes, and EBO compatibility.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ will never have built-in "debug-only" storage:
- `NDEBUG` is a convention, not language feature
- No standard `#ifdef`-like type-level conditional
- `[[no_unique_address]]` doesn't eliminate storage

DebugOnly provides debug-only storage permanently—a pattern the language cannot express natively.

---

## Performance Characteristics

| Scenario | Debug Build | Release Build |
|----------|-------------|---------------|
| `sizeof(DebugOnly<T>)` | `sizeof(T)` | 1 byte (empty) |
| `DebugOnly<T>` with EBO | `sizeof(T)` | 0 bytes effective |
| `get()` call | Direct access | Returns default `T{}` |
| `if_debug(f)` | Calls `f` | No-op (optimized out) |
| `is_active` check | `if constexpr (true)` | `if constexpr (false)` |

### Where Fat-P Wins
- Game engines (debug names, performance counters)
- Libraries (internal diagnostics without release cost)
- Embedded systems (debug info in development, tight release)

### Where Fat-P Loses (Honesty Builds Trust)
- Always-needed data → use regular members
- Runtime debug toggle → DebugOnly is compile-time only
- Complex debug state → consider dedicated debug subsystem

---

## Integration Points

```
DebugOnly.h
    ↓ uses
CppStandardDetection.h  ([[no_unique_address]] detection)
    ↓ used by
SmallVector.h           (debug capacity tracking)
Signal.h                (debug listener counts)
ObjectPool.h            (debug allocation stats)
```

---

## Final Assessment

DebugOnly delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ cannot express "compile away in release" at the type level. DebugOnly provides this permanently through the `NDEBUG`-switched specialization pattern.

### 2. Specialization
Two complete implementations (debug/release) ensure zero release overhead while maintaining type-safe API in both modes. Empty class optimization means truly zero bytes.

### 3. Control
`is_active` enables compile-time branching. `if_debug()` enables runtime-style syntax that compiles to nothing. You choose the idiom that fits your code.

**Architectural Verdict:** DebugOnly transforms debug instrumentation from **scattered `#ifdef` blocks** to **type-safe, zero-overhead wrappers**. Debug code compiles in release (catching errors) while contributing zero bytes to the binary.

---

*DebugOnly.h (646 lines) — Fat-P Library*
