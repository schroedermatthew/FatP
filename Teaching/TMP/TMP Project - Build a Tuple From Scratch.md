# TMP Project - Build a Tuple From Scratch

## Purpose

This project builds a working `Tuple` class template step by step, using every TMP pattern from the ArrayView developer manual. Each phase adds one capability, exercises one pattern, and produces something testable. By the end, you have a functional tuple with element access, type queries, structured bindings support, and concatenation.

A tuple is the right vehicle for this because it exercises all three patterns naturally: storage uses recursive inheritance, type extraction uses the Peeler/inheritance pattern, element access uses value-computing, and concatenation uses the nested-using/accumulation pattern. Every C++ developer has used `std::tuple`; few have built one.

## Prerequisites

- Completed the abstract TMP exercises (or equivalent understanding)
- Can answer "type or value?" and "inheritance or nested using?" for a given problem
- Comfortable with `static_assert` testing

## How to Work Through This

Each phase has a **goal**, a **what the compiler is figuring out** statement, a **pattern** identification, a **paper trace**, and a **solution**. Write your implementation before looking at the solution. Test each phase with the provided `static_assert` and runtime checks before moving on.

All code goes in a single header `Tuple.hpp`. Each phase extends the previous one.

---

## Phase 1: Recursive Storage

### Goal

Store N values of different types in a single object using recursive inheritance. `Tuple<int, double, char>` should hold one `int`, one `double`, and one `char`.

### What Is the Compiler Figuring Out?

The **class layout** — which base classes to inherit from and which element each level stores. This is not a `::type` computation; it is the class hierarchy itself. The compiler figures out the inheritance chain at instantiation time.

### Pattern

Recursive inheritance. Each level of `TupleStorage<I, First, Rest...>` stores `First` as a data member and inherits from `TupleStorage<I+1, Rest...>`. The index `I` is carried along so that each level has a unique base class (even if two elements have the same type). The base case `TupleStorage<I>` (empty `Rest`) is an empty struct.

This parallels Peeler's inheritance chain, but instead of accumulating template parameters, each level accumulates a data member.

### Why Inheritance and Not a Recursive Member?

You could store elements as nested members: `struct Tuple { First head; Tuple<Rest...> tail; }`. This works but makes `get<I>()` harder — you would need to chase `.tail.tail.tail.head` at compile time. With inheritance, `get<I>()` is a single `static_cast` to the correct base class, because each base is at a known offset. The inheritance approach also enables empty base optimization: an empty tuple element occupies zero bytes.

### Paper Trace

For `Tuple<int, double, char>`:

```
TupleStorage<0, int, double, char>
  stores: int value  (at index 0)
  inherits from: TupleStorage<1, double, char>
    stores: double value  (at index 1)
    inherits from: TupleStorage<2, char>
      stores: char value  (at index 2)
      inherits from: TupleStorage<3>
        empty base case
```

The object layout (most-derived to base): the `int` member is in the `<0, int, double, char>` layer, the `double` member is in the `<1, double, char>` layer, the `char` member is in the `<2, char>` layer.

### Solution

```cpp
#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace my {

// Base case: no more elements. Empty struct.
template<std::size_t I, typename... Ts>
struct TupleStorage {};

// Recursive case: store First, inherit for the rest.
template<std::size_t I, typename First, typename... Rest>
struct TupleStorage<I, First, Rest...> : TupleStorage<I + 1, Rest...>
{
    First value;

    constexpr TupleStorage() = default;

    template<typename F, typename... Rs>
    constexpr TupleStorage(F&& f, Rs&&... rs)
        : TupleStorage<I + 1, Rest...>(std::forward<Rs>(rs)...)
        , value(std::forward<F>(f))
    {}
};

// The public Tuple class — just a TupleStorage starting at index 0.
template<typename... Ts>
struct Tuple : TupleStorage<0, Ts...>
{
    constexpr Tuple() = default;

    template<typename... Args>
    constexpr Tuple(Args&&... args)
        : TupleStorage<0, Ts...>(std::forward<Args>(args)...)
    {}
};

} // namespace my

// Phase 1 tests
static_assert(sizeof(my::Tuple<>) == 1);  // Empty, but not zero (C++ rule)
static_assert(sizeof(my::Tuple<int>) >= sizeof(int));
static_assert(std::is_default_constructible_v<my::Tuple<int, double>>);
```

### What to Notice

The recursive specialization does not define a `::type` — it defines a **data member** and an **inheritance relationship**. The "answer" the compiler is computing is the class layout itself, not a type alias. But the structural pattern is identical to Peeler: peel one element, process it (store it), recurse on the rest.

---

## Phase 2: tuple_size (Value-Computing)

### Goal

Compute the number of elements in a `Tuple` at compile time.

### What Is the Compiler Figuring Out?

A **value** — the `size_t` count of types in the pack.

### Pattern

This does not need recursion at all. `sizeof...(Ts)` is a built-in pack query that returns the count directly. The exercise is here to establish the interface and show that the simplest tool should be used when it suffices — do not reach for recursive TMP when a one-liner works.

### Solution

```cpp
template<typename T>
struct tuple_size;

template<typename... Ts>
struct tuple_size<Tuple<Ts...>>
{
    static constexpr std::size_t value = sizeof...(Ts);
};

template<typename T>
inline constexpr std::size_t tuple_size_v = tuple_size<T>::value;

// Tests
static_assert(tuple_size_v<Tuple<int, double, char>> == 3);
static_assert(tuple_size_v<Tuple<>> == 0);
static_assert(tuple_size_v<Tuple<float>> == 1);
```

### What to Notice

No recursion, no partial specialization chain. The answer is a value and `sizeof...` computes it in one expression. Compare with `rank_dynamic_v` from ArrayView, which also uses a one-shot fold expression rather than recursion. The lesson: identify the simplest mechanism that produces your answer.

---

## Phase 3: tuple_element (Type Extraction via Inheritance)

### Goal

Extract the type of the I-th element: `tuple_element_t<1, Tuple<int, double, char>>` should be `double`.

### What Is the Compiler Figuring Out?

A **type** — the I-th type in the pack.

### Pattern

Inheritance (the Peeler pattern). Peel types from the front of the pack, decrementing I. When I reaches 0, the base case defines `type = First`. No step transforms the result — it propagates up via inheritance unchanged.

This is identical to Exercise 6 (TupleElement) from the abstract exercises, now applied to a real use case.

### Solution

```cpp
template<std::size_t I, typename T>
struct tuple_element;

template<std::size_t I, typename First, typename... Rest>
struct tuple_element<I, Tuple<First, Rest...>>
    : tuple_element<I - 1, Tuple<Rest...>>
{};

template<typename First, typename... Rest>
struct tuple_element<0, Tuple<First, Rest...>>
{
    using type = First;
};

template<std::size_t I, typename T>
using tuple_element_t = typename tuple_element<I, T>::type;

// Tests
static_assert(std::is_same_v<tuple_element_t<0, Tuple<int, double, char>>, int>);
static_assert(std::is_same_v<tuple_element_t<1, Tuple<int, double, char>>, double>);
static_assert(std::is_same_v<tuple_element_t<2, Tuple<int, double, char>>, char>);
```

### What to Notice

The recursive specialization does not define `type`. It inherits. The base case (I == 0) is the only place `type` is defined. This is the Peeler pattern: accumulate state downward (decrement I), produce the answer at the bottom, propagate it up unchanged.

---

## Phase 4: get\<I\>() (Value Access via Base Cast)

### Goal

Access the I-th element by reference: `get<1>(t)` returns a `double&` for `Tuple<int, double, char>`.

### What Is the Compiler Figuring Out?

A **reference to a specific base class member**. The compiler must determine which `TupleStorage` base class holds element I, then cast the tuple to that base to access the `value` member.

### Pattern

This is not recursive TMP — it is a direct `static_cast` exploiting the inheritance chain built in Phase 1. The `TupleStorage<I, T, ...>` base class that holds element I is unique (because I is unique at each level). Casting to it gives access to the `value` member at that level.

### Why Does This Work?

In Phase 1, `TupleStorage<0, int, double, char>` inherits from `TupleStorage<1, double, char>`, which inherits from `TupleStorage<2, char>`. Each base has a `value` member of the appropriate type. When we `static_cast` a `Tuple<int, double, char>&` to `TupleStorage<1, double, char>&`, we get a reference to the sub-object that holds the `double` — and its `value` member is the `double` we want.

The key insight: `I` is not just an index into a pack — it is a template parameter of the base class. Each level of the hierarchy is a distinct type (`TupleStorage<0, ...>`, `TupleStorage<1, ...>`, etc.), so the cast is unambiguous even if two elements have the same type.

### Solution

```cpp
template<std::size_t I, typename... Ts>
constexpr auto& get(Tuple<Ts...>& t) noexcept
{
    using ElementType = tuple_element_t<I, Tuple<Ts...>>;
    // Cast to the base class that stores element I.
    // TupleStorage<I, ElementType, ...> is the unique base at index I.
    return static_cast<TupleStorage<I, ElementType,
        /* remaining types after I — but we don't need them for the cast */
        /* the compiler finds the unique base matching TupleStorage<I, ElementType, ...> */
    >&>(t).value;
}
```

Wait — we need the full suffix of the pack after position I for the cast, because `TupleStorage<I, ElementType>` and `TupleStorage<I, ElementType, Extra...>` are different types. A cleaner approach uses a helper that casts directly:

```cpp
// Helper: find the right base class by matching I
template<std::size_t I, typename First, typename... Rest>
constexpr First& getImpl(TupleStorage<I, First, Rest...>& s) noexcept
{
    return s.value;
}

template<std::size_t I, typename First, typename... Rest>
constexpr const First& getImpl(const TupleStorage<I, First, Rest...>& s) noexcept
{
    return s.value;
}

// Public interface: overload resolution finds the right base
template<std::size_t I, typename... Ts>
constexpr auto& get(Tuple<Ts...>& t) noexcept
{
    return getImpl<I>(t);
}

template<std::size_t I, typename... Ts>
constexpr const auto& get(const Tuple<Ts...>& t) noexcept
{
    return getImpl<I>(t);
}

// Tests (runtime)
void test_get()
{
    Tuple<int, double, char> t(42, 3.14, 'x');
    assert(get<0>(t) == 42);
    assert(get<1>(t) == 3.14);
    assert(get<2>(t) == 'x');

    get<0>(t) = 99;
    assert(get<0>(t) == 99);
}
```

### What to Notice

`getImpl<I>` takes a `TupleStorage<I, First, Rest...>&`. When called with a `Tuple<int, double, char>&` (which inherits from the entire `TupleStorage` chain), the compiler finds the unique base class matching `TupleStorage<I, ...>` and performs an implicit derived-to-base conversion. The template parameter `I` is fixed by the caller; `First` and `Rest` are deduced from the matching base class. No recursion at all — the inheritance hierarchy from Phase 1 does the work.

This is why Phase 1 used inheritance instead of nested members. With nested members, `get<2>()` would need to compile-time evaluate `t.tail.tail.head`. With inheritance, it is a single function call that the compiler resolves via implicit conversion to the correct base.

---

## Phase 5: Structured Bindings Support

### Goal

Make `auto [a, b, c] = Tuple<int, double, char>{1, 2.0, 'x'};` work.

### What Is the Compiler Figuring Out?

Nothing new — structured bindings use `tuple_size`, `tuple_element`, and `get<I>()`, all of which we built in Phases 2–4. The compiler applies these three customization points mechanically.

### Pattern

No new TMP. This phase connects the pieces. The compiler requires `tuple_size`, `tuple_element`, and `get` to be findable via ADL or in namespace `std`. We specialize the `std` versions.

### Solution

```cpp
// These specializations go in namespace std
namespace std {

template<typename... Ts>
struct tuple_size<my::Tuple<Ts...>>
    : std::integral_constant<std::size_t, sizeof...(Ts)>
{};

template<std::size_t I, typename... Ts>
struct tuple_element<I, my::Tuple<Ts...>>
    : my::tuple_element<I, my::Tuple<Ts...>>
{};

} // namespace std

// Test
void test_structured_bindings()
{
    my::Tuple<int, double, char> t(42, 3.14, 'x');
    auto [a, b, c] = t;
    assert(a == 42);
    assert(b == 3.14);
    assert(c == 'x');
}
```

### What to Notice

Structured bindings are not magic — they are a protocol. The compiler generates code equivalent to:

```cpp
auto __tmp = t;
auto& a = std::get<0>(__tmp);  // Uses get<I> (Phase 4)
auto& b = std::get<1>(__tmp);  // type of b = tuple_element_t<1, ...> (Phase 3)
auto& c = std::get<2>(__tmp);  // iterations = tuple_size_v<...> (Phase 2)
```

Every phase we built serves a purpose in this protocol.

---

## Phase 6: tuple_cat (Accumulation + Nested-Using)

### Goal

Concatenate two tuples: `tuple_cat(Tuple<int, double>{}, Tuple<char, float>{})` returns `Tuple<int, double, char, float>`.

### What Is the Compiler Figuring Out?

A **type** — the concatenated `Tuple<...>` — and a **value** — the indices needed to extract elements from both source tuples.

### Pattern

This combines two patterns. Computing the result type uses pack expansion (no recursion needed — just concatenate the packs). Constructing the result uses index sequences to extract elements from both source tuples and forward them to the result's constructor.

### Solution

```cpp
template<typename T1, typename T2>
struct tuple_cat_type;

template<typename... T1s, typename... T2s>
struct tuple_cat_type<Tuple<T1s...>, Tuple<T2s...>>
{
    using type = Tuple<T1s..., T2s...>;
};

template<typename T1, typename T2>
using tuple_cat_type_t = typename tuple_cat_type<T1, T2>::type;

template<typename... T1s, typename... T2s,
         std::size_t... I1s, std::size_t... I2s>
constexpr auto tuple_cat_impl(
    const Tuple<T1s...>& t1,
    const Tuple<T2s...>& t2,
    std::index_sequence<I1s...>,
    std::index_sequence<I2s...>)
{
    return tuple_cat_type_t<Tuple<T1s...>, Tuple<T2s...>>(
        get<I1s>(t1)..., get<I2s>(t2)...
    );
}

template<typename... T1s, typename... T2s>
constexpr auto tuple_cat(const Tuple<T1s...>& t1, const Tuple<T2s...>& t2)
{
    return tuple_cat_impl(
        t1, t2,
        std::make_index_sequence<sizeof...(T1s)>{},
        std::make_index_sequence<sizeof...(T2s)>{}
    );
}

// Tests
void test_cat()
{
    Tuple<int, double> t1(1, 2.0);
    Tuple<char, float> t2('a', 3.0f);
    auto t3 = tuple_cat(t1, t2);

    static_assert(std::is_same_v<decltype(t3), Tuple<int, double, char, float>>);
    assert(get<0>(t3) == 1);
    assert(get<1>(t3) == 2.0);
    assert(get<2>(t3) == 'a');
    assert(get<3>(t3) == 3.0f);
}
```

### What to Notice

The type computation (`Tuple<T1s..., T2s...>`) is a pack concatenation, not recursion — C++ allows expanding two packs in sequence. The value computation uses `std::index_sequence` to generate the indices 0..N-1 for each source tuple, then `get<I>(t)...` to expand them into constructor arguments. This is the same `index_sequence` + fold pattern that ArrayView's `ExtentStorage::apply` uses to forward dynamic extents to Kokkos.

---

## What You Built

At the end of these six phases, you have a working tuple with:

- Recursive inheritance for heterogeneous storage (Phase 1 — Peeler pattern)
- Compile-time element count (Phase 2 — value computing, trivial)
- Compile-time type extraction (Phase 3 — inheritance/Peeler pattern)
- Constant-time element access via base-class cast (Phase 4 — leveraging Phase 1's hierarchy)
- Structured bindings support (Phase 5 — protocol integration)
- Tuple concatenation (Phase 6 — pack expansion + index sequences)

Every pattern from the ArrayView developer manual appears:

| Tuple Phase | ArrayView Parallel | Pattern |
|-------------|-------------------|---------|
| Phase 1: Storage | `ExtentStorage` packed array | Recursive inheritance |
| Phase 2: `tuple_size` | `rank_dynamic_v` | Fold / `sizeof...` |
| Phase 3: `tuple_element` | `nth_pack_element` as a type | Inheritance, `::type` in base only |
| Phase 4: `get<I>()` | `ExtentStorage::get<I>()` | Base-class resolution |
| Phase 5: Structured bindings | — | Protocol (connects the pieces) |
| Phase 6: `tuple_cat` | `ExtentStorage::apply` | Index sequence expansion |

---

*TMP Project — Build a Tuple From Scratch*
