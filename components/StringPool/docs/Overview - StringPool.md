# StringPool: A Fat-P Library Showcase

## Executive Summary

StringPool is a **hash-based string interning system** with policy-based thread safety that eliminates duplicate string storage through canonical pointer deduplication. Unlike storing strings directly (each copy allocates), StringPool returns stable pointers to a single canonical copy—enabling **pointer comparison instead of strcmp**, **50-90% memory savings** for duplicate-heavy workloads, and **zero-overhead single-threaded operation** through compile-time policy resolution. This is the data structure compilers use for symbol tables, now available with fat_p's policy-based architecture.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The memory explosion pattern
std::vector<std::string> config_keys;
for (const auto& entry : parse_config(file)) {
    config_keys.push_back(entry.key);  // "server.host" stored 1000x
}
// Result: 1000 copies of "server.host" = 12KB wasted

// The O(n) comparison trap
std::unordered_map<std::string, Value> cache;
for (const auto& key : lookup_keys) {
    auto it = cache.find(key);  // strcmp on every bucket collision
}
```

| Issue | HPC Impact |
|-------|------------|
| Duplicate string storage | Memory bloat: 1000 identical keys = 1000 allocations |
| String comparison overhead | O(n) per strcmp vs. O(1) for pointer comparison |
| Cache pollution | Scattered string storage destroys cache locality |
| Thread-safety overhead | Global mutex for shared string storage kills scalability |

### The Standard's Limitation

The C++ standard provides no string interning facility. `std::string` always owns its storage. Even `std::string_view` requires the underlying string to outlive all views. C++20's `std::unordered_set<std::string>` with heterogeneous lookup helps but still stores duplicates.

**The fundamental problem:** You cannot compare two `std::string` instances by pointer—equal content may have different addresses.

With StringPool: `pool.intern("hello") == pool.intern("hello")` → **true** (same pointer).

---

## Architecture: Hash-Based Deduplication with Policy Threading

### The Mechanism

```cpp
template<typename ConcurrencyPolicy = SingleThreadedPolicy>
class StringPool : private ConcurrencyPolicy {
    std::unordered_set<std::string> strings_;
    // Statistics tracking
    size_t total_bytes_requested_ = 0;
    size_t unique_bytes_stored_ = 0;
    
public:
    const char* intern(std::string_view str) {
        auto lock = this->lock();  // No-op for SingleThreadedPolicy
        
        auto [it, inserted] = strings_.emplace(str);
        if (inserted) {
            unique_bytes_stored_ += str.size();
        }
        total_bytes_requested_ += str.size();
        
        return it->c_str();  // Stable pointer until pool destruction
    }
};
```

**Why this works:**
1. `std::unordered_set<std::string>` stores each unique string once
2. `emplace()` returns existing element if already present
3. `c_str()` returns stable pointer (string won't move after insertion)
4. Policy inheritance uses EBO—zero storage overhead for stateless policies

### Pointer Stability Guarantee

```cpp
const char* s1 = pool.intern("hello");
const char* s2 = pool.intern("world");
// ... insert millions more strings ...
const char* s3 = pool.intern("hello");

assert(s1 == s3);  // Same pointer, guaranteed
```

**Mechanism:** `std::unordered_set` provides pointer stability—elements don't move on rehash (only bucket links change). Strings interned early remain at their original addresses.

---

## Feature Inventory

### 1. Automatic Deduplication

```cpp
StringPool pool;

const char* s1 = pool.intern("hello");
const char* s2 = pool.intern("hello");
const char* s3 = pool.intern("hello");

assert(s1 == s2 && s2 == s3);  // Single storage, three references
// Memory used: 6 bytes (not 18)
```

**Mechanism:** Hash lookup finds existing string; returns its pointer instead of allocating.

### 2. Policy-Based Thread Safety

```cpp
// Single-threaded: zero synchronization overhead
StringPool<SingleThreadedPolicy> st_pool;

// Multi-threaded with shared reads (reader-writer lock)
StringPool<SharedMutexPolicy> shared_pool;

// Multi-threaded with exclusive locking
StringPool<MutexSynchronizationPolicy> mutex_pool;
```

**Mechanism:** `ConcurrencyPolicy::lock()` returns a lock guard. For `SingleThreadedPolicy`, this is an empty struct—the compiler eliminates it entirely.

**Measured overhead:** SingleThreadedPolicy has the lowest overhead (no synchronization). SharedMutexPolicy adds reader/writer lock acquisition. MutexPolicy adds exclusive lock acquisition on every operation. See `components/StringPool/results/` for current platform-specific benchmark data.

### 3. C++20 Heterogeneous Lookup (Zero-Allocation Find)

```cpp
// C++17: find("key") constructs temporary std::string (potential allocation)
// C++20: find("key") uses string_view directly (zero allocation)

StringPool pool;
pool.intern("existing_key");

// In C++20, this does NOT allocate:
if (pool.contains("existing_key")) { /* ... */ }
const char* ptr = pool.find("existing_key");  // No allocation
```

**Mechanism:** C++20's `std::unordered_set` supports heterogeneous lookup via `std::hash<std::string_view>` transparent hashing.

### 4. StringHandle RAII Wrapper

```cpp
StringHandle handle = pool.intern_handle("managed");

// Handle stores pool reference + pointer
std::cout << handle.get();  // "managed"

// Can be stored in containers
std::vector<StringHandle> handles;
handles.push_back(pool.intern_handle("item1"));
handles.push_back(pool.intern_handle("item2"));
```

**Mechanism:** `StringHandle` stores pointer + optional pool reference. Provides RAII semantics when pool lifetime management is needed.

### 5. Usage Statistics

```cpp
StringPool pool;
for (int i = 0; i < 10000; ++i) {
    pool.intern("repeated_key");
}

auto stats = pool.stats();
// stats.unique_strings = 1
// stats.total_requests = 10000
// stats.unique_bytes = 12
// stats.total_bytes_requested = 120000
// stats.memory_saved = 119988 bytes (99.99%)
// stats.hit_rate = 0.9999
```

**Mechanism:** Counters track every intern request vs. unique insertions. `memory_saved = total_requested - unique_stored`.

---

## Why Not Alternatives?

| If You Need... | Why Not std::set<std::string> | Why Not Manual Dedup | Why Not std::string_view | Fat-P Advantage |
|----------------|------------------------------|---------------------|-------------------------|-----------------|
| O(1) lookup | ❌ O(log n) | ✅ Hash table | N/A | ✅ Hash table |
| Pointer stability | ✅ Stable | Manual tracking | ❌ No ownership | ✅ Guaranteed stable |
| Thread safety options | Manual | Manual | N/A | ✅ Policy-based |
| Zero single-threaded overhead | Manual | Manual | N/A | ✅ Compile-time |
| Statistics | Manual | Manual | N/A | ✅ Built-in |
| C++20 zero-alloc lookup | Manual | Manual | N/A | ✅ Automatic |

**The Sweet Spot:** StringPool is the only option combining:
- ✅ Hash-based O(1) lookup
- ✅ Guaranteed pointer stability
- ✅ Policy-based thread safety with EBO
- ✅ C++20 heterogeneous lookup support
- ✅ Built-in statistics tracking

---

## The "Forever Stuck" Reality

**Standard Library Reality:** The C++ committee will not add string interning because:

1. **Lifetime complexity:** Interned strings typically live forever; C++ emphasizes deterministic destruction
2. **Global state concerns:** Classic interning uses global pools; C++ avoids hidden global state
3. **Thread safety tradeoffs:** No single threading model fits all use cases

Compilers, JSON parsers, and game engines have implemented string pools for decades. Fat-P provides a production-ready implementation with policy-based threading, avoiding both the overhead of mandatory thread-safety and the unsafety of no-thread-safety.

---

## Performance Characteristics

### Performance Characteristics

Intern hits are fast (hash lookup only, no allocation). Intern misses are slower (hash + string copy into the pool). `find()` and `contains()` are the cheapest operations (read-only hash lookup, no modification path). SharedMutexPolicy adds lock overhead on every operation, with contention scaling under multiple threads. See `components/StringPool/results/` for current platform-specific benchmark data.

### Memory Savings Examples

| Workload | Without Pool | With Pool | Savings |
|----------|-------------|-----------|---------|
| Config keys (10K entries, 50 unique) | 120 KB | 2.4 KB | 98% |
| JSON keys (100K entries, 100 unique) | 1.5 MB | 15 KB | 99% |
| Log prefixes (1M entries, 20 unique) | 15 MB | 300 bytes | 99.99% |

### Where Fat-P Wins

- **Configuration systems:** Repeated key access
- **JSON parsing:** Duplicate object keys
- **Logging:** Repeated message templates
- **Symbol tables:** Compiler identifier storage
- **Game entities:** Shared names/tags

### Where Fat-P Loses (Honesty Builds Trust)

- **All-unique strings:** No duplicates = no savings, only overhead
- **Short-lived strings:** Pool lifetime must exceed all references
- **Very large strings:** Interning multi-MB strings wastes pool memory if rarely reused
- **C++17 heavy lookup:** Without C++20, `find()` allocates a temporary string

---

## Integration Points

```
StringPool.h
    ↓ uses
ConcurrencyPolicies.h   (SingleThreaded, Mutex, SharedMutex)
    ↓ used by
JsonLite.h      (key deduplication)
FatPJsonLite.h  (key deduplication via StringPool)
Configuration systems
Logging frameworks
```

---

## Final Assessment

StringPool delivers on the fat_p promise through three pillars:

### 1. Permanence
The standard library will never provide string interning—the design requires global or long-lived pools that conflict with C++'s RAII philosophy. StringPool makes a compiler/game-engine-grade primitive available with explicit lifetime management.

### 2. Specialization
Policy-based threading with EBO provides zero-overhead single-threaded operation and correct multi-threaded behavior without runtime dispatch. C++20 heterogeneous lookup eliminates allocation overhead automatically when available.

### 3. Control
Three threading policies let architects choose their safety/performance tradeoff. Statistics tracking is always available. Pool lifetime is explicit—you control when interned strings become invalid.

**Architectural Verdict:** StringPool transforms string handling from **allocation-bound** (every copy allocates) to **lookup-bound** (hash probe only). Pointer comparison replaces strcmp. Memory usage drops 50-99%. It's the optimization compilers have used for decades—now available as a single header with modern threading policies.

---

*StringPool.h (586 lines) — Fat-P Library*
