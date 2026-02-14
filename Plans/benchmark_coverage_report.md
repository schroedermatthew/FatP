# Fat-P Benchmark Coverage Report

**Date:** 2026-02-14
**Total project headers:** 107
**Components audited:** 41 (16 definitely need + 7 could benefit + 18 not applicable)

---

## Current Coverage

**20 components have benchmarks today.** These run across CI in three places:

| Venue | Components | Runner |
|---|---|---|
| `run-all-benchmarks.yml` standard matrix | AlignedVector, AllocationStrategies, BitSet, CircularBuffer, FeatureManager, FlatMapSet, FloatingPointComparison, IntrusiveList, ObjectPool, PolicyIterator, ServiceLocator, SlotMap, SmallVector, SparseSet, Stacktrace, Stringify, StrongId | GCC-13, single job per component |
| `run-all-benchmarks.yml` dedicated jobs | FatPHashMap (GCC-14 + header-only competitors), ThreadPool (TBB, Boost.Asio), WorkQueue (moodycamel, boost::lockfree) | Custom deps per job |
| Component workflows (manual dispatch) | ThreadPool (6 compiler configs × 20 batches), WorkQueue (Linux + MSVC + TSan) | Full matrix with artifacts |

---

## CI Competitor Gap — Existing Benchmarks Running Without Comparisons

Of the 20 benchmarked components, only 3 run with competitor comparisons in CI:
FatPHashMap, ThreadPool, and WorkQueue. These have dedicated jobs in
`run-all-benchmarks.yml` that install external libraries (TBB, Boost.Asio,
moodycamel, boost::lockfree, tsl::robin_map, ankerl::unordered_dense) before
building.

The other 17 run in the `standard` matrix with a bare build command:

```
g++-13 -std=c++20 -O3 -DNDEBUG -march=native -I./include/fat_p -pthread $src -o bench
```

The benchmark source code for these components already contains competitor
adapters — the comparisons are written, guarded behind `__has_include` for
external libraries and compiled unconditionally for `std::` baselines. But
the CI workflow does not install the external competitor headers, so the
`__has_include` guards silently compile out the competitor paths. The result
is that every standard-matrix benchmark runs Fat-P in isolation, producing
absolute numbers (ns/op, tasks/s) with no relative context.

This is a CI configuration problem, not a benchmark code problem. The fix
has two parts:

**Part 1 — `std::` baselines (should already work).** Competitors that use
only standard library types (`std::vector`, `std::deque`, `std::map`,
`std::unordered_map`, `std::list`, `std::bitset`, `std::set`,
`std::unordered_set`, `std::to_string`, raw `new`/`delete`) need no extra
`-I` paths. If the benchmark code includes them unconditionally (not behind
`__has_include`), they should already be active. Verify that each of the 17
standard benchmarks is actually compiling and running its `std::` baselines.
If any are behind `__has_include` guards for `std::` types, remove the guards
— standard headers are always available.

**Part 2 — External competitors need installation steps.** For benchmarks
that compare against external header-only libraries (e.g., SmallVector vs
`boost::container::small_vector`, IntrusiveList vs `boost::intrusive::list`,
Signal vs `Boost.Signals2`), the `standard` matrix either needs a shared
competitor-install step, or these benchmarks should be promoted to dedicated
jobs with their own dependency installation — the same pattern used by
FatPHashMap, ThreadPool, and WorkQueue.

The 17 standard-matrix benchmarks break down as follows:

| Component | `std::` baseline available? | External competitor in code? | Action needed |
|---|---|---|---|
| AlignedVector | `std::vector` | — | Verify std:: baseline compiles |
| AllocationStrategies | `malloc`, `new`/`delete` | — | Verify baseline compiles |
| BitSet | `std::bitset`, `std::vector<bool>` | — | Verify std:: baseline compiles |
| CircularBuffer | `std::deque` | `boost::circular_buffer` | Verify std::; add boost install for external |
| FlatMapSet | `std::map`, `std::unordered_map`, `std::set` | — | Verify std:: baseline compiles |
| IntrusiveList | `std::list` | `boost::intrusive::list` | Verify std::; add boost install for external |
| ObjectPool | `new`/`delete` cycle | — | Verify baseline compiles |
| SlotMap | `std::unordered_map` | — | Verify std:: baseline compiles |
| SmallVector | `std::vector` | `boost::container::small_vector` | Verify std::; add boost install for external |
| SparseSet | `std::unordered_set`, `std::set` | — | Verify std:: baseline compiles |
| Stringify | `std::to_string`, `snprintf` | `std::format` (C++23 only) | Verify std:: baseline compiles |
| StrongId | raw `uint64_t` | — | Verify baseline compiles |
| FeatureManager | (self-comparison) | — | OK as-is |
| FloatingPointComparison | (strategy comparison) | — | OK as-is |
| PolicyIterator | (policy comparison) | — | OK as-is |
| ServiceLocator | (self-measurement) | — | OK as-is |
| Stacktrace | (overhead measurement) | `std::stacktrace` (C++23) | OK as-is; std::stacktrace when compilers catch up |

**12 need verification** that their `std::` baselines are actually compiling
and running. **3 of those 12** also have external competitors (Boost) that
would require a dependency install step in CI to activate. **5 are fine
as-is** because they measure internal behavior with no natural competitor.

---

## Missing Benchmarks — Definitely Need (15 remaining)

These are runtime data structures, concurrency primitives, or compute-heavy components
where performance is a core value proposition. Ordered by implementation priority
(considering complexity, dependency requirements, and value of measured proof).

### Tier 1 — Self-contained, high value, straightforward to write (8)

These need no external dependencies and can go straight into the `standard` matrix in
`run-all-benchmarks.yml`. Each would be a single `.cpp` file using `FatPBenchmarkRunner`.

| Component | Lines | What to measure | Competitors |
|---|---|---|---|
| **Expected** | 4,093 | Monadic chain overhead (and_then/or_else/transform), value vs error construction, comparison vs raw return codes and vs `std::expected` (C++23) | `std::expected` (C++23 only), raw error codes, `std::variant<T,E>` |
| **CheckedArithmetic** | 6,302 | Scalar checked add/mul/div vs unchecked, SIMD batch (SSE2/AVX2/NEON) vs scalar, overflow detection branch cost | Unchecked baseline, compiler built-in `__builtin_add_overflow` |
| **Signal** | 1,002 | Slot dispatch latency (1/10/100/1000 connections), connect/disconnect churn, emission vs direct function call | `std::function` chain, Boost.Signals2 (if available via `__has_include`) |
| **StringPool** | 560 | Intern throughput (unique/duplicate ratio), lookup hit/miss, memory efficiency vs `std::unordered_set<std::string>` | `std::unordered_set<std::string>`, `std::set<std::string>` |
| **CoroutineTask** | 580 | co_await resume latency, coroutine creation/destruction overhead, chain depth scaling | `std::async`, raw thread launch, callback chain |
| **CacheUtilities** | 897 | Prefetch effect on sequential/strided/random access, cache-line flush cost, measured L1/L2/L3 miss reduction | Unprefetched baseline (same access pattern) |
| **BinaryLite** | 743 | Encode/decode throughput for small/medium/large payloads, round-trip correctness | Raw `memcpy` baseline, `std::bit_cast` where applicable |
| **DiagnosticLogger** | 2,074 | Hot-path log overhead (enabled vs disabled), sink throughput (null/file/memory), format string cost | `std::cout`, `fprintf`, `spdlog` (if available via `__has_include`) |

### Tier 2 — Need SIMD/ISA handling (4)

These require ISA-aware compilation flags (`-mavx2`, `-msse2`, etc.) and should have
fallback scalar paths. Each needs careful feature-gating per the Benchmark Style Guide's
ISA section. Best placed as dedicated jobs in `run-all-benchmarks.yml` with
architecture-specific compile flags.

| Component | Lines | What to measure | Competitors |
|---|---|---|---|
| **CSRMatrix** | 3,747 | SpMV throughput (varying sparsity), matrix construction, parallel SpMV scaling | Eigen (if header-only subset), raw loop baseline |
| **HpcVector** | 1,053 | Elementwise ops throughput, dot product, BLAS-like ops vs `std::vector` loops | `std::vector` + raw loops, `std::valarray` |
| **SimdVector** | 2,067 | SIMD-accelerated sum/dot/transform throughput, auto-vectorization comparison | Scalar baseline, compiler auto-vectorized `std::vector` loop |
| **Tensor** | 6,493 | Element access (row-major/col-major), tensor contraction, einsum throughput, memory layout impact | Raw multidimensional array, NumPy-style stride computation baseline |

### Tier 3 — Need external competitors or special hardware (3)

These require either third-party libraries for meaningful comparison or specific
hardware that CI runners may not provide.

| Component | Lines | What to measure | Competitors | CI notes |
|---|---|---|---|---|
| **JsonLite** | 7,234 | Parse/serialize throughput (small/medium/large documents), DOM construction cost | nlohmann/json, rapidjson, simdjson | Header-only competitors via git clone; dedicated job |
| **CborLite** | 493 | Encode/decode throughput, round-trip cost | tinycbor (if header-only), manual baseline | May need dedicated job |
| **NumaAllocator** | 1,170 | NUMA-local vs cross-node bandwidth, allocation throughput, page-fault cost | `std::allocator`, `malloc`, `aligned_alloc` | CI runners are typically single-NUMA-node; benchmark will measure allocation overhead only, not cross-node bandwidth. Document this limitation. |

---

## Could Benefit — Lower Priority (7)

These have some runtime behavior worth measuring but are less performance-critical.
None are blocking.

| Component | What to measure | Notes |
|---|---|---|
| RateLimiter | Token bucket acquire throughput under 1/4/8 thread contention | Interesting but niche |
| MemoryMappedFile | Mapped read throughput vs `fread`/`std::ifstream` | Heavily OS-dependent |
| SlidingFileWindow | Windowed sequential read throughput | Similar to MemoryMappedFile |
| IdGenerator | ID generation throughput under contention | Quick to write |
| Factory | Registration + lookup cost | Typically cold-path |
| Enforce | Macro expansion overhead (debug vs release) | Micro; useful for documentation |
| ScopeGuard | Destructor overhead vs raw RAII | Minimal but measurable |

---

## Not Applicable (18)

Compile-time constructs, type traits, policy tags, detection macros, test
infrastructure, or configuration headers with no meaningful runtime behavior:
Concepts, ConcurrencyPolicies, ConstexprUtilities, ContractException,
CppFeatureDetection, DebugOnly, EnforcedInit, EnhancedBoundsChecking, EnumPlus,
Equality*, FatPBenchmarkRunner, FatPConfig, FatPTest, PlatformDetection,
Reflection, SimdDetection, ValueGuard, ViewLifetimeTracking.

*Equality already has benchmark_EqualityComparisonsAny.cpp but it's not in the
run-all workflow. Should be added to the standard matrix.

---

## Summary

| Category | Count | With benchmarks | Missing |
|---|---|---|---|
| Definitely need | 16 | 1 (WorkQueue) | **15** |
| Could benefit | 7 | 0 | 7 |
| Not applicable | 18 | 0 | — |
| Already benchmarked (not in audit) | — | 19 | — |
| **Total benchmarked / total components** | | **20 / 41** | |

| CI competitor status | Count |
|---|---|
| Benchmarks running with competitors | 3 (FatPHashMap, ThreadPool, WorkQueue) |
| Benchmarks with competitors in code but not in CI | **12** (std:: baselines to verify) |
| Benchmarks with competitors in code needing dep install | **3** (CircularBuffer, IntrusiveList, SmallVector) |
| Benchmarks OK without competitors | 5 |

Two independent problems to address: 15 components still need benchmarks
written, and 12 existing benchmarks are likely running without the competitor
comparisons their source code already contains.

### Suggested implementation order

**Immediate (zero code changes):** Verify that the 12 standard-matrix
benchmarks with `std::` competitors are actually compiling and running
them. This may already work — the `std::` headers need no `-I` flags. If
any `std::` baselines are behind unnecessary `__has_include` guards, remove
the guards. This is the highest-ROI fix: 12 benchmarks gain relative
context with no new code.

**Short-term (CI config only):** For the 3 benchmarks with Boost competitors
in code (CircularBuffer, IntrusiveList, SmallVector), add `libboost-dev` to
the standard matrix's install step or promote them to dedicated jobs.

**Medium-term:** Write the Tier 1 self-contained benchmarks — they slot
directly into the existing `standard` matrix with zero infrastructure work.
Expected and CheckedArithmetic are the highest-value targets (large headers,
performance is the core claim). Signal and StringPool are the quickest wins
(small headers, obvious measurement axes).

**Longer-term:** Tier 2 (SIMD components) should follow once the ISA flag
pattern is established on one component (start with HpcVector as the
simplest). Tier 3 (JsonLite, CborLite, NumaAllocator) can be deferred until
the competitor installation pattern from ThreadPool/WorkQueue is templatized.
