#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPConcepts
  file_role: public_header
  path: include/fat_p/FatPConcepts.h
  namespace: fat_p::concepts
  layer: Foundation
  summary: Fat-P library-specific C++20 concepts.
  api_stability: stable
  related:
    docs_search: "FatPConcepts"
    tests:
      - components/Concepts/tests/test_FatPConcepts.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file FatPConcepts.h
 * @brief Fat-P library-specific C++20 concepts.
 *
 * @details Provides C++20 concepts for detecting and constraining Fat-P library types.
 *
 * All concepts use snake_case naming for STL compatibility.
 *
 * @section architecture Architecture
 * - Forward declarations only (no component includes)
 * - Concept definitions use requires expressions
 * - Zero runtime overhead (compile-time only)
 *
 * @section categories Concept Categories
 *
 * @subsection container_concepts Container Concepts
 * - `small_vector_type<T>` - Is a SmallVector
 * - `circular_buffer_type<T>` - Is a CircularBuffer
 * - `flat_map_type<T>` - Is a FlatMap
 * - `flat_set_type<T>` - Is a FlatSet
 * - `sorted_container_type<T>` - Is a SortedContainer
 * - `sparse_set_type<T>` - Is a SparseSet
 * - `slot_map_type<T>` - Is a SlotMap
 * - `aligned_vector_type<T>` - Is an AlignedVector
 *
 * @subsection tensor_concepts Tensor Concepts
 * - `tensor_type<T>` - Is a Tensor
 * - `fixed_tensor_type<T>` - Is a FixedTensor
 * - `csr_matrix_type<T>` - Is a CSRMatrix
 * - `simd_vector_type<T>` - Is a SimdVector
 * - `tensor_like<T>` - Has shape() method (duck-typed)
 *
 * @subsection concurrency_concepts Concurrency Concepts
 * - `lock_free_queue_type<T>` - Is a LockFreeQueue
 * - `lock_free_ring_buffer_type<T>` - Is a LockFreeRingBuffer
 * - `thread_pool_type<T>` - Is a ThreadPool
 * - `atomic_reference_type<T>` - Is an AtomicReference
 * - `concurrent_container<T>` - Any concurrent container
 * - `lockable<T>` - Has lock/unlock methods
 *
 * @subsection utility_concepts Utility Concepts
 * - `expected_type<T>` - Is an Expected
 * - `strong_id_type<T>` - Is a StrongId
 * - `value_guard_type<T>` - Is a ValueGuard
 * - `scope_guard_type<T>` - Is a ScopeGuard
 * - `guard_type<T>` - Any guard type
 *
 * @subsection composite_concepts Composite Concepts
 * - `library_container<T>` - Any Fat-P container
 * - `small_buffer_optimized<T>` - Uses SBO
 * - `cache_aware<T>` - Cache-aligned types
 * - `binary_serializable<T>` - Has binary serialize/deserialize
 * - `parallel_compatible<T>` - Safe for parallel algorithms
 *
 * @note All concepts have zero runtime overhead (compile-time onl */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>
#include <vector>

#include "CppFeatureDetection.h"

namespace fat_p
{

// =============================================================================
// Forward Declarations
// =============================================================================
// These must match the actual class template signatures in the library.

template <typename T, std::size_t InlineCapacity, typename Allocator>
class SmallVector;

template <typename T, std::size_t Capacity>
class CircularBuffer;

template <typename Key, typename Value, typename Compare, typename Allocator>
class FlatMap;

template <typename Key, typename Compare, typename Allocator>
class FlatSet;

template <typename T,
          typename UniquenessPolicy,
          typename ComparePolicy,
          typename Allocator,
          typename ConcurrencyPolicy,
          template <typename, typename>
          class BackendPolicy>
class SortedContainer;

template <typename T>
class SparseSet;

template <typename T, typename Data>
class SparseSetWithData;

template <typename T, typename Allocator>
class SlotMap;

template <typename T, typename Allocator, typename IteratorPolicy, typename ConcurrencyPolicy>
class Tensor;

template <typename T, std::size_t... Dims>
class FixedTensor;

template <typename T, typename IndexType>
class CSRMatrix;

template <typename T>
class SimdVector;

template <typename T, std::size_t MaxSize, bool EnableStats>
class LockFreeQueue;

template <typename T>
class LockFreeRingBuffer;

template <typename T>
class LockFreeRingBufferMPMC;

class ThreadPool;

template <typename T, typename EnforcementPolicy, template <typename> class WaitPolicy, typename DurationPolicy>
class AtomicReference;

template <typename T, std::size_t Alignment>
class AlignedVector;

template <typename T, typename SyncPolicy>
class ObjectPool;

template <typename T>
class NumaAllocator;

namespace work_queue
{
struct DefaultRoutingPolicy;
struct DefaultBackoffPolicy;
template <typename T,
          std::size_t ShardCount,
          std::size_t ShardCapacity,
          typename RoutingPolicy,
          typename BackoffPolicy>
class WorkQueue;
} // namespace work_queue

template <typename T, typename TopologyPolicy>
class PolicyQueue;

template <typename T, typename E, template <typename, typename> class StoragePolicy>
class ExpectedImpl;

template <typename T, typename Tag, typename CheckPolicy, template <typename> class OpPolicy>
class StrongId;

template <typename T, typename Policy>
class ValueGuard;

template <typename F, typename ThrowingPolicy, template <typename> class ActionPolicy>
class ScopeGuard;

template <typename F>
class ScopeGuardOnFail;

template <typename F>
class ScopeGuardOnSuccess;

namespace concepts
{

// =============================================================================
// Implementation Detail: Type Detection Helpers
// =============================================================================

namespace detail
{

// SmallVector detection
template <typename T>
struct is_small_vector_impl : std::false_type
{
};

template <typename T, std::size_t N, typename A>
struct is_small_vector_impl<SmallVector<T, N, A>> : std::true_type
{
};

// CircularBuffer detection
template <typename T>
struct is_circular_buffer_impl : std::false_type
{
};

template <typename T, std::size_t N>
struct is_circular_buffer_impl<CircularBuffer<T, N>> : std::true_type
{
};

// FlatMap detection
template <typename T>
struct is_flat_map_impl : std::false_type
{
};

template <typename K, typename V, typename C, typename A>
struct is_flat_map_impl<FlatMap<K, V, C, A>> : std::true_type
{
};

// FlatSet detection
template <typename T>
struct is_flat_set_impl : std::false_type
{
};

template <typename K, typename C, typename A>
struct is_flat_set_impl<FlatSet<K, C, A>> : std::true_type
{
};

// SortedContainer detection
template <typename T>
struct is_sorted_container_impl : std::false_type
{
};

template <typename T, typename U, typename C, typename A, typename P, template <typename, typename> class B>
struct is_sorted_container_impl<SortedContainer<T, U, C, A, P, B>> : std::true_type
{
};

// SparseSet detection
template <typename T>
struct is_sparse_set_impl : std::false_type
{
};

template <typename T>
struct is_sparse_set_impl<SparseSet<T>> : std::true_type
{
};

template <typename T, typename Data>
struct is_sparse_set_impl<SparseSetWithData<T, Data>> : std::true_type
{
};

// SlotMap detection
template <typename T>
struct is_slot_map_impl : std::false_type
{
};

template <typename T, typename A>
struct is_slot_map_impl<SlotMap<T, A>> : std::true_type
{
};

// Tensor detection
template <typename T>
struct is_tensor_impl : std::false_type
{
};

template <typename T, typename A, typename I, typename C>
struct is_tensor_impl<Tensor<T, A, I, C>> : std::true_type
{
};

// FixedTensor detection
template <typename T>
struct is_fixed_tensor_impl : std::false_type
{
};

template <typename T, std::size_t... Dims>
struct is_fixed_tensor_impl<FixedTensor<T, Dims...>> : std::true_type
{
};

// CSRMatrix detection
template <typename T>
struct is_csr_matrix_impl : std::false_type
{
};

template <typename T, typename I>
struct is_csr_matrix_impl<CSRMatrix<T, I>> : std::true_type
{
};

// SimdVector detection
template <typename T>
struct is_simd_vector_impl : std::false_type
{
};

template <typename T>
struct is_simd_vector_impl<SimdVector<T>> : std::true_type
{
};

// LockFreeQueue detection
template <typename T>
struct is_lock_free_queue_impl : std::false_type
{
};

template <typename T, std::size_t M, bool S>
struct is_lock_free_queue_impl<LockFreeQueue<T, M, S>> : std::true_type
{
};

// WorkQueue detection (also a lock-free queue)
template <typename T, std::size_t SC, std::size_t SCap, typename RP, typename BP>
struct is_lock_free_queue_impl<work_queue::WorkQueue<T, SC, SCap, RP, BP>> : std::true_type
{
};

// PolicyQueue detection (also a lock-free queue)
template <typename T, typename TP>
struct is_lock_free_queue_impl<PolicyQueue<T, TP>> : std::true_type
{
};

// LockFreeRingBuffer detection
template <typename T>
struct is_lock_free_ring_buffer_impl : std::false_type
{
};

template <typename T>
struct is_lock_free_ring_buffer_impl<LockFreeRingBuffer<T>> : std::true_type
{
};

template <typename T>
struct is_lock_free_ring_buffer_impl<LockFreeRingBufferMPMC<T>> : std::true_type
{
};

// ThreadPool detection
template <typename T>
struct is_thread_pool_impl : std::false_type
{
};

template <>
struct is_thread_pool_impl<ThreadPool> : std::true_type
{
};

// AtomicReference detection
template <typename T>
struct is_atomic_reference_impl : std::false_type
{
};

template <typename T, typename E, template <typename> class W, typename D>
struct is_atomic_reference_impl<AtomicReference<T, E, W, D>> : std::true_type
{
};

// AlignedVector detection
template <typename T>
struct is_aligned_vector_impl : std::false_type
{
};

template <typename T, std::size_t A>
struct is_aligned_vector_impl<AlignedVector<T, A>> : std::true_type
{
};

// ObjectPool detection
template <typename T>
struct is_object_pool_impl : std::false_type
{
};

template <typename T, typename S>
struct is_object_pool_impl<ObjectPool<T, S>> : std::true_type
{
};

// Expected detection
template <typename T>
struct is_expected_impl : std::false_type
{
};

template <typename T, typename E, template <typename, typename> class S>
struct is_expected_impl<ExpectedImpl<T, E, S>> : std::true_type
{
};

// StrongId detection
template <typename T>
struct is_strong_id_impl : std::false_type
{
};

template <typename T, typename Tag, typename C, template <typename> class O>
struct is_strong_id_impl<StrongId<T, Tag, C, O>> : std::true_type
{
};

// ValueGuard detection
template <typename T>
struct is_value_guard_impl : std::false_type
{
};

template <typename T, typename P>
struct is_value_guard_impl<ValueGuard<T, P>> : std::true_type
{
};

// ScopeGuard detection
template <typename T>
struct is_scope_guard_impl : std::false_type
{
};

template <typename F, typename T, template <typename> class A>
struct is_scope_guard_impl<ScopeGuard<F, T, A>> : std::true_type
{
};

template <typename F>
struct is_scope_guard_impl<ScopeGuardOnFail<F>> : std::true_type
{
};

template <typename F>
struct is_scope_guard_impl<ScopeGuardOnSuccess<F>> : std::true_type
{
};

// NumaAllocator detection
template <typename T>
struct is_numa_allocator_impl : std::false_type
{
};

template <typename T>
struct is_numa_allocator_impl<NumaAllocator<T>> : std::true_type
{
};

} // namespace detail

// =============================================================================
// Container Concepts
// =============================================================================

/**
 * @brief Checks if T is a SmallVector.
 * @tparam T The type to check
 */
template <typename T>
concept small_vector_type = detail::is_small_vector_impl<T>::value;

/**
 * @brief Checks if T is a CircularBuffer.
 * @tparam T The type to check
 */
template <typename T>
concept circular_buffer_type = detail::is_circular_buffer_impl<T>::value;

/**
 * @brief Checks if T is a FlatMap.
 * @tparam T The type to check
 */
template <typename T>
concept flat_map_type = detail::is_flat_map_impl<T>::value;

/**
 * @brief Checks if T is a FlatSet.
 * @tparam T The type to check
 */
template <typename T>
concept flat_set_type = detail::is_flat_set_impl<T>::value;

/**
 * @brief Checks if T is a SortedContainer.
 * @tparam T The type to check
 */
template <typename T>
concept sorted_container_type = detail::is_sorted_container_impl<T>::value;

/**
 * @brief Checks if T is a SparseSet.
 * @tparam T The type to check
 */
template <typename T>
concept sparse_set_type = detail::is_sparse_set_impl<T>::value;

/**
 * @brief Checks if T is a SlotMap.
 * @tparam T The type to check
 */
template <typename T>
concept slot_map_type = detail::is_slot_map_impl<T>::value;

/**
 * @brief Checks if T is an AlignedVector.
 * @tparam T The type to check
 */
template <typename T>
concept aligned_vector_type = detail::is_aligned_vector_impl<T>::value;

/**
 * @brief Checks if T is an ObjectPool.
 * @tparam T The type to check
 */
template <typename T>
concept object_pool_type = detail::is_object_pool_impl<T>::value;

// =============================================================================
// Tensor Concepts
// =============================================================================

/**
 * @brief Checks if T is a Tensor.
 * @tparam T The type to check
 */
template <typename T>
concept tensor_type = detail::is_tensor_impl<T>::value;

/**
 * @brief Checks if T is a FixedTensor.
 * @tparam T The type to check
 */
template <typename T>
concept fixed_tensor_type = detail::is_fixed_tensor_impl<T>::value;

/**
 * @brief Checks if T is a CSRMatrix.
 * @tparam T The type to check
 */
template <typename T>
concept csr_matrix_type = detail::is_csr_matrix_impl<T>::value;

/**
 * @brief Checks if T is a SimdVector.
 * @tparam T The type to check
 */
template <typename T>
concept simd_vector_type = detail::is_simd_vector_impl<T>::value;

/**
 * @brief Checks if T behaves like a tensor (has shape() method).
 * @tparam T The type to check
 */
template <typename T>
concept tensor_like = requires(const T& val) {
    { val.shape() };
};

/**
 * @brief Checks if T is any tensor-family type.
 * @tparam T The type to check
 */
template <typename T>
concept any_tensor_type = tensor_type<T> || fixed_tensor_type<T> || csr_matrix_type<T> || simd_vector_type<T>;

// =============================================================================
// Concurrency Concepts
// =============================================================================

/**
 * @brief Checks if T is a LockFreeQueue.
 * @tparam T The type to check
 */
template <typename T>
concept lock_free_queue_type = detail::is_lock_free_queue_impl<T>::value;

/**
 * @brief Checks if T is a LockFreeRingBuffer.
 * @tparam T The type to check
 */
template <typename T>
concept lock_free_ring_buffer_type = detail::is_lock_free_ring_buffer_impl<T>::value;

/**
 * @brief Checks if T is a ThreadPool.
 * @tparam T The type to check
 */
template <typename T>
concept thread_pool_type = detail::is_thread_pool_impl<T>::value;

/**
 * @brief Checks if T is an AtomicReference.
 * @tparam T The type to check
 */
template <typename T>
concept atomic_reference_type = detail::is_atomic_reference_impl<T>::value;

/**
 * @brief Checks if T is any concurrent container.
 * @tparam T The type to check
 */
template <typename T>
concept concurrent_container = lock_free_queue_type<T> || lock_free_ring_buffer_type<T>;

/**
 * @brief Checks if T has lock/unlock/try_lock methods.
 * @tparam T The type to check
 */
template <typename T>
concept lockable = requires(T& val) {
    { val.lock() };
    { val.unlock() };
    { val.try_lock() } -> std::convertible_to<bool>;
};

// =============================================================================
// Utility Concepts
// =============================================================================

/**
 * @brief Checks if T is an Expected.
 * @tparam T The type to check
 */
template <typename T>
concept expected_type = detail::is_expected_impl<T>::value;

/**
 * @brief Checks if T behaves like an Expected (has value() and error() methods).
 * @tparam T The type to check
 */
template <typename T>
concept expected_like = requires(T& val) {
    { val.value() };
    { val.error() };
};

/**
 * @brief Checks if T is a StrongId.
 * @tparam T The type to check
 */
template <typename T>
concept strong_id_type = detail::is_strong_id_impl<T>::value;

/**
 * @brief Checks if T is a ValueGuard.
 * @tparam T The type to check
 */
template <typename T>
concept value_guard_type = detail::is_value_guard_impl<T>::value;

/**
 * @brief Checks if T is a ScopeGuard.
 * @tparam T The type to check
 */
template <typename T>
concept scope_guard_type = detail::is_scope_guard_impl<T>::value;

/**
 * @brief Checks if T is any guard type.
 * @tparam T The type to check
 */
template <typename T>
concept guard_type = value_guard_type<T> || scope_guard_type<T>;

/**
 * @brief Checks if T is a NumaAllocator.
 * @tparam T The type to check
 */
template <typename T>
concept numa_allocator_type = detail::is_numa_allocator_impl<T>::value;

// =============================================================================
// Composite Concepts
// =============================================================================

/**
 * @brief Checks if T is any Fat-P library container.
 * @tparam T The type to check
 */
template <typename T>
concept library_container = small_vector_type<T> || circular_buffer_type<T> || flat_map_type<T> || flat_set_type<T> ||
                            sorted_container_type<T> || sparse_set_type<T> || slot_map_type<T> || aligned_vector_type<T>;

/**
 * @brief Checks if T uses small buffer optimization.
 * @tparam T The type to check
 */
template <typename T>
concept small_buffer_optimized = small_vector_type<T> || flat_map_type<T> || flat_set_type<T>;

/**
 * @brief Checks if T is cache-aware (aligned).
 * @tparam T The type to check
 */
template <typename T>
concept cache_aware = aligned_vector_type<T> || simd_vector_type<T>;

/**
 * @brief Checks if T behaves like a SmallVector (has is_inline() method).
 * @tparam T The type to check
 */
template <typename T>
concept small_vector_like = requires(const T& val) {
    { val.is_inline() } -> std::convertible_to<bool>;
};

// =============================================================================
// Serialization Concepts
// =============================================================================

/**
 * @brief Checks if T has binary_serialize() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_binary_serialize = requires(T& val, std::vector<std::uint8_t>& buf) {
    { val.binary_serialize(buf) };
};

/**
 * @brief Checks if T has static binary_deserialize() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_binary_deserialize = requires(const std::vector<std::uint8_t>& buf) {
    { T::binary_deserialize(buf) };
};

/**
 * @brief Checks if T supports binary serialization.
 * @tparam T The type to check
 */
template <typename T>
concept binary_serializable = has_binary_serialize<T> && has_binary_deserialize<T>;

// =============================================================================
// Method Detection Concepts
// =============================================================================

/**
 * @brief Checks if T has capacity() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_capacity = requires(const T& val) {
    { val.capacity() };
};

/**
 * @brief Checks if T has shrink_to_fit() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_shrink_to_fit = requires(T& val) {
    { val.shrink_to_fit() };
};

/**
 * @brief Checks if T has get_allocator() method.
 * @tparam T The type to check
 */
template <typename T>
concept has_get_allocator = requires(const T& val) {
    { val.get_allocator() };
};

/**
 * @brief Checks if T has key_type member.
 * @tparam T The type to check
 */
template <typename T>
concept has_key_type = requires {
    typename T::key_type;
};

/**
 * @brief Checks if T has mapped_type member.
 * @tparam T The type to check
 */
template <typename T>
concept has_mapped_type = requires {
    typename T::mapped_type;
};

/**
 * @brief Checks if T has error_type member.
 * @tparam T The type to check
 */
template <typename T>
concept has_error_type = requires {
    typename T::error_type;
};

/**
 * @brief Checks if T has tag_type member (for StrongId).
 * @tparam T The type to check
 */
template <typename T>
concept has_tag_type = requires {
    typename T::tag_type;
};

/**
 * @brief Checks if T has benchmark interface methods.
 * @tparam T The type to check
 */
template <typename T>
concept has_benchmark_interface = requires(T& val) {
    { val.benchmark_setup() };
    { val.benchmark_run() };
    { val.benchmark_teardown() };
};

// =============================================================================
// Parallel Algorithm Concepts
// =============================================================================

/**
 * @brief Checks if T is compatible with parallel algorithms.
 * @tparam T The type to check
 *
 * @details Requires T to be iterable with a copy-constructible value_type.
 */
template <typename T>
concept parallel_compatible = requires {
    typename T::value_type;
    requires std::ranges::range<T>;
    requires std::copy_constructible<typename T::value_type>;
};

} // namespace concepts
} // namespace fat_p
