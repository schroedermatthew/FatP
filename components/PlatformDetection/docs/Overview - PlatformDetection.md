---
doc_id: OV-PLATFORMDETECTION-001
doc_type: "Overview"
title: "PlatformDetection"
fatp_components: ["PlatformDetection"]
topics: ["compiler detection", "OS detection", "architecture detection", "NUMA detection", "debug detection"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Overview - PlatformDetection

*February 2026*

---

## What It Does

PlatformDetection provides compile-time macros that identify the compiler (`FATP_COMPILER_CLANG`, `FATP_COMPILER_GCC`, `FATP_COMPILER_MSVC`), operating system (`FATP_PLATFORM_WINDOWS`, `FATP_PLATFORM_LINUX`, `FATP_PLATFORM_MACOS`), CPU architecture (`FATP_ARCH_X64`, `FATP_ARCH_ARM64`), hardware features (`FATP_HAS_NUMA`, `FATP_CACHE_LINE_SIZE`), and build configuration (`FATP_BUILD_DEBUG`, `FATP_BUILD_RELEASE`). All Fat-P headers use these macros instead of probing vendor-specific macros directly.

## Why It Exists

Every platform-conditional `#ifdef` in the library needs the same set of checks: is this MSVC? is this ARM64? is NUMA available? Without centralization, each header re-derives these from raw vendor macros (`_MSC_VER`, `__aarch64__`, etc.), creating inconsistency and maintenance burden. PlatformDetection is the single source of truth.

## Architecture at a Glance

Single header (`PlatformDetection.h`), no dependencies, no namespace (macro-only). Defines approximately 20 macros. Consumed by nearly every other Fat-P header, most notably SimdDetection (which layers SIMD macros on top of the architecture macros) and FatPBenchmarkRunner (which uses OS macros for platform-specific priority/affinity).

---

*PlatformDetection.h --- Fat-P Library*
