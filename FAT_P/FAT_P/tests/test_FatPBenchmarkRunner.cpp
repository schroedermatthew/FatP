/**
 * @file test_FatPBenchmarkRunner.cpp
 * @brief Unit tests for FatPBenchmarkRunner.h
 * @layer Application
 *
 * Build standalone:
 *   g++ -std=c++17 -O2 -DENABLE_TEST_APPLICATION -I. test_FatPBenchmarkRunner.cpp -o test_runner -lpthread
 *   cl /std:c++17 /O2 /DENABLE_TEST_APPLICATION /I. test_FatPBenchmarkRunner.cpp
 */
/*
FATP_META:
  meta_version: 1
  component: FatPBenchmarkRunner
  file_role: test
  path: tests/test_FatPBenchmarkRunner.cpp
  namespace: fat_p
  summary: "Unit tests for FatPBenchmarkRunner."
  related:
    docs_search: "FatPBenchmarkRunner"
    headers:
      - fat_p/FatPBenchmarkRunner.h
      - fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "FatPTest.h"

namespace fat_p::testing::benchmarkrunner
{

// ============================================================================
// Timer Tests
// ============================================================================

FATP_TEST_CASE(timer_basic)
{
    using namespace fat_p::bench;

    Timer timer;
    timer.start();

    // Do some work
    volatile int sum = 0;
    for (int i = 0; i < 1000; ++i)
    {
        sum += i;
    }
    DoNotOptimize(sum);

    double elapsed = timer.elapsedNs();

    FATP_ASSERT_GT(elapsed, 0.0, "Elapsed time should be positive");

    return true;
}

FATP_TEST_CASE(timer_multiple_reads)
{
    using namespace fat_p::bench;

    Timer timer;
    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double t1 = timer.elapsedNs();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double t2 = timer.elapsedNs();

    FATP_ASSERT_GT(t1, 0.0, "First read should be positive");
    FATP_ASSERT_GT(t2, t1, "Second read should be greater than first");

    return true;
}

// ============================================================================
// Statistics Tests
// ============================================================================

FATP_TEST_CASE(statistics_basic)
{
    using namespace fat_p::bench;

    std::vector<double> samples = {1.0, 2.0, 3.0, 4.0, 5.0};
    Statistics stats = Statistics::compute(std::move(samples));

    FATP_ASSERT_EQ(stats.samples, std::size_t(5), "Should have 5 samples");
    FATP_ASSERT_CLOSE(stats.min, 1.0, "Min should be 1.0");
    FATP_ASSERT_CLOSE(stats.max, 5.0, "Max should be 5.0");
    FATP_ASSERT_CLOSE(stats.mean, 3.0, "Mean should be 3.0");
    FATP_ASSERT_CLOSE(stats.median, 3.0, "Median should be 3.0");

    return true;
}

FATP_TEST_CASE(statistics_median_even)
{
    using namespace fat_p::bench;

    std::vector<double> samples = {1.0, 2.0, 3.0, 4.0};
    Statistics stats = Statistics::compute(std::move(samples));

    // Median of [1, 2, 3, 4] = (2 + 3) / 2 = 2.5
    FATP_ASSERT_CLOSE(stats.median, 2.5, "Median of even samples should be average of middle two");

    return true;
}

FATP_TEST_CASE(statistics_stddev)
{
    using namespace fat_p::bench;

    // Values: 2, 4, 4, 4, 5, 5, 7, 9
    // Mean = 5
    // Sample variance = 32/7 ≈ 4.571, Sample StdDev ≈ 2.138
    std::vector<double> samples = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    Statistics stats = Statistics::compute(std::move(samples));

    FATP_ASSERT_CLOSE(stats.mean, 5.0, "Mean should be 5.0");
    FATP_ASSERT_CLOSE_EPS(stats.stddev, 2.138, 0.01, "StdDev should be ~2.138 (sample stddev)");

    return true;
}

FATP_TEST_CASE(statistics_percentiles)
{
    using namespace fat_p::bench;

    // Create 100 samples: 1, 2, 3, ..., 100
    std::vector<double> samples;
    for (int i = 1; i <= 100; ++i)
    {
        samples.push_back(static_cast<double>(i));
    }

    Statistics stats = Statistics::compute(std::move(samples));

    // P95 should be around 95, P99 around 99
    FATP_ASSERT_GE(stats.p95, 94.0, "P95 should be >= 94");
    FATP_ASSERT_LE(stats.p95, 96.0, "P95 should be <= 96");
    FATP_ASSERT_GE(stats.p99, 98.0, "P99 should be >= 98");
    FATP_ASSERT_LE(stats.p99, 100.0, "P99 should be <= 100");

    return true;
}

FATP_TEST_CASE(statistics_ci95)
{
    using namespace fat_p::bench;

    std::vector<double> samples = {100.0, 100.0, 100.0, 100.0, 100.0};
    Statistics stats = Statistics::compute(std::move(samples));

    // With zero stddev, CI95 should equal the mean
    FATP_ASSERT_CLOSE(stats.ci95Low, 100.0, "CI95 low should equal mean when stddev is 0");
    FATP_ASSERT_CLOSE(stats.ci95High, 100.0, "CI95 high should equal mean when stddev is 0");

    return true;
}

FATP_TEST_CASE(statistics_single_sample)
{
    using namespace fat_p::bench;

    std::vector<double> samples = {42.0};
    Statistics stats = Statistics::compute(std::move(samples));

    FATP_ASSERT_EQ(stats.samples, std::size_t(1), "Should have 1 sample");
    FATP_ASSERT_CLOSE(stats.min, 42.0, "Min should be 42.0");
    FATP_ASSERT_CLOSE(stats.max, 42.0, "Max should be 42.0");
    FATP_ASSERT_CLOSE(stats.mean, 42.0, "Mean should be 42.0");
    FATP_ASSERT_CLOSE(stats.median, 42.0, "Median should be 42.0");
    FATP_ASSERT_CLOSE(stats.stddev, 0.0, "StdDev should be 0.0 for single sample");

    return true;
}

// ============================================================================
// BenchConfig Tests
// ============================================================================

FATP_TEST_CASE(config_defaults)
{
    using namespace fat_p::bench;

    // Clear relevant env vars to ensure defaults
    #if defined(_WIN32)
    _putenv("FATP_BENCH_WARMUP_RUNS=");
    _putenv("FATP_BENCH_BATCHES=");
    _putenv("FATP_BENCH_SEED=");
    #else
    unsetenv("FATP_BENCH_WARMUP_RUNS");
    unsetenv("FATP_BENCH_BATCHES");
    unsetenv("FATP_BENCH_SEED");
    #endif

    BenchConfig cfg = BenchConfig::fromEnv();

    FATP_ASSERT_EQ(cfg.warmupRuns, kDefaultWarmupRuns, "Warmup should match default");
    FATP_ASSERT_EQ(cfg.seed, kDefaultSeed, "Seed should match default");

    return true;
}

// ============================================================================
// DoNotOptimize Tests
// ============================================================================

FATP_TEST_CASE(do_not_optimize_compiles)
{
    using namespace fat_p::bench;

    // Just verify these compile without error
    int x = 42;
    DoNotOptimize(x);

    double d = 3.14;
    DoNotOptimize(d);

    std::string s = "test";
    DoNotOptimize(s);

    std::vector<int> v = {1, 2, 3};
    DoNotOptimize(v);

    // If we got here, it compiled
    return true;
}

FATP_TEST_CASE(prevent_opt_compiles)
{
    using namespace fat_p::bench;

    int64_t checksum = 12345;
    preventOpt(checksum);

    // If we got here, it compiled
    FATP_ASSERT_EQ(checksum, int64_t(12345), "Value should be unchanged");

    return true;
}

// ============================================================================
// SpinBarrier Tests
// ============================================================================

FATP_TEST_CASE(spin_barrier_single_thread)
{
    using namespace fat_p::bench;

    SpinBarrier barrier(1);

    // Should not block with a single thread
    barrier.wait();

    return true;
}

FATP_TEST_CASE(spin_barrier_multi_thread)
{
    using namespace fat_p::bench;

    constexpr unsigned int kNumThreads = 4;
    SpinBarrier barrier(kNumThreads);

    std::atomic<unsigned int> counter{0};
    std::atomic<bool> allArrived{false};
    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < kNumThreads; ++i)
    {
        threads.emplace_back([&]()
        {
            counter.fetch_add(1, std::memory_order_relaxed);
            barrier.wait();
            // After barrier, all threads should have incremented
            if (counter.load(std::memory_order_relaxed) == kNumThreads)
            {
                allArrived.store(true, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), kNumThreads, "All threads should have incremented counter");
    FATP_ASSERT_TRUE(allArrived.load(), "All threads should have arrived at barrier");

    return true;
}

FATP_TEST_CASE(spin_barrier_reuse)
{
    using namespace fat_p::bench;

    constexpr unsigned int kNumThreads = 2;
    SpinBarrier barrier(kNumThreads);

    std::atomic<int> phase{0};
    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < kNumThreads; ++i)
    {
        threads.emplace_back([&]()
        {
            // Phase 1
            barrier.wait();
            phase.fetch_add(1, std::memory_order_relaxed);

            // Phase 2 - reuse barrier
            barrier.wait();
            phase.fetch_add(1, std::memory_order_relaxed);
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // 2 threads * 2 phases = 4 increments
    FATP_ASSERT_EQ(phase.load(), 4, "Barrier should be reusable across phases");

    return true;
}

// ============================================================================
// BenchmarkRunner Tests
// ============================================================================

FATP_TEST_CASE(runner_creation)
{
    using namespace fat_p::bench;

    BenchmarkRunner runner = makeTestRunner("TestRunner");

    FATP_ASSERT_EQ(runner.name(), std::string("TestRunner"), "Runner should have correct name");

    return true;
}

FATP_TEST_CASE(runner_section_and_contract)
{
    using namespace fat_p::bench;

    BenchmarkRunner runner = makeTestRunner("TestRunner");

    // Capture output
    std::ostringstream oss;
    auto oldBuf = std::cout.rdbuf(oss.rdbuf());

    runner.section("TEST SECTION")
          .contract("This is a test contract");

    std::cout.rdbuf(oldBuf);

    std::string output = oss.str();
    FATP_ASSERT_TRUE(output.find("TEST SECTION") != std::string::npos,
                "Output should contain section name");
    FATP_ASSERT_TRUE(output.find("Contract:") != std::string::npos ||
                output.find("test contract") != std::string::npos,
                "Output should contain contract");

    return true;
}

FATP_TEST_CASE(runner_add_benchmark)
{
    using namespace fat_p::bench;

    BenchmarkRunner runner = makeTestRunner("TestRunner");

    int callCount = 0;
    runner.add("test_bench", [&]()
    {
        ++callCount;
    });

    // Suppress output during test
    std::ostringstream devnull;
    auto oldBuf = std::cout.rdbuf(devnull.rdbuf());

    // Run should execute the benchmark
    runner.run();

    std::cout.rdbuf(oldBuf);

    FATP_ASSERT_GT(callCount, 0, "Benchmark should have been called at least once");

    return true;
}

FATP_TEST_CASE(runner_results)
{
    using namespace fat_p::bench;

    BenchmarkRunner runner = makeTestRunner("TestRunner");

    runner.add("simple_add", []()
    {
        volatile int x = 1 + 1;
        DoNotOptimize(x);
    });

    // Suppress output during test
    std::ostringstream devnull;
    auto oldBuf = std::cout.rdbuf(devnull.rdbuf());

    runner.run();

    std::cout.rdbuf(oldBuf);

    const auto& results = runner.results();
    FATP_ASSERT_EQ(results.size(), std::size_t(1), "Should have 1 result");
    FATP_ASSERT_EQ(results[0].name, std::string("simple_add"), "Result should have correct name");
    FATP_ASSERT_GT(results[0].stats.samples, std::size_t(0), "Should have samples");

    return true;
}

// ============================================================================
// Format Time Tests
// ============================================================================

FATP_TEST_CASE(format_time_nanoseconds)
{
    using namespace fat_p::bench;

    std::string result = formatTime(500.0);
    FATP_ASSERT_TRUE(result.find("ns") != std::string::npos, "Should format as nanoseconds");

    return true;
}

FATP_TEST_CASE(format_time_microseconds)
{
    using namespace fat_p::bench;

    std::string result = formatTime(5000.0);
    FATP_ASSERT_TRUE(result.find("us") != std::string::npos ||
                result.find("µs") != std::string::npos,
                "Should format as microseconds");

    return true;
}

FATP_TEST_CASE(format_time_milliseconds)
{
    using namespace fat_p::bench;

    std::string result = formatTime(5000000.0);
    FATP_ASSERT_TRUE(result.find("ms") != std::string::npos, "Should format as milliseconds");

    return true;
}

FATP_TEST_CASE(format_time_seconds)
{
    using namespace fat_p::bench;

    std::string result = formatTime(5000000000.0);
    FATP_ASSERT_TRUE(result.find("s") != std::string::npos, "Should format as seconds");

    return true;
}

// ============================================================================
// nsPerOp Tests
// ============================================================================

FATP_TEST_CASE(ns_per_op_basic)
{
    using namespace fat_p::bench;

    double result = nsPerOp(1000.0, 10);
    FATP_ASSERT_CLOSE(result, 100.0, "1000ns / 10 ops = 100 ns/op");

    return true;
}

FATP_TEST_CASE(ns_per_op_zero_ops)
{
    using namespace fat_p::bench;

    double result = nsPerOp(1000.0, 0);
    FATP_ASSERT_CLOSE(result, 0.0, "Should return 0 for 0 ops (avoid divide by zero)");

    return true;
}

// ============================================================================
// IAdapter Tests
// ============================================================================

FATP_TEST_CASE(adapter_interface)
{
    using namespace fat_p::bench;

    class TestAdapter : public IAdapter
    {
        bool mSetupCalled = false;
        bool mTeardownCalled = false;

    public:
        const char* name() const override { return "TestAdapter"; }

        void setup(std::size_t) override { mSetupCalled = true; }
        void teardown() override { mTeardownCalled = true; }

        bool wasSetupCalled() const { return mSetupCalled; }
        bool wasTeardownCalled() const { return mTeardownCalled; }
    };

    TestAdapter adapter;

    FATP_ASSERT_EQ(std::string(adapter.name()), std::string("TestAdapter"),
              "Adapter should return correct name");

    adapter.setup(100);
    FATP_ASSERT_TRUE(adapter.wasSetupCalled(), "setup() should have been called");

    adapter.teardown();
    FATP_ASSERT_TRUE(adapter.wasTeardownCalled(), "teardown() should have been called");

    return true;
}

// ============================================================================
// CPU Frequency Info Tests
// ============================================================================

FATP_TEST_CASE(cpu_freq_info_basic)
{
    using namespace fat_p::bench;

    CpuFreqInfo info = capture_cpu_frequency();

    // We can't assert specific values since they're platform-dependent,
    // but we can verify the struct is usable
    (void)info.throttle_percentage();
    (void)info.has_reliable_detection();
    (void)info.is_throttled();
    (void)info.is_turbo();
    (void)info.ref_label();

    return true;
}

FATP_TEST_CASE(cpu_freq_throttle_calculation)
{
    using namespace fat_p::bench;

    CpuFreqInfo info;
    info.mRefFreqMHz = 3000.0;
    info.mCurrentFreqMHz = 2400.0;
    info.mRefIsMax = false;
    info.mCurrentIsEstimated = false;

    double throttle = info.throttle_percentage();
    FATP_ASSERT_CLOSE_EPS(throttle, 20.0, 0.1, "Should be 20% throttled (2400/3000)");

    return true;
}

// ============================================================================
// Output Format Tests
// ============================================================================

FATP_TEST_CASE(print_section_header)
{
    using namespace fat_p::bench;

    std::ostringstream oss;
    printSectionHeader(oss, "MY SECTION");

    std::string output = oss.str();
    FATP_ASSERT_TRUE(output.find("MY SECTION") != std::string::npos,
                "Section header should contain section name");
    FATP_ASSERT_TRUE(output.find("===") != std::string::npos,
                "Section header should have separator lines");

    return true;
}

FATP_TEST_CASE(print_contract)
{
    using namespace fat_p::bench;

    std::ostringstream oss;
    printContract(oss, "Test contract message");

    std::string output = oss.str();
    FATP_ASSERT_TRUE(output.find("Contract:") != std::string::npos,
                "Should contain Contract: label");
    FATP_ASSERT_TRUE(output.find("Test contract message") != std::string::npos,
                "Should contain the contract text");

    return true;
}

// ============================================================================
// Benchmarks (for the test infrastructure itself)
// ============================================================================

void run_benchmarks()
{
    using namespace fat_p::bench;

    std::cout << colors::cyan() << "\nBenchmarkRunner Infrastructure Benchmarks:"
              << colors::reset() << "\n";

    // Benchmark Timer overhead
    {
        Timer timer;
        const int kIterations = 100000;

        timer.start();
        for (int i = 0; i < kIterations; ++i)
        {
            Timer t;
            t.start();
            double elapsed = t.elapsedNs();
            DoNotOptimize(elapsed);
        }
        double total = timer.elapsedNs();

        std::cout << "  Timer overhead: " << formatTime(total / kIterations) << "/call\n";
    }

    // Benchmark Statistics computation
    {
        const int kSamples = 1000;
        std::vector<double> samples(kSamples);
        for (int i = 0; i < kSamples; ++i)
        {
            samples[i] = static_cast<double>(i);
        }

        Timer timer;
        const int kIterations = 1000;
        volatile double dummy = 0;

        timer.start();
        for (int i = 0; i < kIterations; ++i)
        {
            std::vector<double> copy = samples;
            Statistics stats = Statistics::compute(std::move(copy));
            dummy = stats.median;  // Prevent optimization
        }
        double total = timer.elapsedNs();

        std::cout << "  Statistics::compute(" << kSamples << " samples): "
                  << formatTime(total / kIterations) << "/call\n";
        (void)dummy;
    }

    // Benchmark SpinBarrier
    {
        SpinBarrier barrier(1);

        Timer timer;
        const int kIterations = 100000;

        timer.start();
        for (int i = 0; i < kIterations; ++i)
        {
            barrier.wait();
        }
        double total = timer.elapsedNs();

        std::cout << "  SpinBarrier::wait() (1 thread): "
                  << formatTime(total / kIterations) << "/call\n";
    }
}

} // namespace fat_p::testing::benchmarkrunner

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_FatPBenchmarkRunner()
{
    FATP_PRINT_HEADER(BENCHMARK RUNNER)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Timer tests
    out << colors::blue() << "--- Timer Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, timer_basic);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, timer_multiple_reads);

    // Statistics tests
    out << "\n" << colors::blue() << "--- Statistics Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, statistics_basic);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, statistics_median_even);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, statistics_stddev);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, statistics_percentiles);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, statistics_ci95);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, statistics_single_sample);

    // Config tests
    out << "\n" << colors::blue() << "--- BenchConfig Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, config_defaults);

    // DoNotOptimize tests
    out << "\n" << colors::blue() << "--- DoNotOptimize Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, do_not_optimize_compiles);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, prevent_opt_compiles);

    // SpinBarrier tests
    out << "\n" << colors::blue() << "--- SpinBarrier Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, spin_barrier_single_thread);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, spin_barrier_multi_thread);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, spin_barrier_reuse);

    // BenchmarkRunner tests
    out << "\n" << colors::blue() << "--- BenchmarkRunner Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, runner_creation);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, runner_section_and_contract);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, runner_add_benchmark);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, runner_results);

    // Format tests
    out << "\n" << colors::blue() << "--- Format Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, format_time_nanoseconds);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, format_time_microseconds);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, format_time_milliseconds);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, format_time_seconds);

    // nsPerOp tests
    out << "\n" << colors::blue() << "--- nsPerOp Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, ns_per_op_basic);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, ns_per_op_zero_ops);

    // Adapter tests
    out << "\n" << colors::blue() << "--- IAdapter Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, adapter_interface);

    // CPU Frequency tests
    out << "\n" << colors::blue() << "--- CPU Frequency Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, cpu_freq_info_basic);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, cpu_freq_throttle_calculation);

    // Output format tests
    out << "\n" << colors::blue() << "--- Output Format Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, benchmarkrunner, print_section_header);
    FATP_RUN_TEST_NS(runner, benchmarkrunner, print_contract);

    // Run benchmarks in release builds
#ifdef NDEBUG
    benchmarkrunner::run_benchmarks();
#else
    out << "\n[Debug build - skipping benchmarks]\n";
#endif

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_BenchmarkRunner() ? 0 : 1;
}
#endif
