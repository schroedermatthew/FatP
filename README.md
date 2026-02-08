# FAT-P

A header-only C++20 utility library. Zero external dependencies.

> ⚠️ **Work in progress** — APIs are not yet stable.

---

## Overview

FAT-P provides containers, concurrency primitives, memory management utilities, serialization, and diagnostic tools as a self-contained, header-only library. Copy `include/fat_p/` into your project and go — no build system integration required.

## Requirements

- C++20 (GCC 10+, Clang 12+, MSVC 19.29+)
- No external dependencies

## Installation

```bash
cp -r include/fat_p /your/project/include/
```

Or with CMake:

```cmake
add_subdirectory(path/to/FatP)
target_link_libraries(your_target PRIVATE fatp)
```

## Components

### Containers

| Component | Description |
|-----------|-------------|
| SmallVector | Inline-storage vector, zero heap allocation for small sizes |
| FlatMap / FlatSet | Sorted vector-backed associative containers |
| FastHashMap | High-performance open-addressing hash map |
| StableHashMap | Hash map with pointer/reference stability on insert |
| CircularBuffer | Fixed-capacity ring buffer with O(1) push/pop |
| IntrusiveList | Intrusive doubly-linked list, zero allocation overhead |
| SlotMap | Generational index container with stable handles |
| SparseSet | Sparse set with dense iteration |
| SortedContainer | Policy-based sorted vector |
| BitSet | Fixed-size bit set with compiler intrinsics |

### Memory and Allocation

| Component | Description |
|-----------|-------------|
| ObjectPool | Object pool with concurrency policy support |
| AllocationStrategies | Lightweight allocator policies |
| NumaAllocator | NUMA-aware memory allocator |
| StringPool | String interning with policy-based thread safety |
| AlignedVector | Cache-aligned vector for HPC workloads |
| HpcVector | NUMA-local, SIMD-ready vector |

### Concurrency

| Component | Description |
|-----------|-------------|
| ThreadPool | Work-stealing thread pool with priority queues |
| WorkQueue | Sharded lock-free MPMC work queue |
| LockFreeQueue | Lock-free MPMC queue with ABA prevention |
| LockFreeRingBuffer | Lock-free ring buffers (SPSC and MPMC) |
| PolicyQueue | Policy-selected lock-free queue variants |
| AtomicSharedPtr | Thread-safe atomic wrapper for shared_ptr |
| ConcurrencyPolicies | Thread-safety policy classes for containers |
| RateLimiter | Token bucket and sliding window rate limiters |
| Signal | Signal/slot implementation |

### Error Handling and Contracts

| Component | Description |
|-----------|-------------|
| Expected | Monadic error handling (value-or-error type) |
| Enforce | Design-by-contract macros with customizable predicates |
| ScopeGuard | RAII scope-exit cleanup |
| ContractException | Exception classes for contract violations |
| CheckedArithmetic | Overflow-safe arithmetic for integers and floats |
| EnforcedInit | Wrapper enforcing explicit initialization |

### Serialization

| Component | Description |
|-----------|-------------|
| JSON / JsonLite | JSON serialization with lite and streaming variants |
| CBOR / CborLite | CBOR serialization with lite and streaming variants |
| Binary | Binary serialization format |

### Math and HPC

| Component | Description |
|-----------|-------------|
| Tensor | N-dimensional tensor with math, einsum, and serialization |
| CSRMatrix | Compressed Sparse Row matrix with parallel/HPC variants |
| SimdVector | Universal SIMD wrapper for vectorized operations |
| FloatingPointComparison | ULP, relative, and absolute float comparison |

### Diagnostics and Utilities

| Component | Description |
|-----------|-------------|
| DiagnosticLogger | Structured logging with JSON output and scope tracking |
| Stacktrace | Portable stack trace capture |
| Reflection | Compile-time reflection with unified macro syntax |
| StateMachine | Type-safe FSM with compile-time transition validation |
| EnumPlus | Enhanced enums with string conversion and iteration |
| StrongId | Type-safe ID wrapper preventing cross-domain mixing |
| FeatureManager | Runtime feature flags with compile-time optimization |
| ServiceLocator | Policy-based service locator with scoped overrides |
| PipeOperator | Functional pipe operator for value composition |
| IdGenerator | Type-safe unique ID generation with recycling |
| Stringify | Type-to-string conversion using C++20 concepts |
| DebugOnly | Debug-only utilities that compile away in release |
| ConstexprUtilities | Compile-time utility functions |
| Concepts | C++20 concepts for type constraints |
| PlatformDetection | Compiler, OS, and architecture detection |
| CacheUtilities | Cache control and prefetching utilities |
| ValueGuard | Temporary value modification with automatic restoration |
| ViewLifetimeTracking | Debug-only lifetime tracking for views and references |
| MemoryMappedFile | Cross-platform memory-mapped file I/O |
| SlidingFileWindow | Sliding window access to large files |
| CoroutineTask | C++20 coroutine support with Expected integration |
| AsyncOperations | Async operation utilities |
| Factory | Policy-based factory with compile-time customization |

### Testing and Benchmarking

| Component | Description |
|-----------|-------------|
| FatPTest | Zero-dependency test framework |
| FatPBenchmarkRunner | Zero-dependency benchmark infrastructure |

## Project Structure

```
FatP/
├── include/fat_p/       # Library headers (111 files)
├── components/          # Per-component tests, benchmarks, and docs
├── Teaching/            # Guides, case studies, and reference material
├── Read_Me/             # Development guidelines and style guides
└── .github/workflows/   # CI workflows
```

## Building Tests

```bash
cmake -B build -DFATP_BUILD_TESTS=ON -DFATP_BUILD_BENCHMARKS=OFF
cmake --build build
ctest --test-dir build
```

## License

MIT — see [LICENSE](LICENSE).
