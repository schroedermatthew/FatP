# FatP-specific working rules

These are the project details used with the task routes in [CORE](../CORE.md).
They do not replace the generic modules. Identity, supported configurations,
commands, compatibility status and adopted inventories belong to
[PROJECT_PROFILE](../PROJECT_PROFILE.md); dependency and ownership invariants
belong to [ARCHITECTURE](../ARCHITECTURE.md).

## Feature configuration responsibilities

Architecture's FP-A04 owns the configuration invariant and its operational check.
These are the existing owners to inspect when applying it:

| Header | Actual responsibility |
|---|---|
| [CppFeatureDetection.h](../../include/fat_p/CppFeatureDetection.h) | C++ language mode and library facilities: `FATP_CPLUSPLUS`, the C++20 floor, C++23/26 mode flags and availability macros such as `FATP_HAS_FORMAT`, `FATP_HAS_JTHREAD`, `FATP_HAS_MDSPAN` and `FATP_HAS_ATOMIC_SHARED_PTR`. |
| [PlatformDetection.h](../../include/fat_p/PlatformDetection.h) | Compiler, OS, CPU architecture/word size, NUMA-header availability, debug/release state and `FATP_CACHE_LINE_SIZE`. The latter is overrideable; its current default is 128 on macOS ARM64 and 64 elsewhere. These defaults are configuration choices, not runtime measurements. |
| [SimdDetection.h](../../include/fat_p/SimdDetection.h) | Compile-time SIMD macros, cached runtime capability queries and diagnostics distinguishing compiled backend from CPU capability. It depends on PlatformDetection. Inspect both compilation options and runtime/OS support before selecting a supported SIMD path. |
| [FatPConfig.h](../../include/fat_p/FatPConfig.h) | General portability/configuration macros: `FATP_NO_UNIQUE_ADDRESS`, `FATP_LIKELY`/`FATP_UNLIKELY`, `FATP_FORCEINLINE`, `FATP_NOINLINE` and `FATP_ENABLE_IOSTREAM` (default 1). It includes PlatformDetection for cache-line configuration rather than defining a competing cache-line value. |

Language mode does not establish library facility availability. A NUMA header
probe does not establish a configured linker or running NUMA hardware. Similarly,
SIMD diagnostics do not make an arbitrary ISA-specialized binary runnable on every
machine. Those distinctions are part of checking the actual supported build;
the table is not a claim that all components already use these owners correctly.

## Project semantic names

[C++ Style](../cpp/STYLE.md) owns naming and its Checked/Unchecked/Default/Safe/
ThreadSafe meanings. This registered project record preserves the additional
FatP vocabulary. These meanings require the component's actual contract and
mechanism; they are not proofs of conformance or global performance promises.

| Existing adjective | Meaning to preserve and boundary to state |
|---|---|
| Aligned | The documented memory alignment guarantee; specify its value or policy. |
| Atomic | Atomic operations with the stated ordering/operation scope. Atomicity alone does not establish lock-free progress. |
| Binary | Byte-oriented representation; specify format, byte order and compatibility separately. |
| Circular | Ring-buffer semantics with explicit capacity/full/overwrite behavior. |
| Concurrent | Documented concurrent operations and their synchronization/ownership model; not every conceivable operation is implied. |
| Diagnostic | Observability or debugging support, with its enabled/disabled behavior stated. |
| Expected | A value-or-error result abstraction with its actual access/failure contract. |
| Fast | The established throughput-oriented FastHashMap family trades stability for its documented layout/policies. The name is not a measured speed claim or permission to invent new vague Fast names. |
| Flat | Contiguous representation of the documented element/index surface; not an automatic promise of one allocation for all associated storage. |
| LockFree | The documented operation has a lock-free or stronger progress guarantee; inspect its actual mechanism and supported atomics. |
| Policy | Behavior selected through the stated template-policy interface, with real customization use cases. |
| Ranked | Tensor rank encoded in the type while extents remain runtime values. |
| Small | Inline storage optimization with a stated capacity and overflow/fallback behavior. |
| Sorted | The documented order invariant under the selected comparator. |
| Sparse | A representation for sparse data; document indexing/storage costs instead of assuming every workload uses less memory. |
| Stable | Identify precisely what remains valid and across which operations. StableHashMap promises pointer/reference stability across insert/reserve/rehash, not iterator stability or survival after erasing the referenced value. |
| Strong | A distinct type identity/wrapper; identify any additional validation separately. |

For the concrete hash-map distinction, inspect
[FastHashMap.h](../../include/fat_p/FastHashMap.h) and
[StableHashMap.h](../../include/fat_p/StableHashMap.h). Do not copy obsolete
algorithm labels or treat a historical benchmark as the current name's meaning.

## Component tests

Use the approved FatPTest harness recorded in the profile. Standard component
tests define `FATP_TEST_CASE(name)` in `fat_p::testing::<component>` and run them
with `FATP_RUN_TEST_NS(runner, component, name)`. Each case returns `bool`, with
`true` on success. The public `test_Component()` entry is in `fat_p::testing`;
the standalone entry uses the existing `ENABLE_TEST_APPLICATION` build definition
and returns a failing process status when the suite fails. Use descriptive
assertion messages and the assertion macro expressing the intended comparison.

The reference is
[test_StableHashMap.cpp](../../components/FatPHashMap/tests/test_StableHashMap.cpp),
read as implementation evidence, not as permission to copy any style drift.
The authoritative macro definitions are in
[FatPTest.h](../../include/fat_p/FatPTest.h).
[C++ testing](../cpp/TESTING.md) owns component-header-first ordering, helper
ownership, independent controls, and compile-fail diagnostic requirements.

The following files have different jobs, so do not force the ordinary test-case
wrapper onto them:

| File role | FatP convention |
|---|---|
| Suite orchestrator | Aggregate component entry functions and report failure; do not invent test cases merely to use a macro. |
| FatPTest self-test | Use independent assertions/counters so a broken harness cannot validate itself. |
| Header self-containment | `test_X_HeaderSelfContained.cpp`, target header first, no FatPTest or other FatP prerequisites; a repeated target include may check idempotence. |
| Compile-fail contract | `components/<Component>/tests/compile_fail/compile_fail_<Component>_<Reason>.cpp`; one intended contract failure per translation unit. |

The existing XmlLite self-containment test may additionally exercise its std-only
macro/parser surface without importing FatPTest. This narrow role does not turn
self-containment checks into a substitute for normal behavioral tests.

## Workflow ownership

The [Workflow](../modules/WORKFLOW.md) and [C++ build](../cpp/BUILD_AND_CI.md)
modules own gate integrity and compiler evidence. Actual compiler/runner matrices
and commands are recorded in the profile and executable workflows.

Component correctness workflows support push, pull-request and manual runs.
Their push/pull-request path filters match each other and cover public facades,
owned implementation directories, applicable benchmark source, and the workflow
itself. Test-source changes are covered by
[fatp-test-core.yml](../../.github/workflows/fatp-test-core.yml); a component may
also trigger on them when duplicated coverage is deliberate and documented.
An implementation-only change must receive the same component validation as a
facade change. A path trigger is not evidence that a benchmark source compiled:
the selected build must actually compile it when buildability is the claimed gate.

Benchmark timing belongs in separate `<component>-benchmarks.yml` workflows,
manually dispatched, without embedded or called benchmark jobs in component
correctness workflows. Register new benchmark coverage with
[run-all-benchmarks.yml](../../.github/workflows/run-all-benchmarks.yml) as well as
the component's dedicated benchmark workflow. Competitor dependencies must not
become consumer dependencies or ordinary correctness-gate setup requirements.

Benchmark workflows retain GCC, Clang and MSVC coverage and a summary job that
runs after all results, collects artifacts and publishes their actual outcomes.
Use `bench-<ComponentName>-<compiler>` and `bench-<ComponentName>-summary` artifact
names. Quote benchmark environment values in YAML. CI log output is ASCII.

Cache compiled competitors with keys that distinguish compiler/ABI, dependency
version and changed build flags. Dependency changes invalidate the corresponding
cache. Runtime libraries needed to link/run cached artifacts are installed even
on a cache hit. The historical
[Folly postmortem](../../postmortems/folly-ci-postmortem.md) explains the failure
mechanism; its dated package versions and patches are not current recipes.

Windows test/benchmark targets using the registry-backed harness helpers link
`advapi32`; deliberate alignment may need the existing narrowly justified C4324
suppression. Use the actual target configuration instead of copying an old
compiler command. Do not treat a slash direction as a universal Windows rule.

## Component sanitizer coverage

Every component correctness workflow, including a newly added component's
workflow, must include AddressSanitizer (ASan) and UndefinedBehaviorSanitizer
(UBSan) coverage. Components with concurrency must also include ThreadSanitizer
(TSan) coverage. Each required sanitizer must build and actually execute the
component's tests with instrumentation reaching the affected code; compilation
alone or an uninstrumented test run does not satisfy this requirement. Include
these results in the workflow's required aggregate gate.

Use supported compiler, runtime, architecture and runner configurations, following
[C++ sanitizer and runtime procedures](../cpp/BUILD_AND_CI.md#sanitizers-and-runtime-dependencies)
for instrumentation, linking, incompatible configurations and evidence limits.
An unsupported local or CI environment does not waive the coverage requirement:
select a supported CI configuration and report any still-unavailable execution,
with its cause and narrower evidence, as an unmet gate. Do not report a skipped
or failed sanitizer setup as passing coverage.

Ordinary tests can pass while memory, undefined-behavior or data-race defects
remain undetected. This minimum ensures new components do not omit the runtime
checks that existing component workflows are required to provide.

## Benchmark integration

[Benchmarking](../modules/BENCHMARKING.md) owns measurement mechanics, statistics,
CPU context, semantic comparison, output schema, and result placement. FatP uses
the `FATP_BENCH_` environment prefix and the configuration record below. Actual
build commands remain in the profile/workflow or the named benchmark's header.
The implementation authorities are
[FatPBenchmarkRunner.h](../../include/fat_p/FatPBenchmarkRunner.h) and
[FatPBenchmarkHeader.h](../../include/fat_p/FatPBenchmarkHeader.h).
Keep `FATP_BENCH_VERBOSE_STATS` for detailed samples/CPU diagnostics in addition
to the canonical configuration names in the base module.

Include a Boost comparator when an equivalent exists and its semantics can be
stated honestly. This supplements the standard-library baseline and distinct
design-family selection rule; it does not create a competitor-count quota.
Heavy or linked competitors remain explicit opt-in and the benchmark still
builds/runs without third-party libraries. Label unavailable candidates instead
of silently narrowing the reported comparison. Concurrency cases include a
concurrent baseline when available, or a clearly labeled locked adapter.

On Windows the benchmark measurement scope is enabled by default and can be
disabled with `FATP_BENCH_NO_SCOPE`. Preserve and restore prior affinity/priority
settings, and disclose unavailable observations rather than inventing telemetry.
The startup banner names `fat_p::<Component> Benchmark Suite`; its platform line
uses `warmup`, `measured` and `seed` consistently. CPU diagnostics belong with the
section measured, not only at process startup.

When requested by `FATP_BENCH_OUTPUT_CSV` or `FATP_BENCH_OUTPUT_JSON`, emit that
format with the established schema; output selection must not change measured
semantics. FatP's export fields include median, mean, standard deviation and
`ci95_low`/`ci95_high`; retain those fields when revising an existing exporter.
The normal-approximation interval describes the mean and carries its small-sample
limitations; it is not an interval for the median. The current report and dated
raw outputs use the locations recorded below. Preserve historical reports rather
than repeating their figures as current performance claims.

## Benchmark defaults and output locations

Source observation: 2026-09-06, [FatPBenchmarkRunner.h](../../include/fat_p/FatPBenchmarkRunner.h),
`BenchConfig::fromEnv()` and its `kDefault*` constants. This is the shared runner's
configuration, not a promise that every historical benchmark delegates to it.

| Environment setting | Shared runner default or behavior |
|---|---|
| `FATP_BENCH_WARMUP_RUNS` | 3 unreported runs. |
| `FATP_BENCH_BATCHES` | 15 on Windows; 50 elsewhere. |
| `FATP_BENCH_SEED` | 12345. |
| `FATP_BENCH_MIN_BATCH_MS` | 50 milliseconds. |
| `FATP_BENCH_VERBOSE_STATS` | Off when unset. |
| `FATP_BENCH_OUTPUT_CSV`, `FATP_BENCH_OUTPUT_JSON` | Empty/unset disables that export; otherwise the configured path is used by the exporter. |
| `FATP_BENCH_NO_SCOPE`, `FATP_BENCH_NO_STABILIZE`, `FATP_BENCH_NO_COOLDOWN` | Opt-outs off when unset. |
| `FATP_BENCH_QUICK` | Off when unset; when enabled, forces no cooldown and no stabilization. Workload reduction is benchmark-specific. |
| `FATP_BENCH_TARGET_WORK` | Not parsed by shared BenchConfig. Each benchmark that supports it defines and prints its work unit/default. |

The current `hasEnvVar` helper tests presence (on Windows, a nonempty value).
Consequently, setting a switch to the string `"0"` still enables it. Unset a switch
to disable it; use `"1"` to enable it in workflow configuration. Do not describe
these options as parsing Boolean text. The shared cooldown constants are
section/size/case delays of 2000/1000/300 ms on Windows and 1000/500/200 ms
elsewhere. These are bypassed when the selected mode disables cooldown.

[FatPBenchmarkHeader.h](../../include/fat_p/FatPBenchmarkHeader.h) formats output;
its `HeaderConfig` has its own initial field values and is not an environment
parser. Populate it from the resolved benchmark configuration rather than
printing its default `measured = 15` for a runner using 50 batches.

Concrete configurations illustrate why the source/override distinction matters:

- [Tensor layout benchmark](../../components/Tensor/benchmarks/benchmark_Tensor.cpp)
  reads target work with default 5,000,000 logical elements per calibrated batch.
- [Hash-map benchmark workflow](../../.github/workflows/fatp-hash-map-benchmarks.yml)
  defaults to 5 measured batches and target-work input 100,000; it sets warmup 3,
  quick mode and no cooldown/stabilization. Passing an environment variable alone
  does not prove a particular benchmark implementation consumes it.
- [Unified dispatcher](../../.github/workflows/run-all-benchmarks.yml) defaults to
  20 batches and target-work input 100,000. It currently also has push/PR triggers
  and dispatches component workflows. This is existing drift from the manual-only
  timing policy above, not a new exception ratified by guideline adoption.

The repository's report index is
[benchmark_results_README.md](../../benchmark_results/benchmark_results_README.md).
Family reports use `benchmark_results/Benchmark Results - <Family>.md`; archived
workflow logs are under `benchmark_results/logs/`. Component-local raw data and
dated protocols are under `components/<Component>/results/`; for example,
[TensorRanked's 2026-09-04 protocol](../../components/Tensor/results/2026-09-04-ranked/README.md)
links its CSV/JSON. The hash-map workflow writes compiler-specific CSV/log files
into workflow artifacts. Discover and inspect the applicable report and raw data
before citing them: an existing filename or index entry is not current evidence.

## Teaching and contribution routes

The project's established teaching types and their additional records are in
[TEACHING_TYPES](TEACHING_TYPES.md), read with the complete base
[Teaching](../modules/TEACHING.md) module before writing or reviewing teaching
documents. Operational guidelines, profiles and this document remain governed
by [Documentation](../modules/DOCUMENTATION.md).

[CONTRIBUTING](../../CONTRIBUTING.md) owns the public contribution process and
[LICENSE](../../LICENSE) owns the MIT license. Historical AI participation and
project-origin records are evidence of authorship, not current permission to
invoke peers, publish, or require the owner to operate tools an assistant can use.
