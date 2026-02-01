---
doc_id: CG-CONCEPTS-001
doc_type: "Companion Guide"
title: "Concepts and FatPConcepts"
fatp_components: ["Concepts", "FatPConcepts"]
topics: ["C++20 concepts design", "concept architecture", "requires expressions", "constraint normalization", "concept subsumption", "SFINAE replacement", "forward declaration patterns"]
constraints: ["compile-time evaluation", "concept subsumption rules", "dependent name lookup", "template instantiation"]
cxx_standard: "C++20"
std_equivalent: "std::integral, std::invocable, std::ranges::range (partial)"
std_since: "C++20"
boost_equivalent: "Boost.TypeTraits (SFINAE-based, not C++20 concepts)"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-30"
audience: ["C++ developers", "library maintainers", "template library authors", "AI assistants"]
status: "reviewed"
---

# **The Constraint System**

### *A Companion Guide to Fat-P's Concepts and FatPConcepts*

---

**Scope:** This guide covers the design philosophy, architectural decisions, and implementation rationale behind Fat-P's concept headers. It explains why concepts matter for generic programming, how Fat-P's concepts are organized, the forward-declaration pattern for type detection, and the tradeoffs involved in concept design.

**Not covered:**
- API reference and basic usage (see User Manual - Concepts)
- C++20 concepts tutorial (see cppreference.com)
- SFINAE mechanics (see Foundations - Template Metaprogramming)

**Prerequisites:**
- Working knowledge of C++ templates and generic programming
- Basic understanding of C++20 concepts syntax
- Familiarity with SFINAE and `std::enable_if`

---

## Companion Guide Card

**Component:** Concepts, FatPConcepts  
**Design question:** How do you provide comprehensive type introspection without header coupling?  
**Key tradeoff:** Completeness vs. compile-time cost  
**Decision made:** Two-header split with forward declarations for Fat-P types  
**Rejected alternatives:** Single monolithic header; runtime type detection; full header includes in FatPConcepts  
**Historical context:** Evolution from SFINAE traits (TypeTraits.h, FatPTypeTraits.h) to C++20 concepts

---

# **Table of Contents**

**[Introduction: Why This Component Exists](#introduction-why-this-component-exists)**

## Part I — The Problems

1. [The SFINAE Era](#chapter-1--the-sfinae-era)
2. [The Error Message Problem](#chapter-2--the-error-message-problem)
3. [The Overload Resolution Problem](#chapter-3--the-overload-resolution-problem)
4. [The Documentation Gap](#chapter-4--the-documentation-gap)
5. [The Header Coupling Problem](#chapter-5--the-header-coupling-problem)

## Part II — The Solutions

6. [Architecture Overview](#chapter-6--architecture-overview)
7. [The Zero-Dependency Principle](#chapter-7--the-zero-dependency-principle)
8. [The Forward Declaration Pattern](#chapter-8--the-forward-declaration-pattern)
9. [Concept Naming Conventions](#chapter-9--concept-naming-conventions)
10. [Requires Expression Design](#chapter-10--requires-expression-design)
11. [Subsumption and Overload Ranking](#chapter-11--subsumption-and-overload-ranking)
12. [Duck-Typed vs. Exact-Match Concepts](#chapter-12--duck-typed-vs-exact-match-concepts)

## Part III — Foundations

- [Appendix A — Migration from TypeTraits/FatPTypeTraits](#appendix-a--migration-from-typetraitsfatptypetraits)
- [Appendix B — Design Constraints and Rejected Alternatives](#appendix-b--design-constraints-and-rejected-alternatives)
- [Appendix C — Where Concepts Lose](#appendix-c--where-concepts-lose)
- [Appendix D — The Standard Concepts Gap](#appendix-d--the-standard-concepts-gap)
- [Appendix E — Further Reading](#appendix-e--further-reading)

---

# **Introduction: Why This Component Exists**

You're writing a generic algorithm that processes containers:

```cpp
template <typename Container>
auto sum(const Container& c) {
    typename Container::value_type total{};
    for (const auto& elem : c) {
        total += elem;
    }
    return total;
}
```

This works perfectly for `std::vector<int>`. But what happens when someone passes `int`?

The compiler produces an error—but the error points to `typename Container::value_type`, deep inside your implementation. The user sees your internal logic when they should see a clear statement: "This function requires a container."

Or consider this: you want to optimize for contiguous containers where you can use `data()` for direct memory access:

```cpp
template <typename Container>
auto sum(const Container& c) {
    // How do I detect if c.data() exists?
    // How do I choose the best overload automatically?
}
```

With SFINAE, you'd write traits, primary templates, specializations, and carefully ordered overloads. With concepts, you write:

```cpp
template <fat_p::concepts::iterable Range>
auto sum(const Range& r);  // Generic

template <fat_p::concepts::contiguous_container C>
auto sum(const C& c);  // Optimized (automatically preferred)
```

The compiler selects the more constrained overload when both match. No disambiguation needed.

Or consider Fat-P-specific code. You want a function that accepts any Fat-P container:

```cpp
template <typename T>
void processFatPContainer(T& c) {
    // Is T a SmallVector? A FlatMap? A SlotMap?
    // Without concepts, you need SFINAE traits for each type.
}
```

With `FatPConcepts.h`:

```cpp
template <fat_p::concepts::library_container T>
void processFatPContainer(T& c);  // Works with any Fat-P container
```

Fat-P's concept headers exist for engineers who need:

- **Clear template constraints** that document requirements in signatures
- **Readable error messages** that identify which constraint failed
- **Automatic overload ranking** based on constraint specificity
- **Fat-P type detection** without coupling to implementation headers
- **Zero runtime overhead** (all checks are compile-time)

This guide explains the design decisions behind these headers.

---

# **PART I — THE PROBLEMS**

Before C++20 concepts, generic programming in C++ required SFINAE—a technique that worked but imposed significant costs. Understanding these costs explains why concepts exist and why Fat-P's concept headers are designed the way they are.

---

# **CHAPTER 1 — The SFINAE Era**

SFINAE (Substitution Failure Is Not An Error) exploits a rule in C++ template instantiation: when substituting template arguments causes an error, that candidate is silently removed from overload resolution rather than causing a compile error.

### How SFINAE Traits Work

To detect if a type has a `size()` method, you'd write:

```cpp
// Primary template: T does not have size()
template <typename T, typename = void>
struct has_size : std::false_type {};

// Specialization: T has size() (SFINAE-friendly)
template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

// Convenience alias
template <typename T>
inline constexpr bool has_size_v = has_size<T>::value;
```

The `std::void_t` trick creates a dependent type that's always `void` if the expression inside it is well-formed. When `T().size()` is invalid, the specialization's SFINAE context fails, and the primary template is selected.

### The Boilerplate Explosion

Every type property requires this pattern. A comprehensive type traits library needs:

- Container traits: `has_size`, `has_begin`, `has_end`, `has_data`, `has_reserve`, `has_push_back`, `has_clear`, `has_empty`...
- Comparison traits: `is_hashable`, `is_equality_comparable`, `is_less_than_comparable`...
- Callable traits: `is_invocable`, `is_invocable_r`, `is_nothrow_invocable`...
- Stream traits: `is_streamable`, `is_input_streamable`...

Each trait requires 8-12 lines. A library with 30 traits needs 300+ lines of nearly identical code.

### The Maintenance Burden

SFINAE traits are fragile. Consider:

```cpp
template <typename T>
struct has_push_back<T, std::void_t<
    decltype(std::declval<T>().push_back(std::declval<typename T::value_type>()))
>> : std::true_type {};
```

This works for `std::vector<int>` but fails for `std::vector<MoveOnly>` because the expression requires a copyable value. The fix:

```cpp
template <typename T>
struct has_push_back<T, std::void_t<
    decltype(std::declval<T>().push_back(std::declval<typename T::value_type&>()))
>> : std::true_type {};
```

Or maybe you want to detect `push_back` that accepts any argument? The "correct" SFINAE expression depends on subtle semantic choices that aren't captured in the trait's name.

---

# **CHAPTER 2 — The Error Message Problem**

SFINAE's worst offense is error messages. When a template fails to instantiate, the compiler walks through each overload, shows each SFINAE failure, and eventually reports the actual error deep in implementation details.

### A Simple Example Gone Wrong

```cpp
template <typename T, std::enable_if_t<has_size_v<T>, int> = 0>
void process(const T& container) {
    for (const auto& elem : container) {
        // ...
    }
}

process(42);  // Error!
```

The compiler output (simplified from actual GCC output):

```
error: no matching function for call to 'process(int)'
note: candidate: template<class T, std::enable_if_t<has_size_v<T>, int> <anonymous> >
      void process(const T&)
note:   template argument deduction/substitution failed:
note:   couldn't deduce template parameter '<anonymous>'
```

The user sees `enable_if_t`, `has_size_v`, and `<anonymous>`. They don't see "process requires a container."

### The Concept Alternative

```cpp
template <fat_p::concepts::container T>
void process(const T& c);

process(42);  // Error!
```

Compiler output:

```
error: constraints not satisfied for function template 'process'
note: because 'int' does not satisfy 'container'
note: because 'int' does not satisfy 'iterable'
note: because 'std::begin(val)' would be invalid
```

The user sees exactly what's required (`container`), what's missing (`iterable`), and why (`no begin()`).

---

# **CHAPTER 3 — The Overload Resolution Problem**

With SFINAE, overload selection requires careful ordering. Consider two overloads:

```cpp
template <typename T, std::enable_if_t<is_iterable_v<T>, int> = 0>
void process(const T& t);  // Version A

template <typename T, std::enable_if_t<is_contiguous_v<T>, int> = 0>
void process(const T& t);  // Version B
```

What happens when both constraints are satisfied (e.g., `std::vector<int>`)?

Ambiguous call. The compiler can't decide. You need to manually add exclusion:

```cpp
template <typename T, std::enable_if_t<is_iterable_v<T> && !is_contiguous_v<T>, int> = 0>
void process(const T& t);  // Version A (only if not contiguous)

template <typename T, std::enable_if_t<is_contiguous_v<T>, int> = 0>
void process(const T& t);  // Version B
```

This is fragile. Adding a third overload requires updating all existing constraints.

### Concept Subsumption

C++20 concepts solve this with subsumption: a more constrained overload is preferred over a less constrained one when both match.

```cpp
template <fat_p::concepts::iterable T>
void process(const T& t);  // Less constrained

template <fat_p::concepts::contiguous_container T>
void process(const T& t);  // More constrained (contiguous implies iterable)
```

When both match, the compiler automatically selects `contiguous_container` because it subsumes `iterable`. No manual exclusion needed.

---

# **CHAPTER 4 — The Documentation Gap**

SFINAE traits exist in implementation but not in interfaces. A function signature like:

```cpp
template <typename T, std::enable_if_t<
    has_begin_v<T> && has_end_v<T> && has_size_v<T>, int> = 0>
void process(const T& container);
```

Documents its requirements, but the documentation is buried in template noise. Compare:

```cpp
template <fat_p::concepts::container T>
void process(const T& container);
```

The concept name is the documentation. `container` communicates intent immediately.

### IDE Support

Modern IDEs show concept names in autocomplete and hover documentation. They can expand concept definitions to show requirements. SFINAE traits appear as cryptic `enable_if` expressions that IDEs can't meaningfully summarize.

---

# **CHAPTER 5 — The Header Coupling Problem**

Fat-P's type traits needed to detect Fat-P types like `SmallVector`, `FlatMap`, and `Expected`. The naive approach:

```cpp
// FatPTypeTraits.h (hypothetical bad design)
#include "SmallVector.h"
#include "FlatMap.h"
#include "Expected.h"
// ... 40 more includes

template <typename T>
struct is_small_vector : std::false_type {};

template <typename T, std::size_t N, typename A>
struct is_small_vector<SmallVector<T, N, A>> : std::true_type {};
```

This forces anyone using `FatPTypeTraits.h` to compile every Fat-P header. A simple type check pulls in the entire library.

### The Compile-Time Cost

Fat-P headers are not trivial. `SmallVector.h` alone includes allocator machinery, iterator definitions, exception safety code. Multiply by 40 headers, and "type detection" becomes "compile the world."

### The Forward Declaration Solution

`FatPConcepts.h` uses forward declarations instead of includes:

```cpp
// Forward declare without including
template <typename T, std::size_t InlineCapacity, typename Allocator>
class SmallVector;

// Detection works with forward declaration
template <typename T>
concept small_vector_type = /* ... */;
```

The full `SmallVector.h` is never included. Type detection has near-zero compile-time cost.

---

# **PART II — THE SOLUTIONS**

---

# **CHAPTER 6 — Architecture Overview**

Fat-P provides two concept headers with distinct roles:

### Concepts.h — General Purpose

```cpp
namespace fat_p::concepts {
    // Container concepts
    concept iterable = ...;
    concept sized = ...;
    concept container = ...;
    concept contiguous_container = ...;
    // ...
    
    // Comparison concepts
    concept hashable = ...;
    concept equality_comparable = ...;
    // ...
    
    // Callable concepts
    concept invocable = ...;
    // ...
}
```

**Dependencies:** Only `CppFeatureDetection.h` and standard library headers.

**Use case:** Any C++20 project needing type introspection. Standalone utility.

### FatPConcepts.h — Fat-P Type Detection

```cpp
namespace fat_p::concepts {
    // Fat-P container detection
    concept small_vector_type = ...;
    concept flat_map_type = ...;
    concept slot_map_type = ...;
    // ...
    
    // Composite concepts
    concept library_container = ...;
    concept any_tensor_type = ...;
}
```

**Dependencies:** `CppFeatureDetection.h` plus forward declarations of Fat-P types.

**Use case:** Generic code over Fat-P library types.

---

# **CHAPTER 7 — The Zero-Dependency Principle**

`Concepts.h` has a critical design constraint: **no dependencies on other Fat-P headers** (except `CppFeatureDetection.h` for C++20 enforcement).

### Why This Matters

Users might want Fat-P's general concepts without using any other Fat-P component. A game engine might use `hashable` and `iterable` without ever touching `SmallVector` or `Expected`.

If `Concepts.h` included Fat-P headers, using one concept would drag in unrelated code.

### The Implementation

```cpp
// Concepts.h includes only standard headers
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

#include "CppFeatureDetection.h"  // Only Fat-P dependency
```

Every concept in `Concepts.h` is defined using only standard library facilities.

---

# **CHAPTER 8 — The Forward Declaration Pattern**

`FatPConcepts.h` detects Fat-P types without including their headers through forward declarations and template specialization.

### The Pattern

```cpp
// Forward declare the class template
template <typename T, std::size_t InlineCapacity, typename Allocator>
class SmallVector;

namespace concepts::detail {

// Primary template: not a SmallVector
template <typename T>
struct is_small_vector_impl : std::false_type {};

// Specialization: matches SmallVector instantiations
template <typename T, std::size_t N, typename A>
struct is_small_vector_impl<SmallVector<T, N, A>> : std::true_type {};

}

// Concept using the trait
template <typename T>
concept small_vector_type = detail::is_small_vector_impl<T>::value;
```

### Why This Works

Template specialization doesn't require a complete type. The compiler matches the specialization pattern against the argument's template structure without needing to see `SmallVector`'s definition.

### The Critical Constraint

The forward declaration **must exactly match** the real declaration. If `SmallVector` is declared as:

```cpp
template <typename T, std::size_t N, typename A = std::allocator<T>>
class SmallVector;
```

Then the forward declaration must have the same parameters. A mismatch creates a different template that never matches.

---

# **CHAPTER 9 — Concept Naming Conventions**

Fat-P concepts follow consistent naming patterns:

### Capability Concepts: Verb Form

Concepts that detect capability use the capability name:

```cpp
concept iterable = ...;         // Can iterate
concept reservable = ...;       // Can reserve
concept hashable = ...;         // Can hash
concept streamable = ...;       // Can stream
concept serializable = ...;     // Can serialize
```

### Property Concepts: Adjective Form

Concepts that detect properties use adjectives:

```cpp
concept sized = ...;                    // Has size
concept transparent = ...;              // Has is_transparent
concept equality_comparable = ...;      // Supports ==
concept totally_ordered = ...;          // Supports <, <=, >, >=
```

### Type Detection Concepts: `*_type` Suffix

Concepts that detect specific types use the `_type` suffix:

```cpp
concept small_vector_type = ...;        // Is a SmallVector
concept flat_map_type = ...;            // Is a FlatMap
concept atomic_type = ...;              // Is std::atomic<U>
concept scoped_enum = ...;              // Is enum class (exception: common term)
```

### Duck-Typed Concepts: `*_like` Suffix

Concepts that detect interface (not exact type) use the `_like` suffix:

```cpp
concept expected_like = ...;            // Has value() and error()
concept optional_like = ...;            // Has has_value() and value()
concept tensor_like = ...;              // Has shape()
concept small_vector_like = ...;        // Has is_inline()
```

### Method Detection: `has_*` Prefix

Concepts that detect a specific method use `has_` prefix:

```cpp
concept has_push_back = ...;
concept has_emplace_back = ...;
concept has_clear = ...;
concept has_reserve = ...;
concept has_data = ...;
```

---

# **CHAPTER 10 — Requires Expression Design**

Each concept uses a `requires` expression to specify constraints. The design balances completeness against false positives.

### Simple Method Detection

```cpp
template <typename T>
concept has_clear = requires(T& val) {
    { val.clear() };
};
```

This checks only that `val.clear()` is a valid expression. It doesn't constrain the return type.

### Constrained Return Type

```cpp
template <typename T>
concept sized = requires(const T& val) {
    { val.size() } -> std::integral;
};
```

The `-> std::integral` syntax requires `val.size()` to return something convertible to an integral type.

### Iterator Requirements

```cpp
template <typename T>
concept iterable = requires(T& val) {
    { std::begin(val) } -> std::input_or_output_iterator;
    { std::end(val) } -> std::input_or_output_iterator;
};
```

Using `std::begin`/`std::end` (not `val.begin()`) enables detection of C arrays and types with non-member `begin`/`end`.

### Nested Requirements

```cpp
template <typename T>
concept container = iterable<T> && sized<T>;
```

Concepts can be composed. `container` requires both `iterable` and `sized`.

### Complex Type Requirements

```cpp
template <typename T>
concept map_like = iterable<T> &&
    requires {
        typename T::key_type;
        typename T::mapped_type;
    } &&
    detail::has_pair_value_type<T>;
```

This combines iteration (runtime capability), member type existence (compile-time property), and value type structure (pair detection).

---

# **CHAPTER 11 — Subsumption and Overload Ranking**

C++20 concepts participate in overload resolution through subsumption: a concept A subsumes concept B if A's constraints logically imply B's constraints.

### How Subsumption Works

```cpp
template <typename T>
concept iterable = requires(T& val) {
    { std::begin(val) };
    { std::end(val) };
};

template <typename T>
concept container = iterable<T> && requires(const T& val) {
    { val.size() };
};
```

`container` subsumes `iterable` because `container` includes all of `iterable`'s requirements plus more.

### Automatic Overload Selection

```cpp
template <iterable T>
void process(const T& t) { /* generic */ }

template <container T>
void process(const T& t) { /* container-specific */ }

std::vector<int> v;
std::forward_list<int> fl;

process(v);   // Calls container overload (more constrained)
process(fl);  // Calls iterable overload (forward_list has no size())
```

### Subsumption Chains

Fat-P concepts form subsumption hierarchies:

```
iterable
    ↓
container (iterable + sized)
    ↓
contiguous_container (container + has_data)
```

An overload constrained with `contiguous_container` is preferred over `container`, which is preferred over `iterable`.

### Breaking Subsumption

Subsumption only works for concepts that share atomic constraints. Two independently defined concepts don't subsume each other:

```cpp
// These don't subsume each other!
template <typename T>
concept has_begin = requires(T& t) { std::begin(t); };

template <typename T>
concept has_end = requires(T& t) { std::end(t); };

template <typename T>
concept iterable_v2 = has_begin<T> && has_end<T>;

// iterable (original) and iterable_v2 don't subsume each other
// even though they check the same thing
```

Fat-P concepts are designed with explicit subsumption relationships.

---

# **CHAPTER 12 — Duck-Typed vs. Exact-Match Concepts**

Fat-P provides two styles of type detection:

### Exact-Match Concepts

Detect specific Fat-P types:

```cpp
template <typename T>
concept expected_type = detail::is_expected_impl<T>::value;

// Only matches fat_p::ExpectedImpl
static_assert(expected_type<fat_p::Expected<int, Error>>);
static_assert(!expected_type<std::expected<int, Error>>);  // C++23
static_assert(!expected_type<tl::expected<int, Error>>);   // Third-party
```

**Use case:** Code that relies on Fat-P-specific implementation details.

### Duck-Typed Concepts

Detect interface, not identity:

```cpp
template <typename T>
concept expected_like = requires(const T& val) {
    { val.has_value() } -> std::convertible_to<bool>;
    { val.value() };
    { val.error() };
};

// Matches anything with the expected interface
static_assert(expected_like<fat_p::Expected<int, Error>>);
static_assert(expected_like<std::expected<int, Error>>);   // C++23
static_assert(expected_like<tl::expected<int, Error>>);    // Third-party
```

**Use case:** Generic code that should work with any library's implementation.

### Choosing Between Styles

| Scenario | Use |
|----------|-----|
| Need Fat-P-specific features | Exact-match (`expected_type`) |
| Interoperability with other libraries | Duck-typed (`expected_like`) |
| Type identification for dispatch | Exact-match |
| Generic algorithms | Duck-typed |

---

# **PART III — FOUNDATIONS**

---

# **APPENDIX A — Migration from TypeTraits/FatPTypeTraits**

Fat-P's concept headers replace the older SFINAE-based `TypeTraits.h` and `FatPTypeTraits.h`. This appendix maps old traits to new concepts.

### Trait-to-Concept Mapping

| Old Trait (TypeTraits.h) | New Concept (Concepts.h) |
|--------------------------|--------------------------|
| `is_iterable<T>` | `iterable<T>` |
| `is_sized<T>` | `sized<T>` |
| `is_container<T>` | `container<T>` |
| `is_hashable<T>` | `hashable<T>` |
| `is_equality_comparable<T>` | `equality_comparable<T>` |
| `is_streamable<T>` | `streamable<T>` |
| `is_invocable<F, Args...>` | `invocable<F, Args...>` |
| `has_push_back<T>` | `has_push_back<T>` |
| `has_reserve<T>` | `reservable<T>` |

| Old Trait (FatPTypeTraits.h) | New Concept (FatPConcepts.h) |
|------------------------------|------------------------------|
| `is_small_vector<T>` | `small_vector_type<T>` |
| `is_flat_map<T>` | `flat_map_type<T>` |
| `is_expected<T>` | `expected_type<T>` |
| `is_strong_id<T>` | `strong_id_type<T>` |
| `is_tensor<T>` | `tensor_type<T>` |
| `is_lock_free_queue<T>` | `lock_free_queue_type<T>` |

### Migration Steps

**Step 1: Replace includes**

```cpp
// Old
#include "TypeTraits.h"
#include "FatPTypeTraits.h"

// New
#include "Concepts.h"
#include "FatPConcepts.h"
```

**Step 2: Replace enable_if with concept constraints**

```cpp
// Old
template <typename T, std::enable_if_t<fat_p::is_container_v<T>, int> = 0>
void process(const T& c);

// New
template <fat_p::concepts::container T>
void process(const T& c);
```

**Step 3: Replace trait checks in if constexpr**

```cpp
// Old
if constexpr (fat_p::is_reservable_v<Container>) {
    c.reserve(n);
}

// New
if constexpr (fat_p::concepts::reservable<Container>) {
    c.reserve(n);
}
```

**Step 4: Replace static_assert with trait**

```cpp
// Old
static_assert(fat_p::is_hashable_v<Key>, "Key must be hashable");

// New
static_assert(fat_p::concepts::hashable<Key>, "Key must be hashable");
```

---

# **APPENDIX B — Design Constraints and Rejected Alternatives**

### Hard Constraints

1. **C++20 minimum.** Concepts require C++20. No C++17 compatibility layer.
2. **Zero runtime overhead.** All checks compile-time only.
3. **Concepts.h standalone.** No Fat-P dependencies except feature detection.
4. **Forward declarations only in FatPConcepts.h.** No full header includes.

### Rejected Alternatives

| Alternative | Why Rejected |
|-------------|--------------|
| Single combined header | Forced users to include Fat-P type detection machinery when only wanting general concepts |
| Runtime type detection | RTTI overhead; concepts are compile-time by design |
| Full includes in FatPConcepts.h | Compile-time explosion; every concept check includes the entire library |
| Macros for C++17 compatibility | Macros can't replicate concept subsumption or error messages |
| Boost.Hana for detection | External dependency; Fat-P is header-only with no external deps |

### Accepted Trade-offs

| Trade-off | Rationale |
|-----------|-----------|
| Requires C++20 | Concepts are a C++20 feature; no way around this |
| Forward declaration maintenance | Must keep forward declarations in sync with actual headers; automated testing catches mismatches |
| Limited duck-typing | Can't detect private methods or implementation details |
| No runtime concept checks | Consistent with C++ compile-time philosophy |

---

# **APPENDIX C — Where Concepts Lose**

Fat-P's concept headers are not universally optimal:

### Pre-C++20 Codebases

Concepts require C++20. For C++17 projects, SFINAE traits remain the only option. Fat-P doesn't provide a C++17 compatibility layer.

### Runtime Polymorphism

Concepts enable compile-time polymorphism—each concrete type generates separate code. For runtime polymorphism (heterogeneous collections, plugin systems), use inheritance and virtual functions.

### Very Simple Templates

If your template has one concrete use case, concepts add ceremony without benefit:

```cpp
// Overkill—just use the concrete type
template <fat_p::concepts::container T>
void processInts(const T& c);  // Only ever called with vector<int>

// Better
void processInts(const std::vector<int>& c);
```

### Debugging Template Errors

**Hypothesis:** While concepts improve error messages, they can also hide useful instantiation context. Sometimes the old "wall of template errors" reveals more about what went wrong. (This is situational and depends on the specific error; no systematic comparison has been conducted.)

### Concept Overhead in Build Systems

**Hypothesis:** Each concept check has (small) compile-time cost. A template instantiated thousands of times multiplies this cost. For extremely hot compilation paths, manual optimizations might be needed. (This has not been measured in Fat-P's context; actual impact depends on compiler and codebase structure.)

---

# **APPENDIX D — The Standard Concepts Gap**

C++20 `<concepts>` provides fundamental concepts but leaves gaps that Fat-P fills:

### What `<concepts>` Provides

| Concept | Purpose |
|---------|---------|
| `std::same_as` | Exact type match |
| `std::derived_from` | Inheritance check |
| `std::convertible_to` | Implicit conversion |
| `std::integral` | Integer types |
| `std::floating_point` | Float types |
| `std::invocable` | Callable check |
| `std::regular` | Copyable + equality |
| `std::totally_ordered` | Full comparison |

### What `<concepts>` Lacks

| Need | Status | Fat-P |
|------|--------|-------|
| `hashable` | Not provided | ✓ |
| `streamable` | Not provided | ✓ |
| Container capability detection | Only `ranges::range` | ✓ Full suite |
| Method existence (`has_push_back`) | Not provided | ✓ |
| Custom string method detection | Not provided | ✓ |
| Library type detection | Impossible | ✓ |

### Why the Standard Stays Minimal

The committee avoids domain-specific concepts. `hashable` requires a position on what "hashable" means (must `std::hash` exist? or any callable?). Different libraries have different requirements.

Fat-P makes specific decisions for its domain. `hashable` means `std::hash<T>` exists. This is opinionated but practical.

---

# **APPENDIX E — Further Reading**

**C++20 Concepts (cppreference)**  
https://en.cppreference.com/w/cpp/language/constraints

**CppCon 2018: "Concepts: The Future of Generic Programming"**  
Bjarne Stroustrup  
https://www.youtube.com/watch?v=HddFGPTAmtU

**CppCon 2019: "Concepts in 60: Everything You Need to Know"**  
Andrew Sutton

**"A Concept Design for the STL"**  
Bjarne Stroustrup, Andrew Sutton  
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3351.pdf

**Ranges and Concepts in C++20**  
Eric Niebler, Casey Carter  
https://ericniebler.github.io/range-v3/

**Fat-P Development Guidelines**  
Section 5.3 (Naming Conventions), Section 2 (Layer System)

---

*End of Companion Guide*

*Concepts.h, FatPConcepts.h — Fat-P Library v3.2*
