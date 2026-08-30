/** @file test_TensorMatmul.cpp @brief Native Tensor matrix-multiplication tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorMatmul
  file_role: test
  path: components/Tensor/tests/test_TensorMatmul.cpp
  namespace: fat_p::testing::tensor_matmul
  layer: Testing
  summary: "Vector, matrix, batched, strided, empty, and overflow matmul tests."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorMatmul.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

#include "FatPTest.h"
#include "TensorMatmul.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace fat_p::testing::tensor_matmul
{

FATP_TEST_CASE(vector_and_matrix_forms)
{
    Tensor<int> leftVector({3});
    Tensor<int> rightVector({3});
    std::iota(leftVector.begin(), leftVector.end(), 1);
    rightVector[0] = 4;
    rightVector[1] = 5;
    rightVector[2] = 6;
    const auto dot = matmul(leftVector, rightVector);
    static_assert(std::same_as<typename decltype(dot)::value_type, std::int64_t>);
    FATP_ASSERT_EQ(dot.rank(), std::size_t{0}, "Vector dot product should return a scalar");
    FATP_ASSERT_EQ(dot(), std::int64_t{32}, "Vector dot product should widen and accumulate");

    Tensor<int> matrix({2, 3});
    std::iota(matrix.begin(), matrix.end(), 1);
    const auto matrixVector = matmul(matrix, leftVector);
    FATP_ASSERT_TRUE(matrixVector.extents() == DynamicExtents({2}),
                     "Matrix-vector multiplication should remove the final vector axis");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(matrixVector.begin(), matrixVector.end()) ==
                         std::vector<std::int64_t>({14, 32}),
                     "Matrix-vector multiplication should use row-major logical coordinates");

    Tensor<int> rightMatrix({3, 2});
    std::iota(rightMatrix.begin(), rightMatrix.end(), 1);
    Tensor<int> shortVector({3});
    shortVector[0] = 1;
    shortVector[1] = 2;
    shortVector[2] = 3;
    const auto vectorMatrix = matmul(shortVector, rightMatrix);
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(vectorMatrix.begin(), vectorMatrix.end()) ==
                         std::vector<std::int64_t>({22, 28}),
                     "Vector-matrix multiplication should remove the leading vector axis");
    return true;
}

FATP_TEST_CASE(contiguous_strided_and_batched)
{
    Tensor<int> left({2, 3});
    Tensor<int> right({3, 2});
    std::iota(left.begin(), left.end(), 1);
    std::iota(right.begin(), right.end(), 1);
    const auto product = matmul(left, right);
    FATP_ASSERT_TRUE(product.extents() == DynamicExtents({2, 2}), "Matrix product shape should be M by N");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(product.begin(), product.end()) ==
                         std::vector<std::int64_t>({22, 28, 49, 64}),
                     "Contiguous matrix multiplication should match the scalar oracle");

    Tensor<int> source({2, 3});
    std::iota(source.begin(), source.end(), 1);
    Tensor<int> identity({2, 2}, 0);
    identity(0, 0) = 1;
    identity(1, 1) = 1;
    const auto strided = matmul(source.transposeView(), identity);
    FATP_ASSERT_TRUE(strided.extents() == DynamicExtents({3, 2}),
                     "Strided matrix operands should preserve matrix dimensions");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(strided.begin(), strided.end()) ==
                         std::vector<std::int64_t>({1, 4, 2, 5, 3, 6}),
                     "The generic kernel should honor signed logical strides");

    Tensor<int> batches({2, 2, 3});
    for (std::size_t index = 0; index < 6; ++index)
    {
        batches[index] = 1;
        batches[index + 6] = 2;
    }
    Tensor<int> broadcastRight({1, 3, 2}, 1);
    const auto batched = matmul(batches, broadcastRight);
    FATP_ASSERT_TRUE(batched.extents() == DynamicExtents({2, 2, 2}),
                     "Batch dimensions should use trailing broadcast rules");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(batched.begin(), batched.end()) ==
                         std::vector<std::int64_t>({3, 3, 3, 3, 6, 6, 6, 6}),
                     "Broadcast batches should reuse singleton operand batches");

    Tensor<double> compact({2, 3});
    compact[0] = 1.0e16;
    compact[1] = 1.0;
    compact[2] = -1.0e16;
    compact[3] = 2.0;
    compact[4] = 3.0;
    compact[5] = 4.0;
    Tensor<double> padded({2, 4}, 0.0);
    for (std::size_t row = 0; row < 2; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            padded(row, column) = compact(row, column);
        }
    }
    const auto paddedView = padded.sliceView({All, Slice{0, 3}});
    Tensor<double> doubleRight({3, 2}, 1.0);
    const auto blockedResult = matmul(compact, doubleRight);
    const auto genericResult = matmul(paddedView, doubleRight);
    FATP_ASSERT_TRUE(std::vector<double>(blockedResult.begin(), blockedResult.end()) ==
                         std::vector<double>(genericResult.begin(), genericResult.end()),
                     "Blocked and generic kernels should preserve the same serial accumulation order");
    return true;
}

FATP_TEST_CASE(empty_and_zero_inner_dimensions)
{
    Tensor<int> left({2, 0});
    Tensor<int> right({0, 3});
    const auto zeroInner = matmul(left, right);
    FATP_ASSERT_TRUE(zeroInner.extents() == DynamicExtents({2, 3}),
                     "A zero contraction dimension may still produce a nonempty output");
    FATP_ASSERT_TRUE(std::vector<std::int64_t>(zeroInner.begin(), zeroInner.end()) ==
                         std::vector<std::int64_t>({0, 0, 0, 0, 0, 0}),
                     "A zero-length dot product should use the additive identity");

    Tensor<int> emptyBatch({0, 2, 3});
    Tensor<int> singletonBatch({1, 3, 2}, 1);
    const auto empty = matmul(emptyBatch, singletonBatch);
    FATP_ASSERT_TRUE(empty.extents() == DynamicExtents({0, 2, 2}),
                     "Zero batch extents should survive singleton broadcasting");
    FATP_ASSERT_TRUE(empty.empty(), "A zero batch should not evaluate any elements");
    return true;
}

FATP_TEST_CASE(validation_overflow_and_allocator)
{
    Tensor<int> scalar({}, 2);
    Tensor<int> vector({1}, 3);
    FATP_ASSERT_THROWS(matmul(scalar, vector), std::invalid_argument,
                       "Rank-zero operands are not matrix-multiplication inputs");
    FATP_ASSERT_THROWS(matmul(Tensor<int>({2, 3}), Tensor<int>({4, 2})), std::invalid_argument,
                       "Inner dimensions must match");
    FATP_ASSERT_THROWS(matmul(Tensor<int>({2, 2, 3}), Tensor<int>({3, 3, 2})), std::invalid_argument,
                       "Batch dimensions must be broadcast-compatible");

    Tensor<std::int64_t> overflowLeft({1, 1}, std::numeric_limits<std::int64_t>::max());
    Tensor<std::int64_t> overflowRight({1, 1}, 2);
    FATP_ASSERT_THROWS(matmul(overflowLeft, overflowRight), std::overflow_error,
                       "Integral multiplication overflow should be reported");

    const auto allocated = matmul(vector, vector, std::allocator<std::int64_t>{});
    static_assert(std::same_as<typename decltype(allocated)::allocator_type, std::allocator<std::int64_t>>);
    FATP_ASSERT_EQ(allocated(), std::int64_t{9}, "Explicit result allocators should be supported");

    Tensor<int, std::allocator<int>> standardLeft(std::allocator_arg, std::allocator<int>{},
                                                   DynamicExtents({1}), 3);
    const auto inherited = matmul(standardLeft, vector);
    static_assert(std::same_as<typename decltype(inherited)::allocator_type,
                               std::allocator<std::int64_t>>);
    FATP_ASSERT_EQ(inherited(), std::int64_t{9},
                   "matmul should rebind the first owning operand's allocator");
    return true;
}

} // namespace fat_p::testing::tensor_matmul

namespace fat_p::testing
{

bool test_TensorMatmul()
{
    FATP_PRINT_HEADER(TENSOR MATMUL)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_matmul, vector_and_matrix_forms);
    FATP_RUN_TEST_NS(runner, tensor_matmul, contiguous_strided_and_batched);
    FATP_RUN_TEST_NS(runner, tensor_matmul, empty_and_zero_inner_dimensions);
    FATP_RUN_TEST_NS(runner, tensor_matmul, validation_overflow_and_allocator);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorMatmul() ? 0 : 1;
}
#endif
