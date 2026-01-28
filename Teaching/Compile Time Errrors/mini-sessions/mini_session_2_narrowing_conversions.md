# Mini-Session 2: Narrowing Conversions

## Preventing Silent Data Loss with Brace Initialization

**Estimated time:** 15–20 minutes  
**Prerequisites:** Basic C++ types  
**Guarantee:** ✅ Compile-time (with brace initialization)

---

## The One-Minute Summary

Brace initialization `{}` catches narrowing conversions at compile time:

```cpp
int x = 3.14;    // Compiles! x = 3 (lost .14)
int y{3.14};     // Compile error: narrowing conversion
int z = {3.14};  // Compile error: narrowing conversion
```

---

## What Counts as Narrowing?

| From | To | Example | Problem |
|------|----|---------|---------|
| `double` | `int` | `3.14 → 3` | Fractional part lost |
| `int64_t` | `int32_t` | Large value → truncated | Overflow |
| `int` | `char` | `1000 → ?` | Value doesn't fit |
| `unsigned` | `signed` | `UINT_MAX → -1` | Reinterpretation |
| `int` | `unsigned` | `-1 → UINT_MAX` | Sign change |

---

## The Fix: Use Braces

```cpp
// Bad: silent truncation
void send_packet(uint16_t length);
size_t file_size = get_file_size();  // 64-bit on most systems
send_packet(file_size);               // Silently truncates!

// Good: compiler catches it
send_packet({file_size});             // Error: narrowing conversion

// Explicit when intentional
if (file_size > UINT16_MAX) throw std::overflow_error("too large");
send_packet(static_cast<uint16_t>(file_size));
```

---

## Compiler Flags

```bash
# GCC/Clang
-Wconversion        # Warn on implicit narrowing
-Wsign-conversion   # Warn on signed/unsigned
-Wfloat-conversion  # Warn on float→int

# MSVC
/W4                 # Includes narrowing warnings
```

---

## Gotcha: initializer_list Preference

```cpp
std::vector<int> v1(10);    // 10 elements, all 0
std::vector<int> v2{10};    // 1 element: the value 10

std::vector<int> v3(10, 1); // 10 elements, all 1
std::vector<int> v4{10, 1}; // 2 elements: 10 and 1
```

**Rule:** Use `()` for "n copies", `{}` for "these values".

---

## Safe Pattern

```cpp
// Use braces for single values (catches narrowing)
int port{config.get_port()};
size_t count{items.size()};

// Use explicit cast when narrowing is intentional
auto byte = static_cast<uint8_t>(value & 0xFF);
```

---

## Real Bug: Ariane 5 (1996)

```cpp
int16_t horizontal_bias = velocity;  // 64-bit float → 16-bit int overflow!
// Result: $370 million rocket exploded
```

Brace initialization would have caught this at compile time.

---

## Summary

| Initialization | Narrowing |
|----------------|-----------|
| `int x = val;` | Allowed |
| `int x(val);` | Allowed |
| `int x{val};` | **Error** |
| `int x = {val};` | **Error** |

---

## Exercise

Fix this code to catch narrowing at compile time:

```cpp
void process(uint8_t flags, uint16_t length);
void handle(int user_flags, size_t data_len) {
    process(user_flags, data_len);  // Both narrow silently!
}
```

---

## Further Reading

- [C++ Core Guidelines ES.46](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es46-avoid-lossy-narrowing-truncating-arithmetic-conversions)
- Migration Guide: CheckedArithmetic
