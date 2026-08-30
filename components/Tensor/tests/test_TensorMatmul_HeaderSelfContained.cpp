/** @file test_TensorMatmul_HeaderSelfContained.cpp @brief TensorMatmul facade self-containment check. */

/*
FATP_META:
  meta_version: 1
  component: TensorMatmul
  file_role: test
  path: components/Tensor/tests/test_TensorMatmul_HeaderSelfContained.cpp
  namespace: ""
  layer: Testing
  summary: "Compile-only self-containment check for TensorMatmul.h."
  api_stability: in_work
  related:
    docs: []
    headers:
      - include/fat_p/TensorMatmul.h
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

#include "TensorMatmul.h"

int main()
{
    fat_p::Tensor<int> left({1}, 2);
    fat_p::Tensor<int> right({1}, 3);
    return fat_p::matmul(left, right)() == 6 ? 0 : 1;
}
