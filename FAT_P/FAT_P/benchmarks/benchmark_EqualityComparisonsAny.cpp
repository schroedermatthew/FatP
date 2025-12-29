// benchmark_EqualityComparisonsAny.cpp
//
// FAT-P EqualityComparisons & EqualityAny benchmarks using FatPBenchmarkRunner.
//
// Architecture: Statistical measurement with multiple batches.
// Reports median as primary metric with mean, stddev, and 95% CI.
//
// Design Invariants:
//   1. Each benchmark collects multiple samples (batches)
//   2. Medians are the primary reported statistic
//   3. Setup occurs outside timed regions
//   4. CPU frequency monitoring for thermal awareness
//
// Build:
//   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_EqualityComparisonsAny.cpp -o bench_eq
//   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_EqualityComparisonsAny.cpp
//
// Environment Variables (all optional):
//   FATP_BENCH_TARGET_WORK   - Target element comparisons per benchmark (default: 5000000)
//   FATP_BENCH_BATCHES       - Number of measurement batches (default: 30, Windows: 15)
//   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
//   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
//
// Run:
//   ./bench_eq
//   FATP_BENCH_TARGET_WORK=1000000 ./bench_eq

#include <algorithm>
#include <any>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>


#include "EqualityAny.h"
#include "FatPBenchmarkRunner.h"
#include "FatPTest.h"

// Platform-specific includes
#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winreg.h>
#endif

#if defined(__linux__)
#include <fstream>
#endif

// Targeted imports
using fat_p::areEqual;
using fat_p::HybridComparisonPolicy;
using fat_p::kStopOnFirstError;
using fat_p::registerAnyType;
using fat_p::RelativeComparisonPolicy;
using fat_p::StandardComparisonPolicy;
using fat_p::UlpComparisonPolicy;

using fat_p::testing::BenchmarkStats;
using fat_p::testing::DoNotOptimize;
using fat_p::testing::format_time;
using fat_p::testing::measure_perf_stats;

// ============================================================================
// Platform Configuration
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
static constexpr size_t DEFAULT_BATCHES = 15;
#else
static constexpr size_t DEFAULT_BATCHES = 30;
#endif

// ============================================================================
// CPU Frequency Monitoring (Shared)
// ============================================================================

void print_cpu_context()
{
    std::cout << "  ";
    fat_p::bench::print_cpu_context(std::cout);
}

// ============================================================================
// BenchmarkScope (Windows Priority/Affinity)
// ============================================================================

// Use shared BenchmarkScope from FatPBenchmarkRunner.h
using fat_p::bench::BenchmarkScope;

// ============================================================================
// Extended Statistics
// ============================================================================

struct ExtendedStats
{
    double median_ms = 0;
    double mean_ms = 0;
    double stddev_ms = 0;
    double ci95_low_ms = 0;
    double ci95_high_ms = 0;
    double min_ms = 0;
    double max_ms = 0;
    size_t batches = 0;

    static ExtendedStats from(const BenchmarkStats& bs, size_t batch_count)
    {
        ExtendedStats es;
        es.median_ms = bs.median_ms;
        es.mean_ms = bs.mean_ms;
        es.stddev_ms = bs.stddev_ms;
        es.min_ms = bs.min_ms;
        es.max_ms = bs.max_ms;
        es.batches = batch_count;
        if (batch_count > 1)
        {
            double se = bs.stddev_ms / std::sqrt(static_cast<double>(batch_count));
            es.ci95_low_ms = bs.mean_ms - 1.96 * se;
            es.ci95_high_ms = bs.mean_ms + 1.96 * se;
        }
        else { es.ci95_low_ms = es.ci95_high_ms = bs.mean_ms; }
        return es;
    }
};

// ============================================================================
// Configuration
// ============================================================================

struct BenchConfig { std::size_t targetWork; std::size_t batches; bool verboseStats; bool noScope; };

[[nodiscard]] std::size_t parseSizeT(const char* env, std::size_t fallback)
{
    if (!env || !*env) return fallback;
    const char* p = env;
    while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    if (*p == '-') return fallback;
    errno = 0;
    char* end = nullptr;
    auto v = std::strtoull(p, &end, 10);
    if (end == p || *end || errno == ERANGE || v > std::numeric_limits<std::size_t>::max())
        return fallback;
    return static_cast<std::size_t>(v);
}

[[nodiscard]] bool parseBool(const char* env, bool fallback)
{
    if (!env || !*env) return fallback;
    return env[0] == '1' || env[0] == 't' || env[0] == 'T' || env[0] == 'y' || env[0] == 'Y';
}

[[nodiscard]] bool hasEnvVar(const char* name)
{
    const char* val = std::getenv(name);
    return val != nullptr && val[0] != '\0';
}

[[nodiscard]] BenchConfig loadConfig()
{
    return { parseSizeT(std::getenv("FATP_BENCH_TARGET_WORK"), 5'000'000),
             parseSizeT(std::getenv("FATP_BENCH_BATCHES"), DEFAULT_BATCHES),
             parseBool(std::getenv("FATP_BENCH_VERBOSE_STATS"), false),
             hasEnvVar("FATP_BENCH_NO_SCOPE") };
}

[[nodiscard]] std::size_t computeIterations(std::size_t n, std::size_t target)
{
    return n == 0 ? 1 : std::max<std::size_t>(1, target / n);
}

// ============================================================================
// Output Formatting
// ============================================================================

void printBanner(const char* title)
{
    std::cout << "\n" << fat_p::testing::colors::cyan() << fat_p::testing::colors::bold()
              << title << fat_p::testing::colors::reset() << "\n"
              << std::string(std::strlen(title), '=') << "\n\n";
}

void printSection(const char* title)
{
    std::cout << fat_p::testing::colors::cyan() << title
              << fat_p::testing::colors::reset() << "\n"
              << std::string(std::strlen(title), '-') << "\n";
}

void printTableHeader()
{
    std::cout << std::left << std::setw(24) << "Case"
              << std::right << std::setw(14) << "Fat-P"
              << std::setw(14) << "Baseline" << std::setw(10) << "Speedup" << "\n"
              << std::string(62, '-') << "\n";
}

void printTableHeaderStats()
{
    std::cout << std::left << std::setw(20) << "Case"
              << std::right << std::setw(12) << "Median"
              << std::setw(12) << "Mean"
              << std::setw(12) << "Stddev"
              << "  CI95\n" << std::string(76, '-') << "\n";
}

void printRow(const std::string& label, double fatpTime, double baselineTime)
{
    double speedup = (fatpTime > 0 && baselineTime > 0) ? (baselineTime / fatpTime) : 0.0;
    std::cout << std::left << std::setw(24) << label
              << std::right << std::setw(14) << format_time(fatpTime)
              << std::setw(14) << format_time(baselineTime)
              << std::setw(10) << std::fixed << std::setprecision(2) << speedup << "x\n";
}

void printRowStats(const std::string& label, const ExtendedStats& s)
{
    std::ostringstream ci;
    ci << "[" << format_time(s.ci95_low_ms) << ", " << format_time(s.ci95_high_ms) << "]";
    
    std::cout << std::left << std::setw(20) << label
              << std::right << std::setw(12) << format_time(s.median_ms)
              << std::setw(12) << format_time(s.mean_ms)
              << std::setw(12) << format_time(s.stddev_ms)
              << "  " << ci.str() << "\n";
}

void printRowPair(const std::string& label, const ExtendedStats& fatp, const ExtendedStats& base)
{
    double speedup = (fatp.median_ms > 0 && base.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
    std::cout << std::left << std::setw(24) << label
              << std::right << std::setw(14) << format_time(fatp.median_ms)
              << std::setw(14) << format_time(base.median_ms)
              << std::setw(10) << std::fixed << std::setprecision(2) << speedup << "x\n";
}

void printPlatformInfo()
{
    std::cout << fat_p::testing::colors::bold() << "Platform:" << fat_p::testing::colors::reset() << "\n";
#if defined(__linux__)
    std::cout << "  OS: Linux\n";
#elif defined(_WIN32)
    std::cout << "  OS: Windows\n";
#elif defined(__APPLE__)
    std::cout << "  OS: macOS\n";
#else
    std::cout << "  OS: Unknown\n";
#endif
#if defined(__clang__)
    std::cout << "  Compiler: Clang " << __clang_major__ << "." << __clang_minor__ << "\n";
#elif defined(__GNUC__)
    std::cout << "  Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "\n";
#elif defined(_MSC_VER)
    std::cout << "  Compiler: MSVC " << _MSC_VER << "\n";
#endif
#ifdef NDEBUG
    std::cout << "  Build: Release\n";
#else
    std::cout << "  Build: Debug (benchmarks unreliable)\n";
#endif
    print_cpu_context();
    std::cout << "\n";
}

template <typename Func>
[[nodiscard]] ExtendedStats measure(Func&& func, std::size_t iters, std::size_t batches)
{
    auto stats = measure_perf_stats(std::forward<Func>(func), iters, batches);
    return ExtendedStats::from(stats, batches);
}

// ============================================================================
// Data Generation
// ============================================================================

std::vector<double> generateDoubles(std::size_t n, std::uint32_t seed = 42)
{
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);
    std::vector<double> v(n);
    for (auto& x : v) x = dist(rng);
    return v;
}

std::vector<float> generateFloats(std::size_t n, std::uint32_t seed = 42)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1e3f, 1e3f);
    std::vector<float> v(n);
    for (auto& x : v) x = dist(rng);
    return v;
}

std::tuple<double,double,double> generateTuple3(std::uint32_t seed)
{
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);
    return {dist(rng), dist(rng), dist(rng)};
}

std::map<std::string, std::vector<double>> generateOrderedMap(std::size_t keys, std::size_t vals, std::uint32_t seed = 42)
{
    std::map<std::string, std::vector<double>> m;
    for (std::size_t i = 0; i < keys; ++i)
        m["key_" + std::to_string(i)] = generateDoubles(vals, seed + static_cast<uint32_t>(i * 17));
    return m;
}

std::unordered_map<std::string, std::vector<double>> generateUnorderedMap(std::size_t keys, std::size_t vals, std::uint32_t seed = 42)
{
    std::unordered_map<std::string, std::vector<double>> m;
    m.reserve(keys);
    for (std::size_t i = 0; i < keys; ++i)
        m["key_" + std::to_string(i)] = generateDoubles(vals, seed + static_cast<uint32_t>(i * 23));
    return m;
}

std::unordered_map<std::string, double> generateUnorderedMapScalar(std::size_t keys, std::uint32_t seed = 42)
{
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);
    std::unordered_map<std::string, double> m;
    m.reserve(keys);
    for (std::size_t i = 0; i < keys; ++i) m["key_" + std::to_string(i)] = dist(rng);
    return m;
}

// ============================================================================
// Manual Baselines
// ============================================================================

bool manualVectorEpsilon(const std::vector<double>& a, const std::vector<double>& b, double eps)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::fabs(a[i] - b[i]) > eps) return false;
    return true;
}

bool manualMapNestedEpsilon(const std::map<std::string, std::vector<double>>& a,
                            const std::map<std::string, std::vector<double>>& b, double eps)
{
    if (a.size() != b.size()) return false;
    auto ita = a.begin(), itb = b.begin();
    for (; ita != a.end(); ++ita, ++itb)
    {
        if (ita->first != itb->first) return false;
        if (!manualVectorEpsilon(ita->second, itb->second, eps)) return false;
    }
    return true;
}

bool manualUnorderedMapNestedEpsilon(const std::unordered_map<std::string, std::vector<double>>& a,
                                     const std::unordered_map<std::string, std::vector<double>>& b, double eps)
{
    if (a.size() != b.size()) return false;
    for (const auto& [key, vec] : a)
    {
        auto it = b.find(key);
        if (it == b.end() || !manualVectorEpsilon(vec, it->second, eps)) return false;
    }
    return true;
}

bool manualUnorderedMapScalarEpsilon(const std::unordered_map<std::string, double>& a,
                                     const std::unordered_map<std::string, double>& b, double eps)
{
    if (a.size() != b.size()) return false;
    for (const auto& [key, val] : a)
    {
        auto it = b.find(key);
        if (it == b.end() || std::fabs(val - it->second) > eps) return false;
    }
    return true;
}

bool manualTupleEpsilon(const std::tuple<double,double,double>& a,
                        const std::tuple<double,double,double>& b, double eps)
{
    return std::fabs(std::get<0>(a) - std::get<0>(b)) <= eps &&
           std::fabs(std::get<1>(a) - std::get<1>(b)) <= eps &&
           std::fabs(std::get<2>(a) - std::get<2>(b)) <= eps;
}

// Fat-P semantics control
[[nodiscard]] inline bool manualFatPSemanticsDoubleEpsilon(double a, double b, double eps)
{
    if (a == b) return true;
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isnan(a) || std::isnan(b)) return false;
    if (std::isinf(a) || std::isinf(b)) return false;
    return std::fabs(a - b) <= eps;
}

bool manualVectorFatPSemanticsEpsilon(const std::vector<double>& a, const std::vector<double>& b, double eps)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!manualFatPSemanticsDoubleEpsilon(a[i], b[i], eps)) return false;
    return true;
}

bool manualTupleFatPSemanticsEpsilon(const std::tuple<double,double,double>& a,
                                     const std::tuple<double,double,double>& b, double eps)
{
    return manualFatPSemanticsDoubleEpsilon(std::get<0>(a), std::get<0>(b), eps) &&
           manualFatPSemanticsDoubleEpsilon(std::get<1>(a), std::get<1>(b), eps) &&
           manualFatPSemanticsDoubleEpsilon(std::get<2>(a), std::get<2>(b), eps);
}

void registerExtraAnyTypes() { registerAnyType<std::vector<std::any>>(); }

// ============================================================================
// Benchmark Functions
// ============================================================================

void benchVectorDouble(const BenchConfig& cfg)
{
    printSection("vector<double> vs Manual Epsilon Loop");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::size_t> sizes = {64, 256, 1024, 4096, 16384};
    
    for (std::size_t n : sizes)
    {
        auto v = generateDoubles(n), v2 = v;
        std::size_t iters = computeIterations(n, cfg.targetWork);
        auto fatp = measure([&]() { DoNotOptimize(areEqual(v, v2, kEps)); }, iters, cfg.batches);
        auto base = measure([&]() { DoNotOptimize(manualVectorEpsilon(v, v2, kEps)); }, iters, cfg.batches);
        
        std::cout << "N=" << n << ":\n";
        printTableHeaderStats();
        printRowStats("Fat-P", fatp);
        printRowStats("Baseline", base);
        double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
}

void benchVectorDoubleFatPSemanticsControl(const BenchConfig& cfg)
{
    printSection("vector<double> vs Manual Fat-P Semantics Loop (NaN/Inf aware)");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::size_t> sizes = {64, 256, 1024, 4096, 16384};
    
    for (std::size_t n : sizes)
    {
        auto v = generateDoubles(n), v2 = v;
        std::size_t iters = computeIterations(n, cfg.targetWork);
        auto fatp = measure([&]() { DoNotOptimize(areEqual(v, v2, kEps)); }, iters, cfg.batches);
        auto base = measure([&]() { DoNotOptimize(manualVectorFatPSemanticsEpsilon(v, v2, kEps)); }, iters, cfg.batches);
        
        std::cout << "N=" << n << ":\n";
        printTableHeaderStats();
        printRowStats("Fat-P", fatp);
        printRowStats("Semantics baseline", base);
        double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
}

void benchVectorDoubleVsStdEqual(const BenchConfig& cfg)
{
    printSection("vector<double> Fat-P vs std::equal (exact, no epsilon)");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::size_t> sizes = {1000, 10000, 100000};
    
    for (std::size_t n : sizes)
    {
        auto v = generateDoubles(n), v2 = v;
        std::size_t iters = computeIterations(n, cfg.targetWork);
        auto fatp = measure([&]() { DoNotOptimize(areEqual(v, v2, kEps)); }, iters, cfg.batches);
        auto base = measure([&]() { DoNotOptimize(std::equal(v.begin(), v.end(), v2.begin())); }, iters, cfg.batches);
        
        std::cout << "N=" << n << ":\n";
        printTableHeaderStats();
        printRowStats("Fat-P", fatp);
        printRowStats("std::equal", base);
        double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
    std::cout << "Note: std::equal uses exact bitwise comparison (no epsilon math)\n\n";
}

void benchMapNested(const BenchConfig& cfg)
{
    printSection("map<string, vector<double>> vs Manual Nested Loop");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::pair<std::size_t, std::size_t>> shapes = {{64, 32}, {128, 64}, {256, 64}};
    
    for (auto [keys, vals] : shapes)
    {
        auto m = generateOrderedMap(keys, vals), m2 = m;
        std::size_t iters = computeIterations(keys * vals, cfg.targetWork);
        auto fatp = measure([&]() { DoNotOptimize(areEqual(m, m2, kEps)); }, iters, cfg.batches);
        auto base = measure([&]() { DoNotOptimize(manualMapNestedEpsilon(m, m2, kEps)); }, iters, cfg.batches);
        
        std::cout << "K=" << keys << ", V=" << vals << ":\n";
        printTableHeaderStats();
        printRowStats("Fat-P", fatp);
        printRowStats("Manual nested", base);
        double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
}

void benchUnorderedMapNested(const BenchConfig& cfg)
{
    printSection("unordered_map<string, vector<double>> vs Manual Find+Loop");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::pair<std::size_t, std::size_t>> shapes = {{64, 32}, {128, 64}, {256, 64}};
    
    for (auto [keys, vals] : shapes)
    {
        auto m = generateUnorderedMap(keys, vals), m2 = m;
        std::size_t iters = computeIterations(keys * vals, cfg.targetWork);
        auto fatp = measure([&]() { DoNotOptimize(areEqual(m, m2, kEps)); }, iters, cfg.batches);
        auto base = measure([&]() { DoNotOptimize(manualUnorderedMapNestedEpsilon(m, m2, kEps)); }, iters, cfg.batches);
        
        std::cout << "K=" << keys << ", V=" << vals << ":\n";
        printTableHeaderStats();
        printRowStats("Fat-P", fatp);
        printRowStats("Manual find+loop", base);
        double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
}

void benchUnorderedMapScalar(const BenchConfig& cfg)
{
    printSection("unordered_map<string, double> vs Manual Find");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::size_t> keyCounts = {64, 256, 1024, 4096};
    
    for (std::size_t keys : keyCounts)
    {
        auto m = generateUnorderedMapScalar(keys), m2 = m;
        std::size_t iters = computeIterations(keys, cfg.targetWork);
        auto fatp = measure([&]() { DoNotOptimize(areEqual(m, m2, kEps)); }, iters, cfg.batches);
        auto base = measure([&]() { DoNotOptimize(manualUnorderedMapScalarEpsilon(m, m2, kEps)); }, iters, cfg.batches);
        
        std::cout << "K=" << keys << ":\n";
        printTableHeaderStats();
        printRowStats("Fat-P", fatp);
        printRowStats("Manual find", base);
        double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
}

void benchTuple(const BenchConfig& cfg)
{
    printSection("tuple<double,double,double> vs Manual 3 Compares");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    constexpr std::size_t kCount = 64;
    std::array<std::tuple<double,double,double>, kCount> tuplesA, tuplesB;
    for (std::size_t i = 0; i < kCount; ++i) { tuplesA[i] = generateTuple3(static_cast<uint32_t>(i * 97 + 1)); tuplesB[i] = tuplesA[i]; }
    std::size_t iters = cfg.targetWork / 3;
    volatile std::size_t idx = 0;
    auto fatp = measure([&]() { std::size_t i = idx; DoNotOptimize(areEqual(tuplesA[i], tuplesB[(i+32)%kCount], kEps)); idx = (i+1)%kCount; }, iters, cfg.batches);
    idx = 0;
    auto base = measure([&]() { std::size_t i = idx; DoNotOptimize(manualTupleEpsilon(tuplesA[i], tuplesB[(i+32)%kCount], kEps)); idx = (i+1)%kCount; }, iters, cfg.batches);
    
    std::cout << "3 doubles (from " << kCount << " tuples):\n";
    printTableHeaderStats();
    printRowStats("Fat-P", fatp);
    printRowStats("Manual 3-cmp", base);
    double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
}

void benchTupleFatPSemanticsControl(const BenchConfig& cfg)
{
    printSection("tuple<double,double,double> vs Manual Fat-P Semantics");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    constexpr std::size_t kCount = 64;
    std::array<std::tuple<double,double,double>, kCount> tuplesA, tuplesB;
    for (std::size_t i = 0; i < kCount; ++i) { tuplesA[i] = generateTuple3(static_cast<uint32_t>(i * 97 + 1)); tuplesB[i] = tuplesA[i]; }
    std::size_t iters = cfg.targetWork / 3;
    volatile std::size_t idx = 0;
    auto fatp = measure([&]() { std::size_t i = idx; DoNotOptimize(areEqual(tuplesA[i], tuplesB[(i+32)%kCount], kEps)); idx = (i+1)%kCount; }, iters, cfg.batches);
    idx = 0;
    auto base = measure([&]() { std::size_t i = idx; DoNotOptimize(manualTupleFatPSemanticsEpsilon(tuplesA[i], tuplesB[(i+32)%kCount], kEps)); idx = (i+1)%kCount; }, iters, cfg.batches);
    
    std::cout << "3 doubles (from " << kCount << " tuples):\n";
    printTableHeaderStats();
    printRowStats("Fat-P", fatp);
    printRowStats("Semantics baseline", base);
    double speedup = (fatp.median_ms > 0) ? (base.median_ms / fatp.median_ms) : 0.0;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
}

void benchPolicies(const BenchConfig& cfg)
{
    printSection("Policy Comparison: vector<double>[10000]");
    print_cpu_context();
    constexpr std::size_t N = 10000;
    constexpr double kEps = 1e-9;
    auto v = generateDoubles(N), v2 = v;
    auto vf = generateFloats(N), vf2 = vf;
    std::size_t iters = computeIterations(N, cfg.targetWork);
    
    printTableHeaderStats();
    
    auto s1 = measure([&]() { DoNotOptimize(areEqual<StandardComparisonPolicy>(v, v2, kEps)); }, iters, cfg.batches);
    printRowStats("Standard", s1);
    
    auto s2 = measure([&]() { DoNotOptimize(areEqual<RelativeComparisonPolicy>(v, v2, kEps)); }, iters, cfg.batches);
    printRowStats("Relative", s2);
    
    auto s3 = measure([&]() { DoNotOptimize(areEqual<HybridComparisonPolicy>(v, v2, kEps, kEps)); }, iters, cfg.batches);
    printRowStats("Hybrid", s3);
    
    auto s4 = measure([&]() { DoNotOptimize(areEqual<UlpComparisonPolicy>(vf, vf2, 4.0f)); }, iters, cfg.batches);
    printRowStats("ULP (float)", s4);
    
    std::cout << "\nNote: ULP row uses vector<float>; others use vector<double>.\n\n";
}

void benchMismatchDetection(const BenchConfig& cfg)
{
    printSection("Mismatch Detection (Early Exit Behavior)");
    print_cpu_context();
    if constexpr (kStopOnFirstError)
    {
        constexpr double kEps = 1e-9;
        constexpr std::size_t N = 10000;
        auto v = generateDoubles(N), v2 = v;
        std::size_t iters = computeIterations(N, cfg.targetWork);
        
        printTableHeaderStats();
        for (double pos : {0.01, 0.10, 0.50, 0.90, 1.0})
        {
            auto vMod = v;
            std::size_t idx = static_cast<std::size_t>(pos * (N - 1));
            vMod[idx] += kEps * 1000;
            auto s = measure([&]() { DoNotOptimize(areEqual(vMod, v2, kEps)); }, iters, cfg.batches);
            printRowStats(std::to_string(static_cast<int>(pos*100)) + "% (" + std::to_string(idx) + ")", s);
        }
        std::cout << "\n";
    }
    else
    {
        std::cout << "Skipped: kStopOnFirstError=false (no early-exit behavior to measure).\n\n";
    }
}

void benchSizeScaling(const BenchConfig& cfg)
{
    printSection("Size Scaling with Per-Element Overhead");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::size_t> sizes = {10, 100, 1000, 10000, 100000};
    
    for (std::size_t n : sizes)
    {
        auto v = generateDoubles(n), v2 = v;
        std::size_t iters = computeIterations(n, cfg.targetWork);
        auto base = measure([&]() { DoNotOptimize(manualVectorEpsilon(v, v2, kEps)); }, iters, cfg.batches);
        auto fatp = measure([&]() { DoNotOptimize(areEqual(v, v2, kEps)); }, iters, cfg.batches);
        
        double speedup = fatp.median_ms > 0 ? base.median_ms / fatp.median_ms : 0;
        double perElem = (fatp.median_ms - base.median_ms) / static_cast<double>(n);
        
        std::cout << "N=" << n << ":\n";
        printTableHeaderStats();
        printRowStats("Fat-P", fatp);
        printRowStats("Manual", base);
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x"
                  << "  |  Per-elem overhead: " << format_time(perElem) << "\n\n";
    }
}

void benchNestedContainers(const BenchConfig& cfg)
{
    printSection("Nested Container Overhead");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    constexpr std::size_t kTotal = 1000;
    std::size_t iters = computeIterations(kTotal, cfg.targetWork);
    
    printTableHeaderStats();
    
    { auto v = generateDoubles(kTotal), v2 = v;
      auto s = measure([&]() { DoNotOptimize(areEqual(v, v2, kEps)); }, iters, cfg.batches);
      printRowStats("Flat [1000]", s);
      std::cout << "  Per-element: " << format_time(s.median_ms / kTotal) << "\n"; }
    
    { std::vector<std::vector<double>> vv(10);
      for (auto& inner : vv) inner = generateDoubles(100);
      auto vv2 = vv;
      auto s = measure([&]() { DoNotOptimize(areEqual(vv, vv2, kEps)); }, iters, cfg.batches);
      printRowStats("2-level [10x100]", s);
      std::cout << "  Per-element: " << format_time(s.median_ms / kTotal) << "\n"; }
    
    { std::vector<std::vector<std::vector<double>>> vvv(5);
      for (auto& mid : vvv) { mid.resize(10); for (auto& inner : mid) inner = generateDoubles(20); }
      auto vvv2 = vvv;
      auto s = measure([&]() { DoNotOptimize(areEqual(vvv, vvv2, kEps)); }, iters, cfg.batches);
      printRowStats("3-level [5x10x20]", s);
      std::cout << "  Per-element: " << format_time(s.median_ms / kTotal) << "\n"; }
    
    std::cout << "\nNote: All structures contain " << kTotal << " double elements total.\n\n";
}

// ============================================================================
// EqualityAny Benchmarks
// ============================================================================

void benchAnyScalar(const BenchConfig& cfg)
{
    printSection("std::any(double): Registry vs Direct");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    double a = 3.14159, b = a;
    std::any anyA = a, anyB = b;
    std::size_t iters = cfg.targetWork;
    
    auto reg = measure([&]() { DoNotOptimize(fat_p::areEqual(anyA, anyB, kEps)); }, iters, cfg.batches);
    auto typed = measure([&]() { DoNotOptimize(areEqual(std::any_cast<double>(anyA), std::any_cast<double>(anyB), kEps)); }, iters, cfg.batches);
    auto direct = measure([&]() { DoNotOptimize(areEqual(a, b, kEps)); }, iters, cfg.batches);
    
    printTableHeaderStats();
    printRowStats("any registry", reg);
    printRowStats("any_cast+typed", typed);
    printRowStats("direct double", direct);
    std::cout << "\n";
}

void benchAnyVectorDouble(const BenchConfig& cfg)
{
    printSection("std::any(vector<double>) vs Direct");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::size_t> sizes = {64, 256, 1024, 4096};
    
    for (std::size_t n : sizes)
    {
        auto v = generateDoubles(n), v2 = v;
        std::any anyV = v, anyV2 = v2;
        std::size_t iters = computeIterations(n, cfg.targetWork);
        auto any = measure([&]() { DoNotOptimize(fat_p::areEqual(anyV, anyV2, kEps)); }, iters, cfg.batches);
        auto direct = measure([&]() { DoNotOptimize(areEqual(v, v2, kEps)); }, iters, cfg.batches);
        
        std::cout << "N=" << n << ":\n";
        printTableHeaderStats();
        printRowStats("any(vector)", any);
        printRowStats("direct vector", direct);
        double speedup = (any.median_ms > 0) ? (direct.median_ms / any.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
}

void benchAnyVectorAny(const BenchConfig& cfg)
{
    printSection("std::any(vector<any>) vs Direct vector<any>");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::vector<std::size_t> sizes = {32, 128, 512, 2048};
    
    for (std::size_t n : sizes)
    {
        std::vector<std::any> va(n);
        for (std::size_t i = 0; i < n; ++i) va[i] = static_cast<double>(i) * 1.1;
        auto va2 = va;
        std::any anyVa = va, anyVa2 = va2;
        std::size_t iters = computeIterations(n, cfg.targetWork);
        auto any = measure([&]() { DoNotOptimize(fat_p::areEqual(anyVa, anyVa2, kEps)); }, iters, cfg.batches);
        auto direct = measure([&]() { DoNotOptimize(fat_p::areEqual(va, va2, kEps)); }, iters, cfg.batches);
        
        std::cout << "N=" << n << ":\n";
        printTableHeaderStats();
        printRowStats("any(vec<any>)", any);
        printRowStats("direct vec<any>", direct);
        double speedup = (any.median_ms > 0) ? (direct.median_ms / any.median_ms) : 0.0;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";
    }
}

void benchAnyEdgeCases(const BenchConfig& cfg)
{
    printSection("std::any Edge Cases");
    print_cpu_context();
    constexpr double kEps = 1e-9;
    std::size_t iters = cfg.targetWork;
    
    printTableHeaderStats();
    
    { std::any a, b;
      auto s = measure([&]() { DoNotOptimize(fat_p::areEqual(a, b, kEps)); }, iters, cfg.batches);
      printRowStats("Both empty", s); }
    
    { std::any a = 1.0, b = 1;
      auto s = measure([&]() { DoNotOptimize(fat_p::areEqual(a, b, kEps)); }, iters, cfg.batches);
      printRowStats("Type mismatch", s); }
    
    { std::any inner = 42.0; std::any a = inner, b = inner;
      auto s = measure([&]() { DoNotOptimize(fat_p::areEqual(a, b, kEps)); }, iters, cfg.batches);
      printRowStats("Nested std::any", s); }
    
    std::cout << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    BenchConfig cfg = loadConfig();
    
    // Apply benchmark scope (Windows priority/affinity) unless disabled
    BenchmarkScope scope(!cfg.noScope);
    
    printBanner("EqualityComparisons & EqualityAny Benchmark Suite");
    printPlatformInfo();
    
    std::cout << "Configuration:\n"
              << "  FATP_BENCH_TARGET_WORK = " << cfg.targetWork << "\n"
              << "  FATP_BENCH_BATCHES = " << cfg.batches << "\n"
              << "  FATP_BENCH_VERBOSE_STATS = " << (cfg.verboseStats ? "true" : "false") << "\n"
              << "  FATP_BENCH_NO_SCOPE = " << (cfg.noScope ? "true" : "false") << "\n"
              << "  Primary metric: median (robust to outliers)\n\n";
    
    fat_p::detail::ensureAnyEqualityRegistered();
    registerExtraAnyTypes();
    
    printBanner("EqualityComparisons Benchmarks");
    benchVectorDouble(cfg);
    benchVectorDoubleFatPSemanticsControl(cfg);
    benchVectorDoubleVsStdEqual(cfg);
    benchMapNested(cfg);
    benchUnorderedMapNested(cfg);
    benchUnorderedMapScalar(cfg);
    benchTuple(cfg);
    benchTupleFatPSemanticsControl(cfg);
    benchPolicies(cfg);
    benchMismatchDetection(cfg);
    benchSizeScaling(cfg);
    benchNestedContainers(cfg);
    
    printBanner("EqualityAny Benchmarks");
    benchAnyScalar(cfg);
    benchAnyVectorDouble(cfg);
    benchAnyVectorAny(cfg);
    benchAnyEdgeCases(cfg);
    
    std::cout << fat_p::testing::colors::green() << "Benchmark complete."
              << fat_p::testing::colors::reset() << "\n";
    return 0;
}
