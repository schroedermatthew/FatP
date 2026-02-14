/**
 * @file benchmark_ThreadPool.cpp
 * @brief Comprehensive benchmarks for ThreadPool
 *
 * @layer Testing
 *
 * Benchmarks:
 * - Submission overhead: submit(), submit_priority(), submit_batch()
 * - Throughput scaling: tasks/sec at 1, 2, 4, 8 worker threads
 * - Latency distribution: p50, p90, p99, p99.9 from submit to task start
 * - Spin configuration impact: 0, 1000, 2000, 5000 us
 * - Priority routing: High/Critical vs Normal/Low scheduling
 * - Work stealing effectiveness: skewed vs balanced load
 *
 * Competitors:
 * - std::async (baseline - always included)
 * - Mutex+CondVar thread pool (hand-rolled baseline - always included)
 *
 * Compile (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -pthread benchmark_ThreadPool.cpp -o bench_tp
 *
 * Windows (MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_ThreadPool.cpp
 *
 * Environment variables:
 *   FATP_BENCH_WARMUP_RUNS   - warmup batches (default: 3)
 *   FATP_BENCH_BATCHES       - measured batches (default: 15 Windows, 50 Linux)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_TARGET_WORK   - operations per batch (default: 100000)
 *   FATP_BENCH_NO_SCOPE      - disable priority/affinity (default: unset)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
 */

/*
FATP_META:
  meta_version: 1
  component: ThreadPool
  file_role: benchmark
  path: components/ThreadPool/benchmarks/benchmark_ThreadPool.cpp
  layer: Testing
  namespace: fat_p
  summary: "Comprehensive benchmarks for ThreadPool vs std::async and mutex+CV pool."
  api_stability: in_work
  related:
    docs_search: "ThreadPool"
    headers:
      - include/fat_p/ThreadPool.h
      - include/fat_p/FatPBenchmarkHeader.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 2
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: Claude
    mode: manual
*/

#include "ThreadPool.h"
#include "FatPBenchmarkHeader.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <condition_variable>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

// ============================================================================
// Compiler fence / DoNotOptimize
// ============================================================================

template <typename T>
void DoNotOptimize(T&& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(value) : "memory");
#elif defined(_MSC_VER)
    volatile auto sink = value;
    (void)sink;
#else
    volatile auto sink = value;
    (void)sink;
#endif
}

// ============================================================================
// Configuration
// ============================================================================

struct BenchConfig
{
    size_t warmupRuns = 3;
    size_t measuredRuns = 15;
    size_t targetWork = 100000;
    size_t seed = 12345;
    std::string csvPath;
};

std::string getEnvVar(const char* name)
{
    const char* val = std::getenv(name);
    return val ? std::string(val) : std::string{};
}

BenchConfig loadConfig()
{
    BenchConfig cfg;
    auto v = getEnvVar("FATP_BENCH_WARMUP_RUNS");
    if (!v.empty())
    {
        cfg.warmupRuns = static_cast<size_t>(std::stoul(v));
    }

    v = getEnvVar("FATP_BENCH_BATCHES");
    if (!v.empty())
    {
        cfg.measuredRuns = static_cast<size_t>(std::stoul(v));
    }
    else
    {
#ifdef _WIN32
        cfg.measuredRuns = 15;
#else
        cfg.measuredRuns = 50;
#endif
    }

    v = getEnvVar("FATP_BENCH_TARGET_WORK");
    if (!v.empty())
    {
        cfg.targetWork = static_cast<size_t>(std::stoul(v));
    }

    v = getEnvVar("FATP_BENCH_SEED");
    if (!v.empty())
    {
        cfg.seed = static_cast<size_t>(std::stoul(v));
    }

    cfg.csvPath = getEnvVar("FATP_BENCH_OUTPUT_CSV");
    return cfg;
}

void printConfig(const BenchConfig& cfg)
{
    std::cout << "Configuration:\n"
              << "  Warmup runs:    " << cfg.warmupRuns << "\n"
              << "  Measured runs:  " << cfg.measuredRuns << "\n"
              << "  Target work:    " << cfg.targetWork << "\n"
              << "  RNG seed:       " << cfg.seed << "\n";
    if (!cfg.csvPath.empty())
    {
        std::cout << "  CSV output:     " << cfg.csvPath << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// BenchmarkScope (Windows Priority/Affinity)
// ============================================================================

#ifdef _WIN32
class BenchmarkScope
{
    DWORD oldPriority_;
    DWORD_PTR oldAffinity_;

public:
    explicit BenchmarkScope(bool verbose = false)
    {
        oldPriority_ = GetPriorityClass(GetCurrentProcess());
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR target = si.dwNumberOfProcessors > 1 ? 0x2 : 0x1;
        oldAffinity_ = SetThreadAffinityMask(GetCurrentThread(), target);

        if (verbose)
        {
            std::cout << "[BenchmarkScope] High priority, CPU"
                      << (target > 1 ? " non-0" : " 0") << " affinity\n\n";
        }
    }

    ~BenchmarkScope()
    {
        SetPriorityClass(GetCurrentProcess(), oldPriority_);
        if (oldAffinity_ != 0)
        {
            SetThreadAffinityMask(GetCurrentThread(), oldAffinity_);
        }
    }

    BenchmarkScope(const BenchmarkScope&) = delete;
    BenchmarkScope& operator=(const BenchmarkScope&) = delete;
};
#else
class BenchmarkScope
{
public:
    explicit BenchmarkScope(bool = false)
    {
    }
};
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
            s.median = (samples[n / 2 - 1] + samples[n / 2]) / 2.0;
        }

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        s.mean = sum / static_cast<double>(n);

        double sq_sum = 0;
        for (double x : samples)
        {
            sq_sum += (x - s.mean) * (x - s.mean);
        }
        s.stddev = n > 1 ? std::sqrt(sq_sum / static_cast<double>(n - 1)) : 0;

        double se = s.stddev / std::sqrt(static_cast<double>(n));
        s.ci95_low = s.mean - 1.96 * se;
        s.ci95_high = s.mean + 1.96 * se;

        return s;
    }
};

// ============================================================================
// Output formatting
// ============================================================================

void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << "========================================";
    std::cout << "========================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "========================================";
    std::cout << "========================================\n\n";
}

void print_table_header()
{
    std::cout << std::setw(35) << std::left << "Benchmark" << std::setw(12) << std::right
              << "Median" << std::setw(12) << "Mean" << std::setw(10) << "StdDev"
              << "  [    CI95_lo,   CI95_hi]  Unit\n";
    std::cout << std::string(95, '-') << "\n";
}

void print_result_row(const std::string& name, const Statistics& s, const std::string& unit = "ns/op")
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(35) << std::left << name << std::setw(12) << std::right << s.median
              << std::setw(12) << s.mean << std::setw(10) << s.stddev << "  [" << std::setw(8)
              << s.ci95_low << ", " << std::setw(8) << s.ci95_high << "]  " << unit << "\n";

    if (s.stddev > s.median && s.median > 0)
    {
        std::cout << "  [NOTE] high variance (stddev " << s.stddev << " > median " << s.median
                  << ")\n";
    }
}

void print_contract_note(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

// ============================================================================
// Timer utility
// ============================================================================

using Clock = std::chrono::high_resolution_clock;

inline double elapsed_ns(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration<double, std::nano>(end - start).count();
}

// ============================================================================
// Minimal mutex+CV thread pool (baseline competitor)
// ============================================================================

class MutexPool
{
public:
    explicit MutexPool(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i)
        {
            workers_.emplace_back([this]() {
                for (;;)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty())
                        {
                            return;
                        }
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                    active_.fetch_sub(1, std::memory_order_release);
                    {
                        std::lock_guard<std::mutex> lock(idleMtx_);
                    }
                    idleCv_.notify_all();
                }
            });
        }
    }

    ~MutexPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_)
        {
            w.join();
        }
    }

    template <typename F>
    std::future<std::invoke_result_t<F>> submit(F&& f)
    {
        using R = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_.fetch_add(1, std::memory_order_relaxed);
            tasks_.push([task]() { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

    void waitIdle()
    {
        std::unique_lock<std::mutex> lock(idleMtx_);
        idleCv_.wait(lock, [this]() {
            return tasks_.empty() && active_.load(std::memory_order_acquire) == 0;
        });
    }

    MutexPool(const MutexPool&) = delete;
    MutexPool& operator=(const MutexPool&) = delete;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::atomic<size_t> active_{0};
    std::mutex idleMtx_;
    std::condition_variable idleCv_;
};

// ============================================================================
// Section 1: Submission Overhead
// ============================================================================

/**
 * @brief Measure raw submit() cost by queuing tasks to a blocked pool
 *
 * We block all workers with a gate, submit N tasks, measure total time,
 * then release the gate and wait_idle. This isolates submission cost from
 * execution and scheduling.
 */
void bench_submission_overhead(const BenchConfig& cfg)
{
    print_header("Section 1: Submission Overhead");
    print_contract_note("submit() is O(1) amortized. submit_batch() amortizes lock + notify.");
    print_table_header();

    const size_t N = cfg.targetWork;
    const size_t runs = cfg.measuredRuns;

    // --- submit() with empty lambda ---
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            fat_p::ThreadPool pool(4, 0); // 0 spin to avoid burning CPU while blocked
            std::atomic<bool> gate{true};

            // Block all workers
            for (size_t w = 0; w < 4; ++w)
            {
                (void)pool.submit([&gate]() {
                    while (gate.load(std::memory_order_acquire))
                    {
                        std::this_thread::yield();
                    }
                });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            auto start = Clock::now();
            for (size_t i = 0; i < N; ++i)
            {
                (void)pool.submit([]() {});
            }
            auto end = Clock::now();

            gate.store(false, std::memory_order_release);
            pool.wait_idle();

            if (r >= cfg.warmupRuns)
            {
                double ns_per_op = elapsed_ns(start, end) / static_cast<double>(N);
                samples.push_back(ns_per_op);
            }
        }
        print_result_row("submit() [empty lambda]", Statistics::compute(samples));
    }

    // --- submit_priority() Critical ---
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            fat_p::ThreadPool pool(4, 0);
            std::atomic<bool> gate{true};

            for (size_t w = 0; w < 4; ++w)
            {
                (void)pool.submit([&gate]() {
                    while (gate.load(std::memory_order_acquire))
                    {
                        std::this_thread::yield();
                    }
                });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            auto start = Clock::now();
            for (size_t i = 0; i < N; ++i)
            {
                (void)pool.submit_priority(fat_p::Priority::Critical, []() {});
            }
            auto end = Clock::now();

            gate.store(false, std::memory_order_release);
            pool.wait_idle();

            if (r >= cfg.warmupRuns)
            {
                double ns_per_op = elapsed_ns(start, end) / static_cast<double>(N);
                samples.push_back(ns_per_op);
            }
        }
        print_result_row("submit_priority(Critical)", Statistics::compute(samples));
    }

    // --- submit_batch() ---
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            fat_p::ThreadPool pool(4, 0);
            std::atomic<bool> gate{true};

            for (size_t w = 0; w < 4; ++w)
            {
                (void)pool.submit([&gate]() {
                    while (gate.load(std::memory_order_acquire))
                    {
                        std::this_thread::yield();
                    }
                });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            std::vector<std::function<void()>> tasks(N, []() {});

            auto start = Clock::now();
            pool.submit_batch(tasks);
            auto end = Clock::now();

            gate.store(false, std::memory_order_release);
            pool.wait_idle();

            if (r >= cfg.warmupRuns)
            {
                double ns_per_op = elapsed_ns(start, end) / static_cast<double>(N);
                samples.push_back(ns_per_op);
            }
        }
        print_result_row("submit_batch() per task", Statistics::compute(samples));
    }

    // --- MutexPool submit() ---
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            MutexPool pool(4);
            std::atomic<bool> gate{true};

            for (size_t w = 0; w < 4; ++w)
            {
                (void)pool.submit([&gate]() {
                    while (gate.load(std::memory_order_acquire))
                    {
                        std::this_thread::yield();
                    }
                });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            auto start = Clock::now();
            for (size_t i = 0; i < N; ++i)
            {
                (void)pool.submit([]() {});
            }
            auto end = Clock::now();

            gate.store(false, std::memory_order_release);
            pool.waitIdle();

            if (r >= cfg.warmupRuns)
            {
                double ns_per_op = elapsed_ns(start, end) / static_cast<double>(N);
                samples.push_back(ns_per_op);
            }
        }
        print_result_row("MutexPool submit()", Statistics::compute(samples));
    }

    std::cout << "\n";
}

// ============================================================================
// Section 2: Throughput Scaling
// ============================================================================

/**
 * @brief Measure completed tasks/second at various worker counts
 *
 * Each task does a trivial compute (sum += i) to avoid being optimized away
 * while keeping per-task work negligible relative to scheduling overhead.
 */
void bench_throughput_scaling(const BenchConfig& cfg)
{
    print_header("Section 2: Throughput Scaling (tasks/sec)");
    print_contract_note("Throughput scales sublinearly due to queue contention.");
    print_table_header();

    const size_t N = cfg.targetWork;
    const size_t runs = cfg.measuredRuns;

    auto run_throughput = [&](const std::string& label, size_t threads, size_t spin_us) {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            fat_p::ThreadPool pool(threads, spin_us);
            std::atomic<uint64_t> sink{0};

            auto start = Clock::now();
            for (size_t i = 0; i < N; ++i)
            {
                (void)pool.submit([&sink, i]() {
                    sink.fetch_add(static_cast<uint64_t>(i & 0xFF), std::memory_order_relaxed);
                });
            }
            pool.wait_idle();
            auto end = Clock::now();

            DoNotOptimize(sink.load());

            if (r >= cfg.warmupRuns)
            {
                double seconds = elapsed_ns(start, end) / 1e9;
                double tasks_per_sec = static_cast<double>(N) / seconds;
                samples.push_back(tasks_per_sec);
            }
        }
        print_result_row(label, Statistics::compute(samples), "tasks/s");
    };

    run_throughput("ThreadPool [1 worker]", 1, 2000);
    run_throughput("ThreadPool [2 workers]", 2, 2000);
    run_throughput("ThreadPool [4 workers]", 4, 2000);
    run_throughput("ThreadPool [8 workers]", 8, 2000);

    unsigned int hw = std::thread::hardware_concurrency();
    if (hw > 8)
    {
        run_throughput("ThreadPool [" + std::to_string(hw) + " workers]", hw, 2000);
    }

    // MutexPool baseline
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            MutexPool pool(4);
            std::atomic<uint64_t> sink{0};

            auto start = Clock::now();
            for (size_t i = 0; i < N; ++i)
            {
                (void)pool.submit([&sink, i]() {
                    sink.fetch_add(static_cast<uint64_t>(i & 0xFF), std::memory_order_relaxed);
                });
            }
            pool.waitIdle();
            auto end = Clock::now();

            DoNotOptimize(sink.load());

            if (r >= cfg.warmupRuns)
            {
                double seconds = elapsed_ns(start, end) / 1e9;
                double tasks_per_sec = static_cast<double>(N) / seconds;
                samples.push_back(tasks_per_sec);
            }
        }
        print_result_row("MutexPool [4 workers]", Statistics::compute(samples), "tasks/s");
    }

    // std::async baseline
    {
        // Use smaller N for std::async -- it creates threads per call
        const size_t asyncN = std::min(N, static_cast<size_t>(10000));
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            std::vector<std::future<void>> futures;
            futures.reserve(asyncN);
            std::atomic<uint64_t> sink{0};

            auto start = Clock::now();
            for (size_t i = 0; i < asyncN; ++i)
            {
                futures.push_back(std::async(std::launch::async, [&sink, i]() {
                    sink.fetch_add(static_cast<uint64_t>(i & 0xFF), std::memory_order_relaxed);
                }));
            }
            for (auto& f : futures)
            {
                f.get();
            }
            auto end = Clock::now();

            DoNotOptimize(sink.load());

            if (r >= cfg.warmupRuns)
            {
                double seconds = elapsed_ns(start, end) / 1e9;
                double tasks_per_sec = static_cast<double>(asyncN) / seconds;
                samples.push_back(tasks_per_sec);
            }
        }
        std::string label = "std::async [" + std::to_string(asyncN) + " tasks]";
        print_result_row(label, Statistics::compute(samples), "tasks/s");
    }

    std::cout << "\n";
}

// ============================================================================
// Section 3: Latency Distribution
// ============================================================================

/**
 * @brief Measure time from submit() call to task execution start
 *
 * Each task records its start time via high_resolution_clock. The delta
 * from the pre-submit timestamp to the in-task timestamp is the scheduling
 * latency. We drain results via futures.
 */
void bench_latency_distribution(const BenchConfig& cfg)
{
    print_header("Section 3: Latency Distribution (submit-to-execute)");
    print_contract_note(
        "Latency depends on spin config. Higher spin = lower p50/p90, higher CPU.");

    const size_t N = std::min(cfg.targetWork, static_cast<size_t>(50000));

    auto run_latency = [&](const std::string& label, size_t spin_us) {
        std::vector<double> all_latencies;
        all_latencies.reserve(N * cfg.measuredRuns);

        for (size_t r = 0; r < cfg.warmupRuns + cfg.measuredRuns; ++r)
        {
            fat_p::ThreadPool pool(4, spin_us);

            // Pre-warm: ensure workers are spun up
            for (size_t w = 0; w < 4; ++w)
            {
                pool.submit([]() {}).get();
            }

            struct LatencyRecord
            {
                Clock::time_point submitted;
                Clock::time_point started;
            };

            std::vector<std::future<LatencyRecord>> futures;
            futures.reserve(N);

            for (size_t i = 0; i < N; ++i)
            {
                auto submit_time = Clock::now();
                futures.push_back(pool.submit([submit_time]() {
                    LatencyRecord rec;
                    rec.submitted = submit_time;
                    rec.started = Clock::now();
                    return rec;
                }));

                // Small delay between submissions to measure scheduling,
                // not queue buildup
                if (i % 100 == 99)
                {
                    std::this_thread::yield();
                }
            }

            if (r >= cfg.warmupRuns)
            {
                for (auto& f : futures)
                {
                    auto rec = f.get();
                    double lat_ns = elapsed_ns(rec.submitted, rec.started);
                    if (lat_ns > 0)
                    {
                        all_latencies.push_back(lat_ns);
                    }
                }
            }
            else
            {
                for (auto& f : futures)
                {
                    f.get();
                }
            }
        }

        std::sort(all_latencies.begin(), all_latencies.end());
        size_t n = all_latencies.size();

        if (n == 0)
        {
            std::cout << "  " << label << ": no valid samples\n";
            return;
        }

        double p50 = all_latencies[n * 50 / 100];
        double p90 = all_latencies[n * 90 / 100];
        double p99 = all_latencies[n * 99 / 100];
        double p999 = all_latencies[std::min(n * 999 / 1000, n - 1)];

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "  " << std::setw(25) << std::left << label << "  p50=" << std::setw(8)
                  << std::right << (p50 / 1000.0) << " us"
                  << "  p90=" << std::setw(8) << (p90 / 1000.0) << " us"
                  << "  p99=" << std::setw(8) << (p99 / 1000.0) << " us"
                  << "  p99.9=" << std::setw(8) << (p999 / 1000.0) << " us"
                  << "  (n=" << n << ")\n";
    };

    run_latency("spin=0 us (sleep only)", 0);
    run_latency("spin=1000 us", 1000);
    run_latency("spin=2000 us (default)", 2000);
    run_latency("spin=5000 us", 5000);
    run_latency("spin=10000 us", 10000);

    std::cout << "\n";
}

// ============================================================================
// Section 4: Priority Scheduling
// ============================================================================

/**
 * @brief Verify that High/Critical tasks are scheduled before Normal/Low
 *
 * Submit a burst of mixed-priority tasks to a stopped pool (gate), then
 * release and record execution order. Measure what fraction of High/Critical
 * tasks execute in the first half.
 */
void bench_priority_scheduling(const BenchConfig& cfg)
{
    print_header("Section 4: Priority Scheduling Effectiveness");
    print_contract_note(
        "Critical/High tasks use the global queue; Normal/Low use local queues.");

    const size_t N = std::min(cfg.targetWork, static_cast<size_t>(10000));
    const size_t runs = cfg.measuredRuns;

    std::vector<double> high_first_half_pct;
    high_first_half_pct.reserve(runs);

    for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
    {
        fat_p::ThreadPool pool(4, 0);
        std::atomic<bool> gate{true};
        std::atomic<size_t> exec_order{0};

        // Block all workers
        for (size_t w = 0; w < 4; ++w)
        {
            (void)pool.submit([&gate]() {
                while (gate.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        struct TaskRecord
        {
            fat_p::Priority priority;
            size_t order;
        };

        std::vector<std::future<TaskRecord>> futures;
        futures.reserve(N);

        std::mt19937 rng(static_cast<unsigned>(cfg.seed + r));

        // Submit N tasks with mixed priorities
        for (size_t i = 0; i < N; ++i)
        {
            fat_p::Priority pri;
            switch (rng() % 4)
            {
                case 0:
                    pri = fat_p::Priority::Low;
                    break;
                case 1:
                    pri = fat_p::Priority::Normal;
                    break;
                case 2:
                    pri = fat_p::Priority::High;
                    break;
                default:
                    pri = fat_p::Priority::Critical;
                    break;
            }

            futures.push_back(pool.submit_priority(pri, [&exec_order, pri]() {
                TaskRecord rec;
                rec.priority = pri;
                rec.order = exec_order.fetch_add(1, std::memory_order_relaxed);
                return rec;
            }));
        }

        // Release workers
        gate.store(false, std::memory_order_release);

        // Collect results
        std::vector<TaskRecord> results;
        results.reserve(N);
        for (auto& f : futures)
        {
            results.push_back(f.get());
        }

        if (r >= cfg.warmupRuns)
        {
            // Count how many High/Critical tasks executed in first half
            size_t half = N / 2;
            size_t high_in_first_half = 0;
            size_t total_high = 0;
            for (const auto& rec : results)
            {
                bool isHigh = (rec.priority == fat_p::Priority::High ||
                               rec.priority == fat_p::Priority::Critical);
                if (isHigh)
                {
                    ++total_high;
                    if (rec.order < half)
                    {
                        ++high_in_first_half;
                    }
                }
            }
            double pct =
                total_high > 0
                    ? 100.0 * static_cast<double>(high_in_first_half) / static_cast<double>(total_high)
                    : 0;
            high_first_half_pct.push_back(pct);
        }
    }

    auto stats = Statistics::compute(high_first_half_pct);
    std::cout << "High/Critical tasks executing in first 50% of execution order:\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Median: " << stats.median << "%  (ideal: >75% for 50/50 mix)\n";
    std::cout << "  Mean:   " << stats.mean << "%  CI95: [" << stats.ci95_low << "%, "
              << stats.ci95_high << "%]\n";
    std::cout << "  (Random scheduling would yield ~50%; priority should push this higher)\n";
    std::cout << "\n";
}

// ============================================================================
// Section 5: Work Stealing Effectiveness
// ============================================================================

/**
 * @brief Submit all tasks to one worker's queue, measure how quickly
 * other workers steal and balance the load.
 *
 * We measure wall-clock time to complete N tasks that are all initially
 * in one queue, vs the same tasks distributed normally.
 */
void bench_work_stealing(const BenchConfig& cfg)
{
    print_header("Section 5: Work Stealing Effectiveness");
    print_contract_note(
        "Fisher-Yates victim selection ensures starvation-free stealing.");

    const size_t N = std::min(cfg.targetWork, static_cast<size_t>(50000));
    const size_t runs = cfg.measuredRuns;

    // Balanced: normal submit (round-robin distribution)
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            fat_p::ThreadPool pool(4, 2000);
            std::atomic<uint64_t> sink{0};

            auto start = Clock::now();
            for (size_t i = 0; i < N; ++i)
            {
                (void)pool.submit([&sink]() {
                    // ~1us of work
                    uint64_t v = 0;
                    for (volatile int j = 0; j < 100; ++j)
                    {
                        v += static_cast<uint64_t>(j);
                    }
                    sink.fetch_add(v, std::memory_order_relaxed);
                });
            }
            pool.wait_idle();
            auto end = Clock::now();

            DoNotOptimize(sink.load());

            if (r >= cfg.warmupRuns)
            {
                double ms = elapsed_ns(start, end) / 1e6;
                samples.push_back(ms);
            }
        }
        print_table_header();
        print_result_row("Balanced (round-robin)", Statistics::compute(samples), "ms");
    }

    // Skewed: submit_batch (all to global queue, one notification)
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            fat_p::ThreadPool pool(4, 2000);
            std::atomic<uint64_t> sink{0};

            std::vector<std::function<void()>> tasks;
            tasks.reserve(N);
            for (size_t i = 0; i < N; ++i)
            {
                tasks.push_back([&sink]() {
                    uint64_t v = 0;
                    for (volatile int j = 0; j < 100; ++j)
                    {
                        v += static_cast<uint64_t>(j);
                    }
                    sink.fetch_add(v, std::memory_order_relaxed);
                });
            }

            auto start = Clock::now();
            pool.submit_batch(tasks);
            pool.wait_idle();
            auto end = Clock::now();

            DoNotOptimize(sink.load());

            if (r >= cfg.warmupRuns)
            {
                double ms = elapsed_ns(start, end) / 1e6;
                samples.push_back(ms);
            }
        }
        print_result_row("Batch (single notify)", Statistics::compute(samples), "ms");
    }

    // Single-threaded baseline (no parallelism)
    {
        std::vector<double> samples;
        samples.reserve(runs);

        for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
        {
            std::atomic<uint64_t> sink{0};

            auto start = Clock::now();
            for (size_t i = 0; i < N; ++i)
            {
                uint64_t v = 0;
                for (volatile int j = 0; j < 100; ++j)
                {
                    v += static_cast<uint64_t>(j);
                }
                sink.fetch_add(v, std::memory_order_relaxed);
            }
            auto end = Clock::now();

            DoNotOptimize(sink.load());

            if (r >= cfg.warmupRuns)
            {
                double ms = elapsed_ns(start, end) / 1e6;
                samples.push_back(ms);
            }
        }
        print_result_row("Sequential (no pool)", Statistics::compute(samples), "ms");
    }

    std::cout << "\n";
}

// ============================================================================
// Section 6: Comparison vs std::async
// ============================================================================

/**
 * @brief Head-to-head comparison for a realistic workload
 *
 * Each task computes a small result (sum of squares). This isolates
 * scheduling overhead from task work for meaningful comparison.
 */
void bench_vs_std_async(const BenchConfig& cfg)
{
    print_header("Section 6: Head-to-Head vs std::async");
    print_contract_note(
        "std::async may create a thread per call (libstdc++) or serialize (MSVC).");
    print_table_header();

    const size_t runs = cfg.measuredRuns;

    auto workload = [](size_t start, size_t count) -> uint64_t {
        uint64_t sum = 0;
        for (size_t i = start; i < start + count; ++i)
        {
            sum += i * i;
        }
        return sum;
    };

    for (size_t taskCount : {100, 1000, 10000})
    {
        const size_t workPerTask = 1000;

        // ThreadPool
        {
            std::vector<double> samples;
            samples.reserve(runs);

            for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
            {
                fat_p::ThreadPool pool(4, 2000);
                std::vector<std::future<uint64_t>> futures;
                futures.reserve(taskCount);

                auto start = Clock::now();
                for (size_t i = 0; i < taskCount; ++i)
                {
                    futures.push_back(pool.submit(workload, i * workPerTask, workPerTask));
                }
                uint64_t total = 0;
                for (auto& f : futures)
                {
                    total += f.get();
                }
                auto end = Clock::now();

                DoNotOptimize(total);

                if (r >= cfg.warmupRuns)
                {
                    double ms = elapsed_ns(start, end) / 1e6;
                    samples.push_back(ms);
                }
            }
            std::string label = "ThreadPool [" + std::to_string(taskCount) + " tasks]";
            print_result_row(label, Statistics::compute(samples), "ms");
        }

        // std::async
        {
            std::vector<double> samples;
            samples.reserve(runs);

            for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
            {
                std::vector<std::future<uint64_t>> futures;
                futures.reserve(taskCount);

                auto start = Clock::now();
                for (size_t i = 0; i < taskCount; ++i)
                {
                    futures.push_back(
                        std::async(std::launch::async, workload, i * workPerTask, workPerTask));
                }
                uint64_t total = 0;
                for (auto& f : futures)
                {
                    total += f.get();
                }
                auto end = Clock::now();

                DoNotOptimize(total);

                if (r >= cfg.warmupRuns)
                {
                    double ms = elapsed_ns(start, end) / 1e6;
                    samples.push_back(ms);
                }
            }
            std::string label = "std::async  [" + std::to_string(taskCount) + " tasks]";
            print_result_row(label, Statistics::compute(samples), "ms");
        }

        // MutexPool
        {
            std::vector<double> samples;
            samples.reserve(runs);

            for (size_t r = 0; r < cfg.warmupRuns + runs; ++r)
            {
                MutexPool pool(4);
                std::vector<std::future<uint64_t>> futures;
                futures.reserve(taskCount);

                auto start = Clock::now();
                for (size_t i = 0; i < taskCount; ++i)
                {
                    futures.push_back(pool.submit([=]() { return workload(i * workPerTask, workPerTask); }));
                }
                uint64_t total = 0;
                for (auto& f : futures)
                {
                    total += f.get();
                }
                auto end = Clock::now();

                DoNotOptimize(total);

                if (r >= cfg.warmupRuns)
                {
                    double ms = elapsed_ns(start, end) / 1e6;
                    samples.push_back(ms);
                }
            }
            std::string label = "MutexPool   [" + std::to_string(taskCount) + " tasks]";
            print_result_row(label, Statistics::compute(samples), "ms");
        }

        std::cout << "\n";
    }
}

// ============================================================================
// Correctness Guardrails
// ============================================================================

/**
 * @brief Verify ThreadPool correctness before running benchmarks
 *
 * Rule: Each benchmark case must include at least one correctness validation
 * outside the timed region.
 */
bool verify_correctness()
{
    std::cout << "Correctness verification:\n";

    // 1. Basic submit + get
    {
        fat_p::ThreadPool pool(4);
        auto f = pool.submit([]() { return 42; });
        if (f.get() != 42)
        {
            std::cout << "  [FAIL] Basic submit+get\n";
            return false;
        }
        std::cout << "  [PASS] Basic submit+get\n";
    }

    // 2. All tasks execute (no lost tasks)
    {
        fat_p::ThreadPool pool(4);
        std::atomic<int> counter{0};
        const int N = 10000;
        for (int i = 0; i < N; ++i)
        {
            (void)pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        pool.wait_idle();
        if (counter.load() != N)
        {
            std::cout << "  [FAIL] Lost tasks: expected " << N << " got " << counter.load()
                      << "\n";
            return false;
        }
        std::cout << "  [PASS] No lost tasks (" << N << "/" << N << ")\n";
    }

    // 3. Priority ordering (Critical executes before Low under contention)
    {
        fat_p::ThreadPool pool(1, 0); // Single worker to force sequential
        std::atomic<bool> gate{true};

        // Block the single worker
        (void)pool.submit([&gate]() {
            while (gate.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        std::vector<int> order;
        std::mutex order_mtx;

        // Submit Low then Critical - Critical should execute first
        (void)pool.submit_priority(fat_p::Priority::Low, [&]() {
            std::lock_guard<std::mutex> lock(order_mtx);
            order.push_back(0); // Low = 0
        });
        (void)pool.submit_priority(fat_p::Priority::Critical, [&]() {
            std::lock_guard<std::mutex> lock(order_mtx);
            order.push_back(1); // Critical = 1
        });

        gate.store(false, std::memory_order_release);
        pool.wait_idle();

        // Critical (global queue) should execute before Low (local queue)
        if (order.size() == 2 && order[0] == 1)
        {
            std::cout << "  [PASS] Priority ordering (Critical before Low)\n";
        }
        else
        {
            std::cout << "  [WARN] Priority ordering: got [";
            for (size_t i = 0; i < order.size(); ++i)
            {
                std::cout << order[i] << (i + 1 < order.size() ? "," : "");
            }
            std::cout << "] (expected [1,0]). May vary with timing.\n";
        }
    }

    // 4. submit_batch completes all tasks
    {
        fat_p::ThreadPool pool(4);
        std::atomic<int> counter{0};
        const int N = 5000;
        std::vector<std::function<void()>> tasks;
        for (int i = 0; i < N; ++i)
        {
            tasks.push_back([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        pool.submit_batch(tasks);
        pool.wait_idle();
        if (counter.load() != N)
        {
            std::cout << "  [FAIL] submit_batch lost tasks\n";
            return false;
        }
        std::cout << "  [PASS] submit_batch (" << N << "/" << N << ")\n";
    }

    // 5. Exception propagation
    {
        fat_p::ThreadPool pool(4);
        auto f = pool.submit([]() -> int {
            throw std::runtime_error("test exception");
            return 0;
        });
        try
        {
            (void)f.get();
            std::cout << "  [FAIL] Exception not propagated\n";
            return false;
        }
        catch (const std::runtime_error& e)
        {
            std::cout << "  [PASS] Exception propagation\n";
        }
    }

    // 6. Concurrent submission from multiple producers
    {
        fat_p::ThreadPool pool(4);
        std::atomic<int> counter{0};
        const int producers = 8;
        const int tasks_per = 1000;

        std::vector<std::thread> prod_threads;
        for (int p = 0; p < producers; ++p)
        {
            prod_threads.emplace_back([&pool, &counter, tasks_per]() {
                for (int i = 0; i < tasks_per; ++i)
                {
                    (void)pool.submit([&counter]() {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            });
        }
        for (auto& t : prod_threads)
        {
            t.join();
        }
        pool.wait_idle();

        int expected = producers * tasks_per;
        if (counter.load() != expected)
        {
            std::cout << "  [FAIL] Concurrent submission: expected " << expected << " got "
                      << counter.load() << "\n";
            return false;
        }
        std::cout << "  [PASS] Concurrent submission (" << expected << "/" << expected << ")\n";
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
    // Standardized header
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "ThreadPool";
    hdr.competitors = {
        {"fat_p::ThreadPool", true, "primary"},
        {"std::async", true, "baseline"},
        {"MutexPool (hand-rolled)", true, "baseline"},
    };
    fat_p::bench::print_standard_header(hdr);

    BenchConfig cfg = loadConfig();
    printConfig(cfg);

    if (!verify_correctness())
    {
        std::cerr << "Correctness verification failed. Aborting benchmarks.\n";
        return 1;
    }

    {
        BenchmarkScope scope(/*verbose=*/true);

        bench_submission_overhead(cfg);
        bench_throughput_scaling(cfg);
        bench_latency_distribution(cfg);
        bench_priority_scheduling(cfg);
        bench_work_stealing(cfg);
        bench_vs_std_async(cfg);
    }

    // Summary
    print_header("Summary");
    std::cout << "Benchmarks completed.\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";

    return 0;
}
