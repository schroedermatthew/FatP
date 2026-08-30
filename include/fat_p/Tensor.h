#pragma once

/**
 * @file Tensor.h
 * @brief Public facade for allocator-aware dynamic Tensor ownership and views.
 */

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: public_header
  path: include/fat_p/Tensor.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for the dynamic Tensor API."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/User Manual - Tensor.md
    tests:
      - components/Tensor/tests/test_Tensor.cpp
      - components/Tensor/tests/test_TensorEquality.cpp
      - components/Tensor/tests/test_Tensor_HeaderSelfContained.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

#include "tensor/Tensor.h"
