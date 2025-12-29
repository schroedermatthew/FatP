/**
 * @file SimdVector.h
 * @brief Universal SIMD wrapper for vectorized HPC operations
 * @version 1.2.2
 * 
 * @details Portable SIMD abstraction layer supporting SSE, AVX, AVX-512, and NEON.
 * Provides compile-time architecture detection, safety checks, and masking support.
 * 
 * Version History:
 * - 1.2.2: MSVC __builtin_popcount fix (portable_popcount helper)
 * - 1.2.1: MSVC alignment workaround for partial load/store, enhanced mask docs
 * - 1.2.0: enforce.h integration, improved NEON mask implementations
 * - 1.1.0: SimdMask, comparisons, select(), compound operators, factories
 * - 1.0.0: Initial release with SSE2/AVX/AVX-512/NEON support
 * 
 * @note Floating-Point Only: This library supports ONLY float and double types.
 *       Integer SIMD is intentionally excluded because integer overflow detection
 *       lacks universal hardware semantics (unlike IEEE-754 NaN/Inf for FP).
 *       For checked integer arithmetic, use CheckedArithmetic.h which provides
 *       AVX2 acceleration where available with scalar fallback elsewhere.
 * 
 * @note NEON Double Precision: Only enabled on AArch64 (64-bit ARM).
 *       32-bit ARM uses scalar fallback for double operations (width=1).
 * 
 * @note Mask Invariants: SimdMask represents comparison results where each lane
 *       contains either all-ones (0xFFFFFFFF for true) or all-zeros (for false).
 *       Logical operations preserve this invariant. The popcount() method assumes
 *       masks are derived from comparisons or logical operations on comparison results.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

// Fat-P internal leverage for consistent error handling
#include "enforce.h"

// =============================================================================
// Architecture Detection & Verification
// =============================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #define SIMD_X86
    #if defined(_MSC_VER)
        #include <intrin.h>  // For __popcnt on MSVC
    #endif
    #if defined(__AVX512F__)
        #define SIMD_AVX512
        #include <immintrin.h>
    #elif defined(__AVX2__)
        #define SIMD_AVX2
        #include <immintrin.h>
    #elif defined(__AVX__)
        #define SIMD_AVX
        #include <immintrin.h>
    #elif defined(__SSE4_2__)
        #define SIMD_SSE4_2
        #include <nmmintrin.h>
    #elif defined(__SSE4_1__)
        #define SIMD_SSE4_1
        #include <smmintrin.h>
    #elif defined(__SSE3__)
        #define SIMD_SSE3
        #include <pmmintrin.h>
    #elif defined(__SSE2__)
        #define SIMD_SSE2
        #include <emmintrin.h>
    #endif
#elif defined(__aarch64__)
    #define SIMD_NEON
    #define SIMD_NEON_AARCH64 1
    #include <arm_neon.h>
#elif defined(__ARM_NEON)
    #define SIMD_NEON
    #define SIMD_NEON_AARCH64 0
    #include <arm_neon.h>
#endif

// Compile-time architecture verification (for CI/testing)
#if defined(SIMD_VERIFY_ARCHITECTURE)
    #if defined(SIMD_AVX512)
        static_assert(true, "SimdVector: AVX-512 detected");
    #elif defined(SIMD_AVX2)
        static_assert(true, "SimdVector: AVX2 detected");
    #elif defined(SIMD_AVX)
        static_assert(true, "SimdVector: AVX detected");
    #elif defined(SIMD_SSE2)
        static_assert(true, "SimdVector: SSE2 detected");
    #elif defined(SIMD_NEON) && SIMD_NEON_AARCH64
        static_assert(true, "SimdVector: NEON AArch64 detected");
    #elif defined(SIMD_NEON)
        static_assert(true, "SimdVector: NEON ARM32 detected (double=scalar)");
    #else
        static_assert(true, "SimdVector: Scalar fallback");
    #endif
#endif

#include "FatPTypeTraits.h"

namespace fat_p {

// =============================================================================
// MSVC Alignment Workaround
// =============================================================================

namespace detail {

/**
 * @brief Aligned buffer for partial load/store operations
 * 
 * MSVC prior to VS2022 does not guarantee stack alignment beyond 16 bytes.
 * This wrapper ensures proper alignment for AVX (32-byte) and AVX-512 (64-byte).
 */
template<typename T, size_t N, size_t Alignment>
struct alignas(Alignment) AlignedBuffer {
    T data[N];
    
    T& operator[](size_t i) noexcept { return data[i]; }
    const T& operator[](size_t i) const noexcept { return data[i]; }
    T* ptr() noexcept { return data; }
    const T* ptr() const noexcept { return data; }
};

#if defined(_MSC_VER) && !defined(__clang__) && _MSC_VER < 1930
// MSVC < VS2022: Use heap allocation for over-aligned buffers
template<typename T, size_t N, size_t Alignment>
struct HeapAlignedBuffer {
    T* data;
    
    HeapAlignedBuffer() {
        data = static_cast<T*>(_aligned_malloc(N * sizeof(T), Alignment));
        if (!data) throw std::bad_alloc();
        std::memset(data, 0, N * sizeof(T));
    }
    ~HeapAlignedBuffer() { _aligned_free(data); }
    
    HeapAlignedBuffer(const HeapAlignedBuffer&) = delete;
    HeapAlignedBuffer& operator=(const HeapAlignedBuffer&) = delete;
    
    T& operator[](size_t i) noexcept { return data[i]; }
    const T& operator[](size_t i) const noexcept { return data[i]; }
    T* ptr() noexcept { return data; }
    const T* ptr() const noexcept { return data; }
};

// Use heap allocation for alignment > 16 on old MSVC
template<typename T, size_t N, size_t Alignment>
using PartialBuffer = std::conditional_t<(Alignment > 16), 
                                          HeapAlignedBuffer<T, N, Alignment>,
                                          AlignedBuffer<T, N, Alignment>>;
#else
// Modern compilers: stack alignment works correctly
template<typename T, size_t N, size_t Alignment>
using PartialBuffer = AlignedBuffer<T, N, Alignment>;
#endif

/**
 * @brief Portable population count (number of set bits)
 * 
 * MSVC doesn't have __builtin_popcount, so we provide a cross-platform version.
 * Uses compiler intrinsics where available for optimal performance.
 */
inline unsigned int portable_popcount(unsigned int x) noexcept {
#if defined(_MSC_VER)
    // MSVC: Use __popcnt intrinsic (requires SSE4.2 or fallback)
    #if defined(__AVX__) || defined(__SSE4_2__) || defined(__POPCNT__)
        return __popcnt(x);
    #else
        // Software fallback for older MSVC without POPCNT
        x = x - ((x >> 1) & 0x55555555u);
        x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
        x = (x + (x >> 4)) & 0x0F0F0F0Fu;
        return (x * 0x01010101u) >> 24;
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned int>(__builtin_popcount(x));
#else
    // Portable fallback
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (x * 0x01010101u) >> 24;
#endif
}

} // namespace detail

// =============================================================================
// Forward Declarations
// =============================================================================

template<typename T> class SimdVector;
template<typename T> class SimdMask;

// =============================================================================
// SIMD Architecture Traits
// =============================================================================

struct SimdArchitecture {
#if defined(SIMD_AVX512)
    static constexpr bool has_avx512 = true;
    static constexpr bool has_avx2 = true;
    static constexpr bool has_avx = true;
    static constexpr bool has_sse = true;
    static constexpr bool has_neon = false;
    static constexpr size_t preferred_alignment = 64;
    static constexpr const char* name = "AVX-512";
#elif defined(SIMD_AVX2)
    static constexpr bool has_avx512 = false;
    static constexpr bool has_avx2 = true;
    static constexpr bool has_avx = true;
    static constexpr bool has_sse = true;
    static constexpr bool has_neon = false;
    static constexpr size_t preferred_alignment = 32;
    static constexpr const char* name = "AVX2";
#elif defined(SIMD_AVX)
    static constexpr bool has_avx512 = false;
    static constexpr bool has_avx2 = false;
    static constexpr bool has_avx = true;
    static constexpr bool has_sse = true;
    static constexpr bool has_neon = false;
    static constexpr size_t preferred_alignment = 32;
    static constexpr const char* name = "AVX";
#elif defined(SIMD_SSE2)
    static constexpr bool has_avx512 = false;
    static constexpr bool has_avx2 = false;
    static constexpr bool has_avx = false;
    static constexpr bool has_sse = true;
    static constexpr bool has_neon = false;
    static constexpr size_t preferred_alignment = 16;
    static constexpr const char* name = "SSE2";
#elif defined(SIMD_NEON)
    static constexpr bool has_avx512 = false;
    static constexpr bool has_avx2 = false;
    static constexpr bool has_avx = false;
    static constexpr bool has_sse = false;
    static constexpr bool has_neon = true;
    static constexpr size_t preferred_alignment = 16;
#if SIMD_NEON_AARCH64
    static constexpr const char* name = "NEON-AArch64";
#else
    static constexpr const char* name = "NEON-ARM32";
#endif
#else
    static constexpr bool has_avx512 = false;
    static constexpr bool has_avx2 = false;
    static constexpr bool has_avx = false;
    static constexpr bool has_sse = false;
    static constexpr bool has_neon = false;
    static constexpr size_t preferred_alignment = alignof(std::max_align_t);
    static constexpr const char* name = "Scalar";
#endif
};

// =============================================================================
// SIMD Traits
// =============================================================================

template<typename T>
struct SimdTraits {
    static constexpr size_t width = 1;
    static constexpr size_t alignment = alignof(T);
};

#if defined(SIMD_AVX512)
template<> struct SimdTraits<float> { 
    static constexpr size_t width = 16; 
    static constexpr size_t alignment = 64;
};
template<> struct SimdTraits<double> { 
    static constexpr size_t width = 8; 
    static constexpr size_t alignment = 64;
};
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
template<> struct SimdTraits<float> { 
    static constexpr size_t width = 8; 
    static constexpr size_t alignment = 32;
};
template<> struct SimdTraits<double> { 
    static constexpr size_t width = 4; 
    static constexpr size_t alignment = 32;
};
#elif defined(SIMD_SSE2)
template<> struct SimdTraits<float> { 
    static constexpr size_t width = 4; 
    static constexpr size_t alignment = 16;
};
template<> struct SimdTraits<double> { 
    static constexpr size_t width = 2; 
    static constexpr size_t alignment = 16;
};
#elif defined(SIMD_NEON)
template<> struct SimdTraits<float> { 
    static constexpr size_t width = 4; 
    static constexpr size_t alignment = 16;
};
template<> struct SimdTraits<double> { 
#if SIMD_NEON_AARCH64
    static constexpr size_t width = 2; 
    static constexpr size_t alignment = 16;
#else
    // ARM32 NEON: double uses scalar fallback
    static constexpr size_t width = 1; 
    static constexpr size_t alignment = alignof(double);
#endif
};
#endif

// =============================================================================
// SimdMask: Comparison Results for Conditional Logic
// =============================================================================

/**
 * @brief SIMD mask for conditional operations
 * @tparam T Scalar type matching the SimdVector type
 * 
 * Masks are produced by comparison operations and consumed by select().
 * Internally uses architecture-native mask formats for optimal blending.
 * 
 * @note Mask Invariant: Each lane contains either all-ones (true) or all-zeros (false).
 *       This invariant is maintained by comparison operations and logical operators.
 *       The popcount() method relies on this invariant for correctness.
 */
template<typename T>
class SimdMask {
    static_assert(std::is_floating_point_v<T>, "SimdMask only supports float/double");
    
public:
    static constexpr size_t width = SimdTraits<T>::width;
    
#if defined(SIMD_AVX512)
    using storage_type = std::conditional_t<std::is_same_v<T, float>, __mmask16, __mmask8>;
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
struct storage_float { using type = __m256; };
struct storage_double { using type = __m256d; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#elif defined(SIMD_SSE2)
struct storage_float { using type = __m128; };
struct storage_double { using type = __m128d; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#elif defined(SIMD_NEON) && SIMD_NEON_AARCH64
struct storage_float { using type = uint32x4_t; };
struct storage_double { using type = uint64x2_t; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#elif defined(SIMD_NEON)
    // ARM32: float uses NEON, double uses scalar bool
struct storage_float { using type = uint32x4_t; };
struct storage_double { using type = bool; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#else
    using storage_type = std::array<bool, width>;
#endif

    storage_type data_;

    SimdMask() = default;
    explicit SimdMask(storage_type data) noexcept : data_(data) {}

    /**
     * @brief Check if any lane is true
     * @return true if at least one lane contains all-ones
     */
    bool any() const noexcept {
#if defined(SIMD_AVX512)
        return data_ != 0;
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>)
            return _mm256_movemask_ps(data_) != 0;
        else
            return _mm256_movemask_pd(data_) != 0;
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>)
            return _mm_movemask_ps(data_) != 0;
        else
            return _mm_movemask_pd(data_) != 0;
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) {
            // Robust: explicit lane OR for any()
            return (vgetq_lane_u32(data_, 0) |
                    vgetq_lane_u32(data_, 1) |
                    vgetq_lane_u32(data_, 2) |
                    vgetq_lane_u32(data_, 3)) != 0;
        } else {
#if SIMD_NEON_AARCH64
            return (vgetq_lane_u64(data_, 0) | vgetq_lane_u64(data_, 1)) != 0;
#else
            return data_;  // ARM32 double: scalar bool
#endif
        }
#else
        for (size_t i = 0; i < width; ++i) {
            if (data_[i]) return true;
        }
        return false;
#endif
    }

    /**
     * @brief Check if all lanes are true
     * @return true if all lanes contain all-ones
     */
    bool all() const noexcept {
#if defined(SIMD_AVX512)
        constexpr auto all_bits = std::is_same_v<T, float> ? __mmask16(0xFFFF) : __mmask8(0xFF);
        return (data_ & all_bits) == all_bits;
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>)
            return _mm256_movemask_ps(data_) == 0xFF;
        else
            return _mm256_movemask_pd(data_) == 0xF;
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>)
            return _mm_movemask_ps(data_) == 0xF;
        else
            return _mm_movemask_pd(data_) == 0x3;
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) {
            // Robust: explicit lane AND for all()
            return (vgetq_lane_u32(data_, 0) &
                    vgetq_lane_u32(data_, 1) &
                    vgetq_lane_u32(data_, 2) &
                    vgetq_lane_u32(data_, 3)) != 0;
        } else {
#if SIMD_NEON_AARCH64
            return (vgetq_lane_u64(data_, 0) & vgetq_lane_u64(data_, 1)) != 0;
#else
            return data_;  // ARM32 double: scalar bool
#endif
        }
#else
        for (size_t i = 0; i < width; ++i) {
            if (!data_[i]) return false;
        }
        return true;
#endif
    }

    /**
     * @brief Check if no lanes are true
     * @return true if all lanes contain all-zeros
     */
    bool none() const noexcept {
        return !any();
    }
    
    /**
     * @brief Count number of true lanes
     * @return Number of lanes containing all-ones (0 to width)
     * @note Assumes mask invariant: each lane is either all-ones or all-zeros
     */
    size_t popcount() const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>)
            return detail::portable_popcount(static_cast<unsigned>(data_ & 0xFFFF));
        else
            return detail::portable_popcount(static_cast<unsigned>(data_ & 0xFF));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>)
            return detail::portable_popcount(static_cast<unsigned>(_mm256_movemask_ps(data_)));
        else
            return detail::portable_popcount(static_cast<unsigned>(_mm256_movemask_pd(data_)));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>)
            return detail::portable_popcount(static_cast<unsigned>(_mm_movemask_ps(data_)));
        else
            return detail::portable_popcount(static_cast<unsigned>(_mm_movemask_pd(data_)));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) {
            size_t cnt = 0;
            if (vgetq_lane_u32(data_, 0)) ++cnt;
            if (vgetq_lane_u32(data_, 1)) ++cnt;
            if (vgetq_lane_u32(data_, 2)) ++cnt;
            if (vgetq_lane_u32(data_, 3)) ++cnt;
            return cnt;
        } else {
#if SIMD_NEON_AARCH64
            size_t cnt = 0;
            if (vgetq_lane_u64(data_, 0)) ++cnt;
            if (vgetq_lane_u64(data_, 1)) ++cnt;
            return cnt;
#else
            return data_ ? 1 : 0;  // ARM32 double: scalar
#endif
        }
#else
        size_t cnt = 0;
        for (size_t i = 0; i < width; ++i) {
            if (data_[i]) ++cnt;
        }
        return cnt;
#endif
    }

    // Logical operators (preserve mask invariant)
    SimdMask operator&(const SimdMask& other) const noexcept {
#if defined(SIMD_AVX512)
        return SimdMask(data_ & other.data_);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>)
            return SimdMask(_mm256_and_ps(data_, other.data_));
        else
            return SimdMask(_mm256_and_pd(data_, other.data_));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>)
            return SimdMask(_mm_and_ps(data_, other.data_));
        else
            return SimdMask(_mm_and_pd(data_, other.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>)
            return SimdMask(vandq_u32(data_, other.data_));
#if SIMD_NEON_AARCH64
        else
            return SimdMask(vandq_u64(data_, other.data_));
#else
        else
            return SimdMask(data_ && other.data_);
#endif
#else
        storage_type res;
        for (size_t i = 0; i < width; ++i) res[i] = data_[i] && other.data_[i];
        return SimdMask(res);
#endif
    }

    SimdMask operator|(const SimdMask& other) const noexcept {
#if defined(SIMD_AVX512)
        return SimdMask(data_ | other.data_);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>)
            return SimdMask(_mm256_or_ps(data_, other.data_));
        else
            return SimdMask(_mm256_or_pd(data_, other.data_));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>)
            return SimdMask(_mm_or_ps(data_, other.data_));
        else
            return SimdMask(_mm_or_pd(data_, other.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>)
            return SimdMask(vorrq_u32(data_, other.data_));
#if SIMD_NEON_AARCH64
        else
            return SimdMask(vorrq_u64(data_, other.data_));
#else
        else
            return SimdMask(data_ || other.data_);
#endif
#else
        storage_type res;
        for (size_t i = 0; i < width; ++i) res[i] = data_[i] || other.data_[i];
        return SimdMask(res);
#endif
    }

    SimdMask operator~() const noexcept {
#if defined(SIMD_AVX512)
        return SimdMask(~data_);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) {
            __m256 all_ones = _mm256_castsi256_ps(_mm256_set1_epi32(-1));
            return SimdMask(_mm256_xor_ps(data_, all_ones));
        } else {
            __m256d all_ones = _mm256_castsi256_pd(_mm256_set1_epi64x(-1));
            return SimdMask(_mm256_xor_pd(data_, all_ones));
        }
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) {
            __m128 all_ones = _mm_castsi128_ps(_mm_set1_epi32(-1));
            return SimdMask(_mm_xor_ps(data_, all_ones));
        } else {
            __m128d all_ones = _mm_castsi128_pd(_mm_set1_epi64x(-1));
            return SimdMask(_mm_xor_pd(data_, all_ones));
        }
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>)
            return SimdMask(vmvnq_u32(data_));
#if SIMD_NEON_AARCH64
        // Note: vmvnq_s64 does not exist in NEON; use XOR with all-ones instead
        else
            return SimdMask(veorq_u64(data_, vdupq_n_u64(~0ULL)));
#else
        else
            return SimdMask(!data_);
#endif
#else
        storage_type res;
        for (size_t i = 0; i < width; ++i) res[i] = !data_[i];
        return SimdMask(res);
#endif
    }
};

// =============================================================================
// SimdVector
// =============================================================================

/**
 * @brief SIMD vector wrapper with architecture-specific implementations
 * @tparam T Scalar type (float or double only)
 * 
 * @note Integer types are NOT supported. The static_assert below will trigger
 *       a compile-time error if you attempt SimdVector<int>. For checked integer
 *       arithmetic, use CheckedArithmetic.h which has native integer SIMD paths.
 */
template<typename T>
class SimdVector {
    static_assert(std::is_floating_point_v<T>,
        "SimdVector<T> only supports float and double. "
        "For integer SIMD, use CheckedArithmetic.h's native integer paths.");

public:
    static constexpr size_t width = SimdTraits<T>::width;
    static constexpr size_t alignment = SimdTraits<T>::alignment;
    using value_type = T;
    using mask_type = SimdMask<T>;
    
#if defined(SIMD_AVX512)
struct storage_float { using type = __m512; };
struct storage_double { using type = __m512d; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
struct storage_float { using type = __m256; };
struct storage_double { using type = __m256d; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#elif defined(SIMD_SSE2)
struct storage_float { using type = __m128; };
struct storage_double { using type = __m128d; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#elif defined(SIMD_NEON) && SIMD_NEON_AARCH64
struct storage_float { using type = float32x4_t; };
struct storage_double { using type = float64x2_t; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#elif defined(SIMD_NEON)
    // ARM32: float uses NEON, double uses scalar
struct storage_float { using type = float32x4_t; };
struct storage_double { using type = double; };
using storage_type = typename std::conditional_t<std::is_same_v<T, float>, storage_float, storage_double>::type;
#else
    using storage_type = std::array<T, width>;
#endif
    
private:
    storage_type data_;
    
    static bool is_aligned(const void* ptr) noexcept {
        return (reinterpret_cast<std::uintptr_t>(ptr) % alignment) == 0;
    }
    
public:
    SimdVector() = default;
    explicit SimdVector(storage_type d) noexcept : data_(d) {}
    
    /**
     * @brief Broadcast scalar to all lanes
     */
    explicit SimdVector(T scalar) noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) data_ = _mm512_set1_ps(scalar);
        else data_ = _mm512_set1_pd(scalar);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) data_ = _mm256_set1_ps(scalar);
        else data_ = _mm256_set1_pd(scalar);
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) data_ = _mm_set1_ps(scalar);
        else data_ = _mm_set1_pd(scalar);
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) data_ = vdupq_n_f32(scalar);
#if SIMD_NEON_AARCH64
        else data_ = vdupq_n_f64(scalar);
#else
        else data_ = scalar;  // ARM32 double: scalar
#endif
#else
        data_.fill(scalar);
#endif
    }
    
    // =========================================================================
    // Factory Methods
    // =========================================================================
    
    static SimdVector zero() noexcept { return SimdVector(T{0}); }
    static SimdVector ones() noexcept { return SimdVector(T{1}); }
    
    static SimdVector infinity() noexcept { 
        return SimdVector(std::numeric_limits<T>::infinity()); 
    }
    
    static SimdVector neg_infinity() noexcept { 
        return SimdVector(-std::numeric_limits<T>::infinity()); 
    }
    
    // =========================================================================
    // Loads & Stores
    // =========================================================================
    
    /**
     * @brief Load from aligned memory
     * @param ptr Pointer aligned to SimdVector::alignment bytes
     * @note Uses enforce() for debug-mode alignment check (Fat-P consistency)
     */
    static SimdVector load_aligned(const T* ptr) {
        enforce(is_aligned(ptr), "SimdVector::load_aligned: misaligned pointer, required alignment=", alignment);
        
        SimdVector result;
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) result.data_ = _mm512_load_ps(ptr);
        else result.data_ = _mm512_load_pd(ptr);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) result.data_ = _mm256_load_ps(ptr);
        else result.data_ = _mm256_load_pd(ptr);
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) result.data_ = _mm_load_ps(ptr);
        else result.data_ = _mm_load_pd(ptr);
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) result.data_ = vld1q_f32(ptr);
#if SIMD_NEON_AARCH64
        else result.data_ = vld1q_f64(ptr);
#else
        else result.data_ = *ptr;
#endif
#else
        std::memcpy(result.data_.data(), ptr, width * sizeof(T));
#endif
        return result;
    }
    
    /**
     * @brief Load from unaligned memory (no alignment requirement)
     */
    static SimdVector load_unaligned(const T* ptr) noexcept {
        SimdVector result;
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) result.data_ = _mm512_loadu_ps(ptr);
        else result.data_ = _mm512_loadu_pd(ptr);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) result.data_ = _mm256_loadu_ps(ptr);
        else result.data_ = _mm256_loadu_pd(ptr);
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) result.data_ = _mm_loadu_ps(ptr);
        else result.data_ = _mm_loadu_pd(ptr);
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) result.data_ = vld1q_f32(ptr);
#if SIMD_NEON_AARCH64
        else result.data_ = vld1q_f64(ptr);
#else
        else result.data_ = *ptr;
#endif
#else
        std::memcpy(result.data_.data(), ptr, width * sizeof(T));
#endif
        return result;
    }
    
    /**
     * @brief Load partial vector (for tail processing)
     * @param ptr Source pointer
     * @param count Number of elements to load (remaining lanes zeroed)
     * @note Uses MSVC-safe aligned buffer for portability
     */
    static SimdVector load_partial(const T* ptr, size_t count) noexcept {
        if (count >= width) return load_unaligned(ptr);
        
#if defined(SIMD_AVX512)
        // Use masked load for efficiency on AVX-512
        if constexpr (std::is_same_v<T, float>) {
            __mmask16 mask = static_cast<__mmask16>((1u << count) - 1);
            return SimdVector(_mm512_maskz_loadu_ps(mask, ptr));
        } else {
            __mmask8 mask = static_cast<__mmask8>((1u << count) - 1);
            return SimdVector(_mm512_maskz_loadu_pd(mask, ptr));
        }
#else
        // MSVC-safe: use properly aligned buffer
        detail::PartialBuffer<T, width, alignment> buf;
        for (size_t i = 0; i < count; ++i) buf[i] = ptr[i];
        return load_aligned(buf.ptr());
#endif
    }
    
    /**
     * @brief Store to aligned memory
     * @param ptr Pointer aligned to SimdVector::alignment bytes
     */
    void store_aligned(T* ptr) const {
        enforce(is_aligned(ptr), "SimdVector::store_aligned: misaligned pointer, required alignment=", alignment);
        
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) _mm512_store_ps(ptr, data_);
        else _mm512_store_pd(ptr, data_);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) _mm256_store_ps(ptr, data_);
        else _mm256_store_pd(ptr, data_);
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) _mm_store_ps(ptr, data_);
        else _mm_store_pd(ptr, data_);
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) vst1q_f32(ptr, data_);
#if SIMD_NEON_AARCH64
        else vst1q_f64(ptr, data_);
#else
        else *ptr = data_;
#endif
#else
        std::memcpy(ptr, data_.data(), width * sizeof(T));
#endif
    }

    /**
     * @brief Store to unaligned memory
     */
    void store_unaligned(T* ptr) const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) _mm512_storeu_ps(ptr, data_);
        else _mm512_storeu_pd(ptr, data_);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) _mm256_storeu_ps(ptr, data_);
        else _mm256_storeu_pd(ptr, data_);
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) _mm_storeu_ps(ptr, data_);
        else _mm_storeu_pd(ptr, data_);
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) vst1q_f32(ptr, data_);
#if SIMD_NEON_AARCH64
        else vst1q_f64(ptr, data_);
#else
        else *ptr = data_;
#endif
#else
        std::memcpy(ptr, data_.data(), width * sizeof(T));
#endif
    }
    
    /**
     * @brief Store partial vector (for tail processing)
     * @param ptr Destination pointer
     * @param count Number of elements to store
     * @note Uses MSVC-safe aligned buffer for portability
     */
    void store_partial(T* ptr, size_t count) const noexcept {
        if (count >= width) { store_unaligned(ptr); return; }
        
#if defined(SIMD_AVX512)
        // Use masked store for efficiency on AVX-512
        if constexpr (std::is_same_v<T, float>) {
            __mmask16 mask = static_cast<__mmask16>((1u << count) - 1);
            _mm512_mask_storeu_ps(ptr, mask, data_);
        } else {
            __mmask8 mask = static_cast<__mmask8>((1u << count) - 1);
            _mm512_mask_storeu_pd(ptr, mask, data_);
        }
#else
        // MSVC-safe: use properly aligned buffer
        detail::PartialBuffer<T, width, alignment> buf;
        store_aligned(buf.ptr());
        for (size_t i = 0; i < count; ++i) ptr[i] = buf[i];
#endif
    }

    // =========================================================================
    // Arithmetic Operators
    // =========================================================================

    SimdVector operator+(const SimdVector& rhs) const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_add_ps(data_, rhs.data_));
        else return SimdVector(_mm512_add_pd(data_, rhs.data_));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_add_ps(data_, rhs.data_));
        else return SimdVector(_mm256_add_pd(data_, rhs.data_));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm_add_ps(data_, rhs.data_));
        else return SimdVector(_mm_add_pd(data_, rhs.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdVector(vaddq_f32(data_, rhs.data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vaddq_f64(data_, rhs.data_));
#else
        else return SimdVector(data_ + rhs.data_);
#endif
#else
        SimdVector r; for(size_t i=0; i<width; ++i) r.data_[i] = data_[i] + rhs.data_[i]; return r;
#endif
    }
    
    SimdVector operator-() const noexcept { return zero() - *this; }

    SimdVector operator-(const SimdVector& rhs) const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_sub_ps(data_, rhs.data_));
        else return SimdVector(_mm512_sub_pd(data_, rhs.data_));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_sub_ps(data_, rhs.data_));
        else return SimdVector(_mm256_sub_pd(data_, rhs.data_));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm_sub_ps(data_, rhs.data_));
        else return SimdVector(_mm_sub_pd(data_, rhs.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdVector(vsubq_f32(data_, rhs.data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vsubq_f64(data_, rhs.data_));
#else
        else return SimdVector(data_ - rhs.data_);
#endif
#else
        SimdVector r; for(size_t i=0; i<width; ++i) r.data_[i] = data_[i] - rhs.data_[i]; return r;
#endif
    }

    SimdVector operator*(const SimdVector& rhs) const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_mul_ps(data_, rhs.data_));
        else return SimdVector(_mm512_mul_pd(data_, rhs.data_));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_mul_ps(data_, rhs.data_));
        else return SimdVector(_mm256_mul_pd(data_, rhs.data_));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm_mul_ps(data_, rhs.data_));
        else return SimdVector(_mm_mul_pd(data_, rhs.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdVector(vmulq_f32(data_, rhs.data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vmulq_f64(data_, rhs.data_));
#else
        else return SimdVector(data_ * rhs.data_);
#endif
#else
        SimdVector r; for(size_t i=0; i<width; ++i) r.data_[i] = data_[i] * rhs.data_[i]; return r;
#endif
    }

    SimdVector operator/(const SimdVector& rhs) const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_div_ps(data_, rhs.data_));
        else return SimdVector(_mm512_div_pd(data_, rhs.data_));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_div_ps(data_, rhs.data_));
        else return SimdVector(_mm256_div_pd(data_, rhs.data_));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm_div_ps(data_, rhs.data_));
        else return SimdVector(_mm_div_pd(data_, rhs.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdVector(vdivq_f32(data_, rhs.data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vdivq_f64(data_, rhs.data_));
#else
        else return SimdVector(data_ / rhs.data_);
#endif
#else
        SimdVector r; for(size_t i=0; i<width; ++i) r.data_[i] = data_[i] / rhs.data_[i]; return r;
#endif
    }

    // Compound operators
    SimdVector& operator+=(const SimdVector& r) noexcept { *this = *this + r; return *this; }
    SimdVector& operator-=(const SimdVector& r) noexcept { *this = *this - r; return *this; }
    SimdVector& operator*=(const SimdVector& r) noexcept { *this = *this * r; return *this; }
    SimdVector& operator/=(const SimdVector& r) noexcept { *this = *this / r; return *this; }

    // =========================================================================
    // Comparisons (return SimdMask)
    // =========================================================================

    SimdMask<T> operator==(const SimdVector& rhs) const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(_mm512_cmp_ps_mask(data_, rhs.data_, _CMP_EQ_OQ));
        else return SimdMask<T>(_mm512_cmp_pd_mask(data_, rhs.data_, _CMP_EQ_OQ));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(_mm256_cmp_ps(data_, rhs.data_, _CMP_EQ_OQ));
        else return SimdMask<T>(_mm256_cmp_pd(data_, rhs.data_, _CMP_EQ_OQ));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(_mm_cmpeq_ps(data_, rhs.data_));
        else return SimdMask<T>(_mm_cmpeq_pd(data_, rhs.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(vceqq_f32(data_, rhs.data_));
#if SIMD_NEON_AARCH64
        else return SimdMask<T>(vceqq_f64(data_, rhs.data_));
#else
        else return SimdMask<T>(data_ == rhs.data_);
#endif
#else
        typename SimdMask<T>::storage_type m; for(size_t i=0; i<width; ++i) m[i] = data_[i] == rhs.data_[i]; return SimdMask<T>(m);
#endif
    }

    SimdMask<T> operator!=(const SimdVector& rhs) const noexcept { return ~(*this == rhs); }
    
    SimdMask<T> operator>(const SimdVector& rhs) const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(_mm512_cmp_ps_mask(data_, rhs.data_, _CMP_GT_OQ));
        else return SimdMask<T>(_mm512_cmp_pd_mask(data_, rhs.data_, _CMP_GT_OQ));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(_mm256_cmp_ps(data_, rhs.data_, _CMP_GT_OQ));
        else return SimdMask<T>(_mm256_cmp_pd(data_, rhs.data_, _CMP_GT_OQ));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(_mm_cmpgt_ps(data_, rhs.data_));
        else return SimdMask<T>(_mm_cmpgt_pd(data_, rhs.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdMask<T>(vcgtq_f32(data_, rhs.data_));
#if SIMD_NEON_AARCH64
        else return SimdMask<T>(vcgtq_f64(data_, rhs.data_));
#else
        else return SimdMask<T>(data_ > rhs.data_);
#endif
#else
        typename SimdMask<T>::storage_type m; for(size_t i=0; i<width; ++i) m[i] = data_[i] > rhs.data_[i]; return SimdMask<T>(m);
#endif
    }
    
    SimdMask<T> operator<(const SimdVector& rhs) const noexcept { return rhs > *this; }
    SimdMask<T> operator>=(const SimdVector& rhs) const noexcept { return ~(*this < rhs); }
    SimdMask<T> operator<=(const SimdVector& rhs) const noexcept { return ~(*this > rhs); }

    // =========================================================================
    // Select / Blend
    // =========================================================================

    /**
     * @brief Select elements based on mask: mask ? if_true : if_false
     * @note Uses blendv-style selection. Mask must satisfy the mask invariant
     *       (each lane is all-ones or all-zeros from comparison operations).
     */
    static SimdVector select(const SimdMask<T>& mask, const SimdVector& if_true, const SimdVector& if_false) noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_mask_blend_ps(mask.data_, if_false.data_, if_true.data_));
        else return SimdVector(_mm512_mask_blend_pd(mask.data_, if_false.data_, if_true.data_));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_blendv_ps(if_false.data_, if_true.data_, mask.data_));
        else return SimdVector(_mm256_blendv_pd(if_false.data_, if_true.data_, mask.data_));
#elif defined(SIMD_SSE4_1)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm_blendv_ps(if_false.data_, if_true.data_, mask.data_));
        else return SimdVector(_mm_blendv_pd(if_false.data_, if_true.data_, mask.data_));
#elif defined(SIMD_SSE2)
        // SSE2 manual blend: (true & mask) | (false & ~mask)
        if constexpr (std::is_same_v<T, float>) 
            return SimdVector(_mm_or_ps(_mm_and_ps(mask.data_, if_true.data_), _mm_andnot_ps(mask.data_, if_false.data_)));
        else 
            return SimdVector(_mm_or_pd(_mm_and_pd(mask.data_, if_true.data_), _mm_andnot_pd(mask.data_, if_false.data_)));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdVector(vbslq_f32(mask.data_, if_true.data_, if_false.data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vbslq_f64(mask.data_, if_true.data_, if_false.data_));
#else
        else return mask.data_ ? if_true : if_false;
#endif
#else
        SimdVector r; 
        for(size_t i=0; i<width; ++i) r.data_[i] = mask.data_[i] ? if_true.data_[i] : if_false.data_[i]; 
        return r;
#endif
    }

    // =========================================================================
    // FMA & Math
    // =========================================================================

    /**
     * @brief Fused multiply-add: a * b + c (single rounding)
     */
    static SimdVector fma(const SimdVector& a, const SimdVector& b, const SimdVector& c) noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_fmadd_ps(a.data_, b.data_, c.data_));
        else return SimdVector(_mm512_fmadd_pd(a.data_, b.data_, c.data_));
#elif defined(SIMD_AVX2) && defined(__FMA__)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_fmadd_ps(a.data_, b.data_, c.data_));
        else return SimdVector(_mm256_fmadd_pd(a.data_, b.data_, c.data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdVector(vfmaq_f32(c.data_, a.data_, b.data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vfmaq_f64(c.data_, a.data_, b.data_));
#else
        else return SimdVector(a.data_ * b.data_ + c.data_);
#endif
#else
        return a * b + c;
#endif
    }
    
    /**
     * @brief Fused multiply-subtract: a * b - c
     * 
     * @note NEON implementation: vfmsq computes (first_arg - second*third),
     *       so we use vfmaq with negated c: -c + a*b = a*b - c
     */
    static SimdVector fms(const SimdVector& a, const SimdVector& b, const SimdVector& c) noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_fmsub_ps(a.data_, b.data_, c.data_));
        else return SimdVector(_mm512_fmsub_pd(a.data_, b.data_, c.data_));
#elif defined(SIMD_AVX2) && defined(__FMA__)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_fmsub_ps(a.data_, b.data_, c.data_));
        else return SimdVector(_mm256_fmsub_pd(a.data_, b.data_, c.data_));
#elif defined(SIMD_NEON)
        // vfmsq_f32(a,b,c) = a - b*c, but we want a*b - c
        // Use vfmaq with negated c: vfmaq(-c, a, b) = -c + a*b = a*b - c
        if constexpr (std::is_same_v<T, float>) return SimdVector(vfmaq_f32(vnegq_f32(c.data_), a.data_, b.data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vfmaq_f64(vnegq_f64(c.data_), a.data_, b.data_));
#else
        else return SimdVector(a.data_ * b.data_ - c.data_);
#endif
#else
        return a * b - c;
#endif
    }

    SimdVector sqrt() const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm512_sqrt_ps(data_));
        else return SimdVector(_mm512_sqrt_pd(data_));
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm256_sqrt_ps(data_));
        else return SimdVector(_mm256_sqrt_pd(data_));
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) return SimdVector(_mm_sqrt_ps(data_));
        else return SimdVector(_mm_sqrt_pd(data_));
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return SimdVector(vsqrtq_f32(data_));
#if SIMD_NEON_AARCH64
        else return SimdVector(vsqrtq_f64(data_));
#else
        else return SimdVector(std::sqrt(data_));
#endif
#else
        SimdVector r; for(size_t i=0; i<width; ++i) r.data_[i] = std::sqrt(data_[i]); return r;
#endif
    }

    SimdVector abs() const noexcept { return select(*this >= zero(), *this, -*this); }
    SimdVector max(const SimdVector& other) const noexcept { return select(*this > other, *this, other); }
    SimdVector min(const SimdVector& other) const noexcept { return select(*this < other, *this, other); }
    SimdVector clamp(const SimdVector& lo, const SimdVector& hi) const noexcept { return this->max(lo).min(hi); }

    // =========================================================================
    // Horizontal Operations
    // =========================================================================

    T horizontal_sum() const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return _mm512_reduce_add_ps(data_);
        else return _mm512_reduce_add_pd(data_);
#elif defined(SIMD_AVX) || defined(SIMD_AVX2)
        if constexpr (std::is_same_v<T, float>) {
            __m128 lo = _mm256_castps256_ps128(data_);
            __m128 hi = _mm256_extractf128_ps(data_, 1);
            __m128 s = _mm_add_ps(lo, hi);
            // SSE2-compatible horizontal add
            __m128 sh = _mm_shuffle_ps(s, s, _MM_SHUFFLE(2, 3, 0, 1));
            s = _mm_add_ps(s, sh);
            sh = _mm_shuffle_ps(s, s, _MM_SHUFFLE(1, 0, 3, 2));
            return _mm_cvtss_f32(_mm_add_ss(s, sh));
        } else {
            __m128d lo = _mm256_castpd256_pd128(data_);
            __m128d hi = _mm256_extractf128_pd(data_, 1);
            __m128d s = _mm_add_pd(lo, hi);
            return _mm_cvtsd_f64(_mm_add_sd(s, _mm_unpackhi_pd(s, s)));
        }
#elif defined(SIMD_SSE2)
        if constexpr (std::is_same_v<T, float>) {
            // SSE2-compatible: use shuffles instead of SSE3 hadd
            __m128 sh = _mm_shuffle_ps(data_, data_, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 s = _mm_add_ps(data_, sh);
            sh = _mm_shuffle_ps(s, s, _MM_SHUFFLE(1, 0, 3, 2));
            return _mm_cvtss_f32(_mm_add_ps(s, sh));
        } else {
            __m128d sh = _mm_shuffle_pd(data_, data_, 1);
            return _mm_cvtsd_f64(_mm_add_pd(data_, sh));
        }
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return vaddvq_f32(data_);
#if SIMD_NEON_AARCH64
        else return vaddvq_f64(data_);
#else
        else return data_;
#endif
#else
        T sum = 0; for(size_t i=0; i<width; ++i) sum += data_[i]; return sum;
#endif
    }

    T horizontal_max() const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return _mm512_reduce_max_ps(data_);
        else return _mm512_reduce_max_pd(data_);
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return vmaxvq_f32(data_);
#if SIMD_NEON_AARCH64
        else return vmaxvq_f64(data_);
#else
        else return data_;
#endif
#else
        detail::PartialBuffer<T, width, alignment> temp;
        store_aligned(temp.ptr());
        return *std::max_element(temp.ptr(), temp.ptr() + width);
#endif
    }

    T horizontal_min() const noexcept {
#if defined(SIMD_AVX512)
        if constexpr (std::is_same_v<T, float>) return _mm512_reduce_min_ps(data_);
        else return _mm512_reduce_min_pd(data_);
#elif defined(SIMD_NEON)
        if constexpr (std::is_same_v<T, float>) return vminvq_f32(data_);
#if SIMD_NEON_AARCH64
        else return vminvq_f64(data_);
#else
        else return data_;
#endif
#else
        detail::PartialBuffer<T, width, alignment> temp;
        store_aligned(temp.ptr());
        return *std::min_element(temp.ptr(), temp.ptr() + width);
#endif
    }
    
    // =========================================================================
    // Special Value Checks (for CheckedArithmetic integration)
    // =========================================================================
    
    /**
     * @brief Check if any lane contains NaN
     * @note IEEE-754: NaN != NaN is always true
     */
    bool has_nan() const noexcept {
        return (*this != *this).any();
    }
    
    /**
     * @brief Check if any lane contains infinity (+ or -)
     */
    bool has_inf() const noexcept {
        auto pos_inf = infinity();
        auto neg_inf = neg_infinity();
        return ((*this == pos_inf) | (*this == neg_inf)).any();
    }
    
    /**
     * @brief Check if all lanes are finite (not NaN, not Inf)
     */
    bool all_finite() const noexcept {
        return !has_nan() && !has_inf();
    }
};

// Aliases
using SimdVectorF = SimdVector<float>;
using SimdVectorD = SimdVector<double>;

template <typename T>
struct is_simd_vector<SimdVector<T>> : std::true_type {};

} // namespace fat_p
