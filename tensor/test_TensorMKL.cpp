/**
 * @file test_TensorMKL.cpp
 * @brief Tests for MKL-accelerated tensor operations.
 */

#include "TensorMKL.h"
#include "test_Utilities.h"
#include <cmath>
#include <iostream>

namespace cpp_utilities {
namespace testing {

// ============================================================================
// Backend Detection Tests
// ============================================================================

bool test_mkl_backend_detection() {
#if CPP_UTILITIES_HAS_MKL
    ASSERT_TRUE(MKLBackend::use_mkl, "MKL backend should be available");
    
    // Test version info
    std::string version = MKLBackend::version();
    ASSERT_TRUE(!version.empty(), "MKL version string should not be empty");
    
    std::cout << "MKL Version: " << version << std::endl;
#else
    ASSERT_FALSE(MKLBackend::use_mkl, "MKL backend should not be available");
    std::cout << "MKL not available, using fallback implementations" << std::endl;
#endif
    
    return true;
}

bool test_mkl_threading() {
#if CPP_UTILITIES_HAS_MKL
    int original_threads = MKLBackend::get_num_threads();
    std::cout << "Original MKL threads: " << original_threads << std::endl;
    
    // Test setting threads
    MKLBackend::set_num_threads(2);
    ASSERT_EQ(MKLBackend::get_num_threads(), 2, "Thread count should be 2");
    
    MKLBackend::set_num_threads(4);
    ASSERT_EQ(MKLBackend::get_num_threads(), 4, "Thread count should be 4");
    
    // Restore
    MKLBackend::set_num_threads(original_threads);
#else
    std::cout << "MKL threading not available" << std::endl;
#endif
    
    return true;
}

bool test_mkl_context_raii() {
#if CPP_UTILITIES_HAS_MKL
    int original = MKLBackend::get_num_threads();
    
    {
        MKLContext ctx(2);
        ASSERT_EQ(MKLBackend::get_num_threads(), 2, 
                  "Context should set thread count");
    }
    
    ASSERT_EQ(MKLBackend::get_num_threads(), original,
              "Context destructor should restore thread count");
#else
    std::cout << "MKL context not available" << std::endl;
#endif
    
    return true;
}

// ============================================================================
// Type Traits Tests
// ============================================================================

bool test_mkl_type_support() {
#if CPP_UTILITIES_HAS_MKL
    ASSERT_TRUE(is_mkl_supported_v<float>, "float should be supported");
    ASSERT_TRUE(is_mkl_supported_v<double>, "double should be supported");
    ASSERT_FALSE(is_mkl_supported_v<int>, "int should not be supported");
    ASSERT_FALSE(is_mkl_supported_v<long>, "long should not be supported");
#else
    std::cout << "MKL type support traits not available" << std::endl;
#endif
    
    return true;
}

// ============================================================================
// Matrix Multiplication Tests
// ============================================================================

bool test_mkl_matmul_float() {
    // Create 2x3 matrix A
    TensorMKL<float, StandardAllocatorImpl<float>, SingleThreadedPolicy, MKLBackend> 
        A(TensorShape({2, 3}));
    A.at(0, 0) = 1.0f; A.at(0, 1) = 2.0f; A.at(0, 2) = 3.0f;
    A.at(1, 0) = 4.0f; A.at(1, 1) = 5.0f; A.at(1, 2) = 6.0f;
    
    // Create 3x2 matrix B
    TensorMKL<float, StandardAllocatorImpl<float>, SingleThreadedPolicy, MKLBackend> 
        B(TensorShape({3, 2}));
    B.at(0, 0) = 7.0f;  B.at(0, 1) = 8.0f;
    B.at(1, 0) = 9.0f;  B.at(1, 1) = 10.0f;
    B.at(2, 0) = 11.0f; B.at(2, 1) = 12.0f;
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "Matmul should succeed");
    
    auto& C = result.value();
    
    // Verify results
    // C[0,0] = 1*7 + 2*9 + 3*11 = 58
    float expected_00 = 58.0f;
    ASSERT_TRUE(std::abs(C.at(0, 0) - expected_00) < 1e-5f, 
                "C[0,0] should be correct");
    
    // C[0,1] = 1*8 + 2*10 + 3*12 = 64
    float expected_01 = 64.0f;
    ASSERT_TRUE(std::abs(C.at(0, 1) - expected_01) < 1e-5f,
                "C[0,1] should be correct");
    
    // C[1,0] = 4*7 + 5*9 + 6*11 = 139
    float expected_10 = 139.0f;
    ASSERT_TRUE(std::abs(C.at(1, 0) - expected_10) < 1e-5f,
                "C[1,0] should be correct");
    
    // C[1,1] = 4*8 + 5*10 + 6*12 = 154
    float expected_11 = 154.0f;
    ASSERT_TRUE(std::abs(C.at(1, 1) - expected_11) < 1e-5f,
                "C[1,1] should be correct");
    
    return true;
}

bool test_mkl_matmul_double() {
    TensorDoubleMKL<> A(TensorShape({3, 3}));
    TensorDoubleMKL<> B(TensorShape({3, 3}));
    
    // Identity matrix
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            A.at(i, j) = (i == j) ? 1.0 : 0.0;
            B.at(i, j) = static_cast<double>(i * 3 + j);
        }
    }
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "Matmul should succeed");
    
    auto& C = result.value();
    
    // A*B should equal B (since A is identity)
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            ASSERT_TRUE(std::abs(C.at(i, j) - B.at(i, j)) < 1e-10,
                        "Identity multiplication should preserve B");
        }
    }
    
    return true;
}

bool test_mkl_large_matmul() {
    size_t n = 100;
    
    auto A = ones<double>(TensorShape({n, n}));
    auto B = ones<double>(TensorShape({n, n}));
    
    // Convert to MKL tensors
    TensorDoubleMKL<> A_mkl(TensorShape({n, n}), 1.0);
    TensorDoubleMKL<> B_mkl(TensorShape({n, n}), 1.0);
    
    auto result = A_mkl.matmul(B_mkl);
    ASSERT_TRUE(result.has_value(), "Large matmul should succeed");
    
    auto& C = result.value();
    
    // All elements should be n (since we're multiplying matrices of 1s)
    double expected = static_cast<double>(n);
    bool all_correct = true;
    for (size_t i = 0; i < n && all_correct; ++i) {
        for (size_t j = 0; j < n && all_correct; ++j) {
            if (std::abs(C.at(i, j) - expected) > 1e-8) {
                all_correct = false;
            }
        }
    }
    
    ASSERT_TRUE(all_correct, "Large matmul results should be correct");
    
    return true;
}

// ============================================================================
// Vector Operations Tests
// ============================================================================

#if CPP_UTILITIES_HAS_MKL

bool test_mkl_dot_product() {
    TensorFloatMKL<> a(TensorShape({5}));
    TensorFloatMKL<> b(TensorShape({5}));
    
    for (size_t i = 0; i < 5; ++i) {
        a[i] = static_cast<float>(i + 1);  // [1, 2, 3, 4, 5]
        b[i] = static_cast<float>(i + 1);  // [1, 2, 3, 4, 5]
    }
    
    auto result = a.dot(b);
    ASSERT_TRUE(result.has_value(), "Dot product should succeed");
    
    // 1*1 + 2*2 + 3*3 + 4*4 + 5*5 = 55
    float expected = 55.0f;
    ASSERT_TRUE(std::abs(result.value() - expected) < 1e-5f,
                "Dot product should be correct");
    
    return true;
}

bool test_mkl_norm() {
    TensorDoubleMKL<> v(TensorShape({4}));
    v[0] = 3.0;
    v[1] = 4.0;
    v[2] = 0.0;
    v[3] = 0.0;
    
    double norm = v.norm();
    
    // sqrt(3^2 + 4^2) = 5
    ASSERT_TRUE(std::abs(norm - 5.0) < 1e-10, "Norm should be 5.0");
    
    return true;
}

bool test_mkl_axpy() {
    TensorFloatMKL<> x(TensorShape({3}), 1.0f);
    TensorFloatMKL<> y(TensorShape({3}), 2.0f);
    
    // y = 3*x + y = 3*1 + 2 = 5
    y.axpy(3.0f, x);
    
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_TRUE(std::abs(y[i] - 5.0f) < 1e-5f, "AXPY result should be 5.0");
    }
    
    return true;
}

bool test_mkl_scale() {
    TensorDoubleMKL<> v(TensorShape({4}));
    for (size_t i = 0; i < 4; ++i) {
        v[i] = static_cast<double>(i + 1);  // [1, 2, 3, 4]
    }
    
    v.scale(2.0);
    
    // Should be [2, 4, 6, 8]
    for (size_t i = 0; i < 4; ++i) {
        double expected = static_cast<double>((i + 1) * 2);
        ASSERT_TRUE(std::abs(v[i] - expected) < 1e-10, "Scale result should be correct");
    }
    
    return true;
}

// ============================================================================
// Linear Algebra Tests
// ============================================================================

bool test_mkl_solve() {
    // Solve: A*x = b where A = [[3, 2], [1, 4]]
    TensorDoubleMKL<> A(TensorShape({2, 2}));
    A.at(0, 0) = 3.0; A.at(0, 1) = 2.0;
    A.at(1, 0) = 1.0; A.at(1, 1) = 4.0;
    
    TensorDoubleMKL<> b(TensorShape({2}));
    b[0] = 13.0;  // 3*3 + 2*2
    b[1] = 11.0;  // 1*3 + 4*2
    
    // Expected solution: x = [3, 2]
    auto result = A.solve(b);
    ASSERT_TRUE(result.has_value(), "Solve should succeed");
    
    // Check solution
    ASSERT_TRUE(std::abs(b[0] - 3.0) < 1e-8, "x[0] should be 3.0");
    ASSERT_TRUE(std::abs(b[1] - 2.0) < 1e-8, "x[1] should be 2.0");
    
    return true;
}

bool test_mkl_svd() {
    // Simple 2x2 matrix
    TensorFloatMKL<> A(TensorShape({2, 2}));
    A.at(0, 0) = 3.0f; A.at(0, 1) = 0.0f;
    A.at(1, 0) = 0.0f; A.at(1, 1) = 2.0f;
    
    auto result = A.svd();
    ASSERT_TRUE(result.has_value(), "SVD should succeed");
    
    auto [U, S, Vt] = result.value();
    
    // For diagonal matrix, singular values are diagonal elements
    ASSERT_TRUE(std::abs(S[0] - 3.0f) < 1e-5f || std::abs(S[1] - 3.0f) < 1e-5f,
                "Singular value 3.0 should be present");
    ASSERT_TRUE(std::abs(S[0] - 2.0f) < 1e-5f || std::abs(S[1] - 2.0f) < 1e-5f,
                "Singular value 2.0 should be present");
    
    return true;
}

bool test_mkl_eigenvalues() {
    // Symmetric matrix with known eigenvalues
    TensorDoubleMKL<> A(TensorShape({2, 2}));
    A.at(0, 0) = 2.0; A.at(0, 1) = 1.0;
    A.at(1, 0) = 1.0; A.at(1, 1) = 2.0;
    
    auto result = A.eig();
    ASSERT_TRUE(result.has_value(), "Eigendecomposition should succeed");
    
    auto [eigvals, eigvecs] = result.value();
    
    // Eigenvalues should be 1.0 and 3.0
    ASSERT_TRUE(eigvals.size() == 2, "Should have 2 eigenvalues");
    
    // Check eigenvalues (in ascending order)
    ASSERT_TRUE(std::abs(eigvals[0] - 1.0) < 1e-8, "First eigenvalue should be 1.0");
    ASSERT_TRUE(std::abs(eigvals[1] - 3.0) < 1e-8, "Second eigenvalue should be 3.0");
    
    return true;
}

#endif // CPP_UTILITIES_HAS_MKL

// ============================================================================
// Performance Benchmarks
// ============================================================================

bool test_mkl_matmul_performance() {
    size_t n = 500;
    
    std::cout << "\nMatrix multiplication benchmark (" << n << "x" << n << "):\n";
    
    // Standard tensor
    auto A_std = ones<double>(TensorShape({n, n}));
    auto B_std = ones<double>(TensorShape({n, n}));
    
    benchmark("Standard matmul", [&A_std, &B_std]() {
        auto C = A_std.matmul(B_std);
    }, 3);
    
#if CPP_UTILITIES_HAS_MKL
    // MKL tensor
    TensorDoubleMKL<> A_mkl(TensorShape({n, n}), 1.0);
    TensorDoubleMKL<> B_mkl(TensorShape({n, n}), 1.0);
    
    benchmark("MKL matmul", [&A_mkl, &B_mkl]() {
        auto C = A_mkl.matmul(B_mkl);
    }, 3);
#else
    std::cout << "MKL matmul benchmark skipped (MKL not available)\n";
#endif
    
    return true;
}

bool test_mkl_vector_ops_performance() {
    size_t n = 10000000;  // 10M elements
    
    std::cout << "\nVector operations benchmark (" << n << " elements):\n";
    
#if CPP_UTILITIES_HAS_MKL
    TensorDoubleMKL<> a(TensorShape({n}), 1.0);
    TensorDoubleMKL<> b(TensorShape({n}), 2.0);
    
    benchmark("MKL dot product", [&a, &b]() {
        volatile auto result = a.dot(b);
        (void)result;
    }, 10);
    
    benchmark("MKL norm", [&a]() {
        volatile auto result = a.norm();
        (void)result;
    }, 10);
    
    benchmark("MKL axpy", [&a, &b]() {
        TensorDoubleMKL<> c = b;
        c.axpy(1.5, a);
    }, 10);
#else
    std::cout << "MKL vector benchmarks skipped (MKL not available)\n";
#endif
    
    return true;
}

// ============================================================================
// Integration Tests
// ============================================================================

bool test_mkl_with_allocators() {
    // Test that MKL works with different allocators
    TensorMKL<double, StandardAllocatorImpl<double>, SingleThreadedPolicy, MKLBackend>
        A(TensorShape({3, 3}), 1.0);
    
    TensorMKL<double, StandardAllocatorImpl<double>, SingleThreadedPolicy, MKLBackend>
        B(TensorShape({3, 3}), 2.0);
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "Matmul with standard allocator should work");
    
    return true;
}

bool test_mkl_with_concurrency() {
    // Test thread-safe MKL tensors
    TensorMKLThreadSafe<double> A(TensorShape({10, 10}), 1.0);
    TensorMKLThreadSafe<double> B(TensorShape({10, 10}), 2.0);
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "Thread-safe MKL matmul should work");
    
    return true;
}

bool test_mkl_fallback_for_int() {
    // Integer tensors should use fallback implementation
    TensorMKL<int, StandardAllocatorImpl<int>, SingleThreadedPolicy, MKLBackend>
        A(TensorShape({2, 2}), 1);
    
    TensorMKL<int, StandardAllocatorImpl<int>, SingleThreadedPolicy, MKLBackend>
        B(TensorShape({2, 2}), 2);
    
    auto result = A.matmul(B);
    ASSERT_TRUE(result.has_value(), "Integer matmul should use fallback");
    
    // Result should be all 4s (2x2 matrix of 1s times 2x2 matrix of 2s)
    auto& C = result.value();
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            ASSERT_EQ(C.at(i, j), 4, "Fallback matmul result should be correct");
        }
    }
    
    return true;
}

// ============================================================================
// Test Runner
// ============================================================================

int run_all_mkl_tests() {
    TestRunner runner;
    
    std::cout << "\n" << colors::bold() << colors::cyan() 
              << "========================================\n"
              << "      TENSOR MKL INTEGRATION TESTS      \n"
              << "========================================\n" 
              << colors::reset() << std::endl;
    
    // Backend detection
    std::cout << colors::blue() << "\n[Backend Detection Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, mkl_backend_detection);
    RUN_TEST(runner, mkl_threading);
    RUN_TEST(runner, mkl_context_raii);
    RUN_TEST(runner, mkl_type_support);
    
    // Matrix operations
    std::cout << colors::blue() << "\n[Matrix Multiplication Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, mkl_matmul_float);
    RUN_TEST(runner, mkl_matmul_double);
    RUN_TEST(runner, mkl_large_matmul);
    
#if CPP_UTILITIES_HAS_MKL
    // Vector operations (only if MKL available)
    std::cout << colors::blue() << "\n[Vector Operations Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, mkl_dot_product);
    RUN_TEST(runner, mkl_norm);
    RUN_TEST(runner, mkl_axpy);
    RUN_TEST(runner, mkl_scale);
    
    // Linear algebra (only if MKL available)
    std::cout << colors::blue() << "\n[Linear Algebra Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, mkl_solve);
    RUN_TEST(runner, mkl_svd);
    RUN_TEST(runner, mkl_eigenvalues);
#endif
    
    // Integration tests
    std::cout << colors::blue() << "\n[Integration Tests]\n" 
              << colors::reset();
    RUN_TEST(runner, mkl_with_allocators);
    RUN_TEST(runner, mkl_with_concurrency);
    RUN_TEST(runner, mkl_fallback_for_int);
    
    // Performance benchmarks
    std::cout << colors::blue() << "\n[Performance Benchmarks]\n" 
              << colors::reset();
    RUN_TEST(runner, mkl_matmul_performance);
    RUN_TEST(runner, mkl_vector_ops_performance);
    
    return runner.print_summary();
}

} // namespace testing
} // namespace cpp_utilities

// Main function for standalone testing
int main() {
#if CPP_UTILITIES_HAS_MKL
    // Initialize MKL with 4 threads
    cpp_utilities::MKLContext ctx(4);
    std::cout << "MKL initialized with " 
              << cpp_utilities::MKLBackend::get_num_threads() 
              << " threads\n";
#else
    std::cout << "MKL not available, running with fallback implementations\n";
#endif
    
    return cpp_utilities::testing::run_all_mkl_tests();
}
