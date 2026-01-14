// benchmark_FatPHashMap.cpp
//
// FAT-P HashMap benchmarks using unified FatPBenchmarkRunner infrastructure.
//
// Architecture: Round-robin execution with randomized order per run.
// This ensures all libraries observe the same distribution of machine states,
// eliminating drift-induced unfairness.
//
// Design Invariants:
//   1. Each measured run executes exactly one timed iteration per library.
//   2. Library execution order is randomized per run.
//   3. Setup, reserve, and teardown occur outside timed regions.
//   4. All libraries observe the same distribution of machine states.
//   5. Medians are the primary reported statistic.
//
// Fat-P Libraries (included):
//   - FastHashMap: Swiss table (SIMD), flat storage, built-in SplitMix64 mixer
//   - StableHashMap: Swiss table (SIMD), node-based, WITH ref stability
//
// Competitor Libraries: Auto-detected via __has_include (tsl, ankerl, absl, boost, llvm, folly)
//
// Build:
//   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_FatPHashMap.cpp -o bench_hm
//   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_FatPHashMap.cpp
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
//   ./bench_hm
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_hm

/*
FATP_META:
  meta_version: 1
  component:
    - FastHashMap
    - StableHashMap
  file_role: benchmark
  path: benchmarks/benchmark_FatPHashMap.cpp
  namespace: fat_p::bench
  layer: benchmarks.containers.associative
  summary: Round-robin competitor benchmarks for FastHashMap and StableHashMap.
  api_stability: candidate
  related:
    docs:
      - Documentation/Associative Containers/StableHashMap_Overview.md
      - Documentation/Associative Containers/StableHashMap_User_Manual.md
      - Documentation/Associative Containers/Companion Guide - StableHashMap.md
    tests:
      - tests/test_StableHashMap.cpp
    headers:
      - fat_p/FastHashMap.h
      - fat_p/StableHashMap.h
      - fat_p/FatPBenchmarkRunner.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 14
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
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "FastHashMap.h"
#include "FatPBenchmarkRunner.h"

#define FATP_STABLEHASHMAP_DIAGNOSTICS 1
#include "StableHashMap.h"

// Optional: Include competitor headers if available
#if __has_include("tsl/robin_map.h")
#include "tsl/robin_map.h"
#define HAS_TSL 1
#else
#define HAS_TSL 0
#endif

#if __has_include("ankerl/unordered_dense.h")
#include "ankerl/unordered_dense.h"
#define HAS_ANKERL 1
#else
#define HAS_ANKERL 0
#endif

#if __has_include("boost/unordered/unordered_flat_map.hpp")
#include "boost/unordered/unordered_flat_map.hpp"
#include "boost/unordered/unordered_node_map.hpp"
#define HAS_BOOST_FLAT 1
#else
#define HAS_BOOST_FLAT 0
#endif

// Folly requires special setup (fmt, boost, glog, etc.) - opt-in with -DUSE_FOLLY=1
#if defined(USE_FOLLY) && USE_FOLLY && __has_include("folly/container/F14Map.h")
#include "folly/container/F14Map.h"
#define HAS_FOLLY 1
#else
#define HAS_FOLLY 0
#endif

// These two are generating internal warnings
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244) // conversion from 'uint64_t' to 'unsigned int'
#pragma warning(disable : 4267) // conversion from 'size_t' to 'uint16_t'
#endif

#if __has_include("llvm/ADT/DenseMap.h")
#include "llvm/ADT/DenseMap.h"
#define HAS_LLVM 1
#else
#define HAS_LLVM 0
#endif

#if __has_include("absl/container/flat_hash_map.h")
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#define HAS_ABSL 1
#else
#define HAS_ABSL 0
#endif

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// ============================================================================
// Platform Configuration
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <intrin.h>
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
    using clock = std::chrono::steady_clock;
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
// Uses fat_p::bench::capture_cpu_frequency() from FatPBenchmarkUtils.h
// to actively wait until CPU frequency stabilizes before benchmarks.

// Forward declaration
static inline void cpu_warmup_burst(int milliseconds);

// Wait until CPU frequency stabilizes (not actively changing)
// Returns false if timeout reached while still unstable
static bool wait_for_cpu_stable(double max_variance_percent = 10.0, // Frequency must stay within 10% variance
                                double min_freq_percent = 60.0,     // Frequency must be at least 60% of base
                                int timeout_seconds = 30,           // Give up after 30s
                                int check_interval_ms = 200,        // Check every 200ms
                                bool verbose = true)
{
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);

    // First check if we have reliable frequency detection
    auto initial_info = fat_p::bench::capture_cpu_frequency();
    if (!initial_info.has_reliable_detection())
    {
        if (verbose)
        {
            std::cout << "[CPU frequency detection unavailable - using fixed cooling delay]\n";
        }
        // Fall back to a conservative fixed delay when detection unavailable
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return true; // Assume stable after fixed wait
    }

    double base_freq = initial_info.mRefFreqMHz;
    double min_required_freq = base_freq * (min_freq_percent / 100.0);

    // Initial warmup to get CPU out of idle state
    cpu_warmup_burst(100);

    // Collect recent frequency readings to detect stability
    std::vector<double> recent_readings;
    const size_t window_size = 5;  // Look at last 5 readings
    const int required_stable = 3; // Need 3 consecutive stable windows
    int stable_count = 0;

    while (std::chrono::steady_clock::now() - start < timeout)
    {
        // Keep CPU busy between measurements to prevent idle frequency drops
        cpu_warmup_burst(50);

        auto info = fat_p::bench::capture_cpu_frequency();
        recent_readings.push_back(info.mCurrentFreqMHz);

        // Keep only the last window_size readings
        if (recent_readings.size() > window_size)
        {
            recent_readings.erase(recent_readings.begin());
        }

        // Check stability once we have enough readings
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

            // Variance as percentage of average
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
                                  << std::setprecision(0) << pct_of_base
                                  << "% of base, variance: " << std::setprecision(1) << variance_pct << "%)]\n";
                    }
                    return true;
                }
            }
            else
            {
                stable_count = 0; // Reset on unstable reading
                if (verbose)
                {
                    if (!freq_floor_ok)
                    {
                        double pct_of_base = (avg_freq / base_freq) * 100.0;
                        std::cout << "[Waiting: " << static_cast<int>(avg_freq) << " MHz (" << std::fixed
                                  << std::setprecision(0) << pct_of_base << "% of base, need >" << min_freq_percent
                                  << "%)]   \r" << std::flush;
                    }
                    else
                    {
                        std::cout << "[Waiting: " << static_cast<int>(avg_freq) << " MHz (variance: " << std::fixed
                                  << std::setprecision(1) << variance_pct << "%, need <" << max_variance_percent
                                  << "%)]   \r" << std::flush;
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    }

    if (verbose)
    {
        auto final_info = fat_p::bench::capture_cpu_frequency();
        double pct_of_base = (final_info.mCurrentFreqMHz / base_freq) * 100.0;
        std::cout << "\n[WARNING: CPU frequency still unstable after " << timeout_seconds << "s - "
                  << static_cast<int>(final_info.mCurrentFreqMHz) << " MHz (" << std::fixed << std::setprecision(0)
                  << pct_of_base << "% of base)]\n";
    }
    return false;
}

// Busy work to wake up CPU after idle
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

// Smart cooling: sleep then wait for frequency to stabilize
static inline void cooling_delay(int min_sleep_ms, const char* reason = nullptr)
{
    if (reason)
    {
        std::cout << "[Cooling: " << reason << "]" << std::flush;
    }

    // Minimum sleep to let CPU cool
    std::this_thread::sleep_for(std::chrono::milliseconds(min_sleep_ms));

    // Wait until frequency stabilizes (shorter timeout for between-test waits)
    // wait_for_cpu_stable now keeps CPU busy during check, so no extra warmup needed
    bool stable = wait_for_cpu_stable(10.0, 15, 200, false);

    if (reason)
    {
        auto info = fat_p::bench::capture_cpu_frequency();
        if (info.has_reliable_detection())
        {
            std::cout << " [Ready: " << static_cast<int>(info.mCurrentFreqMHz) << " MHz";
            if (!stable)
            {
                std::cout << " (still fluctuating)";
            }
            std::cout << "]\n";
        }
        else
        {
            std::cout << " [Ready]\n";
        }
    }
}

// Delay durations (minimum sleep before checking stability)
#if defined(_WIN32) || defined(_WIN64)
static constexpr int COOLING_DELAY_SECTION_MS = 2000; // Between major benchmark sections
static constexpr int COOLING_DELAY_SIZE_MS = 1000;    // Between size transitions
static constexpr int COOLING_DELAY_CASE_MS = 300;     // Between test cases within a size
#else
static constexpr int COOLING_DELAY_SECTION_MS = 1000; // Linux generally has better thermal management
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

            // 95% Confidence Interval using Normal Distribution approximation
            // Assumes sample means are approximately normally distributed (CLT).
            // z = 1.96 corresponds to 95% CI for standard normal distribution.
            // CI = mean +/- z * (stddev / sqrt(n))
            double se = s.stddev / std::sqrt(static_cast<double>(n));
            constexpr double z = 1.96; // 95% CI critical value for normal distribution
            s.ci95_low = s.mean - z * se;
            s.ci95_high = s.mean + z * se;
        }

        return s;
    }

    void print(const char* label) const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  " << std::setw(12) << label << ": "
                  << "median=" << std::setw(8) << median << " mean=" << std::setw(8) << mean << " +/-" << std::setw(6)
                  << stddev << " CI95(mean)=[" << ci95_low << "," << ci95_high << "]"
                  << " min=" << min << " max=" << max << "\n";
    }
};

// ============================================================================
// SplitMix64 Hash (high-quality 64-bit mixer)
// Has is_avalanching marker to skip the built-in mixer in FastHashMap/StableHashMap.
// This allows fair comparison between "std::hash + built-in mixer" and
// "SplitMix64Hash alone" in benchmarks.
// ============================================================================

struct SplitMix64Hash
{
    using is_avalanching = void; // Opt-out of built-in mixer

    size_t operator()(int64_t x) const noexcept
    {
        uint64_t z = static_cast<uint64_t>(x);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return static_cast<size_t>(z ^ (z >> 31));
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

std::vector<int64_t> generate_missing_keys(size_t n, uint64_t seed = 99999)
{
    std::vector<int64_t> keys(n);
    std::mt19937_64 rng(seed);

    // Avoid -1/-2: reserved sentinel keys for llvm::DenseMap for many integer key types.
    std::uniform_int_distribution<int64_t> dist(INT64_MIN, -3);

    for (size_t i = 0; i < n; ++i)
    {
        keys[i] = dist(rng);
    }
    return keys;
}


// ============================================================================
// Benchmark Case Enum
// ============================================================================

enum class Case
{
    Insert,
    FindHit,
    FindMiss,
    Erase,
    Churn
};

static inline const char* case_name(Case c)
{
    switch (c)
    {
        case Case::Insert:
            return "Insert (amortized)";
        case Case::FindHit:
            return "Find(hit)";
        case Case::FindMiss:
            return "Find(miss)";
        case Case::Erase:
            return "Erase (25%)";
        case Case::Churn:
            return "Churn";
    }
    return "Unknown";
}

// ============================================================================
// Shared Inputs (generated once, reused across all libraries and runs)
// ============================================================================

struct Inputs
{
    std::vector<int64_t> keys;
    std::vector<int64_t> miss_keys;
    std::vector<int64_t> erase_order;
    std::vector<int64_t> erase_subset;

    // Churn script: each step removes one existing key and inserts a distinct new key.
    // This keeps table cardinality constant and models steady-state key replacement.
    std::vector<int64_t> churn_erase_keys;
    std::vector<int64_t> churn_insert_keys;

    static Inputs make(size_t N, uint64_t seed = 0xC0FFEEULL)
    {
        Inputs in;

        in.keys = generate_random_keys(N, seed);
        in.miss_keys = generate_missing_keys(N, seed ^ 0x12345);

        in.erase_order = in.keys;
        std::mt19937_64 rng(seed ^ 0x9E3779B97F4A7C15ULL);
        std::shuffle(in.erase_order.begin(), in.erase_order.end(), rng);

        in.erase_subset = in.erase_order;
        const size_t erase_count = std::max<size_t>(1, N / 4);
        if (in.erase_subset.size() > erase_count)
        {
            in.erase_subset.resize(erase_count);
        }

        // Deterministic churn script (no RNG work in timed region).
        std::vector<int64_t> current = in.keys;
        in.churn_erase_keys.resize(N);
        in.churn_insert_keys.resize(N);

        for (size_t i = 0; i < N; ++i)
        {
            const size_t idx = static_cast<size_t>(rng() % current.size());
            in.churn_erase_keys[i] = current[idx];

            // Unique negative keys avoid collisions with the initial non-negative key set.
            const int64_t new_key = -static_cast<int64_t>(i + 3);
            in.churn_insert_keys[i] = new_key;
            current[idx] = new_key;
        }

        return in;
    }
};

// ============================================================================
// Map Adapter Interface
// ============================================================================
// Each library is wrapped by a thin adapter. Adapters contain:
//   - no statistics
//   - no timing logic
//   - no policy decisions
// They are dumb, mechanical mappings.

struct IMapAdapter
{
    virtual ~IMapAdapter() = default;

    virtual const char* name() const = 0;

    // setup/teardown must be OUTSIDE timing
    virtual void setup(size_t N, const Inputs& in) = 0;
    virtual void teardown() = 0;

    // preload for cases that need a populated map (find, erase, churn)
    virtual void preload(const Inputs& in) = 0;

    // run_operation does ONLY the measured work
    // returns ops_executed for ns/op calculation
    virtual size_t run_operation(Case c, const Inputs& in) = 0;
};

// ============================================================================
// std::unordered_map Adapter
// ============================================================================

class StdUnorderedMapAdapter final : public IMapAdapter
{
    std::unique_ptr<std::unordered_map<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "std::unordered_map";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<std::unordered_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->emplace(k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->emplace(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// tsl::robin_map Adapter
// ============================================================================

#if HAS_TSL
class TslRobinMapAdapter final : public IMapAdapter
{
    std::unique_ptr<tsl::robin_map<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "tsl::robin_map";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<tsl::robin_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// ankerl::unordered_dense Adapter
// ============================================================================

#if HAS_ANKERL
class AnkerlDenseMapAdapter final : public IMapAdapter
{
    std::unique_ptr<ankerl::unordered_dense::map<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "ankerl::unordered_dense";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<ankerl::unordered_dense::map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// absl::flat_hash_map Adapter
// ============================================================================

#if HAS_ABSL
class AbslFlatHashMapAdapter final : public IMapAdapter
{
    std::unique_ptr<absl::flat_hash_map<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "absl::flat_hash_map";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<absl::flat_hash_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// absl::node_hash_map Adapter (Reference-stable)
// ============================================================================

class AbslNodeHashMapAdapter final : public IMapAdapter
{
    std::unique_ptr<absl::node_hash_map<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "absl::node_hash_map";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<absl::node_hash_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// boost::unordered_flat_map Adapter
// ============================================================================

#if HAS_BOOST_FLAT
class BoostFlatMapAdapter final : public IMapAdapter
{
    std::unique_ptr<boost::unordered_flat_map<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "boost::unordered_flat_map";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<boost::unordered_flat_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// boost::unordered_node_map Adapter (Reference-stable)
// ============================================================================

class BoostNodeMapAdapter final : public IMapAdapter
{
    std::unique_ptr<boost::unordered_node_map<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "boost::unordered_node_map";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<boost::unordered_node_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// folly::F14FastMap Adapter
// ============================================================================

#if HAS_FOLLY
class FollyF14MapAdapter final : public IMapAdapter
{
    std::unique_ptr<folly::F14FastMap<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "folly::F14FastMap";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<folly::F14FastMap<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// folly::F14NodeMap Adapter (Reference-stable)
// ============================================================================

class FollyF14NodeMapAdapter final : public IMapAdapter
{
    std::unique_ptr<folly::F14NodeMap<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "folly::F14NodeMap";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<folly::F14NodeMap<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// llvm::DenseMap Adapter
// ============================================================================

#if HAS_LLVM
class LlvmDenseMapAdapter final : public IMapAdapter
{
    std::unique_ptr<llvm::DenseMap<int64_t, int64_t>> map_;

public:
    const char* name() const override
    {
        return "llvm::DenseMap";
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<llvm::DenseMap<int64_t, int64_t>>();
        map_->reserve(static_cast<unsigned>(N));
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert({k, k});
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto it = map_->find(k);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->second;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->try_emplace(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// fat_p::FastHashMap Adapter
// ============================================================================

template <typename Hash = std::hash<int64_t>,
          typename KeyEqual = std::equal_to<int64_t>,
          typename DeletionPolicy = fat_p::BackwardShiftDeletion>
class FastHashMapAdapter final : public IMapAdapter
{
    std::string name_;
    std::unique_ptr<fat_p::FastHashMap<int64_t, int64_t, Hash, KeyEqual, DeletionPolicy>> map_;

public:
    explicit FastHashMapAdapter(const char* name)
        : name_(name)
    {
    }

    const char* name() const override
    {
        return name_.c_str();
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<fat_p::FastHashMap<int64_t, int64_t, Hash, KeyEqual, DeletionPolicy>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert(k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto* v = map_->find(k);
                    if (v)
                    {
                        benchmark_sink += *v;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto* v = map_->find(k);
                    if (v)
                    {
                        benchmark_sink += *v;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->insert(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};

// Convenience aliases for FastHashMap policies
// std::hash variants use built-in mixer; SplitMix64Hash variants use explicit mixer (no double-mix due to
// is_avalanching)
using FastHashMapBS = FastHashMapAdapter<std::hash<int64_t>, std::equal_to<int64_t>, fat_p::BackwardShiftDeletion>;
using FastHashMapTS = FastHashMapAdapter<std::hash<int64_t>, std::equal_to<int64_t>, fat_p::TombstoneDeletion>;
using FastHashMapBS_SM64 = FastHashMapAdapter<SplitMix64Hash, std::equal_to<int64_t>, fat_p::BackwardShiftDeletion>;
using FastHashMapTS_SM64 = FastHashMapAdapter<SplitMix64Hash, std::equal_to<int64_t>, fat_p::TombstoneDeletion>;

// ============================================================================
// StableHashMap Adapter (Reference-stable node-based map)
// ============================================================================

template <typename Hash = std::hash<int64_t>, typename KeyEqual = std::equal_to<int64_t>>
class StableHashMapAdapter final : public IMapAdapter
{
    std::string name_;
    std::unique_ptr<fat_p::StableHashMap<int64_t, int64_t, Hash, KeyEqual>> map_;

public:
    explicit StableHashMapAdapter(const char* name)
        : name_(name)
    {
    }

    const char* name() const override
    {
        return name_.c_str();
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<fat_p::StableHashMap<int64_t, int64_t, Hash, KeyEqual>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert(k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto* v = map_->find(k);
                    if (v)
                    {
                        benchmark_sink += *v;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto* v = map_->find(k);
                    if (v)
                    {
                        benchmark_sink += *v;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->insert(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};

// Convenience aliases for StableHashMap
// std::hash uses built-in mixer; SplitMix64Hash uses explicit mixer (no double-mix due to is_avalanching)
using StableHashMapStd = StableHashMapAdapter<std::hash<int64_t>, std::equal_to<int64_t>>;
using StableHashMapSM64 = StableHashMapAdapter<SplitMix64Hash, std::equal_to<int64_t>>;

// BlockAllocator variant for better cache locality during allocation-heavy workloads
template <typename Hash = std::hash<int64_t>, typename KeyEqual = std::equal_to<int64_t>>
class StableHashMapBlockAdapter final : public IMapAdapter
{
    std::string name_;
    std::unique_ptr<fat_p::StableHashMap<int64_t, int64_t, Hash, KeyEqual, fat_p::BlockAllocator>> map_;

public:
    explicit StableHashMapBlockAdapter(const char* name)
        : name_(name)
    {
    }

    const char* name() const override
    {
        return name_.c_str();
    }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<fat_p::StableHashMap<int64_t, int64_t, Hash, KeyEqual, fat_p::BlockAllocator>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert(k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
            case Case::Insert:
                for (int64_t k : in.keys)
                {
                    map_->insert(k, k);
                    ++ops;
                }
                break;

            case Case::FindHit:
                for (int64_t k : in.keys)
                {
                    auto* v = map_->find(k);
                    if (v)
                    {
                        benchmark_sink += *v;
                    }
                    ++ops;
                }
                break;

            case Case::FindMiss:
                for (int64_t k : in.miss_keys)
                {
                    auto* v = map_->find(k);
                    if (v)
                    {
                        benchmark_sink += *v;
                    }
                    ++ops;
                }
                break;

            case Case::Erase:
                for (int64_t k : in.erase_subset)
                {
                    map_->erase(k);
                    ++ops;
                }
                break;

            case Case::Churn:
                for (size_t i = 0; i < in.churn_erase_keys.size(); ++i)
                {
                    map_->erase(in.churn_erase_keys[i]);
                    map_->insert(in.churn_insert_keys[i], in.churn_insert_keys[i]);
                    ops += 2;
                }
                break;
        }
        return ops;
    }
};

// BlockAllocator aliases
using StableHashMapBlockStd = StableHashMapBlockAdapter<std::hash<int64_t>, std::equal_to<int64_t>>;
using StableHashMapBlockSM64 = StableHashMapBlockAdapter<SplitMix64Hash, std::equal_to<int64_t>>;

// ============================================================================
// Round-Robin Benchmark Runner (Critical Redesign)
// ============================================================================
// INVARIANT: Each run executes exactly one timed iteration per library.
// INVARIANT: Library order is randomized per run.
// INVARIANT: All libraries observe the same distribution of machine states.

struct CaseResult
{
    std::string library;
    std::vector<double> samples_ns_per_op;
    Statistics stats;
};

struct SuiteResult
{
    Case c{};
    size_t N = 0;
    std::vector<CaseResult> per_library;
};

static inline bool needs_preload(Case c)
{
    return c == Case::FindHit || c == Case::FindMiss || c == Case::Erase || c == Case::Churn;
}

SuiteResult
run_case_round_robin(Case c, size_t N, const Inputs& in, std::vector<IMapAdapter*>& adapters, uint64_t rng_seed)
{
    SuiteResult out;
    out.c = c;
    out.N = N;

    // Storage for samples by adapter pointer
    std::unordered_map<IMapAdapter*, std::vector<double>> samples;
    samples.reserve(adapters.size());

    std::mt19937_64 rng(rng_seed ^ static_cast<uint64_t>(N) ^ static_cast<uint64_t>(static_cast<int>(c) * 0x9E37));

    // --- Warmup phase (round-robin, randomized order) ---
    for (size_t r = 0; r < WARMUP_RUNS(); ++r)
    {
        std::vector<IMapAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IMapAdapter* a : order)
        {
            a->setup(N, in);
            if (needs_preload(c))
            {
                a->preload(in);
            }
            (void)a->run_operation(c, in);
            a->teardown();
        }
    }

    // --- Measured phase (round-robin, randomized order) ---
    for (size_t r = 0; r < MEASURED_RUNS(); ++r)
    {
        std::vector<IMapAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IMapAdapter* a : order)
        {
            a->setup(N, in);
            if (needs_preload(c))
            {
                a->preload(in);
            }

            Timer t;
            t.start();
            size_t ops = a->run_operation(c, in);
            double elapsed = t.elapsed_ns();

            a->teardown();

            if (ops > 0)
            {
                samples[a].push_back(ns_per_op(elapsed, ops));
            }
        }
    }

    // Materialize output in deterministic adapter list order
    for (IMapAdapter* a : adapters)
    {
        CaseResult cr;
        cr.library = a->name();
        auto it = samples.find(a);
        if (it != samples.end())
        {
            cr.samples_ns_per_op = std::move(it->second);
            cr.stats = Statistics::compute(cr.samples_ns_per_op);
        }
        out.per_library.push_back(std::move(cr));
    }

    return out;
}

// ============================================================================
// Sanity Checks (Guardrails)
// ============================================================================
// These do NOT fail the benchmark -- they provide context for interpreting results.

void sanity_check(const CaseResult& cr, size_t N)
{
    if (cr.samples_ns_per_op.empty())
    {
        return;
    }

    // At large N, node-based maps (std::unordered_map) often pay for pointer chasing and allocator locality.
    if (N >= 1'000'000 && cr.stats.median > 100.0 && cr.library == "std::unordered_map")
    {
        std::cout << "  [NOTE] " << cr.library << ": median " << cr.stats.median
                  << " ns (node-based pointer chasing / cache locality effects)\n";
    }

    if (cr.stats.stddev > cr.stats.median && cr.stats.median > 0)
    {
        std::cout << "  [NOTE] " << cr.library << ": high variance (stddev " << cr.stats.stddev << " > median "
                  << cr.stats.median << ") - system noise or memory pressure\n";
    }
}

// ============================================================================
// Output Formatting
// ============================================================================

void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

void print_case_table(const SuiteResult& sr, bool show_warnings = true)
{
    std::cout << "\n--- " << case_name(sr.c) << " ---\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(30) << "Library" << std::setw(12) << "Median" << std::setw(12) << "Mean" << std::setw(10)
              << "Stddev"
              << "  CI95\n";
    std::cout << std::string(79, '-') << "\n";

    for (const auto& cr : sr.per_library)
    {
        if (cr.samples_ns_per_op.empty())
        {
            std::cout << std::setw(30) << cr.library << "  [no samples]\n";
            continue;
        }

        std::cout << std::setw(30) << cr.library << std::setw(12) << cr.stats.median << std::setw(12) << cr.stats.mean
                  << std::setw(10) << cr.stats.stddev << "  [" << cr.stats.ci95_low << ", " << cr.stats.ci95_high
                  << "]\n";
    }

    if (show_warnings)
    {
        for (const auto& cr : sr.per_library)
        {
            sanity_check(cr, sr.N);
        }
    }
}

void print_compact_row(const std::string& name, const std::vector<SuiteResult>& results)
{
    // Assumes results are in order: Insert, FindHit, FindMiss, Erase, Churn
    std::cout << std::setw(30) << name;

    for (const auto& sr : results)
    {
        // Find this library's result
        for (const auto& cr : sr.per_library)
        {
            if (cr.library == name)
            {
                std::cout << std::setw(10) << cr.stats.median;
                break;
            }
        }
    }
    std::cout << "\n";
}

// ============================================================================
// Section 1.5: Miss Diagnostics (why is X fast on Find(miss)?)
// ============================================================================
// Goal: isolate "candidate key compares" vs "pointer hop cost".
//
// We do this by measuring:
//   - ns per miss
//   - hash calls per miss
//   - key_equal calls per miss   (proxy for "candidate compares")
//
// Additionally, we create two "H2-biased" miss key sets, based on SplitMix64 hash:
//   - low7  : hash & 0x7F
//   - high7 : (hash >> 57) & 0x7F
//
// One of these typically corresponds to Swiss-table H2 extraction (implementation-dependent).
// If StableHashMap miss time + key_equal/miss spikes on either set, that's your smoking gun.


struct MissDiagSample
{
    double mNsPerMiss = 0.0;
    double mHashCallsPerMiss = 0.0;
    double mEqCallsPerMiss = 0.0;

    bool mHasProbe = false;
    double mGroupsPerMiss = 0.0;
    double mFullSlotsPerMiss = 0.0;
    double mFullGroupsPerMiss = 0.0;
    double mTagMatchesPerMiss = 0.0;
};

struct IMissDiagAdapter
{
    virtual ~IMissDiagAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t reserveN, const Inputs& in) = 0;
    virtual void teardown() = 0;
    virtual MissDiagSample run_find_miss(const std::vector<int64_t>& miss_keys) = 0;
};

// --------------------------------------------------------------------------
// StableHashMap miss-diag (SplitMix64Hash + counting)
// --------------------------------------------------------------------------

class StableHashMapMissDiagAdapter final : public IMissDiagAdapter
{
    std::string name_;

    struct Hash
    {
        using is_avalanching = void;
        static inline uint64_t* sCalls = nullptr;

        size_t operator()(int64_t x) const noexcept
        {
            if (sCalls)
            {
                ++(*sCalls);
            }
            return SplitMix64Hash{}(x);
        }
    };

    struct Eq
    {
        static inline uint64_t* sCalls = nullptr;

        bool operator()(int64_t a, int64_t b) const noexcept
        {
            if (sCalls)
            {
                ++(*sCalls);
            }
            return a == b;
        }
    };

    std::unique_ptr<fat_p::StableHashMap<int64_t, int64_t, Hash, Eq>> map_;
    uint64_t hashCalls_ = 0;
    uint64_t eqCalls_ = 0;

public:
    explicit StableHashMapMissDiagAdapter(const char* name)
        : name_(name)
    {
    }

    const char* name() const override
    {
        return name_.c_str();
    }

    void setup(size_t reserveN, const Inputs& in) override
    {
        hashCalls_ = 0;
        eqCalls_ = 0;
        Hash::sCalls = &hashCalls_;
        Eq::sCalls = &eqCalls_;

        map_ = std::make_unique<fat_p::StableHashMap<int64_t, int64_t, Hash, Eq>>();
        map_->reserve(reserveN);

        for (int64_t k : in.keys)
        {
            map_->insert(k, k);
        }
    }

    void teardown() override
    {
        map_.reset();
    }

    MissDiagSample run_find_miss(const std::vector<int64_t>& miss_keys) override
    {
        hashCalls_ = 0;
        eqCalls_ = 0;

        Timer t;
        t.start();

        for (int64_t k : miss_keys)
        {
            auto* v = map_->find(k);
            if (v)
            {
                benchmark_sink += *v; // should never hit, but prevents DCE
            }
        }

        const double elapsed = t.elapsed_ns();
        const double iters = static_cast<double>(miss_keys.size());

        MissDiagSample s{};
        s.mNsPerMiss = (iters > 0.0) ? (elapsed / iters) : 0.0;
        s.mHashCallsPerMiss = (iters > 0.0) ? (static_cast<double>(hashCalls_) / iters) : 0.0;
        s.mEqCallsPerMiss = (iters > 0.0) ? (static_cast<double>(eqCalls_) / iters) : 0.0;

#if defined(FATP_STABLEHASHMAP_DIAGNOSTICS)
        // Second pass: collect probe counters without perturbing timed path.
        Hash::sCalls = nullptr;
        Eq::sCalls = nullptr;

        using MapT = std::remove_reference_t<decltype(*map_)>;
        typename MapT::ProbeCounters pc{};
        for (int64_t k : miss_keys)
        {
            (void)map_->diagnostic_find(k, pc);
        }

        s.mHasProbe = true;
        s.mGroupsPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mGroupsVisited) / iters) : 0.0;
        s.mFullSlotsPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mFullSlotsVisited) / iters) : 0.0;
        s.mFullGroupsPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mFullGroupsVisited) / iters) : 0.0;
        s.mTagMatchesPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mTagMatches) / iters) : 0.0;
#endif
        return s;
    }
};

class StableHashMapBlockMissDiagAdapter final : public IMissDiagAdapter
{
    std::string name_;

    struct Hash
    {
        using is_avalanching = void;
        static inline uint64_t* sCalls = nullptr;

        size_t operator()(int64_t x) const noexcept
        {
            if (sCalls)
            {
                ++(*sCalls);
            }
            return SplitMix64Hash{}(x);
        }
    };

    struct Eq
    {
        static inline uint64_t* sCalls = nullptr;

        bool operator()(int64_t a, int64_t b) const noexcept
        {
            if (sCalls)
            {
                ++(*sCalls);
            }
            return a == b;
        }
    };

    std::unique_ptr<fat_p::StableHashMap<int64_t, int64_t, Hash, Eq, fat_p::BlockAllocator>> map_;
    uint64_t hashCalls_ = 0;
    uint64_t eqCalls_ = 0;

public:
    explicit StableHashMapBlockMissDiagAdapter(const char* name)
        : name_(name)
    {
    }

    const char* name() const override
    {
        return name_.c_str();
    }

    void setup(size_t reserveN, const Inputs& in) override
    {
        hashCalls_ = 0;
        eqCalls_ = 0;
        Hash::sCalls = &hashCalls_;
        Eq::sCalls = &eqCalls_;

        map_ = std::make_unique<fat_p::StableHashMap<int64_t, int64_t, Hash, Eq, fat_p::BlockAllocator>>();
        map_->reserve(reserveN);

        for (int64_t k : in.keys)
        {
            map_->insert(k, k);
        }
    }

    void teardown() override
    {
        map_.reset();
    }

    MissDiagSample run_find_miss(const std::vector<int64_t>& miss_keys) override
    {
        hashCalls_ = 0;
        eqCalls_ = 0;

        Timer t;
        t.start();

        for (int64_t k : miss_keys)
        {
            auto* v = map_->find(k);
            if (v)
            {
                benchmark_sink += *v;
            }
        }

        const double elapsed = t.elapsed_ns();
        const double iters = static_cast<double>(miss_keys.size());

        MissDiagSample s{};
        s.mNsPerMiss = (iters > 0.0) ? (elapsed / iters) : 0.0;
        s.mHashCallsPerMiss = (iters > 0.0) ? (static_cast<double>(hashCalls_) / iters) : 0.0;
        s.mEqCallsPerMiss = (iters > 0.0) ? (static_cast<double>(eqCalls_) / iters) : 0.0;

#if defined(FATP_STABLEHASHMAP_DIAGNOSTICS)
        // Second pass: collect probe counters without perturbing timed path.
        Hash::sCalls = nullptr;
        Eq::sCalls = nullptr;

        using MapT = std::remove_reference_t<decltype(*map_)>;
        typename MapT::ProbeCounters pc{};
        for (int64_t k : miss_keys)
        {
            (void)map_->diagnostic_find(k, pc);
        }

        s.mHasProbe = true;
        s.mGroupsPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mGroupsVisited) / iters) : 0.0;
        s.mFullSlotsPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mFullSlotsVisited) / iters) : 0.0;
        s.mFullGroupsPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mFullGroupsVisited) / iters) : 0.0;
        s.mTagMatchesPerMiss = (iters > 0.0) ? (static_cast<double>(pc.mTagMatches) / iters) : 0.0;
#endif
        return s;
    }
};

// --------------------------------------------------------------------------
// boost::unordered_node_map miss-diag (SplitMix64Hash + counting)
// --------------------------------------------------------------------------

#if HAS_BOOST_FLAT
class BoostNodeMapMissDiagAdapter final : public IMissDiagAdapter
{
    std::string name_;

    struct Hash
    {
        using is_avalanching = void;
        static inline uint64_t* sCalls = nullptr;

        size_t operator()(int64_t x) const noexcept
        {
            if (sCalls)
            {
                ++(*sCalls);
            }
            return SplitMix64Hash{}(x);
        }
    };

    struct Eq
    {
        static inline uint64_t* sCalls = nullptr;

        bool operator()(int64_t a, int64_t b) const noexcept
        {
            if (sCalls)
            {
                ++(*sCalls);
            }
            return a == b;
        }
    };

    std::unique_ptr<boost::unordered_node_map<int64_t, int64_t, Hash, Eq>> map_;
    uint64_t hashCalls_ = 0;
    uint64_t eqCalls_ = 0;

public:
    explicit BoostNodeMapMissDiagAdapter(const char* name)
        : name_(name)
    {
    }

    const char* name() const override
    {
        return name_.c_str();
    }

    void setup(size_t reserveN, const Inputs& in) override
    {
        hashCalls_ = 0;
        eqCalls_ = 0;
        Hash::sCalls = &hashCalls_;
        Eq::sCalls = &eqCalls_;

        map_ = std::make_unique<boost::unordered_node_map<int64_t, int64_t, Hash, Eq>>();
        map_->reserve(reserveN);

        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    void teardown() override
    {
        map_.reset();
    }

    MissDiagSample run_find_miss(const std::vector<int64_t>& miss_keys) override
    {
        hashCalls_ = 0;
        eqCalls_ = 0;

        Timer t;
        t.start();

        for (int64_t k : miss_keys)
        {
            auto it = map_->find(k);
            if (it != map_->end())
            {
                benchmark_sink += it->second;
            }
        }

        const double elapsed = t.elapsed_ns();
        const double iters = static_cast<double>(miss_keys.size());

        MissDiagSample s{};
        s.mNsPerMiss = (iters > 0.0) ? (elapsed / iters) : 0.0;
        s.mHashCallsPerMiss = (iters > 0.0) ? (static_cast<double>(hashCalls_) / iters) : 0.0;
        s.mEqCallsPerMiss = (iters > 0.0) ? (static_cast<double>(eqCalls_) / iters) : 0.0;
        return s;
    }
};
#endif

// --------------------------------------------------------------------------
// Round-robin runner for miss diagnostics
// --------------------------------------------------------------------------

struct MissDiagResult
{
    std::string mName;
    std::vector<double> mNs;
    std::vector<double> mHashCalls;
    std::vector<double> mEqCalls;

    std::vector<double> mGroups;
    std::vector<double> mFullSlots;
    std::vector<double> mFullGroups;
    std::vector<double> mTagMatches;
};

static inline void
print_miss_diag_table(const char* title, size_t N, size_t reserveN, const std::vector<MissDiagResult>& results)
{
    std::cout << "\n--- MISS DIAGNOSTIC: " << title << " ---\n";
    std::cout << "N=" << N << " reserve=" << reserveN << "\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(34) << "Library" << std::setw(12) << "Median(ns)" << std::setw(14) << "Eq/miss"
              << std::setw(14) << "Hash/miss" << std::setw(12) << "Grp/miss" << std::setw(14) << "FullSlots/m"
              << std::setw(12) << "FullGrp/m" << std::setw(12) << "Tag/miss"
              << "\n";
    std::cout << std::string(124, '-') << "\n";

    for (const auto& r : results)
    {
        if (r.mNs.empty())
        {
            continue;
        }

        const Statistics nsS = Statistics::compute(r.mNs);
        const Statistics eqS = Statistics::compute(r.mEqCalls);
        const Statistics hS = Statistics::compute(r.mHashCalls);

        std::cout << std::setw(34) << r.mName << std::setw(12) << nsS.median << std::setw(14) << eqS.median
                  << std::setw(14) << hS.median;

        if (!r.mGroups.empty())
        {
            const Statistics gS = Statistics::compute(r.mGroups);
            const Statistics fsS = Statistics::compute(r.mFullSlots);
            const Statistics fgS = Statistics::compute(r.mFullGroups);
            const Statistics tmS = Statistics::compute(r.mTagMatches);

            std::cout << std::setw(12) << gS.median << std::setw(14) << fsS.median << std::setw(12) << fgS.median
                      << std::setw(12) << tmS.median;
        }
        else
        {
            std::cout << std::setw(12) << "-" << std::setw(14) << "-" << std::setw(12) << "-" << std::setw(12) << "-";
        }

        std::cout << "\n";
    }
}

static std::vector<MissDiagResult> run_miss_diag_round_robin(size_t N,
                                                             size_t reserveN,
                                                             const Inputs& in,
                                                             const std::vector<int64_t>& miss_keys,
                                                             std::vector<IMissDiagAdapter*>& adapters,
                                                             uint64_t rng_seed)
{
    std::unordered_map<IMissDiagAdapter*, MissDiagResult> acc;
    acc.reserve(adapters.size());

    for (IMissDiagAdapter* a : adapters)
    {
        MissDiagResult r;
        r.mName = a->name();
        r.mNs.reserve(MEASURED_RUNS());
        r.mEqCalls.reserve(MEASURED_RUNS());
        r.mHashCalls.reserve(MEASURED_RUNS());
        r.mGroups.reserve(MEASURED_RUNS());
        r.mFullSlots.reserve(MEASURED_RUNS());
        r.mFullGroups.reserve(MEASURED_RUNS());
        r.mTagMatches.reserve(MEASURED_RUNS());
        acc.emplace(a, std::move(r));
    }

    std::mt19937_64 rng(rng_seed ^ static_cast<uint64_t>(N) ^ 0xD1A6B17ULL);

    // Warmup
    for (size_t r = 0; r < WARMUP_RUNS(); ++r)
    {
        std::vector<IMissDiagAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IMissDiagAdapter* a : order)
        {
            a->setup(reserveN, in);
            (void)a->run_find_miss(miss_keys);
            a->teardown();
        }
    }

    // Measured (round-robin, randomized order per run)
    for (size_t r = 0; r < MEASURED_RUNS(); ++r)
    {
        std::vector<IMissDiagAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IMissDiagAdapter* a : order)
        {
            a->setup(reserveN, in);

            const MissDiagSample s = a->run_find_miss(miss_keys);
            a->teardown();

            auto it = acc.find(a);
            if (it != acc.end())
            {
                it->second.mNs.push_back(s.mNsPerMiss);
                it->second.mEqCalls.push_back(s.mEqCallsPerMiss);
                it->second.mHashCalls.push_back(s.mHashCallsPerMiss);

                if (s.mHasProbe)
                {
                    it->second.mGroups.push_back(s.mGroupsPerMiss);
                    it->second.mFullSlots.push_back(s.mFullSlotsPerMiss);
                    it->second.mFullGroups.push_back(s.mFullGroupsPerMiss);
                    it->second.mTagMatches.push_back(s.mTagMatchesPerMiss);
                }
            }
        }
    }

    std::vector<MissDiagResult> out;
    out.reserve(adapters.size());
    for (IMissDiagAdapter* a : adapters)
    {
        auto it = acc.find(a);
        if (it != acc.end())
        {
            out.push_back(std::move(it->second));
        }
    }
    return out;
}


void benchmark_miss_diagnostics_slim()
{
    print_header("MISS DIAGNOSTICS (Slim)");

    std::cout << "Random misses only (no H2-biased sets), fixed N=1,000,000.\n";
    std::cout << "Purpose: regression tripwire for unsuccessful lookup behavior.\n\n";

    constexpr size_t N = 1'000'000;
    const std::array<size_t, 2> reserveN = {N, N * 2};

    Inputs in = Inputs::make(N, 0xC0FFEEULL ^ N);
    std::vector<int64_t> miss_keys = generate_missing_keys(N, 0xFEEDFACEULL ^ N);

    StableHashMapMissDiagAdapter stable_sm64("StableHashMap+SM64 (counted)");
    StableHashMapBlockMissDiagAdapter stable_block_sm64("StableHashMap[Block]+SM64 (counted)");

#if HAS_BOOST_FLAT
    BoostNodeMapMissDiagAdapter boost_node_sm64("boost::unordered_node_map+SM64 (counted)");
#endif

    std::vector<IMissDiagAdapter*> adapters = {
        &stable_sm64,
        &stable_block_sm64,
#if HAS_BOOST_FLAT
        &boost_node_sm64,
#endif
    };

    print_cpu_context("MissDiag");

    bool first = true;
    for (size_t rN : reserveN)
    {
        if (!first)
        {
            cooling_delay(COOLING_DELAY_CASE_MS, "miss reserve change");
        }
        first = false;

        const uint64_t seed = 0xBADC0FFEULL ^ static_cast<uint64_t>(N) ^ (static_cast<uint64_t>(rN) << 1);
        const std::vector<MissDiagResult> results = run_miss_diag_round_robin(N, rN, in, miss_keys, adapters, seed);

        print_miss_diag_table("Random misses", N, rN, results);
    }

    std::cout << "\n";
}

// ============================================================================
// Section 1: Core Operations Benchmark (Round-Robin)
// ============================================================================

void benchmark_core_operations(const std::vector<size_t>& sizes)
{
    print_header("CORE OPERATIONS BENCHMARK (Round-Robin)");

    std::cout << "Comparing FastHashMap and StableHashMap (std::hash vs SplitMix64Hash)\n";
    std::cout << "vs std::unordered_map";
#if HAS_TSL
    std::cout << " vs tsl::robin_map";
#endif
#if HAS_ANKERL
    std::cout << " vs ankerl::unordered_dense";
#endif
#if HAS_ABSL
    std::cout << " vs absl::flat_hash_map";
#endif
#if HAS_BOOST_FLAT
    std::cout << " vs boost::unordered_flat_map";
#endif
#if HAS_FOLLY
    std::cout << " vs folly::F14FastMap";
#endif
#if HAS_LLVM
    std::cout << " vs llvm::DenseMap";
#endif
    std::cout << "\n\n";

    std::cout << "Methodology:\n";
    std::cout << "  - " << WARMUP_RUNS() << " warmup + " << MEASURED_RUNS() << " measured runs per test\n";
    std::cout << "  - Round-robin execution with randomized order per run\n";
    std::cout << "  - All libraries observe same distribution of machine states\n";
    std::cout << "  - Primary metric: median (ns/op)\n";
    std::cout << "  - FastHashMap SIMD backend: " << fat_p::FastHashMap<int, int>::simd_backend() << "\n";
    std::cout << "  - StableHashMap SIMD backend: " << fat_p::StableHashMap<int, int>::simd_backend() << "\n";
    std::cout << "  - FastHashMap policies: BackwardShift (BS), Tombstone (TS)\n";
    std::cout << "  - StableHashMap: Reference-stable (pointers valid across insert/reserve)\n\n";

    std::cout << "Cases (ns/op):\n";
    std::cout << "  Insert: insert N unique keys into empty map (after reserve)\n";
    std::cout << "  Find(hit): find N present keys\n";
    std::cout << "  Find(miss): find N absent keys\n";
    std::cout << "  Erase: erase 25% of present keys (random order)\n";
    std::cout << "  Churn: key replacement churn (erase one existing key, insert new key; size constant)\n\n";

    // Create adapters
    // FastHashMap with BackwardShift deletion (faster miss detection)
    FastHashMapBS fast_bs("FastHashMap[BS]");
    FastHashMapBS_SM64 fast_bs_sm64("FastHashMap[BS]+SplitMix64");

    // FastHashMap with Tombstone deletion (faster erase)
    FastHashMapTS fast_ts("FastHashMap[TS]");
    FastHashMapTS_SM64 fast_ts_sm64("FastHashMap[TS]+SplitMix64");

    // StableHashMap (reference-stable, node-based, SIMD-accelerated)
    StableHashMapStd stable_std("StableHashMap");
    StableHashMapSM64 stable_sm64("StableHashMap+SplitMix64");

    // StableHashMap with BlockAllocator (better cache locality for nodes)
    StableHashMapBlockSM64 stable_block_sm64("StableHashMap[Block]+SM64");

    StdUnorderedMapAdapter std_adapter;

#if HAS_TSL
    TslRobinMapAdapter tsl_adapter;
#endif
#if HAS_ANKERL
    AnkerlDenseMapAdapter ankerl_adapter;
#endif
#if HAS_ABSL
    AbslFlatHashMapAdapter absl_adapter;
    AbslNodeHashMapAdapter absl_node_adapter;
#endif
#if HAS_BOOST_FLAT
    BoostFlatMapAdapter boost_adapter;
    BoostNodeMapAdapter boost_node_adapter;
#endif
#if HAS_FOLLY
    FollyF14MapAdapter folly_adapter;
    FollyF14NodeMapAdapter folly_node_adapter;
#endif
#if HAS_LLVM
    LlvmDenseMapAdapter llvm_adapter;
#endif

    std::vector<IMapAdapter*> adapters = {
        &fast_bs,
        &fast_bs_sm64,
        &fast_ts,
        &fast_ts_sm64,
        &stable_std,
        &stable_sm64,
        &stable_block_sm64,
        &std_adapter,
#if HAS_TSL
        &tsl_adapter,
#endif
#if HAS_ANKERL
        &ankerl_adapter,
#endif
#if HAS_ABSL
        &absl_adapter,
        &absl_node_adapter,
#endif
#if HAS_BOOST_FLAT
        &boost_adapter,
        &boost_node_adapter,
#endif
#if HAS_FOLLY
        &folly_adapter,
        &folly_node_adapter,
#endif
#if HAS_LLVM
        &llvm_adapter,
#endif
    };

    const std::vector<Case> cases = {Case::Insert, Case::FindHit, Case::FindMiss, Case::Erase, Case::Churn};

    bool first_size = true;
    for (size_t N : sizes)
    {
        // Cooling delay between size transitions
        if (!first_size)
        {
            cooling_delay(COOLING_DELAY_SIZE_MS, "before next size");
        }
        first_size = false;

        print_cpu_context();
        std::cout << "N = " << N << "\n";

        Inputs in = Inputs::make(N);
        uint64_t rng_seed = 0xBADC0FFE ^ N;

        // Run all cases for this N
        std::vector<SuiteResult> case_results;
        bool first_case = true;
        for (Case c : cases)
        {
            // Short cooling delay between cases
            if (!first_case)
            {
                cooling_delay(COOLING_DELAY_CASE_MS, nullptr); // Silent delay
            }
            first_case = false;

            print_cpu_context(case_name(c));
            SuiteResult sr = run_case_round_robin(c, N, in, adapters, rng_seed);
            case_results.push_back(std::move(sr));
        }

        // Print compact table
        std::cout << std::string(79, '-') << "\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(30) << "Map" << std::setw(10) << "Insert" << std::setw(10) << "Find" << std::setw(10)
                  << "Miss" << std::setw(10) << "Erase" << std::setw(10) << "Churn" << "\n";
        std::cout << std::string(79, '-') << "\n";

        for (IMapAdapter* a : adapters)
        {
            print_compact_row(a->name(), case_results);
        }

        // Print speedup vs std::unordered_map
        struct MapTimes
        {
            double insert = 0, find = 0, miss = 0, erase = 0, churn = 0;
            bool valid() const
            {
                return insert > 0 && find > 0 && miss > 0 && erase > 0 && churn > 0;
            }
        };

        MapTimes std_times, fast_bs, fast_bs_sm64, fast_ts, fast_ts_sm64;
        MapTimes stable_std, stable_sm64, stable_block_sm64;
#if HAS_TSL
        MapTimes tsl_robin;
#endif
#if HAS_ANKERL
        MapTimes ankerl_dense;
#endif
#if HAS_ABSL
        MapTimes absl_flat, absl_node;
#endif
#if HAS_BOOST_FLAT
        MapTimes boost_flat, boost_node;
#endif
#if HAS_FOLLY
        MapTimes folly_fast, folly_node;
#endif
#if HAS_LLVM
        MapTimes llvm_dense;
#endif

        for (const auto& sr : case_results)
        {
            for (const auto& cr : sr.per_library)
            {
                MapTimes* target = nullptr;

                if (cr.library == "std::unordered_map")
                {
                    target = &std_times;
                }
                else if (cr.library == "FastHashMap[BS]")
                {
                    target = &fast_bs;
                }
                else if (cr.library == "FastHashMap[BS]+SplitMix64")
                {
                    target = &fast_bs_sm64;
                }
                else if (cr.library == "FastHashMap[TS]")
                {
                    target = &fast_ts;
                }
                else if (cr.library == "FastHashMap[TS]+SplitMix64")
                {
                    target = &fast_ts_sm64;
                }
                else if (cr.library == "StableHashMap")
                {
                    target = &stable_std;
                }
                else if (cr.library == "StableHashMap+SplitMix64")
                {
                    target = &stable_sm64;
                }
                else if (cr.library == "StableHashMap[Block]+SM64")
                {
                    target = &stable_block_sm64;
                }
#if HAS_TSL
                else if (cr.library == "tsl::robin_map")
                {
                    target = &tsl_robin;
                }
#endif
#if HAS_ANKERL
                else if (cr.library == "ankerl::unordered_dense")
                {
                    target = &ankerl_dense;
                }
#endif
#if HAS_ABSL
                else if (cr.library == "absl::flat_hash_map")
                {
                    target = &absl_flat;
                }
                else if (cr.library == "absl::node_hash_map")
                {
                    target = &absl_node;
                }
#endif
#if HAS_BOOST_FLAT
                else if (cr.library == "boost::unordered_flat_map")
                {
                    target = &boost_flat;
                }
                else if (cr.library == "boost::unordered_node_map")
                {
                    target = &boost_node;
                }
#endif
#if HAS_FOLLY
                else if (cr.library == "folly::F14FastMap")
                {
                    target = &folly_fast;
                }
                else if (cr.library == "folly::F14NodeMap")
                {
                    target = &folly_node;
                }
#endif
#if HAS_LLVM
                else if (cr.library == "llvm::DenseMap")
                {
                    target = &llvm_dense;
                }
#endif

                if (target)
                {
                    if (sr.c == Case::Insert)
                    {
                        target->insert = cr.stats.median;
                    }
                    else if (sr.c == Case::FindHit)
                    {
                        target->find = cr.stats.median;
                    }
                    else if (sr.c == Case::FindMiss)
                    {
                        target->miss = cr.stats.median;
                    }
                    else if (sr.c == Case::Erase)
                    {
                        target->erase = cr.stats.median;
                    }
                    else if (sr.c == Case::Churn)
                    {
                        target->churn = cr.stats.median;
                    }
                }
            }
        }

        auto print_speedup = [&](const char* name, const MapTimes& times)
        {
            if (times.valid())
            {
                std::cout << "    " << std::setw(30) << std::left << name << std::right << std::fixed
                          << std::setprecision(2) << std::setw(6) << (std_times.insert / times.insert) << "x insert, "
                          << std::setw(5) << (std_times.find / times.find) << "x find, " << std::setw(5)
                          << (std_times.miss / times.miss) << "x miss, " << std::setw(5)
                          << (std_times.erase / times.erase) << "x erase, " << std::setw(5)
                          << (std_times.churn / times.churn) << "x churn\n";
            }
        };

        // Collect all maps into categorized vectors for ranking
        struct MapEntry
        {
            const char* name;
            MapTimes times;
            bool is_node_based;
        };
        std::vector<MapEntry> all_maps;

        // Fat-P maps
        if (fast_bs.valid())
        {
            all_maps.push_back({"FastHashMap[BS]", fast_bs, false});
        }
        if (fast_bs_sm64.valid())
        {
            all_maps.push_back({"FastHashMap[BS]+SplitMix64", fast_bs_sm64, false});
        }
        if (fast_ts.valid())
        {
            all_maps.push_back({"FastHashMap[TS]", fast_ts, false});
        }
        if (fast_ts_sm64.valid())
        {
            all_maps.push_back({"FastHashMap[TS]+SplitMix64", fast_ts_sm64, false});
        }
        if (stable_std.valid())
        {
            all_maps.push_back({"StableHashMap", stable_std, true});
        }
        if (stable_sm64.valid())
        {
            all_maps.push_back({"StableHashMap+SplitMix64", stable_sm64, true});
        }
        if (stable_block_sm64.valid())
        {
            all_maps.push_back({"StableHashMap[Block]+SM64", stable_block_sm64, true});
        }

        // Competitor maps
#if HAS_TSL
        if (tsl_robin.valid())
        {
            all_maps.push_back({"tsl::robin_map", tsl_robin, false});
        }
#endif
#if HAS_ANKERL
        if (ankerl_dense.valid())
        {
            all_maps.push_back({"ankerl::unordered_dense", ankerl_dense, false});
        }
#endif
#if HAS_ABSL
        if (absl_flat.valid())
        {
            all_maps.push_back({"absl::flat_hash_map", absl_flat, false});
        }
        if (absl_node.valid())
        {
            all_maps.push_back({"absl::node_hash_map", absl_node, true});
        }
#endif
#if HAS_BOOST_FLAT
        if (boost_flat.valid())
        {
            all_maps.push_back({"boost::unordered_flat_map", boost_flat, false});
        }
        if (boost_node.valid())
        {
            all_maps.push_back({"boost::unordered_node_map", boost_node, true});
        }
#endif
#if HAS_FOLLY
        if (folly_fast.valid())
        {
            all_maps.push_back({"folly::F14FastMap", folly_fast, false});
        }
        if (folly_node.valid())
        {
            all_maps.push_back({"folly::F14NodeMap", folly_node, true});
        }
#endif
#if HAS_LLVM
        if (llvm_dense.valid())
        {
            all_maps.push_back({"llvm::DenseMap", llvm_dense, false});
        }
#endif

        if (std_times.valid() && !all_maps.empty())
        {
            // Separate into flat and node-based
            std::vector<MapEntry> flat_maps, node_maps;
            for (const auto& m : all_maps)
            {
                if (m.is_node_based)
                {
                    node_maps.push_back(m);
                }
                else
                {
                    flat_maps.push_back(m);
                }
            }

            auto print_top3 = [&](const char* metric, auto get_speedup)
            {
                std::vector<std::pair<double, const char*>> ranked;
                for (const auto& m : flat_maps)
                {
                    ranked.push_back({get_speedup(m.times), m.name});
                }
                std::sort(ranked.begin(),
                          ranked.end(),
                          [](auto& a, auto& b)
                          {
                              return a.first > b.first;
                          });

                std::cout << "    Top 3 " << metric << ": ";
                for (size_t i = 0; i < std::min(size_t(3), ranked.size()); ++i)
                {
                    if (i > 0)
                    {
                        std::cout << ", ";
                    }
                    std::cout << ranked[i].second << " (" << std::fixed << std::setprecision(2) << ranked[i].first
                              << "x)";
                }
                std::cout << "\n";
            };

            auto print_top3_node = [&](const char* metric, auto get_speedup)
            {
                std::vector<std::pair<double, const char*>> ranked;
                for (const auto& m : node_maps)
                {
                    ranked.push_back({get_speedup(m.times), m.name});
                }
                std::sort(ranked.begin(),
                          ranked.end(),
                          [](auto& a, auto& b)
                          {
                              return a.first > b.first;
                          });

                std::cout << "    Top 3 " << metric << ": ";
                for (size_t i = 0; i < std::min(size_t(3), ranked.size()); ++i)
                {
                    if (i > 0)
                    {
                        std::cout << ", ";
                    }
                    std::cout << ranked[i].second << " (" << std::fixed << std::setprecision(2) << ranked[i].first
                              << "x)";
                }
                std::cout << "\n";
            };

            std::cout << "\nSpeedup vs std::unordered_map:\n";

            if (!flat_maps.empty())
            {
                std::cout << "  Flat/Fast Maps:\n";
                print_top3("Insert",
                           [&](const MapTimes& t)
                           {
                               return std_times.insert / t.insert;
                           });
                print_top3("Find",
                           [&](const MapTimes& t)
                           {
                               return std_times.find / t.find;
                           });
                print_top3("Miss",
                           [&](const MapTimes& t)
                           {
                               return std_times.miss / t.miss;
                           });
                print_top3("Erase",
                           [&](const MapTimes& t)
                           {
                               return std_times.erase / t.erase;
                           });
                print_top3("Churn",
                           [&](const MapTimes& t)
                           {
                               return std_times.churn / t.churn;
                           });
            }

            if (!node_maps.empty())
            {
                std::cout << "  Node-Based Maps (reference-stable):\n";
                print_top3_node("Insert",
                                [&](const MapTimes& t)
                                {
                                    return std_times.insert / t.insert;
                                });
                print_top3_node("Find",
                                [&](const MapTimes& t)
                                {
                                    return std_times.find / t.find;
                                });
                print_top3_node("Miss",
                                [&](const MapTimes& t)
                                {
                                    return std_times.miss / t.miss;
                                });
                print_top3_node("Erase",
                                [&](const MapTimes& t)
                                {
                                    return std_times.erase / t.erase;
                                });
                print_top3_node("Churn",
                                [&](const MapTimes& t)
                                {
                                    return std_times.churn / t.churn;
                                });
            }

            // Also print full details
            std::cout << "\n  All Results:\n";
            for (const auto& m : all_maps)
            {
                print_speedup(m.name, m.times);
            }
        }

        // Sanity checks
        for (const auto& sr : case_results)
        {
            for (const auto& cr : sr.per_library)
            {
                sanity_check(cr, N);
            }
        }

        // For largest N, print detailed statistics for all Fat-P maps
        if (N == sizes.back())
        {
            const std::vector<std::string> fatp_maps = {"FastHashMap[BS]",
                                                        "FastHashMap[BS]+SplitMix64",
                                                        "FastHashMap[TS]",
                                                        "FastHashMap[TS]+SplitMix64",
                                                        "StableHashMap",
                                                        "StableHashMap+SplitMix64",
                                                        "StableHashMap[Block]+SM64"};

            for (const auto& map_name : fatp_maps)
            {
                bool has_data = false;
                for (const auto& sr : case_results)
                {
                    for (const auto& cr : sr.per_library)
                    {
                        if (cr.library == map_name)
                        {
                            has_data = true;
                            break;
                        }
                    }
                    if (has_data)
                    {
                        break;
                    }
                }

                if (has_data)
                {
                    std::cout << "\n--- Detailed Statistics for " << map_name << " at N=" << N << " ---\n";
                    for (const auto& sr : case_results)
                    {
                        for (const auto& cr : sr.per_library)
                        {
                            if (cr.library == map_name)
                            {
                                cr.stats.print(case_name(sr.c));
                            }
                        }
                    }
                }
            }
        }

        std::cout << "\n";
    }
}

// ============================================================================
// Section 2: Pathological Erase (Tombstone Degradation Test)
// ============================================================================

void benchmark_pathological_erase()
{
    print_header("PATHOLOGICAL ERASE (TOMBSTONE DEGRADATION TEST)");

    std::cout << "Tests sustained churn on a single table without reset.\n";
    std::cout << "Tombstone-based maps may degrade over time.\n";
    std::cout << "Backward-shift maps stay stable.\n";
    std::cout << "Methodology: " << WARMUP_RUNS() << " warmup + " << MEASURED_RUNS() << " measured runs\n";
    std::cout << "             Round-robin execution with randomized order\n\n";

    print_cpu_context("Starting");

    const size_t N = 100000;
    const size_t TOTAL_OPS = 5000000;

    std::cout << "N = " << N << ", Total operations = " << TOTAL_OPS << "\n\n";

    struct Script
    {
        std::vector<int64_t> erase_keys;
        std::vector<int64_t> insert_keys;
    };

    auto keys = generate_random_keys(N, 0xBADC0FFEEULL);

    auto make_script = [&](size_t iter) -> Script
    {
        Script s;
        s.erase_keys.resize(TOTAL_OPS);
        s.insert_keys.resize(TOTAL_OPS);

        std::mt19937_64 rng(12345 + iter);
        std::vector<int64_t> current = keys;

        const int64_t base = static_cast<int64_t>(iter) * static_cast<int64_t>(TOTAL_OPS);

        for (size_t op = 0; op < TOTAL_OPS; ++op)
        {
            const size_t idx = static_cast<size_t>(rng() % current.size());
            s.erase_keys[op] = current[idx];

            // Unique negative keys avoid collisions with the initial non-negative key set and
            // also avoid llvm::DenseMap integer sentinels (-1, -2).
            const int64_t new_key = -(base + static_cast<int64_t>(op) + 3);
            s.insert_keys[op] = new_key;
            current[idx] = new_key;
        }

        return s;
    };

    // Generic pathological test runner: std-like API (reserve, insert({k,v}), erase(k)).
    auto run_pathological_std_api = [&](auto& map, const Script& s) -> double
    {
        map.reserve(N * 2);
        for (const auto& k : keys)
        {
            map.insert({k, k});
        }

        Timer t;
        t.start();
        for (size_t op = 0; op < TOTAL_OPS; ++op)
        {
            map.erase(s.erase_keys[op]);
            map.insert({s.insert_keys[op], s.insert_keys[op]});
        }
        return t.elapsed_ns() / TOTAL_OPS;
    };

    // Generic pathological test runner: Fat-P API (reserve, insert(k,v), erase(k)).
    auto run_pathological_fatp_api = [&](auto& map, const Script& s) -> double
    {
        map.reserve(N * 2);
        for (const auto& k : keys)
        {
            map.insert(k, k);
        }

        Timer t;
        t.start();
        for (size_t op = 0; op < TOTAL_OPS; ++op)
        {
            map.erase(s.erase_keys[op]);
            map.insert(s.insert_keys[op], s.insert_keys[op]);
        }
        return t.elapsed_ns() / TOTAL_OPS;
    };

    // Collect samples with round-robin
    struct PathResult
    {
        std::string name;
        std::vector<double> samples;
    };
    std::vector<PathResult> results;

    results.push_back({"StableHashMap", {}});
    results.push_back({"StableHashMap+SplitMix64", {}});
    results.push_back({"StableHashMap[Block]+SM64", {}});
    results.push_back({"FastHashMap[BS]", {}});
    results.push_back({"FastHashMap[BS]+SplitMix64", {}});
    results.push_back({"FastHashMap[TS]", {}});
    results.push_back({"FastHashMap[TS]+SplitMix64", {}});
#if HAS_TSL
    results.push_back({"tsl::robin_map", {}});
#endif
#if HAS_ANKERL
    results.push_back({"ankerl::unordered_dense", {}});
#endif
#if HAS_ABSL
    results.push_back({"absl::flat_hash_map", {}});
    results.push_back({"absl::node_hash_map", {}});
#endif
#if HAS_BOOST_FLAT
    results.push_back({"boost::unordered_flat_map", {}});
    results.push_back({"boost::unordered_node_map", {}});
#endif
#if HAS_FOLLY
    results.push_back({"folly::F14FastMap", {}});
    results.push_back({"folly::F14NodeMap", {}});
#endif
#if HAS_LLVM
    results.push_back({"llvm::DenseMap", {}});
#endif
    results.push_back({"std::unordered_map", {}});

    std::mt19937_64 order_rng(0x12345678);

    // Warmup + measured runs with round-robin order
    for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
    {
        const bool is_warmup = (run < WARMUP_RUNS());

        Script script = make_script(run);

        // Randomize order
        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), order_rng);

        for (size_t idx : order)
        {
            double ns = 0;

            if (results[idx].name == "StableHashMap")
            {
                fat_p::StableHashMap<int64_t, int64_t> map;
                ns = run_pathological_fatp_api(map, script);
            }
            else if (results[idx].name == "StableHashMap+SplitMix64")
            {
                fat_p::StableHashMap<int64_t, int64_t, SplitMix64Hash> map;
                ns = run_pathological_fatp_api(map, script);
            }
            else if (results[idx].name == "StableHashMap[Block]+SM64")
            {
                fat_p::StableHashMap<int64_t, int64_t, SplitMix64Hash, std::equal_to<int64_t>, fat_p::BlockAllocator>
                    map;
                ns = run_pathological_fatp_api(map, script);
            }
            else if (results[idx].name == "FastHashMap[BS]")
            {
                fat_p::FastHashMap<int64_t,
                                   int64_t,
                                   std::hash<int64_t>,
                                   std::equal_to<int64_t>,
                                   fat_p::BackwardShiftDeletion>
                    map;
                ns = run_pathological_fatp_api(map, script);
            }
            else if (results[idx].name == "FastHashMap[BS]+SplitMix64")
            {
                fat_p::
                    FastHashMap<int64_t, int64_t, SplitMix64Hash, std::equal_to<int64_t>, fat_p::BackwardShiftDeletion>
                        map;
                ns = run_pathological_fatp_api(map, script);
            }
            else if (results[idx].name == "FastHashMap[TS]")
            {
                fat_p::
                    FastHashMap<int64_t, int64_t, std::hash<int64_t>, std::equal_to<int64_t>, fat_p::TombstoneDeletion>
                        map;
                ns = run_pathological_fatp_api(map, script);
            }
            else if (results[idx].name == "FastHashMap[TS]+SplitMix64")
            {
                fat_p::FastHashMap<int64_t, int64_t, SplitMix64Hash, std::equal_to<int64_t>, fat_p::TombstoneDeletion>
                    map;
                ns = run_pathological_fatp_api(map, script);
            }
#if HAS_TSL
            else if (results[idx].name == "tsl::robin_map")
            {
                tsl::robin_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
#endif
#if HAS_ANKERL
            else if (results[idx].name == "ankerl::unordered_dense")
            {
                ankerl::unordered_dense::map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
#endif
#if HAS_ABSL
            else if (results[idx].name == "absl::flat_hash_map")
            {
                absl::flat_hash_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
            else if (results[idx].name == "absl::node_hash_map")
            {
                absl::node_hash_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
#endif
#if HAS_BOOST_FLAT
            else if (results[idx].name == "boost::unordered_flat_map")
            {
                boost::unordered_flat_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
            else if (results[idx].name == "boost::unordered_node_map")
            {
                boost::unordered_node_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
#endif
#if HAS_FOLLY
            else if (results[idx].name == "folly::F14FastMap")
            {
                folly::F14FastMap<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
            else if (results[idx].name == "folly::F14NodeMap")
            {
                folly::F14NodeMap<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }
#endif
#if HAS_LLVM
            else if (results[idx].name == "llvm::DenseMap")
            {
                llvm::DenseMap<int64_t, int64_t> map;
                map.reserve(static_cast<unsigned>(N * 2));
                for (const auto& k : keys)
                {
                    map.insert({k, k});
                }

                Timer t;
                t.start();
                for (size_t op = 0; op < TOTAL_OPS; ++op)
                {
                    map.erase(script.erase_keys[op]);
                    map.insert({script.insert_keys[op], script.insert_keys[op]});
                }
                ns = t.elapsed_ns() / TOTAL_OPS;
            }
#endif
            else if (results[idx].name == "std::unordered_map")
            {
                std::unordered_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, script);
            }

            if (!is_warmup)
            {
                results[idx].samples.push_back(ns);
            }
        }
    }

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : results)
    {
        auto stats = Statistics::compute(r.samples);
        std::cout << std::setw(28) << r.name << ": " << std::setw(8) << stats.median << " ns/step "
                  << "(+/-" << stats.stddev << ", CI:[" << stats.ci95_low << "," << stats.ci95_high << "])\n";
    }
}
// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Load configuration from FATP_BENCH_* environment variables
    g_config = fat_p::bench::BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity) unless disabled
    BenchmarkScope scope(!g_config.noScope);

    std::cout << "================================================================================\n";
    std::cout << "  StableHashMap Comprehensive Benchmark Suite (Round-Robin Architecture)\n";
    std::cout << "================================================================================\n";
    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() << ", seed=" << g_config.seed
              << ")\n";

    std::cout << "Competitor libraries: ";
#if HAS_TSL
    std::cout << "tsl ";
#endif
#if HAS_ANKERL
    std::cout << "ankerl ";
#endif
#if HAS_ABSL
    std::cout << "absl ";
#endif
#if HAS_BOOST_FLAT
    std::cout << "boost ";
#endif
#if HAS_FOLLY
    std::cout << "folly ";
#endif
#if HAS_LLVM
    std::cout << "llvm ";
#endif
#if !HAS_TSL && !HAS_ANKERL && !HAS_ABSL && !HAS_BOOST_FLAT && !HAS_FOLLY && !HAS_LLVM
    std::cout << "(none found)";
#endif
    std::cout << "\n\n";

    // CPU frequency detection diagnostics
    fat_p::bench::print_cpu_detection_info(std::cout);
    std::cout << "\n";

    std::cout << "Design Invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run\n";
    std::cout << "  3. Setup/reserve outside timed regions (Insert is amortized)\n";
    std::cout << "  4. All libraries observe same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n";
    std::cout << "  6. Waits for CPU frequency to stabilize before each test\n\n";

    std::cout << "Cooling delays (min sleep): section=" << COOLING_DELAY_SECTION_MS << "ms, "
              << "size=" << COOLING_DELAY_SIZE_MS << "ms, "
              << "case=" << COOLING_DELAY_CASE_MS << "ms\n";
    std::cout << "Stability detection: frequency variance < 10% AND >= 60% of base (under load)\n\n";

    // Wait for initial CPU stability (unless disabled)
    if (!g_config.noStabilize)
    {
        std::cout << "Checking initial CPU state...\n";
        print_cpu_context("Initial");
        std::cout << "Waiting for CPU to stabilize before benchmarks...\n";
        if (!wait_for_cpu_stable(10.0, 30, 200, true))
        {
            std::cout << "WARNING: CPU frequency still fluctuating, results may have higher variance.\n";
        }
        std::cout << "\n";
    }

    // Default sizes
    std::vector<size_t> core_sizes = {10000, 100000, 1000000};

    // Run selected benchmarks
    benchmark_core_operations(core_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before miss diagnostics");
    benchmark_miss_diagnostics_slim();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before pathological erase");
    benchmark_pathological_erase();
    // benchmark_load_factor_sensitivity();

    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
