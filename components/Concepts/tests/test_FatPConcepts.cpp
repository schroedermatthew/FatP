/**
 * @file test_FatPConcepts.cpp
 * @brief Comprehensive unit tests for FatPConcepts.h
 *
 * @details Tests all Fat-P library-specific concepts.
 */

/*
FATP_META:
  meta_version: 1
  component: Concepts
  file_role: test
  path: components/Concepts/tests/test_FatPConcepts.cpp
  layer: Testing
  namespace: fat_p
  summary: "test file for Concepts"
  api_stability: in_work
  related:
    docs_search: "Concepts"
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/


#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "FatPConcepts.h"
#include "FatPTest.h"

// Include actual Fat-P types for testing
#include "CircularBuffer.h"
#include "FlatMap.h"
#include "FlatSet.h"
#include "LockFreeQueue.h"
#include "LockFreeRingBuffer.h"
#include "SlotMap.h"
#include "SmallVector.h"
#include "StrongId.h"
#include "Tensor.h"
#include "TensorStatic.h"

namespace fat_p::testing::fatpconcepts
{

// =============================================================================
// Helper Types
// =============================================================================

struct HasShape
{
    std::vector<int> shape() const { return {1, 2, 3}; }
};

struct HasTensorVocabulary
{
    using value_type = int;
    std::vector<std::size_t> extents() const { return {}; }
    int layout() const { return 0; }
    std::size_t size() const { return 0; }
    int operator[](std::size_t) const { return 0; }
};

struct HasIsInline
{
    bool is_inline() const { return true; }
};

struct HasLocking
{
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};

struct HasBinarySerialize
{
    void binary_serialize(std::vector<std::uint8_t>&) {}
    static HasBinarySerialize binary_deserialize(const std::vector<std::uint8_t>&) { return {}; }
};

struct HasBenchmarkInterface
{
    void benchmark_setup() {}
    void benchmark_run() {}
    void benchmark_teardown() {}
};

struct PlainType
{
    int value;
};

// Mock types for duck-typing tests
template <typename T, std::size_t N, typename A = std::allocator<T>>
struct MockSmallVector
{
    using value_type = T;
    T* begin() { return nullptr; }
    T* end() { return nullptr; }
    std::size_t size() const { return 0; }
    bool is_inline() const { return true; }
};

template <typename T, std::size_t N>
struct MockCircularBuffer
{
    using value_type = T;
    T* begin() { return nullptr; }
    T* end() { return nullptr; }
    std::size_t size() const { return 0; }
};

template <typename K, typename V, typename C = std::less<K>, typename A = std::allocator<std::pair<K, V>>>
struct MockFlatMap
{
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;
};

template <typename K, typename C = std::less<K>, typename A = std::allocator<K>>
struct MockFlatSet
{
    using key_type = K;
    using value_type = K;
};

template <typename T, typename A = std::allocator<T>, typename I = void, typename C = void>
struct MockTensor
{
    using value_type = T;
    std::vector<std::size_t> shape() const { return {}; }
};

template <typename T, std::size_t... Dims>
struct MockFixedTensor
{
    using value_type = T;
    std::array<std::size_t, sizeof...(Dims)> shape() const { return {}; }
};

template <typename T, std::size_t M = 1024, bool S = false>
struct MockLockFreeQueue
{
    using value_type = T;
};

template <typename T, typename E>
struct MockExpected
{
    using value_type = T;
    using error_type = E;
    T& value()
    {
        static T t{};
        return t;
    }
    E& error()
    {
        static E e{};
        return e;
    }
};

// =============================================================================
// Container Concept Tests
// =============================================================================

FATP_TEST_CASE(small_vector_type_concept)
{
    // Real SmallVector should match
    static_assert(fat_p::concepts::small_vector_type<fat_p::SmallVector<int, 16, std::allocator<int>>>);
    static_assert(fat_p::concepts::small_vector_type<fat_p::SmallVector<double, 8, std::allocator<double>>>);

    // Other types should not match
    static_assert(!fat_p::concepts::small_vector_type<std::vector<int>>);
    static_assert(!fat_p::concepts::small_vector_type<int>);
    static_assert(!fat_p::concepts::small_vector_type<MockSmallVector<int, 16>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for small_vector_type concept");
    return true;
}

FATP_TEST_CASE(circular_buffer_type_concept)
{
    static_assert(fat_p::concepts::circular_buffer_type<fat_p::CircularBuffer<int, 16>>);
    static_assert(!fat_p::concepts::circular_buffer_type<std::vector<int>>);
    static_assert(!fat_p::concepts::circular_buffer_type<MockCircularBuffer<int, 16>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for circular_buffer_type concept");
    return true;
}

FATP_TEST_CASE(flat_map_type_concept)
{
    static_assert(
        fat_p::concepts::flat_map_type<fat_p::FlatMap<int, int, std::less<int>, std::allocator<std::pair<int, int>>>>);
    static_assert(!fat_p::concepts::flat_map_type<std::map<int, int>>);
    static_assert(!fat_p::concepts::flat_map_type<MockFlatMap<int, int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for flat_map_type concept");
    return true;
}

FATP_TEST_CASE(flat_set_type_concept)
{
    static_assert(fat_p::concepts::flat_set_type<fat_p::FlatSet<int, std::less<int>, std::allocator<int>>>);
    static_assert(!fat_p::concepts::flat_set_type<std::set<int>>);
    static_assert(!fat_p::concepts::flat_set_type<MockFlatSet<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for flat_set_type concept");
    return true;
}

FATP_TEST_CASE(slot_map_type_concept)
{
    static_assert(fat_p::concepts::slot_map_type<fat_p::SlotMap<int, std::allocator<int>>>);
    static_assert(!fat_p::concepts::slot_map_type<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for slot_map_type concept");
    return true;
}

// =============================================================================
// Tensor Concept Tests
// =============================================================================

FATP_TEST_CASE(tensor_type_concept)
{
    static_assert(fat_p::concepts::tensor_type<fat_p::Tensor<float, std::allocator<float>>>);
    static_assert(fat_p::concepts::tensor_type<const fat_p::Tensor<float, std::allocator<float>>&>);
    static_assert(fat_p::concepts::tensor_view_type<fat_p::TensorView<float>>);
    static_assert(fat_p::concepts::tensor_view_type<fat_p::SharedTensorView<const float>>);
    static_assert(fat_p::concepts::fixed_tensor_type<fat_p::StaticTensor<float, fat_p::Vector<3>>>);
    static_assert(fat_p::concepts::fixed_tensor_type<const fat_p::StaticTensor<float, fat_p::Vector<3>>&>);
    static_assert(fat_p::concepts::any_tensor_type<fat_p::StaticTensor<float, fat_p::Vector<3>>>);
    static_assert(!fat_p::concepts::tensor_type<std::vector<float>>);
    static_assert(!fat_p::concepts::tensor_type<MockTensor<float>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for tensor_type concept");
    return true;
}

FATP_TEST_CASE(tensor_like_concept)
{
    static_assert(fat_p::concepts::tensor_like<fat_p::Tensor<float>>);
    static_assert(fat_p::concepts::tensor_like<fat_p::TensorView<const float>>);
    static_assert(fat_p::concepts::tensor_like<HasTensorVocabulary>);
    static_assert(!fat_p::concepts::tensor_like<HasShape>);
    static_assert(!fat_p::concepts::tensor_like<MockTensor<float>>);
    static_assert(!fat_p::concepts::tensor_like<MockFixedTensor<float, 2, 3>>);

    static_assert(!fat_p::concepts::tensor_like<std::vector<int>>);
    static_assert(!fat_p::concepts::tensor_like<PlainType>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for tensor_like concept");
    return true;
}

// =============================================================================
// Concurrency Concept Tests
// =============================================================================

FATP_TEST_CASE(lock_free_queue_type_concept)
{
    static_assert(fat_p::concepts::lock_free_queue_type<fat_p::LockFreeQueue<int, 1024, false>>);
    static_assert(!fat_p::concepts::lock_free_queue_type<std::queue<int>>);
    static_assert(!fat_p::concepts::lock_free_queue_type<MockLockFreeQueue<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for lock_free_queue_type concept");
    return true;
}

FATP_TEST_CASE(lock_free_ring_buffer_type_concept)
{
    static_assert(fat_p::concepts::lock_free_ring_buffer_type<fat_p::LockFreeRingBuffer<int>>);
    static_assert(fat_p::concepts::lock_free_ring_buffer_type<fat_p::LockFreeRingBufferMPMC<int>>);
    static_assert(!fat_p::concepts::lock_free_ring_buffer_type<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for lock_free_ring_buffer_type concept");
    return true;
}

FATP_TEST_CASE(concurrent_container_concept)
{
    static_assert(fat_p::concepts::concurrent_container<fat_p::LockFreeQueue<int, 1024, false>>);
    static_assert(fat_p::concepts::concurrent_container<fat_p::LockFreeRingBuffer<int>>);
    static_assert(!fat_p::concepts::concurrent_container<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for concurrent_container concept");
    return true;
}

FATP_TEST_CASE(lockable_concept)
{
    // Duck-typed: anything with lock/unlock/try_lock
    static_assert(fat_p::concepts::lockable<HasLocking>);
    static_assert(fat_p::concepts::lockable<std::mutex>);

    static_assert(!fat_p::concepts::lockable<PlainType>);
    static_assert(!fat_p::concepts::lockable<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for lockable concept");
    return true;
}

// =============================================================================
// Utility Concept Tests
// =============================================================================

FATP_TEST_CASE(expected_like_concept)
{
    // Duck-typed: anything with value() and error()
    static_assert(fat_p::concepts::expected_like<MockExpected<int, std::string>>);

    static_assert(!fat_p::concepts::expected_like<std::optional<int>>);
    static_assert(!fat_p::concepts::expected_like<PlainType>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for expected_like concept");
    return true;
}

FATP_TEST_CASE(strong_id_type_concept)
{
    struct MyTag
    {
    };

    static_assert(fat_p::concepts::strong_id_type<
                  fat_p::StrongId<int, MyTag, fat_p::strong_id::NoCheckPolicy, fat_p::strong_id::DefaultOpPolicy>>);
    static_assert(!fat_p::concepts::strong_id_type<int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for strong_id_type concept");
    return true;
}

// =============================================================================
// Composite Concept Tests
// =============================================================================

FATP_TEST_CASE(library_container_concept)
{
    static_assert(fat_p::concepts::library_container<fat_p::SmallVector<int, 16, std::allocator<int>>>);
    static_assert(fat_p::concepts::library_container<fat_p::CircularBuffer<int, 16>>);
    static_assert(fat_p::concepts::library_container<
                  fat_p::FlatMap<int, int, std::less<int>, std::allocator<std::pair<int, int>>>>);

    static_assert(!fat_p::concepts::library_container<std::vector<int>>);
    static_assert(!fat_p::concepts::library_container<std::map<int, int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for library_container concept");
    return true;
}

FATP_TEST_CASE(small_buffer_optimized_concept)
{
    static_assert(fat_p::concepts::small_buffer_optimized<fat_p::SmallVector<int, 16, std::allocator<int>>>);
    static_assert(fat_p::concepts::small_buffer_optimized<
                  fat_p::FlatMap<int, int, std::less<int>, std::allocator<std::pair<int, int>>>>);

    static_assert(!fat_p::concepts::small_buffer_optimized<fat_p::CircularBuffer<int, 16>>);
    static_assert(!fat_p::concepts::small_buffer_optimized<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for small_buffer_optimized concept");
    return true;
}

FATP_TEST_CASE(small_vector_like_concept)
{
    // Duck-typed: anything with is_inline()
    static_assert(fat_p::concepts::small_vector_like<HasIsInline>);
    static_assert(fat_p::concepts::small_vector_like<MockSmallVector<int, 16>>);

    static_assert(!fat_p::concepts::small_vector_like<std::vector<int>>);
    static_assert(!fat_p::concepts::small_vector_like<PlainType>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for small_vector_like concept");
    return true;
}

// =============================================================================
// Serialization Concept Tests
// =============================================================================

FATP_TEST_CASE(binary_serializable_concept)
{
    static_assert(fat_p::concepts::has_binary_serialize<HasBinarySerialize>);
    static_assert(fat_p::concepts::has_binary_deserialize<HasBinarySerialize>);
    static_assert(fat_p::concepts::binary_serializable<HasBinarySerialize>);

    static_assert(!fat_p::concepts::binary_serializable<PlainType>);
    static_assert(!fat_p::concepts::binary_serializable<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for binary_serializable concept");
    return true;
}

// =============================================================================
// Method Detection Concept Tests
// =============================================================================

FATP_TEST_CASE(method_detection_concepts)
{
    // has_capacity
    static_assert(fat_p::concepts::has_capacity<std::vector<int>>);
    static_assert(fat_p::concepts::has_capacity<std::string>);
    static_assert(!fat_p::concepts::has_capacity<std::list<int>>);

    // has_shrink_to_fit
    static_assert(fat_p::concepts::has_shrink_to_fit<std::vector<int>>);
    static_assert(!fat_p::concepts::has_shrink_to_fit<std::array<int, 5>>);

    // has_get_allocator
    static_assert(fat_p::concepts::has_get_allocator<std::vector<int>>);
    static_assert(!fat_p::concepts::has_get_allocator<std::array<int, 5>>);

    // has_key_type
    static_assert(fat_p::concepts::has_key_type<std::map<int, int>>);
    static_assert(fat_p::concepts::has_key_type<MockFlatMap<int, int>>);
    static_assert(!fat_p::concepts::has_key_type<std::vector<int>>);

    // has_mapped_type
    static_assert(fat_p::concepts::has_mapped_type<std::map<int, int>>);
    static_assert(fat_p::concepts::has_mapped_type<MockFlatMap<int, int>>);
    static_assert(!fat_p::concepts::has_mapped_type<std::set<int>>);

    // has_error_type
    static_assert(fat_p::concepts::has_error_type<MockExpected<int, std::string>>);
    static_assert(!fat_p::concepts::has_error_type<std::optional<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for method detection concepts");
    return true;
}

FATP_TEST_CASE(has_benchmark_interface_concept)
{
    static_assert(fat_p::concepts::has_benchmark_interface<HasBenchmarkInterface>);
    static_assert(!fat_p::concepts::has_benchmark_interface<PlainType>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for has_benchmark_interface concept");
    return true;
}

// =============================================================================
// Parallel Algorithm Concept Tests
// =============================================================================

FATP_TEST_CASE(parallel_compatible_concept)
{
    static_assert(fat_p::concepts::parallel_compatible<std::vector<int>>);
    static_assert(fat_p::concepts::parallel_compatible<std::string>);
    static_assert(fat_p::concepts::parallel_compatible<std::deque<int>>);

    // Types without value_type or not iterable
    static_assert(!fat_p::concepts::parallel_compatible<int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for parallel_compatible concept");
    return true;
}

} // namespace fat_p::testing::fatpconcepts

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_FatPConcepts()
{
    FATP_PRINT_HEADER(FATP CONCEPTS)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Container Concepts
    out << colors::blue() << "--- Container Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, small_vector_type_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, circular_buffer_type_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, flat_map_type_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, flat_set_type_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, slot_map_type_concept);

    // Tensor Concepts
    out << "\n" << colors::blue() << "--- Tensor Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, tensor_type_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, tensor_like_concept);

    // Concurrency Concepts
    out << "\n" << colors::blue() << "--- Concurrency Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, lock_free_queue_type_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, lock_free_ring_buffer_type_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, concurrent_container_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, lockable_concept);

    // Utility Concepts
    out << "\n" << colors::blue() << "--- Utility Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, expected_like_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, strong_id_type_concept);

    // Composite Concepts
    out << "\n" << colors::blue() << "--- Composite Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, library_container_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, small_buffer_optimized_concept);
    FATP_RUN_TEST_NS(runner, fatpconcepts, small_vector_like_concept);

    // Serialization Concepts
    out << "\n" << colors::blue() << "--- Serialization Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, binary_serializable_concept);

    // Method Detection Concepts
    out << "\n" << colors::blue() << "--- Method Detection Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, method_detection_concepts);
    FATP_RUN_TEST_NS(runner, fatpconcepts, has_benchmark_interface_concept);

    // Parallel Algorithm Concepts
    out << "\n" << colors::blue() << "--- Parallel Algorithm Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fatpconcepts, parallel_compatible_concept);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FatPConcepts() ? 0 : 1;
}
#endif
