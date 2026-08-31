/**
 * @file benchmark_TensorContractions.cpp
 * @brief Paired explicit-axis contraction benchmarks with a prevalidated scalar baseline.
 * Build: g++ -std=c++20 -O3 -DNDEBUG -ffp-contract=off -Iinclude/fat_p
 *        components/Tensor/benchmarks/benchmark_TensorContractions.cpp -pthread
 * MSVC: cl /std:c++20 /O2 /DNDEBUG /EHsc /fp:strict /Iinclude\\fat_p
 *       components\\Tensor\\benchmarks\\benchmark_TensorContractions.cpp /link advapi32.lib
 * All variants allocate, initialize, observe, and destroy a fresh Tensor result.
 * Reused inputs; setup and validation checks excluded from the scalar baseline only.
 * No external library comparison, cold-cache, universal speedup, or per-call tail claim.
 */
/*
FATP_META:
  meta_version: 1
  component: TensorContractions
  file_role: benchmark
  path: components/Tensor/benchmarks/benchmark_TensorContractions.cpp
  namespace: fat_p::bench::tensor_contractions
  layer: Testing
  summary: "Paired serial, explicit-context, and scalar contraction baselines."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorContractions.h
      - include/fat_p/TensorExecution.h
    tests:
      - components/Tensor/tests/test_TensorContractions.cpp
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
#include "TensorExecution.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fat_p::bench::tensor_contractions
{
struct Problem
{
    std::string name;
    std::size_t rows;
    std::size_t k;
    std::size_t q;
    std::size_t columns;
    bool reversed;
};
struct Sample
{
    std::size_t iterations;
    double elapsedNs;
    double cpuMhz;
};
struct Result
{
    Problem problem;
    std::size_t variant;
    std::vector<Sample> samples;
};
constexpr std::array<const char*, 4> names{"tensor_dot_serial",
                                           "context_default",
                                           "context_forced",
                                           "scalar_prevalidated"};
void observe(const double* data, std::size_t size)
{
    if (size != 0)
    {
        volatile double value = data[size / 2];
        (void)value;
    }
}
using Observer = void (*)(const double*, std::size_t);
Observer volatile observer = observe;

// Specialized independent K-then-Q fold. Layout/axes validated outside timing.
Tensor<double> scalar(const Problem& p, const double* a, const double* b)
{
    Tensor<double> result(DynamicExtents{p.rows, p.columns}, 0.0);
    for (std::size_t row = 0; row < p.rows; ++row)
    {
        for (std::size_t column = 0; column < p.columns; ++column)
        {
            double total = 0;
            for (std::size_t k = 0; k < p.k; ++k)
            {
                const auto physicalK = p.reversed ? p.k - 1 - k : k;
                for (std::size_t q = 0; q < p.q; ++q)
                {
                    total = total + a[(row * p.q + q) * p.k + physicalK] * b[(k * p.columns + column) * p.q + q];
                }
            }
            result[row * p.columns + column] = total;
        }
    }
    return result;
}

std::vector<Result>
measureProblem(const Problem& p, const BenchConfig& config, std::size_t targetWork, std::mt19937_64& random)
{
    Tensor<double> a({p.rows, p.q, p.k}), b({p.k, p.columns, p.q});
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        a[i] = static_cast<double>(i % 17) / 13;
    }
    for (std::size_t i = 0; i < b.size(); ++i)
    {
        b[i] = static_cast<double>(i % 11) / 7;
    }
    const auto av = a.sliceView({Slice{}, Slice{}, Slice{std::nullopt, std::nullopt, p.reversed ? -1 : 1}});
    const std::vector<TensorAxis> aa{2, 1}, ba{0, 2};
    ThreadPool pool(4); // Caller-owned; default 2000us spin; construction outside timing.
    const auto context = TensorExecutionContext::parallel(pool);
    TensorExecutionOptions options;
    options.minimumWork = 0;
    const auto forced = TensorExecutionContext::parallel(pool, options);
    const std::array<std::function<Tensor<double>()>, 4> operations{[&] {
                                                                        return tensorDot(av, b, aa, ba);
                                                                    },
                                                                    [&] {
                                                                        return tensorDot(av, b, aa, ba, context);
                                                                    },
                                                                    [&] {
                                                                        return tensorDot(av, b, aa, ba, forced);
                                                                    },
                                                                    [&] {
                                                                        return scalar(p, a.data(), b.data());
                                                                    }};
    const auto verify = [&] {
        const auto reference = scalar(p, a.data(), b.data());
        for (const auto& operation : operations)
        {
            const auto actual = operation();
            if (actual.extents() != reference.extents())
            {
                throw std::runtime_error("Benchmark shape mismatch");
            }
            for (std::size_t i = 0; i < actual.size(); ++i)
            {
                if (std::bit_cast<std::uint64_t>(actual[i]) != std::bit_cast<std::uint64_t>(reference[i]))
                {
                    throw std::runtime_error("Benchmark scalar differential: " + p.name);
                }
            }
        }
    };
    verify();
    const auto measure = [&](std::size_t variant, std::size_t iterations) {
        const auto cpu = fat_p::bench::capture_cpu_frequency();
        Timer timer;
        timer.start();
        for (std::size_t i = 0; i < iterations; ++i)
        {
            const auto value = operations[variant]();
            observer(value.data(), value.size());
        }
        return Sample{iterations, timer.elapsedNs(), cpu.mCurrentFreqMHz};
    };
    const auto work = p.rows * p.k * p.q * p.columns;
    std::array<std::size_t, 4> iterations;
    iterations.fill(std::max(std::size_t{1}, targetWork / work));
    std::array<std::size_t, 4> order{0, 1, 2, 3};
    std::shuffle(order.begin(), order.end(), random);
    for (const auto variant : order)
    {
        while (measure(variant, iterations[variant]).elapsedNs < static_cast<double>(config.minBatchMs) * 1e6)
        {
            if (iterations[variant] > 100000000)
            {
                throw std::runtime_error("Calibration limit");
            }
            iterations[variant] *= 2;
        }
    }
    std::vector<Result> results{{p, 0, {}}, {p, 1, {}}, {p, 2, {}}, {p, 3, {}}};
    for (std::size_t round = 0; round < config.warmupRuns + config.measuredRuns; ++round)
    {
        if (!config.noCooldown)
        {
            cooldownDelay(20, "round", config.verboseStats);
        }
        std::shuffle(order.begin(), order.end(), random);
        for (const auto variant : order)
        {
            const auto sample = measure(variant, iterations[variant]);
            if (round >= config.warmupRuns)
            {
                results[variant].samples.push_back(sample);
            }
        }
    }
    verify();
    return results;
}

void exportResults(const BenchConfig& config, const std::vector<Result>& results, const std::string& cpu)
{
    std::ofstream csv, json;
    if (!config.outputCsv.empty())
    {
        csv.open(config.outputCsv);
        csv.exceptions(std::ios::badbit | std::ios::failbit);
        csv << "case,variant,rows,k,q,columns,reversed,sample,iterations,elapsed_ns,ns_per_call,cpu_mhz,"
               "median,mean,stddev,ci95_mean_low,ci95_mean_high\n"
            << std::setprecision(17);
    }
    if (!config.outputJson.empty())
    {
        json.open(config.outputJson);
        json.exceptions(std::ios::badbit | std::ios::failbit);
        json << std::setprecision(17) << "{\"schema_version\":1,\"benchmark\":\"TensorContractions\","
             << "\"compiler\":" << std::quoted(detect_compiler()) << ",\"cpu\":" << std::quoted(cpu)
             << ",\"timestamp\":" << std::quoted(current_timestamp()) << ",\"dtype\":\"double\",\"workers\":4,"
             << "\"pool_spin_us\":2000,\"grain\":32,\"default_minimum_work\":1048576,"
             << "\"pinning\":false,\"cache_regime\":\"reused_inputs_no_flush\",\"seed\":" << config.seed
             << ",\"warmup\":" << config.warmupRuns << ",\"batches\":" << config.measuredRuns
             << ",\"min_batch_ms\":" << config.minBatchMs << ",\"quick\":" << (config.quick ? "true" : "false")
             << ",\"unit\":\"ns/call\",\"statistics_kind\":\"batch_mean_dispersion\",\"cases\":[";
    }
    for (std::size_t index = 0; index < results.size(); ++index)
    {
        const auto& r = results[index];
        const auto& p = r.problem;
        std::vector<double> values;
        for (const auto& s : r.samples)
        {
            values.push_back(s.elapsedNs / static_cast<double>(s.iterations));
        }
        const auto stats = Statistics::compute(values);
        std::cout << p.name << ' ' << names[r.variant] << " median=" << stats.median << " ns/call\n";
        if (json.is_open())
        {
            if (index != 0)
            {
                json << ',';
            }
            json << "{\"name\":" << std::quoted(p.name) << ",\"variant\":" << std::quoted(names[r.variant])
                 << ",\"rows\":" << p.rows << ",\"k\":" << p.k << ",\"q\":" << p.q << ",\"columns\":" << p.columns
                 << ",\"reversed\":" << (p.reversed ? "true" : "false") << ",\"median\":" << stats.median
                 << ",\"mean\":" << stats.mean << ",\"stddev\":" << stats.stddev
                 << ",\"ci95_mean_low\":" << stats.ci95Low << ",\"ci95_mean_high\":" << stats.ci95High
                 << ",\"samples\":[";
        }
        for (std::size_t i = 0; i < r.samples.size(); ++i)
        {
            const auto& s = r.samples[i];
            if (csv.is_open())
            {
                csv << p.name << ',' << names[r.variant] << ',' << p.rows << ',' << p.k << ',' << p.q << ','
                    << p.columns << ',' << p.reversed << ',' << i << ',' << s.iterations << ',' << s.elapsedNs << ','
                    << values[i] << ',' << s.cpuMhz << ',' << stats.median << ',' << stats.mean << ',' << stats.stddev
                    << ',' << stats.ci95Low << ',' << stats.ci95High << '\n';
            }
            if (json.is_open())
            {
                if (i != 0)
                {
                    json << ',';
                }
                json << "{\"iterations\":" << s.iterations << ",\"elapsed_ns\":" << s.elapsedNs
                     << ",\"ns_per_call\":" << values[i] << ",\"cpu_mhz\":" << s.cpuMhz << '}';
            }
        }
        if (json.is_open())
        {
            json << "]}";
        }
    }
    if (csv.is_open())
    {
        csv.close();
    }
    if (json.is_open())
    {
        json << "]}\n";
        json.close();
    }
}

int run()
{
    const auto config = BenchConfig::fromEnv();
    const auto targetWork = detail::getEnvSizeT("FATP_BENCH_TARGET_WORK", 10000);
    if (!config.warmupRuns || !config.measuredRuns || !config.minBatchMs || !targetWork || config.warmupRuns > 100 ||
        config.measuredRuns > 1000 || config.minBatchMs > 10000 || targetWork > 1000000000 ||
        (!config.quick && config.measuredRuns < 7))
    {
        throw std::invalid_argument("Invalid benchmark configuration");
    }
    HeaderConfig header;
    header.component = "TensorContractions";
    header.warmup = config.warmupRuns;
    header.measured = config.measuredRuns;
    header.seed = config.seed;
    header.scope_enabled = false;
    header.stabilize_enabled = !config.noStabilize;
    header.has_stabilization = !config.noStabilize;
    header.cooldown_enabled = !config.noCooldown;
    header.competitors = {{"Tensor tensorDot", true, "serial and explicit native contexts"},
                          {"prevalidated scalar", true, "same K-then-Q fold and result ownership"}};
    print_standard_header(header);
    std::cout << "target_work=" << targetWork << " min_batch_ms=" << config.minBatchMs
              << "\nNo affinity/priority override; 4 workers, default 2000us spin, grain 32.\n"
                 "Shapes A[M,Q,K], B[K,N,Q]; paired axes {2,1}/{0,2}; K-then-Q fold.\n"
                 "Reused inputs; fresh result allocation/init/observation/destruction included.\n"
                 "Scalar baseline excludes validation/planning. Full output bits checked outside timing.\n"
                 "Pool construction excluded; idle pool exists for every variant. No cache flushing.\n"
                 "Batch-mean dispersion is NOT per-call tail latency. No external competitors.\n";
    if (!config.noStabilize)
    {
        waitForStableCpu(config);
    }
    std::ostringstream cpu;
    print_cpu_context(cpu, "contractions benchmark");
    auto cpuText = cpu.str();
    cpuText.erase(std::remove(cpuText.begin(), cpuText.end(), '\n'), cpuText.end());
    cpuText.erase(std::remove(cpuText.begin(), cpuText.end(), '\r'), cpuText.end());
    std::vector<Problem> problems{{"small", 16, 4, 4, 16, false},
                                  {"medium", 32, 16, 16, 32, false},
                                  {"cutoff", 64, 8, 32, 64, false},
                                  {"reversed", 32, 16, 16, 32, true},
                                  {"scalar_output", 1, 64, 64, 1, false}};
    std::mt19937_64 random(config.seed);
    std::shuffle(problems.begin(), problems.end(), random);
    std::vector<Result> results;
    for (const auto& problem : problems)
    {
        std::cout << "Measuring " << problem.name << std::endl;
        auto measured = measureProblem(problem, config, targetWork, random);
        results.insert(results.end(),
                       std::make_move_iterator(measured.begin()),
                       std::make_move_iterator(measured.end()));
    }
    exportResults(config, results, cpuText);
    return 0;
}
} // namespace fat_p::bench::tensor_contractions

int main()
{
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#endif
    try
    {
        return fat_p::bench::tensor_contractions::run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "BENCHMARK FAILED: " << error.what() << '\n';
        return 1;
    }
}
