# EnumPlus: A Fat-P Library Showcase

## Executive Summary

EnumPlus is a **compile-time enum enhancement system** that adds string conversion, iteration, bounds-checked indexing, and bitwise operators to C++ enums via policy-based specialization. Unlike runtime reflection libraries (overhead, complexity) or macro-based solutions (hard to debug, limited), EnumPlus uses **template specialization traits** that the compiler resolves at compile time. The `EnumPlusMap<E>` provides O(1) enum-indexed storage with optional bounds checking, while `to_string`/`from_string` enable type-safe serialization.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The manual string conversion nightmare
enum class Color { Red, Green, Blue };

std::string to_string(Color c) {
    switch (c) {
        case Color::Red:   return "Red";
        case Color::Green: return "Green";
        case Color::Blue:  return "Blue";
    }
    return "Unknown";  // Easy to forget new values
}

Color from_string(const std::string& s) {
    if (s == "Red")   return Color::Red;
    if (s == "Green") return Color::Green;
    if (s == "Blue")  return Color::Blue;
    throw std::invalid_argument("Unknown color: " + s);
}
// Maintenance burden: add enum value → update 2+ functions

// The unsafe array indexing
std::array<Handler, 3> handlers;
handlers[static_cast<size_t>(color)] = myHandler;  // No bounds check!
// If Color gains a 4th value, silent buffer overflow
```

| Issue | HPC Impact |
|-------|------------|
| Manual string conversion | Maintenance burden, easy to forget new values |
| Unsafe enum indexing | Out-of-bounds access if enum grows |
| No iteration | Can't enumerate all values |
| No bitwise flags | Manual implementation for flag enums |

### The Standard's Limitation

C++ enums are just integers with names:
- No reflection (can't list values)
- No string conversion (just integer cast)
- No bounds checking (array indexing is unsafe)
- No iteration (can't `for (auto v : Color)`)

C++23/26 reflection proposals may help eventually, but adoption is years away.

---

## Architecture: Trait-Based Enhancement

### The Mechanism: Specialization Points

```cpp
// User specializes these traits for their enums
template<typename E>
struct EnumSizeTrait {
    // Number of enum values (for iteration/bounds)
    static constexpr std::size_t size = 0;
};

template<typename E>
struct EnumStringPolicy {
    // String conversion (names array + functions)
    static constexpr bool has_names = false;
};

template<typename E>
struct EnableOverloadedOperators {
    // Enable ++, --, bitwise ops
    static constexpr bool value = false;
};

// User provides specializations:
enum class Color { Red, Green, Blue, COUNT };

template<>
struct EnumSizeTrait<Color> {
    static constexpr std::size_t size = static_cast<std::size_t>(Color::COUNT);
};

template<>
struct EnumStringPolicy<Color> {
    static constexpr bool has_names = true;
    static constexpr std::array<std::string_view, 3> names = {
        "Red", "Green", "Blue"
    };
    
    static std::string_view to_string(Color c) {
        return names[static_cast<std::size_t>(c)];
    }
    
    static Color from_string(std::string_view s) {
        auto it = std::find(names.begin(), names.end(), s);
        if (it == names.end()) throw std::invalid_argument("Unknown color");
        return static_cast<Color>(std::distance(names.begin(), it));
    }
};
```

**Why traits over macros:**
- Debuggable (actual C++ code)
- IDE-friendly (completion, navigation)
- Composable (add features independently)
- Type-safe (no string concatenation tricks)

---

## Feature Inventory

### 1. String Conversion

```cpp
enum class Status { Pending, Active, Completed };
// (with EnumStringPolicy specialization)

Status s = Status::Active;
std::string_view name = fat_p::to_string(s);  // "Active"

Status parsed = fat_p::from_string<Status>("Pending");  // Status::Pending

// Optional-returning, case-insensitive variant
std::optional<Status> maybe = fat_p::from_string_icase<Status>("invalid");
// maybe == std::nullopt
```

### 2. Bounds-Checked Enum Map

```cpp
EnumPlusMap<Color, Handler> handlers;

handlers[Color::Red] = redHandler;
handlers[Color::Green] = greenHandler;
handlers[Color::Blue] = blueHandler;

// With bounds checking (default)
handlers[static_cast<Color>(99)];  // throws std::out_of_range

// Without bounds checking (performance mode)
EnumPlusMap<Color, Handler, NoBoundsCheckPolicy> fast_handlers;
```

**Mechanism:** `EnumPlusMap` uses `std::array<T, EnumSizeTrait<E>::size>` internally, with `static_cast` indexing and policy-based bounds checking.

### 3. Enum Iteration

```cpp
// Iterate all values (range-for over the constexpr value array)
for (Color c : enum_values<Color>()) {
    std::cout << fat_p::to_string(c) << "\n";
}
// Output: Red, Green, Blue

// Or with a callable
for_each_enum<Color>([](Color c) { std::cout << fat_p::to_string(c) << "\n"; });

// Get all values as array
constexpr auto all_colors = enum_values<Color>();
static_assert(all_colors.size() == 3);
```

(There are no `++`/`--` operators for enums — the opt-in operator set is bitwise/shift only. To step through values, iterate `enum_values<E>()`.)

### 4. Bitwise Flag Operators

```cpp
enum class Permissions : uint32_t {
    None    = 0,
    Read    = 1 << 0,
    Write   = 1 << 1,
    Execute = 1 << 2,
    All     = Read | Write | Execute
};

template<>
struct EnableOverloadedOperators<Permissions> {
    static constexpr bool value = true;
};

Permissions p = Permissions::Read | Permissions::Write;
bool canWrite = (p & Permissions::Write) != Permissions::None;

p |= Permissions::Execute;  // Add permission
p &= ~Permissions::Write;   // Remove permission
```

### 5. EnumPlusWrapper (Strong Typing)

```cpp
// Prevent implicit conversion from underlying type
using SafeColor = EnumPlusWrapper<Color>;

SafeColor c(Color::Red);
// SafeColor c2(0);  // Compile error! No implicit int conversion

Color raw = c.value();              // Extract enum
auto underlying = c.underlying();   // Get int value
```

---

## Why Not Alternatives?

| If You Need... | Why Not Manual Switch | Why Not magic_enum | Why Not Macro Generator | Fat-P Advantage |
|----------------|----------------------|-------------------|------------------------|-----------------|
| Compile-time names | ❌ Runtime strings | ✅ Constexpr | ✅ Constexpr | ✅ Constexpr |
| Bounds-checked map | ❌ Manual | ❌ No map | ❌ No map | ✅ EnumPlusMap |
| No dependencies | ✅ Pure C++ | ❌ Header-only but complex | ✅ Macros | ✅ Header-only |
| IDE-friendly | ✅ Works | ✅ Works | ❌ Macros obscure | ✅ Works |
| Works with all enums | ✅ Manual work | ❌ Compiler limits | ✅ Works | ✅ Works |

**The Sweet Spot:** EnumPlus provides enum iteration, string conversion, bounds-checked maps, and operators without magic_enum's compiler-specific limits or macro complexity.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ reflection is coming (C++26+?) but:
- Adoption in production codebases: 5-10 years away
- HPC codebases locked to C++20 even longer
- Even with reflection, enum maps need manual implementation

EnumPlus provides enum enhancements **today** on C++20, with a migration path to reflection when available.

---

## Performance Characteristics

| Operation | Cost | Notes |
|----------|------|-------|
| `to_string` | O(1) | Array lookup |
| `from_string` | O(n) | Linear search (n = enum size) |
| `EnumPlusMap[]` | O(1) | Direct array access |
| `enum_values` iteration | O(n) | Loop over values |
| Bounds check | O(1) | Single comparison against min/max |

### Where Fat-P Wins
- Configuration systems with enum serialization
- Game engines with state enums
- Any code needing enum iteration or string conversion

### Where Fat-P Loses (Honesty Builds Trust)
- magic_enum "just works" → EnumPlus requires explicit traits
- Large enums (>100 values) → `from_string` linear search is O(n)
- C++26 reflection available → standard solution preferred

---

## Integration Points

```
EnumPlus.h
    ↓ used by
FeatureManager.h     (FeatureRelationship, FeatureGroupState)
DiagnosticLogger.h   (LogLevel enum)
StateMachine.h       (State enum handling)
JsonLite.h           (Enum serialization)
```

---

## Final Assessment

EnumPlus delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ reflection is years away from production use. EnumPlus provides enum enhancements **now** that will remain useful as a simpler alternative even after reflection arrives.

### 2. Specialization
Trait-based design means you opt-in to features per enum. `EnumPlusMap` provides bounds-checked O(1) enum-indexed storage that no standard container offers.

### 3. Control
Policy-based bounds checking, optional operators, and string conversion are independently configurable. You add only the features each enum needs.

**Architectural Verdict:** EnumPlus transforms enums from **integer aliases** to **rich types** with iteration, serialization, safe indexing, and operators—all resolved at compile time via trait specialization.

---

*EnumPlus.h (659 lines) — Fat-P Library*
