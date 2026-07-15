---
doc_id: UM-CIRCULARBUFFER-001
doc_type: "User Manual"
title: "CircularBuffer"
fatp_components: ["CircularBuffer"]
topics: ["SPSC queue", "lock-free communication", "wait-free operations", "ring buffer", "index caching", "producer-consumer pattern"]
constraints: ["mutex contention", "false sharing", "cache coherency traffic", "bounded completion time"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.Circular_buffer"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-18"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# User Manual - CircularBuffer

*Updated January 2026*

---



**Scope:** Complete usage guide for `fat_p::CircularBuffer<T, N>`: fixed-capacity ring buffer, push/pop operations, non-consuming peek via front(), full/empty detection, and reject-on-full behavior.

**Not covered:**
- Dynamically-sized ring buffers
- Lock-free ring buffers (see LockFreeContainers)
- Concurrent producer/consumer queues (see WorkQueue)

**Prerequisites:** C++20; understanding of ring buffer concepts (circular indexing, head/tail pointers)

---

## User Manual Card

**Component:** CircularBuffer
**Primary use case:** Fixed-capacity wait-free SPSC FIFO buffer with O(1) push/pop; push returns false when full (no overwrite)
**Integration pattern:** Construct `CircularBuffer<T, N>`, producer thread calls `push()`/`emplace()`, consumer thread calls `pop(T&)`/`front()`; check the bool results for full/empty
**Key API:** `CircularBuffer<T, N>`, `.push()`, `.emplace()`, `.pop(T&)`, `.front()`, `.full()`, `.empty()`, `.size()`
**std equivalent:** None
**Common mistakes:** Assuming push overwrites when full (it returns false---check the result); ignoring the `[[nodiscard]]` bool returns; using CircularBuffer for unbounded queues (use WorkQueue instead)
**Performance notes:** All operations are O(1). Contiguous storage enables cache-friendly iteration. See `components/CircularBuffer/results/` for current data

---
## Table of Contents

1. [Getting Started](#getting-started)
2. [Core Concepts](#core-concepts)
3. [Push Operations](#push-operations)
4. [Pop Operations](#pop-operations)
5. [The Peek Pattern](#the-peek-pattern)
6. [Capacity Selection](#capacity-selection)
7. [Thread Safety Model](#thread-safety-model)
8. [Performance Characteristics](#performance-characteristics)
9. [Common Patterns](#common-patterns)
10. [Migration Guide](#migration-guide)
11. [Troubleshooting](#troubleshooting)
12. [API Reference](#api-reference)

---

## Getting Started

### Prerequisites

| Requirement | Version |
|-------------|---------|
| C++ Standard | C++20 or later |
| Compiler | GCC 7+, Clang 5+, MSVC 2017+ |
| Dependencies | None (header-only) |

### Integration

```cpp
#include "CircularBuffer.h"
```

### First Program

```cpp
#include <iostream>
#include <thread>
#include "CircularBuffer.h"

int main() {
    fat_p::CircularBuffer<int, 1023> buffer;
    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (int i = 0; i < 1000; ++i) {
            while (!buffer.push(i)) std::this_thread::yield();
        }
        done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        int sum = 0;
        while (!done.load(std::memory_order_acquire) || !buffer.empty()) {
            if (int val = 0; buffer.pop(val)) sum += val;
        }
        std::cout << "Sum: " << sum << "\n";  // 499500
    });

    producer.join();
    consumer.join();
}
```

---

## Core Concepts

### The SPSC Contract

CircularBuffer is **strictly single-producer single-consumer**. Exactly one thread may call push operations, exactly one thread may call pop operations.

```cpp
// CORRECT
std::thread producer([&]() { buffer.push(data); });
std::thread consumer([&]() { T v; buffer.pop(v); });

// WRONG - undefined behavior
std::thread p1([&]() { buffer.push(1); });
std::thread p2([&]() { buffer.push(2); });  // Multiple producers = UB
```

### Ring Buffer Design

```
Capacity = 8, 4 elements stored
┌─┬─┬─┬─┬─┬─┬─┬─┐
│ │█│█│█│█│ │ │ │
└─┴─┴─┴─┴─┴─┴─┴─┘
   ↑       ↑
   R       W
```

- `push()` writes at W, advances W
- `pop()` reads at R, advances R
- Full when `(W + 1) & mask == R`
- Empty when `R == W`

### Power-of-2 Optimization

Internal buffer size is always power of 2 for fast modulo:

```cpp
next = (idx + 1) & mask;  // 1 cycle, not 20
```

| User Capacity | Internal Size | Overhead |
|---------------|---------------|----------|
| 7 | 8 | 14% |
| 15 | 16 | 7% |
| 63 | 64 | 2% |
| 1023 | 1024 | 0.1% |

**Recommendation:** Use `2^N - 1` capacities (7, 15, 31, 63, 127, 255, 511, 1023).

---

## Push Operations

### push() - Move/Copy Insert

```cpp
bool push(T value);
bool push(const T& value);
```

Returns `true` on success, `false` if full.

```cpp
CircularBuffer<std::string, 100> msgs;

std::string s = "Hello";
if (!msgs.push(std::move(s))) {
    handle_overflow();
}
```

### emplace() - In-Place Construction

```cpp
template<typename... Args>
bool emplace(Args&&... args);
```

Constructs directly in buffer, no temporary.

```cpp
struct Event {
    uint64_t ts;
    int type;
    char data[256];
};

CircularBuffer<Event, 1024> events;
events.emplace(get_time(), EVENT_LOG, "message");
```

---

## Pop Operations

### pop(T&) - The Only Pop

```cpp
[[nodiscard]] bool pop(T& out);
```

`pop()` moves the front element into `out` and returns `true`, or returns `false` if the buffer is empty. There is no `std::optional`-returning overload; the output-parameter form lets the consumer reuse one object across iterations:

```cpp
Message msg;
while (running) {
    if (inbox.pop(msg)) {
        process(msg);
    } else {
        std::this_thread::yield();
    }
}
```

It is equally useful for avoiding per-pop construction of large objects:

```cpp
LargeStruct item;
while (data.pop(item)) {
    process(item);
}
```

---

## The Peek Pattern

### front() - Non-Consuming Access

```cpp
const T* front() const;
```

Inspect without removing:

```cpp
if (const auto* task = tasks.front()) {
    if (task->priority >= HIGH) {
        Task t;
        if (tasks.pop(t)) execute(t);
    }
}
```

### Priority Filtering

```cpp
Message m;
while (const auto* msg = buffer.front()) {
    if (msg->timestamp > deadline) break;
    if (buffer.pop(m)) process(m);
}
```

---

## Capacity Selection

### Sizing Guidelines

**Audio (48kHz, 10ms buffer):**
```cpp
CircularBuffer<Sample, 511> audio;  // 480 samples needed
```

**Logging (burst of 1000):**
```cpp
CircularBuffer<LogEntry, 1023> logs;
```

**Trading (low latency):**
```cpp
CircularBuffer<Order, 63> orders;  // Small = fast
```

### Memory Layout

```cpp
alignas(64) std::atomic<size_t> write_idx_;   // 64 bytes
alignas(64) std::atomic<size_t> read_idx_;    // 64 bytes
alignas(64) size_t cached_read_idx_;          // 64 bytes
alignas(64) size_t cached_write_idx_;         // 64 bytes
alignas(64) std::unique_ptr<T[]> buffer_;     // 64 bytes + N*sizeof(T)
```

Total overhead: ~320 bytes + buffer storage.

---

## Thread Safety Model

### Memory Ordering

```cpp
// Producer
buffer_[write] = std::move(value);
write_idx_.store(next, memory_order_release);  // Publish

// Consumer
size_t w = write_idx_.load(memory_order_acquire);  // Synchronize
T val = std::move(buffer_[read]);
```

Release-acquire ensures consumer sees producer's writes.

### Safe Patterns

```cpp
// One producer thread
void producer() {
    buffer.push(data);
    buffer.emplace(args...);
}

// One consumer thread
void consumer() {
    T value;
    buffer.pop(value);
    buffer.front();
    buffer.empty();
    buffer.size();
}
```

### Unsafe Patterns

```cpp
// Multiple producers - UNDEFINED BEHAVIOR
std::thread([&] { buffer.push(1); });
std::thread([&] { buffer.push(2); });

// Multiple consumers - UNDEFINED BEHAVIOR
std::thread([&] { int v; buffer.pop(v); });
std::thread([&] { int v; buffer.pop(v); });
```

---

## Performance Characteristics

### Benchmark Results

| Operation | CircularBuffer | boost::spsc | mutex+deque |
|-----------|---------------|-------------|-------------|
| Single-threaded | 0.98 ns | 1.25 ns | 25.22 ns |
| SPSC throughput | 62.5 ns | 82.5 ns | 335.9 ns |
| Burst pattern | 0.88 ns | 0.88 ns | 3.67 ns |

### Index Caching Impact

| Buffer State | Cache Hit Rate | Speedup |
|--------------|---------------|---------|
| Mostly empty | Low | 1.2× |
| 25-75% full | High | 1.7× |
| Mostly full | Low | 1.2× |

---

## Common Patterns

### Producer-Consumer Loop

```cpp
CircularBuffer<Work, 1023> queue;
std::atomic<bool> shutdown{false};

// Producer
while (!shutdown) {
    Work w = get_work();
    while (!queue.push(std::move(w))) {
        if (shutdown) return;
        std::this_thread::yield();
    }
}

// Consumer
Work w;
while (!shutdown || !queue.empty()) {
    if (queue.pop(w)) {
        process(w);
    } else {
        std::this_thread::yield();
    }
}
```

### Graceful Shutdown

```cpp
void shutdown() {
    shutdown_flag.store(true);
    producer_thread.join();
    // Consumer drains remaining items
    consumer_thread.join();
}
```

### Overflow Handling

```cpp
if (!buffer.push(data)) {
    // Option 1: Drop
    log_warning("Dropped message");
    
    // Option 2: Block
    while (!buffer.push(data)) yield();
    
    // Option 3: Back-pressure
    slow_down_producer();
}
```

---

## Migration Guide

### From std::queue + mutex

**Before:**
```cpp
std::queue<Message> queue;
std::mutex mtx;

void produce() {
    std::lock_guard lock(mtx);
    queue.push(msg);
}

void consume() {
    std::lock_guard lock(mtx);
    if (!queue.empty()) {
        auto m = queue.front();
        queue.pop();
        process(m);
    }
}
```

**After:**
```cpp
fat_p::CircularBuffer<Message, 1023> queue;

void produce() {
    if (!queue.push(msg)) handle_overflow();
}

void consume() {
    Message m;
    if (queue.pop(m)) process(m);
}
```

### From boost::lockfree::spsc_queue

| boost | fat_p | Notes |
|-------|-------|-------|
| `push(val)` | `push(val)` | Same |
| `pop(val&)` | `pop(val&)` | Same |
| `pop()` | — | Use `pop(val&)` |
| — | `emplace()` | Fat-P only |
| — | `front()` | Fat-P only |

---

## Troubleshooting

### Data Corruption

**Cause:** Multiple producers or consumers.

**Fix:** Audit all call sites. Use ThreadSanitizer:
```bash
g++ -fsanitize=thread -g program.cpp
```

### Always Empty/Full

**Cause:** Thread not running, early shutdown.

**Fix:** Add counters to verify both threads operate.

### High CPU When Idle

**Cause:** Spin loop without yield.

**Fix:**
```cpp
while (!buffer.pop(val)) {
    std::this_thread::yield();
}
```

### Resources Not Freed

**Cause:** For trivially destructible types, `clear()` just resets indices---there are no destructors to run. For non-trivial types (e.g., `shared_ptr`, containers), `clear()` automatically calls `clearAndDestruct()`, so resources are in fact released.

**Fix:** To make destruction explicit regardless of element type, call `clearAndDestruct()` directly:
```cpp
buffer.clearAndDestruct();
```

---

## API Reference

### Construction

| Signature | Description |
|-----------|-------------|
| `CircularBuffer()` | Default construct empty buffer |

### Push Operations

| Method | Returns | Description |
|--------|---------|-------------|
| `push(T)` | `bool` | Insert by move/copy |
| `push(const T&)` | `bool` | Insert by copy |
| `emplace(Args...)` | `bool` | Construct in-place |

### Pop Operations

| Method | Returns | Description |
|--------|---------|-------------|
| `pop(T&)` | `bool` | Move front into reference; false if empty |

### Observers

| Method | Returns | Description |
|--------|---------|-------------|
| `front()` | `const T*` | Pointer to front (nullptr if empty) |
| `empty()` | `bool` | True if no elements |
| `full()` | `bool` | True if at capacity |
| `size()` | `size_t` | Approximate element count |
| `capacity()` | `size_t` | Maximum elements |

### Modifiers

| Method | Description |
|--------|-------------|
| `clear()` | Reset to empty; automatically destroys elements of non-trivially-destructible types |
| `clearAndDestruct()` | Pop and destroy every element, then reset indices |

---

## Summary

**CircularBuffer** is a wait-free SPSC ring buffer for maximum throughput inter-thread communication.

**Key Features:**
- Wait-free operations with bounded completion
- Index caching for 1.5-1.7× throughput boost
- emplace() and front() APIs
- Zero dependencies

**Performance:** 0.98 ns single-threaded, 62.5 ns SPSC throughput

**Use when:** Single producer, single consumer, latency-critical

**Don't use when:** Multiple producers/consumers, need unbounded growth

---

*CircularBuffer.h — Fat-P Library*
