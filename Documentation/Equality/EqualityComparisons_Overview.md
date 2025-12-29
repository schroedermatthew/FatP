# EqualityComparisons: Recursive Tolerance Comparison for Nested Structures

*Updated December 2025*

## Executive Summary

EqualityComparisons provides **recursive epsilon propagation** through arbitrarily nested containers, pairs, and tuples — with a single function call. Unlike hand-written comparison loops or test-framework macros limited to scalars, it traverses any structure depth automatically, applying policy-based floating-point tolerance at every leaf. This eliminates the boilerplate, bugs, and maintenance burden of manual comparison code while remaining **dependency-free** and suitable for production use beyond testing.

---

## The Problem

### The Boilerplate Tax

Every nested structure requires its own comparison function:

```cpp
// Compare map<string, vector<double>> with tolerance
bool compareResults(const std::map<std::string, std::vector<double>>& a,
                    const std::map<std::string, std::vector<double>>& b,
                    double eps) {
    if (a.size() != b.size()) return false;
    for (const auto& [key, vec_a] : a) {
        auto it = b.find(key);
        if (it == b.end()) return false;
        const auto& vec_b = it->second;
        if (vec_a.size() != vec_b.size()) return false;
        for (size_t i = 0; i < vec_a.size(); ++i) {
            if (std::fabs(vec_a[i] - vec_b[i]) > eps) return false;
        }
    }
    return true;
}
```

Now write it again for `map<string, map<int, vector<double>>>`. And again for `vector<tuple<double, double, double>>`. And again for every structure in your codebase.

| Problem | HPC Impact |
|---------|------------|
| Boilerplate explosion | Each nested type needs custom comparison code |
| Subtle bugs | NaN handling, infinity, signed zero, size mismatches, key ordering |
| Maintenance burden | Structure changes require updating comparison functions |
| Copy-paste errors | Tolerance applied inconsistently across functions |
| Test framework coupling | `EXPECT_NEAR` only works in tests, not production validation |

### What Alternatives Don't Solve

| Approach | Limitation |
|----------|------------|
| **Google Test EXPECT_NEAR** | Scalar only, test-time only, requires framework dependency |
| **Catch2 Approx** | Scalar only, no container support |
| **std::equal with predicate** | No recursion into nested structures, no epsilon propagation |
| **Write it yourself** | The boilerplate explosion above |
| **Stringify and diff** | Loses floating-point precision, slow, brittle |

---

## The Solution

```cpp
// Any nesting depth. Any container type. One call.
bool match = areEqual(a, b, epsilon);
```

EqualityComparisons uses **compile-time type dispatch** to recursively traverse:

- **Sequences**: `vector`, `array`, `deque`, `list`, `forward_list`
- **Associative**: `map`, `set`, `multimap`, `multiset`
- **Unordered**: `unordered_map`, `unordered_set`, `unordered_multimap`, `unordered_multiset`
- **Structural**: `pair`, `tuple`
- **Nested combinations**: Any depth of the above

At each leaf, floating-point values are compared using a **policy-based tolerance strategy** — no virtual dispatch, resolved entirely at compile time.

```cpp
// 4 levels deep: vector<map<string, vector<pair<int, double>>>>
std::vector<std::map<std::string, std::vector<std::pair<int, double>>>> expected, actual;
// ... populate ...

bool match = areEqual(expected, actual, 1e-9);  // Epsilon propagates to every double
```

---

## Architecture: Compile-Time Type Dispatch

```cpp
template <typename T, typename Policy = StandardComparisonPolicy, typename... EpsParams>
bool areEqual(const T& a, const T& b, EpsParams... eps);
```

**The Dispatch Mechanism:**

The `EqualDispatcher` template uses `if constexpr` to select the appropriate comparison path at compile time:

1. **Floating-point?** → Apply `Policy::epsilonMatch`
2. **Pair?** → Recurse into `first` and `second`
3. **Tuple?** → Recurse into each element via index sequence
4. **Iterable?** → Compare sizes, then element-by-element
5. **Unordered associative?** → Key-based lookup with multiplicity tracking
6. **Otherwise?** → Use `operator==`

No runtime type checks. No virtual dispatch. No RTTI for dispatch — selection is compile-time via traits and `if constexpr`. (Diagnostics may use `typeid` as a fallback when no `toString()` exists.)

---

## Feature Inventory

### 1. Recursive Container Comparison

Epsilon propagates through any nesting depth:

```cpp
std::vector<std::vector<double>> grid1 = {{1.0, 2.0}, {3.0, 4.0}};
std::vector<std::vector<double>> grid2 = {{1.0 + 1e-10, 2.0}, {3.0, 4.0 - 1e-10}};

areEqual(grid1, grid2, 1e-9);  // true: tolerance applied at every leaf
```

### 2. Policy-Based Floating-Point Comparison

Four policies, selected at compile time:

| Policy | Strategy | Use Case |
|--------|----------|----------|
| `StandardComparisonPolicy` | Absolute tolerance with noise floor | General purpose (default) |
| `UlpComparisonPolicy` | Units in Last Place | Numerical algorithm verification |
| `RelativeComparisonPolicy` | Scale-independent relative error | Multi-scale data |
| `HybridComparisonPolicy` | Relative + absolute floor | Robust across all magnitudes |

```cpp
// ULP comparison: values within 4 representable floats
areEqual<float, UlpComparisonPolicy>(a, b, 4.0f);

// Hybrid: relative tolerance with absolute floor near zero
areEqual<double, HybridComparisonPolicy>(a, b, 1e-6, 1e-12);
```

### 3. Unordered Container Support

Order-independent comparison with multiplicity validation:

```cpp
std::unordered_multiset<int> a = {1, 1, 2, 3};
std::unordered_multiset<int> b = {3, 1, 2, 1};  // Same elements, different order

areEqual(a, b);  // true: multiplicity matches

std::unordered_multiset<int> c = {1, 2, 2, 3};  // Different multiplicities
areEqual(a, c);  // false: {1,1,2,3} vs {1,2,2,3}
```

**Key semantics:** Keys are matched using the container's `key_equal` (exact match for floating-point keys). Epsilon tolerance applies only to mapped values after key correspondence is established — not to approximate key lookup.

### 4. Floating-Point Edge Case Handling

IEEE 754 semantics preserved:

```cpp
double nan = std::numeric_limits<double>::quiet_NaN();
double inf = std::numeric_limits<double>::infinity();

areEqual(nan, nan);    // false (NaN != NaN per IEEE 754)
areEqual(inf, inf);    // true (same-sign infinities equal)
areEqual(0.0, -0.0);   // true (signed zeros equal)
```

### 5. Non-Sized Iterable Support

Containers without `.size()` (e.g., `forward_list`) are compared via iterator exhaustion:

```cpp
std::forward_list<double> a = {1.0, 2.0, 3.0};
std::forward_list<double> b = {1.0, 2.0, 3.0, 4.0};

areEqual(a, b);  // false: different lengths detected via iteration
```

### 6. Explicit Epsilon Override

Default tolerance is `100 × std::numeric_limits<T>::epsilon()`. Override per-call:

```cpp
areEqual(a, b);        // Default tolerance (~2.2e-14 for double)
areEqual(a, b, 1e-6);  // Explicit absolute tolerance
areEqual(a, b, 0.0);   // Exact match only
```

---

## Not Just for Testing

EqualityComparisons is designed for **production use**, not just test assertions:

| Use Case | Example |
|----------|---------|
| **Result validation** | Verify numerical solver output against reference |
| **Checkpoint comparison** | Detect drift in simulation state across restarts |
| **Data diffing** | Compare datasets for equivalence within tolerance |
| **Regression detection** | Validate output hasn't changed beyond acceptable error |
| **Cross-platform verification** | Confirm results match across architectures |

```cpp
// Production validation layer
if (!areEqual(computed_result, expected_baseline, tolerance)) {
    log_error("Result divergence detected");
    return Status::ValidationFailed;
}
```

No test framework required. No conditional compilation. Same code in tests and production.

---

## Why Not Alternatives?

| If You Need... | Why Not Test Frameworks | Why Not Hand-Written | Fat-P Advantage |
|----------------|------------------------|---------------------|-----------------|
| Production use | Test macros don't compile in production | Error-prone, tedious | Single header, no dependencies |
| Nested containers | Scalar comparison only | Boilerplate explosion | Recursive dispatch |
| Policy selection | Fixed tolerance strategy | Rewrite for each policy | Compile-time policy resolution |
| Unordered containers | No multiplicity validation | Complex to get right | Built-in multiplicity tracking |
| Dependency-free | Pulls in test framework | N/A | Header-only, STL only |

---

## The Dependency Reality

Many codebases cannot adopt external test frameworks:

- **Embedded systems**: Binary size constraints prohibit large dependencies
- **HPC clusters**: Approved software lists don't include Google Test
- **Proprietary codebases**: Legal review required for each dependency
- **Build system constraints**: Header-only is the only viable option

EqualityComparisons is header-only, using only the standard library plus Fat-P core headers (`FloatingPointComparison.h`, `Stringify.h`, `DiagnosticLogger_Core.h`). No external libraries or test frameworks required.

---

## Performance Characteristics

| Aspect | Mechanism |
|--------|-----------|
| **Zero overhead dispatch** | `if constexpr` resolves at compile time |
| **No virtual calls** | Policy is a template parameter |
| **Compile-time type selection** | Traits-based dispatch, no runtime branching |
| **Short-circuit evaluation** | Early exit on first mismatch (configurable) |

### Benchmark Context

Typical performance for a 10,000-element `vector<double>` (varies by hardware/compiler):

| Operation | Relative Cost |
|-----------|---------------|
| `areEqual` with epsilon | Baseline |
| Manual epsilon loop | ~0.5-0.8× (tighter loop, may auto-vectorize) |
| `std::equal` (exact) | ~0.1-0.2× (no epsilon math) |

The gap between `areEqual` and a manual loop is generic dispatch overhead. The gap between epsilon comparison and `std::equal` is the inherent cost of floating-point tolerance math (`fabs`, `fmax`, comparisons). This is fundamental to the problem, not library overhead.

---

## Integration Points

```
EqualityComparisons.h
    ├── uses
    │   ├── FloatingPointComparison.h (policy implementations, default tolerances)
    │   ├── Stringify.h (diagnostic formatting)
    │   └── DiagnosticLogger_Core.h (mismatch logging)
    │
    └── used by
        ├── EqualityAny.h (type-erased comparison)
        ├── Test suites (assertion helpers)
        └── Validation layers (production checks)
```

---

## When to Use EqualityComparisons

**Use EqualityComparisons when:**
- Comparing nested structures containing floating-point values
- Tolerance must propagate through arbitrary container depth
- Hand-written comparison code is becoming a maintenance burden
- Production validation requires the same logic as tests
- External dependencies are prohibited or impractical

**Use direct comparison when:**
- Exact equality is required (integer data, strings)
- Single scalar comparison with no nesting
- Performance-critical inner loop where manual optimization matters

---

## Final Assessment

EqualityComparisons delivers on the fat_p promise:

1. **Permanence:** This isn't waiting for a standard library feature. Recursive epsilon propagation through arbitrary structures isn't coming to `std::equal` or test frameworks.

2. **Specialization:** Designed for HPC workloads where floating-point tolerance, nested containers, and production validation are daily requirements.

3. **Control:** Policy-based comparison resolved at compile time — no runtime overhead for flexibility you don't use.

**One function replaces hundreds of lines of hand-written, bug-prone comparison code.**

---

*EqualityComparisons.h — Fat-P Library*
*See EqualityAny_Overview.md for type-erased comparison of std::any values*
