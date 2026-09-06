# C++ authoring

Applies to C++ design, implementation, refactoring, and review. Read the core,
[Engineering](../modules/ENGINEERING.md), and [project profile](../PROJECT_PROFILE.md).

## Language and runtime contracts

C++20 is the minimum. No C++17 fallback or feature-emulation layer. Prefer the
standard language/library facility; retain genuine supported platform boundaries.
Structural invariants must be enforced in every build. Throwing a documented typed
exception is the default; exception-disabled boundaries require an owner-ratified
alternative that prevents invalid construction/state. Access-time checks may have
an explicit checked/unchecked contract; do not use that switch to remove structural
checks. Record exact exception/error categories and test the promised category.

Policy-based design needs identified use cases; do not invent policy axes merely
because templates make them possible.

## Use the C++20 baseline directly

Prefer requires/concepts over enable_if plumbing, standard range concepts over
homemade has-begin detection, and source_location over file/line capture macros
where the standard facility serves the contract. Use three-way comparison only
with the intended ordering contract; do not infer semantic ordering from syntax.

Do not guard guaranteed language features or introduce CONCEPT/REQUIRES emulation
macros. Conditional detection is for newer features, documented library support
gaps in actual supported toolchains, or real platform/backend boundaries. Keep
detection in one authoritative configuration facility (or clearly separated
language/platform/SIMD facilities), not ad hoc probes in every component.
Optional backends do not leak dependencies into unrelated lower components.

Separate orthogonal concerns such as core storage, concurrency, diagnostics and
serialization when their contracts differ. Verify any zero-cost claim rather
than assuming composition or templates guarantee it.

## Interfaces and types

- Express units, ownership, valid states, and distinct identifiers through types
  when that prevents meaningful misuse. Do not substitute a collection of booleans
  or untyped integers for a contract callers must otherwise memorize.
- Initialize objects into a defined state. Constructors or factories establish
  the invariant; distinguish an intentionally empty value from failed acquisition.
- Make single-argument constructors explicit unless implicit conversion is an
  intentional part of the interface. Check overload resolution and narrowing at
  call sites, not just the implementation's preferred invocation.
- Use const where observation is intended. A const member function or const pointer
  does not by itself establish deep immutability or thread safety.
- Preserve required standard-library protocol names, such as begin/end or value_type,
  when implementing those protocols. Local naming conventions must not silently
  break generic interoperability.
- Document parameter retention and result lifetime. Passing a view, pointer, or
  reference conveys access; it does not establish ownership transfer.

## Ownership, lifetime, and special members

Prefer members that manage their own resources so the containing class can use
default special-member behavior. If a class directly manages a resource or a
relationship between members, review destruction, copy construction/assignment,
and move construction/assignment together. Choose defaulted, deleted, or custom
operations from the invariant, not from a mechanical demand to write five bodies.
This follows the [rule-of-zero guidance](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-zero).

- Use RAII for owned memory, handles, locks, registrations, and other resources
  whose release has a scope or lifetime. Keep manual acquisition/release paired
  at a deliberate low-level boundary, not spread across error paths.
- Use unique ownership when it matches the design. Use shared ownership only
  when lifetime really is shared; account for reference-counting costs and cycles.
  A shared_ptr control block does not synchronize access to the pointed-to object.
- Treat raw pointers, references, span, string_view, iterators, and most range
  views as non-owning unless the actual type's contract says otherwise. Check
  invalidation on reallocation, erase, move, reset, and destruction.
- Review a type containing both an owner and pointers/views into that owner.
  Default copying or moving may preserve pointers to the wrong object even when
  each member operation is individually valid.
- Specify the moved-from state callers may use. Do not assume a moved-from object
  is empty unless its type promises that. Check self-assignment and self-move when
  they are part of the operation's supported contract.
- Capturing this in a lambda does not retain the object. Establish callback and
  coroutine capture lifetimes, cancellation, and destruction ordering before
  allowing asynchronous work to outlive its creator.
- Destruction must not leak exceptions into an unwinding path. For resources whose
  finalization can fail meaningfully, provide an explicit checked operation and a
  documented destructor fallback consistent with project policy.

## Error handling and exception guarantees

Follow the project's exception or error-result policy consistently. Do not insert
exceptions into an exception-disabled boundary or discard an error result because
the local caller has no convenient recovery path.

For mutating operations, state the promised failure behavior: no effects on failure,
valid but possibly changed state, or a narrower documented guarantee. Stage work
before committing when the operation needs an atomic state change. Account for
allocation, element operations, callbacks, hashing, comparison, and external APIs
that can fail.

Use noexcept only when the function's implementation and contract justify it.
Generic code may need a conditional specification that reflects its operations.
An incorrect noexcept turns an escaping exception into termination; it is not a
warning-suppression or performance annotation.

Use nodiscard for results whose loss is a likely correctness error, considering
real call sites. Side-effecting operations may legitimately return an optional
convenience result. Discard an intentional result explicitly under the warning
policy; do not remove a useful annotation to silence a test harness.

## Templates and compile-time behavior

- Express the operations a template actually requires, using C++20 concepts/requires where they express the actual contract.
  Do not accidentally require default construction, copying, or ordering when the
  algorithm needs none of them.
- Distinguish syntactic constraints from semantic obligations such as a strict
  weak ordering, equality consistency, or stable hashing. A concept check cannot
  establish every law required by an algorithm.
- Test instantiation, not merely parsing a template definition. Exercise supported
  const/ref categories and meaningful types that invalidate hidden assumptions.
- Use forwarding references and std::forward only for intentional forwarding.
  std::move is a cast that enables move overloads; it does not itself transfer a
  resource and may still result in copying for some expressions.
- Use constexpr for intended constant-evaluation capability and verify that
  capability with static_assert or other compile-time uses. A constexpr function
  can still be called at runtime; an annotation alone proves no optimization.
- Keep specialization and customization at the supported extension point. Do not
  add declarations to namespace std except where the standard permits them.

## Expressions and low-level operations

- Check signed/unsigned comparisons, narrowing conversions, sentinel values,
  shifts, and overflow before operations that require a valid range. Casting
  after overflowing arithmetic cannot repair the earlier operation.
- Separate object lifetime, alignment, aliasing, and byte representation. A cast
  to a pointer type does not make storage contain a live object of that type.
  Restrict raw-memory copying to types and operations whose requirements are met.
- Do not serialize an ordinary object's memory image as a portable format without
  a deliberate representation contract covering padding, endian order, widths,
  and versioning. Pointers and process-local layout are not durable identifiers.
- Use casts that express the intended conversion and justify unsafe boundaries.
  A project may deliberately use RTTI, polymorphism, or a validated static type
  system; do not import a blanket ban on casts or virtual dispatch.

## Concurrency and maintainable implementation

Protect shared mutable state with an explicit synchronization contract. Atomics
on individual fields do not automatically preserve a multi-field invariant;
volatile is not a replacement for inter-thread synchronization. Justify relaxed
ordering, lock-free reclamation, and callback-under-lock designs with the required
ordering and lifetime argument.

Follow [Naming and style](STYLE.md) and its formatter as mandatory rules; project
options cannot waive them. Keep functions responsible for
a coherent operation, keep implementation details out of public interfaces where
practical, and use standard or existing project facilities before adding another
utility. Do not invent an abstraction merely to avoid a few explicit operations.

Public declarations, internal helpers, and inline definitions also follow
[Headers and linkage](HEADERS_AND_LINKAGE.md). Select verification from
[C++ testing](TESTING.md), including failure and lifetime behavior rather than only
successful construction and destruction.
