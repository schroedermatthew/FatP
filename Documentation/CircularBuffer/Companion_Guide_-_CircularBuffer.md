---
doc_id: CG-CIRCULARBUFFER-001
doc_type: "Companion Guide"
title: "CircularBuffer"
fatp_components: ["CircularBuffer"]
topics: ["SPSC design", "lock-free architecture", "cache optimization", "index caching", "memory ordering", "wait-free algorithms", "false sharing prevention"]
constraints: ["cache coherency traffic", "false sharing", "memory ordering correctness", "bounded completion time", "power-of-2 sizing"]
cxx_standard: "C++17"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-18"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# **The Wait-Free Channel**

### *A Companion Guide to Fat-P's CircularBuffer*

---

**Scope:** This guide covers the design philosophy, architectural decisions, and implementation rationale behind CircularBuffer—Fat-P's wait-free SPSC queue with index caching optimization. It explains why lock-free matters for real-time systems, how cache-line alignment prevents false sharing, the mathematics behind index caching, and when the design's tradeoffs work against you.

**Not covered:**
- API reference and usage recipes (see User Manual - CircularBuffer)
- Benchmark methodology (see benchmark_CircularBuffer.cpp)
- General concurrency concepts (see Foundations - Concurrency)

**Prerequisites:**
- Understanding of producer-consumer patterns
- Familiarity with atomic operations and memory ordering
- Awareness of CPU cache hierarchy

---

## Companion Guide Card

**Component:** CircularBuffer  
**Design question:** How do you transfer data between threads with bounded completion time and minimal overhead?  
**Key tradeoff:** Fixed capacity (predictable) vs. dynamic growth (flexible)  
**Decision made:** Fixed power-of-2 capacity with index caching  
**Rejected alternatives:** Lock-based queue (unbounded latency), dynamic ring buffer (allocation in hot path), MPMC queue (unnecessary complexity for SPSC)  
**Historical context:** Audio engine requirements meet HPC cache optimization techniques

---

# **Table of Contents**

**[Introduction: Why This Component Exists](#introduction-why-this-component-exists)**

## Part I — The Problems

1. [The Mutex Ceiling](#chapter-1--the-mutex-ceiling)
2. [The False Sharing Tax](#chapter-2--the-false-sharing-tax)
3. [The Coherency Traffic Problem](#chapter-3--the-coherency-traffic-problem)
4. [The Modulo Bottleneck](#chapter-4--the-modulo-bottleneck)

## Part II — The Solutions

5. [Architecture Overview](#chapter-5--architecture-overview)
6. [Cache-Line Separation: Eliminating False Sharing](#chapter-6--cache-line-separation-eliminating-false-sharing)
7. [Index Caching: The 1.7× Optimization](#chapter-7--index-caching-the-17-optimization)
8. [Power-of-2 Masking: The 20× Micro-Optimization](#chapter-8--power-of-2-masking-the-20-micro-optimization)
9. [Memory Ordering: Correctness Without Locks](#chapter-9--memory-ordering-correctness-without-locks)

## Part III — Case Studies

10. [Case Study: Audio Engine Sample Pipeline](#chapter-10--case-study-audio-engine-sample-pipeline)
11. [Case Study: Trading System Order Flow](#chapter-11--case-study-trading-system-order-flow)
12. [Case Study: High-Throughput Logging](#chapter-12--case-study-high-throughput-logging)

## Part IV — Appendices

- [Appendix A — History of Lock-Free Queues](#appendix-a--history-of-lock-free-queues)
- [Appendix B — Why Not MPMC?](#appendix-b--why-not-mpmc)
- [Appendix C — Design Constraints and Rejected Alternatives](#appendix-c--design-constraints-and-rejected-alternatives)
- [Appendix D — Where CircularBuffer Loses](#appendix-d--where-circularbuffer-loses)
- [Appendix E — Further Reading](#appendix-e--further-reading)

---

# **Introduction: Why This Component Exists**

You're building an audio processing application. The audio capture callback runs at 48kHz—every 20.8 microseconds, it must deliver a sample to the processing thread. Miss a deadline, and the user hears a click.

The obvious solution is a mutex-protected queue:

```cpp
// THE TRAP: Mutex in audio callback
void audio_callback(float sample) {
    std::lock_guard lock(mutex_);
    queue_.push(sample);
}
```

This works in testing. Then you deploy to a user's machine running a busy system. The mutex occasionally takes 500 microseconds to acquire—24 missed deadlines. The audio crackles.

The problem isn't that mutexes are slow on average. They're fast—50 nanoseconds typically. The problem is they have no *bounded* completion time. When another thread holds the lock, you wait. When the kernel gets involved due to contention, you wait longer. Real-time systems can't tolerate "usually fast."

Or consider a different scenario: you've implemented a lock-free queue using compare-and-swap. It works correctly, but profiling shows unexpected overhead. Investigation reveals that your read and write indices share a cache line. Every time the producer updates the write index, it invalidates the consumer's cached copy of the read index. The consumer re-fetches the line, updates the read index, and invalidates the producer's cache. This "ping-pong" adds 100 nanoseconds per operation—more than the actual work.

Or this: you've separated the indices onto different cache lines, but you're still seeing cross-core traffic on every operation. The producer must read the consumer's read index to check if the buffer is full. The consumer must read the producer's write index to check if data is available. Each read crosses cores. Under high throughput, this coherency traffic limits performance.

CircularBuffer exists because real-time systems need all three problems solved simultaneously:

- **Wait-free operation:** Every push/pop completes in bounded time
- **False sharing prevention:** Cache-line aligned indices
- **Coherency traffic reduction:** Index caching

---

# **PART I — THE PROBLEMS**

---

# **CHAPTER 1 — The Mutex Ceiling**

### The Latency Distribution Problem

Mutex performance has two faces: the typical case and the worst case.

**Typical case (uncontended):** A mutex lock-unlock pair takes 20-50 nanoseconds. The implementation is a simple atomic exchange. No kernel involvement.

**Worst case (contended):** When another thread holds the mutex, you block. The kernel puts your thread to sleep. When the mutex releases, the kernel wakes you. This can take microseconds—sometimes tens of microseconds if the scheduler is busy.

For average-case workloads, mutexes are fine. For real-time workloads, the tail latency is catastrophic.

### Priority Inversion

Consider three threads with priorities High, Medium, and Low. Low acquires a mutex. High needs the same mutex and blocks. Medium, which doesn't need the mutex, preempts Low. Now High waits for Medium to finish so Low can run and release the mutex.

This is "priority inversion"—a high-priority thread waits for a lower-priority thread. Mars Pathfinder famously hit this bug in 1997, causing system resets.

Real-time operating systems have mitigations (priority inheritance), but they're complex and imperfect. The cleanest solution is avoiding locks entirely.

### The Wait-Free Alternative

A wait-free algorithm guarantees that every operation completes in a bounded number of steps, regardless of what other threads do. If the producer is stuck in a page fault, the consumer still completes its pop in bounded time.

CircularBuffer achieves wait-free operation through careful design:
- No locks
- No compare-and-swap retry loops
- Bounded work per operation

---

# **CHAPTER 2 — The False Sharing Tax**

### Cache Line Mechanics

Modern CPUs transfer memory in 64-byte cache lines. When Core 0 writes to any byte in a cache line, Core 1's copy of that line is invalidated—even if Core 1 was accessing a different byte in the same line.

```cpp
struct NaiveIndices {
    std::atomic<size_t> write_idx;  // Bytes 0-7
    std::atomic<size_t> read_idx;   // Bytes 8-15
    // Same cache line!
};
```

Producer updates `write_idx`. Consumer's entire cache line is invalidated. Consumer reads `read_idx` from invalidated line—cache miss, fetch from producer's cache. Consumer updates `read_idx`. Producer's line invalidated. Repeat.

### Measuring the Cost

**Without alignment (false sharing):**
```
Push+Pop: ~100 ns
```

**With cache-line alignment:**
```
Push+Pop: ~3-5 ns
```

The difference: 20-30×. False sharing converts memory operations into cross-core communication.

### The Solution

```cpp
alignas(64) std::atomic<size_t> write_idx_{0};  // Own cache line
alignas(64) std::atomic<size_t> read_idx_{0};   // Own cache line
```

Each index occupies a full cache line. Producer and consumer never invalidate each other's cached index.

---

# **CHAPTER 3 — The Coherency Traffic Problem**

### The Hidden Cross-Core Reads

Even with cache-line separation, every operation requires a cross-core read:

```cpp
bool push(T value) {
    size_t w = write_idx_.load();      // Local (producer owns this)
    size_t r = read_idx_.load();       // REMOTE! (consumer owns this)
    if ((w + 1) & mask_ == r)
        return false;  // Full
    // ...
}
```

The producer must read `read_idx_` to check if the buffer is full. This variable lives in the consumer's cache. Every push incurs a cross-core transfer: 40-100 cycles.

Symmetrically, every pop reads `write_idx_` from the producer's cache.

### Quantifying the Cost

At 1 billion operations per second (theoretical limit for trivial work), cross-core latency of 50ns means:

```
50ns × 2 (push + pop) = 100ns per item
Maximum throughput: 10M items/sec
```

With local operations only: potentially 300-500M items/sec.

### The Index Caching Solution

The insight: the producer doesn't need the *current* read index—it needs to know if the buffer is full. If it was not-full last time and the producer has only added one element, it might still be not-full.

```cpp
// Producer maintains a cached copy of read_idx_
size_t cached_read_idx_{0};

bool push(T value) {
    size_t w = write_idx_.load(relaxed);
    size_t next = (w + 1) & mask_;
    
    // Check cached value first (LOCAL)
    if (next == cached_read_idx_) {
        // Maybe full - check actual value (REMOTE)
        cached_read_idx_ = read_idx_.load(acquire);
        if (next == cached_read_idx_)
            return false;  // Actually full
    }
    
    buffer_[w] = std::move(value);
    write_idx_.store(next, release);
    return true;
}
```

The cross-core read only happens when the cached value indicates "maybe full." If the buffer is operating at steady state (neither empty nor full), the cache hit rate is high.

---

# **CHAPTER 4 — The Modulo Bottleneck**

### Division Is Expensive

Index wraparound traditionally uses modulo:

```cpp
next = (idx + 1) % capacity;
```

On x86, integer division takes 20-40 cycles. At high throughput, this adds up.

### Power-of-2 Optimization

When capacity is a power of 2, modulo becomes bitwise AND:

```cpp
// capacity = 1024 = 2^10
// mask = 1023 = 0x3FF
next = (idx + 1) & mask;  // 1 cycle
```

This is 20-40× faster per operation.

### The Capacity Trade-off

Power-of-2 sizing wastes some memory:

| Requested | Actual | Waste |
|-----------|--------|-------|
| 100 | 128 | 28% |
| 1000 | 1024 | 2.4% |
| 500 | 512 | 2.4% |
| 1023 | 1024 | 0.1% |

**Recommendation:** Request `2^N - 1` to minimize waste while keeping the power-of-2 internal size.

---

# **PART II — THE SOLUTIONS**

---

# **CHAPTER 5 — Architecture Overview**

CircularBuffer combines four techniques:

```cpp
template<typename T, size_t Capacity>
class CircularBuffer {
    // 1. Power-of-2 sizing
    static constexpr size_t kInternalCapacity = NextPowerOf2(Capacity + 1);
    static constexpr size_t kMask = kInternalCapacity - 1;
    
    // 2. Cache-line aligned indices (false sharing prevention)
    alignas(64) std::atomic<size_t> write_idx_{0};
    alignas(64) std::atomic<size_t> read_idx_{0};
    
    // 3. Index caching (coherency traffic reduction)
    alignas(64) size_t cached_read_idx_{0};   // Producer's cached copy
    alignas(64) size_t cached_write_idx_{0};  // Consumer's cached copy
    
    // 4. Heap-allocated buffer
    alignas(64) std::unique_ptr<T[]> buffer_;
};
```

Memory layout:

```
Offset 0:    write_idx_         (64 bytes, cache line 0)
Offset 64:   read_idx_          (64 bytes, cache line 1)
Offset 128:  cached_read_idx_   (64 bytes, cache line 2)
Offset 192:  cached_write_idx_  (64 bytes, cache line 3)
Offset 256:  buffer_ pointer    (64 bytes, cache line 4)
Heap:        T[kInternalCapacity]
```

Total metadata: 320 bytes. This is the cost of eliminating false sharing.

---

# **CHAPTER 6 — Cache-Line Separation: Eliminating False Sharing**

### The Alignment Strategy

Each variable that might be accessed by different threads gets its own cache line:

```cpp
alignas(64) std::atomic<size_t> write_idx_{0};   // Producer writes, consumer reads
alignas(64) std::atomic<size_t> read_idx_{0};    // Consumer writes, producer reads
alignas(64) size_t cached_read_idx_{0};          // Producer only
alignas(64) size_t cached_write_idx_{0};         // Consumer only
```

The `alignas(64)` specifier forces 64-byte alignment. Since each variable is smaller than 64 bytes, they don't share cache lines.

### Verification

You can verify alignment at runtime:

```cpp
CircularBuffer<int, 1024> buffer;
assert(reinterpret_cast<uintptr_t>(&buffer.write_idx_) % 64 == 0);
assert(reinterpret_cast<uintptr_t>(&buffer.read_idx_) % 64 == 0);
```

---

# **CHAPTER 7 — Index Caching: The 1.7× Optimization**

### The Mathematics

Let's model the cost savings.

**Without caching:**
- Every push: 1 local read (`write_idx_`) + 1 remote read (`read_idx_`) + 1 local write
- Every pop: 1 local read (`read_idx_`) + 1 remote read (`write_idx_`) + 1 local write

**With caching:**
- Every push: 1 local read + 1 local check + 1 local write + (occasionally) 1 remote read
- Every pop: 1 local read + 1 local check + 1 local write + (occasionally) 1 remote read

The "occasional" remote read happens when the cache indicates potential full/empty. If the buffer operates at 50% capacity, the producer's cached read index is typically stale enough that the buffer isn't actually full, and vice versa.

### Cache Hit Rate Analysis

| Buffer Fill Level | Producer Cache Hits | Consumer Cache Hits |
|-------------------|--------------------|--------------------|
| 0% (empty) | 100% | 0% |
| 25% | ~95% | ~75% |
| 50% | ~90% | ~90% |
| 75% | ~75% | ~95% |
| 100% (full) | 0% | 100% |

At steady state (50% fill), both caches hit ~90% of the time. Each cache hit saves ~50ns of cross-core latency.

### Measured Impact

Benchmark: 10M push+pop operations, two threads, buffer capacity 1024

| Configuration | Time | Throughput |
|--------------|------|------------|
| No caching | 850ms | 11.8M ops/sec |
| With caching | 500ms | 20M ops/sec |

Speedup: **1.7×**

---

# **CHAPTER 8 — Power-of-2 Masking: The 20× Micro-Optimization**

### Implementation

```cpp
static constexpr size_t NextPowerOf2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

static constexpr size_t kInternalCapacity = NextPowerOf2(Capacity + 1);
static constexpr size_t kMask = kInternalCapacity - 1;

size_t next_index(size_t idx) {
    return (idx + 1) & kMask;  // Bitwise AND, not modulo
}
```

### Why Capacity + 1?

A ring buffer needs one "empty" slot to distinguish full from empty. If the user requests capacity 1000, we need 1001 slots internally. NextPowerOf2(1001) = 1024.

### Compiler Output

```asm
; Modulo version
mov  eax, edx
inc  eax
xor  edx, edx
div  ecx       ; 20-40 cycles

; Bitwise AND version
lea  eax, [rdx+1]
and  eax, ecx  ; 1 cycle
```

---

# **CHAPTER 9 — Memory Ordering: Correctness Without Locks**

### The Synchronization Problem

Without locks, we need memory ordering to ensure the consumer sees valid data:

1. Producer writes data to buffer
2. Producer updates write index
3. Consumer reads write index
4. Consumer reads data from buffer

If (2) and (1) are reordered, the consumer might see the updated index but stale data.

### Release-Acquire Semantics

```cpp
// Producer
buffer_[write] = std::move(value);                  // A
write_idx_.store(next, std::memory_order_release);  // B

// Consumer
size_t w = write_idx_.load(std::memory_order_acquire);  // C
T value = std::move(buffer_[read]);                     // D
```

The `release` store (B) synchronizes with the `acquire` load (C). This establishes:
- All writes before B (including A) are visible after C
- The consumer sees valid data

### Relaxed Where Possible

```cpp
// Producer reading its own write index - can be relaxed
size_t w = write_idx_.load(std::memory_order_relaxed);

// Producer reading cached index - not even atomic
if (next == cached_read_idx_) { ... }
```

Only cross-thread synchronization points need acquire/release.

---

# **PART III — CASE STUDIES**

---

# **CHAPTER 10 — Case Study: Audio Engine Sample Pipeline**

### The Requirements

- Sample rate: 48kHz (20.8μs per sample)
- Latency budget: 10μs for queue operations
- Failure mode: Audio glitches on deadline miss

### The Solution

```cpp
// Audio capture callback (real-time thread)
CircularBuffer<AudioSample, 4095> sample_buffer;

void audio_callback(const float* samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!sample_buffer.push(samples[i])) {
            // Buffer full - this shouldn't happen with proper sizing
            log_error("Audio buffer overflow");
        }
    }
}

// Processing thread (normal priority)
void process_audio() {
    while (running) {
        if (auto sample = sample_buffer.pop()) {
            apply_effects(*sample);
            output_sample(*sample);
        } else {
            std::this_thread::yield();
        }
    }
}
```

### Why CircularBuffer?

- **Wait-free:** Callback completes in bounded time even if processing thread is preempted
- **Low latency:** ~1ns per push, well under 10μs budget
- **No allocation:** Fixed capacity, no heap operations in callback

---

# **CHAPTER 11 — Case Study: Trading System Order Flow**

### The Requirements

- Message rate: 100,000/second
- Latency: P99 < 10μs
- Zero message loss

### The Solution

```cpp
CircularBuffer<Order, 8191> order_queue;

// Network thread
void on_order_received(const Order& order) {
    while (!order_queue.push(order)) {
        // Back-pressure: signal upstream to slow down
        signal_back_pressure();
        std::this_thread::yield();
    }
}

// Strategy thread
void process_orders() {
    while (running) {
        if (auto* order = order_queue.front()) {
            if (validate_order(*order)) {
                auto o = order_queue.pop();
                execute_order(*o);
            } else {
                order_queue.pop();  // Discard invalid
            }
        }
    }
}
```

### Why CircularBuffer?

- **Predictable latency:** No mutex contention spikes
- **front() for validation:** Inspect before committing
- **Explicit back-pressure:** Bounded queue forces flow control

---

# **CHAPTER 12 — Case Study: High-Throughput Logging**

### The Requirements

- Log rate: 1M entries/second (burst)
- Sustained: 100K entries/second
- Background thread writes to disk

### The Solution

```cpp
CircularBuffer<LogEntry, 65535> log_buffer;

// Application threads (single designated logger)
void log(Level level, const char* msg) {
    LogEntry entry{level, timestamp(), msg};
    if (!log_buffer.emplace(level, timestamp(), msg)) {
        atomic_fetch_add(&dropped_logs, 1);
    }
}

// Writer thread
void log_writer() {
    std::ofstream file("app.log");
    while (running || !log_buffer.empty()) {
        if (auto entry = log_buffer.pop()) {
            file << format(*entry) << "\n";
        } else {
            file.flush();
            std::this_thread::sleep_for(1ms);
        }
    }
}
```

### Why CircularBuffer?

- **emplace():** Format directly into buffer
- **No blocking:** Application never waits for disk
- **Graceful degradation:** Count drops rather than block

---

# **APPENDIX A — History of Lock-Free Queues**

### 1990s: The Lamport Queue

Leslie Lamport described the basic SPSC queue in 1983. It used volatile variables (pre-C++11 memory model) and assumed sequential consistency.

### 2000s: Memory Model Awareness

With multi-core processors, programmers discovered that volatile wasn't enough. Memory barriers and atomic operations became necessary.

### 2008: Bounded MPMC Queues

Dmitry Vyukov's bounded MPMC queue showed how to achieve high performance with compare-and-swap.

### 2011: C++11 Memory Model

The C++11 standard formalized memory ordering, enabling portable lock-free code.

### Today: Specialized Queues

Modern systems use specialized queues for specific patterns. SPSC queues like CircularBuffer optimize for the common single-producer single-consumer case.

---

# **APPENDIX B — Why Not MPMC?**

Multi-producer multi-consumer queues are more general but have costs:

| Aspect | SPSC | MPMC |
|--------|------|------|
| Push complexity | O(1) wait-free | O(1) lock-free (with CAS retry) |
| Memory overhead | Minimal | Per-slot state for coordination |
| Cache behavior | Predictable | Contention on head/tail |

If you know you have exactly one producer and one consumer, SPSC is simpler and faster.

---

# **APPENDIX C — Design Constraints and Rejected Alternatives**

### Hard Constraints

1. **Wait-free operations:** Real-time systems require bounded completion
2. **Zero allocation in hot path:** No malloc/free in push/pop
3. **No external dependencies:** Header-only, standard library only

### Rejected Alternatives

| Alternative | Why Rejected |
|-------------|--------------|
| Lock-based queue | Unbounded latency |
| Dynamic ring buffer | Allocation in hot path |
| MPMC queue | Unnecessary complexity for SPSC |
| Blocking semantics | Incompatible with real-time |

---

# **APPENDIX D — Where CircularBuffer Loses**

**Multiple producers or consumers:** Use LockFreeQueue or mutex-based queue.

**Unbounded capacity:** Use std::queue with mutex if growth is required.

**Blocking semantics needed:** Use condition_variable wrapper.

**Very large elements:** Consider pointer indirection to avoid copying.

**Memory must shrink:** CircularBuffer has fixed capacity; destroy and recreate.

---

# **APPENDIX E — Further Reading**

**"Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms"**  
Maged M. Michael, Michael L. Scott, 1996

**"Writing Lock-Free Code: A Corrected Queue"**  
Herb Sutter, Dr. Dobb's Journal, 2008

**C++ Concurrency in Action, 2nd Edition**  
Anthony Williams, 2019

**What Every Programmer Should Know About Memory**  
Ulrich Drepper, 2007

---

*End of Companion Guide*

*CircularBuffer.h — Fat-P Library*
