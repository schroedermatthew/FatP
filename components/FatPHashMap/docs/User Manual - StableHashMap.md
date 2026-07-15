---
doc_id: UM-STABLEHASHMAP-001
doc_type: "User Manual"
title: "StableHashMap"
fatp_components: ["StableHashMap"]
topics: ["hash map API", "insert methods", "heterogeneous lookup", "load factor tuning", "block allocator", "hash mixing", "migration from std::unordered_map"]
constraints: ["reference stability requirements", "rehash costs", "allocator strategy", "exception boundaries", "debug-mode enforcement"]
cxx_standard: "C++20"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-09"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# User Manual - StableHashMap

*Updated December 2025*



**Scope:** Complete usage guide for `fat_p::StableHashMap<K, V>`: pointer-stable open-addressing hash map with node indirection, insertion, lookup, erasure, iteration, and comparison with alternatives.

**Not covered:**
- SIMD-accelerated hash map without stability (see FastHashMap)
- Concurrent hash maps
- Hash function design

**Prerequisites:** C++20; understanding of hash table basics; awareness of pointer stability requirements (references remain valid across insert/erase)

---

## User Manual Card

**Component:** StableHashMap
**Primary use case:** Hash map where pointers and references to values remain valid across insertions and erasures
**Integration pattern:** Use where code holds `Value*` or `Value&` that must survive mutations; if you need a read-only frozen mode, that lives on FastHashMap (`freeze()`), not here
**Key API:** `StableHashMap<K, V>`, `.insert()`, `.find()`, `.erase()`, `.insert_or_assign()`, `.try_emplace()`, `.operator[]()`, `.contains()`
**std equivalent:** None
**Common mistakes:** Using StableHashMap when pointer stability isn't needed (FastHashMap is faster); assuming the map is thread-safe (it's not; synchronize externally)
**Performance notes:** Node indirection adds one pointer dereference per access vs FastHashMap. See `components/FatPHashMap/results/` for current data

---
## Table of Contents

1. [The Hash Table Story](#the-hash-table-story)
2. [A Brief History of High-Performance Hashing](#a-brief-history-of-high-performance-hashing)
3. [Understanding Hash Map Architectures](#understanding-hash-map-architectures)
4. [The Design Space](#the-design-space)
5. [The Control Byte Insight](#the-control-byte-insight)
6. [Reference Stability: The Node-Based Advantage](#reference-stability-the-node-based-advantage)
7. [Getting Started](#getting-started)
8. [The Insert Dilemma: Three Methods, Three Philosophies](#the-insert-dilemma-three-methods-three-philosophies)
9. [Finding Values: Why Pointers Beat Iterators](#finding-values-why-pointers-beat-iterators)
10. [The Block Allocator: Eliminating Malloc Overhead](#the-block-allocator-eliminating-malloc-overhead)
11. [Hash Quality: Protecting Against Weak Hashes](#hash-quality-protecting-against-weak-hashes)
12. [Load Factor: The Central Tradeoff](#load-factor-the-central-tradeoff)
13. [Heterogeneous Lookup: Avoiding Temporary Objects](#heterogeneous-lookup-avoiding-temporary-objects)
14. [Benchmarking StableHashMap](#benchmarking-stablehashmap)
15. [When to Use StableHashMap (and When Not To)](#when-to-use-stablehashmap-and-when-not-to)
16. [Migration from std::unordered_map](#migration-from-stdunordered_map)
17. [Troubleshooting](#troubleshooting)
18. [API Reference](#api-reference)
19. [Summary](#summary)

---

## The Hash Table Story

### The Idea That Changed Computing

In 1953, Hans Peter Luhn at IBM filed an internal memorandum describing a technique for storing and retrieving records by their content rather than their location. The idea was elegant: compute a number from the record's key, use that number as an array index, and store the record there. Retrieval was instant--no searching required.

This technique became known as *hashing*, and it's arguably the most important data structure invention of the 20th century. Every database index, every compiler symbol table, every web browser cache, every cryptocurrency uses hash tables at their core.

The elegance is mathematical: if you can distribute keys uniformly across array slots, every operation--insert, find, delete--takes O(1) time. Not O(log n) like balanced trees. Not O(n) like linear search. Constant time, regardless of how many elements you have.

But there's a catch.

### The Collision Problem

Two different keys can hash to the same slot. This is inevitable--if you have more possible keys than slots (and you always do), collisions must occur. The pigeonhole principle guarantees it.

Consider hashing strings to a million slots. There are infinitely many possible strings but only a million slots. Collisions aren't a bug; they're a mathematical certainty. The question is: what do you do when they happen?

Computer scientists developed two fundamentally different answers, and this choice shapes everything about a hash table's performance.

### Two Schools of Thought

**Separate Chaining (1953):** Store colliding elements in a linked list hanging off each slot. The slot becomes a "bucket" pointing to a chain of entries.

```
Bucket 0 --> Alice --> Bob --> null
Bucket 1 --> null
Bucket 2 --> Charlie --> null
Bucket 3 --> null
```

When you look up a key, you hash to its bucket, then walk the chain comparing keys until you find a match or reach the end. Insertion appends to the chain. Deletion removes from the chain.

This is what `std::unordered_map` uses. It accommodates high collision rates gracefully--chains grow longer as needed. It never needs to move elements once inserted; they stay in their nodes forever. The standard library chose it because it provides strong iterator and reference stability guarantees.

**Open Addressing (1954):** Store everything directly in the array. When a collision occurs, probe for another slot--check slot+1, then slot+2, etc., until you find an empty one.

```
Slot 0: Alice (ideal position: 0)
Slot 1: Bob   (ideal position: 0, displaced by 1)
Slot 2: Charlie (ideal position: 2)
Slot 3: empty
```

When you look up a key, you hash to its ideal slot, then probe forward until you find the key or an empty slot (meaning the key doesn't exist). Insertion probes until finding an empty slot. Deletion is more complex--we'll cover this later.

Open addressing keeps all data in a contiguous array. No linked lists, no per-element heap allocations. Everything lives together in memory.

### Why Open Addressing Won in HPC

For decades, separate chaining dominated. It was taught in textbooks, implemented in standard libraries, used in production systems worldwide. But around 2010, something changed: CPUs got fast, but memory got *relatively* slow.

The first commercial microprocessors in the 1970s ran at under 1 MHz. Memory was roughly the same speed--a few cycles to fetch data. By 2020, CPUs run at 4+ GHz, but main memory still takes roughly 100 nanoseconds to access. That's 400 clock cycles of waiting.

Modern processors execute billions of instructions per second, but fetching data from main memory costs hundreds of cycles. The CPU spends most of its time waiting. The solution is caching--keeping recently accessed data in small, fast memories close to the processor.

Here's where hash table architecture matters enormously:

**Separate chaining destroys cache locality.** Each element lives in its own heap allocation. The memory allocator returns addresses scattered across your address space. The chain might look like:

```
Bucket 5 at address 0x1000
  --> Node at 0x5F28 (Alice)
        --> Node at 0x28A0 (Bob)
              --> Node at 0xC410 (Charlie)
                    --> null
```

Following this chain means fetching from four random memory locations. Each fetch potentially misses the CPU cache and waits for main memory. Four cache misses means 400+ cycles of pure waiting.

**Open addressing preserves cache locality.** All elements live in a contiguous array. When you access slot N, the CPU doesn't just fetch that slot--it fetches an entire *cache line*, typically 64 bytes. Slots N+1, N+2, N+3 come along for free. Probing nearby slots costs almost nothing; the data is already in cache.

```
Entry array at 0x1000:
  Slot 0 at 0x1000
  Slot 1 at 0x1020  (same cache line or next)
  Slot 2 at 0x1040  (likely in cache)
  Slot 3 at 0x1060  (likely in cache)
```

Google's internal benchmarks in 2017 showed open addressing 2-3x faster than `std::unordered_map` for typical workloads. Facebook's F14 hash table confirmed similar results. The academic debate was settled by engineering reality.

---

## A Brief History of High-Performance Hashing

### The Academic Foundation

Open addressing wasn't new in 2017. The technique dates to 1954, and researchers had studied it extensively. The classic probing strategies were well understood:

**Linear probing:** Check slot h, h+1, h+2, ... Straightforward and cache-friendly, but prone to clustering. Occupied slots tend to clump together, and clusters grow over time because new keys falling into a cluster make it larger.

**Quadratic probing:** Check slot h, h+1, h+4, h+9, ... The increasing gaps reduce clustering but can miss empty slots (quadratic probing only visits half the table).

**Double hashing:** Check slot h, h+k, h+2k, ... where k is a second hash. Distributes probes well but requires computing two hash functions.

**Robin Hood hashing (1986):** Pedro Celis's doctoral thesis introduced a clever idea: when inserting, if your probe distance exceeds the occupant's probe distance, swap with them. This equalizes probe distances across all elements, bounding the worst case.

Each strategy had tradeoffs. Linear probing was cache-friendly but suffered clustering. Robin Hood bounded worst cases but added per-probe bookkeeping. None stood out as clearly superior.

### The Memory Hierarchy Revelation

What changed in the 2000s was understanding of the memory hierarchy. Researchers at Google, Facebook, and elsewhere began measuring hash tables not by operation counts but by cache misses and memory bandwidth.

The findings were surprising: Robin Hood's bookkeeping--storing and comparing probe distances--cost more than it saved. The extra memory accesses to read distance metadata overwhelmed the benefit of shorter probes. Theoretical improvements didn't translate to faster wall-clock time.

Meanwhile, CPU architects added increasingly powerful vector instructions. SSE, then AVX, then AVX2 gave programmers 128-bit and 256-bit registers that could operate on multiple data elements simultaneously. These SIMD (Single Instruction, Multiple Data) capabilities went largely unused in hash table implementations.

### The Google Innovation: Swiss Tables

In 2017, Google engineers Matt Kulukundis and Sam Benzaquen presented a new hash table design at CppCon. They called it "Swiss Tables"--a playful name referencing Swiss cheese, which is full of holes (empty slots).

The key insight was elegant but revolutionary: **use SIMD instructions to check multiple slots in parallel**.

Instead of probing slots one at a time:

```
Traditional probe sequence:
  Slot 5: occupied, compare key... no match
  Slot 6: occupied, compare key... no match
  Slot 7: occupied, compare key... no match
  Slot 8: empty --> key not found
  (3 key comparisons, 4 memory accesses)
```

Swiss Tables check 16 slots simultaneously:

```
Swiss Table probe:
  Group 0: SIMD compare against 16 slots in ONE instruction
           Matches at positions 3, 7
           Compare key at position 3... no match
           Compare key at position 7... found!
  (2 key comparisons, 1 SIMD operation)
```

The SIMD approach requires some infrastructure--a metadata array storing hash fingerprints, careful memory alignment, specific group sizes--but the payoff is enormous. Miss lookups, which must prove a key doesn't exist, become dramatically faster. Instead of probing through N occupied slots one by one, you scan groups of 16 in a single instruction.

### The Proliferation

Google released Swiss Tables as `absl::flat_hash_map` in their Abseil library. The design quickly influenced other implementations:

**Rust's hashbrown (2018):** The Rust ecosystem adopted a Swiss Table implementation as its default hash map. Amanieu d'Antras ported the design and optimized it for Rust's ownership semantics.

**boost::unordered (2022):** Boost's unordered containers were rewritten to use Swiss Table internals while maintaining API compatibility. Peter Dimov led the effort, creating both flat (values inline) and node (values in separate allocations) variants.

**Facebook's F14 (2019):** Facebook developed their own SIMD-accelerated hash table with different tradeoffs, optimizing for their specific workloads.

**folly::F14FastMap:** Another Facebook variant focused on raw speed over memory efficiency.

The common thread: SIMD-accelerated metadata scanning became the standard approach for high-performance hash tables. The technique was too effective to ignore.

### The Standard's Stagnation

While the broader ecosystem moved to SIMD-accelerated designs, `std::unordered_map` remained frozen.

The problem is ABI stability. Major compilers have committed to not breaking binary compatibility between releases. If `std::unordered_map`'s internal structure changed, code compiled with old compilers couldn't link against libraries compiled with new compilers. This would be catastrophic for the C++ ecosystem.

`std::unordered_map` uses separate chaining. Its nodes are heap-allocated. Its bucket array stores pointers. These details are baked into the ABI. Switching to open addressing would break every library that exposes `std::unordered_map` in its interface.

The committee considered proposals for new containers (`std::flat_map` in C++23, potentially `std::hive` in C++26), but no SIMD-accelerated hash map is on the horizon. The standard library will likely never have one--the ABI stability requirements are too constraining.

This is why third-party hash maps exist. StableHashMap, along with Abseil, Boost, and others, fills the gap that the standard cannot.

---

## Understanding Hash Map Architectures

### Memory Layout: The Critical Difference

To understand why different hash tables perform differently, you need to see how they're actually laid out in memory.

**std::unordered_map (separate chaining):**

```
Bucket array (allocated as contiguous memory):
  [ptr] [ptr] [ptr] [ptr] [ptr] [ptr] ...
    |     |     |
    v     |     v
  Node    |   Node
  K=5     |   K=18
  V=...   |   V=...
    |     v
    v   Node --> Node --> null
  null  K=9     K=2
        V=...   V=...
          |
          v
        null

Node allocations (scattered across heap):
  0x1A20: Node{K=5, V=..., next=null}
  0x3F80: Node{K=9, V=..., next=0x28C0}
  0x28C0: Node{K=2, V=..., next=null}
  0x7100: Node{K=18, V=..., next=null}
```

The bucket array is contiguous, but each element is a heap-allocated node at an unpredictable address. Traversing a chain means following pointers to random locations.

**Flat hash maps (absl::flat_hash_map):**

```
Control bytes (16 per group):
  [H2|H2|__|H2|__|H2|__|__|H2|__|__|__|__|__|__|__]
  
Slot array (directly stores key-value pairs):
  [K0,V0] [K1,V1] [_____] [K2,V2] [_____] [K3,V3] ...

Everything is contiguous. No pointers. No separate allocations.
```

Keys and values live directly in the slot array. When you find a matching control byte, the corresponding slot contains the actual data. This is maximally cache-efficient.

**StableHashMap (node-based Swiss Table):**

```
Control bytes (16 per group):
  [H2|H2|__|H2|__|H2|__|__|H2|__|__|__|__|__|__|__]

Slot array (stores pointers to nodes):
  [ptr] [ptr] [null] [ptr] [null] [ptr] [null] ...
    |     |           |           |
    v     v           v           v
  Node  Node        Node        Node
  K0,V0 K1,V1       K2,V2       K3,V3

Node allocations (may be scattered, or in blocks):
  0x2000: Node{K0, V0}
  0x2040: Node{K1, V1}  (adjacent if block-allocated)
  0x2080: Node{K2, V2}
  0x20C0: Node{K3, V3}
```

The control bytes and slot array are contiguous, enabling SIMD scanning. But slots contain pointers, not data. The actual key-value pairs live in separate node allocations.

This hybrid design sacrifices some cache efficiency (one extra pointer dereference) to preserve pointer stability. When the table rehashes, nodes don't move--only the slot array is rebuilt.

### The Cache Line Effect

Modern CPUs don't fetch individual bytes from memory; they fetch *cache lines*--typically 64 bytes at a time. When you access any byte in a cache line, the entire line is loaded into cache.

This has profound implications for hash table design:

**Control byte arrays are cache-friendly.** A 16-byte control array fits in one quarter of a cache line. Scanning it with SIMD costs one memory access for 16 slots.

**Node chasing is cache-hostile.** Following a pointer to a node at an arbitrary address likely misses the cache. Each node access costs ~100 cycles on a cache miss.

**Block-allocated nodes improve locality.** If nodes are allocated in contiguous blocks, nearby nodes share cache lines. Iteration becomes dramatically faster.

This is why StableHashMap offers a Block allocator--it addresses the locality problem inherent in node-based storage.

---

## The Design Space

### The Fundamental Choices

Implementing a high-performance hash table requires answering several design questions. Different libraries make different choices, and understanding these tradeoffs helps you evaluate whether StableHashMap is right for your use case.

### Probing Strategy: Linear vs. Quadratic vs. Robin Hood

When a collision occurs, where do you look next?

**Linear probing:**

```cpp
size_t probe(size_t hash, size_t attempt, size_t table_size) {
    return (hash + attempt) % table_size;
}
// Sequence: h, h+1, h+2, h+3, ...
```

Cache-friendly because consecutive probes hit consecutive memory locations, maximizing cache utilization. But linear probing suffers from *primary clustering*: occupied slots tend to form contiguous runs. Once a cluster forms, new keys falling into it make it longer, and longer clusters attract more keys.

At high load factors (>0.7), linear probing clusters degrade lookup performance significantly.

**Quadratic probing:**

```cpp
size_t probe(size_t hash, size_t attempt, size_t table_size) {
    return (hash + attempt * attempt) % table_size;
}
// Sequence: h, h+1, h+4, h+9, h+16, ...
```

The increasing gaps reduce clustering. But quadratic probing has a problem: it doesn't visit all slots. For a table of size N, quadratic probing visits at most N/2 distinct slots. If those slots are all occupied, insertion fails even with empty slots elsewhere.

Practical implementations constrain table sizes to powers of 2 and use a modified sequence that guarantees visiting all slots.

**Robin Hood probing:**

```cpp
// During insertion, track "probe distance" (how far from ideal position)
// If new element's distance > existing element's distance, swap them
void insert(Key k, Value v) {
    size_t pos = hash(k) % size;
    size_t dist = 0;
    while (occupied(pos)) {
        if (dist > distance[pos]) {
            swap(k, keys[pos]);
            swap(v, values[pos]);
            swap(dist, distance[pos]);
        }
        pos = (pos + 1) % size;
        dist++;
    }
    keys[pos] = k;
    values[pos] = v;
    distance[pos] = dist;
}
```

Robin Hood equalizes probe distances across all elements. The variance of probe distances is minimized, bounding worst-case lookups. In theory, this enables early termination during lookup: if your probe distance exceeds the stored distance, the key can't exist.

In practice, the extra bookkeeping (storing and comparing distances) costs more than it saves on modern CPUs. The memory access to read the distance metadata often dominates.

**Swiss Table probing:**

Swiss Tables use a form of quadratic probing over *groups* of 16 slots:

```cpp
size_t probe_group(size_t hash, size_t attempt, size_t num_groups) {
    // Triangular number sequence: 0, 1, 3, 6, 10, 15, ...
    return (hash/16 + attempt * (attempt + 1) / 2) % num_groups;
}
```

Within each group, SIMD scans all 16 slots simultaneously. The quadratic sequence between groups avoids primary clustering while the 16-wide SIMD scan amortizes the probing overhead.

This combination--SIMD within groups, quadratic between groups--is what makes Swiss Tables fast.

### Storage Layout: Flat vs. Node

Where do you store the actual key-value pairs?

**Flat storage (absl::flat_hash_map):**

```cpp
template <typename K, typename V>
class FlatHashMap {
    uint8_t* control_;           // Control bytes
    std::pair<K, V>* slots_;     // Directly stores pairs
    // ...
};
```

Keys and values live directly in the slot array. Finding a match gives you the data immediately--no extra indirection.

**Advantages:** Maximum cache efficiency. Finding a key means you've already loaded the value. No pointer chasing.

**Disadvantages:** When the table grows, all key-value pairs are copied to new locations. Any pointer to a value becomes dangling:

```cpp
absl::flat_hash_map<int, LargeObject> map;
map[1] = obj;
LargeObject* ptr = &map[1];  // Get pointer

map[2] = obj2;  // Might trigger rehash...

*ptr = modified;  // UNDEFINED BEHAVIOR if rehash occurred!
```

**Node storage (std::unordered_map, StableHashMap):**

```cpp
template <typename K, typename V>
class NodeHashMap {
    uint8_t* control_;           // Control bytes
    Node<K,V>** slots_;          // Stores pointers to nodes
    // ...
};
```

Slots contain pointers to separately-allocated nodes. Finding a match requires one extra dereference to access the data.

**Advantages:** Node addresses never change. Pointers to values remain valid across insertions and rehashes. Iterator stability is preserved.

**Disadvantages:** Extra indirection on every access (though usually just one L1 cache hit). Per-node allocation overhead unless using a block allocator.

### StableHashMap's Choice: Node-Based Swiss Tables

StableHashMap combines Swiss Table's SIMD-accelerated probing with node-based storage. This gives you:

- SIMD-parallel metadata scanning for fast lookups
- Pointer stability across all mutations (except erasing the pointed-to element)
- The option of block allocation to recover node locality

The tradeoff is one pointer dereference per access. Benchmarks show this costs roughly 2-5ns compared to flat storage. For most applications, this is negligible; for latency-critical code paths, flat tables may be preferable if pointer stability isn't needed.

### Comparing Implementations

| Feature | std::unordered | absl::flat | absl::node | boost::flat | boost::node | **StableHashMap** |
|---------|----------------|------------|------------|-------------|-------------|-------------------|
| Probing | Chaining | Swiss | Swiss | Swiss | Swiss | **Swiss** |
| Storage | Node | Flat | Node | Flat | Node | **Node** |
| SIMD | No | Yes | Yes | Yes | Yes | **Yes** |
| Reference stability | Yes | No | Yes | No | Yes | **Yes** |
| Block allocator | No | N/A | No | N/A | No | **Yes** |
| Hash mixer | No | Yes | Yes | Yes | Yes | **Yes** |
| Dependencies | STL | Abseil | Abseil | Boost | Boost | **None** |

StableHashMap occupies a specific niche: SIMD-accelerated Swiss Table performance with node-based reference stability and zero external dependencies. If you need pointer stability without taking a dependency on Abseil or Boost, StableHashMap is your option.

---

## The Control Byte Insight

### How SIMD Hash Tables Actually Work

The magic of Swiss Tables lies in the control byte array. Understanding this mechanism explains both the performance characteristics and the implementation constraints.

### The H2 Fingerprint

When you insert a key, Swiss Tables compute a full hash (typically 64 bits), then split it:

```cpp
uint64_t hash = hash_function(key);
size_t h1 = hash >> 7;           // Upper 57 bits: determines group
uint8_t h2 = hash & 0x7F;        // Lower 7 bits: fingerprint (0-127)
```

The H2 fingerprint is stored in the control byte array. It acts as a Bloom filter for each slot--before comparing the actual key, you check if the fingerprint matches.

Why 7 bits? Because control bytes reserve values 128-255 for metadata:

| Control Value | Meaning |
|---------------|---------|
| 0x00 - 0x7F | Occupied; value is H2 fingerprint |
| 0x80 | Empty slot (never been used) |
| 0xFE | Deleted (tombstone) |
| 0xFF | Sentinel (end of table) |

With 128 possible H2 values, a random occupied slot has only a 1/128 ≈ 0.78% chance of matching your search key's H2. This means ~99% of occupied slots are eliminated without ever comparing keys.

### The SIMD Comparison

The control byte array is organized into groups of 16 bytes--exactly one SSE register (128 bits) or half an AVX register.

```cpp
// Conceptual SIMD lookup
void find(Key key) {
    uint64_t hash = hash_function(key);
    size_t group_idx = (hash >> 7) % num_groups;
    uint8_t h2 = hash & 0x7F;
    
    while (true) {
        // Load 16 control bytes into SIMD register
        __m128i ctrl = _mm_load_si128(&control[group_idx * 16]);
        
        // Broadcast H2 to all 16 positions
        __m128i h2_vec = _mm_set1_epi8(h2);
        
        // Compare all 16 in parallel (ONE instruction!)
        __m128i matches = _mm_cmpeq_epi8(ctrl, h2_vec);
        
        // Extract match positions as bitmask
        uint32_t mask = _mm_movemask_epi8(matches);
        
        // Check each match
        while (mask) {
            int pos = __builtin_ctz(mask);  // Position of lowest set bit
            if (slots[group_idx * 16 + pos].key == key)
                return &slots[group_idx * 16 + pos].value;
            mask &= mask - 1;  // Clear lowest bit
        }
        
        // Check for empty slot (key definitely not in table)
        __m128i empty_vec = _mm_set1_epi8(0x80);
        __m128i empties = _mm_cmpeq_epi8(ctrl, empty_vec);
        if (_mm_movemask_epi8(empties))
            return nullptr;  // Found empty; key doesn't exist
        
        // Move to next group (quadratic probing)
        group_idx = next_group(group_idx);
    }
}
```

The critical operations are `_mm_cmpeq_epi8` (compare 16 bytes in parallel) and `_mm_movemask_epi8` (extract comparison results as a bitmask). These execute in 1-3 cycles on modern CPUs. Scanning 16 slots costs about the same as comparing one key.

### Why This Is Fast

Consider looking up a key in a table with 1 million elements at 70% load factor:

**Traditional open addressing:**
```
Average probe length ≈ 1.7 (theoretical for linear probing at 0.7 load)
Each probe: load slot, compare key
Total: ~3.4 memory accesses, ~1.7 key comparisons
```

**Swiss Table:**
```
Groups to check: typically 1-2
Per group: load 16 control bytes, SIMD compare, check 0-2 matches
Total: 1-2 cache line loads, 0-2 key comparisons
```

For successful lookups, the numbers are similar. The real difference appears in **miss lookups**--searching for keys that don't exist.

**Traditional miss lookup:**
```
Must probe until finding an empty slot
At 70% load, average ~3.3 probes
Each probe loads and compares a key
```

**Swiss Table miss lookup:**
```
SIMD scan finds empties in the same pass as matches
If any empty exists in the group, done
Typically 1-2 groups, 0 key comparisons (H2 mismatches)
```

Miss lookups are dramatically faster because the SIMD scan detects empty slots for free. Most misses terminate without comparing any keys at all.

### StableHashMap's Implementation

StableHashMap implements the Swiss Table algorithm with AVX2 on x86-64:

- 16-slot groups aligned to 16-byte boundaries
- H2 fingerprints in the low 7 bits of control bytes
- SIMD matching via `_mm_cmpeq_epi8` and `_mm_movemask_epi8`
- Quadratic probing between groups using triangular numbers
- Empty detection integrated into the match loop

On platforms without AVX2, StableHashMap falls back to a portable scalar implementation that emulates the SIMD operations with bitwise tricks. Performance is reduced but still competitive with traditional hash tables.

---

## Reference Stability: The Node-Based Advantage

### Why Pointer Stability Matters

Consider this seemingly innocent code:

```cpp
std::unordered_map<std::string, Config> configs;
configs["database"] = load_database_config();
configs["cache"] = load_cache_config();

Config* db_config = &configs["database"];

// ... later, add more configs ...
configs["logging"] = load_logging_config();
configs["metrics"] = load_metrics_config();

// Use the stored pointer
db_config->connection_string;  // Is this safe?
```

With `std::unordered_map`, this is guaranteed to work correctly. The standard mandates that references and pointers to elements remain valid as long as the element isn't erased.

Now try the same with `absl::flat_hash_map`:

```cpp
absl::flat_hash_map<std::string, Config> configs;
configs["database"] = load_database_config();
Config* db_config = &configs["database"];

configs["logging"] = load_logging_config();  // Might rehash!

db_config->connection_string;  // UNDEFINED BEHAVIOR
```

The insertion might trigger a rehash. When a flat hash map rehashes, it allocates a new, larger slot array and copies all elements to it. The old slot array is deallocated. Your pointer now points to freed memory.

This isn't a bug in Abseil--it's documented behavior. Flat storage is fundamentally incompatible with pointer stability. The speed comes from storing data directly in the array; the instability comes from the same source.

### Real-World Patterns That Need Stability

Many common programming patterns rely on pointer stability:

**Graph structures with node maps:**

```cpp
struct Graph {
    std::unordered_map<NodeId, Node> nodes;
    
    void connect(NodeId a, NodeId b) {
        Node* node_a = &nodes[a];
        Node* node_b = &nodes[b];
        
        // This pattern is safe with std::unordered_map
        // but crashes with flat_hash_map if nodes rehashes
        node_a->neighbors.push_back(node_b);
        node_b->neighbors.push_back(node_a);
    }
    
    void add_node(NodeId id, Node node) {
        nodes[id] = std::move(node);  // Might rehash!
    }
};
```

If `add_node` triggers a rehash during `connect`, the pointers become dangling.

**Observer patterns:**

```cpp
class EventSystem {
    std::unordered_map<EventId, std::vector<Callback>> handlers;
    
    void fire(EventId id) {
        auto* callbacks = find_callbacks(id);  // Get pointer
        
        // Firing might register new handlers...
        for (auto& cb : *callbacks) {
            cb();  // Callback might call register_handler()
        }
    }
    
    void register_handler(EventId id, Callback cb) {
        handlers[id].push_back(cb);  // Might rehash!
    }
};
```

**Index structures:**

```cpp
class Database {
    std::unordered_map<Id, Record> records;
    std::vector<Record*> sorted_view;  // Pointers into records
    std::unordered_map<Field, std::vector<Record*>> indices;
    
    void insert(Id id, Record r) {
        records[id] = std::move(r);  // Invalidates ALL indices!
        rebuild_indices();  // Expensive!
    }
};
```

With flat tables, every insertion potentially invalidates every index. You must rebuild all secondary structures after each insert, or accept the risk of corruption.

### StableHashMap's Guarantee

StableHashMap provides `std::unordered_map`-equivalent stability:

**Pointers to values remain valid** after:
- `insert()` (even if rehash occurs)
- `insert_or_assign()` (even if rehash occurs)
- `try_emplace()` (even if rehash occurs)
- `erase()` of *other* elements
- `operator[]` on *other* keys
- `rehash()` and `reserve()`

**Pointers become invalid** after:
- `erase()` of the pointed-to element
- `clear()`
- Destruction of the map

This matches `std::unordered_map` exactly. Code that works correctly with `std::unordered_map` will work correctly with StableHashMap.

### How Node Storage Provides Stability

The mechanism is straightforward. When you insert an element:

1. StableHashMap allocates a new node on the heap (or from a block allocator)
2. The key-value pair is stored in the node
3. A pointer to the node is stored in the slot array

When rehashing occurs:

1. A new, larger slot array is allocated
2. Control bytes are recomputed for the new table size
3. **Node pointers are copied** to their new positions
4. The old slot array is deallocated
5. **Nodes themselves don't move**

The indirection through pointers is what provides stability. Pointers to nodes remain valid because nodes never move. Only the slot array--which stores pointers, not data--is rebuilt.

### The Cost of Stability

Node storage adds overhead:

**Memory:** Each element requires a separate allocation (node) plus a pointer in the slot array. This adds 8 bytes per element on 64-bit systems, plus per-allocation overhead (typically 16-32 bytes).

**Latency:** Every access requires dereferencing the slot pointer to reach the node. This is typically one L1 cache hit (~4 cycles) but can be more if the node isn't cached.

**Allocation:** Standard node-based maps allocate each node individually. One million insertions means one million `malloc()` calls. This is why StableHashMap offers a Block allocator.

For most applications, these costs are acceptable. The stability guarantee simplifies code and prevents subtle bugs. For latency-critical paths where every nanosecond matters and pointer stability isn't needed, flat tables are faster.

---

## Getting Started

### Prerequisites and Integration

StableHashMap requires C++20 and has no dependencies beyond the standard library. It's a single header file:

```cpp
#include "StableHashMap.h"
```

For optimal performance, enable AVX2 instructions:

```bash
# GCC
g++ -std=c++20 -O2 -mavx2 program.cpp -o program

# Clang
clang++ -std=c++20 -O2 -mavx2 program.cpp -o program

# MSVC
cl /std:c++20 /O2 /arch:AVX2 program.cpp
```

Without AVX2, StableHashMap uses a portable scalar fallback. Performance is reduced but the API is identical.

### Your First StableHashMap

```cpp
#include "StableHashMap.h"
#include <iostream>
#include <string>

int main()
{
    // Create a map from strings to integers
    fat_p::StableHashMap<std::string, int> ages;
    
    // Insert entries (multiple methods available)
    ages.insert("Alice", 30);
    ages["Bob"] = 25;
    ages.insert_or_assign("Charlie", 35);
    
    // Find returns a pointer (nullptr if missing)
    if (int* age = ages.find("Alice"))
    {
        std::cout << "Alice is " << *age << " years old\n";
    }
    
    // Check existence
    if (!ages.find("Dave"))
    {
        std::cout << "Dave is not in the map\n";
    }
    
    // Iterate over all entries
    std::cout << "All people:\n";
    for (auto [name, age] : ages)
    {
        std::cout << "  " << name << ": " << age << "\n";
    }
    
    // Demonstrate reference stability
    int* alice_age = ages.find("Alice");
    
    // Add many more entries (triggers multiple rehashes)
    for (int i = 0; i < 10000; ++i)
    {
        ages["person_" + std::to_string(i)] = i;
    }
    
    // The pointer is still valid!
    std::cout << "Alice is still " << *alice_age << "\n";
    
    return 0;
}
```

### Understanding the Template Parameters

```cpp
template<typename Key,
         typename Value,
         typename Hash = std::hash<Key>,
         typename KeyEqual = std::equal_to<Key>,
         template<typename> class NodeAllocator = fat_p::NewDeleteAllocator>
class StableHashMap;
```


---

## The Insert Dilemma: Three Methods, Three Philosophies

### Why So Many Insert Methods?

StableHashMap provides three ways to add elements: `insert()`, `insert_or_assign()`, and `try_emplace()`. This seems redundant. Why not just one?

The answer involves a fundamental design question: **what happens when you insert a key that already exists?**

Different use cases want different answers:

**Configuration management:** "Update the setting if it exists, add it if it doesn't." You want upsert semantics--overwrite existing values.

**Caching:** "Use the cached value if present, only compute if missing." You don't want to overwrite; the existing value is the one you want.

**Counting:** "Increment if exists, start at 1 if new." You need to modify the existing value, not replace it.

**Deduplication:** "Only insert truly new items." Duplicates should be ignored entirely.

No single behavior fits all cases. Rather than choose wrong for half your users, StableHashMap gives you explicit control.

### insert(): Insert-Only, No Overwrite

```cpp
template<typename K, typename V>
std::pair<Value*, bool> insert(K&& key, V&& value);
```

`insert()` adds the key-value pair only if the key is **missing**. If the key exists, it does nothing:

```cpp
fat_p::StableHashMap<std::string, int> map;
map.insert("x", 1);
auto [ptr, inserted] = map.insert("x", 2);  // inserted == false
std::cout << *map.find("x");  // Prints 1 (original value)
```

**Use insert when:** You're building a set of unique entries, implementing a cache where first-in wins, or deduplicating data.

This matches `std::unordered_map::insert()` semantics--duplicates are silently ignored.

### insert_or_assign(): The Upsert Operation

```cpp
std::pair<Value*, bool> insert_or_assign(const Key& k, Value&& v);
```

`insert_or_assign()` always succeeds. If the key exists, the value is **overwritten**:

```cpp
fat_p::StableHashMap<std::string, int> config;
config.insert_or_assign("timeout", 30);
config.insert_or_assign("timeout", 60);  // Updates!
std::cout << *config.find("timeout");  // Prints 60
```

The return value indicates what happened:
- `{pointer, true}`: New key inserted
- `{pointer, false}`: Existing key updated

**Use insert_or_assign when:** You're setting configuration values, updating state, or implementing "last writer wins" semantics.

### try_emplace(): Conditional In-Place Construction

```cpp
template<typename K, typename... Args>
std::pair<Value*, bool> try_emplace(K&& key, Args&&... args);
```

`try_emplace()` constructs the value in-place from the provided arguments, and only if the key is **missing**. (There is no separate `emplace()`; for overwrite semantics, use `insert_or_assign()`.)

```cpp
fat_p::StableHashMap<std::string, ExpensiveObject> cache;

// ExpensiveObject is only constructed if "key" is missing
auto [ptr, inserted] = cache.try_emplace("key", arg1, arg2, arg3);
if (!inserted) {
    // Key existed, ExpensiveObject was NOT constructed
    // We saved the construction cost
}
```

**Use try_emplace when:** Construction is expensive and you want to avoid it for existing keys. This is the standard library-compatible "emplace if missing" operation.

### Decision Guide

| Scenario | Method | Why |
|----------|--------|-----|
| Add if missing, ignore duplicates | `insert()` | No overwrite, matches std::unordered_map |
| Set value, replace if exists | `insert_or_assign()` | Upsert semantics |
| Construct in-place only if missing | `try_emplace()` | Avoid construction for existing keys |
| Increment/modify existing value | `operator[]` | Returns reference for modification |

---

## Finding Values: Why Pointers Beat Iterators

### The Standard Library Design

`std::unordered_map::find()` returns an iterator:

```cpp
std::unordered_map<std::string, int> map;
map["key"] = 42;

auto it = map.find("key");
if (it != map.end())
{
    std::cout << it->second;  // Access via iterator
}
```

Iterators are powerful--they support increment, comparison, and work with STL algorithms. But for the common case of "find a value by key," they're verbose.

### StableHashMap's Pointer Design

StableHashMap's `find()` returns a pointer:

```cpp
fat_p::StableHashMap<std::string, int> map;
map["key"] = 42;

int* ptr = map.find("key");
if (ptr)
{
    std::cout << *ptr;  // Direct access
}
```

**Why pointers?**

**Cleaner conditional logic.** Pointers are nullable; the null check is idiomatic C++. No need to compare against a sentinel `end()`.

**Direct value access.** You get `Value*`, not `std::pair<const Key&, Value&>*`. No `.second` extraction needed.

**Stability semantics.** The returned pointer remains valid across future insertions. This is the whole point of node-based storage.

**Familiar idiom.** Every C++ programmer knows how to check for null. Iterator validity rules are more obscure.

### When You Need Iterators

StableHashMap still provides iterators for range-based iteration and algorithm compatibility:

```cpp
// Range-based for loop
for (auto [key, value] : map)
{
    std::cout << key << ": " << value << "\n";
}

// Iterator loop
for (auto it = map.begin(); it != map.end(); ++it)
{
    std::cout << it->first << ": " << it->second << "\n";
}
```

For single-element lookup, pointers are more direct.

### Const Correctness

`find()` on a non-const map returns `Value*`. On a const map, it returns `const Value*`:

```cpp
fat_p::StableHashMap<int, int> map;
map[1] = 100;

// Non-const: can modify
int* ptr = map.find(1);
*ptr = 200;  // OK

// Const: can only read
const auto& cmap = map;
const int* cptr = cmap.find(1);
// *cptr = 300;  // ERROR: cannot modify through const pointer
```

---

## The Block Allocator: Eliminating Malloc Overhead

### The Per-Node Allocation Problem

Standard node-based maps allocate each node individually:

```cpp
map[1] = data;  // malloc() for node 1
map[2] = data;  // malloc() for node 2
map[3] = data;  // malloc() for node 3
// ... 
map[1000000] = data;  // malloc() for node 1000000
```

One million insertions means one million `malloc()` calls. Each call has overhead:

**Allocator bookkeeping:** The allocator maintains free lists, size classes, and metadata. Each allocation updates these structures, adding overhead per call.

**Lock contention:** In multi-threaded code, threads compete for allocator locks. At high concurrency, this becomes a bottleneck.

**Memory fragmentation:** Over time, many small allocations leave gaps in the heap. The allocator spends more time searching for free space.

**Cache pollution:** Allocator metadata occupies cache lines that could hold your data. Each allocation touches bookkeeping structures, potentially evicting useful data from cache.

At scale, the overhead is substantial. Inserting one million elements spends significant time in pure allocator overhead.

### How Block Allocation Works

The Block allocator pre-allocates nodes in contiguous chunks:

```cpp
// Conceptual implementation
template <typename Node>
class BlockAllocator {
    static constexpr size_t BLOCK_SIZE = 1024;
    
    struct Block {
        Node nodes[BLOCK_SIZE];
        size_t next_free = 0;
        Block* next_block = nullptr;
    };
    
    Block* current_block_;
    
    Node* allocate() {
        if (current_block_->next_free >= BLOCK_SIZE) {
            // Allocate new block
            current_block_->next_block = new Block();
            current_block_ = current_block_->next_block;
        }
        return &current_block_->nodes[current_block_->next_free++];
    }
    
    void deallocate(Node* node) {
        // Mark as free for reuse (details omitted)
    }
};
```

Instead of calling `malloc()` for every node, the Block allocator:

1. Allocates a large block (e.g., 1024 nodes) at once
2. Returns slots from the block for individual allocations
3. Only calls `malloc()` when the current block is exhausted

One million insertions now requires ~1000 `malloc()` calls instead of one million.

### Memory Layout

```
Block 0: [Node0][Node1][Node2]...[Node1023]  <- contiguous
           ^      ^      ^
           |      |      |
         slot 5  slot 12 slot 7   (non-contiguous logical order)

Block 1: [Node1024][Node1025]...             <- contiguous
```

Nodes within a block are contiguous in memory. When iterating, nearby nodes likely share cache lines. This improves iteration performance significantly.

### Performance Impact

The Block allocator improves performance across all operations through two mechanisms:

**Insert:** Block allocation amortizes `malloc()` overhead across ~1000 nodes. Instead of one system allocation per insert, nodes are carved from pre-allocated contiguous blocks.

**Find:** Block-allocated nodes have better cache locality. Nodes allocated together are stored contiguously, so finding a node is more likely to hit cache.

**Erase:** Deallocation in the Block allocator is O(1) bookkeeping (returning a slot to the free list), not a `free()` call to the system allocator.

**Iteration:** Contiguous node storage means sequential memory access. The prefetcher can stay ahead of the access pattern.

See `components/FatPHashMap/results/` for current platform-specific benchmark data comparing standard and Block allocator performance.

### When to Use Block Allocation

**Use Block allocation when:**
- Inserting many elements (allocation overhead is amortized)
- Erase-heavy workloads (O(1) deallocation)
- Iterating frequently (cache-friendly layout)
- Multi-threaded insertion (reduces allocator contention)

**Use standard allocation when:**
- Map lifetime is short (block overhead isn't amortized)
- Memory must be returned immediately on erase
- You need compatibility with custom per-object allocators
- Map is small (<1000 elements)

### Usage

```cpp
// Standard allocation (default)
fat_p::StableHashMap<int, Data> standard;

// Block allocation
fat_p::StableHashMap<int, Data, std::hash<int>, std::equal_to<int>, fat_p::BlockAllocator> fast;

// Both have identical APIs
fast[1] = data;
Data* ptr = fast.find(1);
```

---

## Hash Quality: Protecting Against Weak Hashes

### The std::hash Problem

Many `std::hash` implementations are surprisingly weak. On MSVC, `std::hash<int>` is often the identity function:

```cpp
// MSVC
std::hash<int>{}(1) == 1
std::hash<int>{}(2) == 2
std::hash<int>{}(1000000) == 1000000
```

This seems reasonable--integers are their own best hash, right? Wrong.

Consider inserting sequential keys into a hash table with 1024 buckets:

```cpp
for (int i = 0; i < 10000; ++i)
    map[i] = data;  // Keys 0, 1, 2, 3, ...
```

With identity hashing:
- Keys 0-1023 hash to buckets 0-1023 (filling evenly)
- Keys 1024-2047 hash to buckets 0-1023 again (now 2 per bucket)
- Keys 2048-3071 hash to the same buckets (3 per bucket)

After 10,000 insertions, each bucket has ~10 elements. Lookups probe through these clusters.

Even worse, the table grows (rehashes) when load factor is exceeded. The new bucket count is typically 2x the old. With identity hashing, the same clustering pattern repeats--keys just redistribute to different clusters of the same size.

### The SplitMix64 Mixer

StableHashMap applies a SplitMix64 finalizer to all hashes by default:

```cpp
inline uint64_t splitmix64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
```

SplitMix64 is a bijective function--every input maps to a unique output. It has excellent *avalanche* properties: changing any input bit changes approximately half the output bits.

```
splitmix64(0) = 0x9e3779b97f4a7c15
splitmix64(1) = 0x85ebca77c2b2ae63
splitmix64(2) = 0xc2b2ae3d27d4eb4f
splitmix64(3) = 0xd4eb4f0f6d8a7c5e  (approximately)
```

Sequential integers produce seemingly random hashes. The clustering problem disappears.

### Benchmark Impact

The mixer's impact depends on the quality of the underlying hash function. On platforms where `std::hash<int>` is an identity function (MSVC), the mixer eliminates clustering and significantly improves probe sequences. On platforms where `std::hash` already includes some mixing (GCC's libstdc++), the additional mixer is neutral.

See `components/FatPHashMap/results/` for platform-specific measurements of mixer impact.

### Opting Out

If your hash function already has excellent avalanche properties (cryptographic hashes, well-designed custom hashes), the mixer adds unnecessary overhead.

Mark your hash as "already mixed" with the `is_avalanching` marker:

```cpp
struct MyExcellentHash {
    using is_avalanching = void;  // Marker: skip built-in mixer
    
    size_t operator()(int x) const {
        return my_well_designed_hash(x);
    }
};

fat_p::StableHashMap<int, Data, MyExcellentHash> map;
```

**Rule of thumb:** Unless you've verified your hash has good distribution with your actual keys, keep the mixer enabled. The ~2ns overhead is negligible compared to the clustering penalty.

---

## Load Factor: The Central Tradeoff

### Understanding Load Factor

Load factor is the ratio of elements to slots:

```
load_factor = size / bucket_count
```

At load factor 0.5, half the slots are occupied. At 0.9, 90% are full.

### The Tradeoff

Higher load factor means:
- **Less memory** (more elements per slot)
- **Longer probes** (more collisions, more groups to scan)

Lower load factor means:
- **More memory** (empty slots waste space)
- **Shorter probes** (fewer collisions, faster lookups)

### StableHashMap's Default: 0.80

StableHashMap uses a maximum load factor of 0.80 by default. When size/bucket_count exceeds 0.80, the table rehashes to roughly double its size.

```cpp
fat_p::StableHashMap<int, int> map;
std::cout << map.max_load_factor();  // 0.8
```

0.80 is a reasonable balance for most workloads. Lookups are fast (typically 1-2 groups), and memory overhead is acceptable (20% empty slots).

### Tuning Load Factor

You can adjust the threshold:

```cpp
// Faster lookups, more memory
fat_p::StableHashMap<int, int> conservative(0, 0.5f);  // 50% max load
conservative.reserve(1000000);

// Slower lookups, less memory
fat_p::StableHashMap<int, int> dense(0, 0.9f);         // 90% max load
dense.reserve(1000000);
```

| Load Factor | Find (ns) | Insert (ns) | Memory Overhead |
|-------------|-----------|-------------|-----------------|
| 0.50 | ~6 | ~14 | +60% |
| **0.80** | **~9** | **~17** | **+25%** |
| 0.90 | ~12 | ~25 | +11% |

### Pre-sizing for Known Workloads

If you know how many elements you'll insert, pre-allocate to avoid rehashing:

```cpp
fat_p::StableHashMap<int, Data> map;
map.reserve(1000000);  // Pre-allocate for 1M elements

for (int i = 0; i < 1000000; ++i)
    map[i] = data;  // No rehashing during loop
```

Avoiding rehashes during bulk insertion eliminates O(n) element relocation per growth event. See `components/FatPHashMap/results/` for current platform-specific benchmark data.

---


## Heterogeneous Lookup: Avoiding Temporary Objects

### The Problem

Consider a map with string keys:

```cpp
StableHashMap<std::string, int> map;
map["hello"] = 1;

// Later, look up with a string_view
std::string_view key = get_key_from_network();
int* val = map.find(std::string(key));  // Temporary string!
```

Every lookup constructs a temporary `std::string` just to search. For high-frequency lookups, this overhead adds up.

### The Solution

StableHashMap supports heterogeneous lookup--finding with any type that can be compared to and hashed like the key:

```cpp
StableHashMap<std::string, int> map;
map["hello"] = 1;

std::string_view key = get_key_from_network();
int* val = map.find(key);  // No allocation!
```

The lookup uses `string_view` directly. No temporary `std::string` is constructed.

### Performance Impact

Heterogeneous lookup eliminates the cost of constructing a temporary `std::string` on every lookup. For string-keyed maps, this avoids a heap allocation and copy per query. The savings are proportional to key length and lookup frequency—for workloads that perform frequent view-based or `const char*` lookups, the overhead elimination is significant.

See `components/FatPHashMap/results/` for measured impact across different map sizes and key types.

### Built-in Support

StableHashMap enables heterogeneous lookup by default for:
- `std::string` keys with `std::string_view` lookup
- `std::string` keys with `const char*` lookup

For custom types, provide a transparent hash and equality:

```cpp
struct TransparentHash {
    using is_transparent = void;  // Marker
    
    size_t operator()(const MyKey& k) const { return hash(k); }
    size_t operator()(const MyKeyView& v) const { return hash(v); }
};

struct TransparentEqual {
    using is_transparent = void;  // Marker
    
    bool operator()(const MyKey& a, const MyKey& b) const;
    bool operator()(const MyKey& a, const MyKeyView& b) const;
    bool operator()(const MyKeyView& a, const MyKey& b) const;
};
```

---

## Benchmarking StableHashMap

### Methodology

All benchmarks were performed on:
- Windows 11
- Intel processor with 3686 MHz base frequency
- CPU throttled to ~65% during sustained tests (thermal management)
- Compiled with MSVC, /O2 /arch:AVX2
- Round-robin execution with randomized test order
- 3 warmup runs + 15 measured runs per test
- Medians reported (robust to outliers)

### Core Operations (N=1,000,000)

Benchmarks compare StableHashMap (with and without Block allocator) against `std::unordered_map`, `boost::unordered_node_map`, and `absl::node_hash_map` across insert, find, miss, and erase operations. All competitors are node-based (reference-stable) for fair comparison.

**What the benchmarks show:**

- **Insert:** The Block allocator variant significantly outperforms all competitors by amortizing allocation overhead across contiguous blocks instead of calling `malloc()` per node.
- **Find (hit):** SIMD-accelerated metadata probing reduces key comparisons. Performance is competitive with boost and substantially faster than `std::unordered_map`.
- **Miss (not found):** boost's group layout achieves fewer candidate key comparisons per miss, giving it an edge on miss-heavy workloads.
- **Erase:** Block allocator's O(1) deallocation (returning a slot to the free list) dramatically outperforms system `free()` calls.

See `components/FatPHashMap/results/` for current platform-specific benchmark data with exact timings.

### Miss Performance Analysis

Miss lookups—searching for keys that don't exist—reveal SIMD efficiency differences. StableHashMap and boost both use SIMD metadata probing, but boost's 15-slot group layout and different probing sequence achieve fewer candidate key comparisons per miss. For miss-heavy workloads, boost has an edge.

### Where StableHashMap Wins

**Reference stability + SIMD:** The only zero-dependency option combining both

**Insert-heavy workloads:** Block allocator amortizes allocation overhead across contiguous blocks, eliminating per-node `malloc()` calls

**Weak hash protection:** Built-in mixer prevents clustering from sequential integer keys

**Zero external dependencies:** Single header, STL only

### Where StableHashMap Loses

**Miss-heavy workloads:** boost's group layout achieves fewer key comparisons per miss

**No stability needed:** Flat maps (e.g., `absl::flat_hash_map`) avoid per-node allocation entirely and are faster overall when pointer stability is not required

**Standard erase without Block allocator:** Per-node `free()` calls are expensive; use the Block allocator variant for erase-heavy workloads

---

## When to Use StableHashMap (and When Not To)

### Use StableHashMap When:

**You store pointers to map values:**
```cpp
Graph g;
Node* n = &g.nodes[id];
// ... many insertions later ...
n->process();  // Must remain valid
```

**Insert-heavy workloads with Block allocator:**
```cpp
StableHashMap<int, Data, std::hash<int>, std::equal_to<int>, fat_p::BlockAllocator> map;
for (int i = 0; i < 1000000; ++i)
    map[i] = compute(i);  // Block allocator amortizes allocation overhead
```

**Zero external dependencies required:**
```cpp
// Single header, no Boost, no Abseil
#include "StableHashMap.h"
```

**Sequential integer keys:**
```cpp
for (int i = 0; i < N; ++i)
    map[i] = data;  // Built-in mixer prevents clustering
```

### Don't Use StableHashMap When:

**Pointer stability not needed:**
```cpp
// Flat maps are faster if you don't need stability
absl::flat_hash_map<int, Data> map;
```

**Miss-heavy workloads:**
```cpp
// boost's group layout achieves fewer key comparisons per miss
boost::unordered_node_map<int, Data> map;
```

**Very small maps (N < 100):**
```cpp
// Setup overhead dominates
std::unordered_map<int, Data> map;  // Fine for small N
```

---

## Migration from std::unordered_map

### Drop-in Compatible Operations

Most code works with minimal changes:

```cpp
// Before
std::unordered_map<std::string, int> map;
map["key"] = value;
map.insert({"key2", value2});
map.erase("key");
map.size();
for (auto& [k, v] : map) { ... }

// After
fat_p::StableHashMap<std::string, int> map;
map["key"] = value;
map.insert("key2", value2);  // Note: different signature
map.erase("key");
map.size();
for (auto [k, v] : map) { ... }  // Note: no &
```

### Critical Differences

**1. find() returns pointer, not iterator**

```cpp
// std::unordered_map
auto it = map.find(key);
if (it != map.end()) {
    use(it->second);
}

// StableHashMap
int* ptr = map.find(key);
if (ptr) {
    use(*ptr);
}
```

**2. No emplace(); use try_emplace() or insert_or_assign()**

```cpp
// std::unordered_map: emplace ignores duplicate
map.emplace(1, "first");
map.emplace(1, "second");  // Ignored!
// map[1] == "first"

// StableHashMap: no emplace(); try_emplace() gives the same
// "construct in place, ignore duplicate" behavior
map.try_emplace(1, "first");
map.try_emplace(1, "second");  // Ignored!
// map[1] == "first"
```

If you actually want overwrite-on-duplicate, use `insert_or_assign()`.

**3. No bucket interface**

```cpp
// std::unordered_map
size_t bucket = map.bucket(key);
for (auto it = map.begin(bucket); it != map.end(bucket); ++it) { ... }

// StableHashMap: not available (internal structure differs)
```

**4. Iterator dereference differs**

```cpp
// std::unordered_map: pair<const Key, Value>&
for (auto& [k, v] : std_map) { ... }

// StableHashMap: structured binding, not reference
for (auto [k, v] : stable_map) { ... }
```

---

## Troubleshooting

### Compilation Errors

**"static assertion failed: Key must be DefaultConstructible"**

Your key or value type doesn't have a default constructor:

```cpp
struct NoDefault {
    NoDefault(int x);  // No default constructor
};
StableHashMap<int, NoDefault> map;  // Error!

// Solutions:
// 1. Add default constructor
struct Fixed {
    Fixed() = default;
    Fixed(int x);
};

// 2. Wrap in optional
StableHashMap<int, std::optional<NoDefault>> map;
```

**"no matching function for call to 'find'"**

Your lookup type isn't compatible with heterogeneous lookup:

```cpp
StableHashMap<std::string, int> map;
map.find(some_custom_type);  // Error!

// Solution: convert to key type or provide transparent hash
map.find(std::string(some_custom_type));
```

### Runtime Errors


### Performance Issues

**Insert slower than expected**

Are you using the Block allocator?

```cpp
// Slow: per-node allocation
StableHashMap<int, Data> map;

// Fast: block allocation
StableHashMap<int, Data, std::hash<int>, std::equal_to<int>, fat_p::BlockAllocator> map;
```

**Find slower than expected**

Check load factor and hash quality:

```cpp
// If you want earlier rehashes, configure max load factor at construction.
// (This StableHashMap version does not provide a runtime setter.)
fat_p::StableHashMap<Key, Value> map(expected_size, 0.7f);

// Pre-size if you know the count
map.reserve(expected_size);
```

**Miss lookups slow**

Miss lookups are inherently slower than hits (must prove non-existence). If miss-heavy, consider `boost::unordered_node_map`.

---

## API Reference

This section reflects the current `StableHashMap` API as implemented in `StableHashMap.h`.

### Construction

| Signature | Description |
|-----------|-------------|
| `StableHashMap()` | Empty map. Default max load factor is 0.80. |
| `explicit StableHashMap(size_t initial_capacity)` | Allocate a table with at least `initial_capacity` slots (power-of-two rounded). |
| `StableHashMap(size_t initial_capacity, float max_load_factor)` | Same, with max load factor configured at construction (`0 < lf <= 1`). |
| `StableHashMap(size_t initial_capacity, float max_load_factor, const Hash& hash)` | Same, with custom hash functor. |
| `StableHashMap(size_t initial_capacity, float max_load_factor, const Hash& hash, const KeyEqual& equal)` | Same, with custom hash and equality. |

### Lookup and access

| Method | Returns | Notes |
|--------|---------|-------|
| `find(key)` | `Value*` / `const Value*` | Returns `nullptr` if missing. Supports heterogeneous lookup when hash/equal are transparent. |
| `contains(key)` | `bool` | True if key exists. |
| `count(key)` | `size_t` | 0 or 1. |
| `operator[](key)` | `Value&` | Inserts default value on miss. Requires `Value` default-constructible. |

### Modifiers

| Method | Returns | Notes |
|--------|---------|-------|
| `insert(k, v)` | `std::pair<Value*, bool>` | Pointer to value; bool indicates inserted (`true`) vs existing (`false`). |
| `insert_or_assign(k, v)` | `std::pair<Value*, bool>` | Inserts or assigns; bool indicates inserted (`true`) vs assigned (`false`). |
| `try_emplace(k, args...)` | `std::pair<Value*, bool>` | Constructs `Value(args...)` only if key is missing. |
| `erase(key)` | `bool` | True if erased. Invalidates pointers/references to the erased value only. |
| `clear()` | `void` | Clears all elements. |

### Capacity and configuration

| Method | Description |
|--------|-------------|
| `size()` / `empty()` | Element count queries |
| `capacity()` | Slot count (power of two) |
| `load_factor()` | `size / capacity` |
| `max_load_factor()` | Configured threshold (construction-time) |
| `reserve(n)` | Ensure capacity for `n` elements without exceeding `max_load_factor()` |
| `get_allocator()` | Access the node allocator instance |

### Iteration

```cpp
for (auto [key, value] : map) { ... }           // yields a pair by value
for (auto it = map.begin(); it != map.end(); ++it) { ... }

// Prefer these if you want references (no pair copy):
it.key();
it.value();
```

## Summary

StableHashMap is a SIMD-accelerated Swiss Table with node-based storage, combining cache-efficient lookups with reference stability.

**Key architectural choices:**

- **SIMD metadata scanning:** 16-slot groups with AVX2 parallel comparison
- **Node-based storage:** Pointers stable across all mutations except erase
- **Block allocator option:** Amortizes allocation overhead across contiguous blocks, significantly improving insert and erase throughput
- **Built-in hash mixer:** Protects against weak std::hash implementations
- **Heterogeneous lookup:** Zero-allocation string_view finds

**Performance characteristics:**

StableHashMap's SIMD-accelerated probing and Block allocator provide substantial improvements over `std::unordered_map` across insert, find, and erase operations. See `components/FatPHashMap/results/` for current platform-specific benchmark data.

**Use StableHashMap for:**
- Code requiring pointer/reference stability
- Insert-heavy workloads with Block allocator
- Zero-dependency requirements
- Integer keys with weak std::hash

**Use alternatives when:**
- Pointer stability unnecessary (flat maps avoid per-node allocation entirely)
- Miss-heavy workloads (boost's group layout achieves fewer key comparisons per miss)
- Very small maps (overhead not amortized)

---

*StableHashMap.h (1025 lines) -- Fat-P Library*