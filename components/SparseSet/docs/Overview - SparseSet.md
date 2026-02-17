---
doc_id: OV-SPARSESET-001
doc_type: "Overview"
title: "SparseSet"
fatp_components: ["SparseSet", "SparseSetWithData"]
topics: ["sparse set", "dense iteration", "swap-with-back erasure", "ECS pattern", "integer set", "O(1) operations"]
constraints: ["memory proportional to max value", "unsigned integers only", "unstable iteration order on erase", "no thread safety for writes"]
cxx_standard: "C++17"
std_equivalent: null
std_since: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-02-14"
audience: ["C++ developers", "game engine developers", "ECS architects", "AI assistants"]
status: "reviewed"
---

# Overview - SparseSet

*Fat-P Library — February 2026*

---

## Executive Summary

SparseSet is an integer set with O(1) insert, erase, contains, and cache-friendly dense iteration. It stores active elements contiguously in a dense array, achieving dramatically faster operations than `std::unordered_set` by avoiding hashing, bucket management, and pointer chasing entirely. The cost is memory: a sparse array sized to the maximum possible value provides the O(1) index mapping. SparseSetWithData extends the pattern to associate arbitrary payload data with each integer key, maintaining the same O(1) guarantees.

---

## Overview Card

**Component:** SparseSet  
**Problem solved:** O(1) integer set operations with cache-friendly dense iteration  
**When to use:** Entity-component systems; integer ID tracking with frequent iteration; any set of bounded unsigned integers where iteration speed matters  
**When NOT to use:** String or float keys (need hash map); sparse key space with huge maximum (memory waste); need stable iteration order across erasures. Composite struct keys that contain an extractable integer index *are* supported via `IndexPolicy`  
**Key guarantee:** Insert, erase, contains are O(1). Iteration is contiguous-memory O(N) where N = active elements, not universe size.  
**std equivalent:** None. `std::unordered_set` is hash-based with O(1) amortized but poor cache behavior.  
**Boost equivalent:** None  
**Other alternatives:** llvm::SparseSet, entt::sparse_set (ECS), absl::flat_hash_set  
**Read next:** User Manual - SparseSet

---

## The Problem Domain

### What Goes Wrong Without It

An entity-component system tracks which entities have a given component. The entity IDs are integers from 0 to N. The naive approach:

```cpp
std::unordered_set<uint32_t> entities_with_physics;

// Add entity
entities_with_physics.insert(entity_id);

// Check membership
if (entities_with_physics.count(entity_id)) { ... }

// Iterate all entities with physics
for (uint32_t id : entities_with_physics) {
    update_physics(id);  // Cache-hostile: nodes scattered across heap
}
```

`std::unordered_set` stores each element in a heap-allocated bucket node linked by pointers. Iteration chases pointers across memory, causing cache misses on every step. At 10,000 entities iterated 60 times per second, the cache miss overhead is measurable. Insertion requires hashing and potential rehashing. Erasure leaves tombstones or triggers relinking.

### The Sparse Set Solution

Two arrays replace the hash table:

```
sparse[entity_id] → index into dense array  (universe-sized, mostly unused)
dense[index] → entity_id                     (compact, only active elements)
```

**Insert:** Append to dense, record position in sparse. O(1).

**Contains:** Check `sparse[value]` points to a valid dense position. O(1), no hashing.

**Erase:** Swap the target with the last element in dense, update sparse for the swapped element, pop back. O(1), no tombstones.

**Iterate:** Walk the dense array sequentially. Contiguous memory, prefetcher-friendly.

### The Cost

Memory is proportional to the maximum possible value, not the number of active elements. If entity IDs range from 0 to 1,000,000 but only 100 are active, the sparse array still occupies 4 MB (1M × 4 bytes). This is the fundamental tradeoff: O(1) operations require O(max_value) memory for the indirection array.

---

## Architecture

```
SparseSet<uint32_t>  (maxValue = 8, active = {2, 5, 0, 7})

sparse:  [2][ ][ 0][ ][ ][ 1][ ][ 3]   ← indexed by value
          ↑       ↑           ↑       ↑
          0       2           5       7

dense:   [ 2 ][ 5 ][ 0 ][ 7 ]           ← indexed by position
          pos0  pos1  pos2  pos3

contains(5) → sparse[5] = 1, dense[1] = 5 ✓  → true
contains(3) → sparse[3] = ?, dense[?] ≠ 3     → false

erase(5):
  1. sparse[5] = 1 (position of 5 in dense)
  2. Swap dense[1] with dense[3] (last): dense = [2, 7, 0, 5]
  3. Update sparse[7] = 1 (7's new position)
  4. Pop back: dense = [2, 7, 0]
  5. Done. O(1), no holes.
```

### SparseSetWithData

Extends SparseSet with a parallel data array:

```cpp
fat_p::SparseSetWithData<uint32_t, Transform> transforms;
transforms.insert(entity_id, Transform{position, rotation, scale});

if (auto* t = transforms.get(entity_id)) {
    t->position += velocity * dt;
}
```

The data array is kept parallel with the dense array. On erase, both the key and its associated data are swapped with the last element. Data access is O(1) via the same sparse indirection.

---

## Feature Inventory

### 1. O(1) Core Operations

Insert, erase, contains, find—all O(1) with no amortization caveat (no rehashing, no rebalancing). The only allocation is sparse array growth when a value exceeds the current capacity.

### 2. Dense Iteration

Active elements are contiguous in the dense array. Iterating 10,000 active elements out of a 1,000,000-element universe touches exactly 10,000 × sizeof(T) bytes, sequentially.

### 3. Swap-With-Back Erasure

Erase has no tombstones, no linked list surgery, no rehashing. The dense array remains compact after every erasure. The tradeoff: iteration order is unstable across erasures.

### 4. Strong Exception Safety

Insert provides the strong guarantee: if the sparse array growth throws `std::bad_alloc`, the set is unchanged. Erase is noexcept for nothrow-movable data types.

### 5. STL-Compatible Iterators

`begin()`, `end()`, `cbegin()`, `cend()` over the dense array. Range-for works directly:

```cpp
for (uint32_t id : sparse_set) {
    process(id);
}
```

---

## Performance Characteristics

Benchmarks compare `fat_p::SparseSet` against `llvm::SparseSet`, `entt`, `absl::flat_hash_set`, and `std::unordered_set` across insert, find, erase, iterate, and clear operations.

SparseSet's O(1) direct-index operations (no hashing, no bucket traversal, no node allocation) provide substantial advantages over hash-based containers for all mutation operations. Iteration performance is comparable to other dense-storage containers. The architectural advantage is that every operation resolves to array indexing rather than hash computation + bucket chain traversal + heap allocation.

See `components/SparseSet/results/` and `benchmark_results/` for current platform-specific data.

### Where SparseSet Wins

**Integer ID tracking.** Entity-component systems, slot map free lists, connection ID sets—any bounded unsigned integer set with frequent membership checks and iteration.

**High insert/erase churn.** No rehashing or rebalancing means consistent O(1) regardless of load factor.

**Iteration-heavy workloads.** Dense contiguous storage means the prefetcher works. `std::unordered_set` iteration chases heap pointers.

### Where SparseSet Loses

**Large sparse universes.** If max_value is 10^9 but active count is 100, the sparse array wastes gigabytes. Use a hash set.

**Non-integer keys (without IndexPolicy).** SparseSet requires an unsigned integer for sparse-array addressing. Plain strings and floats need hashing. However, composite struct keys that contain an extractable unsigned integer index are supported via a custom `IndexPolicy` (see the User Manual).

**Stable iteration order.** Swap-with-back erase changes iteration order. If you need insertion-order iteration, use a different container.

---

## Integration Points

```
SparseSet.h
    → uses: <vector>, <cstdint>, <algorithm>
    → used by: ECS entity tracking, component masks, ID membership queries
    → pairs with: SlotMap.h (SlotMap can use SparseSet for free-list tracking)
    → pairs with: ObjectPool.h (free-index tracking)
```

---

## Final Assessment

**Permanence.** No standard equivalent exists or is planned. `std::unordered_set` serves a different design point (arbitrary hashable keys, heap-allocated nodes). The sparse-set data structure is fundamental to ECS architectures and will remain relevant.

**Specialization.** O(1) everything via array indexing, not hashing. Dense iteration via contiguous storage. Swap-with-back erasure with no tombstones. These are properties that hash-based containers cannot provide.

**Control.** Template parameter for index type (uint8_t through uint64_t) controls the space/range tradeoff. SparseSetWithData adds associated data without changing the complexity profile. No allocator, no hash function, no bucket count to tune.

For integer set workloads with bounded key ranges and iteration requirements, SparseSet replaces hash-based containers with a simpler, faster, more cache-friendly design.

---

*SparseSet.h — Fat-P Library*
