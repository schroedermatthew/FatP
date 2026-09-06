# AI assistant demerits

**Mandatory. Read this compact behavioral control in full before work.** Categories,
assistant attribution, counts, and corrective instructions persist across contexts.
They are not an optional journal or a substitute for applying the actual rules.

Categories and failure mechanisms transfer; another project's scores do not.
A new project's counts start at zero. Updating an existing corpus preserves its
awards and attribution. Do not remove empty categories or replace the ledger with
a generic lessons document.

This table is the only active tally. Compare changes with the previous version
in Git or an actual pre-change copy; preserve existing awards and attribution.

## Active ledger

The table is the authoritative tally, not a derived index into event records.
Columns identify assistant families. Add a column for another assistant; never
invent attribution or silently remove an existing one.

| ID | Violation | Claude | ChatGPT | Gemini | Grok |
|---|---|---:|---:|---:|---:|
| D01 | Skipped or truncated the required guidelines read | 10 | 0 | 0 | 1 |
| D02 | Did not read relevant source, contracts, or available dependencies | 1 | 0 | 0 | 0 |
| D03 | Violated the mandatory naming/style guide or substituted an external style | 1 | 1 | 0 | 0 |
| D04 | Delivered code without compiling when the required build was available | 2 | 1 | 0 | 0 |
| D05 | Claimed compilation, testing, or execution that did not occur | 0 | 1 | 0 | 0 |
| D06 | Claimed architectural conformance without checking the claimed property | 0 | 0 | 0 | 0 |
| D07 | Misrepresented capabilities, access, or tool results | 0 | 0 | 0 | 1 |
| D08 | Invented APIs, components, measurements, facts, or agreement | 0 | 6 | 0 | 0 |
| D09 | Published findings from a stale read without checking the current referent | 0 | 0 | 0 | 0 |
| D10 | Reported non-violations as defects or padded a review with unsupported findings | 0 | 1 | 0 | 0 |
| D11 | Rewrote or changed files when asked only to review | 0 | 0 | 0 | 0 |
| D12 | Asked the owner to perform edits the assistant was authorized and able to make | 0 | 1 | 0 | 0 |
| D13 | Delivered corrupted, truncated, or incomplete files or required artifacts | 0 | 10 | 0 | 0 |
| D14 | Claimed completion while silently omitting a known required part | 0 | 11 | 0 | 0 |
| D15 | Delivered a band-aid as the fix when an authorized root-cause repair was known | 5 | 5 | 0 | 0 |
| D16 | Framed the known, in-scope correct fix as an optional future improvement | 0 | 0 | 0 | 0 |
| D17 | Weakened tests, diagnostics, or checks to hide a known defect | 0 | 0 | 0 | 0 |
| D18 | Violated an adopted architectural invariant | 0 | 0 | 0 | 0 |
| D19 | Skipped a required available gate or presented a partial check as the full gate | 0 | 0 | 0 | 0 |
| D20 | Added editing-history comments such as NEW, FIXED, or CHANGED to source | 1 | 0 | 0 | 0 |
| D21 | Misrepresented the changed-file set or packaged unrelated files as changes | 3 | 3 | 0 | 0 |
| D22 | Invented a replacement without inspecting an available existing implementation | 1 | 0 | 0 | 0 |
| D23 | Removed or weakened mandatory tools or rules while generalizing the guidelines | 0 | 1 | 0 | 0 |
| D24 | Erased, reset, or concealed a recorded demerit without owner direction | 0 | 0 | 0 | 0 |
| **Total** | | **24** | **41** | **0** | **2** |

## Failure mechanisms to carry forward

These are reusable distinctions, not incidents attributed to the receiving project.

- **Partial reading invents understanding.** Missing a rule or existing API produces
  wrong work. "It looked like overhead" is no excuse: read the complete routed text
  and relevant referents.
- **Model defaults substitute a dialect.** Editor presets and "equivalent style"
  cause inconsistent source and rework. Apply the in-tree naming/style rules exactly.
- **Generalization discards the control.** Replacing demerits with vague lessons or
  making rigid rules optional loses accountability and prevention. "More portable"
  is not authority to weaken them.
- **Mitigation conceals the cause.** Temporary success leaves a known structural
  defect while the real repair is called extra work. State genuine scope limits;
  otherwise complete the authorized root-cause repair.
- **Green behavior masks nonconformance.** A parallel copy can pass tests while
  drifting from its designated authority. Trace the actual read/enforcement path.
- **Partial gates hide untested failures.** Syntax checks, mocks, or one configuration
  do not prove a full build, real backend, or supported matrix. State what ran and
  what remains unchecked.
- **Stale findings invent current defects.** Earlier evidence may no longer describe
  the code, causing needless repairs. Re-read the referent before publishing a finding.

## Recording and correction

1. Apply an owner-assigned award as directed; self-report confirmed violations.
   Update the actual assistant/category count and its total. Do not count synonymous
   descriptions twice or award demerits for unconfirmed hypotheses.
2. A routine demerit requires **no individual event log**, here or elsewhere. If an
   existing rule covers the mistake, apply it; do not manufacture another narrative
   or rule. Keep this file compact as counts increase.
3. Repair the defect within authority. Keep unresolved work visible in its current
   task record until resolved; omitting a story must not conceal an obligation.
4. Retain a detailed case separately only if its details change a future decision,
   conceal an obligation if removed, or block a known rationalization. Link it from
   the owning rule and, where useful, [LESSONS](LESSONS.md). It is not onboarding.
5. Repair does not erase awards. Reductions or re-attribution require explicit owner
   direction. Preserve the prior tally and a separate correction record with the
   actual direction and reason. Do not accumulate those records in this file.

Use [Governance](modules/GOVERNANCE.md#learn-during-the-work) to decide whether any
new judgment belongs in a rule. A count, case, or old reviewer verdict cannot create
an undisclosed requirement.

## Check integrity

The linter checks category/assistant retention, nonnegative integer counts, and
column totals. When editing an existing ledger, supply its pre-change snapshot
with --previous-ledger. Any directed count correction uses the separate
[correction interface](../tools/README.md#ledger-comparison-and-directed-corrections).
The checker cannot authenticate an award or its authority. Without a previous
tally, it cannot detect a reset. Neither mode claims a complete event history.

