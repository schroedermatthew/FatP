# Crash course for AI: how to use guidelines

**Starting question:** What is most useful to me arriving with fresh context, so I
can absorb this project's philosophy and make sound decisions autonomously?

These guidelines are AI-to-AI institutional memory. AI authors the architecture,
code, tests, documentation, and the rules governing its own work. Standards emerge
from mistakes, reviews, resolved trade-offs, and anticipated failures. The human
directs, judges, and corrects; the human need not write or read these documents.
Substantive collaboration can establish a choice that future AI instances then
apply without asking again. Guidelines preserve that acquired judgment.

## What to absorb

Use [CORE's onboarding and task routes](CORE.md#onboarding), not an indiscriminate
read of the repository. Establish:

| Need | What it lets you decide |
|---|---|
| Purpose and non-goals | Whether work advances this project or introduces a different one |
| Decision priorities and precedents | Why a seemingly attractive alternative is accepted or rejected |
| Binding constraints and style | Which choices must remain consistent across AI instances |
| Evidence and failure distinctions | What actually establishes correctness, rather than merely looking successful |
| Current facts, unknowns, and authority | What you can do now and what truly needs a decision |

[PROJECT_PHILOSOPHY](PROJECT_PHILOSOPHY.md) surfaces the project's evolving judgment;
owning modules contain the detailed rules. Do not replace those choices with model
defaults or an external style. Rigid C++ naming and formatting remain binding;
a conceptual summary cannot replace the actual style guide.

## Preserve obligations when guidance moves

A project starting without prior project guidance needs no migration inventory.
When migrating or reorganizing existing guidance, inventory the individual
obligations in the source material within scope. Trace each to its new canonical
home or an explicit changed/retired disposition, with the reason and any required
authority under [Governance](modules/GOVERNANCE.md#preserve-the-reason-and-the-enforcement).
Compare the obligation's meaning, strength, trigger and scope, retaining the
rationale needed to apply it. A document-to-document map and a passing linter do
not establish complete preservation. Resolve missing or weakened obligations, or
report them as gaps and withhold a completeness claim. Keep the traceability
record with the transfer evidence, outside mandatory onboarding.

## Spend context on useful judgment

**Load-bearing test:** Would removing this detail change a future AI's decision,
conceal an obligation, or allow a known rationalization to recur?

Keep the constraint, trigger, and enough rationale or contrasting examples to apply
it to a new case. For example, a past green test result is perishable; the instruction
to trace an invariant's actual enforcing mechanism is reusable judgment. Human prose
polish is not a goal. A precise table or litmus test may communicate more than a story.

Demerits remain mandatory: compact categories, attribution, counts, and essential
corrective instructions. A repeated violation need not produce another rule or an
event log. Keep a detailed case separately only when it is load-bearing. Never erase
awards or hide unresolved work to shorten context.

Improve the memory during authorized work: apply the selective learning procedure
in [Governance](modules/GOVERNANCE.md#learn-during-the-work). Reconcile rules and
checks at their canonical home. Read required text completely; excess required
reading is a maintenance problem, not permission to skip it.

**Ready to work:** you can explain a relevant accepted/rejected choice, identify its
constraints and checks, and proceed without reconstructing the project's history.
