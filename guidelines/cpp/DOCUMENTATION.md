# C++ API and example documentation

Extends [Documentation](../modules/DOCUMENTATION.md) for C++ interfaces. Use the
documentation tool and naming/formatting conventions recorded in the project profile;
public C++ contracts use Doxygen comments. Ordinary // comments explain why;
Doxygen explains the caller contract, not implementation narration.

## Document the caller's contract

For a public type or operation, cover applicable obligations and observable
behavior. Put shared class-level rules in one place rather than duplicating them
on every overload.

| Contract area | Questions the documentation should answer |
|---|---|
| Meaning | What does the operation represent, and what are valid states and units? |
| Preconditions | Can pointers be null, ranges empty, inputs alias, or objects be moved from? |
| Ownership | Who owns a resource, and is ownership borrowed, transferred, or shared? |
| Lifetime | What must outlive a result, callback, iterator, view, or asynchronous operation? |
| Invalidation | Which operations invalidate pointers, references, iterators, or cached results? |
| Errors | Which failure channel and categories are used, and what state remains afterward? |
| Concurrency | Are independent objects safe, are concurrent reads safe, and what requires caller synchronization? |
| Cost | What allocations, copies, traversal, locking, or deferred work are part of the operation? |
| Compatibility | Which API, ABI, representation, platform, or feature requirements affect callers? |

Avoid content-free tags and comments that merely translate a declaration into
English. Explain the non-obvious contract, failure boundary, and reason behind
constraints. An undocumented restriction discovered from the implementation is
not an adequate public contract.

## Templates, attributes, and generic requirements

Describe required operations, semantic laws, and relevant complexity assumptions
for template parameters. A requires clause may express that comparison compiles
without expressing the ordering law callers must satisfy.

Explain conditional behavior where it changes use: allocator propagation, copying
versus moving, exception guarantees dependent on element operations, constexpr
availability, or platform-specific overloads. Keep this aligned with the actual
constraints and implementation.

Do not claim a factory leaks merely because a returned RAII object was ignored;
it may release immediately. Explain the actual reason a nodiscard result must be
observed. Similarly, noexcept, const, inline, and constexpr each have language
meanings and must not be used as substitutes for a performance or safety argument.

## Complexity, memory, and concurrency claims

Name the variable in an asymptotic bound and distinguish worst-case, average,
amortized, and expected behavior. State assumptions about hashing, comparison,
allocation, or element operations when they affect the bound.

Distinguish no payload copy from no allocation or no synchronization. Include
control blocks, retained owners, temporary storage, and deferred materialization
when explaining wrapper costs. Do not promise "zero overhead" based only on a
thin-looking class definition.

State exactly which operations may overlap and which objects or views share state.
A const method, atomic reference count, or race-free test result does not establish
that every operation on the abstraction is thread-safe.

## Examples that remain useful

- Use actual public headers, namespaces, types, and methods. Include required
  standard-library headers instead of relying on transitive includes.
- State the required language mode, dependencies, input data, and configuration.
  An example that needs a model file, server, device, or fixture must say where that
  input comes from; do not label it a standalone executable without those inputs.
- Keep a complete example compilable under the promised warning policy. Handle
  intentional discarded results, error paths, and object lifetimes correctly.
- Label partial excerpts and intentionally invalid examples so neither is
  mistaken for production code. Test negative examples for the intended diagnostic.
- Keep asynchronous and borrowed-view examples alive for the period they are used.
  A shortened example must not silently remove the owner or synchronization step
  on which its validity depends.
- Check important examples through the real build or consumer configuration when
  practical. Record syntax-only or unavailable-runtime limits honestly.

When using Doxygen, put parameter, return, exception, template, and note tags where
they add concrete contract information; validate generated output and relevant
documentation warnings. Do not duplicate a hand-written signature or file inventory
that becomes a second source of truth.
