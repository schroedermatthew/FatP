// tests/test_edge_cases.cpp
// Edge case, corner case, and stress tests for tf_wrapper
//
// Compile with: -fsanitize=address,undefined for best coverage

#include "tf/all.hpp"

#include "tf/format.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Test Framework (minimal, matches test_main.cpp style)
// ============================================================================

namespace tf_test {

struct TestCase {
    const char* name;
    void (*fn)();
    bool is_stress;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const char* name, void (*fn)(), bool is_stress = false) {
        registry().push_back({name, fn, is_stress});
    }
};

inline void require_impl(bool cond, const char* expr, const char* file, int line) {
    if (cond) return;
    throw std::runtime_error(tf::detail::format(
        "REQUIRE failed: {} ({}:{})", expr, file, line));
}

template<class Ex, class Fn>
inline void require_throws_impl(Fn&& fn, const char* expr, const char* ex_name,
                                const char* file, int line) {
    try {
        fn();
        throw std::runtime_error(tf::detail::format(
            "REQUIRE_THROWS failed: {} did not throw {} ({}:{})",
            expr, ex_name, file, line));
    } catch (const Ex&) {
        // Expected
    } catch (const std::exception& e) {
        throw std::runtime_error(tf::detail::format(
            "REQUIRE_THROWS failed: {} threw wrong type: {} ({}:{})",
            expr, e.what(), file, line));
    }
}

} // namespace tf_test

// Two-level macro for proper __LINE__ expansion
#define TF_JOIN2(a, b) a##b
#define TF_JOIN(a, b) TF_JOIN2(a, b)

#define TEST_CASE(name) \
    static void TF_JOIN(test_fn_, __LINE__)(); \
    static tf_test::Registrar TF_JOIN(test_reg_, __LINE__)( \
        name, &TF_JOIN(test_fn_, __LINE__), false); \
    static void TF_JOIN(test_fn_, __LINE__)()

#define STRESS_TEST(name) \
    static void TF_JOIN(stress_fn_, __LINE__)(); \
    static tf_test::Registrar TF_JOIN(stress_reg_, __LINE__)( \
        name, &TF_JOIN(stress_fn_, __LINE__), true); \
    static void TF_JOIN(stress_fn_, __LINE__)()

#define REQUIRE(expr) tf_test::require_impl((expr), #expr, __FILE__, __LINE__)
#define REQUIRE_THROWS_AS(expr, ex) \
    tf_test::require_throws_impl<ex>([&]{ (void)(expr); }, #expr, #ex, __FILE__, __LINE__)

// ============================================================================
// Empty/Zero Tensor Tests
// ============================================================================

TEST_CASE("empty tensor from zero-sized vector") {
    std::vector<std::int64_t> shape = {0};
    std::vector<float> data = {};
    auto tensor = tf::FastTensor::FromVector<float>(shape, data);
    REQUIRE(tensor.num_elements() == 0);
    REQUIRE(tensor.byte_size() == 0);
    REQUIRE(tensor.rank() == 1);
}

TEST_CASE("empty tensor from zero dimension in middle") {
    std::vector<std::int64_t> shape = {10, 0, 5};
    auto tensor = tf::FastTensor::Allocate<float>(shape);
    REQUIRE(tensor.num_elements() == 0);
    REQUIRE(tensor.byte_size() == 0);
    REQUIRE(tensor.rank() == 3);
}

TEST_CASE("scalar has empty shape") {
    auto tensor = tf::FastTensor::FromScalar<int>(42);
    REQUIRE(tensor.shape().empty());
    REQUIRE(tensor.rank() == 0);
    REQUIRE(tensor.num_elements() == 1);
}

TEST_CASE("empty tensor read view") {
    std::vector<std::int64_t> shape = {0};
    std::vector<float> data = {};
    auto tensor = tf::FastTensor::FromVector<float>(shape, data);
    auto view = tensor.read<float>();
    REQUIRE(view.size() == 0);
    REQUIRE(view.begin() == view.end());
}

// ============================================================================
// Boundary Value Tests
// ============================================================================

TEST_CASE("single element 1D tensor") {
    std::vector<std::int64_t> shape = {1};
    std::vector<float> data = {42.0f};
    auto tensor = tf::FastTensor::FromVector<float>(shape, data);
    REQUIRE(tensor.num_elements() == 1);
    auto view = tensor.read<float>();
    REQUIRE(view[0] == 42.0f);
}

TEST_CASE("high rank tensor 8 dimensions") {
    std::vector<std::int64_t> shape(8, 2);  // 2^8 = 256 elements
    auto tensor = tf::FastTensor::Allocate<float>(shape);
    REQUIRE(tensor.rank() == 8);
    REQUIRE(tensor.num_elements() == 256);
}

TEST_CASE("negative dimension throws") {
    std::vector<std::int64_t> shape = {10, -1, 5};
    REQUIRE_THROWS_AS(
        tf::FastTensor::Allocate<float>(shape),
        std::invalid_argument);
}

TEST_CASE("overflow in dimension product throws") {
    // Use dimensions that are individually valid but overflow when multiplied
    // INT64_MAX / 2 is positive and valid, but (INT64_MAX/2) * 3 overflows
    std::vector<std::int64_t> shape = {INT64_MAX / 2, 3};
    REQUIRE_THROWS_AS(
        tf::FastTensor::Allocate<float>(shape),
        std::overflow_error);
}

TEST_CASE("INT64_MAX dimension throws overflow") {
    std::vector<std::int64_t> shape = {INT64_MAX};
    REQUIRE_THROWS_AS(
        tf::FastTensor::Allocate<float>(shape),
        std::overflow_error);
}

// ============================================================================
// Moved-From Object Tests
// ============================================================================

TEST_CASE("moved-from tensor has null handle") {
    auto t1 = tf::FastTensor::FromScalar<float>(1.0f);
    REQUIRE(t1.handle() != nullptr);
    
    auto t2 = std::move(t1);
    REQUIRE(t2.handle() != nullptr);
    // t1 is now in moved-from state
}

TEST_CASE("moved-from status is safe to destroy") {
    tf::Status s1;
    s1.set(TF_CANCELLED, "test");
    tf::Status s2 = std::move(s1);
    
    REQUIRE(s2.code() == TF_CANCELLED);
    // s1 destruction should not double-free
}

TEST_CASE("moved-from graph is safe") {
    tf::FastGraph g1;
    auto tensor = tf::FastTensor::FromScalar<float>(1.0f);
    (void)g1.NewOperation("Const", "c")
        .SetAttrTensor("value", tensor.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    tf::FastGraph g2 = std::move(g1);
    REQUIRE(g2.GetOperation("c").has_value());
}

TEST_CASE("moved-from session options is safe") {
    tf::SessionOptions opts1;
    tf::SessionOptions opts2 = std::move(opts1);
    
    REQUIRE(opts2.get() != nullptr);
    REQUIRE(opts1.get() == nullptr);
}

// ============================================================================
// Adopt/AdoptMalloc Error Tests
// ============================================================================

TEST_CASE("AdoptMalloc wrong byte_len throws") {
    void* data = std::malloc(100);
    REQUIRE(data != nullptr);
    
    std::vector<std::int64_t> shape = {10};
    bool threw = false;
    try {
        // Shape {10} with float requires 40 bytes, not 100
        (void)tf::FastTensor::AdoptMalloc<float>(shape, data, 100);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    
    REQUIRE(threw);
    std::free(data);  // We still own it
}

TEST_CASE("Adopt with null deallocator throws") {
    void* data = std::malloc(40);
    REQUIRE(data != nullptr);
    
    std::vector<std::int64_t> shape = {10};
    REQUIRE_THROWS_AS(
        tf::FastTensor::Adopt(TF_FLOAT, shape, data, 40, nullptr),
        std::invalid_argument);
    
    std::free(data);
}

TEST_CASE("Adopt with null data and zero bytes succeeds") {
    auto dealloc = [](void*, std::size_t, void*) {};
    std::vector<std::int64_t> shape = {0};
    
    // Should not throw
    auto tensor = tf::FastTensor::Adopt(TF_FLOAT, shape, nullptr, 0, dealloc);
    REQUIRE(tensor.num_elements() == 0);
}

TEST_CASE("Adopt with null data and non-zero bytes throws") {
    auto dealloc = [](void*, std::size_t, void*) {};
    std::vector<std::int64_t> shape = {10};
    
    REQUIRE_THROWS_AS(
        tf::FastTensor::Adopt(TF_FLOAT, shape, nullptr, 40, dealloc),
        std::invalid_argument);
}

// ============================================================================
// Graph Error Tests
// ============================================================================

TEST_CASE("GetOperationOrThrow on empty graph throws") {
    tf::FastGraph graph;
    REQUIRE_THROWS_AS(
        graph.GetOperationOrThrow("nonexistent"),
        std::runtime_error);
}

TEST_CASE("GetOperation on empty graph returns nullopt") {
    tf::FastGraph graph;
    auto result = graph.GetOperation("nonexistent");
    REQUIRE(!result.has_value());
}

// ============================================================================
// Status Edge Cases
// ============================================================================

TEST_CASE("Status set with empty string_view") {
    tf::Status st;
    st.set(TF_INTERNAL, std::string_view{});
    REQUIRE(st.code() == TF_INTERNAL);
    REQUIRE(std::string(st.message()).empty());
}

TEST_CASE("Status set with very long message") {
    tf::Status st;
    std::string long_msg(10000, 'x');
    st.set(TF_INTERNAL, long_msg);
    REQUIRE(st.code() == TF_INTERNAL);
    REQUIRE(std::string(st.message()) == long_msg);
}

TEST_CASE("Status all error codes have names") {
    std::vector<TF_Code> codes = {
        TF_OK, TF_CANCELLED, TF_UNKNOWN, TF_INVALID_ARGUMENT,
        TF_DEADLINE_EXCEEDED, TF_NOT_FOUND, TF_ALREADY_EXISTS,
        TF_PERMISSION_DENIED, TF_UNAUTHENTICATED, TF_RESOURCE_EXHAUSTED,
        TF_FAILED_PRECONDITION, TF_ABORTED, TF_OUT_OF_RANGE,
        TF_UNIMPLEMENTED, TF_INTERNAL, TF_UNAVAILABLE, TF_DATA_LOSS
    };
    
    for (auto code : codes) {
        const char* name = tf::Status::code_to_string(code);
        REQUIRE(name != nullptr);
        REQUIRE(std::string(name) != "UNKNOWN_CODE");
    }
}

// ============================================================================
// Stress Tests
// ============================================================================

STRESS_TEST("rapid tensor allocation/deallocation") {
    constexpr int iterations = 10000;
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::int64_t> shape = {100, 100};
        auto t = tf::FastTensor::Allocate<float>(shape);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    const double ms_d = static_cast<double>(ms);
    std::cout << "    " << iterations << " allocations in " << ms << "ms "
              << "(" << (static_cast<double>(iterations) * 1000.0 / (ms_d + 1.0))
              << " ops/sec)\n";
}

STRESS_TEST("concurrent tensor creation") {
    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 1000;
    
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;
    
    auto start = std::chrono::steady_clock::now();
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                auto tensor = tf::FastTensor::FromScalar<float>(
                    static_cast<float>(t * ops_per_thread + i));
                ++completed;
            }
        });
    }
    
    for (auto& t : threads) t.join();
    
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    REQUIRE(completed.load() == num_threads * ops_per_thread);
    std::cout << "    " << completed.load() << " concurrent ops in " << ms << "ms\n";
}

STRESS_TEST("SafeTensor reader/writer contention") {
    std::vector<std::int64_t> shape = {1000};
    auto tensor = tf::SafeTensor::Zeros<float>(shape);
    
    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    std::atomic<bool> torn_read{false};
    
    // Writer thread
    std::thread writer([&]() {
        float value = 0.0f;
        while (!stop && !torn_read) {
            {
                auto view = tensor.write<float>();
                for (auto& x : view) x = value;
            }
            value += 1.0f;
            ++writes;
        }
    });
    
    // Multiple reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&]() {
            while (!stop && !torn_read) {
                auto view = tensor.read<float>();
                float first = view[0];
                for (std::size_t j = 1; j < view.size(); ++j) {
                    if (view[j] != first) {
                        torn_read = true;
                        break;
                    }
                }
                ++reads;
            }
        });
    }
    
    // Run for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop = true;
    
    writer.join();
    for (auto& r : readers) r.join();
    
    REQUIRE(!torn_read);
    std::cout << "    " << reads << " reads, " << writes << " writes, no torn reads\n";
}

STRESS_TEST("SharedTensor allows concurrent readers") {
    std::vector<std::int64_t> shape = {1000};
    std::vector<float> data(1000, 42.0f);
    auto tensor = tf::SharedTensor::FromVector<float>(shape, data);
    
    std::atomic<int> max_concurrent{0};
    std::atomic<int> current_readers{0};
    std::atomic<bool> stop{false};
    
    std::vector<std::thread> readers;
    for (int i = 0; i < 8; ++i) {
        readers.emplace_back([&]() {
            while (!stop) {
                auto view = tensor.read<float>();
                
                int current = ++current_readers;
                int expected = max_concurrent.load();
                while (current > expected) {
                    max_concurrent.compare_exchange_weak(expected, current);
                }
                
                float sum = 0;
                for (auto x : view) sum += x;
                (void)sum;
                
                --current_readers;
            }
        });
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stop = true;
    
    for (auto& r : readers) r.join();
    
    std::cout << "    Max concurrent readers: " << max_concurrent << "\n";
    REQUIRE(max_concurrent > 1);
}

STRESS_TEST("Status rapid creation/destruction") {
    constexpr int iterations = 100000;
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        tf::Status st;
        st.set(TF_INTERNAL, "test");
        st.reset();
    }
    
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "    " << iterations << " Status cycles in " << ms << "ms\n";
}

// ============================================================================
// Fuzz Tests - Random inputs to find edge cases
// ============================================================================

STRESS_TEST("fuzz: random tensor shapes") {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> rank_dist(0, 5);
    std::uniform_int_distribution<std::int64_t> dim_dist(0, 50);
    
    int created = 0;
    int failed_expected = 0;
    
    for (int i = 0; i < 500; ++i) {
        std::vector<std::int64_t> shape;
        int rank = rank_dist(gen);
        
        std::int64_t total_elements = 1;
        
        for (int d = 0; d < rank; ++d) {
            std::int64_t dim = dim_dist(gen);
            shape.push_back(dim);
            
            if (dim == 0) {
                total_elements = 0;
            } else if (total_elements > 0 && 
                       total_elements <= std::numeric_limits<std::int64_t>::max() / dim) {
                total_elements *= dim;
            }
        }
        
        try {
            std::vector<float> data(static_cast<std::size_t>(total_elements), 1.0f);
            auto tensor = tf::FastTensor::FromVector<float>(shape, data);
            
            REQUIRE(tensor.shape() == shape);
            REQUIRE(static_cast<std::int64_t>(tensor.num_elements()) == total_elements);
            
            if (total_elements > 0) {
                auto view = tensor.read<float>();
                REQUIRE(view.size() == static_cast<std::size_t>(total_elements));
            }
            
            ++created;
        } catch (const std::exception&) {
            ++failed_expected;
        }
    }
    
    std::cout << "    Created " << created << " tensors, " 
              << failed_expected << " expected failures\n";
    REQUIRE(created > 200);
}

STRESS_TEST("fuzz: random dtype values") {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int tested = 0;
    std::vector<std::int64_t> shape = {5, 5};
    
    // Test float with random values
    for (int i = 0; i < 50; ++i) {
        std::vector<float> data(25);
        for (auto& v : data) {
            v = static_cast<float>(gen()) / static_cast<float>(gen() + 1);
        }
        
        auto tensor = tf::FastTensor::FromVector<float>(shape, data);
        auto extracted = tensor.ToVector<float>();
        REQUIRE(extracted == data);
        ++tested;
    }
    
    // Test int32 with random values  
    for (int i = 0; i < 50; ++i) {
        std::vector<std::int32_t> data(25);
        for (auto& v : data) {
            v = static_cast<std::int32_t>(gen() % 10000);
        }
        
        auto tensor = tf::FastTensor::FromVector<std::int32_t>(shape, data);
        auto extracted = tensor.ToVector<std::int32_t>();
        REQUIRE(extracted == data);
        ++tested;
    }
    
    std::cout << "    Tested " << tested << " random tensor values\n";
}

STRESS_TEST("fuzz: OperationBuilder exception safety") {
    tf::FastGraph graph;
    int abandoned = 0;
    
    for (int i = 0; i < 50; ++i) {
        try {
            auto builder = graph.NewOperation("Const", "test_" + std::to_string(i));
            
            if (i % 3 == 0) {
                throw std::runtime_error("simulated error");
            }
            
            auto t = tf::FastTensor::FromScalar<float>(1.0f);
            builder.SetAttrTensor("value", t.handle());
            builder.SetAttrType("dtype", TF_FLOAT);
            (void)std::move(builder).Finish();
            
        } catch (const std::exception&) {
            ++abandoned;
        }
    }
    
    std::cout << "    " << abandoned << " builders safely abandoned\n";
    REQUIRE(abandoned > 0);
}

STRESS_TEST("fuzz: moved-from object safety") {
    for (int i = 0; i < 50; ++i) {
        tf::FastTensor t1 = tf::FastTensor::FromScalar<float>(static_cast<float>(i));
        tf::FastTensor t2 = std::move(t1);
        
        REQUIRE(t1.empty());
        REQUIRE(t1.handle() == nullptr);
        REQUIRE(!t2.empty());
        REQUIRE(t2.ToScalar<float>() == static_cast<float>(i));
        
        tf::FastTensor t3 = std::move(t2);
        REQUIRE(t2.empty());
        REQUIRE(!t3.empty());
        
        t1 = std::move(t3);
        REQUIRE(t3.empty());
        REQUIRE(!t1.empty());
    }
    
    std::cout << "    50 move sequences completed safely\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    bool run_stress = (argc > 1 && std::string(argv[1]) == "--stress");
    
    int passed = 0, failed = 0, skipped = 0;
    
    std::cout << "Running edge case tests...\n\n";
    
    for (const auto& tc : tf_test::registry()) {
        if (tc.is_stress && !run_stress) {
            ++skipped;
            continue;
        }
        
        std::cout << (tc.is_stress ? "[STRESS] " : "[TEST]   ") << tc.name << "\n";
        
        try {
            tc.fn();
            std::cout << "  PASS\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "  FAIL: " << e.what() << "\n";
            ++failed;
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Passed: " << passed << ", Failed: " << failed;
    if (skipped > 0) {
        std::cout << ", Skipped: " << skipped << " (run with --stress)";
    }
    std::cout << "\n";
    
    return failed > 0 ? 1 : 0;
}
