/**
 * @file CheckedArithmetic.h
 * @brief Provides checked arithmetic operations for integers and floating-
 * point types, with policy-based error handling and extensibility.
 *
 * @details This module extends basic integer operations (addition,
 * subtraction, multiplication) to include division, floating-point support,
 * and vectorized variants. Policies allow customizable error handling, such
 * as throwing exceptions (default) or returning fat_p::Expected<T, MathError> for
 * non-throwing contexts. Compile-time variants use static_assert for
 * constant expressions. The design emphasizes zero-overhead where possible,
 * with portable fallbacks and optional builtins for performance.
 *
 * Key features:
 * - Checked division with zero and overflow handling.
 * - Policy-based returns (e.g., Expected for noexcept safety).
 * - Floating-point ops with policies for overflow/NaN/Inf.
 * - Vectorized ops with SIMD optimization (AVX2 for int32/double).
 * - Compile-time variants for constant expressions.
 * - Bitwise ops (and/or/xor/shift) with UB checks.
 * - noexcept specifications for non-throwing policies.
 * - C++20 concepts support (conditional).
 *
 * @note Supports signed and unsigned integrals. For unsigned types, overflow
 *       checks focus on wraparound patterns (e.g., result < operand).
 *
 * @comparison Similar to Boost.SafeNumerics but with:
 *   - Vector/SIMD extensions
 *   - Lighter weight (header-only, no Boost dependencies)
 *   - Full floating-point support
 *   - Explicit policy selection vs. automatic promotion
 *
 * @performance
 * - Builtin path (GCC/Clang): ~2-5 ns per operation
 * - Fallback path: ~10-20 ns per operation
 * - SIMD vectors (int32/double): ~0.5-1 ns per element (AVX2)
 *
 * All operations enforce integral/floating-point constraints via SFINAE/Concepts.
 */
#pragma once

#include "CppStandardDetection.h"

#include <type_traits>
#include <limits>
#include <vector>
#include <cmath>
#include <cstdint>
#include "enforce.h"
#include "Expected.h"

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#ifdef __AVX2__
constexpr bool has_avx2 = true;
#else
constexpr bool has_avx2 = false;
#endif

namespace fat_p {

constexpr size_t AVX2_INT32_PER_REG = 8;
constexpr size_t AVX2_DOUBLES_PER_REG = 4;

enum class MathError {
    Overflow,
    Underflow,
    DivByZero,
    NaN,
    Inf,
    InvalidArgument
};

inline std::ostream& operator<<(std::ostream& os, MathError err)
{
    switch (err)
    {
    case MathError::Overflow:        return os << "Overflow";
    case MathError::Underflow:       return os << "Underflow";
    case MathError::DivByZero:       return os << "DivByZero";
    case MathError::NaN:             return os << "NaN";
    case MathError::Inf:             return os << "Inf";
    case MathError::InvalidArgument: return os << "InvalidArgument";
    default:                         return os << "Unknown";
    }
}

struct ThrowOnErrorPolicy {};

struct ReturnExpectedPolicy {};

struct SaturatingPolicy {};

struct InfTolerantPolicy {};

#if FATP_HAS_CPP20
template <typename T>
concept IntegralNonBool = std::integral<T> && !std::same_as<T, bool>;

template <typename T>
concept FloatingPoint = std::floating_point<T>;

template <typename T>
concept Arithmetic = IntegralNonBool<T> || FloatingPoint<T>;

#define ENABLE_IF_INTEGRAL IntegralNonBool T
#define ENABLE_IF_FLOATING FloatingPoint T
#define ENABLE_IF_ARITHMETIC Arithmetic T
#else
template <typename T>
using EnableIfIntegral = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, T>;

template <typename T>
using EnableIfFloating = std::enable_if_t<std::is_floating_point_v<T>, T>;

#define ENABLE_IF_INTEGRAL typename T, typename = EnableIfIntegral<T>
#define ENABLE_IF_FLOATING typename T, typename = EnableIfFloating<T>
#define ENABLE_IF_ARITHMETIC typename T
#endif

template <typename Policy, typename R>
using PolicyReturnType = std::conditional_t<
    std::is_same_v<Policy, ReturnExpectedPolicy>,
    fat_p::Expected<R, fat_p::MathError>,
    R>;

template <typename Policy>
struct PolicyTraits
{
    template <typename T>
    static constexpr bool is_noexcept = 
        std::is_same_v<Policy, ReturnExpectedPolicy> || 
        std::is_same_v<Policy, SaturatingPolicy> ||
        std::is_same_v<Policy, InfTolerantPolicy>;
};

#if defined(__has_builtin)
    #if __has_builtin(__builtin_add_overflow) && \
        __has_builtin(__builtin_sub_overflow) && \
        __has_builtin(__builtin_mul_overflow)
        #define HAS_BUILTIN_OVERFLOW 1
    #else
        #define HAS_BUILTIN_OVERFLOW 0
    #endif
#elif defined(__GNUC__) && (__GNUC__ >= 5)
    #define HAS_BUILTIN_OVERFLOW 1
#else
    #define HAS_BUILTIN_OVERFLOW 0
#endif

#define VALIDATE_FP_INPUTS(a, b, op_name)                                              \
    do {                                                                               \
        if (std::isnan(a) || std::isnan(b)) {                                          \
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>) {                \
                always_enforce(false, "FP input contains NaN:", a, op_name, b);        \
            } else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>) {       \
                return Expected<T, MathError>(unexpect, MathError::NaN);               \
            } else if constexpr (std::is_same_v<Policy, SaturatingPolicy>) {           \
                return std::numeric_limits<T>::quiet_NaN();                            \
            } else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>) {          \
                always_enforce(false, "FP input contains NaN:", a, op_name, b);        \
            }                                                                          \
        }                                                                              \
        if (std::isinf(a) && std::isinf(b)) {                                          \
            constexpr bool is_subtraction = (op_name[0] == '-');                       \
            bool same_sign = (a > 0) == (b > 0);                                       \
            if ((!same_sign && op_name[0] == '+') ||                                   \
                (same_sign && is_subtraction)) {                                       \
                if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>) {            \
                    always_enforce(false, "FP Inf-Inf undefined:", a, op_name, b);     \
                } else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>) {   \
                    return Expected<T, MathError>(unexpect, MathError::NaN);           \
                } else if constexpr (std::is_same_v<Policy, SaturatingPolicy>) {       \
                    return std::numeric_limits<T>::quiet_NaN();                        \
                } else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>) {      \
                    always_enforce(false, "FP Inf-Inf undefined:", a, op_name, b);     \
                }                                                                      \
            }                                                                          \
        }                                                                              \
    } while(0)

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_add(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
#if HAS_BUILTIN_OVERFLOW
    T result;
    if (__builtin_add_overflow(a, b, &result))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Addition overflow:", a, "+", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b > 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b > 0) ? std::numeric_limits<T>::max() : 
                                 std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
#else
    T result = a + b;
    bool overflow;
    
    if constexpr (std::is_signed_v<T>)
    {
        overflow = (b > 0 && a > std::numeric_limits<T>::max() - b) ||
                   (b < 0 && a < std::numeric_limits<T>::min() - b);
    }
    else
    {
        // Standard idiom for unsigned overflow detection. Unsigned wraparound is
        // well-defined in C++ (modulo 2^N), making this check reliable and portable.
        overflow = (result < a);
    }
    
    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Addition overflow:", a, "+", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b > 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b > 0) ? std::numeric_limits<T>::max() : 
                                 std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
#endif
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_sub(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
#if HAS_BUILTIN_OVERFLOW
    T result;
    if (__builtin_sub_overflow(a, b, &result))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Subtraction overflow:", a, "-", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b < 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Underflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b < 0) ? std::numeric_limits<T>::max() : 
                                 std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::lowest();
            }
        }
    }
#else
    T result = a - b;
    bool overflow;
    
    if constexpr (std::is_signed_v<T>)
    {
        overflow = (b < 0 && a > std::numeric_limits<T>::max() + b) ||
                   (b > 0 && a < std::numeric_limits<T>::min() + b);
    }
    else
    {
        overflow = (result > a);
    }
    
    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Subtraction overflow:", a, "-", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                MathError code = (b < 0) ? MathError::Overflow : MathError::Underflow;
                return Expected<T, MathError>(unexpect, code);
            }
            else
            {
                return Expected<T, MathError>(unexpect, MathError::Underflow);
            }
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (b < 0) ? std::numeric_limits<T>::max() : 
                                 std::numeric_limits<T>::lowest();
            }
            else
            {
                return std::numeric_limits<T>::lowest();
            }
        }
    }
#endif
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_mul(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
#if HAS_BUILTIN_OVERFLOW
    T result;
    if (__builtin_mul_overflow(a, b, &result))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Multiplication overflow:", a, "*", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::Overflow);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                bool negative = (a < 0) != (b < 0);
                return negative ? std::numeric_limits<T>::lowest() : 
                                  std::numeric_limits<T>::max();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
#else
    if (a == 0 || b == 0)
    {
        return T{0};
    }
    
    T result = a * b;
    bool overflow;
    
    if constexpr (std::is_signed_v<T>)
    {
        overflow = (a > 0 && b > 0 && a > std::numeric_limits<T>::max() / b) ||
                   (a < 0 && b < 0 && a < std::numeric_limits<T>::max() / b) ||
                   (a > 0 && b < 0 && b < std::numeric_limits<T>::min() / a) ||
                   (a < 0 && b > 0 && a < std::numeric_limits<T>::min() / b);
    }
    else
    {
        overflow = (b != 0 && a > std::numeric_limits<T>::max() / b);
    }
    
    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Multiplication overflow:", a, "*", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::Overflow);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                bool negative = (a < 0) != (b < 0);
                return negative ? std::numeric_limits<T>::lowest() : 
                                  std::numeric_limits<T>::max();
            }
            else
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
#endif
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_div(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (b == 0)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Division by zero:", a, "/", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            // When a == 0, division by zero saturates to 0 (numerator dominates)
            if (a == 0) return T{0};
            // Otherwise saturate based on sign of numerator
            return (a > 0) ? std::numeric_limits<T>::max() : 
                             std::numeric_limits<T>::lowest();
        }
    }
    
    if constexpr (std::is_signed_v<T>)
    {
        bool overflow = (a == std::numeric_limits<T>::min() && b == -1);
        if (overflow)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "Overflow in division (min/-1):", a, "/", b);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
    
    T result = a / b;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_mod(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (b == 0)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Modulo by zero:", a, "%", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return T{0};
        }
    }
    
    if constexpr (std::is_signed_v<T>)
    {
        bool overflow = (a == std::numeric_limits<T>::min() && b == -1);
        if (overflow)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "Overflow in mod (min%-1):", a, "%", b);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
            {
                return T{0};
            }
        }
    }
    
    T result = a % b;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_add_fp(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    VALIDATE_FP_INPUTS(a, b, "+");
    
    T result = a + b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);
    
    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            always_enforce(false, "Addition error (NaN):", a, "+", b);
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Addition error (NaN/Inf):", a, "+", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() :
                                  std::numeric_limits<T>::lowest();
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_sub_fp(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    VALIDATE_FP_INPUTS(a, b, "-");
    
    T result = a - b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);
    
    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            always_enforce(false, "Subtraction error (NaN):", a, "-", b);
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Subtraction error (NaN/Inf):", a, "-", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() :
                                  std::numeric_limits<T>::lowest();
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_mul_fp(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    VALIDATE_FP_INPUTS(a, b, "*");
    
    T result = a * b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);
    
    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            always_enforce(false, "Multiplication error (NaN):", a, "*", b);
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Multiplication error (NaN/Inf):", a, "*", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() :
                                  std::numeric_limits<T>::lowest();
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_div_fp(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    VALIDATE_FP_INPUTS(a, b, "/");
    
    if (b == T{0})
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Division by zero in div_fp:", a, "/", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            // When a == 0, division by zero saturates to 0 (consistent with integer behavior)
            if (a == T{0}) return T{0};
            // Otherwise saturate based on sign of numerator
            return (a > T{0}) ? std::numeric_limits<T>::max() :
                                std::numeric_limits<T>::lowest();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            // 0/0 is mathematically undefined - return NaN
            if (a == T{0})
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            // Non-zero / 0 returns appropriately signed infinity
            return (a > T{0}) ? std::numeric_limits<T>::infinity() :
                              -std::numeric_limits<T>::infinity();
        }
    }
    
    T result = a / b;
    bool is_nan = std::isnan(result);
    bool is_inf = std::isinf(result);
    
    if (is_nan || (is_inf && std::isfinite(a) && std::isfinite(b)))
    {
        MathError code = is_nan ? MathError::NaN : MathError::Inf;
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if (is_inf)
            {
                return result;
            }
            always_enforce(false, "Division error (NaN):", a, "/", b);
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Division error (NaN/Inf):", a, "/", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, code);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if (is_nan)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
            return (result > 0) ? std::numeric_limits<T>::max() :
                                  std::numeric_limits<T>::lowest();
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_and(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    T result = a & b;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_or(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    T result = a | b;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_xor(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    T result = a ^ b;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL, typename S>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_left_shift(T a, S shift)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<S>, "Shift must be integral");
    
    constexpr auto max_shift = static_cast<S>(sizeof(T) * 8);
    
    if (shift < 0 || shift >= max_shift)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Invalid left shift amount:", shift);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return T{0};
        }
    }
    
    T result = a << shift;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL, typename S>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_right_shift(T a, S shift)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<S>, "Shift must be integral");
    
    constexpr auto max_shift = static_cast<S>(sizeof(T) * 8);
    
    if (shift < 0 || shift >= max_shift)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Invalid right shift amount:", shift);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return (a < 0) ? T{-1} : T{0};
            }
            else
            {
                return T{0};
            }
        }
    }
    
    T result = a >> shift;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_ARITHMETIC>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_negate(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
    {
        if (a == std::numeric_limits<T>::min())
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "Overflow in negation (min):", a);
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
            {
                return std::numeric_limits<T>::max();
            }
            else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return std::numeric_limits<T>::max();
            }
        }
    }
    
    T result = -a;
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_clamp(T value, T min_val, T max_val)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_arithmetic_v<T>, "checked_clamp requires arithmetic types");
    
    if (min_val > max_val)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Invalid range: min > max");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                          std::is_same_v<Policy, InfTolerantPolicy>)
        {
            std::swap(min_val, max_val);
        }
    }
    
    if constexpr (std::is_floating_point_v<T>)
    {
        if (std::isnan(value) || std::isnan(min_val) || std::isnan(max_val))
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "NaN in clamp");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<T, MathError>(unexpect, MathError::NaN);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                              std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return std::numeric_limits<T>::quiet_NaN();
            }
        }
    }
    
    T result = (value < min_val) ? min_val : (value > max_val) ? max_val : value;
    
    return result;
}

template <typename Policy = ReturnExpectedPolicy, typename T>
[[nodiscard]] constexpr PolicyReturnType<Policy, bool> checked_in_range(T value, T min_val, T max_val)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_arithmetic_v<T>, "checked_in_range requires arithmetic types");
    
    if (min_val > max_val)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Invalid range: min > max");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<bool, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                          std::is_same_v<Policy, InfTolerantPolicy>)
        {
            std::swap(min_val, max_val);
        }
    }
    
    if constexpr (std::is_floating_point_v<T>)
    {
        if (std::isnan(value) || std::isnan(min_val) || std::isnan(max_val))
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "NaN in range check");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<bool, MathError>(unexpect, MathError::NaN);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy> ||
                              std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return false;
            }
        }
    }
    
    bool result = (value >= min_val) && (value <= max_val);
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_mod_fp(T a, T b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    VALIDATE_FP_INPUTS(a, b, "%");
    
    if (b == T{0})
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Division by zero in mod_fp:", a, "%", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::DivByZero);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return T{0};
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    T result = std::fmod(a, b);
    bool is_nan = std::isnan(result);
    
    if (is_nan)
    {
        if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            always_enforce(false, "Modulo error (NaN):", a, "%", b);
        }
        else if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Modulo error (NaN):", a, "%", b);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_abs_fp(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "NaN input in abs_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    T result = std::abs(a);
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_sqrt_fp(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "NaN input in sqrt_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    if (a < 0)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Negative input in sqrt_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return T{0};
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    T result = std::sqrt(a);
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_floor_fp(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "NaN input in floor_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    T result = std::floor(a);
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_ceil_fp(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "NaN input in ceil_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    T result = std::ceil(a);
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_trunc_fp(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "NaN input in trunc_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    T result = std::trunc(a);
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_FLOATING>
[[nodiscard]] PolicyReturnType<Policy, T> checked_round_fp(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (std::isnan(a))
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "NaN input in round_fp:", a);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<T, MathError>(unexpect, MathError::NaN);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            return std::numeric_limits<T>::quiet_NaN();
        }
    }
    
    T result = std::round(a);
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_inc(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    return checked_add<Policy>(a, T{1});
}

template <typename Policy = ThrowOnErrorPolicy, ENABLE_IF_INTEGRAL>
[[nodiscard]] constexpr PolicyReturnType<Policy, T> checked_dec(T a)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    return checked_sub<Policy>(a, T{1});
}

/**
 * @brief Checked pointer arithmetic with address-space overflow detection
 * @warning This function guarantees ADDRESS-SPACE overflow safety but CANNOT
 *          guarantee object lifetime or array bounds safety. The programmer must
 *          ensure the resulting pointer remains within the same allocated object
 *          or one-past-the-end. Violating this remains undefined behavior per the
 *          C++ standard, even though the address arithmetic succeeds.
 * @note The offset is in elements (like standard pointer arithmetic), not bytes.
 *       For ptr + n, the address advances by n * sizeof(P) bytes.
 * @example
 *   int arr[10];
 *   int* p = &arr[0];
 *   auto r1 = checked_add(p, 10);    // OK: one-past-end
 *   auto r2 = checked_add(p, 1000);  // UB: not in same object (even if no overflow)
 */
template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*> checked_add(P* ptr, std::ptrdiff_t offset)
    noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    static_assert(!std::is_void_v<P>, "Cannot perform pointer arithmetic on void*");
    
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    constexpr std::size_t elem_size = sizeof(P);
    
    // First check if offset * sizeof(P) would overflow
    if (offset >= 0)
    {
        auto byte_offset_result = checked_mul<Policy>(
            static_cast<std::uintptr_t>(offset),
            static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_add<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_add<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
    else
    {
        if (offset == std::numeric_limits<std::ptrdiff_t>::min())
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "Pointer arithmetic overflow: offset == PTRDIFF_MIN");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return unexpected(MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
            {
                return nullptr;
            }
            else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return nullptr;
            }
        }
        
        auto byte_offset_result = checked_mul<Policy>(
            static_cast<std::uintptr_t>(-offset),
            static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_sub<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_sub<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
}

/**
 * @brief Checked pointer arithmetic (subtraction) with address-space overflow detection
 * @warning This function guarantees ADDRESS-SPACE overflow safety but CANNOT
 *          guarantee object lifetime or array bounds safety. See checked_add for details.
 * @note The offset is in elements (like standard pointer arithmetic), not bytes.
 */
template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*> checked_sub(P* ptr, std::ptrdiff_t offset)
    noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    static_assert(!std::is_void_v<P>, "Cannot perform pointer arithmetic on void*");
    
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    constexpr std::size_t elem_size = sizeof(P);
    
    if (offset >= 0)
    {
        auto byte_offset_result = checked_mul<Policy>(
            static_cast<std::uintptr_t>(offset),
            static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_sub<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_sub<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
    else
    {
        if (offset == std::numeric_limits<std::ptrdiff_t>::min())
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "Pointer arithmetic overflow: offset == PTRDIFF_MIN");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return unexpected(MathError::Overflow);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
            {
                return nullptr;
            }
            else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
            {
                return nullptr;
            }
        }
        
        auto byte_offset_result = checked_mul<Policy>(
            static_cast<std::uintptr_t>(-offset),
            static_cast<std::uintptr_t>(elem_size));
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!byte_offset_result.has_value())
            {
                return unexpected(byte_offset_result.error());
            }
            auto res = checked_add<Policy>(addr, byte_offset_result.value());
            if (!res.has_value())
            {
                return unexpected(res.error());
            }
            return reinterpret_cast<P*>(res.value());
        }
        else
        {
            auto res = checked_add<Policy>(addr, byte_offset_result);
            return reinterpret_cast<P*>(res);
        }
    }
}

template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*> checked_inc(P* ptr)
    noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    return checked_add<P, Policy>(ptr, std::ptrdiff_t{1});
}

template <typename P, typename Policy = ReturnExpectedPolicy>
[[nodiscard]] PolicyReturnType<Policy, P*> checked_dec(P* ptr)
    noexcept(PolicyTraits<Policy>::template is_noexcept<std::ptrdiff_t>)
{
    return checked_sub<P, Policy>(ptr, std::ptrdiff_t{1});
}

namespace detail {

template <typename Policy, typename T, typename ScalarOp>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> 
checked_vec_op_generic(const std::vector<T>& vec_a, 
                       const std::vector<T>& vec_b,
                       ScalarOp scalar_op,
                       const char* op_name)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in ", op_name);
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, 
                                                       MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = scalar_op(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_add_vec(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<T>, "checked_add_vec requires integral types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in addition");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
    if constexpr (sizeof(T) == 4)
    {
        for (size_t i = 0; i + AVX2_INT32_PER_REG <= n; i += AVX2_INT32_PER_REG)
        {
            __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&vec_a[i]));
            __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&vec_b[i]));
            __m256i vr = _mm256_add_epi32(va, vb);
            
            bool needs_scalar_check = false;
            
            if constexpr (std::is_unsigned_v<T>)
            {
                __m256i sign_bit = _mm256_set1_epi32(static_cast<int>(0x80000000));
                __m256i va_signed = _mm256_xor_si256(va, sign_bit);
                __m256i vr_signed = _mm256_xor_si256(vr, sign_bit);
                __m256i overflow_mask = _mm256_cmpgt_epi32(va_signed, vr_signed);
                int mask = _mm256_movemask_epi8(overflow_mask);
                if (mask != 0)
                {
                    needs_scalar_check = true;
                }
            }
            else if constexpr (std::is_signed_v<T>)
            {
                __m256i zero = _mm256_setzero_si256();
                __m256i va_pos = _mm256_cmpgt_epi32(va, zero);
                __m256i vb_pos = _mm256_cmpgt_epi32(vb, zero);
                __m256i vr_neg = _mm256_cmpgt_epi32(zero, vr);
                __m256i both_pos = _mm256_and_si256(va_pos, vb_pos);
                __m256i pos_overflow = _mm256_and_si256(both_pos, vr_neg);
                
                __m256i va_neg = _mm256_cmpgt_epi32(zero, va);
                __m256i vb_neg = _mm256_cmpgt_epi32(zero, vb);
                __m256i vr_pos = _mm256_cmpgt_epi32(vr, zero);
                __m256i both_neg = _mm256_and_si256(va_neg, vb_neg);
                __m256i neg_overflow = _mm256_and_si256(both_neg, vr_pos);
                
                __m256i overflow_mask = _mm256_or_si256(pos_overflow, neg_overflow);
                int mask = _mm256_movemask_epi8(overflow_mask);
                if (mask != 0)
                {
                    needs_scalar_check = true;
                }
            }
            
            if (needs_scalar_check)
            {
                for (size_t j = i; j < std::min(i + AVX2_INT32_PER_REG, n); ++j)
                {
                    auto temp = checked_add<Policy>(vec_a[j], vec_b[j]);
                    if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                    {
                        if (!temp.has_value())
                        {
                            return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                        }
                        result.push_back(temp.value());
                    }
                    else
                    {
                        result.push_back(temp);
                    }
                }
                continue;
            }
            
            alignas(32) int32_t temp[AVX2_INT32_PER_REG];
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(temp), vr);
            for (size_t j = 0; j < AVX2_INT32_PER_REG && i + j < n; ++j)
            {
                result.push_back(static_cast<T>(temp[j]));
            }
        }
        
        for (size_t i = (n / AVX2_INT32_PER_REG) * AVX2_INT32_PER_REG; i < n; ++i)
        {
            auto temp = checked_add<Policy>(vec_a[i], vec_b[i]);
            if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                if (!temp.has_value())
                {
                    return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                }
                result.push_back(temp.value());
            }
            else
            {
                result.push_back(temp);
            }
        }
        
        return result;
    }
#endif
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_add<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_sub_vec(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<T>, "checked_sub_vec requires integral types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in subtraction");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
    if constexpr (sizeof(T) == 4)
    {
        for (size_t i = 0; i + AVX2_INT32_PER_REG <= n; i += AVX2_INT32_PER_REG)
        {
            __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&vec_a[i]));
            __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&vec_b[i]));
            __m256i vr = _mm256_sub_epi32(va, vb);
            
            bool needs_scalar_check = false;
            
            if constexpr (std::is_unsigned_v<T>)
            {
                __m256i sign_bit = _mm256_set1_epi32(static_cast<int>(0x80000000));
                __m256i va_signed = _mm256_xor_si256(va, sign_bit);
                __m256i vr_signed = _mm256_xor_si256(vr, sign_bit);
                __m256i overflow_mask = _mm256_cmpgt_epi32(vr_signed, va_signed);
                int mask = _mm256_movemask_epi8(overflow_mask);
                if (mask != 0)
                {
                    needs_scalar_check = true;
                }
            }
            else if constexpr (std::is_signed_v<T>)
            {
                __m256i zero = _mm256_setzero_si256();
                __m256i va_pos = _mm256_cmpgt_epi32(va, zero);
                __m256i vb_neg = _mm256_cmpgt_epi32(zero, vb);
                __m256i vr_neg = _mm256_cmpgt_epi32(zero, vr);
                __m256i pos_minus_neg = _mm256_and_si256(va_pos, vb_neg);
                __m256i pos_overflow = _mm256_and_si256(pos_minus_neg, vr_neg);
                
                __m256i va_neg = _mm256_cmpgt_epi32(zero, va);
                __m256i vb_pos = _mm256_cmpgt_epi32(vb, zero);
                __m256i vr_pos = _mm256_cmpgt_epi32(vr, zero);
                __m256i neg_minus_pos = _mm256_and_si256(va_neg, vb_pos);
                __m256i neg_overflow = _mm256_and_si256(neg_minus_pos, vr_pos);
                
                __m256i overflow_mask = _mm256_or_si256(pos_overflow, neg_overflow);
                int mask = _mm256_movemask_epi8(overflow_mask);
                if (mask != 0)
                {
                    needs_scalar_check = true;
                }
            }
            
            if (needs_scalar_check)
            {
                for (size_t j = i; j < std::min(i + AVX2_INT32_PER_REG, n); ++j)
                {
                    auto temp = checked_sub<Policy>(vec_a[j], vec_b[j]);
                    if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                    {
                        if (!temp.has_value())
                        {
                            return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                        }
                        result.push_back(temp.value());
                    }
                    else
                    {
                        result.push_back(temp);
                    }
                }
                continue;
            }
            
            alignas(32) int32_t temp[AVX2_INT32_PER_REG];
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(temp), vr);
            for (size_t j = 0; j < AVX2_INT32_PER_REG && i + j < n; ++j)
            {
                result.push_back(static_cast<T>(temp[j]));
            }
        }
        
        for (size_t i = (n / AVX2_INT32_PER_REG) * AVX2_INT32_PER_REG; i < n; ++i)
        {
            auto temp = checked_sub<Policy>(vec_a[i], vec_b[i]);
            if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                if (!temp.has_value())
                {
                    return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                }
                result.push_back(temp.value());
            }
            else
            {
                result.push_back(temp);
            }
        }
        
        return result;
    }
#endif
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_sub<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_mul_vec(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<T>, "checked_mul_vec requires integral types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in multiplication");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_mul<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_div_vec(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_integral_v<T>, "checked_div_vec requires integral types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in division");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_div<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_add_vec_fp(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_add_vec_fp requires floating-point types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in FP addition");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
    if constexpr (std::is_same_v<T, double>)
    {
auto detect_simd_error = [](const __m256d& va, const __m256d& vb, 
                                      const __m256d& vr) -> bool {
            __m256d nan_mask = _mm256_cmp_pd(vr, vr, _CMP_UNORD_Q);
            if (_mm256_movemask_pd(nan_mask) != 0) {
                return true;
            }
            
            if constexpr (!std::is_same_v<Policy, InfTolerantPolicy>) {
                __m256d inf_pos = _mm256_set1_pd(std::numeric_limits<double>::infinity());
                __m256d inf_neg = _mm256_set1_pd(-std::numeric_limits<double>::infinity());
                __m256d result_is_inf = _mm256_or_pd(
                    _mm256_cmp_pd(vr, inf_pos, _CMP_EQ_OQ),
                    _mm256_cmp_pd(vr, inf_neg, _CMP_EQ_OQ));
                
                __m256d a_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(va, va, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(va, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(va, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d b_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(vb, vb, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(vb, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(vb, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d overflow = _mm256_and_pd(result_is_inf,
                                                 _mm256_and_pd(a_is_finite, b_is_finite));
                
                if (_mm256_movemask_pd(overflow) != 0) {
                    return true;
                }
            }
            return false;
        };
        for (size_t i = 0; i + AVX2_DOUBLES_PER_REG <= n; i += AVX2_DOUBLES_PER_REG)
        {
            __m256d va = _mm256_loadu_pd(&vec_a[i]);
            __m256d vb = _mm256_loadu_pd(&vec_b[i]);
            __m256d vr = _mm256_add_pd(va, vb);
            
            bool has_error = detect_simd_error(va, vb, vr);
            
            if (has_error)
            {
                for (size_t j = i; j < std::min(i + AVX2_DOUBLES_PER_REG, n); ++j)
                {
                    auto temp = checked_add_fp<Policy>(vec_a[j], vec_b[j]);
                    if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                    {
                        if (!temp.has_value())
                        {
                            return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                        }
                        result.push_back(temp.value());
                    }
                    else
                    {
                        result.push_back(temp);
                    }
                }
            }
            else
            {
                alignas(32) double temp[AVX2_DOUBLES_PER_REG];
                _mm256_storeu_pd(temp, vr);
                for (size_t j = 0; j < AVX2_DOUBLES_PER_REG && i + j < n; ++j)
                {
                    result.push_back(static_cast<T>(temp[j]));
                }
            }
        }
        
        for (size_t i = (n / AVX2_DOUBLES_PER_REG) * AVX2_DOUBLES_PER_REG; i < n; ++i)
        {
            auto temp = checked_add_fp<Policy>(vec_a[i], vec_b[i]);
            if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                if (!temp.has_value())
                {
                    return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                }
                result.push_back(temp.value());
            }
            else
            {
                result.push_back(temp);
            }
        }
        
        return result;
    }
#endif
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_add_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_sub_vec_fp(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_sub_vec_fp requires floating-point types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in FP subtraction");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
    if constexpr (std::is_same_v<T, double>)
    {
auto detect_simd_error = [](const __m256d& va, const __m256d& vb, 
                                      const __m256d& vr) -> bool {
            __m256d nan_mask = _mm256_cmp_pd(vr, vr, _CMP_UNORD_Q);
            if (_mm256_movemask_pd(nan_mask) != 0) {
                return true;
            }
            
            if constexpr (!std::is_same_v<Policy, InfTolerantPolicy>) {
                __m256d inf_pos = _mm256_set1_pd(std::numeric_limits<double>::infinity());
                __m256d inf_neg = _mm256_set1_pd(-std::numeric_limits<double>::infinity());
                __m256d result_is_inf = _mm256_or_pd(
                    _mm256_cmp_pd(vr, inf_pos, _CMP_EQ_OQ),
                    _mm256_cmp_pd(vr, inf_neg, _CMP_EQ_OQ));
                
                __m256d a_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(va, va, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(va, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(va, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d b_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(vb, vb, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(vb, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(vb, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d overflow = _mm256_and_pd(result_is_inf,
                                                 _mm256_and_pd(a_is_finite, b_is_finite));
                
                if (_mm256_movemask_pd(overflow) != 0) {
                    return true;
                }
            }
            return false;
        };
        for (size_t i = 0; i + AVX2_DOUBLES_PER_REG <= n; i += AVX2_DOUBLES_PER_REG)
        {
            __m256d va = _mm256_loadu_pd(&vec_a[i]);
            __m256d vb = _mm256_loadu_pd(&vec_b[i]);
            __m256d vr = _mm256_sub_pd(va, vb);
            
            bool has_error = detect_simd_error(va, vb, vr);
            
            if (has_error)
            {
                for (size_t j = i; j < std::min(i + AVX2_DOUBLES_PER_REG, n); ++j)
                {
                    auto temp = checked_sub_fp<Policy>(vec_a[j], vec_b[j]);
                    if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                    {
                        if (!temp.has_value())
                        {
                            return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                        }
                        result.push_back(temp.value());
                    }
                    else
                    {
                        result.push_back(temp);
                    }
                }
            }
            else
            {
                alignas(32) double temp[AVX2_DOUBLES_PER_REG];
                _mm256_storeu_pd(temp, vr);
                for (size_t j = 0; j < AVX2_DOUBLES_PER_REG && i + j < n; ++j)
                {
                    result.push_back(static_cast<T>(temp[j]));
                }
            }
        }
        
        for (size_t i = (n / AVX2_DOUBLES_PER_REG) * AVX2_DOUBLES_PER_REG; i < n; ++i)
        {
            auto temp = checked_sub_fp<Policy>(vec_a[i], vec_b[i]);
            if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                if (!temp.has_value())
                {
                    return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                }
                result.push_back(temp.value());
            }
            else
            {
                result.push_back(temp);
            }
        }
        
        return result;
    }
#endif
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_sub_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_mul_vec_fp(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_mul_vec_fp requires floating-point types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in FP multiplication");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
    if constexpr (std::is_same_v<T, double>)
    {
auto detect_simd_error = [](const __m256d& va, const __m256d& vb, 
                                      const __m256d& vr) -> bool {
            __m256d nan_mask = _mm256_cmp_pd(vr, vr, _CMP_UNORD_Q);
            if (_mm256_movemask_pd(nan_mask) != 0) {
                return true;
            }
            
            if constexpr (!std::is_same_v<Policy, InfTolerantPolicy>) {
                __m256d inf_pos = _mm256_set1_pd(std::numeric_limits<double>::infinity());
                __m256d inf_neg = _mm256_set1_pd(-std::numeric_limits<double>::infinity());
                __m256d result_is_inf = _mm256_or_pd(
                    _mm256_cmp_pd(vr, inf_pos, _CMP_EQ_OQ),
                    _mm256_cmp_pd(vr, inf_neg, _CMP_EQ_OQ));
                
                __m256d a_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(va, va, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(va, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(va, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d b_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(vb, vb, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(vb, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(vb, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d overflow = _mm256_and_pd(result_is_inf,
                                                 _mm256_and_pd(a_is_finite, b_is_finite));
                
                if (_mm256_movemask_pd(overflow) != 0) {
                    return true;
                }
            }
            return false;
        };
        for (size_t i = 0; i + AVX2_DOUBLES_PER_REG <= n; i += AVX2_DOUBLES_PER_REG)
        {
            __m256d va = _mm256_loadu_pd(&vec_a[i]);
            __m256d vb = _mm256_loadu_pd(&vec_b[i]);
            __m256d vr = _mm256_mul_pd(va, vb);
            
            bool has_error = detect_simd_error(va, vb, vr);
            
            if (has_error)
            {
                for (size_t j = i; j < std::min(i + AVX2_DOUBLES_PER_REG, n); ++j)
                {
                    auto temp = checked_mul_fp<Policy>(vec_a[j], vec_b[j]);
                    if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                    {
                        if (!temp.has_value())
                        {
                            return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                        }
                        result.push_back(temp.value());
                    }
                    else
                    {
                        result.push_back(temp);
                    }
                }
            }
            else
            {
                alignas(32) double temp[AVX2_DOUBLES_PER_REG];
                _mm256_storeu_pd(temp, vr);
                for (size_t j = 0; j < AVX2_DOUBLES_PER_REG && i + j < n; ++j)
                {
                    result.push_back(static_cast<T>(temp[j]));
                }
            }
        }
        
        for (size_t i = (n / AVX2_DOUBLES_PER_REG) * AVX2_DOUBLES_PER_REG; i < n; ++i)
        {
            auto temp = checked_mul_fp<Policy>(vec_a[i], vec_b[i]);
            if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                if (!temp.has_value())
                {
                    return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                }
                result.push_back(temp.value());
            }
            else
            {
                result.push_back(temp);
            }
        }
        
        return result;
    }
#endif
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_mul_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

template <typename Policy = ThrowOnErrorPolicy, typename T>
[[nodiscard]] PolicyReturnType<Policy, std::vector<T>> checked_div_vec_fp(const std::vector<T>& vec_a, const std::vector<T>& vec_b)
    noexcept(PolicyTraits<Policy>::template is_noexcept<T>)
{
    static_assert(std::is_floating_point_v<T>, "checked_div_vec_fp requires floating-point types");
    
    if (vec_a.size() != vec_b.size())
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "Vector size mismatch in FP division");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            return Expected<std::vector<T>, MathError>(unexpect, MathError::InvalidArgument);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            return std::vector<T>();
        }
    }
    
    size_t n = vec_a.size();
    std::vector<T> result;
    result.reserve(n);
    
#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
    if constexpr (std::is_same_v<T, double>)
    {
auto detect_simd_error = [](const __m256d& va, const __m256d& vb, 
                                      const __m256d& vr) -> bool {
            __m256d nan_mask = _mm256_cmp_pd(vr, vr, _CMP_UNORD_Q);
            if (_mm256_movemask_pd(nan_mask) != 0) {
                return true;
            }
            
            if constexpr (!std::is_same_v<Policy, InfTolerantPolicy>) {
                __m256d inf_pos = _mm256_set1_pd(std::numeric_limits<double>::infinity());
                __m256d inf_neg = _mm256_set1_pd(-std::numeric_limits<double>::infinity());
                __m256d result_is_inf = _mm256_or_pd(
                    _mm256_cmp_pd(vr, inf_pos, _CMP_EQ_OQ),
                    _mm256_cmp_pd(vr, inf_neg, _CMP_EQ_OQ));
                
                __m256d a_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(va, va, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(va, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(va, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d b_is_finite = _mm256_and_pd(
                    _mm256_cmp_pd(vb, vb, _CMP_ORD_Q),
                    _mm256_and_pd(
                        _mm256_cmp_pd(vb, inf_pos, _CMP_NEQ_OQ),
                        _mm256_cmp_pd(vb, inf_neg, _CMP_NEQ_OQ)));
                
                __m256d overflow = _mm256_and_pd(result_is_inf,
                                                 _mm256_and_pd(a_is_finite, b_is_finite));
                
                if (_mm256_movemask_pd(overflow) != 0) {
                    return true;
                }
            }
            return false;
        };
        for (size_t i = 0; i + AVX2_DOUBLES_PER_REG <= n; i += AVX2_DOUBLES_PER_REG)
        {
            __m256d va = _mm256_loadu_pd(&vec_a[i]);
            __m256d vb = _mm256_loadu_pd(&vec_b[i]);
            __m256d vr = _mm256_div_pd(va, vb);
            
            bool has_error = detect_simd_error(va, vb, vr);
            
            if (has_error)
            {
                for (size_t j = i; j < std::min(i + AVX2_DOUBLES_PER_REG, n); ++j)
                {
                    auto temp = checked_div_fp<Policy>(vec_a[j], vec_b[j]);
                    if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
                    {
                        if (!temp.has_value())
                        {
                            return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                        }
                        result.push_back(temp.value());
                    }
                    else
                    {
                        result.push_back(temp);
                    }
                }
            }
            else
            {
                alignas(32) double temp[AVX2_DOUBLES_PER_REG];
                _mm256_storeu_pd(temp, vr);
                for (size_t j = 0; j < AVX2_DOUBLES_PER_REG && i + j < n; ++j)
                {
                    result.push_back(static_cast<T>(temp[j]));
                }
            }
        }
        
        for (size_t i = (n / AVX2_DOUBLES_PER_REG) * AVX2_DOUBLES_PER_REG; i < n; ++i)
        {
            auto temp = checked_div_fp<Policy>(vec_a[i], vec_b[i]);
            if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                if (!temp.has_value())
                {
                    return Expected<std::vector<T>, MathError>(unexpect, temp.error());
                }
                result.push_back(temp.value());
            }
            else
            {
                result.push_back(temp);
            }
        }
        
        return result;
    }
#endif
    
    for (size_t i = 0; i < n; ++i)
    {
        auto temp = checked_div_fp<Policy>(vec_a[i], vec_b[i]);
        if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if (!temp.has_value())
            {
                return Expected<std::vector<T>, MathError>(unexpect, temp.error());
            }
            result.push_back(temp.value());
        }
        else
        {
            result.push_back(temp);
        }
    }
    
    return result;
}

namespace static_math {

template <typename T, T a, T b>
constexpr T add()
{
    static_assert(b >= 0 ? a <= std::numeric_limits<T>::max() - b : 
                            a >= std::numeric_limits<T>::min() - b,
                  "Overflow in static_math::add");
    return a + b;
}

template <typename T, T a, T b>
constexpr T sub()
{
    static_assert(b >= 0 ? a >= std::numeric_limits<T>::min() + b :
                            a <= std::numeric_limits<T>::max() + b,
                  "Overflow in static_math::sub");
    return a - b;
}

template <typename T, T a, T b>
constexpr T mul()
{
    static_assert(a == 0 || b == 0 ||
        (a > 0 && b > 0 ? a <= std::numeric_limits<T>::max() / b :
         a < 0 && b < 0 ? a >= std::numeric_limits<T>::max() / b :
         (a > 0 ? b >= std::numeric_limits<T>::min() / a :
                  a <= std::numeric_limits<T>::min() / b)),
                  "Overflow in static_math::mul");
    return a * b;
}

template <typename T, T a, T b>
constexpr T div()
{
    static_assert(b != 0, "Division by zero in static_math::div");
    static_assert(!(a == std::numeric_limits<T>::min() && b == -1),
                  "Overflow in static_math::div (min / -1)");
    return a / b;
}

template <typename T, T a, T b>
constexpr T mod()
{
    static_assert(b != 0, "Modulo by zero in static_math::mod");
    static_assert(!(a == std::numeric_limits<T>::min() && b == -1),
                  "Overflow in static_math::mod (min % -1)");
    return a % b;
}

template <typename T, T a, int shift>
constexpr T left_shift()
{
    static_assert(shift >= 0 && shift < static_cast<int>(sizeof(T) * 8),
                  "Invalid shift amount in static_math::left_shift");
    return a << shift;
}

template <typename T, T a, int shift>
constexpr T right_shift()
{
    static_assert(shift >= 0 && shift < static_cast<int>(sizeof(T) * 8),
                  "Invalid shift amount in static_math::right_shift");
    return a >> shift;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T and_op(T a, T b) { return a & b; }

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T or_op(T a, T b) { return a | b; }

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T xor_op(T a, T b) { return a ^ b; }

} // namespace static_math

// =============================================================================
// CHECKED CAST - Safe narrowing conversions
// =============================================================================

namespace detail {

template <typename To, typename From>
struct CastOverflowCheck
{
    static constexpr bool would_overflow(From value) noexcept
    {
        using ToLimits = std::numeric_limits<To>;
        
        if constexpr (std::is_same_v<To, From>)
        {
            return false;
        }
        else if constexpr (std::is_floating_point_v<From> && std::is_integral_v<To>)
        {
            if (std::isnan(value) || std::isinf(value))
            {
                return true;
            }
            return value < static_cast<From>(ToLimits::lowest()) ||
                   value > static_cast<From>(ToLimits::max());
        }
        else if constexpr (std::is_integral_v<From> && std::is_floating_point_v<To>)
        {
            return false;
        }
        else if constexpr (std::is_signed_v<From> == std::is_signed_v<To>)
        {
            if constexpr (sizeof(From) <= sizeof(To))
            {
                return false;
            }
            else
            {
                return value < static_cast<From>(ToLimits::lowest()) ||
                       value > static_cast<From>(ToLimits::max());
            }
        }
        else if constexpr (std::is_signed_v<From> && !std::is_signed_v<To>)
        {
            if (value < 0)
            {
                return true;
            }
            using UnsignedFrom = std::make_unsigned_t<From>;
            return static_cast<UnsignedFrom>(value) > ToLimits::max();
        }
        else
        {
            if constexpr (sizeof(From) < sizeof(To))
            {
                return false;
            }
            else
            {
                return value > static_cast<From>(ToLimits::max());
            }
        }
    }
};

} // namespace detail

/**
 * @brief Safely cast a value from one numeric type to another with overflow detection.
 * 
 * @tparam To Target type
 * @tparam From Source type (deduced)
 * @tparam Policy Error handling policy (default: ThrowOnErrorPolicy)
 * @param value Value to convert
 * @return Converted value or error according to policy
 * 
 * Detects:
 * - Integer narrowing overflow (e.g., int64 -> int32)
 * - Sign conversion errors (e.g., negative int -> unsigned)
 * - Float-to-int overflow and special values (NaN, Inf)
 * 
 * @example
 *   int64_t big = 1000000000000LL;
 *   auto result = checked_cast<int32_t, ThrowOnErrorPolicy>(big);  // throws
 *   
 *   auto safe = checked_cast<int32_t, ReturnExpectedPolicy>(100LL);
 *   if (safe.has_value()) { int32_t x = *safe; }
 */
template <typename To, typename Policy = ThrowOnErrorPolicy, typename From>
[[nodiscard]] constexpr PolicyReturnType<Policy, To> checked_cast(From value)
    noexcept(PolicyTraits<Policy>::template is_noexcept<To>)
{
    static_assert(std::is_arithmetic_v<From>, "checked_cast requires arithmetic source type");
    static_assert(std::is_arithmetic_v<To>, "checked_cast requires arithmetic target type");
    static_assert(!std::is_same_v<From, bool>, "checked_cast does not support bool source");
    static_assert(!std::is_same_v<To, bool>, "checked_cast does not support bool target");
    
    if constexpr (std::is_same_v<To, From>)
    {
        return static_cast<To>(value);
    }
    
    if constexpr (std::is_floating_point_v<From>)
    {
        if (std::isnan(value))
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "checked_cast: NaN cannot be converted");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<To, MathError>(unexpect, MathError::NaN);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
            {
                if constexpr (std::is_floating_point_v<To>)
                {
                    return std::numeric_limits<To>::quiet_NaN();
                }
                else
                {
                    return To{0};
                }
            }
            else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
            {
                if constexpr (std::is_floating_point_v<To>)
                {
                    return std::numeric_limits<To>::quiet_NaN();
                }
                else
                {
                    always_enforce(false, "checked_cast: NaN cannot convert to integer");
                }
            }
        }
        
        if (std::isinf(value) && std::is_integral_v<To>)
        {
            if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
            {
                always_enforce(false, "checked_cast: Inf cannot be converted to integer");
            }
            else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
            {
                return Expected<To, MathError>(unexpect, MathError::Inf);
            }
            else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
            {
                return (value > 0) ? std::numeric_limits<To>::max() :
                                     std::numeric_limits<To>::lowest();
            }
            else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
            {
                always_enforce(false, "checked_cast: Inf cannot convert to integer");
            }
        }
    }
    
    bool overflow = detail::CastOverflowCheck<To, From>::would_overflow(value);
    
    if (overflow)
    {
        if constexpr (std::is_same_v<Policy, ThrowOnErrorPolicy>)
        {
            always_enforce(false, "checked_cast overflow");
        }
        else if constexpr (std::is_same_v<Policy, ReturnExpectedPolicy>)
        {
            if constexpr (std::is_signed_v<From> && !std::is_floating_point_v<From>)
            {
                if (value < 0)
                {
                    return Expected<To, MathError>(unexpect, MathError::Underflow);
                }
            }
            return Expected<To, MathError>(unexpect, MathError::Overflow);
        }
        else if constexpr (std::is_same_v<Policy, SaturatingPolicy>)
        {
            if constexpr (std::is_signed_v<From> && !std::is_floating_point_v<From>)
            {
                if (value < 0)
                {
                    return std::numeric_limits<To>::lowest();
                }
            }
            return std::numeric_limits<To>::max();
        }
        else if constexpr (std::is_same_v<Policy, InfTolerantPolicy>)
        {
            if constexpr (std::is_signed_v<From> && !std::is_floating_point_v<From>)
            {
                if (value < 0)
                {
                    return std::numeric_limits<To>::lowest();
                }
            }
            return std::numeric_limits<To>::max();
        }
    }
    
    return static_cast<To>(value);
}

/**
 * @brief Compile-time checked cast using static_assert.
 * 
 * @tparam To Target type
 * @tparam From Source type
 * @tparam value Value to convert
 * @return Converted value (fails to compile if overflow would occur)
 */
template <typename To, typename From, From value>
constexpr To static_checked_cast()
{
    static_assert(std::is_arithmetic_v<From>, "static_checked_cast requires arithmetic source");
    static_assert(std::is_arithmetic_v<To>, "static_checked_cast requires arithmetic target");
    static_assert(!detail::CastOverflowCheck<To, From>::would_overflow(value),
                  "static_checked_cast: value would overflow target type");
    return static_cast<To>(value);
}

} // namespace fat_p

// =============================================================================
// Macro cleanup - prevent leaking internal macros to user code
// =============================================================================
#undef HAS_BUILTIN_OVERFLOW
#undef VALIDATE_FP_INPUTS
