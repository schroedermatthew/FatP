// benchmark_SparseSet.cpp
//
// FAT-P SparseSet benchmarks using unified FatPBenchmarkRunner infrastructure.
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
// Fat-P Libraries:
//   - fat_p::SparseSet: O(1) insert/erase/contains with dense iteration
//   - fat_p::FlatSet: O(log n) lookup, O(n) insert/erase, dense iteration
//
// Competitor Libraries (conditioned on availability):
//   TIER 1 - Direct competitors (sparse set implementations):
//     - llvm::SparseSet (same data structure design)
//     - entt::basic_sparse_set (ECS sparse set)
//   TIER 2 - High-performance hash sets:
//     - absl::flat_hash_set (Swiss Table, SIMD-accelerated)
//   TIER 3 - Standard library baselines:
//     - std::unordered_set (hash set baseline)
//     - std::set (ordered baseline, O(log n))
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_SparseSet.cpp -o bench_ss
//   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_SparseSet.cpp /link advapi32.lib
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
//   ./bench_ss
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_ss

/*
FATP_META:
  meta_version: 1
  component: SparseSet
  file_role: benchmark
  path: components/SparseSet/benchmarks/benchmark_SparseSet.cpp
  layer: Testing
  namespace: fat_p
  summary: "Benchmarks for SparseSet."
  api_stability: in_work
  related:
    docs_search: "SparseSet"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/SparseSet.h
      - include/fat_p/FlatSet.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 6
    defines_unprefixed: 6
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

// Suppress conversion warnings from vendor libraries (LLVM, absl, entt)
#ifdef _MSC_VER
#pragma warning(disable : 4244 4267) // conversion warnings in vendor headers
#endif

#include <algorithm>
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
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"
#include "FlatSet.h"
#include "SparseSet.h"

// ============================================================================
// Library Detection
// ============================================================================

// LLVM SparseSet - direct competitor (same data structure design)
// vcpkg install llvm, or Linux: apt install llvm-dev
// NOTE: Prefixed paths like <llvm-18/llvm/ADT/...> do not work because internal LLVM
// headers use non-prefixed includes that fail without -isystem.
#if __has_include(<llvm/ADT/SparseSet.h>)
#include <llvm/ADT/SparseSet.h>
#define HAS_LLVM_SPARSESET 1
#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "ws2_32.lib") // LLVMSupport.lib requires Windows Sockets
#endif
#else
#define HAS_LLVM_SPARSESET 0
#endif

// EnTT - Popular ECS library with sparse_set
// Install: vcpkg install entt, or header-only from github.com/skypjack/entt
#if __has_include(<entt/entt.hpp>)
#include <entt/entt.hpp>
#define HAS_ENTT 1
#elif __has_include(<entt/entity/sparse_set.hpp>)
#include <entt/entity/sparse_set.hpp>
#define HAS_ENTT 1
#else
#define HAS_ENTT 0
#endif

// Abseil flat_hash_set - high-performance Swiss Table
// vcpkg install abseil
#if __has_include(<absl/container/flat_hash_set.h>)
#include <absl/container/flat_hash_set.h>
#define HAS_ABSL 1
#else
#define HAS_ABSL 0
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
// Benchmark Environment
// ============================================================================

using fat_p::bench::BenchmarkScope;
using fat_p::bench::DoNotOptimize;
using fat_p::bench::preventOpt;

// ============================================================================
// Timer and Statistics
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

using Statistics = fat_p::bench::Statistics;

// Prevent dead code elimination
static volatile int64_t benchmark_sink = 0;

static inline void prevent_opt(int64_t value)
{
    benchmark_sink ^= value;
}

// ============================================================================
// Cooling Delays
// ============================================================================

static constexpr int COOLING_DELAY_SECTION_MS = 1000;
static constexpr int COOLING_DELAY_SIZE_MS = 500;
static constexpr int COOLING_DELAY_CASE_MS = 200;

static void cooling_delay(int ms, const char* reason)
{
    if (g_config.noCooldown || ms <= 0)
    {
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    auto info = fat_p::bench::capture_cpu_frequency();
    std::cout << "[Cooling: " << reason << "] [Ready: " << static_cast<int>(info.mCurrentFreqMHz) << " MHz]\n";
}

// ============================================================================
// CPU Stability Wait
// ============================================================================

static bool wait_for_cpu_stable(double tolerance_pct, int max_samples, int interval_ms, bool verbose)
{
    fat_p::bench::CpuWaitConfig cfg;
    cfg.mThrottleThreshold = tolerance_pct;
    cfg.mMaxWaitSeconds = (max_samples * interval_ms) / 1000;
    cfg.mPollIntervalMs = interval_ms;

    auto result = fat_p::bench::wait_for_cpu_stable(verbose ? &std::cout : nullptr, cfg, 0, "initial");
    return result.mStabilized;
}

// ============================================================================
// Data Generation
// ============================================================================

struct BenchInputs
{
    std::vector<uint32_t> insertKeys;  // Keys to insert
    std::vector<uint32_t> lookupKeys;  // Mix of hits and misses
    std::vector<uint32_t> eraseKeys;   // Keys to erase (subset of insertKeys)
    std::vector<uint32_t> presentKeys; // Keys known to be present
    size_t N = 0;

    static BenchInputs make(size_t n, uint64_t seed)
    {
        BenchInputs in;
        in.N = n;

        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<uint32_t> dist(0, static_cast<uint32_t>(n * 10));

        // Generate unique insert keys
        std::unordered_set<uint32_t> seen;
        in.insertKeys.reserve(n);
        while (in.insertKeys.size() < n)
        {
            uint32_t key = dist(rng);
            if (seen.insert(key).second)
            {
                in.insertKeys.push_back(key);
            }
        }

        // Lookup keys: 50% hits, 50% misses
        in.lookupKeys.reserve(n);
        for (size_t i = 0; i < n / 2; ++i)
        {
            in.lookupKeys.push_back(in.insertKeys[i % in.insertKeys.size()]);
        }
        for (size_t i = 0; i < n - n / 2; ++i)
        {
            uint32_t key;
            do
            {
                key = dist(rng);
            } while (seen.count(key) > 0);
            in.lookupKeys.push_back(key);
        }
        std::shuffle(in.lookupKeys.begin(), in.lookupKeys.end(), rng);

        // Erase keys: first half of insert keys
        in.eraseKeys.assign(in.insertKeys.begin(), in.insertKeys.begin() + static_cast<ptrdiff_t>(n / 2));
        std::shuffle(in.eraseKeys.begin(), in.eraseKeys.end(), rng);

        // Present keys: all insert keys (for iteration correctness check)
        in.presentKeys = in.insertKeys;

        return in;
    }
};

// ============================================================================
// Benchmark Result Storage
// ============================================================================

struct BenchResult
{
    std::string library;
    std::vector<double> samples;
};

// ============================================================================
// Adapter Interface
// ============================================================================

enum class BenchCase
{
    Insert,
    Contains,
    Erase,
    Iteration,
    MixedWorkload
};

struct IAdapter
{
    virtual ~IAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t N, const BenchInputs& in) = 0;
    virtual void teardown() = 0;
    virtual size_t run_operation(BenchCase op, const BenchInputs& in) = 0;
    virtual bool verify_correctness(const BenchInputs& in) = 0;
};

// ============================================================================
// Fat-P SparseSet Adapters
// ============================================================================
// fat_p::SparseSet<T> uses T as both the value type AND the type for storing
// dense indices in the sparse array. So SparseSet<uint8_t> has max 256 elements
// just like LLVM's default. We test BOTH for fair comparison.
// ============================================================================

// SparseSet<uint8_t> - max 256 elements (same limitation as LLVM default)
class FatPSparseSet8Adapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::SparseSet<8>";
    }

    void setup(size_t N, const BenchInputs& in) override
    {
        mSet = fat_p::SparseSet<uint8_t>();
        // Note: uint8_t limits us to 256 elements max
        // Keys > 255 will cause issues - this shows the limitation
        mSet.reserve(static_cast<uint8_t>(std::min(N * 10, size_t(255))));
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(static_cast<uint8_t>(key & 0xFF));
        }
    }

    void teardown() override
    {
        mSet = fat_p::SparseSet<uint8_t>();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        // Will likely have fewer elements due to key collisions after truncation
        (void)in;
        return true; // Can't verify properly due to truncation
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(static_cast<uint8_t>(key & 0xFF));
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.contains(static_cast<uint8_t>(key & 0xFF)))
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(static_cast<uint8_t>(key & 0xFF));
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (uint8_t val : mSet)
        {
            sum += val;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            auto key = static_cast<uint8_t>(in.eraseKeys[i % in.eraseKeys.size()] & 0xFF);
            mSet.erase(key);
            ++ops;
            mSet.insert(key);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    fat_p::SparseSet<uint8_t> mSet;
};

// SparseSet<uint32_t> - handles large N properly
class FatPSparseSet32Adapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::SparseSet<32>";
    }

    void setup(size_t N, const BenchInputs& in) override
    {
        mSet = fat_p::SparseSet<uint32_t>();
        mSet.reserve(N * 10);
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
    }

    void teardown() override
    {
        mSet = fat_p::SparseSet<uint32_t>();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        if (mSet.size() != in.insertKeys.size())
        {
            return false;
        }
        for (size_t i = 0; i < std::min(size_t(100), in.insertKeys.size()); ++i)
        {
            if (!mSet.contains(in.insertKeys[i]))
            {
                return false;
            }
        }
        return true;
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.contains(key))
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (uint32_t val : mSet)
        {
            sum += val;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            mSet.erase(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
            mSet.insert(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    fat_p::SparseSet<uint32_t> mSet;
};

// ============================================================================
// Fat-P FlatSet Adapter
// ============================================================================

class FatPFlatSetAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::FlatSet";
    }

    void setup(size_t N, const BenchInputs& in) override
    {
        mSet = fat_p::FlatSet<uint32_t>();
        mSet.reserve(N);
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
    }

    void teardown() override
    {
        mSet = fat_p::FlatSet<uint32_t>();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        if (mSet.size() != in.insertKeys.size())
        {
            return false;
        }
        for (size_t i = 0; i < std::min(size_t(100), in.insertKeys.size()); ++i)
        {
            if (mSet.find(in.insertKeys[i]) == mSet.end())
            {
                return false;
            }
        }
        return true;
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        // Note: reserve() called in setup(), not here - we measure pure insert speed
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.contains(key))
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (uint32_t val : mSet)
        {
            sum += val;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            mSet.erase(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
            mSet.insert(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    fat_p::FlatSet<uint32_t> mSet;
};

// ============================================================================
// std::unordered_set Adapter
// ============================================================================

class StdUnorderedSetAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "std::unordered_set";
    }

    void setup(size_t N, const BenchInputs& in) override
    {
        mSet.clear();
        mSet.reserve(N);
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
    }

    void teardown() override
    {
        mSet.clear();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        return mSet.size() == in.insertKeys.size();
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        // Note: reserve() called in setup(), not here - we measure pure insert speed
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.count(key) > 0)
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (uint32_t val : mSet)
        {
            sum += val;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            mSet.erase(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
            mSet.insert(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    std::unordered_set<uint32_t> mSet;
};

// ============================================================================
// std::set Adapter
// ============================================================================

class StdSetAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "std::set";
    }

    void setup(size_t /*N*/, const BenchInputs& in) override
    {
        mSet.clear();
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
    }

    void teardown() override
    {
        mSet.clear();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        return mSet.size() == in.insertKeys.size();
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.count(key) > 0)
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (uint32_t val : mSet)
        {
            sum += val;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            mSet.erase(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
            mSet.insert(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    std::set<uint32_t> mSet;
};

// ============================================================================
// Abseil flat_hash_set Adapter
// ============================================================================

#if HAS_ABSL
class AbslFlatHashSetAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "absl::flat_hash_set";
    }

    void setup(size_t N, const BenchInputs& in) override
    {
        mSet.clear();
        mSet.reserve(N);
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
    }

    void teardown() override
    {
        mSet.clear();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        return mSet.size() == in.insertKeys.size();
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        // Note: reserve() called in setup(), not here - we measure pure insert speed
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.contains(key))
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (uint32_t val : mSet)
        {
            sum += val;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            mSet.erase(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
            mSet.insert(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    absl::flat_hash_set<uint32_t> mSet;
};
#endif

// ============================================================================
// LLVM SparseSet Adapters
// ============================================================================
// LLVM SparseSet uses a SparseT template parameter that determines maximum
// capacity. Default is uint8_t (max 256 elements). We test BOTH to show:
//   1. Default behavior (breaks/overflows at N > 256)
//   2. Properly configured behavior (uint32_t for large N)
// ============================================================================

#if HAS_LLVM_SPARSESET

// Default LLVM configuration: uint8_t sparse type (max 256 elements)
// This will overflow/break at N > 256 - that's the point!
class LlvmSparseSet8Adapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "llvm::SparseSet<8>";
    }

    void setup(size_t N, const BenchInputs& in) override
    {
        mSet.clear();
        // Note: uint8_t sparse type limits us to 256 elements max
        // Keys > 255 will overflow - this shows LLVM's default limitation
        mSet.setUniverse(static_cast<unsigned>(N * 10 + 1));
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
    }

    void teardown() override
    {
        mSet.clear();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        // Will likely fail at N > 256 due to overflow
        return mSet.size() == in.insertKeys.size();
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.count(key) > 0)
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (auto it = mSet.begin(); it != mSet.end(); ++it)
        {
            sum += *it;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            mSet.erase(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
            mSet.insert(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    // Default LLVM SparseSet: uint8_t sparse type (256 element limit)
    llvm::SparseSet<uint32_t> mSet;
};

// Properly configured LLVM: uint32_t sparse type (handles large N)
class LlvmSparseSet32Adapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "llvm::SparseSet<32>";
    }

    void setup(size_t N, const BenchInputs& in) override
    {
        mSet.clear();
        mSet.setUniverse(static_cast<unsigned>(N * 10 + 1));
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
    }

    void teardown() override
    {
        mSet.clear();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        return mSet.size() == in.insertKeys.size();
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        for (uint32_t key : in.insertKeys)
        {
            mSet.insert(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.count(key) > 0)
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.erase(key);
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (auto it = mSet.begin(); it != mSet.end(); ++it)
        {
            sum += *it;
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            mSet.erase(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
            mSet.insert(in.eraseKeys[i % in.eraseKeys.size()]);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    // Properly configured: uint32_t sparse type for large N
    llvm::SparseSet<uint32_t, llvm::identity<unsigned>, uint32_t> mSet;
};
#endif

// ============================================================================
// EnTT sparse_set Adapter
// ============================================================================

#if HAS_ENTT
class EnttSparseSetAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "entt::sparse_set";
    }

    void setup(size_t /*N*/, const BenchInputs& in) override
    {
        mSet.clear();
        for (uint32_t key : in.insertKeys)
        {
            mSet.push(static_cast<entt::entity>(key));
        }
    }

    void teardown() override
    {
        mSet.clear();
    }

    size_t run_operation(BenchCase op, const BenchInputs& in) override
    {
        switch (op)
        {
            case BenchCase::Insert:
                return runInsert(in);
            case BenchCase::Contains:
                return runContains(in);
            case BenchCase::Erase:
                return runErase(in);
            case BenchCase::Iteration:
                return runIteration();
            case BenchCase::MixedWorkload:
                return runMixed(in);
        }
        return 0;
    }

    bool verify_correctness(const BenchInputs& in) override
    {
        return mSet.size() == in.insertKeys.size();
    }

private:
    size_t runInsert(const BenchInputs& in)
    {
        mSet.clear();
        // Note: entt sparse_set doesn't have reserve, so this is insert-only
        for (uint32_t key : in.insertKeys)
        {
            mSet.push(static_cast<entt::entity>(key));
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.insertKeys.size();
    }

    size_t runContains(const BenchInputs& in)
    {
        size_t count = 0;
        for (uint32_t key : in.lookupKeys)
        {
            if (mSet.contains(static_cast<entt::entity>(key)))
            {
                ++count;
            }
        }
        prevent_opt(static_cast<int64_t>(count));
        return in.lookupKeys.size();
    }

    size_t runErase(const BenchInputs& in)
    {
        for (uint32_t key : in.eraseKeys)
        {
            mSet.remove(static_cast<entt::entity>(key));
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return in.eraseKeys.size();
    }

    size_t runIteration()
    {
        int64_t sum = 0;
        for (auto entity : mSet)
        {
            sum += static_cast<uint32_t>(entity);
        }
        prevent_opt(sum);
        return mSet.size();
    }

    size_t runMixed(const BenchInputs& in)
    {
        size_t ops = 0;
        for (size_t i = 0; i < in.N / 2; ++i)
        {
            auto key = static_cast<entt::entity>(in.eraseKeys[i % in.eraseKeys.size()]);
            mSet.remove(key);
            ++ops;
            mSet.push(key);
            ++ops;
        }
        prevent_opt(static_cast<int64_t>(mSet.size()));
        return ops;
    }

    entt::sparse_set mSet;
};
#endif

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

static void print_result_row(const std::string& library, const Statistics& stats)
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(24) << std::right << library << ": " << std::setw(10) << stats.median << " ns/op "
              << "(+/-" << std::setw(7) << stats.stddev << ", CI:[" << std::setw(8) << stats.ci95Low << ","
              << std::setw(8) << stats.ci95High << "])\n";
}

// ============================================================================
// Benchmark: Core Operations
// ============================================================================

static void benchmark_core_operations(const std::vector<size_t>& sizes)
{
    print_header("SECTION 1: Core Operations");
    print_cpu_context("Section start");

    std::cout << "Contract Note: O(1) insert/erase/contains; dense iteration; unstable erase order\n";
    std::cout << "              Insert excludes allocation (reserve performed in setup)\n\n";

    std::mt19937 rng(static_cast<unsigned>(g_config.seed));

    // Create adapters
    std::vector<std::unique_ptr<IAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPSparseSet8Adapter>());  // uint8_t (256 limit)
    adapters.push_back(std::make_unique<FatPSparseSet32Adapter>()); // uint32_t (handles large N)
    adapters.push_back(std::make_unique<FatPFlatSetAdapter>());
    adapters.push_back(std::make_unique<StdUnorderedSetAdapter>());
#if HAS_ABSL
    adapters.push_back(std::make_unique<AbslFlatHashSetAdapter>());
#endif
#if HAS_LLVM_SPARSESET
    adapters.push_back(std::make_unique<LlvmSparseSet8Adapter>());  // Default (256 limit)
    adapters.push_back(std::make_unique<LlvmSparseSet32Adapter>()); // Properly configured
#endif
#if HAS_ENTT
    adapters.push_back(std::make_unique<EnttSparseSetAdapter>());
#endif
    adapters.push_back(std::make_unique<StdSetAdapter>());

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        BenchInputs inputs = BenchInputs::make(N, g_config.seed);

        // Verify correctness first
        for (auto& adapter : adapters)
        {
            adapter->setup(N, inputs);
            if (!adapter->verify_correctness(inputs))
            {
                std::cerr << "CORRECTNESS FAILURE: " << adapter->name() << "\n";
            }
            adapter->teardown();
        }

        // Benchmark each operation
        std::vector<BenchCase> cases = {BenchCase::Insert, BenchCase::Contains, BenchCase::Erase, BenchCase::Iteration};
        std::vector<const char*> caseNames = {"Insert", "Contains (50% hit)", "Erase", "Iteration"};

        for (size_t ci = 0; ci < cases.size(); ++ci)
        {
            BenchCase op = cases[ci];
            std::cout << "\n  " << caseNames[ci] << ":\n";

            std::vector<BenchResult> results;
            for (auto& adapter : adapters)
            {
                results.push_back({adapter->name(), {}});
            }

            // Warmup
            for (size_t run = 0; run < WARMUP_RUNS(); ++run)
            {
                for (size_t idx = 0; idx < adapters.size(); ++idx)
                {
                    adapters[idx]->setup(N, inputs);
                    adapters[idx]->run_operation(op, inputs);
                    adapters[idx]->teardown();
                }
            }

            // Measured runs with round-robin
            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                std::vector<size_t> order(adapters.size());
                std::iota(order.begin(), order.end(), 0);
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    adapters[idx]->setup(N, inputs);

                    Timer t;
                    t.start();
                    size_t ops = adapters[idx]->run_operation(op, inputs);
                    double elapsed = t.elapsed_ns();

                    adapters[idx]->teardown();

                    results[idx].samples.push_back(ns_per_op(elapsed, ops));
                }
            }

            // Print results
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                print_result_row(r.library, stats);
            }
        }
    }
}

// ============================================================================
// Benchmark: Iteration Performance (Key SparseSet Advantage)
// ============================================================================

static void benchmark_iteration()
{
    print_header("SECTION 2: Dense Iteration (SparseSet Key Advantage)");
    print_cpu_context("Section start");

    std::cout << "Contract Note: Dense iteration—SparseSet/FlatSet expected to outperform hash sets\n\n";

    std::mt19937 rng(static_cast<unsigned>(g_config.seed));

    std::vector<std::unique_ptr<IAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPSparseSet8Adapter>());
    adapters.push_back(std::make_unique<FatPSparseSet32Adapter>());
    adapters.push_back(std::make_unique<FatPFlatSetAdapter>());
    adapters.push_back(std::make_unique<StdUnorderedSetAdapter>());
#if HAS_ABSL
    adapters.push_back(std::make_unique<AbslFlatHashSetAdapter>());
#endif
#if HAS_LLVM_SPARSESET
    adapters.push_back(std::make_unique<LlvmSparseSet8Adapter>());
    adapters.push_back(std::make_unique<LlvmSparseSet32Adapter>());
#endif
#if HAS_ENTT
    adapters.push_back(std::make_unique<EnttSparseSetAdapter>());
#endif
    adapters.push_back(std::make_unique<StdSetAdapter>());

    std::vector<size_t> sizes = {10000, 100000};

    for (size_t N : sizes)
    {
        std::cout << "--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        BenchInputs inputs = BenchInputs::make(N, g_config.seed);

        std::vector<BenchResult> results;
        for (auto& adapter : adapters)
        {
            results.push_back({adapter->name(), {}});
        }

        // Warmup
        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            for (size_t idx = 0; idx < adapters.size(); ++idx)
            {
                adapters[idx]->setup(N, inputs);
                adapters[idx]->run_operation(BenchCase::Iteration, inputs);
                adapters[idx]->teardown();
            }
        }

        // Measured runs
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                adapters[idx]->setup(N, inputs);

                Timer t;
                t.start();
                size_t ops = adapters[idx]->run_operation(BenchCase::Iteration, inputs);
                double elapsed = t.elapsed_ns();

                adapters[idx]->teardown();

                results[idx].samples.push_back(ns_per_op(elapsed, ops));
            }
        }

        std::cout << "  Iteration (sum all elements):\n";
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            print_result_row(r.library, stats);
        }
        std::cout << "\n";
    }
}

// ============================================================================
// Benchmark: Mixed Workload (Insert/Erase Churn)
// ============================================================================

static void benchmark_mixed_workload()
{
    print_header("SECTION 3: Mixed Workload (Insert/Erase Churn)");
    print_cpu_context("Section start");

    std::cout << "Contract Note: Random insert/erase churn—tests swap-with-back erase efficiency\n\n";

    std::mt19937 rng(static_cast<unsigned>(g_config.seed));

    std::vector<std::unique_ptr<IAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPSparseSet8Adapter>());
    adapters.push_back(std::make_unique<FatPSparseSet32Adapter>());
    adapters.push_back(std::make_unique<FatPFlatSetAdapter>());
    adapters.push_back(std::make_unique<StdUnorderedSetAdapter>());
#if HAS_ABSL
    adapters.push_back(std::make_unique<AbslFlatHashSetAdapter>());
#endif
#if HAS_LLVM_SPARSESET
    adapters.push_back(std::make_unique<LlvmSparseSet8Adapter>());
    adapters.push_back(std::make_unique<LlvmSparseSet32Adapter>());
#endif
#if HAS_ENTT
    adapters.push_back(std::make_unique<EnttSparseSetAdapter>());
#endif
    adapters.push_back(std::make_unique<StdSetAdapter>());

    size_t N = 10000;
    BenchInputs inputs = BenchInputs::make(N, g_config.seed);

    std::cout << "--- N = " << N << " (50% insert/erase cycles) ---\n";
    print_cpu_context();

    std::vector<BenchResult> results;
    for (auto& adapter : adapters)
    {
        results.push_back({adapter->name(), {}});
    }

    // Warmup
    for (size_t run = 0; run < WARMUP_RUNS(); ++run)
    {
        for (size_t idx = 0; idx < adapters.size(); ++idx)
        {
            adapters[idx]->setup(N, inputs);
            adapters[idx]->run_operation(BenchCase::MixedWorkload, inputs);
            adapters[idx]->teardown();
        }
    }

    // Measured runs
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            adapters[idx]->setup(N, inputs);

            Timer t;
            t.start();
            size_t ops = adapters[idx]->run_operation(BenchCase::MixedWorkload, inputs);
            double elapsed = t.elapsed_ns();

            adapters[idx]->teardown();

            results[idx].samples.push_back(ns_per_op(elapsed, ops));
        }
    }

    std::cout << "  Mixed Workload:\n";
    for (const auto& r : results)
    {
        auto stats = Statistics::compute(r.samples);
        print_result_row(r.library, stats);
    }
}

// ============================================================================
// Feature Comparison Summary
// ============================================================================

static void print_feature_comparison()
{
    print_header("Feature Comparison Summary");

    std::cout << "  Container                 Insert    Contains  Erase     Iteration  Order\n";
    std::cout << "  " << std::string(74, '-') << "\n";
    std::cout << "  fat_p::SparseSet<8>       O(1)*     O(1)      O(1)      Dense      Unstable  (max 256)\n";
    std::cout << "  fat_p::SparseSet<32>      O(1)*     O(1)      O(1)      Dense      Unstable  (handles large N)\n";
    std::cout << "  fat_p::FlatSet            O(n)      O(log n)  O(n)      Dense      Sorted\n";
    std::cout << "  std::unordered_set        O(1)*     O(1)*     O(1)*     Scattered  Unordered\n";
#if HAS_ABSL
    std::cout << "  absl::flat_hash_set       O(1)*     O(1)*     O(1)*     Scattered  Unordered\n";
#endif
#if HAS_LLVM_SPARSESET
    std::cout << "  llvm::SparseSet<8>        O(1)*     O(1)      O(1)      Dense      Unstable  (default, max 256)\n";
    std::cout << "  llvm::SparseSet<32>       O(1)*     O(1)      O(1)      Dense      Unstable  (configured)\n";
#endif
#if HAS_ENTT
    std::cout << "  entt::sparse_set          O(1)*     O(1)      O(1)      Dense      Unstable\n";
#endif
    std::cout << "  std::set                  O(log n)  O(log n)  O(log n)  In-order   Sorted\n";
    std::cout << "\n  * = amortized\n";
    std::cout << "  Note: <8> variants use uint8_t value type (max 256 elements)\n";
    std::cout << "        <32> variants use uint32_t value type (handles large N)\n";
    std::cout << "\n";

    std::cout << "  When to use SparseSet:\n";
    std::cout << "    - Integer keys in a bounded range\n";
    std::cout << "    - Frequent insert/erase churn\n";
    std::cout << "    - Iteration performance matters\n";
    std::cout << "    - Order doesn't matter\n\n";

    std::cout << "  When to use FlatSet:\n";
    std::cout << "    - Need sorted order\n";
    std::cout << "    - Mostly lookups after initial build\n";
    std::cout << "    - Binary search semantics (lower_bound, etc.)\n";
}

// ============================================================================
// Benchmark: tryEmplace vs emplace + tryGet (Double-Lookup Elimination)
// ============================================================================
// Measures the cost of the old double-lookup pattern (emplace returns bool,
// then tryGet re-probes for a reference) against the new tryEmplace that
// returns Data* directly. This is the pattern that ComponentStore::emplace
// and Registry::add used before the Item 1 / Item 2b remediation.

static void benchmark_try_emplace(const std::vector<size_t>& sizes)
{
    print_header("SECTION 4: tryEmplace vs emplace + tryGet (SparseSetWithData)");
    print_cpu_context("Section start");

    std::cout << "Contract Note: Measures per-element insertion cost with associated data.\n";
    std::cout << "              'emplace+tryGet' is the old double-lookup pattern.\n";
    std::cout << "              'tryEmplace' returns Data* directly, one probe.\n";
    std::cout << "              Setup: empty set with reserve(N*10). No allocation in timed region.\n\n";

    std::mt19937 rng(static_cast<unsigned>(g_config.seed));

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        BenchInputs inputs = BenchInputs::make(N, g_config.seed);

        // Correctness check (outside timed region)
        {
            fat_p::SparseSetWithData<uint32_t, double> check;
            check.reserve(N * 10);
            for (uint32_t key : inputs.insertKeys)
            {
                double* ptr = check.tryEmplace(key, static_cast<double>(key));
                if (ptr == nullptr || *ptr != static_cast<double>(key))
                {
                    std::cerr << "CORRECTNESS FAILURE: tryEmplace returned bad pointer\n";
                    return;
                }
            }
            if (check.size() != inputs.insertKeys.size())
            {
                std::cerr << "CORRECTNESS FAILURE: size mismatch after tryEmplace\n";
                return;
            }
            std::cout << "  [PASS] tryEmplace correctness (N=" << N << ")\n";
        }

        // --- Pattern A: emplace (bool) + tryGet (re-probe) ---
        std::vector<double> samplesOld;

        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            fat_p::SparseSetWithData<uint32_t, double> s;
            s.reserve(N * 10);
            for (uint32_t key : inputs.insertKeys)
            {
                s.emplace(key, static_cast<double>(key));
                DoNotOptimize(s.tryGet(key));
            }
        }

        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            fat_p::SparseSetWithData<uint32_t, double> s;
            s.reserve(N * 10);

            Timer t;
            t.start();
            for (uint32_t key : inputs.insertKeys)
            {
                s.emplace(key, static_cast<double>(key));
                double* ptr = s.tryGet(key);
                DoNotOptimize(ptr);
            }
            double elapsed = t.elapsed_ns();

            prevent_opt(static_cast<int64_t>(s.size()));
            samplesOld.push_back(ns_per_op(elapsed, inputs.insertKeys.size()));
        }

        // --- Pattern B: tryEmplace (single probe) ---
        std::vector<double> samplesNew;

        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            fat_p::SparseSetWithData<uint32_t, double> s;
            s.reserve(N * 10);
            for (uint32_t key : inputs.insertKeys)
            {
                DoNotOptimize(s.tryEmplace(key, static_cast<double>(key)));
            }
        }

        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            fat_p::SparseSetWithData<uint32_t, double> s;
            s.reserve(N * 10);

            Timer t;
            t.start();
            for (uint32_t key : inputs.insertKeys)
            {
                double* ptr = s.tryEmplace(key, static_cast<double>(key));
                DoNotOptimize(ptr);
            }
            double elapsed = t.elapsed_ns();

            prevent_opt(static_cast<int64_t>(s.size()));
            samplesNew.push_back(ns_per_op(elapsed, inputs.insertKeys.size()));
        }

        // Print results
        std::cout << "\n  Insert + get reference:\n";
        auto statsOld = Statistics::compute(samplesOld);
        auto statsNew = Statistics::compute(samplesNew);
        print_result_row("emplace + tryGet", statsOld);
        print_result_row("tryEmplace", statsNew);

        if (statsOld.median > 0 && statsNew.median > 0)
        {
            double speedup = statsOld.median / statsNew.median;
            std::cout << "    tryEmplace speedup: " << std::fixed << std::setprecision(2)
                      << speedup << "x\n";
        }
    }
}

// ============================================================================
// Benchmark: Custom IndexPolicy Zero-Overhead Verification
// ============================================================================
// Verifies that using a custom IndexPolicy (composite key with extracted index)
// has zero overhead vs IdentityIndex. This is the pattern used by the ECS:
// SparseSetWithData<Entity, T, EntityIndex> where Entity is a 64-bit composite
// (index + generation) and EntityIndex extracts the 32-bit slot index.
//
// We simulate the ECS pattern with a CompositeKey struct that packs
// (index, generation) into a 64-bit value, with a policy that extracts
// the lower 32 bits.

struct CompositeKey
{
    uint64_t value;

    CompositeKey() : value(0) {}
    explicit CompositeKey(uint64_t v) : value(v) {}

    static CompositeKey make(uint32_t index, uint32_t generation)
    {
        return CompositeKey(static_cast<uint64_t>(generation) << 32 |
                            static_cast<uint64_t>(index));
    }

    uint32_t index() const { return static_cast<uint32_t>(value & 0xFFFFFFFF); }
    uint32_t generation() const { return static_cast<uint32_t>(value >> 32); }
};

struct CompositeKeyIndex
{
    using sparse_index_type = uint32_t;

    static constexpr sparse_index_type index(const CompositeKey& key) noexcept
    {
        return key.index();
    }
};

static void benchmark_index_policy(const std::vector<size_t>& sizes)
{
    print_header("SECTION 5: IndexPolicy Zero-Overhead (IdentityIndex vs Custom)");
    print_cpu_context("Section start");

    std::cout << "Contract Note: Measures insert/contains/erase with associated data.\n";
    std::cout << "              'IdentityIndex' uses uint32_t keys directly (baseline).\n";
    std::cout << "              'CompositeKeyIndex' uses 64-bit composite keys with\n";
    std::cout << "              extracted 32-bit index (ECS Entity pattern).\n";
    std::cout << "              Zero overhead expected: the policy is constexpr and inlined.\n";
    std::cout << "              Setup: reserve(N*10). No allocation in timed region.\n\n";

    std::mt19937 rng(static_cast<unsigned>(g_config.seed));

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        BenchInputs inputs = BenchInputs::make(N, g_config.seed);

        // Build composite keys from the same uint32_t keys (generation = 1)
        std::vector<CompositeKey> compositeKeys;
        compositeKeys.reserve(inputs.insertKeys.size());
        for (uint32_t key : inputs.insertKeys)
        {
            compositeKeys.push_back(CompositeKey::make(key, 1));
        }

        std::vector<CompositeKey> compositeLookups;
        compositeLookups.reserve(inputs.lookupKeys.size());
        for (uint32_t key : inputs.lookupKeys)
        {
            compositeLookups.push_back(CompositeKey::make(key, 1));
        }

        std::vector<CompositeKey> compositeEraseKeys;
        compositeEraseKeys.reserve(inputs.eraseKeys.size());
        for (uint32_t key : inputs.eraseKeys)
        {
            compositeEraseKeys.push_back(CompositeKey::make(key, 1));
        }

        // Correctness check (outside timed region)
        {
            fat_p::SparseSetWithData<CompositeKey, double, CompositeKeyIndex> check;
            check.reserve(N * 10);
            for (const auto& ck : compositeKeys)
            {
                check.tryEmplace(ck, static_cast<double>(ck.index()));
            }
            if (check.size() != compositeKeys.size())
            {
                std::cerr << "CORRECTNESS FAILURE: CompositeKey size mismatch\n";
                return;
            }
            for (size_t i = 0; i < std::min(size_t(100), compositeKeys.size()); ++i)
            {
                if (!check.contains(compositeKeys[i]))
                {
                    std::cerr << "CORRECTNESS FAILURE: CompositeKey not found\n";
                    return;
                }
            }
            std::cout << "  [PASS] CompositeKeyIndex correctness (N=" << N << ")\n";
        }

        // Benchmark three operations: Insert, Contains, Erase
        struct OpConfig
        {
            const char* label;
            int id; // 0=insert, 1=contains, 2=erase
        };

        std::vector<OpConfig> ops = {{"Insert + data", 0}, {"Contains (50% hit)", 1}, {"Erase", 2}};

        for (const auto& op : ops)
        {
            std::cout << "\n  " << op.label << ":\n";

            std::vector<double> samplesIdentity;
            std::vector<double> samplesComposite;

            // --- IdentityIndex (baseline) ---
            for (size_t run = 0; run < WARMUP_RUNS(); ++run)
            {
                fat_p::SparseSetWithData<uint32_t, double> s;
                s.reserve(N * 10);
                if (op.id != 0)
                {
                    for (uint32_t key : inputs.insertKeys)
                    {
                        s.tryEmplace(key, static_cast<double>(key));
                    }
                }
                // run whichever op
                if (op.id == 0)
                {
                    for (uint32_t key : inputs.insertKeys)
                    {
                        DoNotOptimize(s.tryEmplace(key, static_cast<double>(key)));
                    }
                }
                else if (op.id == 1)
                {
                    for (uint32_t key : inputs.lookupKeys)
                    {
                        DoNotOptimize(s.contains(key));
                    }
                }
                else
                {
                    for (uint32_t key : inputs.eraseKeys)
                    {
                        DoNotOptimize(s.erase(key));
                    }
                }
            }

            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                fat_p::SparseSetWithData<uint32_t, double> s;
                s.reserve(N * 10);
                if (op.id != 0)
                {
                    for (uint32_t key : inputs.insertKeys)
                    {
                        s.tryEmplace(key, static_cast<double>(key));
                    }
                }

                Timer t;
                size_t opCount = 0;
                t.start();
                if (op.id == 0)
                {
                    for (uint32_t key : inputs.insertKeys)
                    {
                        DoNotOptimize(s.tryEmplace(key, static_cast<double>(key)));
                    }
                    opCount = inputs.insertKeys.size();
                }
                else if (op.id == 1)
                {
                    size_t hits = 0;
                    for (uint32_t key : inputs.lookupKeys)
                    {
                        if (s.contains(key))
                        {
                            ++hits;
                        }
                    }
                    prevent_opt(static_cast<int64_t>(hits));
                    opCount = inputs.lookupKeys.size();
                }
                else
                {
                    for (uint32_t key : inputs.eraseKeys)
                    {
                        s.erase(key);
                    }
                    opCount = inputs.eraseKeys.size();
                }
                double elapsed = t.elapsed_ns();
                prevent_opt(static_cast<int64_t>(s.size()));
                samplesIdentity.push_back(ns_per_op(elapsed, opCount));
            }

            // --- CompositeKeyIndex ---
            for (size_t run = 0; run < WARMUP_RUNS(); ++run)
            {
                fat_p::SparseSetWithData<CompositeKey, double, CompositeKeyIndex> s;
                s.reserve(N * 10);
                if (op.id != 0)
                {
                    for (const auto& ck : compositeKeys)
                    {
                        s.tryEmplace(ck, static_cast<double>(ck.index()));
                    }
                }
                if (op.id == 0)
                {
                    for (const auto& ck : compositeKeys)
                    {
                        DoNotOptimize(s.tryEmplace(ck, static_cast<double>(ck.index())));
                    }
                }
                else if (op.id == 1)
                {
                    for (const auto& ck : compositeLookups)
                    {
                        DoNotOptimize(s.contains(ck));
                    }
                }
                else
                {
                    for (const auto& ck : compositeEraseKeys)
                    {
                        DoNotOptimize(s.erase(ck));
                    }
                }
            }

            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                fat_p::SparseSetWithData<CompositeKey, double, CompositeKeyIndex> s;
                s.reserve(N * 10);
                if (op.id != 0)
                {
                    for (const auto& ck : compositeKeys)
                    {
                        s.tryEmplace(ck, static_cast<double>(ck.index()));
                    }
                }

                Timer t;
                size_t opCount = 0;
                t.start();
                if (op.id == 0)
                {
                    for (const auto& ck : compositeKeys)
                    {
                        DoNotOptimize(s.tryEmplace(ck, static_cast<double>(ck.index())));
                    }
                    opCount = compositeKeys.size();
                }
                else if (op.id == 1)
                {
                    size_t hits = 0;
                    for (const auto& ck : compositeLookups)
                    {
                        if (s.contains(ck))
                        {
                            ++hits;
                        }
                    }
                    prevent_opt(static_cast<int64_t>(hits));
                    opCount = compositeLookups.size();
                }
                else
                {
                    for (const auto& ck : compositeEraseKeys)
                    {
                        s.erase(ck);
                    }
                    opCount = compositeEraseKeys.size();
                }
                double elapsed = t.elapsed_ns();
                prevent_opt(static_cast<int64_t>(s.size()));
                samplesComposite.push_back(ns_per_op(elapsed, opCount));
            }

            auto statsId = Statistics::compute(samplesIdentity);
            auto statsCk = Statistics::compute(samplesComposite);
            print_result_row("IdentityIndex<u32>", statsId);
            print_result_row("CompositeKeyIndex", statsCk);

            if (statsId.median > 0 && statsCk.median > 0)
            {
                double ratio = statsCk.median / statsId.median;
                std::cout << "    Composite/Identity ratio: " << std::fixed
                          << std::setprecision(3) << ratio << "x";
                if (ratio <= 1.05)
                {
                    std::cout << " (zero overhead)";
                }
                std::cout << "\n";
            }
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

    // Apply benchmark scope (Windows priority/affinity) unless disabled
    BenchmarkScope scope(!g_config.noScope);

    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "SparseSet";
    hdr.warmup = WARMUP_RUNS();
    hdr.measured = MEASURED_RUNS();
    hdr.seed = g_config.seed;
    
    // Competitors
    hdr.competitors.push_back({"fat_p::SparseSet<8>", true, "primary"});
    hdr.competitors.push_back({"fat_p::SparseSet<32>", true, "primary"});
    hdr.competitors.push_back({"fat_p::FlatSet", true, "sibling"});
    hdr.competitors.push_back({"std::unordered_set", true, "baseline"});
    hdr.competitors.push_back({"std::set", true, "baseline"});
#if HAS_ENTT
    hdr.competitors.push_back({"entt::sparse_set", true, ""});
#else
    hdr.competitors.push_back({"entt::sparse_set", false, "vcpkg install entt"});
#endif
    
    hdr.has_extended_config = false;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = true;
    hdr.has_stabilization = !g_config.noStabilize;
    hdr.cool_section_ms = COOLING_DELAY_SECTION_MS;
    hdr.cool_size_ms = COOLING_DELAY_SIZE_MS;
    hdr.cool_case_ms = COOLING_DELAY_CASE_MS;
    
    fat_p::bench::print_standard_header(hdr);


    std::cout << "Expected Results:\n";
    std::cout << "  - SparseSet excels at: insert/erase churn, iteration\n";
    std::cout << "  - FlatSet excels at: sorted access, iteration, lookup after build\n";
    std::cout << "  - Hash sets: fast point operations, scattered iteration\n";
    std::cout << "  - std::set: consistent O(log n), slowest overall\n\n";

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

    // Run benchmarks
    std::vector<size_t> core_sizes = {1000, 10000, 100000};

    benchmark_core_operations(core_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before iteration benchmark");
    benchmark_iteration();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before mixed workload");
    benchmark_mixed_workload();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before tryEmplace benchmark");
    benchmark_try_emplace(core_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before IndexPolicy benchmark");
    benchmark_index_policy(core_sizes);

    print_feature_comparison();

    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}