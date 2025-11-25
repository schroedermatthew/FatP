# CircularBuffer User Manual

**Version:** 1.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [API Reference](#api-reference)
4. [Thread Safety](#thread-safety)
5. [Performance](#performance)
6. [Use Cases](#use-cases)
7. [Best Practices](#best-practices)

---

## Overview

CircularBuffer (ring buffer) is a lock-free, single-producer single-consumer (SPSC) fixed-capacity queue. It provides wait-free push/pop operations with excellent cache performance.

### Key Features

- **Lock-free SPSC**: One producer, one consumer, no locks
- **Wait-free**: Operations complete in bounded time
- **Cache-optimized**: Aligned indices prevent false sharing
- **Fixed capacity**: Compile-time size, no allocations after construction

### Include

```cpp
#include "CircularBuffer.h"
using namespace fat_p;
```

---

## Quick Start

### Basic Usage

```cpp
// Create buffer with capacity 1024
CircularBuffer<int, 1024> buffer;

// Producer pushes
buffer.push(42);
buffer.push(100);

// Consumer pops
int value;
if (buffer.pop(value)) {
    std::cout << value;  // 42
}
```

### Producer-Consumer Pattern

```cpp
CircularBuffer<Message, 256> queue;

// Producer thread
void producer() {
    while (running) {
        Message msg = create_message();
        while (!queue.push(msg)) {
            std::this_thread::yield();  // Buffer full, wait
        }
    }
}

// Consumer thread
void consumer() {
    while (running) {
        Message msg;
        if (queue.pop(msg)) {
            process(msg);
        } else {
            std::this_thread::yield();  // Buffer empty, wait
        }
    }
}
```

---

## API Reference

### Template Parameters

```cpp
template <typename T, size_t Capacity>
class CircularBuffer;
```

- `T`: Element type (must be movable)
- `Capacity`: Maximum number of elements

### Constructor

```cpp
CircularBuffer();  // Creates empty buffer
```

### Push Operations

```cpp
// Push by copy (returns false if full)
bool push(const T& value);

// Push by move (returns false if full)
bool push(T&& value);
```

### Pop Operation

```cpp
// Pop into output parameter (returns false if empty)
bool pop(T& value);
```

### Observers

```cpp
// Current number of elements
size_t size() const;

// Check if empty
bool empty() const;

// Check if full
bool full() const;

// Maximum capacity
static constexpr size_t capacity();
```

### Usage Examples

```cpp
CircularBuffer<int, 16> buf;

// Push
bool ok1 = buf.push(1);      // true
bool ok2 = buf.push(2);      // true

// Check state
std::cout << buf.size();     // 2
std::cout << buf.empty();    // false
std::cout << buf.full();     // false
std::cout << buf.capacity(); // 16

// Pop
int val;
bool ok3 = buf.pop(val);     // true, val = 1
bool ok4 = buf.pop(val);     // true, val = 2
bool ok5 = buf.pop(val);     // false (empty)
```

---

## Thread Safety

### SPSC Guarantee

The buffer is designed for **exactly one producer thread** and **exactly one consumer thread**:

```cpp
// ✅ SAFE: One producer, one consumer
std::thread producer([&] {
    for (int i = 0; i < 1000; ++i) {
        while (!buffer.push(i)) { /* spin */ }
    }
});

std::thread consumer([&] {
    for (int i = 0; i < 1000; ++i) {
        int val;
        while (!buffer.pop(val)) { /* spin */ }
    }
});
```

```cpp
// ❌ UNSAFE: Multiple producers
std::thread p1([&] { buffer.push(1); });
std::thread p2([&] { buffer.push(2); });  // Race condition!

// ❌ UNSAFE: Multiple consumers
std::thread c1([&] { int v; buffer.pop(v); });
std::thread c2([&] { int v; buffer.pop(v); });  // Race condition!
```

### Memory Ordering

The implementation uses proper memory ordering:
- Producer: `release` on write index
- Consumer: `acquire` on read index

This ensures visibility of pushed elements to the consumer.

### Cache Line Alignment

Indices are aligned to 64-byte cache lines to prevent false sharing:

```cpp
alignas(64) std::atomic<size_t> read_idx_{0};
alignas(64) std::atomic<size_t> write_idx_{0};
```

---

## Performance

### Complexity

| Operation | Complexity |
|-----------|------------|
| push | O(1) |
| pop | O(1) |
| size | O(1) |
| empty | O(1) |
| full | O(1) |

### Benchmarks

Typical performance on modern CPUs:

| Operation | Time |
|-----------|------|
| push (uncontended) | ~10-20ns |
| pop (uncontended) | ~10-20ns |
| push + pop (SPSC) | ~30-50ns round-trip |

### Throughput

With optimal conditions:
- ~30-50 million ops/sec per direction
- ~100 million messages/sec in tight SPSC loop

---

## Use Cases

### Audio Processing

```cpp
CircularBuffer<AudioSample, 4096> audio_queue;

// Audio capture thread
void capture_callback(AudioSample* samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        audio_queue.push(samples[i]);
    }
}

// Processing thread
void process_audio() {
    AudioSample sample;
    while (audio_queue.pop(sample)) {
        apply_filter(sample);
        output(sample);
    }
}
```

### Logging

```cpp
CircularBuffer<LogEntry, 1024> log_queue;

// Application threads (producer)
void log(Level level, std::string msg) {
    LogEntry entry{level, std::move(msg), now()};
    if (!log_queue.push(std::move(entry))) {
        // Buffer full - drop or block
    }
}

// Log writer thread (consumer)
void log_writer() {
    LogEntry entry;
    while (running) {
        if (log_queue.pop(entry)) {
            write_to_file(entry);
        }
    }
}
```

### Event Queue

```cpp
CircularBuffer<Event, 256> event_queue;

// Event generator
void on_input(InputEvent e) {
    event_queue.push(Event{e});
}

// Event processor
void update() {
    Event e;
    while (event_queue.pop(e)) {
        dispatch(e);
    }
}
```

---

## Best Practices

### Do

```cpp
// ✅ Size appropriately for workload
CircularBuffer<Data, 1024> buf;  // Power of 2 often optimal

// ✅ Handle full/empty gracefully
if (!buffer.push(value)) {
    // Handle backpressure
    drop_oldest();
    // or wait
    // or grow a secondary buffer
}

// ✅ Use move semantics for large objects
buffer.push(std::move(large_object));

// ✅ Batch operations when possible
void flush_batch(std::vector<Item>& batch) {
    for (auto& item : batch) {
        while (!queue.push(std::move(item))) {
            std::this_thread::yield();
        }
    }
}
```

### Don't

```cpp
// ❌ Don't use with multiple producers
// Use a proper MPSC queue instead

// ❌ Don't use with multiple consumers
// Use a proper SPMC or MPMC queue instead

// ❌ Don't busy-spin without yielding
while (!buffer.pop(val)) { }  // Wastes CPU!

// Better:
while (!buffer.pop(val)) {
    std::this_thread::yield();  // Be nice to scheduler
}

// ❌ Don't assume size() is exact in concurrent use
// size() is approximate when producer/consumer are active
```

### Capacity Selection

```cpp
// Too small: frequent full condition
CircularBuffer<Data, 8> too_small;  // Producer blocks often

// Too large: wasted memory, cache misses
CircularBuffer<Data, 1000000> too_large;  // 1M elements excessive

// Right-sized: balance latency and throughput
CircularBuffer<Data, 1024> good;  // Handles bursts, fits in cache
```

---

## Comparison with Alternatives

| Container | Thread Safety | Capacity | Performance |
|-----------|---------------|----------|-------------|
| CircularBuffer | SPSC | Fixed | Excellent |
| LockFreeQueue | MPMC | Dynamic | Good |
| std::queue + mutex | Any | Dynamic | Moderate |
| std::deque | None | Dynamic | Good (single-threaded) |

Choose CircularBuffer when:
- Exactly one producer and one consumer
- Fixed maximum capacity is acceptable
- Maximum performance required

---

## Related Components

- **LockFreeQueue.h**: Multi-producer, multi-consumer queue
- **LockFreeRingBuffer.h**: Similar but with different API
- **ConcurrencyPolicies.h**: Synchronization utilities

---

**Document Version:** 1.0  
**Last Updated:** November 2025
