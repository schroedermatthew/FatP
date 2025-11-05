/**
 * @file TensorBackendPolicies.h
 * @brief Backend policies for tensor operations (default, MKL, etc.).
 *
 * @details Provides policy-based backend selection for tensor operations:
 * - Compile-time backend selection
 * - Zero-overhead abstractions
 * - Graceful degradation when specialized backends unavailable
 * - SFINAE-based overload resolution
 * 
 * Design Pattern:
 * - Backend policies provide static methods for operations
 * - Operations query policy via SFINAE to select best implementation
 * - Fallback to default implementation when specialized version unavailable
 *
 * @requirements C++17; header-only; optional MKL dependency
 */
#pragma once

#include <type_traits>
#include <tuple>
#include <string>

#include "enforce.h"
#include "Expected.h"

// MKL availability detection
#if defined(CPP_UTILITIES_USE_MKL) && CPP_UTILITIES_USE_MKL
    #if defined(__has_include)
        #if __has_include(<mkl.h>)
            #define CPP_UTILITIES_HAS_MKL 1
            #include <mkl.h>
        #else
            #define CPP_UTILITIES_HAS_MKL 0
            #pragma message("MKL requested but mkl.h not found")
        #endif
    #else
        #define CPP_UTILITIES_HAS_MKL 1
        #include <mkl.h>
    #endif
#else
    #define CPP_UTILITIES_HAS_MKL 0
#endif

namespace cpp_utilities {

// ============================================================================
// Backend Policy Traits
// ============================================================================

/**
 * @brief Trait to detect if backend supports a specific operation.
 */
template <typename Backend, typename = void>
struct has_matmul : std::false_type {};

template <typename Backend>
struct has_matmul<Backend, std::void_t<decltype(
    Backend::template matmul<double>(
        nullptr, nullptr, 0, 0, 0, nullptr
    ))>> : std::true_type {};

template <typename Backend>
inline constexpr bool has_matmul_v = has_matmul<Backend>::value;

/**
 * @brief Trait to detect if backend supports type T.
 */
template <typename Backend, typename T, typename = void>
struct backend_supports_type : std::false_type {};

template <typename Backend, typename T>
struct backend_supports_type<Backend, T, std::void_t<
    typename Backend::template is_supported<T>
>> : Backend::template is_supported<T> {};

template <typename Backend, typename T>
inline constexpr bool backend_supports_type_v = 
    backend_supports_type<Backend, T>::value;

// ============================================================================
// Default Backend Policy
// ============================================================================

/**
 * @brief Default naive backend (always available, no optimizations).
 */
struct DefaultBackendPolicy {
    static constexpr const char* name = "Default";
    static constexpr bool has_optimizations = false;
    
    /**
     * @brief All types supported by default backend.
     */
    template <typename T>
    using is_supported = std::true_type;
    
    /**
     * @brief Initialize backend (no-op for default).
     */
    static void initialize(int = 0) noexcept {}
    
    /**
     * @brief Get version string.
     */
    static std::string version() { return "Default 1.0"; }
    
    // Note: Default backend doesn't provide specialized implementations
    // Operations will use the tensor class's built-in implementations
};

// ============================================================================
// MKL Backend Policy
// ============================================================================

#if CPP_UTILITIES_HAS_MKL

/**
 * @brief Intel MKL backend policy (high-performance implementations).
 */
struct MKLBackendPolicy {
    static constexpr const char* name = "Intel MKL";
    static constexpr bool has_optimizations = true;
    
    /**
     * @brief MKL supports float, double, and complex types.
     */
    template <typename T>
    using is_supported = std::disjunction<
        std::is_same<T, float>,
        std::is_same<T, double>,
        std::is_same<T, std::complex<float>>,
        std::is_same<T, std::complex<double>>
    >;
    
    // ------------------------------------------------------------------------
    // Initialization and Control
    // ------------------------------------------------------------------------
    
    static void initialize(int num_threads = 0) {
        if (num_threads > 0) {
            mkl_set_num_threads(num_threads);
        }
        mkl_set_threading_layer(MKL_THREADING_INTEL);
    }
    
    static int get_num_threads() {
        return mkl_get_max_threads();
    }
    
    static void set_num_threads(int n) {
        mkl_set_num_threads(n);
    }
    
    static void set_verbose(bool enable) {
        mkl_verbose(enable ? 1 : 0);
    }
    
    static std::string version() {
        MKLVersion ver;
        mkl_get_version(&ver);
        return std::string(ver.ProductStatus) + " " + 
               std::to_string(ver.MajorVersion) + "." +
               std::to_string(ver.MinorVersion) + "." +
               std::to_string(ver.UpdateVersion);
    }
    
    // ------------------------------------------------------------------------
    // BLAS Level 3: Matrix Multiplication
    // ------------------------------------------------------------------------
    
    /**
     * @brief Matrix multiplication: C = A * B
     * @param A Data pointer for matrix A (row-major)
     * @param B Data pointer for matrix B (row-major)
     * @param m Rows of A and C
     * @param k Cols of A, rows of B
     * @param n Cols of B and C
     * @param C Output matrix C (row-major, must be pre-allocated)
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, void>
    matmul(const T* A, const T* B, 
           MKL_INT m, MKL_INT k, MKL_INT n, 
           T* C) {
        // C = 1.0 * A * B + 0.0 * C
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    m, n, k,
                    1.0f,      // alpha
                    A, k,      // A, lda
                    B, n,      // B, ldb
                    0.0f,      // beta
                    C, n);     // C, ldc
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, void>
    matmul(const T* A, const T* B,
           MKL_INT m, MKL_INT k, MKL_INT n,
           T* C) {
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    m, n, k,
                    1.0,       // alpha
                    A, k,      // A, lda
                    B, n,      // B, ldb
                    0.0,       // beta
                    C, n);     // C, ldc
    }
    
    // ------------------------------------------------------------------------
    // BLAS Level 1: Vector Operations
    // ------------------------------------------------------------------------
    
    /**
     * @brief Vector dot product.
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, T>
    dot(const T* x, const T* y, MKL_INT n) {
        return cblas_sdot(n, x, 1, y, 1);
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, T>
    dot(const T* x, const T* y, MKL_INT n) {
        return cblas_ddot(n, x, 1, y, 1);
    }
    
    /**
     * @brief Vector L2 norm.
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, T>
    norm(const T* x, MKL_INT n) {
        return cblas_snrm2(n, x, 1);
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, T>
    norm(const T* x, MKL_INT n) {
        return cblas_dnrm2(n, x, 1);
    }
    
    /**
     * @brief Scaled vector addition: y = alpha*x + y
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, void>
    axpy(MKL_INT n, T alpha, const T* x, T* y) {
        cblas_saxpy(n, alpha, x, 1, y, 1);
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, void>
    axpy(MKL_INT n, T alpha, const T* x, T* y) {
        cblas_daxpy(n, alpha, x, 1, y, 1);
    }
    
    /**
     * @brief Vector scaling: x = alpha*x
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, void>
    scale(MKL_INT n, T alpha, T* x) {
        cblas_sscal(n, alpha, x, 1);
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, void>
    scale(MKL_INT n, T alpha, T* x) {
        cblas_dscal(n, alpha, x, 1);
    }
    
    // ------------------------------------------------------------------------
    // LAPACK: Linear Algebra
    // ------------------------------------------------------------------------
    
    /**
     * @brief Solve linear system: A*x = b
     * @return 0 on success, >0 if singular
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, MKL_INT>
    solve(MKL_INT n, T* A, T* b) {
        MKL_INT nrhs = 1;
        std::vector<MKL_INT> ipiv(n);
        MKL_INT info;
        sgesv(&n, &nrhs, A, &n, ipiv.data(), b, &n, &info);
        return info;
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, MKL_INT>
    solve(MKL_INT n, T* A, T* b) {
        MKL_INT nrhs = 1;
        std::vector<MKL_INT> ipiv(n);
        MKL_INT info;
        dgesv(&n, &nrhs, A, &n, ipiv.data(), b, &n, &info);
        return info;
    }
    
    /**
     * @brief Singular Value Decomposition.
     * @return 0 on success, >0 if convergence failed
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, MKL_INT>
    svd(MKL_INT m, MKL_INT n, T* A, T* s, T* u, T* vt) {
        MKL_INT lwork = -1;
        T work_query;
        MKL_INT info;
        
        char jobu = 'A', jobvt = 'A';
        
        // Query workspace
        sgesvd(&jobu, &jobvt, &m, &n, A, &n, s, u, &m, vt, &n,
               &work_query, &lwork, &info);
        
        lwork = static_cast<MKL_INT>(work_query);
        std::vector<T> work(lwork);
        
        // Compute
        sgesvd(&jobu, &jobvt, &m, &n, A, &n, s, u, &m, vt, &n,
               work.data(), &lwork, &info);
        
        return info;
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, MKL_INT>
    svd(MKL_INT m, MKL_INT n, T* A, T* s, T* u, T* vt) {
        MKL_INT lwork = -1;
        T work_query;
        MKL_INT info;
        
        char jobu = 'A', jobvt = 'A';
        
        // Query workspace
        dgesvd(&jobu, &jobvt, &m, &n, A, &n, s, u, &m, vt, &n,
               &work_query, &lwork, &info);
        
        lwork = static_cast<MKL_INT>(work_query);
        std::vector<T> work(lwork);
        
        // Compute
        dgesvd(&jobu, &jobvt, &m, &n, A, &n, s, u, &m, vt, &n,
               work.data(), &lwork, &info);
        
        return info;
    }
    
    /**
     * @brief Eigenvalues/eigenvectors of symmetric matrix.
     * @return 0 on success, >0 if convergence failed
     */
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, float>, MKL_INT>
    eigen_symmetric(MKL_INT n, T* A, T* eigenvalues) {
        char jobz = 'V', uplo = 'U';
        MKL_INT lwork = -1;
        T work_query;
        MKL_INT info;
        
        // Query workspace
        ssyev(&jobz, &uplo, &n, A, &n, eigenvalues, &work_query, &lwork, &info);
        
        lwork = static_cast<MKL_INT>(work_query);
        std::vector<T> work(lwork);
        
        // Compute
        ssyev(&jobz, &uplo, &n, A, &n, eigenvalues, work.data(), &lwork, &info);
        
        return info;
    }
    
    template <typename T>
    static typename std::enable_if_t<std::is_same_v<T, double>, MKL_INT>
    eigen_symmetric(MKL_INT n, T* A, T* eigenvalues) {
        char jobz = 'V', uplo = 'U';
        MKL_INT lwork = -1;
        T work_query;
        MKL_INT info;
        
        // Query workspace
        dsyev(&jobz, &uplo, &n, A, &n, eigenvalues, &work_query, &lwork, &info);
        
        lwork = static_cast<MKL_INT>(work_query);
        std::vector<T> work(lwork);
        
        // Compute
        dsyev(&jobz, &uplo, &n, A, &n, eigenvalues, work.data(), &lwork, &info);
        
        return info;
    }
};

#else // !CPP_UTILITIES_HAS_MKL

/**
 * @brief MKL backend stub when MKL not available.
 */
struct MKLBackendPolicy {
    static constexpr const char* name = "MKL (unavailable)";
    static constexpr bool has_optimizations = false;
    
    template <typename T>
    using is_supported = std::false_type;
    
    static void initialize(int = 0) noexcept {}
    static std::string version() { return "MKL not available"; }
};

#endif // CPP_UTILITIES_HAS_MKL

// ============================================================================
// Backend Policy Utilities
// ============================================================================

/**
 * @brief RAII helper for backend initialization.
 */
template <typename BackendPolicy>
class BackendContext {
private:
    int original_config_;
    
public:
    explicit BackendContext(int config = 0) : original_config_(0) {
        if constexpr (BackendPolicy::has_optimizations) {
#if CPP_UTILITIES_HAS_MKL
            if constexpr (std::is_same_v<BackendPolicy, MKLBackendPolicy>) {
                original_config_ = BackendPolicy::get_num_threads();
                BackendPolicy::initialize(config);
            }
#endif
        } else {
            (void)config;
            BackendPolicy::initialize();
        }
    }
    
    ~BackendContext() {
        if constexpr (BackendPolicy::has_optimizations) {
#if CPP_UTILITIES_HAS_MKL
            if constexpr (std::is_same_v<BackendPolicy, MKLBackendPolicy>) {
                BackendPolicy::set_num_threads(original_config_);
            }
#endif
        }
    }
    
    BackendContext(const BackendContext&) = delete;
    BackendContext& operator=(const BackendContext&) = delete;
};

// Convenient aliases
using MKLContext = BackendContext<MKLBackendPolicy>;
using DefaultContext = BackendContext<DefaultBackendPolicy>;

} // namespace cpp_utilities
