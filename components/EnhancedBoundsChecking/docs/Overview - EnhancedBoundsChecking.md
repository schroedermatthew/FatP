---
doc_id: OV-ENHANCEDBOUNDSCHECKING-001
doc_type: "Overview"
title: "EnhancedBoundsChecking"
fatp_components: ["EnhancedBoundsChecking"]
topics: ["bounds checking", "index validation", "debug-only checks", "out_of_range", "enforce integration", "container bounds", "multi-dimensional bounds", "range validation", "slice validation"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Overview - EnhancedBoundsChecking

*February 2026*

---

## What It Does

EnhancedBoundsChecking provides fifteen bounds-checking functions with detailed error messages, organized as eight check families (most with a `debug_`-prefixed variant that compiles away in release): `bounds_check()`/`debug_bounds_check()` (single index, throws `std::out_of_range`), `bounds_check_with<Exception>()` (custom exception type), `enforce_bounds()`/`debug_enforce_bounds()` (integrates with Fat-P's enforce system), `check_container_bounds()`/`debug_check_container_bounds()` (index against a container's `.size()`), `bounds_check_2d()`/`debug_bounds_check_2d()` (row/column against a matrix shape), `bounds_check_nd()`/`debug_bounds_check_nd()` (N-dimensional index vectors), `validate_range()`/`debug_validate_range()` (a `[start, end)` range against a size), and `validate_slice()`/`debug_validate_slice()` (start/stop/step slice parameters). Every check produces a diagnostic message including the offending value(s), the valid range or shape, and a caller-provided context string.

## Why It Exists

`std::vector::at()` throws `std::out_of_range` with a message like "vector::_M_range_check"---useful for catching the error but useless for diagnosing it. Which element? What container? What was the valid range? EnhancedBoundsChecking answers all three questions in the exception message, turning a mystery crash into an actionable diagnostic.

## Architecture at a Glance

Single header (`EnhancedBoundsChecking.h`) in namespace `fat_p`. Fifteen inline functions in eight families (single-index, custom-exception, enforce-based, container, 2D, N-D, range, and slice checks). Depends on the Enforce component for `enforce_bounds()` and `debug_enforce_bounds()`. All functions are inline; no runtime cost beyond the check itself.

---

*EnhancedBoundsChecking.h --- Fat-P Library*
