/**
 * @file FatPBenchmarkUtils.h
 * @brief Benchmark-only utilities shared across Fat-P benchmark programs.
 *
 * @details
 * Provides platform-specific CPU frequency monitoring, including throttling and
 * turbo detection when available.
 *
 * This header is intended for use by benchmark .cpp files. It is not part of
 * the public Fat-P component API surface.
 *
 * @layer Application
 */
#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winreg.h>
#include <intrin.h>  // For __cpuid

#ifndef FATP_ENABLE_PDH_STATS
#define FATP_ENABLE_PDH_STATS
#endif
#else
#include <fstream>
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

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0,
                      KEY_READ,
                      &hKey) == ERROR_SUCCESS)
    {
        RegQueryValueExA(hKey,
                         "~MHz",
                         nullptr,
                         nullptr,
                         reinterpret_cast<LPBYTE>(&freq),
                         &size);
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
    uint16_t mBaseMHz = 0;    ///< Processor Base Frequency (sustainable)
    uint16_t mMaxMHz = 0;     ///< Maximum Frequency (turbo)
    uint16_t mBusMHz = 0;     ///< Bus (Reference) Frequency
    bool mSupported = false;  ///< True if CPUID 0x16 is available and valid
    
    // Diagnostic fields
    int mMaxLeaf = 0;         ///< Maximum supported CPUID leaf
    uint32_t mRawEAX = 0;     ///< Raw EAX from leaf 0x16
    uint32_t mRawEBX = 0;     ///< Raw EBX from leaf 0x16
    uint32_t mRawECX = 0;     ///< Raw ECX from leaf 0x16
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
        return result;  // Not supported (AMD or older Intel)
    }
    
    // Query leaf 0x16: Processor Frequency Information
    __cpuid(cpuInfo, 0x16);
    
    // Store raw values for diagnostics
    result.mRawEAX = static_cast<uint32_t>(cpuInfo[0]);
    result.mRawEBX = static_cast<uint32_t>(cpuInfo[1]);
    result.mRawECX = static_cast<uint32_t>(cpuInfo[2]);
    
    result.mBaseMHz = static_cast<uint16_t>(cpuInfo[0] & 0xFFFF);  // EAX[15:0]
    result.mMaxMHz  = static_cast<uint16_t>(cpuInfo[1] & 0xFFFF);  // EBX[15:0]
    result.mBusMHz  = static_cast<uint16_t>(cpuInfo[2] & 0xFFFF);  // ECX[15:0]
    
    // Validate - base should be non-zero and less than or equal to max
    result.mSupported = (result.mBaseMHz > 0) && 
                        (result.mMaxMHz == 0 || result.mBaseMHz <= result.mMaxMHz);
    
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
    result.mMaxMHz = regMHz;  // We don't know true max without CPUID
    result.mIsAccurate = false;
    result.mIsEstimated = false;  // Using registry directly, not estimating
    
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
    using PdhGetFormattedCounterValue_t =
        PDH_STATUS(WINAPI*)(PDH_HCOUNTER, DWORD, LPDWORD, PDH_FMT_COUNTERVALUE*);
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
        mCollect =
            reinterpret_cast<PdhCollectQueryData_t>(GetProcAddress(mPdh, "PdhCollectQueryData"));
        mGetValue = reinterpret_cast<PdhGetFormattedCounterValue_t>(
            GetProcAddress(mPdh, "PdhGetFormattedCounterValue"));
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
    info.mRefIsMax = false;  // We're using base (actual or estimated), not max
    info.mRefIsEstimated = baseInfo.mIsEstimated;  // Track if it's an estimate
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
    }

    if (detail::read_int64_from_file("/sys/devices/system/cpu/cpu0/cpufreq/base_frequency", khz))
    {
        info.mRefFreqMHz = static_cast<double>(khz) / 1000.0;
        info.mRefIsMax = false;
    }
    else if (detail::read_int64_from_file(
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
        khz))
    {
        info.mRefFreqMHz = static_cast<double>(khz) / 1000.0;
        info.mRefIsMax = true;
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
            out << "    Raw EAX:         0x" << std::hex << cpuid.mRawEAX << std::dec 
                << " (base: " << cpuid.mBaseMHz << " MHz)\n";
            out << "    Raw EBX:         0x" << std::hex << cpuid.mRawEBX << std::dec 
                << " (max: " << cpuid.mMaxMHz << " MHz)\n";
            out << "    Raw ECX:         0x" << std::hex << cpuid.mRawECX << std::dec 
                << " (bus: " << cpuid.mBusMHz << " MHz)\n";
            // Explain why validation failed
            if (cpuid.mBaseMHz == 0)
            {
                out << "    Reason:          Base frequency is 0\n";
            }
            else if (cpuid.mMaxMHz > 0 && cpuid.mBaseMHz > cpuid.mMaxMHz)
            {
                out << "    Reason:          Base (" << cpuid.mBaseMHz 
                    << ") > Max (" << cpuid.mMaxMHz << ")\n";
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

#else  // Linux
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
    out << "    Reference:      " << static_cast<int>(info.mRefFreqMHz) << " MHz (" 
        << info.ref_label() << ")\n";
    out << "    Current:        " << static_cast<int>(info.mCurrentFreqMHz) << " MHz"
        << (info.mCurrentIsEstimated ? " (estimated)" : " (measured)") << "\n";
    out << "    Throttle %:     " << std::fixed << std::setprecision(1) 
        << info.throttle_percentage() << "%\n";
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
    double mThrottleThreshold = 0.0;  // 0 = auto
    
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
    bool mStabilized = false;      ///< True if CPU stabilized within timeout
    bool mUsedFixedDelay = false;  ///< True if fixed delay was used instead of waiting
    int mWaitedMs = 0;             ///< Actual time waited
    CpuFreqInfo mFinalState;       ///< CPU state after wait
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
inline CpuWaitResult wait_for_cpu_stable(
    std::ostream* out,
    const CpuWaitConfig& config,
    int delayMs,
    const char* label = nullptr)
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
        result.mWaitedMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        
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
                *out << "[Ready: " << static_cast<int>(result.mFinalState.mCurrentFreqMHz)
                     << "/" << static_cast<int>(result.mFinalState.mRefFreqMHz) << " MHz";
                const double pct = result.mFinalState.throttle_percentage();
                if (pct > 5.0)  // Only show if noticeable
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
                     << (result.mFinalState.mRefIsMax ? "max" : "base")
                     << " after " << config.mMaxWaitSeconds << "s - "
                     << static_cast<int>(result.mFinalState.mCurrentFreqMHz)
                     << "/" << static_cast<int>(result.mFinalState.mRefFreqMHz) << " MHz]\n";
            }
            return result;
        }
        
        // Print progress every 5 seconds
        if (out && (result.mWaitedMs % 5000) < config.mPollIntervalMs)
        {
            *out << "[Waiting: " << static_cast<int>(result.mFinalState.mCurrentFreqMHz)
                 << "/" << static_cast<int>(result.mFinalState.mRefFreqMHz)
                 << " MHz (" << std::fixed << std::setprecision(0)
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
        if (std::string(argv[i]) == "--fixed-delays" ||
            std::string(argv[i]) == "-f" ||
            std::string(argv[i]) == "--fixed")
        {
            return true;
        }
    }
    return false;
}

} // namespace bench
} // namespace fat_p
