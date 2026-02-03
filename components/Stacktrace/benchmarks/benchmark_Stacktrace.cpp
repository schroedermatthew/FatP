// Disable MSVC warnings for standard C functions (must be before any includes)

/*
FATP_META:
  meta_version: 1
  component: Stacktrace
  file_role: benchmark
  path: components/Stacktrace/benchmarks/benchmark_Stacktrace.cpp
  layer: Testing
  namespace: fat_p
  summary: "benchmark file for Stacktrace"
  api_stability: in_work
  related:
    docs_search: "Stacktrace"
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 1
    defines_unprefixed: 1
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#define _CRT_SECURE_NO_WARNINGS 1

/**
 * @file benchmark_Stacktrace.cpp
 * @brief Performance benchmarks for fat_p::Stacktrace
 *
 * Measures:
 *   - captureRaw() (address collection only)
 *   - current() (capture + symbol resolution)
 *   - resolveSymbols() (deferred resolution)
 *   - toString() / toJson() (formatting)
 *   - Depth scaling (5, 10, 20, 50 frames)
 *
 * Contract note: This benchmark measures single-threaded capture performance.
 * No competitors are compared (backend is compile-time selected per platform).
 *
 * Compile (GCC/Clang):
 *   g++ -std=c++20 -O2 -g -DNDEBUG benchmark_Stacktrace.cpp -o bench_stacktrace -ldl
 *
 * Compile (MSVC):
 *   cl /std:c++20 /O2 /Zi /DNDEBUG /EHsc benchmark_Stacktrace.cpp
 *
 * Environment variables:
 *   FATP_BENCH_WARMUP_RUNS   - Warmup batches (default: 3)
 *   FATP_BENCH_BATCHES       - Measured batches (default: 15 Windows, 50 Linux)
 *   FATP_BENCH_VERBOSE_STATS - Print raw samples (default: 0)
 */

#include "FatPBenchmarkHeader.h"
#include "Stacktrace.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

// =============================================================================
// Configuration
// =============================================================================

#if defined(_WIN32) || defined(_WIN64)
static constexpr std::size_t kDefaultWarmupRuns = 3;
static constexpr std::size_t kDefaultMeasuredRuns = 15;
#else
static constexpr std::size_t kDefaultWarmupRuns = 3;
static constexpr std::size_t kDefaultMeasuredRuns = 50;
#endif

static std::size_t getEnvOrDefault(const char* name, std::size_t defaultVal)
{
    const char* val = std::getenv(name);
    if (val)
    {
        return static_cast<std::size_t>(std::atoll(val));
    }
    return defaultVal;
}

static bool getEnvBool(const char* name)
{
    const char* val = std::getenv(name);
    return val && val[0] != '0';
}

struct BenchConfig
{
    std::size_t warmupRuns;
    std::size_t measuredRuns;
    bool verboseStats;

    static BenchConfig fromEnv()
    {
        return {getEnvOrDefault("FATP_BENCH_WARMUP_RUNS", kDefaultWarmupRuns),
                getEnvOrDefault("FATP_BENCH_BATCHES", kDefaultMeasuredRuns),
                getEnvBool("FATP_BENCH_VERBOSE_STATS")};
    }
};

// =============================================================================
// CPU Frequency (simplified)
// =============================================================================

static double getCpuFreqMHz()
{
#if defined(__linux__)
    std::ifstream f("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (f)
    {
        double khz = 0;
        f >> khz;
        return khz / 1000.0;
    }
#elif defined(_WIN32)
    // Would read from registry - simplified for portability
#endif
    return 0;
}

static void printCpuContext()
{
    double freq = getCpuFreqMHz();
    if (freq > 0)
    {
        std::cout << "CPU: " << static_cast<int>(freq) << " MHz\n";
    }
}

// =============================================================================
// Statistics
// =============================================================================

struct Stats
{
    double median;
    double mean;
    double stddev;
    double min;
    double max;
};

static Stats computeStats(std::vector<double>& samples)
{
    if (samples.empty())
    {
        return {0, 0, 0, 0, 0};
    }

    std::sort(samples.begin(), samples.end());

    double med = samples[samples.size() / 2];
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    double avg = sum / static_cast<double>(samples.size());

    double sqSum = 0;
    for (double s : samples)
    {
        sqSum += (s - avg) * (s - avg);
    }
    double sd = std::sqrt(sqSum / static_cast<double>(samples.size()));

    return {med, avg, sd, samples.front(), samples.back()};
}

// =============================================================================
// DCE Prevention
// =============================================================================

template <typename T>
static void doNotOptimize(const T& val)
{
    volatile auto sink = &val;
    (void)sink;
}

// =============================================================================
// Output Formatting
// =============================================================================

static void printHeader(const std::string& title)
{
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

static void printResult(const std::string& name, const Stats& stats, const std::string& unit)
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(30) << std::left << name << std::setw(12) << std::right << stats.median << " " << unit
              << "  (mean: " << stats.mean << ", stddev: " << stats.stddev << ")\n";
}

// =============================================================================
// Helper: Create stack depth via recursion
// =============================================================================

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
static fat_p::Stacktrace
captureAtDepthImpl(std::size_t remaining, std::size_t maxFrames, bool raw);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
static fat_p::Stacktrace
captureAtDepth(std::size_t depth, std::size_t maxFrames, bool raw)
{
    if (depth == 0)
    {
        if (raw)
        {
            return fat_p::Stacktrace::captureRaw(1, maxFrames);
        }
        return fat_p::Stacktrace::current(1, maxFrames);
    }
    return captureAtDepthImpl(depth - 1, maxFrames, raw);
}

static fat_p::Stacktrace captureAtDepthImpl(std::size_t remaining, std::size_t maxFrames, bool raw)
{
    return captureAtDepth(remaining, maxFrames, raw);
}

// =============================================================================
// Benchmark Cases
// =============================================================================

static void benchCaptureRaw(const BenchConfig& cfg)
{
    printHeader("captureRaw() - Address Collection Only");
    printCpuContext();
    std::cout << "Contract: Measures address capture without symbol resolution\n\n";

    constexpr std::size_t kIterationsPerBatch = 1000;

    // Warmup
    for (std::size_t w = 0; w < cfg.warmupRuns; ++w)
    {
        for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
        {
            auto st = fat_p::Stacktrace::captureRaw();
            doNotOptimize(st);
        }
    }

    // Measured
    std::vector<double> samples;
    samples.reserve(cfg.measuredRuns);

    for (std::size_t r = 0; r < cfg.measuredRuns; ++r)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
        {
            auto st = fat_p::Stacktrace::captureRaw();
            doNotOptimize(st);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        samples.push_back(us / static_cast<double>(kIterationsPerBatch));
    }

    // Correctness check
    auto verify = fat_p::Stacktrace::captureRaw();
    if (verify.empty() && fat_p::Stacktrace::hasRealBackend())
    {
        std::cerr << "[ERROR] captureRaw() returned empty trace\n";
    }

    auto stats = computeStats(samples);
    printResult("captureRaw()", stats, "us/op");
}

static void benchCurrent(const BenchConfig& cfg)
{
    printHeader("current() - Full Capture with Symbols");
    printCpuContext();
    std::cout << "Contract: Measures capture + symbol resolution\n\n";

    constexpr std::size_t kIterationsPerBatch = 100;

    // Warmup
    for (std::size_t w = 0; w < cfg.warmupRuns; ++w)
    {
        for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
        {
            auto st = fat_p::Stacktrace::current();
            doNotOptimize(st);
        }
    }

    // Measured
    std::vector<double> samples;
    samples.reserve(cfg.measuredRuns);

    for (std::size_t r = 0; r < cfg.measuredRuns; ++r)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
        {
            auto st = fat_p::Stacktrace::current();
            doNotOptimize(st);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        samples.push_back(us / static_cast<double>(kIterationsPerBatch));
    }

    auto stats = computeStats(samples);
    printResult("current()", stats, "us/op");
}

static void benchFormatting(const BenchConfig& cfg)
{
    printHeader("Formatting - toString() and toJson()");
    printCpuContext();
    std::cout << "Contract: Measures output formatting from symbolized trace\n\n";

    constexpr std::size_t kIterationsPerBatch = 500;
    auto trace = fat_p::Stacktrace::current();

    // toString()
    {
        for (std::size_t w = 0; w < cfg.warmupRuns; ++w)
        {
            for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
            {
                auto s = trace.toString();
                doNotOptimize(s);
            }
        }

        std::vector<double> samples;
        samples.reserve(cfg.measuredRuns);

        for (std::size_t r = 0; r < cfg.measuredRuns; ++r)
        {
            auto start = std::chrono::high_resolution_clock::now();
            for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
            {
                auto s = trace.toString();
                doNotOptimize(s);
            }
            auto end = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(end - start).count();
            samples.push_back(us / static_cast<double>(kIterationsPerBatch));
        }

        auto stats = computeStats(samples);
        printResult("toString()", stats, "us/op");
    }

    // toJson()
    {
        for (std::size_t w = 0; w < cfg.warmupRuns; ++w)
        {
            for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
            {
                auto s = trace.toJson();
                doNotOptimize(s);
            }
        }

        std::vector<double> samples;
        samples.reserve(cfg.measuredRuns);

        for (std::size_t r = 0; r < cfg.measuredRuns; ++r)
        {
            auto start = std::chrono::high_resolution_clock::now();
            for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
            {
                auto s = trace.toJson();
                doNotOptimize(s);
            }
            auto end = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(end - start).count();
            samples.push_back(us / static_cast<double>(kIterationsPerBatch));
        }

        auto stats = computeStats(samples);
        printResult("toJson()", stats, "us/op");
    }
}

static void benchDepthScaling(const BenchConfig& cfg)
{
    printHeader("Depth Scaling - captureRaw() at Various Stack Depths");
    printCpuContext();
    std::cout << "Contract: Measures capture cost vs stack depth\n\n";

    constexpr std::size_t kIterationsPerBatch = 200;
    const std::size_t depths[] = {5, 10, 20, 50};

    std::cout << std::setw(30) << std::left << "Depth" << std::setw(12) << std::right << "Median"
              << "  us/op\n";
    std::cout << std::string(50, '-') << "\n";

    for (std::size_t depth : depths)
    {
        // Warmup
        for (std::size_t w = 0; w < cfg.warmupRuns; ++w)
        {
            for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
            {
                auto st = captureAtDepth(depth, 64, true);
                doNotOptimize(st);
            }
        }

        std::vector<double> samples;
        samples.reserve(cfg.measuredRuns);

        for (std::size_t r = 0; r < cfg.measuredRuns; ++r)
        {
            auto start = std::chrono::high_resolution_clock::now();
            for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
            {
                auto st = captureAtDepth(depth, 64, true);
                doNotOptimize(st);
            }
            auto end = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(end - start).count();
            samples.push_back(us / static_cast<double>(kIterationsPerBatch));
        }

        auto stats = computeStats(samples);
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(30) << std::left << ("depth=" + std::to_string(depth)) << std::setw(12) << std::right
                  << stats.median << "  us/op\n";
    }
}

static void benchHash(const BenchConfig& cfg)
{
    printHeader("hash() - For Deduplication");
    printCpuContext();
    std::cout << "Contract: Measures hash computation for container usage\n\n";

    constexpr std::size_t kIterationsPerBatch = 10000;
    auto trace = fat_p::Stacktrace::current();

    // Warmup
    for (std::size_t w = 0; w < cfg.warmupRuns; ++w)
    {
        for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
        {
            auto h = trace.hash();
            doNotOptimize(h);
        }
    }

    std::vector<double> samples;
    samples.reserve(cfg.measuredRuns);

    for (std::size_t r = 0; r < cfg.measuredRuns; ++r)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < kIterationsPerBatch; ++i)
        {
            auto h = trace.hash();
            doNotOptimize(h);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count();
        samples.push_back(ns / static_cast<double>(kIterationsPerBatch));
    }

    auto stats = computeStats(samples);
    printResult("hash()", stats, "ns/op");
}

// =============================================================================
// Main
// =============================================================================

int main()
{
    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    auto cfg = BenchConfig::fromEnv();

    fat_p::bench::HeaderConfig hdr;
    hdr.component = "Stacktrace";
    hdr.warmup = cfg.warmupRuns;
    hdr.measured = cfg.measuredRuns;
    hdr.seed = 12345; // Stacktrace doesn't use seed

    // Competitors
    hdr.competitors.push_back({"fat_p::Stacktrace", true, "primary"});
    hdr.competitors.push_back(
        {"Native backend (" + std::string(fat_p::Stacktrace::backendName()) + ")", true, "baseline"});
    // No external competitor libraries for Stacktrace

    hdr.has_extended_config = false;
    hdr.is_multi_library = false;
    hdr.has_correctness_checks = false;
    hdr.has_stabilization = false;

    fat_p::bench::print_standard_header(hdr);

    std::cout << "Backend info:\n";
    std::cout << "  Backend:       " << fat_p::Stacktrace::backendName() << "\n";
    std::cout << "  Real backend:  " << (fat_p::Stacktrace::hasRealBackend() ? "yes" : "no") << "\n\n";

    benchCaptureRaw(cfg);
    benchCurrent(cfg);
    benchFormatting(cfg);
    benchDepthScaling(cfg);
    benchHash(cfg);

    printHeader("Summary");
    std::cout << "All benchmarks completed.\n";
    std::cout << "Backend used: " << fat_p::Stacktrace::backendName() << "\n\n";

    return 0;
}
