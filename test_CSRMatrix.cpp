// test_CSRMatrix.cpp
#include <iostream>
#include <vector>
#include <cmath>

#include "CSRMatrix.h"
#include "test_CSRMatrix.h"
#include "FatPTest.h"

namespace fat_p::testing
{

bool test_csr_matrix_construction() {
    CSRMatrix<double> mat1;
    SIMPLE_ASSERT(mat1.rows() == 0, "Default constructor should create 0x0 matrix");
    SIMPLE_ASSERT(mat1.cols() == 0, "Default constructor should create 0x0 matrix");
    SIMPLE_ASSERT(mat1.nnz() == 0, "Default matrix should have no non-zeros");
    
    CSRMatrix<double> mat2(3, 4);
    SIMPLE_ASSERT(mat2.rows() == 3, "Should be 3x4 matrix");
    SIMPLE_ASSERT(mat2.cols() == 4, "Should be 3x4 matrix");
    SIMPLE_ASSERT(mat2.nnz() == 0, "Empty matrix should have no non-zeros");
    
    return true;
}

bool test_csr_matrix_from_coo() {
    // Create a 3x3 matrix:
    // [1 0 2]
    // [0 3 0]
    // [4 0 5]
    
    std::vector<int> rows = {0, 0, 1, 2, 2};
    std::vector<int> cols = {0, 2, 1, 0, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);
    
    SIMPLE_ASSERT(mat.rows() == 3, "Should be 3x3");
    SIMPLE_ASSERT(mat.cols() == 3, "Should be 3x3");
    SIMPLE_ASSERT(mat.nnz() == 5, "Should have 5 non-zeros");
    
    SIMPLE_ASSERT(mat(0, 0) == 1.0, "mat(0,0) should be 1.0");
    SIMPLE_ASSERT(mat(0, 2) == 2.0, "mat(0,2) should be 2.0");
    SIMPLE_ASSERT(mat(1, 1) == 3.0, "mat(1,1) should be 3.0");
    SIMPLE_ASSERT(mat(2, 0) == 4.0, "mat(2,0) should be 4.0");
    SIMPLE_ASSERT(mat(2, 2) == 5.0, "mat(2,2) should be 5.0");
    SIMPLE_ASSERT(mat(0, 1) == 0.0, "mat(0,1) should be 0.0");
    
    return true;
}

bool test_csr_matrix_from_dense() {
    double dense[] = {
        1.0, 0.0, 2.0,
        0.0, 3.0, 0.0,
        4.0, 0.0, 5.0
    };
    
    auto mat = CSRMatrix<double>::from_dense(dense, 3, 3);
    
    SIMPLE_ASSERT(mat.nnz() == 5, "Should have 5 non-zeros");
    SIMPLE_ASSERT(mat(0, 0) == 1.0, "mat(0,0) should be 1.0");
    SIMPLE_ASSERT(mat(1, 1) == 3.0, "mat(1,1) should be 3.0");
    
    return true;
}

bool test_csr_matrix_matvec() {
    // Matrix:
    // [1 2]
    // [3 4]
    std::vector<int> rows = {0, 0, 1, 1};
    std::vector<int> cols = {0, 1, 0, 1};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};
    
    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);
    
    std::vector<double> x = {5.0, 6.0};
    std::vector<double> y = mat * x;
    
    // y = [1*5 + 2*6, 3*5 + 4*6] = [17, 39]
    SIMPLE_ASSERT(std::abs(y[0] - 17.0) < 1e-10, "y[0] should be 17.0");
    SIMPLE_ASSERT(std::abs(y[1] - 39.0) < 1e-10, "y[1] should be 39.0");
    
    return true;
}

bool test_csr_matrix_transpose() {
    std::vector<int> rows = {0, 0, 1};
    std::vector<int> cols = {0, 1, 0};
    std::vector<double> vals = {1.0, 2.0, 3.0};
    
    // Original matrix:
    // [1 2]
    // [3 0]
    
    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);
    auto mat_t = mat.transpose();
    
    // Transposed should be:
    // [1 3]
    // [2 0]
    
    SIMPLE_ASSERT(mat_t.rows() == 2 && mat_t.cols() == 2, "Transpose dimensions should be swapped");
    SIMPLE_ASSERT(mat_t(0, 0) == 1.0, "mat_t(0,0) should be 1.0");
    SIMPLE_ASSERT(mat_t(0, 1) == 3.0, "mat_t(0,1) should be 3.0");
    SIMPLE_ASSERT(mat_t(1, 0) == 2.0, "mat_t(1,0) should be 2.0");
    SIMPLE_ASSERT(mat_t(1, 1) == 0.0, "mat_t(1,1) should be 0.0");
    
    return true;
}

bool test_csr_matrix_addition() {
    // Matrix A:
    // [1 0]
    // [0 2]
    std::vector<int> rows_a = {0, 1};
    std::vector<int> cols_a = {0, 1};
    std::vector<double> vals_a = {1.0, 2.0};
    CSRMatrix<double, int> A(2, 2, rows_a, cols_a, vals_a);
    
    // Matrix B:
    // [3 0]
    // [0 4]
    std::vector<int> rows_b = {0, 1};
    std::vector<int> cols_b = {0, 1};
    std::vector<double> vals_b = {3.0, 4.0};
    CSRMatrix<double, int> B(2, 2, rows_b, cols_b, vals_b);
    
    auto C = A + B;
    
    // C should be:
    // [4 0]
    // [0 6]
    SIMPLE_ASSERT(std::abs(C(0, 0) - 4.0) < 1e-10, "C(0,0) should be 4.0");
    SIMPLE_ASSERT(std::abs(C(1, 1) - 6.0) < 1e-10, "C(1,1) should be 6.0");
    
    return true;
}

bool test_csr_matrix_scalar_mult() {
    std::vector<int> rows = {0, 1};
    std::vector<int> cols = {0, 1};
    std::vector<double> vals = {2.0, 3.0};
    
    CSRMatrix<double, int> mat(2, 2, rows, cols, vals);
    auto result = mat * 2.0;
    
    SIMPLE_ASSERT(std::abs(result(0, 0) - 4.0) < 1e-10, "Scalar mult should double values");
    SIMPLE_ASSERT(std::abs(result(1, 1) - 6.0) < 1e-10, "Scalar mult should double values");
    
    return true;
}

bool test_csr_matrix_matmul() {
    // A = [1 2]
    //     [3 4]
    std::vector<int> rows_a = {0, 0, 1, 1};
    std::vector<int> cols_a = {0, 1, 0, 1};
    std::vector<double> vals_a = {1.0, 2.0, 3.0, 4.0};
    CSRMatrix<double, int> A(2, 2, rows_a, cols_a, vals_a);
    
    // B = [5 6]
    //     [7 8]
    std::vector<int> rows_b = {0, 0, 1, 1};
    std::vector<int> cols_b = {0, 1, 0, 1};
    std::vector<double> vals_b = {5.0, 6.0, 7.0, 8.0};
    CSRMatrix<double, int> B(2, 2, rows_b, cols_b, vals_b);
    
    auto C = A.matmul(B);
    
    // C = [1*5+2*7  1*6+2*8] = [19 22]
    //     [3*5+4*7  3*6+4*8]   [43 50]
    
    SIMPLE_ASSERT(std::abs(C(0, 0) - 19.0) < 1e-10, "C(0,0) should be 19.0");
    SIMPLE_ASSERT(std::abs(C(0, 1) - 22.0) < 1e-10, "C(0,1) should be 22.0");
    SIMPLE_ASSERT(std::abs(C(1, 0) - 43.0) < 1e-10, "C(1,0) should be 43.0");
    SIMPLE_ASSERT(std::abs(C(1, 1) - 50.0) < 1e-10, "C(1,1) should be 50.0");
    
    return true;
}

bool test_csr_matrix_to_dense() {
    std::vector<int> rows = {0, 1, 2};
    std::vector<int> cols = {0, 1, 2};
    std::vector<double> vals = {1.0, 2.0, 3.0};
    
    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);
    auto dense = mat.to_dense();
    
    SIMPLE_ASSERT(dense.size() == 9, "Dense should have 9 elements");
    SIMPLE_ASSERT(dense[0] == 1.0, "dense[0] should be 1.0");
    SIMPLE_ASSERT(dense[4] == 2.0, "dense[4] should be 2.0");
    SIMPLE_ASSERT(dense[8] == 3.0, "dense[8] should be 3.0");
    
    return true;
}

bool test_csr_matrix_identity() {
    auto I = identity_matrix<double>(5);
    
    SIMPLE_ASSERT(I.rows() == 5 && I.cols() == 5, "Identity should be 5x5");
    SIMPLE_ASSERT(I.nnz() == 5, "Identity should have 5 non-zeros");
    
    for (size_t i = 0; i < 5; ++i) {
        SIMPLE_ASSERT(I(i, i) == 1.0, "Diagonal should be 1.0");
        for (size_t j = 0; j < 5; ++j) {
            if (i != j) {
                SIMPLE_ASSERT(I(i, j) == 0.0, "Off-diagonal should be 0.0");
            }
        }
    }
    
    return true;
}

bool test_csr_matrix_sparsity() {
    // 3x3 matrix with 2 non-zeros
    std::vector<int> rows = {0, 2};
    std::vector<int> cols = {0, 2};
    std::vector<double> vals = {1.0, 2.0};
    
    CSRMatrix<double, int> mat(3, 3, rows, cols, vals);
    
    double sparsity = mat.sparsity();
    double expected = 2.0 / 9.0;
    
    SIMPLE_ASSERT(std::abs(sparsity - expected) < 1e-10, "Sparsity should be 2/9");
    
    return true;
}

void benchmark_csr_matrix() {
    std::cout << "\n" << colors::cyan() << "CSRMatrix Benchmarks:" << colors::reset() << "\n\n";
    
    // Create a sparse 1000x1000 matrix with ~1% sparsity
    constexpr size_t N = 1000;
    constexpr size_t nnz_per_row = 10;
    
    std::vector<int> rows, cols;
    std::vector<double> vals;
    
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < nnz_per_row; ++j) {
            rows.push_back(static_cast<int>(i));
            cols.push_back(static_cast<int>((i + j * 100) % N));
            vals.push_back(1.0);
        }
    }
    
    CSRMatrix<double, int> mat(N, N, rows, cols, vals);
    
    std::vector<double> x(N, 1.0);
    std::vector<double> y(N);
    
    // Benchmark SpMV
    double spmv_time = measure_perf([&]() {
        mat.matvec(x.data(), y.data());
        DoNotOptimize(y);
    }, 10000, 100);
    
    std::cout << "Sparse MatVec (" << N << "x" << N << ", " << mat.nnz() << " nnz): " 
              << format_time(spmv_time) << "\n";
    
    // Benchmark transpose
    double transpose_time = measure_perf([&]() {
        auto mat_t = mat.transpose();
        DoNotOptimize(mat_t);
    }, 1000, 10);
    
    std::cout << "Transpose: " << format_time(transpose_time) << "\n";
    
    std::cout << "\nSparsity: " << (mat.sparsity() * 100.0) << "%\n";
}

bool test_CSRMatrix() {

    PRINT_HEADER(CSR MATRIX)

    TestRunner runner;

    RUN_TEST(runner, csr_matrix_construction);
    RUN_TEST(runner, csr_matrix_from_coo);
    RUN_TEST(runner, csr_matrix_from_dense);
    RUN_TEST(runner, csr_matrix_matvec);
    RUN_TEST(runner, csr_matrix_transpose);
    RUN_TEST(runner, csr_matrix_addition);
    RUN_TEST(runner, csr_matrix_scalar_mult);
    RUN_TEST(runner, csr_matrix_matmul);
    RUN_TEST(runner, csr_matrix_to_dense);
    RUN_TEST(runner, csr_matrix_identity);
    RUN_TEST(runner, csr_matrix_sparsity);

    benchmark_csr_matrix();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
