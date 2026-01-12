/**
 * @file StringPool.h
 * @brief High-performance string interning pool with policy-based thread safety
 *
 * 
 *
 * @layer Domain
 *
 * @details String pooling (interning) for memory-efficient string storage.
 * Deduplicates identical strings, returning references to single canonical copy.
 *
 * Features:
 * - Automatic deduplication of identical strings
 * - Policy-based conditional thread safety (zero overhead for single-threaded)
 * - Stable string pointers (lifetime = pool lifetime)
 * - Fast lookup via hash table
 * - Memory usage tracking
 * - Reserve capacity to avoid rehashing
 *
 * @section performance Performance Impact
 * - Intern: O(1) average, ~100ns (single-threaded) to ~150ns (multi-threaded)
 * - Memory savings: 50-90% for duplicate-heavy workloads
 * - Cache-friendly: Single string instance improves locality
 * - Zero overhead with SingleThreadedPolicy (no locks, no atomics)
 *
 * @section use_cases Use Cases
 * - Configuration keys/values
 * - JSON parsing (many duplicate keys)
 * - Log messages with repeated patterns
 * - Compiler symbol tables
 * - Game entity names/tags
 *
 * @section usage Usage Examples
 * @code
 * // Single-threaded (zero overhead)
 * StringPool<SingleThreadedPolicy> pool;
 * const char* s1 = pool.intern("hello");
 * const char* s2 = pool.intern("hello");
 * assert(s1 == s2);  // Same pointer!
 *
 * // Multi-threaded with shared reads
 * StringPool<SharedMutexPolicy> shared_pool;
 * auto s = shared_pool.intern("concurrent");  // Thread-safe
 *
 * // Multi-threaded with exclusive locking
 * StringPool<MutexSynchronizationPolicy> mutex_pool;
 *
 * // Get stats
 * auto stats = pool.stats();
 * std::cout << "Unique strings: " << stats.unique_strings << "\n";
 * std::cout << "Memory saved: " << stats.memory_saved << " bytes\n";
 * @endcode
 *
 * Compilation: Requires C++17, optimized for C++20
 * - g++ -std=c++17 -O3 your_code.cpp
 *
 * @section cpp20_optimization C++20 Performance Note
 * In C++17, every lookup (intern, find, contains) with a string_view argument
 * requires constructing a temporary std::string, potentially causing heap
 * allocation for strings exceeding SSO capacity (~15-22 chars).
 *
 * In C++20+, heterogeneous lookup (P0919R3) enables zero-allocation lookups
 * directly with string_view. For HPC workloads with many lookups, C++20 is
 * strongly recommended.
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: StringPool
  file_role: public_header
  path: fat_p/StringPool.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for StringPool."
  api_stability: in_work
  related:
    docs_search: "StringPool"
    tests:
      - tests/test_StringPool.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 3
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include "CppStandardDetection.h"

#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>

#include "ConcurrencyPolicies.h"

namespace fat_p
{

/**
 * @brief Statistics for a StringPool instance
 *
 * Provides insight into pool utilization and memory efficiency.
 */
struct StringPoolStats
{
    size_t unique_strings = 0;   ///< Number of unique strings stored
    size_t total_interns = 0;    ///< Total intern() calls made
    size_t content_bytes = 0;    ///< Logical bytes of string content (not heap allocation)
    size_t memory_saved = 0;     ///< Bytes saved by deduplication
    double hit_rate = 0.0;       ///< Cache hit rate (0.0 to 1.0)
};

namespace detail
{

// P0919R3: Heterogeneous lookup for unordered containers, adopted in C++20
#if FATP_HAS_GENERIC_UNORDERED_LOOKUP
#define FATP_USE_TRANSPARENT_LOOKUP 1
#elif FATP_CPP20_OR_LATER
#define FATP_USE_TRANSPARENT_LOOKUP 1
#else
#define FATP_USE_TRANSPARENT_LOOKUP 0
#endif

#if FATP_USE_TRANSPARENT_LOOKUP

struct StringHash
{
    using is_transparent = void;

    size_t operator()(std::string_view sv) const noexcept
    {
        return std::hash<std::string_view>{}(sv);
    }

    size_t operator()(const std::string& s) const noexcept
    {
        return std::hash<std::string>{}(s);
    }
};

struct StringEqual
{
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept
    {
        return lhs == rhs;
    }

    bool operator()(const std::string& lhs, std::string_view rhs) const noexcept
    {
        return lhs == rhs;
    }

    bool operator()(std::string_view lhs, const std::string& rhs) const noexcept
    {
        return lhs == rhs;
    }
};

using StringSet = std::unordered_set<std::string, StringHash, StringEqual>;

#else

using StringSet = std::unordered_set<std::string>;

#endif

template <typename SyncPolicy, typename T>
using StatType =
    std::conditional_t<std::is_same_v<SyncPolicy, SingleThreadedPolicy>, T, std::atomic<T>>;

template <typename T>
inline void increment_stat(T& stat, size_t delta = 1)
{
    if constexpr (std::is_same_v<T, std::atomic<size_t>>)
    {
        stat.fetch_add(delta, std::memory_order_relaxed);
    }
    else
    {
        stat += delta;
    }
}

template <typename T>
inline size_t load_stat(const T& stat) noexcept
{
    if constexpr (std::is_same_v<T, std::atomic<size_t>>)
    {
        return stat.load(std::memory_order_acquire);
    }
    else
    {
        return stat;
    }
}

template <typename T>
inline void store_stat(T& stat, size_t value) noexcept
{
    if constexpr (std::is_same_v<T, std::atomic<size_t>>)
    {
        stat.store(value, std::memory_order_relaxed);
    }
    else
    {
        stat = value;
    }
}

} // namespace detail

/**
 * @brief Policy-based string interning pool with conditional thread safety
 *
 * @tparam SyncPolicy Synchronization policy (default: SingleThreadedPolicy)
 *
 * @details Stores unique strings and returns const char* pointers to them.
 * Multiple calls with identical strings return the same pointer.
 *
 * Thread-safety: Depends on SyncPolicy
 * - SingleThreadedPolicy: No synchronization (zero overhead)
 * - SharedMutexPolicy: Read/write locks for concurrent access
 * - MutexSynchronizationPolicy: Exclusive locks for concurrent access
 *
 * Exception-safety: Strong guarantee
 *
 * Move/Copy semantics: Non-copyable, non-movable. Pools contain synchronization
 * primitives (mutexes) that cannot be moved, and moving would invalidate all
 * outstanding string pointers.
 *
 * @section performance Performance Characteristics
 * SingleThreadedPolicy:
 * - Intern (hit): ~26ns
 * - Intern (miss): ~240ns
 * - Memory overhead: 0 bytes (no locks, no atomics)
 *
 * SharedMutexPolicy:
 * - Intern (hit, uncontended): ~150ns (shared lock)
 * - Intern (miss, uncontended): ~200ns (exclusive lock)
 * - Memory overhead: ~40 bytes (shared_mutex)
 *
 * @section when_to_use When to Use Each Policy
 * - SingleThreadedPolicy: Command-line tools, single-threaded parsers
 * - SharedMutexPolicy: Multi-threaded servers, read-heavy workloads
 * - MutexSynchronizationPolicy: Write-heavy workloads, simpler locking
 */
template <typename SyncPolicy = SingleThreadedPolicy>
class StringPool
{
public:
    StringPool() = default;

    ~StringPool() = default;

    // Non-copyable: Copying would duplicate all strings and confuse pointer identity
    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;

    // Non-movable: SyncPolicy contains mutex/shared_mutex which are not movable,
    // and moving would invalidate all outstanding string pointers
    StringPool(StringPool&&) = delete;
    StringPool& operator=(StringPool&&) = delete;

    /**
     * @brief Intern a string
     * @param str String to intern
     * @return Pointer to interned string (valid for pool lifetime)
     *
     * If string already exists, returns existing pointer.
     * Otherwise, allocates new copy and returns its pointer.
     *
     * Thread-safe: Depends on SyncPolicy
     * Complexity: O(1) average
     */
    const char* intern(std::string_view str)
    {
#if FATP_USE_TRANSPARENT_LOOKUP
        {
            typename SyncPolicy::ReadLock read_lock(sync_policy_.getLock());
            auto it = m_strings.find(str);
            if (it != m_strings.end())
            {
                detail::increment_stat(m_stats.total_interns);
                detail::increment_stat(m_stats.memory_saved, str.size() + 1);
                return it->c_str();
            }
        }

        typename SyncPolicy::WriteLock write_lock(sync_policy_.getLock());

        auto it = m_strings.find(str);
        if (it != m_strings.end())
        {
            detail::increment_stat(m_stats.total_interns);
            detail::increment_stat(m_stats.memory_saved, str.size() + 1);
            return it->c_str();
        }

        auto [inserted_it, success] = m_strings.emplace(str);
#else
        std::string temp(str);

        {
            typename SyncPolicy::ReadLock read_lock(sync_policy_.getLock());
            auto it = m_strings.find(temp);
            if (it != m_strings.end())
            {
                detail::increment_stat(m_stats.total_interns);
                detail::increment_stat(m_stats.memory_saved, str.size() + 1);
                return it->c_str();
            }
        }

        typename SyncPolicy::WriteLock write_lock(sync_policy_.getLock());

        auto it = m_strings.find(temp);
        if (it != m_strings.end())
        {
            detail::increment_stat(m_stats.total_interns);
            detail::increment_stat(m_stats.memory_saved, str.size() + 1);
            return it->c_str();
        }

        auto [inserted_it, success] = m_strings.insert(std::move(temp));
#endif

        if (success)
        {
            detail::increment_stat(m_stats.content_bytes, str.size() + 1);
        }

        detail::increment_stat(m_stats.total_interns);

        return inserted_it->c_str();
    }

    /**
     * @brief Intern a C string
     * @param str Null-terminated C string (nullptr returns interned empty string)
     * @return Pointer to interned string
     */
    const char* intern(const char* str)
    {
        if (!str)
        {
            return intern(std::string_view(""));
        }
        return intern(std::string_view(str));
    }

    /**
     * @brief Intern a std::string
     * @param str String to intern
     * @return Pointer to interned string
     */
    const char* intern(const std::string& str)
    {
        return intern(std::string_view(str));
    }

    /**
     * @brief Check if string is interned
     * @param str String to check
     * @return true if string exists in pool
     */
    bool contains(std::string_view str) const noexcept
    {
        typename SyncPolicy::ReadLock lock(sync_policy_.getLock());
#if FATP_USE_TRANSPARENT_LOOKUP
        return m_strings.find(str) != m_strings.end();
#else
        return m_strings.find(std::string(str)) != m_strings.end();
#endif
    }

    /**
     * @brief Get pointer to interned string (nullptr if not found)
     * @param str String to find
     * @return Pointer to interned string, or nullptr if not found
     */
    const char* find(std::string_view str) const noexcept
    {
        typename SyncPolicy::ReadLock lock(sync_policy_.getLock());
#if FATP_USE_TRANSPARENT_LOOKUP
        auto it = m_strings.find(str);
#else
        auto it = m_strings.find(std::string(str));
#endif
        return (it != m_strings.end()) ? it->c_str() : nullptr;
    }

    /**
     * @brief Get number of unique strings
     */
    size_t size() const noexcept
    {
        typename SyncPolicy::ReadLock lock(sync_policy_.getLock());
        return m_strings.size();
    }

    /**
     * @brief Check if pool is empty
     */
    bool empty() const noexcept
    {
        typename SyncPolicy::ReadLock lock(sync_policy_.getLock());
        return m_strings.empty();
    }

    /**
     * @brief Reserve space for expected number of unique strings
     *
     * Pre-allocates hash table buckets to avoid rehashing during bulk insertion.
     * Useful when the approximate number of unique strings is known in advance.
     *
     * @param n Expected number of unique strings
     *
     * Example:
     * @code
     * StringPool<> pool;
     * pool.reserve(10000);  // Expect ~10,000 unique strings
     * for (const auto& symbol : symbol_table) {
     *     pool.intern(symbol);  // No rehashing during insertion
     * }
     * @endcode
     */
    void reserve(size_t n)
    {
        typename SyncPolicy::WriteLock lock(sync_policy_.getLock());
        m_strings.reserve(n);
    }

    /**
     * @brief Clear all interned strings
     *
     * WARNING: Invalidates all pointers returned by intern()
     */
    void clear()
    {
        typename SyncPolicy::WriteLock lock(sync_policy_.getLock());
        m_strings.clear();
        detail::store_stat(m_stats.total_interns, 0);
        detail::store_stat(m_stats.content_bytes, 0);
        detail::store_stat(m_stats.memory_saved, 0);
    }

    /**
     * @brief Get statistics
     *
     * @note content_bytes tracks logical string content size (characters + null
     * terminator), not actual heap allocations. Due to Small String Optimization
     * (SSO), strings under ~15-22 characters may not allocate heap memory at all.
     */
    StringPoolStats stats() const noexcept
    {
        typename SyncPolicy::ReadLock lock(sync_policy_.getLock());

        StringPoolStats result;
        // Query container directly for exact count under lock
        result.unique_strings = m_strings.size();
        result.total_interns = detail::load_stat(m_stats.total_interns);
        result.content_bytes = detail::load_stat(m_stats.content_bytes);
        result.memory_saved = detail::load_stat(m_stats.memory_saved);

        if (result.total_interns > 0)
        {
            size_t hits = result.total_interns - result.unique_strings;
            result.hit_rate = static_cast<double>(hits) / result.total_interns;
        }

        return result;
    }

    /**
     * @brief Reset statistics to current pool state
     *
     * Sets total_interns to unique_strings count and resets memory_saved to zero.
     * content_bytes is unchanged as it's already accurate from incremental updates.
     */
    void reset_stats()
    {
        typename SyncPolicy::WriteLock lock(sync_policy_.getLock());
        detail::store_stat(m_stats.total_interns, m_strings.size());
        // content_bytes is already accurate from incremental updates in intern()
        detail::store_stat(m_stats.memory_saved, 0);
    }

private:
    detail::StringSet m_strings;

    mutable SyncPolicy sync_policy_;

    struct
    {
        detail::StatType<SyncPolicy, size_t> total_interns{0};
        detail::StatType<SyncPolicy, size_t> content_bytes{0};
        detail::StatType<SyncPolicy, size_t> memory_saved{0};
    } m_stats;
};

/**
 * @brief RAII wrapper for interned string pointer
 *
 * Provides automatic lifetime management and comparison operators.
 * Useful for storing interned strings in containers.
 *
 * @note The hash specialization uses pointer address, not string content.
 * This works correctly only when all handles come from the same StringPool,
 * since interned strings have unique addresses within a pool.
 */
class StringHandle
{
public:
    constexpr StringHandle() noexcept : m_ptr(nullptr) {}

    constexpr StringHandle(const char* ptr) noexcept : m_ptr(ptr) {}

    const char* get() const noexcept
    {
        return m_ptr;
    }

    const char* c_str() const noexcept
    {
        return m_ptr ? m_ptr : "";
    }

    operator const char*() const noexcept
    {
        return m_ptr;
    }

    operator std::string_view() const noexcept
    {
        return m_ptr ? std::string_view(m_ptr) : std::string_view();
    }

    bool operator==(const StringHandle& other) const noexcept
    {
        return m_ptr == other.m_ptr;
    }

    bool operator!=(const StringHandle& other) const noexcept
    {
        return m_ptr != other.m_ptr;
    }

    /**
     * @brief Pointer-based ordering for use in ordered containers
     *
     * Provides O(1) strict weak ordering based on memory address, consistent
     * with operator==. This does NOT provide lexicographic (alphabetical) order.
     *
     * For alphabetical ordering, use a custom comparator:
     * @code
     * auto alpha_less = [](const StringHandle& a, const StringHandle& b) {
     *     return std::strcmp(a.c_str(), b.c_str()) < 0;
     * };
     * std::set<StringHandle, decltype(alpha_less)> sorted_set(alpha_less);
     * @endcode
     *
     * @note std::less<const char*> provides a total order for pointers,
     * correctly handling comparison of unrelated pointers (required by standard).
     */
    bool operator<(const StringHandle& other) const noexcept
    {
        return std::less<const char*>{}(m_ptr, other.m_ptr);
    }

    explicit operator bool() const noexcept
    {
        return m_ptr != nullptr;
    }

private:
    const char* m_ptr;
};

} // namespace fat_p

namespace std
{

/**
 * @brief Hash specialization for StringHandle
 *
 * Hashes by pointer address, not string content. This is correct and efficient
 * for interned strings since identical content always has the same pointer
 * within a single StringPool. However, handles from different pools with
 * the same string content will hash to different buckets.
 */
template <>
struct hash<fat_p::StringHandle>
{
    size_t operator()(const fat_p::StringHandle& handle) const noexcept
    {
        return std::hash<const void*>{}(handle.get());
    }
};

} // namespace std
