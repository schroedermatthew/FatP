---
doc_id: UM-ENHANCEDBOUNDSCHECKING-001
doc_type: "User Manual"
title: "EnhancedBoundsChecking"
fatp_components: ["EnhancedBoundsChecking"]
topics: ["bounds checking", "index validation", "out_of_range", "debug-only checks", "enforce integration", "custom exception", "diagnostic messages", "container bounds", "multi-dimensional bounds", "range validation", "slice validation"]
constraints: ["debug_bounds_check compiled away in release", "enforce_bounds requires Enforce component"]
cxx_standard: "C++20"
std_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# User Manual - EnhancedBoundsChecking

*February 2026*

---

**Scope:** Usage guide for the fifteen bounds-checking functions in `EnhancedBoundsChecking.h`.

**Not covered:** The internals of Fat-P containers' own `at()` implementations (containers implement `at()` using these functions internally; the functions themselves --- including `check_container_bounds()` --- are covered here).

**Prerequisites:** C++20. Familiarity with `std::out_of_range`.

---

## User Manual Card

**Component:** EnhancedBoundsChecking
**Primary use case:** Validate indices with detailed error messages
**Key API:** `bounds_check()`, `debug_bounds_check()`, `bounds_check_with<E>()`, `enforce_bounds()`, `debug_enforce_bounds()`, `check_container_bounds()`, `bounds_check_2d()`, `bounds_check_nd()`, `validate_range()`, `validate_slice()` (each of the last five also has a `debug_` variant)
**Common mistakes:** Using `bounds_check()` in hot loops (always-on cost); forgetting `debug_bounds_check()` is stripped in release
**Performance notes:** Each check is a single comparison + predicted branch; branch prediction strongly favors the in-bounds path

---

## The Fifteen Functions

Eight check families; every family except `bounds_check_with` also has a `debug_`-prefixed variant that is compiled away when `NDEBUG` is defined.

### bounds_check(index, min, max, context) / debug_bounds_check(...)

Always-on (the `debug_` variant is zero cost in release). Throws `std::out_of_range` if `index < min || index >= max`. All three value parameters share one template type, so pass consistent types:

```cpp
fat_p::bounds_check(i, size_t{0}, vec.size(), "vector element");
// Throws: "vector element 15 out of range [0, 10)"
```

The message format is `<context> <index> out of range [<min>, <max>)`.

### bounds_check_with<ExceptionT>(index, min, max, context)

Throws a custom exception type instead of `std::out_of_range`:

```cpp
fat_p::bounds_check_with<MyRangeError>(i, 0, n, "sensor reading");
```

### enforce_bounds(index, min, max, context) / debug_enforce_bounds(...)

Integrates with Fat-P's enforce system. Uses `fat_p::enforce()` predicates for consistent assertion behavior across the library.

### check_container_bounds(container, index, context) / debug_check_container_bounds(...)

Checks `index` against `container.size()` (any container with a `.size()` method). Equivalent to `bounds_check(index, size_t{0}, container.size(), context)`:

```cpp
fat_p::check_container_bounds(vec, i, "vector element");
```

### bounds_check_2d(row, col, rows, cols, context) / debug_bounds_check_2d(...)

Checks a row/column pair against a matrix shape. Throws `std::out_of_range` with the message `<context> (<row>, <col>) out of bounds for shape (<rows>, <cols>)`:

```cpp
fat_p::bounds_check_2d(r, c, mat.rows(), mat.cols(), "matrix element");
```

### bounds_check_nd(indices, shape, context) / debug_bounds_check_nd(...)

Checks a `std::vector<size_t>` of indices against a `std::vector<size_t>` shape. Throws `std::invalid_argument` if the index count does not match the shape's dimensionality, and `std::out_of_range` naming the offending dimension otherwise:

```cpp
fat_p::bounds_check_nd({i, j, k}, {nx, ny, nz}, "tensor element");
```

### validate_range(start, end, size, context) / debug_validate_range(...)

Validates that a half-open range `[start, end)` fits within `[0, size)`. Throws `std::out_of_range` with the message `<context> [<start>, <end>) invalid for size <size>`:

```cpp
fat_p::validate_range(first, last, vec.size(), "view window");
```

### validate_slice(start, stop, step, size, context) / debug_validate_slice(...)

Validates tensor/array slice parameters, including negative steps. Throws `std::invalid_argument` if `step == 0`, and `std::out_of_range` for invalid start/stop bounds:

```cpp
fat_p::validate_slice(start, stop, step, data.size(), "tensor slice");
```

---

## When to Use Which

| Function | Release cost | Use case |
|---|---|---|
| `bounds_check` | Single comparison + branch | Public API boundaries, user-facing input |
| `debug_bounds_check` | Zero (compiled out) | Internal invariant checks, hot paths |
| `bounds_check_with<E>` | Single comparison + branch | Domain-specific exception hierarchies |
| `enforce_bounds` | Single comparison + branch | Integration with enforce predicates |
| `debug_enforce_bounds` | Zero (compiled out) | Internal checks with enforce integration |
| `check_container_bounds` | Comparison + branch | Index into any container with `.size()` |
| `bounds_check_2d` | Four comparisons + branch | Matrix/image row-column access |
| `bounds_check_nd` | O(dimensions) comparisons | Tensor indexing with runtime dimensionality |
| `validate_range` | Four comparisons + branch | Subranges, views, windows |
| `validate_slice` | Several comparisons + branch | Python-style start/stop/step slicing |

Each of the last five also has a `debug_`-prefixed variant with zero release cost.

---

## Best Practices

Use `debug_bounds_check()` inside performance-critical code and `bounds_check()` at API boundaries where invalid input should always be caught. Include descriptive context strings---"matrix row index" is far more useful than "Index" in a stack trace.

---

## Troubleshooting

### Exception message says "Index" instead of something descriptive

The default context is `"Index"`. Pass a meaningful context string as the fourth argument.

### No bounds checking in release

`debug_bounds_check()` and `debug_enforce_bounds()` are compiled away when `NDEBUG` is defined. Use `bounds_check()` (always-on) for checks that must run in release.

---

## API Reference

| Function | Throws | Release behavior |
|---|---|---|
| `bounds_check(idx, min, max, ctx)` | `std::out_of_range` | Active |
| `debug_bounds_check(idx, min, max, ctx)` | `std::out_of_range` | Compiled away |
| `bounds_check_with<E>(idx, min, max, ctx)` | `E` | Active |
| `enforce_bounds(idx, min, max, ctx)` | via enforce system | Active |
| `debug_enforce_bounds(idx, min, max, ctx)` | via enforce system | Compiled away |
| `check_container_bounds(cont, idx, ctx)` | `std::out_of_range` | Active |
| `debug_check_container_bounds(cont, idx, ctx)` | `std::out_of_range` | Compiled away |
| `bounds_check_2d(row, col, rows, cols, ctx)` | `std::out_of_range` | Active |
| `debug_bounds_check_2d(row, col, rows, cols, ctx)` | `std::out_of_range` | Compiled away |
| `bounds_check_nd(indices, shape, ctx)` | `std::invalid_argument` (dimension mismatch), `std::out_of_range` | Active |
| `debug_bounds_check_nd(indices, shape, ctx)` | same as above | Compiled away |
| `validate_range(start, end, size, ctx)` | `std::out_of_range` | Active |
| `debug_validate_range(start, end, size, ctx)` | `std::out_of_range` | Compiled away |
| `validate_slice(start, stop, step, size, ctx)` | `std::invalid_argument` (`step == 0`), `std::out_of_range` | Active |
| `debug_validate_slice(start, stop, step, size, ctx)` | same as above | Compiled away |

---

*EnhancedBoundsChecking.h --- Fat-P Library*
