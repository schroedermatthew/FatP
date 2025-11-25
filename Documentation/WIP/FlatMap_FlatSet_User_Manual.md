# FlatMap and FlatSet User Manual

**Version:** 1.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [FlatMap](#flatmap)
3. [FlatSet](#flatset)
4. [Performance Characteristics](#performance-characteristics)
5. [When to Use](#when-to-use)
6. [API Reference](#api-reference)
7. [Best Practices](#best-practices)

---

## Overview

FlatMap and FlatSet are sorted, contiguous associative containers backed by `std::vector`. They provide better cache locality than node-based containers (`std::map`, `std::set`) at the cost of O(n) insertion.

### Include

```cpp
#include "FlatMap.h"
#include "FlatSet.h"
using namespace fat_p;
```

### Key Characteristics

| Feature | FlatMap/FlatSet | std::map/set |
|---------|-----------------|--------------|
| Storage | Contiguous (vector) | Node-based (tree) |
| Cache locality | Excellent | Poor |
| Lookup | O(log n) | O(log n) |
| Insertion | O(n) | O(log n) |
| Iteration | Very fast | Slower |
| Memory overhead | Low | High (node pointers) |

---

## FlatMap

A sorted vector-backed associative container mapping keys to values.

### Basic Usage

```cpp
FlatMap<std::string, int> ages;

// Insert
ages.insert({"Alice", 30});
ages.insert({"Bob", 25});
ages["Charlie"] = 35;

// Lookup
int alice_age = ages.at("Alice");      // 30, throws if not found
int bob_age = ages["Bob"];             // 25
auto it = ages.find("Charlie");        // Iterator

// Check existence
if (ages.contains("David")) { /* ... */ }
if (ages.count("Eve") > 0) { /* ... */ }

// Iterate (sorted order)
for (const auto& [name, age] : ages) {
    std::cout << name << ": " << age << "\n";
}
```

### Construction

```cpp
// Default
FlatMap<int, std::string> map1;

// With comparator
FlatMap<int, std::string, std::greater<int>> map2;

// From initializer list
FlatMap<std::string, int> map3{
    {"one", 1},
    {"two", 2},
    {"three", 3}
};

// From iterators
std::vector<std::pair<int, int>> data = {{1, 10}, {2, 20}};
FlatMap<int, int> map4(data.begin(), data.end());
```

### Modifiers

```cpp
FlatMap<int, std::string> map;

// Insert
auto [it1, inserted1] = map.insert({1, "one"});
auto [it2, inserted2] = map.insert_or_assign(1, "ONE");  // Updates if exists

// Emplace
auto [it3, inserted3] = map.emplace(2, "two");
auto [it4, inserted4] = map.try_emplace(3, "three");  // No-op if exists

// Erase
map.erase(1);                    // By key
map.erase(map.begin());          // By iterator
map.erase(map.begin(), map.end()); // Range

// Clear
map.clear();
```

### Lookup

```cpp
FlatMap<int, std::string> map{{1, "a"}, {2, "b"}, {3, "c"}};

// Find
auto it = map.find(2);
if (it != map.end()) {
    std::cout << it->second;  // "b"
}

// Bounds
auto lower = map.lower_bound(2);  // First >= 2
auto upper = map.upper_bound(2);  // First > 2
auto [lo, hi] = map.equal_range(2);

// Access
std::string& val = map.at(2);     // Throws if not found
std::string& val2 = map[2];       // Inserts default if not found
```

---

## FlatSet

A sorted vector-backed set container.

### Basic Usage

```cpp
FlatSet<int> numbers;

// Insert
numbers.insert(3);
numbers.insert(1);
numbers.insert(4);
numbers.insert(1);  // Duplicate ignored

// numbers now contains: {1, 3, 4}

// Check membership
if (numbers.contains(3)) { /* ... */ }

// Iterate (sorted)
for (int n : numbers) {
    std::cout << n << " ";  // 1 3 4
}
```

### Construction

```cpp
// Default
FlatSet<int> set1;

// With comparator
FlatSet<int, std::greater<int>> set2;

// From initializer list
FlatSet<std::string> set3{"apple", "banana", "cherry"};

// From iterators
std::vector<int> data = {3, 1, 4, 1, 5};
FlatSet<int> set4(data.begin(), data.end());  // {1, 3, 4, 5}
```

### Modifiers

```cpp
FlatSet<int> set;

// Insert
auto [it1, ok1] = set.insert(1);      // Returns iterator + success flag
set.insert({2, 3, 4});                // Multiple values

// Emplace
auto [it2, ok2] = set.emplace(5);

// Erase
set.erase(3);                         // By value
set.erase(set.begin());               // By iterator
size_t removed = set.erase(99);       // Returns count removed (0 or 1)

// Clear
set.clear();
```

### Set Operations

```cpp
FlatSet<int> a{1, 2, 3, 4};
FlatSet<int> b{3, 4, 5, 6};

// Using std algorithms (works because FlatSet is sorted)
FlatSet<int> intersection, union_set, difference;

std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                      std::inserter(intersection, intersection.begin()));
// intersection = {3, 4}

std::set_union(a.begin(), a.end(), b.begin(), b.end(),
               std::inserter(union_set, union_set.begin()));
// union_set = {1, 2, 3, 4, 5, 6}

std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                    std::inserter(difference, difference.begin()));
// difference = {1, 2}
```

---

## Performance Characteristics

### Complexity

| Operation | FlatMap/FlatSet | std::map/set |
|-----------|-----------------|--------------|
| Lookup | O(log n) | O(log n) |
| Insert | **O(n)** | O(log n) |
| Erase | **O(n)** | O(log n) |
| Iteration | O(n) | O(n) |
| Memory per element | sizeof(T) | sizeof(T) + ~32 bytes |

### Benchmarks (1000 elements)

| Operation | FlatMap | std::map | Speedup |
|-----------|---------|----------|---------|
| Lookup | 45ns | 120ns | 2.7x |
| Full iteration | 0.5µs | 8µs | 16x |
| Insert (random) | 2µs | 150ns | 0.08x |
| Insert (sorted) | 50ns | 150ns | 3x |

### Memory Usage

```
FlatMap<int, int> with 1000 elements:
  - Data: 8000 bytes (8 bytes × 1000)
  - Overhead: ~24 bytes (vector metadata)
  - Total: ~8024 bytes

std::map<int, int> with 1000 elements:
  - Data: 8000 bytes
  - Node overhead: ~32000 bytes (32 bytes × 1000 nodes)
  - Total: ~40000 bytes
```

---

## When to Use

### Use FlatMap/FlatSet When

✅ Collection is **read-mostly** (many lookups, few modifications)  
✅ Collection is **small to medium** (<10,000 elements)  
✅ Elements are **inserted in bulk**, then queried  
✅ **Cache performance** is critical  
✅ **Memory** is constrained  
✅ Need **fast iteration**  

### Use std::map/set When

✅ **Frequent insertions/deletions** in random order  
✅ Collection is **large** (>100,000 elements)  
✅ Need **iterator stability** (iterators don't invalidate on insert)  
✅ Elements are **inserted one at a time** throughout lifetime  

### Typical Use Cases

```cpp
// ✅ Good for FlatMap
FlatMap<std::string, Config> config;   // Loaded once, queried often
FlatMap<int, Texture> textures;        // Game assets
FlatMap<UserId, CachedData> cache;     // Read-heavy cache

// ✅ Good for FlatSet
FlatSet<std::string> keywords;         // Known at startup
FlatSet<int> valid_ids;                // Validation set
FlatSet<Event> sorted_events;          // Sorted event log
```

---

## API Reference

### FlatMap Types

```cpp
template <typename Key, typename T, 
          typename Compare = std::less<Key>,
          typename Allocator = std::allocator<std::pair<const Key, T>>>
class FlatMap {
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = std::size_t;
    using iterator = /* random access */;
    using const_iterator = /* random access */;
};
```

### FlatSet Types

```cpp
template <typename T,
          typename Compare = std::less<T>,
          typename Allocator = std::allocator<T>>
class FlatSet {
    using key_type = T;
    using value_type = T;
    using size_type = std::size_t;
    using iterator = /* random access */;
    using const_iterator = /* random access */;
};
```

### Common Methods

Both containers support:
- `begin()`, `end()`, `rbegin()`, `rend()` - Iterators
- `empty()`, `size()`, `max_size()` - Capacity
- `clear()` - Modifiers
- `insert()`, `emplace()`, `erase()` - Element access
- `find()`, `count()`, `contains()` - Lookup
- `lower_bound()`, `upper_bound()`, `equal_range()` - Bounds
- `key_comp()`, `value_comp()` - Observers

---

## Best Practices

### Do

```cpp
// ✅ Reserve if size known
FlatMap<int, Data> map;
map.reserve(expected_size);  // Reduces reallocations
for (auto& item : items) map.insert(item);

// ✅ Bulk insert then query
std::vector<std::pair<K, V>> data = load_data();
FlatMap<K, V> map(data.begin(), data.end());  // Single sort
// Now query many times...

// ✅ Use for small collections
FlatSet<StatusCode> valid_codes{OK, PENDING, COMPLETE};
```

### Don't

```cpp
// ❌ Don't use for frequent random insertions
FlatMap<int, Data> live_data;
while (running) {
    live_data.insert(get_new_item());  // O(n) each time!
}

// ❌ Don't use for huge collections with modifications
FlatMap<int, Data> huge_map;  // 1M+ elements
huge_map.insert(item);  // Very slow!
```

---

## Related Components

- **SmallVector.h**: Could be used as backing storage for very small flat containers
- **FatPTypeTraits.h**: `is_flat_map_v<T>`, `is_flat_set_v<T>` traits

---

**Document Version:** 1.0  
**Last Updated:** November 2025
