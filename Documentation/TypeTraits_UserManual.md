# TypeTraits User Manual

## Table of Contents

1. [Introduction](#introduction)
2. [What Are Type Traits?](#what-are-type-traits)
3. [Common Design Idioms](#common-design-idioms)
4. [Detection Idiom](#detection-idiom)
5. [Container Traits](#container-traits)
6. [Comparison Traits](#comparison-traits)
7. [Callable Traits](#callable-traits)
8. [Access Traits](#access-traits)
9. [Serialization Traits](#serialization-traits)
10. [Allocator Traits](#allocator-traits)
11. [Container Operations](#container-operations)
12. [Range Traits](#range-traits)
13. [Advanced Container Traits](#advanced-container-traits)
14. [Library Type Detection](#library-type-detection)
15. [Policy Detection](#policy-detection)
16. [Special Type Categories](#special-type-categories)
17. [Trait Composition](#trait-composition)
18. [Diagnostic Utilities](#diagnostic-utilities)
19. [Custom Trait Patterns](#custom-trait-patterns)
20. [Extension Points](#extension-points)
21. [Design-by-Contract Helpers](#design-by-contract-helpers)
22. [C++20 Concepts](#cpp20-concepts)
23. [Comparison to Other Type Trait Libraries](#comparison-to-other-type-trait-libraries)
24. [Performance Considerations](#performance-considerations)
25. [Best Practices](#best-practices)
26. [Library-Specific Traits (FatPTypeTraits)](#library-specific-traits-fatptypetraits)

---

## Introduction

The fat_penelope TypeTraits library provides comprehensive compile-time type introspection for C++17 and later. This library enables you to query properties of types at compile time, allowing for sophisticated template metaprogramming, SFINAE-based overload resolution, and compile-time validation.

### Key Features

- **C++17 Minimum**: All traits work with C++17, with conditional C++20/23 enhancements
- **Header-Only**: No compilation or linking required
- **Zero Dependencies**: Only uses standard library
- **Thread-Safe**: All operations are compile-time only
- **Zero Runtime Overhead**: Everything resolves at compile time
- **Extensive**: Over 150 different type traits
- **C++20 Concepts**: Full concept support when available
- **Design-by-Contract**: Built-in DbC helpers for compile-time enforcement
- **Comprehensive Diagnostics**: Detailed explanations when traits fail
- **Library-Specific Traits**: Dedicated traits for fat_penelope components

---

## What Are Type Traits?

Type traits are template metafunctions that extract information about types at compile time. They answer questions like:

- Does this type have a `begin()` method?
- Can this type be hashed with `std::hash`?
- Does this type support comparison operators?
- Is this type invocable with certain arguments?
- Is this type a fat_penelope container?
- Can this type be used with parallel algorithms?

Type traits are the foundation of generic programming in C++, enabling you to write code that adapts to the properties of the types it operates on.

### Basic Anatomy

All traits in this library follow a consistent pattern:

```cpp
namespace fat_p {

// Trait structure
template <typename T>
struct is_something : /* implementation */ {};

// Convenience variable template (C++17)
template <typename T>
inline constexpr bool is_something_v = is_something<T>::value;

// C++20 concept (when available)
#if __cplusplus >= 202002L
template <typename T>
concept Something = is_something_v<T>;
#endif

} // namespace fat_p
```

The `_v` suffix provides a convenient shorthand for accessing the boolean result. When C++20 is available, many traits also provide corresponding concepts.

---

## Common Design Idioms

### SFINAE (Substitution Failure Is Not An Error)

SFINAE is a fundamental C++ technique where invalid template substitutions are silently ignored rather than causing compilation errors. This enables overload resolution based on type properties.

```cpp
// Example: Enable function only for iterable types
template <typename T>
std::enable_if_t<fat_p::is_iterable_v<T>, void>
print_container(const T& container) {
    for (const auto& item : container) {
        std::cout << item << " ";
    }
}
```

### Tag Dispatching

Tag dispatching uses empty struct "tags" to select function overloads at compile time based on type properties.

```cpp
namespace detail {
    struct has_reserve_tag {};
    struct no_reserve_tag {};
}

template <typename Container>
void optimize_insert(Container& c, std::size_t size, detail::has_reserve_tag) {
    c.reserve(size);
}

template <typename Container>
void optimize_insert(Container& c, std::size_t size, detail::no_reserve_tag) {
    // No optimization available
}

template <typename Container>
void optimize_insert(Container& c, std::size_t size) {
    using tag = std::conditional_t<
        fat_p::is_reservable_v<Container>,
        detail::has_reserve_tag,
        detail::no_reserve_tag
    >;
    optimize_insert(c, size, tag{});
}
```

### Concept Emulation (C++17)

Before C++20 concepts, we emulate them using `std::enable_if`:

```cpp
template <typename T, typename = std::enable_if_t<fat_p::is_container_v<T>>>
void process(const T& container) {
    // Only available for containers
}
```

### C++20 Concepts

When C++20 is available, the library provides actual concepts:

```cpp
#if __cplusplus >= 202002L
template <fat_p::is_container T>
void process(const T& container) {
    // Only available for containers
}
#endif
```

### Compile-Time Assertions

Static assertions provide clear error messages when type requirements aren't met:

```cpp
template <typename T>
void serialize(const T& obj) {
    static_assert(fat_p::is_serializable_v<T>, 
        "Type must implement serialize() and deserialize()");
    // Implementation
}
```

---

## Detection Idiom

The detection idiom is a powerful technique for checking if a type has specific members, methods, or nested types. It's the foundation for many other traits in this library.

### Core Components

#### `is_detected`

Checks if an operation or expression is valid for a given type.

```cpp
template <template <typename...> class Op, typename... Args>
using is_detected = /* implementation */;

template <template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_v = is_detected<Op, Args...>::value;
```

**Example:**

```cpp
// Define what we want to detect
template <typename T>
using value_type_t = typename T::value_type;

// Check if type has value_type
bool has_value_type = is_detected_v<value_type_t, std::vector<int>>;  // true
bool no_value_type = is_detected_v<value_type_t, int>;  // false
```

#### `detected_t`

Retrieves the detected type if the operation is valid, otherwise returns `nonesuch`.

```cpp
template <template <typename...> class Op, typename... Args>
using detected_t = /* implementation */;
```

**Example:**

```cpp
using vec_value_type = detected_t<value_type_t, std::vector<int>>;
// vec_value_type is int

using int_value_type = detected_t<value_type_t, int>;
// int_value_type is detail::nonesuch
```

#### `detected_or`

Like `detected_t`, but allows specifying a default type when detection fails.

```cpp
template <typename Default, template <typename...> class Op, typename... Args>
using detected_or = /* implementation */;
```

**Example:**

```cpp
// Use int as default if value_type doesn't exist
using value_or_int = detected_or<int, value_type_t, std::vector<double>>;
// value_or_int is double

using int_default = detected_or<int, value_type_t, int>;
// int_default is int (the default)
```

#### `is_detected_exact`

Checks if the detected type exactly matches an expected type.

```cpp
template <typename Expected, template <typename...> class Op, typename... Args>
using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

template <typename Expected, template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_exact_v = is_detected_exact<Expected, Op, Args...>::value;
```

**Example:**

```cpp
// Check if vector's value_type is exactly int
constexpr bool is_int_vec = 
    is_detected_exact_v<int, value_type_t, std::vector<int>>;  // true

constexpr bool is_double_vec = 
    is_detected_exact_v<double, value_type_t, std::vector<int>>;  // false
```

#### `is_detected_convertible`

Checks if the detected type is convertible to a target type.

```cpp
template <typename To, template <typename...> class Op, typename... Args>
using is_detected_convertible = std::is_convertible<detected_t<Op, Args...>, To>;

template <typename To, template <typename...> class Op, typename... Args>
inline constexpr bool is_detected_convertible_v = 
    is_detected_convertible<To, Op, Args...>::value;
```

**Example:**

```cpp
// Check if value_type is convertible to double
constexpr bool convertible_to_double = 
    is_detected_convertible_v<double, value_type_t, std::vector<int>>;  // true
```

### Practical Detection Idiom Example

Here's a complete example showing how to detect if a type has a `size()` method:

```cpp
#include "TypeTraits.h"
#include <vector>
#include <iostream>

// Define what we want to detect: the size() method
template <typename T>
using size_method_t = decltype(std::declval<T&>().size());

// Check various types
int main() {
    using namespace fat_p;
    
    // std::vector has size()
    std::cout << std::boolalpha;
    std::cout << "vector has size: " 
              << is_detected_v<size_method_t, std::vector<int>> << "\n";  // true
    
    // int doesn't have size()
    std::cout << "int has size: " 
              << is_detected_v<size_method_t, int> << "\n";  // false
    
    // Get the return type of size()
    using size_return = detected_or<void, size_method_t, std::vector<int>>;
    // size_return is std::size_t
    
    return 0;
}
```

---

## Container Traits

Container traits detect standard container operations and properties. These are essential for writing generic container algorithms.

### Basic Container Detection

#### `has_begin` / `has_begin_v`

Detects if a type supports `std::begin()` or has a `begin()` method.

```cpp
template <typename T>
struct has_begin : /* implementation */;

template <typename T>
inline constexpr bool has_begin_v = has_begin<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <array>

using namespace fat_p;

// All standard containers have begin()
static_assert(has_begin_v<std::vector<int>>);
static_assert(has_begin_v<std::array<int, 5>>);
static_assert(has_begin_v<std::string>);

// Built-in arrays work with std::begin
static_assert(has_begin_v<int[10]>);

// Primitives don't
static_assert(!has_begin_v<int>);

// Custom container
struct MyContainer {
    int* begin() { return data; }
    int* end() { return data + size; }
    int data[10];
    size_t size = 10;
};

static_assert(has_begin_v<MyContainer>);
```

#### `has_end` / `has_end_v`

Detects if a type supports `std::end()` or has an `end()` method.

```cpp
template <typename T>
struct has_end : /* implementation */;

template <typename T>
inline constexpr bool has_end_v = has_end<T>::value;
```

**Example:**

```cpp
static_assert(has_end_v<std::vector<int>>);
static_assert(has_end_v<MyContainer>);
static_assert(!has_end_v<int>);
```

#### `is_iterable` / `is_iterable_v`

Checks if a type has both `begin()` and `end()`, making it iterable in range-based for loops.

```cpp
template <typename T>
struct is_iterable : std::conjunction<has_begin<T>, has_end<T>> {};

template <typename T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <list>
#include <set>

using namespace fat_p;

// Generic function that works with any iterable
template <typename Container>
std::enable_if_t<is_iterable_v<Container>, void>
print_all(const Container& c) {
    for (const auto& item : c) {
        std::cout << item << " ";
    }
    std::cout << "\n";
}

int main() {
    std::vector<int> vec = {1, 2, 3};
    std::list<double> lst = {1.1, 2.2, 3.3};
    std::set<std::string> s = {"hello", "world"};
    
    print_all(vec);  // Works
    print_all(lst);  // Works
    print_all(s);    // Works
    
    // print_all(42);  // Compile error - int is not iterable
    
    return 0;
}
```

### Size and Empty Detection

#### `has_size` / `has_size_v`

Detects if a type has a `size()` method.

```cpp
template <typename T>
struct has_size : /* implementation */;

template <typename T>
inline constexpr bool has_size_v = has_size<T>::value;
```

**Example:**

```cpp
static_assert(has_size_v<std::vector<int>>);
static_assert(has_size_v<std::string>);
static_assert(has_size_v<std::array<int, 5>>);
static_assert(!has_size_v<std::forward_list<int>>);  // No size()!
```

#### `has_empty` / `has_empty_v`

Detects if a type has an `empty()` method.

```cpp
template <typename T>
struct has_empty : /* implementation */;

template <typename T>
inline constexpr bool has_empty_v = has_empty<T>::value;
```

**Example:**

```cpp
static_assert(has_empty_v<std::vector<int>>);
static_assert(has_empty_v<std::map<int, int>>);
```

#### `is_sized` / `is_sized_v`

Alias for `has_size`. Indicates a type has a `size()` method.

```cpp
template <typename T>
struct is_sized : has_size<T> {};

template <typename T>
inline constexpr bool is_sized_v = is_sized<T>::value;
```

### Composite Container Traits

#### `is_container` / `is_container_v`

Checks if a type is a container (both iterable and sized).

```cpp
template <typename T>
struct is_container : std::conjunction<is_iterable<T>, is_sized<T>> {};

template <typename T>
inline constexpr bool is_container_v = is_container<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <forward_list>

using namespace fat_p;

// Standard containers
static_assert(is_container_v<std::vector<int>>);
static_assert(is_container_v<std::map<int, int>>);
static_assert(is_container_v<std::string>);

// forward_list is iterable but not sized
static_assert(!is_container_v<std::forward_list<int>>);

// Generic algorithm requiring container
template <typename Container>
std::enable_if_t<is_container_v<Container>, std::size_t>
safe_index(const Container& c, std::size_t idx) {
    return std::min(idx, c.size() - 1);
}

int main() {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::cout << safe_index(vec, 10) << "\n";  // Prints 4
    
    // std::forward_list<int> flist = {1, 2, 3};
    // safe_index(flist, 10);  // Compile error - not a container
    
    return 0;
}
```

### Capacity Operations

#### `has_reserve` / `has_reserve_v`

Detects if a type has a `reserve(size_t)` method for capacity pre-allocation.

```cpp
template <typename T>
struct has_reserve : /* implementation */;

template <typename T>
inline constexpr bool has_reserve_v = has_reserve<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <list>
#include <string>

using namespace fat_p;

// Only certain containers support reserve
static_assert(has_reserve_v<std::vector<int>>);
static_assert(has_reserve_v<std::string>);
static_assert(!has_reserve_v<std::list<int>>);  // list doesn't have reserve
static_assert(!has_reserve_v<std::array<int, 5>>);  // array is fixed size

// Optimize insertion for types that support reserve
template <typename Container>
void optimized_bulk_insert(Container& c, std::size_t count) {
    if constexpr (has_reserve_v<Container>) {
        c.reserve(c.size() + count);
        std::cout << "Reserving space for " << count << " elements\n";
    }
    
    for (std::size_t i = 0; i < count; ++i) {
        c.push_back(typename Container::value_type{});
    }
}

int main() {
    std::vector<int> vec;
    optimized_bulk_insert(vec, 1000);  // Will reserve space
    
    std::list<int> lst;
    optimized_bulk_insert(lst, 1000);  // Won't reserve (not possible)
    
    return 0;
}
```

#### `is_reservable` / `is_reservable_v`

Alias for `has_reserve`. Indicates a container supports capacity reservation.

```cpp
template <typename T>
struct is_reservable : has_reserve<T> {};

template <typename T>
inline constexpr bool is_reservable_v = is_reservable<T>::value;
```

### Data Access

#### `has_data` / `has_data_v`

Detects if a type has a `data()` method returning a pointer to the underlying array.

```cpp
template <typename T>
struct has_data : /* implementation */;

template <typename T>
inline constexpr bool has_data_v = has_data<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <array>
#include <list>

using namespace fat_p;

// Contiguous containers have data()
static_assert(has_data_v<std::vector<int>>);
static_assert(has_data_v<std::array<int, 5>>);
static_assert(has_data_v<std::string>);

// Non-contiguous containers don't
static_assert(!has_data_v<std::list<int>>);
static_assert(!has_data_v<std::set<int>>);

// Pass to C API
void process_array(const int* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        std::cout << data[i] << " ";
    }
}

template <typename Container>
std::enable_if_t<has_data_v<Container>, void>
pass_to_c_api(const Container& c) {
    process_array(c.data(), c.size());
}

int main() {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    pass_to_c_api(vec);  // Works - vector has data()
    
    // std::list<int> lst = {1, 2, 3};
    // pass_to_c_api(lst);  // Compile error - list doesn't have data()
    
    return 0;
}
```

#### `is_contiguous_container` / `is_contiguous_container_v`

Checks if a type is a contiguous container (is a container and has `data()`).

```cpp
template <typename T>
struct is_contiguous_container : std::conjunction<is_container<T>, has_data<T>> {};

template <typename T>
inline constexpr bool is_contiguous_container_v = is_contiguous_container<T>::value;
```

**Example:**

```cpp
static_assert(is_contiguous_container_v<std::vector<int>>);
static_assert(is_contiguous_container_v<std::array<int, 5>>);
static_assert(!is_contiguous_container_v<std::list<int>>);
static_assert(!is_contiguous_container_v<std::deque<int>>);
```

### Reverse Iteration

#### `has_rbegin` / `has_rbegin_v`

Detects if a type supports `std::rbegin()` or has an `rbegin()` method.

```cpp
template <typename T>
struct has_rbegin : /* implementation */;

template <typename T>
inline constexpr bool has_rbegin_v = has_rbegin<T>::value;
```

**Example:**

```cpp
static_assert(has_rbegin_v<std::vector<int>>);
static_assert(has_rbegin_v<std::string>);
static_assert(!has_rbegin_v<std::forward_list<int>>);
```

#### `has_rend` / `has_rend_v`

Detects if a type supports `std::rend()` or has an `rend()` method.

```cpp
template <typename T>
struct has_rend : /* implementation */;

template <typename T>
inline constexpr bool has_rend_v = has_rend<T>::value;
```

#### `is_reverse_iterable` / `is_reverse_iterable_v`

Checks if a type supports reverse iteration (has both `rbegin()` and `rend()`).

```cpp
template <typename T>
struct is_reverse_iterable : std::conjunction<has_rbegin<T>, has_rend<T>> {};

template <typename T>
inline constexpr bool is_reverse_iterable_v = is_reverse_iterable<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <forward_list>

using namespace fat_p;

// Print in reverse if possible
template <typename Container>
void print_reverse(const Container& c) {
    if constexpr (is_reverse_iterable_v<Container>) {
        std::cout << "Reverse: ";
        for (auto it = c.rbegin(); it != c.rend(); ++it) {
            std::cout << *it << " ";
        }
    } else {
        std::cout << "Cannot reverse iterate\n";
    }
}

int main() {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    print_reverse(vec);  // Prints: Reverse: 5 4 3 2 1
    
    std::forward_list<int> flist = {1, 2, 3};
    print_reverse(flist);  // Prints: Cannot reverse iterate
    
    return 0;
}
```

### Container Modification Operations

#### `has_clear` / `has_clear_v`

Detects if a type has a `clear()` method.

```cpp
template <typename T>
struct has_clear : /* implementation */;

template <typename T>
inline constexpr bool has_clear_v = has_clear<T>::value;
```

**Example:**

```cpp
static_assert(has_clear_v<std::vector<int>>);
static_assert(has_clear_v<std::map<int, int>>);
static_assert(!has_clear_v<std::array<int, 5>>);  // Arrays are fixed size
```

#### `has_push_back` / `has_push_back_v`

Detects if a type has a `push_back()` method.

```cpp
template <typename T>
struct has_push_back : /* implementation */;

template <typename T>
inline constexpr bool has_push_back_v = has_push_back<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <deque>
#include <set>

using namespace fat_p;

static_assert(has_push_back_v<std::vector<int>>);
static_assert(has_push_back_v<std::deque<int>>);
static_assert(!has_push_back_v<std::set<int>>);  // set uses insert

// Generic append
template <typename Container, typename T>
void append_item(Container& c, T&& item) {
    if constexpr (has_push_back_v<Container>) {
        c.push_back(std::forward<T>(item));
    } else {
        c.insert(std::forward<T>(item));
    }
}
```

#### `has_emplace_back` / `has_emplace_back_v`

Detects if a type has an `emplace_back()` method for in-place construction.

```cpp
template <typename T>
struct has_emplace_back : /* implementation */;

template <typename T>
inline constexpr bool has_emplace_back_v = has_emplace_back<T>::value;
```

**Example:**

```cpp
static_assert(has_emplace_back_v<std::vector<int>>);
static_assert(has_emplace_back_v<std::deque<int>>);

// Efficient insertion
template <typename Container, typename... Args>
void efficient_add(Container& c, Args&&... args) {
    if constexpr (has_emplace_back_v<Container>) {
        c.emplace_back(std::forward<Args>(args)...);
    } else {
        c.insert(typename Container::value_type(std::forward<Args>(args)...));
    }
}
```

#### `has_push_front` / `has_push_front_v`

Detects if a type has a `push_front()` method.

```cpp
template <typename T>
struct has_push_front : /* implementation */;

template <typename T>
inline constexpr bool has_push_front_v = has_push_front<T>::value;
```

**Example:**

```cpp
static_assert(has_push_front_v<std::deque<int>>);
static_assert(has_push_front_v<std::list<int>>);
static_assert(!has_push_front_v<std::vector<int>>);
```

### Map Detection

#### `is_map_like` / `is_map_like_v`

Detects if a type behaves like a map (iterable with `std::pair` as `value_type`).

```cpp
template <typename T>
struct is_map_like : std::conjunction<
    is_iterable<T>,
    detail::has_pair_value_type<T>
> {};

template <typename T>
inline constexpr bool is_map_like_v = is_map_like<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <map>
#include <unordered_map>
#include <vector>

using namespace fat_p;

static_assert(is_map_like_v<std::map<int, std::string>>);
static_assert(is_map_like_v<std::unordered_map<int, std::string>>);
static_assert(!is_map_like_v<std::vector<int>>);

// Generic function for map-like containers
template <typename MapLike>
std::enable_if_t<is_map_like_v<MapLike>, void>
print_map(const MapLike& m) {
    for (const auto& [key, value] : m) {
        std::cout << key << " => " << value << "\n";
    }
}

int main() {
    std::map<int, std::string> m = {{1, "one"}, {2, "two"}};
    print_map(m);  // Works
    
    // std::vector<int> v = {1, 2, 3};
    // print_map(v);  // Compile error - not map-like
    
    return 0;
}
```

---

## Comparison Traits

Comparison traits detect the presence of comparison operators and related functionality.

### Equality Comparison

#### `is_equality_comparable` / `is_equality_comparable_v`

Detects if a type supports `operator==`.

```cpp
template <typename T>
struct is_equality_comparable : /* implementation */;

template <typename T>
inline constexpr bool is_equality_comparable_v = is_equality_comparable<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <string>

using namespace fat_p;

// Built-in types are equality comparable
static_assert(is_equality_comparable_v<int>);
static_assert(is_equality_comparable_v<double>);

// Standard library types
static_assert(is_equality_comparable_v<std::string>);
static_assert(is_equality_comparable_v<std::vector<int>>);

// Custom type without operator==
struct NoEquality {
    int value;
};
static_assert(!is_equality_comparable_v<NoEquality>);

// Custom type with operator==
struct WithEquality {
    int value;
    bool operator==(const WithEquality& other) const {
        return value == other.value;
    }
};
static_assert(is_equality_comparable_v<WithEquality>);

// Generic equality check
template <typename T>
std::enable_if_t<is_equality_comparable_v<T>, bool>
are_equal(const T& a, const T& b) {
    return a == b;
}
```

#### `is_inequality_comparable` / `is_inequality_comparable_v`

Detects if a type supports `operator!=`.

```cpp
template <typename T>
struct is_inequality_comparable : /* implementation */;

template <typename T>
inline constexpr bool is_inequality_comparable_v = is_inequality_comparable<T>::value;
```

**Example:**

```cpp
static_assert(is_inequality_comparable_v<int>);
static_assert(is_inequality_comparable_v<std::string>);
```

### Relational Comparison

#### `has_less` / `has_less_v`

Detects if a type supports `operator<`.

```cpp
template <typename T>
struct has_less : /* implementation */;

template <typename T>
inline constexpr bool has_less_v = has_less<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <string>

using namespace fat_p;

static_assert(has_less_v<int>);
static_assert(has_less_v<std::string>);

struct NotComparable {
    int value;
};
static_assert(!has_less_v<NotComparable>);

// Generic min function
template <typename T>
std::enable_if_t<has_less_v<T>, const T&>
min_value(const T& a, const T& b) {
    return a < b ? a : b;
}
```

#### `has_less_than` / `has_less_than_v`

Detects if a type supports `operator<` for ordering.

```cpp
template <typename T>
struct has_less_than : has_less<T> {};

template <typename T>
inline constexpr bool has_less_than_v = has_less_than<T>::value;
```

#### `has_less_equal` / `has_less_equal_v`

Detects if a type supports `operator<=`.

```cpp
template <typename T>
struct has_less_equal : /* implementation */;

template <typename T>
inline constexpr bool has_less_equal_v = has_less_equal<T>::value;
```

#### `has_greater` / `has_greater_v`

Detects if a type supports `operator>`.

```cpp
template <typename T>
struct has_greater : /* implementation */;

template <typename T>
inline constexpr bool has_greater_v = has_greater<T>::value;
```

#### `has_greater_equal` / `has_greater_equal_v`

Detects if a type supports `operator>=`.

```cpp
template <typename T>
struct has_greater_equal : /* implementation */;

template <typename T>
inline constexpr bool has_greater_equal_v = has_greater_equal<T>::value;
```

#### `is_fully_ordered` / `is_fully_ordered_v`

Checks if a type has all four relational operators (`<`, `<=`, `>`, `>=`).

```cpp
template <typename T>
struct is_fully_ordered : std::conjunction<
    has_less<T>,
    has_less_equal<T>,
    has_greater<T>,
    has_greater_equal<T>
> {};

template <typename T>
inline constexpr bool is_fully_ordered_v = is_fully_ordered<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

// Built-in types are fully ordered
static_assert(is_fully_ordered_v<int>);
static_assert(is_fully_ordered_v<double>);
static_assert(is_fully_ordered_v<std::string>);

// Minimal comparable type
struct MinimalComparable {
    int value;
    bool operator<(const MinimalComparable& other) const {
        return value < other.value;
    }
};

// Only has <, not fully ordered
static_assert(!is_fully_ordered_v<MinimalComparable>);

// Use in sort requirements
template <typename Container>
void sort_if_ordered(Container& c) {
    using T = typename Container::value_type;
    if constexpr (is_fully_ordered_v<T>) {
        std::sort(c.begin(), c.end());
    } else {
        static_assert(has_less_v<T>, "Type must at least support operator<");
        std::sort(c.begin(), c.end());
    }
}
```

### Three-Way Comparison (C++20)

#### `is_three_way_comparable` / `is_three_way_comparable_v` (C++20 only)

Detects if a type supports the spaceship operator `operator<=>`.

```cpp
#if CPP_UTILITIES_HAS_CPP20
template <typename T>
struct is_three_way_comparable : /* implementation */;

template <typename T>
inline constexpr bool is_three_way_comparable_v = is_three_way_comparable<T>::value;
#endif
```

**Example (C++20):**

```cpp
#if __cplusplus >= 202002L
#include "TypeTraits.h"
#include <compare>

using namespace fat_p;

struct ThreeWayComparable {
    int value;
    auto operator<=>(const ThreeWayComparable&) const = default;
};

static_assert(is_three_way_comparable_v<ThreeWayComparable>);
#endif
```

### Custom Comparators

#### `is_valid_comparator` / `is_valid_comparator_v`

Checks if a comparator type can compare objects of type T.

```cpp
template <typename Comp, typename T>
struct is_valid_comparator : /* implementation */;

template <typename Comp, typename T>
inline constexpr bool is_valid_comparator_v = is_valid_comparator<Comp, T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <functional>

using namespace fat_p;

struct MyComparator {
    bool operator()(int a, int b) const {
        return a < b;
    }
};

static_assert(is_valid_comparator_v<MyComparator, int>);
static_assert(is_valid_comparator_v<std::less<int>, int>);

// Use in generic sort
template <typename Container, typename Comparator>
std::enable_if_t<
    is_valid_comparator_v<Comparator, typename Container::value_type>,
    void
>
custom_sort(Container& c, Comparator comp) {
    std::sort(c.begin(), c.end(), comp);
}
```

#### `is_transparent` / `is_transparent_v`

Detects if a comparator or hash function has the `is_transparent` tag, enabling heterogeneous lookup.

```cpp
template <typename T>
struct is_transparent : /* implementation */;

template <typename T>
inline constexpr bool is_transparent_v = is_transparent<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <set>
#include <string>

using namespace fat_p;

// std::less<void> is transparent
static_assert(is_transparent_v<std::less<void>>);
static_assert(is_transparent_v<std::less<>>);

// std::less<int> is not transparent
static_assert(!is_transparent_v<std::less<int>>);

// Heterogeneous lookup example
void heterogeneous_lookup_example() {
    std::set<std::string, std::less<>> s = {"hello", "world"};
    
    // Can find with string_view without creating a string
    auto it = s.find("hello");  // No string allocation!
}
```

### Hashing

#### `is_hashable` / `is_hashable_v`

Detects if `std::hash<T>` is defined and valid for a type.

```cpp
template <typename T>
struct is_hashable : /* implementation */;

template <typename T>
inline constexpr bool is_hashable_v = is_hashable<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <string>
#include <unordered_map>

using namespace fat_p;

// Standard hashable types
static_assert(is_hashable_v<int>);
static_assert(is_hashable_v<std::string>);
static_assert(is_hashable_v<double>);

// Not hashable by default
struct NotHashable {
    int value;
};
static_assert(!is_hashable_v<NotHashable>);

// Can be used as unordered container key
template <typename Key, typename Value>
struct CanBeHashKey {
    static constexpr bool value = is_hashable_v<Key> && 
                                  is_equality_comparable_v<Key>;
};

// Generic unordered map factory
template <typename K, typename V>
std::enable_if_t<CanBeHashKey<K, V>::value, std::unordered_map<K, V>>
make_hash_map() {
    return {};
}

int main() {
    auto m1 = make_hash_map<int, std::string>();  // OK
    auto m2 = make_hash_map<std::string, int>();  // OK
    // auto m3 = make_hash_map<NotHashable, int>();  // Compile error
    
    return 0;
}
```

---

## Callable Traits

Callable traits detect and validate callable objects (functions, lambdas, functors).

### Invocability

#### `is_invocable` / `is_invocable_v`

Checks if a callable `F` can be invoked with arguments `Args...`.

```cpp
template <typename F, typename... Args>
struct is_invocable : std::is_invocable<F, Args...> {};

template <typename F, typename... Args>
inline constexpr bool is_invocable_v = is_invocable<F, Args...>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <functional>

using namespace fat_p;

void func(int x, double y) { }

struct Functor {
    void operator()(int) { }
};

auto lambda = [](int, int) { return 42; };

// Test invocability
static_assert(is_invocable_v<decltype(func), int, double>);
static_assert(is_invocable_v<Functor, int>);
static_assert(is_invocable_v<decltype(lambda), int, int>);

// Wrong arguments
static_assert(!is_invocable_v<decltype(func), int>);  // Missing argument
static_assert(!is_invocable_v<Functor, double>);  // Wrong type

// Generic callback system
template <typename Callback, typename... Args>
std::enable_if_t<is_invocable_v<Callback, Args...>, void>
invoke_callback(Callback&& cb, Args&&... args) {
    std::forward<Callback>(cb)(std::forward<Args>(args)...);
}

int main() {
    invoke_callback([](int x) { 
        std::cout << "Called with: " << x << "\n"; 
    }, 42);
    
    return 0;
}
```

#### `is_invocable_r` / `is_invocable_r_v`

Checks if a callable `F` invoked with `Args...` returns a type convertible to `R`.

```cpp
template <typename R, typename F, typename... Args>
struct is_invocable_r : std::is_invocable_r<R, F, Args...> {};

template <typename R, typename F, typename... Args>
inline constexpr bool is_invocable_r_v = is_invocable_r<R, F, Args...>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

int func1(int x) { return x * 2; }
void func2(int x) { }
double func3(int x) { return x * 1.5; }

// Check return types
static_assert(is_invocable_r_v<int, decltype(func1), int>);
static_assert(is_invocable_r_v<void, decltype(func2), int>);

// Conversion allowed
static_assert(is_invocable_r_v<double, decltype(func1), int>);  // int -> double

// Wrong return type
static_assert(!is_invocable_r_v<std::string, decltype(func1), int>);

// Generic function wrapper with return type check
template <typename R, typename F, typename... Args>
std::enable_if_t<is_invocable_r_v<R, F, Args...>, R>
invoke_and_return(F&& f, Args&&... args) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}

int main() {
    int result = invoke_and_return<int>(func1, 21);
    std::cout << "Result: " << result << "\n";  // 42
    
    return 0;
}
```

#### `is_nothrow_invocable` / `is_nothrow_invocable_v`

Checks if a callable can be invoked without throwing exceptions.

```cpp
template <typename F, typename... Args>
struct is_nothrow_invocable : std::is_nothrow_invocable<F, Args...> {};

template <typename F, typename... Args>
inline constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<F, Args...>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

void no_throw(int x) noexcept { }
void may_throw(int x) { }

static_assert(is_nothrow_invocable_v<decltype(no_throw), int>);
static_assert(!is_nothrow_invocable_v<decltype(may_throw), int>);

// Exception-safe wrapper
template <typename F, typename... Args>
auto safe_invoke(F&& f, Args&&... args) noexcept(
    is_nothrow_invocable_v<F, Args...>
) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}
```

### Function Objects

#### `is_function_object` / `is_function_object_v`

Detects if a type has `operator()`, making it a function object (functor).

```cpp
template <typename T>
struct is_function_object : /* implementation */;

template <typename T>
inline constexpr bool is_function_object_v = is_function_object<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

struct Functor {
    int operator()(int x) const { return x * 2; }
};

auto lambda = [](int x) { return x * 2; };

static_assert(is_function_object_v<Functor>);
static_assert(is_function_object_v<decltype(lambda)>);
static_assert(!is_function_object_v<int>);
static_assert(!is_function_object_v<void(*)(int)>);  // Function pointer

// Store callable objects
template <typename F>
std::enable_if_t<is_function_object_v<F>, void>
store_functor(F&& f) {
    // Can safely store and call later
    auto stored = std::forward<F>(f);
    std::cout << stored(21) << "\n";
}

int main() {
    Functor func;
    store_functor(func);
    
    store_functor([](int x) { return x * 3; });
    
    return 0;
}
```

---

## Access Traits

Access traits detect random access and subscript operations.

### Subscript Access

#### `has_subscript` / `has_subscript_v`

Detects if a type supports `operator[]`.

```cpp
template <typename T>
struct has_subscript : /* implementation */;

template <typename T>
inline constexpr bool has_subscript_v = has_subscript<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <map>
#include <list>

using namespace fat_p;

// Containers with subscript
static_assert(has_subscript_v<std::vector<int>>);
static_assert(has_subscript_v<std::array<int, 5>>);
static_assert(has_subscript_v<std::map<int, int>>);
static_assert(has_subscript_v<std::string>);

// No subscript
static_assert(!has_subscript_v<std::list<int>>);
static_assert(!has_subscript_v<std::set<int>>);

// Generic subscript access
template <typename Container>
std::enable_if_t<has_subscript_v<Container>, 
                 decltype(std::declval<Container>()[0])>
get_at_index(Container& c, std::size_t idx) {
    return c[idx];
}

int main() {
    std::vector<int> vec = {10, 20, 30};
    std::cout << get_at_index(vec, 1) << "\n";  // 20
    
    // std::list<int> lst = {10, 20, 30};
    // get_at_index(lst, 1);  // Compile error - no subscript
    
    return 0;
}
```

#### `has_at` / `has_at_v`

Detects if a type has an `at()` method for bounds-checked access.

```cpp
template <typename T>
struct has_at : /* implementation */;

template <typename T>
inline constexpr bool has_at_v = has_at<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <array>

using namespace fat_p;

static_assert(has_at_v<std::vector<int>>);
static_assert(has_at_v<std::array<int, 5>>);
static_assert(has_at_v<std::map<int, int>>);
static_assert(!has_at_v<int*>);

// Safe bounds-checked access
template <typename Container>
auto safe_access(Container& c, std::size_t idx) {
    if constexpr (has_at_v<Container>) {
        return c.at(idx);  // Bounds checked
    } else if constexpr (has_subscript_v<Container>) {
        return c[idx];  // Unchecked
    } else {
        static_assert(has_at_v<Container> || has_subscript_v<Container>,
                      "Container must support at() or operator[]");
    }
}
```

### Random Access

#### `is_random_accessible` / `is_random_accessible_v`

Checks if a type supports random access (has both subscript and size).

```cpp
template <typename T>
struct is_random_accessible : std::conjunction<has_subscript<T>, has_size<T>> {};

template <typename T>
inline constexpr bool is_random_accessible_v = is_random_accessible<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <deque>
#include <list>

using namespace fat_p;

static_assert(is_random_accessible_v<std::vector<int>>);
static_assert(is_random_accessible_v<std::deque<int>>);
static_assert(is_random_accessible_v<std::array<int, 5>>);
static_assert(!is_random_accessible_v<std::list<int>>);

// Binary search requires random access
template <typename Container, typename T>
std::enable_if_t<is_random_accessible_v<Container>, bool>
binary_search(const Container& c, const T& value) {
    std::size_t left = 0;
    std::size_t right = c.size();
    
    while (left < right) {
        std::size_t mid = (left + right) / 2;
        if (c[mid] < value) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return left < c.size() && c[left] == value;
}

int main() {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    bool found = binary_search(vec, 3);
    std::cout << std::boolalpha << found << "\n";  // true
    
    return 0;
}
```

---

## Serialization Traits

Serialization traits detect serialization capabilities.

### Stream Serialization

#### `has_serialize` / `has_serialize_v`

Detects if a type has a `serialize(std::ostream&)` method.

```cpp
template <typename T>
struct has_serialize : /* implementation */;

template <typename T>
inline constexpr bool has_serialize_v = has_serialize<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <sstream>

using namespace fat_p;

struct Serializable {
    int value;
    void serialize(std::ostream& os) const {
        os << value;
    }
    
    static Serializable deserialize(std::istream& is) {
        Serializable obj;
        is >> obj.value;
        return obj;
    }
};

struct NotSerializable {
    int value;
};

static_assert(has_serialize_v<Serializable>);
static_assert(!has_serialize_v<NotSerializable>);
```

#### `has_deserialize` / `has_deserialize_v`

Detects if a type has a static `deserialize(std::istream&)` method.

```cpp
template <typename T>
struct has_deserialize : /* implementation */;

template <typename T>
inline constexpr bool has_deserialize_v = has_deserialize<T>::value;
```

#### `is_serializable` / `is_serializable_v`

Checks if a type supports both serialization and deserialization.

```cpp
template <typename T>
struct is_serializable : std::conjunction<has_serialize<T>, has_deserialize<T>> {};

template <typename T>
inline constexpr bool is_serializable_v = is_serializable<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <sstream>
#include <vector>

using namespace fat_p;

struct FullySerializable {
    int x;
    std::string name;
    
    void serialize(std::ostream& os) const {
        os << x << " " << name;
    }
    
    static FullySerializable deserialize(std::istream& is) {
        FullySerializable obj;
        is >> obj.x >> obj.name;
        return obj;
    }
};

static_assert(is_serializable_v<FullySerializable>);

// Generic serialization framework
template <typename T>
std::enable_if_t<is_serializable_v<T>, std::string>
to_string(const T& obj) {
    std::ostringstream oss;
    obj.serialize(oss);
    return oss.str();
}

template <typename T>
std::enable_if_t<is_serializable_v<T>, T>
from_string(const std::string& str) {
    std::istringstream iss(str);
    return T::deserialize(iss);
}

int main() {
    FullySerializable obj1{42, "test"};
    
    std::string serialized = to_string(obj1);
    std::cout << "Serialized: " << serialized << "\n";
    
    auto obj2 = from_string<FullySerializable>(serialized);
    std::cout << "Deserialized: " << obj2.x << ", " << obj2.name << "\n";
    
    return 0;
}
```

### Binary Serialization

#### `has_binary_serialize` / `has_binary_serialize_v`

Detects if a type has a `binary_serialize(std::vector<uint8_t>&)` method.

```cpp
template <typename T>
struct has_binary_serialize : /* implementation */;

template <typename T>
inline constexpr bool has_binary_serialize_v = has_binary_serialize<T>::value;
```

#### `has_binary_deserialize` / `has_binary_deserialize_v`

Detects if a type has a static `binary_deserialize(const std::vector<uint8_t>&)` method.

```cpp
template <typename T>
struct has_binary_deserialize : /* implementation */;

template <typename T>
inline constexpr bool has_binary_deserialize_v = has_binary_deserialize<T>::value;
```

#### `is_binary_serializable` / `is_binary_serializable_v`

Checks if a type supports both binary serialization and deserialization.

```cpp
template <typename T>
struct is_binary_serializable {
    static constexpr bool value = has_binary_serialize_v<T> && 
                                  has_binary_deserialize_v<T>;
};

template <typename T>
inline constexpr bool is_binary_serializable_v = is_binary_serializable<T>::value;
```

**Example:**

```cpp
#include "FatPTypeTraits.h"
#include <vector>
#include <cstring>

using namespace fat_p;

struct BinarySerializable {
    int x;
    double y;
    
    void binary_serialize(std::vector<uint8_t>& buffer) const {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        buffer.insert(buffer.end(), data, data + sizeof(*this));
    }
    
    static BinarySerializable binary_deserialize(const std::vector<uint8_t>& buffer) {
        BinarySerializable obj;
        std::memcpy(&obj, buffer.data(), sizeof(obj));
        return obj;
    }
};

static_assert(is_binary_serializable_v<BinarySerializable>);

// Binary serialization framework
template <typename T>
std::enable_if_t<is_binary_serializable_v<T>, std::vector<uint8_t>>
to_binary(const T& obj) {
    std::vector<uint8_t> buffer;
    obj.binary_serialize(buffer);
    return buffer;
}

template <typename T>
std::enable_if_t<is_binary_serializable_v<T>, T>
from_binary(const std::vector<uint8_t>& buffer) {
    return T::binary_deserialize(buffer);
}
```

---

## Allocator Traits

Allocator traits detect allocator-related properties.

### Allocator Detection

#### `has_allocator_type` / `has_allocator_type_v`

Detects if a type has an `allocator_type` member type.

```cpp
template <typename T>
struct has_allocator_type : /* implementation */;

template <typename T>
inline constexpr bool has_allocator_type_v = has_allocator_type<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <array>

using namespace fat_p;

// Containers with allocators
static_assert(has_allocator_type_v<std::vector<int>>);
static_assert(has_allocator_type_v<std::string>);
static_assert(has_allocator_type_v<std::map<int, int>>);

// No allocator
static_assert(!has_allocator_type_v<std::array<int, 5>>);
static_assert(!has_allocator_type_v<int>);
```

#### `is_allocator` / `is_allocator_v`

Checks if a type satisfies allocator requirements.

```cpp
template <typename T>
struct is_allocator : /* implementation */;

template <typename T>
inline constexpr bool is_allocator_v = is_allocator<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <memory>

using namespace fat_p;

static_assert(is_allocator_v<std::allocator<int>>);
static_assert(is_allocator_v<std::allocator<double>>);
static_assert(!is_allocator_v<int>);
static_assert(!is_allocator_v<std::vector<int>>);

// Custom allocator
template <typename T>
struct CustomAllocator {
    using value_type = T;
    
    T* allocate(std::size_t n) {
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    
    void deallocate(T* p, std::size_t n) {
        ::operator delete(p);
    }
};

static_assert(is_allocator_v<CustomAllocator<int>>);

// Generic allocator-aware container
template <typename T, typename Alloc>
struct Container {
    static_assert(is_allocator_v<Alloc>, "Alloc must be an allocator");
    // Implementation
};
```

#### `has_rebind` / `has_rebind_v`

Detects if an allocator has a `rebind` member template.

```cpp
template <typename T>
struct has_rebind : /* implementation */;

template <typename T>
inline constexpr bool has_rebind_v = has_rebind<T>::value;
```

**Example:**

```cpp
static_assert(has_rebind_v<std::allocator<int>>);

// Check for rebind capability
template <typename Alloc, typename NewType>
void rebind_allocator() {
    if constexpr (has_rebind_v<Alloc>) {
        using ReboundAlloc = typename Alloc::template rebind<NewType>;
        // Use rebounded allocator
    }
}
```

---

## Standard Library Wrapper Traits

These traits detect standard library type patterns.

### Atomic Types

#### `is_atomic` / `is_atomic_v`

Detects if a type is `std::atomic<U>` for some U.

```cpp
template <typename T>
struct is_atomic : /* implementation */;

template <typename T>
inline constexpr bool is_atomic_v = is_atomic<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <atomic>

using namespace fat_p;

static_assert(is_atomic_v<std::atomic<int>>);
static_assert(is_atomic_v<std::atomic<bool>>);
static_assert(!is_atomic_v<int>);
static_assert(!is_atomic_v<std::vector<int>>);

// Thread-safe operations on atomic types only
template <typename T>
std::enable_if_t<is_atomic_v<T>, void>
atomic_increment(T& value) {
    value.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    std::atomic<int> counter{0};
    atomic_increment(counter);
    
    return 0;
}
```

### Enum Types

#### `is_scoped_enum` / `is_scoped_enum_v`

Detects if a type is a scoped enum (enum class).

```cpp
template <typename T>
struct is_scoped_enum : std::conjunction<
    std::is_enum<T>,
    std::negation<std::is_convertible<T, int>>
> {};

template <typename T>
inline constexpr bool is_scoped_enum_v = is_scoped_enum<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

enum OldEnum { Value1, Value2 };
enum class NewEnum { Value1, Value2 };

static_assert(!is_scoped_enum_v<OldEnum>);
static_assert(is_scoped_enum_v<NewEnum>);
static_assert(!is_scoped_enum_v<int>);

// Convert scoped enum to underlying type
template <typename E>
std::enable_if_t<is_scoped_enum_v<E>, std::underlying_type_t<E>>
to_underlying(E e) {
    return static_cast<std::underlying_type_t<E>>(e);
}

int main() {
    NewEnum e = NewEnum::Value1;
    int value = to_underlying(e);
    
    return 0;
}
```

### Transparent Functors

#### `is_transparent` / `is_transparent_v`

Already covered in Comparison Traits section. Detects the `is_transparent` tag for heterogeneous lookup support.

---

## Tuple-Like Traits

Tuple-like traits detect types that support the tuple protocol.

### Tuple Protocol Detection

#### `has_tuple_size` / `has_tuple_size_v`

Detects if `std::tuple_size<T>::value` is valid.

```cpp
template <typename T>
struct has_tuple_size : /* implementation */;

template <typename T>
inline constexpr bool has_tuple_size_v = has_tuple_size<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <tuple>
#include <array>
#include <utility>

using namespace fat_p;

static_assert(has_tuple_size_v<std::tuple<int, double>>);
static_assert(has_tuple_size_v<std::pair<int, double>>);
static_assert(has_tuple_size_v<std::array<int, 5>>);
static_assert(!has_tuple_size_v<std::vector<int>>);
```

#### `has_tuple_element` / `has_tuple_element_v`

Detects if `std::tuple_element<0, T>::type` is valid.

```cpp
template <typename T>
struct has_tuple_element : /* implementation */;

template <typename T>
inline constexpr bool has_tuple_element_v = has_tuple_element<T>::value;
```

#### `has_get` / `has_get_v`

Detects if `std::get<0>(T)` is valid.

```cpp
template <typename T>
struct has_get : /* implementation */;

template <typename T>
inline constexpr bool has_get_v = has_get<T>::value;
```

#### `is_tuple_like` / `is_tuple_like_v`

Checks if a type supports the full tuple protocol (tuple_size, tuple_element, and get).

```cpp
template <typename T>
struct is_tuple_like : std::conjunction<
    has_tuple_size<T>,
    has_tuple_element<T>,
    has_get<T>
> {};

template <typename T>
inline constexpr bool is_tuple_like_v = is_tuple_like<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <tuple>
#include <array>

using namespace fat_p;

static_assert(is_tuple_like_v<std::tuple<int, double, std::string>>);
static_assert(is_tuple_like_v<std::pair<int, double>>);
static_assert(is_tuple_like_v<std::array<int, 5>>);
static_assert(!is_tuple_like_v<std::vector<int>>);

// Generic tuple printer
template <typename Tuple, std::size_t... Is>
void print_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    ((std::cout << (Is == 0 ? "" : ", ") << std::get<Is>(t)), ...);
}

template <typename T>
std::enable_if_t<is_tuple_like_v<T>, void>
print_tuple(const T& t) {
    std::cout << "(";
    print_tuple_impl(t, std::make_index_sequence<std::tuple_size_v<T>>{});
    std::cout << ")\n";
}

int main() {
    std::tuple<int, double, std::string> t1{42, 3.14, "hello"};
    print_tuple(t1);  // (42, 3.14, hello)
    
    std::pair<int, std::string> p{100, "world"};
    print_tuple(p);  // (100, world)
    
    std::array<int, 3> arr{1, 2, 3};
    print_tuple(arr);  // (1, 2, 3)
    
    return 0;
}
```

---

## Iterator Traits

Iterator traits detect iterator-related properties.

### Iterator Category Detection

#### `has_iterator_category` / `has_iterator_category_v`

Detects if `std::iterator_traits<T>::iterator_category` exists.

```cpp
template <typename T>
struct has_iterator_category : /* implementation */;

template <typename T>
inline constexpr bool has_iterator_category_v = has_iterator_category<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <iterator>

using namespace fat_p;

using VecIter = std::vector<int>::iterator;
using ListIter = std::list<int>::iterator;

static_assert(has_iterator_category_v<VecIter>);
static_assert(has_iterator_category_v<ListIter>);
static_assert(has_iterator_category_v<int*>);
static_assert(!has_iterator_category_v<int>);

// Check iterator category
template <typename Iterator>
void check_iterator_type() {
    if constexpr (has_iterator_category_v<Iterator>) {
        using category = typename std::iterator_traits<Iterator>::iterator_category;
        
        if constexpr (std::is_same_v<category, std::random_access_iterator_tag>) {
            std::cout << "Random access iterator\n";
        } else if constexpr (std::is_same_v<category, std::bidirectional_iterator_tag>) {
            std::cout << "Bidirectional iterator\n";
        } else {
            std::cout << "Other iterator type\n";
        }
    }
}
```

---

## Optional and Variant Traits

These traits detect optional-like and variant-like types.

### Optional-Like Types

#### `has_has_value` / `has_has_value_v`

Detects if a type has a `has_value()` method.

```cpp
template <typename T>
struct has_has_value : /* implementation */;

template <typename T>
inline constexpr bool has_has_value_v = has_has_value<T>::value;
```

#### `has_value_method` / `has_value_method_v`

Detects if a type has a `value()` method.

```cpp
template <typename T>
struct has_value_method : /* implementation */;

template <typename T>
inline constexpr bool has_value_method_v = has_value_method<T>::value;
```

#### `is_optional_like` / `is_optional_like_v`

Checks if a type behaves like `std::optional`.

```cpp
template <typename T>
struct is_optional_like : std::conjunction<
    has_has_value<T>,
    has_value_method<T>
> {};

template <typename T>
inline constexpr bool is_optional_like_v = is_optional_like<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <optional>

using namespace fat_p;

static_assert(is_optional_like_v<std::optional<int>>);
static_assert(is_optional_like_v<std::optional<std::string>>);
static_assert(!is_optional_like_v<int>);

// Custom optional type
template <typename T>
struct MyOptional {
    T val;
    bool present = false;
    
    bool has_value() const { return present; }
    T& value() { return val; }
    const T& value() const { return val; }
};

static_assert(is_optional_like_v<MyOptional<int>>);

// Generic optional handler
template <typename Opt>
std::enable_if_t<is_optional_like_v<Opt>, void>
process_optional(const Opt& opt) {
    if (opt.has_value()) {
        std::cout << "Value: " << opt.value() << "\n";
    } else {
        std::cout << "No value\n";
    }
}

int main() {
    std::optional<int> opt1 = 42;
    std::optional<int> opt2;
    
    process_optional(opt1);  // Value: 42
    process_optional(opt2);  // No value
    
    return 0;
}
```

### Variant-Like Types

#### `has_index_method` / `has_index_method_v`

Detects if a type has an `index()` method.

```cpp
template <typename T>
struct has_index_method : /* implementation */;

template <typename T>
inline constexpr bool has_index_method_v = has_index_method<T>::value;
```

#### `is_variant_like` / `is_variant_like_v`

Checks if a type behaves like `std::variant`.

```cpp
template <typename T>
struct is_variant_like : has_index_method<T> {};

template <typename T>
inline constexpr bool is_variant_like_v = is_variant_like<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <variant>

using namespace fat_p;

static_assert(is_variant_like_v<std::variant<int, double, std::string>>);
static_assert(!is_variant_like_v<std::optional<int>>);
static_assert(!is_variant_like_v<int>);

// Generic variant visitor
template <typename Var>
std::enable_if_t<is_variant_like_v<Var>, void>
print_variant_index(const Var& v) {
    std::cout << "Active index: " << v.index() << "\n";
}

int main() {
    std::variant<int, double, std::string> v1 = 42;
    std::variant<int, double, std::string> v2 = 3.14;
    std::variant<int, double, std::string> v3 = "hello";
    
    print_variant_index(v1);  // Active index: 0
    print_variant_index(v2);  // Active index: 1
    print_variant_index(v3);  // Active index: 2
    
    return 0;
}
```

---

## Range Traits

Range traits detect range-like properties.

### Basic Range Detection

#### `is_range` / `is_range_v`

Checks if a type can be used as a range (has begin/end). Alias for `is_iterable`.

```cpp
template <typename T>
struct is_range : is_iterable<T> {};

template <typename T>
inline constexpr bool is_range_v = is_range<T>::value;
```

#### `is_sized_range` / `is_sized_range_v`

Checks if a type is a range with `size()`.

```cpp
template <typename T>
struct is_sized_range : std::conjunction<is_iterable<T>, has_size<T>> {};

template <typename T>
inline constexpr bool is_sized_range_v = is_sized_range<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <forward_list>

using namespace fat_p;

static_assert(is_range_v<std::vector<int>>);
static_assert(is_range_v<std::forward_list<int>>);
static_assert(is_range_v<int[10]>);

static_assert(is_sized_range_v<std::vector<int>>);
static_assert(!is_sized_range_v<std::forward_list<int>>);

// Range-based algorithm
template <typename Range>
std::enable_if_t<is_range_v<Range>, void>
print_range(const Range& r) {
    for (const auto& item : r) {
        std::cout << item << " ";
    }
    std::cout << "\n";
}

// Sized range optimization
template <typename Range>
std::enable_if_t<is_sized_range_v<Range>, std::size_t>
process_sized_range(const Range& r) {
    std::cout << "Processing " << r.size() << " elements\n";
    return r.size();
}
```

### String-Like Types

#### `has_c_str` / `has_c_str_v`

Detects if a type has a `c_str()` method.

```cpp
template <typename T>
struct has_c_str : /* implementation */;

template <typename T>
inline constexpr bool has_c_str_v = has_c_str<T>::value;
```

#### `is_string_like` / `is_string_like_v`

Checks if a type behaves like a string (iterable + c_str).

```cpp
template <typename T>
struct is_string_like : std::conjunction<is_iterable<T>, has_c_str<T>> {};

template <typename T>
inline constexpr bool is_string_like_v = is_string_like<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <string>

using namespace fat_p;

static_assert(is_string_like_v<std::string>);
static_assert(!is_string_like_v<std::vector<char>>);
static_assert(!is_string_like_v<const char*>);

// Pass to C API
template <typename StrLike>
std::enable_if_t<is_string_like_v<StrLike>, void>
pass_to_c_function(const StrLike& s) {
    const char* c_str = s.c_str();
    std::cout << "C string: " << c_str << "\n";
}

int main() {
    std::string str = "hello";
    pass_to_c_function(str);
    
    return 0;
}
```

---

## Special Type Categories

Special traits for advanced type categories.

### Aggregate Types (C++17)

#### `is_aggregate` / `is_aggregate_v` (C++17)

Checks if a type is an aggregate type.

```cpp
#if __cplusplus >= 201703L
template <typename T>
struct is_aggregate : std::is_aggregate<T> {};

template <typename T>
inline constexpr bool is_aggregate_v = is_aggregate<T>::value;
#endif
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

struct Aggregate {
    int x;
    double y;
};

struct NonAggregate {
    int x;
    NonAggregate(int x) : x(x) {}
};

static_assert(is_aggregate_v<Aggregate>);
static_assert(!is_aggregate_v<NonAggregate>);

// Aggregate initialization
template <typename T>
std::enable_if_t<is_aggregate_v<T>, T>
make_aggregate(int x, double y) {
    return T{x, y};
}
```

### Array Types

#### `is_bounded_array` / `is_bounded_array_v`

Checks if a type is an array with known bound.

```cpp
template <typename T>
struct is_bounded_array : /* implementation */;

template <typename T>
inline constexpr bool is_bounded_array_v = is_bounded_array<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

static_assert(is_bounded_array_v<int[10]>);
static_assert(is_bounded_array_v<double[5][3]>);
static_assert(!is_bounded_array_v<int[]>);
static_assert(!is_bounded_array_v<int*>);
static_assert(!is_bounded_array_v<std::array<int, 5>>);
```

#### `is_unbounded_array` / `is_unbounded_array_v`

Checks if a type is an array with unknown bound.

```cpp
template <typename T>
struct is_unbounded_array : /* implementation */;

template <typename T>
inline constexpr bool is_unbounded_array_v = is_unbounded_array<T>::value;
```

**Example:**

```cpp
static_assert(is_unbounded_array_v<int[]>);
static_assert(is_unbounded_array_v<double[][10]>);
static_assert(!is_unbounded_array_v<int[10]>);
static_assert(!is_unbounded_array_v<int*>);
```

### Relocatability

#### `is_trivially_relocatable` / `is_trivially_relocatable_v`

Checks if a type can be safely relocated with `memcpy`. Uses `std::is_trivially_copyable` as the implementation, which guarantees that the type has a trivial destructor, trivial copy constructor, and trivial move constructor (if applicable). This is a safe and conservative check for types that can be relocated by simply copying their byte representation.

**Note:** For C++23 and later, this may be replaced with `std::is_trivially_relocatable` when that proposal is standardized.

```cpp
template<typename T>
struct is_trivially_relocatable : std::is_trivially_copyable<T> {};

template<typename T>
inline constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <string>

using namespace fat_p;

static_assert(is_trivially_relocatable_v<int>);
static_assert(is_trivially_relocatable_v<double>);
static_assert(!is_trivially_relocatable_v<std::string>);
static_assert(!is_trivially_relocatable_v<std::vector<int>>);

// Optimized move for trivially relocatable types
template <typename T>
void optimized_move(T* dest, T* src, std::size_t count) {
    if constexpr (is_trivially_relocatable_v<T>) {
        std::memcpy(dest, src, count * sizeof(T));
        std::cout << "Using memcpy for relocation\n";
    } else {
        std::move(src, src + count, dest);
        std::cout << "Using move construction\n";
    }
}
```

---

## Trait Composition

The `trait_ops` namespace provides utilities for combining multiple traits.

### `all_of` / `all_of_v`

Checks if all specified traits are satisfied.

```cpp
namespace trait_ops {
template<template<typename> class... Traits>
struct all_of {
    template<typename T>
    struct apply : std::conjunction<Traits<T>...> {};
};

template<typename T, template<typename> class... Traits>
inline constexpr bool all_of_v = all_of<Traits...>::template apply<T>::value;
}
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>

using namespace fat_p;
using namespace fat_p::trait_ops;

// Check if type is iterable, sized, and hashable
template <typename T>
constexpr bool is_complete_container_v = 
    all_of_v<T, is_iterable, is_sized, is_hashable>;

static_assert(is_complete_container_v<std::vector<int>>);
static_assert(!is_complete_container_v<std::vector<std::vector<int>>>);  // vector not hashable

// Require multiple traits
template <typename Container>
std::enable_if_t<
    all_of_v<Container, is_container, is_reservable, is_contiguous_container>,
    void
>
high_performance_container(Container& c) {
    c.reserve(1000);
    std::cout << "Container supports all performance features\n";
}

int main() {
    std::vector<int> vec;
    high_performance_container(vec);
    
    return 0;
}
```

### `any_of` / `any_of_v`

Checks if any of the specified traits is satisfied.

```cpp
namespace trait_ops {
template<template<typename> class... Traits>
struct any_of {
    template<typename T>
    struct apply : std::disjunction<Traits<T>...> {};
};

template<typename T, template<typename> class... Traits>
inline constexpr bool any_of_v = any_of<Traits...>::template apply<T>::value;
}
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <map>
#include <set>

using namespace fat_p;
using namespace fat_p::trait_ops;

// Type supports subscript or map-like access
template <typename T>
constexpr bool has_indexed_access_v = 
    any_of_v<T, has_subscript, is_map_like>;

static_assert(has_indexed_access_v<std::vector<int>>);
static_assert(has_indexed_access_v<std::map<int, std::string>>);
static_assert(!has_indexed_access_v<std::set<int>>);

// Access element by index or key
template <typename Container, typename Index>
auto get_element(Container& c, Index idx) {
    if constexpr (has_subscript_v<Container>) {
        return c[idx];
    } else if constexpr (is_map_like_v<Container>) {
        return c[idx];
    }
}
```

### `none_of` / `none_of_v`

Checks if a trait is not satisfied.

```cpp
namespace trait_ops {
template<template<typename> class Trait>
struct none_of {
    template<typename T>
    struct apply : std::negation<Trait<T>> {};
};

template<typename T, template<typename> class Trait>
inline constexpr bool none_of_v = none_of<Trait>::template apply<T>::value;
}
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;
using namespace fat_p::trait_ops;

// Ensure type is NOT atomic
template <typename T>
std::enable_if_t<none_of_v<T, is_atomic>, void>
non_atomic_operation(T& value) {
    value = T{};  // Direct assignment, no atomic operation
}
```

---

## Diagnostic Utilities

The `diagnostics` namespace provides compile-time diagnostic helpers.

### `why_not_container`

Explains why a type is not a container.

```cpp
namespace diagnostics {
template<typename T>
struct why_not_container {
    static constexpr const char* reason = /* ... */;
};
}
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <iostream>

using namespace fat_p;

struct NoBegin {
    int* end() { return nullptr; }
    std::size_t size() { return 0; }
};

struct NoEnd {
    int* begin() { return nullptr; }
    std::size_t size() { return 0; }
};

struct NoSize {
    int* begin() { return nullptr; }
    int* end() { return nullptr; }
};

int main() {
    std::cout << "int: " 
              << diagnostics::why_not_container<int>::reason << "\n";
    
    std::cout << "NoBegin: " 
              << diagnostics::why_not_container<NoBegin>::reason << "\n";
    
    std::cout << "NoEnd: " 
              << diagnostics::why_not_container<NoEnd>::reason << "\n";
    
    std::cout << "NoSize: " 
              << diagnostics::why_not_container<NoSize>::reason << "\n";
    
    std::cout << "vector: " 
              << diagnostics::why_not_container<std::vector<int>>::reason << "\n";
    
    return 0;
}
```

### `why_not_hashable`

Explains why a type is not hashable.

```cpp
namespace diagnostics {
template<typename T>
struct why_not_hashable {
    static constexpr const char* reason = /* ... */;
};
}
```

### `why_not_serializable`

Explains why a type is not serializable.

```cpp
namespace diagnostics {
template<typename T>
struct why_not_serializable {
    static constexpr const char* reason = /* ... */;
};
}
```

### `why_not_comparable`

Explains why a type is not comparable.

```cpp
namespace diagnostics {
template<typename T>
struct why_not_comparable {
    static constexpr const char* reason = /* ... */;
};
}
```

### Diagnostic Functions

Runtime-accessible diagnostic functions.

```cpp
template<typename T>
constexpr const char* diagnose_container();

template<typename T>
constexpr const char* diagnose_hashable();

template<typename T>
constexpr const char* diagnose_serializable();

template<typename T>
constexpr const char* diagnose_comparable();
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <iostream>

using namespace fat_p;

struct MyType {
    int value;
};

int main() {
    std::cout << "Why MyType is not a container: "
              << diagnose_container<MyType>() << "\n";
    
    std::cout << "Why MyType is not hashable: "
              << diagnose_hashable<MyType>() << "\n";
    
    return 0;
}
```

---

## Custom Trait Patterns

The library provides powerful patterns for creating custom traits. The recommended approach uses the Detection Idiom with `is_detected`.

### Detecting Member Variables

Use `is_detected` with a helper alias to detect member variables.

```cpp
#include "TypeTraits.h"

using namespace fat_p;

// Define detection helper
template <typename T>
using has_value_t = decltype(&T::value);

// Define trait
template <typename T>
inline constexpr bool has_value_v = is_detected_v<has_value_t, T>;

// Use it
struct WithValue {
    int value;
};

struct WithoutValue {
    int data;
};

static_assert(has_value_v<WithValue>);
static_assert(!has_value_v<WithoutValue>);

// Use in generic code
template <typename T>
auto get_value_if_exists(T& obj) {
    if constexpr (has_value_v<T>) {
        return obj.value;
    } else {
        return 0;
    }
}
```

### Detecting Methods

Use `is_detected` with `decltype` and `std::declval` to detect methods.

```cpp
#include "TypeTraits.h"

using namespace fat_p;

// Define detection helper for reset()
template <typename T>
using has_reset_t = decltype(std::declval<T&>().reset());

// Define trait
template <typename T>
inline constexpr bool has_reset_v = is_detected_v<has_reset_t, T>;

// Use it
struct Resettable {
    void reset() { }
};

struct NotResettable {
};

static_assert(has_reset_v<Resettable>);
static_assert(!has_reset_v<NotResettable>);

// Conditional reset
template <typename T>
void reset_if_possible(T& obj) {
    if constexpr (has_reset_v<T>) {
        obj.reset();
        std::cout << "Reset called\n";
    } else {
        std::cout << "No reset method\n";
    }
}
```

### Detecting Methods with Arguments

For methods with specific arguments, include them in the detection helper.

```cpp
// Define detection helper for process(int)
template <typename T>
using has_process_t = decltype(std::declval<T&>().process(std::declval<int>()));

// Define trait
template <typename T>
inline constexpr bool has_process_v = is_detected_v<has_process_t, T>;

// Use it
struct Processor {
    void process(int x) { }
};

static_assert(has_process_v<Processor>);
```

### Detecting Return Types

You can also check if a method exists AND returns a specific type.

```cpp
// Check if T has size() that returns size_t
template <typename T>
using size_returns_size_t = std::enable_if_t<
    std::is_same_v<decltype(std::declval<T>().size()), std::size_t>
>;

template <typename T>
inline constexpr bool has_size_returning_size_t_v = 
    is_detected_v<size_returns_size_t, T>;
```

---

## Extension Points

Extension points allow users to specialize traits for their own types.

### `custom_traits`

A specialization point for user-defined trait information.

```cpp
namespace extension_points {
template<typename T>
struct custom_traits {
    // Users can add custom trait values here
};
}
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

// Custom type
struct MyCustomType {
    int data;
};

// Specialize custom_traits for MyCustomType
namespace fat_p::extension_points {
    template<>
    struct custom_traits<MyCustomType> {
        static constexpr bool is_relocatable = true;
        static constexpr bool has_custom_hash = true;
        static constexpr bool supports_simd = false;
    };
}

// Use custom traits
template <typename T>
void check_custom_traits() {
    using traits = extension_points::custom_traits<T>;
    
    std::cout << std::boolalpha;
    if constexpr (requires { traits::is_relocatable; }) {
        std::cout << "Is relocatable: " << traits::is_relocatable << "\n";
    }
    if constexpr (requires { traits::has_custom_hash; }) {
        std::cout << "Has custom hash: " << traits::has_custom_hash << "\n";
    }
}

int main() {
    check_custom_traits<MyCustomType>();
    return 0;
}
```

### `library_custom_traits`

Similar to `custom_traits` but for library-specific extensions.

```cpp
namespace extension_points {
template<typename T>
struct library_custom_traits {
};
}
```

---

## Design-by-Contract Helpers

DbC helpers enforce type requirements at compile time with clear error messages.

### Container Requirements

```cpp
template<typename T>
constexpr void requires_iterable();

template<typename T>
constexpr void requires_sized();

template<typename T>
constexpr void requires_container();

template<typename T>
constexpr void requires_contiguous();
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <vector>
#include <list>

using namespace fat_p;

template <typename Container>
void process_container(const Container& c) {
    requires_container<Container>();
    
    std::cout << "Processing container with " << c.size() << " elements\n";
    for (const auto& item : c) {
        std::cout << item << " ";
    }
}

template <typename Container>
void fast_access(const Container& c, std::size_t idx) {
    requires_contiguous<Container>();
    
    // Can use data() pointer
    const auto* ptr = c.data();
    std::cout << "Element at " << idx << ": " << ptr[idx] << "\n";
}

int main() {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    process_container(vec);  // OK
    fast_access(vec, 2);     // OK
    
    // std::list<int> lst = {1, 2, 3};
    // fast_access(lst, 2);  // Compile error with clear message
    
    return 0;
}
```

### Comparison Requirements

```cpp
template<typename T>
constexpr void requires_hashable();

template<typename T>
constexpr void requires_comparable();
```

**Example:**

```cpp
#include "TypeTraits.h"
#include <unordered_set>

using namespace fat_p;

template <typename T>
class HashSet {
public:
    HashSet() {
        requires_hashable<T>();
        requires_comparable<T>();  // For equality
    }
    
private:
    std::unordered_set<T> data;
};

int main() {
    HashSet<int> set1;  // OK
    HashSet<std::string> set2;  // OK
    
    // HashSet<std::vector<int>> set3;  // Compile error: vector not hashable
    
    return 0;
}
```

### Callable Requirements

```cpp
template<typename F, typename... Args>
constexpr void requires_invocable();
```

**Example:**

```cpp
#include "TypeTraits.h"

using namespace fat_p;

template <typename Callback>
void execute_callback(Callback&& cb, int value) {
    requires_invocable<Callback, int>();
    
    cb(value);
}

int main() {
    execute_callback([](int x) { 
        std::cout << "Value: " << x << "\n"; 
    }, 42);
    
    // execute_callback([](double x) { }, 42);  // Compile error: wrong arg type
    
    return 0;
}
```

### Other Requirements

```cpp
template<typename T>
constexpr void requires_allocator();

template<typename T>
constexpr void requires_serializable();
```

### Library-Specific DbC Helpers

```cpp
template<typename T>
constexpr void requires_validate();

template<typename T>
constexpr void requires_expected();

template<typename T>
constexpr void requires_tensor();

template<typename T>
constexpr void requires_parallel_compatible();
```

**Example:**

```cpp
#include "FatPTypeTraits.h"

using namespace fat_p;

template <typename T>
void parallel_process(const T& container) {
    requires_parallel_compatible<T>();
    
    // Can safely use parallel algorithms
    // Implementation...
}
```

---


## Comparison to Other Type Trait Libraries

This section compares fat_penelope TypeTraits to other major C++ type trait libraries and provides guidance on when to use each.

### Feature Comparison Matrix

| Feature | fat_p TypeTraits | std::type_traits (C++17) | Boost.TypeTraits | Boost.Hana | std::ranges (C++20) |
|---------|------------------|--------------------------|------------------|------------|---------------------|
| **Minimum C++ Version** | C++17 | C++11/17 | C++03 | C++14 | C++20 |
| **Dependencies** | None (header-only) | Standard library | None | None | Standard library |
| **Detection Idiom** | âœ… Full support | âŒ No | âœ… Limited | âœ… Yes | âŒ No |
| **Container Traits** | âœ… 40+ traits | âŒ No | âŒ Limited | âŒ No | âœ… Concepts |
| **Comparison Traits** | âœ… Complete | âŒ Basic | âŒ Basic | âœ… Yes | âœ… Concepts |
| **Callable Traits** | âœ… Full (C++17/20) | âœ… C++17+ | âŒ Limited | âœ… Yes | âœ… Concepts |
| **Diagnostic Utilities** | âœ… Extensive | âŒ No | âŒ No | âŒ No | âŒ No |
| **DbC Helpers** | âœ… Yes | âŒ No | âŒ No | âŒ No | âŒ No |
| **Library-Specific Traits** | âœ… FatPTypeTraits | âŒ N/A | âŒ N/A | âŒ N/A | âŒ N/A |
| **C++20 Concepts** | âœ… Full support | âœ… Some | âŒ No | âŒ No | âœ… Core focus |
| **Serialization Traits** | âœ… Yes | âŒ No | âŒ No | âŒ No | âŒ No |
| **Allocator Traits** | âœ… Extended | âœ… Basic | âœ… Basic | âŒ No | âŒ No |
| **Custom Trait Patterns** | âœ… Yes | âŒ No | âŒ No | âŒ No | âŒ No |
| **Compile-time Diagnostics** | âœ… why_not_* | âŒ No | âŒ No | âŒ No | âŒ No |
| **Total Trait Count** | 150+ | ~90 | ~100 | N/A (different paradigm) | ~50 concepts |

### Detailed Library Comparisons

#### vs. `std::type_traits` (C++17)

**std::type_traits Strengths:**
- Part of the standard library (no additional dependencies)
- Well-documented and widely understood
- Stable ABI across compilers
- Fundamental traits (is_integral, is_class, etc.) are definitive

**std::type_traits Weaknesses:**
- No detection idiom support before C++20
- Limited higher-level traits (no container detection)
- No diagnostic utilities
- No library-specific type detection

**When to use std::type_traits:**
- Checking fundamental type properties (is_integral, is_pointer, etc.)
- Standard arithmetic type conversions
- Basic SFINAE operations
- When no additional dependencies are allowed
- When working with very old codebases (C++11/14)

**When to use fat_p TypeTraits instead:**
- Need detection idiom in C++17
- Need container trait detection
- Need diagnostic "why_not" utilities
- Working with generic container algorithms
- Need compile-time contract validation
- Need library-specific type detection

**Integration Strategy:**
```cpp
// Use both together - they complement each other
#include <type_traits>
#include "TypeTraits.h"

template <typename T>
void process(T value) {
    // Use std for fundamental traits
    static_assert(std::is_arithmetic_v<T>, "Must be arithmetic");
    
    // Use fat_p for higher-level traits
    if constexpr (fat_p::is_hashable_v<T>) {
        // Can hash this type
    }
}
```

#### vs. Boost.TypeTraits

**Boost.TypeTraits Strengths:**
- Supports ancient C++ versions (C++03)
- Very stable and battle-tested
- Part of the Boost ecosystem
- Compiler workarounds for old compilers

**Boost.TypeTraits Weaknesses:**
- Requires Boost dependency
- No detection idiom
- Limited modern C++ features
- No diagnostic utilities
- Focused on basic type properties

**When to use Boost.TypeTraits:**
- Must support C++03/C++11
- Already using Boost
- Need compiler workarounds for old compilers
- Working with legacy codebases

**When to use fat_p TypeTraits instead:**
- Using C++17 or later
- Want zero dependencies
- Need modern features (detection idiom, diagnostics)
- Need container/callable traits
- Don't need C++03 compatibility

#### vs. Boost.Hana

**Boost.Hana Strengths:**
- Powerful metaprogramming library
- Type-level computations
- Heterogeneous containers
- Functional programming paradigm
- Runtime and compile-time unified

**Boost.Hana Weaknesses:**
- Requires Boost dependency
- Different programming paradigm (learning curve)
- C++14 minimum
- Heavier compile-time overhead
- Not focused on simple type queries

**When to use Boost.Hana:**
- Need heterogeneous containers (tuple-like operations)
- Doing extensive type-level computations
- Prefer functional programming style
- Need type-level algorithms
- Building complex metaprograms

**When to use fat_p TypeTraits instead:**
- Simple type queries (does type have begin()?)
- Container trait detection
- C++17 codebase with no Boost
- Want lightweight, focused library
- Need specific diagnostic utilities
- Simpler, more direct API preferred

**Comparison Example:**
```cpp
// Boost.Hana approach (functional, type-level)
auto tuple = hana::make_tuple(1, 'x', 3.4);
hana::for_each(tuple, [](auto x) { 
    std::cout << x << "\n"; 
});

// fat_p TypeTraits approach (type queries)
template <typename T>
void process(const T& container) {
    if constexpr (fat_p::is_container_v<T>) {
        std::cout << "Size: " << container.size() << "\n";
    }
}
```

#### vs. `std::ranges` Concepts (C++20)

**std::ranges Strengths:**
- Part of C++20 standard
- First-class concept support
- Integrated with standard algorithms
- Iterator and range concepts
- Strong compiler support

**std::ranges Weaknesses:**
- C++20 required
- Focused on ranges/iterators
- No detection idiom
- No diagnostic utilities
- No serialization/allocator traits
- No library-specific traits

**When to use std::ranges:**
- C++20 or later codebase
- Working with ranges and views
- Using standard algorithms
- Need standard concept vocabulary
- Iterator transformations

**When to use fat_p TypeTraits:**
- C++17 codebase (can't use C++20 yet)
- Need detection idiom
- Need diagnostic utilities
- Need broader trait coverage
- Need library-specific type detection
- Need serialization/allocator traits

**Integration Strategy (C++20):**
```cpp
#include <ranges>
#include "TypeTraits.h"

// Use std::ranges concepts for ranges
template <std::ranges::range R>
void process_range(R&& r) {
    // Use fat_p for additional checks
    if constexpr (fat_p::is_reservable_v<std::remove_cvref_t<R>>) {
        // Can reserve capacity
    }
}

// Combine concepts
template <typename T>
    requires std::ranges::range<T> && fat_p::is_hashable_v<std::ranges::range_value_t<T>>
void process_hashable_range(const T& range) {
    // Range with hashable elements
}
```

### Decision Framework: When to Use What

#### Use fat_p TypeTraits as PRIMARY library when:

1. **C++17 Project**
   - Need modern features but can't use C++20 yet
   - Detection idiom required
   - Container trait detection needed

2. **Generic Container Code**
   - Writing algorithms that adapt to container capabilities
   - Need to detect reserve(), data(), etc.
   - Working with mixed container types

3. **Zero Dependencies Requirement**
   - Can't use Boost
   - Header-only solution needed
   - Minimal footprint preferred

4. **Need Diagnostics**
   - Development phase needing clear error messages
   - Want "why_not_*" explanations
   - Teaching/documentation purposes

5. **Library-Specific Integration**
   - Using fat_penelope containers
   - Need SmallVector, Tensor, etc. detection
   - Integration with library ecosystem

#### Use std::type_traits as PRIMARY library when:

1. **Fundamental Type Queries**
   - Checking is_integral, is_pointer, etc.
   - Standard type conversions
   - Basic SFINAE

2. **Maximum Portability**
   - Must work everywhere
   - No additional dependencies allowed
   - Standard compliance critical

3. **Simple Projects**
   - Basic type checking sufficient
   - No need for advanced features
   - Minimal overhead preferred

#### Use Boost.TypeTraits when:

1. **Legacy C++ Support**
   - C++03/C++11 required
   - Old compiler support needed
   - Boost already in project

2. **Boost Ecosystem**
   - Using other Boost libraries
   - Consistent Boost style preferred
   - Boost dependency acceptable

#### Use Boost.Hana when:

1. **Metaprogramming Heavy**
   - Extensive type-level computations
   - Heterogeneous containers needed
   - Type-level algorithms required

2. **Functional Style**
   - Prefer functional paradigm
   - Need type-level map/filter/fold
   - Runtime/compile-time unification

#### Use std::ranges (C++20+) when:

1. **C++20 Available**
   - Can use C++20 features
   - Range-based operations
   - Standard algorithm integration

2. **Iterator/Range Focus**
   - Working primarily with ranges
   - Views and transformations
   - Standard concepts sufficient

### Hybrid Approach (Recommended)

For most modern C++17/20 projects, use a combination:

```cpp
#include <type_traits>        // Fundamental traits
#include "TypeTraits.h"        // Advanced detection
#include <ranges>              // C++20: Range concepts

template <typename Container>
    requires std::ranges::range<Container>  // C++20 concept
void advanced_process(Container& c) {
    // Fundamental check
    using value_t = std::ranges::range_value_t<Container>;
    static_assert(std::is_arithmetic_v<value_t>);
    
    // Advanced fat_p check with diagnostics
    if constexpr (!fat_p::is_reservable_v<Container>) {
        // Diagnostic available: why_not_reservable<Container>::reason
    } else {
        c.reserve(1000);
    }
    
    // Library-specific optimization
    if constexpr (fat_p::is_small_vector_v<Container>) {
        // SmallVector-specific optimization
    }
}
```

### Migration Strategies

#### Migrating from std::type_traits to fat_p

```cpp
// Before: Limited capability
template <typename T>
std::enable_if_t<std::is_class_v<T>, void>
old_function(const T& obj) {
    // Can only check if it's a class
}

// After: Rich trait checking
template <typename T>
std::enable_if_t<fat_p::is_container_v<T>, void>
new_function(const T& container) {
    // Know it's iterable AND sized
    for (const auto& item : container) {
        // Process
    }
}
```

#### Migrating from Boost.TypeTraits

```cpp
// Before: Boost dependency
#include <boost/type_traits.hpp>

template <typename T>
typename boost::enable_if<boost::is_integral<T>, void>::type
old_boost_function(T value) { }

// After: No dependency, modern C++
#include "TypeTraits.h"

template <typename T>
std::enable_if_t<std::is_integral_v<T>, void>  // Use std for fundamental
new_function(T value) { }

template <typename T>
std::enable_if_t<fat_p::is_container_v<T>, void>  // Use fat_p for advanced
container_function(const T& c) { }
```

#### Adding fat_p to C++20 Projects

```cpp
// Complement C++20 concepts with fat_p traits
#include <concepts>
#include "TypeTraits.h"

// C++20 concept for basic requirement
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// fat_p for detailed capability detection
template <typename Container>
    requires std::ranges::range<Container>
void process(Container& c) {
    using value_t = std::ranges::range_value_t<Container>;
    
    // Use C++20 concept
    static_assert(Numeric<value_t>);
    
    // Use fat_p for capabilities
    if constexpr (fat_p::is_reservable_v<Container>) {
        c.reserve(c.size() * 2);
    }
    
    // Use fat_p diagnostics in development
    static_assert(fat_p::is_hashable_v<value_t>,
        fat_p::diagnostics::why_not_hashable<value_t>::reason);
}
```

### Summary Recommendations

| Project Scenario | Primary Library | Supplementary Libraries |
|-----------------|-----------------|------------------------|
| **C++17, Generic Algorithms** | fat_p TypeTraits | std::type_traits (fundamentals) |
| **C++20, Modern Codebase** | std::ranges + fat_p | std::type_traits |
| **Legacy C++11/14** | std::type_traits | Boost.TypeTraits if needed |
| **C++03 Required** | Boost.TypeTraits | None |
| **Metaprogramming Heavy** | Boost.Hana | fat_p for type queries |
| **fat_penelope Ecosystem** | FatPTypeTraits + fat_p | std::type_traits |
| **Maximum Portability** | std::type_traits only | None |
| **Rich Diagnostics Needed** | fat_p TypeTraits | std::type_traits |

**Key Principle**: Libraries are not mutually exclusive. Mix and match based on strengths:
- **std::type_traits**: Fundamental type properties
- **fat_p TypeTraits**: Detection, containers, diagnostics
- **std::ranges** (C++20): Range/iterator concepts
- **Boost.Hana**: Type-level algorithms
- **Boost.TypeTraits**: Legacy support

---

## Performance Considerations

### Compile-Time Evaluation

All traits in this library are evaluated at compile time:

```cpp
// This produces no runtime code
if constexpr (is_container_v<T>) {
    // Branch selected at compile time
}

// This check happens at compile time
static_assert(is_hashable_v<int>, "int must be hashable");
```

### Zero Runtime Overhead

Type traits have absolutely zero runtime cost. They are template metaprograms that execute during compilation:

```cpp
// No runtime branching - completely optimized away
template <typename Container>
void process(Container& c) {
    if constexpr (is_reservable_v<Container>) {
        c.reserve(1000);  // Only compiled if Container has reserve()
    }
    // Rest of implementation
}
```

### Caching Trait Results

For complex trait queries used multiple times, consider caching:

```cpp
template <typename Container>
class Processor {
    // Cache complex trait computations
    static constexpr bool has_fast_access = 
        is_contiguous_container_v<Container> && 
        is_random_accessible_v<Container>;
    
public:
    void process() {
        if constexpr (has_fast_access) {
            // Use cached result
        }
    }
};
```

---

## Best Practices

### 1. Use `_v` Suffix for Variable Templates

Always prefer the `_v` suffix for cleaner code:

```cpp
// Preferred
if constexpr (is_container_v<T>) { }

// Avoid
if constexpr (is_container<T>::value) { }
```

### 2. Combine with `static_assert` for Clear Errors

Use `static_assert` to provide helpful error messages:

```cpp
template <typename T>
void serialize(const T& obj) {
    static_assert(is_serializable_v<T>, 
        "Type T must implement serialize() and deserialize() methods");
    // Implementation
}
```

### 3. Use DbC Helpers for Requirements

DbC helpers provide standardized error messages:

```cpp
template <typename Container>
void process(const Container& c) {
    requires_container<Container>();  // Clear error if not a container
    // Implementation
}
```

### 4. Prefer `if constexpr` Over SFINAE When Possible

`if constexpr` (C++17) is clearer than SFINAE:

```cpp
// Preferred (C++17)
template <typename T>
void func(T& obj) {
    if constexpr (has_reset_v<T>) {
        obj.reset();
    }
}

// Older style (still valid)
template <typename T>
std::enable_if_t<has_reset_v<T>, void>
func(T& obj) {
    obj.reset();
}
```

### 5. Use Trait Composition for Complex Checks

Combine traits for readability:

```cpp
// Define composite trait
template <typename T>
constexpr bool is_efficient_container_v = 
    is_container_v<T> && 
    is_reservable_v<T> && 
    is_contiguous_container_v<T>;

// Use in code
template <typename T>
std::enable_if_t<is_efficient_container_v<T>, void>
optimize(T& container) {
    // Implementation
}
```

### 6. Document Type Requirements

Always document what traits your templates require:

```cpp
/**
 * @brief Sorts a container in place
 * @tparam Container Must satisfy: is_random_accessible_v
 * @tparam Container::value_type Must satisfy: is_comparable_v
 */
template <typename Container>
void sort(Container& c) {
    requires_container<Container>();
    static_assert(is_random_accessible_v<Container>, 
        "Container must support random access for sorting");
    // Implementation
}
```

### 7. Use Extension Points for Library Types

When defining library types, specialize traits:

```cpp
// In your library
template <typename T>
class MyContainer {
    // Implementation
};

// Specialize traits
template <typename T>
struct is_container<MyContainer<T>> : std::true_type {};

template <typename T>
struct is_reservable<MyContainer<T>> : std::true_type {};
```

### 8. Leverage Diagnostic Utilities During Development

Use diagnostic helpers to understand why traits fail:

```cpp
#include <iostream>
#include "TypeTraits.h"

struct MyType {
    int value;
};

int main() {
    // During development, understand why traits fail
    std::cout << diagnose_container<MyType>() << "\n";
    std::cout << diagnose_hashable<MyType>() << "\n";
    return 0;
}
```

### 9. Use Concepts in C++20

If using C++20, prefer concepts over traits:

```cpp
#if __cplusplus >= 202002L
#include <concepts>

template <typename T>
concept Container = is_container_v<T>;

template <Container C>
void process(const C& c) {
    // Implementation
}
#endif
```

### 10. Test Trait Behavior with `static_assert`

Verify trait behavior in your tests:

```cpp
// In tests
static_assert(is_container_v<std::vector<int>>);
static_assert(is_container_v<std::map<int, int>>);
static_assert(!is_container_v<int>);
static_assert(!is_container_v<std::forward_list<int>>);  // No size()
```

---

## C++20 Concepts

When compiling with C++20 or later (detected via `__cplusplus >= 202002L`), TypeTraits.h provides native concept definitions for all major type traits. These concepts provide cleaner syntax and better error messages than SFINAE-based approaches.

### Available Concepts

The library provides C++20 concepts for all major categories:

#### Container Concepts

```cpp
namespace fat_p {
    template<typename T>
    concept is_iterable = requires(T val) {
        { std::begin(val) } -> std::input_or_output_iterator;
        { std::end(val) } -> std::input_or_output_iterator;
    };

    template<typename T>
    concept is_sized = requires(T val) {
        { val.size() } -> std::integral;
    };

    template<typename T>
    concept is_container = is_iterable<T> && is_sized<T>;
    
    template<typename T>
    concept is_reverse_iterable = requires(T val) {
        { std::rbegin(val) } -> std::input_or_output_iterator;
        { std::rend(val) } -> std::input_or_output_iterator;
    };
}
```

#### Using Container Concepts

```cpp
#include "TypeTraits.h"
#include <vector>
#include <iostream>

// C++20: Direct concept usage
template <fat_p::is_container Container>
void print_size(const Container& c) {
    std::cout << "Size: " << c.size() << "\n";
}

// C++20: Concept in requires clause
template <typename Container>
    requires fat_p::is_container<Container>
void process(const Container& c) {
    for (const auto& item : c) {
        // Process
    }
}

// C++20: Abbreviated function template
void display(fat_p::is_iterable auto const& container) {
    for (const auto& item : container) {
        std::cout << item << " ";
    }
}
```

### Concept Benefits

Concepts provide several advantages over traditional SFINAE:

1. **Better Error Messages**: Constraint violations are reported clearly
2. **Cleaner Syntax**: No need for `std::enable_if_t`
3. **Overload Resolution**: Concepts participate in overload resolution
4. **Subsumption**: More specific concepts automatically preferred

```cpp
// C++20 example showing concept benefits
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

template <typename T>
concept FloatingPoint = Numeric && std::is_floating_point_v<T>;

// More specific concept is automatically preferred
void compute(Numeric auto x) {
    std::cout << "Generic numeric computation\n";
}

void compute(FloatingPoint auto x) {
    std::cout << "Floating-point optimized computation\n";
}

int main() {
    compute(42);    // Calls Numeric version
    compute(3.14);  // Calls FloatingPoint version (more specific)
}
```

### C++17 vs C++20 Comparison

```cpp
// C++17: SFINAE-based approach
template <typename Container>
std::enable_if_t<fat_p::is_container_v<Container>, void>
old_style_process(const Container& c) {
    // Implementation
}

// C++20: Concept-based approach
template <fat_p::is_container Container>
void new_style_process(const Container& c) {
    // Implementation
}

// C++20: Even cleaner with abbreviated template
void newest_style_process(fat_p::is_container auto const& c) {
    // Implementation
}
```

---

## Library-Specific Traits (FatPTypeTraits)

For comprehensive documentation on fat_penelope library-specific type traits, please see the dedicated manual:

**[FatPTypeTraits User Manual](FatPTypeTraits_UserManual.md)**

The `FatPTypeTraits.h` header provides type traits specifically for detecting fat_penelope library components including:

- **Container Detection**: SmallVector, CircularBuffer, FlatMap, FlatSet, SortedContainer, SparseSet, SlotMap
- **Tensor Detection**: Tensor, FixedTensor, CSRMatrix, SimdVector
- **Concurrent Types**: LockFreeQueue, LockFreeRingBuffer, ThreadPool, AtomicReference
- **Utility Types**: Expected, StrongId, ValueGuard, ScopeGuard
- **Composite Traits**: is_library_container, is_tensor_type, is_small_buffer_optimized
- **Duck-Typed Detection**: is_expected_like, is_tensor_like, is_binary_serializable
- **C++20 Concepts**: Full concept support for all library types
- **Diagnostics**: Specialized diagnostic utilities for library types
- **DbC Helpers**: Contract enforcement for library-specific requirements

### Quick Example

```cpp
#include "TypeTraits.h"        // General traits
#include "FatPTypeTraits.h"    // Library-specific traits
#include "SmallVector.h"

template <typename Container>
void process(Container& c) {
    // Use general trait
    if constexpr (fat_p::is_container_v<Container>) {
        std::cout << "Is a container\n";
        
        // Use library-specific trait for optimization
        if constexpr (fat_p::is_small_vector_v<Container>) {
            std::cout << "SmallVector optimization enabled\n";
        }
    }
}
```

For complete documentation, examples, best practices, and integration patterns, see **[FatPTypeTraits User Manual](FatPTypeTraits_UserManual.md)**.

---

## Conclusion

The fat_penelope TypeTraits library provides a comprehensive toolkit for compile-time type introspection. By mastering these traits, you can write more generic, efficient, and maintainable C++ code.

Key takeaways:

1. **Zero Runtime Cost**: All trait evaluations happen at compile time
2. **Comprehensive Coverage**: Over 150 traits covering all common type properties
3. **C++17 Compatible**: Works with C++17 and later, with optional C++20/23 enhancements
4. **C++20 Concepts**: Full concept support when using C++20 or later
5. **Design-by-Contract**: Built-in DbC helpers for clear compile-time validation
6. **Extensible**: Extension points and macros for custom traits
7. **Diagnostic Friendly**: Detailed diagnostic utilities explain why traits fail
8. **Library-Specific Traits**: Dedicated type detection for fat_penelope components

### Documentation

- **TypeTraits.h**: Core type traits and general-purpose traits (this manual)
- **[FatPTypeTraits.h](FatPTypeTraits_UserManual.md)**: Library-specific traits for fat_penelope containers

### Quick Start

```cpp
// Include general traits
#include "TypeTraits.h"

// For library-specific detection
#include "FatPTypeTraits.h"

// Use together for powerful generic programming
template <typename Container>
void process(Container& c) {
    // General trait from TypeTraits.h
    if constexpr (fat_p::is_container_v<Container>) {
        // Library-specific trait from FatPTypeTraits.h
        if constexpr (fat_p::is_small_vector_v<Container>) {
            // Optimized path
        }
    }
}
```

Happy metaprogramming!
