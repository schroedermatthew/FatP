# Mini-Session 4: noexcept Contracts

## Exception Specifications as Compile-Time Contracts

**Estimated time:** 15–20 minutes  
**Prerequisites:** Basic exception handling  
**Guarantee:** ✅ Compile-time (for noexcept checks), ⚠ Runtime (if violated, terminates)

---

## The One-Minute Summary

`noexcept` declares a function won't throw. If it does, `std::terminate()` is called:

```cpp
void safe_cleanup() noexcept {
    // If this throws, program terminates immediately
}
```

---

## Why noexcept Matters

### 1. Destructors Must Be noexcept

```cpp
class Resource {
public:
    ~Resource() noexcept {  // Implicit, but be explicit
        // Throwing here during stack unwinding → std::terminate()
    }
};
```

If a destructor throws during stack unwinding from another exception, the program terminates. Always noexcept.

### 2. Move Operations Enable Optimization

```cpp
class Buffer {
public:
    // Without noexcept: vector copies on reallocation (safe but slow)
    Buffer(Buffer&& other);
    
    // With noexcept: vector moves on reallocation (fast)
    Buffer(Buffer&& other) noexcept;
};
```

`std::vector::push_back` checks `is_nothrow_move_constructible`. If false, it copies instead of moving to maintain strong exception guarantee.

```cpp
// This makes a HUGE performance difference
static_assert(std::is_nothrow_move_constructible_v<Buffer>,
              "Buffer move must be noexcept for vector optimization");
```

### 3. Swap Should Be noexcept

```cpp
void swap(Buffer& a, Buffer& b) noexcept {
    using std::swap;
    swap(a.data_, b.data_);
    swap(a.size_, b.size_);
}
```

Many algorithms require noexcept swap for strong exception safety.

### 4. Enables Compiler Optimizations

```cpp
void fast_path() noexcept {
    // Compiler can omit exception handling tables
    // Smaller code, potentially faster
}
```

---

## Conditional noexcept

noexcept can depend on compile-time conditions:

```cpp
template<typename T>
class Container {
public:
    // noexcept if T's move is noexcept
    Container(Container&& other) 
        noexcept(std::is_nothrow_move_constructible_v<T>);
    
    // noexcept if swap(T,T) is noexcept
    void swap(Container& other)
        noexcept(std::is_nothrow_swappable_v<T>);
};
```

---

## Checking noexcept at Compile Time

```cpp
// Check a type
static_assert(std::is_nothrow_move_constructible_v<MyType>);
static_assert(std::is_nothrow_destructible_v<MyType>);

// Check an expression
static_assert(noexcept(my_func()));
static_assert(noexcept(a.swap(b)));
```

---

## Common Mistakes

### Wrong: Allocating in Move Constructor

```cpp
Buffer(Buffer&& other) noexcept {
    data_ = new char[other.size_];  // WRONG: new can throw!
    // ...
}
```

### Right: Only Transfer Pointers

```cpp
Buffer(Buffer&& other) noexcept
    : data_(std::exchange(other.data_, nullptr))
    , size_(std::exchange(other.size_, 0))
{}
```

### Wrong: Calling Potentially-Throwing Functions

```cpp
void cleanup() noexcept {
    log("cleaning up");  // Does log() throw? If yes, terminate!
}
```

### Right: Catch or Verify

```cpp
void cleanup() noexcept {
    try {
        log("cleaning up");
    } catch (...) {
        // Swallow or handle
    }
}
// Or verify log is noexcept:
static_assert(noexcept(log("")));
```

---

## What Should Be noexcept?

| Function Type | noexcept? | Reason |
|---------------|-----------|--------|
| Destructors | Always | Throwing terminates during unwinding |
| Move constructor | Yes | Enables vector optimization |
| Move assignment | Yes | Enables vector optimization |
| swap() | Yes | Required by many algorithms |
| Comparison operators | Usually | Used in sort, containers |
| Hash functions | Usually | Used in unordered containers |
| Simple getters | Usually | No reason to throw |

---

## noexcept and the Standard Library

```cpp
// These check noexcept status
std::is_nothrow_constructible_v<T, Args...>
std::is_nothrow_default_constructible_v<T>
std::is_nothrow_copy_constructible_v<T>
std::is_nothrow_move_constructible_v<T>
std::is_nothrow_assignable_v<T, U>
std::is_nothrow_copy_assignable_v<T>
std::is_nothrow_move_assignable_v<T>
std::is_nothrow_destructible_v<T>
std::is_nothrow_swappable_v<T>
std::is_nothrow_invocable_v<F, Args...>
```

---

## Summary

| Guideline | Reason |
|-----------|--------|
| Destructors: always noexcept | Throwing during unwind terminates |
| Move operations: noexcept | Enables optimization in containers |
| swap: noexcept | Required for exception safety |
| Mark noexcept when possible | Documents contract, enables optimization |
| Use conditional noexcept for templates | Propagates noexcept from contained types |

---

## Exercise

Check if your class's move operations are noexcept:

```cpp
class MyClass { /* ... */ };

static_assert(std::is_nothrow_move_constructible_v<MyClass>);
static_assert(std::is_nothrow_move_assignable_v<MyClass>);
```

If they're not, figure out why and fix them.

---

## Further Reading

- [C++ Core Guidelines E.12](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#e12-use-noexcept-when-exiting-a-function-because-of-a-throw-is-impossible-or-unacceptable)
- [C++ Core Guidelines C.66](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c66-make-move-operations-noexcept)
