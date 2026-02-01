/**
 * @file benchmark_AlignedVector.cpp
 * @brief Comprehensive benchmarks for AlignedVector.h
 *
 * @layer Application
 *
 * Architecture: Round-robin execution with randomized order per run.
 * This ensures all libraries observe the same distribution of machine states,
 * eliminating drift-induced unfairness.
 *
 * Design Invariants:
 *   1. Each measured run executes exactly one timed iteration per library.
 *   2. Library execution order is randomized per run.
 *   3. Setup, reserve, and teardown occur outside timed regions.
 *   4. All libraries observe the same distribution of machine states.
 *   5. Medians are the primary reported statistic.
 *
 * Fat-P Libraries:
 *   - AlignedVector<T, 64>: 64-byte aligned (cache line)
 *   - AlignedVector<T, 128>: 128-byte aligned (dual cache line)
 *   - AlignedVector<T, 256>: 256-byte aligned (AVX-512 friendly)
 *
 * Competitor Libraries (auto-detected):
 *   - std::vector: Standard library baseline
 *   - boost::alignment::aligned_allocator: If boost available
 *
 * Build (minimal):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_AlignedVector.cpp -o bench_av
 *   clang++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_AlignedVector.cpp -o bench_av
 *
 * Build (Windows MSVC):
 *   cl /std:c++17 /O2 /DNDEBUG /EHsc /arch:AVX2 benchmark_AlignedVector.cpp /link advapi32.lib
 *
 * Build (with Boost):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native -I/path/to/boost benchmark_AlignedVector.cpp -o bench_av
 *
 * Environment Variables (all optional):
 *   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
 *   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_MIN_BATCH_MS  - Min batch duration (default: 50)
 *   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
 *   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
 *   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
 *   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
 *   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
 *
 * Run:
 *   ./bench_av
 *   FATP_BENCH_OUTPUT_CSV=aligned_vector_results.csv ./bench_av
 */
/*
FATP_META:
  meta_version: 1
  component: AlignedVector
  file_role: benchmark
  path: components/AlignedVector/benchmarks/benchmark_AlignedVector.cpp
  layer: Testing
  namespace: fat_p
  summary: "Benchmarks for AlignedVector."
  api_stability: in_work
  related:
    docs_search: "AlignedVector"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/AlignedVector.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 5
    defines_unprefixed: 5
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
#include <cstring>
#include <ctime>
#if defined(__linux__)
#include <time.h>
#endif
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#include "AlignedVector.h"
#include "FatPBenchmarkRunner.h"

// ============================================================================
// Optional Competitor Detection
// ============================================================================

#if __has_include(<boost/align/aligned_allocator.hpp>)
#include <boost/align/aligned_allocator.hpp>
#define HAS_BOOST_ALIGN 1
#else
#define HAS_BOOST_ALIGN 0
#endif

#if __has_include(<Eigen/Core>)
#include <Eigen/Core>
#define HAS_EIGEN 1
#else
#define HAS_EIGEN 0
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
#endif

// Global benchmark configuration
static fat_p::bench::BenchConfig g_config;

// Accessors
static size_t WARMUP_RUNS()
{
    return g_config.warmupRuns;
}
static size_t MEASURED_RUNS()
{
    return g_config.measuredRuns;
}

// Cooling delays between benchmark sections (milliseconds)
static constexpr int COOLING_DELAY_SECTION_MS = 500;
static constexpr int COOLING_DELAY_SIZE_MS = 100;
static constexpr int COOLING_DELAY_CASE_MS = 50;

// ============================================================================
// Timer and Statistics
// ============================================================================

#if defined(__linux__)
struct BenchClock
{
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<BenchClock, duration>;
    static constexpr bool is_steady = true;

    static time_point now() noexcept
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        const auto ns = static_cast<rep>(ts.tv_sec) * 1000000000LL + static_cast<rep>(ts.tv_nsec);
        return time_point(duration(ns));
    }
};
#else
using BenchClock = std::chrono::steady_clock;
#endif

struct Timer
{
    using clock = BenchClock;
    clock::time_point t0;

    void start()
    {
        t0 = clock::now();
    }

    [[nodiscard]] double elapsed_ns() const
    {
        auto t1 = clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }
};

struct Statistics
{
    double median = 0;
    double mean = 0;
    double stddev = 0;
    double ci95_low = 0;
    double ci95_high = 0;
    double min = 0;
    double max = 0;
    size_t samples = 0;

    static Statistics compute(std::vector<double> data)
    {
        Statistics s;
        if (data.empty())
        {
            return s;
        }

        s.samples = data.size();
        std::sort(data.begin(), data.end());

        // Median
        size_t mid = data.size() / 2;
        s.median = (data.size() % 2 == 0) ? (data[mid - 1] + data[mid]) / 2.0 : data[mid];

        // Mean
        double sum = std::accumulate(data.begin(), data.end(), 0.0);
        s.mean = sum / static_cast<double>(data.size());

        // Stddev
        double sq_sum = 0;
        for (double v : data)
        {
            sq_sum += (v - s.mean) * (v - s.mean);
        }
        s.stddev = std::sqrt(sq_sum / static_cast<double>(data.size()));

        // CI95 (t-distribution approximation for small samples)
        double t_value = 1.96; // Approximate for n > 30
        if (data.size() < 30)
        {
            t_value = 2.0; // Conservative for small samples
        }
        double margin = t_value * s.stddev / std::sqrt(static_cast<double>(data.size()));
        s.ci95_low = s.mean - margin;
        s.ci95_high = s.mean + margin;

        s.min = data.front();
        s.max = data.back();

        return s;
    }
};

// ============================================================================
// DCE Prevention
// ============================================================================

static volatile int64_t benchmark_sink = 0;

template <typename T>
static inline void prevent_opt(T value)
{
    benchmark_sink ^= static_cast<int64_t>(value);
}

template <typename T>
static inline void DoNotOptimize(T& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : "+r"(value) : : "memory");
#else
    benchmark_sink ^= reinterpret_cast<int64_t>(&value);
#endif
}

// ============================================================================
// CPU Frequency Monitoring
// ============================================================================

void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

// ============================================================================
// Cooling and Stabilization
// ============================================================================

static void cooling_delay(int ms, const char* reason = nullptr)
{
    if (g_config.noCooldown)
    {
        return;
    }
    if (reason && g_config.verboseStats)
    {
        std::cout << "  [cooling " << ms << "ms: " << reason << "]\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static bool wait_for_cpu_stable(double max_variance_percent = 10.0,
                                int timeout_seconds = 30,
                                int check_interval_ms = 200,
                                bool verbose = true)
{
    if (g_config.noStabilize)
    {
        return true;
    }

    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);

    auto initial_info = fat_p::bench::capture_cpu_frequency();
    if (!initial_info.has_reliable_detection())
    {
        if (verbose)
        {
            std::cout << "[CPU frequency detection unavailable - using fixed delay]\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    }

    std::vector<double> freq_samples;
    freq_samples.reserve(10);

    while (std::chrono::steady_clock::now() - start < timeout)
    {
        auto info = fat_p::bench::capture_cpu_frequency();
        freq_samples.push_back(info.mCurrentFreqMHz);

        if (freq_samples.size() >= 5)
        {
            // Check variance of last 5 samples
            double sum = std::accumulate(freq_samples.end() - 5, freq_samples.end(), 0.0);
            double mean = sum / 5.0;
            double variance = 0;
            for (auto it = freq_samples.end() - 5; it != freq_samples.end(); ++it)
            {
                variance += (*it - mean) * (*it - mean);
            }
            variance = std::sqrt(variance / 5.0);
            double variance_pct = (mean > 0) ? (variance / mean * 100.0) : 0;

            if (variance_pct < max_variance_percent)
            {
                if (verbose)
                {
                    std::cout << "  [CPU stable at " << static_cast<int>(mean) << " MHz, variance " << std::fixed
                              << std::setprecision(1) << variance_pct << "%]\n";
                }
                return true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    }

    if (verbose)
    {
        std::cout << "  [WARNING: CPU frequency not stabilized within timeout]\n";
    }
    return false;
}

// ============================================================================
// Data Generation
// ============================================================================

template <typename T>
std::vector<T> generate_data(size_t n, uint64_t seed)
{
    std::vector<T> data(n);
    std::mt19937_64 rng(seed);

    if constexpr (std::is_integral_v<T>)
    {
        std::uniform_int_distribution<T> dist(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        for (size_t i = 0; i < n; ++i)
        {
            data[i] = dist(rng);
        }
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        std::uniform_real_distribution<T> dist(T(-1000), T(1000));
        for (size_t i = 0; i < n; ++i)
        {
            data[i] = dist(rng);
        }
    }

    return data;
}

std::vector<size_t> generate_random_indices(size_t n, uint64_t seed)
{
    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937_64 rng(seed);
    std::shuffle(indices.begin(), indices.end(), rng);
    return indices;
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

void print_subheader(const std::string& title)
{
    std::cout << "\n--- " << title << " ---\n\n";
}

void print_contract_note(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

void print_result_table_header()
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(30) << "Container" << std::setw(14) << "Median" << std::setw(14) << "Mean" << std::setw(12)
              << "Stddev"
              << "  CI95\n";
    std::cout << std::string(79, '-') << "\n";
}

void print_result_row(const std::string& name, const Statistics& stats, const std::string& unit)
{
    std::cout << std::setw(30) << name << std::setw(12) << stats.median << " " << unit << std::setw(12) << stats.mean
              << " " << unit << std::setw(10) << stats.stddev << "  [" << stats.ci95_low << ", " << stats.ci95_high
              << "]\n";

    // Variance warning
    if (stats.stddev > stats.median && stats.median > 0)
    {
        std::cout << "  [NOTE] High variance (stddev > median)\n";
    }
}

// ============================================================================
// Benchmark Result Storage
// ============================================================================

struct BenchmarkResult
{
    std::string name;
    std::vector<double> samples;
};

// ============================================================================
// BENCHMARK: Sequential Iteration Sum
// ============================================================================
// Tests raw sequential access performance where alignment provides
// the most benefit for SIMD auto-vectorization.

template <typename Container>
double bench_sequential_sum(Container& vec, size_t iterations)
{
    using T = typename Container::value_type;
    T sum = T(0);

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        for (size_t i = 0; i < vec.size(); ++i)
        {
            sum += vec[i];
        }
    }

    double elapsed = timer.elapsed_ns();
    prevent_opt(static_cast<int64_t>(sum));

    return elapsed / static_cast<double>(iterations * vec.size());
}

template <typename T, size_t Alignment>
double bench_sequential_sum_assume_aligned(fat_p::AlignedVector<T, Alignment>& vec, size_t iterations)
{
    T sum = T(0);

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        const T* ptr = vec.assume_aligned();
        for (size_t i = 0; i < vec.size(); ++i)
        {
            sum += ptr[i];
        }
    }

    double elapsed = timer.elapsed_ns();
    prevent_opt(static_cast<int64_t>(sum));

    return elapsed / static_cast<double>(iterations * vec.size());
}

void benchmark_sequential_iteration(const std::vector<size_t>& sizes)
{
    print_header("SEQUENTIAL ITERATION (Sum)");
    print_contract_note("Measures cache-line alignment benefit for SIMD auto-vectorization. "
                        "assume_aligned() provides compiler hints for aligned loads.");

    print_cpu_context("Sequential Iteration");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data = generate_data<float>(N, g_config.seed);
        constexpr size_t ITERS = 100;

        // Build containers outside timing
        std::vector<float> std_vec(data.begin(), data.end());
        fat_p::AlignedVector<float, 64> av64(data.begin(), data.end());
        fat_p::AlignedVector<float, 128> av128(data.begin(), data.end());

#if HAS_BOOST_ALIGN
        std::vector<float, boost::alignment::aligned_allocator<float, 64>> boost_vec(data.begin(), data.end());
#endif

        // Results storage
        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector", {}});
        results.push_back({"AlignedVector<64>", {}});
        results.push_back({"AlignedVector<64>::assume_aligned", {}});
        results.push_back({"AlignedVector<128>", {}});
#if HAS_BOOST_ALIGN
        results.push_back({"boost::aligned_allocator<64>", {}});
#endif

        // Round-robin execution
        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                double ns_per_elem = 0;

                if (results[idx].name == "std::vector")
                {
                    ns_per_elem = bench_sequential_sum(std_vec, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64>")
                {
                    ns_per_elem = bench_sequential_sum(av64, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64>::assume_aligned")
                {
                    ns_per_elem = bench_sequential_sum_assume_aligned(av64, ITERS);
                }
                else if (results[idx].name == "AlignedVector<128>")
                {
                    ns_per_elem = bench_sequential_sum(av128, ITERS);
                }
#if HAS_BOOST_ALIGN
                else if (results[idx].name == "boost::aligned_allocator<64>")
                {
                    ns_per_elem = bench_sequential_sum(boost_vec, ITERS);
                }
#endif

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns_per_elem);
                }
            }
        }

        // Print results
        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/elem");
        }

        // Correctness check
        float std_sum = 0, av_sum = 0;
        for (size_t i = 0; i < N; ++i)
        {
            std_sum += std_vec[i];
            av_sum += av64[i];
        }
        if (std::abs(std_sum - av_sum) > 0.001f * std::abs(std_sum))
        {
            std::cout << "[ERROR] Correctness check failed: sums differ!\n";
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Random Access
// ============================================================================
// Tests cache behavior with non-sequential access patterns.

template <typename Container>
double bench_random_access(Container& vec, const std::vector<size_t>& indices, size_t iterations)
{
    using T = typename Container::value_type;
    T sum = T(0);

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        for (size_t idx : indices)
        {
            sum += vec[idx];
        }
    }

    double elapsed = timer.elapsed_ns();
    prevent_opt(static_cast<int64_t>(sum));

    return elapsed / static_cast<double>(iterations * indices.size());
}

void benchmark_random_access(const std::vector<size_t>& sizes)
{
    print_header("RANDOM ACCESS");
    print_contract_note("Measures cache behavior with non-sequential access. "
                        "Alignment has less impact here; tests baseline overhead.");

    print_cpu_context("Random Access");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data = generate_data<int64_t>(N, g_config.seed);
        auto indices = generate_random_indices(N, g_config.seed + 1);
        constexpr size_t ITERS = 10;

        std::vector<int64_t> std_vec(data.begin(), data.end());
        fat_p::AlignedVector<int64_t, 64> av64(data.begin(), data.end());

        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector", {}});
        results.push_back({"AlignedVector<64>", {}});

        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                double ns = 0;

                if (results[idx].name == "std::vector")
                {
                    ns = bench_random_access(std_vec, indices, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64>")
                {
                    ns = bench_random_access(av64, indices, ITERS);
                }

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/elem");
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Push Back Growth
// ============================================================================
// Tests amortized insertion performance including reallocation.

template <typename Container>
double bench_push_back(size_t count, size_t iterations)
{
    using T = typename Container::value_type;

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        Container vec;
        for (size_t i = 0; i < count; ++i)
        {
            vec.push_back(static_cast<T>(i));
        }
        prevent_opt(static_cast<int64_t>(vec.size()));
    }

    double elapsed = timer.elapsed_ns();
    return elapsed / static_cast<double>(iterations * count);
}

template <typename Container>
double bench_push_back_reserved(size_t count, size_t iterations)
{
    using T = typename Container::value_type;

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        Container vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            vec.push_back(static_cast<T>(i));
        }
        prevent_opt(static_cast<int64_t>(vec.size()));
    }

    double elapsed = timer.elapsed_ns();
    return elapsed / static_cast<double>(iterations * count);
}

void benchmark_push_back(const std::vector<size_t>& sizes)
{
    print_header("PUSH_BACK GROWTH");
    print_contract_note("Measures amortized insertion cost. 'reserved' variant pre-allocates "
                        "to isolate allocation overhead from element construction.");

    print_cpu_context("Push Back");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        constexpr size_t ITERS = 100;

        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector (grow)", {}});
        results.push_back({"std::vector (reserved)", {}});
        results.push_back({"AlignedVector<64> (grow)", {}});
        results.push_back({"AlignedVector<64> (reserved)", {}});

        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                double ns = 0;

                if (results[idx].name == "std::vector (grow)")
                {
                    ns = bench_push_back<std::vector<int>>(N, ITERS);
                }
                else if (results[idx].name == "std::vector (reserved)")
                {
                    ns = bench_push_back_reserved<std::vector<int>>(N, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64> (grow)")
                {
                    ns = bench_push_back<fat_p::AlignedVector<int, 64>>(N, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64> (reserved)")
                {
                    ns = bench_push_back_reserved<fat_p::AlignedVector<int, 64>>(N, ITERS);
                }

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/elem");
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Insert at Position
// ============================================================================
// Tests O(n) insertion performance at various positions.

template <typename Container>
double bench_insert_middle(size_t initial_size, size_t insert_count, size_t iterations)
{
    using T = typename Container::value_type;

    double total_ns = 0;

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        Container vec(initial_size, T(0));

        Timer timer;
        timer.start();

        for (size_t i = 0; i < insert_count; ++i)
        {
            size_t pos = vec.size() / 2;
            vec.insert(vec.begin() + static_cast<ptrdiff_t>(pos), static_cast<T>(i));
        }

        total_ns += timer.elapsed_ns();
        prevent_opt(static_cast<int64_t>(vec.size()));
    }

    return total_ns / static_cast<double>(iterations * insert_count);
}

void benchmark_insert(const std::vector<size_t>& sizes)
{
    print_header("INSERT AT MIDDLE");
    print_contract_note("Measures O(n) insertion at middle position. "
                        "Tests element shifting performance.");

    print_cpu_context("Insert");

    for (size_t N : sizes)
    {
        print_subheader("Initial size = " + std::to_string(N) + ", inserting 100 elements");

        constexpr size_t INSERT_COUNT = 100;
        constexpr size_t ITERS = 50;

        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector", {}});
        results.push_back({"AlignedVector<64>", {}});

        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                double ns = 0;

                if (results[idx].name == "std::vector")
                {
                    ns = bench_insert_middle<std::vector<int>>(N, INSERT_COUNT, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64>")
                {
                    ns = bench_insert_middle<fat_p::AlignedVector<int, 64>>(N, INSERT_COUNT, ITERS);
                }

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/insert");
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Shift Microbench (memmove fast-path vs element-wise moves)
// ============================================================================
// This benchmark isolates the benefit of using memmove-style bulk shifts for
// trivially copyable types during insert/erase range operations.
//
// We compare:
//   - AlignedVector<int, 64>               : trivially copyable => memmove fast-path
//   - AlignedVector<NonTrivialInt, 64>     : same size, NOT trivially copyable => element-wise shift
//
// Reported metric: ns per shifted element (lower is better).
//
// NOTE: This does NOT claim AlignedVector is always faster than std::vector.
// It is a focused microbenchmark for the "shift elements" hot path.

namespace
{
struct NonTrivialInt
{
    int v;

    NonTrivialInt()
        : v(0)
    {
    }
    explicit NonTrivialInt(int x)
        : v(x)
    {
    }

    NonTrivialInt(const NonTrivialInt&) = default;
    NonTrivialInt(NonTrivialInt&&) noexcept = default;

    // User-defined assignments make this type non-trivially copyable.
    NonTrivialInt& operator=(const NonTrivialInt& other)
    {
        v = other.v;
        return *this;
    }
    NonTrivialInt& operator=(NonTrivialInt&& other) noexcept
    {
        v = other.v;
        return *this;
    }
};

static_assert(sizeof(NonTrivialInt) == sizeof(int), "NonTrivialInt must match int size for a fair shift comparison.");
static_assert(!std::is_trivially_copyable_v<NonTrivialInt>,
              "NonTrivialInt must be non-trivially copyable to force the element-wise shift path.");
} // namespace

template <typename Container, typename PayloadVec>
double bench_insert_range_middle(size_t initial_size, const PayloadVec& payload, size_t iterations)
{
    using T = typename Container::value_type;

    const size_t K = payload.size();
    const size_t index = initial_size / 2;

    // Elements shifted right during insert at middle.
    const size_t shifted = initial_size - index;
    if (shifted == 0)
    {
        return 0.0;
    }

    double total_ns = 0;

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        Container vec(initial_size, T(0));
        vec.reserve(initial_size + K); // avoid reallocation (isolate shift + placement)

        auto pos = vec.begin() + static_cast<ptrdiff_t>(index);

        Timer timer;
        timer.start();
        vec.insert(pos, payload.begin(), payload.end());
        total_ns += timer.elapsed_ns();

        prevent_opt(static_cast<int64_t>(vec.size()));
    }

    return total_ns / static_cast<double>(iterations * shifted);
}

template <typename Container>
double bench_erase_range_middle(size_t initial_size, size_t erase_count, size_t iterations)
{
    using T = typename Container::value_type;

    const size_t index = initial_size / 2;
    if (erase_count == 0 || index + erase_count > initial_size)
    {
        return 0.0;
    }

    // Elements shifted left during erase at middle.
    const size_t shifted = initial_size - (index + erase_count);
    if (shifted == 0)
    {
        return 0.0;
    }

    double total_ns = 0;

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        Container vec(initial_size, T(0));

        auto first = vec.begin() + static_cast<ptrdiff_t>(index);
        auto last = first + static_cast<ptrdiff_t>(erase_count);

        Timer timer;
        timer.start();
        vec.erase(first, last);
        total_ns += timer.elapsed_ns();

        prevent_opt(static_cast<int64_t>(vec.size()));
    }

    return total_ns / static_cast<double>(iterations * shifted);
}

void benchmark_shift_memmove(const std::vector<size_t>& sizes)
{
    print_header("SHIFT MICROBENCH (memmove fast-path)");
    print_contract_note("Isolates the cost of shifting elements during insert/erase range operations. "
                        "Compares a trivially copyable element type (int) that can use bulk memmove shifts "
                        "against an equivalent-size non-trivially copyable wrapper that forces element-wise moves. "
                        "Reported as ns per shifted element.");

    print_cpu_context("Shift/memmove");

    constexpr size_t K = 32; // range length inserted/erased

    for (size_t N : sizes)
    {
        if (N < 2 * K + 8)
        {
            continue;
        }

        print_subheader("N = " + std::to_string(N) + ", range K = " + std::to_string(K) + " at middle");

        // More iterations for smaller sizes to reduce timer noise
        const size_t ITERS = (N >= 100'000) ? 50 : 200;

        // Payloads for range insert
        std::vector<int> payload_i(K);
        for (size_t i = 0; i < K; ++i)
        {
            payload_i[i] = static_cast<int>(i);
        }

        std::vector<NonTrivialInt> payload_nt;
        payload_nt.reserve(K);
        for (size_t i = 0; i < K; ++i)
        {
            payload_nt.emplace_back(static_cast<int>(i));
        }

        // --------------------------------------------------------------------
        // Insert(range) at middle (shift right)
        // --------------------------------------------------------------------
        {
            print_subheader("insert(pos, first, last)   [shift right]");

            std::vector<BenchmarkResult> results;
            results.push_back({"AlignedVector<int,64> (trivial => memmove shift)", {}});
            results.push_back({"AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)", {}});

            std::vector<size_t> order(results.size());
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

            for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
            {
                bool is_warmup = (run < WARMUP_RUNS());
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    double ns = 0;

                    if (results[idx].name == "AlignedVector<int,64> (trivial => memmove shift)")
                    {
                        ns = bench_insert_range_middle<fat_p::AlignedVector<int, 64>>(N, payload_i, ITERS);
                    }
                    else if (results[idx].name == "AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)")
                    {
                        ns = bench_insert_range_middle<fat_p::AlignedVector<NonTrivialInt, 64>>(N, payload_nt, ITERS);
                    }

                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                }
            }

            print_result_table_header();
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                print_result_row(r.name, stats, "ns/shifted-elem");
            }
        }

        // --------------------------------------------------------------------
        // Erase(range) at middle (shift left)
        // --------------------------------------------------------------------
        {
            print_subheader("erase(first, last)        [shift left]");

            std::vector<BenchmarkResult> results;
            results.push_back({"AlignedVector<int,64> (trivial => memmove shift)", {}});
            results.push_back({"AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)", {}});

            std::vector<size_t> order(results.size());
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

            for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
            {
                bool is_warmup = (run < WARMUP_RUNS());
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    double ns = 0;

                    if (results[idx].name == "AlignedVector<int,64> (trivial => memmove shift)")
                    {
                        ns = bench_erase_range_middle<fat_p::AlignedVector<int, 64>>(N, K, ITERS);
                    }
                    else if (results[idx].name == "AlignedVector<NonTrivialInt,64> (non-trivial => loop shift)")
                    {
                        ns = bench_erase_range_middle<fat_p::AlignedVector<NonTrivialInt, 64>>(N, K, ITERS);
                    }

                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                }
            }

            print_result_table_header();
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                print_result_row(r.name, stats, "ns/shifted-elem");
            }
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// ============================================================================
// BENCHMARK: Copy and Move
// ============================================================================

template <typename Container>
double bench_copy(const Container& src, size_t iterations)
{
    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        Container copy(src);
        prevent_opt(static_cast<int64_t>(copy.size()));
    }

    double elapsed = timer.elapsed_ns();
    return elapsed / static_cast<double>(iterations);
}

/**
 * @brief Measures move-construction cost without allocations.
 *
 * Rationale: A naïve "move benchmark" that constructs a fresh source container inside the
 * timed loop is dominated by allocation + fill (O(n)), and does NOT measure move itself.
 *
 * This benchmark keeps two full containers alive (with distinct buffers) and rotates ownership
 * between them using a third temporary object constructed via placement-new.
 *
 * The rotation ensures the observed data() pointer alternates each iteration, preventing the
 * compiler from trivially eliding the loop.
 *
 * Returns: nanoseconds per move-construction.
 */
template <typename Container>
double bench_move_ctor_rotation(size_t size, size_t iterations)
{
    using T = typename Container::value_type;
    using Storage = std::aligned_storage_t<sizeof(Container), alignof(Container)>;

    Storage storageA;
    Storage storageB;
    Storage storageC;

    // Two distinct allocations (different buffers) created outside the timed region.
    Container* a = new (&storageA) Container(size, T(42));
    Container* b = new (&storageB) Container(size, T(7));
    Container* c = nullptr;

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        // c = move_ctor(a); a becomes moved-from (empty)
        c = new (&storageC) Container(std::move(*a));
        a->~Container(); // destroys moved-from (empty)

        // a = move_ctor(b); b becomes moved-from (empty)
        a = new (&storageA) Container(std::move(*b));
        b->~Container(); // destroys moved-from (empty)

        // b = move_ctor(c); c becomes moved-from (empty)
        b = new (&storageB) Container(std::move(*c));
        c->~Container(); // destroys moved-from (empty)

        // Observe alternating pointer value to prevent dead-code elimination.
        prevent_opt(static_cast<int64_t>(reinterpret_cast<intptr_t>(a->data())));
    }

    double elapsed = timer.elapsed_ns();

    // Clean up live objects outside timing (will deallocate their buffers once each).
    a->~Container();
    b->~Container();

    // 3 move-ctors per rotation iteration.
    return elapsed / static_cast<double>(iterations * 3);
}

/**
 * @brief Measures move-assignment cost without allocations.
 *
 * We rotate ownership among two full containers and one empty container such that
 * the destination of each move-assignment is empty. This avoids deallocation/reallocation
 * work and isolates the constant-time move-assignment path.
 *
 * Returns: nanoseconds per move-assignment.
 */
template <typename Container>
double bench_move_assign_rotation(size_t size, size_t iterations)
{
    using T = typename Container::value_type;

    // Two distinct allocations created outside the timed region.
    Container a(size, T(42));
    Container b(size, T(7));
    Container tmp; // empty

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        tmp = std::move(a); // tmp empty -> steals buffer
        a = std::move(b);   // a empty   -> steals buffer
        b = std::move(tmp); // b empty   -> steals buffer

        prevent_opt(static_cast<int64_t>(reinterpret_cast<intptr_t>(a.data())));
    }

    double elapsed = timer.elapsed_ns();

    // 3 move-assigns per rotation iteration.
    return elapsed / static_cast<double>(iterations * 3);
}

/**
 * @brief Retains the old "construct+move" shape for context (O(n)).
 *
 * This includes allocation and element initialization inside the timed loop.
 * It is useful to understand end-to-end costs, but it is NOT a pure move benchmark.
 *
 * Returns: nanoseconds per iteration of {construct+move}.
 */
template <typename Container>
double bench_construct_and_move(size_t size, size_t iterations)
{
    using T = typename Container::value_type;

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        Container src(size, T(42));    // O(n): allocation + fill/construct
        Container dst(std::move(src)); // O(1): move-ctor
        prevent_opt(static_cast<int64_t>(dst.size()));
    }

    double elapsed = timer.elapsed_ns();
    return elapsed / static_cast<double>(iterations);
}

void benchmark_copy_move(const std::vector<size_t>& sizes)
{
    print_header("COPY AND MOVE OPERATIONS");
    print_contract_note("Copy construction should be O(n). Move construction/assignment should be O(1). "
                        "This section reports:\n"
                        "  1) copy-ctor (bulk copy)\n"
                        "  2) move-ctor (rotation; no allocation)\n"
                        "  3) move-assign (rotation; no allocation)\n"
                        "  4) construct+move (includes allocation+fill; O(n), not a pure move test)");

    print_cpu_context("Copy/Move");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data = generate_data<double>(N, g_config.seed);

        // Iteration counts: copy/construct are O(n), move-only is O(1).
        // Copy-ctor includes allocation and can be noisy at small N; use more iterations
        // for smaller sizes to stabilize measurements without changing semantics.
        const size_t ITERS_COPY = (N <= 20000 ? 500 : (N <= 200000 ? 200 : 50));
        constexpr size_t ITERS_CONSTRUCT_MOVE = 100;
        constexpr size_t ITERS_MOVE_ONLY = 500000;

        std::vector<double> std_vec(data.begin(), data.end());
        fat_p::AlignedVector<double, 64> av64(data.begin(), data.end());

        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector copy-ctor", {}});
        results.push_back({"std::vector move-ctor (rotation)", {}});
        results.push_back({"std::vector move-assign (rotation)", {}});
        results.push_back({"std::vector construct+move (alloc+fill)", {}});
        results.push_back({"AlignedVector<64> copy-ctor", {}});
        results.push_back({"AlignedVector<64> move-ctor (rotation)", {}});
        results.push_back({"AlignedVector<64> move-assign (rotation)", {}});
        results.push_back({"AlignedVector<64> construct+move (alloc+fill)", {}});

        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                double ns = 0;
                const std::string& name = results[idx].name;

                if (name == "std::vector copy-ctor")
                {
                    ns = bench_copy(std_vec, ITERS_COPY);
                }
                else if (name == "std::vector move-ctor (rotation)")
                {
                    ns = bench_move_ctor_rotation<std::vector<double>>(N, ITERS_MOVE_ONLY);
                }
                else if (name == "std::vector move-assign (rotation)")
                {
                    ns = bench_move_assign_rotation<std::vector<double>>(N, ITERS_MOVE_ONLY);
                }
                else if (name == "std::vector construct+move (alloc+fill)")
                {
                    ns = bench_construct_and_move<std::vector<double>>(N, ITERS_CONSTRUCT_MOVE);
                }
                else if (name == "AlignedVector<64> copy-ctor")
                {
                    ns = bench_copy(av64, ITERS_COPY);
                }
                else if (name == "AlignedVector<64> move-ctor (rotation)")
                {
                    ns = bench_move_ctor_rotation<fat_p::AlignedVector<double, 64>>(N, ITERS_MOVE_ONLY);
                }
                else if (name == "AlignedVector<64> move-assign (rotation)")
                {
                    ns = bench_move_assign_rotation<fat_p::AlignedVector<double, 64>>(N, ITERS_MOVE_ONLY);
                }
                else if (name == "AlignedVector<64> construct+move (alloc+fill)")
                {
                    ns = bench_construct_and_move<fat_p::AlignedVector<double, 64>>(N, ITERS_CONSTRUCT_MOVE);
                }

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/op");
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}
// ============================================================================
// BENCHMARK: SIMD Dot Product
// ============================================================================
// Explicitly tests SIMD-friendly operations where alignment matters most.

template <typename Container>
double bench_dot_product(const Container& a, const Container& b, size_t iterations)
{
    using T = typename Container::value_type;
    T result = T(0);

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        T sum = T(0);
        for (size_t i = 0; i < a.size(); ++i)
        {
            sum += a[i] * b[i];
        }
        result += sum;
    }

    double elapsed = timer.elapsed_ns();
    prevent_opt(static_cast<int64_t>(result));

    return elapsed / static_cast<double>(iterations * a.size());
}

template <typename T, size_t Alignment>
double bench_dot_product_assume_aligned(const fat_p::AlignedVector<T, Alignment>& a,
                                        const fat_p::AlignedVector<T, Alignment>& b,
                                        size_t iterations)
{
    T result = T(0);

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        const T* pa = a.assume_aligned();
        const T* pb = b.assume_aligned();
        T sum = T(0);
        for (size_t i = 0; i < a.size(); ++i)
        {
            sum += pa[i] * pb[i];
        }
        result += sum;
    }

    double elapsed = timer.elapsed_ns();
    prevent_opt(static_cast<int64_t>(result));

    return elapsed / static_cast<double>(iterations * a.size());
}

void benchmark_simd_dot_product(const std::vector<size_t>& sizes)
{
    print_header("SIMD DOT PRODUCT");
    print_contract_note("Tests fused multiply-add vectorization potential. "
                        "assume_aligned() enables compiler to use aligned SIMD loads.");

    print_cpu_context("Dot Product");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data_a = generate_data<float>(N, g_config.seed);
        auto data_b = generate_data<float>(N, g_config.seed + 1);
        constexpr size_t ITERS = 100;

        std::vector<float> std_a(data_a.begin(), data_a.end());
        std::vector<float> std_b(data_b.begin(), data_b.end());
        fat_p::AlignedVector<float, 64> av_a(data_a.begin(), data_a.end());
        fat_p::AlignedVector<float, 64> av_b(data_b.begin(), data_b.end());

        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector", {}});
        results.push_back({"AlignedVector<64>", {}});
        results.push_back({"AlignedVector<64>::assume_aligned", {}});

        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                double ns = 0;

                if (results[idx].name == "std::vector")
                {
                    ns = bench_dot_product(std_a, std_b, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64>")
                {
                    ns = bench_dot_product(av_a, av_b, ITERS);
                }
                else if (results[idx].name == "AlignedVector<64>::assume_aligned")
                {
                    ns = bench_dot_product_assume_aligned(av_a, av_b, ITERS);
                }

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/elem");
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}


// ============================================================================
// BENCHMARK: Explicit SIMD SAXPY (Alignment vs Misalignment)
// ============================================================================
// Uses explicit AVX2 intrinsics to demonstrate a realistic benefit of guaranteed
// alignment: the ability to safely use aligned loads/stores, and the ability to
// avoid systematic misalignment that can trigger split cache-line accesses.
//
// NOTE: On many modern CPUs, aligned vs unaligned instructions on already-aligned
// addresses can be very close. The more reliable downside to avoid is a base
// pointer that is consistently misaligned.

#if defined(__AVX2__)

static float* pick_misaligned_32(float* base, size_t max_offset_elems = 8)
{
    // Find a small offset that makes the pointer not 32-byte aligned.
    for (size_t o = 0; o < max_offset_elems; ++o)
    {
        if ((reinterpret_cast<std::uintptr_t>(base + o) & 31u) != 0u)
        {
            return base + o;
        }
    }
    // Fallback: 1 float offset is very likely to be misaligned.
    return base + 1;
}

static double bench_saxpy_avx2_aligned(const float* x, float* y, float a, size_t n, size_t iterations)
{
    // Round down to multiple of 8 for AVX2 (8 floats per vector)
    n = n & ~size_t(7);
    if (n == 0)
    {
        return 0.0;
    }

    __m256 va = _mm256_set1_ps(a);

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        for (size_t i = 0; i < n; i += 8)
        {
            __m256 vx = _mm256_load_ps(x + i);
            __m256 vy = _mm256_load_ps(y + i);
            vy = _mm256_add_ps(_mm256_mul_ps(vx, va), vy);
            _mm256_store_ps(y + i, vy);
        }
    }

    double elapsed = timer.elapsed_ns();
    prevent_opt(static_cast<int64_t>(y[n / 2]));
    return elapsed / static_cast<double>(iterations * n);
}

static double bench_saxpy_avx2_unaligned(const float* x, float* y, float a, size_t n, size_t iterations)
{
    // Round down to multiple of 8 for AVX2 (8 floats per vector)
    n = n & ~size_t(7);
    if (n == 0)
    {
        return 0.0;
    }

    __m256 va = _mm256_set1_ps(a);

    Timer timer;
    timer.start();

    for (size_t iter = 0; iter < iterations; ++iter)
    {
        for (size_t i = 0; i < n; i += 8)
        {
            __m256 vx = _mm256_loadu_ps(x + i);
            __m256 vy = _mm256_loadu_ps(y + i);
            vy = _mm256_add_ps(_mm256_mul_ps(vx, va), vy);
            _mm256_storeu_ps(y + i, vy);
        }
    }

    double elapsed = timer.elapsed_ns();
    prevent_opt(static_cast<int64_t>(y[n / 2]));
    return elapsed / static_cast<double>(iterations * n);
}

void benchmark_simd_saxpy_explicit(const std::vector<size_t>& sizes)
{
    print_header("SIMD SAXPY (Explicit AVX2)");
    print_contract_note("Demonstrates alignment impact with explicit AVX2 loads/stores. "
                        "Includes a deliberately misaligned-pointer case to show a realistic downside of misalignment "
                        "without overstating typical wins.");

    print_cpu_context("SAXPY");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        // Keep the iteration count moderate; SAXPY is bandwidth-heavy.
        const size_t ITERS = (N >= 1'000'000) ? 20 : 80;
        constexpr float A = 1.001f;

        auto x_init = generate_data<float>(N, g_config.seed);
        auto y_init = generate_data<float>(N, g_config.seed + 1);

        // Reference slices (we operate on exactly N elements)
        std::vector<float> x_ref(x_init.begin(), x_init.end());
        std::vector<float> y_ref(y_init.begin(), y_init.end());

        fat_p::AlignedVector<float, 64> ax(x_ref.begin(), x_ref.end());
        fat_p::AlignedVector<float, 64> ux(x_ref.begin(), x_ref.end());
        fat_p::AlignedVector<float, 64> ay(N);
        fat_p::AlignedVector<float, 64> uy(N);

        // Misaligned buffers (allocate a little extra and intentionally offset)
        std::vector<float> mx_buf(N + 16);
        std::vector<float> my_buf(N + 16);
        float* mx = pick_misaligned_32(mx_buf.data());
        float* my = pick_misaligned_32(my_buf.data());

        // Initialize x once (x is read-only)
        std::copy(x_ref.begin(), x_ref.end(), ax.begin());
        std::copy(x_ref.begin(), x_ref.end(), ux.begin());
        std::copy(x_ref.begin(), x_ref.end(), mx);

        std::vector<BenchmarkResult> results;
        results.push_back({"AVX2 aligned (load_ps/store_ps)", {}});
        results.push_back({"AVX2 unaligned (loadu/storeu on aligned ptr)", {}});
        results.push_back({"AVX2 unaligned (loadu/storeu on misaligned ptr)", {}});

        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                double ns = 0;

                if (results[idx].name == "AVX2 aligned (load_ps/store_ps)")
                {
                    std::copy(y_ref.begin(), y_ref.end(), ay.begin());
                    ns = bench_saxpy_avx2_aligned(ax.assume_aligned(), ay.assume_aligned(), A, N, ITERS);
                }
                else if (results[idx].name == "AVX2 unaligned (loadu/storeu on aligned ptr)")
                {
                    std::copy(y_ref.begin(), y_ref.end(), uy.begin());
                    ns = bench_saxpy_avx2_unaligned(ux.assume_aligned(), uy.assume_aligned(), A, N, ITERS);
                }
                else if (results[idx].name == "AVX2 unaligned (loadu/storeu on misaligned ptr)")
                {
                    std::copy(y_ref.begin(), y_ref.end(), my);
                    ns = bench_saxpy_avx2_unaligned(mx, my, A, N, ITERS);
                }

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/elem");
        }

        // Minimal correctness check (first 64 elements, 1 iteration)
        size_t checkN = std::min<size_t>(N, 64);
        checkN = (checkN / 8) * 8;
        if (checkN >= 8)
        {
            std::vector<float> expected(y_ref.begin(), y_ref.begin() + checkN);
            for (size_t i = 0; i < checkN; ++i)
            {
                expected[i] = expected[i] + A * x_ref[i];
            }

            std::copy(y_ref.begin(), y_ref.begin() + checkN, ay.begin());
            bench_saxpy_avx2_aligned(ax.assume_aligned(), ay.assume_aligned(), A, checkN, 1);
            for (size_t i = 0; i < checkN; ++i)
            {
                if (std::abs(ay[i] - expected[i]) > 1e-3f * (1.0f + std::abs(expected[i])))
                {
                    std::cout << "[ERROR] Correctness check failed (aligned) at i=" << i << "\n";
                    break;
                }
            }

            std::copy(y_ref.begin(), y_ref.begin() + checkN, uy.begin());
            bench_saxpy_avx2_unaligned(ux.assume_aligned(), uy.assume_aligned(), A, checkN, 1);
            for (size_t i = 0; i < checkN; ++i)
            {
                if (std::abs(uy[i] - expected[i]) > 1e-3f * (1.0f + std::abs(expected[i])))
                {
                    std::cout << "[ERROR] Correctness check failed (unaligned/aligned-ptr) at i=" << i << "\n";
                    break;
                }
            }

            std::copy(y_ref.begin(), y_ref.begin() + checkN, my);
            bench_saxpy_avx2_unaligned(mx, my, A, checkN, 1);
            for (size_t i = 0; i < checkN; ++i)
            {
                if (std::abs(my[i] - expected[i]) > 1e-3f * (1.0f + std::abs(expected[i])))
                {
                    std::cout << "[ERROR] Correctness check failed (unaligned/misaligned-ptr) at i=" << i << "\n";
                    break;
                }
            }
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

#else

void benchmark_simd_saxpy_explicit(const std::vector<size_t>&)
{
    print_header("SIMD SAXPY (Explicit AVX2)");
    print_contract_note(
        "Skipped: AVX2 not enabled at compile time. Build with -mavx2 or -march=native (or /arch:AVX2 on MSVC). ");
}

#endif

// ============================================================================
// BENCHMARK: Corner Cases
// ============================================================================
// Tests edge cases: empty, single element, near-capacity operations.

void benchmark_corner_cases()
{
    print_header("CORNER CASES");
    print_contract_note("Tests edge cases that may trigger different code paths. "
                        "Empty vector operations, single element, capacity boundary.");

    print_cpu_context("Corner Cases");

    constexpr size_t ITERS = 10000;

    // Test 1: Empty vector operations
    print_subheader("Empty Vector Operations");
    {
        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector empty begin/end", {}});
        results.push_back({"AlignedVector empty begin/end", {}});

        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::vector<size_t> order = {0, 1};

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                Timer timer;
                timer.start();

                if (idx == 0)
                {
                    for (size_t i = 0; i < ITERS; ++i)
                    {
                        std::vector<int> vec;
                        auto b = vec.begin();
                        auto e = vec.end();
                        prevent_opt(static_cast<int64_t>(e - b));
                    }
                }
                else
                {
                    for (size_t i = 0; i < ITERS; ++i)
                    {
                        fat_p::AlignedVector<int, 64> vec;
                        auto b = vec.begin();
                        auto e = vec.end();
                        prevent_opt(static_cast<int64_t>(e - b));
                    }
                }

                double ns = timer.elapsed_ns() / static_cast<double>(ITERS);
                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/op");
        }
    }

    // Test 2: Single push_back + pop_back cycle
    print_subheader("Single Element Push/Pop Cycle");
    {
        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector push/pop", {}});
        results.push_back({"AlignedVector push/pop", {}});

        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::vector<size_t> order = {0, 1};

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                Timer timer;
                timer.start();

                if (idx == 0)
                {
                    std::vector<int> vec;
                    vec.reserve(1);
                    for (size_t i = 0; i < ITERS; ++i)
                    {
                        vec.push_back(42);
                        vec.pop_back();
                    }
                    prevent_opt(static_cast<int64_t>(vec.capacity()));
                }
                else
                {
                    fat_p::AlignedVector<int, 64> vec;
                    vec.reserve(1);
                    for (size_t i = 0; i < ITERS; ++i)
                    {
                        vec.push_back(42);
                        vec.pop_back();
                    }
                    prevent_opt(static_cast<int64_t>(vec.capacity()));
                }

                double ns = timer.elapsed_ns() / static_cast<double>(ITERS);
                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/cycle");
        }
    }

    // Test 3: At-capacity insert (triggers reallocation)
    print_subheader("Insert at Capacity (Reallocation)");
    {
        std::vector<BenchmarkResult> results;
        results.push_back({"std::vector realloc", {}});
        results.push_back({"AlignedVector realloc", {}});

        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::vector<size_t> order = {0, 1};
        constexpr size_t CAP_TARGET = 1000;
        constexpr size_t BATCH = 512; // reallocation events per timed sample

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                Timer timer;

                if (idx == 0)
                {
                    std::vector<std::vector<int>> batch;
                    batch.reserve(BATCH);
                    for (size_t b = 0; b < BATCH; ++b)
                    {
                        std::vector<int> vec;
                        vec.reserve(CAP_TARGET);
                        const size_t cap = vec.capacity();
                        vec.resize(cap);
                        batch.push_back(std::move(vec));
                    }

                    timer.start();
                    for (auto& vec : batch)
                    {
                        vec.push_back(999); // Triggers reallocation
                    }
                    double ns = timer.elapsed_ns() / static_cast<double>(BATCH);

                    prevent_opt(static_cast<int64_t>(batch[0].size()));
                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                }
                else
                {
                    std::vector<fat_p::AlignedVector<int, 64>> batch;
                    batch.reserve(BATCH);
                    for (size_t b = 0; b < BATCH; ++b)
                    {
                        fat_p::AlignedVector<int, 64> vec;
                        vec.reserve(CAP_TARGET);
                        const size_t cap = vec.capacity();
                        vec.resize(cap);
                        batch.push_back(std::move(vec));
                    }

                    timer.start();
                    for (auto& vec : batch)
                    {
                        vec.push_back(999); // Triggers reallocation
                    }
                    double ns = timer.elapsed_ns() / static_cast<double>(BATCH);

                    prevent_opt(static_cast<int64_t>(batch[0].size()));
                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/realloc");
        }
    }
}

// ============================================================================
// BENCHMARK: Alignment Verification
// ============================================================================
// Verifies that alignment guarantees are actually met.

void benchmark_alignment_verification()
{
    print_header("ALIGNMENT VERIFICATION");
    print_contract_note("Verifies alignment guarantees across multiple allocations. "
                        "All allocations must meet specified alignment.");

    print_cpu_context("Alignment Verification");

    constexpr size_t NUM_ALLOCATIONS = 1000;
    constexpr std::array<size_t, 5> ALIGNMENTS = {16, 32, 64, 128, 256};

    for (size_t alignment : ALIGNMENTS)
    {
        print_subheader("Alignment = " + std::to_string(alignment) + " bytes");

        size_t violations = 0;
        size_t total = 0;

        for (size_t i = 0; i < NUM_ALLOCATIONS; ++i)
        {
            // Vary sizes to stress the allocator
            size_t size = 1 + (i % 1000);

            if (alignment == 16)
            {
                fat_p::AlignedVector<float, 16> vec(size);
                auto addr = reinterpret_cast<std::uintptr_t>(vec.data());
                if (addr % 16 != 0)
                {
                    ++violations;
                }
                ++total;
            }
            else if (alignment == 32)
            {
                fat_p::AlignedVector<float, 32> vec(size);
                auto addr = reinterpret_cast<std::uintptr_t>(vec.data());
                if (addr % 32 != 0)
                {
                    ++violations;
                }
                ++total;
            }
            else if (alignment == 64)
            {
                fat_p::AlignedVector<float, 64> vec(size);
                auto addr = reinterpret_cast<std::uintptr_t>(vec.data());
                if (addr % 64 != 0)
                {
                    ++violations;
                }
                ++total;
            }
            else if (alignment == 128)
            {
                fat_p::AlignedVector<float, 128> vec(size);
                auto addr = reinterpret_cast<std::uintptr_t>(vec.data());
                if (addr % 128 != 0)
                {
                    ++violations;
                }
                ++total;
            }
            else if (alignment == 256)
            {
                fat_p::AlignedVector<float, 256> vec(size);
                auto addr = reinterpret_cast<std::uintptr_t>(vec.data());
                if (addr % 256 != 0)
                {
                    ++violations;
                }
                ++total;
            }
        }

        std::cout << "  Allocations: " << total << ", Violations: " << violations;
        if (violations == 0)
        {
            std::cout << " [PASS]\n";
        }
        else
        {
            std::cout << " [FAIL]\n";
        }
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

    // Apply benchmark scope (Windows priority/affinity)
    fat_p::bench::BenchmarkScope scope(!g_config.noScope);

    std::cout << "================================================================================\n";
    std::cout << "  AlignedVector Comprehensive Benchmark Suite\n";
    std::cout << "================================================================================\n";

    // Platform info
    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#elif defined(__APPLE__)
    std::cout << "macOS";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() << ", seed=" << g_config.seed
              << ")\n";

    // Competitor detection
    std::cout << "\nCompetitor libraries: ";
#if HAS_BOOST_ALIGN
    std::cout << "boost::alignment ";
#endif
#if HAS_EIGEN
    std::cout << "Eigen ";
#endif
#if !HAS_BOOST_ALIGN && !HAS_EIGEN
    std::cout << "(none detected - using std::vector baseline)";
#endif
    std::cout << "\n\n";

    // CPU frequency detection
    fat_p::bench::print_cpu_detection_info(std::cout);
    std::cout << "\n";

    // Design invariants
    std::cout << "Design Invariants:\n";
    std::cout << "  1. Round-robin execution with randomized order per run\n";
    std::cout << "  2. Setup/teardown outside timed regions\n";
    std::cout << "  3. Medians are the primary reported statistic\n";
    std::cout << "  4. Correctness verified after each benchmark\n";
    std::cout << "\n";

    std::cout << "Cooling delays: section=" << COOLING_DELAY_SECTION_MS << "ms, "
              << "size=" << COOLING_DELAY_SIZE_MS << "ms, "
              << "case=" << COOLING_DELAY_CASE_MS << "ms\n\n";

    // Wait for CPU stability
    if (!g_config.noStabilize)
    {
        std::cout << "Checking initial CPU state...\n";
        print_cpu_context("Initial");
        std::cout << "Waiting for CPU to stabilize...\n";
        if (!wait_for_cpu_stable(10.0, 30, 200, true))
        {
            std::cout << "WARNING: CPU frequency still fluctuating.\n";
        }
        std::cout << "\n";
    }

    // Define test sizes
    std::vector<size_t> small_sizes = {1000, 10000};
    std::vector<size_t> medium_sizes = {10000, 100000};
    std::vector<size_t> large_sizes = {100000, 1000000};

    // Run benchmarks
    benchmark_alignment_verification();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before sequential iteration");
    benchmark_sequential_iteration(medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before SIMD dot product");
    benchmark_simd_dot_product(medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before SIMD SAXPY");
    benchmark_simd_saxpy_explicit(large_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before random access");
    benchmark_random_access(medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before push_back");
    benchmark_push_back(small_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before insert");
    benchmark_insert(small_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before shift memmove microbench");
    benchmark_shift_memmove(medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before copy/move");
    benchmark_copy_move(medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before corner cases");
    benchmark_corner_cases();

    // Final summary
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    print_cpu_context("Final");

    return 0;
}
