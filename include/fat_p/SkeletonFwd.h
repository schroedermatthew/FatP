#pragma once
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: public_header
  path: include/fat_p/SkeletonFwd.h
  namespace: fat_p::skeleton
  layer: Domain
  summary: >
    Forward declarations and runtime types for Skeleton: BoneId, HierarchySchema,
    SkeletonCapability, SkeletonMask.
  api_stability: in_work
  related:
    docs_search: "Skeleton"
    tests:
      - components/Skeleton/tests/test_Skeleton.cpp
    benchmarks_search: "Skeleton"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file SkeletonFwd.h
 * @brief Forward declarations and runtime identity types for the Skeleton component.
 *
 * @details Include this header to share HierarchySchema definitions, BoneId values,
 * and capability masks across translation units without pulling in the full
 * Bone<>, SkeletonItem, BasicBoneItem<>, or Skeleton template machinery.
 *
 * Defines:
 * - BoneId          -- 64-bit packed hierarchical address (8 levels x 8 bits)
 * - HierarchySchema -- compile-time binding of depth positions to enum types
 * - SkeletonCapability / SkeletonMask / makeMask()
 * - Forward declarations of Skeleton and SkeletonItem
 *
 * Requirements:
 * - C++20
 */

#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>

#include "enforce.h"

namespace fat_p::skeleton
{

// =============================================================================
// Forward declarations
// =============================================================================

class Skeleton;
class SkeletonItem;
class BoneId;

// =============================================================================
// BoneId -- concrete fixed-width hierarchical address
// =============================================================================

namespace detail
{

// Forward-declare buildBoneId so BoneIdTag can grant it friendship.
// The definition appears later in this file.
template <auto... Levels>
[[nodiscard]] constexpr BoneId buildBoneId() noexcept;

/**
 * @brief Private construction tag. Restricts BoneId construction to authorised
 * factory sites.
 *
 * BoneIdTag has a private default constructor. Only the types listed as friends
 * can construct a tag value and thus reach the tagged BoneId(tag, value, depth)
 * constructor. External code cannot default-construct BoneIdTag, so forged
 * non-canonical BoneIds are impossible from outside this file. This gate is
 * load-bearing: Skeleton::publish() duplicate detection relies on all BoneIds
 * being in canonical form (inactive bytes zero).
 */
struct BoneIdTag
{
private:
    friend class fat_p::skeleton::BoneId;

    template <auto... Levels>
    friend constexpr BoneId buildBoneId() noexcept;

    constexpr BoneIdTag() noexcept = default;
};

} // namespace detail

/**
 * @brief Concrete 64-bit hierarchical address: 8 levels x 8 bits per level.
 *
 * Layout: level 0 occupies bits [63:56], level 1 [55:48], ..., level 7 [7:0].
 * Unused levels are always zero (canonical form). Depth tracks how many levels
 * are active.
 *
 * Limits: 8 levels maximum, 256 values per level (0..255).
 * The 9-byte serialized form (8 value + 1 depth) is suitable for network routing.
 *
 * @par Construction
 * BoneId uses a private-tag constructor to prevent raw BoneId{value, depth}
 * construction outside of the authorised factory sites (child(), parent(),
 * deserialize(), detail::buildBoneId()). All factory sites produce canonical
 * form (inactive bytes zero), which is required for correct equality, hashing,
 * and duplicate detection in Skeleton::publish().
 *
 * @par Invariants
 * - A default-constructed BoneId is null (depth == 0, value == 0).
 * - Inactive level bytes (index >= depth) are always zero.
 * - parent() is UB when depth == 0. Debug builds assert.
 * - child(index) is UB when depth >= 8. Debug builds assert.
 *
 * @note Thread-safety: NOT thread-safe. BoneId is a value type; concurrent
 * access to distinct instances is safe without synchronization.
 */
class BoneId
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /// Default-constructs the null BoneId (depth == 0, value == 0).
    constexpr BoneId() noexcept
        : mValue(0u)
        , mDepth(0u)
    {
    }

    /// @brief Tagged constructor for use by authorised factory sites only.
    ///
    /// The tag parameter (detail::BoneIdTag) is not publicly constructible,
    /// so this constructor is effectively private to code holding a BoneIdTag.
    /// All callers must guarantee inactive bytes are zero (canonical form).
    constexpr BoneId(detail::BoneIdTag, uint64_t v, uint8_t d) noexcept
        : mValue(v), mDepth(d)
    {}

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// Returns the raw 64-bit packed path value. Inactive bytes are zero.
    [[nodiscard]] constexpr uint64_t value() const noexcept
    {
        return mValue;
    }

    /// Returns the number of active depth levels. Range [0, 8]. Zero means null.
    [[nodiscard]] constexpr uint8_t depth() const noexcept
    {
        return mDepth;
    }

    // -------------------------------------------------------------------------
    // Comparison
    // -------------------------------------------------------------------------

    [[nodiscard]] constexpr bool operator==(const BoneId&) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const BoneId&) const noexcept = default;

    // -------------------------------------------------------------------------
    // Null check
    // -------------------------------------------------------------------------

    /// Returns true if this BoneId has no active levels.
    [[nodiscard]] constexpr bool isNull() const noexcept
    {
        return mDepth == 0u;
    }

    // -------------------------------------------------------------------------
    // Hierarchy queries
    // -------------------------------------------------------------------------

    /**
     * @brief Returns true if this BoneId is a proper ancestor of @p other.
     *
     * "Ancestor" means this path is a strict prefix of @p other's path.
     * A BoneId is NOT an ancestor of itself.
     *
     * Example: [1] isAncestorOf [1/2] -> true.
     *          [1] isAncestorOf [1]   -> false (same depth, not strict).
     *
     * @param other The BoneId to test as a potential descendant.
     * @return true if this is a strict prefix of @p other's path.
     * @note Complexity: O(1).
     */
    [[nodiscard]] constexpr bool isAncestorOf(BoneId other) const noexcept
    {
        if (mDepth >= other.mDepth || mDepth == 0u)
        {
            return false;
        }
        const uint64_t shiftBits = static_cast<uint64_t>(64u - 8u * mDepth);
        const uint64_t mask = (~uint64_t{0}) << shiftBits;
        return (mValue & mask) == (other.mValue & mask);
    }

    /**
     * @brief Returns true if this BoneId is a proper descendant of @p ancestor.
     *
     * @param ancestor The BoneId to test as a potential ancestor.
     * @return true if @p ancestor is a strict prefix of this path.
     * @note Complexity: O(1).
     */
    [[nodiscard]] constexpr bool isDescendantOf(BoneId ancestor) const noexcept
    {
        return ancestor.isAncestorOf(*this);
    }

    // -------------------------------------------------------------------------
    // Navigation
    // -------------------------------------------------------------------------

    /**
     * @brief Returns the parent BoneId (one fewer active level).
     *
     * @pre depth > 0. UB in release if violated; asserts in debug.
     * @return A BoneId with one fewer active level.
     * @note Complexity: O(1).
     */
    [[nodiscard]] constexpr BoneId parent() const noexcept
    {
        FATP_ENFORCE(mDepth > 0u, "BoneId::parent() called on null BoneId");
        const uint8_t newDepth = static_cast<uint8_t>(mDepth - 1u);
        const uint64_t shift = static_cast<uint64_t>(8u * (8u - mDepth));
        const uint64_t clearMask = ~(uint64_t{0xFF} << shift);
        return BoneId{detail::BoneIdTag{}, mValue & clearMask, newDepth};
    }

    /**
     * @brief Returns a child BoneId with @p index appended at the next level.
     *
     * @pre depth < 8. UB in release if violated; asserts in debug.
     * @param index The 0-255 value to place at the new depth level.
     * @return A BoneId with one additional active level.
     * @note Complexity: O(1).
     */
    [[nodiscard]] constexpr BoneId child(uint8_t index) const noexcept
    {
        FATP_ENFORCE(mDepth < 8u, "BoneId::child() called on a BoneId already at maximum depth (8)");
        const uint64_t shift = static_cast<uint64_t>(8u * (7u - mDepth));
        const uint64_t newValue = mValue | (static_cast<uint64_t>(index) << shift);
        return BoneId{detail::BoneIdTag{}, newValue, static_cast<uint8_t>(mDepth + 1u)};
    }

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------

    /**
     * @brief Returns a human-readable string representation, e.g. "[1/2/0]".
     *
     * Intended for dump() output and diagnostics. Not for production paths.
     * @return A bracketed slash-separated string of level values, or "[null]".
     * @note Complexity: O(depth), bounded by 8.
     * @note Thread-safety: NOT thread-safe. BoneId is a value type; safe across distinct instances.
     */
    [[nodiscard]] std::string toString() const
    {
        if (mDepth == 0u)
        {
            return "[null]";
        }
        std::string result = "[";
        for (uint8_t i = 0u; i < mDepth; ++i)
        {
            if (i > 0u)
            {
                result += '/';
            }
            const uint64_t shift = static_cast<uint64_t>(8u * (7u - i));
            const uint8_t level = static_cast<uint8_t>((mValue >> shift) & 0xFFu);
            result += std::to_string(level);
        }
        result += ']';
        return result;
    }

    // -------------------------------------------------------------------------
    // Serialization -- 9 bytes (8 value + 1 depth), big-endian value
    // -------------------------------------------------------------------------

    /**
     * @brief Serializes into @p out in a stable 9-byte big-endian form.
     *
     * @param out Output buffer of exactly 9 bytes.
     * @note Complexity: O(1).
     * @note Thread-safety: NOT thread-safe. BoneId is a value type; safe across distinct instances.
     */
    void serialize(std::span<std::byte, 9> out) const noexcept
    {
        for (std::size_t i = 0; i < 8u; ++i)
        {
            out[i] = static_cast<std::byte>((mValue >> (56u - 8u * i)) & 0xFFu);
        }
        out[8] = static_cast<std::byte>(mDepth);
    }

    /**
     * @brief Deserializes from a 9-byte big-endian buffer.
     *
     * The returned BoneId is always in canonical form: bytes below the active
     * depth are masked to zero, ensuring equality and hash consistency with
     * all other BoneIds representing the same path. A depth byte > 8 is treated
     * as a null BoneId.
     *
     * @param in Input buffer of exactly 9 bytes.
     * @return The canonical BoneId encoded in the buffer.
     * @note Complexity: O(1).
     * @note Thread-safety: NOT thread-safe. BoneId is a value type; safe across distinct instances.
     */
    [[nodiscard]] static constexpr BoneId deserialize(std::span<const std::byte, 9> in) noexcept
    {
        uint64_t v = 0u;
        for (std::size_t i = 0; i < 8u; ++i)
        {
            v = (v << 8u) | static_cast<uint64_t>(std::to_integer<uint8_t>(in[i]));
        }
        const uint8_t d = std::to_integer<uint8_t>(in[8]);
        if (d > 8u)
        {
            // Depth byte out of range: treat as null BoneId (corrupted buffer).
            // Documented contract: a depth byte > 8 is silently returned as null.
            return BoneId{};
        }
        if (d == 0u)
        {
            v = 0u;
        }
        else if (d < 8u)
        {
            const uint64_t shiftBits = static_cast<uint64_t>(8u * (8u - d));
            const uint64_t mask = (~uint64_t{0}) << shiftBits;
            v &= mask;
        }
        return BoneId{detail::BoneIdTag{}, v, d};
    }

private:
    uint64_t mValue; ///< Packed path. Level N in bits [63-8N : 56-8N]. Inactive bytes zero.
    uint8_t  mDepth; ///< Number of active levels [0, 8].
};

} // namespace fat_p::skeleton

// =============================================================================
// std::hash specialization -- must live in namespace std
// =============================================================================

template <>
struct std::hash<fat_p::skeleton::BoneId>
{
    [[nodiscard]] std::size_t operator()(const fat_p::skeleton::BoneId& id) const noexcept
    {
        // Mix depth into a separate lane before the finalisation avalanche.
        // Depth occupies bits [63:56] in level-0's byte, so XOR-shifting it
        // there produces structured collisions (e.g. [2] vs [1/0]).
        // Multiplying by a large odd constant disperses depth bits across the
        // full 64-bit width before combining with the path value.
        uint64_t h = id.value();
        h ^= static_cast<uint64_t>(id.depth()) * 0x9e3779b97f4a7c15ull;
        h ^= h >> 30u;
        h *= 0xbf58476d1ce4e5b9ull;
        h ^= h >> 27u;
        h *= 0x94d049bb133111ebull;
        h ^= h >> 31u;
        return static_cast<std::size_t>(h);
    }
};

namespace fat_p::skeleton
{

// =============================================================================
// detail::buildBoneId -- compile-time BoneId factory used by Bone<>
// =============================================================================

namespace detail
{

/**
 * @brief Constructs a canonical BoneId from a compile-time sequence of enum values.
 *
 * Each enum value is cast to its underlying type and packed into the appropriate
 * 8-bit slot. All inactive bytes are zero by construction, satisfying the BoneId
 * canonical-form invariant. Used exclusively by Bone<>::id().
 */
template <auto... Levels>
[[nodiscard]] constexpr BoneId buildBoneId() noexcept
{
    constexpr std::size_t kDepth = sizeof...(Levels);
    static_assert(kDepth <= 8u, "Bone depth exceeds 8");

    uint64_t v = 0u;
    std::size_t i = 0u;
    (
        [&]
        {
            const auto raw =
                static_cast<uint64_t>(static_cast<std::underlying_type_t<decltype(Levels)>>(Levels));
            const uint64_t shift = static_cast<uint64_t>(8u * (7u - i));
            v |= (raw & 0xFFu) << shift;
            ++i;
        }(),
        ...
    );

    return BoneId{BoneIdTag{}, v, static_cast<uint8_t>(kDepth)};
}

} // namespace detail

// =============================================================================
// HierarchySchema -- compile-time binding of depth positions to enum types
// =============================================================================

/**
 * @brief Binds each depth position to its expected enum type.
 *
 * All level types must be enums. The schema depth must not exceed 8, the BoneId
 * physical limit. Schemas are typically defined once per application domain and
 * shared across all Bone instantiations for that domain.
 *
 * Example:
 * @code
 * using SysSchema = HierarchySchema<System, Subsystem, Channel, Node>;
 * // SysSchema::expected_type<0> == System
 * // SysSchema::expected_type<2> == Channel
 * // SysSchema::kMaxDepth        == 4
 * @endcode
 *
 * @tparam LevelTypes... Enum types, one per depth level. All must be enum types.
 */
template <typename... LevelTypes>
struct HierarchySchema
{
    static_assert((std::is_enum_v<LevelTypes> && ...), "All HierarchySchema level types must be enums");
    static_assert(sizeof...(LevelTypes) <= 8u,
                  "HierarchySchema kMaxDepth cannot exceed 8 (BoneId physical limit)");

    /// The enum type expected at depth position @p Depth.
    template <std::size_t Depth>
    using expected_type = std::tuple_element_t<Depth, std::tuple<LevelTypes...>>;

    /// Total number of depth levels this schema defines.
    static constexpr std::size_t kMaxDepth = sizeof...(LevelTypes);
};

// =============================================================================
// SkeletonCapability and SkeletonMask
// =============================================================================

/**
 * @brief Capability bits that describe what an item is and what it can do.
 *
 * Bits 0-7:   Category (what kind of thing this is)
 * Bits 8-15:  Providers (what this item provides)
 * Bits 16-23: Consumers (what this item consumes)
 * Bits 24-31: Properties (access and visibility flags)
 *
 * @note Count is a sentinel for the bitset size. Do not pass it to makeMask().
 */
enum class SkeletonCapability : uint32_t
{
    // --- Category (0-7) ---
    Sensor        = 0,
    Controller    = 1,
    Display       = 2,
    Network       = 3,
    Storage       = 4,

    // --- Providers (8-15) ---
    ProvidesValue   = 8,
    ProvidesCommand = 9,
    ProvidesStatus  = 10,

    // --- Consumers (16-23) ---
    ConsumesValue   = 16,
    ConsumesCommand = 17,
    ConsumesStatus  = 18,

    // --- Properties (24-31) ---
    Readable       = 24,
    Writable       = 25,
    Serializable   = 26,
    NetworkVisible = 27,

    Count = 32 ///< Sentinel: total number of capability bits. Not a valid capability.
};

/**
 * @brief A 32-bit capability bitset.
 *
 * Use makeMask() to construct a mask from SkeletonCapability values.
 */
using SkeletonMask = std::bitset<static_cast<std::size_t>(SkeletonCapability::Count)>;

/**
 * @brief Constructs a SkeletonMask with the given capability bits set.
 *
 * @pre Each value in @p caps must be a defined SkeletonCapability other than Count.
 *      Passing Count (value 32) violates this contract and terminates the process in
 *      all build configurations via FATP_ALWAYS_ENFORCE.
 *
 * @code
 * auto m = makeMask(SkeletonCapability::Sensor, SkeletonCapability::ProvidesValue);
 * @endcode
 *
 * @tparam Caps... Types convertible to SkeletonCapability.
 * @return A SkeletonMask with the corresponding bits set.
 */
template <std::convertible_to<SkeletonCapability>... Caps>
[[nodiscard]] SkeletonMask makeMask(Caps... caps) noexcept
{
    SkeletonMask m;
    (
        [&]
        {
            const auto rawCap = static_cast<SkeletonCapability>(caps);
            const std::size_t idx = static_cast<std::size_t>(static_cast<uint32_t>(rawCap));
            FATP_ALWAYS_ENFORCE(idx < m.size(),
                "makeMask(): capability index out of range. Do not pass SkeletonCapability::Count.");
            m.set(idx);
        }(),
        ...
    );
    return m;
}

} // namespace fat_p::skeleton
