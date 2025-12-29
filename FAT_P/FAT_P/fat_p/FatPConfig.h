#pragma once

// FatPConfig.h
// Central configuration + portability macros for the Fat-P header-only library.
//
// Systemic Hygiene Policy Rule F: single source of truth for configuration macros.

#ifndef FATP_CONFIG_H
#define FATP_CONFIG_H

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

#endif // FATP_CONFIG_H
