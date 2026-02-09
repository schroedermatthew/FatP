# Fat-P Test Suite Style Guide

## Purpose

This guide ensures consistent, thorough test suites across all Fat-P components. Tests are the **executable specification** of the component -- they document behavior, catch regressions, and prove correctness.

The canonical reference implementation is `test_StableHashMap.cpp`.

---

## File Structure

### Implementation File (`test_Component.cpp`)

All tests use a **named nested namespace** with `FATP_TEST_CASE` macro:

```cpp
/**
 * @file test_Component.cpp
 * @brief Comprehensive unit tests for Component.h
 */

#include <iostream>
#include <string>
#include <vector>

#include "Component.h"
#include "FatPTest.h"

namespace fat_p::testing::componentns
{

// ============================================================================
// Helper Types (specific to this component's tests)
// ============================================================================

// Define as needed for this component

// ============================================================================
// Tests
// ============================================================================

FATP_TEST_CASE(basic_operations)
{
    Component<int> c;
    FATP_ASSERT_TRUE(c.empty(), "Should start empty");
    FATP_ASSERT_EQ(c.size(), size_t(0), "Size should be 0");
    return true;
}

FATP_TEST_CASE(insert)
{
    Component<int> c;
    c.insert(42);
    FATP_ASSERT_EQ(c.size(), size_t(1), "Size should be 1");
    return true;
}

} // namespace fat_p::testing::componentns

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_Component()
{
    FATP_PRINT_HEADER(COMPONENT NAME)
    
    TestRunner runner;
    
    FATP_RUN_TEST_NS(runner, componentns, basic_operations);
    FATP_RUN_TEST_NS(runner, componentns, insert);
    // ...
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Component() ? 0 : 1;
}
#endif
```

### Key Requirements

| Element | Requirement |
|---------|-------------|
| Namespace | `fat_p::testing::componentns` (nested, not anonymous) |
| Test definition | `FATP_TEST_CASE(name)` macro |
| Test execution | `FATP_RUN_TEST_NS(runner, componentns, name)` macro |
| Return value | Every test returns `bool` (`return true;` on success) |

---

## Special-Purpose Test Files

Some test files serve architectural roles that exempt them from standard patterns. These files must still follow general Fat-P coding standards but use alternative patterns appropriate to their purpose.

### Test Orchestrators

**Example:** `test_FatP.cpp`

Test orchestrators aggregate and run multiple test suites. They do not define individual tests.

**Characteristics:**
- Calls `test_ComponentName()` functions from other test files
- Uses a simple aggregation pattern (e.g., `RUN_AND_RECORD` macro)
- Reports overall pass/fail across all suites
- Does NOT use `FATP_TEST_CASE` or `FATP_RUN_TEST_NS` (nothing to define/run)

**Required:**
- File header documentation (`@file`, `@brief`)
- `ENABLE_TEST_APPLICATION` guarded `main()`
- Clear pass/fail summary output

**Example pattern:**
```cpp
#define RUN_AND_RECORD(test_func) results.push_back({#test_func, test_func()})

int main()
{
    std::vector<TestResult> results;
    
    RUN_AND_RECORD(test_ComponentA);
    RUN_AND_RECORD(test_ComponentB);
    RUN_AND_RECORD(test_ComponentC);
    
    // Print summary and return appropriate exit code
}
```

### Test Framework Self-Tests

**Example:** `test_FatPTest.cpp`

Tests for the test framework itself cannot use the framework being tested -- this would create circular dependencies and mask framework bugs.

**Characteristics:**
- Tests `FatPTest.h` functionality
- Uses independent verification (custom `VERIFY` macro)
- Maintains its own pass/fail counters
- Still uses nested namespace (`fat_p::testing::fatptest`)

**Required:**
- File header documentation (`@file`, `@brief`)
- Nested namespace for test helpers
- `ENABLE_TEST_APPLICATION` guarded `main()`
- Independent assertion mechanism (not `FATP_ASSERT_*`)

**Example pattern:**
```cpp
namespace fat_p::testing::fatptest
{

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define VERIFY(condition, message) \
    do { \
        ++g_tests_run; \
        if (!(condition)) { \
            std::cerr << "FAILED: " << message << std::endl; \
        } else { \
            ++g_tests_passed; \
        } \
    } while(0)

void test_assert_macros()
{
    // Test FATP_ASSERT_EQ behavior
    VERIFY(/* condition */, "FATP_ASSERT_EQ works correctly");
}

} // namespace fat_p::testing::fatptest
```

### Header Include Hygiene Tests

**Example:** `test_StateMachine_HeaderSelfContained.cpp`

Header include hygiene tests are **compile-only** translation units that enforce the systemic hygiene
requirement that every public header is self-contained and include-order independent. A header must
compile when included alone in an otherwise empty translation unit (no "transitive include" luck). 

**File naming:**
- For a public header `X.h`, create a test file named `test_X_HeaderSelfContained.cpp`.

**Hard requirements:**
- The **first include** in the file is the target header: `#include "X.h"`.
- Do **not** include any other Fat-P headers (including `FatPTest.h`).
- Optional but recommended: include the header **twice** to validate idempotence.
- Provide a minimal `int main()`; this file exists to compile, not to run behavior tests.
- No `using namespace` at global scope.
- Do not add "convenience includes" (the whole point is to fail if `X.h` is missing includes).

**What this catches:**
- Missing standard includes inside `X.h` that only compile because another test happened to include them first.
- Hidden include-order dependencies (header A must be included before header B).
- Header guard / `#pragma once` failures (when the header is included twice).

**Example pattern:**
```cpp
/**
 * @file test_X_HeaderSelfContained.cpp
 * @brief Compile-only header self-contained test for X.h.
 */

#include "X.h"
#include "X.h"  // Optional: validate idempotence

int main()
{
    return 0;
}
```

**How to produce the test (per header):**
1. Identify the public header under test: `fat_p/X.h`.
2. Create `tests/test_X_HeaderSelfContained.cpp` using the above pattern.
3. Add it as a build target that CI compiles (execution is optional; compilation is the gate).
4. Treat failures as P0 hygiene regressions: fix the header (missing includes / collisions), not the include test.

### Compile-Fail Contract Test Suite

**Example:** `tests/compile_fail/compile_fail_StateMachine_BadInitialIndex.cpp`

Compile-fail tests are **negative** translation units that are expected to **fail compilation**.
They validate that compile-time contracts are actually enforced (via `static_assert`, concepts,
`requires`, and deleted overloads), and that the diagnostic surface stays intentional.

These tests are a core correctness tool for template-heavy components: if an invalid configuration
accidentally starts compiling, that is usually a contract regression.

**File location and naming:**
- Place compile-fail translation units under `tests/compile_fail/`.
- File name format: `compile_fail_<Component>_<Reason>.cpp`.
- Keep each file focused: **one primary failure mode per TU**.

**Hard requirements:**
- The TU must fail because of the component's own contract checks (preferred: `static_assert` /
  concept failure), not because of unrelated syntax errors, missing includes, or `#error`.
- Include the header(s) under test directly (e.g., `#include "X.h"`). Do not rely on other test
  files or transitive include luck.
- Force instantiation of the invalid type or expression so the failure triggers deterministically
  (e.g., a `using Bad = ...;` followed by `static_assert(sizeof(Bad) > 0);`).
- No `using namespace` at global scope.
- Std-only: do not add third-party frameworks.
- Include a Doxygen file header (`@file`, `@brief`) describing the intended failure and the
  contract being verified.

**Example pattern:**
```cpp
/**
 * @file compile_fail_Component_BadInitialIndex.cpp
 * @brief Expected-fail: Bad configuration must trigger a clear static_assert.
 */

#include "Component.h"

namespace
{
// Arrange an intentionally-invalid instantiation.
using Bad = /* Component<BadParam...> */;

// Force instantiation so the compile failure reliably triggers.
static_assert(sizeof(Bad) > 0, "Force instantiation");
} // namespace
```

**How to produce the test (per contract):**
1. Identify a compile-time contract you claim to enforce (e.g., unique types, valid indices,
   policy requirements, required hooks/members, valid type lists).
2. Write a TU that violates exactly that contract.
3. Ensure the failure originates from the component's contract check and has an intentional
   diagnostic message.
4. Wire it into CI as an **expected-fail compile** job:
   - Compile with `-c` and assert non-zero exit status, or
   - Use a CMake/CTest script that runs the compiler and expects failure.
5. Treat regressions as P0: if a compile-fail starts compiling, either the contract enforcement
   broke or the test no longer forces instantiation.

**Recommended coverage (when applicable):**
- Duplicate/invalid template parameters (e.g., repeated types).
- Out-of-range indices or sizes.
- Missing required member functions / hooks.
- Policy mismatches (e.g., a `NoExcept` policy with non-`noexcept` hooks).
- Invalid type-list contents (e.g., a transition list referencing an unknown type).

### Summary of Exemptions

| File Type | Uses FATP_TEST_CASE | Uses FATP_RUN_TEST_NS | Uses FATP_ASSERT_* |
|-----------|:------------------:|:--------------------:|:-----------------:|
| Standard test | ✅ Required | ✅ Required | ✅ Required |
| Test orchestrator | ❌ N/A | ❌ N/A | ❌ N/A |
| Framework self-test | ❌ Prohibited | ❌ Prohibited | ❌ Prohibited |
| Header include hygiene test (`test_X_HeaderSelfContained.cpp`) | ❌ N/A | ❌ N/A | ❌ N/A |
| Compile-fail contract test (`tests/compile_fail/*.cpp`) | ❌ N/A | ❌ N/A | ❌ N/A |

---

## Test Categories

Test suites should cover these areas (as applicable to the component):

| Category | What to Test |
|----------|--------------|
| **Basic operations** | Construction, primary methods, destruction |
| **Edge cases** | Empty, single element, max size, boundaries |
| **Semantic behavior** | Documented contracts, return values |
| **Copy/move semantics** | Copy ctor/assign, move ctor/assign, self-assignment |
| **Exception safety** | Throwing types, strong/basic guarantee |
| **RAII correctness** | Resource cleanup, no leaks |
| **Header hygiene** | Compile-only `test_X_HeaderSelfContained.cpp` for each public header (self-contained, include-order independent) |
| **Compile-fail contract** | Expected-fail translation units proving invalid configurations are rejected at compile time |
| **Stress/fuzz** | Random operations, reference oracle comparison |

**Note:** Performance benchmarks belong in separate benchmark files under `components/<Component>/benchmarks/`, not in unit tests. Unit tests focus on correctness verification only.

### Basic Operations

```cpp
FATP_TEST_CASE(basic_insert_get)
{
    SlotMap<Entity> map;
    
    FATP_ASSERT_TRUE(map.empty(), "Map should start empty");
    FATP_ASSERT_EQ(map.size(), size_t(0), "Map should have size 0");
    
    auto handle = map.insert(Entity{1, "Alice", 100.0f});
    
    FATP_ASSERT_FALSE(map.empty(), "Map should not be empty");
    FATP_ASSERT_EQ(map.size(), size_t(1), "Map should have size 1");
    
    Entity* entity = map.get(handle);
    FATP_ASSERT_NOT_NULLPTR(entity, "Should get valid pointer");
    FATP_ASSERT_EQ(entity->id, 1, "ID should match");
    
    return true;
}
```

### Edge Cases

```cpp
FATP_TEST_CASE(empty_operations)
{
    FlatMap<int, std::string> map;
    
    FATP_ASSERT_TRUE(map.find(42) == map.end(), "Find on empty returns end");
    FATP_ASSERT_EQ(map.erase(42), size_t(0), "Erase on empty returns 0");
    
    map.clear();  // Clear empty container should be safe
    FATP_ASSERT_TRUE(map.empty(), "Still empty after clear");
    
    return true;
}
```

### Exception Safety

```cpp
FATP_TEST_CASE(exception_safety_insert)
{
    SmallVector<ThrowOnCopy, 4> v;
    v.push_back(ThrowOnCopy(1));
    v.push_back(ThrowOnCopy(2));
    
    ThrowOnCopy::reset();
    ThrowOnCopy::throw_after = 1;
    
    size_t old_size = v.size();
    bool threw = false;
    
    try
    {
        v.push_back(ThrowOnCopy(3));
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    
    FATP_ASSERT_TRUE(threw, "Should have thrown");
    FATP_ASSERT_EQ(v.size(), old_size, "Size unchanged (strong guarantee)");
    
    return true;
}
```

### Fuzz Testing

```cpp
FATP_TEST_CASE(stress_random)
{
    Container<int, int> container;
    std::map<int, int> reference;
    
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<int> key_dist(0, 999);
    std::uniform_int_distribution<int> op_dist(0, 2);
    
    for (int i = 0; i < 5000; ++i)
    {
        int key = key_dist(rng);
        int op = op_dist(rng);
        
        if (op == 0)
        {
            container.insert({key, i});
            reference.insert({key, i});
        }
        else if (op == 1)
        {
            bool ours = container.find(key) != container.end();
            bool theirs = reference.find(key) != reference.end();
            FATP_ASSERT_EQ(ours, theirs, "Find results should match");
        }
        else
        {
            size_t ours = container.erase(key);
            size_t theirs = reference.erase(key);
            FATP_ASSERT_EQ(ours, theirs, "Erase results should match");
        }
    }
    
    FATP_ASSERT_EQ(container.size(), reference.size(), "Final size should match");
    return true;
}
```

---

## Assertion Macros

Use FatPTest.h assertions consistently. **Choose the macro that makes the test's intention most clear:**

| Macro | When to Use |
|-------|-------------|
| `FATP_ASSERT_TRUE(cond, msg)` | Boolean conditions expected to be true |
| `FATP_ASSERT_FALSE(cond, msg)` | Boolean conditions expected to be false |
| `FATP_ASSERT_EQ(a, b, msg)` | Value equality — produces better diagnostics than `FATP_ASSERT_TRUE(a == b)` |
| `FATP_ASSERT_NE(a, b, msg)` | Value inequality |
| `FATP_ASSERT_LT(a, b, msg)` | Less than comparison |
| `FATP_ASSERT_LE(a, b, msg)` | Less than or equal |
| `FATP_ASSERT_GT(a, b, msg)` | Greater than comparison |
| `FATP_ASSERT_GE(a, b, msg)` | Greater than or equal |
| `FATP_ASSERT_CLOSE(a, b, msg)` | Floating-point with default tolerance |
| `FATP_ASSERT_CLOSE_EPS(a, b, eps, msg)` | Floating-point with custom tolerance |
| `FATP_ASSERT_NULLPTR(ptr, msg)` | Pointer should be null |
| `FATP_ASSERT_NOT_NULLPTR(ptr, msg)` | Pointer should not be null |
| `FATP_ASSERT_THROWS(expr, type, msg)` | Expression should throw specific exception |
| `FATP_ASSERT_NO_THROW(expr, msg)` | Expression should not throw |
| `FATP_ASSERT_CONTAINS(str, sub, msg)` | String contains substring |
| `FATP_ASSERT_STARTS_WITH(str, pre, msg)` | String starts with prefix |
| `FATP_ASSERT_ENDS_WITH(str, suf, msg)` | String ends with suffix |
| `FATP_SIMPLE_ASSERT(cond, msg)` | Legacy alias for `FATP_ASSERT_TRUE` |

### Principle: Intention Over Mechanism

The assertion macro name should communicate **what** you're testing:

```cpp
// Good: The macro name describes the check
FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3 after 3 inserts");
FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");
FATP_ASSERT_CLOSE(result, expected, "Computed value should match");
FATP_ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");

// Less clear: Generic boolean hides the actual check
FATP_ASSERT_TRUE(map.size() == 3, "Size should be 3");
FATP_ASSERT_TRUE(ptr != nullptr, "Allocation should succeed");
```

### Better Diagnostics

`FATP_ASSERT_EQ` produces richer failure output than `FATP_ASSERT_TRUE`:

```
// FATP_ASSERT_TRUE failure:
FATP_ASSERT_TRUE FAILED: Size should be 3
  at test_Component.cpp:42

// FATP_ASSERT_EQ failure:
FATP_ASSERT_EQ FAILED: Size should be 3
  Expected: 3
  Actual:   2
  at test_Component.cpp:42
```

---

## Helper Types

Each test suite defines its own helper types as needed. These are **examples** from existing tests, not a required catalog:

### Lifecycle Tracking

Counts constructor/destructor calls to verify RAII correctness:

```cpp
class LifecycleTracker
{
public:
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> destruct_count{0};
    
    int value;
    
    explicit LifecycleTracker(int v = 0) : value(v) { ++construct_count; }
    ~LifecycleTracker() { ++destruct_count; }
    
    static void reset() { construct_count = destruct_count = 0; }
};
```

### Exception Testing

Types that throw on specific operations:

```cpp
struct ThrowOnCopy
{
    int value;
    static int throw_after;
    static int operation_count;
    
    ThrowOnCopy(const ThrowOnCopy& other) : value(other.value)
    {
        if (++operation_count >= throw_after && throw_after > 0)
            throw std::runtime_error("Copy threw");
    }
    
    static void reset() { operation_count = 0; throw_after = -1; }
};
```

### Allocator Tracking

```cpp
template<typename T>
class TrackingAllocator
{
public:
    using value_type = T;
    inline static size_t allocation_count = 0;
    
    T* allocate(size_t n)
    {
        ++allocation_count;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    // ...
};
```

### Domain Objects

Realistic test data for component-specific testing:

```cpp
struct Entity
{
    int id;
    std::string name;
    float health;
    
    Entity(int i = 0, std::string n = "", float h = 0)
        : id(i), name(std::move(n)), health(h) {}
};
```

Define whatever helpers your component needs. Keep them minimal and within the test's namespace.

---

## Test Runner Integration

### Macros

From FatPTest.h:

| Macro | Purpose |
|-------|---------|
| `FATP_TEST_CASE(name)` | Defines test function `test_##name()` |
| `FATP_RUN_TEST_NS(runner, ns, name)` | Runs `ns::test_##name()` |
| `FATP_PRINT_HEADER(SECTION)` | Prints formatted section header |

### Running Tests

```cpp
namespace fat_p::testing
{

bool test_Component()
{
    FATP_PRINT_HEADER(COMPONENT NAME)
    
    TestRunner runner;
    
    // All tests use FATP_RUN_TEST_NS with the component's namespace
    FATP_RUN_TEST_NS(runner, componentns, basic_operations);
    FATP_RUN_TEST_NS(runner, componentns, insert);
    FATP_RUN_TEST_NS(runner, componentns, erase);
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
```

### Test Grouping with Output Headers

For larger test suites, group related tests with section headers:

```cpp
bool test_StrongId()
{
    FATP_PRINT_HEADER(STRONG ID)
    
    TestRunner runner;
    auto& out = *get_test_config().output;
    
    // Basic Functionality
    out << colors::blue() << "--- Basic Functionality ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, default_constructor);
    FATP_RUN_TEST_NS(runner, strongid, explicit_constructor);
    FATP_RUN_TEST_NS(runner, strongid, type_safety);
    
    // Comparison Operators
    out << "\n" << colors::blue() << "--- Comparison Operators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, equality_comparison);
    FATP_RUN_TEST_NS(runner, strongid, less_than_comparison);
    
    return 0 == runner.print_summary();
}
```

### Standalone Execution

```cpp
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Component() ? 0 : 1;
}
#endif
```

Compile standalone: `g++ -std=c++20 -O2 -DENABLE_TEST_APPLICATION test_Component.cpp`

---

## Checklist Before Submitting

### Structure
- [ ] File has documentation header (`@file`, `@brief`)
- [ ] Implementation uses named nested namespace `fat_p::testing::componentns`
- [ ] Tests defined with `FATP_TEST_CASE(name)` macro
- [ ] Tests executed with `FATP_RUN_TEST_NS(runner, componentns, name)` macro
- [ ] Helper types defined within the component's namespace
- [ ] Public interface in separate `fat_p::testing` namespace block
- [ ] `main()` guarded by `ENABLE_TEST_APPLICATION`

### Special-Purpose Files (Exemptions)
- [ ] Test orchestrators: Uses aggregation pattern, NOT `FATP_TEST_CASE`/`FATP_RUN_TEST_NS`
- [ ] Framework self-tests: Uses independent `VERIFY` macro, NOT `FATP_ASSERT_*`
- [ ] Header include hygiene tests: Provide `test_X_HeaderSelfContained.cpp` per public header (compile-only, include `X.h` first, no other Fat-P includes)
- [ ] Compile-fail contract tests: Provide `tests/compile_fail/compile_fail_<Component>_<Reason>.cpp` for key invalid configurations (expected-fail compile in CI)

### Coverage
- [ ] Basic construction/destruction
- [ ] All public methods tested
- [ ] Edge cases (empty, single element, boundary values)
- [ ] Copy and move semantics
- [ ] Move-only types (if supported by component)
- [ ] Exception safety (if applicable)
- [ ] RAII correctness (if applicable)
- [ ] Fuzz/stress testing (for containers)
- [ ] Compile-fail contract tests (when the component enforces compile-time constraints)

### Assertions
- [ ] Every assertion has a descriptive message
- [ ] Assertion macro matches the check being performed (intention over mechanism)
- [ ] Use `FATP_ASSERT_EQ`/`FATP_ASSERT_NE` for value comparisons (better diagnostics)
- [ ] Use `FATP_ASSERT_TRUE`/`FATP_ASSERT_FALSE` for boolean conditions
- [ ] Use `FATP_ASSERT_CLOSE`/`FATP_ASSERT_CLOSE_EPS` for floating-point comparisons

### Naming
- [ ] Test names are descriptive: `basic_insert_get` not `test7`
- [ ] Namespace matches component: `slotmap`, `strongid`, `valueguard`

---

*Fat-P Test Suite Style Guide v2.3 -- February 2026*
