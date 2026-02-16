# FAT-P Benchmark Analysis — Competitive Position

**Data:** 22 components × 3 compilers (MSVC 19.50/Windows, GCC 14/Ubuntu CI, Clang/Ubuntu CI)
**Date:** 2026-02-15
**Methodology:** Round-robin, randomized order, CPU-stabilized, median-primary, 15 measured runs

---

## Tier 1: Competitive Leaders

These components match or beat best-in-class alternatives across multiple operations and sizes.

### FastHashMap (flat, non-stable)

The best-performing FAT-P component. At N=1M (MSVC), FastHashMap[TS]+SplitMix64:

|             | Insert | Find  | Miss  | Erase  | Churn  |
|-------------|--------|-------|-------|--------|--------|
| **FHM[TS]+SM64** | **9.96** | **15.26** | **4.03** | **12.53** | **15.23** |
| boost::flat | 12.02  | 8.95  | 2.37  | 9.95   | 9.42   |
| absl::flat  | 22.93  | 17.85 | 4.54  | 22.09  | 26.40  |
| llvm::Dense | 18.87  | 10.27 | 14.62 | 8.96   | 20.20  |
| std::umap   | 90.96  | 27.70 | 34.93 | 127.90 | 212.30 |

**Winner:** boost::unordered_flat_map overall. FastHashMap wins insert decisively (9.96 vs 12.02 ns, consistent across all three compilers and all sizes). At N=10K MSVC, insert advantage is even clearer: 4.19 vs 5.30 ns. Erase with tombstone policy is competitive with boost (12.53 vs 9.95 ns at N=1M). FastHashMap loses on find, miss, and churn to boost.

**Cross-platform consistency:** On GCC at N=1M, SplitMix64 provides no benefit (15.70 vs 15.44 ns insert) because libstdc++ std::hash is already decent. On MSVC, SplitMix64 cuts insert time nearly in half (13.07→9.96 ns). The hash function matters more than the table on MSVC.

**Honest placement:** #2 flat map behind boost::unordered_flat_map. Ahead of absl, tsl, ankerl, llvm, and std on most operations. The insert lead is real and significant. The churn deficit to boost is also real and significant (15.23 vs 9.42 at N=1M MSVC, 29.71 vs 13.81 on GCC).

### StableHashMap (node-based, reference-stable)

The correct comparison class is node-based maps. At N=1M (MSVC), StableHashMap[Block]+SM64:

|               | Insert | Find  | Miss | Erase | Churn  |
|---------------|--------|-------|------|-------|--------|
| **SHM[Block]+SM64** | **17.53** | **10.26** | **3.04** | **24.95** | **31.73** |
| boost::node   | 38.24  | 10.70 | 4.52 | 81.16 | 103.11 |
| absl::node    | 42.62  | 17.75 | 8.66 | 104.09| 123.34 |
| folly::F14Node (GCC) | 46.41 | 9.33 | 8.51 | 112.02 | 74.48 |
| std::umap     | 90.96  | 27.70 | 34.93| 127.90| 212.30 |

**Winner:** StableHashMap[Block]+SM64 wins node-based on insert (2.2x over boost::node), erase (3.3x), and churn (3.3x). boost::node wins on find by a small margin and miss by ~50%. This pattern holds on GCC and Clang.

**Critical caveat:** The block allocator variant is essential. Plain StableHashMap without it degrades to 40.43 ns insert / 105.76 ns erase at N=1M — barely faster than std::unordered_map on erase. The block allocator is what makes StableHashMap competitive.

**Miss diagnostics are excellent:** At N=1M, StableHashMap+SM64 averages 1.00 group probe per miss, 0.01 equality comparisons per miss. This is near-optimal — almost every miss is resolved by a single SIMD metadata check.

### CircularBuffer (SPSC)

Uncontended SPSC: 0.95 ns/op, fastest in the field.

| Library              | Uncontended | SPSC Threaded | Batch |
|----------------------|-------------|---------------|-------|
| **fat_p::CircularBuffer** | **0.95** | 42.61     | **0.86** |
| fat_p::LockFreeRingBuffer | 1.17   | **9.14**  | 0.93  |
| boost::lockfree::spsc | 1.25      | 21.04     | 0.89  |
| moodycamel::BRWCB    | 20.63      | 66.72     | 15.93 |
| std::mutex+deque     | 22.44      | 88.66     | 3.70  |

CircularBuffer wins uncontended and batch throughput. LockFreeRingBuffer wins the threaded SPSC case (9.14 vs 42.61 ns) because CircularBuffer uses a mutex that becomes a bottleneck with actual contention. The LockFreeRingBuffer beats boost::lockfree::spsc_queue (9.14 vs 21.04 ns) in the threaded test. Both fat_p SPSC containers beat moodycamel in every test.

### ObjectPool

MSVC at N=1K (single alloc/dealloc cycle):

| Library              | Median (ns) | vs new/delete |
|----------------------|-------------|---------------|
| EASTL::fixed_pool    | 2.10        | 9.8x          |
| **fat_p::ObjectPool** | **2.30**   | **9.0x**      |
| boost::object_pool   | 2.30        | 9.0x          |
| foonathan::memory    | 3.40        | 6.1x          |
| std::pmr::unsync     | 6.30        | 3.3x          |
| new/delete           | 20.60       | 1.0x          |

Batch acquire (N=10K objects, then release):

| Library              | Median (ns) |
|----------------------|-------------|
| **fat_p::ObjectPool** | **2.17**   |
| EASTL::fixed_pool    | 1.69        |
| foonathan::memory    | 1.90        |
| boost::object_pool   | 9.82        |
| std::pmr::unsync     | 10.22       |
| new/delete           | 21.63       |

**Placement:** Consistently top-3. EASTL is slightly faster but doesn't auto-grow (marked [!grow] in benchmarks). fat_p matches boost on single ops and crushes it on batch (2.17 vs 9.82 ns). On GCC, gap narrows: fat_p 5.97 ns vs boost 6.68 ns single op — still competitive but the absolute speedup over new/delete drops from 9x to 3.4x due to Linux's faster allocator.

### SparseSet

At N=1K (MSVC):

| Library              | Insert | Contains | Erase | Iterate |
|----------------------|--------|----------|-------|---------|
| llvm::SparseSet<32>  | 0.80   | 2.10     | 1.00  | 0.20    |
| **fat_p::SparseSet<32>** | **1.00** | **2.30** | **1.00** | **0.20** |
| fat_p::SparseSet<8>  | 1.90   | 0.40     | 2.80  | 0.40    |
| entt::sparse_set     | 3.10   | 2.70     | 2.80  | 0.20    |
| absl::flat_hash_set  | 18.10  | 6.50     | 5.20  | 2.70    |
| std::unordered_set   | 28.30  | 6.20     | 14.80 | 0.90    |
| std::set             | 69.80  | 33.10    | 80.60 | 4.20    |

**Placement:** Matches llvm::SparseSet within measurement noise. Both are 15-30x faster than hash-based sets for the operations sparse sets are designed for. fat_p::SparseSet<8> has faster contains (0.40 ns) due to smaller page size but slower insert. entt::sparse_set is ~2-3x slower on insert/erase.

### PolicyIterator

| Pattern     | fat_p | Raw/Manual | range-v3 | Boost.Iterator |
|-------------|-------|------------|----------|----------------|
| Standard    | 0.25  | 0.25       | -        | -              |
| Stride (1K) | 0.58  | 0.52       | 1.78     | -              |
| Filter      | 3.25  | 3.06       | 3.33     | 3.29           |
| Transform   | 0.38  | 0.38       | 0.38     | 0.38           |
| Stride1D    | 1.00  | 1.00       | -        | -              |

**Verdict:** Zero overhead on standard iteration (1.03x). Negligible overhead on stride (1.11x). Filter and transform are at parity with all competitors. The TensorStridePolicy is 3.90x — expected for multi-dimensional stride computation and documented as such. Eigen beats it at 0.70x on column iteration.

### StrongId

Every operation at parity with raw int (0.09 ns). The checked variant adds 0.46 ns for hash operations (1.49x). This is the textbook zero-cost abstraction — type safety with no runtime penalty except for the explicit checked path.

---

## Tier 2: Competitive, Not Leading

These hold their own but don't distinguish themselves from the field.

### SmallVector

At N=1K push_back (MSVC):

| Library              | Median (ns) |
|----------------------|-------------|
| **fat_p::SmallVector** | **0.60** |
| boost::small_vector  | 0.60        |
| llvm::SmallVector    | 0.60        |
| absl::InlinedVector  | 0.60        |
| ankerl::svector      | 0.60        |
| eastl::fixed_vector  | 0.70        |
| std::vector          | 1.00        |

All small vector implementations are at parity for operations within inline capacity. The value proposition is allocation avoidance (confirmed: 0 heap allocations for inline sizes), not per-op throughput. On GCC, fat_p is slightly behind std::vector (1.30 vs 0.90 ns) for push_back N=100, and behind eastl (1.00 ns). No small vector implementation has a meaningful performance lead over the others. The differentiator is API design and feature set, not speed.

### SlotMap

At N=10K (MSVC):

| Library              | Insert | Lookup | Erase |
|----------------------|--------|--------|-------|
| **fat_p::SlotMap**   | **14.53** | **2.68** | **7.20** |
| sg14::slot_map       | 17.83  | 2.81   | 6.04  |
| plf::hive            | 10.80  | 2.04   | 9.56  |
| entt::registry       | 32.21  | 6.06   | 39.04 |
| std::unordered_map   | 43.96  | 13.00  | 28.64 |

**Placement:** Middle of the pack among slot map implementations. plf::hive is faster on insert and lookup. sg14::slot_map trades blows. The differentiator is ABA safety (generation counting) which plf::hive lacks. Beats entt and std containers convincingly.

### IntrusiveList

| Library              | Push   | Erase  | Iterate |
|----------------------|--------|--------|---------|
| etl::intrusive_list  | 1.45   | 15129  | 76600   |
| eastl::intrusive     | 1.91   | 4.56   | 78400   |
| boost::intrusive     | 1.95   | 5.20   | 71000   |
| llvm::simple_ilist   | 1.99   | 5.15   | 78300   |
| **fat_p::Intrusive (fast)** | **2.01** | **5.33** | **78500** |
| fat_p::Intrusive (safe) | 2.44 | 6.42  | 82100   |
| std::list<T*>        | 19.94  | 28.21  | 39500   |

**Placement:** Matches boost and llvm within ~5%. etl and eastl are marginally faster on push (likely simpler hook logic). The safe policy costs ~20% on push and erase — the cost of pointer validation. Iteration is pointer-chasing bound, so all intrusive lists perform identically. The real win is 8-10x over std::list due to zero allocation.

Note: std::list actually wins iteration (39500 vs 78500 ns) because std::list with std::deque backing has better cache locality than pointer-linked nodes scattered in memory.

### StateMachine

| Library              | Transition (ns) |
|----------------------|-----------------|
| Manual enum-switch   | 2.14            |
| Manual fn-ptr table  | 2.70            |
| std::variant         | 5.94            |
| **fat_p AnyToAny**   | **5.87**        |
| **fat_p Strict**     | **6.16**        |
| [Boost].SML          | 6.20            |
| TinyFSM              | 6.84            |
| Boost.MSM            | 10.81           |

**Placement:** Equivalent to std::variant dispatch and [Boost].SML. 2.7x overhead vs manual enum-switch — the cost of virtual dispatch / indirect call. Beats Boost.MSM by ~1.8x. This is a correctness-focused component (enforced transitions, hooks) rather than a performance-focused one.

### Stringify

| Operation       | fat_p  | fmt    | std::format | std::to_string | ostringstream |
|-----------------|--------|--------|-------------|----------------|---------------|
| Integer         | 8.68   | 18.83  | 39.45       | 7.56           | 266.18        |
| Float           | 82.44  | 72.79  | 83.47       | 195.31         | 388.00        |
| Concatenation   | 93.98  | -      | 104.58      | 204.10 (manual+) | -           |

**Placement:** Integer: slightly slower than std::to_string (8.68 vs 7.56 ns), much faster than fmt and std::format. Float: competitive with fmt and std::format, 2.4x faster than std::to_string. String concatenation: fastest option (94 ns vs 105 std::format vs 204 manual). Overall a solid general-purpose formatter.

---

## Tier 3: Measurable Overhead for Safety/Features

### FloatingPointComparison

Standard policy: 1.72 ns vs 0.36 ns manual absolute epsilon (4.76x). Hybrid policy: 7.16 ns vs 3.15 ns manual (2.27x). This is the cost of NaN/Inf checking, relative+absolute tolerance, and policy dispatch. The NaN fast-path (1.55 ns) and Inf fast-path (2.05 ns) are well-optimized. The overhead is the price of correctness for edge cases — you're paying 1.3 ns for not silently accepting NaN comparisons.

### ServiceLocator

DefaultServiceLocator: 2.67 ns resolution vs 1.19 ns direct pointer (2.2x). ThreadSafe variant: 11.12 ns (shared_mutex cost). Registration (51 ns) is slower than std::unordered_map (35 ns) — the type-erased string key costs more than type_index. entt::locator at 1.23 ns is faster (static global, no indirection). The overhead is acceptable for dependency injection at architectural boundaries, not for hot loops.

### AllocationStrategies

| Allocator             | Single (ns) | Bulk 100 (ns) | Churn (ns) |
|-----------------------|-------------|---------------|------------|
| boost::pool (raw)     | 1.26        | 186.50        | 1.23       |
| fat_p::BlockAllocator | 2.23        | 199.20        | 1.76       |
| fat_p::PoolAllocator  | 2.60        | 203.70        | 2.50       |
| std::pmr::monotonic   | 2.65        | 233.11        | -          |
| std::pmr::unsync_pool | 4.18        | 697.38        | -          |
| new/delete            | 18.86       | 2395.19       | 18.29      |

**Placement:** 7-8x faster than new/delete. boost::pool is ~40% faster on single operations. fat_p allocators provide a middle ground between boost's raw speed and pmr's standard interface.

---

## Tier 4: Scaling Advantages

### WorkQueue (sharded MPMC)

This is where WorkQueue is designed to shine:

| Threads | WorkQueue | LockFreeQueue | moodycamel | mutex+queue |
|---------|-----------|---------------|------------|-------------|
| 1T      | 16.1      | 7.1           | 14.6       | 19.2        |
| 2T      | 13.3      | 24.9          | 20.2       | 19.9        |
| 4T      | 27.8      | 73.7          | 32.9       | 50.0        |
| 8T      | 26.1      | 83.8          | 31.3       | 181.1       |
| 12T     | 24.9      | 87.9          | 27.3       | 250.4       |
| 16T     | 29.4      | 87.9          | 25.8       | 222.0       |

**WorkQueue** scales better than LockFreeQueue (sharding eliminates CAS contention) and much better than mutex+queue. moodycamel::ConcurrentQueue matches or slightly beats WorkQueue at high thread counts (25.8 vs 29.4 at 16T). At single-threaded, WorkQueue pays the shard-routing overhead (16.1 vs 7.1 ns for LockFreeQueue).

Asymmetric MPMC (real-world patterns): WorkQueue delivers consistent 23-28 ns/op regardless of producer/consumer ratio. LockFreeQueue and mutex+queue degrade badly under asymmetric loads (78-184 ns). moodycamel is the only competitor that also stays stable (14-64 ns).

### ThreadPool

p50=1.0 μs, p99=13.9 μs with default 2000 μs spin. Work distribution efficiency: 99.8%. No competitor comparison — this is tested in isolation. The spin-wait tuning data is useful: spin=0 (sleep only) gives p99=55.6 μs, spin=2000 (default) gives p99=13.9 μs, showing the spin period buys 4x improvement on tail latency.

---

## Component Ranking by Competitive Position

| Rank | Component | Position | Primary Competitor |
|------|-----------|----------|--------------------|
| 1 | FastHashMap | #2 flat map, #1 on insert | boost::unordered_flat_map |
| 2 | StableHashMap[Block] | #1 node-based map | boost::unordered_node_map |
| 3 | CircularBuffer | #1 uncontended SPSC | boost::lockfree::spsc_queue |
| 4 | LockFreeRingBuffer | #1 threaded SPSC | boost::lockfree::spsc_queue |
| 5 | SparseSet | Tied #1 with LLVM | llvm::SparseSet |
| 6 | ObjectPool | Top-3 | EASTL::fixed_pool, boost::object_pool |
| 7 | PolicyIterator | Zero overhead (1.03x) | Raw pointers |
| 8 | StrongId | Zero overhead (1.00x) | Raw int |
| 9 | WorkQueue | #2 MPMC at scale | moodycamel::ConcurrentQueue |
| 10 | SmallVector | Parity | All small vector impls |
| 11 | SlotMap | Mid-pack | plf::hive, sg14::slot_map |
| 12 | Stringify | Competitive | fmt::format |
| 13 | IntrusiveList | Parity with boost/llvm | boost::intrusive::list |
| 14 | StateMachine | Parity with SML/variant | [Boost].SML |
| 15 | BitSet | Mixed (compiler-dependent) | llvm::BitVector |
| 16 | AllocationStrategies | Behind boost | boost::pool |
| 17 | ServiceLocator | Behind entt | entt::locator |
| 18 | FloatingPointComparison | Expected overhead for safety | Manual epsilon |

---

## Systemic Observations

**boost::unordered is the overall toughest competitor.** boost::unordered_flat_map wins or ties the flat map category on nearly every operation at every size. boost::unordered_node_map is the closest competitor in the node-based category. boost::pool beats fat_p allocators. boost::intrusive matches fat_p intrusive. Boost 1.84+ containers are genuinely excellent.

**SplitMix64 is a bigger factor on MSVC than GCC/Clang.** On MSVC, switching to SplitMix64 typically gives 1.3-2.0x speedup. On GCC, the improvement is often negligible (<5%). This means MSVC's std::hash<uint64_t> is significantly worse than libstdc++/libc++ implementations.

**Find(hit) degrades at N=1M across all hash maps** because working sets exceed L3 cache. At N=10K, FastHashMap[TS]+SM64 finds in 2.46 ns. At N=1M, it's 15.26 ns — a 6.2x slowdown entirely from cache misses. This affects every competitor equally and isn't a design flaw.

**tsl::robin_map has a catastrophic churn bug on GCC.** 766 ns at N=10K, 14898 ns at N=100K, 16800 ns at N=1M — clearly pathological tombstone accumulation. The benchmark correctly reports this. Robin hood hashing is known to degrade under sustained churn.

**CPU frequency instability on MSVC is concerning.** The stabilization logs show the MSVC machine bouncing between 2200-3100 MHz with variance regularly exceeding 30%. The harness waits for <10% variance, but often settles at 62-70% of base clock. This means the absolute ns/op values on MSVC should be treated as approximate — the relative rankings within each benchmark section are reliable since all competitors see the same frequency.

**GCC CI runners are more stable** (Azure NC US, ~3.2-3.5 GHz, typically ready within a few attempts) but are shared-tenancy, introducing potential neighbor noise. The Clang results are very close to GCC (same CI infrastructure), confirming the measurements are reproducible.
