# Handoff Style Guide — documents that cross a context boundary

**Authority:** [CORE](../CORE.md). Project facts belong in [PROJECT_PROFILE](../PROJECT_PROFILE.md).

A **handoff** carries a task across a context boundary to a fresh AI with no memory
of the producing session and a finite context budget. This module applies to session
handoffs, execution plans, and cross-session task status. A durable guideline or
philosophy record is not a handoff merely because a future AI reads it. Task
coordinates and sequencing below apply to actual handoffs; [Documentation](DOCUMENTATION.md)
and [Governance](GOVERNANCE.md) govern operational rules and their useful rationale.

---

## 1. Write for the amnesiac under a budget

The reader loads this cold, remembers nothing, and every token it spends here is a token it can't spend on the work. Optimize for it loading the *right constraints fast*, not for a human reading comfortably. AI-loadability, not prose polish, is the target.

## 2. Route — do not restate

The handoff coordinates; it does **not** copy the design note, the guidelines, or the version-control log into itself. Point to the one doc the reader needs for the task it is about to start — not all of them. Duplicated content drifts from its source and eats the budget. **A handoff that restates is a handoff that will lie the moment its source changes.** The right shape is a thin router (often only a few dozen lines) over canonical documents.

## 3. One canonical home; the handoff is not the authority

Every fact lives in exactly **one** authoritative place — design note, guideline, plan, or version control — and the handoff links to it. Where the handoff and a canonical source disagree, the **source wins**, and the handoff must say so out loud. A handoff that becomes a second source of truth guarantees divergence. Private per-agent memory is staging, not the home.

## 4. Status is perishable — date it and defer it; never freeze "done" as if current

The single most dangerous line in any handoff is a frozen verdict: *"X DONE, green, verified, Next: Y."* Reloaded cold, it reads as **current** when it is a dated snapshot nothing re-ran. Every status claim must carry (a) the **commit/date** it was true, and (b) a **pointer to the live source** to re-verify against — and it must never substitute for the check. Write *"implemented as of `[commit]`; green ≠ conformant — re-verify against `[source]` before building on it,"* not *"done ✓."* A frozen status can point a fresh context straight past required work: tests can be green while the implementation is not yet conformant to the design. **Green ≠ current.** Mark every number as expected vs measured.

## 5. Instruct the reader to distrust "already checked that" — including yours

Tell the fresh instance explicitly: treat any *done / green / verified* in this handoff **or in private memory** as a dated assertion to re-verify against the code/tests/referent, never as license to skip the check. A model reports its own status off stale snapshots; the honest move is to look at the referent, not the frozen claim. The handoff that says "trust the design note over me where we conflict" is the honest one.

## 6. Carry the burn, not just the rule

Carry the failure distinction and the rationalization the next AI must avoid.
Attach a concrete failure, cost, or case link when those details change its decision;
do not append a story for every mistake. Promote durable judgment into its owning
rule and route to it. Unresolved defects stay visible as work, not buried in history.

## 7. Sequence the work; designate what goes first (and where)

State the task order explicitly, name which task is **first** and why, and flag any task that needs a **genuinely fresh context** rather than a long, loaded one. A fresh instance should not have to reconstruct priorities. Some tasks structurally require a clean context — e.g. a document restructure whose whole premise is fresh-loading cannot be validated by the session that wrote it.

For that task, use [Fresh-context review](FRESH_CONTEXT_REVIEW.md) to define the
input boundary. Do not include prior correctness verdicts in a fresh review packet.

## 8. Demote and mark superseded inputs

When several handoffs, reviews, or plans exist, name **which supersedes which**, and mark the archived ones so they cannot be mistaken for live authority. Conflicting inputs of equal apparent standing are a trap — an archived review may contain a false "correction" that, treated as authoritative, propagates the error. No external AI review is authoritative by default; verify it against the referent.

## 9. Coordinates up front

Open with the operational facts the reader needs *before touching anything*: branch, HEAD commit, pushed / **never-pushed** status, untracked files whose ownership must be confirmed before committing, and the build command/preset. A branch that must never be pushed, or an untracked file of uncertain ownership, must be visible immediately — not discovered by accident after acting.

## 10. Canonical home in the repo, not private memory

The handoff and the docs it routes to live **in the repo**, where a fresh instance working there will find them — not in private AI memory, which is unshared and carries stale state that no other agent (or the human) can see or correct. The repo is the shared, correctable home.

## 11. Record provenance

Note what the handoff was synthesized or updated from, and when, so its lineage is auditable and a reader knows which prior inputs it already subsumes. Without provenance, a reader cannot tell what is already folded in and will re-litigate settled inputs.

---

## Done when

A genuinely fresh instance can load the handoff and, without reading everything, know: the coordinates, the task order and what is first, the **one** doc to open for that first task, which claims to re-verify rather than trust, and which inputs are archived. Short is fine — break it up (thin router + promoted canonical content) rather than pad.

## Handoff document skeleton

```markdown
# SESSION_HANDOFF_[date]

## Coordinates (dated snapshot — re-measure before acting)
Branch: ... | HEAD: ... | pushed: yes/NEVER | untracked needing ownership check: ...
Build: [command/preset]

## Task order
1. <first task> — why first; fresh-context required? yes/no
   → open: <the ONE canonical doc for it>
2. ...

## Verify, don't inherit
- "<claim>" is as of <commit/date>; re-verify against <source>.

## Superseded inputs
- <doc> — ARCHIVED, superseded by <doc>; do not treat as live authority.

## Provenance
Synthesized from <inputs>, <date>; folds in <items>.
```

---

## Appendix — Kick-off prompt template (session-start handoff)

*(The example below is **synthetic and illustrative** — a generic shape, not a record of any real session.)*

```text
You're starting fresh on the project (`[repo path]`; paths below are repo-root-relative).
Your job is Task 1: [task]. Do not touch anything until you've onboarded and run the
baseline pass.

Onboard first, in order:
1. Follow the onboarding protocol in `[guidelines path]/CORE.md`.
2. HANDOFFS.md — it governs exactly this kind of work (route don't
   restate; one canonical home; carry the burn; status is perishable).

Authoritative plan — follow it exactly: `[plan path]`. It supersedes the archived
review handoffs beside it. Execute its passes as written; do not combine them; do not drop
any item without a disposition-ledger row.

Continuity context (router, not authority): the current `[SESSION_HANDOFF path]`.

Coordinates: branch `[branch]`, HEAD `[sha]`, never pushed. The files
`[untracked list]` are untracked — confirm ownership before committing them.
Follow the actual commit/push authorization; a handoff grants none.

Verify, don't inherit:
* Treat any "done / green / verified" in memory or in this prompt as a dated snapshot to
  re-verify against the repo — the repo wins.
* Re-measure any counts/sizes yourself in the baseline pass; don't trust recorded numbers.

First action: onboard, run the baseline pass (git state, counts, cross-link inventory,
ownership of untracked files), and summarize the applicable constraints; continue work already authorized.
Ask only for an unresolved decision or action outside existing authority.
```

**The perishable slot — and why the gate matters.** The coordinates block (branch, HEAD, untracked list, branch-base advice) is a dated snapshot and can be wrong by the time it is read — a prompt can recommend branching off a mainline that the baseline pass then measures as stale. What the template must keep is the **gate** — onboard, run the baseline pass, re-derive the coordinates, and verify authorization before moving anything — because that gate is what catches its own prompt's errors. Never reuse a coordinates block without re-measuring it.
