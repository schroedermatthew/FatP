# Tier 3 Remediation Plan — Detailed

**Scope:** 39 documents across three groups  
**Estimated effort:** High — requires governance decisions before mechanical work

---

## Decision 1: Classify the Communication Guides

**8 files in `communication/`**

These documents are stylized persuasion/humor guides aimed at developers resistant to migration. They don't fit any existing doc_type cleanly. Before scaffolding them, pick one of three options:

### Option A: Create a new doc_type "Communication Guide"

Add to the Teaching Documents Style Guide taxonomy table:

| Doc type | Primary question | When to use | Evidence level |
|----------|-----------------|-------------|----------------|
| **Communication Guide** | "How do I convince a resistant colleague?" | Persuasion, myth-busting, psychological framing for migration conversations | Low (rhetorical) |

Define a Communication Guide Card:

```
## Communication Guide Card
**Audience archetype:** [Who you're persuading]
**Core resistance:** [What they believe]
**Key concession:** [What they're right about]
**Migration path:** [What you're actually proposing]
**Tone:** [Humorous / Direct / Empathetic]
```

Add `CG` as a doc_id prefix. Exempt from Migration Guide section requirements (no Rollback, Compatibility, etc.).

**Pros:** Honest classification; no forced structural mismatch.  
**Cons:** Adds taxonomy complexity; only 8 documents use it.

### Option B: Reclassify as Handbooks

These are closest to Handbooks ("What discipline should teams adopt?"). The "discipline" here is interpersonal — how to handle migration conversations. Use `doc_type: "Handbook"` and Handbook Card. Rename H1 titles to `# Handbook - <Title>`.

**Pros:** No taxonomy change; Handbooks are flexible enough.  
**Cons:** Stretches the Handbook definition; these aren't really about engineering discipline.

### Option C: Exempt with explicit carve-out

Add a note to the Teaching Style Guide:

> **Communication guides** (`communication/` directory) are informal persuasion documents exempt from the full scaffolding requirements. They require YAML front matter and a doc_type of "Communication Guide" but are not required to have Scope/Not covered/Prerequisites, document-type Cards, or the structural sections required by other doc types.

**Pros:** Minimal work; acknowledges their unique nature.  
**Cons:** Creates a governance gap.

### Recommendation

**Option A** if you plan to write more of these. **Option C** if this is a closed set of 8.

---

## Decision 2: Classify the Compile Time Errors Course

**28 files across 6 subdirectories**

The CTE course has its own internal structure (README, overview, mini-sessions, problem sessions, handbooks, appendices, reference). The files map to existing doc_types but use course-specific naming conventions. Decision needed: govern them under the Teaching Style Guide or carve them out.

### Option A: Full governance — map each file to a doc_type

| CTE file pattern | Count | Proposed doc_type | doc_id prefix |
|-----------------|-------|-------------------|---------------|
| `compile_time_error_detection_overview.md` | 1 | Overview | OV-CTE-001 |
| `handbook_*.md` | 3 | Handbook | HB-CTE-001..003 |
| `problem_session_*.md` | 11 | Case Study | CS-CTE-001..011 |
| `mini_session_*.md` | 6 | Design Note | DN-CTE-001..006 |
| `appendix_*.md` | 2 | Foundations | FN-CTE-001..002 |
| `exercises_by_difficulty.md` | 1 | (reference, no doc_type) | — |
| `quick_reference_card_*.md` | 1 | (reference, no doc_type) | — |
| `technique_decision_flowchart.md` | 1 | (reference, no doc_type) | — |
| `Implementation Safety...md` | 1 | Design Note | DN-CTE-007 |
| `README_compile_time_safety_course.md` | 1 | (course README, no doc_type) | — |

This means adding YAML + Scope/Not covered/Prerequisites + appropriate Card to ~25 files. Reference files (exercises, quick reference card, flowchart, README) would get minimal YAML but no Card or structural sections.

**Rename H1 titles** to follow `# <Doc Type> - <Title>`:

- `# Problem-Solving Session 3: The Impossible Transition` → `# Case Study - The Impossible Transition`
- `# Handbook: Phantom Types` → `# Handbook - Phantom Types`  
- `# Mini-Session 1: Deleted Functions` → `# Design Note - Deleted Functions`
- etc.

### Option B: Light governance — YAML only, preserve course structure

Add YAML front matter to all files with `doc_type`, `doc_id`, `title`, `fatp_components`, `topics`, `constraints`, `audience`, `status`, `last_verified`, `cxx_standard`. Do NOT rename H1 titles or add Scope/Not covered/Prerequisites. Accept that these files have a course-specific structure that doesn't map perfectly to the teaching doc templates.

Add a note to the Teaching Style Guide:

> **Course materials** (e.g., `Compile Time Errors/`) use YAML front matter for AI retrieval but follow course-specific structural conventions (sessions, mini-sessions, exercises) rather than the standard document-type templates.

### Option C: Exempt entirely

Add a carve-out:

> Files in `Compile Time Errors/` are course materials and are exempt from the Teaching Documents Style Guide. They are not indexed for AI retrieval.

### Recommendation

**Option B** — YAML makes them searchable by AI without forcing awkward structural changes. The problem sessions have a natural problem→diagnosis→fix arc that doesn't need to be renamed to "Case Study" to be useful.

---

## Execution Plan (after decisions)

### Phase 1: Communication Guides (8 files)

Assuming Option A (new doc_type) or Option C (carve-out):

**Step 1a — Add YAML front matter to all 8 files**

Each file gets:

```yaml
---
doc_id: CG-RUST-001  # (or whatever prefix is chosen)
doc_type: "Communication Guide"
title: "Have You Heard the Good News About Ownership?"
fatp_components: []
topics: ["migration-resistance", "rust-advocacy", "team-dynamics", "ownership-semantics"]
constraints: ["interpersonal", "organizational-change"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "team leads", "AI assistants"]
status: "draft"
---
```

Content per file:

| File | doc_id | Key topics |
|------|--------|------------|
| Rust Evangelist | CG-RUST-001 | rust-advocacy, ownership, borrow-checker, migration-resistance |
| Keep Your RAII | CG-CTOCPP-001 | c-migration, RAII-resistance, manual-memory, discipline-myth |
| Real Programmers | CG-ABSTRACTION-001 | abstraction-wars, over-engineering, under-engineering |
| MATLAB Programmer | CG-MATLAB-001 | matlab-migration, scientific-computing, array-semantics |
| Angle Brackets | CG-TEMPLATES-001 | template-phobia, generic-programming, compile-time-errors |
| Fortran to C++ | CG-FORTRAN-001 | fortran-migration, scientific-legacy, array-layout |
| auto Considered Harmful | CG-CPP98-001 | cpp98-migration, auto-keyword, modern-cpp-resistance |
| pip install reality-check | CG-PYTHON-001 | python-migration, dynamic-typing, performance-expectations |

**Step 1b — Rename H1 titles** (if Option A)

- `# Have You Heard the Good News About Ownership?` → `# Communication Guide - Have You Heard the Good News About Ownership?`
- etc.

If Option C, leave H1 titles as-is (they're creative titles that serve their purpose).

**Step 1c — Add Card** (if Option A)

Communication Guide Card after H1, with fields: Audience archetype, Core resistance, Key concession, Migration path, Tone.

**Step 1d — Fix banned term**

Replace "backport" in Rust Evangelist guide line 277.

**Step 1e — Add TOC** to files >800 lines

4 files need TOC: Rust Evangelist (859), Keep Your RAII (1082), Fortran (1050), auto Considered Harmful (939). pip install reality-check (934) also.

### Phase 2: Compile Time Errors Course (28 files)

Assuming Option B (YAML only):

**Step 2a — Add YAML front matter to all 25 teachable files**

Skip the 3 reference/utility files (README, exercises_by_difficulty, quick_reference_card) or give them minimal YAML.

Content per file type:

**Overview (1 file):**
```yaml
doc_id: OV-CTE-001
doc_type: "Overview"
title: "Compile-Time Error Detection"
fatp_components: ["StrongId", "StateMachine", "Expected", "enforce"]
topics: ["compile-time-safety", "type-system", "deleted-functions", "concepts", "static-assert"]
```

**Handbooks (3 files):**
```yaml
doc_type: "Handbook"
# handbook_phantom_types -> HB-CTE-001, topics: phantom-types, type-tags, zero-cost-abstraction
# handbook_policy_based_design -> HB-CTE-002, topics: policy-pattern, template-policies, compile-time-dispatch
# handbook_safe_reference_patterns -> HB-CTE-003, topics: reference-safety, dangling-references, lifetime
```

**Problem Sessions (11 files):**
```yaml
doc_type: "Case Study"
# Each gets CS-CTE-001 through CS-CTE-011
# Topics derived from the specific technique taught
```

**Mini-Sessions (6 files):**
```yaml
doc_type: "Design Note"
# Each gets DN-CTE-001 through DN-CTE-006
# Short focused pieces on one technique
```

**Appendices (2 files):**
```yaml
doc_type: "Foundations"
# FN-CTE-001: compiler flags
# FN-CTE-002: C++23/26 futures
```

**Step 2b — Rename H1 titles to use `# <Doc Type> - <Title>`**

Even under Option B, H1 consistency aids AI retrieval:

- `# Handbook: Phantom Types` → `# Handbook - Phantom Types`
- `# Problem-Solving Session 3: The Impossible Transition` → `# Case Study - The Impossible Transition`  
- `# Mini-Session 1: Deleted Functions` → `# Design Note - Deleted Functions`
- `# Appendix: Compiler Flags Reference` → `# Foundations - Compiler Flags Reference`

**Step 2c — Add Scope/Not covered/Prerequisites** (Option A only)

If full governance, each file gets a brief scope block. Under Option B, skip this.

**Step 2d — Add Cards** (Option A only)

Under full governance, each file gets its doc-type Card. Under Option B, skip this.

### Phase 3: Root-Level Documents (11 files)

These are standalone Foundations, Handbooks, Case Studies, and a Pattern Guide that have partial compliance. Most have YAML but are missing Scope/Not covered/Prerequisites. Some lack YAML entirely.

**Step 3a — Add YAML to 7 files missing it**

| File | doc_type | doc_id |
|------|----------|--------|
| Foundations - Cpp_Historical_Context.md | Foundations | FN-HISTORY-001 |
| Handbook - Bridging_the_Hardware_Gap.md | Handbook | HB-HARDWARE-001 |
| Handbook - Designing_Performance_Invariants.md | Handbook | HB-INVARIANTS-001 |
| Handbook - Discipline_of_Class_Design.md | Handbook | HB-CLASSDESIGN-001 |
| Handbook - FAT-P Serialization.md | Handbook | HB-SERIAL-001 |
| Case Study - The Noreturn Mirage and the Noexcept Cliff.md | Case Study | CS-NORETURN-001 |
| Case Study - StableHashMap Benchmarking.md | Case Study | CS-HASHBENCH-001 |
| Pattern Guide - Factory_Pattern_Guide.md | Pattern Guide | PG-FACTORY-001 |

(Note: 8 files, not 7 — I miscounted earlier.)

**Step 3b — Add Scope/Not covered/Prerequisites to all 11**

Each file needs a brief scope statement, 2-3 exclusions, and 2-3 prerequisites. These are short additions inserted after the H1 title block.

**Step 3c — Add missing Cards**

Files without their doc-type Card need one added. Check each file individually — some may have informal equivalents that just need renaming.

---

## Execution Order

| Step | Files | Effort | Dependency |
|------|-------|--------|------------|
| **Decisions 1 & 2** | 0 | Discussion | None — do this first |
| Phase 3 (root-level) | 11 | Low-Medium | No dependency |
| Phase 1 (communication) | 8 | Low-Medium | Decision 1 |
| Phase 2 (CTE course) | 25-28 | Medium-High | Decision 2 |

Phase 3 can proceed immediately since there are no governance decisions needed — those are standard doc_types with clear mappings. Phases 1 and 2 are blocked on the classification decisions.

---

## What Tier 3 Does NOT Cover

- Vocabulary audit (removed from scope per guidelines update — banned terms only apply to component Overviews, User Manuals, and Companion Guides)
- Content rewrites (Tier 3 is structural scaffolding, not prose editing)
- User Manuals in `C to C++ Migration/` (5 files with partial compliance — could be a Tier 3b)
- `Migration_Guide_-_C_to_Modern_Cpp_Teaching.md` (2974-line mega-doc missing TOC — should be split per Style Guide policy, but that's a content decision)

---

*Tier 3 Remediation Plan — February 2026*
