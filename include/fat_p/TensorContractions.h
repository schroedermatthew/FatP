#pragma once

/** @file TensorContractions.h @brief Explicit-axis Tensor contractions; serial by default. */

/*
FATP_META:
  meta_version: 1
  component: TensorContractions
  file_role: public_header
  path: include/fat_p/TensorContractions.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for explicit-axis tensorDot contractions."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/tensor/TensorContractions.h
      - include/fat_p/TensorMatmul.h
    tests:
      - components/Tensor/tests/test_TensorContractions.cpp
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

#include "tensor/TensorContractions.h"
