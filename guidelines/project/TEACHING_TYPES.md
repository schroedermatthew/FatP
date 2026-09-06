# FatP teaching types

Read this with [Teaching](../modules/TEACHING.md) for teaching-document work.
The base module owns motivation before code, evidence, honest limitations,
vocabulary, examples, diagrams and common visible structure. This file preserves
FatP's existing document purposes and their project-specific records. It is not
an onboarding document or an alternative teaching rulebook.

## Established types

Use `<Type> - <Topic>.md`, with an H1 matching the declared title. These ten types
already have real FatP documents; they are not speculative additions to the
template's starting taxonomy. A generic Guide or Developer Manual remains
available when its base-module purpose fits the work better.

| Type / ID prefix | Purpose and additional structure |
|---|---|
| Overview / OV | Component selection: problem, mechanism, features, alternatives, performance mechanisms/limits, integration and assessment. |
| User Manual / UM | Correct use: domain context, architecture, quick start, recipes, error model, performance mechanisms, selection, migration where relevant, troubleshooting and concise API reference. |
| Companion Guide / CG | Design reasoning: four-part arc of Problems, Solutions, Case Studies and Foundations. |
| Case Study / CS | An evidenced failure: four-part arc of Problems, Solutions, the Case Study Story and Foundations; end with the design rules and concrete next actions. |
| Foundations / FN | Background concepts, myths versus facts, implications for systems code, brief component connection and glossary. |
| Handbook / HB | Team discipline: principles, hard rules versus heuristics, checks, anti-patterns, worked examples and adoption guidance. |
| Pattern Guide / PG | Reusable recipe: intent/non-goals, use/avoid criteria, steps, variants, real reference implementation, pitfalls and FAQ. |
| Design Note / DN | Narrow decision: context, constraints, options, rationale, consequences/obligations and adopted/proposed status. |
| Benchmark Results / BR | Measured evidence: dated source/configuration, comparison scope, results with units and spread, interpretation, limits and raw data. |
| Migration Guide / MG | C pattern/API to C++ abstraction: alternatives, mapping, steps, compatibility, lifetime, concurrency, failure model, verification and rollback. |

The Four-Part Arc teaches the causal chain, not a quota of chapters. Preserve
constraint grounding, the mechanism, guarantees/non-guarantees, rejected choices
and failure evidence. Do not fill sections with unverified claims merely to fill
the form. An explicitly hypothetical opening is labeled before it is used.

A User Manual's migration chapter may cover std/Boost-to-FatP usage. A standalone
Migration Guide describes a user's external C-code migration; its compatibility
or rollout plan does not authorize compatibility shims in FatP's own pre-release
API. Include alternatives immediately after a Migration Card.

## Cards

Cards are near the start and use the base module's fields for User Manual,
Developer Manual, generic Guide, Design Note and Case Study. Retain the following
FatP-specific fields when the corresponding type is used:

| Type | Additional or type-specific card fields |
|---|---|
| Overview | Component; Problem solved; When to use; When not to use; Key guarantee; std equivalent; Boost equivalent; Other alternatives; Read next. |
| User Manual | Component; Integration pattern; std equivalent; Migration from std; Common mistakes. |
| Companion Guide | Component; Design question; Key tradeoff; Decision made; Rejected alternatives; Historical context. |
| Case Study | Components used; Build-mode gotchas. |
| Foundations | Topic; Why it matters; Key concepts; Mental model; Common misconceptions; Read next. |
| Handbook | Domain; Core principle; Key discipline; Common failure; Hard rules; Applies to; Build-mode notes; Guarantees; Non-guarantees. |
| Pattern Guide | Pattern; Problem; Solution shape; When to use; When not to use; Implementation; Key insight. |
| Benchmark Results | Component; Competitors; Key finding; Conditions; Caveats; Raw data. |
| Migration Guide | From; To; Why migrate; Compatibility strategy; Mechanical steps; Behavioral equivalence; Intentional differences; Failure model; Threading model; Lifetime model; Alternatives; Verification; Rollback plan. |

## Front matter and vocabulary

Use the base module's canonical `components` list for new or substantively edited
teaching documents. When converting a historical `fatp_components` field, carry
the complete associations into `components` and remove the old key; do not keep
two competing indexes. Preserve `doc_id` values and use the established prefixes
above for new IDs. Retain these FatP additions when applicable:

- `cxx_standard`: the actual minimum required by the documented examples, at least
  C++20 for FatP; a later mode requires actual use of that mode.
- `std_equivalent` and `std_since`: a real equivalent and its standard version.
- `boost_equivalent`: a real comparator or an explicit absence.
- `build_modes`: modes that change the documented behavior.
- `audience`: include AI assistants alongside the intended human readership.

Existing documents with historical front matter are not silently reverified by
guideline adoption. Change `last_verified` only after checking the claims and
examples it dates. Do not mass-edit unrelated manuals to fabricate conformance.
The base module's table-of-contents threshold applies to current authoring.

FatP also retains the component-document vocabulary discipline for component
Overviews and Companion Guides, in addition to the User/Developer Manuals covered
by the base module. Apply its same precise replacements to these component
documents. This does not extend the ban to historical teaching narratives or
require promotional claims. A component Overview establishes fit through actual
mechanisms, alternatives and where it loses; it need not claim superiority or
that FatP is the only possible choice.

For containers, present intent, invariant, complexity, iterator/reference/pointer
invalidation, concurrency and use/avoid criteria. Component documents identify
actual std and Boost alternatives instead of inheriting an old comparison table.
Do not infer a competitor's contract from a label or old sample text.
