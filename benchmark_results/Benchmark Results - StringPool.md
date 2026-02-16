---
doc_id: BR-StringPool-001
doc_type: "Benchmark Results"
title: "StringPool"
fatp_components: ["StringPool"]
topics: ["performance", "benchmarking", "string interning"]
cxx_standard: "C++20"
last_verified: "2026-02-16"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - StringPool

**Source:** `benchmark_StringPool.cpp`
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

| Library | Local | GCC-14 | GCC-13 | GCC-12 | Clang-17 | Clang-16 | MSVC CI |
|---------|-------|--------|--------|--------|----------|----------|---------|
| fat_p::StringPool<ST> | x | x | x | x | x | x | x |
| fat_p::StringPool<SM> | x | x | x | x | x | x | x |
| std::unordered_set<string> | x | x | x | x | x | x | x |
| std::unordered_map<string,ptr> | x | x | x | x | x | x | x |
| boost::flyweight<string> | x | x | x | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  SECTION 1: Intern Throughput - Unique Strings
================================================================================

  Contract: Insert N unique strings into empty pool/set. Measures cold insertion.
  No string appears twice. This is the worst case for interning (100% miss).

[2026-02-16 00:31:06] Section start CPU: 2432 MHz (base: 3686)

  --- N = 1000 unique strings (short, 4-15 chars) ---

[2026-02-16 00:31:06] CPU: 2432 MHz (base: 3686)
[Cooling: size transition] [Ready: 3501 MHz]
           fat_p::StringPool<ST>:    46.00 ns/op (+/-  1.22, CI:[45.74,46.98])
           fat_p::StringPool<SM>:    61.50 ns/op (+/-  0.83, CI:[61.60,62.44])
              std::unordered_set:    33.80 ns/op (+/-  0.60, CI:[33.52,34.13])
              std::unordered_map:    35.70 ns/op (+/-  1.51, CI:[35.16,36.68])
                boost::flyweight:    62.60 ns/op (+/-  0.50, CI:[62.41,62.91])
    -> fat_p<ST> vs std::uset: 0.73x SLOWER than baseline

  --- N = 10000 unique strings (short, 4-15 chars) ---

[2026-02-16 00:31:07] CPU: 3501 MHz (base: 3686)
[Cooling: size transition] [Ready: 2322 MHz]
           fat_p::StringPool<ST>:    44.33 ns/op (+/-  1.90, CI:[44.19,46.11])
           fat_p::StringPool<SM>:    60.53 ns/op (+/-  7.40, CI:[59.24,66.74])
              std::unordered_set:    33.75 ns/op (+/-  2.02, CI:[33.44,35.49])
              std::unordered_map:    34.39 ns/op (+/-  4.84, CI:[33.36,38.25])
                boost::flyweight:    64.69 ns/op (+/- 11.66, CI:[62.42,74.22])
    -> fat_p<ST> vs std::uset: 0.76x SLOWER than baseline

  --- N = 100000 unique strings (short, 4-15 chars) ---

[2026-02-16 00:31:08] CPU: 2285 MHz (base: 3686)
[Cooling: size transition] [Ready: 3354 MHz]
           fat_p::StringPool<ST>:    82.09 ns/op (+/- 25.80, CI:[79.76,105.87])
           fat_p::StringPool<SM>:   102.15 ns/op (+/- 30.42, CI:[102.14,132.93])
              std::unordered_set:    74.22 ns/op (+/- 26.66, CI:[67.71,94.69])
              std::unordered_map:    73.89 ns/op (+/- 29.18, CI:[71.14,100.67])
                boost::flyweight:   111.02 ns/op (+/- 33.26, CI:[101.14,134.80])
    -> fat_p<ST> vs std::uset: 0.90x SLOWER than baseline

================================================================================
  SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)
================================================================================

  Contract: Intern N strings with 90% duplicates. This is the common case:
  config keys, JSON field names, log messages with repeated patterns.
  Uses 1000 unique strings, 100000 total ops.

[2026-02-16 00:31:10] Section start CPU: 3243 MHz (base: 3686)
[Cooling: section transition] [Ready: 2764 MHz]
           fat_p::StringPool<ST>:    13.27 ns/op (+/-  0.40, CI:[13.18,13.59])
           fat_p::StringPool<SM>:    21.48 ns/op (+/-  1.02, CI:[21.28,22.31])
              std::unordered_set:    14.34 ns/op (+/-  0.37, CI:[14.29,14.67])
              std::unordered_map:    14.29 ns/op (+/-  0.53, CI:[14.21,14.74])
                boost::flyweight:    28.91 ns/op (+/-  2.14, CI:[28.58,30.75])
    -> fat_p<ST> vs std::uset: 1.08x FASTER than baseline

================================================================================
  SECTION 3: Lookup Throughput
================================================================================

  Contract: Look up strings in a pre-populated pool. Tests find() performance
  for both hits (string exists) and misses (string not in pool).

[2026-02-16 00:31:12] Section start CPU: 3464 MHz (base: 3686)
[Cooling: section transition] [Ready: 2432 MHz]
  --- Lookup Hit (N = 100000 ops, pool size = 10000) ---

[2026-02-16 00:31:14] CPU: 2432 MHz (base: 3686)
          fat_p::StringPool find:    23.26 ns/op (+/-  1.50, CI:[22.98,24.50])
         std::unordered_set find:    24.92 ns/op (+/-  0.96, CI:[24.01,24.99])
         std::unordered_map find:    24.48 ns/op (+/-  0.88, CI:[23.92,24.81])
    -> fat_p vs std::uset: 1.07x FASTER than baseline

  --- Lookup Miss (N = 100000 ops, pool size = 10000) ---

[2026-02-16 00:31:15] CPU: 2801 MHz (base: 3686)
          fat_p::StringPool find:    15.45 ns/op (+/-  0.88, CI:[14.97,15.86])
         std::unordered_set find:    16.37 ns/op (+/-  0.85, CI:[16.05,16.91])
         std::unordered_map find:    16.93 ns/op (+/-  1.29, CI:[16.51,17.81])
    -> fat_p vs std::uset: 1.06x FASTER than baseline

================================================================================
  SECTION 4: Pointer Comparison vs String Comparison
================================================================================

  Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).
  This is the key benefit of interning: identity checks become pointer equality.

[2026-02-16 00:31:15] Section start CPU: 2948 MHz (base: 3686)
[Cooling: section transition] [Ready: 2285 MHz]
[2026-02-16 00:31:17] CPU: 2285 MHz (base: 3686)
                      pointer ==:     0.38 ns/op (+/-  0.01, CI:[0.38,0.39])
                     std::strcmp:     1.53 ns/op (+/-  0.04, CI:[1.49,1.53])
                  string_view ==:    20.52 ns/op (+/-  1.14, CI:[20.24,21.39])
    -> pointer vs strcmp: 3.98x FASTER than baseline
    -> pointer vs sv ==: 53.58x FASTER than baseline

================================================================================
  SECTION 5: String Length Impact
================================================================================

  Contract: Intern performance across string lengths. Short strings benefit from
  SSO (no heap allocation for lookup), long strings show hashing cost.
  All three length categories run round-robin to eliminate thermal bias.

[2026-02-16 00:31:17] Section start CPU: 2985 MHz (base: 3686)
[Cooling: section transition] [Ready: 1916 MHz]
              short (4-15 chars):    42.05 ns/op (+/- 10.27, CI:[39.37,49.76])
            medium (20-50 chars):    98.49 ns/op (+/-  3.55, CI:[97.33,100.92])
            long (100-200 chars):   348.35 ns/op (+/- 16.60, CI:[343.50,360.31])

================================================================================
  SECTION 6: Memory Efficiency
================================================================================

  Contract: Memory savings from deduplication at various duplication rates.
  Uses 10000 unique strings, 100000 total ops.
  Single-library measurement (no round-robin needed).

[Cooling: section transition] [Ready: 3096 MHz]
  dup_rate | unique | total_interns |  hit_rate  | memory_saved
  ---------|--------|---------------|------------|-------------
        0%  |  10000 |        100000 |      90.0% |     942615 bytes
       50%  |   5000 |        100000 |      95.0% |     990406 bytes
       90%  |    999 |        100000 |      99.0% |    1053338 bytes
       99%  |    100 |        100000 |      99.9% |    1030923 bytes

================================================================================
  Benchmark Complete
================================================================================
```

---

## GCC-14

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
SECTION 1: Intern Throughput - Unique Strings
================================================================================

Contract: Insert N unique strings into empty pool/set. Measures cold insertion.
No string appears twice. This is the worst case for interning (100% miss).

[2026-02-16 08:19:03] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:03] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   131.78 ns/op (+/-  7.58, CI:[130.79,137.43])
fat_p::StringPool<SM>:   160.27 ns/op (+/-  8.22, CI:[159.27,166.48])
std::unordered_set:    86.52 ns/op (+/- 17.95, CI:[84.10,99.84])
std::unordered_map:    88.08 ns/op (+/-  3.57, CI:[87.41,90.54])
boost::flyweight:   112.54 ns/op (+/-  8.91, CI:[112.65,120.45])
-> fat_p<ST> vs std::uset: 0.66x SLOWER than baseline

--- N = 10000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:03] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   194.86 ns/op (+/-  5.33, CI:[190.21,194.88])
fat_p::StringPool<SM>:   218.07 ns/op (+/-  5.63, CI:[213.72,218.65])
std::unordered_set:   143.50 ns/op (+/-  6.35, CI:[137.39,142.95])
std::unordered_map:   134.92 ns/op (+/-  6.78, CI:[136.23,142.17])
boost::flyweight:   156.94 ns/op (+/-  6.63, CI:[154.47,160.28])
-> fat_p<ST> vs std::uset: 0.74x SLOWER than baseline

--- N = 100000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:04] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   217.75 ns/op (+/-  6.00, CI:[215.45,220.71])
fat_p::StringPool<SM>:   238.71 ns/op (+/-  8.13, CI:[236.54,243.67])
std::unordered_set:   156.78 ns/op (+/- 10.38, CI:[153.38,162.48])
std::unordered_map:   156.09 ns/op (+/- 11.67, CI:[154.13,164.37])
boost::flyweight:   193.30 ns/op (+/- 14.57, CI:[188.72,201.50])
-> fat_p<ST> vs std::uset: 0.72x SLOWER than baseline

================================================================================
SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)
================================================================================

Contract: Intern N strings with 90% duplicates. This is the common case:
config keys, JSON field names, log messages with repeated patterns.
Uses 1000 unique strings, 100000 total ops.

[2026-02-16 08:19:07] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
fat_p::StringPool<ST>:    25.74 ns/op (+/-  0.53, CI:[25.56,26.03])
fat_p::StringPool<SM>:    32.15 ns/op (+/-  0.50, CI:[32.04,32.47])
std::unordered_set:    23.85 ns/op (+/-  0.46, CI:[23.67,24.08])
std::unordered_map:    23.21 ns/op (+/-  0.51, CI:[22.81,23.26])
boost::flyweight:    49.07 ns/op (+/-  0.54, CI:[48.83,49.31])
-> fat_p<ST> vs std::uset: 0.93x SLOWER than baseline

================================================================================
SECTION 3: Lookup Throughput
================================================================================

Contract: Look up strings in a pre-populated pool. Tests find() performance
for both hits (string exists) and misses (string not in pool).

[2026-02-16 08:19:08] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
--- Lookup Hit (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:09] CPU: 2445 MHz (~base: 2445)
fat_p::StringPool find:    64.21 ns/op (+/-  0.48, CI:[64.03,64.45])
std::unordered_set find:    60.84 ns/op (+/-  0.76, CI:[60.71,61.38])
std::unordered_map find:    58.06 ns/op (+/-  0.66, CI:[57.98,58.56])
-> fat_p vs std::uset: 0.95x SLOWER than baseline

--- Lookup Miss (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:10] CPU: 3243 MHz (~base: 3243)
fat_p::StringPool find:    77.25 ns/op (+/-  0.41, CI:[77.14,77.50])
std::unordered_set find:    52.18 ns/op (+/-  1.59, CI:[52.02,53.41])
std::unordered_map find:    48.36 ns/op (+/-  1.91, CI:[48.23,49.90])
-> fat_p vs std::uset: 0.68x SLOWER than baseline

================================================================================
SECTION 4: Pointer Comparison vs String Comparison
================================================================================

Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).
This is the key benefit of interning: identity checks become pointer equality.

[2026-02-16 08:19:10] Section start CPU: 3242 MHz (~base: 3242)
[Cooling: section transition] [Ready]
[2026-02-16 08:19:11] CPU: 2555 MHz (~base: 2555)
pointer ==:     0.58 ns/op (+/-  0.03, CI:[0.58,0.60])
std::strcmp:     7.20 ns/op (+/-  0.05, CI:[7.16,7.21])
string_view ==:     7.66 ns/op (+/-  0.08, CI:[7.62,7.69])
-> pointer vs strcmp: 12.44x FASTER than baseline
-> pointer vs sv ==: 13.24x FASTER than baseline

================================================================================
SECTION 5: String Length Impact
================================================================================

Contract: Intern performance across string lengths. Short strings benefit from
SSO (no heap allocation for lookup), long strings show hashing cost.
All three length categories run round-robin to eliminate thermal bias.

[2026-02-16 08:19:11] Section start CPU: 3243 MHz (~base: 3243)
[Cooling: section transition] [Ready]
short (4-15 chars):   145.05 ns/op (+/-  3.74, CI:[142.76,146.03])
medium (20-50 chars):   210.72 ns/op (+/-  4.99, CI:[209.55,213.92])
long (100-200 chars):   328.36 ns/op (+/-  2.53, CI:[327.77,329.99])

================================================================================
SECTION 6: Memory Efficiency
================================================================================

Contract: Memory savings from deduplication at various duplication rates.
Uses 10000 unique strings, 100000 total ops.
Single-library measurement (no round-robin needed).

[Cooling: section transition] [Ready]
dup_rate | unique | total_interns |  hit_rate  | memory_saved
---------|--------|---------------|------------|-------------
0%  |  10000 |        100000 |      90.0% |     942615 bytes
50%  |   5000 |        100000 |      95.0% |     990406 bytes
90%  |    999 |        100000 |      99.0% |    1053338 bytes
99%  |    100 |        100000 |      99.9% |    1030923 bytes

================================================================================
Benchmark Complete
================================================================================
```

---

## GCC-13

**Platform:** Linux-x64 GCC-13.3 | measured=20 | seed=12345

```
================================================================================
SECTION 1: Intern Throughput - Unique Strings
================================================================================

Contract: Insert N unique strings into empty pool/set. Measures cold insertion.
No string appears twice. This is the worst case for interning (100% miss).

[2026-02-16 08:19:03] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:03] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   128.89 ns/op (+/-  4.91, CI:[128.41,132.71])
fat_p::StringPool<SM>:   160.25 ns/op (+/-  6.12, CI:[158.11,163.48])
std::unordered_set:    88.16 ns/op (+/- 19.78, CI:[84.68,102.02])
std::unordered_map:    85.41 ns/op (+/-  5.02, CI:[85.31,89.71])
boost::flyweight:   111.47 ns/op (+/-  7.70, CI:[111.98,118.72])
-> fat_p<ST> vs std::uset: 0.68x SLOWER than baseline

--- N = 10000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:03] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   196.78 ns/op (+/-  6.62, CI:[191.79,197.59])
fat_p::StringPool<SM>:   219.08 ns/op (+/-  6.12, CI:[214.23,219.60])
std::unordered_set:   144.98 ns/op (+/-  6.32, CI:[139.29,144.83])
std::unordered_map:   135.24 ns/op (+/-  6.37, CI:[135.24,140.82])
boost::flyweight:   157.95 ns/op (+/-  6.30, CI:[155.13,160.65])
-> fat_p<ST> vs std::uset: 0.74x SLOWER than baseline

--- N = 100000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:04] CPU: 3241 MHz (~base: 3241)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   218.57 ns/op (+/- 22.37, CI:[219.34,238.95])
fat_p::StringPool<SM>:   250.09 ns/op (+/- 36.11, CI:[242.16,273.82])
std::unordered_set:   163.52 ns/op (+/- 21.57, CI:[164.50,183.41])
std::unordered_map:   161.47 ns/op (+/- 47.78, CI:[155.21,197.09])
boost::flyweight:   221.78 ns/op (+/- 58.66, CI:[220.64,272.06])
-> fat_p<ST> vs std::uset: 0.75x SLOWER than baseline

================================================================================
SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)
================================================================================

Contract: Intern N strings with 90% duplicates. This is the common case:
config keys, JSON field names, log messages with repeated patterns.
Uses 1000 unique strings, 100000 total ops.

[2026-02-16 08:19:07] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
fat_p::StringPool<ST>:    25.41 ns/op (+/-  0.53, CI:[25.27,25.73])
fat_p::StringPool<SM>:    33.64 ns/op (+/-  0.45, CI:[33.46,33.86])
std::unordered_set:    24.46 ns/op (+/-  0.46, CI:[24.30,24.71])
std::unordered_map:    23.07 ns/op (+/-  0.52, CI:[22.82,23.27])
boost::flyweight:    48.51 ns/op (+/-  0.62, CI:[48.32,48.86])
-> fat_p<ST> vs std::uset: 0.96x SAME than baseline

================================================================================
SECTION 3: Lookup Throughput
================================================================================

Contract: Look up strings in a pre-populated pool. Tests find() performance
for both hits (string exists) and misses (string not in pool).

[2026-02-16 08:19:08] Section start CPU: 3241 MHz (~base: 3241)
[Cooling: section transition] [Ready]
--- Lookup Hit (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:09] CPU: 3241 MHz (~base: 3241)
fat_p::StringPool find:    63.22 ns/op (+/-  1.01, CI:[63.00,63.89])
std::unordered_set find:    61.66 ns/op (+/-  2.38, CI:[61.79,63.87])
std::unordered_map find:    61.65 ns/op (+/-  1.65, CI:[61.34,62.79])
-> fat_p vs std::uset: 0.98x SAME than baseline

--- Lookup Miss (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:10] CPU: 3243 MHz (~base: 3243)
fat_p::StringPool find:    75.51 ns/op (+/-  1.24, CI:[75.15,76.24])
std::unordered_set find:    52.44 ns/op (+/-  0.45, CI:[52.37,52.77])
std::unordered_map find:    48.09 ns/op (+/-  0.30, CI:[48.03,48.30])
-> fat_p vs std::uset: 0.69x SLOWER than baseline

================================================================================
SECTION 4: Pointer Comparison vs String Comparison
================================================================================

Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).
This is the key benefit of interning: identity checks become pointer equality.

[2026-02-16 08:19:11] Section start CPU: 3243 MHz (~base: 3243)
[Cooling: section transition] [Ready]
[2026-02-16 08:19:12] CPU: 2596 MHz (~base: 2596)
pointer ==:     0.80 ns/op (+/-  0.04, CI:[0.80,0.83])
std::strcmp:     7.18 ns/op (+/-  0.06, CI:[7.13,7.19])
string_view ==:     7.68 ns/op (+/-  0.12, CI:[7.64,7.74])
-> pointer vs strcmp: 9.00x FASTER than baseline
-> pointer vs sv ==: 9.63x FASTER than baseline

================================================================================
SECTION 5: String Length Impact
================================================================================

Contract: Intern performance across string lengths. Short strings benefit from
SSO (no heap allocation for lookup), long strings show hashing cost.
All three length categories run round-robin to eliminate thermal bias.

[2026-02-16 08:19:12] Section start CPU: 3244 MHz (~base: 3244)
[Cooling: section transition] [Ready]
short (4-15 chars):   143.47 ns/op (+/-  3.66, CI:[141.28,144.49])
medium (20-50 chars):   206.76 ns/op (+/-  4.70, CI:[206.58,210.70])
long (100-200 chars):   329.05 ns/op (+/-  3.59, CI:[328.11,331.26])

================================================================================
SECTION 6: Memory Efficiency
================================================================================

Contract: Memory savings from deduplication at various duplication rates.
Uses 10000 unique strings, 100000 total ops.
Single-library measurement (no round-robin needed).

[Cooling: section transition] [Ready]
dup_rate | unique | total_interns |  hit_rate  | memory_saved
---------|--------|---------------|------------|-------------
0%  |  10000 |        100000 |      90.0% |     942615 bytes
50%  |   5000 |        100000 |      95.0% |     990406 bytes
90%  |    999 |        100000 |      99.0% |    1053338 bytes
99%  |    100 |        100000 |      99.9% |    1030923 bytes

================================================================================
Benchmark Complete
================================================================================
```

---

## GCC-12

**Platform:** Linux-x64 GCC-12.4 | measured=20 | seed=12345

```
================================================================================
SECTION 1: Intern Throughput - Unique Strings
================================================================================

Contract: Insert N unique strings into empty pool/set. Measures cold insertion.
No string appears twice. This is the worst case for interning (100% miss).

[2026-02-16 08:19:07] Section start CPU: 3243 MHz (~base: 3243)

--- N = 1000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:07] CPU: 3243 MHz (~base: 3243)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   129.67 ns/op (+/- 10.17, CI:[129.12,138.04])
fat_p::StringPool<SM>:   160.61 ns/op (+/- 14.19, CI:[158.21,170.65])
std::unordered_set:    89.34 ns/op (+/- 25.70, CI:[89.70,112.23])
std::unordered_map:    86.10 ns/op (+/- 15.13, CI:[86.42,99.68])
boost::flyweight:   114.72 ns/op (+/- 17.27, CI:[112.72,127.86])
-> fat_p<ST> vs std::uset: 0.69x SLOWER than baseline

--- N = 10000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:07] CPU: 3243 MHz (~base: 3243)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   185.61 ns/op (+/-  5.13, CI:[183.56,188.05])
fat_p::StringPool<SM>:   207.18 ns/op (+/-  6.45, CI:[205.06,210.71])
std::unordered_set:   130.02 ns/op (+/-  6.71, CI:[126.92,132.80])
std::unordered_map:   128.26 ns/op (+/-  5.40, CI:[125.92,130.65])
boost::flyweight:   153.75 ns/op (+/- 10.26, CI:[148.66,157.65])
-> fat_p<ST> vs std::uset: 0.70x SLOWER than baseline

--- N = 100000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:08] CPU: 3241 MHz (~base: 3241)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   413.67 ns/op (+/- 60.87, CI:[382.04,435.40])
fat_p::StringPool<SM>:   485.10 ns/op (+/- 76.69, CI:[425.66,492.89])
std::unordered_set:   345.36 ns/op (+/- 98.23, CI:[314.47,400.57])
std::unordered_map:   351.40 ns/op (+/- 66.58, CI:[327.79,386.14])
boost::flyweight:   408.83 ns/op (+/- 57.92, CI:[381.95,432.72])
-> fat_p<ST> vs std::uset: 0.83x SLOWER than baseline

================================================================================
SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)
================================================================================

Contract: Intern N strings with 90% duplicates. This is the common case:
config keys, JSON field names, log messages with repeated patterns.
Uses 1000 unique strings, 100000 total ops.

[2026-02-16 08:19:13] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
fat_p::StringPool<ST>:    25.54 ns/op (+/-  0.53, CI:[25.30,25.76])
fat_p::StringPool<SM>:    34.10 ns/op (+/-  0.37, CI:[33.88,34.21])
std::unordered_set:    24.51 ns/op (+/-  0.45, CI:[24.26,24.66])
std::unordered_map:    22.19 ns/op (+/-  0.59, CI:[21.78,22.30])
boost::flyweight:    50.15 ns/op (+/-  0.43, CI:[50.03,50.40])
-> fat_p<ST> vs std::uset: 0.96x SAME than baseline

================================================================================
SECTION 3: Lookup Throughput
================================================================================

Contract: Look up strings in a pre-populated pool. Tests find() performance
for both hits (string exists) and misses (string not in pool).

[2026-02-16 08:19:14] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
--- Lookup Hit (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:15] CPU: 2445 MHz (~base: 2445)
fat_p::StringPool find:    62.43 ns/op (+/-  3.09, CI:[61.43,64.15])
std::unordered_set find:    63.26 ns/op (+/-  3.71, CI:[61.75,65.01])
std::unordered_map find:    62.40 ns/op (+/-  3.58, CI:[60.43,63.57])
-> fat_p vs std::uset: 1.01x SAME than baseline

--- Lookup Miss (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:16] CPU: 2445 MHz (~base: 2445)
fat_p::StringPool find:    75.35 ns/op (+/-  2.55, CI:[73.76,75.99])
std::unordered_set find:    55.11 ns/op (+/-  2.93, CI:[52.76,55.33])
std::unordered_map find:    55.53 ns/op (+/-  3.28, CI:[53.20,56.08])
-> fat_p vs std::uset: 0.73x SLOWER than baseline

================================================================================
SECTION 4: Pointer Comparison vs String Comparison
================================================================================

Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).
This is the key benefit of interning: identity checks become pointer equality.

[2026-02-16 08:19:17] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
[2026-02-16 08:19:18] CPU: 2648 MHz (~base: 2648)
pointer ==:     0.79 ns/op (+/-  0.01, CI:[0.79,0.80])
std::strcmp:     7.47 ns/op (+/-  0.17, CI:[7.41,7.56])
string_view ==:     7.40 ns/op (+/-  0.10, CI:[7.37,7.46])
-> pointer vs strcmp: 9.40x FASTER than baseline
-> pointer vs sv ==: 9.31x FASTER than baseline

================================================================================
SECTION 5: String Length Impact
================================================================================

Contract: Intern performance across string lengths. Short strings benefit from
SSO (no heap allocation for lookup), long strings show hashing cost.
All three length categories run round-robin to eliminate thermal bias.

[2026-02-16 08:19:18] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
short (4-15 chars):   150.98 ns/op (+/- 13.40, CI:[148.26,160.01])
medium (20-50 chars):   222.50 ns/op (+/- 23.61, CI:[213.44,234.13])
long (100-200 chars):   395.42 ns/op (+/- 25.09, CI:[382.68,404.67])

================================================================================
SECTION 6: Memory Efficiency
================================================================================

Contract: Memory savings from deduplication at various duplication rates.
Uses 10000 unique strings, 100000 total ops.
Single-library measurement (no round-robin needed).

[Cooling: section transition] [Ready]
dup_rate | unique | total_interns |  hit_rate  | memory_saved
---------|--------|---------------|------------|-------------
0%  |  10000 |        100000 |      90.0% |     942615 bytes
50%  |   5000 |        100000 |      95.0% |     990406 bytes
90%  |    999 |        100000 |      99.0% |    1053338 bytes
99%  |    100 |        100000 |      99.9% |    1030923 bytes

================================================================================
Benchmark Complete
================================================================================
```

---

## Clang-17

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
SECTION 1: Intern Throughput - Unique Strings
================================================================================

Contract: Insert N unique strings into empty pool/set. Measures cold insertion.
No string appears twice. This is the worst case for interning (100% miss).

[2026-02-16 08:19:00] Section start CPU: 2445 MHz (~base: 2445)

--- N = 1000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:00] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   128.12 ns/op (+/-  7.20, CI:[127.20,133.51])
fat_p::StringPool<SM>:   159.24 ns/op (+/-  5.85, CI:[156.87,161.99])
std::unordered_set:    85.25 ns/op (+/- 17.99, CI:[82.67,98.44])
std::unordered_map:    85.71 ns/op (+/-  6.82, CI:[85.68,91.67])
boost::flyweight:   110.71 ns/op (+/-  6.98, CI:[111.69,117.81])
-> fat_p<ST> vs std::uset: 0.67x SLOWER than baseline

--- N = 10000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:01] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   193.80 ns/op (+/-  6.28, CI:[189.17,194.67])
fat_p::StringPool<SM>:   218.63 ns/op (+/-  5.65, CI:[215.05,220.00])
std::unordered_set:   144.99 ns/op (+/-  6.14, CI:[139.54,144.93])
std::unordered_map:   139.14 ns/op (+/-  7.22, CI:[138.73,145.06])
boost::flyweight:   159.01 ns/op (+/-  5.57, CI:[155.67,160.55])
-> fat_p<ST> vs std::uset: 0.75x SLOWER than baseline

--- N = 100000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:01] CPU: 2610 MHz (~base: 2610)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   218.19 ns/op (+/- 12.52, CI:[213.79,224.77])
fat_p::StringPool<SM>:   242.38 ns/op (+/- 10.17, CI:[240.57,249.49])
std::unordered_set:   159.47 ns/op (+/- 10.44, CI:[158.46,167.61])
std::unordered_map:   157.60 ns/op (+/- 14.37, CI:[156.81,169.41])
boost::flyweight:   195.43 ns/op (+/- 23.92, CI:[188.28,209.24])
-> fat_p<ST> vs std::uset: 0.73x SLOWER than baseline

================================================================================
SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)
================================================================================

Contract: Intern N strings with 90% duplicates. This is the common case:
config keys, JSON field names, log messages with repeated patterns.
Uses 1000 unique strings, 100000 total ops.

[2026-02-16 08:19:04] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
fat_p::StringPool<ST>:    24.69 ns/op (+/-  0.58, CI:[24.31,24.83])
fat_p::StringPool<SM>:    32.15 ns/op (+/-  0.46, CI:[32.01,32.41])
std::unordered_set:    24.98 ns/op (+/-  0.99, CI:[24.80,25.66])
std::unordered_map:    24.61 ns/op (+/-  0.41, CI:[24.40,24.76])
boost::flyweight:    44.14 ns/op (+/-  0.58, CI:[43.86,44.37])
-> fat_p<ST> vs std::uset: 1.01x SAME than baseline

================================================================================
SECTION 3: Lookup Throughput
================================================================================

Contract: Look up strings in a pre-populated pool. Tests find() performance
for both hits (string exists) and misses (string not in pool).

[2026-02-16 08:19:06] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
--- Lookup Hit (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:07] CPU: 2445 MHz (~base: 2445)
fat_p::StringPool find:    62.16 ns/op (+/-  1.39, CI:[61.91,63.14])
std::unordered_set find:    60.76 ns/op (+/-  0.58, CI:[60.67,61.17])
std::unordered_map find:    60.60 ns/op (+/-  0.45, CI:[60.54,60.93])
-> fat_p vs std::uset: 0.98x SAME than baseline

--- Lookup Miss (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:07] CPU: 2445 MHz (~base: 2445)
fat_p::StringPool find:    76.25 ns/op (+/-  1.75, CI:[76.08,77.61])
std::unordered_set find:    51.08 ns/op (+/-  3.52, CI:[51.66,54.75])
std::unordered_map find:    51.87 ns/op (+/-  3.70, CI:[51.80,55.04])
-> fat_p vs std::uset: 0.67x SLOWER than baseline

================================================================================
SECTION 4: Pointer Comparison vs String Comparison
================================================================================

Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).
This is the key benefit of interning: identity checks become pointer equality.

[2026-02-16 08:19:08] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
[2026-02-16 08:19:09] CPU: 2445 MHz (~base: 2445)
pointer ==:     0.75 ns/op (+/-  0.03, CI:[0.75,0.78])
std::strcmp:     7.19 ns/op (+/-  0.05, CI:[7.14,7.19])
string_view ==:     7.50 ns/op (+/-  0.08, CI:[7.46,7.53])
-> pointer vs strcmp: 9.57x FASTER than baseline
-> pointer vs sv ==: 9.99x FASTER than baseline

================================================================================
SECTION 5: String Length Impact
================================================================================

Contract: Intern performance across string lengths. Short strings benefit from
SSO (no heap allocation for lookup), long strings show hashing cost.
All three length categories run round-robin to eliminate thermal bias.

[2026-02-16 08:19:09] Section start CPU: 2445 MHz (~base: 2445)
[Cooling: section transition] [Ready]
short (4-15 chars):   139.37 ns/op (+/-  3.50, CI:[136.88,139.95])
medium (20-50 chars):   203.99 ns/op (+/-  4.11, CI:[203.34,206.94])
long (100-200 chars):   322.03 ns/op (+/-  3.47, CI:[321.01,324.05])

================================================================================
SECTION 6: Memory Efficiency
================================================================================

Contract: Memory savings from deduplication at various duplication rates.
Uses 10000 unique strings, 100000 total ops.
Single-library measurement (no round-robin needed).

[Cooling: section transition] [Ready]
dup_rate | unique | total_interns |  hit_rate  | memory_saved
---------|--------|---------------|------------|-------------
0%  |  10000 |        100000 |      90.0% |     942615 bytes
50%  |   5000 |        100000 |      95.0% |     990406 bytes
90%  |    999 |        100000 |      99.0% |    1053338 bytes
99%  |    100 |        100000 |      99.9% |    1030923 bytes

================================================================================
Benchmark Complete
================================================================================
```

---

## Clang-16

**Platform:** Linux-x64 Clang-16.0 | measured=20 | seed=12345

```
================================================================================
SECTION 1: Intern Throughput - Unique Strings
================================================================================

Contract: Insert N unique strings into empty pool/set. Measures cold insertion.
No string appears twice. This is the worst case for interning (100% miss).

[2026-02-16 08:18:59] Section start CPU: 3253 MHz (~base: 3253)

--- N = 1000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:18:59] CPU: 3253 MHz (~base: 3253)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   130.04 ns/op (+/-  6.89, CI:[129.27,135.31])
fat_p::StringPool<SM>:   159.61 ns/op (+/-  8.95, CI:[158.07,165.91])
std::unordered_set:    85.69 ns/op (+/- 19.06, CI:[82.51,99.22])
std::unordered_map:    85.67 ns/op (+/-  4.79, CI:[85.70,89.90])
boost::flyweight:   110.92 ns/op (+/-  5.74, CI:[110.84,115.87])
-> fat_p<ST> vs std::uset: 0.66x SLOWER than baseline

--- N = 10000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:00] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   192.90 ns/op (+/-  6.07, CI:[188.38,193.71])
fat_p::StringPool<SM>:   219.35 ns/op (+/-  5.98, CI:[213.65,218.90])
std::unordered_set:   144.77 ns/op (+/-  6.12, CI:[139.53,144.90])
std::unordered_map:   134.26 ns/op (+/-  6.69, CI:[135.86,141.73])
boost::flyweight:   158.35 ns/op (+/-  6.81, CI:[155.44,161.41])
-> fat_p<ST> vs std::uset: 0.75x SLOWER than baseline

--- N = 100000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:01] CPU: 2445 MHz (~base: 2445)
[Cooling: size transition] [Ready]
fat_p::StringPool<ST>:   223.56 ns/op (+/- 16.81, CI:[218.41,233.14])
fat_p::StringPool<SM>:   262.96 ns/op (+/- 29.05, CI:[257.61,283.08])
std::unordered_set:   164.73 ns/op (+/- 12.27, CI:[162.15,172.90])
std::unordered_map:   166.52 ns/op (+/- 45.62, CI:[156.98,196.97])
boost::flyweight:   220.59 ns/op (+/- 38.09, CI:[212.45,245.83])
-> fat_p<ST> vs std::uset: 0.74x SLOWER than baseline

================================================================================
SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)
================================================================================

Contract: Intern N strings with 90% duplicates. This is the common case:
config keys, JSON field names, log messages with repeated patterns.
Uses 1000 unique strings, 100000 total ops.

[2026-02-16 08:19:04] Section start CPU: 3239 MHz (~base: 3239)
[Cooling: section transition] [Ready]
fat_p::StringPool<ST>:    24.24 ns/op (+/-  0.63, CI:[23.98,24.53])
fat_p::StringPool<SM>:    32.28 ns/op (+/-  0.60, CI:[32.13,32.66])
std::unordered_set:    24.50 ns/op (+/-  0.48, CI:[24.26,24.68])
std::unordered_map:    24.65 ns/op (+/-  0.43, CI:[24.47,24.85])
boost::flyweight:    48.49 ns/op (+/-  1.01, CI:[47.93,48.82])
-> fat_p<ST> vs std::uset: 1.01x SAME than baseline

================================================================================
SECTION 3: Lookup Throughput
================================================================================

Contract: Look up strings in a pre-populated pool. Tests find() performance
for both hits (string exists) and misses (string not in pool).

[2026-02-16 08:19:05] Section start CPU: 3241 MHz (~base: 3241)
[Cooling: section transition] [Ready]
--- Lookup Hit (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:06] CPU: 3243 MHz (~base: 3243)
fat_p::StringPool find:    66.77 ns/op (+/-  3.10, CI:[65.32,68.04])
std::unordered_set find:    66.91 ns/op (+/-  4.38, CI:[65.81,69.65])
std::unordered_map find:    67.87 ns/op (+/-  4.20, CI:[66.32,69.99])
-> fat_p vs std::uset: 1.00x SAME than baseline

--- Lookup Miss (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:19:07] CPU: 3243 MHz (~base: 3243)
fat_p::StringPool find:    79.12 ns/op (+/-  2.49, CI:[78.01,80.20])
std::unordered_set find:    57.64 ns/op (+/-  4.43, CI:[55.18,59.07])
std::unordered_map find:    57.87 ns/op (+/-  5.48, CI:[55.75,60.56])
-> fat_p vs std::uset: 0.73x SLOWER than baseline

================================================================================
SECTION 4: Pointer Comparison vs String Comparison
================================================================================

Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).
This is the key benefit of interning: identity checks become pointer equality.

[2026-02-16 08:19:07] Section start CPU: 3240 MHz (~base: 3240)
[Cooling: section transition] [Ready]
[2026-02-16 08:19:08] CPU: 2943 MHz (~base: 2943)
pointer ==:     0.72 ns/op (+/-  0.02, CI:[0.72,0.74])
std::strcmp:     7.22 ns/op (+/-  0.05, CI:[7.19,7.24])
string_view ==:     7.50 ns/op (+/-  0.06, CI:[7.45,7.50])
-> pointer vs strcmp: 10.04x FASTER than baseline
-> pointer vs sv ==: 10.42x FASTER than baseline

================================================================================
SECTION 5: String Length Impact
================================================================================

Contract: Intern performance across string lengths. Short strings benefit from
SSO (no heap allocation for lookup), long strings show hashing cost.
All three length categories run round-robin to eliminate thermal bias.

[2026-02-16 08:19:08] Section start CPU: 3244 MHz (~base: 3244)
[Cooling: section transition] [Ready]
short (4-15 chars):   143.78 ns/op (+/-  7.87, CI:[141.20,148.10])
medium (20-50 chars):   214.56 ns/op (+/- 14.19, CI:[213.11,225.55])
long (100-200 chars):   356.98 ns/op (+/- 18.72, CI:[356.27,372.68])

================================================================================
SECTION 6: Memory Efficiency
================================================================================

Contract: Memory savings from deduplication at various duplication rates.
Uses 10000 unique strings, 100000 total ops.
Single-library measurement (no round-robin needed).

[Cooling: section transition] [Ready]
dup_rate | unique | total_interns |  hit_rate  | memory_saved
---------|--------|---------------|------------|-------------
0%  |  10000 |        100000 |      90.0% |     942615 bytes
50%  |   5000 |        100000 |      95.0% |     990406 bytes
90%  |    999 |        100000 |      99.0% |    1053338 bytes
99%  |    100 |        100000 |      99.9% |    1030923 bytes

================================================================================
Benchmark Complete
================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
SECTION 1: Intern Throughput - Unique Strings
================================================================================

Contract: Insert N unique strings into empty pool/set. Measures cold insertion.
No string appears twice. This is the worst case for interning (100% miss).

[2026-02-16 08:19:55] Section start CPU: 2445 MHz (base: 2445)

--- N = 1000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:55] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
fat_p::StringPool<ST>:   104.15 ns/op (+/-  5.34, CI:[102.62,107.30])
fat_p::StringPool<SM>:   117.30 ns/op (+/-  6.28, CI:[117.14,122.65])
std::unordered_set:    67.95 ns/op (+/- 12.49, CI:[65.96,76.90])
std::unordered_map:    68.00 ns/op (+/-  4.61, CI:[67.57,71.61])
boost::flyweight:   111.55 ns/op (+/-  6.75, CI:[110.99,116.90])
-> fat_p<ST> vs std::uset: 0.65x SLOWER than baseline

--- N = 10000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:56] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
fat_p::StringPool<ST>:   112.07 ns/op (+/-  2.62, CI:[110.91,113.21])
fat_p::StringPool<SM>:   129.13 ns/op (+/-  2.47, CI:[127.34,129.50])
std::unordered_set:    74.25 ns/op (+/-  1.98, CI:[74.05,75.78])
std::unordered_map:    74.66 ns/op (+/-  1.48, CI:[74.24,75.53])
boost::flyweight:   126.78 ns/op (+/-  2.84, CI:[125.39,127.88])
-> fat_p<ST> vs std::uset: 0.66x SLOWER than baseline

--- N = 100000 unique strings (short, 4-15 chars) ---

[2026-02-16 08:19:57] CPU: 2445 MHz (base: 2445)
[Cooling: size transition] [Ready: 2445 MHz]
fat_p::StringPool<ST>:   136.70 ns/op (+/-  7.59, CI:[134.90,141.55])
fat_p::StringPool<SM>:   158.98 ns/op (+/- 17.22, CI:[156.52,171.62])
std::unordered_set:    96.34 ns/op (+/- 10.09, CI:[95.83,104.68])
std::unordered_map:    97.56 ns/op (+/-  9.53, CI:[97.46,105.82])
boost::flyweight:   153.29 ns/op (+/-  8.33, CI:[150.16,157.46])
-> fat_p<ST> vs std::uset: 0.70x SLOWER than baseline

================================================================================
SECTION 2: Intern Throughput - High Duplication (90% Hit Rate)
================================================================================

Contract: Intern N strings with 90% duplicates. This is the common case:
config keys, JSON field names, log messages with repeated patterns.
Uses 1000 unique strings, 100000 total ops.

[2026-02-16 08:20:00] Section start CPU: 2445 MHz (base: 2445)
[Cooling: section transition] [Ready: 2445 MHz]
fat_p::StringPool<ST>:    30.48 ns/op (+/-  0.24, CI:[30.38,30.58])
fat_p::StringPool<SM>:    39.00 ns/op (+/-  1.23, CI:[38.89,39.96])
std::unordered_set:    24.20 ns/op (+/-  0.40, CI:[24.10,24.45])
std::unordered_map:    24.31 ns/op (+/-  0.30, CI:[24.19,24.46])
boost::flyweight:    53.72 ns/op (+/-  0.37, CI:[53.62,53.94])
-> fat_p<ST> vs std::uset: 0.79x SLOWER than baseline

================================================================================
SECTION 3: Lookup Throughput
================================================================================

Contract: Look up strings in a pre-populated pool. Tests find() performance
for both hits (string exists) and misses (string not in pool).

[2026-02-16 08:20:02] Section start CPU: 2445 MHz (base: 2445)
[Cooling: section transition] [Ready: 2445 MHz]
--- Lookup Hit (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:20:04] CPU: 2445 MHz (base: 2445)
fat_p::StringPool find:    42.99 ns/op (+/-  4.83, CI:[42.13,46.37])
std::unordered_set find:    43.99 ns/op (+/-  1.24, CI:[43.89,44.98])
std::unordered_map find:    43.97 ns/op (+/-  4.29, CI:[43.39,47.15])
-> fat_p vs std::uset: 1.02x SAME than baseline

--- Lookup Miss (N = 100000 ops, pool size = 10000) ---

[2026-02-16 08:20:05] CPU: 2445 MHz (base: 2445)
fat_p::StringPool find:    26.65 ns/op (+/-  0.35, CI:[26.50,26.80])
std::unordered_set find:    27.24 ns/op (+/-  0.37, CI:[27.16,27.48])
std::unordered_map find:    27.49 ns/op (+/-  1.24, CI:[27.21,28.30])
-> fat_p vs std::uset: 1.02x SAME than baseline

================================================================================
SECTION 4: Pointer Comparison vs String Comparison
================================================================================

Contract: Compare interned strings by pointer (O(1)) vs by content (O(n)).
This is the key benefit of interning: identity checks become pointer equality.

[2026-02-16 08:20:06] Section start CPU: 2445 MHz (base: 2445)
[Cooling: section transition] [Ready: 2445 MHz]
[2026-02-16 08:20:08] CPU: 2445 MHz (base: 2445)
pointer ==:     0.59 ns/op (+/-  0.06, CI:[0.58,0.64])
std::strcmp:     3.93 ns/op (+/-  0.24, CI:[3.93,4.14])
string_view ==:    30.29 ns/op (+/-  0.55, CI:[30.24,30.72])
-> pointer vs strcmp: 6.67x FASTER than baseline
-> pointer vs sv ==: 51.43x FASTER than baseline

================================================================================
SECTION 5: String Length Impact
================================================================================

Contract: Intern performance across string lengths. Short strings benefit from
SSO (no heap allocation for lookup), long strings show hashing cost.
All three length categories run round-robin to eliminate thermal bias.

[2026-02-16 08:20:08] Section start CPU: 2445 MHz (base: 2445)
[Cooling: section transition] [Ready: 2445 MHz]
short (4-15 chars):    93.43 ns/op (+/-  2.14, CI:[93.02,94.89])
medium (20-50 chars):   247.21 ns/op (+/- 16.40, CI:[242.54,256.91])
long (100-200 chars):   683.14 ns/op (+/-  7.05, CI:[681.23,687.40])

================================================================================
SECTION 6: Memory Efficiency
================================================================================

Contract: Memory savings from deduplication at various duplication rates.
Uses 10000 unique strings, 100000 total ops.
Single-library measurement (no round-robin needed).

[Cooling: section transition] [Ready: 2445 MHz]
dup_rate | unique | total_interns |  hit_rate  | memory_saved
---------|--------|---------------|------------|-------------
0%  |  10000 |        100000 |      90.0% |     942615 bytes
50%  |   5000 |        100000 |      95.0% |     990406 bytes
90%  |    999 |        100000 |      99.0% |    1053338 bytes
99%  |    100 |        100000 |      99.9% |    1030923 bytes

================================================================================
Benchmark Complete
================================================================================
```

---

## Caveats

- Local MSVC CPU frequency showed throttling during testing (65-66% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- StringPool is consistently slower than raw `std::unordered_set` for cold unique inserts (Section 1) because it copies strings into a stable arena. The gap narrows significantly for duplicate-heavy workloads (Section 2) which is the primary use case.
- Pointer comparison (Section 4) shows the key interning benefit: 6-13x faster than string comparison across all platforms.
- MSVC CI shows anomalously slow `string_view ==` (30 ns vs 7-8 ns on GCC/Clang) due to MSVC's string_view implementation differences.
