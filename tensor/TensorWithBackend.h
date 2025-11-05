/**
 * @file TensorWithBackend.h
 * @brief Tensor class with backend policy (MKL, etc.).
 *
 * @details Extension of base Tensor class that adds backend policy parameter.
 * Backends provide optimized implementations via policy methods.
 * Falls back to base implementation when backend doesn't support operation.
 *
 * Usage:
 *   Tensor<double, StandardAllocatorImpl<double>, SingleThreadedPolicy, MKLBackendPolicy>
 *
 * @requirements C++17, Tensor.h, TensorBackendPolicies.h
 */
#pragma once

#include "Tensor.h"
#include "TensorBackendPolicies.h"

namespace cpp_utilities {

// ============================================================================
// Tensor with Backend Policy
// ============================================================================

/**
 * @brief Tensor with backend policy for optimized operations.
 * 
 * @tparam T Element type
 * @tparam AllocImpl Allocation policy
 * @tparam ConcurrencyPolicy Thread-safety policy
 * @tparam BackendPolicy Backend for optimized operations (DefaultBackendPolicy, MKLBackendPolicy)
 */
template <typename T, 
          typename AllocImpl = StandardAllocatorImpl<T>,
          typename ConcurrencyPolicy = SingleThreadedPolicy,
          typename BackendPolicy = DefaultBackendPolicy>
class TensorWithBackend : public Tensor<T, AllocImpl, ConcurrencyPolicy> {
public:
    using Base = Tensor<T, AllocImpl, ConcurrencyPolicy>;
    using Backend = BackendPolicy;
    
    // Inherit all base constructors
    using Base::Base;
    
    /**
     * @brief Get backend name.
     */
    static constexpr const char* backend_name() {
        return BackendPolicy::name;
    }
    
    /**
     * @brief Check if backend has optimizations.
     */
    static constexpr bool has_optimizations() {
        return BackendPolicy::has_optimizations;
    }
    
    /**
     * @brief Check if backend supports current type.
     */
    static constexpr bool backend_supports_type() {
        return backend_supports_type_v<BackendPolicy, T>;
    }
    
    // ========================================================================
    // Matrix Operations with Backend Dispatch
    // ========================================================================
    
    /**
     * @brief Matrix multiplication with backend dispatch.
     * 
     * Uses backend if available and type supported, otherwise falls back to base.
     */
    Expected<TensorWithBackend, std::string> matmul(const TensorWithBackend& other) const {
        if (this->rank() != 2 || other.rank() != 2) {
            return make_unexpected(std::string("Matrix multiplication requires 2D tensors"));
        }
        
        if (this->shape()[1] != other.shape()[0]) {
            return make_unexpected(std::string("Incompatible shapes for matrix multiplication"));
        }
        
        // Try backend implementation if supported
        if constexpr (backend_supports_type_v<BackendPolicy, T>) {
            return matmul_backend(other);
        } else {
            // Fall back to base implementation
            return matmul_default(other);
        }
    }
    
private:
    /**
     * @brief Backend-optimized matmul (when backend supports type).
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        backend_supports_type_v<B, T>,
        Expected<TensorWithBackend, std::string>
    >
    matmul_backend(const TensorWithBackend& other) const {
#if CPP_UTILITIES_HAS_MKL
        if constexpr (std::is_same_v<B, MKLBackendPolicy>) {
            MKL_INT m = static_cast<MKL_INT>(this->shape()[0]);
            MKL_INT k = static_cast<MKL_INT>(this->shape()[1]);
            MKL_INT n = static_cast<MKL_INT>(other.shape()[1]);
            
            TensorShape result_shape({static_cast<size_t>(m), static_cast<size_t>(n)});
            TensorWithBackend result(result_shape, T{0});
            
            // Call backend matmul
            BackendPolicy::template matmul<T>(
                this->data(), other.data(),
                m, k, n,
                result.data()
            );
            
            return result;
        }
#endif
        // Fallback if backend not actually available
        return matmul_default(other);
    }
    
    /**
     * @brief Default matmul implementation (uses base class).
     */
    Expected<TensorWithBackend, std::string> matmul_default(const TensorWithBackend& other) const {
        auto result = Base::matmul(static_cast<const Base&>(other));
        if (!result.has_value()) {
            return make_unexpected(result.error());
        }
        
        // Convert base result to TensorWithBackend
        TensorWithBackend backend_result(this->shape());
        backend_result = static_cast<TensorWithBackend&&>(result.value());
        return backend_result;
    }
    
public:
    // ========================================================================
    // Vector Operations (Backend-Optimized)
    // ========================================================================
    
#if CPP_UTILITIES_HAS_MKL
    /**
     * @brief Dot product using backend (MKL only).
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        std::is_same_v<B, MKLBackendPolicy> && backend_supports_type_v<B, T>,
        Expected<T, std::string>
    >
    dot(const TensorWithBackend& other) const {
        if (this->size() != other.size()) {
            return make_unexpected(std::string("Vectors must have same size"));
        }
        
        MKL_INT n = static_cast<MKL_INT>(this->size());
        T result = BackendPolicy::template dot<T>(this->data(), other.data(), n);
        
        return result;
    }
    
    /**
     * @brief Vector norm using backend (MKL only).
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        std::is_same_v<B, MKLBackendPolicy> && backend_supports_type_v<B, T>,
        T
    >
    norm() const {
        MKL_INT n = static_cast<MKL_INT>(this->size());
        return BackendPolicy::template norm<T>(this->data(), n);
    }
    
    /**
     * @brief Scaled vector addition using backend (MKL only).
     * this = alpha * other + this
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        std::is_same_v<B, MKLBackendPolicy> && backend_supports_type_v<B, T>,
        void
    >
    axpy(T alpha, const TensorWithBackend& other) {
        enforce(this->size() == other.size(), "Tensors must have same size");
        
        MKL_INT n = static_cast<MKL_INT>(this->size());
        BackendPolicy::template axpy<T>(n, alpha, other.data(), this->data());
    }
    
    /**
     * @brief Scale tensor using backend (MKL only).
     * this = alpha * this
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        std::is_same_v<B, MKLBackendPolicy> && backend_supports_type_v<B, T>,
        void
    >
    scale(T alpha) {
        MKL_INT n = static_cast<MKL_INT>(this->size());
        BackendPolicy::template scale<T>(n, alpha, this->data());
    }
    
    // ========================================================================
    // Linear Algebra (Backend-Optimized)
    // ========================================================================
    
    /**
     * @brief Solve linear system using backend (MKL only).
     * A*x = b, solution overwrites b.
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        std::is_same_v<B, MKLBackendPolicy> && backend_supports_type_v<B, T>,
        Expected<void, std::string>
    >
    solve(TensorWithBackend& b) {
        if (this->rank() != 2 || b.rank() != 1) {
            return make_unexpected(std::string("A must be 2D, b must be 1D"));
        }
        
        if (this->shape()[0] != this->shape()[1]) {
            return make_unexpected(std::string("A must be square"));
        }
        
        if (this->shape()[0] != b.size()) {
            return make_unexpected(std::string("Dimension mismatch"));
        }
        
        MKL_INT n = static_cast<MKL_INT>(this->shape()[0]);
        
        // Copy A (GESV destroys it)
        TensorWithBackend A_copy = *this;
        
        MKL_INT info = BackendPolicy::template solve<T>(
            n, A_copy.data(), b.data()
        );
        
        if (info != 0) {
            return make_unexpected(std::string("Matrix is singular"));
        }
        
        return {};
    }
    
    /**
     * @brief SVD using backend (MKL only).
     * Returns (U, S, Vt) where A = U * diag(S) * Vt
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        std::is_same_v<B, MKLBackendPolicy> && backend_supports_type_v<B, T>,
        Expected<std::tuple<TensorWithBackend, TensorWithBackend, TensorWithBackend>, std::string>
    >
    svd() const {
        if (this->rank() != 2) {
            return make_unexpected(std::string("SVD requires 2D matrix"));
        }
        
        MKL_INT m = static_cast<MKL_INT>(this->shape()[0]);
        MKL_INT n = static_cast<MKL_INT>(this->shape()[1]);
        MKL_INT min_mn = std::min(m, n);
        
        // Copy A (gesvd destroys it)
        TensorWithBackend A_copy = *this;
        
        TensorWithBackend U(TensorShape({static_cast<size_t>(m), static_cast<size_t>(m)}));
        TensorWithBackend S(TensorShape({static_cast<size_t>(min_mn)}));
        TensorWithBackend Vt(TensorShape({static_cast<size_t>(n), static_cast<size_t>(n)}));
        
        MKL_INT info = BackendPolicy::template svd<T>(
            m, n, A_copy.data(), S.data(), U.data(), Vt.data()
        );
        
        if (info != 0) {
            return make_unexpected(std::string("SVD failed to converge"));
        }
        
        return std::make_tuple(std::move(U), std::move(S), std::move(Vt));
    }
    
    /**
     * @brief Eigendecomposition using backend (MKL only, symmetric matrices).
     * Returns (eigenvalues, eigenvectors).
     */
    template <typename B = BackendPolicy>
    typename std::enable_if_t<
        std::is_same_v<B, MKLBackendPolicy> && backend_supports_type_v<B, T>,
        Expected<std::tuple<TensorWithBackend, TensorWithBackend>, std::string>
    >
    eig() const {
        if (this->rank() != 2) {
            return make_unexpected(std::string("Eigendecomposition requires 2D matrix"));
        }
        
        if (this->shape()[0] != this->shape()[1]) {
            return make_unexpected(std::string("Matrix must be square"));
        }
        
        MKL_INT n = static_cast<MKL_INT>(this->shape()[0]);
        
        // Copy matrix (syev destroys it and stores eigenvectors)
        TensorWithBackend eigvecs = *this;
        TensorWithBackend eigvals(TensorShape({static_cast<size_t>(n)}));
        
        MKL_INT info = BackendPolicy::template eigen_symmetric<T>(
            n, eigvecs.data(), eigvals.data()
        );
        
        if (info != 0) {
            return make_unexpected(std::string("Eigendecomposition failed"));
        }
        
        return std::make_tuple(std::move(eigvals), std::move(eigvecs));
    }
#endif // CPP_UTILITIES_HAS_MKL
};

// ============================================================================
// Convenient Type Aliases
// ============================================================================

// Default backend tensors (same as base Tensor)
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
using TensorDefault = TensorWithBackend<T, AllocImpl, SingleThreadedPolicy, DefaultBackendPolicy>;

// MKL backend tensors
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
using TensorMKL = TensorWithBackend<T, AllocImpl, SingleThreadedPolicy, MKLBackendPolicy>;

// Type-specific aliases
using TensorFloatMKL = TensorMKL<float>;
using TensorDoubleMKL = TensorMKL<double>;

// Thread-safe variants
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
using TensorMKLThreadSafe = TensorWithBackend<T, AllocImpl, MutexSynchronizationPolicy, MKLBackendPolicy>;

} // namespace cpp_utilities
