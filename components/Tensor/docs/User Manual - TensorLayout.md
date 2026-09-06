---
doc_id: UM-TENSORLAYOUT-001
doc_type: "User Manual"
title: "TensorLayout"
fatp_components: ["TensorLayout", "Tensor"]
topics: ["dynamic extents", "signed strides", "inline metadata", "layout reachability", "broadcast layout", "overlapping layout"]
constraints: ["checked ptrdiff arithmetic", "pointer-free metadata", "inline common-rank storage", "no intrinsic rank cap"]
cxx_standard: "C++20"
std_equivalent: "std::mdspan (partial metadata overlap)"
std_since: "C++23"
boost_equivalent: "Boost.MultiArray (partial layout overlap)"
build_modes: ["Debug", "Release"]
last_verified: "2026-09-04"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "draft"
---

# User Manual - TensorLayout

## Scope

This manual covers the pointer-free runtime metadata introduced by
`TensorLayout.h`: checked `DynamicExtents`, signed element strides, axis
normalization, reachable storage offsets, and layout classification.

`TensorRanked.h` adds `RankedExtents<Rank>` and
`RankedTensorLayout<Rank>`, which store the same validated information in
fixed-size arrays. See `User Manual - TensorRanked.md` for the owning/view
family and rank-propagation rules.

## Not covered

- Tensor element ownership or allocation.
- Borrowed or shared views.
- Slice parsing, iteration kernels, reductions, or execution contexts.

## Prerequisites

- C++20 containers and exceptions.
- Row-major logical indexing.
- The semantic decisions in `Design Note - Tensor Semantic Contract.md`.

## User Manual Card

**Component:** TensorLayout  
**Use it for:** Validating an origin/extents/strides mapping before any pointer arithmetic.  
**Key types:** `DynamicExtents`, `TensorStrides`, `TensorLayout`, `TensorLayoutKind`.  
**Key guarantee:** Every nonempty reachable offset lies in `[0, storageLength)`.  
**Failure model:** Invalid rank, arithmetic, origin, or reachability is rejected by a standard typed exception.  
**Allocation model:** Ranks zero through four keep extents and strides inline; higher ranks use an unbounded heap fallback. Tensor element storage is never allocated.

## Include

```cpp
#include "TensorLayout.h"
```

The flat facade owns the public API. Do not include files under
`include/fat_p/tensor/` directly.

## Dynamic extents

`DynamicExtents` stores runtime extents and a checked logical size.

```cpp
fat_p::DynamicExtents scalar;          // rank 0, logical size 1
fat_p::DynamicExtents image{480, 640}; // rank 2, logical size 307200
fat_p::DynamicExtents empty{2, 0, 3};  // rank 3, logical size 0
```

The product is checked against both `size_t` and `ptrdiff_t`. A zero extent is
detected before multiplication, so an empty shape does not fail because an
irrelevant nonzero product would overflow.

Use `rank()`, `logicalSize()`, `hasZeroExtent()`, `values()`, and checked `at()`
to inspect extents.

Ranks zero through four keep extent and stride values inside their metadata
objects, so constructing and copying ordinary low-rank layouts does not allocate
metadata buffers. Higher ranks spill transparently to heap storage; there is no
semantic rank limit. The inline capacity makes each metadata object larger than
a vector-only representation, trading descriptor size for fewer allocations in
owners, views, and iterators.

## Canonical contiguous layouts

```cpp
auto layout = fat_p::TensorLayout::contiguous(fat_p::DynamicExtents{2, 3, 4});

// storageLength() == 24
// originOffset()  == 0
// strides()       == {12, 4, 1}
```

A rank-zero contiguous layout uses one storage element. A zero-extent layout
uses no elements and has no reachable minimum or maximum offset.

## General signed layouts

The constructor takes storage length, logical origin, extents, and signed
element strides:

```cpp
fat_p::TensorLayout reversed(
    6,
    2,
    fat_p::DynamicExtents{2, 3},
    fat_p::TensorStrides{3, -1});

// Logical offsets: 2, 1, 0, 5, 4, 3
```

Construction checks:

- extent rank equals stride rank;
- storage length and logical size fit `ptrdiff_t`;
- every stride contribution and offset sum is representable;
- the origin is in the storage span for nonempty layouts; and
- the minimum and maximum reachable offsets remain inside the span.

No pointer is formed during validation or logical offset calculation.

## Layout classification

`kind()` returns one of:

| Kind | Meaning |
|---|---|
| `Empty` | At least one extent is zero; no element is reachable. |
| `Contiguous` | Canonical positive row-major mapping; singleton-axis strides are ignored. |
| `InjectiveStrided` | Every logical index maps to a distinct offset, but the mapping is not canonical contiguous. |
| `Broadcast` | An expanded axis has stride zero, so multiple indices alias. |
| `Overlapping` | Construction proved that at least two logical indices alias. |
| `Indeterminate` | A bounded large-layout proof established neither injectivity nor overlap. |

Rank-two layouts are classified exactly using a bounded linear Diophantine
test. Higher-rank layouts first attempt a stride-packing injectivity proof,
so ordinary permutations and padded mappings avoid enumerating their elements.
If that proof is inconclusive, layouts up to 8,192 elements use exact offset
enumeration. Larger layouts use a constructive overlap proof; an unresolved
mapping is classified as `Indeterminate`, never falsely labeled
`Overlapping`. Mutable views require `isInjective()`, so an indeterminate layout
is conservatively read-only until a stronger proof is added.

The convenience queries are `isEmpty()`, `isContiguous()`, `isInjective()`,
`isBroadcast()`, `isOverlapping()`, and `isIndeterminate()`.

## Logical linear offsets

`logicalOffset(linearIndex)` decodes a logical row-major index through signed
strides. It throws `std::out_of_range` when the index is outside
`[0, logicalSize())`.

This method returns an element offset, not a pointer and not a byte offset.

## Axis normalization

```cpp
auto last = fat_p::normalizeAxis(-1, 3);       // 2
auto axes = fat_p::normalizeAxes({-1, 0}, 3); // {2, 0}
```

An out-of-range axis throws `std::out_of_range`. A duplicate after
normalization throws `std::invalid_argument`.

## Exception reference

| Condition | Exception |
|---|---|
| Extent product, stride contribution, or offset sum is not representable | `std::overflow_error` |
| Extent/stride rank mismatch or duplicate normalized axis | `std::invalid_argument` |
| Origin/reachability outside storage or checked logical index outside size | `std::out_of_range` |

Construction either produces a fully validated value or throws; no partially
usable layout is returned.

## Current boundary

The rank-policy layout core is the single metadata authority consumed by
dynamic and ranked owners, views, and iteration plans. `TensorLayout` is its
runtime-rank public spelling; `RankedTensorLayout<Rank>` is the array-backed
fixed-rank spelling.
It never owns or allocates Tensor element storage. Exact classification of small
higher-rank layouts may use bounded operation-local metadata scratch.
