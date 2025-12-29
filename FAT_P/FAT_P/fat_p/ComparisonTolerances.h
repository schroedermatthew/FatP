// ComparisonTolerances.h
#pragma once

#include <cmath>
#include <limits>

namespace fat_p {

/** @brief Default epsilon for double-precision floating-point comparisons. */
inline constexpr double kDefaultDoubleEpsilon = 
    std::numeric_limits<double>::epsilon() * 100.0; // Typical industry standard

/** @brief Default epsilon for single-precision floating-point comparisons. */
inline constexpr float kDefaultFloatEpsilon = 
    std::numeric_limits<float>::epsilon() * 100.0f; 

} // namespace fat_p
