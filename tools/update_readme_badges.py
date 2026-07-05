#!/usr/bin/env python3
"""Regenerate README CI badge sections from .github/workflows/*.yml."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github" / "workflows"
README = ROOT / "README.md"
BASE = "https://github.com/schroedermatthew/FatP/actions/workflows"

LABELS = {
    "ci_verify": "Layer & Dependency Verification",
    "fatp_meta_compliance": "FATP_META Compliance",
    "header-hygiene": "Header Hygiene",
    "fatp-test-core": "FatP CI",
    "fatp-test": "FatPTest CI",
    "fatp-benchmark-runner": "FatPBenchmarkRunner CI",
    "fatp-concepts": "FatPConcepts CI",
    "fatp-hash-map": "FatPHashMap CI",
    "fatp-hash-map-benchmarks": "FatPHashMap Benchmarks",
    "fatp-json": "FatPJson CI",
    "fatp-json-stream": "FatPJsonStream CI",
    "fatp-binary": "FatPBinary CI",
    "fatp-cbor": "FatPCbor CI",
    "fatp-cbor-stream": "FatPCborStream CI",
    "build-benchmark-deps": "Build Benchmark Deps",
    "run-all": "Run All",
    "run-all-benchmarks-clang": "Run All Benchmarks (Clang)",
    "run-all-benchmarks-gcc": "Run All Benchmarks (GCC)",
    "run-all-benchmarks-msvc": "Run All Benchmarks (MSVC)",
    "csr-matrix-hpc-parallel": "CSRMatrix_HPC_Parallel CI",
    "csr-matrix-hpc": "CSRMatrix_HPC CI",
    "csr-matrix-parallel": "CSRMatrixParallel CI",
    "csr-matrix": "CSRMatrix CI",
    "diagnostic-logger-core": "DiagnosticLogger_Core CI",
    "diagnostic-logger-io": "DiagnosticLogger_IO CI",
    "diagnostic-logger-json": "DiagnosticLogger_Json CI",
    "diagnostic-logger-scopeguard": "DiagnosticLogger_ScopeGuard CI",
    "json-lite": "JsonLite CI",
    "json-stream-lite": "JsonStreamLite CI",
    "xml-lite": "XmlLite CI",
    "cbor-lite": "CborLite CI",
    "cbor-stream-lite": "CborStreamLite CI",
    "binary-lite": "BinaryLite CI",
    "async-operations": "AsyncOperations CI",
    "diagnostic-context": "DiagnosticContext CI",
}

BENCH = {
    "alignedvector": "AlignedVector",
    "allocationstrategies": "AllocationStrategies",
    "bitset": "BitSet",
    "circularbuffer": "CircularBuffer",
    "flatmapset": "FlatMapSet",
    "intrusivelist": "IntrusiveList",
    "lockfreequeue": "LockFreeQueue",
    "objectpool": "ObjectPool",
    "policyiterator": "PolicyIterator",
    "servicelocator": "ServiceLocator",
    "slotmap": "SlotMap",
    "smallvector": "SmallVector",
    "sparseset": "SparseSet",
    "statemachine": "StateMachine",
    "strongid": "StrongId",
    "threadpool": "ThreadPool",
    "workqueue": "WorkQueue",
    "fatp-hash-map": "FatPHashMap",
    "featuremanager": "FeatureManager",
    "stringpool": "StringPool",
    "skeleton": "Skeleton",
}

CORE = [
    "fatp-test-core.yml",
    "fatp-test.yml",
    "fatp-benchmark-runner.yml",
    "fatp-concepts.yml",
]
VERIFY = ["ci_verify.yml", "fatp_meta_compliance.yml", "header-hygiene.yml"]


def title_words(name: str) -> str:
    parts = name.split("-")
    out: list[str] = []
    for part in parts:
        if part == "fatp":
            out.append("FatP")
        elif part == "csr":
            out.append("CSR")
        elif part in {"rcu", "numa", "simd", "hpc", "io", "json", "cbor", "xml"}:
            out.append(part.upper() if len(part) <= 4 else part.capitalize())
        else:
            out.append(part.capitalize())
    return "".join(out)


def label(fname: str) -> str:
    stem = fname.replace(".yml", "")
    if stem in LABELS:
        return LABELS[stem]
    if stem.endswith("-benchmarks"):
        comp = stem[: -len("-benchmarks")]
        return BENCH.get(comp, title_words(comp)) + " Benchmarks"
    return title_words(stem) + " CI"


def badge_line(fname: str) -> str:
    return f"![{label(fname)}]({BASE}/{fname}/badge.svg)"


def generate_details_block() -> str:
    workflows = sorted(p.name for p in WORKFLOWS.glob("*.yml"))
    aggregate = sorted(f for f in workflows if f.startswith("run-all"))
    bench = sorted(
        f for f in workflows if f.endswith("-benchmarks.yml") or f == "build-benchmark-deps.yml"
    )
    components = sorted(
        set(workflows) - set(CORE) - set(VERIFY) - set(aggregate) - set(bench)
    )

    lines = [
        "<details>",
        "<summary><strong>All CI Workflows</strong></summary>",
        "",
        "#### Core Infrastructure",
        *[badge_line(f) for f in CORE],
        "",
        "#### Verification",
        *[badge_line(f) for f in VERIFY],
        "",
        "#### Components",
        *[badge_line(f) for f in components],
        "",
        "#### Benchmarks",
        *[badge_line(f) for f in bench],
        "",
        "#### Aggregate Runners",
        *[badge_line(f) for f in aggregate],
        "",
        "</details>",
    ]
    return "\n".join(lines)


def main() -> None:
    details = generate_details_block()
    readme = README.read_text(encoding="utf-8")
    readme = re.sub(r"<details>.*?</details>", details, readme, count=1, flags=re.DOTALL)
    README.write_text(readme, encoding="utf-8", newline="\n")
    print(f"Updated {README.relative_to(ROOT)}")


if __name__ == "__main__":
    main()