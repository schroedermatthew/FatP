# FAT-P Benchmark Analysis

**Data:** 23 components, 4 platforms (MSVC 19.50 local PC, GCC 14.2 CI, Clang 17.0 CI, MSVC 19.44 CI)
**Date:** 2026-02-16
**Methodology:** Round-robin, randomized library order, median-primary, 15 measured runs (local) / 20 (CI)

This document summarizes competitive benchmark results across all measured FAT-P components. Per-component details with full tables and raw output are in the individual `Benchmark Results - {Component}.md` files.

---

## Measurement Limitations

These results come from two environments: one Windows desktop PC and shared-tenancy Azure CI runners. Both have significant caveats.

**Local PC (MSVC 19.50, Intel Core Ultra 9 285K):** CPU frequency variance of 30-70% during stabilization. The harness waits for <10% variance but often settles at 62-70% of the 3686 MHz base clock. Absolute ns/op values are approximate. Relative rankings within each benchmark section are more reliable since all competitors observe the same frequency.

**CI runners (GCC 14.2, Clang 17.0, MSVC 19.44 on Azure):** Shared-tenancy introduces potential neighbor noise. No CPU stabilization. Results are reproducible across runs but may differ from dedicated hardware. GCC and Clang results are very close to each other (same CI infrastructure), which provides some confidence in reproducibility.

**General:** These benchmarks measure synthetic microbenchmark throughput. They do not measure behavior under real-world access patterns, memory pressure, or mixed workloads. Established competitors (Boost, Abseil, LLVM, EASTL) have years of cross-platform validation and real-world deployment that FAT-P does not. Benchmark results are one input into an evaluation, not a verdict.

---

## Hash Maps

### FastHashMap (flat, tombstone-based)

At N=1M (MSVC), FastHashMap[TS]+SplitMix64 vs competitors:

|             | Insert | Find  | Miss  | Erase  | Churn  |
|-------------|--------|-------|-------|--------|--------|
| FHM[TS]+SM64 | 9.96  | 15.26 | 4.03  | 12.53  | 15.23  |
| boost::flat | 12.02  | 8.95  | 2.37  | 9.95   | 9.42   |
| absl::flat  | 22.93  | 17.85 | 4.54  | 22.09  | 26.40  |
| llvm::Dense | 18.87  | 10.27 | 14.62 | 8.96   | 20.20  |
| std::umap   | 90.96  | 27.70 | 34.93 | 127.90 | 212.30 |

FastHashMap has a consistent insert advantage over boost::unordered_flat_map (9.96 vs 12.02 ns at N=1M, 4.19 vs 5.30 ns at N=10K, holds on GCC and Clang). boost::unordered_flat_map is faster on find (8.95 vs 15.26), miss (2.37 vs 4.03), and churn (9.42 vs 15.23). The churn gap is significant and consistent across platforms (15.23 vs 9.42 MSVC, 29.71 vs 13.81 GCC).

**Hash function sensitivity (MSVC vs GCC):** On MSVC, SplitMix64 cuts insert time nearly in half (13.07→9.96 ns). On GCC, the improvement is negligible (15.70 vs 15.44 ns) because libstdc++ `std::hash` is already reasonable. MSVC's `std::hash<uint64_t>` is significantly weaker.

### StableHashMap (node-based, reference-stable)

Compared against other node-based maps at N=1M (MSVC), StableHashMap[Block]+SM64:

|               | Insert | Find  | Miss | Erase | Churn  |
|---------------|--------|-------|------|-------|--------|
| SHM[Block]+SM64 | 17.53 | 10.26 | 3.04 | 24.95 | 31.73  |
| boost::node   | 38.24  | 10.70 | 4.52 | 81.16 | 103.11 |
| absl::node    | 42.62  | 17.75 | 8.66 | 104.09| 123.34 |
| folly::F14Node (GCC) | 46.41 | 9.33 | 8.51 | 112.02 | 74.48 |
| std::umap     | 90.96  | 27.70 | 34.93| 127.90| 212.30 |

Insert, erase, and churn are substantially faster than boost::node. Find is at parity with boost::node (10.26 vs 10.70); miss is ~50% faster than boost but this is a single data point — the pattern holds on GCC and Clang but margins vary.

**Block allocator dependency:** The block allocator variant is required for these results. Without it, StableHashMap degrades to 40.43 ns insert / 105.76 ns erase at N=1M — comparable to std::unordered_map on erase. The default allocator path is not competitive.

### FlatMap / FlatSet (sorted, contiguous)

At N=1K (MSVC), sorted bulk operations:

|               | Bulk Build (sorted) | Bulk Insert (sorted) | Bulk Insert (random) |
|---------------|---------------------|----------------------|----------------------|
| fat_p::FlatMap | 1.40               | 5.40                 | 80.90                |
| boost::flat_map | 0.60              | 6.90                 | 86.60                |
| absl::btree_map | 13.30            | 51.10                | 37.40                |
| std::map       | 22.80              | 60.60                | 59.40                |

fat_p::FlatMap and boost::flat_map trade positions depending on the operation. boost is faster on sorted bulk build (0.60 vs 1.40 ns). fat_p is slightly faster on sorted insert (5.40 vs 6.90 ns). absl::btree_map handles random insertion better than either flat map (37.40 vs 80.90 ns) because B-tree insertion is O(log n) vs O(n) shifting. This is the expected tradeoff between flat and tree-based sorted containers.

---

## Queues and Buffers

### CircularBuffer (SPSC) and LockFreeRingBuffer

| Library              | Uncontended | SPSC Threaded | Batch |
|----------------------|-------------|---------------|-------|
| fat_p::CircularBuffer | 0.95       | 42.61         | 0.86  |
| fat_p::LockFreeRingBuffer | 1.17  | 9.14          | 0.93  |
| boost::lockfree::spsc | 1.25      | 21.04         | 0.89  |
| moodycamel::BRWCB    | 20.63      | 66.72         | 15.93 |
| std::mutex+deque     | 22.44      | 88.66         | 3.70  |

CircularBuffer is fastest in the uncontended single-threaded case. Under actual thread contention, its mutex becomes a bottleneck (42.61 ns) and LockFreeRingBuffer is faster (9.14 ns, also faster than boost::lockfree::spsc at 21.04 ns). This is the expected behavior — CircularBuffer is designed for single-threaded or externally-synchronized use, not as a concurrent queue.

### WorkQueue (sharded MPMC)

Scaling behavior (ns/op):

| Threads | WorkQueue | LockFreeQueue | moodycamel | mutex+queue |
|---------|-----------|---------------|------------|-------------|
| 1T      | 16.1      | 7.1           | 14.6       | 19.2        |
| 2T      | 13.3      | 24.9          | 20.2       | 19.9        |
| 4T      | 27.8      | 73.7          | 32.9       | 50.0        |
| 8T      | 26.1      | 83.8          | 31.3       | 181.1       |
| 12T     | 24.9      | 87.9          | 27.3       | 250.4       |
| 16T     | 29.4      | 87.9          | 25.8       | 222.0       |

WorkQueue pays shard-routing overhead at low thread counts (16.1 vs 7.1 ns single-threaded). At higher counts, sharding eliminates CAS contention and it scales well. moodycamel::ConcurrentQueue matches or slightly beats WorkQueue at 16T (25.8 vs 29.4 ns). Under asymmetric producer/consumer loads, WorkQueue stays consistent at 23-28 ns/op while LockFreeQueue and mutex+queue degrade badly (78-184 ns).

### LockFreeQueue (MPMC)

Benchmarked together with WorkQueue in the scaling table above. Single-threaded throughput (7.1 ns) is the fastest in the field, but degrades under contention due to CAS retries. This is the standard tradeoff with lock-free MPMC queues.

---

## Allocators and Pools

### ObjectPool

MSVC at N=1K (single alloc/dealloc cycle):

| Library              | Median (ns) |
|----------------------|-------------|
| EASTL::fixed_pool    | 2.10        |
| fat_p::ObjectPool    | 2.30        |
| boost::object_pool   | 2.30        |
| foonathan::memory    | 3.40        |
| std::pmr::unsync     | 6.30        |
| new/delete           | 20.60       |

EASTL is slightly faster but doesn't auto-grow. On GCC, the gap between all pool allocators and new/delete narrows significantly (fat_p 5.97 ns vs new/delete 20.19 ns = 3.4x) compared to MSVC (2.30 vs 20.60 = 9.0x). This is because Linux's allocator (glibc malloc) is substantially faster than MSVC's CRT allocator, not because the pools perform differently.

### AllocationStrategies

| Allocator             | Single (ns) | Bulk 100 (ns) | Churn (ns) |
|-----------------------|-------------|---------------|------------|
| boost::pool (raw)     | 1.26        | 186.50        | 1.23       |
| fat_p::BlockAllocator | 2.23        | 199.20        | 1.76       |
| fat_p::PoolAllocator  | 2.60        | 203.70        | 2.50       |
| std::pmr::monotonic   | 2.65        | 233.11        | -          |
| std::pmr::unsync_pool | 4.18        | 697.38        | -          |
| new/delete            | 18.86       | 2395.19       | 18.29      |

boost::pool is ~40% faster on single operations. fat_p allocators sit between boost's raw speed and pmr's standard interface.

---

## Small Collections

### SmallVector

At N=1K push_back (MSVC):

| Library              | Median (ns) |
|----------------------|-------------|
| fat_p::SmallVector   | 0.60        |
| boost::small_vector  | 0.60        |
| llvm::SmallVector    | 0.60        |
| absl::InlinedVector  | 0.60        |
| ankerl::svector      | 0.60        |
| eastl::fixed_vector  | 0.70        |
| std::vector          | 1.00        |

All small vector implementations are at parity for inline-capacity operations. On GCC, fat_p is slightly behind std::vector (1.30 vs 0.90 ns) for push_back N=100. No small vector implementation has a meaningful throughput advantage over the others — the value proposition is allocation avoidance, not per-op speed.

### SparseSet

At N=1K (MSVC):

| Library              | Insert | Contains | Erase | Iterate |
|----------------------|--------|----------|-------|---------|
| llvm::SparseSet<32>  | 0.80   | 2.10     | 1.00  | 0.20    |
| fat_p::SparseSet<32> | 1.00   | 2.30     | 1.00  | 0.20    |
| entt::sparse_set     | 3.10   | 2.70     | 2.80  | 0.20    |
| absl::flat_hash_set  | 18.10  | 6.50     | 5.20  | 2.70    |
| std::unordered_set   | 28.30  | 6.20     | 14.80 | 0.90    |

fat_p::SparseSet and llvm::SparseSet are within measurement noise of each other. Both are an order of magnitude faster than hash-based sets for the integer-key operations sparse sets are designed for.

### SlotMap

At N=10K (MSVC):

| Library              | Insert | Lookup | Erase |
|----------------------|--------|--------|-------|
| plf::hive            | 10.80  | 2.04   | 9.56  |
| fat_p::SlotMap       | 14.53  | 2.68   | 7.20  |
| sg14::slot_map       | 17.83  | 2.81   | 6.04  |
| entt::registry       | 32.21  | 6.06   | 39.04 |
| std::unordered_map   | 43.96  | 13.00  | 28.64 |

Middle of the pack among slot map implementations. plf::hive is faster on insert and lookup but lacks generation-based ABA safety. sg14::slot_map trades blows on individual operations.

---

## Linked Structures

### IntrusiveList

| Library              | Push   | Erase  | Iterate |
|----------------------|--------|--------|---------|
| etl::intrusive_list  | 1.45   | 15129  | 76600   |
| eastl::intrusive     | 1.91   | 4.56   | 78400   |
| boost::intrusive     | 1.95   | 5.20   | 71000   |
| llvm::simple_ilist   | 1.99   | 5.15   | 78300   |
| fat_p::Intrusive (fast) | 2.01 | 5.33  | 78500   |
| fat_p::Intrusive (safe) | 2.44 | 6.42  | 82100   |
| std::list<T*>        | 19.94  | 28.21  | 39500   |

All intrusive list implementations cluster together (1.45-2.44 ns push, 4.56-6.42 ns erase). etl and eastl are marginally faster on push. The safe policy adds ~20% overhead for pointer validation. Iteration is pointer-chasing bound, so all intrusive implementations perform identically.

Note: std::list shows a lower iteration total (39500 vs 78500 ns) because std::list with std::deque backing can have better cache locality than pointer-linked nodes scattered across the heap.

---

## String Processing

### Stringify

| Operation       | fat_p  | fmt    | std::format | std::to_string | ostringstream |
|-----------------|--------|--------|-------------|----------------|---------------|
| Integer         | 8.68   | 18.83  | 39.45       | 7.56           | 266.18        |
| Float           | 82.44  | 72.79  | 83.47       | 195.31         | 388.00        |
| Concatenation   | 93.98  | -      | 104.58      | 204.10 (manual+) | -           |

Integer: slightly slower than std::to_string (8.68 vs 7.56 ns). Float: competitive with fmt and std::format. Concatenation: faster than std::format (94 vs 105 ns).

### StringPool

At N=100K total ops with 90% duplication (the primary use case), MSVC local:

| Library              | Median (ns) |
|----------------------|-------------|
| fat_p::StringPool<ST> | 13.27      |
| std::unordered_set   | 14.34       |
| std::unordered_map   | 14.29       |
| boost::flyweight     | 28.91       |

On MSVC, StringPool is ~8% faster than std::unordered_set for the high-duplication case. On GCC, the advantage disappears (25.74 vs 23.85 ns — StringPool is 7% slower). For cold unique inserts (100% miss), StringPool is consistently slower across all platforms (0.66-0.90x of std::unordered_set) because it copies strings into a stable arena.

The value proposition is not throughput but pointer-based comparison after interning: pointer equality is 4-7x faster than strcmp and 50x faster than string_view equality across all platforms.

---

## Abstractions and Patterns

### PolicyIterator

| Pattern     | fat_p | Raw/Manual | range-v3 | Boost.Iterator |
|-------------|-------|------------|----------|----------------|
| Standard    | 0.25  | 0.25       | -        | -              |
| Stride (1K) | 0.58  | 0.52       | 1.78     | -              |
| Filter      | 3.25  | 3.06       | 3.33     | 3.29           |
| Transform   | 0.38  | 0.38       | 0.38     | 0.38           |

Overhead vs raw manual iteration is negligible (1.03-1.11x) for standard and stride patterns. Filter and transform are at parity with range-v3 and Boost.Iterator. The TensorStridePolicy shows 3.90x overhead — expected for multi-dimensional stride computation.

### StrongId

All operations at parity with raw int (0.09 ns). The checked variant adds 0.46 ns for hash operations. Zero runtime cost for the unchecked path.

### StateMachine

| Library              | Transition (ns) |
|----------------------|-----------------|
| Manual enum-switch   | 2.14            |
| Manual fn-ptr table  | 2.70            |
| std::variant         | 5.94            |
| fat_p AnyToAny       | 5.87            |
| fat_p Strict         | 6.16            |
| [Boost].SML          | 6.20            |
| TinyFSM              | 6.84            |
| Boost.MSM            | 10.81           |

All framework-based state machines cluster around 5.9-6.8 ns — the cost of indirect dispatch compared to manual enum-switch (2.14 ns). The overhead is the price of enforced transition validation, entry/exit hooks, and similar features.

### ServiceLocator

DefaultServiceLocator resolution: 2.67 ns vs 1.19 ns direct pointer. ThreadSafe variant: 11.12 ns (shared_mutex). entt::locator at 1.23 ns is faster (static global, no indirection). Registration (51 ns) is slower than std::unordered_map (35 ns) due to type-erased string keys.

### FloatingPointComparison

Standard policy: 1.72 ns vs 0.36 ns manual absolute epsilon. Hybrid policy: 7.16 ns vs 3.15 ns manual. The overhead comes from NaN/Inf checking, combined relative+absolute tolerance, and policy dispatch.

---

## Other Components

### AlignedVector

At parity with std::vector and boost::aligned_allocator on all measured operations. The value is alignment guarantees for SIMD operations, not throughput.

### FeatureManager

Benchmarked against std::map<string, bool> for feature flag lookup. Single-library measurement focused on scaling behavior across feature graph sizes.

### BitSet

Results are mixed and compiler-dependent. Competitive with llvm::BitVector on some operations, behind on others. See per-component results for details.

### Stacktrace

Single-library measurement. No direct competitor comparison — measures capture overhead at various stack depths.

### ThreadPool

p50=1.0 μs, p99=13.9 μs with default 2000 μs spin-wait. Work distribution efficiency: 99.8%. Tested in isolation (no competitor comparison). Spin-wait tuning data: spin=0 (sleep only) gives p99=55.6 μs, spin=2000 (default) gives p99=13.9 μs.

---

## Cross-Cutting Observations

**boost::unordered is the strongest overall competitor.** boost::unordered_flat_map wins or ties the flat map category on most operations. boost::unordered_node_map is the closest competitor in the node-based category. boost::pool beats fat_p allocators on raw single-op throughput. boost::intrusive matches fat_p intrusive. Boost 1.84+ containers have had years of optimization and deployment.

**SplitMix64 impact is MSVC-specific.** On MSVC, switching to SplitMix64 typically gives 1.3-2.0x hash map speedup. On GCC, the improvement is often negligible (<5%). This reflects the quality difference between MSVC's and libstdc++/libc++'s `std::hash` implementations, not the hash maps themselves.

**Cache effects dominate at large sizes.** Find(hit) degrades 6x from N=10K to N=1M across all hash maps (e.g., FastHashMap 2.46→15.26 ns) as working sets exceed L3 cache. This affects every implementation equally.

**tsl::robin_map shows pathological churn behavior on GCC.** 766 ns at N=10K, escalating to 16800 ns at N=1M — indicative of tombstone accumulation. This is a known characteristic of tombstone-based Robin Hood deletion under sustained churn.

**Linux allocators are faster than Windows CRT.** Pool allocator benchmarks show 3-4x speedup over new/delete on GCC vs 8-9x on MSVC, because glibc malloc is substantially faster than MSVC's CRT allocator. This doesn't affect relative rankings between pool implementations, but it changes the absolute value proposition of pool allocation by platform.

**CPU frequency instability affects the MSVC local results.** The stabilization logs show the test machine bouncing between 2200-3100 MHz with variance regularly exceeding 30%. Relative rankings within each benchmark section are reliable (all competitors see the same frequency). Absolute ns/op values should not be compared across sections or across platforms.
