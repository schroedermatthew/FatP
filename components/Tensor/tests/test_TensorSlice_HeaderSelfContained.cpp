/** @file test_TensorSlice_HeaderSelfContained.cpp @brief TensorSlice facade self-containment check. */

/*
FATP_META:
  meta_version: 1
  component: TensorSlice
  file_role: test
  path: components/Tensor/tests/test_TensorSlice_HeaderSelfContained.cpp
  namespace: ""
  layer: Testing
  summary: "Compile-only self-containment check for TensorSlice.h."
  api_stability: in_work
  related:
    docs: []
    headers:
      - include/fat_p/TensorSlice.h
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

#include "TensorSlice.h"

int main()
{
    const fat_p::Slice slice{0, 4, 2};
    const fat_p::SliceSpec specification = slice;
    return std::get<fat_p::Slice>(specification).step == 2 ? 0 : 1;
}
