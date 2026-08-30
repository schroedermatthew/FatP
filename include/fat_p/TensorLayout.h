#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorLayout
  file_role: public_header
  path: include/fat_p/TensorLayout.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for checked runtime Tensor extents and layouts."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Semantic Contract.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/User Manual - TensorLayout.md
    headers:
      - include/fat_p/tensor/TensorExtents.h
      - include/fat_p/tensor/TensorLayout.h
    tests:
      - components/Tensor/tests/test_TensorLayout.cpp
      - components/Tensor/tests/test_TensorLayout_HeaderSelfContained.cpp
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
 * @file TensorLayout.h
 * @brief Checked runtime extents and pointer-free Tensor layout metadata.
 */

#include "tensor/TensorLayout.h"
