#!/usr/bin/env python3
# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/strip_benchmark_ensure_deps.py
#   summary: "Strip ensure-deps job blocks from per-component benchmark workflows."
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

"""Remove ensure-deps job from per-component *-benchmarks.yml workflows.

Run All Benchmarks builds deps once before dispatching; component workflows
only restore shared caches.
"""

import glob
import os
import re

WORKFLOWS_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    ".github",
    "workflows",
)

SKIP = {
    "run-all-benchmarks.yml",
    "run-all-benchmarks-gcc.yml",
    "run-all-benchmarks-clang.yml",
    "run-all-benchmarks-msvc.yml",
    "skeleton-benchmarks.yml",
    "featuremanager-benchmarks.yml",
}

ENSURE_DEPS_BLOCK = re.compile(
    r"""
  \#\s*=+\n
  \#\s*Ensure shared benchmark dependency caches exist\n
  \#\s*=+\n
  ensure-deps:\n
    name: Ensure Benchmark Dependencies\n
    uses: \./\.github/workflows/build-benchmark-deps\.yml\n
    secrets: inherit\n
\n?
""",
    re.VERBOSE,
)

NEEDS_ENSURE = re.compile(r"^    needs: ensure-deps\n", re.MULTILINE)


def patch_file(path: str) -> bool:
    with open(path, encoding="utf-8") as f:
        content = f.read()

    if "ensure-deps:" not in content:
        return False

    new_content, n = ENSURE_DEPS_BLOCK.subn("", content, count=1)
    if n == 0:
        # Header-only variant (servicelocator) uses same structure; try looser match
        start = new_content.find("  ensure-deps:")
        if start == -1:
            return False
        end = new_content.find("\n\n", start)
        if end == -1:
            return False
        new_content = new_content[:start] + new_content[end + 2 :]

    new_content = NEEDS_ENSURE.sub("", new_content)
    if new_content == content:
        return False

    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(new_content)
    return True


def main():
    changed = 0
    for path in sorted(glob.glob(os.path.join(WORKFLOWS_DIR, "*-benchmarks.yml"))):
        name = os.path.basename(path)
        if name in SKIP:
            continue
        if patch_file(path):
            changed += 1
            print(f"  Patched: {name}")
    print(f"\nPatched {changed} benchmark workflow(s)")


if __name__ == "__main__":
    main()