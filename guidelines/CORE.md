# Core guidelines

This corpus preserves AI-authored judgment across contexts and projects. Demerits are
mandatory. C++ style is mandatory for authored C++ and its examples. Neither has
an enable switch, a project-profile waiver, or an external-style substitute.
Google C++ Style Guide is prohibited. C++20 is the minimum for C++ work.

## Authority

System/platform constraints and explicit user instructions take precedence.
Repository instructions cannot create permissions. Within this corpus: CORE,
then the module that owns the rule, then project decisions in their delegated
scope. Surface conflicts; do not silently rewrite a rule to authorize yourself.
Existing session authorization persists. Routine implementation decisions do not
require repeated approval. Investigation is allowed while a design decision is
unresolved; implementation that depends on it waits for that decision.

Each rule has one canonical home. Links route to its full statement; neither an
index nor a profile is an alternative rulebook. Code is evidence of behavior,
not proof of conformance. Private memory, reviews and handoffs are dated claims.

## Onboarding

<!-- onboarding -->
1. Read the [AI crash course](AI_GUIDELINES_CRASH_COURSE.md).
2. Read [DEMERITS](DEMERITS.md), including its compact failure mechanisms and counts.
3. Read [CORE](CORE.md).
4. Read [PROJECT_PHILOSOPHY](PROJECT_PHILOSOPHY.md), including unresolved choices.
5. Read [PROJECT_PROFILE](PROJECT_PROFILE.md) for actual project facts and commands.
<!-- /onboarding -->

Read these documents in full. Then read each task route below before its work.
A whole-document route requires the complete document; an explicitly scoped
section route requires that section through the next heading of the same or higher
level. Links to evidence are not recursive read-all instructions: inspect the
referent when the decision or claim depends on it. Cases and archives are not
onboarding. Machine inventories referenced by the profile are not bulk onboarding
reads either: inspect their relevant records when the task depends on them.
Tools still validate the full referenced data; deferred reading does not waive a
required check. Use the trigger table to exclude clearly unrelated modules; do not read
them merely to confirm that they do not apply. Inspect further when task scope is
genuinely unclear. Apply the relevant constraints and continue authorized work.

The human directs and judges; AI authors architecture, implementation, verification,
and guidelines. Apply settled precedent and make technical choices within existing
authority. Ask only for a genuinely unresolved commitment outside that authority.
Encode useful new judgment during the work; do not ask the owner to perform edits
you can make. Use Governance's selective learning procedure.

## Working contract

- **Review is not implementation.** Review produces evidenced findings and may
  propose targeted patches; it does not authorize applying them. Implementation
  produces complete artifacts within scope. Preserve unrelated work.
- **Evidence or hypothesis.** Do not invent APIs, measurements, executions,
  components, authority or consensus. Use current source and counterexamples.
- **Compile honesty.** A claim of compilation, execution or tests needs an actual
  run, command, source state, result and limits. Historical runs are labeled.
  Syntax-only is not a build; a build is not execution; a stub is not hardware.
- **Conformance is a separate check.** Trace the mechanism an invariant demands,
  not merely its successful output. Identify what remains unchecked.
- **Repair known root causes.** The full procedure and magnitude check live in
  [Review and delivery](modules/REVIEW_AND_DELIVERY.md). Do not soften a test or
  market a known mitigation as a completed repair.
- **Preserve accountability.** Self-report confirmed violations under DEMERITS.
  Repair does not erase awards. Routine demerits require no event log. No automatic resets or score reductions.
- **Preserve binding tools.** Generalization does not authorize removing demerits,
  style, rationales, evidence requirements or applicable procedures. Rule changes
  follow [Governance](modules/GOVERNANCE.md).
- **Finish authorized work honestly.** Enumerate unmet requirements and real
  blockers. Do not infer external-write, destructive-action or peer permissions.

## Trigger table

This is the only module-routing table. Every matching row applies; these are task
triggers, not optional-module selections. A project without C++ need not read C++
rules, but cannot use a switch to bypass them when C++ work begins.

| Before this work | Read |
|---|---|
| Design, implement, refactor, change state/ownership/dependencies | [Engineering](modules/ENGINEERING.md) |
| Establish/change architecture or claim architectural conformance | [Architecture](ARCHITECTURE.md) |
| Design/select/add/change tests or make verification claims | [Testing](modules/TESTING.md) |
| Run/change builds, CI, packaging, or claim gates green | [Workflow](modules/WORKFLOW.md) and [Current verification](CURRENT_VERIFICATION.md) |
| Review, investigate a defect, or deliver work | [Review and delivery](modules/REVIEW_AND_DELIVERY.md) |
| Write prose, examples, or technical promises, including operational AI guidance | [Documentation](modules/DOCUMENTATION.md) |
| Write/review an explanatory teaching document: manual, tutorial, guide, design narrative, or case study | [Teaching](modules/TEACHING.md) |
| Write/resume a handoff, plan, or cross-session status | [Handoffs](modules/HANDOFFS.md) |
| Change guidelines, profiles, tooling, or codify newly acquired judgment | [Governance](modules/GOVERNANCE.md) |
| Conduct a context-reset review or validate a change to onboarding/required reading | [Fresh-context review](modules/FRESH_CONTEXT_REVIEW.md) |
| Any C++ work | [C++ entry](cpp/README.md) and [Style](cpp/STYLE.md) |
| Design/write/review C++ code | [Authoring](cpp/AUTHORING.md) |
| Change/review C++ headers, namespaces, macros, modules or linkage | [Headers and linkage](cpp/HEADERS_AND_LINKAGE.md) |
| Write/review C++ tests or choose C++ checks | [C++ testing](cpp/TESTING.md) |
| Compile/link/package C++, configure diagnostics or sanitizers | [Build and CI](cpp/BUILD_AND_CI.md) |
| Document C++ API contracts or examples | [C++ documentation](cpp/DOCUMENTATION.md) |
| Write/review a benchmark implementation or results report | [Benchmarking](modules/BENCHMARKING.md) |
| Only cite a benchmark result or make a performance claim | [Benchmark claims and citations — section only](modules/BENCHMARKING.md#1-claims-and-citations) |
| Adopt/edit metadata or modify code covered by metadata | [Metadata](modules/METADATA.md) |
| Invoke a peer, integrate its findings or continue an exchange | [Peer bridge](modules/PEER_BRIDGE.md) |

## Project boundaries

The profile owns identity, real paths, actual toolchains, commands, supported
configurations, harness identity, release status and metadata adoption. It cannot
weaken style, demerits, C++20 or the procedures triggered above. Metadata adoption
is explicitly configured; benchmark/teaching/peer conduct is never default-off.

Before release, no compatibility shims: renames and their callers/docs/tests
change coherently. Released API/ABI/data-format obligations must be recorded
before breaking them; an unknown release status is not permission to break users.

[Project philosophy](PROJECT_PHILOSOPHY.md) carries direction and precedents;
[Architecture](ARCHITECTURE.md) owns scoped invariants and their decision basis.
Both start without invented product choices or quotas. Missing decisions block
only dependent work. A clean corpus lint proves neither authority nor judgment.
