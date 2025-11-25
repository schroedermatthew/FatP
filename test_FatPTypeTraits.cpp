/**
 * @file test_FatPTypeTraits.cpp
 * @brief Comprehensive unit tests for FatPTypeTraits.h
 */

#include <vector>
#include <iostream>
#include <cstdint>
#include <string>

#include "CppStandardDetection.h"
#include "SmallVector.h"
#include "CircularBuffer.h"
#include "FlatMap.h"
#include "FlatSet.h"
#include "SortedContainer.h"
#include "SparseSet.h"
#include "SlotMap.h"
#include "Expected.h"
#include "StrongId.h"
#include "ValueGuard.h"

#include "FatPTypeTraits.h"
#ifndef ENABLE_TEST_APPLICATION
    #include "test_FatPTypeTraits.h"
#endif
#include "FatPTest.h"

namespace fat_p::testing
{

struct PolicyWithValidate
{
    bool validate() const { return true; }
    bool validate(int x) const { return x > 0; }
};

struct PolicyWithoutValidate
{
    static void check(int) {}
};

struct PolicyWithSharedLocking
{
    struct SharedGuard {};
    struct LockGuard {};
};

struct PolicyWithoutSharedLocking
{
    struct LockGuard {};
};

struct LockFreePolicy
{
    struct LockFreeTag {};
};

struct LockingPolicy
{
};

struct BinarySerializableType
{
    void binary_serialize(std::vector<uint8_t>& buffer) const
    {
        buffer.push_back(static_cast<uint8_t>(value));
    }
    static BinarySerializableType binary_deserialize(const std::vector<uint8_t>& buffer)
    {
        BinarySerializableType obj;
        if (!buffer.empty())
        {
            obj.value = static_cast<int>(buffer[0]);
        }
        return obj;
    }
    int value = 0;
};

struct OnlyBinarySerializable
{
    void binary_serialize(std::vector<uint8_t>&) const {}
    int value = 0;
};

struct OnlyBinaryDeserializable
{
    static OnlyBinaryDeserializable binary_deserialize(const std::vector<uint8_t>&)
    {
        return OnlyBinaryDeserializable{};
    }
    int value = 0;
};

struct BenchmarkableType
{
    void benchmark_setup() {}
    void benchmark_run() {}
    void benchmark_teardown() {}
};

struct NonBenchmarkableType
{
    void setup() {}
    void run() {}
};

struct TypeWithCapacity
{
    size_t capacity() const { return 100; }
};

struct TypeWithoutCapacity
{
    size_t size() const { return 0; }
};

struct TypeWithShrinkToFit
{
    void shrink_to_fit() {}
};

struct TypeWithGetAllocator
{
    std::allocator<int> get_allocator() const { return {}; }
};

struct TypeWithLocking
{
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};

struct TypeWithoutLocking
{
    void acquire() {}
    void release() {}
};

struct TypeWithIsInline
{
    bool is_inline() const { return true; }
};

struct TypeWithoutIsInline
{
};

struct ExpectedLikeType
{
    int value() { return 42; }
    std::string error() { return "error"; }
};

struct NotExpectedLikeValue
{
    int value() { return 42; }
};

struct NotExpectedLikeError
{
    std::string error() { return "error"; }
};

struct TensorLikeType
{
    std::vector<size_t> shape() const { return {2, 3}; }
};

struct NotTensorLike
{
};

bool test_fatp_type_traits_has_validate()
{
    static_assert(has_validate_v<PolicyWithValidate>, "PolicyWithValidate should have validate");
    static_assert(!has_validate_v<PolicyWithoutValidate>, "PolicyWithoutValidate lacks validate");
    static_assert(!has_validate_v<int>, "int does not have validate");
    static_assert(!has_validate_v<std::vector<int>>, "vector does not have validate");
    
    return true;
}

bool test_fatp_type_traits_has_shared_locking()
{
    static_assert(has_shared_locking_v<PolicyWithSharedLocking>, 
                  "PolicyWithSharedLocking should have shared locking");
    static_assert(!has_shared_locking_v<PolicyWithoutSharedLocking>, 
                  "PolicyWithoutSharedLocking lacks shared locking");
    static_assert(!has_shared_locking_v<int>, "int does not have shared locking");
    
    return true;
}

bool test_fatp_type_traits_is_lock_free_policy()
{
    static_assert(is_lock_free_policy_v<LockFreePolicy>, "LockFreePolicy should be lock-free");
    static_assert(!is_lock_free_policy_v<LockingPolicy>, "LockingPolicy is not lock-free");
    static_assert(!is_lock_free_policy_v<int>, "int is not a lock-free policy");
    
    return true;
}

bool test_fatp_type_traits_binary_serialization()
{
    SUBTEST("has_binary_serialize")
    {
        static_assert(has_binary_serialize_v<BinarySerializableType>, 
                     "BinarySerializableType has serialize");
        static_assert(has_binary_serialize_v<OnlyBinarySerializable>, 
                     "OnlyBinarySerializable has serialize");
        static_assert(!has_binary_serialize_v<OnlyBinaryDeserializable>, 
                     "OnlyBinaryDeserializable lacks serialize");
        static_assert(!has_binary_serialize_v<int>, "int lacks serialize");
    }
    END_SUBTEST
    
    SUBTEST("has_binary_deserialize")
    {
        static_assert(has_binary_deserialize_v<BinarySerializableType>, 
                     "BinarySerializableType has deserialize");
        static_assert(has_binary_deserialize_v<OnlyBinaryDeserializable>, 
                     "OnlyBinaryDeserializable has deserialize");
        static_assert(!has_binary_deserialize_v<OnlyBinarySerializable>, 
                     "OnlyBinarySerializable lacks deserialize");
        static_assert(!has_binary_deserialize_v<int>, "int lacks deserialize");
    }
    END_SUBTEST
    
    SUBTEST("is_binary_serializable")
    {
        static_assert(is_binary_serializable_v<BinarySerializableType>, 
                     "BinarySerializableType is fully serializable");
        static_assert(!is_binary_serializable_v<OnlyBinarySerializable>, 
                     "OnlyBinarySerializable is incomplete");
        static_assert(!is_binary_serializable_v<OnlyBinaryDeserializable>, 
                     "OnlyBinaryDeserializable is incomplete");
        static_assert(!is_binary_serializable_v<int>, "int is not serializable");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_parallel_compatible()
{
    static_assert(is_parallel_algorithm_compatible_v<std::vector<int>>, 
                  "vector<int> is parallel compatible");
    static_assert(is_parallel_algorithm_compatible_v<std::vector<double>>, 
                  "vector<double> is parallel compatible");
    static_assert(!is_parallel_algorithm_compatible_v<int>, 
                  "int is not parallel compatible");
    
    return true;
}

bool test_fatp_type_traits_benchmark_interface()
{
    static_assert(has_benchmark_interface_v<BenchmarkableType>, 
                 "BenchmarkableType has benchmark interface");
    static_assert(!has_benchmark_interface_v<NonBenchmarkableType>, 
                 "NonBenchmarkableType lacks benchmark interface");
    static_assert(!has_benchmark_interface_v<int>, "int lacks benchmark interface");
    
    return true;
}

bool test_fatp_type_traits_small_vector_detection()
{
    using SV = SmallVector<int, 16>;
    
    static_assert(is_small_vector_v<SV>, "SmallVector should be detected");
    static_assert(!is_small_vector_v<std::vector<int>>, "vector is not SmallVector");
    static_assert(!is_small_vector_v<int>, "int is not SmallVector");
    
    static_assert(is_small_buffer_optimized_v<SV>, "SmallVector has SBO");
    static_assert(is_library_container_v<SV>, "SmallVector is library container");
    static_assert(!is_concurrent_container_v<SV>, "SmallVector is not concurrent");
    
    return true;
}

bool test_fatp_type_traits_circular_buffer_detection()
{
    using CB = CircularBuffer<int, 32>;
    
    static_assert(is_circular_buffer_v<CB>, "CircularBuffer should be detected");
    static_assert(!is_circular_buffer_v<std::vector<int>>, "vector is not CircularBuffer");
    static_assert(!is_circular_buffer_v<int>, "int is not CircularBuffer");
    
    static_assert(is_library_container_v<CB>, "CircularBuffer is library container");
    static_assert(!is_small_buffer_optimized_v<CB>, "CircularBuffer does not have SBO");
    
    return true;
}

bool test_fatp_type_traits_flat_containers_detection()
{
    SUBTEST("FlatMap")
    {
        using FM = FlatMap<int, std::string, std::less<int>, 
                          std::allocator<std::pair<const int, std::string>>>;
        
        static_assert(is_flat_map_v<FM>, "FlatMap should be detected");
        static_assert(!is_flat_map_v<std::vector<int>>, "vector is not FlatMap");
        static_assert(is_small_buffer_optimized_v<FM>, "FlatMap has SBO");
        static_assert(is_library_container_v<FM>, "FlatMap is library container");
    }
    END_SUBTEST
    
    SUBTEST("FlatSet")
    {
        using FS = FlatSet<int, std::less<int>, std::allocator<int>>;
        
        static_assert(is_flat_set_v<FS>, "FlatSet should be detected");
        static_assert(!is_flat_set_v<std::vector<int>>, "vector is not FlatSet");
        static_assert(is_small_buffer_optimized_v<FS>, "FlatSet has SBO");
        static_assert(is_library_container_v<FS>, "FlatSet is library container");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_sparse_containers_detection()
{
    SUBTEST("SparseSet")
    {
        using SS = SparseSet<int>;
        
        static_assert(is_sparse_set_v<SS>, "SparseSet should be detected");
        static_assert(!is_sparse_set_v<std::vector<int>>, "vector is not SparseSet");
        static_assert(is_library_container_v<SS>, "SparseSet is library container");
        static_assert(!is_small_buffer_optimized_v<SS>, "SparseSet does not have SBO");
    }
    END_SUBTEST
    
    SUBTEST("SlotMap")
    {
        using SM = SlotMap<int>;
        
        static_assert(is_slot_map_v<SM>, "SlotMap should be detected");
        static_assert(!is_slot_map_v<std::vector<int>>, "vector is not SlotMap");
        static_assert(is_library_container_v<SM>, "SlotMap is library container");
        static_assert(!is_small_buffer_optimized_v<SM>, "SlotMap does not have SBO");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_sorted_container_detection()
{
    static_assert(!is_sorted_container_v<std::vector<int>>, 
                 "vector is not SortedContainer");
    static_assert(!is_sorted_container_v<int>, "int is not SortedContainer");
    
    return true;
}

bool test_fatp_type_traits_tensor_detection()
{
    static_assert(!is_tensor_v<std::vector<int>>, "vector is not Tensor");
    static_assert(!is_fixed_tensor_v<std::vector<int>>, "vector is not FixedTensor");
    static_assert(!is_csr_matrix_v<std::vector<int>>, "vector is not CSRMatrix");
    static_assert(!is_simd_vector_v<std::vector<int>>, "vector is not SimdVector");
    
    static_assert(!is_tensor_type_v<std::vector<int>>, "vector is not any tensor type");
    static_assert(!is_tensor_type_v<int>, "int is not any tensor type");
    
    return true;
}

bool test_fatp_type_traits_concurrency_detection()
{
    static_assert(!is_lock_free_queue_v<std::vector<int>>, 
                 "vector is not LockFreeQueue");
    static_assert(!is_lock_free_ring_buffer_v<std::vector<int>>, 
                 "vector is not LockFreeRingBuffer");
    static_assert(!is_thread_pool_v<std::vector<int>>, "vector is not ThreadPool");
    static_assert(!is_atomic_reference_v<std::vector<int>>, 
                 "vector is not AtomicReference");
    static_assert(!is_spinlock_policy_v<std::vector<int>>, 
                 "vector is not spinlock policy");
    
    static_assert(!is_concurrent_container_v<std::vector<int>>, 
                 "vector is not concurrent container");
    
    return true;
}

bool test_fatp_type_traits_memory_detection()
{
    static_assert(!is_aligned_vector_v<std::vector<int>>, 
                 "vector is not AlignedVector");
    static_assert(!is_object_pool_v<std::vector<int>>, "vector is not ObjectPool");
    static_assert(!has_numa_allocator_v<std::vector<int>>, 
                 "vector does not have NumaAllocator");
    
    return true;
}

bool test_fatp_type_traits_utility_detection()
{
    SUBTEST("Expected")
    {
        static_assert(!is_expected_v<std::vector<int>>, "vector is not Expected");
        static_assert(!is_expected_v<int>, "int is not Expected");
        
        using Exp = expected_internal::ExpectedImpl<int, std::string, UnionStorage>;
        static_assert(is_expected_v<Exp>, "ExpectedImpl should be detected");
    }
    END_SUBTEST
    
    SUBTEST("StrongId")
    {
        static_assert(!is_strong_id_v<int>, "int is not StrongId");
        static_assert(!is_strong_id_v<std::vector<int>>, "vector is not StrongId");
    }
    END_SUBTEST
    
    SUBTEST("ValueGuard")
    {
        static_assert(!is_value_guard_v<int>, "int is not ValueGuard");
        static_assert(!is_value_guard_v<std::vector<int>>, "vector is not ValueGuard");
    }
    END_SUBTEST
    
    SUBTEST("ScopeGuard")
    {
        static_assert(!is_scope_guard_v<int>, "int is not ScopeGuard");
        static_assert(!is_scope_guard_v<std::vector<int>>, "vector is not ScopeGuard");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_small_buffer_optimized()
{
    static_assert(!is_small_buffer_optimized_v<std::vector<int>>, 
                 "vector is not SBO");
    static_assert(!is_small_buffer_optimized_v<int>, "int is not SBO");
    
    using SV = SmallVector<int, 16>;
    static_assert(is_small_buffer_optimized_v<SV>, "SmallVector has SBO");
    
    using FM = FlatMap<int, std::string, std::less<int>, 
                      std::allocator<std::pair<const int, std::string>>>;
    static_assert(is_small_buffer_optimized_v<FM>, "FlatMap has SBO");
    
    using FS = FlatSet<int, std::less<int>, std::allocator<int>>;
    static_assert(is_small_buffer_optimized_v<FS>, "FlatSet has SBO");
    
    return true;
}

bool test_fatp_type_traits_cache_aware()
{
    static_assert(!is_cache_aware_type_v<std::vector<int>>, 
                 "vector is not cache-aware type");
    static_assert(!is_cache_aware_type_v<int>, "int is not cache-aware type");
    
    return true;
}

bool test_fatp_type_traits_guard_type()
{
    static_assert(!is_guard_type_v<int>, "int is not a guard type");
    static_assert(!is_guard_type_v<std::vector<int>>, "vector is not a guard type");
    
    return true;
}

bool test_fatp_type_traits_method_detection_capacity()
{
    SUBTEST("has_capacity")
    {
        static_assert(has_capacity_v<TypeWithCapacity>, "TypeWithCapacity has capacity()");
        static_assert(!has_capacity_v<TypeWithoutCapacity>, 
                     "TypeWithoutCapacity lacks capacity()");
        static_assert(!has_capacity_v<int>, "int lacks capacity()");
    }
    END_SUBTEST
    
    SUBTEST("has_shrink_to_fit")
    {
        static_assert(has_shrink_to_fit_v<TypeWithShrinkToFit>, 
                     "TypeWithShrinkToFit has shrink_to_fit()");
        static_assert(!has_shrink_to_fit_v<int>, "int lacks shrink_to_fit()");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_method_detection_allocator()
{
    static_assert(has_get_allocator_v<TypeWithGetAllocator>, 
                 "TypeWithGetAllocator has get_allocator()");
    static_assert(!has_get_allocator_v<int>, "int lacks get_allocator()");
    
    return true;
}

bool test_fatp_type_traits_method_detection_locking()
{
    SUBTEST("has_lock")
    {
        static_assert(has_lock_v<TypeWithLocking>, "TypeWithLocking has lock()");
        static_assert(!has_lock_v<TypeWithoutLocking>, "TypeWithoutLocking lacks lock()");
        static_assert(!has_lock_v<int>, "int lacks lock()");
    }
    END_SUBTEST
    
    SUBTEST("has_unlock")
    {
        static_assert(has_unlock_v<TypeWithLocking>, "TypeWithLocking has unlock()");
        static_assert(!has_unlock_v<TypeWithoutLocking>, "TypeWithoutLocking lacks unlock()");
        static_assert(!has_unlock_v<int>, "int lacks unlock()");
    }
    END_SUBTEST
    
    SUBTEST("has_try_lock")
    {
        static_assert(has_try_lock_v<TypeWithLocking>, "TypeWithLocking has try_lock()");
        static_assert(!has_try_lock_v<TypeWithoutLocking>, 
                     "TypeWithoutLocking lacks try_lock()");
        static_assert(!has_try_lock_v<int>, "int lacks try_lock()");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_method_detection_container_types()
{
    SUBTEST("has_key_type")
    {
        using FM = FlatMap<int, std::string, std::less<int>, 
                          std::allocator<std::pair<const int, std::string>>>;
        static_assert(has_key_type_v<FM>, "FlatMap has key_type");
        static_assert(!has_key_type_v<std::vector<int>>, "vector lacks key_type");
    }
    END_SUBTEST
    
    SUBTEST("has_mapped_type")
    {
        using FM = FlatMap<int, std::string, std::less<int>, 
                          std::allocator<std::pair<const int, std::string>>>;
        static_assert(has_mapped_type_v<FM>, "FlatMap has mapped_type");
        static_assert(!has_mapped_type_v<std::vector<int>>, "vector lacks mapped_type");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_type_extraction_comprehensive()
{
    SUBTEST("container_value_type_t")
    {
        using SV = SmallVector<int, 16>;
        using value_type = container_value_type_t<SV>;
        static_assert(std::is_same_v<value_type, int>, "Should extract int");
        
        using vec_value = container_value_type_t<std::vector<double>>;
        static_assert(std::is_same_v<vec_value, double>, "Should extract double");
    }
    END_SUBTEST
    
    SUBTEST("container_key_type_t")
    {
        using FM = FlatMap<int, std::string, std::less<int>, 
                          std::allocator<std::pair<const int, std::string>>>;
        using key_type = container_key_type_t<FM>;
        static_assert(std::is_same_v<key_type, int>, "Should extract int key");
    }
    END_SUBTEST
    
    SUBTEST("container_mapped_type_t")
    {
        using FM = FlatMap<int, std::string, std::less<int>, 
                          std::allocator<std::pair<const int, std::string>>>;
        using mapped_type = container_mapped_type_t<FM>;
        static_assert(std::is_same_v<mapped_type, std::string>, 
                     "Should extract string mapped type");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_duck_typing_comprehensive()
{
    SUBTEST("is_small_vector_like")
    {
        static_assert(is_small_vector_like_v<TypeWithIsInline>, 
                     "Should detect is_inline() method");
        static_assert(!is_small_vector_like_v<TypeWithoutIsInline>, 
                     "Should not detect without is_inline()");
        static_assert(!is_small_vector_like_v<int>, "int is not small vector like");
    }
    END_SUBTEST
    
    SUBTEST("is_expected_like")
    {
        static_assert(is_expected_like_v<ExpectedLikeType>, 
                     "Should detect value() and error() methods");
        static_assert(!is_expected_like_v<NotExpectedLikeValue>, 
                     "Should require both value() and error()");
        static_assert(!is_expected_like_v<NotExpectedLikeError>, 
                     "Should require both value() and error()");
        static_assert(!is_expected_like_v<int>, "int is not expected like");
    }
    END_SUBTEST
    
    SUBTEST("is_tensor_like")
    {
        static_assert(is_tensor_like_v<TensorLikeType>, "Should detect shape() method");
        static_assert(!is_tensor_like_v<NotTensorLike>, 
                     "Should not detect without shape()");
        static_assert(!is_tensor_like_v<int>, "int is not tensor like");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_dbc_helpers_comprehensive()
{
    SUBTEST("requires_validate")
    {
        requires_validate<PolicyWithValidate>();
    }
    END_SUBTEST
    
    SUBTEST("requires_parallel_compatible")
    {
        requires_parallel_compatible<std::vector<int>>();
    }
    END_SUBTEST
    
    SUBTEST("requires_binary_serializable")
    {
        requires_binary_serializable<BinarySerializableType>();
    }
    END_SUBTEST
    
    SUBTEST("requires_library_container")
    {
        using SV = SmallVector<int, 16>;
        requires_library_container<SV>();
    }
    END_SUBTEST
    
    SUBTEST("requires_expected")
    {
        using Exp = expected_internal::ExpectedImpl<int, std::string, UnionStorage>;
        requires_expected<Exp>();
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_diagnostics_expected()
{
    SUBTEST("diagnose_expected on non-Expected type")
    {
        const char* diag = diagnose_expected<BinarySerializableType>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
        ASSERT_CONTAINS(diag, "Missing", "Should explain what is missing");
    }
    END_SUBTEST
    
    SUBTEST("diagnose_expected on Expected-like type")
    {
        const char* diag = diagnose_expected<ExpectedLikeType>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
    }
    END_SUBTEST
    
    SUBTEST("why_not_expected")
    {
        ASSERT_NOT_NULLPTR(why_not_expected<int>::reason, "Should provide reason for int");
        ASSERT_CONTAINS(why_not_expected<NotExpectedLikeValue>::reason, "Missing", 
                       "Should explain what is missing");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_diagnostics_tensor()
{
    SUBTEST("diagnose_tensor on non-Tensor type")
    {
        const char* diag = diagnose_tensor<std::vector<int>>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
        ASSERT_CONTAINS(diag, "Missing", "Should explain what is missing");
    }
    END_SUBTEST
    
    SUBTEST("diagnose_tensor on Tensor-like type")
    {
        const char* diag = diagnose_tensor<TensorLikeType>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
    }
    END_SUBTEST
    
    SUBTEST("why_not_tensor")
    {
        ASSERT_NOT_NULLPTR(why_not_tensor<int>::reason, "Should provide reason for int");
        ASSERT_CONTAINS(why_not_tensor<NotTensorLike>::reason, "Missing", 
                       "Should explain what is missing");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_diagnostics_serializable()
{
    SUBTEST("diagnose_binary_serializable on serializable type")
    {
        const char* diag = diagnose_binary_serializable<BinarySerializableType>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
        ASSERT_CONTAINS(diag, "serializable", "Should indicate serializability");
    }
    END_SUBTEST
    
    SUBTEST("diagnose_binary_serializable on partial type")
    {
        const char* diag = diagnose_binary_serializable<OnlyBinarySerializable>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
        ASSERT_CONTAINS(diag, "Missing", "Should explain what is missing");
    }
    END_SUBTEST
    
    SUBTEST("why_not_binary_serializable")
    {
        ASSERT_NOT_NULLPTR(why_not_binary_serializable<int>::reason, 
                          "Should provide reason for int");
        ASSERT_CONTAINS(why_not_binary_serializable<OnlyBinarySerializable>::reason, 
                       "Missing", "Should explain what is missing");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_diagnostics_container()
{
    SUBTEST("diagnose_library_container on standard type")
    {
        const char* diag = diagnose_library_container<std::vector<int>>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
        ASSERT_CONTAINS(diag, "not a library container", "Should indicate not a library type");
    }
    END_SUBTEST
    
    SUBTEST("diagnose_library_container on library type")
    {
        using SV = SmallVector<int, 16>;
        const char* diag = diagnose_library_container<SV>();
        ASSERT_NOT_NULLPTR(diag, "Should return diagnostic string");
        ASSERT_CONTAINS(diag, "SmallVector", "Should detect SmallVector");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_negative_cases()
{
    SUBTEST("Const types")
    {
        static_assert(!is_small_vector_v<const SmallVector<int, 16>>, 
                     "Const SmallVector should not match");
        static_assert(!is_expected_v<const expected_internal::ExpectedImpl<int, std::string, UnionStorage>>, 
                     "Const Expected should not match");
    }
    END_SUBTEST
    
    SUBTEST("Reference types")
    {
        using SV = SmallVector<int, 16>;
        static_assert(!is_small_vector_v<SV&>, "Reference should not match");
        static_assert(!is_small_vector_v<SV&&>, "Rvalue reference should not match");
    }
    END_SUBTEST
    
    SUBTEST("Pointer types")
    {
        using SV = SmallVector<int, 16>;
        static_assert(!is_small_vector_v<SV*>, "Pointer should not match");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_library_container_comprehensive()
{
    SUBTEST("SmallVector is library container")
    {
        using SV = SmallVector<int, 16>;
        static_assert(is_library_container_v<SV>, "SmallVector is library container");
    }
    END_SUBTEST
    
    SUBTEST("CircularBuffer is library container")
    {
        using CB = CircularBuffer<int, 32>;
        static_assert(is_library_container_v<CB>, "CircularBuffer is library container");
    }
    END_SUBTEST
    
    SUBTEST("FlatMap is library container")
    {
        using FM = FlatMap<int, std::string, std::less<int>, 
                          std::allocator<std::pair<const int, std::string>>>;
        static_assert(is_library_container_v<FM>, "FlatMap is library container");
    }
    END_SUBTEST
    
    SUBTEST("FlatSet is library container")
    {
        using FS = FlatSet<int, std::less<int>, std::allocator<int>>;
        static_assert(is_library_container_v<FS>, "FlatSet is library container");
    }
    END_SUBTEST
    
    SUBTEST("SparseSet is library container")
    {
        using SS = SparseSet<int>;
        static_assert(is_library_container_v<SS>, "SparseSet is library container");
    }
    END_SUBTEST
    
    SUBTEST("SlotMap is library container")
    {
        using SM = SlotMap<int>;
        static_assert(is_library_container_v<SM>, "SlotMap is library container");
    }
    END_SUBTEST
    
    SUBTEST("std::vector is not library container")
    {
        static_assert(!is_library_container_v<std::vector<int>>, 
                     "std::vector is not library container");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

bool test_fatp_type_traits_extension_points()
{
    SUBTEST("library_custom_traits default is empty")
    {
        using DefaultTraits = extension_points::library_custom_traits<int>;
        static_assert(std::is_class_v<DefaultTraits>, 
                     "library_custom_traits should be a class");
        // Note: We can't test if it's truly empty without specializing it
    }
    END_SUBTEST
    
    SUBTEST("library_custom_traits for SmallVector")
    {
        using SV = SmallVector<int, 16>;
        using SVTraits = extension_points::library_custom_traits<SV>;
        static_assert(std::is_class_v<SVTraits>, 
                     "library_custom_traits should work with SmallVector");
    }
    END_SUBTEST
    
    SUBTEST("library_custom_traits for custom type")
    {
        struct CustomType {};
        using CustomTraits = extension_points::library_custom_traits<CustomType>;
        static_assert(std::is_class_v<CustomTraits>, 
                     "library_custom_traits should work with custom types");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}

#if FATP_HAS_CPP20
bool test_fatp_type_traits_concepts()
{
    SUBTEST("SmallVectorType concept")
    {
        using SV = SmallVector<int, 16>;
        static_assert(concepts::SmallVectorType<SV>, "SmallVector should satisfy concept");
        static_assert(!concepts::SmallVectorType<std::vector<int>>, 
                     "vector should not satisfy concept");
    }
    END_SUBTEST
    
    SUBTEST("CircularBufferType concept")
    {
        using CB = CircularBuffer<int, 32>;
        static_assert(concepts::CircularBufferType<CB>, 
                     "CircularBuffer should satisfy concept");
        static_assert(!concepts::CircularBufferType<std::vector<int>>, 
                     "vector should not satisfy concept");
    }
    END_SUBTEST
    
    SUBTEST("FlatMapType concept")
    {
        using FM = FlatMap<int, std::string, std::less<int>, 
                          std::allocator<std::pair<const int, std::string>>>;
        static_assert(concepts::FlatMapType<FM>, "FlatMap should satisfy concept");
        static_assert(!concepts::FlatMapType<std::vector<int>>, 
                     "vector should not satisfy concept");
    }
    END_SUBTEST
    
    SUBTEST("FlatSetType concept")
    {
        using FS = FlatSet<int, std::less<int>, std::allocator<int>>;
        static_assert(concepts::FlatSetType<FS>, "FlatSet should satisfy concept");
        static_assert(!concepts::FlatSetType<std::vector<int>>, 
                     "vector should not satisfy concept");
    }
    END_SUBTEST
    
    SUBTEST("ExpectedType concept")
    {
        using Exp = expected_internal::ExpectedImpl<int, std::string, UnionStorage>;
        static_assert(concepts::ExpectedType<Exp>, "ExpectedImpl should satisfy concept");
        static_assert(!concepts::ExpectedType<int>, "int should not satisfy concept");
    }
    END_SUBTEST
    
    SUBTEST("BinarySerializable concept")
    {
        static_assert(concepts::BinarySerializable<BinarySerializableType>, 
                     "BinarySerializableType should satisfy concept");
        static_assert(!concepts::BinarySerializable<int>, 
                     "int should not satisfy concept");
    }
    END_SUBTEST
    
    SUBTEST("ParallelCompatible concept")
    {
        static_assert(concepts::ParallelCompatible<std::vector<int>>, 
                     "vector should satisfy concept");
        static_assert(!concepts::ParallelCompatible<int>, 
                     "int should not satisfy concept");
    }
    END_SUBTEST
    
    SUBTEST("LibraryContainer concept")
    {
        using SV = SmallVector<int, 16>;
        static_assert(concepts::LibraryContainer<SV>, 
                     "SmallVector should satisfy concept");
        static_assert(!concepts::LibraryContainer<std::vector<int>>, 
                     "std::vector should not satisfy concept");
    }
    END_SUBTEST
    
    return get_subtest_tracker().all_passed();
}
#endif

void benchmark_fatp_type_traits()
{
    std::cout << "\n" << colors::cyan() << "FatPTypeTraits Benchmarks:" 
              << colors::reset() << "\n\n";
    
    std::cout << "Note: FatPTypeTraits is compile-time only.\n";
    std::cout << "No runtime benchmarks needed - zero runtime overhead!\n";
    std::cout << "All type trait evaluations happen at compile time.\n";
}

bool test_FatPTypeTraits()
{
    PRINT_HEADER(FAT P TYPE TRAITS)

    TestRunner runner;

    RUN_TEST(runner, fatp_type_traits_has_validate);
    RUN_TEST(runner, fatp_type_traits_has_shared_locking);
    RUN_TEST(runner, fatp_type_traits_is_lock_free_policy);

    RUN_TEST(runner, fatp_type_traits_binary_serialization);
    RUN_TEST(runner, fatp_type_traits_parallel_compatible);
    RUN_TEST(runner, fatp_type_traits_benchmark_interface);

    RUN_TEST(runner, fatp_type_traits_small_vector_detection);
    RUN_TEST(runner, fatp_type_traits_circular_buffer_detection);
    RUN_TEST(runner, fatp_type_traits_flat_containers_detection);
    RUN_TEST(runner, fatp_type_traits_sparse_containers_detection);
    RUN_TEST(runner, fatp_type_traits_sorted_container_detection);
    
    RUN_TEST(runner, fatp_type_traits_tensor_detection);
    RUN_TEST(runner, fatp_type_traits_concurrency_detection);
    RUN_TEST(runner, fatp_type_traits_memory_detection);
    RUN_TEST(runner, fatp_type_traits_utility_detection);

    RUN_TEST(runner, fatp_type_traits_small_buffer_optimized);
    RUN_TEST(runner, fatp_type_traits_cache_aware);
    RUN_TEST(runner, fatp_type_traits_guard_type);
    
    RUN_TEST(runner, fatp_type_traits_method_detection_capacity);
    RUN_TEST(runner, fatp_type_traits_method_detection_allocator);
    RUN_TEST(runner, fatp_type_traits_method_detection_locking);
    RUN_TEST(runner, fatp_type_traits_method_detection_container_types);
    
    RUN_TEST(runner, fatp_type_traits_type_extraction_comprehensive);
    RUN_TEST(runner, fatp_type_traits_duck_typing_comprehensive);
    RUN_TEST(runner, fatp_type_traits_dbc_helpers_comprehensive);

    RUN_TEST(runner, fatp_type_traits_diagnostics_expected);
    RUN_TEST(runner, fatp_type_traits_diagnostics_tensor);
    RUN_TEST(runner, fatp_type_traits_diagnostics_serializable);
    RUN_TEST(runner, fatp_type_traits_diagnostics_container);
    
    RUN_TEST(runner, fatp_type_traits_negative_cases);
    RUN_TEST(runner, fatp_type_traits_library_container_comprehensive);
    RUN_TEST(runner, fatp_type_traits_extension_points);

    #if FATP_HAS_CPP20
    RUN_TEST(runner, fatp_type_traits_concepts);
    #endif

    benchmark_fatp_type_traits();

    int failed = runner.print_summary();
    return failed == 0;
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FatPTypeTraits() ? 0 : 1;
}
#endif
