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
 * - BoneId          -- 256-bit packed hierarchical address (16 levels x 16 bits)
 * - HierarchySchema -- compile-time binding of depth positions to enum types
 * - SkeletonCapability / SkeletonMask / makeMask()
 * - Forward declarations of Skeleton and SkeletonItem
 *
 * Requirements:
 * - C++20
 */

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

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
 * can construct a tag value and thus reach the tagged BoneId(tag, words, depth)
 * constructor. External code cannot default-construct BoneIdTag, so forged
 * non-canonical BoneIds are impossible from outside this file. This gate is
 * load-bearing: Skeleton::publish() duplicate detection relies on all BoneIds
 * being in canonical form (inactive level slots zero).
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
 * @brief Concrete 256-bit hierarchical address: 16 levels x 16 bits per level.
 *
 * Layout: four 64-bit words hold four levels each, most-significant level
 * first. Level i occupies bits [63-16*(i%4) : 48-16*(i%4)] of word i/4, so
 * level 0 is the top 16 bits of word 0. Unused level slots are always zero
 * (canonical form). Depth tracks how many levels are active.
 *
 * The MSB-first packing makes the defaulted member-wise comparison equal to
 * lexicographic path comparison: a parent orders before its children, and
 * siblings order by level value. OwnerSkeleton's parent-before-child sorted
 * traversals depend on this property.
 *
 * Limits: 16 levels maximum, 65,536 values per level (0..65535). These are the
 * starting configuration (BE's ITEM_ID shipped with 16 levels); widening either
 * dimension is a versioned serialization change, not a breaking one, because
 * the serialized form stores only the active levels.
 *
 * @par Construction
 * BoneId uses a private-tag constructor to prevent raw construction outside of
 * the authorised factory sites (child(), parent(), deserialize(),
 * deserializeLegacy9(), detail::buildBoneId()). All factory sites produce
 * canonical form (inactive slots zero), which is required for correct equality,
 * hashing, and duplicate detection in Skeleton::publish().
 *
 * @par Invariants
 * - A default-constructed BoneId is null (depth == 0, all words == 0).
 * - Inactive level slots (index >= depth) are always zero.
 * - parent() is UB when depth == 0. Debug builds assert.
 * - child(index) is UB when depth >= 16. Debug builds assert.
 *
 * @note Thread-safety: NOT thread-safe. BoneId is a value type; concurrent
 * access to distinct instances is safe without synchronization.
 */
class BoneId
{
public:
    // -------------------------------------------------------------------------
    // Limits
    // -------------------------------------------------------------------------

    /// Maximum number of hierarchy levels.
    static constexpr std::size_t kMaxDepth = 16u;

    /// Number of 64-bit words backing the level slots (4 levels per word).
    static constexpr std::size_t kWordCount = kMaxDepth / 4u;

    /// Serialized size of the deepest possible BoneId: 1 depth byte + 2 bytes
    /// per active level. Shallower ids serialize smaller (see serialize()).
    static constexpr std::size_t kMaxSerializedBytes = 1u + 2u * kMaxDepth;

    /// Size of the legacy 8-level x 8-bit serialized form (8 value + 1 depth).
    static constexpr std::size_t kLegacySerializedBytes = 9u;

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /// Default-constructs the null BoneId (depth == 0, all words == 0).
    constexpr BoneId() noexcept
        : mWords{}
        , mDepth(0u)
    {
    }

    /// @brief Tagged constructor for use by authorised factory sites only.
    ///
    /// The tag parameter (detail::BoneIdTag) is not publicly constructible,
    /// so this constructor is effectively private to code holding a BoneIdTag.
    /// All callers must guarantee inactive slots are zero (canonical form).
    constexpr BoneId(detail::BoneIdTag,
                     const std::array<uint64_t, kWordCount>& words,
                     uint8_t d) noexcept
        : mWords(words), mDepth(d)
    {}

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @brief Returns the level value at depth position @p i.
    /// @pre i < depth(). UB in release if violated; asserts in debug.
    [[nodiscard]] constexpr uint16_t level(std::size_t i) const noexcept
    {
        FATP_ENFORCE(i < mDepth, "BoneId::level() index out of range");
        return levelUnchecked(i);
    }

    /// Returns the raw packed words (canonical form: inactive slots zero).
    /// Intended for hashing and diagnostics, not for level extraction — use
    /// level() for that.
    [[nodiscard]] constexpr const std::array<uint64_t, kWordCount>&
    words() const noexcept
    {
        return mWords;
    }

    /// Returns the number of active depth levels. Range [0, 16]. Zero means null.
    [[nodiscard]] constexpr uint8_t depth() const noexcept
    {
        return mDepth;
    }

    // -------------------------------------------------------------------------
    // Comparison
    // -------------------------------------------------------------------------

    // Member order (mWords, mDepth) plus MSB-first packing makes these
    // defaulted operators compare paths lexicographically with parents first:
    // sibling levels compare by value; a parent ties its child's word prefix
    // (inactive slots are zero) and resolves first on the shallower depth.

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
     * @note Complexity: O(1) (word-count bounded).
     */
    [[nodiscard]] constexpr bool isAncestorOf(BoneId other) const noexcept
    {
        if (mDepth >= other.mDepth || mDepth == 0u)
        {
            return false;
        }
        for (std::size_t w = 0u; w < kWordCount; ++w)
        {
            const uint64_t mask = wordMaskForDepth(w, mDepth);
            if ((mWords[w] & mask) != (other.mWords[w] & mask))
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns true if this BoneId is a proper descendant of @p ancestor.
     *
     * @param ancestor The BoneId to test as a potential ancestor.
     * @return true if @p ancestor is a strict prefix of this path.
     * @note Complexity: O(1) (word-count bounded).
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
        std::array<uint64_t, kWordCount> words = mWords;
        clearSlot(words, newDepth);
        return BoneId{detail::BoneIdTag{}, words, newDepth};
    }

    /**
     * @brief Returns a child BoneId with @p index appended at the next level.
     *
     * @pre depth < 16. UB in release if violated; asserts in debug.
     * @param index The 0-65535 value to place at the new depth level.
     * @return A BoneId with one additional active level.
     * @note Complexity: O(1).
     */
    [[nodiscard]] constexpr BoneId child(uint16_t index) const noexcept
    {
        FATP_ENFORCE(mDepth < kMaxDepth,
                     "BoneId::child() called on a BoneId already at maximum depth (16)");
        std::array<uint64_t, kWordCount> words = mWords;
        words[wordOf(mDepth)] |= static_cast<uint64_t>(index) << shiftOf(mDepth);
        return BoneId{detail::BoneIdTag{}, words, static_cast<uint8_t>(mDepth + 1u)};
    }

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------

    /**
     * @brief Returns a human-readable string representation, e.g. "[1/2/0]".
     *
     * Intended for dump() output and diagnostics. Not for production paths.
     * @return A bracketed slash-separated string of level values, or "[null]".
     * @note Complexity: O(depth), bounded by 16.
     * @note Thread-safety: NOT thread-safe. BoneId is a value type; safe across distinct instances.
     */
    [[nodiscard]] std::string toString() const
    {
        if (mDepth == 0u)
        {
            return "[null]";
        }
        std::string result = "[";
        for (std::size_t i = 0u; i < mDepth; ++i)
        {
            if (i > 0u)
            {
                result += '/';
            }
            result += std::to_string(levelUnchecked(i));
        }
        result += ']';
        return result;
    }

    // -------------------------------------------------------------------------
    // Serialization -- 1 depth byte + 2 bytes per active level, big-endian
    // -------------------------------------------------------------------------

    /**
     * @brief Serializes into @p out: depth byte, then each active level as a
     *        big-endian uint16. Shallow ids stay small on disk and on the wire.
     *
     * @param out Output buffer of kMaxSerializedBytes; only the returned count
     *            is written.
     * @return Number of bytes written: 1 + 2 * depth().
     * @note Complexity: O(depth), bounded by 16.
     * @note Thread-safety: NOT thread-safe. BoneId is a value type; safe across distinct instances.
     */
    std::size_t serialize(std::span<std::byte, kMaxSerializedBytes> out) const noexcept
    {
        out[0] = static_cast<std::byte>(mDepth);
        for (std::size_t i = 0u; i < mDepth; ++i)
        {
            const uint16_t v = levelUnchecked(i);
            out[1u + 2u * i] = static_cast<std::byte>((v >> 8u) & 0xFFu);
            out[2u + 2u * i] = static_cast<std::byte>(v & 0xFFu);
        }
        return 1u + 2u * static_cast<std::size_t>(mDepth);
    }

    /**
     * @brief Deserializes from a buffer written by serialize().
     *
     * The returned BoneId is always in canonical form by construction (levels
     * are placed slot by slot; untouched slots stay zero). A depth byte > 16 or
     * a buffer too short for the declared depth is treated as a null BoneId
     * (corrupted input).
     *
     * @param in Input buffer: at least 1 + 2 * depth-byte bytes.
     * @return The canonical BoneId encoded in the buffer, or null on malformed input.
     * @note Complexity: O(depth), bounded by 16.
     */
    [[nodiscard]] static constexpr BoneId deserialize(std::span<const std::byte> in) noexcept
    {
        if (in.size() < 1u)
        {
            return BoneId{};
        }
        const uint8_t d = std::to_integer<uint8_t>(in[0]);
        if (d > kMaxDepth || in.size() < 1u + 2u * static_cast<std::size_t>(d))
        {
            // Malformed buffer. Documented contract: silently returns null.
            return BoneId{};
        }
        std::array<uint64_t, kWordCount> words{};
        for (std::size_t i = 0u; i < d; ++i)
        {
            const uint16_t hi = std::to_integer<uint8_t>(in[1u + 2u * i]);
            const uint16_t lo = std::to_integer<uint8_t>(in[2u + 2u * i]);
            const auto v = static_cast<uint64_t>(static_cast<uint16_t>((hi << 8u) | lo));
            words[wordOf(i)] |= v << shiftOf(i);
        }
        return BoneId{detail::BoneIdTag{}, words, d};
    }

    /**
     * @brief Deserializes the legacy 9-byte form (8 x 8-bit levels + depth).
     *
     * Migration reader for data written before the 16x16 widening. Each legacy
     * 8-bit level value maps unchanged into a 16-bit slot, so a legacy id and
     * its re-serialized form address the same bone. A legacy depth byte > 8 is
     * treated as a null BoneId, matching the legacy contract.
     *
     * @param in Input buffer of exactly 9 bytes (legacy canonical form).
     * @return The canonical BoneId encoded in the buffer.
     * @note Complexity: O(1).
     */
    [[nodiscard]] static constexpr BoneId
    deserializeLegacy9(std::span<const std::byte, kLegacySerializedBytes> in) noexcept
    {
        const uint8_t d = std::to_integer<uint8_t>(in[8]);
        if (d > 8u)
        {
            return BoneId{};
        }
        std::array<uint64_t, kWordCount> words{};
        for (std::size_t i = 0u; i < d; ++i)
        {
            const auto v = static_cast<uint64_t>(std::to_integer<uint8_t>(in[i]));
            words[wordOf(i)] |= v << shiftOf(i);
        }
        return BoneId{detail::BoneIdTag{}, words, d};
    }

private:
    /// Word index holding level @p i (4 levels per word).
    [[nodiscard]] static constexpr std::size_t wordOf(std::size_t i) noexcept
    {
        return i / 4u;
    }

    /// Bit shift of level @p i within its word (MSB-first: level i%4 == 0 is
    /// the top 16 bits).
    [[nodiscard]] static constexpr std::size_t shiftOf(std::size_t i) noexcept
    {
        return 48u - 16u * (i % 4u);
    }

    /// Level value at position @p i with no depth check (canonical slots are
    /// zero, so reading an inactive slot yields 0).
    [[nodiscard]] constexpr uint16_t levelUnchecked(std::size_t i) const noexcept
    {
        return static_cast<uint16_t>((mWords[wordOf(i)] >> shiftOf(i)) & 0xFFFFu);
    }

    /// Zeroes the level slot at position @p i in @p words.
    static constexpr void clearSlot(std::array<uint64_t, kWordCount>& words,
                                    std::size_t i) noexcept
    {
        words[wordOf(i)] &= ~(uint64_t{0xFFFFu} << shiftOf(i));
    }

    /// Mask of word @p w's bits covered by an ancestor of depth @p d: full for
    /// words wholly within the depth, partial for the boundary word, zero past it.
    [[nodiscard]] static constexpr uint64_t wordMaskForDepth(std::size_t w,
                                                             std::size_t d) noexcept
    {
        const std::size_t firstLevelOfWord = 4u * w;
        if (d <= firstLevelOfWord)
        {
            return 0u;
        }
        const std::size_t levelsInWord = d - firstLevelOfWord;
        if (levelsInWord >= 4u)
        {
            return ~uint64_t{0};
        }
        return (~uint64_t{0}) << (64u - 16u * levelsInWord);
    }

    std::array<uint64_t, kWordCount> mWords; ///< Packed path, 4 levels per word,
                                             ///< MSB-first. Inactive slots zero.
    uint8_t mDepth;                          ///< Number of active levels [0, 16].
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
        // Fold the four path words and the depth through the same avalanche
        // used before the widening. Depth is mixed in a separate lane so ids
        // whose paths tie on words but differ in depth (e.g. [1/0] vs [1/0/0])
        // still disperse.
        uint64_t h = 0u;
        for (const uint64_t w : id.words())
        {
            h ^= w + 0x9e3779b97f4a7c15ull + (h << 6u) + (h >> 2u);
        }
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
 * 16-bit slot. All inactive slots are zero by construction, satisfying the BoneId
 * canonical-form invariant. Used exclusively by Bone<>::id().
 */
template <auto... Levels>
[[nodiscard]] constexpr BoneId buildBoneId() noexcept
{
    constexpr std::size_t kDepth = sizeof...(Levels);
    static_assert(kDepth <= BoneId::kMaxDepth, "Bone depth exceeds 16");

    std::array<uint64_t, BoneId::kWordCount> words{};
    std::size_t i = 0u;
    (
        [&]
        {
            const auto raw =
                static_cast<uint64_t>(static_cast<std::underlying_type_t<decltype(Levels)>>(Levels));
            words[i / 4u] |= (raw & 0xFFFFu) << (48u - 16u * (i % 4u));
            ++i;
        }(),
        ...
    );

    return BoneId{BoneIdTag{}, words, static_cast<uint8_t>(kDepth)};
}

} // namespace detail

// =============================================================================
// HierarchySchema -- compile-time binding of depth positions to enum types
// =============================================================================

/**
 * @brief Binds each depth position to its expected enum type.
 *
 * All level types must be enums. The schema depth must not exceed 16, the BoneId
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
    static_assert(sizeof...(LevelTypes) <= BoneId::kMaxDepth,
                  "HierarchySchema kMaxDepth cannot exceed 16 (BoneId physical limit)");

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
 * These are the FRAMEWORK's pre-registered capabilities, occupying the reserved
 * framework band (indices 0-31). The capability space itself is OPEN: the mask
 * is unbounded (see SkeletonMask below) and applications register their own
 * named capabilities at init via CapabilityRegistry (CapabilityRegistry.h),
 * receiving indices from 32 upward. There is no ceiling on how many kinds of
 * capability can exist — the vocabulary is registered, not hardcoded.
 *
 * Framework band layout:
 *   Bits 0-7:   Category   (what kind of thing this is)
 *   Bits 8-15:  Providers  (what this item provides, and the KIND of value —
 *               value kind is orthogonal to provider-ness, so a relay is
 *               ProvidesValue + ValueBinary and an analog channel is
 *               ProvidesValue + ValueContinuous; a future multi-state item is
 *               ProvidesValue + ValueDiscrete with no new provider bit)
 *   Bits 16-23: Consumers  (what this item consumes)
 *   Bits 24-31: Properties (access and visibility flags)
 *
 * @note Count is the size of the framework band (the first application index),
 *       NOT the mask width — the mask has no fixed width. Do not pass Count to
 *       makeMask().
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
    ValueBinary     = 11, ///< Provided value is boolean (relay/switch-like).
    ValueContinuous = 12, ///< Provided value is analog/continuous.
    ValueDiscrete   = 13, ///< Provided value is a discrete multi-state.

    // --- Consumers (16-23) ---
    ConsumesValue   = 16,
    ConsumesCommand = 17,
    ConsumesStatus  = 18,

    // --- Properties (24-31) ---
    Readable       = 24,
    Writable       = 25,
    Serializable   = 26,
    NetworkVisible = 27,

    Count = 32 ///< Framework band size / first application capability index. Not a valid capability.
};

/// Size of the reserved framework capability band; application capability
/// indices are allocated from this value upward by CapabilityRegistry.
inline constexpr std::size_t kFrameworkCapabilityBand =
    static_cast<std::size_t>(SkeletonCapability::Count);

/// Sanity guard for capability indices. This is NOT a design cap on the
/// capability space — it exists solely so a corrupted index cannot demand
/// gigabytes of mask storage. Raise freely if legitimate registrations ever
/// approach it (they will not: capabilities are vocabulary, which grows
/// slowly — the BE reference accumulated ~60 over eight years).
inline constexpr std::size_t kCapabilityIndexSanityBound = std::size_t{1} << 20;

/**
 * @brief An unbounded capability bitset.
 *
 * Replaces the former `std::bitset<32>` alias. The mask has NO fixed width:
 * storage is a word vector that grows to the highest set index, so the
 * capability space is limited only by what has been registered — never by the
 * mask type. This is deliberate ("no limits on expressibility"): the mask is
 * not on any hot path (compares happen at bind/registration/query time), it is
 * never serialized (it is derived per type from skeletonMask() statics at
 * construction), and it crosses no ABI — so a fixed width would be a policy
 * cap inside a mechanism, with nothing to justify it.
 *
 * Semantics:
 *  - Set operations are width-agnostic: missing high words read as zero, so
 *    masks of different storage widths compare and combine correctly.
 *  - Storage is normalized (no trailing zero words), so equality is plain
 *    member comparison.
 *  - Deliberately NOT provided (bitset-isms that presume a width): no-argument
 *    set(), all(), size(), to_ulong(). Use toString() for diagnostics.
 *
 * Use makeMask() to construct a mask from SkeletonCapability values and/or
 * registered capability indices.
 */
class SkeletonMask
{
public:
    /// Empty mask (no capabilities).
    SkeletonMask() = default;

    /// Seeds the low 64 capability indices from @p bits (index i set iff
    /// bit i of @p bits is set). Useful for tests and diagnostics.
    explicit SkeletonMask(unsigned long long bits)
    {
        if (bits != 0ull)
        {
            mWords.push_back(bits);
        }
    }

    /// @brief Sets capability @p index. Grows storage as needed.
    void set(std::size_t index)
    {
        FATP_ALWAYS_ENFORCE(index < kCapabilityIndexSanityBound,
            "SkeletonMask::set(): capability index exceeds the corruption sanity "
            "bound (kCapabilityIndexSanityBound). This is a bug, not a capacity "
            "limit -- indices come from CapabilityRegistry allocation.");
        const std::size_t word = index / 64u;
        if (mWords.size() <= word)
        {
            mWords.resize(word + 1u, 0ull);
        }
        mWords[word] |= (1ull << (index % 64u));
    }

    /// @brief Clears capability @p index. No-op if beyond current storage.
    void reset(std::size_t index) noexcept
    {
        const std::size_t word = index / 64u;
        if (word < mWords.size())
        {
            mWords[word] &= ~(1ull << (index % 64u));
            trim();
        }
    }

    /// @brief True if capability @p index is set (false beyond storage).
    [[nodiscard]] bool test(std::size_t index) const noexcept
    {
        const std::size_t word = index / 64u;
        return word < mWords.size() &&
               ((mWords[word] >> (index % 64u)) & 1ull) != 0ull;
    }

    /// @brief True if no capability is set.
    [[nodiscard]] bool none() const noexcept { return mWords.empty(); }

    /// @brief True if any capability is set.
    [[nodiscard]] bool any() const noexcept { return !mWords.empty(); }

    /// @brief Number of set capabilities.
    [[nodiscard]] std::size_t count() const noexcept
    {
        std::size_t n = 0;
        for (const std::uint64_t w : mWords)
        {
            n += static_cast<std::size_t>(std::popcount(w));
        }
        return n;
    }

    // ── Set algebra (width-agnostic) ────────────────────────────────────

    SkeletonMask& operator&=(const SkeletonMask& rhs) noexcept
    {
        if (mWords.size() > rhs.mWords.size())
        {
            mWords.resize(rhs.mWords.size());
        }
        for (std::size_t i = 0; i < mWords.size(); ++i)
        {
            mWords[i] &= rhs.mWords[i];
        }
        trim();
        return *this;
    }

    SkeletonMask& operator|=(const SkeletonMask& rhs)
    {
        if (mWords.size() < rhs.mWords.size())
        {
            mWords.resize(rhs.mWords.size(), 0ull);
        }
        for (std::size_t i = 0; i < rhs.mWords.size(); ++i)
        {
            mWords[i] |= rhs.mWords[i];
        }
        return *this;
    }

    [[nodiscard]] friend SkeletonMask operator&(SkeletonMask lhs,
                                                const SkeletonMask& rhs) noexcept
    {
        lhs &= rhs;
        return lhs;
    }

    [[nodiscard]] friend SkeletonMask operator|(SkeletonMask lhs,
                                                const SkeletonMask& rhs)
    {
        lhs |= rhs;
        return lhs;
    }

    /// Normalized storage makes defaulted comparison width-correct: equal
    /// masks compare equal regardless of how they were built.
    [[nodiscard]] friend bool operator==(const SkeletonMask&,
                                         const SkeletonMask&) = default;

    // ── Diagnostics ─────────────────────────────────────────────────────

    /// @brief Hex rendering, most-significant word first (e.g. "0x0", "0x900").
    [[nodiscard]] std::string toString() const
    {
        if (mWords.empty())
        {
            return "0x0";
        }
        static constexpr char kHex[] = "0123456789abcdef";
        std::string out = "0x";
        bool leading = true;
        for (std::size_t w = mWords.size(); w-- > 0;)
        {
            for (int nibble = 15; nibble >= 0; --nibble)
            {
                const unsigned digit =
                    static_cast<unsigned>((mWords[w] >> (nibble * 4)) & 0xFull);
                if (leading && digit == 0 && !(w == 0 && nibble == 0))
                {
                    continue;
                }
                leading = false;
                out.push_back(kHex[digit]);
            }
        }
        return out;
    }

private:
    void trim() noexcept
    {
        while (!mWords.empty() && mWords.back() == 0ull)
        {
            mWords.pop_back();
        }
    }

    std::vector<std::uint64_t> mWords; // little-endian words, normalized (no zero tail)
};

/// Streams the mask's toString() (diagnostics; std::bitset had an inserter too).
inline std::ostream& operator<<(std::ostream& os, const SkeletonMask& mask)
{
    return os << mask.toString();
}

namespace detail
{
/// Framework capability → index, rejecting the Count sentinel.
[[nodiscard]] inline std::size_t capabilityIndexOf(SkeletonCapability cap)
{
    const std::size_t idx = static_cast<std::size_t>(static_cast<uint32_t>(cap));
    FATP_ALWAYS_ENFORCE(idx < kFrameworkCapabilityBand,
        "makeMask(): invalid framework capability. Do not pass SkeletonCapability::Count.");
    return idx;
}

/// Registered capability index (application band) — passed through; the
/// sanity bound is enforced by SkeletonMask::set().
[[nodiscard]] inline std::size_t capabilityIndexOf(std::size_t index) noexcept
{
    return index;
}
} // namespace detail

/**
 * @brief Constructs a SkeletonMask with the given capability bits set.
 *
 * Accepts framework capabilities (SkeletonCapability values) and registered
 * application capability indices (std::size_t, as returned by
 * CapabilityRegistry::registerCapability) — freely mixed.
 *
 * @pre Each SkeletonCapability value must be a defined capability other than
 *      Count. Passing Count terminates the process in all build configurations
 *      via FATP_ALWAYS_ENFORCE.
 *
 * @code
 * auto m  = makeMask(SkeletonCapability::Sensor, SkeletonCapability::ProvidesValue);
 * auto m2 = makeMask(SkeletonCapability::ProvidesValue, hydraulicCapIndex);
 * @endcode
 *
 * @tparam Caps... SkeletonCapability values and/or std::size_t indices.
 * @return A SkeletonMask with the corresponding bits set.
 */
template <typename... Caps>
    requires((std::convertible_to<Caps, SkeletonCapability> ||
              std::convertible_to<Caps, std::size_t>) &&
             ...)
[[nodiscard]] SkeletonMask makeMask(Caps... caps)
{
    SkeletonMask m;
    (m.set(detail::capabilityIndexOf(caps)), ...);
    return m;
}

} // namespace fat_p::skeleton
