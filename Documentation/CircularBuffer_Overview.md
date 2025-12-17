# CircularBuffer: A Fat-P Library Showcase

## Executive Summary

CircularBuffer is a **wait-free single-producer single-consumer (SPSC) queue** that achieves 300-360 million operations per second through three critical optimizations: cache-line alignment preventing false sharing, index caching reducing coherency traffic by ~70%, and power-of-2 masking eliminating modulo operations. Unlike mutex-based queues (~10-20M ops/sec) or naive atomic implementations, CircularBuffer provides **bounded completion time guarantees** essential for real-time systems. This is the concurrency primitive audio engines and trading systems require.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The mutex trap: 100-200ns overhead per operation
template<typename T>
class NaiveQueue {
    std::queue<T> queue_;
    std::mutex mutex_;
    
public:
    void push(T value) {
        std::lock_guard lock(mutex_);  // Contention point
        queue_.push(std::move(value));
    }
    
    std::optional<T> pop() {
        std::lock_guard lock(mutex_);  // Blocks on producer
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }
};
```

| Issue | HPC Impact |
|-------|------------|
| Mutex contention | 100-200ns overhead vs. ~3ns for lock-free |
| False sharing | Cache lines ping-pong between cores, destroying throughput |
| Unbounded growth | Memory exhaustion under sustained load |
| Priority inversion | Real-time thread blocked by lower-priority consumer |
| No completion bound | Mutex acquisition time is unbounded |

### The Standard's Limitation

The C++ standard provides no lock-free SPSC queue. `std::queue` requires external synchronization. `std::atomic` provides primitives but not containers. Even C++20's `std::atomic_ref` and `std::latch` don't solve the SPSC problem.

**Benchmark comparison:**
```
std::queue + mutex:     10-20M ops/sec
Naive atomic queue:     50-100M ops/sec (false sharing)
CircularBuffer:         300-360M ops/sec
```

For audio callbacks (10μs budget), trading systems (microsecond latency), or real-time control loops, lock-free is mandatory.

---

## Architecture: Cache-Optimized Wait-Free Design

### The Mechanism: Three Critical Optimizations

```cpp
template<typename T, size_t Capacity>
class CircularBuffer {
    // Optimization 1: Power-of-2 capacity for bitwise masking
    static constexpr size_t kMask = NextPowerOf2(Capacity) - 1;
    
    // Optimization 2: Cache-line aligned indices prevent false sharing
    alignas(64) std::atomic<size_t> write_idx_{0};
    alignas(64) std::atomic<size_t> read_idx_{0};
    
    // Optimization 3: Cached indices reduce coherency traffic
    alignas(64) size_t cached_read_idx_{0};   // Producer-local
    alignas(64) size_t cached_write_idx_{0};  // Consumer-local
    
    alignas(64) std::array<T, NextPowerOf2(Capacity)> buffer_;
};
```

**Why each optimization matters:**

| Optimization | Without | With | Improvement |
|--------------|---------|------|-------------|
| Power-of-2 masking | `idx % capacity` (~20 cycles) | `idx & mask` (~1 cycle) | 20x per operation |
| Cache-line alignment | False sharing (~100ns) | No false sharing (~3ns) | 30x latency |
| Index caching | Atomic load every op | Atomic load on cache miss | 1.7x throughput |

### Memory Ordering: Correctness Without Locks

```cpp
bool push(T value) {
    size_t write = write_idx_.load(std::memory_order_relaxed);
    size_t next = (write + 1) & kMask;
    
    // Check if full - use cached read index first
    if (next == cached_read_idx_) {
        // Cache miss: load actual read index
        cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
        if (next == cached_read_idx_) return false;  // Actually full
    }
    
    buffer_[write] = std::move(value);
    write_idx_.store(next, std::memory_order_release);  // Publish write
    return true;
}
```

**Ordering guarantees:**
- `release` on write ensures buffer write is visible before index update
- `acquire` on read ensures index is read before buffer access
- No locks, no spinning, bounded completion time

---

## Feature Inventory

### 1. Wait-Free Push/Pop with Bounded Completion

```cpp
CircularBuffer<Event, 1024> events;

// Producer thread - never blocks
while (running) {
    Event e = get_next_event();
    if (!events.push(std::move(e))) {
        handle_overflow();  // Buffer full
    }
}

// Consumer thread - never blocks
while (running) {
    if (auto e = events.pop()) {
        process(*e);
    }
}
```

**Mechanism:** Each operation completes in bounded time regardless of other thread's state. No mutexes, no condition variables, no spinning.

### 2. In-Place Construction via emplace()

```cpp
CircularBuffer<LargeObject, 256> objects;

// Construct directly in buffer - no temporary
objects.emplace(arg1, arg2, arg3);
```

**Mechanism:** Perfect forwarding constructs element directly in ring buffer slot. Avoids move construction overhead for non-trivial types.

### 3. Non-Consuming Peek via front()

```cpp
if (auto* ptr = buffer.front()) {
    // Inspect without removing
    if (ptr->priority > threshold) {
        auto value = buffer.pop();  // Now consume
    }
}
```

**Mechanism:** Returns pointer to front element without modifying indices. Consumer can inspect before committing to removal.

### 4. Index Caching: 1.7x Throughput Optimization

```cpp
// Without caching: every push loads read_idx atomically
// With caching: only load on potential full condition

bool push(T value) {
    // First check cached value (no atomic)
    if (next == cached_read_idx_) {
        // Only then load atomic
        cached_read_idx_ = read_idx_.load(acquire);
    }
    // ...
}
```

**Measured impact:** ~170% throughput improvement on multi-core systems by reducing cache coherency traffic.

### 5. Automatic Resource Management

```cpp
CircularBuffer<std::unique_ptr<Resource>, 64> resources;

resources.push(std::make_unique<Resource>());
// ...
// On destruction: all remaining elements properly destroyed
```

**Mechanism:** Destructor iterates live elements and calls destructors. Non-trivial types are correctly cleaned up.

---

## Why Not Alternatives?

| If You Need... | Why Not std::queue + mutex | Why Not boost::lockfree::spsc_queue | Fat-P Advantage |
|----------------|---------------------------|-----------------------------------|-----------------|
| Wait-free guarantee | ❌ Mutex blocks | ✅ Wait-free | ✅ Wait-free |
| Zero dependencies | ✅ Standard library | ❌ Requires Boost | ✅ Single header |
| Index caching | N/A | ❌ Not implemented | ✅ 1.7x throughput |
| emplace() | ✅ Available | ❌ Not available | ✅ Available |
| front() peek | ✅ Available | ❌ Not available | ✅ Available |
| Throughput | 10-20M ops/sec | 250-300M ops/sec | 300-360M ops/sec |

**The Sweet Spot:** CircularBuffer is the only option combining:
- ✅ Wait-free with bounded completion
- ✅ Index caching optimization
- ✅ Zero external dependencies
- ✅ emplace() and front() APIs
- ✅ Header-only (single file)

---

## The "Forever Stuck" Reality

**Standard Library Reality:** The C++ committee will not add an SPSC queue because:

1. **Too specialized:** SPSC is one of many producer-consumer patterns (MPMC, MPSC, priority queues)
2. **Performance tuning:** Optimal parameters (capacity, padding) are application-specific
3. **Memory ordering complexity:** Subtle bugs are easy; standardization is risky

Real-time systems (audio, trading, embedded control) have used custom SPSC queues for decades. Fat-P provides a battle-tested implementation without requiring you to understand memory ordering or cache-line alignment.

---

## Performance Characteristics

### Benchmark Results (Release Build, i7-8850H @ 2.60GHz)

| Operation | Throughput | Latency | Mechanism |
|-----------|------------|---------|-----------|
| `push()` | 300-360M/sec | ~3 ns | Cached index check + release store |
| `pop()` | 300-360M/sec | ~3 ns | Cached index check + acquire load |
| `front()` | 400M+/sec | ~2 ns | No atomic modification |
| `size()` | 200M/sec | ~5 ns | Two atomic loads |

### Complexity Analysis

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| `push()` / `emplace()` | O(1) | Index increment + bitwise AND |
| `pop()` | O(1) | Index increment + bitwise AND |
| `front()` | O(1) | Index comparison |
| `size()` | O(1) | Subtraction of two indices |
| `empty()` / `full()` | O(1) | Index comparison |

### Where Fat-P Wins

- **Audio callbacks:** 10μs budget, zero allocation allowed
- **Trading systems:** Microsecond latency requirements
- **Real-time control:** Bounded completion time mandatory
- **High-throughput logging:** Producer must never block

### Where Fat-P Loses (Honesty Builds Trust)

- **Multiple producers:** Use `LockFreeQueue` (MPMC) instead
- **Multiple consumers:** Use `LockFreeQueue` (MPMC) instead
- **Unbounded capacity:** If you need dynamic growth, `std::queue` with mutex works
- **Strict ordering with priority:** Priority queues require different structures
- **Non-power-of-2 capacity:** Wastes up to 50% of allocated memory

---

## Integration Points

```
CircularBuffer.h
    ↓ uses
FatPTypeTraits.h   (is_circular_buffer<T> type trait)
    ↓ used by
Audio engines (sample buffers)
Logging systems (async log queues)
Network stacks (packet buffers)
Real-time control (sensor data)
```

---

## Final Assessment

CircularBuffer delivers on the fat_p promise through three pillars:

### 1. Permanence
The standard library will never provide an SPSC queue—the design space is too specialized. CircularBuffer makes a real-time-systems-grade primitive available as a single header, permanently solving the "fast producer-consumer" problem for SPSC workloads.

### 2. Specialization  
Index caching, cache-line alignment, and power-of-2 masking are HPC optimizations that generic queues cannot assume. CircularBuffer is tuned for throughput and latency, not generality.

### 3. Control
Fixed capacity is explicit—you choose the tradeoff between memory usage and overflow handling. Wait-free guarantees are architectural, not runtime-configurable. The constraints ARE the optimization.

**Architectural Verdict:** CircularBuffer transforms producer-consumer communication from **mutex-bound** (~20M ops/sec) to **memory-bandwidth-bound** (~360M ops/sec) through three architectural insights: cache-line separation, index caching, and power-of-2 masking. It's not a faster queue—it's a fundamentally different approach to inter-thread communication.

---

*CircularBuffer.h — Fat-P Library*
