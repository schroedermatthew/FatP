---
doc_id: MG-FLATMAP-001
doc_type: "Migration Guide"
title: "Linear Search and bsearch to FlatMap"
from_pattern: "Linear search in arrays, qsort/bsearch, manual binary search"
to_component: "FlatMap"
fatp_version: "1.0"
cxx_standard: "C++20"
migration_complexity: "Low"
breaking_changes: false
last_verified: "2025-01-08"
---

# Migration Guide - Linear Search and bsearch to FlatMap

### *From O(N) Lookups to Cache-Friendly O(log N) Access*

*FAT-P Library — January 2025*

---

## Migration Guide Card

**From:** Linear search arrays, `qsort`/`bsearch`, manual binary search, parallel sorted arrays  
**To:** `FlatMap<Key, Value>` for cache-friendly sorted associative storage  
**Why migrate:** Linear search is O(N); `qsort`/`bsearch` require type-unsafe comparators and manual sorted-array maintenance  
**Compatibility strategy:** Drop-in — `FlatMap` provides standard associative container interface  
**Mechanical steps:**
1. Identify sorted arrays or linear-search lookups.
2. Replace with `FlatMap<Key, Value>`.
3. Replace manual insert-and-sort with `flat_map.insert()`.
4. Replace `bsearch` / manual binary search with `flat_map.find()`.
**Behavioral equivalence:** Same key-value associations; same lookup results  
**Intentional differences:** Sort order maintained automatically; type-checked comparisons; iterator stability on lookup  
**Failure model:** Missing key returns iterator-to-end (standard associative container semantics)  
**Threading model:** Not synchronized — external locking required for concurrent modification  
**Lifetime model:** Value semantics; elements owned by the container  
**Alternatives:** `std::flat_map` (C++23), `std::map`, Boost.Container `flat_map`  
**Verification:** Unit tests for insert/find/erase; benchmark vs linear search for target data sizes  
**Rollback plan:** Replace `FlatMap` with sorted array + `bsearch`; restore manual comparators

---

## Table of Contents

1. [The Problem with Sorted Array Lookup](#the-problem-with-sorted-array-lookup)
2. [Real-World Lookup Disasters](#real-world-lookup-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The FlatMap Solution](#the-flatmap-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When FlatMap Loses](#when-flatmap-loses)

---

## The Problem with Sorted Array Lookup

Small associative collections are everywhere: configuration maps, lookup tables, symbol tables, caches. The standard approaches have tradeoffs:

| Approach | Lookup | Insert | Cache | Type Safety |
|----------|--------|--------|-------|-------------|
| **Linear search** | O(N) | O(1) | Good | Manual |
| **std::map** | O(log N) | O(log N) | Poor | Good |
| **std::unordered_map** | O(1) avg | O(1) avg | Poor | Good |
| **Sorted array + bsearch** | O(log N) | O(N) | Excellent | Terrible |

For small to medium collections (N < 1000) with infrequent modifications, **sorted arrays beat trees and hash tables** due to cache locality. But C's `bsearch()` is a minefield:

```c
/* bsearch signature - spot the problems */
void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *));

/* Usage requires unsafe casts everywhere */
int compare_entry(const void *a, const void *b) {
    const Entry* ea = (const Entry*)a;  /* Unsafe cast */
    const Entry* eb = (const Entry*)b;  /* Another unsafe cast */
    return strcmp(ea->key, eb->key);
}

Entry* result = (Entry*)bsearch(&key, entries, count, sizeof(Entry), compare_entry);
```

---

## Real-World Lookup Disasters

### The strcmp Sign Bug

A classic `qsort`/`bsearch` comparator bug:

```c
int compare_int(const void *a, const void *b) {
    return *(int*)a - *(int*)b;  /* BUG: overflow on INT_MIN - INT_MAX */
}
/* qsort/bsearch with this comparator can corrupt data or crash */
```

### The Wrong Comparator

```c
/* Array sorted by name */
Entry entries[] = {
    {"alice", 100},
    {"bob", 200},
    {"charlie", 300}
};

/* But search uses different comparison! */
int compare_by_value(const void *a, const void *b) {
    return ((Entry*)a)->value - ((Entry*)b)->value;
}

/* This silently returns garbage */
Entry key = {NULL, 200};
Entry* result = bsearch(&key, entries, 3, sizeof(Entry), compare_by_value);
```

### The SQLite Config Lookup

SQLite uses a compile-time sorted array for configuration options:

```c
/* From sqlite3.c - must stay manually sorted! */
static const char *const azPragma[] = {
  "application_id",
  "auto_vacuum", 
  "automatic_index",
  /* ... 100+ more entries that MUST stay sorted ... */
  "writable_schema",
};
```

If a developer adds an entry out of order, binary search silently fails.

### The Performance Trap

```c
/* Looks O(1) but is O(N) */
const char* get_config(const char* key) {
    for (int i = 0; i < config_count; i++) {
        if (strcmp(config[i].key, key) == 0) {
            return config[i].value;
        }
    }
    return NULL;
}
/* Called 1000 times per request × 100 configs = 100,000 comparisons */
```

---

## The C Patterns

### Pattern 1: Linear Search

```c
typedef struct {
    const char* key;
    int value;
} Entry;

Entry table[] = {
    {"alpha", 1},
    {"beta", 2},
    {"gamma", 3}
};

int find_value(const char* key) {
    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        if (strcmp(table[i].key, key) == 0) {
            return table[i].value;
        }
    }
    return -1;  /* Not found */
}
```

**Problems:** O(N) lookup; must scan entire array.

### Pattern 2: qsort + bsearch

```c
int compare_entry(const void *a, const void *b) {
    return strcmp(((Entry*)a)->key, ((Entry*)b)->key);
}

void init_table(Entry* table, size_t count) {
    qsort(table, count, sizeof(Entry), compare_entry);
}

Entry* find_entry(Entry* table, size_t count, const char* key) {
    Entry search_key = {key, 0};  /* Construct dummy for search */
    return bsearch(&search_key, table, count, sizeof(Entry), compare_entry);
}
```

**Problems:**
- Type-unsafe (void* casts everywhere)
- Must construct dummy key object
- Comparator can't be inlined
- Easy to use wrong comparator
- No insert/remove support

### Pattern 3: Manual Binary Search

```c
int binary_search(Entry* table, size_t count, const char* key) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(table[mid].key, key);
        if (cmp < 0) {
            lo = mid + 1;
        } else if (cmp > 0) {
            hi = mid;
        } else {
            return mid;  /* Found */
        }
    }
    return -1;  /* Not found */
}
```

**Problems:**
- Off-by-one errors everywhere
- Must reimplement for each type
- No insert support

### Pattern 4: Parallel Sorted Arrays

```c
/* Keys in one array, values in another */
const char* keys[] = {"alpha", "beta", "gamma"};
int values[] = {1, 2, 3};

int find_value(const char* key) {
    /* Binary search in keys array */
    int idx = binary_search_string(keys, 3, key);
    if (idx >= 0) {
        return values[idx];  /* Index into parallel array */
    }
    return -1;
}
```

**Problems:**
- Arrays can get out of sync
- Must maintain both arrays
- No encapsulation

---

## The FlatMap Solution

### Core Concept

`FlatMap<Key, Value>` is a sorted `std::vector<std::pair<Key, Value>>` with an associative container interface. Binary search is automatic, type-safe, and inlinable.

```cpp
#include "FlatMap.h"
using namespace fat_p;

FlatMap<std::string, int> table = {
    {"alpha", 1},
    {"beta", 2},
    {"gamma", 3}
};

// O(log N) lookup with operator[]
int value = table["beta"];  // Returns 2

// Safe lookup with find()
if (auto it = table.find("delta"); it != table.end()) {
    use(it->second);
}

// Range-based iteration (cache-friendly!)
for (const auto& [key, value] : table) {
    std::cout << key << ": " << value << "\n";
}
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **Contiguous storage** | Cache-friendly iteration and lookup |
| **O(log N) lookup** | Binary search is automatic |
| **Type-safe** | No void* casts, comparator is template parameter |
| **Standard interface** | Compatible with std::map algorithms |
| **Inlinable comparison** | Comparator inlined by compiler |
| **Sorted iteration** | Keys always in order |

### When to Use FlatMap vs Alternatives

| Use FlatMap When | Use std::map When | Use std::unordered_map When |
|------------------|-------------------|----------------------------|
| N < ~1000 | N > ~10000 | Need O(1) average lookup |
| Frequent iteration | Frequent insert/erase | Don't need ordered iteration |
| Read-heavy workload | Write-heavy workload | Keys have good hash |
| Memory constrained | — | — |

### API Overview

```cpp
template<typename Key, typename T, 
         typename Compare = std::less<Key>,
         typename Allocator = std::allocator<std::pair<Key, T>>>
class FlatMap {
public:
    // Construction
    FlatMap();
    FlatMap(std::initializer_list<value_type> init);
    template<typename InputIt>
    FlatMap(InputIt first, InputIt last);
    
    // Pre-sorted construction (O(N) instead of O(N log N))
    template<typename InputIt>
    FlatMap(ordered_unique_range_t, InputIt first, InputIt last);
    
    // Element access
    T& operator[](const Key& key);
    T& at(const Key& key);  // Throws if not found
    
    // Lookup
    iterator find(const Key& key);
    bool contains(const Key& key) const;
    size_type count(const Key& key) const;
    
    // Bounds
    iterator lower_bound(const Key& key);
    iterator upper_bound(const Key& key);
    std::pair<iterator, iterator> equal_range(const Key& key);
    
    // Modifiers
    std::pair<iterator, bool> insert(const value_type& value);
    std::pair<iterator, bool> insert_or_assign(const Key& key, T&& value);
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args);
    iterator erase(const_iterator pos);
    size_type erase(const Key& key);
    void clear();
    
    // Capacity
    bool empty() const;
    size_type size() const;
    void reserve(size_type n);
    
    // Iterators
    iterator begin();
    iterator end();
};
```

---

## Migration Steps

### Step 1: Identify Lookup Tables

Find code using arrays for key-value lookup:

```bash
# Find bsearch usage
grep -rn "bsearch" src/

# Find linear search patterns
grep -rn "for.*strcmp.*==.*0" src/

# Find sorted array comments
grep -rn "must.*sort\|keep.*sorted" src/
```

### Step 2: Replace Array with FlatMap

**Before:**
```c
Entry table[] = {
    {"alpha", 1},
    {"beta", 2},
    {"gamma", 3}
};
const size_t table_size = sizeof(table) / sizeof(table[0]);
```

**After:**
```cpp
FlatMap<std::string, int> table = {
    {"alpha", 1},
    {"beta", 2},
    {"gamma", 3}
};
```

### Step 3: Replace bsearch with find()

**Before:**
```c
Entry key = {search_key, 0};
Entry* result = bsearch(&key, table, count, sizeof(Entry), compare);
if (result) {
    use_value(result->value);
}
```

**After:**
```cpp
if (auto it = table.find(search_key); it != table.end()) {
    use_value(it->second);
}
```

### Step 4: Replace Linear Search

**Before:**
```c
int find_value(const char* key) {
    for (int i = 0; i < count; i++) {
        if (strcmp(table[i].key, key) == 0) {
            return table[i].value;
        }
    }
    return -1;
}
```

**After:**
```cpp
std::optional<int> find_value(const std::string& key) {
    if (auto it = table.find(key); it != table.end()) {
        return it->second;
    }
    return std::nullopt;
}
```

### Step 5: Handle Pre-Sorted Data

If data is known to be sorted, avoid re-sorting:

```cpp
// Data from sorted source (database, file, etc.)
std::vector<std::pair<std::string, int>> sorted_data = load_sorted_data();

// Use ordered_unique_range tag to skip sorting
FlatMap<std::string, int> table(ordered_unique_range, 
                                 sorted_data.begin(), 
                                 sorted_data.end());
```

### Step 6: Optimize Hot Paths

For frequently-accessed keys, consider caching iterators:

```cpp
class ConfigManager {
    FlatMap<std::string, std::string> mConfig;
    FlatMap<std::string, std::string>::iterator mLogLevelIt;
    
public:
    void init() {
        mConfig = load_config();
        mLogLevelIt = mConfig.find("log_level");
    }
    
    const std::string& log_level() const {
        return mLogLevelIt->second;  // O(1) access
    }
};
```

---

## Before/After Examples

### Example 1: HTTP Header Lookup

**Before (O(N) per lookup):**
```c
typedef struct { const char* name; const char* value; } Header;

const char* find_header(Header* headers, size_t count, const char* name) {
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(headers[i].name, name) == 0) {
            return headers[i].value;
        }
    }
    return NULL;
}
```

**After (O(log N) per lookup):**
```cpp
struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const {
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(),
            [](char ca, char cb) { return tolower(ca) < tolower(cb); }
        );
    }
};

using HeaderMap = FlatMap<std::string, std::string, CaseInsensitiveLess>;

std::optional<std::string_view> find_header(const HeaderMap& headers, 
                                             const std::string& name) {
    if (auto it = headers.find(name); it != headers.end()) {
        return it->second;
    }
    return std::nullopt;
}
```

### Example 2: Enum-to-String Mapping

**Before:**
```c
const char* status_to_string(int status) {
    static const struct { int code; const char* name; } table[] = {
        {200, "OK"},
        {201, "Created"},
        {400, "Bad Request"},
        {404, "Not Found"},
        {500, "Internal Server Error"}
    };
    
    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        if (table[i].code == status) {
            return table[i].name;
        }
    }
    return "Unknown";
}
```

**After:**
```cpp
std::string_view status_to_string(int status) {
    static const FlatMap<int, std::string_view> table = {
        {200, "OK"},
        {201, "Created"},
        {400, "Bad Request"},
        {404, "Not Found"},
        {500, "Internal Server Error"}
    };
    
    if (auto it = table.find(status); it != table.end()) {
        return it->second;
    }
    return "Unknown";
}
```

### Example 3: Symbol Table

**Before:**
```c
typedef struct {
    char name[64];
    void* address;
    size_t size;
} Symbol;

Symbol symbols[MAX_SYMBOLS];
size_t symbol_count = 0;

void add_symbol(const char* name, void* addr, size_t size) {
    /* Must keep sorted for binary search */
    size_t pos = 0;
    while (pos < symbol_count && strcmp(symbols[pos].name, name) < 0) {
        pos++;
    }
    
    /* Shift elements */
    memmove(&symbols[pos + 1], &symbols[pos], 
            (symbol_count - pos) * sizeof(Symbol));
    
    strncpy(symbols[pos].name, name, 63);
    symbols[pos].address = addr;
    symbols[pos].size = size;
    symbol_count++;
}

Symbol* find_symbol(const char* name) {
    /* Binary search */
    size_t lo = 0, hi = symbol_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(symbols[mid].name, name);
        if (cmp < 0) lo = mid + 1;
        else if (cmp > 0) hi = mid;
        else return &symbols[mid];
    }
    return NULL;
}
```

**After:**
```cpp
struct SymbolInfo {
    void* address;
    size_t size;
};

FlatMap<std::string, SymbolInfo> symbols;

void add_symbol(std::string name, void* addr, size_t size) {
    symbols.insert_or_assign(std::move(name), SymbolInfo{addr, size});
}

SymbolInfo* find_symbol(const std::string& name) {
    if (auto it = symbols.find(name); it != symbols.end()) {
        return &it->second;
    }
    return nullptr;
}
```

---

## Advanced Patterns

### Pattern: Transparent Comparators

Avoid temporary string construction for lookups:

```cpp
struct StringViewLess {
    using is_transparent = void;  // Enable heterogeneous lookup
    
    bool operator()(std::string_view a, std::string_view b) const {
        return a < b;
    }
};

FlatMap<std::string, int, StringViewLess> table;

// Can search with string_view without constructing std::string
std::string_view key = get_key();
auto it = table.find(key);  // No allocation!
```

### Pattern: Bulk Construction

```cpp
// Building from sorted data (database result, etc.)
std::vector<std::pair<int, Data>> records = fetch_sorted_records();

// O(N) construction instead of O(N log N)
FlatMap<int, Data> cache(ordered_unique_range, 
                         records.begin(), 
                         records.end());
```

### Pattern: Range Queries

```cpp
FlatMap<std::string, int> table = {
    {"apple", 1}, {"apricot", 2}, {"banana", 3},
    {"blueberry", 4}, {"cherry", 5}
};

// Find all entries starting with "a"
auto begin = table.lower_bound("a");
auto end = table.lower_bound("b");

for (auto it = begin; it != end; ++it) {
    std::cout << it->first << "\n";  // apple, apricot
}
```

### Pattern: Merge Two FlatMaps

```cpp
FlatMap<std::string, int> merge(const FlatMap<std::string, int>& a,
                                 const FlatMap<std::string, int>& b) {
    std::vector<std::pair<std::string, int>> merged;
    merged.reserve(a.size() + b.size());
    
    std::merge(a.begin(), a.end(), b.begin(), b.end(),
               std::back_inserter(merged),
               [](const auto& x, const auto& y) { return x.first < y.first; });
    
    // Remove duplicates (keep first)
    merged.erase(std::unique(merged.begin(), merged.end(),
                             [](const auto& x, const auto& y) { 
                                 return x.first == y.first; 
                             }),
                 merged.end());
    
    return FlatMap<std::string, int>(ordered_unique_range,
                                      merged.begin(), merged.end());
}
```

---

## Verification

### Compile-Time Guarantees

```cpp
FlatMap<std::string, int> map;

// Type-safe: can't accidentally use wrong key type
// map.find(42);  // ERROR: no matching function

// Comparator is part of the type
FlatMap<std::string, int, std::greater<>> reverse_map;
// map = reverse_map;  // ERROR: different types
```

### Runtime Tests

```cpp
TEST_CASE(lookup_finds_existing) {
    FlatMap<std::string, int> map = {{"a", 1}, {"b", 2}, {"c", 3}};
    
    ASSERT_TRUE(map.contains("b"));
    ASSERT_EQ(map.at("b"), 2);
}

TEST_CASE(lookup_returns_end_for_missing) {
    FlatMap<std::string, int> map = {{"a", 1}};
    
    ASSERT_EQ(map.find("z"), map.end());
    ASSERT_FALSE(map.contains("z"));
}

TEST_CASE(maintains_sorted_order) {
    FlatMap<int, std::string> map;
    map[3] = "c";
    map[1] = "a";
    map[2] = "b";
    
    std::vector<int> keys;
    for (const auto& [k, v] : map) {
        keys.push_back(k);
    }
    
    ASSERT_EQ(keys, (std::vector<int>{1, 2, 3}));
}

TEST_CASE(ordered_unique_range_construction) {
    std::vector<std::pair<int, int>> sorted = {{1, 1}, {2, 2}, {3, 3}};
    FlatMap<int, int> map(ordered_unique_range, sorted.begin(), sorted.end());
    
    ASSERT_EQ(map.size(), 3u);
    ASSERT_EQ(map[2], 2);
}
```

### Performance Comparison

```cpp
void benchmark_lookup(size_t N) {
    // Setup
    std::vector<std::string> keys = generate_random_strings(N);
    
    FlatMap<std::string, int> flat;
    std::map<std::string, int> tree;
    std::unordered_map<std::string, int> hash;
    
    for (size_t i = 0; i < N; i++) {
        flat[keys[i]] = i;
        tree[keys[i]] = i;
        hash[keys[i]] = i;
    }
    
    // Benchmark random lookups
    // FlatMap wins for N < ~1000 due to cache locality
}
```

---

## When FlatMap Loses

### 1. Large Collections (N > ~1000)

O(log N) binary search in array vs O(log N) in tree:
- Tree nodes are scattered in memory → cache misses
- But at large N, tree structure overhead amortizes

```cpp
// For N > ~1000, consider std::map or std::unordered_map
```

### 2. Frequent Insertions/Deletions

Insert is O(N) due to element shifting:

```cpp
FlatMap<int, int> map;
for (int i = 0; i < 100000; i++) {
    map[rand()] = i;  // O(N) per insert → O(N²) total
}
// Use std::map for write-heavy workloads
```

### 3. Need O(1) Lookup

For truly performance-critical lookups:

```cpp
// Use std::unordered_map for O(1) average
std::unordered_map<std::string, int> hash_map;
```

### 4. Iterator Stability

FlatMap iterators invalidate on any modification:

```cpp
FlatMap<int, int> map = {{1, 1}, {2, 2}};
auto it = map.find(1);
map[3] = 3;  // May invalidate it!

// Use std::map if iterator stability is required
```

### 5. Large Values

FlatMap stores values inline. Large values waste memory when shifting:

```cpp
// Don't do this:
FlatMap<int, std::array<int, 1000>> huge_values;

// Do this instead:
FlatMap<int, std::unique_ptr<std::array<int, 1000>>> pointer_to_values;
```

---

## Summary

| Aspect | C Pattern | FlatMap |
|--------|-----------|---------|
| Lookup complexity | O(N) linear / O(log N) bsearch | O(log N) |
| Type safety | None (void* casts) | Full template safety |
| Comparator | Function pointer, not inlined | Template parameter, inlined |
| Cache locality | Excellent | Excellent |
| Insert complexity | O(N) manual | O(N) automatic |
| Sorted iteration | Manual maintenance | Automatic |

**When to choose FlatMap:**
- Small to medium collections (N < ~1000)
- Read-heavy workloads
- Need sorted iteration
- Memory efficiency matters

---

## References

- [Boost.Container flat_map](https://www.boost.org/doc/libs/release/doc/html/boost/container/flat_map.html)
- [C++23 std::flat_map](https://en.cppreference.com/w/cpp/container/flat_map)
- [Cache Effects on Sorted Containers](https://isocpp.org/files/papers/p0038r0.pdf)
- Fat-P User Manual: FlatMap — Complete API reference

---

*FAT-P Library Documentation — January 2025*
