# IdGenerator: A Fat-P Library Showcase

## Executive Summary

IdGenerator is a **compile-time policy-composed** unique identifier system that resolves allocation, recycling, error handling, and concurrency strategies at build time with zero virtual dispatch. Unlike hand-rolled ID counters—which inevitably miss overflow handling, recycling, or type safety—IdGenerator composes five orthogonal policies into exactly the behavior you need. The resulting code compiles down to optimal machine instructions: a simple counter for single-threaded sequential use, or a mutex-protected hash set for concurrent recycling workloads.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The pattern that breeds bugs in every codebase
class EntityManager {
    uint64_t next_id_ = 0;
    std::unordered_set<uint64_t> active_ids_;
    
public:
    uint64_t create() {
        uint64_t id = next_id_++;  // What about overflow?
        active_ids_.insert(id);    // What if insert fails?
        return id;
    }
    
    void destroy(uint64_t id) {
        active_ids_.erase(id);     // ID wasted forever
    }
};
```

| Issue | HPC Impact |
|-------|------------|
| Silent overflow | `next_id_++` wraps to 0 after 2^64 operations, creating duplicate IDs |
| No recycling | Long-running simulations exhaust ID space permanently |
| No thread safety | Data races corrupt the generator in parallel code |
| No type safety | `UserId` and `OrderId` are both `uint64_t`—mixups compile silently |
| No error reporting | Caller cannot detect exhaustion; corruption propagates silently |
| No RAII | Leaked IDs if exceptions thrown between create and destroy |

### The Standard's Limitation

The C++ standard library provides no ID generation facility. Every project reinvents this wheel, typically getting at least one thing wrong. Even when projects use UUIDs (128-bit), they sacrifice:

- **Sequential ordering** for debugging and cache locality
- **Recycling** for bounded memory usage
- **Type safety** for domain separation
- **Deterministic behavior** for reproducible testing

IdGenerator isn't waiting for a standard—there is no standard coming. This is a domain-specific concern the committee will never address.

---

## Architecture: Compile-Time Policy Composition

### Template Signature

```cpp
template <
    typename IdType_,           // uint64_t, StrongId<uint64_t, Tag>, etc.
    typename AllocationPolicy,  // Sequential, Bounded, Random
    typename RecyclingPolicy,   // FIFO, MinFirst, None
    typename ErrorPolicy,       // Throw, Expected<T, IdError>
    typename ConcurrencyPolicy  // SingleThreaded, Mutex, SharedMutex
>
class IdGenerator;
```

**Five policy axes, all resolved at compile time.** The compiler sees through the abstraction completely—no vtables, no function pointers, no runtime dispatch.

### The Mechanism: Policy Inheritance with EBO

```cpp
template <...>
class IdGenerator : private AllocationPolicy
                  , private RecyclingPolicy
                  , private ConcurrencyPolicy
{
    underlying_type base_id_;
    ActiveIdTracker<underlying_type> ids_in_use_;
};
```

For `SingleThreadedPolicy` (stateless), Empty Base Optimization eliminates storage overhead:

```cpp
sizeof(SimpleIdGenerator<uint64_t>) == 
    sizeof(uint64_t) +           // base_id_
    sizeof(unordered_set) +      // ids_in_use_
    sizeof(deque)                // recycling queue
// Zero bytes for SingleThreadedPolicy
```

### What The Compiler Sees

```cpp
// This high-level code:
SimpleIdGenerator<uint64_t> gen(1);
auto id = gen.generate();

// Compiles to essentially:
if (!recycled_.empty()) {
    uint64_t id = recycled_.front();
    recycled_.pop_front();
    ids_in_use_.insert(id);
    return id;
}
return base_id_++;
```

No indirection. No virtual calls. The policy template parameters are **erased** at compile time, leaving only the selected implementation.

---

## Feature Inventory

### 1. Allocation Policies: Three Strategies

| Policy | Behavior | Use Case |
|--------|----------|----------|
| `SequentialAllocationPolicy` | Monotonically increasing | Default; predictable, debuggable |
| `BoundedSequentialAllocationPolicy` | Sequential with hard limit | Array indexing, protocol constraints |
| `RandomAllocationPolicy` | Uniform random distribution | Security, unpredictability |

```cpp
// Sequential (default): 1, 2, 3, ...
SimpleIdGenerator<uint64_t> seq_gen(1);

// Bounded: 0-999 only, then error
BoundedIdGenerator<uint64_t> bounded_gen(0, 1000);

// Random with seed for reproducibility
RandomIdGenerator<uint64_t> rand_gen(seed_tag, 42);
```

**Mechanism:** Each policy implements `allocate_new()` differently. The compiler inlines the selected implementation directly.

### 2. Recycling Policies: Three Strategies

| Policy | Order | Complexity | Use Case |
|--------|-------|------------|----------|
| `ImmediateRecyclingPolicy` | FIFO queue | O(1) | General purpose |
| `MinRecyclingPolicy` | Smallest first | O(log n) | Dense ID ranges for cache locality |
| `NoRecyclingPolicy` | Never recycle | O(1) | Audit trails, security |

```cpp
// FIFO: release(1), release(2), generate() → 1
SimpleIdGenerator<uint64_t> fifo_gen(1);

// Min-First: release(2), release(1), generate() → 1 (smallest)
DenseIdGenerator<uint64_t> dense_gen(1);
```

**The HPC Case for Min-First:** When IDs index into arrays, keeping active IDs dense improves cache utilization. Min-first recycling fills gaps from the bottom.

### 3. StrongId Integration: Compile-Time Type Safety

```cpp
using UserId = StrongId<uint64_t, struct UserTag>;
using OrderId = StrongId<uint64_t, struct OrderTag>;

IdGenerator<UserId> user_gen(1000);
IdGenerator<OrderId> order_gen(1);

auto user = user_gen.generate();   // Expected<UserId, IdError>
auto order = order_gen.generate(); // Expected<OrderId, IdError>

// Compile error: incompatible types
// if (*user == *order) { }  // ERROR: no operator== for UserId vs OrderId
```

**Mechanism:** IdGenerator detects `StrongId` via `value_type` trait extraction and wraps results appropriately. The underlying arithmetic uses the raw type; the API surface uses the strong type.

### 4. Thread-Safe Variants

```cpp
// Single-threaded: zero synchronization overhead
SimpleIdGenerator<uint64_t> st_gen(1);

// Mutex-protected: safe for concurrent access
ThreadSafeIdGenerator<uint64_t> ts_gen(1);

// SharedMutex: optimized for read-heavy workloads
using ReaderHeavyGen = IdGenerator<uint64_t,
    SequentialAllocationPolicy<uint64_t>,
    ImmediateRecyclingPolicy<uint64_t>,
    ExpectedErrorPolicy<uint64_t, IdError>,
    SharedMutexPolicy>;
```

**Mechanism:** `ConcurrencyPolicy` provides `lock()`, `unlock()`, `lock_shared()`, `unlock_shared()`. For `SingleThreadedPolicy`, these are empty inline functions that compile away.

### 5. RAII IdGuard: Exception-Safe ID Management

```cpp
{
    auto guard = gen.scoped_id();  // Generate ID
    if (!guard) { /* handle error */ }
    
    uint64_t id = guard->get();
    // ... use id, possibly throwing ...
    
}  // ID automatically released, even if exception thrown
```

**Mechanism:** `IdGuard` stores a reference to the generator and the ID. Destructor calls `release()`. Move-only semantics prevent double-release.

### 6. Batch Operations: Amortized Lock Cost

```cpp
// Generate 100 IDs with single lock acquisition
auto ids = gen.generate_batch(100);
if (!ids) { /* handle partial failure */ }

// Release multiple IDs atomically
std::vector<uint64_t> to_release = {1, 5, 9, 13};
auto result = gen.release_batch(to_release);
```

**Mechanism:** Batch operations acquire the lock once, perform all operations, then release. For `ThreadSafeIdGenerator`, this amortizes mutex overhead across N operations.

### 7. Overflow Detection via Expected

```cpp
auto result = gen.generate();
if (!result) {
    switch (result.error()) {
        case IdError::Overflow:
            // ID space exhausted
            break;
        case IdError::AlreadyInUse:
            // Collision during random allocation
            break;
    }
}
```

**Mechanism:** The allocation policy checks against `std::numeric_limits<IdType>::max()` before incrementing, detecting overflow before it occurs. Errors propagate via `Expected<IdType, IdError>` without exceptions.

---

## Why Not Alternatives?

| If You Need... | Why Not Hand-Rolled | Why Not boost::uuid | Why Not Database Sequences | Fat-P Advantage |
|----------------|---------------------|---------------------|---------------------------|-----------------|
| Policy flexibility | Fixed behavior per implementation | No policies | No policies | 5 orthogonal policy axes |
| Recycling | Usually missing | Never recycles (128-bit) | Complex transaction handling | 3 recycling strategies |
| StrongId integration | Manual wrapping | Not applicable | Not applicable | Automatic type extraction |
| Zero dependencies | Works but error-prone | Requires Boost | Requires database | Header-only, STL only |
| Deterministic testing | Random without seed control | Random only | Server-dependent | Seeded random option |
| Thread safety options | Usually forgotten | Thread-safe only | Connection-level | Single-threaded or mutex |

**The Sweet Spot:** IdGenerator is the only option combining:
- ✅ Compile-time policy composition (not runtime configuration)
- ✅ StrongId integration (compile-time type safety)
- ✅ RAII guards (exception-safe lifecycle)
- ✅ Batch operations (amortized lock cost)
- ✅ Zero external dependencies

---

## The "Forever Stuck" Reality

**Compiler Reality Check:** The C++ standard committee will never provide ID generation facilities. This is a domain-specific concern that varies wildly between applications:

- Game engines need dense, recyclable IDs for entity indexing
- Financial systems need non-recyclable audit trails
- Distributed systems need globally unique (often random) IDs
- Embedded systems need bounded, overflow-safe counters

No single standard API could satisfy all these requirements. IdGenerator's policy-based design addresses this permanently—not as a temporary shim, but as an architectural solution that composes exactly the behavior each domain requires.

---

## Performance Characteristics

### Benchmark Results

| Operation | Mechanism | ThreadSafe Overhead |
|-----------|-----------|---------------------|
| Generate (fresh) | Counter increment + hash insert | + mutex lock/unlock per operation |
| Generate (recycled) | Queue pop + hash insert | + mutex lock/unlock per operation |
| Release | Hash erase + queue push | + mutex lock/unlock per operation |
| `is_active()` query | Hash lookup | + mutex lock/unlock per query |
| Batch generate (N) | Single lock, N × O(1) operations | One lock acquisition amortized across N |

See `components/IdGenerator/results/` for current platform-specific benchmark data.

### Complexity Analysis

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| `generate()` | O(1) amortized | Hash insert + counter or queue pop |
| `release()` | O(1) amortized | Hash erase + queue push |
| `is_active()` | O(1) average | Hash lookup |
| Internal max-ID tracking | O(1) cached | Lazy max recomputation in `ActiveIdTracker` |
| `generate_batch(n)` | O(n) | N × O(1) operations |

### Where Fat-P Wins

- **Policy composition:** Different parts of your application need different ID behaviors—IdGenerator provides all combinations
- **StrongId integration:** Compile-time type safety with zero runtime cost
- **Batch operations:** Reduce lock contention in high-throughput scenarios
- **Dense recycling:** Min-first policy keeps IDs cache-friendly

### Where Fat-P Loses (Honesty Builds Trust)

- **Distributed uniqueness:** For multi-node systems, UUIDs or Snowflake IDs are more appropriate
- **128-bit IDs:** If you need 128-bit identifiers, IdGenerator's integral focus doesn't fit
- **Persistence:** IdGenerator is in-memory only; database sequences survive restarts
- **Simple use cases:** If you just need a counter, `std::atomic<uint64_t>` is simpler

---

## Integration Points

```
IdGenerator.h
    ↓ uses
ConcurrencyPolicies.h   (SingleThreaded, Mutex, SharedMutex)
Expected.h              (Expected<T, IdError> error handling)
StrongId.h              (Type-safe ID wrapper detection)
    ↓ used by
Entity-component systems (entity ID management)
Resource managers (handle allocation)
```

---

## Test Suite: 33 Cases, 1,537 Lines

### Coverage Categories

| Category | Tests | Coverage |
|----------|-------|----------|
| Basic functionality | 3 | Sequential generation, StrongId, error handling |
| RAII guards | 3 | Scoped IDs, move semantics, default construction |
| Random allocation | 4 | Uniqueness, seeding, reproducibility |
| Thread safety | 2 | Concurrent generation, concurrent queries |
| Recycling policies | 5 | FIFO, Min-first, no recycling |
| Batch operations | 7 | Generation, release, rollback |
| Edge cases | 5 | Overflow, double release, boundaries |
| Custom policies | 3 | Custom allocation, bounded, tracker |

---

## Final Assessment

IdGenerator delivers on the fat_p promise through three pillars:

### 1. Permanence
The C++ standard will never provide ID generation—the requirements vary too much across domains. IdGenerator's policy-based design permanently addresses this gap by composing exactly the behavior each application needs.

### 2. Specialization
HPC workloads need dense ID ranges (Min-first recycling), game engines need fast recycling (FIFO), financial systems need audit trails (no recycling). IdGenerator serves all three with zero runtime overhead through compile-time policy selection.

### 3. Control
Five orthogonal policy axes (allocation, recycling, error handling, concurrency, type safety) let architects specify exactly the behavior they need. No runtime configuration, no virtual dispatch—the policy IS the implementation.

**Architectural Verdict:** IdGenerator transforms the error-prone "incrementing counter" pattern into a **type-safe, overflow-protected, policy-composed** ID management system. The generated code is identical to hand-optimized implementations, but the source code expresses architectural intent through policy composition rather than scattered conditionals.

---

*IdGenerator.h (1163 lines) — Fat-P Library*
