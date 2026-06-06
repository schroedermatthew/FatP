/**
 * @file test_Jet_HeaderSelfContained.cpp
 * @brief Compile-only header self-contained test for Jet.h.
 *
 * Verifies that Jet.h compiles when included alone in an otherwise empty
 * translation unit (no transitive-include luck), and that including it twice
 * is idempotent (#pragma once). This file is compiled, not run.
 */
/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: test
  path: components/Jet/tests/test_Jet_HeaderSelfContained.cpp
  namespace: fat_p::autodiff
  layer: Testing
  summary: Compile-only self-contained and idempotence check for Jet.h.
  api_stability: in_work
  related:
    headers:
      - include/fat_p/Jet.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include "Jet.h"
#include "Jet.h" // idempotence under #pragma once

int main()
{
    return 0;
}
