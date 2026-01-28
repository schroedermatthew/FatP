# Handbook: Safe Reference Patterns

## Preventing Dangling References Through Type Design

**Estimated time:** 25–35 minutes  
**Prerequisites:** References, rvalue references, std::string_view  
**Guarantee:** ✅ Compile-time (for deleted overloads), ⚠ Runtime (for lifetime issues)

---

## The Problem: References Can Dangle

References and views don't own their data. If the underlying data is destroyed, the reference dangles:

```cpp
std::string_view get_greeting() {
    std::string local = "Hello, World!";
    return local;  // DANGER: returns view to destroyed string
}

void use() {
    std::string_view sv = get_greeting();  // sv dangles!
    std::cout << sv;  // Undefined behavior
}
```

The compiler often doesn't warn. The code compiles, runs, and corrupts memory silently.

---

## Pattern 1: Delete Rvalue Overloads

Prevent binding to temporaries that will be destroyed:

```cpp
class StringView {
    const char* data_;
    size_t size_;
public:
    // OK: binds to lvalue, which must outlive the view
    StringView(const std::string& s) 
        : data_(s.data()), size_(s.size()) {}
    
    // DELETED: would bind to temporary that's about to die
    StringView(std::string&&) = delete;
    
    const char* data() const { return data_; }
    size_t size() const { return size_; }
};

std::string persistent = "hello";
StringView sv1(persistent);              // OK: persistent outlives sv1

StringView sv2(std::string("temp"));     // Compile error: deleted
StringView sv3(get_string());            // Compile error: deleted (if returns by value)
```

### Standard Library Examples

`std::string_view` doesn't delete the rvalue overload for compatibility, but you can wrap it:

```cpp
class SafeStringView {
    std::string_view view_;
public:
    SafeStringView(const std::string& s) : view_(s) {}
    SafeStringView(std::string&&) = delete;
    
    // Also delete construction from temporary char arrays
    template<size_t N>
    SafeStringView(char (&&)[N]) = delete;
    
    // Forward string_view interface
    const char* data() const { return view_.data(); }
    size_t size() const { return view_.size(); }
    // ...
};
```

---

## Pattern 2: Explicit Lifetime Annotation

Document lifetime requirements in the type:

```cpp
// T is borrowed from Owner
template<typename T, typename Owner>
class BorrowedRef {
    T* ptr_;
public:
    // Constructor requires both the reference and its owner
    BorrowedRef(T& ref, Owner& owner) : ptr_(&ref) {
        // Could store weak_ptr to owner for debug checking
    }
    
    T& get() const { return *ptr_; }
    T* operator->() const { return ptr_; }
};

class Document {
    std::string title_;
public:
    // Return type documents: "this reference is borrowed from Document"
    BorrowedRef<const std::string, Document> title() {
        return {title_, *this};
    }
};
```

The type makes the borrowing relationship explicit, though it doesn't enforce lifetime at compile time.

---

## Pattern 3: Return by Value for Small Types

Avoid reference returns when copying is cheap:

```cpp
class Config {
    std::string name_;       // Might be large
    int timeout_ms_ = 1000;  // Small
    
public:
    // Reference for potentially large string
    const std::string& name() const { return name_; }
    
    // Value for small int—no dangling possible
    int timeout_ms() const { return timeout_ms_; }
    
    // For optional values, return std::optional by value
    std::optional<int> max_retries() const { return max_retries_; }
};
```

**Guideline:** Return by value if `sizeof(T) <= 2 * sizeof(void*)` and T is trivially copyable.

---

## Pattern 4: The Owner Keeps the Reference Alive

When returning references, ensure the owner outlives all references:

```cpp
class Cache {
    std::unordered_map<int, std::string> data_;
    
public:
    // DANGEROUS: reference invalidated if map rehashes
    const std::string& get_dangerous(int key) {
        return data_[key];
    }
    
    // SAFER: return a copy
    std::string get_safe(int key) {
        return data_[key];
    }
    
    // SAFEST: use stable addressing (std::map, not unordered_map)
    // or document the invalidation rules clearly
};
```

### Container Invalidation Rules

| Container | References invalidated by |
|-----------|--------------------------|
| `std::vector` | push_back (if reallocates), insert, resize |
| `std::deque` | insert/erase in middle |
| `std::list` | Never (except erase of that element) |
| `std::map` | Never (except erase of that element) |
| `std::unordered_map` | rehash, reserve, insert (if rehashes) |

---

## Pattern 5: Span with Explicit Lifetime

`std::span` has the same dangling problem as string_view. Wrap it safely:

```cpp
template<typename T>
class SafeSpan {
    std::span<T> span_;
public:
    // Only accept lvalue containers
    template<typename Container>
    SafeSpan(Container& c) : span_(c) {}
    
    // Delete rvalue containers
    template<typename Container>
    SafeSpan(Container&&) = delete;
    
    // Forward span interface
    T* data() const { return span_.data(); }
    size_t size() const { return span_.size(); }
    T& operator[](size_t i) const { return span_[i]; }
    auto begin() const { return span_.begin(); }
    auto end() const { return span_.end(); }
};

std::vector<int> vec = {1, 2, 3};
SafeSpan<int> good(vec);                    // OK
SafeSpan<int> bad(std::vector<int>{1,2,3}); // Compile error
```

---

## Pattern 6: Factory Functions Return Owned Types

When a function creates data, return owned types, not references:

```cpp
// BAD: who owns this string?
const std::string& get_default_name();  // Static? Member? Dangling?

// GOOD: caller owns the result
std::string get_default_name();

// BAD: dangling if computed
const Config& get_config(int id);  // What if id not found?

// GOOD: optional owned value
std::optional<Config> get_config(int id);

// OK: reference to long-lived data (document lifetime!)
// "Returns reference to global config. Valid for program lifetime."
const Config& get_global_config();
```

---

## Pattern 7: Clang Lifetime Annotations

Clang supports `[[clang::lifetimebound]]` to catch some dangling issues:

```cpp
class Person {
    std::string name_;
public:
    // Annotation tells Clang: returned reference lifetime bound to *this
    [[clang::lifetimebound]]
    const std::string& name() const { return name_; }
};

const std::string& bad = Person{"temp"}.name();  // Clang warning: dangling

// Enable with: -Wdangling or -Wreturn-stack-address
```

Not standard C++, but helpful where available.

---

## Pattern 8: Proxy Objects for Safe Access

Return a proxy that copies on conversion:

```cpp
class StringProxy {
    const std::string* ptr_;  // Non-owning
public:
    explicit StringProxy(const std::string& s) : ptr_(&s) {}
    
    // Convert to string_view only in expressions (temporary is OK)
    operator std::string_view() const { return *ptr_; }
    
    // Convert to string for storage (makes a copy)
    operator std::string() const { return *ptr_; }
    
    // Prevent storing the proxy itself
    StringProxy(const StringProxy&) = delete;
    StringProxy& operator=(const StringProxy&) = delete;
};

class Config {
public:
    StringProxy name() const { return StringProxy{name_}; }
};

// Usage
Config c;
std::cout << c.name();           // OK: temporary string_view
std::string stored = c.name();   // OK: copies to owned string
auto proxy = c.name();           // Error: can't copy proxy
```

---

## Summary: Safe Reference Checklist

| Situation | Safe Pattern |
|-----------|--------------|
| View of string parameter | Delete rvalue overload |
| Return reference to member | Document lifetime, consider copy |
| Return computed value | Return by value |
| Optional reference | `std::optional<std::reference_wrapper<T>>` |
| Container element access | Document invalidation rules |
| Factory function | Return owned type |
| Span of container | Delete rvalue container overload |

### Common Mistakes

```cpp
// MISTAKE: string_view from temporary
std::string_view sv = get_string();  // If get_string() returns by value → dangling

// MISTAKE: reference to container element after modification
auto& elem = vec[0];
vec.push_back(x);  // May reallocate!
use(elem);         // Dangling if reallocated

// MISTAKE: reference to optional's value
auto& val = opt.value();
opt.reset();       // Destroys value
use(val);          // Dangling

// MISTAKE: returning reference to local
const std::string& bad() {
    std::string local = "oops";
    return local;  // Dangling!
}
```

---

## Summary

| Pattern | Prevents |
|---------|----------|
| Delete rvalue overloads | Binding to temporaries |
| Return by value | Dangling references |
| Explicit lifetime types | Unclear ownership |
| Stable containers | Iterator/reference invalidation |
| Lifetime annotations | Compiler-detected dangling |
| Proxy objects | Accidentally storing references |

### Key Principles

1. **Delete rvalue overloads** for view types—prevents binding to temporaries

2. **Return by value** when copying is cheap—eliminates lifetime issues

3. **Document lifetime requirements** when returning references

4. **Know container invalidation rules** — `vector` and `unordered_map` are dangerous

5. **Use compiler annotations** where available—`[[clang::lifetimebound]]`

### The Guideline in One Sentence

> Delete rvalue overloads on view types to prevent dangling at compile time.

---

## Further Reading

- Session 6: Non-Null References — reference vs pointer decisions
- [C++ Core Guidelines F.42](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f42-return-a-t-to-indicate-a-position-only-when-null-is-a-valid-position)
- [P0936: std::string_view and dangling](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0936r0.pdf)
