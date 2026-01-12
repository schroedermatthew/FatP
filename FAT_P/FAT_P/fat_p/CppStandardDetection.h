/**
 * @file CppStandardDetection.h
 * @brief Centralized C++ standard and feature detection.
 *
 * @layer Foundation
 *
 * This header provides centralized detection of C++ standard version and
 * library feature availability. All other Fat-P headers must use the macros
 * defined here rather than probing __cplusplus or feature-test macros directly.
 *
 * Per Fat-P Guidelines §1.1.2: All feature detection must live in this header.
 *
 * @note Feature macros represent actual library availability, not merely
 *       language mode. A C++20 compiler with an older standard library may
 *       not have all C++20 library features.
 */

#pragma once

// Library feature-test macros (e.g., __cpp_lib_span) are centrally provided via <version>.
// This avoids false negatives across standard library implementations.
#include <version>

// =============================================================================
// Language Standard Detection
// =============================================================================

// Detect C++ standard version
// MSVC uses _MSVC_LANG for /std: flag detection
#if defined(_MSVC_LANG)
    #define FATP_CPLUSPLUS _MSVC_LANG
#else
    #define FATP_CPLUSPLUS __cplusplus
#endif

// Standard version flags
#if FATP_CPLUSPLUS >= 202600L
    #define FATP_CPP26_OR_LATER 1
    #define FATP_CPP23_OR_LATER 1
    #define FATP_CPP20_OR_LATER 1
    #define FATP_CPP17_OR_LATER 1
#elif FATP_CPLUSPLUS >= 202302L
    #define FATP_CPP26_OR_LATER 0
    #define FATP_CPP23_OR_LATER 1
    #define FATP_CPP20_OR_LATER 1
    #define FATP_CPP17_OR_LATER 1
#elif FATP_CPLUSPLUS >= 202002L
    #define FATP_CPP26_OR_LATER 0
    #define FATP_CPP23_OR_LATER 0
    #define FATP_CPP20_OR_LATER 1
    #define FATP_CPP17_OR_LATER 1
#elif FATP_CPLUSPLUS >= 201703L
    #define FATP_CPP26_OR_LATER 0
    #define FATP_CPP23_OR_LATER 0
    #define FATP_CPP20_OR_LATER 0
    #define FATP_CPP17_OR_LATER 1
#else
    #error "Fat-P requires C++17 or later"
#endif

// Backward compatibility aliases
// Some headers use FATP_HAS_CPP20 instead of FATP_CPP20_OR_LATER
#define FATP_HAS_CPP17 FATP_CPP17_OR_LATER
#define FATP_HAS_CPP20 FATP_CPP20_OR_LATER
#define FATP_HAS_CPP23 FATP_CPP23_OR_LATER
#define FATP_HAS_CPP26 FATP_CPP26_OR_LATER

// =============================================================================
// C++20 Language Feature Detection
// =============================================================================

// Concepts
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    #define FATP_HAS_CONCEPTS 1
#else
    #define FATP_HAS_CONCEPTS 0
#endif

// Designated initializers (C++20)
#if defined(__cpp_designated_initializers) && __cpp_designated_initializers >= 201707L
    #define FATP_HAS_DESIGNATED_INIT 1
#else
    #define FATP_HAS_DESIGNATED_INIT 0
#endif

// consteval
#if defined(__cpp_consteval) && __cpp_consteval >= 201811L
    #define FATP_HAS_CONSTEVAL 1
#else
    #define FATP_HAS_CONSTEVAL 0
#endif

// constinit
#if defined(__cpp_constinit) && __cpp_constinit >= 201907L
    #define FATP_HAS_CONSTINIT 1
#else
    #define FATP_HAS_CONSTINIT 0
#endif

// Three-way comparison (spaceship operator)
#if defined(__cpp_impl_three_way_comparison) && __cpp_impl_three_way_comparison >= 201907L
    #define FATP_HAS_SPACESHIP 1
#else
    #define FATP_HAS_SPACESHIP 0
#endif

// Coroutines
#if (defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L) || \
    (defined(__cpp_coroutines) && __cpp_coroutines >= 201902L)
    #define FATP_HAS_COROUTINES 1
#else
    #define FATP_HAS_COROUTINES 0
#endif

// Modules (language feature)
#if defined(__cpp_modules) && __cpp_modules >= 201907L
    #define FATP_HAS_MODULES 1
#else
    #define FATP_HAS_MODULES 0
#endif

// =============================================================================
// C++20 Library Feature Detection
// =============================================================================

// std::source_location
#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L
    #define FATP_HAS_SOURCE_LOCATION 1
#else
    #define FATP_HAS_SOURCE_LOCATION 0
#endif

// std::span
#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L
    #define FATP_HAS_STD_SPAN 1
#else
    #define FATP_HAS_STD_SPAN 0
#endif

// Ranges library
#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
    #define FATP_HAS_RANGES 1
#else
    #define FATP_HAS_RANGES 0
#endif

// std::format
#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
    #define FATP_HAS_FORMAT 1
#else
    #define FATP_HAS_FORMAT 0
#endif

// std::bit_cast
#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
    #define FATP_HAS_BIT_CAST 1
#else
    #define FATP_HAS_BIT_CAST 0
#endif

// std::to_array
#if defined(__cpp_lib_to_array) && __cpp_lib_to_array >= 201907L
    #define FATP_HAS_TO_ARRAY 1
#else
    #define FATP_HAS_TO_ARRAY 0
#endif

// std::starts_with / std::ends_with for strings
#if defined(__cpp_lib_starts_ends_with) && __cpp_lib_starts_ends_with >= 201711L
    #define FATP_HAS_STARTS_ENDS_WITH 1
#else
    #define FATP_HAS_STARTS_ENDS_WITH 0
#endif

// std::erase / std::erase_if
#if defined(__cpp_lib_erase_if) && __cpp_lib_erase_if >= 202002L
    #define FATP_HAS_ERASE_IF 1
#else
    #define FATP_HAS_ERASE_IF 0
#endif

// std::jthread
#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
    #define FATP_HAS_JTHREAD 1
#else
    #define FATP_HAS_JTHREAD 0
#endif

// Atomic wait/notify
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
    #define FATP_HAS_ATOMIC_WAIT 1
#else
    #define FATP_HAS_ATOMIC_WAIT 0
#endif

// std::latch and std::barrier
#if defined(__cpp_lib_latch) && __cpp_lib_latch >= 201907L
    #define FATP_HAS_LATCH 1
#else
    #define FATP_HAS_LATCH 0
#endif

#if defined(__cpp_lib_barrier) && __cpp_lib_barrier >= 201907L
    #define FATP_HAS_BARRIER 1
#else
    #define FATP_HAS_BARRIER 0
#endif

// std::semaphore
#if defined(__cpp_lib_semaphore) && __cpp_lib_semaphore >= 201907L
    #define FATP_HAS_SEMAPHORE 1
#else
    #define FATP_HAS_SEMAPHORE 0
#endif

// =============================================================================
// C++17 Library Feature Detection (for completeness)
// =============================================================================

// std::optional
#if defined(__cpp_lib_optional) && __cpp_lib_optional >= 201606L
    #define FATP_HAS_OPTIONAL 1
#else
    #define FATP_HAS_OPTIONAL 0
#endif

// std::variant
#if defined(__cpp_lib_variant) && __cpp_lib_variant >= 201606L
    #define FATP_HAS_VARIANT 1
#else
    #define FATP_HAS_VARIANT 0
#endif

// std::string_view
#if defined(__cpp_lib_string_view) && __cpp_lib_string_view >= 201606L
    #define FATP_HAS_STRING_VIEW 1
#else
    #define FATP_HAS_STRING_VIEW 0
#endif

// std::filesystem
#if defined(__cpp_lib_filesystem) && __cpp_lib_filesystem >= 201703L
    #define FATP_HAS_FILESYSTEM 1
#else
    #define FATP_HAS_FILESYSTEM 0
#endif

// =============================================================================
// Compiler Detection
// =============================================================================

#if defined(__clang__)
    #define FATP_COMPILER_CLANG 1
    #define FATP_COMPILER_GCC 0
    #define FATP_COMPILER_MSVC 0
#elif defined(__GNUC__)
    #define FATP_COMPILER_CLANG 0
    #define FATP_COMPILER_GCC 1
    #define FATP_COMPILER_MSVC 0
#elif defined(_MSC_VER)
    #define FATP_COMPILER_CLANG 0
    #define FATP_COMPILER_GCC 0
    #define FATP_COMPILER_MSVC 1
#else
    #define FATP_COMPILER_CLANG 0
    #define FATP_COMPILER_GCC 0
    #define FATP_COMPILER_MSVC 0
#endif

// =============================================================================
// Platform Detection
// =============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define FATP_PLATFORM_WINDOWS 1
    #define FATP_PLATFORM_POSIX 0
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    #define FATP_PLATFORM_WINDOWS 0
    #define FATP_PLATFORM_POSIX 1
#else
    #define FATP_PLATFORM_WINDOWS 0
    #define FATP_PLATFORM_POSIX 0
#endif

// =============================================================================
// SIMD Detection
// =============================================================================

#if defined(__AVX512F__)
    #define FATP_HAS_AVX512 1
#else
    #define FATP_HAS_AVX512 0
#endif

#if defined(__AVX2__)
    #define FATP_HAS_AVX2 1
#else
    #define FATP_HAS_AVX2 0
#endif

#if defined(__AVX__)
    #define FATP_HAS_AVX 1
#else
    #define FATP_HAS_AVX 0
#endif

#if defined(__SSE4_2__)
    #define FATP_HAS_SSE42 1
#else
    #define FATP_HAS_SSE42 0
#endif

#if defined(__SSE2__) || (FATP_COMPILER_MSVC && defined(_M_X64))
    #define FATP_HAS_SSE2 1
#else
    #define FATP_HAS_SSE2 0
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define FATP_HAS_NEON 1
#else
    #define FATP_HAS_NEON 0
#endif

// =============================================================================
// Utility Macros
// =============================================================================

// Macro to conditionally include headers
// Usage: #if FATP_HAS_SOURCE_LOCATION
//        #include <source_location>
//        #endif

// Static assert for minimum standard
static_assert(FATP_CPP17_OR_LATER, "Fat-P requires C++17 or later");

// =============================================================================
// Runtime Utility Functions
// =============================================================================

namespace fat_p {
namespace detail {

// Get the actual __cplusplus value (works correctly on MSVC)
inline constexpr long cplusplus_value = FATP_CPLUSPLUS;

// Get C++ standard as string (for debugging/logging)
inline constexpr const char* cplusplus_string()
{
    if constexpr (FATP_CPLUSPLUS >= 202600L)
    {
        return "C++26";
    }
    else if constexpr (FATP_CPLUSPLUS >= 202302L)
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

// Get compiler name as string (for debugging/logging)
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
