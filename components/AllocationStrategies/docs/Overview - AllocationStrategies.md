---
doc_id: OV-ALLOCATIONSTRATEGIES-001
doc_type: "Overview"
title: "AllocationStrategies"
fatp_components: ["AllocationStrategies"]
topics: ["allocator", "NewDeleteAllocator", "BlockAllocator", "PoolAllocator", "bump pointer", "free list", "pre-allocated pool", "memory allocation policy"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "performance engineers", "AI assistants"]
status: "draft"
---

# Overview - AllocationStrategies

*February 2026*

---

## What It Does

AllocationStrategies provides three allocator policies for Fat-P's policy-based containers: `NewDeleteAllocator` (per-object heap allocation via `new`/`delete`), `BlockAllocator` (contiguous block allocation with a bump pointer and free list), and `PoolAllocator` (fixed-size pre-allocated pool). Each allocator exposes the same `allocate(args...)`/`deallocate(ptr)` interface, making them interchangeable as template parameters.

## Why It Exists

Different workloads have different allocation profiles. A hash map with infrequent inserts benefits from standard `new`/`delete`---the OS allocator is well-tuned and cache-friendly. A high-throughput insertion workload benefits from block allocation---amortizing allocation overhead across many objects. A real-time system with a known maximum capacity benefits from pool allocation---zero runtime allocation after startup. AllocationStrategies lets the container user choose the tradeoff without modifying container code.

## Key Concepts

All three allocators construct objects in-place via perfect forwarding. `NewDeleteAllocator` delegates to the heap. `BlockAllocator` allocates contiguous blocks of N objects and hands them out via bump pointer; deallocated objects go to a free list for reuse. `PoolAllocator` pre-allocates a fixed array at construction and manages a free list within it. None are thread-safe; for concurrent access, see `FatPAllocationStrategies.h` which provides `SynchronizedWrapper` and `LockFreeWrapper` policies.

## Architecture at a Glance

Single header (`AllocationStrategies.h`) in namespace `fat_p`. No dependencies beyond the standard library. Primary consumer is `StableHashMap`, which uses the allocator policy for node allocation.

---

*AllocationStrategies.h --- Fat-P Library*
