# FatP Benchmark Gap Analysis Report

*Which components need benchmarks and why*

**February 2026**

---

## Executive Summary

The FatP library contains 62 components. Of these, 22 already have benchmark files with dedicated CI workflows covering multi-compiler (GCC 12/13/14, Clang 16/17, MSVC), multi-platform testing against industry competitors. This report analyzes the remaining 40 components to determine which should have performance benchmarks added.

The analysis identifies 10 components that should have benchmarks (5 high priority, 5 medium), 7 borderline cases, and 23 that should be skipped. The high-priority components are the most performance-sensitive data structures in the library: CSRMatrix, Tensor, SimdVector, Signal, and StringPool.

---

## Current Coverage

The benchmark infrastructure consists of a shared dependency cache builder (build-benchmark-deps.yml) that pre-compiles all competitor libraries, plus 18 dedicated per-component benchmark workflows and a run-all-benchmarks.yml orchestrator. Each workflow tests against industry competitors detected via `__has_include` for graceful degradation.

### Components With Benchmarks (22)

| Component | Key Competitors | Workflow |
|-----------|----------------|----------|
| FatPHashMap | tsl::robin_map, ankerl::unordered_dense, abseil, folly, llvm::DenseMap | fatp-hash-map-benchmarks.yml |
| SmallVector | boost, folly, llvm, absl, ankerl, eastl | smallvector-benchmarks.yml |
| FlatMapSet | boost::flat_map, absl::btree, folly::sorted_vector | flatmapset-benchmarks.yml |
| SparseSet | llvm::SparseSet, entt::sparse_set, absl::flat_hash_set | sparseset-benchmarks.yml |
| IntrusiveList | boost::intrusive, llvm::simple_ilist, eastl, etl | intrusivelist-benchmarks.yml |
| StateMachine | boost::sml, boost::msm, tinyfsm | statemachine-benchmarks.yml |
| ObjectPool | boost::object_pool, eastl::fixed_pool | objectpool-benchmarks.yml |
| BitSet | boost::dynamic_bitset, llvm::BitVector | bitset-benchmarks.yml |
| SlotMap | sg14::slot_map, entt::sparse_set, plf::hive | slotmap-benchmarks.yml |
| ThreadPool | boost::asio::thread_pool, TBB task_group | threadpool-benchmarks.yml |
| WorkQueue | moodycamel::ConcurrentQueue, boost::lockfree::queue | workqueue-benchmarks.yml |
| LockFreeQueue | moodycamel::ConcurrentQueue, boost::lockfree::queue | lockfreequeue-benchmarks.yml |
| CircularBuffer | boost::lockfree::spsc_queue, moodycamel SPSC | circularbuffer-benchmarks.yml |
| PolicyIterator | boost::iterator, std::ranges | policyiterator-benchmarks.yml |
| ServiceLocator | entt::locator | servicelocator-benchmarks.yml |
| StrongId | NamedType, rollbear::strong_type, boost | strongid-benchmarks.yml |
| AlignedVector | boost::alignment::aligned_allocator | alignedvector-benchmarks.yml |
| AllocationStrategies | boost::pool | allocationstrategies-benchmarks.yml |
| FeatureManager | No external competitors | run-all-benchmarks.yml |
| FloatingPointComparison | No external competitors | run-all-benchmarks.yml |
| Stacktrace | No external competitors | run-all-benchmarks.yml |
| Stringify | No external competitors | run-all-benchmarks.yml |

---

## Components That Should Have Benchmarks

These components have measurable runtime performance characteristics, natural industry competitors, and performance-sensitive use cases where users need data to make adoption decisions.

### High Priority (5)

These are the most performance-sensitive data structures in FatP. Users choosing between FatP and established alternatives (Eigen, Boost, etc.) will look for these numbers first.

| Component | Size | Competitors | Rationale |
|-----------|------|-------------|-----------|
| CSRMatrix | 1,389 + 3 sub-headers | Eigen::SparseMatrix, Armadillo, MKL sparse BLAS | Core HPC data structure. SpMV perf is critical for users choosing between FatP and Eigen. |
| Tensor | 2,943 + 5 sub-headers | Eigen::Tensor, xtensor, libtorch ATen | Largest component. Multi-dim array perf directly impacts scientific computing workloads. |
| SimdVector | 2,067 | Eigen vectorized ops, xsimd, std::experimental::simd (GCC) | SIMD is the core value proposition. Must prove FatP's auto-vectorization matches hand-tuned libs. |
| Signal | 1,002 | boost::signals2, nano-signal-slot, palacaze/sigslot | Signal/slot perf varies wildly across implementations. Emission and connection overhead matters. |
| StringPool | 560 | std::unordered_set\<string\>, boost::flyweight, folly::fbstring intern | String interning is a common optimization. Lookup and insertion throughput are key metrics. |

### Medium Priority (5)

Worth benchmarking to validate design decisions and demonstrate zero-cost abstraction properties, but less likely to be the deciding factor for library adoption.

| Component | Size | Competitors | Rationale |
|-----------|------|-------------|-----------|
| CheckedArithmetic | 561 + 7 SIMD headers | SafeInt, Boost.SafeNumerics, compiler builtins (`__builtin_add_overflow`) | Overhead of checked ops vs unchecked is the key question. SIMD path makes it unique. |
| IdGenerator | 1,279 | std::atomic counter, UUID v4/v7, snowflake IDs | ID generation throughput matters in ECS and database workloads. Contention under threads is key. |
| RateLimiter | 441 | folly::TokenBucket, manual token bucket, sliding window | Compact component but perf-sensitive in networking. Burst handling and steady-state throughput. |
| Factory | 911 | std::function\<\> map, manual vtable, boost::factory | Object creation dispatch overhead. Useful to show policy-based design has zero/low cost. |
| HpcVector | 1,053 | std::vector, Eigen::VectorXd, Blaze DynamicVector | Aligned, SIMD-friendly vector. Must justify existence vs std::vector + aligned allocator. |

---

## Borderline Cases

These components could benefit from benchmarks but the value is less clear. Either the performance characteristics are dominated by external factors (I/O, OS), the component should be zero-cost by design, or the competitive landscape is thin.

| Component | Size | Competitors | Rationale |
|-----------|------|-------------|-----------|
| Expected | 4,093 | std::expected (C++23), tl::expected, Boost.Outcome | Error handling overhead is typically negligible. Useful for validating zero-cost abstraction claim. |
| ConcurrencyPolicies | 2,427 | Manual std::mutex, std::shared_mutex, folly::SharedMutex | Policy templates. Benchmark would prove policy selection doesn't add overhead vs hand-written locks. |
| CacheUtilities | 897 | Manual prefetch intrinsics, no-op baseline | Cache-aware access patterns. Benchmarking is essentially the component's purpose. |
| MemoryMappedFile | 548 | boost::mapped_file, raw mmap/MapViewOfFile | I/O component. Benchmark would be mostly OS overhead, not library overhead. |
| SlidingFileWindow | 999 | Sequential mmap, fread with buffer | Similar to MemoryMappedFile — dominated by I/O, not library logic. |
| ScopeGuard | 730 | folly::ScopeGuard, manual try/catch/RAII | Should be zero-cost. Quick micro-benchmark could confirm. |
| EnumPlus | 822 | magic_enum, manual switch/map | Enum-to-string conversion throughput. magic_enum is the obvious competitor. |

---

## Components That Should Not Have Benchmarks

These components fall into categories where benchmarking would be meaningless, misleading, or impossible: compile-time-only constructs, debug-only infrastructure, tooling/meta components, or hardware-dependent components that cannot run on CI.

| Component(s) | Reason |
|--------------|--------|
| _shared, FatPBenchmarkRunner, FatPConfig, FatPTest | Infrastructure and tooling — not user-facing library components. |
| Concepts, CppFeatureDetection, PlatformDetection, SimdDetection | Compile-time detection only. No runtime code path to benchmark. |
| ConstexprUtilities (46L) | Entirely constexpr. Evaluated at compile time, zero runtime cost by definition. |
| ContractException, DebugOnly, EnforcedInit, EnhancedBoundsChecking | Debug/safety infrastructure. Only active in debug builds, optimized out in release. |
| Reflection (931L), ValueGuard (545L), ViewLifetimeTracking (417L) | Thin wrappers and debug aids. Negligible runtime overhead. |
| CoroutineTask (580L) | Coroutine primitive. Hard to benchmark meaningfully in isolation — perf depends on executor. |
| BinarySerialization, Cbor, Json, DiagnosticLogger, Enforce | Map to headers under different names (FatPBinary, CborLite, JsonLite). Some may already have benchmarks via their serialization workflow names. |
| NumaAllocator (1,170L) | Requires NUMA hardware. GitHub Actions runners are single-socket — results would be meaningless. |

---

## Coverage Summary

| Category | Count | Status |
|----------|-------|--------|
| Components with benchmarks | **22** | Complete |
| Should add (high priority) | **5** | CSRMatrix, Tensor, SimdVector, Signal, StringPool |
| Should add (medium priority) | **5** | CheckedArithmetic, IdGenerator, RateLimiter, Factory, HpcVector |
| Borderline / optional | **7** | Expected, ConcurrencyPolicies, CacheUtilities, and 4 others |
| Should not have benchmarks | **23** | Compile-time, debug, infra, or hardware-dependent |
| **Total components** | **62** | 36% covered, 52% target with high+medium |

---

## Recommendations

**1. Prioritize the big five.** CSRMatrix, Tensor, and SimdVector are the HPC core of FatP. Signal and StringPool are the most commonly benchmarked utility patterns. These five should be implemented first using the established workflow template with shared cache architecture.

**2. External dependencies are minimal.** Most high-priority competitors (Eigen, xsimd) are header-only and can be added to the existing header-only cache in build-benchmark-deps.yml with a single git clone. No new compiled cache entries are needed.

**3. Medium priority can wait.** CheckedArithmetic, IdGenerator, RateLimiter, Factory, and HpcVector are valuable but not urgent. They validate design decisions rather than drive adoption. Implement after the high-priority five are stable.

**4. Skip the rest.** The 23 components flagged as skip are genuinely not benchmarkable in any meaningful way. Adding benchmarks would produce noise, not signal. NumaAllocator is the only arguable exception, but it requires NUMA hardware that CI runners do not have.

**5. Serialization components need investigation.** BinarySerialization, Cbor, Json, and DiagnosticLogger map to headers under different names. It is worth verifying whether any existing benchmarks already cover them under their alternate names before creating new ones.
