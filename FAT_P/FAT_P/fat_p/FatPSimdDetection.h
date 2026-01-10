/**
 * @file FatPSimdDetection.h
 * @brief Centralized SIMD capability detection for Fat-P components.
 * @version 1.0
 *
 * Provides both compile-time and runtime SIMD detection:
 *
 * Compile-time:
 *   - FATP_SIMD_AVX512F, FATP_SIMD_AVX2, FATP_SIMD_SSE2 (x86/x64)
 *   - FATP_SIMD_NEON, FATP_SIMD_NEON_AARCH64 (ARM)
 *   - FATP_HAS_SIMD (any SIMD available)
 *
 * Runtime (x86 only):
 *   - cpu_has_sse2(), cpu_has_avx2(), cpu_has_avx512f()
 *   - Results are cached on first call (thread-safe via static local)
 *
 * Diagnostics:
 *   - compiled_backend() - What we compiled with
 *   - cpu_capability() - What the CPU supports (runtime)
 *   - is_optimal() - Are we using the best available?
 *   - optimization_hint() - How to improve (or nullptr if optimal)
 *
 * Usage:
 *   #include "FatPSimdDetection.h"
 *
 *   // Check at startup
 *   if (!fat_p::simd::is_optimal()) {
 *       std::cerr << "Warning: " << fat_p::simd::optimization_hint() << "\n";
 *   }
 *
 *   // In benchmarks
 *   std::cout << "Compiled: " << fat_p::simd::compiled_backend() << "\n";
 *   std::cout << "CPU has:  " << fat_p::simd::cpu_capability() << "\n";
 *
 * MSVC Note:
 *   MSVC requires explicit /arch:AVX2 or /arch:AVX512 flags.
 *   Unlike GCC/Clang's -march=native, MSVC does not auto-detect.
 *
 * @layer Foundation
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPSimdDetection
  file_role: public_header
  path: fat_p/FatPSimdDetection.h
  namespace: fat_p
  summary: "Public header for FatPSimdDetection."
  api_stability: in_work
  related:
    docs_search: "FatPSimdDetection"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 20
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <cstddef>
#include <cstdint>

// =============================================================================
// Compile-Time SIMD Detection
// =============================================================================

// --- x86/x64: AVX-512F (Foundation) + AVX-512BW (Byte/Word operations) ---
// Note: FastHashMap needs AVX-512BW for byte-level comparisons
#if defined(__AVX512F__) && defined(__AVX512BW__)
    #define FATP_SIMD_AVX512F 1
    #define FATP_SIMD_AVX512BW 1
    #define FATP_SIMD_AVX2 1
    #define FATP_SIMD_SSE2 1

// --- x86/x64: AVX-512F only (no BW, can't use for byte ops) ---
#elif defined(__AVX512F__)
    #define FATP_SIMD_AVX512F 1
    // Fall back to AVX2 for byte operations
    #define FATP_SIMD_AVX2 1
    #define FATP_SIMD_SSE2 1

// --- x86/x64: AVX2 ---
#elif defined(__AVX2__)
    #define FATP_SIMD_AVX2 1
    #define FATP_SIMD_SSE2 1

// --- x86/x64: SSE2 ---
#elif defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
    #define FATP_SIMD_SSE2 1
#endif

// --- ARM: NEON ---
#if defined(__aarch64__)
    #define FATP_SIMD_NEON 1
    #define FATP_SIMD_NEON_AARCH64 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define FATP_SIMD_NEON 1
    #define FATP_SIMD_NEON_AARCH64 0
#endif

// --- Unified availability ---
#if defined(FATP_SIMD_AVX512F) || defined(FATP_SIMD_AVX2) || defined(FATP_SIMD_SSE2) || defined(FATP_SIMD_NEON)
    #define FATP_HAS_SIMD 1
#else
    #define FATP_HAS_SIMD 0
#endif

// --- x86/x64 platform detection (for CPUID) ---
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    #define FATP_ARCH_X86 1
    #if defined(_MSC_VER)
        #include <intrin.h>
    #elif defined(__GNUC__) || defined(__clang__)
        #include <cpuid.h>
    #endif
#else
    #define FATP_ARCH_X86 0
#endif

// --- ARM platform detection ---
#if defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
    #define FATP_ARCH_ARM 1
#else
    #define FATP_ARCH_ARM 0
#endif

namespace fat_p {
namespace simd {

// =============================================================================
// Runtime CPUID Detection (x86/x64 only)
// =============================================================================

namespace detail {

#if FATP_ARCH_X86

struct CpuFeatures {
    bool sse2 = false;
    bool avx = false;
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;  // Byte/Word operations (needed for hash map)
    bool osxsave = false;   // OS supports AVX state saving
};

inline CpuFeatures detect_cpu_features() noexcept {
    CpuFeatures f;
    
#if defined(_MSC_VER)
    int cpuInfo[4] = {0};
    
    // Get max supported CPUID function
    __cpuid(cpuInfo, 0);
    const int maxFunc = cpuInfo[0];
    
    if (maxFunc >= 1) {
        __cpuid(cpuInfo, 1);
        f.sse2 = (cpuInfo[3] & (1 << 26)) != 0;      // EDX bit 26
        f.osxsave = (cpuInfo[2] & (1 << 27)) != 0;   // ECX bit 27
        f.avx = (cpuInfo[2] & (1 << 28)) != 0;       // ECX bit 28
    }
    
    if (maxFunc >= 7 && f.osxsave) {
        __cpuidex(cpuInfo, 7, 0);
        f.avx2 = (cpuInfo[1] & (1 << 5)) != 0;       // EBX bit 5
        f.avx512f = (cpuInfo[1] & (1 << 16)) != 0;   // EBX bit 16
        f.avx512bw = (cpuInfo[1] & (1 << 30)) != 0;  // EBX bit 30
    }
    
#elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    
    // Get max supported CPUID function
    if (!__get_cpuid(0, &eax, &ebx, &ecx, &edx)) {
        return f;
    }
    const unsigned int maxFunc = eax;
    
    if (maxFunc >= 1) {
        __get_cpuid(1, &eax, &ebx, &ecx, &edx);
        f.sse2 = (edx & (1 << 26)) != 0;
        f.osxsave = (ecx & (1 << 27)) != 0;
        f.avx = (ecx & (1 << 28)) != 0;
    }
    
    if (maxFunc >= 7 && f.osxsave) {
        __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
        f.avx2 = (ebx & (1 << 5)) != 0;
        f.avx512f = (ebx & (1 << 16)) != 0;
        f.avx512bw = (ebx & (1 << 30)) != 0;
    }
#endif
    
    return f;
}

inline const CpuFeatures& get_cpu_features() noexcept {
    static const CpuFeatures features = detect_cpu_features();
    return features;
}

#endif // FATP_ARCH_X86

} // namespace detail

// =============================================================================
// Public Runtime Detection API
// =============================================================================

/**
 * @brief Check if CPU supports SSE2 (runtime detection).
 * @return true if SSE2 is supported, false otherwise.
 * @note On non-x86, returns true if compiled with SIMD support.
 */
inline bool cpu_has_sse2() noexcept {
#if FATP_ARCH_X86
    return detail::get_cpu_features().sse2;
#elif defined(FATP_SIMD_NEON)
    return false;  // Not applicable to ARM
#else
    return false;
#endif
}

/**
 * @brief Check if CPU supports AVX2 (runtime detection).
 * @return true if AVX2 is supported and OS supports AVX state.
 */
inline bool cpu_has_avx2() noexcept {
#if FATP_ARCH_X86
    const auto& f = detail::get_cpu_features();
    return f.avx2 && f.osxsave;
#else
    return false;
#endif
}

/**
 * @brief Check if CPU supports AVX-512F (runtime detection).
 * @return true if AVX-512F is supported and OS supports AVX-512 state.
 */
inline bool cpu_has_avx512f() noexcept {
#if FATP_ARCH_X86
    const auto& f = detail::get_cpu_features();
    return f.avx512f && f.osxsave;
#else
    return false;
#endif
}

/**
 * @brief Check if CPU supports AVX-512BW (runtime detection).
 * @return true if AVX-512BW is supported (needed for byte-level hash map ops).
 * @note AVX-512BW implies AVX-512F.
 */
inline bool cpu_has_avx512bw() noexcept {
#if FATP_ARCH_X86
    const auto& f = detail::get_cpu_features();
    return f.avx512bw && f.avx512f && f.osxsave;
#else
    return false;
#endif
}

/**
 * @brief Check if platform has NEON support.
 * @return true on ARM with NEON, false otherwise.
 * @note NEON is always available on AArch64; runtime detection not needed.
 */
inline bool cpu_has_neon() noexcept {
#if defined(FATP_SIMD_NEON)
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Compile-Time Query API
// =============================================================================

/**
 * @brief Check if compiled with AVX-512F support.
 */
constexpr bool compiled_with_avx512f() noexcept {
#if defined(FATP_SIMD_AVX512F)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if compiled with AVX-512BW support (needed for byte ops).
 */
constexpr bool compiled_with_avx512bw() noexcept {
#if defined(FATP_SIMD_AVX512BW)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if compiled with AVX2 support.
 */
constexpr bool compiled_with_avx2() noexcept {
#if defined(FATP_SIMD_AVX2)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if compiled with SSE2 support.
 */
constexpr bool compiled_with_sse2() noexcept {
#if defined(FATP_SIMD_SSE2)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if compiled with NEON support.
 */
constexpr bool compiled_with_neon() noexcept {
#if defined(FATP_SIMD_NEON)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if any SIMD support is compiled in.
 */
constexpr bool has_simd() noexcept {
#if FATP_HAS_SIMD
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Diagnostic API
// =============================================================================

/**
 * @brief Get the compile-time SIMD backend name.
 * @return String like "AVX-512BW", "AVX-512F", "AVX2", "SSE2", "NEON-AArch64", "NEON", "Scalar"
 */
inline const char* compiled_backend() noexcept {
#if defined(FATP_SIMD_AVX512BW)
    return "AVX-512BW";
#elif defined(FATP_SIMD_AVX512F)
    return "AVX-512F";
#elif defined(FATP_SIMD_AVX2)
    return "AVX2";
#elif defined(FATP_SIMD_SSE2)
    return "SSE2";
#elif defined(FATP_SIMD_NEON) && FATP_SIMD_NEON_AARCH64
    return "NEON-AArch64";
#elif defined(FATP_SIMD_NEON)
    return "NEON";
#else
    return "Scalar";
#endif
}

/**
 * @brief Get the runtime CPU SIMD capability.
 * @return String like "AVX-512BW", "AVX-512F", "AVX2", "SSE2", "NEON-AArch64", "Scalar"
 */
inline const char* cpu_capability() noexcept {
#if FATP_ARCH_X86
    if (cpu_has_avx512bw()) return "AVX-512BW";
    if (cpu_has_avx512f()) return "AVX-512F";
    if (cpu_has_avx2()) return "AVX2";
    if (cpu_has_sse2()) return "SSE2";
    return "Scalar";
#elif defined(FATP_SIMD_NEON) && FATP_SIMD_NEON_AARCH64
    return "NEON-AArch64";
#elif defined(FATP_SIMD_NEON)
    return "NEON";
#else
    return "Scalar";
#endif
}

/**
 * @brief Check if we're using the best SIMD available on this CPU.
 * @return true if compiled backend matches CPU capability.
 */
inline bool is_optimal() noexcept {
#if FATP_ARCH_X86
    // Check from highest to lowest
    if (cpu_has_avx512bw()) {
        return compiled_with_avx512bw();
    }
    if (cpu_has_avx512f()) {
        return compiled_with_avx512f();
    }
    if (cpu_has_avx2()) {
        return compiled_with_avx2();
    }
    if (cpu_has_sse2()) {
        return compiled_with_sse2();
    }
    return true;  // No SIMD available, scalar is optimal
#elif FATP_ARCH_ARM
    return compiled_with_neon();  // NEON is always optimal on ARM
#else
    return true;  // Unknown arch, assume optimal
#endif
}

/**
 * @brief Get a hint for how to improve SIMD usage.
 * @return Hint string, or nullptr if already optimal.
 *
 * Example returns:
 *   "CPU supports AVX2. Recompile with /arch:AVX2 (MSVC) or -mavx2 (GCC/Clang)"
 *   "CPU supports AVX-512. Recompile with /arch:AVX512 (MSVC) or -mavx512bw (GCC/Clang)"
 *   nullptr (if already optimal)
 */
inline const char* optimization_hint() noexcept {
    if (is_optimal()) {
        return nullptr;
    }
    
#if FATP_ARCH_X86
    if (cpu_has_avx512bw() && !compiled_with_avx512bw()) {
        return "CPU supports AVX-512BW. Recompile with /arch:AVX512 (MSVC) or -mavx512bw (GCC/Clang)";
    }
    if (cpu_has_avx512f() && !compiled_with_avx512f()) {
        return "CPU supports AVX-512F. Recompile with /arch:AVX512 (MSVC) or -mavx512f (GCC/Clang)";
    }
    if (cpu_has_avx2() && !compiled_with_avx2()) {
        return "CPU supports AVX2. Recompile with /arch:AVX2 (MSVC) or -mavx2 (GCC/Clang)";
    }
#endif
    
    return nullptr;
}

/**
 * @brief Get a combined status string for diagnostics.
 * @return String like "AVX2 (optimal)" or "SSE2 (CPU has AVX2)"
 */
inline const char* status() noexcept {
#if defined(FATP_SIMD_AVX512BW)
    return "AVX-512BW (optimal)";
#elif defined(FATP_SIMD_AVX512F)
    if (cpu_has_avx512bw()) {
        return "AVX-512F (CPU has AVX-512BW)";
    }
    return "AVX-512F (optimal)";
#elif defined(FATP_SIMD_AVX2)
    if (cpu_has_avx512bw()) {
        return "AVX2 (CPU has AVX-512BW)";
    }
    if (cpu_has_avx512f()) {
        return "AVX2 (CPU has AVX-512F)";
    }
    return "AVX2 (optimal)";
#elif defined(FATP_SIMD_SSE2)
    if (cpu_has_avx512bw()) {
        return "SSE2 (CPU has AVX-512BW)";
    }
    if (cpu_has_avx512f()) {
        return "SSE2 (CPU has AVX-512F)";
    }
    if (cpu_has_avx2()) {
        return "SSE2 (CPU has AVX2)";
    }
    return "SSE2 (optimal)";
#elif defined(FATP_SIMD_NEON) && FATP_SIMD_NEON_AARCH64
    return "NEON-AArch64 (optimal)";
#elif defined(FATP_SIMD_NEON)
    return "NEON (optimal)";
#else
    return "Scalar";
#endif
}

// =============================================================================
// SIMD Width Constants
// =============================================================================

/**
 * @brief Get the SIMD register width in bytes for the compiled backend.
 */
constexpr size_t register_width_bytes() noexcept {
#if defined(FATP_SIMD_AVX512F)
    return 64;  // 512-bit
#elif defined(FATP_SIMD_AVX2)
    return 32;  // 256-bit
#elif defined(FATP_SIMD_SSE2) || defined(FATP_SIMD_NEON)
    return 16;  // 128-bit
#else
    return 0;   // No SIMD
#endif
}

/**
 * @brief Get the number of 32-bit integers per SIMD register.
 */
constexpr size_t int32_per_register() noexcept {
    return register_width_bytes() / sizeof(int32_t);
}

/**
 * @brief Get the number of 64-bit integers per SIMD register.
 */
constexpr size_t int64_per_register() noexcept {
    return register_width_bytes() / sizeof(int64_t);
}

/**
 * @brief Get the number of doubles per SIMD register.
 */
constexpr size_t doubles_per_register() noexcept {
    return register_width_bytes() / sizeof(double);
}

/**
 * @brief Get the number of floats per SIMD register.
 */
constexpr size_t floats_per_register() noexcept {
    return register_width_bytes() / sizeof(float);
}

} // namespace simd
} // namespace fat_p
