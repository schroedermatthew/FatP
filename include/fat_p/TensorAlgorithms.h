#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorAlgorithms
  file_role: public_header
  path: include/fat_p/TensorAlgorithms.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for serial dynamic Tensor algorithms."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/Tensor.h
      - include/fat_p/TensorView.h
      - include/fat_p/tensor/TensorAlgorithms.h
    tests:
      - components/Tensor/tests/test_TensorAlgorithms.cpp
      - components/Tensor/tests/test_TensorAlgorithms_HeaderSelfContained.cpp
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

/** @file TensorAlgorithms.h @brief Serial owner/view Tensor algorithms. */

#include "tensor/TensorAlgorithms.h"
