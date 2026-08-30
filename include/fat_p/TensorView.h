#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorView
  file_role: public_header
  path: include/fat_p/TensorView.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for borrowed and shared-lifetime Tensor views."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/Tensor.h
      - include/fat_p/TensorLayout.h
      - include/fat_p/tensor/TensorView.h
    tests:
      - components/Tensor/tests/test_TensorView.cpp
      - components/Tensor/tests/test_TensorView_HeaderSelfContained.cpp
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

/**
 * @file TensorView.h
 * @brief Borrowed and shared-lifetime Tensor view public facade.
 */

#include "Tensor.h"
