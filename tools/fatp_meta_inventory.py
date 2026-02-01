#!/usr/bin/env python3
# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/fatp_meta_inventory.py
#   summary: "Inventory tool to report missing or inconsistent FATP_META blocks."
#   api_stability: in_work
#   layer: Infrastructure
#   related:
#     docs_search: ""
#     tests: []
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false

# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/fatp_meta_inventory.py
#   layer: Testing
#   summary: "Repository-wide FATP_META inventory and validation (stdlib-only)."
#   api_stability: internal
#   related:
#     docs_search: "FATP_META"
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false
#   generated:
#     by: fatp-meta-tool
#     mode: authored
#
"""fatp_meta_inventory.py

Repository-wide FATP_META inventory and validation.

This tool is intentionally stdlib-only (no PyYAML dependency) and performs:
- discovery of in-scope "code files"
- FATP_META presence checks
- shallow schema checks (required top-level keys, meta_version)
- layout checks (header: #pragma once before FATP_META when present)
- path checks (FATP_META.path equals repo-relative path)

Exit codes:
- 0: all checks passed (or --inventory without --fail-on-issues)
- 1: issues found and failure requested
- 2: tool usage/config error

Typical usage:
  python tools/fatp_meta_inventory.py --root . --json-out meta_report.json --fail-on-issues
  python tools/fatp_meta_inventory.py --root . --print-missing --print-path-mismatch
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# --------------------------------------------------------------------------------------
# Configuration (kept in-code to avoid external deps/config drift).
# --------------------------------------------------------------------------------------

# Directories that define the scan scope, relative to repo root.
K_SCOPE_DIRS = (
    "include/fat_p",
    "components",
    "cmake",
    "tools",
    "tooling",
)

# Individual files at repo root that are in scope.
K_SCOPE_ROOT_FILES = (
    "CMakeLists.txt",
)

# File extensions considered "code files" for metadata purposes.
K_CODE_EXTS = (
    ".h", ".hpp", ".inl",
    ".cpp", ".cc", ".cxx",
    ".cmake",
    ".py", ".sh", ".ps1",
)

# Extensions explicitly excluded from metadata requirements.
K_EXCLUDED_EXTS = (
    ".yml", ".yaml",
)

# Directory name exclusions anywhere in the path (repo-relative, POSIX-style).
K_EXCLUDED_DIR_NAMES = (
    "ThirdParty",
    ".github",
    "build",
    ".vcpkg_installed",
    ".git",
)

# Additional path-pattern exclusions (regex against repo-relative posix path).
K_EXCLUDED_PATH_PATTERNS = (
    r"(^|/)results(/|$)",   # artifacts
    r"(^|/)out(/|$)",       # common build outputs
    r"(^|/)dist(/|$)",      # common build outputs
)

# Required keys (top-level) for all files.
K_REQUIRED_KEYS_COMMON = (
    "meta_version",
    "component",
    "file_role",
    "path",
    "layer",
    "summary",
    "api_stability",
)

# Required keys for C/C++ files (top-level).
K_REQUIRED_KEYS_CPP = (
    "namespace",
    "hygiene",
)

# --------------------------------------------------------------------------------------
# Extraction helpers
# --------------------------------------------------------------------------------------

_RE_BLOCK = re.compile(r"/\*[\s\S]*?FATP_META:\s*\n([\s\S]*?)\*/", re.MULTILINE)
_RE_LINE_META = re.compile(r"^\s*(?P<prefix>//|#)\s*FATP_META:\s*$")

_RE_TOP_KEY = re.compile(r"^(?P<indent>\s*)(?P<key>[A-Za-z_][A-Za-z0-9_]*)\s*:\s*(?P<rest>.*)$")

@dataclass
class FileIssue:
    code: str
    message: str

@dataclass
class FileReport:
    path: str  # repo-relative POSIX
    ext: str
    has_meta: bool
    meta_version: Optional[int]
    declared_path: Optional[str]
    top_level_keys: List[str]
    issues: List[FileIssue]

@dataclass
class SummaryReport:
    root: str
    files_scanned: int
    files_in_scope: int
    missing_meta: int
    parse_fail: int
    schema_fail: int
    path_mismatch: int
    layout_fail: int
    meta_version_fail: int


def _to_posix_relpath(root: Path, p: Path) -> str:
    rel = p.relative_to(root)
    return rel.as_posix()


def _is_excluded(rel_posix: str) -> bool:
    # Excluded dirs by name.
    parts = rel_posix.split("/")
    for part in parts:
        if part in K_EXCLUDED_DIR_NAMES:
            return True

    # Excluded patterns.
    for pat in K_EXCLUDED_PATH_PATTERNS:
        if re.search(pat, rel_posix):
            return True

    # Excluded extensions.
    lower = rel_posix.lower()
    for ext in K_EXCLUDED_EXTS:
        if lower.endswith(ext):
            return True

    return False


def _is_code_file(p: Path) -> bool:
    if p.name == "CMakeLists.txt":
        return True
    return p.suffix in K_CODE_EXTS


def _collect_files(root: Path) -> List[Path]:
    files: List[Path] = []

    for rel_dir in K_SCOPE_DIRS:
        d = root / rel_dir
        if not d.exists():
            continue
        for p in d.rglob("*"):
            if not p.is_file():
                continue
            rel = _to_posix_relpath(root, p)
            if _is_excluded(rel):
                continue
            if not _is_code_file(p):
                continue
            files.append(p)

    for rel_file in K_SCOPE_ROOT_FILES:
        p = root / rel_file
        if p.exists() and p.is_file():
            rel = _to_posix_relpath(root, p)
            if not _is_excluded(rel) and _is_code_file(p):
                files.append(p)

    # De-dup and sort deterministically.
    unique = {p.resolve(): p for p in files}
    return [unique[k] for k in sorted(unique.keys(), key=lambda x: str(x))]


def _extract_meta_block(content: str) -> Tuple[Optional[str], Optional[str]]:
    # Block comment.
    m = _RE_BLOCK.search(content)
    if m:
        return m.group(1), None

    # Line comment: find sentinel line and consume subsequent comment lines with same prefix.
    lines = content.splitlines()
    for i, line in enumerate(lines):
        m2 = _RE_LINE_META.match(line)
        if not m2:
            continue
        prefix = m2.group("prefix")
        yaml_lines: List[str] = []
        for j in range(i + 1, len(lines)):
            ln = lines[j]
            if re.match(r"^\s*%s" % re.escape(prefix), ln):
                # Strip prefix and a single following space if present.
                stripped = re.sub(r"^\s*%s\s?" % re.escape(prefix), "", ln)
                yaml_lines.append(stripped.rstrip())
            else:
                break
        return "\n".join(yaml_lines), None

    return None, "No FATP_META block found"


def _normalize_yaml_block(raw: str) -> str:
    # Keep this conservative: normalize tabs, strip trailing ws, dedent.
    raw = raw.replace("\t", "  ")
    lines = [ln.rstrip() for ln in raw.splitlines()]
    # Remove leading '*' used in some block comment styles.
    lines = [re.sub(r"^\s*\*\s?", "", ln) for ln in lines]
    # Dedent using minimal common indentation across non-empty lines.
    non_empty = [ln for ln in lines if ln.strip()]
    if non_empty:
        indents = [len(re.match(r"^\s*", ln).group(0)) for ln in non_empty]
        min_indent = min(indents)
        lines = [ln[min_indent:] if len(ln) >= min_indent else ln for ln in lines]
    return "\n".join(lines).strip()


def _scan_top_level_keys(yaml_block: str) -> Dict[str, str]:
    keys: Dict[str, str] = {}
    for ln in yaml_block.splitlines():
        m = _RE_TOP_KEY.match(ln)
        if not m:
            continue
        indent = m.group("indent")
        if len(indent) != 0:
            continue
        key = m.group("key")
        keys[key] = m.group("rest").strip()
    return keys


def _parse_meta_version(value: str) -> Optional[int]:
    # Accept: 1, "1"
    if not value:
        return None
    v = value.strip().strip('"').strip("'")
    if not v.isdigit():
        return None
    return int(v)


def _layout_check_header(content: str) -> Optional[str]:
    # If header contains #pragma once, ensure it appears before FATP_META.
    pragma_idx = content.find("#pragma once")
    meta_idx = content.find("FATP_META:")
    if pragma_idx != -1 and meta_idx != -1 and meta_idx < pragma_idx:
        return "Header contains #pragma once but FATP_META appears before it"
    return None


def _required_keys_for_file(ext: str) -> Tuple[Tuple[str, ...], Tuple[str, ...]]:
    common = K_REQUIRED_KEYS_COMMON
    cpp = ()
    if ext in (".h", ".hpp", ".inl", ".cpp", ".cc", ".cxx"):
        cpp = K_REQUIRED_KEYS_CPP
    return common, cpp


def analyze_file(root: Path, p: Path) -> FileReport:
    rel = _to_posix_relpath(root, p)
    ext = p.suffix if p.name != "CMakeLists.txt" else "CMakeLists.txt"
    issues: List[FileIssue] = []

    try:
        content = p.read_text(encoding="utf-8", errors="replace")
    except Exception as e:
        return FileReport(
            path=rel,
            ext=ext,
            has_meta=False,
            meta_version=None,
            declared_path=None,
            top_level_keys=[],
            issues=[FileIssue("read_fail", f"Failed to read file: {e}")],
        )

    if p.suffix in (".h", ".hpp", ".inl"):
        msg = _layout_check_header(content)
        if msg:
            issues.append(FileIssue("layout", msg))

    raw, err = _extract_meta_block(content)
    if err:
        return FileReport(
            path=rel,
            ext=ext,
            has_meta=False,
            meta_version=None,
            declared_path=None,
            top_level_keys=[],
            issues=issues + [FileIssue("missing_meta", err)],
        )

    yaml_block = _normalize_yaml_block(raw)
    if not yaml_block:
        return FileReport(
            path=rel,
            ext=ext,
            has_meta=True,
            meta_version=None,
            declared_path=None,
            top_level_keys=[],
            issues=issues + [FileIssue("parse", "FATP_META block empty after normalization")],
        )

    top = _scan_top_level_keys(yaml_block)
    top_keys = list(top.keys())

    # meta_version
    mv = _parse_meta_version(top.get("meta_version", ""))
    if mv != 1:
        issues.append(FileIssue("meta_version", f"meta_version is {mv}, expected 1"))

    # Required keys
    required_common, required_cpp = _required_keys_for_file(p.suffix)
    missing = [k for k in required_common if k not in top]
    if missing:
        issues.append(FileIssue("schema", "Missing required top-level keys: " + ", ".join(missing)))

    missing_cpp = [k for k in required_cpp if k not in top]
    if missing_cpp:
        issues.append(FileIssue("schema", "Missing required C/C++ keys: " + ", ".join(missing_cpp)))

    declared_path = top.get("path")
    if declared_path:
        # Normalize both to forward slashes for compare.
        declared_norm = declared_path.strip().strip('"').strip("'")
        if declared_norm != rel:
            issues.append(FileIssue("path_mismatch", f"declared path '{declared_norm}' != actual '{rel}'"))

    return FileReport(
        path=rel,
        ext=ext,
        has_meta=True,
        meta_version=mv,
        declared_path=declared_path,
        top_level_keys=top_keys,
        issues=issues,
    )


def build_report(root: Path) -> Tuple[SummaryReport, List[FileReport]]:
    files = _collect_files(root)

    reports: List[FileReport] = []
    missing_meta = 0
    parse_fail = 0
    schema_fail = 0
    path_mismatch = 0
    layout_fail = 0
    meta_version_fail = 0

    for p in files:
        r = analyze_file(root, p)
        reports.append(r)

        # Aggregate
        has_issue_codes = {i.code for i in r.issues}
        if "missing_meta" in has_issue_codes:
            missing_meta += 1
        if "parse" in has_issue_codes or "read_fail" in has_issue_codes:
            parse_fail += 1
        if "schema" in has_issue_codes:
            schema_fail += 1
        if "path_mismatch" in has_issue_codes:
            path_mismatch += 1
        if "layout" in has_issue_codes:
            layout_fail += 1
        if "meta_version" in has_issue_codes:
            meta_version_fail += 1

    summary = SummaryReport(
        root=str(root),
        files_scanned=len(files),
        files_in_scope=len(files),
        missing_meta=missing_meta,
        parse_fail=parse_fail,
        schema_fail=schema_fail,
        path_mismatch=path_mismatch,
        layout_fail=layout_fail,
        meta_version_fail=meta_version_fail,
    )

    return summary, reports


def _write_json(path: Path, summary: SummaryReport, reports: List[FileReport]) -> None:
    obj = {
        "summary": asdict(summary),
        "files": [
            {
                "path": r.path,
                "ext": r.ext,
                "has_meta": r.has_meta,
                "meta_version": r.meta_version,
                "declared_path": r.declared_path,
                "top_level_keys": r.top_level_keys,
                "issues": [asdict(i) for i in r.issues],
            }
            for r in reports
        ],
    }
    path.write_text(json.dumps(obj, indent=2, sort_keys=True), encoding="utf-8")


def _print_list(title: str, items: List[str], limit: Optional[int]) -> None:
    if not items:
        return
    print()
    print(title)
    print("-" * len(title))
    if limit is None:
        for it in items:
            print(it)
    else:
        for it in items[:limit]:
            print(it)
        if len(items) > limit:
            print(f"... ({len(items) - limit} more)")


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser(description="FATP_META inventory and validation (stdlib-only).")
    ap.add_argument("--root", default=None, help="Repo root (default: auto-detect from tools/).")
    ap.add_argument("--json-out", default=None, help="Write full report as JSON to this path.")
    ap.add_argument("--print-missing", action="store_true", help="Print files missing FATP_META.")
    ap.add_argument("--print-path-mismatch", action="store_true", help="Print files with FATP_META.path mismatch.")
    ap.add_argument("--print-schema-fail", action="store_true", help="Print files failing required key checks.")
    ap.add_argument("--print-layout-fail", action="store_true", help="Print files failing layout checks.")
    ap.add_argument("--limit", type=int, default=200, help="Print limit per category (default: 200).")
    ap.add_argument("--fail-on-issues", action="store_true", help="Exit non-zero if any issues are found.")
    args = ap.parse_args(argv)

    if args.root is None:
        # tools/ -> repo root
        root = Path(__file__).resolve().parents[1]
    else:
        root = Path(args.root).resolve()

    if not root.exists():
        print(f"ERROR: root does not exist: {root}", file=sys.stderr)
        return 2

    summary, reports = build_report(root)

    # Summary
    print("FATP_META Inventory Summary")
    print("==========================")
    print(f"Root:             {summary.root}")
    print(f"Files in scope:   {summary.files_in_scope}")
    print(f"Missing FATP_META:{summary.missing_meta}")
    print(f"Parse/read fails: {summary.parse_fail}")
    print(f"Schema fails:     {summary.schema_fail}")
    print(f"Path mismatches:  {summary.path_mismatch}")
    print(f"Layout fails:     {summary.layout_fail}")
    print(f"meta_version bad: {summary.meta_version_fail}")

    # Lists
    missing = [r.path for r in reports if any(i.code == "missing_meta" for i in r.issues)]
    path_mis = [r.path for r in reports if any(i.code == "path_mismatch" for i in r.issues)]
    schema = [r.path for r in reports if any(i.code == "schema" for i in r.issues)]
    layout = [r.path for r in reports if any(i.code == "layout" for i in r.issues)]

    lim = None if args.limit < 0 else args.limit
    if args.print_missing:
        _print_list("Missing FATP_META", missing, lim)
    if args.print_path_mismatch:
        _print_list("FATP_META.path mismatch", path_mis, lim)
    if args.print_schema_fail:
        _print_list("Schema failures (required keys)", schema, lim)
    if args.print_layout_fail:
        _print_list("Layout failures", layout, lim)

    if args.json_out:
        out = Path(args.json_out)
        _write_json(out, summary, reports)
        print()
        print(f"Wrote JSON report: {out}")

    any_issues = summary.missing_meta or summary.parse_fail or summary.schema_fail or summary.path_mismatch or summary.layout_fail or summary.meta_version_fail
    if args.fail_on_issues and any_issues:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
