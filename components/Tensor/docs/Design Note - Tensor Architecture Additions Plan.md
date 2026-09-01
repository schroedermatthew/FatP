---
doc_id: DN-TENSOR-002
doc_type: "Design Note"
title: "Tensor Architecture Additions Plan"
fatp_components: ["Tensor", "TensorLayout", "TensorSlice", "TensorView", "TensorAlgorithms", "TensorReductions", "TensorInterop", "TensorSelection", "TensorMatmul", "TensorContractions", "TensorExecution", "TensorEquality", "TensorSerializer", "TensorStatic"]
topics: ["tensor layout", "tensor views", "strided iteration", "numeric promotion", "axis reductions", "tensor execution", "tensor contractions"]
constraints: ["signed stride reachability", "aliasing and lifetime", "checked shape arithmetic", "deterministic reduction order", "header-only C++20"]
cxx_standard: "C++20"
std_equivalent: "std::mdspan (partial layout and view overlap)"
std_since: "C++23"
boost_equivalent: "Boost.MultiArray (partial semantic overlap)"
build_modes: ["Debug", "Release"]
last_verified: "2026-08-31"
audience: ["C++ developers", "library maintainers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# Design Note - Tensor Architecture Additions Plan

**Status:** Phases 0-4 complete; Phase 5-8 dependency-light cores and bounded Phase 9-11 increments implemented and validated

**Decided:** The owner/view/layout/kernel foundation and dependency-light algorithm expansion are implemented  
**Last reviewed:** 2026-08-31

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
vector/matrix/batched multiplication, dot, outer, diagonal extraction, and trace.
The remaining work is architectural:

- Compound arithmetic, checked negation/absolute value, and floating sqrt/exp/log are implemented;
  remaining numeric operation families do not yet have implemented contracts.
- Existing calls remain serial by default. Phase 9 added opt-in native execution
  contexts for `matmul` and `dot`; Phase 10 extended that surface to `tensorDot`.
  Broader algorithm scheduling and alternate backends remain separate work.
- Named linear algebra and explicit-axis `tensorDot` have replaced the partial
  einsum API atomically. The current contraction plan is metadata-only; path
  optimization, packing, and a complete einsum grammar have not landed.
- The named linear-algebra benchmark matrix now covers five operations, sizes,
  and supported layout classes; additional specialized kernels still require
  their own direct measurement evidence.

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
include/fat_p/TensorContractions.h
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
include/fat_p/tensor/TensorContractions.h
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

| Phase | Status on 2026-08-31 |
|---:|---|
| 0 | Complete: governance, artifact relocation, Debug/sanitizer CI, and baseline harness |
| 1 | Complete: checked extents, signed layouts, classification, and oracle tests |
| 2 | Complete: owner-only Tensor, borrowed/shared views, explicit clone, and storage consolidation |
| 3 | Complete: counted multi-layout iteration plan and serial base kernels |
| 4 | Complete: slice language, overlap-safe materialization, bounded exhaustive/seeded transform oracles, and element-allocation/lifetime checks |
| 5 | Core implemented: checked axis/boolean reductions, mixed/scalar arithmetic including division, materializing casts, compound updates, checked negate/abs, and floating sqrt/exp/log; broader numeric expansion remains |
| 6 | Interop implemented; named linear-algebra allocation/layout benchmark matrix recorded; broader benchmark domains remain |
| 7 | Core implemented: stack, concatenate, take, takeAlongAxis, and gatherND; broader generic math remains |
| 8 | Named APIs, subset einsum retirement, and measured contiguous-vector dispatch implemented; broader specialization remains |
| 9 | Bounded native execution contexts implemented and remotely validated for `matmul` and `dot`; wider scheduling and backend work remains optional |
| 10 | Explicit-axis `tensorDot`, context overloads, tests, and measurements implemented and remotely validated; packing, path optimization, and complete einsum remain absent |
| 11 | Version 2 serialization limits and same-element-type static/dynamic conversion implemented; canonical dtype, framing, checksums, and stable wire compatibility remain open |

Every delivered free allocating algorithm follows the Phase 0 allocator table:
an explicit allocator wins; otherwise the first owning input from left to right
supplies an allocator rebound to the result type, then selected with SOCCC. View-only calls use
the result type's default allocator.

`copyFrom` returns no new owner: its overlap scratch instead uses an explicit
scratch allocator, the destination owner's exact allocator (without SOCCC), or
the default element allocator for a view destination. Source ownership does not
choose scratch.

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

**Delivered materialization step (2026-08-30)**

- `TensorAlgorithms.h` now exposes `reshapeCopy` and `copyFrom` through the
  existing public facade, with no new component files. `clone` remains the
  canonical-copy name; the originally proposed `contiguousCopy` synonym is
  deliberately omitted following peer review.
- Canonical materialization always owns independent storage. Copy-forcing
  reshape consumes logical row-major values and requires an equal element
  count, including scalar and zero-extent cases.
- Element transfer preserves destination storage, shape, allocator, and view
  validity. Same-type, exact-shape inputs are required. Reachable address
  intervals prove the direct path; all possible overlap uses a full snapshot.
- Allocation and staging failure leave destination values unchanged; final
  throwing assignments have a basic guarantee, not rollback. Current helpers
  require default-initializable, copy-assignable elements.
- Tests cover shifted storage bases, forward/backward overlap, transpose,
  negative strides, broadcast reads, interleaved spans, shared endpoints in both
  directions, self-copy, retained
  shared views, expired borrowed views in Debug, scalar/empty shapes, explicit
  and default allocator identity/counts, and injected allocation/assignment
  failures. A fixed-seed coordinate oracle differentially checks small signed
  and overlapping layouts for both new helpers and the existing `clone`.
- This closed the materialization API gap. The remaining transform-oracle gate
  is covered by the verification step below.
- Validation: 14 TensorAlgorithms tests pass with strict MSVC C++20 Debug,
  MSVC C++23 Release, GCC C++20, and Clang C++20 builds, plus MSVC
  AddressSanitizer. The CMake Debug build passes all 26 Tensor component tests
  and header checks, plus the two related PolicyIterator header checks
  (`test_TensorIteration_HeaderSelfContained` and
  `test_TensorStridePolicy_HeaderSelfContained`): 28 built and executed targets.
  Metadata inventory, layer validation, and `git diff --check`
  pass. These are local Windows results, not a claim of completed remote CI.

**Delivered transform verification step (2026-08-30)**

- `TensorTestSupport.h` supplies a test-only dense logical-coordinate to
  root-storage-offset table. Axis selections enumerate coordinate lists rather
  than reusing production slice-length, stride, or iteration-plan calculations.
  Twenty literal list-slicing anchors check endpoint normalization; further
  coordinate anchors check the oracle itself, including omitted versus explicit
  negative-one reverse stops, permutation order, scalars, and empty axes.
- `test_TensorSlice.cpp` checks all 62,208 combinations of lengths 0-5, omitted
  or explicit bounds -8 through 8, steps -4 through -1 and 1 through 4, and
  source strides 1, 2, -2, and 0. It also checks every permutation for ranks
  0-4 with extents 0-2, every insertion position and valid singleton-removal
  subset, and bounded rectangular/row/column/transpose/reshape/broadcast cases.
- A fixed-seed suite checks 500 six-operation transform chains on signed,
  padded, injective, broadcast, and overlapping read-only mappings. It generates
  public slice syntax and explicit coordinate selections together, instead of
  reimplementing the production ellipsis parser. Assertions prevent an
  accidentally all-empty or all-injective sample from passing vacuously.
- Checks cover exact root-address aliasing through iteration, linear access and
  variadic multidimensional `operator()`, values, write-through effects,
  constness, zero additional owner element allocations, and rejection of
  mutable noninjective layouts. Shared storage retention and Debug borrowed-view
  invalidation are checked for both nonempty and empty owners. Empty transforms
  also check exact shapes and preservation of a nonzero external-storage origin.
  This is bounded differential evidence for every transform family, not an
  exhaustive proof over arbitrary ranks, layouts, or signed-integer values.
- Two temporary mutant builds were rejected: interpreting an explicit reverse
  stop of -1 as an omitted stop, and dropping borrowed-lifetime tracking from
  extended slicing. Production headers were not changed by these checks.
- The signed `Slice` descriptor's existing `ptrdiff_t` extent limit is pinned
  and documented: `All` can preserve a huge empty axis that `Slice{}` rejects.
  No production slicing changes were needed within the supported contract.
- All 12 slicing test groups pass in strict MSVC C++20 Debug/C++23 Release,
  GCC C++20, Clang C++20, and MSVC AddressSanitizer runs. The CMake Debug
  regression run passes 26 Tensor targets plus the two related PolicyIterator
  header checks (28 total). The aggregate CI workflow now also watches the
  shared Tensor test-support header. Results are local Windows evidence;
  remote CI and other operating systems were not executed here.

**Work**

- Add `SliceSpec` with omitted endpoints, negative indices, negative steps,
  empty slices, integer-axis removal, `newaxis`, ellipsis, squeeze, and unsqueeze.
- Add `permuteView` and extend `sliceView` using metadata-only transforms.
- Retain Phase 2 `clone` on the shared kernel, then add `reshapeCopy` and
  `copyFrom`. `copyFrom` proves disjointness or uses temporary
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

**Delivered reduction-contract step (2026-08-30)**

- The current arithmetic-input reductions have an explicit result/accumulator,
  axis, order, initial, exceptional-value, empty-domain, allocator, and lifetime
  contract in `DN-TENSOR-001`. Existing sum/product and mean type rules remain
  unchanged; floating rounding and lack of compensated accumulation are explicit.
- `all` and `any` now complete the planned boolean reduction surface, using the
  existing initialized-reduction kernel and ordinary arithmetic-to-bool conversion.
  They return bool owners, with true/false empty identities, respectively.
- Fixed a reproduced mean defect: a valid external mapping with extents
  `{0, SIZE_MAX, 2}` reduced over `{1, 2}` now returns an empty `{0}` result
  instead of overflowing while counting unreachable domains. Oversized nonempty
  outputs remain rejected by the existing checked output-shape construction.
- An independent test oracle enumerates output coordinates and reduced-coordinate
  domains directly, with root offsets computed from the original layout. It
  uses no production axis normalization, index decoder, iteration plan, or
  checked arithmetic helper. Literal anchors check the oracle itself.
- The finite-value grid covers 4,008 shape/layout/axis/keep-dimensions cases:
  ranks 0-3, extents 0-3, contiguous, padded/reversed, overlapping, and broadcast
  layouts, every nonempty axis subset, and the rank-zero case. Another 600 seeded
  layouts cover ranks 0-5 and extents 0-4, with non-vacuity checks for nonempty,
  overlapping, and negative-stride inputs. All nine reductions are exercised.
- Compile-time checks pin 17 input-type rows, including character signedness.
  Dedicated runtime checks cover
  signed/unsigned intermediate overflow, narrow widening, floating accumulation
  order, mean conversion, long-double precision, NaNs, infinities, signed-zero
  ties, boolean truth, initial values, invalid axes, result allocation/failure
  cleanup, source preservation, and shared/borrowed lifetimes.
- Two compiled temporary mutations were rejected: dropping retained-axis
  coordinates from output indexing, and replacing the first NaN index with the
  last. Mutation headers were isolated from the repository and restored after
  the checks. An AddressSanitizer finding in new assertions was also corrected:
  scalar values are copied before a temporary result owner is destroyed.
- All 14 reduction groups pass strict MSVC C++20 Debug, MSVC C++latest Release,
  GCC C++20, Clang C++20, and assertions-enabled MSVC AddressSanitizer runs.
  The local CMake Debug regression passes 26 Tensor targets and the two related
  PolicyIterator header checks (28 total). Metadata inventory, layer validation,
  and whitespace checks pass. This is bounded local Windows evidence, not a
  claim of completed remote CI, arbitrary-layout proof, or cross-platform
  floating-point bitwise equivalence.
- This closes the reduction-contract increment, not all of Phase 5. Binary
  promotion is addressed in the subsequent increment below; division, remaining
  unary families, and conversion semantics are covered by later increments.


**Delivered mixed-type binary arithmetic step (2026-08-30)**

- `add`, `subtract`, `multiply`, and `+`, `-`, `*` now use one symmetric
  result lattice, exposed as `TensorArithmeticType<A, B>` and the element-type
  concept `TensorArithmeticCompatible<A, B>`. The full table is in
  `DN-TENSOR-001` contract 0.6.
- Integer promotion preserves both input ranges; unrepresentable signed/unsigned
  pairs are rejected. Same-type narrow results remain narrow and checked.
  Floating promotion never implicitly introduces long double, and 64-bit
  integer-to-double rounding is explicitly allowed.
- Both operands convert to the result type before the existing checked helper
  runs. The existing three-layout kernel needs no new walker or production
  header. Bool binary arithmetic is rejected; character values keep numeric
  range/signedness semantics.
- Both inputs and broadcast shape are validated before result element allocation.
  The first owner allocator is rebound to the result type before SOCCC.
  Explicit result allocators remain unchanged; view-only calls use
  `TensorAllocator<Result>`. Failure preserves inputs and reclaims partial
  result buffers.
- Tests pin a literal 11-by-11 type table, named/operator and explicit-allocator constraints,
  and symmetry/range invariants across 18 standard types. Runtime coverage
  includes all 65,536 signed/unsigned byte-value pairs, 600 seeded signed and
  overlapping/broadcast layout cases, scalar/empty output, numeric boundaries,
  rounding/NaN/signed zero, allocator selection, and failure cleanup.
- All 20 algorithm groups pass strict MSVC C++20 Debug, MSVC C++latest Release,
  GCC C++20, Clang C++20, and assertions-enabled MSVC AddressSanitizer runs.
  The local CMake Debug regression passes all 28 Tensor-related targets.
  Two isolated compiled header mutations were rejected by their intended tests:
  calculating in float before widening, and SOCCC before allocator rebinding.
  Repository headers were not changed by either mutation check. This is local
  Windows evidence, not a claim of completed remote CI or all-platform proof.
- No division, unary expansion, general cast, scalar-operand overload, mixed
  copying/equality, or parallel execution is introduced. Phase 5 still has
  remaining numeric families; this is a bounded binary-arithmetic increment.

**Delivered checked-cast and scalar-arithmetic step (2026-08-30)**

- `cast<To>` materializes a canonical, shape-preserving, independent result.
  Integer destinations reject nonfinite/fractional floating values and numeric
  overflow; bool accepts only zero/one. Floating destinations allow rounding
  and underflow to zero but reject finite overflow. The table and precedence
  are explicit in `DN-TENSOR-001` contract 0.7.
- Float-to-integer checks use exclusive power-of-two upper bounds before the
  language conversion, without assuming long double is wider than double.
  Signed and unsigned integer range checks remain separate. Type-changing
  floating NaNs/infinities preserve category when supported.
- Both-order scalar `add`, `subtract`, `multiply`, and `+`, `-`, `*`
  reuse `TensorArithmeticType`, the checked arithmetic helpers, and the unary
  kernel. Scalar values are captured by value, including source-element
  aliases. No scalar Tensor, extra element buffer, or private walker is added.
- Both APIs validate borrowed lifetime before result element allocation and
  use the existing result-rebind-before-SOCCC allocator contract. Explicit
  result allocators remain unchanged. Mid-iteration errors preserve sources
  and destroy unpublished results.
- Tests cover the 19-by-19 cast type matrix, zero/one across every type pair,
  a bounded exhaustive integer/bool domain, all fixed-width integer endpoints
  from float/double/long double, floating narrowing/nonfinite behavior,
  independent coordinates for 600 signed/overlapping/broadcast layouts,
  scalar promotion and both operand orders, allocator counts/identity,
  partial failures, and shared/borrowed lifetimes. The existing 11-by-11
  arithmetic matrix now also checks every scalar overload and operator.
- No division, in-place update, broad unary family, or new conversion-policy
  framework is introduced. `CheckedArithmetic::checked_cast` was inspected
  but not reused: its fractional, bool-target, and nonfinite contracts differ.
  This increment adds no production header or external dependency.
- All 28 algorithm groups pass MSVC C++20 Debug, MSVC C++latest Release,
  assertions-enabled MSVC AddressSanitizer, GCC C++20, and Clang C++20 with
  warnings treated as errors. The existing MSVC `/bigobj` test-build option
  supports the larger template matrix; the direct MinGW GCC command also uses
  its big-object assembler option. No repository build configuration changed.
  The 28-target CMake Tensor regression passes. Metadata, layer, whitespace,
  line-width, and named-test-reference checks pass.
- Two compiled isolated mutations were detected by the expected tests:
  accepting fractional floating-to-integer truncation, and using forward
  operand order for scalar-first subtraction. Mutation headers never replaced
  repository headers and were removed after the checks. This is bounded local
  Windows evidence, not remote CI or universal floating-platform coverage.

**Delivered division step (2026-08-30)**

- `divide` and `/` cover tensor/tensor broadcasting and both scalar operand
  orders, with optional explicit result allocators on the named operations.
  `TensorArithmeticType` and existing allocator selection are reused.
- Contract 0.8 specifies integral truncation toward zero, integer zero-divisor
  domain errors, and signed-result minimum/negative-one overflow errors before
  division. Floating results use native typed arithmetic and its environment;
  the IEC 559 special-value guarantees require nontrapping, semantics-preserving
  builds. Empty outputs evaluate no divisor; rank zero evaluates one.
- Binary and scalar division reuse `binaryKernel` and `scalarArithmetic` /
  `unaryKernel`. No new iteration walker, temporary scalar Tensor, production
  header, external dependency, or build configuration is introduced.
- `CheckedArithmeticInt::checked_div` was inspected. Its default enforcement
  path does not provide Tensor's distinct domain/overflow exception contract;
  importing and adapting its policy stack is unnecessary for these two guards.
  The Tensor-local check matches the existing checked add/subtract/multiply
  structure. `StaticTensor` behavior is unchanged.
- The existing 11-by-11 arithmetic type matrix also checks division overloads
  and result types. Eight division groups cover 15 standard integer types,
  exhaustive bounded byte quotients, promoted/scalar cases, float/double/long
  double special values, 1,200 seeded integer/mixed layout cases, constraints,
  allocator identity/counts, failed-result cleanup, and borrowed/shared lifetime.
- All 36 algorithm groups pass MSVC C++20 Debug, MSVC C++latest Release,
  assertions-enabled MSVC AddressSanitizer, GCC C++20, and Clang C++20 with
  warnings treated as errors. The rebuilt 28-target CMake Tensor regression
  passes. The 15 public Tensor facades also compile together with
  `CheckedArithmetic.h` in forward/reverse include order using the repository's
  existing MSVC `/wd4127 /wd4324` settings; no warning configuration changed.
- Three compiled isolated mutations are detected: accepting integer zero
  division, omitting byte-sized signed overflow, and reversing scalar division
  order. Mutation headers were never substituted into the repository and were
  removed after testing. Metadata, layering, whitespace, line-width, and all
  82 named-test documentation references pass their checks.
- Local Claude and Grok final reviews report no blockers. Review feedback adds
  promoted floating results with integer-source zero divisors, signed minimum
  over zero, mixed integer-result overflow, ordinary finite floating tests
  outside the IEC 559 gate, and precise floating-environment wording.
  This is bounded local Windows/compiler evidence, not remote CI or universal
  floating-platform coverage. The compound and unary increments delivered later
  are recorded below; broader Phase 5 numeric expansion remains open.

**Delivered compound-assignment step (2026-08-31)**

- `addAssign`, `subtractAssign`, `multiplyAssign`, `divideAssign` and the
  four compound operators accept non-const lvalue owners and writable views.
  They return the original destination reference. RHS tensors broadcast only
  into the fixed destination extents; scalar operands are captured before
  scratch element allocation.
- Contract 0.9 defines computation in the existing promoted type followed by
  checked destination conversion. Integer division still truncates when the
  compute type is integral; fractional floating results cannot be silently
  written into integer destinations.
- Nonempty calls compute into one destination-typed scratch buffer, then
  commit through the shared copy kernel. All C++ exceptions preserve the
  destination's values, mapping, storage, allocator, and view validity.
  Overlapping RHS views observe pre-update values. Empty calls validate but
  evaluate and allocate no elements; metadata may allocate separately.
- Explicit scratch allocators are used unchanged. Default scratch uses the
  destination owner's exact allocator without SOCCC/rebinding, or the default
  Tensor allocator for view destinations. No source allocator is selected.
- No new production header, dependency, private traversal loop, or build
  configuration is introduced. Nine new groups cover overload constraints,
  destination stability, checked rollback, alias snapshots, allocator
  identity/counts, failures/lifetimes, and 15 standard integer types.
  The existing 11-by-11 type matrix also checks scalar/tensor compound
  overloads and return types. The independent oracle checks all four updates
  across 1,200 seeded signed/padded/broadcast integer and mixed layouts with
  tensor and scalar RHS forms: 9,600 operation checks.
- All 45 standalone algorithm groups pass MSVC C++20 assertions-enabled,
  MSVC C++latest Release, MSVC ASan, GCC C++20, and Clang C++20 with warnings
  treated as errors. A second ASan run uses native new/delete and passes the
  other 44 groups, retaining allocation/deallocation-mismatch diagnostics.
  The rebuilt 28-target CMake Tensor regression passes with MSVC checked
  iterators enabled. All 15 public Tensor facades plus CheckedArithmetic
  compile in forward/reverse include order with the existing warning settings.
- The registered standalone allocation-failure sweep exhausts each observed
  ordinary allocation site across four operations, two RHS forms, and four
  shape/layout cases. A separate instrumented MSVC probe observed 488 failures
  with unchanged storage and no tracked allocation leaks. Non-allocating
  failure safety separately relies on primitive nonthrowing assignment and
  validated reachable offset arithmetic.
- MSVC checked-iterator builds explicitly skip only the global-new sweep:
  their std::vector move allocates a debug proxy inside a noexcept function,
  so failing that allocation terminates in the runtime. Checked iterators
  stay enabled for the other 44 groups. Standalone test entry points route
  CRT assertions/errors to stderr and suppress abort/debugger dialogs.
  The aggregate object has no replacement-new definitions, avoiding a clash
  with the IdGenerator test's global allocation instrument.
- Three targeted isolated mutations detect unchecked write-back, eager
  destination writes, and incorrect scratch SOCCC. A fourth injects allocation
  after an iteration callback: the actual standalone suite passes 44 groups
  and fails only the allocation-transaction guard. Mutations never replace
  repository headers. Metadata, layering, whitespace, line-width, test
  registration, and named-test documentation checks pass.
- Local Claude and Grok reviews report no blockers. Review feedback adds
  the persistent commit-allocation guard, non-vacuous late-zero rollback,
  default-view allocator isolation, rank-zero RHS broadcasting, and explicit
  exception/initialization-cost documentation. This is bounded local evidence,
  not remote CI or a proof for arbitrary layouts or floating platforms.
  The remaining broad unary work and all of Phase 5 are not closed.

**Delivered negation/absolute-value step (2026-08-31)**

- `negate(source[, allocator])`, unary `-source`, and `abs(source[, allocator])`
  return fresh canonical owners with unchanged dtype and extents. Signed minima
  throw; unsigned negation accepts only zero, while unsigned absolute value
  copies. Widening requires an explicit checked cast. Boolean and non-arithmetic
  elements do not participate. Floating negation uses native unary minus and
  absolute value uses typed `std::fabs`, preserving their signed-zero behavior.
- The existing unary kernel and result allocator selection are reused. There is
  no new traversal loop, element scratch buffer, dependency, or production
  header. Lifetimes are validated before result element allocation; failures
  release unpublished output and never modify the source.
- These constrained overloads extend the existing `fat_p` Tensor numeric core
  across owners, borrowed views, and shared views. They are not generic scalar
  functions or namespace re-exports. The existing Include-All TU includes
  `TensorAlgorithms.h`; all-header and reversed Tensor-facade checks exercise
  the new names alongside scalar math and CheckedArithmetic.
- Seven test groups cover all 18 standard arithmetic dtypes other than bool,
  all 15 standard integer/character endpoint categories, exhaustive 8-bit values,
  native floating special values, 600 deterministic rank-0-through-4
  signed/broadcast/overlapping layouts, allocator selection, independent output, expired borrowed sources,
  retained shared storage, empty results, and late-overflow cleanup.
- Local validation passes 52 algorithm groups under MSVC C++20 assertions,
  MSVC latest-standard Release, MSVC AddressSanitizer, GCC C++20, and Clang
  C++20. The native-allocation ASan leg passes 51 groups, with the existing
  global-allocation injection group disabled. All 28 CMake Tensor targets pass
  in MSVC checked-iterator Debug; its existing injection-group skip remains.
  A GCC GNU++20 compile probe also rejects signed/unsigned 128-bit unary
  operands while confirming that this mode recognizes both as integral types.
- Include-all and forward/reverse Tensor-facade builds exercise the new APIs
  alongside `autodiff::Jet` absolute value, scalar `abs`, `std::negate`, binary
  subtraction, and `approxEqual`. Three isolated mutation probes detect a
  missing signed-minimum check, zero-subtraction negation, and conditional
  floating absolute value that retains negative zero. Repository headers are
  never replaced with mutated copies. These are local checks, not remote CI
  results or proof for every platform and layout.
- This closes negation and absolute value only. Transcendental functions and
  other broader numeric families remain separate increments, not implicit
  promises of Phase 5 completion.

**Delivered floating-unary math step (2026-08-31)**

- `sqrt(source[, allocator])`, `exp(source[, allocator])`, and
  `log(source[, allocator])` preserve floating dtype and shape. The public
  overloads accept only float/double/long double readable owners and views;
  integer callers explicitly cast before evaluation.
- The existing unary kernel and allocator selection materialize one result
  element buffer without an intermediate tensor, new traversal loop, or new
  dependency. Typed standard-library calls retain native domain, pole, range,
  signed-zero, NaN, and infinity behavior rather than throwing Tensor numeric
  exceptions. Lifetime and allocation failures retain the usual source guarantee.
- The three constrained names extend the existing Tensor numeric core across
  all owner/view forms. Header-composition checks cover the existing Include-All
  TU and both Tensor/Jet/CheckedArithmetic orders, exercising scalar math and
  `autodiff::Jet` through their existing namespaces and ADL.
- Six new test groups cover accepted/rejected types, typed native values and
  range boundaries, 900 seeded layouts spanning ranks 0-4 across three dtypes,
  independent result storage, all allocator paths, empty/expired/retained
  inputs, allocation failures, and preservation of pre-existing floating flags
  when the environment supports that probe. Test-only environment changes are
  scoped and restored; production algorithms never alter trap settings.
- Local verification passes 58 algorithm groups on MSVC C++20 assertions,
  latest-standard Release, MSVC AddressSanitizer, GCC C++20, and Clang C++20.
  The native-allocation ASan configuration passes 57 groups with only the
  existing global-new injection group disabled. All 28 MSVC checked-iterator
  Debug Tensor CMake targets pass, retaining the existing injection-test skip.
  Include-all and forward/reverse header compositions pass. Metadata, layer,
  whitespace, line-width, registration, and named-test evidence checks pass.
- Isolated GCC mutation probes detect negative-domain clamping, lost square-root
  signed zero, a double round-trip for long-double logarithms, and explicit
  clearing of a pre-existing floating flag. The flag probe uses volatile input
  under optimization; it does not test exact libm flag sets or rounding modes.
  These checks leave repository headers unmutated and are local evidence, not
  remote CI results or cross-platform accuracy claims.
- This increment does not add general transcendental families, configurable
  floating-error policies, or parallel execution, and does not close Phase 5.

**Work**

- Decide result and accumulator types for every supported arithmetic category.
- Define integer overflow, division, mean, boolean arithmetic, `abs(INT_MIN)`,
  NaN handling, empty-domain identities, and conversion failures.
- Normalize single and multiple axes, negative axes, duplicate axes, and the
  camelCase `keepDimensions` option.
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

**Implemented named-API cutover (2026-08-31)**

- `TensorMatmul.h` now owns dynamic `dot`, `outer`, `matmul`, `diagonal`, and
  `trace`. Their rank, empty-domain, dtype, allocation, and lifetime rules are
  recorded in semantic contract 0.12 and the user manual. No new facade or
  dependency was introduced.
- Dot delegates to matmul. Outer maps vectors to const column/row operands and
  uses the shared binary kernel, preserving direct floating multiplication
  without an additive seed. Diagonal and trace use a validated internal const
  mapping with the existing copy/reduction kernels. No intermediate element
  buffer or duplicate contraction traversal was introduced.
- The mapped operations select allocators from original operands and retain
  lifetime tracking. Empty/singleton diagonal mappings avoid forming unused
  overflowing stride sums. Integral products widen before checked arithmetic.
  Generic matmul also returns its initialized zero result before addressing
  unreachable row/column strides when the contraction dimension is zero.
- These constrained operations extend the existing Tensor numeric vocabulary
  across owners and views. Existing StaticTensor dot/outer remain distinct
  overloads. The Include-All header retains the TensorMatmul facade, and
  forward/reverse facade-order probes exercise these names alongside
  StaticTensor, CheckedArithmetic, and Jet without namespace re-exports.
- `test_TensorMatmul.cpp` has 15 groups, including compile-time dtype and
  allocator rejection tables, all matmul rank forms, 625 outer and 125 dot
  signed-stride cases, 768 batched diagonal/trace cases, and 36 matrix-layout
  pairings with independently computed scalar offsets. Tests also cover
  unused extreme strides, shared/expired lifetimes, signed zero, late numeric
  and copy failures, allocator identity, and result-buffer cleanup.
- Both subset einsum headers, both tests, the dedicated workflow, its generator
  entry, and aggregate/metadata registrations are removed together. Named
  composition tests retain transpose, reductions, elementwise multiplication,
  and Frobenius examples. The user manual explicitly documents migration
  differences; no compatibility aliases or parser remain.
- Expanded small/large/layout benchmarks and further specialized dispatch are
  addressed by the following increment. This initial API cutover itself added
  no new specialized default kernel or throughput claim.

**Implemented serial measurement increment (2026-08-31)**

- A separate `benchmark_TensorMatmul.cpp` covers dot, outer, matmul, diagonal,
  and trace across float/double, three size tiers, and applicable contiguous,
  padded, reversed, transposed, and batched layouts: 126 problems. Inputs have
  nonzero origins and deterministic nonconstant values. Every output element
  and shape is checked against independent scalar loops outside timing.
- Public allocating calls, prevalidated scalar loops with matching Tensor
  result storage, and an allocation-only control are measured separately.
  The result buffer escapes through a volatile indirect observer. A separate
  allocator probe records result-buffer counts/bytes and checks reclamation;
  metadata and global allocations are explicitly excluded.
- Full MSVC and GCC Release runs record three warmups and fifteen randomized
  measured rounds, calibrated batches, raw durations, CPU context, variation,
  and CSV/JSON exports. Quick mode remains a smoke test. The manual component
  workflow, unified compiler sweeps, workflow generator, local quick runner,
  and CMake benchmark target include the new suite without external dependencies.
- Direct, fresh-seed, randomized process-pair measurements justify routing
  contiguous vector pairs through the existing checked contiguous kernel.
  No new contraction loop, API, allocator, or thread pool was added. Validation,
  widening, zero seed, and serial accumulation remain shared; mixed-rank,
  batched, and noncontiguous operands retain the generic path.
- The linear-algebra suite now has sixteen groups. New cases compare contiguous
  and strided floating results around block boundaries, including cancellation,
  NaN/infinity/signed zero, zero and singleton lengths, extreme unused strides,
  late checked overflow, allocator cleanup, and unchanged inputs. MSVC Debug,
  Release, AddressSanitizer, GCC, and Clang pass; all 26 Tensor CMake tests pass.
- Local Claude/Grok reviews found no blocking production defect. Claude's test
  feedback added nonuniform right-hand values, NaN padding, and all four
  contiguous/strided pairings. The new group rejects isolated right-index and
  right-contiguity-predicate mutations; both compiler matrices remain green.
- A results-review follow-up separates exact-input benchmark correctness from
  rounding-order tests, records actual API batch quality, and adds paired
  small-vector checks. No small-case regression was observed on MSVC or GCC.
- Commands, raw measurements, statistical limits, and exact comparison results
  live in [the benchmark report](../results/2026-08-31-linear-algebra/README.md),
  not performance tables in public API documentation. This does not close
  broader integer/interop benchmarks, additional kernel specialization,
  general contraction planning, or execution-context coverage beyond the
  bounded operations delivered below.

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

**Implemented bounded increment (2026-08-31; Phase 9 not closed)**

- Added opt-in TensorExecution.h with default-serial TensorExecutionContext and
  caller-owned native ThreadPool scheduling for matmul. Context-aware dot accepts
  cancellation but retains its single serial fold. Other algorithms remain serial.
- Shared serial/parallel row writers preserve fold order, signed-stride layouts,
  batch broadcasting, checked arithmetic, result ownership, and allocator selection.
- Context options cover grain, task cap, cooperative stop token, and PMR scheduler
  scratch. Parallel submission is bounded; all accepted futures are drained before
  submission/task/cancellation errors are propagated. Any Fat-P pool worker takes
  the serial path. Same-build/same-floating-environment determinism is documented.
- Hardened the existing ThreadPool prerequisite: shutdown admission cutoff,
  enqueue-allocation rollback, partial-batch accounting, concurrent shutdown joins,
  and explicit any-pool worker identity. Regression tests first reproduced both
  stranded post-shutdown futures and an underflowing batch pending counter.
- Checked MSVC Debug exposed allocator-backed vector proxy allocation in a noexcept
  constructor; scheduler scratch now uses a directly allocated future array so
  allocation failure remains catchable on that configuration too.
- The 1,048,576-product default cutoff and 32-row grain are conservative choices
  informed by 1/2/4-worker, small-input, grain, nested, and concurrent-caller measurements.
  See [results and review record](../results/2026-08-31-execution-contexts/README.md).

**Gate update (2026-08-31):** Linux ThreadSanitizer, AddressSanitizer, and
UndefinedBehaviorSanitizer first passed for execution and its ThreadPool prerequisite
on commit 42fca1a, in [TensorExecution CI](https://github.com/schroedermatthew/FatP/actions/runs/33415615587)
and [ThreadPool CI](https://github.com/schroedermatthew/FatP/actions/runs/33415615551).
Those runs also exposed a copied structured binding in TensorSlice and a deprecated
volatile compound assignment in the ThreadPool test under ordinary warning-as-error
builds. After both corrections, [aggregate FatP CI](https://github.com/schroedermatthew/FatP/actions/runs/33474185706)
passed on commit 72d3495b across GCC 12/13/14, Clang 16/17, MSVC C++20/C++23,
strict warnings, self-containment, AddressSanitizer, UndefinedBehaviorSanitizer,
and ThreadSanitizer. This closes remote validation for the delivered context
surface. Broader algorithm scheduling, column tiling for single-row products,
foreign-pool coordination, and other backends remain outside this increment;
they are not implied by the context API.

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

**Implemented bounded increment (2026-08-31; remote gate complete)**

- TensorContractions.h provides tensorDot with explicit paired axis lists,
  negative-axis normalization, fixed free-axis output ordering, and no broadcasting.
- A metadata-only contraction plan shares validated layouts and checked numeric
  primitives. Its range writer handles arbitrary output partitions; unlike
  TensorIterationPlan's full traversal, it needs neither per-worker coordinate
  allocation nor sub-layouts over empty storage.
- The last supplied contracted axis forms a strided run. Its offset advances
  only to reachable elements and never advances after the last term. Earlier
  axes are decoded once per run, preserving exactly the specified fold order.
- Serial remains the default. TensorExecution.h alone adds context overloads;
  scheduling partitions output elements, never the contracted fold.
- Seven test groups include 720 seeded recursive scalar differentials over bool,
  signed/unsigned integer, float, double, and long double strided mappings.
- Dedicated benchmarks compare serial/default-context/forced-context calls with
  matching prevalidated scalar folds and result storage. No einsum grammar,
  path optimizer, operand packing, or external backend is exposed.
- [Validation, peer review, and measurements](../results/2026-08-31-contractions/README.md)
  record the bounded evidence. The dedicated
  [TensorContractions CI run](https://github.com/schroedermatthew/FatP/actions/runs/33419554296)
  passed on commit ee08ead1, and the later
  [aggregate FatP CI run](https://github.com/schroedermatthew/FatP/actions/runs/33474185706)
  passed on commit 72d3495b with the compiler and sanitizer matrix described above.

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

**Implemented bounded increment (2026-08-31; Phase 11 not closed)**

- Wire format version 2 serializes owners and views in logical order, distinguishes
  rank-zero scalars from empty tensors, and supports the documented big-endian
  scalar representations. The decoder accepts only version 2 and rejects every
  other wire-version value; version 1 is incompatible because it encoded dynamic
  rank zero as empty.
- Deserialization enforces caller-configurable rank, extent, element-count, and
  payload-byte limits before allocating Tensor element storage. An overload accepts
  the exact result allocator.
- `toTensor` and `toStaticTensor` provide same-element-type static/dynamic conversion.
  Tests cover exact shapes, mismatched shapes, rank-zero values, and view materialization.
- Extension framing, checksums, a canonical dtype vocabulary, cross-version
  compatibility policy, and any broader numeric conversion surface remain open.
  The current experimental wire format is not a stability promise.

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

**Status:** Accepted; Phases 0-4, the Phase 5-8 dependency-light cores, and bounded Phase 9-11 increments are implemented, with the remaining expansion gates stated above.

**Decision owner:** Fat-P maintainer.  
**Implementation started:** Yes; current executable evidence is recorded in `DN-TENSOR-001` and component tests.

## Review Record

Local read-only Claude and Grok documentation-closure reviews completed on
2026-08-31 with no remaining blockers. Claude identified stale opening context,
missing facade/component inventory, incomplete execution and cancellation terms,
and missing consumer-facing API/wire stability language. Grok independently
separated Phase 9 `matmul`/`dot` ownership from the Phase 10 `tensorDot` context
extension and required the decoder's version-2-only behavior to be explicit.
Those findings are incorporated above. Neither peer edited files or substituted
its opinion for the executable and remote-CI evidence.

Local read-only Claude and Grok design reviews approved floating sqrt/exp/log
on 2026-08-31. Claude's final source review supported shipping; its remaining
documentation/test findings added attached exception tags, volatile input for
the flag probe, and a comment explaining the deliberately shared owner in the
borrowed-expiry/shared-retention test. The owner confirmed `Tensor` destroys its
borrowed lifetime token independently of shared storage retention. An optimized
flag-clearing mutation verified the scope of the environment check without
claiming exact libm exception sets or rounding-mode coverage.

Grok's initial final-source review was cancelled after more than six minutes
without a verdict. A subsequent bounded review of the implementation excerpt,
shared helper, contract, and owner-reported evidence returned no blockers.
Neither peer executed tests; all compiler, sanitizer, and mutation evidence
above was produced by the owner. No browser connection was used.

Local read-only Claude and Grok unary-arithmetic reviews completed on
2026-08-31 with no implementation blockers. Review feedback produced
operation-specific overflow diagnostics, internal unsupported-type assertions,
conditional extended-integer rejection probes, rank-0-through-4 layout oracles,
and include-composition checks with Jet/scalar math. Jet's `abs` is actually in
`fat_p::autodiff`, not the root namespace; the owner verified that source detail
and tested ADL without changing or re-exporting the existing Jet API. An initial
Grok job failed without a verdict; its bounded local retry completed. All
execution evidence above is from the owner's tools, not reviewer execution.

Local read-only Claude and Grok compound-arithmetic reviews completed across
2026-08-30/31 with no implementation blockers. The owner verified the existing
binary kernel's input broadcasting and the concrete owner/view allocator
distinction, then addressed the reviewers' test and documentation findings.
The allocation sweep is a registered standalone test, with explicit MSVC
checked-iterator and aggregate-runner scope documented in contract 0.9.
Native-new ASan complements the fault-injection ASan run. Scratch's initial
element initialization and lifetime metadata remain disclosed reuse costs,
not performance optimizations introduced by this increment.

Local read-only Claude and Grok final reviews of division completed on
2026-08-30 with no remaining blockers. Initial in-progress snapshot findings
were rechecked against the completed test registrations, allocator/failure
cases, and contract 0.8. Result-type-dependent zero-divisor behavior is now
explicit in the header and manual and exercised in all three standard
floating types. Follow-up tests cover signed-minimum/zero precedence and
mixed signed overflow. Native floating arithmetic does not reconfigure the
caller's environment; it may set status flags and does not intercept traps.
The final compiler, sanitizer, regression, composability, and mutation evidence
is recorded in the division step above.

Local read-only Claude and Grok design and implementation reviews of checked
casts and scalar arithmetic completed on 2026-08-30 with no remaining blockers.
Review feedback added negative/minimum/maximum checks for seven character
types, explicit allocator casts from borrowed and shared views, and scalar
long-double maximum checks in both operand orders. Documentation now spells
out scalar literal promotion, strict bool conversion, fractional error order,
and type-changing NaN behavior even when floating representations match.
Character signedness was resolved using the actual standard concept relation
and compiled endpoint tests; scalar overload presence and promotion were
confirmed by the full existing type matrix and runtime coordinate oracle.
Compiler, sanitizer, and mutation evidence is recorded above.

Local read-only Claude and Grok design and implementation reviews of mixed-type
binary arithmetic completed on 2026-08-30 with no remaining blockers in the
reviewed scope. Review feedback added explicit-allocator call-rejection probes
and clearer character-platform and allocator-rebinding documentation. Existing
matrix checks already pinned all three operator result types and rejected each
unsupported call independently; the new explicit forms passed every compiler
configuration listed above. Standard allocator cross-type construction remains
a requirement, not a new permissive allocator model. Extended integers beyond
64 value bits remain deliberately unsupported, including same-type pairs.
The chosen rebind-before-SOCCC order is enforced by a type-sensitive allocator
and a mutation check. Reviews do not establish correctness outside the stated
type/layout domains or constitute remote CI evidence.

Local read-only Claude and Grok reviews of the Phase 5 reduction-contract step
completed on 2026-08-30. The mean empty-output defect was reproduced before its
fix. Review findings added zero-middle/zero-last oversized-domain cases, both
output-size rejection branches, character-type rows, interior extrema seeds,
mixed zero/NaN boolean domains, and allocator identity checks on empty results.
The coordinate oracle stayed independent of production traversal and numeric
helpers. A disagreement about the reversed floating-point fold was resolved by
a standalone C++ calculation: for `L = 2^24`, `-L + 1` is exactly representable,
so its subsequent addition of `L` gives one. No expectation was weakened to
match a review claim. Final local compiler/sanitizer evidence is recorded above.

Local read-only Claude and Grok reviews of the Phase 4 transform verification
completed on 2026-08-30. Grok found no closure blocker in the bounded oracle,
composition grammar, aliasing, or lifetime coverage. Claude's findings led to
explicit multidimensional-access checks, mutable-layout rejection, literal
normalization anchors, empty-owner lifetime/allocation tests, exact empty shapes,
nonzero empty-storage-origin preservation, and documentation of the huge-empty
signed-slice limit. These additions passed the local validation matrix above;
the reviews do not establish correctness outside the documented test domains.

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
