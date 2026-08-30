/** @file test_TensorReductions_HeaderSelfContained.cpp @brief TensorReductions facade self-containment check. */

/*
FATP_META:
  meta_version: 1
  component: TensorReductions
  file_role: test
  path: components/Tensor/tests/test_TensorReductions_HeaderSelfContained.cpp
  namespace: ""
  layer: Testing
  summary: "Compile-only self-containment check for TensorReductions.h."
  api_stability: in_work
  related:
    docs: []
    headers:
      - include/fat_p/TensorReductions.h
    tests: []
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

#include "TensorReductions.h"

int main()
{
    fat_p::Tensor<int> value({}, 3);
    return fat_p::sum(value)() == 3 ? 0 : 1;
}
