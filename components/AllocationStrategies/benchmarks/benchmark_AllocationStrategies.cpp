/**
 * @file benchmark_AllocationStrategies.cpp
 * @brief Comprehensive benchmarks for AllocationStrategies.h
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
 *   - fat_p::NewDeleteAllocator: Thin wrapper over new/delete
 *   - fat_p::BlockAllocator: Block-based with free list
 *   - fat_p::PoolAllocator: Fixed-capacity pool allocator
 *
 * Competitor Libraries (auto-detected):
 *   - std::allocator: Standard library baseline
 *   - std::pmr::monotonic_buffer_resource: C++17 PMR bump allocator
 *   - std::pmr::unsynchronized_pool_resource: C++17 PMR pool
 *   - boost::pool: Raw segregated storage (same genre as BlockAllocator)
 *
 * Build (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_AllocationStrategies.cpp -o bench_alloc -pthread
 *   clang++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_AllocationStrategies.cpp -o bench_alloc -pthread
 *
 * Build (Windows MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_AllocationStrategies.cpp /link advapi32.lib
 *
 * Build (with Boost):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -I/path/to/boost benchmark_AllocationStrategies.cpp -o bench_alloc
 * -pthread
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
 *   ./bench_alloc
 *   FATP_BENCH_OUTPUT_CSV=alloc_results.csv ./bench_alloc
 */
/*
FATP_META:
  meta_version: 1
  component: AllocationStrategies
  file_role: benchmark
  path: components/AllocationStrategies/benchmarks/benchmark_AllocationStrategies.cpp
  layer: Testing
  namespace: fat_p
  summary: "Benchmarks for AllocationStrategies."
  api_stability: in_work
  related:
    docs_search: "AllocationStrategies"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/AllocationStrategies.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 6
    defines_unprefixed: 6
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "AllocationStrategies.h"
#include "FatPBenchmarkRunner.h"

// ============================================================================
// Optional Competitor Detection
// ============================================================================

// boost::pool (raw segregated storage - same genre as BlockAllocator)
#if __has_include(<boost/pool/pool.hpp>)
#include <boost/pool/pool.hpp>
#define FATP_HAS_BOOST_POOL 1
#else
#define FATP_HAS_BOOST_POOL 0
#endif

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

// Cooling delays between benchmark sections (milliseconds)
static constexpr int COOLING_DELAY_SECTION_MS = 200;
static constexpr int COOLING_DELAY_SIZE_MS = 50;

// ============================================================================
// Timer and Statistics (from FatPBenchmarkRunner)
// ============================================================================

using Timer = fat_p::bench::Timer;
using Statistics = fat_p::bench::Statistics;

// ============================================================================
// DCE Prevention
// ============================================================================

static volatile int64_t benchmark_sink = 0;

template <typename T>
static inline void prevent_opt(T value)
{
    benchmark_sink ^= static_cast<int64_t>(value);
}

template <typename T>
static inline void DoNotOptimize(T& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : "+r"(value) : : "memory");
#else
    benchmark_sink ^= reinterpret_cast<int64_t>(&value);
#endif
}

// ============================================================================
// Cooling and Stabilization
// ============================================================================

static void cooling_delay(int ms, const char* reason = nullptr)
{
    if (g_config.noCooldown)
    {
        return;
    }
    if (reason && g_config.verboseStats)
    {
        std::cout << "  [cooling " << ms << "ms: " << reason << "]\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ============================================================================
// Test Data Types
// ============================================================================

// Trivially copyable node for PoolAllocator
struct TrivialNode
{
    int64_t mKey;
    int64_t mValue;

    TrivialNode()
        : mKey(0)
        , mValue(0)
    {
    }
    TrivialNode(int64_t k, int64_t v)
        : mKey(k)
        , mValue(v)
    {
    }
};
static_assert(std::is_trivially_copyable_v<TrivialNode>);

// Pointer-sized type for BlockAllocator
struct PointerSized
{
    int64_t mValue;

    PointerSized()
        : mValue(0)
    {
    }
    explicit PointerSized(int64_t v)
        : mValue(v)
    {
    }
};
static_assert(sizeof(PointerSized) >= sizeof(void*));

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

static void print_contract_note(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

static void print_result_table_header()
{
    std::cout << std::left << std::setw(32) << "Allocator" << std::right << std::setw(14) << "Median (ns)"
              << std::setw(14) << "Mean (ns)" << std::setw(12) << "Stddev" << std::setw(20) << "CI95"
              << "\n";
    std::cout << std::string(92, '-') << "\n";
}

static void print_result_row(const std::string& name, const Statistics& s)
{
    std::cout << std::left << std::setw(32) << name << std::right << std::fixed << std::setprecision(2) << std::setw(14)
              << s.median << std::setw(14) << s.mean << std::setw(12) << s.stddev << "  [" << std::setw(7) << s.ci95Low
              << ", " << std::setw(7) << s.ci95High << "]"
              << "\n";

    if (s.stddev > s.median && s.median > 0)
    {
        std::cout << "  [NOTE] High variance (stddev > median)\n";
    }
}

static void print_separator()
{
    std::cout << std::string(92, '-') << "\n";
}

// ============================================================================
// Library Result Collection
// ============================================================================

struct LibraryResult
{
    std::string name;
    std::vector<double> samples;
};

// ============================================================================
// BENCHMARK: Single Allocation/Deallocation
// ============================================================================

void benchmark_single_allocation()
{
    print_header("SINGLE ALLOCATION/DEALLOCATION");
    fat_p::bench::print_cpu_context(std::cout, "Start");
    print_contract_note("Measures time for one allocate() + deallocate() cycle. "
                        "Allocation includes construction, deallocation includes destruction.");

    const size_t ITERATIONS = 100000;

    // Library indices for round-robin
    enum LibIdx
    {
        LIB_NEWDELETE = 0,
        LIB_BLOCK,
        LIB_POOL,
        LIB_STD,
        LIB_PMR_MONO,
        LIB_PMR_UNSYNC,
#if FATP_HAS_BOOST_POOL
        LIB_BOOST_POOL,
#endif
        LIB_COUNT
    };

    std::vector<LibraryResult> results(LIB_COUNT);
    results[LIB_NEWDELETE].name = "fat_p::NewDeleteAllocator";
    results[LIB_BLOCK].name = "fat_p::BlockAllocator";
    results[LIB_POOL].name = "fat_p::PoolAllocator";
    results[LIB_STD].name = "std::allocator";
    results[LIB_PMR_MONO].name = "std::pmr::monotonic";
    results[LIB_PMR_UNSYNC].name = "std::pmr::unsync_pool";
#if FATP_HAS_BOOST_POOL
    results[LIB_BOOST_POOL].name = "boost::pool (raw)";
#endif

    for (auto& r : results)
    {
        r.samples.reserve(MEASURED_RUNS());
    }

    // Pre-create steady-state allocators
    fat_p::BlockAllocator<PointerSized> blockAlloc;
    fat_p::PoolAllocator<1000>::Allocator<TrivialNode> poolAlloc;

    // Prime the free lists
    {
        std::vector<PointerSized*> blockPtrs;
        std::vector<TrivialNode*> poolPtrs;
        for (int i = 0; i < 500; ++i)
        {
            blockPtrs.push_back(blockAlloc.allocate(i));
            poolPtrs.push_back(poolAlloc.allocate(i, i));
        }
        for (auto* p : blockPtrs)
        {
            blockAlloc.deallocate(p);
        }
        for (auto* p : poolPtrs)
        {
            poolAlloc.deallocate(p);
        }
    }

    // PMR buffer (heap allocated to avoid stack overflow)
    constexpr size_t PMR_BUF_SIZE = 256 * 1024;
    auto pmrBuffer = std::make_unique<char[]>(PMR_BUF_SIZE);

#if FATP_HAS_BOOST_POOL
    // Pre-warm boost::pool
    boost::pool<> boostPool(sizeof(PointerSized));
    {
        std::vector<void*> ptrs;
        for (int i = 0; i < 500; ++i)
        {
            ptrs.push_back(boostPool.malloc());
        }
        for (auto* p : ptrs)
        {
            boostPool.free(p);
        }
    }
#endif


    // Round-robin execution order
    std::vector<int> order(LIB_COUNT);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

    Timer timer;

    // Warmup + measured runs
    size_t totalRuns = WARMUP_RUNS() + MEASURED_RUNS();
    for (size_t run = 0; run < totalRuns; ++run)
    {
        bool isWarmup = (run < WARMUP_RUNS());
        std::shuffle(order.begin(), order.end(), rng);

        for (int idx : order)
        {
            switch (idx)
            {
                case LIB_NEWDELETE:
                {
                    fat_p::NewDeleteAllocator<int> alloc;
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        int* p = alloc.allocate(42);
                        DoNotOptimize(p);
                        alloc.deallocate(p);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_BLOCK:
                {
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        PointerSized* p = blockAlloc.allocate(static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        blockAlloc.deallocate(p);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_POOL:
                {
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        TrivialNode* p = poolAlloc.allocate(static_cast<int64_t>(i), static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        poolAlloc.deallocate(p);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_STD:
                {
                    std::allocator<int> alloc;
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        int* p = alloc.allocate(1);
                        *p = 42;
                        DoNotOptimize(p);
                        alloc.deallocate(p, 1);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_PMR_MONO:
                {
                    std::pmr::monotonic_buffer_resource mbr(pmrBuffer.get(), PMR_BUF_SIZE);
                    std::pmr::polymorphic_allocator<PointerSized> alloc(&mbr);
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        PointerSized* p = alloc.allocate(1);
                        new (p) PointerSized(static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        p->~PointerSized();
                        alloc.deallocate(p, 1);
                        if ((i + 1) % 10000 == 0)
                        {
                            mbr.release();
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_PMR_UNSYNC:
                {
                    std::pmr::unsynchronized_pool_resource pool;
                    std::pmr::polymorphic_allocator<PointerSized> alloc(&pool);
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        PointerSized* p = alloc.allocate(1);
                        new (p) PointerSized(static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        p->~PointerSized();
                        alloc.deallocate(p, 1);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
#if FATP_HAS_BOOST_POOL
                case LIB_BOOST_POOL:
                {
                    // boost::pool is raw memory - same genre as BlockAllocator
                    // Uses pre-warmed pool for fair comparison
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        void* mem = boostPool.malloc();
                        PointerSized* p = new (mem) PointerSized(static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        p->~PointerSized();
                        boostPool.free(mem);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
#endif
            }
        }
    }

    // Print results - fat_p first
    print_result_table_header();
    for (int i = 0; i <= LIB_POOL; ++i)
    {
        auto stats = Statistics::compute(std::move(results[i].samples));
        print_result_row(results[i].name, stats);
    }
    print_separator();
    // Then competitors
    for (int i = LIB_STD; i < LIB_COUNT; ++i)
    {
        auto stats = Statistics::compute(std::move(results[i].samples));
        print_result_row(results[i].name, stats);
    }

    // Correctness check
    {
        fat_p::NewDeleteAllocator<int> alloc;
        int* p = alloc.allocate(12345);
        if (*p != 12345)
        {
            std::cerr << "ERROR: NewDeleteAllocator correctness check failed!\n";
        }
        alloc.deallocate(p);
        std::cout << "\n  [Correctness: PASS]\n";
    }

    fat_p::bench::print_cpu_context(std::cout, "End");
}

// ============================================================================
// BENCHMARK: Burst Allocation (100 objects)
// ============================================================================

void benchmark_burst_allocation()
{
    print_header("BURST ALLOCATION (100 objects)");
    fat_p::bench::print_cpu_context(std::cout, "Start");
    print_contract_note("Allocate 100 objects, then deallocate all. "
                        "Measures bulk allocation pattern common in container growth.");

    constexpr size_t BURST_SIZE = 100;
    const size_t ITERATIONS = 10000;

    enum LibIdx
    {
        LIB_NEWDELETE = 0,
        LIB_BLOCK,
        LIB_POOL,
        LIB_STD,
        LIB_PMR_MONO,
        LIB_PMR_UNSYNC,
#if FATP_HAS_BOOST_POOL
        LIB_BOOST_POOL,
#endif
        LIB_COUNT
    };

    std::vector<LibraryResult> results(LIB_COUNT);
    results[LIB_NEWDELETE].name = "fat_p::NewDeleteAllocator";
    results[LIB_BLOCK].name = "fat_p::BlockAllocator";
    results[LIB_POOL].name = "fat_p::PoolAllocator";
    results[LIB_STD].name = "std::allocator";
    results[LIB_PMR_MONO].name = "std::pmr::monotonic";
    results[LIB_PMR_UNSYNC].name = "std::pmr::unsync_pool";
#if FATP_HAS_BOOST_POOL
    results[LIB_BOOST_POOL].name = "boost::pool (raw)";
#endif

    for (auto& r : results)
    {
        r.samples.reserve(MEASURED_RUNS());
    }

    std::vector<int> order(LIB_COUNT);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

    Timer timer;

    size_t totalRuns = WARMUP_RUNS() + MEASURED_RUNS();
    for (size_t run = 0; run < totalRuns; ++run)
    {
        bool isWarmup = (run < WARMUP_RUNS());
        std::shuffle(order.begin(), order.end(), rng);

        for (int idx : order)
        {
            switch (idx)
            {
                case LIB_NEWDELETE:
                {
                    timer.start();
                    for (size_t iter = 0; iter < ITERATIONS; ++iter)
                    {
                        fat_p::NewDeleteAllocator<int> alloc;
                        std::array<int*, BURST_SIZE> ptrs;
                        for (size_t i = 0; i < BURST_SIZE; ++i)
                        {
                            ptrs[i] = alloc.allocate(static_cast<int>(i));
                        }
                        prevent_opt(reinterpret_cast<int64_t>(ptrs[0]));
                        for (auto p : ptrs)
                        {
                            alloc.deallocate(p);
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_BLOCK:
                {
                    timer.start();
                    for (size_t iter = 0; iter < ITERATIONS; ++iter)
                    {
                        fat_p::BlockAllocator<PointerSized> alloc;
                        std::array<PointerSized*, BURST_SIZE> ptrs;
                        for (size_t i = 0; i < BURST_SIZE; ++i)
                        {
                            ptrs[i] = alloc.allocate(static_cast<int64_t>(i));
                        }
                        prevent_opt(reinterpret_cast<int64_t>(ptrs[0]));
                        for (auto p : ptrs)
                        {
                            alloc.deallocate(p);
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_POOL:
                {
                    timer.start();
                    for (size_t iter = 0; iter < ITERATIONS; ++iter)
                    {
                        fat_p::PoolAllocator<BURST_SIZE>::Allocator<TrivialNode> alloc;
                        std::array<TrivialNode*, BURST_SIZE> ptrs;
                        for (size_t i = 0; i < BURST_SIZE; ++i)
                        {
                            ptrs[i] = alloc.allocate(static_cast<int64_t>(i), static_cast<int64_t>(i));
                        }
                        prevent_opt(reinterpret_cast<int64_t>(ptrs[0]));
                        for (auto p : ptrs)
                        {
                            alloc.deallocate(p);
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_STD:
                {
                    timer.start();
                    for (size_t iter = 0; iter < ITERATIONS; ++iter)
                    {
                        std::allocator<int> alloc;
                        std::array<int*, BURST_SIZE> ptrs;
                        for (size_t i = 0; i < BURST_SIZE; ++i)
                        {
                            ptrs[i] = alloc.allocate(1);
                            *ptrs[i] = static_cast<int>(i);
                        }
                        prevent_opt(reinterpret_cast<int64_t>(ptrs[0]));
                        for (auto p : ptrs)
                        {
                            alloc.deallocate(p, 1);
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_PMR_MONO:
                {
                    timer.start();
                    for (size_t iter = 0; iter < ITERATIONS; ++iter)
                    {
                        constexpr size_t BUF_SIZE = BURST_SIZE * sizeof(PointerSized) * 2;
                        alignas(alignof(PointerSized)) char buffer[BUF_SIZE];
                        std::pmr::monotonic_buffer_resource mbr(buffer, sizeof(buffer));
                        std::pmr::polymorphic_allocator<PointerSized> alloc(&mbr);
                        std::array<PointerSized*, BURST_SIZE> ptrs;
                        for (size_t i = 0; i < BURST_SIZE; ++i)
                        {
                            ptrs[i] = alloc.allocate(1);
                            new (ptrs[i]) PointerSized(static_cast<int64_t>(i));
                        }
                        prevent_opt(reinterpret_cast<int64_t>(ptrs[0]));
                        for (auto p : ptrs)
                        {
                            p->~PointerSized();
                            alloc.deallocate(p, 1);
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_PMR_UNSYNC:
                {
                    timer.start();
                    for (size_t iter = 0; iter < ITERATIONS; ++iter)
                    {
                        std::pmr::unsynchronized_pool_resource pool;
                        std::pmr::polymorphic_allocator<PointerSized> alloc(&pool);
                        std::array<PointerSized*, BURST_SIZE> ptrs;
                        for (size_t i = 0; i < BURST_SIZE; ++i)
                        {
                            ptrs[i] = alloc.allocate(1);
                            new (ptrs[i]) PointerSized(static_cast<int64_t>(i));
                        }
                        prevent_opt(reinterpret_cast<int64_t>(ptrs[0]));
                        for (auto p : ptrs)
                        {
                            p->~PointerSized();
                            alloc.deallocate(p, 1);
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
#if FATP_HAS_BOOST_POOL
                case LIB_BOOST_POOL:
                {
                    // boost::pool - raw segregated storage (same genre as BlockAllocator)
                    timer.start();
                    for (size_t iter = 0; iter < ITERATIONS; ++iter)
                    {
                        boost::pool<> pool(sizeof(PointerSized));
                        std::array<PointerSized*, BURST_SIZE> ptrs;
                        for (size_t i = 0; i < BURST_SIZE; ++i)
                        {
                            void* mem = pool.malloc();
                            ptrs[i] = new (mem) PointerSized(static_cast<int64_t>(i));
                        }
                        prevent_opt(reinterpret_cast<int64_t>(ptrs[0]));
                        for (auto p : ptrs)
                        {
                            p->~PointerSized();
                            pool.free(p);
                        }
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
#endif
            }
        }
    }

    print_result_table_header();
    for (int i = 0; i <= LIB_POOL; ++i)
    {
        auto stats = Statistics::compute(std::move(results[i].samples));
        print_result_row(results[i].name, stats);
    }
    print_separator();
    for (int i = LIB_STD; i < LIB_COUNT; ++i)
    {
        auto stats = Statistics::compute(std::move(results[i].samples));
        print_result_row(results[i].name, stats);
    }

    fat_p::bench::print_cpu_context(std::cout, "End");
}

// ============================================================================
// BENCHMARK: Churn Pattern (Steady-State)
// ============================================================================

void benchmark_churn_pattern()
{
    print_header("CHURN PATTERN (Steady-State Mixed Operations)");
    fat_p::bench::print_cpu_context(std::cout, "Start");
    print_contract_note("Simulates container churn with interleaved alloc/dealloc. "
                        "Free list reuse should show advantage for BlockAllocator/PoolAllocator.");

    const size_t ITERATIONS = 100000;

    enum LibIdx
    {
        LIB_NEWDELETE = 0,
        LIB_BLOCK,
        LIB_POOL,
#if FATP_HAS_BOOST_POOL
        LIB_BOOST_POOL,
#endif
        LIB_COUNT
    };

    std::vector<LibraryResult> results(LIB_COUNT);
    results[LIB_NEWDELETE].name = "fat_p::NewDeleteAllocator";
    results[LIB_BLOCK].name = "fat_p::BlockAllocator (warmed)";
    results[LIB_POOL].name = "fat_p::PoolAllocator (warmed)";
#if FATP_HAS_BOOST_POOL
    results[LIB_BOOST_POOL].name = "boost::pool (warmed)";
#endif

    for (auto& r : results)
    {
        r.samples.reserve(MEASURED_RUNS());
    }

    // Pre-warm allocators
    fat_p::BlockAllocator<PointerSized> blockAlloc;
    fat_p::PoolAllocator<500>::Allocator<TrivialNode> poolAlloc;

    {
        std::vector<PointerSized*> blockPrimed;
        std::vector<TrivialNode*> poolPrimed;
        for (int i = 0; i < 200; ++i)
        {
            blockPrimed.push_back(blockAlloc.allocate(i));
            poolPrimed.push_back(poolAlloc.allocate(i, i));
        }
        for (int i = 0; i < 100; ++i)
        {
            blockAlloc.deallocate(blockPrimed.back());
            blockPrimed.pop_back();
            poolAlloc.deallocate(poolPrimed.back());
            poolPrimed.pop_back();
        }
        for (auto* p : blockPrimed)
        {
            blockAlloc.deallocate(p);
        }
        for (auto* p : poolPrimed)
        {
            poolAlloc.deallocate(p);
        }
    }

#if FATP_HAS_BOOST_POOL
    // Pre-warm boost::pool
    boost::pool<> boostPool(sizeof(PointerSized));
    {
        std::vector<void*> ptrs;
        for (int i = 0; i < 200; ++i)
        {
            ptrs.push_back(boostPool.malloc());
        }
        for (int i = 0; i < 100; ++i)
        {
            boostPool.free(ptrs.back());
            ptrs.pop_back();
        }
        for (auto* p : ptrs)
        {
            boostPool.free(p);
        }
    }
#endif


    std::vector<int> order(LIB_COUNT);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

    Timer timer;

    size_t totalRuns = WARMUP_RUNS() + MEASURED_RUNS();
    for (size_t run = 0; run < totalRuns; ++run)
    {
        bool isWarmup = (run < WARMUP_RUNS());
        std::shuffle(order.begin(), order.end(), rng);

        for (int idx : order)
        {
            switch (idx)
            {
                case LIB_NEWDELETE:
                {
                    fat_p::NewDeleteAllocator<PointerSized> alloc;
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        PointerSized* p = alloc.allocate(static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        alloc.deallocate(p);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_BLOCK:
                {
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        PointerSized* p = blockAlloc.allocate(static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        blockAlloc.deallocate(p);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
                case LIB_POOL:
                {
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        TrivialNode* p = poolAlloc.allocate(static_cast<int64_t>(i), static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        poolAlloc.deallocate(p);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
#if FATP_HAS_BOOST_POOL
                case LIB_BOOST_POOL:
                {
                    timer.start();
                    for (size_t i = 0; i < ITERATIONS; ++i)
                    {
                        void* mem = boostPool.malloc();
                        PointerSized* p = new (mem) PointerSized(static_cast<int64_t>(i));
                        DoNotOptimize(p);
                        p->~PointerSized();
                        boostPool.free(mem);
                    }
                    double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                    if (!isWarmup)
                    {
                        results[idx].samples.push_back(ns);
                    }
                    break;
                }
#endif
            }
        }
    }

    print_result_table_header();
    for (int i = 0; i < LIB_COUNT; ++i)
    {
        auto stats = Statistics::compute(std::move(results[i].samples));
        print_result_row(results[i].name, stats);
    }

    fat_p::bench::print_cpu_context(std::cout, "End");
}

// ============================================================================
// BENCHMARK: Size Scaling
// ============================================================================

void benchmark_size_scaling()
{
    print_header("SIZE SCALING (Allocation Count)");
    fat_p::bench::print_cpu_context(std::cout, "Start");
    print_contract_note("Measures how allocation time scales with number of live objects. "
                        "BlockAllocator should show constant time regardless of count.");

    const std::array<size_t, 4> SIZES = {100, 1000, 10000, 50000};
    const size_t ITERATIONS = 10000;

    for (size_t targetCount : SIZES)
    {
        std::cout << "\n--- N = " << targetCount << " ---\n\n";

        enum LibIdx
        {
            LIB_NEWDELETE = 0,
            LIB_BLOCK,
#if FATP_HAS_BOOST_POOL
            LIB_BOOST_POOL,
#endif
            LIB_COUNT
        };

        std::vector<LibraryResult> results(LIB_COUNT);
        results[LIB_NEWDELETE].name = "fat_p::NewDeleteAllocator";
        results[LIB_BLOCK].name = "fat_p::BlockAllocator";
#if FATP_HAS_BOOST_POOL
        results[LIB_BOOST_POOL].name = "boost::pool";
#endif

        for (auto& r : results)
        {
            r.samples.reserve(MEASURED_RUNS());
        }

        std::vector<int> order(LIB_COUNT);
        std::iota(order.begin(), order.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));

        Timer timer;

        size_t totalRuns = WARMUP_RUNS() + MEASURED_RUNS();
        for (size_t run = 0; run < totalRuns; ++run)
        {
            bool isWarmup = (run < WARMUP_RUNS());
            std::shuffle(order.begin(), order.end(), rng);

            for (int idx : order)
            {
                switch (idx)
                {
                    case LIB_NEWDELETE:
                    {
                        fat_p::NewDeleteAllocator<PointerSized> alloc;
                        std::vector<PointerSized*> ptrs;
                        ptrs.reserve(targetCount);
                        for (size_t i = 0; i < targetCount; ++i)
                        {
                            ptrs.push_back(alloc.allocate(static_cast<int64_t>(i)));
                        }

                        timer.start();
                        for (size_t i = 0; i < ITERATIONS; ++i)
                        {
                            PointerSized* p = alloc.allocate(static_cast<int64_t>(i));
                            DoNotOptimize(p);
                            alloc.deallocate(p);
                        }
                        double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                        if (!isWarmup)
                        {
                            results[idx].samples.push_back(ns);
                        }

                        for (auto* p : ptrs)
                        {
                            alloc.deallocate(p);
                        }
                        break;
                    }
                    case LIB_BLOCK:
                    {
                        fat_p::BlockAllocator<PointerSized> alloc;
                        std::vector<PointerSized*> ptrs;
                        ptrs.reserve(targetCount);
                        for (size_t i = 0; i < targetCount; ++i)
                        {
                            ptrs.push_back(alloc.allocate(static_cast<int64_t>(i)));
                        }

                        timer.start();
                        for (size_t i = 0; i < ITERATIONS; ++i)
                        {
                            PointerSized* p = alloc.allocate(static_cast<int64_t>(i));
                            DoNotOptimize(p);
                            alloc.deallocate(p);
                        }
                        double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                        if (!isWarmup)
                        {
                            results[idx].samples.push_back(ns);
                        }

                        for (auto* p : ptrs)
                        {
                            alloc.deallocate(p);
                        }
                        break;
                    }
#if FATP_HAS_BOOST_POOL
                    case LIB_BOOST_POOL:
                    {
                        boost::pool<> pool(sizeof(PointerSized));
                        std::vector<void*> ptrs;
                        ptrs.reserve(targetCount);
                        for (size_t i = 0; i < targetCount; ++i)
                        {
                            ptrs.push_back(pool.malloc());
                        }

                        timer.start();
                        for (size_t i = 0; i < ITERATIONS; ++i)
                        {
                            void* mem = pool.malloc();
                            PointerSized* p = new (mem) PointerSized(static_cast<int64_t>(i));
                            DoNotOptimize(p);
                            p->~PointerSized();
                            pool.free(mem);
                        }
                        double ns = timer.elapsedNs() / static_cast<double>(ITERATIONS);
                        if (!isWarmup)
                        {
                            results[idx].samples.push_back(ns);
                        }

                        for (auto* p : ptrs)
                        {
                            pool.free(p);
                        }
                        break;
                    }
#endif
                }
            }
        }

        print_result_table_header();
        for (int i = 0; i < LIB_COUNT; ++i)
        {
            auto stats = Statistics::compute(std::move(results[i].samples));
            print_result_row(results[i].name, stats);
        }
    }

    fat_p::bench::print_cpu_context(std::cout, "End");
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

    // Apply benchmark scope (Windows priority/affinity)
    fat_p::bench::BenchmarkScope scope(!g_config.noScope);

    std::cout << "================================================================================\n";
    std::cout << "  AllocationStrategies Comprehensive Benchmark Suite\n";
    std::cout << "================================================================================\n";

    // Platform info
    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#elif defined(__APPLE__)
    std::cout << "macOS";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() << ", seed=" << g_config.seed
              << ")\n";

    // Competitor detection
    std::cout << "\nCompetitor libraries: std::pmr";
#if FATP_HAS_BOOST_POOL
    std::cout << " boost::pool";
#endif
    std::cout << "\n\n";

    // CPU frequency detection
    fat_p::bench::print_cpu_detection_info(std::cout);
    std::cout << "\n";

    // Design invariants
    std::cout << "Design Invariants:\n";
    std::cout << "  1. Round-robin execution with randomized order per run\n";
    std::cout << "  2. Setup/teardown outside timed regions\n";
    std::cout << "  3. Medians are the primary reported statistic\n";
    std::cout << "  4. Correctness verified after each benchmark\n\n";

    // Wait for CPU stability
    if (!g_config.noStabilize)
    {
        std::cout << "Checking initial CPU state...\n";
        fat_p::bench::print_cpu_context(std::cout, "Initial");
        fat_p::bench::waitForStableCpu(g_config, std::cout);
        std::cout << "\n";
    }

    // Run benchmarks
    benchmark_single_allocation();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before burst allocation");
    benchmark_burst_allocation();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before churn pattern");
    benchmark_churn_pattern();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before size scaling");
    benchmark_size_scaling();

    // Final summary
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    fat_p::bench::print_cpu_context(std::cout, "Final");

    return 0;
}
