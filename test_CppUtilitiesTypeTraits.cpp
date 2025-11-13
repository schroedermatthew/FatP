/**
 * @file test_CppUtilitiesTypeTraits.cpp
 * @brief Unit tests for CppUtilitiesTypeTraits.h
 */

#include <vector>
#include <iostream>
#include <cstdint>

#include "CppUtilitiesTypeTraits.h"
#include "test_CppUtilitiesTypeTraits.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

struct PolicyWithValidate {
    bool validate() const { return true; }
    bool validate(int x) const { return x > 0; }
};

struct PolicyWithoutValidate {
    static void check(int) {}
};

struct PolicyWithSharedLocking {
    struct SharedGuard {};
    struct LockGuard {};
};

struct PolicyWithoutSharedLocking {
    struct LockGuard {};
};

struct LockFreePolicy {
    struct LockFreeTag {};
};

struct LockingPolicy {
};

struct BinarySerializableType {
    void binary_serialize(std::vector<uint8_t>& buffer) const {
        buffer.push_back(static_cast<uint8_t>(value));
    }
    static BinarySerializableType binary_deserialize(const std::vector<uint8_t>& buffer) {
        BinarySerializableType obj;
        if (!buffer.empty()) {
            obj.value = static_cast<int>(buffer[0]);
        }
        return obj;
    }
    int value = 0;
};

struct OnlyBinarySerializable {
    void binary_serialize(std::vector<uint8_t>&) const {}
    int value = 0;
};

struct OnlyBinaryDeserializable {
    static OnlyBinaryDeserializable binary_deserialize(const std::vector<uint8_t>&) {
        return OnlyBinaryDeserializable{};
    }
    int value = 0;
};

struct BenchmarkableType {
    void benchmark_setup() {}
    void benchmark_run() {}
    void benchmark_teardown() {}
};

struct NonBenchmarkableType {
    void setup() {}
    void run() {}
};

bool test_cpp_utilities_type_traits_has_validate() {
    static_assert(has_validate_v<PolicyWithValidate>, "PolicyWithValidate");
    
    static_assert(!has_validate_v<PolicyWithoutValidate>, "PolicyWithoutValidate");
    static_assert(!has_validate_v<int>, "int");
    static_assert(!has_validate_v<std::vector<int>>, "vector");
    
    return true;
}

bool test_cpp_utilities_type_traits_has_shared_locking() {
    static_assert(has_shared_locking_v<PolicyWithSharedLocking>, 
                  "PolicyWithSharedLocking");
    
    static_assert(!has_shared_locking_v<PolicyWithoutSharedLocking>, 
                  "PolicyWithoutSharedLocking");
    static_assert(!has_shared_locking_v<int>, "int");
    
    return true;
}

bool test_cpp_utilities_type_traits_is_lock_free_policy() {
    static_assert(is_lock_free_policy_v<LockFreePolicy>, "LockFreePolicy");
    
    static_assert(!is_lock_free_policy_v<LockingPolicy>, "LockingPolicy");
    static_assert(!is_lock_free_policy_v<int>, "int");
    
    return true;
}

bool test_cpp_utilities_type_traits_binary_serialization() {
    static_assert(has_binary_serialize_v<BinarySerializableType>, "BinarySerializableType");
    static_assert(has_binary_deserialize_v<BinarySerializableType>, "BinarySerializableType");
    static_assert(is_binary_serializable_v<BinarySerializableType>, "BinarySerializableType");
    
    static_assert(!is_binary_serializable_v<OnlyBinarySerializable>, "OnlyBinarySerializable");
    static_assert(!is_binary_serializable_v<OnlyBinaryDeserializable>, "OnlyBinaryDeserializable");
    static_assert(!is_binary_serializable_v<int>, "int");
    
    return true;
}

bool test_cpp_utilities_type_traits_parallel_compatible() {
    static_assert(is_parallel_algorithm_compatible_v<std::vector<int>>, 
                  "vector<int> is parallel compatible");
    static_assert(is_parallel_algorithm_compatible_v<std::vector<double>>, 
                  "vector<double> is parallel compatible");
    
    static_assert(!is_parallel_algorithm_compatible_v<int>, 
                  "int is not parallel compatible");
    
    return true;
}

bool test_cpp_utilities_type_traits_benchmark_interface() {
    static_assert(has_benchmark_interface_v<BenchmarkableType>, "BenchmarkableType");
    
    static_assert(!has_benchmark_interface_v<NonBenchmarkableType>, "NonBenchmarkableType");
    static_assert(!has_benchmark_interface_v<int>, "int");
    
    return true;
}

bool test_cpp_utilities_type_traits_container_detection() {
    static_assert(!is_small_vector_v<std::vector<int>>, "vector is not SmallVector");
    static_assert(!is_circular_buffer_v<std::vector<int>>, "vector is not CircularBuffer");
    static_assert(!is_flat_map_v<std::vector<int>>, "vector is not FlatMap");
    static_assert(!is_flat_set_v<std::vector<int>>, "vector is not FlatSet");
    
    return true;
}

bool test_cpp_utilities_type_traits_tensor_detection() {
    static_assert(!is_tensor_v<std::vector<int>>, "vector is not Tensor");
    static_assert(!is_fixed_tensor_v<std::vector<int>>, "vector is not FixedTensor");
    static_assert(!is_csr_matrix_v<std::vector<int>>, "vector is not CSRMatrix");
    static_assert(!is_simd_vector_v<std::vector<int>>, "vector is not SimdVector");
    
    return true;
}

bool test_cpp_utilities_type_traits_concurrency_detection() {
    static_assert(!is_lock_free_queue_v<std::vector<int>>, "vector is not LockFreeQueue");
    static_assert(!is_lock_free_ring_buffer_v<std::vector<int>>, "vector is not LockFreeRingBuffer");
    static_assert(!is_thread_pool_v<std::vector<int>>, "vector is not ThreadPool");
    static_assert(!is_atomic_reference_v<std::vector<int>>, "vector is not AtomicReference");
    
    return true;
}

bool test_cpp_utilities_type_traits_memory_detection() {
    static_assert(!is_aligned_vector_v<std::vector<int>>, "vector is not AlignedVector");
    static_assert(!is_object_pool_v<std::vector<int>>, "vector is not ObjectPool");
    static_assert(!has_numa_allocator_v<std::vector<int>>, "vector doesn't have NumaAllocator");
    
    return true;
}

bool test_cpp_utilities_type_traits_utility_detection() {
    static_assert(!is_expected_v<std::vector<int>>, "vector is not Expected");
    static_assert(!is_strong_id_v<int>, "int is not StrongId");
    static_assert(!is_value_guard_v<int>, "int is not ValueGuard");
    static_assert(!is_scope_guard_v<int>, "int is not ScopeGuard");
    
    return true;
}

bool test_cpp_utilities_type_traits_small_buffer_optimized() {
    static_assert(!is_small_buffer_optimized_v<std::vector<int>>, "vector is not SBO");
    
    return true;
}

bool test_cpp_utilities_type_traits_cache_aware() {
    static_assert(!is_cache_aware_type_v<std::vector<int>>, "vector is not cache-aware type");
    
    return true;
}

bool test_cpp_utilities_type_traits_dbc_helpers() {
    requires_validate<PolicyWithValidate>();
    requires_parallel_compatible<std::vector<int>>();
    
    return true;
}

void benchmark_cpp_utilities_type_traits() {
    std::cout << "\n" << colors::cyan() << "CppUtilitiesTypeTraits Benchmarks:" 
              << colors::reset() << "\n\n";
    
    std::cout << "Note: CppUtilitiesTypeTraits is compile-time only.\n";
    std::cout << "No runtime benchmarks needed - zero runtime overhead!\n";
}

bool test_CppUtilitiesTypeTraits() {
    PRINT_HEADER(CPP UTILITIES TYPE TRAITS)

    TestRunner runner;

    RUN_TEST(runner, cpp_utilities_type_traits_has_validate);
    RUN_TEST(runner, cpp_utilities_type_traits_has_shared_locking);
    RUN_TEST(runner, cpp_utilities_type_traits_is_lock_free_policy);

    RUN_TEST(runner, cpp_utilities_type_traits_binary_serialization);
    RUN_TEST(runner, cpp_utilities_type_traits_parallel_compatible);
    RUN_TEST(runner, cpp_utilities_type_traits_benchmark_interface);

    RUN_TEST(runner, cpp_utilities_type_traits_container_detection);
    RUN_TEST(runner, cpp_utilities_type_traits_tensor_detection);
    RUN_TEST(runner, cpp_utilities_type_traits_concurrency_detection);
    RUN_TEST(runner, cpp_utilities_type_traits_memory_detection);
    RUN_TEST(runner, cpp_utilities_type_traits_utility_detection);

    RUN_TEST(runner, cpp_utilities_type_traits_small_buffer_optimized);
    RUN_TEST(runner, cpp_utilities_type_traits_cache_aware);

    RUN_TEST(runner, cpp_utilities_type_traits_dbc_helpers);

    benchmark_cpp_utilities_type_traits();

    int failed = runner.print_summary();
    return failed == 0;
}

} // namespace cpp_utilities::testing
