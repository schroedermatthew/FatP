# Build, CI, and delivery workflow

Applies to build, CI, packaging, and release procedures. Obtain commands, supported
configurations, required gates, and authorization from the profile and the live
repository. This module does not prescribe a branch model or a CI provider.

## Establish the working state

- Identify the repository root, revision or source snapshot, current branch when
  applicable, and existing changes. Preserve unrelated work and generated outputs
  whose ownership is uncertain.
- Inspect the actual build and workflow definitions. Check that referenced scripts,
  presets, targets, inputs, and dependencies exist before relying on a recipe.
- Use the stated working directory, shell, and environment. Keep machine-specific
  setup in local configuration; reusable instructions should name prerequisites
  without embedding a person's absolute paths or credentials.
- Use an isolated workspace when the required experiment would disturb unrelated
  work. Do not reset, delete, switch away from, or overwrite that work as cleanup.

## Run and interpret gates

- Local checks and CI are distinct evidence sources. Run the checks the profile
  requires; do not automatically declare one a substitute for the other or invent
  a requirement for CI that the project has not adopted.
- Tie results to the revision and configuration actually checked. A prior green
  run is not proof for files changed since that run.
- If a stage is unavailable, record the blocker and the narrower evidence obtained.
  An unavailable job, empty test run, or skipped required stage is not a pass.
- Reproduce the first relevant failure, investigate its cause, apply a coherent
  repair, and rerun the affected checks and required acceptance gate. Do not push
  speculative fixes merely to use CI as an unexamined feedback loop.

## Author or change automation

- Derive the matrix from the support contract. Distinguish required configurations,
  optional experiments, scheduled checks, and manually invoked work.
- Make aggregate gates depend on every result they inspect. Define the treatment
  of failures, cancellations, skips, and conditional jobs explicitly. A required
  skipped job must not silently become success.
- Preserve process exit status through pipelines, logging, summaries, and wrappers.
  Expected-failure checks follow the intended-failure rule in
  [Testing](TESTING.md).
- A claimed enforcement check must validate the property it advertises. Unknown
  schema values, missing mandatory inputs, or unrecognized categories must produce
  a diagnostic rather than silently bypass enforcement.
- Keep generated workflows aligned with their generator. Scope caches to relevant
  inputs and configurations, and do not let cached artifacts hide missing inputs.
- Use minimum necessary permissions, appropriate secret boundaries, and controlled
  dependency/tool versions. Untrusted contributions must not acquire privileged
  execution through workflow changes or uncontrolled inputs.
- Validate workflow structure and representative commands after edits. Prefer a
  link to the executable source over another complete copied recipe in prose.

## Deliver through the adopted process

Follow the profile's commit, review, merge, and release rules within existing
authorization. Include the change's purpose and verification evidence in the
relevant delivery record. Do not conflate a local edit, a commit, a push, a release,
and a deployment; report only the stages actually completed.

## Required guideline and style gates

Commands below run from the receiving repository root. If guidelines are under
`docs/guidelines`, change that argument only; keep --repo-root at the actual root.

| Change | Required check |
|---|---|
| Guidelines/profile adoption or edits | `python tools/lint_guidelines.py guidelines --repo-root . --instantiated` |
| Guideline/style/metadata checker edits | Checker regression suite plus valid/invalid probes; see tools README |
| Authored C++ | `python tools/check_style.py --repo-root . --guidelines guidelines --inventory -- REAL_BUILD_FLAGS` plus semantic review |
| Metadata-covered code or metadata changes | `python tools/lint_metadata.py --repo-root . --guidelines guidelines` |
| Existing demerit ledger changes | Guideline lint with `--previous-ledger PRE_CHANGE_COPY` and owner-evidence review |

Run the project's actual compile, link and test commands in addition to style.
No shipped CI YAML or product build system is implied. Local and configured CI
gates are co-equal required evidence; if CI is absent, state that local is the only
available gate. Required CI that has not run is not replaced by local success.

Run details and test counts have one current home:
[CURRENT_VERIFICATION](../CURRENT_VERIFICATION.md). Dated reports may be retained
with superseded status; one current authority does not require deleting history.

Guideline-only adoption is a dedicated change set, not an opportunity to alter
product code or unrelated build configuration. This template does not require a
commit or remote write without task authorization.
