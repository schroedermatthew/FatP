# Current verification

Source: uncommitted working tree based on
`f4d756ad392656131a76accc3f2960f3cf1ae5b9`.
Observation date: 2026-09-25 (America/Los_Angeles). Scope: checked-cast bounds,
finite ULP-count conversion, bounded concurrent size estimates, and their tests.
The documented ULP subnormal fallback is preserved by explicit maintainer direction.
The [profile](PROJECT_PROFILE.md) owns the standard commands.

| Gate | Result | Scope and limits |
|---|---|---|
| MSVC Release build | PASS | All registered test targets compiled and linked with MSVC 19.51.36252, Microsoft STL, Windows x64, C++20, `/W4 /WX`, and the existing `/wd4127 /wd4324` exceptions |
| MSVC Release runtime suite | PASS: 127/127 CTest tests | Separate scratch build; benchmarks disabled; final incremental rebuild included the corrected exception assertion |
| GCC 16.1 Windows GNU strict builds | FAIL: four affected suites | Each reports the same five existing `-Werror=cast-function-type` diagnostics in unchanged `FatPTest.h` PDH function-pointer casts; no runtime execution or GCC pass claimed |
| Clang undefined-behavior checks | PASS: four affected suites, 139 cases | Clang 22.1.8, Windows GNU/libstdc++, C++20, `-O2 -DNDEBUG`, `-fsanitize=undefined,float-cast-overflow -fsanitize-trap=all`; all suites compiled, linked, and executed |
| MSVC AddressSanitizer | PASS: four affected suites, 139 cases | `/fsanitize=address /Zi`, Release; direct execution in the developer environment passed. CTest launching produced loader error `0xc0000135` for three suites, so that launch route did not pass |
| Public-header self-containment/composition | PASS: 149 compile-only probes | All 124 metadata-designated public headers standalone, one all-public TU, and all 24 permutations of the four edited headers; no harness prerequisite or PCH; MSVC C++20 warning policy above; all produced objects |
| Regression negative controls | PASS: new cases reject original behavior | Before header edits, both numeric suites failed their new cases; queue and SPSC observer stress reported size `18446744073709551615` for capacity 8. Stress scheduling is not deterministic |
| Numeric sanitizer negative controls | PASS: original defects trap, repaired cases pass | Original headers from HEAD with runtime volatile inputs: float `2^31` to int32 and double ULP tolerance `1e20`; `-fsanitize=float-cast-overflow -fsanitize-trap=all` produces `0xc000001d` only for invalid original cases; original valid controls and both repaired cases exit 0 |
| Legacy metadata | PASS: 363 files | No missing/read/schema/path/layout/version findings |
| Strict metadata | FAIL: 383 checked, 377 first diagnostics, 6 passes | Output is identical to the pre-edit baseline; existing schema/layout/hygiene drift remains unresolved |
| Authored C++ style inventory | FAIL / limited: all 347 files checked | 250 formatting differences; 149 files with 400 lexical diagnostics; ASTs available for 249 and unavailable for 98, including 16 timeouts; 15,295 raw naming diagnostics in 191 files; all checked source hashes match final files |
| Guideline corpus and ledger comparison | PASS | Exact previous ledger retained for comparison; corpus/configuration integrity and tally retention checked; not semantic conformance proof |
| ThreadSanitizer | Unavailable locally | Windows Clang rejects `-fsanitize=thread`; the local Ubuntu 24.04 WSL installation has neither GCC nor Clang installed |
| Hosted CI / Linux compiler matrix | Not run for these uncommitted edits | Windows local results do not establish hosted Linux, ARM/GPU, or other compiler/library configurations |
| Guideline checker regressions | Not rerun | Checker implementation is unchanged; the prior 2026-09-06 observation recorded 44 passing tests |

The CMake build used Ninja in a scratch directory, `FATP_BUILD_TESTS=ON`,
`FATP_BUILD_BENCHMARKS=OFF`, `CMAKE_BUILD_TYPE=Release`, and
`CMAKE_CXX_FLAGS=/WX`. The developer-shell `VCPKG_ROOT` was unset for this build
because the existing default manifest path has no manifest. No build-system
repair is claimed. CTest used `--output-on-failure` on the complete test tree.

Clang runs used `-Wall -Wextra -Wpedantic -Werror`, the existing GNU variadic-macro
warning exception, `-I include -iquote include/fat_p`,
`-DENABLE_TEST_APPLICATION`, `-ladvapi32`, and `-fuse-ld=lld`.
The four suites are CheckedArithmetic, FloatingPointComparison, LockFreeQueue,
and LockFreeRingBuffer (78, 37, 11, and 13 cases respectively).

Guideline/metadata tooling used Python 3.12.14 with PyYAML 6.0.3,
markdown-it-py 4.2.0, clang-format 22.1.8, and Clang 22.1.8. The legacy and
strict metadata gates remain separate. Historical findings in the
[conformance report](reports/CPP_CONFORMANCE.md) are not waived by runtime success.

The full style inventory used unchanged per-file checker logic with parallel
scheduling, retained completed results when worker concurrency changed, and
reconciled every final source hash. All eight edited C++ files produced usable
ASTs. Added header lines had no direct naming diagnostics; diagnostics on added
test lines came from the existing required harness macros and their generated
identifiers. Existing formatter differences remain in all eight files; only
edited ranges were formatted. The four tests now include their component header
first, reducing the full lexical inventory from 404 to 400 diagnostics.

Concurrent size tests use bounded stress workloads and do not force a particular
interleaving or exercise complete enqueue/dequeue counter rollover. The size
implementation retains modular subtraction and bounds its approximate result;
it does not promise a linearizable snapshot.

The [ledger](DEMERITS.md) records ChatGPT D02 +1 for initially expecting
`std::runtime_error` without tracing the cast's enforcement helper. The regression
now checks `std::logic_error`, matching `AlwaysEnforcePolicy` and `LogicRaiser`;
the corrected test passed the builds and executions above.
