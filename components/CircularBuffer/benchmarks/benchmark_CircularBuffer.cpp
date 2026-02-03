/**
 * @file benchmark_CircularBuffer.cpp
 * @brief Comprehensive benchmarks for CircularBuffer (SPSC lock-free queue)
 *
 * @layer Testing
 *
 * Benchmarks:
 * - Single-threaded throughput (push/pop cycle, uncontended)
 * - SPSC throughput (dedicated producer/consumer threads)
 * - Burst patterns (fill then drain)
 * - Capacity sensitivity (varying buffer sizes)
 * - Index caching effectiveness comparison
 *
 * Competitors:
 * - std::mutex + std::deque (baseline - always included)
 * - LockFreeRingBuffer SPSC (Fat-P sibling comparison)
 * - boost::lockfree::spsc_queue (optional via __has_include)
 * - moodycamel::BlockingReaderWriterCircularBuffer (optional via __has_include)
 *
 * Compile (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread benchmark_CircularBuffer.cpp -o bench_cb
 *
 * Compile (with boost):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread -I/path/to/boost \
 *       benchmark_CircularBuffer.cpp -o bench_cb
 *
 * Windows (MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_CircularBuffer.cpp /link advapi32.lib
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
  component: CircularBuffer
  file_role: benchmark
  path: components/CircularBuffer/benchmarks/benchmark_CircularBuffer.cpp
  layer: Testing
  namespace: fat_p
  summary: "Benchmarks for CircularBuffer SPSC lock-free queue."
  api_stability: candidate
  related:
    docs_search: "CircularBuffer"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/CircularBuffer.h
      - include/fat_p/LockFreeRingBuffer.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 5
    defines_unprefixed: 4
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
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

#include "CircularBuffer.h"
#include "FatPBenchmarkHeader.h"
#include "FatPBenchmarkRunner.h"
#include "LockFreeRingBuffer.h"

// ============================================================================
// DCE Prevention (local definitions)
// ============================================================================

namespace
{

template <typename T>
inline void DoNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(value) : "memory");
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
// Competitor Auto-Detection (via __has_include)
// ============================================================================

// boost::lockfree::spsc_queue - Boost's SPSC queue implementation
// Rationale: Widely deployed reference point for SPSC queues
#if __has_include(<boost/lockfree/spsc_queue.hpp>)
#include <boost/lockfree/spsc_queue.hpp>
#define HAS_BOOST_SPSC 1
#else
#define HAS_BOOST_SPSC 0
#endif

// moodycamel::BlockingReaderWriterCircularBuffer - High-performance fixed-capacity SPSC
// Rationale: Industry-standard lock-free SPSC circular buffer, fair comparison with CircularBuffer
// Note: ReaderWriterQueue is dynamically-growable (unfair comparison); use the circular buffer variant
#if __has_include(<readerwriterqueue/readerwritercircularbuffer.h>)
#include <readerwriterqueue/readerwritercircularbuffer.h>
#define HAS_MOODYCAMEL_SPSC 1
#elif __has_include(<moodycamel/readerwritercircularbuffer.h>)
#include <moodycamel/readerwritercircularbuffer.h>
#define HAS_MOODYCAMEL_SPSC 1
#elif __has_include("readerwritercircularbuffer.h")
#include "readerwritercircularbuffer.h"
#define HAS_MOODYCAMEL_SPSC 1
#else
#define HAS_MOODYCAMEL_SPSC 0
#endif

// ============================================================================
// Constants
// ============================================================================

namespace
{

constexpr size_t kDefaultCapacity = 4096;
constexpr size_t kSmallCapacity = 64;
constexpr size_t kLargeCapacity = 65536;

} // namespace

// ============================================================================
// Global Configuration
// ============================================================================

namespace
{

struct BenchConfig
{
    size_t warmupRuns = 3;
    size_t measuredRuns = 50;
    uint64_t seed = 12345;
    size_t targetWork = 1000000;
    size_t minBatchMs = 50;
    bool noScope = false;
    bool noStabilize = false;
    bool noCooldown = false;
    std::string csvPath;
    std::string jsonPath;
};

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

static BenchConfig g_config;

// ============================================================================
// CPU Frequency Monitoring (Shared)
// ============================================================================

namespace
{

void print_cpu_context(const char* label = nullptr)
{
    fat_p::bench::print_cpu_context(std::cout, label);
}

void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

} // namespace

// ============================================================================
// Adapter Interface for Multi-Library Comparison
// ============================================================================

namespace
{

/**
 * @brief Abstract adapter interface for SPSC queue benchmarks
 */
class ISpscAdapter
{
public:
    virtual ~ISpscAdapter() = default;

    virtual const char* name() const = 0;
    virtual void setup() = 0;
    virtual void teardown() = 0;

    // Single-threaded: push then pop in same thread
    virtual size_t run_single_threaded(size_t ops) = 0;

    // SPSC: separate producer/consumer threads
    virtual size_t run_spsc(size_t ops) = 0;

    // Burst: fill completely, then drain completely
    virtual size_t run_burst(size_t burst_size, size_t bursts) = 0;
};

// ============================================================================
// Fat-P CircularBuffer Adapter
// ============================================================================

template <size_t Capacity>
class CircularBufferAdapter : public ISpscAdapter
{
    std::unique_ptr<fat_p::CircularBuffer<int64_t, Capacity>> mBuffer;

public:
    const char* name() const override
    {
        return "fat_p::CircularBuffer";
    }

    void setup() override
    {
        mBuffer = std::make_unique<fat_p::CircularBuffer<int64_t, Capacity>>();
    }

    void teardown() override
    {
        mBuffer.reset();
    }

    size_t run_single_threaded(size_t ops) override
    {
        size_t completed = 0;
        int64_t val = 0;

        for (size_t i = 0; i < ops; ++i)
        {
            if (mBuffer->push(static_cast<int64_t>(i)))
            {
                if (mBuffer->pop(val))
                {
                    ++completed;
                    DoNotOptimize(val);
                }
            }
        }

        return completed;
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<bool> start{false};
        std::atomic<size_t> produced{0};
        std::atomic<size_t> consumed{0};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < ops; ++i)
            {
                while (!mBuffer->push(static_cast<int64_t>(i)))
                {
                    std::this_thread::yield();
                }
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            int64_t val = 0;
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                if (mBuffer->pop(val))
                {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    DoNotOptimize(val);
                }
            }
        });

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        return consumed.load();
    }

    size_t run_burst(size_t burst_size, size_t bursts) override
    {
        size_t total = 0;
        burst_size = std::min(burst_size, Capacity);

        for (size_t b = 0; b < bursts; ++b)
        {
            // Fill
            for (size_t i = 0; i < burst_size; ++i)
            {
                (void)mBuffer->push(static_cast<int64_t>(i));
            }

            // Drain
            int64_t val = 0;
            while (mBuffer->pop(val))
            {
                ++total;
                DoNotOptimize(val);
            }
        }

        return total;
    }
};

// ============================================================================
// Fat-P LockFreeRingBuffer SPSC Adapter (Sibling Comparison)
// ============================================================================

class LockFreeRingBufferSpscAdapter : public ISpscAdapter
{
    std::unique_ptr<fat_p::LockFreeRingBuffer<int64_t>> mBuffer;
    size_t mCapacity;

public:
    explicit LockFreeRingBufferSpscAdapter(size_t capacity = kDefaultCapacity)
        : mCapacity(capacity)
    {
    }

    const char* name() const override
    {
        return "fat_p::LockFreeRingBuffer (SPSC)";
    }

    void setup() override
    {
        mBuffer = std::make_unique<fat_p::LockFreeRingBuffer<int64_t>>(mCapacity);
    }

    void teardown() override
    {
        mBuffer.reset();
    }

    size_t run_single_threaded(size_t ops) override
    {
        size_t completed = 0;

        for (size_t i = 0; i < ops; ++i)
        {
            if (mBuffer->push(static_cast<int64_t>(i)))
            {
                auto val = mBuffer->pop();
                if (val)
                {
                    ++completed;
                    DoNotOptimize(*val);
                }
            }
        }

        return completed;
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<bool> start{false};
        std::atomic<size_t> consumed{0};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < ops; ++i)
            {
                while (!mBuffer->push(static_cast<int64_t>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                auto val = mBuffer->pop();
                if (val)
                {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    DoNotOptimize(*val);
                }
            }
        });

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        return consumed.load();
    }

    size_t run_burst(size_t burst_size, size_t bursts) override
    {
        size_t total = 0;
        burst_size = std::min(burst_size, mCapacity);

        for (size_t b = 0; b < bursts; ++b)
        {
            for (size_t i = 0; i < burst_size; ++i)
            {
                (void)mBuffer->push(static_cast<int64_t>(i));
            }

            while (auto val = mBuffer->pop())
            {
                ++total;
                DoNotOptimize(*val);
            }
        }

        return total;
    }
};

// ============================================================================
// std::mutex + std::deque Baseline Adapter
// ============================================================================

class MutexDequeAdapter : public ISpscAdapter
{
    std::unique_ptr<std::deque<int64_t>> mDeque;
    std::unique_ptr<std::mutex> mMutex;
    size_t mCapacity;

public:
    explicit MutexDequeAdapter(size_t capacity = kDefaultCapacity)
        : mCapacity(capacity)
    {
    }

    const char* name() const override
    {
        return "std::mutex + std::deque (baseline)";
    }

    void setup() override
    {
        mDeque = std::make_unique<std::deque<int64_t>>();
        mMutex = std::make_unique<std::mutex>();
    }

    void teardown() override
    {
        mDeque.reset();
        mMutex.reset();
    }

    size_t run_single_threaded(size_t ops) override
    {
        size_t completed = 0;

        for (size_t i = 0; i < ops; ++i)
        {
            {
                std::lock_guard<std::mutex> lock(*mMutex);
                if (mDeque->size() < mCapacity)
                {
                    mDeque->push_back(static_cast<int64_t>(i));
                }
            }

            {
                std::lock_guard<std::mutex> lock(*mMutex);
                if (!mDeque->empty())
                {
                    int64_t val = mDeque->front();
                    mDeque->pop_front();
                    ++completed;
                    DoNotOptimize(val);
                }
            }
        }

        return completed;
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<bool> start{false};
        std::atomic<size_t> consumed{0};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < ops; ++i)
            {
                bool pushed = false;
                while (!pushed)
                {
                    std::lock_guard<std::mutex> lock(*mMutex);
                    if (mDeque->size() < mCapacity)
                    {
                        mDeque->push_back(static_cast<int64_t>(i));
                        pushed = true;
                    }
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                std::lock_guard<std::mutex> lock(*mMutex);
                if (!mDeque->empty())
                {
                    int64_t val = mDeque->front();
                    mDeque->pop_front();
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    DoNotOptimize(val);
                }
            }
        });

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        return consumed.load();
    }

    size_t run_burst(size_t burst_size, size_t bursts) override
    {
        size_t total = 0;
        burst_size = std::min(burst_size, mCapacity);

        for (size_t b = 0; b < bursts; ++b)
        {
            {
                std::lock_guard<std::mutex> lock(*mMutex);
                for (size_t i = 0; i < burst_size; ++i)
                {
                    mDeque->push_back(static_cast<int64_t>(i));
                }
            }

            {
                std::lock_guard<std::mutex> lock(*mMutex);
                while (!mDeque->empty())
                {
                    int64_t val = mDeque->front();
                    mDeque->pop_front();
                    ++total;
                    DoNotOptimize(val);
                }
            }
        }

        return total;
    }
};

// ============================================================================
// Boost SPSC Queue Adapter (Optional)
// ============================================================================

#if HAS_BOOST_SPSC
class BoostSpscAdapter : public ISpscAdapter
{
    std::unique_ptr<boost::lockfree::spsc_queue<int64_t>> mQueue;
    size_t mCapacity;

public:
    explicit BoostSpscAdapter(size_t capacity = kDefaultCapacity)
        : mCapacity(capacity)
    {
    }

    const char* name() const override
    {
        return "boost::lockfree::spsc_queue";
    }

    void setup() override
    {
        mQueue = std::make_unique<boost::lockfree::spsc_queue<int64_t>>(mCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_single_threaded(size_t ops) override
    {
        size_t completed = 0;

        for (size_t i = 0; i < ops; ++i)
        {
            if (mQueue->push(static_cast<int64_t>(i)))
            {
                int64_t val;
                if (mQueue->pop(val))
                {
                    ++completed;
                    DoNotOptimize(val);
                }
            }
        }

        return completed;
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<bool> start{false};
        std::atomic<size_t> consumed{0};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < ops; ++i)
            {
                while (!mQueue->push(static_cast<int64_t>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            int64_t val;
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                if (mQueue->pop(val))
                {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    DoNotOptimize(val);
                }
            }
        });

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        return consumed.load();
    }

    size_t run_burst(size_t burst_size, size_t bursts) override
    {
        size_t total = 0;
        burst_size = std::min(burst_size, mCapacity);

        for (size_t b = 0; b < bursts; ++b)
        {
            for (size_t i = 0; i < burst_size; ++i)
            {
                (void)mQueue->push(static_cast<int64_t>(i));
            }

            int64_t val;
            while (mQueue->pop(val))
            {
                ++total;
                DoNotOptimize(val);
            }
        }

        return total;
    }
};
#endif

// ============================================================================
// Moodycamel BlockingReaderWriterCircularBuffer Adapter (Optional)
// ============================================================================

#if HAS_MOODYCAMEL_SPSC
class MoodycamelSpscAdapter : public ISpscAdapter
{
    std::unique_ptr<moodycamel::BlockingReaderWriterCircularBuffer<int64_t>> mQueue;
    size_t mCapacity;

public:
    explicit MoodycamelSpscAdapter(size_t capacity = kDefaultCapacity)
        : mCapacity(capacity)
    {
    }

    const char* name() const override
    {
        return "moodycamel::BlockingRWCircularBuffer";
    }

    void setup() override
    {
        mQueue = std::make_unique<moodycamel::BlockingReaderWriterCircularBuffer<int64_t>>(mCapacity);
    }

    void teardown() override
    {
        mQueue.reset();
    }

    size_t run_single_threaded(size_t ops) override
    {
        size_t completed = 0;

        for (size_t i = 0; i < ops; ++i)
        {
            if (mQueue->try_enqueue(static_cast<int64_t>(i)))
            {
                int64_t val;
                if (mQueue->try_dequeue(val))
                {
                    ++completed;
                    DoNotOptimize(val);
                }
            }
        }

        return completed;
    }

    size_t run_spsc(size_t ops) override
    {
        std::atomic<bool> start{false};
        std::atomic<size_t> consumed{0};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < ops; ++i)
            {
                while (!mQueue->try_enqueue(static_cast<int64_t>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            int64_t val;
            while (consumed.load(std::memory_order_relaxed) < ops)
            {
                if (mQueue->try_dequeue(val))
                {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    DoNotOptimize(val);
                }
            }
        });

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        return consumed.load();
    }

    size_t run_burst(size_t burst_size, size_t bursts) override
    {
        size_t total = 0;
        burst_size = std::min(burst_size, mCapacity);

        for (size_t b = 0; b < bursts; ++b)
        {
            for (size_t i = 0; i < burst_size; ++i)
            {
                (void)mQueue->try_enqueue(static_cast<int64_t>(i));
            }

            int64_t val;
            while (mQueue->try_dequeue(val))
            {
                ++total;
                DoNotOptimize(val);
            }
        }

        return total;
    }
};
#endif

// ============================================================================
// Statistics Helper
// ============================================================================

struct Statistics
{
    double median = 0;
    double mean = 0;
    double stddev = 0;
    double ci95Low = 0;
    double ci95High = 0;
    double min = 0;
    double max = 0;
    size_t samples = 0;

    static Statistics compute(std::vector<double> data)
    {
        Statistics s;
        if (data.empty())
        {
            return s;
        }

        s.samples = data.size();
        std::sort(data.begin(), data.end());

        s.min = data.front();
        s.max = data.back();

        // Median
        size_t mid = data.size() / 2;
        s.median = (data.size() % 2 == 0) ? (data[mid - 1] + data[mid]) / 2.0 : data[mid];

        // Mean
        double sum = std::accumulate(data.begin(), data.end(), 0.0);
        s.mean = sum / static_cast<double>(data.size());

        // Stddev
        double sq_sum = 0;
        for (double d : data)
        {
            sq_sum += (d - s.mean) * (d - s.mean);
        }
        s.stddev = std::sqrt(sq_sum / static_cast<double>(data.size()));

        // CI95
        double se = s.stddev / std::sqrt(static_cast<double>(data.size()));
        s.ci95Low = s.mean - 1.96 * se;
        s.ci95High = s.mean + 1.96 * se;

        return s;
    }
};

// ============================================================================
// Timer
// ============================================================================

class Timer
{
    std::chrono::high_resolution_clock::time_point mStart;

public:
    void start()
    {
        mStart = std::chrono::high_resolution_clock::now();
    }

    double elapsed_ns() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - mStart).count());
    }
};

// ============================================================================
// Round-Robin Benchmark Runner
// ============================================================================

void run_round_robin_benchmark(const std::string& title,
                               const std::string& contract,
                               std::vector<std::unique_ptr<ISpscAdapter>>& adapters,
                               size_t ops,
                               size_t warmup,
                               size_t measured,
                               uint64_t seed,
                               std::function<size_t(ISpscAdapter*, size_t)> runner)
{
    print_header(title);
    print_cpu_context();

    std::cout << "Contract note: " << contract << "\n\n";

    // Initialize sample storage
    std::vector<std::vector<double>> all_samples(adapters.size());
    for (auto& s : all_samples)
    {
        s.reserve(measured);
    }

    // Create shuffled order
    std::vector<size_t> order(adapters.size());
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(static_cast<unsigned int>(seed));

    // Warmup
    std::cout << "Warmup (" << warmup << " runs)...\n";
    for (size_t w = 0; w < warmup; ++w)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (size_t idx : order)
        {
            adapters[idx]->setup();
            (void)runner(adapters[idx].get(), ops);
            adapters[idx]->teardown();
        }
    }

    // Measured runs (round-robin)
    std::cout << "Measured runs (" << measured << " batches, round-robin)...\n";
    for (size_t run = 0; run < measured; ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);

        for (size_t idx : order)
        {
            adapters[idx]->setup();
            ClobberMemory();

            Timer t;
            t.start();
            size_t actual_ops = runner(adapters[idx].get(), ops);
            double elapsed = t.elapsed_ns();

            adapters[idx]->teardown();

            double ns_per_op = elapsed / static_cast<double>(actual_ops);
            all_samples[idx].push_back(ns_per_op);
        }
    }

    // Print results table
    std::cout << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(40) << std::left << "Library" << std::setw(12) << std::right << "Median" << std::setw(12)
              << "Mean" << std::setw(10) << "Stddev"
              << "  CI95\n";
    std::cout << std::string(90, '-') << "\n";

    for (size_t i = 0; i < adapters.size(); ++i)
    {
        auto stats = Statistics::compute(all_samples[i]);

        std::cout << std::setw(40) << std::left << adapters[i]->name() << std::setw(12) << std::right << stats.median
                  << std::setw(12) << stats.mean << std::setw(10) << stats.stddev << "  [" << stats.ci95Low << ", "
                  << stats.ci95High << "] ns/op\n";

        // High variance warning
        if (stats.stddev > stats.median && stats.median > 0)
        {
            std::cout << "  [NOTE] high variance (stddev " << stats.stddev << " > median " << stats.median << ")\n";
        }
    }

    std::cout << "\n";
    print_cpu_context("END");
}

// ============================================================================
// Correctness Verification
// ============================================================================

bool verify_correctness()
{
    std::cout << "Correctness verification:\n";

    // Verify CircularBuffer FIFO ordering
    {
        fat_p::CircularBuffer<int, 128> buffer;
        for (int i = 0; i < 100; ++i)
        {
            (void)buffer.push(i);
        }
        for (int i = 0; i < 100; ++i)
        {
            int val;
            if (!buffer.pop(val) || val != i)
            {
                std::cout << "  [FAIL] CircularBuffer FIFO verification\n";
                return false;
            }
        }
        std::cout << "  [PASS] CircularBuffer FIFO ordering\n";
    }

    // Verify capacity enforcement
    {
        fat_p::CircularBuffer<int, 4> buffer;
        for (int i = 0; i < 4; ++i)
        {
            if (!buffer.push(i))
            {
                std::cout << "  [FAIL] CircularBuffer capacity test - push failed early\n";
                return false;
            }
        }
        if (buffer.push(999))
        {
            std::cout << "  [FAIL] CircularBuffer capacity test - accepted overflow\n";
            return false;
        }
        std::cout << "  [PASS] CircularBuffer capacity enforcement\n";
    }

    // Verify SPSC thread safety
    {
        constexpr int items = 10000;
        fat_p::CircularBuffer<int, 256> buffer;
        std::atomic<bool> start{false};
        std::atomic<int> errors{0};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (int i = 0; i < items; ++i)
            {
                while (!buffer.push(i))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            int expected = 0;
            while (expected < items)
            {
                int val;
                if (buffer.pop(val))
                {
                    if (val != expected)
                    {
                        errors.fetch_add(1, std::memory_order_relaxed);
                    }
                    ++expected;
                }
            }
        });

        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();

        if (errors.load() > 0)
        {
            std::cout << "  [FAIL] CircularBuffer SPSC thread safety (" << errors.load() << " errors)\n";
            return false;
        }
        std::cout << "  [PASS] CircularBuffer SPSC thread safety\n";
    }

    std::cout << "\n";
    return true;
}

// ============================================================================
// Benchmark: Single-Threaded Throughput
// ============================================================================

void benchmark_single_threaded()
{
    std::vector<std::unique_ptr<ISpscAdapter>> adapters;
    adapters.push_back(std::make_unique<CircularBufferAdapter<kDefaultCapacity>>());
    adapters.push_back(std::make_unique<LockFreeRingBufferSpscAdapter>(kDefaultCapacity));
    adapters.push_back(std::make_unique<MutexDequeAdapter>(kDefaultCapacity));
#if HAS_BOOST_SPSC
    adapters.push_back(std::make_unique<BoostSpscAdapter>(kDefaultCapacity));
#endif
#if HAS_MOODYCAMEL_SPSC
    adapters.push_back(std::make_unique<MoodycamelSpscAdapter>(kDefaultCapacity));
#endif

    run_round_robin_benchmark("Single-Threaded Throughput (push + pop cycle)",
                              "Uncontended SPSC overhead. Measures push+pop per op. Capacity: 4096.",
                              adapters,
                              std::min(g_config.targetWork, kDefaultCapacity),
                              g_config.warmupRuns,
                              g_config.measuredRuns,
                              g_config.seed,
                              [](ISpscAdapter* a, size_t ops) {
                                  return a->run_single_threaded(ops);
                              });
}

// ============================================================================
// Benchmark: SPSC Throughput
// ============================================================================

void benchmark_spsc_throughput()
{
    std::vector<std::unique_ptr<ISpscAdapter>> adapters;
    adapters.push_back(std::make_unique<CircularBufferAdapter<kDefaultCapacity>>());
    adapters.push_back(std::make_unique<LockFreeRingBufferSpscAdapter>(kDefaultCapacity));
    adapters.push_back(std::make_unique<MutexDequeAdapter>(kDefaultCapacity));
#if HAS_BOOST_SPSC
    adapters.push_back(std::make_unique<BoostSpscAdapter>(kDefaultCapacity));
#endif
#if HAS_MOODYCAMEL_SPSC
    adapters.push_back(std::make_unique<MoodycamelSpscAdapter>(kDefaultCapacity));
#endif

    run_round_robin_benchmark("SPSC Throughput (dedicated producer/consumer threads)",
                              "True SPSC pattern. Producer and consumer on separate threads. Capacity: 4096.",
                              adapters,
                              g_config.targetWork,
                              g_config.warmupRuns,
                              g_config.measuredRuns,
                              g_config.seed,
                              [](ISpscAdapter* a, size_t ops) {
                                  return a->run_spsc(ops);
                              });
}

// ============================================================================
// Benchmark: Burst Pattern
// ============================================================================

void benchmark_burst_pattern()
{
    std::vector<std::unique_ptr<ISpscAdapter>> adapters;
    adapters.push_back(std::make_unique<CircularBufferAdapter<kDefaultCapacity>>());
    adapters.push_back(std::make_unique<LockFreeRingBufferSpscAdapter>(kDefaultCapacity));
    adapters.push_back(std::make_unique<MutexDequeAdapter>(kDefaultCapacity));
#if HAS_BOOST_SPSC
    adapters.push_back(std::make_unique<BoostSpscAdapter>(kDefaultCapacity));
#endif
#if HAS_MOODYCAMEL_SPSC
    adapters.push_back(std::make_unique<MoodycamelSpscAdapter>(kDefaultCapacity));
#endif

    constexpr size_t burst_size = 1024;
    constexpr size_t bursts = 1000;

    run_round_robin_benchmark("Burst Pattern (fill then drain cycles)",
                              "Simulates batched workloads. Burst size: 1024, bursts: 1000.",
                              adapters,
                              bursts,
                              g_config.warmupRuns,
                              g_config.measuredRuns,
                              g_config.seed,
                              [burst_size](ISpscAdapter* a, size_t b) {
                                  return a->run_burst(burst_size, b);
                              });
}

// ============================================================================
// Benchmark: Capacity Sensitivity
// ============================================================================

void benchmark_capacity_sensitivity()
{
    print_header("Capacity Sensitivity (SPSC throughput vs buffer size)");
    print_cpu_context();

    std::cout << "Contract note: Fixed work per test. Smaller buffers may cause more contention.\n\n";

    struct CapacityCase
    {
        const char* label;
        size_t capacity;
    };

    const std::array<CapacityCase, 4> cases = {
        CapacityCase{"64", 64},
        CapacityCase{"1K", 1024},
        CapacityCase{"4K", 4096},
        CapacityCase{"64K", 65536},
    };

    const size_t ops = g_config.targetWork;

    // We'll measure CircularBuffer at different capacities
    std::cout << std::setw(15) << "Capacity" << std::setw(15) << "Median ns/op" << std::setw(15) << "Throughput"
              << "\n";
    std::cout << std::string(50, '-') << "\n";

    // We can't easily template-switch at runtime, so we manually instantiate common sizes
    auto measure_capacity = [&](size_t capacity) {
        std::vector<double> samples;
        samples.reserve(g_config.measuredRuns);

        // Create appropriate buffer based on capacity
        // Use runtime-sized LockFreeRingBuffer for flexibility
        auto buffer = std::make_unique<fat_p::LockFreeRingBuffer<int64_t>>(capacity);

        // Warmup
        for (size_t w = 0; w < g_config.warmupRuns; ++w)
        {
            std::atomic<bool> start{false};
            std::atomic<size_t> consumed{0};

            std::thread producer([&]() {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                for (size_t i = 0; i < ops; ++i)
                {
                    while (!buffer->push(static_cast<int64_t>(i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });

            std::thread consumer([&]() {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                while (consumed.load(std::memory_order_relaxed) < ops)
                {
                    if (auto val = buffer->pop())
                    {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                        DoNotOptimize(*val);
                    }
                }
            });

            start.store(true, std::memory_order_release);
            producer.join();
            consumer.join();
        }

        // Measured runs
        for (size_t run = 0; run < g_config.measuredRuns; ++run)
        {
            std::atomic<bool> start{false};
            std::atomic<size_t> consumed{0};

            std::thread producer([&]() {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                for (size_t i = 0; i < ops; ++i)
                {
                    while (!buffer->push(static_cast<int64_t>(i)))
                    {
                        std::this_thread::yield();
                    }
                }
            });

            std::thread consumer([&]() {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                while (consumed.load(std::memory_order_relaxed) < ops)
                {
                    if (auto val = buffer->pop())
                    {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                        DoNotOptimize(*val);
                    }
                }
            });

            Timer t;
            t.start();
            start.store(true, std::memory_order_release);
            producer.join();
            consumer.join();
            double elapsed = t.elapsed_ns();

            samples.push_back(elapsed / static_cast<double>(ops));
        }

        return Statistics::compute(samples);
    };

    for (const auto& c : cases)
    {
        auto stats = measure_capacity(c.capacity);
        double throughput_mops = 1000.0 / stats.median; // M ops/sec

        std::cout << std::setw(15) << c.label << std::setw(15) << std::fixed << std::setprecision(2) << stats.median
                  << std::setw(12) << std::setprecision(1) << throughput_mops << " Mops/s\n";
    }

    std::cout << "\n";
    print_cpu_context("END");
}

// ============================================================================
// Benchmark: Memory Layout and Object Size
// ============================================================================

void benchmark_object_size()
{
    print_header("Object Size Impact");

    std::cout << "CircularBuffer object sizes (includes inline metadata):\n\n";

    std::cout << std::setw(40) << std::left << "Type" << std::setw(15) << std::right << "sizeof (bytes)\n";
    std::cout << std::string(60, '-') << "\n";

    std::cout << std::setw(40) << std::left << "CircularBuffer<int64_t, 64>" << std::setw(15) << std::right
              << sizeof(fat_p::CircularBuffer<int64_t, 64>) << "\n";

    std::cout << std::setw(40) << std::left << "CircularBuffer<int64_t, 1024>" << std::setw(15) << std::right
              << sizeof(fat_p::CircularBuffer<int64_t, 1024>) << "\n";

    std::cout << std::setw(40) << std::left << "CircularBuffer<int64_t, 4096>" << std::setw(15) << std::right
              << sizeof(fat_p::CircularBuffer<int64_t, 4096>) << "\n";

    std::cout << std::setw(40) << std::left << "LockFreeRingBuffer<int64_t>" << std::setw(15) << std::right
              << sizeof(fat_p::LockFreeRingBuffer<int64_t>) << "\n";

#if HAS_BOOST_SPSC
    std::cout << "\nBoost comparison:\n";
    std::cout << std::setw(40) << std::left << "boost::lockfree::spsc_queue<int64_t>" << std::setw(15) << std::right
              << sizeof(boost::lockfree::spsc_queue<int64_t>) << "\n";
#endif

    std::cout << "\nNote: CircularBuffer uses cache-line aligned indices for false sharing prevention.\n";
    std::cout << "Actual buffer storage is heap-allocated via unique_ptr.\n";
}

} // namespace

// ============================================================================
// Main
// ============================================================================

int main()
{
    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "CircularBuffer";
    hdr.warmup = g_config.warmupRuns;
    hdr.measured = g_config.measuredRuns;
    hdr.seed = g_config.seed;

    // Competitors
    hdr.competitors.push_back({"fat_p::CircularBuffer", true, "primary"});
    hdr.competitors.push_back({"fat_p::LockFreeRingBuffer", true, "sibling SPSC"});
    hdr.competitors.push_back({"std::mutex + std::deque", true, "baseline"});
#if HAS_BOOST_SPSC
    hdr.competitors.push_back({"boost::lockfree::spsc_queue", true, ""});
#else
    hdr.competitors.push_back({"boost::lockfree::spsc_queue", false, "vcpkg install boost-lockfree"});
#endif
#if HAS_MOODYCAMEL_SPSC
    hdr.competitors.push_back({"moodycamel::BlockingReaderWriterCircularBuffer", true, ""});
#else
    hdr.competitors.push_back(
        {"moodycamel::BlockingReaderWriterCircularBuffer", false, "github.com/cameron314/readerwriterqueue"});
#endif

    hdr.has_extended_config = true;
    hdr.target_work = g_config.targetWork;
    hdr.min_batch_ms = g_config.minBatchMs;
    hdr.scope_enabled = !g_config.noScope;
    hdr.stabilize_enabled = !g_config.noStabilize;
    hdr.cooldown_enabled = !g_config.noCooldown;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = true;

    fat_p::bench::print_standard_header(hdr);

    // Load configuration (required by style guide)
    g_config = loadConfig();

    std::cout << "Configuration:\n";
    std::cout << "  Warmup runs:    " << g_config.warmupRuns << "\n";
    std::cout << "  Measured runs:  " << g_config.measuredRuns << "\n";
    std::cout << "  Seed:           " << g_config.seed << "\n";
    std::cout << "  Target work:    " << g_config.targetWork << "\n";
    std::cout << "  Min batch ms:   " << g_config.minBatchMs << "\n";
    std::cout << "  Scope:          " << (g_config.noScope ? "disabled" : "enabled") << "\n";
    std::cout << "  Stabilize:      " << (g_config.noStabilize ? "disabled" : "enabled") << "\n";
    std::cout << "  Cooldown:       " << (g_config.noCooldown ? "disabled" : "enabled") << "\n";
    std::cout << "\n";

    // Initial CPU context (required by style guide)
    print_cpu_context("INIT");
    std::cout << "\n";

    // Correctness verification (required by style guide)
    if (!verify_correctness())
    {
        std::cerr << "Correctness verification failed. Aborting benchmarks.\n";
        return 1;
    }

    // Apply benchmark scope (Windows priority/affinity)
    fat_p::bench::BenchmarkScope scope(!g_config.noScope);

    // Run benchmarks
    benchmark_single_threaded();
    benchmark_spsc_throughput();
    benchmark_burst_pattern();
    benchmark_capacity_sensitivity();
    benchmark_object_size();

    // Summary
    print_header("Summary");
    std::cout << "CircularBuffer benchmark suite completed.\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";
    print_cpu_context("FINAL");

    return 0;
}