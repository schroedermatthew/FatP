#!/usr/bin/env python3
# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/collect_fatp_test_sources.py
#   summary: "List component test sources for the FatP aggregate test runner."
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

"""Emit source files for the test_FatP.cpp aggregate test executable."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DECLARATIONS = ROOT / "components" / "FatPTest" / "tests" / "test_FatP.h"
ORCHESTRATOR = ROOT / "components" / "FatPTest" / "tests" / "test_FatP.cpp"


def collect_sources() -> list[Path]:
    text = DECLARATIONS.read_text(encoding="utf-8")
    functions = re.findall(r"bool (test_\w+)\(\);", text)
    if not functions:
        raise SystemExit(f"No test declarations found in {DECLARATIONS}")

    sources: list[Path] = [ORCHESTRATOR]
    for function in functions:
        matches = sorted(ROOT.glob(f"components/**/{function}.cpp"))
        if len(matches) != 1:
            raise SystemExit(
                f"Expected exactly one source for {function}, found {len(matches)}"
            )
        sources.append(matches[0])

    return sources


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--msvc",
        action="store_true",
        help="Use Windows-style path separators for MSVC builds.",
    )
    args = parser.parse_args()

    paths = collect_sources()
    rendered = [
        str(path.relative_to(ROOT)).replace("\\", "/" if not args.msvc else "\\")
        for path in paths
    ]
    sys.stdout.write("\n".join(rendered))


if __name__ == "__main__":
    main()