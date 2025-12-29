/**
 * @file CppStandardDetection.h
 * @brief Portable C++ standard version detection for all compilers
 * 
 * @details Provides robust compile-time detection of C++ standard versions
 * that works correctly across all major compilers including MSVC, GCC, and Clang.
 * 
 * MSVC Issue: By default, MSVC reports __cplusplus as 199711L (C++98) even when
 * compiling with /std:c++17 or later, unless /Zc:__cplusplus is specified.
 * This header detects the actual standard using _MSVC_LANG for MSVC.
 * 
 * @section macros Available Macros
 * 
 * Version Detection:
 * - FATP_HAS_CPP17 - Set to 1 if C++17 or later, 0 otherwise
 * - FATP_HAS_CPP20 - Set to 1 if C++20 or later, 0 otherwise
 * - FATP_HAS_CPP23 - Set to 1 if C++23 or later, 0 otherwise
 * 
 * Standard Values:
 * - FATP_CPLUSPLUS - Actual C++ standard value (works on all compilers)
 * 
 * Compiler Detection:
 * - FATP_COMPILER_MSVC - Defined if compiling with Microsoft Visual C++
 * - FATP_COMPILER_GCC - Defined if compiling with GCC
 * - FATP_COMPILER_CLANG - Defined if compiling with Clang
 * 
 * 
 * @section usage Usage Example
 * @code
 * #include "CppStandardDetection.h"
 * 
 * #if FATP_HAS_CPP17
 *     // Use C++17 features
 * #endif
 * 
 * #if FATP_HAS_CPP20
 *     // Use C++20 features like concepts, ranges
 * #endif
 * 
 * #if FATP_COMPILER_MSVC
 *     // MSVC-specific code
 * #endif
 * @endcode
 * 
 * @note This header is standalone and has no dependencies
 * @note All macros are preprocessor-level (no runtime overhead)
 * @note Thread-safe: All operations are compile-time only
 * @note Header-only, no dependencies beyond standard library
 */

#pragma once

// Compiler detection
#if defined(_MSC_VER)
    #define FATP_COMPILER_MSVC 1
#else
    #define FATP_COMPILER_MSVC 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
    #define FATP_COMPILER_GCC 1
#else
    #define FATP_COMPILER_GCC 0
#endif

#if defined(__clang__)
    #define FATP_COMPILER_CLANG 1
#else
    #define FATP_COMPILER_CLANG 0
#endif

// Get the actual C++ standard value
// MSVC requires special handling via _MSVC_LANG
#if FATP_COMPILER_MSVC
    #ifdef _MSVC_LANG
        #define FATP_CPLUSPLUS _MSVC_LANG
    #else
        #define FATP_CPLUSPLUS __cplusplus
    #endif
#else
    #define FATP_CPLUSPLUS __cplusplus
#endif

// C++17 detection
// Standard value: 201703L
#if FATP_CPLUSPLUS >= 201703L
    #define FATP_HAS_CPP17 1
#else
    #define FATP_HAS_CPP17 0
#endif

// C++20 detection
// Standard value: 202002L
#if FATP_CPLUSPLUS >= 202002L
    #define FATP_HAS_CPP20 1
#else
    #define FATP_HAS_CPP20 0
#endif

// C++23 detection
// Standard value: 202302L
#if FATP_CPLUSPLUS >= 202302L
    #define FATP_HAS_CPP23 1
#else
    #define FATP_HAS_CPP23 0
#endif

// C++26 detection (future-proofing)
// Standard value: TBD (likely 202600L)
#if FATP_CPLUSPLUS >= 202600L
    #define FATP_HAS_CPP26 1
#else
    #define FATP_HAS_CPP26 0
#endif

// Compile-time check that we have at least C++17
#if !FATP_HAS_CPP17
    #error "This library requires C++17 or later. Please use /std:c++17 or -std=c++17"
#endif

// Diagnostic helpers for debugging standard detection
namespace fat_p
{
namespace detail
{

// Get C++ standard as compile-time constant
inline constexpr long cplusplus_value = FATP_CPLUSPLUS;

// Get C++ standard as string (for debugging)
inline constexpr const char* cplusplus_string()
{
    if constexpr (FATP_CPLUSPLUS >= 202302L)
    {
        return "C++23";
    }
    else if constexpr (FATP_CPLUSPLUS >= 202002L)
    {
        return "C++20";
    }
    else if constexpr (FATP_CPLUSPLUS >= 201703L)
    {
        return "C++17";
    }
    else if constexpr (FATP_CPLUSPLUS >= 201402L)
    {
        return "C++14";
    }
    else if constexpr (FATP_CPLUSPLUS >= 201103L)
    {
        return "C++11";
    }
    else
    {
        return "C++98 or earlier";
    }
}

// Get compiler name as string (for debugging)
inline constexpr const char* compiler_name()
{
    if constexpr (FATP_COMPILER_MSVC)
    {
        return "MSVC";
    }
    else if constexpr (FATP_COMPILER_CLANG)
    {
        return "Clang";
    }
    else if constexpr (FATP_COMPILER_GCC)
    {
        return "GCC";
    }
    else
    {
        return "Unknown";
    }
}

} // namespace detail
} // namespace fat_p
