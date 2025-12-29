# FatPTypeTraits: A Fat-P Library Showcase

## Executive Summary

FatPTypeTraits is an **extended type traits library** providing detection idioms, concept emulation, and SFINAE utilities beyond what `<type_traits>` offers. Unlike scattered `std::void_t` tricks (verbose, repetitive) or C++20 concepts on C++17 (unavailable), FatPTypeTraits provides **ready-to-use detection traits** like `has_method_X`, `is_iterable`, `is_hashable` with automatic fallback between concepts (C++20) and SFINAE (C++17). The traits enable compile-time polymorphism without inheritance overhead.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The SFINAE boilerplate explosion
template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_begin : std::false_type {};

template <typename T>
struct has_begin<T, std::void_t<decltype(std::declval<T>().begin())>>
    : std::true_type {};

// Repeat for every method you want to detect...
// has_end, has_push_back, has_insert, has_emplace...
// 20+ lines per trait!

// The detection usage is also verbose
template<typename T>
auto stringify(const T& val) -> std::enable_if_t<has_to_string<T>::value, std::string> {
    return val.to_string();
}
```

| Issue | HPC Impact |
|-------|------------|
| Boilerplate per trait | 15-20 lines for each detection |
| No C++17/20 abstraction | Different code for concepts vs. SFINAE |
| Error messages | SFINAE failures produce cryptic errors |
| Missing detectors | `<type_traits>` lacks method detection |

### The Standard's Limitation

`<type_traits>` provides type property queries but:
- No method detection (`has_size`, `has_begin`)
- No concept emulation for C++17
- No detection idiom helpers
- No composite traits (`is_iterable` = has_begin + has_end)

C++20 concepts help but aren't available in C++17 codebases.

---

## Architecture: Detection Idiom with Concept Fallback

### The Mechanism: Unified Detection

```cpp
// Primary template: false
template <typename T, typename = void>
struct is_iterable : std::false_type {};

// Specialization: true if valid
template <typename T>
struct is_iterable<T, std::void_t<
    decltype(std::begin(std::declval<T&>())),
    decltype(std::end(std::declval<T&>()))
>> : std::true_type {};

// Convenient variable template
template <typename T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;

// C++20 concept version (when available)
#if FATP_HAS_CPP20
template <typename T>
concept Iterable = requires(T& t) {
    std::begin(t);
    std::end(t);
};
#endif
```

**Unified usage:**
```cpp
// Works on C++17 (SFINAE) and C++20 (concepts)
template<typename T>
void process(const T& container) {
    if constexpr (is_iterable_v<T>) {
        for (const auto& elem : container) {
            handle(elem);
        }
    } else {
        handle(container);
    }
}
```

### Method Detection Macro

```cpp
// Generate has_X traits with minimal boilerplate
#define FATP_DEFINE_HAS_METHOD(method_name)                            \
    template <typename T, typename = void>                             \
    struct has_##method_name : std::false_type {};                     \
                                                                        \
    template <typename T>                                              \
    struct has_##method_name<T, std::void_t<                           \
        decltype(std::declval<T>().method_name())                      \
    >> : std::true_type {};                                            \
                                                                        \
    template <typename T>                                              \
    inline constexpr bool has_##method_name##_v = has_##method_name<T>::value;

// Usage
FATP_DEFINE_HAS_METHOD(size)
FATP_DEFINE_HAS_METHOD(empty)
FATP_DEFINE_HAS_METHOD(begin)
FATP_DEFINE_HAS_METHOD(to_string)
```

---

## Feature Inventory

### 1. Container Detection

```cpp
// Check for iterability
static_assert(is_iterable_v<std::vector<int>>);
static_assert(is_iterable_v<std::string>);
static_assert(!is_iterable_v<int>);

// Check for specific container traits
static_assert(is_container_v<std::vector<int>>);
static_assert(is_associative_v<std::map<int, int>>);
static_assert(is_sequence_v<std::vector<int>>);
```

### 2. Method Detection

```cpp
// Built-in detectors
static_assert(has_size_v<std::vector<int>>);
static_assert(has_push_back_v<std::vector<int>>);
static_assert(!has_push_back_v<std::array<int, 5>>);

static_assert(has_to_string_v<MyClass>);  // Has .to_string() method
static_assert(has_ostream_operator_v<MyClass>);  // Has operator<<
```

### 3. Callable Detection

```cpp
static_assert(is_invocable_v<decltype(foo), int, double>);
static_assert(is_invocable_r_v<int, decltype(bar), float>);

// Function signature matching
static_assert(is_callable_with_v<Handler, int, std::string>);
```

### 4. Smart Pointer Detection

```cpp
static_assert(is_smart_pointer_v<std::unique_ptr<int>>);
static_assert(is_smart_pointer_v<std::shared_ptr<int>>);
static_assert(!is_smart_pointer_v<int*>);

// Element type extraction
using elem_t = smart_pointer_element_t<std::unique_ptr<Widget>>;  // Widget
```

### 5. Type Comparison Utilities

```cpp
// Check if any of types match
static_assert(is_any_of_v<int, float, int, double>);  // true
static_assert(!is_any_of_v<char, float, int, double>);  // false

// Check if all types match predicate
static_assert(all_of_v<std::is_integral, int, long, short>);  // true

// None of
static_assert(none_of_v<std::is_pointer, int, float, double>);  // true
```

### 6. Streamable Detection

```cpp
template<typename T>
auto log(const T& val) {
    if constexpr (is_streamable_v<T>) {
        std::cout << val;
    } else if constexpr (has_to_string_v<T>) {
        std::cout << val.to_string();
    } else {
        std::cout << "[unstringifiable]";
    }
}
```

### 7. SFINAE Helpers

```cpp
// enable_if shortcuts
template<typename T, FATP_REQUIRES(is_iterable_v<T>)>
void process(const T& container);

// Cleaner than std::enable_if_t<is_iterable_v<T>, int> = 0
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::void_t Manual | Why Not C++20 Concepts | Why Not Boost.TypeTraits | Fat-P Advantage |
|----------------|---------------------------|----------------------|-------------------------|-----------------|
| C++17 support | ✅ Works but verbose | ❌ C++20 only | ✅ Works | ✅ Works |
| Ready-to-use detectors | ❌ Write each one | ✅ Built-in | ✅ Some | ✅ Comprehensive |
| Zero dependencies | ✅ Standard | ✅ Standard | ❌ Requires Boost | ✅ Header-only |
| Concept fallback | ❌ Manual | N/A | ❌ No concepts | ✅ Automatic |

**The Sweet Spot:** FatPTypeTraits provides ready-to-use detection traits with automatic C++17/20 compatibility, without Boost dependency.

---

## The "Forever Stuck" Reality

**Standard Reality:** `<type_traits>` will not gain method detection:
- Too many possible methods to standardize
- Detection idiom is "user-space" by design
- Concepts don't eliminate need for SFINAE in C++17 codebases

FatPTypeTraits provides the detection traits `<type_traits>` lacks and will continue to lack.

---

## Performance Characteristics

| Aspect | Impact |
|--------|--------|
| Runtime cost | 0 ns (compile-time only) |
| Compile time | ~1-5 ms per trait instantiation |
| Binary size | 0 bytes (type traits don't generate code) |
| Error messages | Cleaner with `if constexpr` than raw SFINAE |

### Where Fat-P Wins
- Generic libraries needing method detection
- C++17 codebases wanting concept-style programming
- Reducing SFINAE boilerplate

### Where Fat-P Loses (Honesty Builds Trust)
- C++20 available → native concepts are cleaner
- Single detection needed → inline `std::void_t` may be simpler
- Boost already in project → Boost.TypeTraits is comprehensive

---

## Integration Points

```
FatPTypeTraits.h
    ↓ uses
CppStandardDetection.h  (C++17/20 detection)
TypeTraits.h            (Base type utilities)
    ↓ used by
Stringify.h             (Method detection for formatting)
JsonLite.h              (Serializable detection)
Signal.h                (Callable detection)
PipeOperator.h          (Return type detection)
```

---

## Final Assessment

FatPTypeTraits delivers on the fat_p promise through three pillars:

### 1. Permanence
`<type_traits>` will never include method detection—too domain-specific. FatPTypeTraits provides these permanently.

### 2. Specialization
Ready-to-use detectors for containers, streams, methods, and callables eliminate SFINAE boilerplate. C++20 concept fallback ensures optimal code on both standards.

### 3. Control
Macro-based trait generation lets you add custom detectors. `FATP_REQUIRES` macro provides clean enable_if syntax. You choose the detection granularity.

**Architectural Verdict:** FatPTypeTraits transforms type detection from **verbose SFINAE boilerplate** to **ready-to-use traits**, enabling compile-time polymorphism with clean, portable code.

---

*FatPTypeTraits.h — Fat-P Library*
