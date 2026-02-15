---
doc_id: OV-CPPFEATUREDETECTION-001
doc_type: "Overview"
title: "CppFeatureDetection"
fatp_components: ["CppFeatureDetection"]
topics: ["C++ standard detection", "feature test macros", "FATP_HAS_FORMAT", "FATP_HAS_COROUTINES", "FATP_HAS_JTHREAD", "library availability"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Overview - CppFeatureDetection

*February 2026*

---

## What It Does

CppFeatureDetection provides compile-time macros for C++ standard version (`FATP_CPP20_OR_LATER`, `FATP_CPP23_OR_LATER`, `FATP_CPP26_OR_LATER`) and library feature availability (`FATP_HAS_FORMAT`, `FATP_HAS_COROUTINES`, `FATP_HAS_JTHREAD`, `FATP_HAS_MODULES`, `FATP_HAS_ATOMIC_WAIT`, `FATP_HAS_LATCH`, `FATP_HAS_BARRIER`, `FATP_HAS_SEMAPHORE`). Fat-P headers use these instead of probing `__cplusplus` or `__has_include` directly.

## Why It Exists

A C++20 compiler with an older standard library may not have all C++20 library features. `__cplusplus >= 202002L` tells you the language mode, not whether `<format>` or `<coroutine>` is available. CppFeatureDetection probes actual library headers and feature-test macros to determine what is usable, handling the MSVC `_MSVC_LANG` quirk (MSVC reports `__cplusplus` as C++14 unless `/Zc:__cplusplus` is passed).

## Architecture at a Glance

Single header (`CppFeatureDetection.h`), no dependencies. Defines approximately 10 macros. Consumed by CoroutineTask (gates on `FATP_HAS_COROUTINES`), DiagnosticLogger (uses `FATP_HAS_FORMAT`), and other components that conditionally use C++20/23 library features.

---

*CppFeatureDetection.h --- Fat-P Library*
