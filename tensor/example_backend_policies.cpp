/**
 * @file example_backend_policies.cpp
 * @brief Example demonstrating clean policy-based backend design.
 */

#include "TensorWithBackend.h"
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

// ============================================================================
// Example 1: Backend as Pure Policy
// ============================================================================

void example1_pure_policy() {
    std::cout << "=== Example 1: Pure Policy-Based Backend ===\n\n";
    
    // Default backend (always available)
    TensorWithBackend<double, StandardAllocatorImpl<double>,
                      SingleThreadedPolicy, DefaultBackendPolicy>
        tensor_default(TensorShape({3, 3}), 1.0);
    
    std::cout << "Backend: " << tensor_default.backend_name() << "\n";
    std::cout << "Has optimizations: " << std::boolalpha 
              << tensor_default.has_optimizations() << "\n\n";
    
    // MKL backend (if available)
    TensorWithBackend<double, StandardAllocatorImpl<double>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        tensor_mkl(TensorShape({3, 3}), 2.0);
    
    std::cout << "Backend: " << tensor_mkl.backend_name() << "\n";
    std::cout << "Has optimizations: " << std::boolalpha 
              << tensor_mkl.has_optimizations() << "\n\n";
}

// ============================================================================
// Example 2: Compile-Time Backend Selection
// ============================================================================

template <typename BackendPolicy>
void compute_with_backend(const char* backend_name) {
    std::cout << "=== Computing with " << backend_name << " ===\n\n";
    
    // Create tensors with specified backend
    TensorWithBackend<double, StandardAllocatorImpl<double>,
                      SingleThreadedPolicy, BackendPolicy>
        A(TensorShape({2, 3}));
    
    A.at(0, 0) = 1.0; A.at(0, 1) = 2.0; A.at(0, 2) = 3.0;
    A.at(1, 0) = 4.0; A.at(1, 1) = 5.0; A.at(1, 2) = 6.0;
    
    TensorWithBackend<double, StandardAllocatorImpl<double>,
                      SingleThreadedPolicy, BackendPolicy>
        B(TensorShape({3, 2}));
    
    B.at(0, 0) = 7.0;  B.at(0, 1) = 8.0;
    B.at(1, 0) = 9.0;  B.at(1, 1) = 10.0;
    B.at(2, 0) = 11.0; B.at(2, 1) = 12.0;
    
    // Matrix multiplication - automatically uses best implementation
    auto C_result = A.matmul(B);
    
    if (C_result.has_value()) {
        print_matrix("Result C = A * B", C_result.value());
    }
    
    std::cout << "Used optimizations: " << std::boolalpha 
              << BackendPolicy::has_optimizations << "\n\n";
}

void example2_compile_time_selection() {
    std::cout << "=== Example 2: Compile-Time Backend Selection ===\n\n";
    
    // Compute with default backend
    compute_with_backend<DefaultBackendPolicy>("Default Backend");
    
    // Compute with MKL backend
    compute_with_backend<MKLBackendPolicy>("MKL Backend");
}

// ============================================================================
// Example 3: Type-Safe Backend Dispatch
// ============================================================================

void example3_type_safe_dispatch() {
    std::cout << "=== Example 3: Type-Safe Backend Dispatch ===\n\n";
    
    // Double: MKL supported
    TensorDoubleMKL tensor_double({10}, 1.0);
    std::cout << "Double tensor:\n";
    std::cout << "  Backend supports type: " << std::boolalpha 
              << tensor_double.backend_supports_type() << "\n";
    std::cout << "  Backend: " << tensor_double.backend_name() << "\n\n";
    
    // Integer: Falls back to default implementation
    TensorWithBackend<int, StandardAllocatorImpl<int>,
                      SingleThreadedPolicy, MKLBackendPolicy>
        tensor_int({10}, 1);
    
    std::cout << "Integer tensor:\n";
    std::cout << "  Backend supports type: " << std::boolalpha 
              << tensor_int.backend_supports_type() << "\n";
    std::cout << "  Will use: Fallback implementation\n\n";
    
    // Matrix multiply works for both (automatic dispatch)
    auto A_int = tensor_int.view(TensorShape({5, 2}));
    auto B_int = tensor_int.view(TensorShape({2, 5}));
    auto C_int = A_int.matmul(B_int);  // Uses default implementation
    
    std::cout << "Integer matmul succeeded: " << std::boolalpha 
              << C_int.has_value() << "\n\n";
}

// ============================================================================
// Example 4: Convenient Type Aliases
// ============================================================================

void example4_convenient_aliases() {
    std::cout << "=== Example 4: Convenient Type Aliases ===\n\n";
    
    // Clean, readable type names
    TensorFloatMKL  tensor_f({100}, 1.0f);
    TensorDoubleMKL tensor_d({100}, 1.0);
    
    std::cout << "TensorFloatMKL backend: " << tensor_f.backend_name() << "\n";
    std::cout << "TensorDoubleMKL backend: " << tensor_d.backend_name() << "\n\n";
    
    // Default backend alias
    TensorDefault<double> tensor_default({100}, 1.0);
    std::cout << "TensorDefault backend: " << tensor_default.backend_name() << "\n\n";
}

// ============================================================================
// Example 5: MKL-Specific Operations
// ============================================================================

void example5_mkl_operations() {
    std::cout << "=== Example 5: MKL-Specific Operations ===\n\n";
    
#if CPP_UTILITIES_HAS_MKL
    TensorDoubleMKL a({5});
    TensorDoubleMKL b({5});
    
    for (size_t i = 0; i < 5; ++i) {
        a[i] = static_cast<double>(i + 1);
        b[i] = static_cast<double>(5 - i);
    }
    
    std::cout << "Vector a: [1, 2, 3, 4, 5]\n";
    std::cout << "Vector b: [5, 4, 3, 2, 1]\n\n";
    
    // Dot product (MKL optimized)
    auto dot_result = a.dot(b);
    if (dot_result.has_value()) {
        std::cout << "Dot product: " << dot_result.value() << "\n";
    }
    
    // Norm (MKL optimized)
    double norm_a = a.norm();
    std::cout << "||a||_2: " << norm_a << "\n\n";
    
    // AXPY: b = 2*a + b (MKL optimized)
    TensorDoubleMKL b_copy = b;
    b_copy.axpy(2.0, a);
    std::cout << "After b = 2*a + b:\n";
    std::cout << "b = [";
    for (size_t i = 0; i < 5; ++i) {
        std::cout << b_copy[i];
        if (i < 4) std::cout << ", ";
    }
    std::cout << "]\n\n";
    
    // Scale: a = 0.5*a (MKL optimized)
    TensorDoubleMKL a_copy = a;
    a_copy.scale(0.5);
    std::cout << "After a = 0.5*a:\n";
    std::cout << "a = [";
    for (size_t i = 0; i < 5; ++i) {
        std::cout << a_copy[i];
        if (i < 4) std::cout << ", ";
    }
    std::cout << "]\n\n";
#else
    std::cout << "MKL not available, skipping MKL-specific operations\n\n";
#endif
}

// ============================================================================
// Example 6: Backend Context (RAII)
// ============================================================================

void example6_backend_context() {
    std::cout << "=== Example 6: Backend Context (RAII) ===\n\n";
    
#if CPP_UTILITIES_HAS_MKL
    std::cout << "Initial MKL threads: " 
              << MKLBackendPolicy::get_num_threads() << "\n\n";
    
    {
        // RAII context for thread control
        MKLContext ctx(2);
        std::cout << "Inside context (2 threads): "
                  << MKLBackendPolicy::get_num_threads() << "\n";
        
        // Operations in this scope use 2 threads
        TensorDoubleMKL A(TensorShape({100, 100}), 1.0);
        TensorDoubleMKL B(TensorShape({100, 100}), 1.0);
        auto C = A.matmul(B);
        std::cout << "Matrix multiply completed with 2 threads\n\n";
    }
    
    std::cout << "After context: "
              << MKLBackendPolicy::get_num_threads() << "\n\n";
#else
    std::cout << "MKL not available, context not applicable\n\n";
#endif
}

// ============================================================================
// Example 7: Policy Composition
// ============================================================================

void example7_policy_composition() {
    std::cout << "=== Example 7: Policy Composition ===\n\n";
    
    // Combine custom allocator + thread safety + MKL backend
    TensorWithBackend<double, 
                      StandardAllocatorImpl<double>,  // Allocator policy
                      MutexSynchronizationPolicy,      // Concurrency policy
                      MKLBackendPolicy>                // Backend policy
        tensor_full(TensorShape({10, 10}), 1.0);
    
    std::cout << "Tensor with all policies:\n";
    std::cout << "  Backend: " << tensor_full.backend_name() << "\n";
    std::cout << "  Thread-safe: Yes (MutexSynchronizationPolicy)\n";
    std::cout << "  Custom allocator: Yes (StandardAllocatorImpl)\n\n";
    
    // Or use convenient alias for thread-safe MKL tensors
    TensorMKLThreadSafe<double> tensor_ts({10, 10}, 1.0);
    std::cout << "TensorMKLThreadSafe backend: " << tensor_ts.backend_name() << "\n\n";
}

// ============================================================================
// Example 8: Performance Comparison
// ============================================================================

void example8_performance_comparison() {
    std::cout << "=== Example 8: Performance Comparison ===\n\n";
    
    const size_t n = 200;
    
    // Default backend
    TensorDefault<double> A_def(TensorShape({n, n}), 1.0);
    TensorDefault<double> B_def(TensorShape({n, n}), 1.0);
    
    auto start_def = std::chrono::high_resolution_clock::now();
    auto C_def = A_def.matmul(B_def);
    auto end_def = std::chrono::high_resolution_clock::now();
    
    auto time_def = std::chrono::duration<double, std::milli>(
        end_def - start_def).count();
    
    std::cout << "Default backend (" << n << "x" << n << "): "
              << time_def << " ms\n";
    
#if CPP_UTILITIES_HAS_MKL
    // MKL backend
    TensorDoubleMKL A_mkl(TensorShape({n, n}), 1.0);
    TensorDoubleMKL B_mkl(TensorShape({n, n}), 1.0);
    
    auto start_mkl = std::chrono::high_resolution_clock::now();
    auto C_mkl = A_mkl.matmul(B_mkl);
    auto end_mkl = std::chrono::high_resolution_clock::now();
    
    auto time_mkl = std::chrono::duration<double, std::milli>(
        end_mkl - start_mkl).count();
    
    std::cout << "MKL backend (" << n << "x" << n << "): "
              << time_mkl << " ms\n";
    std::cout << "Speedup: " << (time_def / time_mkl) << "x\n\n";
#else
    std::cout << "MKL not available for comparison\n\n";
#endif
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << std::fixed << std::setprecision(4);
    
    example1_pure_policy();
    example2_compile_time_selection();
    example3_type_safe_dispatch();
    example4_convenient_aliases();
    example5_mkl_operations();
    example6_backend_context();
    example7_policy_composition();
    example8_performance_comparison();
    
    std::cout << "=== All Examples Complete ===\n";
    
    return 0;
}
