/**
 * @file FatPTypeTraits.h
 * @brief Library-specific type traits for fat_penelope components
 *
 * @details Provides comprehensive type traits for detecting and classifying fat_penelope
 * library types. Uses forward declarations to avoid circular dependencies. Component headers
 * specialize their corresponding traits at the end of the header file.
 *
 * @section architecture Architecture
 * - Forward declarations only (no component includes)
 * - Trait templates default to std::false_type
 * - Specializations added by component headers
 * - Detection-based traits use SFINAE for duck-typing
 * - Zero runtime overhead (compile-time only)
 *
 * @section usage Usage
 * @code
 * // Check if type is a SmallVector
 * if constexpr (is_small_vector_v<MyType>) {
 *     // SmallVector-specific code
 * }
 *
 * // Extract nested types safely
 * using value_type = container_value_type_t<Container>;
 *
 * // Diagnostic utilities
 * auto info = diagnose_expected<MyType>();
 *
 * // C++20 concepts (in fat_p::concepts namespace)
 * template <fat_p::concepts::TensorType T>
 * void process(T&& tensor) { }
 * @endcode
 *
 * @note This is a compile-time only header with zero runtime overhead
 * @see TypeTraits.h for general-purpose type traits
 */

#pragma once

#include <type_traits>
#include <vector>
#include <cstdint>

#include "CppStandardDetection.h"
#include "TypeTraits.h"

namespace fat_p {

// =============================================================================
// Forward Declarations
// =============================================================================

template <typename T, size_t InlineCapacity, typename Allocator> class SmallVector;
template <typename T, size_t Capacity> class CircularBuffer;
template <typename Key, typename Value, typename Compare, typename Allocator> class FlatMap;
template <typename Key, typename Compare, typename Allocator> class FlatSet;
template <typename T, typename UniquenessPolicy, typename ComparePolicy, typename Allocator, typename ConcurrencyPolicy, template <typename, typename> class BackendPolicy> class SortedContainer;
template <typename T> class SparseSet;
template <typename T, typename Allocator> class SlotMap;
template <typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy> class Tensor;
template <typename T, size_t... Dims> class FixedTensor;
template <typename T, typename IndexType> class CSRMatrix;
template <typename T> class SimdVector;
template <typename T, size_t MaxSize> class LockFreeQueue;
template <typename T> class LockFreeRingBuffer;
class ThreadPool;
template <typename T, typename EnforcementPolicy, template <typename> class WaitPolicy, typename DurationPolicy> class AtomicReference;
template <typename T, size_t Alignment> class AlignedVector;
template <typename T, typename SyncPolicy> class ObjectPool;
template <typename T> class NumaAllocator;

namespace expected_internal {
    template <typename T, typename E, template <typename, typename> class StoragePolicy> class ExpectedImpl;
}

// StrongId: 4 template parameters (T, Tag, CheckPolicy, OpPolicy)
// Thread safety via std::atomic<StrongId<...>> wrapper, not internal ConcurrencyPolicy
template <typename T, typename Tag, typename CheckPolicy, template <typename> class OpPolicy> class StrongId;

template <typename T, typename Policy> class ValueGuard;

template <typename F, 
          typename ThrowingPolicy, 
          template <typename> class ActionPolicy> 
class ScopeGuard;

// =============================================================================
// Detection Helpers
// =============================================================================

namespace cpp_util_detail {
    template <typename T>
    using container_value_type_t = typename T::value_type;

    template <typename T>
    using container_key_type_t = typename T::key_type;

    template <typename T>
    using container_mapped_type_t = typename T::mapped_type;

    template <typename T>
    using allocator_type_t = typename T::allocator_type;

    template <typename T>
    using error_type_t = typename T::error_type;

    template <typename T>
    using tag_type_t = typename T::tag_type;

    template <typename T>
    using shape_t = decltype(std::declval<const T&>().shape());

    template <typename T>
    using is_inline_t = decltype(std::declval<const T&>().is_inline());

    template <typename T>
    using value_method_t = decltype(std::declval<T&>().value());

    template <typename T>
    using error_method_t = decltype(std::declval<T&>().error());

    template <typename T>
    using capacity_t = decltype(std::declval<const T&>().capacity());

    template <typename T>
    using shrink_to_fit_t = decltype(std::declval<T&>().shrink_to_fit());

    template <typename T>
    using get_allocator_t = decltype(std::declval<const T&>().get_allocator());

    template <typename T, typename U>
    using insert_t = decltype(std::declval<T&>().insert(std::declval<U>()));

    template <typename T, typename U>
    using erase_t = decltype(std::declval<T&>().erase(std::declval<U>()));

    template <typename T, typename U>
    using find_t = decltype(std::declval<T&>().find(std::declval<U>()));

    template <typename T>
    using lock_t = decltype(std::declval<T&>().lock());

    template <typename T>
    using unlock_t = decltype(std::declval<T&>().unlock());

    template <typename T>
    using try_lock_t = decltype(std::declval<T&>().try_lock());

    template <typename T>
    using binary_serialize_t = decltype(std::declval<T&>().binary_serialize(
        std::declval<std::vector<uint8_t>&>()));

    template <typename T>
    using binary_deserialize_t = decltype(T::binary_deserialize(
        std::declval<const std::vector<uint8_t>&>()));

    template <typename T>
    using benchmark_interface_t = decltype(
        std::declval<T&>().benchmark_setup(),
        std::declval<T&>().benchmark_run(),
        std::declval<T&>().benchmark_teardown()
    );
}

// =============================================================================
// Container Type Traits
// =============================================================================

/**
 * @brief Detects if T is a SmallVector
 */
template <typename T>
struct is_small_vector : std::false_type {};

template <typename T>
inline constexpr bool is_small_vector_v = is_small_vector<T>::value;

/**
 * @brief Detects if T is a CircularBuffer
 */
template <typename T>
struct is_circular_buffer : std::false_type {};

template <typename T>
inline constexpr bool is_circular_buffer_v = is_circular_buffer<T>::value;

/**
 * @brief Detects if T is a FlatMap
 */
template <typename T>
struct is_flat_map : std::false_type {};

template <typename T>
inline constexpr bool is_flat_map_v = is_flat_map<T>::value;

/**
 * @brief Detects if T is a FlatSet
 */
template <typename T>
struct is_flat_set : std::false_type {};

template <typename T>
inline constexpr bool is_flat_set_v = is_flat_set<T>::value;

/**
 * @brief Detects if T is a SortedContainer
 */
template <typename T>
struct is_sorted_container : std::false_type {};

template <typename T>
inline constexpr bool is_sorted_container_v = is_sorted_container<T>::value;

/**
 * @brief Detects if T is a SparseSet
 */
template <typename T>
struct is_sparse_set : std::false_type {};

template <typename T>
inline constexpr bool is_sparse_set_v = is_sparse_set<T>::value;

/**
 * @brief Detects if T is a SlotMap
 */
template <typename T>
struct is_slot_map : std::false_type {};

template <typename T, typename Allocator>
struct is_slot_map<SlotMap<T, Allocator>> : std::true_type {};

template <typename T>
inline constexpr bool is_slot_map_v = is_slot_map<T>::value;

// =============================================================================
// Tensor Type Traits
// =============================================================================

/**
 * @brief Detects if T is a Tensor
 */
template <typename T>
struct is_tensor : std::false_type {};

template <typename T>
inline constexpr bool is_tensor_v = is_tensor<T>::value;

/**
 * @brief Detects if T is a FixedTensor
 */
template <typename T>
struct is_fixed_tensor : std::false_type {};

template <typename T>
inline constexpr bool is_fixed_tensor_v = is_fixed_tensor<T>::value;

/**
 * @brief Detects if T is a CSRMatrix
 */
template <typename T>
struct is_csr_matrix : std::false_type {};

template <typename T>
inline constexpr bool is_csr_matrix_v = is_csr_matrix<T>::value;

/**
 * @brief Detects if T is a SimdVector
 */
template <typename T>
struct is_simd_vector : std::false_type {};

template <typename T>
inline constexpr bool is_simd_vector_v = is_simd_vector<T>::value;

// =============================================================================
// Concurrency Type Traits
// =============================================================================

/**
 * @brief Detects if T is a LockFreeQueue
 */
template <typename T>
struct is_lock_free_queue : std::false_type {};

template <typename T>
inline constexpr bool is_lock_free_queue_v = is_lock_free_queue<T>::value;

/**
 * @brief Detects if T is a LockFreeRingBuffer
 */
template <typename T>
struct is_lock_free_ring_buffer : std::false_type {};

template <typename T>
inline constexpr bool is_lock_free_ring_buffer_v = is_lock_free_ring_buffer<T>::value;

/**
 * @brief Detects if T is a ThreadPool
 */
template <typename T>
struct is_thread_pool : std::false_type {};

template <typename T>
inline constexpr bool is_thread_pool_v = is_thread_pool<T>::value;

/**
 * @brief Detects if T is an AtomicReference
 */
template <typename T>
struct is_atomic_reference : std::false_type {};

template <typename T>
inline constexpr bool is_atomic_reference_v = is_atomic_reference<T>::value;

/**
 * @brief Detects if T is a spinlock policy
 */
template <typename T>
struct is_spinlock_policy : std::false_type {};

template <typename T>
inline constexpr bool is_spinlock_policy_v = is_spinlock_policy<T>::value;

// =============================================================================
// Memory Management Type Traits
// =============================================================================

/**
 * @brief Detects if T is an AlignedVector
 */
template <typename T>
struct is_aligned_vector : std::false_type {};

template <typename T>
inline constexpr bool is_aligned_vector_v = is_aligned_vector<T>::value;

/**
 * @brief Detects if T is an ObjectPool
 */
template <typename T>
struct is_object_pool : std::false_type {};

template <typename T>
inline constexpr bool is_object_pool_v = is_object_pool<T>::value;

/**
 * @brief Detects if T has a NUMA allocator
 */
template <typename T>
struct has_numa_allocator : std::false_type {};

template <typename T>
inline constexpr bool has_numa_allocator_v = has_numa_allocator<T>::value;

// =============================================================================
// Utility Type Traits
// =============================================================================

/**
 * @brief Detects if T is an Expected
 */
template <typename T>
struct is_expected : std::false_type {};

template <typename T>
inline constexpr bool is_expected_v = is_expected<T>::value;

/**
 * @brief Detects if T is a StrongId
 */
template <typename T>
struct is_strong_id : std::false_type {};

template <typename T>
inline constexpr bool is_strong_id_v = is_strong_id<T>::value;

/**
 * @brief Detects if T is a ValueGuard
 */
template <typename T>
struct is_value_guard : std::false_type {};

template <typename T>
inline constexpr bool is_value_guard_v = is_value_guard<T>::value;

/**
 * @brief Detects if T is a ScopeGuard
 */
template <typename T>
struct is_scope_guard : std::false_type {};

template <typename T>
inline constexpr bool is_scope_guard_v = is_scope_guard<T>::value;

// =============================================================================
// Method Detection Traits
// =============================================================================

/**
 * @brief Detects if T has a capacity() method
 */
template <typename T>
struct has_capacity : is_detected<cpp_util_detail::capacity_t, T> {};

template <typename T>
inline constexpr bool has_capacity_v = has_capacity<T>::value;

/**
 * @brief Detects if T has a shrink_to_fit() method
 */
template <typename T>
struct has_shrink_to_fit : is_detected<cpp_util_detail::shrink_to_fit_t, T> {};

template <typename T>
inline constexpr bool has_shrink_to_fit_v = has_shrink_to_fit<T>::value;

/**
 * @brief Detects if T has a get_allocator() method
 */
template <typename T>
struct has_get_allocator : is_detected<cpp_util_detail::get_allocator_t, T> {};

template <typename T>
inline constexpr bool has_get_allocator_v = has_get_allocator<T>::value;

/**
 * @brief Detects if T has a key_type member
 */
template <typename T>
struct has_key_type : is_detected<cpp_util_detail::container_key_type_t, T> {};

template <typename T>
inline constexpr bool has_key_type_v = has_key_type<T>::value;

/**
 * @brief Detects if T has a mapped_type member
 */
template <typename T>
struct has_mapped_type : is_detected<cpp_util_detail::container_mapped_type_t, T> {};

template <typename T>
inline constexpr bool has_mapped_type_v = has_mapped_type<T>::value;

/**
 * @brief Detects if T has lock() method
 */
template <typename T>
struct has_lock : is_detected<cpp_util_detail::lock_t, T> {};

template <typename T>
inline constexpr bool has_lock_v = has_lock<T>::value;

/**
 * @brief Detects if T has unlock() method
 */
template <typename T>
struct has_unlock : is_detected<cpp_util_detail::unlock_t, T> {};

template <typename T>
inline constexpr bool has_unlock_v = has_unlock<T>::value;

/**
 * @brief Detects if T has try_lock() method
 */
template <typename T>
struct has_try_lock : is_detected<cpp_util_detail::try_lock_t, T> {};

template <typename T>
inline constexpr bool has_try_lock_v = has_try_lock<T>::value;

/**
 * @brief Detects if T has binary_serialize() method
 */
template <typename T>
struct has_binary_serialize : is_detected<cpp_util_detail::binary_serialize_t, T> {};

template <typename T>
inline constexpr bool has_binary_serialize_v = has_binary_serialize<T>::value;

/**
 * @brief Detects if T has binary_deserialize() static method
 */
template <typename T>
struct has_binary_deserialize : is_detected<cpp_util_detail::binary_deserialize_t, T> {};

template <typename T>
inline constexpr bool has_binary_deserialize_v = has_binary_deserialize<T>::value;

/**
 * @brief Detects if T has benchmark interface methods
 */
template <typename T>
struct has_benchmark_interface : is_detected<cpp_util_detail::benchmark_interface_t, T> {};

template <typename T>
inline constexpr bool has_benchmark_interface_v = has_benchmark_interface<T>::value;

/**
 * @brief Detects if T supports complete binary serialization
 */
template <typename T>
struct is_binary_serializable {
    static constexpr bool value = has_binary_serialize_v<T> && has_binary_deserialize_v<T>;
};

template <typename T>
inline constexpr bool is_binary_serializable_v = is_binary_serializable<T>::value;

// =============================================================================
// Composite Type Traits
// =============================================================================

/**
 * @brief Detects if T uses small buffer optimization
 */
template <typename T>
struct is_small_buffer_optimized {
    static constexpr bool value = is_small_vector_v<T> || is_flat_map_v<T> || is_flat_set_v<T>;
};

template <typename T>
inline constexpr bool is_small_buffer_optimized_v = is_small_buffer_optimized<T>::value;

/**
 * @brief Detects if T is cache-aware
 */
template <typename T>
struct is_cache_aware_type {
    static constexpr bool value = is_aligned_vector_v<T> || is_simd_vector_v<T>;
};

template <typename T>
inline constexpr bool is_cache_aware_type_v = is_cache_aware_type<T>::value;

/**
 * @brief Detects if T is a concurrent container
 */
template <typename T>
struct is_concurrent_container {
    static constexpr bool value = is_lock_free_queue_v<T> || is_lock_free_ring_buffer_v<T>;
};

template <typename T>
inline constexpr bool is_concurrent_container_v = is_concurrent_container<T>::value;

/**
 * @brief Detects if T is any cpp_utilities container
 */
template <typename T>
struct is_library_container {
    static constexpr bool value = is_small_vector_v<T> || is_circular_buffer_v<T> || 
                                 is_flat_map_v<T> || is_flat_set_v<T> || 
                                 is_sorted_container_v<T> || is_sparse_set_v<T> || 
                                 is_slot_map_v<T> || is_aligned_vector_v<T>;
};

template <typename T>
inline constexpr bool is_library_container_v = is_library_container<T>::value;

/**
 * @brief Detects if T is any tensor-like type
 */
template <typename T>
struct is_tensor_type {
    static constexpr bool value = is_tensor_v<T> || is_fixed_tensor_v<T> || 
                                 is_csr_matrix_v<T> || is_simd_vector_v<T>;
};

template <typename T>
inline constexpr bool is_tensor_type_v = is_tensor_type<T>::value;

/**
 * @brief Detects if T is any guard type
 */
template <typename T>
struct is_guard_type {
    static constexpr bool value = is_value_guard_v<T> || is_scope_guard_v<T>;
};

template <typename T>
inline constexpr bool is_guard_type_v = is_guard_type<T>::value;

// =============================================================================
// Type Extraction Utilities
// =============================================================================

/**
 * @brief Extracts value_type from container T
 */
template <typename T>
using container_value_type_t = detected_t<cpp_util_detail::container_value_type_t, T>;

/**
 * @brief Extracts key_type from container T
 */
template <typename T>
using container_key_type_t = detected_t<cpp_util_detail::container_key_type_t, T>;

/**
 * @brief Extracts mapped_type from container T
 */
template <typename T>
using container_mapped_type_t = detected_t<cpp_util_detail::container_mapped_type_t, T>;

/**
 * @brief Extracts allocator_type from container T
 */
template <typename T>
using allocator_type_t = detected_t<cpp_util_detail::allocator_type_t, T>;

/**
 * @brief Extracts error_type from Expected T
 */
template <typename T>
using error_type_t = detected_t<cpp_util_detail::error_type_t, T>;

/**
 * @brief Extracts tag_type from StrongId T
 */
template <typename T>
using tag_type_t = detected_t<cpp_util_detail::tag_type_t, T>;

// =============================================================================
// Parallel Algorithm Support
// =============================================================================

/**
 * @brief Detects if T is compatible with parallel algorithms
 * @note Uses detected_or to safely check value_type existence before testing copy constructibility
 */
template <typename T, typename = void>
struct is_parallel_algorithm_compatible : std::false_type {};

template <typename T>
struct is_parallel_algorithm_compatible<T, std::enable_if_t<(
    is_iterable_v<T> &&
    is_detected_v<cpp_util_detail::container_value_type_t, T> &&
    std::is_copy_constructible<detected_or<void, cpp_util_detail::container_value_type_t, T>>::value
    )>> : std::true_type {};

template <typename T>
inline constexpr bool is_parallel_algorithm_compatible_v = 
    is_parallel_algorithm_compatible<T>::value;

// =============================================================================
// Duck-Typing Traits
// =============================================================================

/**
 * @brief Detects if T behaves like a SmallVector
 */
template <typename T>
struct is_small_vector_like : is_detected<cpp_util_detail::is_inline_t, T> {};

template <typename T>
inline constexpr bool is_small_vector_like_v = is_small_vector_like<T>::value;

/**
 * @brief Detects if T behaves like an Expected
 */
template <typename T>
struct is_expected_like : 
    std::conjunction<
        is_detected<cpp_util_detail::value_method_t, T>,
        is_detected<cpp_util_detail::error_method_t, T>
    > {};

template <typename T>
inline constexpr bool is_expected_like_v = is_expected_like<T>::value;

/**
 * @brief Detects if T behaves like a Tensor
 */
template <typename T>
struct is_tensor_like : is_detected<cpp_util_detail::shape_t, T> {};

template <typename T>
inline constexpr bool is_tensor_like_v = is_tensor_like<T>::value;

// =============================================================================
// Extension Points
// =============================================================================

namespace extension_points {
    /**
     * @brief Customization point for user-defined traits
     */
    template<typename T>
    struct library_custom_traits {
    };
}

// =============================================================================
// Design by Contract Helpers
// =============================================================================

/**
 * @brief Requires type T to have validate() method
 */
template<typename T>
constexpr void requires_validate() {
    static_assert(has_validate_v<T>,
        "[CONTRACT VIOLATION] Type must have validate() method");
}

/**
 * @brief Requires type T to be an Expected
 */
template<typename T>
constexpr void requires_expected() {
    static_assert(is_expected_v<T>,
        "[CONTRACT VIOLATION] Type must be cpp_utilities::Expected");
}

/**
 * @brief Requires type T to be a Tensor
 */
template<typename T>
constexpr void requires_tensor() {
    static_assert(is_tensor_v<T>,
        "[CONTRACT VIOLATION] Type must be cpp_utilities::Tensor");
}

/**
 * @brief Requires type T to support parallel algorithms
 */
template<typename T>
constexpr void requires_parallel_compatible() {
    static_assert(is_parallel_algorithm_compatible_v<T>,
        "[CONTRACT VIOLATION] Type must support parallel algorithms");
}

/**
 * @brief Requires type T to be binary serializable
 */
template<typename T>
constexpr void requires_binary_serializable() {
    static_assert(is_binary_serializable_v<T>,
        "[CONTRACT VIOLATION] Type must support binary serialization");
}

/**
 * @brief Requires type T to be a cpp_utilities container
 */
template<typename T>
constexpr void requires_library_container() {
    static_assert(is_library_container_v<T>,
        "[CONTRACT VIOLATION] Type must be a cpp_utilities container");
}

/**
 * @brief Requires type T to be a concurrent container
 */
template<typename T>
constexpr void requires_concurrent_container() {
    static_assert(is_concurrent_container_v<T>,
        "[CONTRACT VIOLATION] Type must be a concurrent container");
}

/**
 * @brief Requires type T to be tensor-like
 */
template<typename T>
constexpr void requires_tensor_type() {
    static_assert(is_tensor_type_v<T>,
        "[CONTRACT VIOLATION] Type must be a tensor type");
}

// =============================================================================
// Diagnostic Utilities
// =============================================================================

/**
 * @brief Diagnostic information for Expected types
 * @tparam T Type to diagnose
 * @return Compile-time string describing whether T is Expected-like
 * @note Zero runtime overhead - returns compile-time string literal
 */
template <typename T>
constexpr const char* diagnose_expected() {
    return 
        is_expected_v<T> ? "Type is specialized as Expected" :
        !is_detected_v<cpp_util_detail::value_method_t, T> ? "Missing value() method" :
        !is_detected_v<cpp_util_detail::error_method_t, T> ? "Missing error() method" :
        !is_detected_v<cpp_util_detail::error_type_t, T> ? "Missing error_type typedef" :
        "Type satisfies Expected requirements but is not specialized";
}

/**
 * @brief Diagnostic information for Tensor types
 * @tparam T Type to diagnose
 * @return Compile-time string describing whether T is Tensor-like
 * @note Zero runtime overhead - returns compile-time string literal
 */
template <typename T>
constexpr const char* diagnose_tensor() {
    return 
        is_tensor_v<T> ? "Type is specialized as Tensor" :
        is_fixed_tensor_v<T> ? "Type is specialized as FixedTensor" :
        is_csr_matrix_v<T> ? "Type is specialized as CSRMatrix" :
        is_simd_vector_v<T> ? "Type is specialized as SimdVector" :
        !is_detected_v<cpp_util_detail::shape_t, T> ? "Missing shape() method" :
        "Type satisfies Tensor requirements but is not specialized";
}

/**
 * @brief Diagnostic information for binary serialization
 * @tparam T Type to diagnose
 * @return Compile-time string describing serialization capabilities
 * @note Zero runtime overhead - returns compile-time string literal
 */
template <typename T>
constexpr const char* diagnose_binary_serializable() {
    return 
        is_binary_serializable_v<T> ? "Type is fully binary serializable" :
        !has_binary_serialize_v<T> ? "Missing binary_serialize() method" :
        !has_binary_deserialize_v<T> ? "Missing binary_deserialize() static method" :
        "Type satisfies serialization requirements";
}

/**
 * @brief Diagnostic information for container types
 * @tparam T Type to diagnose
 * @return Compile-time string describing container classification
 * @note Zero runtime overhead - returns compile-time string literal
 */
template <typename T>
constexpr const char* diagnose_library_container() {
    return 
        is_small_vector_v<T> ? "Type is SmallVector" :
        is_circular_buffer_v<T> ? "Type is CircularBuffer" :
        is_flat_map_v<T> ? "Type is FlatMap" :
        is_flat_set_v<T> ? "Type is FlatSet" :
        is_sparse_set_v<T> ? "Type is SparseSet" :
        is_slot_map_v<T> ? "Type is SlotMap" :
        is_concurrent_container_v<T> ? "Type is a concurrent container" :
        is_small_buffer_optimized_v<T> ? "Type uses small buffer optimization" :
        "Type is not a library container";
}

/**
 * @brief Provides explanation why T is not an Expected type
 * @tparam T Type to analyze
 */
template <typename T>
struct why_not_expected {
    static constexpr const char* reason =
        !is_detected_v<cpp_util_detail::value_method_t, T> ? "Missing value() method" :
        !is_detected_v<cpp_util_detail::error_method_t, T> ? "Missing error() method" :
        !is_detected_v<cpp_util_detail::error_type_t, T> ? "Missing error_type typedef" :
        "Type satisfies Expected requirements but is not specialized as Expected";
};

/**
 * @brief Provides explanation why T is not a Tensor type
 * @tparam T Type to analyze
 */
template <typename T>
struct why_not_tensor {
    static constexpr const char* reason =
        !is_detected_v<cpp_util_detail::shape_t, T> ? "Missing shape() method" :
        "Type satisfies Tensor requirements but is not specialized as Tensor";
};

/**
 * @brief Provides explanation why T is not binary serializable
 * @tparam T Type to analyze
 */
template <typename T>
struct why_not_binary_serializable {
    static constexpr const char* reason =
        !has_binary_serialize_v<T> ? "Missing binary_serialize() method" :
        !has_binary_deserialize_v<T> ? "Missing static binary_deserialize() method" :
        "Type satisfies both requirements - this should not appear";
};

// =============================================================================
// C++20 Concepts
// =============================================================================

#if FATP_HAS_CPP20

/**
 * @brief C++20 concepts for library-specific type traits
 * 
 * @details Concepts are in the fat_p::concepts namespace to avoid naming conflicts
 * with forward-declared class templates. Use PascalCase names to distinguish from
 * struct-based traits.
 */
namespace concepts {

/**
 * @brief Concept for SmallVector types
 */
template <typename T>
concept SmallVectorType = is_small_vector_v<T>;

/**
 * @brief Concept for CircularBuffer types
 */
template <typename T>
concept CircularBufferType = is_circular_buffer_v<T>;

/**
 * @brief Concept for FlatMap types
 */
template <typename T>
concept FlatMapType = is_flat_map_v<T>;

/**
 * @brief Concept for FlatSet types
 */
template <typename T>
concept FlatSetType = is_flat_set_v<T>;

/**
 * @brief Concept for Tensor types
 */
template <typename T>
concept TensorType = is_tensor_v<T>;

/**
 * @brief Concept for FixedTensor types
 */
template <typename T>
concept FixedTensorType = is_fixed_tensor_v<T>;

/**
 * @brief Concept for Expected types
 */
template <typename T>
concept ExpectedType = is_expected_v<T>;

/**
 * @brief Concept for StrongId types
 */
template <typename T>
concept StrongIdType = is_strong_id_v<T>;

/**
 * @brief Concept for binary serializable types
 */
template <typename T>
concept BinarySerializable = is_binary_serializable_v<T>;

/**
 * @brief Concept for parallel algorithm compatible types
 */
template <typename T>
concept ParallelCompatible = is_parallel_algorithm_compatible_v<T>;

/**
 * @brief Concept for library container types
 */
template <typename T>
concept LibraryContainer = is_library_container_v<T>;

/**
 * @brief Concept for tensor-like types (duck-typed)
 */
template <typename T>
concept TensorLikeType = is_tensor_type_v<T>;

/**
 * @brief Concept for concurrent container types
 */
template <typename T>
concept ConcurrentContainer = is_concurrent_container_v<T>;

} // namespace concepts

#endif

} // namespace fat_p
