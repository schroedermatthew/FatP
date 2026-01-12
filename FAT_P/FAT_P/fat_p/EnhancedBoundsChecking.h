/**
 * @file EnhancedBoundsChecking.h
 * @brief Enhanced bounds checking with detailed diagnostics
 *
 * @layer Foundation
 */
#pragma once
/*
FATP_META:
  meta_version: 1
  component: EnhancedBoundsChecking
  file_role: public_header
  path: fat_p/EnhancedBoundsChecking.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for EnhancedBoundsChecking."
  api_stability: in_work
  related:
    docs_search: "EnhancedBoundsChecking"
    tests:
      - tests/test_EnhancedBoundsChecking.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include "enforce.h"
#include <string>
#include <sstream>
#include <stdexcept>

namespace fat_p {

// =============================================================================
// Enhanced Bounds Checking Functions
// =============================================================================

/**
 * @brief Always-on bounds checking with enhanced error message
 * @param index Index to check
 * @param min_val Minimum valid value (inclusive)
 * @param max_val Maximum valid value (exclusive)
 * @param context Description of what's being accessed
 * @throws std::out_of_range if index out of bounds
 */
template<typename T>
inline void bounds_check(T index, T min_val, T max_val, const char* context = "Index") {
    if (index < min_val || index >= max_val) {
        std::ostringstream oss;
        oss << context << " " << index << " out of range [" 
            << min_val << ", " << max_val << ")";
        throw std::out_of_range(oss.str());
    }
}

/**
 * @brief Debug-only bounds checking (compiled out in release builds)
 * @details Zero overhead when NDEBUG is defined
 */
template<typename T>
inline void debug_bounds_check(T index, T min_val, T max_val, 
                               const char* context = "Index") {
    #ifndef NDEBUG
    if (index < min_val || index >= max_val) {
        std::ostringstream oss;
        oss << context << " " << index << " out of range [" 
            << min_val << ", " << max_val << ")";
        throw std::out_of_range(oss.str());
    }
    #else
    (void)index; (void)min_val; (void)max_val; (void)context;
    #endif
}

/**
 * @brief Bounds checking with custom exception type
 */
template<typename ExceptionT, typename T>
inline void bounds_check_with(T index, T min_val, T max_val, const char* context = "Index") {
    if (index < min_val || index >= max_val) {
        std::ostringstream oss;
        oss << context << " " << index << " out of range [" 
            << min_val << ", " << max_val << ")";
        throw ExceptionT(oss.str());
    }
}

// =============================================================================
// Enforce-Based Bounds Checking (Using InRangePredicate)
// =============================================================================

/**
 * @brief Bounds check using enforce system (always enforced)
 * @details Provides better integration with enforce predicates
 */
template<typename T>
inline void enforce_bounds(T index, T min_val, T max_val, const char* context = "Index") {
    std::ostringstream oss;
    oss << context << " " << index << " must be in range [" 
        << min_val << ", " << max_val << ")";
    FATP_ALWAYS_ENFORCE_IN_RANGE(min_val, max_val, index, oss.str());
}

/**
 * @brief Debug-only enforce bounds (zero cost in release)
 */
template<typename T>
inline void debug_enforce_bounds(T index, T min_val, T max_val, 
                                const char* context = "Index") {
    #ifndef NDEBUG
    std::ostringstream oss;
    oss << context << " " << index << " must be in range [" 
        << min_val << ", " << max_val << ")";
    FATP_DEBUG_ENFORCE_IN_RANGE(min_val, max_val, index, oss.str());
    #else
    (void)index; (void)min_val; (void)max_val; (void)context;
    #endif
}

// =============================================================================
// Container-Specific Bounds Checking
// =============================================================================

/**
 * @brief Check container index bounds
 * @details Specialized for containers with .size() method
 */
template<typename Container>
inline void check_container_bounds(const Container& container, size_t index,
                                  const char* context = "Container index") {
    bounds_check(index, size_t{0}, container.size(), context);
}

/**
 * @brief Debug-only container bounds check
 */
template<typename Container>
inline void debug_check_container_bounds(const Container& container, size_t index,
                                        const char* context = "Container index") {
    debug_bounds_check(index, size_t{0}, container.size(), context);
}

// =============================================================================
// Multi-Dimensional Bounds Checking
// =============================================================================

/**
 * @brief Check 2D bounds (for matrices/tensors)
 */
template<typename T>
inline void bounds_check_2d(T row, T col, T rows, T cols, 
                           const char* context = "2D Index") {
    if (row < 0 || row >= rows || col < 0 || col >= cols) {
        std::ostringstream oss;
        oss << context << " (" << row << ", " << col 
            << ") out of bounds for shape (" << rows << ", " << cols << ")";
        throw std::out_of_range(oss.str());
    }
}

/**
 * @brief Debug-only 2D bounds check
 */
template<typename T>
inline void debug_bounds_check_2d(T row, T col, T rows, T cols,
                                 const char* context = "2D Index") {
    #ifndef NDEBUG
    bounds_check_2d(row, col, rows, cols, context);
    #else
    (void)row; (void)col; (void)rows; (void)cols; (void)context;
    #endif
}

/**
 * @brief Check N-dimensional bounds
 * @param indices Vector of indices
 * @param shape Vector of dimensions
 */
inline void bounds_check_nd(const std::vector<size_t>& indices,
                           const std::vector<size_t>& shape,
                           const char* context = "N-D Index") {
    if (indices.size() != shape.size()) {
        std::ostringstream oss;
        oss << context << " dimension mismatch: got " << indices.size() 
            << " indices for " << shape.size() << "D shape";
        throw std::invalid_argument(oss.str());
    }
    
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] >= shape[i]) {
            std::ostringstream oss;
            oss << context << " dimension " << i << ": index " << indices[i]
                << " out of range [0, " << shape[i] << ")";
            throw std::out_of_range(oss.str());
        }
    }
}

/**
 * @brief Debug-only N-dimensional bounds check
 */
inline void debug_bounds_check_nd(const std::vector<size_t>& indices,
                                 const std::vector<size_t>& shape,
                                 const char* context = "N-D Index") {
    #ifndef NDEBUG
    bounds_check_nd(indices, shape, context);
    #else
    (void)indices; (void)shape; (void)context;
    #endif
}

// =============================================================================
// Range Validation Helpers
// =============================================================================

/**
 * @brief Validate a range [start, end) is within [0, size)
 */
template<typename T>
inline void validate_range(T start, T end, T size, const char* context = "Range") {
    if (start < 0 || start > size || end < start || end > size) {
        std::ostringstream oss;
        oss << context << " [" << start << ", " << end 
            << ") invalid for size " << size;
        throw std::out_of_range(oss.str());
    }
}

/**
 * @brief Debug-only range validation
 */
template<typename T>
inline void debug_validate_range(T start, T end, T size, 
                                const char* context = "Range") {
    #ifndef NDEBUG
    validate_range(start, end, size, context);
    #else
    (void)start; (void)end; (void)size; (void)context;
    #endif
}

// =============================================================================
// Slice Validation
// =============================================================================

/**
 * @brief Validate slice parameters for tensor/array slicing
 * @param start Start index
 * @param stop Stop index (exclusive)
 * @param step Step size
 * @param size Container size
 */
template<typename T>
inline void validate_slice(T start, T stop, T step, T size,
                          const char* context = "Slice") {
    if (step == 0) {
        throw std::invalid_argument("Slice step cannot be zero");
    }
    
    if (step > 0) {
        if (start < 0 || start >= size) {
            std::ostringstream oss;
            oss << context << " start " << start << " out of range [0, " << size << ")";
            throw std::out_of_range(oss.str());
        }
        if (stop < start || stop > size) {
            std::ostringstream oss;
            oss << context << " stop " << stop << " invalid for start " 
                << start << " and size " << size;
            throw std::out_of_range(oss.str());
        }
    } else {
        // Negative step
        if (start >= size || stop < 0) {
            throw std::out_of_range("Invalid slice bounds for negative step");
        }
    }
}

/**
 * @brief Debug-only slice validation
 */
template<typename T>
inline void debug_validate_slice(T start, T stop, T step, T size,
                                const char* context = "Slice") {
    #ifndef NDEBUG
    validate_slice(start, stop, step, size, context);
    #else
    (void)start; (void)stop; (void)step; (void)size; (void)context;
    #endif
}

} // namespace fat_p

