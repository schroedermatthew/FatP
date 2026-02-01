/**
 * @file TensorEinsum.h
 * @brief Einstein summation notation for Tensor operations
 *
 * Implements a subset of Einstein summation notation for common tensor operations.
 * Supports patterns like:
 *   - Matrix multiplication: "ij,jk->ik"
 *   - Batch matrix multiplication: "bij,bjk->bik"
 *   - Outer product: "i,j->ij"
 *   - Inner product: "i,i->"
 *   - Transpose: "ij->ji"
 *   - Trace: "ii->"
 *   - Sum reductions: "ij->i", "ij->j", "ij->"
 *   - Element-wise: "ij,ij->ij"
 *
 * @layer Domain
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorEinsum
  file_role: public_header
  path: include/fat_p/TensorEinsum.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for TensorEinsum."
  api_stability: in_work
  related:
    docs_search: "TensorEinsum"
    tests:
      - components/Tensor/tests/test_TensorEinsum.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "Tensor.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fat_p
{

namespace detail
{

/**
 * @brief Parse einsum notation string
 * @return Tuple of (input1_indices, input2_indices, output_indices)
 */
inline std::tuple<std::string, std::string, std::string> parse_einsum_notation(const std::string& notation)
{
    // Find the arrow separator
    size_t arrow_pos = notation.find("->");
    if (arrow_pos == std::string::npos)
    {
        throw std::invalid_argument("Invalid einsum notation: missing '->'");
    }

    std::string inputs = notation.substr(0, arrow_pos);
    std::string output = notation.substr(arrow_pos + 2);

    // Remove whitespace
    inputs.erase(std::remove_if(inputs.begin(), inputs.end(), ::isspace), inputs.end());
    output.erase(std::remove_if(output.begin(), output.end(), ::isspace), output.end());

    // Split inputs by comma
    size_t comma_pos = inputs.find(',');

    if (comma_pos != std::string::npos)
    {
        // Two inputs
        std::string input1 = inputs.substr(0, comma_pos);
        std::string input2 = inputs.substr(comma_pos + 1);
        return {input1, input2, output};
    }
    else
    {
        // Single input
        return {inputs, "", output};
    }
}

} // namespace detail

/**
 * @brief Einstein summation for single tensor (unary operations)
 * @tparam T Element type
 * @param notation Einstein notation string (e.g., "ij->ji", "ii->")
 * @param a Input tensor
 * @return Result tensor
 */
template <typename T>
Tensor<T> einsum(const std::string& notation, const Tensor<T>& a)
{
    auto [in1, in2, out] = detail::parse_einsum_notation(notation);

    if (!in2.empty())
    {
        throw std::invalid_argument("einsum: Single tensor version called with two-input notation");
    }

    // Transpose: ij->ji
    if (in1.length() == 2 && out.length() == 2 && in1[0] == out[1] && in1[1] == out[0])
    {
        if (a.ndim() != 2)
        {
            throw std::invalid_argument("einsum transpose: input must be rank 2");
        }

        size_t rows = a.shape()[0];
        size_t cols = a.shape()[1];
        Tensor<T> result({cols, rows});

        for (size_t i = 0; i < rows; ++i)
        {
            for (size_t j = 0; j < cols; ++j)
            {
                result(j, i) = a(i, j);
            }
        }
        return result;
    }

    // Trace: ii->
    if (in1.length() == 2 && in1[0] == in1[1] && out.empty())
    {
        if (a.ndim() != 2)
        {
            throw std::invalid_argument("einsum trace: input must be rank 2");
        }
        if (a.shape()[0] != a.shape()[1])
        {
            throw std::invalid_argument("einsum trace: input must be square");
        }

        T sum = T{0};
        for (size_t i = 0; i < a.shape()[0]; ++i)
        {
            sum += a(i, i);
        }

        Tensor<T> result({1});
        result[0] = sum;
        return result;
    }

    // Sum along axis: ij->i or ij->j
    if (in1.length() == 2 && out.length() == 1)
    {
        if (a.ndim() != 2)
        {
            throw std::invalid_argument("einsum sum: input must be rank 2");
        }

        size_t rows = a.shape()[0];
        size_t cols = a.shape()[1];

        if (out[0] == in1[0])
        {
            // Sum columns, keep rows: ij->i
            Tensor<T> result({rows});
            for (size_t i = 0; i < rows; ++i)
            {
                T sum = T{0};
                for (size_t j = 0; j < cols; ++j)
                {
                    sum += a(i, j);
                }
                result[i] = sum;
            }
            return result;
        }
        else if (out[0] == in1[1])
        {
            // Sum rows, keep columns: ij->j
            Tensor<T> result({cols});
            for (size_t j = 0; j < cols; ++j)
            {
                T sum = T{0};
                for (size_t i = 0; i < rows; ++i)
                {
                    sum += a(i, j);
                }
                result[j] = sum;
            }
            return result;
        }
    }

    // Sum all: ij->
    if (in1.length() == 2 && out.empty())
    {
        if (a.ndim() != 2)
        {
            throw std::invalid_argument("einsum sum all: input must be rank 2");
        }

        T sum = T{0};
        for (size_t i = 0; i < a.size(); ++i)
        {
            sum += a[i];
        }

        Tensor<T> result({1});
        result[0] = sum;
        return result;
    }

    throw std::invalid_argument("einsum: Unsupported unary pattern: " + notation);
}

/**
 * @brief Einstein summation for two tensors (binary operations)
 * @tparam T Element type
 * @param notation Einstein notation string
 * @param a First input tensor
 * @param b Second input tensor
 * @return Result tensor
 */
template <typename T>
Tensor<T> einsum(const std::string& notation, const Tensor<T>& a, const Tensor<T>& b)
{
    auto [in1, in2, out] = detail::parse_einsum_notation(notation);

    if (in2.empty())
    {
        throw std::invalid_argument("einsum: Two tensor version called with single-input notation");
    }

    // Matrix multiplication: ij,jk->ik
    if (in1 == "ij" && in2 == "jk" && out == "ik")
    {
        if (a.ndim() != 2 || b.ndim() != 2)
        {
            throw std::invalid_argument("einsum matmul: inputs must be rank 2");
        }
        if (a.shape()[1] != b.shape()[0])
        {
            throw std::invalid_argument("einsum matmul: incompatible dimensions");
        }

        size_t m = a.shape()[0];
        size_t n = a.shape()[1];
        size_t p = b.shape()[1];

        Tensor<T> result({m, p});
        result.fill(T{0});

        for (size_t i = 0; i < m; ++i)
        {
            for (size_t k = 0; k < p; ++k)
            {
                for (size_t j = 0; j < n; ++j)
                {
                    result(i, k) += a(i, j) * b(j, k);
                }
            }
        }
        return result;
    }

    // Batch matrix multiplication: bij,bjk->bik
    if (in1 == "bij" && in2 == "bjk" && out == "bik")
    {
        if (a.ndim() != 3 || b.ndim() != 3)
        {
            throw std::invalid_argument("einsum batch matmul: inputs must be rank 3");
        }
        if (a.shape()[0] != b.shape()[0])
        {
            throw std::invalid_argument("einsum batch matmul: batch sizes must match");
        }
        if (a.shape()[2] != b.shape()[1])
        {
            throw std::invalid_argument("einsum batch matmul: incompatible dimensions");
        }

        size_t batch = a.shape()[0];
        size_t m = a.shape()[1];
        size_t n = a.shape()[2];
        size_t p = b.shape()[2];

        Tensor<T> result({batch, m, p});
        result.fill(T{0});

        for (size_t b_idx = 0; b_idx < batch; ++b_idx)
        {
            for (size_t i = 0; i < m; ++i)
            {
                for (size_t k = 0; k < p; ++k)
                {
                    for (size_t j = 0; j < n; ++j)
                    {
                        result(b_idx, i, k) += a(b_idx, i, j) * b(b_idx, j, k);
                    }
                }
            }
        }
        return result;
    }

    // Outer product: i,j->ij
    if (in1 == "i" && in2 == "j" && out == "ij")
    {
        if (a.ndim() != 1 || b.ndim() != 1)
        {
            throw std::invalid_argument("einsum outer: inputs must be rank 1");
        }

        size_t m = a.shape()[0];
        size_t n = b.shape()[0];

        Tensor<T> result({m, n});

        for (size_t i = 0; i < m; ++i)
        {
            for (size_t j = 0; j < n; ++j)
            {
                result(i, j) = a[i] * b[j];
            }
        }
        return result;
    }

    // Inner product (dot): i,i->
    if (in1 == "i" && in2 == "i" && out.empty())
    {
        if (a.ndim() != 1 || b.ndim() != 1)
        {
            throw std::invalid_argument("einsum dot: inputs must be rank 1");
        }
        if (a.shape()[0] != b.shape()[0])
        {
            throw std::invalid_argument("einsum dot: vectors must have same length");
        }

        T sum = T{0};
        for (size_t i = 0; i < a.shape()[0]; ++i)
        {
            sum += a[i] * b[i];
        }

        Tensor<T> result({1});
        result[0] = sum;
        return result;
    }

    // Element-wise product: ij,ij->ij
    if (in1 == in2 && in1 == out)
    {
        if (a.ndim() != b.ndim())
        {
            throw std::invalid_argument("einsum element-wise: inputs must have same rank");
        }
        if (a.shape() != b.shape())
        {
            throw std::invalid_argument("einsum element-wise: inputs must have same shape");
        }

        Tensor<T> result(a.shape());
        for (size_t i = 0; i < a.size(); ++i)
        {
            result[i] = a[i] * b[i];
        }
        return result;
    }

    throw std::invalid_argument("einsum: Unsupported binary pattern: " + notation);
}

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Matrix multiplication using einsum
 * Equivalent to einsum("ij,jk->ik", a, b)
 */
template <typename T>
Tensor<T> matmul_einsum(const Tensor<T>& a, const Tensor<T>& b)
{
    return einsum("ij,jk->ik", a, b);
}

/**
 * @brief Batch matrix multiplication using einsum
 * Equivalent to einsum("bij,bjk->bik", a, b)
 */
template <typename T>
Tensor<T> batch_matmul_einsum(const Tensor<T>& a, const Tensor<T>& b)
{
    return einsum("bij,bjk->bik", a, b);
}

/**
 * @brief Outer product using einsum
 * Equivalent to einsum("i,j->ij", a, b)
 */
template <typename T>
Tensor<T> outer_einsum(const Tensor<T>& a, const Tensor<T>& b)
{
    return einsum("i,j->ij", a, b);
}

/**
 * @brief Dot product using einsum
 * Equivalent to einsum("i,i->", a, b)
 * @return Scalar result
 */
template <typename T>
T dot_einsum(const Tensor<T>& a, const Tensor<T>& b)
{
    auto result = einsum("i,i->", a, b);
    return result[0];
}

/**
 * @brief Transpose using einsum
 * Equivalent to einsum("ij->ji", a)
 */
template <typename T>
Tensor<T> transpose_einsum(const Tensor<T>& a)
{
    return einsum("ij->ji", a);
}

/**
 * @brief Trace using einsum
 * Equivalent to einsum("ii->", a)
 * @return Scalar result
 */
template <typename T>
T trace_einsum(const Tensor<T>& a)
{
    auto result = einsum("ii->", a);
    return result[0];
}

} // namespace fat_p
