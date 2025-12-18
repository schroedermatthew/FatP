/**
 * @file benchmark_StableHashMap.cpp
 * @brief Comprehensive benchmark suite for StableHashMap
 *
 * Architecture: Round-robin execution with randomized order per run.
 * This ensures all libraries observe the same distribution of machine states,
 * eliminating drift-induced unfairness.
 *
 * Design Invariants:
 *   1. Each measured run executes exactly one timed iteration per library.
 *   2. Library execution order is randomized per run.
 *   3. Setup, reserve, and teardown occur outside timed regions.
 *      Note: Insert benchmark is "amortized" - includes any growth from reserve(N).
 *   4. All libraries observe the same distribution of machine states.
 *   5. Medians are the primary reported statistic.
 *
 * Sections:
 *   1. Core operations (StableHashMap vs tsl vs ankerl vs absl vs std)
 *   2. Pathological erase (tombstone degradation test)
 *   3. Hash quality impact (std::hash vs SplitMix64)
 *   4. String heterogeneous lookup
 *   5. Load factor sensitivity
 *
 * Compile (minimal - StableHashMap only):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native -I. \
 *       benchmark_StableHashMap.cpp -o benchmark_StableHashMap
 *
 * Compile (with tsl and ankerl - auto-detected via __has_include):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native -I. \
 *       benchmark_StableHashMap.cpp -o benchmark_StableHashMap
 *
 * Compile (with absl - requires explicit flag and linking):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native -DUSE_ABSL=1 \
 *       -I. -I./abseil-cpp-master \
 *       benchmark_StableHashMap.cpp \
 *       -Wl,--whole-archive $(find ./abseil-cpp-master/build -name "*.a") \
 *       -Wl,--no-whole-archive -lpthread -o benchmark_StableHashMap
 *
 * Compile (Windows, MSVC):
 *   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_StableHashMap.cpp
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fatp/StableHashMap.h"

// Optional: Include competitor headers if available
#if __has_include("tsl/robin_map.h")
#include "tsl/robin_map.h"
#define HAS_TSL 1
#else
#define HAS_TSL 0
#endif

#if __has_include("ankerl/unordered_dense.h")
#include "ankerl/unordered_dense.h"
#define HAS_ANKERL 1
#else
#define HAS_ANKERL 0
#endif

// absl requires explicit linking - use -DUSE_ABSL=1 to enable
#if defined(USE_ABSL) && USE_ABSL && __has_include("absl/container/flat_hash_map.h")
#include "absl/container/flat_hash_map.h"
#define HAS_ABSL 1
#else
#define HAS_ABSL 0
#endif

// ============================================================================
// Platform Configuration
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <intrin.h>
#include <windows.h>
#include <winreg.h>  // For RegOpenKeyExA, RegQueryValueExA, RegCloseKey
static constexpr size_t WARMUP_RUNS = 3;
static constexpr size_t MEASURED_RUNS = 15;
#else
#include <fstream>
static constexpr size_t WARMUP_RUNS = 3;
static constexpr size_t MEASURED_RUNS = 50;
#endif

// ============================================================================
// CPU Frequency Monitoring (Platform-Specific)
// ============================================================================

struct CpuFreqInfo
{
    double ref_freq_mhz = 0;     // Reference frequency (base or max, depending on availability)
    double current_freq_mhz = 0;
    bool ref_is_max = false;    // True if ref_freq is max_freq (not reliable for throttle detection)

    double throttle_percentage() const
    {
        if (current_freq_mhz <= 0 || ref_freq_mhz <= 0) return 0;
        return (1.0 - current_freq_mhz / ref_freq_mhz) * 100.0;
    }

    // Only report throttle/turbo if we have reliable base frequency (not max fallback)
    bool is_throttled() const { return !ref_is_max && throttle_percentage() > 5.0; }
    bool is_turbo() const { return !ref_is_max && current_freq_mhz > ref_freq_mhz * 1.05; }
};

#if defined(_WIN32) || defined(_WIN64)
CpuFreqInfo get_cpu_freq()
{
    CpuFreqInfo info;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD freq = 0, size = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&freq), &size) == ERROR_SUCCESS)
        {
            info.ref_freq_mhz = static_cast<double>(freq);
            info.current_freq_mhz = info.ref_freq_mhz;
        }
        RegCloseKey(hKey);
    }
    return info;
}
#else
CpuFreqInfo get_cpu_freq()
{
    CpuFreqInfo info;
    std::ifstream f("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (f.is_open())
    {
        int64_t khz;
        if (f >> khz)
        {
            info.current_freq_mhz = khz / 1000.0;
        }
    }
    std::ifstream f2("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency");
    if (f2.is_open())
    {
        int64_t khz;
        if (f2 >> khz)
        {
            info.ref_freq_mhz = khz / 1000.0;
            info.ref_is_max = false;  // True base frequency
        }
    }
    else
    {
        // Fallback to max_freq - this is turbo frequency, not base
        // Throttle/turbo detection will be disabled
        std::ifstream f3("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        if (f3.is_open())
        {
            int64_t khz;
            if (f3 >> khz)
            {
                info.ref_freq_mhz = khz / 1000.0;
                info.ref_is_max = true;  // Mark as unreliable for throttle detection
            }
        }
    }
    return info;
}
#endif

void print_cpu_context(const char* label = nullptr)
{
    auto info = get_cpu_freq();
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif

    std::cout << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] ";
    if (label) std::cout << label << " ";
    std::cout << "CPU: " << static_cast<int>(info.current_freq_mhz) << " MHz";
    if (info.ref_freq_mhz > 0)
    {
        const char* ref_label = info.ref_is_max ? "max" : "base";
        std::cout << " (" << ref_label << ": " << static_cast<int>(info.ref_freq_mhz) << ")";
        if (info.is_throttled())
        {
            std::cout << " [THROTTLED " << std::fixed << std::setprecision(1)
                << info.throttle_percentage() << "%]";
        }
        else if (info.is_turbo())
        {
            std::cout << " [TURBO]";
        }
    }
    std::cout << "\n";
}

// ============================================================================
// Benchmark Environment Configuration (Windows-specific optimizations)
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
class BenchmarkScope
{
    DWORD old_priority_ = 0;
    DWORD_PTR old_affinity_ = 0;
    bool restored_ = false;

public:
    explicit BenchmarkScope(bool verbose = false)
    {
        HANDLE proc = GetCurrentProcess();
        old_priority_ = GetPriorityClass(proc);
        SetPriorityClass(proc, HIGH_PRIORITY_CLASS);

        HANDLE thread = GetCurrentThread();
        
        // Avoid Core 0: often OS/interrupt heavy on Windows
        DWORD_PTR proc_mask = 0, sys_mask = 0;
        DWORD_PTR target = 1;
        if (GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask) && proc_mask)
        {
            DWORD_PTR nonzero = proc_mask & ~static_cast<DWORD_PTR>(1);
            DWORD_PTR pick = nonzero ? nonzero : proc_mask;
            target = pick & (~pick + 1); // lowest set bit
        }
        old_affinity_ = SetThreadAffinityMask(thread, target);

        if (verbose)
        {
            std::cout << "[BenchmarkScope] High priority, CPU" 
                      << (target > 1 ? "non-0" : "0") << " affinity\n";
        }
    }

    ~BenchmarkScope()
    {
        if (!restored_)
        {
            HANDLE proc = GetCurrentProcess();
            SetPriorityClass(proc, old_priority_);
            HANDLE thread = GetCurrentThread();
            if (old_affinity_ != 0)  // Only restore if SetThreadAffinityMask succeeded
            {
                SetThreadAffinityMask(thread, old_affinity_);
            }
        }
    }
};
#else
class BenchmarkScope
{
public:
    explicit BenchmarkScope(bool = false) {}
};
#endif

static inline bool has_env_var(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
    char buf[2];
    return GetEnvironmentVariableA(name, buf, sizeof(buf)) > 0;
#else
    return std::getenv(name) != nullptr;
#endif
}

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

static inline double ns_per_op(double elapsed_ns, size_t ops)
{
    return (ops == 0) ? 0.0 : (elapsed_ns / static_cast<double>(ops));
}

// Prevent dead code elimination
static volatile int64_t benchmark_sink = 0;

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

        // Median
        if (n % 2 == 1)
        {
            s.median = samples[n / 2];
        }
        else
        {
            s.median = 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
        }

        // Mean
        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        s.mean = sum / static_cast<double>(n);

        // Stddev (sample)
        if (n > 1)
        {
            double acc = 0.0;
            for (double x : samples)
            {
                double d = x - s.mean;
                acc += d * d;
            }
            s.stddev = std::sqrt(acc / static_cast<double>(n - 1));

            // 95% Confidence Interval using Normal Distribution approximation
            // Assumes sample means are approximately normally distributed (CLT).
            // z = 1.96 corresponds to 95% CI for standard normal distribution.
            // CI = mean +/- z * (stddev / sqrt(n))
            double se = s.stddev / std::sqrt(static_cast<double>(n));
            constexpr double z = 1.96;  // 95% CI critical value for normal distribution
            s.ci95_low = s.mean - z * se;
            s.ci95_high = s.mean + z * se;
        }

        return s;
    }

    void print(const char* label) const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  " << std::setw(12) << label << ": "
            << "median=" << std::setw(8) << median
            << " mean=" << std::setw(8) << mean
            << " +/-" << std::setw(6) << stddev
            << " CI95(mean)=[" << ci95_low << "," << ci95_high << "]"
            << " min=" << min << " max=" << max << "\n";
    }
};

// ============================================================================
// SplitMix64 Hash (high-quality 64-bit mixer)
// ============================================================================

struct SplitMix64Hash
{
    size_t operator()(int64_t x) const noexcept
    {
        uint64_t z = static_cast<uint64_t>(x);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return static_cast<size_t>(z ^ (z >> 31));
    }
};

// ============================================================================
// Transparent Hash/Equal for String Heterogeneous Lookup
// ============================================================================

struct TransparentStringHash
{
    using is_transparent = void;

    size_t operator()(std::string_view sv) const noexcept
    {
        return std::hash<std::string_view>{}(sv);
    }

    size_t operator()(const std::string& s) const noexcept
    {
        return std::hash<std::string_view>{}(s);
    }

    size_t operator()(const char* s) const noexcept
    {
        return std::hash<std::string_view>{}(s);
    }
};

struct TransparentStringEqual
{
    using is_transparent = void;

    bool operator()(std::string_view a, std::string_view b) const noexcept
    {
        return a == b;
    }
};

// ============================================================================
// Test Data Generation
// ============================================================================

std::vector<int64_t> generate_random_keys(size_t n, uint64_t seed = 12345)
{
    std::vector<int64_t> keys(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    for (size_t i = 0; i < n; ++i)
    {
        keys[i] = dist(rng);
    }
    return keys;
}

std::vector<int64_t> generate_missing_keys(size_t n, uint64_t seed = 99999)
{
    std::vector<int64_t> keys(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int64_t> dist(INT64_MIN, -1);
    for (size_t i = 0; i < n; ++i)
    {
        keys[i] = dist(rng);
    }
    return keys;
}

std::vector<std::string> generate_string_keys(size_t n)
{
    std::vector<std::string> v;
    v.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        v.emplace_back("config.section.subsection.item." + std::to_string(i));
    }
    return v;
}

// ============================================================================
// Benchmark Case Enum
// ============================================================================

enum class Case
{
    Insert,
    FindHit,
    FindMiss,
    Erase,
    Churn
};

static inline const char* case_name(Case c)
{
    switch (c)
    {
    case Case::Insert:   return "Insert (amortized)";
    case Case::FindHit:  return "Find(hit)";
    case Case::FindMiss: return "Find(miss)";
    case Case::Erase:    return "Erase";
    case Case::Churn:    return "Churn";
    }
    return "Unknown";
}

// ============================================================================
// Shared Inputs (generated once, reused across all libraries and runs)
// ============================================================================

struct Inputs
{
    std::vector<int64_t> keys;
    std::vector<int64_t> miss_keys;
    std::vector<int64_t> erase_order;

    static Inputs make(size_t N, uint64_t seed = 0xC0FFEEULL)
    {
        Inputs in;
        in.keys = generate_random_keys(N, seed);
        in.miss_keys = generate_missing_keys(N, seed ^ 0x12345);
        in.erase_order = in.keys;
        std::mt19937_64 rng(seed ^ 0x9E37);
        std::shuffle(in.erase_order.begin(), in.erase_order.end(), rng);
        return in;
    }
};

// ============================================================================
// Map Adapter Interface
// ============================================================================
// Each library is wrapped by a thin adapter. Adapters contain:
//   - no statistics
//   - no timing logic
//   - no policy decisions
// They are dumb, mechanical mappings.

struct IMapAdapter
{
    virtual ~IMapAdapter() = default;

    virtual const char* name() const = 0;

    // setup/teardown must be OUTSIDE timing
    virtual void setup(size_t N, const Inputs& in) = 0;
    virtual void teardown() = 0;

    // preload for cases that need a populated map (find, erase, churn)
    virtual void preload(const Inputs& in) = 0;

    // run_operation does ONLY the measured work
    // returns ops_executed for ns/op calculation
    virtual size_t run_operation(Case c, const Inputs& in) = 0;
};

// ============================================================================
// StableHashMap Adapter
// ============================================================================

template <typename Policy = fat_p::DefaultPolicy<int64_t, int64_t>>
class StableHashMapAdapter final : public IMapAdapter
{
    std::string name_;
    std::unique_ptr<fat_p::StableHashMap<int64_t, int64_t, Policy>> map_;

public:
    explicit StableHashMapAdapter(const char* name) : name_(name) {}

    const char* name() const override { return name_.c_str(); }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<fat_p::StableHashMap<int64_t, int64_t, Policy>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert(k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::Insert:
            for (int64_t k : in.keys)
            {
                map_->insert(k, k);
                ++ops;
            }
            break;

        case Case::FindHit:
            for (int64_t k : in.keys)
            {
                auto* v = map_->find(k);
                if (v) benchmark_sink += *v;
                ++ops;
            }
            break;

        case Case::FindMiss:
            for (int64_t k : in.miss_keys)
            {
                auto* v = map_->find(k);
                if (v) benchmark_sink += *v;
                ++ops;
            }
            break;

        case Case::Erase:
            for (int64_t k : in.erase_order)
            {
                map_->erase(k);
                ++ops;
            }
            break;

        case Case::Churn:
            for (size_t i = 0; i < in.erase_order.size(); ++i)
            {
                map_->erase(in.erase_order[i]);
                map_->try_emplace(in.erase_order[i], in.erase_order[i]);
                ops += 2;
            }
            break;
        }
        return ops;
    }
};

// ============================================================================
// std::unordered_map Adapter
// ============================================================================

class StdUnorderedMapAdapter final : public IMapAdapter
{
    std::unique_ptr<std::unordered_map<int64_t, int64_t>> map_;

public:
    const char* name() const override { return "std::unordered_map"; }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<std::unordered_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->emplace(k, k);
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::Insert:
            for (int64_t k : in.keys)
            {
                map_->emplace(k, k);
                ++ops;
            }
            break;

        case Case::FindHit:
            for (int64_t k : in.keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::FindMiss:
            for (int64_t k : in.miss_keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::Erase:
            for (int64_t k : in.erase_order)
            {
                map_->erase(k);
                ++ops;
            }
            break;

        case Case::Churn:
            for (size_t i = 0; i < in.erase_order.size(); ++i)
            {
                map_->erase(in.erase_order[i]);
                map_->try_emplace(in.erase_order[i], in.erase_order[i]);
                ops += 2;
            }
            break;
        }
        return ops;
    }
};

// ============================================================================
// tsl::robin_map Adapter
// ============================================================================

#if HAS_TSL
class TslRobinMapAdapter final : public IMapAdapter
{
    std::unique_ptr<tsl::robin_map<int64_t, int64_t>> map_;

public:
    const char* name() const override { return "tsl::robin_map"; }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<tsl::robin_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::Insert:
            for (int64_t k : in.keys)
            {
                map_->insert({k, k});
                ++ops;
            }
            break;

        case Case::FindHit:
            for (int64_t k : in.keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::FindMiss:
            for (int64_t k : in.miss_keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::Erase:
            for (int64_t k : in.erase_order)
            {
                map_->erase(k);
                ++ops;
            }
            break;

        case Case::Churn:
            for (size_t i = 0; i < in.erase_order.size(); ++i)
            {
                map_->erase(in.erase_order[i]);
                map_->try_emplace(in.erase_order[i], in.erase_order[i]);
                ops += 2;
            }
            break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// ankerl::unordered_dense Adapter
// ============================================================================

#if HAS_ANKERL
class AnkerlDenseMapAdapter final : public IMapAdapter
{
    std::unique_ptr<ankerl::unordered_dense::map<int64_t, int64_t>> map_;

public:
    const char* name() const override { return "ankerl::unordered_dense"; }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<ankerl::unordered_dense::map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::Insert:
            for (int64_t k : in.keys)
            {
                map_->insert({k, k});
                ++ops;
            }
            break;

        case Case::FindHit:
            for (int64_t k : in.keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::FindMiss:
            for (int64_t k : in.miss_keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::Erase:
            for (int64_t k : in.erase_order)
            {
                map_->erase(k);
                ++ops;
            }
            break;

        case Case::Churn:
            for (size_t i = 0; i < in.erase_order.size(); ++i)
            {
                map_->erase(in.erase_order[i]);
                map_->try_emplace(in.erase_order[i], in.erase_order[i]);
                ops += 2;
            }
            break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// absl::flat_hash_map Adapter
// ============================================================================

#if HAS_ABSL
class AbslFlatHashMapAdapter final : public IMapAdapter
{
    std::unique_ptr<absl::flat_hash_map<int64_t, int64_t>> map_;

public:
    const char* name() const override { return "absl::flat_hash_map"; }

    void setup(size_t N, const Inputs&) override
    {
        map_ = std::make_unique<absl::flat_hash_map<int64_t, int64_t>>();
        map_->reserve(N);
    }

    void teardown() override
    {
        map_.reset();
    }

    void preload(const Inputs& in) override
    {
        for (int64_t k : in.keys)
        {
            map_->insert({k, k});
        }
    }

    size_t run_operation(Case c, const Inputs& in) override
    {
        size_t ops = 0;
        switch (c)
        {
        case Case::Insert:
            for (int64_t k : in.keys)
            {
                map_->insert({k, k});
                ++ops;
            }
            break;

        case Case::FindHit:
            for (int64_t k : in.keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::FindMiss:
            for (int64_t k : in.miss_keys)
            {
                auto it = map_->find(k);
                if (it != map_->end()) benchmark_sink += it->second;
                ++ops;
            }
            break;

        case Case::Erase:
            for (int64_t k : in.erase_order)
            {
                map_->erase(k);
                ++ops;
            }
            break;

        case Case::Churn:
            for (size_t i = 0; i < in.erase_order.size(); ++i)
            {
                map_->erase(in.erase_order[i]);
                map_->try_emplace(in.erase_order[i], in.erase_order[i]);
                ops += 2;
            }
            break;
        }
        return ops;
    }
};
#endif

// ============================================================================
// Round-Robin Benchmark Runner (Critical Redesign)
// ============================================================================
// INVARIANT: Each run executes exactly one timed iteration per library.
// INVARIANT: Library order is randomized per run.
// INVARIANT: All libraries observe the same distribution of machine states.

struct CaseResult
{
    std::string library;
    std::vector<double> samples_ns_per_op;
    Statistics stats;
};

struct SuiteResult
{
    Case c{};
    size_t N = 0;
    std::vector<CaseResult> per_library;
};

static inline bool needs_preload(Case c)
{
    return c == Case::FindHit || c == Case::FindMiss || c == Case::Erase || c == Case::Churn;
}

SuiteResult run_case_round_robin(
    Case c,
    size_t N,
    const Inputs& in,
    std::vector<IMapAdapter*>& adapters,
    uint64_t rng_seed)
{
    SuiteResult out;
    out.c = c;
    out.N = N;

    // Storage for samples by adapter pointer
    std::unordered_map<IMapAdapter*, std::vector<double>> samples;
    samples.reserve(adapters.size());

    std::mt19937_64 rng(rng_seed ^ static_cast<uint64_t>(N) ^ static_cast<uint64_t>(static_cast<int>(c) * 0x9E37));

    // --- Warmup phase (round-robin, randomized order) ---
    for (size_t r = 0; r < WARMUP_RUNS; ++r)
    {
        std::vector<IMapAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IMapAdapter* a : order)
        {
            a->setup(N, in);
            if (needs_preload(c)) a->preload(in);
            (void)a->run_operation(c, in);
            a->teardown();
        }
    }

    // --- Measured phase (round-robin, randomized order) ---
    for (size_t r = 0; r < MEASURED_RUNS; ++r)
    {
        std::vector<IMapAdapter*> order = adapters;
        std::shuffle(order.begin(), order.end(), rng);

        for (IMapAdapter* a : order)
        {
            a->setup(N, in);
            if (needs_preload(c)) a->preload(in);

            Timer t;
            t.start();
            size_t ops = a->run_operation(c, in);
            double elapsed = t.elapsed_ns();

            a->teardown();

            if (ops > 0)
            {
                samples[a].push_back(ns_per_op(elapsed, ops));
            }
        }
    }

    // Materialize output in deterministic adapter list order
    for (IMapAdapter* a : adapters)
    {
        CaseResult cr;
        cr.library = a->name();
        auto it = samples.find(a);
        if (it != samples.end())
        {
            cr.samples_ns_per_op = std::move(it->second);
            cr.stats = Statistics::compute(cr.samples_ns_per_op);
        }
        out.per_library.push_back(std::move(cr));
    }

    return out;
}

// ============================================================================
// Sanity Checks (Guardrails)
// ============================================================================
// These do NOT fail the benchmark -- they provide context for interpreting results.

void sanity_check(const CaseResult& cr, size_t N)
{
    if (cr.samples_ns_per_op.empty()) return;

    // Note: >100ns at large N is expected for node-based maps (cache misses)
    if (N >= 1'000'000 && cr.stats.median > 100.0)
    {
        std::cout << "  [NOTE] " << cr.library << ": median " << cr.stats.median
            << " ns (expected for node-based maps due to cache effects)\n";
    }

    if (cr.stats.stddev > cr.stats.median && cr.stats.median > 0)
    {
        std::cout << "  [NOTE] " << cr.library << ": high variance (stddev " << cr.stats.stddev
            << " > median " << cr.stats.median << ") - system noise or memory pressure\n";
    }
}

// ============================================================================
// Output Formatting
// ============================================================================

void print_header(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

void print_case_table(const SuiteResult& sr, bool show_warnings = true)
{
    std::cout << "\n--- " << case_name(sr.c) << " ---\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(30) << "Library"
        << std::setw(12) << "Median"
        << std::setw(12) << "Mean"
        << std::setw(10) << "Stddev"
        << "  CI95\n";
    std::cout << std::string(79, '-') << "\n";

    for (const auto& cr : sr.per_library)
    {
        if (cr.samples_ns_per_op.empty())
        {
            std::cout << std::setw(30) << cr.library << "  [no samples]\n";
            continue;
        }

        std::cout << std::setw(30) << cr.library
            << std::setw(12) << cr.stats.median
            << std::setw(12) << cr.stats.mean
            << std::setw(10) << cr.stats.stddev
            << "  [" << cr.stats.ci95_low << ", " << cr.stats.ci95_high << "]\n";
    }

    if (show_warnings)
    {
        for (const auto& cr : sr.per_library)
        {
            sanity_check(cr, sr.N);
        }
    }
}

void print_compact_row(const std::string& name, const std::vector<SuiteResult>& results)
{
    // Assumes results are in order: Insert, FindHit, FindMiss, Erase, Churn
    std::cout << std::setw(30) << name;

    for (const auto& sr : results)
    {
        // Find this library's result
        for (const auto& cr : sr.per_library)
        {
            if (cr.library == name)
            {
                std::cout << std::setw(10) << cr.stats.median;
                break;
            }
        }
    }
    std::cout << "\n";
}

// ============================================================================
// Section 1: Core Operations Benchmark (Round-Robin)
// ============================================================================

void benchmark_core_operations(const std::vector<size_t>& sizes)
{
    print_header("CORE OPERATIONS BENCHMARK (Round-Robin)");

    std::cout << "Comparing StableHashMap (default and SplitMix64 hash)\n";
    std::cout << "vs std::unordered_map";
#if HAS_TSL
    std::cout << " vs tsl::robin_map";
#endif
#if HAS_ANKERL
    std::cout << " vs ankerl::unordered_dense";
#endif
#if HAS_ABSL
    std::cout << " vs absl::flat_hash_map";
#endif
    std::cout << "\n\n";

    std::cout << "Methodology:\n";
    std::cout << "  - " << WARMUP_RUNS << " warmup + " << MEASURED_RUNS << " measured runs per test\n";
    std::cout << "  - Round-robin execution with randomized order per run\n";
    std::cout << "  - All libraries observe same distribution of machine states\n";
    std::cout << "  - Primary metric: median (ns/op)\n\n";

    // Create adapters
    using StableMapDefault = fat_p::StableHashMap<int64_t, int64_t>;
    using StableMapSM64 = fat_p::StableHashMap<int64_t, int64_t,
        fat_p::CustomHashPolicy<int64_t, int64_t, SplitMix64Hash>>;

    StableHashMapAdapter<fat_p::DefaultPolicy<int64_t, int64_t>> stable_default("StableHashMap");
    StableHashMapAdapter<fat_p::CustomHashPolicy<int64_t, int64_t, SplitMix64Hash>> stable_sm64("StableHashMap+SplitMix64");
    StdUnorderedMapAdapter std_adapter;

#if HAS_TSL
    TslRobinMapAdapter tsl_adapter;
#endif
#if HAS_ANKERL
    AnkerlDenseMapAdapter ankerl_adapter;
#endif
#if HAS_ABSL
    AbslFlatHashMapAdapter absl_adapter;
#endif

    std::vector<IMapAdapter*> adapters = {
        &stable_default,
        &stable_sm64,
        &std_adapter,
#if HAS_TSL
        &tsl_adapter,
#endif
#if HAS_ANKERL
        &ankerl_adapter,
#endif
#if HAS_ABSL
        &absl_adapter,
#endif
    };

    const std::vector<Case> cases = {
        Case::Insert, Case::FindHit, Case::FindMiss, Case::Erase, Case::Churn
    };

    for (size_t N : sizes)
    {
        print_cpu_context();
        std::cout << "N = " << N << "\n";

        Inputs in = Inputs::make(N);
        uint64_t rng_seed = 0xBADC0FFE ^ N;

        // Run all cases for this N
        std::vector<SuiteResult> case_results;
        for (Case c : cases)
        {
            SuiteResult sr = run_case_round_robin(c, N, in, adapters, rng_seed);
            case_results.push_back(std::move(sr));
        }

        // Print compact table
        std::cout << std::string(79, '-') << "\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(30) << "Map"
            << std::setw(10) << "Insert"
            << std::setw(10) << "Find"
            << std::setw(10) << "Miss"
            << std::setw(10) << "Erase"
            << std::setw(10) << "Churn" << "\n";
        std::cout << std::string(79, '-') << "\n";

        for (IMapAdapter* a : adapters)
        {
            print_compact_row(a->name(), case_results);
        }

        // Print speedup vs std::unordered_map
        double std_insert = 0, std_find = 0, std_erase = 0;
        double stable_insert = 0, stable_find = 0, stable_erase = 0;

        for (const auto& sr : case_results)
        {
            for (const auto& cr : sr.per_library)
            {
                if (cr.library == "std::unordered_map")
                {
                    if (sr.c == Case::Insert) std_insert = cr.stats.median;
                    else if (sr.c == Case::FindHit) std_find = cr.stats.median;
                    else if (sr.c == Case::Erase) std_erase = cr.stats.median;
                }
                else if (cr.library == "StableHashMap")
                {
                    if (sr.c == Case::Insert) stable_insert = cr.stats.median;
                    else if (sr.c == Case::FindHit) stable_find = cr.stats.median;
                    else if (sr.c == Case::Erase) stable_erase = cr.stats.median;
                }
            }
        }

        if (std_insert > 0 && stable_insert > 0)
        {
            std::cout << "\nSpeedup vs std::unordered_map:\n";
            std::cout << "  StableHashMap: "
                << (std_insert / stable_insert) << "x insert, "
                << (std_find / stable_find) << "x find, "
                << (std_erase / stable_erase) << "x erase\n";
        }

        // Sanity checks
        for (const auto& sr : case_results)
        {
            for (const auto& cr : sr.per_library)
            {
                sanity_check(cr, N);
            }
        }

        // For largest N, print detailed statistics
        if (N == sizes.back())
        {
            std::cout << "\n--- Detailed Statistics for StableHashMap at N=" << N << " ---\n";
            for (const auto& sr : case_results)
            {
                for (const auto& cr : sr.per_library)
                {
                    if (cr.library == "StableHashMap")
                    {
                        cr.stats.print(case_name(sr.c));
                    }
                }
            }
        }

        std::cout << "\n";
    }
}

// ============================================================================
// Section 2: Pathological Erase (Tombstone Degradation Test)
// ============================================================================

void benchmark_pathological_erase()
{
    print_header("PATHOLOGICAL ERASE (TOMBSTONE DEGRADATION TEST)");

    std::cout << "Tests sustained churn on a single table without reset.\n";
    std::cout << "Tombstone-based maps (absl) degrade over time.\n";
    std::cout << "Backward-shift maps (StableHashMap, tsl) stay stable.\n";
    std::cout << "Methodology: " << WARMUP_RUNS << " warmup + " << MEASURED_RUNS << " measured runs\n";
    std::cout << "             Round-robin execution with randomized order\n\n";

    constexpr size_t N = 100000;
    constexpr size_t TOTAL_OPS = 5000000;

    print_cpu_context("Starting");
    std::cout << "N = " << N << ", Total operations = " << TOTAL_OPS << "\n\n";

    auto keys = generate_random_keys(N);

    // Generic pathological test runner
    auto run_pathological_std_api = [&](auto& map, size_t iter) -> double {
        map.reserve(N * 2);
        for (const auto& k : keys)
        {
            map.insert({k, k});
        }

        std::mt19937_64 rng(12345 + iter);
        std::vector<int64_t> current = keys;
        std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);

        Timer t;
        t.start();
        for (size_t op = 0; op < TOTAL_OPS; ++op)
        {
            size_t idx = rng() % current.size();
            map.erase(current[idx]);
            int64_t new_key = dist(rng);
            map.insert({new_key, new_key});
            current[idx] = new_key;
        }
        return t.elapsed_ns() / TOTAL_OPS;
    };

    auto run_pathological_stable = [&](auto& map, size_t iter) -> double {
        map.reserve(N * 2);
        for (const auto& k : keys)
        {
            map.insert(k, k);
        }

        std::mt19937_64 rng(12345 + iter);
        std::vector<int64_t> current = keys;
        std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);

        Timer t;
        t.start();
        for (size_t op = 0; op < TOTAL_OPS; ++op)
        {
            size_t idx = rng() % current.size();
            map.erase(current[idx]);
            int64_t new_key = dist(rng);
            map.insert(new_key, new_key);
            current[idx] = new_key;
        }
        return t.elapsed_ns() / TOTAL_OPS;
    };

    // Collect samples with round-robin
    struct PathResult { std::string name; std::vector<double> samples; };
    std::vector<PathResult> results;

    results.push_back({"StableHashMap", {}});
    results.push_back({"StableHashMap+SplitMix64", {}});
#if HAS_TSL
    results.push_back({"tsl::robin_map", {}});
#endif
#if HAS_ANKERL
    results.push_back({"ankerl::unordered_dense", {}});
#endif
#if HAS_ABSL
    results.push_back({"absl::flat_hash_map", {}});
#endif
    results.push_back({"std::unordered_map", {}});

    std::mt19937_64 order_rng(0x12345678);

    // Warmup + measured runs with round-robin order
    for (size_t run = 0; run < WARMUP_RUNS + MEASURED_RUNS; ++run)
    {
        bool is_warmup = (run < WARMUP_RUNS);

        // Randomize order
        std::vector<size_t> order(results.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), order_rng);

        for (size_t idx : order)
        {
            double ns = 0;

            if (results[idx].name == "StableHashMap")
            {
                fat_p::StableHashMap<int64_t, int64_t> map;
                ns = run_pathological_stable(map, run);
            }
            else if (results[idx].name == "StableHashMap+SplitMix64")
            {
                fat_p::StableHashMap<int64_t, int64_t,
                    fat_p::CustomHashPolicy<int64_t, int64_t, SplitMix64Hash>> map;
                ns = run_pathological_stable(map, run);
            }
#if HAS_TSL
            else if (results[idx].name == "tsl::robin_map")
            {
                tsl::robin_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, run);
            }
#endif
#if HAS_ANKERL
            else if (results[idx].name == "ankerl::unordered_dense")
            {
                ankerl::unordered_dense::map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, run);
            }
#endif
#if HAS_ABSL
            else if (results[idx].name == "absl::flat_hash_map")
            {
                absl::flat_hash_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, run);
            }
#endif
            else if (results[idx].name == "std::unordered_map")
            {
                std::unordered_map<int64_t, int64_t> map;
                ns = run_pathological_std_api(map, run);
            }

            if (!is_warmup)
            {
                results[idx].samples.push_back(ns);
            }
        }
    }

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : results)
    {
        auto stats = Statistics::compute(r.samples);
        std::cout << std::setw(28) << r.name << ": " << std::setw(8) << stats.median << " ns/op  "
            << "(+/-" << stats.stddev << ", CI:[" << stats.ci95_low << "," << stats.ci95_high << "])\n";
    }
}

// ============================================================================
// Section 3: Hash Quality Impact (std::hash vs SplitMix64)
// ============================================================================

void benchmark_hash_quality(const std::vector<size_t>& sizes)
{
    print_header("HASH QUALITY IMPACT (StableHashMap: std::hash vs SplitMix64)");

    std::cout << "Compares StableHashMap<K,V> vs StableHashMap<K,V,SplitMix64Hash>.\n";
    std::cout << "SplitMix64: high-quality 64-bit mixer.\n";
    std::cout << "Both columns are StableHashMap - only the hash function differs.\n\n";

    constexpr size_t WARMUP_ITERS = 1;
    constexpr size_t ITERATIONS = 5;
    constexpr size_t FIND_ITERS = 1000000;

    for (size_t n : sizes)
    {
        print_cpu_context();
        std::cout << "N = " << n << "\n";
        std::cout << std::string(60, '-') << "\n";

        auto keys = generate_random_keys(n);
        auto missing = generate_missing_keys(n);

        // std::hash version
        fat_p::StableHashMap<int64_t, int64_t> map_std;
        map_std.reserve(n);
        for (const auto& k : keys) map_std.insert(k, k);

        // SplitMix64 version
        fat_p::StableHashMap<int64_t, int64_t,
            fat_p::CustomHashPolicy<int64_t, int64_t, SplitMix64Hash>> map_sm;
        map_sm.reserve(n);
        for (const auto& k : keys) map_sm.insert(k, k);

        // Benchmark find hit (interleaved for fairness)
        double std_hit_ns = 0, sm_hit_ns = 0;
        for (size_t iter = 0; iter < ITERATIONS + WARMUP_ITERS; ++iter)
        {
            size_t idx = 0;

            // Alternate which runs first
            if (iter % 2 == 0)
            {
                Timer t1; t1.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_std.find(keys[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) std_hit_ns += t1.elapsed_ns() / FIND_ITERS;

                idx = 0;
                Timer t2; t2.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_sm.find(keys[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) sm_hit_ns += t2.elapsed_ns() / FIND_ITERS;
            }
            else
            {
                Timer t2; t2.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_sm.find(keys[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) sm_hit_ns += t2.elapsed_ns() / FIND_ITERS;

                idx = 0;
                Timer t1; t1.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_std.find(keys[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) std_hit_ns += t1.elapsed_ns() / FIND_ITERS;
            }
        }
        std_hit_ns /= ITERATIONS;
        sm_hit_ns /= ITERATIONS;

        // Benchmark find miss (interleaved)
        double std_miss_ns = 0, sm_miss_ns = 0;
        for (size_t iter = 0; iter < ITERATIONS + WARMUP_ITERS; ++iter)
        {
            size_t idx = 0;

            if (iter % 2 == 0)
            {
                Timer t1; t1.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_std.find(missing[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % missing.size();
                }
                if (iter >= WARMUP_ITERS) std_miss_ns += t1.elapsed_ns() / FIND_ITERS;

                idx = 0;
                Timer t2; t2.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_sm.find(missing[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % missing.size();
                }
                if (iter >= WARMUP_ITERS) sm_miss_ns += t2.elapsed_ns() / FIND_ITERS;
            }
            else
            {
                Timer t2; t2.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_sm.find(missing[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % missing.size();
                }
                if (iter >= WARMUP_ITERS) sm_miss_ns += t2.elapsed_ns() / FIND_ITERS;

                idx = 0;
                Timer t1; t1.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    auto* v = map_std.find(missing[idx]);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % missing.size();
                }
                if (iter >= WARMUP_ITERS) std_miss_ns += t1.elapsed_ns() / FIND_ITERS;
            }
        }
        std_miss_ns /= ITERATIONS;
        sm_miss_ns /= ITERATIONS;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Find (hit):  std::hash=" << std::setw(7) << std_hit_ns
            << " ns  SplitMix64=" << std::setw(7) << sm_hit_ns << " ns"
            << "  (" << (std_hit_ns < sm_hit_ns ? "std wins" : "SM64 wins") << ")\n";
        std::cout << "  Find (miss): std::hash=" << std::setw(7) << std_miss_ns
            << " ns  SplitMix64=" << std::setw(7) << sm_miss_ns << " ns"
            << "  (" << (std_miss_ns < sm_miss_ns ? "std wins" : "SM64 wins") << ")\n";
        std::cout << "\n";
    }
}

// ============================================================================
// Section 4: String Heterogeneous Lookup
// ============================================================================

void benchmark_string_heterogeneous(const std::vector<size_t>& sizes)
{
    print_header("STRING HETEROGENEOUS LOOKUP");

    std::cout << "Benefit of find(string_view) vs find(temp std::string).\n";
    std::cout << "Real workloads often have string keys but view-based lookups.\n";
    std::cout << "Heterogeneous lookup avoids temporary string construction.\n\n";

    constexpr size_t WARMUP_ITERS = 1;
    constexpr size_t ITERATIONS = 5;
    constexpr size_t FIND_ITERS = 1000000;

    using HeteroPolicy = fat_p::CustomHashPolicy<std::string, size_t,
        TransparentStringHash,
        TransparentStringEqual>;
    using HeteroMap = fat_p::StableHashMap<std::string, size_t, HeteroPolicy>;

    for (size_t n : sizes)
    {
        print_cpu_context();
        std::cout << "N = " << n << "\n";
        std::cout << std::string(60, '-') << "\n";

        auto keys = generate_string_keys(n);

        HeteroMap map;
        map.reserve(n);
        for (size_t i = 0; i < n; ++i)
        {
            map.insert(keys[i], i);
        }

        // Benchmark with string_view (heterogeneous) - interleaved
        double hetero_ns = 0, temp_ns = 0;
        for (size_t iter = 0; iter < ITERATIONS + WARMUP_ITERS; ++iter)
        {
            size_t idx = 0;

            if (iter % 2 == 0)
            {
                Timer t; t.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    std::string_view sv{keys[idx]};
                    auto* v = map.find(sv);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) hetero_ns += t.elapsed_ns() / FIND_ITERS;

                idx = 0;
                Timer t2; t2.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    std::string_view sv{keys[idx]};
                    std::string temp{sv};
                    auto* v = map.find(temp);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) temp_ns += t2.elapsed_ns() / FIND_ITERS;
            }
            else
            {
                Timer t2; t2.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    std::string_view sv{keys[idx]};
                    std::string temp{sv};
                    auto* v = map.find(temp);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) temp_ns += t2.elapsed_ns() / FIND_ITERS;

                idx = 0;
                Timer t; t.start();
                for (size_t i = 0; i < FIND_ITERS; ++i)
                {
                    std::string_view sv{keys[idx]};
                    auto* v = map.find(sv);
                    if (v) benchmark_sink += static_cast<size_t>(*v);
                    idx = (idx + 1) % keys.size();
                }
                if (iter >= WARMUP_ITERS) hetero_ns += t.elapsed_ns() / FIND_ITERS;
            }
        }
        hetero_ns /= ITERATIONS;
        temp_ns /= ITERATIONS;

        double speedup = temp_ns / hetero_ns;
        double savings = temp_ns - hetero_ns;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  find(string_view): " << std::setw(8) << hetero_ns << " ns\n";
        std::cout << "  find(temp string): " << std::setw(8) << temp_ns << " ns\n";
        std::cout << "  Speedup: " << speedup << "x (saves " << savings << " ns/op)\n";
        std::cout << "\n";
    }
}

// ============================================================================
// Section 5: Load Factor Sensitivity
// ============================================================================

void benchmark_load_factor_sensitivity()
{
    print_header("LOAD FACTOR SENSITIVITY");

    std::cout << "How performance degrades at high load factors.\n";
    std::cout << "StableHashMap warns at 0.85+, asserts at 0.90+ in debug builds.\n";
    std::cout << "Read-only mode (freeze()) allows safe 0.95 load factor.\n\n";

    print_cpu_context("Starting");

    constexpr size_t BUCKETS = 65536;
    constexpr size_t BATCH_SIZE = 1000;
    constexpr size_t FIND_ITERS = 100000;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Load Factor |   Find (ns) |  Insert (ns) |  Erase (ns)\n";
    std::cout << "------------|-------------|--------------|------------\n";

    for (float target_load : {0.50f, 0.60f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f})
    {
        size_t base_elements = static_cast<size_t>(BUCKETS * target_load);

        std::vector<int> base_keys(base_elements);
        std::iota(base_keys.begin(), base_keys.end(), 0);
        std::shuffle(base_keys.begin(), base_keys.end(), std::mt19937(12345));

        std::vector<int> find_batch(base_keys.begin(),
            base_keys.begin() + std::min(BATCH_SIZE, base_elements));
        std::shuffle(find_batch.begin(), find_batch.end(), std::mt19937(999));

        // Find benchmark (frozen map)
        fat_p::StableHashMap<int, int> find_map(BUCKETS, 0.99f);
        for (int k : base_keys)
        {
            find_map.insert(k, k * 10);
        }
        find_map.freeze();

        double find_ns = 0;
        {
            size_t idx = 0;
            Timer t; t.start();
            for (size_t i = 0; i < FIND_ITERS; ++i)
            {
                auto* v = find_map.find(find_batch[idx]);
                benchmark_sink += reinterpret_cast<size_t>(v);
                idx = (idx + 1) % find_batch.size();
            }
            find_ns = t.elapsed_ns() / FIND_ITERS;
        }

        // Insert benchmark
        double insert_ns = 0;
        {
            fat_p::StableHashMap<int, int> map(BUCKETS, 0.99f);
            for (size_t i = 0; i < base_elements - BATCH_SIZE; ++i)
            {
                map.insert(base_keys[i], base_keys[i]);
            }

            Timer t; t.start();
            for (size_t i = base_elements - BATCH_SIZE; i < base_elements; ++i)
            {
                map.insert(base_keys[i], base_keys[i]);
            }
            insert_ns = t.elapsed_ns() / BATCH_SIZE;
        }

        // Erase benchmark
        double erase_ns = 0;
        {
            fat_p::StableHashMap<int, int> map(BUCKETS, 0.99f);
            for (int k : base_keys)
            {
                map.insert(k, k);
            }

            Timer t; t.start();
            for (size_t i = 0; i < std::min(BATCH_SIZE, base_elements); ++i)
            {
                map.erase(base_keys[i]);
            }
            erase_ns = t.elapsed_ns() / std::min(BATCH_SIZE, base_elements);
        }

        std::cout << "    " << std::setw(5) << (target_load * 100) << "% |"
            << std::setw(12) << find_ns << " |"
            << std::setw(13) << insert_ns << " |"
            << std::setw(12) << erase_ns << "\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    std::cout << "================================================================================\n";
    std::cout << "  StableHashMap Comprehensive Benchmark Suite (Round-Robin Architecture)\n";
    std::cout << "================================================================================\n";
    std::cout << "\nPlatform: ";
#if defined(_WIN32) || defined(_WIN64)
    std::cout << "Windows";
#else
    std::cout << "Linux";
#endif
    std::cout << " (warmup=" << WARMUP_RUNS << ", measured=" << MEASURED_RUNS << ")\n";

    std::cout << "Competitor libraries: ";
#if HAS_TSL
    std::cout << "tsl ";
#endif
#if HAS_ANKERL
    std::cout << "ankerl ";
#endif
#if HAS_ABSL
    std::cout << "absl ";
#endif
#if !HAS_TSL && !HAS_ANKERL && !HAS_ABSL
    std::cout << "(none found)";
#endif
    std::cout << "\n\n";

    std::cout << "Design Invariants:\n";
    std::cout << "  1. Each measured run executes exactly one timed iteration per library\n";
    std::cout << "  2. Library execution order is randomized per run\n";
    std::cout << "  3. Setup/reserve outside timed regions (Insert is amortized)\n";
    std::cout << "  4. All libraries observe same distribution of machine states\n";
    std::cout << "  5. Medians are the primary reported statistic\n\n";

    // Configure benchmark environment
    std::unique_ptr<BenchmarkScope> bench_scope;
#if defined(FATP_BENCHMARK_MODE) || defined(_DEBUG)
    bench_scope = std::make_unique<BenchmarkScope>(true);
#else
    if (has_env_var("FATP_BENCHMARK_MODE"))
    {
        bench_scope = std::make_unique<BenchmarkScope>(true);
    }
#endif

    // Default sizes
    std::vector<size_t> core_sizes = {10000, 100000, 1000000};
    std::vector<size_t> hash_sizes = {10000, 100000, 500000, 1000000};
    std::vector<size_t> string_sizes = {1000, 10000, 100000};

    // Run all benchmarks
    benchmark_core_operations(core_sizes);
    benchmark_pathological_erase();
    benchmark_hash_quality(hash_sizes);
    benchmark_string_heterogeneous(string_sizes);
    benchmark_load_factor_sensitivity();

    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "================================================================================\n";

    return 0;
}
