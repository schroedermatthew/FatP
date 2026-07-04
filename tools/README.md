# Fat-P `tools/` — maintainer scripts

PowerShell and Python utilities for local builds, CI generation, and **Desktop sync** after agent/worktree pushes.

This document focuses on the **commit → push → sync** workflow. For benchmarks see [`BENCHMARK_RUNNER_README.md`](BENCHMARK_RUNNER_README.md). For header self-containment CI see [`HEADER_SELF_CONTAINMENT_CI.md`](HEADER_SELF_CONTAINMENT_CI.md).

---

## Desktop sync workflow

Fat-P development often happens in **agent worktrees** or secondary clones (Cursor, Grok, CI sandboxes). The **canonical local tree** used day to day is the Desktop clone:

```
C:\Users\mtthw\Desktop\AI Projects\FatP
```

A worktree can commit and push to `origin/main` while Desktop still points at an older commit. The sync scripts close that gap: after every push, **fast-forward Desktop to `origin/main`** so the folder on disk matches what GitHub has.

This mirrors SandBox's `ulib/scripts/sync-desktop-sandbox.ps1`, adapted for Fat-P.

### Scripts

| Script | What it does |
|--------|----------------|
| [`commit-push-sync-desktop.ps1`](commit-push-sync-desktop.ps1) | **Primary workflow:** `git add` → `git commit` → `git push` → sync Desktop |
| [`push-and-sync-desktop.ps1`](push-and-sync-desktop.ps1) | `git push` then sync Desktop (commit already done) |
| [`sync-desktop-fatp.ps1`](sync-desktop-fatp.ps1) | Sync only: `git fetch origin main` + fast-forward Desktop |
| [`install-git-hooks.ps1`](install-git-hooks.ps1) | Install `post-push` hook so **every** `git push` auto-syncs Desktop |

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
2. Abort if the index is empty
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

Run **once per clone/worktree** that performs pushes:

```powershell
.\tools\install-git-hooks.ps1
```

Copies [`git-hooks/post-push`](git-hooks/post-push) into `.git/hooks/post-push`. After every successful `git push`, the hook invokes `sync-desktop-fatp.ps1` via PowerShell so Desktop stays current even when you skip the manual sync step.

The hook is a thin shell script: it resolves the repo root, locates `tools/sync-desktop-fatp.ps1`, and runs it with `powershell.exe` (or `pwsh` if PowerShell is unavailable).

---

## Typical workflows

### Agent / worktree finishes a task

```powershell
.\tools\commit-push-sync-desktop.ps1 -Message "Add FatPXml named_enum from_xml" `
    -Paths include/fat_p/FatPXml.h,components/Xml/tests/test_FatPXml.cpp
```

### Already committed on a worktree

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
- They do **not** merge divergent histories. Only fast-forward to `origin/main` (or hard reset with `-Force`).
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