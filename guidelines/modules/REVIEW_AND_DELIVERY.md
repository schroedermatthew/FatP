# Review and delivery

Applies to reviews and completed-work reports. The core defines scope and evidence
rules; this module defines a useful, auditable output.

## Review the actual change

Read enough current source and context to establish the behavior: callers,
contracts, configuration, relevant tests, and intentional local policy. Re-check
stale findings before repeating them. Do not infer that a component is absent
because it was not included in an excerpt.

Prioritize by concrete impact and reachable conditions. Separate defects from
optional improvements and unresolved hypotheses. Do not pad a review to meet a
finding count or invent requirements from a reviewer's preferences. Violations of
the mandatory [C++ style guide](../cpp/STYLE.md) are conformance defects, not
optional aesthetic suggestions. Identify the actual violated rule.

Each actionable finding should identify:

- The location and relevant source evidence.
- The input, state, or configuration that triggers the problem.
- The resulting incorrect behavior or violated contract and why it matters.
- A focused repair direction and, where useful, a reproduction or counterexample.
- What was verified and what remains inferred.

If there are no supported findings, say so and state the review's coverage limits.
Reviews may suggest targeted patches; applying them requires authorization through
the task's scope. Do not rewrite whole files simply to provide a review.

If independent reviews are available, resolve disagreements using source evidence
and experiments, not a vote. Verify adopted findings yourself. This rule does not
require another reviewer, an AI service, or delegation.

Record confirmed assistant violations through [DEMERITS.md](../DEMERITS.md).
Repairing a violation does not erase its award; routine violations need no event log.
Do not pad the ledger with hypotheses or use an unconfirmed finding as an award.

## Deliver the requested work

Inspect the final diff or file set for unintended changes, missing files,
truncation, generated noise, and sensitive content. Preserve unrelated work.

Lead the delivery report with the outcome and why the change was needed. Include
the files or artifacts actually changed, the checks actually performed, and any
material failures, missing verification, or remaining work. Use concrete paths or
links. Do not claim the repository is clean without checking it.

If a demerit was assigned or a confirmed violation was self-reported during the
work, state the category/count change and repair status concisely in the delivery. Do not
silently omit unfinished terms or present a corrected wrong turn as if it never
happened. Keep this report factual and concise.

Use the artifact form the task needs: repository edits, a patch, complete files,
or a package. Patches must be applicable to the stated source state; complete-file
deliveries must not replace necessary content with ellipses. If packaging files,
ensure the manifest and archive contents match. Do not invent changes to satisfy
a delivery format when no file was modified.

Source comments should explain contracts and decisions, not narrate an assistant's
editing process with labels such as "fixed by AI" or "new change here."

## Root-cause litmus and magnitude

After a repair ask: if the owner does not push back, does a known in-scope structural
defect remain that could have been removed? If so, the repair is incomplete. A
larger diff alone does not justify a weaker fix. A requested tactical mitigation or
missing authority/information is recorded with its remaining defect, never sold as
full repair. Fix the assigned issue and report adjacent discoveries without silently
expanding scope. Verify the proposed cause accounts for the full observed effect;
a small recovered fraction of a performance/error delta is not a closed investigation.

## Complete artifacts and manifest

Implementation delivers complete files on disk, without ellipses or omitted sections;
it does not require pasting an entire repository into chat. Review remains findings
and proposed targeted patches, never an unsolicited rewrite. End a change set with
`Modified Files (N)` listing exactly the changed files and their purpose; a linked
complete manifest is appropriate for a large set. No changes means say so, with no
invented manifest. Archives contain only authorized delivery files.

State corrected wrong turns and remaining requirements. Attribute who proposed,
corrected and ratified a decision accurately. Do not convert assistant consensus
into owner instruction. No NEW/FIXED/CHANGED source comments. Evidence supporting a
finding includes current file/line, a quotation or exact output, and the counterexample
or violated rule; a hypothesis is labeled and is not packaged as a verified defect.
