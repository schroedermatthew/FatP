#pragma once

/** @file TensorInterop.h @brief Span, strided-descriptor, mdspan, and StaticTensor interop. */

/*
FATP_META:
  meta_version: 1
  component: TensorInterop
  file_role: public_header
  path: include/fat_p/TensorInterop.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for dependency-light Tensor interoperability."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/Tensor.h
      - include/fat_p/TensorStatic.h
      - include/fat_p/tensor/TensorInterop.h
    tests:
      - components/Tensor/tests/test_TensorInterop.cpp
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

#include "tensor/TensorInterop.h"
