#pragma once
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: public_header
  path: include/fat_p/Skeleton.h
  namespace: fat_p::skeleton
  layer: Domain
  summary: Typed hierarchical item registry -- Bone, SkeletonItem, BasicBoneItem, Skeleton.
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
 * @file Skeleton.h
 * @brief Full Skeleton implementation: typed hierarchical item registry.
 *
 * @details
 * Skeleton is a typed hierarchical framework upon which items are hung.
 * Every item has a bone (typed address), a mask (capability description),
 * and a position in the hierarchy. The skeleton is passive -- items publish
 * themselves onto it; other items find them by address or capability query.
 *
 * Key types:
 * - Bone<Schema, ...Levels>                     -- compile-time typed hierarchical address
 * - SkeletonItem                                -- non-owning base for all items
 * - BasicBoneItem<Schema, MaskPol, Levels...>   -- typed helper base
 * - BoneItem<Schema, Levels...>                 -- common alias (DefaultMaskPolicy)
 * - Skeleton                                    -- single-threaded passive registry
 *
 * Lifecycle contract:
 * - Call publish(skeleton) from the most-derived constructor, after all members
 *   are initialized.
 * - Call unpublish() from the most-derived destructor, before any member teardown.
 * - Destroying a published item terminates the process.
 * - Destroying a non-empty Skeleton terminates the process.
 *
 * Threading:
 * - Skeleton is single-threaded. All operations must run on the same thread
 *   or under external synchronization. There is no ThreadSafeSkeleton in v1.
 *
 * Requirements:
 * - C++20
 * - fat_p headers: SkeletonFwd.h, Signal.h, FastHashMap.h, ScopeGuard.h
 *
 * @see SkeletonFwd.h for BoneId, HierarchySchema, SkeletonCapability, SkeletonMask.
 */

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "enforce.h"
#include "ScopeGuard.h"

#include "FastHashMap.h"

#include "Signal.h"
#include "SkeletonFwd.h"

namespace fat_p::skeleton
{

// =============================================================================
// Exceptions
// =============================================================================

/**
 * @brief Thrown when an item is published twice, or with a null BoneId.
 */
struct PublicationError : std::logic_error
{
    using std::logic_error::logic_error;
};

/**
 * @brief Thrown when two items with the same BoneId are published on the same Skeleton.
 * The registry is unchanged on throw.
 */
struct DuplicateBoneError : std::logic_error
{
    using std::logic_error::logic_error;
};

// =============================================================================
// detail -- internal helpers
// =============================================================================

namespace detail
{

// -------------------------------------------------------------------------
// SkeletonMaskPolicy concept -- constrains the MaskPol parameter of BasicBoneItem.
// -------------------------------------------------------------------------

template <typename MaskPolicy>
concept SkeletonMaskPolicy = requires
{
    typename MaskPolicy::mask_type;
} && std::same_as<typename MaskPolicy::mask_type, SkeletonMask>;

// -------------------------------------------------------------------------
// kLevelsMatchSchema -- validate that each enum value in Levels... has
// the type expected by Schema at that depth position.
// -------------------------------------------------------------------------

template <typename Schema, std::size_t Idx, auto Head, auto... Tail>
struct LevelsMatchSchemaHelper
{
    static constexpr bool kValue =
        std::is_same_v<decltype(Head), typename Schema::template expected_type<Idx>> &&
        LevelsMatchSchemaHelper<Schema, Idx + 1u, Tail...>::kValue;
};

template <typename Schema, std::size_t Idx, auto Last>
struct LevelsMatchSchemaHelper<Schema, Idx, Last>
{
    static constexpr bool kValue =
        std::is_same_v<decltype(Last), typename Schema::template expected_type<Idx>>;
};

template <typename Schema, auto... Levels>
struct LevelsMatchSchemaDispatch
{
    static constexpr bool kValue = LevelsMatchSchemaHelper<Schema, 0u, Levels...>::kValue;
};

template <typename Schema>
struct LevelsMatchSchemaDispatch<Schema>
{
    static constexpr bool kValue = true;
};

template <typename Schema, auto... Levels>
inline constexpr bool kLevelsMatchSchema = LevelsMatchSchemaDispatch<Schema, Levels...>::kValue;

// -------------------------------------------------------------------------
// BoneParentFromTuple -- given a tuple of integral_constants and an index
// sequence selecting all-but-last, produces the parent Bone type.
// -------------------------------------------------------------------------

// Forward declaration -- Bone must be declared before this specialization is used.
template <typename Schema, auto... Levels>
    requires(sizeof...(Levels) <= Schema::kMaxDepth) && kLevelsMatchSchema<Schema, Levels...>
struct BoneForward;

template <typename Schema, typename ICTuple, typename IdxSeq>
struct BoneParentFromTuple;

template <typename Schema, typename ICTuple, std::size_t... Is>
struct BoneParentFromTuple<Schema, ICTuple, std::index_sequence<Is...>>
{
    using type = typename BoneForward<Schema, std::tuple_element_t<Is, ICTuple>::value...>::type;
};

} // namespace detail

// =============================================================================
// Bone -- compile-time typed hierarchical address
// =============================================================================

// Forward declaration needed for BoneParentFromTuple to resolve to the real type.
template <typename Schema, auto... Levels>
    requires(sizeof...(Levels) <= Schema::kMaxDepth) && detail::kLevelsMatchSchema<Schema, Levels...>
struct Bone;

namespace detail
{

// Resolve BoneForward to the real Bone type.
template <typename Schema, auto... Levels>
    requires(sizeof...(Levels) <= Schema::kMaxDepth) && kLevelsMatchSchema<Schema, Levels...>
struct BoneForward
{
    using type = Bone<Schema, Levels...>;
};

} // namespace detail

// -------------------------------------------------------------------------
// Bone definition
// -------------------------------------------------------------------------

/**
 * @brief Compile-time typed hierarchical address.
 *
 * A Bone is a sequence of enum values, one per depth level, validated at
 * instantiation against the Schema's expected types. Two Bone types with the
 * same Schema and enum values are the same type; their BoneId values are equal.
 *
 * @tparam Schema    A HierarchySchema<...> instantiation.
 * @tparam Levels... Enum values, one per depth level, matching the schema.
 *
 * @code
 * using B = Bone<SysSchema, System::Root, Subsystem::Sensors, Channel::Load>;
 * constexpr BoneId id = B::id();    // compile-time
 * using Parent = B::Parent;         // Bone<SysSchema, System::Root, Subsystem::Sensors>
 * using Child  = B::template child<Node::Primary>;
 * @endcode
 */
template <typename Schema, auto... Levels>
    requires(sizeof...(Levels) <= Schema::kMaxDepth) && detail::kLevelsMatchSchema<Schema, Levels...>
struct Bone
{
    /// The HierarchySchema instantiation that defines this Bone's level types.
    using schema_type = Schema;

    /// The number of active depth levels in this Bone's path.
    static constexpr std::size_t kDepth = sizeof...(Levels);

    // Reject enum values that don't fit in one byte -- catches silent truncation.
    static_assert(
        ([]<auto L>() consteval {
            using U = std::underlying_type_t<decltype(L)>;
            if constexpr (std::is_signed_v<U>)
            {
                return static_cast<long long>(static_cast<U>(L)) >= 0LL
                    && static_cast<long long>(static_cast<U>(L)) <= 255LL;
            }
            else
            {
                return static_cast<unsigned long long>(static_cast<U>(L)) <= 255ULL;
            }
        }.template operator()<Levels>() && ...),
        "Each Bone level enum value must fit in one byte (0..255)"
    );

    /**
     * @brief Returns the compile-time BoneId for this path.
     * @return The BoneId corresponding to this Bone's level sequence.
     */
    [[nodiscard]] static constexpr BoneId id() noexcept
    {
        return detail::buildBoneId<Levels...>();
    }

    /**
     * @brief Appends one more level, yielding a deeper Bone type.
     *
     * ChildLevel must have the type Schema::expected_type<kDepth>. This is
     * enforced by the requires clause on Bone<Schema, Levels..., ChildLevel>
     * via kLevelsMatchSchema -- no redundant type check is placed here,
     * because instantiating expected_type<kDepth> when kDepth == kMaxDepth
     * would produce a hard error rather than a clean constraint failure.
     */
    template <auto ChildLevel>
        requires (kDepth < Schema::kMaxDepth)
    using child = Bone<Schema, Levels..., ChildLevel>;

private:
    // Build the parent type using index_sequence to select all-but-last from Levels.
    using AllICs = std::tuple<std::integral_constant<decltype(Levels), Levels>...>;
    using ParentIdxSeq = std::make_index_sequence<(kDepth > 0u ? kDepth - 1u : 0u)>;

public:
    /**
     * @brief The parent Bone type (one fewer level). void if depth == 0.
     */
    using Parent = std::conditional_t<
        kDepth == 0u,
        void,
        typename detail::BoneParentFromTuple<Schema, AllICs, ParentIdxSeq>::type>;
};

// =============================================================================
// DefaultMaskPolicy -- placeholder for the MaskPol extension point
// =============================================================================

/**
 * @brief Default mask policy tag. v1 uses SkeletonMask (std::bitset<32>) directly.
 * Reserved as an extension point so that adding a custom mask policy later
 * does not require a breaking type change on BasicBoneItem.
 */
struct DefaultMaskPolicy
{
    /// The mask type provided by this policy. Always SkeletonMask in v1.
    using mask_type = SkeletonMask;
};

// =============================================================================
// SkeletonItem -- non-owning base for all items
// =============================================================================

// Forward declarations for serialization support (OwnerSkeleton / Phase 0).
// Full types are defined in SkeletonSerializer.h (fat_p Phase 1 deliverable).
// The default no-op virtual bodies on SkeletonItem do not call any methods on
// these types, so forward declarations are sufficient in this header.
class SerializeWriter;
class SerializeReader;

/**
 * @brief Base class for all items that can be published onto a Skeleton.
 *
 * SkeletonItem is non-copyable and non-movable. Address stability is required
 * because the Skeleton registry holds raw pointers.
 *
 * Lifecycle (must be observed by the most-derived class):
 * 1. Call publish(skeleton) from the most-derived constructor body, after all
 *    members are initialized.
 * 2. Call unpublish() from the most-derived destructor body, before any member
 *    teardown begins.
 *
 * Failure to call unpublish() before the object is destroyed terminates the
 * process. A non-owning registry cannot safely clean up objects it does not own.
 */
class SkeletonItem
{
public:
    /**
     * @brief Destructor. Terminates the process if the item is still published.
     *
     * @note Call unpublish() from the most-derived destructor before member
     * teardown to avoid termination.
     */
    virtual ~SkeletonItem() noexcept;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// Returns the BoneId that identifies this item in the registry.
    [[nodiscard]] BoneId boneId() const noexcept
    {
        return mBoneId;
    }

    /// Returns the current capability mask.
    [[nodiscard]] const SkeletonMask& mask() const noexcept
    {
        return mMask;
    }

    /// Returns the diagnostic name. Empty string_view if none was provided.
    [[nodiscard]] std::string_view name() const noexcept
    {
        return mName;
    }

    /// Returns true if this item is currently published on a Skeleton.
    [[nodiscard]] bool isPublished() const noexcept
    {
        return mSkeleton != nullptr;
    }

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Publishes this item onto @p skeleton.
     *
     * The item becomes discoverable via find() and query() immediately.
     * mOnPublished fires after insertion.
     *
     * @param skeleton The Skeleton to register this item on.
     * @throws PublicationError   if already published, or if boneId() is null.
     * @throws DuplicateBoneError if another item at the same BoneId is already
     *         registered on @p skeleton. The registry is unchanged on throw.
     *
     * Call from the most-derived constructor, after all members are initialized.
     */
    void publish(Skeleton& skeleton);

    /**
     * @brief Removes this item from its skeleton.
     *
     * mOnUnpublishing fires while the item is still in the registry.
     * No-op if not currently published.
     *
     * Call from the most-derived destructor, before any member teardown.
     */
    void unpublish() noexcept;

    // Non-copyable, non-movable.
    SkeletonItem(const SkeletonItem&) = delete;
    SkeletonItem& operator=(const SkeletonItem&) = delete;
    SkeletonItem(SkeletonItem&&) = delete;
    SkeletonItem& operator=(SkeletonItem&&) = delete;

protected:
    /**
     * @brief Constructs an unpublished item.
     *
     * @param id    The BoneId that identifies this item.
     * @param mask  Initial capability mask.
     * @param name  Optional diagnostic name (owned copy).
     */
    explicit SkeletonItem(BoneId id, SkeletonMask mask, std::string name = {})
        : mBoneId(id)
        , mMask(mask)
        , mName(std::move(name))
        , mSkeleton(nullptr)
    {
    }

    /**
     * @brief Updates the capability mask.
     *
     * If published: emits mOnMaskChanged on the bound Skeleton.
     * If not published: updates the local mask only; no signal.
     *
     * @param newMask The capability mask to assign. Replaces the previous mask entirely.
     */
    void setMask(SkeletonMask newMask) noexcept;

    // ── Serialization hooks ───────────────────────────────────────────────────
    //
    // Default implementations are no-ops. Override in derived classes to
    // participate in OwnerSkeleton-driven save/load.
    //
    // serializationVersion() is written by save() before calling serialize(),
    // and passed back to deserialize() before calling it. Inspect the version
    // parameter in deserialize() to apply schema migration logic for older files.
    //
    // SerializeWriter and SerializeReader are thin wrappers over a binary stream
    // (see SkeletonSerializer.h, fat_p Phase 1 deliverable). Forward declarations
    // allow these default no-op bodies to compile without the full type.

    [[nodiscard]] virtual uint32_t serializationVersion() const noexcept { return 1u; }

    virtual void serialize(SerializeWriter&) const {}

    virtual void deserialize(SerializeReader&, uint32_t /*version*/) {}

private:
    BoneId mBoneId;
    SkeletonMask mMask;
    std::string mName;       // owning -- no lifetime contract on the caller
    Skeleton* mSkeleton;     // set by publish(), cleared by unpublish()
    bool mUnpublishing{false}; // reentrancy guard for unpublish()

    friend class Skeleton;
};

// =============================================================================
// Skeleton -- single-threaded passive registry
// =============================================================================

/**
 * @brief Passive hierarchical item registry.
 *
 * Skeleton stores non-owning raw pointers to published SkeletonItems.
 * Items own themselves; the Skeleton only observes their existence.
 *
 * Threading: single-threaded. All publish/unpublish/find/query and signal
 * operations must run on the same thread or under external synchronization.
 * There is no ThreadSafeSkeleton in v1.
 *
 * Destruction invariant: if any items remain published when the Skeleton is
 * destroyed, the process is terminated. A non-owning registry must fail fast
 * on lifetime mistakes rather than silently dereference dangling pointers.
 */
class Skeleton
{
public:
    /**
     * @brief Constructs an empty Skeleton.
     *
     * @param name Optional diagnostic name, used in FATAL error messages and dump().
     */
    explicit Skeleton(std::string name = "default")
        : mName(std::move(name))
    {
    }

    /**
     * @brief Terminates the process if any items remain published.
     */
    ~Skeleton() noexcept
    {
        if (!mRegistry.empty())
        {
            std::fprintf(
                stderr,
                "[Skeleton] FATAL: Skeleton \"%s\" destroyed with %zu item(s) still published."
                " Terminating.\n",
                mName.c_str(),
                mRegistry.size()
            );
            std::terminate();
        }
    }

    // Non-copyable, non-movable.
    Skeleton(const Skeleton&) = delete;
    Skeleton& operator=(const Skeleton&) = delete;
    Skeleton(Skeleton&&) = delete;
    Skeleton& operator=(Skeleton&&) = delete;

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    /// Returns the diagnostic name of this Skeleton.
    [[nodiscard]] std::string_view name() const noexcept
    {
        return mName;
    }

private:
    friend class SkeletonItem;

    // -------------------------------------------------------------------------
    // Publication -- called exclusively by SkeletonItem via friendship.
    // Not part of the public API.
    // -------------------------------------------------------------------------

    /**
     * @brief Registers @p item in the skeleton.
     *
     * Called by SkeletonItem::publish(). Not intended for direct external use.
     *
     * @param item The SkeletonItem to register.
     * @throws PublicationError   if boneId is null.
     * @throws DuplicateBoneError if a different item at the same BoneId already exists.
     */
    void publish(SkeletonItem& item)
    {
        if (mTraversalDepth > 0u)
        {
            std::fprintf(
                stderr,
                "[Skeleton] FATAL: publish() called on \"%s\" during visitSubtree traversal."
                " Terminating.\n",
                mName.c_str()
            );
            std::terminate();
        }
        if (item.mBoneId.isNull())
        {
            throw PublicationError("Cannot publish an item with a null BoneId");
        }
        if (mRegistry.contains(item.mBoneId))
        {
            throw DuplicateBoneError(
                "A different item is already published at " + item.mBoneId.toString()
            );
        }
        [[maybe_unused]] auto* inserted = mRegistry.insert(item.mBoneId, &item);

        // Roll back insertion if mOnPublished.emit() propagates an exception.
        // If emit() propagates an exception (under PropagateExceptionPolicy or a custom
        // propagating policy), the guard removes the item from the registry and clears
        // item.mSkeleton. With the default CatchAndIgnorePolicy, emit() never throws
        // and this guard is a no-op at runtime.
        // FATP_ENFORCE does NOT activate this guard -- it terminates the process; no
        // exception is ever in flight on that path.
        // noexcept omitted: FastHashMap::erase() carries no noexcept guarantee
        // in its contract; BoneId keys do not throw in practice.
        auto rollbackGuard = fat_p::makeScopeGuardOnFail([&]()
        {
            mRegistry.erase(item.mBoneId);
            item.mSkeleton = nullptr;
        });

        FATP_ENFORCE(inserted != nullptr,
            "FastHashMap::insert returned nullptr after contains() passed -- internal inconsistency");

        item.mSkeleton = this;

        ++mEmittingDepth;
        // ScopeGuard mirrors the mTraversalDepth pattern in visitSubtree:
        // decrement is unconditional even if emit() propagates an exception.
        auto emitGuard = fat_p::makeScopeGuard([this]() noexcept
        {
            --mEmittingDepth;
        });
        mOnPublished.emit(item);
    }

    /**
     * @brief Removes @p item from the registry.
     *
     * Called by SkeletonItem::unpublish(). Not intended for direct external use.
     * mOnUnpublishing fires before removal so observers have one last consistent view.
     * Guards against wrong-skeleton calls and reentrant unpublish.
     *
     * @param item The SkeletonItem to remove.
     */
    void unpublish(SkeletonItem& item) noexcept
    {
        if (mTraversalDepth > 0u)
        {
            std::fprintf(
                stderr,
                "[Skeleton] FATAL: unpublish() called on \"%s\" during visitSubtree traversal."
                " Terminating.\n",
                mName.c_str()
            );
            std::terminate();
        }
        if (mEmittingDepth > 0u)
        {
            std::fprintf(
                stderr,
                "[Skeleton] FATAL: unpublish() called on \"%s\" during mOnPublished emission."
                " Call unpublish() after publish() returns, not from an mOnPublished callback."
                " Terminating.\n",
                mName.c_str()
            );
            std::terminate();
        }
        if (item.mSkeleton != this)
        {
            return; // Ownership guard: not our item.
        }
        if (item.mUnpublishing)
        {
            return; // Reentrancy guard: already in progress.
        }
        item.mUnpublishing = true;
        auto unpublishGuard = fat_p::makeScopeGuard([&item]() noexcept
        {
            item.mUnpublishing = false;
        });
        mOnUnpublishing.emit(item);
        // erase() does not throw for BoneId keys; Skeleton::unpublish() is
        // noexcept and FastHashMap::erase() carries no noexcept guarantee in
        // its contract, but will not throw for a trivially-hashed value type.
        mRegistry.erase(item.mBoneId);
        item.mSkeleton = nullptr;
    }

    /**
     * @brief Emits mOnMaskChanged for @p item with the previous mask @p oldMask.
     *
     * Called by SkeletonItem::setMask(). Not intended for direct external use.
     *
     * @param item    The item whose mask changed.
     * @param oldMask The mask value before the change.
     */
    void notifyMaskChanged(SkeletonItem& item, SkeletonMask oldMask) noexcept
    {
        mOnMaskChanged.emit(item, oldMask);
    }

    // -------------------------------------------------------------------------
    // Lookup
    // -------------------------------------------------------------------------

public:
    /**
     * @brief Finds the item at the given @p id. Returns nullptr if not found.
     *
     * @param id The BoneId to look up.
     * @return Pointer to the item, or nullptr.
     * @note Complexity: O(1) average.
     * @note Thread-safety: NOT thread-safe.
     */
    [[nodiscard]] SkeletonItem* find(BoneId id) noexcept
    {
        auto* ptr = mRegistry.find(id);
        return ptr ? *ptr : nullptr;
    }

    /// @copydoc find(BoneId)
    [[nodiscard]] const SkeletonItem* find(BoneId id) const noexcept
    {
        const auto* ptr = mRegistry.find(id);
        return ptr ? *ptr : nullptr;
    }

    /**
     * @brief Finds the item at @p id and static_casts it to T*.
     * Returns nullptr if not found.
     *
     * @pre The item at @p id must have dynamic type T. Violating this precondition
     * is undefined behavior. Use only when the schema guarantees the type at the
     * given address.
     *
     * @tparam T The expected concrete type of the item.
     * @param id The BoneId to look up.
     * @return Static-cast pointer to T, or nullptr.
     * @note Complexity: O(1) average.
     * @note Thread-safety: NOT thread-safe.
     */
    template <typename T>
    [[nodiscard]] T* findAs(BoneId id) noexcept
    {
        return static_cast<T*>(find(id));
    }

    /// @copydoc findAs(BoneId)
    template <typename T>
    [[nodiscard]] const T* findAs(BoneId id) const noexcept
    {
        return static_cast<const T*>(find(id));
    }

    // -------------------------------------------------------------------------
    // Traversal
    // -------------------------------------------------------------------------

    /**
     * @brief Visits all items whose BoneId is a descendant of @p root (inclusive).
     *
     * @p fn is called as fn(SkeletonItem&) for each matching item. Order is
     * ascending BoneId (parent-before-child). The callback may not publish
     * or unpublish items during traversal -- doing so terminates the process.
     *
     * @tparam Fn Callable with signature compatible with void(SkeletonItem&).
     * @param root The subtree root BoneId. The root itself is included if present.
     * @param fn   The callback to invoke for each item.
     * @note Exception-safe: mTraversalDepth is restored via ScopeGuard even
     *       if the callback throws.
     * @note Complexity: O(N log N) where N is the number of published items
     *       (dominated by sort; the callback itself is not counted).
     * @note Thread-safety: NOT thread-safe.
     */
    template <typename Fn>
    void visitSubtree(BoneId root, Fn&& fn)
    {
        auto sorted = collectSubtree(root);
        ++mTraversalDepth;
        auto depthGuard = fat_p::makeScopeGuard([this]() noexcept
        {
            --mTraversalDepth;
        });
        for (SkeletonItem* item : sorted)
        {
            std::invoke(fn, *item);
        }
    }

    /// @copydoc visitSubtree(BoneId, Fn&&)
    template <typename Fn>
    void visitSubtree(BoneId root, Fn&& fn) const
    {
        auto sorted = collectSubtreeConst(root);
        ++mTraversalDepth;
        auto depthGuard = fat_p::makeScopeGuard([this]() noexcept
        {
            --mTraversalDepth;
        });
        for (const SkeletonItem* item : sorted)
        {
            std::invoke(fn, *item);
        }
    }

    // -------------------------------------------------------------------------
    // Queries -- results in ascending BoneId order (parent-before-child)
    // -------------------------------------------------------------------------

    /**
     * @brief Returns all published items that have every bit in @p required set
     * and no bit in @p excluded set.
     *
     * Results are sorted in ascending BoneId order. Callers that need
     * child-before-parent order (e.g. shutdown sequencing) iterate in reverse.
     *
     * @param required Mask of bits that must all be set on matching items.
     * @param excluded Mask of bits that must all be clear on matching items.
     * @return Mutable pointers to matching items in ascending BoneId order.
     * @note Complexity: O(N log N) where N is the number of published items (iterate all + sort).
     * @note Thread-safety: NOT thread-safe.
     */
    [[nodiscard]] std::vector<SkeletonItem*>
    query(SkeletonMask required, SkeletonMask excluded = {})
    {
        std::vector<SkeletonItem*> result;
        for (auto it = mRegistry.begin(); it != mRegistry.end(); ++it)
        {
            SkeletonItem* item = it.value();
            if (matchesMask(item->mask(), required, excluded))
            {
                result.push_back(item);
            }
        }
        sortByBoneId(result);
        return result;
    }

    /**
     * @brief Returns all published items that satisfy the mask predicate (const overload).
     *
     * Returns read-only pointers. Use the non-const overload when mutation of
     * items is required.
     *
     * @param required Mask of bits that must all be set on matching items.
     * @param excluded Mask of bits that must all be clear on matching items.
     * @return Read-only pointers to matching items in ascending BoneId order.
     * @note Complexity: O(N log N) where N is the number of published items (iterate all + sort).
     * @note Thread-safety: NOT thread-safe.
     */
    [[nodiscard]] std::vector<const SkeletonItem*>
    query(SkeletonMask required, SkeletonMask excluded = {}) const
    {
        std::vector<const SkeletonItem*> result;
        for (auto it = mRegistry.cbegin(); it != mRegistry.cend(); ++it)
        {
            const SkeletonItem* item = it.value();
            if (matchesMask(item->mask(), required, excluded))
            {
                result.push_back(item);
            }
        }
        sortByBoneId(result);
        return result;
    }

    /**
     * @brief Returns all published items within the subtree rooted at @p root
     * that satisfy the mask predicate.
     *
     * @p root is included if it matches.
     *
     * @param root     The subtree root BoneId.
     * @param required Mask of bits that must all be set on matching items.
     * @param excluded Mask of bits that must all be clear on matching items.
     * @return Mutable pointers to matching items in ascending BoneId order.
     * @note Complexity: O(N log N) where N is the number of published items (iterate all + sort).
     * @note Thread-safety: NOT thread-safe.
     */
    [[nodiscard]] std::vector<SkeletonItem*>
    querySubtree(BoneId root, SkeletonMask required, SkeletonMask excluded = {})
    {
        std::vector<SkeletonItem*> result;
        for (auto it = mRegistry.begin(); it != mRegistry.end(); ++it)
        {
            const BoneId id = it.key();
            SkeletonItem* item = it.value();
            if ((id == root || root.isAncestorOf(id)) &&
                matchesMask(item->mask(), required, excluded))
            {
                result.push_back(item);
            }
        }
        sortByBoneId(result);
        return result;
    }

    /**
     * @brief Returns all published items within the subtree rooted at @p root
     * that satisfy the mask predicate (const overload).
     *
     * Returns read-only pointers. Use the non-const overload when mutation of
     * items is required.
     *
     * @param root     The subtree root BoneId.
     * @param required Mask of bits that must all be set on matching items.
     * @param excluded Mask of bits that must all be clear on matching items.
     * @return Read-only pointers to matching items in ascending BoneId order.
     * @note Complexity: O(N log N) where N is the number of published items (iterate all + sort).
     * @note Thread-safety: NOT thread-safe.
     */
    [[nodiscard]] std::vector<const SkeletonItem*>
    querySubtree(BoneId root, SkeletonMask required, SkeletonMask excluded = {}) const
    {
        std::vector<const SkeletonItem*> result;
        for (auto it = mRegistry.cbegin(); it != mRegistry.cend(); ++it)
        {
            const BoneId id = it.key();
            const SkeletonItem* item = it.value();
            if ((id == root || root.isAncestorOf(id)) &&
                matchesMask(item->mask(), required, excluded))
            {
                result.push_back(item);
            }
        }
        sortByBoneId(result);
        return result;
    }

    // -------------------------------------------------------------------------
    // Lifecycle signal subscription API
    //
    // Signals are private. Callers subscribe via these methods and receive a
    // ScopedConnection that disconnects automatically on destruction.
    //
    // Emission ordering:
    //   publish():   item is inserted into registry, THEN mOnPublished fires.
    //   unpublish(): mOnUnpublishing fires while item is still in registry,
    //                THEN item is removed, THEN skeleton back-reference is cleared.
    //
    // Reentrancy rules (enforced at runtime):
    //   - publish() and unpublish() are forbidden during visitSubtree traversal.
    //   - unpublish() is forbidden during mOnPublished emission. A callback that
    //     tries to unpublish its own item before publish() returns violates the
    //     lifecycle postcondition and terminates the process.
    //   - Reentrant publish() from an mOnPublished callback is permitted: Signal
    //     handles recursive emission, and Skeleton::publish() only modifies
    //     mRegistry (not the signal slot list).
    // -------------------------------------------------------------------------

    /**
     * @brief Subscribes to item-published events.
     *
     * The callback fires after the item is inserted into the registry.
     * find() and query() will return the new item inside the callback.
     *
     * @tparam Fn Callable with signature compatible with void(SkeletonItem&).
     * @param fn Callable with signature void(SkeletonItem&).
     * @param priority Higher-priority slots are called first (default 0).
     * @return ScopedConnection that auto-disconnects on destruction.
     * @note Complexity: O(1).
     * @note Thread-safety: NOT thread-safe.
     */
    template <typename Fn>
    [[nodiscard]] ScopedConnection onPublished(Fn&& fn, int priority = 0)
    {
        return mOnPublished.connect(std::forward<Fn>(fn), priority);
    }

    /**
     * @brief Subscribes to item-unpublishing events.
     *
     * The callback fires while the item is still in the registry (before removal).
     * find() and query() will still return the item inside the callback.
     *
     * @tparam Fn Callable with signature compatible with void(SkeletonItem&).
     * @param fn Callable with signature void(SkeletonItem&).
     * @param priority Higher-priority slots are called first (default 0).
     * @return ScopedConnection that auto-disconnects on destruction.
     * @note Complexity: O(1).
     * @note Thread-safety: NOT thread-safe.
     */
    template <typename Fn>
    [[nodiscard]] ScopedConnection onUnpublishing(Fn&& fn, int priority = 0)
    {
        return mOnUnpublishing.connect(std::forward<Fn>(fn), priority);
    }

    /**
     * @brief Subscribes to capability-mask-changed events.
     *
     * The callback fires after the mask is updated. The second argument is
     * the previous mask value; item.mask() already reflects the new value.
     *
     * @tparam Fn Callable with signature compatible with void(SkeletonItem&, SkeletonMask).
     * @param fn Callable with signature void(SkeletonItem&, SkeletonMask).
     * @param priority Higher-priority slots are called first (default 0).
     * @return ScopedConnection that auto-disconnects on destruction.
     * @note Complexity: O(1).
     * @note Thread-safety: NOT thread-safe.
     */
    template <typename Fn>
    [[nodiscard]] ScopedConnection onMaskChanged(Fn&& fn, int priority = 0)
    {
        return mOnMaskChanged.connect(std::forward<Fn>(fn), priority);
    }

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------

    /// Returns the number of currently published items.
    [[nodiscard]] std::size_t size() const noexcept
    {
        return mRegistry.size();
    }

    /**
     * @brief Prints all published items to @p out in ascending BoneId order.
     *
     * @param out The output stream to write to.
     * @note Complexity: O(N log N) where N is the number of published items.
     * @note Thread-safety: NOT thread-safe.
     */
    void dump(std::ostream& out) const
    {
        out << "Skeleton[\"" << mName << "\"] (" << mRegistry.size() << " items):\n";
        std::vector<std::pair<BoneId, const SkeletonItem*>> items;
        for (auto it = mRegistry.cbegin(); it != mRegistry.cend(); ++it)
        {
            items.emplace_back(it.key(), it.value());
        }
        std::sort(
            items.begin(),
            items.end(),
            [](const auto& a, const auto& b)
            {
                return a.first < b.first;
            }
        );
        for (const auto& [id, item] : items)
        {
            char hexBuf[12];
            std::snprintf(hexBuf, sizeof(hexBuf), "0x%08x",
                static_cast<uint32_t>(item->mask().to_ulong()));
            out << "  " << id.toString()
                << "  name=\"" << item->name() << "\""
                << "  mask=" << hexBuf << '\n';
        }
    }

private:
    std::string mName;
    FastHashMap<BoneId, SkeletonItem*> mRegistry;
    mutable uint32_t mTraversalDepth{0}; // mutation guard: non-zero during visitSubtree
    uint32_t mEmittingDepth{0};          // mutation guard: non-zero during mOnPublished emission

    Signal<void(SkeletonItem&)>               mOnPublished;
    Signal<void(SkeletonItem&)>               mOnUnpublishing;
    Signal<void(SkeletonItem&, SkeletonMask)> mOnMaskChanged;

    [[nodiscard]] static bool
    matchesMask(const SkeletonMask& itemMask, SkeletonMask required, SkeletonMask excluded) noexcept
    {
        return (itemMask & required) == required && (itemMask & excluded).none();
    }

    template <typename ItemPtr>
    static void sortByBoneId(std::vector<ItemPtr>& v)
    {
        // Benchmarks (benchmark_Skeleton.cpp) show that at N=4096 the sort
        // dominates query cost entirely -- the linear scan is negligible.
        // If query ever becomes a hot path at large N, this sort is the first
        // thing to profile. Possible directions: radix sort on BoneId::value()
        // (fixed-width 64-bit key, no comparison needed), or maintaining a
        // sorted auxiliary structure at publish/unpublish time to avoid
        // sorting at query time altogether.
        std::sort(
            v.begin(),
            v.end(),
            [](ItemPtr a, ItemPtr b)
            {
                return a->boneId() < b->boneId();
            }
        );
    }

    // collectSubtree is const: only reads mRegistry. It returns mutable pointers
    // because const Skeleton does not imply const SkeletonItem -- items own
    // themselves and may be mutated independently of the registry's constness.
    // Non-const callers reach mutable items through the non-const query overloads;
    // this method serves visitSubtree (non-const overload) which needs mutable items
    // without itself being const.
    [[nodiscard]] std::vector<SkeletonItem*> collectSubtree(BoneId root) const
    {
        std::vector<SkeletonItem*> result;
        for (auto it = mRegistry.cbegin(); it != mRegistry.cend(); ++it)
        {
            const BoneId id = it.key();
            SkeletonItem* item = it.value();
            if (id == root || root.isAncestorOf(id))
            {
                result.push_back(item);
            }
        }
        sortByBoneId(result);
        return result;
    }

    // collectSubtreeConst is used by the const visitSubtree overload to
    // ensure that const traversal yields only const SkeletonItem references,
    // consistent with the const query() and find() overloads.
    [[nodiscard]] std::vector<const SkeletonItem*> collectSubtreeConst(BoneId root) const
    {
        std::vector<const SkeletonItem*> result;
        for (auto it = mRegistry.cbegin(); it != mRegistry.cend(); ++it)
        {
            const BoneId id = it.key();
            const SkeletonItem* item = it.value();
            if (id == root || root.isAncestorOf(id))
            {
                result.push_back(item);
            }
        }
        sortByBoneId(result);
        return result;
    }
};

// =============================================================================
// SkeletonItem -- method definitions that depend on Skeleton being complete
// =============================================================================

inline SkeletonItem::~SkeletonItem() noexcept
{
    if (mSkeleton != nullptr)
    {
        // Avoid heap allocation (toString()) in noexcept destructor error path.
        // OOM inside a noexcept context would terminate before the diagnostic fires.
        // Raw value/depth identify the offending item without allocation.
        std::fprintf(
            stderr,
            "[Skeleton] FATAL: SkeletonItem \"%s\" at BoneId(value=0x%llx, depth=%u)"
            " destroyed while still published."
            " Call unpublish() in the most-derived destructor before member teardown."
            " Terminating.\n",
            mName.c_str(),
            static_cast<unsigned long long>(mBoneId.value()),
            static_cast<unsigned>(mBoneId.depth())
        );
        std::terminate();
    }
}

inline void SkeletonItem::publish(Skeleton& skeleton)
{
    if (mSkeleton != nullptr)
    {
        throw PublicationError(
            "Item \"" + std::string(mName) + "\" is already published. "
            "unpublish() must be called before publishing again."
        );
    }
    skeleton.publish(*this); // may throw; leaves mSkeleton == nullptr on throw
}

inline void SkeletonItem::unpublish() noexcept
{
    if (mSkeleton == nullptr)
    {
        return; // no-op
    }
    mSkeleton->unpublish(*this);
    // mSkeleton is cleared by Skeleton::unpublish()
}

inline void SkeletonItem::setMask(SkeletonMask newMask) noexcept
{
    SkeletonMask oldMask = mMask;
    mMask = newMask;
    if (mSkeleton != nullptr)
    {
        mSkeleton->notifyMaskChanged(*this, oldMask);
    }
}

// =============================================================================
// BasicBoneItem -- typed helper base
// =============================================================================

/**
 * @brief Typed helper base that derives from SkeletonItem and binds the
 * compile-time bone identity.
 *
 * @tparam Schema   A HierarchySchema<...> instantiation.
 * @tparam MaskPol  Mask policy tag (DefaultMaskPolicy for v1). Extension point.
 * @tparam Levels   Enum values forming the bone path.
 *
 * BasicBoneItem does NOT call publish() automatically. The most-derived type
 * is always responsible for publication and unpublication timing.
 *
 * Usage:
 * @code
 * class MySensor final
 *     : public BoneItem<SysSchema, System::Root, Subsystem::Sensors, Channel::Load>
 * {
 * public:
 *     using Base = BoneItem<SysSchema, System::Root, Subsystem::Sensors, Channel::Load>;
 *
 *     explicit MySensor(Skeleton& sk)
 *         : Base(makeMask(SkeletonCapability::Sensor,
 *                         SkeletonCapability::ProvidesValue,
 *                         SkeletonCapability::Readable),
 *                "my_sensor")
 *         , mValue(0.0)
 *     {
 *         this->publish(sk);  // All members initialized. Safe.
 *     }
 *     ~MySensor() override { this->unpublish(); }
 *
 * private:
 *     double mValue;
 * };
 * @endcode
 */
template <typename Schema, detail::SkeletonMaskPolicy MaskPol, auto... Levels>
class BasicBoneItem : public SkeletonItem
{
protected:
    using BoneType = Bone<Schema, Levels...>;

    explicit BasicBoneItem(SkeletonMask mask, std::string name = {})
        : SkeletonItem(BoneType::id(), mask, std::move(name))
    {
    }

    // Restrict lifecycle operations to derived-class scope.
    // External callers must not publish or unpublish items directly;
    // the most-derived constructor and destructor own that responsibility.
    using SkeletonItem::publish;
    using SkeletonItem::setMask;
    using SkeletonItem::unpublish;
};

/**
 * @brief Convenience alias for BasicBoneItem with DefaultMaskPolicy.
 *
 * This is the expected base class for the overwhelming majority of items.
 *
 * @code
 * class LoadSensor final
 *     : public BoneItem<SysSchema, System::Root, Subsystem::Sensors, Channel::Load>
 * { ... };
 * @endcode
 */
template <typename Schema, auto... Levels>
using BoneItem = BasicBoneItem<Schema, DefaultMaskPolicy, Levels...>;

} // namespace fat_p::skeleton
