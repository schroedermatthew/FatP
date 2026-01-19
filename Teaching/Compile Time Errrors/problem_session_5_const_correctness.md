# Problem-Solving Session 5: The Accidental Mutation

## Const Correctness: Immutability Enforced by the Compiler

**Estimated time:** 45–60 minutes  
**Prerequisites:** Basic C++ references and pointers  
**Fat-P components:** None (native C++ feature)

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

double calculate_average(std::vector<Record>& records) {
    // "Normalize" categories for consistency
    for (auto& r : records) {
        std::transform(r.category.begin(), r.category.end(), 
                       r.category.begin(), ::tolower);
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

## Questions to Consider

Before reading further, think about:

1. **Q1:** How could the compiler have prevented this?
2. **Q2:** What's the difference between `const T&`, `T&`, and `const T*`?
3. **Q3:** What does `const` on a method mean?
4. **Q4:** What about `constexpr` and `consteval`?
5. **Q5:** When should you use `const`?

---

## Q1: The Solution Was Always There

The fix is simple — use `const`:

```cpp
double calculate_average(const std::vector<Record>& records) {
    for (auto& r : records) {
        std::transform(r.category.begin(), r.category.end(),  // Compile error!
                       r.category.begin(), ::tolower);
    }
    // ...
}
```

With `const`, the compiler rejects the mutation:

```
error: cannot assign to variable 'r' with const-qualified type 'const Record&'
```

**The accidental mutation becomes a compile error.**

---

## Q2: Const Reference vs Non-Const Reference

### The Three Ways to Pass

```cpp
void by_value(std::vector<int> v);        // Copy — can modify, changes don't escape
void by_ref(std::vector<int>& v);         // Reference — can modify, changes escape
void by_const_ref(const std::vector<int>& v);  // Const ref — cannot modify
```

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

**Rule of thumb:** Use `const T&` by default. Only remove `const` when you need to modify.

---

## Q3: Const Methods

### What `const` on a Method Means

```cpp
class BankAccount {
    double balance_;
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

```cpp
class Cache {
    mutable std::unordered_map<int, Result> cache_;  // mutable: can modify in const methods
    
public:
    Result lookup(int key) const {
        // Can modify cache_ because it's mutable
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
        Result r = expensive_compute(key);
        cache_[key] = r;  // OK: cache_ is mutable
        return r;
    }
};
```

**Use `mutable` sparingly** — only for caches, mutexes, and other implementation details that don't affect logical constness.

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

### `consteval`: Always Compile-Time (C++20)

```cpp
consteval int cube(int x) {
    return x * x * x;
}

constexpr int a = cube(3);  // OK: computed at compile time
int x = get_input();
int b = cube(x);  // Compile error: cube() must be evaluated at compile time
```

**`consteval` means:** "This *must* be evaluated at compile time. Runtime calls are errors."

### `constexpr` Variables

```cpp
constexpr double PI = 3.14159265358979;  // Compile-time constant
constexpr int MAX_SIZE = 1024;

// Can be used in compile-time contexts
std::array<int, MAX_SIZE> buffer;  // OK: MAX_SIZE is constexpr
```

### `constexpr` vs `const`

| Feature | `const` | `constexpr` |
|---------|---------|-------------|
| Immutable? | Yes | Yes |
| Compile-time value? | Maybe | Yes (if possible) |
| Can use in array size? | No | Yes |
| Can use in template args? | No | Yes |

```cpp
const int a = get_value();      // Runtime constant — value fixed after initialization
constexpr int b = 42;           // Compile-time constant — value known at compile time

std::array<int, a> arr1;  // Compile error: a is not constexpr
std::array<int, b> arr2;  // OK: b is constexpr
```

---

## Q5: Where to Use Const

### 1. Function Parameters (Read-Only Access)

```cpp
// BAD
void process(std::vector<int>& data);  // Might modify

// GOOD  
void process(const std::vector<int>& data);  // Clearly read-only
```

### 2. Return Types (Prevent Modification of Internals)

```cpp
class Document {
    std::string content_;
public:
    // BAD: Caller can modify internal state
    std::string& get_content() { return content_; }
    
    // GOOD: Read-only access
    const std::string& get_content() const { return content_; }
};
```

### 3. Member Functions (Promise No Modification)

```cpp
class Rectangle {
    double width_, height_;
public:
    // GOOD: Getters should be const
    double area() const { return width_ * height_; }
    double perimeter() const { return 2 * (width_ + height_); }
    
    // GOOD: Setters are non-const
    void set_width(double w) { width_ = w; }
};
```

### 4. Local Variables (Prevent Accidental Reassignment)

```cpp
void process() {
    const auto config = load_config();  // Can't accidentally reassign
    
    // config = other_config;  // Compile error
    
    use(config);
}
```

### 5. Pointers (Multiple Levels of Const)

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

---

## Const and Thread Safety

### Const Enables Safe Sharing

```cpp
// Thread-safe: multiple readers, no writers
void reader_thread(const Document& doc) {
    auto content = doc.get_content();  // Safe: doc is const
}

// Multiple threads can call this simultaneously
std::vector<std::thread> threads;
const Document doc = load_document();
for (int i = 0; i < 10; i++) {
    threads.emplace_back(reader_thread, std::cref(doc));
}
```

**The rule:** Const objects can be safely shared across threads (assuming no mutable members with unsynchronized access).

### The Standard Library's Const Contract

The C++ standard library guarantees:
- Const member functions can be called concurrently from multiple threads
- Non-const member functions require exclusive access

```cpp
std::vector<int> v = {1, 2, 3};

// Thread 1: const access
int size = v.size();  // OK: size() is const

// Thread 2: const access (concurrent with thread 1)
int first = v[0];  // OK: operator[] const is const

// Thread 3: non-const access — DATA RACE if concurrent with 1 or 2
v.push_back(4);  // Requires exclusive access
```

---

## Common Const Mistakes

### Mistake 1: Forgetting Const on Getters

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

### Mistake 2: Returning Non-Const Reference to Internal Data

```cpp
class Container {
    std::vector<int> data_;
public:
    // BAD: Exposes internal state for modification
    std::vector<int>& get_data() { return data_; }
    
    // GOOD: Read-only access
    const std::vector<int>& get_data() const { return data_; }
    
    // ALSO GOOD: Return by value for small types
    std::vector<int> get_data_copy() const { return data_; }
};
```

### Mistake 3: Const-Casting Away Const

```cpp
void bad_function(const std::string& s) {
    // TERRIBLE: Undefined behavior if s was originally const
    std::string& mutable_s = const_cast<std::string&>(s);
    mutable_s = "modified";
}

const std::string original = "hello";
bad_function(original);  // UB: modifying a const object
```

**Rule:** Never use `const_cast` to remove const unless you're absolutely certain the underlying object is non-const (and even then, reconsider your design).

### Mistake 4: Not Using Const for Loop Variables

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

---

## Const Correctness Audit Checklist

When reviewing code, check:

| Item | Question |
|------|----------|
| Parameters | Could this be `const T&` instead of `T&`? |
| Return types | Am I exposing internal state for modification? |
| Member functions | Is this logically const? Mark it. |
| Local variables | Will this be reassigned? If not, make it `const`. |
| Loop variables | Am I modifying elements? If not, use `const auto&`. |
| Pointers | Is the pointee modified? Is the pointer reassigned? |

---

## Summary

| Problem | Solution |
|---------|----------|
| Accidental mutation | Use `const T&` for read-only parameters |
| Surprise side effects | Mark non-modifying methods `const` |
| Thread-unsafe sharing | `const` objects are safe to share |
| Unclear API intent | `const` documents read-only vs. read-write |
| Runtime "constants" | Use `constexpr` for compile-time constants |

### Key Principles

1. **Default to const** — Remove it only when mutation is needed
2. **Const is documentation** — It tells callers what to expect
3. **Const is enforced** — The compiler rejects violations
4. **Const enables concurrency** — Safe sharing without locks
5. **Const propagates** — A const object has const members

### The Guideline in One Sentence

> Use `const` everywhere you can. The compiler will tell you where you can't.

---

## Exercises

1. **Audit:** Take a class from your codebase. Mark every getter `const`. How many compile errors do you get? Fix them.

2. **Refactor:** Find a function that takes `T&` but doesn't modify its argument. Change to `const T&`. Does anything break?

3. **Constexpr:** Create a `constexpr` function that computes factorial. Verify it works at compile time by using the result as an array size.

4. **Thread safety:** Write a `SharedConfig` class where:
   - Multiple threads can read concurrently
   - Writes require exclusive access
   - Use `const` to enforce the reader interface

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
