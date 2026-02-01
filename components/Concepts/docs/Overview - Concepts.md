---
doc_id: OV-CONCEPTS-001
doc_type: "Overview"
title: "Concepts and FatPConcepts"
fatp_components: ["Concepts", "FatPConcepts"]
topics: ["C++20 concepts", "type constraints", "compile-time introspection", "SFINAE replacement", "template constraints", "generic programming"]
constraints: ["compile-time only", "zero runtime overhead", "C++20 requirement"]
cxx_standard: "C++20"
std_equivalent: "std::integral, std::invocable, std::ranges::range (partial)"
std_since: "C++20"
boost_equivalent: "Boost.TypeTraits (SFINAE-based, not C++20 concepts)"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-30"
audience: ["C++ developers", "library maintainers", "template library authors", "AI assistants"]
status: "reviewed"
---

# Overview - Concepts and FatPConcepts

*Fat-P Library — January 2026*

---

## Executive Summary

Concepts and FatPConcepts are C++20 concept libraries that transform template error messages from walls of incomprehensible instantiation failures into clear statements about what went wrong. `Concepts.h` provides general-purpose type introspection—container detection, callable constraints, hashable checks—with **zero dependencies on other Fat-P headers**, making it a standalone utility for any C++20 project. `FatPConcepts.h` extends this with Fat-P type detection using forward declarations that avoid header coupling. Together they eliminate hundreds of lines of SFINAE boilerplate while enabling automatic overload ranking through concept subsumption.

---

## Overview Card

**Component:** Concepts, FatPConcepts  
**Problem solved:** Cryptic template errors; SFINAE boilerplate; undocumented type requirements  
**When to use:** Template constraints; compile-time dispatch; generic library code; Fat-P type detection  
**When NOT to use:** Single concrete type; runtime polymorphism; pre-C++20 codebases  
**Key guarantee:** All checks are compile-time only; zero runtime overhead  
**std equivalent:** `<concepts>` provides core concepts (C++20). Fat-P extends with container capability detection and library type identification.  
**Boost equivalent:** None. Boost.TypeTraits provides SFINAE traits, not C++20 concepts.  
**Other alternatives:** Range-v3 concepts, custom SFINAE traits  
**Read next:** User Manual - Concepts, Companion Guide - Concepts

---

## The Problem Domain

### What Goes Wrong Without It

Consider a function template that processes containers:

```cpp
template <typename Container>
void processItems(const Container& c) {
    for (const auto& item : c) {
        std::cout << c.size() << ": " << item << "\n";
    }
}
```

When someone calls `processItems(42)`, the compiler tries to iterate over an integer. The error message points deep into the range-based for implementation, talks about `begin()` and `end()` functions that don't exist for `int`, and spans dozens of lines. The actual problem—passing an integer where a container was expected—is buried.

The situation worsens with SFINAE constraints. When a template uses `std::enable_if` to restrict its applicability, failed instantiations produce error messages about "substitution failure," "no type named 'type' in 'struct std::enable_if<false>'," and nested template parameters named `<anonymous>`. The error messages become archaeological expeditions through template machinery.

Now consider overload sets. You want one implementation for any iterable type, and an optimized implementation for contiguous containers that can use `data()` for direct memory access. With SFINAE, both overloads might match for `std::vector`, creating ambiguity. You manually add exclusion constraints, but this breaks when you add a third overload.

The impacts compound in large codebases:

| Issue | Impact |
|-------|--------|
| Cryptic error messages | Debugging takes minutes instead of seconds; juniors can't interpret template errors |
| SFINAE boilerplate | 10-15 lines per trait; hundreds of lines for comprehensive introspection |
| No overload ranking | Manual exclusion constraints; fragile when adding overloads |
| Undocumented requirements | Type requirements exist in implementation, not interface |

### The Standard's Limitation

C++20's `<concepts>` header provides fundamental concepts like `std::integral`, `std::invocable`, and `std::ranges::range`. These are essential building blocks but deliberately minimal.

The standard doesn't provide:
- `hashable` detection (is `std::hash<T>` specialized?)
- Stream concepts (`streamable`, `input_streamable`)
- Container capability detection (`has_push_back`, `reservable`)
- Custom string method detection (`has_to_string_method`)
- Library-specific type identification

The committee keeps `<concepts>` minimal because domain-specific concepts require semantic decisions. What does "hashable" mean—must `std::hash<T>` exist, or is any hash functor sufficient? Different libraries need different answers.

Fat-P makes specific decisions for its domain. `hashable` means `std::hash<T>` exists and returns `std::size_t`. This is opinionated but practical for real-world generic programming.

---

## Architecture: Two Headers, Zero Coupling

### The Zero-Dependency Principle

`Concepts.h` has a critical design property: **no dependencies on other Fat-P headers** except `CppFeatureDetection.h` for C++20 enforcement. It includes only standard library headers.

This matters because users might want Fat-P's concepts without using Fat-P containers. A game engine using `hashable` and `iterable` shouldn't compile `SmallVector.h` and `FlatMap.h` just to check type properties.

### The Forward Declaration Pattern

`FatPConcepts.h` detects Fat-P types through forward declarations:

```cpp
// Forward declaration only—SmallVector.h not included
template <typename T, std::size_t InlineCapacity, typename Allocator>
class SmallVector;

template <typename T>
concept small_vector_type = detail::is_small_vector_impl<T>::value;
```

Template specialization matching works on structural patterns, not type definitions. Checking `small_vector_type<SmallVector<int, 16>>` doesn't compile `SmallVector.h`—it just matches the template pattern.

This keeps compile times minimal while enabling complete Fat-P type identification.

---

## Feature Inventory

### 1. Container Capability Detection

Concepts for detecting what operations containers support, enabling compile-time dispatch to optimal implementations:

```cpp
template <typename Range>
void fill_zeros(Range& r) {
    if constexpr (fat_p::concepts::contiguous_container<Range>) {
        // Direct memory access available
        std::memset(r.data(), 0, r.size() * sizeof(typename Range::value_type));
    } else if constexpr (fat_p::concepts::iterable<Range>) {
        // Generic iteration
        for (auto& elem : r) elem = 0;
    }
}
```

The `if constexpr` branches compile only for types that satisfy the constraint. No runtime dispatch, no unused code generation.

### 2. Hashable Detection

Constraint for hash map keys that catches errors at the point of use, not deep in hash table internals:

```cpp
template <fat_p::concepts::hashable Key, typename Value>
class SimpleHashMap {
    std::size_t hash(const Key& k) const {
        return std::hash<Key>{}(k);  // Guaranteed valid
    }
};

// Clear error at instantiation, not in hash()
SimpleHashMap<std::vector<int>, int> map;  // Error: vector isn't hashable
```

### 3. Callable Constraints

Type-safe callback registration that catches signature mismatches at registration time:

```cpp
template <typename Handler>
    requires fat_p::concepts::invocable_r<bool, Handler, const Event&>
void setEventFilter(Handler h);

// Error immediately, not when filter is invoked
setEventFilter([](int x) { return x > 0; });  // Wrong signature
```

### 4. Fat-P Type Detection

Identify specific Fat-P types for specialized handling:

```cpp
template <typename Container>
void optimize(Container& c) {
    if constexpr (fat_p::concepts::small_vector_type<Container>) {
        if (c.is_inline()) {
            // Stack storage: use cache-local algorithm
        }
    } else if constexpr (fat_p::concepts::flat_map_type<Container>) {
        // Sorted storage: use binary search optimizations
    }
}
```

### 5. Concept-Based Overloading

Automatic selection of the most constrained matching overload:

```cpp
template <fat_p::concepts::iterable Range>
auto sum(const Range& r);  // Generic

template <fat_p::concepts::contiguous_container C>
auto sum(const C& c);  // Optimized for contiguous

std::vector<int> v;
std::forward_list<int> fl;

sum(v);   // Selects contiguous_container (more constrained)
sum(fl);  // Selects iterable (forward_list has no data())
```

No manual disambiguation. The compiler recognizes that `contiguous_container` subsumes `container` subsumes `iterable`.

---

## Why Not Alternatives?

### vs. Raw SFINAE

SFINAE works but exacts a heavy tax. Each type property requires a primary template, a specialization with `std::void_t`, and a variable template—typically 10-15 lines of boilerplate. Error messages reference internal machinery. Overload resolution requires manual exclusion.

Concepts eliminate the boilerplate, produce readable errors, and handle overload ranking automatically.

### vs. Standard `<concepts>`

The standard provides building blocks but leaves gaps. Fat-P fills specific gaps:

| Need | `<concepts>` | Fat-P |
|------|--------------|-------|
| Hashable detection | Not provided | ✓ `hashable` |
| Stream detection | Not provided | ✓ `streamable`, `input_streamable` |
| Container capabilities | Only `ranges::range` | ✓ `reservable`, `has_push_back`, etc. |
| Method detection | Not provided | ✓ `has_clear`, `has_data`, etc. |
| Fat-P type identification | Not possible | ✓ Forward-declaration based |

### vs. Boost.TypeTraits

Boost.TypeTraits provides SFINAE-based type introspection, not C++20 concepts:

| Aspect | Boost.TypeTraits | Fat-P Concepts |
|--------|------------------|----------------|
| **Mechanism** | SFINAE traits | C++20 concepts |
| **C++ version** | C++03+ | C++20+ |
| **Error messages** | Template instantiation traces | "Constraint not satisfied" |
| **Overload ranking** | Manual exclusion | Automatic subsumption |
| **Syntax** | `std::enable_if_t<...>` | `template <concept T>` |
| **Dependencies** | Boost ecosystem | None (standalone) |
| **Fat-P type detection** | Not provided | ✓ Forward-declaration based |

**When to use Boost.TypeTraits:** Pre-C++20 codebases; existing Boost ecosystem integration.

**When to use Fat-P Concepts:** C++20+ codebases; cleaner syntax; automatic overload ranking; Fat-P type detection.

### The Exclusionary Argument

| If You Need... | Why Not SFINAE | Why Not Just std:: | Fat-P Advantage |
|----------------|----------------|---------------------|-----------------|
| Readable errors | Template instantiation traces | Limited concept coverage | Comprehensive concepts |
| Zero boilerplate | 10+ lines per trait | Not provided | Pre-defined |
| Overload ranking | Manual exclusion | Subsumption works | Full hierarchy |
| Fat-P detection | Impossible | Impossible | Forward declarations |

When you need readable errors, zero boilerplate, automatic overload ranking, *and* Fat-P type detection simultaneously, Fat-P's concept headers are the only option.

---

## The "Forever Stuck" Reality

The C++ standard committee deliberately keeps `<concepts>` minimal. Domain-specific concepts like `hashable` or `serializable` require semantic decisions that vary across codebases. The committee won't standardize these decisions.

What will evolve: additional fundamental concepts, improved subsumption rules, better diagnostic messages.

What won't change: the standard won't provide Fat-P type detection, won't standardize container capability concepts, won't define what "hashable" means for every library.

Fat-P's concept headers fill permanent gaps that standard evolution won't address.

---

## Performance Characteristics

Concepts are pure compile-time constructs. They generate no code, consume no runtime, add no bytes to binaries.

| Metric | Value |
|--------|-------|
| Runtime cost | 0 ns (concepts generate no code) |
| Binary size impact | 0 bytes |
| Compile time | Negligible (concepts are lightweight checks) |

### Where Fat-P Wins

**Template-heavy codebases.** Every constrained template benefits from better error messages and self-documenting interfaces.

**Generic library code.** Libraries working with multiple container types or callable types.

**Fat-P integration.** Any code that needs to detect or specialize on Fat-P types.

### Where Fat-P Loses

**Pre-C++20 codebases.** Concepts require C++20. No compatibility layer exists or is planned.

**Single concrete type.** If you only use `std::vector<int>`, concepts add no value.

**Runtime polymorphism.** Concepts enable compile-time polymorphism. For runtime polymorphism (heterogeneous containers, plugin architectures), use inheritance.

---

## Integration Points

```
Concepts.h
    ├── CppFeatureDetection.h (C++20 enforcement)
    └── No other Fat-P dependencies (standalone utility)

FatPConcepts.h
    ├── CppFeatureDetection.h
    └── Forward declarations of Fat-P types (no full includes)

Used by:
    ├── Stringify.h (streamable, has_custom_string_method)
    ├── Signal.h (invocable constraints)
    ├── Expected.h (expected_like)
    └── Generic Fat-P code throughout the library
```

---

## Final Assessment

Fat-P's concept headers deliver on the library's core promises:

**Permanence.** The standard won't provide container capability detection, hashable checks, or Fat-P type identification. These concepts fill permanent gaps.

**Specialization.** Concepts designed for real generic programming needs—not theoretical completeness, but practical utility. Container detection, stream support, callable constraints, type classification.

**Control.** Choose exactly which concepts to use. No forced dependencies. `Concepts.h` works standalone. `FatPConcepts.h` adds Fat-P detection without coupling.

For any C++20 codebase doing generic programming, these headers transform template code from cryptic to clear—zero runtime cost, zero boilerplate, comprehensive type introspection.

---

*Concepts.h, FatPConcepts.h — Fat-P Library v3.2*
