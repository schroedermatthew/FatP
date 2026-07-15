---
doc_id: UM-SLOTMAP-001
doc_type: "User Manual"
title: "SlotMap User Manual"
fatp_components: ["SlotMap"]
topics: ["slot map", "handle stability", "generation counters"]
constraints: ["ABA safety", "dense storage"]
cxx_standard: "C++20"
last_verified: "2026-01-11"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---
# User Manual - SlotMap

*Fat-P Library — December 2025*

---



**Scope:** Complete usage guide for `fat_p::SlotMap<T>`: generational handle-based container, insertion, removal, lookup by handle, iteration, generation counter ABA safety, and dense storage layout.

**Not covered:**
- Entity Component System (ECS) architecture patterns
- Handle-based memory management beyond SlotMap
- Concurrent slot maps

**Prerequisites:** C++20; understanding of handle-based containers (indirection through handle rather than pointer); awareness of the ABA problem in handle reuse

---

## User Manual Card

**Component:** SlotMap
**Primary use case:** Store objects with stable handles that detect use-after-free via generation counters, with dense storage for cache-friendly iteration
**Integration pattern:** Insert objects, receive a `SlotMapHandle` (the map's `Handle` alias), use the handle for O(1) lookup, handles become invalid after removal (generation mismatch detects stale use)
**Key API:** `SlotMap<T>`, `.insert()`, `.erase()`, `.get()`, `.at()`, `.contains()`, `SlotMapHandle`, `handle.generation`
**std equivalent:** None
**Common mistakes:** Using raw indices instead of handles (bypasses generation check); holding a `SlotMapHandle` to removed elements (stale handle, detected at runtime); assuming iteration order matches insertion order
**Performance notes:** Insert and remove are O(1). Lookup by handle is O(1) with one generation check. Dense storage enables cache-friendly iteration. See `components/SlotMap/results/` for current data

---
## Table of Contents

1. [The Handle Problem: Why Pointers Fail](#the-handle-problem-why-pointers-fail)
2. [How SlotMap Solves It](#how-slotmap-solves-it)
3. [Core Architecture: Four Arrays, Zero Holes](#core-architecture-four-arrays-zero-holes)
4. [Getting Started](#getting-started)
5. [Iteration: Values vs. Entries](#iteration-values-vs-entries)
6. [Generational Protection: Defeating the ABA Problem](#generational-protection-defeating-the-aba-problem)
7. [Performance Characteristics](#performance-characteristics)
8. [When to Use SlotMap (and When Not To)](#when-to-use-slotmap-and-when-not-to)
9. [Migration from Pointers and Indices](#migration-from-pointers-and-indices)
10. [Comparison with Alternatives](#comparison-with-alternatives)
11. [Best Practices](#best-practices)
12. [Troubleshooting](#troubleshooting)
13. [API Reference](#api-reference)
14. [Summary](#summary)

---

## The Handle Problem: Why Pointers Fail

Managing collections of objects with stable references is surprisingly difficult. Consider a game with entities that can be created and destroyed:

```cpp
// The naive approach — dangling pointers everywhere
std::vector<Entity> entities;

Entity* player = &entities[0];
Entity* enemy = &entities[5];

entities.erase(entities.begin() + 3);  // Reallocation!

player->update();  // Undefined behavior — pointer invalidated
enemy->update();   // Also invalid — indices shifted
```

The problem isn't limited to pointers. Using indices doesn't fully solve it either:

```cpp
// The index approach — the ABA problem
std::vector<Entity> entities;
size_t player_idx = 0;
size_t enemy_idx = 5;

entities.erase(entities.begin() + player_idx);

// Later: new entity spawns in slot 0
entities.insert(entities.begin(), new_entity);

// Bug: player_idx still points to slot 0, but it's a different entity!
process(entities[player_idx]);  // Processes wrong entity silently
```

This is the **ABA problem**: slot was A (player), became empty (B), became A again (new entity). Old references can't distinguish between the original and the replacement. The result is silent data corruption—the worst kind of bug.

### The C++ Landscape

| Approach | Pros | Cons |
|----------|------|------|
| **Raw pointers** | Direct access | Invalidated by reallocation, no lifetime tracking |
| **std::shared_ptr** | Automatic lifetime | Reference counting overhead, shared ownership semantics |
| **Indices** | Stable across reallocation | ABA problem, no stale detection |
| **std::map** | Stable iterators | O(log n) access, poor cache locality |
| **SlotMap** | O(1) access, ABA-protected, cache-friendly iteration | Fixed to array storage |

SlotMap fills a specific niche: **high-performance, ABA-protected object pools** where you control the storage and need validated handles without shared ownership semantics.

---

## How SlotMap Solves It

SlotMap uses **generational indices** to detect stale references:

```cpp
#include "SlotMap.h"

fat_p::SlotMap<Entity> entities;

auto player_handle = entities.insert(Entity{"Player", 100});
auto enemy_handle = entities.insert(Entity{"Goblin", 50});

// Access is validated
Entity* player = entities.get(player_handle);
player->health -= 10;

// Destroy the player
entities.erase(player_handle);

// New entity takes the same slot
auto new_entity = entities.insert(Entity{"Skeleton", 30});

// Old handle is INVALID — returns nullptr, never wrong data
Entity* ghost = entities.get(player_handle);  // nullptr!

// New handle works correctly
Entity* skeleton = entities.get(new_entity);  // Points to Skeleton
```

Each handle contains two parts:

```
Handle = { index: 42, generation: 3 }
           ▲              ▲
           │              └── "Which version of this slot"
           └── "Which slot in the array"
```

When a slot is reused, its generation counter increments. Old handles with outdated generations fail validation:

```cpp
// Insert: slot 0, generation 1
Handle player = { .index = 0, .generation = 1 };

// Erase: slot 0's generation becomes 2
// Insert new: slot 0, generation 2
Handle skeleton = { .index = 0, .generation = 2 };

// Validation:
is_valid(player);    // 1 != 2 → FALSE (stale handle)
is_valid(skeleton);  // 2 == 2 → TRUE (valid handle)
```

### Architectural Honesty

Before reaching for SlotMap, an astute programmer should ask: *"Why do I have references I don't own to objects that can disappear?"*

The ABA problem exists because code holds references without controlling lifetime. Consider the proper alternatives:

**If code holds a reference, it should either:**

1. **Own it** — preventing deletion while held (RAII)
2. **Observe it** — be notified of deletion (observer pattern)
3. **Not exist** — reference scoped to guaranteed-valid lifetime

Generational indices (SlotMap) represent a different philosophy: *"We accept that references leak everywhere, so let's detect staleness at runtime."* This is **capitulation to imperfect architecture**, not a solution.

**Why SlotMap exists anyway:**

Game engines and simulations often have:
- Cross-system communication with deferred processing
- Systems that cache entity references across frame boundaries
- Legacy codebases where rewriting ownership is prohibitive
- Performance requirements that outweigh architectural purity

```cpp
// Frame N: Physics system caches collision pairs
collision_pairs.push_back({entity_a, entity_b});

// Frame N+1: Gameplay deletes entity_b
entity_manager.destroy(entity_b);

// Frame N+2: Physics processes stale pair
// Without SlotMap: silent corruption or crash
// With SlotMap: get() returns nullptr, handled gracefully
```

**When to use what:**

| Situation | Recommendation |
|-----------|----------------|
| New codebase, clean architecture | Use RAII ownership (IdGuard) |
| References always short-lived | Scope references, don't store |
| Legacy code with leaked references | SlotMap is a reasonable workaround |
| Cross-system caching required | SlotMap detects staleness |
| Maximum performance, can't restructure | SlotMap adds runtime validation cheaply |

SlotMap is a **high-quality implementation of a workaround** for codebases that have lost control of reference lifetimes. If you're starting fresh, design with ownership in mind and you may not need it at all.

---

## Core Architecture: Four Arrays, Zero Holes

SlotMap maintains four internal arrays that work together to provide O(1) operations with dense iteration:

```
Handle = { index: 2, generation: 3 }
         │
         ▼
slots_[2] = { generation: 3, data_index: 1 }
         │                        │
         │ generation match? ✓    │
         ▼                        ▼
Valid access              data_[1] = actual Entity
```

| Array | Purpose | Indexing |
|-------|---------|----------|
| `data_` | Dense storage of actual values | Contiguous, no holes |
| `slots_` | Maps handle.index → {generation, data_index} | Sparse, may have free slots |
| `erase_map_` | Maps data_index → slot_index (for swap-and-pop) | Parallel to data_ |
| `free_list_` | Indices of reusable slots | Stack (LIFO reuse) |

### Why Four Arrays?

**Dense data storage** enables cache-friendly iteration. Unlike `std::map` or sparse arrays, iterating over `data_` touches contiguous memory with no holes.

**Indirection via slots** enables stable handles. When elements are erased, `data_` is compacted via swap-and-pop, but handles remain valid because they point to slots, not data indices.

**Erase map** enables O(1) erasure. When erasing, we need to update the swapped element's slot to point to its new data index. The erase map provides this reverse lookup.

**Free list** enables O(1) slot reuse. Erased slots are pushed onto the free list for LIFO reuse.

### Operation Complexity

| Operation | Time | Space | Notes |
|-----------|------|-------|-------|
| `insert()` | O(1) amortized | O(1) | May allocate if no free slots |
| `erase()` | O(1) | O(1) | Swap-and-pop, no shifting |
| `get()` | O(1) | O(1) | Two array lookups |
| `is_valid()` | O(1) | O(1) | One lookup + compare |
| Iteration | O(n) | O(1) | Dense, cache-friendly |

---

## Getting Started

### Prerequisites

- C++20 or later
- Header file: `SlotMap.h` (self-contained; standard library headers only)

### Integration

Copy the header to your project's include path. No compilation or linking required—SlotMap is header-only.

### Your First SlotMap

```cpp
#include <iostream>
#include "SlotMap.h"

struct Player
{
    std::string name;
    int health;
    
    Player(std::string n, int h) : name(std::move(n)), health(h) {}
};

int main()
{
    fat_p::SlotMap<Player> players;
    
    // Insert returns a handle
    auto alice = players.insert("Alice", 100);
    auto bob = players.insert("Bob", 80);
    
    // Access via handle
    if (Player* p = players.get(alice))
    {
        std::cout << p->name << " has " << p->health << " HP\n";
    }
    
    // Erase by handle
    players.erase(bob);
    
    // Stale handle returns nullptr
    if (players.get(bob) == nullptr)
    {
        std::cout << "Bob is gone\n";
    }
    
    // Iterate over all players
    for (const Player& p : players)
    {
        std::cout << p.name << "\n";
    }
    
    return 0;
}
```

**Output:**
```
Alice has 100 HP
Bob is gone
Alice
```

---

## Iteration: Values vs. Entries

### Value Iteration: Maximum Cache Efficiency

The default iteration traverses values only, providing cache-friendly access to the dense data array:

```cpp
SlotMap<Enemy> enemies;
// ... insert enemies ...

// Range-based for (values only)
for (Enemy& enemy : enemies)
{
    enemy.update();
}

// Const iteration
for (const Enemy& enemy : enemies)
{
    std::cout << enemy.name << "\n";
}

// Iterator-based
for (auto it = enemies.begin(); it != enemies.end(); ++it)
{
    it->render();
}
```

Value iteration is the fastest option because it directly traverses the contiguous `data_` array. The CPU prefetcher works perfectly, and each cache line may contain multiple elements.

### Entry Iteration: When You Need Handles

When you need both the handle and the value—for example, to pass handles to other systems—use `entries()`:

```cpp
SlotMap<Enemy> enemies;
// ... insert enemies ...

// Iterate over (handle, value) pairs
for (auto entry : enemies.entries())
{
    collision_system.register_entity(entry.handle);
    entry.value.update();
}

// Const entry iteration
for (auto entry : const_enemies.entries())
{
    std::cout << "Handle index: " << entry.handle.index << "\n";
}
```

**Entry structure:**

```cpp
struct Entry
{
    Handle handle;  // Valid handle for this element
    T& value;       // Reference to the element
};

struct ConstEntry
{
    Handle handle;
    const T& value;
};
```

### Iteration Performance Comparison

| Method | Overhead | Use When |
|--------|----------|----------|
| `for (auto& x : map)` | Minimal | You only need values |
| `for (auto e : map.entries())` | ~40% more | You need handles + values |

The overhead comes from reconstructing handles from the erase_map during entries iteration.

---

## Generational Protection: Defeating the ABA Problem

### The ABA Problem Visualized

```cpp
// Frame 1: Player exists at slot 0
Handle player = enemies.insert(Enemy{"Player", 100});
cached_target = player;  // AI caches this handle

// Frame 2: Player dies
enemies.erase(player);

// Frame 3: New enemy spawns, reuses slot 0
Handle goblin = enemies.insert(Enemy{"Goblin", 50});

// Frame 4: AI uses cached handle
Enemy* target = enemies.get(cached_target);
// WITHOUT generational indices: returns Goblin (WRONG!)
// WITH generational indices: returns nullptr (CORRECT!)
```

### How Generations Prevent ABA

```cpp
// Insert: generation increments from 0 to 1
Handle player = { .index = 0, .generation = 1 };
// slots_[0].generation = 1

// Erase: generation increments from 1 to 2
enemies.erase(player);
// slots_[0].generation = 2

// Insert: generation increments from 2 to 3
Handle goblin = { .index = 0, .generation = 3 };
// slots_[0].generation = 3

// Validation:
enemies.is_valid(player);  // 1 != 3 → FALSE
enemies.is_valid(goblin);  // 3 == 3 → TRUE
```

### Generation Overflow

Generations are 32-bit unsigned integers by default (`SlotMap<T, uint64_t>` selects 64-bit generations). After 4 billion reuses of the same slot, the generation wraps to 0, potentially allowing an ancient handle to validate incorrectly.

**In practice, this is not a concern:**

| Scenario | Time to Overflow |
|----------|-----------------|
| 60 FPS, 1 reuse/frame | 2.3 years continuous |
| 1000 reuses/second | 49 days continuous |
| Same slot every time | Requires deliberate attack |

For truly paranoid applications, stop reusing slots at max generation (not implemented, would require code modification).

---

## Performance Characteristics

### Benchmark Environment

| Component | Specification |
|-----------|---------------|
| Environment | Linux container (Ubuntu 24.04) |
| Compiler | GCC 13.3, `-O3` |
| CPU | Sandboxed (variable) |

### Performance Characteristics

| Operation | Mechanism |
|-----------|-----------|
| Insert | Free list pop + data `push_back` — O(1) amortized |
| Get | Two array lookups + generation validation — O(1) |
| get_unchecked | Two array lookups, no validation — O(1) |
| is_valid | One array lookup + generation compare — O(1) |
| Erase | Swap-and-pop + free list push — O(1) |
| Iteration (dense) | Contiguous `data_` traversal — cache-friendly, no gaps |
| Entry iteration | Handle reconstruction from slot indirection adds overhead vs. plain iteration |

See `components/SlotMap/results/` for current platform-specific benchmark data.

**Note:** `get()` and `get_unchecked()` show similar times in benchmarks because branch prediction is perfect when all handles are valid. The difference appears when branches mispredict or in code-size-sensitive scenarios.

### Cache Efficiency

SlotMap's dense storage provides excellent cache behavior:

```cpp
// SlotMap: Linear memory access
for (auto& enemy : enemies)  // data_[0], data_[1], data_[2]...
{
    enemy.update();  // Prefetcher happy
}

// std::map: Pointer chasing
for (auto& [k, v] : map)  // node0->next->next->...
{
    v.update();  // Cache misses
}
```

### Memory Overhead

Per element:
- `data_`: sizeof(T)
- `slots_`: 8 bytes (generation + data_index)
- `erase_map_`: 4 bytes
- Total overhead: **12 bytes per element** + freed slot tracking

Compare to:
- `std::map`: ~32-48 bytes per node (pointers + color bit)
- `std::shared_ptr`: 16+ bytes control block per object

---

## When to Use SlotMap (and When Not To)

### Use SlotMap When

- **Object pools with validated handles:** Game entities, particles, audio sources
- **Cross-system cached references:** Physics caches collision pairs across frames
- **Resource tracking with deletion:** GPU handles, network connections
- **Legacy code with leaked references:** SlotMap detects staleness cheaply

### Don't Use SlotMap When

- **New codebase, clean architecture:** Consider RAII ownership (IdGuard) instead
- **Key-value lookup by semantic key:** Use `std::unordered_map` for string→value
- **Sorted iteration required:** Use `std::map` or `FlatMap`
- **Distributed unique IDs:** Use `IdGenerator` or UUIDs
- **Full ECS architecture:** Use entt, flecs, or similar frameworks

---

## Migration from Pointers and Indices

### Migrating from Raw Pointers

```cpp
// BEFORE: Dangerous raw pointers
std::vector<Entity> entities;
Entity* player = &entities.emplace_back(...);
// ... later, entities grows, player is dangling

// AFTER: Protected handles
fat_p::SlotMap<Entity> entities;
auto player_handle = entities.insert(...);
// ... later, get() validates automatically
if (Entity* player = entities.get(player_handle)) {
    player->update();
}
```

**Key difference:** You must call `get()` each time you need access. Never cache the returned pointer across operations that might erase elements.

### Migrating from Vector + Index

```cpp
// BEFORE: ABA-vulnerable indices
std::vector<Entity> entities;
size_t player_idx = entities.size();
entities.push_back(Entity{...});
// ... player dies, index 0 reused
// entities[player_idx] silently returns wrong entity

// AFTER: Generation-protected handles
fat_p::SlotMap<Entity> entities;
auto player_handle = entities.insert(Entity{...});
// ... player dies, slot 0 reused
// get(player_handle) returns nullptr — ABA protected
```

**Key difference:** Handles encode which *version* of a slot they refer to, not just which slot.

### Migrating from std::map<uint64_t, T>

```cpp
// BEFORE: Manual ID generation + map storage
std::map<uint64_t, Entity> entities;
uint64_t next_id = 0;
uint64_t player_id = next_id++;
entities[player_id] = Entity{...};
// O(log n) insert, O(log n) lookup, pointer chasing on iteration

// AFTER: Integrated ID + storage
fat_p::SlotMap<Entity> entities;
auto player_handle = entities.insert(Entity{...});
// O(1) insert, O(1) lookup, cache-friendly iteration
```

**Key differences:**
1. SlotMap handles are opaque—you don't choose the ID
2. Iteration is cache-friendly (contiguous memory)
3. Lookup is O(1) instead of O(log n)

### Critical Semantic Differences

| Operation | std::map | SlotMap |
|-----------|----------|---------|
| Key type | User-chosen (int, string, etc.) | Opaque Handle (index + generation) |
| Lookup | By key value | By handle only |
| Invalid key behavior | Creates new entry | Returns nullptr |
| Iteration order | Sorted by key | Insertion order (approximately) |
| Pointer stability | Stable | Stable within single `get()` call |

---

## Comparison with Alternatives

### SlotMap vs IdGenerator

Both are fat_p components with complementary purposes:

| Aspect | SlotMap | IdGenerator |
|--------|---------|-------------|
| **Owns data** | Yes | No |
| **Purpose** | Store objects with validated handles | Generate unique IDs for external resources |
| **ABA protection** | Built-in (generational) | Requires `is_active()` check |
| **Validation cost** | O(1) array lookup | O(1) hash lookup (higher constant factor) |
| **Iteration** | Dense, cache-friendly | N/A (doesn't store data) |
| **Use case** | Object pools, ECS | Network handles, file IDs, database keys |

**When to use which:**

```cpp
// SlotMap: You own the objects
SlotMap<Enemy> enemies;
auto h = enemies.insert(Enemy{...});
Enemy* ptr = enemies.get(h);

// IdGenerator: You reference external resources
IdGenerator<ConnectionId> conn_ids;
auto id = conn_ids.generate();
Connection& conn = socket_map[*id];  // External storage
```

### SlotMap vs std::unordered_map

| Aspect | SlotMap | std::unordered_map |
|--------|---------|-------------------|
| Key type | Fixed (Handle) | Any hashable type |
| Iteration | Dense, predictable | Sparse, bucket-dependent |
| Memory | ~12 bytes overhead/element | ~32+ bytes overhead/element |
| Stale detection | Built-in | Manual (key presence check) |
| Access | O(1) guaranteed | O(1) average, O(n) worst |

**Use SlotMap when:**
- Keys are opaque handles (not meaningful values)
- Iteration performance matters
- Memory efficiency matters
- ABA protection is needed

**Use unordered_map when:**
- Keys have semantic meaning (e.g., names, IDs from external systems)
- You need key lookup without a handle
- Key type is not an index

### SlotMap vs ECS Libraries (entt, flecs)

| Aspect | SlotMap | ECS Entity Handles |
|--------|---------|-------------------|
| Scope | Single container | Entire registry |
| Components | Single type T | Multiple component types |
| Complexity | Focused | Full ECS framework |
| Dependency | Header-only, standalone | Full library |

**Use SlotMap for:**
- Focused object pools
- When you don't need a full ECS
- Standalone validated handle semantics

**Use ECS libraries for:**
- Full entity-component-system architecture
- Complex game engines
- When you need queries, systems, archetypes

---

## Best Practices

### Do Store Handles, Not Pointers

```cpp
// WRONG: Pointer may be invalidated
Enemy* enemy = map.get(handle);
// ... other code that might erase elements ...
enemy->update();  // Potentially dangling!

// RIGHT: Re-fetch via handle
if (Enemy* enemy = map.get(handle))
{
    enemy->update();
}
```

### Do Check Validity Before Access

```cpp
// WRONG: Assumes handle is valid
process(map.get(handle)->data);  // Crash if invalid

// RIGHT: Check first
if (auto* ptr = map.get(handle))
{
    process(ptr->data);
}
```

### Do Use entries() When You Need Handles

```cpp
// WRONG: Can't get handle from value iteration
for (auto& enemy : enemies)
{
    // How do I get the handle for this enemy?
}

// RIGHT: Use entries()
for (auto entry : enemies.entries())
{
    register_for_collision(entry.handle);
    entry.value.update();
}
```

### Do Reserve Capacity Upfront

```cpp
SlotMap<Bullet> bullets;
bullets.reserve(10000);  // Avoid reallocations in hot loop

for (int i = 0; i < 10000; ++i)
{
    bullets.insert(Bullet{...});  // No allocations
}
```

### Don't Confuse is_null() with is_valid()

```cpp
SlotMap<int>::Handle h;

h.is_null();          // TRUE (default-constructed)
map.is_valid(h);      // FALSE (not in map)

auto h2 = map.insert(42);
h2.is_null();         // FALSE (assigned)
map.is_valid(h2);     // TRUE (in map)

map.erase(h2);
h2.is_null();         // FALSE (still assigned!)
map.is_valid(h2);     // FALSE (erased)
```

### Don't Erase During Value Iteration

```cpp
// WRONG: Modifying during iteration
for (auto& enemy : enemies)
{
    if (enemy.health <= 0)
    {
        enemies.erase(???);  // Can't get handle, and would invalidate iterator
    }
}

// RIGHT: Collect handles first, erase after
std::vector<Handle> to_remove;
for (auto entry : enemies.entries())
{
    if (entry.value.health <= 0)
    {
        to_remove.push_back(entry.handle);
    }
}
for (auto h : to_remove)
{
    enemies.erase(h);
}
```

---

## Troubleshooting

### Compilation Errors

#### Missing Headers

**Symptom:**
```
fatal error: SlotMap.h: No such file or directory
```

**Solution:** Ensure `SlotMap.h` is in your include path. It is self-contained—it includes only standard library headers, so no other FatP headers are required.

#### C++ Standard Version

**Symptom:**
```
error: requires at least '-std=c++20'
```

**Solution:** Enable C++20 or later:
- GCC/Clang: `-std=c++20`
- MSVC: `/std:c++20`

### Runtime Issues

#### get() Returns nullptr Unexpectedly

**Possible causes:**
1. Handle was erased
2. Handle was from a different SlotMap
3. Handle was default-constructed
4. SlotMap was cleared or moved-from

**Debugging:**
```cpp
if (!map.is_valid(handle))
{
    if (handle.is_null())
    {
        // Handle was never assigned
    }
    else
    {
        // Handle was assigned but is now stale
    }
}
```

#### Incorrect Element Accessed

**This should never happen with SlotMap.** If `get()` returns non-null, it's guaranteed to be the correct element. If you're seeing wrong data:

1. Check for memory corruption elsewhere
2. Verify you're not holding raw pointers across modifications
3. Ensure handles aren't being incorrectly copied/assigned

---

---

## Use Case: Entity Component System (ECS)

Game entities stored in a SlotMap for stable handles and fast iteration:

```cpp
struct Entity { float x, y; int health; std::string name; };

fat_p::SlotMap<Entity> entities;

auto player = entities.insert(Entity{0, 0, 100, "Player"});
auto enemy  = entities.insert(Entity{10, 5, 50, "Goblin"});

// O(1) access by handle — even after other entities are removed
entities.at(player).health -= 10;

// Iteration over all live entities — contiguous, cache-friendly
for (auto& entity : entities)
{
    entity.x += entity.health > 0 ? 1.0f : 0.0f;
}

// Remove enemy — player handle remains valid
entities.erase(enemy);
assert(entities.contains(player));  // Still valid
```

## Use Case: Resource Manager with Safe Handles

Manage GPU textures with handles that detect use-after-free:

```cpp
fat_p::SlotMap<Texture> textures;
auto handle = textures.insert(load_texture("sprite.png"));

// Later: release the texture
textures.erase(handle);

// Even later: accidentally use the old handle
if (textures.contains(handle))     // false — generation mismatch
    draw(textures.at(handle));     // Not reached — safe
```

The generational index catches dangling handles: after erase, the slot's generation increments. The old handle's generation no longer matches, so `contains()` returns false.

## Use Case: Event Listener Registry

Register and unregister listeners with stable keys:

```cpp
fat_p::SlotMap<std::function<void(const Event&)>> listeners;

auto id = listeners.insert([](const Event& e) { handle_click(e); });
// Pass id to caller for later unregistration
// ...
listeners.erase(id);  // Clean removal, no dangling
```

## Best Practices

**Use contains() or get() for untrusted handles.** `at()` validates the generation and throws `std::out_of_range` on a stale handle. If the handle might be stale (from a different system, from serialization), check `contains()` first or use `get()` and test for nullptr.

**Iterate with range-for for bulk processing.** SlotMap stores data contiguously. Range-for iteration is cache-friendly and faster than random access by handle.

**Reserve capacity for known entity counts.** `reserve(n)` avoids reallocations. Reallocation invalidates raw pointers (but not handles).

## Expanded Troubleshooting

### Handle returns wrong entity after many insert/erase cycles

Generation overflow. After 2^32 insert/erase cycles on the same slot, the generation wraps to 0 and a stale handle may accidentally match. This is astronomically unlikely in practice.

### Iteration order changes after insert

SlotMap does not guarantee insertion order during iteration. New entities may fill previously-erased slots. If order matters, maintain a separate sorted index.

---

## API Reference

### Types

```cpp
template<typename T, typename GenerationType = uint32_t, typename Allocator = std::allocator<T>>
class SlotMap
{
public:
    using value_type = T;
    using size_type = uint32_t;
    using generation_type = GenerationType;
    using Handle = SlotMapHandleT<GenerationType>;
    
    struct Entry;        // Handle + value reference pair
    struct ConstEntry;   // Handle + const value reference pair
    
    using iterator = /* vector iterator */;
    using const_iterator = /* const vector iterator */;
    class EntryIterator;      // (Handle, value) iteration
    class ConstEntryIterator; // Const (Handle, value) iteration
};
```

### Handle

A handle is an opaque reference containing a slot index and generation counter. `SlotMapHandle` is the default 32-bit-generation alias for `SlotMapHandleT<uint32_t>`; `SlotMapHandle64` uses 64-bit generations.

```cpp
struct SlotMapHandle
{
    uint32_t index{0};
    uint32_t generation{0};
    
    constexpr bool is_null() const;           // True if default-constructed
    constexpr explicit operator bool() const; // !is_null()
    
    constexpr bool operator==(const SlotMapHandle&) const;
    constexpr bool operator!=(const SlotMapHandle&) const;
    constexpr bool operator<(const SlotMapHandle&) const;  // For std::set/map
};
```

**Important:** `is_null()` only checks if the handle was default-constructed. It does NOT check if the handle is valid in any SlotMap. Always use `map.is_valid(handle)` for that.

### Construction

| Signature | Description |
|-----------|-------------|
| `SlotMap()` | Default constructor, empty map |
| `SlotMap(const Allocator&)` | Construct with custom allocator |
| `SlotMap(const SlotMap&)` | Copy constructor |
| `SlotMap(SlotMap&&)` | Move constructor |

### Modifiers

| Method | Description |
|--------|-------------|
| `Handle insert(Args&&...)` | Insert element, returns handle |
| `Handle emplace(Args&&...)` | Alias for insert |
| `Handle insert_at(size_type hint, Args&&...)` | Insert at preferred slot index (see §[insert_at](#insert_at--hint-based-insertion)) |
| `bool erase(Handle)` | Erase element, returns success |
| `void clear()` | Remove all elements |
| `void reserve(size_type)` | Reserve capacity |
| `void shrink_to_fit()` | Reduce memory usage |

### Access

| Method | Description |
|--------|-------------|
| `T* get(Handle)` | Pointer to element, or nullptr if invalid |
| `T& get_unchecked(Handle)` | Reference without validation (UB if invalid) |
| `T& at(Handle)` | Reference, throws if invalid |
| `bool is_valid(Handle)` | Check if handle refers to valid element |
| `bool contains(Handle)` | Alias for is_valid |

### Capacity

| Method | Description |
|--------|-------------|
| `size_type size()` | Number of elements |
| `size_type capacity()` | Current capacity |
| `bool empty()` | True if empty |
| `size_type slot_count()` | Total slots allocated |
| `size_type free_slot_count()` | Reusable slots |

### Iteration

| Method | Description |
|--------|-------------|
| `begin()` / `end()` | Value iteration |
| `cbegin()` / `cend()` | Const value iteration |
| `entries()` | (Handle, value) pair iteration |

---

## insert_at — Hint-Based Insertion

`insert_at(hint_index, args...)` attempts to place the new element at a specific slot index. It is designed for scenarios where you need the returned handle to carry a particular index value — most commonly when reconstructing a prior session's handle layout from a snapshot.

```cpp
template <typename... Args>
[[nodiscard]] Handle insert_at(size_type hint_index, Args&&... args);
```

### Behaviour

| Slot state at `hint_index` | Result |
|----------------------------|--------|
| Never used or previously erased | Element placed at `hint_index`; `handle.index == hint_index` |
| Currently occupied | Falls back to normal `insert()`; `handle.index != hint_index` |
| Beyond current `slot_count()` | Slot array extended; gap indices added to the free list |

Check `handle.index == hint_index` after the call to confirm whether the hint was honoured.

### Complexity

- **O(1) amortised** when `hint_index` is within the current slot array and the slot is free.
- **O(n)** worst case when `hint_index` extends the slot array far beyond its current size (each new gap slot is pushed onto the free list).

### Use Cases

**Snapshot / save-game restore** — replay handle indices from a serialised session without a post-load fixup pass:

```cpp
// Save: record (index, value) for every live entry
std::vector<std::pair<uint32_t, MyData>> snapshot;
for (auto entry : map.entries()) {
    snapshot.push_back({entry.handle.index, entry.value});
}

// Restore: reconstruct identical indices in a fresh map
fat_p::SlotMap<MyData> restored;
for (auto& [idx, data] : snapshot) {
    auto h = restored.insert_at(idx, data);
    assert(h.index == idx);  // guaranteed when slot was free
}
```

**Deterministic network replication** — two peers create entities in the same order and end up with identical handle indices:

```cpp
// Server and client both call:
auto h = entities.insert_at(server_assigned_id, entity_data);
// Both now hold handles with matching indices — no ID-translation table needed
```

### ABA Safety

`insert_at` increments the slot's generation counter just like regular `insert()`. Stale handles from a previous occupant of that slot remain invalid after `insert_at`.

```cpp
auto old = map.insert(42);          // slot 0, gen 1
map.erase(old);                     // slot 0 freed, gen 2 internally
auto fresh = map.insert_at(0, 99); // slot 0, gen 3 (or next odd value)
assert(!map.is_valid(old));         // true — old handle still dead
assert(map.is_valid(fresh));        // true
```

### Pitfalls

- **Hint not guaranteed:** If the hinted slot is occupied, the element goes elsewhere. Always check `handle.index` if exact placement matters.
- **Gap slots cost memory:** `insert_at(1000)` on an empty map creates 1000 free slots. Use `reserve()` first if large indices are expected.
- **Not a general-purpose API:** For normal insertion patterns, prefer `insert()`. `insert_at` is specifically for snapshot-restore and replication use cases.

---

## Summary

### Key Features

- **Generational indices** prevent ABA bugs automatically
- **O(1) operations** for insert, erase, and access
- **Dense iteration** provides cache-friendly traversal
- **entries()** iteration for handle + value access
- **Header-only** with minimal dependencies
- **Zero-overhead validation** via generation comparison

### When to Use SlotMap

| Scenario | SlotMap Suitable? |
|----------|------------------|
| Object pools with validated handles | ✓ Yes |
| Game entity management | ✓ Yes |
| Resource tracking with deletion | ✓ Yes |
| Cross-system cached references | ✓ Yes (detects staleness) |
| New codebase, clean architecture | Consider RAII ownership first |
| Key-value lookup by semantic key | Use std::unordered_map |
| Distributed unique IDs | Use IdGenerator or UUIDs |
| Full ECS architecture | Use entt, flecs, etc. |

**Note:** SlotMap excels when references leak across system boundaries. If you control all reference lifetimes, RAII ownership (e.g., IdGuard) may be a cleaner solution.

### Quick Reference

```cpp
#include "SlotMap.h"

fat_p::SlotMap<MyType> map;

// Insert
auto handle = map.insert(args...);

// Insert at a preferred slot index (snapshot restore / deterministic sync)
auto h = map.insert_at(saved_index, args...);
if (h.index == saved_index) { /* hint was honoured */ }

// Access (validated)
if (MyType* ptr = map.get(handle)) { ... }

// Access (unchecked, HPC)
MyType& ref = map.get_unchecked(handle);  // UB if invalid!

// Erase
map.erase(handle);

// Check validity
if (map.is_valid(handle)) { ... }

// Value iteration
for (auto& value : map) { ... }

// Entry iteration (handle + value)
for (auto entry : map.entries())
{
    use(entry.handle, entry.value);
}
```

### Related Components

- **IdGenerator**: Unique ID generation without storage
- **StrongId**: Type-safe ID wrappers
- **Expected**: Error handling without exceptions

---

*SlotMap.h — Fat-P Library*
