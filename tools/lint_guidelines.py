# FATP_META:
#   meta_version: 1
#   component: GuidelineTooling
#   file_role: tooling
#   path: tools/lint_guidelines.py
#   layer: Infrastructure
#   summary: Validates the active guideline corpus, project profile, and demerit ledger.
#   api_stability: in_work

"""Validate the corpus, configuration, references, ledger and formatter without edits."""
from __future__ import annotations

import argparse
import re
from datetime import date
from pathlib import Path

from guideline_lib import (
    MARKDOWN, ONBOARDING, REQUIRED_DOCS, console, formatter_integrity, heading_ids, json_value,
    link_target, links, profile, prose, read, report, roots, validate_profile,
)

def ledger_values(text: str) -> tuple[list[str], dict[str, dict[str, int]]]:
    rows = [[cell.strip() for cell in line.strip().strip("|").split("|")]
            for line in text.splitlines() if line.lstrip().startswith("|")]
    headers = [row for row in rows if row[:2] == ["ID", "Violation"]]
    if len(headers) != 1:
        raise ValueError("exactly one assistant-column header required")
    header = headers[0]
    assistants = header[2:]
    if not assistants or not all(assistants) or len(set(assistants)) != len(assistants):
        raise ValueError("missing or duplicated assistant columns")

    def counts(row, *, total=False):
        if len(row) != len(header):
            raise ValueError("malformed category/total row")
        values = [cell.strip("*") if total else cell for cell in row[2:]]
        if any(not re.fullmatch(r"[0-9]+", value) for value in values):
            raise ValueError("counts must be nonnegative integers")
        return dict(zip(assistants, map(int, values)))

    categories = {}
    totals = []
    for row in rows:
        if re.fullmatch(r"D[0-9]+", row[0]):
            if row[0] in categories:
                raise ValueError("duplicate category row")
            categories[row[0]] = counts(row)
        elif row[0] == "**Total**":
            totals.append(counts(row, total=True))
    if not {f"D{i:02}" for i in range(1, 25)} <= categories.keys():
        raise ValueError("an inherited failure category was removed")
    if len(totals) != 1:
        raise ValueError("exactly one total row required")
    expected = {assistant: sum(row[assistant] for row in categories.values())
                for assistant in assistants}
    if totals[0] != expected:
        raise ValueError("totals do not reconcile with category counts")
    return assistants, categories


def directed_corrections(value, before, after) -> set[tuple[str, str]]:
    if (not isinstance(value, dict) or set(value) != {"schema_version", "corrections"}
            or type(value["schema_version"]) is not int or value["schema_version"] != 1
            or not isinstance(value["corrections"], list) or not value["corrections"]):
        raise ValueError("corrections require schema_version=1 and a nonempty corrections list")
    approved = set()
    for item in value["corrections"]:
        if not isinstance(item, dict) or set(item) != {
                "date", "assistant", "category", "before", "after", "direction", "reason"}:
            raise ValueError("correction has missing/unknown fields")
        if any(not isinstance(item[key], str) or not item[key].strip()
               for key in ("date", "assistant", "category", "direction", "reason")):
            raise ValueError("correction needs date, attribution, actual direction and reason")
        if not re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}", item["date"]):
            raise ValueError("correction date must be YYYY-MM-DD")
        date.fromisoformat(item["date"])
        if any(type(item[key]) is not int or item[key] < 0 for key in ("before", "after")):
            raise ValueError("correction before/after must be nonnegative integers")
        key = (item["category"], item["assistant"])
        category, assistant = key
        if key in approved:
            raise ValueError("duplicate correction for a category/assistant")
        if (item["before"] == item["after"]
                or before.get(category, {}).get(assistant) != item["before"]
                or after.get(category, {}).get(assistant) != item["after"]):
            raise ValueError("correction must match the supplied prior and current counts")
        approved.add(key)
    return approved


def ledger(text: str, previous: str | None = None, corrections=None) -> list[str]:
    try:
        assistants, current = ledger_values(text)
        if any(token.type == "fence" and token.info.strip() == "incident"
               for token in MARKDOWN.parse(text)):
            raise ValueError("event logs do not belong in compact DEMERITS; retain only load-bearing cases separately")
        if previous is None:
            if corrections is not None:
                raise ValueError("directed corrections require --previous-ledger")
            return []
        old_assistants, old = ledger_values(previous)
        approved = directed_corrections(corrections, old, current) if corrections is not None else set()
        errors = []
        for assistant in old_assistants:
            if assistant not in assistants:
                errors.append(f"DEMERITS: original assistant column removed: {assistant}")
        for category, row in old.items():
            if category not in current:
                errors.append(f"DEMERITS: original category removed: {category}")
                continue
            for assistant, count in row.items():
                if (assistant in current[category] and current[category][assistant] < count
                        and (category, assistant) not in approved):
                    errors.append(f"DEMERITS: count decreased without matching directed correction: {category}/{assistant}")
        return errors
    except (ValueError, TypeError, KeyError) as exc:
        return [f"DEMERITS: {exc}"]


def inherited_identity_errors(text: str, data: dict, instantiated: bool) -> list[str]:
    """Reject inherited template identities while admitting the receiving project's prefix."""
    errors = []
    if "PROJECT_OPTIONS.md" in text:
        errors.append("obsolete project-options reference")
    identity = data.get("identity")
    receiving_fatp = (instantiated and data.get("status") == "instantiated"
                      and isinstance(identity, dict) and identity.get("macro_prefix") == "FATP")
    if re.search(r"\bFATP_(TEST|ASSERT|CHECK|META)", text) and not receiving_fatp:
        errors.append("inherited harness or metadata identity")
    return errors


def validate(g: Path, repo: Path, *, instantiated=False, formatter="clang-format",
             previous_ledger: Path | None = None, ledger_corrections: Path | None = None) -> list[str]:
    errors = []
    for name in REQUIRED_DOCS:
        if not (g / name).is_file():
            errors.append(f"missing required guideline: {name}")
    if errors:
        return errors
    data = {}
    try:
        data = profile(g)
        errors.extend(validate_profile(data, repo, instantiated or data.get("status") == "instantiated"))
    except (ValueError, OSError) as exc:
        errors.append(f"profile: {exc}")
    docs = [p for p in g.rglob("*.md") if not any(
        part in ("_archive", "_historical", "_inherited") for part in p.relative_to(g).parts)]
    for doc in docs:
        text = read(doc)
        relative = doc.relative_to(g)
        if instantiated and ("{{" in text or "}}" in text):
            errors.append(f"{relative}: unresolved template placeholder")
        errors.extend(f"{relative}: {finding}" for finding in inherited_identity_errors(
            text, data, instantiated or data.get("status") == "instantiated"))
        if "\ufffd" in text:
            errors.append(f"{relative}: corrupted replacement character")
        for line, destination in links(text):
            try:
                target, anchor = link_target(doc, destination, repo)
                if target is None:
                    continue
                if not target.exists():
                    errors.append(f"{relative}:{line}: broken link {destination}")
                elif anchor and target.suffix == ".md" and anchor not in heading_ids(read(target)):
                    errors.append(f"{relative}:{line}: missing heading anchor {destination}")
            except ValueError as exc:
                errors.append(f"{relative}:{line}: {exc}")
    core = read(g / "CORE.md")
    # Live prose only: a demoted statement kept as a specimen is not a policy.
    normalized = prose(core)
    for statement in ("Demerits are mandatory.", "Google C++ Style Guide is prohibited.",
                      "C++20 is the minimum for C++ work."):
        if statement not in normalized:
            errors.append(f"CORE: protected policy statement missing: {statement}")
    if core.count("<!-- onboarding -->") != 1 or core.count("<!-- /onboarding -->") != 1:
        errors.append("CORE: exactly one canonical onboarding block required")
    onboarding = re.search(r"<!-- onboarding -->(.*?)<!-- /onboarding -->", core, re.S)
    order = [dest for _, dest in links(onboarding.group(1))] if onboarding else []
    if order != list(ONBOARDING):
        errors.append("CORE: onboarding must be " + ", ".join(ONBOARDING))
    index_targets = {link_target(g / "README.md", dest, repo)[0]
                     for _, dest in links(read(g / "README.md"))}
    for doc in docs:
        if doc.name != "README.md" or doc.parent != g:
            if doc.resolve() not in index_targets:
                errors.append(f"README: live document not indexed: {doc.relative_to(g)}")
    core_targets = {link_target(g / "CORE.md", dest, repo)[0]
                    for _, dest in links(core) if "#" not in dest}
    for doc in docs:
        if doc.parent.name in ("modules", "cpp") and doc.resolve() not in core_targets:
            errors.append(f"CORE: module not routed by a whole-document link: {doc.relative_to(g)}")
    style = prose(read(g / "cpp/STYLE.md"))
    if "Google" not in style or "prohibited" not in style or "mandatory" not in style:
        errors.append("STYLE: mandatory house-style/Google prohibition missing")
    before = read(previous_ledger) if previous_ledger is not None else None
    corrections = json_value(read(ledger_corrections)) if ledger_corrections is not None else None
    errors.extend(ledger(read(g / "DEMERITS.md"), before, corrections))
    errors.extend(formatter_integrity(repo, formatter))
    return errors


def main() -> int:
    console()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("guidelines", nargs="?", default="guidelines")
    parser.add_argument("--repo-root")
    parser.add_argument("--instantiated", action="store_true")
    parser.add_argument("--clang-format", default="clang-format")
    parser.add_argument("--previous-ledger", type=Path)
    parser.add_argument("--ledger-corrections", type=Path,
                        help="separate JSON record of directed count corrections; requires --previous-ledger")
    args = parser.parse_args()
    try:
        g, repo = roots(args.guidelines, args.repo_root)
        errors = validate(g, repo, instantiated=args.instantiated, formatter=args.clang_format,
                          previous_ledger=args.previous_ledger, ledger_corrections=args.ledger_corrections)
        print("Prior ledger tally: compared" if args.previous_ledger else
              "Prior ledger tally: not checked (supply --previous-ledger when editing an existing ledger)")
        print("Scope: corpus/configuration and tally integrity; no award authentication or complete event-history claim.")
        print("Authority, semantic conformance and fresh-context usefulness require separate review.")
        return report(errors, "guidelines")
    except (OSError, ValueError, TypeError, KeyError) as exc:
        return report([str(exc)], "guidelines")


if __name__ == "__main__":
    raise SystemExit(main())
