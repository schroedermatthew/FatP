/**
 * @file NumaAlignedAllocator.h
 * @brief Combined NUMA-aware + cache-aligned allocator for HPC workloads
 *
 *
 * @layer Domain
 *
 * @version 1.2
 *
 * Version History:
 * - 1.2: Refactored to leverage NumaAllocator.h infrastructure (no duplication)
 * - 1.1: Fixed P0 bugs: recursive libnuma calls, fallback deallocation mismatch
 * - 1.0: Initial release
 *
 * @details This allocator extends the existing NumaAllocator infrastructure with
 * explicit alignment guarantees. It solves the problem that NumaAllocator's
 * fallback path uses malloc() which is NOT guaranteed to be 64-byte aligned.
 *
 * Architecture:
 * - Leverages NumaAllocator.h for: NumaInfo, policies, platform detection
 * - Adds: Strict NUMA mode (no fallback mixing), alignment guarantees
 *
 * Allocation Strategy:
 * - NUMA available: Use NUMA APIs (page-aligned >= 4KB, satisfies alignment)
 *   - If NUMA allocation fails: throw bad_alloc (no fallback to prevent UB)
 * - NUMA unavailable: Use explicit aligned allocation
 *
 * Key Features:
 * - NUMA locality on multi-socket systems
 * - Guaranteed alignment (default 64 bytes for cache lines)
 * - Correct deallocation (no mixing NUMA and aligned paths)
 * - STL allocator interface
 * - Policy-based NUMA node selection (reuses NumaAllocator policies)
 *
 * @note Page alignment (4KB) always satisfies cache alignment (64B):
 *       4096 % 64 == 0, so NUMA allocations are implicitly SIMD-aligned.
 *
 * @see NumaAllocator.h for NUMA infrastructure and policies
 * @see HpcVector.h for the recommended HPC container
 *
 * Requires: C++17, NumaAllocator.h
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: NumaAlignedAllocator
  file_role: public_header
  path: fat_p/NumaAlignedAllocator.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for NumaAlignedAllocator."
  api_stability: in_work
  related:
    docs_search: "NumaAlignedAllocator"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "NumaAllocator.h" // Provides NumaInfo, policies, platform detection

#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>

namespace fat_p
{
namespace memory
{

// =============================================================================
// NUMA Allocation Wrappers (with :: qualification to prevent recursion)
// =============================================================================

namespace detail
{

/**
 * @brief NUMA-aware allocation on specific node
 * @note Uses global :: qualification to call libnuma, preventing recursion
 */
inline void* numa_alloc_on_node_impl(std::size_t size, int node) noexcept
{
    if (size == 0)
    {
        return nullptr;
    }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
    return ::numa_alloc_onnode(size, node);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
    return ::VirtualAllocExNuma(::GetCurrentProcess(),
                                nullptr,
                                size,
                                MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE,
                                static_cast<DWORD>(node));
#else
    (void)size;
    (void)node;
    return nullptr;
#endif
}

/**
 * @brief NUMA-aware interleaved allocation across all nodes
 * @note Uses global :: qualification to call libnuma, preventing recursion
 */
inline void* numa_alloc_interleaved_impl(std::size_t size) noexcept
{
    if (size == 0)
    {
        return nullptr;
    }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
    return ::numa_alloc_interleaved(size);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
    // Windows: No direct interleaved API, use local node as fallback
    return ::VirtualAllocExNuma(::GetCurrentProcess(),
                                nullptr,
                                size,
                                MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE,
                                static_cast<DWORD>(NumaInfo::current_node()));
#else
    (void)size;
    return nullptr;
#endif
}

/**
 * @brief NUMA-aware deallocation
 * @note Uses global :: qualification to call libnuma, preventing recursion
 */
inline void numa_free_impl(void* ptr, std::size_t size) noexcept
{
    if (!ptr)
    {
        return;
    }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
    ::numa_free(ptr, size);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
    (void)size;
    ::VirtualFreeEx(::GetCurrentProcess(), ptr, 0, MEM_RELEASE);
#else
    (void)ptr;
    (void)size;
#endif
}

} // namespace detail

// =============================================================================
// NumaAlignedAllocator
// =============================================================================

/**
 * @brief Combined NUMA-aware + cache-aligned STL allocator
 *
 * @tparam T Element type
 * @tparam Alignment Memory alignment in bytes (must be power of 2, default 64)
 * @tparam Policy NUMA allocation policy (NumaLocalPolicy, NumaPreferredPolicy,
 *         NumaInterleavedPolicy) - reuses policies from NumaAllocator.h
 *
 * Strict Mode Invariants:
 * 1. If NUMA available: ALL allocations use NUMA APIs (page-aligned >= 4KB)
 *    - If NUMA allocation fails: throw bad_alloc (no fallback)
 * 2. If NUMA unavailable: ALL allocations use explicit aligned allocation
 * 3. No mixing of allocation sources (prevents deallocation UB)
 */
template <typename T, std::size_t Alignment = 64, typename Policy = NumaLocalPolicy>
class NumaAlignedAllocator
{
public:
    // STL allocator type definitions
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    static constexpr std::size_t alignment = Alignment;

    static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of two");
    static_assert(Alignment >= alignof(T), "Alignment must be at least alignof(T)");
    static_assert(Alignment <= 4096, "Alignment must not exceed page size (4096)");

    // Constructors
    NumaAlignedAllocator() noexcept
        : mPolicy()
        , numa_available_(NumaInfo::is_available())
    {
    }

    explicit NumaAlignedAllocator(const Policy& policy) noexcept
        : mPolicy(policy)
        , numa_available_(NumaInfo::is_available())
    {
    }

    template <typename U>
    NumaAlignedAllocator(const NumaAlignedAllocator<U, Alignment, Policy>& other) noexcept
        : mPolicy(other.policy())
        , numa_available_(other.numa_available())
    {
    }

    // Rebind for STL containers
    template <typename U>
    struct rebind
    {
        using other = NumaAlignedAllocator<U, Alignment, Policy>;
    };

    /**
     * @brief Allocate memory with NUMA locality and alignment
     *
     * @param n Number of elements to allocate
     * @return Pointer to allocated memory (aligned to Alignment bytes)
     * @throws std::bad_alloc on allocation failure
     *
     * @note The returned pointer is guaranteed to be aligned to Alignment bytes.
     *       On NUMA systems, memory is allocated on the node specified by Policy.
     *       If NUMA is available but allocation fails, throws rather than falling
     *       back to aligned allocation (prevents deallocation mismatch).
     */
    [[nodiscard]] pointer allocate(size_type n)
    {
        if (n == 0)
        {
            return nullptr;
        }

        if (n > max_size())
        {
            throw std::bad_alloc();
        }

        const size_type bytes = n * sizeof(T);
        void* ptr = nullptr;

        if (numa_available_)
        {
            // NUMA path: page-aligned (>= 4KB), satisfies Alignment <= 4096
            // Policy handling matches NumaAllocator.h pattern
            if constexpr (std::is_same_v<Policy, NumaInterleavedPolicy>)
            {
                ptr = detail::numa_alloc_interleaved_impl(bytes);
            }
            else if constexpr (std::is_same_v<Policy, NumaPreferredPolicy>)
            {
                ptr = detail::numa_alloc_on_node_impl(bytes, mPolicy.node);
            }
            else
            {
                // NumaLocalPolicy: allocate on current thread's node
                ptr = detail::numa_alloc_on_node_impl(bytes, NumaInfo::current_node());
            }

            // CRITICAL: Do NOT fall back to aligned allocation if NUMA fails.
            // Falling back would cause deallocate() to call numa_free() on a
            // pointer from aligned_alloc, which is undefined behavior.
            if (!ptr)
            {
                throw std::bad_alloc();
            }
            return static_cast<pointer>(ptr);
        }

        // Non-NUMA path: explicit aligned allocation
        // Uses aligned_alloc_portable from NumaAllocator.h
        ptr = detail::aligned_alloc_portable(Alignment, bytes);
        if (!ptr)
        {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(ptr);
    }

    /**
     * @brief Deallocate memory
     *
     * @param ptr Pointer previously returned by allocate()
     * @param n Number of elements (must match allocate call)
     *
     * @note Deallocation method is determined by numa_available_ which is
     *       constant for the allocator's lifetime. Since allocate() throws
     *       rather than falling back, this is always correct.
     */
    void deallocate(pointer ptr, size_type n) noexcept
    {
        if (!ptr)
        {
            return;
        }

        const size_type bytes = n * sizeof(T);

        if (numa_available_)
        {
            detail::numa_free_impl(ptr, bytes);
        }
        else
        {
            detail::aligned_free_portable(ptr);
        }
    }

    /**
     * @brief Maximum number of elements that can be allocated
     */
    [[nodiscard]] size_type max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    // Accessors
    [[nodiscard]] const Policy& policy() const noexcept
    {
        return mPolicy;
    }
    [[nodiscard]] bool numa_available() const noexcept
    {
        return numa_available_;
    }

private:
    Policy mPolicy;
    bool numa_available_;
};

// Equality operators
// Note: Empty policies (NumaLocalPolicy, NumaInterleavedPolicy) are always equal
// NumaPreferredPolicy compares by node number
template <typename T1, std::size_t A1, typename P1, typename T2, std::size_t A2, typename P2>
bool operator==(const NumaAlignedAllocator<T1, A1, P1>& lhs, const NumaAlignedAllocator<T2, A2, P2>& rhs) noexcept
{
    if constexpr (A1 != A2 || !std::is_same_v<P1, P2>)
    {
        return false;
    }
    else
    {
        // Must have same NUMA availability state to safely deallocate
        if (lhs.numa_available() != rhs.numa_available())
        {
            return false;
        }
        // Policy comparison: empty policies are always equal
        if constexpr (std::is_same_v<P1, NumaPreferredPolicy>)
        {
            return lhs.policy().node == rhs.policy().node;
        }
        else
        {
            // NumaLocalPolicy and NumaInterleavedPolicy are empty - always equal
            return true;
        }
    }
}

template <typename T1, std::size_t A1, typename P1, typename T2, std::size_t A2, typename P2>
bool operator!=(const NumaAlignedAllocator<T1, A1, P1>& lhs, const NumaAlignedAllocator<T2, A2, P2>& rhs) noexcept
{
    return !(lhs == rhs);
}

// =============================================================================
// Convenience Type Aliases
// =============================================================================

/// Allocator for local NUMA node with alignment (default 64 bytes)
template <typename T, std::size_t Alignment = 64>
using NumaLocalAllocator = NumaAlignedAllocator<T, Alignment, NumaLocalPolicy>;

/// Allocator for specific NUMA node with alignment (default 64 bytes)
template <typename T, std::size_t Alignment = 64>
using NumaPreferredAllocator = NumaAlignedAllocator<T, Alignment, NumaPreferredPolicy>;

/// Allocator with interleaved NUMA allocation and alignment (default 64 bytes)
template <typename T, std::size_t Alignment = 64>
using NumaInterleavedAllocator = NumaAlignedAllocator<T, Alignment, NumaInterleavedPolicy>;

} // namespace memory
} // namespace fat_p
