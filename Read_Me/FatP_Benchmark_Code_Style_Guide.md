# Fat-P Benchmark Code Style Guide

**Status:** Active  
**Applies to:** All benchmark translation units (`benchmarks/benchmark_*.cpp`)  
**Authority:** Subordinate to the *Fat-P Library Development Guidelines*  
**Version:** 1.4 (February 2026)

## Purpose

This guide defines how to write benchmark `.cpp` files for Fat-P components. It ensures consistent methodology, statistical rigor, and fair comparisons across all Fat-P benchmarks.

Benchmarks are part of the public credibility story:

* Correctness is established by unit tests.
* Performance claims are established by benchmarks.
* Benchmarks must be reproducible, honest about semantics, and robust to machine state drift.

Reference implementations: `benchmark_StableHashMap.cpp`, `benchmark_SmallVector.cpp`, `include/fat_p/FatPBenchmarkRunner.h`, `include/fat_p/FatPBenchmarkHeader.h`

---

## File Structure

### Naming

```
benchmark_ComponentName.cpp
```

### Required Sections

```cpp
// 1. File header with build instructions
// 2. Includes
// 3. Benchmark configuration (env vars + defaults)
// 4. Platform configuration (warmup/batches + platform differences)
// 5. CPU frequency monitoring (+ optional stabilization)
// 6. Benchmark scope (priority/affinity)
// 7. Startup header printing (standardized format - see "Startup Header Format")
// 8. Timer (+ minimum duration / calibration)
// 9. Statistics
// 10. Data generation
// 11. Correctness guardrails (checks OUTSIDE timed regions)
// 12. Contract note (semantic equivalence)
// 13. Adapter interface (if comparing libraries)
// 14. Benchmark cases
// 15. Output formatting (+ machine-readable export)
// 16. Main
```

---

## Benchmark Configuration (Canonical)

All Fat-P benchmarks must support the same environment variables. Do not invent per-benchmark config names.

### Canonical Environment Variables

| Variable                   | Meaning                                                          | Default                     |
| -------------------------- | ---------------------------------------------------------------- | --------------------------- |
| `FATP_BENCH_WARMUP_RUNS`   | Number of warmup batches (not reported)                          | `3`                         |
| `FATP_BENCH_BATCHES`       | Number of measured batches                                       | Windows: `15`, others: `50` |
| `FATP_BENCH_SEED`          | RNG seed for data generation                                     | `12345`                     |
| `FATP_BENCH_TARGET_WORK`   | Target work per batch (ops or bytes); benchmark-specific meaning | `5_000_000`                 |
| `FATP_BENCH_MIN_BATCH_MS`  | Minimum wall time per measured batch (auto-calibration target)   | `50`                        |
| `FATP_BENCH_VERBOSE_STATS` | Print extra statistics / raw samples                             | `0`                         |
| `FATP_BENCH_OUTPUT_CSV`    | Optional CSV output path                                         | empty (disabled)            |
| `FATP_BENCH_OUTPUT_JSON`   | Optional JSON output path                                        | empty (disabled)            |
| `FATP_BENCH_NO_SCOPE`      | Disable priority/affinity changes                                | unset                       |
| `FATP_BENCH_NO_STABILIZE`  | Disable CPU stabilization wait                                   | unset                       |
| `FATP_BENCH_NO_COOLDOWN`   | Disable cool-down sleeps                                         | unset                       |

**Rule:** Every benchmark must print the resolved configuration once at startup (seed, warmup, batches, target_work, min_batch_ms, scope/stabilize/cooldown status).

---

## Platform Configuration

### Warmup and Measured Runs (Defaults)

These are defaults only. Environment variables override them.

```cpp
#if defined(_WIN32) || defined(_WIN64)
static constexpr size_t DEFAULT_WARMUP_RUNS = 3;
static constexpr size_t DEFAULT_MEASURED_RUNS = 15;  // Windows: higher run-to-run variance
#else
static constexpr size_t DEFAULT_WARMUP_RUNS = 3;
static constexpr size_t DEFAULT_MEASURED_RUNS = 50;
#endif
```

**Rationale:** Windows scheduling is less deterministic; more runs don't help as much. Linux benefits from more samples.

---

## CPU Frequency Monitoring

### Implementation Options

Either:

1. **Implement locally** following this guide's accuracy requirements, or
2. **Use shared helper** (`include/fat_p/FatPBenchmarkRunner.h` or `include/fat_p/FatPTest.h`) if it meets the same `ref_is_max` rules.

If using a shared helper, verify it:

* Distinguishes `base_frequency` from `cpuinfo_max_freq`
* Only prints `[THROTTLED]` when the reference is a true base
* Prints timestamp and frequency info

### Required Structure

```cpp
struct CpuFreqInfo
{
    double ref_freq_mhz = 0;      // Base frequency (or max as fallback)
    double current_freq_mhz = 0;
    bool ref_is_max = false;      // True if using max_freq fallback

    double throttle_percentage() const
    {
        if (current_freq_mhz <= 0 || ref_freq_mhz <= 0) return 0;
        return (1.0 - current_freq_mhz / ref_freq_mhz) * 100.0;
    }

    // CRITICAL: Only claim throttling when ref is true base frequency
    bool is_throttled() const { return !ref_is_max && throttle_percentage() > 5.0; }
    bool is_turbo() const { return !ref_is_max && current_freq_mhz > ref_freq_mhz * 1.05; }
};
```

### Reference Source Rule (P0 - Critical)

**Only print `[THROTTLED]` when the reference is a true base frequency.**

| Source                  | `ref_is_max` | Throttle/Turbo Detection |
| ----------------------- | ------------ | ------------------------ |
| `base_frequency`        | `false`      | ✓ Reliable               |
| `cpuinfo_max_freq`      | `true`       | ✗ Disabled               |
| Windows registry `~MHz` | `false`      | ✓ Reliable               |

When `ref_is_max == true`, either:

* Print no throttle status at all, or
* Print `(max: 4200)` without any `[THROTTLED]` claim

**Rationale:** `cpuinfo_max_freq` is turbo frequency. Running below turbo is normal, not throttling. Claiming "THROTTLED 30%" when the CPU is simply at base clock is misleading.

### Platform-Specific Implementation

**Linux:** Read from `/sys/devices/system/cpu/cpu0/cpufreq/`:

* `scaling_cur_freq` — current frequency
* `base_frequency` — reliable base (preferred)
* `cpuinfo_max_freq` — fallback only; set `ref_is_max = true`

**Windows:** Read from registry:

* `HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor\0\~MHz`

### Output Format

```cpp
void print_cpu_context(const char* label = nullptr)
{
    auto info = get_cpu_freq();
    // ...
    if (info.ref_freq_mhz > 0)
    {
        const char* ref_label = info.ref_is_max ? "max" : "base";
        std::cout << " (" << ref_label << ": " << static_cast<int>(info.ref_freq_mhz) << ")";

        // Only print throttle/turbo status when ref is reliable
        if (!info.ref_is_max)
        {
            if (info.is_throttled())
                std::cout << " [THROTTLED " << info.throttle_percentage() << "%]";
            else if (info.is_turbo())
                std::cout << " [TURBO]";
        }
    }
}
```

**Example outputs:**

* `CPU: 3133 MHz (base: 3700) [THROTTLED 15%]` — reliable, true base
* `CPU: 4200 MHz (base: 3700) [TURBO]` — reliable, true base
* `CPU: 2600 MHz (max: 4200)` — no claim, ref is turbo fallback

### Per-Function CPU Context (Required)

**Every benchmark function must call `print_cpu_context()` at the start** to record the CPU frequency state when that specific benchmark runs:

```cpp
void benchVectorDouble(const BenchConfig& cfg)
{
    print_header("vector<double> vs Manual Epsilon Loop");
    print_cpu_context();  // Required

    // ... benchmark code ...
}
```

### Optional CPU Stabilization (Recommended)

Many FAT-P benchmarks already report throttling and warn about timer precision. Add one more guardrail: **wait for stable CPU frequency before running a section** (unless disabled).

**Rule:** If stabilization is implemented, it must be:

* opt-out via `FATP_BENCH_NO_STABILIZE`
* visible in output (print "Stabilization: ON/OFF")
* bounded (do not wait forever)

Recommended policy:

* Warm up with a short burst of work
* Sample frequency N times (e.g., 5 samples)
* Consider stable if variance ≤ 10% for 3 consecutive windows
* If stability not reached within a timeout (e.g., 3 seconds), continue but print `[NOTE] CPU not stabilized`

### Optional Cool-Down (Recommended)

If the suite runs many heavy benchmarks (hash maps, big vectors), insert small sleeps between major sections to reduce thermal drift.

* opt-out via `FATP_BENCH_NO_COOLDOWN`
* keep sleeps short (e.g., 50–250 ms)
* print when used

---

## BenchmarkScope (Windows)

Pin thread to non-zero CPU core and elevate priority during measurement.

**Policy:** BenchmarkScope is enabled by default on Windows. Opt-out via `FATP_BENCH_NO_SCOPE=1`.

```cpp
#if defined(_WIN32) || defined(_WIN64)

static inline bool has_env_var(const char* name)
{
    char buf[2];
    return GetEnvironmentVariableA(name, buf, sizeof(buf)) > 0;
}

class BenchmarkScope
{
    DWORD old_priority_ = 0;
    DWORD_PTR old_affinity_ = 0;
    bool applied_ = false;

public:
    explicit BenchmarkScope(bool verbose = false)
    {
        if (has_env_var("FATP_BENCH_NO_SCOPE")) return;

        HANDLE proc = GetCurrentProcess();
        old_priority_ = GetPriorityClass(proc);
        SetPriorityClass(proc, HIGH_PRIORITY_CLASS);

        HANDLE thread = GetCurrentThread();
        DWORD_PTR proc_mask = 0, sys_mask = 0;
        DWORD_PTR target = 1;
        if (GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask) && proc_mask)
        {
            DWORD_PTR nonzero = proc_mask & ~static_cast<DWORD_PTR>(1);
            DWORD_PTR pick = nonzero ? nonzero : proc_mask;
            target = pick & (~pick + 1);  // lowest set bit
        }
        old_affinity_ = SetThreadAffinityMask(thread, target);
        applied_ = true;

        if (verbose)
        {
            std::cout << "[BenchmarkScope] High priority, CPU"
                      << (target > 1 ? " non-0" : " 0") << " affinity\n";
        }
    }

    ~BenchmarkScope()
    {
        if (!applied_) return;

        HANDLE proc = GetCurrentProcess();
        SetPriorityClass(proc, old_priority_);

        HANDLE thread = GetCurrentThread();
        if (old_affinity_ != 0)
            SetThreadAffinityMask(thread, old_affinity_);
    }

    BenchmarkScope(const BenchmarkScope&) = delete;
    BenchmarkScope& operator=(const BenchmarkScope&) = delete;
};

#else
class BenchmarkScope
{
public:
    explicit BenchmarkScope(bool = false) {}
};
#endif
```

**Usage:**

```cpp
int main()
{
    BenchmarkScope scope(/*verbose=*/true);
    // ... run benchmarks ...
}
```

---

## Timer

Benchmarks must not be dominated by timer quantization.

### Baseline Timer

Use `std::chrono::steady_clock`:

```cpp
struct Timer
{
    using clock = std::chrono::steady_clock;
    clock::time_point t0;

    void start() { t0 = clock::now(); }

    double elapsed_ns() const
    {
        auto t1 = clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }
};
```

### Minimum Batch Duration (P0 - Critical)

**Each measured batch must run long enough to be meaningfully above timer resolution.**

Rule:

* Either auto-calibrate iterations/ops to hit a minimum duration, or
* ensure the fixed workload is large enough that each batch is ≥ `FATP_BENCH_MIN_BATCH_MS` (default 50 ms).

If you cannot hit the minimum duration, print a warning (do not silently report misleading numbers).

---

## Statistics

### Required Structure

```cpp
struct Statistics
{
    double median = 0;
    double mean = 0;
    double stddev = 0;
    double ci95_low = 0;
    double ci95_high = 0;
    double min = 0;
    double max = 0;

    static Statistics compute(std::vector<double> samples)
    {
        Statistics s{};
        if (samples.empty()) return s;

        std::sort(samples.begin(), samples.end());
        size_t n = samples.size();

        s.min = samples.front();
        s.max = samples.back();

        if (n % 2 == 1) s.median = samples[n / 2];
        else s.median = 0.5 * (samples[n / 2 - 1] + samples[n / 2]);

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        s.mean = sum / static_cast<double>(n);

        if (n > 1)
        {
            double acc = 0.0;
            for (double x : samples)
            {
                double d = x - s.mean;
                acc += d * d;
            }
            s.stddev = std::sqrt(acc / static_cast<double>(n - 1));

            double se = s.stddev / std::sqrt(static_cast<double>(n));
            constexpr double z = 1.96;  // 95% CI (normal approx)
            s.ci95_low = s.mean - z * se;
            s.ci95_high = s.mean + z * se;
        }

        return s;
    }
};
```

### Primary Metric

**Median** is the primary reported statistic. Mean and CI are supplementary.

**CI note:** CI95 is `mean ± 1.96 * SE`. With low batch counts (Windows default 15), this is approximate and assumes normality. Do not over-interpret small CI differences.

### Concurrency Benchmarks: Percentiles

For concurrency benchmarks (thread pool, lock-free queue), also report at least:

* p95 latency
* p99 latency

Do not add percentiles to single-thread microbenches by default.

---

## Preventing Dead Code Elimination

### Required Rule

Benchmarks must prevent the optimizer from deleting or simplifying the work.

Prefer using FAT-P utilities if available (recommended):

* `fat_p::testing::DoNotOptimize(...)`
* `fat_p::testing::ClobberMemory()`

If not available, use the portable volatile sink pattern.

### Volatile Sink (Portable Default)

```cpp
static volatile std::uint64_t benchmark_sink_u64 = 0;

// Usage:
benchmark_sink_u64 += static_cast<std::uint64_t>(value);
```

**Rule:** Do not use `static volatile auto sink = value;` as a DCE barrier. It is not a reliable cross-compiler technique and can introduce one-time initialization artifacts.

### DoNotOptimize (GCC/Clang)

```cpp
template <typename T>
inline void DoNotOptimize(T const& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "r,m"(value) : "memory");
#else
    // MSVC: prefer volatile sink
    (void)value;
#endif
}
```

---

## Round-Robin Execution

### The Problem

Sequential execution (all runs of Library A, then all runs of Library B) causes:

* Thermal drift (later libraries run hotter)
* Frequency scaling drift (turbo decays)
* Cache state differences

### The Solution

Interleave libraries within each run and randomize the order:

```cpp
for (size_t run = 0; run < MEASURED_RUNS; ++run)
{
    std::vector<IAdapter*> order = adapters;
    std::shuffle(order.begin(), order.end(), rng);

    for (IAdapter* a : order)
    {
        a->setup();

        Timer t;
        t.start();
        size_t ops = a->run_operation();
        double elapsed = t.elapsed_ns();

        a->teardown();

        samples[a].push_back(elapsed / ops);
    }
}
```

### Design Invariants

1. Each measured run executes exactly one timed iteration per library
2. Library execution order is randomized per run
3. Setup and teardown occur OUTSIDE timed regions
4. All libraries observe the same distribution of machine states
5. Median is the primary reported statistic

### Single-Library, Multi-Case Benchmarks (Update)

Even when only one implementation is measured, sequential execution of many cases/sizes can bias the printed table due to thermal drift.

**Rule:** If a benchmark prints a table comparing multiple cases/sizes, the benchmark must either:

* randomize case order per batch, or
* apply stabilization/cool-down between cases.

---

## Adapter Pattern (Multi-Library Comparison)

When comparing against competitors (std::, boost::, tsl::, etc.):

```cpp
struct IAdapter
{
    virtual ~IAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t N) = 0;
    virtual void teardown() = 0;
    virtual size_t run_operation() = 0;  // returns ops count
};
```

**Adapters contain:**

* No timing logic
* No statistics
* No policy decisions

They are dumb, mechanical mappings to library APIs.

---

## Competitor Auto-Detection

Use `__has_include` for optional dependencies:

```cpp
#if __has_include("tsl/robin_map.h")
#include "tsl/robin_map.h"
#define HAS_TSL 1
#else
#define HAS_TSL 0
#endif

#if __has_include(<boost/container/small_vector.hpp>)
#include <boost/container/small_vector.hpp>
#define HAS_BOOST 1
#else
#define HAS_BOOST 0
#endif
```

For libraries requiring explicit linking (e.g., Abseil):

```cpp
#if defined(USE_ABSL) && USE_ABSL && __has_include("absl/container/flat_hash_map.h")
#include "absl/container/flat_hash_map.h"
#define HAS_ABSL 1
#else
#define HAS_ABSL 0
#endif
```


---

## Competitor Policy

Benchmarks are evidence, not a sport. Competitors exist to establish *context* and to expose trade-offs.

### Goals

* Demonstrate where Fat-P sits in the real ecosystem for this component category.
* Compare against distinct design families, not near-duplicates.
* Keep the competitor set stable so results remain comparable over time.

### Non-Goals

* Do not add competitors to chase a single headline number.
* Do not remove competitors because Fat-P loses a case.
* Do not benchmark semantics that are not clearly stated in the Contract Note.

### Selection Rules (P0 - Critical)

1. **Always include a standard baseline** when an equivalent exists.

   Examples: `std::vector`, `std::unordered_map`, `std::pmr::unsynchronized_pool_resource`, `new/delete`.

2. **Include Boost when Boost provides an equivalent component.**

   Boost is a widely deployed reference point and is already part of many environments.

3. **Include at most one competitor per design family**, unless there is a stated reason.

   *Example families:* open-addressing hash maps, chained hash maps, B-trees, flat/sorted vectors, fixed-capacity containers, node-pool allocators.

4. **Prefer competitors that are**:

   * widely used in production,
   * maintained,
   * installable via a standard channel (e.g., vcpkg),
   * and have stable APIs.

5. **Do not exceed a practical competitor count.**

   Rule of thumb: Fat-P + standard baseline + Boost + 1–3 additional competitors.

### Semantics and Labeling (P0 - Critical)

* A competitor is "equivalent" only if it matches the Contract Note semantics.

  If semantics differ, the output name must include a label.

  Examples:

  * `EASTL::fixed_pool (fixed capacity)`
  * `unordered_map (no handle safety)`
  * `std::pmr::unsynchronized_pool_resource (thread-unsafe)`

* If a benchmark times only a subset of a compound workflow (e.g., measuring the refill phase but not the free phase), add a second case that measures the full cycle or state clearly that the case isolates one phase.

  **Rule:** If a competitor shifts cost between operations (e.g., ordered-free vs LIFO free list), do not use a one-phase benchmark as the only headline number.

### Optional Dependencies and Build Policy (P0 - Critical)

* The benchmark must compile and run with **zero third-party dependencies installed**.

  Competitors are optional, gated by `__has_include` and feature macros.

* Libraries requiring non-header-only linking or large dependency trees must be **explicit opt-in**.

  Pattern:

  * `USE_X` → user choice (default off)
  * `HAS_X` → compile-time detection

* Every benchmark must print a "Competitor libraries detected" list at startup, including install hints for missing competitors when known.

### Concurrency Competitors (P1 - Recommended)

If the benchmark includes multi-threaded cases, include at least one thread-safe baseline.

Examples:

* `std::pmr::synchronized_pool_resource` for pool benchmarks
* a locked wrapper around a single-threaded competitor (explicitly labeled)

### Change Control

Adding or removing a competitor requires:

* Updating the benchmark file header comment "Competitors" list.
* Updating any printed "Competitor libraries" list.
* A one-line rationale in the benchmark source near the competitor include block.

---

## Data Generation

### Reproducibility

Always use seeded RNGs. The seed must be configurable via `FATP_BENCH_SEED`.

```cpp
std::vector<std::int64_t> generate_keys(std::size_t n, std::uint64_t seed)
{
    std::vector<std::int64_t> keys(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::int64_t> dist(0, INT64_MAX);
    for (std::size_t i = 0; i < n; ++i)
        keys[i] = dist(rng);
    return keys;
}
```

### Preventing Optimizer Effects

For small data (tuples, scalars), generate multiple instances and cycle through them:

```cpp
std::array<std::tuple<double,double,double>, 64> tuples;
for (int i = 0; i < 64; ++i)
    tuples[i] = generateTuple(seed + static_cast<std::uint64_t>(i));

volatile std::size_t idx = 0;
for (std::size_t i = 0; i < iters; ++i)
{
    bool eq = compare(tuples[idx], tuples[(idx + 32) % 64]);
    DoNotOptimize(eq);
    idx = (idx + 1) % 64;
}
```

---

## Correctness Guardrails (Required)

Benchmarks must not silently measure broken behavior.

**Rule:** Each benchmark case must include at least one correctness validation outside the timed region.

Examples:

* After inserting N keys, verify size is N and a sample of lookups succeeds.
* After a round-trip encode/decode, verify decoded data matches the input.
* After a concurrent run, verify total work counts match expectations.

Do not place assertions inside the hot measured loop (unless the benchmark is explicitly measuring validation cost).

---

## Contract Note (Required)

Benchmarks must be explicit about semantics.

**Rule:** Each benchmark section must print a short "Contract Note" describing the semantics being measured.

Examples:

* "pointer stability required"
* "ABA-safe handles required"
* "exact equality vs epsilon equality"
* "deterministic encoding"
* "allocation included/excluded (reserve performed)"

If a competitor baseline does not provide equivalent semantics, label it explicitly in output (e.g., "unordered_map (no handle safety)").

---

## Output Formatting

### Section Headers

```cpp
void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}
```

### Result Tables

```cpp
std::cout << std::fixed << std::setprecision(2);
std::cout << std::setw(30) << "Library"
          << std::setw(12) << "Median"
          << std::setw(12) << "Mean"
          << std::setw(10) << "Stddev"
          << "  CI95\n";
std::cout << std::string(79, '-') << "\n";
```

### Sanity Checks

Report anomalies without failing the benchmark:

```cpp
if (stats.stddev > stats.median && stats.median > 0)
{
    std::cout << "  [NOTE] high variance (stddev " << stats.stddev
              << " > median " << stats.median << ")\n";
}
```

---

## Startup Header Format (P0 - Required)

All benchmarks must print a standardized startup header for consistency across the suite. This aids log parsing, reproducibility, and professional presentation.

### Header Section Order

```
1. [BenchmarkScope] line (if enabled)
2. Title banner (80 '=' chars)
3. Platform line
4. Competitors block
5. Configuration block (if extended config)
6. CPU diagnostics (condensed)
7. Design Invariants
8. Optional sections (cooling, correctness, expected results)
9. Stabilization status
```

### Title Banner

**Always 80 characters wide, always use `fat_p::` prefix:**

```cpp
void print_startup_banner(const std::string& component)
{
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  fat_p::" << component << " Benchmark Suite\n";
    std::cout << std::string(80, '=') << "\n\n";
}
```

**Example:**
```
================================================================================
  fat_p::StableHashMap Benchmark Suite
================================================================================
```

**Do not use:**
- Different widths (70, 72, etc.)
- "Comprehensive Benchmark Suite"
- "FAT-P" (use lowercase `fat_p::`)
- Bare component name without namespace

### Platform Line

**Single line with canonical field names:**

```
Platform: Windows-x64 MSVC-1942 | warmup=3 measured=15 seed=12345
```

**Rules:**
- OS-arch: `Windows-x64`, `Linux-x64`, `Linux-arm64`, `macOS-arm64`
- Compiler-version: `MSVC-1942`, `GCC-13.2`, `Clang-17.0`
- Use `measured=` (not `batches=`)
- No spaces around `=`
- Pipe separator `|` between platform and config

### Competitors Block

**Use `[x]`/`[ ]` checklist format:**

```
Competitors:
  [x] fat_p::StableHashMap (primary)
  [x] tsl::robin_map
  [x] ankerl::unordered_dense
  [x] std::unordered_map (baseline)
  [ ] boost::unordered_flat_map (not detected)
```

**Rules:**
- Header is `Competitors:` (not "Competitor libraries:", "Libraries detected:", etc.)
- `[x]` for detected, `[ ]` for not found
- Primary implementation first with `(primary)` tag
- Baselines labeled with `(baseline)`
- Brief notes in parentheses only

**Do not use:**
- Inline format: `Competitor libraries: tsl ankerl absl`
- Space-separated lists
- Different header names per benchmark

### Extended Configuration Block

**Only include if benchmark has config beyond Platform line basics:**

```
Configuration:
  Target work:    5000000 ops/batch
  Min batch ms:   50
  Scope:          ON
  Stabilize:      ON
  Cooldown:       ON
```

**Rules:**
- Header is `Configuration:` (capitalized)
- Field names left-aligned
- Use `ON`/`OFF` for booleans (not "enabled"/"disabled"/"true"/"false")
- Two-space indent

### CPU Diagnostics (Condensed)

**Single-line summary by default:**

```
CPU: 2469 MHz (base: 3686 MHz) [THROTTLED 33%]
```

**Rules:**
- Show `[THROTTLED N%]` only when reference is true base frequency
- Show `[TURBO]` only when above base
- Use `(max: N MHz)` without throttle claim when using max fallback
- Full CPUID diagnostics only when `FATP_BENCH_VERBOSE_STATS=1`

### Design Invariants

**Use consistent header and wording:**

```
Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark
```

**Rules:**
- Header is `Design Invariants:` (capital I)
- Numbered list
- Two-space indent
- Include only applicable invariants (e.g., skip #1 and #3 for single-library benchmarks)

### Optional Sections

**Cooling delays (if enabled):**
```
Cooling: section=500ms size=100ms case=50ms
```

**Correctness verification (if pre-flight checks):**
```
Correctness:
  [PASS] FIFO ordering
  [PASS] Capacity enforcement
  [PASS] Thread safety (SPSC)
```

**Expected results (for complex comparisons):**
```
Expected Results:
  - fat_p::SlotMap excels at: iteration, O(1) ops, ABA safety
  - std::unordered_map: fast lookup, scattered iteration
```

### Stabilization Status

**Timestamp + status at end of header:**

```
[2026-02-01 09:02:34] CPU stable at 2410 MHz (65% of base, variance 6.1%)
```

Or if not stable:
```
[2026-02-01 09:02:34] WARNING: CPU not stable after 6s (2395 MHz, 65% of base)
```

### ASCII-Only Output (P0 - Required)

All benchmark output must be ASCII-only. No Unicode characters.

| Instead of | Use |
|------------|-----|
| ✓ | `[PASS]` or `[x]` |
| ✗ | `[FAIL]` or `[ ]` |
| ❌ | `[X]` or `[FAIL]` |
| ⚠ | `[WARNING]` or `[!]` |
| § | `Section` |

**Rationale:** Ensures consistent display across terminals, log viewers, CI systems, and Windows consoles.

### Complete Example

```
[BenchmarkScope] High priority, CPU non-0 affinity
================================================================================
  fat_p::StableHashMap Benchmark Suite
================================================================================

Platform: Windows-x64 MSVC-1942 | warmup=3 measured=15 seed=12345

Competitors:
  [x] fat_p::StableHashMap (primary)
  [x] tsl::robin_map
  [x] ankerl::unordered_dense
  [x] absl::flat_hash_map
  [x] std::unordered_map (baseline)

Configuration:
  Target work:    5000000 ops/batch
  Min batch ms:   50
  Scope:          ON
  Stabilize:      ON
  Cooldown:       ON

CPU: 2432 MHz (base: 3686 MHz) [THROTTLED 34%]

Design Invariants:
  1. Round-robin execution with randomized order per run
  2. Setup/teardown outside timed regions
  3. All libraries observe same distribution of machine states
  4. Medians are the primary reported statistic
  5. Correctness verified after each benchmark

Cooling: section=2000ms size=1000ms case=300ms

[2026-02-01 09:02:34] CPU stable at 2410 MHz (65% of base, variance 6.1%)

```

---

## Machine-Readable Output (CSV/JSON)

Benchmarks must support machine-readable export for regression tracking.

### Required behavior

If `FATP_BENCH_OUTPUT_CSV` is set, write a CSV file with at least:

* timestamp
* benchmark name
* case name
* library name
* unit (ns/op, MB/s, ops/s, us/op)
* median/mean/stddev/ci95_low/ci95_high
* platform + compiler string
* CPU context string
* config (seed, warmup, batches, target_work, min_batch_ms)

If `FATP_BENCH_OUTPUT_JSON` is set, write the same information as JSON (one record per case+library).

**Rule:** CSV/JSON output must not change field names without bumping the benchmark schema version.

---

## Concurrency Benchmarks (Additional Requirements)

Concurrency benchmarks must report scaling and latency distribution.

### Requirements

* Measure scaling across thread counts: 1, 2, 4, … up to hardware concurrency (or a documented cap).
* Use a start barrier so all threads begin measurement simultaneously.
* Use warmup + measured phase (steady-state).
* Report at minimum:

  * throughput (ops/sec)
  * median latency (if per-op timing exists)
  * p95 and p99 latency
* Be explicit about pinning policy (on/off).
* Do not perform logging or allocation in the measured region unless that cost is part of the case definition.

---

## Build Instructions

Include in file header:

```cpp
/**
 * Compile (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_X.cpp -o bench_x
 *
 * Compile (with competitors):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -I/path/to/tsl \
 *       benchmark_X.cpp -o bench_x
 *
 * Windows (MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_X.cpp
 */
```

### CPU Feature Flags (ISA)

If a benchmark compiles with explicit ISA flags (e.g. `-mavx2`, `-mfma`,
`/arch:AVX2`), it must not execute on machines that do not support those
features.

Acceptable patterns:

* Default to a baseline build, with ISA flags enabled only by opt-in.
* Build baseline + ISA variants and run the supported one.
* Detect CPU features at runtime and skip ISA-only cases.

---

## CI Workflow Integration

Every benchmark must be runnable in CI. When a new benchmark is added for a
component, two workflows must be updated:

1. **Component workflow** (e.g., `.github/workflows/thread-pool.yml`): Add
   benchmark jobs gated behind the `run_benchmarks` workflow_dispatch input.
   See existing workflows for the pattern.

2. **Unified runner** (`.github/workflows/run-all-benchmarks.yml`): Add the
   new component to the appropriate job. Standard benchmarks (no external
   dependencies) go in the `standard` matrix. Benchmarks requiring external
   libraries (TBB, Boost, moodycamel, etc.) get a dedicated job with their
   own dependency installation steps.

Forgetting step 2 means the component will be missing from cross-project
benchmark sweeps. The run-all workflow is the single place that exercises
every benchmark in parallel.

---

## Checklist

* [ ] Canonical env vars supported (`FATP_BENCH_*` set)
* [ ] Resolved configuration printed at startup
* [ ] Warmup + measured batches (env overrides supported)
* [ ] CpuFreqInfo with correct throttle/turbo semantics (`ref_is_max` rule)
* [ ] `print_cpu_context()` called at start of each benchmark function
* [ ] Optional stabilization/cool-down obey env opt-outs
* [ ] BenchmarkScope enabled on Windows with `FATP_BENCH_NO_SCOPE` opt-out
* [ ] Timer uses minimum batch duration or calibration (`FATP_BENCH_MIN_BATCH_MS`)
* [ ] Statistics struct with median/mean/stddev/CI95 (median is primary)
* [ ] Round-robin execution for multi-library comparisons
* [ ] Multi-case/sizes: randomize order or stabilize between cases
* [ ] Setup/teardown outside timed regions
* [ ] Dead-code elimination prevented (FAT-P DoNotOptimize or portable volatile sink)
* [ ] Seeded RNG for reproducible data (`FATP_BENCH_SEED`)
* [ ] `__has_include` for optional competitors
* [ ] Correctness checks outside timed regions
* [ ] Contract note printed for every section
* [ ] Optional CSV/JSON output via env vars
* [ ] Build instructions in file header
* [ ] ISA flags are feature-gated (avoid illegal-instruction execution)
* [ ] **Startup header uses standardized format (Section "Startup Header Format")**
* [ ] **Title banner: 80 chars, `fat_p::Component Benchmark Suite`**
* [ ] **Platform line: canonical field names (`measured=` not `batches=`)**
* [ ] **Competitors block: `[x]`/`[ ]` format with `(primary)`/`(baseline)` tags**
* [ ] **ASCII-only output (no Unicode symbols)**
* [ ] **Component CI workflow updated with benchmark jobs (gated behind `run_benchmarks`)**
* [ ] **`run-all-benchmarks.yml` updated with new component**

---

## Simplified Benchmarks

For benchmarks without competitors (Fat-P vs manual baseline, or one implementation), the full adapter pattern is optional. However, still use:

* warmup + measured batches
* statistics (median primary)
* CPU frequency monitoring
* correctness guardrail checks
* DCE prevention
* case-order randomization (if printing multiple cases/sizes)

Sequential runs are acceptable only when the benchmark has a single measured case and does not present a comparative table across multiple sizes/cases.

---

*Fat-P Benchmark Code Style Guide v1.5 — February 2026*
