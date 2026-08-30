/**
 * @file benchmark_Tensor.cpp
 * @brief Repeatable layout baselines for the owner/view Tensor architecture.
 *
 * Build:
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -Iinclude/fat_p \
 *       components/Tensor/benchmarks/benchmark_Tensor.cpp -o benchmark_Tensor
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc /Iinclude\fat_p \
 *       components\Tensor\benchmarks\benchmark_Tensor.cpp /Fe:benchmark_Tensor.exe \
 *       /link advapi32.lib
 *
 * Iterator and TensorIterationPlan cases run in the same process so their
 * numbers are directly comparable. The broadcast case measures explicit clone
 * materialization, including allocation and copying.
 */

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: benchmark
  path: components/Tensor/benchmarks/benchmark_Tensor.cpp
  namespace: fat_p
  layer: Testing
  summary: "Repeatable dynamic Tensor layout and broadcast baselines."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
    headers:
      - include/fat_p/FatPBenchmarkHeader.h
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/Tensor.h
    tests:
      - components/Tensor/tests/test_Tensor.cpp
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

#include "FatPBenchmarkHeader.h"
#include "FatPBenchmarkRunner.h"
#include "Tensor.h"
#include "TensorAlgorithms.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

using fat_p::Tensor;
using fat_p::DynamicExtents;
using fat_p::ReadableTensor;
using fat_p::TensorLayout;
using fat_p::TensorStrides;
using fat_p::TensorView;
using fat_p::clone;
using fat_p::bench::BenchConfig;
using fat_p::bench::BenchmarkScope;
using fat_p::bench::DoNotOptimize;
using fat_p::bench::Statistics;
using fat_p::bench::Timer;

struct LayoutCase
{
    LayoutCase(std::string caseName, std::string caseContract, std::size_t elements,
               std::function<double()> caseOperation)
        : name(std::move(caseName))
        , contract(std::move(caseContract))
        , logicalElements(elements)
        , operation(std::move(caseOperation))
    {
    }

    std::string name;
    std::string contract;
    std::size_t logicalElements = 0;
    std::function<double()> operation;
    std::size_t iterations = 1;
    std::vector<double> samplesNsPerElement;
};

template <ReadableTensor R>
double sumIterator(const R& readable)
{
    return std::accumulate(readable.begin(), readable.end(), 0.0);
}

template <ReadableTensor R>
double sumPlan(const R& readable)
{
    fat_p::tensor_detail::TensorAccess::validate(readable);
    const auto* storage = fat_p::tensor_detail::TensorAccess::storageBase(readable);
    const auto& layout = readable.layout();
    const fat_p::tensor_detail::TensorIterationPlan plan(layout.extents(), {std::cref(layout)});
    double total = 0.0;
    plan.forEachOffset([&](std::size_t, const std::vector<std::ptrdiff_t>& offsets) {
        total += static_cast<double>(storage[offsets[0]]);
    });
    return total;
}

std::size_t readTargetWork()
{
    return fat_p::bench::detail::getEnvSizeT("FATP_BENCH_TARGET_WORK", 5'000'000);
}

double timeCase(LayoutCase& benchmarkCase)
{
    Timer timer;
    timer.start();
    double checksum = 0.0;
    for (std::size_t iteration = 0; iteration < benchmarkCase.iterations; ++iteration)
    {
        checksum += benchmarkCase.operation();
    }
    const double elapsedNs = timer.elapsedNs();
    DoNotOptimize(checksum);
    const auto operations = benchmarkCase.iterations * benchmarkCase.logicalElements;
    return elapsedNs / static_cast<double>(operations);
}

void calibrate(LayoutCase& benchmarkCase, std::size_t targetWork, std::size_t minBatchMs)
{
    benchmarkCase.iterations =
        std::max<std::size_t>(1, (targetWork + benchmarkCase.logicalElements - 1) / benchmarkCase.logicalElements);

    const double targetNs = static_cast<double>(minBatchMs) * 1'000'000.0;
    for (;;)
    {
        Timer timer;
        timer.start();
        double checksum = 0.0;
        for (std::size_t iteration = 0; iteration < benchmarkCase.iterations; ++iteration)
        {
            checksum += benchmarkCase.operation();
        }
        const double elapsedNs = timer.elapsedNs();
        DoNotOptimize(checksum);
        if (elapsedNs >= targetNs || benchmarkCase.iterations > std::numeric_limits<std::size_t>::max() / 2)
        {
            if (elapsedNs < targetNs)
            {
                std::cout << "  [NOTE] " << benchmarkCase.name << " did not reach the requested minimum batch time\n";
            }
            return;
        }
        benchmarkCase.iterations *= 2;
    }
}

void writeCsv(const std::string& path, const std::vector<LayoutCase>& cases)
{
    if (path.empty())
    {
        return;
    }
    std::ofstream output(path);
    output << "case,sample,ns_per_element\n";
    for (const auto& benchmarkCase : cases)
    {
        for (std::size_t sample = 0; sample < benchmarkCase.samplesNsPerElement.size(); ++sample)
        {
            output << benchmarkCase.name << ',' << sample << ',' << std::setprecision(12)
                   << benchmarkCase.samplesNsPerElement[sample] << '\n';
        }
    }
}

void writeJson(const std::string& path, const BenchConfig& config, std::size_t targetWork,
               const std::vector<LayoutCase>& cases)
{
    if (path.empty())
    {
        return;
    }
    std::ofstream output(path);
    output << "{\n  \"seed\": " << config.seed << ",\n  \"warmup_runs\": " << config.warmupRuns
           << ",\n  \"measured_runs\": " << config.measuredRuns << ",\n  \"target_work\": " << targetWork
           << ",\n  \"min_batch_ms\": " << config.minBatchMs << ",\n  \"cases\": [\n";
    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex)
    {
        const auto& benchmarkCase = cases[caseIndex];
        const auto stats = Statistics::compute(benchmarkCase.samplesNsPerElement);
        output << "    {\"name\": \"" << benchmarkCase.name << "\", \"median_ns_per_element\": "
               << std::setprecision(12) << stats.median << ", \"mean_ns_per_element\": " << stats.mean
               << ", \"stddev_ns_per_element\": " << stats.stddev << ", \"samples\": [";
        for (std::size_t sample = 0; sample < benchmarkCase.samplesNsPerElement.size(); ++sample)
        {
            if (sample != 0)
            {
                output << ", ";
            }
            output << benchmarkCase.samplesNsPerElement[sample];
        }
        output << "]}" << (caseIndex + 1 == cases.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

} // namespace

int main()
{
    const BenchConfig config = BenchConfig::fromEnv();
    const std::size_t targetWork = readTargetWork();
    std::unique_ptr<BenchmarkScope> benchmarkScope;
    if (!config.noScope)
    {
        benchmarkScope = std::make_unique<BenchmarkScope>(true);
    }

    fat_p::bench::HeaderConfig header;
    header.component = "Tensor layout baselines";
    header.warmup = config.warmupRuns;
    header.measured = config.measuredRuns;
    header.seed = config.seed;
    header.has_extended_config = true;
    header.target_work = targetWork;
    header.min_batch_ms = config.minBatchMs;
    header.scope_enabled = !config.noScope;
    header.stabilize_enabled = !config.noStabilize;
    header.cooldown_enabled = !config.noCooldown;
    header.competitors = {{"fat_p::Tensor", true, "owner/view unified-plan baseline"}};
    fat_p::bench::print_standard_header(header);
    config.print();
    std::cout << "  Target work: " << targetWork << " logical elements per calibrated batch\n";
    std::cout << "  Compiler flags: optimized Release build required (-O3/-O2, NDEBUG)\n";
    std::cout << "  Negative stride: reversed-axis view included\n\n";

    if (!config.noStabilize)
    {
        fat_p::bench::waitForStableCpu(config);
    }
    fat_p::bench::print_cpu_context(std::cout, "Tensor baseline start");

    constexpr std::size_t rows = 512;
    constexpr std::size_t columns = 512;
    Tensor<double> owner({rows, columns}, 1.0);
    auto transposed = owner.transposeView();
    auto slice = owner.sliceView({64, 64}, {448, 448});
    const auto reversed = TensorView<const double>::borrow(
        owner.data(), TensorLayout(owner.size(), static_cast<std::ptrdiff_t>(columns - 1),
                                   DynamicExtents{rows, columns},
                                   TensorStrides{static_cast<std::ptrdiff_t>(columns), -1}));
    Tensor<double> broadcastSource({1, columns}, 1.0);

    const auto initialBroadcast = clone(broadcastSource.broadcastView(DynamicExtents{rows, columns}));
    if (sumIterator(initialBroadcast) != static_cast<double>(rows * columns) ||
        sumPlan(initialBroadcast) != static_cast<double>(rows * columns))
    {
        std::cerr << "CORRECTNESS FAILURE: broadcast materialization\n";
        return 1;
    }
    if (sumIterator(owner) != static_cast<double>(rows * columns) ||
        sumPlan(owner) != static_cast<double>(rows * columns) ||
        sumIterator(transposed) != static_cast<double>(rows * columns) ||
        sumPlan(transposed) != static_cast<double>(rows * columns) ||
        sumIterator(slice) != static_cast<double>((448 - 64) * (448 - 64)) ||
        sumPlan(slice) != static_cast<double>((448 - 64) * (448 - 64)) ||
        sumIterator(reversed) != static_cast<double>(rows * columns) ||
        sumPlan(reversed) != static_cast<double>(rows * columns))
    {
        std::cerr << "CORRECTNESS FAILURE: layout sums\n";
        return 1;
    }

    std::vector<LayoutCase> cases;
    cases.push_back({"contiguous_iterator_sum", "Owner pointer iteration", rows * columns,
                     [&owner]() { return sumIterator(owner); }});
    cases.push_back({"contiguous_plan_sum", "TensorIterationPlan over a canonical owner", rows * columns,
                     [&owner]() { return sumPlan(owner); }});
    cases.push_back({"transpose_iterator_sum", "Logical iterator over a transpose view", rows * columns,
                     [&transposed]() { return sumIterator(transposed); }});
    cases.push_back({"transpose_plan_sum", "TensorIterationPlan over a transpose view", rows * columns,
                     [&transposed]() { return sumPlan(transposed); }});
    cases.push_back({"slice_iterator_sum", "Logical iterator over a 384x384 interior slice",
                     (448 - 64) * (448 - 64), [&slice]() { return sumIterator(slice); }});
    cases.push_back({"slice_plan_sum", "TensorIterationPlan over a 384x384 interior slice",
                     (448 - 64) * (448 - 64), [&slice]() { return sumPlan(slice); }});
    cases.push_back({"negative_stride_iterator_sum", "Logical iterator over a reversed-axis view",
                     rows * columns, [&reversed]() { return sumIterator(reversed); }});
    cases.push_back({"negative_stride_plan_sum", "TensorIterationPlan over a reversed-axis view",
                     rows * columns, [&reversed]() { return sumPlan(reversed); }});
    cases.push_back({"broadcast_materialize_sum", "Materialize 1x512 to 512x512, then sum", rows * columns,
                     [&broadcastSource]() {
                         auto result = clone(broadcastSource.broadcastView(DynamicExtents{rows, columns}));
                         return sumPlan(result);
                     }});

    for (auto& benchmarkCase : cases)
    {
        calibrate(benchmarkCase, targetWork, config.minBatchMs);
        std::cout << "  Calibrated " << benchmarkCase.name << ": " << benchmarkCase.iterations
                  << " operation(s) per batch\n";
    }

    std::mt19937_64 random(config.seed);
    std::vector<std::size_t> order(cases.size());
    for (std::size_t index = 0; index < order.size(); ++index)
    {
        order[index] = index;
    }

    for (std::size_t warmup = 0; warmup < config.warmupRuns; ++warmup)
    {
        std::shuffle(order.begin(), order.end(), random);
        for (const auto index : order)
        {
            DoNotOptimize(timeCase(cases[index]));
        }
    }

    for (std::size_t batch = 0; batch < config.measuredRuns; ++batch)
    {
        std::shuffle(order.begin(), order.end(), random);
        for (const auto index : order)
        {
            cases[index].samplesNsPerElement.push_back(timeCase(cases[index]));
        }
    }

    std::cout << "\nResults (ns/logical element; median is primary):\n";
    for (const auto& benchmarkCase : cases)
    {
        const auto stats = Statistics::compute(benchmarkCase.samplesNsPerElement);
        std::cout << "  " << std::left << std::setw(28) << benchmarkCase.name << std::right << std::fixed
                  << std::setprecision(3) << stats.median << " median, " << stats.mean << " mean, "
                  << stats.stddev << " stddev, CI95 [" << stats.ci95Low << ", " << stats.ci95High << "]\n";
        std::cout << "    contract: " << benchmarkCase.contract << "\n    raw:";
        for (const double sample : benchmarkCase.samplesNsPerElement)
        {
            std::cout << ' ' << sample;
        }
        std::cout << '\n';
    }

    writeCsv(config.outputCsv, cases);
    writeJson(config.outputJson, config, targetWork, cases);
    return 0;
}
