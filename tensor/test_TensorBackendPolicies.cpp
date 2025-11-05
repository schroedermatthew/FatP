/**
 * @file test_TensorBackendPolicies.cpp
 * @brief Tests for tensor backend policies.
 */

#include "TensorWithBackend.h"
#include "test_Utilities.h"
#include <cmath>
#include <iostream>

namespace cpp_utilities {
namespace testing {

// ============================================================================
// Backend Policy Tests
// ============================================================================

bool test_backend_traits() {
    // Check default backend
    ASSERT_TRUE((backend_supports_type_v<DefaultBackendPolicy, int>), 
                "Default backend should support int");
    ASSERT_TRUE((backend_supports_type_v<DefaultBackendPolicy, double>), 
                "Default backend should support double");
    
    // Check MKL backend type support
#if CPP_UTILITIES_HAS_MKL
    ASSERT_TRUE((backend_supports_type_v<MKLBackendPolicy, float>),
                "MKL should support float");
    ASSERT_TRUE((backend_supports_type_v<MKLBackendPolicy, double>),
                "MKL should support double");
    ASSERT_FALSE((backend_supports_type_v<MKLBackendPolicy, int>),
                 "MKL should not support int");
#endif
    
    return true;
}

bool test_backend_names() {
    ASSERT_EQ(std::string(DefaultBackendPolicy::name), std::string("Default"),
              "Default backend name should be correct");
    
    std::cout << "Default backend: " << DefaultBackendPolicy::name << std::endl;
    std::cout << "MKL backend: " << MKLBackendPolicy::name << std::endl;
    
    return true;
}

bool test_backend_initialization() {
    // Test default backend
    DefaultBackendPolicy::initialize();
    std::string ver = DefaultBackendPolicy::version();
    ASSERT_FALSE(ver.empty(), "Default backend should have version");
    
#if CPP_UTILITIES_HAS_MKL
    // Test MKL backend
    MKLBackendPolicy::initialize(2);
    ASSERT_EQ(MKLBackendPolicy::get_num_threads(), 2, 
              "MKL should use 2 threads");
    
    std::string mkl_ver = MKLBackendPolicy::version();
    std::cout << "MKL version: " << mkl_ver << std::endl;
    ASSERT_FALSE(mkl_ver.empty(), "MKL version should not be empty");
#endif
    
    return true;
}

bool test_backend_context() {
#if CPP_UTILITIES_HAS_MKL
    int original = MKLBackendPolicy::get_num_threads();
    
    {
        MKLContext ctx(4);
        ASSERT_EQ(MKLBackendPolicy::get_num_threads(), 4,
                  "Context should set threads to 4");
    }
    
    ASSERT_EQ(MKLBackendPolicy::get_num_threads(), original,
              "Context should restore original threads");
#else
    std::cout << "MKL context test skipped (MKL not available)" << std::endl;
#endif
    
    return true;
}

// ============================================================================
// TensorWithBackend Tests
// ============================================================================

bool test_tensor_backend_default() {
    // Create tensor with default backend
    TensorWithBackend<int, StandardAllocatorImpl<int>, 
                      SingleThreadedPolicy, DefaultBackendPolicy> 
        tensor(TensorShape({3, 4}), 42);
    
    ASSERT_EQ(tensor.backend_name(), std::string("Default"),
              "Backend name should be Default");
    ASSERT_FALSE(tensor.has_optimizations(), 
                 "Default backend has no optimizations");
    ASSERT_TRUE(tensor.backend_supports_type(),
                "Default backend supports all types");
    
    return true;
}

bool test_tensor_backend_mkl() {
    // Create tensor with MKL backend
    TensorMKL<double> tensor(TensorShape({3, 4}), 1.0);
    
    std::cout << "Tensor backend: " << tensor.backend_name() << std::endl;
    
#if CPP_UTILITIES_HAS_MKL
    ASSERT_TRUE(tensor.has_optimizations(),
                "MKL backend should have optimizations");
    ASSERT_TRUE(tensor.backend_supports_type(),
                "MKL backend should support double");
#else
    ASSERT_FALSE(tensor.has_optimizations(),
                 "MKL backend unavailable, no optimizations");
#endif
    
    return true;
}

bool test_tensor_backend_int_fallback() {
    // Integer tensor with MKL backend should use fallback
    TensorWithBackend<int, StandardAllocatorImpl<int>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        tensor(TensorShape({3, 4}), 1);
    
    ASSERT_FALSE(tensor.backend_supports_type(),
                 "MKL backend should not support int");
    
    return true;
}

// ============================================================================
// Matrix Multiplication Tests
// ============================================================================

bool test_matmul_default_backend() {
    TensorWithBackend<double, StandardAllocatorImpl<double>,
                      SingleThreadedPolicy, DefaultBackendPolicy>
        A(TensorShape({2, 3}));
    
    A.at(0, 0) = 1.0; A.at(0, 1) = 2.0; A.at(0, 2) = 3.0;
    A.at(1, 0) = 4.0; A.at(1, 1) = 5.0; A.at(1, 2) = 6.0;
    
    TensorWithBackend<double, StandardAllocatorImpl<double>,
                      SingleThreadedPolicy, DefaultBackendPolicy>
        B(TensorShape({3, 2}));
    
    B.at(0, 0) = 7.0;  B.at(0, 1) = 8.0;
    B.at(1, 0) = 9.0;  B.at(1, 1) = 10.0;
    B.at(2, 0) = 11.0; B.at(2, 1) = 12.0;
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "Matmul should succeed");
    
    auto& C = result.value();
    
    // C[0,0] = 1*7 + 2*9 + 3*11 = 58
    ASSERT_TRUE(std::abs(C.at(0, 0) - 58.0) < 1e-10, "Result should be correct");
    
    return true;
}

bool test_matmul_mkl_backend() {
    TensorDoubleMKL A(TensorShape({2, 3}));
    A.at(0, 0) = 1.0; A.at(0, 1) = 2.0; A.at(0, 2) = 3.0;
    A.at(1, 0) = 4.0; A.at(1, 1) = 5.0; A.at(1, 2) = 6.0;
    
    TensorDoubleMKL B(TensorShape({3, 2}));
    B.at(0, 0) = 7.0;  B.at(0, 1) = 8.0;
    B.at(1, 0) = 9.0;  B.at(1, 1) = 10.0;
    B.at(2, 0) = 11.0; B.at(2, 1) = 12.0;
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "MKL matmul should succeed");
    
    auto& C = result.value();
    
    // Verify results
    ASSERT_TRUE(std::abs(C.at(0, 0) - 58.0) < 1e-10, "C[0,0] should be correct");
    ASSERT_TRUE(std::abs(C.at(0, 1) - 64.0) < 1e-10, "C[0,1] should be correct");
    ASSERT_TRUE(std::abs(C.at(1, 0) - 139.0) < 1e-10, "C[1,0] should be correct");
    ASSERT_TRUE(std::abs(C.at(1, 1) - 154.0) < 1e-10, "C[1,1] should be correct");
    
    return true;
}

bool test_matmul_int_fallback() {
    // Integer tensors should use fallback even with MKL backend
    TensorWithBackend<int, StandardAllocatorImpl<int>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        A(TensorShape({2, 2}), 2);
    
    TensorWithBackend<int, StandardAllocatorImpl<int>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        B(TensorShape({2, 2}), 3);
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "Int matmul should use fallback");
    
    auto& C = result.value();
    // 2x2 matrix of 2s times 2x2 matrix of 3s = 2x2 matrix of 12s
    ASSERT_EQ(C.at(0, 0), 12, "Fallback result should be correct");
    
    return true;
}

// ============================================================================
// Vector Operations Tests (MKL Only)
// ============================================================================

#if CPP_UTILITIES_HAS_MKL

bool test_dot_product_mkl() {
    TensorFloatMKL a(TensorShape({5}));
    TensorFloatMKL b(TensorShape({5}));
    
    for (size_t i = 0; i < 5; ++i) {
        a[i] = static_cast<float>(i + 1);
        b[i] = static_cast<float>(i + 1);
    }
    
    auto result = a.dot(b);
    ASSERT_TRUE(result.has_value(), "Dot product should succeed");
    
    // 1*1 + 2*2 + 3*3 + 4*4 + 5*5 = 55
    ASSERT_TRUE(std::abs(result.value() - 55.0f) < 1e-5f,
                "Dot product should be 55");
    
    return true;
}

bool test_norm_mkl() {
    TensorDoubleMKL v(TensorShape({4}));
    v[0] = 3.0; v[1] = 4.0; v[2] = 0.0; v[3] = 0.0;
    
    double norm = v.norm();
    ASSERT_TRUE(std::abs(norm - 5.0) < 1e-10, "Norm should be 5.0");
    
    return true;
}

bool test_axpy_mkl() {
    TensorFloatMKL x(TensorShape({3}), 1.0f);
    TensorFloatMKL y(TensorShape({3}), 2.0f);
    
    // y = 3*x + y = 3 + 2 = 5
    y.axpy(3.0f, x);
    
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_TRUE(std::abs(y[i] - 5.0f) < 1e-5f, "AXPY result should be 5");
    }
    
    return true;
}

bool test_scale_mkl() {
    TensorDoubleMKL v(TensorShape({4}));
    for (size_t i = 0; i < 4; ++i) {
        v[i] = static_cast<double>(i + 1);
    }
    
    v.scale(2.0);
    
    for (size_t i = 0; i < 4; ++i) {
        ASSERT_TRUE(std::abs(v[i] - static_cast<double>((i + 1) * 2)) < 1e-10,
                    "Scale result should be correct");
    }
    
    return true;
}

#endif // CPP_UTILITIES_HAS_MKL

// ============================================================================
// Performance Benchmarks
// ============================================================================

bool test_matmul_backend_comparison() {
    const size_t n = 200;
    
    std::cout << "\nMatrix multiplication (" << n << "x" << n << "):\n";
    
    // Default backend
    TensorDefault<double> A_def(TensorShape({n, n}), 1.0);
    TensorDefault<double> B_def(TensorShape({n, n}), 1.0);
    
    benchmark("Default backend", [&]() {
        auto C = A_def.matmul(B_def);
    }, 3);
    
    // MKL backend
    TensorDoubleMKL A_mkl(TensorShape({n, n}), 1.0);
    TensorDoubleMKL B_mkl(TensorShape({n, n}), 1.0);
    
    benchmark("MKL backend", [&]() {
        auto C = A_mkl.matmul(B_mkl);
    }, 3);
    
    return true;
}

// ============================================================================
// Test Runner
// ============================================================================

int run_all_backend_policy_tests() {
    TestRunner runner;
    
    std::cout << "\n" << colors::bold() << colors::cyan() 
              << "========================================\n"
              << "    BACKEND POLICY TESTS               \n"
              << "========================================\n" 
              << colors::reset() << std::endl;
    
    // Backend policy tests
    std::cout << colors::blue() << "\n[Backend Policy Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, backend_traits);
    RUN_TEST(runner, backend_names);
    RUN_TEST(runner, backend_initialization);
    RUN_TEST(runner, backend_context);
    
    // TensorWithBackend tests
    std::cout << colors::blue() << "\n[TensorWithBackend Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, tensor_backend_default);
    RUN_TEST(runner, tensor_backend_mkl);
    RUN_TEST(runner, tensor_backend_int_fallback);
    
    // Matrix operations
    std::cout << colors::blue() << "\n[Matrix Operations]\n" 
              << colors::reset();
    RUN_TEST(runner, matmul_default_backend);
    RUN_TEST(runner, matmul_mkl_backend);
    RUN_TEST(runner, matmul_int_fallback);
    
#if CPP_UTILITIES_HAS_MKL
    // Vector operations (MKL only)
    std::cout << colors::blue() << "\n[Vector Operations (MKL)]\n" 
              << colors::reset();
    RUN_TEST(runner, dot_product_mkl);
    RUN_TEST(runner, norm_mkl);
    RUN_TEST(runner, axpy_mkl);
    RUN_TEST(runner, scale_mkl);
#endif
    
    // Performance
    std::cout << colors::blue() << "\n[Performance Benchmarks]\n" 
              << colors::reset();
    RUN_TEST(runner, matmul_backend_comparison);
    
    return runner.print_summary();
}

} // namespace testing
} // namespace cpp_utilities

int main() {
#if CPP_UTILITIES_HAS_MKL
    cpp_utilities::MKLContext ctx(4);
    std::cout << "MKL initialized with " 
              << cpp_utilities::MKLBackendPolicy::get_num_threads() 
              << " threads\n";
#else
    std::cout << "MKL not available, using default backend\n";
#endif
    
    return cpp_utilities::testing::run_all_backend_policy_tests();
}
