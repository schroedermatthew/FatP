/*
FATP_META:
  meta_version: 1
  component: StringPool
  file_role: benchmark
  path: components/StringPool/benchmarks/benchmark_StringPool.cpp
  layer: Testing
  namespace: fat_p
  summary: "Benchmarks for StringPool."
  api_stability: candidate
  related:
    docs_search: "StringPool"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/StringPool.h
    tests:
      - components/StringPool/tests/test_StringPool.cpp
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 2
    defines_unprefixed: 2
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: Claude
    mode: manual
*/

// benchmark_StringPool.cpp
//
// FAT-P StringPool benchmarks using unified FatPBenchmarkRunner infrastructure.
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
// Sections:
//   1. Intern throughput - unique strings (cold insert, 100% miss)
//   2. Intern throughput - high duplication (90% hit rate)
//   3. Lookup throughput (find hit/miss)
//   4. Pointer comparison vs string comparison
//   5. String length impact
//   6. Memory efficiency
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_StringPool.cpp -o bench_stringpool
//   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_StringPool.cpp /link advapi32.lib
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
//   ./bench_stringpool
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_stringpool

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FatPBenchmarkHeader.h"
#include "FatPBenchmarkRunner.h"
#include "StringPool.h"

#pragma warning(push, 0)

// boost::flyweight - direct competitor for string interning
#if __has_include(<boost/flyweight.hpp>)
#include <boost/flyweight.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/hashed_factory.hpp>
#define HAS_BOOST_FLYWEIGHT 1
#else
#define HAS_BOOST_FLYWEIGHT 0
#endif

#pragma warning(pop)

// ============================================================================
// Global Configuration
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
// Benchmark Environment
// ============================================================================

using fat_p::bench::BenchmarkScope;

// ============================================================================
// CPU Frequency Monitoring
// ============================================================================

void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

// ============================================================================
// Timer
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

// Prevent dead code elimination
static volatile int64_t benchmark_sink = 0;

// ============================================================================
// Cooling Delays
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
static constexpr int COOLING_DELAY_SECTION_MS = 2000;
static constexpr int COOLING_DELAY_SIZE_MS = 1000;
static constexpr int COOLING_DELAY_CASE_MS = 300;
#else
static constexpr int COOLING_DELAY_SECTION_MS = 1000;
static constexpr int COOLING_DELAY_SIZE_MS = 500;
static constexpr int COOLING_DELAY_CASE_MS = 200;
#endif

static inline void cooling_delay(int ms, const char* reason = nullptr)
{
    if (g_config.noCooldown)
    {
        return;
    }

    if (reason)
    {
        std::cout << "[Cooling: " << reason << "]" << std::flush;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ms));

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

            double se = s.stddev / std::sqrt(static_cast<double>(n));
            constexpr double z = 1.96;
            s.ci95_low = s.mean - z * se;
            s.ci95_high = s.mean + z * se;
        }

        return s;
    }
};

// ============================================================================
// Result Storage + Output
// ============================================================================

struct BenchResult
{
    std::string library;
    std::vector<double> samples;
};

static void print_results(const std::vector<BenchResult>& results)
{
    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : results)
    {
        auto s = Statistics::compute(r.samples);
        std::cout << "    " << std::setw(28) << r.library << ": "
                  << std::setw(8) << s.median << " ns/op "
                  << "(+/-" << std::setw(6) << s.stddev
                  << ", CI:[" << s.ci95_low << "," << s.ci95_high << "])\n";
    }
}

static void print_speedup(const char* label, double baseline_median, double test_median)
{
    if (test_median <= 0.0 || baseline_median <= 0.0)
    {
        return;
    }
    double speedup = baseline_median / test_median;
    const char* verdict = (speedup > 1.05) ? "FASTER" : (speedup < 0.95) ? "SLOWER" : "SAME";
    std::cout << "    -> " << label << ": " << std::fixed << std::setprecision(2)
              << speedup << "x " << verdict << " than baseline\n";
}

// ============================================================================
// Data Generation
// ============================================================================

static std::vector<std::string> generate_unique_strings(size_t n_unique,
                                                         size_t min_len,
                                                         size_t max_len,
                                                         uint64_t seed)
{
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
    std::uniform_int_distribution<int> char_dist('a', 'z');

    std::unordered_set<std::string> seen;
    std::vector<std::string> result;
    result.reserve(n_unique);

    while (result.size() < n_unique)
    {
        size_t len = len_dist(rng);
        std::string s(len, ' ');
        for (auto& c : s)
        {
            c = static_cast<char>(char_dist(rng));
        }
        if (seen.insert(s).second)
        {
            result.push_back(std::move(s));
        }
    }

    return result;
}

/// Build workload indices with a controlled duplication ratio.
/// The unique portion is computed from n_unique (not n_ops) so that
/// varying dup_ratio produces genuinely different unique counts.
static std::vector<size_t> generate_workload_indices(size_t n_unique,
                                                      size_t n_ops,
                                                      double dup_ratio,
                                                      uint64_t seed)
{
    std::mt19937_64 rng(seed);
    std::vector<size_t> indices;
    indices.reserve(n_ops);

    size_t n_unique_used = static_cast<size_t>(
        static_cast<double>(n_unique) * (1.0 - dup_ratio));
    if (n_unique_used < 1 && n_unique > 0)
    {
        n_unique_used = 1;
    }
    if (n_unique_used > n_unique)
    {
        n_unique_used = n_unique;
    }

    // First: insert each unique string once
    for (size_t i = 0; i < n_unique_used && indices.size() < n_ops; ++i)
    {
        indices.push_back(i);
    }

    // Fill remainder with random picks from the unique portion (duplicates)
    if (n_unique_used > 0)
    {
        std::uniform_int_distribution<size_t> dup_dist(0, n_unique_used - 1);
        while (indices.size() < n_ops)
        {
            indices.push_back(dup_dist(rng));
        }
    }

    std::shuffle(indices.begin(), indices.end(), rng);
    return indices;
}

// ============================================================================
// Shared Corpus (generated once, used by all sections)
// ============================================================================

static constexpr size_t N_UNIQUE_SMALL  = 1000;
static constexpr size_t N_UNIQUE_MEDIUM = 10000;
static constexpr size_t N_UNIQUE_LARGE  = 100000;

struct SharedCorpus
{
    std::vector<std::string> short_strings;  // 4-15 chars (SSO range)
    std::vector<std::string> medium_strings; // 20-50 chars
    std::vector<std::string> long_strings;   // 100-200 chars
    std::vector<std::string> miss_strings;   // guaranteed not in any pool

    size_t n_ops = 100000;

    void generate(uint64_t seed, size_t target_ops)
    {
        n_ops = target_ops;
        short_strings  = generate_unique_strings(N_UNIQUE_LARGE,  4, 15,  seed);
        medium_strings = generate_unique_strings(N_UNIQUE_LARGE,  20, 50, seed + 1);
        long_strings   = generate_unique_strings(N_UNIQUE_MEDIUM, 100, 200, seed + 2);
        miss_strings   = generate_unique_strings(n_ops, 4, 15, seed + 999999);
    }
};

static SharedCorpus g_corpus;

// ============================================================================
// Intern Adapter Interface
//
// Each adapter wraps a different string interning implementation.
// Modeled after IVectorAdapter (SmallVector) / IMapAdapter (FlatMapSet).
// ============================================================================

struct IInternAdapter
{
    virtual ~IInternAdapter() = default;

    virtual const char* name() const = 0;

    virtual void setup(size_t hint) = 0;
    virtual void teardown() = 0;

    virtual size_t intern_unique(const std::vector<std::string>& corpus, size_t N) = 0;
    virtual size_t intern_dup(const std::vector<std::string>& corpus,
                              const std::vector<size_t>& indices) = 0;
};

// ============================================================================
// fat_p::StringPool<SingleThreadedPolicy> Adapter
// ============================================================================

class FatpSTAdapter final : public IInternAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::StringPool<ST>";
    }

    void setup(size_t) override {}
    void teardown() override {}

    size_t intern_unique(const std::vector<std::string>& corpus, size_t N) override
    {
        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(N);
        for (size_t i = 0; i < N; ++i)
        {
            auto ptr = pool.intern(corpus[i]);
            benchmark_sink += reinterpret_cast<intptr_t>(ptr);
        }
        return N;
    }

    size_t intern_dup(const std::vector<std::string>& corpus,
                      const std::vector<size_t>& indices) override
    {
        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(N_UNIQUE_SMALL);
        size_t n = indices.size();
        for (size_t i = 0; i < n; ++i)
        {
            auto ptr = pool.intern(corpus[indices[i]]);
            benchmark_sink += reinterpret_cast<intptr_t>(ptr);
        }
        return n;
    }
};

// ============================================================================
// fat_p::StringPool<SharedMutexPolicy> Adapter
// ============================================================================

class FatpSMAdapter final : public IInternAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::StringPool<SM>";
    }

    void setup(size_t) override {}
    void teardown() override {}

    size_t intern_unique(const std::vector<std::string>& corpus, size_t N) override
    {
        fat_p::StringPool<fat_p::SharedMutexPolicy> pool;
        pool.reserve(N);
        for (size_t i = 0; i < N; ++i)
        {
            auto ptr = pool.intern(corpus[i]);
            benchmark_sink += reinterpret_cast<intptr_t>(ptr);
        }
        return N;
    }

    size_t intern_dup(const std::vector<std::string>& corpus,
                      const std::vector<size_t>& indices) override
    {
        fat_p::StringPool<fat_p::SharedMutexPolicy> pool;
        pool.reserve(N_UNIQUE_SMALL);
        size_t n = indices.size();
        for (size_t i = 0; i < n; ++i)
        {
            auto ptr = pool.intern(corpus[indices[i]]);
            benchmark_sink += reinterpret_cast<intptr_t>(ptr);
        }
        return n;
    }
};

// ============================================================================
// std::unordered_set<string> Adapter (manual interning baseline)
// ============================================================================

class UnorderedSetAdapter final : public IInternAdapter
{
public:
    const char* name() const override
    {
        return "std::unordered_set";
    }

    void setup(size_t) override {}
    void teardown() override {}

    size_t intern_unique(const std::vector<std::string>& corpus, size_t N) override
    {
        std::unordered_set<std::string> set;
        set.reserve(N);
        for (size_t i = 0; i < N; ++i)
        {
            auto [it, inserted] = set.insert(corpus[i]);
            benchmark_sink += reinterpret_cast<intptr_t>(it->c_str());
        }
        return N;
    }

    size_t intern_dup(const std::vector<std::string>& corpus,
                      const std::vector<size_t>& indices) override
    {
        std::unordered_set<std::string> set;
        set.reserve(N_UNIQUE_SMALL);
        size_t n = indices.size();
        for (size_t i = 0; i < n; ++i)
        {
            auto [it, ins] = set.insert(corpus[indices[i]]);
            benchmark_sink += reinterpret_cast<intptr_t>(it->c_str());
        }
        return n;
    }
};

// ============================================================================
// std::unordered_map<string, const char*> Adapter
// ============================================================================

class UnorderedMapAdapter final : public IInternAdapter
{
public:
    const char* name() const override
    {
        return "std::unordered_map";
    }

    void setup(size_t) override {}
    void teardown() override {}

    size_t intern_unique(const std::vector<std::string>& corpus, size_t N) override
    {
        std::unordered_map<std::string, const char*> map;
        map.reserve(N);
        for (size_t i = 0; i < N; ++i)
        {
            auto [it, inserted] = map.try_emplace(corpus[i], nullptr);
            if (inserted)
            {
                it->second = it->first.c_str();
            }
            benchmark_sink += reinterpret_cast<intptr_t>(it->second);
        }
        return N;
    }

    size_t intern_dup(const std::vector<std::string>& corpus,
                      const std::vector<size_t>& indices) override
    {
        std::unordered_map<std::string, const char*> map;
        map.reserve(N_UNIQUE_SMALL);
        size_t n = indices.size();
        for (size_t i = 0; i < n; ++i)
        {
            auto [it, ins] = map.try_emplace(corpus[indices[i]], nullptr);
            if (ins)
            {
                it->second = it->first.c_str();
            }
            benchmark_sink += reinterpret_cast<intptr_t>(it->second);
        }
        return n;
    }
};

// ============================================================================
// boost::flyweight Adapter
// ============================================================================

#if HAS_BOOST_FLYWEIGHT
class BoostFlyweightAdapter final : public IInternAdapter
{
public:
    const char* name() const override
    {
        return "boost::flyweight";
    }

    void setup(size_t) override {}
    void teardown() override {}

    size_t intern_unique(const std::vector<std::string>& corpus, size_t N) override
    {
        std::vector<boost::flyweight<std::string>> storage;
        storage.reserve(N);
        for (size_t i = 0; i < N; ++i)
        {
            storage.emplace_back(corpus[i]);
            benchmark_sink += reinterpret_cast<intptr_t>(storage.back().get().c_str());
        }
        return N;
    }

    size_t intern_dup(const std::vector<std::string>& corpus,
                      const std::vector<size_t>& indices) override
    {
        std::vector<boost::flyweight<std::string>> storage;
        storage.reserve(indices.size());
        size_t n = indices.size();
        for (size_t i = 0; i < n; ++i)
        {
            storage.emplace_back(corpus[indices[i]]);
            benchmark_sink += reinterpret_cast<intptr_t>(storage.back().get().c_str());
        }
        return n;
    }
};
#endif

// ============================================================================
// Correctness Checks (always outside timed regions)
// ============================================================================

static bool verify_intern_correctness()
{
    fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
    const char* a = pool.intern("hello");
    const char* b = pool.intern("hello");
    const char* c = pool.intern("world");

    bool ok = true;
    if (a != b)
    {
        std::cerr << "FAIL: intern(\"hello\") returned different pointers\n";
        ok = false;
    }
    if (a == c)
    {
        std::cerr << "FAIL: intern(\"hello\") == intern(\"world\")\n";
        ok = false;
    }
    if (pool.size() != 2)
    {
        std::cerr << "FAIL: pool.size() = " << pool.size() << ", expected 2\n";
        ok = false;
    }
    if (!pool.contains("hello"))
    {
        std::cerr << "FAIL: pool.contains(\"hello\") returned false\n";
        ok = false;
    }
    if (pool.contains("missing"))
    {
        std::cerr << "FAIL: pool.contains(\"missing\") returned true\n";
        ok = false;
    }

    const char* f = pool.find("hello");
    if (f != a)
    {
        std::cerr << "FAIL: pool.find(\"hello\") != intern result\n";
        ok = false;
    }
    if (pool.find("missing") != nullptr)
    {
        std::cerr << "FAIL: pool.find(\"missing\") != nullptr\n";
        ok = false;
    }

    return ok;
}

// ============================================================================
// Helper: build adapter list for intern benchmarks
// ============================================================================

static std::vector<std::unique_ptr<IInternAdapter>> make_intern_adapters()
{
    std::vector<std::unique_ptr<IInternAdapter>> adapters;
    adapters.push_back(std::make_unique<FatpSTAdapter>());
    adapters.push_back(std::make_unique<FatpSMAdapter>());
    adapters.push_back(std::make_unique<UnorderedSetAdapter>());
    adapters.push_back(std::make_unique<UnorderedMapAdapter>());
#if HAS_BOOST_FLYWEIGHT
    adapters.push_back(std::make_unique<BoostFlyweightAdapter>());
#endif
    return adapters;
}

// ============================================================================
// Section 1: Intern Throughput - Unique Strings
// ============================================================================

void benchmark_intern_unique()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 1: Intern Throughput - Unique Strings\n";
    std::cout << "================================================================================\n";
    std::cout << "\n  Contract: Insert N unique strings into empty pool/set. Measures cold insertion.\n";
    std::cout << "  No string appears twice. This is the worst case for interning (100% miss).\n\n";

    print_cpu_context("Section start");

    auto adapters = make_intern_adapters();
    std::mt19937 rng(42);

    for (size_t N : {N_UNIQUE_SMALL, N_UNIQUE_MEDIUM, N_UNIQUE_LARGE})
    {
        std::cout << "\n  --- N = " << N << " unique strings (short, 4-15 chars) ---\n\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        std::vector<BenchResult> results;
        for (auto& a : adapters)
        {
            results.push_back({a->name(), {}});
        }

        // Warmup (round-robin with randomized order)
        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                adapters[idx]->setup(N);
                adapters[idx]->intern_unique(g_corpus.short_strings, N);
                adapters[idx]->teardown();
            }
        }

        // Measured runs (round-robin with randomized order)
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                adapters[idx]->setup(N);

                Timer t;
                t.start();
                size_t ops = adapters[idx]->intern_unique(g_corpus.short_strings, N);
                double elapsed = t.elapsed_ns();

                adapters[idx]->teardown();

                results[idx].samples.push_back(ns_per_op(elapsed, ops));
            }
        }

        print_results(results);

        auto s_fatp = Statistics::compute(results[0].samples);
        auto s_uset = Statistics::compute(results[2].samples);
        print_speedup("fat_p<ST> vs std::uset", s_uset.median, s_fatp.median);
    }
}

// ============================================================================
// Section 2: Intern Throughput - High Duplication (90% Hit Rate)
// ============================================================================

void benchmark_intern_duplicate()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)\n";
    std::cout << "================================================================================\n";
    std::cout << "\n  Contract: Intern N strings with 90% duplicates. This is the common case:\n";
    std::cout << "  config keys, JSON field names, log messages with repeated patterns.\n";
    std::cout << "  Uses " << N_UNIQUE_SMALL << " unique strings, " << g_corpus.n_ops << " total ops.\n\n";

    print_cpu_context("Section start");
    cooling_delay(COOLING_DELAY_SECTION_MS, "section transition");

    auto adapters = make_intern_adapters();
    std::mt19937 rng(43);

    auto dup_indices = generate_workload_indices(
        N_UNIQUE_SMALL, g_corpus.n_ops, 0.9, g_config.seed + 50);

    std::vector<BenchResult> results;
    for (auto& a : adapters)
    {
        results.push_back({a->name(), {}});
    }

    // Warmup
    for (size_t run = 0; run < WARMUP_RUNS(); ++run)
    {
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            adapters[idx]->setup(g_corpus.n_ops);
            adapters[idx]->intern_dup(g_corpus.short_strings, dup_indices);
            adapters[idx]->teardown();
        }
    }

    // Measured runs (round-robin)
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            adapters[idx]->setup(g_corpus.n_ops);

            Timer t;
            t.start();
            size_t ops = adapters[idx]->intern_dup(g_corpus.short_strings, dup_indices);
            double elapsed = t.elapsed_ns();

            adapters[idx]->teardown();

            results[idx].samples.push_back(ns_per_op(elapsed, ops));
        }
    }

    print_results(results);

    auto s_fatp = Statistics::compute(results[0].samples);
    auto s_uset = Statistics::compute(results[2].samples);
    print_speedup("fat_p<ST> vs std::uset", s_uset.median, s_fatp.median);
}

// ============================================================================
// Section 3: Lookup Throughput
// ============================================================================

void benchmark_lookup()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 3: Lookup Throughput\n";
    std::cout << "================================================================================\n";
    std::cout << "\n  Contract: Look up strings in a pre-populated pool. Tests find() performance\n";
    std::cout << "  for both hits (string exists) and misses (string not in pool).\n\n";

    print_cpu_context("Section start");
    cooling_delay(COOLING_DELAY_SECTION_MS, "section transition");

    const size_t POOL_SIZE = N_UNIQUE_MEDIUM;
    const size_t LOOKUP_OPS = g_corpus.n_ops;

    // Pre-populate pools (outside timed region)
    fat_p::StringPool<fat_p::SingleThreadedPolicy> pool_fatp;
    pool_fatp.reserve(POOL_SIZE);
    std::unordered_set<std::string> pool_uset;
    pool_uset.reserve(POOL_SIZE);
    std::unordered_map<std::string, const char*> pool_umap;
    pool_umap.reserve(POOL_SIZE);

    for (size_t i = 0; i < POOL_SIZE; ++i)
    {
        pool_fatp.intern(g_corpus.short_strings[i]);
        pool_uset.insert(g_corpus.short_strings[i]);
        auto [it, ins] = pool_umap.try_emplace(g_corpus.short_strings[i], nullptr);
        if (ins)
        {
            it->second = it->first.c_str();
        }
    }

    // Lookup indices (random from pool)
    std::mt19937_64 idx_rng(g_config.seed + 100);
    std::uniform_int_distribution<size_t> hit_dist(0, POOL_SIZE - 1);
    std::vector<size_t> hit_indices(LOOKUP_OPS);
    for (auto& idx : hit_indices)
    {
        idx = hit_dist(idx_rng);
    }

    // === Lookup Hit ===
    {
        std::cout << "  --- Lookup Hit (N = " << LOOKUP_OPS
                  << " ops, pool size = " << POOL_SIZE << ") ---\n\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_CASE_MS, nullptr);

        // Use lambda-based libs (pools are pre-populated, no adapter teardown needed)
        struct Lib { const char* nm; std::function<size_t()> fn; };

        std::vector<Lib> libs;
        libs.push_back({"fat_p::StringPool find", [&]() -> size_t {
            size_t found = 0;
            for (size_t i = 0; i < LOOKUP_OPS; ++i)
            {
                auto ptr = pool_fatp.find(g_corpus.short_strings[hit_indices[i]]);
                if (ptr) ++found;
                benchmark_sink += reinterpret_cast<intptr_t>(ptr);
            }
            return LOOKUP_OPS;
        }});
        libs.push_back({"std::unordered_set find", [&]() -> size_t {
            size_t found = 0;
            for (size_t i = 0; i < LOOKUP_OPS; ++i)
            {
                auto it = pool_uset.find(g_corpus.short_strings[hit_indices[i]]);
                if (it != pool_uset.end()) ++found;
                benchmark_sink += static_cast<int64_t>(found);
            }
            return LOOKUP_OPS;
        }});
        libs.push_back({"std::unordered_map find", [&]() -> size_t {
            size_t found = 0;
            for (size_t i = 0; i < LOOKUP_OPS; ++i)
            {
                auto it = pool_umap.find(g_corpus.short_strings[hit_indices[i]]);
                if (it != pool_umap.end()) ++found;
                benchmark_sink += static_cast<int64_t>(found);
            }
            return LOOKUP_OPS;
        }});

        std::vector<BenchResult> results;
        for (auto& l : libs)
        {
            results.push_back({l.nm, {}});
        }

        std::vector<size_t> order(libs.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 shuffle_rng(44);

        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            std::shuffle(order.begin(), order.end(), shuffle_rng);
            for (size_t idx : order) { libs[idx].fn(); }
        }
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            std::shuffle(order.begin(), order.end(), shuffle_rng);
            for (size_t idx : order)
            {
                Timer t;
                t.start();
                size_t ops = libs[idx].fn();
                double elapsed = t.elapsed_ns();
                results[idx].samples.push_back(ns_per_op(elapsed, ops));
            }
        }

        print_results(results);
        auto s0 = Statistics::compute(results[0].samples);
        auto s1 = Statistics::compute(results[1].samples);
        print_speedup("fat_p vs std::uset", s1.median, s0.median);
    }

    // === Lookup Miss ===
    {
        std::cout << "\n  --- Lookup Miss (N = " << LOOKUP_OPS
                  << " ops, pool size = " << POOL_SIZE << ") ---\n\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_CASE_MS, nullptr);

        struct Lib { const char* nm; std::function<size_t()> fn; };

        std::vector<Lib> libs;
        libs.push_back({"fat_p::StringPool find", [&]() -> size_t {
            size_t found = 0;
            for (size_t i = 0; i < LOOKUP_OPS; ++i)
            {
                auto ptr = pool_fatp.find(g_corpus.miss_strings[i]);
                if (ptr) ++found;
                benchmark_sink += reinterpret_cast<intptr_t>(ptr);
            }
            return LOOKUP_OPS;
        }});
        libs.push_back({"std::unordered_set find", [&]() -> size_t {
            size_t found = 0;
            for (size_t i = 0; i < LOOKUP_OPS; ++i)
            {
                auto it = pool_uset.find(g_corpus.miss_strings[i]);
                if (it != pool_uset.end()) ++found;
                benchmark_sink += static_cast<int64_t>(found);
            }
            return LOOKUP_OPS;
        }});
        libs.push_back({"std::unordered_map find", [&]() -> size_t {
            size_t found = 0;
            for (size_t i = 0; i < LOOKUP_OPS; ++i)
            {
                auto it = pool_umap.find(g_corpus.miss_strings[i]);
                if (it != pool_umap.end()) ++found;
                benchmark_sink += static_cast<int64_t>(found);
            }
            return LOOKUP_OPS;
        }});

        std::vector<BenchResult> results;
        for (auto& l : libs)
        {
            results.push_back({l.nm, {}});
        }

        std::vector<size_t> order(libs.size());
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 shuffle_rng(45);

        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            std::shuffle(order.begin(), order.end(), shuffle_rng);
            for (size_t idx : order) { libs[idx].fn(); }
        }
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            std::shuffle(order.begin(), order.end(), shuffle_rng);
            for (size_t idx : order)
            {
                Timer t;
                t.start();
                size_t ops = libs[idx].fn();
                double elapsed = t.elapsed_ns();
                results[idx].samples.push_back(ns_per_op(elapsed, ops));
            }
        }

        print_results(results);
        auto s0 = Statistics::compute(results[0].samples);
        auto s1 = Statistics::compute(results[1].samples);
        print_speedup("fat_p vs std::uset", s1.median, s0.median);
    }
}

// ============================================================================
// Section 4: Pointer Comparison vs String Comparison
// ============================================================================

void benchmark_pointer_comparison()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 4: Pointer Comparison vs String Comparison\n";
    std::cout << "================================================================================\n";
    std::cout << "\n  Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).\n";
    std::cout << "  This is the key benefit of interning: identity checks become pointer equality.\n\n";

    print_cpu_context("Section start");
    cooling_delay(COOLING_DELAY_SECTION_MS, "section transition");

    const size_t CMP_OPS = g_corpus.n_ops;

    // Pre-intern and collect pointers (outside timed region)
    fat_p::StringPool<fat_p::SingleThreadedPolicy> cmp_pool;
    std::vector<const char*> cmp_ptrs;
    cmp_ptrs.reserve(N_UNIQUE_SMALL);
    for (size_t i = 0; i < N_UNIQUE_SMALL; ++i)
    {
        cmp_ptrs.push_back(cmp_pool.intern(g_corpus.short_strings[i]));
    }

    // Generate random comparison pairs
    std::mt19937_64 pair_rng(g_config.seed + 200);
    std::uniform_int_distribution<size_t> cmp_dist(0, N_UNIQUE_SMALL - 1);
    std::vector<std::pair<size_t, size_t>> cmp_pairs(CMP_OPS);
    for (auto& p : cmp_pairs)
    {
        p.first = cmp_dist(pair_rng);
        p.second = cmp_dist(pair_rng);
    }

    print_cpu_context();

    struct Lib { const char* nm; std::function<size_t()> fn; };

    std::vector<Lib> libs;
    libs.push_back({"pointer ==", [&]() -> size_t {
        size_t matches = 0;
        for (size_t i = 0; i < CMP_OPS; ++i)
        {
            if (cmp_ptrs[cmp_pairs[i].first] == cmp_ptrs[cmp_pairs[i].second])
            {
                ++matches;
            }
        }
        benchmark_sink += static_cast<int64_t>(matches);
        return CMP_OPS;
    }});
    libs.push_back({"std::strcmp", [&]() -> size_t {
        size_t matches = 0;
        for (size_t i = 0; i < CMP_OPS; ++i)
        {
            if (std::strcmp(cmp_ptrs[cmp_pairs[i].first],
                            cmp_ptrs[cmp_pairs[i].second]) == 0)
            {
                ++matches;
            }
        }
        benchmark_sink += static_cast<int64_t>(matches);
        return CMP_OPS;
    }});
    libs.push_back({"string_view ==", [&]() -> size_t {
        size_t matches = 0;
        for (size_t i = 0; i < CMP_OPS; ++i)
        {
            std::string_view a(cmp_ptrs[cmp_pairs[i].first]);
            std::string_view b(cmp_ptrs[cmp_pairs[i].second]);
            if (a == b) { ++matches; }
        }
        benchmark_sink += static_cast<int64_t>(matches);
        return CMP_OPS;
    }});

    // Round-robin
    std::vector<BenchResult> results;
    for (auto& l : libs)
    {
        results.push_back({l.nm, {}});
    }

    std::vector<size_t> order(libs.size());
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 shuffle_rng(46);

    for (size_t run = 0; run < WARMUP_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), shuffle_rng);
        for (size_t idx : order) { libs[idx].fn(); }
    }
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), shuffle_rng);
        for (size_t idx : order)
        {
            Timer t;
            t.start();
            size_t ops = libs[idx].fn();
            double elapsed = t.elapsed_ns();
            results[idx].samples.push_back(ns_per_op(elapsed, ops));
        }
    }

    print_results(results);

    auto s_ptr = Statistics::compute(results[0].samples);
    auto s_strcmp = Statistics::compute(results[1].samples);
    auto s_sv = Statistics::compute(results[2].samples);
    print_speedup("pointer vs strcmp", s_strcmp.median, s_ptr.median);
    print_speedup("pointer vs sv ==", s_sv.median, s_ptr.median);
}

// ============================================================================
// Section 5: String Length Impact
// ============================================================================

void benchmark_string_length()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 5: String Length Impact\n";
    std::cout << "================================================================================\n";
    std::cout << "\n  Contract: Intern performance across string lengths. Short strings benefit from\n";
    std::cout << "  SSO (no heap allocation for lookup), long strings show hashing cost.\n";
    std::cout << "  All three length categories run round-robin to eliminate thermal bias.\n\n";

    print_cpu_context("Section start");
    cooling_delay(COOLING_DELAY_SECTION_MS, "section transition");

    const size_t LEN_N = N_UNIQUE_MEDIUM;

    struct LenCase
    {
        const char* name;
        const std::vector<std::string>* corpus;
    };

    std::vector<LenCase> cases = {
        {"short (4-15 chars)",  &g_corpus.short_strings},
        {"medium (20-50 chars)", &g_corpus.medium_strings},
        {"long (100-200 chars)", &g_corpus.long_strings}
    };

    // Round-robin across length categories
    std::vector<BenchResult> results;
    for (auto& c : cases)
    {
        results.push_back({c.name, {}});
    }

    std::vector<size_t> order(cases.size());
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(47);

    // Warmup
    for (size_t run = 0; run < WARMUP_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (size_t idx : order)
        {
            fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
            pool.reserve(LEN_N);
            for (size_t i = 0; i < LEN_N; ++i)
            {
                auto ptr = pool.intern((*cases[idx].corpus)[i]);
                benchmark_sink += reinterpret_cast<intptr_t>(ptr);
            }
        }
    }

    // Measured
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (size_t idx : order)
        {
            fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
            pool.reserve(LEN_N);

            Timer t;
            t.start();
            for (size_t i = 0; i < LEN_N; ++i)
            {
                auto ptr = pool.intern((*cases[idx].corpus)[i]);
                benchmark_sink += reinterpret_cast<intptr_t>(ptr);
            }
            double elapsed = t.elapsed_ns();

            results[idx].samples.push_back(ns_per_op(elapsed, LEN_N));
        }
    }

    print_results(results);
}

// ============================================================================
// Section 6: Memory Efficiency
// ============================================================================

void benchmark_memory_efficiency()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 6: Memory Efficiency\n";
    std::cout << "================================================================================\n";
    std::cout << "\n  Contract: Memory savings from deduplication at various duplication rates.\n";
    std::cout << "  Uses " << N_UNIQUE_MEDIUM << " unique strings, " << g_corpus.n_ops << " total ops.\n";
    std::cout << "  Single-library measurement (no round-robin needed).\n\n";

    cooling_delay(COOLING_DELAY_SECTION_MS, "section transition");

    std::cout << std::fixed;
    std::cout << "  dup_rate | unique | total_interns |  hit_rate  | memory_saved\n";
    std::cout << "  ---------|--------|---------------|------------|-------------\n";

    for (double dup_rate : {0.0, 0.50, 0.90, 0.99})
    {
        auto indices = generate_workload_indices(
            N_UNIQUE_MEDIUM, g_corpus.n_ops, dup_rate, g_config.seed + 50);

        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(N_UNIQUE_MEDIUM);
        for (size_t i = 0; i < g_corpus.n_ops; ++i)
        {
            pool.intern(g_corpus.short_strings[indices[i]]);
        }

        auto st = pool.stats();
        std::cout << "  " << std::setw(7) << std::setprecision(0) << (dup_rate * 100) << "%"
                  << "  | " << std::setw(6) << st.unique_strings
                  << " | " << std::setw(13) << st.total_interns
                  << " | " << std::setw(9) << std::setprecision(1) << (st.hit_rate * 100) << "%"
                  << " | " << std::setw(10) << st.memory_saved << " bytes\n";
    }
}

// ============================================================================
// main
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
    hdr.component = "StringPool";
    hdr.warmup = WARMUP_RUNS();
    hdr.measured = MEASURED_RUNS();
    hdr.seed = g_config.seed;

    hdr.competitors.push_back({"fat_p::StringPool<ST>", true, "primary (SingleThreadedPolicy)"});
    hdr.competitors.push_back({"fat_p::StringPool<SM>", true, "SharedMutexPolicy"});
    hdr.competitors.push_back({"std::unordered_set<string>", true, "baseline"});
    hdr.competitors.push_back({"std::unordered_map<string,ptr>", true, "manual intern"});
#if HAS_BOOST_FLYWEIGHT
    hdr.competitors.push_back({"boost::flyweight<string>", true, ""});
#else
    hdr.competitors.push_back({"boost::flyweight<string>", false, "boost/flyweight.hpp not found"});
#endif

    hdr.has_extended_config = true;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = true;
    hdr.has_stabilization = !g_config.noStabilize;

    hdr.min_batch_ms = g_config.minBatchMs;
    hdr.scope_enabled = !g_config.noScope;
    hdr.stabilize_enabled = !g_config.noStabilize;
    hdr.cooldown_enabled = !g_config.noCooldown;
    hdr.cool_section_ms = COOLING_DELAY_SECTION_MS;
    hdr.cool_size_ms = COOLING_DELAY_SIZE_MS;
    hdr.cool_case_ms = COOLING_DELAY_CASE_MS;

    fat_p::bench::print_standard_header(hdr);

    // =========================================================================
    // Correctness checks (before any timing)
    // =========================================================================
    std::cout << "\n--- Correctness Checks ---\n";
    if (!verify_intern_correctness())
    {
        std::cerr << "FATAL: StringPool correctness check failed. Aborting.\n";
        return 1;
    }
    std::cout << "  intern/find/contains: PASS\n";

    // =========================================================================
    // Generate shared corpus (once)
    // =========================================================================
    std::cout << "\n--- Generating corpus ---\n";
    g_corpus.generate(g_config.seed, 100000);
    std::cout << "  short:  " << g_corpus.short_strings.size() << " unique (4-15 chars)\n";
    std::cout << "  medium: " << g_corpus.medium_strings.size() << " unique (20-50 chars)\n";
    std::cout << "  long:   " << g_corpus.long_strings.size() << " unique (100-200 chars)\n";
    std::cout << "  miss:   " << g_corpus.miss_strings.size() << " unique (guaranteed misses)\n";

    // Verify corpus uniqueness
    {
        fat_p::StringPool<fat_p::SingleThreadedPolicy> verify_pool;
        for (size_t i = 0; i < N_UNIQUE_SMALL; ++i)
        {
            verify_pool.intern(g_corpus.short_strings[i]);
        }
        if (verify_pool.size() != N_UNIQUE_SMALL)
        {
            std::cerr << "FATAL: Corpus not unique (" << verify_pool.size()
                      << " vs " << N_UNIQUE_SMALL << ")\n";
            return 1;
        }
        std::cout << "  uniqueness: PASS\n";
    }

    // =========================================================================
    // Run all benchmark sections
    // =========================================================================
    benchmark_intern_unique();
    benchmark_intern_duplicate();
    benchmark_lookup();
    benchmark_pointer_comparison();
    benchmark_string_length();
    benchmark_memory_efficiency();

    // =========================================================================
    // Footer
    // =========================================================================
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}

// Clean up competitor-detection macros
#undef HAS_BOOST_FLYWEIGHT
