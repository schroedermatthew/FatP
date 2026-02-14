# Fat-P User Manual Coverage Report

**Date:** 2026-02-14
**Unique component families:** 72
**Components needing user manuals:** 39
**User manuals written:** 3

---

## Current Coverage

Three components have user manuals today: SparseSet, ThreadPool, and WorkQueue.
These follow the Teaching Documents Style Guide format (YAML front matter,
opening story, architecture before API, migration chapter, troubleshooting,
API reference).

39 user-facing runtime components need manuals and don't have them. 10 more
could benefit at lower priority. The remaining ~20 are infrastructure,
compile-time constructs, or internal headers that don't warrant standalone
user manuals.

---

## Definitely Need User Manuals (39)

Grouped by domain, sorted by complexity (header line count) within each group.
Higher line counts generally mean more complex APIs with more for a manual to
explain.

### Data Structures / Containers (13)

| Component | Lines | Complexity notes |
|---|---|---|
| **Tensor** | 6,493 | Multi-dimensional array, einsum, stride policies, 7 headers — most complex component in the library |
| **SmallVector** | 2,138 | SBO-optimized vector, iterator invalidation rules, growth semantics |
| **FastHashMap** | 2,146 | Open-addressing hash map, probe sequences, hash policy |
| **AlignedVector** | 1,967 | SIMD-aligned storage, alignment requirements, allocator interaction |
| **StableHashMap** | 1,613 | Pointer-stable hash map, reference stability guarantees |
| **IntrusiveList** | 1,447 | Hook-based list, ownership model, node lifetime rules |
| **FlatMap** | 1,399 | Sorted vector-backed map, binary search, iterator stability |
| **BitSet** | 1,236 | Bit manipulation, dynamic sizing, SIMD-accelerated operations |
| **SlotMap** | 959 | Generational handles, ABA safety, reuse semantics |
| **FlatSet** | 850 | Sorted vector-backed set, companion to FlatMap |
| **StringPool** | 560 | String interning, deduplication, handle-based lookup |
| **CircularBuffer** | 493 | Fixed-capacity ring buffer, overwrite policies |
| **HpcVector** | 1,053 | SIMD-aligned numerics vector, BLAS-like operations |

### Concurrency (4)

| Component | Lines | Complexity notes |
|---|---|---|
| **LockFreeQueue** | 433 | MPMC lock-free queue, memory ordering, ABA prevention |
| **LockFreeRingBuffer** | 643 | SPSC ring buffer, wait-free paths, cache-line padding |
| **ObjectPool** | 801 | Pooled allocation, thread-safety model, RAII handles |
| **RateLimiter** | 441 | Token bucket, burst handling, clock policy |

### Serialization / IO (6)

| Component | Lines | Complexity notes |
|---|---|---|
| **JsonLite** | 7,234 | Full JSON parser/serializer — second-largest component |
| **JsonStreamLite** | 1,339 | SAX-style streaming JSON |
| **CborStreamLite** | 1,252 | Streaming CBOR encoder/decoder |
| **BinaryLite** | 743 | Binary serialization, endianness, type safety |
| **MemoryMappedFile** | 548 | Platform-abstracted mmap, Windows/POSIX |
| **CborLite** | 493 | CBOR codec, RFC 8949 compliance |

### Math / Numerics (5)

| Component | Lines | Complexity notes |
|---|---|---|
| **CheckedArithmetic** | 6,302 | Overflow detection, SIMD paths (SSE2/AVX2/NEON), 10 headers |
| **SimdVector** | 2,067 | SIMD-accelerated vector operations, ISA fallbacks |
| **NumaAllocator** | 1,170 | NUMA-aware memory, topology detection, binding |
| **FloatingPointComparison** | 560 | ULP, relative, absolute comparison strategies |
| **HpcVector** | *(listed under Containers)* | |

### Error Handling / Control Flow (5)

| Component | Lines | Complexity notes |
|---|---|---|
| **Expected** | 4,093 | Monadic error handling, policy-based, EXPECTED_TRY macro |
| **ScopeGuard** | 1,428 | RAII guard with dismiss/commit, policy system, 3 headers |
| **Signal** | 1,002 | Signal/slot, connection lifetime, SBO for small slot counts |
| **StateMachine** | 573 | State/event/transition framework, guard/action system |
| **CoroutineTask** | 580 | C++20 coroutine task, co_await semantics |

### Diagnostics (2)

| Component | Lines | Complexity notes |
|---|---|---|
| **DiagnosticLogger** | 2,074 | Structured logging, sink system, 4 headers |
| **Stacktrace** | 1,148 | Platform-abstracted stack capture, symbol resolution |

### Utilities (4)

| Component | Lines | Complexity notes |
|---|---|---|
| **FeatureManager** | 2,365 | Feature flags, dependency graph, rollout control |
| **ServiceLocator** | 1,766 | Type-erased service registry, lifetime management |
| **IdGenerator** | 1,279 | Thread-safe ID generation, uniqueness guarantees |
| **Factory** | 911 | Type-erased factory, registration, polymorphic creation |

---

## Could Benefit — Lower Priority (10)

These are smaller utilities, narrow-audience tools, or thin wrappers where
Doxygen + a few usage examples in the header may be sufficient.

| Component | Lines | Notes |
|---|---|---|
| AllocationStrategies | 338 | Policy library; users mostly interact via AlignedVector |
| CacheUtilities | 897 | Prefetch/flush hints; narrow HPC audience |
| EnhancedBoundsChecking | 510 | Debug-mode bounds checking; tooling, not user API |
| EnumPlus | 604 | Enhanced enum utilities |
| NumaAlignedAllocator | 258 | Thin wrapper over NumaAllocator |
| PolicyIterator | 616 | Iterator with policies; documented through usage in other components |
| Reflection | 684 | Compile-time reflection utilities |
| SlidingFileWindow | 376 | Windowed file reader; companion to MemoryMappedFile |
| Stringify | 611 | String conversion; mostly self-documenting API |
| StrongId | 362 | Typed ID wrapper; simple template |

---

## Not Applicable (~20)

Infrastructure headers, compile-time constructs, policy tags, detection macros,
test/benchmark tooling, integration shims, and configuration headers. These are
documented through Doxygen comments and don't warrant standalone user manuals:
Concepts, ConcurrencyPolicies, Constexpr* (4 headers), ContractException,
CppFeatureDetection, DebugOnly, enforce (5 headers), EnforcedInit, Equality* (3
headers), FatPBenchmark* (3 headers), FatPBinary, FatPCbor, FatPConcepts,
FatPConfig, FatPJson, FatPTest, PlatformDetection, ScopeGuardPolicies,
ScopeGuardExpected, SimdDetection, ValueGuard, ViewLifetimeTracking.

---

## Summary

| Category | Count | Done | Missing |
|---|---|---|---|
| Definitely need | 39 | 3 | **36** |
| Could benefit | 10 | 0 | 10 |
| Not applicable | ~20 | — | — |

Coverage: **3 / 39** (7.7%) of components that need user manuals have them.

---

## Suggested Prioritization

### Tier 1 — Highest value, write first (10)

These are the most complex components (highest line counts), the ones users are
most likely to reach for first, and the ones where incorrect usage causes the
most damage. Ordered by a combination of complexity and user impact.

| # | Component | Lines | Why first |
|---|---|---|---|
| 1 | **JsonLite** | 7,234 | Second-largest component; every project needs a JSON parser; complex parse/serialize API |
| 2 | **Tensor** | 6,493 | Largest component family; HPC users need einsum/stride/layout guidance |
| 3 | **CheckedArithmetic** | 6,302 | 10 headers, SIMD paths, policy system; easy to misuse without guidance |
| 4 | **Expected** | 4,093 | Monadic error handling is unfamiliar to many C++ devs; policy system needs explanation |
| 5 | **CSRMatrix** | 3,747 | HPC sparse matrix; construction, SpMV, parallel patterns all need documentation |
| 6 | **SmallVector** | 2,138 | Most-requested container type; SBO semantics confuse users |
| 7 | **FastHashMap** | 2,146 | Hash map is the most-used data structure; probe policy needs explanation |
| 8 | **DiagnosticLogger** | 2,074 | Logging is first thing integrated; sink/format/filter system needs a guide |
| 9 | **SimdVector** | 2,067 | SIMD semantics, ISA fallbacks, alignment requirements |
| 10 | **AlignedVector** | 1,967 | Foundation container for HPC; alignment + allocator interaction |

### Tier 2 — High value, write next (10)

| # | Component | Lines | Why |
|---|---|---|---|
| 11 | **ServiceLocator** | 1,766 | Pattern with lifetime pitfalls |
| 12 | **StableHashMap** | 1,613 | Reference stability is the key differentiator; needs clear explanation |
| 13 | **IntrusiveList** | 1,447 | Intrusive containers are unfamiliar; hook ownership rules are subtle |
| 14 | **ScopeGuard** | 1,428 | Policy system, dismiss/commit semantics, exception safety |
| 15 | **FlatMap** | 1,399 | Sorted-vector semantics differ from std::map; needs migration guide |
| 16 | **JsonStreamLite** | 1,339 | SAX vs DOM is a design choice that needs guidance |
| 17 | **CborStreamLite** | 1,252 | Streaming CBOR is niche; users need worked examples |
| 18 | **IdGenerator** | 1,279 | Uniqueness guarantees, thread-safety model |
| 19 | **BitSet** | 1,236 | Dynamic sizing vs std::bitset fixed sizing; SIMD acceleration |
| 20 | **NumaAllocator** | 1,170 | NUMA topology is confusing; users need binding/placement guidance |

### Tier 3 — Everything else (19)

The remaining 19 NEED components are smaller (< 1,100 lines each) and can be
written in any order. Signal, SlotMap, Factory, FeatureManager, and Stacktrace
are the highest-value in this tier. The concurrency primitives (LockFreeQueue,
LockFreeRingBuffer, ObjectPool) should be written together since they share
memory-ordering concepts.

### Pairing with benchmarks

Several components need both a user manual and a benchmark. Where possible,
write the benchmark first — the performance data informs the "Performance Rules
of Thumb" section of the user manual and provides concrete numbers for the
"When to Use / When Not To" decision guide.

| Component | Needs manual | Needs benchmark | Write benchmark first? |
|---|---|---|---|
| CSRMatrix | Yes | Yes | Yes — SpMV throughput defines the pitch |
| Tensor | Yes | Yes | Yes — operation throughput drives the manual's advice |
| CheckedArithmetic | Yes | Yes | Yes — SIMD vs scalar overhead is the core question |
| Signal | Yes | Yes | Yes — dispatch latency vs Boost.Signals2 is the differentiator |
| JsonLite | Yes | Yes | Yes — parse/serialize throughput vs nlohmann is the argument |
| Expected | Yes | Yes | Either order — manual can reference overhead in abstract |
| SmallVector | Yes | Has benchmark (needs competitors) | Fix benchmark competitors first |
| FastHashMap | Yes | Has benchmark (FatPHashMap, with competitors) | Manual can reference existing data |
