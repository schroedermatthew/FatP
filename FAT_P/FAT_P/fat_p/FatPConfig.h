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
  path: fat_p/FatPConfig.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for FatPConfig."
  api_stability: in_work
  related:
    docs_search: "FatPConfig"
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 8
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

namespace fat_p
{
namespace config
{
// Stable, ABI-friendly cache line size constant.
inline constexpr std::size_t cache_line_size = FATP_CACHE_LINE_SIZE;
} // namespace config
} // namespace fat_p

