# C++ testing

Extends [Testing](../modules/TESTING.md) for C++ semantics. Use the actual project
harness and discovery mechanism; this guide does not impose a particular framework.

## Select tests from the C++ contract

| Property | Useful distinguishing cases |
|---|---|
| Ownership and lifetime | Release exactly once, cancellation, failed construction, borrowed-view invalidation, callbacks after owner teardown |
| Value/move behavior | Copy independence, move-only support, supported moved-from operations, aliasing, and required self-assignment behavior |
| Generic operations | Non-default-constructible, non-copyable, non-trivial, over-aligned, and throwing-operation types where supported |
| Containers and allocators | Growth/rehash, erase invalidation, allocator propagation, allocation failure, and large-size arithmetic |
| Exceptions/error results | Expected failure category, preserved invariant, rollback/partial-state guarantee, and cleanup of acquired resources |
| Templates and constraints | Accepted and rejected types, real instantiations, const/ref categories, overload selection, and compile-time evaluation |
| Numerical code | NaN, infinities, signed zero where relevant, near-zero tolerance, overflow, underflow, and conversion boundaries |
| Concurrency | Publication, interleavings, cancellation, teardown, synchronization, and reclamation under the supported model |
| Headers and binary boundaries | Standalone includes, valid compositions, multiple translation units, and consumer configuration |

Use only cases the API promises to handle. A container requiring copyable values
need not accept a move-only type; the restriction must be intentional and tested
at its boundary rather than discovered accidentally by consumers.

## Container operation sequences

Container test suites must include reproducible randomized/stress sequences of
supported operations. Mix mutations and observations, such as insert, lookup,
erase, growth, and clear where the API provides them, rather than testing each
operation only against a fresh container. This exposes state interactions that
isolated examples can miss.

Compare results and state against an independent reference container with matching
semantics, or a simple contract model when no equivalent exists. Check the relevant
invariants throughout the sequence. Use a recorded seed and bounded workload;
failure diagnostics identify the seed and operation position so the sequence can
be replayed. Keep focused boundary and regression tests alongside these sequences.
A stress pass does not establish exhaustive state coverage or concurrency safety.

## Fixture and helper design

- Keep test helpers in test-owned scopes. Give shared helpers one definition home;
  avoid unrelated source files declaring conflicting externally linked State or
  Tracker types.
- Use scoped counters and fault injection for resource/exception behavior. Assert
  invariants and resource balance rather than incidental copy counts unless those
  counts are an explicit contract; copy elision and valid implementation changes
  can alter them.
- Do not bypass the public contract with private-layout hacks merely to simplify
  testing. Provide a deliberate test seam when an important failure cannot be
  reached otherwise.
- Preserve test discovery and process failure status. A standalone test entry
  point must return failure when assertions fail. Do not let NDEBUG turn off the
  only assertions in a configuration used as evidence.

## Assertion and exception hazards

Inspect the chosen harness's macro definitions. Commas inside template arguments
can be parsed as macro-argument separators unless the expression is protected in
a way the macro supports. A parenthesized expression or a named local result can
solve that problem without inventing a new harness convention.

Complete C++20 compile-only illustration using a deliberately single-argument
macro; it should compile. The macro is explanatory, not a proposed test framework.

```cpp
#include <type_traits>

#define GUIDELINE_REQUIRE(expression) static_assert(expression, #expression)

GUIDELINE_REQUIRE((std::is_same_v<int, int>));

#undef GUIDELINE_REQUIRE
```

Expected-exception checks must not catch their own assertion failure. When the
tested expression returns a nodiscard value, explicitly discard it inside the
evaluated expression if the test only needs the exception. Do not assume a
harness macro already handles that correctly on every compiler.

Numerical checks follow the base testing contract. In C++, verify that the chosen
comparison helper rejects unexpected NaN and that intermediate subtraction,
absolute-value operations, or tolerance scaling do not overflow or narrow in a
way that makes a wrong result pass. Floating-point build flags are part of the
configuration being tested.

## Compile-time and expected-failure tests

Use static_assert, traits, or constrained expressions for properties they can
actually establish. Instantiate the operation as well: a trait describing a
declaration may not instantiate its function body and expose a hidden requirement.

Compile-fail cases should isolate one intended contract violation. Compile a
corresponding valid control with the same includes, definitions, standard, and
relevant flags. Then match the intended diagnostic category or stable marker from
the invalid case. Keep compiler-specific matching bounded; do not demand identical
diagnostic prose from different compilers.

Use a compile-only check for a compile-time contract. Link failures are not proof
of a template constraint. If the contract concerns linkage, name it as a separate
link test with the appropriate positive control. Missing headers, a missing main,
an unsupported compiler option, or a tool crash must not count as success.

## Translation units, integration, and concurrency

For public headers, run standalone and applicable composition checks without
accidental prerequisites supplied by the harness. Use at least two source files
when testing a shared address, inline state, or other cross-unit property. Exercise
the installed/exported interface when distribution is part of the contract.

Use barriers, latches, controlled scheduling, or other suitable synchronization
to reach concurrency states reproducibly, subject to the supported language mode.
Bound waits and preserve failure diagnostics. Sleeps alone do not establish that
a race-sensitive state was reached.

Sanitizers and stress runs provide useful evidence but do not prove absence of
data races, lifetime errors, or undefined behavior. Include source reasoning for
ordering and ownership claims. Use [Build and CI](BUILD_AND_CI.md) to configure
instrumented runs and distinguish them from ordinary builds or stub integration.

## Harness isolation and component ownership

Use one approved test-only harness. Its identity and vendoring policy live in the
profile; this template does not import another project's harness or ban a framework
by name. Consumer include paths and exported targets must not expose the harness.
Component source/test files include the component header first, in a separate group.
Keep helpers under the project's `testing::<component>` namespace or file-local
scope; shared helpers have one owner. Individual test translation units must be
buildable with their declared dependencies, not only in a unity aggregate.

Teardown resources before checking final balances: release views/callbacks and
join workers, then assert counts. Test both stub and real backends when supported;
label substitute-only results. Interaction testing is chosen from actual risk:
logic tests do not prove GUI/device interactions, which need their own evidence.

Compile-time contract assertions belong before runtime cases in component tests.
They express promised traits/concepts, not incidental implementation properties.
A compile-fail case that unexpectedly starts compiling is a contract regression;
its diagnostic-matched negative and corresponding valid control must both be run.
