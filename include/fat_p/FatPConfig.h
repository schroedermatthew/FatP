/**
 * @file FatPConfig.h
 * @brief Central configuration macros for FAT-P library
 *
 * @layer Foundation
 *
 * @details
 * Central configuration + portability macros for the Fat-P header-only library.
 * Systemic Hygiene Policy Rule F: single source of truth for configuration macros.
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPConfig
  file_role: public_header
  path: include/fat_p/FatPConfig.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for FatPConfig."
  api_stability: in_work
  related:
    docs_search: "FatPConfig"
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 9
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <cstddef>

// =============================================================================
// Cache line size
// =============================================================================
// Consumers may override by defining FATP_CACHE_LINE_SIZE before including Fat-P.
#ifndef FATP_CACHE_LINE_SIZE
#define FATP_CACHE_LINE_SIZE 64
#endif

// =============================================================================
// [[no_unique_address]] compatibility
// =============================================================================
//
// - On MSVC in C++17, prefer [[msvc::no_unique_address]] when available.
// - On other toolchains, use [[no_unique_address]] when supported, otherwise empty.
//
// NOTE: This macro must be defined in exactly one place (Rule F).
#ifndef FATP_NO_UNIQUE_ADDRESS
#if defined(_MSC_VER)
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(msvc::no_unique_address)
#define FATP_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
#define FATP_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define FATP_NO_UNIQUE_ADDRESS
#endif
#else
#define FATP_NO_UNIQUE_ADDRESS
#endif
#else
#if defined(__has_cpp_attribute) && __has_cpp_attribute(no_unique_address)
#define FATP_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define FATP_NO_UNIQUE_ADDRESS
#endif
#endif
#endif

// =============================================================================
// Branch prediction hints
// =============================================================================
// Help optimizer with branch prediction but not required for correctness.
#ifndef FATP_LIKELY
#if defined(__GNUC__) || defined(__clang__)
#define FATP_LIKELY(x) __builtin_expect(!!(x), 1)
#define FATP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
// MSVC and others: no-op, optimizer handles it
#define FATP_LIKELY(x) (x)
#define FATP_UNLIKELY(x) (x)
#endif
#endif

// =============================================================================
// Force inlining for hot paths
// =============================================================================
// Used to ensure critical fast paths are always inlined regardless of
// compiler optimization settings.
#ifndef FATP_FORCEINLINE
#if defined(_MSC_VER)
#define FATP_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FATP_FORCEINLINE inline __attribute__((always_inline))
#else
#define FATP_FORCEINLINE inline
#endif
#endif

// =============================================================================
// Prevent inlining for cold paths
// =============================================================================
// Used to keep cold paths (error handling, reallocation) out of hot code
// to improve I-cache utilization. The 'cold' attribute on GCC/Clang also
// hints that the function is unlikely to be called.
#ifndef FATP_NOINLINE
#if defined(_MSC_VER)
#define FATP_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define FATP_NOINLINE __attribute__((noinline, cold))
#else
#define FATP_NOINLINE
#endif
#endif

// =============================================================================
// Optional iostream support
// =============================================================================
// Controls whether components provide operator<< for std::ostream.
// Consumers may override by defining FATP_ENABLE_IOSTREAM before including Fat-P.
// Default: enabled (1). Set to 0 to disable <iostream> includes.
#ifndef FATP_ENABLE_IOSTREAM
#define FATP_ENABLE_IOSTREAM 1
#endif

namespace fat_p
{
namespace config
{
// Stable, ABI-friendly cache line size constant.
inline constexpr std::size_t cache_line_size = FATP_CACHE_LINE_SIZE;
} // namespace config
} // namespace fat_p
