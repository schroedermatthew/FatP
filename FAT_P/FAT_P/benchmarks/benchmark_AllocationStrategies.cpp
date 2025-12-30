/**
 * @file benchmark_AllocationStrategies.cpp
 * @brief Performance benchmarks for Fat-P allocation strategies
 *
 * Benchmarks compare:
 * - fat_p::NewDeleteAllocator vs std::allocator
 * - fat_p::BlockAllocator vs std::allocator
 * - fat_p::PoolAllocator vs std::allocator
 *
 * Compile (minimal):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark_AllocationStrategies.cpp -o bench_alloc
 *
 * Compile (with AVX2):
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native -mavx2 benchmark_AllocationStrategies.cpp -o bench_alloc
 *
 * Windows (MSVC):
 *   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark_AllocationStrategies.cpp /link advapi32.lib
 *
 * Run with environment overrides:
 *   FATP_BENCH_BATCHES=100 FATP_BENCH_SEED=42 ./bench_alloc
 *
 * @version 1.0
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "AllocationStrategies.h"

// =============================================================================
// Platform Detection
// =============================================================================

#if defined(_WIN32) || defined(_WIN64)
#define FATP_WINDOWS 1
#include <windows.h>
#include <intrin.h>
#else
#define FATP_WINDOWS 0
#include <unistd.h>
#endif

// =============================================================================
// Benchmark Configuration
// =============================================================================

namespace
{

struct BenchConfig
{
    size_t warmupRuns = 3;
    size_t measuredBatches = 50;
    uint64_t seed = 12345;
    size_t targetWork = 1000000;
    size_t minBatchMs = 50;
    bool noScope = false;
    bool noStabilize = false;
    bool noCooldown = false;
    bool verboseStats = false;
    std::string csvPath;
    std::string jsonPath;
};

inline std::string getEnvOr(const char* name, const char* defaultVal)
{
    const char* val = std::getenv(name);
    return val ? val : defaultVal;
}

inline size_t getEnvSize(const char* name, size_t defaultVal)
{
    const char* val = std::getenv(name);
    return val ? static_cast<size_t>(std::stoull(val)) : defaultVal;
}

inline uint64_t getEnvU64(const char* name, uint64_t defaultVal)
{
    const char* val = std::getenv(name);
    return val ? static_cast<uint64_t>(std::stoull(val)) : defaultVal;
}

inline bool hasEnvVar(const char* name)
{
    const char* val = std::getenv(name);
    return val && val[0] != '\0';
}

BenchConfig loadConfig()
{
    BenchConfig cfg;

#if FATP_WINDOWS
    cfg.measuredBatches = 15;  // Windows: higher run-to-run variance
#else
    cfg.measuredBatches = 50;
#endif

    cfg.warmupRuns = getEnvSize("FATP_BENCH_WARMUP_RUNS", cfg.warmupRuns);
    cfg.measuredBatches = getEnvSize("FATP_BENCH_BATCHES", cfg.measuredBatches);
    cfg.seed = getEnvU64("FATP_BENCH_SEED", cfg.seed);
    cfg.targetWork = getEnvSize("FATP_BENCH_TARGET_WORK", cfg.targetWork);
    cfg.minBatchMs = getEnvSize("FATP_BENCH_MIN_BATCH_MS", cfg.minBatchMs);
    cfg.noScope = hasEnvVar("FATP_BENCH_NO_SCOPE");
    cfg.noStabilize = hasEnvVar("FATP_BENCH_NO_STABILIZE");
    cfg.noCooldown = hasEnvVar("FATP_BENCH_NO_COOLDOWN");
    cfg.verboseStats = hasEnvVar("FATP_BENCH_VERBOSE_STATS");
    cfg.csvPath = getEnvOr("FATP_BENCH_OUTPUT_CSV", "");
    cfg.jsonPath = getEnvOr("FATP_BENCH_OUTPUT_JSON", "");

    return cfg;
}

void printConfig(const BenchConfig& cfg)
{
    std::cout << "Benchmark Configuration:\n";
    std::cout << "  Seed:           " << cfg.seed << "\n";
    std::cout << "  Warmup runs:    " << cfg.warmupRuns << "\n";
    std::cout << "  Measured runs:  " << cfg.measuredBatches << "\n";
    std::cout << "  Target work:    " << cfg.targetWork << "\n";
    std::cout << "  Min batch ms:   " << cfg.minBatchMs << "\n";
    std::cout << "  Scope:          " << (cfg.noScope ? "OFF" : "ON") << "\n";
    std::cout << "  Stabilize:      " << (cfg.noStabilize ? "OFF" : "ON") << "\n";
    std::cout << "  Cooldown:       " << (cfg.noCooldown ? "OFF" : "ON") << "\n";
    if (!cfg.csvPath.empty())
        std::cout << "  CSV output:     " << cfg.csvPath << "\n";
    if (!cfg.jsonPath.empty())
        std::cout << "  JSON output:    " << cfg.jsonPath << "\n";
    std::cout << "\n";
}

} // anonymous namespace

// =============================================================================
// CPU Frequency Monitoring
// =============================================================================

namespace
{

struct CpuFreqInfo
{
    double refFreqMhz = 0;
    double currentFreqMhz = 0;
    bool refIsMax = false;

    double throttlePercentage() const
    {
        if (currentFreqMhz <= 0 || refFreqMhz <= 0)
            return 0;
        return (1.0 - currentFreqMhz / refFreqMhz) * 100.0;
    }

    bool isThrottled() const { return !refIsMax && throttlePercentage() > 5.0; }
    bool isTurbo() const { return !refIsMax && currentFreqMhz > refFreqMhz * 1.05; }
};

#if FATP_WINDOWS

CpuFreqInfo getCpuFreq()
{
    CpuFreqInfo info;

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD mhz = 0;
        DWORD size = sizeof(mhz);
        if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS)
        {
            info.refFreqMhz = static_cast<double>(mhz);
            info.currentFreqMhz = static_cast<double>(mhz);
            info.refIsMax = false;  // Windows registry is reliable base
        }
        RegCloseKey(hKey);
    }

    return info;
}

#else  // Linux

CpuFreqInfo getCpuFreq()
{
    CpuFreqInfo info;

    auto readFreq = [](const char* path) -> double {
        std::ifstream f(path);
        if (!f)
            return 0;
        uint64_t khz = 0;
        f >> khz;
        return static_cast<double>(khz) / 1000.0;
    };

    info.currentFreqMhz = readFreq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");

    // Try base_frequency first (reliable)
    info.refFreqMhz = readFreq("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency");
    if (info.refFreqMhz > 0)
    {
        info.refIsMax = false;
        return info;
    }

    // Fallback to cpuinfo_max_freq (turbo, less reliable)
    info.refFreqMhz = readFreq("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if (info.refFreqMhz > 0)
    {
        info.refIsMax = true;
    }

    return info;
}

#endif

void printCpuContext(const char* label = nullptr)
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::cout << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "] ";
    if (label)
        std::cout << label << " ";

    auto info = getCpuFreq();
    if (info.currentFreqMhz > 0)
    {
        std::cout << "CPU: " << static_cast<int>(info.currentFreqMhz) << " MHz";

        if (info.refFreqMhz > 0)
        {
            const char* refLabel = info.refIsMax ? "max" : "base";
            std::cout << " (" << refLabel << ": " << static_cast<int>(info.refFreqMhz) << ")";

            if (!info.refIsMax)
            {
                if (info.isThrottled())
                {
                    std::cout << " [THROTTLED " << std::fixed << std::setprecision(1)
                              << info.throttlePercentage() << "%]";
                }
                else if (info.isTurbo())
                {
                    std::cout << " [TURBO]";
                }
            }
        }
    }
    else
    {
        std::cout << "CPU: (frequency unavailable)";
    }
    std::cout << "\n";
}

} // anonymous namespace

// =============================================================================
// Benchmark Scope (Windows Priority/Affinity)
// =============================================================================

#if FATP_WINDOWS

class BenchmarkScope
{
    DWORD mOldPriority = 0;
    DWORD_PTR mOldAffinity = 0;
    bool mApplied = false;

public:
    explicit BenchmarkScope(bool noScope = false)
    {
        if (noScope)
            return;

        HANDLE proc = GetCurrentProcess();
        mOldPriority = GetPriorityClass(proc);
        SetPriorityClass(proc, HIGH_PRIORITY_CLASS);

        HANDLE thread = GetCurrentThread();
        mOldAffinity = SetThreadAffinityMask(thread, 2);  // Pin to CPU 1
        mApplied = true;
    }

    ~BenchmarkScope()
    {
        if (mApplied)
        {
            SetPriorityClass(GetCurrentProcess(), mOldPriority);
            SetThreadAffinityMask(GetCurrentThread(), mOldAffinity);
        }
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

// =============================================================================
// Timer and Statistics
// =============================================================================

namespace
{

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::nano>;

struct Stats
{
    double median = 0;
    double mean = 0;
    double stddev = 0;
    double ci95Low = 0;
    double ci95High = 0;
    double minVal = 0;
    double maxVal = 0;
};

Stats computeStats(std::vector<double>& samples)
{
    Stats s;
    if (samples.empty())
        return s;

    std::sort(samples.begin(), samples.end());

    size_t n = samples.size();
    s.minVal = samples.front();
    s.maxVal = samples.back();

    // Median
    if (n % 2 == 0)
        s.median = (samples[n / 2 - 1] + samples[n / 2]) / 2.0;
    else
        s.median = samples[n / 2];

    // Mean
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(n);

    // Stddev
    double sqSum = 0;
    for (double v : samples)
    {
        double diff = v - s.mean;
        sqSum += diff * diff;
    }
    s.stddev = std::sqrt(sqSum / static_cast<double>(n));

    // 95% CI (using t-distribution approximation for small samples)
    double tValue = 1.96;  // Approximate for n >= 30
    if (n < 30)
    {
        // Rough t-values for common small sample sizes
        if (n <= 10)
            tValue = 2.26;
        else if (n <= 20)
            tValue = 2.09;
        else
            tValue = 2.04;
    }
    double margin = tValue * s.stddev / std::sqrt(static_cast<double>(n));
    s.ci95Low = s.mean - margin;
    s.ci95High = s.mean + margin;

    return s;
}

// Prevent dead code elimination
#if defined(_MSC_VER)
// MSVC: use volatile and memory barrier
template<typename T>
inline void doNotOptimize(T const& value)
{
    volatile T sink = value;
    (void)sink;
    _ReadWriteBarrier();
}

template<typename T>
inline void doNotOptimize(T& value)
{
    volatile T* vp = &value;
    (void)*vp;
    _ReadWriteBarrier();
}
#else
// GCC/Clang: use inline assembly
template<typename T>
inline void doNotOptimize(T const& value)
{
    asm volatile("" : : "r,m"(value) : "memory");
}

template<typename T>
inline void doNotOptimize(T& value)
{
#if defined(__clang__)
    asm volatile("" : "+r,m"(value) : : "memory");
#else
    asm volatile("" : "+m,r"(value) : : "memory");
#endif
}
#endif

} // anonymous namespace

// =============================================================================
// Output Formatting
// =============================================================================

namespace
{

void printHeader(const std::string& title)
{
    std::cout << "\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n\n";
}

void printContractNote(const std::string& note)
{
    std::cout << "Contract: " << note << "\n\n";
}

void printResultHeader()
{
    std::cout << std::left
              << std::setw(28) << "Allocator"
              << std::right
              << std::setw(14) << "Median (ns)"
              << std::setw(14) << "Mean (ns)"
              << std::setw(12) << "Stddev"
              << std::setw(22) << "CI95"
              << "\n";
    std::cout << std::string(90, '-') << "\n";
}

void printResult(const std::string& name, const Stats& s)
{
    std::cout << std::left << std::setw(28) << name
              << std::right << std::fixed << std::setprecision(2)
              << std::setw(14) << s.median
              << std::setw(14) << s.mean
              << std::setw(12) << s.stddev
              << "  [" << std::setw(8) << s.ci95Low << ", " << std::setw(8) << s.ci95High << "]"
              << "\n";

    // Sanity check
    if (s.stddev > s.median && s.median > 0)
    {
        std::cout << "  [NOTE] High variance (stddev > median)\n";
    }
}

void printSubheader(const std::string& title)
{
    std::cout << "\n--- " << title << " ---\n\n";
}

} // anonymous namespace

// =============================================================================
// CSV/JSON Output
// =============================================================================

namespace
{

struct BenchmarkResult
{
    std::string benchmark;
    std::string testCase;
    std::string allocator;
    std::string unit;
    Stats stats;
    std::string cpuContext;
    std::string timestamp;
};

std::vector<BenchmarkResult> gResults;

void recordResult(const std::string& benchmark, const std::string& testCase,
                  const std::string& allocator, const Stats& stats,
                  const std::string& unit = "ns/op")
{
    BenchmarkResult r;
    r.benchmark = benchmark;
    r.testCase = testCase;
    r.allocator = allocator;
    r.unit = unit;
    r.stats = stats;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
    r.timestamp = oss.str();

    auto freq = getCpuFreq();
    std::ostringstream cpuOss;
    cpuOss << static_cast<int>(freq.currentFreqMhz) << "MHz";
    if (freq.refFreqMhz > 0)
        cpuOss << " (" << (freq.refIsMax ? "max" : "base") << ": "
               << static_cast<int>(freq.refFreqMhz) << ")";
    r.cpuContext = cpuOss.str();

    gResults.push_back(r);
}

void writeCsv(const std::string& path, const BenchConfig& cfg)
{
    std::ofstream f(path);
    if (!f)
    {
        std::cerr << "Warning: Could not open CSV file: " << path << "\n";
        return;
    }

    f << "Timestamp,Benchmark,Case,Allocator,Unit,Median,Mean,Stddev,CI95_Low,CI95_High,"
      << "CPU_Context,Seed,Warmup,Batches,TargetWork,MinBatchMs\n";

    for (const auto& r : gResults)
    {
        f << r.timestamp << ","
          << r.benchmark << ","
          << r.testCase << ","
          << r.allocator << ","
          << r.unit << ","
          << std::fixed << std::setprecision(2)
          << r.stats.median << ","
          << r.stats.mean << ","
          << r.stats.stddev << ","
          << r.stats.ci95Low << ","
          << r.stats.ci95High << ","
          << "\"" << r.cpuContext << "\","
          << cfg.seed << ","
          << cfg.warmupRuns << ","
          << cfg.measuredBatches << ","
          << cfg.targetWork << ","
          << cfg.minBatchMs << "\n";
    }

    std::cout << "CSV results written to: " << path << "\n";
}

void writeJson(const std::string& path, const BenchConfig& cfg)
{
    std::ofstream f(path);
    if (!f)
    {
        std::cerr << "Warning: Could not open JSON file: " << path << "\n";
        return;
    }

    f << "{\n";
    f << "  \"config\": {\n";
    f << "    \"seed\": " << cfg.seed << ",\n";
    f << "    \"warmup_runs\": " << cfg.warmupRuns << ",\n";
    f << "    \"measured_batches\": " << cfg.measuredBatches << ",\n";
    f << "    \"target_work\": " << cfg.targetWork << ",\n";
    f << "    \"min_batch_ms\": " << cfg.minBatchMs << "\n";
    f << "  },\n";
    f << "  \"results\": [\n";

    for (size_t i = 0; i < gResults.size(); ++i)
    {
        const auto& r = gResults[i];
        f << "    {\n";
        f << "      \"timestamp\": \"" << r.timestamp << "\",\n";
        f << "      \"benchmark\": \"" << r.benchmark << "\",\n";
        f << "      \"case\": \"" << r.testCase << "\",\n";
        f << "      \"allocator\": \"" << r.allocator << "\",\n";
        f << "      \"unit\": \"" << r.unit << "\",\n";
        f << "      \"median\": " << std::fixed << std::setprecision(2) << r.stats.median << ",\n";
        f << "      \"mean\": " << r.stats.mean << ",\n";
        f << "      \"stddev\": " << r.stats.stddev << ",\n";
        f << "      \"ci95_low\": " << r.stats.ci95Low << ",\n";
        f << "      \"ci95_high\": " << r.stats.ci95High << ",\n";
        f << "      \"cpu_context\": \"" << r.cpuContext << "\"\n";
        f << "    }" << (i + 1 < gResults.size() ? "," : "") << "\n";
    }

    f << "  ]\n";
    f << "}\n";

    std::cout << "JSON results written to: " << path << "\n";
}

} // anonymous namespace

// =============================================================================
// Test Data Types
// =============================================================================

namespace
{

// Trivially copyable node for PoolAllocator
struct TrivialNode
{
    int64_t mKey;
    int64_t mValue;

    TrivialNode() : mKey(0), mValue(0) {}
    TrivialNode(int64_t k, int64_t v) : mKey(k), mValue(v) {}
};
static_assert(std::is_trivially_copyable_v<TrivialNode>);

// Pointer-sized type for BlockAllocator
struct PointerSized
{
    int64_t mValue;

    PointerSized() : mValue(0) {}
    explicit PointerSized(int64_t v) : mValue(v) {}
};
static_assert(sizeof(PointerSized) >= sizeof(void*));

} // anonymous namespace

// =============================================================================
// Benchmark: Single Allocation
// =============================================================================

namespace
{

void benchSingleAllocation(const BenchConfig& cfg)
{
    printHeader("Single Allocation/Deallocation");
    printCpuContext("Start");
    printContractNote("Measures time for one allocate() + deallocate() cycle. "
                      "Allocation includes construction, deallocation includes destruction.");

    const size_t kIterations = cfg.targetWork;

    // Round-robin: interleave measurements to reduce bias
    std::vector<double> newDeleteSamples;
    std::vector<double> stdAllocSamples;
    std::vector<double> blockSamples;
    std::vector<double> poolSamples;

    newDeleteSamples.reserve(cfg.measuredBatches);
    stdAllocSamples.reserve(cfg.measuredBatches);
    blockSamples.reserve(cfg.measuredBatches);
    poolSamples.reserve(cfg.measuredBatches);

    // Pre-create allocators for steady-state measurement
    fat_p::BlockAllocator<PointerSized> blockAlloc;
    fat_p::PoolAllocator<1000>::Allocator<TrivialNode> poolAlloc;

    // Warmup
    for (size_t w = 0; w < cfg.warmupRuns; ++w)
    {
        {
            fat_p::NewDeleteAllocator<int> alloc;
            for (size_t i = 0; i < kIterations / 10; ++i)
            {
                int* p = alloc.allocate(42);
                doNotOptimize(p);
                alloc.deallocate(p);
            }
        }
        {
            std::allocator<int> alloc;
            for (size_t i = 0; i < kIterations / 10; ++i)
            {
                int* p = alloc.allocate(1);
                *p = 42;
                doNotOptimize(p);
                alloc.deallocate(p, 1);
            }
        }
        {
            for (size_t i = 0; i < kIterations / 10; ++i)
            {
                PointerSized* p = blockAlloc.allocate(42);
                doNotOptimize(p);
                blockAlloc.deallocate(p);
            }
        }
        {
            for (size_t i = 0; i < kIterations / 10; ++i)
            {
                TrivialNode* p = poolAlloc.allocate(42, 42);
                doNotOptimize(p);
                poolAlloc.deallocate(p);
            }
        }
    }

    // Measured batches (round-robin)
    for (size_t batch = 0; batch < cfg.measuredBatches; ++batch)
    {
        // NewDeleteAllocator
        {
            fat_p::NewDeleteAllocator<int> alloc;
            auto start = Clock::now();
            for (size_t i = 0; i < kIterations; ++i)
            {
                int* p = alloc.allocate(42);
                doNotOptimize(p);
                alloc.deallocate(p);
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            newDeleteSamples.push_back(ns);
        }

        // std::allocator
        {
            std::allocator<int> alloc;
            auto start = Clock::now();
            for (size_t i = 0; i < kIterations; ++i)
            {
                int* p = alloc.allocate(1);
                *p = 42;
                doNotOptimize(p);
                alloc.deallocate(p, 1);
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            stdAllocSamples.push_back(ns);
        }

        // BlockAllocator (steady-state with free list)
        {
            auto start = Clock::now();
            for (size_t i = 0; i < kIterations; ++i)
            {
                PointerSized* p = blockAlloc.allocate(static_cast<int64_t>(i));
                doNotOptimize(p);
                blockAlloc.deallocate(p);
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            blockSamples.push_back(ns);
        }

        // PoolAllocator (steady-state with free list)
        {
            auto start = Clock::now();
            for (size_t i = 0; i < kIterations; ++i)
            {
                TrivialNode* p = poolAlloc.allocate(static_cast<int64_t>(i),
                                                    static_cast<int64_t>(i));
                doNotOptimize(p);
                poolAlloc.deallocate(p);
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            poolSamples.push_back(ns);
        }
    }

    // Compute statistics
    Stats newDeleteStats = computeStats(newDeleteSamples);
    Stats stdAllocStats = computeStats(stdAllocSamples);
    Stats blockStats = computeStats(blockSamples);
    Stats poolStats = computeStats(poolSamples);

    // Print results
    printResultHeader();
    printResult("fat_p::NewDeleteAllocator", newDeleteStats);
    printResult("std::allocator", stdAllocStats);
    printResult("fat_p::BlockAllocator", blockStats);
    printResult("fat_p::PoolAllocator", poolStats);

    // Record for CSV/JSON
    recordResult("AllocationStrategies", "SingleAlloc", "NewDeleteAllocator", newDeleteStats);
    recordResult("AllocationStrategies", "SingleAlloc", "std::allocator", stdAllocStats);
    recordResult("AllocationStrategies", "SingleAlloc", "BlockAllocator", blockStats);
    recordResult("AllocationStrategies", "SingleAlloc", "PoolAllocator", poolStats);

    // Correctness check (outside timed region)
    {
        fat_p::NewDeleteAllocator<int> alloc;
        int* p = alloc.allocate(12345);
        if (*p != 12345)
        {
            std::cerr << "ERROR: NewDeleteAllocator correctness check failed!\n";
        }
        alloc.deallocate(p);
    }

    printCpuContext("End");
}

} // anonymous namespace

// =============================================================================
// Benchmark: Burst Allocation (N objects)
// =============================================================================

namespace
{

void benchBurstAllocation(const BenchConfig& cfg)
{
    printHeader("Burst Allocation (100 objects)");
    printCpuContext("Start");
    printContractNote("Allocate 100 objects, then deallocate all. "
                      "Measures bulk allocation pattern common in container growth.");

    constexpr size_t kBurstSize = 100;
    const size_t kIterations = cfg.targetWork / kBurstSize;

    std::vector<double> newDeleteSamples;
    std::vector<double> stdAllocSamples;
    std::vector<double> blockSamples;
    std::vector<double> poolSamples;

    newDeleteSamples.reserve(cfg.measuredBatches);
    stdAllocSamples.reserve(cfg.measuredBatches);
    blockSamples.reserve(cfg.measuredBatches);
    poolSamples.reserve(cfg.measuredBatches);

    // Warmup
    for (size_t w = 0; w < cfg.warmupRuns; ++w)
    {
        fat_p::NewDeleteAllocator<int> alloc;
        std::array<int*, kBurstSize> ptrs;
        for (size_t i = 0; i < kBurstSize; ++i)
            ptrs[i] = alloc.allocate(static_cast<int>(i));
        for (auto p : ptrs)
            alloc.deallocate(p);
    }

    // Measured batches (round-robin)
    for (size_t batch = 0; batch < cfg.measuredBatches; ++batch)
    {
        // NewDeleteAllocator
        {
            auto start = Clock::now();
            for (size_t iter = 0; iter < kIterations; ++iter)
            {
                fat_p::NewDeleteAllocator<int> alloc;
                std::array<int*, kBurstSize> ptrs;
                for (size_t i = 0; i < kBurstSize; ++i)
                {
                    ptrs[i] = alloc.allocate(static_cast<int>(i));
                }
                doNotOptimize(ptrs);
                for (auto p : ptrs)
                {
                    alloc.deallocate(p);
                }
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            newDeleteSamples.push_back(ns);
        }

        // std::allocator
        {
            auto start = Clock::now();
            for (size_t iter = 0; iter < kIterations; ++iter)
            {
                std::allocator<int> alloc;
                std::array<int*, kBurstSize> ptrs;
                for (size_t i = 0; i < kBurstSize; ++i)
                {
                    ptrs[i] = alloc.allocate(1);
                    *ptrs[i] = static_cast<int>(i);
                }
                doNotOptimize(ptrs);
                for (auto p : ptrs)
                {
                    alloc.deallocate(p, 1);
                }
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            stdAllocSamples.push_back(ns);
        }

        // BlockAllocator
        {
            auto start = Clock::now();
            for (size_t iter = 0; iter < kIterations; ++iter)
            {
                fat_p::BlockAllocator<PointerSized> alloc;
                std::array<PointerSized*, kBurstSize> ptrs;
                for (size_t i = 0; i < kBurstSize; ++i)
                {
                    ptrs[i] = alloc.allocate(static_cast<int64_t>(i));
                }
                doNotOptimize(ptrs);
                for (auto p : ptrs)
                {
                    alloc.deallocate(p);
                }
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            blockSamples.push_back(ns);
        }

        // PoolAllocator
        {
            auto start = Clock::now();
            for (size_t iter = 0; iter < kIterations; ++iter)
            {
                fat_p::PoolAllocator<kBurstSize>::Allocator<TrivialNode> alloc;
                std::array<TrivialNode*, kBurstSize> ptrs;
                for (size_t i = 0; i < kBurstSize; ++i)
                {
                    ptrs[i] = alloc.allocate(static_cast<int64_t>(i), static_cast<int64_t>(i));
                }
                doNotOptimize(ptrs);
                for (auto p : ptrs)
                {
                    alloc.deallocate(p);
                }
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            poolSamples.push_back(ns);
        }
    }

    Stats newDeleteStats = computeStats(newDeleteSamples);
    Stats stdAllocStats = computeStats(stdAllocSamples);
    Stats blockStats = computeStats(blockSamples);
    Stats poolStats = computeStats(poolSamples);

    printResultHeader();
    printResult("fat_p::NewDeleteAllocator", newDeleteStats);
    printResult("std::allocator", stdAllocStats);
    printResult("fat_p::BlockAllocator", blockStats);
    printResult("fat_p::PoolAllocator", poolStats);

    recordResult("AllocationStrategies", "BurstAlloc100", "NewDeleteAllocator", newDeleteStats);
    recordResult("AllocationStrategies", "BurstAlloc100", "std::allocator", stdAllocStats);
    recordResult("AllocationStrategies", "BurstAlloc100", "BlockAllocator", blockStats);
    recordResult("AllocationStrategies", "BurstAlloc100", "PoolAllocator", poolStats);

    printCpuContext("End");
}

} // anonymous namespace

// =============================================================================
// Benchmark: Churn Pattern (Mixed Alloc/Dealloc)
// =============================================================================

namespace
{

void benchChurnPattern(const BenchConfig& cfg)
{
    printHeader("Churn Pattern (Steady-State Mixed Operations)");
    printCpuContext("Start");
    printContractNote("Simulates container churn with interleaved alloc/dealloc. "
                      "Free list reuse should show advantage for BlockAllocator/PoolAllocator.");

    const size_t kIterations = cfg.targetWork;
    std::mt19937_64 rng(cfg.seed);

    std::vector<double> newDeleteSamples;
    std::vector<double> blockSamples;
    std::vector<double> poolSamples;

    newDeleteSamples.reserve(cfg.measuredBatches);
    blockSamples.reserve(cfg.measuredBatches);
    poolSamples.reserve(cfg.measuredBatches);

    // Pre-warm allocators to steady state
    fat_p::BlockAllocator<PointerSized> blockAlloc;
    fat_p::PoolAllocator<500>::Allocator<TrivialNode> poolAlloc;

    // Prime with some allocations
    std::vector<PointerSized*> blockPrimed;
    std::vector<TrivialNode*> poolPrimed;
    for (int i = 0; i < 200; ++i)
    {
        blockPrimed.push_back(blockAlloc.allocate(i));
        poolPrimed.push_back(poolAlloc.allocate(i, i));
    }
    // Return half to free list
    for (int i = 0; i < 100; ++i)
    {
        blockAlloc.deallocate(blockPrimed.back());
        blockPrimed.pop_back();
        poolAlloc.deallocate(poolPrimed.back());
        poolPrimed.pop_back();
    }

    // Warmup
    for (size_t w = 0; w < cfg.warmupRuns; ++w)
    {
        for (size_t i = 0; i < kIterations / 10; ++i)
        {
            PointerSized* p = blockAlloc.allocate(static_cast<int64_t>(i));
            doNotOptimize(p);
            blockAlloc.deallocate(p);
        }
    }

    // Measured batches
    for (size_t batch = 0; batch < cfg.measuredBatches; ++batch)
    {
        // NewDeleteAllocator (no free list benefit)
        {
            fat_p::NewDeleteAllocator<PointerSized> alloc;
            auto start = Clock::now();
            for (size_t i = 0; i < kIterations; ++i)
            {
                PointerSized* p = alloc.allocate(static_cast<int64_t>(i));
                doNotOptimize(p);
                alloc.deallocate(p);
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            newDeleteSamples.push_back(ns);
        }

        // BlockAllocator (free list reuse)
        {
            auto start = Clock::now();
            for (size_t i = 0; i < kIterations; ++i)
            {
                PointerSized* p = blockAlloc.allocate(static_cast<int64_t>(i));
                doNotOptimize(p);
                blockAlloc.deallocate(p);
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            blockSamples.push_back(ns);
        }

        // PoolAllocator (free list reuse)
        {
            auto start = Clock::now();
            for (size_t i = 0; i < kIterations; ++i)
            {
                TrivialNode* p = poolAlloc.allocate(static_cast<int64_t>(i),
                                                    static_cast<int64_t>(i));
                doNotOptimize(p);
                poolAlloc.deallocate(p);
            }
            auto end = Clock::now();
            double ns = Duration(end - start).count() / static_cast<double>(kIterations);
            poolSamples.push_back(ns);
        }
    }

    // Cleanup primed allocations
    for (auto* p : blockPrimed)
        blockAlloc.deallocate(p);
    for (auto* p : poolPrimed)
        poolAlloc.deallocate(p);

    Stats newDeleteStats = computeStats(newDeleteSamples);
    Stats blockStats = computeStats(blockSamples);
    Stats poolStats = computeStats(poolSamples);

    printResultHeader();
    printResult("fat_p::NewDeleteAllocator", newDeleteStats);
    printResult("fat_p::BlockAllocator (warmed)", blockStats);
    printResult("fat_p::PoolAllocator (warmed)", poolStats);

    recordResult("AllocationStrategies", "ChurnPattern", "NewDeleteAllocator", newDeleteStats);
    recordResult("AllocationStrategies", "ChurnPattern", "BlockAllocator", blockStats);
    recordResult("AllocationStrategies", "ChurnPattern", "PoolAllocator", poolStats);

    printCpuContext("End");
}

} // anonymous namespace

// =============================================================================
// Benchmark: Size Scaling
// =============================================================================

namespace
{

void benchSizeScaling(const BenchConfig& cfg)
{
    printHeader("Size Scaling (Allocation Count)");
    printCpuContext("Start");
    printContractNote("Measures how allocation time scales with number of live objects. "
                      "BlockAllocator should show constant time regardless of count.");

    const std::array<size_t, 4> kSizes = {100, 1000, 10000, 50000};

    for (size_t targetCount : kSizes)
    {
        printSubheader("N = " + std::to_string(targetCount));

        const size_t kIterations = cfg.targetWork / 10;

        std::vector<double> newDeleteSamples;
        std::vector<double> blockSamples;

        newDeleteSamples.reserve(cfg.measuredBatches);
        blockSamples.reserve(cfg.measuredBatches);

        // Measured batches
        for (size_t batch = 0; batch < cfg.measuredBatches; ++batch)
        {
            // NewDeleteAllocator
            {
                fat_p::NewDeleteAllocator<PointerSized> alloc;
                std::vector<PointerSized*> ptrs;
                ptrs.reserve(targetCount);

                // Fill to target count
                for (size_t i = 0; i < targetCount; ++i)
                {
                    ptrs.push_back(alloc.allocate(static_cast<int64_t>(i)));
                }

                // Measure additional allocations at steady state
                auto start = Clock::now();
                for (size_t i = 0; i < kIterations; ++i)
                {
                    PointerSized* p = alloc.allocate(static_cast<int64_t>(i));
                    doNotOptimize(p);
                    alloc.deallocate(p);
                }
                auto end = Clock::now();
                double ns = Duration(end - start).count() / static_cast<double>(kIterations);
                newDeleteSamples.push_back(ns);

                // Cleanup
                for (auto* p : ptrs)
                    alloc.deallocate(p);
            }

            // BlockAllocator
            {
                fat_p::BlockAllocator<PointerSized> alloc;
                std::vector<PointerSized*> ptrs;
                ptrs.reserve(targetCount);

                for (size_t i = 0; i < targetCount; ++i)
                {
                    ptrs.push_back(alloc.allocate(static_cast<int64_t>(i)));
                }

                auto start = Clock::now();
                for (size_t i = 0; i < kIterations; ++i)
                {
                    PointerSized* p = alloc.allocate(static_cast<int64_t>(i));
                    doNotOptimize(p);
                    alloc.deallocate(p);
                }
                auto end = Clock::now();
                double ns = Duration(end - start).count() / static_cast<double>(kIterations);
                blockSamples.push_back(ns);

                for (auto* p : ptrs)
                    alloc.deallocate(p);
            }
        }

        Stats newDeleteStats = computeStats(newDeleteSamples);
        Stats blockStats = computeStats(blockSamples);

        printResultHeader();
        printResult("fat_p::NewDeleteAllocator", newDeleteStats);
        printResult("fat_p::BlockAllocator", blockStats);

        std::string caseName = "SizeScaling_N" + std::to_string(targetCount);
        recordResult("AllocationStrategies", caseName, "NewDeleteAllocator", newDeleteStats);
        recordResult("AllocationStrategies", caseName, "BlockAllocator", blockStats);
    }

    printCpuContext("End");
}

} // anonymous namespace

// =============================================================================
// Main
// =============================================================================

int main()
{
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  Fat-P AllocationStrategies Benchmark Suite\n";
    std::cout << std::string(80, '=') << "\n\n";

    BenchConfig cfg = loadConfig();
    printConfig(cfg);

    BenchmarkScope scope(cfg.noScope);

    // Run benchmarks
    benchSingleAllocation(cfg);

    if (!cfg.noCooldown)
    {
        std::cout << "\n[Cooldown: 100ms]\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    benchBurstAllocation(cfg);

    if (!cfg.noCooldown)
    {
        std::cout << "\n[Cooldown: 100ms]\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    benchChurnPattern(cfg);

    if (!cfg.noCooldown)
    {
        std::cout << "\n[Cooldown: 100ms]\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    benchSizeScaling(cfg);

    // Write output files
    if (!cfg.csvPath.empty())
    {
        writeCsv(cfg.csvPath, cfg);
    }
    if (!cfg.jsonPath.empty())
    {
        writeJson(cfg.jsonPath, cfg);
    }

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << std::string(80, '=') << "\n";

    return 0;
}
