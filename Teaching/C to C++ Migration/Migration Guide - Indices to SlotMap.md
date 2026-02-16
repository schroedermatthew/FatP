---
doc_id: MG-SLOTMAP-001
doc_type: "Migration Guide"
title: "Array Indices to Generational Handles"
from_pattern: "Raw indices, index + generation pairs, reusable ID pools"
to_component: "SlotMap"
fatp_version: "1.0"
cxx_standard: "C++20"
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2025-01-08"
fatp_components: ["SlotMap"]
topics: ["c-to-cpp", "migration", "array-indices", "handle-systems", "ABA-problem", "generational-handles"]
constraints: ["stale references", "ABA problem", "index reuse", "generation tracking"]
audience: ["C developers", "C++ developers", "AI assistants"]
status: "draft"
---

# Migration Guide - Array Indices to Generational Handles

### *From Raw Indices to ABA-Safe `SlotMap<T>` Handles*

*FAT-P Library — January 2025*

---

## Scope

This guide targets C code that uses raw integer indices into arrays (with or without manual version counters) and migrates those patterns to `SlotMap<T>` with generation-checked handles.

## Not covered

- ECS (Entity Component System) architectures
- Persistent handle systems across process boundaries (serialization)
- Concurrent slot maps (multi-threaded insert/remove)

## Prerequisites

- Familiarity with array-index-based object references
- Understanding of the ABA problem in reusable-index systems

## Migration Guide Card

**From:** Raw array indices, index + version pairs, reusable ID pools  
**To:** `SlotMap<T>` with `SlotMapHandle` for generation-checked access  
**Why migrate:** Raw indices suffer from the ABA problem — a freed index reused for a new object silently aliases the old reference  
**Compatibility strategy:** Phased — convert index-based access to handle-based access one subsystem at a time  
**Mechanical steps:**
1. Identify arrays accessed by integer index that support add/remove.
2. Replace array + index with `SlotMap<T>`.
3. Replace integer indices with `SlotMapHandle` at all call sites.
4. Replace direct indexing with `slotmap.get(handle)` (returns `Expected`).
**Behavioral equivalence:** Same logical add/remove/access operations on collections  
**Intentional differences:** Stale handles are detected at access time via generation check; no silent aliasing  
**Failure model:** Stale index → silent corruption; stale handle → `Expected` error or enforcement  
**Threading model:** SlotMap itself is not synchronized; external locking required for concurrent access  
**Lifetime model:** Handles are valid only while the SlotMap contains the referenced object at that generation  
**Alternatives:** Manual index + generation pair, `entt::registry`, custom handle system  
**Verification:** Unit tests for stale-handle detection, insert/remove cycles, generation wraparound  
**Rollback plan:** Replace `SlotMapHandle` with integer indices; replace `slotmap.get()` with direct array access

---

## Alternatives

`entt::registry` (ECS-focused, heavier), manual index + generation pair (no type safety), `boost::container::stable_vector` (pointer stability but no generation tracking).

## Mapping: From → To

| C Pattern | C++ Replacement | Notes |
|-----------|----------------|-------|
| `int index` into array | `SlotMapHandle` | Generation-checked; stale access detected |
| `array[index]` | `slotmap.get(handle)` | Returns `Expected`; stale handles produce error |
| `index + version` pair | `SlotMapHandle` (encapsulates both) | Single type; no manual version management |
| Free-index list | `SlotMap` internal free list | Automatic index recycling with generation bump |

## Compatibility and ABI boundaries

If indices cross a C API boundary, convert `SlotMapHandle` to/from a packed integer representation at the boundary. The handle's generation field prevents stale-index bugs even across the boundary.

## Lifetime and ownership model

Handles are valid only while the `SlotMap` contains the referenced object at that generation. After `remove()`, the handle's generation is stale and `get()` returns an error. The `SlotMap` owns all stored objects.

## Thread-safety and reentrancy

`SlotMap` is not internally synchronized. Concurrent `get()` is permitted if no concurrent `insert()`/`remove()`. For concurrent modification, external locking is required.

## Error and failure model

Stale handle access returns `Expected` error (not undefined behavior). Generation mismatch is detected on every access. No silent aliasing.

## Rollback plan

Replace `SlotMapHandle` with integer indices. Replace `slotmap.get(handle)` with direct array access. Restore manual free-index management. Generation-safety guarantees are lost on rollback.

## Table of Contents

1. [The Problem with Array Indices](#the-problem-with-array-indices)
2. [The ABA Problem](#the-aba-problem)
3. [The C Patterns](#the-c-patterns)
4. [The SlotMap Solution](#the-slotmap-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When SlotMap Loses](#when-slotmap-loses)

---

## The Problem with Array Indices

Systems often store objects in arrays and pass around indices as "handles":

```cpp
struct Entity {
    float x, y;
    int health;
    bool active;
};

Entity entities[MAX_ENTITIES];
int free_list[MAX_ENTITIES];
int free_count = MAX_ENTITIES;

int spawn() {
    if (free_count == 0) return -1;
    int index = free_list[--free_count];
    entities[index].active = true;
    return index;  // Return index as "handle"
}

void despawn(int index) {
    entities[index].active = false;
    free_list[free_count++] = index;  // Reuse slot
}

Entity* get(int index) {
    return &entities[index];  // No validation!
}
```

**The danger: stale indices.**

```cpp
int enemy = spawn();
// ... game logic ...
despawn(enemy);               // Enemy dies
int bullet = spawn();         // Reuses same slot!

// Later, code still holding 'enemy' index:
Entity* e = get(enemy);       // Returns bullet data!
e->health -= 10;              // Damages wrong entity!
```

This is a **use-after-free** bug, except with indices instead of pointers. The slot was reused, but the old index still points to it.

---

## The ABA Problem

The "ABA problem" is a class of bugs in concurrent and reusable-resource systems:

1. Thread A reads value "A" at location X
2. Thread B changes X to "B", then back to "A"
3. Thread A sees "A" and thinks nothing changed

With reusable indices:

```
Time 0: entities[5] = Enemy(id=100)
        enemy_handle = 5

Time 1: despawn(5)           // entities[5] freed
Time 2: spawn() -> 5         // entities[5] = Bullet(id=200)
Time 3: get(enemy_handle)    // Returns Bullet, not Enemy!
```

The index is the same (5), but the entity is different. This is the ABA problem applied to slot reuse.

---

## Real-World Handle Disasters

### Game Engine Entity Systems

Unity, Unreal, and most game engines use handle systems to solve this:

```cpp
// Simplified Unity-style entity ID
struct EntityID {
    uint32_t index;
    uint32_t version;  // Increments on reuse
};
```

Without version checking, components pointing to destroyed entities would silently reference wrong objects—causing physics glitches, rendering artifacts, or crashes.

### Database Connection Pools

```cpp
int conn_id = pool.acquire();
// ... network timeout, connection recycled by background thread ...
pool.send(conn_id, data);  // Wrong connection!
```

The connection at `conn_id` might now be connected to a different server, sending data to the wrong destination.

### File Descriptor Reuse

Unix file descriptors are small integers reused by the kernel:

```cpp
int fd = open("log.txt", O_WRONLY);
close(fd);
int new_fd = open("data.txt", O_RDONLY);  // May get same fd!

// Bug: code still has old 'fd' value
write(fd, secret_data, len);  // Writes to wrong file!
```

---

## The C Patterns

### Pattern 1: Raw Array Index

```cpp
#define MAX_OBJECTS 1000
#define INVALID_ID -1

Object objects[MAX_OBJECTS];
bool used[MAX_OBJECTS];

int allocate() {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!used[i]) {
            used[i] = true;
            return i;
        }
    }
    return INVALID_ID;
}

void deallocate(int id) {
    used[id] = false;  // Slot can be reused
}

Object* get(int id) {
    if (id < 0 || id >= MAX_OBJECTS) return NULL;
    if (!used[id]) return NULL;  // But stale ID can still match new object!
    return &objects[id];
}
```

**Problems:**
- O(n) allocation scan
- No ABA protection—reused slot matches old index
- `used` flag doesn't distinguish which instance

### Pattern 2: Index + Generation (Manual)

```cpp
struct Handle {
    uint32_t index;
    uint32_t generation;
};

struct Slot {
    uint32_t generation;
    Object object;
    bool active;
};

Slot slots[MAX_SLOTS];
Handle INVALID_HANDLE = {0, 0};

Handle allocate() {
    for (uint32_t i = 0; i < MAX_SLOTS; i++) {
        if (!slots[i].active) {
            slots[i].active = true;
            slots[i].generation++;  // Increment on reuse
            return {i, slots[i].generation};
        }
    }
    return INVALID_HANDLE;
}

void deallocate(Handle h) {
    if (h.index < MAX_SLOTS && slots[h.index].generation == h.generation) {
        slots[h.index].active = false;
    }
}

Object* get(Handle h) {
    if (h.index >= MAX_SLOTS) return NULL;
    if (slots[h.index].generation != h.generation) return NULL;  // ABA protected!
    if (!slots[h.index].active) return NULL;
    return &slots[h.index].object;
}
```

**Better:** Now ABA-safe. But still has problems:
- O(n) allocation scan
- Fixed capacity
- Sparse storage (dead objects take space)
- Manual implementation error-prone

### Pattern 3: Free List + Generation

```cpp
struct Slot {
    uint32_t generation;
    union {
        Object object;      // Active: holds object
        uint32_t next_free; // Inactive: next in free list
    };
    bool active;
};

uint32_t free_head = 0;  // Head of free list

void init() {
    for (uint32_t i = 0; i < MAX_SLOTS - 1; i++) {
        slots[i].next_free = i + 1;
        slots[i].active = false;
        slots[i].generation = 0;
    }
    slots[MAX_SLOTS-1].next_free = INVALID;
}

Handle allocate() {
    if (free_head == INVALID) return INVALID_HANDLE;
    uint32_t index = free_head;
    free_head = slots[index].next_free;
    slots[index].active = true;
    slots[index].generation++;
    return {index, slots[index].generation};
}
```

**Better:** O(1) allocation. But still:
- Fixed capacity
- Union is tricky (undefined behavior if misused)
- Sparse storage

### Pattern 4: Dense + Sparse (Optimal)

```cpp
// Dense array: actual objects, tightly packed
Object dense[MAX_OBJECTS];
uint32_t dense_to_sparse[MAX_OBJECTS];  // dense index -> slot index
uint32_t dense_count = 0;

// Sparse array: slots with generation
struct Slot {
    uint32_t generation;
    uint32_t dense_index;  // Index into dense array
};
Slot sparse[MAX_SLOTS];
uint32_t free_list[MAX_SLOTS];
uint32_t free_count = 0;
```

This is the optimal structure—**exactly what SlotMap implements**.

---

## The SlotMap Solution

### Core Concept

`SlotMap` provides:
- **Stable handles** that survive insertions and deletions
- **ABA safety** via generation counters
- **Dense storage** for cache-efficient iteration
- **O(1) operations** for insert, remove, access

```cpp
#include "SlotMap.h"
using namespace fat_p;

SlotMap<Enemy> enemies;

// Insert returns a handle (not an index)
SlotMapHandle h1 = enemies.insert(Enemy{100, 50});
SlotMapHandle h2 = enemies.insert(Enemy{80, 30});

// Access via handle - returns nullptr if invalid
Enemy* e = enemies.get(h1);
if (e) {
    e->health -= 10;
}

// Remove by handle
enemies.erase(h1);

// Old handle now returns nullptr
assert(enemies.get(h1) == nullptr);  // Safe!

// New insert may reuse slot, but handle is different
SlotMapHandle h3 = enemies.insert(Enemy{120, 60});
// h3.index might equal h1.index, but h3.generation differs
assert(enemies.get(h1) == nullptr);  // Still nullptr!
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **Generational handles** | Detect stale references |
| **Dense storage** | Cache-efficient iteration |
| **O(1) operations** | Fast insert, remove, access |
| **Swap-and-pop removal** | No holes in dense array |
| **Entry iteration** | Get (handle, value) pairs |
| **Hashable handles** | Use in std::unordered_map |

### Handle Structure

```cpp
struct SlotMapHandle {
    uint32_t index;      // Slot index
    uint32_t generation; // Version counter
    
    bool operator==(const SlotMapHandle&) const noexcept;
    bool operator<(const SlotMapHandle&) const noexcept;  // For std::set
    bool is_null() const noexcept;
    explicit operator bool() const noexcept;
};

// Hash specialization for std::unordered_set/map
template<>
struct std::hash<SlotMapHandle> { ... };
```

### API Overview

```cpp
template<typename T, typename Allocator = std::allocator<T>>
class SlotMap {
public:
    using Handle = SlotMapHandle;
    
    // Insertion
    Handle insert(const T& value);
    Handle insert(T&& value);
    template<typename... Args>
    Handle emplace(Args&&... args);
    
    // Removal
    bool erase(Handle handle);
    void clear();
    
    // Access (safe)
    T* get(Handle handle) noexcept;            // nullptr if invalid
    const T* get(Handle handle) const noexcept;
    T& at(Handle handle);                       // throws if invalid
    
    // Access (unchecked - for HPC)
    T& get_unchecked(Handle handle) noexcept;  // UB if invalid
    
    // Validity
    bool is_valid(Handle handle) const noexcept;
    bool contains(Handle handle) const noexcept;
    
    // Capacity
    size_type size() const noexcept;
    bool empty() const noexcept;
    
    // Iteration (dense - cache efficient)
    iterator begin() noexcept;
    iterator end() noexcept;
    
    // Entry iteration (handle + value)
    EntryRange entries() noexcept;
};
```

---

## Migration Steps

### Step 1: Identify Index-Based Handles

Find code using indices as handles:

```bash
grep -rn "int.*_id\|int.*_index\|int.*_handle" src/
grep -rn "entities\[.*\]\|objects\[.*\]" src/
grep -rn "uint32_t.*index\|size_t.*index" src/
```

### Step 2: Define Handle Types

Replace raw indices with SlotMap handles:

**Before:**
```cpp
int entity_id;
int player_handle;
size_t connection_index;
```

**After:**
```cpp
SlotMapHandle entityHandle;
SlotMapHandle playerHandle;
SlotMapHandle connectionHandle;
```

Or use type aliases for clarity:

```cpp
using EntityHandle = SlotMapHandle;
using ConnectionHandle = SlotMapHandle;
```

### Step 3: Replace Container

**Before:**
```cpp
std::vector<Entity> entities;
std::vector<size_t> free_list;
std::vector<bool> active;
```

**After:**
```cpp
SlotMap<Entity> entities;
// No free list needed - SlotMap manages internally
// No active flag needed - validity checked via generation
```

### Step 4: Update Allocation

**Before:**
```cpp
size_t spawn_entity(int type, float x, float y) {
    size_t index;
    if (!free_list.empty()) {
        index = free_list.back();
        free_list.pop_back();
    } else {
        index = entities.size();
        entities.emplace_back();
    }
    entities[index] = Entity{type, x, y};
    active[index] = true;
    return index;
}
```

**After:**
```cpp
SlotMapHandle spawn_entity(int type, float x, float y) {
    return entities.emplace(type, x, y);
}
```

### Step 5: Update Deallocation

**Before:**
```cpp
void despawn_entity(size_t index) {
    active[index] = false;
    free_list.push_back(index);
}
```

**After:**
```cpp
void despawn_entity(SlotMapHandle handle) {
    entities.erase(handle);
}
```

### Step 6: Update Access

**Before:**
```cpp
Entity* get_entity(size_t index) {
    if (index >= entities.size()) return nullptr;
    if (!active[index]) return nullptr;
    return &entities[index];
}
```

**After:**
```cpp
Entity* get_entity(SlotMapHandle handle) {
    return entities.get(handle);
}
```

### Step 7: Update Iteration

**Before (sparse, may have holes):**
```cpp
for (size_t i = 0; i < entities.size(); i++) {
    if (active[i]) {
        update(entities[i]);
    }
}
```

**After (dense, no holes):**
```cpp
// Simple iteration (values only)
for (Entity& e : entities) {
    update(e);
}

// Entry iteration (handle + value)
for (auto entry : entities.entries()) {
    physics.update(entry.handle, entry.value);
}
```

---

## Before/After Examples

### Example 1: Game Entity System

**Before (manual generation tracking):**
```cpp
struct Entity {
    float x, y;
    int health;
};

struct EntitySlot {
    Entity entity;
    uint32_t generation;
    bool active;
};

struct EntityHandle {
    uint32_t index;
    uint32_t generation;
};

class EntityManager {
    std::vector<EntitySlot> slots_;
    std::vector<uint32_t> free_list_;
    
public:
    EntityHandle spawn(float x, float y, int health) {
        uint32_t index;
        if (!free_list_.empty()) {
            index = free_list_.back();
            free_list_.pop_back();
            slots_[index].generation++;
        } else {
            index = slots_.size();
            slots_.push_back({});
            slots_[index].generation = 1;
        }
        
        slots_[index].entity = {x, y, health};
        slots_[index].active = true;
        return {index, slots_[index].generation};
    }
    
    void despawn(EntityHandle h) {
        if (h.index >= slots_.size()) return;
        if (slots_[h.index].generation != h.generation) return;
        slots_[h.index].active = false;
        free_list_.push_back(h.index);
    }
    
    Entity* get(EntityHandle h) {
        if (h.index >= slots_.size()) return nullptr;
        if (slots_[h.index].generation != h.generation) return nullptr;
        if (!slots_[h.index].active) return nullptr;
        return &slots_[h.index].entity;
    }
    
    void update_all() {
        // Sparse iteration - checks active flag
        for (auto& slot : slots_) {
            if (slot.active) {
                update(slot.entity);
            }
        }
    }
};
```

**After (SlotMap):**
```cpp
struct Entity {
    float x, y;
    int health;
};

class EntityManager {
    SlotMap<Entity> entities_;
    
public:
    SlotMapHandle spawn(float x, float y, int health) {
        return entities_.emplace(x, y, health);
    }
    
    void despawn(SlotMapHandle h) {
        entities_.erase(h);
    }
    
    Entity* get(SlotMapHandle h) {
        return entities_.get(h);
    }
    
    void update_all() {
        // Dense iteration - no holes, cache efficient
        for (Entity& e : entities_) {
            update(e);
        }
    }
};
```

**Lines of code:** 60+ → 20

### Example 2: Connection Manager

**Before:**
```cpp
struct Connection {
    int socket;
    std::string host;
    bool authenticated;
};

#define MAX_CONNECTIONS 1000
Connection connections[MAX_CONNECTIONS];
uint16_t generations[MAX_CONNECTIONS];
bool in_use[MAX_CONNECTIONS];
int free_stack[MAX_CONNECTIONS];
int free_top = MAX_CONNECTIONS;

struct ConnHandle {
    uint16_t index;
    uint16_t generation;
};

ConnHandle accept_connection(int socket, const std::string& host) {
    if (free_top == 0) return {0, 0};  // Invalid
    uint16_t idx = free_stack[--free_top];
    connections[idx] = {socket, host, false};
    generations[idx]++;
    in_use[idx] = true;
    return {idx, generations[idx]};
}

bool send_data(ConnHandle h, const void* data, size_t len) {
    if (h.index >= MAX_CONNECTIONS) return false;
    if (generations[h.index] != h.generation) return false;
    if (!in_use[h.index]) return false;
    
    return ::send(connections[h.index].socket, data, len, 0) > 0;
}
```

**After:**
```cpp
struct Connection {
    int socket;
    std::string host;
    bool authenticated;
    
    Connection(int s, std::string h) : socket(s), host(std::move(h)) {}
};

class ConnectionManager {
    SlotMap<Connection> connections_;
    
public:
    SlotMapHandle accept(int socket, const std::string& host) {
        return connections_.emplace(socket, host);
    }
    
    void close(SlotMapHandle h) {
        if (auto* c = connections_.get(h)) {
            ::close(c->socket);
            connections_.erase(h);
        }
    }
    
    bool send(SlotMapHandle h, const void* data, size_t len) {
        auto* c = connections_.get(h);
        if (!c) return false;
        return ::send(c->socket, data, len, 0) > 0;
    }
    
    // Broadcast to all connections
    void broadcast(const void* data, size_t len) {
        for (auto& conn : connections_) {
            ::send(conn.socket, data, len, 0);
        }
    }
};
```

### Example 3: Resource Cache with References

**Before (raw indices):**
```cpp
std::vector<Texture> textures;
std::unordered_map<std::string, size_t> name_to_index;

size_t load_texture(const std::string& name) {
    auto it = name_to_index.find(name);
    if (it != name_to_index.end()) {
        return it->second;  // Cached
    }
    size_t index = textures.size();
    textures.push_back(load_from_disk(name));
    name_to_index[name] = index;
    return index;
}

void unload_texture(size_t index) {
    // Can't remove from vector - would invalidate other indices!
    // Must leave hole or don't support unloading
}
```

**After (SlotMap):**
```cpp
SlotMap<Texture> textures;
std::unordered_map<std::string, SlotMapHandle> name_to_handle;

SlotMapHandle load_texture(const std::string& name) {
    auto it = name_to_handle.find(name);
    if (it != name_to_handle.end() && textures.is_valid(it->second)) {
        return it->second;
    }
    auto handle = textures.insert(load_from_disk(name));
    name_to_handle[name] = handle;
    return handle;
}

void unload_texture(SlotMapHandle handle) {
    textures.erase(handle);  // Safe - handles are stable
    // Other handles remain valid!
}
```

---

## Advanced Patterns

### Pattern: Handle-Based Component System

```cpp
// Components reference entities by handle
struct TransformComponent {
    SlotMapHandle entity;  // Owning entity
    glm::vec3 position;
    glm::quat rotation;
};

struct RenderComponent {
    SlotMapHandle entity;
    SlotMapHandle mesh;      // Reference to mesh
    SlotMapHandle material;  // Reference to material
};

class World {
    SlotMap<Entity> entities_;
    SlotMap<TransformComponent> transforms_;
    SlotMap<RenderComponent> renderables_;
    SlotMap<Mesh> meshes_;
    SlotMap<Material> materials_;
    
    // Component lookup
    std::unordered_map<SlotMapHandle, SlotMapHandle> entity_to_transform_;
};
```

### Pattern: Cross-References Between SlotMaps

```cpp
SlotMap<Parent> parents;
SlotMap<Child> children;

struct Parent {
    std::vector<SlotMapHandle> child_handles;
};

struct Child {
    SlotMapHandle parent_handle;
};

// Safe cleanup
void remove_parent(SlotMapHandle h) {
    Parent* p = parents.get(h);
    if (!p) return;
    
    // Remove all children
    for (auto child_h : p->child_handles) {
        children.erase(child_h);
    }
    parents.erase(h);
}
```

### Pattern: Handle Serialization

```cpp
// Handles are just two uint32_t - easy to serialize
void serialize(std::ostream& out, SlotMapHandle h) {
    out.write(reinterpret_cast<char*>(&h.index), sizeof(h.index));
    out.write(reinterpret_cast<char*>(&h.generation), sizeof(h.generation));
}

SlotMapHandle deserialize(std::istream& in) {
    SlotMapHandle h;
    in.read(reinterpret_cast<char*>(&h.index), sizeof(h.index));
    in.read(reinterpret_cast<char*>(&h.generation), sizeof(h.generation));
    return h;
}
```

### Pattern: Deferred Deletion

```cpp
class SafeEntityManager {
    SlotMap<Entity> entities_;
    std::vector<SlotMapHandle> pending_deletions_;
    
public:
    void mark_for_deletion(SlotMapHandle h) {
        pending_deletions_.push_back(h);
    }
    
    void flush_deletions() {
        for (auto h : pending_deletions_) {
            entities_.erase(h);
        }
        pending_deletions_.clear();
    }
    
    // Safe iteration - deletions deferred
    void update_all() {
        for (auto entry : entities_.entries()) {
            entry.value.update();
            if (entry.value.should_die()) {
                mark_for_deletion(entry.handle);
            }
        }
        flush_deletions();
    }
};
```

---

## Verification

### Compile-Time Verification

```cpp
// Handle is hashable
static_assert(std::is_default_constructible_v<std::hash<SlotMapHandle>>);

// SlotMap is movable
static_assert(std::is_move_constructible_v<SlotMap<int>>);
```

### Runtime Verification

```cpp
TEST(SlotMap, BasicInsertAndGet) {
    SlotMap<int> map;
    
    auto h = map.insert(42);
    EXPECT_TRUE(map.is_valid(h));
    
    int* p = map.get(h);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(SlotMap, EraseInvalidatesHandle) {
    SlotMap<int> map;
    
    auto h = map.insert(42);
    EXPECT_TRUE(map.is_valid(h));
    
    map.erase(h);
    EXPECT_FALSE(map.is_valid(h));
    EXPECT_EQ(map.get(h), nullptr);
}

TEST(SlotMap, GenerationPreventsABA) {
    SlotMap<int> map;
    
    auto h1 = map.insert(1);
    uint32_t original_index = h1.index;
    
    map.erase(h1);
    
    auto h2 = map.insert(2);
    
    // Same slot reused
    EXPECT_EQ(h2.index, original_index);
    // But different generation
    EXPECT_NE(h2.generation, h1.generation);
    
    // Old handle still invalid
    EXPECT_FALSE(map.is_valid(h1));
    EXPECT_EQ(map.get(h1), nullptr);
    
    // New handle valid
    EXPECT_TRUE(map.is_valid(h2));
    EXPECT_EQ(*map.get(h2), 2);
}

TEST(SlotMap, DenseIteration) {
    SlotMap<int> map;
    
    auto h1 = map.insert(1);
    auto h2 = map.insert(2);
    auto h3 = map.insert(3);
    
    map.erase(h2);  // Remove middle
    
    // Iteration is dense - no holes
    std::vector<int> values;
    for (int& v : map) {
        values.push_back(v);
    }
    
    EXPECT_EQ(values.size(), 2);
    // Order may vary due to swap-and-pop
}

TEST(SlotMap, EntryIteration) {
    SlotMap<std::string> map;
    
    auto h1 = map.insert("hello");
    auto h2 = map.insert("world");
    
    std::unordered_set<SlotMapHandle> seen;
    for (auto entry : map.entries()) {
        seen.insert(entry.handle);
        EXPECT_TRUE(map.is_valid(entry.handle));
    }
    
    EXPECT_TRUE(seen.count(h1));
    EXPECT_TRUE(seen.count(h2));
}

TEST(SlotMap, HandleInUnorderedMap) {
    SlotMap<Entity> entities;
    std::unordered_map<SlotMapHandle, std::string> names;
    
    auto h = entities.insert(Entity{});
    names[h] = "Player";
    
    EXPECT_EQ(names.at(h), "Player");
}
```

---

## When SlotMap Loses

### 1. No Deletion Needed

If objects are never removed:

```cpp
// Simple vector is fine
std::vector<Particle> particles;
size_t id = particles.size();
particles.push_back(p);
// id is stable forever
```

### 2. Pointer Stability Required

If you need pointers that never invalidate:

```cpp
// SlotMap doesn't guarantee pointer stability across insert/erase
auto h = map.insert(x);
int* p = map.get(h);
map.insert(y);  // May invalidate p!

// Use std::list or deque if pointers must stay valid
```

### 3. Very Small Objects

For objects smaller than 8 bytes, the slot overhead (8+ bytes) may be significant:

```cpp
// Overhead per slot: generation (4) + dense_index (4) = 8 bytes
SlotMap<char> chars;  // 8 bytes overhead for 1 byte data
```

### 4. Need Ordered Iteration

SlotMap iteration order is undefined (swap-and-pop changes order):

```cpp
// If you need ordered iteration
std::map<Key, Value> ordered;  // Use this instead
```

### 5. Extremely Tight Memory

SlotMap uses extra memory for indirection:
- Slot array: 8 bytes per max-ever-allocated
- Erase map: 4 bytes per element
- Free list: 4 bytes per free slot

For millions of objects, this overhead matters.

---

## Summary

| Aspect | Raw Indices | SlotMap |
|--------|-------------|---------|
| Stale reference | Returns wrong object | Returns nullptr |
| ABA problem | Vulnerable | Protected by generation |
| Slot reuse | Dangerous | Safe |
| Iteration | Sparse (holes) | Dense (cache efficient) |
| Insert | O(1) or O(n) | O(1) amortized |
| Remove | O(1) | O(1) swap-and-pop |
| Access | O(1) | O(1) (two lookups) |
| Memory | Minimal | 12+ bytes overhead per slot |
| Hashable handles | Manual | Built-in |

**Migration ROI:**
- **Immediate:** Eliminate use-after-free bugs with stale indices
- **Short-term:** Dense iteration improves cache performance
- **Long-term:** Safe handle passing between systems

---

## References

- [Unity Entity Component System](https://docs.unity3d.com/Packages/com.unity.entities) — Handle-based entity references
- [Bevy ECS](https://bevyengine.org/) — Rust ECS with similar generational indices
- [Slot Map Explained](https://gamedev.stackexchange.com/questions/33888) — Original concept discussion
- Fat-P User Manual: SlotMap — Complete API reference

---

*FAT-P Library Documentation — January 2025*
