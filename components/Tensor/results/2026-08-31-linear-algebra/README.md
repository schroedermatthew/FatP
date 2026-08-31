# Tensor linear-algebra measurements — 2026-08-31

## Scope and contract

This directory records Windows-x64 measurements of `dot`, `outer`, `matmul`,
`diagonal`, and `trace`, not a comparison with BLAS or a general tensor library.
The production baseline is commit `d7658a76`. The benchmark source is
`components/Tensor/benchmarks/benchmark_TensorMatmul.cpp`.
The original full-run executable SHA-256 hashes are
`07bc6a597ddf993a6f00d1aa94edfc311007e8680c8f006c7cdd61aa1804dac6` (MSVC) and
`5ec348f2d53be6be7d24b3e291b2ddbfacab6f78c538c1f34d2daf3068f556ea` (GCC).
Focused comparison binaries record their hashes
inside the corresponding JSON files.

Each problem has three independently calibrated, round-interleaved variants:

- `fat_p`: the allocating public API, using borrowed read-only inputs.
- `scalar_prevalidated`: straightforward coordinate loops, with the same Tensor
  result allocator and result ownership, but without the public API's shape and
  layout validation. This is an overhead/algorithm control, not API equivalence
  or a competitive GEMM implementation.
- `allocation_only`: the same result construction, zero initialization,
  observation, and destruction, without arithmetic. It is not bare malloc/free.
  Its median must not be subtracted from another median to invent kernel time.

Input generation and all-output correctness checks occur outside timing.
Result allocation, initialization, observation, and destruction occur inside
timing for all three variants. The result escapes via a volatile indirect
function call. The actual observer samples a runtime-selected element; it does
not add a whole-result checksum traversal. Fixed observer overhead remains in
the absolute numbers, especially for small cases. No LTO or fast-math is used.

Allocation observations come from a separate result-allocator probe, not the
timed allocator. They count only result element buffers and requested bytes;
they exclude metadata, shared-ownership control blocks, process allocations,
allocator overhead, and input storage. Reclamation is checked after destruction.

## Matrix

There are 126 problems, each measured with all three variants:

| Operation | Small | Medium | Large | Layouts |
|---|---|---|---|---|
| dot | K=16 | K=1024 | K=65536 | contiguous, padded, reversed |
| outer | 8x12 | 64x96 | 256x384 | contiguous, padded, reversed |
| matmul | 8x12 @ 12x10 | 48x64 @ 64x40 | 128x192 @ 192x96 | contiguous, padded, reversed, transposed, batched |
| diagonal / trace | 8x12 | 128x192 | 512x768 | contiguous, padded, reversed, transposed, batched |

Both float and double are included. Batched matrix cases use four left batches
and a singleton right batch; diagonal/trace retain four batch domains. Vector
operations do not support transposed/batched input ranks, so no such cases are
invented. Padded layouts have step two, reversed layouts reverse the final axis,
and transposed layouts have column-major physical strides for the same logical
shape. Nonzero origins and guard padding catch incorrect root/origin handling.

Seeded nonconstant multiples of 1/8 keep the bounded float/double reference
arithmetic exactly comparable, with FMA contraction disabled. Every output
element and output shape is checked before and after measurement.
These exactly representable inputs test data, shape, and traversal correctness;
they are not a rounding-order or FMA-contraction oracle. Cancellation and special
floating-value semantics are checked separately in the regression suite.

## Machine and build

- Windows x64; Intel Core Ultra 9 285K, 24 cores / 24 logical processors.
- Windows Balanced power plan left unchanged.
- MSVC 19.51; `/std:c++20 /O2 /MD /DNDEBUG /EHsc /fp:strict /permissive-`.
  The local baseline uses MSVC's default volatile mode; CI pins `/volatile:iso`.
- MSYS2 UCRT GCC 16.1.0; `-std=c++20 -O3 -DNDEBUG -ffp-contract=off`.
- Baseline ISA, no `-march=native`, no LTO, no fast-math, serial kernels only.
- BenchmarkScope requests high priority and a nonzero logical CPU affinity.
  This hybrid CPU's sampled frequency is context, not proof of a thermal fault.

Local compiler commands (from the repository root; output location is arbitrary):

```powershell
# Run in a Visual Studio x64 developer environment.
cl /nologo /std:c++20 /O2 /MD /DNDEBUG /EHsc /fp:strict /permissive- /utf-8 /W4 /WX /wd4324 /wd4127 /Iinclude\fat_p components\Tensor\benchmarks\benchmark_TensorMatmul.cpp /Fe:bench-msvc.exe /link advapi32.lib
& 'C:\msys64\ucrt64\bin\g++.exe' -std=c++20 '-Wa,-mbig-obj' -O3 -DNDEBUG -ffp-contract=off -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wno-cast-function-type -I include -idirafter include/fat_p components/Tensor/benchmarks/benchmark_TensorMatmul.cpp -ladvapi32 -o bench-gcc.exe
```

## Run protocol

```powershell
$env:FATP_BENCH_WARMUP_RUNS = '3'
$env:FATP_BENCH_BATCHES = '15'
$env:FATP_BENCH_MIN_BATCH_MS = '20'
$env:FATP_BENCH_TARGET_WORK = '10000'
$env:FATP_BENCH_SEED = '12345'
$env:FATP_BENCH_OUTPUT_JSON = 'msvc-baseline.json'
$env:FATP_BENCH_OUTPUT_CSV = 'msvc-baseline.csv'
.\bench-msvc.exe 2>&1 | Tee-Object msvc-baseline.txt
# Repeat separately for GCC, with gcc-baseline output names. Do not run concurrently.
```

Leave `FATP_BENCH_QUICK`, `FATP_BENCH_NO_SCOPE`, `FATP_BENCH_NO_STABILIZE`, and
`FATP_BENCH_NO_COOLDOWN` unset for these full runs. Quick mode limits the matrix
to the small tier and is only a smoke test, never performance evidence.

Primary units are ns per complete call, not ns per vaguely defined element.
The target-work setting counts scalar products for dot/outer/matmul and visited
diagonal positions for diagonal/trace, solely to seed calibration. Every case
and all three implementation orders are shuffled per round. Implementations
of a case are adjacent within the same round. JSON records each raw elapsed
duration, repetition count, and sampled CPU frequency; CSV repeats summary
statistics alongside every raw sample.

Reported dispersion is between batch means, not individual-call latency.
CI95 is an approximate interval for the mean, not the median. Calibration can
fall below its requested floor after frequency/cache changes; those samples
are retained and explicitly warned about, not silently selected out.

## Interpretation limits

These are reused-input, repeated allocate/free measurements. They include a
warm allocator free-list regime and do not measure cold starts, first touch,
unique-input streams, input materialization, or cache flushing. The entire
matrix is resident; this is not a guarantee that every large input stays in
L1/L2. Inputs are floating only; integer checking costs need separate evidence.

Ratios against the scalar control mix validation, metadata, traversal, and
algorithm effects. They are not proof that a particular instruction or allocator
causes a gap. Overlapping allocation-control/full-API distributions are poor
kernel-optimization targets. No table-wide statistical significance claim is
made across 126 comparisons.

An optimization candidate must show a substantial, repeatable gap on both local
compilers, then undergo a fresh-seed, randomized before/after experiment. The
decision target is at least 10% median improvement with a paired bootstrap
interval excluding zero, plus correctness/regression tests. No speed claim is
made for unmeasured layouts or other platforms.

## Full baseline results

Both runs completed all 126 problems and 378 implementation series; each
contains 5,670 raw measured batches. `analyze.py` independently recomputes
means, medians, and sample standard deviations and checks JSON/CSV agreement.

| Baseline | Batches below requested 20 ms | Minimum batch | Median series CV | Maximum series CV |
|---|---:|---:|---:|---:|
| MSVC | 109 / 5,670 | 17.814 ms | 1.79% | 15.00% |
| GCC | 651 / 5,670 | 7.301 ms | 12.37% | 65.94% |

CV means standard deviation divided by mean across timed batch averages.
The GCC run was noisier, particularly for short calls. Raw samples are retained;
this table is not evidence of stable clocks, individual-call tail latency, or
repeatability across machines. The broad baseline selects candidates, not winners.

Large contiguous dot (`K=65536`), ns per allocating call:

| Compiler / type | Public API median | Scalar control median | Allocation-only median |
|---|---:|---:|---:|
| MSVC / float | 263,984 | 25,421 | 342 |
| MSVC / double | 267,302 | 25,403 | 338 |
| GCC / float | 261,868 | 24,925 | 99 |
| GCC / double | 262,091 | 25,061 | 100 |

All probed nonempty outputs used one result-element allocation and reclaimed
it on destruction. This is not a claim of one total process allocation per API
call. Raw files cover outer/matmul/diagonal/trace and every listed layout too;
no blanket speed ranking is inferred from their scalar-control ratios.

As an empirical dead-code-elimination sanity check, the double scalar controls
grow across all three sizes: for dot, MSVC is 344 / 721 / 25,403 ns and GCC is
106 / 486 / 25,061 ns; for outer, MSVC is 478 / 4,800 / 64,237 ns and GCC is
223 / 1,417 / 20,288 ns. This is consistent with retaining size-dependent work,
not a proof about every instruction or every possible compiler configuration.
The unknown volatile call target is the primary whole-buffer escape mechanism.

## Focused before/after protocol

The candidate widens the existing contiguous-kernel dispatch to two rank-one
operands. Shape and lifetime validation still precede dispatch; the existing
checked arithmetic, zero seed, result allocator, and serial contraction order
are unchanged. Strided vectors and mixed-rank/batched forms stay generic.

Both binaries use identical benchmark source and compiler flags; only the
production dispatch differs. The full-baseline binaries predate the optional
`--filter substring` CLI and clearer output-contract text. Those later changes
do not change a timed variant. Filtering happens before fixture construction;
focused runs therefore have a smaller resident-input set than the full matrix
and must be compared to each other, not directly to full-matrix medians.

`analyze.py --before <exe> --after <exe> --compiler <label> --output <new.json>`
performs 15 adjacent process pairs for each of float and double at K=65536.
Case order and before/after order are shuffled using seed 98765; each pair uses
the same fresh input seed (98765 + pair index), distinct from discovery.
Every process uses three warmups, seven measured batches, a 20 ms calibration
target, scope/affinity enabled, and no stabilization wait or inter-round cooldown.
Processes run sequentially, without concurrent builds or other benchmark jobs.
The control variants remain present and randomized inside every process.

One process's API median is one replicate. The analysis computes the median
of the 15 relative improvements `(before - after) / before` and a percentile
interval from 10,000 whole-pair bootstrap draws. It does not pretend the nested
seven batches are 105 independent before/after pairs. The two data types are
preselected confirmation cases, not a table-wide multiple-comparison search.
Consolidated comparison exports retain process order, seeds, binary hashes,
stdout/stderr, and every raw sample from both versions.

The component workflow runs the full matrix manually. The unified GCC/Clang/MSVC
sweeps use quick mode and remain build/smoke checks, not substitutes for this
full dataset. The TensorMatmul unified Linux entries override the runner's
generic native-ISA flag with `-march=x86-64`. CMake and manual workflows pin
strict/no-contraction floating options for this benchmark.

## Direct comparison result

| Compiler / type | Before median | After median | Median paired reduction | Paired bootstrap 95% interval |
|---|---:|---:|---:|---:|
| MSVC / float | 268,205 ns | 125,791 ns | 53.21% | [52.74%, 53.74%] |
| MSVC / double | 267,329 ns | 126,452 ns | 52.44% | [52.31%, 52.93%] |
| GCC / float | 263,967 ns | 124,086 ns | 52.76% | [52.14%, 53.08%] |
| GCC / double | 261,037 ns | 126,100 ns | 51.93% | [51.27%, 52.73%] |

Before/after columns are medians of process medians; the percentage is the
median of matched-pair reductions, not a ratio of the displayed medians.
All four confirmation cases exceed the preselected 10% effect target, with
whole-pair intervals excluding zero. The dispatch change is retained. This
supports an improvement for large contiguous floating dot on this machine,
not a general Tensor speedup, a BLAS claim, or an explanation of the entire
gap to the prevalidated scalar control. No default parallelism is introduced.

Unlike the noisy discovery matrix, none of the 840 API batches used for the
large-dot confirmation fall below the requested 20 ms target:

| Compiler / version | Short API batches | Shortest API batch | Median API-series CV |
|---|---:|---:|---:|
| MSVC / before | 0 / 210 | 33.470 ms | 1.23% |
| MSVC / after | 0 / 210 | 31.251 ms | 1.16% |
| GCC / before | 0 / 210 | 32.538 ms | 1.31% |
| GCC / after | 0 / 210 | 30.784 ms | 0.95% |

These are checks on the API samples actually used by the paired analysis;
control samples are still retained in full. The single-sample calibration is
not a guaranteed measured-duration floor. Broad discovery ratios are not used
as optimization acceptance evidence; rejection or subtraction of inconvenient
samples would not fix that limitation.

## Verification

### Small-vector regression guard

Following the results review, the same paired protocol was also run at K=16,
using `--case dot/float/small/contiguous --case dot/double/small/contiguous`.
The binaries and seeds are unchanged. These additional runs check for a
small-vector regression; they are not the preselected large-case performance
acceptance test, a kernel-only measurement, or evidence for every vector length.

| Compiler / type | Before median | After median | Median paired reduction | Paired bootstrap 95% interval |
|---|---:|---:|---:|---:|
| MSVC / float | 565.675 ns | 527.888 ns | 6.68% | [5.46%, 8.94%] |
| MSVC / double | 564.508 ns | 526.860 ns | 6.14% | [4.93%, 8.10%] |
| GCC / float | 169.794 ns | 135.763 ns | 20.42% | [19.07%, 21.32%] |
| GCC / double | 171.343 ns | 135.118 ns | 20.63% | [18.93%, 22.14%] |

No regression was observed in these small cases. Fixed allocation/observer
costs remain included. The small-case medians are not directly comparable
across compiler/runtime implementations as a measure of kernel quality.
The extra two `*-small-dot-comparison.json` files preserve all 120 processes
and 2,520 additional raw samples, using the same whole-pair estimator.
The MSVC small guard was noisier: 42/210 API batches in each version were below
20 ms (minima 15.981/14.272 ms; median series CV 11.34%/12.02%, before/after).
GCC's small guard had 0/210 short API batches in either version (minima
25.958/20.695 ms; median series CV 1.34%/1.53%). No samples were discarded.
The MSVC small result is a bounded no-regression observation, not a precision
latency specification or a reason to promote its small percentage to a headline.

### Correctness and integration

- All full JSON/CSV pairs agree; every stored median, mean, sample standard
  deviation, and approximate mean interval recomputes from the raw batches.
- Both comparison exports contain 15 pairs per type, 60 processes per compiler,
  and 1,260 raw samples per compiler. Process summaries and whole-pair bootstrap
  results independently recompute from the consolidated files.
- All sixteen linear-algebra test groups pass MSVC Debug, Release and ASan,
  GCC, and Clang. All 26 Tensor CMake tests pass.
- The benchmark compiles warning-clean on MSVC, GCC, and Clang. Its MSVC ASan,
  Clang, and CMake Release smoke runs verify all 42 small problems.
  Separate low-duration functional runs on MSVC, GCC, and Clang verify all 126
  problems after the dispatch change; their timings are not performance evidence.
- Invalid configuration, invalid CLI, an unmatched filter, and unwritable JSON
  or CSV destinations return failure. Modified workflows parse, and the
  TensorMatmul workflow matches its generator. Metadata and layer checks pass.
- Local checks are not remote CI evidence; no workflow was dispatched here.

## Local peer review

Claude and Grok reviewed the methodology and then the implementation using the
local peer connections. Both code reviews found no blocking production
correctness defect. Claude requested stronger right-operand coverage.
The tests now preserve the cancellation pattern while adding nonuniform right
values, NaN guard padding, independently strided right operands, and all four
contiguous/strided operand pairings across the block boundaries. A late integer
overflow is isolated to the final right-hand element as well.
The isolated new group rejects two temporary-header mutations: dropping the
right contiguity predicate and repeatedly reading the first right element.
Neither mutation was made in the working production header.

Claude also cautioned against small-dot kernel speed claims. None are made: allocating
call and observer costs remain in the measurement, and the confirmation cases
are the preselected large float/double dots. The follow-up small-case guard
checks for regressions without subtracting allocation/observer costs.
The observer was not changed during
the before/after experiment. Peer comments are review evidence, not test runs;
all execution and raw measurements above came from the owner's local tools.
Claude's results follow-up accepted the numerical-evidence separation and the
actual large-case batch-quality checks, and requested the completed small-case
guard. An additional Grok results-only job was stopped after returning no
verdict; its completed methodology and source reviews are the Grok evidence
recorded here, not approval of every number in this report.
After the GCC small-case results arrived, Claude confirmed that all three
acceptance findings from its excerpt review were resolved. That is a scoped
review conclusion, not a whole-repository approval or independent execution.
