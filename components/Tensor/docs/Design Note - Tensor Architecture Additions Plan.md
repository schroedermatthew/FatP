---
doc_id: DN-TENSOR-002
doc_type: "Design Note"
title: "Tensor Architecture Additions Plan"
fatp_components: ["Tensor", "TensorLayout", "TensorSlice", "TensorView", "TensorAlgorithms", "TensorReductions", "TensorInterop", "TensorSelection", "TensorMatmul", "TensorExecution", "TensorEinsum", "TensorSerializer", "TensorStatic"]
topics: ["tensor layout", "tensor views", "strided iteration", "numeric promotion", "axis reductions", "tensor execution", "tensor contractions"]
constraints: ["signed stride reachability", "aliasing and lifetime", "checked shape arithmetic", "deterministic reduction order", "header-only C++20"]
cxx_standard: "C++20"
std_equivalent: "std::mdspan (partial layout and view overlap)"
std_since: "C++23"
boost_equivalent: "Boost.MultiArray (partial semantic overlap)"
build_modes: ["Debug", "Release"]
last_verified: "2026-08-19"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# Design Note - Tensor Architecture Additions Plan

**Status:** Phases 0-3 implemented; Phase 4-8 dependency-light cores implemented; remaining gates planned  
**Decided:** The owner/view/layout/kernel foundation and dependency-light algorithm expansion are implemented  
**Last reviewed:** 2026-08-19

## Scope

This plan orders the remaining dynamic Tensor architecture and algorithm work
after the semantic-foundation repairs recorded in `DN-TENSOR-001`. It defines
dependencies, repository boundaries, acceptance criteria, and validation gates.

## Not covered

- Sparse tensor storage.
- GPU execution or device memory.
- Automatic differentiation.
- Third-party dependencies in the Domain layer.
- A source, ABI, or wire-compatibility promise before release.

## Prerequisites

- `Design Note - Tensor Semantic Contract.md` (`DN-TENSOR-001`), repaired in
  Phase 0 before implementation begins.
- Fat-P Library Development Guidelines, especially atomic renames and the
  prohibition on compatibility shims or gradual migration.
- Fat-P test, benchmark, metadata, documentation, and layer rules.

## Design Note Card

**Decision:** Build the remaining Tensor system in dependency order, with each phase delivered as an atomic repository-wide cutover.  
**Context:** Layout, views, iteration, numeric semantics, execution, and contractions currently overlap inside a large dynamic Tensor header.  
**Options considered:** Add algorithms to the current type; replace everything in one rewrite; or use dependency-ordered atomic phases.  
**Chosen option:** Dependency-ordered atomic phases with executable entry and exit gates.  
**Rationale:** Every later algorithm depends on layout reachability, ownership, constness, and traversal semantics.  
**Implications:** No deprecated aliases, compatibility wrappers for replaced APIs, dual owner/view models, or partially migrated call sites may remain after a phase lands. Existing public symbols receive an explicit keep, replace, or remove decision before Phase 1.

## Decision

Complete the Tensor architecture through dependency-ordered phases that each
replace their affected API and all repository call sites atomically, beginning
with layout vocabulary and owner/view separation before adding more algorithms.

## Context

The current foundation has explicit scalar and empty semantics, checked shape
arithmetic, allocator-aware ownership, distinct borrowed/shared view types,
validated signed-stride layouts, a reusable multi-operand iteration plan,
bounded serialization, randomized conformance tests, and a repeatable benchmark
harness. It now also has the extended slice language, deterministic reductions,
borrowed interop descriptors, composition/selection operations, and native
vector/matrix/batched multiplication. The remaining work is architectural:

- Mixed-type elementwise promotion and the remaining boolean/numeric operation
  families are not stable contracts.
- Parallel execution contexts have not landed; current dynamic algorithms are
  deliberately serial.
- Named contractions beyond `matmul` and contraction planning have not landed;
  einsum remains a documented subset.
- The expanded benchmark matrix and execution-backed specialized kernels remain
  future work.

The pre-cutover RFC called some APIs transitional and described later
deprecation. Fat-P governance required an atomic correction with no
compatibility aliases; Phase 0 reconciled that conflict before public API work.

## Constraints

1. **C++20 and header-only:** Domain headers may use the standard library and
   lower Fat-P layers, but no third-party numerical library.
2. **Flat public API:** User-facing headers remain under `include/fat_p/`;
   facade-owned implementation headers remain under `include/fat_p/tensor/`.
3. **Atomic cutovers:** A phase may use an implementation branch, but its merged
   state contains one API model, updated tests, updated docs, and updated CI.
4. **No compatibility shims:** Replaced names and signatures are removed rather
   than deprecated or aliased.
5. **Layout proof:** Every accepted nonempty mapping proves its minimum and
   maximum reachable offsets fit inside the backing storage span.
6. **Constness by element type:** Read-only mappings use `TensorView<const T>`;
   `const TensorView<T>` is not a substitute.
7. **Mutation requires injectivity:** Broadcast and overlapping mappings are
   read-only unless an operation defines exact write semantics.
8. **One traversal engine:** Elementwise, copy, fill, reduction, and contraction
   kernels do not implement independent offset walkers.
9. **Measured performance:** Performance claims require the repository benchmark
   protocol and recorded environment; algorithm selection remains evidence-led.
10. **Explicit execution:** Serial execution is the default. Parallel work uses
    an algorithm-owned context and defines determinism and completion behavior.
11. **Static Tensor stays bounded:** `StaticTensor` shares semantic vocabulary and
    conversions but does not acquire heap ownership, runtime-rank views, or the
    entire dynamic algorithm surface.

## Options Considered

### Option A: Add algorithms to the current Tensor type

Add axis reductions, matrix multiplication, and more einsum patterns before
changing ownership or traversal.

**Pros:** New surface area appears quickly; fewer immediate type changes.  
**Cons:** New algorithms would depend on same-type view ambiguity, duplicate
traversal, and unsettled numeric rules. Each addition would increase later
replacement work.

### Option B: Replace the complete Tensor subsystem in one rewrite

Design every type, kernel, algorithm, execution backend, and serializer change
before merging any part.

**Pros:** One theoretical endpoint; no intermediate architectural states.  
**Cons:** The review surface is too large for evidence-based verification;
layout, numeric, execution, and performance defects would be difficult to
isolate.

### Option C: Dependency-ordered atomic phases (Selected)

Separate the work by architectural dependency. Each phase changes all affected
repository files in one cutover and must satisfy its own conformance gate.

**Pros:** Reviewable invariants, bounded failure domains, measurable exit gates,
and no dual public API.  
**Cons:** Cross-phase contracts must be written before implementation, and some
desired algorithms deliberately wait for their prerequisites.

## Decision Rationale

Option C keeps each review grounded in executable invariants without retaining
obsolete APIs. The sequence follows the actual dependency graph:

```text
semantic contract
    -> extents and layout
    -> owner, views, and existing transforms
    -> serial iteration plan and kernels
    -> extended slicing and materialization
    -> numeric contract and reductions
    -> interop and expanded benchmark evidence
    -> composition and indexed selection
    -> named linear algebra
    -> execution context
    -> contractions and optional complete einsum
    -> serializer and StaticTensor closure
```

Algorithms cannot correctly precede the mappings they consume. Parallel
variants cannot precede deterministic serial kernels. Contraction planning
cannot precede numeric promotion, layout classification, or matrix kernels.

## Repository Shape at Completion

Public facades remain flat:

```text
include/fat_p/Tensor.h
include/fat_p/TensorLayout.h
include/fat_p/TensorSlice.h
include/fat_p/TensorView.h
include/fat_p/TensorAlgorithms.h
include/fat_p/TensorReductions.h
include/fat_p/TensorInterop.h
include/fat_p/TensorSelection.h
include/fat_p/TensorMatmul.h
include/fat_p/TensorExecution.h
include/fat_p/TensorEquality.h
include/fat_p/TensorSerializer.h
include/fat_p/TensorStatic.h
```

`TensorEinsum.h` is present only if Phase 10 implements the complete grammar.
`TensorIteration.h` and `TensorStridePolicy.h` remain separate PolicyIterator
facades and are not part of Tensor's kernel architecture. `TensorStorage.h` is
removed as a duplicate public storage implementation in Phase 2.

Facade-owned implementation is centralized:

```text
include/fat_p/tensor/Tensor.h
include/fat_p/tensor/TensorExtents.h
include/fat_p/tensor/TensorLayout.h
include/fat_p/tensor/TensorSlice.h
include/fat_p/tensor/TensorView.h
include/fat_p/tensor/TensorIterationPlan.h
include/fat_p/tensor/TensorKernels.h
include/fat_p/tensor/TensorNumeric.h
include/fat_p/tensor/TensorAlgorithms.h
include/fat_p/tensor/TensorReductions.h
include/fat_p/tensor/TensorInterop.h
include/fat_p/tensor/TensorSelection.h
include/fat_p/tensor/TensorMatmul.h
include/fat_p/tensor/TensorExecution.h
include/fat_p/tensor/TensorContraction.h
include/fat_p/tensor/TensorEquality.h
include/fat_p/tensor/TensorSerializer.h
include/fat_p/tensor/TensorStatic.h
```

These filenames are planned ownership boundaries, not permission to expose every
internal type publicly. Each new public facade requires an independent
header-self-containment test and a declared Domain layer.

## Current Surface Disposition

Phase 0 verifies this inventory against the tree. The target disposition is part
of the plan rather than an implied side effect.

| Current surface | Target disposition | Atomic phase |
|---|---|---|
| `Tensor<T, Allocator, IteratorPolicy>` | Replace with owner-only `Tensor<T, Allocator>` | 2 |
| `RowMajorTensor`, `ColumnMajorTensor`, `StridedTensor`, `BlockedTensor`, `OptimizedTensor` | Remove; traversal belongs to iterators and algorithms | 2 |
| `view`, `row`, `col`, `transpose`, `reshape` returning `Tensor` | Replace with camelCase borrowed/shared view operations | 2 |
| Materializing `broadcast_to` | Replace with read-only `broadcastView` followed by explicit `clone` when ownership is required | 2 |
| `TensorStorage` public component | Remove; retain one internal storage mechanism selected for owner/shared-view needs | 2 |
| `TensorIteration` and `TensorStridePolicy` | Remain PolicyIterator APIs; Tensor stops depending on them | 3 |
| `LazyAdd`, `LazySubtract`, `LazyMultiply`, `LazyScalarMultiply`, `lazy_*` | Remove the deep-copying second evaluation engine | 3 |
| `broadcast_add_vector`, `broadcast_add_scalar` | Remove; generic broadcasting and scalar kernels replace them | 3 |
| Public raw-pointer SIMD helpers and raw ISA probes | Remove; internal specializations use `SimdDetection.h` | 3 |
| Hidden `thread_local ThreadPool` operations | Remove; serial becomes the only implicit execution mode | 3 |
| Equality and approximate equality | Rebuild over owner/view readable concepts and shared kernels | 3 |
| `std::hash<Tensor<...>>` | Replace the owner-only signature in Phase 2; rebuild value traversal on shared kernels in Phase 3 | 2 and 3 |
| `create_tracker` and `create_tracked_*` | Replace with borrowed-view Debug tracking and explicit shared-view lifetime | 2 |
| `at_linear` and view `operator[]` | Rename the checked method `atLinear`; define both as logical row-major indexing | 2 |
| Subset `TensorEinsum` parser and kernels | Remove when named linear-algebra operations land | 8 |
| Duplicate runtime/serializer type-name helpers | Replace with one canonical dtype vocabulary | 11 |
| Owner constructors, allocator-extended constructors, copy/move assignment, `get_allocator`, member/ADL `swap` | Keep allocator-aware value semantics on the owner-only type | 2 |
| `operator()`, `at`, `operator[]`, iterators | Keep logical access; rename `at_linear` as listed and make view behavior explicit | 2 |
| `shape`, `strides`, `size`, `ndim`, `dim`, `empty`, `data` | Replace shape/stride returns with layout vocabulary; keep ordinary size/empty queries; restrict `data` interop by layout | 1, 2, and 6 |
| `fill`, arithmetic operators, scalar operators | Rebuild on shared serial kernels and numeric rules | 3 and 5 |
| `sum`, `mean`, `min`, `max` | Replace with the Phase 5 reduction family and explicit result/accumulator policy | 5 |
| `approx_equal`, `approx_not_equal`, `default_epsilon` | Replace with readable concepts, camelCase names, and explicit tolerance policy | 3 and 5 |
| `compute_broadcast_shape`, `is_broadcastable`, `is_broadcast_compatible`, `broadcast_view_to` | Replace with checked layout broadcasting and camelCase public vocabulary | 2 and 3 |
| `add_safe`, `sub_safe`, `mul_safe`, `view_safe`, `reshape_safe` | Replace with the Phase 0 error table and camelCase operations; retain no old wrapper | 2 through 4 |
| Tensor serializers and serialization error/result types | Keep the component; adapt owner/view input and unify dtype/resource contracts | 2 and 11 |
| `StaticTensor`, compile-time `Shape`, arithmetic policies, fixed aliases and algorithms | Keep the deliberately narrow fixed-size component; align shared semantics/conversions only | 11 |

### Mechanical Removal Manifest

The inventory above was verified against the public facades and
`include/fat_p/tensor/` on 2026-08-19. These expressions are the mechanical
grep gate for symbols that must disappear. A phase removes its entire row before
its exit gate can pass; the command intentionally scans headers, tests, docs,
workflows, CMake, and aggregate registration.

```powershell
rg -n --glob '!Artifacts/**' --glob '!include/fat_p/tensor/TensorStatic.h' --glob '!components/Tensor/tests/test_TensorStatic.cpp' `
  'IteratorPolicy|RowMajorPolicy|ColumnMajorPolicy|StridedPolicy|BlockedPolicy|RowMajorTensor|ColumnMajorTensor|StridedTensor|BlockedTensor|OptimizedTensor|\bview\(|\brow\(|\bcol\(|\btranspose\(|\breshape\(|broadcast_to|broadcast_view_to|create_tracker|create_tracked_|at_linear' `
  include/fat_p components cmake .github tools

rg -n --glob '!Artifacts/**' `
  'LazyAdd|LazySubtract|LazyMultiply|LazyScalarMultiply|lazy_add|lazy_sub|lazy_mul|broadcast_add_vector|broadcast_add_scalar|add_avx512|mul_avx512|simd_add|simd_sub|simd_mul|simd_scalar_mul|parallel_simd_|thread_local.*ThreadPool|wait_for_futures' `
  include/fat_p components cmake .github tools

rg -n --glob '!Artifacts/**' `
  'TensorStorage|TensorControlBlock|ReleaseAcquirePolicy|SeqCstPolicy|TensorEinsum|einsum\(' `
  include/fat_p components cmake .github tools
```

Phase 2 owns the first expression and the storage names in the third. Phase 3
owns the second. Phase 8 owns the einsum names in the third. Phase 11 owns the
duplicate dtype helpers identified by `type_name|get_tensor_type_name` until one
canonical dtype vocabulary replaces them.

## Phase Plan

| Phase | Status on 2026-08-19 |
|---:|---|
| 0 | Complete: governance, artifact relocation, Debug/sanitizer CI, and baseline harness |
| 1 | Complete: checked extents, signed layouts, classification, and oracle tests |
| 2 | Complete: owner-only Tensor, borrowed/shared views, explicit clone, and storage consolidation |
| 3 | Complete: counted multi-layout iteration plan and serial base kernels |
| 4 | Core implemented: signed slicing, ellipsis, axis insertion/removal, squeeze, unsqueeze, and permutation; materialization extensions remain |
| 5 | Core implemented: deterministic checked sum/product/mean/extrema/argument reductions; broader numeric policy remains |
| 6 | Interop implemented: contiguous span, validated strided descriptor, optional mdspan, and static/dynamic conversion; benchmark expansion remains |
| 7 | Core implemented: stack, concatenate, take, takeAlongAxis, and gatherND; broader generic math remains |
| 8 | Partial: vector, matrix, strided, and batched matmul implemented; remaining named operations and einsum removal remain |
| 9-11 | Planned, except bounded serialization and static/dynamic conversion work already delivered |

Every delivered free allocating algorithm follows the Phase 0 allocator table:
an explicit allocator wins; otherwise the first owning input from left to right
supplies a SOCCC-selected, result-type-rebound allocator. View-only calls use
the result type's default allocator.

### Phase 0: Governance, inventory, CI, and evidence harness

**Work**

- Repair `Design Note - Tensor Semantic Contract.md` to remove its duplicate
  `Scope`, add the missing options/rationale/consequences sections, and remove
  obsolete migration scheduling.
- Verify the current-surface table and publish a grep manifest for every symbol
  that must disappear in later phases.
- Move `TensorIteration` / `TensorStridePolicy` teaching docs and header
  self-containment tests from the Tensor component to PolicyIterator, updating
  metadata, registrations, CMake, and workflow ownership together.
- Select camelCase for all non-STL public functions. Existing snake_case Tensor
  functions are renamed in the phase that replaces their behavior; no alias is
  retained.
- Select borrowed `TensorView<T>` / `TensorView<const T>` plus explicitly named
  `SharedTensorView<T>` because the repository currently tests stored views that
  outlive an owner. Borrowed factories are lvalue-qualified.
- Keep compile-time `Shape` for `StaticTensor`; runtime code uses
  `DynamicExtents`, so no later rename is planned.
- Define one result-allocation table for free algorithms, owner member
  convenience functions, views, explicit allocators, and scratch storage.
- Define the error-channel table: direct contract violations, bounds failures,
  recoverable `Expected` results, allocation failure, and partial-mutation rules.
- State that in-memory layouts have no intrinsic rank cap. Serialization retains
  its configurable trust boundary; tests use named representative and generated
  rank ranges rather than a fictitious runtime maximum.
- Add non-`NDEBUG` Debug CI jobs and route future layout/kernel tests into the
  sanitizer jobs.
- Create the repeatable Tensor benchmark harness and record current contiguous,
  transpose, slice, and broadcast baselines. Negative stride has no "before"
  result and receives its first baseline after implementation.
- Establish deterministic randomized-layout helpers and scalar reference oracles.

**Exit gate**

- The semantic contract is a template-compliant Design Note with no occurrence
  of `transitional`, `deprecate`, `deprecation`, or compatibility scheduling.
- Every current public symbol has a keep, replace, or remove entry and an owning
  phase.
- PolicyIterator-owned TensorIteration/TensorStridePolicy docs and
  self-containment tests no longer live under `components/Tensor/`.
- Debug and Release CI jobs exist; sanitizer jobs compile the designated Tensor
  conformance translation units.
- The benchmark harness records commands, compiler flags, environment, raw data,
  and variance for every available baseline.
- Every public semantic promise has a named test or is explicitly target-only.

### Phase 1: Extents, strides, and layout core

**Work**

- Add runtime `DynamicExtents`, signed stride storage, normalized axis
  vocabulary, and `TensorLayout` without colliding with compile-time `Shape`.
- Represent storage length, logical origin offset, extents, and signed strides.
- Check shape products, stride products, origin arithmetic, and minimum/maximum
  reachable offsets.
- Classify layouts as empty, contiguous, injective strided, broadcast,
  overlapping, or explicitly indeterminate when bounded large-layout proofs
  cannot decide safely.
- Support rank zero, zero extents, positive strides, zero strides, and negative
  strides without forming pointers.

**Exit gate**

- Boundary and deterministic randomized tests cover ranks 0, 1, 2, 3, 8, and 32,
  plus generated ranks within the configured test budget; this is test coverage,
  not a public maximum.
- Tests cover zero extents in every axis, singleton axes, negative strides,
  overlap, and every checked-arithmetic boundary.
- A scalar integer oracle proves minimum/maximum reachable offsets against
  storage length; sanitizer pointer tests wait for Phase 2.
- Layout types have no element-allocation or execution dependency.

### Phase 2: Atomic owner, view, and existing-transform cutover

**Work**

- Make `Tensor<T, Allocator>` an owner of canonical contiguous storage only and
  remove `IteratorPolicy` plus all traversal-policy Tensor aliases.
- Add `TensorView<T>`, `TensorView<const T>`, and `SharedTensorView<T>` over
  validated layouts. Owner copy is deep; view copy rebinds metadata.
- Preserve random-access pointer iterators for canonical owners and provide a
  counted logical row-major iterator for views without an out-of-span sentinel.
  Copied equivalent views share an iterator equality domain.
- Replace the existing factory surface atomically with `asView`, `asConstView`,
  `sliceView` (current start/end capability), `rowView`, `columnView`,
  `transposeView`, `reshapeView`, and read-only `broadcastView`.
- Add `clone` as the single owner-from-readable materialization path. It uses the
  Phase 2 logical iterator initially, so non-contiguous and broadcast views have
  an explicit owning conversion before the Phase 3 kernel plan exists.
- Define allocator-extended owner construction, external-memory borrowing, and
  adoption as distinct operations.
- Remove the public `TensorStorage` component and use one internal storage/control
  block implementation for owners and explicitly shared views.
- Replace current Tensor concepts with readable-owner/view and writable-injective
  concepts; update equality, serializer, lifetime tracking, RCU integration,
  docs, workflows, and every call site in the same change.
- Replace the `std::hash` specialization signature so it targets the owner-only
  Tensor type; Phase 3 replaces its traversal body with the shared kernel.
- Define logical linear indexing for views and restrict contiguous-span APIs to
  layouts that prove compatibility.
- Apply the Phase 0 error-channel table to bounds, rank, shape, and transform
  failures.

**Exit gate**

- Compile-time tests prove const owners cannot create mutable views, rvalue
  owners cannot create borrowed views, and Tensor has no traversal-policy
  template parameter.
- Debug lifetime tests and sanitizer tests cover documented dangling borrowed
  views; Release does not claim runtime dangling detection.
- Runtime tests cover shared-view survival, owner destruction, view rebinding,
  allocator propagation, and the Phase 2 invalidation matrix.
- `clone` tests cover owners, non-contiguous views, and read-only broadcast views
  with independent result storage.
- The repository contains no same-type owner/view API, Tensor policy alias,
  public `TensorStorage`, materializing `broadcast_to`, compatibility alias, or
  old snake_case transform name.

### Phase 3: Unified serial iteration plan and base kernels

**Work**

- Add a multi-operand `TensorIterationPlan` that normalizes rank, coalesces axes,
  selects a contiguous inner loop, and carries signed offsets.
- Implement serial copy, fill, unary, binary broadcast, equality, approximate
  equality, and hash kernels on the shared plan.
- Guard the temporary same-type integral arithmetic surface against overflow;
  the complete promotion, division, and reduction policy remains Phase 5 work.
- Remove Tensor's duplicate offset walkers, deep-copying lazy-expression types,
  ad-hoc matrix/vector broadcast helpers, hidden thread-local pools, public
  raw-pointer SIMD functions, and raw `__AVX*` probes.
- Keep scalar kernels as the correctness reference. Internal SIMD specializations
  use centralized feature detection and require measurements.
- Stop using `TensorIteration` and `TensorStridePolicy` inside Tensor. Their
  positive-stride PolicyIterator contract remains a separate component and is
  not rewritten by this plan.

**Exit gate**

- Generic owner/view materialization, fill, unary, binary, equality, and hashing
  enter through the shared plan. Canonical owner copy construction uses direct
  contiguous storage; no alternate layout walker remains. A mechanical grep
  manifest rejects the retired walkers and helper symbols.
- Randomized differential tests pass for one-, two-, and three-layout plans,
  including two inputs plus a distinct broadcast output.
- Iterator/end and kernel tests under sanitizers cover positive, zero, and
  negative strides without out-of-span pointer formation.
- The benchmark executable measures public iterators and the shared plan
  side-by-side for contiguous, transpose, slice, and negative-stride layouts.
  Historical runs from different processors are labeled non-comparable rather
  than presented as before/after evidence.
- Tensor has no implicit parallel execution or raw ISA feature probe.

### Phase 4: Slice language and explicit materialization

**Work**

- Add `SliceSpec` with omitted endpoints, negative indices, negative steps,
  empty slices, integer-axis removal, `newaxis`, ellipsis, squeeze, and unsqueeze.
- Add `permuteView` and extend `sliceView` using metadata-only transforms.
- Reimplement Phase 2 `clone` on the shared kernel, then add `contiguousCopy`,
  `reshapeCopy`, and `copyFrom`. `copyFrom` proves disjointness or uses temporary
  materialization.
- Apply the Phase 0 result-allocator and error-channel tables to every allocating
  or recoverable operation.

**Exit gate**

- Differential tests compare every transform with the scalar coordinate oracle.
- Mutation of non-injective destinations reports through the documented channel
  before modification.
- View operations allocate no element storage; materializing operations have
  allocation-source and allocation-count tests.
- No transform or materialization operation implements a private offset walker.

### Phase 5: Numeric contract and axis reductions

**Work**

- Decide result and accumulator types for every supported arithmetic category.
- Define integer overflow, division, mean, boolean arithmetic, `abs(INT_MIN)`,
  NaN handling, empty-domain identities, and conversion failures.
- Normalize single and multiple axes, negative axes, duplicate axes, and the
  camelCase `keepDims` option.
- Implement `sum`, `product`, `min`, `max`, `mean`, `all`, and `any` on the
  iteration plan with an explicit deterministic serial reduction order.
- Keep dynamic Tensor numeric rules separate from existing `StaticTensor`
  checked/saturating policies unless a shared trait has identical semantics.

**Exit gate**

- Compile-time tests prove result and accumulator types; runtime tests prove
  overflow, NaN, empty-domain, and identity behavior.
- Reductions cover scalar, empty, zero-extent, strided, broadcast, negative
  stride, integral boundary, and axis-normalization cases.
- Serial results are reproducible under the documented order.

### Phase 6: Interop surface and expanded benchmark matrix

**Work**

- Expose `std::span` only for compatible contiguous storage and a C++20
  extents/strides descriptor for arbitrary validated layouts.
- Add optional C++23 `std::mdspan` conversion only through centralized feature
  detection; do not raise the C++20 baseline.
- Define borrowed versus owned lifetime for every interop operation.
- Expand the Phase 0 benchmark harness across allocation count, rank, shape,
  element type, layout class, and cold/warm execution.
- Keep BLAS, CUDA, MKL, and other external bridges in optional Integration-layer
  headers and out of Domain dependencies.

**Exit gate**

- Interop tests prove shape, stride, pointer, lifetime, and constness mapping.
- Benchmark commands, environments, raw outputs, and variance are recorded.
- No performance claim is promoted from an unmeasured code path.

### Phase 7: Composition and indexed-selection algorithms

**Work**

- Add generic unary and binary functions using the numeric contract and base
  kernels.
- Add `stack`, `concat`, `take`, `takeAlongAxis`, and `gatherND` as separate
  operations with distinct shape, bounds, duplicate-index, and allocator rules.

**Exit gate**

- Shape, dtype, bounds, and allocation tables are executable tests for every
  operation, including scalar and zero-extent cases.
- Randomized results match scalar reference implementations on every supported
  layout class.
- No operation adds an algorithm-specific traversal loop.

### Phase 8: Named linear algebra and removal of subset einsum

**Work**

- Implement dynamic `dot`, `outer`, `matmul`, diagonal extraction, and `trace`
  with explicit rank-one, rank-two, batched, zero-batch, and zero-contraction
  semantics.
- Select contiguous, transposed-compatible, packed, or general-strided kernels
  from layout evidence; avoid unconditional materialization.
- Remove the subset `TensorEinsum` facade, implementation, workflow, tests, docs,
  registrations, and duplicate kernels in the same change. A future general
  einsum is a new complete API, not a compatibility layer.

**Exit gate**

- Shape and dtype tables are executable tests for every supported rank case.
- Linear algebra is differentially tested against scalar references for every
  supported layout class.
- Benchmark evidence covers small and large shapes and layout variants before a
  specialized kernel becomes the default.
- No subset einsum symbol, parser, facade, or duplicate contraction loop remains.

### Phase 9: Explicit execution context

**Work**

- Add an optional algorithm argument controlling executor, grain size,
  determinism, scratch allocator, and backend selection.
- Keep serial execution as the default; Phase 3 has already removed hidden pools.
- Define task completion, exception draining, cancellation scope, nested
  parallelism, and deterministic reduction-combine order.
- Parallelize only kernels whose benchmark evidence exceeds task and
  synchronization overhead.

**Exit gate**

- ThreadSanitizer runs pass on a supported platform.
- Submitted work is drained before every synchronous return or throw.
- Serial and deterministic-parallel modes satisfy their documented result rules.
- Oversubscription and small-input thresholds have recorded measurements.

### Phase 10: Contractions and optional complete einsum

**Work**

- Add `tensorDot` and contraction planning on the shared layout, numeric,
  linear-algebra, and execution layers.
- Add `TensorEinsum` only if the phase implements a complete documented grammar:
  repeated labels, ellipsis, implicit/explicit output, multiple operands,
  broadcasting, dtype rules, and contraction planning. Otherwise named
  contractions remain the complete supported surface.

**Exit gate**

- Named contractions match scalar references across layout and dtype classes.
- If einsum exists, parser property tests reject malformed notation and compare
  valid notation with named operations or scalar references.
- Contraction order, temporary allocation, and numeric accumulation are explicit.
- No hard-coded pattern-list implementation is presented as general einsum.

### Phase 11: Serializer, dtype vocabulary, and StaticTensor closure

**Work**

- Serialize owners and views by canonical logical value without ownership or
  physical-layout leakage.
- Finalize extension framing, checksum policy, error codes, and trust-specific
  limits before declaring a stable wire version. Any byte-level format change
  increments the wire version.
- Replace duplicate runtime and serializer type-name helpers with one canonical
  dtype vocabulary that never depends on implementation-defined `typeid` text.
- Add explicit `StaticTensor` to/from dynamic owner conversions with checked
  extent and numeric conversion rules.
- Share only traits and kernels whose semantics are identical; checked and
  saturating policies remain StaticTensor-only unless separately justified.

**Exit gate**

- Golden wire tests cover scalar, empty, multidimensional, endian, malformed,
  checksum, version, resource-limit, and view-materialization cases.
- Static/dynamic conversion tests cover exact match, mismatch, overflow, and
  rank-zero behavior.
- Public docs state the final supported surface without transitional language.

## Cross-Phase Delivery Gate

Every phase must finish all applicable items before merge:

1. Header, facade, tests, docs, CMake registration, aggregate test registration,
   workflows, and metadata change together.
2. Debug and Release suites pass under MSVC, GCC, and Clang where supported.
3. Every new public header has a self-containment test and participates in the
   include-all hygiene target.
4. UndefinedBehaviorSanitizer covers checked layout arithmetic;
   AddressSanitizer covers pointer-forming view and kernel work; ThreadSanitizer
   covers execution work on a supported platform.
5. `fatp_meta_inventory.py`, `validate_layers.py`, formatting, and diff hygiene
   pass.
6. Benchmarks accompany performance-sensitive changes; results include commands,
   compiler flags, environment, raw data, and variance.
7. A fresh independent review searches for correctness, lifetime, overflow,
   exception, concurrency, and documentation-contract defects.
8. No replaced name, compatibility alias, duplicate implementation, or
   transitional call site remains.
9. Each phase's grep manifest confirms that its retired symbols and private
   walkers are absent.

## Review Boundaries

Each phase should be reviewed through three independent lenses before the lead
review reconciles findings by evidence:

- **Ownership and layout:** bounds, lifetime, constness, aliasing, allocator, and
  exception guarantees.
- **Iteration and numerics:** traversal, broadcasting, axes, promotion,
  reduction order, and algorithmic complexity.
- **Storage and integration:** serialization, resource limits, interop, build
  boundaries, metadata, and documentation claims.

Review findings require a source citation and concrete counterexample. Agreement
between reviewers does not replace evidence.

## Consequences

### Positive

- Later algorithms share one mapping and traversal model.
- Ownership, constness, and materialization are visible in type and function
  names.
- Numeric and execution behavior becomes testable before optimization.
- Each merged state has one supported API rather than parallel old and new paths.

### Negative

- Matrix multiplication, general contractions, and parallel kernels wait for
  layout and numeric prerequisites.
- Atomic phase cutovers require coordinated edits across headers, tests, docs,
  workflows, concepts, and aggregate registrations.
- Negative strides and arbitrary overlap substantially expand layout testing.
- The explicit shared-view type adds a second lifetime model that must remain
  visibly distinct from borrowing.

### Obligations

- Keep borrowed and explicitly shared views distinct in names, factories, tests,
  and lifetime documentation.
- Do not introduce new algorithm-specific traversal loops after Phase 3.
- Do not stabilize mixed-type or reduction APIs without the Phase 5 tables.
- Do not enable parallel defaults without Phase 0 benchmark infrastructure and
  Phase 9 execution measurements.
- Remove rather than deprecate every API replaced by a phase.
- Update this plan and `DN-TENSOR-001` when a decision changes.

## Status

**Status:** Accepted; Phases 0-3 implemented and independently reviewed design findings reconciled.  
**Decision owner:** Fat-P maintainer.  
**Implementation started:** Yes; current executable evidence is recorded in `DN-TENSOR-001` and component tests.

## Review Record

Independent read-only Claude and Grok reviews completed on 2026-08-19. Both
reviewers accepted the dependency-first and atomic-cutover direction. Their
evidence led to these material revisions:

- Existing transforms now move with the owner/view split.
- The serial iteration plan precedes materializing transforms.
- `IteratorPolicy`, `TensorStorage`, lazy expressions, ad-hoc broadcast helpers,
  raw SIMD entry points, and hidden pools have explicit removal phases.
- PolicyIterator remains a separate component.
- Debug CI and the benchmark harness move to Phase 0.
- Named linear algebra removes the subset einsum before an optional complete
  grammar can be added.
- CamelCase naming, rank policy, shared-view lifetime, and current-symbol
  inventory are explicit decisions rather than implied work.

The narrow second pass found four remaining cutover details, now incorporated:
PolicyIterator artifact relocation, three-layout broadcast tests, the two-phase
`std::hash` update, and a Phase 2 `clone` path for owner materialization from
non-contiguous or broadcast views.
