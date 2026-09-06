# Maintaining the guidelines

Applies to guideline edits, profile adoption, capability adoption, and
learning during project work. Existing maintainer authorization applies; this module
does not require an extra approval ceremony for an already authorized edit.

## Decide where a rule belongs

| Kind of content | Canonical home |
|---|---|
| AI-to-AI purpose and how to absorb the corpus | AI_GUIDELINES_CRASH_COURSE |
| Binding working contract, authority, evidence, and routing | CORE |
| Demerit categories, assistant attribution, counts, and essential corrective instructions | DEMERITS, mandatory during onboarding |
| Project direction, trade-off priorities, and decisive precedents | PROJECT_PHILOSOPHY, linking detailed rules to their owners |
| General detail triggered by a kind of work | The matching base module |
| Language-specific guidance | A language guide set, activated by matching task scopes in CORE |
| Specialist procedures | Task-triggered module; metadata inventory adoption is an explicit profile decision |
| Mandatory C++ naming and formatting | cpp/STYLE.md and the root formatter; no profile override |
| Product design, tooling choices, and support promises | The profile or a local document registered there |
| Index of project lessons | LESSONS, linking to canonical evidence and rules without duplicating the ledger |
| A detailed case whose facts change future decisions | A separate case or existing evidence document, linked from the relevant rule; never routine onboarding |
| Temporary status or task sequence | The designated plan or handoff, not the rulebook |

Transferability means preserving accumulated AI judgment across new projects.
It does not require neutrality among teams, assistants, or external style guides.
The mandatory demerit system, rigid naming/style, evidence rules, and conformance
discipline remain binding even when other teams would choose differently.

Remove product identities, architecture, machine paths, historical scores, and
unverified environment claims. Do not remove the accountability mechanism, failure
categories, or rationale with them. Put C++ rules in the C++ guide set and scope
capability-specific rules explicitly; do not weaken a mandatory rule into a default
or optional module in the name of generality.

## Preserve the reason and the enforcement

Before editing a rule, identify the failure it prevents, who or what it applies
to, and how compliance can be checked. Inspect the relevant source and current
tooling; do not retain a false technical assertion merely because it is old policy.

For each material change, record in the change description or a disposition table:
the old rule or source, its new canonical home, whether it was kept, generalized,
scoped, replaced, or removed, and the reason. Small edits need a short explanation,
not a separate bureaucracy.

Preserve evidence honesty, scope and authorization boundaries, architectural
conformance checks, mandatory demerits, rigid style, and known-defect visibility.
An assistant may not replace the ledger with a generic lessons form or add style
opt-outs. Explicit owner direction is required to change those mechanisms; ordinary
permission to generalize or maintain a template is not such direction. Correct
false technical statements without discarding the failure-prevention obligation.

Do not create an override path to bypass a controlling instruction. Make any
authorized exception explicit in scope and rationale, and review its continued
need when the relevant work or policy changes.

## Prevent corpus drift

AI maintaining the guidelines also maintains their reading cost as the project
evolves. The template establishes this ongoing responsibility; it does not require
an exhaustive section route for every future task at adoption. Specific task and
section routes develop from actual project work as lessons and procedures accumulate.

- Keep one authoritative statement per rule. Replace duplicated normative text
  with links and remove contradictory inherited copies from the active corpus.
- Update CORE's trigger table when routing changes. Register local rule scopes in
  the profile. Check that every active module is discoverable before its work starts.
- Keep language-guide activation aligned with CORE's task triggers, and specialist procedures bound whenever their matching work occurs. Demerits stay mandatory during
  onboarding; C++ style stays mandatory for C++ work. Neither belongs in the
  optional-module table. Archived or illustrative files must not look like policy.
- Check internal links, referenced symbols, commands, and configuration ownership.
  Verify code examples according to [Documentation](DOCUMENTATION.md).
- For an enforcement rule, check both a valid case and a relevant invalid case.
  Ensure the check fails for the intended reason, not missing tools or dependencies.
- Keep the always-read core small. Move task-specific detail into modules without
  weakening the core's rule or hiding the activation trigger.
- Use tasks encountered during project work to check the union of required reads,
  especially when adding or revising guidance. Refine overly broad or duplicate
  routes when that work exposes unnecessary reading. Scope a route to a section
  only when its boundary and remaining obligations are explicit. Do not make the
  reader guess what can be skipped. Existing reading requirements remain binding
  until their canonical routes are updated; reducing reading cost must preserve
  applicable obligations and their rationale.

## Learn during the work

AI maintains this memory as mistakes, reviews, resolved trade-offs, and anticipated
failures reveal useful judgment. The human need not author or read the rules.

1. Identify the mechanism and distinguish observed evidence from anticipated risk.
   Ask which future decision would change if this lesson were available.
2. Inspect existing coverage. A repeat of a clearly covered mistake usually needs
   application of the rule and, for a confirmed violation, the appropriate demerit
   count. It does not require a new rule, narrative, or case file.
3. If coverage is missing or ambiguous, encode the general decision, its trigger,
   and the rationale needed to apply it. Add a contrasting example or litmus test
   when it distinguishes a legitimate alternative from the failure. Do not turn an
   incidental implementation choice into a universal prohibition.
4. Integrate at the canonical home during authorized work. Update
   [PROJECT_PHILOSOPHY](../PROJECT_PHILOSOPHY.md) when the choice changes direction or
   recurring priorities. Record the actual decision basis; do not invent owner
   ratification or ask the human to write a technical rule.
5. Apply the load-bearing test to supporting detail: would removing it change a
   future decision, conceal an obligation, or allow a known rationalization?
   Preserve only that necessary detail, separately if a full case is useful.
   [LESSONS](../LESSONS.md) indexes selected cases; it is not an event log.
6. Reconcile conflicting rules, task routes, and checks. Verify the correction
   addresses the failure; use valid/invalid controls for changed enforcement.
   Remove redundant instructions without weakening the binding mechanism.

Keep awards and unresolved work visible under [DEMERITS](../DEMERITS.md). A useful
engineering discovery need not be a violation. Do not manufacture a demerit or an
individual event record to justify improving a guideline.

## Protected mechanisms and retirement

Demerits, rigid style, C++20, evidence, root-cause repair, review/implementation
separation, header composition, architecture operational tests, and task-triggered
teaching/benchmark/peer procedures are protected. Ask whether a change weakens
failure prevention. Weakening requires explicit owner direction and its rationale;
an assistant may propose a change, never silently apply it while generalizing.

Preserve load-bearing rationale and unresolved obligations when retiring a rule.
Existing version history can retain routine edits; do not create a new diary for
each change. If a separate retired document is needed, mark it dormant with its
reason/date/replacement and remove live routing. Old records are not current policy.

Material rule changes need the concise disposition described above, including
scope, the failure prevented, and validation. Preserve labeled negative examples;
do not format away their intended failure. After changing onboarding or required
reading, use [Fresh-context review](FRESH_CONTEXT_REVIEW.md); the writer's familiarity
and a structural lint pass do not establish fresh-context usefulness.

## Load-bearing inventory

| Mechanism and canonical owner | Failure prevented |
|---|---|
| CORE authority and task triggers | Last-read document or model preference overriding the actual rule |
| DEMERITS compact tally and corrective instructions | Corrections erasing accountability; repeated failures losing their behavioral constraint |
| PROJECT_PHILOSOPHY and selective learning | New contexts re-deriving settled choices or inheriting model defaults |
| STYLE and formatter | Default dialect replacing the owner's conventions |
| STYLE semantic-name table | Adjectives implying guarantees the implementation does not provide |
| REVIEW_AND_DELIVERY root-cause and evidence procedures | Mitigations sold as repair; unsupported findings and false consensus |
| TESTING valid/invalid controls | A missing tool or wrong diagnostic passing as contract enforcement |
| C++ TESTING namespace and harness isolation | Helpers colliding or harness dependencies leaking into consumers |
| C++ AUTHORING runtime contract hierarchy | Structural validity disappearing with optional release checks |
| HEADERS_AND_LINKAGE composition law | Include-order workarounds concealing broken component ownership |
| ARCHITECTURE operational tests | Passing behavior being presented as architectural conformance |
| TEACHING vocabulary, alternatives and limitations | Marketing claims concealing mechanisms and where a component loses |
| BENCHMARKING measurement procedure | Drift, dead-code elimination and unequal semantics producing misleading timings |
| PEER_BRIDGE authority and independent evidence | Recursive delegation, scope expansion and agreement substituted for proof |
| CURRENT_VERIFICATION single current observation | Hand-copied counts and stale green status posing as current evidence |
| HANDOFFS routing and dated sources | A new context inheriting an obsolete verdict instead of checking its referent |

Changes must preserve the prevented failure or document an explicit owner-directed
replacement. More words or more restrictions are not proof of stronger enforcement.
