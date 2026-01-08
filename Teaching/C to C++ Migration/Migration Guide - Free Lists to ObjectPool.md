---
doc_id: MG-OBJECTPOOL-001
doc_type: "Migration Guide"
title: "Free Lists to Type-Safe Object Pooling"
from_pattern: "Manual free lists, pre-allocated arrays, custom allocators"
to_component: "ObjectPool"
fatp_version: "1.0"
cxx_standard: "C++17"
migration_complexity: "Low-Medium"
breaking_changes: true
last_verified: "2025-01-08"
---

# Migration Guide - Free Lists to Type-Safe Object Pooling

### *From Manual Memory Management to `ObjectPool<T>`*

*FAT-P Library — January 2025*

---

## Migration Card

| Aspect | Detail |
|--------|--------|
| **C Pattern** | Free lists, pre-allocated arrays, custom allocators, lookaside caches |
| **Problems Solved** | Double-free, use-after-free, memory leaks, fragmentation, allocation latency |
| **Fat-P Component** | `ObjectPool<T, SyncPolicy>` + `PooledObject<T>` RAII wrapper |
| **Migration Complexity** | Low-Medium — mostly wrapping existing allocation sites |
| **Runtime Overhead** | Near-zero — O(1) acquire/release, zero heap allocation for pooled objects |
| **Breaking Changes** | Yes — explicit acquire/release semantics |

---

## Table of Contents

1. [The Problem with Manual Pooling](#the-problem-with-manual-pooling)
2. [Real-World Pool Disasters](#real-world-pool-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The ObjectPool Solution](#the-objectpool-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When ObjectPool Loses](#when-objectpool-loses)

---

## The Problem with Manual Pooling

Object pools are essential for performance-critical code: game engines, network servers, real-time systems. The pattern avoids heap allocation by reusing objects from a pre-allocated pool.

The C implementation:

```c
struct Object {
    struct Object* next_free;  // Intrusive free list pointer
    /* ... actual data ... */
};

struct Pool {
    struct Object* storage;    // Pre-allocated array
    struct Object* free_list;  // Head of free list
    size_t capacity;
};

Object* acquire(Pool* pool) {
    if (!pool->free_list) return NULL;
    Object* obj = pool->free_list;
    pool->free_list = obj->next_free;
    return obj;
}

void release(Pool* pool, Object* obj) {
    obj->next_free = pool->free_list;
    pool->free_list = obj;
}
```

**The problems:**

| Problem | Consequence |
|---------|-------------|
| Double-free | Corrupts free list, unpredictable crashes |
| Use-after-free | Data corruption when slot is reused |
| Wrong pool | Releasing to wrong pool corrupts memory |
| Memory leaks | Forgetting to release |
| Type confusion | Wrong type released to pool |
| No initialization | Manual constructor/destructor calls |

---

## Real-World Pool Disasters

### SQLite's Lookaside Allocator

SQLite uses a "lookaside" allocator for small, frequently-allocated objects. From [`src/malloc.c`](https://github.com/sqlite/sqlite/blob/master/src/malloc.c):

```c
/*
** The lookaside subsystem provides very fast, zero-fragmentation memory
** allocation for common small allocations.
*/
struct Lookaside {
  u32 bDisable;           /* Only operate the lookaside when zero */
  u16 sz;                 /* Size of each buffer in bytes */
  u16 bMalloced;          /* True if pStart obtained from sqlite3_malloc() */
  u32 nSlot;              /* Number of lookaside slots allocated */
  u32 anStat[3];          /* 0: hits.  1: size misses.  2: full misses */
  LookasideSlot *pInit;   /* List of buffers not previously used */
  LookasideSlot *pFree;   /* List of available buffers */
  void *pStart;           /* First byte of available memory space */
  void *pEnd;             /* First byte past end of available space */
};
```

The complexity is necessary because:
- Must track "never used" vs "previously used" slots
- Must handle size mismatches
- Must maintain statistics
- Must be thread-safe per-connection

### The Double-Free Catastrophe

From a real production bug:

```c
void process_request(Pool* pool, Request* req) {
    Object* obj = acquire(pool);
    if (!obj) return;
    
    if (process(obj, req) < 0) {
        release(pool, obj);  // Release on error
        log_error("Processing failed");
        // Fall through to release again!
    }
    
    release(pool, obj);  // Double-free!
}
```

The double-free corrupts the free list. Next `acquire()` might return the same pointer twice, leading to data races and corruption.

### The Cross-Pool Release

```c
Pool* pool_a;
Pool* pool_b;

void dangerous_code() {
    Object* obj = acquire(pool_a);
    // ... use obj ...
    release(pool_b, obj);  // WRONG POOL!
    // pool_b's free list now contains pointer from pool_a's memory
    // Next acquire from pool_b returns invalid pointer
}
```

---

## The C Patterns

### Pattern 1: Intrusive Free List

```c
typedef struct Node {
    struct Node* next;
    char data[DATA_SIZE];
} Node;

typedef struct Pool {
    Node* nodes;      // Contiguous array
    Node* free_head;  // Free list head
    size_t capacity;
} Pool;

Pool* pool_create(size_t capacity) {
    Pool* p = malloc(sizeof(Pool));
    p->nodes = malloc(capacity * sizeof(Node));
    p->capacity = capacity;
    p->free_head = NULL;
    
    // Build free list
    for (size_t i = 0; i < capacity; i++) {
        p->nodes[i].next = p->free_head;
        p->free_head = &p->nodes[i];
    }
    return p;
}

Node* pool_alloc(Pool* p) {
    if (!p->free_head) return NULL;
    Node* n = p->free_head;
    p->free_head = n->next;
    return n;
}

void pool_free(Pool* p, Node* n) {
    n->next = p->free_head;  // No validation!
    p->free_head = n;
}
```

**Problems:**
- No double-free detection
- No wrong-pool detection
- No bounds checking
- Manual capacity management

### Pattern 2: Bitset-Based Pool

```c
typedef struct Pool {
    void* storage;
    uint64_t* used_bits;  // Bitset tracking used slots
    size_t capacity;
    size_t item_size;
} Pool;

void* pool_alloc(Pool* p) {
    for (size_t i = 0; i < (p->capacity + 63) / 64; i++) {
        if (p->used_bits[i] != ~0ULL) {
            int bit = __builtin_ffsll(~p->used_bits[i]) - 1;
            size_t index = i * 64 + bit;
            if (index < p->capacity) {
                p->used_bits[i] |= (1ULL << bit);
                return (char*)p->storage + index * p->item_size;
            }
        }
    }
    return NULL;  // Pool exhausted
}

void pool_free(Pool* p, void* ptr) {
    size_t index = ((char*)ptr - (char*)p->storage) / p->item_size;
    size_t word = index / 64;
    size_t bit = index % 64;
    p->used_bits[word] &= ~(1ULL << bit);  // No double-free check!
}
```

**Problems:**
- O(n) allocation scan
- Still no double-free detection
- Pointer arithmetic is error-prone

### Pattern 3: Index-Based Pool

```c
#define INVALID_INDEX ((uint32_t)-1)

typedef struct Pool {
    void* storage;
    uint32_t* next_free;   // next_free[i] = next free index, or INVALID_INDEX
    uint32_t free_head;
    uint32_t capacity;
    size_t item_size;
} Pool;

uint32_t pool_alloc_index(Pool* p) {
    if (p->free_head == INVALID_INDEX) return INVALID_INDEX;
    uint32_t index = p->free_head;
    p->free_head = p->next_free[index];
    return index;
}

void* pool_get(Pool* p, uint32_t index) {
    return (char*)p->storage + index * p->item_size;
}

void pool_free_index(Pool* p, uint32_t index) {
    p->next_free[index] = p->free_head;
    p->free_head = index;
}
```

**Problems:**
- Separate index and pointer tracking
- Still no safety checks
- Manual type casting

### Pattern 4: C++ std::vector as Pool

```cpp
template <typename T>
class NaivePool {
    std::vector<T> storage_;
    std::vector<size_t> free_list_;
    
public:
    T* acquire() {
        if (free_list_.empty()) {
            storage_.emplace_back();  // May invalidate all pointers!
            return &storage_.back();
        }
        size_t idx = free_list_.back();
        free_list_.pop_back();
        return &storage_[idx];
    }
    
    void release(T* obj) {
        size_t idx = obj - storage_.data();  // Assumes contiguous
        free_list_.push_back(idx);  // No bounds check!
    }
};
```

**Problems:**
- `emplace_back()` invalidates all outstanding pointers
- Release doesn't check if pointer is valid
- No RAII wrapper

---

## The ObjectPool Solution

### Core Concept

`ObjectPool` provides type-safe, exception-safe pooling with RAII wrappers:

```cpp
#include "ObjectPool.h"
using namespace fat_p;

// Create pool with initial block size
ObjectPool<Connection> pool(64);

// Acquire with constructor arguments
Connection* conn = pool.acquire("localhost", 8080);

// Use the connection...
conn->send(data);

// Release back to pool (calls destructor)
pool.release(conn);

// Or use RAII wrapper
{
    auto conn = make_pooled(pool, "localhost", 8080);
    conn->send(data);
}  // Automatically released here
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **Type-safe** | Can only release correct type to pool |
| **Constructor forwarding** | `acquire(args...)` constructs in-place |
| **Destructor called** | `release()` destroys object properly |
| **RAII wrapper** | `PooledObject<T>` auto-releases |
| **[[nodiscard]]** | Can't ignore `acquire()` return |
| **Debug assertions** | Detects leaks and wrong-pool release |
| **Thread-safe option** | `ThreadSafeObjectPool<T>` |
| **try_acquire()** | Non-blocking acquire for HPC |

### API Overview

```cpp
template <typename T, typename SyncPolicy = SingleThreadedPolicy>
class ObjectPool {
public:
    // Construction
    explicit ObjectPool(size_t block_size = 64);
    
    // Non-copyable, non-movable (prevents dangling pointers)
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    
    // Core operations
    template <typename... Args>
    [[nodiscard]] T* acquire(Args&&... args);  // Always succeeds (may allocate)
    
    template <typename... Args>
    T* try_acquire(Args&&... args) noexcept(...);  // Returns nullptr if empty
    
    void release(T* obj) noexcept;  // Returns to pool, calls destructor
    
    // Query
    size_t capacity() const;
    size_t available() const;
    size_t active_count() const;  // Debug only
    Stats stats() const;
};

// RAII wrapper
template <typename T, typename SyncPolicy = SingleThreadedPolicy>
class PooledObject {
public:
    // Smart pointer semantics
    T* operator->() { return obj_; }
    T& operator*() { return *obj_; }
    T* get() { return obj_; }
    
    void reset();  // Release early
    [[nodiscard]] T* release();  // Transfer ownership
};

// Factory function
template <typename T, typename SyncPolicy, typename... Args>
[[nodiscard]] PooledObject<T, SyncPolicy>
make_pooled(ObjectPool<T, SyncPolicy>& pool, Args&&... args);
```

---

## Migration Steps

### Step 1: Identify Pool Usage

Find manual pooling patterns:

```bash
grep -rn "free_list\|freeList\|pool_alloc\|pool_free" src/
grep -rn "struct.*next.*;" src/  # Intrusive lists
grep -rn "placement new\|new.*storage" src/
```

### Step 2: Define Pooled Types

For each pooled type, ensure it's properly constructed/destructed:

```cpp
// Before: manually initialized struct
struct Connection {
    int socket;
    char buffer[1024];
    // No constructor - fields uninitialized
};

// After: proper RAII type
class Connection {
    int mSocket = -1;
    std::vector<char> mBuffer;
public:
    Connection(const std::string& host, int port);
    ~Connection();  // Closes socket
};
```

### Step 3: Replace Pool Implementation

**Before:**
```c
typedef struct ConnectionPool {
    Connection* storage;
    Connection* free_head;
    size_t capacity;
    pthread_mutex_t mutex;
} ConnectionPool;

ConnectionPool* pool_create(size_t cap);
void pool_destroy(ConnectionPool* p);
Connection* pool_acquire(ConnectionPool* p);
void pool_release(ConnectionPool* p, Connection* c);
```

**After:**
```cpp
#include "ObjectPool.h"

// Thread-safe pool
using ConnectionPool = fat_p::ThreadSafeObjectPool<Connection>;

// Or single-threaded
using ConnectionPool = fat_p::ObjectPool<Connection>;
```

### Step 4: Update Allocation Sites

**Before:**
```c
Connection* conn = pool_acquire(pool);
if (!conn) return ERROR_NO_CONNECTION;

// Initialize manually
conn->socket = create_socket();
conn->host = strdup(hostname);

// ... use connection ...

// Cleanup manually
close(conn->socket);
free(conn->host);
pool_release(pool, conn);
```

**After:**
```cpp
// Constructor handles initialization
Connection* conn = pool.acquire(hostname, port);
// ObjectPool calls constructor - all fields initialized

// ... use connection ...

// Release handles cleanup
pool.release(conn);  // Destructor called, then returned to pool
```

### Step 5: Add RAII Where Appropriate

**Before:**
```c
void handle_request(Pool* pool) {
    Connection* conn = pool_acquire(pool);
    if (!conn) return;
    
    if (process_request(conn) < 0) {
        pool_release(pool, conn);  // Error path
        return;
    }
    
    pool_release(pool, conn);  // Normal path
}
```

**After:**
```cpp
void handle_request(ConnectionPool& pool) {
    auto conn = make_pooled(pool);
    if (!conn) return;
    
    if (process_request(conn.get()) < 0) {
        return;  // conn automatically released
    }
    
    // conn automatically released
}
```

### Step 6: Add Debug Validation

ObjectPool includes debug-mode assertions:

```cpp
// In debug builds:
// - Tracks acquired count
// - Asserts on destruction if objects still acquired
// - Validates released pointers belong to pool

#ifndef NDEBUG
    pool.stats();  // Get detailed statistics
#endif
```

---

## Before/After Examples

### Example 1: Network Connection Pool

**Before (C-style):**
```c
#define POOL_SIZE 100

struct Connection {
    struct Connection* next_free;
    int socket;
    char* host;
    int port;
    bool in_use;
};

struct ConnectionPool {
    struct Connection pool[POOL_SIZE];
    struct Connection* free_list;
    pthread_mutex_t lock;
};

Connection* acquire_connection(ConnectionPool* p, const char* host, int port) {
    pthread_mutex_lock(&p->lock);
    
    Connection* c = p->free_list;
    if (!c) {
        pthread_mutex_unlock(&p->lock);
        return NULL;
    }
    
    p->free_list = c->next_free;
    c->in_use = true;
    
    pthread_mutex_unlock(&p->lock);
    
    // Manual initialization
    c->socket = socket(AF_INET, SOCK_STREAM, 0);
    c->host = strdup(host);
    c->port = port;
    // What if socket() fails? c is already removed from free list!
    
    return c;
}

void release_connection(ConnectionPool* p, Connection* c) {
    // Manual cleanup
    if (c->socket >= 0) close(c->socket);
    free(c->host);
    
    pthread_mutex_lock(&p->lock);
    c->in_use = false;
    c->next_free = p->free_list;
    p->free_list = c;  // No double-free check!
    pthread_mutex_unlock(&p->lock);
}
```

**After (ObjectPool):**
```cpp
class Connection {
    int mSocket = -1;
    std::string mHost;
    int mPort;
    
public:
    Connection(std::string host, int port) 
        : mSocket(socket(AF_INET, SOCK_STREAM, 0))
        , mHost(std::move(host))
        , mPort(port) 
    {
        if (mSocket < 0) {
            throw std::runtime_error("Failed to create socket");
        }
    }
    
    ~Connection() {
        if (mSocket >= 0) {
            close(mSocket);
        }
    }
    
    // ... methods ...
};

// Pool with thread safety
ThreadSafeObjectPool<Connection> connectionPool(100);

// Acquire - constructor called, exception-safe
Connection* conn = connectionPool.acquire("localhost", 8080);

// Or with RAII
{
    auto conn = make_pooled(connectionPool, "localhost", 8080);
    conn->send(data);
}  // Destructor called, returned to pool
```

### Example 2: Message Buffer Pool

**Before (fixed-size buffers):**
```c
#define BUFFER_SIZE 4096
#define POOL_SIZE 256

typedef struct Buffer {
    struct Buffer* next;
    char data[BUFFER_SIZE];
    size_t length;
} Buffer;

Buffer* buffer_pool[POOL_SIZE];
Buffer* free_head = NULL;
int initialized = 0;

void buffer_pool_init() {
    for (int i = 0; i < POOL_SIZE; i++) {
        buffer_pool[i] = malloc(sizeof(Buffer));
        buffer_pool[i]->next = free_head;
        free_head = buffer_pool[i];
    }
    initialized = 1;
}

Buffer* buffer_alloc() {
    if (!initialized) buffer_pool_init();  // Not thread-safe!
    if (!free_head) return NULL;
    Buffer* b = free_head;
    free_head = b->next;
    b->length = 0;
    return b;
}

void buffer_free(Buffer* b) {
    b->next = free_head;
    free_head = b;
}
```

**After (ObjectPool):**
```cpp
class Buffer {
    static constexpr size_t CAPACITY = 4096;
    std::array<char, CAPACITY> mData;
    size_t mLength = 0;
    
public:
    Buffer() = default;  // Zero-initialized by ObjectPool
    
    void clear() { mLength = 0; }
    
    void append(const char* data, size_t len) {
        size_t to_copy = std::min(len, CAPACITY - mLength);
        std::memcpy(mData.data() + mLength, data, to_copy);
        mLength += to_copy;
    }
    
    std::string_view view() const { 
        return {mData.data(), mLength}; 
    }
};

// Global pool (thread-safe)
ThreadSafeObjectPool<Buffer> bufferPool(256);

// Usage
void process_message(const char* data, size_t len) {
    auto buf = make_pooled(bufferPool);
    buf->append(data, len);
    send_to_network(buf->view());
}  // Buffer returned to pool
```

### Example 3: Game Entity Pool

**Before (index-based):**
```c
#define MAX_ENTITIES 10000

typedef struct Entity {
    float x, y, z;
    float vx, vy, vz;
    int type;
    bool active;
} Entity;

Entity entities[MAX_ENTITIES];
int free_indices[MAX_ENTITIES];
int free_count = MAX_ENTITIES;

void init_entity_pool() {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        free_indices[i] = i;
        entities[i].active = false;
    }
}

int spawn_entity(int type, float x, float y, float z) {
    if (free_count == 0) return -1;
    
    int idx = free_indices[--free_count];
    Entity* e = &entities[idx];
    e->type = type;
    e->x = x; e->y = y; e->z = z;
    e->vx = e->vy = e->vz = 0;
    e->active = true;
    return idx;
}

void despawn_entity(int idx) {
    entities[idx].active = false;
    free_indices[free_count++] = idx;  // No bounds check!
}
```

**After (ObjectPool + PooledObject):**
```cpp
struct Entity {
    glm::vec3 position;
    glm::vec3 velocity;
    EntityType type;
    
    Entity(EntityType t, glm::vec3 pos)
        : position(pos)
        , velocity(0.0f)
        , type(t) 
    {}
};

ObjectPool<Entity> entityPool(10000);

class EntityHandle {
    PooledObject<Entity> mEntity;
public:
    EntityHandle(ObjectPool<Entity>& pool, EntityType type, glm::vec3 pos)
        : mEntity(make_pooled(pool, type, pos))
    {}
    
    Entity* operator->() { return mEntity.get(); }
    bool valid() const { return mEntity.get() != nullptr; }
};

// Usage
EntityHandle enemy(entityPool, EntityType::Enemy, {10, 0, 5});
enemy->velocity = {1, 0, 0};
// Entity returned to pool when EntityHandle is destroyed
```

---

## Advanced Patterns

### Pattern: Non-Growing Pool for Real-Time

```cpp
// For hard real-time: never allocate after initialization
ObjectPool<AudioBuffer> audioPool(1024);

void process_audio() {
    // try_acquire never allocates, returns nullptr if empty
    AudioBuffer* buf = audioPool.try_acquire();
    if (!buf) {
        // Handle overflow - don't block
        drop_audio_frame();
        return;
    }
    
    process(buf);
    audioPool.release(buf);
}
```

### Pattern: Pool Statistics for Monitoring

```cpp
void monitor_pools() {
    auto stats = connectionPool.stats();
    
    metrics.gauge("pool.capacity", stats.capacity);
    metrics.gauge("pool.available", stats.available);
    metrics.gauge("pool.acquired", stats.acquired);
    metrics.gauge("pool.blocks", stats.num_blocks);
    
    if (stats.available < stats.capacity * 0.1) {
        log_warning("Connection pool nearly exhausted");
    }
}
```

### Pattern: Typed Pool Registry

```cpp
// Multiple pools for different object types
template <typename T>
ObjectPool<T>& get_pool() {
    static ObjectPool<T> pool(256);
    return pool;
}

// Usage
auto conn = make_pooled(get_pool<Connection>(), host, port);
auto buffer = make_pooled(get_pool<Buffer>());
```

### Pattern: Scoped Pool for Request Handling

```cpp
void handle_http_request(Request& req) {
    // All allocations from request-local pool
    ObjectPool<JsonNode> jsonPool(64);
    ObjectPool<StringBuffer> stringPool(16);
    
    auto root = make_pooled(jsonPool);
    auto buffer = make_pooled(stringPool);
    
    parse_json(req.body(), root.get(), jsonPool);
    serialize_response(root.get(), buffer.get());
    
    // All memory freed when function returns
}
```

---

## Verification

### Compile-Time Verification

ObjectPool provides compile-time safety:

```cpp
// Type must be destructible
static_assert(std::is_destructible_v<T>, "T must be destructible");

// [[nodiscard]] prevents ignoring acquired pointers
pool.acquire();  // Warning: ignoring return value

// Non-movable prevents dangling pointers
ObjectPool<X> pool2 = std::move(pool1);  // Compile error
```

### Runtime Verification

```cpp
TEST(ObjectPool, BasicAcquireRelease) {
    ObjectPool<int> pool(4);
    
    int* a = pool.acquire(42);
    EXPECT_NE(a, nullptr);
    EXPECT_EQ(*a, 42);
    
    pool.release(a);
    
    // Same slot reused
    int* b = pool.acquire(99);
    EXPECT_EQ(a, b);  // Same memory
    EXPECT_EQ(*b, 99);  // New value
}

TEST(ObjectPool, GrowsOnDemand) {
    ObjectPool<int> pool(2);  // Block size 2
    
    std::vector<int*> ptrs;
    for (int i = 0; i < 10; i++) {
        ptrs.push_back(pool.acquire(i));
    }
    
    EXPECT_EQ(pool.capacity(), 10);  // Grew to accommodate
    EXPECT_EQ(pool.num_blocks(), 5);  // 5 blocks of 2
    
    for (int* p : ptrs) {
        pool.release(p);
    }
}

TEST(ObjectPool, RAIIWrapper) {
    ObjectPool<std::string> pool(4);
    
    {
        auto s = make_pooled(pool, "hello");
        EXPECT_EQ(*s, "hello");
        EXPECT_EQ(pool.available(), 3);
    }  // Released here
    
    EXPECT_EQ(pool.available(), 4);
}

TEST(ObjectPool, ConstructorException) {
    struct Throws {
        Throws(bool should_throw) {
            if (should_throw) throw std::runtime_error("oops");
        }
    };
    
    ObjectPool<Throws> pool(4);
    
    auto* ok = pool.acquire(false);
    EXPECT_NE(ok, nullptr);
    
    EXPECT_THROW(pool.acquire(true), std::runtime_error);
    
    // Slot recovered after exception
    EXPECT_EQ(pool.available(), 3);
}

#ifndef NDEBUG
TEST(ObjectPool, DebugLeakDetection) {
    ObjectPool<int>* pool = new ObjectPool<int>(4);
    int* leaked = pool->acquire(42);
    
    // This should assert in debug mode
    EXPECT_DEBUG_DEATH(delete pool, "unreleased objects");
    
    pool->release(leaked);
    delete pool;  // Now OK
}
#endif
```

---

## When ObjectPool Loses

### 1. Variable-Size Allocations

ObjectPool is for fixed-size objects. For variable sizes:

```cpp
// Can't do: pool of different-sized strings
ObjectPool<std::string> pool;  // Each string has different capacity

// Use std::pmr or custom allocator instead
```

### 2. Objects with Complex Ownership

If objects hold external resources that shouldn't be recycled:

```cpp
struct Widget {
    std::unique_ptr<Texture> texture;  // Don't want to recycle this
};
// Releasing Widget doesn't release texture from GPU
```

**Mitigation:** Clear external resources in destructor or use separate pools.

### 3. Long-Lived Objects

Pools are for short-lived, frequently allocated objects:

```cpp
// Bad: objects live for hours
ObjectPool<DatabaseConnection> pool;  // Connections held open

// Good: objects live for milliseconds
ObjectPool<RequestContext> pool;  // Created per-request
```

### 4. When Standard Allocators Are Fast Enough

Modern allocators (jemalloc, tcmalloc) are very fast:

```cpp
// If you're not seeing allocation in profiles, pool may be premature optimization
auto* obj = new Object();  // Might be fast enough
delete obj;
```

**Measure first.** ObjectPool shines when:
- Allocation is in hot path
- Object sizes are uniform
- Allocation count is high (>1000/sec)
- Memory fragmentation is a concern

---

## Summary

| Aspect | C Pattern | ObjectPool |
|--------|-----------|------------|
| Type safety | None (void*) | Full |
| Constructor/destructor | Manual | Automatic |
| Double-free | Undefined behavior | Debug assertion |
| Wrong-pool release | Corruption | Debug assertion |
| Memory leaks | Silent | Debug assertion on destroy |
| Thread safety | Manual mutex | Policy-based |
| Growth | Manual | Automatic blocks |
| RAII | None | PooledObject wrapper |

**Migration ROI:**
- **Immediate:** Eliminate double-free and use-after-free bugs
- **Short-term:** Constructor/destructor called correctly
- **Long-term:** Leak detection, statistics, maintainable code

---

## References

- [SQLite Lookaside Allocator](https://github.com/sqlite/sqlite/blob/master/src/malloc.c) — Production C pooling
- Fat-P User Manual: ObjectPool — Complete API reference
- Fat-P User Manual: ConcurrencyPolicies — Thread-safety options

---

*FAT-P Library Documentation — January 2025*
