---
doc_id: BR-ServiceLocator-001
doc_type: "Benchmark Results"
title: "ServiceLocator"
fatp_components: ["ServiceLocator"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - ServiceLocator

**Source:** `benchmark_ServiceLocator.cpp`
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
| fat_p::service_locator::DefaultServiceLocator | x | x | x | x |
| fat_p::service_locator::ThreadSafeServiceLocator | x | x | x | x |
| std::unordered_map<type_index> | x | x | x | x |
| Direct pointer | x | x | x | x |
| entt::locator | x | x | x | x |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
[OK] All adapters verified

================================================================================
  SINGLE-TYPE RESOLUTION
================================================================================

Contract: Resolve one service type. O(1) hash lookup for map-based, O(1) static access for EnTT.

[2026-02-15 19:55:18] Starting CPU: 2838 MHz (base: 3686)
  fat_p::service_locator::DefaultServiceLocator: median=    2.67 mean=    2.94 +/-  1.04 [    2.42,     3.47]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   11.12 mean=   11.39 +/-  0.90 [   10.94,    11.84]
  entt::locator (static global)      : median=    1.23 mean=    1.25 +/-  0.06 [    1.22,     1.28]
  std::unordered_map<type_index>     : median=   12.07 mean=   12.49 +/-  0.95 [   12.01,    12.97]
  Direct pointers (baseline)         : median=    1.19 mean=    1.29 +/-  0.30 [    1.14,     1.44]

================================================================================
  MULTI-TYPE RESOLUTION (5 types)
================================================================================

Contract: Resolve 5 different service types per iteration. Measures cumulative lookup cost.

[2026-02-15 19:55:18] Starting CPU: 2801 MHz (base: 3686)
  fat_p::service_locator::DefaultServiceLocator: median=    2.68 mean=    2.77 +/-  0.29 [    2.62,     2.92]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   11.22 mean=   11.81 +/-  1.47 [   11.06,    12.55]
  entt::locator (static global)      : median=    1.14 mean=    1.13 +/-  0.02 [    1.12,     1.14]
  std::unordered_map<type_index>     : median=    8.98 mean=    9.42 +/-  1.17 [    8.83,    10.01]
  Direct pointers (baseline)         : median=    1.14 mean=    1.16 +/-  0.04 [    1.14,     1.18]

================================================================================
  NAMED SERVICES (fat_p exclusive)
================================================================================

Contract: Resolve services by type+name composite key. Competitors do not support named services.

[2026-02-15 19:55:18] Starting CPU: 2469 MHz (base: 3686)
  resolve<ILogger>() (default)       : median=    2.82 mean=    2.99 +/-  0.81 [    2.58,     3.40]
  resolve<ILogger>("file")           : median=    6.72 mean=    7.06 +/-  1.03 [    6.54,     7.58]
  resolve 3 named variants           : median=    5.73 mean=    5.73 +/-  0.09 [    5.68,     5.77]

================================================================================
  SCOPED RESOLUTION (fat_p exclusive)
================================================================================

Contract: Child scope overrides parent. Measures lookup with parent chain traversal.

[2026-02-15 19:55:18] Starting CPU: 2801 MHz (base: 3686)
  resolve (child override)           : median=    1.30 mean=    1.29 +/-  0.04 [    1.27,     1.32]
  resolve (parent inheritance)       : median=    3.23 mean=    3.58 +/-  1.02 [    3.06,     4.10]

================================================================================
  REGISTRATION PERFORMANCE
================================================================================

Contract: Register 5 services including hash map insertion. Measures setup cost.

[2026-02-15 19:55:18] Starting CPU: 2801 MHz (base: 3686)
  fat_p::service_locator::DefaultServiceLocator: median=   51.01 mean=   53.32 +/-  3.62 [   51.49,    55.15]
  std::unordered_map<type_index>     : median=   34.75 mean=   36.23 +/-  2.71 [   34.86,    37.61]

================================================================================
  SIZE SENSITIVITY
================================================================================

Contract: Measure resolve performance as number of registered services scales. Split into unnamed (type-only) and named (type+name) variants.

[2026-02-15 19:55:18] Starting CPU: 2985 MHz (base: 3686)
UNNAMED (TYPE-ONLY)
Services | fat_p median (ns/op) | unordered_map<void*> median (ns/op) | Ratio (unordered_map / fat_p)
---------|----------------------|-----------------------------------|---------------------------
       1 |                 3.07 |                              3.07 |  1.00x
       5 |                 2.98 |                              3.06 |  1.03x
      10 |                 2.97 |                              3.08 |  1.04x
      25 |                 3.05 |                              3.10 |  1.02x
      50 |                 3.29 |                              3.10 |  0.94x
     100 |                 3.57 |                              3.10 |  0.87x

NAMED (TYPE+NAME)
Note: The string-only unordered_map is a name-only key, not a composite (type+name) key.
      The composite-key unordered_map is the apples-to-apples comparator.
Services | fat_p median (ns/op) | unordered_map<string> median (ns/op) | unordered_map<composite> median (ns/op) | Ratio (composite / fat_p)
---------|----------------------|----------------------------------|----------------------------------|---------------------------
       1 |                20.48 |                             4.37 |                             5.63 |  0.27x
       5 |                20.86 |                             4.34 |                             5.58 |  0.27x
      10 |                20.96 |                             4.35 |                             5.55 |  0.26x
      25 |                21.67 |                             6.70 |                             7.32 |  0.34x
      50 |                22.17 |                             7.39 |                             7.73 |  0.35x
     100 |                23.08 |                             8.31 |                             8.39 |  0.36x

================================================================================
  STRING KEY HOT LOOP (named services)
================================================================================

Contract: Hot-loop resolves by type+name (string key) after startup registration.
Measures steady-state cost while varying:
  A) Name length (bytes) at fixed named-variant count.
  B) Named-variant count at fixed name length.

[2026-02-15 19:55:18] Starting CPU: 3059 MHz (base: 3686)
NAME LENGTH SWEEP (variants=100)
Len | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  4 |               7.01 |                  6.67
  8 |               7.95 |                  7.94
 16 |              11.59 |                 12.10
 32 |              21.54 |                 21.46
 64 |              39.38 |                 39.74
128 |              90.69 |                 92.27

VARIANT COUNT SWEEP (len=16)
N   | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  1 |              11.84 |                 12.08
  5 |              12.05 |                 11.91
 10 |              11.96 |                 11.90
 25 |              11.89 |                 12.02
 50 |              11.51 |                 11.62
100 |              11.56 |                 11.76
500 |              11.67 |                 11.87

================================================================================
  CONST RESOLVE (T vs const T)
================================================================================

Contract: Measures steady-state cost of tryResolve<T>() vs tryResolve<const T>() for unnamed services.
TypeKeyPolicy removes cv-qualifiers, so both resolve paths share the same type id.

[2026-02-15 19:55:21] Starting CPU: 3169 MHz (base: 3686)
tryResolve<T> median (ns/op) | tryResolve<const T> median (ns/op) | Ratio
----------------------------|----------------------------------|------
                      1.25 |                             1.25 | 1.00x

================================================================================
  MUTATION COST (unregister / clear)
================================================================================

Contract: Measures registry mutation cost after services have been registered.
  A) unregister N distinct service types (ns/op).
  B) clear N service types (ns/op per entry).

[2026-02-15 19:55:21] Starting CPU: 3169 MHz (base: 3686)
UNREGISTER (ns/op)
N   | median (ns/op)
----|---------------
  1 |          0.00
  5 |         20.00
 10 |         30.00
 25 |         20.00
 50 |         18.00
100 |         16.00

CLEAR (ns/op per entry)
N   | median (ns/op)
----|---------------
  1 |        100.00
  5 |         20.00
 10 |         20.00
 25 |         12.00
 50 |         12.00
100 |         11.00

================================================================================
  OVERHEAD ISOLATION MICRO-BENCHMARKS
================================================================================

Contract: Isolate individual overhead components to identify optimization targets.

[2026-02-15 19:55:21] Starting CPU: 3169 MHz (base: 3686)
Measuring individual overhead components...

  1. Direct pointer access (baseline)          : 1.20 ns/op
  2. std::type_index construction              : 10.80 ns/op
  3. fat_p TypeKeyPolicy (static addr)         : 1.29 ns/op
  4. std::string construction (empty)          : 1.49 ns/op
  5. std::string from string_view (empty)      : 2.45 ns/op
  6. ServiceKey-like struct construction       : 7.38 ns/op
  7. ServiceKeyHash computation                : 2.31 ns/op
  8. unordered_map<type_index>.find()          : 7.11 ns/op
  9. unordered_map<void*>.find() (optimal)     : 2.26 ns/op
  10. ServiceKey map find (with makeKey)       : 9.79 ns/op
  11. std::optional construction + check       : 1.43 ns/op
  12. fat_p::service_locator::DefaultServiceLocator.tryResolve: 2.63 ns/op

Analysis:
  - Items 6+10 show ServiceKey construction + lookup overhead
  - Compare item 9 (optimal) vs item 10 (current) for improvement potential
  - Item 4-5 show std::string allocation overhead (even for empty strings)

Detailed gap analysis (tryResolve breakdown)...

  13. makeKey<T>() simulation                  : 1.24 ns/op
  14. SingleThreadedPolicy lock_shared()       : 1.16 ns/op
  15. SharedMutexPolicy lock_shared()          : 11.01 ns/op
  16. std::function copy                       : 1.52 ns/op
  17. std::shared_ptr<void> copy               : 9.83 ns/op
  18. std::optional<SnapLike> construction     : 1.14 ns/op
  19. FullSnapshot copy (Instance path)        : 1.26 ns/op
  20. FullSnapshot copy (Factory path)         : 2.32 ns/op
  21. Expected<ref_wrapper> construction       : 1.35 ns/op
  22. resolveEntryForRead simulation           : 7.22 ns/op
  23. tryResolve simulation (no snapshot)      : 2.58 ns/op
  24. fat_p tryResolve (actual)                : 2.66 ns/op

Gap Analysis Summary:
  Compare items 22-24 to identify where overhead accumulates.
  Item 23 vs 24 shows cost of snapshot copy + Expected wrapper.
  Item 16 (std::function copy) is often the hidden culprit.

StableHashMap comparison (reference stability)...

  25. StableHashMap<void*> find (no copy, SM64): 1.20 ns/op
  26. StableHashMap<ServiceKey> find           : 5.59 ns/op
  27. Optimal tryResolve (StableHashMap)       : 1.21 ns/op
  28. Current fat_p tryResolve                 : 2.44 ns/op

StableHashMap Advantage:
  - Reference stability eliminates snapshot copy (~10ns saved)
  - SIMD-accelerated probing (faster than std::unordered_map)
  - No shared_ptr atomic refcount overhead on resolve
  - Compare #27 vs #28 for potential improvement

Zero-cost abstraction verification...

  29. Raw StableHashMap (no lock)              : 1.31 ns/op
  30. With SingleThreadedPolicy lock           : 1.19 ns/op
  31. ServiceLocator tryResolve (optimized)    : 3.23 ns/op
  32. Minimal resolve (just hash lookup)       : 1.22 ns/op
  33. Static global (EnTT-style)               : 1.21 ns/op

If #29 == #30, SingleThreadedPolicy is truly zero-cost.
Gap between #32 and #33 is the irreducible hash lookup cost.

================================================================================
  ALTERNATIVE KEY STRATEGIES
================================================================================

Contract: Compare different key designs to identify optimization opportunities.

[2026-02-15 19:55:21] Starting CPU: 2653 MHz (base: 3686)
Testing alternative ServiceKey designs...

  Strategy 1: void* + std::string (current)         : 3.42 ns/op
  Strategy 2: void* + string_view (zero-alloc)      : 2.41 ns/op
  Strategy 3: void* only (no names)                 : 1.65 ns/op
  Strategy 4: Cached hash (still allocates string)  : 2.67 ns/op
  Strategy 5: Two-level map (unnamed fast path)     : 1.25 ns/op
  Strategy 6: std::type_index (no names)            : 7.01 ns/op

Recommendations:
  - Strategy 3/5 show potential for unnamed services (~2x faster)
  - Strategy 2 eliminates allocation but requires API changes
  - Consider two-tier storage: fast path for type-only, slow path for named

================================================================================
  CONCURRENT RESOLUTION
================================================================================

Contract: Multi-threaded read-only resolution. Thread-safe variants only.

[2026-02-15 19:55:21] Starting CPU: 2653 MHz (base: 3686)
Thread count: 24, ops/thread: 100000

  fat_p::service_locator::ThreadSafeServiceLocator: median=  144.07 mean=  144.64 +/-  6.01 [  141.59,   147.68]
  unordered_map<void*> + shared_mutex (type key): median=  103.96 mean=  105.33 +/-  7.03 [  101.77,   108.88]
  StableHashMap<void*> + shared_mutex (type key, SM64): median=  113.47 mean=  113.58 +/- 10.01 [  108.52,   118.65]
  unordered_map<type_index> + shared_mutex (precomputed key): median=  137.84 mean=  140.09 +/- 12.01 [  134.01,   146.16]

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
[OK] All adapters verified
================================================================================
  SINGLE-TYPE RESOLUTION
================================================================================

Contract: Resolve one service type. O(1) hash lookup for map-based, O(1) static access for EnTT.

[2026-02-16 03:38:00] Starting CPU: 3244 MHz (~base: 3244)
  fat_p::service_locator::DefaultServiceLocator: median=    6.50 mean=    6.53 +/-  0.20 [    6.45,     6.62]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   12.46 mean=   12.50 +/-  0.25 [   12.39,    12.61]
  entt::locator (static global)      : median=    0.31 mean=    0.31 +/-  0.00 [    0.31,     0.31]
  std::unordered_map<type_index>     : median=   20.52 mean=   20.63 +/-  0.24 [   20.52,    20.73]
  Direct pointers (baseline)         : median=    0.31 mean=    0.31 +/-  0.00 [    0.31,     0.31]
================================================================================
  MULTI-TYPE RESOLUTION (5 types)
================================================================================

Contract: Resolve 5 different service types per iteration. Measures cumulative lookup cost.

[2026-02-16 03:38:00] Starting CPU: 2445 MHz (~base: 2445)
  fat_p::service_locator::DefaultServiceLocator: median=    5.15 mean=    5.22 +/-  0.25 [    5.11,     5.33]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   10.81 mean=   10.79 +/-  0.34 [   10.64,    10.94]
  entt::locator (static global)      : median=    0.31 mean=    0.31 +/-  0.00 [    0.31,     0.31]
  std::unordered_map<type_index>     : median=   11.21 mean=   11.27 +/-  0.12 [   11.22,    11.32]
  Direct pointers (baseline)         : median=    0.31 mean=    0.31 +/-  0.00 [    0.31,     0.31]
================================================================================
  NAMED SERVICES (fat_p exclusive)
================================================================================

Contract: Resolve services by type+name composite key. Competitors do not support named services.

[2026-02-16 03:38:00] Starting CPU: 2445 MHz (~base: 2445)
  resolve<ILogger>() (default)       : median=    2.03 mean=    2.06 +/-  0.07 [    2.03,     2.09]
  resolve<ILogger>("file")           : median=   10.30 mean=   10.31 +/-  0.09 [   10.27,    10.35]
  resolve 3 named variants           : median=    8.16 mean=    8.16 +/-  0.09 [    8.12,     8.20]
================================================================================
  SCOPED RESOLUTION (fat_p exclusive)
================================================================================

Contract: Child scope overrides parent. Measures lookup with parent chain traversal.

[2026-02-16 03:38:01] Starting CPU: 2445 MHz (~base: 2445)
  resolve (child override)           : median=    2.06 mean=    2.08 +/-  0.06 [    2.05,     2.10]
  resolve (parent inheritance)       : median=    3.66 mean=    3.69 +/-  0.21 [    3.60,     3.78]
================================================================================
  REGISTRATION PERFORMANCE
================================================================================

Contract: Register 5 services including hash map insertion. Measures setup cost.

[2026-02-16 03:38:01] Starting CPU: 2445 MHz (~base: 2445)
  fat_p::service_locator::DefaultServiceLocator: median=   41.97 mean=   42.11 +/-  0.32 [   41.97,    42.25]
  std::unordered_map<type_index>     : median=   41.35 mean=   41.47 +/-  0.49 [   41.25,    41.68]
================================================================================
  SIZE SENSITIVITY
================================================================================

Contract: Measure resolve performance as number of registered services scales. Split into unnamed (type-only) and named (type+name) variants.

[2026-02-16 03:38:01] Starting CPU: 3244 MHz (~base: 3244)
UNNAMED (TYPE-ONLY)
Services | fat_p median (ns/op) | unordered_map<void*> median (ns/op) | Ratio (unordered_map / fat_p)
---------|----------------------|-----------------------------------|---------------------------
       1 |                 6.72 |                              4.62 |  0.69x
       5 |                17.78 |                              4.32 |  0.24x
      10 |                17.63 |                              4.32 |  0.24x
      25 |                17.79 |                              4.32 |  0.24x
      50 |                17.72 |                              4.32 |  0.24x
     100 |                20.78 |                              4.32 |  0.21x

NAMED (TYPE+NAME)
Note: The string-only unordered_map is a name-only key, not a composite (type+name) key.
      The composite-key unordered_map is the apples-to-apples comparator.
Services | fat_p median (ns/op) | unordered_map<string> median (ns/op) | unordered_map<composite> median (ns/op) | Ratio (composite / fat_p)
---------|----------------------|----------------------------------|----------------------------------|---------------------------
       1 |                16.45 |                             4.32 |                            12.00 |  0.73x
       5 |                16.79 |                            12.56 |                            14.71 |  0.88x
      10 |                16.47 |                            22.72 |                            17.69 |  1.07x
      25 |                17.08 |                            13.25 |                            17.26 |  1.01x
      50 |                18.53 |                            13.57 |                            17.72 |  0.96x
     100 |                19.71 |                            13.86 |                            19.32 |  0.98x
================================================================================
  STRING KEY HOT LOOP (named services)
================================================================================

Contract: Hot-loop resolves by type+name (string key) after startup registration.
Measures steady-state cost while varying:
  A) Name length (bytes) at fixed named-variant count.
  B) Named-variant count at fixed name length.
[2026-02-16 03:38:01] Starting CPU: 2445 MHz (~base: 2445)
NAME LENGTH SWEEP (variants=100)
Len | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  4 |              14.25 |                 14.46
  8 |              13.44 |                 13.46
 16 |              14.32 |                 14.38
 32 |              16.92 |                 16.89
 64 |              21.69 |                 21.53
128 |              33.91 |                 34.29

VARIANT COUNT SWEEP (len=16)
N   | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  1 |              14.27 |                 14.40
  5 |              14.38 |                 14.21
 10 |              14.20 |                 14.32
 25 |              14.16 |                 14.30
 50 |              14.14 |                 14.24
100 |              14.26 |                 14.35
500 |              14.29 |                 14.32
================================================================================
  CONST RESOLVE (T vs const T)
================================================================================

Contract: Measures steady-state cost of tryResolve<T>() vs tryResolve<const T>() for unnamed services.
TypeKeyPolicy removes cv-qualifiers, so both resolve paths share the same type id.
[2026-02-16 03:38:04] Starting CPU: 2445 MHz (~base: 2445)
tryResolve<T> median (ns/op) | tryResolve<const T> median (ns/op) | Ratio
----------------------------|----------------------------------|------
                      2.18 |                             2.16 | 1.01x
================================================================================
  MUTATION COST (unregister / clear)
================================================================================

Contract: Measures registry mutation cost after services have been registered.
  A) unregister N distinct service types (ns/op).
  B) clear N service types (ns/op per entry).
[2026-02-16 03:38:04] Starting CPU: 3257 MHz (~base: 3257)
UNREGISTER (ns/op)
N   | median (ns/op)
----|---------------
  1 |         40.00
  5 |         24.00
 10 |         20.00
 25 |         21.64
 50 |         26.05
100 |         26.15

CLEAR (ns/op per entry)
N   | median (ns/op)
----|---------------
  1 |         80.00
  5 |         22.00
 10 |         15.00
 25 |         11.24
 50 |         10.62
100 |         10.72
================================================================================
  OVERHEAD ISOLATION MICRO-BENCHMARKS
================================================================================

Contract: Isolate individual overhead components to identify optimization targets.

[2026-02-16 03:38:04] Starting CPU: 3257 MHz (~base: 3257)
Measuring individual overhead components...

  1. Direct pointer access (baseline)          : 0.33 ns/op
  2. std::type_index construction              : 3.71 ns/op
  3. fat_p TypeKeyPolicy (static addr)         : 0.32 ns/op
  4. std::string construction (empty)          : 0.31 ns/op
  5. std::string from string_view (empty)      : 0.31 ns/op
  6. ServiceKey-like struct construction       : 0.31 ns/op
  7. ServiceKeyHash computation                : 4.04 ns/op
  8. unordered_map<type_index>.find()          : 6.53 ns/op
  9. unordered_map<void*>.find() (optimal)     : 1.25 ns/op
  10. ServiceKey map find (with makeKey)       : 5.30 ns/op
  11. std::optional construction + check       : 0.31 ns/op
  12. fat_p::service_locator::DefaultServiceLocator.tryResolve: 2.48 ns/op

Analysis:
  - Items 6+10 show ServiceKey construction + lookup overhead
  - Compare item 9 (optimal) vs item 10 (current) for improvement potential
  - Item 4-5 show std::string allocation overhead (even for empty strings)

Detailed gap analysis (tryResolve breakdown)...

  13. makeKey<T>() simulation                  : 0.31 ns/op
  14. SingleThreadedPolicy lock_shared()       : 0.31 ns/op
  15. SharedMutexPolicy lock_shared()          : 7.23 ns/op
  16. std::function copy                       : 3.43 ns/op
  17. std::shared_ptr<void> copy               : 6.22 ns/op
  18. std::optional<SnapLike> construction     : 0.31 ns/op
  19. FullSnapshot copy (Instance path)        : 2.18 ns/op
  20. FullSnapshot copy (Factory path)         : 4.05 ns/op
  21. Expected<ref_wrapper> construction       : 0.31 ns/op
  22. resolveEntryForRead simulation           : 13.48 ns/op
  23. tryResolve simulation (no snapshot)      : 7.54 ns/op
  24. fat_p tryResolve (actual)                : 2.54 ns/op

Gap Analysis Summary:
  Compare items 22-24 to identify where overhead accumulates.
  Item 23 vs 24 shows cost of snapshot copy + Expected wrapper.
  Item 16 (std::function copy) is often the hidden culprit.

StableHashMap comparison (reference stability)...

  25. StableHashMap<void*> find (no copy, SM64): 1.70 ns/op
  26. StableHashMap<ServiceKey> find           : 7.66 ns/op
  27. Optimal tryResolve (StableHashMap)       : 1.83 ns/op
  28. Current fat_p tryResolve                 : 2.67 ns/op

StableHashMap Advantage:
  - Reference stability eliminates snapshot copy (~10ns saved)
  - SIMD-accelerated probing (faster than std::unordered_map)
  - No shared_ptr atomic refcount overhead on resolve
  - Compare #27 vs #28 for potential improvement

Zero-cost abstraction verification...

  29. Raw StableHashMap (no lock)              : 1.64 ns/op
  30. With SingleThreadedPolicy lock           : 1.74 ns/op
  31. ServiceLocator tryResolve (optimized)    : 2.62 ns/op
  32. Minimal resolve (just hash lookup)       : 1.62 ns/op
  33. Static global (EnTT-style)               : 0.31 ns/op

If #29 == #30, SingleThreadedPolicy is truly zero-cost.
Gap between #32 and #33 is the irreducible hash lookup cost.
================================================================================
  ALTERNATIVE KEY STRATEGIES
================================================================================

Contract: Compare different key designs to identify optimization opportunities.

[2026-02-16 03:38:04] Starting CPU: 3241 MHz (~base: 3241)
Testing alternative ServiceKey designs...

  Strategy 1: void* + std::string (current)         : 5.92 ns/op
  Strategy 2: void* + string_view (zero-alloc)      : 7.15 ns/op
  Strategy 3: void* only (no names)                 : 1.23 ns/op
  Strategy 4: Cached hash (still allocates string)  : 7.78 ns/op
  Strategy 5: Two-level map (unnamed fast path)     : 0.98 ns/op
  Strategy 6: std::type_index (no names)            : 6.52 ns/op

Recommendations:
  - Strategy 3/5 show potential for unnamed services (~2x faster)
  - Strategy 2 eliminates allocation but requires API changes
  - Consider two-tier storage: fast path for type-only, slow path for named
================================================================================
  CONCURRENT RESOLUTION
================================================================================

Contract: Multi-threaded read-only resolution. Thread-safe variants only.

[2026-02-16 03:38:04] Starting CPU: 3241 MHz (~base: 3241)
Thread count: 4, ops/thread: 100000

  fat_p::service_locator::ThreadSafeServiceLocator: median=   84.25 mean=   84.18 +/-  0.39 [   84.00,    84.35]
  unordered_map<void*> + shared_mutex (type key): median=   80.14 mean=   80.05 +/-  0.80 [   79.70,    80.40]
  StableHashMap<void*> + shared_mutex (type key, SM64): median=   60.85 mean=   60.86 +/-  0.19 [   60.77,    60.94]
  unordered_map<type_index> + shared_mutex (precomputed key): median=   78.66 mean=   77.98 +/-  2.05 [   77.08,    78.88]

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
[OK] All adapters verified
================================================================================
  SINGLE-TYPE RESOLUTION
================================================================================

Contract: Resolve one service type. O(1) hash lookup for map-based, O(1) static access for EnTT.

[2026-02-16 04:11:40] Starting CPU: 3243 MHz (~base: 3243)
  fat_p::service_locator::DefaultServiceLocator: median=    5.08 mean=    5.10 +/-  0.04 [    5.08,     5.12]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   11.85 mean=   11.96 +/-  0.30 [   11.83,    12.09]
  entt::locator (static global)      : median=    0.31 mean=    0.32 +/-  0.02 [    0.31,     0.33]
  std::unordered_map<type_index>     : median=   25.38 mean=   25.71 +/-  1.01 [   25.27,    26.15]
  Direct pointers (baseline)         : median=    0.31 mean=    0.31 +/-  0.00 [    0.31,     0.31]
================================================================================
  MULTI-TYPE RESOLUTION (5 types)
================================================================================

Contract: Resolve 5 different service types per iteration. Measures cumulative lookup cost.

[2026-02-16 04:11:40] Starting CPU: 2445 MHz (~base: 2445)
  fat_p::service_locator::DefaultServiceLocator: median=    5.11 mean=    5.12 +/-  0.04 [    5.10,     5.14]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   11.72 mean=   11.75 +/-  0.11 [   11.70,    11.80]
  entt::locator (static global)      : median=    2.45 mean=    2.34 +/-  0.19 [    2.26,     2.42]
  std::unordered_map<type_index>     : median=   14.97 mean=   15.00 +/-  0.10 [   14.96,    15.04]
  Direct pointers (baseline)         : median=    2.47 mean=    2.33 +/-  0.22 [    2.23,     2.42]
================================================================================
  NAMED SERVICES (fat_p exclusive)
================================================================================

Contract: Resolve services by type+name composite key. Competitors do not support named services.

[2026-02-16 04:11:40] Starting CPU: 2445 MHz (~base: 2445)
  resolve<ILogger>() (default)       : median=    5.09 mean=    5.09 +/-  0.04 [    5.07,     5.11]
  resolve<ILogger>("file")           : median=   18.67 mean=   18.67 +/-  0.06 [   18.65,    18.70]
  resolve 3 named variants           : median=   14.64 mean=   14.63 +/-  0.05 [   14.61,    14.65]
================================================================================
  SCOPED RESOLUTION (fat_p exclusive)
================================================================================

Contract: Child scope overrides parent. Measures lookup with parent chain traversal.

[2026-02-16 04:11:40] Starting CPU: 2445 MHz (~base: 2445)
  resolve (child override)           : median=    5.08 mean=    5.07 +/-  0.05 [    5.05,     5.10]
  resolve (parent inheritance)       : median=   11.04 mean=   11.65 +/-  2.60 [   10.51,    12.79]
================================================================================
  REGISTRATION PERFORMANCE
================================================================================

Contract: Register 5 services including hash map insertion. Measures setup cost.

[2026-02-16 04:11:40] Starting CPU: 2445 MHz (~base: 2445)
  fat_p::service_locator::DefaultServiceLocator: median=   36.85 mean=   36.93 +/-  0.37 [   36.77,    37.10]
  std::unordered_map<type_index>     : median=   35.67 mean=   35.63 +/-  0.24 [   35.53,    35.74]
================================================================================
  SIZE SENSITIVITY
================================================================================

Contract: Measure resolve performance as number of registered services scales. Split into unnamed (type-only) and named (type+name) variants.

[2026-02-16 04:11:40] Starting CPU: 2445 MHz (~base: 2445)
UNNAMED (TYPE-ONLY)
Services | fat_p median (ns/op) | unordered_map<void*> median (ns/op) | Ratio (unordered_map / fat_p)
---------|----------------------|-----------------------------------|---------------------------
       1 |                 5.98 |                              4.62 |  0.77x
       5 |                15.89 |                              4.46 |  0.28x
      10 |                15.99 |                              4.32 |  0.27x
      25 |                16.75 |                              4.32 |  0.26x
      50 |                19.64 |                              4.32 |  0.22x
     100 |                26.51 |                              4.32 |  0.16x

NAMED (TYPE+NAME)
Note: The string-only unordered_map is a name-only key, not a composite (type+name) key.
      The composite-key unordered_map is the apples-to-apples comparator.
Services | fat_p median (ns/op) | unordered_map<string> median (ns/op) | unordered_map<composite> median (ns/op) | Ratio (composite / fat_p)
---------|----------------------|----------------------------------|----------------------------------|---------------------------
       1 |                21.31 |                             4.32 |                            11.52 |  0.54x
       5 |                21.06 |                            11.94 |                            14.51 |  0.69x
      10 |                21.45 |                            22.83 |                            14.31 |  0.67x
      25 |                22.39 |                            12.80 |                            18.82 |  0.84x
      50 |                24.45 |                            13.18 |                            16.60 |  0.68x
     100 |                23.53 |                            13.49 |                            20.82 |  0.88x
================================================================================
  STRING KEY HOT LOOP (named services)
================================================================================

Contract: Hot-loop resolves by type+name (string key) after startup registration.
Measures steady-state cost while varying:
  A) Name length (bytes) at fixed named-variant count.
  B) Named-variant count at fixed name length.
[2026-02-16 04:11:41] Starting CPU: 2445 MHz (~base: 2445)
NAME LENGTH SWEEP (variants=100)
Len | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  4 |              21.94 |                 18.50
  8 |              20.67 |                 17.42
 16 |              25.12 |                 18.48
 32 |              23.92 |                 20.41
 64 |              26.31 |                 25.03
128 |              41.56 |                 37.10

VARIANT COUNT SWEEP (len=16)
N   | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  1 |              25.10 |                 18.40
  5 |              25.13 |                 18.43
 10 |              25.08 |                 18.51
 25 |              25.19 |                 18.52
 50 |              25.50 |                 18.57
100 |              25.18 |                 18.53
500 |              25.50 |                 18.56
================================================================================
  CONST RESOLVE (T vs const T)
================================================================================

Contract: Measures steady-state cost of tryResolve<T>() vs tryResolve<const T>() for unnamed services.
TypeKeyPolicy removes cv-qualifiers, so both resolve paths share the same type id.
[2026-02-16 04:11:45] Starting CPU: 2445 MHz (~base: 2445)
tryResolve<T> median (ns/op) | tryResolve<const T> median (ns/op) | Ratio
----------------------------|----------------------------------|------
                      5.14 |                             5.25 | 0.98x
================================================================================
  MUTATION COST (unregister / clear)
================================================================================

Contract: Measures registry mutation cost after services have been registered.
  A) unregister N distinct service types (ns/op).
  B) clear N service types (ns/op per entry).
[2026-02-16 04:11:45] Starting CPU: 2445 MHz (~base: 2445)
UNREGISTER (ns/op)
N   | median (ns/op)
----|---------------
  1 |         40.00
  5 |         22.00
 10 |         21.55
 25 |         22.84
 50 |         26.04
100 |         28.45

CLEAR (ns/op per entry)
N   | median (ns/op)
----|---------------
  1 |         70.00
  5 |         20.00
 10 |         15.55
 25 |         12.82
 50 |         13.02
100 |         13.32
================================================================================
  OVERHEAD ISOLATION MICRO-BENCHMARKS
================================================================================

Contract: Isolate individual overhead components to identify optimization targets.

[2026-02-16 04:11:45] Starting CPU: 2445 MHz (~base: 2445)
Measuring individual overhead components...

  1. Direct pointer access (baseline)          : 2.43 ns/op
  2. std::type_index construction              : 3.73 ns/op
  3. fat_p TypeKeyPolicy (static addr)         : 2.32 ns/op
  4. std::string construction (empty)          : 1.87 ns/op
  5. std::string from string_view (empty)      : 1.87 ns/op
  6. ServiceKey-like struct construction       : 2.09 ns/op
  7. ServiceKeyHash computation                : 3.74 ns/op
  8. unordered_map<type_index>.find()          : 10.43 ns/op
  9. unordered_map<void*>.find() (optimal)     : 2.19 ns/op
  10. ServiceKey map find (with makeKey)       : 4.98 ns/op
  11. std::optional construction + check       : 2.30 ns/op
  12. fat_p::service_locator::DefaultServiceLocator.tryResolve: 5.07 ns/op

Analysis:
  - Items 6+10 show ServiceKey construction + lookup overhead
  - Compare item 9 (optimal) vs item 10 (current) for improvement potential
  - Item 4-5 show std::string allocation overhead (even for empty strings)

Detailed gap analysis (tryResolve breakdown)...

  13. makeKey<T>() simulation                  : 2.36 ns/op
  14. SingleThreadedPolicy lock_shared()       : 2.33 ns/op
  15. SharedMutexPolicy lock_shared()          : 7.14 ns/op
  16. std::function copy                       : 3.19 ns/op
  17. std::shared_ptr<void> copy               : 6.45 ns/op
  18. std::optional<SnapLike> construction     : 2.07 ns/op
  19. FullSnapshot copy (Instance path)        : 1.47 ns/op
  20. FullSnapshot copy (Factory path)         : 4.04 ns/op
  21. Expected<ref_wrapper> construction       : 1.88 ns/op
  22. resolveEntryForRead simulation           : 14.89 ns/op
  23. tryResolve simulation (no snapshot)      : 5.90 ns/op
  24. fat_p tryResolve (actual)                : 5.10 ns/op

Gap Analysis Summary:
  Compare items 22-24 to identify where overhead accumulates.
  Item 23 vs 24 shows cost of snapshot copy + Expected wrapper.
  Item 16 (std::function copy) is often the hidden culprit.

StableHashMap comparison (reference stability)...

  25. StableHashMap<void*> find (no copy, SM64): 1.59 ns/op
  26. StableHashMap<ServiceKey> find           : 6.77 ns/op
  27. Optimal tryResolve (StableHashMap)       : 2.18 ns/op
  28. Current fat_p tryResolve                 : 5.09 ns/op

StableHashMap Advantage:
  - Reference stability eliminates snapshot copy (~10ns saved)
  - SIMD-accelerated probing (faster than std::unordered_map)
  - No shared_ptr atomic refcount overhead on resolve
  - Compare #27 vs #28 for potential improvement

Zero-cost abstraction verification...

  29. Raw StableHashMap (no lock)              : 1.60 ns/op
  30. With SingleThreadedPolicy lock           : 2.13 ns/op
  31. ServiceLocator tryResolve (optimized)    : 5.11 ns/op
  32. Minimal resolve (just hash lookup)       : 1.57 ns/op
  33. Static global (EnTT-style)               : 1.93 ns/op

If #29 == #30, SingleThreadedPolicy is truly zero-cost.
Gap between #32 and #33 is the irreducible hash lookup cost.
================================================================================
  ALTERNATIVE KEY STRATEGIES
================================================================================

Contract: Compare different key designs to identify optimization opportunities.

[2026-02-16 04:11:45] Starting CPU: 2799 MHz (~base: 2799)
Testing alternative ServiceKey designs...

  Strategy 1: void* + std::string (current)         : 5.57 ns/op
  Strategy 2: void* + string_view (zero-alloc)      : 4.99 ns/op
  Strategy 3: void* only (no names)                 : 2.21 ns/op
  Strategy 4: Cached hash (still allocates string)  : 6.26 ns/op
  Strategy 5: Two-level map (unnamed fast path)     : 2.27 ns/op
  Strategy 6: std::type_index (no names)            : 10.60 ns/op

Recommendations:
  - Strategy 3/5 show potential for unnamed services (~2x faster)
  - Strategy 2 eliminates allocation but requires API changes
  - Consider two-tier storage: fast path for type-only, slow path for named
================================================================================
  CONCURRENT RESOLUTION
================================================================================

Contract: Multi-threaded read-only resolution. Thread-safe variants only.

[2026-02-16 04:11:45] Starting CPU: 2834 MHz (~base: 2834)
Thread count: 4, ops/thread: 100000

  fat_p::service_locator::ThreadSafeServiceLocator: median=   77.51 mean=   77.08 +/-  1.05 [   76.62,    77.54]
  unordered_map<void*> + shared_mutex (type key): median=   79.69 mean=   78.16 +/-  3.31 [   76.71,    79.61]
  StableHashMap<void*> + shared_mutex (type key, SM64): median=   59.24 mean=   58.91 +/-  0.86 [   58.54,    59.29]
  unordered_map<type_index> + shared_mutex (precomputed key): median=   83.33 mean=   80.97 +/-  4.49 [   79.00,    82.93]

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
[OK] All adapters verified
================================================================================
  SINGLE-TYPE RESOLUTION
================================================================================

Contract: Resolve one service type. O(1) hash lookup for map-based, O(1) static access for EnTT.

[2026-02-16 04:54:37] Starting CPU: 2445 MHz (base: 2445)
  fat_p::service_locator::DefaultServiceLocator: median=    5.86 mean=    6.11 +/-  1.13 [    5.61,     6.60]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   11.65 mean=   11.68 +/-  0.14 [   11.62,    11.74]
  entt::locator (static global)      : median=    0.31 mean=    0.35 +/-  0.14 [    0.29,     0.41]
  std::unordered_map<type_index>     : median=   19.88 mean=   19.91 +/-  0.06 [   19.88,    19.94]
  Direct pointers (baseline)         : median=    0.31 mean=    0.31 +/-  0.02 [    0.30,     0.32]
================================================================================
  MULTI-TYPE RESOLUTION (5 types)
================================================================================

Contract: Resolve 5 different service types per iteration. Measures cumulative lookup cost.

[2026-02-16 04:54:37] Starting CPU: 2445 MHz (base: 2445)
  fat_p::service_locator::DefaultServiceLocator: median=    5.87 mean=    5.96 +/-  0.15 [    5.89,     6.03]
  fat_p::service_locator::ThreadSafeServiceLocator: median=   11.59 mean=   11.50 +/-  0.16 [   11.43,    11.57]
  entt::locator (static global)      : median=    0.36 mean=    0.35 +/-  0.02 [    0.34,     0.36]
  std::unordered_map<type_index>     : median=   15.45 mean=   15.41 +/-  0.27 [   15.30,    15.53]
  Direct pointers (baseline)         : median=    0.36 mean=    0.36 +/-  0.01 [    0.36,     0.37]
================================================================================
  NAMED SERVICES (fat_p exclusive)
================================================================================

Contract: Resolve services by type+name composite key. Competitors do not support named services.

[2026-02-16 04:54:37] Starting CPU: 2445 MHz (base: 2445)
  resolve<ILogger>() (default)       : median=    5.56 mean=    5.64 +/-  0.14 [    5.58,     5.70]
  resolve<ILogger>("file")           : median=   35.64 mean=   35.64 +/-  0.24 [   35.54,    35.75]
  resolve 3 named variants           : median=   26.66 mean=   26.64 +/-  0.10 [   26.60,    26.68]
================================================================================
  SCOPED RESOLUTION (fat_p exclusive)
================================================================================

Contract: Child scope overrides parent. Measures lookup with parent chain traversal.

[2026-02-16 04:54:37] Starting CPU: 2445 MHz (base: 2445)
  resolve (child override)           : median=    5.66 mean=    5.69 +/-  0.14 [    5.63,     5.75]
  resolve (parent inheritance)       : median=   13.25 mean=   14.24 +/-  2.01 [   13.36,    15.12]
================================================================================
  REGISTRATION PERFORMANCE
================================================================================

Contract: Register 5 services including hash map insertion. Measures setup cost.

[2026-02-16 04:54:37] Starting CPU: 2445 MHz (base: 2445)
  fat_p::service_locator::DefaultServiceLocator: median=   80.92 mean=   83.85 +/-  8.91 [   79.95,    87.76]
  std::unordered_map<type_index>     : median=   71.72 mean=   76.13 +/- 13.37 [   70.28,    81.99]
================================================================================
  SIZE SENSITIVITY
================================================================================

Contract: Measure resolve performance as number of registered services scales. Split into unnamed (type-only) and named (type+name) variants.

[2026-02-16 04:54:37] Starting CPU: 2445 MHz (base: 2445)
UNNAMED (TYPE-ONLY)
Services | fat_p median (ns/op) | unordered_map<void*> median (ns/op) | Ratio (unordered_map / fat_p)
---------|----------------------|-----------------------------------|---------------------------
       1 |                 9.35 |                              7.86 |  0.84x
       5 |                 9.61 |                              7.86 |  0.82x
      10 |                 9.53 |                              7.85 |  0.82x
      25 |                23.70 |                              9.10 |  0.38x
      50 |                23.52 |                              7.61 |  0.32x
     100 |                19.51 |                              7.86 |  0.40x

NAMED (TYPE+NAME)
Note: The string-only unordered_map is a name-only key, not a composite (type+name) key.
      The composite-key unordered_map is the apples-to-apples comparator.
Services | fat_p median (ns/op) | unordered_map<string> median (ns/op) | unordered_map<composite> median (ns/op) | Ratio (composite / fat_p)
---------|----------------------|----------------------------------|----------------------------------|---------------------------
       1 |                36.19 |                             9.35 |                            23.58 |  0.65x
       5 |                36.20 |                             9.50 |                            23.66 |  0.65x
      10 |                36.10 |                             9.37 |                            23.86 |  0.66x
      25 |                37.94 |                            11.04 |                            25.34 |  0.67x
      50 |                37.99 |                            12.45 |                            26.63 |  0.70x
     100 |                38.91 |                            14.54 |                            27.98 |  0.72x
================================================================================
  STRING KEY HOT LOOP (named services)
================================================================================

Contract: Hot-loop resolves by type+name (string key) after startup registration.
Measures steady-state cost while varying:
  A) Name length (bytes) at fixed named-variant count.
  B) Named-variant count at fixed name length.
[2026-02-16 04:54:38] Starting CPU: 2445 MHz (base: 2445)
NAME LENGTH SWEEP (variants=100)
Len | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  4 |              18.60 |                 18.55
  8 |              21.18 |                 20.62
 16 |              29.94 |                 29.92
 32 |              45.68 |                 45.86
 64 |              91.61 |                 91.08
128 |             181.62 |                181.28

VARIANT COUNT SWEEP (len=16)
N   | Repeat AAAA (ns/op) | Alternate ABAB (ns/op)
----|--------------------|-----------------------
  1 |              30.02 |                 30.05
  5 |              30.03 |                 30.03
 10 |              30.02 |                 30.04
 25 |              30.02 |                 30.06
 50 |              30.03 |                 30.05
100 |              29.97 |                 30.08
500 |              30.03 |                 30.09
================================================================================
  CONST RESOLVE (T vs const T)
================================================================================

Contract: Measures steady-state cost of tryResolve<T>() vs tryResolve<const T>() for unnamed services.
TypeKeyPolicy removes cv-qualifiers, so both resolve paths share the same type id.
[2026-02-16 04:54:46] Starting CPU: 2445 MHz (base: 2445)
tryResolve<T> median (ns/op) | tryResolve<const T> median (ns/op) | Ratio
----------------------------|----------------------------------|------
                      5.70 |                             5.70 | 1.00x
================================================================================
  MUTATION COST (unregister / clear)
================================================================================

Contract: Measures registry mutation cost after services have been registered.
  A) unregister N distinct service types (ns/op).
  B) clear N service types (ns/op per entry).
[2026-02-16 04:54:46] Starting CPU: 2445 MHz (base: 2445)
UNREGISTER (ns/op)
N   | median (ns/op)
----|---------------
  1 |          0.00
  5 |         40.00
 10 |         30.00
 25 |         32.00
 50 |         34.00
100 |         34.00

CLEAR (ns/op per entry)
N   | median (ns/op)
----|---------------
  1 |        100.00
  5 |         20.00
 10 |         20.00
 25 |         16.00
 50 |         18.00
100 |         17.00
================================================================================
  OVERHEAD ISOLATION MICRO-BENCHMARKS
================================================================================

Contract: Isolate individual overhead components to identify optimization targets.

[2026-02-16 04:54:46] Starting CPU: 2445 MHz (base: 2445)
Measuring individual overhead components...

  1. Direct pointer access (baseline)          : 0.31 ns/op
  2. std::type_index construction              : 16.54 ns/op
  3. fat_p TypeKeyPolicy (static addr)         : 0.31 ns/op
  4. std::string construction (empty)          : 3.15 ns/op
  5. std::string from string_view (empty)      : 8.43 ns/op
  6. ServiceKey-like struct construction       : 12.47 ns/op
  7. ServiceKeyHash computation                : 2.89 ns/op
  8. unordered_map<type_index>.find()          : 13.80 ns/op
  9. unordered_map<void*>.find() (optimal)     : 5.60 ns/op
  10. ServiceKey map find (with makeKey)       : 18.65 ns/op
  11. std::optional construction + check       : 1.90 ns/op
  12. fat_p::service_locator::DefaultServiceLocator.tryResolve: 8.55 ns/op

Analysis:
  - Items 6+10 show ServiceKey construction + lookup overhead
  - Compare item 9 (optimal) vs item 10 (current) for improvement potential
  - Item 4-5 show std::string allocation overhead (even for empty strings)

Detailed gap analysis (tryResolve breakdown)...

  13. makeKey<T>() simulation                  : 0.62 ns/op
  14. SingleThreadedPolicy lock_shared()       : 0.62 ns/op
  15. SharedMutexPolicy lock_shared()          : 6.30 ns/op
  16. std::function copy                       : 3.79 ns/op
  17. std::shared_ptr<void> copy               : 4.47 ns/op
  18. std::optional<SnapLike> construction     : 0.31 ns/op
  19. FullSnapshot copy (Instance path)        : 3.49 ns/op
  20. FullSnapshot copy (Factory path)         : 4.99 ns/op
  21. Expected<ref_wrapper> construction       : 0.62 ns/op
  22. resolveEntryForRead simulation           : 25.75 ns/op
  23. tryResolve simulation (no snapshot)      : 9.05 ns/op
  24. fat_p tryResolve (actual)                : 8.50 ns/op

Gap Analysis Summary:
  Compare items 22-24 to identify where overhead accumulates.
  Item 23 vs 24 shows cost of snapshot copy + Expected wrapper.
  Item 16 (std::function copy) is often the hidden culprit.

StableHashMap comparison (reference stability)...

  25. StableHashMap<void*> find (no copy, SM64): 3.14 ns/op
  26. StableHashMap<ServiceKey> find           : 13.92 ns/op
  27. Optimal tryResolve (StableHashMap)       : 3.45 ns/op
  28. Current fat_p tryResolve                 : 8.46 ns/op

StableHashMap Advantage:
  - Reference stability eliminates snapshot copy (~10ns saved)
  - SIMD-accelerated probing (faster than std::unordered_map)
  - No shared_ptr atomic refcount overhead on resolve
  - Compare #27 vs #28 for potential improvement

Zero-cost abstraction verification...

  29. Raw StableHashMap (no lock)              : 4.05 ns/op
  30. With SingleThreadedPolicy lock           : 3.89 ns/op
  31. ServiceLocator tryResolve (optimized)    : 10.44 ns/op
  32. Minimal resolve (just hash lookup)       : 3.85 ns/op
  33. Static global (EnTT-style)               : 0.87 ns/op

If #29 == #30, SingleThreadedPolicy is truly zero-cost.
Gap between #32 and #33 is the irreducible hash lookup cost.
================================================================================
  ALTERNATIVE KEY STRATEGIES
================================================================================

Contract: Compare different key designs to identify optimization opportunities.

[2026-02-16 04:54:47] Starting CPU: 2445 MHz (base: 2445)
Testing alternative ServiceKey designs...

  Strategy 1: void* + std::string (current)         : 13.09 ns/op
  Strategy 2: void* + string_view (zero-alloc)      : 11.13 ns/op
  Strategy 3: void* only (no names)                 : 1.18 ns/op
  Strategy 4: Cached hash (still allocates string)  : 12.62 ns/op
  Strategy 5: Two-level map (unnamed fast path)     : 1.28 ns/op
  Strategy 6: std::type_index (no names)            : 11.61 ns/op

Recommendations:
  - Strategy 3/5 show potential for unnamed services (~2x faster)
  - Strategy 2 eliminates allocation but requires API changes
  - Consider two-tier storage: fast path for type-only, slow path for named
================================================================================
  CONCURRENT RESOLUTION
================================================================================

Contract: Multi-threaded read-only resolution. Thread-safe variants only.

[2026-02-16 04:54:47] Starting CPU: 2445 MHz (base: 2445)
Thread count: 4, ops/thread: 100000

  fat_p::service_locator::ThreadSafeServiceLocator: median=   30.86 mean=   30.79 +/-  0.50 [   30.57,    31.01]
  unordered_map<void*> + shared_mutex (type key): median=   29.16 mean=   29.10 +/-  0.24 [   29.00,    29.21]
  StableHashMap<void*> + shared_mutex (type key, SM64): median=   31.76 mean=   31.82 +/-  0.43 [   31.63,    32.01]
  unordered_map<type_index> + shared_mutex (precomputed key): median=   52.48 mean=   52.47 +/-  0.42 [   52.29,    52.66]

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
