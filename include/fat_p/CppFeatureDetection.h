#pragma once

/*
FATP_META:
  meta_version: 1
  component: CppFeatureDetection
  file_role: public_header
  path: include/fat_p/CppFeatureDetection.h
  namespace: fat_p
  layer: Foundation
  summary: C++ language standard and library feature detection macros.
  api_stability: stable
  related:
    docs_search: "CppFeatureDetection"
    tests: []
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 38
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file CppFeatureDetection.h
 * @brief C++ language and library feature detection.
 *
 * Centralized detection of C++ standard version and library feature availability.
 * All Fat-P headers must use the macros defined here rather than probing
 * __cplusplus or feature-test macros directly.
 *
 * @note Fat-P requires C++20 or later.
 * @note Feature macros represent actual library availability, not merely
 *       language mode. A C++20 compiler with an older standard library may
 *       not have all C++20 library features.
 */

#include <version>

// =============================================================================
// Language Standard Detection
// =============================================================================

#if defined(_MSVC_LANG)
#define FATP_CPLUSPLUS _MSVC_LANG
#else
#define FATP_CPLUSPLUS __cplusplus
#endif

#if FATP_CPLUSPLUS < 202002L
#error "Fat-P requires C++20 or later"
#endif

// Standard version flags
#if FATP_CPLUSPLUS >= 202600L
#define FATP_CPP26_OR_LATER 1
#define FATP_CPP23_OR_LATER 1
#elif FATP_CPLUSPLUS >= 202302L
#define FATP_CPP26_OR_LATER 0
#define FATP_CPP23_OR_LATER 1
#else
#define FATP_CPP26_OR_LATER 0
#define FATP_CPP23_OR_LATER 0
#endif

#define FATP_CPP20_OR_LATER 1

// =============================================================================
// C++20 Library Features (Implementation-Dependent)
// =============================================================================
// These are C++20 features but library support lagged on some implementations.
// Detection is still required.

// std::format - GCC 13 (2023), Clang 17 (2023), MSVC 2019 16.10 (2021)
// Lagged 2-3 years on GCC/Clang. Keep fallback to ostringstream.
#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#define FATP_HAS_FORMAT 1
#else
#define FATP_HAS_FORMAT 0
#endif

// std::jthread - late on some implementations
#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
#define FATP_HAS_JTHREAD 1
#else
#define FATP_HAS_JTHREAD 0
#endif

// Coroutines - library support varies
// Prefer language feature availability, then library macro if present.
// Note: Some standard libraries only expose __cpp_lib_coroutine after <coroutine> is included.
#if defined(__has_include)
#if __has_include(<coroutine>)
#include <coroutine>
#endif
#endif

#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L
#define FATP_HAS_COROUTINES 1
#elif defined(__cpp_lib_coroutine) && __cpp_lib_coroutine >= 201902L
#define FATP_HAS_COROUTINES 1
#else
#define FATP_HAS_COROUTINES 0
#endif

// Modules - optional, build system dependent
#if defined(__cpp_modules) && __cpp_modules >= 201907L
#define FATP_HAS_MODULES 1
#else
#define FATP_HAS_MODULES 0
#endif

// Synchronization primitives - late on some implementations
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
#define FATP_HAS_ATOMIC_WAIT 1
#else
#define FATP_HAS_ATOMIC_WAIT 0
#endif

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

#if defined(__cpp_lib_semaphore) && __cpp_lib_semaphore >= 201907L
#define FATP_HAS_SEMAPHORE 1
#else
#define FATP_HAS_SEMAPHORE 0
#endif

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
#define FATP_HAS_ATOMIC_SHARED_PTR 1
#else
#define FATP_HAS_ATOMIC_SHARED_PTR 0
#endif

// =============================================================================
// C++23 Library Features
// =============================================================================

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#define FATP_HAS_EXPECTED 1
#else
#define FATP_HAS_EXPECTED 0
#endif

#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#define FATP_HAS_STACKTRACE 1
#else
#define FATP_HAS_STACKTRACE 0
#endif

#if FATP_CPP23_OR_LATER && defined(__has_include)
#if __has_include(<mdspan>)
#include <mdspan>
#endif
#endif

#if defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L
#define FATP_HAS_MDSPAN 1
#else
#define FATP_HAS_MDSPAN 0
#endif

#if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
#define FATP_HAS_PRINT 1
#else
#define FATP_HAS_PRINT 0
#endif

// =============================================================================
// C++26 Library Features
// =============================================================================

#if defined(__cpp_lib_reflection) && __cpp_lib_reflection >= 202502L
#define FATP_HAS_REFLECTION 1
#else
#define FATP_HAS_REFLECTION 0
#endif

// =============================================================================
// Utilities
// =============================================================================

namespace fat_p
{

inline constexpr long cplusplus_value = FATP_CPLUSPLUS;

inline constexpr const char* cplusplus_string() noexcept
{
    if constexpr (FATP_CPLUSPLUS >= 202600L)
    {
        return "C++26";
    }
    else if constexpr (FATP_CPLUSPLUS >= 202302L)
    {
        return "C++23";
    }
    else
    {
        return "C++20";
    }
}

} // namespace fat_p
