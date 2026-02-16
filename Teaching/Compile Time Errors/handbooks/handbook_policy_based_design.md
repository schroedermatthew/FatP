# Handbook: Policy-Based Design

## Compile-Time Behavior Selection Through Template Parameters

**Estimated time:** 30–45 minutes  
**Prerequisites:** Templates, basic metaprogramming  
**Guarantee:** ✅ Compile-time behavior selection, zero runtime dispatch

---

## The Pattern

Policy-based design encodes behavioral choices as template parameters. Instead of runtime polymorphism (virtual functions), behavior is selected at compile time and fully inlined.

```cpp
// Runtime polymorphism: virtual dispatch on every call
class Allocator {
public:
    virtual void* allocate(size_t n) = 0;
    virtual void deallocate(void* p) = 0;
};

// Policy-based: behavior baked in at compile time
template<typename AllocationPolicy>
class Container {
    AllocationPolicy allocator_;
public:
    void* allocate(size_t n) {
        return allocator_.allocate(n);  // Inlined, no virtual call
    }
};
```

---

## The Wound: Virtual Dispatch Overhead

Consider a checked integer class that needs configurable overflow behavior:

```cpp
class OverflowHandler {
public:
    virtual int handle(int a, int b) = 0;
};

class CheckedInt {
    int value_;
    OverflowHandler* handler_;  // Pointer to handler
public:
    CheckedInt operator+(CheckedInt other) {
        if (would_overflow(value_, other.value_)) {
            return CheckedInt{handler_->handle(value_, other.value_)};  // Virtual call!
        }
        return CheckedInt{value_ + other.value_};
    }
};
```

**Problems:**
1. Virtual call on every overflow (indirect jump, can't inline)
2. Pointer indirection (cache miss possible)
3. Handler must be heap-allocated or have stable address
4. Can't optimize based on handler behavior

---

## The Solution: Policies as Template Parameters

```cpp
struct ThrowOnOverflow {
    [[noreturn]] static int handle(int a, int b) {
        throw std::overflow_error("integer overflow");
    }
};

struct SaturateOnOverflow {
    static int handle(int a, int b) {
        return (a > 0) ? INT_MAX : INT_MIN;
    }
};

struct WrapOnOverflow {
    static int handle(int a, int b) {
        // Let it wrap (undefined behavior for signed, but we're explicit)
        return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b));
    }
};

template<typename T, typename OverflowPolicy = ThrowOnOverflow>
class CheckedInt {
    T value_;
public:
    explicit CheckedInt(T v) : value_(v) {}
    
    CheckedInt operator+(CheckedInt other) const {
        if (would_overflow(value_, other.value_)) {
            return CheckedInt{OverflowPolicy::handle(value_, other.value_)};
        }
        return CheckedInt{value_ + other.value_};
    }
    
    T value() const { return value_; }
};

// Usage
using SafeInt = CheckedInt<int, ThrowOnOverflow>;
using SaturatingInt = CheckedInt<int, SaturateOnOverflow>;
using WrappingInt = CheckedInt<int, WrapOnOverflow>;
```

**Benefits:**
1. `OverflowPolicy::handle()` is inlined—no virtual call
2. Compiler can optimize based on policy (e.g., eliminate dead branches)
3. No pointer, no indirection, no heap allocation
4. Policy choice is explicit in the type

---

## FAT-P Components Using Policies

### CheckedArithmetic

```cpp
template<typename T, 
         typename OverflowPolicy = ThrowOnOverflow,
         typename DivisionPolicy = ThrowOnDivisionByZero>
class CheckedArithmetic;

using SafeInt = CheckedArithmetic<int>;
using SaturatingInt = CheckedArithmetic<int, SaturateOnOverflow>;
using TerminatingInt = CheckedArithmetic<int, TerminateOnOverflow>;
```

### StateMachine

```cpp
template<typename Context,
         typename TransitionTable,
         typename InvalidTransitionPolicy = ThrowOnInvalidTransition,
         typename ActionExceptionPolicy = PropagateException,
         size_t InitialState,
         typename... States>
class StateMachine;
```

Policies control:
- What happens on invalid transition (throw, terminate, ignore, log)
- How exceptions in actions are handled (propagate, catch and log, terminate)

### Expected

```cpp
template<typename T, typename E, typename AccessPolicy = DefaultAccessPolicy>
class Expected;

// AccessPolicy controls what happens when you access value() on an error:
// - ThrowOnErrorAccess: throws the error
// - TerminateOnErrorAccess: calls std::terminate()
// - UBOnErrorAccess: undefined behavior (maximum performance, zero checks)
```

### StrongId

```cpp
template<typename Tag, 
         typename T = int,
         typename ValidationPolicy = NoValidation>
class StrongId;

struct PositiveValidation {
    static void validate(int v) {
        if (v <= 0) throw std::invalid_argument("ID must be positive");
    }
};

using PositiveId = StrongId<IdTag, int, PositiveValidation>;
```

---

## Designing Your Own Policies

### Step 1: Define the Policy Interface

What operations must a policy provide? Document with a concept (C++20) or comments (C++17):

```cpp
// C++20 concept
template<typename P>
concept OverflowPolicy = requires(int a, int b) {
    { P::handle(a, b) } -> std::same_as<int>;
};

// Or just document:
// OverflowPolicy must provide:
//   static int handle(int a, int b);
```

### Step 2: Implement Policy Classes

Policies are typically stateless structs with static methods:

```cpp
struct ThrowOnOverflow {
    static constexpr const char* name = "throw";
    
    [[noreturn]] static int handle(int a, int b) {
        throw std::overflow_error("overflow in addition");
    }
};

struct SaturateOnOverflow {
    static constexpr const char* name = "saturate";
    
    static constexpr int handle(int a, int b) noexcept {
        return (a > 0) ? INT_MAX : INT_MIN;
    }
};

struct LogAndWrapOnOverflow {
    static constexpr const char* name = "log_and_wrap";
    
    static int handle(int a, int b) {
        std::cerr << "Warning: overflow in " << a << " + " << b << "\n";
        return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b));
    }
};
```

### Step 3: Use in Template

```cpp
template<typename T, OverflowPolicy Policy = ThrowOnOverflow>
class SafeInt {
    T value_;
public:
    SafeInt operator+(SafeInt other) const {
        if (would_overflow(value_, other.value_)) {
            return SafeInt{Policy::handle(value_, other.value_)};
        }
        return SafeInt{value_ + other.value_};
    }
};
```

### Step 4: Provide Convenient Type Aliases

```cpp
using ThrowingInt = SafeInt<int, ThrowOnOverflow>;
using SaturatingInt = SafeInt<int, SaturateOnOverflow>;
using WrappingInt = SafeInt<int, LogAndWrapOnOverflow>;
```

---

## Policy Categories

| Category | Example Policies | Used By |
|----------|-----------------|---------|
| **Error handling** | Throw, Terminate, Return default, Log | Expected, CheckedArithmetic |
| **Validation** | None, Range check, Positive, Non-null | StrongId, containers |
| **Bounds checking** | None, Assert, Throw | Containers, span-like |
| **Threading** | Single-threaded, Mutex-protected, Lock-free | Caches, singletons |
| **Allocation** | Heap, Stack, Pool, Arena | Containers |
| **Logging** | None, Stderr, File, Custom | Throughout |

---

## Composing Multiple Policies

Complex classes may need multiple policies:

```cpp
template<typename T,
         typename OverflowPolicy = ThrowOnOverflow,
         typename UnderflowPolicy = ThrowOnUnderflow,
         typename DivisionPolicy = ThrowOnDivisionByZero,
         typename ConversionPolicy = ThrowOnNarrowing>
class SafeNumeric {
    T value_;
public:
    // Each operation uses appropriate policy
    SafeNumeric operator+(SafeNumeric other) const {
        // Use OverflowPolicy
    }
    
    SafeNumeric operator-(SafeNumeric other) const {
        // Use UnderflowPolicy
    }
    
    SafeNumeric operator/(SafeNumeric other) const {
        // Use DivisionPolicy
    }
    
    template<typename U>
    explicit operator U() const {
        // Use ConversionPolicy
    }
};
```

### Simplifying with Policy Bundles

```cpp
struct StrictPolicies {
    using Overflow = ThrowOnOverflow;
    using Underflow = ThrowOnUnderflow;
    using Division = ThrowOnDivisionByZero;
    using Conversion = ThrowOnNarrowing;
};

struct RelaxedPolicies {
    using Overflow = SaturateOnOverflow;
    using Underflow = SaturateOnUnderflow;
    using Division = ReturnZeroOnDivisionByZero;
    using Conversion = TruncateOnNarrowing;
};

template<typename T, typename PolicyBundle = StrictPolicies>
class SafeNumeric {
    using OverflowPolicy = typename PolicyBundle::Overflow;
    using UnderflowPolicy = typename PolicyBundle::Underflow;
    // ...
};

using StrictInt = SafeNumeric<int, StrictPolicies>;
using RelaxedInt = SafeNumeric<int, RelaxedPolicies>;
```

---

## Stateful Policies

Sometimes policies need state (e.g., allocator with memory pool):

```cpp
template<size_t PoolSize>
struct PoolAllocator {
    char pool_[PoolSize];
    size_t offset_ = 0;
    
    void* allocate(size_t n) {
        if (offset_ + n > PoolSize) throw std::bad_alloc();
        void* p = pool_ + offset_;
        offset_ += n;
        return p;
    }
    
    void deallocate(void*) {
        // Pool allocator doesn't deallocate individual items
    }
    
    void reset() { offset_ = 0; }
};

template<typename T, typename AllocPolicy = std::allocator<T>>
class Container {
    AllocPolicy allocator_;  // Stored as member
    // ...
};

// Empty base optimization for stateless policies
template<typename T, typename AllocPolicy = std::allocator<T>>
class Container : private AllocPolicy {  // Inherit for EBO
    // Access via this->allocate()
};
```

---

## Trade-offs

| Benefit | Cost |
|---------|------|
| Zero runtime dispatch | Code bloat (each policy = separate instantiation) |
| Full inlining | Longer compile times |
| Compile-time selection | Policy can't change at runtime |
| Type encodes behavior | More complex type signatures |
| Testable in isolation | More templates to maintain |

### When to Use Policies

**Good fit:**
- Multiple valid behaviors exist
- Behavior known at compile time
- Performance critical (avoid virtual calls)
- Want to test behaviors independently

**Consider alternatives:**
- Only one behavior needed → just hardcode it
- Behavior chosen at runtime → use virtual dispatch or std::function
- Simplicity valued over flexibility → use simple design
- Compile time already problematic → avoid more templates

---

## Summary

| Aspect | Virtual Dispatch | Policy-Based |
|--------|-----------------|--------------|
| Binding time | Runtime | Compile time |
| Performance | Virtual call overhead | Zero overhead |
| Flexibility | Can change at runtime | Fixed at compile time |
| Code size | One implementation | One per policy combination |
| Testability | Mock via inheritance | Direct policy testing |

### Key Principles

1. **Policies are template parameters** — behavior baked in at compile time

2. **Policies are usually stateless** — static methods, no data members

3. **Define clear policy interfaces** — document or use concepts

4. **Provide convenient aliases** — users shouldn't write full template args

5. **Consider policy bundles** — group related policies together

### The Guideline in One Sentence

> Use policies to select compile-time behavior with zero runtime overhead.

---

## Further Reading

- "Modern C++ Design" by Andrei Alexandrescu — definitive policy-based design book
- [C++ Core Guidelines T.69](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#t69-inside-a-template-dont-make-an-unqualified-non-member-function-call-unless-you-intend-it-to-be-a-customization-point)
- FAT-P source code: CheckedArithmetic.h, StateMachine.h, Expected.h
