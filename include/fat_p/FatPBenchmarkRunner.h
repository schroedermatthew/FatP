#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPBenchmarkRunner
  file_role: public_header
  path: include/fat_p/FatPBenchmarkRunner.h
  namespace: fat_p
  layer: Testing
  summary: "Public header for FatPBenchmarkRunner."
  api_stability: in_work
  related:
    docs_search: "FatPBenchmarkRunner"
    tests:
      - components/FatPBenchmarkRunner/tests/test_FatPBenchmarkRunner.cpp
    benchmarks:
      - components/AlignedVector/benchmarks/benchmark_AlignedVector.cpp
      - components/AllocationStrategies/benchmarks/benchmark_AllocationStrategies.cpp
      - benchmarks/benchmark_EqualityComparisonsAny.cpp
      - components/FatPHashMap/benchmarks/benchmark_FatPHashMap.cpp
      - components/FeatureManager/benchmarks/benchmark_FeatureManager.cpp
      - components/FlatMapSet/benchmarks/benchmark_FlatMapSet.cpp
      - components/FloatingPointComparison/benchmarks/benchmark_FloatingPointComparison.cpp
      - components/PolicyIterator/benchmarks/benchmark_PolicyIterator.cpp
      - components/SlotMap/benchmarks/benchmark_SlotMap.cpp
      - components/SmallVector/benchmarks/benchmark_SmallVector.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 4
    defines_unprefixed: 3
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file FatPBenchmarkRunner.h
 * @brief Unified benchmark infrastructure for Fat-P components.
 *
 * @details
 * Comprehensive benchmarking toolkit that consolidates all Fat-P benchmark
 * infrastructure into a single header. Provides:
 *
 * - CPU frequency monitoring with CPUID and PDH support
 * - Statistical analysis with CI95 confidence intervals
 * - Round-robin multi-library comparison with randomized execution order
 * - FATP_BENCH_* environment variable configuration
 * - Style-guide compliant CSV/JSON export
 * - Readable, compact output formatting
 * - Windows priority/affinity optimization (BenchmarkScope)
 * - Dead-code elimination prevention (DoNotOptimize)
 * - Thread synchronization primitives (SpinBarrier)
 *
 * Design Invariants:
 *   1. Each measured run executes exactly one timed iteration per library
 *   2. Library execution order is randomized per run (round-robin)
 *   3. Setup/teardown occur outside timed regions
 *   4. All libraries observe the same distribution of machine states
 *   5. Medians are the primary reported statistic
 *
 * @version 1.0.0
 * @date 2025-01
 *
 * @section usage Basic Usage
 * @code
 * #include "FatPBenchmarkRunner.h"
 *
 * int main() {
 *     using namespace fat_p::bench;
 *
 *     BenchmarkRunner runner("MyComponent");
 *
 *     runner.section("CORE OPERATIONS")
 *           .contract("Operation X is O(1) amortized");
 *
 *     runner.add("op_x", [&]() {
 *         auto result = component.do_x();
 *         DoNotOptimize(result);
 *     });
 *
 *     runner.run();
 *     runner.printReport();
 *     return 0;
 * }
 * @endcode
 *
 * @section multi Multi-Library Comparison
 * @code
 * runner.addLibrary<FatPAdapter>();
 * runner.addLibrary<StdAdapter>();
 *
 * runner.compare("Insert N", [](IAdapter* a, const Inputs& in) {
 *     return a->run_insert(in);
 * });
 *
 * runner.run();  // Automatic round-robin
 * @endcode
 *
 * Compilation: Requires C++17
 *   g++ -std=c++17 -O3 -DNDEBUG -march=native benchmark.cpp -o benchmark
 *   cl /std:c++17 /O2 /DNDEBUG /EHsc benchmark.cpp
 */

// ============================================================================
// MSVC Compatibility - Must be before ANY includes
// ============================================================================
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#define FATP_DEFINED_CRT_SECURE_NO_WARNINGS_BENCH
#endif
#pragma warning(push)
#pragma warning(disable : 4996) // deprecated functions
#pragma warning(disable : 4267) // size_t to unsigned int
#pragma warning(disable : 4244) // possible loss of data
#endif

#ifndef NOMINMAX
#define NOMINMAX
#define FATP_DEFINED_NOMINMAX_BENCH
#endif

// ============================================================================
// Includes
// ============================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#if defined(__linux__)
#include <time.h>
#endif
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define FATP_DEFINED_WIN32_LEAN_AND_MEAN_BENCH
#endif
#include <intrin.h> // For __cpuid
#include <windows.h>
#include <winreg.h>

#ifndef FATP_ENABLE_PDH_STATS
#define FATP_ENABLE_PDH_STATS
#endif
#endif

namespace fat_p
{
namespace bench
{

// ============================================================================
// CPU Frequency Monitoring
// ============================================================================

/**
 * @brief Captured CPU frequency information.
 *
 * @details
 * On Linux, frequency is read from sysfs cpufreq interfaces.
 * On Windows, base frequency is read from the registry and the current
 * frequency is sampled via PDH when available.
 *
 * IMPORTANT: On Windows, the registry ~MHz value is often the TURBO frequency,
 * not the sustainable base. This means a CPU running at its actual base may
 * appear "throttled" when compared to turbo. The is_throttled() method uses
 * a 40% threshold to account for this.
 */
struct CpuFreqInfo
{
    double mRefFreqMHz = 0.0;
    double mCurrentFreqMHz = 0.0;

    // True if the reference is a turbo/max fallback rather than a base clock.
    bool mRefIsMax = false;

    // True if the reference base was estimated (e.g., 70% of registry turbo)
    bool mRefIsEstimated = false;

    // True if the current frequency is not dynamically sampled.
    bool mCurrentIsEstimated = false;

    [[nodiscard]] double throttle_percentage() const
    {
        if (mCurrentFreqMHz <= 0.0 || mRefFreqMHz <= 0.0)
        {
            return 0.0;
        }
        return (1.0 - mCurrentFreqMHz / mRefFreqMHz) * 100.0;
    }

    [[nodiscard]] bool has_reliable_detection() const
    {
        // We can detect throttling if we have both measurements.
        // Estimated base is still useful for throttle detection.
        return !mCurrentIsEstimated && mRefFreqMHz > 0.0 && mCurrentFreqMHz > 0.0;
    }

    /// Check if CPU is significantly throttled.
    /// With estimated base, use slightly higher threshold (15%) to account for estimation error.
    [[nodiscard]] bool is_throttled(double thresholdPct = 10.0) const
    {
        double effectiveThreshold = mRefIsEstimated ? std::max(thresholdPct, 15.0) : thresholdPct;
        return has_reliable_detection() && throttle_percentage() > effectiveThreshold;
    }

    [[nodiscard]] bool is_turbo() const
    {
        return has_reliable_detection() && mCurrentFreqMHz > mRefFreqMHz * 1.05;
    }

    [[nodiscard]] const char* ref_label() const
    {
        if (mRefFreqMHz <= 0.0)
        {
            return "ref";
        }
        if (mRefIsMax)
        {
            return "max";
        }
        if (mRefIsEstimated)
        {
            return "~base";
        }
        // mRefIsAccurate would be set when from CPUID
        // If not accurate and not estimated, it's from registry
        return "base";
    }
};

namespace detail
{

#if defined(_WIN32) || defined(_WIN64)

inline double read_registry_base_freq_mhz()
{
    DWORD freq = 0;
    DWORD size = sizeof(DWORD);
    HKEY hKey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) ==
        ERROR_SUCCESS)
    {
        RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&freq), &size);
        RegCloseKey(hKey);
    }

    return static_cast<double>(freq);
}

// ============================================================================
// CPUID-based Frequency Detection (Intel Skylake+)
// ============================================================================

/**
 * @brief CPU frequency information from CPUID leaf 0x16.
 *
 * @details
 * CPUID leaf 0x16 (Processor Frequency Information) is supported on Intel
 * Skylake and later processors. It provides accurate base, max, and bus
 * frequencies directly from the CPU.
 *
 * AMD processors do not support this leaf - they use different mechanisms.
 */
struct CpuidFrequencies
{
    uint16_t mBaseMHz = 0;   ///< Processor Base Frequency (sustainable)
    uint16_t mMaxMHz = 0;    ///< Maximum Frequency (turbo)
    uint16_t mBusMHz = 0;    ///< Bus (Reference) Frequency
    bool mSupported = false; ///< True if CPUID 0x16 is available and valid

    // Diagnostic fields
    int mMaxLeaf = 0;     ///< Maximum supported CPUID leaf
    uint32_t mRawEAX = 0; ///< Raw EAX from leaf 0x16
    uint32_t mRawEBX = 0; ///< Raw EBX from leaf 0x16
    uint32_t mRawECX = 0; ///< Raw ECX from leaf 0x16
};

/**
 * @brief Query CPU frequencies via CPUID leaf 0x16.
 *
 * @return Frequency info; check mSupported before using values.
 *
 * @details
 * Leaf 0x16 returns:
 *   EAX: Processor Base Frequency (MHz)
 *   EBX: Maximum Frequency (MHz)
 *   ECX: Bus (Reference) Frequency (MHz)
 *
 * This is the most accurate way to get base frequency on supported Intel CPUs.
 */
inline CpuidFrequencies query_cpuid_frequencies()
{
    CpuidFrequencies result{};

    int cpuInfo[4] = {0};

    // Get maximum supported CPUID leaf
    __cpuid(cpuInfo, 0);
    result.mMaxLeaf = cpuInfo[0];

    // Check if leaf 0x16 is supported
    if (result.mMaxLeaf < 0x16)
    {
        return result; // Not supported (AMD or older Intel)
    }

    // Query leaf 0x16: Processor Frequency Information
    __cpuid(cpuInfo, 0x16);

    // Store raw values for diagnostics
    result.mRawEAX = static_cast<uint32_t>(cpuInfo[0]);
    result.mRawEBX = static_cast<uint32_t>(cpuInfo[1]);
    result.mRawECX = static_cast<uint32_t>(cpuInfo[2]);

    result.mBaseMHz = static_cast<uint16_t>(cpuInfo[0] & 0xFFFF); // EAX[15:0]
    result.mMaxMHz = static_cast<uint16_t>(cpuInfo[1] & 0xFFFF);  // EBX[15:0]
    result.mBusMHz = static_cast<uint16_t>(cpuInfo[2] & 0xFFFF);  // ECX[15:0]

    // Validate - base should be non-zero and less than or equal to max
    result.mSupported = (result.mBaseMHz > 0) && (result.mMaxMHz == 0 || result.mBaseMHz <= result.mMaxMHz);

    return result;
}

/**
 * @brief Get the best available base frequency estimate.
 *
 * @return Base frequency in MHz, or 0 if unavailable.
 *
 * @details
 * Priority:
 *   1. CPUID leaf 0x16 base frequency (most accurate)
 *   2. Registry ~MHz (may be turbo on some systems)
 *
 * Also returns whether the value is known to be accurate (from CPUID)
 * or potentially inflated (from registry).
 */
struct BaseFrequencyResult
{
    double mBaseMHz = 0.0;
    double mMaxMHz = 0.0;      ///< Turbo/max frequency if known
    bool mIsAccurate = false;  ///< True if from CPUID (not registry guess)
    bool mIsEstimated = false; ///< True if base was estimated from max
};

inline BaseFrequencyResult get_best_base_frequency()
{
    BaseFrequencyResult result{};

    // Try CPUID first (accurate on Intel Skylake through Raptor Lake)
    const CpuidFrequencies cpuid = query_cpuid_frequencies();

    if (cpuid.mSupported)
    {
        result.mBaseMHz = static_cast<double>(cpuid.mBaseMHz);
        result.mMaxMHz = static_cast<double>(cpuid.mMaxMHz);
        result.mIsAccurate = true;
        result.mIsEstimated = false;
        return result;
    }

    // CPUID failed - use registry value
    // On modern Intel (including Arrow Lake where CPUID 0x16 returns zeros),
    // the registry typically reports the P-core base frequency, not turbo.
    // On older systems or some configurations, it might report turbo.
    // We use it directly as our reference - PDH will measure actual current.
    const double regMHz = read_registry_base_freq_mhz();
    result.mBaseMHz = regMHz;
    result.mMaxMHz = regMHz; // We don't know true max without CPUID
    result.mIsAccurate = false;
    result.mIsEstimated = false; // Using registry directly, not estimating

    return result;
}

#if defined(FATP_ENABLE_PDH_STATS)

/**
 * @brief PDH-backed CPU performance monitor for Windows.
 *
 * @details
 * Uses PDH counters to sample CPU performance as a percentage. This enables
 * detection of throttling and turbo relative to the registry base frequency.
 *
 * Limitations:
 * - English Windows only (hardcoded counter strings)
 * - PDH must be available (pdh.dll loaded dynamically)
 */
class PdhCpuMonitor
{
private:
    using PDH_HQUERY = void*;
    using PDH_HCOUNTER = void*;
    using PDH_STATUS = long;

    struct PDH_FMT_COUNTERVALUE
    {
        DWORD CStatus;
        union
        {
            LONG longValue;
            double doubleValue;
            LONGLONG largeValue;
            LPCSTR AnsiStringValue;
            LPCWSTR WideStringValue;
        };
    };

    static constexpr DWORD kPdhFmtDouble = 0x00000200;
    static constexpr PDH_STATUS kPdhOk = 0;

    using PdhOpenQueryA_t = PDH_STATUS(WINAPI*)(LPCSTR, DWORD_PTR, PDH_HQUERY*);
    using PdhAddCounterA_t = PDH_STATUS(WINAPI*)(PDH_HQUERY, LPCSTR, DWORD_PTR, PDH_HCOUNTER*);
    using PdhCollectQueryData_t = PDH_STATUS(WINAPI*)(PDH_HQUERY);
    using PdhGetFormattedCounterValue_t = PDH_STATUS(WINAPI*)(PDH_HCOUNTER, DWORD, LPDWORD, PDH_FMT_COUNTERVALUE*);
    using PdhCloseQuery_t = PDH_STATUS(WINAPI*)(PDH_HQUERY);

    HMODULE mPdh = nullptr;
    PDH_HQUERY mQuery = nullptr;
    PDH_HCOUNTER mCounter = nullptr;

    PdhOpenQueryA_t mOpenQuery = nullptr;
    PdhAddCounterA_t mAddCounter = nullptr;
    PdhCollectQueryData_t mCollect = nullptr;
    PdhGetFormattedCounterValue_t mGetValue = nullptr;
    PdhCloseQuery_t mCloseQuery = nullptr;

    bool mInitialized = false;

public:
    PdhCpuMonitor()
    {
        mPdh = LoadLibraryA("pdh.dll");
        if (!mPdh)
        {
            return;
        }

        mOpenQuery = reinterpret_cast<PdhOpenQueryA_t>(GetProcAddress(mPdh, "PdhOpenQueryA"));
        mAddCounter = reinterpret_cast<PdhAddCounterA_t>(GetProcAddress(mPdh, "PdhAddCounterA"));
        mCollect = reinterpret_cast<PdhCollectQueryData_t>(GetProcAddress(mPdh, "PdhCollectQueryData"));
        mGetValue =
            reinterpret_cast<PdhGetFormattedCounterValue_t>(GetProcAddress(mPdh, "PdhGetFormattedCounterValue"));
        mCloseQuery = reinterpret_cast<PdhCloseQuery_t>(GetProcAddress(mPdh, "PdhCloseQuery"));

        if (!mOpenQuery || !mAddCounter || !mCollect || !mGetValue || !mCloseQuery)
        {
            return;
        }

        if (mOpenQuery(nullptr, 0, &mQuery) != kPdhOk)
        {
            return;
        }

        const char* counterPath = "\\Processor Information(_Total)\\% of Maximum Frequency";
        if (mAddCounter(mQuery, counterPath, 0, &mCounter) != kPdhOk)
        {
            counterPath = "\\Processor(_Total)\\% Processor Performance";
            if (mAddCounter(mQuery, counterPath, 0, &mCounter) != kPdhOk)
            {
                return;
            }
        }

        // Prime the counter (first read can be invalid for some counters).
        mCollect(mQuery);
        mInitialized = true;
    }

    ~PdhCpuMonitor()
    {
        if (mQuery && mCloseQuery)
        {
            mCloseQuery(mQuery);
        }
        if (mPdh)
        {
            FreeLibrary(mPdh);
        }
    }

    PdhCpuMonitor(const PdhCpuMonitor&) = delete;
    PdhCpuMonitor& operator=(const PdhCpuMonitor&) = delete;

    [[nodiscard]] bool is_available() const
    {
        return mInitialized;
    }

    /**
     * @brief Returns current CPU performance as a percentage.
     * @return Percent (0.0 on error).
     */
    [[nodiscard]] double get_frequency_percentage()
    {
        if (!mInitialized)
        {
            return 0.0;
        }

        if (mCollect(mQuery) != kPdhOk)
        {
            return 0.0;
        }

        PDH_FMT_COUNTERVALUE value{};
        if (mGetValue(mCounter, kPdhFmtDouble, nullptr, &value) == kPdhOk)
        {
            return value.doubleValue;
        }

        return 0.0;
    }
};

#endif // FATP_ENABLE_PDH_STATS

#else

inline bool read_int64_from_file(const char* path, int64_t& outValue)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        return false;
    }

    int64_t v = 0;
    if (!(f >> v))
    {
        return false;
    }

    outValue = v;
    return true;
}

inline bool read_proc_cpuinfo_double(const char* key, double& outValue)
{
    std::ifstream f("/proc/cpuinfo");
    if (!f.is_open())
    {
        return false;
    }

    std::string line;
    const std::string keyStr(key);

    while (std::getline(f, line))
    {
        // Example: "cpu MHz\t\t: 2793.123"
        if (line.size() >= keyStr.size() && line.compare(0, keyStr.size(), keyStr) == 0)
        {
            const auto colon = line.find(':');
            if (colon == std::string::npos)
            {
                return false;
            }

            const std::string valueStr = line.substr(colon + 1);
            try
            {
                outValue = std::stod(valueStr);
                return outValue > 0.0;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    return false;
}

inline bool read_proc_cpuinfo_model_base_mhz(double& outMHz)
{
    std::ifstream f("/proc/cpuinfo");
    if (!f.is_open())
    {
        return false;
    }

    std::string line;
    const std::string keyStr("model name");

    while (std::getline(f, line))
    {
        if (line.size() >= keyStr.size() && line.compare(0, keyStr.size(), keyStr) == 0)
        {
            const auto colon = line.find(':');
            if (colon == std::string::npos)
            {
                return false;
            }

            // Example: "Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz"
            std::string model = line.substr(colon + 1);
            const auto at = model.find('@');
            if (at == std::string::npos)
            {
                return false;
            }

            std::string freq = model.substr(at + 1);
            const auto first = freq.find_first_not_of(" \t");
            if (first == std::string::npos)
            {
                return false;
            }
            freq = freq.substr(first);

            std::size_t idx = 0;
            double v = 0.0;
            try
            {
                v = std::stod(freq, &idx);
            }
            catch (...)
            {
                return false;
            }

            const std::string unit = freq.substr(idx);

            if (unit.find("GHz") != std::string::npos || unit.find("ghz") != std::string::npos)
            {
                outMHz = v * 1000.0;
                return outMHz > 0.0;
            }

            if (unit.find("MHz") != std::string::npos || unit.find("mhz") != std::string::npos)
            {
                outMHz = v;
                return outMHz > 0.0;
            }

            return false;
        }
    }

    return false;
}

#endif

} // namespace detail

/**
 * @brief Capture CPU frequency information.
 *
 * @complexity O(1)
 * @thread_safety Thread-safe (uses function-local statics).
 *
 * @details
 * On Windows, uses CPUID leaf 0x16 for accurate base frequency when available
 * (Intel Skylake+). Falls back to registry ~MHz which may report turbo instead
 * of base on some systems.
 */
[[nodiscard]] inline CpuFreqInfo capture_cpu_frequency()
{
    CpuFreqInfo info;

#if defined(_WIN32) || defined(_WIN64)
    // Get best available base frequency (CPUID preferred, estimated from registry otherwise)
    const detail::BaseFrequencyResult baseInfo = detail::get_best_base_frequency();

    // Use base frequency (accurate from CPUID, or estimated as 70% of registry turbo)
    info.mRefFreqMHz = baseInfo.mBaseMHz;
    info.mRefIsMax = false;                       // We're using base (actual or estimated), not max
    info.mRefIsEstimated = baseInfo.mIsEstimated; // Track if it's an estimate
    info.mCurrentFreqMHz = baseInfo.mBaseMHz;
    info.mCurrentIsEstimated = true;

#if defined(FATP_ENABLE_PDH_STATS)
    static detail::PdhCpuMonitor sMonitor;
    if (sMonitor.is_available())
    {
        const double pct = sMonitor.get_frequency_percentage();
        if (pct > 0.0)
        {
            // PDH returns percentage of max frequency
            const double maxMHz = baseInfo.mMaxMHz;
            info.mCurrentFreqMHz = maxMHz * (pct / 100.0);
            info.mCurrentIsEstimated = false;
        }
    }
#endif

#else
    int64_t khz = 0;
    if (detail::read_int64_from_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", khz))
    {
        info.mCurrentFreqMHz = static_cast<double>(khz) / 1000.0;
        info.mCurrentIsEstimated = false;
    }
    else
    {
        info.mCurrentIsEstimated = true;

        double mhz = 0.0;
        if (detail::read_proc_cpuinfo_double("cpu MHz", mhz))
        {
            info.mCurrentFreqMHz = mhz;
        }
    }

    if (detail::read_int64_from_file("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency", khz))
    {
        info.mRefFreqMHz = static_cast<double>(khz) / 1000.0;
        info.mRefIsMax = false;
    }
    else if (detail::read_int64_from_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", khz))
    {
        info.mRefFreqMHz = static_cast<double>(khz) / 1000.0;
        info.mRefIsMax = true;
    }

    // If cpufreq sysfs is unavailable (common in containers/CI), fall back to /proc/cpuinfo.
    if (info.mRefFreqMHz <= 0.0)
    {
        double base_mhz = 0.0;
        if (detail::read_proc_cpuinfo_model_base_mhz(base_mhz))
        {
            info.mRefFreqMHz = base_mhz;
            info.mRefIsEstimated = true;
            info.mRefIsMax = false;
        }
        else if (info.mCurrentFreqMHz > 0.0)
        {
            // Last resort: treat current as reference to avoid printing 0 MHz.
            info.mRefFreqMHz = info.mCurrentFreqMHz;
            info.mRefIsEstimated = true;
            info.mRefIsMax = false;
        }
    }

#endif

    return info;
}

/**
 * @brief Print detailed CPU frequency detection diagnostics.
 *
 * @details
 * Shows which detection method is being used (CPUID vs registry) and the
 * raw values obtained. Useful for debugging frequency detection issues.
 */
inline void print_cpu_detection_info(std::ostream& out)
{
    out << "CPU Frequency Detection Diagnostics:\n";

#if defined(_WIN32) || defined(_WIN64)
    // Show CPUID results
    const detail::CpuidFrequencies cpuid = detail::query_cpuid_frequencies();
    out << "  CPUID Leaf 0x16: ";
    if (cpuid.mSupported)
    {
        out << "SUPPORTED\n";
        out << "    Base frequency:  " << cpuid.mBaseMHz << " MHz (sustainable)\n";
        out << "    Max frequency:   " << cpuid.mMaxMHz << " MHz (turbo)\n";
        out << "    Bus frequency:   " << cpuid.mBusMHz << " MHz\n";
    }
    else
    {
        out << "NOT SUPPORTED\n";
        out << "    Max CPUID leaf:  0x" << std::hex << cpuid.mMaxLeaf << std::dec << "\n";
        if (cpuid.mMaxLeaf >= 0x16)
        {
            // Leaf exists but validation failed - show raw values
            out << "    Raw EAX:         0x" << std::hex << cpuid.mRawEAX << std::dec << " (base: " << cpuid.mBaseMHz
                << " MHz)\n";
            out << "    Raw EBX:         0x" << std::hex << cpuid.mRawEBX << std::dec << " (max: " << cpuid.mMaxMHz
                << " MHz)\n";
            out << "    Raw ECX:         0x" << std::hex << cpuid.mRawECX << std::dec << " (bus: " << cpuid.mBusMHz
                << " MHz)\n";
            // Explain why validation failed
            if (cpuid.mBaseMHz == 0)
            {
                out << "    Reason:          Base frequency is 0\n";
            }
            else if (cpuid.mMaxMHz > 0 && cpuid.mBaseMHz > cpuid.mMaxMHz)
            {
                out << "    Reason:          Base (" << cpuid.mBaseMHz << ") > Max (" << cpuid.mMaxMHz << ")\n";
            }
        }
        else
        {
            out << "    Reason:          Leaf 0x16 not available (need >= 0x16)\n";
        }
    }

    // Show registry value
    const double regMHz = detail::read_registry_base_freq_mhz();
    out << "  Registry ~MHz:    " << static_cast<int>(regMHz) << " MHz";
    if (cpuid.mSupported && regMHz > 0)
    {
        if (std::abs(regMHz - cpuid.mMaxMHz) < 100)
        {
            out << " (matches CPUID max/turbo)";
        }
        else if (std::abs(regMHz - cpuid.mBaseMHz) < 100)
        {
            out << " (matches CPUID base)";
        }
        else
        {
            out << " (differs from CPUID)";
        }
    }
    out << "\n";

    // Show what we're using
    const detail::BaseFrequencyResult best = detail::get_best_base_frequency();
    out << "  Using:            ";
    if (best.mIsAccurate)
    {
        out << "CPUID base (" << static_cast<int>(best.mBaseMHz) << " MHz) - ACCURATE\n";
    }
    else if (best.mIsEstimated)
    {
        out << "Estimated base (" << static_cast<int>(best.mBaseMHz) << " MHz) - "
            << "derived from max (" << static_cast<int>(best.mMaxMHz) << " MHz)\n";
    }
    else
    {
        out << "Registry base (" << static_cast<int>(best.mBaseMHz) << " MHz) - "
            << "CPUID 0x16 unavailable, using registry directly\n";
    }

#if defined(FATP_ENABLE_PDH_STATS)
    // Show PDH availability
    static detail::PdhCpuMonitor sMonitor;
    out << "  PDH Monitor:      " << (sMonitor.is_available() ? "AVAILABLE" : "NOT AVAILABLE") << "\n";
    if (sMonitor.is_available())
    {
        const double pct = sMonitor.get_frequency_percentage();
        out << "    Current %:      " << std::fixed << std::setprecision(1) << pct << "%\n";
    }
#else
    out << "  PDH Monitor:      DISABLED\n";
#endif

#else // Linux
    out << "  Linux sysfs interface\n";
    int64_t khz = 0;
    if (detail::read_int64_from_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", khz))
    {
        out << "    Current freq:   " << (khz / 1000) << " MHz\n";
    }
    if (detail::read_int64_from_file("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency", khz))
    {
        out << "    Base freq:      " << (khz / 1000) << " MHz\n";
    }
    if (detail::read_int64_from_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", khz))
    {
        out << "    Max freq:       " << (khz / 1000) << " MHz\n";
    }
#endif

    // Show final captured state
    const CpuFreqInfo info = capture_cpu_frequency();
    out << "  Captured state:\n";
    out << "    Reference:      " << static_cast<int>(info.mRefFreqMHz) << " MHz (" << info.ref_label() << ")\n";
    out << "    Current:        " << static_cast<int>(info.mCurrentFreqMHz) << " MHz"
        << (info.mCurrentIsEstimated ? " (estimated)" : " (measured)") << "\n";
    out << "    Throttle %:     " << std::fixed << std::setprecision(1) << info.throttle_percentage() << "%\n";
    out << "    Reliable:       " << (info.has_reliable_detection() ? "YES" : "NO") << "\n";
}

/**
 * @brief Print a standard benchmark CPU/timestamp context line.
 *
 * Output format:
 *   [YYYY-MM-DD HH:MM:SS] <label?> CPU: <current> MHz (base: <ref>) [TURBO]
 *
 * @complexity O(1)
 * @thread_safety Thread-safe.
 */
inline void print_cpu_context(std::ostream& out, const char* label = nullptr)
{
    const CpuFreqInfo info = capture_cpu_frequency();

    const std::ios_base::fmtflags oldFlags = out.flags();
    const std::streamsize oldPrecision = out.precision();

    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);

    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif

    out << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] ";

    if (label)
    {
        out << label << " ";
    }

    out << "CPU: " << static_cast<int>(info.mCurrentFreqMHz) << " MHz";

    if (info.mRefFreqMHz > 0.0)
    {
        // Show reference frequency for informational purposes
        out << " (" << info.ref_label() << ": " << static_cast<int>(info.mRefFreqMHz) << ")";

        // Only show TURBO if clearly above reference (works on any architecture)
        if (info.is_turbo())
        {
            out << " [TURBO]";
        }
        // Note: We don't show THROTTLED anymore because on hybrid CPUs (Arrow Lake, etc.)
        // running below P-core base is normal (E-cores, efficiency modes, scheduler decisions)
    }

    out.flags(oldFlags);
    out.precision(oldPrecision);

    out << "\n";
}

// ============================================================================
// CPU Stabilization Wait
// ============================================================================

/**
 * @brief Configuration for CPU stabilization wait behavior.
 */
struct CpuWaitConfig
{
    /// If true, use fixed delays instead of waiting for throttle to clear.
    /// Set this when throttle detection is unreliable (e.g., virtualized, BIOS settings).
    bool mUseFixedDelays = false;

    /// Maximum time to wait for CPU to stabilize (seconds). 0 = no limit.
    int mMaxWaitSeconds = 30;

    /// Throttle threshold percentage. CPU is "stable" when below this.
    /// Set to 0 for auto-detection based on CPUID availability:
    ///   - With CPUID base freq: 10% (accurate detection)
    ///   - With registry (may be turbo): 50% (running at base isn't throttling)
    double mThrottleThreshold = 0.0; // 0 = auto

    /// Poll interval while waiting (milliseconds).
    int mPollIntervalMs = 500;

    /// Fixed delay for section transitions (milliseconds). Used when mUseFixedDelays=true.
    int mFixedSectionDelayMs = 2000;

    /// Fixed delay for size transitions (milliseconds). Used when mUseFixedDelays=true.
    int mFixedSizeDelayMs = 1000;

    /// Fixed delay for case transitions (milliseconds). Used when mUseFixedDelays=true.
    int mFixedCaseDelayMs = 300;
};

/**
 * @brief Result of a CPU stabilization wait.
 */
struct CpuWaitResult
{
    bool mStabilized = false;     ///< True if CPU stabilized within timeout
    bool mUsedFixedDelay = false; ///< True if fixed delay was used instead of waiting
    int mWaitedMs = 0;            ///< Actual time waited
    CpuFreqInfo mFinalState;      ///< CPU state after wait
};

/**
 * @brief Wait for CPU to stabilize or use fixed delay.
 *
 * @param out Stream for status messages (nullptr to suppress output)
 * @param config Wait configuration
 * @param label Optional label for log messages
 * @return Wait result with final CPU state
 *
 * @details
 * In fixed-delay mode, simply sleeps for the specified duration.
 * In dynamic mode, polls CPU frequency until throttling clears or timeout.
 *
 * Threshold auto-detection (when config.mThrottleThreshold == 0):
 *   - With CPUID base freq (accurate): 10% threshold
 *   - With registry fallback (may be turbo): 50% threshold
 *
 * @note On hybrid CPUs (Intel Arrow Lake, etc.), throttle-based detection may not
 *       work correctly because the CPU normally runs below P-core base (E-cores,
 *       efficiency modes). Consider using variance-based stability detection instead.
 */
inline CpuWaitResult
wait_for_cpu_stable(std::ostream* out, const CpuWaitConfig& config, int delayMs, const char* label = nullptr)
{
    CpuWaitResult result;

    if (config.mUseFixedDelays)
    {
        // Fixed delay mode - just sleep
        if (out)
        {
            *out << "[Fixed delay: " << delayMs << "ms";
            if (label)
            {
                *out << " (" << label << ")";
            }
            *out << "]\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        result.mUsedFixedDelay = true;
        result.mStabilized = true;
        result.mWaitedMs = delayMs;
        result.mFinalState = capture_cpu_frequency();
        return result;
    }

    // Dynamic wait mode - poll until stable or timeout
    const auto startTime = std::chrono::steady_clock::now();
    const int maxWaitMs = config.mMaxWaitSeconds * 1000;

    while (true)
    {
        result.mFinalState = capture_cpu_frequency();

        const auto elapsed = std::chrono::steady_clock::now() - startTime;
        result.mWaitedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

        // Determine effective threshold
        // - Auto (0): use 10% if CPUID worked (accurate base), 50% if registry (may be turbo)
        // - Manual: use configured value
        double effectiveThreshold = config.mThrottleThreshold;
        if (effectiveThreshold <= 0.0)
        {
            effectiveThreshold = result.mFinalState.mRefIsMax ? 50.0 : 10.0;
        }

        // Check if stable
        if (!result.mFinalState.has_reliable_detection() ||
            result.mFinalState.throttle_percentage() <= effectiveThreshold)
        {
            result.mStabilized = true;

            if (out)
            {
                *out << "[Ready: " << static_cast<int>(result.mFinalState.mCurrentFreqMHz) << "/"
                     << static_cast<int>(result.mFinalState.mRefFreqMHz) << " MHz";
                const double pct = result.mFinalState.throttle_percentage();
                if (pct > 5.0) // Only show if noticeable
                {
                    *out << " (" << std::fixed << std::setprecision(0) << pct << "% below "
                         << (result.mFinalState.mRefIsMax ? "max" : "base") << ")";
                }
                *out << "]\n";
            }
            return result;
        }

        // Check timeout
        if (maxWaitMs > 0 && result.mWaitedMs >= maxWaitMs)
        {
            result.mStabilized = false;

            if (out)
            {
                *out << "[WARNING: CPU still " << std::fixed << std::setprecision(0)
                     << result.mFinalState.throttle_percentage() << "% below "
                     << (result.mFinalState.mRefIsMax ? "max" : "base") << " after " << config.mMaxWaitSeconds << "s - "
                     << static_cast<int>(result.mFinalState.mCurrentFreqMHz) << "/"
                     << static_cast<int>(result.mFinalState.mRefFreqMHz) << " MHz]\n";
            }
            return result;
        }

        // Print progress every 5 seconds
        if (out && (result.mWaitedMs % 5000) < config.mPollIntervalMs)
        {
            *out << "[Waiting: " << static_cast<int>(result.mFinalState.mCurrentFreqMHz) << "/"
                 << static_cast<int>(result.mFinalState.mRefFreqMHz) << " MHz (" << std::fixed << std::setprecision(0)
                 << result.mFinalState.throttle_percentage() << "% below "
                 << (result.mFinalState.mRefIsMax ? "max" : "base") << ")]\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config.mPollIntervalMs));
    }
}

/**
 * @brief Convenience wrapper: wait for section transition.
 */
inline CpuWaitResult wait_section_cooling(std::ostream* out, const CpuWaitConfig& config)
{
    return wait_for_cpu_stable(out, config, config.mFixedSectionDelayMs, "section");
}

/**
 * @brief Convenience wrapper: wait for size transition.
 */
inline CpuWaitResult wait_size_cooling(std::ostream* out, const CpuWaitConfig& config)
{
    return wait_for_cpu_stable(out, config, config.mFixedSizeDelayMs, "size");
}

/**
 * @brief Convenience wrapper: wait for case transition.
 */
inline CpuWaitResult wait_case_cooling(std::ostream* out, const CpuWaitConfig& config)
{
    return wait_for_cpu_stable(out, config, config.mFixedCaseDelayMs, "case");
}

/**
 * @brief Parse command-line flag for fixed-delay mode.
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return true if --fixed-delays or -f flag is present
 *
 * Usage in benchmark main():
 * @code
 * CpuWaitConfig config;
 * config.mUseFixedDelays = parse_fixed_delay_flag(argc, argv);
 * @endcode
 */
inline bool parse_fixed_delay_flag(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--fixed-delays" || std::string(argv[i]) == "-f" ||
            std::string(argv[i]) == "--fixed")
        {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Benchmark Runner (rest of FatPBenchmarkRunner.h)
// ============================================================================

// ============================================================================
// Platform Defaults
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
inline constexpr std::size_t kDefaultWarmupRuns = 3;
inline constexpr std::size_t kDefaultMeasuredRuns = 15; // Windows: higher variance
inline constexpr int kDefaultCooldownSectionMs = 2000;
inline constexpr int kDefaultCooldownSizeMs = 1000;
inline constexpr int kDefaultCooldownCaseMs = 300;
#else
inline constexpr std::size_t kDefaultWarmupRuns = 3;
inline constexpr std::size_t kDefaultMeasuredRuns = 50;
inline constexpr int kDefaultCooldownSectionMs = 1000;
inline constexpr int kDefaultCooldownSizeMs = 500;
inline constexpr int kDefaultCooldownCaseMs = 200;
#endif

inline constexpr std::uint64_t kDefaultSeed = 12345;
inline constexpr std::size_t kDefaultMinBatchMs = 50;

// ============================================================================
// Dead-Code Elimination Prevention
// ============================================================================

/**
 * @brief Global sink to prevent dead-code elimination
 */
inline volatile std::int64_t g_benchmark_sink = 0;

/**
 * @brief Prevent compiler from optimizing away a value
 *
 * @details Uses volatile write to ensure the value is "used" from the
 * compiler's perspective, preventing DCE of the computation.
 */
template <typename T>
inline void DoNotOptimize(const T& value)
{
    // For integral types, XOR into sink
    if constexpr (std::is_integral_v<T> || std::is_pointer_v<T>)
    {
        g_benchmark_sink = g_benchmark_sink ^
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(static_cast<const void*>(&value)));
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        // For floating point, use bit representation
        union
        {
            T f;
            std::int64_t i;
        } u;
        u.f = value;
        g_benchmark_sink = g_benchmark_sink ^ u.i;
    }
    else
    {
        // For complex types, use address
        g_benchmark_sink = g_benchmark_sink ^ reinterpret_cast<std::int64_t>(&value);
    }
}

/**
 * @brief Simple prevent-optimization helper
 */
inline void preventOpt(std::int64_t value)
{
    g_benchmark_sink = g_benchmark_sink ^ value;
}

// ============================================================================
// Timer
// ============================================================================

/**
 * @brief High-resolution timer for benchmarking
 */
#if defined(__linux__)
struct MonotonicRawClock
{
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<MonotonicRawClock, duration>;
    static constexpr bool is_steady = true;

    static time_point now() noexcept
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        const auto ns = static_cast<rep>(ts.tv_sec) * 1000000000LL + static_cast<rep>(ts.tv_nsec);
        return time_point(duration(ns));
    }
};

using BenchClock = MonotonicRawClock;
#else
using BenchClock = std::chrono::steady_clock;
#endif

struct Timer
{
    using Clock = BenchClock;
    using TimePoint = Clock::time_point;

    TimePoint t0;

    void start()
    {
        t0 = Clock::now();
    }

    [[nodiscard]] double elapsedNs() const
    {
        auto t1 = Clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count();
    }

    [[nodiscard]] double elapsedUs() const
    {
        return elapsedNs() / 1000.0;
    }
    [[nodiscard]] double elapsedMs() const
    {
        return elapsedNs() / 1000000.0;
    }
    [[nodiscard]] double elapsedS() const
    {
        return elapsedNs() / 1000000000.0;
    }
};

/**
 * @brief Calculate nanoseconds per operation
 */
inline double nsPerOp(double elapsedNs, std::size_t ops)
{
    return (ops == 0) ? 0.0 : (elapsedNs / static_cast<double>(ops));
}

// ============================================================================
// BenchmarkScope (Windows Priority/Affinity)
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)

/**
 * @brief RAII scope for benchmark environment optimization on Windows
 *
 * @details Sets high priority and pins to a non-zero CPU core to reduce
 * scheduling jitter. Automatically restores original settings on destruction.
 */
class BenchmarkScope
{
    DWORD old_priority_ = 0;
    DWORD_PTR old_affinity_ = 0;
    bool mActive = false;

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
        if (GetProcessAffinityMask(proc, &proc_mask, &sys_mask) && proc_mask)
        {
            DWORD_PTR nonzero = proc_mask & ~static_cast<DWORD_PTR>(1);
            DWORD_PTR pick = nonzero ? nonzero : proc_mask;
            target = pick & (~pick + 1); // Lowest set bit
        }
        old_affinity_ = SetThreadAffinityMask(thread, target);
        mActive = true;

        if (verbose)
        {
            std::cout << "[BenchmarkScope] High priority, CPU" << (target > 1 ? " non-0" : " 0") << " affinity\n";
        }
    }

    ~BenchmarkScope()
    {
        if (mActive)
        {
            HANDLE proc = GetCurrentProcess();
            SetPriorityClass(proc, old_priority_);
            HANDLE thread = GetCurrentThread();
            if (old_affinity_ != 0)
            {
                SetThreadAffinityMask(thread, old_affinity_);
            }
        }
    }

    BenchmarkScope(const BenchmarkScope&) = delete;
    BenchmarkScope& operator=(const BenchmarkScope&) = delete;
};

#else // Non-Windows

/**
 * @brief No-op BenchmarkScope for non-Windows platforms
 */
class BenchmarkScope
{
public:
    explicit BenchmarkScope(bool = false)
    {
    }
};

#endif

// ============================================================================
// SpinBarrier (for concurrent benchmarks)
// ============================================================================

/**
 * @brief Spinning barrier for synchronized thread start
 *
 * @details Ensures all benchmark threads start simultaneously, which is
 * critical for measuring contention and scalability accurately.
 */
class SpinBarrier
{
    std::atomic<unsigned int> mCount;
    std::atomic<unsigned int> mGeneration{0};
    const unsigned int mTotal;

public:
    explicit SpinBarrier(unsigned int count)
        : mCount(count)
        , mTotal(count)
    {
    }

    void wait()
    {
        const unsigned int gen = mGeneration.load(std::memory_order_acquire);
        if (mCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            mCount.store(mTotal, std::memory_order_release);
            mGeneration.fetch_add(1, std::memory_order_release);
        }
        else
        {
            while (mGeneration.load(std::memory_order_acquire) == gen)
            {
                std::this_thread::yield();
            }
        }
    }
};

// ============================================================================
// Environment Variable Helpers
// ============================================================================

namespace detail
{

inline bool hasEnvVar(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
    char buf[2];
    return GetEnvironmentVariableA(name, buf, sizeof(buf)) > 0;
#else
    return std::getenv(name) != nullptr;
#endif
}

inline std::string getEnvVar(const char* name, const char* default_value = "")
{
#if defined(_WIN32) || defined(_WIN64)
    char buf[512];
    DWORD len = GetEnvironmentVariableA(name, buf, sizeof(buf));
    if (len > 0 && len < sizeof(buf))
    {
        return std::string(buf, len);
    }
    return default_value;
#else
    const char* val = std::getenv(name);
    return val ? val : default_value;
#endif
}

inline std::size_t getEnvSizeT(const char* name, std::size_t default_value)
{
    std::string val = getEnvVar(name);
    if (val.empty())
    {
        return default_value;
    }
    try
    {
        return static_cast<std::size_t>(std::stoull(val));
    }
    catch (...)
    {
        return default_value;
    }
}

inline std::uint64_t getEnvUint64(const char* name, std::uint64_t default_value)
{
    std::string val = getEnvVar(name);
    if (val.empty())
    {
        return default_value;
    }
    try
    {
        return static_cast<std::uint64_t>(std::stoull(val));
    }
    catch (...)
    {
        return default_value;
    }
}

} // namespace detail

// ============================================================================
// BenchConfig - Unified Configuration
// ============================================================================

/**
 * @brief Benchmark configuration with FATP_BENCH_* environment variable support
 *
 * @details All Fat-P benchmarks use the same environment variables for
 * consistent configuration across the suite.
 *
 * Environment Variables:
 *   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
 *   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_MIN_BATCH_MS  - Min batch duration (default: 50)
 *   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
 *   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
 *   FATP_BENCH_NO_SCOPE      - Disable priority/affinity
 *   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
 *   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
 */
struct BenchConfig
{
    // Core settings
    std::size_t warmupRuns = kDefaultWarmupRuns;
    std::size_t measuredRuns = kDefaultMeasuredRuns;
    std::uint64_t seed = kDefaultSeed;
    std::size_t minBatchMs = kDefaultMinBatchMs;

    // Output control
    bool verboseStats = false;
    std::string outputCsv;
    std::string outputJson;

    // Feature toggles
    bool noScope = false;
    bool noStabilize = false;
    bool noCooldown = false;

    // Cooldown delays (milliseconds)
    int cooldownSectionMs = kDefaultCooldownSectionMs;
    int cooldownSizeMs = kDefaultCooldownSizeMs;
    int cooldownCaseMs = kDefaultCooldownCaseMs;

    /**
     * @brief Load configuration from FATP_BENCH_* environment variables.
     */
    static BenchConfig fromEnv()
    {
        BenchConfig cfg;

        cfg.warmupRuns = detail::getEnvSizeT("FATP_BENCH_WARMUP_RUNS", kDefaultWarmupRuns);
        cfg.measuredRuns = detail::getEnvSizeT("FATP_BENCH_BATCHES", kDefaultMeasuredRuns);
        cfg.seed = detail::getEnvUint64("FATP_BENCH_SEED", kDefaultSeed);
        cfg.minBatchMs = detail::getEnvSizeT("FATP_BENCH_MIN_BATCH_MS", kDefaultMinBatchMs);

        cfg.verboseStats = detail::hasEnvVar("FATP_BENCH_VERBOSE_STATS");
        cfg.outputCsv = detail::getEnvVar("FATP_BENCH_OUTPUT_CSV");
        cfg.outputJson = detail::getEnvVar("FATP_BENCH_OUTPUT_JSON");

        cfg.noScope = detail::hasEnvVar("FATP_BENCH_NO_SCOPE");
        cfg.noStabilize = detail::hasEnvVar("FATP_BENCH_NO_STABILIZE");
        cfg.noCooldown = detail::hasEnvVar("FATP_BENCH_NO_COOLDOWN");

        return cfg;
    }

    /**
     * @brief Configuration for unit testing - minimal overhead, no waits.
     *
     * @details Returns a config with:
     *   - noStabilize=true (skip 30-second CPU wait)
     *   - noCooldown=true (skip inter-benchmark delays)
     *   - noScope=true (skip priority/affinity changes)
     *   - Minimal warmup/measured runs (3/15)
     *
     * Use with makeTestRunner() for fast unit test execution.
     */
    static BenchConfig forTesting()
    {
        BenchConfig cfg;
        cfg.warmupRuns = 3;
        cfg.measuredRuns = 15;
        cfg.noStabilize = true;
        cfg.noCooldown = true;
        cfg.noScope = true;
        cfg.cooldownSectionMs = 0;
        cfg.cooldownSizeMs = 0;
        cfg.cooldownCaseMs = 0;
        return cfg;
    }

    /**
     * @brief Print resolved configuration
     */
    void print(std::ostream& out = std::cout) const
    {
        out << "  Config: warmup=" << warmupRuns << ", batches=" << measuredRuns << ", seed=" << seed
            << ", minBatchMs=" << minBatchMs << "\n";

        out << "  Options:";
        if (noScope)
        {
            out << " noScope";
        }
        if (noStabilize)
        {
            out << " noStabilize";
        }
        if (noCooldown)
        {
            out << " noCooldown";
        }
        if (verboseStats)
        {
            out << " verbose";
        }
        if (!noScope && !noStabilize && !noCooldown && !verboseStats)
        {
            out << " (defaults)";
        }
        out << "\n";

        if (!outputCsv.empty())
        {
            out << "  CSV output: " << outputCsv << "\n";
        }
        if (!outputJson.empty())
        {
            out << "  JSON output: " << outputJson << "\n";
        }
    }
};

// ============================================================================
// Platform/Compiler Detection Strings
// ============================================================================

/**
 * @brief Get platform identification string
 * @return String like "Windows-x64", "Linux-ARM64", "macOS-x64"
 */
inline std::string getPlatformString()
{
#if defined(_WIN32) || defined(_WIN64)
#if defined(_M_ARM64) || defined(__aarch64__)
    return "Windows-ARM64";
#elif defined(_M_X64) || defined(__x86_64__)
    return "Windows-x64";
#else
    return "Windows-x86";
#endif
#elif defined(__APPLE__)
#if defined(__aarch64__)
    return "macOS-ARM64";
#else
    return "macOS-x64";
#endif
#elif defined(__linux__)
#if defined(__aarch64__)
    return "Linux-ARM64";
#elif defined(__x86_64__)
    return "Linux-x64";
#else
    return "Linux-x86";
#endif
#else
    return "Unknown";
#endif
}

/**
 * @brief Get compiler identification string
 * @return String like "MSVC 1938", "GCC 12.3", "Clang 15.0"
 */
inline std::string getCompilerString()
{
#if defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
    return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
    return "Unknown";
#endif
}

/**
 * @brief Get ISO 8601 timestamp string
 * @return String like "2025-01-15T14:32:01"
 */
inline std::string getTimestampIso()
{
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm_info);
    return buf;
}

// ============================================================================
// Statistics
// ============================================================================

/**
 * @brief Statistical results from benchmark samples
 *
 * @details Computes median, mean, stddev, CI95, and percentiles from
 * a vector of timing samples. Designed for compact, readable output.
 */
struct Statistics
{
    double median = 0;
    double mean = 0;
    double stddev = 0;
    double ci95Low = 0;
    double ci95High = 0;
    double min = 0;
    double max = 0;
    double p95 = 0;
    double p99 = 0;
    std::size_t samples = 0;

    /**
     * @brief Compute statistics from samples
     * @param rawSamples Vector of timing measurements (will be sorted)
     * @return Computed statistics
     */
    static Statistics compute(std::vector<double> rawSamples)
    {
        Statistics s{};
        if (rawSamples.empty())
        {
            return s;
        }

        std::sort(rawSamples.begin(), rawSamples.end());
        const std::size_t n = rawSamples.size();
        s.samples = n;

        s.min = rawSamples.front();
        s.max = rawSamples.back();

        // Median
        if (n % 2 == 1)
        {
            s.median = rawSamples[n / 2];
        }
        else
        {
            s.median = 0.5 * (rawSamples[n / 2 - 1] + rawSamples[n / 2]);
        }

        // Mean
        double sum = std::accumulate(rawSamples.begin(), rawSamples.end(), 0.0);
        s.mean = sum / static_cast<double>(n);

        // Percentiles
        s.p95 = rawSamples[static_cast<std::size_t>(0.95 * static_cast<double>(n - 1))];
        s.p99 = rawSamples[static_cast<std::size_t>(0.99 * static_cast<double>(n - 1))];

        // Standard deviation (sample) and CI95
        if (n > 1)
        {
            double acc = 0.0;
            for (double x : rawSamples)
            {
                double d = x - s.mean;
                acc += d * d;
            }
            s.stddev = std::sqrt(acc / static_cast<double>(n - 1));

            // CI95 using t-distribution approximation
            double t_value;
            if (n >= 120)
            {
                t_value = 1.96;
            }
            else if (n >= 60)
            {
                t_value = 2.00;
            }
            else if (n >= 30)
            {
                t_value = 2.04;
            }
            else if (n >= 15)
            {
                t_value = 2.14;
            }
            else if (n >= 10)
            {
                t_value = 2.26;
            }
            else
            {
                t_value = 2.78;
            }

            double se = s.stddev / std::sqrt(static_cast<double>(n));
            double margin = t_value * se;
            s.ci95Low = s.mean - margin;
            s.ci95High = s.mean + margin;
        }

        return s;
    }

    /**
     * @brief Print compact single-line output (primary format)
     *
     * @details Format: "  label                  :   45.23 ns/op  (+/- 2.31)"
     */
    void printCompact(std::ostream& out, const char* label, const char* unit = "ns/op", int labelWidth = 24) const
    {
        out << std::fixed << std::setprecision(2);
        out << "    " << std::left << std::setw(labelWidth) << label << ": " << std::right << std::setw(8) << mean
            << " " << unit << "  (+/-" << std::setw(6) << stddev << ")";

        // Add CI95 if verbose or samples are few
        if (samples < 30 || stddev / mean > 0.1)
        {
            out << "  CI95=[" << ci95Low << ", " << ci95High << "]";
        }
        out << "\n";
    }

    /**
     * @brief Print compact with median emphasis (comparison format)
     *
     * @details Format: "  label                  :   45.23 ns/op  median=45.00  (+/- 2.31)"
     */
    void printComparison(std::ostream& out, const char* label, const char* unit = "ns/op", int labelWidth = 24) const
    {
        out << std::fixed << std::setprecision(2);
        out << "    " << std::left << std::setw(labelWidth) << label << ": " << std::right << std::setw(8) << median
            << " " << unit << "  (+/-" << std::setw(6) << stddev << ")\n";
    }

    /**
     * @brief Print detailed multi-line output
     */
    void printDetailed(std::ostream& out, const char* label) const
    {
        out << std::fixed << std::setprecision(2);
        out << "    " << label << ":\n";
        out << "      Median:  " << std::setw(10) << median << " ns"
            << "    Mean:    " << std::setw(10) << mean << " ns\n";
        out << "      StdDev:  " << std::setw(10) << stddev << " ns"
            << "    CI95:    [" << ci95Low << ", " << ci95High << "]\n";
        out << "      Min:     " << std::setw(10) << min << " ns"
            << "    Max:     " << std::setw(10) << max << " ns\n";
        out << "      P95:     " << std::setw(10) << p95 << " ns"
            << "    P99:     " << std::setw(10) << p99 << " ns\n";
        out << "      Samples: " << samples << "\n";
    }
};

// ============================================================================
// Result Storage
// ============================================================================

/**
 * @brief Single benchmark result
 */
struct BenchResult
{
    std::string name;
    std::string library; // Empty for single-library benchmarks
    std::string unit = "ns/op";
    Statistics stats;
    CpuFreqInfo cpuContext;
};

/**
 * @brief Results from a comparison benchmark (multiple libraries)
 */
struct ComparisonResult
{
    std::string caseName;
    std::vector<BenchResult> libraries;
    CpuFreqInfo cpuContext;
};

// ============================================================================
// Adapter Interface (for multi-library comparison)
// ============================================================================

/**
 * @brief Interface for library adapters in multi-library comparison
 *
 * @details Implement this interface to benchmark a library. The runner
 * calls methods in this order:
 *   1. setup(N) - allocate/prepare (outside timing)
 *   2. [optional] preload() - load test data (outside timing)
 *   3. run() - timed operation
 *   4. teardown() - cleanup (outside timing)
 */
struct IAdapter
{
    virtual ~IAdapter() = default;

    /// Library name for display
    virtual const char* name() const = 0;

    /// Setup before benchmark (outside timing)
    virtual void setup(std::size_t N) = 0;

    /// Cleanup after benchmark (outside timing)
    virtual void teardown() = 0;

    /// Clear state for next iteration (outside timing)
    virtual void clear()
    {
    }
};

// ============================================================================
// Output Formatting Helpers
// ============================================================================

/**
 * @brief Print section header with ===
 */
inline void printSectionHeader(std::ostream& out, const std::string& title)
{
    out << "\n";
    out << "================================================================================\n";
    out << "  " << title << "\n";
    out << "================================================================================\n\n";
}

/**
 * @brief Print contract note explaining semantic guarantees
 */
inline void printContract(std::ostream& out, const std::string& note)
{
    out << "  Contract: " << note << "\n\n";
}

/**
 * @brief Print subheader for size/case variations
 */
inline void printSubheader(std::ostream& out, const std::string& title)
{
    out << "\n  --- " << title << " ---\n\n";
}

/**
 * @brief Format time in appropriate units
 */
inline std::string formatTime(double ns)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (ns < 1000.0)
    {
        oss << ns << " ns";
    }
    else if (ns < 1000000.0)
    {
        oss << (ns / 1000.0) << " us";
    }
    else if (ns < 1000000000.0)
    {
        oss << (ns / 1000000.0) << " ms";
    }
    else
    {
        oss << (ns / 1000000000.0) << " s";
    }

    return oss.str();
}

// ============================================================================
// Cooldown Helper
// ============================================================================

/**
 * @brief Cooldown delay with optional verbose output
 */
inline void cooldownDelay(int ms, const char* reason = nullptr, bool verbose = false)
{
    if (ms <= 0)
    {
        return;
    }

    if (verbose && reason)
    {
        std::cout << "[Cooling " << ms << "ms: " << reason << "]\n";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ============================================================================
// BenchmarkRunner
// ============================================================================

/**
 * @brief Unified benchmark runner with round-robin multi-library support
 *
 * @details Main orchestrator for Fat-P benchmarks. Supports both single-library
 * benchmarks and multi-library comparisons with round-robin execution order.
 *
 * Features:
 *   - Automatic round-robin randomization for fair comparison
 *   - FATP_BENCH_* environment variable configuration
 *   - CPU frequency monitoring per section
 *   - Style-guide compliant CSV/JSON export
 *   - Readable, compact output formatting
 */
class BenchmarkRunner
{
public:
    using BenchFunc = std::function<void()>;
    using BenchFuncWithOps = std::function<std::size_t()>;

    explicit BenchmarkRunner(std::string name, BenchConfig config = BenchConfig::fromEnv())
        : mName(std::move(name))
        , mConfig(std::move(config))
        , mRng(static_cast<std::mt19937::result_type>(mConfig.seed))
    {
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    const std::string& name() const
    {
        return mName;
    }

    BenchConfig& config()
    {
        return mConfig;
    }
    const BenchConfig& config() const
    {
        return mConfig;
    }

    const std::vector<BenchResult>& results() const
    {
        return mResults;
    }
    const std::vector<ComparisonResult>& comparisonResults() const
    {
        return comparison_mResults;
    }

    // ========================================================================
    // Section Management
    // ========================================================================

    /**
     * @brief Start a new section with header
     */
    BenchmarkRunner& section(const std::string& title)
    {
        mCurrentSection = title;

        if (!mConfig.noCooldown && !mResults.empty())
        {
            cooldownDelay(mConfig.cooldownSectionMs, "section transition", mConfig.verboseStats);
        }

        printSectionHeader(std::cout, title);
        print_cpu_context(std::cout, "Section start");

        return *this;
    }

    /**
     * @brief Add contract note to current section
     */
    BenchmarkRunner& contract(const std::string& note)
    {
        mCurrentContract = note;
        printContract(std::cout, note);
        return *this;
    }

    // ========================================================================
    // Single-Library Benchmarks
    // ========================================================================

    /**
     * @brief Add a simple benchmark (no setup/teardown)
     */
    void add(const std::string& name, BenchFunc func)
    {
        mSingleBenchmarks.push_back({name, std::move(func), nullptr});
    }

    /**
     * @brief Add a benchmark that returns operation count
     */
    void addWithOps(const std::string& name, BenchFuncWithOps func)
    {
        mSingleBenchmarks.push_back({name, nullptr, std::move(func)});
    }

    // ========================================================================
    // Multi-Library Comparison
    // ========================================================================

    /**
     * @brief Register an adapter for multi-library comparison
     */
    template <typename AdapterT>
    void addLibrary()
    {
        mAdapters.push_back(std::make_unique<AdapterT>());
    }

    /**
     * @brief Add a pre-constructed adapter
     */
    void addLibrary(std::unique_ptr<IAdapter> adapter)
    {
        mAdapters.push_back(std::move(adapter));
    }

    /**
     * @brief Run comparison benchmark with round-robin execution
     *
     * @param caseName Name of the benchmark case
     * @param run_func Function that runs the benchmark on an adapter
     * @param N Size parameter for setup
     * @param needs_preload Whether to call preload before timing
     */
    template <typename RunFunc>
    void compare(const std::string& caseName, RunFunc run_func, std::size_t N, bool needs_preload = false)
    {
        (void)needs_preload; // Reserved for future use

        if (mAdapters.empty())
        {
            std::cerr << "Warning: No adapters registered for comparison\n";
            return;
        }

        if (!mConfig.noCooldown)
        {
            cooldownDelay(mConfig.cooldownCaseMs, nullptr, mConfig.verboseStats);
        }

        // Initialize results for each adapter
        std::vector<std::vector<double>> all_samples(mAdapters.size());

        // Warmup phase
        for (std::size_t run = 0; run < mConfig.warmupRuns; ++run)
        {
            for (auto& adapter : mAdapters)
            {
                adapter->setup(N);
                run_func(adapter.get());
                adapter->teardown();
            }
        }

        // Measured runs with round-robin randomization
        std::vector<std::size_t> order(mAdapters.size());
        std::iota(order.begin(), order.end(), 0);

        for (std::size_t run = 0; run < mConfig.measuredRuns; ++run)
        {
            std::shuffle(order.begin(), order.end(), mRng);

            for (std::size_t idx : order)
            {
                mAdapters[idx]->setup(N);

                Timer timer;
                timer.start();
                std::size_t ops = run_func(mAdapters[idx].get());
                double elapsed = timer.elapsedNs();

                mAdapters[idx]->teardown();

                all_samples[idx].push_back(nsPerOp(elapsed, ops));
            }
        }

        // Compute statistics and store results
        ComparisonResult result;
        result.caseName = caseName;
        result.cpuContext = capture_cpu_frequency();

        for (std::size_t i = 0; i < mAdapters.size(); ++i)
        {
            BenchResult br;
            br.name = caseName;
            br.library = mAdapters[i]->name();
            br.stats = Statistics::compute(std::move(all_samples[i]));
            br.cpuContext = result.cpuContext;
            result.libraries.push_back(std::move(br));
        }

        comparison_mResults.push_back(std::move(result));

        // Print immediately
        printComparison_result(comparison_mResults.back());
    }

    // ========================================================================
    // Execution
    // ========================================================================

    /**
     * @brief Run all registered single-library benchmarks
     */
    void run()
    {
        for (auto& bench : mSingleBenchmarks)
        {
            runSingleBenchmark(bench);
        }
    }

    // ========================================================================
    // Output
    // ========================================================================

    /**
     * @brief Print the initial benchmark header with platform info
     */
    void printHeader() const
    {
        std::cout << "================================================================================\n";
        std::cout << "  " << mName << " Benchmark\n";
        std::cout << "================================================================================\n\n";

        std::cout << "  Platform: " << getPlatformString() << ", " << getCompilerString() << "\n";
        mConfig.print(std::cout);

        // CPU detection info
        print_cpu_detection_info(std::cout);
        std::cout << "\n";
    }

    /**
     * @brief Print registered libraries (for multi-library comparison)
     */
    void printLibraries() const
    {
        if (mAdapters.empty())
        {
            return;
        }

        std::cout << "  Libraries:\n";
        for (const auto& adapter : mAdapters)
        {
            std::cout << "    [x] " << adapter->name() << "\n";
        }
        std::cout << "\n";
    }

    /**
     * @brief Print design invariants
     */
    void printInvariants() const
    {
        std::cout << "  Design Invariants:\n";
        std::cout << "    1. Each measured run executes exactly one timed iteration per library\n";
        std::cout << "    2. Library execution order is randomized per run\n";
        std::cout << "    3. Setup/teardown outside timed regions\n";
        std::cout << "    4. All libraries observe same distribution of machine states\n";
        std::cout << "    5. Medians are the primary reported statistic\n\n";
    }

    /**
     * @brief Print summary report
     */
    void printReport() const
    {
        std::cout << "\n";
        std::cout << "================================================================================\n";
        std::cout << "  Benchmark Complete\n";
        std::cout << "================================================================================\n";
    }

    // ========================================================================
    // Export
    // ========================================================================

    /**
     * @brief Export to CSV if FATP_BENCH_OUTPUT_CSV is set
     */
    void exportCsvIfConfigured() const
    {
        if (mConfig.outputCsv.empty())
        {
            return;
        }
        exportCsv(mConfig.outputCsv);
    }

    /**
     * @brief Export to JSON if FATP_BENCH_OUTPUT_JSON is set
     */
    void exportJsonIfConfigured() const
    {
        if (mConfig.outputJson.empty())
        {
            return;
        }
        exportJson(mConfig.outputJson);
    }

    /**
     * @brief Export to both CSV and JSON if configured
     */
    void exportIfConfigured() const
    {
        exportCsvIfConfigured();
        exportJsonIfConfigured();
    }

    /**
     * @brief Export results to CSV (style-guide compliant)
     */
    void exportCsv(const std::string& path) const
    {
        std::ofstream file(path);
        if (!file)
        {
            std::cerr << "Error: Cannot open CSV file: " << path << "\n";
            return;
        }

        // Header
        file << "Timestamp,Benchmark,Case,Library,Unit,Median,Mean,StdDev,"
             << "CI95_Low,CI95_High,Min,Max,P95,P99,Samples,"
             << "Platform,Compiler,CPU_MHz,CPU_Ref_MHz,Ref_Is_Max,"
             << "Seed,Warmup,Batches,Min_Batch_MS\n";

        std::string timestamp = getTimestampIso();
        std::string platform = getPlatformString();
        std::string compiler = getCompilerString();

        // Single benchmarks
        for (const auto& result : mResults)
        {
            writeCsvRow(file, timestamp, platform, compiler, result);
        }

        // Comparison results
        for (const auto& comp : comparison_mResults)
        {
            for (const auto& result : comp.libraries)
            {
                writeCsvRow(file, timestamp, platform, compiler, result);
            }
        }

        std::cout << "  CSV exported: " << path << "\n";
    }

    /**
     * @brief Export results to JSON (style-guide compliant)
     */
    void exportJson(const std::string& path) const
    {
        std::ofstream file(path);
        if (!file)
        {
            std::cerr << "Error: Cannot open JSON file: " << path << "\n";
            return;
        }

        file << std::fixed << std::setprecision(2);
        file << "{\n";
        file << "  \"schema_version\": \"1.0\",\n";
        file << "  \"timestamp\": \"" << getTimestampIso() << "\",\n";
        file << "  \"benchmark\": \"" << mName << "\",\n";
        file << "  \"platform\": \"" << getPlatformString() << "\",\n";
        file << "  \"compiler\": \"" << getCompilerString() << "\",\n";

        // CPU context (from most recent result)
        CpuFreqInfo cpu;
        if (!mResults.empty())
        {
            cpu = mResults.back().cpuContext;
        }
        else if (!comparison_mResults.empty())
        {
            cpu = comparison_mResults.back().cpuContext;
        }
        else
        {
            cpu = capture_cpu_frequency();
        }

        file << "  \"cpuContext\": {\n";
        file << "    \"current_mhz\": " << static_cast<int>(cpu.mCurrentFreqMHz) << ",\n";
        file << "    \"ref_mhz\": " << static_cast<int>(cpu.mRefFreqMHz) << ",\n";
        file << "    \"ref_is_max\": " << (cpu.mRefIsMax ? "true" : "false") << "\n";
        file << "  },\n";

        // Config
        file << "  \"config\": {\n";
        file << "    \"warmupRuns\": " << mConfig.warmupRuns << ",\n";
        file << "    \"measuredRuns\": " << mConfig.measuredRuns << ",\n";
        file << "    \"seed\": " << mConfig.seed << ",\n";
        file << "    \"minBatchMs\": " << mConfig.minBatchMs << "\n";
        file << "  },\n";

        // Results array
        file << "  \"results\": [\n";

        bool first = true;

        // Single benchmarks
        for (const auto& result : mResults)
        {
            if (!first)
            {
                file << ",\n";
            }
            first = false;
            writeJsonResult(file, result);
        }

        // Comparison results
        for (const auto& comp : comparison_mResults)
        {
            for (const auto& result : comp.libraries)
            {
                if (!first)
                {
                    file << ",\n";
                }
                first = false;
                writeJsonResult(file, result);
            }
        }

        file << "\n  ]\n";
        file << "}\n";

        std::cout << "  JSON exported: " << path << "\n";
    }

private:
    // ========================================================================
    // Internal Types
    // ========================================================================

    struct SingleBenchmark
    {
        std::string name;
        BenchFunc func;
        BenchFuncWithOps funcWithOps;
    };

    // ========================================================================
    // Internal Methods
    // ========================================================================

    void runSingleBenchmark(SingleBenchmark& bench)
    {
        if (!mConfig.noCooldown)
        {
            cooldownDelay(mConfig.cooldownCaseMs, nullptr, mConfig.verboseStats);
        }

        std::vector<double> samples;
        samples.reserve(mConfig.measuredRuns);

        // Warmup
        for (std::size_t i = 0; i < mConfig.warmupRuns; ++i)
        {
            if (bench.func)
            {
                bench.func();
            }
            else
            {
                bench.funcWithOps();
            }
        }

        // Measured runs
        for (std::size_t i = 0; i < mConfig.measuredRuns; ++i)
        {
            Timer timer;
            timer.start();

            std::size_t ops = 1;
            if (bench.func)
            {
                bench.func();
            }
            else
            {
                ops = bench.funcWithOps();
            }

            double elapsed = timer.elapsedNs();
            samples.push_back(nsPerOp(elapsed, ops));
        }

        // Store result
        BenchResult result;
        result.name = bench.name;
        result.stats = Statistics::compute(std::move(samples));
        result.cpuContext = capture_cpu_frequency();

        mResults.push_back(result);

        // Print immediately
        if (mConfig.verboseStats)
        {
            result.stats.printDetailed(std::cout, result.name.c_str());
        }
        else
        {
            result.stats.printCompact(std::cout, result.name.c_str());
        }
    }

    void printComparison_result(const ComparisonResult& result) const
    {
        std::cout << "  " << result.caseName << ":\n";

        for (const auto& lib : result.libraries)
        {
            if (mConfig.verboseStats)
            {
                lib.stats.printDetailed(std::cout, lib.library.c_str());
            }
            else
            {
                lib.stats.printComparison(std::cout, lib.library.c_str());
            }
        }

        std::cout << "\n";
    }

    void writeCsvRow(std::ofstream& file,
                     const std::string& timestamp,
                     const std::string& platform,
                     const std::string& compiler,
                     const BenchResult& result) const
    {
        file << timestamp << "," << mName << "," << result.name << "," << result.library << "," << result.unit << ","
             << result.stats.median << "," << result.stats.mean << "," << result.stats.stddev << ","
             << result.stats.ci95Low << "," << result.stats.ci95High << "," << result.stats.min << ","
             << result.stats.max << "," << result.stats.p95 << "," << result.stats.p99 << "," << result.stats.samples
             << "," << platform << "," << compiler << "," << static_cast<int>(result.cpuContext.mCurrentFreqMHz) << ","
             << static_cast<int>(result.cpuContext.mRefFreqMHz) << ","
             << (result.cpuContext.mRefIsMax ? "true" : "false") << "," << mConfig.seed << "," << mConfig.warmupRuns
             << "," << mConfig.measuredRuns << "," << mConfig.minBatchMs << "\n";
    }

    void writeJsonResult(std::ofstream& file, const BenchResult& result) const
    {
        file << "    {\n";
        file << "      \"case\": \"" << result.name << "\",\n";
        if (!result.library.empty())
        {
            file << "      \"library\": \"" << result.library << "\",\n";
        }
        file << "      \"unit\": \"" << result.unit << "\",\n";
        file << "      \"median\": " << result.stats.median << ",\n";
        file << "      \"mean\": " << result.stats.mean << ",\n";
        file << "      \"stddev\": " << result.stats.stddev << ",\n";
        file << "      \"ci95Low\": " << result.stats.ci95Low << ",\n";
        file << "      \"ci95High\": " << result.stats.ci95High << ",\n";
        file << "      \"min\": " << result.stats.min << ",\n";
        file << "      \"max\": " << result.stats.max << ",\n";
        file << "      \"p95\": " << result.stats.p95 << ",\n";
        file << "      \"p99\": " << result.stats.p99 << ",\n";
        file << "      \"samples\": " << result.stats.samples << "\n";
        file << "    }";
    }

    // ========================================================================
    // Data Members
    // ========================================================================

    std::string mName;
    BenchConfig mConfig;
    std::mt19937 mRng;

    std::string mCurrentSection;
    std::string mCurrentContract;

    std::vector<SingleBenchmark> mSingleBenchmarks;
    std::vector<std::unique_ptr<IAdapter>> mAdapters;

    std::vector<BenchResult> mResults;
    std::vector<ComparisonResult> comparison_mResults;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Wait for CPU to stabilize before benchmarking
 */
inline bool waitForStableCpu(const BenchConfig& config, std::ostream& out = std::cout)
{
    if (config.noStabilize)
    {
        out << "  [CPU stabilization disabled]\n";
        return true;
    }

    out << "  Waiting for CPU to stabilize...\n";

    CpuWaitConfig wait_config;
    wait_config.mMaxWaitSeconds = 30;
    wait_config.mPollIntervalMs = 200;

    auto result = wait_for_cpu_stable(&out, wait_config, wait_config.mFixedSectionDelayMs, "initial");

    return result.mStabilized;
}

/**
 * @brief Create and configure a benchmark runner with standard setup
 */
inline BenchmarkRunner makeRunner(const std::string& name)
{
    BenchConfig config = BenchConfig::fromEnv();
    BenchmarkRunner runner(name, config);

    runner.printHeader();

    // Set up BenchmarkScope if not disabled
    static std::unique_ptr<BenchmarkScope> g_scope;
    if (!config.noScope)
    {
        g_scope = std::make_unique<BenchmarkScope>(config.verboseStats);
    }

    // Wait for CPU stability
    waitForStableCpu(config);

    return runner;
}

/**
 * @brief Create a lightweight benchmark runner for unit testing.
 *
 * @details Unlike makeRunner(), this function:
 *   - Skips header printing (no CPU diagnostics spam)
 *   - Skips CPU stabilization wait (no 30-second delays)
 *   - Skips BenchmarkScope (no priority/affinity changes)
 *   - Uses minimal warmup/measured runs
 *
 * Use this in unit tests that need to verify BenchmarkRunner behavior
 * without the overhead of production benchmarking setup.
 *
 * @param name Runner name for identification
 * @param quiet If true, suppresses all output during run() (default: false)
 * @return Configured BenchmarkRunner ready for testing
 *
 * @code
 * FATP_TEST_CASE(runner_basic)
 * {
 *     auto runner = makeTestRunner("TestRunner");
 *     runner.add("test_bench", []() { volatile int x = 1; });
 *     runner.run();
 *     FATP_ASSERT_GT(runner.results().size(), 0);
 *     return true;
 * }
 * @endcode
 */
inline BenchmarkRunner makeTestRunner(const std::string& name, bool quiet = false)
{
    BenchConfig config = BenchConfig::forTesting();
    BenchmarkRunner runner(name, config);

    // Skip header, scope, and CPU wait entirely for tests
    // If quiet mode requested, redirect output during run (caller's responsibility)
    (void)quiet; // Reserved for future use

    return runner;
}

} // namespace bench
} // namespace fat_p

// ============================================================================
// Cleanup - restore macros we may have defined
// ============================================================================
#ifdef FATP_DEFINED_NOMINMAX_BENCH
#undef NOMINMAX
#undef FATP_DEFINED_NOMINMAX_BENCH
#endif
#ifdef FATP_DEFINED_WIN32_LEAN_AND_MEAN_BENCH
#undef WIN32_LEAN_AND_MEAN
#undef FATP_DEFINED_WIN32_LEAN_AND_MEAN_BENCH
#endif
#ifdef FATP_DEFINED_CRT_SECURE_NO_WARNINGS_BENCH
#undef _CRT_SECURE_NO_WARNINGS
#undef FATP_DEFINED_CRT_SECURE_NO_WARNINGS_BENCH
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
