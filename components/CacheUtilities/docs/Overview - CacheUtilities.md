# CacheUtilities: A Fat-P Library Showcase

## Executive Summary

CacheUtilities is a **low-level cache control toolkit** providing software prefetching, false sharing prevention, streaming stores, and cache-aware blocking helpers. Unlike hoping the compiler optimizes memory access (unpredictable) or using platform-specific intrinsics directly (non-portable), CacheUtilities provides **cross-platform cache control** with compile-time architecture detection. The `CacheAligned<T>` wrapper prevents false sharing with zero syntax overhead, while `prefetch_ahead()` reduces memory latency by 2-3× for predictable access patterns.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// False sharing disaster: 4 threads, 4 atomics, ONE cache line
struct alignas(4) Counters {  // Only 4-byte aligned!
    std::atomic<int> thread0;  // Bytes 0-3
    std::atomic<int> thread1;  // Bytes 4-7
    std::atomic<int> thread2;  // Bytes 8-11
    std::atomic<int> thread3;  // Bytes 12-15
};  // All 4 fit in one 64-byte cache line!

Counters counters;

void worker(int id) {
    auto& my_counter = (&counters.thread0)[id];
    for (int i = 0; i < 10000000; ++i) {
        my_counter.fetch_add(1);  // Invalidates entire cache line
        // Other threads' caches are now stale—they must re-fetch
    }
}
// Result: 4 threads run SLOWER than 1 thread
```

| Issue | HPC Impact |
|-------|------------|
| False sharing | Parallel threads invalidate each other's cache lines |
| Missing prefetch | CPU stalls waiting for memory (100+ ns latency) |
| Cache-oblivious blocking | Matrix operations thrash cache, losing 10× performance |
| Platform-specific intrinsics | `_mm_prefetch` (x86) vs `__pld` (ARM) vs nothing (portable) |

### The Hardware Reality

```
CPU Core → L1 Cache (32KB, ~4 cycles)
         → L2 Cache (256KB, ~12 cycles)  
         → L3 Cache (8MB+, ~40 cycles)
         → Main Memory (~200 cycles / ~60ns local, ~150ns remote NUMA)

Cache line: 64 bytes (x86 and ARM)
- Loading 1 byte loads the whole 64-byte line
- Modifying 1 byte invalidates the whole 64-byte line on other cores
```

**The false sharing trap:** Two variables in the same cache line means two threads modifying "different" data actually share the same hardware resource. Each write forces other cores to re-fetch the line.

### The Standard's Limitation

C++17 added `std::hardware_destructive_interference_size` (the cache line size for false sharing) and `std::hardware_constructive_interference_size` (the cache line size for locality). But:

1. Many compilers define it as 64 regardless of actual hardware
2. No prefetch intrinsics in the standard
3. No streaming store (non-temporal) support
4. No cache flush control

---

## Architecture: Portable Cache Control

### The Mechanism: Compile-Time Architecture Detection

```cpp
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #define CACHE_X86
    #include <immintrin.h>  // x86 intrinsics
#elif defined(__ARM_NEON) || defined(__aarch64__)
    #define CACHE_ARM
    #include <arm_acle.h>   // ARM intrinsics
#endif

template<PrefetchLocality Locality, PrefetchOp Op>
inline void prefetch(const void* addr) {
#if defined(CACHE_X86)
    _mm_prefetch(addr, /* hint based on Locality */);
#elif defined(CACHE_ARM)
    __pld(addr);  // or __pldx for write prefetch
#else
    __builtin_prefetch(addr, /* op */, /* locality */);  // GCC/Clang fallback
#endif
}
```

**The key abstraction:** You write `prefetch<PrefetchLocality::High>(ptr)`. The compiler generates `_mm_prefetch` on x86, `__pld` on ARM, or the GCC builtin elsewhere.

---

## Feature Inventory

### 1. False Sharing Prevention: CacheAligned<T>

```cpp
// BEFORE: False sharing disaster
struct Counters {
    std::atomic<int> thread0;
    std::atomic<int> thread1;
    std::atomic<int> thread2;
    std::atomic<int> thread3;
};  // 16 bytes, all in one cache line

// AFTER: Each counter on its own cache line
struct Counters {
    fat_p::perf::CacheAligned<std::atomic<int>> thread0;
    fat_p::perf::CacheAligned<std::atomic<int>> thread1;
    fat_p::perf::CacheAligned<std::atomic<int>> thread2;
    fat_p::perf::CacheAligned<std::atomic<int>> thread3;
};  // 256 bytes, four separate cache lines

// Usage is transparent
counters.thread0.get().fetch_add(1);  // Or use implicit conversion:
int val = counters.thread0;           // Implicit conversion to int
```

**The mechanism:**

```cpp
template<typename T>
struct alignas(64) CacheAligned {  // alignas(destructive_interference_size)
    T value;
    
    // Implicit conversions for transparent usage
    operator T&() noexcept { return value; }
    operator const T&() const noexcept { return value; }
    
    T& get() noexcept { return value; }
};
```

Each `CacheAligned<T>` starts on a 64-byte boundary. No two wrapped values share a cache line.

### 2. CacheLinePadded<T>: Full Cache Line Occupancy

```cpp
// CacheAligned: aligned to cache line START
// CacheLinePadded: occupies the FULL cache line

template<typename T>
struct CacheLinePadded {
    T value;
    char padding[64 - sizeof(T)];  // Pad to fill line
};

// Use when T is small and you want to prevent ANY sharing
fat_p::perf::CacheLinePadded<int> counter;  // 4 bytes value + 60 bytes padding
```

**When to use which:**

| Wrapper | Size | Use Case |
|---------|------|----------|
| `CacheAligned<T>` | `max(sizeof(T), 64)` | Multiple fields accessed together |
| `CacheLinePadded<T>` | Always 64 | Single hot variable, maximum isolation |

### 3. Software Prefetching

```cpp
// Prefetch with temporal locality hints
fat_p::perf::prefetch<PrefetchLocality::High>(ptr);      // Keep in all caches
fat_p::perf::prefetch<PrefetchLocality::Low>(ptr);       // L3 only
fat_p::perf::prefetch<PrefetchLocality::None>(ptr);      // Don't cache (streaming)

// Prefetch for writing (exclusive state)
fat_p::perf::prefetch<PrefetchLocality::High, PrefetchOp::Write>(ptr);

// Prefetch a range
fat_p::perf::prefetch_range(ptr, size_bytes);

// Loop prefetch pattern
for (size_t i = 0; i < n; ++i) {
    fat_p::perf::prefetch_ahead(data, i, 1, 8);  // Prefetch 8 iterations ahead
    process(data[i]);
}
```

**Locality hints explained:**

| Locality | x86 Hint | Meaning | Use Case |
|----------|----------|---------|----------|
| `High` | `_MM_HINT_T0` | Keep in L1/L2/L3 | Data reused many times |
| `Moderate` | `_MM_HINT_T1` | Keep in L2/L3 | Data reused a few times |
| `Low` | `_MM_HINT_T2` | Keep in L3 only | Data reused once or twice |
| `None` | `_MM_HINT_NTA` | Don't cache | Streaming data (one-time use) |

### 4. Streaming (Non-Temporal) Stores

```cpp
// Normal store: data goes to cache, eventually written to memory
*ptr = value;  // Pollutes cache with write-only data

// Streaming store: bypasses cache, writes directly to memory
fat_p::perf::stream_store(ptr, value);  // Doesn't pollute cache

// Streaming memcpy for large data
fat_p::perf::stream_copy(dest, src, size);  // Uses _mm256_stream_si256
```

**When to use streaming stores:**

- Writing data that won't be read again soon
- Initializing large arrays
- Writing to memory-mapped I/O
- Any write-only pattern where caching wastes space

### 5. Cache Flush Operations

```cpp
// Flush a single cache line to memory
fat_p::perf::flush_cache_line(ptr);

// Flush a range
fat_p::perf::flush_cache_range(ptr, size);

// Flush and invalidate (stronger—ensures other CPUs see the write)
fat_p::perf::flush_invalidate(ptr);
```

**Use cases:**
- Ensuring data is visible to DMA devices
- Persistent memory programming
- Benchmarking (cold cache testing)

### 6. Memory Barriers

```cpp
fat_p::perf::memory_barrier();  // Full fence (mfence on x86)
fat_p::perf::store_fence();     // Store fence (sfence)
fat_p::perf::load_fence();      // Load fence (lfence)
```

### 7. Cache-Aware Blocking Helpers

```cpp
// Compute optimal block size for matrix operations
size_t block = fat_p::perf::optimal_block_size(
    elements,           // Total elements
    sizeof(double),     // Element size
    32 * 1024           // L1 cache size (32KB)
);

// 2D blocking iterator for matrices
fat_p::perf::BlockIterator2D iter(rows, cols, block_rows, block_cols);
while (iter.has_next()) {
    size_t i0 = iter.block_start_i(), i1 = iter.block_end_i();
    size_t j0 = iter.block_start_j(), j1 = iter.block_end_j();
    
    // Process block [i0:i1, j0:j1]
    process_block(matrix, i0, i1, j0, j1);
    
    iter.next_block();
}
```

### 8. Alignment Utilities

```cpp
// Check alignment
bool aligned = fat_p::perf::isAligned(ptr, 64);

// Align pointer up
void* aligned_ptr = fat_p::perf::align_up(ptr, 64);

// Align pointer down
void* aligned_ptr = fat_p::perf::align_down(ptr, 64);

// Bytes needed to align
size_t offset = fat_p::perf::alignment_offset(ptr, 64);
```

---

## Why Not Alternatives?

| If You Need... | Why Not Raw Intrinsics | Why Not std:: | Why Not Compiler Builtins | Fat-P Advantage |
|----------------|------------------------|---------------|---------------------------|-----------------|
| Cross-platform | x86 vs ARM differences | Limited support | Non-standard | Unified API |
| Prefetch hints | Different syntax per arch | Not available | Available but verbose | Clean enum-based hints |
| False sharing prevention | Manual `alignas(64)` | `hardware_destructive_interference_size` | Not available | `CacheAligned<T>` wrapper |
| Streaming stores | Different intrinsics | Not available | Not available | `stream_store()`, `stream_copy()` |
| Cache blocking | Manual implementation | Not available | Not available | `optimal_block_size()`, `BlockIterator2D` |

**The Sweet Spot:** CacheUtilities provides a unified, portable interface to cache control primitives that would otherwise require platform-specific intrinsics or manual `alignas` calculations.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ provides `hardware_destructive_interference_size` but no prefetch, no streaming stores, no cache flush. These are "too low-level" for the standard committee.

**Performance Reality:** Cache optimization is where the 10× speedups live. A cache-blocked matrix multiply beats a naive implementation by 10×. Prefetching reduces latency by 2-3×. False sharing prevention makes parallel code actually scale.

**Platform Reality:** Every CPU has cache control instructions. CacheUtilities makes them accessible without `#ifdef` forests.

---

## Performance Characteristics

| Operation | Cost | Mechanism |
|-----------|------|-----------|
| `CacheAligned<T>` access | Same as `T` | Zero overhead after alignment |
| `prefetch()` | ~1-2 cycles | Hint to prefetch unit |
| `stream_store()` | Similar to normal store | Bypasses cache |
| `flush_cache_line()` | ~100 cycles | Forces writeback |
| `memory_barrier()` | ~20-50 cycles | Serializes operations |

### Where Fat-P Wins

**False sharing elimination:** `CacheAligned<T>` makes parallel counters scale linearly instead of negatively.

**Predictable access patterns:** Prefetching matrix rows or array elements ahead of use hides memory latency.

**Write-only data:** Streaming stores prevent cache pollution for initialization or output buffers.

### Where Fat-P Loses (Honesty Builds Trust)

**Unpredictable access:** Prefetching hurts performance if you prefetch the wrong data—the CPU's hardware prefetcher is often smarter than manual hints for irregular patterns.

**Small data:** Cache control overhead matters for large data; for small arrays, let the hardware handle it.

**Overuse:** Excessive prefetch instructions can saturate memory bandwidth and hurt performance.

---

## Integration Points

```
CacheUtilities.h
    ↓ provides
CacheAligned<T>       (False sharing prevention)
prefetch<>()          (Software prefetch hints)
stream_store()        (Non-temporal stores)
    ↓ used by
ThreadPool.h          (Cache-aligned work queues)
LockFreeQueue.h       (Padded head/tail pointers)
HpcVector.h           (Alignment verification)
```

**Typical usage pattern:**

```cpp
// Thread-local counters without false sharing
struct ThreadStats {
    fat_p::perf::CacheAligned<std::atomic<size_t>> processed;
    fat_p::perf::CacheAligned<std::atomic<size_t>> errors;
};

std::array<ThreadStats, 64> stats;  // One per thread, no sharing

// Prefetched array processing
void process_array(float* data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        fat_p::perf::prefetch_ahead(data, i, 1, 16);  // 16 elements ahead
        data[i] = expensive_compute(data[i]);
    }
}
```

---

## Final Assessment

CacheUtilities delivers on the fat_p promise through three pillars:

### 1. Permanence
Cache control intrinsics will never be in the C++ standard—they're too platform-specific. CacheUtilities provides portable cache optimization permanently.

### 2. Specialization
`CacheAligned<T>`, prefetch hints, streaming stores, and blocking helpers address HPC-specific cache optimization needs that generic code ignores.

### 3. Control
Locality hints (`High`, `Low`, `None`) and operation types (`Read`, `Write`) give precise control over prefetch behavior. `CacheAligned` vs `CacheLinePadded` provides fine-grained false sharing control.

**Architectural Verdict:** CacheUtilities transforms cache optimization from **platform-specific intrinsic knowledge** to **portable, intention-revealing code**—letting you write `CacheAligned<std::atomic<int>>` instead of calculating padding manually.

---

*CacheUtilities.h (532 lines) — Fat-P Library*
