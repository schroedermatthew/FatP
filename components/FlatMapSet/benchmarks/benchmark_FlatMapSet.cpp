// benchmark_FlatMapSet.cpp
//
// FAT-P FlatMap/FlatSet benchmarks using unified FatPBenchmarkRunner infrastructure.
//
// Architecture: Round-robin execution with randomized order per run.
// Uses fat_p::ordered_unique_range for bulk build operations (apples-to-apples with Boost).
//
// Design Invariants:
//   1. Each measured run executes exactly one timed iteration per library.
//   2. Library execution order is randomized per run.
//   3. Setup, reserve, and teardown occur outside timed regions.
//   4. All libraries observe the same distribution of machine states.
//   5. Medians are the primary reported statistic.
//
// Fat-P Libraries:
//   - fat_p::FlatMap: Sorted vector with binary search, O(log n) lookup
//   - fat_p::FlatSet: Sorted vector for unique elements
//
// Competitor Libraries: boost::flat_map, absl::btree_map, std::flat_map (C++23), std::map
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_FlatMapSet.cpp -o bench_fm
//   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_FlatMapSet.cpp /link advapi32.lib
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS()   - Warmup iterations (default: 3)
//   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
//   FATP_BENCH_SEED          - RNG seed (default: 12345)
//   FATP_BENCH_MIN_BATCH_MS  - Min batch duration (default: 50)
//   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
//   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
//   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
//   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
//   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
//   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
//
// Run:
//   ./bench_fm
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_fm

/*
FATP_META:
  meta_version: 1
  component: FlatMapSet
  file_role: benchmark
  path: components/FlatMapSet/benchmarks/benchmark_FlatMapSet.cpp
  layer: Testing
  namespace: fat_p
  summary: "Benchmarks for FlatMapSet."
  api_stability: in_work
  related:
    docs_search: "FlatMapSet"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/FlatMap.h
      - include/fat_p/FlatSet.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 13
    defines_unprefixed: 13
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
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
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"

// Fat-P FlatMap/FlatSet (conditional - implementation may be pending)
#if __has_include("FlatMap.h")
#include "FlatMap.h"
#define HAS_FATP_FLATMAP 1
#else
#define HAS_FATP_FLATMAP 0
#endif

#if __has_include("FlatSet.h")
#include "FlatSet.h"
#define HAS_FATP_FLATSET 1
#else
#define HAS_FATP_FLATSET 0
#endif

// Optional: Boost flat containers
#if __has_include(<boost/container/flat_map.hpp>)
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#define HAS_BOOST_FLAT 1
#else
#define HAS_BOOST_FLAT 0
#endif

// Optional: Abseil B-tree containers
#if __has_include("absl/container/btree_map.h")
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#define HAS_ABSL_BTREE 1
#else
#define HAS_ABSL_BTREE 0
#endif

// Optional: Folly sorted_vector_map (requires -DUSE_FOLLY=1)
#ifndef USE_FOLLY
#define USE_FOLLY 0
#endif
#if defined(USE_FOLLY) && USE_FOLLY && __has_include(<folly/sorted_vector_types.h>)
#include <folly/sorted_vector_types.h>
#define HAS_FOLLY 1
#else
#define HAS_FOLLY 0
#endif

// Optional: C++23 std::flat_map and std::flat_set
#if __cplusplus >= 202302L && __has_include(<flat_map>)
#include <flat_map>
#include <flat_set>
#define HAS_STD_FLATMAP 1
#else
#define HAS_STD_FLATMAP 0
#endif

// ============================================================================
// Platform Configuration
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winreg.h>
#endif

// Global benchmark configuration (loaded from FATP_BENCH_* env vars in main)
static fat_p::bench::BenchConfig g_config;

// Accessors for backward compatibility with existing benchmark code
static size_t WARMUP_RUNS()
{
    return g_config.warmupRuns;
}
static size_t MEASURED_RUNS()
{
    return g_config.measuredRuns;
}

// ============================================================================
// CPU Frequency Monitoring (Shared)
// ============================================================================

void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

// ============================================================================
// Benchmark Environment Configuration
// ============================================================================

// Use shared BenchmarkScope from FatPBenchmarkRunner.h
using fat_p::bench::BenchmarkScope;

// ============================================================================
// Timer
// ============================================================================

struct Timer
{
    using clock = fat_p::bench::BenchClock;
    clock::time_point t0;

    void start()
    {
        t0 = clock::now();
    }

    double elapsed_ns() const
    {
        auto t1 = clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }
};

static inline double ns_per_op(double elapsed_ns, size_t ops)
{
    return (ops == 0) ? 0.0 : (elapsed_ns / static_cast<double>(ops));
}

// Prevent dead code elimination
static volatile int64_t benchmark_sink = 0;

static inline void prevent_opt(int64_t value)
{
    benchmark_sink ^= value;
}

// ============================================================================
// CPU Frequency Stability Monitoring
// ============================================================================

static inline void cpu_warmup_burst(int milliseconds)
{
    if (milliseconds <= 0)
    {
        return;
    }

    auto start = std::chrono::steady_clock::now();
    auto duration = std::chrono::milliseconds(milliseconds);

    volatile uint64_t sink = 0;
    volatile uint64_t x = 0xDEADBEEFCAFEBABEULL;

    while (std::chrono::steady_clock::now() - start < duration)
    {
        for (int i = 0; i < 1000; ++i)
        {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            sink += x;
        }
    }

    benchmark_sink ^= static_cast<int64_t>(sink);
}

static bool wait_for_cpu_stable(double max_variance_percent = 10.0,
                                int timeout_seconds = 30,
                                int check_interval_ms = 200,
                                bool verbose = true)
{
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);

    auto initial_info = fat_p::bench::capture_cpu_frequency();
    if (!initial_info.has_reliable_detection())
    {
        if (verbose)
        {
            std::cout << "[CPU frequency detection unavailable - using fixed cooling delay]\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return true;
    }

    double base_freq = initial_info.mRefFreqMHz;
    double min_required_freq = base_freq * 0.60;

    cpu_warmup_burst(100);

    std::vector<double> recent_readings;
    const size_t window_size = 5;
    const int required_stable = 3;
    int stable_count = 0;

    while (std::chrono::steady_clock::now() - start < timeout)
    {
        cpu_warmup_burst(50);

        auto info = fat_p::bench::capture_cpu_frequency();
        recent_readings.push_back(info.mCurrentFreqMHz);

        if (recent_readings.size() > window_size)
        {
            recent_readings.erase(recent_readings.begin());
        }

        if (recent_readings.size() >= window_size)
        {
            double min_freq = *std::min_element(recent_readings.begin(), recent_readings.end());
            double max_freq = *std::max_element(recent_readings.begin(), recent_readings.end());
            double avg_freq = 0;
            for (double f : recent_readings)
            {
                avg_freq += f;
            }
            avg_freq /= recent_readings.size();

            double variance_pct = (max_freq - min_freq) / avg_freq * 100.0;
            bool variance_ok = variance_pct <= max_variance_percent;
            bool freq_floor_ok = avg_freq >= min_required_freq;
            bool is_stable = variance_ok && freq_floor_ok;

            if (is_stable)
            {
                ++stable_count;
                if (stable_count >= required_stable)
                {
                    if (verbose)
                    {
                        double pct_of_base = (avg_freq / base_freq) * 100.0;
                        std::cout << "[CPU stable at " << static_cast<int>(avg_freq) << " MHz (" << std::fixed
                                  << std::setprecision(0) << pct_of_base << "% of base)]\n";
                    }
                    return true;
                }
            }
            else
            {
                stable_count = 0;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    }

    if (verbose)
    {
        std::cout << "[WARNING: CPU frequency still unstable after " << timeout_seconds << "s]\n";
    }
    return false;
}

static inline void cooling_delay(int min_sleep_ms, const char* reason = nullptr)
{
    if (reason)
    {
        std::cout << "[Cooling: " << reason << "]" << std::flush;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(min_sleep_ms));
    wait_for_cpu_stable(10.0, 15, 200, false);

    if (reason)
    {
        auto info = fat_p::bench::capture_cpu_frequency();
        if (info.has_reliable_detection())
        {
            std::cout << " [Ready: " << static_cast<int>(info.mCurrentFreqMHz) << " MHz]\n";
        }
        else
        {
            std::cout << " [Ready]\n";
        }
    }
}

#if defined(_WIN32) || defined(_WIN64)
static constexpr int COOLING_DELAY_SECTION_MS = 2000;
static constexpr int COOLING_DELAY_SIZE_MS = 1000;
static constexpr int COOLING_DELAY_CASE_MS = 300;
#else
static constexpr int COOLING_DELAY_SECTION_MS = 1000;
static constexpr int COOLING_DELAY_SIZE_MS = 500;
static constexpr int COOLING_DELAY_CASE_MS = 200;
#endif

// ============================================================================
// Statistics
// ============================================================================

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
        if (samples.empty())
        {
            return s;
        }

        std::sort(samples.begin(), samples.end());
        size_t n = samples.size();

        s.min = samples.front();
        s.max = samples.back();

        // Median
        if (n % 2 == 1)
        {
            s.median = samples[n / 2];
        }
        else
        {
            s.median = 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
        }

        // Mean
        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        s.mean = sum / static_cast<double>(n);

        // Stddev (sample)
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
            constexpr double z = 1.96;
            s.ci95_low = s.mean - z * se;
            s.ci95_high = s.mean + z * se;
        }

        return s;
    }
};

// ============================================================================
// Test Data Generation
// ============================================================================

std::vector<int64_t> generate_random_keys(size_t n, uint64_t seed = 12345)
{
    std::vector<int64_t> keys(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    for (size_t i = 0; i < n; ++i)
    {
        keys[i] = dist(rng);
    }
    return keys;
}

std::vector<int64_t> generate_sorted_keys(size_t n, uint64_t seed = 12345)
{
    auto keys = generate_random_keys(n, seed);
    std::sort(keys.begin(), keys.end());
    // Remove duplicates for sorted containers
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

std::vector<int64_t> generate_missing_keys(size_t n, uint64_t seed = 99999)
{
    std::vector<int64_t> keys(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int64_t> dist(INT64_MIN, -3);
    for (size_t i = 0; i < n; ++i)
    {
        keys[i] = dist(rng);
    }
    return keys;
}

// String keys for heterogeneous lookup benchmarks
std::vector<std::string> generate_string_keys(size_t n, uint64_t seed = 54321)
{
    std::vector<std::string> keys(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> len_dist(8, 32);
    std::uniform_int_distribution<int> char_dist('a', 'z');

    for (size_t i = 0; i < n; ++i)
    {
        int len = len_dist(rng);
        keys[i].reserve(len);
        for (int j = 0; j < len; ++j)
        {
            keys[i].push_back(static_cast<char>(char_dist(rng)));
        }
    }
    return keys;
}

// ============================================================================
// Benchmark Case Enum
// ============================================================================

enum class Case
{
    BulkInsertSorted,     // Sorted incremental inserts (often uses hints)
    BulkBuildSortedRange, // Bulk build from sorted unique range (range insert / construct)
    BulkInsertRandom,     // FlatMap's worst case
    FindHit,
    FindMiss,
    Iteration,
    LowerBound,
    SingleInsert, // Pathological for flat containers
    Erase
};

static inline const char* case_name(Case c)
{
    switch (c)
    {
        case Case::BulkInsertSorted:
            return "Bulk Insert (sorted)";
        case Case::BulkBuildSortedRange:
            return "Bulk Build (sorted range)";
        case Case::BulkInsertRandom:
            return "Bulk Insert (random)";
        case Case::FindHit:
            return "Find (hit)";
        case Case::FindMiss:
            return "Find (miss)";
        case Case::Iteration:
            return "Iteration";
        case Case::LowerBound:
            return "lower_bound";
        case Case::SingleInsert:
            return "Single Insert (random)";
        case Case::Erase:
            return "Erase (25%)";
    }
    return "Unknown";
}

// ============================================================================
// Shared Inputs
// ============================================================================

struct Inputs
{
    std::vector<int64_t> sorted_keys;                      // Pre-sorted unique keys
    std::vector<std::pair<int64_t, int64_t>> sorted_pairs; // Optional: {k,k} for map bulk-build
    std::vector<int64_t> random_keys;                      // Random order for worst-case insert
    std::vector<int64_t> miss_keys;                        // Keys not in the map
    std::vector<int64_t> lookup_keys;                      // Shuffled keys for find benchmark
    std::vector<int64_t> erase_subset;                     // 25% of keys for erase benchmark
    std::vector<int64_t> bound_targets;                    // Keys for lower_bound/upper_bound

    static Inputs make(size_t N, uint64_t seed = 0xF1A7CAFEBABE01ULL, bool make_pairs = false)
    {
        Inputs in;

        in.random_keys = generate_random_keys(N, seed);
        in.sorted_keys = in.random_keys;
        std::sort(in.sorted_keys.begin(), in.sorted_keys.end());
        in.sorted_keys.erase(std::unique(in.sorted_keys.begin(), in.sorted_keys.end()), in.sorted_keys.end());

        if (make_pairs)
        {
            in.sorted_pairs.reserve(in.sorted_keys.size());
            for (int64_t k : in.sorted_keys)
            {
                in.sorted_pairs.emplace_back(k, k);
            }
        }

        in.miss_keys = generate_missing_keys(N, seed ^ 0x12345);

        in.lookup_keys = in.sorted_keys;
        std::mt19937_64 rng(seed ^ 0x9E3779B97F4A7C15ULL);
        std::shuffle(in.lookup_keys.begin(), in.lookup_keys.end(), rng);

        // 25% subset for erase
        in.erase_subset = in.lookup_keys;
        const size_t erase_count = std::max<size_t>(1, in.sorted_keys.size() / 4);
        if (in.erase_subset.size() > erase_count)
        {
            in.erase_subset.resize(erase_count);
        }

        // Targets for lower_bound (mix of existing and non-existing)
        in.bound_targets.reserve(N);
        for (size_t i = 0; i < N / 2 && i < in.sorted_keys.size(); ++i)
        {
            in.bound_targets.push_back(in.sorted_keys[i]);
        }
        for (size_t i = 0; i < N / 2 && i < in.miss_keys.size(); ++i)
        {
            in.bound_targets.push_back(in.miss_keys[i]);
        }
        std::shuffle(in.bound_targets.begin(), in.bound_targets.end(), rng);

        return in;
    }
};

// ============================================================================
// Map Adapter Interface
// ============================================================================

struct IMapAdapter
{
    virtual ~IMapAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t N) = 0;
    virtual void teardown() = 0;
    virtual void clear() = 0;
    virtual void preload(const Inputs& in) = 0;
    virtual size_t run_operation(Case c, const Inputs& in) = 0;
    virtual int64_t checksum() const = 0;
};

// ============================================================================
// std::map Adapter (Baseline)
// ============================================================================

class StdMapAdapter final : public IMapAdapter
{
    std::unique_ptr<std::map<int64_t, int64_t>> mMap;

public:
    const char* name() const override
    {
        return "std::map";
    }

    void setup(size_t) override
    {
        mMap = std::make_unique<std::map<int64_t, int64_t>>();
    }

    void teardown() override
    {
        mMap.reset();
    }
    void clear() override
    {
        mMap->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mMap->emplace(k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                mMap->insert(in.sorted_pairs.begin(), in.sorted_pairs.end());
                ops += in.sorted_pairs.size();
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mMap->emplace(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (const auto& [key, value] : *mMap)
                {
                    benchmark_sink += value;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mMap->lower_bound(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                // Single inserts at random positions
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mMap->emplace(in.random_keys[i], in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mMap->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mMap || mMap->empty())
        {
            return 0;
        }

        const auto& first = *mMap->begin();
        const auto& last = *mMap->rbegin();
        return static_cast<int64_t>(mMap->size()) ^ first.first ^ last.second;
    }
};

// ============================================================================
// Fat-P FlatMap Adapter
// ============================================================================

#if HAS_FATP_FLATMAP
class FatPFlatMapAdapter final : public IMapAdapter
{
    std::unique_ptr<fat_p::FlatMap<int64_t, int64_t>> mMap;

public:
    const char* name() const override
    {
        return "fat_p::FlatMap";
    }

    void setup(size_t N) override
    {
        mMap = std::make_unique<fat_p::FlatMap<int64_t, int64_t>>();
        mMap->reserve(N);
    }

    void teardown() override
    {
        mMap.reset();
    }
    void clear() override
    {
        mMap->clear();
    }

    void preload(const Inputs& in) override
    {
        // Use ordered_unique_range for O(n) bulk build from sorted data
        std::vector<std::pair<int64_t, int64_t>> pairs;
        pairs.reserve(in.sorted_keys.size());
        for (int64_t k : in.sorted_keys)
        {
            pairs.emplace_back(k, k);
        }
        mMap->insert(fat_p::ordered_unique_range, pairs.begin(), pairs.end());
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                // Use ordered_unique_range tag to skip sorting (apples-to-apples with Boost)
                mMap->insert(fat_p::ordered_unique_range, in.sorted_pairs.begin(), in.sorted_pairs.end());
                ops += in.sorted_pairs.size();
                break;

            case Case::BulkInsertSorted:
            {
                // Hint-based insertion for sorted input
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;
            }

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mMap->emplace(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (const auto& [key, value] : *mMap)
                {
                    benchmark_sink += value;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mMap->lower_bound(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mMap->emplace(in.random_keys[i], in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mMap->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mMap || mMap->empty())
        {
            return 0;
        }

        const auto& first = *mMap->begin();
        const auto& last = *mMap->rbegin();
        return static_cast<int64_t>(mMap->size()) ^ first.first ^ last.second;
    }
};
#endif // HAS_FATP_FLATMAP

// ============================================================================
// Boost flat_map Adapter
// ============================================================================

#if HAS_BOOST_FLAT
class BoostFlatMapAdapter final : public IMapAdapter
{
    std::unique_ptr<boost::container::flat_map<int64_t, int64_t>> mMap;

public:
    const char* name() const override
    {
        return "boost::flat_map";
    }

    void setup(size_t N) override
    {
        mMap = std::make_unique<boost::container::flat_map<int64_t, int64_t>>();
        mMap->reserve(N);
    }

    void teardown() override
    {
        mMap.reset();
    }
    void clear() override
    {
        mMap->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mMap->emplace_hint(mMap->end(), k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                mMap->insert(boost::container::ordered_unique_range, in.sorted_pairs.begin(), in.sorted_pairs.end());
                ops += in.sorted_pairs.size();
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mMap->emplace(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (const auto& [key, value] : *mMap)
                {
                    benchmark_sink += value;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mMap->lower_bound(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mMap->emplace(in.random_keys[i], in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mMap->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mMap || mMap->empty())
        {
            return 0;
        }

        const auto& first = *mMap->begin();
        const auto& last = *mMap->rbegin();
        return static_cast<int64_t>(mMap->size()) ^ first.first ^ last.second;
    }
};
#endif

// ============================================================================
// Abseil btree_map Adapter
// ============================================================================

#if HAS_ABSL_BTREE
class AbslBtreeMapAdapter final : public IMapAdapter
{
    std::unique_ptr<absl::btree_map<int64_t, int64_t>> mMap;

public:
    const char* name() const override
    {
        return "absl::btree_map";
    }

    void setup(size_t) override
    {
        mMap = std::make_unique<absl::btree_map<int64_t, int64_t>>();
    }

    void teardown() override
    {
        mMap.reset();
    }
    void clear() override
    {
        mMap->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mMap->emplace_hint(mMap->end(), k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                mMap->insert(in.sorted_pairs.begin(), in.sorted_pairs.end());
                ops += in.sorted_pairs.size();
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mMap->emplace(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (const auto& [key, value] : *mMap)
                {
                    benchmark_sink += value;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mMap->lower_bound(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mMap->emplace(in.random_keys[i], in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mMap->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mMap || mMap->empty())
        {
            return 0;
        }

        const auto& first = *mMap->begin();
        const auto& last = *mMap->rbegin();
        return static_cast<int64_t>(mMap->size()) ^ first.first ^ last.second;
    }
};
#endif

// ============================================================================
// Folly sorted_vector_map Adapter
// ============================================================================

#if HAS_FOLLY
class FollySortedVectorMapAdapter final : public IMapAdapter
{
    std::unique_ptr<folly::sorted_vector_map<int64_t, int64_t>> mMap;

public:
    const char* name() const override
    {
        return "folly::sorted_vector_map";
    }

    void setup(size_t N) override
    {
        mMap = std::make_unique<folly::sorted_vector_map<int64_t, int64_t>>();
        mMap->reserve(N);
    }

    void teardown() override
    {
        mMap.reset();
    }
    void clear() override
    {
        mMap->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mMap->emplace_hint(mMap->end(), k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mMap->emplace(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (const auto& [key, value] : *mMap)
                {
                    benchmark_sink += value;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mMap->lower_bound(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mMap->emplace(in.random_keys[i], in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mMap->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mMap || mMap->empty())
        {
            return 0;
        }

        const auto& first = *mMap->begin();
        const auto& last = *mMap->rbegin();
        return static_cast<int64_t>(mMap->size()) ^ first.first ^ last.second;
    }
};
#endif

// ============================================================================
// C++23 std::flat_map Adapter
// ============================================================================

#if HAS_STD_FLATMAP
class StdFlatMapAdapter final : public IMapAdapter
{
    std::unique_ptr<std::flat_map<int64_t, int64_t>> mMap;

public:
    const char* name() const override
    {
        return "std::flat_map (C++23)";
    }

    void setup(size_t) override
    {
        mMap = std::make_unique<std::flat_map<int64_t, int64_t>>();
    }

    void teardown() override
    {
        mMap.reset();
    }
    void clear() override
    {
        mMap->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mMap->emplace_hint(mMap->end(), k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mMap->emplace_hint(mMap->end(), k, k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mMap->emplace(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mMap->find(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (const auto& [key, value] : *mMap)
                {
                    benchmark_sink += value;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mMap->lower_bound(k);
                    if (it != mMap->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mMap->emplace(in.random_keys[i], in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mMap->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mMap || mMap->empty())
        {
            return 0;
        }

        const auto& first = *mMap->begin();
        const auto& last = *mMap->rbegin();
        return static_cast<int64_t>(mMap->size()) ^ first.first ^ last.second;
    }
};
#endif // HAS_STD_FLATMAP

// ============================================================================
// ============================================================================
//                          SET ADAPTERS
// ============================================================================
// ============================================================================

// ============================================================================
// Set Adapter Interface
// ============================================================================

struct ISetAdapter
{
    virtual ~ISetAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t N) = 0;
    virtual void teardown() = 0;
    virtual void clear() = 0;
    virtual void preload(const Inputs& in) = 0;
    virtual size_t run_operation(Case c, const Inputs& in) = 0;
    virtual int64_t checksum() const = 0;
};

// ============================================================================
// std::set Adapter (Baseline)
// ============================================================================

class StdSetAdapter final : public ISetAdapter
{
    std::unique_ptr<std::set<int64_t>> mSet;

public:
    const char* name() const override
    {
        return "std::set";
    }

    void setup(size_t) override
    {
        mSet = std::make_unique<std::set<int64_t>>();
    }

    void teardown() override
    {
        mSet.reset();
    }
    void clear() override
    {
        mSet->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mSet->insert(mSet->end(), k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                mSet->insert(in.sorted_keys.begin(), in.sorted_keys.end());
                ops += in.sorted_keys.size();
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mSet->insert(mSet->end(), k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mSet->insert(k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (int64_t val : *mSet)
                {
                    benchmark_sink += val;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mSet->lower_bound(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mSet->insert(in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mSet->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mSet || mSet->empty())
        {
            return 0;
        }

        const int64_t first = *mSet->begin();
        const int64_t last = *mSet->rbegin();
        return static_cast<int64_t>(mSet->size()) ^ first ^ last;
    }
};

// ============================================================================
// Fat-P FlatSet Adapter
// ============================================================================

#if HAS_FATP_FLATSET
class FatPFlatSetAdapter final : public ISetAdapter
{
    std::unique_ptr<fat_p::FlatSet<int64_t>> mSet;

public:
    const char* name() const override
    {
        return "fat_p::FlatSet";
    }

    void setup(size_t N) override
    {
        mSet = std::make_unique<fat_p::FlatSet<int64_t>>();
        mSet->reserve(N);
    }

    void teardown() override
    {
        mSet.reset();
    }
    void clear() override
    {
        mSet->clear();
    }

    void preload(const Inputs& in) override
    {
        // Use ordered_unique_range for O(n) bulk build from sorted data
        mSet->insert(fat_p::ordered_unique_range, in.sorted_keys.begin(), in.sorted_keys.end());
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                // Use ordered_unique_range tag to skip sorting (apples-to-apples with Boost)
                mSet->insert(fat_p::ordered_unique_range, in.sorted_keys.begin(), in.sorted_keys.end());
                ops += in.sorted_keys.size();
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mSet->insert(mSet->end(), k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mSet->insert(k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (int64_t val : *mSet)
                {
                    benchmark_sink += val;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mSet->lower_bound(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mSet->insert(in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mSet->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mSet || mSet->empty())
        {
            return 0;
        }

        const int64_t first = *mSet->begin();
        const int64_t last = *mSet->rbegin();
        return static_cast<int64_t>(mSet->size()) ^ first ^ last;
    }
};
#endif // HAS_FATP_FLATSET

// ============================================================================
// Boost flat_set Adapter
// ============================================================================

#if HAS_BOOST_FLAT
class BoostFlatSetAdapter final : public ISetAdapter
{
    std::unique_ptr<boost::container::flat_set<int64_t>> mSet;

public:
    const char* name() const override
    {
        return "boost::flat_set";
    }

    void setup(size_t N) override
    {
        mSet = std::make_unique<boost::container::flat_set<int64_t>>();
        mSet->reserve(N);
    }

    void teardown() override
    {
        mSet.reset();
    }
    void clear() override
    {
        mSet->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mSet->insert(mSet->end(), k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                mSet->insert(boost::container::ordered_unique_range, in.sorted_keys.begin(), in.sorted_keys.end());
                ops += in.sorted_keys.size();
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mSet->insert(mSet->end(), k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mSet->insert(k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (int64_t val : *mSet)
                {
                    benchmark_sink += val;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mSet->lower_bound(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mSet->insert(in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mSet->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }


    int64_t checksum() const override
    {
        if (!mSet || mSet->empty())
        {
            return 0;
        }

        const int64_t first = *mSet->begin();
        const int64_t last = *mSet->rbegin();
        return static_cast<int64_t>(mSet->size()) ^ first ^ last;
    }
};
#endif // HAS_BOOST_FLAT

// ============================================================================
// Abseil btree_set Adapter
// ============================================================================

#if HAS_ABSL_BTREE
class AbslBtreeSetAdapter final : public ISetAdapter
{
    std::unique_ptr<absl::btree_set<int64_t>> mSet;

public:
    const char* name() const override
    {
        return "absl::btree_set";
    }

    void setup(size_t) override
    {
        mSet = std::make_unique<absl::btree_set<int64_t>>();
    }

    void teardown() override
    {
        mSet.reset();
    }
    void clear() override
    {
        mSet->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mSet->insert(mSet->end(), k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                mSet->insert(in.sorted_keys.begin(), in.sorted_keys.end());
                ops += in.sorted_keys.size();
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mSet->insert(mSet->end(), k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mSet->insert(k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (int64_t val : *mSet)
                {
                    benchmark_sink += val;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mSet->lower_bound(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mSet->insert(in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mSet->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }


    int64_t checksum() const override
    {
        if (!mSet || mSet->empty())
        {
            return 0;
        }

        const int64_t first = *mSet->begin();
        const int64_t last = *mSet->rbegin();
        return static_cast<int64_t>(mSet->size()) ^ first ^ last;
    }
};
#endif // HAS_ABSL_BTREE

// ============================================================================
// C++23 std::flat_set Adapter
// ============================================================================

#if HAS_STD_FLATMAP // flat_set comes with flat_map in C++23
class StdFlatSetAdapter final : public ISetAdapter
{
    std::unique_ptr<std::flat_set<int64_t>> mSet;

public:
    const char* name() const override
    {
        return "std::flat_set (C++23)";
    }

    void setup(size_t) override
    {
        mSet = std::make_unique<std::flat_set<int64_t>>();
    }

    void teardown() override
    {
        mSet.reset();
    }
    void clear() override
    {
        mSet->clear();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.sorted_keys)
        {
            mSet->insert(mSet->end(), k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::BulkBuildSortedRange:
                for (int64_t k : in.sorted_keys)
                {
                    mSet->emplace_hint(mSet->end(), k);
                    ++ops;
                }
                break;

            case Case::BulkInsertSorted:
                for (int64_t k : in.sorted_keys)
                {
                    mSet->insert(mSet->end(), k);
                    ++ops;
                }
                break;

            case Case::BulkInsertRandom:
                for (int64_t k : in.random_keys)
                {
                    mSet->insert(k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.lookup_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = mSet->find(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::Iteration:
                for (int64_t val : *mSet)
                {
                    benchmark_sink += val;
                    ++ops;
                }
                break;

            case Case::LowerBound:
                for (int64_t k : in.bound_targets)
                {
                    auto it = mSet->lower_bound(k);
                    if (it != mSet->end())
                    {
                        benchmark_sink += *it;
                    }
                    ++ops;
                }
                break;

            case Case::SingleInsert:
                for (size_t i = 0; i < std::min<size_t>(100, in.random_keys.size()); ++i)
                {
                    mSet->insert(in.random_keys[i]);
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    mSet->erase(k);
                    ++ops;
                }
                break;
        }
        return ops;
    }

    int64_t checksum() const override
    {
        if (!mSet || mSet->empty())
        {
            return 0;
        }

        const int64_t first = *mSet->begin();
        const int64_t last = *mSet->rbegin();
        return static_cast<int64_t>(mSet->size()) ^ first ^ last;
    }
};
#endif // HAS_STD_FLATMAP

// ============================================================================
// Benchmark Result Structure
// ============================================================================

struct BenchResult
{
    std::string library;
    std::vector<double> samples;
};

// ============================================================================
// Core Operations Benchmark
// ============================================================================

void benchmark_core_operations(const std::vector<size_t>& sizes)
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 1: Core Operations\n";
    std::cout << "================================================================================\n";

    print_cpu_context("Section start");

    // Build adapter list
    std::vector<std::unique_ptr<IMapAdapter>> adapters;
    adapters.push_back(std::make_unique<StdMapAdapter>());
#if HAS_FATP_FLATMAP
    adapters.push_back(std::make_unique<FatPFlatMapAdapter>());
#endif
#if HAS_BOOST_FLAT
    adapters.push_back(std::make_unique<BoostFlatMapAdapter>());
#endif
#if HAS_ABSL_BTREE
    adapters.push_back(std::make_unique<AbslBtreeMapAdapter>());
#endif
#if HAS_FOLLY
    adapters.push_back(std::make_unique<FollySortedVectorMapAdapter>());
#endif
#if HAS_STD_FLATMAP
    adapters.push_back(std::make_unique<StdFlatMapAdapter>());
#endif

    std::vector<Case> cases = {Case::BulkBuildSortedRange,
                               Case::BulkInsertSorted,
                               Case::BulkInsertRandom,
                               Case::FindHit,
                               Case::FindMiss,
                               Case::Iteration,
                               Case::LowerBound};

    std::mt19937_64 rng(42);

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N, 0xF1A7CAFEBABE01ULL, /*make_pairs=*/true);

        for (Case c : cases)
        {
            std::cout << "\n  " << case_name(c) << ":\n";
            cooling_delay(COOLING_DELAY_CASE_MS, nullptr);

            // Collect samples for each adapter
            std::vector<BenchResult> results;
            for (auto& adapter : adapters)
            {
                results.push_back({adapter->name(), {}});
            }

            // Warmup runs
            for (size_t run = 0; run < WARMUP_RUNS(); ++run)
            {
                std::vector<size_t> order(adapters.size());
                std::iota(order.begin(), order.end(), 0);
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];
                    adapter->setup(N);

                    // Preload for find/iteration/bound/erase cases
                    bool needs_preload = (c == Case::FindHit || c == Case::FindMiss || c == Case::Iteration ||
                                          c == Case::LowerBound || c == Case::Erase);
                    if (needs_preload)
                    {
                        adapter->preload(in);
                    }

                    Timer t;
                    t.start();
                    size_t ops = adapter->run_operation(c, in);
                    (void)t.elapsed_ns();
                    (void)ops;
                    benchmark_sink += adapter->checksum();

                    adapter->teardown();
                }
            }

            // Measured runs (round-robin with randomized order)
            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                std::vector<size_t> order(adapters.size());
                std::iota(order.begin(), order.end(), 0);
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];
                    adapter->setup(N);

                    bool needs_preload = (c == Case::FindHit || c == Case::FindMiss || c == Case::Iteration ||
                                          c == Case::LowerBound || c == Case::Erase);
                    if (needs_preload)
                    {
                        adapter->preload(in);
                    }

                    Timer t;
                    t.start();
                    size_t ops = adapter->run_operation(c, in);
                    double elapsed = t.elapsed_ns();
                    benchmark_sink += adapter->checksum();

                    adapter->teardown();

                    results[idx].samples.push_back(ns_per_op(elapsed, ops));
                }
            }

            // Print results
            std::cout << std::fixed << std::setprecision(2);
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                std::cout << "    " << std::setw(24) << r.library << ": " << std::setw(8) << stats.median << " ns/op "
                          << "(+/-" << std::setw(6) << stats.stddev << ", CI:[" << stats.ci95_low << ","
                          << stats.ci95_high << "])\n";
            }
        }
    }
}

// ============================================================================
// Pathological Insert Benchmark (FlatMap's weakness)
// ============================================================================

void benchmark_pathological_insert()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 2: Pathological Random Insert (FlatMap's Weakness)\n";
    std::cout << "================================================================================\n";
    std::cout << "  This benchmark measures single random insertions into a populated container.\n";
    std::cout << "  For flat containers, each insert may require shifting O(n) elements.\n\n";

    print_cpu_context("Section start");

    std::vector<std::unique_ptr<IMapAdapter>> adapters;
    adapters.push_back(std::make_unique<StdMapAdapter>());
#if HAS_FATP_FLATMAP
    adapters.push_back(std::make_unique<FatPFlatMapAdapter>());
#endif
#if HAS_BOOST_FLAT
    adapters.push_back(std::make_unique<BoostFlatMapAdapter>());
#endif
#if HAS_ABSL_BTREE
    adapters.push_back(std::make_unique<AbslBtreeMapAdapter>());
#endif

    // Smaller sizes since random insert is expensive
    const std::vector<size_t> sizes = {1000, 5000, 10000};
    constexpr size_t INSERT_COUNT = 100;

    std::mt19937_64 rng(12345);

    for (size_t N : sizes)
    {
        std::cout << "\n--- Base size: " << N << ", inserting " << INSERT_COUNT << " missing keys ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        const Inputs base = Inputs::make(N);

        // Generate missing keys in the same value domain to distribute insertion positions.
        std::vector<int64_t> insert_keys;
        insert_keys.reserve(INSERT_COUNT);

        std::mt19937_64 insert_rng(0xABCDEF ^ static_cast<uint64_t>(N));
        std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);

        while (insert_keys.size() < INSERT_COUNT)
        {
            const int64_t k = dist(insert_rng);

            if (std::binary_search(base.sorted_keys.begin(), base.sorted_keys.end(), k))
            {
                continue;
            }

            if (std::find(insert_keys.begin(), insert_keys.end(), k) != insert_keys.end())
            {
                continue;
            }

            insert_keys.push_back(k);
        }

        Inputs insert_in = base;
        insert_in.random_keys = std::move(insert_keys);

        // Collect samples for each adapter
        std::vector<BenchResult> results;
        results.reserve(adapters.size());
        for (auto& adapter : adapters)
        {
            results.push_back({adapter->name(), {}});
        }

        auto run_one = [&](bool record_samples) {
            // Randomize order each run
            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                auto& adapter = adapters[idx];

                // Reserve enough headroom to avoid reallocations in flat containers.
                adapter->setup(base.sorted_keys.size() + INSERT_COUNT);
                adapter->preload(base);

                Timer t;
                t.start();
                size_t ops = adapter->run_operation(Case::SingleInsert, insert_in);
                double elapsed = t.elapsed_ns();
                benchmark_sink += adapter->checksum();

                adapter->teardown();

                if (record_samples)
                {
                    results[idx].samples.push_back(ns_per_op(elapsed, ops));
                }
            }
        };

        // Warmup runs
        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            run_one(false);
        }

        // Measured runs
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            run_one(true);
        }

        // Print results
        std::cout << "  Single random insert (into populated map):\n";
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            std::cout << "    " << std::setw(24) << r.library << ": " << std::setw(8) << stats.median << " ns/op "
                      << "(+/-" << std::setw(6) << stats.stddev << ", CI:[" << stats.ci95_low << "," << stats.ci95_high
                      << "])\n";
        }
    }
}


// ============================================================================
// Iteration Speed Comparison
// ============================================================================

void benchmark_iteration_speed()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 3: Iteration Speed (FlatMap's Strength)\n";
    std::cout << "================================================================================\n";
    std::cout << "  FlatMap stores elements contiguously, enabling hardware prefetching.\n";
    std::cout << "  std::map requires pointer chasing through scattered tree nodes.\n\n";

    print_cpu_context("Section start");

    std::vector<std::unique_ptr<IMapAdapter>> adapters;
    adapters.push_back(std::make_unique<StdMapAdapter>());
#if HAS_FATP_FLATMAP
    adapters.push_back(std::make_unique<FatPFlatMapAdapter>());
#endif
#if HAS_BOOST_FLAT
    adapters.push_back(std::make_unique<BoostFlatMapAdapter>());
#endif
#if HAS_ABSL_BTREE
    adapters.push_back(std::make_unique<AbslBtreeMapAdapter>());
#endif

    std::vector<size_t> sizes = {1000, 10000, 100000, 1000000};
    std::mt19937_64 rng(42);

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

        std::cout << "  Iteration (ns/element):\n";
        for (auto& adapter : adapters)
        {
            std::vector<double> samples;

            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                adapter->setup(N);
                adapter->preload(in);

                Timer t;
                t.start();
                size_t ops = adapter->run_operation(Case::Iteration, in);
                double elapsed = t.elapsed_ns();

                adapter->teardown();
                samples.push_back(ns_per_op(elapsed, ops));
            }

            auto stats = Statistics::compute(samples);
            std::cout << "    " << std::setw(24) << adapter->name() << ": " << std::setw(8) << stats.median
                      << " ns/elem "
                      << "(+/-" << stats.stddev << ")\n";
        }
    }
}

// ============================================================================
// Memory Usage Comparison (Informational)
// ============================================================================

void print_memory_comparison()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Memory Usage Comparison (Theoretical)\n";
    std::cout << "================================================================================\n";
    std::cout << "  For map<int64_t, int64_t>:\n";
    std::cout << "\n";
    std::cout << "  Container                   Per-Entry Overhead    Total for N=10000\n";
    std::cout << "  -------------------------   -------------------   -----------------\n";
    std::cout << "  std::map                    ~40 bytes (tree node) ~400 KB + 160 KB data\n";
    std::cout << "  fat_p::FlatMap              ~0 bytes              ~160 KB (data only)\n";
    std::cout << "  boost::container::flat_map  ~0 bytes              ~160 KB (data only)\n";
    std::cout << "  absl::btree_map             ~2-4 bytes (B-tree)   ~180-200 KB\n";
    std::cout << "\n";
    std::cout << "  Note: FlatMap has ~2.5x better memory efficiency than std::map.\n";
}

// ============================================================================
// FlatSet Core Operations Benchmark
// ============================================================================

void benchmark_set_operations(const std::vector<size_t>& sizes)
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 4: FlatSet Core Operations\n";
    std::cout << "================================================================================\n";

    print_cpu_context("Section start");

    // Build adapter list
    std::vector<std::unique_ptr<ISetAdapter>> adapters;
    adapters.push_back(std::make_unique<StdSetAdapter>());
#if HAS_FATP_FLATSET
    adapters.push_back(std::make_unique<FatPFlatSetAdapter>());
#endif
#if HAS_BOOST_FLAT
    adapters.push_back(std::make_unique<BoostFlatSetAdapter>());
#endif
#if HAS_ABSL_BTREE
    adapters.push_back(std::make_unique<AbslBtreeSetAdapter>());
#endif
#if HAS_STD_FLATMAP
    adapters.push_back(std::make_unique<StdFlatSetAdapter>());
#endif

    std::vector<Case> cases = {Case::BulkBuildSortedRange,
                               Case::BulkInsertSorted,
                               Case::BulkInsertRandom,
                               Case::FindHit,
                               Case::FindMiss,
                               Case::Iteration,
                               Case::LowerBound};

    std::mt19937_64 rng(42);

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

        for (Case c : cases)
        {
            std::cout << "\n  " << case_name(c) << ":\n";
            cooling_delay(COOLING_DELAY_CASE_MS, nullptr);

            // Collect samples for each adapter
            std::vector<BenchResult> results;
            for (auto& adapter : adapters)
            {
                results.push_back({adapter->name(), {}});
            }

            // Warmup runs
            for (size_t run = 0; run < WARMUP_RUNS(); ++run)
            {
                std::vector<size_t> order(adapters.size());
                std::iota(order.begin(), order.end(), 0);
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];
                    adapter->setup(N);

                    bool needs_preload = (c == Case::FindHit || c == Case::FindMiss || c == Case::Iteration ||
                                          c == Case::LowerBound || c == Case::Erase);
                    if (needs_preload)
                    {
                        adapter->preload(in);
                    }

                    Timer t;
                    t.start();
                    size_t ops = adapter->run_operation(c, in);
                    (void)t.elapsed_ns();
                    (void)ops;
                    benchmark_sink += adapter->checksum();

                    adapter->teardown();
                }
            }

            // Measured runs (round-robin with randomized order)
            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                std::vector<size_t> order(adapters.size());
                std::iota(order.begin(), order.end(), 0);
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];
                    adapter->setup(N);

                    bool needs_preload = (c == Case::FindHit || c == Case::FindMiss || c == Case::Iteration ||
                                          c == Case::LowerBound || c == Case::Erase);
                    if (needs_preload)
                    {
                        adapter->preload(in);
                    }

                    Timer t;
                    t.start();
                    size_t ops = adapter->run_operation(c, in);
                    double elapsed = t.elapsed_ns();
                    benchmark_sink += adapter->checksum();

                    adapter->teardown();

                    results[idx].samples.push_back(ns_per_op(elapsed, ops));
                }
            }

            // Print results
            std::cout << std::fixed << std::setprecision(2);
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                std::cout << "    " << std::setw(24) << r.library << ": " << std::setw(8) << stats.median << " ns/op "
                          << "(+/-" << std::setw(6) << stats.stddev << ", CI:[" << stats.ci95_low << ","
                          << stats.ci95_high << "])\n";
            }
        }
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    // Load configuration from FATP_BENCH_* environment variables
    g_config = fat_p::bench::BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity) unless disabled
    BenchmarkScope scope(!g_config.noScope);

    bool path_insert_only = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--path-insert-only")
        {
            path_insert_only = true;
        }
    }

    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "FlatMap/FlatSet";
    hdr.warmup = WARMUP_RUNS();
    hdr.measured = MEASURED_RUNS();
    hdr.seed = g_config.seed;
    
    // Competitors
#if HAS_FATP_FLATMAP
    hdr.competitors.push_back({"fat_p::FlatMap", true, "primary"});
#else
    hdr.competitors.push_back({"fat_p::FlatMap", false, "pending"});
#endif
#if HAS_FATP_FLATSET
    hdr.competitors.push_back({"fat_p::FlatSet", true, "primary"});
#else
    hdr.competitors.push_back({"fat_p::FlatSet", false, "pending"});
#endif
    hdr.competitors.push_back({"std::map", true, "baseline"});
    hdr.competitors.push_back({"std::set", true, "baseline"});
#if HAS_BOOST_FLAT
    hdr.competitors.push_back({"boost::flat_map / boost::flat_set", true, ""});
#else
    hdr.competitors.push_back({"boost::flat_map / boost::flat_set", false, "not detected"});
#endif
#if HAS_ABSL_BTREE
    hdr.competitors.push_back({"absl::btree_map / absl::btree_set", true, ""});
#else
    hdr.competitors.push_back({"absl::btree_map / absl::btree_set", false, "not detected"});
#endif
#if HAS_FOLLY
    hdr.competitors.push_back({"folly::sorted_vector_map", true, ""});
#else
    hdr.competitors.push_back({"folly::sorted_vector_map", false, "not detected"});
#endif
#if HAS_STD_FLATMAP
    hdr.competitors.push_back({"std::flat_map / std::flat_set", true, "C++23"});
#else
    hdr.competitors.push_back({"std::flat_map / std::flat_set", false, "C++23 not available"});
#endif
    
    hdr.has_extended_config = false;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = false;
    hdr.has_stabilization = !g_config.noStabilize;
    
    fat_p::bench::print_standard_header(hdr);

    if (path_insert_only)
    {
        benchmark_pathological_insert();
        return 0;
    }

    std::cout << "Expected Results:\n";
    std::cout << "  - FlatMap excels at: iteration, find, bulk insert (sorted)\n";
    std::cout << "  - FlatMap struggles at: random insert, erase (O(n) operations)\n";
    std::cout << "  - std::map has consistent O(log n) for all operations\n";
    std::cout << "  - absl::btree_map balances between tree and flat characteristics\n\n";

    // Wait for initial CPU stability (unless disabled)
    if (!g_config.noStabilize)
    {
        std::cout << "Checking initial CPU state...\n";
        print_cpu_context("Initial");
        std::cout << "Waiting for CPU to stabilize...\n";
        if (!wait_for_cpu_stable(10.0, 30, 200, true))
        {
            std::cout << "WARNING: CPU frequency still fluctuating, results may have higher variance.\n";
        }
        std::cout << "\n";
    }

    // Default sizes for core operations
    std::vector<size_t> core_sizes = {1000, 10000, 100000};

    // Run benchmarks
    benchmark_core_operations(core_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before pathological insert");
    benchmark_pathological_insert();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before iteration benchmark");
    benchmark_iteration_speed();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before set operations");
    benchmark_set_operations(core_sizes);

    print_memory_comparison();

    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
