# BitSet User Manual

## Table of Contents

1. [What is BitSet?](#what-is-bitset)
2. [Core Architecture](#core-architecture)
3. [Getting Started](#getting-started)
4. [Single-Bit Operations](#single-bit-operations)
5. [Bulk Operations](#bulk-operations)
6. [Range Operations](#range-operations)
7. [Find Operations](#find-operations)
8. [Set-Theoretic Operations](#set-theoretic-operations)
9. [Bitwise Operations](#bitwise-operations)
10. [Iteration](#iteration)
11. [Performance](#performance)
12. [Comparison with std::bitset](#comparison-with-stdbitset)
13. [Migration Guide](#migration-guide)
14. [Best Practices](#best-practices)
15. [Troubleshooting](#troubleshooting)
16. [Summary](#summary)

---

## What is BitSet?

### The Problem

When working with large sets of boolean flags, you have limited options in C++:

```cpp
// Option 1: Vector of bools - wastes 7 bits per element
std::vector<bool> flags(10000);  // Implementation-defined, often packed but slow

// Option 2: Array of bools - definitely wastes 7 bits per element
bool flags[10000];  // 10KB instead of ~1.25KB

// Option 3: std::bitset - fixed size, no find operations
std::bitset<10000> flags;
// How do I iterate over only the SET bits?
// How do I find the first set bit efficiently?
for (size_t i = 0; i < 10000; ++i) {  // Must scan ALL bits
    if (flags[i]) { /* ... */ }
}
```

These approaches share common problems:

| Problem | Impact |
|---------|--------|
| Memory waste | 8x overhead with `bool` arrays |
| No efficient sparse iteration | Must scan all N bits to find set ones |
| Missing find operations | `std::bitset` lacks `find_first()`, `find_next()` |
| No range operations | Cannot set/clear/count a contiguous range |
| No set operations | Cannot test subset/intersection directly |

### The Solution

`fat_p::BitSet<N>` is a fixed-size bit set optimized for HPC and scientific computing:

```cpp
#include "BitSet.h"

fat_p::BitSet<10000> flags;

flags.set(42);
flags.set(1337);
flags.set(9999);

// Iterate ONLY over set bits - O(k) where k = number of set bits
for (size_t idx : flags) {
    std::cout << "Flag " << idx << " is set\n";
}
// Output: Flag 42, Flag 1337, Flag 9999 (only 3 iterations, not 10000)

// Find operations using hardware instructions
size_t first = flags.find_first();   // 42, using CTZ instruction
size_t last = flags.find_last();     // 9999, using CLZ instruction

// Range operations
flags.set_range(100, 200);           // Set 100 bits in one call
size_t count = flags.count_range(0, 500);  // Count bits in subrange
```

### Where This Fits

`fat_p::BitSet` occupies a specific niche:

```
                    Dynamic Size          Fixed Size
                   ┌─────────────────┬─────────────────┐
    General Use    │ std::vector<bool>│ std::bitset<N>  │
                   │ boost::dynamic_  │                 │
                   │   bitset         │                 │
                   ├─────────────────┼─────────────────┤
    HPC / Sparse   │ Roaring Bitmaps │ fat_p::BitSet<N>│
    Iteration      │ (compressed)    │                 │
                   └─────────────────┴─────────────────┘
```

Choose `fat_p::BitSet` when:
- Size is known at compile time
- You need efficient iteration over set bits
- You need find first/last/next operations
- You need range operations
- You want hardware-accelerated population count

---

## Core Architecture

### Memory Layout

`BitSet<N>` stores bits in an array of 64-bit words:

```
BitSet<200> layout:

Word 0 (bits 0-63)    Word 1 (bits 64-127)   Word 2 (bits 128-191)  Word 3 (bits 192-199)
┌────────────────────┬────────────────────┬────────────────────┬────────────────────┐
│ 0 1 2 ... 62 63    │ 64 65 ... 126 127  │ 128 129 ... 190 191│ 192 ... 199 [pad] │
└────────────────────┴────────────────────┴────────────────────┴────────────────────┘
                                                                 ↑
                                                           Unused bits always 0
```

Key implementation details:

| Aspect | Value | Rationale |
|--------|-------|-----------|
| Word size | 64 bits | Matches native register size on modern CPUs |
| Word count | `(N + 63) / 64` | Rounded up to hold all bits |
| Unused bits | Always zero | Simplifies comparison and count operations |
| Memory | `ceil(N/64) * 8` bytes | Plus object overhead |

### Hardware Acceleration

BitSet leverages compiler intrinsics that map directly to CPU instructions:

| Operation | GCC/Clang | MSVC | CPU Instruction | Latency |
|-----------|-----------|------|-----------------|---------|
| Population count | `__builtin_popcountll` | `__popcnt64` | POPCNT | ~3 cycles |
| Count trailing zeros | `__builtin_ctzll` | `_tzcnt_u64` | TZCNT/BSF | ~3 cycles |
| Count leading zeros | `__builtin_clzll` | `_lzcnt_u64` | LZCNT/BSR | ~3 cycles |

These instructions process 64 bits in a single operation, making `count()`, `find_first()`, and iteration dramatically faster than bit-by-bit approaches.

### Invariants

The class maintains one critical invariant:

> **Unused bits in the last word are always zero.**

This is enforced by `set_all()`, `flip_all()`, and the NOT operator. If you use `data()` to modify words directly, you must maintain this invariant.

---

## Getting Started

### Prerequisites

- C++17 or later
- A compiler with intrinsics support (GCC, Clang, MSVC)

### Integration

BitSet is header-only. Simply include the header:

```cpp
#include "BitSet.h"
```

### Compilation

```bash
# GCC/Clang - enable POPCNT instruction
g++ -std=c++17 -O2 -mpopcnt your_code.cpp

# MSVC
cl /std:c++17 /O2 your_code.cpp
```

### First Program

```cpp
#include <iostream>
#include "BitSet.h"

int main()
{
    // Create a bitset for tracking 1000 flags
    fat_p::BitSet<1000> flags;
    
    // Set some flags
    flags.set(42);
    flags.set(100);
    flags.set(999);
    
    // Check flag status
    if (flags.test(42)) {
        std::cout << "Flag 42 is set\n";
    }
    
    // Count set flags
    std::cout << "Total flags set: " << flags.count() << "\n";
    
    // Iterate over set flags only
    std::cout << "Set flags: ";
    for (size_t idx : flags) {
        std::cout << idx << " ";
    }
    std::cout << "\n";
    
    return 0;
}
```

Output:
```
Flag 42 is set
Total flags set: 3
Set flags: 42 100 999
```

---

## Single-Bit Operations

### What Are Single-Bit Operations?

Operations that read or modify exactly one bit at a specific index.

### Checked vs. Unchecked

BitSet provides two variants of each operation:

| Variant | Bounds check | Exception | Use case |
|---------|--------------|-----------|----------|
| Checked | Yes | `std::out_of_range` | General use, debugging |
| Unchecked | No | Undefined behavior | HPC inner loops |

### API Reference

#### `set(index)` / `set_unchecked(index)`

Sets bit at `index` to 1.

```cpp
fat_p::BitSet<64> bits;

bits.set(10);           // Checked - throws if index >= 64
bits.set_unchecked(20); // Unchecked - UB if index >= 64
```

#### `set(index, value)`

Sets bit to specified boolean value.

```cpp
bool should_enable = compute_something();
bits.set(5, should_enable);  // Equivalent to: if (should_enable) set(5); else clear(5);
```

#### `clear(index)` / `clear_unchecked(index)`

Sets bit at `index` to 0.

```cpp
bits.set(10);
bits.clear(10);  // Bit 10 is now 0
```

#### `flip(index)` / `flip_unchecked(index)`

Toggles bit (0→1 or 1→0).

```cpp
bits.flip(10);  // If was 0, now 1; if was 1, now 0
```

#### `test(index)` / `test_unchecked(index)`

Returns true if bit is 1.

```cpp
if (bits.test(10)) {
    // Bit 10 is set
}
```

#### `operator[](index)`

Unchecked read access (like `test_unchecked`).

```cpp
bool value = bits[10];  // Same as test_unchecked(10)
```

### Performance Note

In tight loops where indices are known valid, use unchecked operations:

```cpp
// Slow - bounds check on every iteration
for (size_t i = 0; i < 1000; ++i) {
    bits.set(i);  // Branch + potential throw
}

// Fast - no bounds checks
for (size_t i = 0; i < 1000; ++i) {
    bits.set_unchecked(i);  // Direct bit manipulation
}
```

---

## Bulk Operations

### What Are Bulk Operations?

Operations that affect all bits in the set simultaneously.

### API Reference

#### `set_all()`

Sets all N bits to 1.

```cpp
fat_p::BitSet<100> bits;
bits.set_all();
// bits.count() == 100
// bits.all() == true
```

#### `clear_all()` / `reset()`

Sets all bits to 0. `reset()` is an alias for `std::bitset` compatibility.

```cpp
bits.clear_all();  // or bits.reset();
// bits.count() == 0
// bits.none() == true
```

#### `flip_all()`

Toggles every bit.

```cpp
fat_p::BitSet<8> bits;
bits.set(0);
bits.set(2);
// bits: 00000101

bits.flip_all();
// bits: 11111010
```

### Query Operations

#### `count()`

Returns number of set bits using POPCNT instruction.

```cpp
size_t num_set = bits.count();
```

#### `any()`

Returns true if at least one bit is set.

```cpp
if (bits.any()) {
    // At least one flag is active
}
```

#### `none()`

Returns true if no bits are set.

```cpp
if (bits.none()) {
    // All flags are inactive
}
```

#### `all()`

Returns true if every bit is set.

```cpp
if (bits.all()) {
    // Every flag is active
}
```

#### `size()`

Returns the template parameter N (compile-time constant).

```cpp
constexpr size_t capacity = bits.size();  // Always N
```

---

## Range Operations

### What Are Range Operations?

Operations that affect a contiguous span of bits `[start, end)`. The range is half-open: `start` is included, `end` is excluded.

### Why Range Operations?

Without range operations, setting 1000 contiguous bits requires 1000 function calls:

```cpp
// Slow - 1000 function calls
for (size_t i = 1000; i < 2000; ++i) {
    bits.set(i);
}

// Fast - single function call, optimized word-level operations
bits.set_range(1000, 2000);
```

### API Reference

#### `set_range(start, end)`

Sets all bits in `[start, end)` to 1.

```cpp
fat_p::BitSet<256> bits;
bits.set_range(10, 20);  // Sets bits 10, 11, 12, ..., 19
// bits.count() == 10
```

#### `clear_range(start, end)`

Sets all bits in `[start, end)` to 0.

```cpp
bits.set_all();
bits.clear_range(100, 150);
// bits.count() == 206
```

#### `count_range(start, end)`

Counts set bits in `[start, end)`.

```cpp
bits.set_range(0, 100);
bits.set_range(200, 256);
size_t count = bits.count_range(50, 250);  // Counts bits 50-99 and 200-249
// count == 100
```

### Edge Cases

```cpp
// Empty range - no effect
bits.set_range(50, 50);

// Full range - same as set_all()
bits.set_range(0, N);

// Invalid ranges throw std::out_of_range
bits.set_range(100, 50);   // start > end: throws
bits.set_range(0, N + 1);  // end > N: throws
```

---

## Find Operations

### What Are Find Operations?

Operations that locate the position of set bits without scanning every bit.

### Why Find Operations?

Scanning all bits to find set ones is O(N):

```cpp
// O(N) - must check every bit
size_t find_first_slow(const std::bitset<N>& bits) {
    for (size_t i = 0; i < N; ++i) {
        if (bits[i]) return i;
    }
    return N;
}
```

Find operations use CTZ/CLZ instructions for O(N/64) performance:

```cpp
// O(N/64) - checks 64 bits per instruction
size_t first = bits.find_first();
```

### API Reference

#### `find_first()`

Returns index of first set bit, or N if none.

```cpp
fat_p::BitSet<1000> bits;
bits.set(500);
bits.set(700);

size_t first = bits.find_first();  // 500
```

#### `find_next(after)`

Returns index of next set bit after `after`, or N if none.

```cpp
size_t pos = bits.find_first();
while (pos != bits.size()) {
    process(pos);
    pos = bits.find_next(pos);
}
```

#### `find_last()`

Returns index of last set bit, or N if none.

```cpp
size_t last = bits.find_last();  // 700
```

### Return Value Convention

All find operations return N (the size) when no bit is found:

```cpp
fat_p::BitSet<64> empty;
size_t result = empty.find_first();  // Returns 64, not -1 or SIZE_MAX
if (result == empty.size()) {
    // No bits set
}
```

This convention enables clean iteration patterns and avoids sentinel value confusion.

---

## Set-Theoretic Operations

### What Are Set-Theoretic Operations?

Operations that treat bitsets as mathematical sets and test relationships between them.

### API Reference

#### `is_subset_of(other)`

Returns true if every bit set in `this` is also set in `other`.

```cpp
fat_p::BitSet<64> a{1, 2, 3};
fat_p::BitSet<64> b{1, 2, 3, 4, 5};
fat_p::BitSet<64> c{1, 2, 10};

a.is_subset_of(b);  // true - all of a's bits are in b
b.is_subset_of(a);  // false - b has bits not in a
a.is_subset_of(c);  // false - a has bit 3, c doesn't
```

Mathematical notation: `a ⊆ b`

#### `intersects(other)`

Returns true if any bit is set in both sets.

```cpp
fat_p::BitSet<64> a{1, 2, 3};
fat_p::BitSet<64> b{3, 4, 5};
fat_p::BitSet<64> c{10, 20};

a.intersects(b);  // true - both have bit 3
a.intersects(c);  // false - no common bits
```

Mathematical notation: `a ∩ b ≠ ∅`

### Use Cases

```cpp
// Permission checking
fat_p::BitSet<64> required_permissions{PERM_READ, PERM_WRITE};
fat_p::BitSet<64> user_permissions = get_user_perms();

if (required_permissions.is_subset_of(user_permissions)) {
    // User has all required permissions
}

// Collision detection groups
fat_p::BitSet<32> player_collision_mask{LAYER_ENEMY, LAYER_PROJECTILE};
fat_p::BitSet<32> object_layers{LAYER_ENEMY};

if (player_collision_mask.intersects(object_layers)) {
    // Object can collide with player
}
```

---

## Bitwise Operations

### What Are Bitwise Operations?

Operations that combine two bitsets or invert a bitset using boolean logic.

### API Reference

#### `operator&` / `operator&=` (AND)

Result has bit set only where both operands have it set.

```cpp
fat_p::BitSet<8> a{0, 1, 2};     // 00000111
fat_p::BitSet<8> b{1, 2, 3};     // 00001110
auto result = a & b;             // 00000110 (bits 1, 2)
```

#### `operator|` / `operator|=` (OR)

Result has bit set where either operand has it set.

```cpp
auto result = a | b;             // 00001111 (bits 0, 1, 2, 3)
```

#### `operator^` / `operator^=` (XOR)

Result has bit set where exactly one operand has it set.

```cpp
auto result = a ^ b;             // 00001001 (bits 0, 3)
```

#### `operator~` (NOT)

Result has each bit inverted.

```cpp
fat_p::BitSet<8> a{0, 1, 2};     // 00000111
auto result = ~a;                // 11111000 (bits 3, 4, 5, 6, 7)
```

### Compound Assignment

Compound operators modify the left operand in place:

```cpp
fat_p::BitSet<64> accumulator;
fat_p::BitSet<64> new_flags{10, 20, 30};

accumulator |= new_flags;  // Add flags
accumulator &= mask;       // Filter flags
accumulator ^= toggle;     // Toggle flags
```

### Comparison Operators

#### `operator==` / `operator!=`

Tests bit-by-bit equality.

```cpp
fat_p::BitSet<64> a{1, 2, 3};
fat_p::BitSet<64> b{1, 2, 3};
fat_p::BitSet<64> c{1, 2};

a == b;  // true
a != c;  // true
```

---

## Iteration

### What Is Iteration?

BitSet provides STL-compatible iterators that yield indices of set bits only.

### Why Sparse Iteration Matters

Consider a sparse bitset with 3 bits set out of 10,000:

```cpp
// Dense iteration - O(N), wastes time on zeros
for (size_t i = 0; i < 10000; ++i) {
    if (bits.test(i)) process(i);
}
// 10,000 iterations, 3 useful

// Sparse iteration - O(k) where k = set bits
for (size_t idx : bits) {
    process(idx);
}
// 3 iterations, 3 useful
```

### Usage

#### Range-Based For Loop

```cpp
fat_p::BitSet<1000> bits{10, 100, 500};

for (size_t idx : bits) {
    std::cout << idx << "\n";
}
// Output: 10, 100, 500
```

#### Explicit Iterator Use

```cpp
auto it = bits.begin();
auto end = bits.end();

while (it != end) {
    size_t idx = *it;
    ++it;
    // process idx
}
```

### Iterator Properties

| Property | Value |
|----------|-------|
| Category | Forward iterator |
| Value type | `size_t` |
| Dereference | Returns index by value |
| Invalidation | Modifying the bitset invalidates iterators |

### STL Algorithm Compatibility

```cpp
#include <algorithm>
#include <iterator>

fat_p::BitSet<100> bits{5, 10, 15, 20};

// Count iterations (same as bits.count(), but demonstrates compatibility)
size_t n = std::distance(bits.begin(), bits.end());

// Copy to vector
std::vector<size_t> indices(bits.begin(), bits.end());

// Find specific index
auto it = std::find(bits.begin(), bits.end(), 15);
if (it != bits.end()) {
    // Found bit 15
}
```

---

## Performance

### Benchmark Environment

| Component | Specification |
|-----------|---------------|
| Processor | Intel Core i7-8850H @ 2.60 GHz |
| RAM | 32 GB |
| Compiler | GCC 11 with `-O2 -mpopcnt` |
| OS | Ubuntu 24.04 |

### Benchmark Results

| Operation | BitSet<1024> | Notes |
|-----------|--------------|-------|
| Set 1024 bits (unchecked) | 1.34 µs | ~1.3 ns per bit |
| Test 1024 bits (unchecked) | 696 ns | ~0.68 ns per bit |
| Population count | 40 ns | 16 words × ~2.5 ns |
| Bitwise AND | 12 ns | 16 words × ~0.75 ns |
| Iterate ~100 set bits | 394 ns | ~4 ns per set bit |
| Find traversal ~100 bits | 392 ns | ~4 ns per find_next |

### Complexity Analysis

| Operation | Time Complexity | Notes |
|-----------|-----------------|-------|
| `set`, `clear`, `test`, `flip` | O(1) | Single memory access |
| `set_all`, `clear_all` | O(N/64) | One word per iteration |
| `count` | O(N/64) | One POPCNT per word |
| `find_first`, `find_last` | O(N/64) | Best case O(1), worst O(N/64) |
| `find_next` | O(N/64) | Typically O(1) for dense regions |
| Bitwise operators | O(N/64) | One operation per word |
| Iteration (full) | O(k) | k = number of set bits |

### Memory Usage

```
Memory = ceil(N / 64) × 8 bytes

Examples:
  BitSet<64>   = 8 bytes
  BitSet<100>  = 16 bytes
  BitSet<1000> = 128 bytes
  BitSet<1M>   = 128 KB
```

---

## Comparison with std::bitset

### The C++ Standard Library Option

`std::bitset<N>` has been part of C++ since C++98. It's widely available, well-tested, and familiar. However, it was designed for different use cases than HPC workloads.

### Feature Comparison

| Feature | `fat_p::BitSet` | `std::bitset` |
|---------|-----------------|---------------|
| Header-only | Yes | Yes |
| Fixed size | Yes | Yes |
| Bounds-checked access | `test()` throws | `test()` throws |
| Unchecked access | `*_unchecked()` family | `operator[]` |
| `find_first()` | Yes (CTZ) | No |
| `find_next()` | Yes (CTZ) | No |
| `find_last()` | Yes (CLZ) | No |
| Set range | `set_range()` | No |
| Clear range | `clear_range()` | No |
| Count range | `count_range()` | No |
| Sparse iteration | Yes | No |
| `is_subset_of()` | Yes | No |
| `intersects()` | Yes | No |
| `std::hash` | Yes | Yes |
| String conversion | No | `to_string()` |
| Integer conversion | No | `to_ulong()`, `to_ullong()` |
| Bit reference | No | `operator[]` returns reference |

### When to Use Each

**Use `std::bitset` when:**
- You need string/integer conversion
- You need a writable reference from `operator[]`
- You're in a codepath that must avoid any external dependencies
- Sparse iteration isn't needed

**Use `fat_p::BitSet` when:**
- You need to iterate over set bits efficiently
- You need find operations
- You need range operations
- You're in HPC/scientific computing context
- Performance of sparse access matters

### Code Comparison

```cpp
// Task: Find all set bits in a bitset

// std::bitset - O(N) always
std::bitset<10000> std_bits;
std_bits.set(100);
std_bits.set(5000);
std_bits.set(9999);

std::vector<size_t> indices;
for (size_t i = 0; i < 10000; ++i) {   // 10,000 iterations
    if (std_bits[i]) {
        indices.push_back(i);
    }
}

// fat_p::BitSet - O(k) where k = 3
fat_p::BitSet<10000> fat_bits{100, 5000, 9999};

std::vector<size_t> indices2(fat_bits.begin(), fat_bits.end());  // 3 iterations
```

---

## Migration Guide

### From std::bitset

#### Step 1: Change Include and Type

```cpp
// Before
#include <bitset>
std::bitset<1024> flags;

// After
#include "BitSet.h"
fat_p::BitSet<1024> flags;
```

#### Step 2: Replace Incompatible Operations

| `std::bitset` | `fat_p::BitSet` |
|---------------|-----------------|
| `flags.reset()` | `flags.reset()` or `flags.clear_all()` |
| `flags.reset(i)` | `flags.reset(i)` or `flags.clear(i)` |
| `flags[i] = true` | `flags.set(i)` |
| `flags[i] = false` | `flags.clear(i)` |
| `flags.to_string()` | Not available - use iteration |
| `flags.to_ulong()` | Not available - use `data()` |

#### Step 3: Leverage New Features

```cpp
// Before: O(N) scan
for (size_t i = 0; i < 1024; ++i) {
    if (flags[i]) process(i);
}

// After: O(k) iteration
for (size_t i : flags) {
    process(i);
}
```

### From bool Arrays

```cpp
// Before
bool flags[1000];
memset(flags, 0, sizeof(flags));
flags[42] = true;

// After
fat_p::BitSet<1000> flags;
flags.set(42);
```

Benefits:
- 8x memory reduction
- Hardware-accelerated count()
- Efficient sparse iteration

### From std::vector<bool>

```cpp
// Before
std::vector<bool> flags(1000, false);
flags[42] = true;
size_t count = std::count(flags.begin(), flags.end(), true);  // O(N)

// After
fat_p::BitSet<1000> flags;
flags.set(42);
size_t count = flags.count();  // O(N/64) with POPCNT
```

---

## Best Practices

### When to Use BitSet

**Good use cases:**
- Permission/capability flags
- Visited node tracking in graphs
- Active entity masks in ECS
- Feature flags
- Bloom filter backing storage
- Collision layer masks
- Sparse matrix row/column indicators

**Poor use cases:**
- Dynamic size requirements (use `boost::dynamic_bitset` or `std::vector<bool>`)
- Very small sets (< 64 bits) where a plain `uint64_t` suffices
- When you need string/integer conversion frequently

### Naming Conventions

```cpp
// Good: Describe what "set" means
fat_p::BitSet<1024> active_entities;
fat_p::BitSet<64> enabled_features;
fat_p::BitSet<256> visited_nodes;

// Avoid: Generic names
fat_p::BitSet<1024> flags;    // Flags for what?
fat_p::BitSet<64> bits;       // Bits representing what?
```

### Prefer Unchecked in Hot Paths

```cpp
// Game loop - called 60+ times per second
void update_entities(fat_p::BitSet<MAX_ENTITIES>& active) {
    for (size_t id : active) {
        // Already know id is valid because it came from the bitset
        entities[id].update();  
    }
}

// Initialization - called once
void initialize(fat_p::BitSet<MAX_ENTITIES>& active, size_t id) {
    active.set(id);  // Checked version is fine here
}
```

### Use Range Operations for Contiguous Regions

```cpp
// Slow
for (size_t i = chunk_start; i < chunk_end; ++i) {
    bits.set(i);
}

// Fast
bits.set_range(chunk_start, chunk_end);
```

### Leverage Set Operations

```cpp
// Manual intersection check - verbose
bool has_overlap = false;
for (size_t i : set_a) {
    if (set_b.test(i)) {
        has_overlap = true;
        break;
    }
}

// Built-in - clear and fast
bool has_overlap = set_a.intersects(set_b);
```

---

## Troubleshooting

### Compilation Errors

#### "static_assert failed: BitSet size must be greater than 0"

```cpp
fat_p::BitSet<0> bits;  // Error!
```

**Solution:** BitSet requires N > 0.

#### "undefined reference to `__builtin_popcountll`"

Your compiler doesn't support the builtin.

**Solution:** Enable the POPCNT instruction or use a newer compiler:
```bash
g++ -mpopcnt ...
```

### Runtime Errors

#### `std::out_of_range` from `set()`/`clear()`/`test()`

```cpp
fat_p::BitSet<64> bits;
bits.set(64);  // Throws! Valid range is [0, 63]
```

**Solution:** Ensure index < N. Use `size()` to check bounds.

#### Unexpected Results After Using `data()`

```cpp
fat_p::BitSet<100> bits;
uint64_t* raw = bits.data();
raw[1] = ~0ULL;  // Sets all 64 bits in word 1, including invalid bits 100-127!

bits.count();  // Returns wrong value!
```

**Solution:** When modifying via `data()`, mask the last word:
```cpp
// For BitSet<100>, last word must have bits 100-127 clear
raw[1] &= (1ULL << (100 - 64)) - 1;  // Mask to bits 64-99
```

### Performance Issues

#### Slow Iteration

**Symptom:** Iteration over sparse bitset is slow.

**Cause:** Using dense iteration pattern:
```cpp
for (size_t i = 0; i < bits.size(); ++i) {
    if (bits[i]) process(i);
}
```

**Solution:** Use range-based for:
```cpp
for (size_t i : bits) {
    process(i);
}
```

#### Slow Inner Loop

**Symptom:** Hot loop with bit operations is slow.

**Cause:** Using checked operations:
```cpp
for (...) {
    bits.set(i);  // Bounds check every time
}
```

**Solution:** Use unchecked operations when index validity is guaranteed:
```cpp
for (...) {
    bits.set_unchecked(i);
}
```

---

## Summary

### Key Features

| Feature | Benefit |
|---------|---------|
| Fixed-size, header-only | Zero runtime overhead, simple integration |
| Hardware intrinsics | POPCNT/CTZ/CLZ for fast operations |
| Sparse iteration | O(k) iteration over k set bits |
| Find operations | Locate set bits without full scan |
| Range operations | Efficient bulk modifications |
| Set operations | Subset/intersection tests |
| Unchecked variants | Maximum performance in hot paths |
| STL-compatible | Works with algorithms and containers |

### Performance Profile

- Single-bit operations: ~1-2 ns
- Population count: ~40 ns for 1024 bits
- Bitwise operations: ~12 ns for 1024 bits  
- Sparse iteration: ~4 ns per set bit

### Quick Start Code

```cpp
#include "BitSet.h"

int main() {
    // Create and populate
    fat_p::BitSet<1000> bits{10, 100, 500};
    bits.set_range(200, 210);
    
    // Query
    if (bits.any() && bits.count() > 5) {
        // Iterate set bits
        for (size_t idx : bits) {
            process(idx);
        }
    }
    
    // Combine
    fat_p::BitSet<1000> mask;
    mask.set_range(0, 500);
    bits &= mask;  // Keep only bits 0-499
    
    return 0;
}
```

### Related Components

- `SparseSet` — For dense iteration with O(1) add/remove and value association
- `SlotMap` — For stable handles to dynamically allocated objects
- `SmallVector` — For small, stack-allocated dynamic arrays
