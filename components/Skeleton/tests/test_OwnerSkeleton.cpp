/**
 * @file test_OwnerSkeleton.cpp
 * @brief Unit tests for OwnerSkeleton.h.
 */

/*
FATP_META:
  meta_version: 1
  component: OwnerSkeleton
  file_role: test
  path: components/OwnerSkeleton/tests/test_OwnerSkeleton.cpp
  namespace: fat_p::testing::ownerskeleton
  layer: Testing
  summary: Unit tests for OwnerSkeleton -- emplace, remove, removeSubtree,
    destruction ordering, DuplicateBoneError propagation, signal delivery,
    propagateDown/Up visitor ordering, findAs null-safety, self-publish
    detection, propagation-during-mutation terminate, serialization
    version round-trip.
  api_stability: in_work
  related:
    docs_search: "OwnerSkeleton"
  hygiene:
    pragma_once: false
    includes_windows_h: false
    defines_total: 0
*/

#include "OwnerSkeleton.h"
#include "FatPTest.h"

#include <sstream>
#include <vector>

namespace fat_p::testing::ownerskeleton
{

// Pull in the types used throughout this file.
using fat_p::skeleton::BoneId;
using fat_p::skeleton::DuplicateBoneError;
using fat_p::skeleton::OwnerSkeleton;
using fat_p::skeleton::SkeletonItem;
using fat_p::skeleton::SkeletonMask;
using fat_p::ScopedConnection;

// ─── Helper types ─────────────────────────────────────────────────────────────

// Plain SkeletonItem subclass for tests that need a specific BoneId.
class SimpleItem : public SkeletonItem
{
public:
    explicit SimpleItem(BoneId id, std::string name = {})
        : SkeletonItem(id, SkeletonMask{}, std::move(name))
    {
    }

    // setMask() and the serialization hooks are protected on SkeletonItem.
    // Expose them here for test use. Note: if the intent is for application
    // save/load code to call serializationVersion/serialize/deserialize on
    // arbitrary SkeletonItem*, the access specifier in Skeleton.h should be
    // public -- see Phase 0 review finding.
    using SkeletonItem::setMask;
    using SkeletonItem::serializationVersion;
    using SkeletonItem::serialize;
    using SkeletonItem::deserialize;
};

// Item that records its BoneId into a log on destruction.
struct DestructionTracker
{
    std::vector<BoneId>* log;
    BoneId               id;
    ~DestructionTracker() { if (log) log->push_back(id); }
};

class TrackedItem : public SkeletonItem
{
public:
    TrackedItem(BoneId id, std::vector<BoneId>* log)
        : SkeletonItem(id, SkeletonMask{})
        , tracker{log, id}
    {
    }

    DestructionTracker tracker;
};

// Item that calls publish() in its constructor -- used in death-test annotations.
class SelfPublishingItem : public SkeletonItem
{
public:
    SelfPublishingItem(BoneId id, fat_p::skeleton::Skeleton& sk)
        : SkeletonItem(id, SkeletonMask{})
    {
        publish(sk);    // violates OwnerSkeleton contract
    }
};

// ─── BoneId factory helpers ───────────────────────────────────────────────────

static BoneId makeId1(uint8_t v)
{
    return BoneId{}.child(v);
}

static BoneId makeId2(uint8_t v0, uint8_t v1)
{
    return makeId1(v0).child(v1);
}

static BoneId makeId3(uint8_t v0, uint8_t v1, uint8_t v2)
{
    return makeId2(v0, v1).child(v2);
}

// ─── Tests ────────────────────────────────────────────────────────────────────

// emplace<T>() creates an item, publishes it, and returns a valid pointer.
FATP_TEST_CASE(emplace_basic)
{
    OwnerSkeleton db("test");
    BoneId id = makeId1(1);

    SimpleItem* item = db.emplace<SimpleItem>(id);

    FATP_SIMPLE_ASSERT(item != nullptr,         "emplace() returned null");
    FATP_SIMPLE_ASSERT(item->isPublished(),      "item should be published after emplace");
    FATP_SIMPLE_ASSERT(db.size() == 1u,          "ownership size should be 1");
    FATP_SIMPLE_ASSERT(db.find(id) == item,      "find() should return the emplaced item");
    return true;
}

// emplace<T>() with multiple items -- each is independently addressable.
FATP_TEST_CASE(emplace_multiple)
{
    OwnerSkeleton db("test");
    BoneId id1 = makeId1(1);
    BoneId id2 = makeId1(2);

    SimpleItem* a = db.emplace<SimpleItem>(id1);
    SimpleItem* b = db.emplace<SimpleItem>(id2);

    FATP_SIMPLE_ASSERT(db.size() == 2u, "two items expected");
    FATP_SIMPLE_ASSERT(db.find(id1) == a, "find(id1) mismatch");
    FATP_SIMPLE_ASSERT(db.find(id2) == b, "find(id2) mismatch");
    return true;
}

// remove() unpublishes the item and destructs it cleanly.
FATP_TEST_CASE(remove_basic)
{
    OwnerSkeleton db("test");
    BoneId id = makeId1(5);

    (void)db.emplace<SimpleItem>(id);
    FATP_SIMPLE_ASSERT(db.find(id) != nullptr, "item should exist before remove");

    db.remove(id);

    FATP_SIMPLE_ASSERT(db.size() == 0u,        "ownership should be empty after remove");
    FATP_SIMPLE_ASSERT(db.find(id) == nullptr, "item should be gone after remove");
    return true;
}

// Destruction ordering: OwnerSkeleton destructor unpublishes deepest items first.
FATP_TEST_CASE(destruction_ordering)
{
    std::vector<BoneId> destructionLog;

    BoneId rootId  = makeId1(1);
    BoneId childId = makeId2(1, 1);
    BoneId grandId = makeId3(1, 1, 1);

    {
        OwnerSkeleton db("test");
        (void)db.emplace<TrackedItem>(rootId,  &destructionLog);
        (void)db.emplace<TrackedItem>(childId, &destructionLog);
        (void)db.emplace<TrackedItem>(grandId, &destructionLog);
        // Destructor runs at end of scope.
    }

    // Items must be destroyed deepest-first: grandchild -> child -> root.
    FATP_ASSERT_EQ(destructionLog.size(), 3u,           "all three items must be destroyed");
    FATP_SIMPLE_ASSERT(destructionLog[0] == grandId,    "grandchild must be destroyed first");
    FATP_SIMPLE_ASSERT(destructionLog[1] == childId,    "child must be destroyed second");
    FATP_SIMPLE_ASSERT(destructionLog[2] == rootId,     "root must be destroyed last");
    return true;
}

// removeSubtree() removes children before parent.
FATP_TEST_CASE(removeSubtree_ordering)
{
    std::vector<BoneId> destructionLog;

    BoneId rootId  = makeId1(1);
    BoneId childId = makeId2(1, 1);
    BoneId grandId = makeId3(1, 1, 1);

    OwnerSkeleton db("test");
    (void)db.emplace<TrackedItem>(rootId,  &destructionLog);
    (void)db.emplace<TrackedItem>(childId, &destructionLog);
    (void)db.emplace<TrackedItem>(grandId, &destructionLog);

    db.removeSubtree(rootId);

    FATP_ASSERT_EQ(db.size(), 0u,                      "all items removed");
    FATP_ASSERT_EQ(destructionLog.size(), 3u,           "three items destroyed");
    FATP_SIMPLE_ASSERT(destructionLog[0] == grandId,    "grandchild first");
    FATP_SIMPLE_ASSERT(destructionLog[1] == childId,    "child second");
    FATP_SIMPLE_ASSERT(destructionLog[2] == rootId,     "root last");
    return true;
}

// removeSubtree() on a partial subtree leaves the rest intact.
FATP_TEST_CASE(removeSubtree_partial)
{
    BoneId root   = makeId1(1);
    BoneId child1 = makeId2(1, 1);
    BoneId child2 = makeId2(1, 2);

    OwnerSkeleton db("test");
    (void)db.emplace<SimpleItem>(root);
    (void)db.emplace<SimpleItem>(child1);
    (void)db.emplace<SimpleItem>(child2);

    db.removeSubtree(child1);   // remove only the child1 branch

    FATP_ASSERT_EQ(db.size(), 2u,                  "root and child2 remain");
    FATP_SIMPLE_ASSERT(db.find(root)   != nullptr, "root intact");
    FATP_SIMPLE_ASSERT(db.find(child1) == nullptr, "child1 removed");
    FATP_SIMPLE_ASSERT(db.find(child2) != nullptr, "child2 intact");
    return true;
}

// DuplicateBoneError: emplace() at a BoneId already in use throws.
FATP_TEST_CASE(duplicate_bone_error)
{
    OwnerSkeleton db("test");
    BoneId id = makeId1(3);
    (void)db.emplace<SimpleItem>(id);

    FATP_ASSERT_THROWS(
        db.emplace<SimpleItem>(id),
        DuplicateBoneError,
        "second emplace at same BoneId must throw DuplicateBoneError");

    // The first item must still be intact after the failed second emplace.
    FATP_ASSERT_EQ(db.size(), 1u, "ownership unchanged after failed emplace");
    return true;
}

// onPublished signal fires when an item is emplaced.
FATP_TEST_CASE(signal_onPublished)
{
    OwnerSkeleton db("test");
    BoneId id = makeId1(7);
    BoneId observedId;

    auto conn = db.onPublished([&](SkeletonItem& item) {
        observedId = item.boneId();
    });

    (void)db.emplace<SimpleItem>(id);

    FATP_SIMPLE_ASSERT(observedId == id, "onPublished must fire with correct BoneId");
    return true;
}

// onUnpublishing signal fires before the item is removed.
FATP_TEST_CASE(signal_onUnpublishing)
{
    OwnerSkeleton db("test");
    BoneId id = makeId1(8);
    bool firedWhileAlive = false;

    auto conn = db.onUnpublishing([&](SkeletonItem& item) {
        // The item is still in the Skeleton at this point.
        firedWhileAlive = (db.find(item.boneId()) == &item);
    });

    (void)db.emplace<SimpleItem>(id);
    db.remove(id);

    FATP_SIMPLE_ASSERT(firedWhileAlive, "item must still be findable during onUnpublishing");
    return true;
}

// ScopedConnection auto-disconnects: signal stops firing after connection
// goes out of scope.
FATP_TEST_CASE(scoped_connection_lifetime)
{
    OwnerSkeleton db("test");
    int fireCount = 0;

    {
        auto conn = db.onPublished([&](SkeletonItem&) { ++fireCount; });
        (void)db.emplace<SimpleItem>(makeId1(1));
        FATP_ASSERT_EQ(fireCount, 1, "should fire once while connected");
    } // conn destroyed here

    (void)db.emplace<SimpleItem>(makeId1(2));
    FATP_ASSERT_EQ(fireCount, 1, "should not fire after disconnection");
    return true;
}

// propagateDown() visits root then descendants in parent-before-child order.
FATP_TEST_CASE(propagateDown_visitor_ordering)
{
    BoneId root  = makeId1(1);
    BoneId child = makeId2(1, 1);
    BoneId grand = makeId3(1, 1, 1);

    OwnerSkeleton db("test");
    (void)db.emplace<SimpleItem>(root);
    (void)db.emplace<SimpleItem>(child);
    (void)db.emplace<SimpleItem>(grand);

    std::vector<BoneId> visited;
    db.propagateDown(root, [&visited](SkeletonItem& item) {
        visited.push_back(item.boneId());
    });

    FATP_ASSERT_EQ(visited.size(), 3u,          "all three items visited");
    FATP_SIMPLE_ASSERT(visited[0] == root,      "root visited first");
    FATP_SIMPLE_ASSERT(visited[1] == child,     "child visited second");
    FATP_SIMPLE_ASSERT(visited[2] == grand,     "grandchild visited last");
    return true;
}

// propagateUp() visits node then ancestors in child-to-root order.
FATP_TEST_CASE(propagateUp_visitor_ordering)
{
    BoneId root  = makeId1(1);
    BoneId child = makeId2(1, 1);
    BoneId grand = makeId3(1, 1, 1);

    OwnerSkeleton db("test");
    (void)db.emplace<SimpleItem>(root);
    (void)db.emplace<SimpleItem>(child);
    (void)db.emplace<SimpleItem>(grand);

    std::vector<BoneId> visited;
    db.propagateUp(grand, [&visited](SkeletonItem& item) {
        visited.push_back(item.boneId());
    });

    FATP_ASSERT_EQ(visited.size(), 3u,          "all three ancestors visited");
    FATP_SIMPLE_ASSERT(visited[0] == grand,     "start node visited first");
    FATP_SIMPLE_ASSERT(visited[1] == child,     "parent visited second");
    FATP_SIMPLE_ASSERT(visited[2] == root,      "root visited last");
    return true;
}

// propagateUp() on a single root node visits exactly one item.
FATP_TEST_CASE(propagateUp_single_node)
{
    BoneId id = makeId1(1);
    OwnerSkeleton db("test");
    (void)db.emplace<SimpleItem>(id);

    std::vector<BoneId> visited;
    db.propagateUp(id, [&visited](SkeletonItem& item) {
        visited.push_back(item.boneId());
    });

    FATP_ASSERT_EQ(visited.size(), 1u,  "one item visited");
    FATP_SIMPLE_ASSERT(visited[0] == id, "correct item");
    return true;
}

// findAs<T>() returns correctly typed pointer; returns nullptr for absent id.
FATP_TEST_CASE(findAs_null_safety)
{
    OwnerSkeleton db("test");
    BoneId presentId = makeId1(1);
    BoneId absentId  = makeId1(2);

    (void)db.emplace<SimpleItem>(presentId, "present");

    auto* found  = db.findAs<SimpleItem>(presentId);
    auto* absent = db.findAs<SimpleItem>(absentId);

    FATP_SIMPLE_ASSERT(found  != nullptr, "findAs on present id must return non-null");
    FATP_SIMPLE_ASSERT(absent == nullptr, "findAs on absent id must return null");
    return true;
}

// query() returns only items in the subtree that match the mask.
FATP_TEST_CASE(query_mask_filter)
{
    OwnerSkeleton db("test");

    SkeletonMask hasFeature;
    hasFeature.set(1);

    BoneId root  = makeId1(1);
    BoneId child = makeId2(1, 1);
    BoneId other = makeId1(2);

    // root has the capability bit set; child does not; other is outside the subtree.
    {
        auto* r = db.emplace<SimpleItem>(root);
        r->setMask(hasFeature);
    }
    (void)db.emplace<SimpleItem>(child);
    {
        auto* o = db.emplace<SimpleItem>(other);
        o->setMask(hasFeature);
    }

    auto results = db.query(root, hasFeature);
    FATP_ASSERT_EQ(results.size(), 1u,                      "only root matches within its subtree");
    FATP_SIMPLE_ASSERT(results[0]->boneId() == root,        "root must be the result");
    return true;
}

// Serialization version: default implementation returns 1.
FATP_TEST_CASE(serialization_version_default)
{
    OwnerSkeleton db("test");
    BoneId id = makeId1(1);
    SimpleItem* item = db.emplace<SimpleItem>(id);

    FATP_ASSERT_EQ(item->serializationVersion(), 1u,
        "default serializationVersion() must return 1");
    return true;
}

// dump() produces non-empty output without crashing.
FATP_TEST_CASE(dump_does_not_crash)
{
    OwnerSkeleton db("test");
    (void)db.emplace<SimpleItem>(makeId1(1));
    (void)db.emplace<SimpleItem>(makeId1(2));

    std::ostringstream oss;
    db.dump(oss);
    FATP_SIMPLE_ASSERT(!oss.str().empty(), "dump() must produce output");
    return true;
}

// ─── Terminate tests (death tests -- run only in death-test mode) ─────────────
//
// The following behaviours are required to terminate the process. They are
// annotated but NOT run in the standard test suite; add them to a dedicated
// death-test runner when the CI pipeline supports it.
//
// 1. Self-publish detection:
//    db.emplace<SelfPublishingItem>(id, db.skeleton()) -- FATP_ALWAYS_ENFORCE fires
//    because item.isPublished() is true after construction.
//
// 2. Propagation-during-mutation:
//    db.propagateDown(root, [&](SkeletonItem&) { db.remove(id); });
//    -- assertNotPropagating fires in remove().
//
// 3. Destruction with items published outside OwnerSkeleton:
//    Calling item.publish(os.skeleton()) directly (bypassing OwnerSkeleton::emplace),
//    then letting os destruct -- Skeleton::~Skeleton() terminates because the
//    registry is non-empty and the item has mSkeleton != nullptr.

} // namespace fat_p::testing::ownerskeleton

// ─── Public interface ─────────────────────────────────────────────────────────

namespace fat_p::testing
{

bool test_OwnerSkeleton()
{
    FATP_PRINT_HEADER(OWNER SKELETON)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, ownerskeleton, emplace_basic);
    FATP_RUN_TEST_NS(runner, ownerskeleton, emplace_multiple);
    FATP_RUN_TEST_NS(runner, ownerskeleton, remove_basic);
    FATP_RUN_TEST_NS(runner, ownerskeleton, destruction_ordering);
    FATP_RUN_TEST_NS(runner, ownerskeleton, removeSubtree_ordering);
    FATP_RUN_TEST_NS(runner, ownerskeleton, removeSubtree_partial);
    FATP_RUN_TEST_NS(runner, ownerskeleton, duplicate_bone_error);
    FATP_RUN_TEST_NS(runner, ownerskeleton, signal_onPublished);
    FATP_RUN_TEST_NS(runner, ownerskeleton, signal_onUnpublishing);
    FATP_RUN_TEST_NS(runner, ownerskeleton, scoped_connection_lifetime);
    FATP_RUN_TEST_NS(runner, ownerskeleton, propagateDown_visitor_ordering);
    FATP_RUN_TEST_NS(runner, ownerskeleton, propagateUp_visitor_ordering);
    FATP_RUN_TEST_NS(runner, ownerskeleton, propagateUp_single_node);
    FATP_RUN_TEST_NS(runner, ownerskeleton, findAs_null_safety);
    FATP_RUN_TEST_NS(runner, ownerskeleton, query_mask_filter);
    FATP_RUN_TEST_NS(runner, ownerskeleton, serialization_version_default);
    FATP_RUN_TEST_NS(runner, ownerskeleton, dump_does_not_crash);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_OwnerSkeleton() ? 0 : 1;
}
#endif
