# Stringify: A Fat-P Library Showcase

## Executive Summary

Stringify is a **compile-time method-detection** type-to-string conversion system that automatically selects the optimal conversion path for any type through C++20 concepts-based introspection. Unlike `std::to_string()` (integers and floats only) or manual `operator<<` overloads (verbose, scattered), Stringify detects and prioritizes `.toString()`, `.to_string()`, `operator<<`, and ADL `to_string()` at compile time with **zero runtime dispatch**. This provides `printf`-style convenience with type safety and extensibility that the standard library will never offer.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The verbose manual approach
template<typename T>
std::string to_string_manual(const T& value) {
    if constexpr (std::is_same_v<T, int>) {
        return std::to_string(value);
    } else if constexpr (std::is_same_v<T, double>) {
        return std::to_string(value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (has_to_string_method<T>::value) {
        return value.to_string();
    } else if constexpr (is_streamable<T>::value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    } else {
        static_assert(always_false<T>::value, "No conversion");
    }
}
// Repeat for every project. Miss edge cases. Forget containers.
```

| Issue | HPC Impact |
|-------|------------|
| No container support | `std::to_string(vector)` doesn't compile |
| No custom type support | User types require manual operator<< |
| No optional/variant support | Modern vocabulary types ignored |
| Scattered implementations | Every project reinvents this wheel |
| No formatting control | Fixed precision, no scientific notation |

### The Standard's Limitation

`std::to_string()` converts **only** integers and floating-point numbers. C++20's `std::format` improves formatting but still requires explicit format strings and doesn't auto-detect conversion methods.

```cpp
// std::to_string limitations
std::to_string(42);        // "42" ✓
std::to_string(3.14);      // "3.140000" ✓
std::to_string("hello");   // Compile error!
std::to_string(vec);       // Compile error!
std::to_string(optional);  // Compile error!
```

Stringify converts **any** type with a detectable conversion path.

---

## Architecture: Compile-Time Method Detection

### The Mechanism: C++20 Concepts Priority Chain

```cpp
template<typename T>
std::string toString(T&& value) {
    using PlainT = std::decay_t<T>;
    
    // Priority 1: std::string passthrough
    if constexpr (std::same_as<PlainT, std::string>) {
        return std::forward<T>(value);
    }
    // Priority 2: .toString() method
    else if constexpr (concepts::has_to_string_method<PlainT>) {
        return value.toString();
    }
    // Priority 3: .to_string() method
    else if constexpr (concepts::has_to_string_snake_method<PlainT>) {
        return value.to_string();
    }
    // Priority 4: Streamable via operator<<
    else if constexpr (concepts::streamable<PlainT>) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
    // Priority 5: Printable ranges (containers)
    else if constexpr (concepts::printable_range<PlainT>) {
        return detail::stringifyContainer(value, opts);
    }
    else {
        return opts.placeholder;
    }
}
```

**Detection via C++20 Concepts:**
```cpp
template <typename T>
concept has_to_string_method = requires(const T& t) {
    { t.toString() } -> std::convertible_to<std::string>;
};

template <typename T>
concept streamable = requires(std::ostream& os, const T& t) {
    { os << t } -> std::same_as<std::ostream&>;
};
```

**Why priority matters:** A type with both `.toString()` and `operator<<` uses `.toString()`—it's the explicit intent. The standard `operator<<` is often verbose for debugging.

---

## Feature Inventory

### 1. Automatic Method Detection

```cpp
struct CustomType {
    std::string toString() const { return "custom"; }
};

struct LegacyType {
    friend std::ostream& operator<<(std::ostream& os, const LegacyType& l) {
        return os << "legacy";
    }
};

toString(CustomType{});  // Uses .toString() → "custom"
toString(LegacyType{});  // Uses operator<< → "legacy"
toString(42);            // Uses std::to_string → "42"
```

**Mechanism:** `if constexpr` chains check concepts at compile time. Only the matching branch is instantiated.

### 2. Container Support

```cpp
std::vector<int> v = {1, 2, 3};
toString(v);  // "[1, 2, 3]"

std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
toString(m);  // "{a: 1, b: 2}"

std::set<double> s = {1.1, 2.2, 3.3};
toString(s);  // "{1.1, 2.2, 3.3}"
```

**Mechanism:** `concepts::printable_range<T>` detects `begin()`/`end()`. Map-like containers (with `key_type`/`mapped_type`) use `key: value` format.

### 3. Nested Container Support

```cpp
std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}};
toString(nested);  // "[[1, 2], [3, 4]]"

std::map<std::string, std::vector<int>> complex = {
    {"evens", {2, 4, 6}},
    {"odds", {1, 3, 5}}
};
toString(complex);  // "{evens: [2, 4, 6], odds: [1, 3, 5]}"
```

**Mechanism:** Recursive `toString()` calls handle nesting. Recursion depth is limited (100 debug, 200 release) to prevent stack overflow on cyclic structures.

### 4. Tuple/Pair/Optional Support

```cpp
std::pair<int, std::string> p = {42, "hello"};
toString(p);  // "(42, hello)"

std::tuple<int, double, std::string> t = {1, 2.5, "three"};
toString(t);  // "(1, 2.5, three)"

std::optional<int> opt = 42;
toString(opt);  // "42"

std::optional<int> empty;
toString(empty);  // "nullopt"
```

**Mechanism:** `std::tuple_size_v<T>` detection triggers tuple expansion via `std::apply`. Optional types check `has_value()`.

### 5. Formatting Options

```cpp
StringifyOptions opts;
opts.float_precision = 3;
opts.scientific_notation = true;

toString(3.14159, opts);    // "3.14e+00"
toString("hello", opts);    // "hello"
```

**Mechanism:** Options struct passed by const reference. Defaults provide sensible behavior; customization available when needed.

### 6. Enum Support via Specialization

```cpp
enum class Color { Red, Green, Blue };

// User provides specialization (returns const char*, nullptr for unknown)
template<>
struct EnumStringifier<Color> {
    static const char* to_string(Color c) {
        switch (c) {
            case Color::Red: return "Red";
            case Color::Green: return "Green";
            case Color::Blue: return "Blue";
        }
        return nullptr;  // Falls back to integer representation
    }
};

toString(Color::Green);  // "Green" (not "1")
```

**Mechanism:** `EnumStringifier<T>::to_string()` is checked first; if it returns `nullptr`, falls back to integer conversion.

---

## Why Not Alternatives?

| If You Need... | Why Not std::to_string | Why Not operator<< | Why Not std::format | Fat-P Advantage |
|----------------|----------------------|-------------------|---------------------|-----------------|
| Container support | ❌ Not supported | Manual per type | ❌ Not automatic | ✅ Automatic |
| Custom type detection | ❌ Fixed types | ✅ User-defined | Manual formatters | ✅ Auto-detect methods |
| Nested structures | ❌ Not supported | Manual recursion | Manual | ✅ Recursive |
| Zero overhead | ✅ Direct | std::ostringstream | ✅ Direct | ✅ if constexpr |
| Tuple/optional | ❌ Not supported | Manual | Partial | ✅ Full support |

**The Sweet Spot:** Stringify is the only option combining:
- ✅ Automatic method detection (toString, to_string, operator<<)
- ✅ Container/tuple/optional support
- ✅ Recursive nested structure handling
- ✅ Compile-time path selection (zero runtime dispatch)
- ✅ Extensible via traits

---

## The "Forever Stuck" Reality

**Standard Library Reality:** The committee will not add universal stringification because:

1. **Method naming:** No consensus on `toString()` vs `to_string()` vs `str()`
2. **Container format:** No universal agreement on `[a, b]` vs `{a, b}` vs `(a, b)`
3. **Performance tradeoffs:** Stream-based vs direct conversion vs format strings

Each project has different preferences. Stringify provides sensible defaults with trait-based customization, avoiding the committee's endless bikeshedding.

---

## Performance Characteristics

### Benchmark Results (Release Build, i7-8850H @ 2.60GHz)

| Type | Time | Mechanism |
|------|------|-----------|
| `bool` | ~10 ns | Direct string literal |
| `int` | ~50 ns | `std::to_string` |
| `double` | ~400 ns | `std::to_string` (default precision) |
| `std::string` | ~5 ns | Copy/return |
| `vector<int, 10>` | ~600 ns | Iteration + concat |
| Custom with `.toString()` | Method time + ~5 ns | Direct method call |

### Where Fat-P Wins

- **Debug logging:** Quick stringification of any type
- **Serialization helpers:** Convert complex structures to strings
- **Test assertions:** Display expected/actual values
- **Configuration dumps:** Render nested config as readable text

### Where Fat-P Loses (Honesty Builds Trust)

- **Performance-critical paths:** `std::to_string` for primitives is faster than Stringify's abstraction
- **Binary serialization:** String representation is not suitable for binary formats
- **Locale-specific formatting:** Stringify uses C locale; for localized output, use `std::format`
- **Large containers:** O(n) string concatenation; for huge containers, consider streaming

---

## Integration Points

```
Stringify.h
    ↓ uses
Concepts.h              (C++20 concepts for type detection)
CppFeatureDetection.h   (feature detection macros)
    ↓ used by
DiagnosticLogger.h  (automatic value formatting)
FatPTest.h          (assertion message generation)
Debug utilities
```

---

## Final Assessment

Stringify delivers on the fat_p promise through three pillars:

### 1. Permanence
The standard will never provide universal stringification—too many design choices with no consensus. Stringify makes the decision once with sensible defaults and concept-based escape hatches.

### 2. Specialization
Compile-time method detection via C++20 concepts ensures zero runtime dispatch. The priority chain (toString → to_string → streamable → container) reflects common C++ patterns. Recursion depth limits prevent stack overflow.

### 3. Control
`StringifyOptions` customizes output format. `EnumStringifier<T>` trait enables enum-to-string mapping. Container delimiters are configurable. You control the output without modifying the types.

**Architectural Verdict:** Stringify transforms type-to-string conversion from **scattered manual implementations** to **automatic compile-time detection**. Any type with a conversion method "just works." It's `std::to_string` for everything.

---

*Stringify.h (810 lines) — Fat-P Library*
