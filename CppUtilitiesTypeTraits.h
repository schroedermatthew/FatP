#pragma once

#include "TypeTraits.h"
#include <type_traits>
#include <vector>
#include <cstdint>

namespace cpp_utilities {

namespace cpp_util_detail {
    template <typename T>
    using container_value_type_t = typename T::value_type;
}

template <typename T>
struct is_small_vector : std::false_type {};

template <typename T>
inline constexpr bool is_small_vector_v = is_small_vector<T>::value;

template <typename T>
struct is_circular_buffer : std::false_type {};

template <typename T>
inline constexpr bool is_circular_buffer_v = is_circular_buffer<T>::value;

template <typename T>
struct is_flat_map : std::false_type {};

template <typename T>
inline constexpr bool is_flat_map_v = is_flat_map<T>::value;

template <typename T>
struct is_flat_set : std::false_type {};

template <typename T>
inline constexpr bool is_flat_set_v = is_flat_set<T>::value;

template <typename T>
struct is_sorted_container : std::false_type {};

template <typename T>
inline constexpr bool is_sorted_container_v = is_sorted_container<T>::value;

template <typename T>
struct is_sparse_set : std::false_type {};

template <typename T>
inline constexpr bool is_sparse_set_v = is_sparse_set<T>::value;

template <typename T>
struct is_slot_map : std::false_type {};

template <typename T>
inline constexpr bool is_slot_map_v = is_slot_map<T>::value;

template <typename T>
struct is_tensor : std::false_type {};

template <typename T>
inline constexpr bool is_tensor_v = is_tensor<T>::value;

template <typename T>
struct is_fixed_tensor : std::false_type {};

template <typename T>
inline constexpr bool is_fixed_tensor_v = is_fixed_tensor<T>::value;

template <typename T>
struct is_csr_matrix : std::false_type {};

template <typename T>
inline constexpr bool is_csr_matrix_v = is_csr_matrix<T>::value;

template <typename T>
struct is_simd_vector : std::false_type {};

template <typename T>
inline constexpr bool is_simd_vector_v = is_simd_vector<T>::value;

template <typename T>
struct is_lock_free_queue : std::false_type {};

template <typename T>
inline constexpr bool is_lock_free_queue_v = is_lock_free_queue<T>::value;

template <typename T>
struct is_lock_free_ring_buffer : std::false_type {};

template <typename T>
inline constexpr bool is_lock_free_ring_buffer_v = is_lock_free_ring_buffer<T>::value;

template <typename T>
struct is_thread_pool : std::false_type {};

template <typename T>
inline constexpr bool is_thread_pool_v = is_thread_pool<T>::value;

template <typename T>
struct is_atomic_reference : std::false_type {};

template <typename T>
inline constexpr bool is_atomic_reference_v = is_atomic_reference<T>::value;

template <typename T>
struct is_spinlock_policy : std::false_type {};

template <typename T>
inline constexpr bool is_spinlock_policy_v = is_spinlock_policy<T>::value;

template <typename T>
struct is_aligned_vector : std::false_type {};

template <typename T>
inline constexpr bool is_aligned_vector_v = is_aligned_vector<T>::value;

template <typename T>
struct is_object_pool : std::false_type {};

template <typename T>
inline constexpr bool is_object_pool_v = is_object_pool<T>::value;

template <typename T>
struct has_numa_allocator : std::false_type {};

template <typename T>
inline constexpr bool has_numa_allocator_v = has_numa_allocator<T>::value;

template <typename T>
struct is_small_buffer_optimized {
    static constexpr bool value = is_small_vector_v<T> || is_flat_map_v<T> || is_flat_set_v<T>;
};

template <typename T>
inline constexpr bool is_small_buffer_optimized_v = is_small_buffer_optimized<T>::value;

template <typename T>
struct is_expected : std::false_type {};

template <typename T>
inline constexpr bool is_expected_v = is_expected<T>::value;

template <typename T>
struct is_strong_id : std::false_type {};

template <typename T>
inline constexpr bool is_strong_id_v = is_strong_id<T>::value;

template <typename T>
struct is_value_guard : std::false_type {};

template <typename T>
inline constexpr bool is_value_guard_v = is_value_guard<T>::value;

template <typename T>
struct is_scope_guard : std::false_type {};

template <typename T>
inline constexpr bool is_scope_guard_v = is_scope_guard<T>::value;

namespace cpp_util_detail {
    template <typename T>
    using binary_serialize_t = decltype(std::declval<T&>().binary_serialize(
        std::declval<std::vector<uint8_t>&>()));
    
    template <typename T>
    using binary_deserialize_t = decltype(T::binary_deserialize(
        std::declval<const std::vector<uint8_t>&>()));
}

template <typename T>
struct has_binary_serialize : is_detected<cpp_util_detail::binary_serialize_t, T> {};

template <typename T>
inline constexpr bool has_binary_serialize_v = has_binary_serialize<T>::value;

template <typename T>
struct has_binary_deserialize : is_detected<cpp_util_detail::binary_deserialize_t, T> {};

template <typename T>
inline constexpr bool has_binary_deserialize_v = has_binary_deserialize<T>::value;

template <typename T>
struct is_binary_serializable {
    static constexpr bool value = has_binary_serialize_v<T> && has_binary_deserialize_v<T>;
};

template <typename T>
inline constexpr bool is_binary_serializable_v = is_binary_serializable<T>::value;

template <typename T, typename = void>
struct is_parallel_algorithm_compatible : std::false_type {};

template <typename T>
struct is_parallel_algorithm_compatible<T, std::enable_if_t<(
    is_iterable_v<T>&&
    is_detected_v<cpp_util_detail::container_value_type_t, T>&&
    std::is_copy_constructible<detected_t<cpp_util_detail::container_value_type_t, T>>::value
    )>> : std::true_type {};

template <typename T>
inline constexpr bool is_parallel_algorithm_compatible_v = 
    is_parallel_algorithm_compatible<T>::value;

template <typename T>
struct is_cache_aware_type {
    static constexpr bool value = is_aligned_vector_v<T> || is_simd_vector_v<T>;
};

template <typename T>
inline constexpr bool is_cache_aware_type_v = is_cache_aware_type<T>::value;

namespace cpp_util_detail {
    template <typename T>
    using benchmark_interface_t = decltype(
        std::declval<T&>().benchmark_setup(),
        std::declval<T&>().benchmark_run(),
        std::declval<T&>().benchmark_teardown()
    );
}

template <typename T>
struct has_benchmark_interface : is_detected<cpp_util_detail::benchmark_interface_t, T> {};

template <typename T>
inline constexpr bool has_benchmark_interface_v = has_benchmark_interface<T>::value;

namespace extension_points {
    template<typename T>
    struct library_custom_traits {
    };
}

template<typename T>
constexpr void requires_validate() {
    static_assert(has_validate_v<T>,
        "[CONTRACT VIOLATION] Type must have validate() method");
}

template<typename T>
constexpr void requires_expected() {
    static_assert(is_expected_v<T>,
        "[CONTRACT VIOLATION] Type must be cpp_utilities::Expected");
}

template<typename T>
constexpr void requires_tensor() {
    static_assert(is_tensor_v<T>,
        "[CONTRACT VIOLATION] Type must be cpp_utilities::Tensor");
}

template<typename T>
constexpr void requires_parallel_compatible() {
    static_assert(is_parallel_algorithm_compatible_v<T>,
        "[CONTRACT VIOLATION] Type must support parallel algorithms");
}

}