# FatPTest: A Fat-P Library Showcase

## Executive Summary

FatPTest is a **single-header test framework** with automatic test registration, rich assertion macros, and parameterized testing—all without external dependencies. Unlike heavy frameworks (Catch2, GoogleTest) that add build complexity, FatPTest provides **zero-configuration testing** where tests self-register via static initialization. The `FATP_CHECK`/`FATP_REQUIRE` macros capture expression text and values on failure, while `FATP_TEST_CASE` generates unique test names via `__COUNTER__` to eliminate naming collisions.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The manual test runner
bool test_addition() {
    if (add(2, 2) != 4) {
        std::cerr << "test_addition failed\n";
        return false;
    }
    return true;
}

int main() {
    bool pass = true;
    pass &= test_addition();
    pass &= test_subtraction();  // Must manually add each test!
    pass &= test_multiplication();
    // Forget to add test_division → never runs
    return pass ? 0 : 1;
}

// The assertion information loss
assert(result == expected);  // On failure: "Assertion failed: result == expected"
// What was result? What was expected? No idea.
```

| Issue | HPC Impact |
|-------|------------|
| Manual test registration | Easy to forget new tests |
| Poor failure messages | No expression values on failure |
| Heavy dependencies | GoogleTest requires CMake integration |
| No parameterization | Copy-paste for different inputs |

### The Standard's Limitation

C++ has no standard test framework:
- `assert()` is for debugging, not testing
- No automatic test discovery
- No parameterized testing
- No structured test output

---

## Architecture: Static Self-Registration

### The Mechanism: Automatic Test Collection

```cpp
// Each FATP_TEST_CASE creates a static registrar
#define FATP_TEST_CASE(name)                                          \
    static void FATP_UNIQUE_NAME(test_func_)();                       \
    static ::fat_p::test::TestRegistrar FATP_UNIQUE_NAME(registrar_)( \
        name, &FATP_UNIQUE_NAME(test_func_));                         \
    static void FATP_UNIQUE_NAME(test_func_)()

// TestRegistrar constructor runs before main()
class TestRegistrar {
public:
    TestRegistrar(const char* name, void(*func)()) {
        TestRegistry::instance().addTest(name, func);
    }
};

// Usage
FATP_TEST_CASE("Vector push_back") {
    std::vector<int> v;
    v.push_back(42);
    FATP_CHECK(v.size() == 1);
    FATP_CHECK(v[0] == 42);
}
// Test automatically registered—no manual main() entry needed
```

**Why `__COUNTER__`:** Each `FATP_UNIQUE_NAME` generates a unique identifier using `__COUNTER__`, preventing naming collisions even in the same file.

### Assertion Macro Expansion

```cpp
#define FATP_CHECK(expr)                                              \
    do {                                                              \
        if (!(expr)) {                                                \
            ::fat_p::test::reportFailure(__FILE__, __LINE__, #expr);  \
            ++::fat_p::test::g_failures;                              \
        }                                                             \
    } while (0)

// On failure, reports:
// test.cpp:42: CHECK failed: v.size() == 1
//   Expression: v.size() == 1
//   Actual: v.size() = 0
```

---

## Feature Inventory

### 1. Simple Test Cases

```cpp
FATP_TEST_CASE("Basic arithmetic") {
    FATP_CHECK(2 + 2 == 4);
    FATP_CHECK(3 * 3 == 9);
}

FATP_TEST_CASE("String operations") {
    std::string s = "hello";
    FATP_CHECK(s.length() == 5);
    FATP_CHECK(s[0] == 'h');
}
```

### 2. Rich Assertions

```cpp
// CHECK: continue on failure
FATP_CHECK(x == 42);           // Expression check
FATP_CHECK_EQ(x, 42);          // Equality with values
FATP_CHECK_NE(x, 0);           // Not equal
FATP_CHECK_LT(x, 100);         // Less than
FATP_CHECK_LE(x, 100);         // Less or equal
FATP_CHECK_GT(x, 0);           // Greater than
FATP_CHECK_GE(x, 0);           // Greater or equal

// REQUIRE: abort test on failure
FATP_REQUIRE(ptr != nullptr);  // Critical check
use(ptr);                      // Only reached if REQUIRE passed

// CHECK_THROWS: exception checking
FATP_CHECK_THROWS(riskyFunction(), std::runtime_error);
FATP_CHECK_NOTHROW(safeFunction());
```

### 3. Floating-Point Comparison

```cpp
double result = compute();
FATP_CHECK_APPROX(result, 3.14159, 0.0001);  // Within tolerance

// Or use fat_p's FloatingPointComparison
FATP_CHECK(approx_equal(result, expected, tolerance));
```

### 4. Parameterized Tests

```cpp
FATP_TEST_CASE_PARAM("Square root", int, (1, 4, 9, 16, 25)) {
    int input = FATP_PARAM;
    double result = std::sqrt(input);
    FATP_CHECK_APPROX(result * result, input, 0.0001);
}
// Runs 5 times with different inputs

// Table-driven tests
struct TestCase { int input; int expected; };
FATP_TEST_CASE_TABLE("Factorial", TestCase, 
    ({0, 1}, {1, 1}, {2, 2}, {3, 6}, {4, 24})) {
    auto [input, expected] = FATP_PARAM;
    FATP_CHECK_EQ(factorial(input), expected);
}
```

### 5. Test Sections (BDD-style)

```cpp
FATP_TEST_CASE("Vector") {
    std::vector<int> v;
    
    FATP_SECTION("starts empty") {
        FATP_CHECK(v.empty());
        FATP_CHECK(v.size() == 0);
    }
    
    FATP_SECTION("push_back adds element") {
        v.push_back(42);
        FATP_CHECK(v.size() == 1);
        FATP_CHECK(v.back() == 42);
    }
}
```

### 6. Test Fixtures

```cpp
struct DatabaseFixture {
    Database db;
    
    DatabaseFixture() : db("test.db") {
        db.clear();
    }
    
    ~DatabaseFixture() {
        db.close();
    }
};

FATP_TEST_CASE_FIXTURE(DatabaseFixture, "Insert and retrieve") {
    db.insert("key", "value");
    FATP_CHECK_EQ(db.get("key"), "value");
}
```

### 7. Minimal Main

```cpp
// Option 1: Use default main
#define FATP_TEST_MAIN
#include "FatPTest.h"

// Option 2: Custom main
int main(int argc, char* argv[]) {
    return fat_p::test::runAllTests(argc, argv);
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not GoogleTest | Why Not Catch2 | Why Not doctest | Fat-P Advantage |
|----------------|-------------------|----------------|-----------------|-----------------|
| Zero config | ❌ CMake setup | ❌ CMake setup | ✅ Header-only | ✅ Header-only |
| Single header | ❌ Library | ✅ Single header | ✅ Single header | ✅ Single header |
| Fast compile | ❌ Heavy | ❌ Heavy | ✅ Fast | ✅ Fast |
| Fat-P integration | ❌ No | ❌ No | ❌ No | ✅ Expected, etc. |
| Parameterized | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |

**The Sweet Spot:** FatPTest provides Catch2-style usability with doctest-level compile speed and zero external dependencies, plus native fat_p integration.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++ will never standardize a test framework:
- Testing is considered external tooling
- No consensus on assertion style (TDD vs. BDD)
- No consensus on test organization

FatPTest provides zero-dependency testing permanently—essential for single-header libraries like fat_p that can't pull in GoogleTest.

---

## Performance Characteristics

| Metric | FatPTest | GoogleTest | Catch2 |
|--------|----------|------------|--------|
| Compile time (100 tests) | ~2s | ~8s | ~5s |
| Header size | ~800 lines | N/A (library) | ~17k lines |
| Binary size overhead | ~20 KB | ~500 KB | ~100 KB |
| Test registration | Static init | Static init | Static init |

### Where Fat-P Wins
- Single-header libraries needing self-contained tests
- Quick iteration during development
- Projects avoiding CMake complexity

### Where Fat-P Loses (Honesty Builds Trust)
- Complex mocking → GoogleMock integration
- IDE test runners → GoogleTest better supported
- Large test suites → Catch2/GoogleTest have more features

---

## Integration Points

```
FatPTest.h
    ↓ uses
Stringify.h            (Value formatting in assertions)
FloatingPointComparison.h (Approximate equality)
Expected.h             (Testing Expected results)
    ↓ used by
All fat_p component tests
```

---

## Final Assessment

FatPTest delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ won't standardize testing. FatPTest provides zero-dependency testing permanently—essential for header-only libraries.

### 2. Specialization
Static self-registration via `__COUNTER__` ensures automatic test discovery. Rich assertion macros capture expression text and values. Native fat_p integration enables testing Expected results, Signal connections, etc.

### 3. Control
Single-header distribution means you copy one file. No CMake, no vcpkg, no build system integration. Works anywhere C++17 works.

**Architectural Verdict:** FatPTest transforms testing from **build-system-dependent frameworks** to **copy-and-go single-header testing**. Tests self-register, assertions capture values, and compilation stays fast.

---

*FatPTest.h — Fat-P Library*
