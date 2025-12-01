# SortedContainer User Manual

## Table of Contents

1. [What is SortedContainer?](#what-is-sortedcontainer)
2. [Core Architecture](#core-architecture)
3. [Getting Started](#getting-started)
4. [API Reference](#api-reference)
5. [Uniqueness Policies](#uniqueness-policies)
6. [Thread Safety](#thread-safety)
7. [Performance](#performance)
8. [Comparison with Alternatives](#comparison-with-alternatives)
9. [Migration Guide](#migration-guide)
10. [Best Practices](#best-practices)
11. [Troubleshooting](#troubleshooting)
12. [Summary](#summary)

---

## What is SortedContainer?

### The Problem

Sorted data is fundamental to efficient searching. Binary search delivers O(log N) lookups, but maintaining sorted order requires discipline. Consider what happens without a dedicated container:

```cpp
// The manual approach: fragile and error-prone
std::vector<int> data;

void add_value(int value) {
    // Must remember to find insertion point
    auto it = std::lower_bound(data.begin(), data.end(), value);
    data.insert(it, value);
}

void add_values(const std::vector<int>& batch) {
    // Easy to forget: append-then-sort vs insert-each has different performance
    for (int v : batch) {
        add_value(v);  // O(N²) for batch!
    }
}

// Later, someone adds this "optimization":
void add_values_fast(const std::vector<int>& batch) {
    data.insert(data.end(), batch.begin(), batch.end());
    // Forgot to sort! Container is now corrupted.
}
```

This pattern appears constantly in codebases:

1. **Forgotten invariants** — Someone appends without sorting
2. **Wrong algorithm choice** — Individual inserts vs batch sort
3. **Duplicate handling** — Inconsistent policies across the codebase
4. **Thread safety** — No protection for concurrent access
5. **Floating-point data** — Exact equality fails for approximate duplicates

### The C++ Landscape

The standard library offers `std::set` and `std::multiset` for sorted unique/duplicate collections. These are typically red-black trees with O(log N) insert and lookup. However, they have drawbacks:

- **Node-based allocation** — Each element is a separate heap allocation, causing cache misses during iteration
- **No random access** — Cannot index by position
- **No contiguous storage** — Cannot pass underlying data to C APIs or SIMD operations
- **Fixed uniqueness** — `std::set` is always unique, `std::multiset` always allows duplicates
- **No fuzzy matching** — Exact comparison only, problematic for floating-point

Sorted `std::vector` provides contiguous storage and cache-friendly iteration, but requires manual maintenance of the sorted invariant.

### Where SortedContainer Fits

SortedContainer bridges this gap: a sorted vector that automatically maintains its invariant, with policy-based customization for uniqueness, comparison, and thread safety.

**Version:** 1.0  
**License:** Header-only, part of fat_p library  
**C++ Standard:** C++17 or later  
**Dependencies:** Expected.h, ConcurrencyPolicies.h, EqualityComparisons.h, enforce.h, CheckedArithmetic.h, EnforcedInit.h, StrongId.h, TypeTraits.h, ScopeGuard.h, DiagnosticLogger_Core.h

---

## Core Architecture

### Design Decisions

**Why a sorted vector instead of a tree?**

For read-heavy workloads (common in scientific computing), contiguous storage wins. Iterating a 10,000-element vector is 5-10x faster than iterating equivalent tree nodes due to cache locality. The trade-off is O(N) insertion (shifting elements) versus O(log N) for trees. SortedContainer is optimized for:

- Batch insertions (sort once, not per-element)
- Frequent lookups and iteration
- Memory-constrained environments (no per-node overhead)

**Why policy-based design?**

Different applications need different behaviors:

```cpp
// Scientific computing: fuzzy duplicate detection
SortedContainer<double, FuzzyUniquePolicy<HybridComparisonPolicy, double>>

// Game engine: allow duplicates, thread-safe
SortedContainer<Entity, AllowDuplicatesPolicy, EntityCompare, 
                std::allocator<Entity>, MutexSynchronizationPolicy>

// Embedded: unique, single-threaded, custom allocator
SortedContainer<int, OnlyUniquePolicy, std::less<int>, PoolAllocator<int>>
```

Policies are resolved at compile-time with zero runtime overhead.

**Why Expected-based error handling?**

HPC and scientific code often cannot use exceptions (disabled for performance or embedded constraints). Expected<T, E> provides:

- Explicit error handling without exceptions
- Zero overhead in the success path
- Forced acknowledgment of potential failures

### Internal Structure

```
SortedContainer<T, Uniqueness, Compare, Allocator, Concurrency, Backend>
       │
       ├── InternalContainer (std::vector<T> or std::deque<T>)
       │       └── Always sorted according to Compare
       │
       ├── ComparePolicy instance
       │       └── Determines element ordering
       │
       └── Inherits ConcurrencyPolicy
               └── Provides getLock(), LockGuard, SharedGuard
```

**Invariant:** Elements are always sorted. This is:
- Enforced by `insert()`, `insertRange()`, `erase()`
- Validated in debug builds via `enforce()`
- Zero-overhead in release builds

### Batch Insertion Strategy

Small batches (<16 elements) use insertion sort — low overhead for few elements. Large batches use `std::stable_sort` — O(N log N) with stability for equal elements.

```cpp
// Internal logic (simplified):
if (batch_size < 16) {
    // Insertion sort: O(K²) but fast for small K
    for (auto it = start; it != end; ++it) {
        auto pos = std::upper_bound(begin(), it, *it, compare_);
        std::rotate(pos, it, it + 1);
    }
} else {
    // Merge with existing: O(N log N)
    std::stable_sort(begin(), end(), compare_);
}
```

---

## Getting Started

### Prerequisites

Include the header and its dependencies:

```cpp
#include "SortedContainer.h"
```

All dependencies are internal to the fat_p library.

### First Program

```cpp
#include "SortedContainer.h"
#include <iostream>

int main() {
    using namespace fat_p;
    
    // Create a sorted container (allows duplicates by default)
    SortedContainer<int> numbers;
    
    // Insert values — automatically sorted
    auto r1 = numbers.insert(5);
    auto r2 = numbers.insert(2);
    auto r3 = numbers.insert(8);
    auto r4 = numbers.insert(2);  // Duplicate allowed
    
    // Always check Expected returns (or use [[nodiscard]] warnings)
    if (!r1 || !r2 || !r3 || !r4) {
        std::cerr << "Insert failed\n";
        return 1;
    }
    
    // Container is now: [2, 2, 5, 8]
    
    // Binary search: O(log N)
    auto it = numbers.find(5);
    if (it != numbers.end()) {
        std::cout << "Found: " << *it << "\n";
    }
    
    // Count occurrences
    auto count = numbers.count(2);
    std::cout << "Count of 2: " << static_cast<size_t>(count) << "\n";
    
    // Iterate (always sorted)
    std::cout << "All elements: ";
    for (auto it = numbers.begin(); it != numbers.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << "\n";
    
    return 0;
}
```

**Output:**
```
Found: 5
Count of 2: 2
All elements: 2 2 5 8
```

### Unique Elements

```cpp
#include "SortedContainer.h"
#include <iostream>

int main() {
    using namespace fat_p;
    
    SortedContainer<int, OnlyUniquePolicy> unique_numbers;
    
    auto r1 = unique_numbers.insert(5);
    auto r2 = unique_numbers.insert(5);  // Duplicate
    auto r3 = unique_numbers.insert(3);
    
    std::cout << "Insert 5: " << (r1.value() ? "inserted" : "rejected") << "\n";
    std::cout << "Insert 5: " << (r2.value() ? "inserted" : "rejected") << "\n";
    std::cout << "Insert 3: " << (r3.value() ? "inserted" : "rejected") << "\n";
    
    // Container: [3, 5]
    return 0;
}
```

**Output:**
```
Insert 5: inserted
Insert 5: rejected
Insert 3: inserted
```

---

## API Reference

### Template Parameters

```cpp
template <
    typename T,
    typename UniquenessPolicy = AllowDuplicatesPolicy,
    typename ComparePolicy = std::less<T>,
    typename Allocator = std::allocator<T>,
    typename ConcurrencyPolicy = SingleThreadedPolicy,
    template <typename, typename> class BackendPolicy = VectorBackendPolicy
>
class SortedContainer;
```

| Parameter | Default | Purpose |
|-----------|---------|---------|
| `T` | — | Element type |
| `UniquenessPolicy` | `AllowDuplicatesPolicy` | How to handle duplicates |
| `ComparePolicy` | `std::less<T>` | Ordering relation |
| `Allocator` | `std::allocator<T>` | Memory allocation |
| `ConcurrencyPolicy` | `SingleThreadedPolicy` | Thread safety |
| `BackendPolicy` | `VectorBackendPolicy` | Storage container |

### Type Aliases

```cpp
using value_type = T;
using const_iterator = typename InternalContainer::const_iterator;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
using size_type = StrongId<size_t, struct SizeTag>;
```

**Note:** Iterators are const-only to prevent modification that would break the sorted invariant.

### Constructors

#### Default Constructor

```cpp
SortedContainer();
```

Creates an empty container.

#### Range Constructor

```cpp
template <typename InputIt, typename... EpsParams>
SortedContainer(InputIt first, InputIt last, EpsParams... eps);
```

Constructs from iterator range. Elements are sorted after insertion.

```cpp
std::vector<int> data = {5, 2, 8, 1};
fat_p::SortedContainer<int> sorted(data.begin(), data.end());
// Result: [1, 2, 5, 8]
```

### Capacity

| Method | Complexity | Description |
|--------|------------|-------------|
| `size()` | O(1) | Number of elements |
| `empty()` | O(1) | True if empty |
| `capacity()` | O(1) | Current storage capacity |
| `reserve(n)` | O(N) | Reserve storage for n elements |

### Element Access

#### find

```cpp
[[nodiscard]] const_iterator find(const T& value) const;
```

Binary search for an element. Returns `end()` if not found.

**Complexity:** O(log N)

#### lower_bound / upper_bound

```cpp
[[nodiscard]] const_iterator lower_bound(const T& value) const;
[[nodiscard]] const_iterator upper_bound(const T& value) const;
```

Standard binary search bounds.

#### count

```cpp
[[nodiscard]] size_type count(const T& value) const;
```

Count elements equivalent to value.

**Complexity:** O(log N + K) where K is the count

### Modifiers

#### insert

```cpp
template <typename U = T, typename... EpsParams>
[[nodiscard]] Expected<bool, std::string> insert(U&& value, EpsParams... eps);
```

Insert a value while maintaining sorted order.

**Returns:**
- `true` if inserted
- `false` if duplicate rejected (unique policies)
- `unexpected(error)` on failure

**Complexity:** O(log N) search + O(N) insertion

#### insertRange

```cpp
template <typename InputIt, typename... EpsParams>
[[nodiscard]] Expected<void, std::string> insertRange(InputIt first, InputIt last, EpsParams... eps);
```

Batch insert from iterator range. More efficient than individual inserts.

**Complexity:** O(N log N) for large batches

#### erase

```cpp
[[nodiscard]] Expected<bool, std::string> erase(const T& value);
```

Erase first occurrence of value.

**Returns:** `true` if erased, `false` if not found

#### clear

```cpp
void clear();
```

Remove all elements.

### Container Access

#### toVector

```cpp
[[nodiscard]] InternalContainer toVector() const;
```

Returns a **copy** of the internal container. Safe to modify.

#### withInternalContainer

```cpp
template <typename Func>
decltype(auto) withInternalContainer(Func&& func) const;
```

Execute a function with scoped, thread-safe access to the internal container. **This is the preferred method for thread-safe access.**

```cpp
// Sum all elements (lock held during entire operation)
int sum = container.withInternalContainer([](const auto& vec) {
    return std::accumulate(vec.begin(), vec.end(), 0);
});
```

---

## Uniqueness Policies

### What is a Uniqueness Policy?

A uniqueness policy determines how the container handles duplicate elements. Unlike `std::set` (always unique) or `std::multiset` (always duplicates), SortedContainer lets you choose — and even define custom logic like fuzzy matching.

### AllowDuplicatesPolicy (Default)

Allows duplicate elements. Equal elements maintain insertion stability (first inserted appears first).

```cpp
fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy> sv;
sv.insert(5);
sv.insert(5);
sv.insert(5);
// Result: [5, 5, 5]
```

**When to use:** Multisets, histograms, event logs with timestamps.

### OnlyUniquePolicy

Rejects duplicates silently (returns `false`, not an error).

```cpp
fat_p::SortedContainer<int, fat_p::OnlyUniquePolicy> sv;
auto r1 = sv.insert(5);  // r1.value() == true
auto r2 = sv.insert(5);  // r2.value() == false (rejected)
auto r3 = sv.insert(3);  // r3.value() == true
// Result: [3, 5]
```

**When to use:** Unique IDs, symbol tables, deduplication.

### FuzzyUniquePolicy

Rejects elements within epsilon tolerance. Essential for floating-point data where exact equality is unreliable.

```cpp
using FuzzySV = fat_p::SortedContainer<double, 
    fat_p::FuzzyUniquePolicy<fat_p::HybridComparisonPolicy, double>>;

FuzzySV sv;
constexpr double eps = 0.01;

sv.insert(1.0, eps, eps);    // Inserted
sv.insert(1.005, eps, eps);  // Rejected (within epsilon of 1.0)
sv.insert(1.02, eps, eps);   // Inserted (outside epsilon)
// Result: [1.0, 1.02]
```

**Available equality policies:**
- `ExactComparisonPolicy` — Bitwise equality
- `AbsoluteComparisonPolicy` — |a - b| < ε
- `RelativeComparisonPolicy` — |a - b| / max(|a|, |b|) < ε
- `HybridComparisonPolicy` — Combines absolute and relative (recommended)
- `ULPComparisonPolicy` — Units in Last Place comparison

**When to use:** Scientific data, sensor readings, numerical computations.

### LoggingUniquePolicy

Wraps another policy and logs insertions via DiagnosticLogger.

```cpp
using LoggingSV = fat_p::SortedContainer<int, 
    fat_p::LoggingUniquePolicy<fat_p::OnlyUniquePolicy>>;

LoggingSV sv;
sv.insert(42);  // Logs: "Inserting: 42"
```

**When to use:** Debugging, auditing, understanding insertion patterns.

### TransformUniquenessPolicy

Applies a transformation before uniqueness comparison.

```cpp
struct AbsTransformer {
    int operator()(int x) const { return std::abs(x); }
};

using TransformSV = fat_p::SortedContainer<int, 
    fat_p::TransformUniquenessPolicy<fat_p::OnlyUniquePolicy, AbsTransformer>>;

TransformSV sv;
sv.insert(5);   // Inserted
sv.insert(-5);  // Rejected (|−5| == |5|)
sv.insert(3);   // Inserted
// Result: sorted by original value, unique by absolute value
```

**When to use:** Case-insensitive strings, normalized keys, equivalence classes.

---

## Thread Safety

### Why Thread Safety Matters

Without synchronization, concurrent access to a container causes data races — undefined behavior that can corrupt memory, crash programs, or produce incorrect results silently.

### Available Policies

| Policy | Description | Overhead | Use Case |
|--------|-------------|----------|----------|
| `SingleThreadedPolicy` | No synchronization | Zero | Single-threaded code |
| `MutexSynchronizationPolicy` | `std::mutex` | Medium | General thread safety |
| `SharedMutexPolicy` | `std::shared_mutex` | Medium | Read-heavy workloads |
| `SpinlockSynchronizationPolicy` | Atomic spinlock | Low | Short critical sections |

### Thread-Safe Container

```cpp
#include "SortedContainer.h"
#include <thread>
#include <vector>

int main() {
    using namespace fat_p;
    
    using ThreadSafeSV = SortedContainer<int, 
        AllowDuplicatesPolicy, 
        std::less<int>, 
        std::allocator<int>, 
        MutexSynchronizationPolicy>;
    
    ThreadSafeSV container;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&container, t]() {
            for (int i = 0; i < 1000; ++i) {
                (void)container.insert(t * 1000 + i);
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    // Container has 4000 sorted elements
    return 0;
}
```

### Safe Iteration Pattern

**Problem:** Iterators from `begin()`/`end()` become invalid after the lock is released.

```cpp
// UNSAFE: Lock released after begin()/end() return
for (auto it = container.begin(); it != container.end(); ++it) {
    // Another thread could modify the container here!
}
```

**Solution:** Use `withInternalContainer()` to hold the lock during iteration:

```cpp
container.withInternalContainer([](const auto& vec) {
    for (const auto& elem : vec) {
        // Lock held for entire iteration
        process(elem);
    }
});
```

### Read-Heavy Optimization

When reads greatly outnumber writes, `SharedMutexPolicy` allows multiple concurrent readers:

```cpp
using ReadHeavySV = fat_p::SortedContainer<Data, 
    fat_p::OnlyUniquePolicy, 
    std::less<Data>, 
    std::allocator<Data>, 
    fat_p::SharedMutexPolicy>;
```

---

## Performance

### Benchmark Environment

| Component | Specification |
|-----------|---------------|
| Processor | Intel Core i7-8850H @ 2.60 GHz |
| RAM | 32.0 GB |
| Compiler | MSVC 2022, Release build (`/O2 /DNDEBUG`) |

### Complexity Summary

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `insert` | O(log N + N) | Binary search + vector shift |
| `insertRange` (small <16) | O(K²) | Insertion sort |
| `insertRange` (large) | O(N log N) | stable_sort |
| `find` | O(log N) | Binary search |
| `lower_bound` | O(log N) | |
| `upper_bound` | O(log N) | |
| `count` | O(log N + K) | K = match count |
| `erase` | O(log N + N) | Find + vector erase |
| `size` | O(1) | |
| `empty` | O(1) | |

### Benchmark Results

**Individual vs Batch Insertion (10,000 elements):**

| Method | Time | Relative |
|--------|------|----------|
| Individual `insert()` | 45 ms | 1.0x |
| `insertRange()` | 2.3 ms | 19.5x faster |

**Lookup Performance (100,000 elements):**

| Operation | Time per op |
|-----------|-------------|
| `find()` | 85 ns |
| `lower_bound()` | 82 ns |
| `count()` (unique) | 90 ns |

**Iteration Performance (100,000 elements):**

| Method | Time |
|--------|------|
| `withInternalContainer()` | 0.8 ms |
| `toVector()` + iterate | 2.1 ms |

### Optimization Tips

1. **Reserve before batch inserts:**
   ```cpp
   container.reserve(size_type(expected_size));
   container.insertRange(data.begin(), data.end());
   ```

2. **Use insertRange for multiple elements** — One sort beats N individual inserts.

3. **Use withInternalContainer for read access** — Avoids copying.

---

## Comparison with Alternatives

### The C++ Sorted Container Landscape

**std::set / std::multiset:** Standard library tree-based containers. Introduced in C++98, they provide O(log N) operations with iterator stability. The node-based design means each element is a separate heap allocation.

**Sorted std::vector:** A common pattern where developers manually maintain sorted order. Provides contiguous storage and cache-friendly iteration but requires discipline to preserve the invariant.

**boost::container::flat_set:** Boost's sorted vector implementation. Similar goals to SortedContainer but with different design choices (exceptions, no fuzzy matching, no built-in thread safety).

### Feature Comparison

| Feature | SortedContainer | std::set | Sorted vector | boost::flat_set |
|---------|-----------------|----------|---------------|-----------------|
| Contiguous storage | ✅ | ❌ | ✅ | ✅ |
| Cache-friendly | ✅ | ❌ | ✅ | ✅ |
| O(log N) lookup | ✅ | ✅ | ✅ | ✅ |
| O(log N) insert | ❌ O(N) | ✅ | ❌ O(N) | ❌ O(N) |
| Iterator stability | ❌ | ✅ | ❌ | ❌ |
| Duplicate policy | ✅ Configurable | ❌ Fixed | Manual | ❌ Fixed |
| Fuzzy uniqueness | ✅ Built-in | ❌ | ❌ | ❌ |
| Thread-safe options | ✅ Built-in | ❌ | ❌ | ❌ |
| Expected errors | ✅ | ❌ Exceptions | N/A | ❌ Exceptions |
| Header-only | ✅ | ✅ | ✅ | ❌ |
| Dependencies | fat_p only | None | None | Boost |

### When to Choose Each

**Choose SortedContainer when:**
- Read-heavy workload (iteration, lookups dominate)
- Need contiguous storage (SIMD, C API interop)
- Need fuzzy duplicate detection
- Need built-in thread safety
- Cannot use exceptions

**Choose std::set when:**
- Write-heavy workload (frequent insert/erase)
- Need iterator stability after modifications
- Need worst-case O(log N) insert guarantee

**Choose sorted std::vector when:**
- Simple use case with minimal modifications
- Already have vector-based code to adapt
- Minimal dependencies required

---

## Migration Guide

### From std::set

**Before:**
```cpp
std::set<int> unique_ids;
unique_ids.insert(42);
unique_ids.insert(17);

auto it = unique_ids.find(42);
if (it != unique_ids.end()) {
    process(*it);
}
```

**After:**
```cpp
fat_p::SortedContainer<int, fat_p::OnlyUniquePolicy> unique_ids;
(void)unique_ids.insert(42);  // Note: returns Expected, not pair
(void)unique_ids.insert(17);

auto it = unique_ids.find(42);
if (it != unique_ids.end()) {
    process(*it);
}
```

**Key differences:**
- `insert()` returns `Expected<bool>`, not `std::pair<iterator, bool>`
- Use `(void)` to suppress `[[nodiscard]]` warnings if ignoring result
- Iterators are const-only
- No `emplace()` — use `insert()` with forwarding

### From std::multiset

**Before:**
```cpp
std::multiset<int> values;
values.insert(5);
values.insert(5);
values.insert(5);

size_t count = values.count(5);  // 3
```

**After:**
```cpp
fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy> values;
(void)values.insert(5);
(void)values.insert(5);
(void)values.insert(5);

auto count = values.count(5);  // StrongId<size_t>
size_t raw_count = static_cast<size_t>(count);  // 3
```

**Key difference:** `count()` returns `StrongId<size_t>`, not raw `size_t`.

### From Sorted std::vector

**Before:**
```cpp
std::vector<int> data;

void add(int value) {
    auto it = std::lower_bound(data.begin(), data.end(), value);
    data.insert(it, value);
}

void add_batch(const std::vector<int>& batch) {
    data.insert(data.end(), batch.begin(), batch.end());
    std::sort(data.begin(), data.end());
}
```

**After:**
```cpp
fat_p::SortedContainer<int> data;

void add(int value) {
    (void)data.insert(value);  // Automatic sorted insertion
}

void add_batch(const std::vector<int>& batch) {
    (void)data.insertRange(batch.begin(), batch.end());  // Optimized
}
```

### Incremental Adoption

1. **Start with single-threaded, AllowDuplicates** — Drop-in for sorted vector
2. **Add uniqueness policy** — Match your deduplication needs
3. **Add thread safety** — When ready for concurrent access
4. **Add fuzzy matching** — For floating-point data

---

## Best Practices

### Do

✅ **Check Expected return values:**
```cpp
auto result = container.insert(value);
if (!result) {
    handle_error(result.error());
}
```

✅ **Use insertRange for batches:**
```cpp
container.insertRange(data.begin(), data.end());  // One sort
```

✅ **Use withInternalContainer for thread-safe access:**
```cpp
container.withInternalContainer([](const auto& vec) {
    // Lock held during entire operation
});
```

✅ **Reserve before large insertions:**
```cpp
container.reserve(size_type(expected_count));
```

✅ **Use FuzzyUniquePolicy for floating-point:**
```cpp
SortedContainer<double, FuzzyUniquePolicy<HybridComparisonPolicy, double>> data;
```

### Don't

❌ **Ignore Expected returns:**
```cpp
container.insert(value);  // Compiler warning: [[nodiscard]]
```

❌ **Insert individually when you have a batch:**
```cpp
for (auto& x : batch) container.insert(x);  // Slow!
```

❌ **Use exact equality for floating-point uniqueness:**
```cpp
SortedContainer<double, OnlyUniquePolicy> data;  // 0.1 + 0.2 != 0.3
```

❌ **Hold iterators across operations in multi-threaded code:**
```cpp
auto it = container.begin();  // Lock released!
// ... other thread modifies ...
*it;  // Undefined behavior
```

---

## Troubleshooting

### "SortedContainer invariant violated"

**Symptom:** Assertion failure in debug builds.

**Cause:** The container's sorted order was corrupted.

**Solutions:**
1. Don't modify elements through exposed references
2. Ensure custom comparators define strict weak ordering
3. Check for data races in multi-threaded code without proper policy

### Thread-Safety Crashes

**Symptom:** Crashes, corrupted data, or assertion failures in multi-threaded code.

**Solutions:**
1. Use `MutexSynchronizationPolicy` or `SharedMutexPolicy`
2. Use `withInternalContainer()` instead of `begin()`/`end()`
3. Don't hold `toVector()` results across operations

### Fuzzy Uniqueness Not Working

**Symptom:** Duplicates within epsilon are not rejected.

**Cause:** Missing epsilon parameters.

```cpp
// WRONG: No epsilon
container.insert(1.0);        
container.insert(1.005);  // Not rejected!

// CORRECT: Provide epsilon
container.insert(1.0, 0.01, 0.01);
container.insert(1.005, 0.01, 0.01);  // Rejected
```

### Compilation Errors

**"no matching function for call to 'insert'"**

Check that your type is copyable or movable:
```cpp
static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>);
```

**"StrongId conversion"**

`size()` and `count()` return `StrongId<size_t>`. Cast explicitly:
```cpp
size_t raw_size = static_cast<size_t>(container.size());
```

---

## Summary

### Key Features

- **Always sorted** — Invariant maintained automatically
- **Policy-based** — Uniqueness, comparison, concurrency all configurable
- **Thread-safe options** — Mutex, shared mutex, spinlock policies
- **Fuzzy matching** — Built-in floating-point tolerance
- **Expected-based** — Error handling without exceptions
- **Contiguous storage** — Cache-friendly, SIMD-compatible

### Performance Profile

- **Lookups:** O(log N) — fast binary search
- **Individual insert:** O(N) — vector shift
- **Batch insert:** O(N log N) — optimized sorting
- **Iteration:** Very fast — contiguous memory

### Quick Start Code

```cpp
#include "SortedContainer.h"

int main() {
    using namespace fat_p;
    
    SortedContainer<int, OnlyUniquePolicy> ids;
    (void)ids.insert(42);
    (void)ids.insert(17);
    
    if (ids.find(42) != ids.end()) {
        // Found!
    }
    
    return 0;
}
```

### Related Components

- **Expected.h** — Error handling without exceptions
- **ConcurrencyPolicies.h** — Thread safety policies
- **EqualityComparisons.h** — Fuzzy comparison policies
- **enforce.h** — Debug-only invariant checking
