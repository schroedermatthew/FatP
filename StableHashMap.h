// StableHashMap.h - High-performance Robin Hood hash map
// Optimized for cache efficiency and minimal per-probe overhead
//
// Features:
// - Robin Hood hashing with backward-shift deletion
// - Adaptive load factor warnings (debug-only)
// - Read-only mode for high-density lookup tables
// - Heterogeneous lookup (find/contains/erase with transparent hash/equal)
// - Policy-based extensibility (custom hash, allocator)
// - Rehash strong exception guarantee (when policy requires noexcept default ctors)
//
#ifndef STABLE_HASH_MAP_H
#define STABLE_HASH_MAP_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// Fat-P Design-by-Contract support
// If enforce.h is available, use it; otherwise provide fallback
#if __has_include("enforce.h")
    #include "enforce.h"
#else
    // Fallback: enforce() compiles out in release, asserts in debug
    // Matches the enforce.h API: enforce(condition, message...)
    #ifndef enforce
        #ifdef NDEBUG
            #define enforce(cond, ...) ((void)0)
        #else
            #define enforce(cond, ...) assert((cond) && "enforce failed")
        #endif
    #endif
#endif

// Forward declaration for white-box testing
namespace fat_p::testing::stablehashmap {
    template <typename MapType> class StableHashMapTester;
}

namespace fat_p
{

// ============================================================================
// Acceleration Policies
// ============================================================================

/**
 * @brief Default policy using standard library implementations
 * 
 * Uses std::hash, std::equal_to, std::allocator.
 * Suitable for all platforms and provides baseline performance.
 * 
 * The Policy template parameter exists for future extensibility.
 * Custom policies can provide alternative hash functions, allocators,
 * or bulk memory operations.
 */
template<typename Key, typename Value>
struct DefaultPolicy
{
    // Type configuration
    using hash_type = std::hash<Key>;
    using key_equal_type = std::equal_to<Key>;
    using allocator_type = std::allocator<std::pair<Key, Value>>;
    
    // Bulk memory operations
    static void bulk_zero(void* dst, size_t bytes) noexcept
    {
        std::memset(dst, 0, bytes);
    }
    
    // Cache line size hint
    static constexpr size_t cache_line_size = 64;

    // =========================================================================
    // Compile-time Safety / Performance Switches
    // =========================================================================

    /**
     * @brief Enforces STRONG rehash exception guarantee
     * 
     * When true: Requires noexcept default constructors for Key and Value.
     *            Guarantees rehash is atomic (either succeeds or *this unchanged).
     * 
     * When false: Allows throwing default constructors.
     *             Provides BASIC exception guarantee (no leaks, valid state).
     */
    static constexpr bool require_nothrow_default_constructible = true;

    /**
     * @brief Enables bulk-zero fast path in clear()
     * 
     * When true: clear() may use memset for trivially copyable types.
     * When false: clear() always uses RAII-safe per-entry reset.
     */
    static constexpr bool enable_bulk_zero_clear = true;

    /**
     * @brief Requires unique object representation for bulk-zero
     * 
     * When true: Bulk-zero only used if Entry has no padding bytes.
     *            This ensures all-bits-zero is a valid representation.
     * 
     * When false: Expert mode - representation assumed valid for bulk-zero.
     *             Faster but technically UB for types with padding.
     * 
     * Only applies if enable_bulk_zero_clear is true.
     */
    static constexpr bool require_unique_object_repr_for_bulk_zero = true;

    /**
     * @brief Human-readable policy identifier for debug diagnostics
     */
    static constexpr const char* policy_name = "DefaultPolicy";
};

/**
 * @brief Policy adapter for custom hash functions
 * 
 * Use this to wrap a custom hash function into a Policy:
 * @code
 *   struct MyHash { size_t operator()(const Key& k) const; };
 *   using MyMap = StableHashMap<Key, Value, CustomHashPolicy<Key, Value, MyHash>>;
 * @endcode
 */
template<typename Key, typename Value, typename Hash, 
         typename KeyEqual = std::equal_to<Key>,
         typename Allocator = std::allocator<std::pair<Key, Value>>>
struct CustomHashPolicy : DefaultPolicy<Key, Value>
{
    using hash_type = Hash;
    using key_equal_type = KeyEqual;
    using allocator_type = Allocator;
};

// ============================================================================
// Policy Presets (for common configurations)
// ============================================================================

/**
 * @brief Maximum Safety Policy - BASIC exception guarantee, no bulk-zero
 * 
 * Use when:
 * - Key/Value have throwing default constructors
 * - Maximum safety is preferred over performance
 * - Types may have padding bytes
 */
template<typename Key, typename Value>
struct SafePolicy : DefaultPolicy<Key, Value>
{
    static constexpr bool require_nothrow_default_constructible = false;
    static constexpr bool enable_bulk_zero_clear = false;
    static constexpr const char* policy_name = "SafePolicy";
};

/**
 * @brief Maximum HPC Performance Policy - Expert mode, no padding check
 * 
 * Use when:
 * - Performance is critical
 * - You guarantee your types are safe for bulk-zero
 * - You understand the representation requirements
 * 
 * @warning Expert-only. Incorrect use may cause subtle UB.
 */
template<typename Key, typename Value>
struct FastHpcPolicy : DefaultPolicy<Key, Value>
{
    static constexpr bool require_unique_object_repr_for_bulk_zero = false;
    static constexpr const char* policy_name = "FastHpcPolicy";
};

/**
 * @section StableHashMap Policy Guarantees Summary
 *
 * 1) require_nothrow_default_constructible
 *    true  (default) -> STRONG rehash exception guarantee (noexcept ctors required)
 *    false           -> BASIC rehash exception guarantee (throwing ctors allowed)
 *
 * 2) enable_bulk_zero_clear
 *    true  (default) -> clear() may bulk-zero storage for trivially copyable types
 *    false           -> clear() always uses RAII-safe per-entry reset
 *
 * 3) require_unique_object_repr_for_bulk_zero
 *    true  (default) -> all-bits-zero validity is enforced via has_unique_object_representations
 *    false           -> expert-only mode, representation assumed valid
 *
 * Recommended Configurations:
 *  - Maximum Safety:       SafePolicy<K,V>
 *  - Balanced (Default):   DefaultPolicy<K,V>
 *  - Maximum Performance:  FastHpcPolicy<K,V>
 */

// ============================================================================
// Debug Banner (prints policy configuration in debug builds)
// ============================================================================

#ifndef NDEBUG
namespace detail
{
    template<typename Policy>
    struct PolicyBanner
    {
        // Use std::call_once to ensure banner prints exactly once per Policy type,
        // and only when explicitly triggered (not at static initialization time).
        static std::once_flag flag_;
        
        static void print_once()
        {
            std::call_once(flag_, [](){
                std::fprintf(stderr,
                    "[StableHashMap Policy Configuration]\n"
                    "  Policy: %s\n"
                    "  Rehash guarantee: %s\n"
                    "  Bulk-zero clear: %s\n"
                    "  Unique repr guard: %s\n\n",
                    Policy::policy_name,
                    Policy::require_nothrow_default_constructible ? "STRONG" : "BASIC",
                    Policy::enable_bulk_zero_clear ? "ENABLED" : "DISABLED",
                    Policy::enable_bulk_zero_clear && Policy::require_unique_object_repr_for_bulk_zero 
                        ? "REQUIRED" : (Policy::enable_bulk_zero_clear ? "EXPERT MODE" : "N/A")
                );
            });
        }
    };
    
    template<typename Policy>
    std::once_flag PolicyBanner<Policy>::flag_;
} // namespace detail
#endif

// ============================================================================
// Heterogeneous Lookup Support
// ============================================================================

namespace detail
{

// Detect if a type has is_transparent member type
template <typename, typename = void>
struct has_is_transparent : std::false_type {};

template <typename T>
struct has_is_transparent<T, std::void_t<typename T::is_transparent>> : std::true_type {};

template <typename T>
inline constexpr bool has_is_transparent_v = has_is_transparent<T>::value;

// Check if both Hash and KeyEqual support transparent lookup
template <typename Hash, typename KeyEqual>
inline constexpr bool is_transparent_v = 
    has_is_transparent_v<Hash> && has_is_transparent_v<KeyEqual>;

} // namespace detail

/**
 * @brief Mutation policy for StableHashMap
 * 
 * ReadOnly mode allows high load factors (up to 0.95) for lookup-only tables.
 * In ReadOnly mode, insert/erase/operator[] are forbidden and will assert.
 */
enum class MutationPolicy
{
    Mutable,   ///< Normal mode: insert, erase, operator[] allowed
    ReadOnly   ///< Read-only mode: only find() allowed, high load factor OK
};

/**
 * @brief Fast hash map using Robin Hood hashing
 * 
 * @tparam Key Key type (must be hashable and equality-comparable)
 * @tparam Value Value type
 * @tparam Policy Configuration policy providing Hash, KeyEqual, Allocator, and
 *         bulk memory operation hooks. Default: DefaultPolicy.
 *         Use CustomHashPolicy for alternative hash functions.
 * 
 * @details Implementation uses:
 * - Open addressing with linear probing
 * - Robin Hood hashing for insert (steals from rich)
 * - Backward-shift deletion (no tombstones)
 * - hash == 0 marks empty slots (no separate bool)
 * - Power-of-two table sizing for fast modulo
 * - Default 0.75 load factor
 * 
 * Performance characteristics (empirically measured):
 * - Find: O(1) average, stable up to 0.95 load factor (~3 ns)
 * - Insert: O(1) average, degrades exponentially above 0.80 load factor
 * - Erase: O(1) average, degrades exponentially above 0.80 load factor
 * 
 * Load Factor Guidelines:
 * - 0.50-0.75: Optimal for mixed workloads (default: 0.75)
 * - 0.75-0.80: Acceptable, slight insert/erase slowdown
 * - 0.80-0.90: WARNING - 10x slower insert/erase
 * - 0.90-0.95: CRITICAL - 70x slower insert/erase, OK for read-only
 * 
 * @warning Not thread-safe - synchronization must be external
 * 
 * @warning Iterator and Reference Invalidation (STRICTER than std::unordered_map):
 * ALL iterators, pointers, and references are invalidated by ANY mutation:
 * insert, insert_or_assign, emplace, try_emplace, erase, clear, rehash,
 * reserve, and operator[] when it causes insertion.
 * 
 * This differs from std::unordered_map where:
 * - Iterators are only invalidated by insert if rehash occurs
 * - References/pointers remain valid even across rehash
 * 
 * DANGER: Do not hold pointers across mutations:
 * @code
 *   int* ptr = map.find(1);
 *   map.insert(2, 2);  // ptr is NOW INVALID
 *   *ptr = 5;          // UNDEFINED BEHAVIOR / CRASH
 * @endcode
 * 
 * @see make_read_only() for high-density lookup tables
 * @see CustomHashPolicy for custom hash function configuration
 */

// ============================================================================
// EXPLICIT NON-GOALS & EXCEPTION SAFETY GUARANTEES
// ============================================================================
//
// This section documents intentional design constraints and boundaries.
// These are NOT bugs - they are deliberate HPC trade-offs.
//
// 1. DESTRUCTOR TIMING
//    StableHashMap uses assignment (Key{}, Value{}) to release resources in
//    erase() and clear(). It does NOT guarantee immediate destructor calls.
//    Types relying on side-effects in destructors (beyond resource release)
//    are not supported.
//
// 2. STRONG REHASH GUARANTEE
//    The "Strong Rehash Guarantee" (DefaultPolicy) requires that Key/Value
//    move operations do NOT throw. This is enforced via static_assert.
//    Rehash operations strictly avoid calling user predicates (KeyEqual)
//    to prevent user-throwing code from violating atomicity.
//
// 3. EXCEPTION SAFETY OF CLEAR/ERASE
//    - If Policy::require_nothrow_default_constructible is TRUE (DefaultPolicy):
//      clear() and erase() are noexcept. Strong guarantee.
//    - If FALSE (SafePolicy): clear() and erase() are NOT noexcept.
//      If a default constructor throws, the map state remains VALID (slots are
//      logically empty), but the contained object's resources might not be
//      fully released until overwrite or map destruction. BASIC guarantee.
//
// 4. ALLOCATOR CONSTRAINTS
//    swap() requires equal allocators. Swapping maps with unequal, stateful
//    allocators that don't propagate on swap is undefined behavior.
//
// 5. BULK-ZERO OPTIMIZATION
//    When enabled (FastHpcPolicy), bulk-zero assumes all-bits-zero is a valid
//    object representation for Key and Value. This is expert-only mode.
//    DefaultPolicy guards this with has_unique_object_representations check.
//
// 6. THREAD SAFETY
//    None. External synchronization is required for concurrent access.
//
// ============================================================================

template <typename Key,
          typename Value,
          typename Policy = DefaultPolicy<Key, Value>>
class StableHashMap
{
    // Extract types from Policy
    using Hash = typename Policy::hash_type;
    using KeyEqual = typename Policy::key_equal_type;
    using Allocator = typename Policy::allocator_type;

    // Require nothrow moves for correctness of Robin Hood and backward-shift
    static_assert(
        std::is_nothrow_move_constructible_v<Key> &&
        std::is_nothrow_move_constructible_v<Value>,
        "StableHashMap requires nothrow move constructible Key and Value."
    );

    static_assert(
        std::is_nothrow_move_assignable_v<Key> &&
        std::is_nothrow_move_assignable_v<Value>,
        "StableHashMap requires nothrow move assignable Key and Value."
    );

    // Storage strategy requires default constructibility (HPC design constraint).
    // PERFORMANCE NOTE: Default construction occurs for every empty slot in the
    // bucket array. Ensure Key and Value default constructors are cheap (ideally
    // trivial or zero-init). Expensive default constructors will degrade performance.
    static_assert(
        std::is_default_constructible_v<Key> &&
        std::is_default_constructible_v<Value>,
        "StableHashMap requires DefaultConstructible Key and Value. "
        "This is an intentional HPC design constraint for optimal memory layout."
    );

    // Optional enforcement of noexcept default constructors for STRONG rehash guarantee
    static_assert(
        !Policy::require_nothrow_default_constructible ||
        (std::is_nothrow_default_constructible_v<Key> &&
         std::is_nothrow_default_constructible_v<Value>),
        "Policy requires noexcept default constructors for STRONG rehash exception guarantee. "
        "Either make Key/Value default constructors noexcept, or set "
        "Policy::require_nothrow_default_constructible = false for BASIC guarantee."
    );

    // Require hasher to return size_t (or convertible to size_t)
    static_assert(
        std::is_invocable_r_v<size_t, Hash, const Key&>,
        "StableHashMap requires Hash callable as size_t(const Key&)."
    );

public:
    // =========================================================================
    // Type Aliases
    // =========================================================================
    
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using policy_type = Policy;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;

    // =========================================================================
    // Load Factor Thresholds (empirically determined)
    // =========================================================================

    /// Warning threshold - insert/erase begin to degrade
    static constexpr float LOAD_FACTOR_WARNING = 0.80f;

    /// Critical threshold - pathological insert/erase costs
    static constexpr float LOAD_FACTOR_CRITICAL = 0.90f;

    // Friend declaration for white-box testing of internal invariants
    // (probe distances, tombstone absence, etc.)
    template <typename>
    friend class fat_p::testing::stablehashmap::StableHashMapTester;

private:
    // Entry layout optimized for cache: hash first for quick empty check
    // hash == 0 means empty slot (actual hash 0 is remapped to 1)
    struct Entry
    {
        size_t hash;    // 0 = empty, nonzero = occupied
        Key key;
        Value value;

        // Default constructor: initializes hash to 0 (empty), default-constructs key/value.
        // Exception safety: If Policy::require_nothrow_default_constructible is true (default),
        // Key() and Value() are guaranteed noexcept at compile time. Otherwise may throw.
        Entry() : hash(0) {}

        bool occupied() const noexcept { return hash != 0; }
        void clear() noexcept { hash = 0; }
    };

    // Rebind allocator for Entry storage
    using entry_allocator_type = 
        typename std::allocator_traits<Allocator>::template rebind_alloc<Entry>;

    std::vector<Entry, entry_allocator_type> buckets_;
    size_t num_elements_ = 0;
    size_t mask_ = 0;
    float max_load_factor_ = 0.75f;
    Hash hasher_;
    KeyEqual key_equal_;
    MutationPolicy mutation_policy_ = MutationPolicy::Mutable;

#ifndef NDEBUG
    mutable bool warned_high_load_ = false;
#endif

    static constexpr size_t MIN_CAPACITY = 16;

    // =========================================================================
    // Debug Diagnostics
    // =========================================================================

    /// Check load factor and warn/assert in debug builds (never in find())
    void check_load_factor_diagnostics() const
    {
#ifndef NDEBUG
        // Skip diagnostics in read-only mode (user explicitly accepts high load)
        if (mutation_policy_ == MutationPolicy::ReadOnly)
        {
            return;
        }

        const float lf = load_factor();

        // Warn once when exceeding warning threshold
        // Only warn if user hasn't explicitly set a high max_load_factor
        if (lf > LOAD_FACTOR_WARNING && max_load_factor_ <= LOAD_FACTOR_WARNING && !warned_high_load_)
        {
            warned_high_load_ = true;
            std::fprintf(
                stderr,
                "[StableHashMap] Warning: load factor %.2f exceeds %.2f.\n"
                "Insert/erase costs degrade exponentially beyond this point.\n"
                "Consider: reserve(), lower max_load_factor(), or make_read_only().\n",
                lf, LOAD_FACTOR_WARNING
            );
        }

        // Hard assert at critical threshold
        // Only assert if user hasn't explicitly set a high max_load_factor
        // (if they set max_load_factor = 0.95, they know what they're doing)
        if (lf > LOAD_FACTOR_CRITICAL && max_load_factor_ <= LOAD_FACTOR_CRITICAL)
        {
            assert(false && 
                "StableHashMap load factor > 0.90 causes pathological insert/erase costs. "
                "Use make_read_only() if this is a lookup-only table.");
        }
#endif
    }

    /// Assert that mutations are allowed (debug-only, zero overhead in release)
    /// 
    /// In debug builds: triggers assertion failure with diagnostic message
    /// In release builds: no-op (zero overhead)
    /// 
    /// @note Read-only mode is a development/debugging aid, not a security feature.
    ///       For production safety, control access at the API level.
    void assert_mutable() const noexcept
    {
        assert(mutation_policy_ == MutationPolicy::Mutable &&
               "StableHashMap mutation attempted in read-only mode. "
               "Use MutationPolicy::Mutable or don't call make_read_only()/freeze().");
    }

    // =========================================================================
    // Internal Helpers
    // =========================================================================

    // Hash key, ensuring result is never 0 (0 = empty marker)
    size_t hash_key(const Key& k) const
    {
        size_t h = hasher_(k);
        return h ? h : 1;  // Remap 0 -> 1
    }

    // Templated hash_key for heterogeneous lookup
    template <typename K>
    size_t hash_key_transparent(const K& k) const
    {
        size_t h = hasher_(k);
        return h ? h : 1;  // Remap 0 -> 1
    }

    size_t probe_distance(size_t hash_val, size_t slot) const
    {
        size_t ideal = hash_val & mask_;
        return (slot + buckets_.size() - ideal) & mask_;
    }

    static size_t next_power_of_2(size_t n)
    {
        if (n < MIN_CAPACITY)
        {
            return MIN_CAPACITY;
        }
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(size_t) > 4)
        {
            n |= n >> 32;
        }
        return n + 1;
    }

    void rehash_to(size_t new_cap)
    {
        // P0 FIX: Ensure capacity can hold all existing elements
        // Without this check, rehash(1) on a map with 100 elements would cause
        // insert_internal to loop infinitely trying to fit 100 elements into 16 slots.
        if (num_elements_ > 0 && max_load_factor_ > 0.0f)
        {
            size_t min_buckets = static_cast<size_t>(num_elements_ / max_load_factor_) + 1;
            new_cap = std::max(new_cap, min_buckets);
        }
        
        new_cap = next_power_of_2(std::max(new_cap, MIN_CAPACITY));
        
        // Exception Safety (STRONG guarantee):
        // 1. Capture allocator and allocate new storage FIRST
        // 2. If allocation throws, buckets_ is untouched
        // 3. Only then move old data out
        //
        // Allocator-safe: explicitly use same allocator instance for new storage
        const entry_allocator_type alloc = buckets_.get_allocator();
        
        std::vector<Entry, entry_allocator_type> fresh(alloc);
        fresh.resize(new_cap);  // May throw std::bad_alloc or Key()/Value() - buckets_ untouched
        
        // Now safe to move old data out (allocation succeeded)
        std::vector<Entry, entry_allocator_type> old_buckets(std::move(buckets_));
        
        // Install fresh storage (same allocator, so move-assign is safe)
        buckets_ = std::move(fresh);
        mask_ = new_cap - 1;
        num_elements_ = 0;

        // Re-insert all elements from old data using insert_on_rehash
        // This function never calls key_equal_ (user code), preserving STRONG guarantee.
        // Safe because all keys are guaranteed unique from the original map.
        for (Entry& e : old_buckets)
        {
            if (e.occupied())
            {
                insert_on_rehash(std::move(e.key), std::move(e.value), e.hash);
            }
        }

        check_load_factor_diagnostics();
        
        // Post-condition: load factor should be valid after rehash
        enforce(load_factor() <= max_load_factor_,
                "StableHashMap invariant violated: load factor exceeds max after rehash");
    }

    // Insert with upsert semantics (used by insert_or_assign)
    // If key exists, overwrites value. Returns slot index.
    size_t insert_internal(Key&& k, Value&& v, size_t h)
    {
        size_t slot = h & mask_;
        size_t dist = 0;
        const size_t cap = bucket_count();

        // Belt-and-suspenders: probe limit prevents infinite loop even if
        // load factor invariants are somehow violated. Should never trigger
        // in normal operation due to 0.99 max load factor clamp.
        while (dist < cap)
        {
            Entry& e = buckets_[slot];

            if (!e.occupied())
            {
                // Commit point: write key/value first, hash last
                e.key = std::move(k);
                e.value = std::move(v);
                e.hash = h;  // Publishing hash marks slot occupied
                ++num_elements_;
                return slot;
            }

            // Update existing key (upsert behavior)
            if (e.hash == h && key_equal_(e.key, k))
            {
                e.value = std::move(v);
                return slot;
            }

            // Robin Hood: steal from the rich
            size_t existing_dist = probe_distance(e.hash, slot);
            if (dist > existing_dist)
            {
                std::swap(k, e.key);
                std::swap(v, e.value);
                std::swap(h, e.hash);
                dist = existing_dist;
            }

            slot = (slot + 1) & mask_;
            ++dist;
        }
        
        // Should never reach here if load factor invariants are maintained
        assert(false && "insert_internal: probe limit exceeded - table invariant violated");
        return SIZE_MAX;
    }

    // Rehash-only insertion: assumes all keys are unique from original map.
    // IMPORTANT: Never calls key_equal_ (user code), preserving STRONG rehash guarantee.
    // This is safe because we're reinserting elements that were already unique in the
    // original map - no duplicates are possible.
    void insert_on_rehash(Key&& k, Value&& v, size_t h)
    {
        size_t slot = h & mask_;
        size_t dist = 0;
        const size_t cap = bucket_count();

        while (dist < cap)
        {
            Entry& e = buckets_[slot];

            if (!e.occupied())
            {
                // Commit point: write key/value first, hash last
                e.key = std::move(k);
                e.value = std::move(v);
                e.hash = h;
                ++num_elements_;
                return;
            }

            // NO key_equal_ CHECK - keys are guaranteed unique from original map
            // Standard Robin Hood swap
            const size_t existing_dist = probe_distance(e.hash, slot);
            if (dist > existing_dist)
            {
                std::swap(k, e.key);
                std::swap(v, e.value);
                std::swap(h, e.hash);
                dist = existing_dist;
            }

            slot = (slot + 1) & mask_;
            ++dist;
        }

        assert(false && "insert_on_rehash: probe limit exceeded - table invariant violated");
    }

    // Insert assuming key is NEW (caller must verify key doesn't exist)
    // Used by insert() after find() confirms key is absent.
    // Slightly faster than insert_internal because we skip equality checks
    // after Robin Hood swaps (swapped entries are already in the table).
    size_t insert_internal_new(Key&& k, Value&& v, size_t h)
    {
        size_t slot = h & mask_;
        size_t dist = 0;
        bool swapped = false;  // Track if we've done any Robin Hood swaps
        const size_t cap = bucket_count();

        // Belt-and-suspenders: probe limit prevents infinite loop
        while (dist < cap)
        {
            Entry& e = buckets_[slot];

            if (!e.occupied())
            {
                // Commit point: write key/value first, hash last
                e.key = std::move(k);
                e.value = std::move(v);
                e.hash = h;  // Publishing hash marks slot occupied
                ++num_elements_;
                return slot;
            }

            // Only check for existing key before first swap
            // After swap, we're placing an evicted entry (which is already unique)
            if (!swapped && e.hash == h && key_equal_(e.key, k))
            {
                // Should not happen if caller verified key doesn't exist
                // But handle gracefully: don't overwrite, just return
                return slot;
            }

            // Robin Hood: steal from the rich
            size_t existing_dist = probe_distance(e.hash, slot);
            if (dist > existing_dist)
            {
                std::swap(k, e.key);
                std::swap(v, e.value);
                std::swap(h, e.hash);
                dist = existing_dist;
                swapped = true;
            }

            slot = (slot + 1) & mask_;
            ++dist;
        }
        
        // Should never reach here if load factor invariants are maintained
        assert(false && "insert_internal_new: probe limit exceeded - table invariant violated");
        return SIZE_MAX;
    }

    // -------------------------------------------------------------------------
    // PROBING STRATEGY DESIGN DECISION
    // -------------------------------------------------------------------------
    // Early-exit probing and software prefetching were evaluated and rejected.
    // In practice, both increased branch or memory pressure and degraded
    // performance on Linux VM and Windows systems. StableHashMap therefore
    // favors predictable, branch-light linear probing and relies on hardware
    // prefetching.
    //
    // Specifics:
    // - Early-exit optimization: Checking probe_distance to bail early on miss
    //   adds per-probe calculations that hurt hit performance more than they
    //   help miss performance. Modern branch predictors handle the simple
    //   "keep probing" pattern efficiently.
    // - Software prefetching: __builtin_prefetch() on upcoming slots added
    //   memory bandwidth pressure without measurable latency improvement.
    //   Hardware prefetchers already detect linear access patterns.
    //
    // Trade-off: Miss performance is dominated by cluster length; hit
    // performance remains competitive with more complex schemes.
    // -------------------------------------------------------------------------
    
    size_t find_slot(const Key& k) const
    {
        if (bucket_count() == 0)
        {
            return SIZE_MAX;
        }

        const size_t h = hash_key(k);
        size_t slot = h & mask_;
        size_t probes = 0;
        const size_t cap = bucket_count();

        // P0 FIX: Added probe counter to prevent infinite loop if table is 100% full
        while (probes < cap)
        {
            const Entry& e = buckets_[slot];

            // Empty slot = not found
            if (!e.occupied())
            {
                return SIZE_MAX;
            }

            // Found it
            if (e.hash == h && key_equal_(e.key, k))
            {
                return slot;
            }

            slot = (slot + 1) & mask_;
            ++probes;
        }
        
        // Table completely full and key not found
        return SIZE_MAX;
    }

    // Templated find_slot for heterogeneous lookup
    template <typename K>
    size_t find_slot_transparent(const K& k) const
    {
        // P0 FIX: Ensure Hash is actually callable with K
        // Without this, users get a hard error inside hash_key_transparent
        // instead of a clean SFINAE failure at the call site.
        static_assert(
            std::is_invocable_r_v<size_t, Hash, const K&>,
            "Heterogeneous lookup requires Hash to be callable as size_t(const K&). "
            "Ensure your Hash type has is_transparent and an overload for this key type."
        );
        
        // Ensure KeyEqual is callable with (Key, K)
        static_assert(
            std::is_invocable_r_v<bool, KeyEqual, const Key&, const K&>,
            "Heterogeneous lookup requires KeyEqual to be callable as bool(const Key&, const K&). "
            "Ensure your KeyEqual type has is_transparent and an overload for this key type."
        );
        
        if (bucket_count() == 0)
        {
            return SIZE_MAX;
        }

        const size_t h = hash_key_transparent(k);
        size_t slot = h & mask_;
        size_t probes = 0;
        const size_t cap = bucket_count();

        // P0 FIX: Added probe counter to prevent infinite loop if table is 100% full
        while (probes < cap)
        {
            const Entry& e = buckets_[slot];

            // Empty slot = not found
            if (!e.occupied())
            {
                return SIZE_MAX;
            }

            // Found it
            if (e.hash == h && key_equal_(e.key, k))
            {
                return slot;
            }

            slot = (slot + 1) & mask_;
            ++probes;
        }
        
        // Table completely full and key not found
        return SIZE_MAX;
    }

    // Internal helper: Find slot using a pre-calculated hash.
    // Optimized for heterogeneous try_emplace to avoid re-hashing.
    // INVARIANT: Caller must ensure h == hash_key_transparent(k)
    template <typename K>
    size_t find_slot_with_hash(const K& k, size_t h) const
    {
        if (bucket_count() == 0)
        {
            return SIZE_MAX;
        }

        size_t slot = h & mask_;
        size_t probes = 0;
        const size_t cap = bucket_count();

        while (probes < cap)
        {
            const Entry& e = buckets_[slot];

            if (!e.occupied())
            {
                return SIZE_MAX;
            }

            if (e.hash == h && key_equal_(e.key, k))
            {
                return slot;
            }

            slot = (slot + 1) & mask_;
            ++probes;
        }
        
        return SIZE_MAX;
    }

    // Insert with "no-overwrite" semantics (std::unordered_map::insert-like).
    // Returns {slot, inserted}. If key exists, inserted=false and map is unchanged.
    // Single-pass: avoids separate find() then insert() double-probe.
    //
    // Defensive ordering: hash is written LAST as the "commit point".
    // If key/value assignment threw (not possible with current static_asserts,
    // but defensive against future relaxation), slot remains empty.
    std::pair<size_t, bool> insert_internal_no_overwrite(Key&& k, Value&& v, size_t h)
    {
        size_t slot = h & mask_;
        size_t dist = 0;
        bool swapped = false;
        const size_t cap = bucket_count();

        while (dist < cap)
        {
            Entry& e = buckets_[slot];

            if (!e.occupied())
            {
                // Commit point: write key/value first, hash last
                e.key = std::move(k);
                e.value = std::move(v);
                e.hash = h;  // Publishing hash marks slot occupied
                ++num_elements_;
                return {slot, true};
            }

            // Only check for duplicate before first Robin Hood swap
            // After swap, we're placing an evicted entry (already unique)
            if (!swapped && e.hash == h && key_equal_(e.key, k))
            {
                return {slot, false};  // Key exists, no insertion
            }

            const size_t existing_dist = probe_distance(e.hash, slot);
            if (dist > existing_dist)
            {
                std::swap(k, e.key);
                std::swap(v, e.value);
                std::swap(h, e.hash);
                dist = existing_dist;
                swapped = true;
            }

            slot = (slot + 1) & mask_;
            ++dist;
        }

        assert(false && "insert_internal_no_overwrite: probe limit exceeded");
        return {SIZE_MAX, false};
    }

public:
    // =========================================================================
    // Construction
    // =========================================================================

    explicit StableHashMap(size_t initial_cap = MIN_CAPACITY, 
                           float load_factor = 0.75f)
        : max_load_factor_(load_factor)
    {
#ifndef NDEBUG
        // Trigger policy configuration banner (prints once per Policy type)
        detail::PolicyBanner<Policy>::print_once();
#endif

        // Validate load_factor: must be in range (0, 1]
        if (!(load_factor > 0.0f && load_factor <= 1.0f))
        {
            throw std::invalid_argument(
                "StableHashMap::max_load_factor must be in range (0, 1]. "
                "Received invalid value (zero, negative, >1, or NaN).");
        }
        
        // Clamp to 0.99 to guarantee at least one empty slot exists
        // when insert_internal is called (belt-and-suspenders safety)
        constexpr float MAX_SAFE_LOAD = 0.99f;
        max_load_factor_ = std::min(max_load_factor_, MAX_SAFE_LOAD);
        
        size_t cap = next_power_of_2(std::max(initial_cap, MIN_CAPACITY));
        buckets_.resize(cap);
        mask_ = cap - 1;
    }

    StableHashMap(const StableHashMap&) = default;
    StableHashMap(StableHashMap&&) noexcept = default;
    StableHashMap& operator=(const StableHashMap&) = default;
    StableHashMap& operator=(StableHashMap&&) noexcept = default;
    ~StableHashMap() = default;

    /// Get the allocator
    allocator_type get_allocator() const noexcept 
    { 
        return buckets_.get_allocator(); 
    }

    // =========================================================================
    // Mutation Policy
    // =========================================================================

    /**
     * @brief Enable read-only mode for high-density lookup tables
     * 
     * In read-only mode:
     * - insert(), erase(), operator[] are forbidden (assert in debug)
     * - Load factor warnings are suppressed
     * - Safe to use load factors up to 0.95 for find-only workloads
     * 
     * Use cases:
     * - Immutable lookup tables
     * - Prebuilt caches
     * - Static configuration maps
     * - Read-mostly hot paths
     * 
     * @return Reference to this map (enables fluent chaining)
     * @note Cannot be reverted - create a new map if mutations needed
     */
    StableHashMap& make_read_only() noexcept
    {
        mutation_policy_ = MutationPolicy::ReadOnly;
        return *this;
    }

    /**
     * @brief Alias for make_read_only()
     * 
     * Shorter name for those who prefer it. Semantically identical.
     * 
     * @return Reference to this map (enables fluent chaining)
     */
    StableHashMap& freeze() noexcept
    {
        return make_read_only();
    }

    /**
     * @brief Check if map is in read-only mode
     */
    bool is_read_only() const noexcept
    {
        return mutation_policy_ == MutationPolicy::ReadOnly;
    }

    /**
     * @brief Get current mutation policy
     */
    MutationPolicy mutation_policy() const noexcept
    {
        return mutation_policy_;
    }

    // =========================================================================
    // Insert
    // =========================================================================

    /**
     * @brief Insert a key-value pair (does NOT overwrite existing keys)
     * @param k Key to insert
     * @param v Value to insert
     * @return true if inserted, false if key already existed
     * 
     * Matches std::unordered_map::insert semantics: if key exists, does nothing.
     * Use insert_or_assign() if you want upsert (overwrite) behavior.
     * 
     * Optimized: Single-pass probe (no separate find() then insert()).
     */
    bool insert(const Key& k, const Value& v)
    {
        assert_mutable();

        if (num_elements_ + 1 > static_cast<size_t>(bucket_count() * max_load_factor_))
        {
            rehash_to(bucket_count() * 2);
        }
        
        const size_t h = hash_key(k);
        const auto [slot, inserted] = insert_internal_no_overwrite(Key(k), Value(v), h);
        
        if (inserted)
        {
            check_load_factor_diagnostics();
        }
        return inserted;
    }

    /**
     * @brief Insert a key-value pair (move version, does NOT overwrite)
     * @return true if inserted, false if key already existed
     * 
     * Optimized: Single-pass probe (no separate find() then insert()).
     */
    bool insert(Key&& k, Value&& v)
    {
        assert_mutable();

        if (num_elements_ + 1 > static_cast<size_t>(bucket_count() * max_load_factor_))
        {
            rehash_to(bucket_count() * 2);
        }
        
        const size_t h = hash_key(k);
        const auto [slot, inserted] = insert_internal_no_overwrite(std::move(k), std::move(v), h);
        
        if (inserted)
        {
            check_load_factor_diagnostics();
        }
        return inserted;
    }

    /**
     * @brief Insert or assign (upsert with return info)
     * @param k Key to insert or update
     * @param v Value to assign
     * @return Pair of (pointer to value, true if inserted / false if assigned)
     *
     * Single-pass implementation: computes hash once and uses insert_internal
     * directly, which handles both insert and update in one probe sequence.
     */
    template <typename V>
    std::pair<Value*, bool> insert_or_assign(const Key& k, V&& v)
    {
        assert_mutable();

        if (num_elements_ + 1 > static_cast<size_t>(bucket_count() * max_load_factor_))
        {
            rehash_to(bucket_count() * 2);
        }

        const size_t old_size = num_elements_;
        const size_t h = hash_key(k);
        size_t slot = insert_internal(Key(k), Value(std::forward<V>(v)), h);

        const bool inserted = (num_elements_ > old_size);
        if (inserted)
        {
            check_load_factor_diagnostics();
        }
        return {&buckets_[slot].value, inserted};
    }

    /**
     * @brief Try to emplace (insert only if key doesn't exist)
     * @param k Key to insert
     * @param args Arguments forwarded to Value constructor
     * @return Pair of (pointer to value, true if inserted / false if key existed)
     * 
     * Unlike insert(), this does NOT overwrite existing values.
     * Matches std::unordered_map::try_emplace semantics.
     *
     * Single-pass implementation: computes hash once, checks existence with
     * find_slot_with_hash, then inserts using insert_internal_no_overwrite.
     */
    template <typename... Args>
    std::pair<Value*, bool> try_emplace(const Key& k, Args&&... args)
    {
        assert_mutable();

        if (num_elements_ + 1 > static_cast<size_t>(bucket_count() * max_load_factor_))
        {
            rehash_to(bucket_count() * 2);
        }

        const size_t h = hash_key(k);
        auto [slot, inserted] = insert_internal_no_overwrite(
            Key(k), Value(std::forward<Args>(args)...), h);

        if (inserted)
        {
            check_load_factor_diagnostics();
        }
        return {&buckets_[slot].value, inserted};
    }

    /**
     * @brief Heterogeneous try_emplace - avoids Key construction if key exists
     * @param k Lookup key (can be different type, e.g., string_view for string keys)
     * @param args Arguments forwarded to Value constructor
     * @return Pair of (pointer to value, true if inserted / false if key existed)
     * 
     * This is the key optimization for heterogeneous lookup:
     * - Lookup uses K directly (no allocation)
     * - Key is only constructed if insertion actually happens
     * - Hash is computed ONCE on the transparent key and reused for insertion
     * 
     * Example:
     *   StableHashMap<std::string, int, TransparentPolicy> map;
     *   map.try_emplace("hello", 42);  // No std::string created if "hello" exists
     * 
     * Enabled only when Hash and KeyEqual both have is_transparent member type.
     * 
     * INVARIANT: Transparent hash must satisfy Hash(k) == Hash(Key(k))
     * This is required for heterogeneous lookup correctness anyway.
     */
    template <typename K, typename... Args,
              std::enable_if_t<detail::is_transparent_v<Hash, KeyEqual> &&
                               !std::is_same_v<std::decay_t<K>, Key>, int> = 0>
    std::pair<Value*, bool> try_emplace(K&& k, Args&&... args)
    {
        assert_mutable();

        // 1. Calculate hash ONCE using the transparent key
        const size_t h = hash_key_transparent(k);

        // 2. Lookup using the pre-calculated hash
        size_t slot = find_slot_with_hash(k, h);
        if (slot != SIZE_MAX)
        {
            return {&buckets_[slot].value, false};  // Key exists, don't overwrite
        }

        // 3. Key not found - construct Key and insert using the SAME hash
        if (num_elements_ + 1 > static_cast<size_t>(bucket_count() * max_load_factor_))
        {
            rehash_to(bucket_count() * 2);
            // Note: rehash invalidates slots but hash 'h' remains valid
        }
        
        // Construct Key from K. We reuse 'h' - no second hash computation.
        Key key_copy(std::forward<K>(k));
        auto [inserted_slot, inserted] = insert_internal_no_overwrite(
            std::move(key_copy), Value(std::forward<Args>(args)...), h);
        
        if (inserted)
        {
            check_load_factor_diagnostics();
        }
        
        return {&buckets_[inserted_slot].value, inserted};
    }

    /**
     * @brief Emplace (construct value in place) - UPSERT semantics
     * @param k Key to insert or update
     * @param args Arguments forwarded to Value constructor
     * @return Pair of (pointer to value, true if inserted / false if updated)
     * 
     * Note: Unlike std::unordered_map::emplace, this OVERWRITES existing values.
     * Use try_emplace() for insert-only behavior.
     *
     * Single-pass implementation: computes hash once and uses insert_internal
     * directly, which handles both insert and update in one probe sequence.
     */
    template <typename... Args>
    std::pair<Value*, bool> emplace(const Key& k, Args&&... args)
    {
        assert_mutable();

        if (num_elements_ + 1 > static_cast<size_t>(bucket_count() * max_load_factor_))
        {
            rehash_to(bucket_count() * 2);
        }

        const size_t old_size = num_elements_;
        const size_t h = hash_key(k);
        size_t slot = insert_internal(Key(k), Value(std::forward<Args>(args)...), h);

        const bool inserted = (num_elements_ > old_size);
        if (inserted)
        {
            check_load_factor_diagnostics();
        }
        return {&buckets_[slot].value, inserted};
    }

    // =========================================================================
    // Find (always allowed, even in read-only mode)
    // =========================================================================

    Value* find(const Key& k)
    {
        size_t slot = find_slot(k);
        return slot != SIZE_MAX ? &buckets_[slot].value : nullptr;
    }

    const Value* find(const Key& k) const
    {
        size_t slot = find_slot(k);
        return slot != SIZE_MAX ? &buckets_[slot].value : nullptr;
    }

    /**
     * @brief Heterogeneous lookup (transparent find)
     * 
     * Enabled only when Hash and KeyEqual both have is_transparent member type.
     * Allows finding keys without constructing a Key object.
     * 
     * Example with std::string keys:
     * @code
     *   StableHashMap<std::string, int, StringHash, StringEqual> map;
     *   map.find("hello");  // No std::string temporary created
     * @endcode
     * 
     * @tparam K The lookup key type (e.g., const char* for std::string keys)
     * @param k The key to search for
     * @return Pointer to value if found, nullptr otherwise
     */
    template <typename K,
              std::enable_if_t<detail::is_transparent_v<Hash, KeyEqual> &&
                               !std::is_same_v<std::decay_t<K>, Key>, int> = 0>
    Value* find(const K& k)
    {
        size_t slot = find_slot_transparent(k);
        return slot != SIZE_MAX ? &buckets_[slot].value : nullptr;
    }

    template <typename K,
              std::enable_if_t<detail::is_transparent_v<Hash, KeyEqual> &&
                               !std::is_same_v<std::decay_t<K>, Key>, int> = 0>
    const Value* find(const K& k) const
    {
        size_t slot = find_slot_transparent(k);
        return slot != SIZE_MAX ? &buckets_[slot].value : nullptr;
    }

    // =========================================================================
    // Contains (always allowed, even in read-only mode)
    // =========================================================================

    /**
     * @brief Check if key exists in map
     * @param k The key to check
     * @return true if key exists, false otherwise
     */
    bool contains(const Key& k) const
    {
        return find_slot(k) != SIZE_MAX;
    }

    /**
     * @brief Heterogeneous contains (transparent lookup)
     * 
     * Enabled only when Hash and KeyEqual both have is_transparent member type.
     */
    template <typename K,
              std::enable_if_t<detail::is_transparent_v<Hash, KeyEqual> &&
                               !std::is_same_v<std::decay_t<K>, Key>, int> = 0>
    bool contains(const K& k) const
    {
        return find_slot_transparent(k) != SIZE_MAX;
    }

    // =========================================================================
    // Erase
    // =========================================================================

    bool erase(const Key& k)
    {
        assert_mutable();

        size_t slot = find_slot(k);
        if (slot == SIZE_MAX)
        {
            return false;
        }

        erase_slot(slot);
        return true;
    }

    /**
     * @brief Heterogeneous erase (transparent lookup)
     * 
     * Enabled only when Hash and KeyEqual both have is_transparent member type.
     */
    template <typename K,
              std::enable_if_t<detail::is_transparent_v<Hash, KeyEqual> &&
                               !std::is_same_v<std::decay_t<K>, Key>, int> = 0>
    bool erase(const K& k)
    {
        assert_mutable();

        size_t slot = find_slot_transparent(k);
        if (slot == SIZE_MAX)
        {
            return false;
        }

        erase_slot(slot);
        return true;
    }

private:
    // Internal helper for slot-based erase (used by both regular and heterogeneous erase)
    //
    // IMPORTANT LIFETIME NOTE:
    // erase_slot() guarantees logical erasure and resource release via assignment
    // (Key{}, Value{}). It does NOT guarantee destructor invocation.
    //
    // For types like shared_ptr, assignment to default decrements refcount correctly.
    // For types requiring destructor side effects beyond what assignment provides
    // (e.g., self-referential types, intrusive containers), this may be insufficient.
    //
    // This matches std::unordered_map's allowance for assignment-based cleanup.
    // Use SafePolicy and avoid bulk operations if strict destructor semantics are required.
    //
    // Exception Safety (invariant-first):
    // The slot is marked empty (hash=0) and size decremented BEFORE potentially-throwing
    // assignments. This ensures the map is always in a valid state, even if Key{} or
    // Value{} throws with SafePolicy. Resource release may be deferred in that case.
    void erase_slot(size_t slot)
    {
        // Backward-shift deletion (noexcept - uses move operations)
        size_t curr = slot;
        size_t next = (curr + 1) & mask_;

        while (buckets_[next].occupied() && probe_distance(buckets_[next].hash, next) > 0)
        {
            buckets_[curr] = std::move(buckets_[next]);
            curr = next;
            next = (next + 1) & mask_;
        }

        // Invariant-first: make slot empty and update size BEFORE any throwing ops.
        // This ensures map state is valid even if Key{}/Value{} throws.
        buckets_[curr].clear();  // Mark slot empty (hash=0)
        --num_elements_;
        
        // Best-effort resource release (may throw with SafePolicy types)
        buckets_[curr].key = Key{};
        buckets_[curr].value = Value{};
        
        // Post-condition: size invariant
        enforce(num_elements_ <= buckets_.size(),
                "StableHashMap invariant violated: size > capacity after erase");
    }

public:

    // =========================================================================
    // Capacity
    // =========================================================================

    size_t size() const noexcept
    {
        return num_elements_;
    }

    bool empty() const noexcept
    {
        return num_elements_ == 0;
    }

    float load_factor() const noexcept
    {
        return bucket_count() > 0 ? static_cast<float>(num_elements_) / bucket_count() : 0.0f;
    }

    float max_load_factor() const noexcept
    {
        return max_load_factor_;
    }

    /**
     * @brief Set maximum load factor
     * @param ml New max load factor (must be in range (0, 1])
     * @throws std::invalid_argument if ml is invalid (<=0, >1, or NaN)
     * @note Values are clamped to 0.99 maximum to guarantee at least one empty
     *       slot, ensuring insert_internal's Robin Hood loop always terminates.
     */
    void max_load_factor(float ml)
    {
        // Validate: must be positive, <= 1.0, and not NaN
        if (!(ml > 0.0f && ml <= 1.0f))
        {
            throw std::invalid_argument(
                "StableHashMap::max_load_factor must be in range (0, 1]. "
                "Received invalid value (zero, negative, >1, or NaN).");
        }
        // Clamp to 0.99 to guarantee at least one empty slot exists
        // when insert_internal is called (belt-and-suspenders safety)
        constexpr float MAX_SAFE_LOAD = 0.99f;
        max_load_factor_ = std::min(ml, MAX_SAFE_LOAD);
    }

    /**
     * @brief Returns the number of buckets
     */
    size_t bucket_count() const noexcept
    {
        return buckets_.size();
    }

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Remove all elements from the map
     * 
     * Lifetime model (differs from erase()):
     * - For trivially destructible/copyable types with bulk-zero enabled:
     *   Memory is zeroed directly via memset. No destructors are called.
     *   This assumes types have no externally visible destructor side effects.
     * - For other types: Each entry is reset via assignment (Key{}, Value{}).
     * 
     * Exception Safety:
     * - If bulk-zero path is taken: noexcept (no user code runs)
     * - If slow path AND defaults are nothrow: noexcept
     * - If slow path AND defaults can throw: NOT noexcept, but provides BASIC guarantee
     *   (map remains valid; slots are logically empty before potentially-throwing assignments)
     * 
     * @note This asymmetry is intentional for performance. Use SafePolicy if
     *       you require consistent RAII semantics for all operations.
     */
    void clear() noexcept(
        // Bulk-zero path is always noexcept
        (Policy::enable_bulk_zero_clear &&
         std::is_trivially_destructible_v<Entry> &&
         std::is_trivially_copyable_v<Entry> &&
         (!Policy::require_unique_object_repr_for_bulk_zero ||
          std::has_unique_object_representations_v<Entry>)) ||
        // Slow path is noexcept if defaults don't throw
        (std::is_nothrow_default_constructible_v<Key> &&
         std::is_nothrow_default_constructible_v<Value>))
    {
        assert_mutable();

        // Fast path for trivially copyable types, controlled by Policy switches
        // 
        // Conditions for bulk-zero:
        // 1. Policy::enable_bulk_zero_clear must be true
        // 2. Entry must be trivially destructible (no cleanup needed)
        // 3. Entry must be trivially copyable (memset is valid)
        // 4. Either Policy::require_unique_object_repr_for_bulk_zero is false (expert mode),
        //    OR Entry has unique object representations (no padding bytes)
        if constexpr (Policy::enable_bulk_zero_clear &&
                      std::is_trivially_destructible_v<Entry> &&
                      std::is_trivially_copyable_v<Entry> &&
                      (!Policy::require_unique_object_repr_for_bulk_zero ||
                       std::has_unique_object_representations_v<Entry>))
        {
            // Ensure layout assumption: hash at offset 0 so zeroing marks slots empty
            static_assert(offsetof(Entry, hash) == 0,
                          "Entry::hash must be at offset 0 for bulk_zero optimization");
            
            // Fast path: bulk zero entire bucket array
            // Safe because:
            // - Entry's empty state is hash=0
            // - Trivially copyable means memset is valid
            // - Unique object representation means no padding UB
            Policy::bulk_zero(buckets_.data(), buckets_.size() * sizeof(Entry));
            num_elements_ = 0;
        }
        else
        {
            // Invariant-first slow path:
            // 1. Set size to 0 first (map logically empty)
            // 2. Mark slots empty (hash=0) before any potentially-throwing ops
            // 3. Best-effort release via assignment (may throw with SafePolicy)
            // This preserves BASIC exception guarantee: map is always in valid state.
            num_elements_ = 0;
            for (Entry& e : buckets_)
            {
                if (e.occupied())
                {
                    e.clear();  // Mark slot empty FIRST (hash=0)
                    e.key = Key{};    // Then reset (may throw)
                    e.value = Value{};
                }
            }
        }
    }

    Value& operator[](const Key& k)
    {
        assert_mutable();

        Value* val = find(k);
        if (val)
        {
            return *val;
        }
        
        // Key doesn't exist - insert with default value
        if (num_elements_ + 1 > static_cast<size_t>(bucket_count() * max_load_factor_))
        {
            rehash_to(bucket_count() * 2);
        }
        size_t h = hash_key(k);
        size_t slot = insert_internal_new(Key(k), Value{}, h);
        check_load_factor_diagnostics();
        return buckets_[slot].value;
    }

    void reserve(size_t count)
    {
        // P0 FIX: Prevent modification of frozen map
        assert_mutable();
        
        size_t needed = static_cast<size_t>(count / max_load_factor_) + 1;
        if (needed > bucket_count())
        {
            rehash_to(needed);
        }
    }

    void rehash(size_t count)
    {
        // P0 FIX: Prevent modification of frozen map
        assert_mutable();
        
        rehash_to(count);
    }

    // =========================================================================
    // Element Access: at()
    // =========================================================================

    /**
     * @brief Access element with bounds checking
     * @param k The key to find
     * @return Reference to the value
     * @throws std::out_of_range if key not found
     */
    Value& at(const Key& k)
    {
        Value* val = find(k);
        if (!val)
        {
            throw std::out_of_range("StableHashMap::at: key not found");
        }
        return *val;
    }

    const Value& at(const Key& k) const
    {
        const Value* val = find(k);
        if (!val)
        {
            throw std::out_of_range("StableHashMap::at: key not found");
        }
        return *val;
    }

    // =========================================================================
    // Lookup: count()
    // =========================================================================

    /**
     * @brief Count elements with specific key (always 0 or 1)
     * @param k The key to count
     * @return 1 if key exists, 0 otherwise
     */
    size_t count(const Key& k) const
    {
        return contains(k) ? 1 : 0;
    }

    /**
     * @brief Heterogeneous count (transparent lookup)
     */
    template <typename K,
              std::enable_if_t<detail::is_transparent_v<Hash, KeyEqual> &&
                               !std::is_same_v<std::decay_t<K>, Key>, int> = 0>
    size_t count(const K& k) const
    {
        return contains(k) ? 1 : 0;
    }

    // =========================================================================
    // Modifiers: swap()
    // =========================================================================

    /**
     * @brief Swap contents with another map
     * @param other Map to swap with
     * @note noexcept only if underlying types are nothrow swappable
     */
    void swap(StableHashMap& other) noexcept(
        std::is_nothrow_swappable_v<std::vector<Entry, entry_allocator_type>> &&
        std::is_nothrow_swappable_v<Hash> &&
        std::is_nothrow_swappable_v<KeyEqual>)
    {
        using std::swap;
        // P0 FIX: Allocator safety
        // std::vector::swap is UB if allocators are unequal and don't propagate on swap.
        // This matches libc++ debug mode behavior.
        assert(buckets_.get_allocator() == other.buckets_.get_allocator() &&
               "StableHashMap::swap requires equal allocators");
        swap(buckets_, other.buckets_);
        swap(num_elements_, other.num_elements_);
        swap(mask_, other.mask_);
        swap(max_load_factor_, other.max_load_factor_);
        swap(hasher_, other.hasher_);
        swap(key_equal_, other.key_equal_);
        swap(mutation_policy_, other.mutation_policy_);
#ifndef NDEBUG
        swap(warned_high_load_, other.warned_high_load_);
#endif
    }

    // =========================================================================
    // Iterators
    // =========================================================================

private:
    // Iterator implementation - skips empty slots
    // This is a PROXY ITERATOR: operator* returns a temporary pair of references,
    // not a reference to a stored pair. This is necessary because Entry stores
    // Key and Value separately, not as std::pair.
    //
    // Implications:
    // - `auto& ref = *it;` won't compile (can't bind lvalue ref to temporary)
    // - `auto ref = *it;` works (copies the reference pair)
    // - `const auto& ref = *it;` works (lifetime extension)
    // - `for (auto [k, v] : map)` works (structured bindings)
    template <bool IsConst>
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        
        // P1 FIX: value_type must be non-reference per standard iterator requirements
        // This allows generic code using Iterator::value_type to work correctly.
        using value_type = std::pair<const Key, Value>;
        
        // reference is the proxy type returned by operator*
        using reference = std::pair<const Key&, std::conditional_t<IsConst, const Value&, Value&>>;
        
        // Arrow proxy for operator-> (required for proxy iterators)
        // Note: The pointer returned by ArrowProxy::operator->() is only valid for
        // the duration of the expression (e.g., `it->second`). Do not store it.
        struct ArrowProxy
        {
            reference pair;
            const reference* operator->() const noexcept { return &pair; }
        };
        using pointer = ArrowProxy;

    private:
        using BucketPtr = std::conditional_t<IsConst, const Entry*, Entry*>;

        BucketPtr current_;
        BucketPtr end_;

        void advance_to_occupied()
        {
            while (current_ != end_ && !current_->occupied())
            {
                ++current_;
            }
        }

    public:
        Iterator() noexcept : current_(nullptr), end_(nullptr) {}

        Iterator(BucketPtr start, BucketPtr end) noexcept
            : current_(start), end_(end)
        {
            advance_to_occupied();
        }

        reference operator*() const
        {
            // P0 FIX: Debug assert to catch dereference of end iterator
            assert(current_ != end_ && "StableHashMap: cannot dereference end() iterator");
            return {current_->key, current_->value};
        }

        ArrowProxy operator->() const
        {
            // P0 FIX: Debug assert to catch dereference of end iterator
            assert(current_ != end_ && "StableHashMap: cannot dereference end() iterator");
            return ArrowProxy{{current_->key, current_->value}};
        }

        Iterator& operator++()
        {
            ++current_;
            advance_to_occupied();
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const noexcept
        {
            return current_ == other.current_;
        }

        bool operator!=(const Iterator& other) const noexcept
        {
            return current_ != other.current_;
        }

        // Allow conversion from non-const to const iterator
        template <bool OtherConst, std::enable_if_t<IsConst && !OtherConst, int> = 0>
        Iterator(const Iterator<OtherConst>& other) noexcept
            : current_(other.current_), end_(other.end_) {}

        // Give const iterator access to non-const iterator's private members
        friend class Iterator<!IsConst>;
        
        // Allow StableHashMap to access private members for erase(iterator)
        friend class StableHashMap;
    };

public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    /**
     * @brief Returns iterator to the beginning
     * @note Iterators are invalidated by ANY mutation
     */
    iterator begin() noexcept
    {
        if (buckets_.empty()) return end();
        return iterator(buckets_.data(), buckets_.data() + buckets_.size());
    }

    /**
     * @brief Returns iterator to the end
     */
    iterator end() noexcept
    {
        if (buckets_.empty()) return iterator(nullptr, nullptr);
        Entry* e = buckets_.data() + buckets_.size();
        return iterator(e, e);
    }

    const_iterator begin() const noexcept
    {
        if (buckets_.empty()) return end();
        return const_iterator(buckets_.data(), buckets_.data() + buckets_.size());
    }

    const_iterator end() const noexcept
    {
        if (buckets_.empty()) return const_iterator(nullptr, nullptr);
        const Entry* e = buckets_.data() + buckets_.size();
        return const_iterator(e, e);
    }

    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    /**
     * @brief Erase element at iterator position
     * @param pos Iterator to element to erase (must be valid, non-end iterator)
     * @return Iterator to element following the erased element
     * 
     * @warning All iterators are invalidated after this call due to backward-shift
     */
    iterator erase(iterator pos)
    {
        assert_mutable();
        
        if (pos == end() || buckets_.empty())
        {
            return end();
        }

        // Get slot index from iterator's internal pointer
        size_t slot = static_cast<size_t>(pos.current_ - buckets_.data());
        
        if (slot >= buckets_.size() || !buckets_[slot].occupied())
        {
            return end();  // Invalid iterator
        }

        erase_slot(slot);
        
        // Return iterator starting from same slot (which now contains shifted element or is empty)
        // Due to backward-shift, we re-scan from current position
        Entry* start = buckets_.data() + slot;
        Entry* end_ptr = buckets_.data() + buckets_.size();
        return iterator(start, end_ptr);
    }

    /**
     * @brief Erase element at const_iterator position
     * @param pos Const iterator to element to erase
     * @return Iterator to element following the erased element
     */
    iterator erase(const_iterator pos)
    {
        assert_mutable();
        
        if (pos == cend() || buckets_.empty())
        {
            return end();
        }

        // Get slot index from iterator's internal pointer
        size_t slot = static_cast<size_t>(pos.current_ - buckets_.data());
        
        if (slot >= buckets_.size() || !buckets_[slot].occupied())
        {
            return end();
        }

        erase_slot(slot);
        
        Entry* start = buckets_.data() + slot;
        Entry* end_ptr = buckets_.data() + buckets_.size();
        return iterator(start, end_ptr);
    }

    // =========================================================================
    // Comparison Operators
    // =========================================================================

    /**
     * @brief Equality comparison
     * @param other Map to compare against
     * @return true if both maps contain the same key-value pairs
     *
     * Two maps are equal if they have the same size and every key-value pair
     * in one map exists with the same value in the other. Order does not matter.
     *
     * Complexity: O(n) where n is the number of elements.
     * Note: This requires iterating and looking up each element, so it's not
     * a hot-path operation. Use sparingly on large maps.
     */
    friend bool operator==(const StableHashMap& lhs, const StableHashMap& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return false;
        }

        for (const auto& [key, value] : lhs)
        {
            const Value* rhs_value = rhs.find(key);
            if (!rhs_value || !(*rhs_value == value))
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Inequality comparison
     * @param other Map to compare against
     * @return true if maps differ in size or any key-value pair
     */
    friend bool operator!=(const StableHashMap& lhs, const StableHashMap& rhs)
    {
        return !(lhs == rhs);
    }
};

}  // namespace fat_p

#endif  // STABLE_HASH_MAP_H
