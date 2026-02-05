---
doc_id: PS-CONST-005
doc_type: "Problem Session"
title: "The Accidental Mutation"
fatp_components: []
topics: ["const correctness", "immutability", "const reference", "constexpr",
         "consteval", "constinit", "if constexpr", "mutable keyword",
         "API contracts", "const method", "overload resolution",
         "move semantics", "shallow const", "lambda captures"]
constraints: ["accidental mutation", "aliasing", "register allocation",
              "const propagation", "undefined behavior via const_cast",
              "mutable data races", "move suppression", "shallow indirection"]
cxx_standard: "C++20"
last_verified: "2026-02-05"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Problem Session - The Accidental Mutation

## Const Correctness: Immutability Enforced by the Compiler

**Estimated time:** 45–60 minutes  
**Fat-P components:** None (native C++ feature)

---

## Scope

This session covers `const` as a compile-time enforcement mechanism: const references and pointers on function parameters, const member functions, the `mutable` keyword, `constexpr` and `consteval`, pointer-level const distinctions, and the interaction between `const` and concurrent read access.

## Not covered

- `const` in template metaprogramming and SFINAE contexts
- `std::as_const` utility
- `const` in C++20 concepts and constraints
- `volatile` and its interaction with `const`
- `const` in module interfaces (C++20)

## Prerequisites

- Basic C++ references and pointers (what `T&` and `T*` mean)
- Familiarity with classes and member functions
- Awareness of move semantics (for the `T&&` row in the parameter table)

## Problem Session Card

**Problem:** Read-only function silently mutates caller's data  
**Constraint:** Non-const reference parameter permits unintended writes  
**Symptom:** Downstream grouping breaks because categories were lowercased by a function that only computed an average  
**Root cause:** `calculate_average()` takes vector by non-const reference and normalizes categories as a side effect  
**Fix pattern:** Use `const T&` for read-only parameters; separate mutation into explicitly-named functions  
**FAT-P components used:** None (native C++ feature)  
**Build-mode gotchas:** None  
**Guarantees:** Compiler rejects mutation through const references  
**Non-guarantees:** Const does not prevent mutation through `mutable` members, `const_cast`, or aliased non-const paths; const is not a synchronization primitive

---

## The Bug

Your team's analytics pipeline processes millions of records daily. One day, results start looking wrong:

> "The aggregation numbers don't match the raw data anymore."

You trace it to this code:

```cpp
struct Record {
    std::string category;
    double value;
};

// Precondition: records is non-empty
double calculate_average(std::vector<Record>& records) {
    // "Normalize" categories for consistency
    for (auto& r : records) {
        std::transform(r.category.begin(), r.category.end(),
                       r.category.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
    }
    
    double sum = 0;
    for (const auto& r : records) {
        sum += r.value;
    }
    return sum / records.size();
}

void process_data(std::vector<Record>& data) {
    double avg = calculate_average(data);
    
    // Later: group by category
    std::map<std::string, double> by_category;
    for (const auto& r : data) {
        by_category[r.category] += r.value;  // BUG: categories were lowercased!
    }
    // Original case-sensitive categories are gone
}
```

**The bug:** `calculate_average()` mutates its input as a side effect. The caller didn't expect this. Original category casing is lost, breaking downstream grouping that depends on case-sensitive categories.

---

## Table of Contents

- [The Bug](#the-bug)
- [Questions to Consider](#questions-to-consider)
- [Q1: The Solution Was Always There](#q1-the-solution-was-always-there)
- [Q2: Const Reference vs Non-Const Reference](#q2-const-reference-vs-non-const-reference)
  - [What `const T&` Gives the Compiler](#what-const-t-gives-the-compiler) — with annotated assembly
- [Q3: Const Methods](#q3-const-methods)
  - [How Const Participates in Overload Resolution](#how-const-participates-in-overload-resolution)
  - [`mutable`: When the Implementation Changes but the Value Doesn't](#mutable-when-the-implementation-changes-but-the-value-doesnt)
- [Q4: Constexpr and Consteval](#q4-constexpr-and-consteval)
  - [`constexpr` Has Grown: What You Can Do Today](#constexpr-has-grown-what-you-can-do-today)
  - [`constinit`: Compile-Time Initialization, Runtime Mutability](#constinit-compile-time-initialization-runtime-mutability-c20)
  - [`if constexpr`: Compile-Time Branching](#if-constexpr-compile-time-branching-c17)
- [Q5: Where to Use Const](#q5-where-to-use-const)
- [Const and Thread Safety](#const-and-thread-safety)
  - [The Trap: `mutable` Breaks the Const-Means-Concurrent-Safe Rule](#the-trap-mutable-breaks-the-const-means-concurrent-safe-rule)
- [Where Const Loses](#where-const-loses)
  - [Shallow Const](#shallow-const) — with code examples
- [Const Blocks Move Semantics](#const-blocks-move-semantics)
- [Common Const Mistakes](#common-const-mistakes)
- [Guarantees / Non-Guarantees](#guarantees--non-guarantees)
- [Const Correctness Audit Checklist](#const-correctness-audit-checklist)
- [Summary](#summary)
- [Exercises](#exercises)
- [Further Reading](#further-reading)

---

## Questions to Consider

Before reading further, think about:

1. **Q1:** How could the compiler have prevented this?
2. **Q2:** What's the difference between `const T&`, `T&`, and `const T*`?
3. **Q3:** What does `const` on a method mean?
4. **Q4:** What about `constexpr` and `consteval`?
5. **Q5:** When should you use `const`?

---

## Q1: The Solution Was Always There

The fix is a single keyword — `const`:

```cpp
// Precondition: records is non-empty
double calculate_average(const std::vector<Record>& records) {
    for (auto& r : records) {
        std::transform(r.category.begin(), r.category.end(),  // Compile error!
                       r.category.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
    }
    // ...
}
```

With `const`, the compiler rejects the mutation (example Clang output):

```
error: cannot assign to variable 'r' with const-qualified type 'const Record&'
```

**The accidental mutation becomes a compile error.**

The corrected function removes the normalization entirely — it was never part of "computing an average":

```cpp
// Precondition: records is non-empty
double calculate_average(const std::vector<Record>& records) {
    double sum = 0;
    for (const auto& r : records) {
        sum += r.value;
    }
    return sum / records.size();
}
```

But what if normalization *is* needed somewhere? The answer is that normalization is a different operation and deserves a different function with a signature that makes the mutation explicit:

```cpp
// Read-only: takes const reference, cannot mutate
double calculate_average(const std::vector<Record>& records);

// Explicitly mutating: name and signature both say "I modify your data"
void normalize_categories_in_place(std::vector<Record>& records);
```

Now the caller decides whether and when to normalize. The compiler makes it impossible to accidentally embed normalization inside a read-only function. This is const correctness as **API contract design**, not just a compiler trick.

---

## Q2: Const Reference vs Non-Const Reference

### The Four Ways to Pass

C++ gives you four ways to accept an argument, each with different implications for who can see modifications and whether data is copied. The choice matters because it determines the function's contract with its caller — passing by non-const reference says "I may change your data," while passing by const reference says "I will only read." Getting this wrong is exactly how the opening bug happened: `calculate_average` accepted a non-const reference, and nothing in the signature told the caller to expect mutation.

```cpp
void by_value(std::vector<int> v);        // Copy — can modify, changes don't escape
void by_ref(std::vector<int>& v);         // Reference — can modify, changes escape
void by_const_ref(const std::vector<int>& v);  // Const ref — cannot modify
```

The following table summarizes the tradeoffs. The key column is "Can Modify?" — it determines the API contract.

| Parameter Type | Can Modify? | Copies Data? | Use When |
|----------------|-------------|--------------|----------|
| `T` | Yes (local) | Yes | Small types, need local copy |
| `T&` | Yes (caller sees) | No | Intent is to modify |
| `const T&` | No | No | Read-only access to large objects |
| `T&&` | Yes (move from) | No | Taking ownership |

### The Default Should Be `const`

```cpp
// BAD: Non-const when const would work
void print_stats(std::vector<Record>& records);  // Looks like it might modify

// GOOD: Const signals read-only intent
void print_stats(const std::vector<Record>& records);  // Clearly read-only
```

**Rule of thumb:** Use `const T&` by default for types that are expensive to copy. Only remove `const` when you need to modify.

### Why Not `const T&` for Everything?

The table above says "small types" should be passed by value, not by `const T&`. This matters — for cheap-to-copy types like `int`, `double`, `bool`, and pointers, `const T&` is actually worse than by-value.

A reference is a pointer under the hood. When you pass `const int&`, the caller has to store the int somewhere addressable and pass a pointer to it. The callee dereferences that pointer every time it reads the value — that's a memory load instead of a register read. With plain `int`, the value goes directly into a register (on any standard calling convention, the first few integer/floating-point arguments are passed in registers). No address taken, no indirection, no memory traffic.

There's also an aliasing cost. When the compiler sees `const int&`, it knows *this* reference won't be used to write — but it doesn't know whether the referenced memory might be modified through some other path (another pointer, a global, a function call with side effects). So the compiler may need to reload the value from memory after any intervening call or store. With `int` by value, the compiler knows the local copy can't be affected by anything else, so the value stays in a register across calls without reloads.

Note that `const` on a by-value parameter (`const int x`) doesn't help the compiler — it already does its own dataflow analysis to determine whether a local is reassigned. `void f(int)` and `void f(const int)` are even the same function signature in C++ (the `const` is stripped from the type). But it's still good practice inside the function body: if you don't intend to modify the parameter, marking it `const` catches accidental reassignment the same way it does for any other local variable. The benefit is intent declaration and mistake prevention, not codegen.

Here's a non-trivial case where it catches a real bug. Consider a function that interpolates between two values using a blend factor `t` that should stay in [0, 1]:

```cpp
double lerp(double a, double b, double t) {
    // Clamp to valid range... but this is a typo.
    // The developer meant to write: t = std::clamp(t, 0.0, 1.0);
    t = std::clamp(t, 0.0, b);  // Oops: 'b' instead of '1.0'
    return a + t * (b - a);
}
```

This compiles, runs, and produces plausible-looking results for many inputs — `b` happens to be greater than 1.0 often enough that the clamp appears to work. The bug only surfaces when `b` is between 0 and 1, at which point the interpolation silently clamps to the wrong range.

With `const` parameters, the typo is caught immediately:

```cpp
double lerp(const double a, const double b, const double t) {
    t = std::clamp(t, 0.0, b);  // Compile error: cannot assign to const double
    return a + t * (b - a);
}
```

The fix is to assign to a new local, which also makes the intent clearer:

```cpp
double lerp(const double a, const double b, const double t) {
    const double clamped = std::clamp(t, 0.0, 1.0);
    return a + clamped * (b - a);
}
```

The broader pattern: when a function takes several parameters of the same type, an accidental assignment to the wrong one compiles without warning and is hard to spot in review. `const` turns that silent logic error into a compile error.

**The cutoff:** pass by value for types that fit in one or two registers and have trivial copies — `int`, `double`, `float`, `char`, `bool`, raw pointers, iterators. Use `const T&` for everything else.

### What `const T&` Gives the Compiler

**Fact:** For references and pointers, `const` gives the compiler real information that affects code generation. The following describes well-established optimizer behavior observable in GCC, Clang, and MSVC at `-O2` and above.

When a function takes `std::vector<Record>&` (non-const), the compiler must assume the function might modify the vector. Any data the caller cached from the vector before the call — its size, a pointer to its first element, values read from it — must be reloaded from memory after the function returns, because the callee might have invalidated all of it.

When the same function takes `const std::vector<Record>&`, the compiler knows the vector's state will not change through this reference. This enables the compiler to keep values it already loaded in registers across the call, avoid redundant memory loads, and in some cases hoist loop-invariant reads out of loops that contain calls to const-ref functions.

This directly affects how aggressively the optimizer can schedule loads and stores around function calls. The more the compiler knows about what *won't* change, the less conservatively it needs to treat memory.

To make this concrete, consider two functions that sum an array of integers and then return the sum plus the vector's size:

```cpp
int sum_and_count(std::vector<int>& v) {        // non-const
    int total = 0;
    for (int x : v) total += x;
    return total + v.size();                     // must reload size — did the loop modify v?
}

int sum_and_count(const std::vector<int>& v) {   // const
    int total = 0;
    for (int x : v) total += x;
    return total + v.size();                     // size cached from before the loop
}
```

**Fact:** On x86-64 with GCC 13.2 at `-O2`, the non-const version reloads the vector's size from memory after the loop, because the compiler cannot prove the loop body didn't modify `v` through an aliased path. The const version hoists the size load before the loop and keeps it in a register throughout. The relevant difference in the generated assembly (simplified, AT&T syntax):

```asm
; --- non-const version (after the summing loop) ---
    mov    rax, QWORD PTR [rdi+8]    ; reload v.end() from memory
    sub    rax, QWORD PTR [rdi]      ; reload v.begin() from memory
    sar    rax, 2                     ; compute size = (end - begin) / sizeof(int)
    add    eax, ecx                   ; add to total

; --- const version (after the summing loop) ---
    add    eax, r12d                  ; r12d already holds size from before the loop
```

The non-const version issues two memory loads (`mov` and `sub` from `[rdi]`); the const version uses a register (`r12d`) that was loaded once before the loop began. In a tight loop that calls `sum_and_count` repeatedly, those extra loads hit L1 cache at best and main memory at worst. The cost scales with call frequency.

The same pattern applies to the "why not `const int&` for small types" discussion above. When a function takes `const int&`, the compiler passes a pointer and dereferences it on every read. With plain `int`, the value lives in a register (e.g., `edi` on x86-64 System V) with zero memory traffic:

```asm
; --- void f(const int& x) ---
    mov    eax, DWORD PTR [rdi]      ; dereference the pointer to read x

; --- void f(int x) ---
    ; x is already in edi — no load needed
```

This is why the "pass small types by value" rule exists: it's not a style preference, it's an instruction-count difference.

---

## Q3: Const Methods

### What `const` on a Method Means

```cpp
class BankAccount {
    double balance_ = 0.0;
public:
    // Const method: promises not to modify *this
    double get_balance() const {
        return balance_;
    }
    
    // Non-const method: may modify *this
    void deposit(double amount) {
        balance_ += amount;
    }
};
```

**A `const` method can be called on a `const` object. A non-const method cannot.**

```cpp
void audit(const BankAccount& account) {
    double b = account.get_balance();  // OK: get_balance() is const
    account.deposit(100);  // Compile error: deposit() is non-const
}
```

### The Const Method Contract

When you mark a method `const`, you promise:
1. The method won't modify any non-mutable member variables
2. The method won't call any non-const methods on `*this`
3. Callers can safely call this on `const` objects

### How Const Participates in Overload Resolution

When a class provides both const and non-const overloads of the same method, the compiler selects which one to call based on the const-qualification of the object — not the caller's intent. This is how `std::vector::operator[]`, `std::map::at()`, and many standard library accessors provide both read-only and read-write access through the same syntax.

The mechanism is straightforward: a `const` method's implicit `this` parameter has type `const T*`, while a non-const method's `this` has type `T*`. When you call `obj.method()`, the compiler matches the const-qualification of `obj` against the available overloads, just like any other overload resolution.

```cpp
class Matrix {
    std::vector<double> data_;
    int cols_;
public:
    Matrix(int rows, int cols) : data_(rows * cols, 0.0), cols_(cols) {}

    // Non-const overload: returns mutable reference
    double& operator()(int r, int c) {
        return data_[r * cols_ + c];
    }

    // Const overload: returns const reference
    const double& operator()(int r, int c) const {
        return data_[r * cols_ + c];
    }
};

void fill(Matrix& m) {
    m(0, 0) = 1.0;    // Calls non-const operator(): m is non-const, returns double&
}

double trace(const Matrix& m) {
    return m(0, 0);    // Calls const operator(): m is const, returns const double&
    // m(0, 0) = 5.0;  // Compile error: const double& is not assignable
}
```

The same syntax — `m(0, 0)` — produces either a mutable or immutable reference depending solely on whether `m` is const. The compiler makes the decision; the caller writes natural code.

This has an important consequence for class design: if you provide only the non-const overload, the method becomes invisible to every function that receives the object by `const T&`. Since the "default to const" rule means most functions take const references, a missing const overload effectively hides the method from most of the codebase. This is why Mistake 1 in the Common Const Mistakes section below is so pervasive — one missing `const` qualifier on a getter cascades into compile errors across every const-correct caller.

### `mutable`: When the Implementation Changes but the Value Doesn't

Sometimes a method is logically read-only — it returns the same answer every time for the same object state — but the implementation needs to modify internal bookkeeping to do its job. The classic case is a cache: `lookup(key)` doesn't change what the object *means*, but it might store the result so the next call is faster. Without `mutable`, you'd be forced to either make `lookup` non-const (wrong — callers shouldn't need non-const access just to read a value) or move the cache outside the object (breaks encapsulation).

The same problem arises with mutexes. A `get_balance()` method on a mutex-protected class needs to lock a mutex to read safely, but locking modifies the mutex. The method is logically const — it doesn't change the balance — but mechanically it mutates internal synchronization state.

`mutable` resolves this tension. It tells the compiler "this member is an implementation detail that can change even when the object's observable state doesn't":

```cpp
class Cache {
    mutable std::unordered_map<int, Result> cache_;

public:
    Result lookup(int key) const {
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
        Result r = expensive_compute(key);
        cache_[key] = r;  // OK: cache_ is mutable
        return r;
    }
};
```

The caller sees a const method that returns a `Result`. Whether the result was computed fresh or served from cache is invisible — the object's logical state is unchanged either way.

**Use `mutable` sparingly** — it is appropriate for caches, mutexes, and instrumentation counters. If you find yourself marking a member `mutable` because a "getter" needs to modify real state, the method isn't actually a getter — reconsider the design.

---

## Q4: Constexpr and Consteval

### `constexpr`: Maybe Compile-Time

```cpp
constexpr int square(int x) {
    return x * x;
}

constexpr int a = square(5);  // Computed at compile time: a = 25
int b = square(runtime_value);  // Computed at runtime (if runtime_value isn't constexpr)
```

**`constexpr` means:** "This *can* be evaluated at compile time if the inputs are known at compile time."

### `constexpr` Has Grown: What You Can Do Today

Many developers carry a C++11 mental model of `constexpr` — a single return statement, no loops, no local variables. That model is years out of date. The relaxations across C++14 through C++23 have made `constexpr` a general-purpose compile-time computation engine, and underestimating it means leaving compile-time validation on the table.

Consider computing a compile-time lookup table. In C++11, this required recursive template tricks or a recursive `constexpr` function with a single return statement:

```cpp
// C++11 style: recursive, single return, hard to read
constexpr int factorial_11(int n) {
    return n <= 1 ? 1 : n * factorial_11(n - 1);
}
```

C++14 relaxed the rules to allow loops, local variables, multiple statements, and mutation of local state — all at compile time:

```cpp
// C++14+: loops, locals, mutation — just normal code
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

constexpr auto f10 = factorial(10);  // Computed at compile time: 3628800
```

C++20 went further. `constexpr` functions can now allocate heap memory (`new`/`delete`), use `std::vector`, `std::string`, and other allocating containers — as long as all allocations are freed before the function returns. This enables non-trivial compile-time data processing:

```cpp
// C++20: std::vector and std::string at compile time
constexpr std::size_t count_unique_words(std::string_view text) {
    std::vector<std::string_view> words;
    std::size_t start = 0;
    bool in_word = false;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        bool is_space = (i == text.size()) || text[i] == ' '
                        || text[i] == '\n' || text[i] == '\t';
        if (is_space && in_word) {
            words.push_back(text.substr(start, i - start));
            in_word = false;
        } else if (!is_space && !in_word) {
            start = i;
            in_word = true;
        }
    }
    // Sort and deduplicate — all at compile time
    std::sort(words.begin(), words.end());
    auto last = std::unique(words.begin(), words.end());
    return static_cast<std::size_t>(last - words.begin());
    // words' heap memory is freed here — required by constexpr rules
}

// Validated at compile time — a duplicate field name is caught before the program runs
constexpr auto kFieldCount = count_unique_words("name age score name rank");
static_assert(kFieldCount == 4, "duplicate field name detected");
```

C++23 extended this further to allow `constexpr` in almost all contexts where runtime code works, including `goto`, labels, and `static` local variables (with restrictions). The trajectory is clear: `constexpr` is converging toward "any function can run at compile time."

The practical consequence: if you find yourself maintaining a hand-rolled compile-time lookup table or a recursive template metafunction, consider whether a `constexpr` function with loops and local variables would be clearer. The answer is almost always yes.

### `consteval`: Always Compile-Time (C++20)

`constexpr` has a subtle problem: it doesn't *guarantee* compile-time evaluation. A `constexpr` function silently falls back to runtime when called with non-constant arguments, and the caller has no way to know. This matters when the whole point of the function is to catch mistakes before the program runs.

Consider a function that converts a port number to a network-byte-order value for socket configuration. If the port is known at compile time, you want the conversion done at compile time — and more importantly, you want an out-of-range port to be a compile error, not a runtime surprise:

```cpp
consteval std::uint16_t port(int p) {
    if (p < 1 || p > 65535) {
        throw "port out of range";  // Compile error — not a runtime exception
    }
    // Network byte order (big-endian)
    return static_cast<std::uint16_t>((p >> 8) | ((p & 0xFF) << 8));
}

constexpr auto kHttpPort = port(80);     // OK: computed and validated at compile time
constexpr auto kBadPort  = port(99999);  // Compile error: "port out of range"
```

If this were `constexpr` instead of `consteval`, someone could accidentally call `port(user_input)` and the validation would happen at runtime — or worse, the optimizer might inline it and eliminate the check entirely. `consteval` makes the contract explicit: this function exists to produce compile-time constants, and calling it with runtime values is an error.

**`consteval` means:** "This *must* be evaluated at compile time. Runtime calls are errors."

The general pattern: use `consteval` when the function's purpose is to validate or transform configuration that should never depend on runtime input — port numbers, hash seeds, lookup tables, protocol magic numbers, compile-time string processing.

### `constexpr` Variables

```cpp
constexpr double PI = 3.14159265358979;  // Compile-time constant
constexpr int MAX_SIZE = 1024;

// Can be used in compile-time contexts
std::array<int, MAX_SIZE> buffer;  // OK: MAX_SIZE is constexpr
```

### `constexpr` vs `const`

The distinction matters because `const` and `constexpr` make different promises. A `const` variable is immutable after initialization, but its initial value might come from a runtime computation — `const int x = get_value()` is perfectly legal. A `constexpr` variable is immutable *and* its value is known at compile time — the compiler can use it in template arguments, array sizes, and other contexts that require constant expressions. Treating them as interchangeable leads to subtle failures: a `const int` initialized from a function call cannot be used as an array size, even though it looks like a constant.

| Feature | `const` | `constexpr` |
|---------|---------|-------------|
| Immutable? | Yes | Yes |
| Compile-time value? | Sometimes (integral types with constant initializer) | Yes (if possible) |
| Can use in array size? | Only integral const with constant initializer | Yes |
| Can use in template args? | Only integral const with constant initializer | Yes |

The real distinction: `constexpr` *guarantees* the value is available for compile-time evaluation. `const` only prevents modification after initialization — it may or may not be a compile-time constant. For integral types initialized with a constant expression, `const` happens to work in compile-time contexts, but this is fragile and unclear. Prefer `constexpr` when you need a compile-time constant.

```cpp
const int a = get_value();      // Runtime constant — value fixed after initialization
constexpr int b = 42;           // Compile-time constant — value known at compile time
const int c = 42;               // Also usable at compile time (integral const with constant
                                // initializer), but constexpr makes the intent explicit

std::array<int, a> arr1;  // Compile error: a is not a constant expression
std::array<int, b> arr2;  // OK: b is constexpr
std::array<int, c> arr3;  // OK: c is integral const initialized with constant expression
```

### `constinit`: Compile-Time Initialization, Runtime Mutability (C++20)

The `const`/`constexpr` discussion has a missing third member: `constinit`. It solves a different problem — the static initialization order fiasco — and understanding it completes the picture.

Global and static variables in C++ are initialized in two phases. *Constant initialization* happens before `main()` and is deterministic — the compiler embeds the initial values directly into the binary. *Dynamic initialization* happens at runtime, in an order that is unspecified across translation units. If global `A` in `file1.cpp` depends on global `B` in `file2.cpp`, and `B` is dynamically initialized, `A` might read an uninitialized `B`. This is the static initialization order fiasco.

`constexpr` prevents this because the variable is fully evaluated at compile time. But `constexpr` also makes the variable `const` — you can never modify it. What if you need a global that is initialized at compile time (avoiding the fiasco) but modified at runtime?

```cpp
constinit int request_count = 0;  // Initialized at compile time (constant initialization)
                                  // NOT const — can be modified at runtime

void handle_request() {
    ++request_count;  // OK: request_count is mutable
}
```

Without `constinit`, the compiler would silently accept `int request_count = compute_initial_value()` and generate a dynamic initializer — introducing the fiasco risk. `constinit` forces the compiler to reject any initializer that isn't a constant expression.

The three keywords occupy distinct points in the design space:

| Keyword | Compile-time init? | Immutable? | Use case |
|---------|-------------------|------------|----------|
| `const` | Sometimes | Yes | Runtime-determined values that shouldn't change |
| `constexpr` | Always | Yes | True compile-time constants |
| `constinit` | Always | No | Global/static variables that need deterministic initialization but runtime mutation |

### `if constexpr`: Compile-Time Branching (C++17)

`if constexpr` applies the same philosophy to control flow: give the compiler enough information to eliminate dead code at compile time, and it will reject invalid code in the eliminated branches rather than letting it compile and lurk.

In a regular `if` statement, both branches must be valid code for every template instantiation, even if one branch is never taken. This forces defensive coding patterns — SFINAE, tag dispatch, or runtime checks — to handle type-dependent behavior:

```cpp
// Runtime if: both branches must compile for ALL types
template<typename T>
std::string to_string(const T& val) {
    if (std::is_integral_v<T>) {
        return std::to_string(val);              // Error for non-integral T:
    } else {                                     // std::to_string(MyClass) doesn't exist
        return val.serialize();                   // Error for int:
    }                                            // int has no .serialize()
}
```

`if constexpr` resolves this. The compiler evaluates the condition at compile time, keeps the taken branch, and discards the other — the discarded branch doesn't even need to be valid code for the current instantiation:

```cpp
// if constexpr: only the taken branch is compiled
template<typename T>
std::string to_string(const T& val) {
    if constexpr (std::is_integral_v<T>) {
        return std::to_string(val);              // Only compiled when T is integral
    } else {
        return val.serialize();                  // Only compiled when T is NOT integral
    }
}

auto a = to_string(42);             // Compiles: takes the integral branch
auto b = to_string(my_object);      // Compiles: takes the serialize branch
```

The connection to const correctness is conceptual: both `const` and `if constexpr` give the compiler information that turns potential runtime errors into compile-time rejections. `const` says "this value won't change" and the compiler rejects mutations. `if constexpr` says "this branch won't be taken" and the compiler discards it entirely. Both are mechanisms for shifting error detection from runtime to compile time — which is the central thesis of this course.

---

## Q5: Where to Use Const

The previous sections covered `const` on function parameters (Q1–Q2) and member functions (Q3). Two placement categories remain that haven't been addressed: local variables and pointers.

### 1. Local Variables (Prevent Accidental Reassignment)

Configuration objects, computed results, and captured snapshots share a property: once initialized, they should not change. If a function loads a config at the top and then uses it throughout, an accidental `config = other_config` halfway through creates a subtle inconsistency — the first half of the function operated under one config, the second half under another. Marking locals `const` catches this the same way const parameters catch accidental mutation: the compiler rejects the reassignment at the point of the mistake.

```cpp
void process() {
    const auto config = load_config();  // Can't accidentally reassign
    
    // config = other_config;  // Compile error
    
    use(config);
}
```

### 2. Pointers (Multiple Levels of Const)

Pointers introduce two independent dimensions of mutation — the pointer itself can be reassigned to point somewhere else, and the pointee can be modified through the pointer. These are separate concerns, and `const` can constrain either one independently. This matters in practice: a function that walks a linked list needs to reassign its traversal pointer but should not modify the nodes. A function that holds a configuration singleton needs to read through a fixed pointer but must never redirect the pointer to a different config. Each scenario calls for a different `const` placement.

```cpp
int x = 10;
int y = 20;

int* p1 = &x;              // Non-const pointer to non-const int
const int* p2 = &x;        // Non-const pointer to const int (can't modify *p2)
int* const p3 = &x;        // Const pointer to non-const int (can't reassign p3)
const int* const p4 = &x;  // Const pointer to const int (can't modify or reassign)

*p1 = 5;   // OK
*p2 = 5;   // Compile error: *p2 is const
p3 = &y;   // Compile error: p3 is const
*p3 = 5;   // OK
*p4 = 5;   // Compile error
p4 = &y;   // Compile error
```

**Read right-to-left:** `const int* const p` = "p is a const pointer to a const int"

This mnemonic works because of C++'s actual parsing rule: `const` applies to whatever is immediately to its left, *unless* it's the leftmost token, in which case it applies to what's to its right. This is why `const int*` and `int const*` are identical — in the first form, `const` is leftmost so it applies rightward to `int`; in the second form, `const` applies leftward to `int`. Either way: pointer to a const int.

The "east const" convention (`int const*`, `int const&`) makes the rule consistent: `const` always applies to its left. The "west const" convention (`const int*`, `const int&`) relies on the special-case leftmost rule. Neither is wrong; both are in wide use. The value of knowing the parsing rule is that it eliminates confusion with complex declarations:

```cpp
const int* const* p;       // West const: pointer to (const pointer to (const int))
int const* const* p;       // East const: same type, reads left-to-right naturally
```

The most common form in practice is `const T*` (or equivalently `T const*`) — a pointer you can redirect but cannot write through. This is what C APIs use for read-only buffers (`const char*`), and it's the pointer equivalent of `const T&`.

---

## Const and Thread Safety

The previous sections covered where to place `const` and what it means for the compiler. This section covers what `const` means when multiple threads are reading — and why `mutable` can silently break that promise.

### Const Enables Concurrent Read Access

```cpp
// Multiple readers, no writers — data-race-free
void reader_thread(const Document& doc) {
    auto content = doc.get_content();  // OK: doc is const, no concurrent mutation
}

// Multiple threads can call this simultaneously — but only because
// Document has no mutable members or unsynchronized internal state
std::vector<std::thread> threads;
const Document doc = load_document();
for (int i = 0; i < 10; i++) {
    threads.emplace_back([&doc]() { reader_thread(doc); });
}
```

**The rule:** Concurrent reads are data-race-free **only if** nothing is mutating the object — including via `mutable` members, shared pointees, or unsynchronized internal caches. `const` helps express read-only intent, but it is not a synchronization primitive.

### The Standard Library's Const Contract

The C++ standard library guarantees:
- Const member functions can be called concurrently from multiple threads
- Non-const member functions require exclusive access

```cpp
const std::vector<int> v = {1, 2, 3};

// Thread 1: const access
int size = v.size();  // OK: size() is const

// Thread 2: const access (concurrent with thread 1)
int first = v[0];  // OK: operator[] on const vector returns const int&

// Thread 3: cannot call non-const methods — v is const
// v.push_back(4);  // Compile error: push_back() is non-const
```

If `v` were non-const, a concurrent call to `push_back` while another thread reads would be a data race — `push_back` requires exclusive access. By declaring `v` const, the compiler prevents this category of mistake at the API level. Note that `const` alone does not *synchronize* access; it prevents the mutation that would make synchronization necessary.

### The Trap: `mutable` Breaks the Const-Means-Concurrent-Safe Rule

The standard library upholds a discipline: const member functions do not introduce data races. Formally, `[res.on.data.races]` requires that standard library const member functions behave as read-only operations on the object's representation. This means multiple threads can safely call `v.size()`, `v[0]`, or `m.find(key)` concurrently on a const container, because the implementations guarantee no hidden writes.

Your own classes get no such guarantee for free. The `Cache` class from Q3 illustrates the trap:

```cpp
class Cache {
    mutable std::unordered_map<int, Result> cache_;

public:
    Result lookup(int key) const {
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
        Result r = expensive_compute(key);
        cache_[key] = r;  // Write to mutable member
        return r;
    }
};
```

A caller who follows the "const means concurrent-read-safe" rule will write:

```cpp
const Cache c;
std::jthread t1([&c]() { c.lookup(1); });
std::jthread t2([&c]() { c.lookup(2); });
// DATA RACE: both threads write to cache_ concurrently
```

The object is `const`. Both calls go through a `const` method. The program has undefined behavior. The `mutable` keyword punched a hole through the const contract, and nothing in the type system warns the caller.

The fix is to pair `mutable` data with `mutable` synchronization. When a class has mutable state that may be accessed concurrently, the mutex that protects it must also be mutable — otherwise the const method can't lock it:

```cpp
class Cache {
    mutable std::mutex mutex_;
    mutable std::unordered_map<int, Result> cache_;

public:
    Result lookup(int key) const {
        std::lock_guard lock(mutex_);  // OK: mutex_ is mutable
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
        Result r = expensive_compute(key);
        cache_[key] = r;
        return r;
    }
};
```

Now concurrent calls to `lookup` serialize on the mutex. The const contract is restored: callers can treat `const Cache&` as concurrent-read-safe because the class itself enforces the synchronization that `mutable` requires.

The discipline: every `mutable` data member should have a corresponding synchronization strategy. A `mutable` cache without a `mutable` mutex is a data race waiting for a second thread. If you audit a class and find `mutable` members without synchronization, either the class is single-threaded-only (document it) or it has a latent race.

---

## Where Const Loses

Const correctness is one of the most cost-effective compile-time safety tools in C++, but it has real boundaries. Understanding them prevents both false confidence and unnecessary frustration.

### Shallow Const

`const` on a container constrains the container, not what the container holds through indirection. A `const std::vector<int*>` prevents you from calling `push_back` or `clear`, but it does not prevent you from modifying the integers through the stored pointers:

```cpp
void modify_through_const(const std::vector<int*>& v) {
    // v.push_back(nullptr);  // Compile error: v is const
    // v.clear();             // Compile error: v is const
    *v[0] = 42;              // Compiles fine: v[0] is int* const, not const int*
}
```

The `const` on the vector makes each element `int* const` — a const pointer to a non-const int. The pointer can't be reassigned, but the int it points to is fully mutable. To prevent writes through the pointer, you need `const` at the pointee level: `std::vector<const int*>`.

The same distinction applies to smart pointers, where it creates a common bug pattern:

```cpp
class Engine {
    const std::unique_ptr<Config> config_;   // const pointer, non-const pointee
public:
    Engine(std::unique_ptr<Config> c) : config_(std::move(c)) {}

    void reconfigure() {
        // config_ = std::make_unique<Config>();  // Compile error: can't reassign
        config_->timeout = 30;                    // Compiles fine: Config is mutable
    }
};
```

If the intent is an immutable configuration, the type should be `std::unique_ptr<const Config>` — the pointer is reassignable but the config is immutable. Or `const std::unique_ptr<const Config>` for both. These are four different types with four different mutation contracts:

```cpp
std::unique_ptr<Config>             // mutable pointer, mutable config
const std::unique_ptr<Config>       // fixed pointer, mutable config
std::unique_ptr<const Config>       // mutable pointer, immutable config
const std::unique_ptr<const Config> // fixed pointer, immutable config
```

The language does not propagate const through pointers automatically. `std::experimental::propagate_const` (Library Fundamentals TS v2) wraps a pointer-like type and makes `const` on the wrapper propagate to the pointee, but it is not yet standardized and has limited compiler support. In practice, choosing the correct combination of const placements is the only portable mechanism.

### The `mutable` Escape Hatch

As discussed in the `mutable` section above, any member marked `mutable` can be modified through a `const` reference. This is by design — caches, mutexes, and instrumentation counters are legitimate uses. But it means that "const object" and "truly immutable object" are not synonymous. A class with `mutable` members may change internal state during ostensibly read-only operations.

### `const_cast` and Undefined Behavior

`const_cast` can strip `const` from a reference, but writing through the result is undefined behavior if the original object was declared `const`. See Mistake 3 below for the full mechanism and why the compiler is entitled to break your code when you do this.

### C API Boundaries

C has `const` but many C APIs predate its adoption or simply don't use it consistently. When calling a C function that takes `char*` but is known not to modify the buffer, the caller must `const_cast` or copy — both of which erode the guarantees that const correctness provides on the C++ side. Wrapping C APIs in const-correct C++ interfaces is the standard mitigation, but the boundary itself remains a gap.

### Viral Propagation

Adding `const` to one function parameter or method can cascade: every function it calls must also be const-correct, every getter it invokes must be marked `const`, and every reference it passes along must be `const T&`. In a codebase that didn't start with const correctness, retrofitting it is a bottom-up process that can touch dozens of files. This is not a flaw in `const` — it's a consequence of `const` actually enforcing what it promises — but it is a real adoption cost.

---

## Const Blocks Move Semantics

This is the one case where "default to const" has a genuine performance cost. You cannot move from a `const` object. `std::move(x)` on a `const T` produces a `const T&&`, which cannot bind to a move constructor (move constructors take `T&&`, not `const T&&`). The result: the copy constructor is silently selected instead.

Consider a function that builds a large result and returns it:

```cpp
std::vector<std::string> build_report() {
    const auto lines = generate_lines();   // const local
    // ... maybe log lines.size() ...
    return lines;                          // COPIES — cannot move from const
}
```

Without `const`, the compiler can apply Named Return Value Optimization (NRVO) to eliminate the copy entirely, or failing that, implicitly move from the local variable into the return value. With `const`, NRVO is still *permitted* by the standard but some compilers won't apply it (the rules are implementation-defined), and the implicit move fallback is blocked — the `const` qualifier prevents binding to the move constructor.

The cost is real: for a `std::vector<std::string>` with 10,000 elements, the difference between a move (constant time — pointer swap) and a copy (linear time — allocate new buffer, copy every string, each of which allocates and copies its own buffer) can be measured in microseconds for the move vs. milliseconds for the copy.

The assembly tells the story. With a non-const local, the return path moves three pointer-sized values (data, size, capacity). With a const local and NRVO disabled, the return path calls `operator new`, enters a loop that copies each element, and calls `operator new` again inside each string copy.

The guideline: mark local variables `const` when they won't be moved from. For variables that will be returned, passed to a sink parameter (`T&&` or by value), or transferred into a container via `emplace` or `push_back`, leave them non-const. The "default to const" rule applies to the variable's *lifetime* — if the variable lives and dies in the same scope without being consumed, const is correct. If the variable is a resource that will be moved elsewhere, const is a performance trap.

---

## Common Const Mistakes

### Mistake 1: Forgetting Const on Getters

The most common const mistake in practice, and the one most likely to unravel const correctness across a codebase. The mechanism is explained in Q3's overload resolution section — a missing `const` on a getter hides it from every function that takes `const T&`. Here's what it looks like in review:

```cpp
class Person {
    std::string name_;
public:
    // BAD: Not const — can't call on const Person
    std::string get_name() { return name_; }
    
    // GOOD: Const — can call on any Person
    std::string get_name() const { return name_; }
};
```

The temptation is to remove `const` from the caller's parameter to make it compile. That's the wrong fix — it propagates the missing const outward instead of fixing it at the source.

### Mistake 2: Returning Non-Const Reference to Internal Data

When a class manages invariants — say, keeping a sorted vector, or ensuring a size counter stays in sync with a collection — those invariants depend on all modifications going through the class's own methods. Returning a non-const reference to internal state creates a backdoor: any caller can modify the data directly, bypassing the methods that maintain the invariants. The class can no longer guarantee anything about its own state.

The fix is to provide a const overload that returns `const T&` for read-only access. If mutable access is genuinely needed, a separate non-const overload makes the intent explicit — but you're consciously handing the caller a way to break invariants, so it should be a deliberate design choice rather than the default.

```cpp
class Container {
    std::vector<int> data_;
public:
    // GOOD: Provide a const overload for read-only access
    const std::vector<int>& get_data() const { return data_; }

    // Non-const overload only if mutable access is genuinely needed
    std::vector<int>& get_data() { return data_; }
};
```

### Mistake 3: Const-Casting Away Const

When you declare an object `const`, the compiler is permitted to make assumptions based on that promise. It may place the object in read-only memory (especially for globals and string literals). It may optimize away repeated reads, substituting the known value directly. It may reorder or eliminate stores because it knows no legal write can occur. These optimizations are valid precisely because the language guarantees that a `const` object's value will never change.

`const_cast` breaks this contract. When you cast away const and write through the resulting reference, the compiler's assumptions become wrong — but the compiler has already acted on them. The generated code may never see your write (because the read was hoisted or folded), may crash on a write-protected page, or may produce results that change between optimization levels. This is why the standard says modifying an originally-const object is undefined behavior — not "bad practice," but a category of bug where the compiler is free to generate any code at all.

```cpp
void bad_function(const std::string& s) {
    std::string& mutable_s = const_cast<std::string&>(s);
    mutable_s = "modified";  // UB if s refers to an originally-const object
}

const std::string original = "hello";
bad_function(original);  // Undefined behavior: original was declared const
```

`const_cast` does have one legitimate use: interfacing with legacy APIs that lack const correctness but are known not to modify the argument. Even then, you must be certain the underlying object was not originally declared `const` — the cast produces defined behavior only when the object was born non-const and acquired `const` along the way (e.g., passed through a `const T&` parameter).

### Mistake 4: Not Using Const for Loop Variables

Range-based `for` loops have the same const issue as function parameters: `auto&` lets the loop body modify each element, and nothing in the code signals whether that modification is intentional. When a later maintainer adds a line that accidentally writes through `r`, the bug is invisible — the loop compiled before, it compiles now, and the mutation escapes silently. `const auto&` makes the read-only intent explicit and catches accidental writes at the point they're introduced.

```cpp
std::vector<Record> records = load_records();

// BAD: Accidentally modifiable
for (auto& r : records) {
    process(r);  // Did process() modify r? Who knows.
}

// GOOD: Clearly read-only
for (const auto& r : records) {
    process(r);  // Compile error if process() tries to modify
}
```

### Mistake 5: Forgetting That Lambda Captures Are Const by Default

A lambda that captures by value makes its captures `const` by default. This surprises developers who expect captured variables to behave like local variables inside the lambda body:

```cpp
void process_items(const std::vector<Item>& items) {
    int count = 0;
    std::for_each(items.begin(), items.end(), [count](const Item& item) {
        if (item.is_valid()) {
            ++count;  // Compile error: count is const inside the lambda
        }
    });
}
```

The reason connects back to Q3: a lambda is a compiler-generated class whose `operator()` is `const` by default. The captured `count` is a member variable of that class. A const method cannot modify non-mutable members — so the captured value is effectively immutable.

The `mutable` keyword on the lambda makes `operator()` non-const, allowing modification of captured values:

```cpp
std::for_each(items.begin(), items.end(), [count](const Item& item) mutable {
    if (item.is_valid()) {
        ++count;  // OK: lambda is mutable, operator() is non-const
    }
});
// But count here is still 0 — the lambda modified its own copy
```

Note the second trap: even with `mutable`, the lambda captured `count` *by value* — it has its own copy. The original `count` outside the lambda is unchanged. If you need the lambda to modify the caller's variable, capture by reference (`[&count]`), in which case `mutable` is unnecessary because the lambda isn't modifying its own member — it's writing through a reference.

The broader pattern: lambdas are objects, and their const behavior follows the same rules as any class. Understanding this connection — captured values are members, `operator()` is const by default, `mutable` makes it non-const — eliminates the surprise.

---

## Guarantees / Non-Guarantees

| Property | Guaranteed? | Conditions | Notes |
|----------|-------------|------------|-------|
| Mutation through `const T&` rejected at compile time | ✅ Yes | All cases | Core language guarantee |
| Non-const methods inaccessible on `const` objects | ✅ Yes | All cases | Core language guarantee |
| Const overload selected for `const` objects | ✅ Yes | Both overloads exist | Overload resolution based on `this` qualification |
| `constexpr` variables available at compile time | ✅ Yes | Initializer is a constant expression | Required by the standard |
| `consteval` functions evaluated at compile time | ✅ Yes | C++20 and later | Runtime calls are compile errors |
| `constinit` prevents dynamic initialization | ✅ Yes | C++20 and later | Variable is still mutable after initialization |
| `if constexpr` discards untaken branches | ✅ Yes | C++17 and later | Discarded branch need not be valid for the instantiation |
| Lambda captures immutable by default | ✅ Yes | Capture by value without `mutable` | `operator()` is const by default |
| Deep immutability through pointers/references | ❌ No | `const` is shallow | Must add `const` at each indirection level |
| Move semantics work on `const` objects | ❌ No | `std::move` on `const T` yields `const T&&` | Silently degrades to copy |
| Thread safety from `const` alone | ❌ No | `const` prevents mutation, not races | `mutable` members can introduce data races in const methods |
| Protection against `const_cast` misuse | ❌ No | `const_cast` + write = UB if originally const | Discipline and code review are the only guards |
| `mutable` members remain unchanged | ❌ No | By design | Pair every `mutable` data member with a synchronization strategy |
| Const propagation across C API boundaries | ❌ No | C APIs may lack `const` annotations | Wrap in const-correct C++ interfaces |

---

## Const Correctness Audit Checklist

When reviewing code, check:

| Item | Question |
|------|----------|
| Parameters | Could this be `const T&` instead of `T&`? |
| Return types | Am I exposing internal state for modification? |
| Member functions | Is this logically const? Mark it. |
| Const overloads | Does this class need both const and non-const overloads for accessors? |
| Local variables | Will this be reassigned? If not, make it `const`. |
| Local variables (move) | Will this variable be returned or moved from? If so, do *not* make it `const`. |
| Loop variables | Am I modifying elements? If not, use `const auto&`. |
| Pointers | Is the pointee modified? Is the pointer reassigned? Is `const` at the right level of indirection? |
| `mutable` members | Does every `mutable` data member have a synchronization strategy? |
| Lambda captures | Are value-captured variables modified? If so, is `mutable` on the lambda, or should the capture be by reference? |
| Globals/statics | Could this use `constexpr` (compile-time + immutable) or `constinit` (compile-time + mutable)? |

---

## Summary

| Problem | Solution |
|---------|----------|
| Accidental mutation | Use `const T&` for read-only parameters |
| Surprise side effects | Mark non-modifying methods `const` |
| Thread-unsafe sharing | `const` makes read-only intent explicit; concurrent read access still depends on the object being truly immutable (no `mutable` caches, no internal synchronization side effects, etc.). |
| `mutable` data races | Pair every `mutable` data member with `mutable` synchronization |
| Unclear API intent | `const` documents read-only vs. read-write |
| Runtime "constants" | Use `constexpr` for compile-time constants |
| Static init order fiasco | Use `constinit` for compile-time initialization of mutable globals |
| Type-dependent dead branches | Use `if constexpr` to discard invalid branches at compile time |
| Silent copy instead of move | Do not `const`-qualify local variables that will be returned or moved from |
| Shallow const surprise | Place `const` at each level of indirection: `const T*`, `unique_ptr<const T>` |

### Key Principles

1. **Default to const** — Remove it only when mutation is needed, or when the variable will be moved from
2. **Const is documentation** — It tells callers what to expect
3. **Const is enforced** — The compiler rejects violations
4. **Const can enable concurrent read access** — but it does not replace real thread-safety guarantees (watch for `mutable`, shared ownership, and internal state changes)
5. **Const propagates** — A const object has const members, but const does not propagate through pointers
6. **Const participates in overload resolution** — The compiler selects const vs. non-const overloads based on the object's qualification
7. **`mutable` requires synchronization** — Every mutable data member needs a concurrent-access strategy

### The Guideline in One Sentence

> Use `const` everywhere you can. The compiler will tell you where you can't.

---

## Exercises

1. **Audit:** Take a class from your codebase. Mark every getter `const`. How many compile errors do you get? Fix them.

2. **Refactor:** Find a function that takes `T&` but doesn't modify its argument. Change to `const T&`. Does anything break?

3. **Constexpr:** Create a `constexpr` function that computes factorial. Verify it works at compile time by using the result as an array size.

4. **Concurrent access:** Write a `SharedConfig` class where:
   - Multiple threads can read concurrently
   - Writes require exclusive access
   - Use `const` to enforce the reader interface

5. **The mutable trap:** Take the `Cache` class from Q3 and write a test that launches two threads calling `lookup()` concurrently. Run it under ThreadSanitizer (`-fsanitize=thread`). Verify that TSan reports the data race. Then add `mutable std::mutex` and verify the race disappears.

6. **Move vs. const:** Write a function that builds a `std::vector<std::string>` with 10,000 elements. Return it with and without `const` on the local variable. Measure the time difference with `-O2` and NRVO disabled (`-fno-elide-constructors` on GCC/Clang). Explain the results.

7. **Shallow const:** Create a `const std::vector<std::unique_ptr<Widget>>`. Attempt to (a) add an element, (b) remove an element, (c) call a non-const method on a stored Widget. Which operations compile? Fix the type to prevent (c).

8. **`if constexpr`:** Write a `print_value<T>()` function template that uses `if constexpr` to choose between `std::to_string()` for arithmetic types, `.str()` for types with that method, and a fallback that triggers a `static_assert` with a helpful error message. Verify that instantiating with an unsupported type produces your custom error, not a wall of template noise.

---

## Further Reading

**C++ Core Guidelines:**
- [Con: Constants and Immutability](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-const)
- Con.1: By default, make objects immutable
- Con.2: By default, make member functions const
- Con.3: By default, pass pointers and references to consts
- Con.4: Use const to define objects with values that do not change after construction

**Books:**
- "Effective C++" (Scott Meyers) — Item 3: Use const whenever possible
- "C++ Coding Standards" (Sutter & Alexandrescu) — Item 15: Use const proactively

**Talks:**
- "const and constexpr" — Jason Turner, CppCon
- "Const Correctness in C++" — Kate Gregory

**Tools:**
- [Compiler Explorer (Godbolt)](https://godbolt.org) — reproduce the assembly examples from Q2 with GCC, Clang, or MSVC at various optimization levels
