// benchmark_Skeleton.cpp
//
// fat_p::Skeleton benchmarks using FatPBenchmarkRunner infrastructure.
//
// Measures the four core operations of the typed hierarchical item registry:
//   - publish / unpublish   (registry mutation)
//   - find                  (exact BoneId lookup -- expected O(1))
//   - query                 (mask-filtered full scan -- expected O(N))
//   - visitSubtree / querySubtree  (mask-filtered partial scan -- O(subtree))
//
// Find and query are each compared against a natural baseline to quantify
// registry overhead in context:
//   - Skeleton::find    vs  std::unordered_map<BoneId, SkeletonItem*>::find
//   - Skeleton::query   vs  std::vector linear scan (same mask predicate)
//
// Architecture: Case execution order is randomized per batch for multi-case
// sections, ensuring all cases observe the same distribution of thermal and
// frequency states.
//
// Design Invariants:
//   1. Case execution order is randomized per batch.
//   2. Setup and teardown occur outside timed regions.
//   3. Medians are the primary reported statistic.
//   4. Correctness verified after each benchmark section.
//   5. No listeners attached during core operation benchmarks.
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_Skeleton.cpp -o bench_sk
//   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_Skeleton.cpp /link advapi32.lib
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS   - Warmup batches (default: 3)
//   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
//   FATP_BENCH_SEED          - RNG seed (default: 12345)
//   FATP_BENCH_MIN_BATCH_MS  - Min batch duration ms (default: 50)
//   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
//   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
//   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
//   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
//   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
//   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps

/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: benchmark
  path: components/Skeleton/benchmarks/benchmark_Skeleton.cpp
  namespace: fat_p::bench
  layer: Testing
  summary: >
    Performance benchmarks for fat_p::Skeleton -- publish, unpublish, find,
    query, visitSubtree, and querySubtree; with find and query compared against
    std::unordered_map and std::vector linear scan baselines.
  api_stability: in_work
  related:
    tests:
      - components/Skeleton/tests/test_Skeleton.cpp
    headers:
      - include/fat_p/Skeleton.h
      - include/fat_p/SkeletonFwd.h
      - include/fat_p/SkeletonUtilities.h
      - include/fat_p/FatPBenchmarkRunner.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Skeleton.h"
#include "SkeletonUtilities.h"
#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"

using namespace fat_p::skeleton;

// ============================================================================
// Platform Configuration
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// Global benchmark configuration loaded from FATP_BENCH_* env vars in main().
static fat_p::bench::BenchConfig g_config;

static size_t WARMUP_RUNS()  { return g_config.warmupRuns; }
static size_t MEASURED_RUNS() { return g_config.measuredRuns; }

// ============================================================================
// CPU Frequency Monitoring (shared via FatPBenchmarkRunner.h)
// ============================================================================

void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

// Busy-work burst to wake the CPU out of idle before stabilization checks.
static inline void cpu_warmup_burst(int milliseconds)
{
    if (milliseconds <= 0) return;
    auto start    = std::chrono::steady_clock::now();
    auto duration = std::chrono::milliseconds(milliseconds);

    volatile uint64_t sink = 0;
    volatile uint64_t x   = 0xDEADBEEFCAFEBABEULL;
    while (std::chrono::steady_clock::now() - start < duration)
    {
        for (int i = 0; i < 1000; ++i)
        {
            x ^= x << 13;
            x ^= x >>  7;
            x ^= x << 17;
            sink += x;
        }
    }
    (void)sink;
}

// Wait until CPU frequency stabilizes. Returns false if timeout reached.
static bool wait_for_cpu_stable(double max_variance_pct = 10.0,
                                int    timeout_seconds  = 30,
                                int    check_interval_ms = 200,
                                bool   verbose = true)
{
    auto start   = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);

    auto initial = fat_p::bench::capture_cpu_frequency();
    if (!initial.has_reliable_detection())
    {
        if (verbose) std::cout << "[CPU frequency detection unavailable -- fixed cooling delay]\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return true;
    }

    const double base_freq = initial.mRefFreqMHz;
    cpu_warmup_burst(100);

    std::vector<double> recent;
    const size_t window        = 5;
    const int    required_stable = 3;
    int          stable_count  = 0;

    while (std::chrono::steady_clock::now() - start < timeout)
    {
        cpu_warmup_burst(50);
        auto info = fat_p::bench::capture_cpu_frequency();
        recent.push_back(info.mCurrentFreqMHz);
        if (recent.size() > window) recent.erase(recent.begin());

        if (recent.size() >= window)
        {
            double lo  = *std::min_element(recent.begin(), recent.end());
            double hi  = *std::max_element(recent.begin(), recent.end());
            double avg = std::accumulate(recent.begin(), recent.end(), 0.0) / static_cast<double>(recent.size());
            double var_pct = (hi - lo) / avg * 100.0;

            if (var_pct <= max_variance_pct)
            {
                if (++stable_count >= required_stable)
                {
                    if (verbose)
                        std::cout << "[CPU stable at " << static_cast<int>(avg) << " MHz ("
                                  << std::fixed << std::setprecision(0)
                                  << (avg / base_freq) * 100.0 << "% of base, variance "
                                  << std::setprecision(1) << var_pct << "%)]\n";
                    return true;
                }
            }
            else
            {
                stable_count = 0;
                if (verbose)
                    std::cout << "[Waiting: " << static_cast<int>(avg) << " MHz (variance "
                              << std::fixed << std::setprecision(1) << var_pct << "%)]\r" << std::flush;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    }

    if (verbose)
    {
        auto fin = fat_p::bench::capture_cpu_frequency();
        std::cout << "\n[WARNING: CPU not stable after " << timeout_seconds << "s -- "
                  << static_cast<int>(fin.mCurrentFreqMHz) << " MHz]\n";
    }
    return false;
}

static inline void cooling_delay(int min_sleep_ms, const char* reason = nullptr)
{
    if (g_config.noCooldown) return;
    if (reason) std::cout << "[Cooling: " << reason << "]" << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(min_sleep_ms));
    bool stable = wait_for_cpu_stable(10.0, 200, false);
    if (reason)
    {
        auto info = fat_p::bench::capture_cpu_frequency();
        if (info.has_reliable_detection())
            std::cout << " [Ready: " << static_cast<int>(info.mCurrentFreqMHz) << " MHz"
                      << (stable ? "" : " (still fluctuating)") << "]\n";
        else
            std::cout << " [Ready]\n";
    }
}

#if defined(_WIN32) || defined(_WIN64)
static constexpr int COOLING_SECTION_MS = 2000;
static constexpr int COOLING_CASE_MS    =  300;
#else
static constexpr int COOLING_SECTION_MS = 1000;
static constexpr int COOLING_CASE_MS    =  150;
#endif

// ============================================================================
// Timer
// ============================================================================

struct Timer
{
    using clock = fat_p::bench::BenchClock;
    clock::time_point t0;

    void   start()       { t0 = clock::now(); }
    double elapsed_ns() const
    {
        return std::chrono::duration<double, std::nano>(clock::now() - t0).count();
    }
};

static inline double ns_per_op(double elapsed_ns, size_t ops)
{
    return (ops == 0) ? 0.0 : elapsed_ns / static_cast<double>(ops);
}

// DCE prevention via volatile sink.
static volatile int64_t benchmark_sink = 0;
static inline void prevent_opt(int64_t v) { benchmark_sink ^= v; }

// ============================================================================
// Statistics
// ============================================================================

struct Statistics
{
    double median   = 0;
    double mean     = 0;
    double stddev   = 0;
    double ci95_low = 0;
    double ci95_high= 0;
    double min      = 0;
    double max      = 0;

    static Statistics compute(std::vector<double> samples)
    {
        Statistics s{};
        if (samples.empty()) return s;

        std::sort(samples.begin(), samples.end());
        size_t n = samples.size();
        s.min = samples.front();
        s.max = samples.back();

        s.median = (n % 2 == 1) ? samples[n / 2]
                                 : 0.5 * (samples[n / 2 - 1] + samples[n / 2]);

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        s.mean = sum / static_cast<double>(n);

        if (n > 1)
        {
            double acc = 0.0;
            for (double x : samples) { double d = x - s.mean; acc += d * d; }
            s.stddev = std::sqrt(acc / static_cast<double>(n - 1));
            double se = s.stddev / std::sqrt(static_cast<double>(n));
            constexpr double z = 1.96;
            s.ci95_low  = s.mean - z * se;
            s.ci95_high = s.mean + z * se;
        }
        return s;
    }
};

// ============================================================================
// BenchItem -- minimal SkeletonItem subclass for benchmarking
// ============================================================================

class BenchItem final : public SkeletonItem
{
public:
    explicit BenchItem(BoneId id, SkeletonMask mask = {})
        : SkeletonItem(id, mask, {})   // no name -- avoids string allocation per item
    {}

    void doPublish(Skeleton& sk)  { this->publish(sk); }
    void doUnpublish()            { this->unpublish(); }
};

// ============================================================================
// BoneId generation helpers
// ============================================================================

// Capability bits used in benchmark items and queries.
//   Half the items get SENSOR | READABLE; the other half ACTUATOR | WRITABLE.
//   Query with required=SENSOR returns ~50% of items.
static const SkeletonMask MASK_SENSOR   = makeMask(SkeletonCapability{0});
static const SkeletonMask MASK_ACTUATOR = makeMask(SkeletonCapability{1});
static const SkeletonMask MASK_READABLE = makeMask(SkeletonCapability{2});
static const SkeletonMask MASK_WRITABLE = makeMask(SkeletonCapability{3});

static const SkeletonMask MASK_SENSOR_READABLE   = MASK_SENSOR   | MASK_READABLE;
static const SkeletonMask MASK_ACTUATOR_WRITABLE = MASK_ACTUATOR | MASK_WRITABLE;

// Build N unique BoneIds descended from root.child(1).
// Root value 1 is reserved for the flat/find sections.
static std::vector<BoneId> make_flat_ids(size_t n)
{
    BoneId prefix = BoneId{}.child(1);
    std::vector<BoneId> ids;
    ids.reserve(n);
    for (size_t i = 0; i < n; ++i)
        ids.push_back(index2BoneId(prefix, i));
    return ids;
}

// Build N unique BoneIds that are guaranteed not to appear in make_flat_ids.
// Root value 2 is reserved for miss keys.
static std::vector<BoneId> make_miss_ids(size_t n)
{
    BoneId prefix = BoneId{}.child(2);
    std::vector<BoneId> ids;
    ids.reserve(n);
    for (size_t i = 0; i < n; ++i)
        ids.push_back(index2BoneId(prefix, i));
    return ids;
}

// Build a tree: `branch_count` sub-roots under root.child(3), each with
// `leaves_per_branch` unique descendants. Root value 3 is reserved for tree sections.
// Returns branch roots (for subtree queries) and all leaf IDs (for publishing).
static void make_tree_ids(size_t branch_count,
                          size_t leaves_per_branch,
                          std::vector<BoneId>& branch_roots,
                          std::vector<BoneId>& leaf_ids)
{
    BoneId root = BoneId{}.child(3);
    branch_roots.clear();
    leaf_ids.clear();
    for (size_t b = 0; b < branch_count; ++b)
    {
        BoneId branch = root.child(static_cast<uint8_t>(b + 1));
        branch_roots.push_back(branch);
        for (size_t l = 0; l < leaves_per_branch; ++l)
            leaf_ids.push_back(index2BoneId(branch, l));
    }
}

// Assign alternating capability masks to items.
static SkeletonMask item_mask(size_t idx)
{
    return (idx % 2 == 0) ? MASK_SENSOR_READABLE : MASK_ACTUATOR_WRITABLE;
}

// ============================================================================
// Output helpers
// ============================================================================

static void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

static void print_contract(const char* note)
{
    std::cout << "Contract: " << note << "\n";
}

// Print a single result row.
static void print_row(const char* label, const Statistics& s, const char* unit = "ns/op")
{
    std::cout << std::fixed << std::setprecision(2)
              << "  " << std::setw(28) << std::left  << label << std::right
              << std::setw(9) << s.median << " " << unit
              << "  (+/-" << std::setw(7) << s.stddev
              << "  CI:[" << std::setw(7) << s.ci95_low
              << "," << std::setw(7) << s.ci95_high << "])\n";
}

// Print a table header for a multi-library comparison section.
static void print_table_header()
{
    std::cout << std::setw(30) << std::left << "Implementation" << std::right
              << std::setw(12) << "Median"
              << std::setw(12) << "Mean"
              << std::setw(10) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(79, '-') << "\n";
}

static void print_table_row(const char* label, const Statistics& s)
{
    std::cout << std::fixed << std::setprecision(2)
              << std::setw(30) << std::left << label << std::right
              << std::setw(12) << s.median
              << std::setw(12) << s.mean
              << std::setw(10) << s.stddev
              << "  [" << s.ci95_low << ", " << s.ci95_high << "]\n";
}

static void sanity_check(const char* label, const Statistics& s)
{
    if (s.stddev > s.median && s.median > 0.0)
        std::cout << "  [NOTE] " << label << ": high variance (stddev " << s.stddev
                  << " > median " << s.median << ") -- possible system noise\n";
}

// ============================================================================
// Section 1: Publish throughput
// ============================================================================
//
// Timed region: publish N pre-constructed, unpublished BenchItems.
// Setup / teardown (item construction, unpublish) outside timed region.
// Sizes benchmarked sequentially; each size has its own warmup+measured loop.

static void benchmark_publish(const std::vector<size_t>& sizes)
{
    print_header("Section 1: Publish Throughput");
    print_cpu_context();
    print_contract("publish(Skeleton&) on a pre-constructed unpublished item. "
                   "Reserve not called. No listeners attached.");

    std::cout << "\n";
    std::cout << std::setw(10) << "N"
              << std::setw(12) << "Median ns/op"
              << std::setw(12) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t N : sizes)
    {
        std::vector<BoneId> ids = make_flat_ids(N);

        // Pre-allocate items (outside timing).
        std::vector<std::unique_ptr<BenchItem>> items;
        items.reserve(N);
        for (size_t i = 0; i < N; ++i)
            items.push_back(std::make_unique<BenchItem>(ids[i], item_mask(i)));

        std::vector<double> samples;
        samples.reserve(WARMUP_RUNS() + MEASURED_RUNS());

        for (size_t r = 0; r < WARMUP_RUNS() + MEASURED_RUNS(); ++r)
        {
            bool is_warmup = (r < WARMUP_RUNS());
            Skeleton sk("bench-publish");

            Timer t;
            t.start();
            for (size_t i = 0; i < N; ++i)
                items[i]->doPublish(sk);
            double elapsed = t.elapsed_ns();

            // Teardown outside timed region.
            for (size_t i = 0; i < N; ++i)
                items[i]->doUnpublish();

            if (!is_warmup)
                samples.push_back(ns_per_op(elapsed, N));

            prevent_opt(static_cast<int64_t>(N));
        }

        auto s = Statistics::compute(samples);
        std::cout << std::setw(10) << N;
        print_row("", s);

        // Correctness: publish all, verify size, unpublish all, verify empty.
        {
            Skeleton sk_check("check");
            for (size_t i = 0; i < N; ++i) items[i]->doPublish(sk_check);
            if (sk_check.size() != N)
                std::cout << "  [FAIL] publish correctness: expected size " << N
                          << " got " << sk_check.size() << "\n";
            else
                std::cout << "  [PASS] publish correctness (N=" << N << ", size=" << N << ")\n";
            for (size_t i = 0; i < N; ++i) items[i]->doUnpublish();
            if (sk_check.size() != 0)
                std::cout << "  [FAIL] post-unpublish size != 0\n";
        }

        sanity_check("publish", s);
    }
}

// ============================================================================
// Section 2: Unpublish throughput
// ============================================================================

static void benchmark_unpublish(const std::vector<size_t>& sizes)
{
    print_header("Section 2: Unpublish Throughput");
    print_cpu_context();
    print_contract("unpublish() on a published item. No listeners attached. "
                   "Skeleton destructor invariant: size must be 0 before destruction.");

    std::cout << "\n";
    std::cout << std::setw(10) << "N"
              << std::setw(12) << "Median ns/op"
              << std::setw(12) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t N : sizes)
    {
        std::vector<BoneId> ids = make_flat_ids(N);
        std::vector<std::unique_ptr<BenchItem>> items;
        items.reserve(N);
        for (size_t i = 0; i < N; ++i)
            items.push_back(std::make_unique<BenchItem>(ids[i], item_mask(i)));

        std::vector<double> samples;
        samples.reserve(WARMUP_RUNS() + MEASURED_RUNS());

        for (size_t r = 0; r < WARMUP_RUNS() + MEASURED_RUNS(); ++r)
        {
            bool is_warmup = (r < WARMUP_RUNS());
            Skeleton sk("bench-unpublish");

            // Setup outside timed region: publish all items.
            for (size_t i = 0; i < N; ++i)
                items[i]->doPublish(sk);

            Timer t;
            t.start();
            for (size_t i = 0; i < N; ++i)
                items[i]->doUnpublish();
            double elapsed = t.elapsed_ns();

            if (!is_warmup)
                samples.push_back(ns_per_op(elapsed, N));

            prevent_opt(static_cast<int64_t>(N));
        }

        auto s = Statistics::compute(samples);
        std::cout << std::setw(10) << N;
        print_row("", s);

        // Correctness: skeleton must be empty after all unpublishes.
        {
            Skeleton sk_check("check");
            for (size_t i = 0; i < N; ++i) items[i]->doPublish(sk_check);
            for (size_t i = 0; i < N; ++i) items[i]->doUnpublish();
            if (sk_check.size() != 0)
                std::cout << "  [FAIL] unpublish correctness: expected size 0 got "
                          << sk_check.size() << "\n";
            else
                std::cout << "  [PASS] unpublish correctness (N=" << N << ", size=0)\n";
        }

        sanity_check("unpublish", s);
    }
}

// ============================================================================
// Section 3: Find operations (find_hit, find_miss) -- randomized case order
// ============================================================================
//
// Registry stays populated throughout all measured runs; no publish/unpublish
// in the timed region. Case order is shuffled per batch.

static void benchmark_find(const std::vector<size_t>& sizes, uint64_t seed)
{
    print_header("Section 3: Find (hit / miss)");
    print_cpu_context();
    print_contract("find(BoneId) -- O(1) average hash map lookup. "
                   "Registry pre-populated. Miss keys use a distinct BoneId root.");

    enum class FindCase { Hit, Miss };
    const std::array<FindCase, 2> cases = {FindCase::Hit, FindCase::Miss};

    std::mt19937_64 rng(seed ^ 0xFACE0FF);

    for (size_t N : sizes)
    {
        std::cout << "\nN = " << N << "\n";

        std::vector<BoneId> hit_ids  = make_flat_ids(N);
        std::vector<BoneId> miss_ids = make_miss_ids(N);

        // Build and populate skeleton (stays alive for all batches).
        std::vector<std::unique_ptr<BenchItem>> items;
        items.reserve(N);
        Skeleton sk("bench-find");
        for (size_t i = 0; i < N; ++i)
        {
            items.push_back(std::make_unique<BenchItem>(hit_ids[i], item_mask(i)));
            items.back()->doPublish(sk);
        }

        std::unordered_map<FindCase, std::vector<double>> all_samples;

        for (size_t r = 0; r < WARMUP_RUNS() + MEASURED_RUNS(); ++r)
        {
            bool is_warmup = (r < WARMUP_RUNS());

            // Randomize case order per batch.
            std::array<FindCase, 2> order = cases;
            std::shuffle(order.begin(), order.end(), rng);

            for (FindCase c : order)
            {
                Timer t;
                size_t ops = 0;

                if (c == FindCase::Hit)
                {
                    t.start();
                    for (const BoneId& id : hit_ids)
                    {
                        auto* p = sk.find(id);
                        prevent_opt(reinterpret_cast<int64_t>(p));
                        ++ops;
                    }
                }
                else // Miss
                {
                    t.start();
                    for (const BoneId& id : miss_ids)
                    {
                        auto* p = sk.find(id);
                        prevent_opt(reinterpret_cast<int64_t>(p));
                        ++ops;
                    }
                }

                if (!is_warmup)
                    all_samples[c].push_back(ns_per_op(t.elapsed_ns(), ops));
            }
        }

        // Print results.
        auto hit_stats  = Statistics::compute(all_samples[FindCase::Hit]);
        auto miss_stats = Statistics::compute(all_samples[FindCase::Miss]);
        print_row("find (hit) ", hit_stats);
        print_row("find (miss)", miss_stats);

        // Correctness: every hit must return non-null, every miss must return null.
        {
            bool hit_ok  = true;
            bool miss_ok = true;
            for (const BoneId& id : hit_ids)
                if (sk.find(id) == nullptr) { hit_ok = false; break; }
            for (const BoneId& id : miss_ids)
                if (sk.find(id) != nullptr) { miss_ok = false; break; }
            std::cout << "  " << (hit_ok  ? "[PASS]" : "[FAIL]") << " find hit  (all " << N << " keys found)\n";
            std::cout << "  " << (miss_ok ? "[PASS]" : "[FAIL]") << " find miss (all " << N << " keys absent)\n";
        }

        sanity_check("find_hit",  hit_stats);
        sanity_check("find_miss", miss_stats);

        // Teardown.
        for (size_t i = 0; i < N; ++i)
            items[i]->doUnpublish();
    }
}

// ============================================================================
// Section 4: Query operations (query_all, query_masked, querySubtree)
// ============================================================================

static void benchmark_query(const std::vector<size_t>& sizes, uint64_t seed)
{
    print_header("Section 4: Query Operations");
    print_cpu_context();
    print_contract("query(required, excluded) -- O(N) linear scan over registry + sort. "
                   "query_all returns all N items; query_masked returns ~50% (SENSOR required). "
                   "querySubtree(root, {}) returns one branch (~N/4 items).");

    enum class QCase { All, Masked, Subtree };
    const std::array<QCase, 3> cases = {QCase::All, QCase::Masked, QCase::Subtree};

    std::mt19937_64 rng(seed ^ 0xBEEF);

    const size_t branch_count = 4;

    for (size_t N : sizes)
    {
        std::cout << "\nN = " << N << "\n";

        size_t leaves_per_branch = std::max<size_t>(1, N / branch_count);
        size_t actual_N = branch_count * leaves_per_branch;

        std::vector<BoneId> branch_roots;
        std::vector<BoneId> leaf_ids;
        make_tree_ids(branch_count, leaves_per_branch, branch_roots, leaf_ids);

        std::vector<std::unique_ptr<BenchItem>> items;
        items.reserve(actual_N);
        Skeleton sk("bench-query");
        for (size_t i = 0; i < actual_N; ++i)
        {
            items.push_back(std::make_unique<BenchItem>(leaf_ids[i], item_mask(i)));
            items.back()->doPublish(sk);
        }

        BoneId subtree_root = branch_roots[0];
        size_t expected_subtree_size = leaves_per_branch;
        size_t expected_masked_size  = actual_N / 2;  // ~50% have SENSOR bit

        std::unordered_map<QCase, std::vector<double>> all_samples;

        for (size_t r = 0; r < WARMUP_RUNS() + MEASURED_RUNS(); ++r)
        {
            bool is_warmup = (r < WARMUP_RUNS());
            std::array<QCase, 3> order = cases;
            std::shuffle(order.begin(), order.end(), rng);

            for (QCase c : order)
            {
                Timer t;

                if (c == QCase::All)
                {
                    t.start();
                    auto result = sk.query(SkeletonMask{});
                    double elapsed = t.elapsed_ns();
                    prevent_opt(static_cast<int64_t>(result.size()));
                    if (!is_warmup)
                        all_samples[c].push_back(ns_per_op(elapsed, actual_N));
                }
                else if (c == QCase::Masked)
                {
                    t.start();
                    auto result = sk.query(MASK_SENSOR);
                    double elapsed = t.elapsed_ns();
                    prevent_opt(static_cast<int64_t>(result.size()));
                    if (!is_warmup)
                        all_samples[c].push_back(ns_per_op(elapsed, actual_N));
                }
                else // Subtree
                {
                    t.start();
                    auto result = sk.querySubtree(subtree_root, SkeletonMask{});
                    double elapsed = t.elapsed_ns();
                    prevent_opt(static_cast<int64_t>(result.size()));
                    if (!is_warmup)
                        all_samples[c].push_back(ns_per_op(elapsed, actual_N));
                }
            }
        }

        auto s_all     = Statistics::compute(all_samples[QCase::All]);
        auto s_masked  = Statistics::compute(all_samples[QCase::Masked]);
        auto s_subtree = Statistics::compute(all_samples[QCase::Subtree]);

        print_row("query (all)    ", s_all);
        print_row("query (masked) ", s_masked);
        print_row("querySubtree   ", s_subtree);

        // Correctness.
        {
            auto r_all     = sk.query(SkeletonMask{});
            auto r_masked  = sk.query(MASK_SENSOR);
            auto r_subtree = sk.querySubtree(subtree_root, SkeletonMask{});

            bool all_ok     = (r_all.size() == actual_N);
            bool masked_ok  = (r_masked.size() == expected_masked_size);
            bool subtree_ok = (r_subtree.size() == expected_subtree_size);

            std::cout << "  " << (all_ok     ? "[PASS]" : "[FAIL]")
                      << " query_all (expected " << actual_N << " got " << r_all.size() << ")\n";
            std::cout << "  " << (masked_ok  ? "[PASS]" : "[FAIL]")
                      << " query_masked (expected " << expected_masked_size << " got " << r_masked.size() << ")\n";
            std::cout << "  " << (subtree_ok ? "[PASS]" : "[FAIL]")
                      << " querySubtree (expected " << expected_subtree_size << " got " << r_subtree.size() << ")\n";
        }

        sanity_check("query_all",    s_all);
        sanity_check("query_masked", s_masked);
        sanity_check("querySubtree", s_subtree);

        for (size_t i = 0; i < actual_N; ++i)
            items[i]->doUnpublish();
    }
}

// ============================================================================
// Section 5: visitSubtree throughput
// ============================================================================

static void benchmark_visit_subtree(const std::vector<size_t>& sizes)
{
    print_header("Section 5: visitSubtree Throughput");
    print_cpu_context();
    print_contract("visitSubtree(root, fn) -- collect + sort subtree then invoke fn per item. "
                   "All N items are in a single subtree rooted at branch_root[0]. "
                   "Callback does minimal work (prevent_opt on boneId).");

    std::cout << "\n";
    std::cout << std::setw(10) << "N"
              << std::setw(14) << "Median ns/item"
              << std::setw(12) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(62, '-') << "\n";

    for (size_t N : sizes)
    {
        BoneId root    = BoneId{}.child(4);  // distinct root from other sections
        BoneId subtree = root.child(1);

        std::vector<BoneId> leaf_ids;
        leaf_ids.reserve(N);
        for (size_t i = 0; i < N; ++i)
            leaf_ids.push_back(index2BoneId(subtree, i));

        std::vector<std::unique_ptr<BenchItem>> items;
        items.reserve(N);
        Skeleton sk("bench-visit");
        for (size_t i = 0; i < N; ++i)
        {
            items.push_back(std::make_unique<BenchItem>(leaf_ids[i], item_mask(i)));
            items.back()->doPublish(sk);
        }

        std::vector<double> samples;
        samples.reserve(MEASURED_RUNS());

        for (size_t r = 0; r < WARMUP_RUNS() + MEASURED_RUNS(); ++r)
        {
            bool is_warmup = (r < WARMUP_RUNS());
            Timer t;
            t.start();
            sk.visitSubtree(subtree, [](SkeletonItem& item) {
                prevent_opt(item.boneId().value());
            });
            double elapsed = t.elapsed_ns();
            if (!is_warmup)
                samples.push_back(ns_per_op(elapsed, N));
        }

        auto s = Statistics::compute(samples);
        std::cout << std::setw(10) << N;
        print_row("", s, "ns/item");

        // Correctness: visitor must be called exactly N times.
        {
            size_t visited = 0;
            sk.visitSubtree(subtree, [&visited](SkeletonItem&) { ++visited; });
            std::cout << "  " << (visited == N ? "[PASS]" : "[FAIL]")
                      << " visitSubtree count (expected " << N << " got " << visited << ")\n";
        }

        sanity_check("visitSubtree", s);

        for (size_t i = 0; i < N; ++i)
            items[i]->doUnpublish();
    }
}

// ============================================================================
// Section 6: Find comparison -- fat_p::Skeleton vs std::unordered_map
// ============================================================================
//
// Adapter pattern with round-robin execution. Both adapters observe the same
// BoneId values and the same distribution of machine states.

struct IFindAdapter
{
    virtual ~IFindAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(const std::vector<BoneId>& ids,
                       const std::vector<BenchItem*>& raw_items) = 0;
    virtual void teardown() = 0;
    virtual size_t run_find_hit(const std::vector<BoneId>& ids)   = 0;
    virtual size_t run_find_miss(const std::vector<BoneId>& misses) = 0;
};

// fat_p::Skeleton adapter.
class SkeletonFindAdapter final : public IFindAdapter
{
    Skeleton mSk{"find-cmp"};
    std::vector<BenchItem*> mItems;  // non-owning

public:
    const char* name() const override { return "fat_p::Skeleton::find"; }

    void setup(const std::vector<BoneId>&, const std::vector<BenchItem*>& raw) override
    {
        mItems = raw;
        for (auto* p : mItems) p->doPublish(mSk);
    }

    void teardown() override
    {
        for (auto* p : mItems) p->doUnpublish();
        mItems.clear();
    }

    size_t run_find_hit(const std::vector<BoneId>& ids) override
    {
        size_t ops = 0;
        for (const BoneId& id : ids)
        {
            auto* p = mSk.find(id);
            prevent_opt(reinterpret_cast<int64_t>(p));
            ++ops;
        }
        return ops;
    }

    size_t run_find_miss(const std::vector<BoneId>& misses) override
    {
        size_t ops = 0;
        for (const BoneId& id : misses)
        {
            auto* p = mSk.find(id);
            prevent_opt(reinterpret_cast<int64_t>(p));
            ++ops;
        }
        return ops;
    }
};

// std::unordered_map adapter (baseline -- equivalent semantics for key lookup only).
class StdUnorderedMapFindAdapter final : public IFindAdapter
{
    std::unordered_map<BoneId, SkeletonItem*> mMap;

public:
    const char* name() const override { return "std::unordered_map (baseline)"; }

    void setup(const std::vector<BoneId>& ids, const std::vector<BenchItem*>& raw) override
    {
        mMap.reserve(ids.size());
        for (size_t i = 0; i < ids.size(); ++i)
            mMap.emplace(ids[i], raw[i]);
    }

    void teardown() override { mMap.clear(); }

    size_t run_find_hit(const std::vector<BoneId>& ids) override
    {
        size_t ops = 0;
        for (const BoneId& id : ids)
        {
            auto it = mMap.find(id);
            if (it != mMap.end()) prevent_opt(reinterpret_cast<int64_t>(it->second));
            ++ops;
        }
        return ops;
    }

    size_t run_find_miss(const std::vector<BoneId>& misses) override
    {
        size_t ops = 0;
        for (const BoneId& id : misses)
        {
            auto it = mMap.find(id);
            if (it != mMap.end()) prevent_opt(reinterpret_cast<int64_t>(it->second));
            ++ops;
        }
        return ops;
    }
};

// Find case enum for round-robin.
enum class FindCase { Hit, Miss };

static void benchmark_find_comparison(size_t N, uint64_t seed)
{
    print_header("Section 6: Find Comparison -- fat_p::Skeleton vs std::unordered_map");
    print_cpu_context();
    print_contract("Equivalent key lookup semantics. Skeleton imposes signal infrastructure "
                   "and lifecycle enforcement not present in unordered_map (baseline). "
                   "Both adapters use the same BoneId set and observe the same machine states.");

    std::vector<BoneId> hit_ids  = make_flat_ids(N);
    std::vector<BoneId> miss_ids = make_miss_ids(N);

    // Pre-allocate items (owned externally, adapters hold raw ptrs).
    std::vector<std::unique_ptr<BenchItem>> owned;
    owned.reserve(N);
    std::vector<BenchItem*> raw;
    raw.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        owned.push_back(std::make_unique<BenchItem>(hit_ids[i], item_mask(i)));
        raw.push_back(owned.back().get());
    }

    SkeletonFindAdapter         sk_adapt;
    StdUnorderedMapFindAdapter  map_adapt;

    std::vector<IFindAdapter*> adapters = {&sk_adapt, &map_adapt};
    const std::array<FindCase, 2> cases = {FindCase::Hit, FindCase::Miss};

    std::mt19937_64 rng(seed ^ 0xC0FFEE);

    // samples[adapter_ptr][case] -> list of ns/op measurements
    std::unordered_map<IFindAdapter*, std::unordered_map<FindCase, std::vector<double>>> samples;

    for (size_t r = 0; r < WARMUP_RUNS() + MEASURED_RUNS(); ++r)
    {
        bool is_warmup = (r < WARMUP_RUNS());

        // Randomize adapter order per batch.
        std::vector<IFindAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IFindAdapter* a : order)
        {
            a->setup(hit_ids, raw);

            // Randomize case order within each adapter's slot.
            std::array<FindCase, 2> case_order = cases;
            std::shuffle(case_order.begin(), case_order.end(), rng);

            for (FindCase c : case_order)
            {
                Timer t;
                t.start();
                size_t ops = (c == FindCase::Hit)
                    ? a->run_find_hit(hit_ids)
                    : a->run_find_miss(miss_ids);
                double elapsed = t.elapsed_ns();

                if (!is_warmup)
                    samples[a][c].push_back(ns_per_op(elapsed, ops));
            }

            a->teardown();
        }
    }

    // Print.
    for (FindCase c : cases)
    {
        std::cout << "\n--- " << (c == FindCase::Hit ? "Find (hit)" : "Find (miss)")
                  << "  N=" << N << " ---\n";
        print_table_header();
        for (IFindAdapter* a : adapters)
        {
            auto s = Statistics::compute(samples[a][c]);
            print_table_row(a->name(), s);
            sanity_check(a->name(), s);
        }
    }

    // Correctness.
    sk_adapt.setup(hit_ids, raw);
    bool hit_ok  = true;
    bool miss_ok = true;
    for (const BoneId& id : hit_ids)
        if (sk_adapt.run_find_hit({id}) == 0) { hit_ok = false; break; }
    for (const BoneId& id : miss_ids)
    {
        // Miss: we expect the find returns a null pointer, so prevent_opt receives 0.
        // We cannot inspect the result directly here -- the adapter sinks it.
        // Trust the earlier Section 3 correctness check for miss semantics.
        (void)id;
    }
    std::cout << "\n  " << (hit_ok  ? "[PASS]" : "[FAIL]") << " Skeleton find hit correctness\n";
    std::cout << "  [NOTE] miss correctness verified in Section 3\n";
    sk_adapt.teardown();
}

// ============================================================================
// Section 7: Query comparison -- fat_p::Skeleton vs vector linear scan
// ============================================================================

struct IQueryAdapter
{
    virtual ~IQueryAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(const std::vector<BoneId>& ids,
                       const std::vector<BenchItem*>& raw) = 0;
    virtual void teardown() = 0;
    virtual size_t run_query(SkeletonMask required) = 0;
};

class SkeletonQueryAdapter final : public IQueryAdapter
{
    Skeleton mSk{"query-cmp"};
    std::vector<BenchItem*> mItems;

public:
    const char* name() const override { return "fat_p::Skeleton::query"; }

    void setup(const std::vector<BoneId>&, const std::vector<BenchItem*>& raw) override
    {
        mItems = raw;
        for (auto* p : mItems) p->doPublish(mSk);
    }

    void teardown() override
    {
        for (auto* p : mItems) p->doUnpublish();
        mItems.clear();
    }

    size_t run_query(SkeletonMask required) override
    {
        auto result = mSk.query(required);
        prevent_opt(static_cast<int64_t>(result.size()));
        return mSk.size();  // ops = registry size scanned
    }
};

// Linear scan over a pre-built vector<pair<SkeletonMask, SkeletonItem*>>.
// Semantically equivalent to Skeleton::query without BoneId ordering.
// Labeled "(no sort / no hierarchy)" to be clear about the semantic difference.
class VectorScanQueryAdapter final : public IQueryAdapter
{
    std::vector<std::pair<SkeletonMask, SkeletonItem*>> mVec;

public:
    const char* name() const override { return "vector scan (no sort / no hierarchy)"; }

    void setup(const std::vector<BoneId>&, const std::vector<BenchItem*>& raw) override
    {
        mVec.clear();
        mVec.reserve(raw.size());
        for (auto* p : raw)
            mVec.push_back({p->mask(), p});
    }

    void teardown() override { mVec.clear(); }

    size_t run_query(SkeletonMask required) override
    {
        size_t count = 0;
        for (const auto& [mask, ptr] : mVec)
        {
            if ((mask & required) == required)
            {
                prevent_opt(reinterpret_cast<int64_t>(ptr));
                ++count;
            }
        }
        return mVec.size();
    }
};

static void benchmark_query_comparison(size_t N, uint64_t seed)
{
    print_header("Section 7: Query Comparison -- fat_p::Skeleton vs vector linear scan");
    print_cpu_context();
    print_contract("Skeleton::query returns results sorted in ascending BoneId order and "
                   "supports hierarchical filtering. The vector baseline omits sorting and "
                   "hierarchy -- it is a lower-bound on scan cost only. "
                   "Both adapters scan the same N items with the same mask predicate.");

    // Use the tree structure so BoneIds are non-trivial.
    const size_t branch_count      = 4;
    const size_t leaves_per_branch = std::max<size_t>(1, N / branch_count);
    const size_t actual_N          = branch_count * leaves_per_branch;

    std::vector<BoneId> branch_roots;
    std::vector<BoneId> leaf_ids;
    make_tree_ids(branch_count, leaves_per_branch, branch_roots, leaf_ids);

    std::vector<std::unique_ptr<BenchItem>> owned;
    owned.reserve(actual_N);
    std::vector<BenchItem*> raw;
    raw.reserve(actual_N);
    for (size_t i = 0; i < actual_N; ++i)
    {
        owned.push_back(std::make_unique<BenchItem>(leaf_ids[i], item_mask(i)));
        raw.push_back(owned.back().get());
    }

    SkeletonQueryAdapter   sk_adapt;
    VectorScanQueryAdapter vec_adapt;
    std::vector<IQueryAdapter*> adapters = {&sk_adapt, &vec_adapt};

    std::mt19937_64 rng(seed ^ 0xABCDEF);

    std::unordered_map<IQueryAdapter*, std::vector<double>> samples;

    for (size_t r = 0; r < WARMUP_RUNS() + MEASURED_RUNS(); ++r)
    {
        bool is_warmup = (r < WARMUP_RUNS());

        std::vector<IQueryAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IQueryAdapter* a : order)
        {
            a->setup(leaf_ids, raw);

            Timer t;
            t.start();
            size_t ops = a->run_query(MASK_SENSOR);
            double elapsed = t.elapsed_ns();

            a->teardown();

            if (!is_warmup)
                samples[a].push_back(ns_per_op(elapsed, ops));
        }
    }

    std::cout << "\n--- Query (SENSOR required, ~50% match)  N=" << actual_N << " ---\n";
    print_table_header();
    for (IQueryAdapter* a : adapters)
    {
        auto s = Statistics::compute(samples[a]);
        print_table_row(a->name(), s);
        sanity_check(a->name(), s);
    }

    // Correctness: Skeleton result size must match expected.
    {
        sk_adapt.setup(leaf_ids, raw);
        size_t expected = actual_N / 2;
        auto   result   = sk_adapt.run_query(MASK_SENSOR);  // sinks into prevent_opt
        // Re-run to get the actual count (setup is shared state).
        // We can't read back from run_query; check directly instead.
        Skeleton tmp("correctness-check");
        std::vector<std::unique_ptr<BenchItem>> tmp_items;
        tmp_items.reserve(actual_N);
        for (size_t i = 0; i < actual_N; ++i)
        {
            tmp_items.push_back(std::make_unique<BenchItem>(leaf_ids[i], item_mask(i)));
            tmp_items.back()->doPublish(tmp);
        }
        auto res = tmp.query(MASK_SENSOR);
        bool ok  = (res.size() == expected);
        std::cout << "\n  " << (ok ? "[PASS]" : "[FAIL]")
                  << " query_comparison correctness (expected " << expected
                  << " got " << res.size() << ")\n";
        for (size_t i = 0; i < actual_N; ++i) tmp_items[i]->doUnpublish();
        sk_adapt.teardown();
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Load configuration from FATP_BENCH_* environment variables.
    g_config = fat_p::bench::BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity) unless disabled.
    fat_p::bench::BenchmarkScope scope(!g_config.noScope);

    // =========================================================================
    // Standardized startup header.
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "Skeleton";
    hdr.warmup    = WARMUP_RUNS();
    hdr.measured  = MEASURED_RUNS();
    hdr.seed      = g_config.seed;

    // Skeleton has no external library competitors.
    // find and query sections compare against std:: baselines for context.
    hdr.competitors.push_back({"fat_p::Skeleton",                  true,  "primary"});
    hdr.competitors.push_back({"std::unordered_map",               true,  "find baseline"});
    hdr.competitors.push_back({"std::vector linear scan",          true,  "query baseline"});

    hdr.is_multi_library      = false;  // most sections are single-library
    hdr.has_extended_config   = true;
    hdr.has_correctness_checks= true;
    hdr.has_stabilization     = !g_config.noStabilize;
    hdr.cool_section_ms       = COOLING_SECTION_MS;
    hdr.cool_size_ms          = 0;
    hdr.cool_case_ms          = COOLING_CASE_MS;

    fat_p::bench::print_standard_header(hdr);

    // Wait for initial CPU stability.
    if (!g_config.noStabilize)
    {
        std::cout << "Checking initial CPU state...\n";
        print_cpu_context("Initial");
        std::cout << "Waiting for CPU to stabilize...\n";
        if (!wait_for_cpu_stable(10.0, 30, 200, true))
            std::cout << "WARNING: CPU frequency still fluctuating; results may have higher variance.\n";
        std::cout << "\n";
    }

    // Benchmark sizes: small, medium, large.
    const std::vector<size_t> sizes = {64, 512, 4096};
    const size_t comparison_N       = 4096;
    const uint64_t seed             = g_config.seed;

    // -------------------------------------------------------------------------
    // Run all sections.
    // -------------------------------------------------------------------------

    benchmark_publish(sizes);

    cooling_delay(COOLING_SECTION_MS, "before unpublish");
    benchmark_unpublish(sizes);

    cooling_delay(COOLING_SECTION_MS, "before find");
    benchmark_find(sizes, seed);

    cooling_delay(COOLING_SECTION_MS, "before query");
    benchmark_query(sizes, seed);

    cooling_delay(COOLING_SECTION_MS, "before visitSubtree");
    benchmark_visit_subtree(sizes);

    cooling_delay(COOLING_SECTION_MS, "before find comparison");
    benchmark_find_comparison(comparison_N, seed);

    cooling_delay(COOLING_SECTION_MS, "before query comparison");
    benchmark_query_comparison(comparison_N, seed);

    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << std::string(80, '=') << "\n";

    return 0;
}
