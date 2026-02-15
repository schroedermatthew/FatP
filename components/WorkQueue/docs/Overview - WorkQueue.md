---
doc_id: OV-WORKQUEUE-001
doc_type: "Overview"
title: "WorkQueue"
fatp_components: ["WorkQueue"]
topics: ["work queue", "sharded queue", "lock-free", "MPMC", "producer-consumer", "concurrent queue", "shard routing"]
constraints: ["trivially copyable types only", "no global FIFO ordering", "bounded capacity", "relaxed ordering across shards"]
cxx_standard: "C++17"
std_equivalent: null
std_since: null
boost_equivalent: "boost::lockfree::queue"
build_modes: ["Debug", "Release"]
last_verified: "2026-02-14"
audience: ["C++ developers", "systems programmers", "concurrency engineers", "AI assistants"]
status: "reviewed"
---

# Overview - WorkQueue

*Fat-P Library — February 2026*

---

## Executive Summary

WorkQueue is a sharded lock-free MPMC queue that eliminates the single-counter bottleneck of traditional concurrent queues. Instead of one shared head/tail pair that every thread contends on, WorkQueue distributes work across N independent lock-free shards. Each thread prefers a local shard determined by thread ID hashing, and probes other shards when its preferred shard is full or empty. This trades strict global FIFO ordering for dramatically lower contention under high producer counts, maintaining stable throughput as thread count increases while single-queue designs degrade under CAS retry storms.

---

## Overview Card

**Component:** WorkQueue  
**Problem solved:** High-throughput MPMC queueing without single-counter contention bottleneck  
**When to use:** Many-producer workloads (task dispatching, event buses, log aggregation); any MPMC scenario where strict FIFO is not required  
**When NOT to use:** Strict FIFO ordering required (use LockFreeQueue); SPSC only (use LockFreeRingBuffer, substantially faster); non-trivially-copyable types; unbounded queue needed  
**Key guarantee:** Lock-free enqueue and dequeue; exactly-once delivery; bounded total capacity  
**std equivalent:** None  
**Boost equivalent:** `boost::lockfree::queue` (single queue, not sharded)  
**Other alternatives:** moodycamel::ConcurrentQueue, folly::MPMCQueue, Intel TBB concurrent_queue  
**Read next:** User Manual - WorkQueue

---

## The Problem Domain

### What Goes Wrong Without It

A typical lock-free MPMC queue uses a single atomic head and a single atomic tail:

```cpp
// Simplified lock-free queue core
std::atomic<size_t> head_{0};  // All consumers CAS on this
std::atomic<size_t> tail_{0};  // All producers CAS on this

bool enqueue(T value) {
    auto tail = tail_.load();
    // ... CAS loop on tail_ ...
}
```

With 8 producers, each `enqueue()` contends on the same `tail_` counter. Every CAS failure means a wasted atomic round-trip. Under high contention, most CAS attempts fail, and throughput collapses. The queue's theoretical O(1) enqueue becomes O(P) in practice, where P is the producer count.

Mutex-based queues are worse: `std::mutex` serializes all access. Under multi-producer contention, throughput degrades significantly compared to the uncontended baseline.

### The Sharding Solution

WorkQueue distributes the hot counters across N independent shards. Each shard is a bounded lock-free MPMC queue (a `fat_p::LockFreeQueue` instance). Producers and consumers are assigned a preferred shard via thread ID hashing, so threads that happen to map to different shards never contend with each other at all.

When a producer's preferred shard is full, it probes neighboring shards using a lightweight RNG. When a consumer's preferred shard is empty, it scans other shards. This means:

- **Best case (low contention):** Threads hit different shards, zero contention, near-uncontended single-queue performance
- **Moderate contention:** Some shard overlap, partial CAS failures, graceful degradation
- **High contention (8P:1C):** Sharding reduces effective contention by factor of N, maintaining stable throughput where single-queue designs degrade

The tradeoff is ordering: elements enqueued on different shards may be dequeued in a different order. WorkQueue provides *work-queue semantics* (every element is delivered exactly once, but not necessarily in global FIFO order), which is correct for task dispatching, event handling, and log aggregation.

---

## Architecture

```
WorkQueue<int, ShardCount=4, ShardCapacity=1024>

Total capacity: 4 × 1024 = 4,096 elements

┌─────────────────────────────────────────────────┐
│  Shard 0         Shard 1        Shard 2        Shard 3       │
│  ┌──────────┐   ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │LockFree  │   │LockFree  │  │LockFree  │  │LockFree  │   │
│  │Queue     │   │Queue     │  │Queue     │  │Queue     │   │
│  │[1024]    │   │[1024]    │  │[1024]    │  │[1024]    │   │
│  └──────────┘   └──────────┘  └──────────┘  └──────────┘   │
│       ↑              ↑             ↑              ↑          │
│  Thread A        Thread B     Thread C       Thread D        │
│  (preferred)     (preferred)  (preferred)    (preferred)     │
└─────────────────────────────────────────────────┘

enqueue path:
  1. Hash thread ID → preferred shard
  2. Try enqueue on preferred shard
  3. If full → probe next kEnqueueProbeCount shards
  4. If all probed shards full → scan all shards
  5. If still full → return false (bounded queue is at capacity)

dequeue path:
  1. Hash thread ID → preferred shard  
  2. Try dequeue from preferred shard
  3. If empty → probe next kDequeueProbeCount shards
  4. If all probed shards empty → scan all shards
  5. If still empty → return false (queue is empty)
```

### Token-Based Affinity

Producer and consumer tokens store the preferred shard index and an RNG state for probe ordering:

```cpp
fat_p::work_queue::WorkQueue<int> q;

auto ptok = q.makeProducerToken();   // Thread-local affinity
auto ctok = q.makeConsumerToken();

q.enqueue(ptok, 42);     // Routes to preferred shard
int value;
q.dequeue(ctok, value);  // Routes to preferred shard
```

Tokens are lightweight (two integers). The tokenless `enqueue()`/`dequeue()` overloads use thread-local storage internally—convenient but slightly slower on first call per thread due to TLS initialization.

### Policy-Based Configuration

```cpp
// Custom routing: more aggressive probing
struct AggressiveRouting {
    static constexpr size_t kEnqueueProbeCount = 8;
    static constexpr size_t kDequeueProbeCount = 16;
    static constexpr bool kScanAllOnEnqueueFail = true;
    static constexpr bool kScanAllOnDequeueFail = true;
};

// Custom backoff: use pause instruction
struct PauseBackoff {
    static void pause() noexcept {
        #if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
        #else
        std::this_thread::yield();
        #endif
    }
};

using MyQueue = fat_p::work_queue::WorkQueue<
    int,
    32,               // 32 shards
    4096,             // 4096 elements per shard
    AggressiveRouting,
    PauseBackoff
>;
```

---

## Feature Inventory

### 1. Lock-Free MPMC

Every shard is a lock-free bounded MPMC queue built on atomic CAS. No mutexes anywhere in the enqueue or dequeue paths. Progress guarantee: at least one thread makes progress in any contention scenario.

### 2. Shard-Based Contention Reduction

With 16 shards (default), 8 producers have only a 50% chance of any two mapping to the same shard. Effective contention is reduced by a factor proportional to shard count.

### 3. Bounded Capacity

Total capacity is `ShardCount × ShardCapacity`, fixed at compile time. No dynamic allocation after construction. This makes WorkQueue suitable for real-time and embedded contexts where heap allocation is forbidden.

### 4. Token Affinity

Producer and consumer tokens provide stable shard affinity across calls, improving cache locality. The same producer repeatedly hitting the same shard keeps that shard's cache lines hot.

### 5. Configurable Probe and Backoff

Routing policy controls how many shards are probed before scanning all shards. Backoff policy controls the pause between retry attempts in `tryDequeue`. Both are compile-time policies with zero runtime dispatch overhead.

---

## Performance Characteristics

Benchmarks compare WorkQueue against `fat_p::LockFreeQueue` (single MPMC queue), `moodycamel::ConcurrentQueue`, `std::mutex + std::queue`, and `boost::lockfree::queue` across contention patterns from 1P:1C through 8P:2C.

**What the benchmarks show:**

- **Single-threaded (1P:1C):** WorkQueue adds modest overhead vs a single LockFreeQueue due to shard routing. This is the cost of sharding when there's no contention to distribute.

- **Multi-producer contention (4P+):** WorkQueue's sharding eliminates the CAS retry storm that cripples single-queue designs. Throughput remains stable as producer count increases, while single-queue designs degrade significantly.

- **Balanced MPMC (8P:2C):** WorkQueue outperforms all alternatives tested at this contention level. The combination of producer-side sharding and consumer-side probe scanning efficiently distributes load.

See `components/WorkQueue/results/` and `benchmark_results/` for current platform-specific benchmark data.

### Where WorkQueue Wins

**Many-producer scenarios.** Task dispatching with many submitter threads. Event buses where many subsystems emit events.

**Balanced MPMC.** Sharding distributes contention across both producer and consumer sides, maintaining throughput where single-queue designs collapse.

**Zero-dependency requirement.** WorkQueue is header-only with no external dependencies. moodycamel requires a third-party header.

### Where WorkQueue Loses

**Low contention.** With 1-2 producers, a single LockFreeQueue is simpler and equally fast.

**Strict FIFO.** Cross-shard ordering is relaxed. Use LockFreeQueue if global FIFO matters.

**Non-trivially-copyable types.** The lock-free core requires `std::is_trivially_copyable_v<T>`. For complex types, either wrap in a pointer/index, or use a different queue.

---

## Integration Points

```
WorkQueue.h
    → uses: LockFreeQueue.h (shard implementation)
    → uses: FatPConfig.h (cache line size)
    → used by: task dispatching, event aggregation, log pipelines
    → pairs with: ThreadPool.h (WorkQueue can serve as the internal task queue)
    → pairs with: LockFreeRingBuffer.h (use RingBuffer for SPSC, WorkQueue for MPMC)
```

---

## Final Assessment

**Permanence.** C++ has no standard concurrent queue. `std::execution` (C++26) provides asynchronous composition abstractions but no concrete queue. The sharded-queue pattern is a proven scalability technique used in production systems (LMAX Disruptor, folly::MPMCQueue, Java's ConcurrentLinkedQueue with segment-based sharding).

**Specialization.** WorkQueue's shard-based design is specifically optimized for the many-producer pattern that single-queue designs handle poorly. The policy-based routing and backoff allow tuning for specific contention profiles without runtime overhead.

**Control.** Compile-time shard count, shard capacity, routing policy, and backoff policy. No dynamic allocation. Bounded capacity provides deterministic memory usage and backpressure via `enqueue()` returning false.

For MPMC workloads with many producers and relaxed ordering requirements, WorkQueue provides the best throughput-per-contention tradeoff in the Fat-P concurrency toolkit.

---

*WorkQueue.h — Fat-P Library*
