/**
 * @file ComparisonTolerances.h
 * @brief Tolerance definitions for floating-point comparisons
 *
 * @layer Foundation
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: ComparisonTolerances
  file_role: public_header
  path: include/fat_p/ComparisonTolerances.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for ComparisonTolerances."
  api_stability: in_work
  related:
    docs_search: "ComparisonTolerances"
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
#include <cmath>
#include <limits>

namespace fat_p
{

/** @brief Default epsilon for double-precision floating-point comparisons. */
inline constexpr double kDefaultDoubleEpsilon =
    std::numeric_limits<double>::epsilon() * 100.0; // Typical industry standard

/** @brief Default epsilon for single-precision floating-point comparisons. */
inline constexpr float kDefaultFloatEpsilon = std::numeric_limits<float>::epsilon() * 100.0f;

} // namespace fat_p
