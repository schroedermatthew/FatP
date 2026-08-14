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
    typename ErrorPolicy,       // ExpectedErrorPolicy<T, IdError> (the only one shipped)
    typename ConcurrencyPolicy  // SingleThreaded, Mutex, SharedMutex
>
class IdGenerator;
```

**Five policy axes, all resolved at compile time.** The compiler sees through the abstraction completely—no vtables, no function pointers, no runtime dispatch.

### The Mechanism: Policy Inheritance

```cpp
template <...>
class IdGenerator : private AllocationPolicy
                  , private RecyclingPolicy
                  , private ConcurrencyPolicy
{
    underlying_type mBaseId;
    ActiveIdTracker<underlying_type> mIdsInUse;
    std::size_t mEpoch;              // bumped by reset(); guards refuse to cross it
};
```

Inheriting the policies rather than holding them by value is what lets the compiler inline every policy call. It does not make them free. `SingleThreadedPolicy` holds a `mutable NoOpLock` member, so it is not an empty class and Empty Base Optimization does not apply to it — `std::is_empty_v<SingleThreadedPolicy>` is `false`. A generator's footprint is the allocation policy's counters and flags, the recycling container (`std::deque` for FIFO, `std::set` for min-first), the active-ID `std::unordered_set` plus its cached maximum, the base ID, and the reset epoch. Measure `sizeof` on your own toolchain rather than summing those parts: the containers dominate and their sizes are implementation-specific.

### What The Compiler Sees

```cpp
// This high-level code:
SimpleIdGenerator<uint64_t> gen(1);
auto id = gen.generate();

// Compiles to essentially (pseudocode):
if (!recycled_.empty()) {
    uint64_t id = recycled_.front();
    recycled_.pop_front();
    ids_in_use_.insert(id);
    return id;
}
// Not a bare counter: the sequential policy takes the larger of its own
// cursor and one past the tracker's current maximum, which is what keeps
// recycling gaps and the counter from disagreeing.
uint64_t id = max(next_id_, ids_in_use_.max_element() + 1);
next_id_ = id + 1;
ids_in_use_.insert(id);
return id;
```

No indirection. No virtual calls. The policy template parameters are **erased** at compile time, leaving only the selected implementation.

---

## Feature Inventory

### 1. Allocation Policies: Three Strategies

| Policy | Behavior | Use Case |
|--------|----------|----------|
| `SequentialAllocationPolicy` | Monotonically increasing | Default; predictable, debuggable |
| `BoundedSequentialAllocationPolicy` | Sequential with hard limit | Array indexing, protocol constraints |
| `RandomAllocationPolicy` | Uniform random distribution (`std::mt19937_64`) | Non-sequential IDs, seeded reproducibility |

```cpp
// Sequential (default): 1, 2, 3, ...
SimpleIdGenerator<uint64_t> seq_gen(1);

// Bounded: 0-1000 inclusive, then error. There is no bounded generator alias,
// and IdGenerator's constructor forwards only the base ID, so the bound is set
// by using the policy directly (or by wrapping it in a policy that fixes the
// bound at compile time).
BoundedSequentialAllocationPolicy<uint64_t> bounded_policy(0, 1000);

// Random with seed for reproducibility
RandomIdGenerator<uint64_t> rand_gen(seed_tag, 42);
```

**Mechanism:** Each policy implements `next_id(max_id, first_call)` differently, returning `std::optional<IdType>` where `nullopt` means the domain is spent. The compiler inlines the selected implementation directly.

### 2. Recycling Policies: Four Strategies

| Policy | Order | Complexity | Use Case |
|--------|-------|------------|----------|
| `ImmediateRecyclingPolicy` | FIFO queue | O(1) | General purpose |
| `MinRecyclingPolicy` | Smallest first | O(log n) | Dense ID ranges for cache locality |
| `NoRecyclingPolicy` | Never recycle | O(1) | Audit trails, security |
| `SparseRecyclingPolicy` | Smallest first, over the whole domain | O(log I) | Claiming persisted IDs by value |

```cpp
// FIFO: release(1), release(2), generate() → 1
SimpleIdGenerator<uint64_t> fifo_gen(1);

// Min-First: release(2), release(1), generate() → 1 (smallest)
DenseIdGenerator<uint64_t> dense_gen(1);

// Sparse: reserve a specific ID without consuming everything below it
SparseIdGenerator<uint32_t> sparse_gen(0, 1'000'000);
sparse_gen.claim(999'999);          // O(log I), NOT O(gap)
sparse_gen.generate();              // 0 -- nothing below the claim was spent
```

**The HPC Case for Min-First:** When IDs index into arrays, keeping active IDs dense improves cache utilization. Min-first recycling fills gaps from the bottom.

**The Persistence Case for Sparse:** The first three policies hold only what was released back to them, so an ID that was never issued in *this* run can only be reserved by generating up to it and releasing everything on the way — O(gap), and usually capped by an arbitrary attempt ceiling that turns a large gap into a failure. `SparseRecyclingPolicy` holds the complete domain as disjoint intervals, so reserving a persisted ID is one interval lookup regardless of its numeric value.

It is the only policy that offers `claim()`, and the only one where `recycled_count()` counts never-issued IDs — so a zero from it means the domain is exhausted, not that nothing is pending. Exhaustion is an empty interval set rather than a question for the allocation policy.

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
    id_generator::ExpectedErrorPolicy<uint64_t, IdError>,
    SharedMutexPolicy>;
```

**Mechanism:** `ConcurrencyPolicy` provides `lock()` and `lock_shared()`, each returning an RAII guard that releases on scope exit; there is no separate `unlock()` in the interface IdGenerator uses. For `SingleThreadedPolicy` both guards are empty types, so the acquisition and the release compile away.

### 5. RAII IdGuard: Exception-Safe ID Management

```cpp
{
    auto guard = gen.scoped_id();  // Generate ID
    if (!guard) { /* handle error */ }
    
    uint64_t id = guard->get();
    // ... use id, possibly throwing ...
    
}  // ID automatically released, even if exception thrown
```

**Mechanism:** `IdGuard` stores a pointer to the generator, the ID, and the generator's epoch at construction. The destructor calls `release_if_current()`, which releases only if `reset()` has not bumped the epoch since — a guard that outlived a `reset()` goes inert rather than releasing an ID the generator has already reissued to someone else. Move-only semantics prevent double-release.

### 6. Batch Operations: Amortized Lock Cost

```cpp
// Generate 100 IDs with single lock acquisition
auto ids = gen.generate_batch(100);
if (!ids) { /* nothing was kept: the batch rolled itself back */ }

// Release multiple IDs under one lock. NOT atomic: release_batch commits each
// ID as it goes and returns on the first invalid one, leaving the releases it
// already performed in place.
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
            // ID space exhausted, or the next ID would be the type's
            // reserved invalid() sentinel, which is never issued
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

### Operation Mechanisms

| Operation | Mechanism | ThreadSafe Overhead |
|-----------|-----------|---------------------|
| Generate (fresh) | Counter increment + hash insert | + mutex lock/unlock per operation |
| Generate (recycled) | Queue pop + hash insert | + mutex lock/unlock per operation |
| Release | Hash erase + queue push | + mutex lock/unlock per operation |
| `is_active()` query | Hash lookup | + mutex lock/unlock per query |
| Batch generate (N) | Single lock, N × O(1) operations | One lock acquisition amortized across N |

No benchmark ships with this component yet: `components/IdGenerator/benchmarks/` and `components/IdGenerator/results/` are empty. The table above states mechanism, not measurement.

### Complexity Analysis

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| `generate()` | O(1); O(n) on the first call after the highest active ID is released | Hash insert + counter or queue pop, plus the max lookup below |
| `release()` | O(1) amortized with FIFO recycling, O(log n) with min-first | Hash erase + `deque` push, or `set` insert for `MinRecyclingPolicy` |
| `is_active()` | O(1) average | Hash lookup |
| Internal max-ID tracking | O(1) while the cache is valid, O(n) to rebuild it | `ActiveIdTracker` caches the maximum and invalidates it when that element is erased; the next `max_element()` is a linear `std::max_element` scan of the `unordered_set` |
| `generate_batch(n)` | O(n) | N × O(1) operations |
| `claim(id)` (sparse only) | O(log I) in free-interval count | Interval lookup, then at most one split. Independent of `id`'s numeric value and of its distance from any active ID |
| `release()` (sparse only) | O(log I) | Merge with adjacent intervals; allocation-free, funded by the credit reserved at activation |
| `recycled_count()` (sparse only) | O(I) | Checked, saturating sum of interval cardinalities |

### Where Fat-P Wins

- **Policy composition:** Different parts of your application need different ID behaviors—IdGenerator provides all combinations
- **StrongId integration:** Compile-time type safety with zero runtime cost
- **Batch operations:** Reduce lock contention in high-throughput scenarios
- **Dense recycling:** Min-first policy keeps IDs cache-friendly
- **Claim by value:** Sparse recycling reserves a specific persisted ID in O(log I), where the alternative is a gap walk with an arbitrary attempt ceiling

### Where Fat-P Loses (Honesty Builds Trust)

- **Distributed uniqueness:** For multi-node systems, UUIDs or Snowflake IDs are more appropriate
- **128-bit IDs:** If you need 128-bit identifiers, IdGenerator's integral focus doesn't fit
- **Persistence:** IdGenerator is in-memory only; database sequences survive restarts. `SparseRecyclingPolicy` makes *reinstating* persisted IDs cheap, but the persisting is still yours
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

## Test Suite

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
| Active ID tracking | 3 | Lazy max recompute, dirty-max insert |
| Post-review coverage | 15 | AlreadyInUse reachability, retry loop, exhausted latch, batch pool restoration, saturated revert, reset-invalidated guards, guard move-assignment, sentinel refusal, StrongId batch, movability, bounded upper bound, random base as minimum, impossible batch count |
| Sparse ID claiming | 36 | Claim at endpoints, in gaps and on singleton intervals; single-value domains; order independence; duplicate and out-of-domain refusal; all four release/merge transitions asserted on interval count; base release on a full-width domain; staged generation; batch exclusion, preflight and rollback; `release_batch`; exhaustion at the configured ceiling; exact-then-saturating free count; domain rebuild on reset; credit accounting; exception injection at each allocating step; **measured allocation-freedom of every return path**; gap-independence with a counting comparator over a fragmented domain; revert suppression; guard release; constructor routing; sentinel normalization; moved-from consistency; and **contended multithreaded claiming over a real `shared_mutex`** |

Total: 84 tests. The sparse group was additionally put through two mutation gates against the header. The second followed an adversarial coverage audit that surfaced four defects in the landed code — a batch preflight gated on the wrong width, a moved-from policy with a stale credit count, an inverted domain that could issue a `StrongId`'s reserved sentinel, and a `Compare` parameter that promised more generality than it delivered. All are fixed and carry regression tests; every mutant in the current gate dies. The one provably equivalent mutation, and the absence of compile-fail coverage, are recorded in the design note rather than papered over.

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

*IdGenerator.h — Fat-P Library*
