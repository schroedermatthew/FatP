# EnumPlus User Manual

**Version:** 1.0  
**Library:** fat_p C++ Utilities  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [String Conversion](#string-conversion)
4. [Bounds Checking](#bounds-checking)
5. [Bitwise Operations](#bitwise-operations)
6. [Iteration](#iteration)
7. [EnumPlusMap](#enumplusmap)
8. [Integration](#integration)
9. [Best Practices](#best-practices)

---

## Overview

EnumPlus provides enhanced enum utilities including string conversion, bounds checking, iteration, and safe array indexing.

### Include

```cpp
#include "EnumPlus.h"
using namespace fat_p;
```

### Key Features

- **String conversion**: Enum to/from string
- **Bounds checking**: Safe value validation
- **Array indexing**: Type-safe enum-indexed arrays
- **Iteration**: Range-based for over enum values
- **Bitwise ops**: Flag operations with type safety

---

## Quick Start

### Define Enum with Metadata

```cpp
// Step 1: Define enum
enum class Color { Red, Green, Blue, COUNT };

// Step 2: Specialize traits
template <>
struct EnumSizeTrait<Color> {
    static constexpr size_t size = static_cast<size_t>(Color::COUNT);
};

template <>
struct EnumStringPolicy<Color> {
    static constexpr std::array<std::string_view, 3> names = {
        "Red", "Green", "Blue"
    };
};

// Step 3: Use
std::string name = enum_to_string(Color::Red);  // "Red"
auto color = string_to_enum<Color>("Green");    // Color::Green
```

### Macro Helper

```cpp
// Simplified definition
DEFINE_ENUM_STRINGS(Color, "Red", "Green", "Blue")

// Now use:
std::cout << enum_to_string(Color::Green);  // "Green"
```

---

## String Conversion

### Enum to String

```cpp
enum class Status { OK, Error, Pending, COUNT };

// With EnumStringPolicy specialized
std::string s = enum_to_string(Status::OK);     // "OK"
std::string_view sv = enum_name(Status::Error); // "Error"
```

### String to Enum

```cpp
// Returns std::optional
auto status = string_to_enum<Status>("OK");
if (status) {
    use(*status);
}

// With default value
Status s = string_to_enum_or(std::string_view{"Unknown"}, Status::Error);
```

### Case-Insensitive Conversion

```cpp
auto color = string_to_enum_icase<Color>("RED");  // Color::Red
auto color2 = string_to_enum_icase<Color>("red"); // Color::Red
```

---

## Bounds Checking

### Policies

```cpp
// DefaultBoundsCheckPolicy - throws on out of bounds
// NoBoundsCheckPolicy - no checking (faster)

// Apply policy
enum_to_string<Color, DefaultBoundsCheckPolicy>(color);
enum_to_string<Color, NoBoundsCheckPolicy>(color);  // Dangerous!
```

### Safe Casting

```cpp
// Checked cast from integer
auto result = safe_enum_cast<Color>(1);  // std::optional<Color::Green>
auto bad = safe_enum_cast<Color>(99);    // std::nullopt

// Unchecked (only if you're sure)
Color c = static_cast<Color>(1);  // Color::Green
```

### Validation

```cpp
bool valid = is_valid_enum<Color>(value);

int raw = 5;
if (is_valid_enum<Color>(raw)) {
    Color c = static_cast<Color>(raw);
}
```

---

## Bitwise Operations

### Enable Operators

```cpp
enum class Flags { None = 0, Read = 1, Write = 2, Execute = 4 };

// Enable bitwise operators
template <>
struct EnableBitwiseOperators<Flags> : std::true_type {};

// Now use:
Flags rw = Flags::Read | Flags::Write;
Flags r = rw & Flags::Read;
bool has_write = (rw & Flags::Write) != Flags::None;

// Toggle
rw ^= Flags::Execute;

// Clear
rw &= ~Flags::Write;
```

### Helper Functions

```cpp
// Check if flag set
bool can_read = has_flag(permissions, Flags::Read);

// Set flag
permissions = set_flag(permissions, Flags::Write);

// Clear flag
permissions = clear_flag(permissions, Flags::Execute);

// Toggle flag
permissions = toggle_flag(permissions, Flags::Read);
```

---

## Iteration

### Range-Based For

```cpp
// With EnumRange helper
for (Color c : EnumRange<Color>()) {
    std::cout << enum_to_string(c) << "\n";
}
// Output: Red, Green, Blue
```

### All Values

```cpp
// Get array of all values
constexpr auto colors = enum_values<Color>();
for (Color c : colors) {
    process(c);
}
```

### Index Access

```cpp
Color c = enum_at<Color>(1);  // Color::Green

// With bounds check
auto maybe_color = enum_at_checked<Color>(5);  // std::nullopt
```

---

## EnumPlusMap

Type-safe array indexed by enum values.

### Basic Usage

```cpp
enum class Day { Mon, Tue, Wed, Thu, Fri, Sat, Sun, COUNT };

// Array indexed by Day
EnumPlusMap<Day, std::string> day_names;
day_names[Day::Mon] = "Monday";
day_names[Day::Tue] = "Tuesday";
// ...

std::cout << day_names[Day::Mon];  // "Monday"
```

### Initialization

```cpp
// Default initialization
EnumPlusMap<Color, int> counts;  // All zeros

// With initializer
EnumPlusMap<Color, int> values{10, 20, 30};  // Red=10, Green=20, Blue=30

// Fill
EnumPlusMap<Color, bool> flags;
flags.fill(true);  // All true
```

### Iteration

```cpp
EnumPlusMap<Color, int> counts;

// Iterate with enum key
for (Color c : EnumRange<Color>()) {
    std::cout << enum_to_string(c) << ": " << counts[c] << "\n";
}

// Iterate values only
for (int count : counts) {
    total += count;
}
```

### Bounds Checking

```cpp
// Default: checked access
EnumPlusMap<Color, int, DefaultBoundsCheckPolicy> safe_map;

// Unchecked for performance
EnumPlusMap<Color, int, NoBoundsCheckPolicy> fast_map;
```

---

## Integration

### With JSON

```cpp
// Serialize
JsonValue to_json(Color c) {
    return JsonValue(enum_to_string(c));
}

// Deserialize
Color from_json(const JsonValue& j) {
    auto result = string_to_enum<Color>(j.as_string());
    if (!result) throw std::runtime_error("Invalid color");
    return *result;
}
```

### With Expected

```cpp
Expected<Color, std::string> parse_color(std::string_view s) {
    auto result = string_to_enum<Color>(s);
    if (!result) {
        return make_unexpected("Invalid color: " + std::string(s));
    }
    return *result;
}
```

### With Streams

```cpp
// Output
std::ostream& operator<<(std::ostream& os, Color c) {
    return os << enum_to_string(c);
}

// Input
std::istream& operator>>(std::istream& is, Color& c) {
    std::string s;
    is >> s;
    if (auto result = string_to_enum<Color>(s)) {
        c = *result;
    } else {
        is.setstate(std::ios::failbit);
    }
    return is;
}
```

---

## Best Practices

### Do

```cpp
// ✅ Use COUNT sentinel for size
enum class Status { OK, Error, Pending, COUNT };

// ✅ Specialize traits once
template <>
struct EnumSizeTrait<Status> {
    static constexpr size_t size = static_cast<size_t>(Status::COUNT);
};

// ✅ Use EnumPlusMap for enum-indexed data
EnumPlusMap<Priority, Handler> handlers;

// ✅ Validate input
auto status = string_to_enum<Status>(user_input);
if (!status) {
    report_error("Invalid status");
}

// ✅ Use bitwise ops for flags
Flags perms = Flags::Read | Flags::Write;
```

### Don't

```cpp
// ❌ Don't cast without validation
Color c = static_cast<Color>(user_input);  // Dangerous!

// ❌ Don't forget COUNT in iteration
for (int i = 0; i <= static_cast<int>(Status::COUNT); ++i)  // Off by one!

// ❌ Don't mix enum types
Color c = Color::Red;
Status s = static_cast<Status>(c);  // Type confusion!

// ❌ Don't hardcode string comparisons
if (str == "Red") color = Color::Red;  // Use string_to_enum
```

---

## Complete Example

```cpp
#include "EnumPlus.h"

// Define enum
enum class Priority { Low, Medium, High, Critical, COUNT };

// Traits
template <>
struct EnumSizeTrait<Priority> {
    static constexpr size_t size = 4;
};

template <>
struct EnumStringPolicy<Priority> {
    static constexpr std::array<std::string_view, 4> names = {
        "Low", "Medium", "High", "Critical"
    };
};

// Enable operators (optional, for flags)
template <>
struct EnableBitwiseOperators<Priority> : std::false_type {};

// Usage
void example() {
    // String conversion
    std::cout << enum_to_string(Priority::High) << "\n";
    
    // Parse
    auto p = string_to_enum<Priority>("Medium");
    
    // Iterate
    for (auto priority : EnumRange<Priority>()) {
        std::cout << enum_to_string(priority) << "\n";
    }
    
    // Indexed map
    EnumPlusMap<Priority, int> counts;
    counts[Priority::High] = 42;
}
```

---

## Related Components

- **Stringify.h**: `EnumStringifier` trait for custom conversion
- **FatPJsonLite.h**: Enum serialization support
- **enforce.h**: Use with enum validation

---

**Document Version:** 1.0  
**Last Updated:** November 2025
