#pragma once
/*
FATP_META:
  meta_version: 1
  component: OwnerSkeleton
  file_role: public_header
  path: include/fat_p/OwnerSkeleton.h
  namespace: fat_p::skeleton
  layer: Domain
  summary: >
    Owning extension of Skeleton. Combines FastHashMap<BoneId,
    unique_ptr<SkeletonItem>> ownership with Skeleton addressing and
    signals. Items emplaced here must not call publish() themselves;
    use OwnedBoneItem<> base to enforce this at compile time.
  api_stability: in_work
  related:
    docs_search: "OwnerSkeleton Skeleton"
    tests:
      - components/OwnerSkeleton/tests/test_OwnerSkeleton.cpp
  hygiene:
    pragma_once: true
    includes_windows_h: false
    defines_total: 0
*/

/**
 * @file OwnerSkeleton.h
 * @brief Owning hierarchical item registry.
 *
 * @details
 * OwnerSkeleton extends Skeleton with ownership. It holds a parallel
 * FastHashMap<BoneId, unique_ptr<SkeletonItem>> alongside the inner Skeleton's
 * non-owning registry. The two maps are always in sync: the Skeleton never sees
 * an item that OwnerSkeleton does not own, and OwnerSkeleton never owns an item
 * that is not published on the inner Skeleton.
 *
 * Consumers that only need lookup, traversal, and signals receive a Skeleton&
 * via skeleton(). Only the component responsible for the full object graph holds
 * the OwnerSkeleton.
 *
 * Message passing:
 * - BoneId encodes the full path from the tree root to any node. This makes
 *   the hierarchy itself the message routing mechanism -- no separate event bus,
 *   no manual parent pointers, and no observer registration are required.
 * - propagateUp() is the standard pattern for child-to-parent notification:
 *   a child that changes (e.g. a windowless control whose size changes) calls
 *   propagateUp() and each ancestor receives the visitor in child-to-root order.
 *   The first ancestor that owns the response (e.g. a panel that triggers a
 *   redraw) handles it; deeper ancestors ignore it or accumulate it.
 * - propagateDown() is the standard pattern for parent-to-child broadcast:
 *   a state change at a parent node (e.g. a lock or visibility change) fans out
 *   to all descendants in parent-before-child order.
 * - visitSubtree() supports read-only queries over a subtree (e.g. collecting
 *   all dirty controls under a panel before a redraw pass).
 * - The ordering guarantee comes from BoneId::operator<: ascending sort always
 *   places every parent before its children, across the entire registry.
 *   Reverse iteration gives leaf-first order. No auxiliary structures needed.
 *
 * Lifecycle contract:
 * - Items emplaced into OwnerSkeleton must NOT call publish() in their
 *   constructors. OwnerSkeleton calls publish() after construction completes.
 *   Use OwnedBoneItem<Schema, Levels...> as the base for managed items to
 *   enforce this at compile time.
 * - Items managed by OwnerSkeleton must not call unpublish() themselves.
 *   OwnerSkeleton owns both ends of the lifecycle.
 * - Visitor callbacks passed to propagateDown() or propagateUp() must not
 *   call emplace(), remove(), or removeSubtree() on the same OwnerSkeleton.
 *   Mutations during propagation terminate the process.
 *
 * Serialization hooks:
 * - SkeletonItem gains virtual serialize(), deserialize(), and
 *   serializationVersion() methods (see Skeleton.h, Phase 0 delivery).
 *   Default implementations are no-ops. OwnerSkeleton itself provides no
 *   save()/load(); that is the application's responsibility.
 *
 * Requirements:
 * - C++20
 * - fat_p headers: Skeleton.h, FastHashMap.h, ScopeGuard.h, enforce.h
 *
 * @see Skeleton.h for BoneId, SkeletonItem, SkeletonMask, Skeleton,
 *      SerializeWriter, SerializeReader.
 */

#include <algorithm>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "enforce.h"
#include "ScopeGuard.h"
#include "FastHashMap.h"
#include "Skeleton.h"

// SerializeWriter and SerializeReader are forward-declared in Skeleton.h
// (Phase 0 delivery). No re-declaration here.

namespace fat_p::skeleton {

// ─── OwnedBoneItem ────────────────────────────────────────────────────────────

/**
 * @brief Recommended base for items managed by OwnerSkeleton.
 *
 * @details
 * Inherits BoneItem<Schema, Levels...> and deletes publish() and unpublish()
 * in derived-class scope. This provides compile-time enforcement of the
 * no-self-publish rule required by OwnerSkeleton.
 *
 * Items that do not use this base must not call publish() or unpublish()
 * themselves when managed by an OwnerSkeleton. The runtime FATP_ALWAYS_ENFORCE
 * in emplace() will catch violations in debug and release builds, but
 * OwnedBoneItem closes the issue at compile time.
 *
 * @tparam Schema   HierarchySchema type (see SkeletonFwd.h).
 * @tparam Levels   Compile-time level values identifying the bone address.
 */
template <typename Schema, auto... Levels>
class OwnedBoneItem : public BoneItem<Schema, Levels...>
{
public:
    using BoneItem<Schema, Levels...>::BoneItem;

    // Deleted: OwnerSkeleton manages the full publish/unpublish lifecycle.
    void publish(Skeleton&)   = delete;
    void unpublish() noexcept = delete;
};

// ─── OwnerSkeleton ────────────────────────────────────────────────────────────

/**
 * @brief Owning extension of Skeleton.
 *
 * @details
 * Holds a FastHashMap<BoneId, unique_ptr<SkeletonItem>> in sync with an inner
 * Skeleton. Provides emplace<T>() and remove() / removeSubtree() as the sole
 * lifecycle interface. Propagation, signal subscriptions, and bulk queries are
 * layered on top. Non-owning consumers receive Skeleton& via skeleton().
 */
class OwnerSkeleton
{
public:
    explicit OwnerSkeleton(std::string name = "default");
    ~OwnerSkeleton() noexcept;

    OwnerSkeleton(const OwnerSkeleton&)            = delete;
    OwnerSkeleton& operator=(const OwnerSkeleton&) = delete;

    // ── Creation ──────────────────────────────────────────────────────────────

    /**
     * @brief Constructs T in-place and publishes it onto the inner Skeleton.
     *
     * @details
     * T must NOT call publish() in its constructor. Prefer OwnedBoneItem<> as
     * the base to enforce this at compile time. Ownership is established before
     * publication; onPublished callbacks run with the item already visible in
     * mOwnership for a consistent view.
     *
     * Returns T* (not T&): storing a reference across a remove() call on the
     * same BoneId is a dangling-reference bug.
     *
     * A ScopeGuardOnFail rolls back mOwnership insertion if publish() throws;
     * the item is left in a clean unpublished state for the unique_ptr
     * destructor.
     *
     * @throws DuplicateBoneError if another item at the same BoneId is already
     *         owned by this OwnerSkeleton. Thrown before T is constructed, so
     *         the existing item is never disturbed.
     * @throws Any exception thrown by T's constructor.
     */
    template <typename T, typename... Args>
    [[nodiscard]] T* emplace(Args&&... args);

    // ── Removal ───────────────────────────────────────────────────────────────

    /**
     * @brief Unpublishes and destroys the item at @p id.
     *
     * @details
     * onUnpublishing fires on the inner Skeleton before removal, so observers
     * have one last consistent view with the item still alive. The item is
     * then erased from the Skeleton registry (item.mSkeleton cleared) and from
     * mOwnership (unique_ptr destructor fires with mSkeleton == nullptr;
     * no terminate).
     *
     * @pre @p id must be owned by this OwnerSkeleton. Terminates on violation.
     * @pre Must not be called from within a propagateDown() or propagateUp()
     *      visitor callback. Terminates on violation.
     */
    void remove(BoneId id);

    /**
     * @brief Removes the subtree rooted at @p root, deepest descendants first.
     *
     * @details
     * Collects all items in the subtree, sorts by BoneId deepest-first, and
     * calls remove() on each. Leaves are always removed before parents.
     *
     * Calling remove() on a node that still has published descendants (instead
     * of removeSubtree()) leaves orphans in the Skeleton registry. Use
     * removeSubtree() for branch removal.
     *
     * @pre @p root must be owned by this OwnerSkeleton. Terminates on violation.
     * @pre Must not be called from within a propagation visitor. Terminates.
     */
    void removeSubtree(BoneId root);

    // ── Lookup ────────────────────────────────────────────────────────────────

    [[nodiscard]] SkeletonItem*       find(BoneId id) noexcept;
    [[nodiscard]] const SkeletonItem* find(BoneId id) const noexcept;

    /**
     * @brief Returns static_cast<T*>(find(id)), or nullptr if absent.
     *
     * @details
     * No dynamic_cast; the caller asserts the concrete type. Returns nullptr
     * if no item is published at @p id.
     */
    template <typename T>
    [[nodiscard]] T* findAs(BoneId id) noexcept;

    /**
     * @brief Returns items in the subtree rooted at @p root satisfying the
     *        capability masks.
     *
     * @details
     * Forwards to Skeleton::querySubtree(). Returns items whose mask has all
     * bits in @p required set and no bits in @p excluded set.
     *
     * @note Not const: delegates to the non-const Skeleton overload to return
     *       mutable SkeletonItem pointers.
     */
    [[nodiscard]] std::vector<SkeletonItem*> query(BoneId       root,
                                                    SkeletonMask required,
                                                    SkeletonMask excluded = {});

    /**
     * @brief Returns all owned items in ascending BoneId order (parent-before-child).
     *
     * Used by save orchestration (ActiveDatabase::save) to iterate items in
     * hierarchy order. Same ordering used internally by the destructor.
     */
    [[nodiscard]] std::vector<SkeletonItem*> sortedItems();

    /// @copydoc sortedItems()
    [[nodiscard]] std::vector<const SkeletonItem*> sortedItems() const;

    // ── Traversal ─────────────────────────────────────────────────────────────

    /**
     * @brief Calls @p visitor for each item in the subtree rooted at @p root.
     *
     * @details
     * Forwards to Skeleton::visitSubtree(). Mutations (emplace, remove,
     * removeSubtree) on this OwnerSkeleton are forbidden inside @p visitor;
     * they terminate the process via the Skeleton's mTraversalDepth guard.
     */
    void visitSubtree(BoneId root, std::function<void(SkeletonItem&)> visitor);

    /**
     * @brief Calls @p visitor for @p root and every descendant, parents first.
     *
     * @details
     * Implemented over Skeleton::visitSubtree(); the Skeleton's mTraversalDepth
     * guard is active. Mutations (emplace, remove, removeSubtree) on this
     * OwnerSkeleton inside @p visitor terminate the process.
     */
    void propagateDown(BoneId root, std::function<void(SkeletonItem&)> visitor);

    /**
     * @brief Calls @p visitor for @p node, its parent, grandparent, up to the
     *        domain root.
     *
     * @details
     * Walks the ancestor chain by stripping one BoneId depth level per step.
     * Items not found in the Skeleton are skipped silently (consistent with
     * Skeleton::find() semantics). OwnerSkeleton's mPropagationDepth guard is
     * active; mutations via emplace(), remove(), or removeSubtree() terminate
     * the process. Note: Skeleton's mTraversalDepth guard is NOT active during
     * propagateUp() -- direct calls to item.publish() or item.unpublish() on
     * the inner Skeleton are not caught by the Skeleton layer.
     */
    void propagateUp(BoneId node, std::function<void(SkeletonItem&)> visitor);

    // ── Signal subscriptions ──────────────────────────────────────────────────

    /**
     * @brief Subscribes to the inner Skeleton's onPublished signal.
     *
     * @return A ScopedConnection that auto-disconnects on destruction.
     */
    template <typename Fn>
    [[nodiscard]] ScopedConnection onPublished(Fn&& fn, int priority = 0);

    /// @copydoc onPublished
    template <typename Fn>
    [[nodiscard]] ScopedConnection onUnpublishing(Fn&& fn, int priority = 0);

    /// @copydoc onPublished
    template <typename Fn>
    [[nodiscard]] ScopedConnection onMaskChanged(Fn&& fn, int priority = 0);

    // ── Access to inner Skeleton ──────────────────────────────────────────────

    /// Returns a non-owning reference to the inner Skeleton for consumers that
    /// only need lookup and signals.
    [[nodiscard]] Skeleton&       skeleton() noexcept       { return mSkeleton; }
    [[nodiscard]] const Skeleton& skeleton() const noexcept { return mSkeleton; }

    [[nodiscard]] std::string_view name() const noexcept    { return mName; }
    [[nodiscard]] std::size_t      size() const noexcept    { return mOwnership.size(); }

    /// Dumps the inner Skeleton registry to @p out.
    void dump(std::ostream& out) const;

private:
    std::string mName;
    Skeleton    mSkeleton;
    FastHashMap<BoneId, std::unique_ptr<SkeletonItem>> mOwnership;
    uint32_t    mPropagationDepth{0};   // mutation guard during propagateDown/Up

    void assertNotPropagating(const char* ctx) const noexcept;
};

// ─── OwnerSkeleton Implementation ─────────────────────────────────────────────

inline OwnerSkeleton::OwnerSkeleton(std::string name)
    : mName(std::move(name))
    , mSkeleton(mName)
{
}

inline OwnerSkeleton::~OwnerSkeleton() noexcept
{
    // Collect all items and sort deepest BoneId first (child-before-parent).
    // BoneId::operator< is the Skeleton's own ordering: ascending sort places
    // every parent before its children. Iterating in reverse gives leaf-first
    // destruction -- the same invariant BE enforces via SI_STORAGE::OnDestruction.
    // Member declaration order alone cannot save us:
    // - mOwnership first: destructors fire with mSkeleton != nullptr -> terminate.
    // - mSkeleton first: Skeleton::~Skeleton() sees a non-empty registry -> terminate.
    // The explicit body uses the Skeleton's ordering to unpublish and erase each
    // item individually in leaf-first order. Both maps are empty before any
    // member destructor runs.

    std::vector<SkeletonItem*> items;
    items.reserve(mOwnership.size());

    for (auto it = mOwnership.begin(); it != mOwnership.end(); ++it)
        items.push_back(it.value().get());

    std::sort(items.begin(), items.end(),
              [](const SkeletonItem* a, const SkeletonItem* b) noexcept
              {
                  return a->boneId() < b->boneId(); // ascending: parent before child
              });

    for (auto it = items.rbegin(); it != items.rend(); ++it)
    {
        const BoneId id = (*it)->boneId();
        (*it)->unpublish();     // clears item.mSkeleton
        mOwnership.erase(id);  // unique_ptr destructs here; mSkeleton == nullptr; no terminate
    }

    // Both maps are now empty. mSkeleton and mOwnership destruct cleanly.
}

template <typename T, typename... Args>
T* OwnerSkeleton::emplace(Args&&... args)
{
    assertNotPropagating("emplace()");

    // Check for duplicate ownership before constructing T. Throwing here means
    // no object is ever constructed for a duplicate id -- matches the contract
    // that the first item at that BoneId is never disturbed.
    auto placeholder = std::make_unique<T>(std::forward<Args>(args)...);
    T*     raw = placeholder.get();
    BoneId id  = raw->boneId();

    if (mOwnership.contains(id))
    {
        throw DuplicateBoneError(
            "OwnerSkeleton::emplace(): item at " + id.toString() +
            " is already owned by this OwnerSkeleton.");
    }

    FATP_ALWAYS_ENFORCE(
        !raw->isPublished(),
        "OwnerSkeleton::emplace(): item called publish() in its constructor. "
        "Use OwnedBoneItem<> base or remove the publish() call.");

    // Ownership before publication: onPublished observers see a consistent view.
    // contains() passed above so insert() will succeed; the return value is not
    // checked here.
    mOwnership.insert(id, std::move(placeholder));

    // Rollback on any exception escaping publish(). insert() succeeded above,
    // so erasing id here is safe. Skeleton::publish() already rolls back its
    // own registry entry and clears item.mSkeleton, so the unique_ptr destructor
    // fires with mSkeleton == nullptr; no terminate.
    auto rollback = makeScopeGuardOnFail([&]() noexcept {
        mOwnership.erase(id);
    });

    raw->publish(mSkeleton);    // public on SkeletonItem; fires onPublished signal

    return raw;                 // T* not T&: caller must not store past remove(id)
}

inline void OwnerSkeleton::remove(BoneId id)
{
    assertNotPropagating("remove()");

    auto* upptr = mOwnership.find(id);
    FATP_ALWAYS_ENFORCE(upptr != nullptr,
        "OwnerSkeleton::remove(): BoneId not found in ownership map.");

    upptr->get()->unpublish();  // fires onUnpublishing; clears item.mSkeleton
    mOwnership.erase(id);       // unique_ptr destructs; item.mSkeleton == nullptr; no terminate
}

inline void OwnerSkeleton::removeSubtree(BoneId root)
{
    assertNotPropagating("removeSubtree()");

    // Collect all BoneIds in the subtree.
    std::vector<BoneId> ids;
    mSkeleton.visitSubtree(root, [&ids](SkeletonItem& item) {
        ids.push_back(item.boneId());
    });

    // Sort deepest BoneId first (children before parents).
    std::sort(ids.begin(), ids.end(),
              [](const BoneId& a, const BoneId& b) noexcept {
                  return b < a;
              });

    for (const BoneId& id : ids)
        remove(id);
}

[[nodiscard]] inline SkeletonItem* OwnerSkeleton::find(BoneId id) noexcept
{
    return mSkeleton.find(id);
}

[[nodiscard]] inline const SkeletonItem* OwnerSkeleton::find(BoneId id) const noexcept
{
    return mSkeleton.find(id);
}

template <typename T>
T* OwnerSkeleton::findAs(BoneId id) noexcept
{
    return static_cast<T*>(mSkeleton.find(id));
}

[[nodiscard]] inline
std::vector<SkeletonItem*> OwnerSkeleton::query(BoneId       root,
                                                 SkeletonMask required,
                                                 SkeletonMask excluded)
{
    return mSkeleton.querySubtree(root, required, excluded);
}

// Same collect-and-sort pattern used by the destructor, but without
// unpublish/erase — this is a read-only snapshot for save orchestration.

[[nodiscard]] inline
std::vector<SkeletonItem*> OwnerSkeleton::sortedItems()
{
    std::vector<SkeletonItem*> items;
    items.reserve(mOwnership.size());

    for (auto it = mOwnership.begin(); it != mOwnership.end(); ++it)
    {
        items.push_back(it.value().get());
    }

    std::sort(items.begin(), items.end(),
              [](const SkeletonItem* a, const SkeletonItem* b) noexcept
              {
                  return a->boneId() < b->boneId();
              });

    return items;
}

[[nodiscard]] inline
std::vector<const SkeletonItem*> OwnerSkeleton::sortedItems() const
{
    std::vector<const SkeletonItem*> items;
    items.reserve(mOwnership.size());

    for (auto it = mOwnership.cbegin(); it != mOwnership.cend(); ++it)
    {
        items.push_back(it.value().get());
    }

    std::sort(items.begin(), items.end(),
              [](const SkeletonItem* a, const SkeletonItem* b) noexcept
              {
                  return a->boneId() < b->boneId();
              });

    return items;
}

inline void OwnerSkeleton::visitSubtree(BoneId root,
                                         std::function<void(SkeletonItem&)> visitor)
{
    mSkeleton.visitSubtree(root, std::move(visitor));
}

inline void OwnerSkeleton::propagateDown(BoneId root,
                                          std::function<void(SkeletonItem&)> visitor)
{
    ++mPropagationDepth;
    auto guard = makeScopeGuard([this]() noexcept { --mPropagationDepth; });
    // visitSubtree activates Skeleton's mTraversalDepth guard, which catches
    // any publish/unpublish call (and therefore any emplace/remove) that
    // escapes our own mPropagationDepth check via a direct SkeletonItem call.
    mSkeleton.visitSubtree(root, std::move(visitor));
}

inline void OwnerSkeleton::propagateUp(BoneId node,
                                        std::function<void(SkeletonItem&)> visitor)
{
    ++mPropagationDepth;
    auto guard = makeScopeGuard([this]() noexcept { --mPropagationDepth; });

    // Walk the ancestor chain. Nodes absent from the Skeleton are skipped;
    // this is consistent with find() semantics and handles gaps in the tree.
    // Note: Skeleton's mTraversalDepth guard is NOT active here (no visitSubtree
    // call). OwnerSkeleton's mPropagationDepth guard (assertNotPropagating)
    // covers emplace/remove/removeSubtree. Direct calls to item.publish() or
    // item.unpublish() on the inner Skeleton are not intercepted.
    BoneId current = node;
    while (!current.isNull())
    {
        if (SkeletonItem* item = mSkeleton.find(current))
            visitor(*item);
        current = current.parent();
    }
}

template <typename Fn>
ScopedConnection OwnerSkeleton::onPublished(Fn&& fn, int priority)
{
    return mSkeleton.onPublished(std::forward<Fn>(fn), priority);
}

template <typename Fn>
ScopedConnection OwnerSkeleton::onUnpublishing(Fn&& fn, int priority)
{
    return mSkeleton.onUnpublishing(std::forward<Fn>(fn), priority);
}

template <typename Fn>
ScopedConnection OwnerSkeleton::onMaskChanged(Fn&& fn, int priority)
{
    return mSkeleton.onMaskChanged(std::forward<Fn>(fn), priority);
}

inline void OwnerSkeleton::dump(std::ostream& out) const
{
    mSkeleton.dump(out);
}

inline void OwnerSkeleton::assertNotPropagating(const char* ctx) const noexcept
{
    FATP_ALWAYS_ENFORCE(
        mPropagationDepth == 0,
        "OwnerSkeleton::%s called during propagation. "
        "Queue mutations and apply after propagation returns.",
        ctx);
}

} // namespace fat_p::skeleton
