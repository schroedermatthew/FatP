/**
 * @file example_tensor_mkl.cpp
 * @brief Example usage of Tensor with MKL integration.
 */

#include "TensorMKL.h"
#include <iostream>
#include <iomanip>

using namespace cpp_utilities;

void print_matrix(const char* name, const auto& tensor) {
    std::cout << name << ":\n";
    for (size_t i = 0; i < tensor.shape()[0]; ++i) {
        for (size_t j = 0; j < tensor.shape()[1]; ++j) {
            std::cout << std::setw(10) << std::setprecision(4) 
                      << tensor.at(i, j) << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void print_vector(const char* name, const auto& tensor) {
    std::cout << name << ": [";
    for (size_t i = 0; i < tensor.size(); ++i) {
        std::cout << std::setw(8) << std::setprecision(4) << tensor[i];
        if (i < tensor.size() - 1) std::cout << ", ";
    }
    std::cout << "]\n\n";
}

int main() {
    std::cout << std::fixed << std::setprecision(4);
    
    // ========================================================================
    // Example 1: Basic Matrix Multiplication
    // ========================================================================
    std::cout << "=== Example 1: Matrix Multiplication ===\n\n";
    
    {
        // Create matrices using MKL backend
        TensorDoubleMKL<> A(TensorShape({3, 2}));
        A.at(0, 0) = 1.0; A.at(0, 1) = 2.0;
        A.at(1, 0) = 3.0; A.at(1, 1) = 4.0;
        A.at(2, 0) = 5.0; A.at(2, 1) = 6.0;
        
        TensorDoubleMKL<> B(TensorShape({2, 3}));
        B.at(0, 0) = 7.0;  B.at(0, 1) = 8.0;  B.at(0, 2) = 9.0;
        B.at(1, 0) = 10.0; B.at(1, 1) = 11.0; B.at(1, 2) = 12.0;
        
        print_matrix("Matrix A (3x2)", A);
        print_matrix("Matrix B (2x3)", B);
        
        auto C_result = A.matmul(B);
        if (C_result.has_value()) {
            print_matrix("Matrix C = A * B (3x3)", C_result.value());
        }
    }
    
#if CPP_UTILITIES_HAS_MKL
    // ========================================================================
    // Example 2: Vector Operations
    // ========================================================================
    std::cout << "=== Example 2: Vector Operations (MKL) ===\n\n";
    
    {
        TensorFloatMKL<> v1(TensorShape({5}));
        TensorFloatMKL<> v2(TensorShape({5}));
        
        for (size_t i = 0; i < 5; ++i) {
            v1[i] = static_cast<float>(i + 1);
            v2[i] = static_cast<float>(5 - i);
        }
        
        print_vector("Vector v1", v1);
        print_vector("Vector v2", v2);
        
        // Dot product
        auto dot_result = v1.dot(v2);
        if (dot_result.has_value()) {
            std::cout << "Dot product: " << dot_result.value() << "\n\n";
        }
        
        // Norm
        float norm = v1.norm();
        std::cout << "||v1||_2: " << norm << "\n\n";
        
        // AXPY: v2 = 2.0 * v1 + v2
        v2.axpy(2.0f, v1);
        print_vector("v2 after AXPY (2*v1 + v2)", v2);
    }
    
    // ========================================================================
    // Example 3: Solving Linear Systems
    // ========================================================================
    std::cout << "=== Example 3: Solving Linear System ===\n\n";
    
    {
        // Solve: A*x = b
        // A = [[2, 1], [1, 3]]
        // b = [5, 7]
        // Solution: x = [1, 3]
        
        TensorDoubleMKL<> A(TensorShape({2, 2}));
        A.at(0, 0) = 2.0; A.at(0, 1) = 1.0;
        A.at(1, 0) = 1.0; A.at(1, 1) = 3.0;
        
        TensorDoubleMKL<> b(TensorShape({2}));
        b[0] = 5.0;
        b[1] = 7.0;
        
        print_matrix("Coefficient matrix A", A);
        print_vector("Right-hand side b", b);
        
        auto solve_result = A.solve(b);
        if (solve_result.has_value()) {
            print_vector("Solution x", b);
        } else {
            std::cout << "Error: " << solve_result.error() << "\n";
        }
    }
    
    // ========================================================================
    // Example 4: Singular Value Decomposition
    // ========================================================================
    std::cout << "=== Example 4: SVD ===\n\n";
    
    {
        TensorFloatMKL<> A(TensorShape({3, 2}));
        A.at(0, 0) = 3.0f; A.at(0, 1) = 2.0f;
        A.at(1, 0) = 2.0f; A.at(1, 1) = 3.0f;
        A.at(2, 0) = 2.0f; A.at(2, 1) = 2.0f;
        
        print_matrix("Matrix A (3x2)", A);
        
        auto svd_result = A.svd();
        if (svd_result.has_value()) {
            auto [U, S, Vt] = svd_result.value();
            
            print_vector("Singular values", S);
            print_matrix("Left singular vectors U", U);
            print_matrix("Right singular vectors V^T", Vt);
        }
    }
    
    // ========================================================================
    // Example 5: Eigenvalue Decomposition
    // ========================================================================
    std::cout << "=== Example 5: Eigenvalues ===\n\n";
    
    {
        // Symmetric matrix
        TensorDoubleMKL<> A(TensorShape({3, 3}));
        A.at(0, 0) = 4.0; A.at(0, 1) = 1.0; A.at(0, 2) = 0.0;
        A.at(1, 0) = 1.0; A.at(1, 1) = 3.0; A.at(1, 2) = 1.0;
        A.at(2, 0) = 0.0; A.at(2, 1) = 1.0; A.at(2, 2) = 2.0;
        
        print_matrix("Symmetric matrix A", A);
        
        auto eig_result = A.eig();
        if (eig_result.has_value()) {
            auto [eigvals, eigvecs] = eig_result.value();
            
            print_vector("Eigenvalues", eigvals);
            print_matrix("Eigenvectors", eigvecs);
        }
    }
    
    // ========================================================================
    // Example 6: Threading Control
    // ========================================================================
    std::cout << "=== Example 6: MKL Threading ===\n\n";
    
    {
        std::cout << "Current MKL threads: " 
                  << MKLBackend::get_num_threads() << "\n";
        
        std::cout << "MKL version: " << MKLBackend::version() << "\n\n";
        
        // Create RAII context to control threading
        {
            MKLContext ctx(2);
            std::cout << "Inside context (2 threads): "
                      << MKLBackend::get_num_threads() << "\n";
            
            // Perform computation with 2 threads
            TensorDoubleMKL<> A(TensorShape({100, 100}), 1.0);
            TensorDoubleMKL<> B(TensorShape({100, 100}), 1.0);
            auto C = A.matmul(B);
        }
        
        std::cout << "After context: "
                  << MKLBackend::get_num_threads() << "\n\n";
    }
#else
    std::cout << "\n=== MKL not available ===\n";
    std::cout << "Falling back to default implementations\n\n";
#endif
    
    // ========================================================================
    // Example 7: Backend Selection
    // ========================================================================
    std::cout << "=== Example 7: Backend Selection ===\n\n";
    
    {
        // Default backend (naive implementation)
        TensorMKL<double, StandardAllocatorImpl<double>, 
                  SingleThreadedPolicy, DefaultBackend> 
            A_default(TensorShape({10, 10}), 1.0);
        
        std::cout << "Tensor with DefaultBackend: " 
                  << A_default.backend_name() << "\n";
        
        // MKL backend
        TensorMKL<double, StandardAllocatorImpl<double>, 
                  SingleThreadedPolicy, MKLBackend> 
            A_mkl(TensorShape({10, 10}), 1.0);
        
        std::cout << "Tensor with MKLBackend: " 
                  << A_mkl.backend_name() << "\n\n";
    }
    
    // ========================================================================
    // Example 8: Performance Comparison
    // ========================================================================
    std::cout << "=== Example 8: Performance Comparison ===\n\n";
    
    {
        const size_t n = 200;
        
        // Default implementation
        auto A_std = ones<double>(TensorShape({n, n}));
        auto B_std = ones<double>(TensorShape({n, n}));
        
        auto start_std = std::chrono::high_resolution_clock::now();
        auto C_std = A_std.matmul(B_std);
        auto end_std = std::chrono::high_resolution_clock::now();
        
        auto time_std = std::chrono::duration<double, std::milli>(
            end_std - start_std).count();
        
        std::cout << "Standard matmul (" << n << "x" << n << "): "
                  << time_std << " ms\n";
        
#if CPP_UTILITIES_HAS_MKL
        // MKL implementation
        TensorDoubleMKL<> A_mkl(TensorShape({n, n}), 1.0);
        TensorDoubleMKL<> B_mkl(TensorShape({n, n}), 1.0);
        
        auto start_mkl = std::chrono::high_resolution_clock::now();
        auto C_mkl = A_mkl.matmul(B_mkl);
        auto end_mkl = std::chrono::high_resolution_clock::now();
        
        auto time_mkl = std::chrono::duration<double, std::milli>(
            end_mkl - start_mkl).count();
        
        std::cout << "MKL matmul (" << n << "x" << n << "): "
                  << time_mkl << " ms\n";
        std::cout << "Speedup: " << (time_std / time_mkl) << "x\n\n";
#endif
    }
    
    return 0;
}
