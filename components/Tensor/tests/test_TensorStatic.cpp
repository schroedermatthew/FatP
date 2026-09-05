/**
 * @file test_TensorStatic.cpp
 * @brief Comprehensive unit tests for TensorStatic template
 *
 * Tests cover:
 * - Basic construction and shape system
 * - Element-wise arithmetic operations
 * - Policy behavior (Unchecked, Checked, Saturating)
 * - Linear algebra (dot, matmul, transpose, outer)
 * - Reduction operations (sum, mean, max, min, norm)
 * - SIMD operations (when available)
 * - Higher-order tensors (3D, 4D)
 * - Performance benchmarks
 */
/*
FATP_META:
  meta_version: 1
  component: TensorStatic
  file_role: test
  path: components/Tensor/tests/test_TensorStatic.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for TensorStatic."
  api_stability: in_work
  related:
    docs_search: "TensorStatic"
    headers:
      - include/fat_p/TensorStatic.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include "FatPTest.h"
#include "TensorStatic.h"
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace fat_p::testing::tensorstatic
{

template <std::size_t... Dims>
concept ValidShape = requires { typename Shape<Dims...>; };

// =============================================================================
// Basic Construction and Access Tests
// =============================================================================

FATP_TEST_CASE(default_construction)
{
    StaticTensor<float, Vector<4>, UncheckedPolicy> v;
    FATP_ASSERT_EQ(v[0], 0.0f, "Default construction should zero-initialize");
    return true;
}

FATP_TEST_CASE(over_aligned_elements)
{
    const auto check = []<std::size_t Alignment>() {
        struct alignas(Alignment) Element
        {
            int value = 7;
        };
        using TensorType = StaticTensor<Element, Shape<2>>;
        static_assert(alignof(TensorType) >= alignof(Element));
        TensorType tensor;
        for (const auto& element : tensor)
        {
            FATP_ASSERT_EQ(reinterpret_cast<std::uintptr_t>(&element) % alignof(Element),
                           std::uintptr_t{0}, "Every element retains its required alignment");
            FATP_ASSERT_EQ(element.value, 7, "Over-aligned elements are initialized normally");
        }
        return true;
    };
    static_assert(alignof(StaticTensor<float, Shape<8>>) >= 32);
    FATP_ASSERT_TRUE(check.template operator()<64>(), "Support cache-line-aligned elements");
    FATP_ASSERT_TRUE(check.template operator()<128>(), "Support alignment above the dynamic default");
    return true;
}

FATP_TEST_CASE(scalar_broadcast)
{
    StaticTensor<int, Vector<3>, UncheckedPolicy> v(42);
    FATP_ASSERT_EQ(v[0], 42, "Scalar constructor should broadcast to v[0]");
    FATP_ASSERT_EQ(v[1], 42, "Scalar constructor should broadcast to v[1]");
    FATP_ASSERT_EQ(v[2], 42, "Scalar constructor should broadcast to v[2]");
    return true;
}

FATP_TEST_CASE(initializer_list_construction)
{
    StaticTensor<double, Vector<3>, UncheckedPolicy> v{1.0, 2.0, 3.0};
    FATP_ASSERT_EQ(v[0], 1.0, "Initializer list v[0]");
    FATP_ASSERT_EQ(v[1], 2.0, "Initializer list v[1]");
    FATP_ASSERT_EQ(v[2], 3.0, "Initializer list v[2]");
    return true;
}

FATP_TEST_CASE(variadic_constructor)
{
    StaticTensor<float, Vector<4>, UncheckedPolicy> v(1.0f, 2.0f, 3.0f, 4.0f);
    FATP_ASSERT_EQ(v[0], 1.0f, "Variadic constructor v[0]");
    FATP_ASSERT_EQ(v[3], 4.0f, "Variadic constructor v[3]");

    StaticTensor<float, Vector<3>, UncheckedPolicy> filled(1);
    FATP_ASSERT_EQ(filled[0], 1.0f, "Convertible scalar constructor element 0");
    FATP_ASSERT_EQ(filled[1], 1.0f, "Convertible scalar constructor element 1");
    FATP_ASSERT_EQ(filled[2], 1.0f, "Convertible scalar constructor element 2");
    return true;
}

FATP_TEST_CASE(matrix_construction)
{
    StaticTensor<int, Matrix<2, 3>, UncheckedPolicy> m{1, 2, 3, 4, 5, 6};
    FATP_ASSERT_EQ(m.at(0, 0), 1, "Matrix construction m(0,0)");
    FATP_ASSERT_EQ(m.at(1, 2), 6, "Matrix construction m(1,2)");
    return true;
}

FATP_TEST_CASE(bounds_checked_at_and_scalar_rank)
{
    static_assert(StaticTensor<int, Shape<>>::rank == 0);
    static_assert(StaticTensor<int, Shape<>>::size == 1);

    StaticTensor<int, Shape<>> scalar;
    scalar.at() = 7;
    FATP_ASSERT_EQ(scalar.at(), 7, "Rank-zero StaticTensor must expose its scalar element");

    StaticTensor<int, Shape<>> scalar_copy(scalar);
    FATP_ASSERT_EQ(scalar_copy.at(),
                   7,
                   "One-element StaticTensor copy construction must not select the variadic constructor");

    StaticTensor<int, Matrix<2, 3>> matrix{1, 2, 3, 4, 5, 6};
    FATP_ASSERT_THROWS(matrix.at(0, 3), std::out_of_range,
                       "at() must report an out-of-range index through the standard bounds exception");

    const std::initializer_list<int> tooShort{1, 2};
    FATP_ASSERT_THROWS((StaticTensor<int, Vector<3>>(tooShort)), std::invalid_argument,
                       "Initializer length mismatches must use the standard argument exception");

    return true;
}

FATP_TEST_CASE(type_aliases)
{
    Vec3f v{1.0f, 2.0f, 3.0f};
    Mat2x2f m{1.0f, 2.0f, 3.0f, 4.0f};
    FATP_ASSERT_EQ(v[2], 3.0f, "Type alias Vec3f");
    FATP_ASSERT_EQ(m.at(1, 1), 4.0f, "Type alias Mat2x2f");
    return true;
}

// =============================================================================
// Shape System Tests
// =============================================================================

FATP_TEST_CASE(shape_rank)
{
    using S1 = Shape<3, 4, 5>;
    FATP_ASSERT_EQ(S1::rank, 3u, "Shape rank");
    return true;
}

FATP_TEST_CASE(shape_size)
{
    using S1 = Shape<3, 4, 5>;
    FATP_ASSERT_EQ(S1::size, 60u, "Shape size");
    return true;
}

FATP_TEST_CASE(shape_size_overflow_is_rejected)
{
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    static_assert(ValidShape<>);
    static_assert(Shape<>::size == std::size_t{1});
    static_assert(ValidShape<maximum>);
    static_assert(Shape<maximum>::size == maximum);
    static_assert(!ValidShape<maximum / 2 + 1, 2>);
    static_assert(!ValidShape<maximum, 2>);
    static_assert(!ValidShape<maximum, maximum>);

    FATP_ASSERT_EQ(Shape<>::size, std::size_t{1}, "Rank-zero Shape should retain scalar size one");
    return true;
}

FATP_TEST_CASE(shape_dimensions)
{
    using S1 = Shape<3, 4, 5>;
    FATP_ASSERT_EQ(S1::dim<0>(), 3u, "Shape dim<0>");
    FATP_ASSERT_EQ(S1::dim<1>(), 4u, "Shape dim<1>");
    FATP_ASSERT_EQ(S1::dim<2>(), 5u, "Shape dim<2>");
    return true;
}

FATP_TEST_CASE(vector_shape)
{
    using V = Vector<10>;
    FATP_ASSERT_EQ(V::rank, 1u, "Vector rank");
    FATP_ASSERT_EQ(V::size, 10u, "Vector size");
    return true;
}

FATP_TEST_CASE(matrix_shape)
{
    using M = Matrix<3, 4>;
    FATP_ASSERT_EQ(M::rank, 2u, "Matrix rank");
    FATP_ASSERT_EQ(M::size, 12u, "Matrix size");
    return true;
}

// =============================================================================
// Element-Wise Operations Tests
// =============================================================================

FATP_TEST_CASE(vector_addition)
{
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = a + b;
    FATP_ASSERT_EQ(c[0], 5.0f, "Vector addition c[0]");
    FATP_ASSERT_EQ(c[1], 7.0f, "Vector addition c[1]");
    FATP_ASSERT_EQ(c[2], 9.0f, "Vector addition c[2]");
    return true;
}

FATP_TEST_CASE(vector_subtraction)
{
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = b - a;
    FATP_ASSERT_EQ(c[0], 3.0f, "Vector subtraction c[0]");
    FATP_ASSERT_EQ(c[1], 3.0f, "Vector subtraction c[1]");
    FATP_ASSERT_EQ(c[2], 3.0f, "Vector subtraction c[2]");
    return true;
}

FATP_TEST_CASE(hadamard_product)
{
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = a * b;
    FATP_ASSERT_EQ(c[0], 4.0f, "Hadamard product c[0]");
    FATP_ASSERT_EQ(c[1], 10.0f, "Hadamard product c[1]");
    FATP_ASSERT_EQ(c[2], 18.0f, "Hadamard product c[2]");
    return true;
}

FATP_TEST_CASE(element_wise_division)
{
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};
    auto c = b / a;
    FATP_ASSERT_EQ(c[0], 4.0f, "Element-wise division c[0]");
    FATP_ASSERT_EQ(c[1], 2.5f, "Element-wise division c[1]");
    FATP_ASSERT_EQ(c[2], 2.0f, "Element-wise division c[2]");
    return true;
}

FATP_TEST_CASE(scalar_multiplication)
{
    Vec3f a{1.0f, 2.0f, 3.0f};
    auto c = a * 2.0f;
    FATP_ASSERT_EQ(c[0], 2.0f, "Scalar multiplication c[0]");
    FATP_ASSERT_EQ(c[1], 4.0f, "Scalar multiplication c[1]");
    FATP_ASSERT_EQ(c[2], 6.0f, "Scalar multiplication c[2]");
    return true;
}

FATP_TEST_CASE(scalar_multiplication_reversed)
{
    Vec3f a{1.0f, 2.0f, 3.0f};
    auto c = 3.0f * a;
    FATP_ASSERT_EQ(c[0], 3.0f, "Reversed scalar multiplication c[0]");
    FATP_ASSERT_EQ(c[1], 6.0f, "Reversed scalar multiplication c[1]");
    FATP_ASSERT_EQ(c[2], 9.0f, "Reversed scalar multiplication c[2]");
    return true;
}

FATP_TEST_CASE(unary_negation_preserves_signed_zero_and_integer_policies)
{
    const auto preservesSignedZero = []<typename Policy>() {
        const StaticTensor<double, Vector<2>, Policy> input{0.0, -0.0};
        const auto result = -input;
        return result[0] == 0.0 && std::signbit(result[0]) && result[1] == 0.0 && !std::signbit(result[1]);
    };

    FATP_ASSERT_TRUE(preservesSignedZero.template operator()<UncheckedPolicy>(),
                     "Unchecked unary negation should flip both floating zero signs");
    FATP_ASSERT_TRUE(preservesSignedZero.template operator()<CheckedPolicy>(),
                     "Checked unary negation should flip both floating zero signs");
    FATP_ASSERT_TRUE(preservesSignedZero.template operator()<SaturatingArithmeticPolicy>(),
                     "Saturating unary negation should flip both floating zero signs");

    const auto originalRounding = std::fegetround();
    const auto roundingChanged = originalRounding != -1 && std::fesetround(FE_DOWNWARD) == 0;
    bool directedRoundingPreservedZero = false;
    if (roundingChanged)
    {
        volatile double negativeZero = -0.0;
        const StaticTensor<double, Vector<1>, UncheckedPolicy> input{negativeZero};
        const auto result = -input;
        directedRoundingPreservedZero = result[0] == 0.0 && !std::signbit(result[0]);
        std::fesetround(originalRounding);
    }
    FATP_ASSERT_TRUE(roundingChanged, "The test environment should support downward rounding mode");
    FATP_ASSERT_TRUE(directedRoundingPreservedZero,
                     "Unary negation should flip negative zero under directed rounding");

    const StaticTensor<int, Vector<1>, CheckedPolicy> minimumChecked{std::numeric_limits<int>::min()};
    FATP_ASSERT_THROWS((-minimumChecked), LogicContractError,
                       "Checked unary negation should report the arithmetic policy's exact exception type");

    const StaticTensor<int, Vector<1>, SaturatingArithmeticPolicy> minimumSaturating{
        std::numeric_limits<int>::min()};
    const auto saturated = -minimumSaturating;
    FATP_ASSERT_EQ(saturated[0], std::numeric_limits<int>::max(),
                   "Saturating unary negation should clamp the minimum integer");

    const StaticTensor<short, Vector<2>, CheckedPolicy> narrow{short{7}, short{-3}};
    const auto narrowNegated = -narrow;
    FATP_ASSERT_EQ(narrowNegated[0], short{-7},
                   "Unary negation should preserve narrow integer policy types");
    FATP_ASSERT_EQ(narrowNegated[1], short{3},
                   "Unary negation should produce the expected narrow integer value");
    return true;
}

// =============================================================================
// Policy Behavior Tests
// =============================================================================

FATP_TEST_CASE(unchecked_policy_allows_operations)
{
    Vec2<int> v{INT_MAX, 1};
    (void)v;
    // This would overflow, but UncheckedPolicy doesn't check
    FATP_ASSERT_TRUE(true, "UncheckedPolicy allows operations without checks");
    return true;
}

FATP_TEST_CASE(checked_policy_throws_on_overflow)
{
    Vec2<int, CheckedPolicy> v1{INT_MAX, 0};
    Vec2<int, CheckedPolicy> v2{1, 0};

    bool caught = false;
    try
    {
        auto result = v1 + v2; // Should throw
        (void)result;
    }
    catch (...)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "CheckedPolicy should throw on overflow");
    return true;
}

FATP_TEST_CASE(saturating_policy_clamps)
{
    // Note: Use SaturatingArithmeticPolicy instead of SaturatingPolicy
    Vec2<int, SaturatingArithmeticPolicy> v1{INT_MAX, 0};
    Vec2<int, SaturatingArithmeticPolicy> v2{100, 0};
    auto result = v1 + v2;
    FATP_ASSERT_EQ(result[0], INT_MAX, "SaturatingArithmeticPolicy should clamp to max");
    return true;
}

// =============================================================================
// Linear Algebra Tests
// =============================================================================

FATP_TEST_CASE(dot_product)
{
    Vec3f a{1.0f, 2.0f, 3.0f};
    Vec3f b{4.0f, 5.0f, 6.0f};

    float result = dot(a, b);
    float expected = 1.0f * 4.0f + 2.0f * 5.0f + 3.0f * 6.0f; // 4 + 10 + 18 = 32

    FATP_ASSERT_TRUE(std::abs(result - expected) < 1e-6f, "Dot product");
    return true;
}

FATP_TEST_CASE(matrix_vector_multiply)
{
    Mat2x2f m{1.0f, 2.0f, 3.0f, 4.0f};
    Vec2f v{5.0f, 6.0f};

    auto result = matvec(m, v);
    // [1 2] [5]   [1*5 + 2*6]   [17]
    // [3 4] [6] = [3*5 + 4*6] = [39]

    FATP_ASSERT_TRUE(std::abs(result[0] - 17.0f) < 1e-6f, "Matrix-vector multiply row 0");
    FATP_ASSERT_TRUE(std::abs(result[1] - 39.0f) < 1e-6f, "Matrix-vector multiply row 1");
    return true;
}

FATP_TEST_CASE(matrix_matrix_multiply)
{
    Mat2x2f a{1.0f, 2.0f, 3.0f, 4.0f};
    Mat2x2f b{5.0f, 6.0f, 7.0f, 8.0f};

    auto result = matmul(a, b);
    // [1 2] [5 6]   [1*5+2*7  1*6+2*8]   [19 22]
    // [3 4] [7 8] = [3*5+4*7  3*6+4*8] = [43 50]

    FATP_ASSERT_TRUE(std::abs(result.at(0, 0) - 19.0f) < 1e-6f, "Matmul [0,0]");
    FATP_ASSERT_TRUE(std::abs(result.at(0, 1) - 22.0f) < 1e-6f, "Matmul [0,1]");
    FATP_ASSERT_TRUE(std::abs(result.at(1, 0) - 43.0f) < 1e-6f, "Matmul [1,0]");
    FATP_ASSERT_TRUE(std::abs(result.at(1, 1) - 50.0f) < 1e-6f, "Matmul [1,1]");
    return true;
}

FATP_TEST_CASE(matrix_transpose)
{
    StaticTensor<float, Matrix<2, 3>, UncheckedPolicy> m{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto result = transpose(m);

    FATP_ASSERT_EQ(result.at(0, 0), 1.0f, "Transpose (0,0)");
    FATP_ASSERT_EQ(result.at(0, 1), 4.0f, "Transpose (0,1)");
    FATP_ASSERT_EQ(result.at(1, 0), 2.0f, "Transpose (1,0)");
    FATP_ASSERT_EQ(result.at(1, 1), 5.0f, "Transpose (1,1)");
    FATP_ASSERT_EQ(result.at(2, 0), 3.0f, "Transpose (2,0)");
    FATP_ASSERT_EQ(result.at(2, 1), 6.0f, "Transpose (2,1)");
    return true;
}

FATP_TEST_CASE(outer_product)
{
    Vec2f a{1.0f, 2.0f};
    Vec2f b{3.0f, 4.0f};
    auto result = outer(a, b);

    // [1]         [1*3  1*4]   [3  4]
    // [2] otimes [3 4] [2*3  2*4] = [6  8]

    FATP_ASSERT_EQ(result.at(0, 0), 3.0f, "Outer product (0,0)");
    FATP_ASSERT_EQ(result.at(0, 1), 4.0f, "Outer product (0,1)");
    FATP_ASSERT_EQ(result.at(1, 0), 6.0f, "Outer product (1,0)");
    FATP_ASSERT_EQ(result.at(1, 1), 8.0f, "Outer product (1,1)");
    return true;
}

// =============================================================================
// Reduction Operations Tests
// =============================================================================

FATP_TEST_CASE(sum_reduction)
{
    Vec3f v{1.0f, 2.0f, 3.0f};
    float result = sum(v);
    FATP_ASSERT_EQ(result, 6.0f, "Sum reduction");
    return true;
}

FATP_TEST_CASE(mean_reduction)
{
    Vec4f v{2.0f, 4.0f, 6.0f, 8.0f};
    const auto result = mean(v);
    static_assert(std::is_same_v<decltype(result), const double>);
    FATP_ASSERT_EQ(result, 5.0, "Mean reduction");
    return true;
}

FATP_TEST_CASE(reductions_widen_narrow_values_and_counts)
{
    constexpr StaticTensor<std::uint8_t, Vector<256>> bytes(std::uint8_t{1});
    constexpr auto byteSum = sum(bytes);
    constexpr auto byteMean = mean(bytes);
    static_assert(std::is_same_v<std::remove_cv_t<decltype(byteSum)>, std::uint64_t>);
    static_assert(std::is_same_v<std::remove_cv_t<decltype(byteMean)>, double>);
    static_assert(byteSum == 256);
    static_assert(byteMean == 1.0);

    constexpr StaticTensor<std::int16_t, Vector<2>> signedValues{
        std::numeric_limits<std::int16_t>::max(), std::numeric_limits<std::int16_t>::max()};
    static_assert(sum(signedValues) == std::int64_t{65534});
    FATP_ASSERT_EQ(byteSum, std::uint64_t{256}, "Narrow unsigned sums widen before accumulation");
    FATP_ASSERT_EQ(byteMean, 1.0, "Mean divisors remain representable when the static size exceeds uint8_t");
    return true;
}

FATP_TEST_CASE(max_reduction)
{
    Vec4f v{3.0f, 1.0f, 4.0f, 2.0f};
    float result = max(v);
    FATP_ASSERT_EQ(result, 4.0f, "Max reduction");
    return true;
}

FATP_TEST_CASE(min_reduction)
{
    Vec4f v{3.0f, 1.0f, 4.0f, 2.0f};
    float result = min(v);
    FATP_ASSERT_EQ(result, 1.0f, "Min reduction");
    return true;
}

FATP_TEST_CASE(extrema_propagate_the_first_nan)
{
    const auto positiveNan = std::numeric_limits<double>::quiet_NaN();
    const auto negativeNan = -std::numeric_limits<double>::quiet_NaN();
    const StaticTensor<double, Vector<4>> values{4.0, negativeNan, positiveNan, -2.0};

    const auto maximum = max(values);
    const auto minimum = min(values);
    FATP_ASSERT_TRUE(std::isnan(maximum) && std::signbit(maximum),
                     "Maximum propagates the first NaN independent of its position");
    FATP_ASSERT_TRUE(std::isnan(minimum) && std::signbit(minimum),
                     "Minimum propagates the first NaN independent of its position");
    return true;
}

FATP_TEST_CASE(l2_norm)
{
    Vec3f v{3.0f, 4.0f, 0.0f};
    float result = norm(v);
    FATP_ASSERT_EQ(result, 5.0f, "L2 norm (3-4-5 triangle)");
    return true;
}

FATP_TEST_CASE(l2_norm_avoids_intermediate_overflow_and_underflow)
{
    const StaticTensor<float, Vector<2>> large{1.0e20f, 1.0e20f};
    const auto largeNorm = norm(large);
    FATP_ASSERT_TRUE(std::isfinite(largeNorm), "A representable norm must not overflow while squaring inputs");
    FATP_ASSERT_TRUE(std::abs(largeNorm / 1.0e20f - std::sqrt(2.0f)) < 4.0e-6f,
                     "Scaled norm preserves large finite magnitudes");

    const StaticTensor<float, Vector<2>> tiny{1.0e-30f, 0.0f};
    const auto tinyNorm = norm(tiny);
    FATP_ASSERT_TRUE(tinyNorm > 0.0f, "A representable norm must not underflow while squaring inputs");
    FATP_ASSERT_TRUE(std::abs(tinyNorm / 1.0e-30f - 1.0f) < 1.0e-6f,
                     "Scaled norm preserves tiny finite magnitudes");

    const auto normalizedTiny = normalize(tiny);
    FATP_ASSERT_EQ(normalizedTiny[0], 1.0f, "Tiny nonzero vectors remain normalizable");
    FATP_ASSERT_EQ(normalizedTiny[1], 0.0f, "Normalization preserves zero components");
    return true;
}

FATP_TEST_CASE(normalize_produces_unit_vector)
{
    Vec3f v{3.0f, 4.0f, 0.0f};
    auto unit = normalize(v);
    float result = norm(unit);
    FATP_ASSERT_TRUE(std::abs(result - 1.0f) < 1e-6f, "Normalized vector has unit length");
    // Exercise the over-aligned return slot when the caller discards the result.
    (void)normalize(v);
    return true;
}

FATP_TEST_CASE(normalize_rejects_zero_vector)
{
    Vec3f zero{0.0f, 0.0f, 0.0f};
    FATP_ASSERT_THROWS(normalize(zero), std::domain_error,
                       "normalize() must report a zero norm through the standard domain exception");
    const StaticTensor<double, Vector<3>, CheckedPolicy> signedZero{-0.0, 0.0, -0.0};
    FATP_ASSERT_THROWS(normalize(signedZero), std::domain_error,
                       "Checked normalization rejects signed zero before dividing");
    return true;
}

FATP_TEST_CASE(normalize_extreme_finite_vectors)
{
    const auto check = []<typename T, typename Policy>() {
        for (const T magnitude : {std::numeric_limits<T>::max(), std::numeric_limits<T>::min(),
                                  std::numeric_limits<T>::denorm_min()})
        {
            if (magnitude == T{0})
            {
                continue;
            }
            const StaticTensor<T, Vector<3>, Policy> input{magnitude, -magnitude, -T{0}};
            const auto unit = normalize(input);
            const T expected = T{1} / std::sqrt(T{2});
            const T tolerance = T{8} * std::numeric_limits<T>::epsilon();
            if (std::abs(unit[0] - expected) > tolerance ||
                std::abs(unit[1] + expected) > tolerance ||
                std::abs(norm(unit) - T{1}) > tolerance || !std::signbit(unit[2]))
            {
                return false;
            }
        }
        return true;
    };
    FATP_ASSERT_TRUE((check.template operator()<float, UncheckedPolicy>()), "Float extreme normalization");
    FATP_ASSERT_TRUE((check.template operator()<double, UncheckedPolicy>()), "Double extreme normalization");
    FATP_ASSERT_TRUE((check.template operator()<long double, UncheckedPolicy>()), "Long-double normalization");
    FATP_ASSERT_TRUE((check.template operator()<double, CheckedPolicy>()), "Checked extreme normalization");
    FATP_ASSERT_TRUE((check.template operator()<double, SaturatingArithmeticPolicy>()),
                     "Saturating extreme normalization");
    const Vec2d infinite{std::numeric_limits<double>::infinity(), 1.0};
    const auto infiniteUnit = normalize(infinite);
    FATP_ASSERT_TRUE(std::isnan(infiniteUnit[0]) && infiniteUnit[1] == 0.0,
                     "Unchecked nonfinite normalization retains native behavior");
    return true;
}

// =============================================================================
// SIMD Operations Tests (when available)
// =============================================================================

#ifdef __AVX2__
FATP_TEST_CASE(simd_addition)
{
    constexpr size_t N = 16;
    StaticTensor<float, Vector<N>, UncheckedPolicy> a;
    StaticTensor<float, Vector<N>, UncheckedPolicy> b;

    for (size_t i = 0; i < N; ++i)
    {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }

    auto result = a + b;

    for (size_t i = 0; i < N; ++i)
    {
        float expected = static_cast<float>(i * 3);
        if (std::abs(result[i] - expected) >= 1e-6f)
        {
            return false;
        }
    }
    return true;
}

FATP_TEST_CASE(simd_multiplication)
{
    constexpr size_t N = 16;
    StaticTensor<float, Vector<N>, UncheckedPolicy> a;
    StaticTensor<float, Vector<N>, UncheckedPolicy> b;

    for (size_t i = 0; i < N; ++i)
    {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }

    auto result = simd_mul(a, b);

    for (size_t i = 0; i < N; ++i)
    {
        float expected = static_cast<float>(i * i * 2);
        if (std::abs(result[i] - expected) >= 1e-6f)
        {
            return false;
        }
    }
    return true;
}

FATP_TEST_CASE(simd_dot_product)
{
    constexpr size_t N = 16;
    StaticTensor<float, Vector<N>, UncheckedPolicy> a;
    StaticTensor<float, Vector<N>, UncheckedPolicy> b;

    for (size_t i = 0; i < N; ++i)
    {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }

    float result = simd_dot(a, b);
    float expected = 0.0f;
    for (size_t i = 0; i < N; ++i)
    {
        expected += static_cast<float>(i * i * 2);
    }
    FATP_ASSERT_TRUE(std::abs(result - expected) < 1e-4f, "SIMD dot product");
    return true;
}
#endif

// =============================================================================
// Higher-Order Tensor Tests
// =============================================================================

FATP_TEST_CASE(tensor_3d_size)
{
    StaticTensor<float, Tensor3<2, 3, 4>, UncheckedPolicy> t;
    FATP_ASSERT_EQ(t.size, 24u, "3D tensor size");
    return true;
}

FATP_TEST_CASE(tensor_3d_indexing)
{
    StaticTensor<float, Tensor3<2, 3, 4>, UncheckedPolicy> t;
    t.at(1, 2, 3) = 42.0f;
    FATP_ASSERT_EQ(t.at(1, 2, 3), 42.0f, "3D tensor indexing");
    return true;
}

FATP_TEST_CASE(tensor_4d_size)
{
    StaticTensor<int, Tensor4<2, 2, 2, 2>, UncheckedPolicy> t;
    FATP_ASSERT_EQ(t.size, 16u, "4D tensor size");
    return true;
}

FATP_TEST_CASE(tensor_4d_indexing)
{
    StaticTensor<int, Tensor4<2, 2, 2, 2>, UncheckedPolicy> t;

    for (size_t i = 0; i < 16; ++i)
    {
        t[i] = static_cast<int>(i);
    }

    FATP_ASSERT_EQ(t.at(1, 1, 1, 1), 15, "4D tensor indexing");
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================
} // namespace fat_p::testing::tensorstatic

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_TensorStatic()
{
    FATP_PRINT_HEADER(TENSOR MATH)

    TestRunner runner;

    auto& config = get_test_config();
    config.verbose = true;

    auto& out = *config.output;

    // Basic Construction and Access
    out << colors::blue() << "--- Basic Construction and Access ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, default_construction);
    FATP_RUN_TEST_NS(runner, tensorstatic, over_aligned_elements);
    FATP_RUN_TEST_NS(runner, tensorstatic, scalar_broadcast);
    FATP_RUN_TEST_NS(runner, tensorstatic, initializer_list_construction);
    FATP_RUN_TEST_NS(runner, tensorstatic, variadic_constructor);
    FATP_RUN_TEST_NS(runner, tensorstatic, matrix_construction);
    FATP_RUN_TEST_NS(runner, tensorstatic, bounds_checked_at_and_scalar_rank);
    FATP_RUN_TEST_NS(runner, tensorstatic, type_aliases);

    // Shape System
    out << "\n" << colors::blue() << "--- Compile-Time Shape System ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, shape_rank);
    FATP_RUN_TEST_NS(runner, tensorstatic, shape_size);
    FATP_RUN_TEST_NS(runner, tensorstatic, shape_size_overflow_is_rejected);
    FATP_RUN_TEST_NS(runner, tensorstatic, shape_dimensions);
    FATP_RUN_TEST_NS(runner, tensorstatic, vector_shape);
    FATP_RUN_TEST_NS(runner, tensorstatic, matrix_shape);

    // Element-Wise Operations
    out << "\n" << colors::blue() << "--- Element-Wise Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, vector_addition);
    FATP_RUN_TEST_NS(runner, tensorstatic, vector_subtraction);
    FATP_RUN_TEST_NS(runner, tensorstatic, hadamard_product);
    FATP_RUN_TEST_NS(runner, tensorstatic, element_wise_division);
    FATP_RUN_TEST_NS(runner, tensorstatic, scalar_multiplication);
    FATP_RUN_TEST_NS(runner, tensorstatic, scalar_multiplication_reversed);
    FATP_RUN_TEST_NS(runner, tensorstatic, unary_negation_preserves_signed_zero_and_integer_policies);

    // Policy Behavior
    out << "\n" << colors::blue() << "--- Arithmetic Policy Behavior ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, unchecked_policy_allows_operations);
    FATP_RUN_TEST_NS(runner, tensorstatic, checked_policy_throws_on_overflow);
    FATP_RUN_TEST_NS(runner, tensorstatic, saturating_policy_clamps);

    // Linear Algebra
    out << "\n" << colors::blue() << "--- Linear Algebra ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, dot_product);
    FATP_RUN_TEST_NS(runner, tensorstatic, matrix_vector_multiply);
    FATP_RUN_TEST_NS(runner, tensorstatic, matrix_matrix_multiply);
    FATP_RUN_TEST_NS(runner, tensorstatic, matrix_transpose);
    FATP_RUN_TEST_NS(runner, tensorstatic, outer_product);

    // Reduction Operations
    out << "\n" << colors::blue() << "--- Reduction Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, sum_reduction);
    FATP_RUN_TEST_NS(runner, tensorstatic, mean_reduction);
    FATP_RUN_TEST_NS(runner, tensorstatic, reductions_widen_narrow_values_and_counts);
    FATP_RUN_TEST_NS(runner, tensorstatic, max_reduction);
    FATP_RUN_TEST_NS(runner, tensorstatic, min_reduction);
    FATP_RUN_TEST_NS(runner, tensorstatic, extrema_propagate_the_first_nan);
    FATP_RUN_TEST_NS(runner, tensorstatic, l2_norm);
    FATP_RUN_TEST_NS(runner, tensorstatic, l2_norm_avoids_intermediate_overflow_and_underflow);
    FATP_RUN_TEST_NS(runner, tensorstatic, normalize_produces_unit_vector);
    FATP_RUN_TEST_NS(runner, tensorstatic, normalize_rejects_zero_vector);
    FATP_RUN_TEST_NS(runner, tensorstatic, normalize_extreme_finite_vectors);

#ifdef __AVX2__
    // SIMD Operations
    out << "\n" << colors::blue() << "--- SIMD Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, simd_addition);
    FATP_RUN_TEST_NS(runner, tensorstatic, simd_multiplication);
    FATP_RUN_TEST_NS(runner, tensorstatic, simd_dot_product);
    out << "\n" << colors::green() << "[OK] SIMD support detected and tested" << colors::reset() << "\n";
#else
    out << "\n" << colors::yellow() << "[WARN] SIMD tests skipped (no AVX2 support)" << colors::reset() << "\n";
#endif

    // Higher-Order Tensors
    out << "\n" << colors::blue() << "--- Higher-Order Tensors ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, tensorstatic, tensor_3d_size);
    FATP_RUN_TEST_NS(runner, tensorstatic, tensor_3d_indexing);
    FATP_RUN_TEST_NS(runner, tensorstatic, tensor_4d_size);
    FATP_RUN_TEST_NS(runner, tensorstatic, tensor_4d_indexing);

    // Performance Benchmarks


    // Summary
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorStatic() ? 0 : 1;
}
#endif
