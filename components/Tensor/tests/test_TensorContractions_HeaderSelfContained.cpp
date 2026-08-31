/** @file test_TensorContractions_HeaderSelfContained.cpp @brief Standalone contraction facade check. */

/*
FATP_META:
  meta_version: 1
  component: TensorContractions
  file_role: test
  path: components/Tensor/tests/test_TensorContractions_HeaderSelfContained.cpp
  namespace: ""
  layer: Testing
  summary: "Standalone contraction facade instantiation."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorContractions.h
    tests:
      - components/Tensor/tests/test_TensorContractions.cpp
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

#include "TensorContractions.h"

int main()
{
    const fat_p::Tensor<int> a({2, 3}, 2);
    const fat_p::Tensor<int> b({3, 4}, 3);
    const auto result = fat_p::tensorDot(a, b, {1}, {0});
    return result.size() == 8 && result[0] == 18 ? 0 : 1;
}
