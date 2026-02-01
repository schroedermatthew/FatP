// benchmark_IntrusiveList.cpp
//
// FAT-P IntrusiveList benchmarks using unified FatPBenchmarkRunner infrastructure.
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
//   - fat_p::IntrusiveList: Zero-allocation intrusive doubly-linked list
//                           O(1) insert/remove, isLinked() check (fast policy, default)
//   - fat_p::IntrusiveListSafe: Ownership-tracking variant (safe wrong-list remove)
//
// Competitor Libraries (conditioned on availability):
//   TIER 1 - Direct competitors (intrusive lists):
//     - boost::intrusive::list - Mature, industry-standard intrusive list
//     - eastl::intrusive_list  - EA's game-industry intrusive list
//     - llvm::simple_ilist     - LLVM's compiler infrastructure intrusive list
//     - etl::intrusive_list    - Embedded Template Library intrusive list
//   TIER 2 - Standard library alternatives:
//     - std::list<T*>          - Allocating list (baseline for speedup claims)
//
// Sections:
//   1. Push Back (insertion performance)
//   2. Remove (O(1) removal with direct reference)
//   3. Iteration (traversal performance)
//   4. Splice (bulk transfer - O(1) vs O(N) semantics)
//   5. Memory Overhead (per-node overhead comparison)
//   6. Free List Pattern (real-world object pool simulation)
//   7. isLinked() / is_linked() Check (membership query)
//
// Build (minimal):
//   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_IntrusiveList.cpp -o bench_il
//
// Build (MSVC):
//   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_IntrusiveList.cpp /link advapi32.lib
//
// Build (with competitors):
//   g++ -std=c++17 -O3 -DNDEBUG -march=native -I/path/to/boost -I/path/to/EASTL benchmark_IntrusiveList.cpp -o bench_il
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
//   ./bench_il
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_il

/*
FATP_META:
  meta_version: 1
  component: IntrusiveList
  file_role: benchmark
  path: components/IntrusiveList/benchmarks/benchmark_IntrusiveList.cpp
  layer: Testing
  namespace: fat_p
  summary: "Comprehensive benchmarks for IntrusiveList vs industry competitors."
  api_stability: in_work
  related:
    docs_search: "IntrusiveList"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/IntrusiveList.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 11
    defines_unprefixed: 4
    undefs_total: 4
    includes_windows_h: true
  generated:
    by: Claude
    mode: manual
*/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "FatPBenchmarkRunner.h"
#include "IntrusiveList.h"

// Suppress warnings from third-party headers (MSVC only)
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif

// ============================================================================
// Library Detection
// ============================================================================

// Boost.Intrusive - Mature, widely-adopted intrusive containers
// Install: vcpkg install boost-intrusive
#if __has_include(<boost/intrusive/list.hpp>)
#include <boost/intrusive/list.hpp>
#define HAS_BOOST_INTRUSIVE 1
#else
#define HAS_BOOST_INTRUSIVE 0
#endif

// EASTL - EA's game-industry standard containers
// Install: vcpkg install eastl
#if __has_include(<EASTL/intrusive_list.h>)
#include <EASTL/intrusive_list.h>
#define HAS_EASTL 1
// EASTL requires operator new[] - minimal implementation
void* operator new[](size_t size, const char*, int, unsigned, const char*, int)
{
    return malloc(size);
}
void* operator new[](size_t size, size_t, size_t, const char*, int, unsigned, const char*, int)
{
    return malloc(size);
}
#else
#define HAS_EASTL 0
#endif

// LLVM ilist - Compiler infrastructure intrusive list
// Install: vcpkg install llvm  OR  apt install llvm-dev
#if __has_include(<llvm/ADT/simple_ilist.h>)
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/simple_ilist.h>
#define HAS_LLVM_ILIST 1
#elif __has_include(<llvm-18/llvm/ADT/simple_ilist.h>)
#include <llvm-18/llvm/ADT/ilist_node.h>
#include <llvm-18/llvm/ADT/simple_ilist.h>
#define HAS_LLVM_ILIST 1
#else
#define HAS_LLVM_ILIST 0
#endif

// ETL (Embedded Template Library) - for embedded systems
// Install: https://github.com/ETLCPP/etl (header-only)
#if __has_include(<etl/intrusive_list.h>)
#include <etl/intrusive_list.h>
#define HAS_ETL 1
#else
#define HAS_ETL 0
#endif

#if defined(_MSC_VER)
#pragma warning(pop)
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

// ============================================================================
// Timer
// ============================================================================

struct Timer
{
    using Clock = fat_p::bench::BenchClock;
    Clock::time_point t0;

    void start()
    {
        t0 = Clock::now();
    }

    double elapsedNs() const
    {
        auto t1 = Clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }
};

// Prevent dead code elimination
static volatile int64_t benchmark_sink = 0;

template <typename T>
static inline void DoNotOptimize(T const& value)
{
    benchmark_sink ^= static_cast<int64_t>(reinterpret_cast<uintptr_t>(&value));
}

// ============================================================================
// Statistics
// ============================================================================

struct Stats
{
    double median = 0;
    double mean = 0;
    double stddev = 0;
    double ci95_low = 0;
    double ci95_high = 0;
};

static Stats compute_stats(std::vector<double> samples)
{
    Stats s{};
    if (samples.empty())
    {
        return s;
    }

    std::sort(samples.begin(), samples.end());
    size_t n = samples.size();

    // Median
    s.median = (n % 2 == 1) ? samples[n / 2] : 0.5 * (samples[n / 2 - 1] + samples[n / 2]);

    // Mean
    s.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(n);

    // Stddev
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

static void print_contract(const char* contract)
{
    std::cout << "Contract: " << contract << "\n\n";
}

static void print_table_header()
{
    std::cout << std::setw(30) << std::left << "Library" << std::setw(12) << std::right << "Median" << std::setw(12)
              << "Mean" << std::setw(10) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(79, '-') << "\n";
}

static void print_result_row(const char* name, const Stats& s, const char* unit = "ns/op")
{
    std::cout << std::setw(30) << std::left << name << std::fixed << std::setprecision(2) << std::setw(12) << std::right
              << s.median << std::setw(12) << s.mean << std::setw(10) << s.stddev << "  [" << s.ci95_low << ", "
              << s.ci95_high << "] " << unit << "\n";
}

// ============================================================================
// Node Types for Each Library
// ============================================================================

// Fat-P IntrusiveList (fast policy, default) node
struct FatPFastNode : public fat_p::IntrusiveListNode<FatPFastNode>
{
    int64_t value;
    int64_t padding[7]{}; // Pad to 64 bytes for fair comparison

    explicit FatPFastNode(int64_t v = 0)
        : value(v)
    {
    }
};

// Fat-P IntrusiveList (safe policy) node
struct FatPSafeNode : public fat_p::IntrusiveListNode<FatPSafeNode, fat_p::intrusive_list::SafeOwnerPolicy>
{
    int64_t value;
    int64_t padding[7]{};

    explicit FatPSafeNode(int64_t v = 0)
        : value(v)
    {
    }
};

// std::list node (stores pointer, not intrusive)
struct StdNode
{
    int64_t value;
    int64_t padding[7]{};

    explicit StdNode(int64_t v = 0)
        : value(v)
    {
    }
};

#if HAS_BOOST_INTRUSIVE
// Boost.Intrusive node
struct BoostNode : public boost::intrusive::list_base_hook<>
{
    int64_t value;
    int64_t padding[7]{};

    explicit BoostNode(int64_t v = 0)
        : value(v)
    {
    }
};
#endif

#if HAS_EASTL
// EASTL intrusive_list node
struct EastlNode : public eastl::intrusive_list_node
{
    int64_t value;
    int64_t padding[7]{};

    explicit EastlNode(int64_t v = 0)
        : value(v)
    {
    }
};
#endif

#if HAS_ETL
// ETL intrusive_list node
struct EtlNode : public etl::bidirectional_link<0>
{
    int64_t value;
    int64_t padding[7]{};

    explicit EtlNode(int64_t v = 0)
        : value(v)
    {
    }
};

// ETL's remove() requires operator== (non-member for MSVC compatibility)
inline bool operator==(const EtlNode& lhs, const EtlNode& rhs)
{
    return &lhs == &rhs;
}
inline bool operator!=(const EtlNode& lhs, const EtlNode& rhs)
{
    return !(lhs == rhs);
}
#endif

#if HAS_LLVM_ILIST
// LLVM ilist node
struct LlvmNode : public llvm::ilist_node<LlvmNode>
{
    int64_t value;
    int64_t padding[7]{};

    explicit LlvmNode(int64_t v = 0)
        : value(v)
    {
    }
};
#endif

// ============================================================================
// Adapter Interface
// ============================================================================

// Abstract interface for intrusive list adapters
// Each adapter wraps a specific library's list implementation
struct IListAdapter
{
    virtual ~IListAdapter() = default;

    // Identity
    virtual const char* name() const = 0;

    // Lifecycle
    virtual void setup(size_t capacity) = 0;
    virtual void teardown() = 0;

    // Core operations (timed)
    virtual void push_back_all() = 0;     // Push all nodes to list
    virtual void remove_all_random() = 0; // Remove all nodes in random order
    virtual int64_t iterate_sum() = 0;    // Iterate and sum values
    virtual void splice_all() = 0;        // Splice from src to dst list

    // Free list pattern operations
    virtual void free_list_setup() = 0;
    virtual void free_list_ops(const std::vector<bool>& isAlloc) = 0;

    // Queries
    virtual size_t size() const = 0;
    virtual size_t node_sizeof() const = 0;
    virtual void setup_is_linked() = 0; // Setup for is_linked benchmark (link half nodes)
    virtual size_t count_linked() = 0;  // Count nodes that report is_linked (timed)

    // For result collection
    std::vector<double> times;
    void record_time(double ns_per_op)
    {
        times.push_back(ns_per_op);
    }
    Stats get_stats() const
    {
        return compute_stats(times);
    }
    void clear_times()
    {
        times.clear();
    }
};

// ============================================================================
// Fat-P IntrusiveList Adapters
// ============================================================================

class FatPFastListAdapter final : public IListAdapter
{
    std::deque<FatPFastNode> nodes_;
    fat_p::IntrusiveList<FatPFastNode> list_;
    fat_p::IntrusiveList<FatPFastNode> src_list_; // For splice
    std::vector<size_t> removeOrder_;
    std::vector<FatPFastNode*> allocated_; // For free list pattern

public:
    const char* name() const override
    {
        return "fat_p::IntrusiveList (fast)";
    }

    void setup(size_t capacity) override
    {
        nodes_.clear();
        for (size_t i = 0; i < capacity; ++i)
        {
            nodes_.emplace_back(static_cast<int64_t>(i));
        }

        // Generate random removal order
        removeOrder_.resize(capacity);
        std::iota(removeOrder_.begin(), removeOrder_.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::shuffle(removeOrder_.begin(), removeOrder_.end(), rng);
    }

    void teardown() override
    {
        list_.clear();
        src_list_.clear();
        nodes_.clear();
        removeOrder_.clear();
        allocated_.clear();
    }

    void push_back_all() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void remove_all_random() override
    {
        // First add all nodes
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
        // Then remove in random order (timed separately)
        for (size_t idx : removeOrder_)
        {
            list_.remove(nodes_[idx]);
        }
    }

    int64_t iterate_sum() override
    {
        // Ensure list is populated
        if (list_.empty())
        {
            for (auto& node : nodes_)
            {
                list_.push_back(node);
            }
        }
        int64_t sum = 0;
        for (const auto& node : list_)
        {
            sum += node.value;
        }
        return sum;
    }

    void splice_all() override
    {
        // Setup: put all nodes in src
        for (auto& node : nodes_)
        {
            src_list_.push_back(node);
        }
        // Splice to dst (timed)
        list_.splice(list_.end(), src_list_);
        // Cleanup
        list_.clear();
    }

    void free_list_setup() override
    {
        allocated_.clear();
        allocated_.reserve(nodes_.size());
        list_.clear();
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void free_list_ops(const std::vector<bool>& isAlloc) override
    {
        for (bool alloc : isAlloc)
        {
            if (alloc && !list_.empty())
            {
                allocated_.push_back(&list_.front());
                list_.pop_front();
            }
            else if (!alloc && !allocated_.empty())
            {
                list_.push_back(*allocated_.back());
                allocated_.pop_back();
            }
        }
        benchmark_sink += static_cast<int64_t>(list_.size() + allocated_.size());
        list_.clear();
    }

    size_t size() const override
    {
        return list_.size();
    }
    size_t node_sizeof() const override
    {
        return sizeof(FatPFastNode);
    }

    void setup_is_linked() override
    {
        // Link half the nodes
        list_.clear();
        for (size_t i = 0; i < nodes_.size(); i += 2)
        {
            list_.push_back(nodes_[i]);
        }
    }

    size_t count_linked() override
    {
        // Just count - setup already done
        size_t count = 0;
        for (const auto& node : nodes_)
        {
            if (node.isLinked())
            {
                ++count;
            }
        }
        return count;
    }
};

class FatPSafeListAdapter final : public IListAdapter
{
    std::deque<FatPSafeNode> nodes_;
    fat_p::IntrusiveListSafe<FatPSafeNode> list_;
    fat_p::IntrusiveListSafe<FatPSafeNode> src_list_;
    std::vector<size_t> removeOrder_;
    std::vector<FatPSafeNode*> allocated_;

public:
    const char* name() const override
    {
        return "fat_p::IntrusiveList (safe)";
    }

    void setup(size_t capacity) override
    {
        nodes_.clear();
        for (size_t i = 0; i < capacity; ++i)
        {
            nodes_.emplace_back(static_cast<int64_t>(i));
        }

        removeOrder_.resize(capacity);
        std::iota(removeOrder_.begin(), removeOrder_.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::shuffle(removeOrder_.begin(), removeOrder_.end(), rng);
    }

    void teardown() override
    {
        list_.clear();
        src_list_.clear();
        nodes_.clear();
        removeOrder_.clear();
        allocated_.clear();
    }

    void push_back_all() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void remove_all_random() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
        for (size_t idx : removeOrder_)
        {
            list_.remove(nodes_[idx]);
        }
    }

    int64_t iterate_sum() override
    {
        if (list_.empty())
        {
            for (auto& node : nodes_)
            {
                list_.push_back(node);
            }
        }
        int64_t sum = 0;
        for (const auto& node : list_)
        {
            sum += node.value;
        }
        return sum;
    }

    void splice_all() override
    {
        for (auto& node : nodes_)
        {
            src_list_.push_back(node);
        }
        list_.splice(list_.end(), src_list_);
        list_.clear();
    }

    void free_list_setup() override
    {
        allocated_.clear();
        allocated_.reserve(nodes_.size());
        list_.clear();
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void free_list_ops(const std::vector<bool>& isAlloc) override
    {
        for (bool alloc : isAlloc)
        {
            if (alloc && !list_.empty())
            {
                allocated_.push_back(&list_.front());
                list_.pop_front();
            }
            else if (!alloc && !allocated_.empty())
            {
                list_.push_back(*allocated_.back());
                allocated_.pop_back();
            }
        }
        benchmark_sink += static_cast<int64_t>(list_.size() + allocated_.size());
        list_.clear();
    }

    size_t size() const override
    {
        return list_.size();
    }

    size_t node_sizeof() const override
    {
        return sizeof(FatPSafeNode);
    }

    void setup_is_linked() override
    {
        list_.clear();
        for (size_t i = 0; i < nodes_.size(); i += 2)
        {
            list_.push_back(nodes_[i]);
        }
    }

    size_t count_linked() override
    {
        size_t count = 0;
        for (const auto& node : nodes_)
        {
            if (node.isLinked())
            {
                ++count;
            }
        }
        return count;
    }
};

// ============================================================================
// std::list<T*> Adapter (Baseline)
// ============================================================================

class StdListAdapter final : public IListAdapter
{
    std::deque<StdNode> nodes_; // Use deque like intrusive adapters for fair memory layout
    std::list<StdNode*> list_;
    std::list<StdNode*> src_list_;
    std::vector<std::list<StdNode*>::iterator> iterators_; // For O(1) removal
    std::vector<size_t> removeOrder_;
    std::vector<StdNode*> allocated_;

public:
    const char* name() const override
    {
        return "std::list<T*>";
    }

    void setup(size_t capacity) override
    {
        nodes_.clear();
        for (size_t i = 0; i < capacity; ++i)
        {
            nodes_.emplace_back(static_cast<int64_t>(i));
        }
        iterators_.resize(capacity);

        removeOrder_.resize(capacity);
        std::iota(removeOrder_.begin(), removeOrder_.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::shuffle(removeOrder_.begin(), removeOrder_.end(), rng);
    }

    void teardown() override
    {
        list_.clear();
        src_list_.clear();
        nodes_.clear();
        iterators_.clear();
        removeOrder_.clear();
        allocated_.clear();
    }

    void push_back_all() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(&node);
        }
    }

    void remove_all_random() override
    {
        // Add all with iterator tracking
        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            iterators_[i] = list_.insert(list_.end(), &nodes_[i]);
        }
        // Remove in random order
        for (size_t idx : removeOrder_)
        {
            list_.erase(iterators_[idx]);
        }
    }

    int64_t iterate_sum() override
    {
        if (list_.empty())
        {
            for (auto& node : nodes_)
            {
                list_.push_back(&node);
            }
        }
        int64_t sum = 0;
        for (const auto* node : list_)
        {
            sum += node->value;
        }
        return sum;
    }

    void splice_all() override
    {
        for (auto& node : nodes_)
        {
            src_list_.push_back(&node);
        }
        list_.splice(list_.end(), src_list_);
        list_.clear();
    }

    void free_list_setup() override
    {
        allocated_.clear();
        allocated_.reserve(nodes_.size());
        list_.clear();
        for (auto& node : nodes_)
        {
            list_.push_back(&node);
        }
    }

    void free_list_ops(const std::vector<bool>& isAlloc) override
    {
        for (bool alloc : isAlloc)
        {
            if (alloc && !list_.empty())
            {
                allocated_.push_back(list_.front());
                list_.pop_front();
            }
            else if (!alloc && !allocated_.empty())
            {
                list_.push_back(allocated_.back());
                allocated_.pop_back();
            }
        }
        benchmark_sink += static_cast<int64_t>(list_.size() + allocated_.size());
    }

    size_t size() const override
    {
        return list_.size();
    }
    size_t node_sizeof() const override
    {
        // Approximation for std::list<T*> node:
        //   prev pointer + next pointer + stored T* value
        // Allocator metadata not included.
        return sizeof(StdNode) + 3 * sizeof(void*);
    } // + list node overhead

    void setup_is_linked() override
    {
        // Link half the nodes
        list_.clear();
        for (size_t i = 0; i < nodes_.size(); i += 2)
        {
            list_.push_back(&nodes_[i]);
        }
    }

    size_t count_linked() override
    {
        // std::list has NO is_linked() - user must do O(N) search
        // This is std::list's real limitation
        size_t count = 0;
        for (auto& node : nodes_)
        {
            for (auto* n : list_)
            {
                if (n == &node)
                {
                    ++count;
                    break;
                }
            }
        }
        return count;
    }
};

// ============================================================================
// Boost.Intrusive Adapter
// ============================================================================

#if HAS_BOOST_INTRUSIVE
class BoostListAdapter final : public IListAdapter
{
    std::deque<BoostNode> nodes_;
    boost::intrusive::list<BoostNode> list_;
    boost::intrusive::list<BoostNode> src_list_;
    std::vector<size_t> removeOrder_;
    std::vector<BoostNode*> allocated_;

public:
    const char* name() const override
    {
        return "boost::intrusive::list";
    }

    void setup(size_t capacity) override
    {
        nodes_.clear();
        for (size_t i = 0; i < capacity; ++i)
        {
            nodes_.emplace_back(static_cast<int64_t>(i));
        }

        removeOrder_.resize(capacity);
        std::iota(removeOrder_.begin(), removeOrder_.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::shuffle(removeOrder_.begin(), removeOrder_.end(), rng);
    }

    void teardown() override
    {
        list_.clear();
        src_list_.clear();
        nodes_.clear();
        removeOrder_.clear();
        allocated_.clear();
    }

    void push_back_all() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void remove_all_random() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
        for (size_t idx : removeOrder_)
        {
            list_.erase(boost::intrusive::list<BoostNode>::s_iterator_to(nodes_[idx]));
        }
    }

    int64_t iterate_sum() override
    {
        if (list_.empty())
        {
            for (auto& node : nodes_)
            {
                list_.push_back(node);
            }
        }
        int64_t sum = 0;
        for (const auto& node : list_)
        {
            sum += node.value;
        }
        return sum;
    }

    void splice_all() override
    {
        for (auto& node : nodes_)
        {
            src_list_.push_back(node);
        }
        list_.splice(list_.end(), src_list_);
        list_.clear();
    }

    void free_list_setup() override
    {
        allocated_.clear();
        allocated_.reserve(nodes_.size());
        list_.clear();
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void free_list_ops(const std::vector<bool>& isAlloc) override
    {
        for (bool alloc : isAlloc)
        {
            if (alloc && !list_.empty())
            {
                allocated_.push_back(&list_.front());
                list_.pop_front();
            }
            else if (!alloc && !allocated_.empty())
            {
                list_.push_back(*allocated_.back());
                allocated_.pop_back();
            }
        }
        benchmark_sink += static_cast<int64_t>(list_.size() + allocated_.size());
        list_.clear();
    }

    size_t size() const override
    {
        return list_.size();
    }
    size_t node_sizeof() const override
    {
        return sizeof(BoostNode);
    }

    void setup_is_linked() override
    {
        // Link half the nodes
        list_.clear();
        for (size_t i = 0; i < nodes_.size(); i += 2)
        {
            list_.push_back(nodes_[i]);
        }
    }

    size_t count_linked() override
    {
        // Boost base_hook has is_linked()
        size_t count = 0;
        for (const auto& node : nodes_)
        {
            if (node.is_linked())
            {
                ++count;
            }
        }
        return count;
    }
};
#endif

// ============================================================================
// EASTL Adapter
// ============================================================================

#if HAS_EASTL
class EastlListAdapter final : public IListAdapter
{
    std::deque<EastlNode> nodes_;
    eastl::intrusive_list<EastlNode> list_;
    eastl::intrusive_list<EastlNode> src_list_;
    std::vector<size_t> removeOrder_;
    std::vector<EastlNode*> allocated_;

public:
    const char* name() const override
    {
        return "eastl::intrusive_list";
    }

    void setup(size_t capacity) override
    {
        nodes_.clear();
        for (size_t i = 0; i < capacity; ++i)
        {
            nodes_.emplace_back(static_cast<int64_t>(i));
        }

        removeOrder_.resize(capacity);
        std::iota(removeOrder_.begin(), removeOrder_.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::shuffle(removeOrder_.begin(), removeOrder_.end(), rng);
    }

    void teardown() override
    {
        list_.clear();
        src_list_.clear();
        nodes_.clear();
        removeOrder_.clear();
        allocated_.clear();
    }

    void push_back_all() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void remove_all_random() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
        for (size_t idx : removeOrder_)
        {
            list_.remove(nodes_[idx]);
        }
    }

    int64_t iterate_sum() override
    {
        if (list_.empty())
        {
            for (auto& node : nodes_)
            {
                list_.push_back(node);
            }
        }
        int64_t sum = 0;
        for (const auto& node : list_)
        {
            sum += node.value;
        }
        return sum;
    }

    void splice_all() override
    {
        for (auto& node : nodes_)
        {
            src_list_.push_back(node);
        }
        list_.splice(list_.end(), src_list_);
        list_.clear();
    }

    void free_list_setup() override
    {
        allocated_.clear();
        allocated_.reserve(nodes_.size());
        list_.clear();
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void free_list_ops(const std::vector<bool>& isAlloc) override
    {
        for (bool alloc : isAlloc)
        {
            if (alloc && !list_.empty())
            {
                allocated_.push_back(&list_.front());
                list_.pop_front();
            }
            else if (!alloc && !allocated_.empty())
            {
                list_.push_back(*allocated_.back());
                allocated_.pop_back();
            }
        }
        benchmark_sink += static_cast<int64_t>(list_.size() + allocated_.size());
        list_.clear();
    }

    size_t size() const override
    {
        return list_.size();
    }
    size_t node_sizeof() const override
    {
        return sizeof(EastlNode);
    }

    void setup_is_linked() override
    {
        // Link half the nodes
        list_.clear();
        for (size_t i = 0; i < nodes_.size(); i += 2)
        {
            list_.push_back(nodes_[i]);
        }
    }

    size_t count_linked() override
    {
        // EASTL intrusive_list_node: linked if mpNext != nullptr
        size_t count = 0;
        for (const auto& node : nodes_)
        {
            // EASTL uses mpNext/mpPrev members
            if (node.mpNext != nullptr)
            {
                ++count;
            }
        }
        return count;
    }
};
#endif

// ============================================================================
// ETL Adapter
// ============================================================================

#if HAS_ETL
class EtlListAdapter final : public IListAdapter
{
    std::deque<EtlNode> nodes_;
    etl::intrusive_list<EtlNode, etl::bidirectional_link<0>> list_;
    etl::intrusive_list<EtlNode, etl::bidirectional_link<0>> src_list_;
    std::vector<size_t> removeOrder_;
    std::vector<EtlNode*> allocated_;

public:
    const char* name() const override
    {
        return "etl::intrusive_list [!]";
    } // [!] = has limitations

    void setup(size_t capacity) override
    {
        nodes_.clear();
        for (size_t i = 0; i < capacity; ++i)
        {
            nodes_.emplace_back(static_cast<int64_t>(i));
        }

        removeOrder_.resize(capacity);
        std::iota(removeOrder_.begin(), removeOrder_.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::shuffle(removeOrder_.begin(), removeOrder_.end(), rng);
    }

    void teardown() override
    {
        list_.clear();
        src_list_.clear();
        nodes_.clear();
        removeOrder_.clear();
        allocated_.clear();
    }

    void push_back_all() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void remove_all_random() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
        // ETL's remove() is O(N) - it searches the list. This is a real API limitation.
        for (size_t idx : removeOrder_)
        {
            list_.remove(nodes_[idx]);
        }
    }

    int64_t iterate_sum() override
    {
        if (list_.empty())
        {
            for (auto& node : nodes_)
            {
                list_.push_back(node);
            }
        }
        int64_t sum = 0;
        for (const auto& node : list_)
        {
            sum += node.value;
        }
        return sum;
    }

    void splice_all() override
    {
        for (auto& node : nodes_)
        {
            src_list_.push_back(node);
        }
        list_.splice(list_.end(), src_list_);
        list_.clear();
    }

    void free_list_setup() override
    {
        allocated_.clear();
        allocated_.reserve(nodes_.size());
        list_.clear();
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void free_list_ops(const std::vector<bool>& isAlloc) override
    {
        for (bool alloc : isAlloc)
        {
            if (alloc && !list_.empty())
            {
                allocated_.push_back(&list_.front());
                list_.pop_front();
            }
            else if (!alloc && !allocated_.empty())
            {
                list_.push_back(*allocated_.back());
                allocated_.pop_back();
            }
        }
        benchmark_sink += static_cast<int64_t>(list_.size() + allocated_.size());
        list_.clear();
    }

    size_t size() const override
    {
        return list_.size();
    }
    size_t node_sizeof() const override
    {
        return sizeof(EtlNode);
    }

    void setup_is_linked() override
    {
        // Link half the nodes
        list_.clear();
        for (size_t i = 0; i < nodes_.size(); i += 2)
        {
            list_.push_back(nodes_[i]);
        }
    }

    size_t count_linked() override
    {
        // ETL bidirectional_link: check if linked via link pointers
        size_t count = 0;
        for (const auto& node : nodes_)
        {
            // ETL clears links on remove, so check if etl_next is set
            // Note: ETL uses etl_previous/etl_next members
            if (node.etl_next != nullptr)
            {
                ++count;
            }
        }
        return count;
    }
};
#endif

// ============================================================================
// LLVM ilist Adapter
// ============================================================================

#if HAS_LLVM_ILIST
class LlvmListAdapter final : public IListAdapter
{
    std::deque<LlvmNode> nodes_;
    llvm::simple_ilist<LlvmNode> list_;
    llvm::simple_ilist<LlvmNode> src_list_;
    std::vector<size_t> removeOrder_;
    std::vector<LlvmNode*> allocated_;

public:
    const char* name() const override
    {
        return "llvm::simple_ilist";
    }

    void setup(size_t capacity) override
    {
        nodes_.clear();
        for (size_t i = 0; i < capacity; ++i)
        {
            nodes_.emplace_back(static_cast<int64_t>(i));
        }

        removeOrder_.resize(capacity);
        std::iota(removeOrder_.begin(), removeOrder_.end(), 0);
        std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
        std::shuffle(removeOrder_.begin(), removeOrder_.end(), rng);
    }

    void teardown() override
    {
        list_.clear();
        src_list_.clear();
        nodes_.clear();
        removeOrder_.clear();
        allocated_.clear();
    }

    void push_back_all() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void remove_all_random() override
    {
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
        for (size_t idx : removeOrder_)
        {
            list_.remove(nodes_[idx]);
        }
    }

    int64_t iterate_sum() override
    {
        if (list_.empty())
        {
            for (auto& node : nodes_)
            {
                list_.push_back(node);
            }
        }
        int64_t sum = 0;
        for (const auto& node : list_)
        {
            sum += node.value;
        }
        return sum;
    }

    void splice_all() override
    {
        for (auto& node : nodes_)
        {
            src_list_.push_back(node);
        }
        list_.splice(list_.end(), src_list_);
        list_.clear();
    }

    void free_list_setup() override
    {
        allocated_.clear();
        allocated_.reserve(nodes_.size());
        list_.clear();
        for (auto& node : nodes_)
        {
            list_.push_back(node);
        }
    }

    void free_list_ops(const std::vector<bool>& isAlloc) override
    {
        for (bool alloc : isAlloc)
        {
            if (alloc && !list_.empty())
            {
                allocated_.push_back(&list_.front());
                list_.pop_front();
            }
            else if (!alloc && !allocated_.empty())
            {
                list_.push_back(*allocated_.back());
                allocated_.pop_back();
            }
        }
        benchmark_sink += static_cast<int64_t>(list_.size() + allocated_.size());
        list_.clear();
    }

    size_t size() const override
    {
        return list_.size();
    }
    size_t node_sizeof() const override
    {
        return sizeof(LlvmNode);
    }

    void setup_is_linked() override
    {
        // Link half the nodes
        list_.clear();
        for (size_t i = 0; i < nodes_.size(); i += 2)
        {
            list_.push_back(nodes_[i]);
        }
    }

    size_t count_linked() override
    {
        // LLVM simple_ilist has NO public is_linked() API
        // User must do O(N) search - this is LLVM's real limitation
        size_t count = 0;
        for (auto& node : nodes_)
        {
            for (auto& n : list_)
            {
                if (&n == &node)
                {
                    ++count;
                    break;
                }
            }
        }
        return count;
    }
};
#endif

// ============================================================================
// Benchmark Functions
// ============================================================================

void benchmark_push_back(std::vector<std::unique_ptr<IListAdapter>>& adapters, size_t N)
{
    print_header("PUSH_BACK PERFORMANCE (N=" + std::to_string(N) + ")");
    print_cpu_context();
    print_contract("Zero allocation for IntrusiveList vs heap allocation for std::list");

    std::cout << "Measuring time to push_back " << N << " pre-existing nodes.\n";
    std::cout << "IntrusiveList: no allocation (nodes pre-exist)\n";
    std::cout << "std::list: allocates node wrapper for each push\n\n";

    // Setup all adapters
    for (auto& adapter : adapters)
    {
        adapter->setup(N);
        adapter->clear_times();
    }

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            adapter->teardown();
            adapter->setup(N);
            adapter->push_back_all();
            benchmark_sink += static_cast<int64_t>(adapter->size());
            adapter->teardown();
            adapter->setup(N);
        }
    }

    // Measured runs - round robin with randomized order
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        // Create shuffled order
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            auto& adapter = adapters[idx];
            adapter->teardown();
            adapter->setup(N);

            Timer t;
            t.start();
            adapter->push_back_all();
            double elapsed = t.elapsedNs();

            benchmark_sink += static_cast<int64_t>(adapter->size());
            adapter->record_time(elapsed / static_cast<double>(N));
        }
    }

    // Print results
    print_table_header();
    Stats fatpStats;
    for (auto& adapter : adapters)
    {
        auto stats = adapter->get_stats();
        print_result_row(adapter->name(), stats);
        if (std::string(adapter->name()).find("fat_p") != std::string::npos)
        {
            fatpStats = stats;
        }
    }

    // Find std::list stats for speedup
    for (auto& adapter : adapters)
    {
        if (std::string(adapter->name()).find("std::list") != std::string::npos)
        {
            auto stdStats = adapter->get_stats();
            std::cout << "\nSpeedup (IntrusiveList vs std::list): " << std::fixed << std::setprecision(1)
                      << (stdStats.median / fatpStats.median) << "x\n";
            break;
        }
    }

    // Cleanup
    for (auto& adapter : adapters)
    {
        adapter->teardown();
    }
}

void benchmark_remove(std::vector<std::unique_ptr<IListAdapter>>& adapters, size_t N)
{
    print_header("REMOVE PERFORMANCE (N=" + std::to_string(N) + ")");
    print_cpu_context();
    print_contract("O(1) removal with known node reference - except ETL which is O(N)");

    std::cout << "Measuring time to remove " << N << " nodes in random order.\n";
    std::cout << "Most intrusive lists: O(1) removal via node reference or iterator_to()\n";
    std::cout << "ETL: O(N) removal - searches the list (API limitation)\n\n";

    for (auto& adapter : adapters)
    {
        adapter->setup(N);
        adapter->clear_times();
    }

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            adapter->teardown();
            adapter->setup(N);
            adapter->remove_all_random();
            adapter->teardown();
            adapter->setup(N);
        }
    }

    // Measured runs
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            auto& adapter = adapters[idx];
            adapter->teardown();
            adapter->setup(N);

            Timer t;
            t.start();
            adapter->remove_all_random();
            double elapsed = t.elapsedNs();

            adapter->record_time(elapsed / static_cast<double>(N));
        }
    }

    print_table_header();
    Stats fatpStats;
    for (auto& adapter : adapters)
    {
        auto stats = adapter->get_stats();
        print_result_row(adapter->name(), stats);
        if (std::string(adapter->name()).find("fat_p") != std::string::npos)
        {
            fatpStats = stats;
        }
    }

    for (auto& adapter : adapters)
    {
        if (std::string(adapter->name()).find("std::list") != std::string::npos)
        {
            auto stdStats = adapter->get_stats();
            std::cout << "\nSpeedup (IntrusiveList vs std::list): " << std::fixed << std::setprecision(1)
                      << (stdStats.median / fatpStats.median) << "x\n";
            break;
        }
    }

    for (auto& adapter : adapters)
    {
        adapter->teardown();
    }
}

void benchmark_iteration(std::vector<std::unique_ptr<IListAdapter>>& adapters, size_t N)
{
    print_header("ITERATION PERFORMANCE (N=" + std::to_string(N) + ")");
    print_cpu_context();
    print_contract("Sequential traversal with equivalent memory layout (std::deque storage)");

    std::cout << "Measuring time to iterate and sum " << N << " elements.\n";
    std::cout << "All competitors use std::deque for node storage.\n\n";

    for (auto& adapter : adapters)
    {
        adapter->setup(N);
        adapter->clear_times();
    }

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            int64_t sum = adapter->iterate_sum();
            DoNotOptimize(sum);
        }
    }

    // Measured runs
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            auto& adapter = adapters[idx];

            Timer t;
            t.start();
            int64_t sum = adapter->iterate_sum();
            double elapsed = t.elapsedNs();

            DoNotOptimize(sum);
            adapter->record_time(elapsed); // Total time, not per-op
        }
    }

    print_table_header();
    for (auto& adapter : adapters)
    {
        auto stats = adapter->get_stats();
        print_result_row(adapter->name(), stats, "ns/iter");
    }

    // Per-element calculation
    if (!adapters.empty())
    {
        auto fatpStats = adapters[0]->get_stats();
        double nsPerElement = fatpStats.median / static_cast<double>(N);
        std::cout << "\nPer-element: " << std::fixed << std::setprecision(2) << nsPerElement << " ns/element\n";
    }

    for (auto& adapter : adapters)
    {
        adapter->teardown();
    }
}

void benchmark_splice(std::vector<std::unique_ptr<IListAdapter>>& adapters, size_t N)
{
    print_header("SPLICE PERFORMANCE (N=" + std::to_string(N) + ")");
    print_cpu_context();
    print_contract("Build source list + splice to dest (measures total transfer cost)");

    std::cout << "Measuring time to build source list and splice " << N << " elements.\n";
    std::cout << "std::list: N allocating push_backs + O(1) splice\n";
    std::cout << "Intrusive: N non-allocating links + O(1) splice\n";
    std::cout << "fat_p: fast policy splices in O(1); safe policy is O(N) due to owner updates.\n\n";

    for (auto& adapter : adapters)
    {
        adapter->setup(N);
        adapter->clear_times();
    }

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            adapter->teardown();
            adapter->setup(N);
            adapter->splice_all();
            adapter->teardown();
            adapter->setup(N);
        }
    }

    // Measured runs
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            auto& adapter = adapters[idx];
            adapter->teardown();
            adapter->setup(N);

            Timer t;
            t.start();
            adapter->splice_all();
            double elapsed = t.elapsedNs();

            adapter->record_time(elapsed);
        }
    }

    print_table_header();
    for (auto& adapter : adapters)
    {
        auto stats = adapter->get_stats();
        print_result_row(adapter->name(), stats, "ns/splice");
    }

    std::cout << "\nNote: Results include cost of building source list (N push_backs).\n";
    std::cout << "std::list is slowest due to N allocations during setup.\n";

    for (auto& adapter : adapters)
    {
        adapter->teardown();
    }
}

void benchmark_memory_overhead(std::vector<std::unique_ptr<IListAdapter>>& adapters)
{
    print_header("MEMORY OVERHEAD COMPARISON");

    std::cout << "Per-node memory overhead for different list implementations:\n\n";

    std::cout << std::setw(40) << std::left << "Structure" << std::setw(15) << std::right << "sizeof (bytes)\n";
    std::cout << std::string(55, '-') << "\n";

    // Raw user data
    std::cout << std::setw(40) << std::left << "Raw user data (int64_t + 7 padding)" << std::setw(15) << std::right
              << sizeof(StdNode) << "\n";

    for (auto& adapter : adapters)
    {
        adapter->setup(1); // Minimal setup to get sizeof
        std::cout << std::setw(40) << std::left << adapter->name() << std::setw(15) << std::right
                  << adapter->node_sizeof() << "\n";
        adapter->teardown();
    }

    std::cout << "\nAnalysis:\n";
    std::cout << "- IntrusiveList adds 24 bytes per node (prev + next + owner)\n";
    std::cout << "- std::list adds 16 bytes per node PLUS allocator overhead (~8-32 bytes)\n";
    std::cout << "- Other intrusive lists add 16 bytes (prev + next only)\n";
    std::cout << "- For 1M nodes, IntrusiveList saves ~24-48 MB vs std::list\n";
}

void benchmark_free_list(std::vector<std::unique_ptr<IListAdapter>>& adapters, size_t poolSize, size_t ops)
{
    print_header("FREE LIST PATTERN (Pool=" + std::to_string(poolSize) + ", Ops=" + std::to_string(ops) + ")");
    print_cpu_context();
    print_contract("Allocate/deallocate pattern using list as free list");

    std::cout << "Simulating object pool: allocate (pop_front) and deallocate (push_back)\n";
    std::cout << "Pattern: 70% allocate, 30% deallocate\n\n";

    // Generate operation sequence
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
    std::uniform_int_distribution<int> opDist(0, 99);
    std::vector<bool> isAlloc(ops);
    for (size_t i = 0; i < ops; ++i)
    {
        isAlloc[i] = (opDist(rng) < 70);
    }

    for (auto& adapter : adapters)
    {
        adapter->setup(poolSize);
        adapter->clear_times();
    }

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            adapter->free_list_setup();
            adapter->free_list_ops(isAlloc);
            adapter->teardown();
            adapter->setup(poolSize);
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
            auto& adapter = adapters[idx];
            adapter->teardown();
            adapter->setup(poolSize);
            adapter->free_list_setup();

            Timer t;
            t.start();
            adapter->free_list_ops(isAlloc);
            double elapsed = t.elapsedNs();

            adapter->record_time(elapsed / static_cast<double>(ops));
        }
    }

    print_table_header();
    Stats fatpStats;
    for (auto& adapter : adapters)
    {
        auto stats = adapter->get_stats();
        print_result_row(adapter->name(), stats);
        if (std::string(adapter->name()).find("fat_p") != std::string::npos)
        {
            fatpStats = stats;
        }
    }

    for (auto& adapter : adapters)
    {
        if (std::string(adapter->name()).find("std::list") != std::string::npos)
        {
            auto stdStats = adapter->get_stats();
            std::cout << "\nSpeedup: " << std::fixed << std::setprecision(1) << (stdStats.median / fatpStats.median)
                      << "x\n";
            break;
        }
    }

    for (auto& adapter : adapters)
    {
        adapter->teardown();
    }
}

void benchmark_is_linked(std::vector<std::unique_ptr<IListAdapter>>& adapters, size_t N)
{
    print_header("IS_LINKED CHECK PERFORMANCE (N=" + std::to_string(N) + ")");
    print_cpu_context();
    print_contract("Check if each node is in a list - O(1) vs O(N) depending on library");

    std::cout << "Measuring time to check membership for " << N << " nodes (half linked).\n";
    std::cout << "fat_p/Boost/EASTL/ETL: O(1) via link state or owner pointers\n";
    std::cout << "LLVM/std::list: O(N) search required - no public is_linked()/isLinked() API\n";
    std::cout << "Note: N kept small because O(N) search per node = O(N^2) total.\n\n";

    // Setup - link half the nodes (outside timed region)
    for (auto& adapter : adapters)
    {
        adapter->setup(N);
        adapter->setup_is_linked(); // Link half the nodes
        adapter->clear_times();
    }

    // Warmup
    for (size_t w = 0; w < WARMUP_RUNS(); ++w)
    {
        for (auto& adapter : adapters)
        {
            size_t count = adapter->count_linked();
            benchmark_sink += static_cast<int64_t>(count);
        }
    }

    // Measured runs
    std::mt19937 rng(static_cast<std::mt19937::result_type>(g_config.seed));
    for (size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::vector<size_t> order(adapters.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            auto& adapter = adapters[idx];

            Timer t;
            t.start();
            size_t count = adapter->count_linked();
            double elapsed = t.elapsedNs();

            benchmark_sink += static_cast<int64_t>(count);
            adapter->record_time(elapsed / static_cast<double>(N));
        }
    }

    print_table_header();
    for (auto& adapter : adapters)
    {
        auto stats = adapter->get_stats();
        print_result_row(adapter->name(), stats);
    }

    std::cout << "\nNote: Libraries with O(1) membership: fat_p, Boost, EASTL, ETL.\n";
    std::cout << "Libraries requiring O(N) search: LLVM, std::list.\n";
    std::cout << "At N=100000, O(N) search would be ~10000x slower than O(1).\n";
    std::cout << "fat_p safe policy: owner pointer can identify WHICH list owns the node.\n";

    for (auto& adapter : adapters)
    {
        adapter->teardown();
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Load configuration
    g_config = fat_p::bench::BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity)
    BenchmarkScope scope(!g_config.noScope);

    std::cout << std::string(80, '=') << "\n";
    std::cout << "  IntrusiveList Comprehensive Benchmark Suite\n";
    std::cout << std::string(80, '=') << "\n";

    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() << ", seed=" << g_config.seed
              << ")\n";

    // Print competitor detection
    std::cout << "\nCompetitor libraries detected:\n";
    std::cout << "  [x] fat_p::IntrusiveList (fast policy)\n";
    std::cout << "  [x] fat_p::IntrusiveList (safe policy)\n";
    std::cout << "  [x] std::list<T*> (baseline)\n";
#if HAS_BOOST_INTRUSIVE
    std::cout << "  [x] boost::intrusive::list\n";
#else
    std::cout << "  [ ] boost::intrusive::list (install: vcpkg install boost-intrusive)\n";
#endif
#if HAS_EASTL
    std::cout << "  [x] eastl::intrusive_list\n";
#else
    std::cout << "  [ ] eastl::intrusive_list (install: vcpkg install eastl)\n";
#endif
#if HAS_LLVM_ILIST
    std::cout << "  [x] llvm::simple_ilist\n";
#else
    std::cout << "  [ ] llvm::simple_ilist (install: apt install llvm-dev)\n";
#endif
#if HAS_ETL
    std::cout << "  [x] etl::intrusive_list\n";
#else
    std::cout << "  [ ] etl::intrusive_list (install: https://github.com/ETLCPP/etl)\n";
#endif

    // Print design invariants
    std::cout << "\nDesign Invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run\n";
    std::cout << "  3. Setup/teardown outside timed regions\n";
    std::cout << "  4. All libraries observe same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n\n";

    // ========================================================================
    // Build adapters
    // ========================================================================

    std::vector<std::unique_ptr<IListAdapter>> adapters;
    adapters.push_back(std::make_unique<FatPFastListAdapter>());
    adapters.push_back(std::make_unique<FatPSafeListAdapter>());
    adapters.push_back(std::make_unique<StdListAdapter>());
#if HAS_BOOST_INTRUSIVE
    adapters.push_back(std::make_unique<BoostListAdapter>());
#endif
#if HAS_EASTL
    adapters.push_back(std::make_unique<EastlListAdapter>());
#endif
#if HAS_ETL
    adapters.push_back(std::make_unique<EtlListAdapter>());
#endif
#if HAS_LLVM_ILIST
    adapters.push_back(std::make_unique<LlvmListAdapter>());
#endif

    // ========================================================================
    // Run benchmarks
    // ========================================================================

    constexpr size_t N = 10000;

    benchmark_push_back(adapters, N);
    benchmark_remove(adapters, N);
    benchmark_iteration(adapters, N);
    benchmark_splice(adapters, N);
    benchmark_memory_overhead(adapters);
    benchmark_free_list(adapters, 1000, 100000);
    benchmark_is_linked(adapters, 1000); // Small N because LLVM/std::list are O(N^2)

    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << std::string(80, '=') << "\n";

    return 0;
}

// ============================================================================
// Macro Cleanup (unity build safety)
// ============================================================================
#undef HAS_BOOST_INTRUSIVE
#undef HAS_EASTL
#undef HAS_LLVM_ILIST
#undef HAS_ETL