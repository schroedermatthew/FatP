// benchmark_ObjectPool.cpp
//
// FAT-P ObjectPool benchmarks using unified FatPBenchmarkRunner infrastructure.
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
//   - fat_p::ObjectPool: Fixed-size object pool with O(1) acquire/release
//                        Thread-safe variant via MutexSynchronizationPolicy
//                        RAII wrapper via PooledObject
//
// Competitor Libraries (conditioned on availability):
//   TIER 1 - Direct competitors (object pools):
//     - boost::object_pool - Mature, auto-growing, auto-destruction
//     - foonathan::memory_pool - Modern C++ memory pools
//     - EASTL::fixed_pool - EA's game-industry pool
//   TIER 2 - Standard library alternatives:
//     - std::pmr::unsynchronized_pool_resource - C++17 PMR pools
//     - new/delete baseline
//   TIER 3 - Other allocators:
//     - boost::pool_allocator - Pool-backed allocator
//
// Build (minimal):
//   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_ObjectPool.cpp -o bench_objpool -lpthread
//
// Build (MSVC):
//   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_ObjectPool.cpp /link advapi32.lib
//
// Build (with competitors):
//   g++ -std=c++17 -O3 -DNDEBUG -march=native -I/path/to/boost \
//       -I/path/to/foonathan -I/path/to/EASTL benchmark_ObjectPool.cpp -o bench_objpool -lpthread
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
//   FATP_BENCH_MEASURED_RUNS - Measured batches (default: 50, Windows: 15)
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
//   ./bench_objpool
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_objpool

/*
FATP_META:
  meta_version: 1
  component: ObjectPool
  file_role: benchmark
  path: benchmarks/benchmark_ObjectPool.cpp
  namespace: fat_p
  summary: "Comprehensive benchmarks for ObjectPool vs industry competitors."
  related:
    docs_search: "ObjectPool"
    headers:
      - fat_p/FatPBenchmarkRunner.h
      - fat_p/ObjectPool.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 12
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: Claude
    mode: manual
*/

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "FatPBenchmarkRunner.h"

// ============================================================================
// Library Detection
// ============================================================================

// Fat-P ObjectPool
#if __has_include("ObjectPool.h")
#include "ObjectPool.h"
#define HAS_FATP_OBJECTPOOL 1
#else
#define HAS_FATP_OBJECTPOOL 0
#endif

// Boost.Pool - Mature object pool implementation
// Install: vcpkg install boost-pool, or from boost.org
#if __has_include(<boost/pool/object_pool.hpp>)
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#define HAS_BOOST_POOL 1
#else
#define HAS_BOOST_POOL 0
#endif

// foonathan::memory - Modern C++ memory allocation library
// Install: vcpkg install foonathan-memory, or github.com/foonathan/memory
#if __has_include(<foonathan/memory/memory_pool.hpp>)
#include <foonathan/memory/memory_pool.hpp>
#include <foonathan/memory/namespace_alias.hpp>
#define HAS_FOONATHAN_MEMORY 1
#else
#define HAS_FOONATHAN_MEMORY 0
#endif

// EASTL fixed_pool - EA's game-industry allocator
// Install: vcpkg install eastl, or github.com/electronicarts/EASTL
#if __has_include(<EASTL/fixed_pool.h>)
#include <EASTL/fixed_pool.h>
#define HAS_EASTL_POOL 1
#else
#define HAS_EASTL_POOL 0
#endif

// Apache APR pools (C library, less common in C++ projects)
#if __has_include(<apr_pools.h>)
#include <apr_pools.h>
#define HAS_APR_POOL 1
#else
#define HAS_APR_POOL 0
#endif

// ============================================================================
// Platform Configuration
// ============================================================================

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
// Benchmark Environment Configuration
// ============================================================================

using fat_p::bench::BenchmarkScope;

// ============================================================================
// Timer (using BenchClock for consistency)
// ============================================================================

struct Timer
{
    using Clock = fat_p::bench::BenchClock;
    Clock::time_point t0;

    void start()
    {
        t0 = Clock::now();
    }

    double elapsed_ns() const
    {
        auto t1 = Clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }
};

static inline double ns_per_op(double elapsed_ns, size_t ops)
{
    return (ops == 0) ? 0.0 : (elapsed_ns / static_cast<double>(ops));
}

// Prevent dead code elimination
static volatile int64_t benchmark_sink = 0;

template <typename T>
static inline void prevent_opt(T value)
{
    benchmark_sink ^= static_cast<int64_t>(reinterpret_cast<uintptr_t>(&value) ^ static_cast<uintptr_t>(value));
}

template <typename T>
static inline void prevent_opt_ptr(T* ptr)
{
    benchmark_sink ^= reinterpret_cast<int64_t>(ptr);
}

// ============================================================================
// CPU Frequency Stability
// ============================================================================

static inline void cpu_warmup_burst(int milliseconds)
{
    if (milliseconds <= 0)
        return;

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

static bool wait_for_cpu_stable(double max_variance_percent = 10.0,
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
            std::cout << "[CPU frequency detection unavailable - using fixed cooling delay]\n";
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
            recent_readings.erase(recent_readings.begin());

        if (recent_readings.size() >= window_size)
        {
            double min_freq = *std::min_element(recent_readings.begin(), recent_readings.end());
            double max_freq = *std::max_element(recent_readings.begin(), recent_readings.end());
            double avg_freq = std::accumulate(recent_readings.begin(), recent_readings.end(), 0.0) / recent_readings.size();

            double variance_pct = (max_freq - min_freq) / avg_freq * 100.0;
            bool is_stable = (variance_pct <= max_variance_percent) && (avg_freq >= min_required_freq);

            if (is_stable)
            {
                if (++stable_count >= required_stable)
                {
                    if (verbose)
                    {
                        double pct_of_base = (avg_freq / base_freq) * 100.0;
                        std::cout << "[CPU stable at " << static_cast<int>(avg_freq) << " MHz ("
                                  << std::fixed << std::setprecision(0) << pct_of_base << "% of base)]\n";
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
        std::cout << "[WARNING: CPU frequency still unstable after " << timeout_seconds << "s]\n";
    return false;
}

static inline void cooling_delay(int min_sleep_ms, const char* reason = nullptr)
{
    if (g_config.noCooldown)
        return;

    if (reason)
        std::cout << "[Cooling: " << reason << "]" << std::flush;

    std::this_thread::sleep_for(std::chrono::milliseconds(min_sleep_ms));
    wait_for_cpu_stable(10.0, 15, 200, false);

    if (reason)
    {
        auto info = fat_p::bench::capture_cpu_frequency();
        if (info.has_reliable_detection())
            std::cout << " [Ready: " << static_cast<int>(info.mCurrentFreqMHz) << " MHz]\n";
        else
            std::cout << " [Ready]\n";
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
        if (samples.empty())
            return s;

        std::sort(samples.begin(), samples.end());
        size_t n = samples.size();

        s.min = samples.front();
        s.max = samples.back();

        // Median
        s.median = (n % 2 == 1) ? samples[n / 2] : 0.5 * (samples[n / 2 - 1] + samples[n / 2]);

        // Mean
        s.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(n);

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
// Test Object Types
// ============================================================================

// Small trivial object (16 bytes) - best case for pools
struct SmallTrivial
{
    int64_t a;
    int64_t b;

    SmallTrivial() = default;
    explicit SmallTrivial(int64_t v) : a(v), b(v * 2) {}
    int64_t checksum() const { return a ^ b; }
};
static_assert(sizeof(SmallTrivial) == 16, "SmallTrivial should be 16 bytes");
static_assert(std::is_trivially_destructible_v<SmallTrivial>, "SmallTrivial should be trivially destructible");
static_assert(std::is_trivially_constructible_v<SmallTrivial>, "SmallTrivial should be trivially constructible");

// Medium object (64 bytes) - typical game entity
struct MediumObject
{
    int64_t id;
    double x, y, z;
    int32_t health;
    int32_t flags;
    char name[16];
    char padding_[8]; // Explicit padding to reach 64 bytes

    MediumObject() : id(0), x(0), y(0), z(0), health(100), flags(0)
    {
        std::memset(name, 0, sizeof(name));
        std::memset(padding_, 0, sizeof(padding_));
    }
    explicit MediumObject(int64_t v) : id(v), x(static_cast<double>(v)), y(v * 0.5), z(v * 0.25), health(100), flags(0)
    {
        std::memset(name, 0, sizeof(name));
        std::memset(padding_, 0, sizeof(padding_));
    }
    int64_t checksum() const { return id + static_cast<int64_t>(x + y + z) + health + flags; }
};
static_assert(sizeof(MediumObject) == 64, "MediumObject should be 64 bytes");

// Large object (256 bytes) - stress test
struct LargeObject
{
    int64_t id;
    double matrix[4][4];
    char buffer[120];

    LargeObject() : id(0)
    {
        std::memset(matrix, 0, sizeof(matrix));
        std::memset(buffer, 0, sizeof(buffer));
    }
    explicit LargeObject(int64_t v) : id(v)
    {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                matrix[i][j] = static_cast<double>(v + i * 4 + j);
        std::memset(buffer, static_cast<int>(v & 0xFF), sizeof(buffer));
    }
    int64_t checksum() const { return id + static_cast<int64_t>(matrix[0][0] + matrix[3][3]); }
};
static_assert(sizeof(LargeObject) == 256, "LargeObject should be 256 bytes");

// Non-trivial object with constructor/destructor overhead
struct NonTrivialObject
{
    std::unique_ptr<int64_t[]> data;
    size_t size;

    static inline std::atomic<int64_t> construct_count{0};
    static inline std::atomic<int64_t> destruct_count{0};

    NonTrivialObject() : data(std::make_unique<int64_t[]>(8)), size(8) { ++construct_count; }
    explicit NonTrivialObject(int64_t v) : data(std::make_unique<int64_t[]>(8)), size(8)
    {
        ++construct_count;
        for (size_t i = 0; i < size; ++i)
            data[i] = v + static_cast<int64_t>(i);
    }
    ~NonTrivialObject() { ++destruct_count; }

    // Move only
    NonTrivialObject(NonTrivialObject&&) = default;
    NonTrivialObject& operator=(NonTrivialObject&&) = default;
    NonTrivialObject(const NonTrivialObject&) = delete;
    NonTrivialObject& operator=(const NonTrivialObject&) = delete;

    int64_t checksum() const { return data ? data[0] + data[size - 1] : 0; }

    static void reset_counts()
    {
        construct_count = 0;
        destruct_count = 0;
    }
};

// ============================================================================
// Benchmark Case Enum
// ============================================================================

enum class Case
{
    AcquireRelease,       // Single acquire + release cycle
    BulkAcquire,          // Acquire N objects, then release all
    InterleavedOps,       // Interleaved acquire/release pattern
    TryAcquireSuccess,    // try_acquire when pool has capacity
    TryAcquireFail,       // try_acquire when pool exhausted (no growth)
    RAIIWrapper,          // PooledObject RAII overhead
    PoolReuse,            // Release and re-acquire (tests free list)
    Construction,         // Object construction overhead
    UninitializedAcquire, // acquire_uninitialized (trivial types only)
    ZeroedAcquire,        // acquire_zeroed (trivial types only)
};

static inline const char* case_name(Case c)
{
    switch (c)
    {
        case Case::AcquireRelease: return "Acquire+Release";
        case Case::BulkAcquire: return "Bulk Acquire";
        case Case::InterleavedOps: return "Interleaved Ops";
        case Case::TryAcquireSuccess: return "try_acquire (success)";
        case Case::TryAcquireFail: return "try_acquire (fail)";
        case Case::RAIIWrapper: return "RAII Wrapper";
        case Case::PoolReuse: return "Pool Reuse";
        case Case::Construction: return "Construction Cost";
        case Case::UninitializedAcquire: return "Uninitialized Acquire";
        case Case::ZeroedAcquire: return "Zeroed Acquire";
    }
    return "Unknown";
}

// ============================================================================
// Shared Inputs
// ============================================================================

struct Inputs
{
    size_t N;
    std::vector<int64_t> values;
    std::vector<size_t> release_order; // Randomized release order
    uint64_t seed;

    static Inputs make(size_t n, uint64_t seed = 0)
    {
        if (seed == 0)
            seed = g_config.seed;

        Inputs in;
        in.N = n;
        in.seed = seed;

        in.values.resize(n);
        std::mt19937_64 rng(seed);
        for (size_t i = 0; i < n; ++i)
            in.values[i] = static_cast<int64_t>(i * 1000 + rng() % 1000);

        in.release_order.resize(n);
        std::iota(in.release_order.begin(), in.release_order.end(), 0);
        std::shuffle(in.release_order.begin(), in.release_order.end(), rng);

        return in;
    }
};

// ============================================================================
// Adapter Interface
// ============================================================================

template <typename T>
struct IPoolAdapter
{
    virtual ~IPoolAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t capacity) = 0;
    virtual void teardown() = 0;
    virtual T* acquire(int64_t value) = 0;
    virtual T* try_acquire(int64_t value) = 0; // Returns nullptr if pool exhausted
    virtual void release(T* obj) = 0;
    virtual size_t capacity() const = 0;
    virtual size_t available() const = 0;
    virtual bool supports_try_acquire() const { return false; }
    virtual bool supports_uninitialized() const { return false; }
    virtual T* acquire_uninitialized() { return nullptr; }
    virtual T* acquire_zeroed() { return nullptr; }
};

// ============================================================================
// Fat-P ObjectPool Adapter
// ============================================================================

#if HAS_FATP_OBJECTPOOL
template <typename T>
class FatPPoolAdapter final : public IPoolAdapter<T>
{
    std::unique_ptr<fat_p::ObjectPool<T>> pool_;
    size_t block_size_ = 64;

public:
    const char* name() const override { return "fat_p::ObjectPool"; }

    void setup(size_t capacity) override
    {
        block_size_ = std::max<size_t>(64, capacity);
        pool_ = std::make_unique<fat_p::ObjectPool<T>>(block_size_);
        // Pre-allocate to match capacity
        size_t blocks_needed = (capacity + block_size_ - 1) / block_size_;
        pool_->reserve_blocks(blocks_needed);
    }

    void teardown() override { pool_.reset(); }

    T* acquire(int64_t value) override { return pool_->acquire(value); }

    T* try_acquire(int64_t value) override { return pool_->try_acquire(value); }

    void release(T* obj) override { pool_->release(obj); }

    size_t capacity() const override { return pool_ ? pool_->capacity() : 0; }
    size_t available() const override { return pool_ ? pool_->available() : 0; }

    bool supports_try_acquire() const override { return true; }

    bool supports_uninitialized() const override { return std::is_trivially_destructible_v<T>; }

    T* acquire_uninitialized() override
    {
        if constexpr (std::is_trivially_destructible_v<T>)
        {
            T* raw = pool_->acquire_uninitialized();
            ::new (static_cast<void*>(raw)) T; // Start lifetime
            return raw;
        }
        return nullptr;
    }

    T* acquire_zeroed() override
    {
        if constexpr (std::is_trivially_constructible_v<T> && std::is_trivially_destructible_v<T>)
        {
            T* raw = pool_->acquire_zeroed();
            ::new (static_cast<void*>(raw)) T; // Start lifetime
            return raw;
        }
        return nullptr;
    }
};
#endif

// ============================================================================
// Boost.Pool Adapter
// ============================================================================

#if HAS_BOOST_POOL
template <typename T>
class BoostPoolAdapter final : public IPoolAdapter<T>
{
    std::unique_ptr<boost::object_pool<T>> pool_;
    size_t capacity_ = 0;
    size_t acquired_ = 0;

public:
    const char* name() const override { return "boost::object_pool"; }

    void setup(size_t capacity) override
    {
        pool_ = std::make_unique<boost::object_pool<T>>();
        capacity_ = capacity;
        acquired_ = 0;
        // Boost object_pool grows on demand, no pre-reserve
    }

    void teardown() override
    {
        pool_.reset();
        acquired_ = 0;
    }

    T* acquire(int64_t value) override
    {
        ++acquired_;
        return pool_->construct(value);
    }

    T* try_acquire(int64_t value) override
    {
        // Boost object_pool always grows, so this always succeeds
        return acquire(value);
    }

    void release(T* obj) override
    {
        pool_->destroy(obj);
        if (acquired_ > 0)
            --acquired_;
    }

    size_t capacity() const override { return capacity_; }
    size_t available() const override { return capacity_ > acquired_ ? capacity_ - acquired_ : 0; }
};
#endif

// ============================================================================
// std::pmr Pool Adapter
// ============================================================================

template <typename T>
class PmrPoolAdapter final : public IPoolAdapter<T>
{
    std::unique_ptr<std::pmr::unsynchronized_pool_resource> pool_;
    size_t capacity_ = 0;
    size_t acquired_ = 0;

public:
    const char* name() const override { return "std::pmr::pool_resource"; }

    void setup(size_t capacity) override
    {
        std::pmr::pool_options opts;
        opts.max_blocks_per_chunk = capacity;
        opts.largest_required_pool_block = sizeof(T) * 2;
        pool_ = std::make_unique<std::pmr::unsynchronized_pool_resource>(opts);
        capacity_ = capacity;
        acquired_ = 0;
    }

    void teardown() override
    {
        pool_.reset();
        acquired_ = 0;
    }

    T* acquire(int64_t value) override
    {
        std::pmr::polymorphic_allocator<T> alloc(pool_.get());
        T* ptr = alloc.allocate(1);
        std::allocator_traits<std::pmr::polymorphic_allocator<T>>::construct(alloc, ptr, value);
        ++acquired_;
        return ptr;
    }

    T* try_acquire(int64_t value) override { return acquire(value); }

    void release(T* obj) override
    {
        std::pmr::polymorphic_allocator<T> alloc(pool_.get());
        std::allocator_traits<std::pmr::polymorphic_allocator<T>>::destroy(alloc, obj);
        alloc.deallocate(obj, 1);
        if (acquired_ > 0)
            --acquired_;
    }

    size_t capacity() const override { return capacity_; }
    size_t available() const override { return capacity_ > acquired_ ? capacity_ - acquired_ : 0; }
};

// ============================================================================
// new/delete Baseline Adapter
// ============================================================================

template <typename T>
class NewDeleteAdapter final : public IPoolAdapter<T>
{
    size_t capacity_ = 0;
    size_t acquired_ = 0;

public:
    const char* name() const override { return "new/delete"; }

    void setup(size_t capacity) override
    {
        capacity_ = capacity;
        acquired_ = 0;
    }

    void teardown() override { acquired_ = 0; }

    T* acquire(int64_t value) override
    {
        ++acquired_;
        return new T(value);
    }

    T* try_acquire(int64_t value) override { return acquire(value); }

    void release(T* obj) override
    {
        delete obj;
        if (acquired_ > 0)
            --acquired_;
    }

    size_t capacity() const override { return capacity_; }
    size_t available() const override { return capacity_ > acquired_ ? capacity_ - acquired_ : 0; }
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
// Core Operations Benchmark
// ============================================================================

template <typename T>
void benchmark_acquire_release(std::vector<std::unique_ptr<IPoolAdapter<T>>>& adapters,
                               const std::vector<size_t>& sizes,
                               const char* type_name)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Acquire + Release Cycle (" << type_name << ")\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Contract: Single acquire followed by immediate release\n";
    std::cout << "          Measures raw pool overhead without allocation growth\n\n";

    std::mt19937_64 rng(g_config.seed);

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " ops ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

        std::vector<BenchResult> results;
        for (auto& adapter : adapters)
            results.push_back({adapter->name(), {}});

        // Warmup
        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            for (size_t idx = 0; idx < adapters.size(); ++idx)
            {
                adapters[idx]->setup(N);
                for (size_t i = 0; i < N; ++i)
                {
                    T* obj = adapters[idx]->acquire(in.values[i % in.values.size()]);
                    prevent_opt_ptr(obj);
                    adapters[idx]->release(obj);
                }
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
                for (size_t i = 0; i < N; ++i)
                {
                    T* obj = adapters[idx]->acquire(in.values[i % in.values.size()]);
                    prevent_opt_ptr(obj);
                    adapters[idx]->release(obj);
                }
                double elapsed = t.elapsed_ns();

                adapters[idx]->teardown();
                results[idx].samples.push_back(ns_per_op(elapsed, N));
            }
        }

        // Print results
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            std::cout << "  " << std::setw(26) << r.library << ": median=" << std::setw(8) << stats.median
                      << " ns/op  mean=" << std::setw(8) << stats.mean << " +/-" << std::setw(6) << stats.stddev << "\n";
        }

        // Speedup vs new/delete
        if (results.size() >= 2)
        {
            auto baseline_stats = Statistics::compute(results.back().samples); // new/delete is last
            auto fatp_stats = Statistics::compute(results[0].samples);
            if (baseline_stats.median > 0)
            {
                double speedup = baseline_stats.median / fatp_stats.median;
                std::cout << "  Speedup vs new/delete: " << std::setprecision(1) << speedup << "x\n";
            }
        }
    }
}

// ============================================================================
// Bulk Acquire Benchmark
// ============================================================================

template <typename T>
void benchmark_bulk_acquire(std::vector<std::unique_ptr<IPoolAdapter<T>>>& adapters,
                            const std::vector<size_t>& sizes,
                            const char* type_name)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Bulk Acquire (" << type_name << ")\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Contract: Acquire N objects, then release all\n";
    std::cout << "          Tests sustained allocation throughput\n\n";

    std::mt19937_64 rng(g_config.seed);

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " objects ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

        std::vector<BenchResult> results;
        for (auto& adapter : adapters)
            results.push_back({adapter->name(), {}});

        std::vector<T*> acquired(N);

        // Warmup
        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            for (size_t idx = 0; idx < adapters.size(); ++idx)
            {
                adapters[idx]->setup(N * 2); // Extra capacity to avoid growth during measurement

                for (size_t i = 0; i < N; ++i)
                    acquired[i] = adapters[idx]->acquire(in.values[i]);
                for (size_t i = 0; i < N; ++i)
                    adapters[idx]->release(acquired[i]);

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
                adapters[idx]->setup(N * 2);

                Timer t;
                t.start();
                for (size_t i = 0; i < N; ++i)
                {
                    acquired[i] = adapters[idx]->acquire(in.values[i]);
                    prevent_opt_ptr(acquired[i]);
                }
                double elapsed = t.elapsed_ns();

                // Release outside timed region
                for (size_t i = 0; i < N; ++i)
                    adapters[idx]->release(acquired[i]);

                adapters[idx]->teardown();
                results[idx].samples.push_back(ns_per_op(elapsed, N));
            }
        }

        // Correctness check
        {
            adapters[0]->setup(N * 2);
            for (size_t i = 0; i < N; ++i)
                acquired[i] = adapters[0]->acquire(in.values[i]);

            // Verify all pointers are unique
            std::sort(acquired.begin(), acquired.end());
            bool all_unique = std::adjacent_find(acquired.begin(), acquired.end()) == acquired.end();
            if (!all_unique)
                std::cout << "  [WARNING] Duplicate pointers detected!\n";

            for (size_t i = 0; i < N; ++i)
                adapters[0]->release(acquired[i]);
            adapters[0]->teardown();
        }

        // Print results
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            std::cout << "  " << std::setw(26) << r.library << ": median=" << std::setw(8) << stats.median
                      << " ns/op  mean=" << std::setw(8) << stats.mean << " +/-" << std::setw(6) << stats.stddev << "\n";
        }
    }
}

// ============================================================================
// Interleaved Operations Benchmark
// ============================================================================

template <typename T>
void benchmark_interleaved(std::vector<std::unique_ptr<IPoolAdapter<T>>>& adapters,
                           const std::vector<size_t>& sizes,
                           const char* type_name)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Interleaved Acquire/Release (" << type_name << ")\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Contract: Realistic workload with interleaved operations\n";
    std::cout << "          50% acquire, 50% release (steady-state simulation)\n\n";

    std::mt19937_64 rng(g_config.seed);

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " operations ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

        std::vector<BenchResult> results;
        for (auto& adapter : adapters)
            results.push_back({adapter->name(), {}});

        // Pre-allocate storage for acquired pointers
        std::vector<T*> live_objects;
        live_objects.reserve(N);

        // Warmup
        for (size_t run = 0; run < WARMUP_RUNS(); ++run)
        {
            for (size_t idx = 0; idx < adapters.size(); ++idx)
            {
                adapters[idx]->setup(N);
                live_objects.clear();

                std::mt19937_64 op_rng(in.seed + run);
                for (size_t i = 0; i < N; ++i)
                {
                    bool do_acquire = live_objects.empty() || (op_rng() % 2 == 0 && live_objects.size() < N / 2);
                    if (do_acquire)
                    {
                        live_objects.push_back(adapters[idx]->acquire(in.values[i % in.values.size()]));
                    }
                    else if (!live_objects.empty())
                    {
                        size_t release_idx = op_rng() % live_objects.size();
                        adapters[idx]->release(live_objects[release_idx]);
                        live_objects[release_idx] = live_objects.back();
                        live_objects.pop_back();
                    }
                }

                // Cleanup
                for (T* obj : live_objects)
                    adapters[idx]->release(obj);
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
                live_objects.clear();

                std::mt19937_64 op_rng(in.seed + run + 1000);

                Timer t;
                t.start();
                size_t ops = 0;
                for (size_t i = 0; i < N; ++i)
                {
                    bool do_acquire = live_objects.empty() || (op_rng() % 2 == 0 && live_objects.size() < N / 2);
                    if (do_acquire)
                    {
                        T* obj = adapters[idx]->acquire(in.values[i % in.values.size()]);
                        prevent_opt_ptr(obj);
                        live_objects.push_back(obj);
                    }
                    else if (!live_objects.empty())
                    {
                        size_t release_idx = op_rng() % live_objects.size();
                        adapters[idx]->release(live_objects[release_idx]);
                        live_objects[release_idx] = live_objects.back();
                        live_objects.pop_back();
                    }
                    ++ops;
                }
                double elapsed = t.elapsed_ns();

                // Cleanup outside timed region
                for (T* obj : live_objects)
                    adapters[idx]->release(obj);

                adapters[idx]->teardown();
                results[idx].samples.push_back(ns_per_op(elapsed, ops));
            }
        }

        // Print results
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            std::cout << "  " << std::setw(26) << r.library << ": median=" << std::setw(8) << stats.median
                      << " ns/op  mean=" << std::setw(8) << stats.mean << " +/-" << std::setw(6) << stats.stddev << "\n";
        }
    }
}

// ============================================================================
// Pool Reuse Benchmark (Tests Free List Efficiency)
// ============================================================================

template <typename T>
void benchmark_pool_reuse(std::vector<std::unique_ptr<IPoolAdapter<T>>>& adapters,
                          const std::vector<size_t>& sizes,
                          const char* type_name)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Pool Reuse / Free List Efficiency (" << type_name << ")\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Contract: Acquire N, release all in random order, acquire N again\n";
    std::cout << "          Tests free list traversal and memory reuse\n\n";

    std::mt19937_64 rng(g_config.seed);

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " objects ---\n";
        print_cpu_context();
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        Inputs in = Inputs::make(N);

        std::vector<BenchResult> results;
        for (auto& adapter : adapters)
            results.push_back({adapter->name(), {}});

        std::vector<T*> acquired(N);

        // Measured runs
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            std::vector<size_t> order(adapters.size());
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), rng);

            for (size_t idx : order)
            {
                adapters[idx]->setup(N * 2);

                // Phase 1: Acquire all (outside timing)
                for (size_t i = 0; i < N; ++i)
                    acquired[i] = adapters[idx]->acquire(in.values[i]);

                // Phase 2: Release in random order (outside timing)
                for (size_t i : in.release_order)
                    adapters[idx]->release(acquired[i]);

                // Phase 3: Re-acquire all (TIMED - tests free list reuse)
                Timer t;
                t.start();
                for (size_t i = 0; i < N; ++i)
                {
                    acquired[i] = adapters[idx]->acquire(in.values[i]);
                    prevent_opt_ptr(acquired[i]);
                }
                double elapsed = t.elapsed_ns();

                // Cleanup
                for (size_t i = 0; i < N; ++i)
                    adapters[idx]->release(acquired[i]);

                adapters[idx]->teardown();
                results[idx].samples.push_back(ns_per_op(elapsed, N));
            }
        }

        // Print results
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& r : results)
        {
            auto stats = Statistics::compute(r.samples);
            std::cout << "  " << std::setw(26) << r.library << ": median=" << std::setw(8) << stats.median
                      << " ns/op  mean=" << std::setw(8) << stats.mean << " +/-" << std::setw(6) << stats.stddev << "\n";
        }
    }
}

// ============================================================================
// Specialized Acquire Benchmark (Trivial Types Only)
// ============================================================================

#if HAS_FATP_OBJECTPOOL
void benchmark_specialized_acquire()
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Specialized Acquire (SmallTrivial - fat_p only)\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Contract: Compare acquire() vs acquire_uninitialized() vs acquire_zeroed()\n";
    std::cout << "          Shows overhead of zero-initialization and default construction\n\n";

    print_cpu_context();

    const std::vector<size_t> sizes = {1000, 10000, 100000};

    for (size_t N : sizes)
    {
        std::cout << "\n--- N = " << N << " objects ---\n";
        cooling_delay(COOLING_DELAY_SIZE_MS, "size transition");

        std::vector<SmallTrivial*> acquired(N);

        fat_p::ObjectPool<SmallTrivial> pool(N);

        // Benchmark: acquire()
        std::vector<double> acquire_samples;
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            Timer t;
            t.start();
            for (size_t i = 0; i < N; ++i)
            {
                acquired[i] = pool.acquire(static_cast<int64_t>(i));
                prevent_opt_ptr(acquired[i]);
            }
            double elapsed = t.elapsed_ns();
            acquire_samples.push_back(ns_per_op(elapsed, N));

            for (size_t i = 0; i < N; ++i)
                pool.release(acquired[i]);
        }

        // Benchmark: acquire_uninitialized()
        std::vector<double> uninit_samples;
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            Timer t;
            t.start();
            for (size_t i = 0; i < N; ++i)
            {
                SmallTrivial* raw = pool.acquire_uninitialized();
                ::new (static_cast<void*>(raw)) SmallTrivial; // Start lifetime
                raw->a = static_cast<int64_t>(i);
                raw->b = static_cast<int64_t>(i) * 2;
                prevent_opt_ptr(raw);
                acquired[i] = raw;
            }
            double elapsed = t.elapsed_ns();
            uninit_samples.push_back(ns_per_op(elapsed, N));

            for (size_t i = 0; i < N; ++i)
                pool.release(acquired[i]);
        }

        // Benchmark: acquire_zeroed()
        std::vector<double> zeroed_samples;
        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            Timer t;
            t.start();
            for (size_t i = 0; i < N; ++i)
            {
                SmallTrivial* raw = pool.acquire_zeroed();
                ::new (static_cast<void*>(raw)) SmallTrivial; // Start lifetime
                prevent_opt_ptr(raw);
                acquired[i] = raw;
            }
            double elapsed = t.elapsed_ns();
            zeroed_samples.push_back(ns_per_op(elapsed, N));

            for (size_t i = 0; i < N; ++i)
                pool.release(acquired[i]);
        }

        // Print results
        auto acquire_stats = Statistics::compute(acquire_samples);
        auto uninit_stats = Statistics::compute(uninit_samples);
        auto zeroed_stats = Statistics::compute(zeroed_samples);

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  " << std::setw(26) << "acquire(value)" << ": median=" << std::setw(8) << acquire_stats.median
                  << " ns/op\n";
        std::cout << "  " << std::setw(26) << "acquire_uninitialized()" << ": median=" << std::setw(8)
                  << uninit_stats.median << " ns/op";
        if (acquire_stats.median > 0)
            std::cout << "  (" << std::setprecision(1) << (acquire_stats.median / uninit_stats.median) << "x faster)";
        std::cout << "\n";

        std::cout << "  " << std::setw(26) << "acquire_zeroed()" << ": median=" << std::setw(8) << zeroed_stats.median
                  << " ns/op\n";
    }
}
#endif

// ============================================================================
// Multi-threaded Contention Benchmark
// ============================================================================

#if HAS_FATP_OBJECTPOOL
void benchmark_multithreaded()
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Multi-threaded Contention (fat_p::ThreadSafeObjectPool)\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Contract: Concurrent acquire/release from multiple threads\n";
    std::cout << "          Tests lock contention and scalability\n\n";

    print_cpu_context();

    const size_t OPS_PER_THREAD = 10000;
    const std::vector<size_t> thread_counts = {1, 2, 4, 8};
    const size_t max_threads = std::min<size_t>(8, std::thread::hardware_concurrency());

    for (size_t num_threads : thread_counts)
    {
        if (num_threads > max_threads)
            continue;

        std::cout << "\n--- " << num_threads << " threads, " << OPS_PER_THREAD << " ops/thread ---\n";
        cooling_delay(COOLING_DELAY_SIZE_MS, "thread count transition");

        std::vector<double> samples;

        for (size_t run = 0; run < MEASURED_RUNS(); ++run)
        {
            fat_p::ThreadSafeObjectPool<MediumObject> pool(1024);

            std::atomic<bool> start_flag{false};
            std::atomic<size_t> total_ops{0};

            std::vector<std::thread> threads;
            threads.reserve(num_threads);

            auto worker = [&](size_t thread_id) {
                // Wait for start signal
                while (!start_flag.load(std::memory_order_acquire))
                    std::this_thread::yield();

                size_t local_ops = 0;
                for (size_t i = 0; i < OPS_PER_THREAD; ++i)
                {
                    MediumObject* obj = pool.acquire(static_cast<int64_t>(thread_id * 1000000 + i));
                    prevent_opt(obj->checksum());
                    pool.release(obj);
                    ++local_ops;
                }
                total_ops.fetch_add(local_ops, std::memory_order_relaxed);
            };

            // Create threads
            for (size_t t = 0; t < num_threads; ++t)
                threads.emplace_back(worker, t);

            // Start timing and signal threads
            Timer timer;
            timer.start();
            start_flag.store(true, std::memory_order_release);

            // Wait for all threads
            for (auto& th : threads)
                th.join();

            double elapsed = timer.elapsed_ns();
            size_t ops = total_ops.load();

            samples.push_back(ns_per_op(elapsed, ops));
        }

        auto stats = Statistics::compute(samples);
        double throughput = 1e9 / stats.median; // ops/sec

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  median=" << std::setw(8) << stats.median << " ns/op  throughput=" << std::setprecision(0)
                  << std::setw(10) << throughput << " ops/sec\n";
    }
}
#endif

// ============================================================================
// Memory Overhead Comparison
// ============================================================================

void print_memory_comparison()
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Memory Overhead Comparison (Theoretical)\n";
    std::cout << std::string(80, '=') << "\n\n";

    std::cout << "  Per-object overhead (in addition to object storage):\n";
#if HAS_FATP_OBJECTPOOL
    std::cout << "    fat_p::ObjectPool:    " << sizeof(void*) << " bytes (next pointer in free list)\n";
#endif
#if HAS_BOOST_POOL
    std::cout << "    boost::object_pool:   " << sizeof(void*) << " bytes (chunk linkage)\n";
#endif
    std::cout << "    std::pmr::pool:       " << sizeof(void*) << "-" << (sizeof(void*) * 2)
              << " bytes (block headers)\n";
    std::cout << "    new/delete:           " << sizeof(void*) << "-" << (sizeof(void*) + 16)
              << " bytes (malloc metadata)\n\n";

    std::cout << "  Feature comparison:\n";
    std::cout << "    Allocator             O(1)   Thread-Safe  Auto-Grow  RAII Wrapper  try_acquire\n";
    std::cout << "    -------------------------------------------------------------------------------\n";
#if HAS_FATP_OBJECTPOOL
    std::cout << "    fat_p::ObjectPool     Yes    Optional     Yes        Yes           Yes\n";
#endif
#if HAS_BOOST_POOL
    std::cout << "    boost::object_pool    Yes    No           Yes        No            No\n";
#endif
    std::cout << "    std::pmr::pool        Yes    No           Yes        No            No\n";
    std::cout << "    new/delete            No*    Yes          N/A        No            No\n";
    std::cout << "    (* malloc may have O(1) fast path but can degrade)\n\n";

#if HAS_FATP_OBJECTPOOL
    // Actual memory measurement
    constexpr size_t N = 10000;
    fat_p::ObjectPool<MediumObject> pool(N);

    auto stats = pool.stats();
    std::cout << "  fat_p::ObjectPool<MediumObject> with capacity " << N << ":\n";
    std::cout << "    sizeof(MediumObject): " << sizeof(MediumObject) << " bytes\n";
    std::cout << "    total_capacity:       " << stats.total_capacity << " objects\n";
    std::cout << "    num_blocks:           " << stats.num_blocks << "\n";
    std::cout << "    block_size:           " << stats.block_size << " objects\n";
#endif
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Load configuration from environment
    g_config = fat_p::bench::BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity)
    BenchmarkScope scope(!g_config.noScope);

    std::cout << std::string(80, '=') << "\n";
    std::cout << "  ObjectPool Comprehensive Benchmark Suite\n";
    std::cout << std::string(80, '=') << "\n";

    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS() << ", measured=" << MEASURED_RUNS() << ", seed=" << g_config.seed << ")\n";

    std::cout << "\nCompetitor libraries detected:\n";
#if HAS_FATP_OBJECTPOOL
    std::cout << "  [x] fat_p::ObjectPool\n";
#else
    std::cout << "  [ ] fat_p::ObjectPool (not found - include ObjectPool.h)\n";
#endif
#if HAS_BOOST_POOL
    std::cout << "  [x] boost::object_pool\n";
#else
    std::cout << "  [ ] boost::object_pool (install: vcpkg install boost-pool)\n";
#endif
#if HAS_FOONATHAN_MEMORY
    std::cout << "  [x] foonathan::memory_pool\n";
#else
    std::cout << "  [ ] foonathan::memory_pool (install: vcpkg install foonathan-memory)\n";
#endif
#if HAS_EASTL_POOL
    std::cout << "  [x] EASTL::fixed_pool\n";
#else
    std::cout << "  [ ] EASTL::fixed_pool (install: vcpkg install eastl)\n";
#endif
    std::cout << "  [x] std::pmr::pool_resource (C++17 standard)\n";
    std::cout << "  [x] new/delete (baseline)\n\n";

    // CPU detection
    fat_p::bench::print_cpu_detection_info(std::cout);
    std::cout << "\n";

    std::cout << "Design Invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run\n";
    std::cout << "  3. Setup/teardown outside timed regions\n";
    std::cout << "  4. All libraries observe same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n\n";

    // CPU stabilization
    if (!g_config.noStabilize)
    {
        std::cout << "Checking initial CPU state...\n";
        print_cpu_context("Initial");
        std::cout << "Waiting for CPU to stabilize...\n";
        if (!wait_for_cpu_stable(10.0, 30, 200, true))
            std::cout << "WARNING: CPU frequency still fluctuating, results may have higher variance.\n";
        std::cout << "\n";
    }

    // ========================================================================
    // Build adapters for each object size
    // ========================================================================

    // Small trivial objects
    {
        std::vector<std::unique_ptr<IPoolAdapter<SmallTrivial>>> adapters;
#if HAS_FATP_OBJECTPOOL
        adapters.push_back(std::make_unique<FatPPoolAdapter<SmallTrivial>>());
#endif
#if HAS_BOOST_POOL
        adapters.push_back(std::make_unique<BoostPoolAdapter<SmallTrivial>>());
#endif
        adapters.push_back(std::make_unique<PmrPoolAdapter<SmallTrivial>>());
        adapters.push_back(std::make_unique<NewDeleteAdapter<SmallTrivial>>());

        std::vector<size_t> sizes = {1000, 10000, 100000};

        benchmark_acquire_release(adapters, sizes, "SmallTrivial 16B");
        cooling_delay(COOLING_DELAY_SECTION_MS, "before bulk acquire");
        benchmark_bulk_acquire(adapters, sizes, "SmallTrivial 16B");
        cooling_delay(COOLING_DELAY_SECTION_MS, "before interleaved");
        benchmark_interleaved(adapters, sizes, "SmallTrivial 16B");
        cooling_delay(COOLING_DELAY_SECTION_MS, "before pool reuse");
        benchmark_pool_reuse(adapters, sizes, "SmallTrivial 16B");
    }

    // Medium objects
    cooling_delay(COOLING_DELAY_SECTION_MS, "before medium object benchmarks");
    {
        std::vector<std::unique_ptr<IPoolAdapter<MediumObject>>> adapters;
#if HAS_FATP_OBJECTPOOL
        adapters.push_back(std::make_unique<FatPPoolAdapter<MediumObject>>());
#endif
#if HAS_BOOST_POOL
        adapters.push_back(std::make_unique<BoostPoolAdapter<MediumObject>>());
#endif
        adapters.push_back(std::make_unique<PmrPoolAdapter<MediumObject>>());
        adapters.push_back(std::make_unique<NewDeleteAdapter<MediumObject>>());

        std::vector<size_t> sizes = {1000, 10000, 50000};

        benchmark_acquire_release(adapters, sizes, "MediumObject 64B");
        cooling_delay(COOLING_DELAY_SECTION_MS, "before bulk acquire");
        benchmark_bulk_acquire(adapters, sizes, "MediumObject 64B");
    }

    // Large objects
    cooling_delay(COOLING_DELAY_SECTION_MS, "before large object benchmarks");
    {
        std::vector<std::unique_ptr<IPoolAdapter<LargeObject>>> adapters;
#if HAS_FATP_OBJECTPOOL
        adapters.push_back(std::make_unique<FatPPoolAdapter<LargeObject>>());
#endif
#if HAS_BOOST_POOL
        adapters.push_back(std::make_unique<BoostPoolAdapter<LargeObject>>());
#endif
        adapters.push_back(std::make_unique<PmrPoolAdapter<LargeObject>>());
        adapters.push_back(std::make_unique<NewDeleteAdapter<LargeObject>>());

        std::vector<size_t> sizes = {1000, 10000};

        benchmark_acquire_release(adapters, sizes, "LargeObject 256B");
    }

    // Specialized acquire (fat_p only)
#if HAS_FATP_OBJECTPOOL
    cooling_delay(COOLING_DELAY_SECTION_MS, "before specialized acquire");
    benchmark_specialized_acquire();
#endif

    // Multi-threaded contention
#if HAS_FATP_OBJECTPOOL
    cooling_delay(COOLING_DELAY_SECTION_MS, "before multithreaded");
    benchmark_multithreaded();
#endif

    // Memory comparison
    print_memory_comparison();

    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << std::string(80, '=') << "\n";

    return 0;
}
