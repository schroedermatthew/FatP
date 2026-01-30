# Migration Procedure — C to Modern C++

## Executive Summary

This procedure defines how we migrate legacy C code to modern C++ without breaking existing systems or creating rework cascades. The key insight is that **design decisions must be locked in a specific order**—boundaries before dependencies, concurrency before memory management, ownership before feature selection—because reversing that order forces expensive revisits across already-migrated code.

The checklists below capture both the system-wide policies we establish once and the repeatable workflow we apply to each module. Part 2 provides definitions, justifications, and an analysis of how this procedure increases development velocity.

---

## Part 1 — Strategic Execution Checklist

### Part A: System-Wide Policies

Establish these decisions once, before any module migration begins.

| #      | Policy                          | Summary                                                                 |
|--------|---------------------------------|-------------------------------------------------------------------------|
| **A1** | Namespace convention            | Define the naming hierarchy all modules will use.                       |
| **A2** | Error-handling policy           | Standardize how functions report success and failure in C++ code.       |
| **A3** | C boundary translation          | Define how errors and exceptions convert to stable C return codes.      |
| **A4** | C ABI contract                  | Hard rules for what can and cannot cross the C interface.               |
| **A5** | Contract and invariant approach | Establish how preconditions and postconditions are checked.             |
| **A6** | Tooling and verification        | Specify static analysis, sanitizers, and CI requirements.               |
| **A7** | Testing patterns                | Define categories of tests every module must have.                      |
| **A8** | Avoidances                      | Explicit bans on patterns that create maintenance problems.             |

### Part B: Per-Module Workflow

Apply these steps in order when migrating each module.

| #      | Step                                | Summary                                                                  |
|--------|-------------------------------------|--------------------------------------------------------------------------|
| **B1** | Define C API and ABI boundaries     | Identify what external callers depend on and what must stay stable.      |
| **B2** | Classify globals and dependencies   | Decide how the module obtains the services and data it needs.            |
| **B3** | Decompose into namespace hierarchy  | Organize code into the system naming convention before adding types.     |
| **B4** | Define concurrency requirements     | Declare threading behavior and shutdown rules.                           |
| **B5** | Decide memory and ownership         | Define who creates, owns, and destroys each resource.                    |
| **B6** | Identify compile-time opportunities | Determine which computations, checks, and configurations can be resolved at build time. |
| **B7** | Select language features            | Choose C++ facilities that match the ownership, concurrency, and compile-time models. |
| **B8** | Write tests                         | Verify correctness of both the C++ core and the C interface.             |
| **B9** | Validate performance                | Confirm the migrated code meets performance requirements.                |
| **B10** | Update documentation                | Record decisions so future work does not contradict them.                |

---

## Part 2 — Definitions and Justification

### Why This Order Matters

Migration failures come from making decisions in the wrong order, forcing teams to revisit already-migrated code when a later decision invalidates earlier assumptions.

The ordering in Part B ensures each decision is constrained by the decisions before it:

1. **Boundaries first** because they define what you cannot change without breaking external systems.

2. **Dependencies second** because how a module obtains services determines how invasive later changes will be.

3. **Namespaces third** because organizing names is cheap now and expensive after types and inheritance exist.

4. **Concurrency fourth** because threading behavior dictates ownership rules.

5. **Ownership fifth** because memory management must encode the concurrency and lifetime rules already established.

6. **Compile-time identification sixth** because the previous steps reveal which values are fixed by design versus genuinely dynamic.

7. **Features last** because containers, views, and utilities must match all the preceding decisions.

---

### How This Procedure Increases Velocity

Migration projects fail because rework consumes the schedule. A decision made in week three invalidates work from weeks one and two. Developers revisit the same files repeatedly. Merge conflicts multiply. Estimates become meaningless.

This procedure eliminates rework by front-loading decisions that constrain later choices. Each step produces a stable foundation that subsequent steps build on without disturbing.

**Stable foundations enable parallel work (B1–B3).** Once boundaries are defined, teams working on different modules proceed independently. Changes inside a module do not ripple into other teams' code. Early namespace organization avoids the high cost of moving classes with inheritance relationships later.

**Service registries reduce churn (B2).** With explicit parameter passing, adding a dependency to a low-level function changes its signature—and every caller's signature up to the entry point. One new dependency can touch dozens of files. With a service registry, you register once and request where needed. Files that don't use the new dependency remain untouched.

**Upfront concurrency prevents ownership rework (B4–B5).** If ownership is decided first and threading requirements emerge later, the ownership model must be revised—shared pointers added, synchronization retrofitted. Deciding concurrency first means ownership decisions stick.

**Compile-time classification accelerates feedback (B6).** Errors caught by the compiler appear in seconds. Errors caught at runtime require build-deploy-run-debug cycles.

**Deferred feature selection avoids premature commitment (B7).** Choosing a container before understanding ownership leads to fighting its semantics. Choosing after ownership and concurrency are settled means the choice fits the first time.

**Per-module validation catches problems early (B8–B9).** Defects found during module testing are localized. Defects found after integration require cross-team coordination. Performance problems found late are expensive to fix because the design is frozen.

**Documentation preserves decisions (B10).** Without this, future developers relitigate settled questions, sometimes reaching contradictory conclusions.

The ordering is a velocity multiplier. When followed, each decision is made once. When violated, the team pays for the same decision multiple times.

---

### A1 — Namespace Convention

Define a consistent hierarchy pattern (e.g., `company::project::module`) that all migrated code follows. This replaces C-style prefixes with language-enforced scoping.

**What to decide:** Hierarchy depth, naming rules per level, reserved names.

---

### A2 — Error-Handling Policy

Standardize when functions use expected values versus exceptions.

- **Expected values** for anticipated failures callers commonly handle (not found, validation failure, timeout).
- **Exceptions** for failures that are not normal outcomes (invariant violations, construction failure, unrecoverable states).

**The rule:** Each function chooses one mechanism. A function either returns expected (and does not throw for normal failures) or throws (and returns a plain result on success). No mixing.

---

### A3 — C Boundary Translation

**Hard rule:** Exceptions never cross the C boundary.

- If the C++ function returns expected, the façade maps success to out-params + OK, failure to error code.
- If the C++ function throws, the façade catches and maps to error codes. Unknown exceptions map to a general "unexpected" code.

**Optional:** Store diagnostics in thread-local storage; expose via a separate C function.

---

### A4 — C ABI Contract

Rules that keep the C interface stable across compilers, platforms, and versions:

- **No C++ types** in signatures or structs exposed to C.
- **Separate headers:** C-visible headers must not transitively include C++ headers.
- **Opaque handles** for C++ objects—pointer to incomplete type with explicit create/destroy functions.
- **Fixed-width integers** (`uint32_t`, `int64_t`)—avoid `int`, `long`.
- **Explicit allocation ownership**—document who frees; provide dedicated free functions.
- **Version structures** if they may grow—include size or version field.

---

### A5 — Contract and Invariant Approach

Establish how preconditions, postconditions, and invariants are checked:

- **Compile-time checks** for constraints on types, sizes, configurations.
- **Runtime checks** in debug/test builds. Decide whether they remain in release.

Check invariants at public function boundaries. Be cautious in destructors—objects may be intentionally partially dismantled.

---

### A6 — Tooling and Verification

Specify which tools run and what must pass before merge:

- **Static analysis** for bugs, style, known-bad patterns.
- **Sanitizers** (memory, thread, undefined behavior) where platform-supported.
- **CI matrix** across compilers and platforms.

Require sanitizer testing where supported; don't block the strategy where coverage is unavailable.

---

### A7 — Testing Patterns

Define required test categories:

- **Unit tests:** C++ implementation in isolation.
- **Integration tests:** C interface behaves as documented.
- **Translation tests:** Error mapping is correct; exceptions do not escape.
- **Benchmark tests:** Performance compared against original.

---

### A8 — Avoidances

Explicit bans:

- **No raw `new`/`delete`** except in dedicated low-level allocators.
- **No C-style casts**—use explicit cast operators.
- **No premature optimization**—profile first.
- **No monolithic service registries**—split by subsystem.

---

### B1 — Define C API and ABI Boundaries

Identify every function and structure external code depends on.

**Produce:**
- List of exported functions with signatures and behavior.
- Ownership rules for buffers/handles crossing the boundary.
- Error model: codes, meanings, diagnostic retrieval.

**Why first:** Everything else must fit within what this boundary can represent.

---

### B2 — Classify Globals and Dependencies

Inventory global state and decide how migrated code accesses it:

- **Immutable shared data:** Set once at startup, never modified—safe to remain global.
- **Service registry:** Central access to services; reduces signature size.
- **Explicit passing:** Dependencies as parameters; increases signature size but simplifies testing.

Decide per-global based on usage frequency, stability, test isolation needs, and acceptable churn.

**Why second:** Dependency strategy determines how much code changes when swapping implementations.

---

### B3 — Decompose into Namespace Hierarchy

Map legacy files and functions to their new namespace locations before introducing types or classes. Group related functionality. Identify boundaries between potential components.

**Why third:** Moving free functions is trivial. Moving classes with inheritance relationships is not.

---

### B4 — Define Concurrency Requirements

Declare threading behavior and shutdown rules.

**Classifications:**
- **Thread-confined:** Single-thread access only.
- **Thread-compatible:** Safe with external synchronization.
- **Thread-safe:** Handles concurrency internally.

**Shutdown:** Document who initiates, resource release order, and how access after destruction is prevented.

**Lock ordering:** If multiple locks exist, document acquisition order to prevent deadlocks.

**Why fourth:** Ownership rules depend on threading. You cannot design memory management correctly without knowing the concurrency model.

---

### B5 — Decide Memory and Ownership

For every resource (memory, handles, connections, locks), define who creates, accesses, and destroys it.

**Patterns:**
- **Exclusive ownership:** One component controls lifetime.
- **Shared ownership:** Released when last holder is done. Adds overhead.
- **Borrowed access:** Short-term use; owner guarantees validity.

**Why fifth:** Concurrency constrains these choices. Making ownership decisions before knowing the threading model causes either over-engineering or races.

---

### B6 — Identify Compile-Time Opportunities

Classify which values, checks, and computations can be resolved at build time:

- **Constants:** Buffer sizes, table dimensions, protocol limits.
- **Validation:** Array bounds, enum ranges, type compatibility.
- **Computation:** Lookup tables, conversion factors, mathematical constants.
- **Type properties:** Sizes, alignments, interface conformance.

**Produce:** A classification of significant values as compile-time or runtime, with rationale for borderline cases.

**Why sixth:** Previous steps reveal what's fixed by design. This informs feature selection.

---

### B7 — Select Language Features

Choose containers, views, synchronization primitives, and compile-time facilities:

- Match containers to ownership (growable vs fixed-size).
- Match views to documented lifetime guarantees.
- Match synchronization to the concurrency model.
- Match compile-time facilities to the B6 classification.

**Why last among design steps:** Features must serve the design. Choosing early forces redesign when they don't fit.

---

### B8 — Write Tests

Implement the categories from A7:

- C++ implementation in isolation.
- C interface with success and failure paths.
- Every error code, verifying correct mapping.
- Exception-triggering paths, verifying no escape.

**Why now:** Testing after design validates the design. Testing later risks shipping unvalidated code.

---

### B9 — Validate Performance

Measure migrated implementation against the original.

- Baseline the original under representative workload.
- Measure migrated code under the same workload.
- Define acceptable threshold (e.g., no more than 5% slower on critical paths).
- Profile before deciding to accept, optimize, or redesign.

Microbenchmarks can mislead. Prefer representative workloads.

**Why now:** Performance problems are cheaper to fix before the module is integrated.

---

### B10 — Update Documentation

Record decisions so future work does not contradict them:

- Boundary summary and stability guarantees.
- Dependency decisions.
- Concurrency and ownership model.
- Error handling approach.
- Test coverage and thresholds.
- Known limitations and deferred work.

**Why last:** Documentation written after decisions are final captures the actual state.
