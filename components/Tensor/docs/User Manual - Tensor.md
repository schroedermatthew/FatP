---
doc_id: UM-TENSOR-001
doc_type: "User Manual"
title: "Tensor"
fatp_components: ["Tensor", "TensorLayout", "TensorSlice", "TensorView", "TensorAlgorithms", "TensorReductions", "TensorInterop", "TensorSelection", "TensorMatmul", "TensorEquality", "TensorSerializer"]
topics: ["dynamic tensor", "tensor owner", "tensor view", "signed strides", "slicing", "broadcasting", "reductions", "interop", "matrix multiplication", "indexed selection", "tensor serialization"]
constraints: ["C++20", "header-only", "canonical owning storage", "borrowed lifetime", "injective mutation"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.MultiArray (partial overlap)"
build_modes: ["Debug", "Release"]
last_verified: "2026-08-19"
audience: ["C++ developers", "library maintainers"]
status: "in_work"
---

# User Manual - Tensor

`fat_p::Tensor<T, Allocator>` is the owning dynamic tensor type. Owners always
store values in canonical contiguous row-major order. Non-owning mappings use
`TensorView<T>`, `TensorView<const T>`, or `SharedTensorView<T>`.

The separation is deliberate:

| Type | Storage lifetime | Writable | Layout |
|---|---|---:|---|
| `Tensor<T, Allocator>` | Owned | Yes | Canonical contiguous |
| `TensorView<T>` | Borrowed | Yes, when layout is injective | Any validated injective layout |
| `TensorView<const T>` | Borrowed | No | Any validated layout |
| `SharedTensorView<T>` | Shared | Yes, when layout is injective | Any validated injective layout |
| `SharedTensorView<const T>` | Shared | No | Any validated layout |

## Headers

```cpp
#include "Tensor.h"             // owner plus view factories
#include "TensorView.h"         // explicit view-facing include
#include "TensorLayout.h"       // extents, axes, strides, layouts
#include "TensorAlgorithms.h"   // serial owner/view algorithms
#include "TensorReductions.h"   // deterministic axis reductions
#include "TensorInterop.h"      // span, descriptor, mdspan, and static conversion
#include "TensorMatmul.h"       // vector, matrix, and batched multiplication
#include "TensorSelection.h"    // stack, concatenate, take, and gather
#include "TensorSlice.h"        // extended slice vocabulary
#include "TensorEquality.h"     // EqualityComparisons integration
#include "TensorSerializer.h"   // portable logical-value serialization
```

All public facades live in `include/fat_p/`. Implementation-owned headers are
centralized under `include/fat_p/tensor/`.

## Construction

```cpp
using namespace fat_p;

Tensor<float> empty;                   // extents {0}, rank 1, size 0
Tensor<float> scalar({}, 3.5f);        // extents {}, rank 0, size 1
Tensor<float> matrix({2, 3}, 0.0f);    // rank 2, size 6

matrix(1, 2) = 7.0f;
float value = matrix.atLinear(5);
```

Any zero extent makes a tensor empty. Extent products and canonical strides
are checked against both `size_t` and `ptrdiff_t` before element allocation.
A moved-from owner is reset to the canonical empty state `{0}`.
Move construction is not declared `noexcept`: assertions-enabled lifetime
tracking and allocator state may require operations that can throw. Code that
requires a non-throwing relocation primitive should store an indirection to the
owner instead of assuming `Tensor` is nothrow-movable.

Allocator instances are explicit when needed:

```cpp
PoolAllocator<float> pool(/* state */);
Tensor<float, PoolAllocator<float>> values(
    std::allocator_arg, pool, DynamicExtents{100, 20}, 0.0f);
```

Copy construction uses `select_on_container_copy_construction`. Copy
assignment, move assignment, and swap honor POCCA, POCMA, and POCS. Storage
deleters retain the allocator instance that performed allocation, so destruction
does not depend on the current owner object's allocator state.

## Extents and layouts

`DynamicExtents` is the runtime shape vocabulary. `TensorLayout` is pointer-free
metadata containing:

- backing storage length in elements;
- logical origin offset;
- extents;
- signed element strides;
- checked minimum and maximum reachable offsets;
- a layout classification.

```cpp
TensorLayout reversed(
    6,                         // storage length
    2,                         // logical origin
    DynamicExtents{2, 3},
    TensorStrides{3, -1});

auto kind = reversed.kind();  // TensorLayoutKind::InjectiveStrided
```

The classifications are `Empty`, `Contiguous`, `InjectiveStrided`,
`Broadcast`, `Overlapping`, and `Indeterminate`. Empty layouts are contiguous
and injective. Singleton-axis strides do not constrain contiguity. Writable
views require a proven-injective layout; broadcast, overlapping, and
indeterminate mappings remain read-only.

Axes use `TensorAxis` (`ptrdiff_t`). `normalizeAxis` accepts negative axes, and
`normalizeAxes` rejects duplicates after normalization.

## Owner access

Owner storage is contiguous, so `data()`, pointer iterators, and ordinary STL
algorithms are valid:

```cpp
Tensor<int> values({2, 3});
std::iota(values.begin(), values.end(), 1);

assert(values(1, 2) == 6);
assert(values[3] == 4);
assert(values.data()[5] == 6);
```

Multidimensional access enforces exact rank and per-axis bounds. `operator[]`
is conventional unchecked contiguous owner access. `atLinear` is the checked
logical row-major form.

## Borrowed views

Borrowed factories are lvalue-qualified. A temporary owner cannot create a
borrowed mapping.

```cpp
Tensor<int> matrix({3, 4});
std::iota(matrix.begin(), matrix.end(), 1);

TensorView<int> all = matrix.asView();
TensorView<const int> readOnly = std::as_const(matrix).asConstView();
auto row = matrix.rowView(1);                    // extents {1, 4}
auto column = matrix.columnView(2);              // extents {3, 1}
auto interior = matrix.sliceView({1, 1}, {3, 4});
auto transposed = matrix.transposeView();         // extents {4, 3}
auto reshaped = matrix.reshapeView(DynamicExtents{2, 6});
```

Slice bounds are half-open. The original start/finish overload remains useful
for rectangular subviews. `TensorSlice.h` adds omitted endpoints, negative
indices and steps, integer-axis removal, `NewAxis`, and one `Ellipsis`:

```cpp
auto reversed = matrix.sliceView({All, Slice{std::nullopt, std::nullopt, -1}});
auto selected = cube.sliceView({-1, NewAxis, Ellipsis, Slice{0, std::nullopt, 2}});
auto permuted = cube.permuteView({2, 0, 1});
auto compactRank = singletonAxes.squeezeView();
auto expandedRank = compactRank.unsqueezeView(-1);
```

All of these operations are metadata-only. Integer indices are checked and
remove an axis. Negative steps produce signed-stride views. `squeezeView`
rejects a requested non-singleton axis, and permutation axes must be complete
and unique.

`reshapeView` requires a contiguous source and an unchanged logical element
count. `transposeView` is rank-two. These transforms modify metadata only and
allocate no element storage. Layout classification may use bounded temporary
metadata scratch for small higher-rank mappings.

For arbitrary external storage, construction is visibly non-owning:

```cpp
int storage[6]{};
auto view = TensorView<int>::borrow(
    storage,
    TensorLayout(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1}));
```

Mutable `borrow` and `share` reject layouts that do not prove injectivity.
Read-only `TensorView<const T>` and `SharedTensorView<const T>` may represent
broadcast, overlapping, or indeterminate layouts.

External ownership transfer is explicit and retains the supplied deleter:

```cpp
auto* storage = new int[3]{1, 2, 3};
auto owned = Tensor<int>::adopt(
    storage, DynamicExtents{3}, [](int* values) { delete[] values; });
```

The layout proves every reachable offset before the view is returned. A
nonempty layout rejects a null storage pointer.

## Shared views and lifetime

Use `asSharedView()` when the mapping must extend element-storage lifetime:

```cpp
SharedTensorView<int> saved;
{
    Tensor<int> owner({2}, 9);
    saved = owner.asSharedView();
}
assert(saved[1] == 9);
```

Borrowed views never extend lifetime. In assertions-enabled Debug builds,
owner-created borrowed views carry a weak lifetime token and throw
`std::runtime_error` when accessed after documented invalidation. Release builds
do not turn dangling access into a supported operation.

| Owner event | Borrowed view | Shared view |
|---|---|---|
| Element mutation | Remains valid; observes mutation | Remains valid; observes mutation |
| Owner destruction | Invalid | Remains valid |
| Owner move | Invalid | Remains valid |
| Owner copy/move assignment | Destination's previous borrowed views invalid | Previous shared storage remains alive |
| Owner swap | Borrowed views from both owners invalid | Shared storage remains alive |
| View copy/assignment | Copies/rebinds mapping metadata | Copies/rebinds mapping and lifetime handle |

## Constness and broadcasting

Element constness is represented in the element type. A const owner produces
only `TensorView<const T>`. Broadcast mappings are always read-only because an
expanded zero-stride axis aliases values:

```cpp
const Tensor<int> scalar({}, 5);
TensorView<const int> grid = scalar.broadcastView(DynamicExtents{2, 3});
```

Broadcast compatibility follows trailing-axis rules. Equal extents match;
extent one may expand; zero with one produces zero. Zero with an extent greater
than one is incompatible.

## Materialization

`clone` is the explicit owner-from-readable operation:

```cpp
auto column = matrix.columnView(1);
Tensor<int> compact = clone(column);

auto broadcast = scalar.broadcastView(DynamicExtents{100, 100});
Tensor<int> ownedBroadcast = clone(broadcast);
```

The result is canonical contiguous storage and never aliases the source. An
explicit allocator overload is available. Without one, owner inputs use SOCCC;
view-only calls use `TensorAllocator<T>`.

The same rule applies to all free allocating algorithms in this chapter:
without an explicit allocator, the first owning argument from left to right
supplies its allocator through SOCCC. Type-changing operations such as integral
`sum` and `matmul` rebind that allocator to the result element type. A call made
only from views uses the result type's default `TensorAllocator`. Each operation
also provides an explicit result-allocator overload.

## Serial algorithms

`TensorAlgorithms.h` provides the current base-kernel surface:

```cpp
auto sum = add(left, right);
auto difference = subtract(left, right);
auto product = multiply(left, right);
auto negated = transform(input, [](auto value) { return -value; });

bool exact = exactEqual(left, right);
bool close = approxEqual(left, right, 1e-6, 1e-5);
```

Binary operations apply trailing-axis broadcasting and produce an owning
canonical result. Same-type arithmetic is intentional until the numeric
promotion contract lands. For integral element types other than `bool`, current
`add`, `subtract`, and `multiply` detect overflow or underflow and throw
`std::overflow_error` before evaluating the invalid C++ operation. Mixed-type
promotion and division remain future numeric-policy work. `approxEqual` is
restricted to floating-point element and tolerance types; integral tensors use
`exactEqual` until a wider numeric policy is defined. Same-sign infinities are
equal; an infinity and a finite value are never approximately equal, regardless
of relative tolerance. NaNs are not approximately equal.

Generic owner/view materialization, fill, unary, binary, policy equality, exact
equality, and hash traversal use one `TensorIterationPlan`. Canonical owner copy
construction uses its contiguous storage invariant directly and does not run a
private layout walker. The plan normalizes ranks, coalesces compatible axes,
uses counted signed offsets, and never constructs an out-of-span sentinel
pointer. Owners expose contiguous random-access pointer iterators; views expose
counted logical forward iterators whose equality domain is the underlying
mapping, not the address of a particular view object.

## Reductions

`TensorReductions.h` reduces all axes by default or an explicit normalized axis
list. Negative axes are accepted; duplicates are rejected. `keepDimensions`
retains reduced axes with extent one.

```cpp
auto total = sum(values);                  // rank-zero result
auto rows = sum(values, {1});
auto columns = mean(values, {0}, true);    // shape {1, columns}
auto locations = argmax(values, {1});
```

`sum` and `product` widen `bool` and small integral types to `size_t`,
`int64_t`, or `uint64_t`; checked integral overflow throws. `mean` returns
`double` except for `long double` input. Empty sum/product domains use zero/one.
Mean and extrema reject a nonempty output with an empty reduction domain;
`min` and `max` accept an optional initial value. Extrema propagate the first
NaN, and argument reductions return the first tie as a row-major flattened
coordinate within the reduced axes.

## Matrix multiplication

`TensorMatmul.h` supports rank-one vectors, rank-two matrices, and trailing-axis
broadcasted batches:

```cpp
auto scalar = matmul(vectorA, vectorB);       // K @ K -> {}
auto output = matmul(matrixA, matrixB);       // MxK @ KxN -> MxN
auto batches = matmul(leftBatches, right);    // ...MxK @ ...KxN
```

Small integral inputs use the same widened result type as reductions, and all
integral multiply/add steps are checked. A zero contraction dimension produces
the additive identity. Contiguous rank-two operands use a cache-blocked serial
kernel; other validated signed-stride layouts use the general coordinate
kernel. No external BLAS dependency is required.

## Composition and indexed selection

`TensorSelection.h` keeps operations with different index semantics separate:

```cpp
auto layered = stack(first, second, 0);
auto joined = concatenate(left, right, 1);
auto rows = take(matrix, rowIndices, 0);
auto chosen = takeAlongAxis(matrix, indexTensor, 1);
auto gathered = gatherND(cube, coordinateTuples);
```

`stack` inserts an axis; `concatenate` adds extents on an existing axis. The
two-operand overloads accept different readable mapping types with the same
value type. Homogeneous multi-input overloads accept an initializer list or a span of
`reference_wrapper<const Source>`. `take` accepts duplicates and negative
indices. `takeAlongAxis` requires equal ranks and matching non-axis extents.
For `gatherND`, the final indices extent is tuple depth, and unreplaced source
axes are appended to the output. Bounds are validated before result evaluation.

## Interop

`TensorInterop.h` makes layout restrictions visible:

```cpp
auto contiguous = contiguousSpan(owner);
auto descriptor = describeTensor(stridedView);
auto borrowedAgain = descriptor.borrow();
auto fixed = toStaticTensor<Shape<2, 3>>(owner);
auto dynamic = toTensor(fixed);
```

`contiguousSpan` rejects non-contiguous mappings. `StridedTensorDescriptor<T>`
owns its extent/stride metadata but borrows element storage; callers must keep
that storage alive. A descriptor created from `SharedTensorView` retains that
view's shared storage handle for the descriptor's lifetime; a view returned by
`borrow()` must not outlive the descriptor. Interop functions reject rvalues so they cannot immediately
return dangling borrowed objects. In Debug builds, descriptors preserve the
source owner's lifetime token when converted back to a view. A descriptor
preserves storage base, storage length, logical origin, extents, and signed
strides, and validates those public fields before pointer formation. When the
C++ standard library supplies C++23
`std::mdspan`, `asMdspan<Rank>` is available for injective non-negative-stride
mappings without changing the C++20 baseline.

## Hashing and policy equality

`std::hash<Tensor<T, Allocator>>` hashes extents and logical values, not physical
strides or allocator state. Equal owners therefore hash equally, including
positive and negative floating-point zero when the element hash follows the
standard equality law.

`TensorEquality.h` integrates owning tensors with `areEqual`. `exactEqual` and
`approxEqual` accept any readable owner/view pair with the same value type.

## Serialization

```cpp
auto bytes = serialize_tensor(readable);
auto loaded = deserialize_tensor<float>(*bytes);

TensorDeserializationLimits limits;
limits.max_elements = 1'000'000;
limits.max_payload_bytes = 8 * 1024 * 1024;
auto bounded = deserialize_tensor<float>(*bytes, limits);
```

Serialization writes canonical logical values and accepts owners or views.
Deserialization always produces an owner. Version 2 distinguishes a rank-zero
scalar from an empty tensor and uses portable big-endian fields on supported
integer and IEEE-754 targets.

Deserializer rank, extent, element, and byte budgets are checked before Tensor
element storage allocation. A supplied-allocator overload lets applications
choose the result memory resource.

## Current boundaries

The following plan items are not yet current API promises:

- mixed-type elementwise promotion, division, and the remaining broad numeric
  operation families;
- advanced materialization helpers such as overlap-safe `copyFrom` and
  copy-forcing reshape;
- named dynamic linear algebra beyond `matmul`;
- explicit parallel execution contexts;
- a complete einsum grammar.

The existing einsum facade remains a documented pattern subset until named
linear algebra replaces it atomically. `StaticTensor` remains a separate,
fixed-size type with its own checked/saturating arithmetic policies.

## API summary

| Category | Current API |
|---|---|
| Owner | `Tensor<T, Allocator>` |
| Runtime metadata | `DynamicExtents`, `TensorStrides`, `TensorLayout`, `TensorLayoutKind` |
| Ownership transfer | `Tensor::adopt` |
| Borrowing | `TensorView<T>::borrow`, `asView`, `asConstView` |
| Shared lifetime | `SharedTensorView<T>::share`, `asSharedView` |
| View transforms | `sliceView`, `rowView`, `columnView`, `transposeView`, `reshapeView`, `broadcastView` |
| Extended transforms | `Slice`, `All`, `NewAxis`, `Ellipsis`, `permuteView`, `squeezeView`, `unsqueezeView` |
| Materialization | `clone` |
| Base algorithms | `add`, `subtract`, `multiply`, `transform`, `exactEqual`, `approxEqual` |
| Reductions | `sum`, `product`, `mean`, `min`, `max`, `argmin`, `argmax` |
| Linear algebra | `matmul` |
| Composition/selection | `stack`, `concatenate`, `take`, `takeAlongAxis`, `gatherND` |
| Interop | `contiguousSpan`, `describeTensor`, `StridedTensorDescriptor`, `asMdspan`, `toTensor`, `toStaticTensor` |
| Owner queries | `extents`, `layout`, `strides`, `rank`, `extent`, `size`, `empty`, `data` |
| Access | `operator[]`, `atLinear`, `operator()`, `at`, `begin`, `end` |
| Owner operations | `fill`, `clone`, `swap`, `get_allocator`, `operator==` |
| Arithmetic operators | `operator+`, `operator-`, `operator*` |
| Serialization | `serialize_tensor`, `deserialize_tensor`, `TensorDeserializationLimits` |
