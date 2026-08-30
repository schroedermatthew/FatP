/** @file test_TensorSelection_HeaderSelfContained.cpp @brief TensorSelection facade self-containment check. */

/*
FATP_META:
  meta_version: 1
  component: TensorSelection
  file_role: test
  path: components/Tensor/tests/test_TensorSelection_HeaderSelfContained.cpp
  namespace: ""
  layer: Testing
  summary: "Compile-only self-containment check for TensorSelection.h."
  api_stability: in_work
  related:
    docs: []
    headers:
      - include/fat_p/TensorSelection.h
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

#include "TensorSelection.h"

#include <array>
#include <cstddef>
#include <span>

int main()
{
    fat_p::Tensor<int> source({2}, 1);
    const std::array<std::ptrdiff_t, 1> indices = {0};
    return fat_p::take(source, std::span<const std::ptrdiff_t>(indices))[0] == 1 ? 0 : 1;
}
