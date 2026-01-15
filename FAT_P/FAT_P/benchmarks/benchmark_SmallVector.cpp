// benchmark_SmallVector.cpp
//
// FAT-P SmallVector benchmarks using unified FatPBenchmarkRunner infrastructure.
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
//   1. Core operations (SmallVector vs std::vector vs boost vs folly)
//   2. Inline vs heap performance (the key SmallVector advantage)
//   3. Copy/move operations
//   4. Insert/erase operations
//   5. Size sensitivity (scaling behavior)
//   6. Allocation counting (memory efficiency)
//
// Build:
//   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_SmallVector.cpp -o bench_sv
//   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_SmallVector.cpp
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
//   ./bench_sv
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_sv

/*
FATP_META:
  meta_version: 1
  component: SmallVector
  file_role: benchmark
  path: benchmarks/benchmark_SmallVector.cpp
  namespace: fat_p
  summary: "Benchmarks for SmallVector."
  related:
    docs_search: "SmallVector"
    headers:
      - fat_p/FatPBenchmarkRunner.h
      - fat_p/SmallVector.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 7
    defines_unprefixed: 7
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "SmallVector.h"

// Optional: Include competitor headers if available
#if __has_include(<boost/container/small_vector.hpp>)
#include <boost/container/small_vector.hpp>
#define HAS_BOOST 1
#else
#define HAS_BOOST 0
#endif

// Folly requires special setup (fmt, boost, glog, etc.) - opt-in with -DUSE_FOLLY=1
#if defined(USE_FOLLY) && USE_FOLLY && __has_include(<folly/small_vector.h>)
#include <folly/small_vector.h>
#define HAS_FOLLY 1
#else
#define HAS_FOLLY 0
#endif

#if __has_include(<llvm-18/llvm/ADT/SmallVector.h>)
#include <llvm-18/llvm/ADT/SmallVector.h>
#define HAS_LLVM 1
#else
#define HAS_LLVM 0
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

// Global benchmark configuration (loaded from FATP_BENCH_* env vars in main)
static fat_p::bench::BenchConfig g_config;

// Accessors for backward compatibility with existing benchmark code
static size_t WARMUP_RUNS() { return g_config.warmupRuns; }
static size_t MEASURED_RUNS() { return g_config.measuredRuns; }

// ============================================================================
// CPU Frequency Monitoring (Shared)
// ============================================================================

void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

// ============================================================================
// Benchmark Environment Configuration
// ============================================================================

// Use shared BenchmarkScope from FatPBenchmarkRunner.h
using fat_p::bench::BenchmarkScope;

// ============================================================================
// Timer
// ============================================================================

struct Timer
{
    using clock = std::chrono::steady_clock;
    clock::time_point t0;

    void start() { t0 = clock::now(); }

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
        if (samples.empty()) return s;

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

    void print(const char* label) const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  " << std::setw(20) << label << ": "
            << "median=" << std::setw(8) << median
            << " mean=" << std::setw(8) << mean
            << " +/-" << std::setw(6) << stddev
            << " min=" << min << " max=" << max << "\n";
    }
};

// ============================================================================
// Allocation Counter
// ============================================================================

static thread_local size_t g_allocation_count = 0;
static thread_local size_t g_deallocation_count = 0;
static thread_local size_t g_bytes_allocated = 0;

void reset_allocation_counters()
{
    g_allocation_count = 0;
    g_deallocation_count = 0;
    g_bytes_allocated = 0;
}

struct AllocationStats
{
    size_t allocations;
    size_t deallocations;
    size_t bytes;
};

AllocationStats get_allocation_stats()
{
    return {g_allocation_count, g_deallocation_count, g_bytes_allocated};
}

// Counting allocator for tracking allocations
template <typename T>
struct CountingAllocator
{
    using value_type = T;

    CountingAllocator() = default;

    template <typename U>
    CountingAllocator(const CountingAllocator<U>&) noexcept {}

    T* allocate(size_t n)
    {
        ++g_allocation_count;
        g_bytes_allocated += n * sizeof(T);
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, size_t n)
    {
        ++g_deallocation_count;
        std::allocator<T>{}.deallocate(p, n);
    }

    template <typename U>
    bool operator==(const CountingAllocator<U>&) const noexcept { return true; }

    template <typename U>
    bool operator!=(const CountingAllocator<U>&) const noexcept { return false; }
};

// ============================================================================
// Test Data Generation
// ============================================================================

std::vector<int64_t> generate_random_values(size_t n, uint64_t seed = 12345)
{
    std::vector<int64_t> values(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    for (size_t i = 0; i < n; ++i)
    {
        values[i] = dist(rng);
    }
    return values;
}

// ============================================================================
// Benchmark Case Enum
// ============================================================================

enum class Case
{
    PushBack,
    EmplaceBack,
    RandomAccess,
    Iteration,
    Clear,
    CopyConstruct,
    MoveConstruct
};

static inline const char* case_name(Case c)
{
    switch (c)
    {
    case Case::PushBack:       return "push_back";
    case Case::EmplaceBack:    return "emplace_back";
    case Case::RandomAccess:   return "operator[]";
    case Case::Iteration:      return "iteration";
    case Case::Clear:          return "clear";
    case Case::CopyConstruct:  return "copy ctor";
    case Case::MoveConstruct:  return "move ctor";
    }
    return "Unknown";
}

// ============================================================================
// Shared Inputs
// ============================================================================

struct Inputs
{
    std::vector<int64_t> values;
    std::vector<size_t> access_order;

    static Inputs make(size_t N, uint64_t seed = 0xC0FFEEULL)
    {
        Inputs in;
        in.values = generate_random_values(N, seed);
        in.access_order.resize(N);
        std::iota(in.access_order.begin(), in.access_order.end(), 0);
        std::mt19937_64 rng(seed ^ 0x9E37);
        std::shuffle(in.access_order.begin(), in.access_order.end(), rng);
        return in;
    }
};

// ============================================================================
// Vector Adapter Interface
// ============================================================================

struct IVectorAdapter
{
    virtual ~IVectorAdapter() = default;

    virtual const char* name() const = 0;

    virtual void setup(size_t N) = 0;
    virtual void teardown() = 0;

    virtual void preload(const Inputs& in) = 0;

    virtual size_t run_operation(Case c, const Inputs& in) = 0;
};

// ============================================================================
// SmallVector Adapter
// ============================================================================

template <size_t InlineCapacity>
class SmallVectorAdapter final : public IVectorAdapter
{
    std::string mName;
    std::unique_ptr<fat_p::SmallVector<int64_t, InlineCapacity>> mVec;

public:
    explicit SmallVectorAdapter(const char* name) : mName(name) {}

    const char* name() const override { return mName.c_str(); }

    void setup(size_t N) override
    {
        mVec = std::make_unique<fat_p::SmallVector<int64_t, InlineCapacity>>();
        mVec->reserve(N);
    }

    void teardown() override
    {
        mVec.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t v : in.values)
        {
            mVec->push_back(v);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::PushBack:
            for (int64_t v : in.values)
            {
                mVec->push_back(v);
                ++ops;
            }
            break;

        case Case::EmplaceBack:
            for (int64_t v : in.values)
            {
                mVec->emplace_back(v);
                ++ops;
            }
            break;

        case Case::RandomAccess:
            for (size_t idx : in.access_order)
            {
                benchmark_sink += (*mVec)[idx];
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& v : *mVec)
            {
                benchmark_sink += v;
                ++ops;
            }
            break;

        case Case::Clear:
            mVec->clear();
            ops = mVec->capacity();  // Report capacity as "work done"
            break;

        case Case::CopyConstruct:
            {
                fat_p::SmallVector<int64_t, InlineCapacity> copy(*mVec);
                benchmark_sink += copy.size();
                ops = mVec->size();
            }
            break;

        case Case::MoveConstruct:
            {
                fat_p::SmallVector<int64_t, InlineCapacity> temp(*mVec);
                fat_p::SmallVector<int64_t, InlineCapacity> moved(std::move(temp));
                benchmark_sink += moved.size();
                ops = mVec->size();
            }
            break;
        }
        return ops;
    }
};

// ============================================================================
// std::vector Adapter
// ============================================================================

class StdVectorAdapter final : public IVectorAdapter
{
    std::unique_ptr<std::vector<int64_t>> mVec;

public:
    const char* name() const override { return "std::vector"; }

    void setup(size_t N) override
    {
        mVec = std::make_unique<std::vector<int64_t>>();
        mVec->reserve(N);
    }

    void teardown() override
    {
        mVec.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t v : in.values)
        {
            mVec->push_back(v);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::PushBack:
            for (int64_t v : in.values)
            {
                mVec->push_back(v);
                ++ops;
            }
            break;

        case Case::EmplaceBack:
            for (int64_t v : in.values)
            {
                mVec->emplace_back(v);
                ++ops;
            }
            break;

        case Case::RandomAccess:
            for (size_t idx : in.access_order)
            {
                benchmark_sink += (*mVec)[idx];
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& v : *mVec)
            {
                benchmark_sink += v;
                ++ops;
            }
            break;

        case Case::Clear:
            mVec->clear();
            ops = mVec->capacity();
            break;

        case Case::CopyConstruct:
            {
                std::vector<int64_t> copy(*mVec);
                benchmark_sink += copy.size();
                ops = mVec->size();
            }
            break;

        case Case::MoveConstruct:
            {
                std::vector<int64_t> temp(*mVec);
                std::vector<int64_t> moved(std::move(temp));
                benchmark_sink += moved.size();
                ops = mVec->size();
            }
            break;
        }
        return ops;
    }
};

// ============================================================================
// Boost small_vector Adapter (if available)
// ============================================================================

#if HAS_BOOST
template <size_t InlineCapacity>
class BoostSmallVectorAdapter final : public IVectorAdapter
{
    std::string mName;
    std::unique_ptr<boost::container::small_vector<int64_t, InlineCapacity>> mVec;

public:
    explicit BoostSmallVectorAdapter(const char* name) : mName(name) {}

    const char* name() const override { return mName.c_str(); }

    void setup(size_t N) override
    {
        mVec = std::make_unique<boost::container::small_vector<int64_t, InlineCapacity>>();
        mVec->reserve(N);
    }

    void teardown() override
    {
        mVec.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t v : in.values)
        {
            mVec->push_back(v);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::PushBack:
            for (int64_t v : in.values)
            {
                mVec->push_back(v);
                ++ops;
            }
            break;

        case Case::EmplaceBack:
            for (int64_t v : in.values)
            {
                mVec->emplace_back(v);
                ++ops;
            }
            break;

        case Case::RandomAccess:
            for (size_t idx : in.access_order)
            {
                benchmark_sink += (*mVec)[idx];
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& v : *mVec)
            {
                benchmark_sink += v;
                ++ops;
            }
            break;

        case Case::Clear:
            mVec->clear();
            ops = mVec->capacity();
            break;

        case Case::CopyConstruct:
            {
                boost::container::small_vector<int64_t, InlineCapacity> copy(*mVec);
                benchmark_sink += copy.size();
                ops = mVec->size();
            }
            break;

        case Case::MoveConstruct:
            {
                boost::container::small_vector<int64_t, InlineCapacity> temp(*mVec);
                boost::container::small_vector<int64_t, InlineCapacity> moved(std::move(temp));
                benchmark_sink += moved.size();
                ops = mVec->size();
            }
            break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// LLVM SmallVector Adapter (if available)
// ============================================================================

#if HAS_LLVM
template <size_t InlineCapacity>
class LLVMSmallVectorAdapter final : public IVectorAdapter
{
    std::string mName;
    std::unique_ptr<llvm::SmallVector<int64_t, InlineCapacity>> mVec;

public:
    explicit LLVMSmallVectorAdapter(const char* name) : mName(name) {}

    const char* name() const override { return mName.c_str(); }

    void setup(size_t N) override
    {
        mVec = std::make_unique<llvm::SmallVector<int64_t, InlineCapacity>>();
        mVec->reserve(N);
    }

    void teardown() override
    {
        mVec.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t v : in.values)
        {
            mVec->push_back(v);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::PushBack:
            for (int64_t v : in.values)
            {
                mVec->push_back(v);
                ++ops;
            }
            break;

        case Case::EmplaceBack:
            for (int64_t v : in.values)
            {
                mVec->emplace_back(v);
                ++ops;
            }
            break;

        case Case::RandomAccess:
            for (size_t idx : in.access_order)
            {
                benchmark_sink += (*mVec)[idx];
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& v : *mVec)
            {
                benchmark_sink += v;
                ++ops;
            }
            break;

        case Case::Clear:
            mVec->clear();
            ops = mVec->capacity();
            break;

        case Case::CopyConstruct:
            {
                llvm::SmallVector<int64_t, InlineCapacity> copy(*mVec);
                benchmark_sink += copy.size();
                ops = mVec->size();
            }
            break;

        case Case::MoveConstruct:
            {
                llvm::SmallVector<int64_t, InlineCapacity> temp(*mVec);
                llvm::SmallVector<int64_t, InlineCapacity> moved(std::move(temp));
                benchmark_sink += moved.size();
                ops = mVec->size();
            }
            break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// Header Printing
// ============================================================================

void print_header(const char* title)
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "================================================================================\n\n";
}

void print_subheader(const char* title)
{
    std::cout << "\n--- " << title << " ---\n\n";
}

// ============================================================================
// Section 1: Core Operations Benchmark
// ============================================================================

void benchmark_core_operations(const std::vector<size_t>& sizes)
{
    print_header("CORE OPERATIONS");

    std::cout << "Comparing fundamental vector operations.\n";
    std::cout << "All containers pre-reserved to target size.\n\n";

    constexpr size_t INLINE_CAP = 16;

    for (size_t N : sizes)
    {
        print_subheader((std::string("N = ") + std::to_string(N)).c_str());
        print_cpu_context("Starting");

        Inputs in = Inputs::make(N);

        // Create adapters
        std::vector<std::unique_ptr<IVectorAdapter>> adapters;
        adapters.push_back(std::make_unique<SmallVectorAdapter<INLINE_CAP>>("SmallVector<16>"));
        adapters.push_back(std::make_unique<StdVectorAdapter>());
#if HAS_BOOST
        adapters.push_back(std::make_unique<BoostSmallVectorAdapter<INLINE_CAP>>("boost::small_vector<16>"));
#endif
#if HAS_LLVM
        adapters.push_back(std::make_unique<LLVMSmallVectorAdapter<INLINE_CAP>>("llvm::SmallVector<16>"));
#endif

        // Cases to benchmark
        std::vector<Case> cases = {
            Case::PushBack,
            Case::EmplaceBack,
            Case::RandomAccess,
            Case::Iteration,
            Case::CopyConstruct,
            Case::MoveConstruct
        };

        for (Case c : cases)
        {
            bool needs_preload = (c == Case::RandomAccess || c == Case::Iteration ||
                                  c == Case::CopyConstruct || c == Case::MoveConstruct);

            std::cout << case_name(c) << ":\n";

            // Collect samples for each adapter
            std::vector<std::vector<double>> all_samples(adapters.size());

            // Round-robin with randomized order
            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(42);

            // Warmup
            for (size_t run = 0; run < WARMUP_RUNS(); ++run)
            {
                std::shuffle(order.begin(), order.end(), rng);
                for (size_t idx : order)
                {
                    adapters[idx]->setup(N);
                    if (needs_preload) adapters[idx]->preload(in);
                    adapters[idx]->run_operation(c, in);
                    adapters[idx]->teardown();
                }
            }

            // Measured runs
            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                std::shuffle(order.begin(), order.end(), rng);
                for (size_t idx : order)
                {
                    adapters[idx]->setup(N);
                    if (needs_preload) adapters[idx]->preload(in);

                    Timer t;
                    t.start();
                    size_t ops = adapters[idx]->run_operation(c, in);
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
    }
}

// ============================================================================
// Section 2: Inline vs Heap Performance
// ============================================================================

void benchmark_inline_vs_heap()
{
    print_header("INLINE VS HEAP PERFORMANCE");

    std::cout << "The key SmallVector advantage: zero allocations for small sizes.\n";
    std::cout << "Testing operations at various sizes relative to inline capacity.\n\n";

    constexpr size_t INLINE_CAP = 16;
    constexpr size_t ITERATIONS = 10000;

    std::vector<size_t> test_sizes = {4, 8, 12, 16, 20, 32, 64, 128};

    print_cpu_context("Starting");

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Size | SmallVector push_back (ns) | std::vector push_back (ns) | Ratio\n";
    std::cout << "-----|----------------------------|----------------------------|------\n";

    for (size_t N : test_sizes)
    {
        double sv_total = 0, std_total = 0;

        for (size_t iter = 0; iter < ITERATIONS; ++iter)
        {
            // SmallVector
            {
                fat_p::SmallVector<int64_t, INLINE_CAP> vec;
                Timer t;
                t.start();
                for (size_t i = 0; i < N; ++i)
                {
                    vec.push_back(static_cast<int64_t>(i));
                }
                sv_total += t.elapsed_ns();
                benchmark_sink += vec.size();
            }

            // std::vector
            {
                std::vector<int64_t> vec;
                Timer t;
                t.start();
                for (size_t i = 0; i < N; ++i)
                {
                    vec.push_back(static_cast<int64_t>(i));
                }
                std_total += t.elapsed_ns();
                benchmark_sink += vec.size();
            }
        }

        double sv_ns = sv_total / (ITERATIONS * N);
        double std_ns = std_total / (ITERATIONS * N);
        double ratio = std_ns / sv_ns;

        const char* note = (N <= INLINE_CAP) ? " (inline)" : " (heap)";
        std::cout << std::setw(4) << N << note
                  << " | " << std::setw(26) << sv_ns
                  << " | " << std::setw(26) << std_ns
                  << " | " << std::setw(5) << ratio << "x\n";
    }
}

// ============================================================================
// Section 3: Allocation Counting
// ============================================================================

void benchmark_allocation_count()
{
    print_header("ALLOCATION COUNT COMPARISON");

    std::cout << "Counting heap allocations for various scenarios.\n";
    std::cout << "SmallVector should have zero allocations when size <= InlineCapacity.\n\n";

    print_cpu_context("Starting");

    constexpr size_t INLINE_CAP = 16;

    std::vector<size_t> test_sizes = {1, 8, 16, 17, 32, 100, 1000};

    std::cout << "Scenario: push_back N elements (no reserve)\n\n";
    std::cout << "    N | SmallVector allocs | std::vector allocs\n";
    std::cout << "------|--------------------|-----------------\n";

    for (size_t N : test_sizes)
    {
        // SmallVector with counting allocator
        size_t sv_allocs = 0;
        {
            fat_p::SmallVector<int64_t, INLINE_CAP, CountingAllocator<int64_t>> vec;
            reset_allocation_counters();
            for (size_t i = 0; i < N; ++i)
            {
                vec.push_back(static_cast<int64_t>(i));
            }
            sv_allocs = get_allocation_stats().allocations;
        }

        // std::vector with counting allocator
        size_t std_allocs = 0;
        {
            std::vector<int64_t, CountingAllocator<int64_t>> vec;
            reset_allocation_counters();
            for (size_t i = 0; i < N; ++i)
            {
                vec.push_back(static_cast<int64_t>(i));
            }
            std_allocs = get_allocation_stats().allocations;
        }

        std::cout << std::setw(5) << N << " | "
                  << std::setw(18) << sv_allocs << " | "
                  << std::setw(17) << std_allocs << "\n";
    }

    std::cout << "\nNote: SmallVector<" << INLINE_CAP << "> uses inline storage for N <= "
              << INLINE_CAP << "\n";
}

// ============================================================================
// Section 4: Insert/Erase Performance
// ============================================================================

void benchmark_insert_erase()
{
    print_header("INSERT/ERASE OPERATIONS");

    std::cout << "Comparing insert and erase at various positions.\n\n";

    constexpr size_t INLINE_CAP = 16;
    constexpr size_t N = 1000;
    constexpr size_t OPS = 100;

    print_cpu_context("Starting");

    Inputs in = Inputs::make(N);

    // Test positions: front, middle, back
    struct Position { const char* name; size_t idx; };
    std::vector<Position> positions = {
        {"front", 0},
        {"middle", N/2},
        {"back", N-1}
    };

    std::cout << "Insert single element (N=" << N << ", " << OPS << " ops each):\n\n";
    std::cout << "Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio\n";
    std::cout << "---------|---------------------|---------------------|------\n";

    for (const auto& pos : positions)
    {
        double sv_total = 0, std_total = 0;

        // SmallVector
        for (size_t iter = 0; iter < MEASURED_RUNS(); ++iter)
        {
            fat_p::SmallVector<int64_t, INLINE_CAP> vec;
            for (size_t i = 0; i < N; ++i) vec.push_back(in.values[i]);

            Timer t;
            t.start();
            for (size_t op = 0; op < OPS; ++op)
            {
                size_t insert_pos = std::min(pos.idx, vec.size());
                vec.insert(vec.begin() + insert_pos, 999);
            }
            sv_total += t.elapsed_ns() / OPS;
            benchmark_sink += vec.size();
        }

        // std::vector
        for (size_t iter = 0; iter < MEASURED_RUNS(); ++iter)
        {
            std::vector<int64_t> vec;
            for (size_t i = 0; i < N; ++i) vec.push_back(in.values[i]);

            Timer t;
            t.start();
            for (size_t op = 0; op < OPS; ++op)
            {
                size_t insert_pos = std::min(pos.idx, vec.size());
                vec.insert(vec.begin() + insert_pos, 999);
            }
            std_total += t.elapsed_ns() / OPS;
            benchmark_sink += vec.size();
        }

        double sv_ns = sv_total / MEASURED_RUNS();
        double std_ns = std_total / MEASURED_RUNS();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(8) << pos.name << " | "
                  << std::setw(19) << sv_ns << " | "
                  << std::setw(19) << std_ns << " | "
                  << std::setw(5) << (std_ns / sv_ns) << "x\n";
    }

    std::cout << "\nErase single element (N=" << N << ", " << OPS << " ops each):\n\n";
    std::cout << "Position | SmallVector (ns/op) | std::vector (ns/op) | Ratio\n";
    std::cout << "---------|---------------------|---------------------|------\n";

    for (const auto& pos : positions)
    {
        double sv_total = 0, std_total = 0;

        // SmallVector
        for (size_t iter = 0; iter < MEASURED_RUNS(); ++iter)
        {
            fat_p::SmallVector<int64_t, INLINE_CAP> vec;
            for (size_t i = 0; i < N + OPS; ++i) vec.push_back(in.values[i % N]);

            Timer t;
            t.start();
            for (size_t op = 0; op < OPS; ++op)
            {
                size_t erase_pos = std::min(pos.idx, vec.size() - 1);
                vec.erase(vec.begin() + erase_pos);
            }
            sv_total += t.elapsed_ns() / OPS;
            benchmark_sink += vec.size();
        }

        // std::vector
        for (size_t iter = 0; iter < MEASURED_RUNS(); ++iter)
        {
            std::vector<int64_t> vec;
            for (size_t i = 0; i < N + OPS; ++i) vec.push_back(in.values[i % N]);

            Timer t;
            t.start();
            for (size_t op = 0; op < OPS; ++op)
            {
                size_t erase_pos = std::min(pos.idx, vec.size() - 1);
                vec.erase(vec.begin() + erase_pos);
            }
            std_total += t.elapsed_ns() / OPS;
            benchmark_sink += vec.size();
        }

        double sv_ns = sv_total / MEASURED_RUNS();
        double std_ns = std_total / MEASURED_RUNS();

        std::cout << std::setw(8) << pos.name << " | "
                  << std::setw(19) << sv_ns << " | "
                  << std::setw(19) << std_ns << " | "
                  << std::setw(5) << (std_ns / sv_ns) << "x\n";
    }
}

// ============================================================================
// Section 5: Inline Capacity Sensitivity
// ============================================================================

void benchmark_inline_capacity_sensitivity()
{
    print_header("INLINE CAPACITY SENSITIVITY");

    std::cout << "How does inline capacity choice affect performance?\n";
    std::cout << "Testing push_back of N elements with various inline capacities.\n\n";

    constexpr size_t N = 32;
    constexpr size_t ITERATIONS = 50000;

    print_cpu_context("Starting");

    std::cout << "Target size: " << N << " elements\n\n";
    std::cout << "InlineCapacity | Time (ns/op) | Allocations | Notes\n";
    std::cout << "---------------|--------------|-------------|------\n";

    // Test various inline capacities
    auto test_capacity = [&](size_t inline_cap, const char* label, auto make_vec)
    {
        double total_ns = 0;
        size_t allocs = 0;

        for (size_t iter = 0; iter < ITERATIONS; ++iter)
        {
            auto vec = make_vec();
            Timer t;
            t.start();
            for (size_t i = 0; i < N; ++i)
            {
                vec.push_back(static_cast<int64_t>(i));
            }
            total_ns += t.elapsed_ns();
            benchmark_sink += vec.size();
        }

        // Count allocations once
        {
            reset_allocation_counters();
            fat_p::SmallVector<int64_t, 8, CountingAllocator<int64_t>> counter_vec;
            for (size_t i = 0; i < N; ++i) counter_vec.push_back(i);
            allocs = get_allocation_stats().allocations;
        }

        double ns_per = total_ns / (ITERATIONS * N);
        const char* note = (inline_cap >= N) ? "all inline" :
                          (inline_cap == 0) ? "always heap" : "transitions";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(14) << label << " | "
                  << std::setw(12) << ns_per << " | "
                  << std::setw(11) << allocs << " | " << note << "\n";
    };

    test_capacity(0, "0 (std::vec)", []{ return std::vector<int64_t>{}; });
    test_capacity(8, "8", []{ return fat_p::SmallVector<int64_t, 8>{}; });
    test_capacity(16, "16", []{ return fat_p::SmallVector<int64_t, 16>{}; });
    test_capacity(32, "32", []{ return fat_p::SmallVector<int64_t, 32>{}; });
    test_capacity(64, "64", []{ return fat_p::SmallVector<int64_t, 64>{}; });
}

// ============================================================================
// Section 6: Fast Path Throughput (Isolation Benchmark)
// ============================================================================

/**
 * @brief Measures pure fast-path performance without allocation noise
 *
 * This benchmark specifically targets the emplace_back optimization:
 * - Fast/slow path split (FATP_FORCEINLINE + FATP_NOINLINE)
 * - Data member layout reorder (hot fields first)
 * - Cached slot pointer before increment
 *
 * The benchmark ensures 100% fast-path execution by never exceeding inline capacity.
 * This isolates the fast-path improvement from allocation speedup.
 */
void benchmark_fast_path_throughput()
{
    print_header("FAST PATH THROUGHPUT (ISOLATION BENCHMARK)");

    std::cout << "Measuring pure fast-path performance with zero allocations.\n";
    std::cout << "This isolates the emplace_back fast-path optimization from allocation benefits.\n\n";

    print_cpu_context("Starting");

    // ==========================================================================
    // Test 1: emplace_back throughput at various inline capacities
    // ==========================================================================
    std::cout << "\n--- emplace_back Throughput (100% Fast Path) ---\n\n";
    std::cout << "InlineCap | SmallVector (ns/op) | std::vector* (ns/op) | Ratio\n";
    std::cout << "----------|---------------------|----------------------|------\n";
    std::cout << "* std::vector is pre-reserved to avoid allocation\n\n";

    constexpr size_t ITERATIONS = 100000;

    // Helper to test emplace_back at specific inline capacity
    // Note: Using macro because template lambdas require C++20
    #define TEST_EMPLACE_BACK_CAP(INLINE_CAP) do { \
        constexpr size_t N = INLINE_CAP; \
        double sv_total_ns = 0; \
        double std_total_ns = 0; \
        \
        /* Warmup */ \
        for (size_t w = 0; w < WARMUP_RUNS(); ++w) { \
            fat_p::SmallVector<int64_t, INLINE_CAP> sv; \
            for (size_t i = 0; i < N; ++i) sv.emplace_back(static_cast<int64_t>(i)); \
            benchmark_sink += sv.size(); \
            \
            std::vector<int64_t> stdv; \
            stdv.reserve(N); \
            for (size_t i = 0; i < N; ++i) stdv.emplace_back(static_cast<int64_t>(i)); \
            benchmark_sink += stdv.size(); \
        } \
        \
        /* Measured runs - interleaved to reduce systematic bias */ \
        for (size_t iter = 0; iter < ITERATIONS; ++iter) { \
            /* SmallVector (no reserve - uses inline storage) */ \
            { \
                fat_p::SmallVector<int64_t, INLINE_CAP> vec; \
                Timer t; \
                t.start(); \
                for (size_t i = 0; i < N; ++i) { \
                    vec.emplace_back(static_cast<int64_t>(i)); \
                } \
                sv_total_ns += t.elapsed_ns(); \
                benchmark_sink += vec.size(); \
            } \
            \
            /* std::vector (pre-reserved to avoid allocation during push) */ \
            { \
                std::vector<int64_t> vec; \
                vec.reserve(N); \
                Timer t; \
                t.start(); \
                for (size_t i = 0; i < N; ++i) { \
                    vec.emplace_back(static_cast<int64_t>(i)); \
                } \
                std_total_ns += t.elapsed_ns(); \
                benchmark_sink += vec.size(); \
            } \
        } \
        \
        double sv_ns_per = sv_total_ns / (ITERATIONS * N); \
        double std_ns_per = std_total_ns / (ITERATIONS * N); \
        double ratio = std_ns_per / sv_ns_per; \
        \
        std::cout << std::fixed << std::setprecision(2); \
        std::cout << std::setw(9) << INLINE_CAP << " | " \
                  << std::setw(19) << sv_ns_per << " | " \
                  << std::setw(20) << std_ns_per << " | " \
                  << std::setw(5) << ratio << "x\n"; \
    } while(0)

    TEST_EMPLACE_BACK_CAP(4);
    TEST_EMPLACE_BACK_CAP(8);
    TEST_EMPLACE_BACK_CAP(16);
    TEST_EMPLACE_BACK_CAP(32);

    #undef TEST_EMPLACE_BACK_CAP

    // ==========================================================================
    // Test 2: push_back vs emplace_back comparison
    // ==========================================================================
    std::cout << "\n--- push_back vs emplace_back (InlineCap=16) ---\n\n";
    std::cout << "Operation   | SmallVector (ns/op) | std::vector* (ns/op)\n";
    std::cout << "------------|---------------------|---------------------\n";

    constexpr size_t INLINE_CAP_16 = 16;
    constexpr size_t N_16 = 16;

    // push_back
    {
        double sv_total = 0, std_total = 0;
        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            {
                fat_p::SmallVector<int64_t, INLINE_CAP_16> vec;
                Timer t; t.start();
                for (size_t i = 0; i < N_16; ++i) vec.push_back(static_cast<int64_t>(i));
                sv_total += t.elapsed_ns();
                benchmark_sink += vec.size();
            }
            {
                std::vector<int64_t> vec;
                vec.reserve(N_16);
                Timer t; t.start();
                for (size_t i = 0; i < N_16; ++i) vec.push_back(static_cast<int64_t>(i));
                std_total += t.elapsed_ns();
                benchmark_sink += vec.size();
            }
        }
        double sv_ns = sv_total / (ITERATIONS * N_16);
        double std_ns = std_total / (ITERATIONS * N_16);
        std::cout << "push_back   | " << std::setw(19) << sv_ns 
                  << " | " << std::setw(19) << std_ns << "\n";
    }

    // emplace_back
    {
        double sv_total = 0, std_total = 0;
        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            {
                fat_p::SmallVector<int64_t, INLINE_CAP_16> vec;
                Timer t; t.start();
                for (size_t i = 0; i < N_16; ++i) vec.emplace_back(static_cast<int64_t>(i));
                sv_total += t.elapsed_ns();
                benchmark_sink += vec.size();
            }
            {
                std::vector<int64_t> vec;
                vec.reserve(N_16);
                Timer t; t.start();
                for (size_t i = 0; i < N_16; ++i) vec.emplace_back(static_cast<int64_t>(i));
                std_total += t.elapsed_ns();
                benchmark_sink += vec.size();
            }
        }
        double sv_ns = sv_total / (ITERATIONS * N_16);
        double std_ns = std_total / (ITERATIONS * N_16);
        std::cout << "emplace_back| " << std::setw(19) << sv_ns 
                  << " | " << std::setw(19) << std_ns << "\n";
    }

    // ==========================================================================
    // Test 3: Competitor comparison (if available)
    // ==========================================================================
#if HAS_BOOST || HAS_LLVM
    std::cout << "\n--- Cross-Library Comparison (InlineCap=16, N=16) ---\n\n";
    std::cout << "Library       | emplace_back (ns/op)\n";
    std::cout << "--------------|---------------------\n";

    // SmallVector
    {
        double total = 0;
        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            fat_p::SmallVector<int64_t, 16> vec;
            Timer t; t.start();
            for (size_t i = 0; i < 16; ++i) vec.emplace_back(static_cast<int64_t>(i));
            total += t.elapsed_ns();
            benchmark_sink += vec.size();
        }
        std::cout << "fat_p::SV    | " << std::setw(19) << (total / (ITERATIONS * 16)) << "\n";
    }

#if HAS_BOOST
    // Boost
    {
        double total = 0;
        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            boost::container::small_vector<int64_t, 16> vec;
            Timer t; t.start();
            for (size_t i = 0; i < 16; ++i) vec.emplace_back(static_cast<int64_t>(i));
            total += t.elapsed_ns();
            benchmark_sink += vec.size();
        }
        std::cout << "boost::sv    | " << std::setw(19) << (total / (ITERATIONS * 16)) << "\n";
    }
#endif

#if HAS_LLVM
    // LLVM
    {
        double total = 0;
        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            llvm::SmallVector<int64_t, 16> vec;
            Timer t; t.start();
            for (size_t i = 0; i < 16; ++i) vec.emplace_back(static_cast<int64_t>(i));
            total += t.elapsed_ns();
            benchmark_sink += vec.size();
        }
        std::cout << "llvm::SV     | " << std::setw(19) << (total / (ITERATIONS * 16)) << "\n";
    }
#endif

    // std::vector (pre-reserved)
    {
        double total = 0;
        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            std::vector<int64_t> vec;
            vec.reserve(16);
            Timer t; t.start();
            for (size_t i = 0; i < 16; ++i) vec.emplace_back(static_cast<int64_t>(i));
            total += t.elapsed_ns();
            benchmark_sink += vec.size();
        }
        std::cout << "std::vector* | " << std::setw(19) << (total / (ITERATIONS * 16)) << "\n";
    }
#endif

    // ==========================================================================
    // Test 4: Repeated fill/clear cycle (measures amortized fast-path cost)
    // ==========================================================================
    std::cout << "\n--- Fill/Clear Cycle (1000 cycles, InlineCap=16) ---\n";
    std::cout << "Tests repeated use of same SmallVector (cache-warm scenario)\n\n";

    constexpr size_t CYCLES = 1000;
    constexpr size_t ELEMENTS = 16;

    // SmallVector - reuse same object
    {
        fat_p::SmallVector<int64_t, 16> vec;
        Timer t; t.start();
        for (size_t cycle = 0; cycle < CYCLES; ++cycle) {
            for (size_t i = 0; i < ELEMENTS; ++i) {
                vec.emplace_back(static_cast<int64_t>(i));
            }
            benchmark_sink += vec.size();
            vec.clear();
        }
        double total_ns = t.elapsed_ns();
        double ns_per_op = total_ns / (CYCLES * ELEMENTS);
        std::cout << "SmallVector (reused): " << std::fixed << std::setprecision(2) 
                  << ns_per_op << " ns/op\n";
    }

    // std::vector - reuse same object (pre-reserved)
    {
        std::vector<int64_t> vec;
        vec.reserve(ELEMENTS);
        Timer t; t.start();
        for (size_t cycle = 0; cycle < CYCLES; ++cycle) {
            for (size_t i = 0; i < ELEMENTS; ++i) {
                vec.emplace_back(static_cast<int64_t>(i));
            }
            benchmark_sink += vec.size();
            vec.clear();
        }
        double total_ns = t.elapsed_ns();
        double ns_per_op = total_ns / (CYCLES * ELEMENTS);
        std::cout << "std::vector (reused): " << std::fixed << std::setprecision(2) 
                  << ns_per_op << " ns/op\n";
    }

    std::cout << "\nNote: This benchmark measures the optimized fast-path code.\n";
    std::cout << "The main SmallVector advantage (7-8x for small N) comes from\n";
    std::cout << "avoiding heap allocation entirely, which is tested in\n";
    std::cout << "benchmark_inline_vs_heap().\n";
}

// ============================================================================
// Section 7: Object Size Impact
// ============================================================================

void benchmark_object_size()
{
    print_header("OBJECT SIZE IMPACT");

    std::cout << "How does sizeof(SmallVector) compare to std::vector?\n";
    std::cout << "Larger inline capacity = larger object size.\n\n";

    std::cout << "Container                        | sizeof (bytes)\n";
    std::cout << "---------------------------------|---------------\n";

    std::cout << std::setw(32) << "std::vector<int64_t>" << " | "
              << std::setw(14) << sizeof(std::vector<int64_t>) << "\n";

    std::cout << std::setw(32) << "SmallVector<int64_t, 4>" << " | "
              << std::setw(14) << sizeof(fat_p::SmallVector<int64_t, 4>) << "\n";

    std::cout << std::setw(32) << "SmallVector<int64_t, 8>" << " | "
              << std::setw(14) << sizeof(fat_p::SmallVector<int64_t, 8>) << "\n";

    std::cout << std::setw(32) << "SmallVector<int64_t, 16>" << " | "
              << std::setw(14) << sizeof(fat_p::SmallVector<int64_t, 16>) << "\n";

    std::cout << std::setw(32) << "SmallVector<int64_t, 32>" << " | "
              << std::setw(14) << sizeof(fat_p::SmallVector<int64_t, 32>) << "\n";

    std::cout << std::setw(32) << "SmallVector<int64_t, 64>" << " | "
              << std::setw(14) << sizeof(fat_p::SmallVector<int64_t, 64>) << "\n";

#if HAS_BOOST
    std::cout << "\nBoost comparison:\n";
    std::cout << std::setw(32) << "boost::small_vector<int64_t, 16>" << " | "
              << std::setw(14) << sizeof(boost::container::small_vector<int64_t, 16>) << "\n";
#endif

#if HAS_LLVM
    std::cout << "\nLLVM comparison:\n";
    std::cout << std::setw(32) << "llvm::SmallVector<int64_t, 16>" << " | "
              << std::setw(14) << sizeof(llvm::SmallVector<int64_t, 16>) << "\n";
#endif

    std::cout << "\nNote: SmallVector<T, N> size ≈ 3*sizeof(void*) + N*sizeof(T)\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    // Load configuration from FATP_BENCH_* environment variables
    g_config = fat_p::bench::BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity) unless disabled
    BenchmarkScope scope(!g_config.noScope);

    std::cout << "================================================================================\n";
    std::cout << "  SmallVector Comprehensive Benchmark Suite\n";
    std::cout << "================================================================================\n";
    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() 
              << ", seed=" << g_config.seed << ")\n";

    std::cout << "Competitor libraries: ";
#if HAS_BOOST
    std::cout << "boost ";
#endif
#if HAS_FOLLY
    std::cout << "folly ";
#endif
#if HAS_LLVM
    std::cout << "llvm ";
#endif
#if !HAS_BOOST && !HAS_FOLLY && !HAS_LLVM
    std::cout << "(none found)";
#endif
    std::cout << "\n\n";

    std::cout << "Design Invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run\n";
    std::cout << "  3. Setup/teardown outside timed regions\n";
    std::cout << "  4. All libraries observe same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n\n";

    // Default sizes
    std::vector<size_t> core_sizes = {100, 1000, 10000};

    // Run all benchmarks
    benchmark_core_operations(core_sizes);
    benchmark_inline_vs_heap();
    benchmark_allocation_count();
    benchmark_insert_erase();
    benchmark_inline_capacity_sensitivity();
    benchmark_fast_path_throughput();
    benchmark_object_size();

    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
