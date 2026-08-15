---
doc_id: DN-IDGENERATOR-001
doc_type: "Design Note"
title: "Design Note - Sparse ID Claiming"
fatp_components: ["IdGenerator"]
topics:
  - "persisted identifier reservation"
  - "sparse identifier domains"
  - "interval free sets"
  - "identifier recycling"
  - "batch rollback"
constraints:
  - "claim cost independent of numeric gap"
  - "unclaimed lower identifiers remain issuable"
  - "existing policy behavior remains unchanged"
  - "finite unsigned identifier domains"
cxx_standard: "C++20"
last_verified: "2026-08-14"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "implemented"
---

# Design Note - Sparse ID Claiming

**Status:** Implemented 2026-08-14; contract ratified 2026-08-14  
**Decided:** Mechanism authorized 2026-07-22; detailed Fat-P contract ratified 2026-08-14  
**Last reviewed:** 2026-08-14 against Fat-P `adf89510` and Loom `20c80eb`

## Implementation status

Shipped in `include/fat_p/IdGenerator.h`: `SparseRecyclingPolicy`, `IdError::InvalidClaim`,
`IdGenerator::claim()`, the `configure_domain` seam, the three `detail::` traits, and the
`SparseIdGenerator` / `ThreadSafeSparseIdGenerator` aliases. The contract below is the
specification the implementation was written against; the deviations and the one gap are
recorded in §Implementation notes at the end rather than left for a reader to discover.

## Scope

This note defines an additive `IdGenerator` policy and claim operation for reserving arbitrary
identifiers without work proportional to the identifier's numeric value or its distance from the
generator's current high-water mark.

## Not covered

- Changing the behavior or aliases of existing recycling policies
- Persistence formats or recovery policy in consuming applications
- Hierarchical identifier encoding in Loom
- Replacing `ActiveIdTracker` or changing random allocation
- Benchmark claims; this note specifies complexity and required measurements only

## Prerequisites

- Familiarity with `IdGenerator`, `AllocationPolicy`, and `RecyclingPolicy`
- Familiarity with ordered-map predecessor and successor searches
- Understanding of `Expected<T, IdError>` and the generator's batch rollback behavior

## Design Note Card

**Decision:** Add sparse claiming through one new recycling policy and a constrained generator
operation.  
**Context:** A persisted high identifier cannot be reserved efficiently through repeated
`generate()` calls, and an active-set-only claim can still be reissued from a recycle pool.  
**Options considered:** Bounded generation, active-set marking, changing all policies, and a
complete interval free domain.  
**Chosen option:** `SparseRecyclingPolicy` with disjoint inclusive free intervals and
`IdGenerator::claim()`.  
**Rationale:** It coordinates every source of identifiers, preserves lower free identifiers, and
makes claim cost depend on interval count rather than numeric distance.  
**Implications:** Sparse-policy instances gain explicit claiming and different free-count and
allocation-failure characteristics; existing policy instances retain their current API behavior.

## Decision

Fat-P will add `SparseRecyclingPolicy` and a policy-constrained `IdGenerator::claim()` so an
arbitrary free identifier can be made active in O(log I) time while every other free identifier,
including lower identifiers and the open tail, remains issuable; `I` is the number of free
intervals.

---

## Context

`IdGenerator::generate()` currently asks `RecyclingPolicy::get_recycled()` before consulting the
allocation policy. The existing immediate and minimum policies store identifiers only after
`release()`. The generator has no operation that reserves an identifier chosen by a caller.

The first consumer is a persisted hierarchical allocator. Its current workaround repeatedly
generates identifiers until it reaches the persisted value, releases every intermediate value,
and stops after a fixed number of attempts. A sparse claim such as 60000 therefore performs work
proportional to the gap and can fail even though the requested identifier is free.

Adding the requested identifier only to `ActiveIdTracker` is insufficient. A stale occurrence may
still exist in a recycling policy, and `generate()` consults that policy first. Advancing a
sequential allocation high-water mark also discards unclaimed lower identifiers, contrary to the
selected sparse-domain policy.

The new policy therefore represents the complete free domain. The existing `generate()`
dispatch is the right shape — consult the recycle source first, and treat its emptiness as
exhaustion — but the full-domain path needs a different accessor and a different step order
than the shipped body provides; §Generation specifies both.

---

## Constraints

1. **Additive policy surface** -- Existing policies, aliases, and their successful-operation
   sequences must not change.
2. **Gap-independent claim** -- Claiming an identifier must do no work proportional to its value
   or distance from the current high-water mark.
3. **Sparse preservation** -- Claiming a high identifier must not make lower free identifiers
   unavailable.
4. **Complete coordination** -- `generate()`, `generate_batch()`, `release()`, batch rollback,
   `claim()`, and `reset()` must agree on the same active/free partition.
5. **Finite-domain correctness** -- The minimum, maximum, empty-domain, and arithmetic-boundary
   cases must not overflow.
6. **Typed refusal** -- Duplicate or invalid claims return an `IdError`; they do not assert or
   terminate as normal control flow.
7. **Policy-local cost** -- Complexity is stated in terms of free-interval count, never numeric
   identifier value.
8. **Shared locking** -- `claim()` uses the generator's exclusive `ConcurrencyPolicy` lock, like
   generation and release.

---

## Options Considered

### Option A: Bounded generate-and-release walk

The consumer can generate until it reaches the requested value and release all intermediate
identifiers.

**Pros:** Requires no Fat-P change and uses only current public operations.  
**Cons:** Work and temporary storage are O(gap). Any fixed attempt ceiling rejects valid sparse
state, while removing the ceiling permits an attacker-controlled or corrupt value to drive an
unbounded walk.

### Option B: Mark only the active set

`claim(id)` can insert directly into `ActiveIdTracker` and make `generate()` skip active recycled
entries lazily.

**Pros:** The direct claim is independent of the numeric gap, and existing recycle containers can
remain unchanged.  
**Cons:** A high claim moves sequential allocation above the claimed identifier, so never-issued
lower identifiers disappear unless a second free structure is added. Lazy recycle invalidation
also makes reclamation cost and `recycled_count()` describe stale entries rather than issuable
identifiers.

### Option C: Replace every recycling policy with one interval policy

The generator can adopt interval recycling as its only recycling representation.

**Pros:** One representation and one claim mechanism apply to every generator.  
**Cons:** FIFO and minimum-first users receive new ordering, complexity, memory, and exception
behavior even when they never claim a persisted identifier. This violates the additive-only
decision.

### Option D: Add a complete sparse free-domain policy (Selected)

A new policy stores every free identifier as a union of disjoint intervals. Only generators that
select that policy expose `claim()`.

**Pros:** Claim, release, and minimum-free generation are independent of numeric gaps; the policy
preserves every lower free identifier; existing policies and aliases remain unchanged.  
**Cons:** The policy uses ordered dynamic storage, free-count calculation is O(I), and each
activation must pre-fund the node its release may need (§Return credits), costing `O(A)`
reserved nodes and moving allocation onto `generate()`/`claim()`.

---

## Detailed Contract

### Domain configuration

The policy cannot discover its own domain. `IdGenerator` passes `base_id` to the allocation
policy and to `mBaseId`, but **default-constructs the recycling policy**, which therefore never
learns the base:

```cpp
explicit IdGenerator(underlying_type base_id = 0)
    : AllocationPolicy(base_id)
    , RecyclingPolicy()          // receives nothing
    , ConcurrencyPolicy()
    , mBaseId(base_id)
```

`reset()` has the same shape: it reaches the recycling policy only through the nullary
`clear()` — the identical call the destructor makes — so a policy that must *rebuild* a
domain has no hook to do it in.

Fat-P already has an allocation-side upper-bound seam. Commit `adf89510` added a two-argument
`IdGenerator(base_id, upper_bound)` constructor constrained by
`detail::accepts_upper_bound_v<AllocationPolicy>`; the participating allocation policy receives
both values. The opt-in is declared by the policy rather than inferred from constructor arity,
because `RandomAllocationPolicy(uint64_t seed, int ignored)` also accepts two arguments and must
not be mistaken for a bounded policy.

The sparse policy needs the same public construction shape but consumes the domain on the
**recycling** side. Fat-P therefore adds an independent guarded configuration seam, used at
exactly two call sites:

1. **Construction.** After the initializer list, `IdGenerator` calls
   `RecyclingPolicy::configure_domain(base_id, effective_upper_bound)` when
   `detail::has_domain_configure_v<RecyclingPolicy>` holds.
2. **`reset()`.** The same guarded call rebuilds `[base, effective_upper_bound]` **before** the
   active set is cleared. This call is distinct from `clear()`; `reset()` must not rely on
   `clear()` to restore a domain. The rebuild reuses an owned node in every state reachable
   without a move, and is conditionally `noexcept` for exactly the state that is not — see
   §Reset and destruction.

Destruction continues to call the non-allocating `clear()` and does not rebuild.

The three existing recycling policies do not model `has_domain_configure_v`, so neither call site
invokes a new recycling operation for them and their observable behavior is unchanged.

**Upper-bound construction.** The generator keeps its shipped one-argument constructor and its
shipped allocation-policy opt-in. The two-argument constructor is available when either policy
role declares that it consumes a bound:

```cpp
explicit IdGenerator(underlying_type base_id = 0);

template <typename AP = AllocationPolicy, typename RP = RecyclingPolicy>
    requires (detail::accepts_upper_bound_v<AP> ||
              detail::has_domain_configure_v<RP>)
IdGenerator(underlying_type base_id, underlying_type upper_bound);
```

This is one public operation with role-directed forwarding, not one policy pretending to be the
other. An allocation policy modelling `accepts_upper_bound_v` is constructed with
`(base_id, effective_upper_bound)`; otherwise it retains its `(base_id)` construction. A
recycling policy modelling `has_domain_configure_v` is default-constructed and then receives
`configure_domain(base_id, effective_upper_bound)`. If both roles opt in, both receive the same
effective value. Private tag-dispatched constructors may select the correct initializer-list
form; constructing a temporary policy and moving it into the base is not required and would add
an accidental movability requirement to custom policies.

The one-argument constructor delegates through the same role dispatch using the default request.
An opted-in allocation policy receives the effective default, and an opted-in recycling policy
receives it through `configure_domain`; policies that opt into neither retain their shipped
construction. The generator retains the effective value for `reset()` and the sparse
claim-domain check. An `effective_upper_bound < base_id` is a precondition violation, not a
runtime error.

**Reserved invalid sentinel.** `detail::has_invalid_sentinel_v<IdType_>` identifies the
StrongId-style contract already enforced by `generate()`: the underlying maximum denotes
`invalid()` and must never be issued. For such an ID type, the effective upper bound is
`min(requested_upper_bound, numeric_limits<underlying_type>::max() - 1)`, which is the underlying
value of `StrongId::max()`. For an integral ID type without that sentinel, the effective bound is
the requested value unchanged. The one-argument default request remains
`numeric_limits<underlying_type>::max()`; normalization excludes the sentinel only when the ID
type declares one.

This normalization is part of domain construction, not a late generation guard. It keeps the
free set equal to the set of issuable identifiers: `claim(invalid())` is outside the effective
domain, `recycled_count()` never counts an unissuable value, and exhaustion still means an empty
interval map. If a future ID wrapper declares `invalid()` at a value other than the underlying
maximum, the sentinel trait must be strengthened to expose that value before the wrapper can use
the sparse policy; this contract does not infer arbitrary sentinel placement from the function
name alone.

In the remainder of this note, `upper_bound` means this normalized effective upper bound unless
the text explicitly says `requested_upper_bound`.

`upper_bound` is configurable per instance because a consumer's valid domain can be narrower
than its ID type. Loom is exactly that case: `index2BoneId` consumes an index 16 bits per level and
`BoneId::child()` requires `depth < 16`, so a parent at depth 13 has three levels left and a
valid index ceiling of `2^48 - 1`, while a `std::size_t` generator's domain runs to
`2^64 - 1`. Without a configurable ceiling the generator would happily issue an index the
consumer cannot represent, and generator exhaustion would never coincide with the consumer's
real exhaustion. A consumer may instead enforce its own bound before calling, but the note
does not permit both to be left unstated.

### Representation and invariant

`SparseRecyclingPolicy<IdType>` stores inclusive intervals in an ordered map keyed by upper bound:

```text
upper -> lower
4     -> 1       represents [1, 4]
59999 -> 6       represents [6, 59999]
LIMIT -> 60001   represents [60001, effective_upper_bound]
```

Keying by upper bound lets `get_recycled()` take the first interval's lower value without changing
the map key. The policy maintains these invariants:

1. Every interval satisfies `base <= lower <= upper <= upper_bound`, where both endpoints come
   from the configuration seam above.
2. Intervals are disjoint and non-adjacent; adjacent intervals are merged immediately.
3. The union of free intervals and `ActiveIdTracker` is exactly `[base, upper_bound]`, **as
   observed between operations**. Inside an operation, under the lock, an identifier may be
   momentarily in both sets (`generate()`, `claim()`) or in neither (`release()`), because
   each such window is closed by a step that cannot throw. No operation may RETURN, or
   propagate an exception, with an identifier in neither set.
4. The free intervals and active set are disjoint between operations.

The empty generator starts with one interval, `[base, effective_upper_bound]`. For an ordinary
integral ID type `LIMIT` may be the underlying maximum; for a StrongId-style type it is at most
one less, so the invalid sentinel is never part of the interval union. Exhaustion is represented
by an empty map, not by a guessed sentinel value, and specifically not by a zero count.

### Generation

**The full-domain path does not use `get_recycled()`.** That accessor is a pop — every
shipping policy removes the value as it returns it — and the staged order below must know the
identifier *before* the free set is mutated. A full-domain policy therefore exposes two
nothrow, non-allocating operations instead:

- `peek_lowest() -> std::optional<IdType>` — the lowest free identifier; mutates nothing.
- `remove_lowest()` — removes that identifier, advancing the first interval's lower bound or
  erasing it when it held one value. **Precondition:** the map is non-empty, i.e. a prior
  `peek_lowest()` returned a value under the same lock. This operation never splits, so it
  never allocates.

Removing an *arbitrary* free identifier is a different operation with different costs — it
can split an interval — and belongs to `claim_at(id)`, not here. Naming the generate-path
operation `remove_lowest()` rather than a general `remove(IdType)` keeps that distinction in
the type system instead of in a precondition comment.

`get_recycled()` remains for the three shipping policies and is not called on the full-domain
path. A `nullopt` from `peek_lowest()` means the domain is exhausted, and the generator
reports `Overflow` without consulting the allocation policy — see §Exhaustion below.

**The generate ordering therefore changes for a full-domain policy.** The shipped body takes
the value out of the recycling policy first and only then calls `mIdsInUse.insert()`, which
allocates. If that insertion throws, the identifier is in neither structure and, because
exhaustion is an empty map, it is never recovered — invariant 3 would be permanently false.
The staged order is:

1. `peek_lowest()`; if empty, report `Overflow`.
2. Acquire the identifier's **return credit** (below) — may allocate; nothing is mutated yet,
   so a throw leaves the pre-call state exactly.
3. Insert into `ActiveIdTracker` — may throw; the free set is still untouched.
4. `remove_lowest()` — no allocation, cannot throw.

Steps 3 and 4 briefly hold the identifier in both sets, which invariant 3 permits inside an
operation. No step can leave it in neither.

### Exhaustion

With a configurable effective upper bound, the allocation policy can no longer answer the
exhaustion question: `SequentialAllocationPolicy::next_id` reports exhaustion at
`std::numeric_limits<IdType>::max()`, which may be far above the configured ceiling. A
full-domain generator therefore never falls through to the allocation policy — an empty free
set *is* exhaustion, and `Overflow` is reported directly.

This makes the allocation policy **semantically vestigial** on the full-domain path, and the
note says so rather than implying otherwise. Both `next_id()` call sites and full-domain
rollback's `revert()` become unreachable, and the `is_first`/`max_element()` logic can no
longer influence issuance. `AllocationPolicy::reset(mBaseId)` is still called by `reset()`,
and the allocation policy is still constructed and moved; that state is inert and cannot
disagree with the free set, but it is not removed — the policy remains part of the template
signature so the existing architecture is untouched.

**The pairing constraint needs its own check.** A trait on the RECYCLING policy cannot
constrain which ALLOCATION policy it is paired with, so the earlier claim that the
full-domain trait "prevents pairing the policy with random allocation" was wrong on
mechanism. The generator constrains the pair: when
`detail::is_full_domain_policy_v<RecyclingPolicy>` holds, `AllocationPolicy` must model
`detail::is_sequential_policy_v`. Random allocation is rejected there, at the pairing.
`next_id()` must remain well-formed for the permitted set
even though it is never called, because the branch is discarded at compile time rather than
removed from the class.

The implementation must identify a full-domain policy at compile time, in the `detail::` trait
style the header already uses for `may_collide_v` and `is_random_policy_v`. The trait is a
**positive property a policy opts into**, not a blacklist: a policy is full-domain only if it
declares itself so. This prevents pairing the policy with random allocation, lets batch
rollback distinguish a free-domain value from a new allocation-policy value, and keeps the
`Overflow` guarantee provable — that guarantee holds for the sequential allocation policies
this note authorizes, and the trait is what refuses the combinations it does not.

### Return credits — why release cannot allocate

Every mutating path that hands an identifier *back* may need a new map node: a singleton
release into a gap, an interior claim that splits one interval into two, and batch rollback
restoring several isolated values. Taking an identifier does not free a node — removing the
head of `[0, MAX]` just increments the lower bound — so nodes are not conserved and a
"reservoir of exhausted nodes" does not fund the return path. That approach was considered
and rejected on this counterexample:

```text
free [0, MAX];  generate 0 -> [1, MAX]   (no node freed)
                generate 1 -> [2, MAX]   (no node freed)
                release 0  -> needs [0, 0], reservoir empty
```

Instead, **each activation carries a return credit**: the node its eventual release may need
is reserved when the identifier becomes active — in `generate()` step 2 above, and in
`claim()` before any interval is disturbed. An interior claim reserves the one extra node its
split requires. Release, batch rollback, and reset then consume credits and **allocate
nothing**.

**Where credits live.** A credit is a detached map node (`map::node_type`), held in a
policy-owned stack. Reserving one means allocating a node — by inserting a scratch key and
immediately extracting it, or by an equivalent node-source — and pushing it; consuming one
means popping it, rewriting its key and value, and inserting it. The stack's own capacity is
grown on the activation side only, so no consuming path can allocate.

The stack must **release capacity as credits are spent**, not merely shrink its size. A
`std::vector` that retains its high-water capacity would make storage `O(max A)` rather than
`O(A)`, so a generator that briefly held a million identifiers would keep that footprint for
its lifetime. Either use a node-chaining container with no separate capacity, or shrink
explicitly; the storage bound in §Complexity is stated for current `A` and must be honoured
as written.

This is not the rejected reservoir. A reservoir is fed *opportunistically* by intervals that
happen to be exhausted, and is empty exactly when the first singleton release needs it.
Credits are funded *per activation*, before the activation is allowed to proceed.

**The invariant is `C == A` at operation boundaries, not at every instant.** Each operation
crosses it deliberately, and the note is precise about which way:

| Operation | Transient state | Restored at return |
|---|---|---|
| `generate()`, `claim()` | `C = A + 1` after reserving, before the active insert | `C == A` |
| `release()` | `C = A + 1` after the active erase, before the credit is resolved | `C == A` |
| exhausted `reset()` | `C = A - 1` after taking the rebuild credit | `C == A == 0` |

Per-transition arithmetic over a completed operation: singleton insertion, either extension,
and merge-both each move `(A, C)` by `(-1, -1)`; an interior `claim()` reserves two nodes,
spends one on the split, and finishes `(+1, +1)`; batch rollback is repeated release and
finishes `(0, 0)` relative to its own pre-batch state.

**Unwinding.** A reservation is scope-guarded and committed only when its operation succeeds.
If the active-set insert throws after the credit was reserved — the one throwing step in
`generate()` and `claim()` — the credit is released during unwind, so `C == A` still holds at
the boundary. Without that rule the throw path would leak a credit on every failure and the
`C == A` relation would drift upward, which matters because §Reset's second case depends on
it.

A transition that does not consume its credit destroys it, so the reserved-node count tracks
`A` exactly and does not accumulate:

- Singleton insertion consumes the credit.
- Merging both neighbours consumes one *map* node and destroys the credit.
- Extending either neighbour consumes no node and destroys the credit.

The cost is `O(A)` reserved nodes, which the storage bound already accounts for. The benefit
is that release is nothrow *by construction* rather than by hiding a `bad_alloc` behind a
`noexcept` boundary — see the exception specifications below.

### Claim

`claim(id)` is available only when the selected recycling policy models the sparse-claim contract.
It acquires the generator's exclusive lock and follows this order:

1. Reject an already-active identifier with `IdError::AlreadyInUse`.
2. Reject an identifier outside `[base, effective_upper_bound]` with `IdError::InvalidClaim`,
   before any lookup. This is the below-base, above-ceiling, and reserved-sentinel test, and it
   is a domain test, not a free-set test.
3. Find the first interval whose upper bound is not less than `id`; if none contains `id`, the
   identifier is free-set-absent but domain-valid, which means it is active — already refused
   at step 1 — or the policy state is inconsistent. Report `IdError::InvalidClaim`.
4. Reserve this activation's return credit, plus one extra node if the claim splits an
   interval. All allocation happens here, while nothing has been mutated.
5. Insert `id` into `ActiveIdTracker` — may throw; the free set is still intact.
6. Remove `id` from the interval by erasing, trimming, or splitting it, consuming the reserved
   nodes. No allocation, cannot throw.

Removing an endpoint does not add an interval. Removing an interior value replaces `[a, b]`
with `[a, id - 1]` and `[id + 1, b]`; both operations are guarded so neither `id - 1` nor
`id + 1` is evaluated at a domain endpoint (§Boundary arithmetic). Because every allocating
step precedes every mutating step, a throw from `claim()` leaves the generator exactly as it
was found.

`IdError::InvalidClaim` is an additive error value for an identifier outside the configured
domain or a detected policy-state contradiction. It is distinct from `AlreadyInUse`, which means
the caller named an active identifier.

### Release and merging

The shipped `release()` erases from `mIdsInUse` first and uses that erase as its existence
proof, then calls `add_recycled`; `release_batch()` repeats that order. **The sparse policy
keeps this order**, which is a deliberate reversal of an earlier draft of this note: because
return credits make the free-set transition allocation-free, there is no longer any reason to
put the free-set step first, and keeping the shipped order means `release()` needs no
restructuring for existing policies.

Having proved `id` was active, the policy finds the neighboring intervals and performs exactly
one of four transitions:

1. Merge both neighbours through `id` — the surviving entry is the RIGHT neighbour, whose key
   is already the merged interval's upper bound; its value becomes the left neighbour's
   lower bound and the LEFT entry is erased. The credit is destroyed.
2. Extend the left interval's upper bound — rekeys that entry (§Rekeying). Credit destroyed.
3. Extend the right interval's lower bound — a value-only write. Credit destroyed.
4. Insert the singleton interval `[id, id]` — consumes this activation's return credit.

Every transition is allocation-free because the credit was reserved at activation, so none of
them can throw. **Rekeying** an entry (transitions 1 and 2) is done by extracting its node,
assigning the new key through the node handle, and reinserting — never by erase-and-insert,
which would deallocate and reallocate.

Release keeps the shipped order: the active-set erase comes FIRST and is the existence proof,
then the free-set transition runs. That leaves the identifier in neither set for the width of
one nothrow step — permitted by invariant 3, which constrains observable states, and safe
precisely because the free-set transition cannot fail. `generate()` and `claim()` have the
mirror-image window, holding the identifier in both sets. Both are invisible outside the
operation: the lock is held throughout, and no exception can escape between the two steps.

Arithmetic checks use comparisons guarded at `base` and `upper_bound`; they never evaluate an
overflowing `id + 1` or an underflowing `id - 1` (§Boundary arithmetic).

### Exception specifications

`release()` and `release_batch()` **keep the exception specifications they ship with today.**
An earlier draft proposed making them conditionally `noexcept` for the sparse instantiation;
that is withdrawn, because it does not achieve what it appears to:

- `~IdGuard()` calls `release()` and is implicitly `noexcept`.
- `IdGuard::operator=(IdGuard&&) noexcept` also calls `release()`.
- The first consumer's `LoomIdAllocator::release` is itself declared `noexcept`.

A throwing sparse `release()` would therefore terminate at three call sites regardless of the
generator's own specification — one of them outside Fat-P entirely. Conditional `noexcept`
would relocate the failure, not prevent it, while making the generator's signature vary by
policy.

The specifications are kept because return credits make sparse release **nothrow by
construction**, not because termination hides an allocation failure. That distinction is the
whole point: the two shipping recycling policies already declare `add_recycled` `noexcept`
over allocating containers (`deque::push_back`, `set::insert`), so today's convention is that
out-of-memory terminates. The sparse policy does not need that convention, and must not rely
on it.

Allocation failures surface where allocation happens — `generate()`, `claim()`, and
`generate_batch()`, none of which is `noexcept` today, and each of which allocates only its
return credits, before mutating anything. `release()` and `release_batch()` allocate nothing
and keep their shipped `noexcept`. `reset()` is the one exception: it is allocation-free in
every state reachable without a move, and conditionally `noexcept` to cover the moved-from
case (§Reset and destruction). An allocation exception is not translated
to `IdError`, because `IdError` describes identifier-domain outcomes rather than memory
failure; but it must never escape after a partial mutation.

**Traits.** The shipped `detail::accepts_upper_bound_v<P>` remains the allocation-policy opt-in
for constructor forwarding. Three additional `detail::` traits are added, in the style of the
header's existing `may_collide_v` and `is_random_policy_v`:

- `detail::is_full_domain_policy_v<P>` — the policy owns the complete domain, exposes
  `peek_lowest`/`remove_lowest`/`claim_at`, and opts in explicitly. This gates the generate
  staging order, the exhaustion path, batch rollback, and the availability of `claim()`.
- `detail::has_domain_configure_v<P>` — the recycling policy accepts
  `configure_domain(base, upper)`; this also opts the policy into the generator's two-argument
  construction surface.
- `detail::is_sequential_policy_v<P>` — the allocation-policy side of the pairing check
  above. It exists because the other two traits describe the recycling policy and therefore
  cannot say anything about what it is paired with.

`SparseRecyclingPolicy` models the first two. A policy modelling the first but not the second
is ill-formed and is rejected with a `static_assert`, since a full-domain policy that cannot
learn its domain is exactly the defect this contract exists to prevent.

**Generator move operations — movability is CONDITIONAL, and the declarations mislead.**
`IdGenerator` declares `IdGenerator(IdGenerator&&) noexcept = default;` and the matching
assignment (`IdGenerator.h:804-805`). Those declarations do not mean what they appear to
mean, and this note states the measured behavior rather than the declared one.

`= default` is a request, not a grant: it becomes `= delete` when a base cannot satisfy it.
`SingleThreadedPolicy` deletes its copy constructor and copy assignment
(`ConcurrencyPolicies.h:293-295`), which suppresses its implicit moves, and `IdGenerator`
inherits it privately. Measured with trait probes (MSVC 18, `/std:c++20`):

| Instantiation | move ctor | move assign |
|---|---:|---:|
| `SimpleIdGenerator`, `DenseIdGenerator`, `ThreadSafeIdGenerator`, `RandomIdGenerator` | 0 | 0 |
| Same policies but `ConcurrencyPolicy = UniqueRWLockPolicy` | 1 | 1 |

So **every shipped alias is immovable**, while a custom instantiation over a movable
concurrency policy — `UniqueRWLockPolicy` holds its mutex in a `unique_ptr` and declares
proper moves (`ConcurrencyPolicies.h:555-562`) — **is** movable. A `SparseIdGenerator`
alias that keeps `SingleThreadedPolicy` is therefore immovable, but this note permits
explicit instantiations, so the sparse policy must remain correct under a movable one.

Consequences for this contract:

- The moved-from SOURCE state — empty interval map, no actives, no credits — is
  **unreachable through the shipped aliases and reachable through a movable
  instantiation**. It is the sole reason `reset()` is conditionally `noexcept`
  (§Reset and destruction), and that conditional stands. It must not be removed on the
  argument that the shipped aliases cannot move: the contract is written for the template,
  not for four of its instantiations.
- A moved-from generator is **not** safe to operate on. `UniqueRWLockPolicy`'s moved-from
  source is left with a null mutex pointer, so operations that take the lock are undefined
  rather than merely degenerate. An earlier revision of this note claimed every operation
  stays well-defined on a moved-from generator, reporting `Overflow`/`InvalidClaim`/
  `InvalidRelease`; that claim is **withdrawn as false**. The only operations a moved-from
  generator supports are destruction and move-assignment.
- If `IdGenerator` is ever made unconditionally immovable — by deleting its move members
  rather than defaulting them — the moved-from case disappears and `reset()` may return to
  an unconditional `noexcept`. That is an owner decision recorded in §13 of the Loom
  register discussion, not something this note assumes.

### Batch rollback

Batch rollback records provenance per element when each identifier is committed. Commit
`b7b16d1e` replaced the former value-based inference with a `from_pool` record because comparison
with the pre-batch maximum was wrong for every recycling policy: a pooled identifier above the
active maximum was misclassified as newly allocated, and an empty pre-batch active set caused all
pooled identifiers consumed by the batch to be discarded on rollback. No policy may reconstruct
provenance from identifier values.

The sparse path uses that same recorded-provenance contract, with every generated element marked
as coming from the full-domain free set. A failed `generate_batch()` returns every generated
identifier to the interval set and erases it from the active tracker. It does not call the
allocation policy's `revert()`. The restored interval union and active set must be identical to
their pre-batch state. What distinguishes the sparse policy is not how provenance is determined;
it is that the return credits make the policy-facing restoration allocation-free.

**That guarantee is only achievable because rollback allocates nothing.** Each element of the
batch acquired a return credit when it became active, and rollback consumes those credits, so
restoring even a set of isolated singletons — the worst case, where every take erased its
interval — needs no new node. Without credits this promise would be unkeepable: consider
`SparseIdGenerator<uint8_t>` with free `{[0,0],[2,2],[4,4],[6,6]}` and `generate_batch(5)`.
The first four elements empty the map, the fifth finds the domain exhausted, and rollback must
then rebuild four separate intervals — four allocations, on the failure path, in a function
whose contract is refusal rather than throwing. Rollback is therefore specified as
allocation-free and `noexcept` in its policy-facing step.

### Reset and destruction

`reset()` restores the free domain to exactly `[base, upper_bound]`, clears all active
identifiers, and resets the allocation policy to the same base. It reaches the policy through
the guarded `configure_domain` seam, **not** through `clear()` — `clear()` alone would leave
the interval map empty, and an empty map means exhausted, so a literal `clear()`-only reset
would silently degrade the generator to plain sequential allocation while reporting nothing.

`reset()` does not build a replacement map. It reuses a node it already owns:

- If the interval map is non-empty, extract any node, rewrite its key and value to
  `[base, upper_bound]`, clear the remainder, and reinsert it. Node handles permit the key
  change, so nothing is allocated.
- If the map is empty but identifiers are active, the domain is fully exhausted, so at least
  one return credit is held. Reset consumes one credit for the rebuilt interval.

Reset destroys the remaining credits with the active set it clears.

**Those two cases are exhaustive only while the generator cannot move, so `reset()` is
conditionally `noexcept`.** Subject to the invariants above, the proof is: the configured
domain is non-empty because `base <= upper_bound`; free ∪ active equals that domain at every
operation boundary; credits equal the active count; therefore a non-empty map supplies a
node, and an empty map implies full exhaustion and so at least one active identifier and one
credit. No other conforming route to empty-map-with-zero-credits exists — `clear()` is
unreachable through private inheritance, and exception paths preserve the invariants.

The exception is a MOVED-FROM generator, whose map, active tracker, and credit stack are all
empty at once. Whether that state is reachable depends on the instantiation
(§Generator move operations): the shipped aliases cannot move, but an instantiation over
`UniqueRWLockPolicy` can. The contract is written for the template, so it must cover it:

```cpp
using MovableGen = fat_p::IdGenerator<uint16_t, Sequential, Sparse, ExpectedError,
                                      fat_p::UniqueRWLockPolicy>;   // move traits: 1, 1
MovableGen a(0);
auto b = std::move(a);
// a now has an empty map, no actives, no credits — and a null mutex.
```

The contract is therefore:

- For a full-domain policy, `reset()` is **conditionally `noexcept`**, false exactly when the
  policy may need to allocate its rebuild node. The three existing policies keep the
  unconditional `noexcept` they ship with, because their `clear()` cannot allocate.
- The `IdGuard` objection that withdrew conditional `noexcept` from `release()` does not
  apply here: no destructor and no guard calls `reset()`.
- Allocation is confined to the moved-from case, which only a movable instantiation can
  reach. In every state reachable without a move, one of the two reuse cases applies and the
  rebuild is allocation-free.
- Calling `reset()` on a moved-from generator is in any case undefined for a
  `UniqueRWLockPolicy` instantiation, because taking the lock dereferences a null mutex. The
  conditional `noexcept` exists so the specification is honest about the allocation, not
  because that call is supported.

**Two earlier revisions of this note got this wrong in opposite directions**, and both errors
are recorded rather than quietly fixed. The first claimed the two cases were exhaustive full
stop, omitting moved-from state entirely. The second — after both an independent review and
the peer round agreed the first was broken — was about to declare the moved-from case
unreachable for ALL instantiations, which is equally false. The measured answer is
conditional, and only a compiler probe produced it.

Destruction uses the policy's non-allocating `clear()` operation. It does not rebuild the
domain.

### `recycled_count()`

For the sparse policy, `recycled_count()` means the number of currently free identifiers, including
never-issued identifiers in the open tail. It sums inclusive interval cardinalities in O(I) time.

For a plain integral ID type, the full domain may contain `SIZE_MAX + 1` values, which cannot be
represented by the existing `std::size_t` return type. A StrongId-style domain never contains its
reserved top sentinel and therefore does not have that extra value. The result saturates at
`std::numeric_limits<std::size_t>::max()`. It is exact whenever the free cardinality is
representable. The operation performs checked addition and never wraps.

**The per-interval term saturates too, not just the sum.** `upper - lower + 1` is itself
unrepresentable for a full-width interval — `[0, 2^64-1]` has cardinality `2^64`, and the
expression wraps to `0` in both `IdType` and `std::size_t`. Compute `span = upper - lower`,
which is always representable, and saturate before adding one. A zero result must never be
produced for a non-empty map: exhaustion is an empty map (§Representation), and a count of
zero from a domain that still holds free identifiers would invert the query's meaning.

### Boundary arithmetic

No operation evaluates `id - 1` when `id == base`, or `id + 1` when `id == upper_bound`. The
split in `claim()` is the only place both appear, and each side is guarded by the comparison
that makes it meaningful:

- `id > lower` gates the left fragment `[lower, id - 1]`; when `id == lower` the interval is
  trimmed from the front instead.
- `id < upper` gates the right fragment `[id + 1, upper]`; when `id == upper` the interval is
  trimmed from the back instead.
- When `id == lower == upper` the interval is erased and no fragment is formed.

Release adjacency uses the same shape: a left neighbour is adjacent when its `upper` equals
`id - 1`, tested as `upper + 1 == id` only when `upper < upper_bound`; a right neighbour is
adjacent when its `lower` equals `id + 1`, tested as `lower - 1 == id` only when
`lower > base`. A single-value domain (`base == upper_bound`) is well-formed: one interval,
one claimable identifier, and an empty map after that claim.

### Public construction

Fat-P adds a `SparseIdGenerator<IdType>` alias pairing sequential allocation with
`SparseRecyclingPolicy`. The policy remains usable in an explicit `IdGenerator` instantiation, but
the generator rejects allocation-policy combinations that cannot honor the full-domain invariant.

No existing alias selects the new policy. Code that uses `SimpleIdGenerator`,
`ThreadSafeIdGenerator`, `DenseIdGenerator`, or `RandomIdGenerator` receives no claim member and no
ordering change.

---

## Complexity Contract

Let `I` be the number of free intervals and `A` the number of active identifiers.

| Operation | Sparse-policy cost | Notes |
|---|---:|---|
| `generate()` policy step | O(1) amortized | Peek then remove the first interval's low value |
| `claim()` | O(log I) | At most one interval split; reserves one credit, two on a split |
| `release()` | O(log I) | Merge or singleton insertion; allocation-free |
| Batch rollback | O(k log I) | Allocation-free; consumes the batch's credits |
| `recycled_count()` | O(I) | Checked, saturating cardinality sum |
| `reset()` | O(I + A) | Rebuilds one interval by node reuse; clears the active set |
| `is_active()` | O(1) average | Existing `ActiveIdTracker` hash lookup |
| Storage | O(I + A) | Interval map, active tracker, and one reserved node per active id |

No operation performs work proportional to the numeric identifier value or the gap between active
identifiers.

---

## Required Tests

The Fat-P slice must add red-first tests for:

1. Claiming a fresh base identifier and a fresh maximum identifier.
2. Claiming a sparse high identifier without consuming lower identifiers.
3. Claiming `{0, 5, 60000}` in ascending, descending, and mixed order.
4. Duplicate claim returning `AlreadyInUse` without changing counts or issuance order.
5. Release-then-claim of the same identifier, including an identifier already in a free interval.
6. Releasing a claimed identifier exactly once and rejecting the duplicate release.
7. Releasing the middle of a multi-value active run, exercising singleton insertion.
8. Generation after sparse claims returning the lowest remaining free identifier.
9. `generate_batch()` excluding every claim and restoring exact policy state after rollback.
10. Exhaustion and maximum-bound behavior with a small unsigned type.
11. `reset()` restoring the configured base and making every prior claim inactive.
12. Exact and saturated `recycled_count()` results, including the full-width domain.
13. The same claim/release contract under `MutexSynchronizationPolicy`.
14. Regression sequences proving every existing alias retains its current ordering and errors.
15. `InvalidClaim` for an identifier below `base` and for one above `upper_bound`, proving the
    domain test is distinct from `AlreadyInUse` and that neither mutates the generator.
16. All four release/merge transitions individually — merge-both, extend-left, extend-right,
    and singleton insertion — each asserted on the resulting interval **count**, so a merge
    that leaves two adjacent intervals where one belongs is caught. Canonicalization is a
    stated invariant (§Representation, invariant 2) and needs its own oracle.
17. Configuration: a generator constructed with a non-zero base issues that base first, and
    `reset()` on a generator with sparse claims restores `[base, upper_bound]` — asserted by
    generating the base again, not only by `is_active()` being false, so a `clear()`-only
    reset that degrades to sequential allocation fails the test.
18. Gap independence, instrumented: claiming a high identifier performs a number of
    comparisons bounded by interval count, not by numeric distance. A counting comparator or
    operation counter is required; without it nothing distinguishes `claim()` from the
    `O(gap)` walk this note exists to replace.
19. Batch rollback with its exhaustion setup stated explicitly, asserting that the allocation
    policy's `revert()` is **not** called — observable through an instrumented allocation
    policy carrying a static call counter — and that the interval set and active set are
    byte-identical to their pre-batch state.
20. Exception injection at every allocating step of `generate()`, `claim()`, and
    `generate_batch()`, proving the pre-call state is restored exactly. A throwing allocator
    is the hook; the exception-safety claims in this note are otherwise untested assertions.
21. `IdGuard` and move-assignment across a sparse release, proving neither terminates.
22. Credit accounting across every path, asserted at operation boundaries: `C == A` after
    generate, claim with and without split, all four release transitions, batch rollback,
    and reset. A throwing active-set insert must leave `C == A`, not `C == A + 1` — the
    unwind rule has no other oracle.
23. Movability is asserted, not assumed. `static_assert` the move traits for every shipped
    alias (expected: NOT movable, because `SingleThreadedPolicy` deletes its copy operations
    and suppresses its moves) AND for an instantiation over `UniqueRWLockPolicy` (expected:
    movable). Assert an actual move EXPRESSION for the movable one, not only the trait —
    traits do not instantiate the bodies that fail. `reset()` on that moved-from generator is
    the one state where the rebuild allocates; it is not otherwise a supported call, because
    the moved-from lock policy holds a null mutex.
24. Storage does not grow monotonically: after activating and releasing a large set, the
    policy's reserved-node footprint returns to `O(A)`, not the high-water mark.
25. A `requires` expression rejects `SparseRecyclingPolicy` paired with
    `RandomAllocationPolicy`, and accepts the sequential pairing.
26. Constructor routing is proved for each policy-role combination: allocation-only opt-in,
    recycling-only opt-in, both roles opted in, and neither role opted in. Each participating
    policy observes the same effective bound; the two-argument constructor is unavailable when
    neither role opts in, and `RandomAllocationPolicy` is never selected by accidental arity.
27. With `using Id = StrongId<uint8_t, Tag>`, a default-constructed sparse generator contains and
    issues exactly `0..254`: `recycled_count()` starts at 255, reaches zero after those values are
    active, and the next `generate()` reports `Overflow`; `claim(Id::invalid())` returns
    `InvalidClaim`, and reset does not reintroduce `255`. Repeat with an explicit requested upper
    bound of `255` to prove normalization, and with a plain `uint8_t` generator to prove that
    `255` remains valid when the ID type declares no sentinel.

The Loom integration slice must then prove that a lone sparse persisted child loads without a
generate walk, unclaimed lower slots remain available, an already-active claim is refused without
partial allocator mutation, and full child-slot exhaustion returns an empty result rather than an
occupied slot-zero fallback.

That slice touches these consumer symbols by name, and this note authorizes no others:

- `LoomIdAllocator::allocateAt` — its bounded generate-and-release walk (512 attempts) is
  replaced by a single `claim()`.
- **The `std::terminate()` that walk falls into.** Today it fires when the walk exhausts its
  attempt ceiling. If the walk is simply replaced, the same branch fires when `claim()` is
  refused — which converts a corrupt or out-of-domain persisted index from a data problem
  into a process kill during load, and makes it the designed path rather than an accident of
  the ceiling. The integration slice must decide that branch explicitly: a refused claim is a
  refusal to surface, not a reason to terminate. Leaving the branch untouched is not an
  option this note permits.
- `firstFreeChildSlot` — returns an optional result so true exhaustion is distinguishable
  from slot zero.
- The depth-derived index ceiling — Loom passes `upper_bound` for the parent's remaining
  depth, or enforces the bound itself before calling. See §Domain configuration.

---

## Decision Rationale

Option D is the only evaluated mechanism that satisfies both owner-selected properties at once:
claim cost is independent of numeric distance, and lower unclaimed identifiers remain issuable.
It also fits the existing policy dispatch because `generate()` already prioritizes recycled state.

Keeping the policy opt-in preserves the observable order and cost model of current Fat-P users.
Making claim availability a compile-time property prevents a generator from advertising an
operation its recycle source cannot honor.

---

## Consequences

### Positive

- Persisted sparse identifiers can be reserved without a gap walk or fixed attempt ceiling.
- Every identifier source observes one active/free partition.
- Exhaustion is an explicit empty-domain state.
- Existing aliases retain their current successful-operation behavior.
- The representation naturally returns the lowest free identifier.
- Allocation and recycling policies share one upper-bound construction surface while opting in
  independently, and StrongId-style domains exclude the reserved invalid sentinel.

### Negative

- Sparse-policy activation allocates: `generate()` and `claim()` reserve a return credit, and
  an interior claim reserves one node more. Release, rollback, and reset then allocate nothing.
- Reserved credits cost `O(A)` nodes beyond the interval map.
- `recycled_count()` is O(I), saturates when the exact cardinality exceeds `SIZE_MAX`, and
  **means something different from the other policies**: free cardinality including
  never-issued identifiers, rather than "released and not yet reused". For the shipping
  policies a zero means nothing is pending; for this one a zero means the domain is exhausted.
  Generic code that reads the count without knowing its policy will misread it.
- The policy is appropriate for sparse reservation, not FIFO recycle-order requirements.
  Loom's current allocator uses `ImmediateRecyclingPolicy`, so adopting this policy changes
  its recycle order from FIFO to minimum-first.
- `IdGenerator` gains a compile-time full-domain branch in six places: the constructor and
  `reset()` (the configuration seam), `generate()` and `generate_batch()`'s per-element step
  (the staging order and the exhaustion test), batch rollback, and the `claim()` declaration
  itself.

### Obligations

- Implement the complete Fat-P test matrix before changing the Loom consumer.
- Keep `claim()` unavailable for policies that do not model complete sparse claiming.
- Preserve the active/free invariant across allocation exceptions: an identifier may be in
  both sets inside an operation, never in neither between operations.
- Normalize a StrongId-style requested ceiling before constructing the free domain; the invalid
  sentinel must never enter an interval, a free count, or a successful claim.
- Reserve every return credit before the first mutation of an operation, so that release,
  batch rollback, and reset remain allocation-free.
- Decide the `allocateAt` terminate branch explicitly during Loom integration; a refused
  claim must not kill the process.
- Update the IdGenerator overview and user manual with the new policy, error, count semantics, and
  exception behavior when code lands.
- Run Fat-P metadata and layer validation after modifying the header or tests.
- Integrate Loom only after the Fat-P contract and tests land.
- Change Loom's child-slot query to an optional result and test true exhaustion.

---

## Status

**Status:** Implemented in Fat-P 2026-08-14; contract ratified 2026-08-14  
**Mechanism authority:** Loom owner decisions `D-ALLOC-SPARSE-POLICY` and
`D-ALLOC-FATP-CHANGE`, 2026-07-22  
**Implementation:** Fat-P complete; Loom integration not started  

---

## Implementation notes

Written 2026-08-14, after the slice landed. The contract above is unchanged; this section
records where the implementation added to it, and the one claim that is not covered by a test.

**Additions the contract did not specify.**

- `SparseRecyclingPolicy` takes a `Compare` template parameter, defaulting to
  `std::less<IdType>`, as every ordered container does. Required test 18 asks for a counting
  comparator, and there was no way to supply one.
- `detail::is_sequential_policy_v` recognizes an opt-in `using is_sequential_policy = void;`
  alias in addition to the two shipped specializations. Without it the pairing check was a
  whitelist of two types, no user could pair a custom sequential policy with the sparse
  policy, and required test 19 — which needs an instrumented allocation policy — was
  unwritable.
- `free_interval_count()` and `reserved_credit_count()` are exposed on the generator, gated on
  the full-domain trait. Canonicalization (invariant 2) and the `C == A` relation are stated
  invariants with no other oracle: every other query returns the same answer whether or not
  adjacent intervals were merged.
- `generate()` constructs the caller's `id_type` *before* it mutates anything, which the
  staged order in §Generation did not require. A throwing `id_type` constructor would
  otherwise leave the identifier active and out of the caller's hands. `generate_batch()`
  already had this ordering; `generate()` did not.
- The batch capacity preflight now measures against the configured `upper_bound` rather than
  the ID type's maximum, which it must for a bounded or full-domain generator.

**Verification.** 27 new tests (`test_IdGenerator` 48 → 75), then a 12-mutant gate. A second,
adversarial coverage audit followed — six independent lenses over implementation-versus-suite,
each finding put through a refutation round — and it was worth running: it surfaced **four
defects in the landed code**, all reproduced by direct execution before being believed, plus
six mutations the 75-test suite did not kill. Both are recorded below. The suite is now 84
tests and a 12-mutant gate in which every mutant dies.

**Defects found after landing, and fixed.**

- **The batch capacity preflight was gated on the ID type's width, not the configured
  domain's.** `sizeof(underlying_type) < sizeof(size_t)` compiled the guard out for every
  64-bit generator, *including* a sparse or bounded one whose domain holds ten identifiers, so
  `generate_batch(SIZE_MAX)` reached `reserve()` and threw `std::length_error` out of an
  `Expected`-returning API — the exact outcome the guard's own comment claims to prevent. That
  is precisely the first consumer's shape: `size_t`-wide indices with a depth-derived ceiling.
  The test that should have caught it deliberately reached for `uint64_t` *so the preflight
  would be compiled out*. Now measured against `mUpperBound - mBaseId`.
  - Consequence worth stating: the preflight is now **exact** for any domain narrower than
    `SIZE_MAX + 1`, so domain exhaustion is always refused before the loop and the batch's
    mid-loop `Overflow` arm is defensive. Ordinary rollback is reached by an injected
    mid-batch reservation failure instead, which is a better test anyway — it separates "the
    allocator ran out" from "the domain ran out".
- **A moved-from `SparseRecyclingPolicy` kept `mCreditCount` while losing the credits.** The
  implicit move transfers the containers and *copies* the scalar, so the source reported
  credits it did not hold; `configure_domain`'s reuse arm keys off exactly that count and
  called `take_credit()` on an empty list. Move operations are now user-declared and zero the
  source — which also makes this note's stated basis for `reset()`'s conditional `noexcept`
  true, since the moved-from state now really does take the allocating arm.
- **A sparse StrongId generator could issue `invalid()`.** `SparseIdGenerator<Id>(255)` over a
  `StrongId<uint8_t>` normalizes the ceiling to 254, below the base; `configure_domain` built
  an inverted interval and `generate()` handed out the sentinel. `base > upper` remains a
  precondition violation, but it now yields an **empty** domain, matching what the non-sparse
  path does in the same situation. This mattered because the full-domain path has no late
  `generate()` sentinel guard to fall back on — domain construction is the only mechanism, and
  its only check disappears under `NDEBUG`.
- **`Compare` was documented as a general ordering seam, and is not one.** Five sites mix map
  order with raw arithmetic. `std::greater` compiled and was silently wrong: `claim()` of a
  wholly free identifier returned `InvalidClaim`, and generation ran descending. Resolved by
  **narrowing, not genericising** — but the first attempt narrowed it only in prose, which was
  not enough (see below).

**On surface added for testability.** Three of the additions above the ratified contract exist
so a required test could be written, and they are not equivalent in cost. The distinction that
matters is whether the addition lets a caller reach a state the type could not previously
reach:

- **`Compare` did**, and that is a contract violation, not a coupling cost. Before the
  parameter, no caller could construct a `SparseRecyclingPolicy` that violated §Representation's
  four invariants. After it, `SparseRecyclingPolicy<T, std::greater<T>>` compiled and falsified
  all four. Documenting the requirement and asserting the direction at runtime left the hole
  open — an instrument that can introduce a defect is not worth its measurement. The parameter
  is now constrained by `detail::is_ascending_order_v`: `std::less` and any comparator declaring
  `using ascending_order = void;` are accepted, and everything else is a **compile error**.
  Verified both ways — `std::greater` fails to compile; `std::less` and an opted-in counting
  comparator still work, so required test 18 keeps its instrument.
- **`free_interval_count()` / `reserved_credit_count()` did not.** They are read-only and cannot
  produce an invalid state. Their cost is real but different: they publish the representation, so
  changing the interval structure later breaks them and much of the suite with them. They are
  kept because invariant 2 (canonicalization) and the `C == A` relation have no other oracle —
  every other query returns the same answer whether or not adjacent intervals were merged.
- **`is_sequential_policy`'s opt-in alias did not either**, though it weakens a gate: a policy
  can now declare itself sequential without being so. That gate turns out to enforce nothing
  (see the `next_id()` note above), so the correct repair is to this note's §Exhaustion, which
  overstates it, rather than to the code. The door is independently justified: without it no
  user could pair a custom sequential policy with this one at all.

The allocation/recycling pairing remains structural hygiene rather than a runtime behavioural
guard: `next_id()` is never called on the full-domain path and `revert()` is suppressed there.
It is nevertheless part of the admitted public syntax, so the class now carries a `requires`
constraint rather than a body-level `static_assert`; ordinary compile-time tests can prove an
invalid pair is absent. `is_sequential_policy`'s specialization set still becomes ambiguous the
moment either shipped policy also declares the opt-in alias — a tempting symmetry edit, since
`BoundedSequentialAllocationPolicy` already declares `accepts_upper_bound`. There is a comment
at the specializations saying so.

**Mutants the 75-test suite did not kill, all now killed.** Claiming a *singleton* free
interval (`claim_at`'s erase arm was dead — regressing it to a front trim yields an inverted
entry, after which `generate()` hands out an already-active identifier); releasing the base of
a *full-width* domain (the `id > mBase` guard is load-bearing only when the wrapped `id - 1`
lands on a live key, and every test released inside a narrowed domain, where deleting the
guard **erases the entire domain**); `release_batch` on a sparse generator (zero coverage — it
is a separate copy of `release()`'s erase-first ordering); `generate()`'s id_type construction
order; `recycled_count()`'s saturation threshold; and `claim()` taking no lock at all.

**Allocation is now measured, which the credit mechanism had no witness for.** Every prior
oracle was a *count*, and no count distinguishes a node taken from the credit stack from one
freshly allocated — both decrement the count and leave identical structure. The suite now
replaces global `operator new` (armed only inside two tests) and asserts **zero** allocations
across `release()`, `release_batch()`, and both reset paths, having first shown the counter
live on the activation path. The same instrument closes the split-reservation unwind, whose
`catch` block had never executed.

**No longer un-injectable.** The earlier note that a throwing *active-set* insert could not be
tested was true only of the policy-level seam. With `operator new` replaced, allocation
failure can be injected at any index, `ActiveIdTracker`'s insert included.

**Concurrency.** `claim()` is exercised under real contention over `UniqueRWLockPolicy` — the
only shipped policy whose `lock_shared()` is a genuine `std::shared_lock`; `MutexSynchronization
Policy` aliases its shared guard to its exclusive one, so a shared/exclusive swap is invisible
there. Four writers claim interleaved stripes so every claim contends on a node a neighbour is
splitting, with two readers holding the shared lock throughout. Deleting `claim()`'s lock now
crashes with an access violation rather than passing.

**The one surviving mutant, recorded rather than hidden.** Deleting the `is_in_domain()` check
from `claim()` passes the entire suite. It is a provably equivalent mutation: the free set is a
subset of `[base, upper_bound]` by invariant 1, so `is_free()` already rejects every
out-of-domain identifier with the same `InvalidClaim`. No test can distinguish them, and
required test 15 does not, despite appearing to. The check stays — it is `O(1)` ahead of an
`O(log I)` lookup, it names which question is being asked, and it is the one of the two that
survives a violation of invariant 1 — but it is enforced by argument, not by the suite.

**Documentation used as enforcement — a second sweep, and five more sites.** The `Compare`
lesson generalized. A focused audit asked, of every `assert`, `@pre` and directive comment in
the header: *could the type have refused instead?* Five sites said yes. Three were confirmed by
running them, and every one was silent — no assert fires, in any build:

```
owner holds 1;  { IdGuard foreign(gen, *owner); }  ->  SAME IDENTIFIER ISSUED TWICE
policy.reserve_credit()                            ->  credits=1, actives=0, no diagnostic
guard + generator move                             ->  identifier stranded active in the destination
```

- **`IdGuard`'s adopting constructor was public**, so a caller could mint a guard over an
  identifier they never acquired; the destructor then released it out from under its owner. The
  guard's own asserts do not catch that shape *in any build*, because the release SUCCEEDS —
  they catch only the never-active and double-release shapes, which are the harmless half. Now
  private with `friend IdGenerator`, plus `scoped_claim()` to keep the legitimate use.
- **The policy's whole mutating protocol was public** under a `@warning` reading "only usable
  through a generator that configures it" — a sentence doing an access specifier's job. Now
  `protected`, which costs no coverage: a derived class still reaches it, which is the access a
  test is entitled to.
- **Guards and a movable generator** are incompatible: a guard holds a raw pointer to the source,
  so moving the generator strands that authority at the wrong object. The factories carry a
  `requires` constraint, while the private adopting constructor retains a diagnostic assertion.
- **`RandomAllocationPolicy(uint64_t seed, int ignored)`** meant `p(1000, 5000)` — read as
  `(base, upper_bound)` — compiled clean and discarded the base, reinstating the defect the
  one-argument constructor was fixed to remove. It now takes the `seed_tag_t` the generator
  already presented on its own seeded constructor.
- **`configure_domain`'s asserts sat above the early-out**, so the function aborted on the one
  input the previous fix taught it to handle. Every sparse sentinel test constructs that
  generator, so **the suite could not run with asserts enabled at all** — it was green only
  because the gate builds `NDEBUG`. Both asserts moved below the branch, the `@pre` became a
  `@note`, and the gate now runs Debug as well as Release: 85/85 in both.

**Verification, including what it got wrong.** Seven mutants over the five narrowings. Two
early results were my error, not the code's: a mutant flipped a different access label than the
assertions covered, and one "kill" came from a test call site rather than the `static_assert` I
had pointed at. The first was a real gap — the protocol sits under three labels and only two
were asserted — and is closed with one assertion per label, each now killed by its own
diagnostic.

**Compile-time absence is now covered without a compile-fail fixture.** The policy-pair and
guard-factory rules are `requires` constraints, as are the comparator contract and
`generate_batch()`'s nothrow-move requirement. Ordinary `requires` expressions assert both the
accepted and rejected forms, so a hard-error `try_compile` harness is no longer needed for these
contracts.

**The pattern, stated so it transfers.** All five sites, and the `Compare` defect before them,
are *protocol* constraints — how a thing must be used, in what order, by whom — rather than
*type* constraints. Type constraints go into the type system here by reflex; protocol
constraints get narrated. The tell is a comment telling a caller what not to do. Loom's
`allocateAt` terminate branch is the same shape.

**Review record.** The first draft was reviewed against Fat-P `5366366d` and Loom `20c80eb`
by a six-lens pass that derived required behavior from live source before reading the draft,
with each finding put through an independent refutation round, and then cross-validated with
the draft's author through the bounded peer bridge. Nineteen defects reduced to five root
causes; the author refuted one of them and corrected four supporting claims. The revisions
that resulted:

- **Domain configuration seam** (§Domain configuration). The policy could not learn `base`:
  the generator default-constructs the recycling policy, and `reset()` reaches it only
  through `clear()`. Three stated invariants and one required test depended on a hook that
  did not exist.
- **Return credits** (§Return credits, §Exception specifications). An earlier draft made
  release allocate and proposed conditional `noexcept` to cover it. That neither helped —
  `~IdGuard`, `IdGuard::operator=(IdGuard&&)`, and Loom's own `release` are all `noexcept` —
  nor was it needed once activation reserves the node its release will require. An
  intermediate proposal to recycle exhausted nodes into a reservoir was rejected on the
  author's counterexample: taking an interval's head frees no node, so the reservoir is
  empty exactly when the first singleton release needs it.
- **Generate ordering** (§Generation). The claim that `generate()` needed no change was
  wrong: the shipped body removes from the free set before the allocating active-set insert,
  which can strand an identifier in neither set.
- **Configurable upper bound** (§Domain configuration). Loom's index space is bounded by
  `BoneId` depth at `2^48` for a depth-13 parent, far below a `size_t` domain; a fixed `MAX`
  would let the generator issue indices the consumer cannot represent.
- **`recycled_count()` per-interval arithmetic** (§`recycled_count()`). The saturating sum
  was specified but the per-interval term wraps at full width. Recorded as a clarification,
  not a defect — the note's normative text already forbade the wrong result.
- **Named consumer symbols** (§Required Tests) and eleven further test rows (15-25),
  including the instrumented gap-independence proof without which nothing distinguishes this
  design from the walk it replaces.

**Second review round (2026-08-14).** The revision above was itself checked — three passes
over the rewritten note, then a scoped peer round on the four claims that were new design
rather than corrections. Both found the same central error, independently: the argument that
`reset()` could stay unconditionally `noexcept` rested on an exhaustiveness proof that
omitted moved-from state. That is the second short exhaustiveness proof in this arc to fail
on an unexamined case; the first was the node reservoir.

**Third round (2026-08-14) — the correction was itself wrong, and this is the one to
remember.** A deep review of the live component established by COMPILER PROBE that no shipped
`IdGenerator` alias is movable at all: `SingleThreadedPolicy` deletes its copy operations
(`ConcurrencyPolicies.h:293-295`), suppressing its implicit moves, so
`IdGenerator(IdGenerator&&) noexcept = default` (`IdGenerator.h:804`) is defined as DELETED.
Both the second-round reviewer and the peer had read that declaration and believed it; a
ten-line `is_move_constructible_v` probe falsified both. The obvious repair — declare the
moved-from case unreachable and restore an unconditional `noexcept` — was **also wrong**:
the peer's own probe showed the generic template IS movable over `UniqueRWLockPolicy`, which
this note explicitly permits. The conditional `noexcept` therefore stands, on the corrected
justification now in §Reset and destruction, and the note's claim that a moved-from generator
supports every operation is withdrawn as false (that policy's moved-from source holds a null
mutex).

**Rule this produced:** a claim about a DECLARED language-level property — movable,
`noexcept`, trivially copyable, convertible — is settled by compiling it, never by reading
the declaration. `= default` is a request that silently becomes `= delete`. And a refutation
round must change the EVIDENCE CLASS — a compiler probe, a throwing allocator, a throwing
lock, a sanitizer — rather than adding a second reader who consults the same declaration.
Two independent reviewers agreeing raised confidence in the reasoning and did nothing about
the shared unexamined premise beneath it. Also corrected in that round: the credit invariant is
`C == A` at operation boundaries, not at every instant, and the unwind rule that keeps it
true was unstated; the credit stack must release capacity or the storage bound is `O(max A)`
rather than `O(A)`; `remove(IdType)` was split into `remove_lowest()` and `claim_at()`
because the general form implied a split the generate path never performs; and a trait on the
recycling policy cannot constrain the allocation policy it is paired with, so the
random-allocation rejection needed its own constraint on the pair.

**Fourth round (2026-08-14) — reconciliation with the shipped allocator fixes.** Review against
Fat-P `adf89510` found three stale or incomplete contract statements. First, batch provenance is
now recorded for every policy by `from_pool`; the sparse policy does not need a separate
value-inference rule, only its allocation-free credit-funded return path. Second, the shipped
upper-bound constructor is an allocation-policy opt-in, so the sparse domain now joins that
public surface through an independent recycling-policy opt-in and role-directed forwarding.
An inline C++20 compiler probe covered allocation-only, recycling-only, dual-opt-in, and
neither-opt-in combinations with non-movable policy bases; the tag-dispatched initializer shape
compiled and the accidental two-argument arity match remained unavailable.
Third, the default interval could include `StrongId::invalid()`. The effective domain now clamps
a StrongId-style requested ceiling to the largest valid value, and required tests prove that
generation, claiming, free counts, reset, and exhaustion all agree on the same issuable set.

**Fifth round (2026-08-14) — activation identity and transactional issuance.** The remaining
guard counterexample was real: `scoped_claim(7)`, direct `release(7)`, a second `claim(7)`, then
destruction of the first guard released the second owner's activation. A reset-only epoch could
not distinguish two lifetimes of the same raw value. Active entries now carry an optional shared
activation token. `scoped_id()` and `scoped_claim()` allocate the token before mutation, then
issue and attach it under one lock; destruction releases only on an exact ID-and-token match.
Reset, direct release, and release-and-reissue all erase or replace that match, making stale
guards inert without global generations or wraparound arithmetic.

The same round closed non-sparse exception seams that the sparse implementation had made easier
to see. A failed active-tracker insertion now restores a recycled candidate or rewinds a fresh
sequential allocation. A fresh batch candidate that throws before entering the result vector is
also rewound, and accumulated recycled candidates are restored in reverse removal order so a
FIFO policy regains its exact pre-call sequence. The recycling-policy protocol therefore includes
`restore_recycled(id) noexcept`, distinct from ordinary release-to-the-back semantics.

Four prose/body-level restrictions moved onto the public syntax: sparse comparators, full-domain
policy pairs, guard factories, and batch movement. The comparator constraint includes nothrow
swap because sparse policy move-assignment uses container swap under `noexcept`.
`generate_batch()` is absent for an ID type with a throwing move. Compile-time tests exercise both
accepted and rejected forms with `requires` expressions. Runtime regressions inject allocation
failure at active-tracker insertion, throw the current fresh and recycled batch candidate, verify
exact FIFO recovery, and destroy a stale guard after both ordinary and sparse reissue. The focused
suite is 90/90 in assert-enabled Debug; the test entry point suppresses MSVC's modal abort/report
dialogs so failures remain suitable for unattended gates.
