#pragma once

/*
FATP_META:
  meta_version: 1
  component: IdGenerator
  file_role: public_header
  path: include/fat_p/IdGenerator.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for IdGenerator."
  api_stability: in_work
  related:
    docs_search: "IdGenerator"
    tests:
      - components/IdGenerator/tests/test_IdGenerator.cpp
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

/**
 * @file IdGenerator.h
 * @brief Policy-based unique identifier generator with type-safe IDs and recycling.
 *
 *
 *
 * @details Provides a flexible ID generation system with:
 *   - Sequential or random allocation strategies (a bounded sequential policy also
 *     ships, but its upper bound can only be set by constructing the policy
 *     directly: the generator's constructor forwards nothing but base_id)
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
struct has_value_type : std::false_type
{
};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type
{
};

// Helper to detect underlying_type alias (common alternative)
template <typename T, typename = void>
struct has_underlying_type : std::false_type
{
};

template <typename T>
struct has_underlying_type<T, std::void_t<typename T::underlying_type>> : std::true_type
{
};

// Primary template - for integral types
template <typename T>
struct extract_value_type
{
    static_assert(std::is_integral_v<T> || has_value_type<T>::value || has_underlying_type<T>::value,
                  "IdType must be integral or expose value_type/underlying_type");
    using type = T;
};

// Specialization for integral types
template <typename T>
    requires std::is_integral_v<T>
struct extract_value_type<T>
{
    using type = T;
};

// Specialization for types with value_type (like StrongId)
template <typename T>
    requires (!std::is_integral_v<T> && has_value_type<T>::value)
struct extract_value_type<T>
{
    using type = typename T::value_type;
};

// Specialization for types with underlying_type but not value_type
template <typename T>
    requires (!std::is_integral_v<T> && !has_value_type<T>::value && has_underlying_type<T>::value)
struct extract_value_type<T>
{
    using type = typename T::underlying_type;
};

// Distribution type selector for RandomAllocationPolicy
// std::uniform_int_distribution requires at least short (standard §26.6.1.1)
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
struct has_revert : std::false_type
{
};

template <typename T>
struct has_revert<T, std::void_t<decltype(std::declval<T>().revert(size_t{}))>> : std::true_type
{
};

template <typename T>
inline constexpr bool has_revert_v = has_revert<T>::value;

// Trait to detect an ID type that reserves a sentinel meaning "no ID"
// (StrongId::invalid(), whose underlying value is numeric_limits<T>::max()).
// A generator must never ISSUE that value: a caller comparing against
// invalid(), or calling isValid(), would read a live ID as absent.
template <typename T, typename = void>
struct has_invalid_sentinel : std::false_type
{
};

template <typename T>
struct has_invalid_sentinel<T, std::void_t<decltype(T::invalid())>> : std::true_type
{
};

template <typename T>
inline constexpr bool has_invalid_sentinel_v = has_invalid_sentinel<T>::value;

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
 * cached and only recomputed when the current max is erased.
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
        auto [it, inserted] = mContainer.insert(id);
        if (inserted)
        {
            if (mContainer.size() == 1)
            {
                // First element is always max
                mMax = id;
                mMaxValid = true;
            }
            else if (mMaxValid && id > mMax)
            {
                // Extend known max
                mMax = id;
            }
            // If !mMaxValid and size > 1, leave invalid for lazy recompute
        }
        return inserted;
    }

    size_t erase(T id)
    {
        size_t result = mContainer.erase(id);
        if (result > 0 && id == mMax)
        {
            mMaxValid = false; // Invalidate, recompute lazily
        }
        return result;
    }

    size_t count(T id) const
    {
        return mContainer.count(id);
    }

    bool empty() const noexcept
    {
        return mContainer.empty();
    }

    size_t size() const noexcept
    {
        return mContainer.size();
    }

    /**
     * @brief Get the maximum element in the tracker.
     * @return The maximum element, or std::nullopt if empty.
     */
    std::optional<T> max_element()
    {
        if (mContainer.empty())
        {
            return std::nullopt;
        }
        if (!mMaxValid)
        {
            mMax = *std::max_element(mContainer.begin(), mContainer.end());
            mMaxValid = true;
        }
        return mMax;
    }

    void clear() noexcept
    {
        mContainer.clear();
        mMaxValid = false;
        mMax = T{};
    }

private:
    std::unordered_set<T> mContainer;
    T mMax = T{};
    bool mMaxValid = false;
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
        : mBaseId(base_id)
        , mNextId(base_id)
        , mExhausted(false)
    {
    }

    std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept
    {
        // Exhausted: the domain is spent and we cannot generate any more.
        if (mExhausted)
        {
            return std::nullopt;
        }

        IdType candidate;

        if (first_call)
        {
            // First call: use our internal counter
            // If mNextId is already at max AND we've previously generated max,
            // we're exhausted (defensive check for edge cases)
            candidate = mNextId;
        }
        else
        {
            // Check for overflow before computing max_id + 1
            if (max_id == std::numeric_limits<IdType>::max())
            {
                mExhausted = true;
                return std::nullopt;
            }

            // Subsequent calls: use max of our counter and current maximum + 1
            // This ensures we never go backwards and respect both recycling gaps
            // and our internal sequence state
            IdType next_after_max = max_id + 1;
            candidate = (mNextId > next_after_max) ? mNextId : next_after_max;
        }

        // Update mNextId for next call
        if (candidate == std::numeric_limits<IdType>::max())
        {
            mNextId = candidate; // Stay at max
            // Mark as exhausted AFTER returning max (max is still a valid ID)
            // The NEXT call will fail.
            //
            // The counter could not advance past max, so this issue advanced it
            // by ZERO rather than one. revert() must know, or it rewinds one too
            // far and the next generate() re-issues an ID this policy already
            // handed out -- which under NoRecyclingPolicy is a silent
            // never-reuse violation.
            mSaturated = true;
        }
        else
        {
            mNextId = candidate + 1;
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
        if (count == 0)
        {
            return;
        }

        // Clear exhausted flag since we're reverting
        mExhausted = false;

        // If the last issue saturated at max, the counter advanced by one less
        // than the number of IDs issued, so rewind by one less too.
        if (mSaturated)
        {
            mSaturated = false;
            --count;
            if (count == 0)
            {
                return;
            }
        }

        // Protect against underflow
        if (count > static_cast<size_t>(mNextId - mBaseId))
        {
            mNextId = mBaseId;
            return;
        }
        mNextId -= static_cast<IdType>(count);
        if (mNextId < mBaseId)
        {
            mNextId = mBaseId;
        }
    }

    void reset(IdType base_id = 0) noexcept
    {
        mBaseId = base_id;
        mNextId = base_id;
        mExhausted = false;
        mSaturated = false;
    }

private:
    IdType mBaseId;
    IdType mNextId;
    bool mExhausted; // Track if we've hit the limit
    bool mSaturated = false; // Last issue parked the counter at max (see revert)
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
        : mRng(std::random_device{}())
        , mDist(static_cast<DistType>(0), static_cast<DistType>(std::numeric_limits<IdType>::max()))
    {
    }

    /// @brief Construct with explicit seed for reproducible randomness
    /// @param seed The seed value for the RNG
    /// @param ignored Disambiguator (use any value)
    RandomAllocationPolicy(uint64_t seed, int /*ignored*/)
        : mRng(seed)
        , mDist(static_cast<DistType>(0), static_cast<DistType>(std::numeric_limits<IdType>::max()))
    {
    }

    std::optional<IdType> next_id(IdType, bool = false) noexcept
    {
        // No try-catch needed: uniform_int_distribution doesn't throw
        return static_cast<IdType>(mDist(mRng));
    }

    void reset(IdType = 0) noexcept
    {
        mRng.seed(std::random_device{}());
    }

    /// @brief Reset with explicit seed for reproducible randomness
    void reset_with_seed(uint64_t seed) noexcept
    {
        mRng.seed(seed);
    }

private:
    std::mt19937_64 mRng;
    std::uniform_int_distribution<DistType> mDist;
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
        : mBaseId(base_id)
        , mNextId(base_id)
        , mMaxBound(max_bound)
    {
    }

    std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept
    {
        IdType candidate;

        if (first_call)
        {
            candidate = mNextId;
        }
        else
        {
            // Check for overflow before computing max_id + 1
            if (max_id >= mMaxBound)
            {
                return std::nullopt;
            }

            IdType next_after_max = max_id + 1;
            candidate = (mNextId > next_after_max) ? mNextId : next_after_max;
        }

        // Check against custom bound
        if (candidate > mMaxBound)
        {
            return std::nullopt;
        }

        // Update mNextId for next call
        if (candidate == mMaxBound)
        {
            mNextId = candidate; // Stay at max
            // Parking means this issue advanced the counter by ZERO. revert()
            // must know, or it rewinds one too far and re-issues an ID this
            // policy already handed out -- the same defect the unbounded
            // sequential policy carried at its own saturation point.
            mSaturated = true;
        }
        else
        {
            mNextId = candidate + 1;
        }

        return candidate;
    }

    void reset(IdType base_id = 0) noexcept
    {
        mBaseId = base_id;
        mNextId = base_id;
        mSaturated = false;
        // Note: mMaxBound is not reset - it's a construction-time constraint
    }

    /// @brief Revert the internal counter by a specified count.
    /// @param count Number of IDs to revert
    void revert(size_t count) noexcept
    {
        if (count == 0)
        {
            return;
        }

        // The last issue parked at the bound and advanced nothing; rewind by
        // one less to match.
        if (mSaturated)
        {
            mSaturated = false;
            --count;
            if (count == 0)
            {
                return;
            }
        }

        // Protect against underflow
        if (count > static_cast<size_t>(mNextId - mBaseId))
        {
            mNextId = mBaseId;
            return;
        }
        mNextId -= static_cast<IdType>(count);
        if (mNextId < mBaseId)
        {
            mNextId = mBaseId;
        }
    }

private:
    IdType mBaseId;
    IdType mNextId;
    IdType mMaxBound;
    bool mSaturated = false; // Last issue parked at mMaxBound (see revert)
};

// =============================================================================
// Policy Traits
// =============================================================================

namespace detail
{

/// @brief Trait to detect if an allocation policy may produce collisions.
/// Sequential policies are deterministic and don't need retry loops.
template <typename T>
struct may_collide : std::true_type
{
};

template <typename IdType>
struct may_collide<SequentialAllocationPolicy<IdType>> : std::false_type
{
};

template <typename IdType>
struct may_collide<BoundedSequentialAllocationPolicy<IdType>> : std::false_type
{
};

template <typename T>
inline constexpr bool may_collide_v = may_collide<T>::value;

/// @brief Trait to detect if allocation policy is RandomAllocationPolicy
template <typename T>
struct is_random_policy : std::false_type
{
};

template <typename IdType>
struct is_random_policy<RandomAllocationPolicy<IdType>> : std::true_type
{
};

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
        if (mRecycled.empty())
        {
            return std::nullopt;
        }
        IdType id = mRecycled.front();
        mRecycled.pop_front();
        return id;
    }

    void add_recycled(IdType id) noexcept
    {
        mRecycled.push_back(id);
    }

    size_t recycled_count() const noexcept
    {
        return mRecycled.size();
    }

    void clear() noexcept
    {
        mRecycled.clear();
    }

private:
    std::deque<IdType> mRecycled;
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
        if (mRecycled.empty())
        {
            return std::nullopt;
        }
        auto it = mRecycled.begin();
        IdType id = *it;
        mRecycled.erase(it);
        return id;
    }

    void add_recycled(IdType id) noexcept
    {
        mRecycled.insert(id);
    }

    size_t recycled_count() const noexcept
    {
        return mRecycled.size();
    }

    void clear() noexcept
    {
        mRecycled.clear();
    }

private:
    std::set<IdType> mRecycled;
};

template <typename IdType = uint64_t>
class NoRecyclingPolicy
{
public:
    std::optional<IdType> get_recycled() noexcept
    {
        return std::nullopt;
    }
    void add_recycled(IdType) noexcept
    {
    }
    size_t recycled_count() const noexcept
    {
        return 0;
    }
    void clear() noexcept
    {
    }
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

    static result_type report_success(IdType id) noexcept
    {
        return id;
    }

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
 *       created from it. Destroying the generator while guards exist is
 *       undefined behavior. Movability is conditional on ConcurrencyPolicy,
 *       and none of the four aliases below is movable: SingleThreadedPolicy
 *       deletes its copy operations, which suppresses its moves, and
 *       MutexSynchronizationPolicy deletes its moves outright, so the
 *       defaulted moves here resolve to deleted and a move is a compile
 *       error. Under a policy that keeps its move (UniqueRWLockPolicy,
 *       MovableSingleThreadedPolicy) the generator is movable, and moving it
 *       with live guards is undefined behavior.
 */
template <typename IdType_,
          typename AllocationPolicy = SequentialAllocationPolicy<underlying_id_type_t<IdType_>>,
          typename RecyclingPolicy = ImmediateRecyclingPolicy<underlying_id_type_t<IdType_>>,
          typename ErrorPolicy = id_generator::ExpectedErrorPolicy<IdType_, IdError>,
          typename ConcurrencyPolicy = SingleThreadedPolicy>
class IdGenerator : private AllocationPolicy, private RecyclingPolicy, private ConcurrencyPolicy
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
        , mBaseId(base_id)
        , mIdsInUse()
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
    template <typename AP = AllocationPolicy>
        requires detail::is_random_policy_v<AP>
    IdGenerator(seed_tag_t /*tag*/, uint64_t seed)
        : AllocationPolicy(seed, 0)
        , RecyclingPolicy()
        , ConcurrencyPolicy()
        , mBaseId(0)
        , mIdsInUse()
    {
    }

    /**
     * @brief Destructor that explicitly clears internal state.
     *
     * @details Clears mIdsInUse and recycle pools to help AddressSanitizer
     * detect use-after-free errors more reliably. If a dangling IdGuard tries
     * to release after destruction, this increases the chance of catching the
     * error rather than silently corrupting memory.
     *
     * @warning IdGuard instances must not outlive the generator. Destroying the
     * generator while guards exist results in undefined behavior; see the class
     * note for when a move is possible at all.
     */
    ~IdGenerator()
    {
        mIdsInUse.clear();
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
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();

        // Try recycled IDs first (guaranteed unique, no retry needed)
        if (auto recycled = RecyclingPolicy::get_recycled())
        {
            underlying_type raw_id = *recycled;

#ifndef NDEBUG
            // Debug assertion: recycled ID should not be in active set
            assert(mIdsInUse.count(raw_id) == 0 && "Recycled ID already in use");
#endif

            (void)mIdsInUse.insert(raw_id);

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
            bool is_first = mIdsInUse.empty();
            auto max_opt = mIdsInUse.max_element();
            underlying_type max_id = is_first ? mBaseId : *max_opt;
            auto new_id_opt = AllocationPolicy::next_id(max_id, is_first);

            if (!new_id_opt)
            {
                return ErrorPolicy::report_error(IdError::Overflow);
            }

            underlying_type raw_id = *new_id_opt;

            // Never issue the ID type's reserved "no ID" sentinel. For StrongId
            // that is numeric_limits<underlying>::max(), so a generator whose
            // domain reaches the top would otherwise hand back a value that
            // isValid() reports as invalid.
            //
            // What the refusal MEANS depends on the allocation policy. A
            // sequential policy reaches the sentinel only at the end of its
            // domain, so that is genuine exhaustion. A random policy can draw it
            // at any time with the space nearly empty, where it is an ordinary
            // collision: retry instead, or a single unlucky draw would report
            // Overflow against a nearly empty generator.
            if constexpr (detail::has_invalid_sentinel_v<IdType_>)
            {
                if (raw_id == std::numeric_limits<underlying_type>::max())
                {
                    if constexpr (needs_retry)
                    {
                        continue;
                    }
                    else
                    {
                        return ErrorPolicy::report_error(IdError::Overflow);
                    }
                }
            }

            // Single-lookup: insert returns false if already present (collision)
            if (mIdsInUse.insert(raw_id))
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
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();

        underlying_type raw_id;
        if constexpr (std::is_same_v<IdType_, underlying_type>)
        {
            raw_id = id;
        }
        else
        {
            raw_id = id.get();
        }

        if (mIdsInUse.erase(raw_id) == 0)
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
     * partway through, the IDs accumulated so far are rolled back.
     *
     * Rollback behavior: every ID that came from the recycle pool is returned to
     * it, and IDs the allocation policy produced are discarded with the counter
     * rewound by exactly that many, preserving ID density for MinRecyclingPolicy
     * and similar policies. Provenance is recorded per element, not inferred
     * from the value.
     *
     * Rollback also runs if an exception escapes mid-batch (a failing allocation,
     * or a throwing id_type constructor): the caller never receives the partial
     * result, so the IDs accumulated so far are returned rather than left
     * active and unreachable.
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

        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();

        std::vector<id_type> result;
        // Provenance is RECORDED, not inferred. The previous implementation
        // compared each ID against the pre-batch maximum to guess whether it
        // had come from the recycle pool, which was wrong in two directions:
        // an ID pooled above the current maximum was misfiled as newly
        // allocated, and when the active set was empty the pre-batch maximum
        // was nullopt, so EVERY pooled ID consumed by the batch was discarded
        // on rollback. Both cases lost pool entries permanently and inflated
        // the count handed to the allocation policy's revert().
        std::vector<bool> from_pool;
        result.reserve(count);
        from_pool.reserve(count);

        // Compile-time optimization: only use retry loop for policies that may collide
        // Higher retry count (100) handles small ID types where collisions are frequent
        constexpr bool needs_retry = detail::may_collide_v<AllocationPolicy>;
        constexpr int kMaxRetries = needs_retry ? 100 : 1;

        // Commit one raw ID into the active set and the result. Ordered so the
        // throwing steps run before any state the caller can observe: the
        // id_type is constructed first (a user-supplied strong-ID constructor
        // may throw), then the active-set insertion, and only then the
        // push_backs, which cannot reallocate because both vectors are
        // reserved. If either of the first two steps throws, `raw_id` has not
        // been recorded anywhere, so the caller-visible state is exactly the
        // pre-call state once the accumulated batch is rolled back. This
        // assumes id_type moves without throwing: a throwing move constructor
        // in the push_back would leave `raw_id` in the active set but out of
        // `result`, where rollback cannot see it.
        const auto commit = [this, &result, &from_pool](underlying_type raw_id, bool pooled)
        {
            if constexpr (std::is_same_v<IdType_, underlying_type>)
            {
                (void)mIdsInUse.insert(raw_id);
                result.push_back(raw_id);
            }
            else
            {
                IdType_ typed(raw_id);
                (void)mIdsInUse.insert(raw_id);
                result.push_back(std::move(typed));
            }
            from_pool.push_back(pooled);
        };

        try
        {
            for (size_t i = 0; i < count; ++i)
            {
                // Try recycled IDs first
                if (auto recycled = RecyclingPolicy::get_recycled())
                {
                    const underlying_type raw_id = *recycled;
                    try
                    {
                        commit(raw_id, true);
                    }
                    catch (...)
                    {
                        // get_recycled() already removed it from the pool; put
                        // it back before unwinding or it exists nowhere.
                        RecyclingPolicy::add_recycled(raw_id);
                        throw;
                    }
                    continue;
                }

                // Generate new ID with retry loop (optimized at compile-time)
                bool generated = false;

                for (int attempt = 0; attempt < kMaxRetries; ++attempt)
                {
                    bool is_first = mIdsInUse.empty();
                    auto max_opt = mIdsInUse.max_element();
                    underlying_type max_id = is_first ? mBaseId : *max_opt;
                    auto new_id_opt = AllocationPolicy::next_id(max_id, is_first);

                    if (!new_id_opt)
                    {
                        // Overflow - rollback all generated IDs
                        rollback_batch(result, from_pool);
                        return make_unexpected(IdError::Overflow);
                    }

                    underlying_type raw_id = *new_id_opt;

                    // Same sentinel guard as generate(), including the same
                    // policy distinction: a random draw landing on the sentinel
                    // is a collision to retry, not exhaustion.
                    if constexpr (detail::has_invalid_sentinel_v<IdType_>)
                    {
                        if (raw_id == std::numeric_limits<underlying_type>::max())
                        {
                            if constexpr (needs_retry)
                            {
                                continue;
                            }
                            else
                            {
                                rollback_batch(result, from_pool);
                                return make_unexpected(IdError::Overflow);
                            }
                        }
                    }

                    if (mIdsInUse.count(raw_id) == 0)
                    {
                        commit(raw_id, false);
                        generated = true;
                        break;
                    }
                }

                if (!generated)
                {
                    // Collision exhaustion - rollback
                    rollback_batch(result, from_pool);
                    return make_unexpected(IdError::AlreadyInUse);
                }
            }
        }
        catch (...)
        {
            // An allocation or a throwing id_type constructor must not strand
            // the IDs already committed: the caller never receives `result`, so
            // without this they would stay active forever, unreleasable and
            // unissuable.
            rollback_batch(result, from_pool);
            throw;
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

        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();

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

            if (mIdsInUse.erase(raw_id) == 0)
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
     * @details Each element's provenance is read from @p from_pool, recorded
     * when the ID was committed. Pool IDs go back to the pool; allocator IDs
     * are discarded and the allocation policy counter is reverted by exactly
     * that count to prevent permanent sequence gaps.
     */
    void rollback_batch(const std::vector<id_type>& result, const std::vector<bool>& from_pool)
    {
        size_t newly_generated_count = 0;

        for (size_t i = 0; i < result.size(); ++i)
        {
            underlying_type raw;
            if constexpr (std::is_same_v<IdType_, underlying_type>)
            {
                raw = result[i];
            }
            else
            {
                raw = result[i].get();
            }
            mIdsInUse.erase(raw);

            // Provenance is read from the recorded flag, never guessed from the
            // value. An ID that came from the pool goes back to the pool; only
            // IDs the allocation policy actually produced are counted for
            // revert(), so the counter is rewound by exactly what it advanced.
            if (i < from_pool.size() && from_pool[i])
            {
                RecyclingPolicy::add_recycled(raw);
            }
            else
            {
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
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock_shared();
        underlying_type raw_id;
        if constexpr (std::is_same_v<IdType_, underlying_type>)
        {
            raw_id = id;
        }
        else
        {
            raw_id = id.get();
        }
        return mIdsInUse.count(raw_id) > 0;
    }

    size_t active_count() const noexcept
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock_shared();
        return mIdsInUse.size();
    }

    size_t recycled_count() const noexcept
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock_shared();
        return RecyclingPolicy::recycled_count();
    }

    void reset() noexcept
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();
        mIdsInUse.clear();
        RecyclingPolicy::clear();
        AllocationPolicy::reset(mBaseId);

        // Invalidate outstanding guards. Without this a guard created before
        // the reset releases on destruction against the post-reset state: at
        // best a spurious InvalidRelease (and a failed debug assert), at worst
        // a SUCCESSFUL release of an ID the generator has already reissued to a
        // different owner, corrupting that owner's state in release builds.
        ++mEpoch;
    }

    // =========================================================================
    // RAII Helper
    // =========================================================================

    /**
     * @brief RAII guard that automatically releases an ID when destroyed.
     *
     * @warning The IdGenerator must outlive all IdGuard instances created from it.
     *          Destroying it while guards exist is undefined behavior; moving it is
     *          a compile error under all four shipped aliases (see the class note).
     */
    class IdGuard
    {
    public:
        IdGuard() noexcept
            : mGenerator(nullptr)
            , mId{}
            , mValid(false)
            , mEpoch(0)
        {
        }

        IdGuard(IdGenerator& gen, IdType_ id) noexcept
            : mGenerator(&gen)
            , mId(id)
            , mValid(true)
            , mEpoch(gen.mEpoch)
        {
        }

        ~IdGuard()
        {
            if (mValid && mGenerator)
            {
                // Releases only if the generator has not been reset since this
                // guard was made; a stale guard is silently inert rather than
                // releasing an ID that now belongs to someone else.
                [[maybe_unused]] const bool released =
                    mGenerator->release_if_current(mId, mEpoch);
#ifndef NDEBUG
                // A failure here means a double-release or an ID that was never
                // active -- a real bug. Being stale is not a bug, so it is
                // excluded from the assertion.
                assert((released || mEpoch != mGenerator->mEpoch) &&
                       "IdGuard: release failed in destructor");
#endif
            }
        }

        // Move-only
        IdGuard(const IdGuard&) = delete;
        IdGuard& operator=(const IdGuard&) = delete;

        IdGuard(IdGuard&& other) noexcept
            : mGenerator(other.mGenerator)
            , mId(other.mId)
            , mValid(other.mValid)
            , mEpoch(other.mEpoch)
        {
            other.mValid = false;
        }

        IdGuard& operator=(IdGuard&& other) noexcept
        {
            if (this != &other)
            {
                if (mValid && mGenerator)
                {
                    // Same staleness rule as the destructor: releasing the ID
                    // this guard is dropping must not touch a post-reset
                    // generation.
                    [[maybe_unused]] const bool released =
                        mGenerator->release_if_current(mId, mEpoch);
#ifndef NDEBUG
                    assert((released || mEpoch != mGenerator->mEpoch) &&
                           "IdGuard: release failed in move assignment");
#endif
                }
                mGenerator = other.mGenerator;
                mId = other.mId;
                mValid = other.mValid;
                mEpoch = other.mEpoch;
                other.mValid = false;
            }
            return *this;
        }

        IdType_ get() const noexcept
        {
            return mId;
        }
        IdType_ operator*() const noexcept
        {
            return mId;
        }
        explicit operator bool() const noexcept
        {
            return mValid;
        }

        void release_ownership() noexcept
        {
            mValid = false;
        }

    private:
        IdGenerator* mGenerator;
        IdType_ mId;
        bool mValid;
        std::size_t mEpoch; // generator epoch at construction; see reset()
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

    // Releases @p id only if @p epoch is still the generator's current epoch.
    // reset() bumps the epoch, so a guard created before a reset becomes inert
    // instead of releasing an ID the generator has since reissued to someone
    // else. The check runs under the lock, so it cannot race a concurrent
    // reset(). Returns true if the release happened.
    bool release_if_current(IdType_ id, std::size_t epoch) noexcept
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();

        if (epoch != mEpoch)
        {
            return false;
        }

        underlying_type raw_id;
        if constexpr (std::is_same_v<IdType_, underlying_type>)
        {
            raw_id = id;
        }
        else
        {
            raw_id = id.get();
        }

        if (mIdsInUse.erase(raw_id) == 0)
        {
            return false;
        }

        RecyclingPolicy::add_recycled(raw_id);
        return true;
    }

private:
    underlying_type mBaseId;
    ActiveIdTracker<underlying_type> mIdsInUse;
    // Bumped by reset(). Outstanding IdGuards carry the epoch they were made
    // in and refuse to release across a bump.
    std::size_t mEpoch{0};
};

// =============================================================================
// Convenience Aliases
// =============================================================================

/// @brief Simple single-threaded generator with FIFO recycling
template <typename IdType = uint64_t>
using SimpleIdGenerator = IdGenerator<IdType,
                                      SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                                      ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
                                      id_generator::ExpectedErrorPolicy<IdType, IdError>,
                                      SingleThreadedPolicy>;

/// @brief Thread-safe generator with mutex synchronization
template <typename IdType = uint64_t>
using ThreadSafeIdGenerator = IdGenerator<IdType,
                                          SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                                          ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
                                          id_generator::ExpectedErrorPolicy<IdType, IdError>,
                                          MutexSynchronizationPolicy>;

/// @brief Generator prioritizing ID density (Min-First recycling)
/// @details Recommended for HPC applications where contiguous ID ranges
/// improve cache performance in ID-indexed data structures.
template <typename IdType = uint64_t>
using DenseIdGenerator = IdGenerator<IdType,
                                     SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                                     MinRecyclingPolicy<underlying_id_type_t<IdType>>,
                                     id_generator::ExpectedErrorPolicy<IdType, IdError>,
                                     SingleThreadedPolicy>;

/// @brief Random ID generator (no recycling, with retry for collisions)
template <typename IdType = uint64_t>
using RandomIdGenerator = IdGenerator<IdType,
                                      RandomAllocationPolicy<underlying_id_type_t<IdType>>,
                                      NoRecyclingPolicy<underlying_id_type_t<IdType>>,
                                      id_generator::ExpectedErrorPolicy<IdType, IdError>,
                                      SingleThreadedPolicy>;

} // namespace fat_p
