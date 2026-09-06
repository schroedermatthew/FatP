# Benchmarking Module

**Authority:** [CORE](../CORE.md). Project facts belong in [PROJECT_PROFILE](../PROJECT_PROFILE.md).

Assumes `../CORE.md` has been read. Writing/reviewing a benchmark implementation
or results report requires this complete module. Work that only cites a result or
makes a performance claim requires the complete claims-and-citations section below
and the actual supporting evidence. If new measurement or review of the measurement
method is needed, the full procedure applies. No profile switch disables it.

---

## 1. Claims and citations

**No performance claim without a measurement behind it.** This binds everywhere prose lives: header comments, teaching documents, commit messages, review responses, README text, chat with the owner. "Should be faster because it avoids an allocation" is a hypothesis, not a claim; state it as one or measure it.

The failure mechanism: unmeasured performance claims turn plausible intuition into durable misinformation. Compilers, allocators, and caches routinely invert intuitive cost models, and a wrong number in a header outlives the code it described. A claim that turns out false after shipping is a corpus-integrity failure, not a rounding error.

Placement rule:

- Benchmark numbers live in **benchmark results reports**, one current report per benchmark family (see §9).
- Headers and user-facing prose may describe **complexity and design intent** ("O(1) amortized", "no allocation on the hot path" — when structurally true), never measured throughput/latency numbers. Numbers rot with hardware; structure does not.
- If a comparative claim survives into prose ("competitive with [category] alternatives"), it must cite the report that established it.

Before citing, inspect the report's source state/date, hardware, toolchain/build
configuration, workload, units, compared semantics, method, spread, and limitations.
Keep the claim within those measured conditions. A historical report is not current
verification. Missing evidence means narrow the statement or label a hypothesis;
do not manufacture a run. Structurally proved complexity and design intent are not
timing measurements and must be supported by the actual mechanism.

An explicitly historical number in a teaching case study may describe a dated
measurement with its source and limits; it must not imply current performance.
This exception does not permit numbers in headers or undated claims of superiority.
For teaching-document form, use [Teaching](TEACHING.md) when that task applies.

## 2. Benchmarks are not tests

Correctness is established by the test suite (`TESTING.md`); performance is established by benchmarks. Keep the machinery separate:

- Benchmark translation units never join the test suite, and test files never contain timed assertions ("must complete in under N ms" is a flaky test, not a benchmark).
- Benchmarks build as **separate targets** with release/optimized flags; tests keep their assertions effective. Tests may also run optimized builds; the
  separation prevents measurement code and timing assertions from becoming the correctness suite.
- Benchmarks are **excluded from the ordinary CI gate**. They run via manual dispatch or a dedicated workflow, unless the project explicitly establishes a controlled performance runner and a
  justified threshold. Shared-runner timing failures are not correctness evidence. Gate placement and workflow wiring route to `WORKFLOW.md`.
- Benchmark counts are not verification-baseline counts; `../CURRENT_VERIFICATION.md` tracks tests, not benchmarks.

## 3. Canonical file structure

One benchmark translation unit per component: `[benchmark file pattern, e.g. benchmarks/benchmark_Component.ext]`. Every benchmark file carries the same section skeleton, in order, so a reader of one benchmark can read them all:

1. File header with standalone build instructions ([toolchain command lines])
2. Includes
3. Benchmark configuration (env vars + defaults, §4)
4. Platform configuration (warmup/batch defaults per platform)
5. CPU-frequency / thermal monitoring (§5)
6. Measurement scope (priority/affinity handling, if the platform supports it)
7. Startup header printing (resolved config, detected competitors, invariants)
8. Timer with minimum-batch-duration enforcement (§5)
9. Statistics (§6)
10. Data generation (seeded, reproducible)
11. Correctness guardrails — outside timed regions (§7)
12. Contract note — semantic equivalence statement (§8)
13. Adapter interface, if comparing implementations (§8)
14. Benchmark cases
15. Output formatting + machine-readable export (§9)
16. `main`

A single-implementation benchmark may drop the adapter machinery (13) but keeps everything else — warmup, statistics, monitoring, guardrails, and DCE prevention are about honest measurement, not about competitors.

## 4. Canonical configuration

The EXAMPLE prefix below is illustrative. Substitute the profile's actual macro
prefix when creating benchmark code; the guideline itself is reusable. Record
actual benchmark commands and defaults in the project profile.

All benchmarks in the project share **one project-wide set of environment variable names**. Do not invent per-benchmark config names — shared names are what make a suite scriptable and results comparable.

| Variable | Meaning |
| --- | --- |
| `EXAMPLE_BENCH_WARMUP_RUNS` | Warmup batches (executed, never reported) |
| `EXAMPLE_BENCH_BATCHES` | Measured batches |
| `EXAMPLE_BENCH_SEED` | RNG seed for data generation |
| `EXAMPLE_BENCH_TARGET_WORK` | Target work per batch (benchmark-defined unit) |
| `EXAMPLE_BENCH_MIN_BATCH_MS` | Minimum wall time per measured batch |
| `EXAMPLE_BENCH_OUTPUT_CSV` / `_OUTPUT_JSON` | Machine-readable export paths |
| `[opt-outs: NO_SCOPE / NO_STABILIZE / NO_COOLDOWN]` | Disable priority/affinity, stabilization wait, cool-down sleeps |

Rules:

- Defaults may differ by platform (e.g., fewer batches where scheduling variance dominates), but env vars always override.
- Every benchmark **prints its fully resolved configuration once at startup** — seed, warmup, batches, target work, minimum batch time, and the on/off state of each opt-out. A result that can't be reproduced because the config wasn't recorded is not a result.

## 5. Measurement honesty machinery

Each mechanism below exists because a specific, named failure mode produces plausible-looking wrong numbers. None is optional decoration.

**CPU-frequency / thermal monitoring.** Record the CPU frequency context at the start of every benchmark section. Distinguish a true *base* frequency reference from a *max/turbo* fallback: running below turbo is normal, so a throttling claim is only honest when the reference is a true base — with a max-only reference, print the frequency and make no throttle claim. Optionally wait (bounded, opt-out via env var, status printed) for frequency to stabilize before a section, and insert short cool-down sleeps between heavy sections.

**Round-robin interleaving.** Never run all batches of implementation A, then all of B. Sequential blocks hand the later implementation a hotter, more throttled, differently-cached machine — the comparison measures machine drift, not code. Instead: each measured run executes exactly one timed iteration per implementation, in an order **re-randomized per run**, so every implementation observes the same distribution of machine states.

```
for each measured run:
    order = shuffle(adapters)          // re-randomized every run
    for each adapter in order:
        adapter.setup()                // outside the timer
        t0 = now()
        ops = adapter.run_operation()  // the only timed line
        elapsed = now() - t0
        adapter.teardown()             // outside the timer
        samples[adapter].push(elapsed / ops)
```

Invariants of this loop:

1. One timed iteration per implementation per run — never a block of runs for one implementation.
2. Execution order randomized per run.
3. Setup and teardown outside timed regions.
4. All implementations observe the same distribution of machine states.

Even a single-implementation benchmark that prints a table across cases/sizes must randomize case order per batch (or stabilize between cases) — thermal drift biases a sequential table exactly the way it biases a sequential comparison.

**Dead-code-elimination prevention.** An optimizer that can prove a result unused deletes the work, and the benchmark then measures an empty loop at astonishing speed. Sink every result: use the project's `DoNotOptimize`/`ClobberMemory` equivalents ([utility location]) or a separately verified sink. The following is pseudocode, not a universal optimizer barrier:

```
sink(result)  // verify generated code or an independently strengthened run
```

Do not use a locally declared `volatile auto sink = value;` as the barrier — it is not reliable across compilers and can introduce one-time initialization artifacts. Never rely on "the compiler probably won't notice" — treat any suspiciously fast result as a DCE suspect and verify against the generated code or a sink-strengthened rerun.

For small hot data (scalars, small tuples), generate an array of instances and cycle through them with an input-varying index and a verified sink; a single constant operand invites constant folding of the very operation under measurement.

**Warmup.** Run a small number of unreported warmup batches before measurement so caches, branch predictors, page mappings, and frequency governors reach steady state. First-run numbers measure cold-start artifacts.

**Monotonic timer.** Measure elapsed intervals with a monotonic clock. C++ benchmarks
use `std::chrono::steady_clock`. Wall-clock corrections can shorten, lengthen, or
reverse an interval; a clock's advertised resolution does not establish monotonicity.
Check the timer actually used by the runner, including helpers, before accepting
its measurements. A system-clock timestamp may label an exported result, but must
not supply the elapsed interval.

**Minimum batch duration.** Each measured batch must run long enough to sit well above timer resolution — auto-calibrate iteration counts to hit `EXAMPLE_BENCH_MIN_BATCH_MS`, or size the fixed workload accordingly. If the minimum can't be met, print a warning; never silently report quantization noise as data.

**Setup/teardown outside timed regions.** Construction, population, and destruction happen around the timer, not inside it, unless the case explicitly measures them.

**Reproducible data generation.** All input data comes from RNGs seeded by `EXAMPLE_BENCH_SEED`. Unseeded data makes run-to-run comparison meaningless and regression bisection impossible.

## 6. Statistics

Compute per sample set, at minimum:

```
median      // primary
mean, stddev, min, max
ci95_low, ci95_high   // optional; normal approximation
```

- **Median is the primary reported statistic.** The mean is polluted by scheduler preemptions, interrupts, and thermal events — one-sided outliers that are properties of the machine, not the code. The median is less sensitive to isolated extremes, not immune to bias; report it first with spread.
- **Report variance alongside.** At minimum stddev and min/max; a confidence interval if computed (note its normality assumption at low batch counts and don't over-interpret small differences). A median with no spread attached hides whether the number is stable or a coin flip.
- Flag anomalies without failing: when stddev exceeds the median, print a high-variance note so the reader knows to distrust the row.
- Concurrency benchmarks additionally report throughput and tail latency (p95/p99) across a documented range of thread counts, with a start barrier so all threads enter measurement together.

## 7. Correctness guardrails

A benchmark must not measure broken behavior at record speed. Every benchmark case includes **at least one correctness validation outside the timed region**: after the timed work, verify the result against a reference — sizes match, a sample of lookups succeeds, a round-trip reproduces the input, concurrent work totals add up. Only then is the timing trusted.

Never place assertions inside the hot timed loop (unless the case explicitly measures validation cost) — that measures the assertion, and the two failure directions are equally bad: a check inside the loop distorts the number; no check anywhere blesses garbage.

## 8. Competitor policy

Comparisons against other implementations (standard-library baselines, established third-party libraries in the same design category) establish context. They are evidence, not sport: don't add competitors to chase a headline number, and don't remove one because the project loses a case.

**Adapter pattern.** Each compared implementation sits behind a small uniform adapter interface:

```
interface IAdapter:
    name() -> string          // printed label, carries any semantic tag
    setup(work_size)          // outside timed region
    run_operation() -> ops    // the timed unit; returns op count
    teardown()                // outside timed region
```

Adapters are dumb mechanical mappings to the foreign API — no timing, no statistics, no policy. All measurement logic lives once, in the runner, so no implementation gets a private code path.

**Auto-detection with explicit reporting.** Competitor headers are optional, detected at compile time ([detection mechanism, e.g. __has_include + feature macros]); libraries needing linking or heavy dependency trees are explicit opt-in. The benchmark must compile and run with **zero third-party dependencies installed**. At startup it prints a competitors checklist — every candidate, marked detected or not found, with the primary implementation and baselines labeled — so a report reader knows exactly what the comparison did and did not include. A silently absent competitor is a silently narrowed claim.

**Semantic-equivalence contract note (mandatory).** Every benchmark section prints a short contract note stating the semantics being measured — e.g. "pointer stability required", "exact vs epsilon equality", "allocation excluded (reserve performed)". A comparison is only honest if both sides do the same job; where a competitor's semantics differ (weaker guarantees, fixed capacity, no thread safety), its output name carries an explicit label saying so. If a competitor shifts cost between phases, don't let a one-phase case be the only headline — add a full-cycle case or state the isolation.

**Selection discipline.** Always include a standard-library baseline when an equivalent exists; at most one competitor per design family unless there's a stated reason; prefer maintained, widely deployed, libraries installable through the project's recorded dependency channel. Adding or removing a competitor updates the file-header list, the printed checklist, and carries a one-line rationale in source.

## 9. Output discipline

- **Standardized startup header**, identical shape across the suite so logs are parseable: title banner naming the component with its `[namespace prefix]`, a one-line platform/compiler/config summary, the competitors checklist, the design invariants in force for this benchmark, and CPU context. Example shape:

  ```
  ================================================================================
    [namespace]::Component Benchmark Suite
  ================================================================================
  Platform: [os-arch compiler] | warmup=N measured=N seed=N
  Competitors:
    [x] [primary] (primary)
    [x] [standard baseline] (baseline)
    [ ] [optional competitor] (not detected)
  CPU: N MHz (base: N MHz)
  ```

- **Plain-ASCII summary** to stdout: consistent section banners, aligned result tables, no Unicode symbols (`[PASS]`/`[FAIL]`/`[x]`/`[ ]` instead) — output must survive every terminal, log viewer, and CI capture unmangled.
- **Machine-readable export** (CSV and/or JSON via the canonical env vars) for regression tracking: timestamp, benchmark/case/implementation names, unit, the full statistics set, platform + compiler string, CPU context, and resolved config per record. Field names don't change without bumping a schema version — downstream tooling parses these.
- **One current results report per benchmark family.** Mark its predecessor
  superseded and route readers to the current report. Preserve dated prior reports
  and raw exports as non-authoritative evidence under the retention policy; do not
  destroy an audit trail merely to maintain one current authority.
- If a benchmark builds with explicit ISA/feature flags, it must not execute unsupported instructions on lesser machines: feature-gate at runtime, build baseline + variant targets, or make the flags opt-in.

## 10. Checklist

Before a benchmark (or a performance claim derived from one) lands:

- [ ] Canonical `EXAMPLE_BENCH_*` env vars supported; resolved config printed at startup
- [ ] Warmup + measured batches; minimum batch duration enforced or warned
- [ ] Elapsed timer is monotonic; C++ runner uses `std::chrono::steady_clock`
- [ ] CPU-frequency context recorded per section; throttle claims only against a true base reference
- [ ] Round-robin interleaving with per-run randomized order (or randomized case order for single-implementation tables)
- [ ] DCE prevented on every result path
- [ ] Median primary; variance reported; anomalies flagged
- [ ] Correctness validated outside the timed region; nothing asserted inside the hot loop
- [ ] Competitors behind adapters; detection checklist printed; semantic differences labeled; contract note per section
- [ ] Separate build target; excluded from the ordinary CI gate (wiring per `WORKFLOW.md`)
- [ ] Machine-readable export works; ASCII-only summary
- [ ] Results written to the family's single current report; superseded report clearly retired
- [ ] No number from this run copied into a header, commit message, or teaching document (placement per `TEACHING.md`)

## Configuration boundaries and unavailable observations

Bracketed paths and command descriptions above are specimen fields. Resolve them
against the project profile when creating a benchmark; they are not installed tools.
If frequency/thermal/affinity instrumentation is unavailable, print unavailable
and its limitation. Never invent telemetry or silently omit the observation.
Buildability of benchmark targets is checked separately from noisy timing gates.
