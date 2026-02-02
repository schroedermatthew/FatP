// benchmark_Stringify.cpp
//
// FAT-P Stringify benchmarks using unified FatPBenchmarkRunner infrastructure.
//
// Competitors: std::to_string, std::format (C++20), std::ostringstream, fmt::format
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_Stringify.cpp -o bench_stringify
//   cl /std:c++20 /O2 /DNDEBUG /EHsc /utf-8 benchmark_Stringify.cpp /link advapi32.lib
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
//   ./bench_stringify
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_stringify

/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: benchmark
  path: components/Stringify/benchmarks/benchmark_Stringify.cpp
  layer: Testing
  namespace: fat_p
  summary: "Benchmarks for Stringify."
  api_stability: stable
  related:
    docs_search: "Stringify"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/Stringify.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 2
    defines_unprefixed: 2
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "CppFeatureDetection.h"
#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"
#include "Stringify.h"

#if FATP_HAS_FORMAT
#include <format>
#endif

// Optional: fmt library
#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
#define HAS_FMT 1
#else
#define HAS_FMT 0
#endif

namespace
{

using namespace fat_p::bench;

// ============================================================================
// Data Generation
// ============================================================================

std::vector<int> generate_ints(std::size_t n, std::uint64_t seed)
{
    std::vector<int> data(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> dist(-1000000, 1000000);
    for (std::size_t i = 0; i < n; ++i)
    {
        data[i] = dist(rng);
    }
    return data;
}

std::vector<double> generate_doubles(std::size_t n, std::uint64_t seed)
{
    std::vector<double> data(n);
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-1000.0, 1000.0);
    for (std::size_t i = 0; i < n; ++i)
    {
        data[i] = dist(rng);
    }
    return data;
}

// ============================================================================
// Benchmark Runner (shared pattern)
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

void print_speedup(const char* name, double baseline_median, double test_median)
{
    double speedup = baseline_median / test_median;
    const char* verdict = (speedup > 1.05) ? "FASTER" : (speedup < 0.95) ? "SLOWER" : "SAME";
    std::cout << "    -> " << name << ": " << std::fixed << std::setprecision(2) << speedup << "x " << verdict
              << " than baseline\n";
}

// ============================================================================
// Correctness Checks (outside timed regions)
// ============================================================================

bool verify_integer_correctness()
{
    int test_val = 12345;
    std::string expected = std::to_string(test_val);
    std::string actual = fat_p::toString(test_val);
    return expected == actual;
}

bool verify_container_correctness()
{
    std::vector<int> vec{1, 2, 3};
    auto result = fat_p::toString(vec);
    return result == "[1, 2, 3]";
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

    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "Stringify";
    hdr.warmup = config.warmupRuns;
    hdr.measured = config.measuredRuns;
    hdr.seed = config.seed;
    
    // Competitors
    hdr.competitors.push_back({"fat_p::stringify", true, "primary"});
    hdr.competitors.push_back({"std::to_string", true, "baseline"});
    hdr.competitors.push_back({"std::ostringstream", true, "baseline"});
#if FATP_HAS_FORMAT
    hdr.competitors.push_back({"std::format", true, "C++20"});
#else
    hdr.competitors.push_back({"std::format", false, "not available"});
#endif
#if HAS_FMT
    hdr.competitors.push_back({"fmt::format", true, ""});
#else
    hdr.competitors.push_back({"fmt::format", false, "not detected"});
#endif
    
    hdr.has_extended_config = false;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = false;
    hdr.has_stabilization = !config.noStabilize;
    
    fat_p::bench::print_standard_header(hdr);


    constexpr std::size_t N = 100'000;

    // Generate test data using configured seed
    auto int_data = generate_ints(1024, config.seed);
    auto double_data = generate_doubles(1024, config.seed);

    // ========================================================================
    // Section 1: Integer Stringification
    // ========================================================================
    std::cout << "--- Section 1: Integer Stringification ---\n";
    std::cout << "Contract: Convert int to std::string. No locale formatting.\n\n";
    print_cpu_context(std::cout);

    auto s_to_string_int = run_benchmark("std::to_string", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            auto s = std::to_string(int_data[i % int_data.size()]);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_int = run_benchmark("fat_p::toString", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            auto s = fat_p::toString(int_data[i % int_data.size()]);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_format_int = run_benchmark("std::format", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
#if FATP_HAS_FORMAT
            auto s = std::format("{}", int_data[i % int_data.size()]);
#else
            auto s = std::to_string(int_data[i % int_data.size()]);
#endif
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_oss_int = run_benchmark("std::ostringstream", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            std::ostringstream oss;
            oss << int_data[i % int_data.size()];
            auto s = oss.str();
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("std::to_string (baseline)", s_to_string_int);
    print_stats("fat_p::toString", s_fatp_int);
    print_speedup("fat_p::toString", s_to_string_int.median, s_fatp_int.median);
    print_stats("std::format", s_format_int);
    print_speedup("std::format", s_to_string_int.median, s_format_int.median);
    print_stats("std::ostringstream", s_oss_int);
    print_speedup("std::ostringstream", s_to_string_int.median, s_oss_int.median);

#if HAS_FMT
    auto s_fmt_int = run_benchmark("fmt::format", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            auto s = fmt::format("{}", int_data[i % int_data.size()]);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });
    print_stats("fmt::format", s_fmt_int);
    print_speedup("fmt::format", s_to_string_int.median, s_fmt_int.median);
#endif

    // ========================================================================
    // Section 2: Floating-Point Stringification
    // ========================================================================
    std::cout << "\n--- Section 2: Floating-Point Stringification ---\n";
    std::cout << "Contract: Convert double to std::string. Default precision.\n\n";
    print_cpu_context(std::cout);

    auto s_to_string_dbl = run_benchmark("std::to_string", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            auto s = std::to_string(double_data[i % double_data.size()]);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_dbl = run_benchmark("fat_p::toString", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            auto s = fat_p::toString(double_data[i % double_data.size()]);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_format_dbl = run_benchmark("std::format", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
#if FATP_HAS_FORMAT
            auto s = std::format("{}", double_data[i % double_data.size()]);
#else
            auto s = std::to_string(double_data[i % double_data.size()]);
#endif
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_oss_dbl = run_benchmark("std::ostringstream", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            std::ostringstream oss;
            oss << double_data[i % double_data.size()];
            auto s = oss.str();
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("std::to_string (baseline)", s_to_string_dbl);
    print_stats("fat_p::toString", s_fatp_dbl);
    print_speedup("fat_p::toString", s_to_string_dbl.median, s_fatp_dbl.median);
    print_stats("std::format", s_format_dbl);
    print_speedup("std::format", s_to_string_dbl.median, s_format_dbl.median);
    print_stats("std::ostringstream", s_oss_dbl);
    print_speedup("std::ostringstream", s_to_string_dbl.median, s_oss_dbl.median);

#if HAS_FMT
    auto s_fmt_dbl = run_benchmark("fmt::format", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            auto s = fmt::format("{}", double_data[i % double_data.size()]);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });
    print_stats("fmt::format", s_fmt_dbl);
    print_speedup("fmt::format", s_to_string_dbl.median, s_fmt_dbl.median);
#endif

    // ========================================================================
    // Section 3: Boolean Stringification
    // ========================================================================
    std::cout << "\n--- Section 3: Boolean Stringification ---\n";
    std::cout << "Contract: Convert bool to 'true'/'false' string.\n\n";
    print_cpu_context(std::cout);

    auto s_ternary_bool = run_benchmark("ternary", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            bool val = (i % 2 == 0);
            std::string s = val ? "true" : "false";
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_bool = run_benchmark("fat_p::toString", N, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            bool val = (i % 2 == 0);
            auto s = fat_p::toString(val);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("ternary (baseline)", s_ternary_bool);
    print_stats("fat_p::toString", s_fatp_bool);
    print_speedup("fat_p::toString", s_ternary_bool.median, s_fatp_bool.median);

    // ========================================================================
    // Section 4: Container Stringification (Fat-P unique feature)
    // ========================================================================
    std::cout << "\n--- Section 4: Container Stringification ---\n";
    std::cout << "Contract: Convert vector<int> to '[elem, ...]' format. Fat-P unique.\n\n";
    print_cpu_context(std::cout);

    constexpr std::size_t N_CONTAINER = N / 100; // Containers are heavier
    std::vector<int> test_vec{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto s_manual_vec = run_benchmark("manual loop", N_CONTAINER, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_CONTAINER; ++i)
        {
            std::string result = "[";
            bool first = true;
            for (int v : test_vec)
            {
                if (!first)
                    result += ", ";
                first = false;
                result += std::to_string(v);
            }
            result += "]";
            DoNotOptimize(result);
            len += result.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_vec = run_benchmark("fat_p::toString", N_CONTAINER, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_CONTAINER; ++i)
        {
            auto s = fat_p::toString(test_vec);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("manual loop (baseline)", s_manual_vec);
    print_stats("fat_p::toString", s_fatp_vec);
    print_speedup("fat_p::toString", s_manual_vec.median, s_fatp_vec.median);

    // ========================================================================
    // Section 5: Map Stringification (Fat-P unique feature)
    // ========================================================================
    std::cout << "\n--- Section 5: Map Stringification ---\n";
    std::cout << "Contract: Convert map<string,int> to '{k: v, ...}' format. Fat-P unique.\n\n";
    print_cpu_context(std::cout);

    std::map<std::string, int> test_map{{"alpha", 1}, {"beta", 2}, {"gamma", 3}};

    auto s_manual_map = run_benchmark("manual loop", N_CONTAINER, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_CONTAINER; ++i)
        {
            std::string result = "{";
            bool first = true;
            for (const auto& [k, v] : test_map)
            {
                if (!first)
                    result += ", ";
                first = false;
                result += k + ": " + std::to_string(v);
            }
            result += "}";
            DoNotOptimize(result);
            len += result.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_map = run_benchmark("fat_p::toString", N_CONTAINER, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_CONTAINER; ++i)
        {
            auto s = fat_p::toString(test_map);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("manual loop (baseline)", s_manual_map);
    print_stats("fat_p::toString", s_fatp_map);
    print_speedup("fat_p::toString", s_manual_map.median, s_fatp_map.median);

    // ========================================================================
    // Section 6: Tuple Stringification (Fat-P unique feature)
    // ========================================================================
    std::cout << "\n--- Section 6: Tuple/Pair Stringification ---\n";
    std::cout << "Contract: Convert tuple to '(a, b, c)' format. Fat-P unique.\n\n";
    print_cpu_context(std::cout);

    constexpr std::size_t N_TUPLE = N / 10;
    auto test_tuple = std::make_tuple(42, 3.14, "hello");
    auto test_pair = std::make_pair(42, "value");

    auto s_fatp_tuple = run_benchmark("fat_p::toString(tuple)", N_TUPLE, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_TUPLE; ++i)
        {
            auto s = fat_p::toString(test_tuple);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_pair = run_benchmark("fat_p::toString(pair)", N_TUPLE, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_TUPLE; ++i)
        {
            auto s = fat_p::toString(test_pair);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("fat_p::toString(tuple)", s_fatp_tuple);
    print_stats("fat_p::toString(pair)", s_fatp_pair);

    // ========================================================================
    // Section 7: Optional Stringification (Fat-P unique feature)
    // ========================================================================
    std::cout << "\n--- Section 7: Optional Stringification ---\n";
    std::cout << "Contract: Convert optional<int> to value or 'nullopt'.\n\n";
    print_cpu_context(std::cout);

    std::optional<int> has_value = 42;
    std::optional<int> no_value = std::nullopt;

    auto s_fatp_opt_val = run_benchmark("fat_p::toString(has_value)", N_TUPLE, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_TUPLE; ++i)
        {
            auto s = fat_p::toString(has_value);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_opt_empty = run_benchmark("fat_p::toString(nullopt)", N_TUPLE, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_TUPLE; ++i)
        {
            auto s = fat_p::toString(no_value);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("fat_p::toString(has_value)", s_fatp_opt_val);
    print_stats("fat_p::toString(nullopt)", s_fatp_opt_empty);

    // ========================================================================
    // Section 8: String Concatenation
    // ========================================================================
    std::cout << "\n--- Section 8: String Concatenation (toStringConcat) ---\n";
    std::cout << "Contract: Concatenate multiple values into single string.\n\n";
    print_cpu_context(std::cout);

    auto s_manual_concat = run_benchmark("manual +", N_TUPLE, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_TUPLE; ++i)
        {
            std::string s = "Value: " + std::to_string(42) + ", Pi: " + std::to_string(3.14);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_fatp_concat = run_benchmark("fat_p::toStringConcat", N_TUPLE, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_TUPLE; ++i)
        {
            auto s = fat_p::toStringConcat("Value: ", 42, ", Pi: ", 3.14);
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    auto s_format_concat = run_benchmark("std::format", N_TUPLE, config, [&]() {
        std::size_t len = 0;
        for (std::size_t i = 0; i < N_TUPLE; ++i)
        {
#if FATP_HAS_FORMAT
            auto s = std::format("Value: {}, Pi: {}", 42, 3.14);
#else
            std::ostringstream oss;
            oss << "Value: " << 42 << ", Pi: " << 3.14;
            auto s = oss.str();
#endif
            DoNotOptimize(s);
            len += s.size();
        }
        DoNotOptimize(len);
    });

    print_stats("manual + (baseline)", s_manual_concat);
    print_stats("fat_p::toStringConcat", s_fatp_concat);
    print_speedup("fat_p::toStringConcat", s_manual_concat.median, s_fatp_concat.median);
    print_stats("std::format", s_format_concat);
    print_speedup("std::format", s_manual_concat.median, s_format_concat.median);

    // ========================================================================
    // Correctness Verification
    // ========================================================================
    std::cout << "\n--- Correctness Verification ---\n";
    std::cout << "  Integer: " << (verify_integer_correctness() ? "PASS" : "FAIL") << "\n";
    std::cout << "  Container: " << (verify_container_correctness() ? "PASS" : "FAIL") << "\n";

    // ========================================================================
    std::cout << "\n================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
