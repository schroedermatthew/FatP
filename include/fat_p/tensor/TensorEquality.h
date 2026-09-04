#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorEquality
  file_role: internal_header
  path: include/fat_p/tensor/TensorEquality.h
  namespace: fat_p
  layer: Domain
  summary: "Internal Tensor integration for policy-based equality."
  api_stability: in_work
  related:
    docs_search: "TensorEquality"
    tests:
      - components/Tensor/tests/test_TensorEquality.cpp
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

/**
 * @file TensorEquality.h
 * @brief EqualityComparisons.h integration for Tensor
 *
 *
 * @version 1.0
 *
 * @details Provides EqualDispatcher specialization for Tensor to enable
 * policy-based comparison in test frameworks using the EqualityComparisons.h API.
 *
 * This allows using:
 *   - areEqual<Policy>(tensor1, tensor2, epsilon...)
 *   - Different comparison policies (StandardComparisonPolicy, StrictPolicy, etc.)
 *   - Detailed diagnostic messages on comparison failures
 *
 * Usage:
 *   #include "Tensor.h"
 *   #include "EqualityComparisons.h"
 *   #include "TensorEquality.h"
 *
 * Example:
 *   Tensor<float> a({100, 100}, 1.0f);
 *   Tensor<float> b({100, 100}, 1.0f + 1e-7f);
 *
 *   // Use StandardComparisonPolicy with custom epsilon
 *   bool result = areEqual<StandardComparisonPolicy>(a, b, 1e-6f);
 *
 *   // Use StrictPolicy for exact comparison
 *   bool exact = areEqual<StrictPolicy>(a, b);
 *
 * @note For readable owner/view comparisons, use exactEqual() or approxEqual()
 *       from TensorAlgorithms.h. For exact owner comparison, use operator==.
 *       Use this integration for test frameworks requiring policy-based comparison.
 *
 * @author cpp_utilities
 * @date 2025
 */

#include <cmath>
#include <sstream>
#include <string>

#include "EqualityComparisons.h"
#include "Tensor.h"

namespace fat_p
{

// =============================================================================
// EqualDispatcher Specialization for Tensor
// =============================================================================

/**
 * @brief Policy-based comparison for owning Tensor values
 *
 * @tparam T Element type
 * @tparam Alloc Allocator type
 * @tparam Policy Comparison policy (StandardComparisonPolicy, StrictPolicy, etc.)
 *
 * @details This specialization enables Tensor to work with the EqualityComparisons
 * framework. It performs:
 * 1. Shape comparison
 * 2. Element-wise logical comparison using Policy::epsilonMatch()
 *
 * Owning tensors are canonical contiguous values. Layout metadata is not part of
 * owner equality; readable owner/view comparison is provided by TensorAlgorithms.
 */
template <typename T, typename Alloc, std::size_t Rank, typename Policy>
struct EqualDispatcher<Tensor<T, Alloc, Rank>, Policy>
{
    /**
     * @brief Compare two tensors using the specified policy
     *
     * @param a First tensor
     * @param b Second tensor
     * @param eps Optional epsilon parameters for floating-point comparison
     * @return true if tensors are equal according to Policy, false otherwise
     *
     * @note This dispatcher integrates owning Tensor values with the policy framework.
     */
    template <typename... EpsParams>
    static bool
    compare(const Tensor<T, Alloc, Rank>& a, const Tensor<T, Alloc, Rank>& b, EpsParams... eps)
    {
        // Check shape match
        if (a.extents() != b.extents())
        {
            FATP_LOG_ERROR(([&]() {
                std::ostringstream oss;
                oss << "Tensor shapes differ:\n";
                oss << "  Expected shape: [";
                for (size_t i = 0; i < a.rank(); ++i)
                {
                    if (i > 0)
                    {
                        oss << ", ";
                    }
                    oss << a.extent(i);
                }
                oss << "]\n  Got shape: [";
                for (size_t i = 0; i < b.rank(); ++i)
                {
                    if (i > 0)
                    {
                        oss << ", ";
                    }
                    oss << b.extent(i);
                }
                oss << "]";
                return oss.str();
            })());
            return false;
        }

        // Check size match
        if (a.size() != b.size())
        {
            FATP_LOG_ERROR("Tensor sizes differ: " + std::to_string(a.size()) + " vs " + std::to_string(b.size()));
            return false;
        }

        bool all_equal = true;
        (void)tensor_detail::forEachPairKernel(a, b, [&](size_t logicalOffset, const T& value_a,
                                                        const T& value_b) {
            if constexpr (kStopOnFirstError)
            {
                if (!all_equal)
                {
                    return;
                }
            }
            const auto i = logicalOffset;
            // Use Policy's epsilon matching for comparison
            if constexpr (std::is_floating_point_v<T>)
            {
                // Floating-point comparison with policy
                if (!Policy::epsilonMatch(value_a, value_b, eps...))
                {
                    all_equal = false;
                    FATP_LOG_ERROR(([&]() {
                        std::ostringstream oss;

                        // Convert linear index to multi-dimensional indices
                        oss << "[";
                        size_t remaining = i;
                        for (size_t dim = 0; dim < a.rank(); ++dim)
                        {
                            if (dim > 0)
                            {
                                oss << ", ";
                            }
                            size_t dim_size = 1;
                            for (size_t j = dim + 1; j < a.rank(); ++j)
                            {
                                dim_size *= a.extent(j);
                            }
                            oss << (remaining / dim_size);
                            remaining %= dim_size;
                        }
                        oss << "]";

                        std::string indices = oss.str();
                        oss.str(""); // Clear the stream
                        oss.clear(); // Clear any error flags

                        oss << "Tensor elements differ at logical index " << i << " " << indices << ":\n";
                        oss << "  Expected: " << toString(value_a) << "\n";
                        oss << "  Got:      " << toString(value_b) << "\n";
                        oss << "  Diff:     " << toString(std::abs(value_a - value_b));

                        return oss.str();
                    })());
                    if constexpr (kStopOnFirstError)
                    {
                        return;
                    }
                }
            }
            else
            {
                // Integer/exact comparison
                if (value_a != value_b)
                {
                    all_equal = false;
                    FATP_LOG_ERROR("Tensor elements differ at logical index " + std::to_string(i) + ": " +
                                   toString(value_a) + " vs " + toString(value_b));

                    if constexpr (kStopOnFirstError)
                    {
                        return;
                    }
                }
            }
        });

        return all_equal;
    }
};

} // namespace fat_p
