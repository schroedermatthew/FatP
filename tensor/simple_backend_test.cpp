/**
 * @file simple_backend_test.cpp
 * @brief Simple test of backend policies.
 */

#include "TensorWithBackend.h"
#include <iostream>
#include <iomanip>

using namespace cpp_utilities;

int main() {
    std::cout << "=== Backend Policy Test ===\n\n";
    
    // Test 1: Default backend
    std::cout << "Test 1: Default Backend\n";
    TensorWithBackend<double, StandardAllocatorImpl<double>,
                      SingleThreadedPolicy, DefaultBackendPolicy>
        tensor_default(TensorShape({3, 3}), 1.0);
    
    std::cout << "  Backend: " << tensor_default.backend_name() << "\n";
    std::cout << "  Has optimizations: " << tensor_default.has_optimizations() << "\n";
    std::cout << "  Size: " << tensor_default.size() << "\n";
    std::cout << "  ✓ Default backend works\n\n";
    
    // Test 2: MKL backend with double (supported type)
    std::cout << "Test 2: MKL Backend (double)\n";
    TensorDoubleMKL tensor_mkl(TensorShape({3, 3}), 2.0);
    
    std::cout << "  Backend: " << tensor_mkl.backend_name() << "\n";
    std::cout << "  Has optimizations: " << tensor_mkl.has_optimizations() << "\n";
    std::cout << "  Supports type: " << tensor_mkl.backend_supports_type() << "\n";
    std::cout << "  Size: " << tensor_mkl.size() << "\n";
    std::cout << "  ✓ MKL backend works\n\n";
    
    // Test 3: MKL backend with int (falls back automatically)
    std::cout << "Test 3: MKL Backend (int - fallback)\n";
    TensorWithBackend<int, StandardAllocatorImpl<int>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        tensor_int(TensorShape({3, 3}), 5);
    
    std::cout << "  Backend: " << tensor_int.backend_name() << "\n";
    std::cout << "  Supports type: " << tensor_int.backend_supports_type() << "\n";
    std::cout << "  Size: " << tensor_int.size() << "\n";
    std::cout << "  ✓ Automatic fallback works\n\n";
    
    // Test 4: Matrix multiplication with default backend
    std::cout << "Test 4: Matrix Multiplication (Default)\n";
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
    
    auto C_result = A.matmul(B);
    if (!C_result.has_value()) {
        std::cerr << "  ✗ Matmul failed: " << C_result.error() << "\n";
        return 1;
    }
    
    auto& C = C_result.value();
    std::cout << "  Result C[0,0] = " << C.at(0, 0) << " (expected 58)\n";
    std::cout << "  Result C[1,1] = " << C.at(1, 1) << " (expected 154)\n";
    
    if (std::abs(C.at(0, 0) - 58.0) < 1e-10 && 
        std::abs(C.at(1, 1) - 154.0) < 1e-10) {
        std::cout << "  ✓ Matrix multiplication works\n\n";
    } else {
        std::cout << "  ✗ Results incorrect\n\n";
        return 1;
    }
    
    // Test 5: Matrix multiplication with MKL backend
    std::cout << "Test 5: Matrix Multiplication (MKL)\n";
    TensorDoubleMKL A_mkl(TensorShape({2, 3}));
    A_mkl.at(0, 0) = 1.0; A_mkl.at(0, 1) = 2.0; A_mkl.at(0, 2) = 3.0;
    A_mkl.at(1, 0) = 4.0; A_mkl.at(1, 1) = 5.0; A_mkl.at(1, 2) = 6.0;
    
    TensorDoubleMKL B_mkl(TensorShape({3, 2}));
    B_mkl.at(0, 0) = 7.0;  B_mkl.at(0, 1) = 8.0;
    B_mkl.at(1, 0) = 9.0;  B_mkl.at(1, 1) = 10.0;
    B_mkl.at(2, 0) = 11.0; B_mkl.at(2, 1) = 12.0;
    
    auto C_mkl_result = A_mkl.matmul(B_mkl);
    if (!C_mkl_result.has_value()) {
        std::cerr << "  ✗ MKL Matmul failed: " << C_mkl_result.error() << "\n";
        return 1;
    }
    
    auto& C_mkl = C_mkl_result.value();
    std::cout << "  Result C[0,0] = " << C_mkl.at(0, 0) << " (expected 58)\n";
    std::cout << "  Result C[1,1] = " << C_mkl.at(1, 1) << " (expected 154)\n";
    
    if (std::abs(C_mkl.at(0, 0) - 58.0) < 1e-10 && 
        std::abs(C_mkl.at(1, 1) - 154.0) < 1e-10) {
        std::cout << "  ✓ MKL matrix multiplication works\n\n";
    } else {
        std::cout << "  ✗ Results incorrect\n\n";
        return 1;
    }
    
    // Test 6: Integer matmul with MKL backend (should fallback)
    std::cout << "Test 6: Integer Matrix Multiplication (MKL fallback)\n";
    TensorWithBackend<int, StandardAllocatorImpl<int>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        A_int(TensorShape({2, 2}), 2);
    
    TensorWithBackend<int, StandardAllocatorImpl<int>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        B_int(TensorShape({2, 2}), 3);
    
    auto C_int_result = A_int.matmul(B_int);
    if (!C_int_result.has_value()) {
        std::cerr << "  ✗ Integer matmul failed: " << C_int_result.error() << "\n";
        return 1;
    }
    
    auto& C_int = C_int_result.value();
    std::cout << "  Result C[0,0] = " << C_int.at(0, 0) << " (expected 12)\n";
    
    if (C_int.at(0, 0) == 12) {
        std::cout << "  ✓ Integer fallback works\n\n";
    } else {
        std::cout << "  ✗ Results incorrect\n\n";
        return 1;
    }
    
    std::cout << "=== All Tests Passed! ===\n";
    return 0;
}
