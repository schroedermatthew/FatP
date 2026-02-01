# FAT-P

**A C++20 library that competes with Boost, LLVM, and Abseilâ€”built entirely through AI collaboration.**

---

## The Experiment

What happens when you treat AI as **author and architect**, not assistant?

FAT-P is the answer: 425,000 lines of production C++ and documentation, zero external dependencies, competitive with libraries maintained by Google, Meta, EA, and the LLVM project.

| Metric | Value |
|--------|-------|
| C++ code | 232,644 lines |
| Documentation | 192,302 lines |
| Components | 111 headers |
| External dependencies | **Zero** |

The AI wrote the code. The AI designed the architectures. The AI wrote the documentation. The human provided vision, constraints, and judgmentâ€”but did not write code.

This is not "AI-assisted development." This is **AI-authored software** with human direction.

---

## The Methodology

Four AI systems collaborate through an 8-phase pipeline:

| Phase | Description |
|-------|-------------|
| 1. Requirements | Human defines constraints |
| 2. Parallel Design | 4 AIs design independently |
| 3. Cross-Review | AIs critique each other's work |
| 4. Synthesis | Lead AI merges best elements |
| 5. Implementation | Code with autonomous debugging |
| 6. Context Reset | Fresh AI reviews with new eyes |
| 7. Documentation | Teaching-first, not API-first |
| 8. Final Review | Human approval to ship |

**Key innovations:**

- **Context Reset**: Deliberately starting fresh sessions to escape accumulated assumptions. The AI that wrote the code is not the AI that reviews it.
- **Cross-Review**: AIs reject each other's proposals. Grok's "hallucinations" become creative options. ChatGPT surfaces architectures Claude wouldn't consider.
- **Guidelines as Automated Judgment**: Every collaborative decision becomes a rule. Guidelines compound over time, enabling autonomous decisions in similar situations.
- **Demerit System**: AI mistakes are tracked. Accountability exists.

The full methodology is documented and reproducible: **[AI-Collaborative Development Methodology](Read_Me/Fat-P_AI_Collaborative_Development_Methodology.md)**

---

## The Proof

Benchmarked against 50+ competitor implementations. All times in nanoseconds per operation.

### ObjectPool (16-byte objects, 100K ops)

| Implementation | Acquire/Release | Bulk Acquire | Interleaved |
|----------------|-----------------|--------------|-------------|
| EASTL::fixed_pool | 2.13 | 2.10 | 8.11 |
| **fat_p::ObjectPool** | **2.31** | **2.09** | **9.23** |
| boost::object_pool | 2.37 | 8.35 | 12.81 |
| foonathan::memory_pool | 2.87 | 2.29 | 8.22 |
| std::pmr::unsync_pool | 5.94 | 8.09 | 12.00 |
| new/delete | 19.87 | 23.06 | 18.69 |

FAT-P ties the leaders, **8.6x faster than new/delete**.

### SmallVector (N=1000 elements)

| Implementation | push_back | emplace_back | iteration | operator[] |
|----------------|-----------|--------------|-----------|------------|
| **fat_p::SmallVector** | **0.60** | **0.60** | 1.20 | 1.20 |
| llvm::SmallVector | 0.70 | 0.50 | 1.20 | 1.20 |
| boost::small_vector | 0.70 | 0.60 | 1.20 | 1.20 |
| absl::InlinedVector | 0.70 | 0.60 | 1.20 | 1.20 |
| ankerl::svector | 0.60 | 0.60 | 1.30 | 1.20 |
| eastl::fixed_vector | 1.00 | 0.40 | 1.20 | 1.20 |
| std::vector | 1.00 | 0.60 | 1.20 | 1.20 |

All implementations within measurement noise for core operations. The real win is **avoiding heap allocation**:

| Size | SmallVector | std::vector | Speedup |
|------|-------------|-------------|---------|
| 4 (inline) | 4.05 ns | 22.89 ns | **5.6x** |
| 8 (inline) | 2.32 ns | 16.39 ns | **7.1x** |
| 16 (inline) | 1.16 ns | 10.80 ns | **9.3x** |

### Lock-Free Containers

| Implementation | Single-Thread | SPSC (2 threads) | MPMC (16 threads) |
|----------------|---------------|------------------|-------------------|
| **fat_p::LockFreeRingBuffer (SPSC)** | **0.52** | 31.13 | â€” |
| **fat_p::WorkQueue (sharded)** | 10.06 | â€” | **24.4** |
| fat_p::LockFreeQueue | 8.25 | 10.43 | 89.5 |
| moodycamel::ConcurrentQueue | 8.68 | 19.00 | 38.8 |
| std::mutex + std::queue | 16.65 | 20.58 | 247.4 |
| boost::lockfree::queue | 54.15 | 113.99 | 287.5 |

### FastHashMap

| Implementation | Insert | Lookup | Erase |
|----------------|--------|--------|-------|
| **fat_p::FastHashMap** | **4.37** | â€” | â€” |
| boost::unordered_flat_map | 6.72 | â€” | â€” |
| tsl::robin_map | 7.29 | â€” | â€” |
| llvm::DenseMap | 7.81 | â€” | â€” |
| absl::flat_hash_map | 11.72 | â€” | â€” |
| ankerl::unordered_dense | 19.92 | â€” | â€” |
| std::unordered_map | 34.96 | â€” | â€” |

### StableHashMap (Pointer-Stable)

| Implementation | Insert | Lookup | Erase |
|----------------|--------|--------|-------|
| **fat_p::StableHashMap** | **6.01** | â€” | â€” |
| boost::unordered_node_map | 26.04 | â€” | â€” |
| absl::node_hash_map | 29.33 | â€” | â€” |
| std::unordered_map | 34.96 | â€” | â€” |

StableHashMap guarantees pointer/reference stability on insertâ€”critical for intrusive data structures and caches. It beats all pointer-stable competitors by 4x+.

**The critical difference**: FAT-P achieves this with **zero external dependencies**. Competitors require Boost (massive), LLVM libraries, EASTL runtime, or Abseil build integration. FAT-P is header-onlyâ€”copy the `include/fat_p/` directory and go.

---

## The Library

### What's Included

### Components

| Component | Description |
|-----------|-------------|
| **AlignedVector** | Cache-aware aligned vector container for HPC workloads |
| **AllocationStrategies** | Lightweight allocator policies for FAT-P containers |
| **AsyncOperations** | Asynchronous operation utilities with Expected integration |
| **AtomicSharedPtr** | Minimal thread-safe atomic wrapper for std::shared_ptr |
| **BitSet** | High-performance fixed-size bit set with compiler intrinsics |
| **CacheUtilities** | Cache control, prefetching, and cache-aware programming utilities |
| **CheckedArithmetic** | Overflow-safe arithmetic for integers and floating-point |
| **CircularBuffer** | Fixed-capacity circular buffer with O(1) push/pop |
| **ComparisonTolerances** | Tolerance definitions for floating-point comparisons |
| **Concepts** | C++20 concepts for type constraints |
| **ConcurrencyPolicies** | Thread-safety policy classes for container customization |
| **ConstexprUtilities** | Compile-time utility functions and metaprogramming helpers |
| **ContractException** | Base exception classes for design-by-contract violations |
| **CoroutineTask** | C++20 coroutine support with Expected-based error handling |
| **CppFeatureDetection** | C++ language and library feature detection |
| **CSRMatrix** | Compressed Sparse Row matrix with parallel/HPC variants |
| **DebugOnly** | Debug-only utilities that compile to nothing in release |
| **DiagnosticLogger** | Structured logging with JSON output and scope tracking |
| **Enforce** | Design-by-contract macros with customizable predicates and raisers |
| **EnforcedInit** | Wrapper enforcing explicit initialization before use |
| **EnhancedBoundsChecking** | Bounds checking with detailed diagnostic messages |
| **EnumPlus** | Enhanced enums with string conversion and iteration |
| **EqualityComparisons** | Recursive equality for containers and nested types |
| **Expected** | Monadic error handling (value-or-error type) |
| **Factory** | Policy-based factory with compile-time customization |
| **FastHashMap** | High-performance hash map optimized for speed |
| **FatPBenchmarkRunner** | Zero-dependency benchmark infrastructure |
| **FatPConfig** | Central configuration macros |
| **FatPTest** | Zero-dependency test framework |
| **FeatureManager** | Runtime feature flags with compile-time optimization |
| **FlatMap** | Sorted vector-backed associative container |
| **FlatSet** | Sorted vector-backed set with contiguous storage |
| **FloatingPointComparison** | Robust floating-point comparison (ULP, relative, absolute) |
| **HpcVector** | Cache-aligned, NUMA-local, SIMD-ready vector |
| **IdGenerator** | Type-safe unique ID generation with recycling support |
| **IntrusiveList** | Intrusive doubly-linked list with zero allocation overhead |
| **JSON** | JSON serialization with lite and streaming variants |
| **CBOR** | CBOR serialization with lite and streaming variants |
| **Binary** | Binary serialization format |
| **LockFreeQueue** | Lock-free MPMC queue with ABA prevention |
| **LockFreeRingBuffer** | Lock-free ring buffers for SPSC and MPMC |
| **MemoryMappedFile** | Cross-platform memory-mapped file I/O |
| **NumaAllocator** | NUMA-aware memory allocator for many-core systems |
| **ObjectPool** | High-performance object pool with concurrency policies |
| **PipeOperator** | Functional pipe operator for value composition |
| **PlatformDetection** | Compiler, OS, and architecture detection |
| **PolicyIterator** | Customizable traversal strategies for containers |
| **PolicyQueue** | Policy-selected lock-free queue variants |
| **RateLimiter** | Token bucket and sliding window rate limiters |
| **Reflection** | Compile-time reflection with unified macro syntax |
| **ScopeGuard** | RAII scope-exit cleanup |
| **ServiceLocator** | Policy-based service locator with scoped overrides |
| **Signal** | High-performance signal/slot implementation |
| **SimdDetection** | SIMD capability detection |
| **SimdVector** | Universal SIMD wrapper for vectorized operations |
| **SlidingFileWindow** | Sliding window access to large files with on-demand paging |
| **SlotMap** | Generational index container with stable handles and O(1) access |
| **SmallVector** | Inline storage vectorâ€”zero heap allocation for small sizes |
| **SortedContainer** | Policy-based sorted vector maintaining order on insert |
| **SparseSet** | Sparse set with dense iteration |
| **StableHashMap** | Reference-stable hash map with SIMD-accelerated probing |
| **Stacktrace** | Portable stack trace capture with multi-backend support |
| **StateMachine** | Type-safe FSM with compile-time transition validation |
| **StringPool** | String interning with policy-based thread safety |
| **Stringify** | Type-to-string conversion using C++20 concepts |
| **StrongId** | Type-safe ID wrapper preventing cross-domain mixing |
| **Tensor** | N-dimensional tensor with math, einsum, and serialization |
| **ThreadPool** | Work-stealing thread pool with priority queues |
| **ValueGuard** | Temporary value modification with automatic restoration |
| **ViewLifetimeTracking** | Debug-only lifetime tracking for views and references |
| **WorkQueue** | Sharded lock-free MPMC work queue |

See [`include/fat_p/`](include/fat_p/) for all headers.

### Quick Start

```cpp
#include "SmallVector.h"
#include "StrongId.h"
#include "Expected.h"

// Type-safe IDs: compiler prevents mixing UserId with OrderId
using UserId = fat_p::StrongId<struct UserTag, int>;
using OrderId = fat_p::StrongId<struct OrderTag, int>;

// SmallVector: zero heap allocation for small sizes
fat_p::SmallVector<int, 8> numbers = {1, 2, 3, 4};  // All inline, no malloc
numbers.push_back(5);  // Still inline

// Expected: errors are values, not exceptions
fat_p::Expected<Config, ParseError> result = parseConfig("app.json");
if (result) {
    useConfig(*result);
} else {
    log(result.error());  // Compiler enforces you handle this
}
```

### Requirements

- **C++20** (GCC 10+, Clang 12+, MSVC 19.29+)
- Header-onlyâ€”no build step required
- **Fully self-contained**: includes its own test framework (FatPTest) and benchmark runner (FatPBenchmarkRunner)

### Installation

**Option 1: Copy headers**
```bash
cp -r include/fat_p /your/project/include/
```

**Option 2: CMake**
```cmake
add_subdirectory(path/to/FatP)
target_link_libraries(your_target PRIVATE fatp)
```

---

## The Teaching Library

FAT-P includes 192,000 lines of documentationâ€”not API references, but **teaching materials**:

### Migration Guides (C â†’ Modern C++)

18 guides transforming legacy patterns into FAT-P components:

| Legacy Pattern | FAT-P Component |
|----------------|-----------------|
| Switch statements | â†’ StateMachine |
| Callbacks/function pointers | â†’ Signal |
| Integer handles | â†’ StrongId |
| Manual memory management | â†’ ScopeGuard, ObjectPool |
| Error codes | â†’ Expected |
| Linear search in arrays | â†’ FlatMap |
| Feature flags | â†’ FeatureManager |

### Case Studies

Real debugging sessions with real bugs:
- "The Slow Miss" â€” hash map performance investigation
- "Fuzzy Equality Non-Transitivity" â€” when approximate comparison breaks sorting
- "The Noreturn Mirage" â€” exception safety edge cases

### Compile-Time Safety Course

11 problem sessions on catching errors before runtime:
- StrongId and phantom types
- State machine transition validation
- Template constraints with concepts
- Variant exhaustiveness

### Handbooks

- Discipline of Class Design
- Performance Engineering Methodology
- Bridging the Hardware Gap
- FAT-P Serialization

See [`Teaching/`](Teaching/) for the complete library.

---

## Project Structure

```
FatP/
â”œâ”€â”€ include/fat_p/       # The library (111 headers)
â”œâ”€â”€ components/          # Tests, benchmarks, docs per component
â”‚   â””â”€â”€ <Component>/
â”‚       â”œâ”€â”€ tests/
â”‚       â”œâ”€â”€ benchmarks/
â”‚       â”œâ”€â”€ docs/
â”‚       â””â”€â”€ results/
â”œâ”€â”€ Teaching/            # Migration guides, case studies, courses
â”œâ”€â”€ Read_Me/             # Governance and methodology docs
â”œâ”€â”€ ThirdParty/          # Benchmark competitors not available in vcpkg
â””â”€â”€ .github/workflows/   # CI (label-gated)
```

**Note:** Benchmark comparisons use third-party libraries via **vcpkg** (primary) and a local `ThirdParty/` folder (for libraries not in vcpkg). These are **not** part of the FAT-P library. The `fatp` CMake target exports only `include/fat_p/` with zero external dependencies.

---

## Building Tests & Benchmarks

**Tests only (no external dependencies):**
```bash
cmake -B build -DFATP_BUILD_TESTS=ON -DFATP_BUILD_BENCHMARKS=OFF
cmake --build build
ctest --test-dir build
```

**Benchmarks with competitor comparisons (requires vcpkg):**
```bash
# Ensure VCPKG_ROOT is set, then:
cmake -B build -DFATP_BUILD_TESTS=ON -DFATP_BUILD_BENCHMARKS=ON
cmake --build build
```

Competitor libraries (Abseil, Boost, EASTL, etc.) are installed via vcpkg. Libraries not available in vcpkg can be placed in `ThirdParty/`.

---

## The Philosophy

> "The AIs are authors, not tools. They designed the architectures. They found the edge cases. They debugged autonomously. They wrote the documentationâ€”not transcription, authorship."

See **[Authors.md](Authors.md)** for the full philosophy on human-AI collaboration.

> "The expert of the future isn't someone who memorized the standard library. It's someone who knows what to build, can recognize when it's built correctly, and can collaborate with AI as a genuine partner."

---

## Governance

FAT-P is governed by a documented set of style guides and policies:

| Document | Purpose |
|----------|---------|
| [Development Guidelines](Read_Me/Fat-P_Library_Development_Guidelines.md) | Code standards, design principles |
| [AI Methodology](Read_Me/Fat-P_AI_Collaborative_Development_Methodology.md) | The 8-phase pipeline |
| [Teaching Documents Style Guide](Read_Me/FatP_Teaching_Documents_Style_Guide.md) | Documentation standards |
| [Test Suite Style Guide](Read_Me/FatP_Test_Suite_Style_Guide.md) | Testing requirements |

---

## License

MIT License. See [LICENSE](LICENSE) for details.

---

*FAT-P: What AI-collaborative development produces when you take it seriously.*
