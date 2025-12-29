# Fat-P Test Suite Style Guide

## Purpose

This guide ensures consistent, thorough test suites across all Fat-P components. Tests are the **executable specification** of the component -- they document behavior, catch regressions, and prove correctness.

The canonical reference implementation is `test_StableHashMap.cpp`.

---

## File Structure

### Implementation File (`test_Component.cpp`)

All tests use a **named nested namespace** with `TEST_CASE` macro:

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

TEST_CASE(basic_operations)
{
    Component<int> c;
    ASSERT_TRUE(c.empty(), "Should start empty");
    ASSERT_EQ(c.size(), size_t(0), "Size should be 0");
    return true;
}

TEST_CASE(insert)
{
    Component<int> c;
    c.insert(42);
    ASSERT_EQ(c.size(), size_t(1), "Size should be 1");
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void run_benchmarks()
{
    std::cout << colors::cyan() << "Component Benchmarks:" << colors::reset() << "\n";
    // ...
}

} // namespace fat_p::testing::componentns

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_Component()
{
    PRINT_HEADER(COMPONENT NAME)
    
    TestRunner runner;
    
    RUN_TEST_NS(runner, componentns, basic_operations);
    RUN_TEST_NS(runner, componentns, insert);
    // ...
    
    componentns::run_benchmarks();
    
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
| Test definition | `TEST_CASE(name)` macro |
| Test execution | `RUN_TEST_NS(runner, componentns, name)` macro |
| Return value | Every test returns `bool` (`return true;` on success) |

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
| **Stress/fuzz** | Random operations, reference oracle comparison |
| **Performance** | Benchmarks vs std:: equivalent |

### Basic Operations

```cpp
TEST_CASE(basic_insert_get)
{
    SlotMap<Entity> map;
    
    ASSERT_TRUE(map.empty(), "Map should start empty");
    ASSERT_EQ(map.size(), size_t(0), "Map should have size 0");
    
    auto handle = map.insert(Entity{1, "Alice", 100.0f});
    
    ASSERT_FALSE(map.empty(), "Map should not be empty");
    ASSERT_EQ(map.size(), size_t(1), "Map should have size 1");
    
    Entity* entity = map.get(handle);
    ASSERT_NOT_NULLPTR(entity, "Should get valid pointer");
    ASSERT_EQ(entity->id, 1, "ID should match");
    
    return true;
}
```

### Edge Cases

```cpp
TEST_CASE(empty_operations)
{
    FlatMap<int, std::string> map;
    
    ASSERT_TRUE(map.find(42) == map.end(), "Find on empty returns end");
    ASSERT_EQ(map.erase(42), size_t(0), "Erase on empty returns 0");
    
    map.clear();  // Clear empty container should be safe
    ASSERT_TRUE(map.empty(), "Still empty after clear");
    
    return true;
}
```

### Exception Safety

```cpp
TEST_CASE(exception_safety_insert)
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
    
    ASSERT_TRUE(threw, "Should have thrown");
    ASSERT_EQ(v.size(), old_size, "Size unchanged (strong guarantee)");
    
    return true;
}
```

### Fuzz Testing

```cpp
TEST_CASE(stress_random)
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
            ASSERT_EQ(ours, theirs, "Find results should match");
        }
        else
        {
            size_t ours = container.erase(key);
            size_t theirs = reference.erase(key);
            ASSERT_EQ(ours, theirs, "Erase results should match");
        }
    }
    
    ASSERT_EQ(container.size(), reference.size(), "Final size should match");
    return true;
}
```

---

## Assertion Macros

Use FatPTest.h assertions consistently. **Choose the macro that makes the test's intention most clear:**

| Macro | When to Use |
|-------|-------------|
| `ASSERT_TRUE(cond, msg)` | Boolean conditions expected to be true |
| `ASSERT_FALSE(cond, msg)` | Boolean conditions expected to be false |
| `ASSERT_EQ(a, b, msg)` | Value equality — produces better diagnostics than `ASSERT_TRUE(a == b)` |
| `ASSERT_NE(a, b, msg)` | Value inequality |
| `ASSERT_LT(a, b, msg)` | Less than comparison |
| `ASSERT_LE(a, b, msg)` | Less than or equal |
| `ASSERT_GT(a, b, msg)` | Greater than comparison |
| `ASSERT_GE(a, b, msg)` | Greater than or equal |
| `ASSERT_CLOSE(a, b, msg)` | Floating-point with default tolerance |
| `ASSERT_CLOSE_EPS(a, b, eps, msg)` | Floating-point with custom tolerance |
| `ASSERT_NULLPTR(ptr, msg)` | Pointer should be null |
| `ASSERT_NOT_NULLPTR(ptr, msg)` | Pointer should not be null |
| `ASSERT_THROWS(expr, type, msg)` | Expression should throw specific exception |
| `ASSERT_NO_THROW(expr, msg)` | Expression should not throw |
| `ASSERT_CONTAINS(str, sub, msg)` | String contains substring |
| `ASSERT_STARTS_WITH(str, pre, msg)` | String starts with prefix |
| `ASSERT_ENDS_WITH(str, suf, msg)` | String ends with suffix |
| `SIMPLE_ASSERT(cond, msg)` | Legacy alias for `ASSERT_TRUE` |

### Principle: Intention Over Mechanism

The assertion macro name should communicate **what** you're testing:

```cpp
// Good: The macro name describes the check
ASSERT_EQ(map.size(), size_t(3), "Size should be 3 after 3 inserts");
ASSERT_TRUE(map.empty(), "Map should be empty after clear");
ASSERT_CLOSE(result, expected, "Computed value should match");
ASSERT_NOT_NULLPTR(ptr, "Allocation should succeed");

// Less clear: Generic boolean hides the actual check
ASSERT_TRUE(map.size() == 3, "Size should be 3");
ASSERT_TRUE(ptr != nullptr, "Allocation should succeed");
```

### Better Diagnostics

`ASSERT_EQ` produces richer failure output than `ASSERT_TRUE`:

```
// ASSERT_TRUE failure:
ASSERT_TRUE FAILED: Size should be 3
  at test_Component.cpp:42

// ASSERT_EQ failure:
ASSERT_EQ FAILED: Size should be 3
  Expected: 3
  Actual:   2
  at test_Component.cpp:42
```

**Every assertion needs a message** describing what went wrong:

```cpp
// Good
ASSERT_EQ(map.size(), size_t(3), "Size should be 3 after 3 inserts");

// Bad - no diagnostic value
ASSERT_EQ(map.size(), size_t(3), "");
```

---

## Benchmark Structure

### Infrastructure

Benchmarks use `measure_perf()` from FatPTest.h:

```cpp
void benchmark_component()
{
    std::cout << colors::cyan() << "Component Benchmarks:" << colors::reset() << "\n";
    
    constexpr int N = 1000;
    
    // Use volatile or DoNotOptimize to prevent optimization
    volatile int accumulator = 0;
    
    double time = measure_perf(
        [&accumulator]() {
            // Operation to measure
            accumulator += 1;
        },
        100000,  // iterations
        1000     // warmup
    );
    
    std::cout << "Operation: " << format_time(time) << "\n";
    DoNotOptimize(accumulator);
}
```

### Comparison Benchmarks

Always compare against `std::` equivalent (when one exists):

```cpp
void benchmark_find(size_t N)
{
    // Build both containers
    YourContainer<int, int> yours;
    std::map<int, int> theirs;
    
    for (int i = 0; i < N; ++i)
    {
        yours.insert({i, i * 10});
        theirs.insert({i, i * 10});
    }
    
    // Benchmark yours
    volatile int yours_sum = 0;
    double yours_time = measure_perf(
        [&yours, &yours_sum, i = 0]() mutable {
            auto it = yours.find(i % N);
            if (it != yours.end()) yours_sum += it->second;
            ++i;
        },
        100000, 1000);
    DoNotOptimize(yours_sum);
    
    // Benchmark theirs
    volatile int theirs_sum = 0;
    double theirs_time = measure_perf(
        [&theirs, &theirs_sum, i = 0]() mutable {
            auto it = theirs.find(i % N);
            if (it != theirs.end()) theirs_sum += it->second;
            ++i;
        },
        100000, 1000);
    DoNotOptimize(theirs_sum);
    
    // Report
    std::cout << "Ours: " << format_time(yours_time)
              << " | std::map: " << format_time(theirs_time) << "\n";
}
```

### Conditional Benchmarking

Skip benchmarks in debug builds:

```cpp
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    bool success = fat_p::testing::test_Component();
    
#ifndef NDEBUG
    std::cout << "\n[Debug build - skipping benchmarks]\n";
#else
    fat_p::testing::componentns::run_benchmarks();
#endif
    
    return success ? 0 : 1;
}
#endif
```

### Benchmark Categories

1. **Single operations** -- Insert, find, erase on populated container
2. **Iteration** -- Sequential access, range-for performance  
3. **Build from empty** -- Construction + N insertions
4. **Comparison** -- Your implementation vs std:: equivalent
5. **Sensitivity analysis** -- Vary size, load factor, etc.

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
| `TEST_CASE(name)` | Defines test function `test_##name()` |
| `RUN_TEST_NS(runner, ns, name)` | Runs `ns::test_##name()` |
| `PRINT_HEADER(SECTION)` | Prints formatted section header |

### Running Tests

```cpp
namespace fat_p::testing
{

bool test_Component()
{
    PRINT_HEADER(COMPONENT NAME)
    
    TestRunner runner;
    
    // All tests use RUN_TEST_NS with the component's namespace
    RUN_TEST_NS(runner, componentns, basic_operations);
    RUN_TEST_NS(runner, componentns, insert);
    RUN_TEST_NS(runner, componentns, erase);
    
    // Benchmarks run after tests
    componentns::run_benchmarks();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
```

### Test Grouping with Output Headers

For larger test suites, group related tests with section headers:

```cpp
bool test_StrongId()
{
    PRINT_HEADER(STRONG ID)
    
    TestRunner runner;
    auto& out = *get_test_config().output;
    
    // Basic Functionality
    out << colors::blue() << "--- Basic Functionality ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, strongid, default_constructor);
    RUN_TEST_NS(runner, strongid, explicit_constructor);
    RUN_TEST_NS(runner, strongid, type_safety);
    
    // Comparison Operators
    out << "\n" << colors::blue() << "--- Comparison Operators ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, strongid, equality_comparison);
    RUN_TEST_NS(runner, strongid, less_than_comparison);
    
    // Benchmarks
    strongid::run_benchmarks();
    
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

Compile standalone: `g++ -std=c++17 -O2 -DENABLE_TEST_APPLICATION test_Component.cpp`

---

## Checklist Before Submitting

### Structure
- [ ] File has documentation header (`@file`, `@brief`)
- [ ] Implementation uses named nested namespace `fat_p::testing::componentns`
- [ ] Tests defined with `TEST_CASE(name)` macro
- [ ] Tests executed with `RUN_TEST_NS(runner, componentns, name)` macro
- [ ] Helper types defined within the component's namespace
- [ ] Public interface in separate `fat_p::testing` namespace block
- [ ] `main()` guarded by `ENABLE_TEST_APPLICATION`

### Coverage
- [ ] Basic construction/destruction
- [ ] All public methods tested
- [ ] Edge cases (empty, single element, boundary values)
- [ ] Copy and move semantics
- [ ] Move-only types (if supported by component)
- [ ] Exception safety (if applicable)
- [ ] RAII correctness (if applicable)
- [ ] Fuzz/stress testing (for containers)

### Assertions
- [ ] Every assertion has a descriptive message
- [ ] Assertion macro matches the check being performed (intention over mechanism)
- [ ] Use `ASSERT_EQ`/`ASSERT_NE` for value comparisons (better diagnostics)
- [ ] Use `ASSERT_TRUE`/`ASSERT_FALSE` for boolean conditions
- [ ] Use `ASSERT_CLOSE`/`ASSERT_CLOSE_EPS` for floating-point comparisons

### Benchmarks
- [ ] Compare against std:: equivalent (when one exists)
- [ ] Use `DoNotOptimize()` to prevent optimization
- [ ] Consider conditional execution based on NDEBUG

### Naming
- [ ] Test names are descriptive: `basic_insert_get` not `test7`
- [ ] Namespace matches component: `slotmap`, `strongid`, `valueguard`

---

*Fat-P Test Suite Style Guide v2.1 -- December 2025*
