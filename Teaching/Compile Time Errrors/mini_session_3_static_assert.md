# Mini-Session 3: static_assert

## Compile-Time Assertions for Assumptions and Requirements

**Estimated time:** 15–20 minutes  
**Prerequisites:** Basic C++ templates  
**Guarantee:** ✅ Compile-time

---

## The One-Minute Summary

`static_assert` checks conditions at compile time—if false, compilation fails:

```cpp
static_assert(sizeof(void*) == 8, "64-bit platform required");
static_assert(std::is_trivially_copyable_v<MyType>, "Must be trivially copyable");
```

---

## Use Cases

### 1. Platform Requirements

```cpp
// Ensure expected sizes
static_assert(sizeof(int) >= 4, "int must be at least 32 bits");
static_assert(sizeof(size_t) == sizeof(void*), "size_t must match pointer size");
static_assert(CHAR_BIT == 8, "8-bit bytes required");

// Ensure endianness (C++20)
static_assert(std::endian::native == std::endian::little, "Little-endian required");
```

### 2. Template Type Requirements

```cpp
template<typename T>
class Buffer {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Buffer uses memcpy; T must be trivially copyable");
    static_assert(sizeof(T) <= 64,
                  "Buffer optimized for small types");
    // ...
};

Buffer<int> ok;           // Compiles
Buffer<std::string> bad;  // Error: string not trivially copyable
```

### 3. Struct Layout Verification

```cpp
#pragma pack(push, 1)
struct NetworkPacket {
    uint32_t magic;
    uint16_t length;
    uint16_t flags;
    uint8_t  payload[1024];
};
#pragma pack(pop)

static_assert(offsetof(NetworkPacket, magic) == 0);
static_assert(offsetof(NetworkPacket, length) == 4);
static_assert(offsetof(NetworkPacket, flags) == 6);
static_assert(offsetof(NetworkPacket, payload) == 8);
static_assert(sizeof(NetworkPacket) == 1032);
```

### 4. Enum Integrity

```cpp
enum class ErrorCode : uint8_t {
    Success = 0,
    NotFound,
    InvalidArg,
    Timeout,
    COUNT_
};

static_assert(static_cast<int>(ErrorCode::COUNT_) == 4,
              "Update serialization code when adding error codes");
```

### 5. Compile-Time Math Verification

```cpp
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}

static_assert(factorial(5) == 120);
static_assert(factorial(10) == 3628800);

constexpr size_t BUFFER_SIZE = 1024;
constexpr size_t ALIGNMENT = 64;
static_assert(BUFFER_SIZE % ALIGNMENT == 0, "Buffer must be aligned");
```

### 6. Type Relationships

```cpp
template<typename Derived, typename Base>
void safe_downcast(Base* b) {
    static_assert(std::is_base_of_v<Base, Derived>,
                  "Derived must inherit from Base");
    return static_cast<Derived*>(b);
}

template<typename T, typename U>
void safe_assign(T& dest, U value) {
    static_assert(std::is_assignable_v<T&, U>,
                  "Cannot assign U to T");
    dest = value;
}
```

---

## static_assert vs assert

| Feature | `static_assert` | `assert` |
|---------|-----------------|----------|
| When checked | Compile time | Runtime |
| Performance cost | Zero | Debug only (usually) |
| Expression type | Constant expression | Any expression |
| Failure mode | Compile error | Abort/crash |
| Can check types | Yes | No |

---

## C++17: Message is Optional

```cpp
// C++11/14: message required
static_assert(sizeof(int) == 4, "int must be 32 bits");

// C++17: message optional
static_assert(sizeof(int) == 4);  // OK, compiler provides generic message
```

---

## Common Patterns

```cpp
// Prevent instantiation with pointer types
template<typename T>
class Container {
    static_assert(!std::is_pointer_v<T>, "Use smart pointers instead");
};

// Ensure type is complete
template<typename T>
class Wrapper {
    static_assert(sizeof(T) > 0, "T must be a complete type");
};

// Check alignment requirements
template<typename T>
class AlignedBuffer {
    static_assert(alignof(T) <= 64, "Alignment too strict");
    alignas(T) char buffer[sizeof(T)];
};
```

---

## Summary

| Check This | With This |
|------------|-----------|
| Platform assumptions | `sizeof`, `alignof`, `CHAR_BIT` |
| Type properties | `is_trivially_copyable`, `is_integral`, etc. |
| Struct layout | `sizeof`, `offsetof` |
| Template requirements | Type traits in `<type_traits>` |
| Compile-time math | `constexpr` functions |

---

## Exercise

Add static_asserts to verify:
1. A struct fits in a cache line (64 bytes)
2. A template parameter is an integral type
3. Two types have the same size

---

## Further Reading

- [C++ Core Guidelines P.5](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#p5-prefer-compile-time-checking-to-run-time-checking)
- Session 8: Template Constraints (for concepts)
