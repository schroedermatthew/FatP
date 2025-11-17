#include <iostream>
#include <cmath>

#include "SimdVector.h"
#include "test_SimdVector.h"
#include "FatPTest.h"

namespace fat_p::testing
{

bool test_simd_vector_construction() {
    SimdVectorF vec_scalar(5.0f);
    
    alignas(SimdVectorF::alignment) float data[SimdVectorF::width];
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        data[i] = static_cast<float>(i);
    }
    
    auto vec_loaded = SimdVectorF::load_aligned(data);
    
    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    vec_scalar.store_aligned(result);
    
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        SIMPLE_ASSERT(result[i] == 5.0f, "Scalar broadcast should fill all lanes");
    }
    
    return true;
}

bool test_simd_vector_arithmetic() {
    SimdVectorF a(2.0f);
    SimdVectorF b(3.0f);
    
    auto sum = a + b;
    auto diff = a - b;
    auto prod = a * b;
    auto quot = a / b;
    
    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    
    sum.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        SIMPLE_ASSERT(std::abs(result[i] - 5.0f) < 1e-6f, "Addition should work");
    }
    
    diff.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        SIMPLE_ASSERT(std::abs(result[i] - (-1.0f)) < 1e-6f, "Subtraction should work");
    }
    
    prod.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        SIMPLE_ASSERT(std::abs(result[i] - 6.0f) < 1e-6f, "Multiplication should work");
    }
    
    quot.store_aligned(result);
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        SIMPLE_ASSERT(std::abs(result[i] - (2.0f/3.0f)) < 1e-6f, "Division should work");
    }
    
    return true;
}

bool test_simd_vector_fma() {
    SimdVectorF a(2.0f);
    SimdVectorF b(3.0f);
    SimdVectorF c(4.0f);
    
    auto result_vec = SimdVectorF::fma(a, b, c);
    
    alignas(SimdVectorF::alignment) float result[SimdVectorF::width];
    result_vec.store_aligned(result);
    
    float expected = 2.0f * 3.0f + 4.0f; // 10.0
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        SIMPLE_ASSERT(std::abs(result[i] - expected) < 1e-6f, "FMA should compute a*b+c");
    }
    
    return true;
}

bool test_simd_vector_horizontal_ops() {
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
    
    SIMPLE_ASSERT(std::abs(sum - expected_sum) < 1e-5f, "Horizontal sum should be correct");
    
    float max_val = vec.horizontal_max();
    SIMPLE_ASSERT(std::abs(max_val - static_cast<float>(SimdVectorF::width)) < 1e-6f, 
                  "Horizontal max should be width");
    
    float min_val = vec.horizontal_min();
    SIMPLE_ASSERT(std::abs(min_val - 1.0f) < 1e-6f, "Horizontal min should be 1.0");
    
    return true;
}

bool test_simd_vector_sqrt() {
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
        SIMPLE_ASSERT(std::abs(result[i] - expected) < 1e-5f, "Square root should be correct");
    }
    
    return true;
}

bool test_simd_vector_min_max() {
    SimdVectorF a(2.0f);
    SimdVectorF b(5.0f);
    
    auto min_vec = a.min(b);
    auto max_vec = a.max(b);
    
    alignas(SimdVectorF::alignment) float min_result[SimdVectorF::width];
    alignas(SimdVectorF::alignment) float max_result[SimdVectorF::width];
    
    min_vec.store_aligned(min_result);
    max_vec.store_aligned(max_result);
    
    for (size_t i = 0; i < SimdVectorF::width; ++i) {
        SIMPLE_ASSERT(std::abs(min_result[i] - 2.0f) < 1e-6f, "Min should be 2.0");
        SIMPLE_ASSERT(std::abs(max_result[i] - 5.0f) < 1e-6f, "Max should be 5.0");
    }
    
    return true;
}

bool test_simd_vector_unaligned_loads() {
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
        SIMPLE_ASSERT(std::abs(result[i] - static_cast<float>(i + 1)) < 1e-6f, 
                      "Unaligned load should work correctly");
    }
    
    return true;
}

bool test_simd_vector_double_precision() {
    SimdVectorD a(2.0);
    SimdVectorD b(3.0);
    
    auto sum = a + b;
    auto prod = a * b;
    
    alignas(SimdVectorD::alignment) double result[SimdVectorD::width];
    
    sum.store_aligned(result);
    for (size_t i = 0; i < SimdVectorD::width; ++i) {
        SIMPLE_ASSERT(std::abs(result[i] - 5.0) < 1e-12, "Double precision addition should work");
    }
    
    prod.store_aligned(result);
    for (size_t i = 0; i < SimdVectorD::width; ++i) {
        SIMPLE_ASSERT(std::abs(result[i] - 6.0) < 1e-12, "Double precision multiplication should work");
    }
    
    return true;
}

void benchmark_simd_vector() {
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
        DoNotOptimize(c);
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
        DoNotOptimize(c);
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

bool test_SimdVector() {

    PRINT_HEADER(SIMD VECTOR)

    TestRunner runner;

    RUN_TEST(runner, simd_vector_construction);
    RUN_TEST(runner, simd_vector_arithmetic);
    RUN_TEST(runner, simd_vector_fma);
    RUN_TEST(runner, simd_vector_horizontal_ops);
    RUN_TEST(runner, simd_vector_sqrt);
    RUN_TEST(runner, simd_vector_min_max);
    RUN_TEST(runner, simd_vector_unaligned_loads);
    RUN_TEST(runner, simd_vector_double_precision);

    benchmark_simd_vector();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
