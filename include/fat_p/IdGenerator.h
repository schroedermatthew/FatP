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
 *   - Sequential, bounded-sequential, or random allocation strategies. The
 *     upper bound is set through the generator's (base_id, upper_bound)
 *     constructor, available when EITHER policy role opts in: the allocation
 *     side via an accepts_upper_bound alias, the recycling side by exposing
 *     configure_domain. Random allocation treats base_id as a MINIMUM, not a
 *     first ID.
 *   - Configurable recycling policies (FIFO, Min-First, None, Sparse)
 *   - Sparse recycling owns the COMPLETE domain as disjoint intervals, which is
 *     what makes claim(id) possible: reserving a persisted identifier costs
 *     O(log I) in the free-interval count rather than O(gap), and lower
 *     unclaimed identifiers stay issuable. Exhaustion is an empty interval set,
 *     not a question for the allocation policy.
 *   - Thread-safe and single-threaded variants
 *   - StrongId integration for type safety, including exclusion of the reserved
 *     invalid() sentinel from the issuable domain at construction
 *   - Expected-based error handling
 *   - RAII IdGuard for automatic cleanup
 *   - O(1) active ID tracking via unordered_set with lazy max
 *   - Batch generation/release with single lock acquisition
 *   - Seeded random generation for reproducibility
 *
 * @version 1.5
 */

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <forward_list>
#include <limits>
#include <map>
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
    AlreadyInUse,
    /// @brief A claimed identifier lies outside the configured domain, or the
    /// free-set state contradicts the active set. Distinct from AlreadyInUse,
    /// which means the caller named an identifier that is currently active.
    InvalidClaim
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

// Opt-in marker: an allocation policy that takes (base_id, upper_bound) and
// wants IdGenerator's two-argument constructor to forward both. Detection is by
// a DECLARED alias, never by constructibility. When this was written, a
// constructibility test also matched RandomAllocationPolicy(uint64_t seed, int)
// and would have silently built a seeded random policy where a bounded one was
// requested. That constructor now takes a seed_tag_t, so the specific impostor
// is gone -- but the reason stands: an intent as consequential as "this policy
// consumes a bound" should be declared, not inferred from a call being
// well-formed, and the next two-argument policy would reintroduce it.
template <typename T, typename = void>
struct accepts_upper_bound : std::false_type
{
};

template <typename T>
struct accepts_upper_bound<T, std::void_t<typename T::accepts_upper_bound>> : std::true_type
{
};

template <typename T>
inline constexpr bool accepts_upper_bound_v = accepts_upper_bound<T>::value;

// A recycling policy that learns its domain through configure_domain(base, upper).
// Detected by the expression, not by a marker alias: unlike the allocation-side
// opt-in above there is no same-arity impostor to confuse it with, and detecting
// the call means a policy cannot claim the seam without providing it.
template <typename T, typename = void>
struct has_domain_configure : std::false_type
{
};

template <typename T>
struct has_domain_configure<
    T,
    std::void_t<decltype(std::declval<T&>().configure_domain(
        std::declval<typename T::value_type>(), std::declval<typename T::value_type>()))>>
    : std::true_type
{
};

template <typename T>
inline constexpr bool has_domain_configure_v = has_domain_configure<T>::value;

// Is that seam nothrow? This is what makes IdGenerator::reset() conditionally
// noexcept: a full-domain policy rebuilds its domain there, and the rebuild is
// allocation-free in every state reachable WITHOUT a move but not in the
// moved-from state, which a movable instantiation can reach.
template <typename T, typename = void>
struct domain_configure_is_noexcept : std::true_type
{
};

template <typename T>
struct domain_configure_is_noexcept<T, std::enable_if_t<has_domain_configure<T>::value>>
    : std::bool_constant<noexcept(std::declval<T&>().configure_domain(
          std::declval<typename T::value_type>(), std::declval<typename T::value_type>()))>
{
};

template <typename T>
inline constexpr bool domain_configure_is_noexcept_v = domain_configure_is_noexcept<T>::value;

// Opt-in marker: a recycling policy that owns the COMPLETE domain [base, upper]
// rather than only the identifiers released back to it. Such a policy exposes
// peek_lowest/remove_lowest/claim_at, and its empty free set -- not the
// allocation policy -- is what exhaustion means. A positive property a policy
// declares about itself, never a blacklist of the policies that lack it.
template <typename T, typename = void>
struct is_full_domain_policy : std::false_type
{
};

template <typename T>
struct is_full_domain_policy<T, std::void_t<typename T::is_full_domain_policy>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_full_domain_policy_v = is_full_domain_policy<T>::value;

// An ordering that a full-domain interval map may actually be built on.
//
// SparseRecyclingPolicy mixes map order with raw interval arithmetic, so it is
// correct ONLY for orderings equivalent to ascending numeric order. Left
// unconstrained, `SparseRecyclingPolicy<T, std::greater<T>>` compiles and
// silently falsifies all four invariants of the representation -- a hole that
// did not exist before the parameter was added, and that a runtime assert
// documents rather than closes. Requiring an explicit opt-in makes the wrong
// comparator a COMPILE error and leaves the parameter useful for the one thing
// it exists for: instrumenting the complexity contract.
template <typename C, typename = void>
struct is_ascending_order : std::false_type
{
};

template <typename C>
struct is_ascending_order<C, std::void_t<typename C::ascending_order>> : std::true_type
{
};

// The standard orderings that ARE ascending. As with is_sequential_policy, do
// not let a type match both this and the void_t specialization above: the two
// would be ambiguous, because the void_t argument is a non-deduced context.
template <typename T>
struct is_ascending_order<std::less<T>, void> : std::true_type
{
};

template <typename T>
inline constexpr bool is_ascending_order_v = is_ascending_order<T>::value;

// May the interval map's noexcept operations call this comparator safely?
//
// std::less is admitted directly: the standard does not declare its operator()
// noexcept (MSVC does, libstdc++ does not), yet comparing two unsigned integers
// cannot throw anywhere. Demanding the trait of it would reject the library's
// own default on GCC. Any other comparator must declare noexcept.
template <typename C, typename IdT>
inline constexpr bool comparator_is_nothrow_v =
    std::is_same_v<C, std::less<IdT>> ||
    std::is_nothrow_invocable_r_v<bool, const C&, const IdT&, const IdT&>;

// What IdGenerator actually requires of a concurrency policy: an exclusive lock
// on a mutable policy, and a shared lock on a CONST one (the query methods hold
// the policy const). This is deliberately narrower than fat_p::ConcurrencyPolicy,
// which additionally requires LockGuard/SharedGuard aliases that IdGenerator
// never names, and which does not require lock_shared() to be const-callable.
//
// This is a diagnostic constraint, not a synchronization guarantee: the DEFAULT
// policy provides no mutual exclusion at all, by design. It turns a mis-supplied
// type into one clear error at the point of instantiation instead of a cascade
// from deep inside a member.
template <typename P>
concept UsableConcurrencyPolicy = requires(P p, const P cp)
{
    p.lock();
    cp.lock_shared();
};

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
// Seeded-construction tag
// =============================================================================

/// @brief Disambiguation tag for seeded construction.
///
/// @details Declared here, above the allocation policies, because
/// RandomAllocationPolicy takes it directly. That constructor previously read
/// `(uint64_t seed, int ignored)`, so `RandomAllocationPolicy<uint32_t>
/// p(1000, 5000)` -- written by someone reasonably reading those as
/// `(base, upper_bound)` -- compiled clean and silently discarded the base,
/// reinstating the exact defect the one-argument constructor was fixed to
/// remove. A tag cannot be mistaken for a bound, and the generator already
/// presented this same tag on its own seeded constructor.
struct seed_tag_t
{
    explicit seed_tag_t() = default;
};

/// @brief Constant for seeded construction: `IdGenerator(seed_tag, 42)`
inline constexpr seed_tag_t seed_tag{};

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
    /// @param base_id The MINIMUM ID this policy will draw. Unlike the
    ///        sequential policies, where base_id is the first ID issued, a
    ///        random policy has no "first" -- it draws uniformly from
    ///        [base_id, max]. Previously this argument was accepted and
    ///        discarded, so a generator constructed with a base produced IDs
    ///        below it.
    explicit RandomAllocationPolicy(IdType base_id = 0)
        : mBaseId(base_id)
        , mRng(std::random_device{}())
        , mDist(static_cast<DistType>(base_id),
                static_cast<DistType>(std::numeric_limits<IdType>::max()))
    {
    }

    /// @brief Construct with explicit seed for reproducible randomness
    /// @param tag Disambiguation tag (use `seed_tag`)
    /// @param seed The seed value for the RNG
    RandomAllocationPolicy(seed_tag_t /*tag*/, uint64_t seed)
        : mBaseId(0)
        , mRng(seed)
        , mDist(static_cast<DistType>(0), static_cast<DistType>(std::numeric_limits<IdType>::max()))
    {
    }

    std::optional<IdType> next_id(IdType, bool = false) noexcept
    {
        // No try-catch needed: uniform_int_distribution doesn't throw
        return static_cast<IdType>(mDist(mRng));
    }

    void reset(IdType base_id = 0) noexcept
    {
        // Rebuild the distribution: reset(base) must honor the new lower bound,
        // or a reset generator would resume drawing below it.
        mBaseId = base_id;
        mDist = std::uniform_int_distribution<DistType>(
            static_cast<DistType>(base_id), static_cast<DistType>(std::numeric_limits<IdType>::max()));
        mRng.seed(std::random_device{}());
    }

    /// @brief Reset with explicit seed for reproducible randomness
    void reset_with_seed(uint64_t seed) noexcept
    {
        mRng.seed(seed);
    }

private:
    IdType mBaseId;
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
    /// Opts this policy into IdGenerator's (base_id, upper_bound) constructor.
    /// Without it the bound is unreachable through the generator and this policy
    /// silently behaves as an unbounded one.
    using accepts_upper_bound = void;

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

/// @brief Trait to detect a sequential allocation policy.
///
/// @details This is the ALLOCATION side of the full-domain pairing check. A
/// trait on the recycling policy cannot constrain what it is paired with, so
/// the generator needs a separate predicate to refuse random allocation under a
/// full-domain recycling policy -- where issuance comes from the free set and a
/// random draw has nothing to contribute.
///
/// The two shipped policies are recognized by specialization; anyone else may
/// opt in with `using is_sequential_policy = void;`. Without that door a custom
/// sequential policy could never be paired with the sparse policy at all, and
/// the pairing check would be a whitelist of two rather than a property.
template <typename T, typename = void>
struct is_sequential_policy : std::false_type
{
};

template <typename T>
struct is_sequential_policy<T, std::void_t<typename T::is_sequential_policy>> : std::true_type
{
};

// DO NOT declare `using is_sequential_policy = void;` on either policy below.
// The void_t specialization above and these two would then both match with a
// second argument of `void`, and partial ordering cannot separate them because
// the void_t argument is a non-deduced context -- the trait stops compiling.
// The symmetry is tempting precisely because BoundedSequentialAllocationPolicy
// already declares `accepts_upper_bound`.
template <typename IdType>
struct is_sequential_policy<SequentialAllocationPolicy<IdType>, void> : std::true_type
{
};

template <typename IdType>
struct is_sequential_policy<BoundedSequentialAllocationPolicy<IdType>, void> : std::true_type
{
};

template <typename T>
inline constexpr bool is_sequential_policy_v = is_sequential_policy<T>::value;

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

/**
 * @brief Full-domain recycling: the policy owns every identifier in
 * `[base, upper_bound]`, free or not, as a set of disjoint inclusive intervals.
 *
 * @details The three policies above hold only what was released back to them,
 * and the allocation policy decides what has never been issued. This one holds
 * both, which is what makes an identifier CLAIMABLE by value: a caller can ask
 * for 60000 out of an empty generator without first consuming 0..59999, and the
 * generator still knows that 60000 is spoken for.
 *
 * Intervals are stored in an ordered map keyed by UPPER bound so the lowest
 * free identifier is `begin()->second` and taking it does not change a key:
 *
 * @code
 *   upper -> lower
 *   4     -> 1        [1, 4]
 *   59999 -> 6        [6, 59999]
 *   LIMIT -> 60001    [60001, upper_bound]
 * @endcode
 *
 * Invariants, observed BETWEEN operations: intervals are disjoint and
 * non-adjacent (adjacent ones merge immediately); the union of the free
 * intervals and the generator's active set is exactly `[base, upper_bound]`;
 * and the two are disjoint. Inside an operation, under the generator's lock, an
 * identifier may be momentarily in both sets or in neither -- every such window
 * is closed by a step that cannot throw.
 *
 * **Exhaustion is an empty map**, not a count of zero and not a sentinel value.
 *
 * @section return_credits Return credits
 *
 * `add_recycled()` is `noexcept` and inserts into a map. That is only honest
 * because the node it may need was already allocated: each activation reserves
 * a **return credit** -- a detached map node -- before the identifier becomes
 * active, and every path that hands an identifier back consumes or destroys
 * one. Release, batch rollback, and the ordinary reset are therefore
 * allocation-free by construction rather than by hiding a `bad_alloc` behind a
 * `noexcept` boundary.
 *
 * A reservoir of nodes freed by exhausted intervals does NOT fund this, which
 * is why credits are per-activation:
 *
 * @code
 *   free [0, MAX];  generate 0 -> [1, MAX]   (no node freed)
 *                   generate 1 -> [2, MAX]   (no node freed)
 *                   release 0  -> needs [0, 0], reservoir empty
 * @endcode
 *
 * The credit count equals the active count at every operation boundary. Credits
 * are held in a `std::forward_list`, not a vector: the stack must release
 * storage as credits are spent, or a generator that briefly held a million
 * identifiers would keep that footprint for its lifetime.
 *
 * @warning Only usable through a generator that configures it. A
 * default-constructed policy has an empty domain, which reads as exhausted;
 * IdGenerator calls `configure_domain()` during construction and again on
 * `reset()`.
 */
/// @tparam Compare Ordering for the interval map. **It must induce ASCENDING
///         numeric order**, and it must SAY SO: `std::less<IdType>`, or a type
///         declaring `using ascending_order = void;`. Anything else is a compile
///         error.
///
///         This is not a general ordering seam despite its shape. Five sites mix
///         map order with raw arithmetic: `peek_lowest()` returns
///         `begin()->second` *as the lowest free identifier*; `is_free()`,
///         `reserve_claim_credits()` and `claim_at()` do `lower_bound(id)` then
///         test `it->second <= id`; `add_recycled()` finds the right neighbour
///         with `upper_bound(id)`. Under `std::greater` the policy would be
///         silently wrong -- `claim()` of a wholly free identifier reports
///         `InvalidClaim`, generation runs descending, and every invariant in
///         the representation is false. That is a state the type could not
///         reach before this parameter existed, so the parameter must not be
///         able to reach it either: the opt-in is what keeps that true, and the
///         runtime assert in `configure_domain()` is only a second line.
///
///         The parameter exists because the complexity contract has no other
///         witness: a counting comparator is the only instrument that separates
///         `claim()` from the generate-and-release walk it replaces, since both
///         leave one identifier active and everything else free. An instrument
///         that can also introduce a defect is not worth its measurement, which
///         is why this is constrained rather than merely documented.
template <typename IdType = uint64_t, typename Compare = std::less<IdType>>
class SparseRecyclingPolicy
{
public:
    // Ordering was constrained; its EXCEPTION behaviour was left to prose --
    // the same mistake one layer down. is_free(), claim_at() and add_recycled()
    // are noexcept and reach Compare through map lookup and insertion, so a
    // throwing comparator terminates rather than propagating.
    //
    // std::less is accepted without the trait because the standard does not
    // declare its operator() noexcept -- MSVC does, libstdc++ does not -- while
    // comparing two unsigned integers cannot throw on any implementation.
    // Requiring the trait outright would reject the library's own default
    // comparator on GCC, which is a portability regression, not a safety gain.
    // Every OTHER comparator must say so.
    static_assert(detail::comparator_is_nothrow_v<Compare, IdType>,
                  "SparseRecyclingPolicy's Compare must be nothrow-invocable: the policy's "
                  "noexcept operations reach it through std::map, so a throwing comparator "
                  "terminates rather than propagating. Declare operator() noexcept.");

    static_assert(std::is_nothrow_move_constructible_v<Compare> &&
                      std::is_nothrow_default_constructible_v<Compare>,
                  "and nothrow to construct and move, because the policy's own move operations "
                  "are noexcept over containers that hold it.");

    static_assert(detail::is_ascending_order_v<Compare>,
                  "SparseRecyclingPolicy's Compare must induce ASCENDING order and declare it: "
                  "use std::less<IdType>, or declare `using ascending_order = void;` on your "
                  "comparator. The policy mixes map order with interval arithmetic, so a "
                  "descending order would compile and silently falsify every invariant of the "
                  "interval representation.");

    using value_type = IdType;
    using key_compare = Compare;

    /// @brief Opt-in marker for detail::is_full_domain_policy_v.
    using is_full_domain_policy = void;

    // =========================================================================
    // Special members
    // =========================================================================

    SparseRecyclingPolicy() = default;
    ~SparseRecyclingPolicy() = default;

    // Return credits are detached map nodes, which are move-only, so the policy
    // could never be copied. Say so rather than leaving it to be inferred.
    SparseRecyclingPolicy(const SparseRecyclingPolicy&) = delete;
    SparseRecyclingPolicy& operator=(const SparseRecyclingPolicy&) = delete;

    /**
     * @brief Move, leaving the source consistent rather than merely empty.
     *
     * @details These are declared rather than defaulted because the implicit
     * versions get it WRONG in a way nothing would notice until a rebuild: the
     * containers move out and are left empty, but `mCreditCount` is a scalar and
     * is COPIED. A moved-from source then reports credits it does not hold, and
     * `configure_domain()`'s reuse arm -- which keys off `mCreditCount > 0` --
     * calls `take_credit()` on an empty list.
     *
     * Zeroing the source's count also makes the documented basis for
     * `reset()`'s conditional `noexcept` true: the moved-from state really is
     * empty map, no actives, no credits, so the rebuild really does take the
     * allocating arm.
     */
    SparseRecyclingPolicy(SparseRecyclingPolicy&& other) noexcept
        : mFree(std::move(other.mFree))
        , mNodeSource(std::move(other.mNodeSource))
        , mCredits(std::move(other.mCredits))
        , mCreditCount(other.mCreditCount)
        , mBase(other.mBase)
        , mUpper(other.mUpper)
    {
        other.mCreditCount = 0;
    }

    SparseRecyclingPolicy& operator=(SparseRecyclingPolicy&& other) noexcept
    {
        if (this != &other)
        {
            // clear() then swap() rather than container move-assignment: both
            // are unconditionally noexcept, which move-assignment is not, and a
            // potentially-throwing base would silently DELETE IdGenerator's
            // `operator=(IdGenerator&&) noexcept = default`.
            mFree.clear();
            mNodeSource.clear();
            mCredits.clear();

            mFree.swap(other.mFree);
            mNodeSource.swap(other.mNodeSource);
            mCredits.swap(other.mCredits);

            mCreditCount = other.mCreditCount;
            other.mCreditCount = 0;
            mBase = other.mBase;
            mUpper = other.mUpper;
        }
        return *this;
    }

    // =========================================================================
    // Domain configuration
    // =========================================================================

    /**
     * @brief Rebuild the free domain to exactly `[base, upper]`.
     *
     * @details Called by IdGenerator at construction and from `reset()`. It does
     * NOT build a replacement map; it reuses a node it already owns -- any node
     * from a non-empty map, or one return credit when the map is empty because
     * the domain is fully claimed. Both cases are allocation-free.
     *
     * The one state that must allocate is a MOVED-FROM policy, whose map,
     * credits, and the generator's active set are all empty at once. That state
     * is unreachable through the shipped aliases (SingleThreadedPolicy is
     * immovable, which suppresses the generator's moves) and reachable through
     * an instantiation over UniqueRWLockPolicy, so this function is not
     * `noexcept` and IdGenerator::reset() is conditionally `noexcept` because of
     * it. Construction takes the same path, and construction may allocate.
     *
     * `base > upper` is a precondition violation. It is nonetheless handled,
     * because the release build is where it bites: this policy is the ONLY
     * guard against issuing a StrongId's reserved sentinel -- the full-domain
     * path has no late `generate()` check to fall back on -- and an inverted
     * interval hands that sentinel straight out. `SparseIdGenerator<Id>(255)`
     * over a `StrongId<uint8_t>` is the whole recipe: the ceiling normalizes to
     * 254 below a base of 255. An empty domain reports exhaustion instead,
     * matching what the non-sparse path does in the same situation.
     *
     * @param base Lowest identifier in the domain
     * @param upper Highest identifier in the domain (inclusive)
     * @note `base > upper` is not a precondition of this function. It is a
     *       defined, tested outcome: an empty (exhausted) domain. The assert
     *       below therefore sits AFTER that branch, not before it -- placing it
     *       first made the function abort on the one input it exists to handle,
     *       which no Release build could see and which stopped an
     *       assert-enabled run of the suite at its first sentinel test.
     */
    void configure_domain(IdType base, IdType upper)
    {
        if (upper < base)
        {
            mBase = base;
            mUpper = upper;
            mFree.clear();
            drop_all_credits();
            return;
        }

        // Reached only for a well-ordered request, which is what makes this a
        // meaningful check on the COMPARATOR rather than on the arguments. The
        // compile-time is_ascending_order_v constraint is the primary guard;
        // this catches a comparator that declares the marker and then lies.
        assert((base == upper || Compare{}(base, upper)) &&
               "SparseRecyclingPolicy requires a Compare inducing ASCENDING order");

        mBase = base;
        mUpper = upper;

        if (!mFree.empty())
        {
            NodeType node = mFree.extract(mFree.begin());
            mFree.clear();
            rewrite(node, upper, base);
            (void)mFree.insert(std::move(node));
        }
        else if (mCreditCount > 0)
        {
            NodeType node = take_credit();
            rewrite(node, upper, base);
            (void)mFree.insert(std::move(node));
        }
        else
        {
            // Construction, or a moved-from policy. The only allocating path.
            (void)mFree.emplace(upper, base);
        }

        drop_all_credits();
    }

    /// @brief Is @p id inside the configured domain? A domain test, not a free-set test.
    bool is_in_domain(IdType id) const noexcept
    {
        return id >= mBase && id <= mUpper;
    }

    // =========================================================================
    // Generation path -- peek then remove, never a pop
    // =========================================================================

    /**
     * @brief The lowest free identifier, or nullopt when the domain is exhausted.
     *
     * @details Deliberately NOT `get_recycled()`. That accessor is a pop, and
     * the generator must know the identifier BEFORE the free set is mutated: it
     * reserves the return credit and inserts into the active set first, so a
     * throw from either leaves the free set untouched. With a pop, a throwing
     * active-set insert would leave the identifier in neither set, and because
     * exhaustion is an empty map it could never be recovered.
     */
    std::optional<IdType> peek_lowest() const noexcept
    {
        if (mFree.empty())
        {
            return std::nullopt;
        }
        return mFree.begin()->second;
    }

protected:
    // ---- Mutating protocol. Every operation below carries a sequencing or
    // state precondition that only IdGenerator can honour, and violating one is
    // silent under NDEBUG: a bare reserve_credit() falsifies credits == actives
    // with no diagnostic anywhere, and claim_at() on a non-free identifier walks
    // off the end of the map inside a noexcept function. These were public, with
    // an @warning saying "only usable through a generator that configures it" --
    // a sentence doing an access specifier's job.
    //
    // IdGenerator reaches them through private inheritance, which grants access
    // to protected base members; so does its nested CreditGuard, and so does a
    // derived policy that shadows them.

    /**
     * @brief Remove the identifier that `peek_lowest()` just reported.
     *
     * @details Advances the first interval's lower bound, or erases it when it
     * held one value. Never splits, therefore never allocates, therefore cannot
     * throw. Removing an ARBITRARY free identifier can split and is `claim_at()`
     * -- a different operation with a different cost, kept distinct in the type
     * system rather than in a precondition comment.
     *
     * @pre The map is non-empty: a prior `peek_lowest()` returned a value under
     * the same lock.
     */
    void remove_lowest() noexcept
    {
        assert(!mFree.empty() && "remove_lowest() requires a prior peek_lowest()");

        auto it = mFree.begin();
        if (it->second == it->first)
        {
            mFree.erase(it);
        }
        else
        {
            ++(it->second);
        }
    }

    // =========================================================================
    // Claim path
    // =========================================================================

public:
    /// @brief Is @p id currently free? False for an active or out-of-domain identifier.
    bool is_free(IdType id) const noexcept
    {
        auto it = mFree.lower_bound(id);
        return it != mFree.end() && it->second <= id;
    }

protected:
    /**
     * @brief Reserve the nodes a claim of @p id will need, before anything is mutated.
     *
     * @details One return credit for the activation, plus one more when the
     * claim splits an interval. Strongly exception-safe: if the second
     * reservation throws, the first is released, so the credit count still
     * equals the active count at the boundary.
     *
     * @pre `is_free(id)`.
     * @return The number of credits reserved, for the caller's unwind guard.
     */
    std::size_t reserve_claim_credits(IdType id)
    {
        auto it = mFree.lower_bound(id);
        assert(it != mFree.end() && it->second <= id && "reserve_claim_credits() requires a free id");

        const bool splits = it->second < id && id < it->first;
        const std::size_t needed = splits ? 2u : 1u;

        std::size_t done = 0;
        try
        {
            for (; done < needed; ++done)
            {
                reserve_credit();
            }
        }
        catch (...)
        {
            while (done-- > 0)
            {
                discard_credit();
            }
            throw;
        }
        return needed;
    }

    /**
     * @brief Remove @p id from the free set, consuming the reserved nodes.
     *
     * @details Erases, trims, or splits. Neither `id - 1` nor `id + 1` is ever
     * evaluated at a domain endpoint: each is guarded by the comparison that
     * makes it meaningful. Trimming the back rekeys through a node handle rather
     * than erase-and-insert, which would deallocate and reallocate.
     *
     * @pre `reserve_claim_credits(id)` succeeded under the same lock and the
     * free set has not changed since.
     */
    void claim_at(IdType id) noexcept
    {
        auto it = mFree.lower_bound(id);
        assert(it != mFree.end() && it->second <= id && "claim_at() requires a free id");

        const IdType lower = it->second;
        const IdType upper = it->first;

        if (lower == id && upper == id)
        {
            mFree.erase(it);
        }
        else if (lower == id)
        {
            ++(it->second);
        }
        else if (upper == id)
        {
            NodeType node = mFree.extract(it);
            rewrite(node, static_cast<IdType>(id - 1), lower);
            (void)mFree.insert(std::move(node));
        }
        else
        {
            // Interior: the surviving entry keeps its key and becomes the RIGHT
            // fragment; the reserved node carries the left one.
            it->second = static_cast<IdType>(id + 1);
            NodeType node = take_credit();
            rewrite(node, static_cast<IdType>(id - 1), lower);
            (void)mFree.insert(std::move(node));
        }
    }

    // =========================================================================
    // Activation credits
    // =========================================================================

    /// @brief Reserve one return credit. The only allocating step of an activation.
    void reserve_credit()
    {
        (void)mNodeSource.emplace(IdType{}, IdType{});
        NodeType node = mNodeSource.extract(mNodeSource.begin());
        mCredits.push_front(std::move(node));
        ++mCreditCount;
    }

    /// @brief Release an unused reservation during unwind, restoring credits == actives.
    void discard_credit() noexcept
    {
        assert(mCreditCount > 0 && "discard_credit() without a reservation");
        mCredits.pop_front();
        --mCreditCount;
    }

public:
    /// @brief Reserved return credits. Equals the active count at every operation boundary.
    std::size_t credit_count() const noexcept
    {
        return mCreditCount;
    }

    // =========================================================================
    // RecyclingPolicy surface
    // =========================================================================

protected:
    /**
     * @brief Return @p id to the free set, merging with adjacent intervals.
     *
     * @details Exactly one of four transitions, all allocation-free because this
     * activation's return credit was reserved when the identifier was taken:
     *
     *  1. Both neighbours adjacent: merge through @p id. The RIGHT entry
     *     survives -- its key is already the merged upper bound -- taking the
     *     left's lower bound; the left entry is erased. Credit destroyed.
     *  2. Left neighbour adjacent: extend its upper bound to @p id, which
     *     rekeys through a node handle. Credit destroyed.
     *  3. Right neighbour adjacent: lower its lower bound to @p id, a
     *     value-only write. Credit destroyed.
     *  4. Neither: insert the singleton `[id, id]`, consuming the credit.
     *
     * Adjacency arithmetic is guarded at both domain endpoints.
     *
     * @pre @p id was active and has just been erased from the active set.
     */
    void add_recycled(IdType id) noexcept
    {
        auto left = mFree.end();
        if (id > mBase)
        {
            left = mFree.find(static_cast<IdType>(id - 1));
        }

        auto right = mFree.end();
        if (id < mUpper)
        {
            auto it = mFree.upper_bound(id);
            if (it != mFree.end() && it->second == static_cast<IdType>(id + 1))
            {
                right = it;
            }
        }

        if (left != mFree.end() && right != mFree.end())
        {
            right->second = left->second;
            mFree.erase(left);
            discard_credit();
        }
        else if (left != mFree.end())
        {
            NodeType node = mFree.extract(left);
            const IdType lower = node.mapped();
            rewrite(node, id, lower);
            (void)mFree.insert(std::move(node));
            discard_credit();
        }
        else if (right != mFree.end())
        {
            right->second = id;
            discard_credit();
        }
        else
        {
            NodeType node = take_credit();
            rewrite(node, id, id);
            (void)mFree.insert(std::move(node));
        }
    }

public:
    /**
     * @brief The number of currently FREE identifiers, including never-issued ones.
     *
     * @details Sums inclusive interval cardinalities in O(I). Saturates at
     * `SIZE_MAX`, per term as well as in the sum: `[0, 2^64-1]` has cardinality
     * `2^64`, and computing it as `upper - lower + 1` wraps to zero. A zero
     * result from a non-empty map would invert this query's meaning, since
     * exhaustion is an empty map -- so the span is computed first and saturated
     * before the increment.
     */
    size_t recycled_count() const noexcept
    {
        constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();

        std::size_t total = 0;
        for (const auto& entry : mFree)
        {
            const auto span = static_cast<std::uint64_t>(entry.first - entry.second);
            if (span >= static_cast<std::uint64_t>(kMax))
            {
                return kMax;
            }
            const std::size_t cardinality = static_cast<std::size_t>(span) + 1u;
            if (total > kMax - cardinality)
            {
                return kMax;
            }
            total += cardinality;
        }
        return total;
    }

    /**
     * @brief Drop all state without rebuilding the domain.
     *
     * @details The destructor's operation. NOT reset's: an empty map means
     * exhausted, so a `clear()`-only reset would silently degrade the generator
     * to plain sequential allocation while reporting nothing. `reset()` reaches
     * this policy through `configure_domain()` instead.
     */
    void clear() noexcept
    {
        mFree.clear();
        mNodeSource.clear();
        drop_all_credits();
    }

    /// @brief Free interval count. Exposed so tests can assert canonicalization.
    std::size_t interval_count() const noexcept
    {
        return mFree.size();
    }

private:
    using IntervalMap = std::map<IdType, IdType, Compare>; // upper -> lower
    using NodeType = typename IntervalMap::node_type;

    static void rewrite(NodeType& node, IdType upper, IdType lower) noexcept
    {
        node.key() = upper;
        node.mapped() = lower;
    }

    NodeType take_credit() noexcept
    {
        assert(mCreditCount > 0 && "take_credit() without a reservation");
        NodeType node = std::move(mCredits.front());
        mCredits.pop_front();
        --mCreditCount;
        return node;
    }

    void drop_all_credits() noexcept
    {
        mCredits.clear();
        mCreditCount = 0;
    }

    IntervalMap mFree;

    /// @brief Node factory. Empty between operations; `reserve_credit()` inserts
    /// one scratch entry and immediately extracts it to obtain a detached node.
    IntervalMap mNodeSource;

    /// @brief Reserved return credits. forward_list, not vector: storage must
    /// shrink as credits are spent, so the bound stays O(active), not O(peak).
    std::forward_list<NodeType> mCredits;

    std::size_t mCreditCount{0};
    IdType mBase{0};
    IdType mUpper{std::numeric_limits<IdType>::max()};
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
          detail::UsableConcurrencyPolicy ConcurrencyPolicy = SingleThreadedPolicy>
class IdGenerator : private AllocationPolicy, private RecyclingPolicy, private ConcurrencyPolicy
{
public:
    using id_type = IdType_;
    using result_type = typename ErrorPolicy::result_type;
    using underlying_type = underlying_id_type_t<IdType_>;

private:
    /// @brief Does the recycling policy own the whole domain rather than only what was released?
    static constexpr bool kFullDomain = detail::is_full_domain_policy_v<RecyclingPolicy>;

    static_assert(!kFullDomain || detail::is_sequential_policy_v<AllocationPolicy>,
                  "A full-domain RecyclingPolicy (SparseRecyclingPolicy) requires a sequential "
                  "AllocationPolicy. Issuance comes from the free set, so a random draw has "
                  "nothing to contribute, and the free set -- not the allocation policy -- is "
                  "what exhaustion means.");

    static_assert(!kFullDomain || detail::has_domain_configure_v<RecyclingPolicy>,
                  "A full-domain RecyclingPolicy must also model has_domain_configure_v: a policy "
                  "that owns the whole domain but cannot be told what the domain IS is exactly the "
                  "defect the full-domain contract exists to prevent.");

    /**
     * @brief Normalize a requested upper bound against the ID type's reserved sentinel.
     *
     * @details For a StrongId-style type the underlying maximum denotes
     * `invalid()` and must never be issued, so the effective bound is one less.
     * Doing this at DOMAIN CONSTRUCTION rather than as a late generation guard
     * keeps the free set equal to the set of issuable identifiers: `claim()` of
     * the sentinel is out of domain, `recycled_count()` never counts an
     * unissuable value, and exhaustion still means an empty interval map.
     *
     * A future wrapper declaring `invalid()` at some other value would need the
     * sentinel trait strengthened to expose that value first; this does not
     * infer arbitrary sentinel placement from the function name alone.
     */
    static constexpr underlying_type effective_upper_bound(underlying_type requested) noexcept
    {
        if constexpr (detail::has_invalid_sentinel_v<IdType_>)
        {
            constexpr auto kSentinel = std::numeric_limits<underlying_type>::max();
            return requested >= kSentinel ? static_cast<underlying_type>(kSentinel - 1) : requested;
        }
        else
        {
            return requested;
        }
    }

    static constexpr underlying_type kDefaultUpperBound =
        effective_upper_bound(std::numeric_limits<underlying_type>::max());

    // Role-directed forwarding tags. An allocation policy that models
    // accepts_upper_bound_v is CONSTRUCTED with the bound; one that does not
    // keeps its shipped single-argument construction. Neither pretends to be the
    // other, and no temporary policy is constructed and moved in -- that would
    // impose an accidental movability requirement on custom policies.
    struct bounded_alloc_tag
    {
    };
    struct plain_alloc_tag
    {
    };

    using alloc_tag = std::conditional_t<detail::accepts_upper_bound_v<AllocationPolicy>,
                                         bounded_alloc_tag,
                                         plain_alloc_tag>;

    IdGenerator(bounded_alloc_tag, underlying_type base_id, underlying_type upper_bound)
        : AllocationPolicy(base_id, upper_bound)
        , RecyclingPolicy()
        , ConcurrencyPolicy()
        , mBaseId(base_id)
        , mUpperBound(upper_bound)
        , mIdsInUse()
    {
        configure_recycling_domain();
    }

    IdGenerator(plain_alloc_tag, underlying_type base_id, underlying_type upper_bound)
        : AllocationPolicy(base_id)
        , RecyclingPolicy()
        , ConcurrencyPolicy()
        , mBaseId(base_id)
        , mUpperBound(upper_bound)
        , mIdsInUse()
    {
        configure_recycling_domain();
    }

    /// @brief The guarded recycling-side domain seam. A no-op for the three
    /// shipping policies, which do not model it, so their behavior is unchanged.
    void configure_recycling_domain() noexcept(
        detail::domain_configure_is_noexcept_v<RecyclingPolicy>)
    {
        if constexpr (detail::has_domain_configure_v<RecyclingPolicy>)
        {
            RecyclingPolicy::configure_domain(mBaseId, mUpperBound);
        }
    }

    static constexpr underlying_type to_underlying(IdType_ id) noexcept
    {
        if constexpr (std::is_same_v<IdType_, underlying_type>)
        {
            return id;
        }
        else
        {
            return id.get();
        }
    }

    result_type report_id(underlying_type raw_id) const
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

    /**
     * @brief Undo return-credit reservations if the activation does not complete.
     *
     * @details The active-set insert is the one throwing step between reserving
     * a credit and consuming it. Without this the throw path would leak a credit
     * on every failure and the credits == actives relation would drift upward --
     * which matters because reset()'s exhausted case depends on it.
     */
    class CreditGuard
    {
    public:
        CreditGuard(IdGenerator& gen, std::size_t count) noexcept
            : mGen(&gen)
            , mCount(count)
        {
        }

        ~CreditGuard()
        {
            for (std::size_t i = 0; i < mCount; ++i)
            {
                mGen->RecyclingPolicy::discard_credit();
            }
        }

        void commit() noexcept
        {
            mCount = 0;
        }

        CreditGuard(const CreditGuard&) = delete;
        CreditGuard& operator=(const CreditGuard&) = delete;

    private:
        IdGenerator* mGen;
        std::size_t mCount;
    };

    /**
     * @brief Make @p raw_id active, funding its eventual release first.
     *
     * @details For a full-domain policy the return credit is reserved BEFORE the
     * active-set insert, so `release()` can be nothrow by construction rather
     * than by hiding a bad_alloc behind a noexcept boundary. Nothing in the free
     * set is touched here: the caller removes the identifier only after this
     * returns, so a throw from either step leaves the free set intact.
     */
    void make_active(underlying_type raw_id)
    {
        if constexpr (kFullDomain)
        {
            RecyclingPolicy::reserve_credit();
            CreditGuard guard(*this, 1);
            (void)mIdsInUse.insert(raw_id);
            guard.commit();
        }
        else
        {
            (void)mIdsInUse.insert(raw_id);
        }
    }

public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct over the ID type's full domain, starting at @p base_id.
     *
     * @details Delegates through the same role dispatch as the two-argument form
     * using the default request. An opted-in allocation policy receives the
     * effective default bound; an opted-in recycling policy receives it through
     * `configure_domain`; a policy that opts into neither keeps its shipped
     * construction.
     */
    explicit IdGenerator(underlying_type base_id = 0)
        : IdGenerator(alloc_tag{}, base_id, kDefaultUpperBound)
    {
    }

    /**
     * @brief Construct with an explicit upper bound.
     *
     * @details Available when EITHER policy role declares that it consumes a
     * bound: the allocation side by `using accepts_upper_bound = void;`
     * (`BoundedSequentialAllocationPolicy`), or the recycling side by exposing
     * `configure_domain` (`SparseRecyclingPolicy`). One public operation with
     * role-directed forwarding; if both roles opt in, both receive the same
     * effective value.
     *
     * Without this the allocation-side bound was unreachable through
     * IdGenerator: the general constructor forwarded `base_id` alone, so a
     * bounded generator silently got the policy's default bound of
     * `numeric_limits::max()` and was not bounded at all.
     *
     * The allocation-side opt-in is a declared alias rather than "forward a
     * second argument whenever the policy accepts one" -- see
     * detail::accepts_upper_bound for why inference was the wrong test even
     * after the impostor that motivated it was retired.
     *
     * @param base_id The first ID to generate
     * @param upper_bound The last ID that may be issued (inclusive), normalized
     *        against the ID type's reserved sentinel
     * @note `base_id` above the effective upper bound is not a precondition:
     *       for a full-domain policy it yields an empty (exhausted) domain,
     *       matching what the non-sparse path reports in the same situation.
     */
    template <typename AP = AllocationPolicy, typename RP = RecyclingPolicy>
        requires(detail::accepts_upper_bound_v<AP> || detail::has_domain_configure_v<RP>)
    IdGenerator(underlying_type base_id, underlying_type upper_bound)
        : IdGenerator(alloc_tag{}, base_id, effective_upper_bound(upper_bound))
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
        : AllocationPolicy(seed_tag_t{}, seed)
        , RecyclingPolicy()
        , ConcurrencyPolicy()
        , mBaseId(0)
        , mUpperBound(kDefaultUpperBound)
        , mIdsInUse()
    {
        configure_recycling_domain();
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

        // A full-domain policy holds every identifier, issued or not, so the
        // generation path is a staged take from the free set and the allocation
        // policy is never consulted -- see make_active() for why the order is
        // peek / reserve / insert / remove rather than the pop below.
        if constexpr (kFullDomain)
        {
            auto lowest = RecyclingPolicy::peek_lowest();
            if (!lowest)
            {
                // Exhaustion IS an empty free set. The allocation policy cannot
                // answer this question: its own exhaustion point is the ID
                // type's maximum, which may be far above the configured ceiling.
                return ErrorPolicy::report_error(IdError::Overflow);
            }

            // Construct the caller's value BEFORE anything is mutated. A
            // throwing id_type constructor would otherwise leave the identifier
            // active and out of the caller's hands -- unreleasable, because the
            // caller never received the value to release.
            const underlying_type raw_id = *lowest;
            result_type issued = report_id(raw_id);
            make_active(raw_id);
            RecyclingPolicy::remove_lowest();
            return issued;
        }
        else
        {

        // Try recycled IDs first (guaranteed unique, no retry needed)
        if (auto recycled = RecyclingPolicy::get_recycled())
        {
            underlying_type raw_id = *recycled;

#ifndef NDEBUG
            // Debug assertion: recycled ID should not be in active set
            assert(mIdsInUse.count(raw_id) == 0 && "Recycled ID already in use");
#endif

            // Construct the caller's value BEFORE the active-set insert. A
            // throwing id_type constructor would otherwise leave the identifier
            // active and out of the caller's hands, unreleasable because they
            // never received it. The recycled value has already been popped, so
            // it must go back if the construction fails -- otherwise it exists
            // in neither set.
            result_type issued = [&]
            {
                if constexpr (std::is_same_v<IdType_, underlying_type>)
                {
                    return ErrorPolicy::report_success(raw_id);
                }
                else
                {
                    try
                    {
                        return ErrorPolicy::report_success(IdType_(raw_id));
                    }
                    catch (...)
                    {
                        RecyclingPolicy::add_recycled(raw_id);
                        throw;
                    }
                }
            }();

            (void)mIdsInUse.insert(raw_id);
            return issued;
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

            // Same ordering as the recycled path: construct the caller's value
            // before publishing it, so a throwing id_type constructor cannot
            // strand an active identifier. Here the allocation policy has also
            // already advanced, so its counter is rewound too -- otherwise the
            // throw leaves a permanent gap in the sequence.
            if constexpr (std::is_same_v<IdType_, underlying_type>)
            {
                // Single-lookup: insert returns false if already present (collision)
                if (mIdsInUse.insert(raw_id))
                {
                    return ErrorPolicy::report_success(raw_id);
                }
            }
            else
            {
                result_type issued = [&]
                {
                    try
                    {
                        return ErrorPolicy::report_success(IdType_(raw_id));
                    }
                    catch (...)
                    {
                        if constexpr (detail::has_revert_v<AllocationPolicy>)
                        {
                            AllocationPolicy::revert(1);
                        }
                        throw;
                    }
                }();

                if (mIdsInUse.insert(raw_id))
                {
                    return issued;
                }
            }
            // Collision - retry with next ID from policy (only for random policies)
        }

        return ErrorPolicy::report_error(IdError::AlreadyInUse);

        } // if constexpr (!kFullDomain)
    }

    /**
     * @brief Take ownership of a specific identifier by value.
     *
     * @details Available only when the recycling policy owns the whole domain.
     * The point of the full-domain policy: a caller can claim 60000 out of an
     * empty generator without first consuming 0..59999, and the generator still
     * knows 60000 is spoken for.
     *
     * Every allocating step precedes every mutating step, so a throw leaves the
     * generator exactly as it was found:
     *
     *  1. An active identifier is refused with `AlreadyInUse`.
     *  2. An identifier outside `[base, upper_bound]` is refused with
     *     `InvalidClaim`, BEFORE any lookup -- the below-base, above-ceiling and
     *     reserved-sentinel test, which is a domain test, not a free-set test.
     *  3. An identifier that is in-domain but not free, having already passed
     *     step 1, means the policy state contradicts the active set:
     *     `InvalidClaim`.
     *  4. Reserve the return credit, plus one more if the claim splits an
     *     interval. All allocation happens here, with nothing yet mutated.
     *  5. Insert into the active set -- may throw; the free set is still intact.
     *  6. Remove from the free set: erase, trim, or split. Cannot throw.
     *
     * @param id The identifier to claim
     * @return Success, or AlreadyInUse / InvalidClaim
     */
    template <typename RP = RecyclingPolicy>
        requires detail::is_full_domain_policy_v<RP>
    Expected<void, IdError> claim(IdType_ id)
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();

        const underlying_type raw_id = to_underlying(id);

        if (mIdsInUse.count(raw_id) > 0)
        {
            return make_unexpected(IdError::AlreadyInUse);
        }

        // Deliberately redundant while the invariants hold, and kept anyway.
        // The free set is a subset of [base, upper_bound], so is_free() already
        // rejects everything out of domain with the same error -- deleting this
        // check passes the whole suite, and no test can distinguish the two.
        // It stays because it is O(1) ahead of an O(log I) lookup, it says which
        // question is being asked, and it is the one of the two that remains
        // correct if the free set ever violates invariant 1.
        if (!RecyclingPolicy::is_in_domain(raw_id))
        {
            return make_unexpected(IdError::InvalidClaim);
        }

        if (!RecyclingPolicy::is_free(raw_id))
        {
            return make_unexpected(IdError::InvalidClaim);
        }

        {
            CreditGuard guard(*this, RecyclingPolicy::reserve_claim_credits(raw_id));
            (void)mIdsInUse.insert(raw_id);
            guard.commit();
        }
        RecyclingPolicy::claim_at(raw_id);

        return {};
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

        // Refuse a count the ID space cannot satisfy, BEFORE reserving for it.
        // Without this, result.reserve(count) throws length_error or bad_alloc
        // out of an Expected-returning API for a count that was never
        // satisfiable. This is a domain check, not an allocation guard: a
        // feasible count can still fail to allocate, and that exception still
        // propagates, because IdError describes identifier-domain outcomes and
        // running out of memory is not one of them.
        //
        // The test is on the CONFIGURED DOMAIN's width, not the ID type's. Those
        // came apart the moment the domain became narrower than its type: a
        // `sizeof(underlying_type) < sizeof(size_t)` gate compiles the guard out
        // for every 64-bit generator, including a sparse or bounded one whose
        // domain holds ten identifiers, so `generate_batch(SIZE_MAX)` reached
        // `reserve()` and threw the exact exception this exists to prevent. That
        // is precisely the shape of the first consumer -- size_t-wide indices
        // with a depth-derived ceiling.
        {
            if (mUpperBound < mBaseId)
            {
                // Precondition-violating domain: empty, so nothing is satisfiable.
                return make_unexpected(IdError::Overflow);
            }

            const auto span = static_cast<std::uint64_t>(mUpperBound) -
                              static_cast<std::uint64_t>(mBaseId);
            constexpr auto kSizeMax =
                static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
            const std::size_t taken = mIdsInUse.size();

            if (span < kSizeMax)
            {
                // Capacity is representable, so the check is exact: total slots
                // minus those already taken.
                const std::size_t capacity = static_cast<std::size_t>(span) + 1u;
                if (count > (capacity > taken ? capacity - taken : std::size_t{0}))
                {
                    return make_unexpected(IdError::Overflow);
                }
            }
            else if (taken > 0)
            {
                // The full-width domain: `span + 1` is unrepresentable, which is
                // why the branch above excludes it -- and that exclusion was
                // itself a hole, because AVAILABILITY becomes representable as
                // soon as one identifier is active. With `taken` taken, exactly
                // `2^N - taken` remain, i.e. `SIZE_MAX - taken + 1`. Leaving
                // this out let the original defect survive at the one boundary
                // the new arithmetic did not cover.
                const std::size_t available = kSizeMax - taken + 1u;
                if (count > available)
                {
                    return make_unexpected(IdError::Overflow);
                }
            }
            // taken == 0 on a full-width domain: every representable count fits.
        }

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
        // pre-call state once the accumulated batch is rolled back.
        //
        // The nothrow move is REQUIRED, not assumed. A throwing move
        // constructor in the push_back would leave `raw_id` in the active set
        // but out of `result`, where rollback cannot see it -- simultaneously
        // active and free, with no credit backing it. Documenting that
        // assumption in this very comment, rather than enforcing it, is the
        // pattern this class has been corrected for elsewhere.
        static_assert(std::is_nothrow_move_constructible_v<id_type>,
                      "generate_batch() requires a nothrow-move id_type: the batch records an "
                      "identifier in the active set before moving it into the result, so a "
                      "throwing move strands it in neither structure and rollback cannot see it.");

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
            if constexpr (kFullDomain)
            {
                // Every element comes from the free set, so every element is
                // recorded as pooled and rollback returns all of them there. The
                // allocation policy is not consulted and is not reverted.
                for (size_t i = 0; i < count; ++i)
                {
                    auto lowest = RecyclingPolicy::peek_lowest();
                    if (!lowest)
                    {
                        rollback_batch(result, from_pool);
                        return make_unexpected(IdError::Overflow);
                    }

                    const underlying_type raw_id = *lowest;
                    RecyclingPolicy::reserve_credit();
                    {
                        CreditGuard guard(*this, 1);
                        commit(raw_id, true);
                        guard.commit();
                    }
                    RecyclingPolicy::remove_lowest();
                }

                return result;
            }
            else
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

            } // if constexpr (!kFullDomain)
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

        // Revert the allocation policy counter to prevent sequence gaps.
        // Only policies that support revert() will have this called, and never
        // on the full-domain path, where the allocation policy issued nothing
        // and its counter never advanced.
        if constexpr (detail::has_revert_v<AllocationPolicy> && !kFullDomain)
        {
            AllocationPolicy::revert(newly_generated_count);
        }
        else
        {
            (void)newly_generated_count;
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

    /**
     * @brief Number of disjoint free intervals held by a full-domain policy.
     *
     * @details The oracle for canonicalization. "Intervals are disjoint and
     * non-adjacent" is an invariant, not a performance note, and a release that
     * leaves two adjacent intervals where one belongs is invisible to every
     * other query -- membership, counts and issuance order all still look right.
     * This is what makes that failure observable.
     */
    template <typename RP = RecyclingPolicy>
        requires detail::is_full_domain_policy_v<RP>
    std::size_t free_interval_count() const noexcept
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock_shared();
        return RecyclingPolicy::interval_count();
    }

    /**
     * @brief Reserved return credits, which must equal `active_count()` here.
     *
     * @details The credits == actives relation is what makes `release()` nothrow
     * by construction and what `reset()`'s exhausted case depends on. It holds at
     * every operation boundary -- and every boundary is exactly where a caller
     * can observe it.
     */
    template <typename RP = RecyclingPolicy>
        requires detail::is_full_domain_policy_v<RP>
    std::size_t reserved_credit_count() const noexcept
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock_shared();
        return RecyclingPolicy::credit_count();
    }

    /**
     * @brief Clear every active identifier and restore the configured domain.
     *
     * @details A policy that owns its domain is reached through
     * `configure_domain`, NOT through `clear()`: `clear()` alone leaves the
     * interval map empty, an empty map means exhausted, so a literal
     * `clear()`-only reset would silently degrade the generator to plain
     * sequential allocation while reporting nothing.
     *
     * The rebuild runs BEFORE the active set is cleared, because an exhausted
     * full-domain policy funds it from a return credit and the credits belong to
     * the identifiers still active at that point.
     *
     * Conditionally `noexcept`: the rebuild reuses an owned node in every state
     * reachable WITHOUT a move, and allocates only for a moved-from generator,
     * which a movable instantiation can reach (see the class note on move
     * operations). The IdGuard objection that keeps `release()` unconditionally
     * `noexcept` does not apply -- no destructor and no guard calls `reset()`.
     * The three shipping recycling policies keep the unconditional `noexcept`.
     */
    void reset() noexcept(detail::domain_configure_is_noexcept_v<RecyclingPolicy>)
    {
        [[maybe_unused]] auto lock = ConcurrencyPolicy::lock();
        if constexpr (detail::has_domain_configure_v<RecyclingPolicy>)
        {
            configure_recycling_domain();
        }
        else
        {
            RecyclingPolicy::clear();
        }
        mIdsInUse.clear();
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
        friend IdGenerator;

        /**
         * @brief Adopt @p id, which the generator has just issued to this guard.
         *
         * @details Private, and reachable only through `scoped_id()` /
         * `scoped_claim()`. As a public constructor it let a caller mint a guard
         * over an identifier they never acquired, and the destructor then
         * released it out from under its real owner:
         *
         * @code
         *   auto owner = gen.generate();          // owner holds 1
         *   { IdGuard foreign(gen, *owner); }     // ~IdGuard releases 1
         *   auto other = gen.generate();          // 1 is issued a SECOND time
         * @endcode
         *
         * The asserts below do not catch that shape, in any build: the release
         * SUCCEEDS, so `released` is true. They catch only the never-active and
         * double-release shapes, which are the harmless half. A guard must
         * therefore be unable to name an identifier it was not given.
         *
         * @warning Movability is checked here rather than only documented -- see
         * the static_assert.
         */
        IdGuard(IdGenerator& gen, IdType_ id) noexcept
            : mGenerator(&gen)
            , mId(id)
            , mValid(true)
            , mEpoch(gen.mEpoch)
        {
            // The epoch covers reset() and ONLY reset(). A generator MOVE copies
            // mEpoch into the destination and leaves the source's value alone,
            // while this guard holds a raw pointer to the source -- so the
            // staleness check cannot see the move, and the identifier is
            // stranded active in the destination: unreleasable and unissuable.
            // Over UniqueRWLockPolicy it is worse, because the source's mutex
            // pointer is null and the release dereferences it.
            //
            // The manual already claimed this was a compile error. This makes
            // that true of the template rather than of four of its
            // instantiations. Guards and a movable generator are not a
            // combination that works today; this refuses it instead of
            // documenting it.
            static_assert(!std::is_move_constructible_v<IdGenerator> &&
                              !std::is_move_assignable_v<IdGenerator>,
                          "IdGuard requires an immovable IdGenerator: a guard holds a raw pointer "
                          "to its generator and its epoch check cannot detect a move, so moving "
                          "the generator strands the guarded identifier. Every shipped alias is "
                          "immovable; if you need reader/writer locking with guards, use "
                          "SharedMutexPolicy, which is immovable and has a real shared lock.");
        }

    public:
        /// @brief An empty guard, owning nothing. Safe to construct directly:
        /// it cannot name an identifier, so it cannot fabricate ownership.
        IdGuard() noexcept
            : mGenerator(nullptr)
            , mId{}
            , mValid(false)
            , mEpoch(0)
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

    /// @brief Generate an identifier and get a guard that releases it.
    ///
    /// @details Constrained rather than merely asserted: with the check living
    /// only inside IdGuard's constructor, this factory still SATISFIED a
    /// `requires` expression and failed hard on instantiation. A caller could
    /// not detect that guards were unavailable for their instantiation without
    /// triggering the error. The constraint makes the absence a property.
    template <typename Self = IdGenerator>
        requires(!std::is_move_constructible_v<Self> && !std::is_move_assignable_v<Self>)
    [[nodiscard]] Expected<IdGuard, IdError> scoped_id()
    {
        auto result = generate();
        if (!result.has_value())
        {
            return make_unexpected(result.error());
        }
        return IdGuard(*this, *result);
    }

    /**
     * @brief Claim a specific identifier and get a guard that releases it.
     *
     * @details The `scoped_id()` sibling for the claim path. It exists because
     * `IdGuard`'s adopting constructor is private: the legitimate use of that
     * constructor was "I acquired this identifier, now guard it", and this is
     * that use, expressed so the guard cannot name an identifier the generator
     * did not just hand over.
     */
    template <typename RP = RecyclingPolicy, typename Self = IdGenerator>
        requires(detail::is_full_domain_policy_v<RP> && !std::is_move_constructible_v<Self> &&
                 !std::is_move_assignable_v<Self>)
    [[nodiscard]] Expected<IdGuard, IdError> scoped_claim(IdType_ id)
    {
        auto result = claim(id);
        if (!result.has_value())
        {
            return make_unexpected(result.error());
        }
        return IdGuard(*this, id);
    }

private:
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

    /// @brief Highest issuable identifier, already normalized against the ID
    /// type's reserved sentinel. Retained for reset() and the claim domain test.
    underlying_type mUpperBound;

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

/// @brief Generator supporting claim-by-value over the whole domain.
///
/// @details The only alias whose recycling policy owns every identifier rather
/// than only the released ones, so it is the only one offering `claim(id)`. Use
/// it when identifiers arrive from outside -- a load, a peer, a file -- and must
/// be reserved without first consuming everything below them.
///
/// Construct with `SparseIdGenerator<T> gen(base, upper_bound)` to narrow the
/// domain to what the consumer can actually represent; `gen(base)` takes the ID
/// type's full domain, less its reserved sentinel if it declares one.
///
/// No existing alias selects this policy: SimpleIdGenerator, ThreadSafeIdGenerator,
/// DenseIdGenerator and RandomIdGenerator receive no claim member and no
/// ordering change.
template <typename IdType = uint64_t>
using SparseIdGenerator = IdGenerator<IdType,
                                      SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                                      SparseRecyclingPolicy<underlying_id_type_t<IdType>>,
                                      id_generator::ExpectedErrorPolicy<IdType, IdError>,
                                      SingleThreadedPolicy>;

/// @brief Thread-safe claim-by-value generator.
template <typename IdType = uint64_t>
using ThreadSafeSparseIdGenerator =
    IdGenerator<IdType,
                SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
                SparseRecyclingPolicy<underlying_id_type_t<IdType>>,
                id_generator::ExpectedErrorPolicy<IdType, IdError>,
                MutexSynchronizationPolicy>;

/// @brief Random ID generator (no recycling, with retry for collisions)
template <typename IdType = uint64_t>
using RandomIdGenerator = IdGenerator<IdType,
                                      RandomAllocationPolicy<underlying_id_type_t<IdType>>,
                                      NoRecyclingPolicy<underlying_id_type_t<IdType>>,
                                      id_generator::ExpectedErrorPolicy<IdType, IdError>,
                                      SingleThreadedPolicy>;

} // namespace fat_p
