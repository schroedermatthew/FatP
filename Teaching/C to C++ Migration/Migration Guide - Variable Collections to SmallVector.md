---
doc_id: MG-SMALLVECTOR-001
doc_type: "Migration Guide"
title: "Variable-Size Small Collections to SmallVector"
from_pattern: "Fixed array + count, alloca(), manual SBO, std::vector in hot paths"
to_component: "SmallVector"
fatp_version: "1.0"
cxx_standard: "C++20"
migration_complexity: "Low"
breaking_changes: false
last_verified: "2025-01-08"
fatp_components: ["SmallVector"]
topics: ["c-to-cpp", "migration", "small-buffer-optimization", "stack-allocation", "variable-length-arrays", "alloca"]
constraints: ["heap allocation in loops", "stack overflow", "fixed-array truncation", "cache locality"]
audience: ["C developers", "C++ developers", "AI assistants"]
status: "draft"
---

# Migration Guide - Variable-Size Small Collections to SmallVector

### *From Heap Allocation in Hot Paths to Zero-Cost Small Collections*

*FAT-P Library — January 2025*

---

## Scope

This guide targets C code that uses fixed-size arrays with count variables, `alloca()`, or manual small-buffer optimization for usually-small variable-length collections, and migrates those to `SmallVector<T, N>` with inline storage and heap fallback.

## Not covered

- Large, always-heap collections (use `std::vector` directly)
- Fixed-capacity containers with no heap fallback (see `std::inplace_vector` in C++26)
- Custom allocator integration with `SmallVector`

## Prerequisites

- Familiarity with C variable-length array patterns and `alloca()`
- Understanding of stack vs heap allocation tradeoffs

## Migration Guide Card

**From:** Fixed array + count, `alloca()`, manual small buffer optimization  
**To:** `SmallVector<T, InlineCapacity>` with stack-local storage and heap fallback  
**Why migrate:** Fixed arrays truncate at max size; `alloca()` risks stack overflow; manual SBO is error-prone and non-portable  
**Compatibility strategy:** Drop-in — `SmallVector` API matches `std::vector`  
**Mechanical steps:**
1. Identify fixed-size arrays used as variable-length collections.
2. Choose `InlineCapacity` based on typical element count.
3. Replace `T arr[MAX]; int count;` with `SmallVector<T, N>`.
4. Replace manual bounds tracking with `push_back()` / `size()`.
**Behavioral equivalence:** Same elements stored; same iteration order  
**Intentional differences:** No truncation — heap fallback for collections exceeding inline capacity  
**Failure model:** Allocation failure on heap fallback throws `std::bad_alloc` (standard vector semantics)  
**Threading model:** Unchanged — not synchronized; same thread-safety as `std::vector`  
**Lifetime model:** Container owns elements; destroyed on scope exit (RAII)  
**Alternatives:** `std::inplace_vector` (C++26), Boost.Container `small_vector`, LLVM `SmallVector`  
**Verification:** Unit tests for inline and heap paths; benchmark vs `std::vector` for target sizes  
**Rollback plan:** Replace `SmallVector<T, N>` with `std::vector<T>` or fixed array + count

---

## Alternatives

`std::inplace_vector<T, N>` (C++26 — fixed capacity, no heap fallback), Boost.Container `small_vector` (similar design, Boost dependency), LLVM `SmallVector` (requires LLVM headers), `std::vector` with `reserve()` (heap-only, no inline storage).

## Mapping: From → To

| C Pattern | C++ Replacement | Notes |
|-----------|----------------|-------|
| `T arr[MAX]; int count = 0;` | `SmallVector<T, MAX>` | No truncation; heap fallback if exceeded |
| `alloca(n * sizeof(T))` | `SmallVector<T, N>` with appropriate N | No stack overflow risk; heap fallback |
| Manual SBO (union + flag) | `SmallVector<T, N>` | Branchless access; no manual flag |
| `std::vector<T>` in hot loop | `SmallVector<T, N>` | Inline storage eliminates allocator calls |

## Compatibility and ABI boundaries

API matches `std::vector<T>`. At C boundaries, use `.data()` and `.size()` to pass as C array + length. No ABI concerns.

## Lifetime and ownership model

`SmallVector` owns its elements. Inline elements are on the stack; heap elements are owned by the container. Destruction frees heap storage and destroys all elements. Move semantics transfer ownership efficiently.

## Thread-safety and reentrancy

Same thread-safety as `std::vector` — not internally synchronized. Concurrent reads are permitted; concurrent modification requires external locking.

## Error and failure model

Heap fallback allocation failure throws `std::bad_alloc` (same as `std::vector`). Bounds-checked access via `at()` throws `std::out_of_range`. Unchecked access via `operator[]` has undefined behavior on out-of-bounds (same as `std::vector`).

## Rollback plan

Replace `SmallVector<T, N>` with `std::vector<T>` (loses inline storage) or restore fixed array + count pattern (loses dynamic sizing). Heap-avoidance benefit is lost on rollback to `std::vector`.

## Table of Contents

1. [The Problem: Usually-Small Collections](#the-problem-usually-small-collections)
2. [Real-World Allocation Disasters](#real-world-allocation-disasters)
3. [The C Patterns](#the-c-patterns)
4. [Choosing the Right Container](#choosing-the-right-container)
5. [The SmallVector Solution](#the-smallvector-solution)
6. [Migration Steps](#migration-steps)
7. [Before/After Examples](#beforeafter-examples)
8. [Advanced Patterns](#advanced-patterns)
9. [Verification](#verification)
10. [When SmallVector Loses](#when-smallvector-loses)

---

## The Problem: Usually-Small Collections

Many collections in real code are *usually* small but *occasionally* large:

- JSON arrays: 90% have ≤8 elements
- Graph adjacency lists: Most nodes have ≤6 neighbors
- Compiler IR operands: Most instructions have 1-3 operands
- Tokenizer results: Most lines have ≤16 tokens

The standard approaches force a bad tradeoff:

| Approach | Advantage | Disadvantage |
|----------|-----------|--------------|
| **Fixed array** | Zero allocation | Truncates or crashes if exceeded |
| **std::vector** | Grows as needed | Heap allocation every time |
| **alloca()** | Dynamic stack | No growth; stack overflow risk |

In performance-critical code, heap allocation dominates:

```cpp
void process_batch(const std::vector<Item>& items) {
    for (const auto& item : items) {
        std::vector<int> temp;  // Heap allocation!
        temp.reserve(8);        // Still heap allocation!
        
        for (int i = 0; i < item.count; i++) {
            temp.push_back(compute(item, i));
        }
        
        use(temp);
    }  // Deallocation
}
// 1 million items = 1 million malloc/free pairs
```

---

## Real-World Allocation Disasters

### The JSON Parser Bottleneck

Profiling a JSON parser revealed 40% of time spent in malloc:

```cpp
Value parse_array(Lexer& lexer) {
    std::vector<Value> elements;  // Heap allocation
    
    while (lexer.peek() != ']') {
        elements.push_back(parse_value(lexer));
        if (lexer.peek() == ',') lexer.advance();
    }
    
    return Value(std::move(elements));
}
// Parsing "[1,2,3]" does: malloc, possibly realloc, realloc...
```

Most JSON arrays contain fewer than 8 elements. SmallVector<Value, 8> eliminated 95% of allocations.

### The Game Engine Frame Spike

A game engine had mysterious frame time spikes:

```cpp
void update_entities(const std::vector<Entity>& entities) {
    for (auto& entity : entities) {
        std::vector<Component*> components;  // Allocation per entity!
        entity.get_components(components);
        
        for (auto* comp : components) {
            comp->update();
        }
    }
}
// 10,000 entities × 60 fps = 600,000 allocations/second
```

### The Truncation Bug

Fixed arrays silently truncate:

```c
#define MAX_RESULTS 64

void search(Query* q, Result results[], int* count) {
    *count = 0;
    for (/* ... */) {
        if (*count >= MAX_RESULTS) {
            break;  // Silent data loss!
        }
        results[(*count)++] = found;
    }
}
// User searches for common term, gets only first 64 results
```

### The alloca Stack Overflow

```c
void process(int n) {
    int* buffer = (int*)alloca(n * sizeof(int));
    // No failure indication!
    // Stack overflow if n is large
    // Undefined behavior waiting to happen
}
```

---

## The C Patterns

### Pattern 1: Fixed Array + Count (Truncating)

```c
#define MAX_ITEMS 64

struct Collection {
    Item items[MAX_ITEMS];
    int count;
};

void add_item(struct Collection* c, Item item) {
    if (c->count < MAX_ITEMS) {
        c->items[c->count++] = item;
    }
    // Silently drops item if full!
}
```

**Problems:** 
- Wastes space when count is small
- Silently truncates when count exceeds limit
- MAX_ITEMS is a guess that's often wrong

### Pattern 2: alloca() for Dynamic Stack

```c
void process(Item* items, int count) {
    int* results = alloca(count * sizeof(int));
    /* No failure indication! */
    /* Stack overflow is silent crash */
    
    for (int i = 0; i < count; i++) {
        results[i] = compute(items[i]);
    }
}
```

**Problems:**
- No failure detection—stack overflow crashes
- Non-portable (not in C standard)
- Cannot be used in loops (stack grows each iteration)
- Cannot grow if you need more space

### Pattern 3: Manual Small Buffer Optimization

```c
#define INLINE_CAPACITY 8

struct SmallArray {
    union {
        int inline_buffer[INLINE_CAPACITY];
        int* heap_buffer;
    };
    size_t size;
    size_t capacity;
    int is_heap;
};

void small_array_push(struct SmallArray* arr, int value) {
    if (arr->size >= arr->capacity) {
        size_t new_cap = arr->capacity * 2;
        int* new_data = malloc(new_cap * sizeof(int));
        
        if (arr->is_heap) {
            memcpy(new_data, arr->heap_buffer, arr->size * sizeof(int));
            free(arr->heap_buffer);
        } else {
            memcpy(new_data, arr->inline_buffer, arr->size * sizeof(int));
        }
        
        arr->heap_buffer = new_data;
        arr->capacity = new_cap;
        arr->is_heap = 1;
    }
    
    int* data = arr->is_heap ? arr->heap_buffer : arr->inline_buffer;
    data[arr->size++] = value;
}
```

**Problems:**
- Complex implementation (~50 lines for basic operations)
- Easy to get wrong (memory leaks, double-free)
- Branching on every access (is_heap check)
- Must reimplement for each type

### Pattern 4: std::vector in Hot Paths

```cpp
void hot_function() {
    for (int i = 0; i < 1000000; i++) {
        std::vector<int> temp;  // malloc
        temp.push_back(compute(i));
        use(temp);
    }  // free
}
// 1 million malloc/free pairs for single-element vectors
```

**Problems:**
- Heap allocation even for trivially small collections
- Allocation overhead dominates runtime

---

## Choosing the Right Container

Before reaching for SmallVector, consider whether it's the right tool:

| Situation | Correct Container | Reason |
|-----------|-------------------|--------|
| Size is always exactly N | `std::array<T, N>` | Compile-time fixed size |
| Size is always 0 or 1 | `std::optional<T>` | Semantic clarity |
| Size is usually ≤N, sometimes more | `SmallVector<T, N>` | ✓ This is SmallVector's niche |
| Size is unknown, often large | `std::vector<T>` | SmallVector wastes stack space |
| Need stable iterators/pointers | `std::vector<T>` or `std::list<T>` | SmallVector invalidates on growth |

**SmallVector is NOT a replacement for:**

```cpp
// Fixed-size buffer: use std::array
std::array<char, 256> fixed_buffer;

// Single optional value: use std::optional
std::optional<Config> maybe_config;

// Large collections: use std::vector
std::vector<Record> million_records;
```

**SmallVector IS for:**

```cpp
// Usually small, occasionally large
SmallVector<Value, 8> json_array_elements;
SmallVector<Node*, 4> tree_children;
SmallVector<Token, 16> line_tokens;

// Hot loop temporaries
for (const auto& item : items) {
    SmallVector<int, 8> temp;  // No allocation for ≤8 elements
    process(item, temp);
}
```

---

## The SmallVector Solution

### Core Concept

SmallVector stores up to `InlineCapacity` elements directly inside the object. When size exceeds this, it automatically transitions to heap storage—no truncation, no crash.

```cpp
#include "SmallVector.h"
using namespace fat_p;

void process_batch(const std::vector<Item>& items) {
    for (const auto& item : items) {
        SmallVector<int, 8> temp;  // Inline storage for ≤8 elements
        
        for (int i = 0; i < item.count; i++) {
            temp.push_back(compute(item, i));  // No allocation if count ≤ 8
        }
        
        use(temp);
    }
}
// 1 million items with count ≤ 8 = ZERO malloc calls
// Items with count > 8 still work (heap allocation)
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **Inline storage** | Zero heap allocation for small sizes |
| **Automatic growth** | Never truncates—grows to heap when needed |
| **std::vector API** | Drop-in replacement |
| **Pointer-based discrimination** | Branchless data access |
| **Move optimization** | O(1) move for heap storage |

### Memory Layout

```
SmallVector<int, 4>:
┌──────────────────────────────────────┐
│ data_ ───────────────────────┐       │  (points to inline or heap)
│ size_                        │       │
│ capacity_                    │       │
│ inline_buffer_: [_, _, _, _] ◄───────┤  (4 ints inline)
└──────────────────────────────────────┘

When inline (size ≤ 4): data_ == &inline_buffer_[0]
When heap (size > 4):   data_ == heap_allocated_memory
```

### API Overview (std::vector Compatible)

```cpp
template<typename T, size_t InlineCapacity, typename Allocator = std::allocator<T>>
class SmallVector {
public:
    // Construction
    SmallVector();
    explicit SmallVector(size_t count);
    SmallVector(size_t count, const T& value);
    SmallVector(std::initializer_list<T> init);
    
    // Element access
    T& operator[](size_t pos);
    T& at(size_t pos);  // Bounds-checked
    T& front();
    T& back();
    T* data();
    
    // Iterators
    iterator begin();
    iterator end();
    
    // Capacity
    bool empty() const;
    size_t size() const;
    size_t capacity() const;
    void reserve(size_t new_cap);
    void shrink_to_fit();
    
    // Modifiers
    void clear();
    void push_back(const T& value);
    void push_back(T&& value);
    template<typename... Args> T& emplace_back(Args&&... args);
    void pop_back();
    iterator insert(const_iterator pos, const T& value);
    iterator erase(const_iterator pos);
    void resize(size_t count);
};
```

---

## Migration Steps

### Step 1: Identify Hot Allocation Sites

Profile to find allocation-heavy code:

```bash
# Using perf
perf record -g ./program
perf report  # Look for malloc/new in hot paths

# Using Valgrind
valgrind --tool=massif ./program
ms_print massif.out.*
```

### Step 2: Analyze Size Distribution

Add instrumentation to understand typical sizes:

```cpp
// Temporary instrumentation
static std::map<size_t, size_t> size_histogram;
void track_size(size_t n) {
    size_histogram[n]++;
}

// Analyze: what's the 90th percentile size?
```

### Step 3: Replace Fixed Array + Count

**Before:**
```c
#define MAX_SIZE 64
int buffer[MAX_SIZE];
int count = 0;

void add(int value) {
    if (count < MAX_SIZE) {
        buffer[count++] = value;
    }
    // Truncates!
}
```

**After:**
```cpp
SmallVector<int, 64> buffer;

void add(int value) {
    buffer.push_back(value);  // Grows if needed, never truncates
}
```

### Step 4: Replace std::vector in Hot Paths

**Before:**
```cpp
for (const auto& item : items) {
    std::vector<int> temp;  // Heap allocation
    process(item, temp);
}
```

**After:**
```cpp
for (const auto& item : items) {
    SmallVector<int, 16> temp;  // Stack for common case
    process(item, temp);
}
```

### Step 5: Replace alloca()

**Before:**
```cpp
void process(size_t n) {
    int* temp = (int*)alloca(n * sizeof(int));
    // Dangerous: no growth, stack overflow risk
}
```

**After:**
```cpp
void process(size_t n) {
    SmallVector<int, 256> temp;  // Stack for n ≤ 256
    temp.resize(n);              // Heap for n > 256, safely
}
```

### Step 6: Choose InlineCapacity

| Use Case | Recommended Capacity |
|----------|---------------------|
| JSON array elements | 8-16 |
| Graph adjacency lists | 4-8 |
| Compiler IR operands | 2-4 |
| String tokenization | 16-32 |
| Path components | 8-16 |
| Command-line args | 8 |

**Rule of thumb:** Profile your workload, choose the 90th percentile size.

---

## Before/After Examples

### Example 1: Tokenizer

**Before (allocation per call):**
```cpp
std::vector<std::string_view> tokenize(std::string_view input) {
    std::vector<std::string_view> tokens;  // Heap allocation
    
    size_t start = 0;
    for (size_t i = 0; i <= input.size(); i++) {
        if (i == input.size() || input[i] == ' ') {
            if (i > start) {
                tokens.push_back(input.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    
    return tokens;
}
```

**After (zero allocation for typical input):**
```cpp
SmallVector<std::string_view, 16> tokenize(std::string_view input) {
    SmallVector<std::string_view, 16> tokens;  // Inline for ≤16 tokens
    
    size_t start = 0;
    for (size_t i = 0; i <= input.size(); i++) {
        if (i == input.size() || input[i] == ' ') {
            if (i > start) {
                tokens.push_back(input.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    
    return tokens;
}
```

### Example 2: Graph Traversal

**Before:**
```cpp
void bfs(const Graph& graph, int start) {
    std::queue<int> frontier;
    std::unordered_set<int> visited;
    
    frontier.push(start);
    while (!frontier.empty()) {
        int node = frontier.front();
        frontier.pop();
        
        std::vector<int> neighbors;  // Heap allocation per node!
        graph.get_neighbors(node, neighbors);
        
        for (int neighbor : neighbors) {
            if (visited.insert(neighbor).second) {
                frontier.push(neighbor);
            }
        }
    }
}
```

**After:**
```cpp
void bfs(const Graph& graph, int start) {
    std::queue<int> frontier;
    std::unordered_set<int> visited;
    
    frontier.push(start);
    while (!frontier.empty()) {
        int node = frontier.front();
        frontier.pop();
        
        SmallVector<int, 8> neighbors;  // Most nodes have ≤8 neighbors
        graph.get_neighbors(node, neighbors);
        
        for (int neighbor : neighbors) {
            if (visited.insert(neighbor).second) {
                frontier.push(neighbor);
            }
        }
    }
}
```

### Example 3: Compiler IR

**Before:**
```cpp
class Instruction {
    std::vector<Value*> operands;  // Most instructions have 1-3 operands
public:
    void add_operand(Value* v) {
        operands.push_back(v);
    }
};
// Millions of instructions = millions of allocations
```

**After:**
```cpp
class Instruction {
    SmallVector<Value*, 4> operands;  // Inline for ≤4 operands
public:
    void add_operand(Value* v) {
        operands.push_back(v);
    }
};
// Millions of instructions = nearly zero allocations
```

---

## Advanced Patterns

### Pattern: Return Value Optimization

```cpp
// SmallVector enables efficient return-by-value
SmallVector<int, 16> compute_factors(int n) {
    SmallVector<int, 16> factors;
    for (int i = 1; i * i <= n; i++) {
        if (n % i == 0) {
            factors.push_back(i);
            if (i != n / i) factors.push_back(n / i);
        }
    }
    return factors;  // No heap allocation for most numbers
}
```

### Pattern: Reusable Scratch Buffer

```cpp
class Processor {
    SmallVector<char, 4096> scratch_;  // Reusable scratch space
    
public:
    void process(const Data& data) {
        scratch_.clear();  // Reset but keep inline capacity
        scratch_.resize(data.required_size());
        
        // Use scratch_.data() as temporary buffer
        transform(data, scratch_.data());
    }
};
```

### Pattern: Output Parameter

```cpp
// Efficient output parameter—caller controls inline capacity
template<size_t N>
void get_neighbors(int node, SmallVector<int, N>& out) {
    out.clear();
    for (const auto& edge : adjacency_[node]) {
        out.push_back(edge.target);
    }
}

// Caller chooses appropriate capacity based on expected degree
SmallVector<int, 4> sparse_neighbors;
SmallVector<int, 64> dense_neighbors;
```

---

## Verification

### Compile-Time Guarantees

```cpp
// Capacity is part of the type
SmallVector<int, 8> a;
SmallVector<int, 16> b;

// a = b;  // ERROR: different types

// Use same capacity or template functions
template<size_t N>
void process(SmallVector<int, N>& v);
```

### Runtime Tests

```cpp
TEST_CASE(inline_storage_no_allocation) {
    SmallVector<int, 8> v;
    void* inline_addr = v.data();
    
    for (int i = 0; i < 8; i++) {
        v.push_back(i);
    }
    
    // Data pointer should still be inline buffer
    ASSERT_EQ(v.data(), inline_addr);
}

TEST_CASE(transition_to_heap_on_overflow) {
    SmallVector<int, 4> v;
    void* inline_addr = v.data();
    
    for (int i = 0; i < 10; i++) {
        v.push_back(i);
    }
    
    // Data pointer should now be heap (different from inline)
    ASSERT_NE(v.data(), inline_addr);
    ASSERT_EQ(v.size(), 10u);
    
    // All elements preserved
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(v[i], i);
    }
}

TEST_CASE(never_truncates) {
    SmallVector<int, 4> v;
    
    for (int i = 0; i < 1000; i++) {
        v.push_back(i);
    }
    
    ASSERT_EQ(v.size(), 1000u);  // All elements stored
}
```

### Allocation Tracking

```cpp
// Custom allocator to verify zero allocations for small sizes
template<typename T>
struct CountingAllocator : std::allocator<T> {
    static size_t alloc_count;
    T* allocate(size_t n) {
        alloc_count++;
        return std::allocator<T>::allocate(n);
    }
};

TEST_CASE(no_allocation_within_capacity) {
    CountingAllocator<int>::alloc_count = 0;
    
    SmallVector<int, 8, CountingAllocator<int>> v;
    for (int i = 0; i < 8; i++) {
        v.push_back(i);
    }
    
    ASSERT_EQ(CountingAllocator<int>::alloc_count, 0u);
}
```

---

## When SmallVector Loses

### 1. Fixed-Size Arrays

If size is always exactly N, use `std::array`:

```cpp
// Don't do this:
SmallVector<int, 3> rgb_color;  // Always exactly 3 elements

// Do this:
std::array<int, 3> rgb_color;
```

### 2. Consistently Large Collections

If your vectors are always large, SmallVector wastes stack space:

```cpp
// Don't do this:
SmallVector<int, 16> v;  // But v always has 10,000+ elements

// Do this:
std::vector<int> v;  // No wasted inline storage
```

### 3. Memory-Constrained Environments

Inline capacity increases object size:

```cpp
sizeof(std::vector<int>)        // ~24 bytes
sizeof(SmallVector<int, 8>)     // ~24 + 32 = ~56 bytes
sizeof(SmallVector<int, 64>)    // ~24 + 256 = ~280 bytes
```

### 4. Need Iterator Stability

SmallVector invalidates iterators on any growth:

```cpp
SmallVector<int, 4> v = {1, 2, 3, 4};
auto it = v.begin();
v.push_back(5);  // May reallocate, invalidating it!

// Use std::vector with reserve() if stability needed
```

### 5. Different-Capacity Interoperability

SmallVectors with different capacities are different types:

```cpp
SmallVector<int, 8> a;
SmallVector<int, 16> b;

void process(SmallVector<int, 8>& v);
process(b);  // ERROR: type mismatch

// Solution: template or use std::span
void process(std::span<int> v);
process(a);  // OK
process(b);  // OK
```

---

## Summary

| Aspect | C Pattern | SmallVector |
|--------|-----------|-------------|
| Small collection allocation | Fixed array (truncates) or heap | Zero (inline storage) |
| Large collection handling | Truncate/crash or heap always | Automatic heap transition |
| Safety | Buffer overflow, stack overflow | Bounds-checked, safe growth |
| API complexity | Manual memory management | std::vector compatible |
| Cache locality | Good (fixed) or poor (heap) | Excellent for common case |

**Use SmallVector when:**
- Size is *usually* small but *might* be large
- You have heap allocation in hot loops
- You're replacing fixed array + count patterns
- You're replacing manual SBO implementations

**Don't use SmallVector when:**
- Size is always exactly N → use `std::array`
- Size is always large → use `std::vector`
- You need iterator stability → use `std::vector`

---

## References

- [LLVM SmallVector](https://llvm.org/docs/ProgrammersManual.html#llvm-adt-smallvector-h)
- [Folly small_vector](https://github.com/facebook/folly/blob/main/folly/small_vector.h)
- [Boost.Container small_vector](https://www.boost.org/doc/libs/release/doc/html/container/non_standard_containers.html)
- Fat-P User Manual: SmallVector — Complete API reference

---

*FAT-P Library Documentation — January 2025*
