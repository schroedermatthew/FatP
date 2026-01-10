/**
 * @file test_SimdVector.cpp
 * @brief Comprehensive unit tests for SimdVector.h
 */
/*
FATP_META:
  meta_version: 1
  component: SimdVector
  file_role: test
  path: tests/test_SimdVector.cpp
  namespace: fat_p
  summary: "Unit tests for SimdVector."
  related:
    docs_search: "SimdVector"
    headers:
      - fat_p/SimdVector.h
      - fat_p/FatPTest.h
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

#include <iostream>
#include <cmath>
#include <vector>
#include <limits>

#include "SimdVector.h"
#include "FatPTest.h"

namespace fat_p::testing::simdvector
{

TEST_CASE(construction)
{
    SimdVectorF vec_scalar(5.0f);

    alignas(SimdVectorF::alignment) float data[SimdVectorF::width];
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        data[i] = static_cast<float>(i);
    }

    auto vec_loaded = SimdVectorF::load_aligned(data);

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    vec_scalar.store_aligned(result);

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_EQ(result[i], 5.0f, "Scalar broadcast should fill all lanes");
    }

    // Test that loaded vector contains expected values
    alignas(SimdVectorF::alignment) float loaded_result[SimdVectorF::width];
    vec_loaded.store_aligned(loaded_result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_EQ(loaded_result[i], static_cast<float>(i), "Loaded vector should contain original data");
    }

    return true;
}

TEST_CASE(arithmetic)
{
    SimdVectorF a(2.0f);
    SimdVectorF b(3.0f);

    auto sum = a + b;
    auto diff = a - b;
    auto prod = a * b;
    auto quot = a / b;

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];

    sum.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - 5.0f), 1e-6f, "Addition should work");
    }

    diff.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - (-1.0f)), 1e-6f, "Subtraction should work");
    }

    prod.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - 6.0f), 1e-6f, "Multiplication should work");
    }

    quot.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - (2.0f / 3.0f)), 1e-6f, "Division should work");
    }

    return true;
}

TEST_CASE(fma)
{
    SimdVectorF a(2.0f);
    SimdVectorF b(3.0f);
    SimdVectorF c(4.0f);

    auto result_vec = SimdVectorF::fma(a, b, c);

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    result_vec.store_aligned(result);

    float expected = 2.0f * 3.0f + 4.0f; // 10.0
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - expected), 1e-6f, "FMA should compute a*b+c");
    }

    return true;
}

// P0-3 Regression Test: NEON FMS was returning c-a*b instead of a*b-c
TEST_CASE(fms)
{
    SimdVectorF a(2.0f);
    SimdVectorF b(3.0f);
    SimdVectorF c(1.0f);

    auto result_vec = SimdVectorF::fms(a, b, c);

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    result_vec.store_aligned(result);

    // FMS should compute a*b - c = 2*3 - 1 = 5.0
    // Bug was returning c - a*b = 1 - 6 = -5.0
    float expected = 2.0f * 3.0f - 1.0f; // 5.0
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - expected), 1e-6f, 
                      "FMS should compute a*b-c, not c-a*b");
    }

    // Additional test with different values to catch sign errors
    SimdVectorF x(5.0f);
    SimdVectorF y(4.0f);
    SimdVectorF z(3.0f);
    
    auto result2_vec = SimdVectorF::fms(x, y, z);
    alignas(SimdVectorF::alignment) float result2[SimdVectorF::width];
    result2_vec.store_aligned(result2);
    
    float expected2 = 5.0f * 4.0f - 3.0f; // 17.0
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result2[i] - expected2), 1e-6f, 
                      "FMS(5,4,3) should be 17, not -17");
    }

    return true;
}

TEST_CASE(horizontal_ops)
{
    alignas(SimdVectorF::alignment) float data[SimdVectorF::width];
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        data[i] = static_cast<float>(i + 1);
    }

    auto vec = SimdVectorF::load_aligned(data);

    float sum = vec.horizontal_sum();
    float expected_sum = 0.0f;
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        expected_sum += data[i];
    }

    ASSERT_LT(std::abs(sum - expected_sum), 1e-5f, "Horizontal sum should be correct");

    float max_val = vec.horizontal_max();
    ASSERT_LT(std::abs(max_val - static_cast<float>(SimdVectorF::width)), 1e-6f,
                  "Horizontal max should be width");

    float min_val = vec.horizontal_min();
    ASSERT_LT(std::abs(min_val - 1.0f), 1e-6f, "Horizontal min should be 1.0");

    return true;
}

TEST_CASE(sqrt)
{
    alignas(SimdVectorF::alignment) float data[SimdVectorF::width];
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        data[i] = static_cast<float>((i + 1) * (i + 1)); // Perfect squares
    }

    auto vec = SimdVectorF::load_aligned(data);
    auto result_vec = vec.sqrt();

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    result_vec.store_aligned(result);

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        float expected = static_cast<float>(i + 1);
        ASSERT_LT(std::abs(result[i] - expected), 1e-5f, "Square root should be correct");
    }

    return true;
}

TEST_CASE(min_max)
{
    SimdVectorF a(2.0f);
    SimdVectorF b(5.0f);

    auto min_vec = a.min(b);
    auto max_vec = a.max(b);

    alignas(SimdVectorF::alignment) float min_result[SimdVectorF::width];
    alignas(SimdVectorF::alignment) float max_result[SimdVectorF::width];

    min_vec.store_aligned(min_result);
    max_vec.store_aligned(max_result);

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(min_result[i] - 2.0f), 1e-6f, "Min should be 2.0");
        ASSERT_LT(std::abs(max_result[i] - 5.0f), 1e-6f, "Max should be 5.0");
    }

    return true;
}

TEST_CASE(unaligned_loads)
{
    // Create unaligned buffer
    std::vector<float> buffer(SimdVectorF::width + 1);
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = static_cast<float>(i);
    }

    // Load from unaligned address (offset by 1 float)
    auto vec = SimdVectorF::load_unaligned(buffer.data() + 1);

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    vec.store_aligned(result);

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - static_cast<float>(i + 1)), 1e-6f,
                      "Unaligned load should work correctly");
    }

    return true;
}

TEST_CASE(double_precision)
{
    SimdVectorD a(2.0);
    SimdVectorD b(3.0);

    auto sum = a + b;
    auto prod = a * b;

    alignas(SimdVectorD::alignment) double result[SimdVectorD::width];

    sum.store_aligned(result);
    for (size_t i = 0; i < SimdVectorD::width; ++i) {
        ASSERT_LT(std::abs(result[i] - 5.0), 1e-12, "Double precision addition should work");
    }

    prod.store_aligned(result);
    for (size_t i = 0; i < SimdVectorD::width; ++i) {
        ASSERT_LT(std::abs(result[i] - 6.0), 1e-12,
                      "Double precision multiplication should work");
    }

    return true;
}

TEST_CASE(mask_operations)
{
    SimdVectorF a(2.0f);
    SimdVectorF b(3.0f);

    auto mask_lt = a < b;
    auto mask_gt = a > b;
    auto mask_eq = a == a;

    // a < b should be all true
    ASSERT_TRUE(mask_lt.all(), "2.0 < 3.0 should be true for all lanes");

    // a > b should be all false
    ASSERT_TRUE(mask_gt.none(), "2.0 > 3.0 should be false for all lanes");

    // a == a should be all true
    ASSERT_TRUE(mask_eq.all(), "a == a should be true for all lanes");

    // Test popcount
    ASSERT_EQ(mask_lt.popcount(), SimdVectorF::width, "All-true mask popcount");
    ASSERT_EQ(mask_gt.popcount(), 0, "All-false mask popcount");

    return true;
}

TEST_CASE(select_blend)
{
    SimdVectorF a(2.0f);
    SimdVectorF b(5.0f);

    auto mask = a < b; // All true

    auto selected = SimdVectorF::select(mask, a, b);

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    selected.store_aligned(result);

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        ASSERT_LT(std::abs(result[i] - 2.0f), 1e-6f,
                      "Select with all-true mask should return first operand");
    }

    return true;
}

// ============================================================================
// Adversarial Tests (from ChatGPT four-AI analysis)
// ============================================================================

// Tests mixed true/false mask lanes - not just all-true or all-false
TEST_CASE(partial_mask_select)
{
    // A "partial" mask (some true, some false) requires at least 2 lanes.
    // In scalar mode (width=1), skip this test since partial masks are impossible.
    if constexpr (SimdVectorF::width < 2) {
        // Scalar fallback: cannot have partial mask with single lane
        // Still verify select() works with all-true and all-false masks
        auto a = SimdVectorF(42.0f);
        auto b = SimdVectorF(-1.0f);
        
        auto all_true_mask = (a > b);  // 42 > -1 → true
        auto all_false_mask = (a < b); // 42 < -1 → false
        
        ASSERT_TRUE(all_true_mask.all(), "partial_mask_select: scalar all-true mask");
        ASSERT_TRUE(all_false_mask.none(), "partial_mask_select: scalar all-false mask");
        
        auto sel_true = SimdVectorF::select(all_true_mask, a, b);
        auto sel_false = SimdVectorF::select(all_false_mask, a, b);
        
        alignas(SimdVectorF::alignment) float r1[SimdVectorF::width];
        alignas(SimdVectorF::alignment) float r2[SimdVectorF::width];
        sel_true.store_aligned(r1);
        sel_false.store_aligned(r2);
        
        ASSERT_LT(std::abs(r1[0] - 42.0f), 1e-6f, 
                      "partial_mask_select: scalar select with true mask");
        ASSERT_LT(std::abs(r2[0] - (-1.0f)), 1e-6f, 
                      "partial_mask_select: scalar select with false mask");
        
        return true;
    }

    // Build two distinct vectors a and b
    alignas(SimdVectorF::alignment) float a_data[SimdVectorF::width];
    alignas(SimdVectorF::alignment) float b_data[SimdVectorF::width];

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        a_data[i] = 1.0f + static_cast<float>(i);
        b_data[i] = -1.0f - static_cast<float>(i);
    }

    auto a = SimdVectorF::load_aligned(a_data);
    auto b = SimdVectorF::load_aligned(b_data);

    // Build a mask that is "first half true, second half false"
    // e.g. for width=8: lanes 0-3 true, 4-7 false
    alignas(SimdVectorF::alignment) float mask_builder[SimdVectorF::width];
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        mask_builder[i] = (i < SimdVectorF::width / 2) ? 1.0f : -1.0f;
    }
    auto cmp = SimdVectorF::load_aligned(mask_builder);
    auto mask = (cmp > SimdVectorF(0.0f)); // true for first half, false for second

    auto selected = SimdVectorF::select(mask, a, b);

    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    selected.store_aligned(result);

    // Check each lane against expected
    size_t expected_true = 0;
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        const float expected =
            (i < SimdVectorF::width / 2) ? a_data[i] : b_data[i];

        if (i < SimdVectorF::width / 2) {
            ++expected_true;
        }

        ASSERT_LT(std::abs(result[i] - expected), 1e-6f,
                      "partial_mask_select: lane value mismatch");
    }

    // Also verify popcount reflects number of true lanes
    ASSERT_EQ(mask.popcount(), expected_true,
                  "partial_mask_select: mask popcount should match true lanes");

    // Sanity check: mask should be neither all() nor none()
    ASSERT_FALSE(mask.all(), "partial_mask_select: mask should not be all()");
    ASSERT_FALSE(mask.none(), "partial_mask_select: mask should not be none()");

    return true;
}

// Tests NaN propagation - NaN in one lane should not corrupt others
TEST_CASE(nan_propagation)
{
    // Construct a vector where exactly one lane is NaN and others are finite
    alignas(SimdVectorF::alignment) float data_a[SimdVectorF::width];
    alignas(SimdVectorF::alignment) float data_b[SimdVectorF::width];

    const size_t nan_lane = SimdVectorF::width / 2;

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        data_a[i] = static_cast<float>(i + 1);
        data_b[i] = static_cast<float>(2 * (i + 1));
    }

    data_a[nan_lane] = std::numeric_limits<float>::quiet_NaN();

    auto a = SimdVectorF::load_aligned(data_a);
    auto b = SimdVectorF::load_aligned(data_b);

    // Test NaN propagation through basic arithmetic
    auto sum = a + b;
    auto prod = a * b;

    alignas(SimdVectorF::alignment) float sum_result[SimdVectorF::width];
    alignas(SimdVectorF::alignment) float prod_result[SimdVectorF::width];

    sum.store_aligned(sum_result);
    prod.store_aligned(prod_result);

    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        if (i == nan_lane) {
            ASSERT_TRUE(std::isnan(sum_result[i]),
                          "nan_propagation: sum lane with NaN should be NaN");
            ASSERT_TRUE(std::isnan(prod_result[i]),
                          "nan_propagation: prod lane with NaN should be NaN");
        } else {
            const float expected_sum = data_a[i] + data_b[i];
            const float expected_prod = data_a[i] * data_b[i];

            ASSERT_LT(std::abs(sum_result[i] - expected_sum), 1e-5f,
                          "nan_propagation: finite sum mismatch");
            ASSERT_LT(std::abs(prod_result[i] - expected_prod), 1e-5f,
                          "nan_propagation: finite prod mismatch");
        }
    }

    return true;
}

// P1-1 Regression Test: vmvnq_s64 doesn't exist, verify mask NOT works
TEST_CASE(mask_not_operator)
{
    SimdVectorF a(2.0f);
    SimdVectorF b(3.0f);

    auto mask = a < b;      // All true
    auto not_mask = ~mask;  // All false (tests the vmvnq fix)

    ASSERT_TRUE(mask.all(), "Original mask should be all true");
    ASSERT_TRUE(not_mask.none(), "NOT of all-true should be all-false");
    ASSERT_EQ(not_mask.popcount(), 0, "NOT mask popcount should be 0");

    // Test with partial mask
    alignas(SimdVectorF::alignment) float data[SimdVectorF::width];
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        data[i] = (i < SimdVectorF::width / 2) ? 1.0f : -1.0f;
    }
    auto vec = SimdVectorF::load_aligned(data);
    auto partial = vec > SimdVectorF(0.0f);
    auto not_partial = ~partial;

    // NOT should flip: first half false, second half true
    size_t original_count = partial.popcount();
    size_t flipped_count = not_partial.popcount();
    
    ASSERT_EQ(original_count + flipped_count, SimdVectorF::width,
                  "NOT should flip all bits");

    return true;
}

void benchmark_simd_vector()
{
    std::cout << "\n" << colors::cyan() << "SimdVector Benchmarks:" << colors::reset() << "\n\n";

    constexpr size_t N = 1024;
    alignas(64) float a[N], b[N], c[N];

    for (size_t i = 0; i < N; ++i) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }

    // Benchmark SIMD addition
    double simd_time = measure_perf([&]() {
        for (size_t i = 0; i < N; i += SimdVectorF::width) {
            auto va = SimdVectorF::load_aligned(&a[i]);
            auto vb = SimdVectorF::load_aligned(&b[i]);
            auto vc = va + vb;
            vc.store_aligned(&c[i]);
        }
        DoNotOptimize(c[0]);
    }, 100000, 100);

    std::cout << "SIMD addition (" << SimdVectorF::width << " wide): "
              << format_time(simd_time) << "\n";

    // Benchmark FMA
    double fma_time = measure_perf([&]() {
        for (size_t i = 0; i < N; i += SimdVectorF::width) {
            auto va = SimdVectorF::load_aligned(&a[i]);
            auto vb = SimdVectorF::load_aligned(&b[i]);
            auto vc = SimdVectorF::load_aligned(&c[i]);
            auto result = SimdVectorF::fma(va, vb, vc);
            result.store_aligned(&c[i]);
        }
        DoNotOptimize(c[0]);
    }, 100000, 100);

    std::cout << "SIMD FMA: " << format_time(fma_time) << "\n";

    // Benchmark horizontal sum
    double hsum_time = measure_perf([&]() {
        float sum = 0.0f;
        for (size_t i = 0; i < N; i += SimdVectorF::width) {
            auto va = SimdVectorF::load_aligned(&a[i]);
            sum += va.horizontal_sum();
        }
        DoNotOptimize(sum);
    }, 10000, 100);

    std::cout << "Horizontal sum: " << format_time(hsum_time) << "\n";

    std::cout << "\nArchitecture: " << SimdArchitecture::name << "\n";
    std::cout << "SIMD width (float): " << SimdVectorF::width << "\n";
    std::cout << "SIMD width (double): " << SimdVectorD::width << "\n";
}

} // namespace fat_p::testing::simdvector

namespace fat_p::testing
{

bool test_SimdVector()
{
    PRINT_HEADER(SIMD VECTOR)

    TestRunner runner;

    RUN_TEST_NS(runner, simdvector, construction);
    RUN_TEST_NS(runner, simdvector, arithmetic);
    RUN_TEST_NS(runner, simdvector, fma);
    RUN_TEST_NS(runner, simdvector, fms);  // P0-3 regression test
    RUN_TEST_NS(runner, simdvector, horizontal_ops);
    RUN_TEST_NS(runner, simdvector, sqrt);
    RUN_TEST_NS(runner, simdvector, min_max);
    RUN_TEST_NS(runner, simdvector, unaligned_loads);
    RUN_TEST_NS(runner, simdvector, double_precision);
    RUN_TEST_NS(runner, simdvector, mask_operations);
    RUN_TEST_NS(runner, simdvector, select_blend);
    
    // Adversarial tests (from four-AI analysis)
    RUN_TEST_NS(runner, simdvector, partial_mask_select);
    RUN_TEST_NS(runner, simdvector, nan_propagation);
    RUN_TEST_NS(runner, simdvector, mask_not_operator);  // P1-1 regression test

    simdvector::benchmark_simd_vector();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SimdVector() ? 0 : 1;
}
#endif
