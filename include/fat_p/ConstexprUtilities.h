#pragma once

/*
FATP_META:
  meta_version: 1
  component: ConstexprUtilities
  file_role: public_header
  path: include/fat_p/ConstexprUtilities.h
  namespace: fat_p
  layer: Foundation
  summary: Umbrella header for constexpr hashing, bit ops, and string conversion.
  api_stability: candidate
  related:
    docs_search: "ConstexprUtilities"
    tests:
      - components/ConstexprUtilities/tests/test_ConstexprUtilities.cpp
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
 * @file ConstexprUtilities.h
 * @brief Umbrella header for compile-time utility functions.
 *
 * Includes three focused headers:
 *   - ConstexprHash.h: FNV-1a hashing and hash_combine
 *   - ConstexprBitOps.h: Power-of-two, log2, popcount, clz, ctz
 *   - ConstexprStringConversion.h: Integer/float/hex to_string_view,
 *     ConstexprString, constexpr_concat, and compile-time string utilities
 *
 * Existing code that includes ConstexprUtilities.h continues to work unchanged.
 * New code may include only the specific sub-header it needs to minimize
 * transitive dependencies.
 */

#include "ConstexprHash.h"
#include "ConstexprBitOps.h"
#include "ConstexprStringConversion.h"
