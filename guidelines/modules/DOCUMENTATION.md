# Documentation and claims

Applies to documentation, examples, API guarantees, and user-facing claims.
Use the core's evidence requirements and the profile's documentation conventions.

## Write for the reader's task

State the problem, the relevant contract, and the limits before adding implementation
detail. Use plain language and concrete mechanisms. Include rationale when it helps
a reader make a decision or avoid a mistake; omit promotional filler.

Distinguish requirements, recommendations, examples, observed results, and future
plans. A planned feature or validation target must not read as present capability.
Choose tables, diagrams, or code when they make the explanation easier to use,
not to satisfy a universal document format.

Operational AI guidance uses the shortest form that preserves the decision. Consider
its trigger, instruction, necessary rationale, and any discriminating example/check;
these are decision questions, **not required headings or slots**. A sentence and a
canonical link may suffice. Do not expand a sound short instruction into a form or
repeat evidence/style rules already owned elsewhere. Add detail only when omitting
it changes the next decision. Tables and bullets may carry the explanation. No
paragraph quota, type card, or teaching narrative is required for operational guidance.

## Keep references and examples executable where claimed

- Link to canonical source, configuration, or procedures instead of duplicating
  long commands, lists, schemas, matrices, or API definitions.
- Verify file paths, symbols, links, arguments, and supported versions against the
  current source. Do not transfer an old project's names into a new example.
- Label snippets as runnable examples, partial excerpts, or pseudocode. A runnable
  example must state its prerequisites and inputs and use the actual public API.
  Validate it in the environment claimed, or clearly label it unverified.
- Keep frequently used runnable examples in a location that existing tooling can
  check. A copied sample that drifts silently is a documentation defect.
- Do not maintain hand-copied inventory or test counts when the repository can
  derive them. Date snapshots and name their source when a count is necessary.

## Bound technical promises

State preconditions, ownership and lifetime, failure behavior, concurrency limits,
compatibility, and supported configurations where they matter to use of an API.

Terms such as fast, safe, zero-overhead, allocation-free, thread-safe, portable, or
production-ready need a specific meaning and supporting evidence. Replacing a
vague adjective with a technical claim does not supply the missing proof.

No data copy does not mean no allocation; constant algorithmic complexity does not
mean low measured latency; passing tests does not establish production deployment.
Describe mechanisms and observed evidence without implying a broader guarantee.

For measured claims, link a dated result with method, configuration, comparison
scope, and limitations. Do not embed unexplained ratios or claim universal parity
from selected cases. Read [Benchmark claims and citations](BENCHMARKING.md#1-claims-and-citations)
for claim-only work; the complete Benchmarking module governs measurement work.
[Teaching](TEACHING.md) governs explanatory teaching documents, not every document
containing prose. Its full procedures remain binding when that work occurs.

## Maintain with the implementation

Update affected examples and contracts with code changes. Mark obsolete documents
as superseded and point to their replacement. If documentation and implementation
disagree, determine whether the code or the intended contract needs correction;
do not silently change a promise just to match a defect.
