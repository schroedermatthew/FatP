# Teaching Documents Module — documentation that teaches

**Authority:** [CORE](../CORE.md). Project facts belong in [PROJECT_PROFILE](../PROJECT_PROFILE.md).

Assumes [CORE](../CORE.md) has been read. This complete module applies to explanatory
teaching documents: manuals, tutorials, guides, design narratives, and case studies.
It does not prescribe the form of operational guidelines, philosophy/decision
records, profiles, ledgers, reviews, handoffs, status, or chat replies. Those use
[Documentation](DOCUMENTATION.md). A document's job determines its scope, not its
filename or the presence of prose/code. Do not disguise a teaching document as an
operational note to evade its requirements.

Documentation exists to make readers better at using the project and understanding the patterns behind it — not to summarize an API. Prefer many small documents over one large one: each document has one job, one audience, and one "Not covered" list pointing to its siblings.

---

## 1. Document-type system

Every teaching document declares its type, and each type has a defined intent, required sections, and a **Card** — a quick-reference summary block in a fixed format near the top. Cards let a reader extract the key facts without reading the whole document, and they force the author to actually know those facts.

A minimal starting taxonomy:

| Type | Question it answers | When to write it |
|------|---------------------|-----------------|
| **User Manual** | "How do I use this correctly?" | One per component: integration, recipes, edge cases, troubleshooting |
| **Developer Manual** | "How does this work internally?" | When internals are non-obvious: rationale, invariants, extension points |
| **Design Note** | "What decision did we make, and why?" | When a significant design choice needs a permanent record |
| **Case Study** | "What went wrong, and what does it teach?" | After an instructive failure: symptom → root cause → fix pattern |
| **Guide** | "How do I accomplish this cross-cutting task?" | Migration, companion, or reference material spanning components |

Naming: `<Type> - <Component or Topic>.md`. Add types ([Overview, Handbook, Foundations, Benchmark Results]) only when a real document does not fit the existing ones.

### Per-type cards

Define one Card template per type. Examples to adapt:

```markdown
## Case Study Card
**Problem:** [one sentence — what went wrong]
**Constraint:** [the hardware/language/design constraint that caused it]
**Symptom:** [what the user observed]
**Root cause:** [the actual bug]
**Fix pattern:** [how to fix it]
**Guarantees:** [what the fix guarantees]
**Non-guarantees:** [what the fix does NOT guarantee]
```

```markdown
## Design Note Card
**Decision:** [what was decided]
**Context:** [why a decision was needed]
**Options considered:** [alternatives evaluated]
**Chosen option:** [what was selected]
**Rationale:** [why it won]
**Implications:** [what this means for users/maintainers]
```

The **Guarantees / Non-guarantees** pairing recurs across cards deliberately: stating what a mechanism does *not* promise is as load-bearing as stating what it does.

### YAML front matter

Every teaching document opens with a machine-readable YAML block — hidden when rendered, parsed for retrieval and cross-referencing:

```yaml
---
doc_id: [TYPE-COMPONENT-NNN]
doc_type: "Case Study"
title: "..."            # must match the H1 exactly
components: ["..."]     # header/component names this document is about
topics: ["..."]         # 3-8 specific, searchable noun phrases — problem terms AND solution terms
constraints: ["..."]    # the engineering forces: "cache line size", not "performance issues"
last_verified: "YYYY-MM-DD"   # when code excerpts and claims were last checked
audience: ["developers", "AI assistants"]
status: "draft" | "reviewed" | "final"
---
```

Topics and constraints are the search surface — choose terms someone with the *problem* would type. `last_verified` is a promise: update it only when you actually re-verified the excerpts.

### Universal visible structure

| Element | Purpose |
|---------|---------|
| H1 title | `# <Type> - <Component or Topic>` — matches the YAML title |
| Scope | What this document covers (1–3 sentences) |
| Not covered | Explicit exclusions, each with a pointer to the right document |
| Prerequisites | Knowledge assumed of the reader |
| Table of contents | Required above 200 lines |

---

## 2. Manuals teach, they do not summarize

Assume the reader is intelligent but unfamiliar with this specific problem domain. Every major section answers four questions in order:

1. **What is this?** Define the concept in plain terms.
2. **Why does it exist?** What problem does it solve? What goes wrong without it?
3. **When should I use it?** Decision criteria, tradeoffs, alternatives.
4. **How do I use it?** API with complete, compilable examples.

A section that only answers "how" is an API summary the reader could have gotten from the header. Pair anti-patterns with correct patterns: show the failing/naive code, explain the failure mechanism in prose, then show the corrected form — the contrast is the lesson.

## 3. Prose–code discipline

**Motivation before code.** Every code block is earned by the prose that precedes it: what problem this code addresses, why the reader should care, what to notice when reading it.

**No orphan code blocks.** "Here's an example:", "Consider this code:", or a bare section heading are not sufficient motivation.

**Bullet lists are not explanation.** A bullet list of "Problems:" after a code block is incomplete thinking — if you can list the problems, you can explain them in prose *before* showing the code. Bullets summarize an explanation that already happened; they never substitute for it.

**Tables are reference, not teaching.** Comparison tables belong in reference sections and quick-reference summaries. The main narrative explains differences in prose; a table as the primary teaching vehicle is a skipped explanation.

**Code density target:** 2–4 paragraphs of prose per code block in teaching sections. API-reference sections may run denser. Three or more code blocks in a row without substantial prose means restructure; a section that is mostly code should probably be an appendix.

**The readability test:** would a reader read this paragraph by paragraph, or skim looking for code? If they'd skim, the prose isn't doing its job — rewrite until the code feels like illustration, not the only thing worth reading.

## 4. Vocabulary discipline

### 4.1 Banned terms in component documentation

The following terms are banned in component User Manuals and Developer Manuals. Each is vague where the document must be precise — replace with mechanism-specific language:

| Banned | Required replacement | Why |
|--------|---------------------|-----|
| Fast | Zero-allocation, O(1), cache-local, "check mode X eliminates branches" | Name the mechanism |
| Safe | Bounds-verified, lifetime-tracked, type-checked, "throws X on violation" | Specify what is checked |
| Efficient | Constant-time, single-pass, zero-copy | Specify complexity |
| Simple | Minimal API, single-header, no configuration required | Specify the simplicity |
| Powerful | Composable, policy-based, "mixed static/dynamic dimensions" | Specify the capability |
| Easy | No configuration required, deduction-guided | Avoid condescension |
| Flexible | Configurable, pluggable | Specify the axis of variation |
| Thread-safe | Lock-free, mutex-protected, reader/writer-locked | Specify the strategy |
| Modern | C++20 or the actual higher standard, constexpr-evaluated, concepts-constrained | Specify the standard |
| Lightweight | Header-only, zero dependencies, N bytes per instance | Specify the weight |
| Robust | State the failure modes handled, one by one | "Robust" is a claim, not a fact |
| Seamless | State the integration steps; if there are none, say "no integration steps" | Nothing is seamless |
| "Handles errors" | Propagates, throws `<type>`, returns `<result type>`, terminates | Name the mechanism |

**Scope:** the ban applies to component User Manuals and Developer Manuals — the documents that make promises the library must keep. It does not apply to teaching narrative in Case Studies/Guides, code comments, Doxygen, or this guidelines corpus, where precise language is still encouraged but vague terms are permitted.

### 4.2 Forbidden phrases (all documents)

| Forbidden | Instead |
|-----------|---------|
| "This class provides..." | Start with what problem it solves |
| "This component allows..." | State what it *does* |
| "In this section, we will..." | Just start |
| "As mentioned earlier..." | Remove, or link directly |
| "It is important to note that..." | State the fact |
| "In order to..." | "To" |
| "Utilize" | "Use" |

### 4.3 No meta-commentary (all documents)

Do not comment on the psychological effect documentation will have on readers: no "builds trust," "builds credibility," "makes readers confident," "establishes authority," "demonstrates expertise." If you have to announce it, you don't have it. *Wrong:* "Being honest about limitations builds trust." *Right:* "Be honest about limitations" — then be honest.

## 5. "Why Not Alternatives?" is required

Every component-level document (Overview or User Manual) must explicitly address the alternatives: does a standard-library equivalent exist (and since which standard version)? A well-known third-party equivalent? Why does this component exist anyway — what concrete mechanism does it add or omit? Assume the reader does not know the alternatives; give context before comparing features. A bare feature-matrix row ("CheckMode: Yes/No") without a paragraph explaining what the alternative is and where it fits is not a comparison. "No equivalent exists" is a valid and useful answer — state it explicitly.

## 6. Evidence standards

**Never invent evidence.** Numbers require benchmark output, profiler data, sanitizer output, or a clearly defined theoretical count ("N allocations because..."). Unproven claims are labeled **hypothesis**.

**Label code blocks precisely:**

- **Verbatim** — name the file and symbol; no editorial edits inside the block.
- **Annotated excerpt** — labeled as annotated; editorial notes clearly marked as comments.
- **Pseudocode** — labeled as pseudocode; never used to justify forensic conclusions about real code.

**Debug vs release must be explicit.** If behavior changes under `NDEBUG`, sanitizers, or different check modes, include a callout saying exactly what changes in which build.

**Every example claimed compilable must compile** against the current source.
Deliberately invalid examples need a valid control and must fail for the intended
diagnostic; partial excerpts and pseudocode are labeled, not falsely compiled. API drift between docs and code (a documented signature the code no longer accepts) is the canonical failure this rule prevents; where practical, examples go in a doc-example compile target so CI verifies them.

**No specific benchmark numbers in manuals or headers.** Multipliers, absolute timings, and percentages are platform-dependent, compiler-dependent, and stale on the next code change. Describe architectural mechanisms (O(1), zero-allocation path, cache-friendly layout); numbers live in timestamped, platform-identified benchmark result files. Exception: historical narrative in Case Studies ("during development, we measured a 3× improvement from this change") is a fact about what happened, not a claim about current performance. Litmus test: if a compiler update could invalidate the number, it does not belong in prose.

## 7. Mandatory honesty sections

**"Where it loses."** Every component-level document includes an explicit account of where the component is worse than the alternatives — workloads, constraints, or environments where the reader should choose something else. A document with only a "where it wins" story is marketing.

**Maturity honesty.** A library without an installed base must never be described as "production-tested," "battle-tested," or "proven." "Written to production standards" is acceptable as a statement about code-quality intent; "production" as a deployment status requires actual deployments. When comparing against established libraries, acknowledge what they have that the project does not: installed base, cross-platform validation, years of real-world bug reports. Blanket parity claims are banned; per-component benchmark comparisons with explicit methodology are fine.

## 8. Diagram rules

Use diagrams (mermaid or equivalent) when architecture, memory layout, data flow, decision trees, or multi-actor sequences are easier to follow visually than in prose. Do not use them to replace prose, for simple linear sequences that read fine as a numbered list, or for API reference.

- **Black-and-white by default.** Structure, labels, edge direction, and grouping carry the meaning. Do not add color to look polished.
- **Color only when it encodes information** (pass vs fail paths, required vs optional, real vs mock backend, active vs disabled) — and then a legend or preceding sentence must define exactly what the colors mean *in that diagram*. No global palette whose meaning drifts between diagrams.
- **Diagram truth:** a diagram must not imply dependencies, ownership, lifetime, control flow, or data flow the current code does not have. Verify every edge against the implementation before publishing.
- **No orphan diagrams:** the same discipline as code blocks — preceding prose says what the diagram shows and what to notice.

## 9. Checklist

- [ ] YAML front matter complete; `title` matches H1; `last_verified` honest
- [ ] Type Card present and filled (including Non-guarantees where the card has them)
- [ ] Scope and Not covered sections present, with pointers
- [ ] Each section answers what / why / when / how — not just how
- [ ] Anti-pattern → correct-pattern pairs where a failure mode is taught
- [ ] No orphan code blocks; no bullet-list-as-explanation; tables in reference roles only
- [ ] Code density within target; readability test passed
- [ ] Banned vocabulary replaced; no forbidden phrases; no meta-commentary
- [ ] "Why Not Alternatives?" addressed; "Where it loses" present
- [ ] No maturity overclaims; no benchmark numbers in prose
- [ ] Code blocks labeled verbatim / annotated / pseudocode; examples compile
- [ ] Diagrams black-and-white or locally legended; every edge verified against code

## Complete card set

Use these required card fields, with explicit non-guarantees where applicable:

| Type | Card fields |
|---|---|
| User Manual | Problem; Use when; Prerequisites; Main operations; Guarantees; Non-guarantees; Failure modes |
| Developer Manual | Responsibility; Invariants; Ownership/lifetime; Extension points; Constraints; Non-guarantees |
| Guide | Task; Inputs; Prerequisites; Procedure; Expected result; Limits |
| Design Note | Decision; Context; Options; Chosen option; Rationale; Implications |
| Case Study | Problem; Constraint; Symptom; Root cause; Fix pattern; Guarantees; Non-guarantees |

Examples containing bracketed descriptions are synthetic authoring forms, not
receiving-project facts. Add a type only for a real unmet document purpose.
