/**
 * @file benchmark_LockFreeQueue.cpp
 * @brief Comprehensive benchmarks for LockFreeQueue and LockFreeRingBuffer
 *
 * @layer Testing
 *
 * Benchmarks:
 * - Single-threaded throughput (enqueue/dequeue cycle)
 * - SPSC throughput (dedicated producer/consumer threads)
 * - MPMC throughput with scaling (1, 2, 4, 8, 12, 16 threads)
 * - Burst patterns (fill then drain)
 *
 * Competitors:
 * - std::mutex + std::queue (baseline - always included)
 * - moodycamel::ConcurrentQueue (optional via __has_include)
 * - boost::lockfree::queue (optional via __has_include)
 *
 * Compile (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread benchmark_LockFreeQueue.cpp -o bench_lfq
 *
 * Compile (with moodycamel):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread -I/path/to/concurrentqueue \
 *       benchmark_LockFreeQueue.cpp -o bench_lfq
 *
 * Windows (MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_LockFreeQueue.cpp
 *
 * Environment variables:
 *   FATP_BENCH_WARMUP_RUNS   - warmup batches (default: 3)
 *   FATP_BENCH_BATCHES       - measured batches (default: 15 Windows, 50 Linux)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_TARGET_WORK   - operations per batch (default: 1000000)
 *   FATP_BENCH_MIN_BATCH_MS  - minimum batch duration in ms (default: 50)
 *   FATP_BENCH_NO_SCOPE      - disable priority/affinity (default: unset)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
 */

/*
FATP_META:
  meta_version: 1
  component: LockFreeContainers
  file_role: benchmark
  path: components/LockFreeContainers/benchmarks/benchmark_LockFreeQueue.cpp
  layer: Testing
  namespace: fat_p
  summary: "benchmark file for LockFreeContainers"
  api_stability: in_work
  related:
    docs_search: "LockFreeContainers"
  hygiene:
    pragma_once: false
    include_guard: true
    defines_total: 8
    defines_unprefixed: 7
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/



#ifndef FATP_BENCH_ENABLE_FAA
#define FATP_BENCH_ENABLE_FAA 0
#endif

#include "LockFreeQueue.h"
#include "LockFreeRingBuffer.h"
#include "WorkQueue.h"

#if FATP_BENCH_ENABLE_FAA
#include "LockFreeQueueFAA.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fstream>
#include <sched.h>
#include <unistd.h>
#endif

// ============================================================================
// Competitor Auto-Detection (via __has_include)
// ============================================================================

// moodycamel::ConcurrentQueue - high-performance lock-free queue
// Rationale: Industry-standard lock-free queue, widely used in production
#if __has_include(<concurrentqueue/moodycamel/concurrentqueue.h>)
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#define HAS_MOODYCAMEL 1
#elif __has_include(<moodycamel/concurrentqueue.h>)
#include <moodycamel/concurrentqueue.h>
#define HAS_MOODYCAMEL 1
#elif __has_include("concurrentqueue.h")
#include "concurrentqueue.h"
#define HAS_MOODYCAMEL 1    
#else
#define HAS_MOODYCAMEL 0
#endif

// boost::lockfree::queue - Boost's lock-free queue implementation
// Rationale: Boost is widely deployed reference point
#if __has_include(<boost/lockfree/queue.hpp>)
#include <boost/lockfree/queue.hpp>
#define HAS_BOOST_LOCKFREE 1
#else
#define HAS_BOOST_LOCKFREE 0
#endif

// ============================================================================
// Benchmark Configuration (Canonical Environment Variables)
// ============================================================================

namespace
{

inline bool hasEnvVar(const char* name)
{
#ifdef _WIN32
    char buf[2];
    return GetEnvironmentVariableA(name, buf, sizeof(buf)) > 0;
#else
    return std::getenv(name) != nullptr;
#endif
}

inline std::string getEnvVar(const char* name, const char* defaultVal = "")
{
#ifdef _WIN32
    char buf[256];
    DWORD len = GetEnvironmentVariableA(name, buf, sizeof(buf));
    return (len > 0 && len < sizeof(buf)) ? std::string(buf) : defaultVal;
#else
    const char* val = std::getenv(name);
    return val ? val : defaultVal;
#endif
}

inline size_t getEnvSize(const char* name, size_t defaultVal)
{
    std::string val = getEnvVar(name);
    return val.empty() ? defaultVal : static_cast<size_t>(std::stoull(val));
}

struct BenchConfig
{
    size_t warmupRuns = 3;
    size_t measuredRuns = 15;
    uint64_t seed = 12345;
    size_t targetWork = 1000000;
    size_t minBatchMs = 50;
    bool noScope = false;
    bool noStabilize = false;
    bool noCooldown = false;
    std::string csvPath;
    std::string jsonPath;
};

BenchConfig loadConfig()
{
    BenchConfig cfg;

#if defined(_WIN32)
    cfg.measuredRuns = 15; // Windows: higher run-to-run variance
#else
    cfg.measuredRuns = 50; // Linux benefits from more samples
#endif

    cfg.warmupRuns = getEnvSize("FATP_BENCH_WARMUP_RUNS", cfg.warmupRuns);
    cfg.measuredRuns = getEnvSize("FATP_BENCH_BATCHES", cfg.measuredRuns);
    cfg.seed = getEnvSize("FATP_BENCH_SEED", cfg.seed);
    cfg.targetWork = getEnvSize("FATP_BENCH_TARGET_WORK", cfg.targetWork);
    cfg.minBatchMs = getEnvSize("FATP_BENCH_MIN_BATCH_MS", cfg.minBatchMs);
    cfg.noScope = hasEnvVar("FATP_BENCH_NO_SCOPE");
    cfg.noStabilize = hasEnvVar("FATP_BENCH_NO_STABILIZE");
    cfg.noCooldown = hasEnvVar("FATP_BENCH_NO_COOLDOWN");
    cfg.csvPath = getEnvVar("FATP_BENCH_OUTPUT_CSV");
    cfg.jsonPath = getEnvVar("FATP_BENCH_OUTPUT_JSON");

    return cfg;
}

void printConfig(const BenchConfig& cfg)
{
    std::cout << "Configuration:\n";
    std::cout << "  Warmup runs:    " << cfg.warmupRuns << "\n";
    std::cout << "  Measured runs:  " << cfg.measuredRuns << "\n";
    std::cout << "  Seed:           " << cfg.seed << "\n";
    std::cout << "  Target work:    " << cfg.targetWork << "\n";
    std::cout << "  Min batch ms:   " << cfg.minBatchMs << "\n";
    std::cout << "  Scope:          " << (cfg.noScope ? "disabled" : "enabled") << "\n";
    std::cout << "  Stabilize:      " << (cfg.noStabilize ? "disabled" : "enabled") << "\n";
    std::cout << "  Cooldown:       " << (cfg.noCooldown ? "disabled" : "enabled") << "\n";
    if (!cfg.csvPath.empty())
    {
        std::cout << "  CSV output:     " << cfg.csvPath << "\n";
    }
    if (!cfg.jsonPath.empty())
    {
        std::cout << "  JSON output:    " << cfg.jsonPath << "\n";
    }
    std::cout << "\n";
}

} // namespace

// ============================================================================
// CPU Frequency Monitoring
// ============================================================================

namespace
{

struct CpuFreqInfo
{
    double ref_freq_mhz = 0;
    double current_freq_mhz = 0;
    bool ref_is_max = false; // True if using max_freq fallback

    double throttle_percentage() const
    {
        if (current_freq_mhz <= 0 || ref_freq_mhz <= 0)
        {
            return 0;
        }
        return (1.0 - current_freq_mhz / ref_freq_mhz) * 100.0;
    }

    // CRITICAL: Only claim throttling when ref is true base frequency
    bool is_throttled() const
    {
        return !ref_is_max && throttle_percentage() > 5.0;
    }

    bool is_turbo() const
    {
        return !ref_is_max && current_freq_mhz > ref_freq_mhz * 1.05;
    }
};

CpuFreqInfo get_cpu_freq()
{
    CpuFreqInfo info;

#ifdef _WIN32
    // Windows: Read from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0,
                      KEY_READ,
                      &hKey) == ERROR_SUCCESS)
    {
        DWORD mhz = 0;
        DWORD size = sizeof(mhz);
        if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&mhz), &size) ==
            ERROR_SUCCESS)
        {
            info.ref_freq_mhz = static_cast<double>(mhz);
            info.current_freq_mhz = static_cast<double>(mhz);
            info.ref_is_max = false; // Windows registry gives reliable base
        }
        RegCloseKey(hKey);
    }
#else
    // Linux: Read from /sys/devices/system/cpu/cpu0/cpufreq/
    auto readFreq = [](const char* path) -> double {
        std::ifstream f(path);
        if (f)
        {
            double khz;
            f >> khz;
            return khz / 1000.0;
        }
        return 0;
    };

    info.current_freq_mhz = readFreq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");

    // Prefer base_frequency (reliable base clock)
    info.ref_freq_mhz = readFreq("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency");
    if (info.ref_freq_mhz > 0)
    {
        info.ref_is_max = false;
    }
    else
    {
        // Fallback to max (which is turbo, not base)
        info.ref_freq_mhz = readFreq("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        info.ref_is_max = true;
    }
#endif

    return info;
}

void print_cpu_context(const char* label = nullptr)
{
    auto info = get_cpu_freq();
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::cout << "[";
    if (label)
    {
        std::cout << label << " ";
    }
    
#ifdef _WIN32
    std::tm tm_buf;
    localtime_s(&tm_buf, &time);
    std::cout << std::put_time(&tm_buf, "%H:%M:%S") << "] ";
#else
    std::cout << std::put_time(std::localtime(&time), "%H:%M:%S") << "] ";
#endif

    if (info.current_freq_mhz > 0)
    {
        std::cout << "CPU: " << static_cast<int>(info.current_freq_mhz) << " MHz";

        if (info.ref_freq_mhz > 0)
        {
            const char* ref_label = info.ref_is_max ? "max" : "base";
            std::cout << " (" << ref_label << ": " << static_cast<int>(info.ref_freq_mhz) << ")";

            // Only print throttle/turbo status when ref is reliable
            if (!info.ref_is_max)
            {
                if (info.is_throttled())
                {
                    std::cout << " [THROTTLED " << std::fixed << std::setprecision(1)
                              << info.throttle_percentage() << "%]";
                }
                else if (info.is_turbo())
                {
                    std::cout << " [TURBO]";
                }
            }
        }
    }
    else
    {
        std::cout << "CPU: (frequency unavailable)";
    }
    std::cout << "\n";
}

} // namespace

// ============================================================================
// BenchmarkScope (Windows Priority/Affinity)
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
namespace
{

class BenchmarkScope
{
    DWORD old_priority_ = 0;
    DWORD_PTR old_affinity_ = 0;
    bool applied_ = false;

public:
    explicit BenchmarkScope(bool verbose = false)
    {
        if (hasEnvVar("FATP_BENCH_NO_SCOPE"))
        {
            return;
        }

        HANDLE proc = GetCurrentProcess();
        old_priority_ = GetPriorityClass(proc);
        SetPriorityClass(proc, HIGH_PRIORITY_CLASS);

        HANDLE thread = GetCurrentThread();
        DWORD_PTR proc_mask = 0;
        DWORD_PTR sys_mask = 0;
        DWORD_PTR target = 1;
        if (GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask) && proc_mask)
        {
            DWORD_PTR nonzero = proc_mask & ~static_cast<DWORD_PTR>(1);
            DWORD_PTR pick = nonzero ? nonzero : proc_mask;
            target = pick & (~pick + 1); // lowest set bit
        }
        old_affinity_ = SetThreadAffinityMask(thread, target);
        applied_ = true;

        if (verbose)
        {
            std::cout << "[BenchmarkScope] High priority, CPU" << (target > 1 ? " non-0" : " 0")
                      << " affinity\n";
        }
    }

    ~BenchmarkScope()
    {
        if (!applied_)
        {
            return;
        }

        HANDLE proc = GetCurrentProcess();
        SetPriorityClass(proc, old_priority_);

        HANDLE thread = GetCurrentThread();
        if (old_affinity_ != 0)
        {
            SetThreadAffinityMask(thread, old_affinity_);
        }
    }

    BenchmarkScope(const BenchmarkScope&) = delete;
    BenchmarkScope& operator=(const BenchmarkScope&) = delete;
};

} // namespace
#else
namespace
{

class BenchmarkScope
{
public:
    explicit BenchmarkScope(bool = false)
    {
    }
};

} // namespace
#endif

// ============================================================================
// Timer
// ============================================================================

namespace
{

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

} // namespace

// ============================================================================
// Statistics
// ============================================================================

namespace
{

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
            constexpr double z = 1.96; // 95% CI (normal approx)
            s.ci95_low = s.mean - z * se;
            s.ci95_high = s.mean + z * se;
        }

        return s;
    }
};

} // namespace

// ============================================================================
// Preventing Dead Code Elimination
// ============================================================================

namespace
{

template <typename T>
inline void DoNotOptimize(T const& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "r,m"(value) : "memory");
#else
    // MSVC: use volatile sink
    volatile auto sink = value;
    (void)sink;
#endif
}

template <typename T>
inline void DoNotOptimize(T& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : "+r,m"(value) : : "memory");
#else
    volatile auto sink = value;
    (void)sink;
#endif
}

inline void ClobberMemory()
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : : "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif
}

} // namespace

// ============================================================================
// Output Formatting
// ============================================================================

namespace
{

void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

void print_table_header()
{
    std::cout << std::setw(35) << std::left << "Library" << std::setw(12) << std::right << "Median"
              << std::setw(12) << "Mean" << std::setw(10) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(90, '-') << "\n";
}

void print_result_row(const std::string& name, const Statistics& s, const std::string& unit = "ns/op")
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(35) << std::left << name << std::setw(12) << std::right << s.median
              << std::setw(12) << s.mean << std::setw(10) << s.stddev << "  [" << std::setw(8)
              << s.ci95_low << ", " << std::setw(8) << s.ci95_high << "]  " << unit << "\n";

    // Sanity check: high variance warning
    if (s.stddev > s.median && s.median > 0)
    {
        std::cout << "  [NOTE] high variance (stddev " << s.stddev << " > median " << s.median << ")\n";
    }
}

void print_contract_note(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

} // namespace

// ============================================================================
// Adapter Interface (IAdapter)
// ============================================================================

namespace
{

/**
 * @brief Abstract adapter interface for benchmark operations
 *
 * Adapters contain:
 * - No timing logic
 * - No statistics
 * - No policy decisions
 *
 * They are dumb, mechanical mappings to library APIs.
 */
struct IAdapter
{
    virtual ~IAdapter() = default;

    /// @brief Human-readable name of the library
    virtual const char* name() const = 0;

    /// @brief Setup before timed region (allocate, populate)
    virtual void setup(size_t N) = 0;

    /// @brief Teardown after timed region (deallocate)
    virtual void teardown() = 0;

    /// @brief Run the operation, return number of ops performed
    virtual size_t run_operation() = 0;
};

} // namespace

// ============================================================================
// Single-Threaded Adapters
// ============================================================================

namespace
{

constexpr size_t kQueueCapacity = 131072; // 128K

constexpr size_t kWorkQueueShardCount = 16;
constexpr size_t kWorkQueueShardCapacity = kQueueCapacity / kWorkQueueShardCount;

static_assert((kWorkQueueShardCapacity & (kWorkQueueShardCapacity - 1)) == 0,
              "WorkQueue shard capacity must be power of 2");
static_assert(kWorkQueueShardCount * kWorkQueueShardCapacity == kQueueCapacity,
              "WorkQueue total capacity must match kQueueCapacity");

// -----------------------------------------------------------------------------
// fat_p::LockFreeQueue Adapter
// -----------------------------------------------------------------------------

class FatpLockFreeQueueAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeQueue";
    }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<fat_p::LockFreeQueue<int, kQueueCapacity>>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_operation() override
    {
        // Enqueue all
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->enqueue(static_cast<int>(i));
        }

        // Dequeue all
        int value;
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->dequeue(value);
            DoNotOptimize(value);
        }

        return mN * 2; // enqueue + dequeue
    }

private:
    size_t mN = 0;
    std::unique_ptr<fat_p::LockFreeQueue<int, kQueueCapacity>> mQueue;
};

// -----------------------------------------------------------------------------
// fat_p::WorkQueue (sharded) Adapter
// -----------------------------------------------------------------------------

class FatpWorkQueueAdapter : public IAdapter
{
private:
    using QueueType =
        fat_p::work_queue::WorkQueue<int, kWorkQueueShardCount, kWorkQueueShardCapacity>;

public:
    const char* name() const override
    {
        return "fat_p::WorkQueue (sharded)";
    }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<QueueType>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_operation() override
    {
        auto pTok = mQueue->makeProducerToken();
        auto cTok = mQueue->makeConsumerToken();

        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->enqueue(pTok, static_cast<int>(i));
        }

        int value;
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->dequeue(cTok, value);
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<QueueType> mQueue;
};

// -----------------------------------------------------------------------------
#if FATP_BENCH_ENABLE_FAA

// fat_p::LockFreeQueueFAA (FAA-based MPMC) Adapter
// -----------------------------------------------------------------------------

class FatpLockFreeQueueFAAAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeQueueFAA";
    }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<fat_p::LockFreeQueueFAA<int, kQueueCapacity>>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->enqueue(static_cast<int>(i));
        }

        int value;
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->dequeue(value);
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<fat_p::LockFreeQueueFAA<int, kQueueCapacity>> mQueue;
};

#endif

// -----------------------------------------------------------------------------
// fat_p::LockFreeRingBuffer (SPSC) Adapter
// -----------------------------------------------------------------------------

class FatpRingBufferSpscAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeRingBuffer (SPSC)";
    }

    void setup(size_t n) override
    {
        mN = n;
        mBuffer = std::make_unique<fat_p::LockFreeRingBuffer<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mBuffer.reset();
    }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mBuffer->push(static_cast<int>(i));
        }

        int value;
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mBuffer->pop(value);
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<fat_p::LockFreeRingBuffer<int>> mBuffer;
};

// -----------------------------------------------------------------------------
// fat_p::LockFreeRingBufferMPMC Adapter
// -----------------------------------------------------------------------------

class FatpRingBufferMpmcAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeRingBufferMPMC";
    }

    void setup(size_t n) override
    {
        mN = n;
        mBuffer = std::make_unique<fat_p::LockFreeRingBufferMPMC<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mBuffer.reset();
    }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mBuffer->push(static_cast<int>(i));
        }

        for (size_t i = 0; i < mN; ++i)
        {
            auto val = mBuffer->pop();
            DoNotOptimize(val);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<fat_p::LockFreeRingBufferMPMC<int>> mBuffer;
};

// -----------------------------------------------------------------------------
// std::mutex + std::queue Adapter (Baseline)
// -----------------------------------------------------------------------------

class MutexQueueAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "std::mutex + std::queue (baseline)";
    }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::queue<int>{};
    }

    void teardown() override
    {
        mQueue = std::queue<int>{};
    }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mQueue.push(static_cast<int>(i));
        }

        for (size_t i = 0; i < mN; ++i)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            int value = mQueue.front();
            mQueue.pop();
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::mutex mMutex;
    std::queue<int> mQueue;
};

// -----------------------------------------------------------------------------
// moodycamel::ConcurrentQueue Adapter
// -----------------------------------------------------------------------------

#if HAS_MOODYCAMEL
class MoodycamelAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "moodycamel::ConcurrentQueue";
    }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<moodycamel::ConcurrentQueue<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            mQueue->enqueue(static_cast<int>(i));
        }

        int value;
        for (size_t i = 0; i < mN; ++i)
        {
            mQueue->try_dequeue(value);
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<moodycamel::ConcurrentQueue<int>> mQueue;
};
#endif

// -----------------------------------------------------------------------------
// boost::lockfree::queue Adapter
// -----------------------------------------------------------------------------

#if HAS_BOOST_LOCKFREE
class BoostLockfreeAdapter : public IAdapter
{
public:
    const char* name() const override
    {
        return "boost::lockfree::queue";
    }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<boost::lockfree::queue<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            mQueue->push(static_cast<int>(i));
        }

        int value;
        for (size_t i = 0; i < mN; ++i)
        {
            mQueue->pop(value);
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<boost::lockfree::queue<int>> mQueue;
};
#endif

} // namespace

// ============================================================================
// Round-Robin Benchmark Runner
// ============================================================================

namespace
{

/**
 * @brief Run benchmarks with round-robin execution to eliminate thermal drift bias
 *
 * Design invariants (per style guide):
 * 1. Each measured run executes exactly one timed iteration per library
 * 2. Library execution order is randomized per run
 * 3. Setup and teardown occur OUTSIDE timed regions
 * 4. All libraries observe the same distribution of machine states
 * 5. Median is the primary reported statistic
 */
void run_round_robin_benchmark(const std::string& title,
                               const std::string& contract,
                               std::vector<std::unique_ptr<IAdapter>>& adapters,
                               size_t work_per_batch,
                               size_t warmup_runs,
                               size_t measured_runs,
                               uint64_t seed)
{
    print_header(title);
    print_cpu_context("START");
    print_contract_note(contract);

    // Sample storage per adapter
    std::map<IAdapter*, std::vector<double>> samples;
    for (auto& a : adapters)
    {
        samples[a.get()] = {};
    }

    // Build adapter pointer list for shuffling
    std::vector<IAdapter*> adapter_ptrs;
    for (auto& a : adapters)
    {
        adapter_ptrs.push_back(a.get());
    }

    std::mt19937_64 rng(seed);

    // Warmup runs (not recorded)
    for (size_t run = 0; run < warmup_runs; ++run)
    {
        std::shuffle(adapter_ptrs.begin(), adapter_ptrs.end(), rng);
        for (IAdapter* a : adapter_ptrs)
        {
            a->setup(work_per_batch);

            Timer t;
            t.start();
            size_t ops = a->run_operation();
            (void)t.elapsed_ns();
            (void)ops;

            a->teardown();
        }
    }

    // Measured runs with round-robin shuffling
    for (size_t run = 0; run < measured_runs; ++run)
    {
        std::shuffle(adapter_ptrs.begin(), adapter_ptrs.end(), rng);

        for (IAdapter* a : adapter_ptrs)
        {
            a->setup(work_per_batch);
            ClobberMemory();

            Timer t;
            t.start();
            size_t ops = a->run_operation();
            double elapsed = t.elapsed_ns();

            a->teardown();

            double ns_per_op = elapsed / static_cast<double>(ops);
            samples[a].push_back(ns_per_op);
        }
    }

    // Compute statistics and print results
    print_table_header();

    for (auto& a : adapters)
    {
        auto stats = Statistics::compute(samples[a.get()]);
        print_result_row(a->name(), stats);
    }

    std::cout << "\n";
    print_cpu_context("END");
}

} // namespace

// ============================================================================
// SPSC Adapters (Producer/Consumer Threads)
// ============================================================================

namespace
{

/**
 * @brief Abstract adapter for SPSC (single-producer single-consumer) benchmarks
 */
struct ISpscAdapter
{
    virtual ~ISpscAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t capacity) = 0;
    virtual void teardown() = 0;
    virtual size_t run_spsc(size_t ops_per_side) = 0;
};

// -----------------------------------------------------------------------------
// fat_p::LockFreeQueue SPSC Adapter
// -----------------------------------------------------------------------------

class FatpLockFreeQueueSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeQueue";
    }

    void setup(size_t /*capacity*/) override
    {
        mQueue = std::make_unique<fat_p::LockFreeQueue<int, kQueueCapacity>>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<size_t> consumed{0};

        std::thread consumer([this, ops, &consumed]() {
            int value;
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                if (mQueue->dequeue(value))
                {
                    DoNotOptimize(value);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        // Producer in main thread
        for (size_t i = 0; i < ops; ++i)
        {
            while (!mQueue->enqueue(static_cast<int>(i)))
            {
                // Spin
            }
        }

        consumer.join();
        return ops * 2;
    }

private:
    std::unique_ptr<fat_p::LockFreeQueue<int, kQueueCapacity>> mQueue;
};

// -----------------------------------------------------------------------------
// fat_p::LockFreeRingBuffer (SPSC) - Native SPSC Adapter
// -----------------------------------------------------------------------------

class FatpRingBufferNativeSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeRingBuffer (SPSC)";
    }

    void setup(size_t /*capacity*/) override
    {
        mBuffer = std::make_unique<fat_p::LockFreeRingBuffer<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mBuffer.reset();
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<size_t> consumed{0};

        std::thread consumer([this, ops, &consumed]() {
            int value;
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                if (mBuffer->pop(value))
                {
                    DoNotOptimize(value);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        for (size_t i = 0; i < ops; ++i)
        {
            while (!mBuffer->push(static_cast<int>(i)))
            {
            }
        }

        consumer.join();
        return ops * 2;
    }

private:
    std::unique_ptr<fat_p::LockFreeRingBuffer<int>> mBuffer;
};

// -----------------------------------------------------------------------------
// std::mutex + std::queue SPSC Adapter
// -----------------------------------------------------------------------------

class MutexQueueSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override
    {
        return "std::mutex + std::queue (baseline)";
    }

    void setup(size_t /*capacity*/) override
    {
        mQueue = std::queue<int>{};
    }

    void teardown() override
    {
        mQueue = std::queue<int>{};
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<size_t> consumed{0};

        std::thread consumer([this, ops, &consumed]() {
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                int value;
                bool got = false;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (!mQueue.empty())
                    {
                        value = mQueue.front();
                        mQueue.pop();
                        got = true;
                    }
                }
                if (got)
                {
                    DoNotOptimize(value);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        for (size_t i = 0; i < ops; ++i)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mQueue.push(static_cast<int>(i));
        }

        consumer.join();
        return ops * 2;
    }

private:
    std::mutex mMutex;
    std::queue<int> mQueue;
};

// -----------------------------------------------------------------------------
// moodycamel::ConcurrentQueue SPSC Adapter
// -----------------------------------------------------------------------------

#if HAS_MOODYCAMEL
class MoodycamelSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override
    {
        return "moodycamel::ConcurrentQueue";
    }

    void setup(size_t /*capacity*/) override
    {
        mQueue = std::make_unique<moodycamel::ConcurrentQueue<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<size_t> consumed{0};

        std::thread consumer([this, ops, &consumed]() {
            int value;
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                if (mQueue->try_dequeue(value))
                {
                    DoNotOptimize(value);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        for (size_t i = 0; i < ops; ++i)
        {
            while (!mQueue->enqueue(static_cast<int>(i)))
            {
            }
        }

        consumer.join();
        return ops * 2;
    }

private:
    std::unique_ptr<moodycamel::ConcurrentQueue<int>> mQueue;
};
#endif

// -----------------------------------------------------------------------------
// boost::lockfree::queue SPSC Adapter
// -----------------------------------------------------------------------------

#if HAS_BOOST_LOCKFREE
class BoostLockfreeSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override
    {
        return "boost::lockfree::queue";
    }

    void setup(size_t /*capacity*/) override
    {
        mQueue = std::make_unique<boost::lockfree::queue<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<size_t> consumed{0};

        std::thread consumer([this, ops, &consumed]() {
            int value;
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                if (mQueue->pop(value))
                {
                    DoNotOptimize(value);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });

        for (size_t i = 0; i < ops; ++i)
        {
            while (!mQueue->push(static_cast<int>(i)))
            {
            }
        }

        consumer.join();
        return ops * 2;
    }

private:
    std::unique_ptr<boost::lockfree::queue<int>> mQueue;
};
#endif

/**
 * @brief Run SPSC benchmark with round-robin execution
 */
void run_spsc_benchmark(const BenchConfig& cfg)
{
    print_header("SPSC Throughput (1 producer, 1 consumer threads)");
    print_cpu_context("START");
    print_contract_note("Dedicated producer and consumer threads. Native SPSC use case.");

    std::vector<std::unique_ptr<ISpscAdapter>> adapters;
    adapters.push_back(std::make_unique<FatpLockFreeQueueSpscAdapter>());
    adapters.push_back(std::make_unique<FatpRingBufferNativeSpscAdapter>());
    adapters.push_back(std::make_unique<MutexQueueSpscAdapter>());
#if HAS_MOODYCAMEL
    adapters.push_back(std::make_unique<MoodycamelSpscAdapter>());
#endif
#if HAS_BOOST_LOCKFREE
    adapters.push_back(std::make_unique<BoostLockfreeSpscAdapter>());
#endif

    std::map<ISpscAdapter*, std::vector<double>> samples;
    for (auto& a : adapters)
    {
        samples[a.get()] = {};
    }

    std::vector<ISpscAdapter*> adapter_ptrs;
    for (auto& a : adapters)
    {
        adapter_ptrs.push_back(a.get());
    }

    std::mt19937_64 rng(cfg.seed);
    size_t ops = cfg.targetWork;

    // Warmup
    for (size_t run = 0; run < cfg.warmupRuns; ++run)
    {
        std::shuffle(adapter_ptrs.begin(), adapter_ptrs.end(), rng);
        for (ISpscAdapter* a : adapter_ptrs)
        {
            a->setup(kQueueCapacity);
            Timer t;
            t.start();
            a->run_spsc(ops);
            (void)t.elapsed_ns();
            a->teardown();
        }
    }

    // Measured with round-robin
    for (size_t run = 0; run < cfg.measuredRuns; ++run)
    {
        std::shuffle(adapter_ptrs.begin(), adapter_ptrs.end(), rng);

        for (ISpscAdapter* a : adapter_ptrs)
        {
            a->setup(kQueueCapacity);
            ClobberMemory();

            Timer t;
            t.start();
            size_t total_ops = a->run_spsc(ops);
            double elapsed = t.elapsed_ns();

            a->teardown();

            double ns_per_op = elapsed / static_cast<double>(total_ops);
            samples[a].push_back(ns_per_op);
        }
    }

    print_table_header();
    for (auto& a : adapters)
    {
        auto stats = Statistics::compute(samples[a.get()]);
        print_result_row(a->name(), stats);
    }

    std::cout << "\n";
    print_cpu_context("END");
}

} // namespace

// ============================================================================
// MPMC Adapters (Multi-Producer Multi-Consumer)
// ============================================================================

namespace
{

/**
 * @brief Abstract adapter for MPMC scaling benchmarks
 */
struct IMpmcAdapter
{
    virtual ~IMpmcAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup() = 0;
    virtual void teardown() = 0;
    virtual size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) = 0;
};

// -----------------------------------------------------------------------------
// fat_p::LockFreeQueue MPMC Adapter
// -----------------------------------------------------------------------------

class FatpLockFreeQueueMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeQueue";
    }

    void setup() override
    {
        mQueue = std::make_unique<fat_p::LockFreeQueue<int, kQueueCapacity>>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        // Producers
        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    while (!mQueue->enqueue(static_cast<int>(t * 1000000 + i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        // Consumers
        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                int value;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->dequeue(value))
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<fat_p::LockFreeQueue<int, kQueueCapacity>> mQueue;
};


// -----------------------------------------------------------------------------
// fat_p::WorkQueue (sharded) MPMC Adapter
// -----------------------------------------------------------------------------

class FatpWorkQueueMpmcAdapter : public IMpmcAdapter
{
private:
    using QueueType =
        fat_p::work_queue::WorkQueue<int, kWorkQueueShardCount, kWorkQueueShardCapacity>;

public:
    const char* name() const override
    {
        return "fat_p::WorkQueue (sharded)";
    }

    void setup() override
    {
        mQueue = std::make_unique<QueueType>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                auto tok = mQueue->makeProducerToken();

                while (!start.load(std::memory_order_acquire))
                {
                }

                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    const int value = static_cast<int>(t * 1000000 + i);
                    while (!mQueue->enqueue(tok, value))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                auto tok = mQueue->makeConsumerToken();

                while (!start.load(std::memory_order_acquire))
                {
                }

                int value;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->dequeue(tok, value))
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<QueueType> mQueue;
};

// -----------------------------------------------------------------------------
// fat_p::WorkQueue (RoundRobin) MPMC Adapter
// -----------------------------------------------------------------------------

class FatpWorkQueueRoundRobinMpmcAdapter : public IMpmcAdapter
{
private:
    using QueueType = fat_p::work_queue::WorkQueue<
        int,
        kWorkQueueShardCount,
        kWorkQueueShardCapacity,
        fat_p::work_queue::RoundRobinRoutingPolicy>;

public:
    const char* name() const override
    {
        return "fat_p::WorkQueue (round-robin)";
    }

    void setup() override
    {
        mQueue = std::make_unique<QueueType>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                auto tok = mQueue->makeProducerToken();

                while (!start.load(std::memory_order_acquire))
                {
                }

                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    const int value = static_cast<int>(t * 1000000 + i);
                    while (!mQueue->enqueue(tok, value))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                auto tok = mQueue->makeConsumerToken();

                while (!start.load(std::memory_order_acquire))
                {
                }

                int value;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->dequeue(tok, value))
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<QueueType> mQueue;
};

// -----------------------------------------------------------------------------
// fat_p::WorkQueue (Stride<3>) MPMC Adapter
// -----------------------------------------------------------------------------

class FatpWorkQueueStride3MpmcAdapter : public IMpmcAdapter
{
private:
    using QueueType = fat_p::work_queue::WorkQueue<
        int,
        kWorkQueueShardCount,
        kWorkQueueShardCapacity,
        fat_p::work_queue::StrideRoutingPolicy<3>>;

public:
    const char* name() const override
    {
        return "fat_p::WorkQueue (stride-3)";
    }

    void setup() override
    {
        mQueue = std::make_unique<QueueType>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                auto tok = mQueue->makeProducerToken();

                while (!start.load(std::memory_order_acquire))
                {
                }

                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    const int value = static_cast<int>(t * 1000000 + i);
                    while (!mQueue->enqueue(tok, value))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                auto tok = mQueue->makeConsumerToken();

                while (!start.load(std::memory_order_acquire))
                {
                }

                int value;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->dequeue(tok, value))
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<QueueType> mQueue;
};

#if FATP_BENCH_ENABLE_FAA

// -----------------------------------------------------------------------------
// fat_p::LockFreeQueueFAA MPMC Adapter
// -----------------------------------------------------------------------------

class FatpLockFreeQueueFAAMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeQueueFAA";
    }

    void setup() override
    {
        mQueue = std::make_unique<fat_p::LockFreeQueueFAA<int, kQueueCapacity>>();
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    while (!mQueue->enqueue(static_cast<int>(t * 1000000 + i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                int value;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->dequeue(value))
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<fat_p::LockFreeQueueFAA<int, kQueueCapacity>> mQueue;
};

#endif

// -----------------------------------------------------------------------------
// fat_p::LockFreeRingBufferMPMC Adapter
// -----------------------------------------------------------------------------

class FatpRingBufferMpmcMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override
    {
        return "fat_p::LockFreeRingBufferMPMC";
    }

    void setup() override
    {
        mBuffer = std::make_unique<fat_p::LockFreeRingBufferMPMC<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mBuffer.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    while (!mBuffer->push(static_cast<int>(t * 1000000 + i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    auto val = mBuffer->pop();
                    if (val)
                    {
                        DoNotOptimize(*val);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<fat_p::LockFreeRingBufferMPMC<int>> mBuffer;
};

// -----------------------------------------------------------------------------
// std::mutex + std::queue MPMC Adapter
// -----------------------------------------------------------------------------

class MutexQueueMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override
    {
        return "std::mutex + std::queue (baseline)";
    }

    void setup() override
    {
        mQueue = std::queue<int>{};
    }

    void teardown() override
    {
        mQueue = std::queue<int>{};
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mQueue.push(static_cast<int>(t * 1000000 + i));
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    int value;
                    bool got = false;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        if (!mQueue.empty())
                        {
                            value = mQueue.front();
                            mQueue.pop();
                            got = true;
                        }
                    }
                    if (got)
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::mutex mMutex;
    std::queue<int> mQueue;
};

// -----------------------------------------------------------------------------
// moodycamel::ConcurrentQueue MPMC Adapter
// -----------------------------------------------------------------------------

#if HAS_MOODYCAMEL
class MoodycamelMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override
    {
        return "moodycamel::ConcurrentQueue";
    }

    void setup() override
    {
        mQueue = std::make_unique<moodycamel::ConcurrentQueue<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    while (!mQueue->enqueue(static_cast<int>(t * 1000000 + i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                int value;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->try_dequeue(value))
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<moodycamel::ConcurrentQueue<int>> mQueue;
};
#endif

// -----------------------------------------------------------------------------
// boost::lockfree::queue MPMC Adapter
// -----------------------------------------------------------------------------

#if HAS_BOOST_LOCKFREE
class BoostLockfreeMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override
    {
        return "boost::lockfree::queue";
    }

    void setup() override
    {
        mQueue = std::make_unique<boost::lockfree::queue<int>>(kQueueCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    while (!mQueue->push(static_cast<int>(t * 1000000 + i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                while (!start.load(std::memory_order_acquire))
                {
                }
                int value;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->pop(value))
                    {
                        DoNotOptimize(value);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& th : threads)
        {
            th.join();
        }

        return total_ops * 2;
    }

private:
    std::unique_ptr<boost::lockfree::queue<int>> mQueue;
};
#endif

/**
 * @brief Run MPMC scaling benchmark with round-robin per thread count
 */
void run_mpmc_scaling_benchmark(const BenchConfig& cfg)
{
    print_header("MPMC Scaling (N producers, N consumers)");
    print_cpu_context("START");
    print_contract_note("Equal producer and consumer threads. Tests lock-free scaling.");

    std::vector<std::unique_ptr<IMpmcAdapter>> adapters;
    adapters.push_back(std::make_unique<FatpLockFreeQueueMpmcAdapter>());
    adapters.push_back(std::make_unique<FatpWorkQueueMpmcAdapter>());
    adapters.push_back(std::make_unique<FatpWorkQueueRoundRobinMpmcAdapter>());
    adapters.push_back(std::make_unique<FatpWorkQueueStride3MpmcAdapter>());
#if FATP_BENCH_ENABLE_FAA
    adapters.push_back(std::make_unique<FatpLockFreeQueueFAAMpmcAdapter>());
#endif
    adapters.push_back(std::make_unique<FatpRingBufferMpmcMpmcAdapter>());
    adapters.push_back(std::make_unique<MutexQueueMpmcAdapter>());
#if HAS_MOODYCAMEL
    adapters.push_back(std::make_unique<MoodycamelMpmcAdapter>());
#endif
#if HAS_BOOST_LOCKFREE
    adapters.push_back(std::make_unique<BoostLockfreeMpmcAdapter>());
#endif

    size_t max_threads = std::thread::hardware_concurrency();
    std::vector<size_t> thread_counts = {1, 2, 4};
    if (max_threads >= 8)
    {
        thread_counts.push_back(8);
    }
    if (max_threads >= 12)
    {
        thread_counts.push_back(12);
    }
    if (max_threads >= 16)
    {
        thread_counts.push_back(16);
    }

    size_t ops_per_thread = cfg.targetWork / 10;
    std::mt19937_64 rng(cfg.seed);

    std::cout << "Thread counts: ";
    for (size_t tc : thread_counts)
    {
        std::cout << tc << " ";
    }
    std::cout << "\n\n";

    // Column header
    std::cout << std::setw(35) << std::left << "Library";
    for (size_t tc : thread_counts)
    {
        std::cout << std::setw(12) << std::right << (std::to_string(tc) + "T");
    }
    std::cout << "\n" << std::string(35 + 12 * thread_counts.size(), '-') << "\n";

    // Build adapter pointer list
    std::vector<IMpmcAdapter*> adapter_ptrs;
    for (auto& a : adapters)
    {
        adapter_ptrs.push_back(a.get());
    }

    // For each library
    for (auto& adapter : adapters)
    {
        std::cout << std::setw(35) << std::left << adapter->name();

        // For each thread count
        for (size_t tc : thread_counts)
        {
            std::vector<double> samples;

            // Warmup
            for (size_t run = 0; run < cfg.warmupRuns; ++run)
            {
                adapter->setup();
                Timer t;
                t.start();
                adapter->run_mpmc(tc, tc, ops_per_thread);
                (void)t.elapsed_ns();
                adapter->teardown();
            }

            // Measured runs
            for (size_t run = 0; run < cfg.measuredRuns; ++run)
            {
                adapter->setup();
                ClobberMemory();

                Timer t;
                t.start();
                size_t ops = adapter->run_mpmc(tc, tc, ops_per_thread);
                double elapsed = t.elapsed_ns();

                adapter->teardown();

                double ns_per_op = elapsed / static_cast<double>(ops);
                samples.push_back(ns_per_op);
            }

            auto stats = Statistics::compute(samples);
            std::cout << std::setw(12) << std::right << std::fixed << std::setprecision(1)
                      << stats.median;
        }
        std::cout << " ns/op\n";
    }

    std::cout << "\n";
    print_cpu_context("END");
}

struct AsymCase
{
    const char* mLabel = nullptr;
    size_t mProducers = 0;
    size_t mConsumers = 0;
};

/**
 * @brief Run asymmetric MPMC benchmark (MPSC, SPMC, and unbalanced)
 */
void run_asymmetric_mpmc_benchmark(const BenchConfig& cfg)
{
    print_header("Asymmetric MPMC (MPSC, SPMC, Unbalanced)");
    print_cpu_context("START");
    print_contract_note("Tests non-symmetric producer/consumer ratios.");

    std::vector<std::unique_ptr<IMpmcAdapter>> adapters;
    adapters.push_back(std::make_unique<FatpLockFreeQueueMpmcAdapter>());
    adapters.push_back(std::make_unique<FatpWorkQueueMpmcAdapter>());
#if FATP_BENCH_ENABLE_FAA
    adapters.push_back(std::make_unique<FatpLockFreeQueueFAAMpmcAdapter>());
#endif
    adapters.push_back(std::make_unique<FatpRingBufferMpmcMpmcAdapter>());
    adapters.push_back(std::make_unique<MutexQueueMpmcAdapter>());
#if HAS_MOODYCAMEL
    adapters.push_back(std::make_unique<MoodycamelMpmcAdapter>());
#endif
#if HAS_BOOST_LOCKFREE
    adapters.push_back(std::make_unique<BoostLockfreeMpmcAdapter>());
#endif

    // Match the table ordering you printed.
    const std::array<AsymCase, 6> cases = {
        AsymCase{"8P:1C", 8, 1},
        AsymCase{"4P:1C", 4, 1},
        AsymCase{"1P:8C", 1, 8},
        AsymCase{"1P:4C", 1, 4},
        AsymCase{"8P:2C", 8, 2},
        AsymCase{"2P:8C", 2, 8},
    };

    std::cout << "\n";
    std::cout << std::setw(37) << std::left << "Library";
    for (const auto& c : cases)
    {
        std::cout << std::setw(10) << std::right << c.mLabel;
    }
    std::cout << "\n" << std::string(37 + 10 * cases.size(), '-') << "\n";

    for (auto& adapter : adapters)
    {
        std::cout << std::setw(37) << std::left << adapter->name();

        for (const auto& c : cases)
        {
            // Keep total work approximately cfg.targetWork per case.
            // Ensure at least 1 op per producer.
            const size_t ops_per_producer =
                std::max<size_t>(1, cfg.targetWork / std::max<size_t>(1, c.mProducers));

            std::vector<double> samples;
            samples.reserve(cfg.measuredRuns);

            // Warmup
            for (size_t run = 0; run < cfg.warmupRuns; ++run)
            {
                adapter->setup();
                Timer t;
                t.start();
                adapter->run_mpmc(c.mProducers, c.mConsumers, ops_per_producer);
                (void)t.elapsed_ns();
                adapter->teardown();
            }

            // Measured
            for (size_t run = 0; run < cfg.measuredRuns; ++run)
            {
                adapter->setup();
                ClobberMemory();

                Timer t;
                t.start();
                const size_t ops = adapter->run_mpmc(c.mProducers, c.mConsumers, ops_per_producer);
                const double elapsed = t.elapsed_ns();

                adapter->teardown();

                const double ns_per_op = elapsed / static_cast<double>(ops);
                samples.push_back(ns_per_op);
            }

            const auto stats = Statistics::compute(samples);
            std::cout << std::setw(10) << std::right << std::fixed << std::setprecision(1)
                      << stats.median;
        }

        std::cout << " ns/op\n";
    }

    std::cout << "\n";
    print_cpu_context("END");
}

} // namespace

// ============================================================================
// Correctness Guardrails
// ============================================================================

namespace
{

/**
 * @brief Verify queue correctness before running benchmarks
 *
 * Rule: Each benchmark case must include at least one correctness validation
 * outside the timed region.
 */
bool verify_correctness()
{
    std::cout << "Correctness verification:\n";

    // Verify LockFreeQueue FIFO ordering
    {
        fat_p::LockFreeQueue<int, 1024> queue;
        for (int i = 0; i < 100; ++i)
        {
            (void)queue.enqueue(i);
        }
        for (int i = 0; i < 100; ++i)
        {
            int value;
            if (!queue.dequeue(value) || value != i)
            {
                std::cout << "  [FAIL] LockFreeQueue FIFO verification\n";
                return false;
            }
        }
        std::cout << "  [PASS] LockFreeQueue FIFO ordering\n";
    }

    // Verify LockFreeRingBuffer FIFO ordering
    {
        fat_p::LockFreeRingBuffer<int> buffer(1024);
        for (int i = 0; i < 100; ++i)
        {
            (void)buffer.push(i);
        }
        for (int i = 0; i < 100; ++i)
        {
            auto val = buffer.pop();
            if (!val || *val != i)
            {
                std::cout << "  [FAIL] LockFreeRingBuffer FIFO verification\n";
                return false;
            }
        }
        std::cout << "  [PASS] LockFreeRingBuffer (SPSC) FIFO ordering\n";
    }

    // Verify LockFreeRingBufferMPMC FIFO ordering
    {
        fat_p::LockFreeRingBufferMPMC<int> buffer(1024);
        for (int i = 0; i < 100; ++i)
        {
            (void)buffer.push(i);
        }
        for (int i = 0; i < 100; ++i)
        {
            auto val = buffer.pop();
            if (!val || *val != i)
            {
                std::cout << "  [FAIL] LockFreeRingBufferMPMC FIFO verification\n";
                return false;
            }
        }
        std::cout << "  [PASS] LockFreeRingBufferMPMC FIFO ordering\n";
    }

    std::cout << "\n";
    return true;
}

} // namespace

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "================================================================================\n";
    std::cout << "  Fat-P LockFreeQueue / LockFreeRingBuffer Benchmark Suite\n";
    std::cout << "================================================================================\n\n";

    // Print competitor libraries detected (required by style guide)
    std::cout << "Competitor libraries detected:\n";
    std::cout << "  [x] std::mutex + std::queue (baseline)\n";
#if HAS_MOODYCAMEL
    std::cout << "  [x] moodycamel::ConcurrentQueue\n";
#else
    std::cout << "  [ ] moodycamel::ConcurrentQueue "
              << "(install: https://github.com/cameron314/concurrentqueue)\n";
#endif
#if HAS_BOOST_LOCKFREE
    std::cout << "  [x] boost::lockfree::queue\n";
#else
    std::cout << "  [ ] boost::lockfree::queue (install: vcpkg install boost-lockfree)\n";
#endif
    std::cout << "\n";

    // Load and print configuration (required by style guide)
    auto cfg = loadConfig();
    printConfig(cfg);

    // Print initial CPU context (required by style guide)
    print_cpu_context("INIT");
    std::cout << "\n";

    // Correctness guardrails (required by style guide)
    if (!verify_correctness())
    {
        std::cerr << "Correctness verification failed. Aborting benchmarks.\n";
        return 1;
    }

    {
        BenchmarkScope scope(/*verbose=*/true);

        // 1. Single-threaded benchmark with round-robin
        {
            std::vector<std::unique_ptr<IAdapter>> adapters;
            adapters.push_back(std::make_unique<FatpLockFreeQueueAdapter>());
            adapters.push_back(std::make_unique<FatpWorkQueueAdapter>());
#if FATP_BENCH_ENABLE_FAA
            adapters.push_back(std::make_unique<FatpLockFreeQueueFAAAdapter>());
#endif
            adapters.push_back(std::make_unique<FatpRingBufferSpscAdapter>());
            adapters.push_back(std::make_unique<FatpRingBufferMpmcAdapter>());
            adapters.push_back(std::make_unique<MutexQueueAdapter>());
#if HAS_MOODYCAMEL
            adapters.push_back(std::make_unique<MoodycamelAdapter>());
#endif
#if HAS_BOOST_LOCKFREE
            adapters.push_back(std::make_unique<BoostLockfreeAdapter>());
#endif

            const size_t single_thread_work = std::min(cfg.targetWork, kQueueCapacity);
            if (single_thread_work != cfg.targetWork)
            {
                std::cout << "  [NOTE] Single-thread target work clamped from "
                          << cfg.targetWork << " to " << single_thread_work
                          << " to avoid capacity overflow.\n";
            }

            run_round_robin_benchmark(
                "Single-Threaded Throughput (enqueue + dequeue cycle)",
                "Pure queue overhead without contention. Measures enqueue+dequeue per op.",
                adapters,
                single_thread_work,
                cfg.warmupRuns,
                cfg.measuredRuns,
                cfg.seed);
        }

        // 2. SPSC benchmark with round-robin
        run_spsc_benchmark(cfg);

        // 3. MPMC scaling benchmark
        run_mpmc_scaling_benchmark(cfg);

        // 4. Asymmetric MPMC benchmark
        run_asymmetric_mpmc_benchmark(cfg);
    }

    // Summary
    print_header("Summary");
    std::cout << "Benchmarks completed.\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";
    print_cpu_context("FINAL");

    return 0;
}
