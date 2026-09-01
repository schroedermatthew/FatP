# Tensor execution contexts — implementation and measurement record

Date: 2026-08-31. Base revision: eec75394. The local evidence below was recorded
before publication; the later remote gate closure is recorded separately.

## Delivered contract

- TensorExecutionContext defaults to serial; only explicit parallel(pool) borrows
  workers. The ordinary TensorMatmul.h dependency graph does not acquire ThreadPool.
- Context overloads cover matmul and dot; no hidden pools or backend placeholders.
  Batch-times-row ranges are independent, inner folds remain serial, and a
  one-row result cannot use more than one task.
- Default grain: 32 rows. Default minimum work: 1,048,576 scalar products.
  maxTasks=0 means pool-size cap; nested calls on any Fat-P pool worker run serially.
- Caller-owned pool and PMR scratch; unchanged result allocator selection.
  Scratch allocates only a future array, not task/promise or Tensor element storage.
  Share scratch only if the resource is thread-safe or calls are serialized.
- All accepted tasks drain before return or throw. Submission error precedes
  lowest-index task error, which precedes cancellation. Cooperative cancellation
  has no hard latency bound and can discard a fully computed result at the final check.
- Floating agreement assumes the same build and floating environment. Caller TLS
  and rounding state are not propagated.

These delivered-contract bullets describe the base execution-context increment.
The later contraction increment added the current `tensorDot` context overloads;
see the user manual and contraction record for that extension.

The ThreadPool prerequisites reject submissions after a synchronized shutdown
cutoff, roll back failed enqueue accounting, account for partially accepted
batches, join concurrent shutdown callers, and expose isAnyPoolWorkerThread().
Own-worker shutdown is rejected before mutation; own-worker pool destruction
remains an invalid lifetime arrangement.

## Measurement protocol

Hardware: Intel Core Ultra 9 285K, 24 logical processors, Windows x64, Balanced
power plan. No affinity or priority override. No simultaneous local compilation
or test load during the recorded performance runs; OS activity and hybrid-core
migration are not controlled. CPU frequency snapshots are included in raw samples.

One reused caller-owned pool is active per problem. Baseline: the same native
matmul kernels without a context. All paths allocate, initialize, observe, and
destroy a fresh owning result inside timing. Input setup, pool creation, and
caller-thread construction are outside timing. No external competitor library,
fast-math, LTO, cache flush, or cold-cache claim.

Each compiler covers 28 problems and three variants: serial, context_default,
and context_forced (minimumWork=0, other settings unchanged). Workers: 1/2/4,
deliberately capped rather than presented as a full-machine scaling study.
Cases cover 16/32/64/128/256-square, grain 1/8/32/64, padded and batch-one-row
layouts, two concurrent callers, four saturated nested callers, and selected
32/64/128 cases with ThreadPool's default 2000-microsecond spin setting.
Other cases use spin=0; each case exports its actual setting.

Configuration: three warmup rounds, nine measured rounds, 20 ms calibration
target, target work 10,000, seed 12345; quick mode off, stabilization and cooldown
off. Case and variant orders are shuffled. Standard-library shuffle differences
mean identical seeds do not imply identical permutations across compilers.
Bitwise results are checked before and after timing.

Statistics are dispersion of batch means, not individual-call p95/p99 latency.
Concurrent rows report inverse throughput (elapsed time / all completed calls),
not the latency experienced by an individual caller. Mean CI95 intervals are
approximate; medians are primary. Calibration does not guarantee every later
sample meets 20 ms as CPU/scheduler state changes; all raw samples are retained.

Files:

- [MSVC JSON](msvc.json), [MSVC CSV](msvc.csv), [MSVC console](msvc.txt).
- [GCC JSON](gcc.json), [GCC CSV](gcc.csv), [GCC console](gcc.txt).

## Recorded outcomes

Medians in microseconds per completed call. Each cell is serial / context-default;
the listed matrix cases use four workers. Concurrent/nested rows are inverse
throughput, not individual-call latency.

| Case | MSVC | GCC |
|---|---:|---:|
| 128-square, spin 0 | 943.89 / 338.55 | 310.00 / 112.29 |
| 256-square, spin 0 | 8164.03 / 2456.11 | 2736.48 / 851.42 |
| 128-square, spin 2000 | 909.33 / 277.48 | 309.50 / 119.77 |
| Two concurrent callers | 424.34 / 225.51 | 156.79 / 87.61 |
| Four nested callers (serial fallback) | 284.12 / 261.75 | 89.68 / 86.91 |

The 128-square default context was about 2.8x faster than the matched serial
path with spin=0 on both compilers. At 256-square the measured ratios were
3.3x MSVC and 3.2x GCC. These are local observations, not portable guarantees.

Small-work evidence explains the conservative cutoff. GCC 32-square at
spin=2000 and grain=1 took 4.96 us serial versus 31.12 us with forced scheduling;
64-square took 39.03 versus 36.21 us (only a small improvement), while 128-square
clearly benefited. MSVC had lower scheduling overhead and benefited earlier.
Thus the default is intentionally not tuned to the lowest crossover of the
fastest pool/toolchain combination. A caller can lower minimumWork explicitly.

All 84 variant rows and 756 raw samples per compiler were validated against
the CSV exports, including sample counts, normalized times, and recomputed
medians. Of those samples, 124 MSVC and 72 GCC batches were shorter than the
20 ms calibration target; none were discarded. Small percentage differences,
especially between serial fallback variants, should not be read as robust
speedup claims on this unpinned Balanced-power machine.

MSVC 19.51 used C++20, /O2 /MD /DNDEBUG /EHsc /fp:strict /permissive- /utf-8
and /W4 /WX (standard alignment/constant-condition exemptions).
GCC 16.1 used C++20, -O3 -DNDEBUG -ffp-contract=off and strict warnings.
Clang 22.1 builds the benchmark under the same GNU-style floating settings;
its quick run is a functional smoke check, not performance evidence.

Measured source SHA256:

- benchmark_TensorExecution.cpp:
  8598CF3849399598F736A06BB0CABF9F1C718033C9166C13E1A1EA460C833BB5
- tensor/TensorExecution.h:
  DD4E6894C342242295EB9EB4A582CC04766CF60901D18560CEE303951A1AD4F9

## Validation and review

- 29 CMake Debug tests: Tensor, facade self-containment, stride/iteration support,
  and ThreadPool all pass, including MSVC checked iterators.
- Four existing consumers pass: CSRMatrixParallel, CSRMatrix_HPC,
  CSRMatrix_HPC_Parallel, and DiagnosticLogger_IO.
- The pre-publication execution suite passed MSVC Debug/Release, GCC 16.1 and
  Clang 22.1 with warnings treated as errors. MSVC AddressSanitizer passed with
  ordinary-new interception disabled. This local evidence did not include Linux
  UBSan/TSan; the later remote evidence is recorded below.
- Allocation fault sweeps run in standalone non-checked-iterator builds: 48
  caller-thread allocation positions through scheduling, plus both queue kinds
  around capacity boundaries. Checked Debug and ASan still test throwing PMR
  scratch and result allocators without replacing global new.
- Saturated same-pool/cross-pool nested calls, concurrent callers, task-count
  bounds, disjoint coverage, arithmetic/cancellation precedence, bitwise floating
  folds, and pre/mid-call cancellation are tested.
- The CMake Release benchmark target builds. MSVC Include-All compiles; a three-translation-unit aggregate runs TensorExecution,
  TensorMatmul, and ThreadPool together. Metadata, workflow generation/parsing,
  changed-header layers, and whitespace checks pass.
- Regression-first evidence: the old pool returned a stranded future after
  shutdown and left pending_tasks at SIZE_MAX after a throwing second batch copy.
  The new tests pass after the fixes.
- Full checked Debug exposed a noexcept PMR-vector proxy allocation failure;
  a directly allocated future array fixes it. ASan exposed a test assertion that
  retained a reference into a temporary dot result; the test now retains its owner.
- Claude and Grok reviewed the design and final future-array implementation via
  the local peer connections. Both found no remaining lifetime/draining defect.
  Claude's final benchmark review accepted the harness fixes; its spin-setting
  concern led to explicit 2000-microsecond probes and per-case exports.
  One Grok file-based review failed during source changes; subsequent immutable
  source reviews completed. Reviewer opinions are not substituted for test runs.

## Remote CI closure

- [TensorExecution CI](https://github.com/schroedermatthew/FatP/actions/runs/33415615587)
  and [ThreadPool CI](https://github.com/schroedermatthew/FatP/actions/runs/33415615551)
  passed Linux AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer
  for the delivered context surface and its pool prerequisite on commit 42fca1a.
  Their ordinary warning-as-error jobs exposed two unrelated portability warnings,
  which were corrected before the final aggregate run.
- [Aggregate FatP CI](https://github.com/schroedermatthew/FatP/actions/runs/33474185706)
  passed on commit 72d3495b across GCC 12/13/14, Clang 16/17, MSVC C++20/C++23,
  strict warnings, self-containment, AddressSanitizer, UndefinedBehaviorSanitizer,
  and ThreadSanitizer. That revision isolates global-new allocation probes from
  incompatible aggregate and checked-iterator modes; it does not expand the
  execution API. The remote gate for this bounded increment is complete.

## Limits and remaining scope

The cutoff is a conservative portable starting point, not a promise to find the
fastest mode on every CPU, compiler, shape, or pool configuration. Keeping it high
deliberately gives up some small/batched-case speedups, including on spinning pools,
to avoid scheduling regressions on sleeping pools. Applications can lower it after
measuring their actual workloads. No hidden autotuning changes existing calls.

Column tiling, additional Tensor algorithms, foreign-executor coordination, and
alternate backends remain separate increments. The completed remote gate validates
the delivered surface; it does not imply those broader Phase 9 additions.

## Modified Files (32)

Paths are relative to the repository root.

| File | Intent |
|---|---|
| include/fat_p/TensorExecution.h | Optional public facade; serial users need not include it. |
| include/fat_p/tensor/TensorExecution.h | Borrowed execution context, options, cancellation, bounded draining scheduler, and overloads. |
| include/fat_p/tensor/TensorMatmul.h | Shared serial/parallel row-range kernels. |
| include/fat_p/ThreadPool.h | Shutdown admission, failure accounting, and any-pool worker identity. |
| components/Tensor/tests/test_TensorExecution.cpp | Execution contract, cancellation, nesting, and allocation-failure tests. |
| components/Tensor/tests/test_TensorExecution_HeaderSelfContained.cpp | Standalone facade compilation and serial smoke test. |
| components/ThreadPool/tests/test_ThreadPool.cpp | Shutdown/copy-failure regressions and worker/race coverage. |
| components/FatPTest/tests/IncludeAllFatPHeaders.h | Include the optional facade in the hygiene gate. |
| components/FatPTest/tests/test_FatP.h | Declare the execution test entry point. |
| components/FatPTest/tests/test_FatP.cpp | Run execution tests in the aggregate suite. |
| components/Tensor/benchmarks/benchmark_TensorExecution.cpp | Reproducible crossover, spin, grain, nesting, and concurrency benchmark. |
| cmake/FatPComponents.cmake | Test timeout and strict floating benchmark flags. |
| tools/generate_workflows.py | Generate execution CI and sanitizer source/flag coverage. |
| .github/workflows/tensor-execution.yml | New compiler, Debug, header, strict-warning, and sanitizer gates. |
| .github/workflows/tensor.yml | Include execution in Tensor sanitizer coverage. |
| .github/workflows/tensor-benchmarks.yml | Add the execution suite to manual benchmark matrices. |
| .github/workflows/run-all-benchmarks-gcc.yml | Register the GCC execution benchmark. |
| .github/workflows/run-all-benchmarks-clang.yml | Register the Clang execution benchmark. |
| .github/workflows/run-all-benchmarks-msvc.yml | Register the MSVC execution benchmark. |
| components/Tensor/docs/User Manual - Tensor.md | Document public API, defaults, lifetime, cancellation, and limits. |
| components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md | Record the bounded increment, initial TSan gate, and later remote closure. |
| components/ThreadPool/docs/User Manual - ThreadPool.md | Replace obsolete shutdown advice with the actual cutoff contract. |
| README.md | Surface TensorExecution and refresh current inventory. |
| Authors.md | Refresh current inventory. |
| Read_Me/Fat-P_AI_Collaborative_Development_Methodology.md | Refresh current inventory and benchmark row without rewriting historical totals. |
| components/Tensor/results/2026-08-31-execution-contexts/README.md | Measurement protocol, decisions, validation, reviews, and this manifest. |
| components/Tensor/results/2026-08-31-execution-contexts/msvc.json | MSVC machine-readable results and raw samples. |
| components/Tensor/results/2026-08-31-execution-contexts/msvc.csv | MSVC per-sample table. |
| components/Tensor/results/2026-08-31-execution-contexts/msvc.txt | MSVC console record. |
| components/Tensor/results/2026-08-31-execution-contexts/gcc.json | GCC machine-readable results and raw samples. |
| components/Tensor/results/2026-08-31-execution-contexts/gcc.csv | GCC per-sample table. |
| components/Tensor/results/2026-08-31-execution-contexts/gcc.txt | GCC console record. |
