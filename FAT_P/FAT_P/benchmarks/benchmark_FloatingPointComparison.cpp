// benchmark_FloatingPointComparison.cpp
//
// FAT-P FloatingPointComparison benchmarks using unified FatPBenchmarkRunner infrastructure.
//
// Build:
//   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_FloatingPointComparison.cpp -o bench_fp
//   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_FloatingPointComparison.cpp
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
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
//   ./bench_fp
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_fp

/*
FATP_META:
  meta_version: 1
  component: FloatingPointComparison
  file_role: benchmark
  path: benchmarks/benchmark_FloatingPointComparison.cpp
  namespace: fat_p
  summary: "Benchmarks for FloatingPointComparison."
  related:
    docs_search: "FloatingPointComparison"
    headers:
      - fat_p/FatPBenchmarkRunner.h
      - fat_p/FloatingPointComparison.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "FloatingPointComparison.h"

namespace
{

using namespace fat_p::bench;

// ============================================================================
// Data Generation
// ============================================================================

struct TestData
{
    std::vector<double> values_a;
    std::vector<double> values_b;
    double epsilon = 1e-9;
};

TestData generate_normal_pairs(std::size_t n, std::uint64_t seed = 12345)
{
    TestData data;
    data.values_a.reserve(n);
    data.values_b.reserve(n);

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);
    std::uniform_real_distribution<double> noise(-1e-10, 1e-10);

    for (std::size_t i = 0; i < n; ++i)
    {
        double v = dist(rng);
        data.values_a.push_back(v);
        data.values_b.push_back(v + noise(rng));
    }
    return data;
}

TestData generate_multiscale_pairs(std::size_t n, std::uint64_t seed = 12345)
{
    TestData data;
    data.values_a.reserve(n);
    data.values_b.reserve(n);

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> exp_dist(-20, 20);
    std::uniform_real_distribution<double> mant_dist(1.0, 10.0);
    std::uniform_real_distribution<double> noise(-1e-10, 1e-10);

    for (std::size_t i = 0; i < n; ++i)
    {
        double v = mant_dist(rng) * std::pow(10.0, exp_dist(rng));
        data.values_a.push_back(v);
        data.values_b.push_back(v * (1.0 + noise(rng)));
    }
    return data;
}

TestData generate_special_values(std::size_t n, bool nan_values)
{
    TestData data;
    data.values_a.resize(n);
    data.values_b.resize(n);

    double special = nan_values ? std::nan("") : std::numeric_limits<double>::infinity();
    std::fill(data.values_a.begin(), data.values_a.end(), special);
    std::fill(data.values_b.begin(), data.values_b.end(), special);
    return data;
}

// ============================================================================
// Baseline Implementations (what users write without Fat-P)
// ============================================================================

namespace baseline
{

bool manual_absolute(double a, double b, double eps)
{
    return std::fabs(a - b) <= eps;
}

bool manual_relative(double a, double b, double eps)
{
    return std::fabs(a - b) <= eps * std::max(std::fabs(a), std::fabs(b));
}

bool manual_hybrid(double a, double b, double rel_eps, double abs_eps)
{
    double diff = std::fabs(a - b);
    if (diff <= abs_eps)
    {
        return true;
    }
    return diff <= rel_eps * std::max(std::fabs(a), std::fabs(b));
}

} // namespace baseline

// ============================================================================
// Benchmark Runner
// ============================================================================

template <typename Func>
Statistics run_benchmark(const char* /*name*/, std::size_t ops_per_batch, const BenchConfig& config, Func&& func)
{
    // Warmup
    for (std::size_t i = 0; i < config.warmupRuns; ++i)
    {
        func();
    }

    // Measured runs
    std::vector<double> samples;
    samples.reserve(config.measuredRuns);

    for (std::size_t run = 0; run < config.measuredRuns; ++run)
    {
        Timer t;
        t.start();
        func();
        double elapsed = t.elapsedNs();
        samples.push_back(elapsed / static_cast<double>(ops_per_batch));
    }

    return Statistics::compute(samples);
}

void print_stats(const char* name, const Statistics& s)
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << std::setw(30) << std::left << name << std::setw(10) << s.median << " ns"
              << "  (mean: " << s.mean << ", stddev: " << s.stddev << ")"
              << "  CI95: [" << s.ci95Low << ", " << s.ci95High << "]\n";
}

} // anonymous namespace

// ============================================================================
// Main
// ============================================================================

int main()
{
    using namespace fat_p::bench;

    // Load configuration from environment
    BenchConfig config = BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity)
    BenchmarkScope scope(!config.noScope);

    std::cout << "================================================================\n";
    std::cout << "  FloatingPointComparison Benchmark\n";
    std::cout << "================================================================\n\n";

    print_cpu_context(std::cout);

    constexpr std::size_t N = 1'000'000;

    std::cout << "\nConfiguration:\n";
    std::cout << "  Operations per batch: " << N << "\n";
    std::cout << "  Measured runs: " << config.measuredRuns << "\n";
    std::cout << "  Warmup runs: " << config.warmupRuns << "\n";
    std::cout << "  Seed: " << config.seed << "\n\n";

    // Generate test data using configured seed
    auto normal_data = generate_normal_pairs(N, config.seed);
    auto multiscale_data = generate_multiscale_pairs(N, config.seed);
    auto nan_data = generate_special_values(N, true);
    auto inf_data = generate_special_values(N, false);

    // ========================================================================
    std::cout << "--- Fat-P Policies vs Manual Baseline ---\n";
    std::cout << "Contract: absolute/hybrid tolerance comparison semantics\n\n";
    print_cpu_context(std::cout);

    // Standard vs manual_absolute
    {
        auto s1 = run_benchmark("Fat-P Standard", N, config, [&]() {
            std::size_t count = 0;
            for (std::size_t i = 0; i < N; ++i)
            {
                if (fat_p::floatEqual<double, fat_p::StandardComparisonPolicy>(normal_data.values_a[i],
                                                                               normal_data.values_b[i],
                                                                               1e-9))
                {
                    ++count;
                }
            }
            DoNotOptimize(count);
        });

        auto s2 = run_benchmark("Manual absolute", N, config, [&]() {
            std::size_t count = 0;
            for (std::size_t i = 0; i < N; ++i)
            {
                if (baseline::manual_absolute(normal_data.values_a[i], normal_data.values_b[i], 1e-9))
                {
                    ++count;
                }
            }
            DoNotOptimize(count);
        });

        print_stats("Fat-P Standard", s1);
        print_stats("Manual absolute", s2);
        std::cout << "  Ratio: " << std::fixed << std::setprecision(2) << (s1.median / s2.median) << "x\n\n";
    }

    // Hybrid vs manual_hybrid
    {
        auto s1 = run_benchmark("Fat-P Hybrid", N, config, [&]() {
            std::size_t count = 0;
            for (std::size_t i = 0; i < N; ++i)
            {
                if (fat_p::approximateEqual(normal_data.values_a[i], normal_data.values_b[i], 1e-9, 1e-12))
                {
                    ++count;
                }
            }
            DoNotOptimize(count);
        });

        auto s2 = run_benchmark("Manual hybrid", N, config, [&]() {
            std::size_t count = 0;
            for (std::size_t i = 0; i < N; ++i)
            {
                if (baseline::manual_hybrid(normal_data.values_a[i], normal_data.values_b[i], 1e-9, 1e-12))
                {
                    ++count;
                }
            }
            DoNotOptimize(count);
        });

        print_stats("Fat-P Hybrid", s1);
        print_stats("Manual hybrid", s2);
        std::cout << "  Ratio: " << std::fixed << std::setprecision(2) << (s1.median / s2.median) << "x\n\n";
    }

    // ========================================================================
    std::cout << "--- Policy Comparison (Normal Values) ---\n";
    std::cout << "Contract: epsilon-based floating-point equality\n\n";
    print_cpu_context(std::cout);

    auto s_standard = run_benchmark("Standard", N, config, [&]() {
        std::size_t count = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (fat_p::floatEqual<double, fat_p::StandardComparisonPolicy>(normal_data.values_a[i],
                                                                           normal_data.values_b[i],
                                                                           1e-9))
            {
                ++count;
            }
        }
        DoNotOptimize(count);
    });

    auto s_relative = run_benchmark("Relative", N, config, [&]() {
        std::size_t count = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (fat_p::floatEqual<double, fat_p::RelativeComparisonPolicy>(normal_data.values_a[i],
                                                                           normal_data.values_b[i],
                                                                           1e-9))
            {
                ++count;
            }
        }
        DoNotOptimize(count);
    });

    auto s_ulp = run_benchmark("ULP", N, config, [&]() {
        std::size_t count = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (fat_p::floatEqual<double, fat_p::UlpComparisonPolicy>(normal_data.values_a[i],
                                                                      normal_data.values_b[i],
                                                                      4.0))
            {
                ++count;
            }
        }
        DoNotOptimize(count);
    });

    auto s_hybrid = run_benchmark("Hybrid", N, config, [&]() {
        std::size_t count = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (fat_p::approximateEqual(normal_data.values_a[i], normal_data.values_b[i]))
            {
                ++count;
            }
        }
        DoNotOptimize(count);
    });

    print_stats("Standard", s_standard);
    print_stats("Relative", s_relative);
    print_stats("ULP", s_ulp);
    print_stats("Hybrid", s_hybrid);

    // ========================================================================
    std::cout << "\n--- Special Value Handling ---\n";
    std::cout << "Contract: IEEE 754 NaN/Inf semantics\n\n";
    print_cpu_context(std::cout);

    auto s_nan = run_benchmark("NaN", N, config, [&]() {
        std::size_t count = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (fat_p::approximateEqual(nan_data.values_a[i], nan_data.values_b[i]))
            {
                ++count;
            }
        }
        DoNotOptimize(count);
    });

    auto s_inf = run_benchmark("Infinity", N, config, [&]() {
        std::size_t count = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (fat_p::approximateEqual(inf_data.values_a[i], inf_data.values_b[i]))
            {
                ++count;
            }
        }
        DoNotOptimize(count);
    });

    print_stats("NaN", s_nan);
    print_stats("Infinity", s_inf);
    std::cout << "  Speedup vs normal: " << std::fixed << std::setprecision(1) << (s_hybrid.median / s_nan.median)
              << "x (NaN), " << (s_hybrid.median / s_inf.median) << "x (Inf)\n";

    // ========================================================================
    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================\n";

    return 0;
}
