# Problem-Solving Session 2: The Missing Case

## Exhaustive Enum Handling with EnumPlus

**Estimated time:** 45–60 minutes  
**Prerequisites:** Familiarity with C++ enums and switch statements  
**Fat-P components:** `EnumPlus.h`, compiler flags

---

## The Bug

Your team's e-commerce platform has been running smoothly for months. Then a support ticket arrives:

> "When I check my order status, sometimes I see a blank message instead of the status."

You trace it to this code, which passed code review six months ago:

```cpp
enum class OrderStatus {
    Pending,
    Processing,
    Shipped,
    Delivered,
    Cancelled   // Added 3 months ago
};

std::string get_status_message(OrderStatus status) {
    switch (status) {
        case OrderStatus::Pending:    return "Your order is being prepared.";
        case OrderStatus::Processing: return "Your order is being processed.";
        case OrderStatus::Shipped:    return "Your order is on the way!";
        case OrderStatus::Delivered:  return "Your order has been delivered.";
    }
    return "";  // "Just in case"
}
```

**The bug:** `Cancelled` was added three months ago, but `get_status_message()` was never updated. Customers with cancelled orders see a blank message.

---

## Questions to Consider

Before reading further, think about:

1. **Q1:** Why didn't the compiler warn about the missing case?
2. **Q2:** Would adding `default:` have helped?
3. **Q3:** How can we make the compiler enforce exhaustiveness?
4. **Q4:** Is there a way to avoid switch statements entirely?
5. **Q5:** What compiler flags should we enable?

---

## Q1: Why Didn't the Compiler Warn?

The compiler *can* warn about missing enum cases, but in this code it didn't because:

**The `return "";` at the end suppresses the warning.**

```cpp
switch (status) {
    case OrderStatus::Pending:    return "...";
    case OrderStatus::Processing: return "...";
    case OrderStatus::Shipped:    return "...";
    case OrderStatus::Delivered:  return "...";
}
return "";  // This line tells the compiler "I've got it covered"
```

The compiler sees that all code paths return a value, so it doesn't warn about missing cases.

**Without the trailing return:**
```cpp
switch (status) {
    case OrderStatus::Pending:    return "...";
    case OrderStatus::Processing: return "...";
    // Missing cases...
}
// warning: control reaches end of non-void function
```

But even this warning only appears with the right compiler flags enabled.

---

## Q2: Would `default:` Have Helped?

Many developers instinctively add `default:` as "defensive programming":

```cpp
switch (status) {
    case OrderStatus::Pending:    return "Preparing...";
    case OrderStatus::Processing: return "Processing...";
    case OrderStatus::Shipped:    return "On the way!";
    case OrderStatus::Delivered:  return "Delivered.";
    default:                      return "Unknown status";
}
```

**This is worse, not better.** Here's why:

| Approach | When `Cancelled` is added |
|----------|---------------------------|
| No default, no trailing return | Compiler warning (with flags) |
| Trailing `return ""` | Silent bug — blank message |
| `default: return "Unknown"` | Silent bug — wrong message |

**The fundamental problem:** Both `return ""` and `default:` say "I'll handle unknown cases at runtime." But the entire point of an enum is that there *are* no unknown cases—you've enumerated all of them!

**When `default:` IS appropriate:**
- External enums from libraries you don't control
- Enums that explicitly represent extensible protocols
- Enums with intentional gaps in values

**For your own enums:** Avoid `default:`. Let the compiler enforce exhaustiveness.

---

## Q3: The Compiler-Enforced Fix

**Step 1: Remove the safety nets**

```cpp
std::string get_status_message(OrderStatus status) {
    switch (status) {
        case OrderStatus::Pending:    return "Your order is being prepared.";
        case OrderStatus::Processing: return "Your order is being processed.";
        case OrderStatus::Shipped:    return "Your order is on the way!";
        case OrderStatus::Delivered:  return "Your order has been delivered.";
        case OrderStatus::Cancelled:  return "Your order has been cancelled.";
    }
    // No default! No trailing return!
}
```

**Step 2: Enable compiler warnings as errors**

```cmake
# CMakeLists.txt
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Werror=switch-enum)
elseif(MSVC)
    add_compile_options(/we4062)
endif()
```

**Now when someone adds `Refunded`:**

```
error: enumeration value 'Refunded' not handled in switch [-Werror=switch-enum]
```

The code won't compile until every switch statement is updated.

---

## Q4: Avoiding Switch Statements with EnumPlusMap

Fat-P's `EnumPlus.h` provides a better pattern: **data-driven dispatch** using `EnumPlusMap`.

### The EnumPlusMap Approach

```cpp
#include "fat_p/EnumPlus.h"

enum class OrderStatus {
    Pending,
    Processing,
    Shipped,
    Delivered,
    Cancelled
};

// Step 1: Declare the enum size (required for EnumPlus)
template<>
struct fat_p::EnumSizeTrait<OrderStatus> {
    static constexpr std::size_t size = 5;
};

// Step 2: Create a compile-time map from enum to message
constexpr fat_p::EnumPlusMap<OrderStatus, std::string_view> STATUS_MESSAGES = {{
    "Your order is being prepared.",   // Pending
    "Your order is being processed.",  // Processing
    "Your order is on the way!",       // Shipped
    "Your order has been delivered.",  // Delivered
    "Your order has been cancelled."   // Cancelled
}};

// Step 3: Use it
std::string_view get_status_message(OrderStatus status) {
    return STATUS_MESSAGES[status];
}
```

### Why This Is Better

| Switch Statement | EnumPlusMap |
|------------------|-------------|
| Logic scattered across cases | Data centralized in one place |
| Easy to forget a case | Array size must match enum size |
| Requires compiler flags | Compile error if sizes mismatch |
| Runtime dispatch | Compile-time array, O(1) lookup |

### What Happens When You Add a Value?

If someone adds `Refunded` to the enum but forgets to update `EnumSizeTrait`:

```cpp
enum class OrderStatus { Pending, Processing, Shipped, Delivered, Cancelled, Refunded };

template<>
struct fat_p::EnumSizeTrait<OrderStatus> {
    static constexpr std::size_t size = 5;  // Oops, should be 6
};
```

Now `STATUS_MESSAGES[OrderStatus::Refunded]` accesses index 5 in a 5-element array → **bounds check failure** (with `DefaultBoundsCheckPolicy`) or undefined behavior.

**Better: Keep EnumSizeTrait in sync with a static_assert:**

```cpp
enum class OrderStatus {
    Pending,
    Processing,
    Shipped,
    Delivered,
    Cancelled,
    COUNT_  // Sentinel value
};

template<>
struct fat_p::EnumSizeTrait<OrderStatus> {
    static constexpr std::size_t size = static_cast<std::size_t>(OrderStatus::COUNT_);
};

// Now adding a value before COUNT_ automatically updates the size
```

### Iterating Over All Values

EnumPlus provides iteration utilities:

```cpp
// Print all status messages
fat_p::for_each_enum<OrderStatus>([](OrderStatus s) {
    std::cout << static_cast<int>(s) << ": " << STATUS_MESSAGES[s] << "\n";
});

// Get array of all values
constexpr auto all_statuses = fat_p::enum_values<OrderStatus>();
```

### String Conversion

For enum-to-string and string-to-enum conversion, specialize `EnumStringPolicy`:

```cpp
template<>
struct fat_p::EnumStringPolicy<OrderStatus> {
    static constexpr bool has_names = true;
    
    static std::string_view to_string(OrderStatus s) {
        constexpr std::array<std::string_view, 5> names = {
            "Pending", "Processing", "Shipped", "Delivered", "Cancelled"
        };
        return names[static_cast<std::size_t>(s)];
    }
    
    static OrderStatus from_string(std::string_view str) {
        for (std::size_t i = 0; i < 5; ++i) {
            if (to_string(static_cast<OrderStatus>(i)) == str) {
                return static_cast<OrderStatus>(i);
            }
        }
        throw std::invalid_argument("Unknown OrderStatus: " + std::string(str));
    }
};

// Usage
std::cout << fat_p::to_string(OrderStatus::Shipped);  // "Shipped"
auto status = fat_p::from_string<OrderStatus>("Pending");  // OrderStatus::Pending

// Case-insensitive parsing (returns optional)
auto maybe = fat_p::from_string_icase<OrderStatus>("SHIPPED");  // std::optional<OrderStatus>
```

---

## Q5: Compiler Flags Reference

### GCC and Clang

```makefile
CXXFLAGS += -Wall              # Enables -Wswitch (but not -Wswitch-enum)
CXXFLAGS += -Wswitch-enum      # Warn even if default exists
CXXFLAGS += -Werror=switch     # Treat -Wswitch as error
CXXFLAGS += -Werror=switch-enum # Treat -Wswitch-enum as error (recommended)
```

**`-Wswitch` vs `-Wswitch-enum`:**

| Flag | With `default:` | Without `default:` |
|------|-----------------|-------------------|
| `-Wswitch` | No warning | Warning on missing case |
| `-Wswitch-enum` | Warning on missing case | Warning on missing case |

**Recommendation:** Use `-Werror=switch-enum` to catch missing cases even when `default:` is present.

### MSVC

```
/W4         # Enables C4062 (unhandled enum in switch)
/we4062     # Treat C4062 as error
```

### CMake

```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wswitch-enum
        -Werror=switch-enum
    )
elseif(MSVC)
    add_compile_options(
        /W4
        /we4062
    )
endif()
```

---

## Complete Example: Before and After

### Before (Bug-Prone)

```cpp
enum class OrderStatus { Pending, Processing, Shipped, Delivered, Cancelled };

std::string get_status_message(OrderStatus status) {
    switch (status) {
        case OrderStatus::Pending:    return "Preparing...";
        case OrderStatus::Processing: return "Processing...";
        case OrderStatus::Shipped:    return "On the way!";
        case OrderStatus::Delivered:  return "Delivered.";
        default:                      return "";  // Hides missing Cancelled!
    }
}

std::string get_status_color(OrderStatus status) {
    switch (status) {
        case OrderStatus::Pending:    return "yellow";
        case OrderStatus::Processing: return "blue";
        case OrderStatus::Shipped:    return "purple";
        case OrderStatus::Delivered:  return "green";
        default:                      return "gray";  // Hides missing Cancelled!
    }
}
```

### After (EnumPlus + Compiler Flags)

```cpp
#include "fat_p/EnumPlus.h"

enum class OrderStatus {
    Pending,
    Processing,
    Shipped,
    Delivered,
    Cancelled,
    COUNT_
};

template<>
struct fat_p::EnumSizeTrait<OrderStatus> {
    static constexpr std::size_t size = static_cast<std::size_t>(OrderStatus::COUNT_);
};

// All data in one place, compile-time verified
constexpr fat_p::EnumPlusMap<OrderStatus, std::string_view> STATUS_MESSAGES = {{
    "Your order is being prepared.",
    "Your order is being processed.",
    "Your order is on the way!",
    "Your order has been delivered.",
    "Your order has been cancelled."
}};

constexpr fat_p::EnumPlusMap<OrderStatus, std::string_view> STATUS_COLORS = {{
    "yellow",  // Pending
    "blue",    // Processing
    "purple",  // Shipped
    "green",   // Delivered
    "red"      // Cancelled
}};

std::string_view get_status_message(OrderStatus s) { return STATUS_MESSAGES[s]; }
std::string_view get_status_color(OrderStatus s) { return STATUS_COLORS[s]; }
```

**Adding `Refunded`:**
1. Add to enum (before `COUNT_`)
2. `EnumSizeTrait::size` automatically updates
3. `EnumPlusMap` initializers now have wrong size → **compile error**
4. Fix all maps → done!

---

## Summary

| Problem | Solution |
|---------|----------|
| Switch misses new enum values | Remove `default:`, remove trailing return, use `-Werror=switch-enum` |
| Multiple switches for same enum | Use `EnumPlusMap` for data-driven dispatch |
| Enum size out of sync | Use `COUNT_` sentinel pattern |
| String conversion | Specialize `EnumStringPolicy` |
| Iteration over all values | `for_each_enum<E>()` or `enum_values<E>()` |

### Key Principles

1. **`default:` hides bugs** — it tells the compiler "trust me," and the compiler stops checking
2. **Data-driven beats code-driven** — `EnumPlusMap` centralizes enum handling and enforces completeness
3. **Compiler flags are essential** — `-Werror=switch-enum` catches bugs at compile time
4. **Make illegal states unrepresentable** — if every enum value must have a handler, make the compiler enforce it

### When to Use Each Approach

| Scenario | Recommendation |
|----------|----------------|
| Simple value lookup (string, color, etc.) | `EnumPlusMap` |
| Complex logic per case | Switch with `-Werror=switch-enum` |
| External/library enums | `default:` with logging |
| Flag enums (bitwise OR) | `EnableOverloadedOperators` + `has_flag()` |

---

## Exercises

1. **Warm-up:** Take an existing switch statement in your codebase and remove the `default:` case. Does it compile with `-Wswitch-enum`?

2. **Refactor:** Convert a switch-based enum handler to use `EnumPlusMap`. Measure the before/after line count.

3. **String conversion:** Implement `EnumStringPolicy` for an enum in your codebase. Add a unit test that iterates all values and verifies round-trip conversion.

4. **Flag enum:** Create a `Permissions` flag enum with `Read`, `Write`, `Execute`. Use `EnableOverloadedOperators` to enable bitwise operations and `has_flag()` for checking.

---

## Further Reading

From your materials:
- `fat_p/EnumPlus.h` — full implementation with documentation
- `tests/test_EnumPlus.cpp` — usage examples and edge cases

External:
- [GCC Warning Options](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html) — `-Wswitch` and `-Wswitch-enum` documentation
- [Clang Diagnostics Reference](https://clang.llvm.org/docs/DiagnosticsReference.html) — equivalent Clang flags
- [MSVC Warning C4062](https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-4-c4062) — MSVC's enum switch warning

---

## Alternatives to EnumPlus

### When EnumPlus Is the Right Choice

Fat-P `EnumPlus` excels when you need:
- **Compile-time enum-indexed arrays** — `EnumPlusMap<E, T>`
- **Iteration over all enum values** — `for_each_enum<E>()`
- **String conversion** — `to_string()`, `from_string()`
- **Bounds-checked access** — `DefaultBoundsCheckPolicy`
- **Zero external dependencies** — header-only, no macros

### Alternatives

**magic_enum (C++17)**
```cpp
#include <magic_enum.hpp>

// Automatic reflection — no manual size/string specification!
auto name = magic_enum::enum_name(OrderStatus::Shipped);  // "Shipped"
auto value = magic_enum::enum_cast<OrderStatus>("Shipped");  // optional

// Iteration
for (auto s : magic_enum::enum_values<OrderStatus>()) { ... }
```

**Pros:**
- Zero boilerplate — no `EnumSizeTrait` or `EnumStringPolicy` needed
- Automatic string conversion
- Works with any enum

**Cons:**
- Relies on compiler-specific tricks (may not work on all compilers)
- Limited range by default (-128 to 128)
- Longer compile times

**Better Enums (C++11)**
```cpp
BETTER_ENUM(OrderStatus, int, Pending, Processing, Shipped, Delivered, Cancelled)

// Automatic everything
OrderStatus s = OrderStatus::Shipped;
std::cout << s._to_string();  // "Shipped"
```

**Pros:**
- Full reflection
- Compile-time string conversion
- C++11 compatible

**Cons:**
- Macro-based definition
- Not a real `enum class` (different syntax)

### Decision Matrix

| Need | EnumPlus | magic_enum | Better Enums |
|------|----------|------------|--------------|
| No macros | ✓ | ✓ | ✗ |
| No boilerplate | ✗ | ✓ | ✓ |
| Enum-indexed map | ✓ | ✗ | ✗ |
| Standard enum class | ✓ | ✓ | ✗ |
| C++11 support | ✗ | ✗ | ✓ |
| Compile speed | Fast | Slower | Medium |
