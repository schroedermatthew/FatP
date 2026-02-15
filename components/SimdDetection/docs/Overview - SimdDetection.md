---
doc_id: OV-SIMDDETECTION-001
doc_type: "Overview"
title: "SimdDetection"
fatp_components: ["SimdDetection"]
topics: ["SIMD", "SSE2", "AVX2", "AVX-512", "NEON", "compile-time detection", "runtime CPUID", "ISA hierarchy", "optimization hints", "register width"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "performance engineers", "AI assistants"]
status: "draft"
---

# Overview - SimdDetection

*February 2026*

---

## What It Does

SimdDetection answers two questions that every SIMD-accelerated component in Fat-P needs answered: "what SIMD instructions did we compile with?" and "what SIMD instructions does this CPU actually support?" It provides compile-time macros (`FATP_SIMD_AVX2`, `FATP_SIMD_NEON`, etc.) that gate instruction selection at build time, runtime query functions (`cpu_has_avx2()`, `cpu_has_avx512f()`) that detect hardware capabilities via CPUID, and a diagnostic API that compares the two and tells the user whether they are leaving performance on the table.

## Why It Exists

SIMD code faces a deployment problem. You compile with `-mavx2` on your development machine, ship the binary, and it crashes with SIGILL on a customer's older Haswell that only supports SSE4.2. Or you compile conservatively with `-msse2` and leave 4x throughput on the table for customers with AVX-512 hardware.

The standard solution is runtime dispatch: compile multiple code paths, detect the CPU at startup, and select the best path. SimdDetection provides the detection half of this pattern. Fat-P components like CheckedArithmetic, SimdVector, FastHashMap, and BitSet use SimdDetection's macros to compile the right intrinsics and its runtime API to verify the match at startup.

## Key Concepts

SimdDetection operates on three layers. The **compile-time macros** (`FATP_SIMD_*`) are set based on compiler flags and define which intrinsic headers are included. They form a hierarchy: if `FATP_SIMD_AVX2` is defined, then `FATP_SIMD_AVX`, `FATP_SIMD_SSE4_2`, `FATP_SIMD_SSE4_1`, `FATP_SIMD_SSE3`, and `FATP_SIMD_SSE2` are all also defined, because AVX2 implies all lower levels. The **runtime detection** uses CPUID (x86) to query the actual CPU, caching results in a static local for thread-safe one-time initialization. The **diagnostic API** compares compiled vs runtime and produces human-readable status strings and optimization hints.

## When To Use

Use SimdDetection when writing SIMD-accelerated code that must run on varying hardware, when you need to verify at startup that your binary matches the deployment CPU, or when reporting SIMD status in benchmarks and diagnostics. Every Fat-P benchmark prints `compiled_backend()` and `cpu_capability()` in its header.

## Architecture at a Glance

SimdDetection is a single header (`SimdDetection.h`) in namespace `fat_p::simd`. It depends on `PlatformDetection.h` for architecture macros. On x86, it includes the appropriate intrinsics header (`emmintrin.h` through `immintrin.h`) based on the detected level. On ARM, it includes `arm_neon.h` when NEON is available.

| API surface | Purpose |
|---|---|
| `FATP_SIMD_*` macros | Compile-time ISA gates |
| `FATP_HAS_SIMD`, `FATP_SIMD_LEVEL` | Unified availability checks |
| `cpu_has_sse2()` through `cpu_has_avx512bw()` | Runtime CPUID queries (x86) |
| `compiled_backend()`, `cpu_capability()` | Human-readable ISA names |
| `is_optimal()`, `optimization_hint()` | Mismatch detection |
| `register_width_bytes()`, `floats_per_register()` | Lane count constants |

## Relationship to Other Components

SimdDetection is a foundation header. It is consumed by CheckedArithmetic (overflow detection paths), SimdVector (vectorized operations), FastHashMap (SIMD probe sequences), BitSet (population count and scanning), HpcVector (BLAS-like operations), and FatPBenchmarkRunner (diagnostic output). It depends only on PlatformDetection.

---

*SimdDetection.h --- Fat-P Library*
