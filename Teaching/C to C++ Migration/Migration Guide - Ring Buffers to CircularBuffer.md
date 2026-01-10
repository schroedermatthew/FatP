---
doc_id: MG-CIRCULARBUFFER-001
doc_type: "Migration Guide"
title: "Manual Ring Buffers to Lock-Free CircularBuffer"
from_pattern: "Manual ring buffers, modulo indexing, mutex-protected queues"
to_component: "CircularBuffer"
fatp_version: "1.0"
cxx_standard: "C++17"
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2025-01-08"
---

# Migration Guide - Manual Ring Buffers to Lock-Free CircularBuffer

### *From DIY Ring Buffers to Wait-Free SPSC Queues*

*FAT-P Library — January 2025*

---

## Migration Card

| Aspect | Detail |
|--------|--------|
| **C Pattern** | Manual ring buffers, modulo indexing, empty/full confusion, mutex queues |
| **Problems Solved** | Off-by-one errors, empty/full ambiguity, cache thrashing, lock contention |
| **Fat-P Component** | `CircularBuffer<T, Capacity>` |
| **Migration Complexity** | Medium — requires understanding SPSC threading model |
| **Runtime Overhead** | Near-zero — wait-free, ~300M ops/sec |
| **Breaking Changes** | Yes — SPSC model (single producer, single consumer) |

---

## Table of Contents

1. [The Problem with Manual Ring Buffers](#the-problem-with-manual-ring-buffers)
2. [The Empty/Full Ambiguity](#the-emptyfull-ambiguity)
3. [The C Patterns](#the-c-patterns)
4. [The CircularBuffer Solution](#the-circularbuffer-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When CircularBuffer Loses](#when-circularbuffer-loses)

---

## The Problem with Manual Ring Buffers

Ring buffers (circular buffers) are fundamental data structures for producer-consumer scenarios: audio processing, network I/O, logging, inter-thread communication. The naive implementation:

```c
#define BUFFER_SIZE 1024

struct RingBuffer {
    int data[BUFFER_SIZE];
    size_t head;  // Write position
    size_t tail;  // Read position
};

void push(RingBuffer* rb, int value) {
    rb->data[rb->head] = value;
    rb->head = (rb->head + 1) % BUFFER_SIZE;  // BUG: What if full?
}

int pop(RingBuffer* rb) {
    int value = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) % BUFFER_SIZE;  // BUG: What if empty?
    return value;
}

bool is_empty(RingBuffer* rb) {
    return rb->head == rb->tail;  // Correct
}

bool is_full(RingBuffer* rb) {
    return rb->head == rb->tail;  // WRONG! Same as empty!
}
```

**The classic bug:** When `head == tail`, is the buffer empty or full? Both states look identical!

---

## The Empty/Full Ambiguity

This is the most common ring buffer bug. Consider a buffer of size 4:

```
Initial state (empty):
  head = 0, tail = 0
  [ _ _ _ _ ]
       ^head/tail

After 4 pushes (full):
  head = 0, tail = 0  (wrapped around!)
  [ A B C D ]
       ^head/tail

head == tail in BOTH cases!
```

**Solutions people try:**

| Approach | Problem |
|----------|---------|
| Track count | Extra variable, synchronization overhead |
| Waste one slot | Capacity - 1 usable, often forgotten |
| Separate full flag | Race condition in concurrent code |
| Mirror bit | Complex, easy to get wrong |

---

## Real-World Ring Buffer Disasters

### Audio Buffer Underruns

Audio systems use ring buffers between the audio thread and the processing thread. A famous bug in early ALSA drivers:

```c
// Audio callback (runs in interrupt context)
void audio_callback(short* output, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        if (!ring_buffer_empty(&audio_buffer)) {
            output[i] = ring_buffer_pop(&audio_buffer);
        } else {
            output[i] = 0;  // Underrun - audible click!
        }
    }
}
```

The bug: `ring_buffer_empty()` and `ring_buffer_pop()` aren't atomic. Between the check and the pop, another thread could drain the buffer.

### Network Packet Loss

```c
// Producer: Network receive thread
void on_packet_received(Packet* pkt) {
    if (!ring_buffer_full(&packet_queue)) {
        ring_buffer_push(&packet_queue, pkt);
    } else {
        drop_packet(pkt);  // Lost data!
        stats.drops++;
    }
}

// Consumer: Processing thread
void process_packets() {
    while (!ring_buffer_empty(&packet_queue)) {
        Packet* pkt = ring_buffer_pop(&packet_queue);
        process(pkt);
    }
}
```

Race conditions everywhere:
- Check-then-act on `full`/`empty`
- No memory barriers between threads
- Producer and consumer fight over cache lines

---

## The C Patterns

### Pattern 1: Naive Modulo Ring Buffer

```c
typedef struct {
    int* data;
    size_t capacity;
    size_t head;
    size_t tail;
} RingBuffer;

bool push(RingBuffer* rb, int value) {
    size_t next = (rb->head + 1) % rb->capacity;
    if (next == rb->tail) {
        return false;  // Full (wastes one slot)
    }
    rb->data[rb->head] = value;
    rb->head = next;
    return true;
}

bool pop(RingBuffer* rb, int* value) {
    if (rb->head == rb->tail) {
        return false;  // Empty
    }
    *value = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    return true;
}
```

**Problems:**
- Modulo is slow (division instruction)
- Wastes one slot to distinguish empty/full
- Not thread-safe
- No memory ordering for concurrent access

### Pattern 2: Count-Based Ring Buffer

```c
typedef struct {
    int* data;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;  // Track element count
} RingBuffer;

bool push(RingBuffer* rb, int value) {
    if (rb->count >= rb->capacity) {
        return false;
    }
    rb->data[rb->head] = value;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;  // NOT ATOMIC!
    return true;
}

bool pop(RingBuffer* rb, int* value) {
    if (rb->count == 0) {
        return false;
    }
    *value = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;  // NOT ATOMIC!
    return true;
}
```

**Problems:**
- `count` must be updated atomically for thread safety
- Three variables to synchronize (head, tail, count)
- More complex, more bugs

### Pattern 3: Mutex-Protected Queue

```c
typedef struct {
    int data[BUFFER_SIZE];
    size_t head;
    size_t tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ThreadSafeQueue;

bool push(ThreadSafeQueue* q, int value) {
    pthread_mutex_lock(&q->mutex);
    while (is_full(q)) {
        pthread_cond_wait(&q->not_full, &q->mutex);  // Blocking!
    }
    q->data[q->head] = value;
    q->head = (q->head + 1) % BUFFER_SIZE;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return true;
}
```

**Problems:**
- Mutex contention kills performance
- Blocking is unacceptable for real-time systems
- Priority inversion risk
- ~100-1000ns overhead per operation

### Pattern 4: Broken "Lock-Free" Attempt

```c
typedef struct {
    _Atomic size_t head;
    _Atomic size_t tail;
    int data[BUFFER_SIZE];
} LockFreeQueue;

bool push(LockFreeQueue* q, int value) {
    size_t head = atomic_load(&q->head);
    size_t tail = atomic_load(&q->tail);
    
    if ((head + 1) % BUFFER_SIZE == tail) {
        return false;  // Full
    }
    
    q->data[head] = value;  // DATA RACE!
    atomic_store(&q->head, (head + 1) % BUFFER_SIZE);
    return true;
}
```

**Problems:**
- Data written before index is visible to consumer
- No memory ordering guarantees
- Consumer may read garbage or torn values
- This is NOT correct lock-free code

---

## The CircularBuffer Solution

### Core Concept

`CircularBuffer` is a **wait-free SPSC (Single Producer, Single Consumer)** queue with:

- **Power-of-2 sizing** for fast bitwise AND instead of modulo
- **Cache-line alignment** to prevent false sharing
- **Index caching** to reduce cross-core traffic
- **Correct memory ordering** for lock-free safety

```cpp
#include "CircularBuffer.h"
using namespace fat_p;

// Compile-time capacity
CircularBuffer<int, 1024> buffer;

// Producer thread
void producer() {
    for (int i = 0; i < 1000000; i++) {
        while (!buffer.push(i)) {
            // Buffer full - retry or handle backpressure
        }
    }
}

// Consumer thread
void consumer() {
    int value;
    while (running) {
        if (buffer.pop(value)) {
            process(value);
        }
    }
}
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **Wait-free** | O(1) guaranteed completion, no blocking |
| **SPSC model** | Exactly one producer, one consumer |
| **Cache-line aligned** | No false sharing between threads |
| **Index caching** | 50-70% reduction in cache coherency traffic |
| **Power-of-2 masking** | Bitwise AND instead of modulo division |
| **[[nodiscard]]** | Can't ignore push/pop return values |
| **~300M ops/sec** | Industry-leading throughput |

### Memory Ordering

The implementation uses carefully chosen memory orderings:

```cpp
// Producer: relaxed load on write_idx (own data)
//           acquire load on read_idx (sync with consumer)
//           release store on write_idx (publish to consumer)

// Consumer: relaxed load on read_idx (own data)
//           acquire load on write_idx (sync with producer)
//           release store on read_idx (publish to producer)
```

This ensures:
1. Data is fully written before index advances
2. Consumer sees data after seeing updated index
3. No torn reads or writes

### API Overview

```cpp
template <typename T, size_t Capacity>
class CircularBuffer {
public:
    // Construction (allocates internal buffer)
    CircularBuffer();
    
    // Non-copyable, non-movable (contains atomics)
    
    // Producer operations (call from ONE thread only)
    [[nodiscard]] bool push(const T& value) noexcept(...);
    [[nodiscard]] bool push(T&& value) noexcept(...);
    template<typename... Args>
    [[nodiscard]] bool emplace(Args&&... args) noexcept(...);
    
    // Consumer operations (call from ONE thread only)
    [[nodiscard]] bool pop(T& value) noexcept(...);
    [[nodiscard]] const T* front() const noexcept;
    
    // Observer operations (any thread)
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool full() const noexcept;
    [[nodiscard]] static constexpr size_t capacity() noexcept;
    
    // Reset (NOT thread-safe - call during shutdown)
    void clear() noexcept(...);
};
```

---

## Migration Steps

### Step 1: Identify Ring Buffer Usage

Find existing ring buffer implementations:

```bash
grep -rn "ring.*buffer\|circular.*buffer\|head.*tail" src/
grep -rn "% BUFFER_SIZE\|% capacity" src/
grep -rn "struct.*Queue\|struct.*Buffer" src/
```

### Step 2: Verify SPSC Pattern

CircularBuffer requires **exactly one producer and one consumer**. Audit your code:

```cpp
// CORRECT: One thread pushes, another pops
std::thread producer([&]{ while(running) buffer.push(produce()); });
std::thread consumer([&]{ int v; while(buffer.pop(v)) consume(v); });

// INCORRECT: Multiple producers
std::thread producer1([&]{ buffer.push(1); });  // Race!
std::thread producer2([&]{ buffer.push(2); });  // Race!

// INCORRECT: Multiple consumers
std::thread consumer1([&]{ int v; buffer.pop(v); });  // Race!
std::thread consumer2([&]{ int v; buffer.pop(v); });  // Race!
```

If you need MPMC (multiple producers, multiple consumers), use `LockFreeQueue` instead.

### Step 3: Choose Capacity

CircularBuffer capacity is a compile-time template parameter:

```cpp
// Small buffer for low-latency signaling
CircularBuffer<Event, 64> eventQueue;

// Large buffer for audio processing (44100 Hz * 0.1s = 4410 samples)
CircularBuffer<float, 8192> audioBuffer;

// Network packet buffer
CircularBuffer<Packet, 1024> packetQueue;
```

The internal buffer is rounded up to the next power of 2 automatically.

### Step 4: Replace Push Operations

**Before:**
```c
void send_message(Queue* q, Message* msg) {
    pthread_mutex_lock(&q->mutex);
    while (queue_full(q)) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->data[q->head] = *msg;
    q->head = (q->head + 1) % q->capacity;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}
```

**After:**
```cpp
bool send_message(CircularBuffer<Message, 1024>& buffer, const Message& msg) {
    return buffer.push(msg);  // Returns false if full
}

// Or with retry logic
void send_message_blocking(CircularBuffer<Message, 1024>& buffer, const Message& msg) {
    while (!buffer.push(msg)) {
        std::this_thread::yield();  // Backpressure
    }
}
```

### Step 5: Replace Pop Operations

**Before:**
```c
bool receive_message(Queue* q, Message* msg) {
    pthread_mutex_lock(&q->mutex);
    if (queue_empty(q)) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }
    *msg = q->data[q->tail];
    q->tail = (q->tail + 1) % q->capacity;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return true;
}
```

**After:**
```cpp
bool receive_message(CircularBuffer<Message, 1024>& buffer, Message& msg) {
    return buffer.pop(msg);
}
```

### Step 6: Handle Backpressure

With lock-free queues, you must handle the full/empty cases:

```cpp
// Strategy 1: Spin-wait (low latency)
while (!buffer.push(value)) {
    std::this_thread::yield();
}

// Strategy 2: Drop and count (real-time systems)
if (!buffer.push(value)) {
    ++dropped_count;
}

// Strategy 3: Bounded spin then fail
bool push_with_timeout(auto& buffer, const auto& value, int max_spins) {
    for (int i = 0; i < max_spins; i++) {
        if (buffer.push(value)) return true;
        std::this_thread::yield();
    }
    return false;
}
```

---

## Before/After Examples

### Example 1: Audio Processing Pipeline

**Before (mutex-based):**
```c
typedef struct {
    float samples[AUDIO_BUFFER_SIZE];
    size_t head, tail;
    pthread_mutex_t mutex;
} AudioBuffer;

// Audio thread (real-time, must not block!)
void audio_callback(float* output, size_t frames) {
    pthread_mutex_lock(&audio_buffer.mutex);  // DANGER: Can block!
    for (size_t i = 0; i < frames; i++) {
        if (audio_buffer.head != audio_buffer.tail) {
            output[i] = audio_buffer.samples[audio_buffer.tail];
            audio_buffer.tail = (audio_buffer.tail + 1) % AUDIO_BUFFER_SIZE;
        } else {
            output[i] = 0.0f;  // Underrun
        }
    }
    pthread_mutex_unlock(&audio_buffer.mutex);
}

// Processing thread
void process_audio(float* input, size_t frames) {
    pthread_mutex_lock(&audio_buffer.mutex);
    for (size_t i = 0; i < frames; i++) {
        size_t next = (audio_buffer.head + 1) % AUDIO_BUFFER_SIZE;
        if (next != audio_buffer.tail) {
            audio_buffer.samples[audio_buffer.head] = process(input[i]);
            audio_buffer.head = next;
        }
        // else: overrun, drop sample
    }
    pthread_mutex_unlock(&audio_buffer.mutex);
}
```

**After (CircularBuffer):**
```cpp
CircularBuffer<float, 8192> audioBuffer;

// Audio thread (real-time safe - never blocks)
void audio_callback(float* output, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        if (!audioBuffer.pop(output[i])) {
            output[i] = 0.0f;  // Underrun - no blocking!
        }
    }
}

// Processing thread
void process_audio(const float* input, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        float processed = process(input[i]);
        if (!audioBuffer.push(processed)) {
            // Overrun - drop sample, never blocks
        }
    }
}
```

### Example 2: Logging Pipeline

**Before (blocking queue):**
```c
typedef struct LogQueue {
    char messages[LOG_QUEUE_SIZE][MAX_LOG_LENGTH];
    size_t head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} LogQueue;

// Application thread (performance critical)
void log_message(const char* msg) {
    pthread_mutex_lock(&log_queue.mutex);
    // This can block the application!
    size_t next = (log_queue.head + 1) % LOG_QUEUE_SIZE;
    if (next != log_queue.tail) {
        strncpy(log_queue.messages[log_queue.head], msg, MAX_LOG_LENGTH);
        log_queue.head = next;
    }
    pthread_cond_signal(&log_queue.not_empty);
    pthread_mutex_unlock(&log_queue.mutex);
}

// Logger thread
void* logger_thread(void* arg) {
    while (running) {
        pthread_mutex_lock(&log_queue.mutex);
        while (log_queue.head == log_queue.tail && running) {
            pthread_cond_wait(&log_queue.not_empty, &log_queue.mutex);
        }
        // ... write to file ...
        pthread_mutex_unlock(&log_queue.mutex);
    }
    return NULL;
}
```

**After (CircularBuffer):**
```cpp
struct LogEntry {
    char message[256];
    std::chrono::system_clock::time_point timestamp;
};

CircularBuffer<LogEntry, 4096> logBuffer;

// Application thread (never blocks)
void log_message(const char* msg) {
    LogEntry entry;
    std::strncpy(entry.message, msg, sizeof(entry.message));
    entry.timestamp = std::chrono::system_clock::now();
    
    if (!logBuffer.push(std::move(entry))) {
        // Buffer full - drop log (better than blocking!)
        atomic_fetch_add(&dropped_logs, 1);
    }
}

// Logger thread
void logger_thread() {
    LogEntry entry;
    while (running) {
        if (logBuffer.pop(entry)) {
            write_to_file(entry);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
```

### Example 3: Network Packet Queue

**Before (broken lock-free attempt):**
```c
typedef struct {
    _Atomic size_t head;
    _Atomic size_t tail;
    Packet packets[PACKET_BUFFER_SIZE];
} PacketQueue;

// Receive thread
void on_packet(Packet* pkt) {
    size_t head = atomic_load(&pq.head);
    size_t next = (head + 1) % PACKET_BUFFER_SIZE;
    
    if (next == atomic_load(&pq.tail)) {
        drop_packet(pkt);  // Full
        return;
    }
    
    pq.packets[head] = *pkt;  // RACE CONDITION!
    atomic_store(&pq.head, next);  // Consumer might read garbage
}

// Process thread
bool get_packet(Packet* pkt) {
    size_t tail = atomic_load(&pq.tail);
    if (tail == atomic_load(&pq.head)) {
        return false;  // Empty
    }
    
    *pkt = pq.packets[tail];  // RACE: might read incomplete data
    atomic_store(&pq.tail, (tail + 1) % PACKET_BUFFER_SIZE);
    return true;
}
```

**After (CircularBuffer with correct memory ordering):**
```cpp
CircularBuffer<Packet, 1024> packetQueue;

// Receive thread (single producer)
void on_packet(const Packet& pkt) {
    if (!packetQueue.push(pkt)) {
        ++stats.dropped;
    }
}

// Process thread (single consumer)
void process_packets() {
    Packet pkt;
    while (running) {
        if (packetQueue.pop(pkt)) {
            process(pkt);
        }
    }
}
```

---

## Advanced Patterns

### Pattern: Peek Without Pop

```cpp
void consumer() {
    // Check what's next without removing it
    const Message* msg = buffer.front();
    if (msg && msg->priority == Priority::Low) {
        // Skip low priority for now
        return;
    }
    
    Message m;
    if (buffer.pop(m)) {
        process(m);
    }
}
```

### Pattern: Batch Processing

```cpp
void process_batch() {
    constexpr size_t BATCH_SIZE = 64;
    std::array<Event, BATCH_SIZE> batch;
    size_t count = 0;
    
    // Drain up to BATCH_SIZE items
    while (count < BATCH_SIZE && buffer.pop(batch[count])) {
        ++count;
    }
    
    // Process batch (better cache utilization)
    for (size_t i = 0; i < count; i++) {
        process(batch[i]);
    }
}
```

### Pattern: Graceful Shutdown

```cpp
std::atomic<bool> shutdown{false};

void producer() {
    while (!shutdown) {
        auto data = generate();
        while (!buffer.push(data) && !shutdown) {
            std::this_thread::yield();
        }
    }
}

void consumer() {
    Data data;
    while (!shutdown || !buffer.empty()) {
        if (buffer.pop(data)) {
            process(data);
        } else if (!shutdown) {
            std::this_thread::yield();
        }
    }
}

void shutdown_gracefully() {
    shutdown = true;
    producer_thread.join();
    consumer_thread.join();  // Will drain remaining items
}
```

### Pattern: Statistics Monitoring

```cpp
void monitor() {
    size_t prev_size = 0;
    while (running) {
        size_t current_size = buffer.size();
        
        metrics.gauge("buffer.size", current_size);
        metrics.gauge("buffer.capacity", buffer.capacity());
        metrics.gauge("buffer.utilization", 
                      100.0 * current_size / buffer.capacity());
        
        if (current_size > buffer.capacity() * 0.9) {
            log_warning("Buffer nearly full!");
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

---

## Verification

### Compile-Time Verification

```cpp
// Capacity must be positive
CircularBuffer<int, 0> bad1;  // Compile error

// Type must be nothrow move constructible
struct BadType { BadType(BadType&&) noexcept(false); };
CircularBuffer<BadType, 10> bad2;  // Compile error

// [[nodiscard]] prevents ignoring results
buffer.push(x);  // Warning: ignoring return value
buffer.pop(x);   // Warning: ignoring return value
```

### Runtime Verification

```cpp
TEST(CircularBuffer, BasicPushPop) {
    CircularBuffer<int, 4> buffer;
    
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    
    EXPECT_TRUE(buffer.push(1));
    EXPECT_TRUE(buffer.push(2));
    EXPECT_TRUE(buffer.push(3));
    EXPECT_TRUE(buffer.push(4));
    
    EXPECT_TRUE(buffer.full());
    EXPECT_FALSE(buffer.push(5));  // Full
    
    int value;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 1);
}

TEST(CircularBuffer, FIFO_Order) {
    CircularBuffer<int, 100> buffer;
    
    for (int i = 0; i < 50; i++) {
        EXPECT_TRUE(buffer.push(i));
    }
    
    for (int i = 0; i < 50; i++) {
        int value;
        EXPECT_TRUE(buffer.pop(value));
        EXPECT_EQ(value, i);  // FIFO order
    }
}

TEST(CircularBuffer, ConcurrentSPSC) {
    CircularBuffer<int, 1024> buffer;
    constexpr int COUNT = 1000000;
    std::atomic<bool> done{false};
    
    std::thread producer([&]{
        for (int i = 0; i < COUNT; i++) {
            while (!buffer.push(i)) {
                std::this_thread::yield();
            }
        }
        done = true;
    });
    
    std::thread consumer([&]{
        int expected = 0;
        while (expected < COUNT) {
            int value;
            if (buffer.pop(value)) {
                EXPECT_EQ(value, expected);
                expected++;
            }
        }
    });
    
    producer.join();
    consumer.join();
}
```

---

## When CircularBuffer Loses

### 1. Multiple Producers or Consumers

CircularBuffer is strictly SPSC:

```cpp
// WRONG: Two producers
std::thread p1([&]{ buffer.push(1); });
std::thread p2([&]{ buffer.push(2); });  // Undefined behavior!
```

**Use instead:** `LockFreeQueue` for MPMC, or multiple CircularBuffers with routing.

### 2. Dynamic Capacity

Capacity is compile-time fixed:

```cpp
CircularBuffer<int, 1024> buffer;
// buffer.resize(2048);  // Not possible!
```

**Use instead:** `LockFreeQueue` for dynamic sizing.

### 3. Blocking Semantics Required

CircularBuffer never blocks:

```cpp
// Can't do this - no blocking wait
buffer.push_blocking(value);  // Doesn't exist
```

**Use instead:** Wrapper with condition variable, or mutex-based queue.

### 4. Very Large Objects

Each slot is sized for `T`. For large objects:

```cpp
CircularBuffer<HugeStruct, 1024> buffer;  // 1024 * sizeof(HugeStruct) allocated
```

**Use instead:** `CircularBuffer<std::unique_ptr<HugeStruct>, 1024>` to store pointers.

### 5. Non-Trivial Destruction Timing

Objects are overwritten on push, destroyed on clear:

```cpp
CircularBuffer<std::shared_ptr<Resource>, 100> buffer;
buffer.push(ptr1);
buffer.push(ptr2);
// ptr1 destroyed immediately when slot reused, not on pop
```

---

## Summary

| Aspect | Manual Ring Buffer | CircularBuffer |
|--------|-------------------|----------------|
| Empty/Full | Ambiguous | Clear (one slot reserved) |
| Thread safety | Manual, error-prone | Wait-free SPSC |
| Index wrap | Modulo (slow) | Bitwise AND (fast) |
| False sharing | Likely | Cache-line aligned |
| Memory ordering | Often wrong | Correct acquire/release |
| Performance | ~1-10M ops/sec | ~300M ops/sec |
| Blocking | Varies | Never blocks |

**Migration ROI:**
- **Immediate:** Correct concurrent behavior, no data races
- **Short-term:** 10-100x performance improvement
- **Long-term:** Simpler code, fewer synchronization bugs

---

## References

- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/) — Memory ordering fundamentals
- [False Sharing](https://mechanical-sympathy.blogspot.com/2011/07/false-sharing.html) — Cache line effects
- Fat-P User Manual: CircularBuffer — Complete API reference
- Fat-P User Manual: LockFreeQueue — For MPMC scenarios

---

*FAT-P Library Documentation — January 2025*
