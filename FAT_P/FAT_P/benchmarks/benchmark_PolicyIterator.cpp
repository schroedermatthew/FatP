/**
 * @file benchmark_PolicyIterator.cpp
 * @brief Performance benchmarks for PolicyIterator vs raw loops and std algorithms.
 *
 * @layer Application
 *
 * Benchmarks:
 *   1. Standard iteration vs raw pointer
 *   2. Stride policies (2, 4, 8) vs manual stride loops
 *   3. Filter policy vs manual predicate loop
 *   4. Transform policy vs manual transform
 *   5. TensorStridePolicy vs manual multi-dim access
 *   6. Multiple data sizes (L1, L2, L3, RAM)
 *
 * Compile (minimal):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_PolicyIterator.cpp -o bench
 *
 * Compile (with sanitizers for debugging):
 *   g++ -std=c++17 -O2 -g -fsanitize=address,undefined benchmark_PolicyIterator.cpp -o bench
 *
 * Windows (MSVC):
 *   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_PolicyIterator.cpp
 *
 * Environment variables (see FatPBenchmarkRunner.h):
 *   FATP_BENCH_WARMUP_RUNS   - Warmup batches (default: 3)
 *   FATP_BENCH_BATCHES       - Measured batches (default: 50)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_MIN_BATCH_MS  - Minimum batch duration (default: 50)
 *   FATP_BENCH_VERBOSE_STATS - Print raw samples (default: 0)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (optional)
 *   FATP_BENCH_NO_SCOPE      - Disable priority/affinity (Windows)
 *   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
 */

#include "FatPBenchmarkRunner.h"
#include "PolicyIterator.h"
#include "TensorStridePolicy.h"

#include <numeric>

using namespace fat_p::bench;
using namespace fat_p::iterator;

// ============================================================================
// Data Generation
// ============================================================================

inline std::vector<int64_t> generateData(std::size_t n, std::uint64_t seed)
{
    std::vector<int64_t> data(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int64_t> dist(1, 1000000);
    for (std::size_t i = 0; i < n; ++i)
    {
        data[i] = dist(rng);
    }
    return data;
}

// ============================================================================
// Benchmark: Standard Iteration vs Raw Pointer
// ============================================================================

void benchStandardVsRawPointer(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "STANDARD ITERATION VS RAW POINTER");
    printContract(std::cout, "Sequential sum accumulation, no predicate/transform");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t N = 1000000;
    auto data = generateData(N, cfg.seed);

    std::vector<double> rawTimes, policyTimes;

    // Correctness check
    int64_t expectedSum = 0;
    for (std::size_t i = 0; i < N; ++i) expectedSum += data[i];

    // Warmup + Measured runs
    for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // Raw pointer
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (int64_t* ptr = data.data(); ptr < data.data() + N; ++ptr)
            {
                sum += *ptr;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) rawTimes.push_back(elapsed / static_cast<double>(N));
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: raw pointer sum\n";
                return;
            }
        }

        // PolicyIterator
        {
            int64_t sum = 0;
            PolicyIterator<int64_t> begin(data.data(), data.data() + N);
            PolicyIterator<int64_t> end(data.data() + N, data.data() + N);
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) policyTimes.push_back(elapsed / static_cast<double>(N));
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: PolicyIterator sum\n";
                return;
            }
        }
    }

    auto rawStats = Statistics::compute(std::move(rawTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));

    std::cout << "  Results (ns/element):\n";
    rawStats.printComparison(std::cout, "Raw pointer", "ns/op");
    policyStats.printComparison(std::cout, "PolicyIterator<Standard>", "ns/op");

    double ratio = policyStats.median / rawStats.median;
    std::cout << "\n  Overhead ratio: " << std::fixed << std::setprecision(2) << ratio << "x\n";
}

// ============================================================================
// Benchmark: Stride Policies vs Manual Loops
// ============================================================================

void benchStrideVsManual(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "STRIDE POLICIES VS MANUAL LOOPS");
    printContract(std::cout, "Sum with fixed compile-time stride, bounds clamping included");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t N = 1000000;
    auto data = generateData(N, cfg.seed);

    // Test stride 2, 4, 8
    for (int stride : {2, 4, 8})
    {
        printSubheader(std::cout, "Stride " + std::to_string(stride));

        std::vector<double> manualTimes, policyTimes;

        // Expected count
        std::size_t expectedIters = (N + static_cast<std::size_t>(stride) - 1) /
                                     static_cast<std::size_t>(stride);

        for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
        {
            bool measured = (run >= cfg.warmupRuns);

            // Manual stride loop
            {
                int64_t sum = 0;
                Timer t;
                t.start();
                for (std::size_t i = 0; i < N; i += static_cast<std::size_t>(stride))
                {
                    sum += data[i];
                }
                double elapsed = t.elapsedNs();
                DoNotOptimize(sum);
                if (measured) manualTimes.push_back(elapsed / static_cast<double>(expectedIters));
            }

            // PolicyIterator with stride
            {
                int64_t sum = 0;
                Timer t;
                t.start();

                if (stride == 2)
                {
                    PolicyIterator<int64_t, StridePolicy<int64_t, 2>> begin(data.data(), data.data() + N);
                    PolicyIterator<int64_t, StridePolicy<int64_t, 2>> end(data.data() + N, data.data() + N);
                    for (auto it = begin; it != end; ++it) sum += *it;
                }
                else if (stride == 4)
                {
                    PolicyIterator<int64_t, StridePolicy<int64_t, 4>> begin(data.data(), data.data() + N);
                    PolicyIterator<int64_t, StridePolicy<int64_t, 4>> end(data.data() + N, data.data() + N);
                    for (auto it = begin; it != end; ++it) sum += *it;
                }
                else
                {
                    PolicyIterator<int64_t, StridePolicy<int64_t, 8>> begin(data.data(), data.data() + N);
                    PolicyIterator<int64_t, StridePolicy<int64_t, 8>> end(data.data() + N, data.data() + N);
                    for (auto it = begin; it != end; ++it) sum += *it;
                }

                double elapsed = t.elapsedNs();
                DoNotOptimize(sum);
                if (measured) policyTimes.push_back(elapsed / static_cast<double>(expectedIters));
            }
        }

        auto manualStats = Statistics::compute(std::move(manualTimes));
        auto policyStats = Statistics::compute(std::move(policyTimes));

        manualStats.printComparison(std::cout, "Manual loop", "ns/op");
        policyStats.printComparison(std::cout, "PolicyIterator<Stride>", "ns/op");

        double ratio = policyStats.median / manualStats.median;
        std::cout << "    Overhead ratio: " << std::fixed << std::setprecision(2) << ratio << "x\n";
    }
}

// ============================================================================
// Benchmark: Filter Policy vs Manual Predicate Loop
// ============================================================================

void benchFilterVsManual(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "FILTER POLICY VS MANUAL PREDICATE LOOP");
    printContract(std::cout, "Sum of even values only, predicate evaluation cost included");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t N = 1000000;
    auto data = generateData(N, cfg.seed);

    auto isEven = [](const int64_t& v) { return v % 2 == 0; };

    std::vector<double> manualTimes, policyTimes;

    // Expected values
    int64_t expectedSum = 0;
    std::size_t matchCount = 0;
    for (std::size_t i = 0; i < N; ++i)
    {
        if (isEven(data[i]))
        {
            expectedSum += data[i];
            ++matchCount;
        }
    }

    std::cout << "  Match ratio: " << std::fixed << std::setprecision(1)
              << (100.0 * static_cast<double>(matchCount) / static_cast<double>(N)) << "%\n\n";

    for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // Manual loop
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t i = 0; i < N; ++i)
            {
                if (isEven(data[i])) sum += data[i];
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) manualTimes.push_back(elapsed / static_cast<double>(N));
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: manual filter sum\n";
                return;
            }
        }

        // PolicyIterator with filter
        {
            using Policy = FilterPolicy<int64_t, decltype(isEven)>;
            PolicyIterator<int64_t, Policy> begin(data.data(), data.data() + N, Policy{}, isEven);
            PolicyIterator<int64_t, Policy> end(data.data() + N, data.data() + N, Policy{}, isEven);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) policyTimes.push_back(elapsed / static_cast<double>(N));
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: PolicyIterator filter sum\n";
                return;
            }
        }
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));

    std::cout << "  Results (ns/element scanned):\n";
    manualStats.printComparison(std::cout, "Manual if-check loop", "ns/op");
    policyStats.printComparison(std::cout, "PolicyIterator<Filter>", "ns/op");

    double ratio = policyStats.median / manualStats.median;
    std::cout << "\n  Overhead ratio: " << std::fixed << std::setprecision(2) << ratio << "x\n";
}

// ============================================================================
// Benchmark: Transform Policy vs Manual Transform
// ============================================================================

void benchTransformVsManual(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "TRANSFORM POLICY VS MANUAL TRANSFORM");
    printContract(std::cout, "Sum of doubled values, transform cost included");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t N = 1000000;
    auto data = generateData(N, cfg.seed);

    auto doubler = [](const int64_t& v) -> int64_t { return v * 2; };

    std::vector<double> manualTimes, policyTimes;

    // Expected sum
    int64_t expectedSum = 0;
    for (std::size_t i = 0; i < N; ++i) expectedSum += data[i] * 2;

    for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // Manual loop
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t i = 0; i < N; ++i)
            {
                sum += doubler(data[i]);
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) manualTimes.push_back(elapsed / static_cast<double>(N));
        }

        // PolicyIterator with transform
        {
            using Policy = TransformPolicy<int64_t, decltype(doubler)>;
            PolicyIterator<int64_t, Policy> begin(data.data(), data.data() + N, Policy{}, doubler);
            PolicyIterator<int64_t, Policy> end(data.data() + N, data.data() + N, Policy{}, doubler);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) policyTimes.push_back(elapsed / static_cast<double>(N));
        }
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));

    std::cout << "  Results (ns/element):\n";
    manualStats.printComparison(std::cout, "Manual transform loop", "ns/op");
    policyStats.printComparison(std::cout, "PolicyIterator<Transform>", "ns/op");

    double ratio = policyStats.median / manualStats.median;
    std::cout << "\n  Overhead ratio: " << std::fixed << std::setprecision(2) << ratio << "x\n";
}

// ============================================================================
// Benchmark: TensorStridePolicy Row Access
// ============================================================================

void benchTensorStride(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "TENSOR STRIDE POLICY VS MANUAL MULTI-DIM ACCESS");
    printContract(std::cout, "Row-major matrix row iteration, stride = columns");
    print_cpu_context(std::cout, "Start");

    // Create a 1000 x 1000 matrix (1M elements)
    constexpr std::size_t rows = 1000;
    constexpr std::size_t cols = 1000;
    auto data = generateData(rows * cols, cfg.seed);

    std::cout << "  Matrix: " << rows << " x " << cols << " (" << rows * cols << " elements)\n\n";

    std::vector<double> manualTimes, policyTimes;

    // Sum first column (every cols-th element)
    int64_t expectedSum = 0;
    for (std::size_t r = 0; r < rows; ++r) expectedSum += data[r * cols];

    for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // Manual row stride
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t r = 0; r < rows; ++r)
            {
                sum += data[r * cols];
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) manualTimes.push_back(elapsed / static_cast<double>(rows));
        }

        // TensorStridePolicy - iterate over first column (1000 elements, stride 1000)
        {
            // Shape: {rows} elements to iterate
            // Stride: {cols} elements between each
            TensorStridePolicy<int64_t> policy(
                {rows},
                {static_cast<std::ptrdiff_t>(cols)}
            );
            PolicyIterator<int64_t, TensorStridePolicy<int64_t>> it(
                data.data(), data.data() + rows * cols, policy);
            auto endIt = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::makeEnd(
                data.data(), data.data() + rows * cols, policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured) policyTimes.push_back(elapsed / static_cast<double>(rows));
        }
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));

    std::cout << "  Results (ns/row accessed):\n";
    manualStats.printComparison(std::cout, "Manual r*cols indexing", "ns/op");
    policyStats.printComparison(std::cout, "TensorStridePolicy", "ns/op");

    double ratio = policyStats.median / manualStats.median;
    std::cout << "\n  Overhead ratio: " << std::fixed << std::setprecision(2) << ratio << "x\n";
}

// ============================================================================
// Benchmark: Data Size Scaling (Cache Effects)
// ============================================================================

void benchSizeScaling(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "SIZE SCALING (CACHE EFFECTS)");
    printContract(std::cout, "Standard iteration sum at different data sizes");
    print_cpu_context(std::cout, "Start");

    // Sizes targeting L1 (32KB), L2 (256KB), L3 (8MB), RAM
    struct SizeCase { const char* name; std::size_t n; };
    SizeCase sizes[] = {
        {"L1 (4K)",      4 * 1024 / sizeof(int64_t)},
        {"L2 (32K)",    32 * 1024 / sizeof(int64_t)},
        {"L3 (512K)",  512 * 1024 / sizeof(int64_t)},
        {"RAM (8M)",     8 * 1024 * 1024 / sizeof(int64_t)}
    };

    for (const auto& sz : sizes)
    {
        printSubheader(std::cout, std::string(sz.name) + " (" + std::to_string(sz.n) + " elements)");

        auto data = generateData(sz.n, cfg.seed);

        std::vector<double> rawTimes, policyTimes;

        for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
        {
            bool measured = (run >= cfg.warmupRuns);

            // Raw pointer
            {
                int64_t sum = 0;
                Timer t;
                t.start();
                for (int64_t* ptr = data.data(); ptr < data.data() + sz.n; ++ptr)
                {
                    sum += *ptr;
                }
                double elapsed = t.elapsedNs();
                DoNotOptimize(sum);
                if (measured) rawTimes.push_back(elapsed / static_cast<double>(sz.n));
            }

            // PolicyIterator
            {
                int64_t sum = 0;
                PolicyIterator<int64_t> begin(data.data(), data.data() + sz.n);
                PolicyIterator<int64_t> end(data.data() + sz.n, data.data() + sz.n);
                Timer t;
                t.start();
                for (auto it = begin; it != end; ++it)
                {
                    sum += *it;
                }
                double elapsed = t.elapsedNs();
                DoNotOptimize(sum);
                if (measured) policyTimes.push_back(elapsed / static_cast<double>(sz.n));
            }
        }

        auto rawStats = Statistics::compute(std::move(rawTimes));
        auto policyStats = Statistics::compute(std::move(policyTimes));

        rawStats.printComparison(std::cout, "Raw pointer", "ns/op");
        policyStats.printComparison(std::cout, "PolicyIterator", "ns/op");

        double ratio = policyStats.median / rawStats.median;
        std::cout << "    Overhead ratio: " << std::fixed << std::setprecision(2) << ratio << "x\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << R"(
================================================================================
  PolicyIterator Benchmark Suite
  Fat-P Library - Policy-Based Iterator Performance
================================================================================
)";

    auto cfg = BenchConfig::fromEnv();

    std::cout << "\nConfiguration:\n";
    cfg.print(std::cout);

    std::cout << "\nPlatform: " << getPlatformString()
              << ", Compiler: " << getCompilerString() << "\n";
    std::cout << "Timestamp: " << getTimestampIso() << "\n";

    // Print initial CPU state
    std::cout << "\nInitial CPU state:\n";
    print_cpu_context(std::cout, "Benchmark start");

    // Run benchmarks with cooldown between sections
    benchStandardVsRawPointer(cfg);
    if (!cfg.noCooldown) cooldownDelay(cfg.cooldownSectionMs, "section transition");

    benchStrideVsManual(cfg);
    if (!cfg.noCooldown) cooldownDelay(cfg.cooldownSectionMs, "section transition");

    benchFilterVsManual(cfg);
    if (!cfg.noCooldown) cooldownDelay(cfg.cooldownSectionMs, "section transition");

    benchTransformVsManual(cfg);
    if (!cfg.noCooldown) cooldownDelay(cfg.cooldownSectionMs, "section transition");

    benchTensorStride(cfg);
    if (!cfg.noCooldown) cooldownDelay(cfg.cooldownSectionMs, "section transition");

    benchSizeScaling(cfg);

    // Summary
    std::cout << "\n";
    printSectionHeader(std::cout, "BENCHMARK COMPLETE");
    print_cpu_context(std::cout, "Final state");

    return 0;
}
