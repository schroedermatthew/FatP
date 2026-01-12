---
doc_id: OV-SLOTMAP-001
doc_type: "Overview"
title: "SlotMap Overview"
fatp_components: ["SlotMap"]
topics: ["slot map", "handle stability", "generation counters"]
constraints: ["ABA safety", "dense storage"]
cxx_standard: "C++17"
last_verified: "2026-01-11"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---
# SlotMap: A Fat-P Library Showcase

## Executive Summary

SlotMap is a **generational-index container** that provides stable handles to deletable objects through a two-level indirection scheme with ABA protection. Unlike `std::vector` (pointer invalidation on insert), `std::map` (O(log n) operations), or `shared_ptr` (heap allocation per object), SlotMap achieves **O(1) insert, O(1) erase via swap-and-pop, and O(1) access** while automatically detecting stale handles through generation counters. This is the data structure game engines use for entity management—now available as a zero-dependency header.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The pattern that breeds use-after-free bugs
class EntityManager {
    std::vector<Entity> entities_;
    
public:
    Entity* create() {
        entities_.emplace_back();
        return &entities_.back();  // Danger: invalidated on next insert!
    }
    
    void destroy(size_t index) {
        entities_.erase(entities_.begin() + index);  // O(n) + invalidates indices!
    }
};

// The disaster
Entity* player = manager.create();
Entity* enemy = manager.create();  // Might invalidate player pointer!
player->update();                   // Undefined behavior
```

| Issue | HPC Impact |
|-------|------------|
| Pointer invalidation | Any vector growth invalidates all existing pointers |
| O(n) erase | Shifting elements kills performance for large collections |
| Index instability | Indices change after erase, corrupting external references |
| No stale detection | Stale pointers silently access wrong or freed memory |
| ABA problem | Reused indices make stale handles appear valid |

### The Standard's Limitation

**Why raw indices fail (ABA problem):**
```cpp
size_t player_id = manager.create();  // Index 0
manager.destroy(player_id);           // Free slot 0
size_t npc_id = manager.create();     // Reuses index 0!

manager.get(player_id);               // Returns NPC, not player!
                                      // Silent data corruption
```

**Why `shared_ptr` fails for games:**
```cpp
std::vector<std::shared_ptr<Entity>> entities_;
// Problems:
// - Heap allocation per entity (~50ns overhead)
// - Reference counting on every access
// - Poor cache locality (scattered memory)
// - 16-24 bytes per pointer vs 8 bytes for a handle
```

The standard library provides no container that combines stable handles, O(1) operations, dense storage, and stale handle detection. This is a game engine requirement the committee won't address.

---

## Architecture: Two-Level Indirection with Generation Counters

### The Mechanism

```
Handle = { index: 5, generation: 3 }
         │
         ▼
slots_[5] = { generation: 3, data_index: 2 }
         │                        │
         │ generation match? ✓    │
         ▼                        ▼
Valid access              data_[2] = actual Entity
```

**Two arrays:**
1. `slots_[]` — Sparse array mapping index → (generation, data_index)
2. `data_[]` — Dense array holding actual values (cache-friendly iteration)

**The generation counter:** When a slot is freed and reused, its generation increments. A stale handle's generation won't match, returning `nullptr` instead of wrong data.

```cpp
template<typename T>
class SlotMap {
    struct Slot {
        uint32_t generation;    // Incremented on each reuse
        uint32_t data_index;    // Points into data_ when alive
        uint32_t next_free;     // Free list linkage when dead
        bool is_alive;
    };
    
    std::vector<Slot> slots_;   // Sparse: indexed by handle.index
    std::vector<T> data_;       // Dense: actual values
    std::vector<uint32_t> erase_map_;  // data_index → slot_index
    uint32_t free_head_;        // Head of free list
};
```

### Why This Design

| Design Choice | Benefit |
|--------------|---------|
| Generation counter | Detects stale handles (ABA protection) |
| Dense data storage | Cache-friendly iteration |
| Swap-and-pop erase | O(1) removal without shifting |
| Free list | O(1) slot reuse without scanning |
| 8-byte handles | Smaller than pointers on 64-bit, smaller than `shared_ptr` |

---

## Feature Inventory

### 1. O(1) Insert with Stable Handles

```cpp
SlotMap<Entity> entities;
auto h1 = entities.insert(Entity{"Player", 100});
auto h2 = entities.insert(Entity{"Enemy", 50});

// Handles remain valid regardless of subsequent inserts
entities.insert(Entity{"NPC", 25});
Entity* player = entities.get(h1);  // Still valid!
```

**Mechanism:** Insert pops from free list (or extends slots_), constructs in dense data_, updates slot indirection.

### 2. O(1) Erase via Swap-and-Pop

```cpp
entities.erase(h1);  // O(1), no shifting

// Data array is compacted:
// Before: [Player, Enemy, NPC]
// After:  [NPC, Enemy]  (NPC swapped into Player's slot)
```

**Mechanism:** Swap removed element with last element, pop back. Update erase_map_ to maintain slot→data mapping. O(1) regardless of container size.

### 3. ABA-Protected Handle Validation

```cpp
auto handle = entities.insert(Entity{"Temp"});
entities.erase(handle);

// Slot 0 reused for new entity
auto new_handle = entities.insert(Entity{"New"});

// Stale handle returns nullptr—no silent corruption
Entity* ptr = entities.get(handle);  // nullptr (generation mismatch)
Entity* new_ptr = entities.get(new_handle);  // Valid pointer
```

**Mechanism:** Each slot has a generation counter. Erase increments generation. Access compares handle.generation against slot.generation.

### 4. Dense Iteration for Cache Efficiency

```cpp
// Iterate values only (cache-optimal, no handle overhead)
for (Entity& e : entities) {
    e.update();
}

// Iterate with handles when you need them
for (auto entry : entities.entries()) {
    save_handle(entry.handle);
    entry.value.process();
}
```

**Mechanism:** Data array is contiguous. Range-based for iterates data_ directly. No indirection during iteration.

### 5. Checked vs. Unchecked Access

| Method | Validation | Performance | Use Case |
|--------|--------|-------------|----------|
| `get(handle)` | Returns `nullptr` if invalid | ~5 ns | General code |
| `get_unchecked(handle)` | Undefined if invalid | ~2 ns | Hot paths with pre-validated handles |
| `operator[](handle)` | Throws if invalid | ~5 ns | When exceptions are appropriate |

```cpp
// Validated: check before use
if (Entity* e = entities.get(handle)) {
    e->update();
}

// Unchecked: for pre-validated handles in hot loops
for (auto h : validated_handles) {
    entities.get_unchecked(h).update();  // Skip validity check
}
```

### 6. Capacity Management

```cpp
SlotMap<Entity> entities;
entities.reserve(10000);  // Pre-allocate slots and data

size_t cap = entities.capacity();  // Current slot capacity
size_t sz = entities.size();       // Number of alive elements
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::vector | Why Not std::map | Why Not shared_ptr | Fat-P Advantage |
|----------------|---------------------|------------------|-------------------|-----------------|
| Stable handles | ✗ Pointers invalidate | ✓ Iterators stable | ✓ Pointers stable | ✓ 8-byte handles |
| O(1) insert | ✓ Amortized | ✗ O(log n) | ✓ O(1) | ✓ O(1) |
| O(1) erase | ✗ O(n) shift | ✗ O(log n) | ✓ O(1) | ✓ Swap-and-pop |
| Dense iteration | ✓ Contiguous | ✗ Node-based | ✗ Scattered | ✓ Contiguous data_ |
| ABA protection | ✗ None | ✗ None | ✗ None | ✓ Generation counters |
| Low overhead | ✓ Zero | 32+ bytes/entry | 16-24 bytes/ptr | 8 bytes/handle |

**The Sweet Spot:** SlotMap is the only option combining:
- ✓ Stable handles (don't invalidate on insert/erase)
- ✓ O(1) insert and erase
- ✓ Dense, cache-friendly iteration
- ✓ Automatic stale handle detection
- ✓ Zero external dependencies

---

## The "Forever Stuck" Reality

**Standard Library Reality:** The C++ committee will never add a SlotMap-style container because:

1. **Domain-specific:** It's optimized for game engines and ECS architectures, not general use
2. **Tradeoff choices:** Generation width, handle size, and iteration semantics require application-specific tuning
3. **Design philosophy:** The standard library favors general containers over specialized ones

Game engines have implemented this pattern for decades (Unity, Unreal, EnTT). Fat-P makes it available as a zero-dependency header without buying into a full engine or ECS framework.

---

## Performance Characteristics

### Benchmark Results (Release Build, i7-8850H @ 2.60GHz)

| Operation | Time | Mechanism |
|-----------|------|-----------|
| `insert()` | ~15 ns | Free list pop + data push_back |
| `erase()` | ~12 ns | Swap-and-pop + free list push |
| `get()` (valid) | ~5 ns | Two array lookups + generation compare |
| `get_unchecked()` | ~2 ns | Two array lookups, no validation |
| Iteration (1000 elements) | ~450 ns | Direct data_ traversal |

### Complexity Analysis

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| `insert()` | O(1) amortized | Free list or vector growth |
| `erase()` | O(1) | Swap-and-pop |
| `get()` / `is_valid()` | O(1) | Two array lookups |
| `begin()` / `end()` | O(1) | Direct data_ pointers |
| `size()` | O(1) | Cached counter |
| `clear()` | O(n) | Reset all slots |

### Where Fat-P Wins

- **Entity management:** Thousands of entities created/destroyed per frame
- **Resource pools:** GPU handles, audio sources, network connections
- **Event systems:** Listener handles that may be unsubscribed
- **Any deletable-object scenario** where `shared_ptr` overhead is unacceptable

### Where Fat-P Loses

- **Pointer stability required:** If you need actual stable pointers (not handles), use `std::list` or `std::deque`
- **Sorted access:** If you need ordered iteration, `std::map` is more appropriate
- **Minimal use cases:** For non-deletable collections, `std::vector` is more straightforward
- **Very large handles:** If you need 64-bit indices AND 64-bit generations, SlotMap's 32-bit defaults need modification

---

## Integration Points

```
SlotMap.h
    → uses
FatPTypeTraits.h   (is_slot_map<T> type trait)
    → used by
Entity-Component Systems (entity storage)
Resource managers (handle-based access)
Event systems (listener registration)
```

---

## Final Assessment

SlotMap delivers on the fat_p promise through three pillars:

### 1. Permanence
The standard library will never provide this container—it's too domain-specific. SlotMap makes a game-engine-grade data structure available as a single header, permanently solving the "validated handles to deletable objects" problem.

### 2. Specialization
Two-level indirection with generation counters is an HPC pattern optimized for entity management. Dense storage enables cache-friendly iteration; swap-and-pop enables O(1) erase. These are game engine requirements, not general-purpose design choices.

### 3. Control
Checked vs. unchecked access lets developers choose their validation/performance tradeoff. Hot paths use `get_unchecked()`; general code uses `get()`. No runtime configuration—the choice is explicit at each call site.

**Architectural Verdict:** SlotMap transforms the "validated handle to deletable object" problem from scattered `shared_ptr` usage or risky raw pointers into a **single-container solution** with O(1) operations, ABA protection, and cache-friendly iteration. It's the data structure game engines use—now available without the engine.

---

*SlotMap.h (584 lines) — Fat-P Library*
