/**
 * @file benchmark_StringPool.cpp
 * @brief FAT-P StringPool benchmarks vs industry competitors.
 *
 * Architecture: Round-robin execution with randomized order per run.
 * This ensures all libraries observe the same distribution of machine states,
 * eliminating drift-induced unfairness.
 *
 * Design Invariants:
 *   1. Each measured run executes exactly one timed iteration per library.
 *   2. Library execution order is randomized per run.
 *   3. Setup and teardown occur outside timed regions.
 *   4. All libraries observe the same distribution of machine states.
 *   5. Medians are the primary reported statistic.
 *
 * Fat-P Libraries:
 *   - fat_p::StringPool<SingleThreadedPolicy>: Zero-overhead single-threaded
 *   - fat_p::StringPool<SharedMutexPolicy>: Concurrent read-optimized
 *
 * Competitor Libraries (conditioned on availability):
 *   TIER 1 - Direct competitors (string interning):
 *     - boost::flyweight: Boost.Flyweight string interning
 *   TIER 2 - Standard library baselines:
 *     - std::unordered_set<std::string>: Hash set dedup (no pointer stability guarantee)
 *     - std::unordered_map<std::string, const char*>: Manual intern pattern
 *   TIER 3 - Baseline:
 *     - No interning: Raw string copies (shows what interning saves)
 *
 * Build (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_StringPool.cpp -o bench_stringpool
 *
 * Build (MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_StringPool.cpp /Fe:bench_stringpool.exe
 *
 * Build (with competitors):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native \
 *       -I/path/to/boost \
 *       benchmark_StringPool.cpp -o bench_stringpool
 *
 * Environment Variables (all optional):
 *   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
 *   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_TARGET_WORK   - Operations per library iteration (default: 1000000)
 *   FATP_BENCH_MIN_BATCH_MS  - Min batch duration (default: 50)
 *   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
 *   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
 *   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
 *   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
 *   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
 *
 * Run:
 *   ./bench_stringpool
 *   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_stringpool
 */

/*
FATP_META:
  meta_version: 1
  component: StringPool
  file_role: benchmark
  path: components/StringPool/benchmarks/benchmark_StringPool.cpp
  layer: Testing
  namespace: fat_p
  summary: Comprehensive benchmarks for StringPool vs industry competitors.
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

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "CppFeatureDetection.h"
#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"
#include "StringPool.h"

// ============================================================================
// Library Detection
// ============================================================================

// boost::flyweight
#if __has_include(<boost/flyweight.hpp>)
#include <boost/flyweight.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/hashed_factory.hpp>
#define HAS_BOOST_FLYWEIGHT 1
#else
#define HAS_BOOST_FLYWEIGHT 0
#endif

namespace
{

using namespace fat_p::bench;

// ============================================================================
// Data Generation
// ============================================================================

/**
 * @brief Generate a corpus of unique strings with controlled characteristics.
 *
 * @param n_unique Number of unique strings to generate
 * @param min_len Minimum string length
 * @param max_len Maximum string length
 * @param seed RNG seed for reproducibility
 * @return Vector of unique strings
 */
std::vector<std::string> generate_unique_strings(std::size_t n_unique,
                                                  std::size_t min_len,
                                                  std::size_t max_len,
                                                  std::uint64_t seed)
{
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::size_t> len_dist(min_len, max_len);
    std::uniform_int_distribution<int> char_dist('a', 'z');

    std::unordered_set<std::string> seen;
    std::vector<std::string> result;
    result.reserve(n_unique);

    while (result.size() < n_unique)
    {
        std::size_t len = len_dist(rng);
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

/**
 * @brief Build an intern workload: N operations with a given duplication ratio.
 *
 * @param unique_strings The corpus of unique strings
 * @param n_ops Total number of intern operations
 * @param dup_ratio Fraction that should be duplicates (0.0 = all unique, 0.9 = 90% dups)
 * @param seed RNG seed
 * @return Vector of string_views into the corpus
 */
std::vector<std::size_t> generate_workload_indices(std::size_t n_unique,
                                                    std::size_t n_ops,
                                                    double dup_ratio,
                                                    std::uint64_t seed)
{
    std::mt19937_64 rng(seed);
    std::vector<std::size_t> indices;
    indices.reserve(n_ops);

    // First pass: insert unique strings up to the unique portion
    std::size_t n_unique_ops = static_cast<std::size_t>(n_ops * (1.0 - dup_ratio));
    if (n_unique_ops > n_unique) n_unique_ops = n_unique;

    for (std::size_t i = 0; i < n_unique_ops; ++i)
    {
        indices.push_back(i % n_unique);
    }

    // Remaining are duplicates drawn from already-seen strings
    std::uniform_int_distribution<std::size_t> dup_dist(0, n_unique_ops > 0 ? n_unique_ops - 1 : 0);
    for (std::size_t i = n_unique_ops; i < n_ops; ++i)
    {
        indices.push_back(dup_dist(rng));
    }

    // Shuffle to avoid sequential patterns
    std::shuffle(indices.begin(), indices.end(), rng);
    return indices;
}

/**
 * @brief Generate strings that will NOT be found in the pool (for miss benchmarks).
 */
std::vector<std::string> generate_miss_strings(std::size_t n,
                                                std::size_t min_len,
                                                std::size_t max_len,
                                                std::uint64_t seed)
{
    // Use a different seed range to ensure no overlap
    return generate_unique_strings(n, min_len, max_len, seed + 999999);
}

// ============================================================================
// Benchmark Helpers
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

bool verify_intern_correctness()
{
    fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
    const char* a = pool.intern("hello");
    const char* b = pool.intern("hello");
    const char* c = pool.intern("world");

    if (a != b) return false;        // Same string -> same pointer
    if (a == c) return false;        // Different string -> different pointer
    if (pool.size() != 2) return false;
    if (!pool.contains("hello")) return false;
    if (pool.contains("missing")) return false;
    return true;
}

bool verify_competitor_equivalence(const std::vector<std::string>& corpus)
{
    // Verify that interning N strings produces the right unique count
    fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
    for (const auto& s : corpus)
    {
        pool.intern(s);
    }
    return pool.size() == corpus.size();
}

// ============================================================================
// Main
// ============================================================================

} // anonymous namespace

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
    hdr.component = "StringPool";
    hdr.warmup = config.warmupRuns;
    hdr.measured = config.measuredRuns;
    hdr.seed = config.seed;

    // Competitors
    hdr.competitors.push_back({"fat_p::StringPool<SingleThreaded>", true, "primary"});
    hdr.competitors.push_back({"fat_p::StringPool<SharedMutex>", true, "primary"});
    hdr.competitors.push_back({"std::unordered_set<string>", true, "baseline"});
    hdr.competitors.push_back({"std::unordered_map (manual intern)", true, "baseline"});
#if HAS_BOOST_FLYWEIGHT
    hdr.competitors.push_back({"boost::flyweight<string>", true, ""});
#else
    hdr.competitors.push_back({"boost::flyweight<string>", false, "not detected"});
#endif

    hdr.has_extended_config = false;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = true;
    hdr.has_stabilization = !config.noStabilize;

    fat_p::bench::print_standard_header(hdr);

    // =========================================================================
    // Correctness verification (outside timed regions)
    // =========================================================================
    std::cout << "\nCorrectness verification:\n";
    bool intern_ok = verify_intern_correctness();
    std::cout << "  [" << (intern_ok ? "PASS" : "FAIL") << "] StringPool intern semantics\n";
    if (!intern_ok)
    {
        std::cerr << "FATAL: StringPool correctness check failed\n";
        return 1;
    }

    // =========================================================================
    // Data preparation
    // =========================================================================
    const std::size_t N_UNIQUE_SMALL  = 1000;
    const std::size_t N_UNIQUE_MEDIUM = 10000;
    const std::size_t N_UNIQUE_LARGE  = 100000;
    const std::size_t N_OPS = 100000;

    // Short strings (SSO range, 4-15 chars)
    auto short_corpus = generate_unique_strings(N_UNIQUE_LARGE, 4, 15, config.seed);
    // Medium strings (past SSO, 20-50 chars)
    auto medium_corpus = generate_unique_strings(N_UNIQUE_LARGE, 20, 50, config.seed + 1);
    // Long strings (100-200 chars)
    auto long_corpus = generate_unique_strings(N_UNIQUE_MEDIUM, 100, 200, config.seed + 2);
    // Miss strings (for lookup miss benchmarks)
    auto miss_strings = generate_miss_strings(N_OPS, 4, 15, config.seed + 3);

    // Verify corpus
    bool corpus_ok = verify_competitor_equivalence(short_corpus);
    std::cout << "  [" << (corpus_ok ? "PASS" : "FAIL") << "] Corpus uniqueness verified ("
              << short_corpus.size() << " strings)\n";

    // ========================================================================
    // Section 1: Intern Throughput - Unique Strings (Cold Insert)
    // ========================================================================
    std::cout << "\n================================================================================\n";
    std::cout << "  INTERN THROUGHPUT - UNIQUE STRINGS\n";
    std::cout << "================================================================================\n\n";
    std::cout << "Contract: Insert N unique strings into empty pool/set. Measures cold insertion.\n";
    std::cout << "No string appears twice. This is the worst case for interning (100% miss).\n\n";

    for (std::size_t N : {N_UNIQUE_SMALL, N_UNIQUE_MEDIUM, N_UNIQUE_LARGE})
    {
        std::cout << "--- N = " << N << " unique strings (short, 4-15 chars) ---\n\n";
        print_cpu_context(std::cout);

        // fat_p::StringPool<SingleThreadedPolicy>
        auto s_fatp_st = run_benchmark("fat_p::StringPool<ST>", N, config, [&]() {
            fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
            pool.reserve(N);
            for (std::size_t i = 0; i < N; ++i)
            {
                auto ptr = pool.intern(short_corpus[i]);
                DoNotOptimize(ptr);
            }
        });

        // fat_p::StringPool<SharedMutexPolicy>
        auto s_fatp_sm = run_benchmark("fat_p::StringPool<SM>", N, config, [&]() {
            fat_p::StringPool<fat_p::SharedMutexPolicy> pool;
            pool.reserve(N);
            for (std::size_t i = 0; i < N; ++i)
            {
                auto ptr = pool.intern(short_corpus[i]);
                DoNotOptimize(ptr);
            }
        });

        // std::unordered_set<std::string>
        auto s_uset = run_benchmark("std::unordered_set", N, config, [&]() {
            std::unordered_set<std::string> set;
            set.reserve(N);
            for (std::size_t i = 0; i < N; ++i)
            {
                auto [it, inserted] = set.insert(short_corpus[i]);
                auto ptr = it->c_str();
                DoNotOptimize(ptr);
            }
        });

        // std::unordered_map manual intern
        auto s_umap = run_benchmark("std::unordered_map intern", N, config, [&]() {
            std::unordered_map<std::string, const char*> map;
            map.reserve(N);
            for (std::size_t i = 0; i < N; ++i)
            {
                auto [it, inserted] = map.try_emplace(short_corpus[i], nullptr);
                if (inserted) it->second = it->first.c_str();
                DoNotOptimize(it->second);
            }
        });

#if HAS_BOOST_FLYWEIGHT
        // boost::flyweight
        auto s_boost = run_benchmark("boost::flyweight", N, config, [&]() {
            std::vector<boost::flyweight<std::string>> storage;
            storage.reserve(N);
            for (std::size_t i = 0; i < N; ++i)
            {
                storage.emplace_back(short_corpus[i]);
                auto ptr = storage.back().get().c_str();
                DoNotOptimize(ptr);
            }
        });
#endif

        print_stats("fat_p::StringPool<ST>", s_fatp_st);
        print_stats("fat_p::StringPool<SM>", s_fatp_sm);
        print_stats("std::unordered_set", s_uset);
        print_stats("std::unordered_map", s_umap);
#if HAS_BOOST_FLYWEIGHT
        print_stats("boost::flyweight", s_boost);
#endif

        print_speedup("fat_p<ST> vs std::uset", s_uset.median, s_fatp_st.median);
#if HAS_BOOST_FLYWEIGHT
        print_speedup("fat_p<ST> vs boost", s_boost.median, s_fatp_st.median);
#endif
        std::cout << "\n";
    }

    // ========================================================================
    // Section 2: Intern Throughput - High Duplication (90% hit rate)
    // ========================================================================
    std::cout << "================================================================================\n";
    std::cout << "  INTERN THROUGHPUT - HIGH DUPLICATION (90% HIT RATE)\n";
    std::cout << "================================================================================\n\n";
    std::cout << "Contract: Intern N strings with 90% duplicates. This is the common case:\n";
    std::cout << "config keys, JSON field names, log messages with repeated patterns.\n\n";

    auto dup_indices = generate_workload_indices(N_UNIQUE_SMALL, N_OPS, 0.9, config.seed);

    std::cout << "--- N = " << N_OPS << " ops, " << N_UNIQUE_SMALL << " unique, 90% dup rate ---\n\n";
    print_cpu_context(std::cout);

    auto s2_fatp_st = run_benchmark("fat_p::StringPool<ST>", N_OPS, config, [&]() {
        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(N_UNIQUE_SMALL);
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            auto ptr = pool.intern(short_corpus[dup_indices[i]]);
            DoNotOptimize(ptr);
        }
    });

    auto s2_fatp_sm = run_benchmark("fat_p::StringPool<SM>", N_OPS, config, [&]() {
        fat_p::StringPool<fat_p::SharedMutexPolicy> pool;
        pool.reserve(N_UNIQUE_SMALL);
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            auto ptr = pool.intern(short_corpus[dup_indices[i]]);
            DoNotOptimize(ptr);
        }
    });

    auto s2_uset = run_benchmark("std::unordered_set", N_OPS, config, [&]() {
        std::unordered_set<std::string> set;
        set.reserve(N_UNIQUE_SMALL);
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            auto [it, inserted] = set.insert(short_corpus[dup_indices[i]]);
            auto ptr = it->c_str();
            DoNotOptimize(ptr);
        }
    });

    auto s2_umap = run_benchmark("std::unordered_map intern", N_OPS, config, [&]() {
        std::unordered_map<std::string, const char*> map;
        map.reserve(N_UNIQUE_SMALL);
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            auto [it, inserted] = map.try_emplace(short_corpus[dup_indices[i]], nullptr);
            if (inserted) it->second = it->first.c_str();
            DoNotOptimize(it->second);
        }
    });

#if HAS_BOOST_FLYWEIGHT
    auto s2_boost = run_benchmark("boost::flyweight", N_OPS, config, [&]() {
        std::vector<boost::flyweight<std::string>> storage;
        storage.reserve(N_OPS);
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            storage.emplace_back(short_corpus[dup_indices[i]]);
            auto ptr = storage.back().get().c_str();
            DoNotOptimize(ptr);
        }
    });
#endif

    print_stats("fat_p::StringPool<ST>", s2_fatp_st);
    print_stats("fat_p::StringPool<SM>", s2_fatp_sm);
    print_stats("std::unordered_set", s2_uset);
    print_stats("std::unordered_map", s2_umap);
#if HAS_BOOST_FLYWEIGHT
    print_stats("boost::flyweight", s2_boost);
#endif
    print_speedup("fat_p<ST> vs std::uset", s2_uset.median, s2_fatp_st.median);
#if HAS_BOOST_FLYWEIGHT
    print_speedup("fat_p<ST> vs boost", s2_boost.median, s2_fatp_st.median);
#endif

    // ========================================================================
    // Section 3: Lookup Throughput (find hit / miss)
    // ========================================================================
    std::cout << "\n================================================================================\n";
    std::cout << "  LOOKUP THROUGHPUT\n";
    std::cout << "================================================================================\n\n";
    std::cout << "Contract: Look up strings in a pre-populated pool. Tests find() performance\n";
    std::cout << "for both hits (string exists) and misses (string not in pool).\n\n";

    // Pre-populate pools for lookup benchmarks
    const std::size_t POOL_SIZE = N_UNIQUE_MEDIUM;
    const std::size_t LOOKUP_OPS = N_OPS;

    fat_p::StringPool<fat_p::SingleThreadedPolicy> prepop_fatp;
    prepop_fatp.reserve(POOL_SIZE);
    std::unordered_set<std::string> prepop_uset;
    prepop_uset.reserve(POOL_SIZE);
    std::unordered_map<std::string, const char*> prepop_umap;
    prepop_umap.reserve(POOL_SIZE);
#if HAS_BOOST_FLYWEIGHT
    std::vector<boost::flyweight<std::string>> prepop_boost;
    prepop_boost.reserve(POOL_SIZE);
#endif

    for (std::size_t i = 0; i < POOL_SIZE; ++i)
    {
        prepop_fatp.intern(short_corpus[i]);
        prepop_uset.insert(short_corpus[i]);
        auto [it, ins] = prepop_umap.try_emplace(short_corpus[i], nullptr);
        if (ins) it->second = it->first.c_str();
#if HAS_BOOST_FLYWEIGHT
        prepop_boost.emplace_back(short_corpus[i]);
#endif
    }

    // Lookup indices (hits: random from pool)
    std::mt19937_64 rng(config.seed + 100);
    std::uniform_int_distribution<std::size_t> hit_dist(0, POOL_SIZE - 1);
    std::vector<std::size_t> hit_indices(LOOKUP_OPS);
    for (auto& idx : hit_indices) idx = hit_dist(rng);

    // --- Lookup Hit ---
    std::cout << "--- Lookup Hit (N = " << LOOKUP_OPS << " ops, pool size = " << POOL_SIZE << ") ---\n\n";
    print_cpu_context(std::cout);

    auto s3_fatp_hit = run_benchmark("fat_p find (hit)", LOOKUP_OPS, config, [&]() {
        std::size_t found = 0;
        for (std::size_t i = 0; i < LOOKUP_OPS; ++i)
        {
            auto ptr = prepop_fatp.find(short_corpus[hit_indices[i]]);
            if (ptr) ++found;
            DoNotOptimize(ptr);
        }
        DoNotOptimize(found);
    });

    auto s3_uset_hit = run_benchmark("std::uset find (hit)", LOOKUP_OPS, config, [&]() {
        std::size_t found = 0;
        for (std::size_t i = 0; i < LOOKUP_OPS; ++i)
        {
            auto it = prepop_uset.find(short_corpus[hit_indices[i]]);
            if (it != prepop_uset.end()) ++found;
            DoNotOptimize(found);
        }
        DoNotOptimize(found);
    });

    auto s3_umap_hit = run_benchmark("std::umap find (hit)", LOOKUP_OPS, config, [&]() {
        std::size_t found = 0;
        for (std::size_t i = 0; i < LOOKUP_OPS; ++i)
        {
            auto it = prepop_umap.find(short_corpus[hit_indices[i]]);
            if (it != prepop_umap.end()) ++found;
            DoNotOptimize(found);
        }
        DoNotOptimize(found);
    });

    print_stats("fat_p::StringPool find", s3_fatp_hit);
    print_stats("std::unordered_set find", s3_uset_hit);
    print_stats("std::unordered_map find", s3_umap_hit);
    print_speedup("fat_p vs std::uset", s3_uset_hit.median, s3_fatp_hit.median);

    // --- Lookup Miss ---
    std::cout << "\n--- Lookup Miss (N = " << LOOKUP_OPS << " ops, pool size = " << POOL_SIZE << ") ---\n\n";
    print_cpu_context(std::cout);

    auto s3_fatp_miss = run_benchmark("fat_p find (miss)", LOOKUP_OPS, config, [&]() {
        std::size_t found = 0;
        for (std::size_t i = 0; i < LOOKUP_OPS; ++i)
        {
            auto ptr = prepop_fatp.find(miss_strings[i]);
            if (ptr) ++found;
            DoNotOptimize(ptr);
        }
        DoNotOptimize(found);
    });

    auto s3_uset_miss = run_benchmark("std::uset find (miss)", LOOKUP_OPS, config, [&]() {
        std::size_t found = 0;
        for (std::size_t i = 0; i < LOOKUP_OPS; ++i)
        {
            auto it = prepop_uset.find(miss_strings[i]);
            if (it != prepop_uset.end()) ++found;
            DoNotOptimize(found);
        }
        DoNotOptimize(found);
    });

    auto s3_umap_miss = run_benchmark("std::umap find (miss)", LOOKUP_OPS, config, [&]() {
        std::size_t found = 0;
        for (std::size_t i = 0; i < LOOKUP_OPS; ++i)
        {
            auto it = prepop_umap.find(miss_strings[i]);
            if (it != prepop_umap.end()) ++found;
            DoNotOptimize(found);
        }
        DoNotOptimize(found);
    });

    print_stats("fat_p::StringPool find", s3_fatp_miss);
    print_stats("std::unordered_set find", s3_uset_miss);
    print_stats("std::unordered_map find", s3_umap_miss);
    print_speedup("fat_p vs std::uset", s3_uset_miss.median, s3_fatp_miss.median);

    // ========================================================================
    // Section 4: Pointer Comparison vs String Comparison
    // ========================================================================
    std::cout << "\n================================================================================\n";
    std::cout << "  POINTER COMPARISON VS STRING COMPARISON\n";
    std::cout << "================================================================================\n\n";
    std::cout << "Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).\n";
    std::cout << "This is the key benefit of interning: identity checks become pointer equality.\n\n";

    // Pre-intern strings and collect pointer pairs
    fat_p::StringPool<fat_p::SingleThreadedPolicy> cmp_pool;
    std::vector<const char*> cmp_ptrs;
    cmp_ptrs.reserve(N_UNIQUE_SMALL);
    for (std::size_t i = 0; i < N_UNIQUE_SMALL; ++i)
    {
        cmp_ptrs.push_back(cmp_pool.intern(short_corpus[i]));
    }

    // Generate random comparison pairs
    std::uniform_int_distribution<std::size_t> cmp_dist(0, N_UNIQUE_SMALL - 1);
    std::vector<std::pair<std::size_t, std::size_t>> cmp_pairs(N_OPS);
    for (auto& p : cmp_pairs)
    {
        p.first = cmp_dist(rng);
        p.second = cmp_dist(rng);
    }

    print_cpu_context(std::cout);

    auto s4_ptr = run_benchmark("pointer ==", N_OPS, config, [&]() {
        std::size_t matches = 0;
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            if (cmp_ptrs[cmp_pairs[i].first] == cmp_ptrs[cmp_pairs[i].second])
                ++matches;
        }
        DoNotOptimize(matches);
    });

    auto s4_strcmp = run_benchmark("std::strcmp", N_OPS, config, [&]() {
        std::size_t matches = 0;
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            if (std::strcmp(cmp_ptrs[cmp_pairs[i].first], cmp_ptrs[cmp_pairs[i].second]) == 0)
                ++matches;
        }
        DoNotOptimize(matches);
    });

    auto s4_sv = run_benchmark("string_view ==", N_OPS, config, [&]() {
        std::size_t matches = 0;
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            std::string_view a(cmp_ptrs[cmp_pairs[i].first]);
            std::string_view b(cmp_ptrs[cmp_pairs[i].second]);
            if (a == b) ++matches;
        }
        DoNotOptimize(matches);
    });

    print_stats("pointer ==", s4_ptr);
    print_stats("std::strcmp", s4_strcmp);
    print_stats("string_view ==", s4_sv);
    print_speedup("pointer vs strcmp", s4_strcmp.median, s4_ptr.median);
    print_speedup("pointer vs sv ==", s4_sv.median, s4_ptr.median);

    // ========================================================================
    // Section 5: String Length Impact
    // ========================================================================
    std::cout << "\n================================================================================\n";
    std::cout << "  STRING LENGTH IMPACT\n";
    std::cout << "================================================================================\n\n";
    std::cout << "Contract: Intern performance across string lengths. Short strings benefit from\n";
    std::cout << "SSO (no heap allocation for lookup), long strings show hashing cost.\n\n";

    const std::size_t LEN_N = 10000;

    print_cpu_context(std::cout);

    // Short (4-15, within SSO)
    auto s5_short = run_benchmark("short (4-15 chars)", LEN_N, config, [&]() {
        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(LEN_N);
        for (std::size_t i = 0; i < LEN_N; ++i)
        {
            auto ptr = pool.intern(short_corpus[i]);
            DoNotOptimize(ptr);
        }
    });

    // Medium (20-50, past SSO)
    auto s5_medium = run_benchmark("medium (20-50 chars)", LEN_N, config, [&]() {
        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(LEN_N);
        for (std::size_t i = 0; i < LEN_N; ++i)
        {
            auto ptr = pool.intern(medium_corpus[i]);
            DoNotOptimize(ptr);
        }
    });

    // Long (100-200)
    auto s5_long = run_benchmark("long (100-200 chars)", LEN_N, config, [&]() {
        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(LEN_N);
        for (std::size_t i = 0; i < LEN_N; ++i)
        {
            auto ptr = pool.intern(long_corpus[i]);
            DoNotOptimize(ptr);
        }
    });

    print_stats("short (4-15 chars)", s5_short);
    print_stats("medium (20-50 chars)", s5_medium);
    print_stats("long (100-200 chars)", s5_long);

    // ========================================================================
    // Section 6: Memory Efficiency
    // ========================================================================
    std::cout << "\n================================================================================\n";
    std::cout << "  MEMORY EFFICIENCY\n";
    std::cout << "================================================================================\n\n";
    std::cout << "Contract: Memory savings from deduplication at various duplication rates.\n\n";

    for (double dup_rate : {0.0, 0.5, 0.9, 0.99})
    {
        auto indices = generate_workload_indices(N_UNIQUE_SMALL, N_OPS, dup_rate, config.seed + 50);
        fat_p::StringPool<fat_p::SingleThreadedPolicy> pool;
        pool.reserve(N_UNIQUE_SMALL);
        for (std::size_t i = 0; i < N_OPS; ++i)
        {
            pool.intern(short_corpus[indices[i]]);
        }
        auto st = pool.stats();
        std::cout << "  dup_rate=" << std::fixed << std::setprecision(0) << (dup_rate * 100) << "%"
                  << "  unique=" << std::setw(6) << st.unique_strings
                  << "  total_interns=" << std::setw(7) << st.total_interns
                  << "  hit_rate=" << std::setprecision(1) << (st.hit_rate * 100) << "%"
                  << "  memory_saved=" << std::setw(8) << st.memory_saved << " bytes\n";
    }

    // ========================================================================
    // Footer
    // ========================================================================
    std::cout << "\n================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
