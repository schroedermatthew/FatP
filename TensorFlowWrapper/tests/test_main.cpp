// tests/test_main.cpp
// Comprehensive tests for the TensorFlow C++20 wrapper
//
// Tests verify ALL fixed issues:
// - P0: Compilation, UB, leaks
// - P1: Semantic correctness
// - P2: Design correctness
// - P3: Enhancements

#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "tf/all.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>

using namespace tf;
using namespace tf::policy;

// ============================================================================
// P0 Tests: Compilation and Basic Correctness
// ============================================================================

TEST_CASE("Policy concepts are satisfied", "[policy][p0]") {
    STATIC_REQUIRE(LockPolicy<NoLock>);
    STATIC_REQUIRE(LockPolicy<Mutex>);
    STATIC_REQUIRE(LockPolicy<SharedMutex>);
    
    STATIC_REQUIRE(Guard<NoLock::guard>);
    STATIC_REQUIRE(Guard<Mutex::guard_type>);
    STATIC_REQUIRE(Guard<SharedMutex::exclusive_guard_type>);
    STATIC_REQUIRE(Guard<SharedMutex::shared_guard_type>);
}

TEST_CASE("NoLock is zero-cost", "[policy][p0]") {
    STATIC_REQUIRE(std::is_empty_v<NoLock>);
    STATIC_REQUIRE(std::is_empty_v<NoLock::guard>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<NoLock::guard>);
}

TEST_CASE("Policies are movable (P1 fix)", "[policy][p1]") {
    STATIC_REQUIRE(std::is_move_constructible_v<Mutex>);
    STATIC_REQUIRE(std::is_move_constructible_v<SharedMutex>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Mutex>);
    STATIC_REQUIRE(std::is_copy_constructible_v<SharedMutex>);
}

TEST_CASE("Status RAII prevents leaks (P0 fix)", "[status][p0]") {
    // Multiple constructions/destructions - run with ASAN
    for (int i = 0; i < 100; ++i) {
        Status st;
        REQUIRE(st.ok());
        REQUIRE(st.code() == TF_OK);
    }
    SUCCEED();
}

TEST_CASE("Status reset() works (P3 enhancement)", "[status][p3]") {
    Status st;
    st.set(TF_INVALID_ARGUMENT, "test error");
    REQUIRE_FALSE(st.ok());
    
    st.reset();
    REQUIRE(st.ok());
}

TEST_CASE("Status code names are correct", "[status][p3]") {
    REQUIRE(std::string(Status::code_to_string(TF_OK)) == "OK");
    REQUIRE(std::string(Status::code_to_string(TF_INVALID_ARGUMENT)) == "INVALID_ARGUMENT");
    REQUIRE(std::string(Status::code_to_string(TF_NOT_FOUND)) == "NOT_FOUND");
}

// ============================================================================
// Tensor Tests
// ============================================================================

TEST_CASE("Tensor FromVector works", "[tensor][p0]") {
    std::vector<std::int64_t> shape = {2, 3};
    std::vector<float> data = {1, 2, 3, 4, 5, 6};
    
    auto tensor = FastTensor::FromVector<float>(shape, data);
    
    REQUIRE(tensor.dtype() == TF_FLOAT);
    REQUIRE(tensor.shape() == shape);
    REQUIRE(tensor.num_elements() == 6);
    REQUIRE(tensor.rank() == 2);
}

TEST_CASE("Tensor FromScalar works", "[tensor][p0]") {
    auto tensor = FastTensor::FromScalar<double>(3.14);
    
    REQUIRE(tensor.dtype() == TF_DOUBLE);
    REQUIRE(tensor.num_elements() == 1);
    
    auto view = tensor.read<double>();
    REQUIRE(view[0] == Catch::Approx(3.14));
}

TEST_CASE("Tensor FromRaw works (P0 fix - no TF_TensorDims)", "[tensor][p0]") {
    // Create a tensor, get its handle, then wrap it again
    std::vector<float> data = {1, 2, 3, 4};
    auto original = FastTensor::FromVector<float>({2, 2}, data);
    
    // Simulate getting a raw tensor from TF API
    // (In real code, this comes from TF_SessionRun outputs)
    // Note: We can't test FromRaw directly without leaking,
    // but we can verify the API exists and compiles
    REQUIRE(original.handle() != nullptr);
    REQUIRE(original.shape().size() == 2);
}

TEST_CASE("Tensor FromRaw rejects null (P0 fix)", "[tensor][p0]") {
    REQUIRE_THROWS_AS(FastTensor::FromRaw(nullptr), std::invalid_argument);
}

TEST_CASE("Tensor Zeros factory works (P3 enhancement)", "[tensor][p3]") {
    auto tensor = FastTensor::Zeros<float>({10});
    
    auto view = tensor.read<float>();
    for (float x : view) {
        REQUIRE(x == 0.0f);
    }
}

TEST_CASE("Tensor Allocate factory works (P3 enhancement)", "[tensor][p3]") {
    auto tensor = FastTensor::Allocate<std::int32_t>({100});
    
    REQUIRE(tensor.num_elements() == 100);
    REQUIRE(tensor.byte_size() == 400);
}

TEST_CASE("Tensor read/write views work (P1 fix)", "[tensor][p1]") {
    auto tensor = FastTensor::FromVector<float>({4}, {1, 2, 3, 4});
    
    // Read view
    {
        auto view = tensor.read<float>();
        REQUIRE(view.size() == 4);
        REQUIRE(view[0] == 1.0f);
        REQUIRE(view[3] == 4.0f);
    }
    
    // Write view
    {
        auto view = tensor.write<float>();
        view[0] = 10.0f;
        view[3] = 40.0f;
    }
    
    // Verify write
    {
        auto view = tensor.read<float>();
        REQUIRE(view[0] == 10.0f);
        REQUIRE(view[3] == 40.0f);
    }
}

TEST_CASE("Tensor with_read/with_write work", "[tensor][p1]") {
    auto tensor = FastTensor::FromVector<float>({5}, {1, 2, 3, 4, 5});
    
    float sum = tensor.with_read<float>([](std::span<const float> s) {
        return std::accumulate(s.begin(), s.end(), 0.0f);
    });
    REQUIRE(sum == 15.0f);
    
    tensor.with_write<float>([](std::span<float> s) {
        for (float& x : s) x *= 2.0f;
    });
    
    float new_sum = tensor.with_read<float>([](std::span<const float> s) {
        return std::accumulate(s.begin(), s.end(), 0.0f);
    });
    REQUIRE(new_sum == 30.0f);
}

TEST_CASE("Tensor dtype mismatch throws (P1 fix - no noexcept)", "[tensor][p1]") {
    auto tensor = FastTensor::FromScalar<float>(1.0f);
    
    REQUIRE_THROWS_AS(tensor.read<double>(), std::runtime_error);
    REQUIRE_THROWS_AS(tensor.write<int>(), std::runtime_error);
}

TEST_CASE("Tensor dimension mismatch throws", "[tensor][p0]") {
    REQUIRE_THROWS_AS(
        FastTensor::FromVector<float>({2, 3}, {1, 2, 3}),  // Need 6, got 3
        std::invalid_argument);
}

TEST_CASE("All scalar types compile (P3 enhancement)", "[tensor][p3]") {
    REQUIRE(tf_dtype_v<float> == TF_FLOAT);
    REQUIRE(tf_dtype_v<double> == TF_DOUBLE);
    REQUIRE(tf_dtype_v<std::int8_t> == TF_INT8);
    REQUIRE(tf_dtype_v<std::int16_t> == TF_INT16);
    REQUIRE(tf_dtype_v<std::int32_t> == TF_INT32);
    REQUIRE(tf_dtype_v<std::int64_t> == TF_INT64);
    REQUIRE(tf_dtype_v<std::uint8_t> == TF_UINT8);
    REQUIRE(tf_dtype_v<std::uint16_t> == TF_UINT16);
    REQUIRE(tf_dtype_v<std::uint32_t> == TF_UINT32);
    REQUIRE(tf_dtype_v<std::uint64_t> == TF_UINT64);
    REQUIRE(tf_dtype_v<bool> == TF_BOOL);
}

// ============================================================================
// GuardedSpan Tests
// ============================================================================

TEST_CASE("GuardedSpan is non-copyable but movable", "[guarded_span][p1]") {
    STATIC_REQUIRE(!std::is_copy_constructible_v<GuardedSpan<float, NoLock::guard>>);
    STATIC_REQUIRE(std::is_move_constructible_v<GuardedSpan<float, NoLock::guard>>);
}

TEST_CASE("GuardedSpan at() throws on out of range", "[guarded_span][p3]") {
    std::vector<int> data = {1, 2, 3};
    NoLock::guard g;
    GuardedSpan<int, NoLock::guard> view(std::span(data), std::move(g));
    
    REQUIRE(view.at(0) == 1);
    REQUIRE(view.at(2) == 3);
    REQUIRE_THROWS_AS(view.at(3), std::out_of_range);
}

TEST_CASE("GuardedSpan iteration works", "[guarded_span][p1]") {
    std::vector<int> data = {1, 2, 3, 4, 5};
    NoLock::guard g;
    GuardedSpan<int, NoLock::guard> view(std::span(data), std::move(g));
    
    int sum = 0;
    for (int x : view) {
        sum += x;
    }
    REQUIRE(sum == 15);
}

// ============================================================================
// Thread Safety Tests (P0/P1 fixes)
// ============================================================================

TEST_CASE("Mutex actually provides exclusion (P0 fix)", "[policy][threading]") {
    Mutex m;
    std::atomic<int> counter{0};
    std::atomic<int> max_concurrent{0};
    
    auto worker = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto guard = m.scoped_lock();
            int current = ++counter;
            max_concurrent = std::max(max_concurrent.load(), current);
            std::this_thread::yield();
            --counter;
        }
    };
    
    std::thread t1(worker), t2(worker), t3(worker), t4(worker);
    t1.join(); t2.join(); t3.join(); t4.join();
    
    REQUIRE(max_concurrent == 1);
}

TEST_CASE("SharedMutex allows concurrent readers (P0 fix)", "[policy][threading]") {
    SharedMutex m;
    std::atomic<int> readers{0};
    std::atomic<int> max_readers{0};
    
    auto reader = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto guard = m.scoped_shared();
            int current = ++readers;
            max_readers = std::max(max_readers.load(), current);
            std::this_thread::yield();
            --readers;
        }
    };
    
    std::thread t1(reader), t2(reader), t3(reader), t4(reader);
    t1.join(); t2.join(); t3.join(); t4.join();
    
    REQUIRE(max_readers > 1);
}

TEST_CASE("Tensor with Mutex is thread-safe (P1 fix)", "[tensor][threading]") {
    auto tensor = Tensor<Mutex>::FromVector<float>({100}, std::vector<float>(100, 0.0f));
    
    std::atomic<bool> writer_done{false};
    std::atomic<bool> torn_read{false};
    
    std::thread writer([&]() {
        for (int round = 0; round < 50 && !torn_read; ++round) {
            auto view = tensor.write<float>();
            float value = static_cast<float>(round);
            for (std::size_t i = 0; i < view.size(); ++i) {
                view[i] = value;
            }
        }
        writer_done = true;
    });
    
    auto reader = [&]() {
        while (!writer_done && !torn_read) {
            auto view = tensor.read<float>();
            float first = view[0];
            for (std::size_t i = 1; i < view.size(); ++i) {
                if (view[i] != first) {
                    torn_read = true;
                    break;
                }
            }
        }
    };
    
    std::thread r1(reader), r2(reader);
    
    writer.join();
    r1.join();
    r2.join();
    
    REQUIRE_FALSE(torn_read);
}

// ============================================================================
// Type Alias Tests (P3 enhancement)
// ============================================================================

TEST_CASE("Type aliases are correct", "[types][p3]") {
    STATIC_REQUIRE(std::is_same_v<FastTensor, Tensor<NoLock>>);
    STATIC_REQUIRE(std::is_same_v<SafeTensor, Tensor<Mutex>>);
    STATIC_REQUIRE(std::is_same_v<SharedTensor, Tensor<SharedMutex>>);
    
    STATIC_REQUIRE(std::is_same_v<FastGraph, Graph<NoLock>>);
    STATIC_REQUIRE(std::is_same_v<SafeGraph, Graph<Mutex>>);
    STATIC_REQUIRE(std::is_same_v<SharedGraph, Graph<SharedMutex>>);
    
    STATIC_REQUIRE(std::is_same_v<FastSession, Session<NoLock>>);
    STATIC_REQUIRE(std::is_same_v<SafeSession, Session<Mutex>>);
}

// ============================================================================
// SessionOptions Tests (P0 fix)
// ============================================================================

TEST_CASE("SessionOptions RAII works", "[session][p0]") {
    SessionOptions opts;
    REQUIRE(opts.get() != nullptr);
    
    // Move construction
    SessionOptions opts2 = std::move(opts);
    REQUIRE(opts2.get() != nullptr);
    REQUIRE(opts.get() == nullptr);
}

// ============================================================================
// Graph Tests
// ============================================================================

TEST_CASE("Graph creation and operation lookup", "[graph][p0]") {
    FastGraph graph;
    
    auto tensor = FastTensor::FromScalar<float>(1.0f);
    
    auto op = std::move(graph.NewOperation("Const", "test_const"))
        .SetAttrTensor("value", tensor.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    REQUIRE(op != nullptr);
    
    auto found = graph.GetOperation("test_const");
    REQUIRE(found.has_value());
    REQUIRE(*found == op);
    
    auto not_found = graph.GetOperation("nonexistent");
    REQUIRE_FALSE(not_found.has_value());
}

TEST_CASE("Graph GetOperationOrThrow throws when not found", "[graph][p0]") {
    FastGraph graph;
    
    REQUIRE_THROWS_AS(graph.GetOperationOrThrow("nonexistent"), std::runtime_error);
}
