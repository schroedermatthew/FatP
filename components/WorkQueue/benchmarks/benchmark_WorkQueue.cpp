/**
 * @file benchmark_WorkQueue.cpp
 * @brief Comprehensive benchmarks for fat_p::WorkQueue (sharded lock-free MPMC)
 *
 * @layer Testing
 *
 * Benchmarks:
 * - Single-threaded throughput (enqueue/dequeue cycle, measures shard overhead)
 * - SPSC throughput (dedicated producer/consumer threads)
 * - MPMC symmetric scaling (N producers, N consumers for N = 1, 2, 4, 8, ...)
 * - MPMC asymmetric (8P:1C, 4P:1C, 1P:8C, 1P:4C, 8P:2C, 2P:8C)
 * - Burst/drain pattern (fill to capacity, then drain)
 *
 * Competitors:
 * - fat_p::WorkQueue (primary) -- sharded lock-free MPMC with token affinity
 * - fat_p::LockFreeQueue (sibling) -- single CAS-based lock-free MPMC from same lib
 * - std::mutex + std::queue (baseline) -- textbook correct-but-slow approach
 * - moodycamel::ConcurrentQueue (optional) -- industry-standard high-perf MPMC
 * - boost::lockfree::queue (optional) -- widely deployed Boost reference
 *
 * Compile (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread benchmark_WorkQueue.cpp -o bench_wq
 *
 * Compile (with moodycamel):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread -I/path/to/concurrentqueue \
 *       benchmark_WorkQueue.cpp -o bench_wq
 *
 * Windows (MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_WorkQueue.cpp
 *
 * Environment variables:
 *   FATP_BENCH_WARMUP_RUNS   - warmup batches (default: 3)
 *   FATP_BENCH_BATCHES       - measured batches (default: 15 Windows, 50 Linux)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_TARGET_WORK   - operations per batch (default: 1000000)
 *   FATP_BENCH_MIN_BATCH_MS  - minimum batch duration in ms (default: 50)
 *   FATP_BENCH_NO_SCOPE      - disable priority/affinity (default: unset)
 *   FATP_BENCH_NO_STABILIZE  - disable CPU stabilization (default: unset)
 *   FATP_BENCH_NO_COOLDOWN   - disable cool-down sleeps (default: unset)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
 *   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
 */

/*
FATP_META:
  meta_version: 1
  component: WorkQueue
  file_role: benchmark
  path: components/WorkQueue/benchmarks/benchmark_WorkQueue.cpp
  layer: Testing
  namespace: fat_p::work_queue
  summary: "Benchmark file for WorkQueue (sharded lock-free MPMC)"
  api_stability: in_work
  related:
    docs_search: "WorkQueue"
    tests:
      - components/WorkQueue/tests/test_WorkQueue.cpp
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 5
    defines_unprefixed: 4
    undefs_total: 0
    includes_windows_h: true
*/

#include "WorkQueue.h"
#include "LockFreeQueue.h"

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
#include <set>
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
// Rationale: Industry-standard lock-free MPMC queue, widely used in production.
// Distinct design family: implicit per-producer blocks, unbounded growth.
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

// boost::lockfree::queue - Boost's lock-free MPMC queue
// Rationale: Boost is a widely deployed reference point.
// Distinct design family: single node-based lock-free queue with global FIFO.
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
    bool ref_is_max = false;

    double throttle_percentage() const
    {
        if (current_freq_mhz <= 0 || ref_freq_mhz <= 0)
        {
            return 0;
        }
        return (1.0 - current_freq_mhz / ref_freq_mhz) * 100.0;
    }

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
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0,
                      KEY_READ,
                      &hKey) == ERROR_SUCCESS)
    {
        DWORD mhz = 0;
        DWORD size = sizeof(mhz);
        if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS)
        {
            info.ref_freq_mhz = static_cast<double>(mhz);
            info.current_freq_mhz = static_cast<double>(mhz);
            info.ref_is_max = false;
        }
        RegCloseKey(hKey);
    }
#else
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

    info.ref_freq_mhz = readFreq("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency");
    if (info.ref_freq_mhz > 0)
    {
        info.ref_is_max = false;
    }
    else
    {
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
            target = pick & (~pick + 1);
        }
        old_affinity_ = SetThreadAffinityMask(thread, target);
        applied_ = true;

        if (verbose)
        {
            std::cout << "[BenchmarkScope] High priority, CPU"
                      << (target > 1 ? " non-0" : " 0") << " affinity\n";
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
    explicit BenchmarkScope(bool = false) {}
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

    void start() { t0 = clock::now(); }

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
            constexpr double z = 1.96;
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
    std::cout << std::setw(35) << std::left << "Library"
              << std::setw(12) << std::right << "Median"
              << std::setw(12) << "Mean"
              << std::setw(10) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(90, '-') << "\n";
}

void print_result_row(const std::string& name, const Statistics& s,
                      const std::string& unit = "ns/op")
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(35) << std::left << name
              << std::setw(12) << std::right << s.median
              << std::setw(12) << s.mean
              << std::setw(10) << s.stddev
              << "  [" << std::setw(8) << s.ci95_low
              << ", " << std::setw(8) << s.ci95_high << "]  " << unit << "\n";

    if (s.stddev > s.median && s.median > 0)
    {
        std::cout << "  [NOTE] high variance (stddev " << s.stddev
                  << " > median " << s.median << ")\n";
    }
}

void print_contract_note(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

void cooldown_sleep(const BenchConfig& cfg, int ms)
{
    if (!cfg.noCooldown)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

} // namespace

// ============================================================================
// Queue Configuration Constants
// ============================================================================

namespace
{

constexpr size_t kQueueCapacity = 131072; // 128K total capacity

// WorkQueue sharding: 16 shards x 8192 slots = 131072 total
constexpr size_t kWorkQueueShardCount = 16;
constexpr size_t kWorkQueueShardCapacity = kQueueCapacity / kWorkQueueShardCount;

static_assert((kWorkQueueShardCapacity & (kWorkQueueShardCapacity - 1)) == 0,
              "WorkQueue shard capacity must be power of 2");
static_assert(kWorkQueueShardCount * kWorkQueueShardCapacity == kQueueCapacity,
              "WorkQueue total capacity must match kQueueCapacity");

} // namespace

// ============================================================================
// Single-Threaded Adapters
// ============================================================================

namespace
{

struct IAdapter
{
    virtual ~IAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup(size_t N) = 0;
    virtual void teardown() = 0;
    virtual size_t run_operation() = 0;
};

// ---------------------------------------------------------------------------
// fat_p::WorkQueue (primary)
// ---------------------------------------------------------------------------

class WorkQueueAdapter : public IAdapter
{
    using QueueType =
        fat_p::work_queue::WorkQueue<int, kWorkQueueShardCount, kWorkQueueShardCapacity>;

public:
    const char* name() const override { return "fat_p::WorkQueue (16 shards)"; }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<QueueType>();
    }

    void teardown() override { mQueue.reset(); }

    size_t run_operation() override
    {
        auto pTok = mQueue->makeProducerToken();
        auto cTok = mQueue->makeConsumerToken();

        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->enqueue(pTok, static_cast<int>(i));
        }

        int value = 0;
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

// ---------------------------------------------------------------------------
// fat_p::LockFreeQueue (sibling - single lock-free MPMC queue)
// ---------------------------------------------------------------------------

class LockFreeQueueAdapter : public IAdapter
{
public:
    const char* name() const override { return "fat_p::LockFreeQueue"; }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<fat_p::LockFreeQueue<int, kQueueCapacity>>();
    }

    void teardown() override { mQueue.reset(); }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->enqueue(static_cast<int>(i));
        }

        int value = 0;
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->dequeue(value);
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<fat_p::LockFreeQueue<int, kQueueCapacity>> mQueue;
};

// ---------------------------------------------------------------------------
// std::mutex + std::queue (baseline)
// ---------------------------------------------------------------------------

class MutexQueueAdapter : public IAdapter
{
public:
    const char* name() const override { return "std::mutex + std::queue (baseline)"; }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::queue<int>();
    }

    void teardown() override
    {
        mQueue = std::queue<int>();
    }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mQueue.push(static_cast<int>(i));
        }

        int value = 0;
        for (size_t i = 0; i < mN; ++i)
        {
            std::lock_guard<std::mutex> lk(mMutex);
            value = mQueue.front();
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

// ---------------------------------------------------------------------------
// moodycamel::ConcurrentQueue (optional)
// ---------------------------------------------------------------------------

#if HAS_MOODYCAMEL
class MoodycamelAdapter : public IAdapter
{
public:
    const char* name() const override { return "moodycamel::ConcurrentQueue"; }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<moodycamel::ConcurrentQueue<int>>(n);
    }

    void teardown() override { mQueue.reset(); }

    size_t run_operation() override
    {
        moodycamel::ProducerToken pTok(*mQueue);

        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->enqueue(pTok, static_cast<int>(i));
        }

        moodycamel::ConsumerToken cTok(*mQueue);
        int value = 0;
        for (size_t i = 0; i < mN; ++i)
        {
            (void)mQueue->try_dequeue(cTok, value);
            DoNotOptimize(value);
        }

        return mN * 2;
    }

private:
    size_t mN = 0;
    std::unique_ptr<moodycamel::ConcurrentQueue<int>> mQueue;
};
#endif

// ---------------------------------------------------------------------------
// boost::lockfree::queue (optional)
// ---------------------------------------------------------------------------

#if HAS_BOOST_LOCKFREE
class BoostLockfreeAdapter : public IAdapter
{
public:
    const char* name() const override { return "boost::lockfree::queue"; }

    void setup(size_t n) override
    {
        mN = n;
        mQueue = std::make_unique<boost::lockfree::queue<int>>(n);
    }

    void teardown() override { mQueue.reset(); }

    size_t run_operation() override
    {
        for (size_t i = 0; i < mN; ++i)
        {
            while (!mQueue->push(static_cast<int>(i)))
            {
            }
        }

        int value = 0;
        for (size_t i = 0; i < mN; ++i)
        {
            while (!mQueue->pop(value))
            {
            }
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
// Single-Threaded Round-Robin Runner
// ============================================================================

namespace
{

void run_single_threaded_benchmark(std::vector<std::unique_ptr<IAdapter>>& adapters,
                                   size_t work_per_batch,
                                   size_t warmup_runs,
                                   size_t measured_runs,
                                   uint64_t seed)
{
    print_header("Single-Threaded Throughput (enqueue + dequeue cycle)");
    print_cpu_context("START");
    print_contract_note(
        "Pure queue overhead without contention. Measures enqueue+dequeue per op. "
        "WorkQueue pays shard-routing cost even single-threaded; this quantifies that overhead.");

    std::map<IAdapter*, std::vector<double>> samples;
    for (auto& a : adapters)
    {
        samples[a.get()] = {};
    }

    std::vector<IAdapter*> adapter_ptrs;
    for (auto& a : adapters)
    {
        adapter_ptrs.push_back(a.get());
    }

    std::mt19937_64 rng(seed);

    // Warmup
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
// MPMC Adapter Interface
// ============================================================================

namespace
{

struct IMpmcAdapter
{
    virtual ~IMpmcAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup() = 0;
    virtual void teardown() = 0;
    virtual size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) = 0;
};

// ---------------------------------------------------------------------------
// fat_p::WorkQueue MPMC Adapter (primary)
// ---------------------------------------------------------------------------

class WorkQueueMpmcAdapter : public IMpmcAdapter
{
    using QueueType =
        fat_p::work_queue::WorkQueue<int, kWorkQueueShardCount, kWorkQueueShardCapacity>;

public:
    const char* name() const override { return "fat_p::WorkQueue (16 shards)"; }

    void setup() override { mQueue = std::make_unique<QueueType>(); }
    void teardown() override { mQueue.reset(); }

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
                    while (!mQueue->enqueue(tok, static_cast<int>(t * 1000000 + i)))
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
                int value = 0;
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

// ---------------------------------------------------------------------------
// fat_p::LockFreeQueue MPMC Adapter (sibling)
// ---------------------------------------------------------------------------

class LockFreeQueueMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override { return "fat_p::LockFreeQueue"; }

    void setup() override
    {
        mQueue = std::make_unique<fat_p::LockFreeQueue<int, kQueueCapacity>>();
    }

    void teardown() override { mQueue.reset(); }

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
                int value = 0;
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

// ---------------------------------------------------------------------------
// std::mutex + std::queue MPMC Adapter (baseline)
// ---------------------------------------------------------------------------

class MutexQueueMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override { return "std::mutex + std::queue (baseline)"; }

    void setup() override
    {
        std::lock_guard<std::mutex> lk(mMutex);
        mQueue = std::queue<int>();
    }

    void teardown() override
    {
        std::lock_guard<std::mutex> lk(mMutex);
        mQueue = std::queue<int>();
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
                    std::lock_guard<std::mutex> lk(mMutex);
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
                    int value = 0;
                    bool got = false;
                    {
                        std::lock_guard<std::mutex> lk(mMutex);
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
    std::mutex mMutex;
    std::queue<int> mQueue;
};

// ---------------------------------------------------------------------------
// moodycamel::ConcurrentQueue MPMC Adapter (optional)
// ---------------------------------------------------------------------------

#if HAS_MOODYCAMEL
class MoodycamelMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override { return "moodycamel::ConcurrentQueue"; }

    void setup() override
    {
        mQueue = std::make_unique<moodycamel::ConcurrentQueue<int>>(kQueueCapacity);
    }

    void teardown() override { mQueue.reset(); }

    size_t run_mpmc(size_t producers, size_t consumers, size_t ops_per_producer) override
    {
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        size_t total_ops = producers * ops_per_producer;

        for (size_t t = 0; t < producers; ++t)
        {
            threads.emplace_back([this, t, ops_per_producer, &start]() {
                moodycamel::ProducerToken tok(*mQueue);
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < ops_per_producer; ++i)
                {
                    while (!mQueue->enqueue(tok, static_cast<int>(t * 1000000 + i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < consumers; ++t)
        {
            threads.emplace_back([this, total_ops, &consumed, &start]() {
                moodycamel::ConsumerToken tok(*mQueue);
                while (!start.load(std::memory_order_acquire))
                {
                }
                int value = 0;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->try_dequeue(tok, value))
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
    std::unique_ptr<moodycamel::ConcurrentQueue<int>> mQueue;
};
#endif

// ---------------------------------------------------------------------------
// boost::lockfree::queue MPMC Adapter (optional)
// ---------------------------------------------------------------------------

#if HAS_BOOST_LOCKFREE
class BoostLockfreeMpmcAdapter : public IMpmcAdapter
{
public:
    const char* name() const override { return "boost::lockfree::queue"; }

    void setup() override
    {
        mQueue = std::make_unique<boost::lockfree::queue<int>>(kQueueCapacity);
    }

    void teardown() override { mQueue.reset(); }

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
                int value = 0;
                while (consumed.load(std::memory_order_relaxed) < total_ops)
                {
                    if (mQueue->pop(value))
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
    std::unique_ptr<boost::lockfree::queue<int>> mQueue;
};
#endif

} // namespace

// ============================================================================
// SPSC Benchmark
// ============================================================================

namespace
{

struct ISpscAdapter
{
    virtual ~ISpscAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup() = 0;
    virtual void teardown() = 0;
    virtual size_t run_spsc(size_t ops_per_side) = 0;
};

// ---------------------------------------------------------------------------
// fat_p::WorkQueue SPSC Adapter
// ---------------------------------------------------------------------------

class WorkQueueSpscAdapter : public ISpscAdapter
{
    using QueueType =
        fat_p::work_queue::WorkQueue<int, kWorkQueueShardCount, kWorkQueueShardCapacity>;

public:
    const char* name() const override { return "fat_p::WorkQueue (16 shards)"; }

    void setup() override { mQueue = std::make_unique<QueueType>(); }
    void teardown() override { mQueue.reset(); }

    size_t run_spsc(size_t ops_per_side) override
    {
        std::atomic<bool> start{false};

        std::thread producer([this, ops_per_side, &start]() {
            auto tok = mQueue->makeProducerToken();
            while (!start.load(std::memory_order_acquire))
            {
            }
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->enqueue(tok, static_cast<int>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([this, ops_per_side, &start]() {
            auto tok = mQueue->makeConsumerToken();
            while (!start.load(std::memory_order_acquire))
            {
            }
            int value = 0;
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->dequeue(tok, value))
                {
                    std::this_thread::yield();
                }
                DoNotOptimize(value);
            }
        });

        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();

        return ops_per_side * 2;
    }

private:
    std::unique_ptr<QueueType> mQueue;
};

// ---------------------------------------------------------------------------
// fat_p::LockFreeQueue SPSC Adapter
// ---------------------------------------------------------------------------

class LockFreeQueueSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override { return "fat_p::LockFreeQueue"; }

    void setup() override
    {
        mQueue = std::make_unique<fat_p::LockFreeQueue<int, kQueueCapacity>>();
    }

    void teardown() override { mQueue.reset(); }

    size_t run_spsc(size_t ops_per_side) override
    {
        std::atomic<bool> start{false};

        std::thread producer([this, ops_per_side, &start]() {
            while (!start.load(std::memory_order_acquire))
            {
            }
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->enqueue(static_cast<int>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([this, ops_per_side, &start]() {
            while (!start.load(std::memory_order_acquire))
            {
            }
            int value = 0;
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->dequeue(value))
                {
                    std::this_thread::yield();
                }
                DoNotOptimize(value);
            }
        });

        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();

        return ops_per_side * 2;
    }

private:
    std::unique_ptr<fat_p::LockFreeQueue<int, kQueueCapacity>> mQueue;
};

// ---------------------------------------------------------------------------
// std::mutex + std::queue SPSC Adapter
// ---------------------------------------------------------------------------

class MutexQueueSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override { return "std::mutex + std::queue (baseline)"; }

    void setup() override
    {
        std::lock_guard<std::mutex> lk(mMutex);
        mQueue = std::queue<int>();
    }

    void teardown() override
    {
        std::lock_guard<std::mutex> lk(mMutex);
        mQueue = std::queue<int>();
    }

    size_t run_spsc(size_t ops_per_side) override
    {
        std::atomic<bool> start{false};

        std::thread producer([this, ops_per_side, &start]() {
            while (!start.load(std::memory_order_acquire))
            {
            }
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                std::lock_guard<std::mutex> lk(mMutex);
                mQueue.push(static_cast<int>(i));
            }
        });

        std::thread consumer([this, ops_per_side, &start]() {
            while (!start.load(std::memory_order_acquire))
            {
            }
            int value = 0;
            size_t received = 0;
            while (received < ops_per_side)
            {
                bool got = false;
                {
                    std::lock_guard<std::mutex> lk(mMutex);
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
                    ++received;
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });

        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();

        return ops_per_side * 2;
    }

private:
    std::mutex mMutex;
    std::queue<int> mQueue;
};

// ---------------------------------------------------------------------------
// moodycamel::ConcurrentQueue SPSC Adapter
// ---------------------------------------------------------------------------

#if HAS_MOODYCAMEL
class MoodycamelSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override { return "moodycamel::ConcurrentQueue"; }

    void setup() override
    {
        mQueue = std::make_unique<moodycamel::ConcurrentQueue<int>>(kQueueCapacity);
    }

    void teardown() override { mQueue.reset(); }

    size_t run_spsc(size_t ops_per_side) override
    {
        std::atomic<bool> start{false};

        std::thread producer([this, ops_per_side, &start]() {
            moodycamel::ProducerToken tok(*mQueue);
            while (!start.load(std::memory_order_acquire))
            {
            }
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->enqueue(tok, static_cast<int>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([this, ops_per_side, &start]() {
            moodycamel::ConsumerToken tok(*mQueue);
            while (!start.load(std::memory_order_acquire))
            {
            }
            int value = 0;
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->try_dequeue(tok, value))
                {
                    std::this_thread::yield();
                }
                DoNotOptimize(value);
            }
        });

        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();

        return ops_per_side * 2;
    }

private:
    std::unique_ptr<moodycamel::ConcurrentQueue<int>> mQueue;
};
#endif

// ---------------------------------------------------------------------------
// boost::lockfree::queue SPSC Adapter
// ---------------------------------------------------------------------------

#if HAS_BOOST_LOCKFREE
class BoostLockfreeSpscAdapter : public ISpscAdapter
{
public:
    const char* name() const override { return "boost::lockfree::queue"; }

    void setup() override
    {
        mQueue = std::make_unique<boost::lockfree::queue<int>>(kQueueCapacity);
    }

    void teardown() override { mQueue.reset(); }

    size_t run_spsc(size_t ops_per_side) override
    {
        std::atomic<bool> start{false};

        std::thread producer([this, ops_per_side, &start]() {
            while (!start.load(std::memory_order_acquire))
            {
            }
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->push(static_cast<int>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([this, ops_per_side, &start]() {
            while (!start.load(std::memory_order_acquire))
            {
            }
            int value = 0;
            for (size_t i = 0; i < ops_per_side; ++i)
            {
                while (!mQueue->pop(value))
                {
                    std::this_thread::yield();
                }
                DoNotOptimize(value);
            }
        });

        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();

        return ops_per_side * 2;
    }

private:
    std::unique_ptr<boost::lockfree::queue<int>> mQueue;
};
#endif

// ---------------------------------------------------------------------------
// SPSC Runner
// ---------------------------------------------------------------------------

void run_spsc_benchmark(const BenchConfig& cfg)
{
    print_header("SPSC Throughput (1 producer, 1 consumer)");
    print_cpu_context("START");
    print_contract_note(
        "Dedicated producer and consumer threads. Start barrier ensures simultaneous launch. "
        "WorkQueue is not optimized for SPSC; this measures the baseline cost of shard routing "
        "when contention is absent.");

    std::vector<std::unique_ptr<ISpscAdapter>> adapters;
    adapters.push_back(std::make_unique<WorkQueueSpscAdapter>());
    adapters.push_back(std::make_unique<LockFreeQueueSpscAdapter>());
    adapters.push_back(std::make_unique<MutexQueueSpscAdapter>());
#if HAS_MOODYCAMEL
    adapters.push_back(std::make_unique<MoodycamelSpscAdapter>());
#endif
#if HAS_BOOST_LOCKFREE
    adapters.push_back(std::make_unique<BoostLockfreeSpscAdapter>());
#endif

    size_t ops_per_side = cfg.targetWork;

    print_table_header();

    for (auto& adapter : adapters)
    {
        // Warmup
        for (size_t run = 0; run < cfg.warmupRuns; ++run)
        {
            adapter->setup();
            Timer t;
            t.start();
            adapter->run_spsc(ops_per_side);
            (void)t.elapsed_ns();
            adapter->teardown();
        }

        // Measured
        std::vector<double> samples;
        for (size_t run = 0; run < cfg.measuredRuns; ++run)
        {
            adapter->setup();
            ClobberMemory();

            Timer t;
            t.start();
            size_t ops = adapter->run_spsc(ops_per_side);
            double elapsed = t.elapsed_ns();

            adapter->teardown();

            double ns_per_op = elapsed / static_cast<double>(ops);
            samples.push_back(ns_per_op);
        }

        auto stats = Statistics::compute(samples);
        print_result_row(adapter->name(), stats);
    }

    std::cout << "\n";
    print_cpu_context("END");
}

} // namespace

// ============================================================================
// MPMC Symmetric Scaling Benchmark
// ============================================================================

namespace
{

void run_mpmc_scaling_benchmark(const BenchConfig& cfg)
{
    print_header("MPMC Symmetric Scaling (N producers, N consumers)");
    print_cpu_context("START");
    print_contract_note(
        "Equal producer and consumer threads. Start barrier ensures simultaneous launch. "
        "Tests lock-free scaling under increasing contention. "
        "WorkQueue should dominate at >= 4 threads where CAS contention matters.");

    std::vector<std::unique_ptr<IMpmcAdapter>> adapters;
    adapters.push_back(std::make_unique<WorkQueueMpmcAdapter>());
    adapters.push_back(std::make_unique<LockFreeQueueMpmcAdapter>());
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

    for (auto& adapter : adapters)
    {
        std::cout << std::setw(35) << std::left << adapter->name();

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

            // Measured
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

} // namespace

// ============================================================================
// MPMC Asymmetric Benchmark
// ============================================================================

namespace
{

struct AsymCase
{
    const char* mLabel = nullptr;
    size_t mProducers = 0;
    size_t mConsumers = 0;
};

void run_asymmetric_mpmc_benchmark(const BenchConfig& cfg)
{
    print_header("Asymmetric MPMC (MPSC, SPMC, Unbalanced)");
    print_cpu_context("START");
    print_contract_note(
        "Non-symmetric producer/consumer ratios. Tests WorkQueue under real-world "
        "dispatch patterns: many-producer/single-consumer (task submission), "
        "single-producer/many-consumer (fan-out), and unbalanced ratios.");

    std::vector<std::unique_ptr<IMpmcAdapter>> adapters;
    adapters.push_back(std::make_unique<WorkQueueMpmcAdapter>());
    adapters.push_back(std::make_unique<LockFreeQueueMpmcAdapter>());
    adapters.push_back(std::make_unique<MutexQueueMpmcAdapter>());
#if HAS_MOODYCAMEL
    adapters.push_back(std::make_unique<MoodycamelMpmcAdapter>());
#endif
#if HAS_BOOST_LOCKFREE
    adapters.push_back(std::make_unique<BoostLockfreeMpmcAdapter>());
#endif

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
// Burst/Drain Benchmark
// ============================================================================

namespace
{

struct IBurstAdapter
{
    virtual ~IBurstAdapter() = default;
    virtual const char* name() const = 0;
    virtual void setup() = 0;
    virtual void teardown() = 0;
    virtual size_t run_burst_drain(size_t burst_size) = 0;
};

class WorkQueueBurstAdapter : public IBurstAdapter
{
    using QueueType =
        fat_p::work_queue::WorkQueue<int, kWorkQueueShardCount, kWorkQueueShardCapacity>;

public:
    const char* name() const override { return "fat_p::WorkQueue (16 shards)"; }

    void setup() override { mQueue = std::make_unique<QueueType>(); }
    void teardown() override { mQueue.reset(); }

    size_t run_burst_drain(size_t burst_size) override
    {
        auto pTok = mQueue->makeProducerToken();
        auto cTok = mQueue->makeConsumerToken();

        // Fill
        for (size_t i = 0; i < burst_size; ++i)
        {
            (void)mQueue->enqueue(pTok, static_cast<int>(i));
        }

        // Drain
        int value = 0;
        size_t drained = 0;
        while (mQueue->dequeue(cTok, value))
        {
            DoNotOptimize(value);
            ++drained;
        }

        return burst_size + drained;
    }

private:
    std::unique_ptr<QueueType> mQueue;
};

class LockFreeQueueBurstAdapter : public IBurstAdapter
{
public:
    const char* name() const override { return "fat_p::LockFreeQueue"; }

    void setup() override
    {
        mQueue = std::make_unique<fat_p::LockFreeQueue<int, kQueueCapacity>>();
    }

    void teardown() override { mQueue.reset(); }

    size_t run_burst_drain(size_t burst_size) override
    {
        for (size_t i = 0; i < burst_size; ++i)
        {
            (void)mQueue->enqueue(static_cast<int>(i));
        }

        int value = 0;
        size_t drained = 0;
        while (mQueue->dequeue(value))
        {
            DoNotOptimize(value);
            ++drained;
        }

        return burst_size + drained;
    }

private:
    std::unique_ptr<fat_p::LockFreeQueue<int, kQueueCapacity>> mQueue;
};

class MutexQueueBurstAdapter : public IBurstAdapter
{
public:
    const char* name() const override { return "std::mutex + std::queue (baseline)"; }

    void setup() override { mQueue = std::queue<int>(); }
    void teardown() override { mQueue = std::queue<int>(); }

    size_t run_burst_drain(size_t burst_size) override
    {
        for (size_t i = 0; i < burst_size; ++i)
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mQueue.push(static_cast<int>(i));
        }

        size_t drained = 0;
        while (true)
        {
            int value = 0;
            std::lock_guard<std::mutex> lk(mMutex);
            if (mQueue.empty())
            {
                break;
            }
            value = mQueue.front();
            mQueue.pop();
            DoNotOptimize(value);
            ++drained;
        }

        return burst_size + drained;
    }

private:
    std::mutex mMutex;
    std::queue<int> mQueue;
};

void run_burst_drain_benchmark(const BenchConfig& cfg)
{
    print_header("Burst/Drain (single-threaded fill then drain)");
    print_cpu_context("START");
    print_contract_note(
        "Fill the queue to a target burst size, then drain completely. "
        "Single-threaded, no contention. Measures throughput under bursty access "
        "where the queue transitions between near-full and empty states. "
        "Allocation is excluded (reserve performed).");

    std::vector<std::unique_ptr<IBurstAdapter>> adapters;
    adapters.push_back(std::make_unique<WorkQueueBurstAdapter>());
    adapters.push_back(std::make_unique<LockFreeQueueBurstAdapter>());
    adapters.push_back(std::make_unique<MutexQueueBurstAdapter>());

    std::vector<size_t> burst_sizes = {1024, 8192, 65536};

    std::cout << std::setw(35) << std::left << "Library";
    for (size_t sz : burst_sizes)
    {
        std::cout << std::setw(14) << std::right << (std::to_string(sz) + " ops");
    }
    std::cout << "\n" << std::string(35 + 14 * burst_sizes.size(), '-') << "\n";

    std::mt19937_64 rng(cfg.seed);

    for (auto& adapter : adapters)
    {
        std::cout << std::setw(35) << std::left << adapter->name();

        for (size_t burst_size : burst_sizes)
        {
            // Warmup
            for (size_t run = 0; run < cfg.warmupRuns; ++run)
            {
                adapter->setup();
                adapter->run_burst_drain(burst_size);
                adapter->teardown();
            }

            // Measured
            std::vector<double> samples;
            for (size_t run = 0; run < cfg.measuredRuns; ++run)
            {
                adapter->setup();
                ClobberMemory();

                Timer t;
                t.start();
                size_t ops = adapter->run_burst_drain(burst_size);
                double elapsed = t.elapsed_ns();

                adapter->teardown();

                double ns_per_op = elapsed / static_cast<double>(ops);
                samples.push_back(ns_per_op);
            }

            auto stats = Statistics::compute(samples);
            std::cout << std::setw(14) << std::right << std::fixed << std::setprecision(1)
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

bool verify_correctness()
{
    std::cout << "Correctness:\n";

    // Verify WorkQueue single-threaded enqueue/dequeue round-trip
    {
        fat_p::work_queue::WorkQueue<int, 4, 256> q;
        auto pTok = q.makeProducerToken();
        auto cTok = q.makeConsumerToken();
        for (int i = 0; i < 100; ++i)
        {
            if (!q.enqueue(pTok, i))
            {
                std::cout << "  [FAIL] WorkQueue enqueue\n";
                return false;
            }
        }
        if (q.size() != 100)
        {
            std::cout << "  [FAIL] WorkQueue size after 100 enqueues (got "
                      << q.size() << ")\n";
            return false;
        }
        std::set<int> received;
        int value = 0;
        while (q.dequeue(cTok, value))
        {
            received.insert(value);
        }
        if (received.size() != 100)
        {
            std::cout << "  [FAIL] WorkQueue dequeue count (got "
                      << received.size() << ")\n";
            return false;
        }
        std::cout << "  [PASS] WorkQueue round-trip (100 elements)\n";
    }

    // Verify WorkQueue capacity enforcement
    {
        fat_p::work_queue::WorkQueue<int, 2, 16> q; // 32 total capacity
        auto tok = q.makeProducerToken();
        size_t accepted = 0;
        for (int i = 0; i < 64; ++i)
        {
            if (q.enqueue(tok, i))
            {
                ++accepted;
            }
        }
        if (accepted > 32)
        {
            std::cout << "  [FAIL] WorkQueue capacity enforcement (accepted "
                      << accepted << " into capacity 32)\n";
            return false;
        }
        std::cout << "  [PASS] WorkQueue capacity enforcement (accepted "
                  << accepted << " / 32)\n";
    }

    // Verify WorkQueue MPMC exactly-once delivery
    {
        constexpr size_t kProducers = 4;
        constexpr size_t kConsumers = 4;
        constexpr size_t kOpsPerProducer = 1000;

        fat_p::work_queue::WorkQueue<int, 8, 1024> q;
        std::atomic<size_t> consumed{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;

        for (size_t t = 0; t < kProducers; ++t)
        {
            threads.emplace_back([&q, t, &start]() {
                auto tok = q.makeProducerToken();
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (size_t i = 0; i < kOpsPerProducer; ++i)
                {
                    while (!q.enqueue(tok, static_cast<int>(t * 1000000 + i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (size_t t = 0; t < kConsumers; ++t)
        {
            threads.emplace_back([&q, &consumed, &start]() {
                auto tok = q.makeConsumerToken();
                while (!start.load(std::memory_order_acquire))
                {
                }
                int value = 0;
                size_t total = kProducers * kOpsPerProducer;
                while (consumed.load(std::memory_order_relaxed) < total)
                {
                    if (q.dequeue(tok, value))
                    {
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

        if (consumed.load() != kProducers * kOpsPerProducer)
        {
            std::cout << "  [FAIL] WorkQueue MPMC exactly-once (consumed "
                      << consumed.load() << ", expected "
                      << kProducers * kOpsPerProducer << ")\n";
            return false;
        }
        std::cout << "  [PASS] WorkQueue MPMC exactly-once ("
                  << kProducers << "P:" << kConsumers << "C, "
                  << kProducers * kOpsPerProducer << " elements)\n";
    }

    // Verify LockFreeQueue FIFO ordering (for comparison correctness)
    {
        fat_p::LockFreeQueue<int, 1024> queue;
        for (int i = 0; i < 100; ++i)
        {
            (void)queue.enqueue(i);
        }
        for (int i = 0; i < 100; ++i)
        {
            int value = 0;
            if (!queue.dequeue(value) || value != i)
            {
                std::cout << "  [FAIL] LockFreeQueue FIFO verification\n";
                return false;
            }
        }
        std::cout << "  [PASS] LockFreeQueue FIFO ordering\n";
    }

    std::cout << "\n";
    return true;
}

} // namespace

// ============================================================================
// Startup Header
// ============================================================================

namespace
{

std::string platform_string()
{
#if defined(_WIN32)
    std::string os = "Windows";
#elif defined(__linux__)
    std::string os = "Linux";
#elif defined(__APPLE__)
    std::string os = "macOS";
#else
    std::string os = "Unknown";
#endif

#if defined(__x86_64__) || defined(_M_X64)
    os += "-x64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    os += "-arm64";
#elif defined(__i386__) || defined(_M_IX86)
    os += "-x86";
#endif

#if defined(_MSC_VER)
    os += " MSVC-" + std::to_string(_MSC_VER);
#elif defined(__clang__)
    os += " Clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
    os += " GCC-" + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#endif

    return os;
}

void print_startup_header(const BenchConfig& cfg)
{
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  fat_p::WorkQueue Benchmark Suite\n";
    std::cout << std::string(80, '=') << "\n\n";

    std::cout << "Platform: " << platform_string()
              << " | warmup=" << cfg.warmupRuns
              << " measured=" << cfg.measuredRuns
              << " seed=" << cfg.seed << "\n\n";

    std::cout << "Competitors:\n";
    std::cout << "  [x] fat_p::WorkQueue (primary)\n";
    std::cout << "  [x] fat_p::LockFreeQueue (sibling)\n";
    std::cout << "  [x] std::mutex + std::queue (baseline)\n";
#if HAS_MOODYCAMEL
    std::cout << "  [x] moodycamel::ConcurrentQueue\n";
#else
    std::cout << "  [ ] moodycamel::ConcurrentQueue (not detected; github.com/cameron314/concurrentqueue)\n";
#endif
#if HAS_BOOST_LOCKFREE
    std::cout << "  [x] boost::lockfree::queue\n";
#else
    std::cout << "  [ ] boost::lockfree::queue (not detected; vcpkg install boost-lockfree)\n";
#endif

    std::cout << "\nConfiguration:\n";
    std::cout << "  Target work:    " << cfg.targetWork << " ops/batch\n";
    std::cout << "  Min batch ms:   " << cfg.minBatchMs << "\n";
    std::cout << "  Shards:         " << kWorkQueueShardCount << "\n";
    std::cout << "  Shard capacity: " << kWorkQueueShardCapacity << "\n";
    std::cout << "  Total capacity: " << kQueueCapacity << "\n";
    std::cout << "  Scope:          " << (cfg.noScope ? "OFF" : "ON") << "\n";
    std::cout << "  Stabilize:      " << (cfg.noStabilize ? "OFF" : "ON") << "\n";
    std::cout << "  Cooldown:       " << (cfg.noCooldown ? "OFF" : "ON") << "\n";
    if (!cfg.csvPath.empty())
    {
        std::cout << "  CSV output:     " << cfg.csvPath << "\n";
    }
    if (!cfg.jsonPath.empty())
    {
        std::cout << "  JSON output:    " << cfg.jsonPath << "\n";
    }

    std::cout << "\n";
    print_cpu_context("INIT");

    std::cout << "\nDesign Invariants:\n";
    std::cout << "  1. Round-robin execution with randomized order per run\n";
    std::cout << "  2. Setup/teardown outside timed regions\n";
    std::cout << "  3. All libraries observe same distribution of machine states\n";
    std::cout << "  4. Medians are the primary reported statistic\n";
    std::cout << "  5. Correctness verified after each benchmark\n";
    std::cout << "  6. Start barrier for all concurrent benchmarks\n";
    std::cout << "\n";
}

} // namespace

// ============================================================================
// Main
// ============================================================================

int main()
{
    auto cfg = loadConfig();

    print_startup_header(cfg);

    // Correctness guardrails (required by style guide)
    if (!verify_correctness())
    {
        std::cerr << "Correctness verification failed. Aborting benchmarks.\n";
        return 1;
    }

    std::cout << "Expected Results:\n";
    std::cout << "  - fat_p::WorkQueue excels at: high-contention MPMC (>= 4 threads)\n";
    std::cout << "  - fat_p::LockFreeQueue: lower overhead at low contention (1-2 threads)\n";
    std::cout << "  - std::mutex + std::queue: correct but slow under contention\n";
    std::cout << "  - WorkQueue pays shard-routing overhead in single-threaded/SPSC\n";
    std::cout << "\n";

    {
        BenchmarkScope scope(/*verbose=*/true);

        // 1. Single-threaded throughput (round-robin)
        {
            std::vector<std::unique_ptr<IAdapter>> adapters;
            adapters.push_back(std::make_unique<WorkQueueAdapter>());
            adapters.push_back(std::make_unique<LockFreeQueueAdapter>());
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

            run_single_threaded_benchmark(adapters, single_thread_work,
                                          cfg.warmupRuns, cfg.measuredRuns, cfg.seed);
        }

        cooldown_sleep(cfg, 200);

        // 2. SPSC throughput
        run_spsc_benchmark(cfg);

        cooldown_sleep(cfg, 200);

        // 3. MPMC symmetric scaling
        run_mpmc_scaling_benchmark(cfg);

        cooldown_sleep(cfg, 500);

        // 4. MPMC asymmetric
        run_asymmetric_mpmc_benchmark(cfg);

        cooldown_sleep(cfg, 200);

        // 5. Burst/drain pattern
        run_burst_drain_benchmark(cfg);
    }

    // Summary
    print_header("Summary");
    std::cout << "Benchmarks completed.\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";
    print_cpu_context("FINAL");

    return 0;
}
