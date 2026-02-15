---
doc_id: OV-ENHANCEDBOUNDSCHECKING-001
doc_type: "Overview"
title: "EnhancedBoundsChecking"
fatp_components: ["EnhancedBoundsChecking"]
topics: ["bounds checking", "index validation", "debug-only checks", "out_of_range", "enforce integration"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Overview - EnhancedBoundsChecking

*February 2026*

---

## What It Does

EnhancedBoundsChecking provides four bounds-checking functions with detailed error messages: `bounds_check()` (always-on, throws `std::out_of_range`), `debug_bounds_check()` (compiled away in release), `bounds_check_with<Exception>()` (custom exception type), and `enforce_bounds()` (integrates with Fat-P's enforce system). Each function validates that an index falls within `[min_val, max_val)` and produces a diagnostic message including the index value, valid range, and caller-provided context string.

## Why It Exists

`std::vector::at()` throws `std::out_of_range` with a message like "vector::_M_range_check"---useful for catching the error but useless for diagnosing it. Which element? What container? What was the valid range? EnhancedBoundsChecking answers all three questions in the exception message, turning a mystery crash into an actionable diagnostic.

## Architecture at a Glance

Single header (`EnhancedBoundsChecking.h`) in namespace `fat_p`. Six template functions. Depends on the Enforce component for `enforce_bounds()` and `debug_enforce_bounds()`. All functions are inline; no runtime cost beyond the check itself.

---

*EnhancedBoundsChecking.h --- Fat-P Library*
