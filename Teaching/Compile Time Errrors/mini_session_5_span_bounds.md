# Mini-Session 5: std::span Bounds Safety

## Bundling Pointer and Size for Safer Buffer Access

**Estimated time:** 15–20 minutes  
**Prerequisites:** Arrays, pointers  
**Guarantee:** ⚠ Runtime (bounds checking is implementation-defined)

---

## The One-Minute Summary

`std::span` bundles a pointer and size together, preventing them from getting out of sync:

```cpp
// Bad: pointer and size can mismatch
void process(int* data, size_t size);

// Good: span keeps them together
void process(std::span<int> data);
```

---

## Basic Usage

```cpp
#include <span>

void process(std::span<int> data) {
    for (int x : data) {           // Range-for works
        std::cout << x << "\n";
    }
    
    data[0] = 42;                  // Subscript works
    data.size();                   // Size always available
    data.data();                   // Underlying pointer accessible
    data.empty();                  // Convenience check
}

// Many ways to call:
std::vector<int> vec = {1, 2, 3};
process(vec);                      // Implicit conversion

int arr[] = {1, 2, 3, 4, 5};
process(arr);                      // Array decays to span

process({vec.data() + 1, 2});      // Subrange: 2 elements starting at index 1
```

---

## Fixed-Size Span

When size is known at compile time:

```cpp
void process_header(std::span<int, 4> header) {
    // Compiler knows exactly 4 elements
    // May enable optimizations and compile-time checks
}

int arr[4] = {1, 2, 3, 4};
process_header(arr);              // OK: size matches

std::vector<int> vec = {1, 2, 3, 4};
process_header(vec);              // Error: vector size not known at compile time

// Explicit construction (your risk):
process_header(std::span<int, 4>{vec.data(), 4});
```

---

## Subspans

```cpp
void process(std::span<int> data) {
    auto first_half = data.first(data.size() / 2);   // First n elements
    auto last_half = data.last(data.size() / 2);     // Last n elements
    auto middle = data.subspan(10, 20);              // 20 elements at offset 10
    auto tail = data.subspan(10);                    // Everything from offset 10
}
```

---

## What span Does NOT Do

| Feature | span | vector |
|---------|------|--------|
| Owns data | ❌ No | ✅ Yes |
| Resizable | ❌ No | ✅ Yes |
| Bounds checking | Implementation-defined | `at()` throws |
| Memory management | ❌ None | ✅ Automatic |

**Critical: span can dangle!**

```cpp
std::span<int> dangerous() {
    std::vector<int> local = {1, 2, 3};
    return local;  // DANGER: span points to destroyed vector!
}
```

span is a **view**, not a container. The underlying data must outlive the span.

---

## Replacing C-Style APIs

### Before: Separate Pointer and Size

```cpp
// Easy to pass wrong size
void process_buffer(const char* data, size_t len);

char buf[100];
size_t actual = read(fd, buf, sizeof(buf));
process_buffer(buf, sizeof(buf));  // BUG: should be 'actual'
```

### After: Span Bundles Them

```cpp
void process_buffer(std::span<const char> data);

char buf[100];
size_t actual = read(fd, buf, sizeof(buf));
process_buffer({buf, actual});     // Correct: explicit size
// Or:
process_buffer(std::span{buf, actual});
```

---

## GSL span vs std::span

```cpp
// GSL (pre-C++20): bounds checking in debug mode
gsl::span<int> data = vec;
data[100];  // Asserts in debug, UB in release

// std::span (C++20): implementation-defined
std::span<int> data = vec;
data[100];  // Might assert, might be UB—check your compiler
```

For guaranteed bounds checking, use `at()`-style access or GSL.

---

## Common Patterns

### Accept Anything Array-Like

```cpp
void process(std::span<const int> data);  // const: won't modify

process(std::vector<int>{1, 2, 3});       // vector
process(std::array<int, 3>{1, 2, 3});     // array
int c_arr[] = {1, 2, 3};
process(c_arr);                            // C array
process({ptr, count});                     // raw pointer + size
```

### Return Subview of Internal Data

```cpp
class Buffer {
    std::vector<char> data_;
public:
    std::span<const char> view() const { return data_; }
    std::span<char> view() { return data_; }
};
```

---

## Summary

| Use span when | Use vector when |
|---------------|-----------------|
| Just viewing data | Owning data |
| Fixed or known size | Dynamic size |
| Interfacing with C | Pure C++ |
| Performance critical | Safety critical |
| Non-owning parameter | Return value |

---

## Exercise

Refactor this function to use span:

```cpp
int sum(const int* arr, size_t count) {
    int total = 0;
    for (size_t i = 0; i < count; ++i) {
        total += arr[i];
    }
    return total;
}
```

---

## Further Reading

- [C++ Core Guidelines SL.str.1](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#slstr1-use-stdstring-to-own-character-sequences)
- [P0122: span](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0122r7.pdf)
