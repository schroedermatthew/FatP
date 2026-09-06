# AI Peer Bridge Guideline

**Authority:** [CORE](../CORE.md). Project facts belong in [PROJECT_PROFILE](../PROJECT_PROFILE.md).

This guideline binds whenever an agent working on this project asks another model to investigate, review, or challenge work through a configured peer bridge. It governs authority, evidence, and task ownership. The bridge mechanism's own README (wherever installed) describes *how* it works; this document is the authority for *how it may be used* on the project.

A peer bridge connects new managed peer sessions between models. It does not transfer the private memory of already-open chat windows and does not turn any model into an autonomous maintainer.

---

## 1. Authority and roles

- The human maintainer remains the final authority.
- Exactly one model is the **task owner**. It integrates evidence and communicates the result to the maintainer.
- The invoked model is a **peer**: a read-only reviewer, investigator, or challenger. Peer output is advisory input, never a ruling.
- Exactly one model may write in a worktree. A peer must not edit, create, delete, move, commit, push, merge, publish, or perform external writes.
- Commit, push, merge, deletion, publication, credentials, and scope expansion remain
  governed by actual user/platform authority; the bridge cannot grant it.

The approved bridge uses **mechanically read-only wrappers**. Do not replace them with a raw model endpoint whose permissions depend only on prompt wording — read-only behavior that depends on the caller remembering to request it is not a boundary. Before relying on the bridge, verify that the exposed tools are the bounded tools named in §6.

## 2. No recursive delegation

A peer must not call the task owner, another model, model-invoking tools, or its own subagents. One task-owner call produces one peer turn; a necessary clarification may use the returned session identifier. Multiple peers may review the same work sequentially, but each receives an independent request and must not see another peer's conclusions before its own analysis. Model-to-model loops and concurrent peer chains are prohibited even when every individual call is read-only.

This is both a cost boundary and an authority boundary. Without it, two models can amplify an unsupported premise, consume unbounded usage, and obscure which model owns the result.

## 3. Independent review before cross-validation

1. **Round 1 (independent):** give the peer the objective, live source coordinates, and evidence scope **without giving it another model's conclusions to endorse**. Let the peer analyze the live referent independently. Anchoring a peer on prior conclusions converts a second opinion into an echo.
2. **Round 2 (cross-validation):** the task owner then compares the analyses.
3. Resolve conflicts by source evidence and counterexample, not by vote or model prestige.
4. Apply settled intent and integrate confirmed repairs within the task owner's existing authority. Escalate unresolved design intent, conflicts requiring a policy choice, and mutations beyond that authority to the human maintainer. The peer remains read-only.

Independent opinion and context reset are distinct. When the task requires a
fresh review, use [Fresh-context review](FRESH_CONTEXT_REVIEW.md) to exclude prior
verdicts and incidental histories while retaining durable constraints and rationale.

**Agreement between two models is not proof.** A peer claim with no current source evidence is a hypothesis and must not become a patch or a completion claim.

## 4. Peer request envelope

A peer request must state:

1. The concrete question or review objective.
2. The repository working directory.
3. The branch, commit, and dirty-worktree facts relevant to the request.
4. The files, symbols, behavior, or decision in scope.
5. The evidence and counterexample standard expected in the response.
6. The requested response shape: findings, alternatives, or a bounded answer.

Keep requests **bounded and tightly scoped** — one question or one review objective per call, sized to fit the peer's time budget (see §7). Coordinates are perishable: the peer must re-measure the live tree before citing it and must re-read cited lines before returning a finding if another agent may be editing the worktree. A prompt's branch, status, test count, or "already verified" statement is context to check, not permission to skip verification.

Do not put credentials, tokens, private keys, or unrelated personal data in a peer prompt.

## 5. Integrating the response

The task owner must:

- preserve the peer's provenance when reporting a finding;
- verify cited code against the live tree before acting on it;
- distinguish what the peer inspected from what it inferred;
- state which commands were actually run and by which session;
- discard unevidenced claims rather than averaging them into the answer; and
- keep peer requests read-only; handle changes under the existing user authorization
  and obtain new direction only for a real scope or authority expansion.

Peer output does not silently broaden the user's request. A reviewer finding an adjacent problem may report it, but neither model may implement that adjacent work without authority.

## 6. Approved tool surface

Actual tool registrations live only in PROJECT_PROFILE.peer_routes. No route is
pre-installed or authorized by this document. Verify the live tool surface,
read-only boundary, timeout and current user/platform permission before use.

Requirements for any approved route:

- The new-turn wrapper enforces read-only execution, hides or denies every other model-invoking tool from the peer, bounds prompt/output size, and imposes a timeout.
- Where the OS sandbox is not kernel-enforced on the host platform, the tool allowlist is the primary write boundary — verify it, don't assume it.
- Asynchronous routes: retain the start call's job ID, poll until a terminal state, and integrate only a `succeeded` result. Non-terminal states are not results. A terminal job's returned session ID is the only authority for a necessary follow-up.
- Any documented behavior of the bridge is dated status: **verify the live registration rather than inheriting it.** Registration and authentication never substitute for an end-to-end probe — a route can register cleanly and still mis-parse the peer's live protocol.

If the bounded tool is missing, fails authentication, or exposes a different surface, stop using the bridge. A manual, clearly attributed handoff is the fallback; weakening the permission boundary is not.

Each peer invocation consumes model usage. Use one initial call and only the follow-up necessary to resolve an identified ambiguity.

## 7. Timeouts and asynchrony (general lessons)

- **Long peer calls need an async job-and-poll pattern.** A synchronous MCP-style call that holds the request open for the whole peer turn will hit transport or watchdog timeouts on real review workloads; turns routinely approach or exceed fixed cutoffs, and prompt length does not predict which ones. The correct shape: start returns a job ID, status polling collects the result, cancellation is explicit, and the worker has a separate bounded lifetime.
- **Scope prompts to fit the peer's time budget.** If a route has a hard turn timeout, split the review into questions the peer can answer inside it rather than asking one omnibus question and losing the whole turn at the cutoff.
- **Do not start a second job in an endpoint until the prior process has exited**; overlapping jobs corrupt attribution and can wedge the worker.
- **Peer session lifetime is tied to the wrapper process** where the bridge is process-hosted: a "continue" handle can die with the process. Do not plan multi-day continuations on a handle you cannot verify is alive.

## 8. Audit and privacy

The bridge audit may record timestamps, providers, working directories, prompt lengths, prompt hashes, job and session identifiers, exit status, and duration. It must not record full prompts, model responses, or secrets by default. Complete responses may live transiently in an asynchronous server's in-memory job table so the task owner can collect them; they are not audit content.

Audit metadata proves that a call occurred; it does not prove the response was correct. Source evidence and the task's own verification gate remain required.

---

## Learning from peer failures

Record confirmed violations in the compact [DEMERITS](../DEMERITS.md) tally.
Routine events need no log. Preserve route/session details in a separate case only
when they explain a load-bearing failure mechanism; use Governance's learning
procedure. Inherited rules above are not claims of local incidents.
