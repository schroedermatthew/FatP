/**
 * @file benchmark_FloatingPointComparison.cpp
 * @brief Production benchmarks for FloatingPointComparison.h
 * @layer Testing
 *
 * Compile (minimal):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_FloatingPointComparison.cpp -o bench_fp
 *
 * Windows (MSVC):
 *   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_FloatingPointComparison.cpp
 *
 * Environment variables:
 *   FATP_BENCH_NO_SCOPE=1    Disable Windows priority/affinity
 *   FATP_BENCH_BATCHES=50    Override batch count
 */

#include "FloatingPointComparison.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// ============================================================================
// Platform Configuration
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#define FATP_WINDOWS 1
#include <windows.h>
static constexpr size_t WARMUP_RUNS = 3;
static constexpr size_t DEFAULT_MEASURED_RUNS = 15;
#else
#define FATP_WINDOWS 0
static constexpr size_t WARMUP_RUNS = 3;
static constexpr size_t DEFAULT_MEASURED_RUNS = 50;
#endif

// ============================================================================
// CPU Frequency Monitoring
// ============================================================================

struct CpuFreqInfo
{
    double ref_freq_mhz = 0;
    double current_freq_mhz = 0;
    bool ref_is_max = false;  // True if using max_freq fallback

    double throttle_percentage() const
    {
        if (current_freq_mhz <= 0 || ref_freq_mhz <= 0) return 0;
        return (1.0 - current_freq_mhz / ref_freq_mhz) * 100.0;
    }

    bool is_throttled() const { return !ref_is_max && throttle_percentage() > 5.0; }
    bool is_turbo() const { return !ref_is_max && current_freq_mhz > ref_freq_mhz * 1.05; }
};

#if FATP_WINDOWS
CpuFreqInfo get_cpu_freq()
{
    CpuFreqInfo info;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD mhz = 0, size = sizeof(mhz);
        if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS)
        {
            info.current_freq_mhz = static_cast<double>(mhz);
            info.ref_freq_mhz = info.current_freq_mhz;
            info.ref_is_max = false;
        }
        RegCloseKey(hKey);
    }
    return info;
}
#else
#include <fstream>
CpuFreqInfo get_cpu_freq()
{
    CpuFreqInfo info;
    auto read_khz = [](const char* path) -> double {
        std::ifstream f(path);
        double khz = 0;
        if (f >> khz) return khz / 1000.0;
        return 0;
    };

    info.current_freq_mhz = read_khz("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    info.ref_freq_mhz = read_khz("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency");
    
    if (info.ref_freq_mhz <= 0)
    {
        info.ref_freq_mhz = read_khz("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        info.ref_is_max = true;
    }
    return info;
}
#endif

void print_cpu_context()
{
    auto info = get_cpu_freq();
    std::cout << "CPU: " << static_cast<int>(info.current_freq_mhz) << " MHz";
    if (info.ref_freq_mhz > 0)
    {
        const char* ref_label = info.ref_is_max ? "max" : "base";
        std::cout << " (" << ref_label << ": " << static_cast<int>(info.ref_freq_mhz) << ")";
        
        if (!info.ref_is_max)
        {
            if (info.is_throttled())
                std::cout << " [THROTTLED " << std::fixed << std::setprecision(0) 
                          << info.throttle_percentage() << "%]";
            else if (info.is_turbo())
                std::cout << " [TURBO]";
        }
    }
    std::cout << "\n";
}

// ============================================================================
// BenchmarkScope (Windows priority/affinity)
// ============================================================================

#if FATP_WINDOWS
static inline bool has_env_var(const char* name)
{
    char buf[2];
    return GetEnvironmentVariableA(name, buf, sizeof(buf)) > 0;
}

class BenchmarkScope
{
    DWORD old_priority_ = 0;
    DWORD_PTR old_affinity_ = 0;
    bool applied_ = false;

public:
    explicit BenchmarkScope(bool verbose = false)
    {
        if (has_env_var("FATP_BENCH_NO_SCOPE")) return;

        HANDLE proc = GetCurrentProcess();
        old_priority_ = GetPriorityClass(proc);
        SetPriorityClass(proc, HIGH_PRIORITY_CLASS);

        HANDLE thread = GetCurrentThread();
        DWORD_PTR proc_mask = 0, sys_mask = 0;
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
            std::cout << "[BenchmarkScope] High priority, CPU affinity set\n";
    }

    ~BenchmarkScope()
    {
        if (!applied_) return;
        SetPriorityClass(GetCurrentProcess(), old_priority_);
        if (old_affinity_ != 0)
            SetThreadAffinityMask(GetCurrentThread(), old_affinity_);
    }

    BenchmarkScope(const BenchmarkScope&) = delete;
    BenchmarkScope& operator=(const BenchmarkScope&) = delete;
};
#else
class BenchmarkScope
{
public:
    explicit BenchmarkScope(bool = false) {}
};
#endif

// ============================================================================
// Timer
// ============================================================================

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
        if (samples.empty()) return s;

        std::sort(samples.begin(), samples.end());
        size_t n = samples.size();

        s.min = samples.front();
        s.max = samples.back();

        if (n % 2 == 1)
            s.median = samples[n / 2];
        else
            s.median = 0.5 * (samples[n / 2 - 1] + samples[n / 2]);

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

// ============================================================================
// Dead Code Elimination Prevention
// ============================================================================

static volatile int64_t benchmark_sink = 0;

template <typename T>
inline void DoNotOptimize(T const& value)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "r,m"(value) : "memory");
#else
    benchmark_sink += static_cast<int64_t>(value);
#endif
}

// ============================================================================
// Data Generation
// ============================================================================

struct TestData
{
    std::vector<double> values_a;
    std::vector<double> values_b;
    double epsilon = 1e-9;
};

TestData generate_normal_pairs(size_t n, uint64_t seed = 12345)
{
    TestData data;
    data.values_a.reserve(n);
    data.values_b.reserve(n);
    
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);
    std::uniform_real_distribution<double> noise(-1e-10, 1e-10);
    
    for (size_t i = 0; i < n; ++i)
    {
        double v = dist(rng);
        data.values_a.push_back(v);
        data.values_b.push_back(v + noise(rng));
    }
    return data;
}

TestData generate_multiscale_pairs(size_t n, uint64_t seed = 12345)
{
    TestData data;
    data.values_a.reserve(n);
    data.values_b.reserve(n);
    
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> exp_dist(-20, 20);
    std::uniform_real_distribution<double> mant_dist(1.0, 10.0);
    std::uniform_real_distribution<double> noise(-1e-10, 1e-10);
    
    for (size_t i = 0; i < n; ++i)
    {
        double v = mant_dist(rng) * std::pow(10.0, exp_dist(rng));
        data.values_a.push_back(v);
        data.values_b.push_back(v * (1.0 + noise(rng)));
    }
    return data;
}

TestData generate_special_values(size_t n, bool nan_values)
{
    TestData data;
    data.values_a.resize(n);
    data.values_b.resize(n);
    
    double special = nan_values ? std::nan("") : std::numeric_limits<double>::infinity();
    std::fill(data.values_a.begin(), data.values_a.end(), special);
    std::fill(data.values_b.begin(), data.values_b.end(), special);
    return data;
}

// ============================================================================
// Baseline Implementations (what users write without Fat-P)
// ============================================================================

namespace baseline {

bool manual_absolute(double a, double b, double eps)
{
    return std::fabs(a - b) <= eps;
}

bool manual_relative(double a, double b, double eps)
{
    return std::fabs(a - b) <= eps * std::max(std::fabs(a), std::fabs(b));
}

bool manual_hybrid(double a, double b, double rel_eps, double abs_eps)
{
    double diff = std::fabs(a - b);
    if (diff <= abs_eps) return true;
    return diff <= rel_eps * std::max(std::fabs(a), std::fabs(b));
}

} // namespace baseline

// ============================================================================
// Benchmark Runner
// ============================================================================

template <typename Func>
Statistics run_benchmark(const char* name, size_t ops_per_batch, 
                         size_t measured_runs, Func&& func)
{
    // Warmup
    for (size_t i = 0; i < WARMUP_RUNS; ++i)
    {
        func();
    }

    // Measured runs
    std::vector<double> samples;
    samples.reserve(measured_runs);

    for (size_t run = 0; run < measured_runs; ++run)
    {
        Timer t;
        t.start();
        func();
        double elapsed = t.elapsed_ns();
        samples.push_back(elapsed / static_cast<double>(ops_per_batch));
    }

    return Statistics::compute(samples);
}

void print_stats(const char* name, const Statistics& s)
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << std::setw(30) << std::left << name
              << std::setw(10) << s.median << " ns"
              << "  (mean: " << s.mean << ", stddev: " << s.stddev << ")"
              << "  CI95: [" << s.ci95_low << ", " << s.ci95_high << "]\n";
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    BenchmarkScope scope(true);
    
    std::cout << "================================================================\n";
    std::cout << "  FloatingPointComparison Benchmark\n";
    std::cout << "================================================================\n\n";

    print_cpu_context();
    
    constexpr size_t N = 1'000'000;
    size_t measured_runs = DEFAULT_MEASURED_RUNS;
    
    std::cout << "\nConfiguration:\n";
    std::cout << "  Operations per batch: " << N << "\n";
    std::cout << "  Measured runs: " << measured_runs << "\n";
    std::cout << "  Warmup runs: " << WARMUP_RUNS << "\n\n";

    // Generate test data
    auto normal_data = generate_normal_pairs(N);
    auto multiscale_data = generate_multiscale_pairs(N);
    auto nan_data = generate_special_values(N, true);
    auto inf_data = generate_special_values(N, false);

    // ========================================================================
    std::cout << "--- Fat-P Policies vs Manual Baseline ---\n\n";
    print_cpu_context();
    
    // Standard vs manual_absolute
    {
        auto s1 = run_benchmark("Fat-P Standard", N, measured_runs, [&]() {
            size_t count = 0;
            for (size_t i = 0; i < N; ++i)
            {
                if (fat_p::floatEqual<double, fat_p::StandardComparisonPolicy>(
                        normal_data.values_a[i], normal_data.values_b[i], 1e-9))
                    ++count;
            }
            DoNotOptimize(count);
        });
        
        auto s2 = run_benchmark("Manual absolute", N, measured_runs, [&]() {
            size_t count = 0;
            for (size_t i = 0; i < N; ++i)
            {
                if (baseline::manual_absolute(
                        normal_data.values_a[i], normal_data.values_b[i], 1e-9))
                    ++count;
            }
            DoNotOptimize(count);
        });
        
        print_stats("Fat-P Standard", s1);
        print_stats("Manual absolute", s2);
        std::cout << "  Ratio: " << std::fixed << std::setprecision(2) 
                  << (s1.median / s2.median) << "x\n\n";
    }

    // Hybrid vs manual_hybrid
    {
        auto s1 = run_benchmark("Fat-P Hybrid", N, measured_runs, [&]() {
            size_t count = 0;
            for (size_t i = 0; i < N; ++i)
            {
                if (fat_p::approximateEqual(
                        normal_data.values_a[i], normal_data.values_b[i], 1e-9, 1e-12))
                    ++count;
            }
            DoNotOptimize(count);
        });
        
        auto s2 = run_benchmark("Manual hybrid", N, measured_runs, [&]() {
            size_t count = 0;
            for (size_t i = 0; i < N; ++i)
            {
                if (baseline::manual_hybrid(
                        normal_data.values_a[i], normal_data.values_b[i], 1e-9, 1e-12))
                    ++count;
            }
            DoNotOptimize(count);
        });
        
        print_stats("Fat-P Hybrid", s1);
        print_stats("Manual hybrid", s2);
        std::cout << "  Ratio: " << std::fixed << std::setprecision(2) 
                  << (s1.median / s2.median) << "x\n\n";
    }

    // ========================================================================
    std::cout << "--- Policy Comparison (Normal Values) ---\n\n";
    print_cpu_context();
    
    auto s_standard = run_benchmark("Standard", N, measured_runs, [&]() {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i)
        {
            if (fat_p::floatEqual<double, fat_p::StandardComparisonPolicy>(
                    normal_data.values_a[i], normal_data.values_b[i], 1e-9))
                ++count;
        }
        DoNotOptimize(count);
    });
    
    auto s_relative = run_benchmark("Relative", N, measured_runs, [&]() {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i)
        {
            if (fat_p::floatEqual<double, fat_p::RelativeComparisonPolicy>(
                    normal_data.values_a[i], normal_data.values_b[i], 1e-9))
                ++count;
        }
        DoNotOptimize(count);
    });
    
    auto s_ulp = run_benchmark("ULP", N, measured_runs, [&]() {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i)
        {
            if (fat_p::floatEqual<double, fat_p::UlpComparisonPolicy>(
                    normal_data.values_a[i], normal_data.values_b[i], 4.0))
                ++count;
        }
        DoNotOptimize(count);
    });
    
    auto s_hybrid = run_benchmark("Hybrid", N, measured_runs, [&]() {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i)
        {
            if (fat_p::approximateEqual(
                    normal_data.values_a[i], normal_data.values_b[i]))
                ++count;
        }
        DoNotOptimize(count);
    });
    
    print_stats("Standard", s_standard);
    print_stats("Relative", s_relative);
    print_stats("ULP", s_ulp);
    print_stats("Hybrid", s_hybrid);

    // ========================================================================
    std::cout << "\n--- Special Value Handling ---\n\n";
    print_cpu_context();
    
    auto s_nan = run_benchmark("NaN", N, measured_runs, [&]() {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i)
        {
            if (fat_p::approximateEqual(nan_data.values_a[i], nan_data.values_b[i]))
                ++count;
        }
        DoNotOptimize(count);
    });
    
    auto s_inf = run_benchmark("Infinity", N, measured_runs, [&]() {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i)
        {
            if (fat_p::approximateEqual(inf_data.values_a[i], inf_data.values_b[i]))
                ++count;
        }
        DoNotOptimize(count);
    });
    
    print_stats("NaN", s_nan);
    print_stats("Infinity", s_inf);
    std::cout << "  Speedup vs normal: " << std::fixed << std::setprecision(1)
              << (s_hybrid.median / s_nan.median) << "x (NaN), "
              << (s_hybrid.median / s_inf.median) << "x (Inf)\n";

    // ========================================================================
    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================\n";

    return 0;
}
