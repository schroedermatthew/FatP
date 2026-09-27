# Current verification

## README and workflow badge refresh

Source: documentation and generator working tree based on
`afdc8ca5a62173dec3bc4a51f85e804fea1a0f42`.
Observation date: 2026-09-26 (America/Los_Angeles). Scope: README requirements,
installation/build commands, component descriptions, repository paths, and the
workflow badge generator. Library, test, and benchmark C++ sources are unchanged.

| Gate | Result | Scope and limits |
|---|---|---|
| README references and workflow inventory | PASS | All 29 local link destinations resolve to tracked files/directories; all 129 tracked workflows occur exactly once in the generated block with matching clickable badge targets |
| Badge generator controls | PASS: 19 checks | Syntax, current marked-block equality, preservation of unrelated details/prose, idempotence, workflow categories, and malformed-marker rejection; no tracked bytecode regenerated |
| Manual header-copy installation | PASS | Actual recursive copy, then GCC 13.3/libstdc++ C++20 `-Wall -Wextra -Wpedantic -Werror` compile/link/run using SmallVector and the nested TensorRanked include chain; both `-I include` and `-iquote include/fat_p` are required and documented |
| External CMake consumer | PASS | Configure/build/run with tests and benchmarks disabled; MSVC 19.51, Microsoft STL, x64, C++20 Release, `/W4 /WX` and the existing `/wd4324` intentional-padding exception; `fatp` supplies include paths and language mode |
| Documented test commands | PASS: 127/127 CTest tests | Reconfigured the existing MSVC Release scratch build; incremental build found no C++ changes; the complete test command ran successfully |
| Documented benchmark target | PASS: configure/build/link | Separate MSVC Release configuration built `benchmark_SlotMap` with vendored competitors and no vcpkg setup; no benchmark timing measurement was requested |
| Legacy metadata | PASS: 363 files | Zero findings; output identical to the pre-edit baseline |
| Strict metadata | FAIL: 383 checked, 377 first diagnostics | Output identical to the baseline, including the generator's existing metadata key-order finding; no introduced metadata diagnostics |
| C++ style inventory | Not rerun | No authored C++ file changed; the previous full inventory and its limits remain below |

The copy-header check initially exposed the missing short-header search path in
the installation prose; the documented flags were corrected and the same consumer
then compiled and ran. The strict MSVC consumer initially reported C4324 for
SmallVector's explicit alignment; it passed with the project's existing narrow
warning exception. The README now distinguishes configured CI coverage, historical
benchmark reports, optional platform/competitor dependencies, and current results.

## SlotMap benchmark dependency cache

Source: workflow repair commit
`438e9d35d5cd93c9465bb057254f8df449d76da8`, based on
`6a9f42b0ac8fb927a0f2ac3bfa71a99ba733dbc1`.
Observation date: 2026-09-26 (America/Los_Angeles). Scope: pinned Hive dependency,
header cache invalidation, C++20 cache publication gate, and SlotMap summary
failure propagation. Product and benchmark C++ sources are unchanged.

The supplied [failed run](https://github.com/schroedermatthew/FatP/actions/runs/36218608677)
restored header cache `fatp-bench-deps-headeronly-v1-36210665780`; all five Linux
builds failed in Hive. Upstream commit
`94e396f5542448d9158145119684306cd54c234f` replaced its range-constructor fallback
with C++23 `std::from_range_t`. The builder now pins its preceding revision,
`aa47e3627a08251d6df2103ddc3be482eb9fa5b3`, and all 15 header-cache consumers use
v2 without a v1 restore fallback. Compiled dependency cache keys remain unchanged.

| Gate | Result | Scope and limits |
|---|---|---|
| Pinned fetch provenance | PASS | Exact shallow fetch/check-out command resolves the requested SHA; header SHA-256 is `ab9c7b42b3c4ee491b6cf0babe49ab654077d2a78f21ac9eade521b5f5e9c3fa`, matching the local test header |
| Linux C++20 benchmark build/link | PASS: five compilers | WSL Ubuntu 24.04 x86-64, libstdc++, GCC 12.4/13.3/14.2 and Clang 16.0.6/17.0.6; actual SlotMap source with pinned Hive, EnTT, vendored SG14, `-O3 -DNDEBUG -march=native -pthread` |
| Windows C++20 benchmark build/link | PASS: two compilers | MSYS2 UCRT64 GCC 16.1 and Clang 22.1.8, libstdc++, `-O2`; pinned Hive and SG14 enabled |
| Regression negative controls | PASS: four compiler configurations | Current upstream Hive `89b0b8c7af5d74f6c9f1b6e1b9411ed567b790a6` fails the actual C++20 benchmark on Linux GCC 13/Clang 16 and Windows GCC 16/Clang 22; the pinned header removes the range-constructor and Clang allocator errors |
| Linux runtime smoke checks | PASS: five full executions | All five compiler builds exit 0, reach `Benchmark Complete`, and report Hive and EnTT enabled; one warmup and one measured batch, parallel runs, about 91 seconds each; no performance inference |
| Windows runtime checks | Partial | Focused Hive-adapter checks pass all eight operation cases on both compilers; both full unchanged benchmark runs timed out at 300 seconds during later cooldown/stabilization waits, without reported runtime errors; full completion is not claimed |
| Workflow structure and script behavior | PASS | All 16 edited workflows pass actionlint 1.7.12 structural validation; 147 Bash syntax probes, 23 extracted-summary success/failure/missing-result scenarios, and six actual Linux/Windows wrapper exit-status probes pass |
| Full actionlint and ShellCheck | FAIL: existing diagnostics | ShellCheck 0.9.0 reports 292 diagnostics across the 16 files, down from the 295-diagnostic baseline; zero added and three removed; structural validity is separate from this remaining lint debt |
| Guideline corpus | PASS | Instantiated corpus, profile, links, and ledger arithmetic; no ledger change |
| Hosted cache publication | PASS | [Dependency build](https://github.com/schroedermatthew/FatP/actions/runs/36250029036) missed v2, fetched the pinned Hive revision, passed all five C++20 compiler checks, and saved `fatp-bench-deps-headeronly-v2-36250029036`; existing compiled caches were reused |
| Hosted SlotMap benchmark matrix | PASS: all seven jobs | [SlotMap run](https://github.com/schroedermatthew/FatP/actions/runs/36250161009): GCC 12/13/14, Clang 16/17, MSVC, and summary all succeed; all six benchmarks reach completion, and all five Linux logs confirm the v2 cache hit and exact pinned Hive SHA |
| Hosted guideline checks | PASS | [Guidelines and tooling](https://github.com/schroedermatthew/FatP/actions/runs/36250028754) at the repair commit |
| C++ style and metadata inventories | Not rerun | YAML and this Markdown record are outside their authored-code change triggers; prior conformance failures remain recorded below |

The Linux build gate uses the benchmark's existing CI warning policy. Existing
GCC 12 volatile-assignment and Clang switch-enumerator warnings remain visible.
Local Linux checks did not include Boost; hosted workflows restore that competitor
from the compiled dependency cache.
The benchmark's local cooldown helper still performs its own stabilization waits;
setting shared no-cooldown flags does not eliminate those waits. This dependency
repair does not change benchmark timing or make performance claims.

## Previous numeric and concurrent-size verification

Source: `6a9f42b0ac8fb927a0f2ac3bfa71a99ba733dbc1`, originally verified as a working
tree based on `f4d756ad392656131a76accc3f2960f3cf1ae5b9`.
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
