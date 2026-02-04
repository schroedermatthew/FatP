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
 *
 * Competitor Libraries (auto-detected):
 *   - std::vector: Standard library baseline
 *   - boost::alignment::aligned_allocator: If Boost available
 *   - Eigen::aligned_allocator: If Eigen available
 *
 * Build (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_AlignedVector.cpp -o bench_av
 *   clang++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_AlignedVector.cpp -o bench_av
 *
 * Build (Windows MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc /arch:AVX2 benchmark_AlignedVector.cpp /link advapi32.lib
 *
 * Build (with Boost + Eigen):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -I/path/to/boost -I/path/to/eigen \
 *       benchmark_AlignedVector.cpp -o bench_av
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
  summary: "Benchmarks for AlignedVector vs competitors using adapter pattern."
  api_stability: in_work
  related:
    docs_search: "AlignedVector"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/AlignedVector.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 6
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: Claude
    mode: manual
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
#define FATP_HAS_AVX2 1
#else
#define FATP_HAS_AVX2 0
#endif

#include "AlignedVector.h"
#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"

// ============================================================================
// Optional Competitor Detection
// ============================================================================

#if __has_include(<boost/align/aligned_allocator.hpp>)
#include <boost/align/aligned_allocator.hpp>
#define FATP_HAS_BOOST_ALIGN 1
#else
#define FATP_HAS_BOOST_ALIGN 0
#endif

#if __has_include(<Eigen/Core>)
#include <Eigen/Core>
#define FATP_HAS_EIGEN 1
#else
#define FATP_HAS_EIGEN 0
#endif

// ============================================================================
// Platform Configuration
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#endif

// Global benchmark configuration
static fat_p::bench::BenchConfig g_config;

static size_t WARMUP_RUNS()
{
    return g_config.warmupRuns;
}
static size_t MEASURED_RUNS()
{
    return g_config.measuredRuns;
}

// ============================================================================
// CPU Frequency Monitoring
// ============================================================================

void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

// ============================================================================
// Timer (using BenchClock for consistency)
// ============================================================================

struct Timer
{
    using Clock = fat_p::bench::BenchClock;
    Clock::time_point t0;

    void start()
    {
        t0 = Clock::now();
    }

    double elapsed_ns() const
    {
        auto t1 = Clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }
};

// Prevent dead code elimination
static volatile int64_t g_benchmarkSink = 0;

template <typename T>
static inline void prevent_opt(T value)
{
    g_benchmarkSink ^= static_cast<int64_t>(reinterpret_cast<uintptr_t>(&value) ^ static_cast<uintptr_t>(value));
}

template <typename T>
static inline void prevent_opt_ptr(T* ptr)
{
    g_benchmarkSink ^= reinterpret_cast<int64_t>(ptr);
}

// ============================================================================
// CPU Frequency Stability
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
    g_benchmarkSink ^= static_cast<int64_t>(sink);
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
            double avg_freq =
                std::accumulate(recent_readings.begin(), recent_readings.end(), 0.0) / recent_readings.size();

            double variance_pct = (max_freq - min_freq) / avg_freq * 100.0;
            bool is_stable = (variance_pct <= max_variance_percent) && (avg_freq >= min_required_freq);

            if (is_stable)
            {
                if (++stable_count >= required_stable)
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
    if (g_config.noCooldown)
    {
        return;
    }

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
        s.median = (n % 2 == 1) ? samples[n / 2] : 0.5 * (samples[n / 2 - 1] + samples[n / 2]);

        // Mean
        s.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(n);

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
// Output Formatting
// ============================================================================

static void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

static void print_subheader(const std::string& subtitle)
{
    std::cout << "\n--- " << subtitle << " ---\n\n";
}

static void print_contract_note(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

static void print_result_table_header()
{
    std::cout << std::setw(40) << std::right << "Container"
              << std::setw(16) << "Median"
              << std::setw(16) << "Mean"
              << std::setw(10) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(79, '-') << "\n";
}

static void print_result_row(const std::string& name, const Statistics& stats, const std::string& unit)
{
    std::cout << std::setw(40) << std::right << name
              << std::setw(10) << std::fixed << std::setprecision(2) << stats.median << " " << unit
              << std::setw(10) << std::fixed << std::setprecision(2) << stats.mean << " " << unit
              << std::setw(10) << std::fixed << std::setprecision(2) << stats.stddev
              << "  [" << std::fixed << std::setprecision(2) << stats.ci95_low
              << ", " << std::fixed << std::setprecision(2) << stats.ci95_high << "]\n";

    // High variance warning
    if (stats.stddev > stats.median && stats.median > 0)
    {
        std::cout << "  [NOTE] High variance (stddev > median)\n";
    }
}

// ============================================================================
// Data Generation
// ============================================================================

template <typename T>
std::vector<T> generate_data(size_t N, uint64_t seed)
{
    std::vector<T> data(N);
    std::mt19937_64 rng(seed);

    if constexpr (std::is_floating_point_v<T>)
    {
        std::uniform_real_distribution<T> dist(T(-1000), T(1000));
        for (size_t i = 0; i < N; ++i)
        {
            data[i] = dist(rng);
        }
    }
    else
    {
        std::uniform_int_distribution<T> dist(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        for (size_t i = 0; i < N; ++i)
        {
            data[i] = dist(rng);
        }
    }
    return data;
}

// ============================================================================
// Adapter Interface (per Style Guide Section 13)
// ============================================================================

template <typename T>
struct IVectorAdapter
{
    virtual ~IVectorAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(const std::vector<T>& data) = 0;
    virtual void setup_empty(size_t reserve_capacity) = 0;
    virtual void teardown() = 0;
    virtual T* data_ptr() = 0;
    virtual const T* data_ptr() const = 0;
    virtual size_t size() const = 0;
    virtual void push_back(const T& value) = 0;
    virtual void pop_back() = 0;
    virtual void clear() = 0;
    virtual void insert_at(size_t pos, const T& value) = 0;
    virtual T& operator[](size_t i) = 0;
    virtual const T& operator[](size_t i) const = 0;
    virtual size_t alignment() const = 0;
    
    // For copy/move benchmarks
    virtual std::unique_ptr<IVectorAdapter<T>> clone() const = 0;
    virtual void swap_with(IVectorAdapter<T>& other) = 0;
};

// ============================================================================
// std::vector Adapter (Baseline)
// ============================================================================

template <typename T>
class StdVectorAdapter final : public IVectorAdapter<T>
{
    std::vector<T> vec_;

public:
    const char* name() const override { return "std::vector"; }

    void setup(const std::vector<T>& data) override
    {
        vec_.assign(data.begin(), data.end());
    }

    void setup_empty(size_t reserve_capacity) override
    {
        vec_.clear();
        vec_.reserve(reserve_capacity);
    }

    void teardown() override
    {
        vec_.clear();
        vec_.shrink_to_fit();
    }

    T* data_ptr() override { return vec_.data(); }
    const T* data_ptr() const override { return vec_.data(); }
    size_t size() const override { return vec_.size(); }

    void push_back(const T& value) override { vec_.push_back(value); }
    void pop_back() override { vec_.pop_back(); }
    void clear() override { vec_.clear(); }

    void insert_at(size_t pos, const T& value) override
    {
        vec_.insert(vec_.begin() + static_cast<ptrdiff_t>(pos), value);
    }

    T& operator[](size_t i) override { return vec_[i]; }
    const T& operator[](size_t i) const override { return vec_[i]; }

    size_t alignment() const override { return alignof(T); }

    std::unique_ptr<IVectorAdapter<T>> clone() const override
    {
        auto copy = std::make_unique<StdVectorAdapter<T>>();
        copy->vec_ = vec_;
        return copy;
    }

    void swap_with(IVectorAdapter<T>& other) override
    {
        if (auto* o = dynamic_cast<StdVectorAdapter<T>*>(&other))
        {
            std::swap(vec_, o->vec_);
        }
    }
};

// ============================================================================
// AlignedVector<64> Adapter
// ============================================================================

template <typename T>
class AlignedVector64Adapter final : public IVectorAdapter<T>
{
    fat_p::AlignedVector<T, 64> vec_;

public:
    const char* name() const override { return "AlignedVector<64>"; }

    void setup(const std::vector<T>& data) override
    {
        vec_.assign(data.begin(), data.end());
    }

    void setup_empty(size_t reserve_capacity) override
    {
        vec_.clear();
        vec_.reserve(reserve_capacity);
    }

    void teardown() override
    {
        vec_.clear();
        vec_.shrink_to_fit();
    }

    T* data_ptr() override { return vec_.data(); }
    const T* data_ptr() const override { return vec_.data(); }
    size_t size() const override { return vec_.size(); }

    void push_back(const T& value) override { vec_.push_back(value); }
    void pop_back() override { vec_.pop_back(); }
    void clear() override { vec_.clear(); }

    void insert_at(size_t pos, const T& value) override
    {
        vec_.insert(vec_.begin() + static_cast<ptrdiff_t>(pos), value);
    }

    T& operator[](size_t i) override { return vec_[i]; }
    const T& operator[](size_t i) const override { return vec_[i]; }

    size_t alignment() const override { return 64; }

    std::unique_ptr<IVectorAdapter<T>> clone() const override
    {
        auto copy = std::make_unique<AlignedVector64Adapter<T>>();
        copy->vec_ = vec_;
        return copy;
    }

    void swap_with(IVectorAdapter<T>& other) override
    {
        if (auto* o = dynamic_cast<AlignedVector64Adapter<T>*>(&other))
        {
            std::swap(vec_, o->vec_);
        }
    }
};

// ============================================================================
// AlignedVector<128> Adapter
// ============================================================================

template <typename T>
class AlignedVector128Adapter final : public IVectorAdapter<T>
{
    fat_p::AlignedVector<T, 128> vec_;

public:
    const char* name() const override { return "AlignedVector<128>"; }

    void setup(const std::vector<T>& data) override
    {
        vec_.assign(data.begin(), data.end());
    }

    void setup_empty(size_t reserve_capacity) override
    {
        vec_.clear();
        vec_.reserve(reserve_capacity);
    }

    void teardown() override
    {
        vec_.clear();
        vec_.shrink_to_fit();
    }

    T* data_ptr() override { return vec_.data(); }
    const T* data_ptr() const override { return vec_.data(); }
    size_t size() const override { return vec_.size(); }

    void push_back(const T& value) override { vec_.push_back(value); }
    void pop_back() override { vec_.pop_back(); }
    void clear() override { vec_.clear(); }

    void insert_at(size_t pos, const T& value) override
    {
        vec_.insert(vec_.begin() + static_cast<ptrdiff_t>(pos), value);
    }

    T& operator[](size_t i) override { return vec_[i]; }
    const T& operator[](size_t i) const override { return vec_[i]; }

    size_t alignment() const override { return 128; }

    std::unique_ptr<IVectorAdapter<T>> clone() const override
    {
        auto copy = std::make_unique<AlignedVector128Adapter<T>>();
        copy->vec_ = vec_;
        return copy;
    }

    void swap_with(IVectorAdapter<T>& other) override
    {
        if (auto* o = dynamic_cast<AlignedVector128Adapter<T>*>(&other))
        {
            std::swap(vec_, o->vec_);
        }
    }
};

// ============================================================================
// Boost Aligned Allocator Adapter
// ============================================================================

#if FATP_HAS_BOOST_ALIGN
template <typename T>
class BoostAlignedVectorAdapter final : public IVectorAdapter<T>
{
    std::vector<T, boost::alignment::aligned_allocator<T, 64>> vec_;

public:
    const char* name() const override { return "boost::aligned_allocator<64>"; }

    void setup(const std::vector<T>& data) override
    {
        vec_.assign(data.begin(), data.end());
    }

    void setup_empty(size_t reserve_capacity) override
    {
        vec_.clear();
        vec_.reserve(reserve_capacity);
    }

    void teardown() override
    {
        vec_.clear();
        vec_.shrink_to_fit();
    }

    T* data_ptr() override { return vec_.data(); }
    const T* data_ptr() const override { return vec_.data(); }
    size_t size() const override { return vec_.size(); }

    void push_back(const T& value) override { vec_.push_back(value); }
    void pop_back() override { vec_.pop_back(); }
    void clear() override { vec_.clear(); }

    void insert_at(size_t pos, const T& value) override
    {
        vec_.insert(vec_.begin() + static_cast<ptrdiff_t>(pos), value);
    }

    T& operator[](size_t i) override { return vec_[i]; }
    const T& operator[](size_t i) const override { return vec_[i]; }

    size_t alignment() const override { return 64; }

    std::unique_ptr<IVectorAdapter<T>> clone() const override
    {
        auto copy = std::make_unique<BoostAlignedVectorAdapter<T>>();
        copy->vec_ = vec_;
        return copy;
    }

    void swap_with(IVectorAdapter<T>& other) override
    {
        if (auto* o = dynamic_cast<BoostAlignedVectorAdapter<T>*>(&other))
        {
            std::swap(vec_, o->vec_);
        }
    }
};
#endif

// ============================================================================
// Eigen Aligned Allocator Adapter
// ============================================================================

#if FATP_HAS_EIGEN
template <typename T>
class EigenAlignedVectorAdapter final : public IVectorAdapter<T>
{
    std::vector<T, Eigen::aligned_allocator<T>> vec_;

public:
    const char* name() const override { return "Eigen::aligned_allocator"; }

    void setup(const std::vector<T>& data) override
    {
        vec_.assign(data.begin(), data.end());
    }

    void setup_empty(size_t reserve_capacity) override
    {
        vec_.clear();
        vec_.reserve(reserve_capacity);
    }

    void teardown() override
    {
        vec_.clear();
        vec_.shrink_to_fit();
    }

    T* data_ptr() override { return vec_.data(); }
    const T* data_ptr() const override { return vec_.data(); }
    size_t size() const override { return vec_.size(); }

    void push_back(const T& value) override { vec_.push_back(value); }
    void pop_back() override { vec_.pop_back(); }
    void clear() override { vec_.clear(); }

    void insert_at(size_t pos, const T& value) override
    {
        vec_.insert(vec_.begin() + static_cast<ptrdiff_t>(pos), value);
    }

    T& operator[](size_t i) override { return vec_[i]; }
    const T& operator[](size_t i) const override { return vec_[i]; }

    size_t alignment() const override { return EIGEN_MAX_ALIGN_BYTES; }

    std::unique_ptr<IVectorAdapter<T>> clone() const override
    {
        auto copy = std::make_unique<EigenAlignedVectorAdapter<T>>();
        copy->vec_ = vec_;
        return copy;
    }

    void swap_with(IVectorAdapter<T>& other) override
    {
        if (auto* o = dynamic_cast<EigenAlignedVectorAdapter<T>*>(&other))
        {
            std::swap(vec_, o->vec_);
        }
    }
};
#endif

// ============================================================================
// Adapter Factory
// ============================================================================

template <typename T>
std::vector<std::unique_ptr<IVectorAdapter<T>>> create_all_adapters()
{
    std::vector<std::unique_ptr<IVectorAdapter<T>>> adapters;

    adapters.push_back(std::make_unique<StdVectorAdapter<T>>());
    adapters.push_back(std::make_unique<AlignedVector64Adapter<T>>());
    adapters.push_back(std::make_unique<AlignedVector128Adapter<T>>());

#if FATP_HAS_BOOST_ALIGN
    adapters.push_back(std::make_unique<BoostAlignedVectorAdapter<T>>());
#endif

#if FATP_HAS_EIGEN
    adapters.push_back(std::make_unique<EigenAlignedVectorAdapter<T>>());
#endif

    return adapters;
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
// BENCHMARK: Sequential Iteration (Sum)
// ============================================================================

template <typename T>
void benchmark_sequential_iteration(std::vector<std::unique_ptr<IVectorAdapter<T>>>& adapters,
                                     const std::vector<size_t>& sizes)
{
    print_header("SEQUENTIAL ITERATION (Sum)");
    print_contract_note("Measures cache-line alignment benefit for SIMD auto-vectorization.");

    print_cpu_context("Sequential Iteration");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data = generate_data<T>(N, g_config.seed);
        constexpr size_t ITERS = 100;

        // Setup all adapters outside timing
        for (auto& adapter : adapters)
        {
            adapter->setup(data);
        }

        // Results storage
        std::vector<BenchmarkResult> results;
        for (const auto& adapter : adapters)
        {
            results.push_back({adapter->name(), {}});
        }

        // Round-robin execution
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                auto& adapter = adapters[idx];
                T sum = T(0);

                Timer timer;
                timer.start();

                for (size_t iter = 0; iter < ITERS; ++iter)
                {
                    const T* ptr = adapter->data_ptr();
                    size_t n = adapter->size();
                    for (size_t i = 0; i < n; ++i)
                    {
                        sum += ptr[i];
                    }
                }

                double elapsed = timer.elapsed_ns();
                prevent_opt(static_cast<int64_t>(sum));

                double ns_per_elem = elapsed / static_cast<double>(ITERS * N);

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

        // Teardown
        for (auto& adapter : adapters)
        {
            adapter->teardown();
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Random Access
// ============================================================================

template <typename T>
void benchmark_random_access(std::vector<std::unique_ptr<IVectorAdapter<T>>>& adapters,
                              const std::vector<size_t>& sizes)
{
    print_header("RANDOM ACCESS");
    print_contract_note("Measures cache behavior with non-sequential access patterns. "
                        "Alignment has less impact here; tests baseline overhead.");

    print_cpu_context("Random Access");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data = generate_data<T>(N, g_config.seed);

        // Generate random indices
        std::vector<size_t> indices(N);
        std::iota(indices.begin(), indices.end(), 0);
        std::mt19937 idx_rng(g_config.seed + 1);
        std::shuffle(indices.begin(), indices.end(), idx_rng);

        constexpr size_t ITERS = 10;

        // Setup all adapters outside timing
        for (auto& adapter : adapters)
        {
            adapter->setup(data);
        }

        // Results storage
        std::vector<BenchmarkResult> results;
        for (const auto& adapter : adapters)
        {
            results.push_back({adapter->name(), {}});
        }

        // Round-robin execution
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                auto& adapter = adapters[idx];
                T sum = T(0);

                Timer timer;
                timer.start();

                for (size_t iter = 0; iter < ITERS; ++iter)
                {
                    for (size_t i : indices)
                    {
                        sum += (*adapter)[i];
                    }
                }

                double elapsed = timer.elapsed_ns();
                prevent_opt(static_cast<int64_t>(sum));

                double ns_per_elem = elapsed / static_cast<double>(ITERS * N);

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

        // Teardown
        for (auto& adapter : adapters)
        {
            adapter->teardown();
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Push Back Growth
// ============================================================================

template <typename T>
void benchmark_push_back(std::vector<std::unique_ptr<IVectorAdapter<T>>>& adapters,
                          const std::vector<size_t>& sizes)
{
    print_header("PUSH_BACK GROWTH");
    print_contract_note("Tests amortized O(1) push_back with geometric growth. "
                        "Compares growing vs pre-reserved scenarios.");

    print_cpu_context("Push Back");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data = generate_data<T>(N, g_config.seed);

        // Test 1: Growing (no reserve)
        {
            std::cout << "\n  [Growing - no reserve]\n\n";

            std::vector<BenchmarkResult> results;
            for (const auto& adapter : adapters)
            {
                results.push_back({std::string(adapter->name()) + " (grow)", {}});
            }

            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

            for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
            {
                bool is_warmup = (run < WARMUP_RUNS());
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];
                    adapter->setup_empty(0); // No reserve

                    Timer timer;
                    timer.start();

                    for (size_t i = 0; i < N; ++i)
                    {
                        adapter->push_back(data[i]);
                    }

                    double elapsed = timer.elapsed_ns();
                    prevent_opt_ptr(adapter->data_ptr());

                    double ns_per_op = elapsed / static_cast<double>(N);

                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(ns_per_op);
                    }

                    adapter->teardown();
                }
            }

            print_result_table_header();
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                print_result_row(r.name, stats, "ns/elem");
            }
        }

        // Test 2: Pre-reserved
        {
            std::cout << "\n  [Pre-reserved]\n\n";

            std::vector<BenchmarkResult> results;
            for (const auto& adapter : adapters)
            {
                results.push_back({std::string(adapter->name()) + " (reserved)", {}});
            }

            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed + 1));

            for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
            {
                bool is_warmup = (run < WARMUP_RUNS());
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];
                    adapter->setup_empty(N); // Reserve exactly N

                    Timer timer;
                    timer.start();

                    for (size_t i = 0; i < N; ++i)
                    {
                        adapter->push_back(data[i]);
                    }

                    double elapsed = timer.elapsed_ns();
                    prevent_opt_ptr(adapter->data_ptr());

                    double ns_per_op = elapsed / static_cast<double>(N);

                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(ns_per_op);
                    }

                    adapter->teardown();
                }
            }

            print_result_table_header();
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                print_result_row(r.name, stats, "ns/elem");
            }
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Insert at Middle
// ============================================================================

template <typename T>
void benchmark_insert(std::vector<std::unique_ptr<IVectorAdapter<T>>>& adapters,
                       const std::vector<size_t>& sizes)
{
    print_header("INSERT AT MIDDLE");
    print_contract_note("Measures O(n) insertion at middle position. Tests element shifting performance.");

    print_cpu_context("Insert");

    for (size_t N : sizes)
    {
        constexpr size_t INSERT_COUNT = 100;

        print_subheader("Initial size = " + std::to_string(N) + ", inserting " +
                        std::to_string(INSERT_COUNT) + " elements");

        auto data = generate_data<T>(N, g_config.seed);

        std::vector<BenchmarkResult> results;
        for (const auto& adapter : adapters)
        {
            results.push_back({adapter->name(), {}});
        }

        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                auto& adapter = adapters[idx];
                adapter->setup(data);

                Timer timer;
                timer.start();

                for (size_t i = 0; i < INSERT_COUNT; ++i)
                {
                    size_t pos = adapter->size() / 2;
                    adapter->insert_at(pos, static_cast<T>(i));
                }

                double elapsed = timer.elapsed_ns();
                prevent_opt_ptr(adapter->data_ptr());

                double ns_per_insert = elapsed / static_cast<double>(INSERT_COUNT);

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns_per_insert);
                }

                adapter->teardown();
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
// BENCHMARK: Copy and Move Operations
// ============================================================================

template <typename T>
void benchmark_copy_move(std::vector<std::unique_ptr<IVectorAdapter<T>>>& adapters,
                          const std::vector<size_t>& sizes)
{
    print_header("COPY AND MOVE OPERATIONS");
    print_contract_note("Copy construction should be O(n). Move construction/assignment should be O(1). "
                        "This section reports: copy-ctor, move-ctor (rotation), move-assign (rotation).");

    print_cpu_context("Copy/Move");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data = generate_data<T>(N, g_config.seed);

        // Setup all adapters
        for (auto& adapter : adapters)
        {
            adapter->setup(data);
        }

        // Copy construction benchmark
        {
            std::cout << "\n  [Copy Construction]\n\n";

            std::vector<BenchmarkResult> results;
            for (const auto& adapter : adapters)
            {
                results.push_back({std::string(adapter->name()) + " copy-ctor", {}});
            }

            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

            for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
            {
                bool is_warmup = (run < WARMUP_RUNS());
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];

                    Timer timer;
                    timer.start();

                    auto copy = adapter->clone();

                    double elapsed = timer.elapsed_ns();
                    prevent_opt_ptr(copy->data_ptr());

                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(elapsed);
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

        // Move construction benchmark (via rotation pattern)
        {
            std::cout << "\n  [Move Construction (rotation)]\n\n";

            std::vector<BenchmarkResult> results;
            for (const auto& adapter : adapters)
            {
                results.push_back({std::string(adapter->name()) + " move-ctor", {}});
            }

            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed + 1));

            constexpr size_t MOVE_ITERS = 1000;

            for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
            {
                bool is_warmup = (run < WARMUP_RUNS());
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    auto& adapter = adapters[idx];
                    auto a = adapter->clone();
                    auto b = adapter->clone();
                    auto c = adapter->clone();

                    Timer timer;
                    timer.start();

                    for (size_t i = 0; i < MOVE_ITERS; ++i)
                    {
                        a->swap_with(*b);
                        b->swap_with(*c);
                        c->swap_with(*a);
                    }

                    double elapsed = timer.elapsed_ns();
                    prevent_opt_ptr(a->data_ptr());

                    double ns_per_move = elapsed / static_cast<double>(MOVE_ITERS * 3);

                    if (!is_warmup)
                    {
                        results[idx].samples.push_back(ns_per_move);
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

        // Teardown
        for (auto& adapter : adapters)
        {
            adapter->teardown();
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: Corner Cases
// ============================================================================

template <typename T>
void benchmark_corner_cases(std::vector<std::unique_ptr<IVectorAdapter<T>>>& adapters)
{
    print_header("CORNER CASES");
    print_contract_note("Tests edge cases: empty vector operations, single element, capacity boundary.");

    print_cpu_context("Corner Cases");

    // Test 1: Empty vector begin/end
    {
        print_subheader("Empty Vector Operations");

        std::vector<BenchmarkResult> results;
        for (const auto& adapter : adapters)
        {
            results.push_back({std::string(adapter->name()) + " empty begin/end", {}});
        }

        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        constexpr size_t ITERS = 100000;

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                auto& adapter = adapters[idx];
                adapter->setup_empty(0);

                Timer timer;
                timer.start();

                for (size_t i = 0; i < ITERS; ++i)
                {
                    prevent_opt_ptr(adapter->data_ptr());
                    prevent_opt(static_cast<int64_t>(adapter->size()));
                }

                double elapsed = timer.elapsed_ns();
                double ns_per_op = elapsed / static_cast<double>(ITERS);

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns_per_op);
                }

                adapter->teardown();
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/op");
        }
    }

    // Test 2: Single element push/pop cycle
    {
        print_subheader("Single Element Push/Pop Cycle");

        std::vector<BenchmarkResult> results;
        for (const auto& adapter : adapters)
        {
            results.push_back({std::string(adapter->name()) + " push/pop", {}});
        }

        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed + 1));

        constexpr size_t CYCLES = 10000;

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                auto& adapter = adapters[idx];
                adapter->setup_empty(1);

                Timer timer;
                timer.start();

                for (size_t i = 0; i < CYCLES; ++i)
                {
                    adapter->push_back(static_cast<T>(i));
                    adapter->pop_back();
                }

                double elapsed = timer.elapsed_ns();
                double ns_per_cycle = elapsed / static_cast<double>(CYCLES);

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns_per_cycle);
                }

                adapter->teardown();
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/cycle");
        }
    }
}

// ============================================================================
// BENCHMARK: Alignment Verification (Fat-P specific)
// ============================================================================

void benchmark_alignment_verification()
{
    print_header("ALIGNMENT VERIFICATION");
    print_contract_note("Verifies alignment guarantees across multiple allocations. "
                        "All allocations must meet specified alignment.");

    print_cpu_context("Alignment Verification");

    std::vector<size_t> alignments = {16, 32, 64, 128, 256};

    for (size_t align : alignments)
    {
        print_subheader("Alignment = " + std::to_string(align) + " bytes");

        constexpr size_t NUM_ALLOCS = 1000;
        size_t violations = 0;

        for (size_t i = 0; i < NUM_ALLOCS; ++i)
        {
            // Test different alignments based on size
            void* ptr = nullptr;
            size_t alloc_size = 100 + i;

            if (align == 16)
            {
                fat_p::AlignedVector<float, 16> v(alloc_size);
                ptr = v.data();
            }
            else if (align == 32)
            {
                fat_p::AlignedVector<float, 32> v(alloc_size);
                ptr = v.data();
            }
            else if (align == 64)
            {
                fat_p::AlignedVector<float, 64> v(alloc_size);
                ptr = v.data();
            }
            else if (align == 128)
            {
                fat_p::AlignedVector<float, 128> v(alloc_size);
                ptr = v.data();
            }
            else if (align == 256)
            {
                fat_p::AlignedVector<float, 256> v(alloc_size);
                ptr = v.data();
            }

            if (ptr && (reinterpret_cast<uintptr_t>(ptr) % align) != 0)
            {
                ++violations;
            }
        }

        std::cout << "  Allocations: " << NUM_ALLOCS << ", Violations: " << violations;
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
// BENCHMARK: SIMD Dot Product
// ============================================================================

template <typename T>
void benchmark_simd_dot_product(std::vector<std::unique_ptr<IVectorAdapter<T>>>& adapters,
                                 const std::vector<size_t>& sizes)
{
    print_header("SIMD DOT PRODUCT");
    print_contract_note("Tests fused multiply-add vectorization potential. "
                        "assume_aligned() enables compiler to use aligned SIMD loads.");

    print_cpu_context("Dot Product");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data_a = generate_data<T>(N, g_config.seed);
        auto data_b = generate_data<T>(N, g_config.seed + 1);

        constexpr size_t ITERS = 100;

        // We need two vectors for dot product, so set up pairs
        std::vector<BenchmarkResult> results;
        for (const auto& adapter : adapters)
        {
            results.push_back({adapter->name(), {}});
        }

        // Setup adapters with first data set
        for (auto& adapter : adapters)
        {
            adapter->setup(data_a);
        }

        // Create second set of adapters for data_b
        auto adapters_b = create_all_adapters<T>();
        for (auto& adapter : adapters_b)
        {
            adapter->setup(data_b);
        }

        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                auto& adapter_a = adapters[idx];
                auto& adapter_b = adapters_b[idx];

                T dot = T(0);

                Timer timer;
                timer.start();

                for (size_t iter = 0; iter < ITERS; ++iter)
                {
                    const T* a = adapter_a->data_ptr();
                    const T* b = adapter_b->data_ptr();
                    T local_dot = T(0);

                    for (size_t i = 0; i < N; ++i)
                    {
                        local_dot += a[i] * b[i];
                    }
                    dot += local_dot;
                }

                double elapsed = timer.elapsed_ns();
                prevent_opt(static_cast<int64_t>(dot));

                double ns_per_elem = elapsed / static_cast<double>(ITERS * N);

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns_per_elem);
                }
            }
        }

        print_result_table_header();
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.name, stats, "ns/elem");
        }

        // Teardown
        for (auto& adapter : adapters)
        {
            adapter->teardown();
        }
        for (auto& adapter : adapters_b)
        {
            adapter->teardown();
        }

        cooling_delay(COOLING_DELAY_SIZE_MS, "between sizes");
    }
}

// ============================================================================
// BENCHMARK: SIMD SAXPY (Explicit AVX2) - AlignedVector specific
// ============================================================================

#if FATP_HAS_AVX2
void benchmark_simd_saxpy_explicit(const std::vector<size_t>& sizes)
{
    print_header("SIMD SAXPY (Explicit AVX2)");
    print_contract_note("Demonstrates alignment impact with explicit AVX2 loads/stores. "
                        "Includes a deliberately misaligned-pointer case.");

    print_cpu_context("SAXPY");

    for (size_t N : sizes)
    {
        print_subheader("N = " + std::to_string(N));

        auto data_x = generate_data<float>(N, g_config.seed);
        auto data_y = generate_data<float>(N, g_config.seed + 1);
        float alpha = 2.5f;

        fat_p::AlignedVector<float, 32> x_aligned(data_x.begin(), data_x.end());
        fat_p::AlignedVector<float, 32> y_aligned(data_y.begin(), data_y.end());

        // Misaligned version (offset by 4 bytes)
        std::vector<float> x_misaligned_storage(N + 1);
        std::vector<float> y_misaligned_storage(N + 1);
        float* x_misaligned = x_misaligned_storage.data() + 1;
        float* y_misaligned = y_misaligned_storage.data() + 1;
        std::copy(data_x.begin(), data_x.end(), x_misaligned);
        std::copy(data_y.begin(), data_y.end(), y_misaligned);

        constexpr size_t ITERS = 100;

        std::vector<BenchmarkResult> results;
        results.push_back({"AVX2 aligned (load_ps/store_ps)", {}});
        results.push_back({"AVX2 unaligned (loadu/storeu on aligned ptr)", {}});
        results.push_back({"AVX2 unaligned (loadu/storeu on misaligned ptr)", {}});

        std::vector<size_t> order = {0, 1, 2};
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        for (size_t run = 0; run < WARMUP_RUNS() + MEASURED_RUNS(); ++run)
        {
            bool is_warmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                Timer timer;
                timer.start();

                __m256 valpha = _mm256_set1_ps(alpha);

                if (idx == 0)
                {
                    // Aligned loads/stores
                    for (size_t iter = 0; iter < ITERS; ++iter)
                    {
                        float* x = x_aligned.data();
                        float* y = y_aligned.data();
                        for (size_t i = 0; i + 8 <= N; i += 8)
                        {
                            __m256 vx = _mm256_load_ps(x + i);
                            __m256 vy = _mm256_load_ps(y + i);
                            __m256 result = _mm256_fmadd_ps(valpha, vx, vy);
                            _mm256_store_ps(y + i, result);
                        }
                    }
                }
                else if (idx == 1)
                {
                    // Unaligned loads on aligned data
                    for (size_t iter = 0; iter < ITERS; ++iter)
                    {
                        float* x = x_aligned.data();
                        float* y = y_aligned.data();
                        for (size_t i = 0; i + 8 <= N; i += 8)
                        {
                            __m256 vx = _mm256_loadu_ps(x + i);
                            __m256 vy = _mm256_loadu_ps(y + i);
                            __m256 result = _mm256_fmadd_ps(valpha, vx, vy);
                            _mm256_storeu_ps(y + i, result);
                        }
                    }
                }
                else
                {
                    // Unaligned loads on misaligned data
                    for (size_t iter = 0; iter < ITERS; ++iter)
                    {
                        for (size_t i = 0; i + 8 <= N; i += 8)
                        {
                            __m256 vx = _mm256_loadu_ps(x_misaligned + i);
                            __m256 vy = _mm256_loadu_ps(y_misaligned + i);
                            __m256 result = _mm256_fmadd_ps(valpha, vx, vy);
                            _mm256_storeu_ps(y_misaligned + i, result);
                        }
                    }
                }

                double elapsed = timer.elapsed_ns();
                double ns_per_elem = elapsed / static_cast<double>(ITERS * N);

                if (!is_warmup)
                {
                    results[idx].samples.push_back(ns_per_elem);
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
#else
void benchmark_simd_saxpy_explicit(const std::vector<size_t>&)
{
    print_header("SIMD SAXPY (Explicit AVX2)");
    std::cout << "[SKIPPED] AVX2 not available on this platform\n";
}
#endif

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

    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "AlignedVector";
    hdr.warmup = WARMUP_RUNS();
    hdr.measured = MEASURED_RUNS();
    hdr.seed = g_config.seed;

    // Competitors
    hdr.competitors.push_back({"fat_p::AlignedVector", true, "primary"});
    hdr.competitors.push_back({"std::vector", true, "baseline"});
#if FATP_HAS_BOOST_ALIGN
    hdr.competitors.push_back({"boost::alignment::aligned_allocator", true, ""});
#else
    hdr.competitors.push_back({"boost::alignment", false, "not detected"});
#endif
#if FATP_HAS_EIGEN
    hdr.competitors.push_back({"Eigen::aligned_allocator", true, ""});
#else
    hdr.competitors.push_back({"Eigen", false, "not detected"});
#endif

    // Configuration
    hdr.has_extended_config = false;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = true;
    hdr.has_stabilization = !g_config.noStabilize;
    hdr.cool_section_ms = COOLING_DELAY_SECTION_MS;
    hdr.cool_size_ms = COOLING_DELAY_SIZE_MS;
    hdr.cool_case_ms = COOLING_DELAY_CASE_MS;

    fat_p::bench::print_standard_header(hdr);

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

    // =========================================================================
    // Create adapters
    // =========================================================================
    auto float_adapters = create_all_adapters<float>();
    auto int_adapters = create_all_adapters<int>();

    // Define test sizes
    std::vector<size_t> small_sizes = {1000, 10000};
    std::vector<size_t> medium_sizes = {10000, 100000};
    std::vector<size_t> large_sizes = {100000, 1000000};

    // =========================================================================
    // Run benchmarks with adapters (all competitors in all tests)
    // =========================================================================
    benchmark_alignment_verification();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before sequential iteration");
    benchmark_sequential_iteration(float_adapters, medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before SIMD dot product");
    benchmark_simd_dot_product(float_adapters, medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before SIMD SAXPY");
    benchmark_simd_saxpy_explicit(large_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before random access");
    benchmark_random_access(float_adapters, medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before push_back");
    benchmark_push_back(float_adapters, small_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before insert");
    benchmark_insert(int_adapters, small_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before copy/move");
    benchmark_copy_move(float_adapters, medium_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before corner cases");
    benchmark_corner_cases(float_adapters);

    // Final summary
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    print_cpu_context("Final");

    return 0;
}
