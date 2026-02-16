---
doc_id: BR-Stringify-001
doc_type: "Benchmark Results"
title: "Stringify"
fatp_components: ["Stringify"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - Stringify

**Source:** `benchmark_Stringify.cpp`
**Date:** February 2026
**Methodology:** Round-robin, randomized order, CPU-stabilized (local) / unstabilized (CI), median-primary

---

## Test Environments

| Property | Local (MSVC) | GCC CI | Clang CI | MSVC CI |
|----------|-------------|--------|----------|---------|
| OS | Windows 11 Pro | Ubuntu (Azure) | Ubuntu (Azure) | Windows (Azure) |
| Compiler | Windows-x64 MSVC-1950 | Linux-x64 GCC-14.2 | Linux-x64 Clang-17.0 | Windows-x64 MSVC-1944 |
| CPU | Intel Core Ultra 9 285K | Azure (shared) | Azure (shared) | Azure (shared) |
| RAM | 64 GB DDR5 | Shared tenancy | Shared tenancy | Shared tenancy |
| Measured runs | 15 | 20 | 20 | 20 |
| CPU stabilization | Yes | No | No | No |

**Competitors detected:**

| Library | Local | GCC | Clang | MSVC CI |
|---------|-------|-----|-------|---------|
| fat_p::stringify | x | x | x | x |
| std::to_string | x | x | x | x |
| std::ostringstream | x | x | x | x |
| std::format (C++20) | x | x | x | x |
| fmt::format | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
--- Section 1: Integer Stringification ---
Contract: Convert int to std::string. No locale formatting.

[2026-02-15 20:00:11] CPU: 2911 MHz (base: 3686)
  std::to_string (baseline)     7.56       ns  (mean: 7.80, stddev: 0.76)  CI95: [7.37, 8.22]
  fat_p::toString               8.68       ns  (mean: 9.02, stddev: 0.79)  CI95: [8.59, 9.46]
    -> fat_p::toString: 0.87x SLOWER than baseline
  std::format                   39.45      ns  (mean: 39.86, stddev: 1.28)  CI95: [39.15, 40.57]
    -> std::format: 0.19x SLOWER than baseline
  std::ostringstream            266.18     ns  (mean: 265.62, stddev: 5.01)  CI95: [262.85, 268.39]
    -> std::ostringstream: 0.03x SLOWER than baseline
  fmt::format                   18.83      ns  (mean: 18.98, stddev: 1.00)  CI95: [18.42, 19.53]
    -> fmt::format: 0.40x SLOWER than baseline

--- Section 2: Floating-Point Stringification ---
Contract: Convert double to std::string. Default precision.

[2026-02-15 20:00:11] CPU: 2174 MHz (base: 3686)
  std::to_string (baseline)     195.31     ns  (mean: 195.62, stddev: 4.52)  CI95: [193.12, 198.12]
  fat_p::toString               82.44      ns  (mean: 81.63, stddev: 2.76)  CI95: [80.11, 83.16]
    -> fat_p::toString: 2.37x FASTER than baseline
  std::format                   83.47      ns  (mean: 83.28, stddev: 2.84)  CI95: [81.71, 84.85]
    -> std::format: 2.34x FASTER than baseline
  std::ostringstream            388.00     ns  (mean: 387.77, stddev: 6.27)  CI95: [384.31, 391.24]
    -> std::ostringstream: 0.50x SLOWER than baseline
  fmt::format                   72.79      ns  (mean: 73.03, stddev: 1.68)  CI95: [72.10, 73.96]
    -> fmt::format: 2.68x FASTER than baseline

--- Section 3: Boolean Stringification ---
Contract: Convert bool to 'true'/'false' string.

[2026-02-15 20:00:13] CPU: 2322 MHz (base: 3686)
  ternary (baseline)            4.71       ns  (mean: 4.73, stddev: 0.32)  CI95: [4.56, 4.91]
  fat_p::toString               4.29       ns  (mean: 4.35, stddev: 0.15)  CI95: [4.27, 4.44]
    -> fat_p::toString: 1.10x FASTER than baseline

--- Section 4: Container Stringification ---
Contract: Convert vector<int> to '[elem, ...]' format. Fat-P unique.

[2026-02-15 20:00:13] CPU: 2469 MHz (base: 3686)
  manual loop (baseline)        107.90     ns  (mean: 106.80, stddev: 3.30)  CI95: [104.98, 108.62]
  fat_p::toString               106.50     ns  (mean: 107.96, stddev: 2.09)  CI95: [106.81, 109.11]
    -> fat_p::toString: 1.01x SAME than baseline

--- Section 5: Map Stringification ---
Contract: Convert map<string,int> to '{k: v, ...}' format. Fat-P unique.

[2026-02-15 20:00:13] CPU: 2469 MHz (base: 3686)
  manual loop (baseline)        64.80      ns  (mean: 65.27, stddev: 1.38)  CI95: [64.51, 66.04]
  fat_p::toString               60.70      ns  (mean: 60.97, stddev: 1.26)  CI95: [60.28, 61.67]
    -> fat_p::toString: 1.07x FASTER than baseline

--- Section 6: Tuple/Pair Stringification ---
Contract: Convert tuple to '(a, b, c)' format. Fat-P unique.

[2026-02-15 20:00:13] CPU: 2469 MHz (base: 3686)
  fat_p::toString(tuple)        107.96     ns  (mean: 108.25, stddev: 2.81)  CI95: [106.70, 109.81]
  fat_p::toString(pair)         39.43      ns  (mean: 40.10, stddev: 1.28)  CI95: [39.39, 40.81]

--- Section 7: Optional Stringification ---
Contract: Convert optional<int> to value or 'nullopt'.

[2026-02-15 20:00:13] CPU: 2580 MHz (base: 3686)
  fat_p::toString(has_value)    8.87       ns  (mean: 8.94, stddev: 0.17)  CI95: [8.85, 9.04]
  fat_p::toString(nullopt)      5.28       ns  (mean: 5.42, stddev: 0.21)  CI95: [5.30, 5.53]

--- Section 8: String Concatenation (toStringConcat) ---
Contract: Concatenate multiple values into single string.

[2026-02-15 20:00:13] CPU: 2580 MHz (base: 3686)
  manual + (baseline)           204.10     ns  (mean: 204.66, stddev: 3.41)  CI95: [202.78, 206.55]
  fat_p::toStringConcat         93.98      ns  (mean: 95.53, stddev: 2.42)  CI95: [94.19, 96.87]
    -> fat_p::toStringConcat: 2.17x FASTER than baseline
  std::format                   104.58     ns  (mean: 106.53, stddev: 7.17)  CI95: [102.57, 110.49]
    -> std::format: 1.95x FASTER than baseline

--- Correctness Verification ---
  Integer: PASS
  Container: PASS

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
--- Section 1: Integer Stringification ---
Contract: Convert int to std::string. No locale formatting.

[2026-02-16 03:37:51] CPU: 3242 MHz (~base: 3242)
  std::to_string (baseline)     19.85      ns  (mean: 19.84, stddev: 0.04)  CI95: [19.83, 19.86]
  fat_p::toString               18.19      ns  (mean: 18.18, stddev: 0.04)  CI95: [18.16, 18.20]
    -> fat_p::toString: 1.09x FASTER than baseline
  std::format                   42.84      ns  (mean: 43.00, stddev: 0.32)  CI95: [42.84, 43.15]
    -> std::format: 0.46x SLOWER than baseline
  std::ostringstream            107.82     ns  (mean: 107.89, stddev: 0.69)  CI95: [107.55, 108.22]
    -> std::ostringstream: 0.18x SLOWER than baseline
  fmt::format                   30.44      ns  (mean: 31.04, stddev: 1.97)  CI95: [30.10, 31.98]
    -> fmt::format: 0.65x SLOWER than baseline

--- Section 2: Floating-Point Stringification ---
Contract: Convert double to std::string. Default precision.

[2026-02-16 03:37:52] CPU: 2445 MHz (~base: 2445)
  std::to_string (baseline)     221.94     ns  (mean: 222.10, stddev: 0.59)  CI95: [221.82, 222.38]
  fat_p::toString               138.98     ns  (mean: 144.54, stddev: 17.11)  CI95: [136.35, 152.72]
    -> fat_p::toString: 1.60x FASTER than baseline
  std::format                   140.70     ns  (mean: 140.83, stddev: 0.46)  CI95: [140.61, 141.05]
    -> std::format: 1.58x FASTER than baseline
  std::ostringstream            370.70     ns  (mean: 370.89, stddev: 0.67)  CI95: [370.57, 371.21]
    -> std::ostringstream: 0.60x SLOWER than baseline
  fmt::format                   86.57      ns  (mean: 86.63, stddev: 0.34)  CI95: [86.47, 86.79]
    -> fmt::format: 2.56x FASTER than baseline

--- Section 3: Boolean Stringification ---
Contract: Convert bool to 'true'/'false' string.

[2026-02-16 03:37:54] CPU: 2445 MHz (~base: 2445)
  ternary (baseline)            2.77       ns  (mean: 2.80, stddev: 0.04)  CI95: [2.78, 2.81]
  fat_p::toString               2.47       ns  (mean: 2.49, stddev: 0.03)  CI95: [2.47, 2.50]
    -> fat_p::toString: 1.12x FASTER than baseline

--- Section 4: Container Stringification ---
Contract: Convert vector<int> to '[elem, ...]' format. Fat-P unique.

[2026-02-16 03:37:54] CPU: 2445 MHz (~base: 2445)
  manual loop (baseline)        97.49      ns  (mean: 96.98, stddev: 2.83)  CI95: [95.62, 98.33]
  fat_p::toString               227.03     ns  (mean: 228.49, stddev: 3.96)  CI95: [226.60, 230.38]
    -> fat_p::toString: 0.43x SLOWER than baseline

--- Section 5: Map Stringification ---
Contract: Convert map<string,int> to '{k: v, ...}' format. Fat-P unique.

[2026-02-16 03:37:54] CPU: 2445 MHz (~base: 2445)
  manual loop (baseline)        77.26      ns  (mean: 77.57, stddev: 1.69)  CI95: [76.76, 78.38]
  fat_p::toString               94.17      ns  (mean: 95.68, stddev: 2.04)  CI95: [94.70, 96.66]
    -> fat_p::toString: 0.82x SLOWER than baseline

--- Section 6: Tuple/Pair Stringification ---
Contract: Convert tuple to '(a, b, c)' format. Fat-P unique.

[2026-02-16 03:37:54] CPU: 2445 MHz (~base: 2445)
  fat_p::toString(tuple)        182.24     ns  (mean: 182.49, stddev: 1.25)  CI95: [181.90, 183.09]
  fat_p::toString(pair)         41.05      ns  (mean: 41.40, stddev: 1.09)  CI95: [40.88, 41.93]

--- Section 7: Optional Stringification ---
Contract: Convert optional<int> to value or 'nullopt'.

[2026-02-16 03:37:54] CPU: 2445 MHz (~base: 2445)
  fat_p::toString(has_value)    16.41      ns  (mean: 16.51, stddev: 0.29)  CI95: [16.37, 16.66]
  fat_p::toString(nullopt)      0.70       ns  (mean: 0.69, stddev: 0.00)  CI95: [0.69, 0.70]

--- Section 8: String Concatenation (toStringConcat) ---
Contract: Concatenate multiple values into single string.

[2026-02-16 03:37:54] CPU: 2445 MHz (~base: 2445)
  manual + (baseline)           131.65     ns  (mean: 133.86, stddev: 7.06)  CI95: [130.48, 137.24]
  fat_p::toStringConcat         170.77     ns  (mean: 194.60, stddev: 30.67)  CI95: [179.93, 209.28]
    -> fat_p::toStringConcat: 0.77x SLOWER than baseline
  std::format                   191.24     ns  (mean: 199.20, stddev: 25.33)  CI95: [187.07, 211.32]
    -> std::format: 0.69x SLOWER than baseline

--- Correctness Verification ---
  Integer: PASS
  Container: PASS

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
--- Section 1: Integer Stringification ---
Contract: Convert int to std::string. No locale formatting.

[2026-02-16 04:11:21] CPU: 3243 MHz (~base: 3243)
  std::to_string (baseline)     10.88      ns  (mean: 10.94, stddev: 0.19)  CI95: [10.85, 11.03]
  fat_p::toString               10.84      ns  (mean: 10.86, stddev: 0.05)  CI95: [10.83, 10.88]
    -> fat_p::toString: 1.00x SAME than baseline
  std::format                   32.58      ns  (mean: 32.58, stddev: 0.10)  CI95: [32.53, 32.62]
    -> std::format: 0.33x SLOWER than baseline
  std::ostringstream            117.98     ns  (mean: 118.14, stddev: 0.65)  CI95: [117.83, 118.45]
    -> std::ostringstream: 0.09x SLOWER than baseline
  fmt::format                   29.42      ns  (mean: 29.43, stddev: 0.09)  CI95: [29.39, 29.47]
    -> fmt::format: 0.37x SLOWER than baseline

--- Section 2: Floating-Point Stringification ---
Contract: Convert double to std::string. Default precision.

[2026-02-16 04:11:21] CPU: 3241 MHz (~base: 3241)
  std::to_string (baseline)     217.02     ns  (mean: 234.91, stddev: 46.17)  CI95: [212.82, 257.00]
  fat_p::toString               116.75     ns  (mean: 117.37, stddev: 2.44)  CI95: [116.20, 118.54]
    -> fat_p::toString: 1.86x FASTER than baseline
  std::format                   115.01     ns  (mean: 115.48, stddev: 1.07)  CI95: [114.97, 116.00]
    -> std::format: 1.89x FASTER than baseline
  std::ostringstream            368.83     ns  (mean: 368.92, stddev: 1.08)  CI95: [368.40, 369.43]
    -> std::ostringstream: 0.59x SLOWER than baseline
  fmt::format                   84.72      ns  (mean: 84.81, stddev: 0.29)  CI95: [84.67, 84.94]
    -> fmt::format: 2.56x FASTER than baseline

--- Section 3: Boolean Stringification ---
Contract: Convert bool to 'true'/'false' string.

[2026-02-16 04:11:23] CPU: 2445 MHz (~base: 2445)
  ternary (baseline)            2.31       ns  (mean: 2.19, stddev: 0.18)  CI95: [2.10, 2.27]
  fat_p::toString               2.32       ns  (mean: 2.28, stddev: 0.12)  CI95: [2.22, 2.34]
    -> fat_p::toString: 0.99x SAME than baseline

--- Section 4: Container Stringification ---
Contract: Convert vector<int> to '[elem, ...]' format. Fat-P unique.

[2026-02-16 04:11:23] CPU: 3238 MHz (~base: 3238)
  manual loop (baseline)        94.70      ns  (mean: 96.64, stddev: 6.55)  CI95: [93.50, 99.77]
  fat_p::toString               155.02     ns  (mean: 157.48, stddev: 6.89)  CI95: [154.18, 160.78]
    -> fat_p::toString: 0.61x SLOWER than baseline

--- Section 5: Map Stringification ---
Contract: Convert map<string,int> to '{k: v, ...}' format. Fat-P unique.

[2026-02-16 04:11:23] CPU: 3245 MHz (~base: 3245)
  manual loop (baseline)        87.58      ns  (mean: 88.44, stddev: 2.52)  CI95: [87.23, 89.64]
  fat_p::toString               88.27      ns  (mean: 90.45, stddev: 6.20)  CI95: [87.49, 93.42]
    -> fat_p::toString: 0.99x SAME than baseline

--- Section 6: Tuple/Pair Stringification ---
Contract: Convert tuple to '(a, b, c)' format. Fat-P unique.

[2026-02-16 04:11:23] CPU: 3155 MHz (~base: 3155)
  fat_p::toString(tuple)        154.92     ns  (mean: 155.21, stddev: 1.42)  CI95: [154.53, 155.88]
  fat_p::toString(pair)         41.58      ns  (mean: 41.52, stddev: 0.85)  CI95: [41.11, 41.93]

--- Section 7: Optional Stringification ---
Contract: Convert optional<int> to value or 'nullopt'.

[2026-02-16 04:11:23] CPU: 2445 MHz (~base: 2445)
  fat_p::toString(has_value)    5.55       ns  (mean: 5.64, stddev: 0.26)  CI95: [5.51, 5.76]
  fat_p::toString(nullopt)      0.62       ns  (mean: 0.62, stddev: 0.00)  CI95: [0.62, 0.62]

--- Section 8: String Concatenation (toStringConcat) ---
Contract: Concatenate multiple values into single string.

[2026-02-16 04:11:23] CPU: 2445 MHz (~base: 2445)
  manual + (baseline)           160.60     ns  (mean: 160.84, stddev: 1.57)  CI95: [160.08, 161.59]
  fat_p::toStringConcat         163.30     ns  (mean: 163.28, stddev: 1.28)  CI95: [162.67, 163.89]
    -> fat_p::toStringConcat: 0.98x SAME than baseline
  std::format                   152.30     ns  (mean: 152.40, stddev: 0.78)  CI95: [152.02, 152.77]
    -> std::format: 1.05x FASTER than baseline

--- Correctness Verification ---
  Integer: PASS
  Container: PASS

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
--- Section 1: Integer Stringification ---
Contract: Convert int to std::string. No locale formatting.

[2026-02-16 04:54:14] CPU: 2445 MHz (base: 2445)
  std::to_string (baseline)     13.31      ns  (mean: 13.39, stddev: 0.48)  CI95: [13.16, 13.62]
  fat_p::toString               16.06      ns  (mean: 16.12, stddev: 0.34)  CI95: [15.96, 16.29]
    -> fat_p::toString: 0.83x SLOWER than baseline
  std::format                   75.43      ns  (mean: 77.45, stddev: 5.36)  CI95: [74.88, 80.01]
    -> std::format: 0.18x SLOWER than baseline
  std::ostringstream            482.37     ns  (mean: 513.87, stddev: 63.56)  CI95: [483.46, 544.28]
    -> std::ostringstream: 0.03x SLOWER than baseline
  fmt::format                   32.15      ns  (mean: 32.28, stddev: 0.46)  CI95: [32.06, 32.51]
    -> fmt::format: 0.41x SLOWER than baseline

--- Section 2: Floating-Point Stringification ---
Contract: Convert double to std::string. Default precision.

[2026-02-16 04:54:16] CPU: 2445 MHz (base: 2445)
  std::to_string (baseline)     457.91     ns  (mean: 470.63, stddev: 31.54)  CI95: [455.54, 485.72]
  fat_p::toString               213.79     ns  (mean: 235.14, stddev: 32.28)  CI95: [219.70, 250.59]
    -> fat_p::toString: 2.14x FASTER than baseline
  std::format                   205.08     ns  (mean: 215.32, stddev: 18.03)  CI95: [206.69, 223.94]
    -> std::format: 2.23x FASTER than baseline
  std::ostringstream            835.89     ns  (mean: 849.52, stddev: 37.12)  CI95: [831.76, 867.29]
    -> std::ostringstream: 0.55x SLOWER than baseline
  fmt::format                   141.66     ns  (mean: 143.29, stddev: 7.21)  CI95: [139.84, 146.74]
    -> fmt::format: 3.23x FASTER than baseline

--- Section 3: Boolean Stringification ---
Contract: Convert bool to 'true'/'false' string.

[2026-02-16 04:54:20] CPU: 2445 MHz (base: 2445)
  ternary (baseline)            10.13      ns  (mean: 10.22, stddev: 0.74)  CI95: [9.86, 10.57]
  fat_p::toString               10.01      ns  (mean: 10.02, stddev: 0.15)  CI95: [9.95, 10.09]
    -> fat_p::toString: 1.01x SAME than baseline

--- Section 4: Container Stringification ---
Contract: Convert vector<int> to '[elem, ...]' format. Fat-P unique.

[2026-02-16 04:54:20] CPU: 2445 MHz (base: 2445)
  manual loop (baseline)        194.25     ns  (mean: 197.09, stddev: 8.10)  CI95: [193.21, 200.96]
  fat_p::toString               269.25     ns  (mean: 274.78, stddev: 11.52)  CI95: [269.27, 280.29]
    -> fat_p::toString: 0.72x SLOWER than baseline

--- Section 5: Map Stringification ---
Contract: Convert map<string,int> to '{k: v, ...}' format. Fat-P unique.

[2026-02-16 04:54:20] CPU: 2445 MHz (base: 2445)
  manual loop (baseline)        124.70     ns  (mean: 125.95, stddev: 5.81)  CI95: [123.17, 128.73]
  fat_p::toString               135.30     ns  (mean: 138.55, stddev: 8.01)  CI95: [134.72, 142.38]
    -> fat_p::toString: 0.92x SLOWER than baseline

--- Section 6: Tuple/Pair Stringification ---
Contract: Convert tuple to '(a, b, c)' format. Fat-P unique.

[2026-02-16 04:54:20] CPU: 2445 MHz (base: 2445)
  fat_p::toString(tuple)        236.35     ns  (mean: 237.32, stddev: 3.14)  CI95: [235.81, 238.82]
  fat_p::toString(pair)         83.78      ns  (mean: 84.57, stddev: 2.16)  CI95: [83.53, 85.61]

--- Section 7: Optional Stringification ---
Contract: Convert optional<int> to value or 'nullopt'.

[2026-02-16 04:54:20] CPU: 2445 MHz (base: 2445)
  fat_p::toString(has_value)    16.97      ns  (mean: 17.60, stddev: 1.15)  CI95: [17.05, 18.16]
  fat_p::toString(nullopt)      9.27       ns  (mean: 9.37, stddev: 0.43)  CI95: [9.16, 9.58]

--- Section 8: String Concatenation (toStringConcat) ---
Contract: Concatenate multiple values into single string.

[2026-02-16 04:54:20] CPU: 2445 MHz (base: 2445)
  manual + (baseline)           527.03     ns  (mean: 562.58, stddev: 102.63)  CI95: [513.47, 611.69]
  fat_p::toStringConcat         197.03     ns  (mean: 197.33, stddev: 1.59)  CI95: [196.57, 198.09]
    -> fat_p::toStringConcat: 2.67x FASTER than baseline
  std::format                   341.51     ns  (mean: 325.62, stddev: 78.91)  CI95: [287.86, 363.38]
    -> std::format: 1.54x FASTER than baseline

--- Correctness Verification ---
  Integer: PASS
  Container: PASS

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
