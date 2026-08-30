/** @file test_TensorInterop_HeaderSelfContained.cpp @brief TensorInterop facade self-containment check. */

/*
FATP_META:
  meta_version: 1
  component: TensorInterop
  file_role: test
  path: components/Tensor/tests/test_TensorInterop_HeaderSelfContained.cpp
  namespace: ""
  layer: Testing
  summary: "Compile-only self-containment check for TensorInterop.h."
  api_stability: in_work
  related:
    docs: []
    headers:
      - include/fat_p/TensorInterop.h
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

#include "TensorInterop.h"

int main()
{
    fat_p::Tensor<int> value({}, 3);
    return fat_p::contiguousSpan(value).front() == 3 ? 0 : 1;
}
