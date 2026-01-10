/**
 * @file NumaAllocator.h
 * @brief NUMA-aware memory allocator for many-core HPC systems
 *
 * @details Optimized memory allocation for Non-Uniform Memory Access (NUMA) architectures.
 * Reduces inter-node memory latency by allocating on local NUMA nodes.
 *
 * Key Features:
 * - NUMA node auto-detection
 * - Thread-local memory allocation with pool source tracking
 * - Interleaved allocation policies
 * - Local allocation for thread-bound data
 * - Cross-platform (Linux, Windows)
 * - Fallback to standard allocation when NUMA unavailable
 * - STL allocator interface
 * - Cached availability check for allocation/deallocation consistency
 *
 * Platform Requirements:
 * - Linux: Requires libnuma (-lnuma). Without it, falls back to standard allocation.
 * - Windows: Uses native NUMA APIs (no additional dependencies).
 *
 * IMPORTANT - Page Granularity Warning:
 * NUMA allocation functions allocate memory in OS page granularity (typically 4KB).
 * This allocator is designed for:
 * - Contiguous containers (std::vector, arrays)
 * - Large allocations where page overhead is negligible
 *
 * DO NOT use with node-based containers (std::list, std::map) directly.
 *
 * Requires: C++17
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: NumaAllocator
  file_role: public_header
  path: fat_p/NumaAllocator.h
  namespace: fat_p
  summary: "Public header for NumaAllocator."
  api_stability: in_work
  related:
    docs_search: "NumaAllocator"
    tests:
      - tests/test_NumaAllocator.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 6
    defines_unprefixed: 2
    undefs_total: 1
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>
#include <vector>

// =============================================================================
// Platform Detection and Feature Flags
// =============================================================================
// Following FATP_HAS_* pattern with explicit 0/1 values for safer #if checks.

#if defined(__linux__)
    #if __has_include(<numa.h>)
        #include <numa.h>
        #include <numaif.h>
        #include <sched.h>
        #include <unistd.h>
        #define FATP_HAS_NUMA_SUPPORT 1
    #else
        #define FATP_HAS_NUMA_SUPPORT 0
    #endif
#elif defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <malloc.h>
    #define FATP_HAS_NUMA_SUPPORT 1
#else
    #define FATP_HAS_NUMA_SUPPORT 0
#endif

// Clean up Windows macro pollution
#ifdef max
    #undef max
#endif

namespace fat_p
{
namespace memory
{

namespace detail
{

/// @brief Round size up to be a multiple of alignment (required by std::aligned_alloc)
inline size_t align_size(size_t size, size_t alignment) noexcept
{
    return ((size + alignment - 1) / alignment) * alignment;
}

/// @brief Platform-independent aligned allocation for over-aligned types
inline void* aligned_alloc_portable(size_t alignment, size_t size) noexcept
{
    // std::aligned_alloc requires size to be a multiple of alignment
    size_t aligned_size = align_size(size, alignment);

#if defined(__linux__)
    #if __cplusplus >= 201703L
    return std::aligned_alloc(alignment, aligned_size);
    #else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, aligned_size) != 0)
    {
        return nullptr;
    }
    return ptr;
    #endif
#elif defined(_WIN32)
    return _aligned_malloc(aligned_size, alignment);
#else
    // Generic C++17 fallback - use std::aligned_alloc
    // Note: std::aligned_alloc requires alignment to be a power of 2
    // and size to be a multiple of alignment (handled by align_size above)
    return std::aligned_alloc(alignment, aligned_size);
#endif
}

/// @brief Platform-independent aligned deallocation
inline void aligned_free_portable(void* ptr) noexcept
{
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    // On Linux and generic platforms, aligned_alloc/posix_memalign freed with std::free
    std::free(ptr);
#endif
}

struct NumaState
{
    std::atomic<bool> available{false};
    std::atomic<bool> initialized{false};
    std::mutex init_mutex;

    void initialize()
    {
        if (initialized.load(std::memory_order_acquire))
        {
            return;
        }

        std::lock_guard<std::mutex> lock(init_mutex);
        if (initialized.load(std::memory_order_relaxed))
        {
            return;
        }

        bool result = false;
#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        result = (numa_available() != -1);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
        ULONG highest_node;
        result = (GetNumaHighestNodeNumber(&highest_node) != 0);
#endif
        available.store(result, std::memory_order_relaxed);
        initialized.store(true, std::memory_order_release);
    }

    bool is_available()
    {
        if (!initialized.load(std::memory_order_acquire))
        {
            initialize();
        }
        return available.load(std::memory_order_relaxed);
    }
};

inline NumaState& get_numa_state()
{
    static NumaState state;
    return state;
}

} // namespace detail

/**
 * @brief Query interface for NUMA topology information
 * 
 * CONTRACT GUARANTEES (always hold, even when NUMA unavailable):
 * - num_nodes() >= 1 (returns 1 when NUMA unavailable - "virtual single-node model")
 * - 0 <= current_node() < num_nodes() (always valid node index)
 * - cpus_on_node(n) > 0 for valid n
 * 
 * This design ensures code can always iterate over nodes without special-casing
 * NUMA availability. When NUMA is unavailable, the system presents itself as a
 * single-node topology with node 0 containing all CPUs.
 * 
 * @note These guarantees are relied upon by tests and container implementations.
 */
class NumaInfo
{
public:
    static bool is_available() noexcept
    {
        return detail::get_numa_state().is_available();
    }

    /// @return Number of NUMA nodes. Always >= 1 (1 when NUMA unavailable)
    static int num_nodes() noexcept
    {
        if (!is_available())
        {
            return 1;
        }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        int nodes = numa_num_configured_nodes();
        return nodes > 0 ? nodes : 1;
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
        ULONG highest_node;
        if (GetNumaHighestNodeNumber(&highest_node))
        {
            return static_cast<int>(highest_node + 1);
        }
#endif
        return 1;
    }

    static int current_node() noexcept
    {
        if (!is_available())
        {
            return 0;
        }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        int cpu = sched_getcpu();
        if (cpu >= 0)
        {
            int node = numa_node_of_cpu(cpu);
            if (node >= 0)
            {
                return node;
            }
        }
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
        PROCESSOR_NUMBER proc_num;
        GetCurrentProcessorNumberEx(&proc_num);
        USHORT node;
        if (GetNumaProcessorNodeEx(&proc_num, &node))
        {
            return static_cast<int>(node);
        }
#endif
        return 0;
    }

    /// @note On Windows systems with >64 logical processors per NUMA node,
    /// this function may undercount. Use GetLogicalProcessorInformationEx
    /// for complete accuracy on such systems.
    static int cpus_on_node(int node) noexcept
    {
        if (!is_available())
        {
            return static_cast<int>(std::thread::hardware_concurrency());
        }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        if (node >= 0 && node < numa_num_configured_nodes())
        {
            struct bitmask* cpus = numa_allocate_cpumask();
            if (cpus)
            {
                int ret = numa_node_to_cpus(node, cpus);
                if (ret == 0)
                {
                    int count = numa_bitmask_weight(cpus);
                    numa_free_cpumask(cpus);
                    return count;
                }
                numa_free_cpumask(cpus);
            }
        }
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
        GROUP_AFFINITY affinity;
        if (GetNumaNodeProcessorMaskEx(static_cast<USHORT>(node), &affinity))
        {
            ULONGLONG mask = affinity.Mask;
            int count = 0;
            while (mask)
            {
                count += static_cast<int>(mask & 1);
                mask >>= 1;
            }
            return count;
        }
#endif
        (void)node;
        return static_cast<int>(std::thread::hardware_concurrency());
    }
};

struct NumaLocalPolicy
{
};

struct NumaInterleavedPolicy
{
};

struct NumaPreferredPolicy
{
    int node = 0;

    bool operator==(const NumaPreferredPolicy& other) const noexcept
    {
        return node == other.node;
    }

    bool operator!=(const NumaPreferredPolicy& other) const noexcept
    {
        return node != other.node;
    }
};

template<typename T, typename Policy = NumaLocalPolicy>
class NumaAllocator
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal =
        std::bool_constant<std::is_same_v<Policy, NumaLocalPolicy> ||
                           std::is_same_v<Policy, NumaInterleavedPolicy>>;

private:
    static constexpr bool is_over_aligned = alignof(T) > alignof(std::max_align_t);

    Policy policy_;
    bool numa_available_;

    void* allocate_on_node(size_t size, int node)
    {
        if (!numa_available_)
        {
            if constexpr (is_over_aligned)
            {
                return detail::aligned_alloc_portable(alignof(T), size);
            }
            else
            {
                return std::malloc(size);
            }
        }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        return numa_alloc_onnode(size, node);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
        // NOTE: VirtualAllocExNuma is a "preferred" hint, not a guarantee.
        // Under memory pressure, Windows may allocate on a different node.
        // This can cause cross-socket latency (150ns vs 60ns). For critical
        // real-time workloads, verify placement with get_memory_node().
        void* ptr = VirtualAllocExNuma(GetCurrentProcess(),
                                       nullptr,
                                       size,
                                       MEM_RESERVE | MEM_COMMIT,
                                       PAGE_READWRITE,
                                       static_cast<DWORD>(node));
        return ptr;
#else
        (void)node;
        if constexpr (is_over_aligned)
        {
            return detail::aligned_alloc_portable(alignof(T), size);
        }
        else
        {
            return std::malloc(size);
        }
#endif
    }

    void* allocate_interleaved(size_t size)
    {
        if (!numa_available_)
        {
            if constexpr (is_over_aligned)
            {
                return detail::aligned_alloc_portable(alignof(T), size);
            }
            else
            {
                return std::malloc(size);
            }
        }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        return numa_alloc_interleaved(size);
#else
        if constexpr (is_over_aligned)
        {
            return detail::aligned_alloc_portable(alignof(T), size);
        }
        else
        {
            return std::malloc(size);
        }
#endif
    }

    void free_numa(void* ptr, size_t size)
    {
        if (!numa_available_)
        {
            if constexpr (is_over_aligned)
            {
                detail::aligned_free_portable(ptr);
            }
            else
            {
                std::free(ptr);
            }
            return;
        }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        numa_free(ptr, size);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
        if constexpr (std::is_same_v<Policy, NumaInterleavedPolicy>)
        {
            // Interleaved on Windows falls back to malloc/aligned_alloc
            if constexpr (is_over_aligned)
            {
                detail::aligned_free_portable(ptr);
            }
            else
            {
                std::free(ptr);
            }
        }
        else
        {
            VirtualFreeEx(GetCurrentProcess(), ptr, 0, MEM_RELEASE);
        }
#else
        (void)size;
        if constexpr (is_over_aligned)
        {
            detail::aligned_free_portable(ptr);
        }
        else
        {
            std::free(ptr);
        }
#endif
    }

public:
    NumaAllocator() noexcept : policy_(), numa_available_(NumaInfo::is_available())
    {
    }

    explicit NumaAllocator(Policy policy) noexcept
        : policy_(policy), numa_available_(NumaInfo::is_available())
    {
        if constexpr (std::is_same_v<Policy, NumaPreferredPolicy>)
        {
            int max_node = NumaInfo::num_nodes() - 1;
            if (policy_.node < 0 || policy_.node > max_node)
            {
                assert(false && "NumaPreferredPolicy: node out of valid range");
                policy_.node = std::clamp(policy_.node, 0, max_node);
            }
        }
    }

    template<typename U>
    NumaAllocator(const NumaAllocator<U, Policy>& other) noexcept
        : policy_(other.get_policy()), numa_available_(other.is_numa_available())
    {
    }

    /// @note allocate(0) returns nullptr per C++ standard permission.
    /// Some STL debug modes may expect non-null; use allocate(1) if needed.
    [[nodiscard]] T* allocate(size_t n)
    {
        if (n == 0)
        {
            return nullptr;
        }

        if (n > (std::numeric_limits<size_t>::max)() / sizeof(T))
        {
            throw std::bad_alloc();
        }

        size_t size = n * sizeof(T);
        void* ptr = nullptr;

        if constexpr (std::is_same_v<Policy, NumaLocalPolicy>)
        {
            int node = NumaInfo::current_node();
            ptr = allocate_on_node(size, node);
        }
        else if constexpr (std::is_same_v<Policy, NumaInterleavedPolicy>)
        {
            ptr = allocate_interleaved(size);
        }
        else if constexpr (std::is_same_v<Policy, NumaPreferredPolicy>)
        {
            ptr = allocate_on_node(size, policy_.node);
        }

        if (!ptr)
        {
            throw std::bad_alloc();
        }

        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, size_t n) noexcept
    {
        if (!ptr)
        {
            return;
        }
        free_numa(ptr, n * sizeof(T));
    }

    template<typename U>
    struct rebind
    {
        using other = NumaAllocator<U, Policy>;
    };

    const Policy& get_policy() const noexcept
    {
        return policy_;
    }

    bool is_numa_available() const noexcept
    {
        return numa_available_;
    }

    template<typename, typename>
    friend class NumaAllocator;
};

template<typename T1, typename P1, typename T2, typename P2>
bool operator==(const NumaAllocator<T1, P1>& lhs, const NumaAllocator<T2, P2>& rhs) noexcept
{
    if constexpr (!std::is_same_v<P1, P2>)
    {
        return false;
    }
    else if constexpr (std::is_same_v<P1, NumaPreferredPolicy>)
    {
        return lhs.get_policy() == rhs.get_policy();
    }
    else
    {
        return true;
    }
}

template<typename T1, typename P1, typename T2, typename P2>
bool operator!=(const NumaAllocator<T1, P1>& lhs, const NumaAllocator<T2, P2>& rhs) noexcept
{
    return !(lhs == rhs);
}

/**
 * @brief Thread-local memory pool for small, high-frequency allocations.
 *
 * ThreadLocalNumaPool Design Notes:
 *
 * LIFETIME CONSTRAINTS (CRITICAL):
 * - Pool memory is owned by the thread that created it.
 * - When the owning thread exits, the pool is destroyed and all memory freed.
 * - Pointers from ThreadLocalNumaPool MUST NOT outlive the owning thread.
 * - Calling deallocate() on a pointer after the owning thread has exited is
 *   UNDEFINED BEHAVIOR (use-after-free).
 *
 * Cross-thread usage patterns:
 * - SAFE: Thread A allocates, passes pointer to thread B, thread B uses it,
 *         thread A is still alive when thread B finishes.
 * - UNSAFE: Thread A allocates, passes pointer to thread B, thread A exits,
 *           thread B calls deallocate() -> USE-AFTER-FREE.
 *
 * For cross-thread scenarios where producer may exit before consumer:
 * - Use NumaAllocator directly (not ThreadLocalNumaPool)
 * - Or ensure producer thread outlives all consumers
 *
 * Deallocation behavior:
 * - Pool allocations: deallocate() is a no-op (memory reclaimed on reset/exit)
 * - Direct allocations (pool overflow): properly freed using stored metadata
 *
 * Type requirements: Designed for trivially destructible types. Non-trivial
 * types require explicit destructor calls before deallocation.
 */
template<typename T>
class ThreadLocalNumaPool
{
private:
    static constexpr bool is_over_aligned = alignof(T) > alignof(std::max_align_t);

    /// @brief Magic number for header validation
    static constexpr uint32_t HEADER_MAGIC = 0xFADE'0A11;

    struct AllocationHeader
    {
        uint32_t magic;
        enum class Source : uint8_t
        {
            Pool,
            Direct
        };
        Source source;
        bool numa_available;
    };

    // Calculate alignment requirements to ensure payload T is aligned correctly
    // after the header.
    static constexpr size_t align_req = alignof(T);
    static constexpr size_t header_size_bytes = sizeof(AllocationHeader);
    static constexpr size_t padding =
        (align_req - (header_size_bytes % align_req)) % align_req;
    static constexpr size_t total_overhead = header_size_bytes + padding;

    static void free_memory_safe(void* ptr, size_t bytes, bool numa_available)
    {
        if (!numa_available)
        {
            if constexpr (is_over_aligned)
            {
                detail::aligned_free_portable(ptr);
            }
            else
            {
                std::free(ptr);
            }
            return;
        }
#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
        numa_free(ptr, bytes);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
        (void)bytes;
        VirtualFreeEx(GetCurrentProcess(), ptr, 0, MEM_RELEASE);
#else
        (void)bytes;
        if constexpr (is_over_aligned)
        {
            detail::aligned_free_portable(ptr);
        }
        else
        {
            std::free(ptr);
        }
#endif
    }

    struct PoolNode
    {
        // Use char* for byte-level pointer arithmetic and header management
        char* memory = nullptr;
        size_t capacity_bytes = 0;
        size_t used_bytes = 0;
        int numa_node = 0;
        bool numa_was_available = false;

        PoolNode() = default;

        ~PoolNode()
        {
            if (memory)
            {
                // Use the stored NUMA state, as global state may have changed or be invalid
                // during static destruction.
                free_memory_safe(memory, capacity_bytes, numa_was_available);
                memory = nullptr;
                capacity_bytes = 0;
                used_bytes = 0;
            }
        }

        PoolNode(const PoolNode&) = delete;
        PoolNode& operator=(const PoolNode&) = delete;
        PoolNode(PoolNode&&) = delete;
        PoolNode& operator=(PoolNode&&) = delete;
    };

    /// @brief Target element count for pool sizing.
    /// Actual usable capacity may vary due to header overhead.
    static constexpr size_t default_pool_count = 1024;

    static PoolNode& get_thread_pool()
    {
        thread_local PoolNode pool;
        return pool;
    }

    static void initialize_pool()
    {
        auto& pool = get_thread_pool();
        if (pool.memory == nullptr)
        {
            bool numa_avail = NumaInfo::is_available();
            int node = NumaInfo::current_node();

            // Account for header overhead in pool sizing
            // Check for overflow
            constexpr size_t per_item = sizeof(T) + total_overhead;
            static_assert(default_pool_count <= (std::numeric_limits<size_t>::max)() / per_item,
                          "Pool sizing would overflow");
            size_t size_bytes = default_pool_count * per_item;

            // Allocate memory for the pool itself.
            void* raw_mem = nullptr;
            if (numa_avail)
            {
#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
                raw_mem = numa_alloc_onnode(size_bytes, node);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
                raw_mem = VirtualAllocExNuma(GetCurrentProcess(), nullptr, size_bytes,
                                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE,
                                             static_cast<DWORD>(node));
#endif
            }

            if (!raw_mem)
            {
                if constexpr (is_over_aligned)
                {
                    raw_mem = detail::aligned_alloc_portable(alignof(T), size_bytes);
                }
                else
                {
                    raw_mem = std::malloc(size_bytes);
                }
                // If we fell back to malloc, we must treat numa as unavailable for this block
                numa_avail = false;
            }

            if (!raw_mem)
            {
                throw std::bad_alloc();
            }

            pool.memory = static_cast<char*>(raw_mem);
            pool.capacity_bytes = size_bytes;
            pool.used_bytes = 0;
            pool.numa_node = node;
            pool.numa_was_available = numa_avail;
        }
    }

    /// @brief Check if a pointer falls within this thread's pool range.
    /// @return true if pointer is within our pool, false otherwise.
    static bool is_in_our_pool(const void* ptr) noexcept
    {
        auto& pool = get_thread_pool();
        if (pool.memory == nullptr)
        {
            return false;
        }
        const char* p = static_cast<const char*>(ptr);
        const char* pool_start = pool.memory;
        const char* pool_end = pool_start + pool.capacity_bytes;
        return (p >= pool_start && p < pool_end);
    }

public:
    /// @note This pool is optimized for trivially destructible types.
    /// For non-trivial types, use placement new after allocation and
    /// explicit destructor calls before deallocation.
    static T* allocate(size_t n)
    {
        static_assert(std::is_trivially_destructible_v<T>,
                      "ThreadLocalNumaPool is designed for trivially destructible types. "
                      "Non-trivial types require explicit destructor calls before deallocation.");

        initialize_pool();
        auto& pool = get_thread_pool();

        size_t needed_bytes = n * sizeof(T);

        // Try to allocate from thread-local pool
        // Check if we have space AND if the request is small enough (1/4 of pool bytes)
        if (needed_bytes <= pool.capacity_bytes / 4 &&
            pool.used_bytes + needed_bytes + total_overhead <= pool.capacity_bytes)
        {
            char* header_loc = pool.memory + pool.used_bytes;

            // Construct header
            auto* header = reinterpret_cast<AllocationHeader*>(header_loc);
            header->magic = HEADER_MAGIC;
            header->source = AllocationHeader::Source::Pool;
            header->numa_available = pool.numa_was_available;

            // Advance to payload
            char* payload_loc = header_loc + total_overhead;
            pool.used_bytes += total_overhead + needed_bytes;

            return reinterpret_cast<T*>(payload_loc);
        }

        // Fallback: Direct allocation
        // We must still add a header so deallocate knows how to free it
        bool numa_avail = NumaInfo::is_available();
        size_t alloc_size = needed_bytes + total_overhead;
        void* raw_mem = nullptr;

        if (numa_avail)
        {
#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
            int node = NumaInfo::current_node();
            raw_mem = numa_alloc_onnode(alloc_size, node);
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
            int node = NumaInfo::current_node();
            raw_mem = VirtualAllocExNuma(GetCurrentProcess(), nullptr, alloc_size,
                                         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE,
                                         static_cast<DWORD>(node));
#endif
        }

        if (!raw_mem)
        {
            if constexpr (is_over_aligned)
            {
                raw_mem = detail::aligned_alloc_portable(alignof(T), alloc_size);
            }
            else
            {
                raw_mem = std::malloc(alloc_size);
            }
            numa_avail = false;
        }

        if (!raw_mem)
        {
            throw std::bad_alloc();
        }

        auto* header = reinterpret_cast<AllocationHeader*>(raw_mem);
        header->magic = HEADER_MAGIC;
        header->source = AllocationHeader::Source::Direct;
        header->numa_available = numa_avail;

        return reinterpret_cast<T*>(static_cast<char*>(raw_mem) + total_overhead);
    }

    /// @brief Deallocate memory previously allocated from this pool.
    ///
    /// IMPORTANT SAFETY CONSTRAINTS:
    /// - Pool allocations from THIS thread: no-op (memory reclaimed on reset/exit)
    /// - Pool allocations from OTHER threads: no-op IF owner thread is still alive;
    ///   UNDEFINED BEHAVIOR if owner thread has exited (though we attempt graceful
    ///   recovery on Windows via SEH).
    /// - Direct allocations: properly freed regardless of calling thread.
    ///
    /// For guaranteed cross-thread safety, use NumaAllocator directly.
    static void deallocate(T* ptr, size_t n)
    {
        if (!ptr)
        {
            return;
        }

        // Fast path: Check if this pointer is in OUR thread's pool
        // This is safe because we're only reading our own thread-local data
        if (is_in_our_pool(ptr))
        {
            // Our pool, our memory - no-op (will be freed on reset/exit)
            return;
        }

        // Slow path: Pointer is NOT in our pool
        // It's either:
        // 1. A direct allocation (from any thread) - safe to read header and free
        // 2. A pool allocation from another thread - reading header is safe only if
        //    that thread is still alive
        //
        // If the owning thread has exited and freed its pool, the memory may be
        // unmapped. On Windows, we use SEH to catch access violations gracefully.
        // On other platforms, this remains UB that may crash.

#if defined(_WIN32)
        __try
        {
            deallocate_slow_path(ptr, n);
        }
        __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                      ? EXCEPTION_EXECUTE_HANDLER
                      : EXCEPTION_CONTINUE_SEARCH)
        {
            // Access violation: memory was unmapped (owner thread exited).
            // Gracefully return without crashing - this is documented UB
            // but we prefer not to crash in production.
            return;
        }
#else
        deallocate_slow_path(ptr, n);
#endif
    }

private:
    /// @brief Slow path for deallocation - may access potentially invalid memory
    static void deallocate_slow_path(T* ptr, size_t n)
    {
        char* payload_loc = reinterpret_cast<char*>(ptr);
        char* header_loc = payload_loc - total_overhead;
        auto* header = reinterpret_cast<AllocationHeader*>(header_loc);

        // Validate magic number as a safety check
        // This catches some (but not all) cases of reading freed-but-mapped memory
        if (header->magic != HEADER_MAGIC)
        {
            // Invalid header - memory may be corrupted or reused
            // Best we can do is return without further damage
            return;
        }

        if (header->source == AllocationHeader::Source::Pool)
        {
            // Memory belongs to another thread's pool.
            // No-op - that thread will reclaim it on reset/exit.
            return;
        }

        // Direct allocation: free it
        size_t total_size = (n * sizeof(T)) + total_overhead;
        free_memory_safe(header_loc, total_size, header->numa_available);
    }

public:

    /// @warning Invalidates ALL previously allocated pointers from this pool.
    /// Deallocating pointers obtained before reset() is undefined behavior.
    /// This includes pointers that were passed to other threads.
    static void reset()
    {
        get_thread_pool().used_bytes = 0;
    }

    static int numa_node()
    {
        initialize_pool();
        return get_thread_pool().numa_node;
    }

    /// @brief Returns the target element capacity (not accounting for overhead).
    /// @note This is the configured target count, not a precise capacity.
    /// Actual usable elements depend on allocation patterns and header overhead.
    static size_t capacity()
    {
        return default_pool_count;
    }

    /// @brief Returns approximate number of elements allocated from the pool.
    /// @note This is an estimate for monitoring purposes, not exact accounting.
    /// Due to header overhead and alignment, actual count may differ slightly.
    static size_t used()
    {
        size_t per_item = sizeof(T) + total_overhead;
        return get_thread_pool().used_bytes / per_item;
    }
};

template<typename T>
using NumaLocalVector = std::vector<T, NumaAllocator<T, NumaLocalPolicy>>;

template<typename T>
using NumaInterleavedVector = std::vector<T, NumaAllocator<T, NumaInterleavedPolicy>>;

template<typename T>
using NumaPreferredVector = std::vector<T, NumaAllocator<T, NumaPreferredPolicy>>;

template<typename T>
NumaPreferredVector<T> make_preferred_vector(int node)
{
    return NumaPreferredVector<T>(
        NumaAllocator<T, NumaPreferredPolicy>(NumaPreferredPolicy{node}));
}

template<typename T>
NumaPreferredVector<T> make_preferred_vector(int node, size_t count, const T& value = T())
{
    return NumaPreferredVector<T>(
        count, value, NumaAllocator<T, NumaPreferredPolicy>(NumaPreferredPolicy{node}));
}

/// @brief Bind the current thread to execute only on CPUs belonging to the specified NUMA node.
/// @param node The NUMA node index (0-based).
/// @return true if binding succeeded, false otherwise.
/// @note On Linux, uses sched_setaffinity for explicit CPU affinity control.
///       On Windows, uses SetThreadGroupAffinity.
inline bool bind_thread_to_node(int node) noexcept
{
    if (!NumaInfo::is_available())
    {
        return false;
    }

    if (node < 0 || node >= NumaInfo::num_nodes())
    {
        return false;
    }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
    // Use explicit CPU affinity via sched_setaffinity for more reliable binding
    struct bitmask* cpus = numa_allocate_cpumask();
    if (!cpus)
    {
        return false;
    }

    if (numa_node_to_cpus(node, cpus) != 0)
    {
        numa_free_cpumask(cpus);
        return false;
    }

    cpu_set_t set;
    CPU_ZERO(&set);
    for (unsigned int i = 0; i < cpus->size; ++i)
    {
        if (numa_bitmask_isbitset(cpus, i))
        {
            // Guard against systems with > CPU_SETSIZE (1024) cores
            if (i < CPU_SETSIZE)
            {
                CPU_SET(i, &set);
            }
        }
    }
    numa_free_cpumask(cpus);

    return sched_setaffinity(0, sizeof(cpu_set_t), &set) == 0;
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
    GROUP_AFFINITY affinity;
    if (GetNumaNodeProcessorMaskEx(static_cast<USHORT>(node), &affinity))
    {
        return SetThreadGroupAffinity(GetCurrentThread(), &affinity, nullptr) != 0;
    }
    return false;
#else
    (void)node;
    return false;
#endif
}

struct NumaMemoryStats
{
    size_t total_bytes = 0;
    size_t free_bytes = 0;
    size_t used_bytes = 0;
    bool valid = false;
    bool has_total = false;
};

inline NumaMemoryStats get_node_memory_stats(int node) noexcept
{
    NumaMemoryStats stats{};

    if (!NumaInfo::is_available())
    {
        return stats;
    }

    if (node < 0 || node >= NumaInfo::num_nodes())
    {
        return stats;
    }

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
    long long free_mem = 0;
    long long total_mem = numa_node_size64(node, &free_mem);
    if (total_mem > 0)
    {
        stats.total_bytes = static_cast<size_t>(total_mem);
        stats.free_bytes = static_cast<size_t>(free_mem);
        stats.used_bytes = stats.total_bytes - stats.free_bytes;
        stats.valid = true;
        stats.has_total = true;
    }
#elif defined(_WIN32) && FATP_HAS_NUMA_SUPPORT
    ULONGLONG available_bytes = 0;
    if (GetNumaAvailableMemoryNodeEx(static_cast<USHORT>(node), &available_bytes))
    {
        stats.free_bytes = static_cast<size_t>(available_bytes);
        stats.total_bytes = 0;
        stats.used_bytes = 0;
        stats.valid = true;
        stats.has_total = false;
    }
#else
    (void)node;
#endif

    return stats;
}

#if defined(__linux__) && FATP_HAS_NUMA_SUPPORT
inline int get_memory_node(void* ptr) noexcept
{
    if (!NumaInfo::is_available() || !ptr)
    {
        return -1;
    }

    int status = -1;
    void* pages[] = {ptr};
    if (move_pages(0, 1, pages, nullptr, &status, 0) == 0)
    {
        return status;
    }
    return -1;
}
#else
inline int get_memory_node(void* /*ptr*/) noexcept
{
    return -1;
}
#endif

} // namespace memory
} // namespace fat_p
