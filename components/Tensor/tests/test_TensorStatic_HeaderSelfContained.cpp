/**
 * @file test_TensorStatic_HeaderSelfContained.cpp
 * @brief Compile-only self-containment test for TensorStatic.h.
 */

/*
FATP_META:
  meta_version: 1
  component: TensorStatic
  file_role: test
  path: components/Tensor/tests/test_TensorStatic_HeaderSelfContained.cpp
  namespace: fat_p::testing
  layer: Testing
  summary: "Compile-only self-containment check for TensorStatic.h."
  api_stability: in_work
  related:
    headers:
      - include/fat_p/TensorStatic.h
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

#include "TensorStatic.h"

int main()
{
    return 0;
}
