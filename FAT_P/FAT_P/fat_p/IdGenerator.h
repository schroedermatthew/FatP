/**
 * @file IdGenerator.h
 * @brief Policy-based unique identifier generator with type-safe IDs and recycling.
 *
 * @details Provides a flexible ID generation system with:
 *   - Sequential, bounded, or random allocation strategies
 *   - Configurable recycling policies (FIFO, Min-First, None)
 *   - Thread-safe and single-threaded variants
 *   - StrongId integration for type safety
 *   - Expected-based error handling
 *   - RAII IdGuard for automatic cleanup
 *   - O(1) active ID tracking via unordered_set with lazy max
 *   - Batch generation/release with single lock acquisition
 *   - Seeded random generation for reproducibility
 *
 * @version 1.4
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: IdGenerator
  file_role: public_header
  path: fat_p/IdGenerator.h
  namespace: fat_p
  summary: "Public header for IdGenerator."
  api_stability: in_work
  related:
    docs_search: "IdGenerator"
    tests:
      - tests/test_IdGenerator.cpp
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
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "ConcurrencyPolicies.h"
#include "Expected.h"
#include "StrongId.h"

namespace fat_p
{

// =============================================================================
// Error Types
// =============================================================================

enum class IdError
{
    Overflow,
    InvalidRelease,
    AlreadyInUse
};

// =============================================================================
// Helper to extract underlying integral type
// =============================================================================

namespace detail
{

// Helper to detect value_type alias
template <typename T, typename = void>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {};

// Helper to detect underlying_type alias (common alternative)
template <typename T, typename = void>
struct has_underlying_type : std::false_type {};

template <typename T>
struct has_underlying_type<T, std::void_t<typename T::underlying_type>> : std::true_type {};

// Primary template - for integral types
template <typename T, typename = void>
struct extract_value_type
{
    static_assert(std::is_integral_v<T> || has_value_type<T>::value || has_underlying_type<T>::value,
                  "IdType must be integral or expose value_type/underlying_type");
    using type = T;
};

// Specialization for integral types
template <typename T>
struct extract_value_type<T, std::enable_if_t<std::is_integral_v<T>>>
{
    using type = T;
};

// Specialization for types with value_type (like StrongId)
template <typename T>
struct extract_value_type<T, std::enable_if_t<!std::is_integral_v<T> && has_value_type<T>::value>>
{
    using type = typename T::value_type;
};

// Specialization for types with underlying_type but not value_type
template <typename T>
struct extract_value_type<T, std::enable_if_t<!std::is_integral_v<T> && 
                                               !has_value_type<T>::value && 
                                               has_underlying_type<T>::value>>
{
    using type = typename T::underlying_type;
};

// Distribution type selector for RandomAllocationPolicy
// std::uniform_int_distribution requires at least short (C++17 26.6.1.1)
// For uint8_t and int8_t, we use unsigned int and cast down
template <typename T>
struct distribution_type
{
    using type = std::conditional_t<(sizeof(T) < sizeof(unsigned short)), unsigned int, T>;
};

template <typename T>
using distribution_type_t = typename distribution_type<T>::type;

// Trait to detect if policy has revert() method
template <typename T, typename = void>
struct has_revert : std::false_type {};

template <typename T>
struct has_revert<T, std::void_t<decltype(std::declval<T>().revert(size_t{}))>> : std::true_type {};

template <typename T>
inline constexpr bool has_revert_v = has_revert<T>::value;

} // namespace detail

template <typename T>
using underlying_id_type_t = typename detail::extract_value_type<T>::type;

// =============================================================================
// =============================================================================
// Active ID Tracking
// =============================================================================

/**
 * @brief High-performance tracker for active IDs using std::unordered_set with lazy max.
 *
 * @details Provides O(1) average insert/erase/contains operations. The max element is
 * cached and only recomputed when the current max is erased. Benchmarks show ~3x faster
 * than std::set-based tracking.
 *
 * @tparam T The ID type (must be unsigned and hashable)
 */
template <typename T>
class ActiveIdTracker
{
    static_assert(std::is_unsigned_v<T>, "ID type should be unsigned for proper max tracking");

public:
    /**
     * @brief Insert an ID into the tracker.
     * @param id The ID to insert.
     * @return true if inserted, false if already present.
     */
    bool insert(T id)
    {
        auto [it, inserted] = container_.insert(id);
        if (inserted)
        {
            if (container_.size() == 1)
            {
                // First element is always max
                max_ = id;
                max_valid_ = true;
            }
            else if (max_valid_ && id > max_)
            {
                // Extend known max
                max_ = id;
            }
            // If !max_valid_ and size > 1, leave invalid for lazy recompute
        }
        return inserted;
    }

    size_t erase(T id)
    {
        size_t result = container_.erase(id);
        if (result > 0 && id == max_)
        {
            max_valid_ = false; // Invalidate, recompute lazily
        }
        return result;
    }

    size_t count(T id) const { return container_.count(id); }

    bool empty() const noexcept { return container_.empty(); }

    size_t size() const noexcept { return container_.size(); }

    /**
     * @brief Get the maximum element in the tracker.
     * @return The maximum element, or std::nullopt if empty.
     */
    std::optional<T> max_element()
    {
        if (container_.empty())
        {
            return std::nullopt;
        }
        if (!max_valid_)
        {
            max_ = *std::max_element(container_.begin(), container_.end());
            max_valid_ = true;
        }
        return max_;
    }

    void clear() noexcept
    {
        container_.clear();
        max_valid_ = false;
        max_ = T{};
    }

private:
    std::unordered_set<T> container_;
    T max_ = T{};
    bool max_valid_ = false;
};

// =============================================================================
// IdAllocationPolicy
// =============================================================================

template <typename IdType = uint64_t>
class SequentialAllocationPolicy
{
    static_assert(std::is_integral_v<IdType>, "IdType must be integral");
    static_assert(std::is_unsigned_v<IdType>, "IdType should be unsigned for ID generation");

public:
    explicit SequentialAllocationPolicy(IdType base_id = 0)
        : base_id_(base_id)
        , next_id_(base_id)
        , exhausted_(false)
    {
    }

    std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept
    {
        // If we've previously exhausted the ID space, don't generate more
        if (exhausted_)
        {
            return std::nullopt;
        }

        IdType candidate;

        if (first_call)
        {
            // First call: use our internal counter
            // If next_id_ is already at max AND we've previously generated max,
            // we're exhausted (defensive check for edge cases)
            candidate = next_id_;
        }
        else
        {
            // Check for overflow before computing max_id + 1
            if (max_id == std::numeric_limits<IdType>::max())
            {
                exhausted_ = true;
                return std::nullopt;
            }

            // Subsequent calls: use max of our counter and current maximum + 1
            // This ensures we never go backwards and respect both recycling gaps
            // and our internal sequence state
            IdType next_after_max = max_id + 1;
            candidate = (next_id_ > next_after_max) ? next_id_ : next_after_max;
        }

        // Update next_id_ for next call
        if (candidate == std::numeric_limits<IdType>::max())
        {
            next_id_ = candidate; // Stay at max
            // Mark as exhausted AFTER returning max (max is still a valid ID)
            // The NEXT call will fail
        }
        else
        {
            next_id_ = candidate + 1;
        }

        return candidate;
    }

    /// @brief Revert the internal counter by a specified count.
    /// @details Used by rollback_batch to prevent sequence gaps when batch
    /// generation fails partway through. Only reverts newly generated IDs,
    /// not recycled ones.
    /// @param count Number of IDs to revert
    void revert(size_t count) noexcept
    {
        if (count == 0) return;
        
        // Clear exhausted flag since we're reverting
        exhausted_ = false;
        
        // Protect against underflow
        if (count > static_cast<size_t>(next_id_ - base_id_))
        {
            next_id_ = base_id_;
            return;
        }
        next_id_ -= static_cast<IdType>(count);
        if (next_id_ < base_id_)
        {
            next_id_ = base_id_;
        }
    }

    void reset(IdType base_id = 0) noexcept
    {
        base_id_ = base_id;
        next_id_ = base_id;
        exhausted_ = false;
    }

private:
    IdType base_id_;
    IdType next_id_;
    bool exhausted_;  // Track if we've hit the limit
};

template <typename IdType = uint64_t>
class RandomAllocationPolicy
{
    static_assert(std::is_integral_v<IdType>, "IdType must be integral");
    static_assert(std::is_unsigned_v<IdType>, "IdType should be unsigned for ID generation");

    // Use a wider distribution type for small integers to avoid UB
    // std::uniform_int_distribution requires at least short
    using DistType = detail::distribution_type_t<IdType>;

public:
    /// @brief Construct with random seed from std::random_device
    explicit RandomAllocationPolicy(IdType = 0)
        : rng_(std::random_device{}())
        , dist_(static_cast<DistType>(0),
                static_cast<DistType>(std::numeric_limits<IdType>::max()))
    {
    }

    /// @brief Construct with explicit seed for reproducible randomness
    /// @param seed The seed value for the RNG
    /// @param ignored Disambiguator (use any value)
    RandomAllocationPolicy(uint64_t seed, int /*ignored*/)
        : rng_(seed)
        , dist_(static_cast<DistType>(0),
                static_cast<DistType>(std::numeric_limits<IdType>::max()))
    {
    }

    std::optional<IdType> next_id(IdType, bool = false) noexcept
    {
        // No try-catch needed: uniform_int_distribution doesn't throw
        return static_cast<IdType>(dist_(rng_));
    }

    void reset(IdType = 0) noexcept { rng_.seed(std::random_device{}()); }

    /// @brief Reset with explicit seed for reproducible randomness
    void reset_with_seed(uint64_t seed) noexcept { rng_.seed(seed); }

private:
    std::mt19937_64 rng_;
    std::uniform_int_distribution<DistType> dist_;
};

/**
 * @brief Sequential allocation with custom upper bound.
 * 
 * @details Generates IDs in monotonically increasing order within a specified range.
 * Useful for domain-specific constraints like array indexing or protocol compliance.
 * Returns nullopt (overflow) when max_bound is exceeded.
 * 
 * @tparam IdType The underlying ID type (must be unsigned integral)
 */
template <typename IdType = uint64_t>
class BoundedSequentialAllocationPolicy
{
    static_assert(std::is_integral_v<IdType>, "IdType must be integral");
    static_assert(std::is_unsigned_v<IdType>, "IdType should be unsigned for ID generation");

public:
    explicit BoundedSequentialAllocationPolicy(IdType base_id = 0,
                                                IdType max_bound = std::numeric_limits<IdType>::max())
        : base_id_(base_id)
        , next_id_(base_id)
        , max_bound_(max_bound)
    {
    }

    std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept
    {
        IdType candidate;

        if (first_call)
        {
            candidate = next_id_;
        }
        else
        {
            // Check for overflow before computing max_id + 1
            if (max_id >= max_bound_)
            {
                return std::nullopt;
            }

            IdType next_after_max = max_id + 1;
            candidate = (next_id_ > next_after_max) ? next_id_ : next_after_max;
        }

        // Check against custom bound
        if (candidate > max_bound_)
        {
            return std::nullopt;
        }

        // Update next_id_ for next call
        if (candidate == max_bound_)
        {
            next_id_ = candidate; // Stay at max
        }
        else
        {
            next_id_ = candidate + 1;
        }

        return candidate;
    }

    void reset(IdType base_id = 0) noexcept
    {
        base_id_ = base_id;
        next_id_ = base_id;
        // Note: max_bound_ is not reset - it's a construction-time constraint
    }

    /// @brief Revert the internal counter by a specified count.
    /// @param count Number of IDs to revert
    void revert(size_t count) noexcept
    {
        if (count == 0) return;
        
        // Protect against underflow
        if (count > static_cast<size_t>(next_id_ - base_id_))
        {
            next_id_ = base_id_;
            return;
        }
        next_id_ -= static_cast<IdType>(count);
        if (next_id_ < base_id_)
        {
            next_id_ = base_id_;
        }
    }

private:
    IdType base_id_;
    IdType next_id_;
    IdType max_bound_;
};

// =============================================================================
// Policy Traits
// =============================================================================

namespace detail
{

/// @brief Trait to detect if an allocation policy may produce collisions.
/// Sequential policies are deterministic and don't need retry loops.
template <typename T>
struct may_collide : std::true_type {};

template <typename IdType>
struct may_collide<SequentialAllocationPolicy<IdType>> : std::false_type {};

template <typename IdType>
struct may_collide<BoundedSequentialAllocationPolicy<IdType>> : std::false_type {};

template <typename T>
inline constexpr bool may_collide_v = may_collide<T>::value;

/// @brief Trait to detect if allocation policy is RandomAllocationPolicy
template <typename T>
struct is_random_policy : std::false_type {};

template <typename IdType>
struct is_random_policy<RandomAllocationPolicy<IdType>> : std::true_type {};

template <typename T>
inline constexpr bool is_random_policy_v = is_random_policy<T>::value;

} // namespace detail

// =============================================================================
// RecyclingPolicy
// =============================================================================

/**
 * @brief FIFO recycling using std::deque.
 * 
 * @details Fast O(1) operations for both add and get. IDs are recycled in the
 * order they were released. This is the default policy, suitable for most use cases.
 */
template <typename IdType = uint64_t>
class ImmediateRecyclingPolicy
{
public:
    std::optional<IdType> get_recycled() noexcept
    {
        if (recycled_.empty())
        {
            return std::nullopt;
        }
        IdType id = recycled_.front();
        recycled_.pop_front();
        return id;
    }

    void add_recycled(IdType id) noexcept { recycled_.push_back(id); }

    size_t recycled_count() const noexcept { return recycled_.size(); }

    void clear() noexcept { recycled_.clear(); }

private:
    std::deque<IdType> recycled_;
};

/**
 * @brief Min-first recycling using std::set.
 * 
 * @details Always recycles the smallest available ID first, promoting dense ID ranges.
 * Uses O(log n) operations. Recommended for HPC applications where memory locality
 * benefits from contiguous ID ranges in ID-indexed data structures.
 */
template <typename IdType = uint64_t>
class MinRecyclingPolicy
{
public:
    std::optional<IdType> get_recycled() noexcept
    {
        if (recycled_.empty())
        {
            return std::nullopt;
        }
        auto it = recycled_.begin();
        IdType id = *it;
        recycled_.erase(it);
        return id;
    }

    void add_recycled(IdType id) noexcept { recycled_.insert(id); }

    size_t recycled_count() const noexcept { return recycled_.size(); }

    void clear() noexcept { recycled_.clear(); }

private:
    std::set<IdType> recycled_;
};

template <typename IdType = uint64_t>
class NoRecyclingPolicy
{
public:
    std::optional<IdType> get_recycled() noexcept { return std::nullopt; }
    void add_recycled(IdType) noexcept {}
    size_t recycled_count() const noexcept { return 0; }
    void clear() noexcept {}
};

// =============================================================================
// ErrorPolicy
// =============================================================================

namespace id_generator
{

template <typename IdType, typename ErrorType = IdError>
class ExpectedErrorPolicy
{
public:
    using result_type = Expected<IdType, ErrorType>;
    using void_result_type = Expected<void, ErrorType>;

    static result_type report_success(IdType id) noexcept { return id; }

    static result_type report_error(ErrorType error) noexcept
    {
        return make_unexpected(error);
    }
};

} // namespace id_generator

// Convenience alias for verbose module namespace.
namespace idg = id_generator;

// =============================================================================
// Seed Tag for Seeded Construction

// =============================================================================

/// @brief Tag type for seeded construction of RandomIdGenerator
struct seed_tag_t
{
    explicit seed_tag_t() = default;
};

/// @brief Constant for seeded construction: `IdGenerator(seed_tag, 42)`
inline constexpr seed_tag_t seed_tag{};

// =============================================================================
// IdGenerator
// =============================================================================

/**
 * @brief Policy-based unique identifier generator.
 *
 * @tparam IdType_ The ID type (integral or StrongId)
 * @tparam AllocationPolicy Strategy for generating new IDs
 * @tparam RecyclingPolicy Strategy for reusing released IDs
 * @tparam ErrorPolicy Strategy for error reporting
 * @tparam ConcurrencyPolicy Thread safety strategy
 *
 * @note The generator's lifetime must exceed that of any IdGuard instances
 *       created from it. Moving or destroying the generator while guards
 *       exist results in undefined behavior.
 */
template <typename IdType_,
          typename AllocationPolicy =
              SequentialAllocationPolicy<underlying_id_type_t<IdType_>>,
          typename RecyclingPolicy =
              ImmediateRecyclingPolicy<underlying_id_type_t<IdType_>>,
          typename ErrorPolicy = id_generator::ExpectedErrorPolicy<IdType_, IdError>,
          typename ConcurrencyPolicy = SingleThreadedPolicy>
class IdGenerator : private AllocationPolicy
                  , private RecyclingPolicy
                  , private ConcurrencyPolicy
{
public:
    using id_type = IdType_;
    using result_type = typename ErrorPolicy::result_type;
    using underlying_type = underlying_id_type_t<IdType_>;

    // =========================================================================
    // Construction
    // =========================================================================

    explicit IdGenerator(underlying_type base_id = 0)
        : AllocationPolicy(base_id)
        , RecyclingPolicy()
        , ConcurrencyPolicy()
        , base_id_(base_id)
        , ids_in_use_()
    {
    }

    /**
     * @brief Construct with explicit seed for reproducible random generation.
     * 
     * @details Only available when AllocationPolicy is RandomAllocationPolicy.
     * Enables deterministic ID sequences for testing and simulations.
     * Use: `RandomIdGenerator<uint64_t> gen(seed_tag, 42);`
     * 
     * @param tag Disambiguation tag (use `seed_tag`)
     * @param seed The seed value for the random number generator
     */
    template <typename AP = AllocationPolicy,
              typename = std::enable_if_t<detail::is_random_policy_v<AP>>>
    IdGenerator(seed_tag_t /*tag*/, uint64_t seed)
        : AllocationPolicy(seed, 0)
        , RecyclingPolicy()
        , ConcurrencyPolicy()
        , base_id_(0)
        , ids_in_use_()
    {
    }

    /**
     * @brief Destructor that explicitly clears internal state.
     * 
     * @details Clears ids_in_use_ and recycle pools to help AddressSanitizer
     * detect use-after-free errors more reliably. If a dangling IdGuard tries
     * to release after destruction, this increases the chance of catching the
     * error rather than silently corrupting memory.
     * 
     * @warning IdGuard instances must not outlive the generator. Moving or
     * destroying the generator while guards exist results in undefined behavior.
     */
    ~IdGenerator()
    {
        ids_in_use_.clear();
        RecyclingPolicy::clear();
    }

    IdGenerator(const IdGenerator&) = delete;
    IdGenerator& operator=(const IdGenerator&) = delete;
    IdGenerator(IdGenerator&&) noexcept = default;
    IdGenerator& operator=(IdGenerator&&) noexcept = default;

    // =========================================================================
    // ID Generation and Release
    // =========================================================================

    result_type generate()
    {
        auto lock = ConcurrencyPolicy::lock();

        // Try recycled IDs first (guaranteed unique, no retry needed)
        if (auto recycled = RecyclingPolicy::get_recycled())
        {
            underlying_type raw_id = *recycled;

#ifndef NDEBUG
            // Debug assertion: recycled ID should not be in active set
            assert(ids_in_use_.count(raw_id) == 0 && "Recycled ID already in use");
#endif

            (void)ids_in_use_.insert(raw_id);

            if constexpr (std::is_same_v<IdType_, underlying_type>)
            {
                return ErrorPolicy::report_success(raw_id);
            }
            else
            {
                return ErrorPolicy::report_success(IdType_(raw_id));
            }
        }

        // Generate new ID
        // Compile-time optimization: only use retry loop for policies that may collide
        // (e.g., RandomAllocationPolicy). Sequential policies never collide.
        // Higher retry count (100) handles small ID types like uint8_t where collisions
        // are more probable even when space isn't exhausted.
        constexpr bool needs_retry = detail::may_collide_v<AllocationPolicy>;
        constexpr int kMaxRetries = needs_retry ? 100 : 1;

        for (int attempt = 0; attempt < kMaxRetries; ++attempt)
        {
            bool is_first = ids_in_use_.empty();
            auto max_opt = ids_in_use_.max_element();
            underlying_type max_id = is_first ? base_id_ : *max_opt;
            auto new_id_opt = AllocationPolicy::next_id(max_id, is_first);

            if (!new_id_opt)
            {
                return ErrorPolicy::report_error(IdError::Overflow);
            }

            underlying_type raw_id = *new_id_opt;

            // Single-lookup: insert returns false if already present (collision)
            if (ids_in_use_.insert(raw_id))
            {
                if constexpr (std::is_same_v<IdType_, underlying_type>)
                {
                    return ErrorPolicy::report_success(raw_id);
                }
                else
                {
                    return ErrorPolicy::report_success(IdType_(raw_id));
                }
            }
            // Collision - retry with next ID from policy (only for random policies)
        }

        return ErrorPolicy::report_error(IdError::AlreadyInUse);
    }

    Expected<void, IdError> release(IdType_ id) noexcept
    {
        auto lock = ConcurrencyPolicy::lock();

        underlying_type raw_id;
        if constexpr (std::is_same_v<IdType_, underlying_type>)
        {
            raw_id = id;
        }
        else
        {
            raw_id = id.get();
        }

        if (ids_in_use_.erase(raw_id) == 0)
        {
            return make_unexpected(IdError::InvalidRelease);
        }

        RecyclingPolicy::add_recycled(raw_id);
        return {};
    }

    /**
     * @brief Generate multiple IDs in a single operation.
     * 
     * @details Acquires lock once and generates all requested IDs, reducing
     * synchronization overhead for thread-safe variants. If generation fails
     * partway through, all successfully generated IDs are released (rollback).
     * 
     * Rollback behavior: IDs that were recycled are returned to the recycle pool.
     * Newly generated IDs (beyond previous max) are discarded to preserve ID density
     * for MinRecyclingPolicy and similar policies.
     * 
     * @param count Number of IDs to generate
     * @return Expected containing vector of IDs, or error if generation failed
     */
    Expected<std::vector<id_type>, IdError> generate_batch(size_t count)
    {
        if (count == 0)
        {
            return std::vector<id_type>{};
        }

        auto lock = ConcurrencyPolicy::lock();

        // Track the max before batch for smart rollback
        auto pre_batch_max = ids_in_use_.max_element();

        std::vector<id_type> result;
        result.reserve(count);

        // Compile-time optimization: only use retry loop for policies that may collide
        // Higher retry count (100) handles small ID types where collisions are frequent
        constexpr bool needs_retry = detail::may_collide_v<AllocationPolicy>;
        constexpr int kMaxRetries = needs_retry ? 100 : 1;

        for (size_t i = 0; i < count; ++i)
        {
            // Try recycled IDs first
            if (auto recycled = RecyclingPolicy::get_recycled())
            {
                underlying_type raw_id = *recycled;
                (void)ids_in_use_.insert(raw_id);

                if constexpr (std::is_same_v<IdType_, underlying_type>)
                {
                    result.push_back(raw_id);
                }
                else
                {
                    result.push_back(IdType_(raw_id));
                }
                continue;
            }

            // Generate new ID with retry loop (optimized at compile-time)
            bool generated = false;

            for (int attempt = 0; attempt < kMaxRetries; ++attempt)
            {
                bool is_first = ids_in_use_.empty();
                auto max_opt = ids_in_use_.max_element();
                underlying_type max_id = is_first ? base_id_ : *max_opt;
                auto new_id_opt = AllocationPolicy::next_id(max_id, is_first);

                if (!new_id_opt)
                {
                    // Overflow - rollback all generated IDs
                    rollback_batch(result, pre_batch_max);
                    return make_unexpected(IdError::Overflow);
                }

                underlying_type raw_id = *new_id_opt;

                if (ids_in_use_.insert(raw_id))
                {
                    if constexpr (std::is_same_v<IdType_, underlying_type>)
                    {
                        result.push_back(raw_id);
                    }
                    else
                    {
                        result.push_back(IdType_(raw_id));
                    }
                    generated = true;
                    break;
                }
            }

            if (!generated)
            {
                // Collision exhaustion - rollback
                rollback_batch(result, pre_batch_max);
                return make_unexpected(IdError::AlreadyInUse);
            }
        }

        return result;
    }

    /**
     * @brief Release multiple IDs in a single operation.
     * 
     * @details Acquires lock once and releases all specified IDs, reducing
     * synchronization overhead for thread-safe variants. Stops on first error.
     * 
     * @param ids Vector of IDs to release
     * @return Expected<void, IdError> - success or first error encountered
     */
    Expected<void, IdError> release_batch(const std::vector<id_type>& ids) noexcept
    {
        if (ids.empty())
        {
            return {};
        }

        auto lock = ConcurrencyPolicy::lock();

        for (const auto& id : ids)
        {
            underlying_type raw_id;
            if constexpr (std::is_same_v<IdType_, underlying_type>)
            {
                raw_id = id;
            }
            else
            {
                raw_id = id.get();
            }

            if (ids_in_use_.erase(raw_id) == 0)
            {
                return make_unexpected(IdError::InvalidRelease);
            }

            RecyclingPolicy::add_recycled(raw_id);
        }

        return {};
    }

private:
    /**
     * @brief Rollback batch generation, preserving ID density.
     * 
     * @details IDs that were recycled (below or equal to pre_batch_max) are
     * returned to the recycle pool. Newly generated IDs (above pre_batch_max)
     * are discarded and the allocation policy counter is reverted to prevent
     * permanent sequence gaps.
     */
    void rollback_batch(const std::vector<id_type>& result,
                        std::optional<underlying_type> pre_batch_max)
    {
        size_t newly_generated_count = 0;

        for (const auto& id : result)
        {
            underlying_type raw;
            if constexpr (std::is_same_v<IdType_, underlying_type>)
            {
                raw = id;
            }
            else
            {
                raw = id.get();
            }
            ids_in_use_.erase(raw);

            // Only recycle IDs that were originally recycled (not newly generated)
            // This preserves density for MinRecyclingPolicy
            if (pre_batch_max.has_value() && raw <= *pre_batch_max)
            {
                RecyclingPolicy::add_recycled(raw);
            }
            else
            {
                // This was a newly generated ID
                ++newly_generated_count;
            }
        }

        // Revert the allocation policy counter to prevent sequence gaps
        // Only policies that support revert() will have this called
        if constexpr (detail::has_revert_v<AllocationPolicy>)
        {
            AllocationPolicy::revert(newly_generated_count);
        }
    }

public:

    // =========================================================================
    // Query Operations
    // =========================================================================

    bool is_active(IdType_ id) const noexcept
    {
        auto lock = ConcurrencyPolicy::lock_shared();
        underlying_type raw_id;
        if constexpr (std::is_same_v<IdType_, underlying_type>)
        {
            raw_id = id;
        }
        else
        {
            raw_id = id.get();
        }
        return ids_in_use_.count(raw_id) > 0;
    }

    size_t active_count() const noexcept
    {
        auto lock = ConcurrencyPolicy::lock_shared();
        return ids_in_use_.size();
    }

    size_t recycled_count() const noexcept
    {
        auto lock = ConcurrencyPolicy::lock_shared();
        return RecyclingPolicy::recycled_count();
    }

    void reset() noexcept
    {
        auto lock = ConcurrencyPolicy::lock();
        ids_in_use_.clear();
        RecyclingPolicy::clear();
        AllocationPolicy::reset(base_id_);
    }

    // =========================================================================
    // RAII Helper
    // =========================================================================

    /**
     * @brief RAII guard that automatically releases an ID when destroyed.
     *
     * @warning The IdGenerator must outlive all IdGuard instances created from it.
     *          Destroying or moving the generator while guards exist is undefined behavior.
     */
    class IdGuard
    {
    public:
        IdGuard() noexcept : generator_(nullptr), id_{}, valid_(false) {}

        IdGuard(IdGenerator& gen, IdType_ id) noexcept
            : generator_(&gen)
            , id_(id)
            , valid_(true)
        {
        }

        ~IdGuard()
        {
            if (valid_ && generator_)
            {
                auto result = generator_->release(id_);
#ifndef NDEBUG
                // In debug builds, assert that release succeeded
                // Failure indicates a bug: double-release or invalid ID
                assert(result.has_value() && "IdGuard: release failed in destructor");
#else
                (void)result;
#endif
            }
        }

        // Move-only
        IdGuard(const IdGuard&) = delete;
        IdGuard& operator=(const IdGuard&) = delete;

        IdGuard(IdGuard&& other) noexcept
            : generator_(other.generator_)
            , id_(other.id_)
            , valid_(other.valid_)
        {
            other.valid_ = false;
        }

        IdGuard& operator=(IdGuard&& other) noexcept
        {
            if (this != &other)
            {
                if (valid_ && generator_)
                {
                    auto result = generator_->release(id_);
#ifndef NDEBUG
                    assert(result.has_value() && "IdGuard: release failed in move assignment");
#else
                    (void)result;
#endif
                }
                generator_ = other.generator_;
                id_ = other.id_;
                valid_ = other.valid_;
                other.valid_ = false;
            }
            return *this;
        }

        IdType_ get() const noexcept { return id_; }
        IdType_ operator*() const noexcept { return id_; }
        explicit operator bool() const noexcept { return valid_; }

        void release_ownership() noexcept { valid_ = false; }

    private:
        IdGenerator* generator_;
        IdType_ id_;
        bool valid_;
    };

    [[nodiscard]] Expected<IdGuard, IdError> scoped_id()
    {
        auto result = generate();
        if (!result.has_value())
        {
            return make_unexpected(result.error());
        }
        return IdGuard(*this, *result);
    }

private:
    underlying_type base_id_;
    ActiveIdTracker<underlying_type> ids_in_use_;
};

// =============================================================================
// Convenience Aliases
// =============================================================================

/// @brief Simple single-threaded generator with FIFO recycling
template <typename IdType = uint64_t>
using SimpleIdGenerator =
    IdGenerator<IdType,
                SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
                id_generator::ExpectedErrorPolicy<IdType, IdError>,
                SingleThreadedPolicy>;

/// @brief Thread-safe generator with mutex synchronization
template <typename IdType = uint64_t>
using ThreadSafeIdGenerator =
    IdGenerator<IdType,
                SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
                id_generator::ExpectedErrorPolicy<IdType, IdError>,
                MutexSynchronizationPolicy>;

/// @brief Generator prioritizing ID density (Min-First recycling)
/// @details Recommended for HPC applications where contiguous ID ranges
/// improve cache performance in ID-indexed data structures.
template <typename IdType = uint64_t>
using DenseIdGenerator =
    IdGenerator<IdType,
                SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                MinRecyclingPolicy<underlying_id_type_t<IdType>>,
                id_generator::ExpectedErrorPolicy<IdType, IdError>,
                SingleThreadedPolicy>;

/// @brief Random ID generator (no recycling, with retry for collisions)
template <typename IdType = uint64_t>
using RandomIdGenerator =
    IdGenerator<IdType,
                RandomAllocationPolicy<underlying_id_type_t<IdType>>,
                NoRecyclingPolicy<underlying_id_type_t<IdType>>,
                id_generator::ExpectedErrorPolicy<IdType, IdError>,
                SingleThreadedPolicy>;

} // namespace fat_p
