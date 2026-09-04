/**
 * @file benchmark_TensorMatmul.cpp
 * @brief Reused-input linear-algebra baselines with matched result ownership.
 * Build: g++ -std=c++20 -O3 -DNDEBUG -ffp-contract=off -Iinclude/fat_p
 *        components/Tensor/benchmarks/benchmark_TensorMatmul.cpp -o bench_linalg -pthread
 * MSVC: cl /std:c++20 /O2 /DNDEBUG /EHsc /fp:strict /Iinclude\fat_p
 *       components\Tensor\benchmarks\benchmark_TensorMatmul.cpp /link advapi32.lib
 * Competitors: Fat-P (primary), prevalidated scalar loops with Tensor result storage (baseline).
 * Standard C++ has no matching tensor API. Boost.MultiArray is a storage container, not this
 * algorithm surface; external linear-algebra libraries are deliberately outside this baseline.
 * Input setup and allocation instrumentation are outside timing. Result allocation, initialization,
 * observation, and destruction are included for both implementations. No cold-cache claim is made.
 * Optional CLI: --filter substring (case-sensitive; applied before input construction).
 */

/*
FATP_META:
  meta_version: 1
  component: TensorMatmul
  file_role: benchmark
  path: components/Tensor/benchmarks/benchmark_TensorMatmul.cpp
  namespace: ""
  layer: Testing
  summary: "Paired linear-algebra timing, scalar correctness, and result-allocation baselines."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorMatmul.h
      - include/fat_p/FatPBenchmarkRunner.h
    tests:
      - components/Tensor/tests/test_TensorMatmul.cpp
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
#include "TensorMatmul.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using fat_p::DynamicExtents;
using fat_p::Tensor;
using fat_p::TensorAllocator;
using fat_p::TensorLayout;
using fat_p::TensorStrides;
using fat_p::TensorView;
using fat_p::bench::BenchConfig;
using fat_p::bench::Statistics;

enum class Operation { Dot, Outer, Matmul, Diagonal, Trace };
enum class Layout { Contiguous, Padded, Reversed, Transposed, Batched };

constexpr std::array<const char*, 5> operationNames{"dot", "outer", "matmul", "diagonal", "trace"};
constexpr std::array<const char*, 5> layoutNames{"contiguous", "padded", "reversed", "transposed", "batched"};
constexpr std::array<const char*, 3> variantNames{"fat_p", "scalar_prevalidated", "allocation_only"};

struct Problem
{
    Operation operation;
    Layout layout;
    std::size_t rows;
    std::size_t inner;
    std::size_t columns;

    [[nodiscard]] std::size_t batches() const { return layout == Layout::Batched ? 4 : 1; }
    [[nodiscard]] bool vectorInputs() const { return operation == Operation::Dot || operation == Operation::Outer; }
    [[nodiscard]] std::vector<std::size_t> outputShape() const
    {
        std::vector<std::size_t> result;
        if (layout == Layout::Batched)
        {
            result.push_back(batches());
        }
        if (operation == Operation::Outer || operation == Operation::Matmul)
        {
            result.push_back(rows);
            result.push_back(columns);
        }
        else if (operation == Operation::Diagonal)
        {
            result.push_back(std::min(rows, columns));
        }
        return result;
    }
    [[nodiscard]] std::size_t work() const
    {
        if (operation == Operation::Dot) { return inner; }
        if (operation == Operation::Outer) { return rows * columns; }
        if (operation == Operation::Matmul) { return batches() * rows * inner * columns; }
        return batches() * std::min(rows, columns);
    }
};

template <typename T>
struct Input
{
    std::vector<std::size_t> shape;
    TensorStrides strides;
    std::ptrdiff_t origin = 7;
    std::vector<T> storage;

    Input(std::vector<std::size_t> extents, Layout kind, std::uint64_t seed)
        : shape(std::move(extents)), strides(shape.size(), 1)
    {
        std::size_t span = 1;
        for (std::size_t axis = shape.size(); axis > 0; --axis)
        {
            strides[axis - 1] = static_cast<std::ptrdiff_t>(span);
            span *= shape[axis - 1];
        }
        if (kind == Layout::Transposed)
        {
            strides[shape.size() - 2] = 1;
            strides.back() = static_cast<std::ptrdiff_t>(shape[shape.size() - 2]);
        }
        if (kind == Layout::Padded)
        {
            for (auto& stride : strides) { stride *= 2; }
            span *= 2;
        }
        if (kind == Layout::Reversed)
        {
            origin += static_cast<std::ptrdiff_t>(shape.back() - 1) * strides.back();
            strides.back() = -strides.back();
        }
        storage.assign(span + 14, std::numeric_limits<T>::quiet_NaN());
        std::mt19937_64 random(seed);
        const auto count = std::accumulate(shape.begin(), shape.end(), std::size_t{1}, std::multiplies<>{});
        for (std::size_t linear = 0; linear < count; ++linear)
        {
            auto remaining = linear;
            auto offset = origin;
            for (std::size_t axis = shape.size(); axis > 0; --axis)
            {
                offset += static_cast<std::ptrdiff_t>(remaining % shape[axis - 1]) * strides[axis - 1];
                remaining /= shape[axis - 1];
            }
            const auto numerator = static_cast<int>(random() % 17) - 8;
            storage[static_cast<std::size_t>(offset)] = static_cast<T>(numerator) / T{8};
        }
    }

    [[nodiscard]] auto view() const
    {
        return TensorView<const T>::borrow(storage.data(),
            TensorLayout(storage.size(), origin, DynamicExtents(shape), strides));
    }
    [[nodiscard]] T vectorValue(std::size_t index) const
    {
        return storage[static_cast<std::size_t>(origin + static_cast<std::ptrdiff_t>(index) * strides[0])];
    }
    [[nodiscard]] T matrixValue(std::size_t batch, std::size_t row, std::size_t column) const
    {
        const auto offset = origin + (shape.size() == 3 && shape[0] > 1
            ? static_cast<std::ptrdiff_t>(batch) * strides[0] : 0) +
            static_cast<std::ptrdiff_t>(row) * strides[shape.size() - 2] +
            static_cast<std::ptrdiff_t>(column) * strides.back();
        return storage[static_cast<std::size_t>(offset)];
    }
};

template <typename T>
struct Fixture
{
    Problem problem;
    Input<T> left;
    Input<T> right;
    TensorView<const T> leftView;
    TensorView<const T> rightView;

    static std::vector<std::size_t> inputShape(const Problem& p, bool rightOperand)
    {
        if (p.operation == Operation::Dot) { return {p.inner}; }
        if (p.operation == Operation::Outer) { return {rightOperand ? p.columns : p.rows}; }
        auto result = rightOperand && p.operation == Operation::Matmul
            ? std::vector<std::size_t>{p.inner, p.columns}
            : std::vector<std::size_t>{p.rows, p.operation == Operation::Matmul ? p.inner : p.columns};
        if (p.layout == Layout::Batched) { result.insert(result.begin(), rightOperand ? 1 : p.batches()); }
        return result;
    }

    Fixture(Problem p, std::uint64_t seed)
        : problem(p), left(inputShape(p, false), p.layout, seed),
          right(inputShape(p, true), p.layout, seed + 1), leftView(left.view()), rightView(right.view())
    {
    }

    template <typename Allocator, typename Consumer>
    void withNativeResult(const Allocator& allocator, Consumer&& consumer) const
    {
        switch (problem.operation)
        {
        case Operation::Dot:
            std::invoke(consumer, fat_p::dot(leftView, rightView, allocator));
            return;
        case Operation::Outer:
            std::invoke(consumer, fat_p::outer(leftView, rightView, allocator));
            return;
        case Operation::Matmul:
            std::invoke(consumer, fat_p::matmul(leftView, rightView, allocator));
            return;
        case Operation::Diagonal:
            std::invoke(consumer, fat_p::diagonal(leftView, allocator));
            return;
        case Operation::Trace:
            std::invoke(consumer, fat_p::trace(leftView, allocator));
            return;
        }
        throw std::logic_error("Unknown benchmark operation");
    }

    // Bounded prevalidated scalar baseline; intentionally no production traversal/shape helpers.
    template <typename Allocator>
    [[nodiscard]] auto reference(const Allocator& allocator) const -> Tensor<T, Allocator>
    {
        Tensor<T, Allocator> result(std::allocator_arg, allocator, DynamicExtents(problem.outputShape()));
        if (problem.operation == Operation::Dot)
        {
            T total = 0;
            for (std::size_t k = 0; k < problem.inner; ++k) { total += left.vectorValue(k) * right.vectorValue(k); }
            result[0] = total;
        }
        else if (problem.operation == Operation::Outer)
        {
            for (std::size_t i = 0; i < problem.rows; ++i)
            {
                for (std::size_t j = 0; j < problem.columns; ++j)
                {
                    result[i * problem.columns + j] = left.vectorValue(i) * right.vectorValue(j);
                }
            }
        }
        else if (problem.operation == Operation::Matmul)
        {
            for (std::size_t b = 0; b < problem.batches(); ++b)
            {
                for (std::size_t i = 0; i < problem.rows; ++i)
                {
                    for (std::size_t j = 0; j < problem.columns; ++j)
                    {
                        T total = 0;
                        for (std::size_t k = 0; k < problem.inner; ++k)
                        {
                            total += left.matrixValue(b, i, k) * right.matrixValue(b, k, j);
                        }
                        result[(b * problem.rows + i) * problem.columns + j] = total;
                    }
                }
            }
        }
        else
        {
            const auto length = std::min(problem.rows, problem.columns);
            for (std::size_t b = 0; b < problem.batches(); ++b)
            {
                T total = 0;
                for (std::size_t i = 0; i < length; ++i)
                {
                    const auto value = left.matrixValue(b, i, i);
                    if (problem.operation == Operation::Diagonal) { result[b * length + i] = value; }
                    else { total += value; }
                }
                if (problem.operation == Operation::Trace) { result[b] = total; }
            }
        }
        return result;
    }
};

struct AllocationObservation
{
    std::size_t calls = 0;
    std::size_t bytes = 0;
    std::size_t liveBytes = 0;
};

template <typename T>
struct ProbeAllocator
{
    using value_type = T;
    AllocationObservation* observation;
    [[nodiscard]] T* allocate(std::size_t count)
    {
        auto* result = TensorAllocator<T>{}.allocate(count);
        ++observation->calls;
        observation->bytes += count * sizeof(T);
        observation->liveBytes += count * sizeof(T);
        return result;
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    {
        observation->liveBytes -= count * sizeof(T);
        TensorAllocator<T>{}.deallocate(pointer, count);
    }
    bool operator==(const ProbeAllocator&) const = default;
};

volatile std::size_t observationIndex = 0;
volatile double observationSink = 0;

// Call through a volatile function pointer: the compiler cannot assume the observer only
// samples one element. The entire result escapes, including on MSVC, without a checksum pass.
template <typename T>
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void observeResult(const T* data, std::size_t size)
{
    const auto index = observationIndex;
    observationSink = static_cast<double>(data[index % size]);
    observationIndex = index + 1;
}

template <typename T>
using ResultObserver = void (*)(const T*, std::size_t);
template <typename T>
ResultObserver<T> volatile resultObserver = &observeResult<T>;

template <typename T, typename Allocator, std::size_t Rank>
void consumeResult(const Tensor<T, Allocator, Rank>& result)
{
    resultObserver<T>(result.data(), result.size());
}

struct Sample
{
    std::size_t iterations;
    double elapsedNs;
    double cpuMhz;
    [[nodiscard]] double nsPerCall() const { return elapsedNs / static_cast<double>(iterations); }
};

struct Variant
{
    std::function<void()> invoke;
    AllocationObservation allocation;
    std::size_t iterations = 1;
    std::vector<Sample> samples;
};

struct Case
{
    std::string name;
    std::string dtype;
    std::string inputs;
    Problem problem;
    std::array<Variant, 3> variants;
    std::function<void()> verify;
};

template <typename Range>
std::string formatRange(const Range& range)
{
    std::ostringstream output;
    output << '[';
    bool first = true;
    for (const auto value : range)
    {
        if (!first) { output << ';'; }
        output << value;
        first = false;
    }
    output << ']';
    return output.str();
}

template <typename T>
void addCase(std::vector<Case>& cases, Problem problem, const char* sizeName, std::uint64_t seed,
             const std::string& filter)
{
    Case item;
    item.problem = problem;
    item.dtype = std::same_as<T, float> ? "float" : "double";
    item.name = std::string(operationNames[static_cast<std::size_t>(problem.operation)]) + '/' + item.dtype + '/' +
        sizeName + '/' + layoutNames[static_cast<std::size_t>(problem.layout)];
    if (item.name.find(filter) == std::string::npos) { return; }
    const auto fixture = std::make_shared<Fixture<T>>(problem, seed);
    item.inputs = "left=" + formatRange(fixture->left.shape) + " strides=" + formatRange(fixture->left.strides) +
        " origin=" + std::to_string(fixture->left.origin);
    if (problem.operation != Operation::Diagonal && problem.operation != Operation::Trace)
    {
        item.inputs += " right=" + formatRange(fixture->right.shape) +
            " strides=" + formatRange(fixture->right.strides) + " origin=" + std::to_string(fixture->right.origin);
    }
    item.verify = [fixture] {
        const auto expected = fixture->reference(TensorAllocator<T>{});
        fixture->withNativeResult(TensorAllocator<T>{}, [&expected](const auto& actual) {
            if (actual.extents() != expected.extents() ||
                !std::equal(actual.begin(), actual.end(), expected.begin()))
            {
                throw std::runtime_error("Scalar reference mismatch");
            }
        });
    };
    item.verify();
    for (std::size_t variant = 0; variant < 3; ++variant)
    {
        AllocationObservation probe;
        {
            std::size_t resultSize = 0;
            if (variant == 0)
            {
                fixture->withNativeResult(ProbeAllocator<T>{&probe}, [&resultSize](const auto& result) {
                    resultSize = result.size();
                });
            }
            else if (variant == 1)
            {
                const auto result = fixture->reference(ProbeAllocator<T>{&probe});
                resultSize = result.size();
            }
            else
            {
                const Tensor<T, ProbeAllocator<T>> result(
                    std::allocator_arg, ProbeAllocator<T>{&probe}, DynamicExtents(problem.outputShape()));
                resultSize = result.size();
            }
            if (probe.calls != 1 || probe.bytes != resultSize * sizeof(T))
            {
                throw std::runtime_error("Unexpected result allocation count");
            }
        }
        if (probe.liveBytes != 0) { throw std::runtime_error("Result allocation was not reclaimed"); }
        item.variants[variant].allocation = probe;
        item.variants[variant].invoke = [fixture, variant] {
            if (variant == 0)
            {
                fixture->withNativeResult(TensorAllocator<T>{}, [](const auto& result) {
                    consumeResult(result);
                });
            }
            else if (variant == 1)
            {
                const auto result = fixture->reference(TensorAllocator<T>{});
                consumeResult(result);
            }
            else
            {
                const Tensor<T> result(DynamicExtents(fixture->problem.outputShape()));
                consumeResult(result);
            }
        };
    }
    cases.push_back(std::move(item));
}

template <typename T>
void addMatrix(std::vector<Case>& cases, const BenchConfig& config, const std::string& filter)
{
    const std::array<std::array<std::size_t, 3>, 3> multiplySizes{{{8, 12, 10}, {48, 64, 40}, {128, 192, 96}}};
    const std::array<std::array<std::size_t, 2>, 3> outerSizes{{{8, 12}, {64, 96}, {256, 384}}};
    const std::array<std::array<std::size_t, 2>, 3> diagonalSizes{{{8, 12}, {128, 192}, {512, 768}}};
    const std::array<std::size_t, 3> dotSizes{16, 1024, 65536};
    const std::array<const char*, 3> names{"small", "medium", "large"};
    for (std::size_t operation = 0; operation < 5; ++operation)
    {
        for (std::size_t size = 0; size < (config.quick ? 1U : 3U); ++size)
        {
            for (std::size_t layout = 0; layout < (operation < 2 ? 3U : 5U); ++layout)
            {
                Problem problem{static_cast<Operation>(operation), static_cast<Layout>(layout), 1, 1, 1};
                if (problem.operation == Operation::Dot) { problem.inner = dotSizes[size]; }
                else if (problem.operation == Operation::Outer)
                {
                    problem.rows = outerSizes[size][0]; problem.columns = outerSizes[size][1];
                }
                else if (problem.operation == Operation::Matmul)
                {
                    problem.rows = multiplySizes[size][0]; problem.inner = multiplySizes[size][1];
                    problem.columns = multiplySizes[size][2];
                }
                else { problem.rows = diagonalSizes[size][0]; problem.columns = diagonalSizes[size][1]; }
                addCase<T>(cases, problem, names[size], config.seed + operation * 17 + size * 101, filter);
            }
        }
    }
}

Sample measure(const Variant& variant)
{
    const auto cpu = fat_p::bench::capture_cpu_frequency();
    fat_p::bench::Timer timer;
    timer.start();
    for (std::size_t i = 0; i < variant.iterations; ++i) { variant.invoke(); }
    return {variant.iterations, timer.elapsedNs(), cpu.mCurrentFreqMHz};
}

void calibrate(Variant& variant, std::size_t work, std::size_t targetWork, std::size_t minBatchMs)
{
    variant.iterations = std::max(std::size_t{1}, targetWork / work + (targetWork % work != 0 ? 1U : 0U));
    const auto targetNs = static_cast<double>(minBatchMs) * 1e6;
    for (std::size_t attempt = 0; attempt < 32; ++attempt)
    {
        if (measure(variant).elapsedNs >= targetNs) { return; }
        if (variant.iterations > std::numeric_limits<std::size_t>::max() / 2) { break; }
        variant.iterations *= 2;
    }
    throw std::runtime_error("Could not calibrate benchmark duration safely");
}

[[nodiscard]] Statistics statistics(const Variant& variant)
{
    std::vector<double> samples;
    for (const auto& sample : variant.samples) { samples.push_back(sample.nsPerCall()); }
    return Statistics::compute(samples);
}

std::string quoted(const std::string& value)
{
    std::ostringstream output;
    output << std::quoted(value);
    return output.str();
}

void writeResults(const BenchConfig& config, std::size_t targetWork, const std::string& timestamp,
                  const std::string& cpuContext, const std::vector<Case>& cases, const std::string& filter)
{
    const auto platform = fat_p::bench::detect_platform();
    const auto compiler = fat_p::bench::detect_compiler();
    if (!config.outputJson.empty())
    {
        std::ofstream output(config.outputJson);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << std::setprecision(17) << "{\"schema_version\":1,\"benchmark\":\"TensorMatmul\",\"timestamp\":"
               << quoted(timestamp) << ",\"platform\":" << quoted(platform) << ",\"compiler\":" << quoted(compiler)
               << ",\"cpu_context\":" << quoted(cpuContext) << ",\"seed\":" << config.seed
               << ",\"case_filter\":" << quoted(filter)
               << ",\"warmup\":" << config.warmupRuns << ",\"batches\":" << config.measuredRuns
               << ",\"min_batch_ms\":" << config.minBatchMs << ",\"target_work\":" << targetWork
               << ",\"quick\":" << (config.quick ? "true" : "false")
               << ",\"scope\":" << (!config.noScope ? "true" : "false")
               << ",\"stabilize\":" << (!config.noStabilize ? "true" : "false")
               << ",\"cooldown\":" << (!config.noCooldown ? "true" : "false")
               << ",\"cache_regime\":\"reused_inputs_no_flush\",\"unit\":\"ns/call\""
               << ",\"statistics_kind\":\"dispersion_of_batch_means\",\"cases\":[\n";
        bool first = true;
        for (const auto& item : cases)
        {
            for (std::size_t v = 0; v < 3; ++v)
            {
                if (!first) { output << ",\n"; }
                first = false;
                const auto& variant = item.variants[v];
                const auto stats = statistics(variant);
                output << "{\"name\":" << quoted(item.name) << ",\"dtype\":" << quoted(item.dtype)
                       << ",\"inputs\":" << quoted(item.inputs) << ",\"output\":"
                       << quoted(formatRange(item.problem.outputShape())) << ",\"implementation\":"
                       << quoted(variantNames[v]) << ",\"median\":" << stats.median
                       << ",\"mean\":" << stats.mean << ",\"stddev\":" << stats.stddev
                       << ",\"ci95_mean_low\":" << stats.ci95Low << ",\"ci95_mean_high\":" << stats.ci95High
                       << ",\"result_allocations\":" << variant.allocation.calls
                       << ",\"result_bytes\":" << variant.allocation.bytes
                       << ",\"work_per_call\":" << item.problem.work() << ",\"samples\":[";
                for (std::size_t s = 0; s < variant.samples.size(); ++s)
                {
                    if (s) { output << ','; }
                    const auto& sample = variant.samples[s];
                    output << "{\"iterations\":" << sample.iterations << ",\"elapsed_ns\":" << sample.elapsedNs
                           << ",\"ns_per_call\":" << sample.nsPerCall() << ",\"cpu_mhz\":" << sample.cpuMhz << '}';
                }
                output << "]}";
            }
        }
        output << "\n]}\n";
        output.close();
    }
    if (!config.outputCsv.empty())
    {
        std::ofstream output(config.outputCsv);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << "schema_version,timestamp,benchmark,case,implementation,unit,median,mean,stddev,ci95_mean_low,"
                  "ci95_mean_high,result_allocations,result_bytes,platform,compiler,cpu_context,seed,warmup,batches,"
                  "target_work,min_batch_ms,sample,iterations,elapsed_ns,ns_per_call,cpu_mhz,dtype,inputs,output,"
                  "quick,scope,stabilize,cooldown,cache_regime,work_per_call,statistics_kind,case_filter\n";
        output << std::setprecision(17);
        for (const auto& item : cases)
        {
            for (std::size_t v = 0; v < 3; ++v)
            {
                const auto& variant = item.variants[v];
                const auto stats = statistics(variant);
                for (std::size_t s = 0; s < variant.samples.size(); ++s)
                {
                    const auto& sample = variant.samples[s];
                    output << "1," << quoted(timestamp) << ",TensorMatmul," << item.name << ','
                           << variantNames[v] << ",ns/call," << stats.median << ','
                           << stats.mean << ',' << stats.stddev << ',' << stats.ci95Low << ',' << stats.ci95High
                           << ',' << variant.allocation.calls << ',' << variant.allocation.bytes << ',' << platform
                           << ',' << compiler << ',' << quoted(cpuContext) << ',' << config.seed << ','
                           << config.warmupRuns << ',' << config.measuredRuns << ',' << targetWork << ','
                           << config.minBatchMs << ',' << s << ',' << sample.iterations << ',' << sample.elapsedNs
                           << ',' << sample.nsPerCall() << ',' << sample.cpuMhz << ',' << item.dtype << ','
                           << quoted(item.inputs) << ',' << quoted(formatRange(item.problem.outputShape())) << ','
                           << config.quick << ',' << !config.noScope << ',' << !config.noStabilize << ','
                           << !config.noCooldown << ",reused_inputs_no_flush," << item.problem.work()
                           << ",dispersion_of_batch_means," << quoted(filter) << '\n';
                }
            }
        }
        output.close();
    }
}

int run(const std::string& filter)
{
    const auto config = BenchConfig::fromEnv();
    const auto targetWork = fat_p::bench::detail::getEnvSizeT("FATP_BENCH_TARGET_WORK", 5'000'000);
    if (!config.measuredRuns || !config.warmupRuns || !config.minBatchMs || !targetWork ||
        config.measuredRuns > 1000 || config.warmupRuns > 100 || config.minBatchMs > 10000 ||
        targetWork > 1'000'000'000 || (!config.quick && config.measuredRuns < 7))
    {
        throw std::invalid_argument("Invalid or unreasonably large benchmark configuration");
    }
    std::unique_ptr<fat_p::bench::BenchmarkScope> scope;
    if (!config.noScope) { scope = std::make_unique<fat_p::bench::BenchmarkScope>(true); }
    fat_p::bench::HeaderConfig header;
    header.component = "TensorMatmul";
    header.warmup = config.warmupRuns; header.measured = config.measuredRuns; header.seed = config.seed;
    header.has_extended_config = true; header.target_work = targetWork; header.min_batch_ms = config.minBatchMs;
    header.scope_enabled = !config.noScope; header.stabilize_enabled = !config.noStabilize;
    header.cooldown_enabled = !config.noCooldown;
    header.has_stabilization = !config.noStabilize;
    header.competitors = {{"fat_p::Tensor", true, "primary"},
                         {"scalar loops + Tensor result", true, "baseline; prevalidated, not API-equivalent"},
                         {"Tensor result allocation only", true, "control; no arithmetic"}};
    fat_p::bench::print_standard_header(header);
    std::cout << "Contract: header setup/teardown means inputs only. Reused borrowed inputs; fresh owning result.\n"
                 "  Result allocation, initialization,\n"
                 "  observation and destruction included in both paths. Input setup excluded. No fast-math/LTO.\n"
                 "  Scalar baseline omits API validation and uses the same allocator and output ownership.\n"
                 "  Allocation probes count result elements ONLY; metadata/global allocations are not counted.\n"
                 "  Target work means scalar products (dot/outer/matmul), diagonal visits (diagonal/trace).\n"
                 "  ns/call is primary; CI95 is an approximate interval for the mean, not the median.\n"
                 "  Dispersion describes batch means, not individual-call latency. Do not subtract medians.\n"
              << "  Quick mode: " << (config.quick ? "ON (smoke only)" : "OFF") << '\n';
    std::cout << "  Case filter: " << (filter.empty() ? "(all)" : filter) << '\n';
    if (!config.noStabilize) { fat_p::bench::waitForStableCpu(config); }
    std::ostringstream cpuContext;
    fat_p::bench::print_cpu_context(cpuContext, "linear algebra start");
    auto cpuText = cpuContext.str();
    cpuText.erase(std::remove(cpuText.begin(), cpuText.end(), '\n'), cpuText.end());
    cpuText.erase(std::remove(cpuText.begin(), cpuText.end(), '\r'), cpuText.end());
    std::cout << cpuText << '\n';
    const auto timestamp = fat_p::bench::current_timestamp();
    std::vector<Case> cases;
    addMatrix<float>(cases, config, filter);
    addMatrix<double>(cases, config, filter);
    if (cases.empty()) { throw std::invalid_argument("Case filter matched no problems"); }
    std::cout << "[PASS] Full outputs and allocation reclamation checked for " << cases.size() << " paired cases\n";
    std::mt19937_64 random(config.seed);
    std::vector<std::size_t> order(cases.size());
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), random);
    for (const auto index : order)
    {
        auto& item = cases[index];
        fat_p::bench::print_cpu_context(std::cout, item.name.c_str());
        std::array<std::size_t, 3> variants{0, 1, 2};
        std::shuffle(variants.begin(), variants.end(), random);
        for (const auto v : variants)
        {
            calibrate(item.variants[v], item.problem.work(), targetWork, config.minBatchMs);
        }
    }
    for (std::size_t round = 0; round < config.warmupRuns + config.measuredRuns; ++round)
    {
        if (!config.noCooldown) { fat_p::bench::cooldownDelay(50, "round", config.verboseStats); }
        std::shuffle(order.begin(), order.end(), random);
        for (const auto index : order)
        {
            auto& item = cases[index];
            std::array<std::size_t, 3> variants{0, 1, 2};
            std::shuffle(variants.begin(), variants.end(), random);
            for (const auto v : variants)
            {
                auto& variant = item.variants[v];
                const auto sample = measure(variant);
                if (round >= config.warmupRuns)
                {
                    if (sample.elapsedNs < static_cast<double>(config.minBatchMs) * 1e6)
                    {
                        std::cout << "[NOTE] Short batch " << item.name << " implementation=" << v << '\n';
                    }
                    variant.samples.push_back(sample);
                }
            }
        }
        std::cout << "Completed " << (round < config.warmupRuns ? "warmup " : "measured ") << round + 1 << '\n';
    }
    for (const auto& item : cases)
    {
        item.verify();
        std::cout << item.name << " " << item.inputs << " output=" << formatRange(item.problem.outputShape()) << '\n';
        for (std::size_t v = 0; v < 3; ++v)
        {
            const auto stats = statistics(item.variants[v]);
            std::cout << "  " << variantNames[v] << ": " << stats.median
                      << " ns/call median; mean=" << stats.mean << " stddev=" << stats.stddev
                      << " mean_CI95=[" << stats.ci95Low << ',' << stats.ci95High << "] result_allocations="
                      << item.variants[v].allocation.calls << " bytes=" << item.variants[v].allocation.bytes << '\n';
        }
    }
    writeResults(config, targetWork, timestamp, cpuText, cases, filter);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#endif
    try
    {
        std::string filter;
        if (argc != 1)
        {
            if (argc != 3 || std::string(argv[1]) != "--filter")
            {
                throw std::invalid_argument("Usage: benchmark_TensorMatmul [--filter substring]");
            }
            filter = argv[2];
            if (filter.empty() || filter.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789/_-") !=
                                      std::string::npos)
            {
                throw std::invalid_argument("Filter must contain only lowercase case-name characters");
            }
        }
        return run(filter);
    }
    catch (const std::exception& error)
    {
        std::cerr << "BENCHMARK FAILED: " << error.what() << '\n';
        return 1;
    }
}
