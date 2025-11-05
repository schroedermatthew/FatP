/**
 * @file TensorMKL.h
 * @brief Intel MKL integration for high-performance tensor operations.
 *
 * @details Provides optimized implementations of tensor operations using Intel MKL:
 * - BLAS Level 1: Vector operations (dot, norm, axpy)
 * - BLAS Level 2: Matrix-vector operations (gemv)
 * - BLAS Level 3: Matrix-matrix operations (gemm)
 * - LAPACK: Linear algebra (SVD, eigenvalues, solving systems)
 * - VSL: Vector statistics and random number generation
 * 
 * Design follows library principles:
 * - Policy-based: MKL as optional backend policy
 * - Zero-overhead: Compile-time selection, no runtime dispatch
 * - Type-safe: Strong typing via StrongId
 * - DbC: enforce checks for all operations
 * - No dependencies when MKL disabled
 *
 * @performance With MKL enabled:
 * - Matrix multiply (1000x1000): ~10-20ms (vs ~1000ms naive)
 * - Vector operations: 2-5x faster than scalar
 * - Multi-threaded by default (configure via MKL_NUM_THREADS)
 *
 * @requirements 
 * - C++17
 * - Intel MKL library (optional, graceful fallback)
 * - Define CPP_UTILITIES_USE_MKL=1 to enable
 *
 * @example
 * // Enable MKL backend
 * Tensor<double, StandardAllocatorImpl<double>, SingleThreadedPolicy, MKLBackend> A(...);
 * Tensor<double, StandardAllocatorImpl<double>, SingleThreadedPolicy, MKLBackend> B(...);
 * 
 * // Matrix multiply uses MKL GEMM
 * auto C = A.matmul(B);
 */
#pragma once

#include "Tensor.h"
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
            #pragma message("MKL requested but mkl.h not found, falling back to default implementation")
        #endif
    #else
        // Try to include anyway for older compilers
        #define CPP_UTILITIES_HAS_MKL 1
        #include <mkl.h>
    #endif
#else
    #define CPP_UTILITIES_HAS_MKL 0
#endif

namespace cpp_utilities {

// ============================================================================
// Backend Policy Tags
// ============================================================================

/**
 * @brief Tag for default (naive) tensor backend.
 */
struct DefaultBackend {
    static constexpr bool use_mkl = false;
    static constexpr const char* name = "Default";
};

/**
 * @brief Tag for MKL-accelerated tensor backend.
 */
struct MKLBackend {
    static constexpr bool use_mkl = CPP_UTILITIES_HAS_MKL;
    static constexpr const char* name = "MKL";
    
#if CPP_UTILITIES_HAS_MKL
    /**
     * @brief Initialize MKL threading (call once at startup).
     */
    static void initialize(int num_threads = 0) {
        if (num_threads > 0) {
            mkl_set_num_threads(num_threads);
        }
        // Use Intel's threading layer by default
        mkl_set_threading_layer(MKL_THREADING_INTEL);
    }
    
    /**
     * @brief Get current number of MKL threads.
     */
    static int get_num_threads() {
        return mkl_get_max_threads();
    }
    
    /**
     * @brief Set number of MKL threads dynamically.
     */
    static void set_num_threads(int n) {
        mkl_set_num_threads(n);
    }
    
    /**
     * @brief Enable/disable MKL verbose mode (for debugging).
     */
    static void set_verbose(bool enable) {
        mkl_verbose(enable ? 1 : 0);
    }
    
    /**
     * @brief Get MKL version string.
     */
    static std::string version() {
        MKLVersion ver;
        mkl_get_version(&ver);
        return std::string(ver.ProductStatus) + " " + 
               std::to_string(ver.MajorVersion) + "." +
               std::to_string(ver.MinorVersion) + "." +
               std::to_string(ver.UpdateVersion);
    }
#else
    static void initialize(int = 0) {}
    static int get_num_threads() { return 1; }
    static void set_num_threads(int) {}
    static void set_verbose(bool) {}
    static std::string version() { return "MKL not available"; }
#endif
};

// ============================================================================
// MKL Type Traits
// ============================================================================

#if CPP_UTILITIES_HAS_MKL

/**
 * @brief Trait to determine if type is supported by MKL.
 */
template <typename T>
struct is_mkl_supported : std::false_type {};

template <>
struct is_mkl_supported<float> : std::true_type {};

template <>
struct is_mkl_supported<double> : std::true_type {};

template <>
struct is_mkl_supported<std::complex<float>> : std::true_type {};

template <>
struct is_mkl_supported<std::complex<double>> : std::true_type {};

/**
 * @brief Helper for MKL type checking.
 */
template <typename T>
inline constexpr bool is_mkl_supported_v = is_mkl_supported<T>::value;

/**
 * @brief MKL BLAS transpose options.
 */
enum class MKLTranspose {
    NoTrans = CblasNoTrans,
    Trans = CblasTrans,
    ConjTrans = CblasConjTrans
};

/**
 * @brief Convert MKLTranspose to CBLAS_TRANSPOSE.
 */
inline CBLAS_TRANSPOSE to_cblas_transpose(MKLTranspose trans) {
    return static_cast<CBLAS_TRANSPOSE>(trans);
}

#endif // CPP_UTILITIES_HAS_MKL

// ============================================================================
// MKL Operations Namespace
// ============================================================================

namespace mkl_ops {

#if CPP_UTILITIES_HAS_MKL

// ----------------------------------------------------------------------------
// BLAS Level 1: Vector Operations
// ----------------------------------------------------------------------------

/**
 * @brief Vector dot product: result = dot(x, y)
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, T>
dot(const T* x, const T* y, MKL_INT n) {
    return cblas_sdot(n, x, 1, y, 1);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, T>
dot(const T* x, const T* y, MKL_INT n) {
    return cblas_ddot(n, x, 1, y, 1);
}

/**
 * @brief Vector 2-norm: result = ||x||_2
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, T>
nrm2(const T* x, MKL_INT n) {
    return cblas_snrm2(n, x, 1);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, T>
nrm2(const T* x, MKL_INT n) {
    return cblas_dnrm2(n, x, 1);
}

/**
 * @brief Vector sum: result = sum(x)
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, T>
asum(const T* x, MKL_INT n) {
    return cblas_sasum(n, x, 1);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, T>
asum(const T* x, MKL_INT n) {
    return cblas_dasum(n, x, 1);
}

/**
 * @brief Scaled vector addition: y = alpha*x + y
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, void>
axpy(MKL_INT n, T alpha, const T* x, T* y) {
    cblas_saxpy(n, alpha, x, 1, y, 1);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, void>
axpy(MKL_INT n, T alpha, const T* x, T* y) {
    cblas_daxpy(n, alpha, x, 1, y, 1);
}

/**
 * @brief Vector scale: x = alpha*x
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, void>
scal(MKL_INT n, T alpha, T* x) {
    cblas_sscal(n, alpha, x, 1);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, void>
scal(MKL_INT n, T alpha, T* x) {
    cblas_dscal(n, alpha, x, 1);
}

/**
 * @brief Vector copy: y = x
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, void>
copy(MKL_INT n, const T* x, T* y) {
    cblas_scopy(n, x, 1, y, 1);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, void>
copy(MKL_INT n, const T* x, T* y) {
    cblas_dcopy(n, x, 1, y, 1);
}

// ----------------------------------------------------------------------------
// BLAS Level 2: Matrix-Vector Operations
// ----------------------------------------------------------------------------

/**
 * @brief Matrix-vector multiply: y = alpha*A*x + beta*y
 * @param trans Whether to transpose A
 * @param m Number of rows in A
 * @param n Number of columns in A
 * @param alpha Scalar multiplier for A*x
 * @param A Matrix data (row-major)
 * @param lda Leading dimension of A
 * @param x Vector x
 * @param beta Scalar multiplier for y
 * @param y Result vector y (modified in-place)
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, void>
gemv(CBLAS_TRANSPOSE trans, MKL_INT m, MKL_INT n,
     T alpha, const T* A, MKL_INT lda,
     const T* x, T beta, T* y) {
    cblas_sgemv(CblasRowMajor, trans, m, n, alpha, A, lda, x, 1, beta, y, 1);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, void>
gemv(CBLAS_TRANSPOSE trans, MKL_INT m, MKL_INT n,
     T alpha, const T* A, MKL_INT lda,
     const T* x, T beta, T* y) {
    cblas_dgemv(CblasRowMajor, trans, m, n, alpha, A, lda, x, 1, beta, y, 1);
}

// ----------------------------------------------------------------------------
// BLAS Level 3: Matrix-Matrix Operations
// ----------------------------------------------------------------------------

/**
 * @brief General matrix-matrix multiply: C = alpha*op(A)*op(B) + beta*C
 * @param transA Transpose operation for A
 * @param transB Transpose operation for B
 * @param m Number of rows in op(A) and C
 * @param n Number of columns in op(B) and C
 * @param k Number of columns in op(A) and rows in op(B)
 * @param alpha Scalar multiplier for A*B
 * @param A Matrix A data (row-major)
 * @param lda Leading dimension of A
 * @param B Matrix B data (row-major)
 * @param ldb Leading dimension of B
 * @param beta Scalar multiplier for C
 * @param C Result matrix C (modified in-place)
 * @param ldc Leading dimension of C
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, void>
gemm(CBLAS_TRANSPOSE transA, CBLAS_TRANSPOSE transB,
     MKL_INT m, MKL_INT n, MKL_INT k,
     T alpha, const T* A, MKL_INT lda,
     const T* B, MKL_INT ldb,
     T beta, T* C, MKL_INT ldc) {
    cblas_sgemm(CblasRowMajor, transA, transB, m, n, k,
                alpha, A, lda, B, ldb, beta, C, ldc);
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, void>
gemm(CBLAS_TRANSPOSE transA, CBLAS_TRANSPOSE transB,
     MKL_INT m, MKL_INT n, MKL_INT k,
     T alpha, const T* A, MKL_INT lda,
     const T* B, MKL_INT ldb,
     T beta, T* C, MKL_INT ldc) {
    cblas_dgemm(CblasRowMajor, transA, transB, m, n, k,
                alpha, A, lda, B, ldb, beta, C, ldc);
}

// ----------------------------------------------------------------------------
// LAPACK: Linear Algebra Operations
// ----------------------------------------------------------------------------

/**
 * @brief Solve linear system A*X = B using LU decomposition.
 * @param n Order of matrix A
 * @param nrhs Number of right-hand sides
 * @param A Matrix A (destroyed on output, contains L and U)
 * @param lda Leading dimension of A
 * @param ipiv Pivot indices
 * @param B Right-hand side matrix B (contains solution X on output)
 * @param ldb Leading dimension of B
 * @return 0 on success, >0 if U is singular
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, MKL_INT>
gesv(MKL_INT n, MKL_INT nrhs, T* A, MKL_INT lda,
     MKL_INT* ipiv, T* B, MKL_INT ldb) {
    MKL_INT info;
    sgesv(&n, &nrhs, A, &lda, ipiv, B, &ldb, &info);
    return info;
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, MKL_INT>
gesv(MKL_INT n, MKL_INT nrhs, T* A, MKL_INT lda,
     MKL_INT* ipiv, T* B, MKL_INT ldb) {
    MKL_INT info;
    dgesv(&n, &nrhs, A, &lda, ipiv, B, &ldb, &info);
    return info;
}

/**
 * @brief Compute SVD: A = U * Sigma * V^T
 * @param m Number of rows in A
 * @param n Number of columns in A
 * @param A Matrix to decompose (destroyed on output)
 * @param lda Leading dimension of A
 * @param s Singular values (min(m,n) elements)
 * @param u Left singular vectors (m x m if full, m x min(m,n) if thin)
 * @param ldu Leading dimension of U
 * @param vt Right singular vectors transposed (n x n if full)
 * @param ldvt Leading dimension of VT
 * @return 0 on success, >0 if convergence failed
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, MKL_INT>
gesvd(char jobu, char jobvt, MKL_INT m, MKL_INT n,
      T* A, MKL_INT lda, T* s,
      T* u, MKL_INT ldu, T* vt, MKL_INT ldvt) {
    MKL_INT lwork = -1;
    T work_query;
    MKL_INT info;
    
    // Query optimal workspace size
    sgesvd(&jobu, &jobvt, &m, &n, A, &lda, s, u, &ldu, vt, &ldvt,
           &work_query, &lwork, &info);
    
    lwork = static_cast<MKL_INT>(work_query);
    std::vector<T> work(lwork);
    
    // Compute SVD
    sgesvd(&jobu, &jobvt, &m, &n, A, &lda, s, u, &ldu, vt, &ldvt,
           work.data(), &lwork, &info);
    
    return info;
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, MKL_INT>
gesvd(char jobu, char jobvt, MKL_INT m, MKL_INT n,
      T* A, MKL_INT lda, T* s,
      T* u, MKL_INT ldu, T* vt, MKL_INT ldvt) {
    MKL_INT lwork = -1;
    T work_query;
    MKL_INT info;
    
    // Query optimal workspace size
    dgesvd(&jobu, &jobvt, &m, &n, A, &lda, s, u, &ldu, vt, &ldvt,
           &work_query, &lwork, &info);
    
    lwork = static_cast<MKL_INT>(work_query);
    std::vector<T> work(lwork);
    
    // Compute SVD
    dgesvd(&jobu, &jobvt, &m, &n, A, &lda, s, u, &ldu, vt, &ldvt,
           work.data(), &lwork, &info);
    
    return info;
}

/**
 * @brief Compute eigenvalues and eigenvectors of symmetric matrix.
 * @param n Order of matrix A
 * @param A Symmetric matrix (upper triangle used, destroyed on output)
 * @param lda Leading dimension of A
 * @param w Eigenvalues in ascending order
 * @param eigenvectors If non-null, contains eigenvectors on output
 * @param ldz Leading dimension of eigenvectors matrix
 * @return 0 on success, >0 if convergence failed
 */
template <typename T>
typename std::enable_if_t<std::is_same_v<T, float>, MKL_INT>
syev(char jobz, MKL_INT n, T* A, MKL_INT lda, T* w) {
    char uplo = 'U';
    MKL_INT lwork = -1;
    T work_query;
    MKL_INT info;
    
    // Query optimal workspace
    ssyev(&jobz, &uplo, &n, A, &lda, w, &work_query, &lwork, &info);
    
    lwork = static_cast<MKL_INT>(work_query);
    std::vector<T> work(lwork);
    
    // Compute eigenvalues/vectors
    ssyev(&jobz, &uplo, &n, A, &lda, w, work.data(), &lwork, &info);
    
    return info;
}

template <typename T>
typename std::enable_if_t<std::is_same_v<T, double>, MKL_INT>
syev(char jobz, MKL_INT n, T* A, MKL_INT lda, T* w) {
    char uplo = 'U';
    MKL_INT lwork = -1;
    T work_query;
    MKL_INT info;
    
    // Query optimal workspace
    dsyev(&jobz, &uplo, &n, A, &lda, w, &work_query, &lwork, &info);
    
    lwork = static_cast<MKL_INT>(work_query);
    std::vector<T> work(lwork);
    
    // Compute eigenvalues/vectors
    dsyev(&jobz, &uplo, &n, A, &lda, w, work.data(), &lwork, &info);
    
    return info;
}

#endif // CPP_UTILITIES_HAS_MKL

} // namespace mkl_ops

// ============================================================================
// Extended Tensor with MKL Backend Support
// ============================================================================

/**
 * @brief Extended Tensor class template with backend policy.
 */
template <typename T, 
          typename AllocImpl = StandardAllocatorImpl<T>,
          typename ConcurrencyPolicy = SingleThreadedPolicy,
          typename Backend = DefaultBackend>
class TensorMKL : public Tensor<T, AllocImpl, ConcurrencyPolicy> {
public:
    using Base = Tensor<T, AllocImpl, ConcurrencyPolicy>;
    using Base::Base; // Inherit constructors
    
    static constexpr bool has_mkl = Backend::use_mkl;
    
#if CPP_UTILITIES_HAS_MKL
    
    // ========================================================================
    // MKL-Optimized Operations (only when MKL available and backend enabled)
    // ========================================================================
    
    /**
     * @brief Matrix multiplication using MKL GEMM.
     * 
     * @details This overrides the base class matmul with MKL-accelerated version.
     * Falls back to base implementation for non-MKL types or if MKL disabled.
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        Expected<TensorMKL, std::string>
    >
    matmul(const TensorMKL& other) const {
        if (this->rank() != 2 || other.rank() != 2) {
            return make_unexpected(std::string("Matrix multiplication requires 2D tensors"));
        }
        
        if (this->shape()[1] != other.shape()[0]) {
            return make_unexpected(std::string("Incompatible shapes for matrix multiplication"));
        }
        
        MKL_INT m = static_cast<MKL_INT>(this->shape()[0]);
        MKL_INT k = static_cast<MKL_INT>(this->shape()[1]);
        MKL_INT n = static_cast<MKL_INT>(other.shape()[1]);
        
        TensorShape result_shape({static_cast<size_t>(m), static_cast<size_t>(n)});
        TensorMKL result(result_shape, T{0});
        
        // C = 1.0 * A * B + 0.0 * C
        mkl_ops::gemm<T>(
            CblasNoTrans, CblasNoTrans,
            m, n, k,
            T{1},                           // alpha
            this->data(), k,                // A, lda
            other.data(), n,                // B, ldb
            T{0},                           // beta
            result.data(), n                // C, ldc
        );
        
        return result;
    }
    
    /**
     * @brief Fallback to base implementation for non-MKL types.
     */
    template <typename U = T>
    typename std::enable_if_t<
        !Backend::use_mkl || !is_mkl_supported_v<U>,
        Expected<TensorMKL, std::string>
    >
    matmul(const TensorMKL& other) const {
        auto result = Base::matmul(static_cast<const Base&>(other));
        if (!result.has_value()) {
            return make_unexpected(result.error());
        }
        return TensorMKL(std::move(result.value()));
    }
    
    /**
     * @brief Dot product using MKL.
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        Expected<T, std::string>
    >
    dot(const TensorMKL& other) const {
        if (this->size() != other.size()) {
            return make_unexpected(std::string("Vectors must have same size"));
        }
        
        MKL_INT n = static_cast<MKL_INT>(this->size());
        T result = mkl_ops::dot<T>(this->data(), other.data(), n);
        
        return result;
    }
    
    /**
     * @brief 2-norm using MKL.
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        T
    >
    norm() const {
        MKL_INT n = static_cast<MKL_INT>(this->size());
        return mkl_ops::nrm2<T>(this->data(), n);
    }
    
    /**
     * @brief Sum using MKL (absolute sum for complex types).
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        T
    >
    sum_mkl() const {
        MKL_INT n = static_cast<MKL_INT>(this->size());
        return mkl_ops::asum<T>(this->data(), n);
    }
    
    /**
     * @brief Scaled addition: this = alpha*other + this (in-place, using MKL).
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        void
    >
    axpy(T alpha, const TensorMKL& other) {
        enforce(this->size() == other.size(), "Tensors must have same size");
        
        MKL_INT n = static_cast<MKL_INT>(this->size());
        mkl_ops::axpy<T>(n, alpha, other.data(), this->data());
    }
    
    /**
     * @brief Scale tensor in-place using MKL.
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        void
    >
    scale(T alpha) {
        MKL_INT n = static_cast<MKL_INT>(this->size());
        mkl_ops::scal<T>(n, alpha, this->data());
    }
    
    /**
     * @brief Solve linear system Ax = b using MKL GESV.
     * @param b Right-hand side (modified to contain solution).
     * @return 0 on success, >0 if singular.
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        Expected<void, std::string>
    >
    solve(TensorMKL& b) {
        if (this->rank() != 2 || b.rank() != 1) {
            return make_unexpected(std::string("A must be 2D matrix, b must be 1D vector"));
        }
        
        if (this->shape()[0] != this->shape()[1]) {
            return make_unexpected(std::string("A must be square matrix"));
        }
        
        if (this->shape()[0] != b.size()) {
            return make_unexpected(std::string("Dimension mismatch"));
        }
        
        MKL_INT n = static_cast<MKL_INT>(this->shape()[0]);
        MKL_INT nrhs = 1;
        MKL_INT lda = n;
        MKL_INT ldb = n;
        
        // Need to copy A since GESV destroys it
        TensorMKL A_copy = *this;
        
        std::vector<MKL_INT> ipiv(n);
        
        MKL_INT info = mkl_ops::gesv<T>(n, nrhs, A_copy.data(), lda,
                                        ipiv.data(), b.data(), ldb);
        
        if (info != 0) {
            return make_unexpected(std::string("Matrix is singular or GESV failed"));
        }
        
        return {};
    }
    
    /**
     * @brief Compute SVD using MKL.
     * @return Tuple of (U, S, Vt) where A = U * diag(S) * Vt
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        Expected<std::tuple<TensorMKL, TensorMKL, TensorMKL>, std::string>
    >
    svd() const {
        if (this->rank() != 2) {
            return make_unexpected(std::string("SVD requires 2D matrix"));
        }
        
        MKL_INT m = static_cast<MKL_INT>(this->shape()[0]);
        MKL_INT n = static_cast<MKL_INT>(this->shape()[1]);
        MKL_INT min_mn = std::min(m, n);
        
        // Need to copy A since GESVD destroys it
        TensorMKL A_copy = *this;
        
        TensorMKL U(TensorShape({static_cast<size_t>(m), static_cast<size_t>(m)}));
        TensorMKL S(TensorShape({static_cast<size_t>(min_mn)}));
        TensorMKL Vt(TensorShape({static_cast<size_t>(n), static_cast<size_t>(n)}));
        
        MKL_INT info = mkl_ops::gesvd<T>(
            'A', 'A',  // Compute full U and Vt
            m, n,
            A_copy.data(), n,
            S.data(),
            U.data(), m,
            Vt.data(), n
        );
        
        if (info != 0) {
            return make_unexpected(std::string("SVD failed to converge"));
        }
        
        return std::make_tuple(std::move(U), std::move(S), std::move(Vt));
    }
    
    /**
     * @brief Compute eigenvalues and eigenvectors (symmetric matrices only).
     * @return Tuple of (eigenvalues, eigenvectors) in ascending order.
     */
    template <typename U = T>
    typename std::enable_if_t<
        Backend::use_mkl && is_mkl_supported_v<U>,
        Expected<std::tuple<TensorMKL, TensorMKL>, std::string>
    >
    eig() const {
        if (this->rank() != 2) {
            return make_unexpected(std::string("Eigendecomposition requires 2D matrix"));
        }
        
        if (this->shape()[0] != this->shape()[1]) {
            return make_unexpected(std::string("Matrix must be square"));
        }
        
        MKL_INT n = static_cast<MKL_INT>(this->shape()[0]);
        
        // Copy matrix (SYEV destroys input and stores eigenvectors there)
        TensorMKL eigvecs = *this;
        TensorMKL eigvals(TensorShape({static_cast<size_t>(n)}));
        
        MKL_INT info = mkl_ops::syev<T>('V', n, eigvecs.data(), n, eigvals.data());
        
        if (info != 0) {
            return make_unexpected(std::string("Eigendecomposition failed"));
        }
        
        return std::make_tuple(std::move(eigvals), std::move(eigvecs));
    }
    
#endif // CPP_UTILITIES_HAS_MKL
    
    /**
     * @brief Get backend name.
     */
    static constexpr const char* backend_name() {
        return Backend::name;
    }
};

// ============================================================================
// Convenient Type Aliases
// ============================================================================

// Float tensors with MKL
template <typename AllocImpl = StandardAllocatorImpl<float>>
using TensorFloatMKL = TensorMKL<float, AllocImpl, SingleThreadedPolicy, MKLBackend>;

// Double tensors with MKL
template <typename AllocImpl = StandardAllocatorImpl<double>>
using TensorDoubleMKL = TensorMKL<double, AllocImpl, SingleThreadedPolicy, MKLBackend>;

// Thread-safe MKL tensors
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
using TensorMKLThreadSafe = TensorMKL<T, AllocImpl, MutexSynchronizationPolicy, MKLBackend>;

// ============================================================================
// MKL Initialization Helper
// ============================================================================

/**
 * @brief RAII helper for MKL initialization and cleanup.
 */
class MKLContext {
private:
    int original_num_threads_;
    
public:
    /**
     * @brief Initialize MKL with specified number of threads.
     */
    explicit MKLContext(int num_threads = 0) {
#if CPP_UTILITIES_HAS_MKL
        original_num_threads_ = MKLBackend::get_num_threads();
        MKLBackend::initialize(num_threads);
#else
        (void)num_threads;
        original_num_threads_ = 1;
#endif
    }
    
    /**
     * @brief Restore original thread count.
     */
    ~MKLContext() {
#if CPP_UTILITIES_HAS_MKL
        MKLBackend::set_num_threads(original_num_threads_);
#endif
    }
    
    MKLContext(const MKLContext&) = delete;
    MKLContext& operator=(const MKLContext&) = delete;
};

} // namespace cpp_utilities
