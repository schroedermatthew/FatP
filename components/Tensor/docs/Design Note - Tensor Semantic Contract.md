---
doc_id: DN-TENSOR-001
doc_type: "Design Note"
title: "Tensor Semantic Contract"
fatp_components: ["Tensor", "TensorStatic", "TensorSerializer"]
topics: ["rank-zero tensor", "zero-extent tensor", "tensor ownership", "strided layout", "broadcast aliasing", "tensor serialization"]
constraints: ["owner and view lifetime", "signed stride reachability", "overlapping tensor mappings", "portable wire format"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.MultiArray (partial semantic overlap)"
build_modes: ["Debug", "Release"]
last_verified: "2026-08-31"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "draft"
---

# Design Note - Tensor Semantic Contract

**Status:** Experimental  
**Contract version:** 0.9\
**Applies to:** `Tensor`, `StaticTensor`, tensor views, and tensor algorithms  
**Stability:** This design note is intentionally not an API or wire-format stability promise.

## Scope

This design note records the shared semantic decisions implemented by the Tensor
layout, owner/view, and serial-kernel foundation. It covers rank, extents, size,
strides, layouts, ownership, lifetime, constness, aliasing, copying, transforms,
iteration, numeric boundaries, errors, execution, and serialization. It
distinguishes current decisions enforced by tests from later numeric-family,
execution, and contraction contracts.

## Not covered

- General einsum, sparse storage, GPU execution, or automatic differentiation.
- In-place arithmetic, configurable conversion policies, or compensated/parallel reductions.
- A stable C++ ABI or stable tensor wire format.

## Prerequisites

- Familiarity with multidimensional extents and element strides.
- Familiarity with owning containers, borrowed views, and shared ownership.
- C++20 library development and Fat-P test conventions.

## Design Note Card

**Decision:** Establish one experimental semantic contract before extending the Tensor API.  
**Context:** Dynamic Tensor, StaticTensor, views, iteration, and serialization previously disagreed on foundational meanings.  
**Options considered:** Preserve legacy behavior; add algorithms first; or settle semantics and executable conformance first.  
**Chosen option:** Settle semantics first, enforce implemented decisions with tests, and mark unimplemented portions as target contract.  
**Rationale:** Ownership, rank, layout, aliasing, and numeric rules constrain every later Tensor API.  
**Implications:** Rank zero means one-element scalar, empty tensors carry a zero extent, public broadcast is read-only, and ownership is distinct from mapping metadata.

## Purpose

This design note defines the semantic foundation that tensor APIs must share before the
library adds more slicing, reductions, execution backends, or contraction
algorithms. Tests are the executable evidence for decisions already implemented.
Items labeled **target contract** describe the required direction but are not yet
claims about the current public API.

The key words **must**, **must not**, **should**, and **may** are normative.

## Canonical vocabulary

| Term | Meaning |
|---|---|
| Rank | Number of logical axes. Rank is the number of extents. |
| Extent | Non-negative logical length of one axis. |
| Shape | Ordered collection of extents. |
| Stride | Signed distance in elements between adjacent indices on an axis. |
| Layout | Origin, extents, strides, and accessible storage bounds together. |
| Injective layout | Each logical index addresses a distinct element. |
| Broadcast layout | A non-injective layout containing at least one zero stride on an expanded axis. |
| Indeterminate layout | A large higher-rank layout for which bounded proofs establish neither injectivity nor overlap. |
| Owning tensor | A value that controls independent element storage. |
| Borrowed view | A mapping that does not extend storage lifetime. |
| Shared view | A mapping that participates in storage lifetime. |
| Materialization | Creation of independent owning storage in logical order. |

The existing compile-time `Shape<Dims...>` belongs to `TensorStatic`. Runtime
metadata uses `DynamicExtents`, `TensorStrides`, and `TensorLayout`, avoiding a
second public type named `Shape`.

## Options Considered

### Preserve the combined owner/view Tensor

Keep storage ownership, view metadata, traversal policy, and execution policy in
one dynamic type.

- Benefit: fewer public type names.
- Cost: copy, move, allocator, constness, lifetime, and mutation behavior depend
  on hidden provenance rather than the static type.

### Add algorithms before settling semantics

Extend the current type with reductions, matrix multiplication, and a broader
einsum surface.

- Benefit: visible feature growth arrives quickly.
- Cost: every algorithm duplicates unresolved layout, promotion, error, and
  aliasing decisions.

### Establish explicit contracts and distinct owner/view types (selected)

Define the semantic rules first, represent ownership and mapping with distinct
types, and build algorithms over validated layouts and shared iteration plans.

- Benefit: invalid states and writable broadcast aliases can be rejected at
  type or construction boundaries.
- Cost: the change is repository-wide and later algorithms wait for their
  prerequisites.

## Decision Rationale

Ownership, layout reachability, constness, and numeric rules constrain every
Tensor operation. Distinct types make those contracts visible to callers, while
checked extents and layouts provide one place to validate pointer-relevant
arithmetic. A shared iteration plan then lets correctness and optimization use
the same address model. This order minimizes duplicate mechanisms and makes each
public promise testable.

## Consequences

### Positive

- Owner copies, view copies, and shared-lifetime mappings have unambiguous
  semantics.
- Zero-, positive-, and negative-stride layouts share one reachability model.
- Algorithms can consume owners and views without assuming contiguous storage.
- Allocation, error, and mutation rules are reviewable from common tables.

### Negative

- The owner/view cutover touches headers, tests, docs, serializers, concepts,
  workflows, and call sites together.
- Readable views add public vocabulary that simple contiguous use cases do not
  otherwise need.
- Optimized kernels cannot land before the scalar layout model is executable.

### Obligations

- Every accepted decision has named executable evidence or is marked
  target-only.
- Public allocating operations follow the result-allocation table below.
- Public errors follow the error-channel table below and never report success
  after partial mutation.
- Benchmark claims are recorded only with commands, flags, environment, raw
  samples, and variance.

## Accepted decisions

### 1. Rank-zero and empty tensors

- Shape `{}` denotes a rank-zero scalar containing exactly one element.
- A default-constructed dynamic `Tensor` is empty and has canonical shape `{0}`.
- Any shape containing a zero extent has size zero.
- Rank and emptiness are independent: a scalar has rank zero and is not empty;
  an empty tensor has at least one zero extent.
- `StaticTensor<T, Shape<>>` and
  `Tensor<T>(std::vector<size_t>{})` must agree on rank and size.
- Moved-from tensors are valid empty objects with canonical shape `{0}`.

### 2. Extents, size, and strides

- Dynamic extents use `size_t`; strides use `ptrdiff_t` and are measured in
  elements, not bytes.
- Shape rank and stride rank must match for every usable layout.
- Owning dynamic tensors use canonical row-major strides. Nonempty owners have
  positive strides; canonical empty layouts may contain zero strides because
  multiplying by a zero extent collapses earlier strides.
- Empty layouts are contiguous. Axes with extent one do not constrain
  contiguity; all other axes must have canonical positive row-major strides.
- Size and canonical stride products must be checked before allocation and must
  be representable by both `size_t` and `ptrdiff_t`.
- In-memory layouts have no intrinsic rank cap. Tests use named representative
  ranks and generated ranges; serializer rank limits are configurable trust
  policy, not an in-memory semantic limit.
- In nonempty layouts, a zero stride on an axis with extent greater than one is
  reserved for read-only broadcast mappings. Zero strides in empty layouts do
  not create aliases because the layout addresses no elements.
- Negative strides are accepted by validated external views and by the shared
  iteration plan. Extended slice syntax creates them with negative steps.
- Layout validation proves that the minimum and maximum reachable offsets remain
  within the backing storage span before a view can be constructed.

### 3. Ownership and lifetime

The current contract has distinct types:

```cpp
Tensor<T>                 // owning, deep-copy value type
TensorView<T>             // mutable borrowed mapping
TensorView<const T>       // read-only borrowed mapping
SharedTensorView<T>       // optional shared-lifetime mapping
```

- A borrowed view must not outlive its storage owner.
- A shared view may survive destruction of any single owner handle.
- View factories returning borrowed views must be lvalue-qualified and must
  reject rvalues.
- External memory must use explicit `borrow`, `share`, or `adopt` operations;
  ownership must never be inferred from a raw pointer.
- `adopt` retains the supplied storage deleter. Its allocator argument governs
  future owner operations and may be supplied explicitly for a stateful or
  non-default-constructible allocator.
- Dynamic Tensor follows `allocator_traits` POCCA, POCMA, and POCS for assignment
  and swap. Its current allocator concept deliberately requires `pointer == T*`;
  fancy-pointer support would require a different storage control block.

No owner-returning view factory exists. Copying an owner deep-copies values;
copying a view rebinds mapping metadata and retains that view's lifetime model.

### 4. Constness

- Element constness is represented by `TensorView<const T>`, not by
  `const TensorView<T>`.
- A mutable owner may produce mutable or read-only views.
- A const owner may produce only read-only views.
- Constness must propagate through slice, transpose, reshape, and broadcast
  transforms.

The current factories and all derived transforms preserve this element
constness rule at compile time.

### 5. Aliasing and broadcast mappings

- Mutating algorithms require an injective destination layout unless an
  operation explicitly defines overlapping behavior.
- Public broadcast results must not expose writable zero-stride aliases.
- Mutable `borrow` and `share` construction requires a proven-injective layout;
  read-only views may represent broadcast, overlapping, or indeterminate layouts.
  This construction check normally rejects invalid copy destinations; copy
  kernels additionally retain a defensive injectivity check.
- `broadcastView` returns `TensorView<const T>` or
  `SharedTensorView<const T>`; `clone` performs explicit materialization.
- Internal broadcast mappings may be used as read-only algorithm inputs.
- Assignment to a view rebinds the view; element transfer uses an
  explicitly named `copyFrom` operation.
- `copyFrom(destination, source[, scratchAllocator])` requires a non-const
  lvalue destination, identical extents, and the same value type. It never
  resizes, rebinds, replaces storage, or implicitly broadcasts.
- `copyFrom` snapshots the source unless reachable address intervals are
  provably disjoint. Endpoints are reachable elements, compared with the
  standard pointer total order; different storage-base pointers alone do not
  prove disjointness. Negative strides, shifted bases, broadcasts, and self-copy
  obey the same rule. Overlapping intervals conservatively stage even if
  individual elements are interleaved and disjoint. Empty copies still validate
  lifetimes and matching extents before returning, without pointer arithmetic.

### 6. Copying and materialization

- Copying an owning `Tensor` creates independent storage and preserves logical
  values, independent of the source layout.
- An ordinary or allocator-compatible move of an owning `Tensor` transfers its
  storage handle. An unequal allocator-extended move, or unequal move assignment
  with POCMA disabled, materializes logical elements through the destination
  allocator instead.
- `clone()` is an unconditional deep logical copy into canonical storage.
- Existing `clone(owner)` copy-constructs elements and does not require default
  initialization. `clone(view)` and `clone(source, allocator)` default-initialize
  result elements and copy-assign them, so those paths require both operations.
  "Unconditional" denotes independent storage, not unrestricted element-type
  support; these existing overloads are unchanged by this materialization step.
- `clone` remains the canonical-copy name; no synonymous `contiguousCopy`
  entry point is introduced.
- `reshapeCopy(source, target[, allocator])` is an unconditional canonical copy
  of the source's logical row-major values into a shape with the same element
  count. Scalars and empty shapes follow the normal logical-count rules.
- `reshapeCopy` and `copyFrom` require default-initializable,
  copy-assignable elements. General non-default-initializable or move-only
  element materialization is not part of these helpers' current contract.
- Current allocating member operations choose their result allocator with
  `select_on_container_copy_construction` from the receiver. For binary member
  operations, the left-hand receiver supplies that allocator.
- An unequal non-propagating allocator move copies logical values whenever a
  shared view retains the source storage, preserving that alias. A unique source
  may move its elements. A non-copyable shared source is rejected before
  destination element storage allocation.
- `is_contiguous()` is only a query; conversion APIs must not sometimes alias
  and sometimes allocate under the same name.
- View and materializing transforms must have visibly different names, such as
  `transposeView` and `transposeCopy`.

### 7. Slicing and transforms

`SliceSpec` defines signed start, stop, and step values; omitted
endpoints; negative indices; negative steps; empty slices; integer axis removal;
`newaxis`; ellipsis; squeeze; and unsqueeze. A zero step is always invalid.

Signed `Slice` normalization requires the selected source-axis extent to fit
in `ptrdiff_t`, and the resulting stride product must also be representable.
It throws `std::overflow_error` otherwise, even if another axis makes the
tensor empty. `All` and integer indexing do not have that extent-normalization
requirement: they can preserve/select an enormous empty axis. Consequently
`Slice{}` and `All` are not interchangeable above `PTRDIFF_MAX`. This is an
existing representation limit, not a promise of arbitrary-size signed slicing.

`reshapeView` may succeed only when the existing mapping can represent the new
shape without reordering logical elements. `reshapeCopy` accepts noncontiguous
sources by copying their logical sequence and never returns an alias.

### 8. Iteration and invalidation

- Owner pointer iterators visit canonical contiguous storage. View iterators
  visit every element exactly once in row-major logical order and use a logical
  count as the sentinel state. Copied equivalent mappings share an iterator
  equality domain.
- `TensorIterationPlan` supports owners and positive, zero, and negative-stride
  inputs without forming out-of-allocation sentinel pointers.
- Generic materialization, fill, unary, binary, equality, approximate equality,
  and hashing share the plan rather than implementing private offset walkers.
  Canonical owner copy construction may use direct contiguous storage.
- Owner destruction, assignment, move, and swap invalidate borrowed views.
  Shared views retain element storage. View assignment rebinds metadata.

### 9. Numeric operations

#### Elementwise arithmetic types

`add`, `subtract`, `multiply`, `divide`, and their `+`, `-`, `*`, `/` operators accept
owners or readable views with different standard arithmetic element types.
Their public result alias is `TensorArithmeticType<A, B>`; the element-type
concept `TensorArithmeticCompatible<A, B>` reports whether that alias exists.
Both normalize cv/ref qualifiers. Results are fresh canonical owners, use
trailing-axis broadcasting, and never alias or modify either input.

| Inputs (rules applied in order) | Result element type |
|---|---|
| `bool`, non-arithmetic types, or integers wider than 64 value bits | Unsupported; constraints reject the call |
| Same non-bool arithmetic type | That exact type; narrow integers do not automatically widen |
| Two floating types | Usual wider floating type; `long double` wins even when its representation equals `double` |
| `float` and an integer | `float` if its binary significand covers all integer value bits; otherwise `double` |
| `double` or `long double` and an integer | The supplied floating type |
| Different integers with the same signedness | Operand with more value bits; equal-range ties use the usual C++ arithmetic conversions, including narrow integer promotions |
| Signed and unsigned integers | Keep the signed operand type if it represents the unsigned range; otherwise the first of `int16_t`, `int32_t`, `int64_t` representing both ranges |
| Signed/unsigned pair without such an integer type | Unsupported, even when the actual input values happen to fit |

The rules are symmetric in element types. Standard character types are numeric
code-unit values, not text: `char` and `wchar_t` follow the implementation's
signedness/range, as they do for reductions. No complex numbers, enums, or
user-defined arithmetic types are supported by these four operations.

On the supported binary-floating platforms, examples are
`int8_t + uint8_t -> int16_t`, `int32_t + uint32_t -> int64_t`,
`uint8_t + uint16_t -> uint16_t`, `float + int16_t -> float`, and
`float + int32_t -> double`. Signed integers paired with `uint64_t` are
rejected. `long double` is never introduced unless an operand already uses it.
On a non-binary-float implementation the float/integer rule conservatively
selects `double`.

Both operands convert to the result type **before** computing. Integral
conversions preserve every input value; the result operation checks its own
range and throws `std::overflow_error` for overflow or underflow. This does not
promise every possible arithmetic result fits: `int16_t{32767} + uint8_t{1}`
still overflows its `int16_t` result. Floating conversions and operations may
round, overflow to infinity, or produce NaNs; they do not use integer overflow
exceptions. In particular, 64-bit integers can round when converted to
`double`. Signed zero and non-finite results follow ordinary typed arithmetic.

**Behavior change:** binary arithmetic on `bool` is rejected, including
`bool + bool`. The former implicit conversion back to bool is not retained.
Use `all`/`any` for truth reductions or `sum` for counting true values.

Both input lifetimes and broadcast extents are checked before result element
allocation. Borrowed-lifetime diagnostics apply only in assertions-enabled
builds; dangling Release views remain unsupported. An explicit allocator must
have the result value type and is used unchanged. Otherwise select the first
owner left-to-right, rebind its allocator to the result type, **then** apply
SOCCC on that rebound allocator. View-only calls use `TensorAllocator<Result>`.
As required by the standard Allocator model, that rebound allocator must be
constructible from the source allocator; an explicit result allocator bypasses
owner-allocator selection.
Empty results still carry this allocator but allocate no result elements.
A nonempty result uses one result element buffer; layout/iteration metadata may
allocate separately. Allocation or checked-arithmetic failure publishes no
partial result, releases the unpublished buffer, and preserves both inputs.
Callers synchronize concurrent writes through aliases.

`transform` retains its source value type and existing callback conversions.
`copyFrom`, `exactEqual`, and `approxEqual` still require matching element
types. `approxEqual` also requires floating-point values and tolerances.
Compound updates have the transactional contract below; additional unary
families remain future work.

#### Division

`divide` and `/` use the same result type and convert both operands before
computing, for tensor/tensor and both scalar orders. Integer division returns
an integer quotient truncated toward zero, not floor division or an implicit
floating result: `-7 / 2` is `-3`. With an integral result type, an evaluated zero divisor throws
`std::domain_error`. The signed result type's `lowest() / -1` throws
`std::overflow_error` before language division, including narrow integer types
that C++ would promote to int. No wrap or saturation is allowed.

These checks concern the **promoted result**, not either original operand:
`Tensor<int8_t>(..., -128) / int{-1}` produces int `128`, but division by an
int8_t negative-one scalar throws. A floating operand selects floating
division; use `integers / 2.0` or an explicit `cast<double>` when fractional
quotients are required. Integer-only division never converts through floating
point, including uint64 values above the exact double range.

Floating division performs native typed `/`, without integer domain/overflow
exceptions or reconfiguring the caller's floating-point environment. Native
operations may set floating exception flags. On supported IEC 559
platforms with the default nontrapping environment, signed zero divisors can
produce signed infinities; zero/zero and infinity/infinity produce NaN;
NaNs propagate; finite overflow and underflow follow native rounding.
No NaN payload/sign, exception-flag suppression, or trap interception is
promised. Compiler modes that discard NaN or signed-zero semantics do not
provide this contract. Other floating implementations retain their native
behavior, not an emulated IEC 559 guarantee.

Lifetime and broadcast validation precede result element allocation. Numeric
checks occur during evaluation, not in a divisor pre-scan: allocation failure
may precede a numeric error. A later invalid quotient releases the unpublished
result without modifying either input. Empty outputs evaluate no quotient,
so an empty integer tensor divided by zero succeeds with an empty result and
no element allocation; rank zero evaluates its one quotient and throws for
an integer zero divisor. Allocator selection, explicit allocator constraints,
source scalar snapshots, and alias/lifetime obligations are unchanged.

#### Scalar operands

Each of `add`, `subtract`, `multiply`, `divide`, and `+`, `-`, `*`, `/` also accepts
one readable Tensor operand and one arithmetic scalar in either order.
Named operations accept an explicit result allocator as a third argument;
`subtract(tensor, scalar, allocator)` and
`subtract(scalar, tensor, allocator)` retain their respective operand order.
These allocating forms return independent owners; no scalar-only overloads
are introduced. Compound updates have the separate contract below.

Result type and integer overflow behavior use the existing
`TensorArithmeticType<T, Scalar>` rules; literal type matters. For example,
`Tensor<int8_t> + 100` produces `Tensor<int>`, while adding an `int8_t`
scalar retains the checked 8-bit result. A supplied long-double scalar can
produce a long-double result. Bool, enum, and user-defined scalar types remain
excluded, just as for tensor/tensor arithmetic.

The scalar is copied by value before computation, including when it refers
to an input element. Output extents exactly match the tensor operand.
Scalar operations use the existing unary kernel with a captured scalar;
no temporary scalar Tensor or intermediate element buffer is allocated.
Rank zero invokes the operation once; empty extents invoke it zero times.
The tensor owner's allocator is rebound then SOCCC-selected regardless of
operand order. Views use `TensorAllocator<Result>`; an explicit allocator
wins unchanged. Lifetime validation precedes result element allocation.
Checked failure releases the unpublished output and leaves the source intact.

#### Transactional compound arithmetic

`addAssign`, `subtractAssign`, `multiplyAssign`, `divideAssign` and
`+=`, `-=`, `*=`, `/=` update a non-const lvalue owner or writable view
and return that same destination by reference. Const handles, read-only
element views, temporary destinations, bool arithmetic, and unsupported
arithmetic type pairs are rejected at compile time. A named operation accepts
an optional third argument specifying the scratch allocator.

The RHS is a readable tensor or arithmetic scalar. Tensor RHS values
broadcast over trailing axes, but the resulting extents must exactly equal
the destination's original extents, including rank. An extra leading singleton
RHS axis is therefore rejected. Neither rank, extents, strides, storage address,
allocator, nor existing borrowed/shared view validity changes.

Each result is computed in `TensorArithmeticType<DestinationValue, RhsValue>`
using the existing checked arithmetic rules, then converted back using
`cast<DestinationValue>`'s element-conversion rules. In particular:

- Integer `/= 2` truncates toward zero; integer `/= 2.0` rejects a
  fractional quotient instead of silently truncating on write-back.
- Promoted computation can succeed while destination conversion overflows:
  an int8_t destination containing -128 cannot `/= int{-1}`.
- Unsigned underflow throws. Full-width uint64_t with a signed literal remains
  an unsupported promotion pair; use `+= 1u`, not `+= 1`.
- Floating back-conversion permits rounding, underflow, and NaN/infinity
  category preservation, but rejects a finite value outside destination range.
  Floating computation retains the native floating-environment policy.

All prospective values are computed before any destination store. Overlapping,
shifted, permuted, broadcast, and self RHS aliases therefore observe the original
values. A scalar referencing a destination element is captured before scratch
element allocation. Callers must exclude concurrent/reentrant alias mutation;
hardware floating traps are not C++ exceptions and are outside this guarantee.

Any C++ exception leaves destination values and metadata unchanged. Nonempty
updates use one canonical destination-value-typed scratch element buffer.
The commit uses the shared copy kernel: validation and all traversal metadata
allocation finish before the first store; validated reachable coordinates
make subsequent checked offset arithmetic nonthrowing, and primitive element
assignment is statically required to be nonthrowing. No swap or owner
replacement occurs. Allocator bookkeeping and floating status flags are not
rolled back. Scratch is reclaimed on success and failure.
The scratch owner value-initializes its element storage before computation
and creates normal owner lifetime metadata; these are deliberate reuse costs.

Explicit scratch allocators must have the destination value type, not the
promoted compute type, and are used unchanged. Otherwise an owner supplies its
exact allocator without rebinding or SOCCC; a view uses
`TensorAllocator<DestinationValue>`. The RHS never selects scratch allocation.
After shape/lifetime validation, empty updates evaluate no elements and
allocate no scratch elements; metadata may allocate separately. Rank zero
evaluates one element. Shape errors precede scratch element allocation;
allocation failure can precede a later arithmetic/conversion error.
Debug borrowed-lifetime checks still apply to empty operands.

#### Checked materializing casts

`cast<To>(source)` and `cast<To>(source, allocator)` convert logical values
into a fresh canonical `Tensor<To>` with identical extents. `To` must be an
unqualified standard arithmetic value type: bool, standard integers up to
64 bits, or float/double/long double. Standard character types use numeric
range and signedness, not text encoding. Cv/ref targets, enums (including
`std::byte`), pointers, complex numbers, and user-defined numeric types are
rejected by constraints. Explicit `cast<long double>` is permitted.

This is a range/domain-checked conversion, **not** an exactness policy:

| Conversion | Rule |
|---|---|
| Same type | Copy values unchanged into independent storage, including floating NaNs |
| Bool to numeric | False becomes zero and true becomes one |
| Numeric to bool | Only exactly zero or one is accepted; negative zero is zero; other values throw `std::domain_error` |
| Integer to integer | Value must fit the destination range; otherwise `std::overflow_error` |
| Floating to integer | Require finite, then integral-valued input; violations throw `std::domain_error`. Then check range and throw `std::overflow_error` if outside it |
| Integer to floating | Allow rounding; every supported integer fits the range of a standard floating type |
| Floating to floating | Allow rounding and subnormal underflow to zero; reject finite magnitude above destination maximum with `std::overflow_error` |
| Type-changing floating NaN/infinity | Preserve category and infinity sign when supported; otherwise `std::domain_error`. No NaN payload/sign preservation is promised |

Floating-to-integer conversion never truncates a fractional value, saturates,
or wraps. Error precedence is deliberate: `-0.5 -> unsigned` is a domain
error before range checking. Signed zero converts to integer zero. On the
supported binary-floating platforms the exclusive upper limits `2^63` and
`2^64` are rejected for signed64 and unsigned64 respectively; rounded
floating representations of integer maxima cannot slip through an inclusive
maximum comparison.

Floating destinations may lose precision: `9007199254740993 -> double`
can become `9007199254740992`. Finite overflow is rejected even if hardware
rounding might otherwise round a just-too-large value down to the maximum.
Tiny values may nevertheless round to signed zero. Floating results follow
the implementation's normal conversion/rounding behavior; no exact-roundtrip,
saturation, or truncation policy is added.

Even same-type and rvalue-source casts materialize independent element storage.
Source lifetime validation precedes result allocation; value validation occurs
during logical iteration, so allocation failure can precede a value error.
A later conversion failure releases the unpublished result and leaves all
source values unchanged. Rank zero converts one value; zero extents convert
none. Output uses the explicit allocator unchanged or the owner allocator
rebound to `To` before SOCCC; view-only inputs use `TensorAllocator<To>`.
A nonempty output allocates one result element buffer; empty output allocates
none. Layout/iteration metadata may allocate separately. Callers synchronize
concurrent alias writes; Debug lifetime diagnostics do not make dangling
Release views safe.

#### Current reduction types

`TensorReductions.h` accepts standard arithmetic element types, including
`bool`; complex and user-defined numeric types are not supported. Results are
fresh canonical owners, not aliases. The source is never modified.
Character types are treated as numeric code-unit values, not decoded text;
their widening follows the integer width/signedness rows below. In particular,
plain `char` and `wchar_t` follow their implementation-defined signedness.

| Operation / input | Result and accumulator type |
|---|---|
| `sum`, `product` on `bool` | `std::size_t`; true converts to one, false to zero |
| `sum`, `product` on signed integers smaller than `std::int64_t` | `std::int64_t` |
| `sum`, `product` on unsigned integers smaller than `std::int64_t` | `std::uint64_t` |
| `sum`, `product` on other arithmetic types | Source value type, including `float`, `double`, and `long double` |
| `mean` on `long double` | `long double` |
| `mean` on every other supported type | `double` |
| `min`, `max` | Source value type |
| `argmin`, `argmax` | `std::size_t` index; comparisons retain the source value type |
| `all`, `any` | `bool`; each input converts to `bool` before combining |

The public aliases `TensorSumType<T>` and `TensorMeanType<T>` express these
rules for the unqualified source value type. Sum and product deliberately keep
floating input precision rather than silently widening every accumulator.
Mean uses a separate floating conversion-and-division rule; compensated
accumulation is not implemented.

#### Axes, order, and exceptional values

- An omitted or empty axis list reduces all axes; it does not mean a no-op.
  Negative axes normalize against rank. Duplicates after normalization throw
  `std::invalid_argument`; out-of-range axes throw `std::out_of_range`.
- Reduced axes are interpreted in ascending source-axis order, independent of
  the caller's list order. `keepDimensions` retains them with extent one.
  A rank-zero input has one value and no explicit axis zero.
- Each output domain folds its logical row-major source values serially.
  Broadcast and overlapping read-only mappings repeat logical values even
  when storage addresses coincide. No short-circuit traversal is promised.
- `sum` and `product` convert each value to the accumulator type first. Every
  integral combine checks overflow/underflow before evaluation and throws
  `std::overflow_error`, including intermediate overflow that a later operand
  could mathematically cancel. Floating combines use ordinary typed arithmetic.
- `mean` converts each input to its floating accumulator, sums from zero,
  then divides by the domain count converted to that type. Integer inputs and
  counts may round during conversion. Consequently `mean * count` need not
  equal `sum`, even when an integral sum is exact. This is not a checked
  conversion API or a compensated mean.
- In floating environments supporting infinities and NaNs, arithmetic can
  produce them without an integral-overflow exception. Deterministic order is
  not a cross-platform bitwise reproducibility guarantee. Floating environment,
  compiler reassociation/fast-math settings, and type precision affect results;
  the conformance tests use ordinary non-fast-math builds and default rounding.
- Extrema propagate the first NaN in a domain. Otherwise the first tied value
  wins, including its signed-zero representation. An explicit extremum
  `initial` participates before source values, including NaN and tie selection.
- `argmin`/`argmax` return the first winning coordinate flattened row-major over
  the reduced axes in ascending source-axis order, not a backing-storage offset.
  They have no `initial` argument: each index must identify a source coordinate.
- `all`/`any` use arithmetic-to-boolean conversion: positive and negative zero
  are false; nonzero values, infinities, and NaNs are true. Boolean sum counts
  true values, boolean product multiplies their zero/one values, and boolean
  mean gives their floating fraction.

#### Empty domains, initial values, and allocation

An empty output has no domains: every reduction returns that empty owner after
validating the source and axes. It does not count unreachable reduced extents.
Output shape and canonical-layout representability still apply. Reducing away
a zero axis can produce a nonempty output; the source being empty does not
waive output-size checks or allocation requirements.

| Operation on an empty domain in a nonempty output | Result |
|---|---|
| `sum` | Zero, or the supplied `initial` |
| `product` | One, or the supplied `initial` |
| `all`, `any` | True, false, respectively |
| `min`, `max` | Supplied `initial`; otherwise `std::domain_error` |
| `mean`, `argmin`, `argmax` | `std::domain_error` |

The `initial` for sum/product/min/max seeds each output domain exactly once,
including nonempty domains. It is typed as the result value; conversions in
caller arguments follow normal C++ rules and are not checked by the reduction.
The default floating sum starts at positive zero.

An explicit result allocator is used unchanged. Without one, an owner input
supplies its allocator rebound to the result type and selected with SOCCC;
borrowed/shared views use `TensorAllocator<Result>`. A nonempty result uses one
element buffer; an empty result uses none. Metadata and extrema scratch buffers
may allocate separately through standard allocators. No global allocation-free
or caller-controlled-scratch guarantee is made.

Source/axis validation precedes result element allocation. Allocation or
arithmetic failure publishes no partial result and leaves the source unchanged;
RAII releases any unpublished result buffer. Shape arithmetic can throw
`std::overflow_error`, and allocation can throw `std::bad_alloc`. Borrowed-view
lifetime diagnostics apply in assertions-enabled builds; dangling borrowed
sources remain invalid in Release. Shared sources retain storage lifetime.

### 10. Errors and execution

#### Result allocation

| Operation category | Result allocator rule |
|---|---|
| Owner constructor | Use the supplied allocator instance; otherwise use the allocator type's default instance. |
| Allocating owner member | Use `select_on_container_copy_construction` on the receiver allocator. |
| Allocating binary owner member | The left receiver supplies the allocator under the owner-member rule. |
| Free allocating algorithm with explicit allocator | Use that exact allocator instance. |
| Free allocating algorithm without explicit allocator | Rebind the first owner argument's allocator (left to right) to the result type, then apply `select_on_container_copy_construction`; a view-only call uses `TensorAllocator<Result>`. |
| `clone` / materialization from an owner | Follow the allocating owner-member rule. |
| `clone` / materialization from a view | Use an explicit allocator when supplied; otherwise use the result type's default allocator. |
| Borrowed or shared view transform | Allocate no element storage and retain its existing lifetime model. |
| `copyFrom` overlap scratch | Explicit scratch allocator wins; otherwise use the destination owner's exact allocator without SOCCC, or `TensorAllocator<T>` for a view destination. Never select scratch from the source. Scratch is operation-local. |
| Compound arithmetic scratch | One destination-typed buffer for each nonempty update. Explicit allocator wins; otherwise the destination owner's exact allocator without SOCCC, or `TensorAllocator<T>` for a view. No RHS allocator selection. |
| Scratch storage | Use the execution context scratch allocator when supplied; serial fallback scratch is operation-local and is never retained by a result. |

#### Error channels

| Failure category | Channel | Mutation guarantee |
|---|---|---|
| Direct owner, view, or algorithm precondition violation | `std::invalid_argument` or `std::logic_error` as documented | Validate before mutation. |
| Standalone extents/layout rank, arithmetic, or reachability failure | `std::invalid_argument`, `std::overflow_error`, or `std::out_of_range` as documented | Construction publishes no partial layout. |
| Checked element bounds failure | `std::out_of_range` | No mutation. |
| Assertions-enabled access through an invalidated borrowed view | `std::runtime_error` | No mutation; Release builds do not make dangling access supported. |
| Future recoverable shape, broadcast, or transform failure | `Expected<..., TensorError>` | No mutation and no partial result. |
| Element/result allocation failure in a direct API | `std::bad_alloc` | Destination owner remains unchanged. |
| Element/result allocation failure in a recoverable API | `Unexpected(TensorError::AllocationFailure)` | No mutation and no partial result. |
| User element operation throws during a new result | Original exception | No published partial result; input operands are unchanged. |
| User element assignment throws during `copyFrom` | Original exception | Validation and overlap staging occur first; element assignment provides the documented basic guarantee. |
| Compound arithmetic validation, allocation, arithmetic, or conversion fails | Original documented exception | All destination values, storage, and metadata remain unchanged; scratch is reclaimed. |

- `copyFrom` validates before mutation. Validation, allocation, or source
  snapshot failure leaves the destination unchanged. If assignment throws in
  the final transfer, completed writes remain, storage and metadata remain
  intact, and element validity depends on the element's own assignment
  guarantee. Scratch is reclaimed; there is no rollback.
- Disjoint and empty `copyFrom` calls allocate zero scratch elements; possible
  overlap uses one buffer of the logical element count. Iteration and layout
  metadata may allocate separately, so this is not a global allocation-free
  guarantee. Both new helpers use the shared iteration plan and copy
  kernel rather than private offset walkers.

- An execution context belongs to algorithms, not tensor ownership. It controls
  executor, grain size, scratch allocation, determinism, and backend selection.
- Synchronous algorithms complete all submitted work before returning or
  throwing.

### 11. Serialization

- Portable serialization stores canonical logical values, not allocator state,
  storage ownership, OS handles, or arbitrary physical view strides.
- Rank-zero scalars encode one payload element. Empty tensors encode a shape with
  at least one zero extent and no payload elements.
- Portable big-endian interchange and native memory-mapped images are different
  formats and must remain separate.
- Version 2 interoperability requires 8-bit bytes, pure little- or big-endian
  storage, two's-complement signed integers, and IEEE-754 binary32/binary64
  floating point. The implementation rejects unsupported representations at
  compile time.
- Wire format version 2 distinguishes a rank-zero scalar from an empty tensor.
  Version 1 encoded dynamic rank zero as empty and is deliberately rejected
  rather than reinterpreted under the new scalar rule.
- The wire format remains experimental until limits, extension, checksum, and
  compatibility rules are finalized.
- Deserialization applies caller-configurable rank, extent, element-count, and
  payload byte limits before allocating Tensor element storage. Defaults cap one
  payload at 64 MiB; applications should normally choose a smaller trust-specific
  budget. Callers may supply the allocator instance used for the result.
- C++ source compatibility, binary ABI compatibility, and wire compatibility are
  separate promises.

## Current conformance evidence

| Decision | Executable evidence |
|---|---|
| Rank-zero dynamic scalar has one element; default owner is empty | `test_Tensor.cpp::owner_scalar_empty_and_access` |
| Owning copies are independent and moved-from owners are canonical empty | `test_Tensor.cpp::owner_copy_move_fill_and_hash` |
| Allocator assignment and swap follow allocator traits | `test_Tensor.cpp::owner_allocator_semantics` |
| External borrow and explicit adoption are distinct | `test_Tensor.cpp::owner_adoption_and_external_borrowing` |
| Borrowed and shared lifetime models are distinct | `test_TensorView.cpp::shared_and_borrowed_lifetime` |
| Constness propagates and public broadcast is read-only | Compile-time assertions and `test_TensorView.cpp::readonly_broadcast` |
| Clone materializes strided and broadcast logical values | `test_Tensor.cpp::view_transforms_constness_and_clone` |
| Rank-zero and empty serialization are distinct | `test_TensorSerializer.cpp::rank_zero_scalar_roundtrip`; `test_TensorSerializer.cpp::zero_extent_empty_roundtrip` |
| Serializer trust limits precede Tensor element storage | `test_TensorSerializer.cpp::deserialization_resource_limits` |
| Signed pointer-forming views stay within validated storage | `test_TensorView.cpp::external_mapping_validation` |
| Checked runtime extents and signed layout reachability | `test_TensorLayout.cpp::dynamic_extents_and_axes`; `test_TensorLayout.cpp::validation_boundaries`; `test_TensorLayout.cpp::randomized_scalar_oracle` |
| Empty, contiguous, injective, broadcast, overlap, and indeterminate classification | `test_TensorLayout.cpp::canonical_layouts`; `test_TensorLayout.cpp::signed_reachability_and_classification` |
| Counted iteration supports signed, scalar, and empty mappings | `test_TensorAlgorithms.cpp::counted_signed_iteration` |
| Three-layout broadcasting handles reversed, broadcast, and padded mappings | `test_TensorAlgorithms.cpp::binary_broadcast_three_layouts` |
| Randomized one-, two-, and three-layout plans match a coordinate oracle | `test_TensorAlgorithms.cpp::randomized_multi_layout_plan_oracle` |
| Shifted bases, signed strides, broadcasts, transpose and self-copy have snapshot semantics | `test_TensorAlgorithms.cpp::copy_from_shifted_and_permuted_aliases`; `test_TensorAlgorithms.cpp::randomized_materialization_and_overlap_oracle` |
| Reshaped copies are independent, logical-order and allocator-aware | `test_TensorAlgorithms.cpp::materialization_shapes_values_and_allocators` |
| Copy scratch has exact allocator identity/counts and failure cleanup | `test_TensorAlgorithms.cpp::copy_from_allocation_contract`; `test_TensorAlgorithms.cpp::copy_from_validation_empty_and_exceptions` |
| Shared range endpoints require staging in both directions | `test_TensorAlgorithms.cpp::copy_from_shared_endpoint_requires_staging` |
| Copied-view iterators terminate and mutable aliases fail at construction | `test_TensorView.cpp::iterator_identity_and_writable_layout_guards` |
| Owner invalidation and throwing element-copy rollback | `test_Tensor.cpp::invalidation_matrix_and_copy_exception_safety` |
| Same-type integral arithmetic rejects overflow and underflow | `test_TensorAlgorithms.cpp::integral_arithmetic_is_checked` |
| Mixed types follow a symmetric range-preserving integer promotion table | `test_TensorAlgorithms.cpp::mixed_arithmetic_type_matrix` |
| Mixed byte arithmetic matches all 65,536 value pairs | `test_TensorAlgorithms.cpp::mixed_arithmetic_exhaustive_byte_values` |
| Promoted computation, integer limits, and floating conversion are explicit | `test_TensorAlgorithms.cpp::mixed_arithmetic_numeric_boundaries` |
| Casts cover every pair of 19 arithmetic types and reject unsupported targets | `test_TensorAlgorithms.cpp::cast_type_matrix_and_identity` |
| Narrowing and bool conversion reject invalid domains | `test_TensorAlgorithms.cpp::cast_integer_and_bool_domains` |
| Floating-to-integer limits are checked before unsafe conversion | `test_TensorAlgorithms.cpp::cast_float_integer_boundaries` |
| Floating casts define rounding, tiny values, and nonfinite behavior | `test_TensorAlgorithms.cpp::cast_floating_rounding_and_nonfinite` |
| Casts and both-order scalar operations follow independent coordinates | `test_TensorAlgorithms.cpp::cast_and_scalar_layout_oracle` |
| Scalar promotions, operand order, and input-element snapshots are explicit | `test_TensorAlgorithms.cpp::scalar_numeric_boundaries_and_snapshot` |
| Cast/scalar allocator, exception, and lifetime rules are enforced | `test_TensorAlgorithms.cpp::cast_and_scalar_allocator_contract`; `test_TensorAlgorithms.cpp::cast_and_scalar_failure_and_lifetime` |
| Division type constraints cover owners, borrowed/shared views, scalar orders, and explicit allocators | `test_TensorAlgorithms.cpp::mixed_arithmetic_type_matrix`; `test_TensorAlgorithms.cpp::division_constraints` |
| Integer division rejects zero and signed-minimum overflow; signed quotients truncate toward zero | `test_TensorAlgorithms.cpp::division_integer_boundaries`; `test_TensorAlgorithms.cpp::division_exhaustive_byte_values` |
| Quotients convert before computation and snapshot aliased scalars | `test_TensorAlgorithms.cpp::division_promotion_and_snapshot` |
| Floating division preserves native values, NaN category, and signed zero/infinity | `test_TensorAlgorithms.cpp::division_floating_special_values` |
| Division handles signed/broadcast layouts and both operand orders | `test_TensorAlgorithms.cpp::division_layout_oracle` |
| Division allocators, empty/scalar distinctions, failures, and lifetimes follow the contract | `test_TensorAlgorithms.cpp::division_allocator_contract`; `test_TensorAlgorithms.cpp::division_failure_and_lifetime` |
| Compound overloads require writable non-const lvalues and preserve the existing promotion matrix | `test_TensorAlgorithms.cpp::mixed_arithmetic_type_matrix`; `test_TensorAlgorithms.cpp::compound_constraints` |
| Compound broadcasting preserves destination storage, shape, and view validity | `test_TensorAlgorithms.cpp::compound_broadcast_and_storage` |
| Checked write-back rejects narrowing/fractional results without partial updates | `test_TensorAlgorithms.cpp::compound_checked_conversion_rollback`; `test_TensorAlgorithms.cpp::compound_integer_types` |
| Overlapping compound inputs observe pre-update values; signed layouts preserve padding | `test_TensorAlgorithms.cpp::compound_aliasing_snapshots`; `test_TensorAlgorithms.cpp::compound_layout_oracle` |
| Compound scratch allocator identity, cleanup, empty behavior, and lifetimes are explicit | `test_TensorAlgorithms.cpp::compound_allocator_contract`; `test_TensorAlgorithms.cpp::compound_failure_and_lifetime` |
| All observed allocation failures, including commit traversal metadata, precede destination writes | `test_TensorAlgorithms.cpp::compound_allocation_failure_transaction` (standalone fault-injection configurations) |
| Mixed element sizes use independent signed/broadcast layouts | `test_TensorAlgorithms.cpp::mixed_arithmetic_layout_oracle` |
| Binary allocators rebind before SOCCC and validate before element allocation | `test_TensorAlgorithms.cpp::mixed_arithmetic_allocators`; `test_TensorAlgorithms.cpp::mixed_arithmetic_validation_and_cleanup` |
| Equality and hashing ignore physical layout | `test_TensorEquality.cpp::readable_layout_independence` |
| Extended slicing preserves signed layout reachability | `test_TensorSlice.cpp::negative_and_bounded_slices`; `test_TensorSlice.cpp::ellipsis_newaxis_and_integer_axis` |
| Bounded signed slices agree with independently enumerated coordinates | `test_TensorSlice.cpp::coordinate_oracle_anchors`; `test_TensorSlice.cpp::exhaustive_small_signed_slices` |
| Permutation, insertion/removal, reshape, and composed transforms preserve root addresses | `test_TensorSlice.cpp::exhaustive_small_permutations_and_axis_transforms`; `test_TensorSlice.cpp::randomized_composed_transform_oracle` |
| Rectangular slices and convenience/broadcast views match explicit coordinate selections | `test_TensorSlice.cpp::rectangular_convenience_and_broadcast_oracle` |
| Transforms allocate no owner element buffers and retain their lifetime/constness model | `test_TensorSlice.cpp::transform_allocation_aliasing_and_lifetime`; `test_TensorSlice.cpp::empty_transform_allocation_and_lifetime` |
| Extreme bounds and invalid transformations preserve source state | `test_TensorSlice.cpp::extreme_slice_bounds_and_validation` |
| Axis reductions have checked deterministic scalar, empty, and strided semantics | `test_TensorReductions.cpp::all_and_axis_reductions`; `test_TensorReductions.cpp::empty_initial_nan_and_ties` |
| Reduction coordinates match an independent output-domain oracle | `test_TensorReductions.cpp::coordinate_reference_anchors`; `test_TensorReductions.cpp::exhaustive_small_reduction_coordinates`; `test_TensorReductions.cpp::randomized_signed_layout_reductions` |
| Empty outputs do not count unreachable oversized domains; oversized nonempty outputs fail | `test_TensorReductions.cpp::empty_output_does_not_count_unreachable_domains` |
| Numeric boundaries, floating order, NaNs, ties, and boolean truth rules are explicit | `test_TensorReductions.cpp::integral_accumulator_boundaries`; `test_TensorReductions.cpp::floating_order_nan_infinity_and_signed_zero`; `test_TensorReductions.cpp::boolean_truth_and_identities` |
| Reduction axes, allocation failures, and shared/borrowed lifetimes follow the contract | `test_TensorReductions.cpp::axis_validation_and_source_preservation`; `test_TensorReductions.cpp::result_allocation_and_failure_contract`; `test_TensorReductions.cpp::shared_retention_and_borrowed_invalidation` |
| Interop rejects temporaries and preserves mapping, constness, and Debug lifetime | `test_TensorInterop.cpp::contiguous_span_contract`; `test_TensorInterop.cpp::strided_descriptor_roundtrip` |
| Matmul covers vector, matrix, strided, batched, and zero-contraction forms | `test_TensorMatmul.cpp::vector_and_matrix_forms`; `test_TensorMatmul.cpp::contiguous_strided_and_batched`; `test_TensorMatmul.cpp::empty_and_zero_inner_dimensions` |
| Composition and indexed selection validate shape and bounds before evaluation | `test_TensorSelection.cpp::stack_pair_and_many`; `test_TensorSelection.cpp::take_and_take_along_axis`; `test_TensorSelection.cpp::gather_nd_and_zero_depth` |

These tests are conformance seeds, not a proof over arbitrary layouts.
The compound oracle checks 9,600 operations: 1,200 seeded integer/mixed
signed/padded/broadcast layouts, four operators, and tensor/scalar RHS forms.
The separate allocation-failure sweep replaces ordinary global new only in
standalone configurations without MSVC checked iterators. MSVC's checked
`std::vector` move allocates a debug proxy inside a `noexcept` function;
failing that allocation terminates inside the runtime rather than producing
a catchable exception. Checked-iterator builds retain their normal checks and
explicitly skip this sweep; the other compound groups still run. The sweep
does not replace over-aligned allocation. A separate native-new ASan run
retains allocation/deallocation-mismatch diagnostics. Allocation-order evidence
complements the nonthrowing-assignment assertion and reachable-offset argument;
it does not by itself prove the absence of non-allocating throws.
The Phase 4 oracle exhausts its documented small domains and checks seeded
multidimensional compositions. Expected root offsets do not use production
slice normalization, transformed layouts, or the iteration plan. Address
comparisons and write-through checks distinguish a correct view from a
materialized copy with equal values. Element-allocation counters do not imply
that layout/iterator metadata is globally allocation-free.
The reduction oracle separately enumerates output coordinates and each selected
source-coordinate domain; expected values do not call production axis decoding,
iteration, or checked arithmetic helpers. Its finite-value grid has 4,008 cases
(ranks 0-3, extents 0-3, four layout families, every nonempty axis subset, both
dimension-retention modes, plus the rank-zero case) and 600 seeded layouts up
to rank 5 with extents 0-4. Boundary tests supplement that bounded coverage.
Further numeric families, execution contexts, expanded benchmarks,
and contraction planning remain target-only.
Their phase exit gates require named tests before introduction.

## Implementation Status

The public runtime vocabulary is fixed as `DynamicExtents`, `TensorLayout`,
`SliceSpec`, `TensorView<T>`, `TensorView<const T>`, and
`SharedTensorView<T>`. Borrowed factories are lvalue-qualified. Compile-time
`Shape` remains the `StaticTensor` vocabulary.

Rank/scalar rules, checked signed layouts, distinct owner/view types, allocator
propagation, read-only broadcasting, explicit clone/reshape materialization,
overlap-safe element transfer, mixed-type and scalar checked arithmetic including division,
transactional compound updates,
materializing numeric casts, unified serial kernels, extended slicing, checked
axis reductions, borrowed interop,
native matmul, indexed selection, and serializer resource limits have current
executable evidence. Further numeric families, execution contexts, expanded
benchmark evidence, and general contractions remain target-only until
their named phase exit gates pass in the architecture additions plan.

The remaining policy decisions are deliberately owned by later phase contracts:

- Optional optimizations beyond `copyFrom`'s conservative staging fallback.
- Raw `data()` semantics for arbitrary negative-stride mappings; descriptor and
  optional `mdspan` interop are explicit alternatives.
- Numeric families beyond the current binary/scalar operations, materializing
  casts, compound updates, reductions, and matmul, including further unary operations.
- Serializer extension framing and checksums.
- Any storage model that supports allocator fancy pointers; the current public
  constraint remains `allocator_traits<Allocator>::pointer == T*`.
