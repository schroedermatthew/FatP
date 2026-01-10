---
doc_id: stablehashmap-overview
doc_type: overview
status: candidate
fatp_components: [StableHashMap]
last_updated: 2026-01-09
---

# StableHashMap: A Fat-P Library Showcase

*Updated December 2025 -- Benchmarks: Windows, 3.7 GHz base (throttled ~65%)*

## Executive Summary

StableHashMap is a **SIMD-accelerated Swiss Table** with **reference stability**—pointers to values remain valid across insertions and rehashes. Unlike `std::unordered_map` (slow chaining) or `absl::flat_hash_map` (invalidates pointers), StableHashMap combines node-based storage with SIMD metadata probing. The **Block allocator** variant eliminates per-node allocation overhead, delivering **3-5x speedup** over `std::unordered_map` while preserving pointer stability.

---

## The Problem

```cpp
// The chaining tax: pointer chasing on every lookup
std::unordered_map<int, Data> map;
for (int i = 0; i < 1000000; ++i) {
    auto it = map.find(i);  // hash → bucket → node → node → node...
    process(it->second);    // Cache miss per node in chain
}

// The flat table trap: pointer invalidation
absl::flat_hash_map<int, Data*> index;
index[1] = &storage[0];
index[2] = &storage[1];  // May rehash—all existing pointers now dangling!
```

| Constraint | Why Alternatives Fail |
|------------|----------------------|
| std::unordered_map | Per-node allocation, pointer chasing, no SIMD |
| absl::flat_hash_map | Pointers invalidated on insert |
| boost::unordered_node_map | Requires Boost dependency |
| C++23/26 | ABI-frozen; chaining design cannot change |

---

## The Solution

SIMD metadata (16-slot groups) + node-based storage (stable addresses):

```
Group 0:
  Control bytes: [H2|H2|__|H2|__|...] ← SIMD compares 16 in parallel
  Node pointers: [ptr|ptr|null|ptr|null|...]
                   ↓
                 Node { key, value } ← stable address, never moves
```

**The insight:** Control bytes enable SIMD-accelerated lookup; node indirection preserves pointer stability. Rehashing shuffles pointers, not data.

---

## Feature Summary

| Feature | Mechanism | Benefit |
|---------|-----------|---------|
| SIMD lookup | AVX2 H2 fingerprint comparison | 16 candidates checked per instruction |
| Reference stability | Node-based storage | Pointers valid across insert/rehash |
| Block allocator | Contiguous node allocation | 2-4x faster insert/erase |
| Built-in hash mixer | SplitMix64 finalizer | Prevents clustering from weak hashes |
| Heterogeneous lookup | Transparent hash/equal | Zero-allocation string_view finds |

---

## Key Behaviors

### Reference Stability

```cpp
StableHashMap<int, HeavyObject> map;
HeavyObject* ptr = &map[1];

for (int i = 2; i < 10000; ++i)
    map[i] = HeavyObject{};  // May trigger multiple rehashes

ptr->modify();  // Still valid—node never moved
```

### Block Allocator (Insert-Heavy Workloads)

```cpp
// Standard: each insert calls malloc
StableHashMap<int, Data> standard;

// Block: nodes allocated in contiguous chunks
using BlockStableMap = fat_p::StableHashMap<int, Data, std::hash<int>, std::equal_to<int>, fat_p::BlockAllocator>;
BlockStableMap block;
// 2.3x faster insert, 4.2x faster erase at N=1M
```

### Built-in Hash Protection

```cpp
// Sequential integers with std::hash (often identity) → clustering disaster
// StableHashMap applies SplitMix64 automatically

StableHashMap<int, Data> map;
for (int i = 0; i < 100000; ++i)
    map[i] = data;  // Works well despite weak std::hash

// Opt out for already-good hashes:
struct MyHash {
    using is_avalanching = void;  // Skip mixer
    size_t operator()(int x) const { return excellent_hash(x); }
};
```

### Heterogeneous Lookup

```cpp
StableHashMap<std::string, int> map;
map["hello"] = 1;

std::string_view key = get_key_from_network();
int* val = map.find(key);  // No temporary string allocation
// Saves 25-35 ns per lookup
```

---

## Performance

| Map | Insert | Find | Miss | Erase |
|-----|--------|------|------|-------|
| **StableHashMap[Block]+SM64** | **17** | **9** | **9** | **24** |
| StableHashMap+SM64 | 40 | 13 | 10 | 100 |
| boost::unordered_node_map | 36 | 11 | **5** | 81 |
| std::unordered_map | 83 | 29 | 36 | 128 |

*N=1M, median ns/op. All node-based maps for fair comparison.*

**Speedup vs std::unordered_map:** 4.8x insert, 3.3x find, 5.4x erase

### Where StableHashMap Wins
- Reference stability required (graph structures, indices, observers)
- Insert-heavy workloads (Block allocator: 4.8x speedup)
- Zero dependencies (single header, STL only)
- Weak hash protection (sequential integer keys)

### Where StableHashMap Loses
- Miss-heavy workloads: boost achieves ~2x faster miss detection
- Flat storage acceptable: flat maps are 2-3x lower latency if pointers don't matter
- Erase without Block allocator: ~100 ns per erase (use Block variant)

---

## Why Not Alternatives?

| Criterion | std | absl::flat | absl::node | boost::node | **Fat-P** |
|-----------|-----|------------|------------|-------------|-----------|
| Reference stability | ✓ | ✗ | ✓ | ✓ | **✓** |
| SIMD acceleration | ✗ | ✓ | ✓ | ✓ | **✓** |
| Zero dependencies | ✓ | ✗ | ✗ | ✗ | **✓** |
| Block allocator | ✗ | N/A | ✗ | ✗ | **✓** |
| Built-in hash mixer | ✗ | ✗ | ✗ | ✗ | **✓** |

---

## The "Forever Stuck" Reality

**ABI Reality:** `std::unordered_map`'s chaining design is frozen—SIMD acceleration would break binary compatibility. No standard "node-based Swiss Table" is proposed.

**Dependency Reality:** `boost::unordered_node_map` provides similar performance but requires Boost. `absl::node_hash_map` requires Abseil.

**StableHashMap is not a temporary shim.** It provides SIMD-accelerated, reference-stable, zero-dependency hash maps that no standard or major library offers.

---

## Integration Points

```
StableHashMap.h
    → uses: std::vector, std::functional
    → used by: StringPool.h, Factory.h, FeatureManager.h
```

---

## Final Assessment

StableHashMap delivers on the fat_p promise:

1. **Permanence:** `std::unordered_map` cannot adopt SIMD without ABI breakage. This gap is permanent.

2. **Specialization:** SIMD metadata + node storage + block allocation addresses HPC needs: cache-efficient lookups with pointer stability.

3. **Control:** Block allocator, hash mixer opt-out, read-only mode, load factor tuning—fine-grained space/speed tradeoffs.

**Architectural Verdict:** StableHashMap transforms hash table performance from **chaining-limited pointer chasing** to **SIMD-accelerated parallel probing**, while preserving reference stability guarantees.

---

