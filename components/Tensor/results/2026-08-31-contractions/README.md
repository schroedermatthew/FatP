# Tensor contractions: implementation and validation

Implemented 2026-08-31 on top of 42fca1a. This report records pre-publication
local validation. The new Linux matrix and sanitizer jobs require a successful
CI run on the published commit.

## Delivered surface

- TensorContractions.h adds tensorDot(left, right, leftAxes, rightAxes[, allocator]).
- TensorExecution.h adds the corresponding explicit context overloads, with
  context fifth and optional result allocator sixth. Default calls remain serial.
- Axis lists are normalized and unique per operand; paired extents must match.
  Free output axes are left then right, each in original order. No broadcasting.
- Empty axis lists mean generalized outer product with a positive-zero seed.
  Full contractions give rank zero; scalar inputs and zero extents are supported.
- TensorMatmulType widening occurs before multiplication. Every integral product
  and sum is checked. Floating folds retain supplied pair order, last pair fastest.
- Planning is O(left rank + right rank) metadata, with no input packing or
  tensor-size offset tables. The last contracted axis is traversed as a strided
  run; its signed offsets are advanced only when a next reachable term exists.
- Borrowed/shared view lifetimes and first-owner rebound SOCCC allocator semantics
  match the existing Tensor APIs. Explicit allocators are used unchanged.
- No einsum grammar, path optimizer, hidden pool, BLAS dependency, or new backend.

See [the user manual](../../docs/User%20Manual%20-%20Tensor.md) for examples and
[the architecture plan](../../docs/Design%20Note%20-%20Tensor%20Architecture%20Additions%20Plan.md)
for the remaining gates.

## Prior execution-context CI and portability fixes

Public job status and authenticated build logs for commit 42fca1a establish:

- [TensorExecution CI](https://github.com/schroedermatthew/FatP/actions/runs/33415615587):
  Linux ASan, UBSan, and TSan passed, as did MSVC Debug/C++20/C++23.
- [ThreadPool CI](https://github.com/schroedermatthew/FatP/actions/runs/33415615551):
  all three sanitizers passed. GCC 12 failed an ordinary build.
- Ordinary Tensor/Matmul/Execution Linux builds and benchmark builds failed
  -Werror=range-loop-construct at TensorSlice.h:295. The originTerms structured
  binding now takes const auto& instead of copying each pair.
- The ThreadPool GCC 12 failure was -Werror=volatile at test_ThreadPool.cpp:470.
  The race-window test now uses dummy = dummy + k, preserving the volatile
  accesses without deprecated compound assignment.

These are narrow corrections to the observed diagnostics. Remote sanitizer
success applies to the prior execution commit, NOT to the unpublished contraction
implementation. Fresh CI is necessary; this report does not call the overall
Linux matrix green.

## Local validation

- Seven contraction groups pass with MSVC 19.51 C++20 Debug, C++latest Release,
  MSVC AddressSanitizer, GCC 16.1 C++20, and Clang 22.1 C++20, with warnings as errors.
- 720 seeded scalar differentials cover ranks 0-4, arbitrary paired axis subsets
  and orders, and signed/zero/overlapping readable strides over bool, int32,
  uint64, float, double, and long double. Expected folds recurse over coordinates
  independently of production planning and offset decoding.
- Fixed tests cover rank-five full contraction, matmul/dot/integer-outer equivalence,
  trillion-element output planning without result storage, zero inner/free extents,
  huge unreachable subdomains, invalid/duplicate/negative axes, extreme singleton
  strides, ordered floating cancellation, NaN/infinity/signed zero, widened
  products, checked overflow, allocation failure and reclamation, and view lifetimes.
- Context tests cover bitwise agreement, signed strides, nested calls, cancellation,
  scratch failure, checked task failure, shutdown submission, default threshold,
  task cap, one-tile fallback, and repeated cancellable serial tiles.
- All 31 Tensor/ThreadPool CMake Debug tests pass, including header self-containment
  targets and checked-iterator coverage.
- A focused five-translation-unit MSVC aggregate passes: a main including every
  public header plus TensorContractions, TensorExecution, TensorMatmul, and ThreadPool
  test translation units. This is not a claim that the entire FatP aggregate ran.
- Preprocessor dependency inspection confirms the serial TensorContractions facade
  does not include ThreadPool.
- The new benchmark builds on MSVC, GCC, Clang, and the CMake Release target.
  A Clang quick-mode correctness smoke run also passes; its timings are not
  used as performance evidence. GCC and Clang standalone facade checks pass.
- FATP_META inventory (352 scoped files), header-layer validation, all workflow YAML
  parsing, targeted generator equivalence, aggregate source collection, formatting
  of the new C++ files, and git diff --check pass.
- This Windows host has no Linux distribution/TSan runtime. New Linux ASan/UBSan/TSan
  coverage is wired into tensor-contractions.yml and the existing Tensor/execution
  sanitizer source lists; it is not a locally completed gate.

## Measurements

Hardware: Intel Core Ultra 9 285K, Windows, Balanced power plan; 24 logical threads.
Compiler runs were sequential with no concurrent compilation or stress tests.
The pool has four workers and the existing default 2000us spin; construction is
outside timing and an idle pool exists during every variant. No affinity, priority,
or cache-flush override is made.

The suite uses A[M,Q,K], B[K,N,Q], paired axes {2,1}/{0,2}, and a K-then-Q fold.
Cases are small (16,4,4,16), medium/reversed (32,16,16,32),
cutoff (64,8,32,64), and one-element output (1,64,64,1).
The reversed case reverses A's K mapping. The one-element output benchmark is
shape {1,1}; rank-zero behavior is covered separately in tests.

Each variant includes fresh Tensor allocation, zero initialization, computation,
observation, and destruction. The scalar baseline skips validation/planning and
uses a specialized independent four-loop implementation with identical fold order
and storage ownership. Full output bits are checked before and after timing.
This is not a BLAS or third-party-library comparison.

Each compiler records 20 case/variant rows and 180 measured batches, with three
warmup rounds, nine measured rounds, a 20ms calibration target, seed 12345,
and target_work=10000. Order is randomized per round; all measured batches met
20ms. Raw per-batch timings and CPU frequencies, medians, means, standard
deviations, and mean confidence intervals are retained. Dispersion is of batch
means, not individual-call tail latency.

Final run-kernel medians in microseconds per call:

| Case/compiler | Serial tensorDot | Default context | Forced context | Prevalidated scalar |
|---|---:|---:|---:|---:|
| Medium / MSVC | 131.09 | 130.25 | 43.03 | 101.23 |
| Medium / GCC | 124.85 | 124.45 | 50.69 | 75.97 |
| Cutoff / MSVC | 429.67 | 125.48 | 126.98 | 363.66 |
| Cutoff / GCC | 412.52 | 131.98 | 130.47 | 300.51 |
| Small / MSVC | 8.94 | 8.78 | 7.02 | 1.87 |
| Small / GCC | 8.48 | 8.49 | 27.41 | 1.27 |

The general API remains slower than specialized scalar code when serial. In these
medium/large cases the run kernel limits that overhead to about 1.2-1.6x.
Small tensors still pay proportionally more for planning; forced parallel work
is notably worse for the GCC small case. The unchanged conservative default
cutoff avoids that small-input scheduling cost. A full contraction with one
output always stays serial. These measurements do not establish a universal
optimal threshold.

Final rows' maximum coefficient of variation was 7.39% for MSVC and 7.86% for GCC.
See [MSVC CSV](run-kernel/msvc.csv), [MSVC JSON](run-kernel/msvc.json),
[GCC CSV](run-kernel/gcc.csv), [GCC JSON](run-kernel/gcc.json),
and the matching .txt console records.

The first implementation decoded every contracted coordinate for every product;
the preserved root-level msvc/gcc outputs document that exploratory run.
Its medium serial median was about 3.1ms, versus 0.13ms for the run kernel.
The [initial kernel snapshot](initial-kernel.txt) makes that discarded implementation
inspectable. These sequential exploratory/final runs are not a randomized A/B
comparison and do not support a platform-wide speedup claim.

Measured final source SHA256:
- tensor/TensorContractions.h:
  F68354BE928F14D3817B42EF539AC8DDAB51D1823784DBCF9F32700C990C3092
- benchmark_TensorContractions.cpp:
  57706DE6C5842E231C88D8110AA8134A6E95679F2BE648D3741F2B92688CEC0B

## Reproduction

From the repository root, in an x64 MSVC Developer Command Prompt:

~~~bat
cl /nologo /std:c++20 /O2 /MD /DNDEBUG /EHsc /fp:strict /permissive- /utf-8 /W4 /WX /wd4324 /wd4127 /Iinclude\fat_p components\Tensor\benchmarks\benchmark_TensorContractions.cpp /Fe:contractions-msvc.exe /link advapi32.lib
~~~

GCC build (PowerShell, the installed Windows compiler; Linux omits -ladvapi32 and
-Wa,-mbig-obj and adds -pthread):

~~~powershell
& 'C:\msys64\ucrt64\bin\g++.exe' -std=c++20 '-Wa,-mbig-obj' -O3 -DNDEBUG -ffp-contract=off -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wno-cast-function-type -I include -idirafter include/fat_p components/Tensor/benchmarks/benchmark_TensorContractions.cpp -ladvapi32 -o contractions-gcc.exe
~~~

Run each executable sequentially in PowerShell. Use new output filenames to
preserve the recorded artifacts:

~~~powershell
$env:FATP_BENCH_WARMUP_RUNS='3'
$env:FATP_BENCH_BATCHES='9'
$env:FATP_BENCH_MIN_BATCH_MS='20'
$env:FATP_BENCH_TARGET_WORK='10000'
$env:FATP_BENCH_SEED='12345'
$env:FATP_BENCH_NO_STABILIZE='1'
$env:FATP_BENCH_NO_COOLDOWN='1'
Remove-Item Env:FATP_BENCH_QUICK -ErrorAction SilentlyContinue
$env:FATP_BENCH_OUTPUT_CSV='contractions-local.csv'
$env:FATP_BENCH_OUTPUT_JSON='contractions-local.json'
.\contractions-msvc.exe
~~~

For tests, compile test_TensorContractions.cpp with ENABLE_TEST_APPLICATION,
C++20, and the same include roots. MSVC Debug used /Od /RTC1 /Zi /W4 /WX /EHsc
/permissive- /Zc:preprocessor /wd4324 /wd4127, Release used /O2 /DNDEBUG and
/std:c++latest, and ASan used /Od /Zi /fsanitize=address. GCC used -O2 with the
strict warning flags above; Clang used -O1 -Wall -Wextra -Wpedantic -Werror
-Wno-gnu-zero-variadic-macro-arguments. Windows test builds link advapi32.

CMake discovers the new source targets automatically:

~~~text
cmake --build <debug-build> --target test_TensorContractions test_TensorContractions_HeaderSelfContained
ctest --test-dir <debug-build> -R "Tensor|ThreadPool" --output-on-failure
cmake --build <release-build> --target benchmark_TensorContractions
~~~

## Independent review

Claude reviewed the complete final kernel snapshot and found no blocking
correctness issue. Its review checked signed offset reachability, singleton
extreme strides, empty axes, and fold order. Earlier feedback led to explicit
cross-operation equivalence, scheduling-fallback, high-rank, and metadata-size
tests, plus clearer signed-zero and empty-output documentation.

The owner did not accept stale repository-state guesses from the initial
filesystem review: the new files are uncommitted, the headers do exist, and
the CI diagnostics came from authenticated logs. Two suggested hazards are
already excluded: innerRun is initialized to 1 and is changed only to a nonzero
extent, and normalized duplicate axes are rejected and tested. Bounding the
minimum/maximum affine offsets proves reachability for all Cartesian coordinates;
no enumeration of every coordinate is required.

Grok's bounded final review examined the stride-loop reasoning, not the full
repository, and found no blocker in the loop/offset proof. It asked about empty
sums, uniqueness on each side, and the count type. The actual implementation
zero-initializes the result before skipping a zero-inner writer, calls
normalizeAxes separately for both operands, and uses size_t for both the checked
count and loop indices. All three conditions have explicit tests or visible
type constraints. Its earlier full review failed or did not return; it is not
counted as completed review evidence.

Completed local-peer sessions:
- Claude final full-header review: 0097ab83-2f65-4a80-b47b-8945834bb3ef.
- Grok final loop-proof review: 70a0412d-0206-4789-ab18-8378dddadc1c.

No online Claude/Grok site was opened. Neither peer executed the tests or
benchmarks; validation and timing evidence above came from the owner's tools.

## Remaining work

After publication, require the complete Linux compiler/sanitizer matrix to pass.
Further output-iteration optimization, axis coalescing, packing,
and contraction-path search need their own measurements and contracts. Full
einsum remains deliberately absent.

## ModifiedFiles

The manifest below includes source, integration, documentation, and raw evidence
files for this increment. Temporary local build products are not repository changes.

- `.github/workflows/run-all-benchmarks-clang.yml`
- `.github/workflows/run-all-benchmarks-gcc.yml`
- `.github/workflows/run-all-benchmarks-msvc.yml`
- `.github/workflows/tensor-benchmarks.yml`
- `.github/workflows/tensor-contractions.yml`
- `.github/workflows/tensor-execution.yml`
- `.github/workflows/tensor.yml`
- `Authors.md`
- `README.md`
- `Read_Me/Fat-P_AI_Collaborative_Development_Methodology.md`
- `cmake/FatPComponents.cmake`
- `components/FatPTest/tests/IncludeAllFatPHeaders.h`
- `components/FatPTest/tests/test_FatP.cpp`
- `components/FatPTest/tests/test_FatP.h`
- `components/Tensor/benchmarks/benchmark_TensorContractions.cpp`
- `components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md`
- `components/Tensor/docs/Design Note - Tensor Semantic Contract.md`
- `components/Tensor/docs/User Manual - Tensor.md`
- `components/Tensor/results/2026-08-31-contractions/README.md`
- `components/Tensor/results/2026-08-31-contractions/gcc.csv`
- `components/Tensor/results/2026-08-31-contractions/gcc.json`
- `components/Tensor/results/2026-08-31-contractions/gcc.txt`
- `components/Tensor/results/2026-08-31-contractions/initial-kernel.txt`
- `components/Tensor/results/2026-08-31-contractions/msvc.csv`
- `components/Tensor/results/2026-08-31-contractions/msvc.json`
- `components/Tensor/results/2026-08-31-contractions/msvc.txt`
- `components/Tensor/results/2026-08-31-contractions/run-kernel/gcc.csv`
- `components/Tensor/results/2026-08-31-contractions/run-kernel/gcc.json`
- `components/Tensor/results/2026-08-31-contractions/run-kernel/gcc.txt`
- `components/Tensor/results/2026-08-31-contractions/run-kernel/msvc.csv`
- `components/Tensor/results/2026-08-31-contractions/run-kernel/msvc.json`
- `components/Tensor/results/2026-08-31-contractions/run-kernel/msvc.txt`
- `components/Tensor/tests/test_TensorContractions.cpp`
- `components/Tensor/tests/test_TensorContractions_HeaderSelfContained.cpp`
- `components/ThreadPool/tests/test_ThreadPool.cpp`
- `include/fat_p/TensorContractions.h`
- `include/fat_p/tensor/TensorContractions.h`
- `include/fat_p/tensor/TensorExecution.h`
- `include/fat_p/tensor/TensorSlice.h`
- `tools/generate_workflows.py`
