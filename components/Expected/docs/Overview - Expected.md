# Expected: A Fat-P Library Showcase

## Executive Summary

Expected is a **storage-policy-customizable** result type that represents either a success value or an error without exceptions. Unlike C++23's `std::expected` (fixed storage, no customization), fat_p Expected lets you choose between **TrivialStorage** (zero-overhead for trivial types), **UnionStorage** (manual lifetime, minimal overhead), and **VariantStorage** (debug-friendly, bounds-checked). The complete monadic interface (`map`, `and_then`, `or_else`, `transform_error`) enables **railway-oriented programming** where errors propagate automatically through function chains.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// Exception-based: hidden control flow
std::string readConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("File not found");  // Hidden jump
    std::string content;
    if (!(file >> content)) throw std::runtime_error("Read failed");
    return content;
}

void processConfig() {
    auto config = readConfig("app.cfg");  // Can throw—invisible at call site!
}

// Error code: manual propagation, out-parameters
int readConfig(const std::string& path, std::string* out) {
    std::ifstream file(path);
    if (!file) return -1;  // Caller must check
    if (!(file >> *out)) return -2;  // Caller must check
    return 0;
}
// Problem: Easy to forget checks. No composition. Ugly out-parameters.
```

| Issue | HPC Impact |
|-------|------------|
| Exception overhead | Stack unwinding costs 1000x normal return in hot paths |
| Hidden control flow | Can't reason about throw points without reading all code |
| Error code tedium | Manual propagation, out-parameters, forgotten checks |
| No composition | Can't chain fallible operations elegantly |

### The Standard's Limitation

C++23's `std::expected` provides monadic operations but:
- **No storage policy customization**—implementation-defined
- **No `inspect()`**—can't peek at value without extracting
- **No `value_or_else()`**—must write verbose conditionals
- **No trivial storage optimization**—may waste space for trivial types

---

## Architecture: Policy-Based Storage

### The Mechanism: Compile-Time Storage Selection

```cpp
template<typename T, typename E, 
         template<typename, typename> class StoragePolicy = AutoStorage>
class Expected {
    StoragePolicy<T, E> storage_;
    bool has_value_;
    
    // AutoStorage selects:
    // - TrivialStorage for trivially copyable T and E
    // - UnionStorage otherwise
};

// Storage policies
template<typename T, typename E>
struct TrivialStorage {
    union { T value; E error; };  // Zero overhead for trivial types
};

template<typename T, typename E>
struct UnionStorage {
    union { T value; E error; };
    // Manual placement new/destroy for non-trivial types
};

template<typename T, typename E>
struct VariantStorage {
    std::variant<T, E> data;  // Debug-friendly, bounds-checked
};
```

**Why storage policies matter:**

| Policy | Size | Debug | Use Case |
|--------|------|-------|----------|
| TrivialStorage | `sizeof(T) + 1` | Minimal | Hot paths with POD types |
| UnionStorage | `max(sizeof(T), sizeof(E)) + 1` | Minimal | General production |
| VariantStorage | `sizeof(variant<T,E>)` | Full bounds checking | Debug builds |

### Memory Layout

```cpp
Expected<int, Error> result;  // TrivialStorage

// sizeof(Expected<int, Error>) == max(sizeof(int), sizeof(Error)) + 1 (bool)
// Contrast: std::optional<std::variant<int, Error>> adds variant overhead
```

---

## Feature Inventory

### 1. Value-or-Error Construction

```cpp
Expected<int, std::string> success{42};
Expected<int, std::string> failure{unexpect, "File not found"};

// In-place construction
Expected<std::vector<int>, Error> vec{std::in_place, {1, 2, 3}};
Expected<int, ComplexError> err{unexpect, std::in_place, arg1, arg2};
```

### 2. Monadic Operations: Railway-Oriented Programming

```cpp
Expected<Config, Error> loadConfig(const std::string& path);
Expected<Settings, Error> parseConfig(const Config& cfg);
Expected<App, Error> createApp(const Settings& s);

// Chain operations—errors propagate automatically
auto result = loadConfig("app.cfg")
    .and_then(parseConfig)      // Only runs if loadConfig succeeded
    .and_then(createApp);       // Only runs if parseConfig succeeded

// result is Expected<App, Error>
// If any step failed, result contains that error
```

**The Operations:**

| Operation | Signature | Behavior |
|-----------|-----------|----------|
| `map(f)` | `(T→U) → Expected<U,E>` | Transform value, propagate error |
| `and_then(f)` | `(T→Expected<U,E>) → Expected<U,E>` | Chain fallible operations |
| `or_else(f)` | `(E→Expected<T,E>) → Expected<T,E>` | Recover from error |
| `transform_error(f)` | `(E→E2) → Expected<T,E2>` | Transform error type |

### 3. Safe Value Access

```cpp
Expected<int, Error> result = compute();

// Pattern 1: Check and access
if (result) {
    use(*result);
}

// Pattern 2: Default value
int value = result.value_or(42);

// Pattern 3: Lazy default
int value = result.value_or_else([] { return expensive_default(); });

// Pattern 4: Inspect without extraction (fat_p extension)
result.inspect([](int v) { log("Got value:", v); });
result.inspect_error([](Error e) { log("Got error:", e); });
```

### 4. Expected<void, E> for Status-Only Operations

```cpp
Expected<void, Error> validateInput(const Input& input) {
    if (!input.valid()) return {unexpect, Error::InvalidInput};
    return {};  // Success with no value
}

auto result = validateInput(input)
    .and_then([&] { return processInput(input); })  // Returns Expected<Output, Error>
    .map([](Output o) { return formatOutput(o); }); // Returns Expected<string, Error>
```

### 5. Reference Support

```cpp
Expected<int&, Error> getRef();

auto result = getRef();
if (result) {
    *result = 42;  // Modifies referenced int
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not std::expected (C++23) | Why Not std::optional | Why Not Error Codes | Fat-P Advantage |
|----------------|------------------------------|----------------------|---------------------|-----------------|
| Storage policy | ❌ Implementation-defined | N/A | N/A | ✅ Three policies |
| `inspect()` | ❌ Not available | ❌ Not available | N/A | ✅ Peek without extract |
| `value_or_else()` | ❌ Not available | ❌ Not available | N/A | ✅ Lazy default |
| C++17 support | ❌ C++23 required | ✅ C++17 | ✅ Always | ✅ C++17 |
| Monadic ops | ✅ Has them | ❌ Limited | ❌ None | ✅ Full set |
| `Expected<void, E>` | ✅ Has it | N/A | N/A | ✅ Has it |

**The Sweet Spot:** Fat-P Expected is the only option combining storage policies, extended operations (`inspect`, `value_or_else`), C++17 support, and full monadic interface.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++23's `std::expected` is finalized. It will **never** gain:
- Storage policy customization (ABI stability)
- `inspect()` / `inspect_error()` (not proposed)
- `value_or_else()` with lazy evaluation (not proposed)

These are architectural decisions, not oversights. Fat-P Expected provides these capabilities permanently for codebases that need them.

**Compiler Lock-in:** Many HPC codebases are locked to C++17 for 5+ years due to driver compatibility. Fat-P Expected provides C++23-style error handling **today** on C++17.

---

## Performance Characteristics

| Operation | Mechanism | Cost Driver |
|-----------|-----------|-------------|
| Construction (value) | Placement new into discriminated union | Same cost as constructing T — no heap, no indirection |
| Construction (error) | Placement new into discriminated union | Same cost as constructing E |
| `has_value()` check | Boolean read | Single branch — zero overhead beyond a comparison |
| `value()` access | Direct member access | No indirection — equivalent to accessing a struct field |
| `map()` | Conditional + construction of new Expected | Function call cost + one branch + one placement new |
| `and_then()` | Conditional + construction of new Expected | Function call cost + one branch + one placement new |

See `components/Expected/results/` for current platform-specific benchmark data.

### Where Fat-P Wins
- Error-handling hot paths where exceptions are too expensive
- Monadic composition of fallible operations
- APIs that need explicit error visibility

### Where Fat-P Loses (Honesty Builds Trust)
- Simple success/failure with no error data → `std::optional` is simpler
- Truly exceptional conditions → exceptions may be clearer
- C++23 codebases → `std::expected` is standard

---

## Integration Points

```
Expected.h
    ↓ uses
TypeTraits.h           (SFINAE helpers, C++ version detection)
    ↓ used by
CheckedArithmetic.h    (ReturnExpectedPolicy)
IdGenerator.h          (Expected<Id, IdError>)
JsonLite.h             (Expected<Value, ParseError>)
```

---

## Final Assessment

Expected delivers on the fat_p promise through three pillars:

### 1. Permanence
C++23's `std::expected` will never gain storage policies or extended operations due to ABI stability. Fat-P Expected provides these permanently—and works on C++17 today.

### 2. Specialization
Storage policies let you choose between zero-overhead production builds (TrivialStorage/UnionStorage) and debug-friendly development (VariantStorage). The automatic policy selection (AutoStorage) picks optimally based on type traits.

### 3. Control
Three storage policies, extended operations (`inspect`, `value_or_else`), and full monadic interface give you control over error handling architecture. No runtime dispatch—policies resolve at compile time.

**Architectural Verdict:** Expected transforms error handling from **hidden exceptions** or **tedious error codes** to **visible, composable, type-safe** result types. The monadic interface enables railway-oriented programming where errors propagate automatically through operation chains.

---

*Expected.h — Fat-P Library*
