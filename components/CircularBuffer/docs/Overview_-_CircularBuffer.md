---
doc_id: OV-CIRCULARBUFFER-001
doc_type: "Overview"
title: "CircularBuffer"
fatp_components: ["CircularBuffer"]
topics: ["SPSC queue", "lock-free communication", "wait-free operations", "ring buffer", "index caching", "cache-line alignment", "producer-consumer pattern"]
constraints: ["mutex contention in hot paths", "false sharing between cores", "modulo operation overhead", "cache coherency traffic", "bounded completion time"]
cxx_standard: "C++17"
std_equivalent: null
boost_equivalent: "Boost.Circular_buffer"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-18"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# Overview - CircularBuffer

*Fat-P Library — January 2026*

---

## Executive Summary

CircularBuffer is a **wait-free single-producer single-consumer (SPSC) queue** that achieves 300-360 million operations per second through three critical optimizations: cache-line alignment preventing false sharing, index caching reducing coherency traffic by ~70%, and power-of-2 masking eliminating modulo operations. Unlike mutex-based queues (~10-20M ops/sec) or naive atomic implementations, CircularBuffer provides **bounded completion time guarantees** essential for real-time systems.

---

## Overview Card

**Component:** CircularBuffer  
**Problem solved:** Inter-thread communication overhead and latency unpredictability in single-producer single-consumer scenarios  
**When to use:** Real-time audio callbacks, trading message passing, sensor data streaming, async logging—anywhere one thread produces and one consumes with strict latency requirements  
**When NOT to use:** Multiple producers or consumers; unbounded queue growth needed; blocking semantics required  
**Key guarantee:** Wait-free—every push/pop completes in bounded time regardless of other thread's state  
**std equivalent:** None. No standard equivalent exists or is planned.  
**Boost equivalent:** `boost::circular_buffer` (similar concept, different threading model)  
**Other alternatives:** moodycamel::ReaderWriterQueue, boost::lockfree::spsc_queue  
**Read next:** User Manual - CircularBuffer, Companion Guide - CircularBuffer

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The mutex trap: latency spikes under contention
template<typename T>
class NaiveQueue {
    std::queue<T> queue_;
    std::mutex mutex_;
    
public:
    void push(T value) {
        std::lock_guard lock(mutex_);  // Can block for 50-500ns
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

| Issue | Real-Time Impact |
|-------|------------------|
| Mutex contention | 50-500ns blocking destroys 20μs audio budget |
| Priority inversion | Real-time thread blocked by lower-priority lock holder |
| Unbounded growth | Memory exhaustion under sustained load |
| Cache line ping-pong | Producer/consumer indices on same cache line = 100ns penalty |
| No completion bound | Mutex acquisition time is unbounded |

### The Standard's Limitation

The C++ standard provides no lock-free SPSC queue. `std::queue` requires external synchronization. `std::atomic` provides primitives but not containers.

**Benchmark comparison:** CircularBuffer's lock-free, cache-line-separated design dramatically outperforms `std::queue + mutex` because it eliminates mutex acquisition, cache line ping-pong, and unbounded wait times. See `components/CircularBuffer/results/` for current data.

---

## Architecture: Cache-Optimized Wait-Free Design

### Three Critical Optimizations

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
    
    alignas(64) std::unique_ptr<T[]> buffer_;
};
```

| Optimization | Without | With | Improvement |
|--------------|---------|------|-------------|
| Power-of-2 masking | `idx % capacity` (~20 cycles) | `idx & mask` (~1 cycle) | 20× per operation |
| Cache-line alignment | False sharing (~100ns) | No false sharing (~3ns) | 30× latency |
| Index caching | Atomic load every op | Atomic load on cache miss | 1.5-1.7× throughput |

### Memory Ordering

```cpp
bool push(T value) {
    size_t write = write_idx_.load(std::memory_order_relaxed);
    size_t next = (write + 1) & kMask;
    
    if (next == cached_read_idx_) {
        cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
        if (next == cached_read_idx_) return false;
    }
    
    buffer_[write] = std::move(value);
    write_idx_.store(next, std::memory_order_release);
    return true;
}
```

---

## Feature Inventory

### 1. Wait-Free Push/Pop

```cpp
CircularBuffer<Event, 1024> events;

// Producer thread - never blocks
while (running) {
    if (!events.push(std::move(e))) handle_overflow();
}

// Consumer thread - never blocks
while (running) {
    if (auto e = events.pop()) process(*e);
}
```

### 2. In-Place Construction via emplace()

```cpp
CircularBuffer<LargeObject, 256> objects;
objects.emplace(arg1, arg2, arg3);  // No temporary
```

### 3. Non-Consuming Peek via front()

```cpp
if (auto* ptr = buffer.front()) {
    if (ptr->priority > threshold) {
        auto value = buffer.pop();
    }
}
```

### 4. Index Caching

50-70% reduction in cache coherency traffic, 1.5-1.7× throughput improvement.

---

## Why Not Alternatives?

| Aspect | boost::spsc | moodycamel (blocking) | Fat-P CircularBuffer |
|--------|-------------|----------------------|----------------------|
| Index caching | No | No | Yes |
| emplace() | No | No | Yes |
| front() peek | No | No | Yes |
| Dependencies | Boost | Header-only | None |
| Single-threaded | Fastest (sub-ns) | Slower (mutex overhead) | **Fastest (sub-ns)** |
| SPSC throughput | Good | Slower (cache invalidation) | **Best (cached indices)** |

---

## Performance Characteristics

CircularBuffer's wait-free design with cache-line-separated indices and cached counters provides sub-nanosecond single-threaded operations and significantly higher SPSC throughput than mutex-based alternatives. The three optimizations (power-of-2 masking, cache-line alignment, index caching) combine to minimize both instruction count and coherency traffic.

See `components/CircularBuffer/results/` and `benchmark_results/` for current platform-specific data.

### Where Fat-P Wins

- Audio callbacks (10μs budget)
- Trading systems (microsecond latency)
- Real-time control (deterministic timing)
- High-throughput logging

### Where Fat-P Loses

- Multiple producers → Use LockFreeQueue
- Multiple consumers → Use LockFreeQueue
- Unbounded capacity → Use mutex+deque
- Blocking semantics → Use condition variables

---

## Final Assessment

CircularBuffer transforms producer-consumer communication from **mutex-bound** (~25M ops/sec) to **memory-bandwidth-bound** (~360M ops/sec) through cache-line separation, index caching, and power-of-2 masking.

---

*CircularBuffer.h — Fat-P Library*
