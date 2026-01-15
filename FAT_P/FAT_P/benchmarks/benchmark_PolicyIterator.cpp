/**
 * @file benchmark_PolicyIterator.cpp
 * @brief Performance benchmarks for PolicyIterator vs raw loops, Boost, range-v3, Eigen, xtensor.
 *
 * @layer Application
 *
 * Benchmarks:
 *   1. Standard iteration vs raw pointer
 *   2. Stride policies vs manual stride loops (+ range-v3::views::stride)
 *   3. Filter policy vs manual loop (+ Boost.filter_iterator, range-v3::views::filter)
 *   4. Transform policy vs manual (+ Boost.transform_iterator, range-v3::views::transform)
 *   5. TensorStridePolicy vs manual multi-dim (+ Eigen, xtensor, Boost.MultiArray)
 *   6. Multiple data sizes (L1, L2, L3, RAM)
 *
 * Library comparisons are AUTO-DETECTED via __has_include:
 *   - Boost.Iterator:     <boost/iterator/filter_iterator.hpp>
 *   - Boost.MultiArray:   <boost/multi_array.hpp>
 *   - range-v3:           <range/v3/view/filter.hpp>
 *   - Eigen:              <Eigen/Dense>
 *   - xtensor:            <xtensor/xarray.hpp>
 *
 * Compile (basic - auto-detects available libraries):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_PolicyIterator.cpp -o bench
 *
 * With vcpkg (headers auto-discovered):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native \
 *       -I/path/to/vcpkg/installed/x64-linux/include \
 *       benchmark_PolicyIterator.cpp -o bench
 *
 * Environment variables (see FatPBenchmarkRunner.h):
 *   FATP_BENCH_WARMUP_RUNS   - Warmup batches (default: 3)
 *   FATP_BENCH_BATCHES       - Measured batches (default: 50)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_MIN_BATCH_MS  - Minimum batch duration (default: 50)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (optional)
 */
/*
FATP_META:
  meta_version: 1
  component: PolicyIterator
  file_role: benchmark
  path: benchmarks/benchmark_PolicyIterator.cpp
  namespace: fat_p
  summary: "Benchmarks for PolicyIterator."
  related:
    docs_search: "PolicyIterator"
    headers:
      - fat_p/FatPBenchmarkRunner.h
      - fat_p/PolicyIterator.h
      - fat_p/TensorStridePolicy.h
      - fat_p/TensorIteration.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 13
    defines_unprefixed: 13
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include "FatPBenchmarkRunner.h"
#include "PolicyIterator.h"
#include "TensorIteration.h"
#include "TensorStridePolicy.h"

#include <numeric>

// ============================================================================
// Optional Library Detection (via __has_include)
// ============================================================================

#if __has_include(<boost/iterator/filter_iterator.hpp>)
#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#define HAS_BOOST_ITERATOR 1
#else
#define HAS_BOOST_ITERATOR 0
#endif

#if __has_include(<range/v3/view/filter.hpp>)
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/stride.hpp>
#include <range/v3/view/transform.hpp>
#define HAS_RANGE_V3 1
#else
#define HAS_RANGE_V3 0
#endif

// Eigen: check both common paths (vcpkg uses eigen3/ prefix)
#if __has_include(<Eigen/Dense>)
#include <Eigen/Dense>
#define HAS_EIGEN 1
#elif __has_include(<eigen3/Eigen/Dense>)
#include <eigen3/Eigen/Dense>
#define HAS_EIGEN 1
#else
#define HAS_EIGEN 0
#endif

// xtensor: dynamic N-D tensor library (requires C++20 for xtensor 0.27+)
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#if __has_include(<xtensor/xarray.hpp>)
#include <xtensor/xadapt.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xstrided_view.hpp>
#define HAS_XTENSOR 1
#elif __has_include(<xtensor/containers/xarray.hpp>)
#include <xtensor/containers/xadapt.hpp>
#include <xtensor/containers/xarray.hpp>
#include <xtensor/views/xstrided_view.hpp>
#define HAS_XTENSOR 1
#else
#define HAS_XTENSOR 0
#endif
#else
#define HAS_XTENSOR 0
#endif

// Boost.MultiArray
#if __has_include(<boost/multi_array.hpp>)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // Suppress deprecated 'assign' warning in Boost.MultiArray
#endif
#include <boost/multi_array.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#define HAS_BOOST_MULTIARRAY 1
#else
#define HAS_BOOST_MULTIARRAY 0
#endif

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
    for (std::size_t i = 0; i < N; ++i)
    {
        expectedSum += data[i];
    }

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
            if (measured)
            {
                rawTimes.push_back(elapsed / static_cast<double>(N));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: raw pointer sum\n";
                return;
            }
        }

        // PolicyIterator
        {
            int64_t sum = 0;
            auto begin = PolicyIterator<int64_t>::begin(data.data(), data.data() + N);
            auto end = PolicyIterator<int64_t>::end(data.data(), data.data() + N);
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                policyTimes.push_back(elapsed / static_cast<double>(N));
            }
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
// Benchmark: Stride Policies vs Manual Loops (+ range-v3)
// ============================================================================

void benchStrideVsManual(const BenchConfig& cfg)
{
    printSectionHeader(std::cout,
                       "STRIDE POLICIES VS MANUAL LOOPS"
#if HAS_RANGE_V3
                       " + RANGE-V3"
#endif
    );
    printContract(std::cout, "Sum with fixed compile-time stride, bounds clamping included");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t N = 1000000;
    auto data = generateData(N, cfg.seed);

    // Test stride 2, 4, 8
    for (int stride : {2, 4, 8})
    {
        printSubheader(std::cout, "Stride " + std::to_string(stride));

        std::vector<double> manualTimes, policyTimes;
#if HAS_RANGE_V3
        std::vector<double> rangesTimes;
#endif

        std::size_t expectedIters = (N + static_cast<std::size_t>(stride) - 1) / static_cast<std::size_t>(stride);

        // Expected sum for correctness
        int64_t expectedSum = 0;
        for (std::size_t i = 0; i < N; i += static_cast<std::size_t>(stride))
        {
            expectedSum += data[i];
        }

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
                if (measured)
                {
                    manualTimes.push_back(elapsed / static_cast<double>(expectedIters));
                }
            }

            // PolicyIterator with stride
            {
                int64_t sum = 0;
                Timer t;
                t.start();

                if (stride == 2)
                {
                    using Iter = PolicyIterator<int64_t, StridePolicy<int64_t, 2>>;
                    auto begin = Iter::begin(data.data(), data.data() + N);
                    auto end = Iter::end(data.data(), data.data() + N);
                    for (auto it = begin; it != end; ++it)
                    {
                        sum += *it;
                    }
                }
                else if (stride == 4)
                {
                    using Iter = PolicyIterator<int64_t, StridePolicy<int64_t, 4>>;
                    auto begin = Iter::begin(data.data(), data.data() + N);
                    auto end = Iter::end(data.data(), data.data() + N);
                    for (auto it = begin; it != end; ++it)
                    {
                        sum += *it;
                    }
                }
                else
                {
                    using Iter = PolicyIterator<int64_t, StridePolicy<int64_t, 8>>;
                    auto begin = Iter::begin(data.data(), data.data() + N);
                    auto end = Iter::end(data.data(), data.data() + N);
                    for (auto it = begin; it != end; ++it)
                    {
                        sum += *it;
                    }
                }

                double elapsed = t.elapsedNs();
                DoNotOptimize(sum);
                if (measured)
                {
                    policyTimes.push_back(elapsed / static_cast<double>(expectedIters));
                }
            }

#if HAS_RANGE_V3
            // range-v3 views::stride
            {
                int64_t sum = 0;
                Timer t;
                t.start();
                for (auto val : data | ranges::views::stride(stride))
                {
                    sum += val;
                }
                double elapsed = t.elapsedNs();
                DoNotOptimize(sum);
                if (measured)
                {
                    rangesTimes.push_back(elapsed / static_cast<double>(expectedIters));
                }
                if (run == 0 && sum != expectedSum)
                {
                    std::cerr << "CORRECTNESS FAILURE: range-v3 stride sum\n";
                    return;
                }
            }
#endif
        }

        auto manualStats = Statistics::compute(std::move(manualTimes));
        auto policyStats = Statistics::compute(std::move(policyTimes));

        manualStats.printComparison(std::cout, "Manual loop", "ns/op");
        policyStats.printComparison(std::cout, "PolicyIterator<Stride>", "ns/op");

#if HAS_RANGE_V3
        auto rangesStats = Statistics::compute(std::move(rangesTimes));
        rangesStats.printComparison(std::cout, "range-v3::views::stride", "ns/op");

        std::cout << "    vs Manual: Policy " << std::fixed << std::setprecision(2)
                  << (policyStats.median / manualStats.median) << "x, range-v3 "
                  << (rangesStats.median / manualStats.median) << "x\n";
#else
        double ratio = policyStats.median / manualStats.median;
        std::cout << "    Overhead ratio: " << std::fixed << std::setprecision(2) << ratio << "x\n";
#endif
    }
}

// ============================================================================
// Benchmark: Filter Policy vs Manual Loop (+ Boost + range-v3)
// ============================================================================

void benchFilterVsManual(const BenchConfig& cfg)
{
    printSectionHeader(std::cout,
                       "FILTER POLICY VS MANUAL LOOP"
#if HAS_BOOST_ITERATOR
                       " + BOOST"
#endif
#if HAS_RANGE_V3
                       " + RANGE-V3"
#endif
    );
    printContract(std::cout, "Sum of even values only, predicate evaluation cost included");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t N = 1000000;
    auto data = generateData(N, cfg.seed);

    auto isEven = [](const int64_t& v) {
        return v % 2 == 0;
    };

    std::vector<double> manualTimes, policyTimes;
#if HAS_BOOST_ITERATOR
    std::vector<double> boostTimes;
#endif
#if HAS_RANGE_V3
    std::vector<double> rangesTimes;
#endif

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
                if (isEven(data[i]))
                {
                    sum += data[i];
                }
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                manualTimes.push_back(elapsed / static_cast<double>(N));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: manual filter sum\n";
                return;
            }
        }

        // PolicyIterator with filter
        {
            using Policy = FilterPolicy<int64_t, decltype(isEven)>;
            auto begin = PolicyIterator<int64_t, Policy>::begin(data.data(), data.data() + N, Policy{}, isEven);
            auto end = PolicyIterator<int64_t, Policy>::end(data.data(), data.data() + N, Policy{}, isEven);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                policyTimes.push_back(elapsed / static_cast<double>(N));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: PolicyIterator filter sum\n";
                return;
            }
        }

#if HAS_BOOST_ITERATOR
        // Boost.filter_iterator
        {
            auto begin = boost::make_filter_iterator(isEven, data.begin(), data.end());
            auto end = boost::make_filter_iterator(isEven, data.end(), data.end());

            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                boostTimes.push_back(elapsed / static_cast<double>(N));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: Boost filter sum\n";
                return;
            }
        }
#endif

#if HAS_RANGE_V3
        // range-v3 views::filter
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto val : data | ranges::views::filter(isEven))
            {
                sum += val;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                rangesTimes.push_back(elapsed / static_cast<double>(N));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: range-v3 filter sum\n";
                return;
            }
        }
#endif
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));

    std::cout << "  Results (ns/element scanned):\n";
    manualStats.printComparison(std::cout, "Manual if-check loop", "ns/op");
    policyStats.printComparison(std::cout, "PolicyIterator<Filter>", "ns/op");

#if HAS_BOOST_ITERATOR
    auto boostStats = Statistics::compute(std::move(boostTimes));
    boostStats.printComparison(std::cout, "Boost.filter_iterator", "ns/op");
#endif

#if HAS_RANGE_V3
    auto rangesStats = Statistics::compute(std::move(rangesTimes));
    rangesStats.printComparison(std::cout, "range-v3::views::filter", "ns/op");
#endif

    std::cout << "\n  vs Manual:\n";
    std::cout << "    PolicyIterator: " << std::fixed << std::setprecision(2)
              << (policyStats.median / manualStats.median) << "x\n";
#if HAS_BOOST_ITERATOR
    std::cout << "    Boost:          " << (boostStats.median / manualStats.median) << "x\n";
#endif
#if HAS_RANGE_V3
    std::cout << "    range-v3:       " << (rangesStats.median / manualStats.median) << "x\n";
#endif
}

// ============================================================================
// Benchmark: Transform Policy vs Manual Transform (+ Boost + range-v3)
// ============================================================================

void benchTransformVsManual(const BenchConfig& cfg)
{
    printSectionHeader(std::cout,
                       "TRANSFORM POLICY VS MANUAL LOOP"
#if HAS_BOOST_ITERATOR
                       " + BOOST"
#endif
#if HAS_RANGE_V3
                       " + RANGE-V3"
#endif
    );
    printContract(std::cout, "Sum of doubled values, transform cost included");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t N = 1000000;
    auto data = generateData(N, cfg.seed);

    auto doubler = [](const int64_t& v) -> int64_t {
        return v * 2;
    };

    std::vector<double> manualTimes, policyTimes;
#if HAS_BOOST_ITERATOR
    std::vector<double> boostTimes;
#endif
#if HAS_RANGE_V3
    std::vector<double> rangesTimes;
#endif

    // Expected sum
    int64_t expectedSum = 0;
    for (std::size_t i = 0; i < N; ++i)
    {
        expectedSum += data[i] * 2;
    }

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
            if (measured)
            {
                manualTimes.push_back(elapsed / static_cast<double>(N));
            }
        }

        // PolicyIterator with transform
        {
            using Policy = TransformPolicy<int64_t, decltype(doubler)>;
            auto begin = PolicyIterator<int64_t, Policy>::begin(data.data(), data.data() + N, Policy{}, doubler);
            auto end = PolicyIterator<int64_t, Policy>::end(data.data(), data.data() + N, Policy{}, doubler);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                policyTimes.push_back(elapsed / static_cast<double>(N));
            }
        }

#if HAS_BOOST_ITERATOR
        // Boost.transform_iterator
        {
            auto begin = boost::make_transform_iterator(data.begin(), doubler);
            auto end = boost::make_transform_iterator(data.end(), doubler);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto it = begin; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                boostTimes.push_back(elapsed / static_cast<double>(N));
            }
        }
#endif

#if HAS_RANGE_V3
        // range-v3 views::transform
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto val : data | ranges::views::transform(doubler))
            {
                sum += val;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                rangesTimes.push_back(elapsed / static_cast<double>(N));
            }
        }
#endif
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));

    std::cout << "  Results (ns/element):\n";
    manualStats.printComparison(std::cout, "Manual transform loop", "ns/op");
    policyStats.printComparison(std::cout, "PolicyIterator<Transform>", "ns/op");

#if HAS_BOOST_ITERATOR
    auto boostStats = Statistics::compute(std::move(boostTimes));
    boostStats.printComparison(std::cout, "Boost.transform_iterator", "ns/op");
#endif

#if HAS_RANGE_V3
    auto rangesStats = Statistics::compute(std::move(rangesTimes));
    rangesStats.printComparison(std::cout, "range-v3::views::transform", "ns/op");
#endif

    std::cout << "\n  vs Manual:\n";
    std::cout << "    PolicyIterator: " << std::fixed << std::setprecision(2)
              << (policyStats.median / manualStats.median) << "x\n";
#if HAS_BOOST_ITERATOR
    std::cout << "    Boost:          " << (boostStats.median / manualStats.median) << "x\n";
#endif
#if HAS_RANGE_V3
    std::cout << "    range-v3:       " << (rangesStats.median / manualStats.median) << "x\n";
#endif
}

// ============================================================================
// Benchmark: TensorStridePolicy vs Manual + Eigen
// ============================================================================

void benchTensorStride(const BenchConfig& cfg)
{
    printSectionHeader(std::cout,
                       "TENSOR STRIDE POLICY VS MANUAL"
#if HAS_EIGEN
                       " + EIGEN"
#endif
#if HAS_XTENSOR
                       " + XTENSOR"
#endif
#if HAS_BOOST_MULTIARRAY
                       " + BOOST"
#endif
    );
    printContract(std::cout, "Row-major matrix column iteration (strided access)");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t rows = 1000;
    constexpr std::size_t cols = 1000;
    auto data = generateData(rows * cols, cfg.seed);

    std::cout << "  Matrix: " << rows << " x " << cols << " (" << rows * cols << " elements)\n";
    std::cout << "  Task: Sum first column (stride = " << cols << ")\n\n";

    std::vector<double> manualTimes, policyTimes, stride1dTimes;
#if HAS_EIGEN
    std::vector<double> eigenTimes;
#endif
#if HAS_XTENSOR
    std::vector<double> xtensorTimes;
#endif
#if HAS_BOOST_MULTIARRAY
    std::vector<double> boostTimes;
#endif

    // Expected sum (first column)
    int64_t expectedSum = 0;
    for (std::size_t r = 0; r < rows; ++r)
    {
        expectedSum += data[r * cols];
    }

#if HAS_EIGEN
    // Create Eigen matrix (Map over existing data to avoid copy)
    Eigen::Map<Eigen::Matrix<int64_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> eigenMat(
        data.data(),
        static_cast<Eigen::Index>(rows),
        static_cast<Eigen::Index>(cols));
#endif

#if HAS_XTENSOR
    // Create xtensor xarray (dynamic shape)
    xt::xarray<int64_t> xtArr =
        xt::adapt(data.data(), rows * cols, xt::no_ownership(), std::vector<std::size_t>{rows, cols});
#endif

#if HAS_BOOST_MULTIARRAY
    // Create Boost.MultiArray (view over existing data)
    boost::const_multi_array_ref<int64_t, 2> boostArr(data.data(), boost::extents[rows][cols]);
#endif

    for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // Manual indexing
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
            if (measured)
            {
                manualTimes.push_back(elapsed / static_cast<double>(rows));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: manual column sum\n";
                return;
            }
        }

        // TensorStridePolicy
        {
            TensorStridePolicy<int64_t> policy({rows}, {static_cast<std::ptrdiff_t>(cols)});
            auto it = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::begin(data.data(),
                                                                                  data.data() + rows * cols,
                                                                                  policy);
            auto endIt = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::end(data.data(),
                                                                                   data.data() + rows * cols,
                                                                                   policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                policyTimes.push_back(elapsed / static_cast<double>(rows));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: TensorStridePolicy column sum\n";
                return;
            }
        }

        // Stride1DPolicy (lightweight specialization)
        {
            Stride1DPolicy<int64_t> policy(rows, static_cast<std::ptrdiff_t>(cols));
            auto it =
                PolicyIterator<int64_t, Stride1DPolicy<int64_t>>::begin(data.data(), data.data() + rows * cols, policy);
            auto endIt =
                PolicyIterator<int64_t, Stride1DPolicy<int64_t>>::end(data.data(), data.data() + rows * cols, policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                stride1dTimes.push_back(elapsed / static_cast<double>(rows));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: Stride1DPolicy column sum\n";
                return;
            }
        }

#if HAS_EIGEN
        // Eigen column access
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            sum = eigenMat.col(0).sum();
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                eigenTimes.push_back(elapsed / static_cast<double>(rows));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: Eigen column sum\n";
                return;
            }
        }
#endif

#if HAS_XTENSOR
        // xtensor strided_view (column 0)
        {
            auto view = xt::strided_view(xtArr, {xt::all(), 0});
            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto& val : view)
            {
                sum += val;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                xtensorTimes.push_back(elapsed / static_cast<double>(rows));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: xtensor strided_view column sum\n";
                return;
            }
        }
#endif

#if HAS_BOOST_MULTIARRAY
        // Boost.MultiArray nested iteration (column 0)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t r = 0; r < rows; ++r)
            {
                sum += boostArr[r][0];
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                boostTimes.push_back(elapsed / static_cast<double>(rows));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: Boost.MultiArray column sum\n";
                return;
            }
        }
#endif
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));
    auto stride1dStats = Statistics::compute(std::move(stride1dTimes));

    std::cout << "  Results (ns/row):\n";
    manualStats.printComparison(std::cout, "Manual r*cols indexing", "ns/op");
    policyStats.printComparison(std::cout, "TensorStridePolicy", "ns/op");
    stride1dStats.printComparison(std::cout, "Stride1DPolicy", "ns/op");

#if HAS_EIGEN
    auto eigenStats = Statistics::compute(std::move(eigenTimes));
    eigenStats.printComparison(std::cout, "Eigen::col(0).sum()", "ns/op");
#endif

#if HAS_XTENSOR
    auto xtensorStats = Statistics::compute(std::move(xtensorTimes));
    xtensorStats.printComparison(std::cout, "xtensor strided_view", "ns/op");
#endif

#if HAS_BOOST_MULTIARRAY
    auto boostStats = Statistics::compute(std::move(boostTimes));
    boostStats.printComparison(std::cout, "Boost.MultiArray [][0]", "ns/op");
#endif

    std::cout << "\n  vs Manual:\n";
    std::cout << "    TensorStridePolicy: " << std::fixed << std::setprecision(2)
              << (policyStats.median / manualStats.median) << "x\n";
    std::cout << "    Stride1DPolicy:     " << std::fixed << std::setprecision(2)
              << (stride1dStats.median / manualStats.median) << "x\n";
#if HAS_EIGEN
    std::cout << "    Eigen:              " << (eigenStats.median / manualStats.median) << "x\n";
#endif
#if HAS_XTENSOR
    std::cout << "    xtensor:            " << (xtensorStats.median / manualStats.median) << "x\n";
#endif
#if HAS_BOOST_MULTIARRAY
    std::cout << "    Boost:              " << (boostStats.median / manualStats.median) << "x\n";
#endif

#if HAS_XTENSOR
    std::cout << "\n  TensorStridePolicy vs xtensor (strided iteration):\n";
    std::cout << "    TensorStridePolicy: " << std::fixed << std::setprecision(2) << policyStats.median << " ns\n";
    std::cout << "    xtensor:            " << xtensorStats.median << " ns ("
              << (policyStats.median / xtensorStats.median) << "x)\n";
#endif
}

// ============================================================================
// Benchmark: Contiguous vs Non-Contiguous Tensor Iteration
// ============================================================================

void benchTensorContiguous(const BenchConfig& cfg)
{
    printSectionHeader(std::cout,
                       "TENSOR: CONTIGUOUS VS NON-CONTIGUOUS VS SPECIALIZED"
#if HAS_XTENSOR
                       " + XTENSOR"
#endif
#if HAS_BOOST_MULTIARRAY
                       " + BOOST"
#endif
    );
    printContract(std::cout, "Compare general TensorStridePolicy vs lightweight 1D/2D policies");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t rows = 1000;
    constexpr std::size_t cols = 1000;
    auto data = generateData(rows * cols, cfg.seed);

    std::cout << "  Matrix: " << rows << " x " << cols << " (" << rows * cols << " elements)\n\n";

    std::vector<double> manualTimes, tensorContiguousTimes, tensorStridedTimes;
    std::vector<double> stride1dTimes, stride2dTimes;
#if HAS_XTENSOR
    std::vector<double> xtensorContiguousTimes, xtensorStridedTimes;
#endif
#if HAS_BOOST_MULTIARRAY
    std::vector<double> boostContiguousTimes;
#endif

    int64_t expectedSum = 0;
    for (auto v : data)
    {
        expectedSum += v;
    }

    // Expected for column sum (stride=1000)
    int64_t expectedColSum = 0;
    for (std::size_t r = 0; r < rows; ++r)
    {
        expectedColSum += data[r * cols];
    }

#if HAS_XTENSOR
    // Create xtensor xarray
    xt::xarray<int64_t> xtArr =
        xt::adapt(data.data(), rows * cols, xt::no_ownership(), std::vector<std::size_t>{rows, cols});
#endif

#if HAS_BOOST_MULTIARRAY
    // Create Boost.MultiArray
    boost::const_multi_array_ref<int64_t, 2> boostArr(data.data(), boost::extents[rows][cols]);
#endif

    for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // Manual flat iteration (baseline)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t i = 0; i < rows * cols; ++i)
            {
                sum += data[i];
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                manualTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }

        // TensorStridePolicy - CONTIGUOUS (full matrix, row-major)
        {
            TensorStridePolicy<int64_t> policy({rows, cols});
            auto it = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::begin(data.data(),
                                                                                  data.data() + rows * cols,
                                                                                  policy);
            auto endIt = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::end(data.data(),
                                                                                   data.data() + rows * cols,
                                                                                   policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                tensorContiguousTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }

        // TensorStridePolicy - NON-CONTIGUOUS (column sum with stride)
        {
            TensorStridePolicy<int64_t> policy({rows}, {static_cast<std::ptrdiff_t>(cols)});
            auto it = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::begin(data.data(),
                                                                                  data.data() + rows * cols,
                                                                                  policy);
            auto endIt = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::end(data.data(),
                                                                                   data.data() + rows * cols,
                                                                                   policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            // Note: This visits only `rows` elements, normalize per element visited
            if (measured)
            {
                tensorStridedTimes.push_back(elapsed / static_cast<double>(rows));
            }
        }

        // Stride1DPolicy - Column sum (lightweight)
        {
            Stride1DPolicy<int64_t> policy(rows, static_cast<std::ptrdiff_t>(cols));
            auto it =
                PolicyIterator<int64_t, Stride1DPolicy<int64_t>>::begin(data.data(), data.data() + rows * cols, policy);
            auto endIt =
                PolicyIterator<int64_t, Stride1DPolicy<int64_t>>::end(data.data(), data.data() + rows * cols, policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                stride1dTimes.push_back(elapsed / static_cast<double>(rows));
            }
            if (run == 0 && sum != expectedColSum)
            {
                std::cerr << "CORRECTNESS FAILURE: Stride1DPolicy column sum\n";
                return;
            }
        }

        // Stride2DPolicy - Full matrix (lightweight)
        {
            Stride2DPolicy<int64_t> policy(rows, cols); // Row-major contiguous
            auto it =
                PolicyIterator<int64_t, Stride2DPolicy<int64_t>>::begin(data.data(), data.data() + rows * cols, policy);
            auto endIt =
                PolicyIterator<int64_t, Stride2DPolicy<int64_t>>::end(data.data(), data.data() + rows * cols, policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                stride2dTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
            if (run == 0 && sum != expectedSum)
            {
                std::cerr << "CORRECTNESS FAILURE: Stride2DPolicy full sum\n";
                return;
            }
        }

#if HAS_XTENSOR
        // xtensor - full iteration
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto& val : xtArr)
            {
                sum += val;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                xtensorContiguousTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }

        // xtensor - strided_view column 0
        {
            auto view = xt::strided_view(xtArr, {xt::all(), 0});
            int64_t sum = 0;
            Timer t;
            t.start();
            for (auto& val : view)
            {
                sum += val;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                xtensorStridedTimes.push_back(elapsed / static_cast<double>(rows));
            }
        }
#endif

#if HAS_BOOST_MULTIARRAY
        // Boost.MultiArray - data() pointer (contiguous)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            const int64_t* ptr = boostArr.data();
            const int64_t* end = ptr + boostArr.num_elements();
            for (; ptr != end; ++ptr)
            {
                sum += *ptr;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                boostContiguousTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }
#endif
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto tensorContiguousStats = Statistics::compute(std::move(tensorContiguousTimes));
    auto tensorStridedStats = Statistics::compute(std::move(tensorStridedTimes));
    auto stride1dStats = Statistics::compute(std::move(stride1dTimes));
    auto stride2dStats = Statistics::compute(std::move(stride2dTimes));

    std::cout << "  Full matrix iteration (ns/element):\n";
    manualStats.printComparison(std::cout, "Manual flat loop", "ns/op");
    tensorContiguousStats.printComparison(std::cout, "TensorStridePolicy (general)", "ns/op");
    stride2dStats.printComparison(std::cout, "Stride2DPolicy (specialized)", "ns/op");

#if HAS_XTENSOR
    auto xtensorContiguousStats = Statistics::compute(std::move(xtensorContiguousTimes));
    xtensorContiguousStats.printComparison(std::cout, "xtensor iterator", "ns/op");
#endif

#if HAS_BOOST_MULTIARRAY
    auto boostContiguousStats = Statistics::compute(std::move(boostContiguousTimes));
    boostContiguousStats.printComparison(std::cout, "Boost.MultiArray data()", "ns/op");
#endif

    std::cout << "\n  Column iteration (ns/row, stride=" << cols << "):\n";
    tensorStridedStats.printComparison(std::cout, "TensorStridePolicy (general)", "ns/op");
    stride1dStats.printComparison(std::cout, "Stride1DPolicy (specialized)", "ns/op");

#if HAS_XTENSOR
    auto xtensorStridedStats = Statistics::compute(std::move(xtensorStridedTimes));
    xtensorStridedStats.printComparison(std::cout, "xtensor strided_view", "ns/op");
#endif

    std::cout << "\n  vs Manual (full matrix):\n";
    std::cout << "    TensorStridePolicy: " << std::fixed << std::setprecision(2)
              << (tensorContiguousStats.median / manualStats.median) << "x\n";
    std::cout << "    Stride2DPolicy:     " << std::fixed << std::setprecision(2)
              << (stride2dStats.median / manualStats.median) << "x\n";
#if HAS_XTENSOR
    std::cout << "    xtensor:            " << std::fixed << std::setprecision(2)
              << (xtensorContiguousStats.median / manualStats.median) << "x\n";
#endif
#if HAS_BOOST_MULTIARRAY
    std::cout << "    Boost:              " << std::fixed << std::setprecision(2)
              << (boostContiguousStats.median / manualStats.median) << "x\n";
#endif

    std::cout << "\n  Stride1D vs TensorStride (column iteration):\n";
    std::cout << "    TensorStridePolicy: " << std::fixed << std::setprecision(2) << tensorStridedStats.median
              << " ns/row\n";
    std::cout << "    Stride1DPolicy:     " << std::fixed << std::setprecision(2) << stride1dStats.median << " ns/row ("
              << (stride1dStats.median / tensorStridedStats.median) << "x)\n";

#if HAS_XTENSOR
    std::cout << "\n  TensorStridePolicy vs xtensor (strided iteration):\n";
    std::cout << "    TensorStridePolicy: " << tensorStridedStats.median << " ns\n";
    std::cout << "    xtensor:            " << xtensorStridedStats.median << " ns ("
              << (tensorStridedStats.median / xtensorStridedStats.median) << "x)\n";
#endif
}

// ============================================================================
// Benchmark: 2D Matrix Full Iteration (+ Eigen)
// ============================================================================

#if HAS_EIGEN
void benchMatrixIteration(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "2D MATRIX FULL ITERATION: POLICIES VS EIGEN");
    printContract(std::cout, "Sum all elements of 1000x1000 row-major matrix");
    print_cpu_context(std::cout, "Start");

    constexpr std::size_t rows = 1000;
    constexpr std::size_t cols = 1000;
    auto data = generateData(rows * cols, cfg.seed);

    std::cout << "  Matrix: " << rows << " x " << cols << "\n\n";

    std::vector<double> manualTimes, policyTimes, stride2dTimes, eigenTimes, eigenCoeffTimes;

    int64_t expectedSum = 0;
    for (auto v : data)
    {
        expectedSum += v;
    }

    Eigen::Map<Eigen::Matrix<int64_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> eigenMat(
        data.data(),
        static_cast<Eigen::Index>(rows),
        static_cast<Eigen::Index>(cols));

    for (std::size_t run = 0; run < cfg.warmupRuns + cfg.measuredRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // Manual flat iteration
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t i = 0; i < rows * cols; ++i)
            {
                sum += data[i];
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                manualTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }

        // TensorStridePolicy 2D
        {
            TensorStridePolicy<int64_t> policy({rows, cols}); // Row-major default
            auto it = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::begin(data.data(),
                                                                                  data.data() + rows * cols,
                                                                                  policy);
            auto endIt = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>::end(data.data(),
                                                                                   data.data() + rows * cols,
                                                                                   policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                policyTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }

        // Stride2DPolicy (specialized lightweight)
        {
            Stride2DPolicy<int64_t> policy(rows, cols);
            auto it =
                PolicyIterator<int64_t, Stride2DPolicy<int64_t>>::begin(data.data(), data.data() + rows * cols, policy);
            auto endIt =
                PolicyIterator<int64_t, Stride2DPolicy<int64_t>>::end(data.data(), data.data() + rows * cols, policy);

            int64_t sum = 0;
            Timer t;
            t.start();
            for (; it != endIt; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                stride2dTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }

        // Eigen .sum()
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            sum = eigenMat.sum();
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                eigenTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }

        // Eigen coefficient-wise iteration
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (Eigen::Index r = 0; r < static_cast<Eigen::Index>(rows); ++r)
            {
                for (Eigen::Index c = 0; c < static_cast<Eigen::Index>(cols); ++c)
                {
                    sum += eigenMat(r, c);
                }
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                eigenCoeffTimes.push_back(elapsed / static_cast<double>(rows * cols));
            }
        }
    }

    auto manualStats = Statistics::compute(std::move(manualTimes));
    auto policyStats = Statistics::compute(std::move(policyTimes));
    auto stride2dStats = Statistics::compute(std::move(stride2dTimes));
    auto eigenStats = Statistics::compute(std::move(eigenTimes));
    auto eigenCoeffStats = Statistics::compute(std::move(eigenCoeffTimes));

    std::cout << "  Results (ns/element):\n";
    manualStats.printComparison(std::cout, "Manual flat loop", "ns/op");
    stride2dStats.printComparison(std::cout, "Stride2DPolicy", "ns/op");
    policyStats.printComparison(std::cout, "TensorStridePolicy 2D", "ns/op");
    eigenStats.printComparison(std::cout, "Eigen::sum()", "ns/op");
    eigenCoeffStats.printComparison(std::cout, "Eigen coeff (r,c) loop", "ns/op");

    std::cout << "\n  vs Manual:\n";
    std::cout << "    Stride2DPolicy:     " << std::fixed << std::setprecision(2)
              << (stride2dStats.median / manualStats.median) << "x\n";
    std::cout << "    TensorStridePolicy: " << std::fixed << std::setprecision(2)
              << (policyStats.median / manualStats.median) << "x\n";
    std::cout << "    Eigen::sum():       " << (eigenStats.median / manualStats.median) << "x\n";
    std::cout << "    Eigen coeff loop:   " << (eigenCoeffStats.median / manualStats.median) << "x\n";
}
#endif

// ============================================================================
// Benchmark: TensorIteration Composition Helpers
// ============================================================================

void benchTensorIteration(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "TENSOR ITERATION COMPOSITION HELPERS");
    printContract(std::cout, "Compare iterateND composition vs manual loops vs TensorStridePolicy");
    print_cpu_context(std::cout, "Start");

    // 3D benchmark: 50 x 100 x 200 = 1,000,000 elements
    constexpr std::size_t d1 = 50, d2 = 100, d3 = 200;
    constexpr std::size_t total3D = d1 * d2 * d3;
    auto data3D = generateData(total3D, cfg.seed);

    // 4D benchmark: 8 x 16 x 50 x 100 = 640,000 elements
    constexpr std::size_t e1 = 8, e2 = 16, e3 = 50, e4 = 100;
    constexpr std::size_t total4D = e1 * e2 * e3 * e4;
    auto data4D = generateData(total4D, cfg.seed + 1);

    std::cout << "  3D Tensor: " << d1 << " x " << d2 << " x " << d3 << " (" << total3D << " elements)\n";
    std::cout << "  4D Tensor: " << e1 << " x " << e2 << " x " << e3 << " x " << e4 << " (" << total4D
              << " elements)\n\n";

    // Pre-compute expected sums for correctness checks
    int64_t expected3D = 0;
    for (auto v : data3D)
    {
        expected3D += v;
    }
    int64_t expected4D = 0;
    for (auto v : data4D)
    {
        expected4D += v;
    }

    std::vector<double> manual3DTimes, tensor3DTimes, iterateND3DTimes;
    std::vector<double> manual4DTimes, tensor4DTimes, iterateND4DTimes;

    const std::size_t totalRuns = cfg.warmupRuns + cfg.measuredRuns;
    for (std::size_t run = 0; run < totalRuns; ++run)
    {
        bool measured = (run >= cfg.warmupRuns);

        // ===== 3D BENCHMARKS =====

        // Manual nested loop (3D)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t i = 0; i < d1; ++i)
            {
                for (std::size_t j = 0; j < d2; ++j)
                {
                    for (std::size_t k = 0; k < d3; ++k)
                    {
                        sum += data3D[i * d2 * d3 + j * d3 + k];
                    }
                }
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                manual3DTimes.push_back(elapsed / static_cast<double>(total3D));
            }
            if (run == 0 && sum != expected3D)
            {
                std::cerr << "CORRECTNESS FAILURE: Manual 3D sum\n";
                return;
            }
        }

        // TensorStridePolicy (3D)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            TensorStridePolicy<int64_t> policy(
                {d1, d2, d3},
                {static_cast<std::ptrdiff_t>(d2 * d3), static_cast<std::ptrdiff_t>(d3), 1});
            using Iter = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>;
            auto it = Iter::begin(data3D.data(), data3D.data() + data3D.size(), policy);
            auto end = Iter::end(data3D.data(), data3D.data() + data3D.size(), policy);
            for (; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                tensor3DTimes.push_back(elapsed / static_cast<double>(total3D));
            }
            if (run == 0 && sum != expected3D)
            {
                std::cerr << "CORRECTNESS FAILURE: TensorStridePolicy 3D sum\n";
                return;
            }
        }

        // iterateND composition (3D)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            iterateND(data3D.data(), {d1, d2, d3}, [&](int64_t v) {
                sum += v;
            });
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                iterateND3DTimes.push_back(elapsed / static_cast<double>(total3D));
            }
            if (run == 0 && sum != expected3D)
            {
                std::cerr << "CORRECTNESS FAILURE: iterateND 3D sum\n";
                return;
            }
        }

        // ===== 4D BENCHMARKS =====

        // Manual nested loop (4D)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            for (std::size_t i = 0; i < e1; ++i)
            {
                for (std::size_t j = 0; j < e2; ++j)
                {
                    for (std::size_t k = 0; k < e3; ++k)
                    {
                        for (std::size_t l = 0; l < e4; ++l)
                        {
                            sum += data4D[i * e2 * e3 * e4 + j * e3 * e4 + k * e4 + l];
                        }
                    }
                }
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                manual4DTimes.push_back(elapsed / static_cast<double>(total4D));
            }
            if (run == 0 && sum != expected4D)
            {
                std::cerr << "CORRECTNESS FAILURE: Manual 4D sum\n";
                return;
            }
        }

        // TensorStridePolicy (4D)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            TensorStridePolicy<int64_t> policy({e1, e2, e3, e4},
                                               {static_cast<std::ptrdiff_t>(e2 * e3 * e4),
                                                static_cast<std::ptrdiff_t>(e3 * e4),
                                                static_cast<std::ptrdiff_t>(e4),
                                                1});
            using Iter = PolicyIterator<int64_t, TensorStridePolicy<int64_t>>;
            auto it = Iter::begin(data4D.data(), data4D.data() + data4D.size(), policy);
            auto end = Iter::end(data4D.data(), data4D.data() + data4D.size(), policy);
            for (; it != end; ++it)
            {
                sum += *it;
            }
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                tensor4DTimes.push_back(elapsed / static_cast<double>(total4D));
            }
            if (run == 0 && sum != expected4D)
            {
                std::cerr << "CORRECTNESS FAILURE: TensorStridePolicy 4D sum\n";
                return;
            }
        }

        // iterateND composition (4D)
        {
            int64_t sum = 0;
            Timer t;
            t.start();
            iterateND(data4D.data(), {e1, e2, e3, e4}, [&](int64_t v) {
                sum += v;
            });
            double elapsed = t.elapsedNs();
            DoNotOptimize(sum);
            if (measured)
            {
                iterateND4DTimes.push_back(elapsed / static_cast<double>(total4D));
            }
            if (run == 0 && sum != expected4D)
            {
                std::cerr << "CORRECTNESS FAILURE: iterateND 4D sum\n";
                return;
            }
        }
    }

    // Compute statistics
    auto manual3DStats = Statistics::compute(std::move(manual3DTimes));
    auto tensor3DStats = Statistics::compute(std::move(tensor3DTimes));
    auto iterateND3DStats = Statistics::compute(std::move(iterateND3DTimes));

    auto manual4DStats = Statistics::compute(std::move(manual4DTimes));
    auto tensor4DStats = Statistics::compute(std::move(tensor4DTimes));
    auto iterateND4DStats = Statistics::compute(std::move(iterateND4DTimes));

    std::cout << "  3D Results (ns/element):\n";
    manual3DStats.printComparison(std::cout, "Manual nested loop", "ns/op");
    tensor3DStats.printComparison(std::cout, "TensorStridePolicy", "ns/op");
    iterateND3DStats.printComparison(std::cout, "iterateND (composition)", "ns/op");

    std::cout << "\n  3D vs Manual:\n";
    std::cout << "    TensorStridePolicy:     " << std::fixed << std::setprecision(2)
              << (tensor3DStats.median / manual3DStats.median) << "x\n";
    std::cout << "    iterateND (composition): " << std::fixed << std::setprecision(2)
              << (iterateND3DStats.median / manual3DStats.median) << "x\n";

    std::cout << "\n  4D Results (ns/element):\n";
    manual4DStats.printComparison(std::cout, "Manual nested loop", "ns/op");
    tensor4DStats.printComparison(std::cout, "TensorStridePolicy", "ns/op");
    iterateND4DStats.printComparison(std::cout, "iterateND (composition)", "ns/op");

    std::cout << "\n  4D vs Manual:\n";
    std::cout << "    TensorStridePolicy:     " << std::fixed << std::setprecision(2)
              << (tensor4DStats.median / manual4DStats.median) << "x\n";
    std::cout << "    iterateND (composition): " << std::fixed << std::setprecision(2)
              << (iterateND4DStats.median / manual4DStats.median) << "x\n";

    std::cout << "\n  Composition vs TensorStridePolicy:\n";
    std::cout << "    3D: iterateND is " << std::fixed << std::setprecision(2)
              << (tensor3DStats.median / iterateND3DStats.median) << "x faster\n";
    std::cout << "    4D: iterateND is " << std::fixed << std::setprecision(2)
              << (tensor4DStats.median / iterateND4DStats.median) << "x faster\n";
}

// ============================================================================
// Benchmark: Data Size Scaling (Cache Effects)
// ============================================================================

void benchSizeScaling(const BenchConfig& cfg)
{
    printSectionHeader(std::cout, "SIZE SCALING (CACHE EFFECTS)");
    printContract(std::cout, "Standard iteration sum at different data sizes");
    print_cpu_context(std::cout, "Start");

    struct SizeCase
    {
        const char* name;
        std::size_t n;
    };
    SizeCase sizes[] = {{"L1 (4K)", 4 * 1024 / sizeof(int64_t)},
                        {"L2 (32K)", 32 * 1024 / sizeof(int64_t)},
                        {"L3 (512K)", 512 * 1024 / sizeof(int64_t)},
                        {"RAM (8M)", 8 * 1024 * 1024 / sizeof(int64_t)}};

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
                if (measured)
                {
                    rawTimes.push_back(elapsed / static_cast<double>(sz.n));
                }
            }

            // PolicyIterator
            {
                int64_t sum = 0;
                auto begin = PolicyIterator<int64_t>::begin(data.data(), data.data() + sz.n);
                auto end = PolicyIterator<int64_t>::end(data.data(), data.data() + sz.n);
                Timer t;
                t.start();
                for (auto it = begin; it != end; ++it)
                {
                    sum += *it;
                }
                double elapsed = t.elapsedNs();
                DoNotOptimize(sum);
                if (measured)
                {
                    policyTimes.push_back(elapsed / static_cast<double>(sz.n));
                }
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

    std::cout << "  Library comparisons (auto-detected via __has_include):\n";
#if HAS_BOOST_ITERATOR
    std::cout << "    [x] Boost.Iterator\n";
#else
    std::cout << "    [ ] Boost.Iterator  (not found: <boost/iterator/filter_iterator.hpp>)\n";
#endif
#if HAS_RANGE_V3
    std::cout << "    [x] range-v3\n";
#else
    std::cout << "    [ ] range-v3        (not found: <range/v3/view/filter.hpp>)\n";
#endif
#if HAS_EIGEN
    std::cout << "    [x] Eigen\n";
#else
    std::cout << "    [ ] Eigen           (not found: <Eigen/Dense> or <eigen3/Eigen/Dense>)\n";
#endif
#if HAS_XTENSOR
    std::cout << "    [x] xtensor\n";
#else
    std::cout << "    [ ] xtensor         (requires C++20 or header not found)\n";
#endif
#if HAS_BOOST_MULTIARRAY
    std::cout << "    [x] Boost.MultiArray\n";
#else
    std::cout << "    [ ] Boost.MultiArray (not found: <boost/multi_array.hpp>)\n";
#endif

    auto cfg = BenchConfig::fromEnv();

    std::cout << "\nConfiguration:\n";
    cfg.print(std::cout);

    std::cout << "\nPlatform: " << getPlatformString() << ", Compiler: " << getCompilerString() << "\n";
    std::cout << "Timestamp: " << getTimestampIso() << "\n";

    std::cout << "\nInitial CPU state:\n";
    print_cpu_context(std::cout, "Benchmark start");

    // Run benchmarks
    benchStandardVsRawPointer(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }

    benchStrideVsManual(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }

    benchFilterVsManual(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }

    benchTransformVsManual(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }

    benchTensorStride(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }

    benchTensorContiguous(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }

#if HAS_EIGEN
    benchMatrixIteration(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }
#endif

    benchTensorIteration(cfg);
    if (!cfg.noCooldown)
    {
        cooldownDelay(cfg.cooldownSectionMs, "section transition");
    }

    benchSizeScaling(cfg);

    // Summary
    std::cout << "\n";
    printSectionHeader(std::cout, "BENCHMARK COMPLETE");
    print_cpu_context(std::cout, "Final state");

    return 0;
}
