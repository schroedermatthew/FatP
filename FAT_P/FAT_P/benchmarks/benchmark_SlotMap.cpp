// benchmark_SlotMap.cpp
//
// FAT-P SlotMap benchmarks using unified FatPBenchmarkRunner infrastructure.
//
// Architecture: Round-robin execution with randomized order per run.
// This ensures all libraries observe the same distribution of machine states,
// eliminating drift-induced unfairness.
//
// Design Invariants:
//   1. Each measured run executes exactly one timed iteration per library.
//   2. Library execution order is randomized per run.
//   3. Setup, reserve, and teardown occur outside timed regions.
//   4. All libraries observe the same distribution of machine states.
//   5. Medians are the primary reported statistic.
//
// Fat-P Libraries:
//   - fat_p::SlotMap: Generational index container with O(1) insert/erase/access
//                     Dense storage for cache-friendly iteration
//                     ABA-safe handles via generation counters
//
// Competitor Libraries (conditioned on availability):
//   TIER 1 - Direct competitors (generational/handle-based):
//     - entt::basic_sparse_set, dod::slot_map, plf::hive
//   TIER 2 - Standard library alternatives:
//     - std::unordered_map<uint64_t, T>, std::map<uint64_t, T>
//   TIER 3 - Baseline reference:
//     - std::vector<T> + index
//
// Build:
//   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_SlotMap.cpp -o bench_sm
//   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_SlotMap.cpp
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS()   - Warmup iterations (default: 3)
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
//   ./bench_sm
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_sm

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "FatPBenchmarkRunner.h"

// ============================================================================
// Library Detection
// ============================================================================

// Fat-P SlotMap
#if __has_include("SlotMap.h")
#include "SlotMap.h"
#define HAS_FATP_SLOTMAP 1
#else
#define HAS_FATP_SLOTMAP 0
#endif

// EnTT - Popular ECS library with sparse_set
// Install: vcpkg install entt, or header-only from github.com/skypjack/entt
#if __has_include(<entt/entt.hpp>)
#include <entt/entt.hpp>
#define HAS_ENTT 1
#elif __has_include(<entt/entity/sparse_set.hpp>)
#include <entt/entity/sparse_set.hpp>
#include <entt/entity/storage.hpp>
#define HAS_ENTT 1
#else
#define HAS_ENTT 0
#endif

// plf::hive (formerly plf::colony) - Stable pointer container
// Install: header-only from github.com/mattreecebentley/plf_hive
#if __has_include(<plf_hive.h>)
#include <plf_hive.h>
#define HAS_PLF_HIVE 1
#elif __has_include("plf_hive.h")
#include "plf_hive.h"
#define HAS_PLF_HIVE 1
#else
#define HAS_PLF_HIVE 0
#endif

// SG14 slot_map - WG21 study group reference implementation
// Install: header-only from github.com/WG21-SG14/SG14
#if __has_include(<sg14/slot_map.h>)
#include <sg14/slot_map.h>
#define HAS_SG14_SLOTMAP 1
#elif __has_include("sg14_slot_map.h")
#include "sg14_slot_map.h"
#define HAS_SG14_SLOTMAP 1
#else
#define HAS_SG14_SLOTMAP 0
#endif

// Boost.Container stable_vector - Pointer-stable vector
#if __has_include(<boost/container/stable_vector.hpp>)
#include <boost/container/stable_vector.hpp>
#define HAS_BOOST_STABLE_VECTOR 1
#else
#define HAS_BOOST_STABLE_VECTOR 0
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

static inline void prevent_opt(int64_t value)
{
    benchmark_sink ^= value;
}

// ============================================================================
// CPU Frequency Stability Monitoring
// ============================================================================

static inline void cpu_warmup_burst(int milliseconds)
{
    if (milliseconds <= 0) return;
    
    auto start = std::chrono::steady_clock::now();
    auto duration = std::chrono::milliseconds(milliseconds);
    
    volatile uint64_t sink = 0;
    volatile uint64_t x = 0xDEADBEEFCAFEBABEULL;
    
    while (std::chrono::steady_clock::now() - start < duration)
    {
        for (int i = 0; i < 1000; ++i)
        {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            sink += x;
        }
    }
    
    benchmark_sink ^= static_cast<int64_t>(sink);
}

static bool wait_for_cpu_stable(
    double max_variance_percent = 10.0,
    int timeout_seconds = 30,
    int check_interval_ms = 200,
    bool verbose = true)
{
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);
    
    auto initial_info = fat_p::bench::capture_cpu_frequency();
    if (!initial_info.has_reliable_detection())
    {
        if (verbose)
        {
            std::cout << "[CPU frequency detection unavailable - using fixed cooling delay]\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return true;
    }
    
    double base_freq = initial_info.mRefFreqMHz;
    double min_required_freq = base_freq * 0.60;
    
    cpu_warmup_burst(100);
    
    std::vector<double> recent_readings;
    const size_t window_size = 5;
    const int required_stable = 3;
    int stable_count = 0;
    
    while (std::chrono::steady_clock::now() - start < timeout)
    {
        cpu_warmup_burst(50);
        
        auto info = fat_p::bench::capture_cpu_frequency();
        recent_readings.push_back(info.mCurrentFreqMHz);
        
        if (recent_readings.size() > window_size)
        {
            recent_readings.erase(recent_readings.begin());
        }
        
        if (recent_readings.size() >= window_size)
        {
            double min_freq = *std::min_element(recent_readings.begin(), recent_readings.end());
            double max_freq = *std::max_element(recent_readings.begin(), recent_readings.end());
            double avg_freq = 0;
            for (double f : recent_readings) avg_freq += f;
            avg_freq /= recent_readings.size();
            
            double variance_pct = (max_freq - min_freq) / avg_freq * 100.0;
            bool variance_ok = variance_pct <= max_variance_percent;
            bool freq_floor_ok = avg_freq >= min_required_freq;
            bool is_stable = variance_ok && freq_floor_ok;
            
            if (is_stable)
            {
                ++stable_count;
                if (stable_count >= required_stable)
                {
                    if (verbose)
                    {
                        double pct_of_base = (avg_freq / base_freq) * 100.0;
                        std::cout << "[CPU stable at " << static_cast<int>(avg_freq) 
                                  << " MHz (" << std::fixed << std::setprecision(0) << pct_of_base 
                                  << "% of base)]\n";
                    }
                    return true;
                }
            }
            else
            {
                stable_count = 0;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    }
    
    if (verbose)
    {
        std::cout << "[WARNING: CPU frequency still unstable after " << timeout_seconds << "s]\n";
    }
    return false;
}

static inline void cooling_delay(int min_sleep_ms, const char* reason = nullptr)
{
    if (reason)
    {
        std::cout << "[Cooling: " << reason << "]" << std::flush;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(min_sleep_ms));
    wait_for_cpu_stable(10.0, 15, 200, false);
    
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

#if defined(_WIN32) || defined(_WIN64)
static constexpr int COOLING_DELAY_SECTION_MS = 2000;
static constexpr int COOLING_DELAY_SIZE_MS = 1000;
static constexpr int COOLING_DELAY_CASE_MS = 300;
#else
static constexpr int COOLING_DELAY_SECTION_MS = 1000;
static constexpr int COOLING_DELAY_SIZE_MS = 500;
static constexpr int COOLING_DELAY_CASE_MS = 200;
#endif

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
// Test Value Type
// ============================================================================

// Simple value type to benchmark with - represents a game entity or similar
struct TestValue
{
    int64_t id;
    double x, y, z;
    int32_t health;
    int32_t flags;
    
    TestValue() : id(0), x(0), y(0), z(0), health(100), flags(0) {}
    explicit TestValue(int64_t i) 
        : id(i), x(static_cast<double>(i)), y(static_cast<double>(i) * 0.5), 
          z(static_cast<double>(i) * 0.25), health(100), flags(0) {}
    
    int64_t checksum() const 
    { 
        return id + static_cast<int64_t>(x + y + z) + health + flags; 
    }
};

// ============================================================================
// Benchmark Case Enum
// ============================================================================

enum class Case
{
    SequentialInsert,    // Insert N elements sequentially
    RandomAccessValid,   // Access N valid handles
    RandomAccessMixed,   // Access mix of valid/invalid handles (ABA test)
    Iteration,           // Iterate over all elements
    Erase25Percent,      // Erase 25% of elements
    EraseAll,            // Erase all elements one by one
    MixedWorkload,       // Interleaved insert/erase/access
    SlotReuse            // Insert, erase, insert again (tests slot reuse)
};

static inline const char* case_name(Case c)
{
    switch (c)
    {
    case Case::SequentialInsert:   return "Sequential Insert";
    case Case::RandomAccessValid:  return "Random Access (valid)";
    case Case::RandomAccessMixed:  return "Random Access (mixed valid/invalid)";
    case Case::Iteration:          return "Iteration";
    case Case::Erase25Percent:     return "Erase (25%)";
    case Case::EraseAll:           return "Erase (all)";
    case Case::MixedWorkload:      return "Mixed Workload";
    case Case::SlotReuse:          return "Slot Reuse";
    }
    return "Unknown";
}

// ============================================================================
// Shared Inputs
// ============================================================================

struct Inputs
{
    size_t N;
    std::vector<int64_t> values;            // Values to insert
    std::vector<size_t> access_indices;     // Random indices for access benchmarks
    std::vector<size_t> erase_indices;      // Indices of elements to erase (25%)
    uint64_t seed;

    static Inputs make(size_t n, uint64_t seed = 0x5107CAFEBABE01ULL)
    {
        Inputs in;
        in.N = n;
        in.seed = seed;

        // Generate values
        in.values.resize(n);
        std::mt19937_64 rng(seed);
        for (size_t i = 0; i < n; ++i)
        {
            in.values[i] = static_cast<int64_t>(i * 1000 + rng() % 1000);
        }

        // Random access indices
        in.access_indices.resize(n);
        std::iota(in.access_indices.begin(), in.access_indices.end(), 0);
        std::shuffle(in.access_indices.begin(), in.access_indices.end(), rng);

        // 25% subset for erase
        in.erase_indices = in.access_indices;
        const size_t erase_count = std::max<size_t>(1, n / 4);
        if (in.erase_indices.size() > erase_count)
        {
            in.erase_indices.resize(erase_count);
        }

        return in;
    }
};

// ============================================================================
// Adapter Interface
// ============================================================================

struct ISlotMapAdapter
{
    virtual ~ISlotMapAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t N) = 0;
    virtual void teardown() = 0;
    virtual void clear() = 0;
    virtual void preload(const Inputs& in) = 0;
    virtual size_t run_operation(Case c, const Inputs& in) = 0;
};

// ============================================================================
// Fat-P SlotMap Adapter
// ============================================================================

#if HAS_FATP_SLOTMAP
class FatPSlotMapAdapter final : public ISlotMapAdapter
{
    std::unique_ptr<fat_p::SlotMap<TestValue>> map_;
    std::vector<fat_p::SlotMapHandle> handles_;
    std::vector<fat_p::SlotMapHandle> invalid_handles_;

public:
    const char* name() const override { return "fat_p::SlotMap"; }

    void setup(size_t N) override
    {
        map_ = std::make_unique<fat_p::SlotMap<TestValue>>();
        map_->reserve(static_cast<uint32_t>(N));
        handles_.clear();
        handles_.reserve(N);
        invalid_handles_.clear();
    }

    void teardown() override 
    { 
        map_.reset(); 
        handles_.clear();
        invalid_handles_.clear();
    }

    void clear() override 
    { 
        map_->clear(); 
        handles_.clear();
        invalid_handles_.clear();
    }

    void preload(const Inputs& in) override
    {
        handles_.clear();
        handles_.reserve(in.N);
        for (int64_t v : in.values)
        {
            handles_.push_back(map_->insert(TestValue(v)));
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        std::mt19937_64 rng(in.seed ^ 0x12345);

        switch (c)
        {
        case Case::SequentialInsert:
            handles_.clear();
            for (int64_t v : in.values)
            {
                handles_.push_back(map_->insert(TestValue(v)));
                ++ops;
            }
            break;

        case Case::RandomAccessValid:
            for (size_t idx : in.access_indices)
            {
                if (idx < handles_.size())
                {
                    if (auto* ptr = map_->get(handles_[idx]))
                    {
                        benchmark_sink += ptr->checksum();
                    }
                }
                ++ops;
            }
            break;

        case Case::RandomAccessMixed:
            // Create some invalid handles first
            if (invalid_handles_.empty())
            {
                size_t to_invalidate = std::min<size_t>(in.N / 4, handles_.size());
                for (size_t i = 0; i < to_invalidate; ++i)
                {
                    invalid_handles_.push_back(handles_[i]);
                    map_->erase(handles_[i]);
                }
                // Re-insert to reuse slots (handles now invalid)
                for (size_t i = 0; i < to_invalidate; ++i)
                {
                    handles_[i] = map_->insert(TestValue(in.values[i] + 1000000));
                }
            }
            
            // Access mix of valid and invalid
            for (size_t i = 0; i < in.access_indices.size(); ++i)
            {
                size_t idx = in.access_indices[i];
                if (i % 4 == 0 && idx < invalid_handles_.size())
                {
                    // Try invalid handle (should return nullptr)
                    auto* ptr = map_->get(invalid_handles_[idx]);
                    if (ptr) benchmark_sink += ptr->checksum();
                }
                else if (idx < handles_.size())
                {
                    if (auto* ptr = map_->get(handles_[idx]))
                    {
                        benchmark_sink += ptr->checksum();
                    }
                }
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& val : *map_)
            {
                benchmark_sink += val.checksum();
                ++ops;
            }
            break;

        case Case::Erase25Percent:
            for (size_t idx : in.erase_indices)
            {
                if (idx < handles_.size())
                {
                    map_->erase(handles_[idx]);
                }
                ++ops;
            }
            break;

        case Case::EraseAll:
            for (auto& h : handles_)
            {
                map_->erase(h);
                ++ops;
            }
            handles_.clear();
            break;

        case Case::MixedWorkload:
        {
            size_t batch = std::max<size_t>(1, in.N / 10);
            
            // Insert batch
            for (size_t i = 0; i < batch && i < in.values.size(); ++i)
            {
                handles_.push_back(map_->insert(TestValue(in.values[i])));
                ++ops;
            }
            
            // Access batch
            for (size_t i = 0; i < batch && i < handles_.size(); ++i)
            {
                size_t idx = rng() % handles_.size();
                if (auto* ptr = map_->get(handles_[idx]))
                {
                    benchmark_sink += ptr->checksum();
                }
                ++ops;
            }
            
            // Erase half the batch
            size_t erase_count = std::min(batch / 2, handles_.size());
            for (size_t i = 0; i < erase_count; ++i)
            {
                if (!handles_.empty())
                {
                    size_t idx = rng() % handles_.size();
                    map_->erase(handles_[idx]);
                    handles_[idx] = handles_.back();
                    handles_.pop_back();
                }
                ++ops;
            }
            break;
        }

        case Case::SlotReuse:
        {
            // Insert, erase half, insert again to test slot reuse
            size_t half = in.N / 2;
            
            // Insert all
            handles_.clear();
            for (size_t i = 0; i < in.N && i < in.values.size(); ++i)
            {
                handles_.push_back(map_->insert(TestValue(in.values[i])));
                ++ops;
            }
            
            // Erase first half
            for (size_t i = 0; i < half && i < handles_.size(); ++i)
            {
                map_->erase(handles_[i]);
                ++ops;
            }
            
            // Insert again (should reuse slots)
            for (size_t i = 0; i < half && i < in.values.size(); ++i)
            {
                handles_.push_back(map_->insert(TestValue(in.values[i] + 1000000)));
                ++ops;
            }
            break;
        }
        }
        return ops;
    }
};
#endif // HAS_FATP_SLOTMAP

// ============================================================================
// EnTT Adapter (ECS sparse_set/storage pattern)
// ============================================================================

#if HAS_ENTT
class EnTTAdapter final : public ISlotMapAdapter
{
    // EnTT uses entity IDs (uint32_t by default) with version bits for ABA protection
    entt::basic_registry<entt::entity> registry_;
    std::vector<entt::entity> entities_;

public:
    const char* name() const override { return "entt::registry"; }

    void setup(size_t N) override
    {
        registry_ = entt::basic_registry<entt::entity>{};
        // EnTT doesn't have a direct reserve, but we can hint
        entities_.clear();
        entities_.reserve(N);
    }

    void teardown() override 
    { 
        registry_.clear();
        entities_.clear();
    }

    void clear() override 
    { 
        registry_.clear();
        entities_.clear();
    }

    void preload(const Inputs& in) override
    {
        entities_.clear();
        entities_.reserve(in.N);
        for (int64_t v : in.values)
        {
            auto e = registry_.create();
            registry_.emplace<TestValue>(e, TestValue(v));
            entities_.push_back(e);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        std::mt19937_64 rng(in.seed ^ 0x12345);

        switch (c)
        {
        case Case::SequentialInsert:
            entities_.clear();
            for (int64_t v : in.values)
            {
                auto e = registry_.create();
                registry_.emplace<TestValue>(e, TestValue(v));
                entities_.push_back(e);
                ++ops;
            }
            break;

        case Case::RandomAccessValid:
            for (size_t idx : in.access_indices)
            {
                if (idx < entities_.size())
                {
                    auto e = entities_[idx];
                    if (registry_.valid(e))
                    {
                        auto* ptr = registry_.try_get<TestValue>(e);
                        if (ptr) benchmark_sink += ptr->checksum();
                    }
                }
                ++ops;
            }
            break;

        case Case::RandomAccessMixed:
            // EnTT has built-in versioning for ABA protection
            for (size_t idx : in.access_indices)
            {
                if (idx < entities_.size())
                {
                    auto e = entities_[idx];
                    // registry_.valid() checks version
                    if (registry_.valid(e))
                    {
                        auto* ptr = registry_.try_get<TestValue>(e);
                        if (ptr) benchmark_sink += ptr->checksum();
                    }
                }
                ++ops;
            }
            break;

        case Case::Iteration:
        {
            auto view = registry_.view<TestValue>();
            for (auto e : view)
            {
                auto& val = view.get<TestValue>(e);
                benchmark_sink += val.checksum();
                ++ops;
            }
            break;
        }

        case Case::Erase25Percent:
            for (size_t idx : in.erase_indices)
            {
                if (idx < entities_.size())
                {
                    auto e = entities_[idx];
                    if (registry_.valid(e))
                    {
                        registry_.destroy(e);
                    }
                }
                ++ops;
            }
            break;

        case Case::EraseAll:
            for (auto e : entities_)
            {
                if (registry_.valid(e))
                {
                    registry_.destroy(e);
                }
                ++ops;
            }
            entities_.clear();
            break;

        case Case::MixedWorkload:
        {
            size_t batch = std::max<size_t>(1, in.N / 10);
            
            for (size_t i = 0; i < batch && i < in.values.size(); ++i)
            {
                auto e = registry_.create();
                registry_.emplace<TestValue>(e, TestValue(in.values[i]));
                entities_.push_back(e);
                ++ops;
            }
            
            for (size_t i = 0; i < batch && i < entities_.size(); ++i)
            {
                size_t idx = rng() % entities_.size();
                auto e = entities_[idx];
                if (registry_.valid(e))
                {
                    auto* ptr = registry_.try_get<TestValue>(e);
                    if (ptr) benchmark_sink += ptr->checksum();
                }
                ++ops;
            }
            
            size_t erase_count = std::min(batch / 2, entities_.size());
            for (size_t i = 0; i < erase_count; ++i)
            {
                if (!entities_.empty())
                {
                    size_t idx = rng() % entities_.size();
                    auto e = entities_[idx];
                    if (registry_.valid(e))
                    {
                        registry_.destroy(e);
                    }
                    entities_[idx] = entities_.back();
                    entities_.pop_back();
                }
                ++ops;
            }
            break;
        }

        case Case::SlotReuse:
        {
            size_t half = in.N / 2;
            
            entities_.clear();
            for (size_t i = 0; i < in.N && i < in.values.size(); ++i)
            {
                auto e = registry_.create();
                registry_.emplace<TestValue>(e, TestValue(in.values[i]));
                entities_.push_back(e);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < entities_.size(); ++i)
            {
                if (registry_.valid(entities_[i]))
                {
                    registry_.destroy(entities_[i]);
                }
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < in.values.size(); ++i)
            {
                auto e = registry_.create();
                registry_.emplace<TestValue>(e, TestValue(in.values[i] + 1000000));
                entities_.push_back(e);
                ++ops;
            }
            break;
        }
        }
        return ops;
    }
};
#endif // HAS_ENTT

// ============================================================================
// plf::hive Adapter (Stable pointer container)
// ============================================================================

#if HAS_PLF_HIVE
class PlfHiveAdapter final : public ISlotMapAdapter
{
    std::unique_ptr<plf::hive<TestValue>> hive_;
    // plf::hive provides stable pointers but not random access by index
    // We store iterators for handle-like access (though this isn't idiomatic)
    std::vector<plf::hive<TestValue>::iterator> iters_;

public:
    const char* name() const override { return "plf::hive"; }

    void setup(size_t N) override
    {
        hive_ = std::make_unique<plf::hive<TestValue>>();
        hive_->reserve(N);
        iters_.clear();
        iters_.reserve(N);
    }

    void teardown() override 
    { 
        hive_.reset();
        iters_.clear();
    }

    void clear() override 
    { 
        hive_->clear();
        iters_.clear();
    }

    void preload(const Inputs& in) override
    {
        iters_.clear();
        iters_.reserve(in.N);
        for (int64_t v : in.values)
        {
            auto it = hive_->insert(TestValue(v));
            iters_.push_back(it);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        std::mt19937_64 rng(in.seed ^ 0x12345);

        switch (c)
        {
        case Case::SequentialInsert:
            iters_.clear();
            for (int64_t v : in.values)
            {
                auto it = hive_->insert(TestValue(v));
                iters_.push_back(it);
                ++ops;
            }
            break;

        case Case::RandomAccessValid:
        case Case::RandomAccessMixed:
            // plf::hive iterators remain valid after other erasures
            for (size_t idx : in.access_indices)
            {
                if (idx < iters_.size())
                {
                    // Note: No way to check if iterator is still valid in hive
                    // This demonstrates a key difference from SlotMap
                    benchmark_sink += iters_[idx]->checksum();
                }
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& val : *hive_)
            {
                benchmark_sink += val.checksum();
                ++ops;
            }
            break;

        case Case::Erase25Percent:
            for (size_t idx : in.erase_indices)
            {
                if (idx < iters_.size())
                {
                    hive_->erase(iters_[idx]);
                    // Mark as invalid (can't actually check in hive)
                }
                ++ops;
            }
            break;

        case Case::EraseAll:
            hive_->clear();
            iters_.clear();
            ops = in.N;
            break;

        case Case::MixedWorkload:
        {
            size_t batch = std::max<size_t>(1, in.N / 10);
            
            for (size_t i = 0; i < batch && i < in.values.size(); ++i)
            {
                auto it = hive_->insert(TestValue(in.values[i]));
                iters_.push_back(it);
                ++ops;
            }
            
            for (size_t i = 0; i < batch && i < iters_.size(); ++i)
            {
                size_t idx = rng() % iters_.size();
                benchmark_sink += iters_[idx]->checksum();
                ++ops;
            }
            
            size_t erase_count = std::min(batch / 2, iters_.size());
            for (size_t i = 0; i < erase_count; ++i)
            {
                if (!iters_.empty())
                {
                    size_t idx = rng() % iters_.size();
                    hive_->erase(iters_[idx]);
                    iters_[idx] = iters_.back();
                    iters_.pop_back();
                }
                ++ops;
            }
            break;
        }

        case Case::SlotReuse:
        {
            size_t half = in.N / 2;
            
            iters_.clear();
            for (size_t i = 0; i < in.N && i < in.values.size(); ++i)
            {
                auto it = hive_->insert(TestValue(in.values[i]));
                iters_.push_back(it);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < iters_.size(); ++i)
            {
                hive_->erase(iters_[i]);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < in.values.size(); ++i)
            {
                auto it = hive_->insert(TestValue(in.values[i] + 1000000));
                iters_.push_back(it);
                ++ops;
            }
            break;
        }
        }
        return ops;
    }
};
#endif // HAS_PLF_HIVE

// ============================================================================
// SG14 slot_map Adapter (WG21 study group reference)
// ============================================================================

#if HAS_SG14_SLOTMAP
class SG14SlotMapAdapter final : public ISlotMapAdapter
{
    std::unique_ptr<stdext::slot_map<TestValue>> map_;
    std::vector<stdext::slot_map<TestValue>::key_type> keys_;

public:
    const char* name() const override { return "sg14::slot_map"; }

    void setup(size_t N) override
    {
        map_ = std::make_unique<stdext::slot_map<TestValue>>();
        map_->reserve(N);
        keys_.clear();
        keys_.reserve(N);
    }

    void teardown() override 
    { 
        map_.reset();
        keys_.clear();
    }

    void clear() override 
    { 
        map_->clear();
        keys_.clear();
    }

    void preload(const Inputs& in) override
    {
        keys_.clear();
        keys_.reserve(in.N);
        for (int64_t v : in.values)
        {
            auto key = map_->insert(TestValue(v));
            keys_.push_back(key);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        std::mt19937_64 rng(in.seed ^ 0x12345);

        switch (c)
        {
        case Case::SequentialInsert:
            keys_.clear();
            for (int64_t v : in.values)
            {
                auto key = map_->insert(TestValue(v));
                keys_.push_back(key);
                ++ops;
            }
            break;

        case Case::RandomAccessValid:
            for (size_t idx : in.access_indices)
            {
                if (idx < keys_.size())
                {
                    auto it = map_->find(keys_[idx]);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->checksum();
                    }
                }
                ++ops;
            }
            break;

        case Case::RandomAccessMixed:
            // SG14 slot_map has generational indices like fat_p::SlotMap
            for (size_t idx : in.access_indices)
            {
                if (idx < keys_.size())
                {
                    auto it = map_->find(keys_[idx]);
                    if (it != map_->end())
                    {
                        benchmark_sink += it->checksum();
                    }
                }
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& val : *map_)
            {
                benchmark_sink += val.checksum();
                ++ops;
            }
            break;

        case Case::Erase25Percent:
            for (size_t idx : in.erase_indices)
            {
                if (idx < keys_.size())
                {
                    map_->erase(keys_[idx]);
                }
                ++ops;
            }
            break;

        case Case::EraseAll:
            for (auto& k : keys_)
            {
                map_->erase(k);
                ++ops;
            }
            keys_.clear();
            break;

        case Case::MixedWorkload:
        {
            size_t batch = std::max<size_t>(1, in.N / 10);
            
            for (size_t i = 0; i < batch && i < in.values.size(); ++i)
            {
                auto key = map_->insert(TestValue(in.values[i]));
                keys_.push_back(key);
                ++ops;
            }
            
            for (size_t i = 0; i < batch && i < keys_.size(); ++i)
            {
                size_t idx = rng() % keys_.size();
                auto it = map_->find(keys_[idx]);
                if (it != map_->end())
                {
                    benchmark_sink += it->checksum();
                }
                ++ops;
            }
            
            size_t erase_count = std::min(batch / 2, keys_.size());
            for (size_t i = 0; i < erase_count; ++i)
            {
                if (!keys_.empty())
                {
                    size_t idx = rng() % keys_.size();
                    map_->erase(keys_[idx]);
                    keys_[idx] = keys_.back();
                    keys_.pop_back();
                }
                ++ops;
            }
            break;
        }

        case Case::SlotReuse:
        {
            size_t half = in.N / 2;
            
            keys_.clear();
            for (size_t i = 0; i < in.N && i < in.values.size(); ++i)
            {
                auto key = map_->insert(TestValue(in.values[i]));
                keys_.push_back(key);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < keys_.size(); ++i)
            {
                map_->erase(keys_[i]);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < in.values.size(); ++i)
            {
                auto key = map_->insert(TestValue(in.values[i] + 1000000));
                keys_.push_back(key);
                ++ops;
            }
            break;
        }
        }
        return ops;
    }
};
#endif // HAS_SG14_SLOTMAP

// ============================================================================
// std::unordered_map<uint64_t, T> Adapter (Hash-based baseline)
// ============================================================================

class UnorderedMapAdapter final : public ISlotMapAdapter
{
    std::unique_ptr<std::unordered_map<uint64_t, TestValue>> map_;
    std::vector<uint64_t> keys_;
    uint64_t next_key_ = 0;

public:
    const char* name() const override { return "std::unordered_map"; }

    void setup(size_t N) override
    {
        map_ = std::make_unique<std::unordered_map<uint64_t, TestValue>>();
        map_->reserve(N);
        keys_.clear();
        keys_.reserve(N);
        next_key_ = 0;
    }

    void teardown() override 
    { 
        map_.reset(); 
        keys_.clear();
    }

    void clear() override 
    { 
        map_->clear(); 
        keys_.clear();
        next_key_ = 0;
    }

    void preload(const Inputs& in) override
    {
        keys_.clear();
        keys_.reserve(in.N);
        for (int64_t v : in.values)
        {
            uint64_t key = next_key_++;
            map_->emplace(key, TestValue(v));
            keys_.push_back(key);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        std::mt19937_64 rng(in.seed ^ 0x12345);

        switch (c)
        {
        case Case::SequentialInsert:
            keys_.clear();
            for (int64_t v : in.values)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(v));
                keys_.push_back(key);
                ++ops;
            }
            break;

        case Case::RandomAccessValid:
        case Case::RandomAccessMixed:  // No ABA safety, so same as valid
            for (size_t idx : in.access_indices)
            {
                if (idx < keys_.size())
                {
                    auto it = map_->find(keys_[idx]);
                    if (it != map_->end()) benchmark_sink += it->second.checksum();
                }
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& [key, val] : *map_)
            {
                benchmark_sink += val.checksum();
                ++ops;
            }
            break;

        case Case::Erase25Percent:
            for (size_t idx : in.erase_indices)
            {
                if (idx < keys_.size())
                {
                    map_->erase(keys_[idx]);
                }
                ++ops;
            }
            break;

        case Case::EraseAll:
            for (uint64_t k : keys_)
            {
                map_->erase(k);
                ++ops;
            }
            keys_.clear();
            break;

        case Case::MixedWorkload:
        {
            size_t batch = std::max<size_t>(1, in.N / 10);
            
            for (size_t i = 0; i < batch && i < in.values.size(); ++i)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(in.values[i]));
                keys_.push_back(key);
                ++ops;
            }
            
            for (size_t i = 0; i < batch && i < keys_.size(); ++i)
            {
                size_t idx = rng() % keys_.size();
                auto it = map_->find(keys_[idx]);
                if (it != map_->end()) benchmark_sink += it->second.checksum();
                ++ops;
            }
            
            size_t erase_count = std::min(batch / 2, keys_.size());
            for (size_t i = 0; i < erase_count; ++i)
            {
                if (!keys_.empty())
                {
                    size_t idx = rng() % keys_.size();
                    map_->erase(keys_[idx]);
                    keys_[idx] = keys_.back();
                    keys_.pop_back();
                }
                ++ops;
            }
            break;
        }

        case Case::SlotReuse:
        {
            size_t half = in.N / 2;
            
            keys_.clear();
            for (size_t i = 0; i < in.N && i < in.values.size(); ++i)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(in.values[i]));
                keys_.push_back(key);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < keys_.size(); ++i)
            {
                map_->erase(keys_[i]);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < in.values.size(); ++i)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(in.values[i] + 1000000));
                keys_.push_back(key);
                ++ops;
            }
            break;
        }
        }
        return ops;
    }
};

// ============================================================================
// std::map<uint64_t, T> Adapter (Tree-based baseline)
// ============================================================================

class StdMapAdapter final : public ISlotMapAdapter
{
    std::unique_ptr<std::map<uint64_t, TestValue>> map_;
    std::vector<uint64_t> keys_;
    uint64_t next_key_ = 0;

public:
    const char* name() const override { return "std::map"; }

    void setup(size_t) override
    {
        map_ = std::make_unique<std::map<uint64_t, TestValue>>();
        keys_.clear();
        next_key_ = 0;
    }

    void teardown() override 
    { 
        map_.reset(); 
        keys_.clear();
    }

    void clear() override 
    { 
        map_->clear(); 
        keys_.clear();
        next_key_ = 0;
    }

    void preload(const Inputs& in) override
    {
        keys_.clear();
        keys_.reserve(in.N);
        for (int64_t v : in.values)
        {
            uint64_t key = next_key_++;
            map_->emplace(key, TestValue(v));
            keys_.push_back(key);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        std::mt19937_64 rng(in.seed ^ 0x12345);

        switch (c)
        {
        case Case::SequentialInsert:
            keys_.clear();
            for (int64_t v : in.values)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(v));
                keys_.push_back(key);
                ++ops;
            }
            break;

        case Case::RandomAccessValid:
        case Case::RandomAccessMixed:
            for (size_t idx : in.access_indices)
            {
                if (idx < keys_.size())
                {
                    auto it = map_->find(keys_[idx]);
                    if (it != map_->end()) benchmark_sink += it->second.checksum();
                }
                ++ops;
            }
            break;

        case Case::Iteration:
            for (const auto& [key, val] : *map_)
            {
                benchmark_sink += val.checksum();
                ++ops;
            }
            break;

        case Case::Erase25Percent:
            for (size_t idx : in.erase_indices)
            {
                if (idx < keys_.size())
                {
                    map_->erase(keys_[idx]);
                }
                ++ops;
            }
            break;

        case Case::EraseAll:
            for (uint64_t k : keys_)
            {
                map_->erase(k);
                ++ops;
            }
            keys_.clear();
            break;

        case Case::MixedWorkload:
        {
            size_t batch = std::max<size_t>(1, in.N / 10);
            
            for (size_t i = 0; i < batch && i < in.values.size(); ++i)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(in.values[i]));
                keys_.push_back(key);
                ++ops;
            }
            
            for (size_t i = 0; i < batch && i < keys_.size(); ++i)
            {
                size_t idx = rng() % keys_.size();
                auto it = map_->find(keys_[idx]);
                if (it != map_->end()) benchmark_sink += it->second.checksum();
                ++ops;
            }
            
            size_t erase_count = std::min(batch / 2, keys_.size());
            for (size_t i = 0; i < erase_count; ++i)
            {
                if (!keys_.empty())
                {
                    size_t idx = rng() % keys_.size();
                    map_->erase(keys_[idx]);
                    keys_[idx] = keys_.back();
                    keys_.pop_back();
                }
                ++ops;
            }
            break;
        }

        case Case::SlotReuse:
        {
            size_t half = in.N / 2;
            
            keys_.clear();
            for (size_t i = 0; i < in.N && i < in.values.size(); ++i)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(in.values[i]));
                keys_.push_back(key);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < keys_.size(); ++i)
            {
                map_->erase(keys_[i]);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < in.values.size(); ++i)
            {
                uint64_t key = next_key_++;
                map_->emplace(key, TestValue(in.values[i] + 1000000));
                keys_.push_back(key);
                ++ops;
            }
            break;
        }
        }
        return ops;
    }
};

// ============================================================================
// std::vector<T> Adapter (Raw baseline - no handle safety)
// ============================================================================

class VectorAdapter final : public ISlotMapAdapter
{
    std::unique_ptr<std::vector<TestValue>> vec_;
    std::vector<bool> valid_;  // Track which slots are valid (simple free list)

public:
    const char* name() const override { return "std::vector (raw)"; }

    void setup(size_t N) override
    {
        vec_ = std::make_unique<std::vector<TestValue>>();
        vec_->reserve(N);
        valid_.clear();
        valid_.reserve(N);
    }

    void teardown() override 
    { 
        vec_.reset(); 
        valid_.clear();
    }

    void clear() override 
    { 
        vec_->clear(); 
        valid_.clear();
    }

    void preload(const Inputs& in) override
    {
        vec_->clear();
        valid_.clear();
        for (int64_t v : in.values)
        {
            vec_->emplace_back(TestValue(v));
            valid_.push_back(true);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        std::mt19937_64 rng(in.seed ^ 0x12345);

        switch (c)
        {
        case Case::SequentialInsert:
            vec_->clear();
            valid_.clear();
            for (int64_t v : in.values)
            {
                vec_->emplace_back(TestValue(v));
                valid_.push_back(true);
                ++ops;
            }
            break;

        case Case::RandomAccessValid:
        case Case::RandomAccessMixed:
            for (size_t idx : in.access_indices)
            {
                if (idx < vec_->size() && valid_[idx])
                {
                    benchmark_sink += (*vec_)[idx].checksum();
                }
                ++ops;
            }
            break;

        case Case::Iteration:
            for (size_t i = 0; i < vec_->size(); ++i)
            {
                if (valid_[i])
                {
                    benchmark_sink += (*vec_)[i].checksum();
                }
                ++ops;
            }
            break;

        case Case::Erase25Percent:
            // Just mark as invalid (no actual removal - demonstrates issue)
            for (size_t idx : in.erase_indices)
            {
                if (idx < valid_.size())
                {
                    valid_[idx] = false;
                }
                ++ops;
            }
            break;

        case Case::EraseAll:
            for (size_t i = 0; i < valid_.size(); ++i)
            {
                valid_[i] = false;
                ++ops;
            }
            break;

        case Case::MixedWorkload:
        {
            size_t batch = std::max<size_t>(1, in.N / 10);
            
            for (size_t i = 0; i < batch && i < in.values.size(); ++i)
            {
                vec_->emplace_back(TestValue(in.values[i]));
                valid_.push_back(true);
                ++ops;
            }
            
            for (size_t i = 0; i < batch && i < vec_->size(); ++i)
            {
                size_t idx = rng() % vec_->size();
                if (valid_[idx])
                {
                    benchmark_sink += (*vec_)[idx].checksum();
                }
                ++ops;
            }
            
            size_t erase_count = std::min(batch / 2, vec_->size());
            for (size_t i = 0; i < erase_count; ++i)
            {
                if (!vec_->empty())
                {
                    size_t idx = rng() % vec_->size();
                    valid_[idx] = false;
                }
                ++ops;
            }
            break;
        }

        case Case::SlotReuse:
        {
            size_t half = in.N / 2;
            
            vec_->clear();
            valid_.clear();
            for (size_t i = 0; i < in.N && i < in.values.size(); ++i)
            {
                vec_->emplace_back(TestValue(in.values[i]));
                valid_.push_back(true);
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < valid_.size(); ++i)
            {
                valid_[i] = false;
                ++ops;
            }
            
            for (size_t i = 0; i < half && i < in.values.size(); ++i)
            {
                vec_->emplace_back(TestValue(in.values[i] + 1000000));
                valid_.push_back(true);
                ++ops;
            }
            break;
        }
        }
        return ops;
    }
};

// ============================================================================
// Benchmark Results
// ============================================================================

struct BenchResult
{
    std::string library;
    std::vector<double> samples;
};

// ============================================================================
// Core Operations Benchmark
// ============================================================================

void benchmark_core_operations(const std::vector<size_t>& sizes)
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 1: Core Operations (Insert, Access, Erase)\n";
    std::cout << "================================================================================\n";
    
    print_cpu_context("Section start");

    // Build adapter list with all available libraries
    std::vector<std::unique_ptr<ISlotMapAdapter>> adapters;
    
#if HAS_FATP_SLOTMAP
    adapters.push_back(std::make_unique<FatPSlotMapAdapter>());
#endif
#if HAS_ENTT
    adapters.push_back(std::make_unique<EnTTAdapter>());
#endif
#if HAS_PLF_HIVE
    adapters.push_back(std::make_unique<PlfHiveAdapter>());
#endif
#if HAS_SG14_SLOTMAP
    adapters.push_back(std::make_unique<SG14SlotMapAdapter>());
#endif
    adapters.push_back(std::make_unique<UnorderedMapAdapter>());
    adapters.push_back(std::make_unique<StdMapAdapter>());
    adapters.push_back(std::make_unique<VectorAdapter>());

    std::mt19937_64 rng(42);
    std::vector<Case> cases = {
        Case::SequentialInsert,
        Case::RandomAccessValid,
        Case::Erase25Percent
    };

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

        for (Case c : cases)
        {
            cooling_delay(COOLING_DELAY_CASE_MS, nullptr);

            std::vector<BenchResult> results;
            for (auto& adapter : adapters)
            {
                results.push_back({adapter->name(), {}});
            }

            bool needs_preload = (c != Case::SequentialInsert);

            // Warmup
            for (size_t run = 0; run < WARMUP_RUNS(); ++run)
            {
                for (size_t idx = 0; idx < adapters.size(); ++idx)
                {
                    adapters[idx]->setup(N);
                    if (needs_preload) adapters[idx]->preload(in);
                    adapters[idx]->run_operation(c, in);
                    adapters[idx]->teardown();
                }
            }

            // Measured runs with round-robin randomization
            for (size_t run = 0; run < MEASURED_RUNS(); ++run)
            {
                std::vector<size_t> order(adapters.size());
                std::iota(order.begin(), order.end(), 0);
                std::shuffle(order.begin(), order.end(), rng);

                for (size_t idx : order)
                {
                    adapters[idx]->setup(N);
                    if (needs_preload) adapters[idx]->preload(in);

                    Timer t;
                    t.start();
                    size_t ops = adapters[idx]->run_operation(c, in);
                    double elapsed = t.elapsed_ns();

                    adapters[idx]->teardown();

                    results[idx].samples.push_back(ns_per_op(elapsed, ops));
                }
            }

            // Print results
            std::cout << "  " << case_name(c) << ":\n";
            std::cout << std::fixed << std::setprecision(2);
            for (const auto& r : results)
            {
                auto stats = Statistics::compute(r.samples);
                std::cout << "    " << std::setw(24) << r.library << ": "
                          << std::setw(8) << stats.median << " ns/op "
                          << "(+/-" << std::setw(6) << stats.stddev << ")\n";
            }
        }
    }
}

// ============================================================================
// ABA Safety Benchmark
// ============================================================================

void benchmark_aba_safety()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 2: ABA Safety Test (Generational Index Validation)\n";
    std::cout << "================================================================================\n";
    
    print_cpu_context("Section start");

#if HAS_FATP_SLOTMAP
    std::cout << "\n  Testing fat_p::SlotMap ABA protection:\n";
    
    fat_p::SlotMap<TestValue> map;
    constexpr size_t N = 10000;
    
    // Insert elements
    std::vector<fat_p::SlotMapHandle> handles;
    handles.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        handles.push_back(map.insert(TestValue(static_cast<int64_t>(i))));
    }
    
    // Store some handles before erasing
    std::vector<fat_p::SlotMapHandle> old_handles;
    for (size_t i = 0; i < N / 2; ++i)
    {
        old_handles.push_back(handles[i]);
    }
    
    // Erase first half
    for (size_t i = 0; i < N / 2; ++i)
    {
        map.erase(handles[i]);
    }
    
    // Verify old handles are now invalid
    size_t invalid_count = 0;
    for (const auto& h : old_handles)
    {
        if (!map.is_valid(h)) ++invalid_count;
    }
    
    std::cout << "    Erased handles correctly invalidated: " 
              << invalid_count << "/" << old_handles.size();
    if (invalid_count == old_handles.size())
    {
        std::cout << " [PASS]\n";
    }
    else
    {
        std::cout << " [FAIL]\n";
    }
    
    // Reinsert (should reuse slots)
    for (size_t i = 0; i < N / 2; ++i)
    {
        handles[i] = map.insert(TestValue(static_cast<int64_t>(i + N)));
    }
    
    // Old handles should STILL be invalid (generation changed)
    size_t still_invalid = 0;
    for (const auto& h : old_handles)
    {
        if (!map.is_valid(h)) ++still_invalid;
    }
    
    std::cout << "    Old handles invalid after slot reuse: " 
              << still_invalid << "/" << old_handles.size();
    if (still_invalid == old_handles.size())
    {
        std::cout << " [PASS]\n";
    }
    else
    {
        std::cout << " [FAIL - ABA VULNERABILITY!]\n";
    }
    
    // New handles should be valid
    size_t new_valid = 0;
    for (size_t i = 0; i < N; ++i)
    {
        if (map.is_valid(handles[i])) ++new_valid;
    }
    
    std::cout << "    New handles valid: " << new_valid << "/" << N;
    if (new_valid == N)
    {
        std::cout << " [PASS]\n";
    }
    else
    {
        std::cout << " [FAIL]\n";
    }
#else
    std::cout << "\n  (fat_p::SlotMap not available - skipping ABA test)\n";
#endif

#if HAS_ENTT
    std::cout << "\n  Testing entt::registry ABA protection:\n";
    
    entt::basic_registry<entt::entity> registry;
    constexpr size_t N_ENTT = 10000;
    
    std::vector<entt::entity> entities;
    entities.reserve(N_ENTT);
    for (size_t i = 0; i < N_ENTT; ++i)
    {
        auto e = registry.create();
        registry.emplace<TestValue>(e, TestValue(static_cast<int64_t>(i)));
        entities.push_back(e);
    }
    
    std::vector<entt::entity> old_entities;
    for (size_t i = 0; i < N_ENTT / 2; ++i)
    {
        old_entities.push_back(entities[i]);
    }
    
    for (size_t i = 0; i < N_ENTT / 2; ++i)
    {
        registry.destroy(entities[i]);
    }
    
    size_t invalid_count_entt = 0;
    for (const auto& e : old_entities)
    {
        if (!registry.valid(e)) ++invalid_count_entt;
    }
    
    std::cout << "    Destroyed entities correctly invalidated: " 
              << invalid_count_entt << "/" << old_entities.size();
    if (invalid_count_entt == old_entities.size())
    {
        std::cout << " [PASS]\n";
    }
    else
    {
        std::cout << " [FAIL]\n";
    }
#endif
}

// ============================================================================
// Iteration Speed Benchmark
// ============================================================================

void benchmark_iteration_speed()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 3: Iteration Speed (Dense Storage Advantage)\n";
    std::cout << "================================================================================\n";
    
    print_cpu_context("Section start");

    std::vector<std::unique_ptr<ISlotMapAdapter>> adapters;
#if HAS_FATP_SLOTMAP
    adapters.push_back(std::make_unique<FatPSlotMapAdapter>());
#endif
#if HAS_ENTT
    adapters.push_back(std::make_unique<EnTTAdapter>());
#endif
#if HAS_PLF_HIVE
    adapters.push_back(std::make_unique<PlfHiveAdapter>());
#endif
    adapters.push_back(std::make_unique<UnorderedMapAdapter>());
    adapters.push_back(std::make_unique<StdMapAdapter>());
    adapters.push_back(std::make_unique<VectorAdapter>());

    std::mt19937_64 rng(99);
    std::vector<size_t> sizes = {1000, 10000, 100000, 500000};

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

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
                adapters[idx]->setup(N);
                adapters[idx]->preload(in);
                adapters[idx]->run_operation(Case::Iteration, in);
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
                adapters[idx]->setup(N);
                adapters[idx]->preload(in);

                Timer t;
                t.start();
                size_t ops = adapters[idx]->run_operation(Case::Iteration, in);
                double elapsed = t.elapsed_ns();

                adapters[idx]->teardown();

                results[idx].samples.push_back(ns_per_op(elapsed, ops));
            }
        }

        // Print results
        std::cout << "  Iteration:\n";
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            std::cout << "    " << std::setw(24) << r.library << ": "
                      << std::setw(8) << stats.median << " ns/op "
                      << "(+/-" << std::setw(6) << stats.stddev << ")\n";
        }
    }
}

// ============================================================================
// Mixed Workload Benchmark
// ============================================================================

void benchmark_mixed_workload()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  SECTION 4: Mixed Workload (Insert/Access/Erase Interleaved)\n";
    std::cout << "================================================================================\n";
    
    print_cpu_context("Section start");

    std::vector<std::unique_ptr<ISlotMapAdapter>> adapters;
#if HAS_FATP_SLOTMAP
    adapters.push_back(std::make_unique<FatPSlotMapAdapter>());
#endif
#if HAS_ENTT
    adapters.push_back(std::make_unique<EnTTAdapter>());
#endif
#if HAS_PLF_HIVE
    adapters.push_back(std::make_unique<PlfHiveAdapter>());
#endif
    adapters.push_back(std::make_unique<UnorderedMapAdapter>());
    adapters.push_back(std::make_unique<StdMapAdapter>());

    std::mt19937_64 rng(77);
    std::vector<size_t> sizes = {1000, 10000, 50000};

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

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
                adapters[idx]->setup(N);
                adapters[idx]->run_operation(Case::MixedWorkload, in);
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
                adapters[idx]->setup(N);

                Timer t;
                t.start();
                size_t ops = adapters[idx]->run_operation(Case::MixedWorkload, in);
                double elapsed = t.elapsed_ns();

                adapters[idx]->teardown();

                results[idx].samples.push_back(ns_per_op(elapsed, ops));
            }
        }

        // Print results
        std::cout << "  Mixed Workload:\n";
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            std::cout << "    " << std::setw(24) << r.library << ": "
                      << std::setw(8) << stats.median << " ns/op "
                      << "(+/-" << std::setw(6) << stats.stddev << ")\n";
        }
    }
}

// ============================================================================
// Memory Comparison
// ============================================================================

void print_memory_comparison()
{
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Memory Overhead Comparison (Theoretical)\n";
    std::cout << "================================================================================\n\n";

    std::cout << "  Per-element overhead (excluding value storage):\n";
    std::cout << "    fat_p::SlotMap:      ~12 bytes (slot: 8B + erase_map entry: 4B)\n";
#if HAS_ENTT
    std::cout << "    entt::registry:      ~8-12 bytes (sparse set + component pool)\n";
#endif
#if HAS_PLF_HIVE
    std::cout << "    plf::hive:           ~0-8 bytes (skipfield metadata, amortized)\n";
#endif
#if HAS_SG14_SLOTMAP
    std::cout << "    sg14::slot_map:      ~12 bytes (similar to fat_p)\n";
#endif
    std::cout << "    std::unordered_map:  ~8-16 bytes (bucket pointer + next pointer)\n";
    std::cout << "    std::map:            ~32-40 bytes (RB-tree node: color + 3 pointers)\n";
    std::cout << "    std::vector:         ~0 bytes (dense, but no handle safety)\n\n";

    std::cout << "  Feature comparison:\n";
    std::cout << "    Container             ABA-Safe  O(1) Access  Dense Iter  Stable Ptr\n";
    std::cout << "    -----------------------------------------------------------------------\n";
    std::cout << "    fat_p::SlotMap        Yes       Yes          Yes         No\n";
#if HAS_ENTT
    std::cout << "    entt::registry        Yes       Yes          Yes         No\n";
#endif
#if HAS_PLF_HIVE
    std::cout << "    plf::hive             No        No           Yes         Yes\n";
#endif
#if HAS_SG14_SLOTMAP
    std::cout << "    sg14::slot_map        Yes       Yes          Yes         No\n";
#endif
    std::cout << "    std::unordered_map    No        Yes*         No          Yes\n";
    std::cout << "    std::map              No        No           No          Yes\n";
    std::cout << "    std::vector           No        Yes          Yes         No\n";
    std::cout << "    (* average case)\n\n";

#if HAS_FATP_SLOTMAP
    constexpr size_t N = 10000;
    fat_p::SlotMap<TestValue> slotmap;
    for (size_t i = 0; i < N; ++i)
    {
        (void)slotmap.insert(TestValue(static_cast<int64_t>(i)));
    }
    
    std::cout << "  fat_p::SlotMap<TestValue> with " << N << " elements:\n";
    std::cout << "    size():        " << slotmap.size() << "\n";
    std::cout << "    capacity():    " << slotmap.capacity() << "\n";
    std::cout << "    slot_count():  " << slotmap.slot_count() << "\n";
    std::cout << "    sizeof(TestValue): " << sizeof(TestValue) << " bytes\n";
#endif
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
    std::cout << "  SlotMap Comprehensive Benchmark Suite\n";
    std::cout << "================================================================================\n";
    
    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() 
              << ", seed=" << g_config.seed << ")\n";

    std::cout << "\nLibraries detected:\n";
#if HAS_FATP_SLOTMAP
    std::cout << "  [x] fat_p::SlotMap\n";
#else
    std::cout << "  [ ] fat_p::SlotMap (not found)\n";
#endif
#if HAS_ENTT
    std::cout << "  [x] entt::registry (popular ECS library)\n";
#else
    std::cout << "  [ ] entt::registry (install: vcpkg install entt)\n";
#endif
#if HAS_PLF_HIVE
    std::cout << "  [x] plf::hive (stable pointer container)\n";
#else
    std::cout << "  [ ] plf::hive (get: github.com/mattreecebentley/plf_hive)\n";
#endif
#if HAS_SG14_SLOTMAP
    std::cout << "  [x] sg14::slot_map (WG21 study group reference)\n";
#else
    std::cout << "  [ ] sg14::slot_map (get: github.com/WG21-SG14/SG14)\n";
#endif
#if HAS_BOOST_STABLE_VECTOR
    std::cout << "  [x] boost::container::stable_vector\n";
#else
    std::cout << "  [ ] boost::container::stable_vector\n";
#endif
    std::cout << "  [x] std::unordered_map (baseline)\n";
    std::cout << "  [x] std::map (baseline)\n";
    std::cout << "  [x] std::vector (baseline)\n";
    std::cout << "\n";

    // CPU detection
    fat_p::bench::print_cpu_detection_info(std::cout);
    std::cout << "\n";

    std::cout << "Design Invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run\n";
    std::cout << "  3. Setup/teardown outside timed regions\n";
    std::cout << "  4. All libraries observe same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n\n";

    std::cout << "Expected Results:\n";
    std::cout << "  - SlotMap excels at: iteration (dense storage), O(1) operations, ABA safety\n";
    std::cout << "  - EnTT: similar design, optimized for ECS patterns\n";
    std::cout << "  - plf::hive: stable pointers, good iteration, no handle safety\n";
    std::cout << "  - unordered_map: fast lookup but scattered iteration\n";
    std::cout << "  - std::map: consistent O(log n) but slowest overall\n";
    std::cout << "  - std::vector: fastest raw access but no handle safety\n\n";

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

    // Default sizes for core operations
    std::vector<size_t> core_sizes = {1000, 10000, 100000};

    // Run benchmarks
    benchmark_core_operations(core_sizes);

    cooling_delay(COOLING_DELAY_SECTION_MS, "before ABA safety test");
    benchmark_aba_safety();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before iteration benchmark");
    benchmark_iteration_speed();

    cooling_delay(COOLING_DELAY_SECTION_MS, "before mixed workload");
    benchmark_mixed_workload();

    print_memory_comparison();

    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
