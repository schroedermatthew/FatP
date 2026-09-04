#pragma once

/*
FATP_META:
  meta_version: 1
  component: TensorRanked
  file_role: public_header
  path: include/fat_p/TensorRanked.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for fixed-rank, runtime-extents Tensor types."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
      - components/Tensor/docs/User Manual - TensorRanked.md
    headers:
      - include/fat_p/tensor/TensorRanked.h
    tests:
      - components/Tensor/tests/test_TensorRanked.cpp
      - components/Tensor/tests/test_TensorRanked_HeaderSelfContained.cpp
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

/** @file TensorRanked.h @brief Fixed-rank, runtime-extents Tensor ownership and views. */

#include "tensor/TensorRanked.h"
