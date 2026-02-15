---
doc_id: OV-FATPBENCHMARKRUNNER-001
doc_type: "Overview"
title: "FatPBenchmarkRunner"
fatp_components: ["FatPBenchmarkRunner"]
topics: ["benchmark", "performance measurement", "statistical analysis", "CI95", "round-robin comparison", "CPU frequency monitoring", "DoNotOptimize", "BenchmarkScope", "CSV export", "JSON export", "warm-up", "cooldown", "outlier removal"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "performance engineers", "AI assistants"]
status: "draft"
---

# Overview - FatPBenchmarkRunner

*February 2026*

---

## What It Does

FatPBenchmarkRunner is Fat-P's unified benchmarking infrastructure. It measures operation latency with nanosecond precision, computes statistical summaries (median, CI95, percentiles), supports round-robin multi-library comparison with randomized execution order, monitors CPU frequency to detect thermal throttling, and exports results to CSV and JSON for CI integration. Every benchmark in the Fat-P project uses this single header.

## Why It Exists

Microbenchmarking is deceptively hard. Naive timing (`clock()` around a loop) produces results corrupted by compiler dead-code elimination, CPU frequency scaling, cache warm-up effects, and OS scheduling noise. Each of these failure modes produces plausible-looking but wrong numbers.

FatPBenchmarkRunner addresses each failure mode systematically. `DoNotOptimize()` prevents the compiler from eliminating measured code. `BenchmarkScope` sets process priority and CPU affinity (Windows). CPU frequency monitoring detects throttling. Configurable warm-up runs prime the cache. Cooldown delays between sections prevent thermal cross-contamination. Statistical analysis with CI95 confidence intervals quantifies measurement uncertainty.

## Key Concepts

The runner operates on a **section/benchmark/run** hierarchy. Sections group related benchmarks (e.g., "CORE OPERATIONS", "SCALING"). Within each section, individual benchmarks are registered with `add()` or `compare()`. Each benchmark executes `warmupRuns` unmeasured iterations followed by `measuredRuns` timed iterations. For multi-library comparison, library execution order is randomized per run (round-robin) so all libraries observe the same distribution of machine states.

The five design invariants are: each measured run executes exactly one timed iteration per library; library order is randomized per run; setup/teardown occur outside timed regions; all libraries observe the same machine state distribution; medians are the primary reported statistic.

## When To Use

Use FatPBenchmarkRunner for any performance measurement in the Fat-P project. Use `makeRunner()` for production benchmarks (with CPU stabilization, priority boost, cooldown delays). Use `makeTestRunner()` for unit tests that verify benchmark infrastructure without the 30-second startup overhead.

## Architecture at a Glance

Single header: `FatPBenchmarkRunner.h` in namespace `fat_p::bench`. Key types: `BenchmarkRunner` (orchestrator), `BenchConfig` (configuration from environment variables), `BenchmarkScope` (Windows priority/affinity RAII), `Statistics` (median/CI95/percentiles), `IAdapter` (interface for multi-library comparison), `DoNotOptimize()` (dead-code prevention), `SpinBarrier` (concurrent benchmark synchronization).

## Relationship to Other Components

FatPBenchmarkRunner depends on PlatformDetection (OS detection) and SimdDetection (SIMD status in diagnostic output). It is consumed by every `benchmark_*.cpp` file in the project. It does not depend on any other Fat-P runtime component, making it safe to include in benchmark executables without pulling in the entire library.

---

*FatPBenchmarkRunner.h --- Fat-P Library*
