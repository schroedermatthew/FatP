---
doc_id: OV-FATPCONFIG-001
doc_type: "Overview"
title: "FatPConfig"
fatp_components: ["FatPConfig"]
topics: ["configuration macros", "FATP_NO_UNIQUE_ADDRESS", "FATP_LIKELY", "FATP_FORCEINLINE", "FATP_NOINLINE", "cache line size", "iostream toggle"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Overview - FatPConfig

*February 2026*

---

## What It Does

FatPConfig defines library-wide configuration and portability macros: `FATP_NO_UNIQUE_ADDRESS` (portable `[[no_unique_address]]`), `FATP_LIKELY`/`FATP_UNLIKELY` (branch hints), `FATP_FORCEINLINE`/`FATP_NOINLINE` (inlining control), `FATP_CACHE_LINE_SIZE` (default 64 bytes), and `FATP_ENABLE_IOSTREAM`. These macros abstract compiler-specific syntax into portable names used throughout the library.

## Why It Exists

`[[no_unique_address]]` is spelled `[[msvc::no_unique_address]]` on MSVC. `__builtin_expect` exists on GCC/Clang but not MSVC. `__forceinline` is MSVC-only; `__attribute__((always_inline))` is GCC/Clang-only. FatPConfig provides a single portable spelling for each, following the Systemic Hygiene Policy Rule F: single source of truth for configuration macros.

## Architecture at a Glance

Single header (`FatPConfig.h`), depends on compiler detection only. Defines approximately 8 macros and one `constexpr` constant (`fat_p::config::cache_line_size`). Consumed by nearly every Fat-P header.

---

*FatPConfig.h --- Fat-P Library*
