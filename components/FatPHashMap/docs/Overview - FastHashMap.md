---
doc_id: OV-FASTHASHMAP-001
doc_type: "Overview"
title: "FastHashMap Overview"
fatp_components: ["FastHashMap"]
topics: ["hash map", "SIMD probing", "Swiss Table"]
constraints: ["cache efficiency", "probe sequences"]
cxx_standard: "C++17"
last_verified: "2026-01-11"
audience: ["C++ developers", "AI assistants"]
status: "reviewed"
---
# FastHashMap: Maximum Throughput Hash Table

*Fat-P Library — December 2025*

---

## Executive Summary

FastHashMap is a SIMD-accelerated hash table that delivers 3-5x insert throughput and 7-10x erase throughput compared to `std::unordered_map`. It achieves this through Swiss Table architecture: parallel slot comparison using AVX2/SSE2/NEON instructions, configurable deletion policies, and an optional fixed-buffer allocator for embedded systems. Unlike node-based hash tables, FastHashMap stores key-value pairs directly in slots—maximizing cache locality at the cost of pointer stability.

---

## The Problem Domain

### What Goes Wrong Without It

Consider a game engine that builds a transform lookup table every frame:

```cpp
void render_frame(const Scene& scene) {
    std::unordered_map<EntityId, Transform> transforms;
    
    for (const auto& entity : scene.entities()) {
        transforms[entity.id] = entity.transform();
    }
    
    for (const auto& [id, transform] : transforms) {
        render_entity(id, transform);
    }
}
```

This code spends 15% of every frame in hash table operations. The problem isn't the algorithm—it's the data structure. Each `transforms[entity.id] = ...` potentially calls `malloc()`. With 10,000 entities at 60 FPS, that's 600,000 allocations per second, each one contending for heap locks in parallel workloads.

The lookup cost compounds the allocation overhead. `std::unordered_map` uses separate chaining: each bucket contains a linked list of entries. Finding an entry requires following 1-3 pointers on average, and each pointer dereference is a potential cache miss. Modern CPUs fetch 64-byte cache lines; chasing a pointer to read 8 bytes wastes 87% of each memory transaction.

Meanwhile, the CPU's SIMD units sit idle. The standard library was designed before SIMD became ubiquitous. It examines slots one at a time, leaving 31 of your 32 AVX2 lanes unused.

### The Standard's Limitation

`std::unordered_map` prioritizes iterator and reference stability over throughput. Insert a new element, and all existing iterators and references remain valid. This guarantee requires separate heap allocation for each entry—the very thing that kills performance.

Even C++20 and C++23 don't address this. The standard's ABI stability requirements prevent adopting modern hash table designs. Your compiler vendor cannot ship a Swiss Table without breaking binary compatibility with every library compiled against older standard library versions.

---

## Architecture: Swiss Table with Flat Storage

FastHashMap implements Google's Swiss Table design, published in 2017. The key insight: instead of linked lists per bucket, store all entries in a flat array. Use a separate control byte array to track which slots are occupied. When searching, load 16-32 control bytes into a SIMD register and compare them all at once.

The control array uses one byte per slot. Occupied slots store an H2 fingerprint—the top 7 bits of the hash value with the high bit set (values 0x80-0xFF). Empty slots contain 0x00. Deleted slots (with the tombstone deletion policy) contain 0x7E.

The H2 fingerprint acts as a Bloom filter for each slot. With 128 possible values, a random occupied slot has only a 0.78% chance of matching your search key's H2. This eliminates approximately 99% of false-positive key comparisons before touching the actual keys.

The SIMD lookup proceeds as follows. First, compute the hash and extract the H2 fingerprint. Then compute the starting group position from the hash. Load 32 control bytes (on AVX2) into a SIMD register and broadcast the target H2 to all 32 positions. A single `_mm256_cmpeq_epi8` instruction compares all 32 slots simultaneously. The resulting bitmask indicates which slots might contain the target key. For each matching position, compare the actual key. If no slots match and an empty slot exists in the group, the key definitely isn't present.

```cpp
// AVX2: Check 32 slots in ONE instruction
__m256i ctrl = _mm256_loadu_si256(&control[group]);
__m256i h2_vec = _mm256_set1_epi8(target_h2);
__m256i matches = _mm256_cmpeq_epi8(ctrl, h2_vec);
uint32_t mask = _mm256_movemask_epi8(matches);
```

This is why miss lookups complete in nanoseconds: the SIMD comparison finds no matches, and the presence of an empty slot proves the key doesn't exist—all without examining a single actual key.

---

## Feature Inventory

### 1. SIMD-Accelerated Lookup

FastHashMap selects the optimal SIMD backend at compile time based on compiler flags. On modern x86-64 with AVX2, each probe examines 32 slots. On baseline x86-64 with SSE2 or ARM64 with NEON, each probe examines 16 slots. A portable C++ fallback emulates 16-slot groups on platforms without SIMD support.

The group width directly affects probe efficiency. With 32-slot groups, the average successful lookup examines 1.0-1.5 groups at typical load factors. Miss detection is even more efficient: finding an empty slot in the first group proves the key doesn't exist.

### 2. Configurable Deletion Policy

FastHashMap offers two deletion strategies through a template parameter.

TombstoneDeletion (the default) marks deleted slots with a special value (0x7E). Probing continues through tombstones but stops at truly empty slots. Erase is O(1)—just change one byte. The tradeoff: tombstones accumulate over time, potentially degrading lookup performance. FastHashMap triggers automatic rehashing when tombstones plus live entries exceed the growth threshold.

BackwardShiftDeletion shifts subsequent elements backward to fill gaps. No tombstones ever exist, so probe chains stay short. The tradeoff: worst-case erase is O(n) when shifting an entire probe chain. This policy suits long-lived maps with infrequent erasure.

```cpp
fat_p::FastHashMap<int, Data> map;           // TombstoneDeletion (default)
fat_p::FastHashMapBS<int, Data> stable_map;  // BackwardShiftDeletion
```

### 3. Fixed-Buffer Allocator

For embedded systems, real-time applications, or hot paths where heap allocation is prohibited, FastHashMap offers a fixed-buffer allocator. The `FixedHashMap` alias pre-allocates control bytes and slots from an embedded buffer, achieving zero heap allocation after construction.

```cpp
fat_p::FixedHashMap<int, Vec3, 8192> particle_lookup;  // 8KB stack buffer
```

The critical constraint: `FixedHashMap` is non-movable and non-swappable. The embedded buffer contains raw pointers to its own storage. Moving the map would leave those pointers dangling. Use `HeapAllocator` if you need move semantics.

### 4. Built-In Hash Mixer

`std::hash<int>` on MSVC is the identity function—the hash of 42 is 42. This is catastrophic for hash tables: sequential keys map to sequential slots, creating long probe chains that defeat the entire Swiss Table design.

FastHashMap applies a SplitMix64 finalizer (or MurmurHash3 on 32-bit platforms) to all hashes by default. This transforms sequential inputs into seemingly random outputs with excellent bit distribution.

For hash functions that already have good avalanche properties—wyhash, xxHash, absl::Hash—the mixer is unnecessary overhead. Define `is_avalanching` in your hash functor to opt out:

```cpp
struct MyWyHash {
    using is_avalanching = void;  // Skip built-in mixer
    size_t operator()(int64_t x) const { return wyhash(&x, sizeof(x), 0, _wyp); }
};
```

### 5. Heterogeneous Lookup

With `std::string` keys, every lookup allocates a temporary string from `const char*` input. FastHashMap supports heterogeneous lookup: search with `string_view` or `const char*` without constructing a `std::string`.

Enable this by defining `is_transparent` in both your hash and equality functors. The hash must accept any type you want to search with; the equality must compare your key type against the search type.

### 6. Freeze Mode

For static lookup tables built once and never modified, `freeze()` enables read-only mode. Frozen maps assert on mutation attempts in debug builds, providing protection against accidental modification. Concurrent read access to a frozen map is data-race-free.

---

## Why Not Alternatives?

The decision matrix for hash table selection typically involves four constraints: dependencies, allocation model, hash quality requirements, and deletion behavior.

`std::unordered_map` has zero dependencies but uses separate chaining with per-entry allocation, offers no protection against weak hash functions, and provides a fixed deletion strategy.

`absl::flat_hash_map` delivers excellent performance but requires the Abseil library—a significant dependency for projects that need to remain lightweight. It also uses a fixed tombstone-based deletion strategy.

`boost::unordered_flat_map` provides strong performance, particularly for miss-heavy workloads where its group layout examines fewer candidates per miss. However, it requires Boost headers.

FastHashMap occupies the intersection: single-header with STL-only dependencies, built-in hash mixing that protects against weak hash functions, configurable deletion policy, and optional fixed-buffer allocation. If your constraints require all of these simultaneously, FastHashMap is the only option.

### When FastHashMap Loses

FastHashMap is not superior in every scenario.

For miss-heavy workloads where most queries return "not found," `boost::unordered_flat_map` is faster due to its group layout examining fewer candidates per miss.

For maximum possible throughput when dependencies are acceptable, `absl::flat_hash_map` has more aggressive optimizations from years of production tuning at Google scale.

For workloads requiring pointer stability—where pointers to values must survive insertions—FastHashMap is unsuitable. Use `StableHashMap` (the node-based Fat-P variant) or `std::unordered_map` instead.

---

## The "Forever Stuck" Reality

Scientific clusters often run RHEL 7/8 with GCC 7.x for driver compatibility. Government contracts may mandate C++17 for the next decade. CUDA development frequently requires older toolchains that match driver requirements.

FastHashMap isn't waiting for C++23 or C++26 to deliver Swiss Table performance. It works today, in C++17, with zero external dependencies. Even when newer standards eventually offer similar features, your codebase may remain contractually locked to older compilers. Fat-P bridges this gap permanently—not as a temporary shim, but as an architecturally superior solution that remains valuable even after compiler upgrades.

---

## Performance Characteristics

FastHashMap's performance advantages stem from three architectural mechanisms. First, flat storage eliminates per-entry allocation, removing `malloc()` overhead and heap lock contention. Second, SIMD probing examines multiple candidates per cache line access instead of one. Third, contiguous iteration benefits from hardware prefetching that can stay ahead of sequential access patterns.

FastHashMap wins on insert-heavy workloads (no per-entry allocation), erase-heavy workloads (O(1) tombstone marking versus `free()` plus linked-list surgery), iteration (contiguous memory layout), and any SIMD-capable platform (full hardware utilization).

FastHashMap loses on miss-heavy workloads (`boost::unordered_flat_map`'s group layout examines fewer candidates per miss), maximum optimization (`absl::flat_hash_map` has more aggressive tuning from years of production use), and pointer stability (any insertion or rehash can invalidate pointers to values).

See `components/FatPHashMap/results/` and `benchmark_results/` for current platform-specific benchmark data.

---

## Integration Points

FastHashMap depends on `FatPSimdDetection.h` for compile-time SIMD backend selection.

FastHashMap contrasts with `StableHashMap`, the node-based Fat-P hash table. Where FastHashMap stores values directly in slots for maximum throughput, StableHashMap stores values in separate heap nodes for pointer stability. FastHashMap delivers higher throughput due to eliminating per-node allocation; StableHashMap guarantees that pointers to values survive insertions and rehashing.

Choose FastHashMap for temporary maps, caches, and per-frame data structures where throughput dominates. Choose StableHashMap for long-lived maps where external code holds pointers to stored values.

---

## Final Assessment

FastHashMap delivers on the Fat-P promise.

**Permanence:** Swiss Table architecture with configurable deletion and allocation policies is not available in any C++ standard, current or proposed. This isn't a compatibility shim—it's a capability the standard cannot provide due to ABI constraints.

**Specialization:** SIMD-accelerated probing, built-in hash mixing, and fixed-buffer allocation address HPC requirements absent from general-purpose libraries. The standard library cannot assume SIMD availability; FastHashMap exploits it.

**Control:** Deletion policy, allocator policy, hash function, and mixer behavior are all configurable through template parameters. The standard provides one-size-fits-all; Fat-P provides compile-time policy resolution.

For workloads where throughput matters more than pointer stability, FastHashMap transforms allocation-bound and cache-miss-bound code into compute-bound operations.

---

*FastHashMap.h — Fat-P Library*
