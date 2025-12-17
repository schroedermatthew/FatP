# StableHashMap

A single-header C++17 hash map optimized for workloads with high insert/erase churn.

## The Problem

Most hash tables degrade under sustained insert/delete cycles. Tombstone-based implementations accumulate dead slots that slow lookups over time. After millions of operations, a table that started at 30ns/lookup can degrade to 180ns or worse.

## The Solution

StableHashMap uses **Robin Hood hashing with backward-shift deletion**. When an element is erased, subsequent elements shift backward to fill the gap. No tombstones accumulate. Performance remains stable indefinitely.

## When to Use It

✓ Long-running caches with continuous eviction  
✓ Simulation engines with entity creation/destruction  
✓ Streaming aggregation with key expiration  
✓ Any workload where insert/erase ratio approaches 1:1  

## When Not to Use It

✗ Pointer/iterator stability required (use `std::unordered_map`)  
✗ Small tables (<100 elements) where overhead dominates  
✗ Lookup-dominated workloads where SIMD tables excel (use `absl::flat_hash_map`)  
✗ Non-DefaultConstructible value types  

## Quick Start

```cpp
#include "StableHashMap.h"

int main() {
    fat_p::StableHashMap<std::string, int> scores;
    
    // Insert
    scores.insert("alice", 100);
    scores.emplace("bob", 85);
    
    // Lookup
    if (int* p = scores.find("alice")) {
        *p += 10;  // Direct mutation
    }
    
    // Erase (triggers backward-shift, no tombstones)
    scores.erase("bob");
    
    // Iterate
    for (auto& [name, score] : scores) {
        std::cout << name << ": " << score << "\n";
    }
    
    return 0;
}
```

## Installation

Copy `StableHashMap.h` into your project. No dependencies beyond the C++17 standard library.

```cpp
#include "StableHashMap.h"
```

## Thread Safety

**StableHashMap is not thread-safe.** Concurrent access requires external synchronization.

| Operation | Concurrent with Readers | Concurrent with Writers |
|-----------|------------------------|------------------------|
| `find()` | ✓ Safe | ✗ Unsafe |
| `insert()` | ✗ Unsafe | ✗ Unsafe |
| `erase()` | ✗ Unsafe | ✗ Unsafe |
| `begin()`/`end()` | ✓ Safe | ✗ Unsafe |

For concurrent access, wrap the map with a mutex or use a concurrent hash map implementation.

```cpp
// Example: External synchronization
std::shared_mutex mutex;
fat_p::StableHashMap<int, Data> map;

// Read path
{
    std::shared_lock lock(mutex);
    if (auto* p = map.find(key)) { /* use *p */ }
}

// Write path
{
    std::unique_lock lock(mutex);
    map.insert(key, value);
}
```

## Performance

Under sustained churn (10M insert/erase cycles):

| Implementation | Fresh Lookup | After 10M Churn |
|----------------|--------------|-----------------|
| StableHashMap | 30 ns | 30 ns |
| robin_hood::unordered_map | 24 ns | 180 ns |
| std::unordered_map | 45 ns | 45 ns |

StableHashMap is not the fastest on fresh tables. It is the fastest that *stays* fast.

## Documentation

| Document | Purpose |
|----------|---------|
| [User Manual](StableHashMap_User_Manual.md) | API reference, migration guide, benchmarks |
| [Companion Guide](StableHashMap_Companion_Guide.md) | Design rationale, case studies, foundations |
| [Benchmarking Case Study](StableHashMap_Benchmarking_Case_Study.md) | Methodology, measurement traps, verification |

## API Summary

```cpp
// Construction
StableHashMap<K, V> map;
StableHashMap<K, V, Policy> map;  // Custom policy

// Insertion
map.insert(key, value);           // Overwrites if exists
map.emplace(key, args...);        // Constructs in-place, overwrites
auto [ptr, inserted] = map.try_emplace(key, args...);  // No overwrite

// Lookup
V* ptr = map.find(key);           // nullptr if not found
V& ref = map.at(key);             // Throws if not found
bool exists = map.contains(key);

// Deletion
bool removed = map.erase(key);

// Capacity
size_t n = map.size();
bool empty = map.empty();
map.reserve(n);
map.clear();

// Iteration
for (auto& [k, v] : map) { ... }

// Read-only optimization
map.freeze();    // Enable 0.95 load factor
map.unfreeze();  // Return to 0.875 load factor
```

## License

See LICENSE file.

---

*FAT-P Library — December 2025*
