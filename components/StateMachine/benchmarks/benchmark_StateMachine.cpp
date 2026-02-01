// benchmark_StateMachine.cpp
//
// FAT-P StateMachine comprehensive benchmark suite.
// Validates O(1) transition performance and compares policy overhead.
//
// Architecture: Round-robin execution with randomized order per run.
// This ensures all libraries observe the same distribution of machine states,
// eliminating drift-induced unfairness.
//
// Design Invariants:
//   1. Each measured run executes exactly one timed iteration per library.
//   2. Library execution order is randomized per run.
//   3. Setup and teardown occur outside timed regions.
//   4. All libraries observe the same distribution of machine states.
//   5. Medians are the primary reported statistic.
//
// Competitors:
//   - fat_p::StateMachine<AnyToAnyTransitionPolicy> (baseline fat_p)
//   - fat_p::StateMachine<StrictTransitionPolicy> (policy overhead measurement)
//   - Manual enum-switch state machine (classic C++ pattern)
//   - Manual function-pointer table state machine (table-driven dispatch)
//   - std::variant + std::visit (C++17 standard baseline)
//   - [Boost::ext].SML (optional, header-only, popular modern FSM)
//   - Boost.MSM (optional, full Boost, canonical state machine library)
//   - TinyFSM (optional, header-only, minimal footprint)
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native -I../fat_p benchmark_StateMachine.cpp
//       -o bench_sm
//   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_StateMachine.cpp /link advapi32.lib
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
//   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
//   FATP_BENCH_SEED          - RNG seed (default: 12345)
//   FATP_BENCH_TARGET_WORK   - Target ops per batch (default: 5,000,000)
//   FATP_BENCH_MIN_BATCH_MS  - Min batch duration (default: 50)
//   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
//   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
//   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
//   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
//   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
//   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
//
// Run:
//   ./bench_sm
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_sm

// ============================================================================
// MSVC Compatibility - Must be before ANY includes
// ============================================================================
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#pragma warning(push)
#pragma warning(disable : 4996)  // getenv and other "unsafe" CRT functions
#endif

/*
FATP_META:
  meta_version: 1
  component: StateMachine
  file_role: benchmark
  path: components/StateMachine/benchmarks/benchmark_StateMachine.cpp
  layer: Testing
  namespace:
    - fat_p
    - fat_p::bench
  summary: "Performance benchmarks for StateMachine component."
  api_stability: stable
  related:
    docs_search: "StateMachine"
    headers:
      - include/fat_p/StateMachine.h
      - include/fat_p/FatPBenchmarkRunner.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 5
    defines_unprefixed: 3
    undefs_total: 0
    includes_windows_h: false
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
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "StateMachine.h"

// ============================================================================
// Optional Competitor Libraries
// ============================================================================

// [Boost::ext].SML - Popular header-only state machine library
// https://github.com/boost-ext/sml
// Rationale: Most popular modern C++ state machine library
#if __has_include(<boost/sml.hpp>)
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <boost/sml.hpp>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#define HAS_BOOST_SML 1
#else
#define HAS_BOOST_SML 0
#endif

// Boost.MSM - Full Boost Meta State Machine library
// https://www.boost.org/doc/libs/release/libs/msm/
// Rationale: Canonical Boost state machine, widely deployed reference point
#ifndef USE_BOOST_MSM
#define USE_BOOST_MSM 1
#endif

#if __has_include(<boost/msm/back/state_machine.hpp>)
#define HAS_BOOST_MSM_HEADERS 1
#else
#define HAS_BOOST_MSM_HEADERS 0
#endif

#if USE_BOOST_MSM && HAS_BOOST_MSM_HEADERS
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4244 4267 4459)  // unreferenced parameter, conversion, shadowing
#endif
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/mpl/vector/vector50.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#define HAS_BOOST_MSM 1
#else
#define HAS_BOOST_MSM 0
#endif

// TinyFSM - Lightweight header-only FSM
// https://github.com/digint/tinyfsm
// Rationale: Minimal footprint alternative, popular in embedded
#if __has_include(<tinyfsm/tinyfsm.hpp>)
#include <tinyfsm/tinyfsm.hpp>
#define HAS_TINYFSM 1
#elif __has_include("tinyfsm.hpp")
#include "tinyfsm.hpp"
#define HAS_TINYFSM 1
#else
#define HAS_TINYFSM 0
#endif

// ============================================================================
// TinyFSM State Machine Implementation (Outside anonymous namespace)
// FSM_INITIAL_STATE macro requires global scope for template specialization
// ============================================================================

#if HAS_TINYFSM
namespace benchmark_tinyfsm
{

// Forward declarations
struct TinyS0;
struct TinyS1;
struct TinyS2;
struct TinyS3;

// Events
struct EvToS0 : ::tinyfsm::Event {};
struct EvToS1 : ::tinyfsm::Event {};
struct EvToS2 : ::tinyfsm::Event {};
struct EvToS3 : ::tinyfsm::Event {};

// Context for counting hooks
struct TinyContext
{
    std::uint64_t mTransitionCount = 0;
};

// Shared context pointer (TinyFSM uses static state, so we use a global)
inline TinyContext* g_tinyCtx = nullptr;

// Base FSM class with counting hooks
class TinyFsm4Cnt : public ::tinyfsm::Fsm<TinyFsm4Cnt>
{
public:
    virtual void react(EvToS0 const&) {}
    virtual void react(EvToS1 const&) {}
    virtual void react(EvToS2 const&) {}
    virtual void react(EvToS3 const&) {}

    virtual void entry() { if (g_tinyCtx) ++g_tinyCtx->mTransitionCount; }
    virtual void exit() {}
};

// State definitions with counting hooks
struct TinyS0 : TinyFsm4Cnt
{
    void react(EvToS1 const&) override { transit<TinyS1>(); }
    void react(EvToS2 const&) override { transit<TinyS2>(); }
    void react(EvToS3 const&) override { transit<TinyS3>(); }
};

struct TinyS1 : TinyFsm4Cnt
{
    void react(EvToS0 const&) override { transit<TinyS0>(); }
    void react(EvToS2 const&) override { transit<TinyS2>(); }
    void react(EvToS3 const&) override { transit<TinyS3>(); }
};

struct TinyS2 : TinyFsm4Cnt
{
    void react(EvToS0 const&) override { transit<TinyS0>(); }
    void react(EvToS1 const&) override { transit<TinyS1>(); }
    void react(EvToS3 const&) override { transit<TinyS3>(); }
};

struct TinyS3 : TinyFsm4Cnt
{
    void react(EvToS0 const&) override { transit<TinyS0>(); }
    void react(EvToS1 const&) override { transit<TinyS1>(); }
    void react(EvToS2 const&) override { transit<TinyS2>(); }
};

} // namespace benchmark_tinyfsm

// FSM_INITIAL_STATE must be at global scope
FSM_INITIAL_STATE(benchmark_tinyfsm::TinyFsm4Cnt, benchmark_tinyfsm::TinyS0)

namespace benchmark_tinyfsm
{

// Wrapper class for counting hooks
class TinyFsm4CntWrapper
{
    std::size_t mCurrentState = 0;

public:
    explicit TinyFsm4CntWrapper(TinyContext& ctx)
    {
        g_tinyCtx = &ctx;
        TinyFsm4Cnt::start();
    }

    ~TinyFsm4CntWrapper()
    {
        g_tinyCtx = nullptr;
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrentState == idx) return;
        switch (idx)
        {
        case 0: TinyFsm4Cnt::dispatch(EvToS0{}); break;
        case 1: TinyFsm4Cnt::dispatch(EvToS1{}); break;
        case 2: TinyFsm4Cnt::dispatch(EvToS2{}); break;
        case 3: TinyFsm4Cnt::dispatch(EvToS3{}); break;
        }
        mCurrentState = idx;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrentState; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrentState == s; }
};

// Empty hooks version
struct TinyEmptyS0;
struct TinyEmptyS1;
struct TinyEmptyS2;
struct TinyEmptyS3;

struct TinyEmptyContext
{
    std::uint64_t mDummy = 0;
};

inline TinyEmptyContext* g_tinyEmptyCtx = nullptr;

class TinyFsm4Empty : public ::tinyfsm::Fsm<TinyFsm4Empty>
{
public:
    virtual void react(EvToS0 const&) {}
    virtual void react(EvToS1 const&) {}
    virtual void react(EvToS2 const&) {}
    virtual void react(EvToS3 const&) {}

    virtual void entry() {}
    virtual void exit() {}
};

struct TinyEmptyS0 : TinyFsm4Empty
{
    void react(EvToS1 const&) override { transit<TinyEmptyS1>(); }
    void react(EvToS2 const&) override { transit<TinyEmptyS2>(); }
    void react(EvToS3 const&) override { transit<TinyEmptyS3>(); }
};

struct TinyEmptyS1 : TinyFsm4Empty
{
    void react(EvToS0 const&) override { transit<TinyEmptyS0>(); }
    void react(EvToS2 const&) override { transit<TinyEmptyS2>(); }
    void react(EvToS3 const&) override { transit<TinyEmptyS3>(); }
};

struct TinyEmptyS2 : TinyFsm4Empty
{
    void react(EvToS0 const&) override { transit<TinyEmptyS0>(); }
    void react(EvToS1 const&) override { transit<TinyEmptyS1>(); }
    void react(EvToS3 const&) override { transit<TinyEmptyS3>(); }
};

struct TinyEmptyS3 : TinyFsm4Empty
{
    void react(EvToS0 const&) override { transit<TinyEmptyS0>(); }
    void react(EvToS1 const&) override { transit<TinyEmptyS1>(); }
    void react(EvToS2 const&) override { transit<TinyEmptyS2>(); }
};

} // namespace benchmark_tinyfsm

FSM_INITIAL_STATE(benchmark_tinyfsm::TinyFsm4Empty, benchmark_tinyfsm::TinyEmptyS0)

namespace benchmark_tinyfsm
{

class TinyFsm4EmptyWrapper
{
    std::size_t mCurrentState = 0;

public:
    explicit TinyFsm4EmptyWrapper(TinyEmptyContext& ctx)
    {
        g_tinyEmptyCtx = &ctx;
        TinyFsm4Empty::start();
    }

    ~TinyFsm4EmptyWrapper()
    {
        g_tinyEmptyCtx = nullptr;
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrentState == idx) return;
        switch (idx)
        {
        case 0: TinyFsm4Empty::dispatch(EvToS0{}); break;
        case 1: TinyFsm4Empty::dispatch(EvToS1{}); break;
        case 2: TinyFsm4Empty::dispatch(EvToS2{}); break;
        case 3: TinyFsm4Empty::dispatch(EvToS3{}); break;
        }
        mCurrentState = idx;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrentState; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrentState == s; }
};

} // namespace benchmark_tinyfsm
#endif // HAS_TINYFSM

namespace
{

using namespace fat_p::bench;

// ============================================================================
// Global Configuration
// ============================================================================

static BenchConfig g_config;
static std::mt19937_64 g_rng;
static std::size_t g_targetWork = 5'000'000;

inline std::size_t WARMUP_RUNS() { return g_config.warmupRuns; }
inline std::size_t MEASURED_RUNS() { return g_config.measuredRuns; }
inline std::uint64_t SEED() { return g_config.seed; }
inline std::size_t TARGET_WORK() { return g_targetWork; }

volatile std::size_t g_benchmarkSink = 0;

// ============================================================================
// CSV/JSON Output Support
// ============================================================================

struct BenchmarkRecord
{
    std::string timestamp;
    std::string benchmarkName;
    std::string caseName;
    std::string libraryName;
    std::string unit;
    double median = 0;
    double mean = 0;
    double stddev = 0;
    double ci95Low = 0;
    double ci95High = 0;
    std::string cpuContext;
    std::string platform;
    std::uint64_t seed = 0;
    std::size_t warmupRuns = 0;
    std::size_t measuredRuns = 0;
    std::size_t targetWork = 0;
};

static std::vector<BenchmarkRecord> g_allRecords;

std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

std::string getPlatformString()
{
#if defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Unknown";
#endif
}

void writeCSV(const std::string& path)
{
    std::ofstream out(path);
    if (!out)
    {
        std::cerr << "[ERROR] Cannot open CSV file: " << path << "\n";
        return;
    }

    out << "timestamp,benchmark,case,library,unit,median,mean,stddev,"
        << "ci95_low,ci95_high,cpu_context,platform,seed,warmup,batches,target_work\n";

    for (const auto& r : g_allRecords)
    {
        out << r.timestamp << "," << r.benchmarkName << "," << r.caseName << ","
            << r.libraryName << "," << r.unit << "," << std::fixed << std::setprecision(4)
            << r.median << "," << r.mean << "," << r.stddev << "," << r.ci95Low << ","
            << r.ci95High << ",\"" << r.cpuContext << "\"," << r.platform << "," << r.seed
            << "," << r.warmupRuns << "," << r.measuredRuns << "," << r.targetWork << "\n";
    }

    std::cout << "[INFO] CSV written to: " << path << "\n";
}

void writeJSON(const std::string& path)
{
    std::ofstream out(path);
    if (!out)
    {
        std::cerr << "[ERROR] Cannot open JSON file: " << path << "\n";
        return;
    }

    out << "[\n";
    for (std::size_t i = 0; i < g_allRecords.size(); ++i)
    {
        const auto& r = g_allRecords[i];
        out << "  {\n";
        out << "    \"timestamp\": \"" << r.timestamp << "\",\n";
        out << "    \"benchmark\": \"" << r.benchmarkName << "\",\n";
        out << "    \"case\": \"" << r.caseName << "\",\n";
        out << "    \"library\": \"" << r.libraryName << "\",\n";
        out << "    \"unit\": \"" << r.unit << "\",\n";
        out << "    \"median\": " << std::fixed << std::setprecision(4) << r.median << ",\n";
        out << "    \"mean\": " << r.mean << ",\n";
        out << "    \"stddev\": " << r.stddev << ",\n";
        out << "    \"ci95_low\": " << r.ci95Low << ",\n";
        out << "    \"ci95_high\": " << r.ci95High << ",\n";
        out << "    \"cpu_context\": \"" << r.cpuContext << "\",\n";
        out << "    \"platform\": \"" << r.platform << "\",\n";
        out << "    \"seed\": " << r.seed << ",\n";
        out << "    \"warmup\": " << r.warmupRuns << ",\n";
        out << "    \"batches\": " << r.measuredRuns << ",\n";
        out << "    \"target_work\": " << r.targetWork << "\n";
        out << "  }" << (i + 1 < g_allRecords.size() ? "," : "") << "\n";
    }
    out << "]\n";

    std::cout << "[INFO] JSON written to: " << path << "\n";
}

// ============================================================================
// Statistics
// ============================================================================

struct Stats
{
    double median = 0.0;
    double mean = 0.0;
    double stddev = 0.0;
    double ci95Low = 0.0;
    double ci95High = 0.0;
};

Stats computeStats(std::vector<double>& samples)
{
    Stats s;
    if (samples.empty()) return s;

    std::sort(samples.begin(), samples.end());
    const std::size_t n = samples.size();

    s.median = (n % 2 == 0) ? (samples[n / 2 - 1] + samples[n / 2]) / 2.0 : samples[n / 2];
    s.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(n);

    double variance = 0.0;
    for (double v : samples) variance += (v - s.mean) * (v - s.mean);
    s.stddev = (n > 1) ? std::sqrt(variance / static_cast<double>(n - 1)) : 0.0;

    const double z = 1.96;
    const double margin = z * s.stddev / std::sqrt(static_cast<double>(n));
    s.ci95Low = s.mean - margin;
    s.ci95High = s.mean + margin;

    return s;
}

// ============================================================================
// Output Formatting
// ============================================================================

void printHeader(const std::string& title)
{
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

void printContractNote(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

void printResultHeader()
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(35) << std::left << "Implementation" << std::setw(12) << std::right
              << "Median" << std::setw(12) << "Mean" << std::setw(10) << "Stddev"
              << "   CI95\n";
    std::cout << std::string(79, '-') << "\n";
}

void printResult(const std::string& name, const Stats& s, const std::string& unit = "ns/op")
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(35) << std::left << name << std::setw(12) << std::right << s.median
              << std::setw(12) << s.mean << std::setw(10) << s.stddev << "  [" << std::setw(6)
              << s.ci95Low << ", " << std::setw(6) << s.ci95High << "] " << unit << "\n";

    if (s.stddev > s.median && s.median > 0)
    {
        std::cout << "  [NOTE] High variance (stddev " << s.stddev << " > median " << s.median
                  << ")\n";
    }
}

void recordResult(const std::string& benchName, const std::string& caseName,
                  const std::string& libName, const Stats& s, const std::string& cpuCtx)
{
    BenchmarkRecord rec;
    rec.timestamp = getCurrentTimestamp();
    rec.benchmarkName = benchName;
    rec.caseName = caseName;
    rec.libraryName = libName;
    rec.unit = "ns/op";
    rec.median = s.median;
    rec.mean = s.mean;
    rec.stddev = s.stddev;
    rec.ci95Low = s.ci95Low;
    rec.ci95High = s.ci95High;
    rec.cpuContext = cpuCtx;
    rec.platform = getPlatformString();
    rec.seed = g_config.seed;
    rec.warmupRuns = g_config.warmupRuns;
    rec.measuredRuns = g_config.measuredRuns;
    rec.targetWork = g_targetWork;
    g_allRecords.push_back(std::move(rec));
}

std::string captureCpuContext()
{
    std::ostringstream oss;
    print_cpu_context(oss);
    return oss.str();
}

// ============================================================================
// Transition Sequence Generation
// ============================================================================

template <std::size_t NumStates>
std::vector<std::size_t> generateTransitionSequence(std::size_t n, std::uint64_t seed)
{
    std::vector<std::size_t> seq(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::size_t> dist(0, NumStates - 1);
    for (std::size_t i = 0; i < n; ++i) seq[i] = dist(rng);
    return seq;
}

std::size_t countActualTransitions(const std::vector<std::size_t>& seq, std::size_t initialState)
{
    std::size_t count = 1;  // Initial entry
    std::size_t prev = initialState;
    for (std::size_t s : seq)
    {
        if (s != prev) ++count;
        prev = s;
    }
    return count;
}

// ============================================================================
// Context Types
// ============================================================================

struct CountingContext
{
    std::uint64_t mTransitionCount = 0;
};

struct EmptyContext
{
    std::uint64_t mDummy = 0;  // For sink
};

// ============================================================================
// Fat-P State Definitions
// ============================================================================

namespace fatp_states
{

// 4-State Counting (on_entry increments counter)
struct Cnt4S0 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt4S1 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt4S2 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt4S3 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };

// 4-State Empty (no-op hooks)
struct Empty4S0 { void on_entry(EmptyContext&) noexcept {} void on_exit(EmptyContext&) noexcept {} };
struct Empty4S1 { void on_entry(EmptyContext&) noexcept {} void on_exit(EmptyContext&) noexcept {} };
struct Empty4S2 { void on_entry(EmptyContext&) noexcept {} void on_exit(EmptyContext&) noexcept {} };
struct Empty4S3 { void on_entry(EmptyContext&) noexcept {} void on_exit(EmptyContext&) noexcept {} };

// 8-State Counting
struct Cnt8S0 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt8S1 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt8S2 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt8S3 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt8S4 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt8S5 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt8S6 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };
struct Cnt8S7 { void on_entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
               void on_exit(CountingContext&) noexcept {} };

} // namespace fatp_states

// ============================================================================
// Fat-P StateMachine Type Aliases
// ============================================================================

using TL_Empty = std::tuple<>;

// 4-state counting
using SM_AnyToAny_4_Cnt = fat_p::StateMachine<CountingContext, TL_Empty,
    fat_p::AnyToAnyTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
    fatp_states::Cnt4S0, fatp_states::Cnt4S1, fatp_states::Cnt4S2, fatp_states::Cnt4S3>;

// Strict 4-state: fully connected (any state can go to any other)
using TL_Strict_4 = std::tuple<
    std::pair<fatp_states::Cnt4S0, fatp_states::Cnt4S1>,
    std::pair<fatp_states::Cnt4S0, fatp_states::Cnt4S2>,
    std::pair<fatp_states::Cnt4S0, fatp_states::Cnt4S3>,
    std::pair<fatp_states::Cnt4S1, fatp_states::Cnt4S0>,
    std::pair<fatp_states::Cnt4S1, fatp_states::Cnt4S2>,
    std::pair<fatp_states::Cnt4S1, fatp_states::Cnt4S3>,
    std::pair<fatp_states::Cnt4S2, fatp_states::Cnt4S0>,
    std::pair<fatp_states::Cnt4S2, fatp_states::Cnt4S1>,
    std::pair<fatp_states::Cnt4S2, fatp_states::Cnt4S3>,
    std::pair<fatp_states::Cnt4S3, fatp_states::Cnt4S0>,
    std::pair<fatp_states::Cnt4S3, fatp_states::Cnt4S1>,
    std::pair<fatp_states::Cnt4S3, fatp_states::Cnt4S2>>;

using SM_Strict_4_Cnt = fat_p::StateMachine<CountingContext, TL_Strict_4,
    fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
    fatp_states::Cnt4S0, fatp_states::Cnt4S1, fatp_states::Cnt4S2, fatp_states::Cnt4S3>;

// 4-state empty
using SM_AnyToAny_4_Empty = fat_p::StateMachine<EmptyContext, TL_Empty,
    fat_p::AnyToAnyTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
    fatp_states::Empty4S0, fatp_states::Empty4S1, fatp_states::Empty4S2, fatp_states::Empty4S3>;

using TL_Strict_4_Empty = std::tuple<
    std::pair<fatp_states::Empty4S0, fatp_states::Empty4S1>,
    std::pair<fatp_states::Empty4S0, fatp_states::Empty4S2>,
    std::pair<fatp_states::Empty4S0, fatp_states::Empty4S3>,
    std::pair<fatp_states::Empty4S1, fatp_states::Empty4S0>,
    std::pair<fatp_states::Empty4S1, fatp_states::Empty4S2>,
    std::pair<fatp_states::Empty4S1, fatp_states::Empty4S3>,
    std::pair<fatp_states::Empty4S2, fatp_states::Empty4S0>,
    std::pair<fatp_states::Empty4S2, fatp_states::Empty4S1>,
    std::pair<fatp_states::Empty4S2, fatp_states::Empty4S3>,
    std::pair<fatp_states::Empty4S3, fatp_states::Empty4S0>,
    std::pair<fatp_states::Empty4S3, fatp_states::Empty4S1>,
    std::pair<fatp_states::Empty4S3, fatp_states::Empty4S2>>;

using SM_Strict_4_Empty = fat_p::StateMachine<EmptyContext, TL_Strict_4_Empty,
    fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
    fatp_states::Empty4S0, fatp_states::Empty4S1, fatp_states::Empty4S2, fatp_states::Empty4S3>;

// 8-state counting
using SM_AnyToAny_8_Cnt = fat_p::StateMachine<CountingContext, TL_Empty,
    fat_p::AnyToAnyTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
    fatp_states::Cnt8S0, fatp_states::Cnt8S1, fatp_states::Cnt8S2, fatp_states::Cnt8S3,
    fatp_states::Cnt8S4, fatp_states::Cnt8S5, fatp_states::Cnt8S6, fatp_states::Cnt8S7>;

// Strict 8-state: fully connected
using TL_Strict_8 = std::tuple<
    std::pair<fatp_states::Cnt8S0, fatp_states::Cnt8S1>,
    std::pair<fatp_states::Cnt8S0, fatp_states::Cnt8S2>,
    std::pair<fatp_states::Cnt8S0, fatp_states::Cnt8S3>,
    std::pair<fatp_states::Cnt8S0, fatp_states::Cnt8S4>,
    std::pair<fatp_states::Cnt8S0, fatp_states::Cnt8S5>,
    std::pair<fatp_states::Cnt8S0, fatp_states::Cnt8S6>,
    std::pair<fatp_states::Cnt8S0, fatp_states::Cnt8S7>,
    std::pair<fatp_states::Cnt8S1, fatp_states::Cnt8S0>,
    std::pair<fatp_states::Cnt8S1, fatp_states::Cnt8S2>,
    std::pair<fatp_states::Cnt8S1, fatp_states::Cnt8S3>,
    std::pair<fatp_states::Cnt8S1, fatp_states::Cnt8S4>,
    std::pair<fatp_states::Cnt8S1, fatp_states::Cnt8S5>,
    std::pair<fatp_states::Cnt8S1, fatp_states::Cnt8S6>,
    std::pair<fatp_states::Cnt8S1, fatp_states::Cnt8S7>,
    std::pair<fatp_states::Cnt8S2, fatp_states::Cnt8S0>,
    std::pair<fatp_states::Cnt8S2, fatp_states::Cnt8S1>,
    std::pair<fatp_states::Cnt8S2, fatp_states::Cnt8S3>,
    std::pair<fatp_states::Cnt8S2, fatp_states::Cnt8S4>,
    std::pair<fatp_states::Cnt8S2, fatp_states::Cnt8S5>,
    std::pair<fatp_states::Cnt8S2, fatp_states::Cnt8S6>,
    std::pair<fatp_states::Cnt8S2, fatp_states::Cnt8S7>,
    std::pair<fatp_states::Cnt8S3, fatp_states::Cnt8S0>,
    std::pair<fatp_states::Cnt8S3, fatp_states::Cnt8S1>,
    std::pair<fatp_states::Cnt8S3, fatp_states::Cnt8S2>,
    std::pair<fatp_states::Cnt8S3, fatp_states::Cnt8S4>,
    std::pair<fatp_states::Cnt8S3, fatp_states::Cnt8S5>,
    std::pair<fatp_states::Cnt8S3, fatp_states::Cnt8S6>,
    std::pair<fatp_states::Cnt8S3, fatp_states::Cnt8S7>,
    std::pair<fatp_states::Cnt8S4, fatp_states::Cnt8S0>,
    std::pair<fatp_states::Cnt8S4, fatp_states::Cnt8S1>,
    std::pair<fatp_states::Cnt8S4, fatp_states::Cnt8S2>,
    std::pair<fatp_states::Cnt8S4, fatp_states::Cnt8S3>,
    std::pair<fatp_states::Cnt8S4, fatp_states::Cnt8S5>,
    std::pair<fatp_states::Cnt8S4, fatp_states::Cnt8S6>,
    std::pair<fatp_states::Cnt8S4, fatp_states::Cnt8S7>,
    std::pair<fatp_states::Cnt8S5, fatp_states::Cnt8S0>,
    std::pair<fatp_states::Cnt8S5, fatp_states::Cnt8S1>,
    std::pair<fatp_states::Cnt8S5, fatp_states::Cnt8S2>,
    std::pair<fatp_states::Cnt8S5, fatp_states::Cnt8S3>,
    std::pair<fatp_states::Cnt8S5, fatp_states::Cnt8S4>,
    std::pair<fatp_states::Cnt8S5, fatp_states::Cnt8S6>,
    std::pair<fatp_states::Cnt8S5, fatp_states::Cnt8S7>,
    std::pair<fatp_states::Cnt8S6, fatp_states::Cnt8S0>,
    std::pair<fatp_states::Cnt8S6, fatp_states::Cnt8S1>,
    std::pair<fatp_states::Cnt8S6, fatp_states::Cnt8S2>,
    std::pair<fatp_states::Cnt8S6, fatp_states::Cnt8S3>,
    std::pair<fatp_states::Cnt8S6, fatp_states::Cnt8S4>,
    std::pair<fatp_states::Cnt8S6, fatp_states::Cnt8S5>,
    std::pair<fatp_states::Cnt8S6, fatp_states::Cnt8S7>,
    std::pair<fatp_states::Cnt8S7, fatp_states::Cnt8S0>,
    std::pair<fatp_states::Cnt8S7, fatp_states::Cnt8S1>,
    std::pair<fatp_states::Cnt8S7, fatp_states::Cnt8S2>,
    std::pair<fatp_states::Cnt8S7, fatp_states::Cnt8S3>,
    std::pair<fatp_states::Cnt8S7, fatp_states::Cnt8S4>,
    std::pair<fatp_states::Cnt8S7, fatp_states::Cnt8S5>,
    std::pair<fatp_states::Cnt8S7, fatp_states::Cnt8S6>>;

using SM_Strict_8_Cnt = fat_p::StateMachine<CountingContext, TL_Strict_8,
    fat_p::StrictTransitionPolicy, fat_p::NoExceptActionPolicy, 0,
    fatp_states::Cnt8S0, fatp_states::Cnt8S1, fatp_states::Cnt8S2, fatp_states::Cnt8S3,
    fatp_states::Cnt8S4, fatp_states::Cnt8S5, fatp_states::Cnt8S6, fatp_states::Cnt8S7>;

// ============================================================================
// Manual Enum-Switch State Machine
// ============================================================================

namespace manual
{

// 4-state counting
class EnumSwitch4Cnt
{
    CountingContext& mCtx;
    std::size_t mCurrent = 0;

public:
    explicit EnumSwitch4Cnt(CountingContext& ctx) : mCtx(ctx) { ++mCtx.mTransitionCount; }

    void transition(std::size_t next) noexcept
    {
        if (mCurrent == next) return;
        mCurrent = next;
        ++mCtx.mTransitionCount;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent == s; }
};

// 4-state empty
class EnumSwitch4Empty
{
    EmptyContext& mCtx;
    std::size_t mCurrent = 0;

public:
    explicit EnumSwitch4Empty(EmptyContext& ctx) : mCtx(ctx) { ++mCtx.mDummy; }

    void transition(std::size_t next) noexcept
    {
        if (mCurrent == next) return;
        mCurrent = next;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent == s; }
};

// 8-state counting
class EnumSwitch8Cnt
{
    CountingContext& mCtx;
    std::size_t mCurrent = 0;

public:
    explicit EnumSwitch8Cnt(CountingContext& ctx) : mCtx(ctx) { ++mCtx.mTransitionCount; }

    void transition(std::size_t next) noexcept
    {
        if (mCurrent == next) return;
        mCurrent = next;
        ++mCtx.mTransitionCount;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent == s; }
};

// ============================================================================
// Manual Function-Pointer Table State Machine
// ============================================================================

// 4-state counting
class FnPtr4Cnt
{
    using EntryFn = void (*)(CountingContext&) noexcept;
    static void entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
    static constexpr std::array<EntryFn, 4> kEntryTable = {entry, entry, entry, entry};

    CountingContext& mCtx;
    std::size_t mCurrent = 0;

public:
    explicit FnPtr4Cnt(CountingContext& ctx) : mCtx(ctx) { kEntryTable[0](mCtx); }

    void transition(std::size_t next) noexcept
    {
        if (mCurrent == next) return;
        mCurrent = next;
        kEntryTable[next](mCtx);
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent == s; }
};

// 4-state empty
class FnPtr4Empty
{
    using EntryFn = void (*)(EmptyContext&) noexcept;
    static void entry(EmptyContext&) noexcept {}
    static constexpr std::array<EntryFn, 4> kEntryTable = {entry, entry, entry, entry};

    EmptyContext& mCtx;
    std::size_t mCurrent = 0;

public:
    explicit FnPtr4Empty(EmptyContext& ctx) : mCtx(ctx) { kEntryTable[0](mCtx); }

    void transition(std::size_t next) noexcept
    {
        if (mCurrent == next) return;
        mCurrent = next;
        kEntryTable[next](mCtx);
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent == s; }
};

// 8-state counting
class FnPtr8Cnt
{
    using EntryFn = void (*)(CountingContext&) noexcept;
    static void entry(CountingContext& c) noexcept { ++c.mTransitionCount; }
    static constexpr std::array<EntryFn, 8> kEntryTable = {
        entry, entry, entry, entry, entry, entry, entry, entry};

    CountingContext& mCtx;
    std::size_t mCurrent = 0;

public:
    explicit FnPtr8Cnt(CountingContext& ctx) : mCtx(ctx) { kEntryTable[0](mCtx); }

    void transition(std::size_t next) noexcept
    {
        if (mCurrent == next) return;
        mCurrent = next;
        kEntryTable[next](mCtx);
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent == s; }
};

} // namespace manual

// ============================================================================
// std::variant State Machine
// ============================================================================

namespace variant_sm
{

// 4-state types
struct V4S0 {}; struct V4S1 {}; struct V4S2 {}; struct V4S3 {};
using Variant4 = std::variant<V4S0, V4S1, V4S2, V4S3>;

// 8-state types
struct V8S0 {}; struct V8S1 {}; struct V8S2 {}; struct V8S3 {};
struct V8S4 {}; struct V8S5 {}; struct V8S6 {}; struct V8S7 {};
using Variant8 = std::variant<V8S0, V8S1, V8S2, V8S3, V8S4, V8S5, V8S6, V8S7>;

// 4-state counting
class Variant4Cnt
{
    CountingContext& mCtx;
    Variant4 mCurrent = V4S0{};

    struct EntryVisitor
    {
        CountingContext& ctx;
        void operator()(V4S0) { ++ctx.mTransitionCount; }
        void operator()(V4S1) { ++ctx.mTransitionCount; }
        void operator()(V4S2) { ++ctx.mTransitionCount; }
        void operator()(V4S3) { ++ctx.mTransitionCount; }
    };

public:
    explicit Variant4Cnt(CountingContext& ctx) : mCtx(ctx)
    {
        std::visit(EntryVisitor{mCtx}, mCurrent);
    }

    template <typename T>
    void transition()
    {
        if (std::holds_alternative<T>(mCurrent)) return;
        mCurrent = T{};
        std::visit(EntryVisitor{mCtx}, mCurrent);
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrent.index() == idx) return;
        switch (idx)
        {
        case 0: mCurrent = V4S0{}; break;
        case 1: mCurrent = V4S1{}; break;
        case 2: mCurrent = V4S2{}; break;
        case 3: mCurrent = V4S3{}; break;
        }
        std::visit(EntryVisitor{mCtx}, mCurrent);
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent.index(); }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent.index() == s; }
};

// 4-state empty
class Variant4Empty
{
    EmptyContext& mCtx;
    Variant4 mCurrent = V4S0{};

public:
    explicit Variant4Empty(EmptyContext& ctx) : mCtx(ctx) { ++mCtx.mDummy; }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrent.index() == idx) return;
        switch (idx)
        {
        case 0: mCurrent = V4S0{}; break;
        case 1: mCurrent = V4S1{}; break;
        case 2: mCurrent = V4S2{}; break;
        case 3: mCurrent = V4S3{}; break;
        }
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent.index(); }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent.index() == s; }
};

// 8-state counting
class Variant8Cnt
{
    CountingContext& mCtx;
    Variant8 mCurrent = V8S0{};

    struct EntryVisitor
    {
        CountingContext& ctx;
        void operator()(V8S0) { ++ctx.mTransitionCount; }
        void operator()(V8S1) { ++ctx.mTransitionCount; }
        void operator()(V8S2) { ++ctx.mTransitionCount; }
        void operator()(V8S3) { ++ctx.mTransitionCount; }
        void operator()(V8S4) { ++ctx.mTransitionCount; }
        void operator()(V8S5) { ++ctx.mTransitionCount; }
        void operator()(V8S6) { ++ctx.mTransitionCount; }
        void operator()(V8S7) { ++ctx.mTransitionCount; }
    };

public:
    explicit Variant8Cnt(CountingContext& ctx) : mCtx(ctx)
    {
        std::visit(EntryVisitor{mCtx}, mCurrent);
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrent.index() == idx) return;
        switch (idx)
        {
        case 0: mCurrent = V8S0{}; break;
        case 1: mCurrent = V8S1{}; break;
        case 2: mCurrent = V8S2{}; break;
        case 3: mCurrent = V8S3{}; break;
        case 4: mCurrent = V8S4{}; break;
        case 5: mCurrent = V8S5{}; break;
        case 6: mCurrent = V8S6{}; break;
        case 7: mCurrent = V8S7{}; break;
        }
        std::visit(EntryVisitor{mCtx}, mCurrent);
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrent.index(); }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrent.index() == s; }
};

} // namespace variant_sm

// ============================================================================
// Boost.SML State Machine (Optional)
// ============================================================================

#if HAS_BOOST_SML
namespace sml_sm
{
namespace sml = boost::sml;

// Events
struct ToS0 {};
struct ToS1 {};
struct ToS2 {};
struct ToS3 {};
struct ToS4 {};
struct ToS5 {};
struct ToS6 {};
struct ToS7 {};

// 4-state counting transition table
struct Sml4CntTable
{
    auto operator()() const noexcept
    {
        using namespace sml;
        return make_transition_table(
            *"s0"_s + event<ToS0> = "s0"_s,
             "s0"_s + event<ToS1> = "s1"_s,
             "s0"_s + event<ToS2> = "s2"_s,
             "s0"_s + event<ToS3> = "s3"_s,
             "s1"_s + event<ToS0> = "s0"_s,
             "s1"_s + event<ToS1> = "s1"_s,
             "s1"_s + event<ToS2> = "s2"_s,
             "s1"_s + event<ToS3> = "s3"_s,
             "s2"_s + event<ToS0> = "s0"_s,
             "s2"_s + event<ToS1> = "s1"_s,
             "s2"_s + event<ToS2> = "s2"_s,
             "s2"_s + event<ToS3> = "s3"_s,
             "s3"_s + event<ToS0> = "s0"_s,
             "s3"_s + event<ToS1> = "s1"_s,
             "s3"_s + event<ToS2> = "s2"_s,
             "s3"_s + event<ToS3> = "s3"_s
        );
    }
};

// 4-state empty transition table
struct Sml4EmptyTable
{
    auto operator()() const noexcept
    {
        using namespace sml;
        return make_transition_table(
            *"s0"_s + event<ToS0> = "s0"_s,
             "s0"_s + event<ToS1> = "s1"_s,
             "s0"_s + event<ToS2> = "s2"_s,
             "s0"_s + event<ToS3> = "s3"_s,
             "s1"_s + event<ToS0> = "s0"_s,
             "s1"_s + event<ToS1> = "s1"_s,
             "s1"_s + event<ToS2> = "s2"_s,
             "s1"_s + event<ToS3> = "s3"_s,
             "s2"_s + event<ToS0> = "s0"_s,
             "s2"_s + event<ToS1> = "s1"_s,
             "s2"_s + event<ToS2> = "s2"_s,
             "s2"_s + event<ToS3> = "s3"_s,
             "s3"_s + event<ToS0> = "s0"_s,
             "s3"_s + event<ToS1> = "s1"_s,
             "s3"_s + event<ToS2> = "s2"_s,
             "s3"_s + event<ToS3> = "s3"_s
        );
    }
};

// 8-state counting transition table
struct Sml8CntTable
{
    auto operator()() const noexcept
    {
        using namespace sml;
        return make_transition_table(
            *"s0"_s + event<ToS0> = "s0"_s, "s0"_s + event<ToS1> = "s1"_s,
             "s0"_s + event<ToS2> = "s2"_s, "s0"_s + event<ToS3> = "s3"_s,
             "s0"_s + event<ToS4> = "s4"_s, "s0"_s + event<ToS5> = "s5"_s,
             "s0"_s + event<ToS6> = "s6"_s, "s0"_s + event<ToS7> = "s7"_s,
             "s1"_s + event<ToS0> = "s0"_s, "s1"_s + event<ToS1> = "s1"_s,
             "s1"_s + event<ToS2> = "s2"_s, "s1"_s + event<ToS3> = "s3"_s,
             "s1"_s + event<ToS4> = "s4"_s, "s1"_s + event<ToS5> = "s5"_s,
             "s1"_s + event<ToS6> = "s6"_s, "s1"_s + event<ToS7> = "s7"_s,
             "s2"_s + event<ToS0> = "s0"_s, "s2"_s + event<ToS1> = "s1"_s,
             "s2"_s + event<ToS2> = "s2"_s, "s2"_s + event<ToS3> = "s3"_s,
             "s2"_s + event<ToS4> = "s4"_s, "s2"_s + event<ToS5> = "s5"_s,
             "s2"_s + event<ToS6> = "s6"_s, "s2"_s + event<ToS7> = "s7"_s,
             "s3"_s + event<ToS0> = "s0"_s, "s3"_s + event<ToS1> = "s1"_s,
             "s3"_s + event<ToS2> = "s2"_s, "s3"_s + event<ToS3> = "s3"_s,
             "s3"_s + event<ToS4> = "s4"_s, "s3"_s + event<ToS5> = "s5"_s,
             "s3"_s + event<ToS6> = "s6"_s, "s3"_s + event<ToS7> = "s7"_s,
             "s4"_s + event<ToS0> = "s0"_s, "s4"_s + event<ToS1> = "s1"_s,
             "s4"_s + event<ToS2> = "s2"_s, "s4"_s + event<ToS3> = "s3"_s,
             "s4"_s + event<ToS4> = "s4"_s, "s4"_s + event<ToS5> = "s5"_s,
             "s4"_s + event<ToS6> = "s6"_s, "s4"_s + event<ToS7> = "s7"_s,
             "s5"_s + event<ToS0> = "s0"_s, "s5"_s + event<ToS1> = "s1"_s,
             "s5"_s + event<ToS2> = "s2"_s, "s5"_s + event<ToS3> = "s3"_s,
             "s5"_s + event<ToS4> = "s4"_s, "s5"_s + event<ToS5> = "s5"_s,
             "s5"_s + event<ToS6> = "s6"_s, "s5"_s + event<ToS7> = "s7"_s,
             "s6"_s + event<ToS0> = "s0"_s, "s6"_s + event<ToS1> = "s1"_s,
             "s6"_s + event<ToS2> = "s2"_s, "s6"_s + event<ToS3> = "s3"_s,
             "s6"_s + event<ToS4> = "s4"_s, "s6"_s + event<ToS5> = "s5"_s,
             "s6"_s + event<ToS6> = "s6"_s, "s6"_s + event<ToS7> = "s7"_s,
             "s7"_s + event<ToS0> = "s0"_s, "s7"_s + event<ToS1> = "s1"_s,
             "s7"_s + event<ToS2> = "s2"_s, "s7"_s + event<ToS3> = "s3"_s,
             "s7"_s + event<ToS4> = "s4"_s, "s7"_s + event<ToS5> = "s5"_s,
             "s7"_s + event<ToS6> = "s6"_s, "s7"_s + event<ToS7> = "s7"_s
        );
    }
};

// Wrapper for counting context
class Sml4Cnt
{
    CountingContext& mCtx;
    sml::sm<Sml4CntTable> mSm;
    std::size_t mCurrentState = 0;

public:
    explicit Sml4Cnt(CountingContext& ctx) : mCtx(ctx), mSm{}
    {
        ++mCtx.mTransitionCount;  // Initial entry
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrentState == idx) return;
        switch (idx)
        {
        case 0: mSm.process_event(ToS0{}); break;
        case 1: mSm.process_event(ToS1{}); break;
        case 2: mSm.process_event(ToS2{}); break;
        case 3: mSm.process_event(ToS3{}); break;
        }
        mCurrentState = idx;
        ++mCtx.mTransitionCount;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrentState; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrentState == s; }
};

class Sml4Empty
{
    sml::sm<Sml4EmptyTable> mSm;
    std::size_t mCurrentState = 0;

public:
    explicit Sml4Empty(EmptyContext&) : mSm{} {}

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrentState == idx) return;
        switch (idx)
        {
        case 0: mSm.process_event(ToS0{}); break;
        case 1: mSm.process_event(ToS1{}); break;
        case 2: mSm.process_event(ToS2{}); break;
        case 3: mSm.process_event(ToS3{}); break;
        }
        mCurrentState = idx;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrentState; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrentState == s; }
};

class Sml8Cnt
{
    CountingContext& mCtx;
    sml::sm<Sml8CntTable> mSm;
    std::size_t mCurrentState = 0;

public:
    explicit Sml8Cnt(CountingContext& ctx) : mCtx(ctx), mSm{}
    {
        ++mCtx.mTransitionCount;  // Initial entry
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrentState == idx) return;
        switch (idx)
        {
        case 0: mSm.process_event(ToS0{}); break;
        case 1: mSm.process_event(ToS1{}); break;
        case 2: mSm.process_event(ToS2{}); break;
        case 3: mSm.process_event(ToS3{}); break;
        case 4: mSm.process_event(ToS4{}); break;
        case 5: mSm.process_event(ToS5{}); break;
        case 6: mSm.process_event(ToS6{}); break;
        case 7: mSm.process_event(ToS7{}); break;
        }
        mCurrentState = idx;
        ++mCtx.mTransitionCount;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrentState; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrentState == s; }
};

} // namespace sml_sm
#endif // HAS_BOOST_SML

// ============================================================================
// Boost.MSM State Machine (Optional)
// ============================================================================

#if HAS_BOOST_MSM
namespace msm_sm
{
namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace msm::front;

// Events
struct EvToS0 {};
struct EvToS1 {};
struct EvToS2 {};
struct EvToS3 {};
struct EvToS4 {};
struct EvToS5 {};
struct EvToS6 {};
struct EvToS7 {};

// 4-state counting front-end
struct Msm4CntFrontEnd : public msm::front::state_machine_def<Msm4CntFrontEnd>
{
    CountingContext* ctx = nullptr;

    // States
    struct S0 : public msm::front::state<>
    {
        template <class Event, class FSM>
        void on_entry(Event const&, FSM& fsm) { if (fsm.ctx) ++fsm.ctx->mTransitionCount; }
    };
    struct S1 : public msm::front::state<>
    {
        template <class Event, class FSM>
        void on_entry(Event const&, FSM& fsm) { if (fsm.ctx) ++fsm.ctx->mTransitionCount; }
    };
    struct S2 : public msm::front::state<>
    {
        template <class Event, class FSM>
        void on_entry(Event const&, FSM& fsm) { if (fsm.ctx) ++fsm.ctx->mTransitionCount; }
    };
    struct S3 : public msm::front::state<>
    {
        template <class Event, class FSM>
        void on_entry(Event const&, FSM& fsm) { if (fsm.ctx) ++fsm.ctx->mTransitionCount; }
    };

    using initial_state = S0;

    // Transition table - use _row for actionless transitions
    struct transition_table : mpl::vector<
        //    Start  Event    Next
        _row<S0,    EvToS0,  S0>,
        _row<S0,    EvToS1,  S1>,
        _row<S0,    EvToS2,  S2>,
        _row<S0,    EvToS3,  S3>,
        _row<S1,    EvToS0,  S0>,
        _row<S1,    EvToS1,  S1>,
        _row<S1,    EvToS2,  S2>,
        _row<S1,    EvToS3,  S3>,
        _row<S2,    EvToS0,  S0>,
        _row<S2,    EvToS1,  S1>,
        _row<S2,    EvToS2,  S2>,
        _row<S2,    EvToS3,  S3>,
        _row<S3,    EvToS0,  S0>,
        _row<S3,    EvToS1,  S1>,
        _row<S3,    EvToS2,  S2>,
        _row<S3,    EvToS3,  S3>
    > {};
};

using Msm4CntBackEnd = msm::back::state_machine<Msm4CntFrontEnd>;

// Wrapper class
class Msm4Cnt
{
    CountingContext& mCtx;
    Msm4CntBackEnd mSm;
    std::size_t mCurrentState = 0;

public:
    explicit Msm4Cnt(CountingContext& ctx) : mCtx(ctx)
    {
        mSm.ctx = &mCtx;
        mSm.start();
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrentState == idx) return;
        switch (idx)
        {
        case 0: mSm.process_event(EvToS0{}); break;
        case 1: mSm.process_event(EvToS1{}); break;
        case 2: mSm.process_event(EvToS2{}); break;
        case 3: mSm.process_event(EvToS3{}); break;
        }
        mCurrentState = idx;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrentState; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrentState == s; }
};

// 4-state empty front-end
struct Msm4EmptyFrontEnd : public msm::front::state_machine_def<Msm4EmptyFrontEnd>
{
    struct S0 : public msm::front::state<> {};
    struct S1 : public msm::front::state<> {};
    struct S2 : public msm::front::state<> {};
    struct S3 : public msm::front::state<> {};

    using initial_state = S0;

    struct transition_table : mpl::vector<
        _row<S0, EvToS0, S0>,
        _row<S0, EvToS1, S1>,
        _row<S0, EvToS2, S2>,
        _row<S0, EvToS3, S3>,
        _row<S1, EvToS0, S0>,
        _row<S1, EvToS1, S1>,
        _row<S1, EvToS2, S2>,
        _row<S1, EvToS3, S3>,
        _row<S2, EvToS0, S0>,
        _row<S2, EvToS1, S1>,
        _row<S2, EvToS2, S2>,
        _row<S2, EvToS3, S3>,
        _row<S3, EvToS0, S0>,
        _row<S3, EvToS1, S1>,
        _row<S3, EvToS2, S2>,
        _row<S3, EvToS3, S3>
    > {};
};

using Msm4EmptyBackEnd = msm::back::state_machine<Msm4EmptyFrontEnd>;

class Msm4Empty
{
    Msm4EmptyBackEnd mSm;
    std::size_t mCurrentState = 0;

public:
    explicit Msm4Empty(EmptyContext&)
    {
        mSm.start();
    }

    void transitionByIndex(std::size_t idx)
    {
        if (mCurrentState == idx) return;
        switch (idx)
        {
        case 0: mSm.process_event(EvToS0{}); break;
        case 1: mSm.process_event(EvToS1{}); break;
        case 2: mSm.process_event(EvToS2{}); break;
        case 3: mSm.process_event(EvToS3{}); break;
        }
        mCurrentState = idx;
    }

    [[nodiscard]] std::size_t current() const noexcept { return mCurrentState; }
    [[nodiscard]] bool isInState(std::size_t s) const noexcept { return mCurrentState == s; }
};

// Note: 8-state MSM not implemented - mpl::vector has 20-element limit by default
// and 64 transitions would require BOOST_MPL_LIMIT_VECTOR_SIZE=64 before all Boost headers

} // namespace msm_sm
#endif // HAS_BOOST_MSM


// ============================================================================
// Adapter Interface
// ============================================================================

struct IAdapter
{
    virtual ~IAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup() = 0;
    virtual void runTransitions(const std::vector<std::size_t>& seq) = 0;
    virtual void runSelfTransitions(std::size_t count) = 0;
    virtual std::size_t runStateQueries(std::size_t count) = 0;
    virtual std::uint64_t getCount() const = 0;
    virtual std::size_t currentState() const = 0;
    virtual void teardown() = 0;
};

// ============================================================================
// Fat-P Adapters
// ============================================================================

// AnyToAny 4-state counting
class FatPAnyToAny4CntAdapter : public IAdapter
{
    std::unique_ptr<SM_AnyToAny_4_Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "fat_p AnyToAny"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<SM_AnyToAny_4_Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq)
        {
            switch (t)
            {
            case 0: mSm->transition<fatp_states::Cnt4S0>(); break;
            case 1: mSm->transition<fatp_states::Cnt4S1>(); break;
            case 2: mSm->transition<fatp_states::Cnt4S2>(); break;
            case 3: mSm->transition<fatp_states::Cnt4S3>(); break;
            }
        }
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i)
            mSm->transition<fatp_states::Cnt4S0>();
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState<fatp_states::Cnt4S0>()) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->currentStateIndex(); }
    void teardown() override { mSm.reset(); }
};

// Strict 4-state counting
class FatPStrict4CntAdapter : public IAdapter
{
    std::unique_ptr<SM_Strict_4_Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "fat_p Strict"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<SM_Strict_4_Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq)
        {
            switch (t)
            {
            case 0: mSm->transition<fatp_states::Cnt4S0>(); break;
            case 1: mSm->transition<fatp_states::Cnt4S1>(); break;
            case 2: mSm->transition<fatp_states::Cnt4S2>(); break;
            case 3: mSm->transition<fatp_states::Cnt4S3>(); break;
            }
        }
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i)
            mSm->transition<fatp_states::Cnt4S0>();
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState<fatp_states::Cnt4S0>()) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->currentStateIndex(); }
    void teardown() override { mSm.reset(); }
};

// AnyToAny 4-state empty
class FatPAnyToAny4EmptyAdapter : public IAdapter
{
    std::unique_ptr<SM_AnyToAny_4_Empty> mSm;
    EmptyContext mCtx;

public:
    const char* name() const override { return "fat_p AnyToAny (empty)"; }

    void setup() override
    {
        mCtx = EmptyContext{};
        mSm = std::make_unique<SM_AnyToAny_4_Empty>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq)
        {
            switch (t)
            {
            case 0: mSm->transition<fatp_states::Empty4S0>(); break;
            case 1: mSm->transition<fatp_states::Empty4S1>(); break;
            case 2: mSm->transition<fatp_states::Empty4S2>(); break;
            case 3: mSm->transition<fatp_states::Empty4S3>(); break;
            }
        }
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->currentStateIndex(); }
    void teardown() override { mSm.reset(); }
};

// Strict 4-state empty
class FatPStrict4EmptyAdapter : public IAdapter
{
    std::unique_ptr<SM_Strict_4_Empty> mSm;
    EmptyContext mCtx;

public:
    const char* name() const override { return "fat_p Strict (empty)"; }

    void setup() override
    {
        mCtx = EmptyContext{};
        mSm = std::make_unique<SM_Strict_4_Empty>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq)
        {
            switch (t)
            {
            case 0: mSm->transition<fatp_states::Empty4S0>(); break;
            case 1: mSm->transition<fatp_states::Empty4S1>(); break;
            case 2: mSm->transition<fatp_states::Empty4S2>(); break;
            case 3: mSm->transition<fatp_states::Empty4S3>(); break;
            }
        }
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->currentStateIndex(); }
    void teardown() override { mSm.reset(); }
};

// AnyToAny 8-state counting
class FatPAnyToAny8CntAdapter : public IAdapter
{
    std::unique_ptr<SM_AnyToAny_8_Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "fat_p AnyToAny (8-state)"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<SM_AnyToAny_8_Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq)
        {
            switch (t)
            {
            case 0: mSm->transition<fatp_states::Cnt8S0>(); break;
            case 1: mSm->transition<fatp_states::Cnt8S1>(); break;
            case 2: mSm->transition<fatp_states::Cnt8S2>(); break;
            case 3: mSm->transition<fatp_states::Cnt8S3>(); break;
            case 4: mSm->transition<fatp_states::Cnt8S4>(); break;
            case 5: mSm->transition<fatp_states::Cnt8S5>(); break;
            case 6: mSm->transition<fatp_states::Cnt8S6>(); break;
            case 7: mSm->transition<fatp_states::Cnt8S7>(); break;
            }
        }
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->currentStateIndex(); }
    void teardown() override { mSm.reset(); }
};

// Strict 8-state counting
class FatPStrict8CntAdapter : public IAdapter
{
    std::unique_ptr<SM_Strict_8_Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "fat_p Strict (8-state)"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<SM_Strict_8_Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq)
        {
            switch (t)
            {
            case 0: mSm->transition<fatp_states::Cnt8S0>(); break;
            case 1: mSm->transition<fatp_states::Cnt8S1>(); break;
            case 2: mSm->transition<fatp_states::Cnt8S2>(); break;
            case 3: mSm->transition<fatp_states::Cnt8S3>(); break;
            case 4: mSm->transition<fatp_states::Cnt8S4>(); break;
            case 5: mSm->transition<fatp_states::Cnt8S5>(); break;
            case 6: mSm->transition<fatp_states::Cnt8S6>(); break;
            case 7: mSm->transition<fatp_states::Cnt8S7>(); break;
            }
        }
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->currentStateIndex(); }
    void teardown() override { mSm.reset(); }
};

// ============================================================================
// Manual Enum-Switch Adapters
// ============================================================================

class ManualEnum4CntAdapter : public IAdapter
{
    std::unique_ptr<manual::EnumSwitch4Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "Manual enum-switch"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<manual::EnumSwitch4Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transition(t);
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i) mSm->transition(0);
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState(0)) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class ManualEnum4EmptyAdapter : public IAdapter
{
    std::unique_ptr<manual::EnumSwitch4Empty> mSm;
    EmptyContext mCtx;

public:
    const char* name() const override { return "Manual enum-switch (empty)"; }

    void setup() override
    {
        mCtx = EmptyContext{};
        mSm = std::make_unique<manual::EnumSwitch4Empty>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transition(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class ManualEnum8CntAdapter : public IAdapter
{
    std::unique_ptr<manual::EnumSwitch8Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "Manual enum-switch (8-state)"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<manual::EnumSwitch8Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transition(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

// ============================================================================
// Manual Function-Pointer Adapters
// ============================================================================

class ManualFnPtr4CntAdapter : public IAdapter
{
    std::unique_ptr<manual::FnPtr4Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "Manual fn-ptr table"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<manual::FnPtr4Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transition(t);
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i) mSm->transition(0);
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState(0)) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class ManualFnPtr4EmptyAdapter : public IAdapter
{
    std::unique_ptr<manual::FnPtr4Empty> mSm;
    EmptyContext mCtx;

public:
    const char* name() const override { return "Manual fn-ptr table (empty)"; }

    void setup() override
    {
        mCtx = EmptyContext{};
        mSm = std::make_unique<manual::FnPtr4Empty>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transition(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class ManualFnPtr8CntAdapter : public IAdapter
{
    std::unique_ptr<manual::FnPtr8Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "Manual fn-ptr table (8-state)"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<manual::FnPtr8Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transition(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

// ============================================================================
// std::variant Adapters
// ============================================================================

class Variant4CntAdapter : public IAdapter
{
    std::unique_ptr<variant_sm::Variant4Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "std::variant"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<variant_sm::Variant4Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i) mSm->transitionByIndex(0);
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState(0)) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class Variant4EmptyAdapter : public IAdapter
{
    std::unique_ptr<variant_sm::Variant4Empty> mSm;
    EmptyContext mCtx;

public:
    const char* name() const override { return "std::variant (empty)"; }

    void setup() override
    {
        mCtx = EmptyContext{};
        mSm = std::make_unique<variant_sm::Variant4Empty>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class Variant8CntAdapter : public IAdapter
{
    std::unique_ptr<variant_sm::Variant8Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "std::variant (8-state)"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<variant_sm::Variant8Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

// ============================================================================
// Boost.SML Adapters (Optional)
// ============================================================================

#if HAS_BOOST_SML
class Sml4CntAdapter : public IAdapter
{
    std::unique_ptr<sml_sm::Sml4Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "[Boost].SML"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<sml_sm::Sml4Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i) mSm->transitionByIndex(0);
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState(0)) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class Sml4EmptyAdapter : public IAdapter
{
    std::unique_ptr<sml_sm::Sml4Empty> mSm;
    EmptyContext mCtx;

public:
    const char* name() const override { return "[Boost].SML (empty)"; }

    void setup() override
    {
        mCtx = EmptyContext{};
        mSm = std::make_unique<sml_sm::Sml4Empty>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class Sml8CntAdapter : public IAdapter
{
    std::unique_ptr<sml_sm::Sml8Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "[Boost].SML (8-state)"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<sml_sm::Sml8Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};
#endif // HAS_BOOST_SML

// ============================================================================
// TinyFSM Adapters (Optional)
// ============================================================================

#if HAS_TINYFSM
class TinyFsm4CntAdapter : public IAdapter
{
    std::unique_ptr<benchmark_tinyfsm::TinyFsm4CntWrapper> mSm;
    benchmark_tinyfsm::TinyContext mCtx;

public:
    const char* name() const override { return "TinyFSM"; }

    void setup() override
    {
        mCtx = benchmark_tinyfsm::TinyContext{};
        mSm = std::make_unique<benchmark_tinyfsm::TinyFsm4CntWrapper>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i) mSm->transitionByIndex(0);
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState(0)) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class TinyFsm4EmptyAdapter : public IAdapter
{
    std::unique_ptr<benchmark_tinyfsm::TinyFsm4EmptyWrapper> mSm;
    benchmark_tinyfsm::TinyEmptyContext mCtx;

public:
    const char* name() const override { return "TinyFSM (empty)"; }

    void setup() override
    {
        mCtx = benchmark_tinyfsm::TinyEmptyContext{};
        mSm = std::make_unique<benchmark_tinyfsm::TinyFsm4EmptyWrapper>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};
#endif // HAS_TINYFSM

// ============================================================================
// Boost.MSM Adapters (Optional)
// ============================================================================

#if HAS_BOOST_MSM
class Msm4CntAdapter : public IAdapter
{
    std::unique_ptr<msm_sm::Msm4Cnt> mSm;
    CountingContext mCtx;

public:
    const char* name() const override { return "Boost.MSM"; }

    void setup() override
    {
        mCtx = CountingContext{};
        mSm = std::make_unique<msm_sm::Msm4Cnt>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i) mSm->transitionByIndex(0);
    }

    std::size_t runStateQueries(std::size_t count) override
    {
        std::size_t c = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (mSm->isInState(0)) ++c;
        return c;
    }

    std::uint64_t getCount() const override { return mCtx.mTransitionCount; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};

class Msm4EmptyAdapter : public IAdapter
{
    std::unique_ptr<msm_sm::Msm4Empty> mSm;
    EmptyContext mCtx;

public:
    const char* name() const override { return "Boost.MSM (empty)"; }

    void setup() override
    {
        mCtx = EmptyContext{};
        mSm = std::make_unique<msm_sm::Msm4Empty>(mCtx);
    }

    void runTransitions(const std::vector<std::size_t>& seq) override
    {
        for (std::size_t t : seq) mSm->transitionByIndex(t);
    }

    void runSelfTransitions(std::size_t) override {}
    std::size_t runStateQueries(std::size_t) override { return 0; }
    std::uint64_t getCount() const override { return mCtx.mDummy; }
    std::size_t currentState() const override { return mSm->current(); }
    void teardown() override { mSm.reset(); }
};
// Note: No Msm8CntAdapter - mpl::vector limit precludes 64-transition table
#endif // HAS_BOOST_MSM

void runRoundRobin(
    const std::string& sectionTitle,
    const std::string& caseName,
    const std::string& contract,
    std::vector<std::unique_ptr<IAdapter>>& adapters,
    std::function<void(IAdapter*)> runFunc,
    std::size_t opsPerRun,
    std::function<std::uint64_t(IAdapter*)> expectedCountFunc = nullptr)
{
    printHeader(sectionTitle);
    printContractNote(contract);

    std::string cpuCtx = captureCpuContext();
    print_cpu_context(std::cout);

    const std::size_t numAdapters = adapters.size();
    std::vector<std::vector<double>> allSamples(numAdapters);
    for (auto& v : allSamples) v.reserve(MEASURED_RUNS());

    // Warmup
    for (std::size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            adapter->setup();
            runFunc(adapter.get());
            g_benchmarkSink += adapter->getCount();
            adapter->teardown();
        }
    }

    // Measured runs with round-robin randomization
    std::vector<std::size_t> order(numAdapters);
    std::iota(order.begin(), order.end(), 0);

    for (std::size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), g_rng);

        for (std::size_t idx : order)
        {
            adapters[idx]->setup();

            Timer timer;
            timer.start();
            runFunc(adapters[idx].get());
            double elapsed = timer.elapsedNs();

            allSamples[idx].push_back(elapsed / static_cast<double>(opsPerRun));
            g_benchmarkSink += adapters[idx]->getCount();
            adapters[idx]->teardown();
        }
    }

    // Print results
    printResultHeader();
    for (std::size_t i = 0; i < numAdapters; ++i)
    {
        Stats s = computeStats(allSamples[i]);
        printResult(adapters[i]->name(), s);
        recordResult("StateMachine", caseName, adapters[i]->name(), s, cpuCtx);
    }

    // Correctness validation
    if (expectedCountFunc)
    {
        bool allCorrect = true;
        for (auto& adapter : adapters)
        {
            adapter->setup();
            runFunc(adapter.get());
            std::uint64_t actual = adapter->getCount();
            std::uint64_t expected = expectedCountFunc(adapter.get());
            adapter->teardown();

            if (actual != expected)
            {
                std::cerr << "[ERROR] " << adapter->name() << " count mismatch: "
                          << actual << " != " << expected << "\n";
                allCorrect = false;
            }
        }
        if (allCorrect)
        {
            std::cout << "\n[OK] Correctness validated for all " << numAdapters
                      << " implementations\n";
        }
    }

    std::cout << "\n";
}

// ============================================================================
// Section 1: Core Transition Performance
// ============================================================================

void benchmarkCoreTransition()
{
    const std::size_t iterations = TARGET_WORK();
    auto sequence = generateTransitionSequence<4>(iterations, SEED());
    std::uint64_t expectedCount = countActualTransitions(sequence, 0);

    std::vector<std::unique_ptr<IAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPAnyToAny4CntAdapter>());
    adapters.push_back(std::make_unique<FatPStrict4CntAdapter>());
    adapters.push_back(std::make_unique<ManualEnum4CntAdapter>());
    adapters.push_back(std::make_unique<ManualFnPtr4CntAdapter>());
    adapters.push_back(std::make_unique<Variant4CntAdapter>());
#if HAS_BOOST_SML
    adapters.push_back(std::make_unique<Sml4CntAdapter>());
#endif
#if HAS_TINYFSM
    adapters.push_back(std::make_unique<TinyFsm4CntAdapter>());
#endif
#if HAS_BOOST_MSM
    adapters.push_back(std::make_unique<Msm4CntAdapter>());
#endif

    runRoundRobin(
        "Section 1: Core Transition Performance",
        "CoreTransition",
        "transition() is O(1) for all implementations. All use 4 states with counting hooks.",
        adapters,
        [&sequence](IAdapter* a) { a->runTransitions(sequence); },
        iterations,
        [expectedCount](IAdapter*) { return expectedCount; });
}

// ============================================================================
// Section 2: Hook Overhead
// ============================================================================

void benchmarkHookOverhead()
{
    const std::size_t iterations = TARGET_WORK();
    auto sequence = generateTransitionSequence<4>(iterations, SEED());

    // Empty hooks
    {
        std::vector<std::unique_ptr<IAdapter>> adapters;
        adapters.push_back(std::make_unique<FatPAnyToAny4EmptyAdapter>());
        adapters.push_back(std::make_unique<FatPStrict4EmptyAdapter>());
        adapters.push_back(std::make_unique<ManualEnum4EmptyAdapter>());
        adapters.push_back(std::make_unique<ManualFnPtr4EmptyAdapter>());
        adapters.push_back(std::make_unique<Variant4EmptyAdapter>());
#if HAS_BOOST_SML
        adapters.push_back(std::make_unique<Sml4EmptyAdapter>());
#endif
#if HAS_TINYFSM
        adapters.push_back(std::make_unique<TinyFsm4EmptyAdapter>());
#endif
#if HAS_BOOST_MSM
        adapters.push_back(std::make_unique<Msm4EmptyAdapter>());
#endif

        runRoundRobin(
            "Section 2a: Hook Overhead (Empty Hooks)",
            "HookOverhead_Empty",
            "Baseline with empty on_entry/on_exit hooks. Measures pure dispatch overhead.",
            adapters,
            [&sequence](IAdapter* a) { a->runTransitions(sequence); },
            iterations,
            nullptr);
    }

    // Counting hooks
    {
        std::uint64_t expectedCount = countActualTransitions(sequence, 0);

        std::vector<std::unique_ptr<IAdapter>> adapters;
        adapters.push_back(std::make_unique<FatPAnyToAny4CntAdapter>());
        adapters.push_back(std::make_unique<FatPStrict4CntAdapter>());
        adapters.push_back(std::make_unique<ManualEnum4CntAdapter>());
        adapters.push_back(std::make_unique<ManualFnPtr4CntAdapter>());
        adapters.push_back(std::make_unique<Variant4CntAdapter>());
#if HAS_BOOST_SML
        adapters.push_back(std::make_unique<Sml4CntAdapter>());
#endif
#if HAS_TINYFSM
        adapters.push_back(std::make_unique<TinyFsm4CntAdapter>());
#endif
#if HAS_BOOST_MSM
        adapters.push_back(std::make_unique<Msm4CntAdapter>());
#endif

        runRoundRobin(
            "Section 2b: Hook Overhead (Counting Hooks)",
            "HookOverhead_Counting",
            "Hooks increment a counter. Measures dispatch + minimal work.",
            adapters,
            [&sequence](IAdapter* a) { a->runTransitions(sequence); },
            iterations,
            [expectedCount](IAdapter*) { return expectedCount; });
    }
}

// ============================================================================
// Section 3: State Count Scaling
// ============================================================================

void benchmarkStateScaling()
{
    const std::size_t iterations = TARGET_WORK();

    // 4 states
    {
        auto sequence = generateTransitionSequence<4>(iterations, SEED());
        std::uint64_t expectedCount = countActualTransitions(sequence, 0);

        std::vector<std::unique_ptr<IAdapter>> adapters;
        adapters.push_back(std::make_unique<FatPAnyToAny4CntAdapter>());
        adapters.push_back(std::make_unique<FatPStrict4CntAdapter>());
        adapters.push_back(std::make_unique<ManualEnum4CntAdapter>());
        adapters.push_back(std::make_unique<ManualFnPtr4CntAdapter>());
        adapters.push_back(std::make_unique<Variant4CntAdapter>());
#if HAS_BOOST_SML
        adapters.push_back(std::make_unique<Sml4CntAdapter>());
#endif
#if HAS_TINYFSM
        adapters.push_back(std::make_unique<TinyFsm4CntAdapter>());
#endif
#if HAS_BOOST_MSM
        adapters.push_back(std::make_unique<Msm4CntAdapter>());
#endif

        runRoundRobin(
            "Section 3a: State Scaling (4 States)",
            "StateScaling_4",
            "4-state machines with counting hooks. Baseline for scaling comparison.",
            adapters,
            [&sequence](IAdapter* a) { a->runTransitions(sequence); },
            iterations,
            [expectedCount](IAdapter*) { return expectedCount; });
    }

    // 8 states
    {
        auto sequence = generateTransitionSequence<8>(iterations, SEED());
        std::uint64_t expectedCount = countActualTransitions(sequence, 0);

        std::vector<std::unique_ptr<IAdapter>> adapters;
        adapters.push_back(std::make_unique<FatPAnyToAny8CntAdapter>());
        adapters.push_back(std::make_unique<FatPStrict8CntAdapter>());
        adapters.push_back(std::make_unique<ManualEnum8CntAdapter>());
        adapters.push_back(std::make_unique<ManualFnPtr8CntAdapter>());
        adapters.push_back(std::make_unique<Variant8CntAdapter>());
#if HAS_BOOST_SML
        adapters.push_back(std::make_unique<Sml8CntAdapter>());
#endif
        // Note: Boost.MSM 8-state not implemented (mpl::vector limit)
        // Note: TinyFSM 8-state not implemented (would require 8 state classes)

        runRoundRobin(
            "Section 3b: State Scaling (8 States)",
            "StateScaling_8",
            "8-state machines with counting hooks. O(1) claim: should match 4-state performance.",
            adapters,
            [&sequence](IAdapter* a) { a->runTransitions(sequence); },
            iterations,
            [expectedCount](IAdapter*) { return expectedCount; });
    }
}

// ============================================================================
// Section 4: Self-Transition Performance
// ============================================================================

void benchmarkSelfTransition()
{
    const std::size_t iterations = TARGET_WORK();

    std::vector<std::unique_ptr<IAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPAnyToAny4CntAdapter>());
    adapters.push_back(std::make_unique<FatPStrict4CntAdapter>());
    adapters.push_back(std::make_unique<ManualEnum4CntAdapter>());
    adapters.push_back(std::make_unique<ManualFnPtr4CntAdapter>());
    adapters.push_back(std::make_unique<Variant4CntAdapter>());
#if HAS_BOOST_SML
    adapters.push_back(std::make_unique<Sml4CntAdapter>());
#endif
#if HAS_TINYFSM
    adapters.push_back(std::make_unique<TinyFsm4CntAdapter>());
#endif
#if HAS_BOOST_MSM
    adapters.push_back(std::make_unique<Msm4CntAdapter>());
#endif

    runRoundRobin(
        "Section 4: Self-Transition (No-Op) Performance",
        "SelfTransition",
        "Self-transitions should early-exit without invoking hooks. "
        "Expected count is 1 (initial entry only).",
        adapters,
        [iterations](IAdapter* a) { a->runSelfTransitions(iterations); },
        iterations,
        [](IAdapter*) -> std::uint64_t { return 1; });  // Only initial entry counts
}

// ============================================================================
// Section 5: State Query Performance
// ============================================================================

void benchmarkStateQuery()
{
    const std::size_t iterations = TARGET_WORK();

    std::vector<std::unique_ptr<IAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPAnyToAny4CntAdapter>());
    adapters.push_back(std::make_unique<FatPStrict4CntAdapter>());
    adapters.push_back(std::make_unique<ManualEnum4CntAdapter>());
    adapters.push_back(std::make_unique<ManualFnPtr4CntAdapter>());
    adapters.push_back(std::make_unique<Variant4CntAdapter>());
#if HAS_BOOST_SML
    adapters.push_back(std::make_unique<Sml4CntAdapter>());
#endif
#if HAS_TINYFSM
    adapters.push_back(std::make_unique<TinyFsm4CntAdapter>());
#endif
#if HAS_BOOST_MSM
    adapters.push_back(std::make_unique<Msm4CntAdapter>());
#endif

    printHeader("Section 5: State Query Performance");
    printContractNote("isInState<T>() / equivalent should be O(1). Query from initial state.");

    std::string cpuCtx = captureCpuContext();
    print_cpu_context(std::cout);

    const std::size_t numAdapters = adapters.size();
    std::vector<std::vector<double>> allSamples(numAdapters);
    for (auto& v : allSamples) v.reserve(MEASURED_RUNS());

    // Warmup
    for (std::size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            adapter->setup();
            g_benchmarkSink += adapter->runStateQueries(iterations);
            adapter->teardown();
        }
    }

    // Measured runs
    std::vector<std::size_t> order(numAdapters);
    std::iota(order.begin(), order.end(), 0);

    for (std::size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), g_rng);

        for (std::size_t idx : order)
        {
            adapters[idx]->setup();

            Timer timer;
            timer.start();
            std::size_t count = adapters[idx]->runStateQueries(iterations);
            double elapsed = timer.elapsedNs();

            allSamples[idx].push_back(elapsed / static_cast<double>(iterations));
            g_benchmarkSink += count;
            adapters[idx]->teardown();
        }
    }

    // Print results
    printResultHeader();
    for (std::size_t i = 0; i < numAdapters; ++i)
    {
        Stats s = computeStats(allSamples[i]);
        printResult(adapters[i]->name(), s);
        recordResult("StateMachine", "StateQuery", adapters[i]->name(), s, cpuCtx);
    }

    // Correctness: all queries should return true (we're in state 0)
    bool allCorrect = true;
    for (auto& adapter : adapters)
    {
        adapter->setup();
        std::size_t count = adapter->runStateQueries(iterations);
        adapter->teardown();
        if (count != iterations)
        {
            std::cerr << "[ERROR] " << adapter->name() << " query returned false: "
                      << count << "/" << iterations << "\n";
            allCorrect = false;
        }
    }
    if (allCorrect)
    {
        std::cout << "\n[OK] All " << numAdapters << " implementations returned "
                  << iterations << "/" << iterations << " true\n";
    }

    std::cout << "\n";
}

// ============================================================================
// Print Configuration
// ============================================================================

void printConfiguration()
{
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  FAT-P StateMachine Benchmark Suite\n";
    std::cout << std::string(80, '=') << "\n\n";

    std::cout << "Configuration:\n";
    std::cout << "  Seed:           " << g_config.seed << "\n";
    std::cout << "  Warmup runs:    " << g_config.warmupRuns << "\n";
    std::cout << "  Measured runs:  " << g_config.measuredRuns << "\n";
    std::cout << "  Target work:    " << g_targetWork << " ops/batch\n";
    std::cout << "  Min batch ms:   " << g_config.minBatchMs << "\n";
    std::cout << "  Scope:          " << (g_config.noScope ? "OFF" : "ON") << "\n";
    std::cout << "  Stabilize:      " << (g_config.noStabilize ? "OFF" : "ON") << "\n";
    std::cout << "  Cooldown:       " << (g_config.noCooldown ? "OFF" : "ON") << "\n";
    std::cout << "\n";

    std::cout << "Design invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run (round-robin)\n";
    std::cout << "  3. Setup/teardown occur outside timed regions\n";
    std::cout << "  4. All libraries observe the same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n";
    std::cout << "\n";

    std::cout << "Competitors (tested in ALL applicable sections):\n";
    std::cout << "  - fat_p::StateMachine<AnyToAnyTransitionPolicy>\n";
    std::cout << "  - fat_p::StateMachine<StrictTransitionPolicy>\n";
    std::cout << "  - Manual enum-switch state machine\n";
    std::cout << "  - Manual function-pointer table state machine\n";
    std::cout << "  - std::variant + std::visit (C++17 standard baseline)\n";
#if HAS_BOOST_SML
    std::cout << "  - [Boost::ext].SML (header-only)\n";
#else
    std::cout << "  - [Boost::ext].SML (NOT FOUND - install boost/sml.hpp)\n";
#endif
#if HAS_BOOST_MSM
    std::cout << "  - Boost.MSM (full Boost)\n";
#elif HAS_BOOST_MSM_HEADERS
    std::cout << "  - Boost.MSM (present but disabled - define USE_BOOST_MSM=1)\n";
#else
    std::cout << "  - Boost.MSM (NOT FOUND - install Boost)\n";
#endif
#if HAS_TINYFSM
    std::cout << "  - TinyFSM (header-only)\n";
#else
    std::cout << "  - TinyFSM (NOT FOUND - install tinyfsm.hpp)\n";
#endif
    std::cout << "\n";
}

// ============================================================================
// Print Summary
// ============================================================================

void printSummary()
{
    printHeader("Summary");
    std::cout << "All benchmarks completed.\n\n";

    std::cout << "Key comparisons:\n";
    std::cout << "  - fat_p AnyToAny vs fat_p Strict: policy validation overhead\n";
    std::cout << "  - fat_p vs Manual: abstraction overhead\n";
    std::cout << "  - fat_p vs std::variant: type-safe alternative comparison\n";
    std::cout << "  - 4-state vs 8-state: O(1) scaling validation\n";
    std::cout << "  - Empty vs Counting hooks: hook invocation cost\n";
    std::cout << "\n";

    std::cout << "Implementation characteristics:\n";
    std::cout << "  - fat_p AnyToAny: compile-time state set, O(1) fn-ptr dispatch, no validation\n";
    std::cout << "  - fat_p Strict: same + O(1) matrix lookup for transition validation\n";
    std::cout << "  - Manual enum-switch: runtime switch/case, inline-friendly\n";
    std::cout << "  - Manual fn-ptr table: constexpr table, indirect call\n";
    std::cout << "  - std::variant: type-safe union, visitor pattern dispatch\n";
    std::cout << "\n";
}

} // anonymous namespace

// ============================================================================
// Main
// ============================================================================

int main()
{
    g_config = fat_p::bench::BenchConfig::fromEnv();

    if (const char* env = std::getenv("FATP_BENCH_TARGET_WORK"))
        g_targetWork = static_cast<std::size_t>(std::strtoull(env, nullptr, 10));

    g_rng.seed(g_config.seed);

    printConfiguration();

    benchmarkCoreTransition();
    benchmarkHookOverhead();
    benchmarkStateScaling();
    benchmarkSelfTransition();
    benchmarkStateQuery();

    printSummary();

    if (!g_config.outputCsv.empty()) writeCSV(g_config.outputCsv);
    if (!g_config.outputJson.empty()) writeJSON(g_config.outputJson);

    std::cout << "[Sink: " << g_benchmarkSink << "]\n";

    return 0;
}
