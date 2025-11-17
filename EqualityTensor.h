/**
 * @file Tensor_EqualityComparisons.h
 * @brief EqualityComparisons.h integration for Tensor
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
 *   #include "Tensor_EqualityComparisons.h"  // This file
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
 * @note For simple floating-point comparisons, use Tensor::approx_equal() instead.
 *       For exact comparisons, use operator==.
 *       Use this integration for test frameworks requiring policy-based comparison.
 * 
 * @author cpp_utilities
 * @date 2025
 */

#pragma once

#include "Tensor.h"
#include "EqualityComparisons.h"

namespace fat_p {

// =============================================================================
// EqualDispatcher Specialization for Tensor
// =============================================================================

/**
 * @brief Policy-based comparison for Tensor<T, Alloc, IteratorPolicy>
 * 
 * @tparam T Element type
 * @tparam Alloc Allocator type
 * @tparam IteratorPolicy Iterator policy (RowMajorIterator, ColumnMajorIterator, etc.)
 * @tparam Policy Comparison policy (StandardComparisonPolicy, StrictPolicy, etc.)
 * 
 * @details This specialization enables Tensor to work with the EqualityComparisons
 * framework. It performs:
 * 1. Shape comparison
 * 2. Strides comparison
 * 3. Element-wise comparison using Policy::epsilonMatch()
 * 
 * The comparison is stride-aware and uses Tensor's iterator system for correct
 * traversal of non-contiguous views.
 */
template <typename T, typename Alloc, typename IteratorPolicy, typename Policy>
struct EqualDispatcher<Tensor<T, Alloc, IteratorPolicy>, Policy> {
    /**
     * @brief Compare two tensors using the specified policy
     * 
     * @param a First tensor
     * @param b Second tensor
     * @param eps Optional epsilon parameters for floating-point comparison
     * @return true if tensors are equal according to Policy, false otherwise
     * 
     * @note Comparison is stride-aware and works correctly with views and slices.
     */
    template <typename... EpsParams>
    static bool compare(const Tensor<T, Alloc, IteratorPolicy>& a,
                        const Tensor<T, Alloc, IteratorPolicy>& b,
                        EpsParams... eps) {
        // Check shape match
        if (a.shape() != b.shape()) {
            diagnostic::conditionalPrintError([&]() -> std::string {
                std::string msg = "Tensor shapes differ:\n";
                msg += "  Expected shape: [";
                for (size_t i = 0; i < a.shape().size(); ++i) {
                    if (i > 0) msg += ", ";
                    msg += std::to_string(a.shape()[i]);
                }
                msg += "]\n  Got shape: [";
                for (size_t i = 0; i < b.shape().size(); ++i) {
                    if (i > 0) msg += ", ";
                    msg += std::to_string(b.shape()[i]);
                }
                msg += "]";
                return msg;
            });
            return false;
        }
        
        // Check strides match (important for views)
        if (a.strides() != b.strides()) {
            diagnostic::conditionalPrintError([&]() -> std::string {
                std::string msg = "Tensor strides differ (different memory layouts):\n";
                msg += "  Expected strides: [";
                for (size_t i = 0; i < a.strides().size(); ++i) {
                    if (i > 0) msg += ", ";
                    msg += std::to_string(a.strides()[i]);
                }
                msg += "]\n  Got strides: [";
                for (size_t i = 0; i < b.strides().size(); ++i) {
                    if (i > 0) msg += ", ";
                    msg += std::to_string(b.strides()[i]);
                }
                msg += "]";
                return msg;
            });
            return false;
        }
        
        // Check size match
        if (a.size() != b.size()) {
            diagnostic::conditionalPrintError([&]() -> std::string {
                return "Tensor sizes differ: " + 
                       std::to_string(a.size()) + " vs " + std::to_string(b.size());
            });
            return false;
        }
        
        // Stride-aware element comparison using iterators
        // This correctly handles non-contiguous views and different memory layouts
        auto it_a = a.begin();
        auto it_b = b.begin();
        
        for (size_t i = 0; i < a.size(); ++i, ++it_a, ++it_b) {
            // Use Policy's epsilon matching for comparison
            if constexpr (std::is_floating_point_v<T>) {
                // Floating-point comparison with policy
                if (!Policy::epsilonMatch(*it_a, *it_b, eps...)) {
                    diagnostic::conditionalPrintError([&]() -> std::string {
                        // Convert linear index to multi-dimensional indices
                        std::string indices = "[";
                        size_t remaining = i;
                        for (size_t dim = 0; dim < a.shape().size(); ++dim) {
                            if (dim > 0) indices += ", ";
                            size_t dim_size = 1;
                            for (size_t j = dim + 1; j < a.shape().size(); ++j) {
                                dim_size *= a.shape()[j];
                            }
                            indices += std::to_string(remaining / dim_size);
                            remaining %= dim_size;
                        }
                        indices += "]";
                        
                        return "Tensor elements differ at logical index " + 
                               std::to_string(i) + " " + indices + ":\n" +
                               "  Expected: " + toString(*it_a) + "\n" +
                               "  Got:      " + toString(*it_b) + "\n" +
                               "  Diff:     " + toString(std::abs(*it_a - *it_b));
                    });
                    
                    if constexpr (kStopOnFirstError) {
                        return false;
                    }
                }
            } else {
                // Integer/exact comparison
                if (*it_a != *it_b) {
                    diagnostic::conditionalPrintError([&]() -> std::string {
                        return "Tensor elements differ at logical index " + 
                               std::to_string(i) + ": " +
                               toString(*it_a) + " vs " + toString(*it_b);
                    });
                    
                    if constexpr (kStopOnFirstError) {
                        return false;
                    }
                }
            }
        }
        
        return true;
    }
};

} // namespace fat_p
