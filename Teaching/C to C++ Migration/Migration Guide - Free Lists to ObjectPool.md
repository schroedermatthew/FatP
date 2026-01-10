---
doc_id: MG-OBJECTPOOL-001
doc_type: "Migration Guide"
title: "Free Lists to Type-Safe Object Pooling"
from_pattern: "Manual free lists, pre-allocated arrays, custom allocators"
to_component: "ObjectPool"
fatp_version: "1.0"
cxx_standard: "C++17"
std_equivalent: null
std_since: null
boost_equivalent: "Boost.Pool"
migration_complexity: "Low-Medium"
breaking_changes: true
last_verified: "2025-01-09"
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

## Alternatives

- **Boost.Pool** — Mature, feature-rich, part of Boost
- **std::pmr::unsynchronized_pool_resource** (C++17) — Standard polymorphic allocator
- **std::pmr::synchronized_pool_resource** (C++17) — Thread-safe variant
- **jemalloc / tcmalloc** — General-purpose allocators with pooling behavior
- **folly::Arena** — Facebook's arena allocator
- **mimalloc** — Microsoft's high-performance allocator

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
10. [Summary](#summary)

---

## The Problem with Manual Pooling

Object pools are essential for performance-critical code: game engines, network servers, real-time systems. The pattern avoids heap allocation by reusing objects from a pre-allocated pool. When you need a connection, you grab one from the pool. When you're done, you return it. No malloc, no free, no fragmentation.

The C implementation looks simple. You maintain a free list—a linked list of available slots. Acquire pops from the head; release pushes to the head. O(1) allocation with zero heap overhead:

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

The simplicity is deceptive. This code has no safety checks. Release the same object twice, and you corrupt the free list—the next two acquires return the same pointer. Release an object to the wrong pool, and you've spliced foreign memory into your free list. Forget to release, and you've leaked a slot. Use an object after releasing it, and you'll corrupt whatever reused that slot.

| Problem | Consequence |
|---------|-------------|
| Double-free | Corrupts free list, unpredictable crashes |
| Use-after-free | Data corruption when slot is reused |
| Wrong pool | Releasing to wrong pool corrupts memory |
| Memory leaks | Forgetting to release |
| Type confusion | Wrong type released to pool |
| No initialization | Manual constructor/destructor calls |

These bugs are silent. The code compiles. It might even run correctly for months. Then one edge case triggers a double-free, and you're debugging memory corruption with no stack trace.

---

## Real-World Pool Disasters

These aren't hypothetical concerns. Pool bugs cause real crashes in production systems.

### SQLite's Lookaside Allocator

SQLite uses a "lookaside" allocator for small, frequently-allocated objects. The implementation in [`src/malloc.c`](https://github.com/sqlite/sqlite/blob/master/src/malloc.c) shows how complex production pooling becomes:

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

SQLite needs separate tracking for "never used" versus "previously used" slots, size mismatch handling, statistics for performance monitoring, and per-connection thread safety. A simple free list isn't enough. The code grows to handle every edge case, and each addition is another place for bugs.

### The Double-Free Catastrophe

A production bug that took weeks to diagnose:

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

The control flow is subtle. On error, the code releases and logs, but doesn't return. It falls through to the normal release path. The free list now contains the same slot twice. The next two acquires return the same pointer. Two threads write to the same memory. Corruption ensues.

### The Cross-Pool Release

When you have multiple pools, releasing to the wrong one is an easy mistake:

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

The release function has no way to know which pool the object came from. It just pushes to the free list. Now pool_b's free list points into pool_a's storage. The corruption spreads.

---

## The C Patterns

Before migrating, we need to recognize the patterns we're replacing. Pool implementations in C evolved in several directions, each trading off different concerns.

### Pattern 1: Intrusive Free List

The most common pattern embeds the free list pointer inside the objects themselves. When an object is free, its first bytes store a pointer to the next free object. This wastes no extra memory—the pointer overlays the data:

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

The allocation and free are O(1), just pointer manipulation. But there's no validation. Double-free corrupts the list. Wrong-pool release corrupts the list. The pointer arithmetic is error-prone, and there's no bounds checking.

### Pattern 2: Bitset-Based Pool

To detect double-frees, some implementations track which slots are in use with a bitset:

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

The bitset could detect double-frees—check if the bit is already zero before clearing. But most implementations don't bother. And allocation is now O(n) in the worst case, scanning for a free bit.

### Pattern 3: Index-Based Pool

Returning indices instead of pointers avoids some pointer arithmetic errors:

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

Now callers work with indices, converting to pointers only when needed. But the separation adds complexity—you're tracking both indices and pointers. And there are still no safety checks.

### Pattern 4: C++ std::vector as Pool

A naive C++ approach uses vector for storage and a separate vector for the free list:

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

This has a critical flaw: `emplace_back()` may reallocate the vector, invalidating every outstanding pointer. And release doesn't validate that the pointer actually came from this pool.

---

## The ObjectPool Solution

The fundamental insight behind `ObjectPool` is that pooling is a well-defined pattern with well-known failure modes. Double-free, wrong-pool release, memory leaks—we know exactly what can go wrong. A type-safe pool can prevent or detect all of them.

ObjectPool manages blocks of pre-allocated storage. When you acquire an object, it constructs it in place using your constructor arguments. When you release it, the destructor runs and the slot returns to the free list. The type system ensures you can only release objects of the correct type. Debug assertions catch double-frees and leaks.

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

The `[[nodiscard]]` attribute on `acquire()` means the compiler warns if you ignore the return value—a common source of leaks. The `PooledObject<T>` RAII wrapper eliminates manual release calls entirely: the object returns to the pool when the wrapper goes out of scope.

### Type Safety and Debug Assertions

In debug builds, ObjectPool tracks which pointers are currently acquired. Releasing a pointer that wasn't acquired from this pool triggers an assertion. Destroying the pool while objects are still outstanding triggers an assertion. These checks cost nothing in release builds but catch bugs during development.

The pool is also non-copyable and non-movable. Copying a pool would create ambiguity about which pool owns which storage. Moving would invalidate outstanding pointers. By deleting these operations, the compiler catches use-after-move and copy bugs.

### Thread-Safe Variant

For multi-threaded code, use `ThreadSafeObjectPool<T>`, which wraps all operations in appropriate synchronization. The single-threaded version has zero locking overhead.

---

## Migration Steps

Migration from manual pooling to ObjectPool is mostly mechanical: replace pool_alloc/pool_free calls with acquire/release, and add RAII wrappers where appropriate. The harder part is ensuring your types have proper constructors and destructors.

### Step 1: Identify Pool Usage

Find manual pooling patterns in your codebase. Look for free list pointers, placement new, and manual initialization:

```bash
grep -rn "free_list\|freeList\|pool_alloc\|pool_free" src/
grep -rn "struct.*next.*;" src/  # Intrusive lists
grep -rn "placement new\|new.*storage" src/
```

### Step 2: Define Pooled Types

C pools often use structs with uninitialized fields. ObjectPool calls constructors, so your types need proper initialization:

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

If construction can fail, throw an exception. ObjectPool handles constructor exceptions correctly—the slot is recovered and the exception propagates to the caller.

### Step 3: Replace Pool Implementation

The C pool struct with its mutex and free list becomes a single template instantiation:

```c
// Before
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

```cpp
// After
#include "ObjectPool.h"

// Thread-safe pool
using ConnectionPool = fat_p::ThreadSafeObjectPool<Connection>;

// Or single-threaded
using ConnectionPool = fat_p::ObjectPool<Connection>;
```

### Step 4: Update Allocation Sites

The C code manually initializes fields after acquiring and cleans them up before releasing:

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

The C++ code passes constructor arguments to acquire. The constructor initializes; the destructor cleans up:

```cpp
// Constructor handles initialization
Connection* conn = pool.acquire(hostname, port);
// ObjectPool calls constructor - all fields initialized

// ... use connection ...

// Release handles cleanup
pool.release(conn);  // Destructor called, then returned to pool
```

### Step 5: Add RAII Where Appropriate

C code has multiple release paths—error handling, early returns, normal completion. Each path must remember to release. The RAII wrapper eliminates this problem:

```c
// Before: multiple release paths
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

```cpp
// After: automatic release on all paths
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

ObjectPool's debug assertions catch bugs during development. Use them:

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

These examples show complete transformations of realistic C pooling code to ObjectPool.

### Example 1: Network Connection Pool

Network servers pool connections to avoid socket creation overhead. The C version combines a free list with manual initialization and cleanup:

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

This code has a subtle bug: if socket() fails, the connection is already removed from the free list but will never be returned. The slot leaks. And there's no double-free detection—calling release_connection twice corrupts the free list.

The ObjectPool version moves initialization into the constructor, where exceptions are handled properly:

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

If the constructor throws, the slot returns to the free list automatically. The RAII wrapper ensures the destructor runs.

### Example 2: Message Buffer Pool

Message processing often uses fixed-size buffers. The C version has a lazy initialization pattern that isn't thread-safe:

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

The check-then-initialize pattern is a race condition. Two threads calling buffer_alloc simultaneously might both call buffer_pool_init.

The ObjectPool version handles initialization safely and provides a modern API:

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

Game engines spawn and destroy entities constantly—enemies, projectiles, particles. An index-based pool avoids pointer invalidation but has no safety checks:

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

Calling despawn_entity twice with the same index corrupts the free_indices array. Calling it with an out-of-bounds index is undefined behavior.

The ObjectPool version wraps entities in handles that manage lifetime automatically:

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

The EntityHandle owns the PooledObject. When the handle is destroyed (or goes out of scope), the entity returns to the pool.

---

## Advanced Patterns

Once you've migrated basic pooling, these patterns address more specialized scenarios.

### Pattern: Non-Growing Pool for Real-Time

Hard real-time systems can't tolerate allocation latency. The `try_acquire()` method never allocates—it returns nullptr if the pool is empty. Pre-allocate enough capacity at startup, then handle exhaustion gracefully:

```cpp
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

This pattern is essential for audio processing, game engines, and embedded systems where allocation latency is unacceptable.

### Pattern: Pool Statistics for Monitoring

Production systems need visibility into pool utilization. The `stats()` method provides capacity, available count, and block information:

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

Alerting on low availability prevents pool exhaustion before it causes request failures.

### Pattern: Typed Pool Registry

When you have many pooled types, a template function provides type-safe access to per-type pools:

```cpp
template <typename T>
ObjectPool<T>& get_pool() {
    static ObjectPool<T> pool(256);
    return pool;
}

// Usage
auto conn = make_pooled(get_pool<Connection>(), host, port);
auto buffer = make_pooled(get_pool<Buffer>());
```

Each type gets its own pool, lazily initialized on first use. The static local guarantees thread-safe initialization in C++11 and later.

### Pattern: Scoped Pool for Request Handling

For request handlers that allocate many temporary objects, a request-scoped pool provides fast allocation with automatic cleanup:

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

When the pools go out of scope, all their storage is freed in one operation. No per-object deallocation, no fragmentation within the request.

---

## Verification

ObjectPool catches bugs at both compile time and runtime.

### Compile-Time Safety

The type system prevents several classes of errors. Types must be destructible—ObjectPool calls destructors on release. The `[[nodiscard]]` attribute warns if you ignore the acquire() return value, catching a common leak pattern. And the pool is non-copyable and non-movable, preventing use-after-move bugs:

```cpp
// Type must be destructible
static_assert(std::is_destructible_v<T>, "T must be destructible");

// [[nodiscard]] prevents ignoring acquired pointers
pool.acquire();  // Warning: ignoring return value

// Non-movable prevents dangling pointers
ObjectPool<X> pool2 = std::move(pool1);  // Compile error
```

### Runtime Tests

Unit tests verify the core invariants. Basic acquire/release should round-trip values correctly, and released slots should be reused:

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
```

Pools should grow automatically when exhausted, allocating new blocks:

```cpp
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
```

The RAII wrapper should release automatically when it goes out of scope:

```cpp
TEST(ObjectPool, RAIIWrapper) {
    ObjectPool<std::string> pool(4);
    
    {
        auto s = make_pooled(pool, "hello");
        EXPECT_EQ(*s, "hello");
        EXPECT_EQ(pool.available(), 3);
    }  // Released here
    
    EXPECT_EQ(pool.available(), 4);
}
```

Constructor exceptions should be handled correctly—the slot should return to the free list:

```cpp
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
```

In debug builds, destroying a pool with unreleased objects should assert:

```cpp
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

ObjectPool excels at fixed-size, short-lived, frequently allocated objects. Some scenarios call for different approaches.

### 1. Variable-Size Allocations

ObjectPool allocates fixed-size slots. If your objects have variable size—strings of different lengths, buffers of different capacities—the pool can't optimize effectively:

```cpp
// Can't do: pool of different-sized strings
ObjectPool<std::string> pool;  // Each string has different capacity

// Use std::pmr or custom allocator instead
```

The pool allocates slots big enough for the object's fixed size. The string's internal buffer is still heap-allocated. For truly variable-size allocation, use `std::pmr` pool resources or a custom arena allocator.

### 2. Objects with Complex Ownership

If objects hold external resources that shouldn't be recycled—GPU textures, file handles, database connections—you need to ensure those resources are released before the object returns to the pool:

```cpp
struct Widget {
    std::unique_ptr<Texture> texture;  // Don't want to recycle this
};
// Releasing Widget doesn't release texture from GPU
```

The destructor runs on release, so the unique_ptr would release the texture. But if you want to keep the texture alive across object reuse, pooling doesn't help. Use separate pools for the object and its resources, or manage resources independently.

### 3. Long-Lived Objects

Pools optimize allocation, not lifetime. If objects live for minutes or hours, pooling provides little benefit and fragments pool capacity:

```cpp
// Bad: objects live for hours
ObjectPool<DatabaseConnection> pool;  // Connections held open

// Good: objects live for milliseconds
ObjectPool<RequestContext> pool;  // Created per-request
```

Database connection pools are different—they're really about connection reuse, not allocation optimization. Use a different pattern for long-lived resources.

### 4. When Standard Allocators Are Fast Enough

Modern allocators like jemalloc and tcmalloc are highly optimized. If allocation isn't showing up in profiles, ObjectPool may be premature optimization:

```cpp
// If you're not seeing allocation in profiles, pool may be premature optimization
auto* obj = new Object();  // Might be fast enough
delete obj;
```

Measure first. ObjectPool provides clear benefits when allocation is in the hot path, object sizes are uniform, allocation rates exceed 1000/sec, or memory fragmentation is causing problems. If none of these apply, standard allocation may be simpler and fast enough.

---

## Summary

C-style pooling trades safety for performance. Free lists are fast, but they offer no protection against double-free, wrong-pool release, or memory leaks. Every pool bug is silent corruption that manifests far from its cause.

ObjectPool provides the same performance—O(1) acquire and release—with type safety and debug assertions. The type system ensures you can only release objects of the correct type. Debug builds catch double-frees and leaks at the point of error. The RAII wrapper eliminates manual release calls entirely.

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

The migration pays off immediately when you eliminate the class of bugs that C pools can't catch. In the short term, proper constructor/destructor calls mean resources are initialized and cleaned up correctly. In the long term, the debug assertions, statistics, and RAII wrappers make pool code maintainable rather than a source of mysterious crashes.

---

## References

- [SQLite Lookaside Allocator](https://github.com/sqlite/sqlite/blob/master/src/malloc.c) — Production C pooling
- Fat-P User Manual: ObjectPool — Complete API reference
- Fat-P User Manual: ConcurrencyPolicies — Thread-safety options

---

*FAT-P Library Documentation — January 2025*
