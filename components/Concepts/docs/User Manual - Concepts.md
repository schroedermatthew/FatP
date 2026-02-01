---
doc_id: UM-CONCEPTS-001
doc_type: "User Manual"
title: "Concepts and FatPConcepts"
fatp_components: ["Concepts", "FatPConcepts"]
topics: ["C++20 concepts", "type constraints", "compile-time introspection", "SFINAE replacement", "container detection", "callable concepts", "Fat-P type detection"]
constraints: ["compile-time only", "zero runtime overhead", "type system limitations", "concept subsumption"]
cxx_standard: "C++20"
std_equivalent: "std::integral, std::invocable, std::ranges::range (partial)"
std_since: "C++20"
boost_equivalent: "Boost.TypeTraits (SFINAE-based, not C++20 concepts)"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-30"
audience: ["C++ developers", "library maintainers", "template library authors", "AI assistants"]
status: "reviewed"
---

# User Manual - Concepts and FatPConcepts

*Fat-P Library — January 2026*

---

## Table of Contents

1. [The Type Constraint Story](#the-type-constraint-story)
2. [Understanding Why SFINAE Hurts](#understanding-why-sfinae-hurts)
3. [The Requires Expression Insight](#the-requires-expression-insight)
4. [Two Headers, One Purpose](#two-headers-one-purpose)
5. [Getting Started](#getting-started)
6. [The Container Detection Problem: What Does "Iterable" Mean?](#the-container-detection-problem-what-does-iterable-mean)
7. [The Hashable Question: When Can You Hash?](#the-hashable-question-when-can-you-hash)
8. [The Callable Constraint: Type-Safe Callbacks](#the-callable-constraint-type-safe-callbacks)
9. [Stream and String Concepts: Universal Stringification](#stream-and-string-concepts-universal-stringification)
10. [Fat-P Type Detection: The Forward Declaration Pattern](#fat-p-type-detection-the-forward-declaration-pattern)
11. [Duck-Typed vs Exact-Match: Choosing Your Detection Strategy](#duck-typed-vs-exact-match-choosing-your-detection-strategy)
12. [Concept-Based Overloading: The Subsumption Hierarchy](#concept-based-overloading-the-subsumption-hierarchy)
13. [When to Use Concepts (and When Not To)](#when-to-use-concepts-and-when-not-to)
14. [Migration from SFINAE](#migration-from-sfinae)
15. [Troubleshooting](#troubleshooting)
16. [API Reference](#api-reference)
17. [Summary](#summary)

---

## The Type Constraint Story

### The Oldest Problem in Generic Programming

In 1988, Alexander Stepanov was working at Bell Labs on what would eventually become the Standard Template Library. He had a vision: algorithms should be written once and work with any data structure that supported the required operations. A `find` algorithm shouldn't care whether it's searching a linked list, an array, or a tree—it should work with anything that can be iterated.

The problem was expressing this requirement. C++ templates let you write generic code, but they didn't let you say *what the generic code needed*. You could write:

```cpp
template <typename Iterator>
Iterator find(Iterator first, Iterator last, const auto& value) {
    while (first != last) {
        if (*first == value) return first;
        ++first;
    }
    return last;
}
```

This works beautifully for `std::vector<int>::iterator`. But what happens when someone passes `int`?

The compiler tries to instantiate `find<int>`. It substitutes `int` for `Iterator`. Then it tries to compile `first != last` where both are `int`, and `++first` for an `int`. These operations work fine for `int`. But `*first`—dereferencing an integer? That's nonsense.

The compiler reports the error. But it reports it *inside the template*, pointing at `*first == value`. The user sees your implementation details. They see a line of code they didn't write, in a context they don't understand. The error message might span hundreds of lines as the compiler tries every overload, fails, and explains why.

The user's actual mistake was simple: they passed an integer where they should have passed an iterator. But nothing in your code *said* it needed an iterator. The requirement was implicit in the implementation, not explicit in the interface.

### The SFINAE Era

For decades, C++ programmers worked around this limitation using a technique called SFINAE—Substitution Failure Is Not An Error. The idea exploits a quirk in template overload resolution: when the compiler tries to instantiate a template and the substitution causes an error, that candidate is silently removed from consideration rather than causing a compile error.

By carefully constructing templates that would fail substitution for "wrong" types, you could selectively enable or disable function templates. The technique worked, but it required arcane incantations:

```cpp
template <typename T, typename = std::void_t<
    decltype(std::begin(std::declval<T&>())),
    decltype(std::end(std::declval<T&>())),
    decltype(*std::begin(std::declval<T&>()))
>>
void process(T& container) {
    for (auto& elem : container) {
        // ...
    }
}
```

This says "only enable this function if `T` has `begin()`, `end()`, and the result of `begin()` can be dereferenced." The syntax is impenetrable to anyone who hasn't studied template metaprogramming. Error messages remain cryptic. And you need a separate trait for every property you want to check.

### The C++20 Revolution

C++20 introduced concepts—named constraints that express requirements directly:

```cpp
template <typename T>
concept iterable = requires(T& val) {
    { std::begin(val) } -> std::input_or_output_iterator;
    { std::end(val) } -> std::input_or_output_iterator;
};

template <iterable T>
void process(T& container) {
    for (auto& elem : container) {
        // ...
    }
}
```

The concept says exactly what's required: `T` must have `begin()` and `end()` that return iterators. When you pass the wrong type, the compiler says "constraint `iterable<int>` not satisfied because `std::begin(val)` is not valid for `int`." The error points at the constraint, not the implementation.

This is what Stepanov dreamed of in 1988. After thirty years, C++ finally has a way to say what generic code needs.

---

## Understanding Why SFINAE Hurts

### The Boilerplate Tax

Before concepts, every type property required a custom trait. Want to detect if a type has `push_back`? Write a trait:

```cpp
template <typename T, typename = void>
struct has_push_back : std::false_type {};

template <typename T>
struct has_push_back<T, std::void_t<
    decltype(std::declval<T>().push_back(std::declval<typename T::value_type>()))
>> : std::true_type {};

template <typename T>
inline constexpr bool has_push_back_v = has_push_back<T>::value;
```

That's twelve lines for one property. A comprehensive type introspection library needs traits for `has_size`, `has_begin`, `has_end`, `has_data`, `has_reserve`, `has_clear`, `has_empty`, `is_hashable`, `is_equality_comparable`, `is_streamable`, `is_invocable`... Each trait follows the same pattern but with different expressions. You end up with hundreds of lines of nearly identical code.

Worse, these traits are fragile. The `has_push_back` trait above checks if you can call `push_back` with a `value_type`. But what if `value_type` is move-only? The trait fails even though `push_back(std::move(val))` would work. Getting SFINAE traits exactly right requires understanding subtle interactions between expression validity, implicit conversions, and reference qualification.

### The Error Message Catastrophe

When SFINAE-constrained code fails, the compiler walks through every overload, explains why each one failed, and eventually reports the error deep in template instantiation. Consider this code:

```cpp
template <typename T, std::enable_if_t<has_size_v<T> && has_begin_v<T>, int> = 0>
void process(const T& container);

process(42);  // Error!
```

A typical compiler produces output like:

```
error: no matching function for call to 'process(int)'
note: candidate: template<class T, std::enable_if_t<has_size_v<T> && has_begin_v<T>, int> <anonymous>>
      void process(const T&)
note: template argument deduction/substitution failed:
note: in substitution of 'template<class T, std::enable_if_t<has_size_v<T> && has_begin_v<T>, int> <anonymous>>
      void process(const T&) [with T = int; std::enable_if_t<has_size_v<T> && has_begin_v<T>, int> <anonymous> = 0]':
error: no type named 'type' in 'struct std::enable_if<false, int>'
```

The user sees `enable_if`, `has_size_v`, template argument deduction, and `no type named 'type'`. They don't see "process requires a container, and int isn't one."

With concepts, the same code produces:

```
error: constraints not satisfied for function template 'process'
note: because 'int' does not satisfy 'container'
note: because 'int' does not satisfy 'iterable'
note: because 'std::begin(val)' would be invalid
```

Three lines. Clear hierarchy. The user knows exactly what's required and exactly what's missing.

### The Overload Resolution Problem

SFINAE enables or disables overloads, but it doesn't rank them. If you have two overloads that both pass SFINAE checks, you get an ambiguity error:

```cpp
template <typename T, std::enable_if_t<is_iterable_v<T>, int> = 0>
void process(const T& t);  // Version A: any iterable

template <typename T, std::enable_if_t<has_data_v<T>, int> = 0>
void process(const T& t);  // Version B: has data() pointer

std::vector<int> v;
process(v);  // Ambiguous! Both constraints satisfied
```

The only solution is manual exclusion—make Version A explicitly exclude types that Version B handles:

```cpp
template <typename T, std::enable_if_t<is_iterable_v<T> && !has_data_v<T>, int> = 0>
void process(const T& t);  // Version A: iterable but no data()
```

This is fragile and doesn't scale. Adding a third overload means updating two existing ones.

---

## The Requires Expression Insight

### Constraint as Question

A concept is fundamentally a yes-or-no question about a type: "Does this type support these operations?" The `requires` expression is how you ask that question:

```cpp
template <typename T>
concept hashable = requires(const T& val) {
    { std::hash<T>{}(val) } -> std::convertible_to<std::size_t>;
};
```

Read this as: "T is hashable if, given a const reference `val` of type T, the expression `std::hash<T>{}(val)` is valid and returns something convertible to `std::size_t`."

The parameters in the `requires` clause (`const T& val`) are not real variables—they're hypothetical values used to form expressions. The compiler checks whether those expressions *would be* valid, without actually executing anything.

### Compound Requirements

The `{ expr } -> concept<args>` syntax is a compound requirement. It checks two things: that the expression is valid, and that its result satisfies another concept. You can also check just validity:

```cpp
template <typename T>
concept has_clear = requires(T& val) {
    val.clear();  // Just needs to be valid, don't care about return type
};
```

Or check multiple requirements together:

```cpp
template <typename T>
concept container = requires(T& val, const T& cval) {
    { std::begin(val) } -> std::input_or_output_iterator;
    { std::end(val) } -> std::input_or_output_iterator;
    { cval.size() } -> std::integral;
};
```

### Nested Requirements

Concepts can be composed from other concepts:

```cpp
template <typename T>
concept iterable = requires(T& val) {
    { std::begin(val) } -> std::input_or_output_iterator;
    { std::end(val) } -> std::input_or_output_iterator;
};

template <typename T>
concept sized = requires(const T& val) {
    { val.size() } -> std::integral;
};

template <typename T>
concept container = iterable<T> && sized<T>;
```

This decomposition serves two purposes. First, it's reusable—you can constrain on `iterable` alone when you don't need size. Second, it creates a subsumption hierarchy: `container` is "more constrained" than `iterable`, which matters for overload resolution.

---

## Two Headers, One Purpose

Fat-P provides two concept headers designed to work together while maintaining clean separation.

### Concepts.h: The Standalone Utility

`Concepts.h` provides general-purpose concepts for any C++20 codebase. Its critical property is **zero dependencies on other Fat-P headers** (except `CppFeatureDetection.h` for C++20 enforcement).

Why does this matter? Consider a game engine that wants `hashable` and `iterable` concepts but doesn't use any Fat-P containers. If `Concepts.h` included `SmallVector.h` and `FlatMap.h`, using one concept would compile the entire Fat-P container library.

Instead, `Concepts.h` includes only standard headers:

```cpp
#include <atomic>
#include <concepts>
#include <cstddef>
#include <functional>
#include <istream>
#include <iterator>
#include <ostream>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "CppFeatureDetection.h"  // Only Fat-P include
```

Every concept in `Concepts.h` is defined using only standard library facilities. You can use it as a completely standalone utility.

### FatPConcepts.h: Type Detection Without Coupling

`FatPConcepts.h` provides concepts for detecting Fat-P library types. The design challenge: how do you detect `SmallVector<int, 16>` without including `SmallVector.h`?

The answer is forward declarations. `FatPConcepts.h` declares the *shape* of Fat-P templates without their implementations:

```cpp
// Forward declaration only—no SmallVector.h included
template <typename T, std::size_t InlineCapacity, typename Allocator>
class SmallVector;

namespace detail {
template <typename T>
struct is_small_vector_impl : std::false_type {};

template <typename T, std::size_t N, typename A>
struct is_small_vector_impl<SmallVector<T, N, A>> : std::true_type {};
}

template <typename T>
concept small_vector_type = detail::is_small_vector_impl<T>::value;
```

Template specialization matching doesn't require a complete type definition. The compiler matches the specialization pattern against the argument's template structure without needing to see `SmallVector`'s members or methods.

This keeps compile times low. Using `small_vector_type<T>` doesn't compile `SmallVector.h`—it just checks whether `T` matches the `SmallVector<...>` pattern.

---

## Getting Started

### Include the Headers

For general-purpose concepts without Fat-P dependencies:

```cpp
#include "Concepts.h"
```

For Fat-P type detection:

```cpp
#include "FatPConcepts.h"
```

Both headers place their contents in `namespace fat_p::concepts`.

### Basic Usage Patterns

The most common use is constraining template parameters:

```cpp
// Constrain as type parameter
template <fat_p::concepts::container C>
void printSize(const C& c) {
    std::cout << "Size: " << c.size() << "\n";
}

// Constrain with requires clause
template <typename T>
    requires fat_p::concepts::iterable<T> && fat_p::concepts::sized<T>
void processContainer(const T& t) {
    for (const auto& elem : t) {
        // ...
    }
}

// Abbreviated function template (C++20)
void process(fat_p::concepts::container auto const& c) {
    // ...
}
```

You can also use concepts with `if constexpr` for compile-time branching:

```cpp
template <typename T>
std::string stringify(const T& val) {
    if constexpr (fat_p::concepts::streamable<T>) {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    } else if constexpr (fat_p::concepts::has_custom_string_method<T>) {
        return val.toString();
    } else {
        return "[unstringifiable]";
    }
}
```

The `if constexpr` branches are evaluated at compile time. Only the valid branch is compiled for each instantiation.

---

## The Container Detection Problem: What Does "Iterable" Mean?

### Beyond Member Functions

A naive approach to detecting iterability would check for `begin()` and `end()` member functions. But C++ supports iteration in multiple ways. C arrays don't have methods—they use non-member `std::begin()` and `std::end()`. Custom types might define free functions in their namespace that get found by argument-dependent lookup.

Fat-P's `iterable` concept handles all cases:

```cpp
template <typename T>
concept iterable = requires(T& val) {
    { std::begin(val) } -> std::input_or_output_iterator;
    { std::end(val) } -> std::input_or_output_iterator;
};
```

Using `std::begin(val)` instead of `val.begin()` invokes the standard library's `begin` function, which handles arrays, classes with member `begin()`, and ADL-found free functions uniformly.

This means `iterable` correctly accepts:

```cpp
static_assert(fat_p::concepts::iterable<std::vector<int>>);     // Member begin/end
static_assert(fat_p::concepts::iterable<int[5]>);               // C array
static_assert(fat_p::concepts::iterable<std::initializer_list<int>>);  // Special container
```

### The Sized vs Iterable Distinction

Not all iterable types have a `size()` method. The most notable example is `std::forward_list`, which deliberately omits `size()` because it would require O(n) traversal—violating the library's complexity guarantees.

Fat-P separates these properties:

```cpp
template <typename T>
concept sized = requires(const T& val) {
    { val.size() } -> std::integral;
};

template <typename T>
concept container = iterable<T> && sized<T>;
```

Use `iterable` when you only need to iterate. Use `container` when you also need size. Use `sized` alone for non-iterable types with size (if any exist in your codebase).

```cpp
static_assert(fat_p::concepts::iterable<std::forward_list<int>>);
static_assert(!fat_p::concepts::sized<std::forward_list<int>>);      // No size()!
static_assert(!fat_p::concepts::container<std::forward_list<int>>);
```

### Contiguous Memory Detection

Some algorithms can be optimized when they know data is contiguous in memory. SIMD operations, `memcpy` optimization, and direct pointer arithmetic all require contiguous storage.

The `contiguous_container` concept adds the `data()` requirement:

```cpp
template <typename T>
concept has_data = requires(T& val) {
    { val.data() } -> std::same_as<typename T::value_type*>;
};

template <typename T>
concept contiguous_container = container<T> && has_data<T>;
```

This creates a hierarchy: `contiguous_container` implies `container`, which implies `iterable` and `sized`.

```cpp
static_assert(fat_p::concepts::contiguous_container<std::vector<int>>);
static_assert(fat_p::concepts::contiguous_container<std::array<int, 5>>);
static_assert(fat_p::concepts::contiguous_container<std::string>);
static_assert(!fat_p::concepts::contiguous_container<std::list<int>>);   // No data()
static_assert(!fat_p::concepts::contiguous_container<std::deque<int>>);  // Not contiguous
```

---

## The Hashable Question: When Can You Hash?

### The std::hash Contract

C++ hash tables use `std::hash<Key>` to compute hash values. But `std::hash` is only specialized for certain types—built-in types, standard library types like `std::string`, and types for which users have provided specializations.

How do you know if `std::hash<T>` is valid? You can't just try to instantiate it—the primary template exists but may not compile:

```cpp
// Primary template exists but doesn't work
std::hash<MyCustomType>{}(value);  // Compile error if not specialized
```

Fat-P's `hashable` concept checks for a valid specialization:

```cpp
template <typename T>
concept hashable = requires(const T& val) {
    { std::hash<T>{}(val) } -> std::convertible_to<std::size_t>;
};
```

This requires that `std::hash<T>` can be default-constructed and called with a `const T&`, returning something convertible to `std::size_t`.

```cpp
static_assert(fat_p::concepts::hashable<int>);
static_assert(fat_p::concepts::hashable<std::string>);
static_assert(fat_p::concepts::hashable<std::string_view>);
static_assert(!fat_p::concepts::hashable<std::vector<int>>);  // No default std::hash
```

### Using Hashable for Map Keys

The primary use case is constraining hash map keys:

```cpp
template <fat_p::concepts::hashable Key, typename Value>
class SimpleHashMap {
    std::vector<std::optional<std::pair<Key, Value>>> buckets_;
    
    std::size_t hash_index(const Key& k) const {
        // This is guaranteed valid because Key satisfies hashable
        return std::hash<Key>{}(k) % buckets_.size();
    }
public:
    void insert(const Key& k, const Value& v) {
        std::size_t idx = hash_index(k);
        // ...
    }
};
```

Without the constraint, someone could instantiate `SimpleHashMap<std::vector<int>, int>`, and the error would appear deep inside `hash_index`. With the constraint, the error appears at instantiation with a clear message about `hashable`.

---

## The Callable Constraint: Type-Safe Callbacks

### Why Callable Concepts Matter

Generic code often accepts callbacks, event handlers, or function objects. Without constraints, errors appear when you try to invoke the callback, not when you register it:

```cpp
template <typename Handler>
void on_click(Handler h) {
    // ...later, in event loop...
    h(x, y);  // Error here if Handler isn't callable with (int, int)
}

on_click([](std::string s) { });  // Registers fine, crashes later
```

The `invocable` concept catches this at registration:

```cpp
template <typename Handler>
    requires fat_p::concepts::invocable<Handler, int, int>
void on_click(Handler h) {
    // ...
}

on_click([](std::string s) { });  // Error: constraint not satisfied
```

### Return Type Constraints

Sometimes you need to constrain not just that something is callable, but what it returns. A filter callback should return `bool`. A transform callback should return the transformed type.

```cpp
template <typename Filter>
    requires fat_p::concepts::invocable_r<bool, Filter, const Event&>
void set_event_filter(Filter f) {
    // f(event) is guaranteed to return bool
}

template <typename Transform>
    requires fat_p::concepts::invocable_r<std::string, Transform, int>
std::vector<std::string> transform_ids(std::span<int> ids, Transform t) {
    std::vector<std::string> result;
    for (int id : ids) {
        result.push_back(t(id));  // Guaranteed to return std::string
    }
    return result;
}
```

### Exception Specifications

For exception-safe code, `nothrow_invocable` checks that a callable is `noexcept`:

```cpp
template <typename Cleanup>
    requires fat_p::concepts::nothrow_invocable<Cleanup>
class ScopeGuard {
    Cleanup cleanup_;
public:
    ~ScopeGuard() noexcept {
        cleanup_();  // Safe to call in destructor—guaranteed noexcept
    }
};
```

---

## Stream and String Concepts: Universal Stringification

### The Stringification Hierarchy

Different types support different ways of converting to strings. Some have `operator<<` for streams. Some have a `toString()` method. Some have `to_string()` (snake_case variant). Generic stringification code needs to detect which mechanism is available.

Fat-P provides a hierarchy of concepts:

```cpp
// Stream insertion (operator<<)
template <typename T>
concept streamable = requires(std::ostream& os, const T& val) {
    { os << val } -> std::convertible_to<std::ostream&>;
};

// Custom toString() method (camelCase)
template <typename T>
concept has_to_string_method = requires(const T& val) {
    { val.toString() } -> std::convertible_to<std::string>;
};

// Custom to_string() method (snake_case)
template <typename T>
concept has_to_string_snake_method = requires(const T& val) {
    { val.to_string() } -> std::convertible_to<std::string>;
};

// Either custom method
template <typename T>
concept has_custom_string_method = has_to_string_method<T> || has_to_string_snake_method<T>;
```

You can build a universal stringification function that uses the best available mechanism:

```cpp
template <typename T>
std::string to_string(const T& val) {
    if constexpr (fat_p::concepts::streamable<T>) {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    } else if constexpr (fat_p::concepts::has_to_string_method<T>) {
        return val.toString();
    } else if constexpr (fat_p::concepts::has_to_string_snake_method<T>) {
        return val.to_string();
    } else {
        return "[unstringifiable]";
    }
}
```

### The Printable Range Problem

When stringifying containers, you want to print them element-by-element: `[1, 2, 3]`. But strings are ranges too—iterating over `std::string` gives individual characters. You don't want `"hello"` printed as `[h, e, l, l, o]`.

The `printable_range` concept excludes string types:

```cpp
template <typename T>
concept std_string_type = std::same_as<std::remove_cvref_t<T>, std::string> ||
                          std::same_as<std::remove_cvref_t<T>, std::string_view> ||
                          std::same_as<std::remove_cvref_t<T>, const char*> ||
                          std::same_as<std::remove_cvref_t<T>, char*>;

template <typename T>
concept printable_range = std::ranges::range<T> && (!std_string_type<T>);
```

Now container stringification can branch correctly:

```cpp
template <typename T>
std::string stringify(const T& val) {
    if constexpr (fat_p::concepts::std_string_type<T>) {
        return std::string(val);  // Print as string
    } else if constexpr (fat_p::concepts::printable_range<T>) {
        // Print as [elem, elem, elem]
        std::ostringstream oss;
        oss << "[";
        bool first = true;
        for (const auto& elem : val) {
            if (!first) oss << ", ";
            oss << stringify(elem);  // Recursive
            first = false;
        }
        oss << "]";
        return oss.str();
    } else if constexpr (fat_p::concepts::streamable<T>) {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    } else {
        return "?";
    }
}
```

---

## Fat-P Type Detection: The Forward Declaration Pattern

### Why Detect Fat-P Types?

Generic code over Fat-P containers sometimes needs to know *which* container it's working with. Different containers have different capabilities—`SmallVector` has `is_inline()` to check if storage is on the stack, `FlatMap` has `keys()` and `values()` views, `SlotMap` has generation-based handles.

You might want to write:

```cpp
template <typename Container>
void optimize_container(Container& c) {
    if constexpr (/* c is a SmallVector */) {
        if (c.is_inline()) {
            // Stack-allocated, use cache-friendly algorithm
        }
    } else if constexpr (/* c is a FlatMap */) {
        // Use sorted container optimizations
    }
}
```

Without type detection concepts, you'd need SFINAE traits for each Fat-P type—hundreds of lines of boilerplate.

### How Forward Declaration Detection Works

`FatPConcepts.h` declares Fat-P class templates without including their headers:

```cpp
// Forward declaration only
template <typename T, std::size_t InlineCapacity, typename Allocator>
class SmallVector;
```

Then it defines a detection trait using template specialization:

```cpp
namespace detail {
template <typename T>
struct is_small_vector_impl : std::false_type {};

// Specialization matches SmallVector instantiations
template <typename T, std::size_t N, typename A>
struct is_small_vector_impl<SmallVector<T, N, A>> : std::true_type {};
}

template <typename T>
concept small_vector_type = detail::is_small_vector_impl<T>::value;
```

When you write `small_vector_type<fat_p::SmallVector<int, 16>>`, the compiler:
1. Sees that `SmallVector<int, 16>` matches the pattern `SmallVector<T, N, A>` with `T=int`, `N=16`, `A=std::allocator<int>`
2. Selects the specialization, which inherits from `std::true_type`
3. Evaluates `::value` as `true`

The matching happens purely on template structure—the compiler doesn't need `SmallVector`'s definition.

### The Forward Declaration Contract

This pattern has a critical requirement: the forward declaration must exactly match the actual declaration. If `SmallVector` is declared as:

```cpp
template <typename T, std::size_t N, typename A = std::allocator<T>>
class SmallVector;
```

The forward declaration must have the same template parameters. A mismatch creates a *different* template that never matches actual `SmallVector` instantiations.

Fat-P's test suite verifies that forward declarations stay synchronized with implementations.

---

## Duck-Typed vs Exact-Match: Choosing Your Detection Strategy

### Exact-Match: Fat-P Types Only

The `*_type` concepts detect exact Fat-P types:

```cpp
template <typename T>
concept expected_type = detail::is_expected_impl<T>::value;
```

This matches only `fat_p::ExpectedImpl<...>`. It doesn't match `std::expected`, `tl::expected`, or any other implementation:

```cpp
static_assert(fat_p::concepts::expected_type<fat_p::Expected<int, Error>>);
static_assert(!fat_p::concepts::expected_type<std::expected<int, Error>>);  // C++23
```

Use exact-match when you need Fat-P-specific features like policy templates, extended methods, or internal structure.

### Duck-Typed: Interface Detection

The `*_like` concepts detect interface compatibility:

```cpp
template <typename T>
concept expected_like = requires(const T& val) {
    { val.has_value() } -> std::convertible_to<bool>;
    { val.value() };
    { val.error() };
};
```

This matches anything with the expected interface—Fat-P, std, tl::expected, or a custom implementation:

```cpp
static_assert(fat_p::concepts::expected_like<fat_p::Expected<int, Error>>);
static_assert(fat_p::concepts::expected_like<std::expected<int, Error>>);  // C++23
static_assert(fat_p::concepts::expected_like<tl::expected<int, Error>>);   // Third-party
```

Use duck-typed when you want generic algorithms that work with any conforming type.

### Choosing Your Strategy

The choice depends on your code's purpose:

**Use exact-match when:**
- You need Fat-P-specific methods not in the standard interface
- You're implementing Fat-P library internals
- Type identity matters for serialization, debugging, or logging

**Use duck-typed when:**
- You're writing generic algorithms
- You want to support multiple libraries
- Interface matters more than identity

A common pattern is to provide duck-typed public APIs with exact-match optimizations:

```cpp
template <fat_p::concepts::expected_like Result>
void handle_result(const Result& r) {
    if constexpr (fat_p::concepts::expected_type<Result>) {
        // Fat-P-specific optimization using internal knowledge
    } else {
        // Generic path for any expected-like type
    }
}
```

---

## Concept-Based Overloading: The Subsumption Hierarchy

### The Automatic Selection Problem

Consider two overloads for processing containers:

```cpp
template <fat_p::concepts::iterable Range>
void process(const Range& r) {
    for (const auto& elem : r) {
        // Generic iteration
    }
}

template <fat_p::concepts::contiguous_container C>
void process(const C& c) {
    const auto* data = c.data();
    // Optimized direct memory access
}
```

What happens when you call `process(std::vector<int>{})`? Both constraints are satisfied—`vector` is both `iterable` and `contiguous_container`. With SFINAE, this would be ambiguous.

But concepts use *subsumption*: the compiler recognizes that `contiguous_container` is "more constrained" than `iterable`. The more constrained overload wins automatically.

### How Subsumption Works

A concept A *subsumes* concept B if A's constraints logically imply B's constraints. In Fat-P's hierarchy:

```cpp
concept iterable = /* has begin/end */;
concept sized = /* has size */;
concept container = iterable && sized;
concept has_data = /* has data() */;
concept contiguous_container = container && has_data;
```

`contiguous_container` subsumes `container` because `contiguous_container`'s definition includes all of `container`'s requirements plus more. Similarly, `container` subsumes both `iterable` and `sized`.

The subsumption hierarchy:

```
iterable    sized
    \         /
     container
         |
         + has_data
         |
 contiguous_container
```

When multiple overloads match, the compiler selects the one with the most constrained (most derived in the subsumption hierarchy) concept.

### Breaking Subsumption

Subsumption only works when concepts share atomic constraints—the same `requires` expressions at the leaves. Two independently defined concepts don't subsume each other even if they check the same thing:

```cpp
// Library A
template <typename T>
concept has_begin_a = requires(T& t) { std::begin(t); };

// Library B
template <typename T>
concept has_begin_b = requires(T& t) { std::begin(t); };

// These are different atomic constraints!
// has_begin_a and has_begin_b don't subsume each other
```

Fat-P concepts are carefully designed with proper subsumption relationships. Use Fat-P concepts consistently rather than mixing with ad-hoc concepts to get correct overload resolution.

---

## When to Use Concepts (and When Not To)

### When to Use Concepts

**Template constraints that document intent.** Any template with implicit requirements should make them explicit:

```cpp
// Before: requirements hidden in implementation
template <typename Container>
void processItems(Container& c);

// After: requirements documented in interface
template <fat_p::concepts::container Container>
void processItems(Container& c);
```

**Compile-time dispatch based on capabilities.** When different implementations are optimal for different types:

```cpp
template <typename Range>
void fill_buffer(Range& r, int value) {
    if constexpr (fat_p::concepts::contiguous_container<Range>) {
        std::memset(r.data(), value, r.size() * sizeof(typename Range::value_type));
    } else {
        for (auto& elem : r) elem = value;
    }
}
```

**Overload sets with automatic selection.** When you want the compiler to pick the best implementation:

```cpp
template <fat_p::concepts::iterable R>
auto sum(const R& r);  // Generic

template <fat_p::concepts::contiguous_container C>
auto sum(const C& c);  // Optimized for contiguous
```

**Fat-P library integration.** When writing generic code over Fat-P containers.

### When Not to Use Concepts

**Single concrete type.** If a function only works with `std::vector<int>`, just use `std::vector<int>`. Concepts add value when multiple types should work.

```cpp
// Overkill
template <fat_p::concepts::container C>
void processSpecificVector(const C& c);  // Only ever called with vector<int>

// Better
void processSpecificVector(const std::vector<int>& c);
```

**Runtime polymorphism.** Concepts enable compile-time polymorphism—each type generates separate code. If you need runtime polymorphism (heterogeneous containers, plugin systems), use inheritance and virtual functions.

**Pre-C++20 codebases.** Concepts require C++20. For C++17, SFINAE traits remain the only option.

**Standard concepts suffice.** If `std::ranges::range` or `std::totally_ordered` does what you need, use the standard. Fat-P concepts complement, not replace, standard concepts.

---

## Migration from SFINAE

### Alternatives

Before migrating, consider these alternatives to Fat-P concepts:

- **`<concepts>` (C++20)** — Standard library concepts; limited to fundamentals like `std::integral`, `std::invocable`
- **Boost.TypeTraits** — SFINAE-based type introspection; works on C++11/14/17 but no concept syntax
- **Range-v3 concepts** — Comprehensive range concepts; heavier dependency
- **Custom SFINAE traits** — Full control but verbose and error-prone
- **Boost.Hana** — Metaprogramming library with type introspection; complex

Fat-P Concepts is the choice when you need C++20 concepts with comprehensive container detection, Fat-P type identification, and zero external dependencies.

### The Before Picture

```cpp
// SFINAE trait: 12 lines for one property
template <typename T, typename = void>
struct is_container : std::false_type {};

template <typename T>
struct is_container<T, std::void_t<
    decltype(std::begin(std::declval<T&>())),
    decltype(std::end(std::declval<T&>())),
    decltype(std::declval<const T&>().size())
>> : std::true_type {};

template <typename T>
inline constexpr bool is_container_v = is_container<T>::value;

// Usage
template <typename T, std::enable_if_t<is_container_v<T>, int> = 0>
void process(const T& c) {
    for (const auto& elem : c) {
        std::cout << elem << "\n";
    }
}
```

### The After Picture

```cpp
#include "Concepts.h"

// Usage (concept already defined in Concepts.h)
template <fat_p::concepts::container T>
void process(const T& c) {
    for (const auto& elem : c) {
        std::cout << elem << "\n";
    }
}
```

### Migration Steps

1. **Replace includes:**
   ```cpp
   // Old
   #include "TypeTraits.h"
   
   // New
   #include "Concepts.h"
   ```

2. **Replace `enable_if` with concept constraints:**
   ```cpp
   // Old
   template <typename T, std::enable_if_t<is_container_v<T>, int> = 0>
   
   // New
   template <fat_p::concepts::container T>
   ```

3. **Replace trait checks with concept checks:**
   ```cpp
   // Old
   if constexpr (is_reservable_v<Container>) { c.reserve(n); }
   
   // New
   if constexpr (fat_p::concepts::reservable<Container>) { c.reserve(n); }
   ```

4. **Replace static_assert with concept:**
   ```cpp
   // Old
   static_assert(is_hashable_v<Key>, "Key must be hashable");
   
   // New  
   static_assert(fat_p::concepts::hashable<Key>, "Key must be hashable");
   ```

### Common SFINAE to Concept Mappings

| SFINAE Pattern | Fat-P Concept |
|----------------|---------------|
| `std::void_t<decltype(val.begin())>` | `iterable<T>` |
| `std::void_t<decltype(val.size())>` | `sized<T>` |
| `std::is_invocable_v<F, Args...>` | `invocable<F, Args...>` |
| `std::void_t<decltype(std::hash<T>{}(val))>` | `hashable<T>` |
| `std::void_t<decltype(val == val)>` | `equality_comparable<T>` |

---

## Troubleshooting

### Compilation Error: "Constraints not satisfied"

**Symptom:** Compiler reports that a concept constraint isn't satisfied.

**Diagnosis:** The compiler tells you which concept failed and why. Read the nested notes—they show the hierarchy of unsatisfied requirements.

**Example:**
```
error: constraints not satisfied for 'void process(const T&)'
note: because 'int' does not satisfy 'container'
note: because 'int' does not satisfy 'iterable'  
note: because 'std::begin(val)' would be invalid
```

This tells you: `process` requires `container`, which requires `iterable`, which requires `std::begin(val)` to be valid. `int` doesn't have `begin()`.

**Fix:** Either use the correct type, or relax the constraint if your algorithm can work with simpler requirements.

### Compilation Error: "Ambiguous overload"

**Symptom:** Multiple concept-constrained overloads match equally well.

**Cause:** The concepts don't have a subsumption relationship.

**Example:**
```cpp
template <fat_p::concepts::iterable T> void f(T&);
template <fat_p::concepts::sized T> void f(T&);

f(std::vector<int>{});  // Ambiguous—vector is both iterable AND sized
```

**Fix:** Either make one concept subsume the other, or add explicit exclusion:

```cpp
// Option 1: Use a composed concept
template <fat_p::concepts::container T> void f(T&);  // container = iterable + sized

// Option 2: Explicit exclusion
template <typename T>
    requires fat_p::concepts::iterable<T> && (!fat_p::concepts::sized<T>)
void f(T&);
```

### Runtime: Unexpected overload selected

**Symptom:** The "wrong" overload is called despite constraints seeming correct.

**Diagnosis:** Check which concepts are actually satisfied:

```cpp
std::cout << std::boolalpha;
std::cout << "iterable: " << fat_p::concepts::iterable<MyType> << "\n";
std::cout << "container: " << fat_p::concepts::container<MyType> << "\n";
std::cout << "contiguous: " << fat_p::concepts::contiguous_container<MyType> << "\n";
```

**Common causes:**
- Type doesn't satisfy the expected concept (missing method, wrong return type)
- Concepts from different libraries don't subsume each other
- Template argument deduction chose a different overload

### Forward Declaration Mismatch

**Symptom:** `small_vector_type<SmallVector<int, 16>>` returns `false` despite obviously being a SmallVector.

**Cause:** The forward declaration in `FatPConcepts.h` doesn't match the actual declaration in `SmallVector.h`.

**Fix:** This indicates a Fat-P bug. Report it—forward declarations must stay synchronized with implementations.

---

## API Reference

### Container Concepts (Concepts.h)

| Concept | Requirement |
|---------|-------------|
| `iterable<T>` | `std::begin(val)` and `std::end(val)` return iterators |
| `sized<T>` | `val.size()` returns integral |
| `container<T>` | `iterable<T> && sized<T>` |
| `has_data<T>` | `val.data()` returns pointer to `value_type` |
| `contiguous_container<T>` | `container<T> && has_data<T>` |
| `reservable<T>` | `val.reserve(n)` is valid |
| `has_clear<T>` | `val.clear()` is valid |
| `has_push_back<T>` | `val.push_back(elem)` is valid |
| `has_emplace_back<T>` | `val.emplace_back()` is valid |
| `random_accessible<T>` | `val[n]` and `val.size()` are valid |
| `map_like<T>` | Iterable with `key_type`, `mapped_type`, and pair `value_type` |

### Comparison Concepts (Concepts.h)

| Concept | Requirement |
|---------|-------------|
| `hashable<T>` | `std::hash<T>{}(val)` returns size_t |
| `equality_comparable<T>` | `a == b` returns bool-like |
| `totally_ordered<T>` | `<`, `<=`, `>`, `>=` all return bool-like |
| `three_way_comparable<T>` | `a <=> b` is valid |
| `transparent<T>` | Has `is_transparent` member type |

### Callable Concepts (Concepts.h)

| Concept | Requirement |
|---------|-------------|
| `invocable<F, Args...>` | `f(args...)` is valid |
| `invocable_r<R, F, Args...>` | `f(args...)` returns `R` |
| `nothrow_invocable<F, Args...>` | `f(args...)` is noexcept |
| `function_object<T>` | Has `operator()` |

### Stream Concepts (Concepts.h)

| Concept | Requirement |
|---------|-------------|
| `streamable<T>` | `os << val` is valid for ostream |
| `input_streamable<T>` | `is >> val` is valid for istream |
| `has_to_string_method<T>` | `val.toString()` returns string |
| `has_to_string_snake_method<T>` | `val.to_string()` returns string |
| `printable_range<T>` | Range but not string type |

### Fat-P Type Detection (FatPConcepts.h)

| Concept | Detects |
|---------|---------|
| `small_vector_type<T>` | `fat_p::SmallVector<...>` |
| `flat_map_type<T>` | `fat_p::FlatMap<...>` |
| `flat_set_type<T>` | `fat_p::FlatSet<...>` |
| `slot_map_type<T>` | `fat_p::SlotMap<...>` |
| `expected_type<T>` | `fat_p::ExpectedImpl<...>` |
| `strong_id_type<T>` | `fat_p::StrongId<...>` |
| `tensor_type<T>` | `fat_p::Tensor<...>` |
| `lock_free_queue_type<T>` | `fat_p::LockFreeQueue<...>` etc. |
| `library_container<T>` | Any Fat-P container type |

### Duck-Typed Concepts (FatPConcepts.h)

| Concept | Requirement |
|---------|-------------|
| `expected_like<T>` | Has `has_value()`, `value()`, `error()` |
| `optional_like<T>` | Has `has_value()`, `value()` |
| `tensor_like<T>` | Has `shape()` |
| `small_vector_like<T>` | Has `is_inline()` |

---

## Summary

Fat-P's concept headers solve the type introspection problem that has plagued C++ generic programming for decades. `Concepts.h` provides general-purpose concepts with zero Fat-P dependencies—container detection, callable constraints, comparison concepts, and stream concepts that work in any C++20 codebase. `FatPConcepts.h` adds Fat-P type detection using forward declarations to avoid header coupling.

The key benefits over SFINAE:
- **Clear error messages** that identify which constraint failed
- **Self-documenting templates** with requirements in the signature
- **Automatic overload ranking** via concept subsumption
- **Dramatically less boilerplate** than SFINAE traits

All concepts are compile-time only with zero runtime overhead.

---

## Read Next

- **[Overview - Concepts](Overview_-_Concepts.md)** — Executive summary and positioning
- **[Companion Guide - Concepts](Companion_Guide_-_Concepts.md)** — Design rationale and advanced patterns
- **test_Concepts.cpp** — Test suite demonstrating all concepts
- **test_FatPConcepts.cpp** — Fat-P type detection tests

---

*Concepts.h, FatPConcepts.h — Fat-P Library v3.2*
