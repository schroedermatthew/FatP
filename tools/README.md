# Fat-P maintainer and guideline tools

Python 3.10 or later, PyYAML 6.x and markdown-it-py 4.x are required. Install the declared dependencies
in a virtual environment with `python -m pip install -r tools/requirements.txt`. Formatting requires a
clang-format version compatible with the supplied configuration (validated on
22.1.8); AST naming requires Clang's C++ driver with JSON AST support. Actual
compiler/build compatibility is the receiving project's contract, not this version.

The guideline checkers are read-only. Exit 0 means the stated automated checks passed, exit 1
means a finding or unavailable prerequisite; CLI argument errors may return 2.
No tool executes command strings from the project profile.
Each supplied build-command record requires nonblank strings for `name`, `command`,
`cwd`, `property`, `when`, and `evidence`, in both template and instantiated modes.

## Commands (receiving repository root)

- `python tools/lint_guidelines.py guidelines --repo-root . --instantiated`
- `python tools/check_style.py --repo-root . --guidelines guidelines --inventory -- -Iinclude`
- `python tools/lint_metadata.py --repo-root . --guidelines guidelines`

For `docs/guidelines`, change the guidelines argument. The root formatter stays
at repository root. Supply actual compiler include paths/defines after `--`, and
repeat checks for relevant configurations. `--file path` checks selected files;
AST checks do not automatically validate included headers as independent files.
Use inventory mode for all owned C++ files and separate composition/build gates.

The metadata checker visits its complete adopted inventory by default, including
when `--file` adds explicit paths. For a deliberately scoped check, use
`python tools/lint_metadata.py --repo-root . --guidelines guidelines --selected-only --file PATH`.
`--selected-only` requires at least one file, retains profile/coverage validation,
and requires metadata on every selected covered file. An existing block outside
coverage is still checked. The output identifies the subset; this option does not
replace the full inventory gate or imply that unselected files conform.

Style and metadata checks accept `.h`, `.hpp`, `.hh`, `.cpp`, `.cc`, `.cxx`,
`.cppm`, and `.ixx`. For named modules, build the required Clang module dependencies
first and supply their mappings after `--`, such as
`-fmodule-file=example=build/example.pcm`. The style checker does not build module
dependencies; missing imports fail its syntax gate. Module metadata follows source
placement: before any module declaration, imports, includes, or other code.

## Coverage contract

### Referenced profile data

Profile schema 2 replaces the inline `cpp.component_headers` map with
`cpp.component_headers_file`, a forward-slash path relative to `--repo-root`.
The referenced JSON file contains the single source-path-to-header-spelling map;
schema 1 and the old inline field are rejected. Configured C++ requires the file
reference even for an empty map. Non-C++ or unconfigured profiles may use null.

All three validator commands load and validate this data through the shared
profile check. Missing or malformed files, duplicate JSON keys, non-object data,
paths escaping the repository, and invalid mapping entries fail; there is no
fallback to an empty map. Source keys must be canonical paths in the authored
C++ source inventory. Header values must be nonblank relative include spellings
that identify an authored header, either relative to the source or by an inventory
path suffix. Parent-relative includes are allowed when their resolved target stays
inside the repository. The style checker reuses one mapping throughout the file
traversal after the shared profile check.

Header existence does not prove compiler include-search resolution or ownership
when multiple headers share a spelling. Those properties, and whether every
source needing a mapping has one, remain part of inventory review. Moving the
data out of onboarding does not weaken the corresponding-header-first rule or
the mandatory full-inventory delivery gate. Changing the mapping reference or
data requires the corpus check and applicable style checks.

### Automated and manual checks

| Mechanism | Automated | Required review or external evidence |
|---|---|---|
| Corpus | Required documents, parsed Markdown link/image/reference destinations and local heading anchors, index/trigger coverage, a single onboarding block in the required order, protected policy present as live prose, profile keys/core types, tokens. Code specimens supply no links, headings or policy statements | Meaning of rules, adequacy of triggers, raw HTML/custom-renderer anchors, external URL reachability, non-link prose references, archive dormancy, stale numeric prose, architectural ratification |
| Formatter | Effective LLVM/Allman/four-space/width/pointer/braces/order configuration; selected-file output comparison | Semantic effect of inserted braces, macro formatting, generated/vendor boundaries |
| Naming | Clang AST lexical names: types/functions/variables/templates/enums/aliases, aggregate vs class fields, static and constexpr precedence. Operator functions are exempted by their actual operator spelling, not by a name prefix | Actual protocol/ABI exception justification, useful names, semantic invariant-bearing aggregates, inactive preprocessing configurations |
| Include order | First include for profile-mapped source/test files | Completeness of mappings, alphabetical order inside groups, architecture layering, real prerequisites |
| Header/macros | Pragma once, using-directive prohibition, prefixed macro spelling, non-macro hard width | Macro lifetime/cleanup, raw-string/preprocessor edge cases, ODR/lookup correctness |
| Demerits | Unique assistant/category rows, nonnegative integer counts, column totals, no active incident fences; category/assistant preservation and decrease detection against a previous tally; exact directed-correction matching | Authentic awards and correction authority, complete attribution, unresolved-work visibility, load-bearing case selection; no event-history reconstruction |
| Metadata | Adoption/coverage, YAML parse and duplicate/alias rejection, keys/types/order, comment placement/safety, prefix/path/enums/related paths, known hygiene counts excluding literal contents, Python tokenize literals including FSTRING/TSTRING tokens when present, CMake quoted and bracket arguments | Taxonomy meaning, generated-tool provenance/recomputation beyond known counts, platform-header classification, shell/PowerShell here-strings and block comments |

No formatter/AST/linter pass proves full style compliance or architecture. The
manual column is part of the gate, not a list of optional improvements. A lexical
protocol allowlist permits required spellings but cannot prove they implement a
standard interface. Override names are preserved at their ABI boundary and reviewed.
Semantic exceptions are not a route for unrelated authored code to adopt another dialect.

Markdown is parsed as CommonMark with tables and strikethrough enabled. Fenced,
indented and inline code are excluded; reference links and images are resolved by
the parser. Heading IDs use rendered inline text and GitHub-like unique slugs,
including setext headings. A link diagnostic's line is the start of its containing
inline block, not necessarily the exact line of a link in a multiline paragraph.
Raw HTML anchors and renderer-specific ID conventions still require manual review.
Policy checks exclude inline code and image alt text without joining prose across
those boundaries. Emphasis, links, and line breaks within live prose remain valid;
heading IDs still include rendered inline code text.

The AST checker inspects declarations in the selected source file, in one supplied
configuration. It uses source locations to exclude implicit declarations and every
declaration an included file contributes, including text included inside a function
body, whose names belong to the file that declares them. It does not replace multi-TU builds, template instantiation coverage,
linking, execution or sanitizer checks. Compiler diagnostics fail the AST gate.
Static data members follow s naming; function-local statics follow camelCase,
and constexpr constants take k precedence. Metadata
uses its own two-space YAML indentation inside comments, not C++ code indentation.
Explicit variable-template specializations represented by Clang's
`VarTemplateSpecializationDecl` are not currently classified by the naming pass.
Review those selected-file declarations manually; checking the primary template
in a separately inventoried header does not establish the specialization's own
conformance. Diagnostic byte-offset fallback uses the original file bytes so
CRLF, UTF-8 text and a UTF-8 BOM do not shift reported physical source lines.
Metadata extraction distinguishes C++ comments from ordinary/raw string and character
literals. Python metadata uses the standard tokenizer (STRING and, on 3.12+,
FSTRING_START/MIDDLE/END, plus TSTRING_START/MIDDLE/END when available), so escaped
triple quotes, interpolations and Python 3.14 template strings stay literals.
CMake metadata uses CMake quoted arguments and `[==[ ... ]==]` bracket arguments;
a lone `[` is not a bracket. Shell and PowerShell still use a generic quote matcher.
A C++ line comment ending in a backslash continues onto the next line as the compiler
reads it, so a directive spliced into a comment is comment text. Shell/PowerShell
here-strings and block comments remain a review concern. Hygiene counts cover written directives, including inactive branches;
they do not represent expanded preprocessor output. Preprocessor edge cases remain
subject to the manual review above.

## Maintaining checks

The receiving regression suite is under `tools/tests`, with validator controls
ported from GuidelinesTemplate4 and isolated Git histories exercising the actual
workflow's ledger-selection Python. Run
`python -m unittest discover -s tools/tests -v` from the repository root.
Set `FATP_GUIDELINE_FORMATTER` and `FATP_GUIDELINE_CLANG` when executables are not
on PATH. Tests invoke actual formatter and Clang processes; missing prerequisites
fail rather than skip. The suite covers the adopted checker changes and selected
schema, formatting, naming, metadata, and process-failure boundaries. It is not
an assertion that the complete upstream package matrix ran for this repository.

Inventory checks continue after an individual file's tool timeout or launch
failure, retain its diagnostic, and still exit with failure. This prevents a
blocked file from concealing the unvisited remainder. Metadata reports the first
confirmed violation in each file; a report over every file is not an exhaustive
list of every latent violation within each file.

## Ledger comparison and directed corrections

Guidelines CI obtains the previous ledger from Git, including an earlier ancestor
if the ledger was deleted. It needs no saved project-history file. When no prior
ledger exists, CI reports that comparison is unavailable and validates the current
tally; it does not manufacture a retention pass. Invalid revisions and unreadable
existing ledger blobs fail the job.

The category table is authoritative. A routine award updates its cell and total;
no event object or narrative is required. For an existing ledger, use its actual
pre-change snapshot with `--previous-ledger PATH`. The checker rejects removed
assistant/category rows and unexplained decreases. It compares by labels, not column
position. A legacy snapshot may contain old incident entries; they are not copied
into the compact current ledger. Preserve its tally and any load-bearing obligations.

Only an explicitly directed correction needs `--ledger-corrections PATH`, a separate
JSON evidence file retained outside onboarding. It requires the previous snapshot.
It is not a log of ordinary awards or a permission switch. Schema:

```json
{
  "schema_version": 1,
  "corrections": [
    {
      "date": "2026-08-30",
      "assistant": "ChatGPT",
      "category": "D03",
      "before": 1,
      "after": 0,
      "direction": "Synthetic specimen: replace with the actual owner instruction source",
      "reason": "Synthetic specimen: replace with the factual correction reason"
    }
  ]
}
```

Each correction must match the exact prior/current cell, name an actual direction
and reason, and have a valid ISO date and distinct nonnegative integer counts.
Duplicate, stale, unknown-field, or unrelated records fail. Re-attribution preserves
the old columns, adjusts the affected cells/totals, and records both changed cells.
The specimen authorizes nothing. The checker cannot authenticate its text or prove
history without a prior tally. Review that evidence; do not reduce awards on your own.

Fresh-context acceptance is a separate procedure in the adopted guidelines, not a
property these structural checks can establish. CORE routes to FRESH_CONTEXT_REVIEW.


# Existing maintainer workflows

PowerShell and Python utilities for local builds, CI generation, and **Desktop sync** after agent/worktree pushes.

This section describes the optional **commit → push → sync** helper scripts. For benchmarks see [`BENCHMARK_RUNNER_README.md`](BENCHMARK_RUNNER_README.md). For header self-containment CI see [`HEADER_SELF_CONTAINMENT_CI.md`](HEADER_SELF_CONTAINMENT_CI.md).

---

## Desktop sync workflow

Fat-P development often happens in **agent worktrees** or secondary clones (Cursor, Grok, CI sandboxes). The **canonical local tree** used day to day is the Desktop clone:

```
C:\Users\mtthw\Desktop\AI Projects\FatP
```

These helpers run only when explicitly invoked within the session's authorized
scope. They do not establish a required branch model, authorize a push/reset, or
require every feature-branch push to synchronize the Desktop checkout.

Both combined wrappers always invoke a sync to `origin/main`, even when their
push arguments name another branch. Inspect the intended destination and working
state before choosing a wrapper. The profile and current session own publication
authority; these descriptions explain existing script behavior.

### Scripts

| Script | What it does |
|--------|----------------|
| [`commit-push-sync-desktop.ps1`](commit-push-sync-desktop.ps1) | Combined helper: `git add` → `git commit` → `git push` → sync Desktop to `origin/main` |
| [`push-and-sync-desktop.ps1`](push-and-sync-desktop.ps1) | `git push` then sync Desktop (commit already done) |
| [`sync-desktop-fatp.ps1`](sync-desktop-fatp.ps1) | Sync only: `git fetch origin main` + fast-forward Desktop |
| [`install-git-hooks.ps1`](install-git-hooks.ps1) | Copies custom hook templates; does not create a supported automatic after-push event |

All sync scripts are PowerShell. Run them from the **Fat-P repo root** (any clone or worktree).

---

## `commit-push-sync-desktop.ps1`

Stages files, commits with your message, pushes to `origin main` (by default), then runs `sync-desktop-fatp.ps1`.

### Examples

```powershell
# Stage modified tracked files only (git add -u), commit, push, sync
.\tools\commit-push-sync-desktop.ps1 -Message "Harden XmlLite close-tag parsing"

# Stage specific paths (comma-separated list is OK)
.\tools\commit-push-sync-desktop.ps1 -Message "Fix parser" `
    -Paths include/fat_p/XmlLite.h,components/Xml/tests/test_XmlLite.cpp

# Stage everything, including new untracked files (git add -A)
.\tools\commit-push-sync-desktop.ps1 -Message "Add FatPXml header" -All

# Push to a different remote or branch
.\tools\commit-push-sync-desktop.ps1 -Message "Topic branch work" origin my-feature-branch
```

### Staging rules

| Flag | `git add` behavior |
|------|-------------------|
| *(default)* | `-u` — only **tracked** files that changed or were deleted |
| `-Paths a,b,c` | Explicit paths only; comma-separated entries in one `-Paths` argument are split automatically |
| `-All` | `-A` — all changes including **new untracked** files |

`-All` and `-Paths` cannot be combined. If nothing is staged after the add step, the script exits with an error (use `-All` or `-Paths` to include new files).

### Parameters

| Parameter | Required | Default | Purpose |
|-----------|----------|---------|---------|
| `-Message` | yes | — | Commit message (single string) |
| `-Paths` | no | — | Limit what gets staged |
| `-All` | no | off | Stage entire working tree |
| trailing args | no | `origin main` | Passed to `git push` (remote and branch) |

### Execution order

1. `git add` (per staging rules above)
2. Check repository status; `git commit` still rejects an empty staged change set
3. `git commit -m <Message>`
4. `git push <remote> <branch>` (default `origin main`)
5. `sync-desktop-fatp.ps1`

Any step failure stops the script with a non-zero exit code.

---

## `push-and-sync-desktop.ps1`

Use when the commit already exists locally and you only need to publish and sync.

```powershell
.\tools\push-and-sync-desktop.ps1
.\tools\push-and-sync-desktop.ps1 origin main
```

Trailing arguments are forwarded to `git push` (default `origin main`).

---

## `sync-desktop-fatp.ps1`

Fetches `origin/main` and fast-forwards the Desktop clone. Safe to run manually after any push that did not go through the combined scripts.

```powershell
.\tools\sync-desktop-fatp.ps1
.\tools\sync-desktop-fatp.ps1 -Force
```

### Sync logic

1. Resolve Desktop path (see [Desktop path](#desktop-path) below).
2. `git fetch origin main` on that path.
3. Compare Desktop `HEAD` to `origin/main`.
4. **Already current:** if SHAs match, print the short SHA and exit **0** — even when Desktop has other uncommitted edits. This is intentional: committing from Desktop itself leaves `HEAD` aligned with `origin/main` after push while unrelated files (e.g. local workflow churn) may still be dirty.
5. **Behind and clean:** `git merge --ff-only origin/main`.
6. **Behind and dirty:** abort with local/remote SHAs. Commit or stash on Desktop, or use `-Force`:

```powershell
.\tools\sync-desktop-fatp.ps1 -Force   # git reset --hard origin/main
```

### Desktop path

| Source | Path |
|--------|------|
| Default | `C:\Users\mtthw\Desktop\AI Projects\FatP` |
| Override | `$env:FATP_DESKTOP_PATH` |

```powershell
$env:FATP_DESKTOP_PATH = 'D:\repos\FatP'
.\tools\sync-desktop-fatp.ps1
```

If the Desktop path does not exist or is not a git repository, sync prints a warning and exits 0 (no-op).

---

## `install-git-hooks.ps1`

The existing helper copies [tools/git-hooks/post-push](git-hooks/post-push) into
`.git/hooks`; it does not establish that the file will run. Standard Git's
[supported hooks](https://git-scm.com/docs/githooks) include `pre-push` and server
receive hooks, but no client `post-push` event. A file with that custom name is not
automatically invoked by a normal `git push`.

The copied script can invoke the PowerShell sync helper when separately executed.
Its presence does not establish an installed hook or external runner. Use an
explicit combined wrapper only when its `origin/main` sync behavior is intended
and authorized. The installer also assumes `.git` is a directory; its handling
of worktree Git files and custom `core.hooksPath` has not been established.

---

## Typical workflows

### Authorized main-branch commit, push and Desktop sync

```powershell
.\tools\commit-push-sync-desktop.ps1 -Message "Add FatPXml named_enum from_xml" `
    -Paths include/fat_p/FatPXml.h,components/Xml/tests/test_FatPXml.cpp
```

### Authorized main-branch push after a commit

```powershell
git commit -m "Fix XmlLite EOF handling"   # if not done yet
.\tools\push-and-sync-desktop.ps1
```

### Desktop-only edit

Commit and push from Desktop as usual. Sync is a no-op if `HEAD` already matches `origin/main` after push. Running `.\tools\sync-desktop-fatp.ps1` anyway is harmless.

### Recover Desktop after a forced remote update

```powershell
.\tools\sync-desktop-fatp.ps1 -Force
```

---

## What these scripts do not do

- They do **not** run tests or CI. Build and test before committing.
- They do **not** replace GitHub Actions. Local sync and CI are separate gates.
- They do **not** merge divergent histories. Only fast-forward to `origin/main` (or hard reset when `-Force` is used with a dirty checkout).
- `commit-push-sync-desktop.ps1` does **not** amend, rebase, or create branches — it is a straight add/commit/push/sync path for `main`-line work.

---

## Other scripts in this folder (brief)

| Script | Role |
|--------|------|
| `build.ps1` / `build.sh` / `build.bat` | Local CMake build helpers |
| `rebuild.ps1` / `rebuild.sh` / `rebuild.bat` | Clean rebuild |
| `generate_workflows.py` | Regenerate `.github/workflows/*.yml` from component metadata |
| `validate_layers.py` | Layer dependency verification |
| `check_header_self_containment.py` | Header isolation checks |
| `fatp_meta_parser.py` / `fatp_meta_inventory.py` | `FATP_META` tooling |
| `run_all_benchmarks.ps1` | Local benchmark runner |
