#pragma once

/** @file TensorMatmul.h @brief Dependency-free named Tensor linear algebra. */

/*
FATP_META:
  meta_version: 1
  component: TensorMatmul
  file_role: public_header
  path: include/fat_p/TensorMatmul.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for Tensor matmul, dot, outer, diagonal, and trace."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorReductions.h
      - include/fat_p/tensor/TensorMatmul.h
    tests:
      - components/Tensor/tests/test_TensorMatmul.cpp
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

#include "tensor/TensorMatmul.h"
