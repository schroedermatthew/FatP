# TensorFlow C++20 Wrapper - Comprehensive Test Analysis

## Executive Summary

| Category | Status | Notes |
|----------|--------|-------|
| Unit Tests | ⚠️ Basic | 24 tests, covers happy paths |
| Fuzz Testing | ❌ None | No fuzzing infrastructure |
| Stress Testing | ⚠️ Minimal | 3 threading tests, not comprehensive |
| Corner Cases | ❌ Sparse | Many edge cases untested |
| Error Paths | ⚠️ Partial | Some throws tested, many missing |
| Memory Safety | ❌ None | No ASAN/MSAN/Valgrind integration |
| Sanitizers | ❌ None | No CI sanitizer builds |

---

## Current Test Coverage (24 tests)

### Status (5 tests)
- ✅ RAII leak prevention (loop of 100)
- ✅ reset() method
- ✅ code_to_string() mapping
- ✅ operator!() 
- ✅ set() with string_view

### Tensor (10 tests)
- ✅ FromVector basic
- ✅ FromScalar basic
- ✅ FromRaw null rejection
- ✅ Zeros factory
- ✅ Allocate factory
- ✅ read/write views
- ✅ with_read/with_write
- ✅ dtype mismatch throws
- ✅ dimension mismatch throws
- ✅ dtype mapping (11 types)

### GuardedSpan (2 tests)
- ✅ at() bounds checking
- ✅ iteration

### Threading (3 tests)
- ✅ Mutex exclusion
- ✅ SharedMutex concurrent readers
- ✅ Tensor+Mutex torn read detection

### Session/Graph (3 tests)
- ✅ SessionOptions RAII
- ✅ Graph operation creation/lookup
- ✅ GetOperationOrThrow

### Compile-time (static_assert)
- ✅ LockPolicy concepts
- ✅ Guard concepts
- ✅ NoLock is empty/trivial
- ✅ Policy movability/copyability
- ✅ Type aliases

---

## CRITICAL GAPS: Missing Tests

### 1. Fuzz Testing (Priority: HIGH)
**None exists.** Needed for:

```cpp
// Shape fuzzing - malformed dimensions
fuzz_test(tensor_shape_fuzz) {
    std::vector<int64_t> dims = fuzz_input<std::vector<int64_t>>();
    // Should handle: empty, negative, huge, overflow-inducing
    try {
        auto t = FastTensor::Allocate<float>(dims);
    } catch (const std::exception&) { /* expected */ }
}

// String fuzzing - operation names, error messages
fuzz_test(operation_name_fuzz) {
    std::string name = fuzz_input<std::string>();
    // Should handle: null chars, unicode, very long, empty
    try {
        graph.NewOperation("Const", name);
    } catch (...) {}
}
```

### 2. Stress Testing (Priority: HIGH)
Current threading tests are weak:

**Missing stress tests:**
```cpp
// Rapid allocation/deallocation
stress_test(tensor_allocation_storm) {
    for (int i = 0; i < 100000; ++i) {
        auto t = FastTensor::Allocate<float>({100, 100});
        // Force immediate destruction
    }
}

// Concurrent tensor creation
stress_test(concurrent_tensor_creation) {
    std::vector<std::thread> threads;
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([]() {
            for (int j = 0; j < 1000; ++j) {
                auto t = FastTensor::FromScalar<float>(j);
            }
        });
    }
    for (auto& t : threads) t.join();
}

// Reader/writer contention with SharedMutex
stress_test(reader_writer_contention) {
    SharedTensor tensor = SharedTensor::Zeros<float>({1000});
    std::atomic<bool> stop{false};
    std::atomic<int> reads{0}, writes{0};
    
    // 10 readers, 2 writers, 5 seconds
    // Track operations per second, detect deadlocks
}

// Graph mutation under contention
stress_test(graph_contention) {
    SafeGraph graph;
    // Multiple threads adding operations simultaneously
}
```

### 3. Corner Cases (Priority: HIGH)

**Empty/Zero cases:**
```cpp
TEST(empty_tensor) {
    auto t = FastTensor::FromVector<float>({0}, {});
    REQUIRE(t.num_elements() == 0);
    REQUIRE(t.byte_size() == 0);
    // Can we read/write empty tensor?
}

TEST(empty_shape_scalar) {
    auto t = FastTensor::FromScalar<int>(42);
    REQUIRE(t.shape().empty());  // Scalar has rank 0
    REQUIRE(t.rank() == 0);
}

TEST(zero_dimension_in_shape) {
    auto t = FastTensor::Allocate<float>({10, 0, 5});
    REQUIRE(t.num_elements() == 0);
}
```

**Boundary values:**
```cpp
TEST(max_rank_tensor) {
    // TensorFlow supports up to rank 254
    std::vector<int64_t> shape(254, 1);
    auto t = FastTensor::Allocate<float>(shape);
}

TEST(large_dimension) {
    // Single dimension at size_t max / sizeof(T) boundary
    constexpr auto max_elems = std::numeric_limits<size_t>::max() / sizeof(float);
    // Should throw overflow_error before attempting allocation
}

TEST(int64_max_dimension) {
    REQUIRE_THROWS_AS(
        FastTensor::Allocate<float>({INT64_MAX}),
        std::overflow_error);
}
```

**Move semantics:**
```cpp
TEST(moved_from_tensor_is_empty) {
    auto t1 = FastTensor::FromScalar<float>(1.0f);
    auto t2 = std::move(t1);
    
    REQUIRE(t1.handle() == nullptr);  // Or however you detect empty
    REQUIRE_THROWS(t1.read<float>());  // Should throw, not crash
}

TEST(moved_from_status) {
    Status s1;
    s1.set(TF_CANCELLED, "test");
    Status s2 = std::move(s1);
    
    // What state is s1 in? Is it safe to use?
}

TEST(moved_from_graph) {
    FastGraph g1;
    FastGraph g2 = std::move(g1);
    
    REQUIRE_THROWS(g1.NewOperation("Const", "x"));
}

TEST(moved_from_session) {
    // Similar pattern
}
```

### 4. Error Path Testing (Priority: MEDIUM)

**OperationBuilder errors:**
```cpp
TEST(operation_builder_invalid_attr_type) {
    FastGraph graph;
    auto builder = graph.NewOperation("Const", "c");
    // What happens if we set wrong attr type?
    REQUIRE_THROWS(builder.SetAttrString("value", "not_a_tensor"));
}

TEST(operation_builder_duplicate_name) {
    FastGraph graph;
    graph.NewOperation("Const", "c")
        .SetAttrTensor("value", tensor.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    // Duplicate name should fail
    REQUIRE_THROWS(
        graph.NewOperation("Const", "c")
            .SetAttrTensor("value", tensor.handle())
            .SetAttrType("dtype", TF_FLOAT)
            .Finish()
    );
}

TEST(operation_builder_use_after_finish) {
    FastGraph graph;
    auto builder = graph.NewOperation("Const", "c");
    builder.SetAttrTensor("value", tensor.handle())
           .SetAttrType("dtype", TF_FLOAT);
    auto op = std::move(builder).Finish();
    
    // builder is now in moved-from state
    // Any further use should be UB or throw
}
```

**Session errors:**
```cpp
TEST(session_run_nonexistent_feed) {
    // Already tested via GetOperationOrThrow pattern
}

TEST(session_run_wrong_dtype_feed) {
    // Feed a float tensor to an int placeholder
}

TEST(session_run_wrong_shape_feed) {
    // Feed [2,3] tensor to [3,2] placeholder
}

TEST(session_closed_then_run) {
    // Call Close(), then try Run()
}
```

**Adopt/AdoptMalloc errors:**
```cpp
TEST(adopt_malloc_wrong_byte_len) {
    void* data = std::malloc(100);
    REQUIRE_THROWS_AS(
        FastTensor::AdoptMalloc<float>({10}, data, 50),  // Wrong size
        std::invalid_argument);
    std::free(data);  // Still need to free - Adopt didn't take ownership
}

TEST(adopt_null_deallocator) {
    void* data = std::malloc(40);
    REQUIRE_THROWS_AS(
        FastTensor::Adopt(TF_FLOAT, {10}, data, 40, nullptr),
        std::invalid_argument);
    std::free(data);
}

TEST(adopt_variable_length_dtype) {
    // TF_STRING has variable length
    void* data = std::malloc(100);
    REQUIRE_THROWS_AS(
        FastTensor::Adopt(TF_STRING, {10}, data, 100, 
                          [](void* d, size_t, void*) { std::free(d); }),
        std::invalid_argument);
    std::free(data);
}
```

### 5. Memory Safety Testing (Priority: HIGH)

**DataGuard leak test:**
```cpp
TEST(adopt_leak_on_allocation_failure) {
    // Force allocation failure to verify DataGuard cleans up
    std::atomic<bool> deallocator_called{false};
    
    auto dealloc = [](void* data, size_t, void* arg) {
        *static_cast<std::atomic<bool>*>(arg) = true;
        std::free(data);
    };
    
    void* data = std::malloc(40);
    
    // Simulate TF_NewTensor failure - need mock or huge allocation
    // This is hard to test without mocking TensorFlow
}
```

**View lifetime safety:**
```cpp
TEST(view_outlives_tensor) {
    tf::TensorView<const float, policy::NoLock, policy::NoLock::guard>* view_ptr;
    {
        auto tensor = FastTensor::FromVector<float>({4}, {1,2,3,4});
        auto view = tensor.read<float>();
        view_ptr = &view;  // BAD: Don't do this
        // But with shared_ptr<TensorState>, view keeps tensor alive
    }
    // Old bug: view_ptr->data would be dangling
    // With fix: TensorState still alive via shared_ptr in view
}
```

### 6. Concurrency Edge Cases (Priority: MEDIUM)

```cpp
TEST(concurrent_graph_modification) {
    SafeGraph graph;
    std::vector<std::thread> threads;
    std::atomic<int> success{0}, failure{0};
    
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto tensor = FastTensor::FromScalar<float>(i);
                graph.NewOperation("Const", "const_" + std::to_string(i))
                    .SetAttrTensor("value", tensor.handle())
                    .SetAttrType("dtype", TF_FLOAT)
                    .Finish();
                ++success;
            } catch (...) {
                ++failure;
            }
        });
    }
    
    for (auto& t : threads) t.join();
    REQUIRE(success + failure == 100);
}

TEST(deadlock_detection_timeout) {
    // Test that operations complete within reasonable time
    // Detect potential deadlocks by timing out
}

TEST(recursive_lock_attempt) {
    // With Mutex policy, verify recursive locking deadlocks
    // (or document that it does)
}
```

---

## Recommended Test Infrastructure

### 1. Add Sanitizer Builds to CI

```yaml
# .github/workflows/ci.yml additions
sanitizers:
  strategy:
    matrix:
      sanitizer: [address, undefined, thread, memory]
  steps:
    - run: |
        cmake -DCMAKE_CXX_FLAGS="-fsanitize=${{ matrix.sanitizer }}" ..
        make && ./tests
```

### 2. Add Fuzz Testing with libFuzzer

```cpp
// tests/fuzz_tensor_shape.cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) return 0;
    
    size_t num_dims = data[0] % 8;
    std::vector<int64_t> dims;
    
    for (size_t i = 0; i < num_dims && (i+1)*8 < size; ++i) {
        int64_t dim;
        std::memcpy(&dim, data + 1 + i*8, sizeof(dim));
        dims.push_back(dim);
    }
    
    try {
        auto t = tf::FastTensor::Allocate<float>(dims);
    } catch (const std::exception&) {
        // Expected for invalid inputs
    }
    
    return 0;
}
```

### 3. Add Property-Based Testing

```cpp
// Using rapidcheck or similar
rc::check("FromVector round-trips", [](std::vector<float> data) {
    if (data.empty()) return true;
    
    auto tensor = FastTensor::FromVector<float>({(int64_t)data.size()}, data);
    auto view = tensor.read<float>();
    
    return std::equal(data.begin(), data.end(), view.begin());
});
```

### 4. Add Benchmark Tests

```cpp
BENCHMARK(tensor_creation_small) {
    for (auto _ : state) {
        auto t = FastTensor::Allocate<float>({10, 10});
        benchmark::DoNotOptimize(t);
    }
}

BENCHMARK(tensor_creation_large) {
    for (auto _ : state) {
        auto t = FastTensor::Allocate<float>({1000, 1000});
        benchmark::DoNotOptimize(t);
    }
}

BENCHMARK(mutex_contention) {
    SafeTensor tensor = SafeTensor::Zeros<float>({100});
    // Measure lock acquisition overhead
}
```

---

## Priority Recommendations

### Immediate (Before Release)
1. Add moved-from object tests
2. Add empty tensor tests  
3. Add overflow boundary tests
4. Add ASAN to CI

### Short-term
1. Add stress test suite (separate from unit tests)
2. Add OperationBuilder error path tests
3. Add Session::Run error path tests

### Medium-term
1. Set up libFuzzer infrastructure
2. Add property-based tests
3. Add benchmark suite
4. Add TSAN, MSAN, UBSAN to CI

---

## Test File Structure Recommendation

```
tests/
├── test_main.cpp           # Current unit tests
├── test_stress.cpp         # Stress tests (run separately)
├── test_edge_cases.cpp     # Corner cases and boundaries
├── fuzz/
│   ├── fuzz_tensor_shape.cpp
│   ├── fuzz_operation_name.cpp
│   └── fuzz_graphdef.cpp
├── benchmarks/
│   └── bench_tensor.cpp
└── CMakeLists.txt          # With sanitizer options
```

---

## Summary

The current test suite covers **basic happy paths** but lacks:
- **No fuzz testing** - critical for a C API wrapper
- **Minimal stress testing** - only 3 threading tests
- **Sparse corner case coverage** - empty tensors, boundaries, moved-from states untested
- **Incomplete error path testing** - many throw paths untested
- **No sanitizer integration** - memory bugs could go undetected
- **No benchmarks** - performance regressions invisible

**Risk Level: MEDIUM-HIGH** - The wrapper handles raw pointers and C API calls. Without comprehensive testing, memory corruption and UB could slip through.
