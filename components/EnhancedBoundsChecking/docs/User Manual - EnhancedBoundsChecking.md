---
doc_id: UM-ENHANCEDBOUNDSCHECKING-001
doc_type: "User Manual"
title: "EnhancedBoundsChecking"
fatp_components: ["EnhancedBoundsChecking"]
topics: ["bounds checking", "index validation", "out_of_range", "debug-only checks", "enforce integration", "custom exception", "diagnostic messages"]
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

**Scope:** Usage guide for the six bounds-checking functions in `EnhancedBoundsChecking.h`.

**Not covered:** Container-specific bounds checking (each container implements its own `at()` using these functions internally).

**Prerequisites:** C++20. Familiarity with `std::out_of_range`.

---

## User Manual Card

**Component:** EnhancedBoundsChecking
**Primary use case:** Validate indices with detailed error messages
**Key API:** `bounds_check()`, `debug_bounds_check()`, `bounds_check_with<E>()`, `enforce_bounds()`, `debug_enforce_bounds()`
**Common mistakes:** Using `bounds_check()` in hot loops (always-on cost); forgetting `debug_bounds_check()` is stripped in release
**Performance notes:** ~2-5 ns per check (branch prediction favors the in-bounds path)

---

## The Six Functions

### bounds_check(index, min, max, context)

Always-on. Throws `std::out_of_range` if `index < min || index >= max`:

```cpp
fat_p::bounds_check(i, 0, vec.size(), "vector element");
// Throws: "Index out of range: vector element index 15 not in [0, 10)"
```

### debug_bounds_check(index, min, max, context)

Same check, but compiled away when `NDEBUG` is defined. Zero cost in release.

### bounds_check_with<ExceptionT>(index, min, max, context)

Throws a custom exception type instead of `std::out_of_range`:

```cpp
fat_p::bounds_check_with<MyRangeError>(i, 0, n, "sensor reading");
```

### enforce_bounds(index, min, max, context) / debug_enforce_bounds(...)

Integrates with Fat-P's enforce system. Uses `fat_p::enforce()` predicates for consistent assertion behavior across the library.

---

## When to Use Which

| Function | Release cost | Use case |
|---|---|---|
| `bounds_check` | ~2-5 ns | Public API boundaries, user-facing input |
| `debug_bounds_check` | Zero | Internal invariant checks, hot paths |
| `bounds_check_with<E>` | ~2-5 ns | Domain-specific exception hierarchies |
| `enforce_bounds` | ~2-5 ns | Integration with enforce predicates |
| `debug_enforce_bounds` | Zero | Internal checks with enforce integration |

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

---

*EnhancedBoundsChecking.h --- Fat-P Library*
