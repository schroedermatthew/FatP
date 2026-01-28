# Appendix: C++23/26 Compile-Time Safety Features

## What's Coming and How to Prepare

---

## C++23 Features (Available Now)

### std::expected

The standard library now has `expected<T, E>`:

```cpp
#include <expected>

std::expected<int, std::string> parse(const std::string& s) {
    if (s.empty()) {
        return std::unexpected("empty string");
    }
    try {
        return std::stoi(s);
    } catch (...) {
        return std::unexpected("invalid number");
    }
}

auto result = parse("42");
if (result) {
    std::cout << *result;  // 42
} else {
    std::cout << result.error();  // error message
}
```

**Migration from FAT-P Expected:**

| Feature | FAT-P Expected | std::expected |
|---------|---------------|---------------|
| Core API | ✅ | ✅ Compatible |
| `transform()` | ✅ | ✅ |
| `and_then()` | ✅ | ✅ |
| `or_else()` | ✅ | ✅ |
| `EXPECTED_TRY` | ✅ | ❌ Not in standard |
| Policies | ✅ | ❌ Not in standard |
| `[[nodiscard]]` | ✅ | ✅ |

**Keep FAT-P Expected for:**
- `EXPECTED_TRY` macro
- Custom policies (ThrowOnError, TerminateOnError)
- C++17 compatibility

---

### Deducing This

Simplifies CRTP and reduces boilerplate:

```cpp
// Before C++23: CRTP for polymorphic chaining
template<typename Derived>
class BuilderBase {
public:
    Derived& set_name(std::string name) {
        name_ = std::move(name);
        return static_cast<Derived&>(*this);
    }
protected:
    std::string name_;
};

class MyBuilder : public BuilderBase<MyBuilder> {
    // Inherits set_name returning MyBuilder&
};

// C++23: deducing this
class Builder {
    std::string name_;
public:
    template<typename Self>
    Self&& set_name(this Self&& self, std::string name) {
        self.name_ = std::move(name);
        return std::forward<Self>(self);
    }
};

// Works correctly for:
Builder b;
b.set_name("x");           // Returns Builder&
std::move(b).set_name("y"); // Returns Builder&&
```

**Also simplifies:**
- Const/non-const overloads (one definition)
- Recursive lambdas
- Visitor patterns

---

### if consteval

Detect compile-time vs runtime context:

```cpp
constexpr int compute(int n) {
    if consteval {
        // Compile-time: can use slower but clearer algorithm
        return slow_but_clear_algorithm(n);
    } else {
        // Runtime: use optimized version
        return fast_algorithm(n);
    }
}

// Compile time: uses slow_but_clear_algorithm
constexpr int a = compute(10);

// Runtime: uses fast_algorithm
int b = compute(runtime_value);
```

---

### Multidimensional operator[]

```cpp
class Matrix {
    std::vector<double> data_;
    size_t rows_, cols_;
public:
    double& operator[](size_t row, size_t col) {
        return data_[row * cols_ + col];
    }
    
    const double& operator[](size_t row, size_t col) const {
        return data_[row * cols_ + col];
    }
};

Matrix m(3, 3);
m[1, 2] = 3.14;  // Instead of m[1][2] or m(1, 2)
```

---

### std::unreachable()

Marks code as unreachable (enables optimization):

```cpp
enum class Color { Red, Green, Blue };

const char* to_string(Color c) {
    switch (c) {
        case Color::Red: return "red";
        case Color::Green: return "green";
        case Color::Blue: return "blue";
    }
    std::unreachable();  // Tells compiler this can't happen
}
```

---

### Explicit Object Parameter (Deducing This) for Lambdas

```cpp
// Recursive lambda without std::function
auto factorial = [](this auto self, int n) -> int {
    if (n <= 1) return 1;
    return n * self(n - 1);
};

factorial(5);  // 120
```

---

## C++26 Features (Proposed/Expected)

### Contracts

Preconditions, postconditions, and assertions as language features:

```cpp
// Syntax may change
double sqrt(double x)
    pre(x >= 0)
    post(r: r >= 0 && r * r == x)  // r is the return value
{
    return std::sqrt(x);
}

class Stack {
public:
    void push(int x)
        pre(!full())
        post(size() == old size() + 1);
    
    int pop()
        pre(!empty())
        post(size() == old size() - 1);
    
    bool empty() const;
    bool full() const;
    size_t size() const;
};
```

**Current status:** Removed from C++20, redesigned for C++26.

**Prepare now:**
```cpp
// Write assertions that can become contracts later
void push(int x) {
    assert(!full() && "precondition: not full");
    // ... implementation ...
    assert(size() == old_size + 1 && "postcondition: size increased");
}
```

---

### Static Reflection

Introspect types at compile time:

```cpp
// Proposed syntax (may change)
struct Point {
    int x;
    int y;
};

template<typename T>
void print_members() {
    constexpr auto members = std::meta::members_of(^T);
    template for (constexpr auto member : members) {
        std::cout << std::meta::name_of(member) << ": "
                  << std::meta::type_of(member) << "\n";
    }
}

print_members<Point>();
// Output:
// x: int
// y: int
```

**Use cases:**
- Automatic serialization/deserialization
- ORM without macros
- Debug printing
- Enum-to-string without macros

---

### Pattern Matching

```cpp
// Proposed syntax (may change)
int describe(const Shape& s) {
    return inspect(s) {
        Circle{.radius = r} => std::format("circle r={}", r);
        Rectangle{.width = w, .height = h} => std::format("rect {}x{}", w, h);
        __ => "unknown";
    };
}

// For variants
std::variant<int, double, std::string> v = "hello";
auto result = inspect(v) {
    <int> i => std::format("int: {}", i);
    <double> d => std::format("double: {}", d);
    <std::string> s => std::format("string: {}", s);
};
```

---

### Sender/Receiver (std::execution)

Structured concurrency and async operations:

```cpp
// Proposed
auto work = std::execution::just(42)
          | std::execution::then([](int x) { return x * 2; })
          | std::execution::then([](int x) { return std::to_string(x); });

std::execution::sync_wait(work);  // Returns "84"
```

---

## Migration Strategies

### FAT-P Expected → std::expected

**Gradual migration:**

```cpp
// Compatibility header
#if __cplusplus >= 202302L && __has_include(<expected>)
    #include <expected>
    namespace my_project {
        using std::expected;
        using std::unexpected;
    }
#else
    #include "fat_p/Expected.h"
    namespace my_project {
        using fat_p::Expected as expected;
        using fat_p::unexpected;
    }
#endif
```

**Keep FAT-P when:**
- Need `EXPECTED_TRY` macro
- Need policies
- Supporting C++17

---

### Preparing for Contracts

Write code that's easy to migrate:

```cpp
// Today: use assert with message
void transfer(Account& from, Account& to, Money amount) {
    // Preconditions
    assert(amount > Money{0} && "amount must be positive");
    assert(from.balance() >= amount && "insufficient funds");
    
    auto old_total = from.balance() + to.balance();
    
    // Implementation
    from.withdraw(amount);
    to.deposit(amount);
    
    // Postconditions
    assert(from.balance() + to.balance() == old_total && "money conserved");
}

// Future (C++26): contracts
void transfer(Account& from, Account& to, Money amount)
    pre(amount > Money{0})
    pre(from.balance() >= amount)
    post(from.balance() + to.balance() == old(from.balance() + to.balance()))
{
    from.withdraw(amount);
    to.deposit(amount);
}
```

---

### Preparing for Reflection

Design types that will work well with reflection:

```cpp
// Good: simple aggregates
struct Person {
    std::string name;
    int age;
    std::string email;
};

// Avoid: complex invariants that break with naive serialization
class Person {
    std::string name_;  // Private with getters—harder to reflect
public:
    const std::string& name() const;
};
```

---

## Feature Detection

### Standard Feature Test Macros

```cpp
// C++23 features
#if __cpp_lib_expected >= 202211L
    // std::expected available
#endif

#if __cpp_explicit_this_parameter >= 202110L
    // deducing this available
#endif

#if __cpp_if_consteval >= 202106L
    // if consteval available
#endif

#if __cpp_multidimensional_subscript >= 202211L
    // multi-dim operator[] available
#endif

// General version check
#if __cplusplus >= 202302L
    // C++23 mode
#endif
```

### Compiler Version Checks

```cpp
#if defined(__clang__)
    #if __clang_major__ >= 16
        // Clang 16+ features
    #endif
#elif defined(__GNUC__)
    #if __GNUC__ >= 13
        // GCC 13+ features
    #endif
#elif defined(_MSC_VER)
    #if _MSC_VER >= 1936
        // VS 2022 17.6+ features
    #endif
#endif
```

---

## Summary

### Available Now (C++23)

| Feature | Compile-Time Safety Use |
|---------|------------------------|
| `std::expected` | Type-safe error handling |
| Deducing this | Cleaner CRTP, less boilerplate |
| `if consteval` | Compile-time vs runtime paths |
| `std::unreachable()` | Document impossible states |
| Multidim `operator[]` | Cleaner matrix/tensor access |

### Coming Soon (C++26)

| Feature | Compile-Time Safety Use |
|---------|------------------------|
| Contracts | Preconditions/postconditions |
| Reflection | Automatic serialization, enum-to-string |
| Pattern matching | Exhaustive variant handling |

### Preparation Guidelines

1. **Use assertions** with clear messages → easy migration to contracts
2. **Use FAT-P Expected** → compatible API with std::expected
3. **Prefer aggregates** → ready for reflection
4. **Use `overloaded{}`** → pattern matching will improve it
5. **Write concepts** → foundation for future constraint features

---

## Further Reading

- [cppreference C++23](https://en.cppreference.com/w/cpp/23)
- [P2900: Contracts](http://wg21.link/p2900) — contracts proposal
- [P2996: Reflection](http://wg21.link/p2996) — reflection proposal  
- [P1371: Pattern Matching](http://wg21.link/p1371) — pattern matching proposal
- [P2300: std::execution](http://wg21.link/p2300) — sender/receiver proposal
