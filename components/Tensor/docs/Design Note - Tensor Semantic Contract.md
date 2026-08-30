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
last_verified: "2026-08-19"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "draft"
---

# Design Note - Tensor Semantic Contract

**Status:** Experimental  
**Contract version:** 0.3  
**Applies to:** `Tensor`, `StaticTensor`, tensor views, and tensor algorithms  
**Stability:** This design note is intentionally not an API or wire-format stability promise.

## Scope

This design note records the shared semantic decisions implemented by the Tensor
layout, owner/view, and serial-kernel foundation. It covers rank, extents, size,
strides, layouts, ownership, lifetime, constness, aliasing, copying, transforms,
iteration, numeric boundaries, errors, execution, and serialization. It
distinguishes current decisions enforced by tests from later numeric, slicing,
execution, and contraction contracts.

## Not covered

- General einsum, sparse storage, GPU execution, or automatic differentiation.
- A final numeric promotion table or reduction policy.
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
  iteration plan. Extended slice syntax will create them in a later phase.
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
- `broadcastView` returns `TensorView<const T>` or
  `SharedTensorView<const T>`; `clone` performs explicit materialization.
- Internal broadcast mappings may be used as read-only algorithm inputs.
- Assignment to a future view rebinds the view; element transfer uses an
  explicitly named `copyFrom` operation.
- `copyFrom` must eventually specify overlap handling. The default target rule
  is temporary materialization when overlap cannot be proven safe.

### 6. Copying and materialization

- Copying an owning `Tensor` creates independent storage and preserves logical
  values, independent of the source layout.
- An ordinary or allocator-compatible move of an owning `Tensor` transfers its
  storage handle. An unequal allocator-extended move, or unequal move assignment
  with POCMA disabled, materializes logical elements through the destination
  allocator instead.
- `clone()` is an unconditional deep logical copy into canonical storage.
- A future `contiguousCopy()` is a more descriptive materialization convenience,
  not a conditional aliasing operation.
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

The target `SliceSpec` must define signed start, stop, and step values; omitted
endpoints; negative indices; negative steps; empty slices; integer axis removal;
`newaxis`; ellipsis; squeeze; and unsqueeze. A zero step is always invalid.

`reshapeView` may succeed only when the existing mapping can represent the new
shape without reordering logical elements. A separate materializing reshape may
copy.

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

Before axis reductions or mixed-type arithmetic become stable, each operation
must define:

- Result and accumulator types.
- Integer overflow and integer mean behavior.
- Empty-domain behavior and reduction identities.
- NaN handling.
- Axis normalization, duplicate axes, and `keepdims`.
- Deterministic versus implementation-dependent reduction order.

Mixed-type arithmetic and reductions are intentionally absent from the new base
surface until these rules are decided. Current `add`, `subtract`, `multiply`,
and `transform` require one value type and do not establish future promotion.
For integral element types other than `bool`, the three named binary operations
throw `std::overflow_error` on overflow or underflow instead of evaluating an
undefined signed operation or wrapping an unsigned operation. `approxEqual` is
restricted to floating-point values and tolerances; exact integral comparison
uses `exactEqual`.

### 10. Errors and execution

#### Result allocation

| Operation category | Result allocator rule |
|---|---|
| Owner constructor | Use the supplied allocator instance; otherwise use the allocator type's default instance. |
| Allocating owner member | Use `select_on_container_copy_construction` on the receiver allocator. |
| Allocating binary owner member | The left receiver supplies the allocator under the owner-member rule. |
| Free allocating algorithm with explicit allocator | Use that exact allocator instance. |
| Free allocating algorithm without explicit allocator | Use `select_on_container_copy_construction` on the first owner argument from left to right; a view-only call uses the result type's default allocator. |
| `clone` / materialization from an owner | Follow the allocating owner-member rule. |
| `clone` / materialization from a view | Use an explicit allocator when supplied; otherwise use the result type's default allocator. |
| Borrowed or shared view transform | Allocate no element storage and retain its existing lifetime model. |
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
| Copied-view iterators terminate and mutable aliases fail at construction | `test_TensorView.cpp::iterator_identity_and_writable_layout_guards` |
| Owner invalidation and throwing element-copy rollback | `test_Tensor.cpp::invalidation_matrix_and_copy_exception_safety` |
| Same-type integral arithmetic rejects overflow and underflow | `test_TensorAlgorithms.cpp::integral_arithmetic_is_checked` |
| Equality and hashing ignore physical layout | `test_TensorEquality.cpp::readable_layout_independence` |
| Extended slicing preserves signed layout reachability | `test_TensorSlice.cpp::negative_steps_and_omitted_bounds`; `test_TensorSlice.cpp::integer_ellipsis_and_newaxis` |
| Axis reductions have checked deterministic scalar, empty, and strided semantics | `test_TensorReductions.cpp::all_and_axis_reductions`; `test_TensorReductions.cpp::empty_initial_nan_and_ties` |
| Interop rejects temporaries and preserves mapping, constness, and Debug lifetime | `test_TensorInterop.cpp::contiguous_span_contract`; `test_TensorInterop.cpp::strided_descriptor_roundtrip` |
| Matmul covers vector, matrix, strided, batched, and zero-contraction forms | `test_TensorMatmul.cpp::vector_and_matrix_forms`; `test_TensorMatmul.cpp::contiguous_strided_and_batched`; `test_TensorMatmul.cpp::empty_and_zero_inner_dimensions` |
| Composition and indexed selection validate shape and bounds before evaluation | `test_TensorSelection.cpp::stack_pair_and_many`; `test_TensorSelection.cpp::take_and_take_along_axis`; `test_TensorSelection.cpp::gather_nd_and_zero_depth` |

These tests are conformance seeds, not a proof over arbitrary layouts.
Overlap-safe `copyFrom`, the complete mixed-type numeric policy, execution
contexts, expanded benchmarks, and contraction planning remain target-only.
Their phase exit gates require named tests before introduction.

## Implementation Status

The public runtime vocabulary is fixed as `DynamicExtents`, `TensorLayout`,
`SliceSpec`, `TensorView<T>`, `TensorView<const T>`, and
`SharedTensorView<T>`. Borrowed factories are lvalue-qualified. Compile-time
`Shape` remains the `StaticTensor` vocabulary.

Rank/scalar rules, checked signed layouts, distinct owner/view types, allocator
propagation, read-only broadcasting, explicit clone materialization, unified
serial kernels, extended slicing, checked axis reductions, borrowed interop,
native matmul, indexed selection, and serializer resource limits have current
executable evidence. Complete mixed-type promotion, execution contexts,
expanded benchmark evidence, and general contractions remain target-only until
their named phase exit gates pass in the architecture additions plan.

The remaining policy decisions are deliberately owned by later phase contracts:

- `copyFrom` overlap behavior beyond the required safe staging fallback.
- Raw `data()` semantics for arbitrary negative-stride mappings; descriptor and
  optional `mdspan` interop are explicit alternatives.
- Numeric result and accumulator promotion beyond the current reduction and
  matmul types.
- Serializer extension framing and checksums.
- Any storage model that supports allocator fancy pointers; the current public
  constraint remains `allocator_traits<Allocator>::pointer == T*`.
