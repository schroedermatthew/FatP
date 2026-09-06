# Engineering

Applies to design and implementation. Read the core and consult the profile's
local architecture, compatibility, and tooling decisions.

## Establish the contract

- Identify the behavior being changed, its callers, its authoritative state,
  and the invariants it must preserve. Describe the acceptance conditions before
  deciding that the implementation is complete.
- Trace a working path when one exists. Reuse its validated requirements and
  mechanisms where they fit; historical implementation is not proof that its
  design is required or correct.
- Distinguish a bug, an intentional policy, a missing requirement, and a proposed
  improvement. A preference for a different design is not evidence of a defect.
- Choose the smallest coherent design that satisfies the contract. Do not add
  unused extension points, speculative platforms, or parallel frameworks without
  a requirement that justifies their cost.

## State, resources, and boundaries

- Give mutable state an explicit authority. If caches, projections, replicas, or
  derived indexes exist, define their update, invalidation, and recovery rules.
  Do not let an unsynchronized duplicate become a second accidental authority.
- Make ownership, borrowing, transfer, and lifetime constraints explicit. Include
  teardown, cancellation, callback re-entry, and asynchronous completion in the
  design when they apply.
- Define transaction and failure boundaries. Preserve invariants through partial
  failure and specify whether callers may retry. Do not disguise errors as valid
  empty results unless that behavior is the documented contract.
- Validate untrusted inputs at the appropriate boundary. Protect secrets and
  sensitive values in logs, fixtures, generated artifacts, and error messages.
- Do not mutate a traversal or call unknown code under a lock without establishing
  that iterator validity, re-entry, and synchronization remain correct. Select a
  snapshot, staging, or other safe mechanism appropriate to the actual design.

These are properties to establish, not a mandate for a particular data model,
ownership library, exception policy, or concurrency framework.

## Make the change coherent

- For C++, follow the mandatory [naming and style guide](../cpp/STYLE.md); existing
  drift or a blank profile does not waive it. For other languages, follow the
  project's configured rules. Avoid unrelated formatting churn.
- Place shared behavior at its actual owner. Check for existing helpers and
  contracts before introducing another implementation or another source of truth.
- Apply the profile's compatibility policy to callers, stored data, wire formats,
  migrations, and generated artifacts. CORE defines the pre-release no-shims rule; released obligations need explicit
  preservation or an authorized migration decision.
- Evaluate a dependency's role, maintenance, integration, and licensing against
  local policy. Do not assume either zero dependencies or arbitrary additions.
- Keep generated and vendor-owned files distinct from authored source. Change the
  generator or supported extension point when that is the authoritative input.

## Establish conformance

For every significant invariant claimed, identify the enforcing code and the
failure path that would violate it. Use tests, source tracing, or other appropriate
evidence. "It builds" does not establish that a cache is coherent or an object has
one owner. Use [Testing](TESTING.md) to select checks and record remaining limits.
