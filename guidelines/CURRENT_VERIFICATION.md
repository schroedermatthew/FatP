# Current verification

Source: working tree based on 9ff6d855f5ce5d949387586fad9e74cc5e121349.
Observation date: 2026-09-06. The [profile](PROJECT_PROFILE.md) owns commands;
this file records their measured results and remaining limits.

| Gate | Result | Scope and limits |
|---|---|---|
| Guideline corpus | PASS | Required documents, links, profile, formatter and active ledger arithmetic |
| Guideline tool regressions | PASS: 44 tests, no skips | Real formatter/Clang controls and isolated Git tests of the actual CI ledger resolver |
| Selected tooling metadata | PASS: 6 files | Four validator/shared-library files and two test files; an explicit subset |
| Full strict metadata inventory | FAIL: 383 files, 377 first diagnostics, 6 passes | Existing findings remain failures; selected tooling success does not replace this gate |
| Authored C++ style inventory | FAIL / limited | Recorded source-bound inventory: 347 formatter/lexical checks and AST attempts; 250 files differ from formatting rules, naming available for 249 and unavailable for 98. See the [conformance report](reports/CPP_CONFORMANCE.md) for raw observations, source hashes and limits. No new C++ build or full AST run is claimed |
| Guidelines CI | Not dispatched remotely | Local workflow controls do not establish hosted Linux installation or job execution |
| Product builds, runtime tests and sanitizers | Not rerun for this documentation/tooling change | Existing conformance and coverage gaps remain explicit; no new runtime result is claimed |

CI compares demerit counts against the applicable previous ledger in Git, including
an earlier ancestor when the ledger was deleted. If no prior ledger exists, it
reports that no prior comparison is available and validates current arithmetic.
Current awards remain in [DEMERITS](DEMERITS.md).

The [C++ conformance report](reports/CPP_CONFORMANCE.md) and its per-file inventory
retain actionable code gaps and tool-coverage limits. A green corpus or checker
test suite does not establish full code conformance. Re-run affected gates when
their inputs change.

Local commands used Python 3.12.14 with PyYAML 6.0.3, markdown-it-py 4.2.0,
clang-format 22.1.8 and Clang 22.1.8 on Windows UCRT64. Corpus lint used the
profile command with the installed formatter selected by --clang-format. Tests
used the profile command with FATP_GUIDELINE_FORMATTER and FATP_GUIDELINE_CLANG.
The selected metadata run used --selected-only and the six tooling/test paths
listed by guidelines.yml; the full metadata command omitted those subset flags.
