/**
 * @file benchmark_TensorExecution.cpp
 * @brief Matched serial/context benchmark; baseline uses the same kernels and ownership.
 * Build: g++ -std=c++20 -O3 -DNDEBUG -ffp-contract=off -Iinclude/fat_p
 *        components/Tensor/benchmarks/benchmark_TensorExecution.cpp -pthread -o bench_execution
 * MSVC: /std:c++20 /O2 /DNDEBUG /EHsc /fp:strict /Iinclude\fat_p, link advapi32.lib.
 * Reused inputs; result allocation, computation, observation and destruction included.
 * Pool construction excluded. No affinity pinning, fast-math, LTO or cold-cache claim.
 */
/*
FATP_META:
  meta_version: 1
  component: TensorExecution
  file_role: benchmark
  path: components/Tensor/benchmarks/benchmark_TensorExecution.cpp
  namespace: ""
  layer: Testing
  summary: "Execution crossover, grain, worker-count and concurrent/nested-caller measurements."
  api_stability: in_work
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  related:
    headers:
      - include/fat_p/TensorExecution.h
      - include/fat_p/FatPBenchmarkRunner.h
*/

#include "FatPBenchmarkHeader.h"
#include "FatPBenchmarkRunner.h"
#include "TensorExecution.h"

#include <algorithm>
#include <array>
#include <barrier>
#include <bit>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>

namespace
{
using fat_p::Tensor;
using fat_p::TensorExecutionContext;
using fat_p::TensorExecutionOptions;
using fat_p::ThreadPool;
using fat_p::bench::BenchConfig;
using fat_p::bench::Statistics;

void observe(const double* data, std::size_t size)
{
    if (size != 0)
    {
        volatile double value = data[size / 2];
        (void)value;
    }
}
// Unknown whole-buffer observer; shared pointer is read-only during concurrent calls.
void (*volatile observer)(const double*, std::size_t) = observe;

struct Problem
{
    std::string name;
    std::size_t rows;
    std::size_t inner;
    std::size_t columns;
    std::size_t batches = 1;
    std::size_t workers = 4;
    std::size_t grain = 32;
    std::size_t callers = 1;
    bool nested = false;
    bool padded = false;
    std::size_t spinUs = 0;
};

struct Sample
{
    std::size_t iterations;
    double elapsedNs;
    double cpuMhz;
    double nsPerCall(std::size_t callers) const
    {
        return elapsedNs / static_cast<double>(iterations * callers);
    }
};

// Persistent callers are created before timing. Each batch includes the two barriers,
// but excludes thread/pool construction. Nested callers occupy every pool worker.
class Callers
{
public:
    Callers(ThreadPool& pool, std::size_t count, bool nested)
        : mStart(static_cast<std::ptrdiff_t>(count + 1))
        , mFinish(static_cast<std::ptrdiff_t>(count + 1))
        , mErrors(count)
    {
        if (count == 0 || (nested && count > pool.thread_count()))
        {
            throw std::invalid_argument("Caller count exceeds nested pool capacity");
        }
        mFutures.reserve(count);
        mThreads.reserve(count);
        std::size_t launched = 0;
        try
        {
            for (; launched < count; ++launched)
            {
                if (nested)
                {
                    mFutures.emplace_back(pool.submit([this, launched] {
                        work(launched);
                    }));
                }
                else
                {
                    mThreads.emplace_back([this, launched] {
                        work(launched);
                    });
                }
            }
        }
        catch (...)
        {
            for (; launched < count; ++launched)
            {
                mStart.arrive_and_drop();
                mFinish.arrive_and_drop();
            }
            stop();
            throw;
        }
    }
    ~Callers()
    {
        stop();
    }
    void run(const std::function<void()>& operation, std::size_t iterations)
    {
        mOperation = &operation;
        mIterations = iterations;
        mStart.arrive_and_wait();
        mFinish.arrive_and_wait();
        for (const auto& error : mErrors)
        {
            if (error)
            {
                std::rethrow_exception(error);
            }
        }
    }
    Callers(const Callers&) = delete;
    Callers& operator=(const Callers&) = delete;

private:
    void stop()
    {
        mStop = true;
        mStart.arrive_and_wait();
        for (auto& thread : mThreads)
        {
            thread.join();
        }
        for (auto& future : mFutures)
        {
            future.wait(); // Task exceptions are propagated by run(), not by cleanup.
        }
    }
    void work(std::size_t index)
    {
        for (;;)
        {
            mStart.arrive_and_wait();
            if (mStop)
            {
                return;
            }
            mErrors[index] = nullptr;
            try
            {
                for (std::size_t i = 0; i < mIterations; ++i)
                {
                    (*mOperation)();
                }
            }
            catch (...)
            {
                mErrors[index] = std::current_exception();
            }
            mFinish.arrive_and_wait();
        }
    }
    std::barrier<> mStart;
    std::barrier<> mFinish;
    std::vector<std::exception_ptr> mErrors;
    std::vector<std::thread> mThreads;
    std::vector<std::future<void>> mFutures;
    const std::function<void()>* mOperation = nullptr;
    std::size_t mIterations = 0;
    bool mStop = false;
};

struct Result
{
    Problem problem;
    std::size_t variant;
    std::vector<Sample> samples;
};
constexpr std::array<const char*, 3> names{"serial", "context_default", "context_forced"};

std::vector<Problem> problems()
{
    std::vector<Problem> result;
    for (std::size_t workers : {std::size_t{1}, std::size_t{2}, std::size_t{4}})
    {
        for (std::size_t n : {std::size_t{16}, std::size_t{32}, std::size_t{64}, std::size_t{128}, std::size_t{256}})
        {
            result.push_back(
                {"square" + std::to_string(n) + "/workers" + std::to_string(workers), n, n, n, 1, workers});
        }
    }
    for (std::size_t grain : {std::size_t{1}, std::size_t{8}, std::size_t{64}})
    {
        result.push_back({"grain" + std::to_string(grain), 128, 128, 128, 1, 4, grain});
    }
    result.push_back({"padded128", 128, 128, 128, 1, 4, 32, 1, false, true});
    result.push_back({"small_grain16", 16, 16, 16, 1, 4, 1});
    result.push_back({"small_grain32", 32, 32, 32, 1, 4, 1});
    result.push_back({"near_cutoff48", 48, 48, 48, 1, 4, 16});
    result.push_back({"batch_one_row", 1, 64, 64, 128, 4, 16});
    result.push_back({"concurrent2", 128, 128, 128, 1, 4, 32, 2});
    result.push_back({"nested4", 128, 128, 128, 1, 4, 32, 4, true});
    result.push_back({"spin2000_square32", 32, 32, 32, 1, 4, 1, 1, false, false, 2000});
    result.push_back({"spin2000_square64", 64, 64, 64, 1, 4, 32, 1, false, false, 2000});
    result.push_back({"spin2000_square128", 128, 128, 128, 1, 4, 32, 1, false, false, 2000});
    return result;
}

std::vector<Result>
measureProblem(const Problem& problem, const BenchConfig& config, std::size_t targetWork, std::mt19937_64& random)
{
    ThreadPool pool(problem.workers, problem.spinUs);
    Tensor<double> leftOwner({problem.batches, problem.rows, problem.inner * (problem.padded ? 2U : 1U)});
    Tensor<double> rightOwner({problem.batches, problem.inner, problem.columns});
    for (std::size_t i = 0; i < leftOwner.size(); ++i)
    {
        leftOwner[i] = static_cast<double>(static_cast<int>(i % 17) - 8) / 13.0;
    }
    for (std::size_t i = 0; i < rightOwner.size(); ++i)
    {
        rightOwner[i] = static_cast<double>(static_cast<int>(i % 11) - 5) / 7.0;
    }
    const auto leftShape = problem.batches == 1 ? fat_p::DynamicExtents{problem.rows, problem.inner}
                                                : fat_p::DynamicExtents{problem.batches, problem.rows, problem.inner};
    const auto rightShape = problem.batches == 1
                                ? fat_p::DynamicExtents{problem.inner, problem.columns}
                                : fat_p::DynamicExtents{problem.batches, problem.inner, problem.columns};
    const auto step = problem.padded ? std::ptrdiff_t{2} : std::ptrdiff_t{1};
    fat_p::TensorStrides leftStrides{static_cast<std::ptrdiff_t>(problem.inner) * step, step};
    fat_p::TensorStrides rightStrides{static_cast<std::ptrdiff_t>(problem.columns), 1};
    if (problem.batches != 1)
    {
        leftStrides.insert(leftStrides.begin(), static_cast<std::ptrdiff_t>(problem.rows * problem.inner) * step);
        rightStrides.insert(rightStrides.begin(), static_cast<std::ptrdiff_t>(problem.inner * problem.columns));
    }
    const auto left =
        fat_p::TensorView<const double>::borrow(leftOwner.data(),
                                                fat_p::TensorLayout(leftOwner.size(), 0, leftShape, leftStrides));
    const auto right =
        fat_p::TensorView<const double>::borrow(rightOwner.data(),
                                                fat_p::TensorLayout(rightOwner.size(), 0, rightShape, rightStrides));
    TensorExecutionOptions options;
    options.grainSize = problem.grain;
    const auto context = TensorExecutionContext::parallel(pool, options);
    options.minimumWork = 0;
    const auto forced = TensorExecutionContext::parallel(pool, options);
    const auto reference = fat_p::matmul(left, right);
    const auto verify = [&] {
        for (const auto& policy : {context, forced})
        {
            const auto check = fat_p::matmul(left, right, policy);
            for (std::size_t i = 0; i < reference.size(); ++i)
            {
                if (std::bit_cast<std::uint64_t>(reference[i]) != std::bit_cast<std::uint64_t>(check[i]))
                {
                    throw std::runtime_error("Serial/parallel mismatch: " + problem.name);
                }
            }
        }
    };
    verify();
    std::array<std::function<void()>, 3> operations{[&] {
                                                        auto value = fat_p::matmul(left, right);
                                                        observer(value.data(), value.size());
                                                    },
                                                    [&] {
                                                        auto value = fat_p::matmul(left, right, context);
                                                        observer(value.data(), value.size());
                                                    },
                                                    [&] {
                                                        auto value = fat_p::matmul(left, right, forced);
                                                        observer(value.data(), value.size());
                                                    }};
    std::unique_ptr<Callers> callers;
    if (problem.callers != 1)
    {
        callers = std::make_unique<Callers>(pool, problem.callers, problem.nested);
    }
    auto measure = [&](std::size_t variant, std::size_t iterations) {
        const auto cpu = fat_p::bench::capture_cpu_frequency();
        fat_p::bench::Timer timer;
        timer.start();
        if (callers)
        {
            callers->run(operations[variant], iterations);
        }
        else
        {
            for (std::size_t i = 0; i < iterations; ++i)
            {
                operations[variant]();
            }
        }
        return Sample{iterations, timer.elapsedNs(), cpu.mCurrentFreqMHz};
    };
    const auto work = problem.batches * problem.rows * problem.inner * problem.columns;
    std::array<std::size_t, 3> iterations;
    iterations.fill(std::max(std::size_t{1}, targetWork / work));
    std::array<std::size_t, 3> order{0, 1, 2};
    std::shuffle(order.begin(), order.end(), random);
    for (auto variant : order)
    {
        while (measure(variant, iterations[variant]).elapsedNs < static_cast<double>(config.minBatchMs) * 1e6)
        {
            if (iterations[variant] > 100000000)
            {
                throw std::runtime_error("Calibration overflow");
            }
            iterations[variant] *= 2;
        }
    }
    std::vector<Result> result{{problem, 0, {}}, {problem, 1, {}}, {problem, 2, {}}};
    for (std::size_t round = 0; round < config.warmupRuns + config.measuredRuns; ++round)
    {
        if (!config.noCooldown)
        {
            fat_p::bench::cooldownDelay(20, "round", config.verboseStats);
        }
        std::shuffle(order.begin(), order.end(), random);
        for (auto variant : order)
        {
            auto sample = measure(variant, iterations[variant]);
            if (round >= config.warmupRuns)
            {
                result[variant].samples.push_back(sample);
            }
        }
    }
    callers.reset(); // Release nested workers before the caller-thread correctness check.
    verify();
    return result;
}

Statistics statistics(const Result& result)
{
    std::vector<double> values;
    for (const auto& sample : result.samples)
    {
        values.push_back(sample.nsPerCall(result.problem.callers));
    }
    return Statistics::compute(values);
}

void exportResults(const BenchConfig& config, const std::vector<Result>& results, const std::string& cpu)
{
    const auto timestamp = fat_p::bench::current_timestamp();
    const auto compiler = fat_p::bench::detect_compiler();
    std::ofstream json, csv;
    if (!config.outputJson.empty())
    {
        json.open(config.outputJson);
        json.exceptions(std::ios::failbit | std::ios::badbit);
        json << std::setprecision(17)
             << "{\"schema_version\":1,\"benchmark\":\"TensorExecution\",\"timestamp\":" << std::quoted(timestamp)
             << ",\"compiler\":" << std::quoted(compiler) << ",\"cpu\":" << std::quoted(cpu)
             << ",\"seed\":" << config.seed << ",\"warmup\":" << config.warmupRuns
             << ",\"batches\":" << config.measuredRuns << ",\"min_batch_ms\":" << config.minBatchMs
             << ",\"quick\":" << (config.quick ? "true" : "false")
             << ",\"pinning\":false,\"max_workers\":4,\"dtype\":\"double\""
             << ",\"default_minimum_work\":" << TensorExecutionOptions{}.minimumWork
             << ",\"cache_regime\":\"reused_inputs_no_flush\",\"statistics_kind\":\"batch_mean_dispersion\""
             << ",\"unit\":\"ns/completed_call\",\"cases\":[\n";
    }
    if (!config.outputCsv.empty())
    {
        csv.open(config.outputCsv);
        csv.exceptions(std::ios::failbit | std::ios::badbit);
        csv << "case,variant,workers,callers,grain,rows,inner,columns,batches,nested,padded,pool_spin_us,sample,"
               "iterations,"
               "elapsed_ns,ns_per_call,cpu_mhz,median,mean,stddev,ci95_mean_low,ci95_mean_high,throughput_calls_s\n";
        csv << std::setprecision(17);
    }
    for (std::size_t index = 0; index < results.size(); ++index)
    {
        const auto& result = results[index];
        const auto& p = result.problem;
        const auto stats = statistics(result);
        std::cout << p.name << ' ' << names[result.variant] << " median=" << stats.median
                  << " ns/completed_call throughput=" << 1e9 / stats.median << " calls/s\n";
        if (json.is_open())
        {
            if (index != 0)
            {
                json << ",\n";
            }
            json << "{\"name\":" << std::quoted(p.name) << ",\"variant\":" << std::quoted(names[result.variant])
                 << ",\"workers\":" << p.workers << ",\"callers\":" << p.callers << ",\"grain\":" << p.grain
                 << ",\"pool_spin_us\":" << p.spinUs << ",\"rows\":" << p.rows << ",\"inner\":" << p.inner
                 << ",\"columns\":" << p.columns << ",\"batches\":" << p.batches
                 << ",\"nested\":" << (p.nested ? "true" : "false") << ",\"padded\":" << (p.padded ? "true" : "false")
                 << ",\"median\":" << stats.median << ",\"mean\":" << stats.mean << ",\"stddev\":" << stats.stddev
                 << ",\"ci95_mean_low\":" << stats.ci95Low << ",\"ci95_mean_high\":" << stats.ci95High
                 << ",\"throughput_calls_s\":" << 1e9 / stats.median << ",\"samples\":[";
        }
        for (std::size_t s = 0; s < result.samples.size(); ++s)
        {
            const auto& sample = result.samples[s];
            if (json.is_open())
            {
                if (s != 0)
                {
                    json << ',';
                }
                json << "{\"iterations\":" << sample.iterations << ",\"elapsed_ns\":" << sample.elapsedNs
                     << ",\"ns_per_call\":" << sample.nsPerCall(p.callers) << ",\"cpu_mhz\":" << sample.cpuMhz << '}';
            }
            if (csv.is_open())
            {
                csv << p.name << ',' << names[result.variant] << ',' << p.workers << ',' << p.callers << ',' << p.grain
                    << ',' << p.rows << ',' << p.inner << ',' << p.columns << ',' << p.batches << ',' << p.nested << ','
                    << p.padded << ',' << p.spinUs << ',' << s << ',' << sample.iterations << ',' << sample.elapsedNs
                    << ',' << sample.nsPerCall(p.callers) << ',' << sample.cpuMhz << ',' << stats.median << ','
                    << stats.mean << ',' << stats.stddev << ',' << stats.ci95Low << ',' << stats.ci95High << ','
                    << 1e9 / stats.median << '\n';
            }
        }
        if (json.is_open())
        {
            json << "]}";
        }
    }
    if (json.is_open())
    {
        json << "\n]}\n";
        json.close();
    }
    if (csv.is_open())
    {
        csv.close();
    }
}

int run(const std::string& filter)
{
    const auto config = BenchConfig::fromEnv();
    const auto targetWork = fat_p::bench::detail::getEnvSizeT("FATP_BENCH_TARGET_WORK", 10000);
    if (!config.warmupRuns || !config.measuredRuns || !config.minBatchMs || !targetWork || config.warmupRuns > 100 ||
        config.measuredRuns > 1000 || config.minBatchMs > 10000 || targetWork > 1000000000 ||
        (!config.quick && config.measuredRuns < 7))
    {
        throw std::invalid_argument("Invalid benchmark configuration");
    }
    fat_p::bench::HeaderConfig header;
    header.component = "TensorExecution";
    header.warmup = config.warmupRuns;
    header.measured = config.measuredRuns;
    header.seed = config.seed;
    header.scope_enabled = false;
    header.stabilize_enabled = !config.noStabilize;
    header.cooldown_enabled = !config.noCooldown;
    header.has_stabilization = !config.noStabilize;
    header.competitors = {{"Tensor serial", true, "matched baseline"},
                          {"explicit context", true, "default cutoff and forced scheduling"}};
    fat_p::bench::print_standard_header(header);
    std::cout << "Workers=1,2,4 (deliberately capped); pool spin=0us plus default-2000us probes.\n"
                 "No affinity or priority override; FATP_BENCH_NO_SCOPE is immaterial to this suite.\n"
                 "Reused inputs; fresh result allocation/init/destruction and scheduling included.\n"
                 "Pool/caller construction excluded; concurrent batches include start/finish barriers.\n"
                 "All output bits checked outside timing. Same kernel and result ownership in every path.\n"
                 "No cold-cache claim. Batch-mean dispersion is NOT per-call p95/p99 latency.\n"
                 "Concurrent rows are inverse throughput per completed call, not individual-call latency.\n";
    if (!config.noStabilize)
    {
        fat_p::bench::waitForStableCpu(config);
    }
    std::ostringstream cpu;
    fat_p::bench::print_cpu_context(cpu, "execution benchmark");
    auto cpuText = cpu.str();
    cpuText.erase(std::remove(cpuText.begin(), cpuText.end(), '\n'), cpuText.end());
    cpuText.erase(std::remove(cpuText.begin(), cpuText.end(), '\r'), cpuText.end());
    std::mt19937_64 random(config.seed);
    auto cases = problems();
    std::shuffle(cases.begin(), cases.end(), random);
    std::vector<Result> results;
    for (const auto& problem : cases)
    {
        if (problem.name.find(filter) == std::string::npos)
        {
            continue;
        }
        std::cout << "Measuring " << problem.name << std::endl;
        auto measured = measureProblem(problem, config, targetWork, random);
        results.insert(results.end(),
                       std::make_move_iterator(measured.begin()),
                       std::make_move_iterator(measured.end()));
    }
    if (results.empty())
    {
        throw std::invalid_argument("No matching cases");
    }
    exportResults(config, results, cpuText);
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
        if (argc == 1)
        {
            return run("");
        }
        if (argc == 3 && std::string(argv[1]) == "--filter")
        {
            return run(argv[2]);
        }
        throw std::invalid_argument("Usage: benchmark_TensorExecution [--filter substring]");
    }
    catch (const std::exception& error)
    {
        std::cerr << "BENCHMARK FAILED: " << error.what() << '\n';
        return 1;
    }
}
