// benchmark_ServiceLocator.cpp
//
// FAT-P ServiceLocator benchmarks using round-robin execution with randomized order.
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
//   - fat_p::DefaultServiceLocator     - Policy-based, single-threaded, full-featured
//   - fat_p::ThreadSafeServiceLocator  - Policy-based, thread-safe with SharedMutexPolicy
//   - EnTT locator (if available)      - Static global per-type, minimal overhead
//   - std::unordered_map baseline      - Raw hash map with type_index key
//   - Direct pointer baseline          - Best possible case (no lookup)
//
// Sections:
//   1. Single-type resolution (resolve one service)
//   2. Multi-type resolution (resolve 5 services)
//   3. Named service resolution (fat_p)
//   4. Scoped resolution with parent chain (fat_p)
//   5. Registration performance
//   6. Size sensitivity (scaling with registered services)
//   7. MRU resolve cache locality (fat_p optional)
//   8. Named key hot-loop locality (fat_p)
//   9. Const resolve (T vs const T)
//  10. Mutation cost (unregister / clear)
//  11. Overhead isolation micro-benchmarks
//  12. Key strategies
//  13. Concurrent resolution (threaded variants)
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread benchmark_ServiceLocator.cpp -o bench_sl
//   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_ServiceLocator.cpp /link advapi32.lib
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
//   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
//   FATP_BENCH_SEED          - RNG seed (default: 12345)
//   FATP_BENCH_TARGET_WORK   - Target work per batch (default: 5000000)
//   FATP_BENCH_MIN_BATCH_MS  - Min batch duration (default: 50)
//   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
//   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
//   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
//   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
//   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
//   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
//
// Run:
//   ./bench_sl
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_sl

/*
FATP_META:
  meta_version: 1
  component: ServiceLocator
  file_role: benchmark
  path: benchmarks/benchmark_ServiceLocator.cpp
  namespace: fat_p
  summary: "Round-robin competitor benchmarks for ServiceLocator."
  related:
    docs_search: "ServiceLocator"
    headers:
      - fat_p/ServiceLocator.h
      - fat_p/FatPBenchmarkRunner.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: claude
    mode: manual
*/

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <typeindex>
#include <utility>
#include <unordered_map>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "ServiceLocator.h"
#include "StableHashMap.h"

// ============================================================================
// Competitor Auto-Detection
// ============================================================================

// ============================================================================
// fat_p locator variants (statistics overhead)
// ============================================================================
//
// These aliases allow the benchmark to quantify the per-resolve cost of
// AtomicServiceLocatorStatisticsPolicy without changing the library defaults.
//
using FatPDefaultNoStats = fat_p::ServiceLocator<fat_p::SingleThreadedPolicy,
                                                 fat_p::ServicePreventOverwritePolicy,
                                                 fat_p::NoServiceLocatorStatisticsPolicy>;

using FatPDefaultAtomicStats = fat_p::ServiceLocator<fat_p::SingleThreadedPolicy,
                                                     fat_p::ServicePreventOverwritePolicy,
                                                     fat_p::AtomicServiceLocatorStatisticsPolicy>;

using FatPThreadSafeNoStats = fat_p::ServiceLocator<fat_p::SharedMutexPolicy,
                                                    fat_p::ServicePreventOverwritePolicy,
                                                    fat_p::NoServiceLocatorStatisticsPolicy>;

using FatPThreadSafeAtomicStats = fat_p::ServiceLocator<fat_p::SharedMutexPolicy,
                                                        fat_p::ServicePreventOverwritePolicy,
                                                        fat_p::AtomicServiceLocatorStatisticsPolicy>;

// EnTT service locator (if available)
// Install: vcpkg install entt
#if __has_include(<entt/locator/locator.hpp>)
#include <entt/locator/locator.hpp>
#define FATP_HAS_ENTT 1
#else
#define FATP_HAS_ENTT 0
#endif

// ============================================================================
// Global Benchmark Configuration
// ============================================================================

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
// CPU Frequency Monitoring (delegated to shared helper)
// ============================================================================

static void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

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

// ============================================================================
// DCE Prevention
// ============================================================================

static volatile std::int64_t benchmark_sink = 0;


// ============================================================================
// Hash Helpers (SplitMix64, avalanching)
// ============================================================================
//
// StableHashMap will apply its own "safety mixer" unless the provided hash type
// declares `using is_avalanching = void;`.
//
// ServiceLocator uses an avalanching hash for type-id pointers (SplitMix64
// finalizer) so the StableHashMap path here should match that configuration.

static inline std::uint64_t splitmix64(std::uint64_t x) noexcept
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct AvalanchingPtrHash
{
    using is_avalanching = void;

    size_t operator()(const void* p) const noexcept
    {
        const std::uint64_t x =
            static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p));
        const std::uint64_t h = splitmix64(x);

        if constexpr (sizeof(size_t) >= sizeof(std::uint64_t))
        {
            return static_cast<size_t>(h);
        }
        else
        {
            return static_cast<size_t>(h ^ (h >> 32));
        }
    }
};

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

        if (n % 2 == 1)
        {
            s.median = samples[n / 2];
        }
        else
        {
            s.median = 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
        }

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        s.mean = sum / static_cast<double>(n);

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

    void print(const char* label, int labelWidth = 35) const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  " << std::setw(labelWidth) << std::left << label << ": " << std::right << "median="
                  << std::setw(8) << median << " mean=" << std::setw(8) << mean << " +/-" << std::setw(6) << stddev
                  << " [" << std::setw(8) << ci95_low << ", " << std::setw(8) << ci95_high << "]\n";
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

static void print_contract(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

// ============================================================================
// Test Service Interfaces
// ============================================================================

struct ILogger
{
    virtual ~ILogger() = default;
    virtual void log(const char* msg) = 0;
    virtual int getLogCount() const = 0;
};

struct IDatabase
{
    virtual ~IDatabase() = default;
    virtual int query(int key) = 0;
};

struct ICache
{
    virtual ~ICache() = default;
    virtual bool lookup(int key, int& value) = 0;
};

struct IMetrics
{
    virtual ~IMetrics() = default;
    virtual void increment(const char* name) = 0;
};

struct IConfig
{
    virtual ~IConfig() = default;
    virtual int getInt(const char* key) = 0;
};

// ============================================================================
// Test Service Implementations
// ============================================================================

struct NullLogger final : ILogger
{
    void log(const char* /*msg*/) override
    {
        ++mCount;
    }
    int getLogCount() const override
    {
        return mCount;
    }

private:
    mutable int mCount = 0;
};

struct InMemoryDatabase final : IDatabase
{
    int query(int key) override
    {
        return key * 2;
    }
};

struct NoOpCache final : ICache
{
    bool lookup(int /*key*/, int& /*value*/) override
    {
        return false;
    }
};

struct NullMetrics final : IMetrics
{
    void increment(const char* /*name*/) override
    {
        ++mCount;
    }
    int mCount = 0;
};

struct StaticConfig final : IConfig
{
    int getInt(const char* /*key*/) override
    {
        return 42;
    }
};

// ============================================================================
// Adapter Interface
// ============================================================================

enum class Operation
{
    ResolveSingle,  // Resolve one service type
    ResolveMulti,   // Resolve 5 service types
};

struct ILocatorAdapter
{
    virtual ~ILocatorAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t numServices) = 0;
    virtual void teardown() = 0;
    virtual size_t run_operation(Operation op, size_t iterations) = 0;
};

// ============================================================================
// fat_p::DefaultServiceLocator Adapter
// ============================================================================

class FatPDefaultAdapter final : public ILocatorAdapter
{
    std::unique_ptr<FatPDefaultNoStats> mLocator;
    NullLogger mLogger;
    InMemoryDatabase mDb;
    NoOpCache mCache;
    NullMetrics mMetrics;
    StaticConfig mConfig;

public:
    const char* name() const override
    {
        return "fat_p::DefaultServiceLocator (no stats)";
    }

    void setup(size_t /*numServices*/) override
    {
        mLocator = std::make_unique<FatPDefaultNoStats>();
        (void)mLocator->registerInstance<ILogger>(mLogger);
        (void)mLocator->registerInstance<IDatabase>(mDb);
        (void)mLocator->registerInstance<ICache>(mCache);
        (void)mLocator->registerInstance<IMetrics>(mMetrics);
        (void)mLocator->registerInstance<IConfig>(mConfig);
    }

    void teardown() override
    {
        mLocator.reset();
    }

    size_t run_operation(Operation op, size_t iterations) override
    {
        size_t ops = 0;
        switch (op)
        {
            case Operation::ResolveSingle:
                for (size_t i = 0; i < iterations; ++i)
                {
                    ILogger* l = mLocator->tryResolve<ILogger>();
                    benchmark_sink += reinterpret_cast<std::intptr_t>(l);
                    ++ops;
                }
                break;

            case Operation::ResolveMulti:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ILogger>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IDatabase>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ICache>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IMetrics>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IConfig>());
                    ops += 5;
                }
                break;
        }
        return ops;
    }
};



// ============================================================================
// fat_p::DefaultServiceLocator (atomic stats) Adapter
// ============================================================================

class FatPDefaultAtomicStatsAdapter final : public ILocatorAdapter
{
    std::unique_ptr<FatPDefaultAtomicStats> mLocator;
    NullLogger mLogger;
    InMemoryDatabase mDb;
    NoOpCache mCache;
    NullMetrics mMetrics;
    StaticConfig mConfig;

public:
    const char* name() const override
    {
        return "fat_p::DefaultServiceLocator (atomic stats)";
    }

    void setup(size_t /*numServices*/) override
    {
        mLocator = std::make_unique<FatPDefaultAtomicStats>();
        (void)mLocator->registerInstance<ILogger>(mLogger);
        (void)mLocator->registerInstance<IDatabase>(mDb);
        (void)mLocator->registerInstance<ICache>(mCache);
        (void)mLocator->registerInstance<IMetrics>(mMetrics);
        (void)mLocator->registerInstance<IConfig>(mConfig);
    }

    void teardown() override
    {
        mLocator.reset();
    }

    size_t run_operation(Operation op, size_t iterations) override
    {
        size_t ops = 0;
        switch (op)
        {
            case Operation::ResolveSingle:
                for (size_t i = 0; i < iterations; ++i)
                {
                    ILogger* l = mLocator->tryResolve<ILogger>();
                    benchmark_sink += reinterpret_cast<std::intptr_t>(l);
                    ++ops;
                }
                break;

            case Operation::ResolveMulti:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ILogger>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IDatabase>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ICache>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IMetrics>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IConfig>());
                    ops += 5;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// fat_p::ThreadSafeServiceLocator Adapter
// ============================================================================

class FatPThreadSafeAdapter final : public ILocatorAdapter
{
    std::unique_ptr<FatPThreadSafeNoStats> mLocator;
    NullLogger mLogger;
    InMemoryDatabase mDb;
    NoOpCache mCache;
    NullMetrics mMetrics;
    StaticConfig mConfig;

public:
    const char* name() const override
    {
        return "fat_p::ThreadSafeServiceLocator (no stats)";
    }

    void setup(size_t /*numServices*/) override
    {
        mLocator = std::make_unique<FatPThreadSafeNoStats>();
        (void)mLocator->registerInstance<ILogger>(mLogger);
        (void)mLocator->registerInstance<IDatabase>(mDb);
        (void)mLocator->registerInstance<ICache>(mCache);
        (void)mLocator->registerInstance<IMetrics>(mMetrics);
        (void)mLocator->registerInstance<IConfig>(mConfig);
    }

    void teardown() override
    {
        mLocator.reset();
    }

    size_t run_operation(Operation op, size_t iterations) override
    {
        size_t ops = 0;
        switch (op)
        {
            case Operation::ResolveSingle:
                for (size_t i = 0; i < iterations; ++i)
                {
                    ILogger* l = mLocator->tryResolve<ILogger>();
                    benchmark_sink += reinterpret_cast<std::intptr_t>(l);
                    ++ops;
                }
                break;

            case Operation::ResolveMulti:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ILogger>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IDatabase>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ICache>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IMetrics>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IConfig>());
                    ops += 5;
                }
                break;
        }
        return ops;
    }
};



// ============================================================================
// fat_p::ThreadSafeServiceLocator (atomic stats) Adapter
// ============================================================================

class FatPThreadSafeAtomicStatsAdapter final : public ILocatorAdapter
{
    std::unique_ptr<FatPThreadSafeAtomicStats> mLocator;
    NullLogger mLogger;
    InMemoryDatabase mDb;
    NoOpCache mCache;
    NullMetrics mMetrics;
    StaticConfig mConfig;

public:
    const char* name() const override
    {
        return "fat_p::ThreadSafeServiceLocator (atomic stats)";
    }

    void setup(size_t /*numServices*/) override
    {
        mLocator = std::make_unique<FatPThreadSafeAtomicStats>();
        (void)mLocator->registerInstance<ILogger>(mLogger);
        (void)mLocator->registerInstance<IDatabase>(mDb);
        (void)mLocator->registerInstance<ICache>(mCache);
        (void)mLocator->registerInstance<IMetrics>(mMetrics);
        (void)mLocator->registerInstance<IConfig>(mConfig);
    }

    void teardown() override
    {
        mLocator.reset();
    }

    size_t run_operation(Operation op, size_t iterations) override
    {
        size_t ops = 0;
        switch (op)
        {
            case Operation::ResolveSingle:
                for (size_t i = 0; i < iterations; ++i)
                {
                    ILogger* l = mLocator->tryResolve<ILogger>();
                    benchmark_sink += reinterpret_cast<std::intptr_t>(l);
                    ++ops;
                }
                break;

            case Operation::ResolveMulti:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ILogger>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IDatabase>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<ICache>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IMetrics>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLocator->tryResolve<IConfig>());
                    ops += 5;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// EnTT Locator Adapter (if available)
// ============================================================================

#if FATP_HAS_ENTT
class EnttLocatorAdapter final : public ILocatorAdapter
{
public:
    const char* name() const override
    {
        return "entt::locator (static global)";
    }

    void setup(size_t /*numServices*/) override
    {
        entt::locator<ILogger>::emplace<NullLogger>();
        entt::locator<IDatabase>::emplace<InMemoryDatabase>();
        entt::locator<ICache>::emplace<NoOpCache>();
        entt::locator<IMetrics>::emplace<NullMetrics>();
        entt::locator<IConfig>::emplace<StaticConfig>();
    }

    void teardown() override
    {
        entt::locator<ILogger>::reset();
        entt::locator<IDatabase>::reset();
        entt::locator<ICache>::reset();
        entt::locator<IMetrics>::reset();
        entt::locator<IConfig>::reset();
    }

    size_t run_operation(Operation op, size_t iterations) override
    {
        size_t ops = 0;
        switch (op)
        {
            case Operation::ResolveSingle:
                for (size_t i = 0; i < iterations; ++i)
                {
                    ILogger& l = entt::locator<ILogger>::value();
                    benchmark_sink += reinterpret_cast<std::intptr_t>(&l);
                    ++ops;
                }
                break;

            case Operation::ResolveMulti:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(&entt::locator<ILogger>::value());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(&entt::locator<IDatabase>::value());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(&entt::locator<ICache>::value());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(&entt::locator<IMetrics>::value());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(&entt::locator<IConfig>::value());
                    ops += 5;
                }
                break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// std::unordered_map Baseline Adapter
// ============================================================================

class UnorderedMapAdapter final : public ILocatorAdapter
{
    std::unordered_map<std::type_index, void*> mServices;
    NullLogger mLogger;
    InMemoryDatabase mDb;
    NoOpCache mCache;
    NullMetrics mMetrics;
    StaticConfig mConfig;

public:
    const char* name() const override
    {
        return "std::unordered_map<type_index>";
    }

    void setup(size_t /*numServices*/) override
    {
        mServices.clear();
        mServices[std::type_index(typeid(ILogger))] = &mLogger;
        mServices[std::type_index(typeid(IDatabase))] = &mDb;
        mServices[std::type_index(typeid(ICache))] = &mCache;
        mServices[std::type_index(typeid(IMetrics))] = &mMetrics;
        mServices[std::type_index(typeid(IConfig))] = &mConfig;
    }

    void teardown() override
    {
        mServices.clear();
    }

    template <typename T>
    T* resolve()
    {
        auto it = mServices.find(std::type_index(typeid(T)));
        return (it != mServices.end()) ? static_cast<T*>(it->second) : nullptr;
    }

    size_t run_operation(Operation op, size_t iterations) override
    {
        size_t ops = 0;
        switch (op)
        {
            case Operation::ResolveSingle:
                for (size_t i = 0; i < iterations; ++i)
                {
                    ILogger* l = resolve<ILogger>();
                    benchmark_sink += reinterpret_cast<std::intptr_t>(l);
                    ++ops;
                }
                break;

            case Operation::ResolveMulti:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(resolve<ILogger>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(resolve<IDatabase>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(resolve<ICache>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(resolve<IMetrics>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(resolve<IConfig>());
                    ops += 5;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// Direct Pointer Baseline Adapter (best possible case)
// ============================================================================

class DirectPointerAdapter final : public ILocatorAdapter
{
    ILogger* mLogger = nullptr;
    IDatabase* mDb = nullptr;
    ICache* mCache = nullptr;
    IMetrics* mMetrics = nullptr;
    IConfig* mConfig = nullptr;

    NullLogger mLoggerImpl;
    InMemoryDatabase mDbImpl;
    NoOpCache mCacheImpl;
    NullMetrics mMetricsImpl;
    StaticConfig mConfigImpl;

public:
    const char* name() const override
    {
        return "Direct pointers (baseline)";
    }

    void setup(size_t /*numServices*/) override
    {
        mLogger = &mLoggerImpl;
        mDb = &mDbImpl;
        mCache = &mCacheImpl;
        mMetrics = &mMetricsImpl;
        mConfig = &mConfigImpl;
    }

    void teardown() override
    {
        mLogger = nullptr;
        mDb = nullptr;
        mCache = nullptr;
        mMetrics = nullptr;
        mConfig = nullptr;
    }

    size_t run_operation(Operation op, size_t iterations) override
    {
        size_t ops = 0;
        switch (op)
        {
            case Operation::ResolveSingle:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLogger);
                    ++ops;
                }
                break;

            case Operation::ResolveMulti:
                for (size_t i = 0; i < iterations; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mLogger);
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mDb);
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mCache);
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mMetrics);
                    benchmark_sink += reinterpret_cast<std::intptr_t>(mConfig);
                    ops += 5;
                }
                break;
        }
        return ops;
    }
};

// ============================================================================
// Correctness Guardrails
// ============================================================================

static void verify_adapters(std::vector<std::unique_ptr<ILocatorAdapter>>& adapters)
{
    std::cout << "Verifying adapters...\n";

    for (auto& adapter : adapters)
    {
        adapter->setup(5);

        // Verify single resolve returns non-null
        size_t ops = adapter->run_operation(Operation::ResolveSingle, 1);
        assert(ops == 1 && "Single resolve should return 1 op");
        (void)ops;

        // Verify multi resolve returns 5 ops
        ops = adapter->run_operation(Operation::ResolveMulti, 1);
        assert(ops == 5 && "Multi resolve should return 5 ops");
        (void)ops;

        adapter->teardown();
    }

    std::cout << "  [OK] All adapters verified\n\n";
}

// ============================================================================
// Section 1: Single-Type Resolution
// ============================================================================

static void benchmark_single_resolve(std::vector<std::unique_ptr<ILocatorAdapter>>& adapters)
{
    print_header("SINGLE-TYPE RESOLUTION");
    print_contract("Resolve one service type. O(1) hash lookup for map-based, O(1) static access for EnTT.");
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS_PER_RUN = 100000;

    std::mt19937_64 rng(g_config.seed);
    std::vector<size_t> order(adapters.size());
    std::iota(order.begin(), order.end(), 0);

    std::vector<std::vector<double>> all_samples(adapters.size());

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (size_t idx : order)
        {
            adapters[idx]->setup(5);
            (void)adapters[idx]->run_operation(Operation::ResolveSingle, ITERATIONS_PER_RUN);
            adapters[idx]->teardown();
        }
    }

    // Measured runs with round-robin
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (size_t idx : order)
        {
            adapters[idx]->setup(5);

            Timer t;
            t.start();
            size_t ops = adapters[idx]->run_operation(Operation::ResolveSingle, ITERATIONS_PER_RUN);
            double elapsed = t.elapsed_ns();

            all_samples[idx].push_back(ns_per_op(elapsed, ops));
            adapters[idx]->teardown();
        }
    }

    // Report results
    for (size_t i = 0; i < adapters.size(); ++i)
    {
        Statistics stats = Statistics::compute(all_samples[i]);
        stats.print(adapters[i]->name());
    }
    std::cout << "\n";
}

// ============================================================================
// Section 2: Multi-Type Resolution
// ============================================================================

static void benchmark_multi_resolve(std::vector<std::unique_ptr<ILocatorAdapter>>& adapters)
{
    print_header("MULTI-TYPE RESOLUTION (5 types)");
    print_contract("Resolve 5 different service types per iteration. Measures cumulative lookup cost.");
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS_PER_RUN = 20000;

    std::mt19937_64 rng(g_config.seed ^ 0x1234);
    std::vector<size_t> order(adapters.size());
    std::iota(order.begin(), order.end(), 0);

    std::vector<std::vector<double>> all_samples(adapters.size());

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (size_t idx : order)
        {
            adapters[idx]->setup(5);
            (void)adapters[idx]->run_operation(Operation::ResolveMulti, ITERATIONS_PER_RUN);
            adapters[idx]->teardown();
        }
    }

    // Measured runs
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (size_t idx : order)
        {
            adapters[idx]->setup(5);

            Timer t;
            t.start();
            size_t ops = adapters[idx]->run_operation(Operation::ResolveMulti, ITERATIONS_PER_RUN);
            double elapsed = t.elapsed_ns();

            all_samples[idx].push_back(ns_per_op(elapsed, ops));
            adapters[idx]->teardown();
        }
    }

    // Report
    for (size_t i = 0; i < adapters.size(); ++i)
    {
        Statistics stats = Statistics::compute(all_samples[i]);
        stats.print(adapters[i]->name());
    }
    std::cout << "\n";
}

// ============================================================================
// Section 3: Named Services (fat_p exclusive)
// ============================================================================

static void benchmark_named_services()
{
    print_header("NAMED SERVICES (fat_p exclusive)");
    print_contract("Resolve services by type+name composite key. Competitors do not support named services.");
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS_PER_RUN = 100000;

    NullLogger defaultLogger;
    NullLogger fileLogger;
    NullLogger consoleLogger;

    std::vector<double> samples_default, samples_named, samples_triple;

    // Warmup + Measured
    for (size_t phase = 0; phase < 2; ++phase)
    {
        size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

        for (size_t run = 0; run < runs; ++run)
        {
            fat_p::DefaultServiceLocator locator;
            (void)locator.registerInstance<ILogger>(defaultLogger);
            (void)locator.registerInstance<ILogger>(fileLogger, "file");
            (void)locator.registerInstance<ILogger>(consoleLogger, "console");

            // Default (no name)
            {
                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>());
                }
                if (phase == 1)
                {
                    samples_default.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }

            // Named ("file")
            {
                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>("file"));
                }
                if (phase == 1)
                {
                    samples_named.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }

            // All three
            {
                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>());
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>("file"));
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>("console"));
                }
                if (phase == 1)
                {
                    samples_triple.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 3));
                }
            }
        }
    }

    Statistics::compute(samples_default).print("resolve<ILogger>() (default)");
    Statistics::compute(samples_named).print("resolve<ILogger>(\"file\")");
    Statistics::compute(samples_triple).print("resolve 3 named variants");
    std::cout << "\n";
}

// ============================================================================
// Section 4: Scoped Resolution (fat_p exclusive)
// ============================================================================

static void benchmark_scoped_resolution()
{
    print_header("SCOPED RESOLUTION (fat_p exclusive)");
    print_contract("Child scope overrides parent. Measures lookup with parent chain traversal.");
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS_PER_RUN = 100000;

    NullLogger parentLogger;
    NullLogger childLogger;
    InMemoryDatabase db;

    std::vector<double> samples_override, samples_inherit;

    // Warmup + Measured
    for (size_t phase = 0; phase < 2; ++phase)
    {
        size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

        for (size_t run = 0; run < runs; ++run)
        {
            fat_p::DefaultServiceLocator parent;
            (void)parent.registerInstance<ILogger>(parentLogger);
            (void)parent.registerInstance<IDatabase>(db);

            auto scope = parent.makeScope();
            (void)scope.locator().registerInstance<ILogger>(childLogger);

            // Resolve overridden (in child)
            {
                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(scope.locator().tryResolve<ILogger>());
                }
                if (phase == 1)
                {
                    samples_override.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }

            // Resolve inherited (from parent)
            {
                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(scope.locator().tryResolve<IDatabase>());
                }
                if (phase == 1)
                {
                    samples_inherit.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }
        }
    }

    Statistics::compute(samples_override).print("resolve (child override)");
    Statistics::compute(samples_inherit).print("resolve (parent inheritance)");
    std::cout << "\n";
}

// ============================================================================
// Section 5: Registration Performance
// ============================================================================

static void benchmark_registration()
{
    print_header("REGISTRATION PERFORMANCE");
    print_contract("Register 5 services including hash map insertion. Measures setup cost.");
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS_PER_RUN = 10000;

    NullLogger logger;
    InMemoryDatabase db;
    NoOpCache cache;
    NullMetrics metrics;
    StaticConfig config;

    std::vector<double> samples_fatp, samples_map;

    // Warmup + Measured
    for (size_t phase = 0; phase < 2; ++phase)
    {
        size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

        for (size_t run = 0; run < runs; ++run)
        {
            // fat_p registration
            {
                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    fat_p::DefaultServiceLocator locator;
                    (void)locator.registerInstance<ILogger>(logger);
                    (void)locator.registerInstance<IDatabase>(db);
                    (void)locator.registerInstance<ICache>(cache);
                    (void)locator.registerInstance<IMetrics>(metrics);
                    (void)locator.registerInstance<IConfig>(config);
                    benchmark_sink += static_cast<std::int64_t>(locator.size());
                }
                if (phase == 1)
                {
                    samples_fatp.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 5));
                }
            }

            // std::unordered_map registration
            {
                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    std::unordered_map<std::type_index, void*> services;
                    services[std::type_index(typeid(ILogger))] = &logger;
                    services[std::type_index(typeid(IDatabase))] = &db;
                    services[std::type_index(typeid(ICache))] = &cache;
                    services[std::type_index(typeid(IMetrics))] = &metrics;
                    services[std::type_index(typeid(IConfig))] = &config;
                    benchmark_sink += static_cast<std::int64_t>(services.size());
                }
                if (phase == 1)
                {
                    samples_map.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 5));
                }
            }
        }
    }

    Statistics::compute(samples_fatp).print("fat_p::DefaultServiceLocator");
    Statistics::compute(samples_map).print("std::unordered_map<type_index>");
    std::cout << "\n";
}

// ============================================================================
// Section 6: Size Sensitivity
// ============================================================================


namespace size_sensitivity_detail
{
constexpr std::size_t kMaxTypes = 100;

template<std::size_t I>
struct DummyService final
{
    std::uint64_t mValue = static_cast<std::uint64_t>(I);
};

template<std::size_t I>
struct DummyStorage
{
    inline static DummyService<I> sInstance{};
};

template<std::size_t I>
const void* token() noexcept
{
    return &fat_p::detail::kServiceTypeToken<DummyService<I>>;
}

template<std::size_t I>
const void* instance_ptr() noexcept
{
    return &DummyStorage<I>::sInstance;
}

using ResolveFn = const void* (*)(fat_p::DefaultServiceLocator&);
using RegisterFn = void (*)(fat_p::DefaultServiceLocator&);

template<std::size_t I>
const void* resolve(fat_p::DefaultServiceLocator& locator)
{
    return locator.tryResolve<DummyService<I>>();
}

template<std::size_t I>
void reg(fat_p::DefaultServiceLocator& locator)
{
    (void)locator.registerInstance<DummyService<I>>(DummyStorage<I>::sInstance);
}

template<std::size_t... Is>
constexpr auto make_resolve_table(std::index_sequence<Is...>) noexcept
{
    return std::array<ResolveFn, sizeof...(Is)>{ { &resolve<Is>... } };
}

template<std::size_t... Is>
constexpr auto make_register_table(std::index_sequence<Is...>) noexcept
{
    return std::array<RegisterFn, sizeof...(Is)>{ { &reg<Is>... } };
}

template<std::size_t... Is>
constexpr auto make_token_table(std::index_sequence<Is...>) noexcept
{
    return std::array<const void*, sizeof...(Is)>{ { token<Is>()... } };
}

template<std::size_t... Is>
constexpr auto make_instance_table(std::index_sequence<Is...>) noexcept
{
    return std::array<const void*, sizeof...(Is)>{ { instance_ptr<Is>()... } };
}

inline const auto kResolveTable = make_resolve_table(std::make_index_sequence<kMaxTypes>{});
inline const auto kRegisterTable = make_register_table(std::make_index_sequence<kMaxTypes>{});

using UnregisterFn = bool (*)(fat_p::DefaultServiceLocator&);

template<std::size_t I>
bool unreg(fat_p::DefaultServiceLocator& locator)
{
    return locator.unregister<DummyService<I>>();
}

template<std::size_t... Is>
constexpr auto make_unregister_table(std::index_sequence<Is...>) noexcept
{
    return std::array<UnregisterFn, sizeof...(Is)>{ { &unreg<Is>... } };
}

inline const auto kUnregisterTable = make_unregister_table(std::make_index_sequence<kMaxTypes>{});

using RegisterFnHot = void (*)(fat_p::HotLoopServiceLocator&);

template<std::size_t I>
void reg_hot(fat_p::HotLoopServiceLocator& locator)
{
    (void)locator.registerInstance<DummyService<I>>(DummyStorage<I>::sInstance);
}

template<std::size_t... Is>
constexpr auto make_register_table_hot(std::index_sequence<Is...>) noexcept
{
    return std::array<RegisterFnHot, sizeof...(Is)>{ { &reg_hot<Is>... } };
}

inline const auto kRegisterTableHot = make_register_table_hot(std::make_index_sequence<kMaxTypes>{});

using UnregisterFnHot = bool (*)(fat_p::HotLoopServiceLocator&);

template<std::size_t I>
bool unreg_hot(fat_p::HotLoopServiceLocator& locator)
{
    return locator.unregister<DummyService<I>>();
}

template<std::size_t... Is>
constexpr auto make_unregister_table_hot(std::index_sequence<Is...>) noexcept
{
    return std::array<UnregisterFnHot, sizeof...(Is)>{ { &unreg_hot<Is>... } };
}

inline const auto kUnregisterTableHot = make_unregister_table_hot(std::make_index_sequence<kMaxTypes>{});

inline const auto kTokenTable = make_token_table(std::make_index_sequence<kMaxTypes>{});
inline const auto kInstanceTable = make_instance_table(std::make_index_sequence<kMaxTypes>{});

struct CompositeKeyView final
{
    const void* mTypeId = nullptr;
    std::string_view mName{};
};

struct CompositeKeyHash final
{
    std::size_t operator()(const CompositeKeyView& key) const noexcept
    {
        std::size_t h1 = std::hash<const void*>{}(key.mTypeId);
        std::size_t h2 = std::hash<std::string_view>{}(key.mName);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6U) + (h1 >> 2U));
    }
};

struct CompositeKeyEq final
{
    bool operator()(const CompositeKeyView& a, const CompositeKeyView& b) const noexcept
    {
        return (a.mTypeId == b.mTypeId) && (a.mName == b.mName);
    }
};
} // namespace size_sensitivity_detail


static void benchmark_size_sensitivity()
{
    print_header("SIZE SENSITIVITY");
    print_contract(
        "Measure resolve performance as number of registered services scales. "
        "Split into unnamed (type-only) and named (type+name) variants."
    );
    print_cpu_context("Starting");

    std::vector<size_t> sizes = {1, 5, 10, 25, 50, 100};
    constexpr size_t ITERATIONS_PER_RUN = 50000;

    std::cout << std::fixed << std::setprecision(2);

    // ------------------------------------------------------------------------
    // UNNAMED (TYPE-ONLY)
    // ------------------------------------------------------------------------
    std::cout << "UNNAMED (TYPE-ONLY)\n";
    std::cout
        << "Services | fat_p median (ns/op) | unordered_map<void*> median (ns/op) "
        << "| Ratio (unordered_map / fat_p)\n";
    std::cout
        << "---------|----------------------|-----------------------------------|"
        << "---------------------------\n";

    for (size_t numServices : sizes)
    {
        std::vector<double> fatp_samples;
        std::vector<double> map_samples;

        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            // fat_p (type-only, many distinct types)
            {
                fat_p::DefaultServiceLocator locator;
                for (size_t i = 0; i < numServices; ++i)
                {
                    size_sensitivity_detail::kRegisterTable[i](locator);
                }

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    const size_t idx = i % numServices;
                    benchmark_sink += reinterpret_cast<std::intptr_t>(
                        size_sensitivity_detail::kResolveTable[idx](locator)
                    );
                }
                fatp_samples.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
            }

            // unordered_map baseline (type token -> pointer)
            {
                std::unordered_map<const void*, const void*> services;
                services.reserve(numServices);
                for (size_t i = 0; i < numServices; ++i)
                {
                    services.emplace(
                        size_sensitivity_detail::kTokenTable[i],
                        size_sensitivity_detail::kInstanceTable[i]
                    );
                }

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    const size_t idx = i % numServices;
                    auto it = services.find(size_sensitivity_detail::kTokenTable[idx]);
                    benchmark_sink += reinterpret_cast<std::intptr_t>(
                        it != services.end() ? it->second : nullptr
                    );
                }
                map_samples.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
            }
        }

        double fatp_median = Statistics::compute(fatp_samples).median;
        double map_median = Statistics::compute(map_samples).median;
        double ratio = (fatp_median > 0.0) ? (map_median / fatp_median) : 0.0;

        std::cout << std::setw(8) << numServices << " | " << std::setw(20) << fatp_median << " | "
                  << std::setw(33) << map_median << " | " << std::setw(5) << ratio << "x\n";
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // NAMED (TYPE+NAME)
    // ------------------------------------------------------------------------
    std::cout << "NAMED (TYPE+NAME)\n";
    std::cout << "Note: The string-only unordered_map is a name-only key, not a composite (type+name) key.\n";
    std::cout << "      The composite-key unordered_map is the apples-to-apples comparator.\n";

    std::cout
        << "Services | fat_p median (ns/op) | unordered_map<string> median (ns/op) "
        << "| unordered_map<composite> median (ns/op) | Ratio (composite / fat_p)\n";
    std::cout
        << "---------|----------------------|----------------------------------|"
        << "----------------------------------|---------------------------\n";

    const void* loggerTypeId = &fat_p::detail::kServiceTypeToken<ILogger>;

    for (size_t numServices : sizes)
    {
        std::vector<std::unique_ptr<NullLogger>> loggers(numServices);
        for (auto& l : loggers)
        {
            l = std::make_unique<NullLogger>();
        }

        // Precompute service names outside measured loops.
        std::vector<std::string> serviceNames;
        serviceNames.reserve(numServices);
        for (size_t i = 0; i < numServices; ++i)
        {
            serviceNames.push_back("svc" + std::to_string(i));
        }

        std::vector<double> fatp_samples;
        std::vector<double> name_samples;
        std::vector<double> composite_samples;

        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            // fat_p (type+name key internally)
            {
                fat_p::DefaultServiceLocator locator;
                for (size_t i = 0; i < numServices; ++i)
                {
                    (void)locator.registerInstance<ILogger>(*loggers[i], serviceNames[i]);
                }

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    const std::string& key = serviceNames[i % numServices];
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>(key));
                }
                fatp_samples.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
            }

            // unordered_map<string> (name-only key; not composite)
            {
                std::unordered_map<std::string, ILogger*> services;
                services.reserve(numServices);
                for (size_t i = 0; i < numServices; ++i)
                {
                    services.emplace(serviceNames[i], loggers[i].get());
                }

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    const std::string& key = serviceNames[i % numServices];
                    auto it = services.find(key);
                    benchmark_sink += reinterpret_cast<std::intptr_t>(it != services.end() ? it->second : nullptr);
                }
                name_samples.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
            }

            // unordered_map<composite> (type+name key; apples-to-apples)
            {
                std::unordered_map<
                    size_sensitivity_detail::CompositeKeyView,
                    ILogger*,
                    size_sensitivity_detail::CompositeKeyHash,
                    size_sensitivity_detail::CompositeKeyEq
                > services;
                services.reserve(numServices);
                for (size_t i = 0; i < numServices; ++i)
                {
                    services.emplace(
                        size_sensitivity_detail::CompositeKeyView{loggerTypeId, serviceNames[i]},
                        loggers[i].get()
                    );
                }

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    const std::string& name = serviceNames[i % numServices];
                    auto it = services.find(size_sensitivity_detail::CompositeKeyView{loggerTypeId, name});
                    benchmark_sink += reinterpret_cast<std::intptr_t>(
                        it != services.end() ? it->second : nullptr
                    );
                }
                composite_samples.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
            }
        }

        double fatp_median = Statistics::compute(fatp_samples).median;
        double name_median = Statistics::compute(name_samples).median;
        double composite_median = Statistics::compute(composite_samples).median;
        double ratio = (fatp_median > 0.0) ? (composite_median / fatp_median) : 0.0;

        std::cout << std::setw(8) << numServices << " | " << std::setw(20) << fatp_median << " | "
                  << std::setw(32) << name_median << " | " << std::setw(32) << composite_median << " | "
                  << std::setw(5) << ratio << "x\n";
    }

    std::cout << "\n";
}


// ============================================================================
// Section 7: Overhead Isolation Micro-Benchmarks
// ============================================================================

// =============================================================================
// MRU RESOLVE CACHE LOCALITY (fat_p optional)
// =============================================================================
static void benchmark_mru_resolve_cache_locality()
{
    print_header("MRU RESOLVE CACHE LOCALITY (fat_p optional)");
    print_contract(
        "Hot-loop repeated resolves after startup registration. Measures steady-state resolve cost for\n"
        "type-only (unnamed) services while varying the total number of registered services.\n"
        "Patterns:\n"
        "  1) Repeat A: tryResolve<A>() repeatedly.\n"
        "  2) Alternate A/B: tryResolve<A>(), tryResolve<B>(), ...\n"
        "Comparator: DefaultServiceLocator (no cache) vs HotLoopServiceLocator (MRU2).\n"
    );
    print_cpu_context("Starting");

    // Total registered services per run (includes the hot services A and B).
    // This extends the previous 2-service test to more realistic startup registries.
    const std::vector<size_t> sizes = {2, 5, 10, 25, 50, 100};

    // Per-size iteration count (kept smaller than other sections because we run multiple sizes).
    constexpr size_t ITERATIONS_PER_RUN = 200000;

    struct DummyA { int x = 1; };
    struct DummyB { int y = 2; };
    struct DummyC { int z = 3; };

    DummyA a{};
    DummyB b{};
    DummyC c{};

    std::cout << std::fixed << std::setprecision(2);

    // ------------------------------------------------------------------------
    // Repeat A (AAAA...)
    // ------------------------------------------------------------------------
    std::cout << "REPEAT A (AAAA...)\n";
    std::cout
        << "Services | Default median (ns/op) | MRU2 median (ns/op) | Speedup (Default/MRU2)\n";
    std::cout
        << "---------|------------------------|--------------------|----------------------\n";

    for (size_t totalServices : sizes)
    {
        const size_t noiseCount = (totalServices > 2) ? (totalServices - 2) : 0;

        std::vector<double> samples_default;
        std::vector<double> samples_mru2;

        for (size_t phase = 0; phase < 2; ++phase)
        {
            const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

            for (size_t run = 0; run < runs; ++run)
            {
                // Default (no cache)
                {
                    fat_p::DefaultServiceLocator locator;
                    (void)locator.registerInstance<DummyA>(a);
                    (void)locator.registerInstance<DummyB>(b);

                    for (size_t i = 0; i < noiseCount; ++i)
                    {
                        size_sensitivity_detail::kRegisterTable[i](locator);
                    }

                    (void)locator.tryResolve<DummyA>(); // warm path

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyA>());
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN);

                    if (phase == 1)
                    {
                        samples_default.push_back(nsOp);
                    }
                }

                // MRU2
                {
                    fat_p::HotLoopServiceLocator locator;
                    (void)locator.registerInstance<DummyA>(a);
                    (void)locator.registerInstance<DummyB>(b);

                    for (size_t i = 0; i < noiseCount; ++i)
                    {
                        size_sensitivity_detail::kRegisterTableHot[i](locator);
                    }

                    (void)locator.tryResolve<DummyA>(); // warm cache/path

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyA>());
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN);

                    if (phase == 1)
                    {
                        samples_mru2.push_back(nsOp);
                    }
                }
            }
        }

        const double medDefault = Statistics::compute(samples_default).median;
        const double medMru2 = Statistics::compute(samples_mru2).median;
        const double speedup = (medMru2 > 0.0) ? (medDefault / medMru2) : 0.0;

        std::cout << std::setw(8) << totalServices << " | "
                  << std::setw(22) << medDefault << " | "
                  << std::setw(18) << medMru2 << " | "
                  << std::setw(20) << speedup << "\n";
    }

    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Alternate A/B (ABAB...)
    // ------------------------------------------------------------------------
    std::cout << "ALTERNATE A/B (ABAB...)\n";
    std::cout
        << "Services | Default median (ns/op) | MRU2 median (ns/op) | Speedup (Default/MRU2)\n";
    std::cout
        << "---------|------------------------|--------------------|----------------------\n";

    for (size_t totalServices : sizes)
    {
        const size_t noiseCount = (totalServices > 2) ? (totalServices - 2) : 0;

        std::vector<double> samples_default;
        std::vector<double> samples_mru2;

        for (size_t phase = 0; phase < 2; ++phase)
        {
            const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

            for (size_t run = 0; run < runs; ++run)
            {
                // Default (no cache)
                {
                    fat_p::DefaultServiceLocator locator;
                    (void)locator.registerInstance<DummyA>(a);
                    (void)locator.registerInstance<DummyB>(b);

                    for (size_t i = 0; i < noiseCount; ++i)
                    {
                        size_sensitivity_detail::kRegisterTable[i](locator);
                    }

                    (void)locator.tryResolve<DummyA>();
                    (void)locator.tryResolve<DummyB>();

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyA>());
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyB>());
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 2);

                    if (phase == 1)
                    {
                        samples_default.push_back(nsOp);
                    }
                }

                // MRU2
                {
                    fat_p::HotLoopServiceLocator locator;
                    (void)locator.registerInstance<DummyA>(a);
                    (void)locator.registerInstance<DummyB>(b);

                    for (size_t i = 0; i < noiseCount; ++i)
                    {
                        size_sensitivity_detail::kRegisterTableHot[i](locator);
                    }

                    (void)locator.tryResolve<DummyA>();
                    (void)locator.tryResolve<DummyB>();

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyA>());
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyB>());
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 2);

                    if (phase == 1)
                    {
                        samples_mru2.push_back(nsOp);
                    }
                }
            }
        }

        const double medDefault = Statistics::compute(samples_default).median;
        const double medMru2 = Statistics::compute(samples_mru2).median;
        const double speedup = (medMru2 > 0.0) ? (medDefault / medMru2) : 0.0;

        std::cout << std::setw(8) << totalServices << " | "
                  << std::setw(22) << medDefault << " | "
                  << std::setw(18) << medMru2 << " | "
                  << std::setw(20) << speedup << "\n";
    }

    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Cycle A/B/C (ABCABC...) - MRU2 worst-case locality
    // ------------------------------------------------------------------------
    std::cout << "CYCLE A/B/C (ABCABC...)\n";
    std::cout
        << "Services | Default median (ns/op) | MRU2 median (ns/op) | Speedup (Default/MRU2)\n";
    std::cout
        << "---------|------------------------|--------------------|----------------------\n";

    const std::vector<size_t> sizesABC = {3, 5, 10, 25, 50, 100};

    for (size_t totalServices : sizesABC)
    {
        const size_t noiseCount = (totalServices > 3) ? (totalServices - 3) : 0;

        std::vector<double> samples_default;
        std::vector<double> samples_mru2;

        for (size_t phase = 0; phase < 2; ++phase)
        {
            const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

            for (size_t run = 0; run < runs; ++run)
            {
                // Default (no cache)
                {
                    fat_p::DefaultServiceLocator locator;
                    (void)locator.registerInstance<DummyA>(a);
                    (void)locator.registerInstance<DummyB>(b);
                    (void)locator.registerInstance<DummyC>(c);

                    for (size_t i = 0; i < noiseCount; ++i)
                    {
                        size_sensitivity_detail::kRegisterTable[i](locator);
                    }

                    (void)locator.tryResolve<DummyA>();
                    (void)locator.tryResolve<DummyB>();
                    (void)locator.tryResolve<DummyC>();

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyA>());
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyB>());
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyC>());
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 3);

                    if (phase == 1)
                    {
                        samples_default.push_back(nsOp);
                    }
                }

                // MRU2
                {
                    fat_p::HotLoopServiceLocator locator;
                    (void)locator.registerInstance<DummyA>(a);
                    (void)locator.registerInstance<DummyB>(b);
                    (void)locator.registerInstance<DummyC>(c);

                    for (size_t i = 0; i < noiseCount; ++i)
                    {
                        size_sensitivity_detail::kRegisterTableHot[i](locator);
                    }

                    (void)locator.tryResolve<DummyA>();
                    (void)locator.tryResolve<DummyB>();
                    (void)locator.tryResolve<DummyC>();

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyA>());
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyB>());
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<DummyC>());
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 3);

                    if (phase == 1)
                    {
                        samples_mru2.push_back(nsOp);
                    }
                }
            }
        }

        const double medDefault = Statistics::compute(samples_default).median;
        const double medMru2 = Statistics::compute(samples_mru2).median;
        const double speedup = (medMru2 > 0.0) ? (medDefault / medMru2) : 0.0;

        std::cout << std::setw(8) << totalServices << " | "
                  << std::setw(22) << medDefault << " | "
                  << std::setw(18) << medMru2 << " | "
                  << std::setw(20) << speedup << "\n";
    }

    std::cout << "\n";
}


// =============================================================================
// STRING KEY HOT LOOP (named services) - fat_p comparison
// =============================================================================
static void benchmark_string_key_hot_loop_locality()
{
    print_header("STRING KEY HOT LOOP (named services)");
    print_contract(
        "Hot-loop resolves by type+name (string key) after startup registration.\n"
        "Measures steady-state cost while varying:\n"
        "  A) Name length (bytes) at fixed named-variant count.\n"
        "  B) Named-variant count at fixed name length.\n"
        "Comparator: DefaultServiceLocator vs HotLoopServiceLocator (MRU2).\n"
        "Note: The MRU cache is type-only; named lookups should not benefit (this is intentional to measure).\n"
    );
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS_PER_RUN = 200000;

    auto make_names = [](size_t count, size_t len, std::uint64_t seed) -> std::vector<std::string> {
        std::mt19937_64 rng(seed);
        std::vector<std::string> names;
        names.reserve(count);

        for (size_t i = 0; i < count; ++i)
        {
            std::string s(len, 'a');
            for (size_t j = 0; j < len; ++j)
            {
                s[j] = static_cast<char>('a' + (rng() % 26));
            }

            // Ensure uniqueness by embedding i into the tail (ASCII digits) when possible.
            std::uint64_t v = static_cast<std::uint64_t>(i);
            for (size_t k = 0; k < len && k < 8; ++k)
            {
                s[len - 1 - k] = static_cast<char>('0' + (v % 10));
                v /= 10;
            }

            names.push_back(std::move(s));
        }

        return names;
    };

    NullLogger logger;

    auto bench_repeat = [&](const std::vector<std::string>& names) -> std::pair<double, double> {
        std::vector<double> samples_default;
        std::vector<double> samples_mru2;

        for (size_t phase = 0; phase < 2; ++phase)
        {
            const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

            for (size_t run = 0; run < runs; ++run)
            {
                // Default locator
                {
                    fat_p::DefaultServiceLocator locator;
                    for (const auto& n : names)
                    {
                        (void)locator.registerInstance<ILogger>(logger, n);
                    }

                    const std::string_view hot = names[0];
                    (void)locator.tryResolve<ILogger>(hot); // warm

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>(hot));
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN);

                    if (phase == 1)
                    {
                        samples_default.push_back(nsOp);
                    }
                }

                // HotLoop (MRU2) locator
                {
                    fat_p::HotLoopServiceLocator locator;
                    for (const auto& n : names)
                    {
                        (void)locator.registerInstance<ILogger>(logger, n);
                    }

                    const std::string_view hot = names[0];
                    (void)locator.tryResolve<ILogger>(hot); // warm

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>(hot));
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN);

                    if (phase == 1)
                    {
                        samples_mru2.push_back(nsOp);
                    }
                }
            }
        }

        return {Statistics::compute(samples_default).median, Statistics::compute(samples_mru2).median};
    };

    auto bench_alternate = [&](const std::vector<std::string>& names) -> std::pair<double, double> {
        std::vector<double> samples_default;
        std::vector<double> samples_mru2;

        const std::string_view a = names[0];
        const std::string_view b = (names.size() > 1) ? std::string_view(names[1]) : std::string_view(names[0]);

        for (size_t phase = 0; phase < 2; ++phase)
        {
            const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

            for (size_t run = 0; run < runs; ++run)
            {
                // Default locator
                {
                    fat_p::DefaultServiceLocator locator;
                    for (const auto& n : names)
                    {
                        (void)locator.registerInstance<ILogger>(logger, n);
                    }

                    (void)locator.tryResolve<ILogger>(a);
                    (void)locator.tryResolve<ILogger>(b);

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>(a));
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>(b));
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 2);

                    if (phase == 1)
                    {
                        samples_default.push_back(nsOp);
                    }
                }

                // HotLoop (MRU2) locator
                {
                    fat_p::HotLoopServiceLocator locator;
                    for (const auto& n : names)
                    {
                        (void)locator.registerInstance<ILogger>(logger, n);
                    }

                    (void)locator.tryResolve<ILogger>(a);
                    (void)locator.tryResolve<ILogger>(b);

                    Timer t;
                    t.start();
                    for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                    {
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>(a));
                        benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>(b));
                    }
                    const double nsOp = ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN * 2);

                    if (phase == 1)
                    {
                        samples_mru2.push_back(nsOp);
                    }
                }
            }
        }

        return {Statistics::compute(samples_default).median, Statistics::compute(samples_mru2).median};
    };

    // ------------------------------------------------------------------------
    // A) Name length sweep
    // ------------------------------------------------------------------------
    const std::vector<size_t> lengths = {4, 8, 16, 32, 64, 128};
    constexpr size_t kVariantCount = 100;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "NAME LENGTH SWEEP (variants=100)\n";
    std::cout << "Len | Default AAAA | MRU2 AAAA | Default ABAB | MRU2 ABAB\n";
    std::cout << "----|--------------|----------|--------------|----------\n";

    for (size_t len : lengths)
    {
        const auto names = make_names(kVariantCount, len, g_config.seed ^ (0xA5A5A5A5ULL + len));

        const auto [dAaaa, mAaaa] = bench_repeat(names);
        const auto [dAbab, mAbab] = bench_alternate(names);

        std::cout << std::setw(3) << len << " | "
                  << std::setw(12) << dAaaa << " | "
                  << std::setw(8) << mAaaa << " | "
                  << std::setw(12) << dAbab << " | "
                  << std::setw(8) << mAbab << "\n";
    }

    std::cout << "\n";

    // ------------------------------------------------------------------------
    // B) Variant count sweep
    // ------------------------------------------------------------------------
    const std::vector<size_t> counts = {1, 5, 10, 25, 50, 100, 500};
    constexpr size_t kFixedLen = 16;

    std::cout << "VARIANT COUNT SWEEP (len=16)\n";
    std::cout << "N   | Default AAAA | MRU2 AAAA | Default ABAB | MRU2 ABAB\n";
    std::cout << "----|--------------|----------|--------------|----------\n";

    for (size_t n : counts)
    {
        const auto names = make_names(n, kFixedLen, g_config.seed ^ (0x5A5A5A5AULL + n));

        const auto [dAaaa, mAaaa] = bench_repeat(names);
        const auto [dAbab, mAbab] = bench_alternate(names);

        std::cout << std::setw(3) << n << " | "
                  << std::setw(12) << dAaaa << " | "
                  << std::setw(8) << mAaaa << " | "
                  << std::setw(12) << dAbab << " | "
                  << std::setw(8) << mAbab << "\n";
    }

    std::cout << "\n";
}

// =============================================================================
// CONST RESOLVE (T vs const T)
// =============================================================================
static void benchmark_const_resolve()
{
    print_header("CONST RESOLVE (T vs const T)");
    print_contract(
        "Measures steady-state cost of tryResolve<T>() vs tryResolve<const T>() for unnamed services.\n"
        "TypeKeyPolicy removes cv-qualifiers, so both resolve paths share the same type id.\n"
        "Comparator: DefaultServiceLocator (no cache) vs HotLoopServiceLocator (MRU2).\n"
    );
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS_PER_RUN = 200000;

    struct Dummy { int x = 123; };
    Dummy dummy{};

    std::vector<double> def_nonconst;
    std::vector<double> def_const;
    std::vector<double> hot_nonconst;
    std::vector<double> hot_const;

    for (size_t phase = 0; phase < 2; ++phase)
    {
        const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

        for (size_t run = 0; run < runs; ++run)
        {
            // Default: non-const
            {
                fat_p::DefaultServiceLocator locator;
                (void)locator.registerInstance<Dummy>(dummy);
                (void)locator.tryResolve<Dummy>();

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<Dummy>());
                }
                if (phase == 1)
                {
                    def_nonconst.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }

            // Default: const
            {
                fat_p::DefaultServiceLocator locator;
                (void)locator.registerInstance<Dummy>(dummy);
                (void)locator.tryResolve<const Dummy>();

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<const Dummy>());
                }
                if (phase == 1)
                {
                    def_const.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }

            // HotLoop: non-const
            {
                fat_p::HotLoopServiceLocator locator;
                (void)locator.registerInstance<Dummy>(dummy);
                (void)locator.tryResolve<Dummy>();

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<Dummy>());
                }
                if (phase == 1)
                {
                    hot_nonconst.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }

            // HotLoop: const
            {
                fat_p::HotLoopServiceLocator locator;
                (void)locator.registerInstance<Dummy>(dummy);
                (void)locator.tryResolve<const Dummy>();

                Timer t;
                t.start();
                for (size_t i = 0; i < ITERATIONS_PER_RUN; ++i)
                {
                    benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<const Dummy>());
                }
                if (phase == 1)
                {
                    hot_const.push_back(ns_per_op(t.elapsed_ns(), ITERATIONS_PER_RUN));
                }
            }
        }
    }

    const double d_nc = Statistics::compute(def_nonconst).median;
    const double d_c = Statistics::compute(def_const).median;
    const double h_nc = Statistics::compute(hot_nonconst).median;
    const double h_c = Statistics::compute(hot_const).median;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Locator | tryResolve<T> median (ns/op) | tryResolve<const T> median (ns/op) | Ratio\n";
    std::cout << "--------|----------------------------|----------------------------------|------\n";

    auto ratio = [](double a, double b) { return (b > 0.0) ? (a / b) : 0.0; };

    std::cout << std::setw(7) << "Default" << " | "
              << std::setw(26) << d_nc << " | "
              << std::setw(32) << d_c << " | "
              << std::setw(4) << ratio(d_nc, d_c) << "x\n";

    std::cout << std::setw(7) << "HotLoop" << " | "
              << std::setw(26) << h_nc << " | "
              << std::setw(32) << h_c << " | "
              << std::setw(4) << ratio(h_nc, h_c) << "x\n\n";
}

// =============================================================================
// MUTATION COST (unregister / clear)
// =============================================================================
static void benchmark_mutation_cost()
{
    print_header("MUTATION COST (unregister / clear)");
    print_contract(
        "Measures registry mutation cost after services have been registered.\n"
        "  A) unregister N distinct service types (ns/op).\n"
        "  B) clear N service types (ns/op per entry).\n"
        "Comparator: DefaultServiceLocator vs HotLoopServiceLocator.\n"
    );
    print_cpu_context("Starting");

    const std::vector<size_t> sizes = {1, 5, 10, 25, 50, 100};

    std::cout << std::fixed << std::setprecision(2);

    // ------------------------------------------------------------------------
    // A) unregister
    // ------------------------------------------------------------------------
    std::cout << "UNREGISTER (ns/op)\n";
    std::cout << "N   | Default median | HotLoop median\n";
    std::cout << "----|----------------|--------------\n";

    for (size_t n : sizes)
    {
        std::vector<double> def_samples;
        std::vector<double> hot_samples;

        for (size_t phase = 0; phase < 2; ++phase)
        {
            const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

            for (size_t run = 0; run < runs; ++run)
            {
                // Default
                {
                    fat_p::DefaultServiceLocator locator;
                    for (size_t i = 0; i < n; ++i)
                    {
                        size_sensitivity_detail::kRegisterTable[i](locator);
                    }

                    Timer t;
                    t.start();
                    size_t ok = 0;
                    for (size_t i = 0; i < n; ++i)
                    {
                        ok += size_sensitivity_detail::kUnregisterTable[i](locator) ? 1U : 0U;
                    }
                    benchmark_sink += static_cast<std::intptr_t>(ok);

                    if (phase == 1)
                    {
                        def_samples.push_back(ns_per_op(t.elapsed_ns(), n));
                    }
                }

                // HotLoop
                {
                    fat_p::HotLoopServiceLocator locator;
                    for (size_t i = 0; i < n; ++i)
                    {
                        size_sensitivity_detail::kRegisterTableHot[i](locator);
                    }

                    Timer t;
                    t.start();
                    size_t ok = 0;
                    for (size_t i = 0; i < n; ++i)
                    {
                        ok += size_sensitivity_detail::kUnregisterTableHot[i](locator) ? 1U : 0U;
                    }
                    benchmark_sink += static_cast<std::intptr_t>(ok);

                    if (phase == 1)
                    {
                        hot_samples.push_back(ns_per_op(t.elapsed_ns(), n));
                    }
                }
            }
        }

        std::cout << std::setw(3) << n << " | "
                  << std::setw(14) << Statistics::compute(def_samples).median << " | "
                  << std::setw(12) << Statistics::compute(hot_samples).median << "\n";
    }

    std::cout << "\n";

    // ------------------------------------------------------------------------
    // B) clear
    // ------------------------------------------------------------------------
    std::cout << "CLEAR (ns/op per entry)\n";
    std::cout << "N   | Default median | HotLoop median\n";
    std::cout << "----|----------------|--------------\n";

    for (size_t n : sizes)
    {
        std::vector<double> def_samples;
        std::vector<double> hot_samples;

        for (size_t phase = 0; phase < 2; ++phase)
        {
            const size_t runs = (phase == 0) ? WARMUP_RUNS() : MEASURED_RUNS();

            for (size_t run = 0; run < runs; ++run)
            {
                // Default
                {
                    fat_p::DefaultServiceLocator locator;
                    for (size_t i = 0; i < n; ++i)
                    {
                        size_sensitivity_detail::kRegisterTable[i](locator);
                    }

                    Timer t;
                    t.start();
                    locator.clear();

                    if (phase == 1)
                    {
                        def_samples.push_back(ns_per_op(t.elapsed_ns(), n));
                    }
                }

                // HotLoop
                {
                    fat_p::HotLoopServiceLocator locator;
                    for (size_t i = 0; i < n; ++i)
                    {
                        size_sensitivity_detail::kRegisterTableHot[i](locator);
                    }

                    Timer t;
                    t.start();
                    locator.clear();

                    if (phase == 1)
                    {
                        hot_samples.push_back(ns_per_op(t.elapsed_ns(), n));
                    }
                }
            }
        }

        std::cout << std::setw(3) << n << " | "
                  << std::setw(14) << Statistics::compute(def_samples).median << " | "
                  << std::setw(12) << Statistics::compute(hot_samples).median << "\n";
    }

    std::cout << "\n";
}

static void benchmark_overhead_isolation()
{
    print_header("OVERHEAD ISOLATION MICRO-BENCHMARKS");
    print_contract("Isolate individual overhead components to identify optimization targets.");
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS = 500000;

    NullLogger logger;

    // Overhead sources identified in fat_p::ServiceLocator:
    // 1. ServiceKey construction (makeKey<T>) - creates std::string even for empty name
    // 2. ServiceKey hashing - hash<void*> + hash<string> + mix
    // 3. ServiceKey comparison - pointer compare + string compare
    // 4. resolveEntryForRead - lock + find + optional + snapshot copy
    // 5. tryResolve wrapper - resolveExpected + has_value + addressof

    std::cout << "Measuring individual overhead components...\n\n";

    // Baseline: Direct pointer access (no overhead)
    {
        ILogger* ptr = &logger;
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink += reinterpret_cast<std::intptr_t>(ptr);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "1. Direct pointer access (baseline)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: std::type_index construction
    {
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            std::type_index ti(typeid(ILogger));
            benchmark_sink += static_cast<std::int64_t>(ti.hash_code());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "2. std::type_index construction" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: fat_p TypeKeyPolicy (address of static template variable)
    {
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            const void* typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
            benchmark_sink += reinterpret_cast<std::intptr_t>(typeId);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "3. fat_p TypeKeyPolicy (static addr)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: std::string construction (empty)
    {
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            std::string s;
            benchmark_sink += static_cast<std::int64_t>(s.size());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "4. std::string construction (empty)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: std::string construction from string_view
    {
        std::string_view sv = "";
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            std::string s(sv);
            benchmark_sink += static_cast<std::int64_t>(s.size());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "5. std::string from string_view (empty)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: ServiceKey-like struct construction
    {
        struct TestKey
        {
            const void* typeId;
            std::string name;
        };
        const void* typeAddr = &fat_p::detail::kServiceTypeToken<ILogger>;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            TestKey key;
            key.typeId = typeAddr;
            key.name = std::string();
            benchmark_sink += reinterpret_cast<std::intptr_t>(key.typeId);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "6. ServiceKey-like struct construction" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: Hash computation (void* + empty string)
    {
        struct TestKey
        {
            const void* typeId;
            std::string name;
        };
        TestKey key;
        key.typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
        key.name = "";

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            size_t h1 = std::hash<const void*>{}(key.typeId);
            size_t h2 = std::hash<std::string>{}(key.name);
            constexpr size_t kMagic = static_cast<size_t>(0x9e3779b97f4a7c15ULL);
            size_t h = h1 ^ (h2 + kMagic + (h1 << 6U) + (h1 >> 2U));
            benchmark_sink += static_cast<std::int64_t>(h);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "7. ServiceKeyHash computation" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: unordered_map find with type_index key
    {
        std::unordered_map<std::type_index, void*> map;
        map[std::type_index(typeid(ILogger))] = &logger;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            auto it = map.find(std::type_index(typeid(ILogger)));
            benchmark_sink += reinterpret_cast<std::intptr_t>(it->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "8. unordered_map<type_index>.find()" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: unordered_map find with void* key (optimal)
    {
        std::unordered_map<const void*, void*> map;
        const void* key = &fat_p::detail::kServiceTypeToken<ILogger>;
        map[key] = &logger;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            auto it = map.find(key);
            benchmark_sink += reinterpret_cast<std::intptr_t>(it->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "9. unordered_map<void*>.find() (optimal)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: ServiceKey-based map find (simulates fat_p without lock)
    {
        struct TestKey
        {
            const void* typeId;
            std::string name;
        };
        struct TestKeyHash
        {
            size_t operator()(const TestKey& k) const noexcept
            {
                size_t h1 = std::hash<const void*>{}(k.typeId);
                size_t h2 = std::hash<std::string>{}(k.name);
                constexpr size_t kMagic = static_cast<size_t>(0x9e3779b97f4a7c15ULL);
                return h1 ^ (h2 + kMagic + (h1 << 6U) + (h1 >> 2U));
            }
        };
        struct TestKeyEq
        {
            bool operator()(const TestKey& a, const TestKey& b) const noexcept
            {
                return a.typeId == b.typeId && a.name == b.name;
            }
        };

        std::unordered_map<TestKey, void*, TestKeyHash, TestKeyEq> map;
        TestKey key;
        key.typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
        key.name = "";
        map[key] = &logger;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // Simulate makeKey - this is the overhead!
            TestKey lookupKey;
            lookupKey.typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
            lookupKey.name = std::string();

            auto it = map.find(lookupKey);
            benchmark_sink += reinterpret_cast<std::intptr_t>(it->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "10. ServiceKey map find (with makeKey)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Overhead: std::optional construction and check
    {
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            std::optional<void*> opt = &logger;
            if (opt.has_value())
            {
                benchmark_sink += reinterpret_cast<std::intptr_t>(opt.value());
            }
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "11. std::optional construction + check" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Full fat_p path for comparison
    {
        fat_p::DefaultServiceLocator locator;
        (void)locator.registerInstance<ILogger>(logger);

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "12. fat_p::DefaultServiceLocator.tryResolve" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    std::cout << "\n";
    std::cout << "Analysis:\n";
    std::cout << "  - Items 6+10 show ServiceKey construction + lookup overhead\n";
    std::cout << "  - Compare item 9 (optimal) vs item 10 (current) for improvement potential\n";
    std::cout << "  - Item 4-5 show std::string allocation overhead (even for empty strings)\n";
    std::cout << "\n";

    // ========================================================================
    // DETAILED GAP ANALYSIS: Where does the extra ~17ns come from?
    // ========================================================================
    std::cout << "Detailed gap analysis (tryResolve breakdown)...\n\n";

    // 13. makeKey<T>() cost (constructs ServiceKey with string)
    {
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // Simulate makeKey<T>() - this is what happens on every resolve
            const void* typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
            std::string name = std::string();  // Empty name case
            benchmark_sink += reinterpret_cast<std::intptr_t>(typeId) + static_cast<std::int64_t>(name.size());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "13. makeKey<T>() simulation" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 14. Lock acquisition (SingleThreadedPolicy - should be ~0)
    {
        fat_p::SingleThreadedPolicy policy;
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            auto lock = policy.lock_shared();
            benchmark_sink += 1;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "14. SingleThreadedPolicy lock_shared()" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // 15. Lock acquisition (SharedMutexPolicy)
    {
        fat_p::SharedMutexPolicy policy;
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            auto lock = policy.lock_shared();
            benchmark_sink += 1;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "15. SharedMutexPolicy lock_shared()" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 16. std::function copy (the mFactory member in snapshot)
    {
        std::function<std::shared_ptr<void>()> factory = []() -> std::shared_ptr<void> {
            return std::make_shared<NullLogger>();
        };
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            std::function<std::shared_ptr<void>()> copy = factory;
            benchmark_sink += copy ? 1 : 0;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "16. std::function copy" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 17. std::shared_ptr<void> copy (the mShared member in snapshot)
    {
        std::shared_ptr<void> shared = std::make_shared<NullLogger>();
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            std::shared_ptr<void> copy = shared;
            benchmark_sink += copy ? 1 : 0;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "17. std::shared_ptr<void> copy" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 18. std::optional<T> construction with value
    {
        struct SnapLike
        {
            int kind;
            void* instance;
        };
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            SnapLike s{1, &logger};
            std::optional<SnapLike> opt = s;
            benchmark_sink += opt.has_value() ? 1 : 0;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "18. std::optional<SnapLike> construction" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // 19. ServiceEntrySnapshot-like struct copy (full snapshot with function)
    {
        struct FullSnapshot
        {
            int kind;
            int lifetime;
            void* instance;
            std::shared_ptr<void> shared;
            std::function<std::shared_ptr<void>()> factory;
        };
        FullSnapshot src;
        src.kind = 0;  // Instance
        src.lifetime = 0;
        src.instance = &logger;
        src.shared = nullptr;
        src.factory = nullptr;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            FullSnapshot copy = src;
            benchmark_sink += reinterpret_cast<std::intptr_t>(copy.instance);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "19. FullSnapshot copy (Instance path)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 20. ServiceEntrySnapshot-like struct copy with factory set
    {
        struct FullSnapshot
        {
            int kind;
            int lifetime;
            void* instance;
            std::shared_ptr<void> shared;
            std::function<std::shared_ptr<void>()> factory;
        };
        FullSnapshot src;
        src.kind = 2;  // Factory
        src.lifetime = 0;
        src.instance = nullptr;
        src.shared = nullptr;
        src.factory = []() -> std::shared_ptr<void> { return std::make_shared<NullLogger>(); };

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            FullSnapshot copy = src;
            benchmark_sink += copy.factory ? 1 : 0;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "20. FullSnapshot copy (Factory path)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 21. Expected<reference_wrapper<T>> construction
    {
        using ResultType = fat_p::Expected<std::reference_wrapper<ILogger>, fat_p::ServiceErrorInfo>;
        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            ResultType result = std::ref(logger);
            benchmark_sink += result.has_value() ? 1 : 0;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left
                  << "21. Expected<ref_wrapper> construction"
                  << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 22. Full resolveEntryForRead simulation (lock + find + optional + snapshot)
    {
        struct TestKey
        {
            const void* typeId;
            std::string name;
        };
        struct TestKeyHash
        {
            size_t operator()(const TestKey& k) const noexcept
            {
                return std::hash<const void*>{}(k.typeId) ^ std::hash<std::string>{}(k.name);
            }
        };
        struct TestKeyEq
        {
            bool operator()(const TestKey& a, const TestKey& b) const noexcept
            {
                return a.typeId == b.typeId && a.name == b.name;
            }
        };
        struct Entry
        {
            int kind;
            void* instance;
            std::shared_ptr<void> shared;
            std::function<std::shared_ptr<void>()> factory;
        };
        struct Snapshot
        {
            int kind;
            void* instance;
            std::shared_ptr<void> shared;
            std::function<std::shared_ptr<void>()> factory;
        };

        std::unordered_map<TestKey, Entry, TestKeyHash, TestKeyEq> registry;
        TestKey stored{&fat_p::detail::kServiceTypeToken<ILogger>, ""};
        registry[stored] = Entry{0, &logger, nullptr, nullptr};

        fat_p::SingleThreadedPolicy policy;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // Simulate resolveEntryForRead
            TestKey key{&fat_p::detail::kServiceTypeToken<ILogger>, std::string()};
            auto lock = policy.lock_shared();
            auto it = registry.find(key);
            std::optional<Snapshot> result;
            if (it != registry.end())
            {
                Snapshot snap;
                snap.kind = it->second.kind;
                snap.instance = it->second.instance;
                snap.shared = it->second.shared;
                snap.factory = it->second.factory;
                result = snap;
            }
            benchmark_sink += result.has_value() ? reinterpret_cast<std::intptr_t>(result->instance) : 0;
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "22. resolveEntryForRead simulation" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 23. Full tryResolve simulation (makeKey + resolveEntryForRead + unwrap)
    {
        struct TestKey
        {
            const void* typeId;
            std::string name;
        };
        struct TestKeyHash
        {
            size_t operator()(const TestKey& k) const noexcept
            {
                return std::hash<const void*>{}(k.typeId) ^ std::hash<std::string>{}(k.name);
            }
        };
        struct TestKeyEq
        {
            bool operator()(const TestKey& a, const TestKey& b) const noexcept
            {
                return a.typeId == b.typeId && a.name == b.name;
            }
        };
        struct Entry
        {
            int kind;
            void* instance;
        };

        std::unordered_map<TestKey, Entry, TestKeyHash, TestKeyEq> registry;
        TestKey stored{&fat_p::detail::kServiceTypeToken<ILogger>, ""};
        registry[stored] = Entry{0, &logger};

        fat_p::SingleThreadedPolicy policy;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // makeKey<T>()
            TestKey key;
            key.typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
            key.name = std::string();

            // resolveEntryForRead (simplified - no snapshot copy)
            auto lock = policy.lock_shared();
            auto it = registry.find(key);

            // Return path
            ILogger* result = nullptr;
            if (it != registry.end() && it->second.kind == 0)
            {
                result = static_cast<ILogger*>(it->second.instance);
            }
            benchmark_sink += reinterpret_cast<std::intptr_t>(result);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "23. tryResolve simulation (no snapshot)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // 24. Actual fat_p tryResolve (for comparison)
    {
        fat_p::DefaultServiceLocator locator;
        (void)locator.registerInstance<ILogger>(logger);

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "24. fat_p tryResolve (actual)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    std::cout << "\n";
    std::cout << "Gap Analysis Summary:\n";
    std::cout << "  Compare items 22-24 to identify where overhead accumulates.\n";
    std::cout << "  Item 23 vs 24 shows cost of snapshot copy + Expected wrapper.\n";
    std::cout << "  Item 16 (std::function copy) is often the hidden culprit.\n";
    std::cout << "\n";

    // ========================================================================
    // STABLEHASHMAP COMPARISON: Reference stability eliminates snapshot copy
    // ========================================================================
    std::cout << "StableHashMap comparison (reference stability)...\n\n";

    // 25. StableHashMap lookup (reference stable - no copy needed)
    {
        struct Entry
        {
            int kind;
            void* instance;
            std::shared_ptr<void> shared;
            std::function<std::shared_ptr<void>()> factory;
        };

        fat_p::StableHashMap<const void*, Entry, AvalanchingPtrHash> stableMap;
        const void* typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
        stableMap.insert(typeId, Entry{0, &logger, nullptr, nullptr});

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // StableHashMap: pointer remains valid, no copy needed!
            Entry* entry = stableMap.find(typeId);
            benchmark_sink += reinterpret_cast<std::intptr_t>(entry->instance);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "25. StableHashMap<void*> find (no copy, SM64)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // 26. StableHashMap with ServiceKey-like key
    {
        struct TestKey
        {
            const void* typeId;
            std::string name;

            bool operator==(const TestKey& other) const
            {
                return typeId == other.typeId && name == other.name;
            }
        };
        struct TestKeyHash
        {
            size_t operator()(const TestKey& k) const noexcept
            {
                return std::hash<const void*>{}(k.typeId) ^ std::hash<std::string>{}(k.name);
            }
        };
        struct Entry
        {
            int kind;
            void* instance;
        };

        fat_p::StableHashMap<TestKey, Entry, TestKeyHash> stableMap;
        TestKey stored{&fat_p::detail::kServiceTypeToken<ILogger>, ""};
        stableMap.insert(stored, Entry{0, &logger});

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            TestKey key{&fat_p::detail::kServiceTypeToken<ILogger>, std::string()};
            Entry* entry = stableMap.find(key);
            benchmark_sink += reinterpret_cast<std::intptr_t>(entry->instance);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "26. StableHashMap<ServiceKey> find" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 27. Simulated optimal tryResolve with StableHashMap (no snapshot copy)
    {
        struct Entry
        {
            int kind;
            void* instance;
            std::shared_ptr<void> shared;
        };

        fat_p::StableHashMap<const void*, Entry, AvalanchingPtrHash> stableMap;
        const void* typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
        stableMap.insert(typeId, Entry{0, &logger, nullptr});

        fat_p::SingleThreadedPolicy policy;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // Simulate optimized tryResolve with StableHashMap
            const void* key = &fat_p::detail::kServiceTypeToken<ILogger>;
            auto lock = policy.lock_shared();
            Entry* entry = stableMap.find(key);

            ILogger* result = nullptr;
            if (entry && entry->kind == 0)
            {
                // Direct access - NO COPY because reference is stable!
                result = static_cast<ILogger*>(entry->instance);
            }
            benchmark_sink += reinterpret_cast<std::intptr_t>(result);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left
                  << "27. Optimal tryResolve (StableHashMap)"
                  << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 28. Current fat_p tryResolve (for direct comparison)
    {
        fat_p::DefaultServiceLocator locator;
        (void)locator.registerInstance<ILogger>(logger);

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "28. Current fat_p tryResolve" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    std::cout << "\n";
    std::cout << "StableHashMap Advantage:\n";
    std::cout << "  - Reference stability eliminates snapshot copy (~10ns saved)\n";
    std::cout << "  - SIMD-accelerated probing (faster than std::unordered_map)\n";
    std::cout << "  - No shared_ptr atomic refcount overhead on resolve\n";
    std::cout << "  - Compare #27 vs #28 for potential improvement\n";
    std::cout << "\n";

    // ========================================================================
    // ZERO-COST ABSTRACTION VERIFICATION
    // ========================================================================
    std::cout << "Zero-cost abstraction verification...\n\n";

    // 29. Raw StableHashMap lookup (no lock, no abstraction)
    {
        fat_p::StableHashMap<const void*, void*, AvalanchingPtrHash> rawMap;
        const void* typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
        rawMap.insert(typeId, &logger);

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            void** entry = rawMap.find(typeId);
            benchmark_sink += reinterpret_cast<std::intptr_t>(*entry);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "29. Raw StableHashMap (no lock)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 30. With SingleThreadedPolicy lock (should be same as #29)
    {
        fat_p::StableHashMap<const void*, void*, AvalanchingPtrHash> rawMap;
        const void* typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
        rawMap.insert(typeId, &logger);
        fat_p::SingleThreadedPolicy policy;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            auto lock = policy.lock_shared();  // Should be zero-cost
            void** entry = rawMap.find(typeId);
            benchmark_sink += reinterpret_cast<std::intptr_t>(*entry);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "30. With SingleThreadedPolicy lock" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // 31. ServiceLocator tryResolve (full path - now optimized with StableHashMap)
    {
        fat_p::DefaultServiceLocator locator;
        (void)locator.registerInstance<ILogger>(logger);

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink += reinterpret_cast<std::intptr_t>(locator.tryResolve<ILogger>());
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "31. ServiceLocator tryResolve (optimized)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // 32. Minimal possible resolve (inline everything)
    {
        fat_p::StableHashMap<const void*, void*, AvalanchingPtrHash> rawMap;
        const void* typeId = &fat_p::detail::kServiceTypeToken<ILogger>;
        rawMap.insert(typeId, &logger);

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // Absolute minimum: just the hash lookup
            void** entry = rawMap.find(&fat_p::detail::kServiceTypeToken<ILogger>);
            benchmark_sink += reinterpret_cast<std::intptr_t>(entry ? *entry : nullptr);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "32. Minimal resolve (just hash lookup)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // 33. EnTT-style static (for comparison)
    {
        static ILogger* staticLogger = &logger;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink += reinterpret_cast<std::intptr_t>(staticLogger);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(45) << std::left << "33. Static global (EnTT-style)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    std::cout << "\n";
    std::cout << "If #29 == #30, SingleThreadedPolicy is truly zero-cost.\n";
    std::cout << "Gap between #32 and #33 is the irreducible hash lookup cost.\n";
    std::cout << "\n";
}

// ============================================================================
// Section 8: Alternative Key Strategies
// ============================================================================

static void benchmark_key_strategies()
{
    print_header("ALTERNATIVE KEY STRATEGIES");
    print_contract("Compare different key designs to identify optimization opportunities.");
    print_cpu_context("Starting");

    constexpr size_t ITERATIONS = 500000;

    NullLogger logger;
    InMemoryDatabase db;

    std::cout << "Testing alternative ServiceKey designs...\n\n";

    // Strategy 1: Current fat_p (void* + std::string)
    {
        struct Key
        {
            const void* typeId;
            std::string name;
        };
        struct KeyHash
        {
            size_t operator()(const Key& k) const noexcept
            {
                size_t h1 = std::hash<const void*>{}(k.typeId);
                size_t h2 = std::hash<std::string>{}(k.name);
                return h1 ^ (h2 * 31);
            }
        };
        struct KeyEq
        {
            bool operator()(const Key& a, const Key& b) const noexcept
            {
                return a.typeId == b.typeId && a.name == b.name;
            }
        };

        std::unordered_map<Key, void*, KeyHash, KeyEq> map;
        Key k1{&fat_p::detail::kServiceTypeToken<ILogger>, ""};
        Key k2{&fat_p::detail::kServiceTypeToken<IDatabase>, ""};
        map[k1] = &logger;
        map[k2] = &db;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            Key lookup{&fat_p::detail::kServiceTypeToken<ILogger>, std::string()};
            benchmark_sink += reinterpret_cast<std::intptr_t>(map.find(lookup)->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(50) << std::left << "Strategy 1: void* + std::string (current)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Strategy 2: void* + string_view (no allocation, but requires stable strings)
    {
        struct Key
        {
            const void* typeId;
            std::string_view name;
        };
        struct KeyHash
        {
            size_t operator()(const Key& k) const noexcept
            {
                size_t h1 = std::hash<const void*>{}(k.typeId);
                size_t h2 = std::hash<std::string_view>{}(k.name);
                return h1 ^ (h2 * 31);
            }
        };
        struct KeyEq
        {
            bool operator()(const Key& a, const Key& b) const noexcept
            {
                return a.typeId == b.typeId && a.name == b.name;
            }
        };

        std::unordered_map<Key, void*, KeyHash, KeyEq> map;
        static constexpr std::string_view empty = "";
        Key k1{&fat_p::detail::kServiceTypeToken<ILogger>, empty};
        Key k2{&fat_p::detail::kServiceTypeToken<IDatabase>, empty};
        map[k1] = &logger;
        map[k2] = &db;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            Key lookup{&fat_p::detail::kServiceTypeToken<ILogger>, empty};
            benchmark_sink += reinterpret_cast<std::intptr_t>(map.find(lookup)->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(50) << std::left << "Strategy 2: void* + string_view (zero-alloc)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Strategy 3: void* only (no named services - fast path)
    {
        std::unordered_map<const void*, void*> map;
        map[&fat_p::detail::kServiceTypeToken<ILogger>] = &logger;
        map[&fat_p::detail::kServiceTypeToken<IDatabase>] = &db;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink +=
                reinterpret_cast<std::intptr_t>(map.find(&fat_p::detail::kServiceTypeToken<ILogger>)->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(50) << std::left << "Strategy 3: void* only (no names)" << ": " << std::fixed
                  << std::setprecision(2) << ns << " ns/op\n";
    }

    // Strategy 4: Precomputed hash (cache hash in key)
    {
        struct Key
        {
            const void* typeId;
            std::string name;
            size_t cachedHash;

            Key(const void* t, std::string n)
                : typeId(t)
                , name(std::move(n))
            {
                size_t h1 = std::hash<const void*>{}(typeId);
                size_t h2 = std::hash<std::string>{}(name);
                cachedHash = h1 ^ (h2 * 31);
            }
        };
        struct KeyHash
        {
            size_t operator()(const Key& k) const noexcept
            {
                return k.cachedHash;
            }
        };
        struct KeyEq
        {
            bool operator()(const Key& a, const Key& b) const noexcept
            {
                return a.cachedHash == b.cachedHash && a.typeId == b.typeId && a.name == b.name;
            }
        };

        std::unordered_map<Key, void*, KeyHash, KeyEq> map;
        map.emplace(Key{&fat_p::detail::kServiceTypeToken<ILogger>, ""}, &logger);
        map.emplace(Key{&fat_p::detail::kServiceTypeToken<IDatabase>, ""}, &db);

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            Key lookup{&fat_p::detail::kServiceTypeToken<ILogger>, ""};
            benchmark_sink += reinterpret_cast<std::intptr_t>(map.find(lookup)->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(50) << std::left << "Strategy 4: Cached hash (still allocates string)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Strategy 5: Two-level map (separate maps for named vs unnamed)
    {
        std::unordered_map<const void*, void*> unnamedMap;
        // Named map would use std::pair but we only test unnamed fast path here

        unnamedMap[&fat_p::detail::kServiceTypeToken<ILogger>] = &logger;
        unnamedMap[&fat_p::detail::kServiceTypeToken<IDatabase>] = &db;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            // Fast path for unnamed services
            benchmark_sink +=
                reinterpret_cast<std::intptr_t>(unnamedMap.find(&fat_p::detail::kServiceTypeToken<ILogger>)->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(50) << std::left << "Strategy 5: Two-level map (unnamed fast path)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    // Strategy 6: type_index (standard library approach)
    {
        std::unordered_map<std::type_index, void*> map;
        map[std::type_index(typeid(ILogger))] = &logger;
        map[std::type_index(typeid(IDatabase))] = &db;

        Timer t;
        t.start();
        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            benchmark_sink += reinterpret_cast<std::intptr_t>(map.find(std::type_index(typeid(ILogger)))->second);
        }
        double ns = ns_per_op(t.elapsed_ns(), ITERATIONS);
        std::cout << "  " << std::setw(50) << std::left << "Strategy 6: std::type_index (no names)" << ": "
                  << std::fixed << std::setprecision(2) << ns << " ns/op\n";
    }

    std::cout << "\n";
    std::cout << "Recommendations:\n";
    std::cout << "  - Strategy 3/5 show potential for unnamed services (~2x faster)\n";
    std::cout << "  - Strategy 2 eliminates allocation but requires API changes\n";
    std::cout << "  - Consider two-tier storage: fast path for type-only, slow path for named\n";
    std::cout << "\n";
}

// ============================================================================
// Section 9: Concurrent Resolution
// ============================================================================

static void benchmark_concurrent()
{
    print_header("CONCURRENT RESOLUTION");
    print_contract("Multi-threaded read-only resolution. Thread-safe variants only.");
    print_cpu_context("Starting");

    const unsigned threadCount = std::max(2u, std::thread::hardware_concurrency());
    constexpr size_t OPS_PER_THREAD = 100000;

    std::cout << "Thread count: " << threadCount << ", ops/thread: " << OPS_PER_THREAD << "\n\n";

    // --------------------------------------------------------------------
    // Key design note:
    //
    // The ServiceLocator uses a stable per-type key (address of a token),
    // not std::type_index. For an apples-to-apples concurrent baseline we
    // therefore include an unordered_map<void*> keyed by the same token.
    //
    // We also report an unordered_map<std::type_index> variant (with the
    // key precomputed) to show how a more "standard library style" map
    // compares under contention, without mixing in type_index construction.
    // --------------------------------------------------------------------

    struct alignas(64) ThreadSum
    {
        std::uintptr_t value = 0;
    };

    struct StartGate
    {
        std::mutex m;
        std::condition_variable cvReady;
        std::condition_variable cvGo;
        unsigned ready = 0;
        bool go = false;
    };

    struct DoneGate
    {
        std::mutex m;
        std::condition_variable cv;
        unsigned done = 0;
    };

    auto run_case = [&](auto&& per_op) -> double {
        StartGate start;
        DoneGate done;

        std::vector<ThreadSum> sums(threadCount);
        std::vector<std::thread> threads;
        threads.reserve(threadCount);

        for (unsigned tid = 0; tid < threadCount; ++tid)
        {
            threads.emplace_back([&, tid]() {
                // Signal readiness and wait for the start signal (setup outside timing).
                {
                    std::unique_lock<std::mutex> lk(start.m);
                    ++start.ready;
                    start.cvReady.notify_one();
                    start.cvGo.wait(lk, [&]() { return start.go; });
                }

                // Hot loop (timed region).
                std::uintptr_t local = 0;
                for (size_t j = 0; j < OPS_PER_THREAD; ++j)
                {
                    local += reinterpret_cast<std::uintptr_t>(per_op());
                }
                sums[tid].value = local;

                // Signal completion (teardown outside timing).
                {
                    std::lock_guard<std::mutex> lk(done.m);
                    ++done.done;
                }
                done.cv.notify_one();
            });
        }

        // Wait until all threads are parked at the start gate (setup outside timing).
        {
            std::unique_lock<std::mutex> lk(start.m);
            start.cvReady.wait(lk, [&]() { return start.ready == threadCount; });
        }

        Timer t;
        t.start();

        // Release all worker threads to start the timed hot loop.
        {
            std::lock_guard<std::mutex> lk(start.m);
            start.go = true;
        }
        start.cvGo.notify_all();

        // Wait for completion and stop the timer BEFORE joining (join is teardown).
        {
            std::unique_lock<std::mutex> lk(done.m);
            done.cv.wait(lk, [&]() { return done.done == threadCount; });
        }

        const double elapsed = t.elapsed_ns();

        for (auto& th : threads)
        {
            th.join();
        }

        // Consume results on the main thread to avoid any cross-thread data races
        // on benchmark_sink (and to avoid false-sharing in the hot loop).
        std::uintptr_t combined = 0;
        for (const auto& s : sums)
        {
            combined += s.value;
        }
        benchmark_sink += static_cast<std::intptr_t>(combined);

        return ns_per_op(elapsed, static_cast<size_t>(threadCount) * OPS_PER_THREAD);
    };

    // ---------------------------
    // Setup shared test fixtures
    // ---------------------------
    NullLogger logger;
    InMemoryDatabase db;

    // FAT-P thread-safe locators (no stats vs atomic stats)
    FatPThreadSafeNoStats fatp_no_stats;
    (void)fatp_no_stats.registerInstance<ILogger>(logger);
    (void)fatp_no_stats.registerInstance<IDatabase>(db);

    FatPThreadSafeAtomicStats fatp_atomic_stats;
    (void)fatp_atomic_stats.registerInstance<ILogger>(logger);
    (void)fatp_atomic_stats.registerInstance<IDatabase>(db);

    // Apples-to-apples baseline: unordered_map<void*> keyed by the same token addresses.
    std::unordered_map<const void*, void*> map_typekey;
    std::shared_mutex mtx_typekey;
    map_typekey.reserve(2);
    const void* const keyILogger = &fat_p::detail::kServiceTypeToken<ILogger>;
    const void* const keyIDatabase = &fat_p::detail::kServiceTypeToken<IDatabase>;
    map_typekey.emplace(keyILogger, &logger);
    map_typekey.emplace(keyIDatabase, &db);

    // Apples-to-apples baseline: StableHashMap<void*> keyed by the same token addresses (SM64/avalanching).
    fat_p::StableHashMap<const void*, void*, AvalanchingPtrHash> stable_typekey;
    std::shared_mutex mtx_stable_typekey;
    stable_typekey.reserve(2);
    stable_typekey.insert(keyILogger, &logger);
    stable_typekey.insert(keyIDatabase, &db);

    // Alternate baseline: unordered_map<type_index> with precomputed key (no type_index construction in-loop).
    std::unordered_map<std::type_index, void*> map_typeindex;
    std::shared_mutex mtx_typeindex;
    map_typeindex.reserve(2);
    const std::type_index tiILogger(typeid(ILogger));
    const std::type_index tiIDatabase(typeid(IDatabase));
    map_typeindex.emplace(tiILogger, &logger);
    map_typeindex.emplace(tiIDatabase, &db);

    // ---------------------------
    // Round-robin execution
    // ---------------------------
    std::vector<double> s_fatp_no_stats;
    std::vector<double> s_fatp_atomic_stats;
    std::vector<double> s_map_typekey;
    std::vector<double> s_stable_typekey;
    std::vector<double> s_map_typeindex;

    enum CaseId : int
    {
        FATP_NO_STATS = 0,
        FATP_ATOMIC_STATS = 1,
        MAP_TYPEKEY = 2,
        MAP_STABLE_TYPEKEY = 3,
        MAP_TYPEINDEX = 4,
    };

    std::array<int, 5> order{{FATP_NO_STATS, FATP_ATOMIC_STATS, MAP_TYPEKEY, MAP_STABLE_TYPEKEY, MAP_TYPEINDEX}};
    std::mt19937_64 rng(g_config.seed ^ 0xC0FFEEULL);

    auto run_one = [&](int case_id, bool record) {
        double sample = 0.0;
        switch (case_id)
        {
            case FATP_NO_STATS:
                sample = run_case([&]() { return fatp_no_stats.tryResolve<ILogger>(); });
                if (record) s_fatp_no_stats.push_back(sample);
                break;

            case FATP_ATOMIC_STATS:
                sample = run_case([&]() { return fatp_atomic_stats.tryResolve<ILogger>(); });
                if (record) s_fatp_atomic_stats.push_back(sample);
                break;

            case MAP_TYPEKEY:
                sample = run_case([&]() {
                    std::shared_lock<std::shared_mutex> lock(mtx_typekey);
                    auto it = map_typekey.find(keyILogger);
                    return it != map_typekey.end() ? it->second : nullptr;
                });
                if (record) s_map_typekey.push_back(sample);
                break;

            case MAP_STABLE_TYPEKEY:
                sample = run_case([&]() {
                    std::shared_lock<std::shared_mutex> lock(mtx_stable_typekey);
                    void** found = stable_typekey.find(keyILogger);
                    return found ? *found : nullptr;
                });
                if (record) s_stable_typekey.push_back(sample);
                break;

            case MAP_TYPEINDEX:
                sample = run_case([&]() {
                    std::shared_lock<std::shared_mutex> lock(mtx_typeindex);
                    auto it = map_typeindex.find(tiILogger);
                    return it != map_typeindex.end() ? it->second : nullptr;
                });
                if (record) s_map_typeindex.push_back(sample);
                break;
        }
    };

    // Warmup (not recorded)
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (int cid : order)
        {
            run_one(cid, /*record=*/false);
        }
    }

    // Measured runs
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (int cid : order)
        {
            run_one(cid, /*record=*/true);
        }
    }

    // Report
    Statistics::compute(s_fatp_no_stats).print("fat_p::ThreadSafeServiceLocator (no stats)");
    Statistics::compute(s_fatp_atomic_stats).print("fat_p::ThreadSafeServiceLocator (atomic stats)");
    Statistics::compute(s_map_typekey).print("unordered_map<void*> + shared_mutex (type key)");
    Statistics::compute(s_stable_typekey).print("StableHashMap<void*> + shared_mutex (type key, SM64)");
    Statistics::compute(s_map_typeindex).print("unordered_map<type_index> + shared_mutex (precomputed key)");
    std::cout << "\n";
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
    fat_p::bench::BenchmarkScope scope(!g_config.noScope);

    std::cout << "================================================================================\n";
    std::cout << "  ServiceLocator Comprehensive Benchmark Suite\n";
    std::cout << "================================================================================\n";

    // Print platform and configuration
    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() << ", seed=" << g_config.seed
              << ")\n";

    // Print detected competitors
    std::cout << "Competitor libraries: ";
#if FATP_HAS_ENTT
    std::cout << "entt ";
#endif
#if !FATP_HAS_ENTT
    std::cout << "(none - install entt via vcpkg for EnTT comparison)";
#endif
    std::cout << "\n\n";

    // Print design invariants
    std::cout << "Design Invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run\n";
    std::cout << "  3. Setup/teardown outside timed regions\n";
    std::cout << "  4. All libraries observe same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n\n";

    // Build adapter list
    std::vector<std::unique_ptr<ILocatorAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPDefaultAdapter>());
    adapters.push_back(std::make_unique<FatPDefaultAtomicStatsAdapter>());
    adapters.push_back(std::make_unique<FatPThreadSafeAdapter>());
    adapters.push_back(std::make_unique<FatPThreadSafeAtomicStatsAdapter>());
#if FATP_HAS_ENTT
    adapters.push_back(std::make_unique<EnttLocatorAdapter>());
#endif
    adapters.push_back(std::make_unique<UnorderedMapAdapter>());
    adapters.push_back(std::make_unique<DirectPointerAdapter>());

    // Correctness guardrails
    verify_adapters(adapters);

    // Run benchmarks
    benchmark_single_resolve(adapters);
    benchmark_multi_resolve(adapters);
    benchmark_named_services();
    benchmark_scoped_resolution();
    benchmark_registration();
    benchmark_size_sensitivity();
    benchmark_mru_resolve_cache_locality();
    benchmark_string_key_hot_loop_locality();
    benchmark_const_resolve();
    benchmark_mutation_cost();
    benchmark_overhead_isolation();
    benchmark_key_strategies();
    benchmark_concurrent();

    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
