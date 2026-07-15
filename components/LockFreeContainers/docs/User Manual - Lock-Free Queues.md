---
doc_id: UM-LOCKFREEQUEUE-001
doc_type: "User Manual"
title: "Lock-Free Queues"
fatp_components: ["LockFreeQueue", "WorkQueue", "LockFreeRingBuffer"]
topics: ["lock-free programming", "MPMC queues", "SPSC queues", "producer-consumer patterns", "thread safety", "capacity management", "sequence numbers"]
constraints: ["bounded capacity", "trivially copyable types", "memory ordering", "ABA problem", "cache contention"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "boost::lockfree::queue"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-17"
audience: ["C++ developers", "systems programmers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# User Manual - Lock-Free Queues

*Updated January 2026*

---

## Table of Contents

1. [The Lock-Free Queue Story](#the-lock-free-queue-story)
2. [Understanding Why Locks Hurt](#understanding-why-locks-hurt)
3. [The Sequence Number Insight](#the-sequence-number-insight)
4. [Getting Started](#getting-started)
5. [LockFreeQueue: The FIFO Foundation](#lockfreequeue-the-fifo-foundation)
6. [WorkQueue: Sharding for Scale](#workqueue-sharding-for-scale)
7. [PolicyQueue (design sketch — not in the library)](#policyqueue-design-sketch--not-in-the-library)
8. [LockFreeRingBuffer: Dedicated SPSC](#lockfreeringbuffer-dedicated-spsc)
9. [The Token System: Thread Affinity](#the-token-system-thread-affinity)
10. [Common Patterns](#common-patterns)
11. [Performance Tuning](#performance-tuning)
12. [Error Handling](#error-handling)
13. [Thread Safety Guarantees](#thread-safety-guarantees)
14. [When to Use What](#when-to-use-what)
15. [Migration from std::mutex Queues](#migration-from-stdmutex-queues)
16. [Migration from boost::lockfree::queue](#migration-from-boostlockfreequeue)
17. [Migration from moodycamel::ConcurrentQueue](#migration-from-moodycamelconcurrentqueue)
18. [Alternatives](#alternatives)
19. [Troubleshooting](#troubleshooting)
20. [API Reference](#api-reference)
21. [Glossary](#glossary)

---

## User Manual Card

**Component:** Lock-Free Queue Family  
**Primary use case:** High-throughput producer-consumer communication with 4+ threads  
**Integration pattern:** Replace `std::mutex` + `std::queue` with drop-in lock-free equivalent  
**Key API:** `enqueue(value)` / `dequeue(value)` returning `bool`  
**std equivalent:** None  
**Migration from std:** Replace lock acquisition with return-value checking  
**Common mistakes:** Using with non-trivially-copyable types; ignoring failed enqueue/dequeue returns; destroying queue while threads are active  
**Performance notes:** WorkQueue scales to 16+ threads; LockFreeQueue better for strict FIFO with few threads

---

**Scope:** Practical usage of Fat-P's lock-free queue family: LockFreeQueue, WorkQueue, and LockFreeRingBuffer. Covers integration, API, patterns, migration, and troubleshooting.

**Not covered:**
- Design rationale and algorithm derivation (see Companion Guide - Lock-Free Queues)
- SIMD-accelerated containers and hash maps (see User Manual - FastHashMap, User Manual - StableHashMap)
- General concurrency policy design (see User Manual - ConcurrencyPolicies)
- Memory ordering theory and C++ memory model foundations
- Unbounded or dynamically-resizing lock-free queues

**Prerequisites:** C++20. Familiarity with `std::atomic` and basic multi-threaded programming. Understanding of producer-consumer patterns.

---

## The Lock-Free Queue Story

### The Concurrency Revolution

In 1965, Edsger Dijkstra introduced the semaphore—a synchronization primitive that allowed threads to coordinate access to shared resources. For decades, this model dominated concurrent programming: protect shared data with locks, and only one thread at a time could access it.

The model worked. It was correct. It was understandable. And for single-core processors, it was efficient enough. When only one thread could run at a time anyway, the overhead of lock acquisition was negligible.

Then came multi-core processors.

In 2005, Intel released the Pentium D—two cores on one chip. By 2010, quad-core processors were mainstream. By 2020, server processors had 64 cores. Suddenly, the lock-based model faced a problem it was never designed to handle: genuine parallelism.

When eight threads try to enqueue messages simultaneously, and they all must acquire the same lock, seven threads wait while one works. The lock that ensured correctness now ensures serialization. Eight cores deliver the throughput of one.

Researchers had been studying alternatives since the 1990s. Michael and Scott published their famous lock-free queue algorithm in 1996. Herlihy and Shavit's "The Art of Multiprocessor Programming" (2008) systematized the field. But these algorithms remained academic curiosities for most developers—complex, error-prone, and unnecessary on the hardware of the time.

High-frequency trading changed that. When microseconds mean millions of dollars, the difference between a lock-free queue and a mutex queue is the difference between profit and loss. Game engines followed—60 frames per second leaves 16 milliseconds per frame, and every microsecond spent waiting on locks is a microsecond not spent rendering. Then came cloud infrastructure, where millions of requests per second make even nanosecond overheads significant.

Today, lock-free data structures are essential infrastructure for high-performance systems. Not because they're faster in isolation—a single lock acquisition takes perhaps 20 nanoseconds, trivial for most applications. But because they scale. Because they don't block. Because they provide bounded worst-case latency instead of unbounded worst-case latency.

Fat-P's lock-free queues exist for engineers who've hit the scaling wall. When your profiler shows threads spending 30% of their time waiting for a queue lock, when your P99 latency spikes because a thread holding the lock got preempted, when your 16-core server performs like a 4-core server because of contention—that's when lock-free becomes not just an optimization but a necessity.

### The ABA Problem: Why Lock-Free Is Hard

Lock-free programming is notoriously difficult, and the difficulty centers on a subtle bug called the ABA problem.

Imagine a simple lock-free stack implemented with compare-and-swap (CAS). The stack is a linked list; `top` points to the first node:

```
Initial state: top → A → B → C → null
```

Thread 1 wants to pop. It reads `top` (which is A) and prepares to CAS `top` from A to A's next (which is B):

```
Thread 1: old_top = A, new_top = B
          about to execute: CAS(top, A, B)
```

But Thread 1 gets preempted. While it's sleeping, Thread 2 executes:

```
Thread 2: pop A (top becomes B)
Thread 2: pop B (top becomes C)  
Thread 2: push A back (top becomes A → C → null)
```

Now the stack looks like this:

```
Current state: top → A → C → null
```

Thread 1 wakes up. Its CAS succeeds—`top` is still A! But A's next pointer is now C, not B. Thread 1 sets `top` to B, which is no longer in the stack. The stack is corrupted.

This is the ABA problem: a value changes from A to B to A, and a CAS that expected A succeeds even though the state has changed. The CAS sees the same *bits* but a different *meaning*.

Solutions exist—hazard pointers, epoch-based reclamation, tagged pointers—but they add complexity and overhead. Fat-P takes a different approach: sequence numbers.

Each slot in the queue carries a sequence number that increments every time the slot is used. Even if the slot returns to "empty" state, its sequence number is different. The CAS equivalent checks both the slot state and the sequence, distinguishing "empty for the first time" from "empty again after being used."

This is the core insight that makes Fat-P's queues both correct and efficient.

---

## Understanding Why Locks Hurt

### The Memory Hierarchy

To understand why lock contention hurts so much, you need to understand how modern CPUs access memory.

A modern processor runs at roughly 4 GHz—4 billion cycles per second. Main memory (DDR5 RAM) responds in about 80 nanoseconds—320 cycles. If the CPU had to wait for memory on every access, it would spend most of its time idle.

Caches solve this. The processor maintains multiple levels of cache, each faster but smaller than the next:

| Level | Size | Latency | Distance |
|-------|------|---------|----------|
| Registers | ~1 KB | 0 cycles | On-core |
| L1 Cache | 64 KB | 4 cycles | On-core |
| L2 Cache | 512 KB | 12 cycles | On-core |
| L3 Cache | 32 MB | 40 cycles | Shared across cores |
| Main Memory | 64 GB | 320 cycles | Off-chip |

When you access a memory location, the CPU first checks L1. If the data isn't there (a "cache miss"), it checks L2, then L3, then main memory. Each level is roughly 3-4× slower than the previous.

The critical insight is that caches operate on *cache lines*—64-byte chunks. When you read one byte, the hardware fetches the surrounding 64 bytes. If your next access is nearby, it's probably already cached.

### The Lock Word Problem

A mutex is fundamentally a shared memory location—the "lock word"—that threads read and write atomically. When Thread A acquires the lock, it writes to the lock word. When Thread B tries to acquire, it reads the lock word, sees it's held, and waits.

Here's the problem: on a multi-core system, each core has its own L1 and L2 caches. When Thread A (on Core 0) writes to the lock word, the cache line containing that word is marked "dirty" in Core 0's cache. When Thread B (on Core 1) reads the lock word, it must fetch Core 0's dirty version. This is called *cache coherence*—the hardware ensures all cores see consistent memory.

Cache coherence isn't free. Moving a cache line between cores takes roughly 40-100 nanoseconds on modern systems. When eight threads contend for the same lock, the lock word bounces between eight cores. Every lock acquisition incurs this cost. Every release does too.

This is *cache line bouncing*, and it's the hidden cost of mutex contention. The threads aren't just waiting for each other—they're also waiting for cache lines to migrate between cores.

### The Convoy Effect

There's a second, subtler problem. Consider what happens when multiple threads contend for a lock:

1. Thread A holds the lock, doing work
2. Threads B, C, D arrive and block, waiting for the lock
3. Thread A releases; the OS wakes Thread B
4. Thread B acquires, does minimal work, releases
5. The OS wakes Thread C
6. Thread C acquires, does minimal work, releases
7. And so on...

The threads form a "convoy"—each one wakes up, does a tiny amount of work, and yields to the next. Even though each thread does little work, the convoy persists because threads keep arriving faster than they're serviced.

The convoy effect is particularly insidious because it's self-reinforcing. The more threads waiting, the longer each thread waits, the more likely new threads are to arrive before the queue drains. A queue that performs well at low load can collapse suddenly at high load.

### The Numbers

On our benchmark system (AMD Ryzen 9, 16 cores), the impact is dramatic:

| Threads | std::mutex queue | Fat-P WorkQueue | Slowdown |
|---------|------------------|-----------------|----------|
| 1 | 19.8 ns | 9.3 ns | 2.1× |
| 2 | 26.4 ns | 11.7 ns | 2.3× |
| 4 | 47.2 ns | 22.7 ns | 2.1× |
| 8 | 202.3 ns | 23.2 ns | **8.7×** |
| 16 | 247.4 ns | 24.4 ns | **10.1×** |

At low thread counts, the mutex queue is slower but acceptable. At 8+ threads, it collapses. Fat-P's WorkQueue maintains nearly constant latency regardless of thread count.

---

## The Sequence Number Insight

Fat-P queues avoid the ABA problem and achieve lock-freedom through a technique called sequence-number coordination. Each slot in the queue carries a sequence number that indicates its state and history.

### How It Works

The queue is a ring buffer of slots. Each slot contains:

```cpp
struct Slot {
    std::atomic<size_t> sequence;  // State indicator
    T data;                         // The actual element
};
```

The `sequence` field serves multiple purposes:

1. **State indication**: Tells whether the slot is empty, being written, full, or being read
2. **ABA prevention**: Because the sequence increments monotonically, "empty again" is distinguishable from "empty for the first time"
3. **Memory ordering**: The atomic operations on `sequence` provide the synchronization between producers and consumers

### The Producer Protocol

When a producer wants to enqueue:

1. Atomically increment `tail` to claim a position
2. Compute the slot: `slot = &slots[position % capacity]`
3. Wait until `slot.sequence == position` (slot is ready for writing)
4. Write the data
5. Set `slot.sequence = position + 1` (publish the data)

The key insight is step 3: the producer doesn't proceed until the slot's sequence indicates it's ready. If the consumer hasn't yet released this slot from a previous use, the sequence will be wrong, and the producer waits.

### The Consumer Protocol

When a consumer wants to dequeue:

1. Atomically increment `head` to claim a position
2. Compute the slot: `slot = &slots[position % capacity]`
3. Wait until `slot.sequence == position + 1` (slot contains data)
4. Read the data
5. Set `slot.sequence = position + capacity` (release for reuse)

The consumer waits in step 3 until the producer has published. The "+1" distinguishes "producer finished" from "slot just claimed."

### Why This Is Lock-Free

No thread ever blocks another indefinitely. If a producer is slow writing slot N, other producers proceed with slots N+1, N+2, etc. If a consumer is slow reading, other consumers proceed with later slots. The waits in steps 3 are *spin-waits*—the thread checks a condition repeatedly rather than sleeping. At least one thread always makes progress.

This is the definition of lock-free: the system as a whole makes progress even if individual threads are delayed. Compare to a mutex, where one thread holding the lock can delay all others indefinitely.

---

## Getting Started

### Prerequisites and Integration

Fat-P's lock-free queues require C++20 and have no external dependencies. Include the appropriate header:

```cpp
#include "LockFreeQueue.h"   // LockFreeQueue
#include "WorkQueue.h"       // WorkQueue
#include "LockFreeRingBuffer.h"  // SPSC ring buffer
```

### Type Requirements

All Fat-P queues require elements to be trivially copyable:

```cpp
// OK: Trivially copyable types
fat_p::LockFreeQueue<int, 1024> q1;           // Primitives
fat_p::LockFreeQueue<double, 1024> q2;        // Floating point
fat_p::LockFreeQueue<MyPOD, 1024> q3;         // POD structs
fat_p::LockFreeQueue<void*, 1024> q4;         // Pointers

// ERROR: Not trivially copyable (compile-time failure)
fat_p::LockFreeQueue<std::string, 1024> bad1;      // Non-trivial copy
fat_p::LockFreeQueue<std::vector<int>, 1024> bad2; // Non-trivial copy
```

The restriction exists because lock-free algorithms copy data without holding locks. A non-trivial copy constructor could allocate memory, throw exceptions, or take arbitrary time—all incompatible with lock-free progress guarantees.

For non-trivially-copyable types, queue indices or pointers into separate storage:

```cpp
// Pattern: Queue indices into a separate container
std::vector<LargeObject> storage(MAX_OBJECTS);
fat_p::LockFreeQueue<uint32_t, 1024> index_queue;

// Producer: store object, enqueue index
uint32_t idx = allocate_from_storage();
storage[idx] = produce_object();
index_queue.enqueue(idx);

// Consumer: dequeue index, use object
uint32_t idx;
if (index_queue.dequeue(idx)) {
    process(storage[idx]);
    release_to_storage(idx);
}
```

### Your First Queue

The simplest usage—a bounded MPMC queue with strict FIFO ordering:

```cpp
#include "LockFreeQueue.h"
#include <thread>
#include <atomic>

fat_p::LockFreeQueue<int, 1024> queue;  // Capacity must be power of 2
std::atomic<bool> done{false};

void producer() {
    for (int i = 0; i < 10000; ++i) {
        // enqueue returns false if queue is full
        while (!queue.enqueue(i)) {
            std::this_thread::yield();  // Back off and retry
        }
    }
    done = true;
}

void consumer() {
    int value;
    int count = 0;
    while (!done || !queue.empty()) {
        if (queue.dequeue(value)) {
            ++count;
            // process(value);
        } else {
            std::this_thread::yield();  // Nothing available, back off
        }
    }
    std::cout << "Consumed " << count << " items\n";
}

int main() {
    std::thread p(producer);
    std::thread c(consumer);
    p.join();
    c.join();
}
```

This demonstrates the fundamental pattern: producers call `enqueue()` and handle the full-queue case; consumers call `dequeue()` and handle the empty-queue case. Both operations return `bool` indicating success.

---

## LockFreeQueue: The FIFO Foundation

### When Ordering Matters

LockFreeQueue provides strict FIFO ordering: if item A is enqueued before item B (by the same producer or by different producers in real-time order), then any consumer that dequeues both will receive A before B.

This guarantee matters when order has semantic meaning:

- **Command streams**: Commands must execute in submission order
- **Event logs**: Events must be recorded in occurrence order  
- **Transaction sequences**: Dependencies between transactions require ordering

The cost of this guarantee is contention. All producers compete for the tail position; all consumers compete for the head position. Under high load, this limits scaling.

### Template Parameters

```cpp
template <
    typename T,                    // Element type (must be trivially copyable)
    size_t Capacity,               // Maximum elements (must be power of 2)
    bool EnableStatistics = false  // Track contention metrics
>
class LockFreeQueue;
```

The capacity must be a power of 2 for efficient modular arithmetic (bit masking instead of division). The compile will reject non-power-of-2 values:

```cpp
fat_p::LockFreeQueue<int, 1000> q1;  // ERROR: not power of 2
fat_p::LockFreeQueue<int, 1024> q2;  // OK: 1024 = 2^10
```

### Core Operations

```cpp
[[nodiscard]] bool enqueue(const T& value) noexcept;
[[nodiscard]] bool enqueue(T&& value) noexcept;
```

Attempts to enqueue an element. Returns `true` on success, `false` if the queue is full. Never blocks; never throws.

The `[[nodiscard]]` attribute reminds you to check the return value. Ignoring a failed enqueue means losing data.

```cpp
[[nodiscard]] bool dequeue(T& value) noexcept;
```

Attempts to dequeue into `value`. Returns `true` on success, `false` if the queue is empty. On failure, `value` is unchanged.

```cpp
[[nodiscard]] bool tryDequeue(T& value, size_t maxAttempts = 100) noexcept;
```

Spin-waits up to `maxAttempts` times before returning false. Useful when you expect data to arrive soon:

```cpp
Message msg;
if (queue.tryDequeue(msg, 1000)) {
    // Got a message within ~1000 spins
} else {
    // No message after extended wait; do something else
}
```

### Query Operations

```cpp
[[nodiscard]] bool empty() const noexcept;
[[nodiscard]] size_t size() const noexcept;
[[nodiscard]] static constexpr size_t capacity() noexcept;
```

These are approximate under concurrent modification. Between checking `empty()` and acting on the result, another thread may have enqueued or dequeued. Use them for diagnostics, not for synchronization:

```cpp
// WRONG: Race condition between empty() and dequeue()
if (!queue.empty()) {
    queue.dequeue(value);  // Might fail anyway!
}

// RIGHT: Just try to dequeue
if (queue.dequeue(value)) {
    // Success
}
```

### Optional Statistics

When `EnableStats = true`, the queue tracks operation counts:

```cpp
fat_p::LockFreeQueue<int, 1024, true> queue;

// ... use queue ...

auto stats = queue.stats();
std::cout << "Total enqueues:  " << stats.totalEnqueues << "\n";
std::cout << "Total dequeues:  " << stats.totalDequeues << "\n";
std::cout << "Failed enqueues: " << stats.failedEnqueues << "\n";
std::cout << "Failed dequeues: " << stats.failedDequeues << "\n";
std::cout << "Current size:    " << stats.currentSize
          << " / " << stats.capacity << "\n";

queue.resetStats();  // Clear for next measurement period
```

High failed-enqueue counts indicate the queue is a bottleneck (producers finding it full). Consider increasing capacity, switching to WorkQueue, or reducing producer/consumer thread counts.

---

## WorkQueue: Sharding for Scale

### Why Sharding Wins

WorkQueue sacrifices global FIFO ordering for dramatically better scaling. Instead of one queue with one head and one tail, it maintains multiple independent "shards"—each a full LockFreeQueue:

```
WorkQueue with 4 shards:
  Shard 0: [───────────────]
  Shard 1: [───────────────]
  Shard 2: [───────────────]
  Shard 3: [───────────────]
```

When a producer enqueues, it picks a shard (based on affinity or probing) and enqueues there. When a consumer dequeues, it picks a shard and dequeues from there. With N shards and M threads, contention drops to roughly M/N threads per shard.

The tradeoff: an item enqueued to shard 2 might be dequeued before an item enqueued earlier to shard 0. Global FIFO is lost. For work distribution (thread pools, job systems, task schedulers), this is acceptable—you care that work gets done, not the exact order.

### Template Parameters

```cpp
template <
    typename T,                                  // Element type
    size_t ShardCount = 16,                      // Number of shards
    size_t ShardCapacity = 1024,                 // Capacity per shard
    typename RoutingPolicy = DefaultRoutingPolicy,
    typename BackoffPolicy = DefaultBackoffPolicy
>
class WorkQueue;
```

Total capacity is `ShardCount × ShardCapacity`. More shards mean less contention but more memory overhead for small queues.

### The Token System

WorkQueue uses "tokens" to maintain thread affinity to shards:

```cpp
fat_p::work_queue::WorkQueue<Task, 16, 1024> queue;

// Create tokens (typically once per thread, stored in thread-local)
auto producerToken = queue.makeProducerToken();
auto consumerToken = queue.makeConsumerToken();

// Use tokens for better locality
queue.enqueue(producerToken, task);

Task received;
queue.dequeue(consumerToken, received);
```

Tokens cache the last successful shard. When a producer's enqueue succeeds on shard 5, the token remembers that. The next enqueue tries shard 5 first. If that shard is full, it probes others.

This affinity has two benefits. First, repeated operations from the same thread tend to hit the same cache lines, improving locality. Second, threads naturally distribute across shards over time, balancing load without explicit coordination.

You can also use the token-free API for simplicity:

```cpp
queue.enqueue(task);      // Uses thread-local token internally
queue.dequeue(received);  // Uses thread-local token internally
```

The token-free API is slightly slower due to thread-local lookup but is fine for most use cases.

### Enqueue Strategy

When you call `enqueue()`, WorkQueue follows a three-phase strategy:

1. **Preferred shard**: Try the shard cached in the token (O(1))
2. **Probe phase**: Try 4 random shards (O(1) expected)
3. **Scan phase**: Try all shards sequentially (O(shards) worst case)

Returns `false` only if ALL shards are full. This is rare with appropriate sizing.

### Dequeue Strategy

Dequeue is symmetric:

1. **Preferred shard**: Try the cached shard (O(1))
2. **Probe phase**: Try 8 random shards (O(1) expected)
3. **Scan phase**: Try all shards (O(shards) worst case)

Returns `false` only if ALL shards are empty.

The asymmetry (4 probes for enqueue, 8 for dequeue) reflects typical workloads where empty shards are more common than full shards.

---

## PolicyQueue (design sketch — not in the library)

> **Note:** PolicyQueue (with SingleTopology/ShardedTopology policies) was a design sketch and is not part of the library. To write topology-generic code, template on the queue type directly—LockFreeQueue and WorkQueue share the `enqueue(value)` / `dequeue(value)` interface:
>
> ```cpp
> template <typename Queue>
> void worker(Queue& q) {
>     Task task;
>     while (q.dequeue(task)) {
>         process(task);
>     }
> }
> ```

---

## LockFreeRingBuffer: Dedicated SPSC

### The SPSC Specialization

When exactly one thread produces and exactly one thread consumes, the full MPMC machinery is unnecessary. LockFreeRingBuffer provides an optimized Single-Producer Single-Consumer implementation:

```cpp
fat_p::LockFreeRingBuffer<Sample> audio_buffer(256);  // capacity rounded up to a power of 2

// Producer thread only
void capture() {
    Sample s = read_hardware();
    while (!audio_buffer.push(s)) { /* buffer full */ }
}

// Consumer thread only
void process() {
    Sample s;
    if (audio_buffer.pop(s)) {
        output(transform(s));
    }
}
```

SPSC needs only two atomic variables (head and tail) with relaxed ordering. No CAS loops, no contention, no retries. The result is sub-nanosecond overhead in the common case.

### The Contract

Using LockFreeRingBuffer with multiple producers or multiple consumers is **undefined behavior**. The implementation does not check or enforce the single-producer/single-consumer constraint. If you violate it, you'll get data corruption, not a helpful error message.

```cpp
// UNDEFINED BEHAVIOR: Two producers
std::thread p1([&] { buffer.push(1); });
std::thread p2([&] { buffer.push(2); });  // WRONG!

// CORRECT: One producer, one consumer
std::thread producer([&] { while (running) buffer.push(produce()); });
std::thread consumer([&] { while (running) { Sample s; buffer.pop(s); } });
```

If you might have multiple producers or consumers, use LockFreeQueue instead.

---

## The Token System: Thread Affinity

### Why Tokens Exist

In a sharded queue, the choice of shard affects both performance and fairness. Random shard selection distributes load but incurs cache misses. Fixed shard assignment causes imbalance if some threads are faster than others.

Tokens provide adaptive affinity: threads tend to reuse shards that worked recently, but can migrate when their preferred shard is busy. This balances locality (cache efficiency) with load distribution (no hot spots).

### Token Lifecycle

Tokens are lightweight (one `size_t` for the cached shard index) and cheap to create:

```cpp
// Option 1: Store in thread-local (common pattern)
thread_local auto producerToken = queue.makeProducerToken();
thread_local auto consumerToken = queue.makeConsumerToken();

// Option 2: Store in thread context
struct WorkerContext {
    WorkQueue::ProducerToken ptok;
    WorkQueue::ConsumerToken ctok;
    
    WorkerContext(WorkQueue& q) 
        : ptok(q.makeProducerToken())
        , ctok(q.makeConsumerToken()) {}
};

// Option 3: Use token-free API (simplest, slightly slower)
queue.enqueue(value);  // Internal thread-local token
```

### Token Best Practices

Create tokens early, use them throughout the thread's lifetime:

```cpp
void worker_thread(WorkQueue<Task>& queue) {
    auto ctok = queue.makeConsumerToken();  // Create once
    
    while (running) {
        Task task;
        if (queue.dequeue(ctok, task)) {   // Reuse token
            process(task);
        }
    }
}
```

Don't create tokens per-operation—you'll lose the affinity benefit.

---

## Common Patterns

### Producer-Consumer Pipeline

The classic pattern: one or more producers generate work; one or more consumers process it:

```cpp
fat_p::work_queue::WorkQueue<Work, 16, 1024> queue;
std::atomic<bool> shutdown{false};
std::atomic<int> active_producers{0};

void producer(int id) {
    ++active_producers;
    auto tok = queue.makeProducerToken();
    
    while (has_work()) {
        Work w = generate_work();
        while (!queue.enqueue(tok, w) && !shutdown) {
            std::this_thread::yield();
        }
    }
    
    --active_producers;
}

void consumer() {
    auto tok = queue.makeConsumerToken();
    
    while (!shutdown || active_producers > 0 || !queue.empty()) {
        Work w;
        if (queue.dequeue(tok, w)) {
            process(w);
        } else {
            std::this_thread::yield();
        }
    }
}
```

### Bounded Buffer with Backpressure

When producers might outpace consumers, apply backpressure:

```cpp
template <typename T, size_t Cap>
class BoundedChannel {
    fat_p::LockFreeQueue<T, Cap> queue_;
    std::atomic<size_t> approx_size_{0};
    
public:
    // Returns false if soft limit exceeded (backpressure)
    bool try_send(const T& value, size_t soft_limit) {
        if (approx_size_.load(std::memory_order_relaxed) >= soft_limit) {
            return false;  // Apply backpressure
        }
        if (queue_.enqueue(value)) {
            approx_size_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return false;  // Queue full
    }
    
    bool try_recv(T& value) {
        if (queue_.dequeue(value)) {
            approx_size_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }
};
```

The soft limit lets you start rejecting work before the hard limit, giving consumers time to catch up.

### SPSC Pipeline Stages

For dedicated producer-consumer pairs, chain SPSC buffers:

```cpp
// Pipeline: Capture → Process → Output
fat_p::LockFreeRingBuffer<RawData> capture_to_process(256);
fat_p::LockFreeRingBuffer<ProcessedData> process_to_output(256);

void capture_thread() {
    while (running) {
        RawData d = read_sensor();
        while (!capture_to_process.push(d) && running) { spin_wait(); }
    }
}

void process_thread() {
    while (running) {
        RawData raw;
        if (capture_to_process.pop(raw)) {
            ProcessedData processed = transform(raw);
            while (!process_to_output.push(processed) && running) { spin_wait(); }
        }
    }
}

void output_thread() {
    while (running) {
        ProcessedData p;
        if (process_to_output.pop(p)) {
            write_output(p);
        }
    }
}
```

Each stage has dedicated threads; each buffer has exactly one producer and one consumer.

---

## Performance Tuning

### Choosing Capacity

Queue capacity should accommodate burst traffic without overflow. Rule of thumb:

```cpp
capacity = 2 × max_producers × expected_burst_size
```

For 8 producers with 100-item bursts:

```cpp
constexpr size_t capacity = next_power_of_2(2 * 8 * 100);  // 2048
fat_p::LockFreeQueue<Item, capacity> queue;
```

Too small: frequent `enqueue` failures, backpressure or data loss.
Too large: wasted memory, worse cache behavior.

### Choosing Shard Count

For WorkQueue, shard count determines contention distribution:

```cpp
shards ≈ 2 × hardware_concurrency
```

For an 8-core system:

```cpp
fat_p::work_queue::WorkQueue<Task, 16, 1024> queue;  // 16 shards
```

More shards reduce contention but increase memory footprint and probe costs.

### Avoiding False Sharing

Fat-P queues pad internal structures to cache line boundaries. But ensure queue objects themselves don't share cache lines with frequently-modified data:

```cpp
// BAD: Counter might share cache line with queue internals
struct BadLayout {
    fat_p::LockFreeQueue<int, 1024> queue;
    std::atomic<int> hot_counter;  // Updated frequently
};

// GOOD: Explicit separation
struct GoodLayout {
    fat_p::LockFreeQueue<int, 1024> queue;
    alignas(64) std::atomic<int> hot_counter;  // Own cache line
};
```

---

## Error Handling

### Full Queue

When `enqueue` returns `false`, the queue is full. Options:

```cpp
// Pattern 1: Spin until success (simple, may waste CPU)
while (!queue.enqueue(value)) {
    std::this_thread::yield();
}

// Pattern 2: Bounded retry with failure reporting
bool enqueue_with_retry(const T& value, int max_retries) {
    for (int i = 0; i < max_retries; ++i) {
        if (queue.enqueue(value)) return true;
        std::this_thread::yield();
    }
    return false;  // Caller handles overflow
}

// Pattern 3: Drop oldest (lossy queue)
if (!queue.enqueue(value)) {
    T discarded;
    queue.dequeue(discarded);  // Make room
    queue.enqueue(value);      // Should succeed now
}
```

### Empty Queue

When `dequeue` returns `false`, the queue is empty. Options:

```cpp
// Pattern 1: Return optional
std::optional<T> try_dequeue() {
    T value;
    if (queue.dequeue(value)) {
        return value;
    }
    return std::nullopt;
}

// Pattern 2: Spin with timeout
bool dequeue_with_timeout(T& value, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (queue.dequeue(value)) return true;
        std::this_thread::yield();
    }
    return false;
}

// Pattern 3: Use tryDequeue with spin count
T value;
if (queue.tryDequeue(value, 1000)) {
    // Got value within ~1000 spins
}
```

---

## Thread Safety Guarantees

### What Is Guaranteed

| Operation | Guarantee |
|-----------|-----------|
| `enqueue` from multiple threads | Safe, lock-free |
| `dequeue` from multiple threads | Safe, lock-free |
| `enqueue` + `dequeue` concurrently | Safe, lock-free |
| `size()` / `empty()` during operations | Safe, approximate |
| Construction | Not thread-safe (complete before sharing) |
| Destruction | Not thread-safe (drain and join before destroying) |

### What Is NOT Guaranteed

| Operation | Issue |
|-----------|-------|
| `size()` accuracy | Momentarily incorrect under concurrent modification |
| Order across shards (WorkQueue) | No global FIFO |
| SPSC buffer with multiple producers | **Undefined behavior** |
| SPSC buffer with multiple consumers | **Undefined behavior** |
| Destruction during operations | **Undefined behavior** |

### Safe Shutdown Pattern

```cpp
std::atomic<bool> shutdown{false};
fat_p::work_queue::WorkQueue<Task, 16, 1024> queue;

void producer() {
    while (!shutdown) {
        queue.enqueue(produce_task());
    }
    // Producer exits; stops adding work
}

void consumer() {
    while (!shutdown || !queue.empty()) {
        Task t;
        if (queue.dequeue(t)) {
            process(t);
        } else {
            std::this_thread::yield();
        }
    }
    // Consumer drains remaining work then exits
}

void shutdown_system() {
    shutdown = true;
    
    // Wait for producers to stop
    for (auto& p : producer_threads) p.join();
    
    // Wait for consumers to drain queue
    for (auto& c : consumer_threads) c.join();
    
    // Now safe to destroy queue
}
```

---

## When to Use What

| Scenario | Recommendation | Why |
|----------|----------------|-----|
| Strict FIFO required | LockFreeQueue | Global ordering guaranteed |
| Maximum throughput, ordering doesn't matter | WorkQueue | Sharding reduces contention |
| 1-2 threads only | `std::mutex` + `std::queue` | Simpler, fast enough |
| 4+ threads, symmetric load | WorkQueue | Scales well |
| Many producers, few consumers (MPSC) | WorkQueue or moodycamel | Both handle this well |
| Few producers, many consumers (SPMC) | WorkQueue | Beats moodycamel 2× |
| Dedicated producer-consumer pair | LockFreeRingBuffer | Minimum overhead |
| Non-trivially-copyable types | Queue of indices/pointers | Indirect through separate storage |
| Unbounded capacity required | moodycamel | Fat-P queues are bounded |

---

## Migration from std::mutex Queues

### API Mapping

| std::mutex + std::queue | Fat-P LockFreeQueue |
|-------------------------|---------------------|
| `lock(); queue.push(x); unlock();` | `if (queue.enqueue(x)) { /* success */ }` |
| `lock(); if (!queue.empty()) { x = queue.front(); queue.pop(); } unlock();` | `if (queue.dequeue(x)) { /* success */ }` |
| `lock(); bool e = queue.empty(); unlock();` | `bool e = queue.empty();` (approximate) |
| `lock(); size_t s = queue.size(); unlock();` | `size_t s = queue.size();` (approximate) |

### Key Differences

1. **Return values matter**: Lock-free operations can fail; always check returns
2. **No blocking**: Operations return immediately; you decide whether to retry
3. **Approximate queries**: `empty()` and `size()` are snapshots, not synchronized
4. **Bounded capacity**: Must handle full-queue case (mutex queues are typically unbounded)

### Migration Example

Before:
```cpp
std::mutex mtx;
std::queue<Message> queue;

void send(Message m) {
    std::lock_guard lock(mtx);
    queue.push(std::move(m));
}

bool receive(Message& m) {
    std::lock_guard lock(mtx);
    if (queue.empty()) return false;
    m = std::move(queue.front());
    queue.pop();
    return true;
}
```

After:
```cpp
fat_p::LockFreeQueue<Message, 4096> queue;

void send(Message m) {
    while (!queue.enqueue(std::move(m))) {
        std::this_thread::yield();  // Handle full queue
    }
}

bool receive(Message& m) {
    return queue.dequeue(m);  // Returns false if empty
}
```

---

## Migration from boost::lockfree::queue

### API Mapping

| boost::lockfree::queue | Fat-P LockFreeQueue |
|------------------------|---------------------|
| `queue.push(x)` | `queue.enqueue(x)` |
| `queue.pop(x)` | `queue.dequeue(x)` |
| `queue.empty()` | `queue.empty()` |
| `queue.consume_all(f)` | Loop with `dequeue()` |

### Key Differences

1. **Method names**: `push`/`pop` → `enqueue`/`dequeue`
2. **No consume_all**: Fat-P doesn't have bulk operations; loop manually
3. **Better scaling**: Fat-P's algorithm scales better under contention
4. **No bounded option**: Boost has both bounded and unbounded; Fat-P is bounded-only

---

## Migration from moodycamel::ConcurrentQueue

### API Mapping

| moodycamel | Fat-P WorkQueue |
|------------|-----------------|
| `queue.enqueue(x)` | `queue.enqueue(tok, x)` |
| `queue.try_dequeue(x)` | `queue.dequeue(tok, x)` |
| `ProducerToken` | `ProducerToken` (similar concept) |
| `ConsumerToken` | `ConsumerToken` (similar concept) |
| `queue.try_enqueue(x)` | `queue.enqueue(tok, x)` (always non-blocking) |
| `queue.enqueue_bulk(...)` | Loop with `enqueue()` |

### Key Differences

1. **Bounded only**: Fat-P WorkQueue is always bounded; moodycamel can be unbounded
2. **SPMC performance**: Fat-P beats moodycamel 2× in consumer-heavy patterns
3. **No bulk operations**: Fat-P doesn't have `enqueue_bulk`/`dequeue_bulk`
4. **Simpler API**: Fewer options, fewer ways to misconfigure

---

## Alternatives

- **moodycamel::ConcurrentQueue** — Mature, well-tested, supports unbounded mode. Excels at MPSC. Consider if you need unbounded capacity or bulk operations.

- **boost::lockfree::queue** — Part of Boost. Michael-Scott algorithm. Consider if you're already using Boost and scaling isn't critical.

- **folly::MPMCQueue** — Facebook's implementation. Similar bounded MPMC design. Consider if you're already using Folly.

- **Intel TBB concurrent_queue** — Part of Threading Building Blocks. Consider if you're already using TBB.

- **std::mutex + std::queue** — The baseline. Simple and correct. Consider if you have 1-2 threads or contention isn't a problem.

---

## Troubleshooting

### Compilation Errors

**"static_assert failed: T must be trivially copyable"**

Your element type has a non-trivial copy constructor or destructor. Queue indices or pointers instead:

```cpp
// Instead of:
fat_p::LockFreeQueue<std::string, 1024> queue;  // ERROR

// Do:
std::vector<std::string> storage;
fat_p::LockFreeQueue<size_t, 1024> index_queue;
```

**"static_assert failed: Capacity must be a power of 2"**

```cpp
// Instead of:
fat_p::LockFreeQueue<int, 1000> queue;  // ERROR: 1000 ≠ 2^n

// Do:
fat_p::LockFreeQueue<int, 1024> queue;  // OK: 1024 = 2^10
```

### Runtime Issues

**Enqueue always returns false**

The queue is full. Either:
- Increase capacity
- Add consumers to drain faster
- Implement backpressure to slow producers

**Dequeue always returns false**

The queue is empty. Either:
- Producers aren't producing
- Check your shutdown logic (consumers may be stopping too early)
- Use `tryDequeue()` with a spin count

**Performance degrades over time**

Possible causes:
- Memory fragmentation elsewhere (not in the queue itself)
- Thermal throttling on sustained load
- Growing contention as more threads spawn

**Data corruption with SPSC ring buffer**

You're violating the single-producer or single-consumer contract. Use LockFreeQueue instead.

### Performance Issues

**High failure statistics**

```cpp
auto stats = queue.stats();
if (stats.failedEnqueues > threshold) {
    // Queue is bottleneck; consider:
    // 1. More shards (WorkQueue)
    // 2. Larger capacity
    // 3. Fewer producers
}
```

**Latency spikes**

Possible causes:
- Queue repeatedly filling and draining (size oscillates around capacity)
- Cache pollution from other code
- OS scheduler interference

Consider:
- Larger capacity to absorb bursts
- CPU affinity to reduce scheduler interference
- Dedicated cores for latency-critical threads

---

## API Reference

### LockFreeQueue<T, Capacity, EnableStats>

| Member | Description |
|--------|-------------|
| `bool enqueue(const T&)` | Enqueue by copy; returns false if full |
| `bool enqueue(T&&)` | Enqueue by move; returns false if full |
| `bool dequeue(T&)` | Dequeue into reference; returns false if empty |
| `bool tryDequeue(T&, size_t)` | Spin-wait dequeue; returns false after max attempts |
| `bool empty() const` | Approximate emptiness check |
| `size_t size() const` | Approximate element count |
| `static constexpr size_t capacity()` | Compile-time capacity |
| `Stats stats() const` | Operation counts: totalEnqueues, totalDequeues, failedEnqueues, failedDequeues, currentSize, capacity (if EnableStats) |
| `void resetStats()` | Clear statistics (if EnableStats) |

### WorkQueue<T, ShardCount, ShardCapacity, RoutingPolicy, BackoffPolicy>

| Member | Description |
|--------|-------------|
| `ProducerToken makeProducerToken()` | Create producer affinity token |
| `ConsumerToken makeConsumerToken()` | Create consumer affinity token |
| `bool enqueue(ProducerToken&, const T&)` | Enqueue with token |
| `bool enqueue(const T&)` | Enqueue with thread-local token |
| `bool dequeue(ConsumerToken&, T&)` | Dequeue with token |
| `bool dequeue(T&)` | Dequeue with thread-local token |
| `bool empty() const` | True if all shards empty |
| `size_t size() const` | Sum of all shard sizes |
| `static constexpr size_t capacity()` | Total capacity (shards × shard_capacity) |
| `static constexpr size_t shard_count()` | Number of shards |
| `static constexpr size_t shard_capacity()` | Per-shard capacity |

### LockFreeRingBuffer<T>

Capacity is a runtime constructor argument, rounded up to the next power of 2: `LockFreeRingBuffer(size_t capacity)`.

| Member | Description |
|--------|-------------|
| `bool push(const T&)` | Push by copy (single producer only!) |
| `bool push(T&&)` | Push by move (single producer only!) |
| `std::optional<T> pop()` | Pop; nullopt if empty (single consumer only!) |
| `bool pop(T&)` | Pop into reference (single consumer only!) |
| `std::optional<T> peek() const` | Peek at front without removing (single consumer only!) |
| `bool empty() const` | Check if empty |
| `bool full() const` | Check if full |
| `size_t size() const` | Current element count (approximate) |
| `size_t capacity() const` | Capacity (as rounded at construction) |

---

## Glossary

| Term | Definition |
|------|------------|
| **ABA problem** | A CAS succeeds because value returned to A, but state changed A→B→A |
| **Bounded queue** | Queue with fixed maximum capacity; enqueue can fail |
| **Cache line** | Unit of cache transfer, typically 64 bytes |
| **CAS** | Compare-and-swap; atomic read-modify-write operation |
| **Convoy effect** | Threads serialize behind a lock holder, waking one at a time |
| **False sharing** | Unrelated data on same cache line causing invalidation traffic |
| **Lock-free** | At least one thread makes progress under contention |
| **MPMC** | Multiple-Producer Multiple-Consumer |
| **MPSC** | Multiple-Producer Single-Consumer |
| **Sequence number** | Monotonic counter per slot indicating state and preventing ABA |
| **Shard** | Independent sub-queue in a sharded design |
| **SPMC** | Single-Producer Multiple-Consumer |
| **SPSC** | Single-Producer Single-Consumer |
| **Spin-wait** | Busy-waiting in a loop, checking a condition repeatedly |
| **Token** | Object caching thread affinity to a shard |

---

*LockFreeQueue.h | WorkQueue.h | LockFreeRingBuffer.h — Fat-P Library v3.2*
