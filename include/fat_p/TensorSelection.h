#pragma once

/** @file TensorSelection.h @brief Public facade for Tensor composition and indexed selection. */

/*
FATP_META:
  meta_version: 1
  component: TensorSelection
  file_role: public_header
  path: include/fat_p/TensorSelection.h
  namespace: fat_p
  layer: Domain
  summary: "Public facade for stack, concatenate, take, takeAlongAxis, and gatherND."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorAlgorithms.h
      - include/fat_p/tensor/TensorSelection.h
    tests:
      - components/Tensor/tests/test_TensorSelection.cpp
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

#include "tensor/TensorSelection.h"
