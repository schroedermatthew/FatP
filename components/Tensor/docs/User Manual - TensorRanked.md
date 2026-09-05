---
doc_id: UM-TENSOR-RANKED-001
doc_type: "User Manual"
title: "TensorRanked"
fatp_components: ["TensorRanked", "Tensor", "TensorLayout", "TensorView"]
topics: ["fixed rank", "runtime extents", "tensor", "zero-copy adapter"]
constraints: ["C++20", "header-only", "borrowed lifetime"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.MultiArray (partial semantic overlap)"
build_modes: ["Debug", "Release"]
status: "in_work"
---

# User Manual - TensorRanked

`TensorRanked.h` provides tensors whose number of axes is part of the C++ type
while the length of every axis is supplied at runtime. It is the middle choice
between dynamic-rank `Tensor` and fully compile-time, inline-element
`StaticTensor`.

```cpp
#include "TensorRanked.h"

fat_p::RankedTensor<float, 2> image({1080, 1920}, 0.0f);
image(20, 30) = 1.0f; // exactly two indices are required at compile time
```

Include the existing operation header for the family you use, such as
`TensorAlgorithms.h`, `TensorReductions.h`, `TensorSelection.h`,
`TensorMatmul.h`, `TensorContractions.h`, or `TensorExecution.h`. Ranked and
dynamic tensors use the same kernels and numeric contract.

## Choosing a tensor family

| Family | Rank | Extents | Elements | Typical use |
|---|---|---|---|---|
| `RankedTensor<T, R>` | compile time | runtime | allocator-owned | Matrices and tensors whose arity is known but dimensions are input data |
| `Tensor<T>` | runtime | runtime | allocator-owned | Pipelines where rank itself varies at runtime |
| `StaticTensor<T, Shape<...>>` | compile time | compile time | inline | Small shapes fixed entirely by the program |

`RankedTensor` stores extents and strides in `std::array`. Its elements remain
allocator-owned: creating a nonempty owner still allocates element storage and
a lifetime control block. The ranked family removes persistent heap metadata;
it is not a small-buffer optimization for elements.

## Public vocabulary

```cpp
fat_p::RankedExtents<3> extents{2, 4, 8};
fat_p::RankedTensorLayout<3> layout =
    fat_p::RankedTensorLayout<3>::contiguous(extents);

fat_p::RankedTensor<double, 3> owner(extents, 1.0);
fat_p::RankedTensorView<double, 3> borrowed = owner.asView();
fat_p::SharedRankedTensorView<double, 3> shared = owner.asSharedView();

static_assert(fat_p::tensor_static_rank_v<decltype(owner)> == 3);
static_assert(fat_p::tensor_static_rank_v<fat_p::Tensor<double>> ==
              fat_p::kDynamicTensorRank);
```

When dimensions arrive in a span, use the checked factory:

```cpp
std::array<std::size_t, 3> input{2, 4, 8};
auto extents = fat_p::RankedExtents<3>::fromSpan(input);
```

The factory throws `std::invalid_argument` unless the span contains exactly
three extents. Extent products and canonical strides retain the checked
overflow behavior of dynamic tensors.

## Rank zero, empty defaults, and moves

- `RankedTensor<T, 0>` is a scalar with one value-initialized element.
- A default `RankedTensor<T, R>` for `R > 0` has `R` zero extents and is empty.
- Moving a positive-rank owner transfers compatible storage and resets the
  source to its all-zero empty shape.
- Moving a uniquely owned rank-zero owner moves its element but leaves the
  source as a valid scalar containing the element type's ordinary moved-from
  value. When shared aliases exist, a copyable element is copied to preserve
  those aliases; a non-copyable element reports `std::logic_error` before
  either owner changes.
- Ranked borrowed and shared views have no disengaged default state. Moving one
  has copy-equivalent binding semantics, so the source view stays bound.

Owner moves, assignment, destruction, and swap invalidate tracked borrowed
views in assertions-enabled builds. Shared views retain element storage.

## Result-rank rules

An allocating operation returns a ranked owner only when its output rank is
known from ranked inputs and template arguments. If any allocating input has
dynamic rank, the result is a dynamic `Tensor`.

| Operation | Ranked result |
|---|---|
| Unary, scalar arithmetic, cast, clone | source rank |
| Binary elementwise | `max(left rank, right rank)` |
| Transpose and permutation | source rank |
| Unsqueeze | source rank + 1 |
| `squeezeView<Axes...>()` | source rank - number of axes |
| Typed slice | source rank - integral indices + `NewAxis` entries |
| Fixed-rank reshape or broadcast target | target rank |
| `sum<Keep, Axes...>` and other typed reductions | source rank when kept, otherwise source rank - axis count; empty pack reduces all to rank zero |
| Stack / concatenate | source rank + 1 / source rank |
| Take / take-along-axis | source rank / indices rank |
| `gatherND<K>` | indices rank - 1 + source rank - `K` |
| Matmul | NumPy-style vector/matrix/batch rank formula |
| Dot / outer / diagonal / trace | 0 / 2 / source rank - 1 / source rank - 2 |
| Fixed-array `tensorDot` | left rank + right rank - twice the axis-pair count |

Runtime axis vectors and runtime-rank target extents deliberately return the
dynamic family when the output rank is not encoded in the call.
Pairwise `stack` and `concatenate` require equal compile-time ranks when both
inputs are ranked; a dynamic input defers that rank check to runtime and makes
the result dynamic.

```cpp
fat_p::RankedTensor<int, 3> source({2, 3, 4}, 1);

auto rows = fat_p::sum<false, 0, 2>(source); // RankedTensor<int64_t, 1>
auto scalar = fat_p::sum<false>(source);     // RankedTensor<int64_t, 0>
auto view = source.sliceView(fat_p::All, 1, fat_p::NewAxis, fat_p::All);
static_assert(fat_p::tensor_static_rank_v<decltype(view)> == 3);
```

## Views and rank-changing transforms

Transforms remain zero-copy mappings. Fixed-output forms preserve rank:

```cpp
auto reshaped = source.reshapeView(fat_p::RankedExtents<2>{6, 4});
auto inserted = source.unsqueezeView(1);       // rank 4
fat_p::RankedTensor<int, 3> singleton({1, 3, 4}, 0);
auto removed = singleton.squeezeView<0>();     // rank 2
auto runtime = source.squeezeView();           // dynamic-rank view
```

`broadcastView` is read-only because expanded zero strides alias elements.
Arbitrary read-only ranked layouts may be padded, reversed, broadcast,
overlapping, or indeterminate. Mutable construction requires a proven
injective layout, exactly as for dynamic views.

## Dynamic/ranked adaptation

Borrowed adapters copy only metadata and preserve the original storage address,
signed strides, constness, and tracked lifetime:

```cpp
auto dynamic = fat_p::asDynamicView(owner);       // owner must be an lvalue
auto ranked = fat_p::asRankedView<3>(dynamic);    // checked runtime rank
```

View constness is shallow: adapting a `const RankedTensorView<T, R>` still
produces a view of `T`. Use `RankedTensorView<const T, R>` for read-only
elements. Adapting a const owner produces a const-element view.

`asRankedView<R>` throws `std::invalid_argument` before publishing a view when
the runtime rank differs. Borrowed adapters reject owner and view temporaries.
Use `asDynamicSharedView` and `asRankedSharedView<R>` when the adapted view must
retain storage lifetime. These shared adapters accept named owners and shared
view values, while owner temporaries are rejected. A borrowed adapter called on
an lvalue `SharedTensorView` does not transfer its shared handle, so the
resulting borrow must not outlive that shared source.

Ranked-to-dynamic adaptation is O(rank). Dynamic metadata keeps ranks zero
through four inline; adapting a higher rank may allocate its metadata fallback.
Dynamic-to-ranked adaptation stores the checked metadata directly in arrays.

Owning conversions are explicit:

```cpp
auto independent = fat_p::toRankedTensor<3>(dynamicOwner); // copies elements
auto transferred = fat_p::toDynamicTensor(std::move(owner)); // may transfer storage
```

An rvalue rank mismatch is checked before the source allocator, storage,
lifetime token, or borrowed views are changed.

## Descriptors and standard interop

`describeTensor(rankedSource)` returns
`RankedStridedTensorDescriptor<T, R>` and retains array metadata. It does not
silently erase rank through `StridedTensorDescriptor`.

Include `TensorInterop.h` for `contiguousSpan`, exact `StaticTensor` conversion,
and `asMdspan` when the standard library provides `std::mdspan`. Ranked mdspan
conversion checks layout injectivity and signed-stride representability but does
not repeat a runtime rank comparison when the source rank already matches in
the type.

## Allocation and performance boundaries

Canonical ranked extents, strides, layouts, borrowed views, and fixed-rank
iteration-plan metadata use no heap storage. Public construction of an arbitrary
higher-rank strided layout may use bounded scratch while proving injectivity.
Owners allocate their elements and lifetime state; shared ownership uses its
normal control block. Some rank-changing runtime APIs build dynamic metadata by
design.

`components/Tensor/benchmarks/benchmark_TensorRanked.cpp` records object sizes
for ranks 0-8 and compares construction, copy, indexing, native elementwise
kernels, and both adapter directions against dynamic `Tensor` and
`StaticTensor`. Its observations are evidence for a build and machine, not ABI
or timing guarantees.

The ranked facade deliberately does not include `TensorStatic.h` or
`TensorInterop.h`; those remain opt-in for conversion work. As one include-cost
observation, Clang 22.1 on Windows with the local C++20 standard library emitted
73,488 preprocessed lines for `Tensor.h` and 73,725 for `TensorRanked.h`, a
237-line delta. Preprocessed line counts vary by compiler and standard library
and are not a compatibility or performance guarantee.

## Error summary

| Condition | Error |
|---|---|
| Wrong runtime rank for ranked extents or an adapter | `std::invalid_argument` |
| Incompatible extents, axes, reshape, broadcast, or contraction | `std::invalid_argument` |
| Axis, index, origin, or reachability outside its domain | `std::out_of_range` |
| Unrepresentable extent, stride, offset, or checked arithmetic | `std::overflow_error` |
| Empty extremum domain without an initial value | `std::domain_error` |
| Expired tracked borrowed view in assertions-enabled builds | `std::runtime_error` |
| Element or metadata allocation failure | `std::bad_alloc` |

Validation precedes publication. Operations that allocate a result do not
modify their inputs, and compound/copy operations retain their existing
transactional aliasing rules.
