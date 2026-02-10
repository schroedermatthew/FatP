#pragma once

/*
FATP_META:
  meta_version: 1
  component: PlatformDetection
  file_role: public_header
  path: include/fat_p/PlatformDetection.h
  namespace: fat_p
  layer: Foundation
  summary: Compiler, platform, and hardware feature detection macros.
  api_stability: stable
  related:
    docs_search: "PlatformDetection"
    tests: []
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 80
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file PlatformDetection.h
 * @brief Compiler, platform, and architecture detection.
 *
 * Centralized detection of:
 * - Compiler (MSVC, GCC, Clang)
 * - Operating system (Windows, POSIX, Linux, macOS)
 * - CPU architecture (x64, x86, ARM64, ARM32)
 * - Hardware features (NUMA)
 * - Build configuration (debug/release)
 *
 * For SIMD detection, use SimdDetection.h instead.
 *
 * All Fat-P headers should use the macros defined here rather than probing
 * compiler/platform macros directly.
 */

// =============================================================================
// Compiler Detection
// =============================================================================

#if defined(__clang__)
#define FATP_COMPILER_CLANG 1
#define FATP_COMPILER_GCC 0
#define FATP_COMPILER_MSVC 0
#define FATP_COMPILER_NAME "Clang"
#elif defined(__GNUC__)
#define FATP_COMPILER_CLANG 0
#define FATP_COMPILER_GCC 1
#define FATP_COMPILER_MSVC 0
#define FATP_COMPILER_NAME "GCC"
#elif defined(_MSC_VER)
#define FATP_COMPILER_CLANG 0
#define FATP_COMPILER_GCC 0
#define FATP_COMPILER_MSVC 1
#define FATP_COMPILER_NAME "MSVC"
#else
#define FATP_COMPILER_CLANG 0
#define FATP_COMPILER_GCC 0
#define FATP_COMPILER_MSVC 0
#define FATP_COMPILER_NAME "Unknown"
#endif

// Compiler version (major * 100 + minor)
#if FATP_COMPILER_CLANG
#define FATP_COMPILER_VERSION (__clang_major__ * 100 + __clang_minor__)
#elif FATP_COMPILER_GCC
#define FATP_COMPILER_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#elif FATP_COMPILER_MSVC
#define FATP_COMPILER_VERSION (_MSC_VER)
#else
#define FATP_COMPILER_VERSION 0
#endif

// =============================================================================
// Platform Detection
// =============================================================================

#if defined(_WIN32) || defined(_WIN64)
#define FATP_PLATFORM_WINDOWS 1
#define FATP_PLATFORM_POSIX 0
#define FATP_PLATFORM_LINUX 0
#define FATP_PLATFORM_MACOS 0
#define FATP_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
#define FATP_PLATFORM_WINDOWS 0
#define FATP_PLATFORM_POSIX 1
#define FATP_PLATFORM_LINUX 0
#define FATP_PLATFORM_MACOS 1
#define FATP_PLATFORM_NAME "macOS"
#elif defined(__linux__)
#define FATP_PLATFORM_WINDOWS 0
#define FATP_PLATFORM_POSIX 1
#define FATP_PLATFORM_LINUX 1
#define FATP_PLATFORM_MACOS 0
#define FATP_PLATFORM_NAME "Linux"
#elif defined(__unix__)
#define FATP_PLATFORM_WINDOWS 0
#define FATP_PLATFORM_POSIX 1
#define FATP_PLATFORM_LINUX 0
#define FATP_PLATFORM_MACOS 0
#define FATP_PLATFORM_NAME "Unix"
#else
#define FATP_PLATFORM_WINDOWS 0
#define FATP_PLATFORM_POSIX 0
#define FATP_PLATFORM_LINUX 0
#define FATP_PLATFORM_MACOS 0
#define FATP_PLATFORM_NAME "Unknown"
#endif

// =============================================================================
// Architecture Detection
// =============================================================================

#if defined(__x86_64__) || defined(_M_X64)
#define FATP_ARCH_X64 1
#define FATP_ARCH_X86 0
#define FATP_ARCH_ARM64 0
#define FATP_ARCH_ARM32 0
#define FATP_ARCH_NAME "x64"
#elif defined(__i386__) || defined(_M_IX86)
#define FATP_ARCH_X64 0
#define FATP_ARCH_X86 1
#define FATP_ARCH_ARM64 0
#define FATP_ARCH_ARM32 0
#define FATP_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define FATP_ARCH_X64 0
#define FATP_ARCH_X86 0
#define FATP_ARCH_ARM64 1
#define FATP_ARCH_ARM32 0
#define FATP_ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
#define FATP_ARCH_X64 0
#define FATP_ARCH_X86 0
#define FATP_ARCH_ARM64 0
#define FATP_ARCH_ARM32 1
#define FATP_ARCH_NAME "ARM32"
#else
#define FATP_ARCH_X64 0
#define FATP_ARCH_X86 0
#define FATP_ARCH_ARM64 0
#define FATP_ARCH_ARM32 0
#define FATP_ARCH_NAME "Unknown"
#endif

// Pointer size
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#define FATP_ARCH_64BIT 1
#define FATP_ARCH_32BIT 0
#else
#define FATP_ARCH_64BIT 0
#define FATP_ARCH_32BIT 1
#endif

// =============================================================================
// Hardware Features
// =============================================================================

// NUMA support (Linux libnuma)
#if __has_include(<numa.h>)
#define FATP_HAS_NUMA 1
#else
#define FATP_HAS_NUMA 0
#endif

// Cache line size (bytes).
// Consumers may override by defining FATP_CACHE_LINE_SIZE before including Fat-P.
//   - Apple Silicon (M1/M2/M3): 128-byte coherency granule
//   - x86/x64, most ARM: 64 bytes
#ifndef FATP_CACHE_LINE_SIZE
#if FATP_PLATFORM_MACOS && FATP_ARCH_ARM64
#define FATP_CACHE_LINE_SIZE 128
#else
#define FATP_CACHE_LINE_SIZE 64
#endif
#endif

// =============================================================================
// Build Configuration
// =============================================================================

#if defined(NDEBUG)
#define FATP_BUILD_RELEASE 1
#define FATP_BUILD_DEBUG 0
#else
#define FATP_BUILD_RELEASE 0
#define FATP_BUILD_DEBUG 1
#endif

// =============================================================================
// Utilities
// =============================================================================

namespace fat_p
{

inline constexpr const char* compiler_name() noexcept
{
    return FATP_COMPILER_NAME;
}

inline constexpr int compiler_version() noexcept
{
    return FATP_COMPILER_VERSION;
}

inline constexpr const char* platform_name() noexcept
{
    return FATP_PLATFORM_NAME;
}

inline constexpr const char* arch_name() noexcept
{
    return FATP_ARCH_NAME;
}

} // namespace fat_p
