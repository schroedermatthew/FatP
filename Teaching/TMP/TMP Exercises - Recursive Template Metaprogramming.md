# TMP Exercises - Recursive Template Metaprogramming

## Purpose

These exercises teach the three recursive TMP patterns used in ArrayView's internals. Each exercise states what the compiler is figuring out, which pattern to use, and why. Students should work each exercise on paper first (trace the recursion with a concrete example), then implement and verify with `static_assert`.

## Prerequisites

- Read the "How to Read the TMP Traits" section of the Developer Manual - ArrayView
- Understand the difference between inheritance-based recursion (Peeler), nested-using recursion (BuildArrayType), and value-computing TMP (fold expressions, `if constexpr` recursion, `consteval` loops)
- Comfortable with `static_assert`, `std::is_same_v`, parameter packs, partial specialization

## The Three Questions (Review)

Before starting any exercise, answer:

1. **What is the compiler figuring out — a type or a value?**
2. **If a type:** does each step transform the result, or does the base case produce it alone?
3. **If a value:** is it a reduction, a lookup, or a scan?

---

## Exercise 1: Pack Reversal (Inheritance Pattern)

### The Problem

Write a trait `Reverse` that reverses the order of types in a parameter pack:

```cpp
// Reverse<int, double, float>::type should be std::tuple<float, double, int>
static_assert(std::is_same_v<
    Reverse<int, double, float>::type,
    std::tuple<float, double, int>
>);
```

### What Is the Compiler Figuring Out?

A **type** — specifically, a `std::tuple` with the pack elements in reversed order.

### Which Pattern and Why?

Inheritance (the Peeler pattern). The recursion accumulates elements into a growing pack, and the base case produces the final `std::tuple` from the fully-accumulated pack. No step needs to transform the result on the way back up — the answer is computed once at the bottom.

This directly parallels Peeler: Peeler strips array extents and accumulates them left-to-right into the `Accumulated` pack. Reverse strips types from the front of the input pack and prepends them to the `Accumulated` pack (reversing the order).

### Hints

The key difference from Peeler is the direction of accumulation. Peeler appends (`Accumulated..., N`). Reverse prepends (`First, Accumulated...`). Think about why prepending reverses the order: the first element stripped becomes the last element accumulated.

### Paper Trace

Before coding, trace `Reverse<int, double, float>` on paper:

```
Step 0: Input = <int, double, float>, Accumulated = <>
Step 1: Input = <double, float>,      Accumulated = <int>
Step 2: Input = <float>,              Accumulated = <double, int>
Step 3: Input = <>,                   Accumulated = <float, double, int>
Base case: type = tuple<float, double, int>  ✓
```

### Solution

```cpp
#include <tuple>
#include <type_traits>

// Base case: input pack is empty, accumulated pack is the answer.
// This is the ONLY place ::type is defined.
template<typename Accumulated, typename... Input>
struct ReverseImpl;

template<typename... Acc>
struct ReverseImpl<std::tuple<Acc...>>
{
    using type = std::tuple<Acc...>;
};

// Recursive case: peel First off Input, prepend to Accumulated, inherit.
// Does NOT define ::type — inherits it from the base case.
template<typename... Acc, typename First, typename... Rest>
struct ReverseImpl<std::tuple<Acc...>, First, Rest...>
    : ReverseImpl<std::tuple<First, Acc...>, Rest...>
{};

// Public interface: start with empty accumulator
template<typename... Ts>
using Reverse = ReverseImpl<std::tuple<>, Ts...>;

// Tests
static_assert(std::is_same_v<Reverse<int, double, float>::type,
                              std::tuple<float, double, int>>);
static_assert(std::is_same_v<Reverse<int>::type,
                              std::tuple<int>>);
static_assert(std::is_same_v<Reverse<>::type,
                              std::tuple<>>);
```

### What to Notice

The recursive specialization has no `using type`. It inherits from the next level. The `type` defined in the base case propagates upward through the inheritance chain. This is the same mechanism as Peeler — and for the same reason: no step needs to modify the result on the way back up.

---

## Exercise 2: Nested Pair Construction (Nested-Using Pattern)

### The Problem

Write a trait `NestRight` that builds a right-nested `std::pair` chain from a parameter pack:

```cpp
// NestRight<int, double, float>::type should be std::pair<int, std::pair<double, float>>
static_assert(std::is_same_v<
    NestRight<int, double, float>::type,
    std::pair<int, std::pair<double, float>>
>);
```

### What Is the Compiler Figuring Out?

A **type** — a nested `std::pair` structure.

### Which Pattern and Why?

Nested `using type` (the BuildArrayType pattern). Each recursive step must wrap the result from the inner recursion: `type = std::pair<First, inner::type>`. The base case produces the innermost type, and each step on the way back up wraps it with one more `pair` layer. Inheritance cannot express this wrapping — each level must define its own `type`.

This directly parallels BuildArrayType: BuildArrayType wraps the inner result with `[First]` (`type = inner::type[First]`). NestRight wraps the inner result with `pair<First, ...>` (`type = pair<First, inner::type>`).

### Paper Trace

```
Descent:
  NestRight<int, double, float>  →  needs NestRight<double, float>::type
  NestRight<double, float>       →  needs NestRight<float>::type (base case)

Base case:
  NestRight<float>::type = float

Ascent (wrapping on the way back up):
  NestRight<double, float>::type = pair<double, float>
  NestRight<int, double, float>::type = pair<int, pair<double, float>>  ✓
```

### Solution

```cpp
#include <utility>
#include <type_traits>

// Primary template (declared but not defined — forces specialization matching)
template<typename... Ts>
struct NestRight;

// Base case: single type, no wrapping needed
template<typename T>
struct NestRight<T>
{
    using type = T;
};

// Recursive case: wrap inner result with pair<First, ...>
// EACH LEVEL defines its own ::type
template<typename First, typename... Rest>
struct NestRight<First, Rest...>
{
    using type = std::pair<First, typename NestRight<Rest...>::type>;
};

// Tests
static_assert(std::is_same_v<
    NestRight<int, double, float>::type,
    std::pair<int, std::pair<double, float>>
>);
static_assert(std::is_same_v<
    NestRight<int>::type,
    int
>);
static_assert(std::is_same_v<
    NestRight<int, double>::type,
    std::pair<int, double>
>);
```

### What to Notice

Every level defines `using type`. The recursive case says "my type is `pair<First, whatever-the-inner-type-is>`." This is the inside-out construction — the same pattern as BuildArrayType, where each level says "my type is `inner::type[First]`."

Try rewriting this with inheritance. You will find it impossible: there is no way to say "inherit `::type` but wrap it in `pair<First, ...>`" through inheritance alone.

---

## Exercise 3: Type Index Finder (Value-Computing, if constexpr Recursion)

### The Problem

Write a function `type_index` that finds the position of a type in a parameter pack:

```cpp
static_assert(type_index<double, int, double, float>() == 1);
static_assert(type_index<float, int, double, float>() == 2);
static_assert(type_index<int, int, double, float>() == 0);
```

### What Is the Compiler Figuring Out?

A **value** — the `size_t` index of the target type in the pack.

### Which Pattern and Why?

`if constexpr` recursion (the `nth_pack_element` pattern). The function peels one type off the front of the pack each step, checking if it matches the target. If it matches, return the current index. If not, recurse with the remaining types.

This parallels `nth_pack_element`, which peels elements until it reaches position I. Here we peel elements until we find a type match.

### Paper Trace

```
type_index<double, int, double, float>()
  First = int, Target = double → no match → 1 + type_index<double, double, float>()
  First = double, Target = double → match → return 0
  Result: 1 + 0 = 1  ✓
```

### Solution

```cpp
#include <cstddef>
#include <type_traits>

template<typename Target, typename First, typename... Rest>
constexpr std::size_t type_index()
{
    if constexpr (std::is_same_v<Target, First>)
        return 0;
    else
        return 1 + type_index<Target, Rest...>();
}

// Tests
static_assert(type_index<double, int, double, float>() == 1);
static_assert(type_index<float, int, double, float>() == 2);
static_assert(type_index<int, int, double, float>() == 0);
```

### What to Notice

This is a `constexpr` function, not a class template with specializations. The answer is a value, so normal C++ control flow works — no need for partial specialization machinery. The `if constexpr` branch is evaluated at compile time, and the non-taken branch is discarded without instantiation (which matters because `type_index<Target>()` with an empty `Rest` pack would be ill-formed).

### Extension

What happens if the target type is not in the pack? The current implementation fails to compile (empty pack, no matching call). Add a `static_assert` with a clear message, or return a sentinel value like `SIZE_MAX`:

```cpp
template<typename Target>
constexpr std::size_t type_index()
{
    return static_cast<std::size_t>(-1);  // Not found sentinel
}

template<typename Target, typename First, typename... Rest>
constexpr std::size_t type_index()
{
    if constexpr (std::is_same_v<Target, First>)
        return 0;
    else
        return 1 + type_index<Target, Rest...>();
}
```

---

## Exercise 4: Pointer Depth Counter (Value-Computing, constexpr Recursion on Types)

### The Problem

Write a trait that counts how many levels of pointer indirection a type has:

```cpp
static_assert(pointer_depth<int>() == 0);
static_assert(pointer_depth<int*>() == 1);
static_assert(pointer_depth<int**>() == 2);
static_assert(pointer_depth<int***>() == 3);
static_assert(pointer_depth<const int**>() == 2);
```

### What Is the Compiler Figuring Out?

A **value** — the `size_t` count of pointer levels.

### Which Pattern and Why?

`if constexpr` recursion, but this time recursing on the *type* rather than a *parameter pack*. Each step checks if T is a pointer; if so, strip one pointer level and recurse. This mirrors `flatPtr`, which recurses on the type until it reaches a non-array type.

### Paper Trace

```
pointer_depth<int**>()
  T = int** → is_pointer → 1 + pointer_depth<int*>()
  T = int*  → is_pointer → 1 + pointer_depth<int>()
  T = int   → not pointer → 0
  Result: 1 + 1 + 0 = 2  ✓
```

### Solution

```cpp
#include <cstddef>
#include <type_traits>

template<typename T>
constexpr std::size_t pointer_depth()
{
    if constexpr (std::is_pointer_v<T>)
        return 1 + pointer_depth<std::remove_pointer_t<T>>();
    else
        return 0;
}

// Tests
static_assert(pointer_depth<int>() == 0);
static_assert(pointer_depth<int*>() == 1);
static_assert(pointer_depth<int**>() == 2);
static_assert(pointer_depth<int***>() == 3);
static_assert(pointer_depth<const int**>() == 2);
```

### What to Notice

The recursion peels type structure (pointers) rather than parameter pack elements. This is valid because `if constexpr` makes the recursive call conditional — without it, `pointer_depth<int>` would try to instantiate `pointer_depth<remove_pointer_t<int>>` even when `int` is not a pointer, causing infinite recursion.

---

## Exercise 5: Type-to-Pointer Builder (Nested-Using Pattern on Values)

### The Problem

Write a trait that adds N levels of pointer indirection to a type:

```cpp
// AddPointers<int, 3>::type should be int***
static_assert(std::is_same_v<AddPointers<int, 0>::type, int>);
static_assert(std::is_same_v<AddPointers<int, 1>::type, int*>);
static_assert(std::is_same_v<AddPointers<int, 3>::type, int***>);
```

### What Is the Compiler Figuring Out?

A **type** — the input type with N pointer levels added.

### Which Pattern and Why?

Nested `using type`. Each step must wrap the inner result with one more pointer level: `type = inner::type*`. This parallels the KokkosDataType dynamic specialization, which adds a pointer star at each level (`type = KokkosDataType<T*, Rest...>::type`).

This exercise inverts Exercise 4: Exercise 4 counts pointers (value from type), this one builds pointers (type from value).

### Paper Trace

```
AddPointers<int, 3>  →  needs AddPointers<int, 2>::type
AddPointers<int, 2>  →  needs AddPointers<int, 1>::type
AddPointers<int, 1>  →  needs AddPointers<int, 0>::type
AddPointers<int, 0>  →  base case: type = int

Ascent:
  AddPointers<int, 0>::type = int
  AddPointers<int, 1>::type = int*
  AddPointers<int, 2>::type = int**
  AddPointers<int, 3>::type = int***  ✓
```

### Solution

```cpp
#include <type_traits>

template<typename T, std::size_t N>
struct AddPointers
{
    using type = typename AddPointers<T, N - 1>::type*;
};

template<typename T>
struct AddPointers<T, 0>
{
    using type = T;
};

// Tests
static_assert(std::is_same_v<AddPointers<int, 0>::type, int>);
static_assert(std::is_same_v<AddPointers<int, 1>::type, int*>);
static_assert(std::is_same_v<AddPointers<int, 3>::type, int***>);
static_assert(std::is_same_v<AddPointers<double, 2>::type, double**>);
```

### What to Notice

The wrapping `::type*` happens at every level — each step defines its own `type` by adding one star to the inner result. Inheritance cannot express this. The partial specialization on `N = 0` is the base case, unlike the pack-based traits where the base case is an empty pack.

---

## Exercise 6: Tuple Element Extractor (Inheritance + Value Indexing)

### The Problem

Write a trait `TupleElement` that extracts the I-th type from a parameter pack (your own version of `std::tuple_element`):

```cpp
static_assert(std::is_same_v<TupleElement<0, int, double, float>::type, int>);
static_assert(std::is_same_v<TupleElement<1, int, double, float>::type, double>);
static_assert(std::is_same_v<TupleElement<2, int, double, float>::type, float>);
```

### What Is the Compiler Figuring Out?

A **type** — the I-th type in the pack.

### Which Pattern and Why?

This is a choice point. You can solve it with either pattern:

**Option A: Inheritance.** Peel elements off the pack, decrementing I each time. When I reaches 0, the base case defines `type = First`. No step transforms the result — it propagates up via inheritance. This parallels `nth_pack_element` but as a type trait instead of a value function.

**Option B: Nested using.** Each level could define `using type = typename TupleElement<I-1, Rest...>::type` — but this is unnecessary wrapping because the type passes through unchanged. Inheritance is simpler.

### Solution (Inheritance Pattern)

```cpp
#include <cstddef>
#include <type_traits>

// Recursive case: I > 0, skip First, inherit with I-1 and Rest...
template<std::size_t I, typename First, typename... Rest>
struct TupleElement : TupleElement<I - 1, Rest...>
{};

// Base case: I == 0, First is the answer
template<typename First, typename... Rest>
struct TupleElement<0, First, Rest...>
{
    using type = First;
};

// Tests
static_assert(std::is_same_v<TupleElement<0, int, double, float>::type, int>);
static_assert(std::is_same_v<TupleElement<1, int, double, float>::type, double>);
static_assert(std::is_same_v<TupleElement<2, int, double, float>::type, float>);
```

### What to Notice

The recursive specialization does not define `type`. It inherits. The base case (I == 0) is the only place `type` is defined. This is correct because no step needs to modify the result — the I-th type passes through unchanged.

Compare with the `type_index` function from Exercise 3, which solves the inverse problem (value from types rather than type from value). That one uses a `constexpr` function because the answer is a value. This one uses a class template because the answer is a type.

---

## Exercise 7: Constrained Filtering (Combining All Three Patterns)

### The Problem

Write a trait `Filter` that extracts only the types satisfying a predicate from a parameter pack:

```cpp
// Keep only pointer types
static_assert(std::is_same_v<
    Filter<std::is_pointer, int, double*, float, int*>::type,
    std::tuple<double*, int*>
>);
```

### What Is the Compiler Figuring Out?

A **type** — a `std::tuple` containing only the types that satisfy the predicate.

### Which Pattern and Why?

Inheritance with conditional accumulation. Like Reverse (Exercise 1), the base case produces the final `std::tuple` from an accumulated pack. But unlike Reverse, each step conditionally includes or skips the current element based on the predicate. The predicate evaluation is a value computation (`Pred<T>::value`), but the overall result is a type — so we need the struct-based approach for the outer recursion.

### Paper Trace

```
Filter<is_pointer, int, double*, float, int*>
  int: is_pointer<int> = false → skip → inherit with Acc=<>
  double*: is_pointer<double*> = true → include → inherit with Acc=<double*>
  float: is_pointer<float> = false → skip → inherit with Acc=<double*>
  int*: is_pointer<int*> = true → include → inherit with Acc=<double*, int*>
  Base case: type = tuple<double*, int*>  ✓
```

### Solution

```cpp
#include <tuple>
#include <type_traits>

// Primary template
template<template<typename> class Pred, typename Accumulated, typename... Input>
struct FilterImpl;

// Base case: no more input, accumulated pack is the answer
template<template<typename> class Pred, typename... Acc>
struct FilterImpl<Pred, std::tuple<Acc...>>
{
    using type = std::tuple<Acc...>;
};

// Recursive case: First satisfies predicate → append to accumulator
template<template<typename> class Pred, typename... Acc, typename First, typename... Rest>
    requires (Pred<First>::value)
struct FilterImpl<Pred, std::tuple<Acc...>, First, Rest...>
    : FilterImpl<Pred, std::tuple<Acc..., First>, Rest...>
{};

// Recursive case: First does NOT satisfy predicate → skip it
template<template<typename> class Pred, typename... Acc, typename First, typename... Rest>
    requires (!Pred<First>::value)
struct FilterImpl<Pred, std::tuple<Acc...>, First, Rest...>
    : FilterImpl<Pred, std::tuple<Acc...>, Rest...>
{};

// Public interface
template<template<typename> class Pred, typename... Ts>
using Filter = FilterImpl<Pred, std::tuple<>, Ts...>;

// Tests
static_assert(std::is_same_v<
    Filter<std::is_pointer, int, double*, float, int*>::type,
    std::tuple<double*, int*>
>);
static_assert(std::is_same_v<
    Filter<std::is_pointer, int, float>::type,
    std::tuple<>
>);
static_assert(std::is_same_v<
    Filter<std::is_integral, int, double, short, float, long>::type,
    std::tuple<int, short, long>
>);
```

### What to Notice

This combines a value computation (the predicate check) with type-level recursion (the accumulation). The two constrained specializations replace the `if constexpr` that a `constexpr` function would use — we cannot use `if constexpr` because the answer is a type, not a value. The `requires` clauses on the two recursive specializations make them mutually exclusive, so exactly one matches for each element.

The inheritance pattern is used because no step transforms the result — the accumulated `tuple` is built during descent and produced as-is by the base case.

---

## Summary: Pattern Selection Cheat Sheet

| Exercise | Answer | Pattern | Why |
|----------|--------|---------|-----|
| 1. Reverse | Type | Inheritance | Accumulates during descent, base produces result |
| 2. NestRight | Type | Nested using | Each step wraps inner result with `pair<First, ...>` |
| 3. TypeIndex | Value | `if constexpr` recursion | Peels pack, returns count |
| 4. PointerDepth | Value | `if constexpr` recursion | Peels type structure, returns count |
| 5. AddPointers | Type | Nested using | Each step wraps inner result with `*` |
| 6. TupleElement | Type | Inheritance | Peels pack, base case produces result unchanged |
| 7. Filter | Type | Inheritance + constraint | Conditionally accumulates, base produces result |

The decision rule remains the same as in ArrayView's code:

- **Computing a type with no per-step transformation:** inheritance (Peeler, Reverse, TupleElement)
- **Computing a type with per-step wrapping:** nested `using type` (BuildArrayType, NestRight, AddPointers)
- **Computing a value:** `constexpr` function (dynamic_slot, TypeIndex, PointerDepth)

---

*Exercises for Developer Manual - ArrayView TMP Patterns*
