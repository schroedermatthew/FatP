---
doc_id: BR-StrongId-001
doc_type: "Benchmark Results"
title: "StrongId"
fatp_components: ["StrongId"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - StrongId

**Source:** `benchmark_StrongId.cpp`
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
| fat_p::StrongId<Checked> | x | x | x | x |
| fat_p::StrongId<Unchecked> | x | x | x | x |
| fluent::NamedType | x | x | x | x |
| ts::strong_typedef (type_safe) | x | x | x | — |
| strong::type (rollbear) | x | x | x | x |
| boost::strong_typedef | x | x | x | x |
| enum class (built-in) | x | x | x | x |
| Manual wrapper struct | x | x | x | x |
| Raw int | x | x | x | x |
| ts::strong_typedef (vcpkg install type-safe) | — | — | — | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
------------------------------------------------------------------------
  CONSTRUCTION (from int)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.09       ns  (stddev: 0.01  )  1.00 x
  fat_p::StrongId (Unchecked)   0.09       ns  (stddev: 0.01  )  1.00 x
  fluent::NamedType             0.09       ns  (stddev: 0.01  )  1.03 x
  ts::strong_typedef            0.09       ns  (stddev: 0.01  )  1.00 x
  strong::type (rollbear)       0.20       ns  (stddev: 0.01  )  2.23 x
  boost::strong_typedef         0.09       ns  (stddev: 0.01  )  1.00 x
  enum class                    0.09       ns  (stddev: 0.01  )  1.00 x
  Manual wrapper struct         0.09       ns  (stddev: 0.09  )  1.00 x
  Raw int (baseline)            0.09       ns  (stddev: 0.01  )  1.00 x

------------------------------------------------------------------------
  COMPARISON (operator<)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.38       ns  (stddev: 0.01  )  1.01 x
  fat_p::StrongId (Unchecked)   0.37       ns  (stddev: 0.04  )  1.00 x
  fluent::NamedType             0.38       ns  (stddev: 0.01  )  1.01 x
  ts::strong_typedef            0.38       ns  (stddev: 0.07  )  1.02 x
  strong::type (rollbear)       0.37       ns  (stddev: 0.01  )  1.00 x
  boost::strong_typedef         0.38       ns  (stddev: 0.07  )  1.02 x
  enum class                    0.38       ns  (stddev: 0.00  )  1.01 x
  Manual wrapper struct         0.37       ns  (stddev: 0.08  )  1.00 x
  Raw int (baseline)            0.37       ns  (stddev: 0.00  )  1.00 x

------------------------------------------------------------------------
  ADDITION (compound +=)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     1.39       ns  (stddev: 0.12  )  1.49 x
  fat_p::StrongId (Unchecked)   0.93       ns  (stddev: 0.04  )  1.00 x
  fluent::NamedType             0.92       ns  (stddev: 0.09  )  0.99 x
  ts::strong_typedef            0.93       ns  (stddev: 0.10  )  1.00 x
  strong::type (rollbear)       0.93       ns  (stddev: 0.05  )  1.00 x
  boost::strong_typedef         0.94       ns  (stddev: 0.12  )  1.01 x
  enum class                    0.93       ns  (stddev: 0.04  )  1.00 x
  Manual wrapper struct         0.94       ns  (stddev: 0.07  )  1.01 x
  Raw int (baseline)            0.93       ns  (stddev: 0.08  )  1.00 x

------------------------------------------------------------------------
  INCREMENT (prefix ++)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.19       ns  (stddev: 0.03  )  1.03 x
  fat_p::StrongId (Unchecked)   0.19       ns  (stddev: 0.07  )  1.00 x
  fluent::NamedType             0.19       ns  (stddev: 0.00  )  1.00 x
  ts::strong_typedef            0.19       ns  (stddev: 0.01  )  1.00 x
  strong::type (rollbear)       0.19       ns  (stddev: 0.07  )  1.00 x
  boost::strong_typedef         0.19       ns  (stddev: 0.01  )  1.01 x
  enum class                    0.19       ns  (stddev: 0.01  )  1.00 x
  Manual wrapper struct         0.19       ns  (stddev: 0.04  )  1.00 x
  Raw int (baseline)            0.19       ns  (stddev: 0.01  )  1.00 x

------------------------------------------------------------------------
  HASH (std::hash)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.60       ns  (stddev: 0.10  )  1.00 x
  fat_p::StrongId (Unchecked)   0.60       ns  (stddev: 0.08  )  1.00 x
  fluent::NamedType             0.59       ns  (stddev: 0.06  )  1.00 x
  ts::strong_typedef            0.59       ns  (stddev: 0.02  )  1.00 x
  strong::type (rollbear)       0.62       ns  (stddev: 0.10  )  1.05 x
  boost::strong_typedef         0.60       ns  (stddev: 0.06  )  1.00 x
  enum class                    0.60       ns  (stddev: 0.05  )  1.00 x
  Manual wrapper struct         0.60       ns  (stddev: 0.04  )  1.00 x
  Raw int (baseline)            0.60       ns  (stddev: 0.11  )  1.00 x

========================================================================
  INTERPRETATION GUIDE
========================================================================

  1.00x = Zero overhead (identical to raw int)
  <1.5x = Negligible overhead for most applications
  >2.0x = Measurable overhead, consider for hot paths

  Key findings:
  - fat_p::StrongId (Unchecked) should match raw int exactly
  - fat_p::StrongId (Checked) has overflow detection cost
  - enum class lacks arithmetic/increment operators
  - Manual wrapper validates zero-overhead design pattern

========================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
------------------------------------------------------------------------
  CONSTRUCTION (from int)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.14       ns  (stddev: 0.05  )  1.00 x
  fat_p::StrongId (Unchecked)   0.14       ns  (stddev: 0.00  )  1.00 x
  fluent::NamedType             0.14       ns  (stddev: 0.00  )  1.00 x
  ts::strong_typedef            0.14       ns  (stddev: 0.00  )  1.00 x
  strong::type (rollbear)       0.14       ns  (stddev: 0.00  )  1.00 x
  boost::strong_typedef         0.14       ns  (stddev: 0.01  )  1.00 x
  enum class                    0.14       ns  (stddev: 0.00  )  1.00 x
  Manual wrapper struct         0.14       ns  (stddev: 0.01  )  1.00 x
  Raw int (baseline)            0.14       ns  (stddev: 0.00  )  1.00 x

------------------------------------------------------------------------
  COMPARISON (operator<)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.14       ns  (stddev: 0.00  )  1.00 x
  fat_p::StrongId (Unchecked)   0.14       ns  (stddev: 0.00  )  1.00 x
  fluent::NamedType             0.14       ns  (stddev: 0.00  )  1.00 x
  ts::strong_typedef            0.14       ns  (stddev: 0.00  )  1.00 x
  strong::type (rollbear)       0.14       ns  (stddev: 0.00  )  1.00 x
  boost::strong_typedef         0.14       ns  (stddev: 0.04  )  1.00 x
  enum class                    0.14       ns  (stddev: 0.00  )  1.00 x
  Manual wrapper struct         0.14       ns  (stddev: 0.00  )  1.00 x
  Raw int (baseline)            0.14       ns  (stddev: 0.04  )  1.00 x

------------------------------------------------------------------------
  ADDITION (compound +=)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     2.08       ns  (stddev: 0.03  )  1.34 x
  fat_p::StrongId (Unchecked)   1.55       ns  (stddev: 0.03  )  1.00 x
  fluent::NamedType             1.55       ns  (stddev: 0.00  )  1.00 x
  ts::strong_typedef            1.55       ns  (stddev: 0.04  )  1.00 x
  strong::type (rollbear)       1.55       ns  (stddev: 0.18  )  1.00 x
  boost::strong_typedef         1.55       ns  (stddev: 0.04  )  1.00 x
  enum class                    1.55       ns  (stddev: 0.05  )  1.00 x
  Manual wrapper struct         1.55       ns  (stddev: 0.03  )  1.00 x
  Raw int (baseline)            1.55       ns  (stddev: 0.03  )  1.00 x

------------------------------------------------------------------------
  INCREMENT (prefix ++)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.62       ns  (stddev: 0.05  )  2056.17x
  fat_p::StrongId (Unchecked)   0.00       ns  (stddev: 0.00  )  1.00 x
  fluent::NamedType             0.00       ns  (stddev: 0.00  )  1.00 x
  ts::strong_typedef            0.00       ns  (stddev: 0.00  )  1.00 x
  strong::type (rollbear)       0.00       ns  (stddev: 0.00  )  1.00 x
  boost::strong_typedef         0.00       ns  (stddev: 0.00  )  1.00 x
  enum class                    0.00       ns  (stddev: 0.00  )  1.00 x
  Manual wrapper struct         0.00       ns  (stddev: 0.00  )  1.00 x
  Raw int (baseline)            0.00       ns  (stddev: 0.00  )  1.00 x

------------------------------------------------------------------------
  HASH (std::hash)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.15       ns  (stddev: 0.03  )  1.00 x
  fat_p::StrongId (Unchecked)   0.15       ns  (stddev: 0.00  )  1.00 x
  fluent::NamedType             0.15       ns  (stddev: 0.00  )  1.00 x
  ts::strong_typedef            0.15       ns  (stddev: 0.01  )  1.00 x
  strong::type (rollbear)       0.15       ns  (stddev: 0.01  )  1.00 x
  boost::strong_typedef         0.15       ns  (stddev: 0.03  )  1.00 x
  enum class                    0.15       ns  (stddev: 0.05  )  1.01 x
  Manual wrapper struct         0.15       ns  (stddev: 0.00  )  1.00 x
  Raw int (baseline)            0.15       ns  (stddev: 0.00  )  1.00 x

========================================================================
  INTERPRETATION GUIDE
========================================================================

  1.00x = Zero overhead (identical to raw int)
  <1.5x = Negligible overhead for most applications
  >2.0x = Measurable overhead, consider for hot paths

  Key findings:
  - fat_p::StrongId (Unchecked) should match raw int exactly
  - fat_p::StrongId (Checked) has overflow detection cost
  - enum class lacks arithmetic/increment operators
  - Manual wrapper validates zero-overhead design pattern

========================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
------------------------------------------------------------------------
  CONSTRUCTION (from int)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.07       ns  (stddev: 0.04  )  1.32 x
  fat_p::StrongId (Unchecked)   0.05       ns  (stddev: 0.01  )  1.00 x
  fluent::NamedType             0.05       ns  (stddev: 0.00  )  0.99 x
  ts::strong_typedef            0.05       ns  (stddev: 0.01  )  0.99 x
  strong::type (rollbear)       0.06       ns  (stddev: 0.01  )  1.20 x
  boost::strong_typedef         0.05       ns  (stddev: 0.01  )  0.99 x
  enum class                    0.06       ns  (stddev: 0.00  )  1.11 x
  Manual wrapper struct         0.06       ns  (stddev: 0.01  )  1.17 x
  Raw int (baseline)            0.05       ns  (stddev: 0.01  )  1.00 x

------------------------------------------------------------------------
  COMPARISON (operator<)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.14       ns  (stddev: 0.00  )  1.00 x
  fat_p::StrongId (Unchecked)   0.14       ns  (stddev: 0.00  )  1.01 x
  fluent::NamedType             0.14       ns  (stddev: 0.03  )  1.00 x
  ts::strong_typedef            0.14       ns  (stddev: 0.00  )  1.01 x
  strong::type (rollbear)       0.14       ns  (stddev: 0.00  )  1.01 x
  boost::strong_typedef         0.14       ns  (stddev: 0.00  )  1.00 x
  enum class                    0.14       ns  (stddev: 0.00  )  1.00 x
  Manual wrapper struct         0.14       ns  (stddev: 0.04  )  1.01 x
  Raw int (baseline)            0.14       ns  (stddev: 0.03  )  1.00 x

------------------------------------------------------------------------
  ADDITION (compound +=)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     2.41       ns  (stddev: 0.02  )  2.21 x
  fat_p::StrongId (Unchecked)   1.08       ns  (stddev: 0.03  )  0.99 x
  fluent::NamedType             1.08       ns  (stddev: 0.03  )  0.99 x
  ts::strong_typedef            1.00       ns  (stddev: 0.02  )  0.92 x
  strong::type (rollbear)       1.09       ns  (stddev: 0.03  )  1.00 x
  boost::strong_typedef         1.09       ns  (stddev: 0.03  )  1.00 x
  enum class                    1.09       ns  (stddev: 0.03  )  1.00 x
  Manual wrapper struct         1.00       ns  (stddev: 0.08  )  0.92 x
  Raw int (baseline)            1.09       ns  (stddev: 0.02  )  1.00 x

------------------------------------------------------------------------
  INCREMENT (prefix ++)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.62       ns  (stddev: 0.03  )  2056.50x
  fat_p::StrongId (Unchecked)   0.00       ns  (stddev: 0.00  )  1.00 x
  fluent::NamedType             0.00       ns  (stddev: 0.00  )  1.00 x
  ts::strong_typedef            0.00       ns  (stddev: 0.00  )  1.00 x
  strong::type (rollbear)       0.00       ns  (stddev: 0.00  )  1.00 x
  boost::strong_typedef         0.00       ns  (stddev: 0.00  )  1.00 x
  enum class                    0.00       ns  (stddev: 0.00  )  1.00 x
  Manual wrapper struct         0.00       ns  (stddev: 0.00  )  1.00 x
  Raw int (baseline)            0.00       ns  (stddev: 0.00  )  1.00 x

------------------------------------------------------------------------
  HASH (std::hash)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.08       ns  (stddev: 0.01  )  0.86 x
  fat_p::StrongId (Unchecked)   0.09       ns  (stddev: 0.00  )  0.99 x
  fluent::NamedType             0.09       ns  (stddev: 0.00  )  0.99 x
  ts::strong_typedef            0.09       ns  (stddev: 0.05  )  0.99 x
  strong::type (rollbear)       0.08       ns  (stddev: 0.01  )  0.86 x
  boost::strong_typedef         0.10       ns  (stddev: 0.02  )  1.00 x
  enum class                    0.09       ns  (stddev: 0.00  )  1.00 x
  Manual wrapper struct         0.10       ns  (stddev: 0.01  )  1.00 x
  Raw int (baseline)            0.09       ns  (stddev: 0.03  )  1.00 x

========================================================================
  INTERPRETATION GUIDE
========================================================================

  1.00x = Zero overhead (identical to raw int)
  <1.5x = Negligible overhead for most applications
  >2.0x = Measurable overhead, consider for hot paths

  Key findings:
  - fat_p::StrongId (Unchecked) should match raw int exactly
  - fat_p::StrongId (Checked) has overflow detection cost
  - enum class lacks arithmetic/increment operators
  - Manual wrapper validates zero-overhead design pattern

========================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
------------------------------------------------------------------------
  CONSTRUCTION (from int)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.18       ns  (stddev: 0.03  )  1.00 x
  fat_p::StrongId (Unchecked)   0.18       ns  (stddev: 0.07  )  1.00 x
  fluent::NamedType             0.18       ns  (stddev: 0.02  )  1.00 x
  strong::type (rollbear)       0.42       ns  (stddev: 0.05  )  2.40 x
  boost::strong_typedef         0.18       ns  (stddev: 0.02  )  1.00 x
  enum class                    0.18       ns  (stddev: 0.06  )  1.00 x
  Manual wrapper struct         0.18       ns  (stddev: 0.02  )  1.00 x
  Raw int (baseline)            0.18       ns  (stddev: 0.03  )  1.00 x

------------------------------------------------------------------------
  COMPARISON (operator<)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.57       ns  (stddev: 0.05  )  1.00 x
  fat_p::StrongId (Unchecked)   0.57       ns  (stddev: 0.00  )  1.00 x
  fluent::NamedType             0.57       ns  (stddev: 0.05  )  1.00 x
  strong::type (rollbear)       0.57       ns  (stddev: 0.07  )  1.00 x
  boost::strong_typedef         0.57       ns  (stddev: 0.00  )  1.00 x
  enum class                    0.57       ns  (stddev: 0.01  )  1.00 x
  Manual wrapper struct         0.57       ns  (stddev: 0.03  )  1.00 x
  Raw int (baseline)            0.57       ns  (stddev: 0.01  )  1.00 x

------------------------------------------------------------------------
  ADDITION (compound +=)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     3.31       ns  (stddev: 1.03  )  1.49 x
  fat_p::StrongId (Unchecked)   2.10       ns  (stddev: 0.55  )  0.95 x
  fluent::NamedType             2.09       ns  (stddev: 0.71  )  0.94 x
  strong::type (rollbear)       2.19       ns  (stddev: 0.70  )  0.99 x
  boost::strong_typedef         2.23       ns  (stddev: 0.53  )  1.01 x
  enum class                    2.20       ns  (stddev: 0.63  )  0.99 x
  Manual wrapper struct         2.09       ns  (stddev: 0.55  )  0.94 x
  Raw int (baseline)            2.22       ns  (stddev: 0.62  )  1.00 x

------------------------------------------------------------------------
  INCREMENT (prefix ++)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     0.57       ns  (stddev: 0.00  )  0.00 x
  fat_p::StrongId (Unchecked)   0.00       ns  (stddev: 0.00  )  0.00 x
  fluent::NamedType             0.00       ns  (stddev: 0.00  )  0.00 x
  strong::type (rollbear)       0.00       ns  (stddev: 0.00  )  0.00 x
  boost::strong_typedef         0.00       ns  (stddev: 0.00  )  0.00 x
  enum class                    0.00       ns  (stddev: 0.00  )  0.00 x
  Manual wrapper struct         0.00       ns  (stddev: 0.00  )  0.00 x
  Raw int (baseline)            0.00       ns  (stddev: 0.00  )  0.00 x

------------------------------------------------------------------------
  HASH (std::hash)
------------------------------------------------------------------------

  fat_p::StrongId (Checked)     1.19       ns  (stddev: 0.10  )  1.00 x
  fat_p::StrongId (Unchecked)   1.19       ns  (stddev: 0.05  )  1.00 x
  fluent::NamedType             1.19       ns  (stddev: 0.07  )  1.00 x
  strong::type (rollbear)       1.27       ns  (stddev: 0.06  )  1.07 x
  boost::strong_typedef         1.19       ns  (stddev: 0.09  )  1.00 x
  enum class                    1.18       ns  (stddev: 0.05  )  1.00 x
  Manual wrapper struct         1.19       ns  (stddev: 0.07  )  1.00 x
  Raw int (baseline)            1.18       ns  (stddev: 0.03  )  1.00 x

========================================================================
  INTERPRETATION GUIDE
========================================================================

  1.00x = Zero overhead (identical to raw int)
  <1.5x = Negligible overhead for most applications
  >2.0x = Measurable overhead, consider for hot paths

  Key findings:
  - fat_p::StrongId (Unchecked) should match raw int exactly
  - fat_p::StrongId (Checked) has overflow detection cost
  - enum class lacks arithmetic/increment operators
  - Manual wrapper validates zero-overhead design pattern

========================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- ts::strong_typedef (vcpkg install type-safe) was not detected on MSVC CI.
