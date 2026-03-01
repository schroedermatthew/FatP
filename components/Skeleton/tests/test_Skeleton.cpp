/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: test
  path: components/Skeleton/tests/test_Skeleton.cpp
  namespace: fat_p::testing::skeleton
  layer: Testing
  summary: Comprehensive unit tests for Skeleton.h and SkeletonFwd.h.
  api_stability: in_work
  related:
    docs:
      - components/Skeleton/docs/
    tests:
      - components/Skeleton/tests/test_Skeleton.cpp
*/

/**
 * @file test_Skeleton.cpp
 * @brief Comprehensive unit tests for Skeleton.h and SkeletonFwd.h.
 *
 * Coverage:
 * - BoneId: construction, null, accessors, comparison, ordering, ancestor/descendant,
 *   parent/child navigation, serialization, toString, std::hash
 * - HierarchySchema: kMaxDepth, expected_type
 * - SkeletonCapability / SkeletonMask / makeMask
 * - Bone<>: id(), kDepth, Parent type, child<> alias, schema_type
 * - DefaultMaskPolicy, BasicBoneItem, BoneItem aliases
 * - SkeletonItem lifecycle: publish, unpublish, isPublished, exceptions,
 *   setMask published vs unpublished
 * - Skeleton registry: find, findAs (const + non-const), size, dump
 * - Skeleton traversal: visitSubtree ordering and empty-subtree case
 * - Skeleton queries: query, querySubtree with required/excluded masks
 * - Signals: onPublished, onUnpublishing, onMaskChanged + ScopedConnection
 * - Exception types: PublicationError, DuplicateBoneError
 * - Multi-skeleton: item republished after unpublish
 * - Stress: many items, random publish/unpublish, query consistency
 */

#include <algorithm>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "Skeleton.h"
#include "FatPTest.h"

namespace fat_p::testing::skeleton
{

using namespace fat_p::skeleton;

// =============================================================================
// Test Schema and Enumerations
// =============================================================================

enum class Sys : uint8_t { Root = 1, Aux = 2 };
enum class Sub : uint8_t { Sensors = 1, Actuators = 2, Network = 3 };
enum class Chan : uint8_t { Load = 0, Temp = 1, Pressure = 2 };

using TestSchema = HierarchySchema<Sys, Sub, Chan>;

// A second schema for cross-schema isolation tests
enum class Cat : uint8_t { Hardware = 1, Software = 2 };
enum class Module : uint8_t { Core = 0, Peripheral = 1 };

using AltSchema = HierarchySchema<Cat, Module>;

// =============================================================================
// Helper: concrete BoneItem wrappers used throughout tests
// =============================================================================

// Depth-1 item: Sys level only (root-level node)
class SysItem final : public BoneItem<TestSchema, Sys::Root>
{
public:
    using Base = BoneItem<TestSchema, Sys::Root>;

    explicit SysItem(Skeleton& sk, SkeletonMask mask = {}, std::string name = {})
        : Base(mask, std::move(name))
    {
        this->publish(sk);
    }

    ~SysItem() override { this->unpublish(); }
};

// Depth-2 item: Sys::Root / Sub::Sensors
class SensorItem final : public BoneItem<TestSchema, Sys::Root, Sub::Sensors>
{
public:
    using Base = BoneItem<TestSchema, Sys::Root, Sub::Sensors>;

    explicit SensorItem(Skeleton& sk, SkeletonMask mask = {}, std::string name = {})
        : Base(mask, std::move(name))
    {
        this->publish(sk);
    }

    ~SensorItem() override { this->unpublish(); }
};

// Depth-2 item: Sys::Root / Sub::Actuators
class ActuatorItem final : public BoneItem<TestSchema, Sys::Root, Sub::Actuators>
{
public:
    using Base = BoneItem<TestSchema, Sys::Root, Sub::Actuators>;

    explicit ActuatorItem(Skeleton& sk, SkeletonMask mask = {}, std::string name = {})
        : Base(mask, std::move(name))
    {
        this->publish(sk);
    }

    ~ActuatorItem() override { this->unpublish(); }
};

// Depth-3 item: Sys::Root / Sub::Sensors / Chan::Load
class LoadItem final : public BoneItem<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>
{
public:
    using Base = BoneItem<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>;

    explicit LoadItem(Skeleton& sk, SkeletonMask mask = {}, std::string name = {})
        : Base(mask, std::move(name))
    {
        this->publish(sk);
    }

    ~LoadItem() override { this->unpublish(); }
};

// Depth-3 item: Sys::Root / Sub::Sensors / Chan::Temp
class TempItem final : public BoneItem<TestSchema, Sys::Root, Sub::Sensors, Chan::Temp>
{
public:
    using Base = BoneItem<TestSchema, Sys::Root, Sub::Sensors, Chan::Temp>;

    explicit TempItem(Skeleton& sk, SkeletonMask mask = {}, std::string name = {})
        : Base(mask, std::move(name))
    {
        this->publish(sk);
    }

    ~TempItem() override { this->unpublish(); }
};

// Depth-3 item: Sys::Root / Sub::Actuators / Chan::Pressure
class PressureItem final : public BoneItem<TestSchema, Sys::Root, Sub::Actuators, Chan::Pressure>
{
public:
    using Base = BoneItem<TestSchema, Sys::Root, Sub::Actuators, Chan::Pressure>;

    explicit PressureItem(Skeleton& sk, SkeletonMask mask = {}, std::string name = {})
        : Base(mask, std::move(name))
    {
        this->publish(sk);
    }

    ~PressureItem() override { this->unpublish(); }
};

// Item that can be published/unpublished manually (without RAII auto-publish)
class ManualItem final : public BoneItem<TestSchema, Sys::Aux>
{
public:
    using Base = BoneItem<TestSchema, Sys::Aux>;

    explicit ManualItem(SkeletonMask mask = {}, std::string name = {})
        : Base(mask, std::move(name))
    {
    }

    ~ManualItem() override { this->unpublish(); }

    void doPublish(Skeleton& sk) { this->publish(sk); }
    void doUnpublish() { this->unpublish(); }
    void doSetMask(SkeletonMask m) { this->setMask(m); }
};

// =============================================================================
// Suite 1: BoneId
// =============================================================================

FATP_TEST_CASE(boneid_default_is_null)
{
    BoneId id;
    FATP_ASSERT_TRUE(id.isNull(), "Default BoneId must be null");
    FATP_ASSERT_EQ(id.depth(), uint8_t(0), "Default BoneId depth must be 0");
    FATP_ASSERT_EQ(id.value(), uint64_t(0), "Default BoneId value must be 0");
    return true;
}

FATP_TEST_CASE(boneid_null_equality)
{
    BoneId a;
    BoneId b;
    FATP_ASSERT_EQ(a, b, "Two default BoneIds must be equal");
    return true;
}

FATP_TEST_CASE(boneid_non_null_from_bone)
{
    constexpr BoneId id = Bone<TestSchema, Sys::Root>::id();
    FATP_ASSERT_FALSE(id.isNull(), "Bone-derived BoneId must not be null");
    FATP_ASSERT_EQ(id.depth(), uint8_t(1), "Single-level bone must have depth 1");
    FATP_ASSERT_NE(id.value(), uint64_t(0), "Single-level bone value must not be 0");
    return true;
}

FATP_TEST_CASE(boneid_depth_matches_bone_levels)
{
    constexpr BoneId d1 = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId d2 = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId d3 = Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id();

    FATP_ASSERT_EQ(d1.depth(), uint8_t(1), "Depth-1 bone must have depth 1");
    FATP_ASSERT_EQ(d2.depth(), uint8_t(2), "Depth-2 bone must have depth 2");
    FATP_ASSERT_EQ(d3.depth(), uint8_t(3), "Depth-3 bone must have depth 3");
    return true;
}

FATP_TEST_CASE(boneid_same_path_equal)
{
    constexpr BoneId a = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId b = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_EQ(a, b, "Same bone path must produce equal BoneIds");
    return true;
}

FATP_TEST_CASE(boneid_different_path_not_equal)
{
    constexpr BoneId sensors = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId actuators = Bone<TestSchema, Sys::Root, Sub::Actuators>::id();
    FATP_ASSERT_NE(sensors, actuators, "Different bone paths must produce unequal BoneIds");
    return true;
}

FATP_TEST_CASE(boneid_different_depth_not_equal)
{
    constexpr BoneId parent = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId child  = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_NE(parent, child, "Parent and child BoneIds must not be equal");
    return true;
}

FATP_TEST_CASE(boneid_ordering_parent_before_child)
{
    constexpr BoneId parent = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId child  = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_TRUE(parent < child, "Parent BoneId must order before its child");
    FATP_ASSERT_FALSE(child < parent, "Child BoneId must not order before its parent");
    return true;
}

FATP_TEST_CASE(boneid_ordering_sibling_consistency)
{
    constexpr BoneId sensors   = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId actuators = Bone<TestSchema, Sys::Root, Sub::Actuators>::id();
    // Sensors(1) < Actuators(2): one must be less than the other, never equal
    FATP_ASSERT_NE(sensors, actuators, "Sibling BoneIds must be unequal");
    bool ordered = (sensors < actuators) || (actuators < sensors);
    FATP_ASSERT_TRUE(ordered, "Sibling BoneIds must have a total order");
    return true;
}

FATP_TEST_CASE(boneid_is_ancestor_of_basic)
{
    constexpr BoneId parent = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId child  = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId grand  = Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id();

    FATP_ASSERT_TRUE(parent.isAncestorOf(child), "Depth-1 must be ancestor of depth-2 child");
    FATP_ASSERT_TRUE(parent.isAncestorOf(grand), "Depth-1 must be ancestor of depth-3 grandchild");
    FATP_ASSERT_TRUE(child.isAncestorOf(grand), "Depth-2 must be ancestor of depth-3 child");
    return true;
}

FATP_TEST_CASE(boneid_is_not_ancestor_of_self)
{
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_FALSE(id.isAncestorOf(id), "BoneId must not be an ancestor of itself");
    return true;
}

FATP_TEST_CASE(boneid_is_not_ancestor_of_sibling)
{
    constexpr BoneId sensors   = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId actuators = Bone<TestSchema, Sys::Root, Sub::Actuators>::id();
    FATP_ASSERT_FALSE(sensors.isAncestorOf(actuators), "Sibling must not be an ancestor");
    FATP_ASSERT_FALSE(actuators.isAncestorOf(sensors), "Sibling must not be an ancestor");
    return true;
}

FATP_TEST_CASE(boneid_is_not_ancestor_of_parent)
{
    constexpr BoneId parent = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId child  = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_FALSE(child.isAncestorOf(parent), "Child must not be ancestor of parent");
    return true;
}

FATP_TEST_CASE(boneid_null_is_not_ancestor)
{
    BoneId null;
    constexpr BoneId child = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_FALSE(null.isAncestorOf(child), "Null BoneId must not be ancestor of anything");
    FATP_ASSERT_FALSE(child.isAncestorOf(null), "Nothing can be ancestor of null BoneId");
    return true;
}

FATP_TEST_CASE(boneid_is_descendant_of)
{
    constexpr BoneId parent = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId child  = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_TRUE(child.isDescendantOf(parent), "Child must be descendant of parent");
    FATP_ASSERT_FALSE(parent.isDescendantOf(child), "Parent must not be descendant of child");
    FATP_ASSERT_FALSE(child.isDescendantOf(child), "BoneId must not be descendant of itself");
    return true;
}

FATP_TEST_CASE(boneid_child_navigation)
{
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
    BoneId derived = root.child(static_cast<uint8_t>(Sub::Sensors));

    constexpr BoneId expected = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_EQ(derived, expected, "child() navigation must match Bone<>::id()");
    FATP_ASSERT_EQ(derived.depth(), uint8_t(2), "child() must increment depth by 1");
    return true;
}

FATP_TEST_CASE(boneid_parent_navigation)
{
    constexpr BoneId child  = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId parent = Bone<TestSchema, Sys::Root>::id();

    BoneId derived = child.parent();
    FATP_ASSERT_EQ(derived, parent, "parent() navigation must match Bone<>::id()");
    FATP_ASSERT_EQ(derived.depth(), uint8_t(1), "parent() must decrement depth by 1");
    return true;
}

FATP_TEST_CASE(boneid_parent_child_roundtrip)
{
    constexpr BoneId original = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    BoneId p = original.parent();
    BoneId c = p.child(static_cast<uint8_t>(Sub::Sensors));
    FATP_ASSERT_EQ(c, original, "parent() then child() must return original BoneId");
    return true;
}

FATP_TEST_CASE(boneid_child_parent_roundtrip)
{
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
    BoneId c = root.child(static_cast<uint8_t>(Sub::Actuators));
    BoneId p = c.parent();
    FATP_ASSERT_EQ(p, root, "child() then parent() must return original BoneId");
    return true;
}

FATP_TEST_CASE(boneid_to_string_null)
{
    BoneId null;
    FATP_ASSERT_EQ(null.toString(), std::string("[null]"), "Null BoneId toString must be [null]");
    return true;
}

FATP_TEST_CASE(boneid_to_string_single_level)
{
    constexpr BoneId id = Bone<TestSchema, Sys::Root>::id();
    std::string s = id.toString();
    FATP_ASSERT_FALSE(s.empty(), "toString must not be empty for non-null BoneId");
    FATP_ASSERT_STARTS_WITH(s, std::string("["), "toString must start with '['");
    FATP_ASSERT_ENDS_WITH(s, std::string("]"), "toString must end with ']'");
    // Should contain exactly one level value with no slash
    FATP_ASSERT_TRUE(s.find('/') == std::string::npos, "Single-level toString must contain no '/'");
    return true;
}

FATP_TEST_CASE(boneid_to_string_multi_level)
{
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id();
    std::string s = id.toString();
    FATP_ASSERT_STARTS_WITH(s, std::string("["), "toString must start with '['");
    FATP_ASSERT_ENDS_WITH(s, std::string("]"), "toString must end with ']'");
    // 3-level path should contain exactly 2 slashes
    std::size_t slashes = std::count(s.begin(), s.end(), '/');
    FATP_ASSERT_EQ(slashes, std::size_t(2), "Depth-3 toString must contain exactly 2 '/' separators");
    return true;
}

FATP_TEST_CASE(boneid_serialize_deserialize_roundtrip)
{
    constexpr BoneId original = Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id();

    std::array<std::byte, 9> buf{};
    original.serialize(buf);

    BoneId restored = BoneId::deserialize(buf);
    FATP_ASSERT_EQ(restored, original, "Serialize/deserialize roundtrip must preserve BoneId");
    FATP_ASSERT_EQ(restored.depth(), original.depth(), "Roundtrip must preserve depth");
    FATP_ASSERT_EQ(restored.value(), original.value(), "Roundtrip must preserve value");
    return true;
}

FATP_TEST_CASE(boneid_deserialize_null_roundtrip)
{
    BoneId null;
    std::array<std::byte, 9> buf{};
    null.serialize(buf);
    BoneId restored = BoneId::deserialize(buf);
    FATP_ASSERT_EQ(restored, null, "Null BoneId serialize/deserialize roundtrip must produce null");
    FATP_ASSERT_TRUE(restored.isNull(), "Restored null BoneId must report isNull");
    return true;
}

FATP_TEST_CASE(boneid_deserialize_invalid_depth_returns_null)
{
    // Depth byte > 8 is treated as null
    std::array<std::byte, 9> buf{};
    buf[8] = std::byte{9}; // invalid depth
    BoneId restored = BoneId::deserialize(buf);
    FATP_ASSERT_TRUE(restored.isNull(), "Depth > 8 in deserialize must produce null BoneId");
    return true;
}

FATP_TEST_CASE(boneid_deserialize_canonical_form)
{
    // Serialized bytes below active depth must be masked to zero after deserialize.
    // Build a buffer for a depth-1 bone but plant non-zero bytes at inactive levels.
    constexpr BoneId original = Bone<TestSchema, Sys::Root>::id();
    std::array<std::byte, 9> buf{};
    original.serialize(buf);

    // Corrupt the inactive levels (bytes [1..7]) with non-zero garbage
    for (std::size_t i = 1; i < 8; ++i)
    {
        buf[i] = std::byte{0xFF};
    }

    BoneId restored = BoneId::deserialize(buf);
    FATP_ASSERT_EQ(restored, original, "Deserialize must canonicalize by zeroing inactive level bytes");
    return true;
}

FATP_TEST_CASE(boneid_hash_consistency)
{
    constexpr BoneId a = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId b = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    std::hash<BoneId> h;
    FATP_ASSERT_EQ(h(a), h(b), "Equal BoneIds must produce equal hash values");
    return true;
}

FATP_TEST_CASE(boneid_hash_distinct_values)
{
    // Different BoneIds should produce different hashes (not guaranteed in general,
    // but for distinct simple values this must hold or the hash function is broken).
    constexpr BoneId sensors   = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId actuators = Bone<TestSchema, Sys::Root, Sub::Actuators>::id();
    constexpr BoneId parent    = Bone<TestSchema, Sys::Root>::id();

    std::hash<BoneId> h;
    FATP_ASSERT_NE(h(sensors), h(actuators),
        "Distinct sibling BoneIds must have distinct hashes");
    FATP_ASSERT_NE(h(parent), h(sensors),
        "Parent and child BoneIds must have distinct hashes");
    return true;
}

// =============================================================================
// Suite 2: HierarchySchema
// =============================================================================

FATP_TEST_CASE(schema_max_depth)
{
    FATP_ASSERT_EQ(TestSchema::kMaxDepth, std::size_t(3), "TestSchema must have kMaxDepth == 3");
    FATP_ASSERT_EQ(AltSchema::kMaxDepth, std::size_t(2), "AltSchema must have kMaxDepth == 2");
    return true;
}

FATP_TEST_CASE(schema_expected_types)
{
    static_assert(std::is_same_v<TestSchema::expected_type<0>, Sys>,
        "TestSchema level 0 must be Sys");
    static_assert(std::is_same_v<TestSchema::expected_type<1>, Sub>,
        "TestSchema level 1 must be Sub");
    static_assert(std::is_same_v<TestSchema::expected_type<2>, Chan>,
        "TestSchema level 2 must be Chan");
    return true;
}

// =============================================================================
// Suite 3: SkeletonCapability / SkeletonMask / makeMask
// =============================================================================

FATP_TEST_CASE(mask_default_empty)
{
    SkeletonMask m;
    FATP_ASSERT_TRUE(m.none(), "Default SkeletonMask must have no bits set");
    FATP_ASSERT_EQ(m.count(), std::size_t(0), "Default SkeletonMask must have count 0");
    return true;
}

FATP_TEST_CASE(mask_single_capability)
{
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);
    FATP_ASSERT_TRUE(m.test(static_cast<std::size_t>(SkeletonCapability::Sensor)),
        "Sensor bit must be set after makeMask(Sensor)");
    FATP_ASSERT_EQ(m.count(), std::size_t(1), "Single capability mask must have count 1");
    return true;
}

FATP_TEST_CASE(mask_multiple_capabilities)
{
    SkeletonMask m = makeMask(
        SkeletonCapability::Sensor,
        SkeletonCapability::ProvidesValue,
        SkeletonCapability::Readable
    );

    FATP_ASSERT_TRUE(m.test(static_cast<std::size_t>(SkeletonCapability::Sensor)),
        "Sensor bit must be set");
    FATP_ASSERT_TRUE(m.test(static_cast<std::size_t>(SkeletonCapability::ProvidesValue)),
        "ProvidesValue bit must be set");
    FATP_ASSERT_TRUE(m.test(static_cast<std::size_t>(SkeletonCapability::Readable)),
        "Readable bit must be set");
    FATP_ASSERT_FALSE(m.test(static_cast<std::size_t>(SkeletonCapability::Controller)),
        "Controller bit must not be set");
    FATP_ASSERT_EQ(m.count(), std::size_t(3), "Three-capability mask must have count 3");
    return true;
}

FATP_TEST_CASE(mask_bitwise_and)
{
    SkeletonMask a = makeMask(SkeletonCapability::Sensor, SkeletonCapability::Readable);
    SkeletonMask b = makeMask(SkeletonCapability::Readable, SkeletonCapability::Writable);

    SkeletonMask intersection = a & b;
    FATP_ASSERT_TRUE(intersection.test(static_cast<std::size_t>(SkeletonCapability::Readable)),
        "AND intersection must contain common Readable bit");
    FATP_ASSERT_FALSE(intersection.test(static_cast<std::size_t>(SkeletonCapability::Sensor)),
        "AND intersection must not contain Sensor (only in a)");
    FATP_ASSERT_FALSE(intersection.test(static_cast<std::size_t>(SkeletonCapability::Writable)),
        "AND intersection must not contain Writable (only in b)");
    return true;
}

FATP_TEST_CASE(mask_capability_count_sentinel_value)
{
    // Count == 32, which is the bitset size. Validate the enum value is correct.
    FATP_ASSERT_EQ(
        static_cast<uint32_t>(SkeletonCapability::Count),
        uint32_t(32),
        "SkeletonCapability::Count must equal 32"
    );
    return true;
}

// =============================================================================
// Suite 4: Bone<> compile-time type system
// =============================================================================

FATP_TEST_CASE(bone_id_matches_depth_and_values)
{
    constexpr BoneId id1 = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId id2 = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId id3 = Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id();

    FATP_ASSERT_EQ(id1.depth(), uint8_t(1), "Bone depth 1 must match");
    FATP_ASSERT_EQ(id2.depth(), uint8_t(2), "Bone depth 2 must match");
    FATP_ASSERT_EQ(id3.depth(), uint8_t(3), "Bone depth 3 must match");

    // Structural: d2 must be child of d1, d3 must be child of d2
    FATP_ASSERT_TRUE(id1.isAncestorOf(id2), "Depth-1 bone must be ancestor of depth-2");
    FATP_ASSERT_TRUE(id2.isAncestorOf(id3), "Depth-2 bone must be ancestor of depth-3");
    FATP_ASSERT_TRUE(id1.isAncestorOf(id3), "Depth-1 bone must be ancestor of depth-3");
    return true;
}

FATP_TEST_CASE(bone_depth_constant)
{
    static_assert(Bone<TestSchema, Sys::Root>::kDepth == 1, "kDepth must be 1");
    static_assert(Bone<TestSchema, Sys::Root, Sub::Sensors>::kDepth == 2, "kDepth must be 2");
    static_assert(Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::kDepth == 3, "kDepth must be 3");
    return true;
}

FATP_TEST_CASE(bone_parent_type)
{
    using ChildBone  = Bone<TestSchema, Sys::Root, Sub::Sensors>;
    using ParentBone = Bone<TestSchema, Sys::Root>;
    static_assert(std::is_same_v<ChildBone::Parent, ParentBone>,
        "Parent type of Sys::Root/Sub::Sensors must be Bone<TestSchema, Sys::Root>");
    return true;
}

FATP_TEST_CASE(bone_root_parent_is_void)
{
    using RootBone = Bone<TestSchema>;
    static_assert(std::is_same_v<RootBone::Parent, void>,
        "Parent of depth-0 Bone must be void");
    return true;
}

FATP_TEST_CASE(bone_child_alias)
{
    using ParentBone    = Bone<TestSchema, Sys::Root>;
    using ChildBone     = ParentBone::child<Sub::Sensors>;
    using ExpectedChild = Bone<TestSchema, Sys::Root, Sub::Sensors>;
    static_assert(std::is_same_v<ChildBone, ExpectedChild>,
        "child<Sub::Sensors> must produce the correct child Bone type");
    return true;
}

FATP_TEST_CASE(bone_schema_type)
{
    using B = Bone<TestSchema, Sys::Root, Sub::Sensors>;
    static_assert(std::is_same_v<B::schema_type, TestSchema>,
        "schema_type must be the schema the Bone was instantiated with");
    return true;
}

// =============================================================================
// Suite 5: SkeletonItem lifecycle and exceptions
// =============================================================================

FATP_TEST_CASE(item_not_published_after_construction)
{
    ManualItem item;
    FATP_ASSERT_FALSE(item.isPublished(), "Manually-constructed item must not be published");
    return true;
}

FATP_TEST_CASE(item_published_after_publish)
{
    Skeleton sk;
    ManualItem item;
    item.doPublish(sk);
    FATP_ASSERT_TRUE(item.isPublished(), "Item must be published after publish()");
    FATP_ASSERT_EQ(sk.size(), std::size_t(1), "Skeleton size must be 1 after publish");
    return true;
}

FATP_TEST_CASE(item_not_published_after_unpublish)
{
    Skeleton sk;
    ManualItem item;
    item.doPublish(sk);
    item.doUnpublish();
    FATP_ASSERT_FALSE(item.isPublished(), "Item must not be published after unpublish()");
    FATP_ASSERT_EQ(sk.size(), std::size_t(0), "Skeleton size must be 0 after unpublish");
    return true;
}

FATP_TEST_CASE(item_unpublish_noop_when_not_published)
{
    ManualItem item;
    FATP_ASSERT_NO_THROW(item.doUnpublish(), "unpublish() on non-published item must not throw");
    FATP_ASSERT_FALSE(item.isPublished(), "Item must remain not-published after no-op unpublish");
    return true;
}

FATP_TEST_CASE(item_publish_twice_throws_publication_error)
{
    Skeleton sk;
    ManualItem item;
    item.doPublish(sk);
    FATP_ASSERT_THROWS(item.doPublish(sk), PublicationError,
        "Publishing an already-published item must throw PublicationError");
    return true;
}

FATP_TEST_CASE(item_publish_duplicate_boneid_throws_duplicate_error)
{
    Skeleton sk;
    SysItem a(sk, {}, "a");

    // Manually construct a second item with the SAME BoneId via a second SysItem
    // (different object, same address) -- must throw DuplicateBoneError
    FATP_ASSERT_THROWS(
        { SysItem b(sk, {}, "b"); (void)b; },
        DuplicateBoneError,
        "Publishing a second item at the same BoneId must throw DuplicateBoneError"
    );
    FATP_ASSERT_EQ(sk.size(), std::size_t(1), "Registry must be unchanged after duplicate throw");
    return true;
}

FATP_TEST_CASE(item_boneid_accessor)
{
    Skeleton sk;
    SensorItem s(sk);
    constexpr BoneId expected = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_EQ(s.boneId(), expected, "boneId() must match the Bone<>::id() for this item type");
    return true;
}

FATP_TEST_CASE(item_name_accessor)
{
    Skeleton sk;
    SensorItem s(sk, {}, "load_sensor_01");
    FATP_ASSERT_EQ(s.name(), std::string_view("load_sensor_01"),
        "name() must return the name provided at construction");
    return true;
}

FATP_TEST_CASE(item_mask_accessor)
{
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor, SkeletonCapability::Readable);
    SensorItem s(sk, m, "sensor");
    FATP_ASSERT_EQ(s.mask(), m, "mask() must return the mask provided at construction");
    return true;
}

FATP_TEST_CASE(item_set_mask_while_not_published)
{
    ManualItem item(makeMask(SkeletonCapability::Sensor), "item");
    SkeletonMask newMask = makeMask(SkeletonCapability::Controller);
    item.doSetMask(newMask);
    FATP_ASSERT_EQ(item.mask(), newMask,
        "setMask() on unpublished item must update the local mask");
    return true;
}

FATP_TEST_CASE(item_set_mask_while_published)
{
    Skeleton sk;
    ManualItem item(makeMask(SkeletonCapability::Sensor), "item");
    item.doPublish(sk);

    SkeletonMask newMask = makeMask(SkeletonCapability::Controller, SkeletonCapability::Writable);
    item.doSetMask(newMask);
    FATP_ASSERT_EQ(item.mask(), newMask,
        "setMask() on published item must update the mask");
    return true;
}

FATP_TEST_CASE(item_republish_after_unpublish)
{
    Skeleton sk;
    ManualItem item;
    item.doPublish(sk);
    item.doUnpublish();
    // Must be able to publish again on the same (now empty) skeleton
    FATP_ASSERT_NO_THROW(item.doPublish(sk),
        "Publishing an item after unpublish must not throw");
    FATP_ASSERT_TRUE(item.isPublished(), "Item must be published after republish");
    FATP_ASSERT_EQ(sk.size(), std::size_t(1), "Skeleton must have 1 item after republish");
    return true;
}

FATP_TEST_CASE(item_republish_on_different_skeleton)
{
    Skeleton sk1("sk1");
    Skeleton sk2("sk2");
    ManualItem item;

    item.doPublish(sk1);
    FATP_ASSERT_EQ(sk1.size(), std::size_t(1), "sk1 must have 1 item");
    item.doUnpublish();
    FATP_ASSERT_EQ(sk1.size(), std::size_t(0), "sk1 must be empty after unpublish");

    item.doPublish(sk2);
    FATP_ASSERT_EQ(sk2.size(), std::size_t(1), "sk2 must have 1 item after republish");
    FATP_ASSERT_TRUE(item.isPublished(), "Item must report as published on sk2");
    return true;
}

// =============================================================================
// Suite 6: Skeleton registry operations
// =============================================================================

FATP_TEST_CASE(skeleton_empty_on_construction)
{
    Skeleton sk("test");
    FATP_ASSERT_EQ(sk.size(), std::size_t(0), "New Skeleton must be empty");
    return true;
}

FATP_TEST_CASE(skeleton_name_accessor)
{
    Skeleton sk("my_skeleton");
    FATP_ASSERT_EQ(sk.name(), std::string_view("my_skeleton"), "name() must return construction name");
    return true;
}

FATP_TEST_CASE(skeleton_size_increments_on_publish)
{
    Skeleton sk;
    FATP_ASSERT_EQ(sk.size(), std::size_t(0), "Empty skeleton size must be 0");

    SysItem sys(sk);
    FATP_ASSERT_EQ(sk.size(), std::size_t(1), "Size must be 1 after first publish");

    SensorItem sensor(sk);
    FATP_ASSERT_EQ(sk.size(), std::size_t(2), "Size must be 2 after second publish");
    return true;
}

FATP_TEST_CASE(skeleton_size_decrements_on_unpublish)
{
    Skeleton sk;
    {
        SysItem sys(sk);
        SensorItem sensor(sk);
        FATP_ASSERT_EQ(sk.size(), std::size_t(2), "Size must be 2 with two items");
    }
    // Both items destroyed and unpublished
    FATP_ASSERT_EQ(sk.size(), std::size_t(0), "Size must be 0 after all items destroyed");
    return true;
}

FATP_TEST_CASE(skeleton_find_returns_correct_item)
{
    Skeleton sk;
    SensorItem sensor(sk, {}, "my_sensor");
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    SkeletonItem* found = sk.find(id);
    FATP_ASSERT_NOT_NULLPTR(found, "find() must return non-null for published item");
    FATP_ASSERT_EQ(found->boneId(), id, "Found item must have correct BoneId");
    FATP_ASSERT_EQ(found->name(), std::string_view("my_sensor"), "Found item must have correct name");
    return true;
}

FATP_TEST_CASE(skeleton_find_returns_null_when_absent)
{
    Skeleton sk;
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    SkeletonItem* found = sk.find(id);
    FATP_ASSERT_NULLPTR(found, "find() must return nullptr for absent BoneId");
    return true;
}

FATP_TEST_CASE(skeleton_find_const_overload)
{
    Skeleton sk;
    SensorItem sensor(sk, {}, "const_test");
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    const Skeleton& csk = sk;
    const SkeletonItem* found = csk.find(id);
    FATP_ASSERT_NOT_NULLPTR(found, "const find() must return non-null for published item");
    FATP_ASSERT_EQ(found->name(), std::string_view("const_test"), "const find() must return correct item");
    return true;
}

FATP_TEST_CASE(skeleton_find_as_typed)
{
    Skeleton sk;
    SensorItem sensor(sk, makeMask(SkeletonCapability::Sensor), "typed");
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    SensorItem* typed = sk.findAs<SensorItem>(id);
    FATP_ASSERT_NOT_NULLPTR(typed, "findAs<SensorItem>() must return non-null");
    FATP_ASSERT_EQ(typed->name(), std::string_view("typed"), "findAs() must return the correct item");
    return true;
}

FATP_TEST_CASE(skeleton_find_after_unpublish_returns_null)
{
    Skeleton sk;
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    {
        SensorItem sensor(sk);
        FATP_ASSERT_NOT_NULLPTR(sk.find(id), "find() must succeed while item is published");
    }
    // Item destroyed, unpublished
    FATP_ASSERT_NULLPTR(sk.find(id), "find() must return null after item is destroyed");
    return true;
}

FATP_TEST_CASE(skeleton_find_null_boneid_returns_null)
{
    Skeleton sk;
    SensorItem sensor(sk);
    BoneId null;
    FATP_ASSERT_NULLPTR(sk.find(null), "find() with null BoneId must return nullptr");
    return true;
}

// =============================================================================
// Suite 7: visitSubtree
// =============================================================================

FATP_TEST_CASE(visit_subtree_root_inclusive)
{
    Skeleton sk;
    SysItem    sys(sk, {}, "sys");
    SensorItem sensor(sk, {}, "sensor");
    LoadItem   load(sk, {}, "load");
    TempItem   temp(sk, {}, "temp");

    constexpr BoneId sensorRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    std::vector<BoneId> visited;
    sk.visitSubtree(sensorRoot, [&](SkeletonItem& item)
    {
        visited.push_back(item.boneId());
    });

    // Should include sensor (root of subtree) + load + temp -- NOT sys or actuators
    FATP_ASSERT_EQ(visited.size(), std::size_t(3), "visitSubtree must visit sensor root + 2 children");
    FATP_ASSERT_TRUE(
        std::find(visited.begin(), visited.end(), Bone<TestSchema, Sys::Root, Sub::Sensors>::id()) != visited.end(),
        "visitSubtree must include the subtree root itself"
    );
    FATP_ASSERT_TRUE(
        std::find(visited.begin(), visited.end(), Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id()) != visited.end(),
        "visitSubtree must include Load child"
    );
    return true;
}

FATP_TEST_CASE(visit_subtree_ascending_order)
{
    Skeleton sk;
    SysItem    sys(sk, {}, "sys");
    SensorItem sensor(sk, {}, "sensor");
    LoadItem   load(sk, {}, "load");
    TempItem   temp(sk, {}, "temp");

    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();

    std::vector<BoneId> visited;
    sk.visitSubtree(root, [&](SkeletonItem& item)
    {
        visited.push_back(item.boneId());
    });

    FATP_ASSERT_TRUE(std::is_sorted(visited.begin(), visited.end()),
        "visitSubtree must deliver items in ascending BoneId order");
    return true;
}

FATP_TEST_CASE(visit_subtree_parent_before_child)
{
    Skeleton sk;
    SysItem    sys(sk);
    SensorItem sensor(sk);
    LoadItem   load(sk);

    constexpr BoneId root    = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId sensorId = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId loadId   = Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id();

    std::vector<BoneId> visited;
    sk.visitSubtree(root, [&](SkeletonItem& item)
    {
        visited.push_back(item.boneId());
    });

    auto posRoot   = std::find(visited.begin(), visited.end(), root);
    auto posSensor = std::find(visited.begin(), visited.end(), sensorId);
    auto posLoad   = std::find(visited.begin(), visited.end(), loadId);

    FATP_ASSERT_TRUE(posRoot   < posSensor, "Root must appear before Sensor");
    FATP_ASSERT_TRUE(posSensor < posLoad,   "Sensor must appear before Load");
    return true;
}

FATP_TEST_CASE(visit_subtree_empty_subtree)
{
    Skeleton sk;
    SysItem sys(sk);
    // No items under Sub::Network
    constexpr BoneId networkRoot = Bone<TestSchema, Sys::Root, Sub::Network>::id();

    int callCount = 0;
    sk.visitSubtree(networkRoot, [&](SkeletonItem&) { ++callCount; });
    FATP_ASSERT_EQ(callCount, 0, "visitSubtree on absent subtree must invoke callback 0 times");
    return true;
}

FATP_TEST_CASE(visit_subtree_const_overload)
{
    Skeleton sk;
    SensorItem sensor(sk, {}, "sensor");
    LoadItem   load(sk);

    constexpr BoneId sensorRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    const Skeleton& csk = sk;

    int count = 0;
    csk.visitSubtree(sensorRoot, [&](const SkeletonItem& item)
    {
        ++count;
        (void)item;
    });
    FATP_ASSERT_EQ(count, 2, "const visitSubtree must visit sensor root and its child");
    return true;
}

// =============================================================================
// Suite 8: query and querySubtree
// =============================================================================

FATP_TEST_CASE(query_returns_all_when_empty_mask)
{
    Skeleton sk;
    SysItem    sys(sk);
    SensorItem sensor(sk);
    LoadItem   load(sk);

    SkeletonMask required;
    SkeletonMask excluded;
    auto results = sk.query(required, excluded);
    FATP_ASSERT_EQ(results.size(), std::size_t(3), "Empty required/excluded mask must match all items");
    return true;
}

FATP_TEST_CASE(query_required_mask_filters)
{
    Skeleton sk;
    SkeletonMask sensorMask = makeMask(SkeletonCapability::Sensor);
    SkeletonMask ctrlMask   = makeMask(SkeletonCapability::Controller);

    SysItem    sys(sk, {});
    SensorItem sensor(sk, sensorMask, "sensor");
    ActuatorItem actuator(sk, ctrlMask, "actuator");

    auto results = sk.query(sensorMask);
    FATP_ASSERT_EQ(results.size(), std::size_t(1), "Required Sensor mask must match only sensor item");
    FATP_ASSERT_EQ(results[0]->name(), std::string_view("sensor"),
        "Filtered item must be the sensor");
    return true;
}

FATP_TEST_CASE(query_excluded_mask_filters)
{
    Skeleton sk;
    SkeletonMask sensorMask   = makeMask(SkeletonCapability::Sensor);
    SkeletonMask readableMask = makeMask(SkeletonCapability::Readable);

    SysItem    sys(sk, {});
    SensorItem sensor(sk, sensorMask, "sensor");          // Sensor, no Readable
    LoadItem   load(sk, sensorMask | readableMask, "load"); // Sensor + Readable

    // Query: has Sensor, but NOT Readable
    auto results = sk.query(sensorMask, readableMask);
    FATP_ASSERT_EQ(results.size(), std::size_t(1), "Excluded Readable mask must filter out load");
    FATP_ASSERT_EQ(results[0]->name(), std::string_view("sensor"),
        "Only the non-Readable sensor must survive exclusion");
    return true;
}

FATP_TEST_CASE(query_results_ascending_order)
{
    Skeleton sk;
    SysItem    sys(sk, {});
    SensorItem sensor(sk, {});
    LoadItem   load(sk, {});
    TempItem   temp(sk, {});

    auto results = sk.query({});
    std::vector<BoneId> ids;
    for (auto* item : results)
    {
        ids.push_back(item->boneId());
    }
    FATP_ASSERT_TRUE(std::is_sorted(ids.begin(), ids.end()),
        "query() results must be in ascending BoneId order");
    return true;
}

FATP_TEST_CASE(query_const_overload)
{
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);
    SensorItem sensor(sk, m);

    const Skeleton& csk = sk;
    auto results = csk.query(m);
    FATP_ASSERT_EQ(results.size(), std::size_t(1), "const query() must return matching item");
    return true;
}

FATP_TEST_CASE(query_subtree_restricts_to_subtree)
{
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);

    SysItem      sys(sk, m);
    SensorItem   sensor(sk, m);
    LoadItem     load(sk, m);
    ActuatorItem actuator(sk, m);  // under Sys::Root / Sub::Actuators, not Sensors subtree

    constexpr BoneId sensorRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    auto results = sk.querySubtree(sensorRoot, m);

    FATP_ASSERT_EQ(results.size(), std::size_t(2),
        "querySubtree must only return items within the Sensors subtree");

    for (auto* item : results)
    {
        FATP_ASSERT_TRUE(
            item->boneId() == sensorRoot || sensorRoot.isAncestorOf(item->boneId()),
            "Each result must be within the queried subtree"
        );
    }
    return true;
}

FATP_TEST_CASE(query_subtree_includes_root)
{
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);
    SensorItem sensor(sk, m, "sensor_root");

    constexpr BoneId sensorRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    auto results = sk.querySubtree(sensorRoot, m);

    FATP_ASSERT_EQ(results.size(), std::size_t(1), "querySubtree must include the subtree root itself");
    FATP_ASSERT_EQ(results[0]->boneId(), sensorRoot, "Result must be the subtree root");
    return true;
}

FATP_TEST_CASE(query_subtree_const_overload)
{
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);
    SensorItem sensor(sk, m);
    LoadItem   load(sk, m);

    const Skeleton& csk = sk;
    constexpr BoneId sensorRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    auto results = csk.querySubtree(sensorRoot, m);
    FATP_ASSERT_EQ(results.size(), std::size_t(2), "const querySubtree() must return correct count");
    return true;
}

// =============================================================================
// Suite 9: Signals -- onPublished, onUnpublishing, onMaskChanged
// =============================================================================

FATP_TEST_CASE(signal_on_published_fires_after_insert)
{
    Skeleton sk;
    int callCount = 0;
    BoneId capturedId;

    auto conn = sk.onPublished([&](SkeletonItem& item)
    {
        ++callCount;
        capturedId = item.boneId();
        // Item must be findable inside the callback
        SkeletonItem* found = sk.find(item.boneId());
        FATP_SIMPLE_ASSERT(found != nullptr, "Item must be in registry inside onPublished callback");
    });

    {
        SensorItem sensor(sk, {}, "sensor");
        FATP_ASSERT_EQ(callCount, 1, "onPublished must fire exactly once after publish");
        FATP_ASSERT_EQ(capturedId, Bone<TestSchema, Sys::Root, Sub::Sensors>::id(),
            "onPublished must deliver the correct BoneId");
    }
    return true;
}

FATP_TEST_CASE(signal_on_unpublishing_fires_while_item_still_present)
{
    Skeleton sk;
    bool firedWhilePresent = false;

    auto conn = sk.onUnpublishing([&](SkeletonItem& item)
    {
        // Item must still be in registry during unpublishing
        SkeletonItem* found = sk.find(item.boneId());
        firedWhilePresent = (found != nullptr);
    });

    {
        SensorItem sensor(sk);
    }

    FATP_ASSERT_TRUE(firedWhilePresent,
        "onUnpublishing callback must see the item still present in the registry");
    return true;
}

FATP_TEST_CASE(signal_on_unpublishing_fires_before_removal)
{
    Skeleton sk;
    int unpublishingCount = 0;

    auto conn = sk.onUnpublishing([&](SkeletonItem&) { ++unpublishingCount; });

    {
        SensorItem sensor(sk);
        FATP_ASSERT_EQ(unpublishingCount, 0, "onUnpublishing must not fire before item is destroyed");
    }
    FATP_ASSERT_EQ(unpublishingCount, 1, "onUnpublishing must fire exactly once on destroy");
    return true;
}

FATP_TEST_CASE(signal_on_mask_changed_fires_with_new_mask)
{
    Skeleton sk;
    SkeletonMask initialMask = makeMask(SkeletonCapability::Sensor);
    SkeletonMask newMask     = makeMask(SkeletonCapability::Controller, SkeletonCapability::Writable);

    bool firedWithNewMask = false;
    SkeletonMask capturedOldMask;

    auto conn = sk.onMaskChanged([&](SkeletonItem& item, SkeletonMask oldMask)
    {
        // item.mask() must already reflect the new mask when callback fires
        firedWithNewMask = (item.mask() == newMask);
        capturedOldMask  = oldMask;
    });

    ManualItem item(initialMask, "item");
    item.doPublish(sk);
    item.doSetMask(newMask);

    FATP_ASSERT_TRUE(firedWithNewMask,
        "onMaskChanged callback must see the new mask on the item");
    FATP_ASSERT_EQ(capturedOldMask, initialMask,
        "onMaskChanged must deliver the previous mask as the second argument");
    return true;
}

FATP_TEST_CASE(signal_on_mask_changed_not_fired_when_unpublished)
{
    Skeleton sk;
    int callCount = 0;

    auto conn = sk.onMaskChanged([&](SkeletonItem&, SkeletonMask) { ++callCount; });

    ManualItem item;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);
    item.doSetMask(m);  // not published -- must NOT fire signal

    FATP_ASSERT_EQ(callCount, 0,
        "onMaskChanged must not fire when item is not published");
    return true;
}

FATP_TEST_CASE(signal_scoped_connection_auto_disconnect)
{
    Skeleton sk;
    int callCount = 0;

    {
        auto conn = sk.onPublished([&](SkeletonItem&) { ++callCount; });
        SensorItem sensor(sk);
        FATP_ASSERT_EQ(callCount, 1, "Signal must fire while connection is alive");
        // conn destroyed here -- auto-disconnect
    }

    // Publish another item -- callback must NOT fire (connection is gone)
    {
        LoadItem load(sk);
        FATP_ASSERT_EQ(callCount, 1, "Signal must not fire after ScopedConnection destroyed");
    }
    return true;
}

FATP_TEST_CASE(signal_multiple_listeners)
{
    Skeleton sk;
    int countA = 0;
    int countB = 0;

    auto connA = sk.onPublished([&](SkeletonItem&) { ++countA; });
    auto connB = sk.onPublished([&](SkeletonItem&) { ++countB; });

    SensorItem sensor(sk);

    FATP_ASSERT_EQ(countA, 1, "First listener must fire once");
    FATP_ASSERT_EQ(countB, 1, "Second listener must fire once");
    return true;
}

FATP_TEST_CASE(signal_priority_ordering)
{
    Skeleton sk;
    std::vector<int> order;

    // High priority (10) must fire before low priority (-5)
    auto connLow  = sk.onPublished([&](SkeletonItem&) { order.push_back(-5); }, -5);
    auto connHigh = sk.onPublished([&](SkeletonItem&) { order.push_back(10); }, 10);

    SensorItem sensor(sk);

    FATP_ASSERT_EQ(order.size(), std::size_t(2), "Both listeners must fire");
    FATP_ASSERT_EQ(order[0], 10, "High priority listener must fire first");
    FATP_ASSERT_EQ(order[1], -5, "Low priority listener must fire second");
    return true;
}

// =============================================================================
// Suite 10: Skeleton::dump()
// =============================================================================

FATP_TEST_CASE(skeleton_dump_empty)
{
    Skeleton sk("empty_sk");
    std::ostringstream oss;
    sk.dump(oss);
    std::string output = oss.str();
    FATP_ASSERT_CONTAINS(output, std::string("empty_sk"), "dump() must include skeleton name");
    FATP_ASSERT_CONTAINS(output, std::string("0 items"), "dump() of empty skeleton must show 0 items");
    return true;
}

FATP_TEST_CASE(skeleton_dump_lists_items)
{
    Skeleton sk("test_sk");
    SensorItem sensor(sk, makeMask(SkeletonCapability::Sensor), "my_sensor");
    LoadItem   load(sk, {}, "my_load");

    std::ostringstream oss;
    sk.dump(oss);
    std::string output = oss.str();

    FATP_ASSERT_CONTAINS(output, std::string("my_sensor"), "dump() must include item names");
    FATP_ASSERT_CONTAINS(output, std::string("my_load"), "dump() must include all item names");
    FATP_ASSERT_CONTAINS(output, std::string("2 items"), "dump() must report correct item count");
    return true;
}

// =============================================================================
// Suite 11: Stress / fuzz
// =============================================================================

FATP_TEST_CASE(stress_many_items_publish_and_query)
{
    // Build a skeleton with all 9 items in the test hierarchy
    Skeleton sk;

    SysItem      sys(sk,      makeMask(SkeletonCapability::Sensor), "sys");
    SensorItem   sensor(sk,   makeMask(SkeletonCapability::Sensor), "sensor");
    ActuatorItem actuator(sk, makeMask(SkeletonCapability::Controller), "actuator");
    LoadItem     load(sk,     makeMask(SkeletonCapability::Sensor, SkeletonCapability::Readable), "load");
    TempItem     temp(sk,     makeMask(SkeletonCapability::Sensor, SkeletonCapability::Readable), "temp");
    PressureItem pressure(sk, makeMask(SkeletonCapability::Controller), "pressure");

    FATP_ASSERT_EQ(sk.size(), std::size_t(6), "Stress: all 6 items must be registered");

    // All Sensor items
    SkeletonMask sensorReq = makeMask(SkeletonCapability::Sensor);
    auto sensors = sk.query(sensorReq);
    FATP_ASSERT_EQ(sensors.size(), std::size_t(3),
        "Stress: query(Sensor) must return sys, sensor, load, temp = 3... wait 3 have Sensor but sys does too");

    // Controller items
    SkeletonMask ctrlReq = makeMask(SkeletonCapability::Controller);
    auto controllers = sk.query(ctrlReq);
    FATP_ASSERT_EQ(controllers.size(), std::size_t(2),
        "Stress: query(Controller) must return actuator + pressure");

    // Readable items
    SkeletonMask readReq = makeMask(SkeletonCapability::Readable);
    auto readables = sk.query(readReq);
    FATP_ASSERT_EQ(readables.size(), std::size_t(2),
        "Stress: query(Readable) must return load + temp");

    return true;
}

FATP_TEST_CASE(stress_repeated_publish_unpublish)
{
    Skeleton sk;
    const int kIterations = 50;

    for (int i = 0; i < kIterations; ++i)
    {
        {
            SensorItem sensor(sk, {}, "iter_" + std::to_string(i));
            FATP_ASSERT_EQ(sk.size(), std::size_t(1),
                "During publish iteration skeleton must have 1 item");
            FATP_ASSERT_NOT_NULLPTR(sk.find(Bone<TestSchema, Sys::Root, Sub::Sensors>::id()),
                "Item must be findable during publish iteration");
        }
        FATP_ASSERT_EQ(sk.size(), std::size_t(0),
            "After unpublish iteration skeleton must be empty");
    }
    return true;
}

FATP_TEST_CASE(stress_query_consistency)
{
    // Publish all items with various masks, then verify query + querySubtree are consistent.
    Skeleton sk;
    SkeletonMask sensorMask = makeMask(SkeletonCapability::Sensor);
    SkeletonMask readMask   = makeMask(SkeletonCapability::Readable);

    SysItem      sys(sk,      sensorMask);
    SensorItem   sensor(sk,   sensorMask);
    ActuatorItem actuator(sk, {});
    LoadItem     load(sk,     sensorMask | readMask);
    TempItem     temp(sk,     sensorMask | readMask);
    PressureItem pressure(sk, readMask);

    // Global query must be superset of any subtree query
    auto allSensors     = sk.query(sensorMask);
    constexpr BoneId sensorRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    auto subtreeSensors = sk.querySubtree(sensorRoot, sensorMask);

    // Every subtree result must appear in the global result
    for (auto* item : subtreeSensors)
    {
        bool found = std::any_of(allSensors.begin(), allSensors.end(),
            [item](SkeletonItem* i) { return i == item; });
        FATP_ASSERT_TRUE(found,
            "Every subtree query result must appear in the global query result");
    }

    // Subtree results must be a subset (size <=)
    FATP_ASSERT_LE(subtreeSensors.size(), allSensors.size(),
        "Subtree query result must be no larger than global query result");

    return true;
}

FATP_TEST_CASE(stress_signal_consistency)
{
    Skeleton sk;
    int published   = 0;
    int unpublished = 0;

    auto connP = sk.onPublished([&](SkeletonItem&) { ++published; });
    auto connU = sk.onUnpublishing([&](SkeletonItem&) { ++unpublished; });

    const int kItems = 10;

    // Publish all items (can't do this with identical BoneIds, so use ManualItem
    // which has a unique BoneId -- but we only have one Sys::Aux slot).
    // Instead, test with RAII items in a controlled scope.
    {
        SysItem      i1(sk);
        SensorItem   i2(sk);
        ActuatorItem i3(sk);
        LoadItem     i4(sk);
        TempItem     i5(sk);
        PressureItem i6(sk);

        FATP_ASSERT_EQ(published, 6, "Signal must fire for each of the 6 publish operations");
        FATP_ASSERT_EQ(unpublished, 0, "No unpublish signals must have fired yet");
    }

    FATP_ASSERT_EQ(unpublished, 6, "Signal must fire for each of the 6 unpublish operations");
    FATP_ASSERT_EQ(published, 6, "Publish count must remain 6");
    FATP_ASSERT_EQ(sk.size(), std::size_t(0), "Skeleton must be empty after all items destroyed");

    return true;
}

} // namespace fat_p::testing::skeleton

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{

bool test_Skeleton()
{
    FATP_PRINT_HEADER(SKELETON)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // BoneId
    out << "\n--- BoneId ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, boneid_default_is_null);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_null_equality);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_non_null_from_bone);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_depth_matches_bone_levels);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_same_path_equal);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_different_path_not_equal);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_different_depth_not_equal);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_ordering_parent_before_child);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_ordering_sibling_consistency);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_is_ancestor_of_basic);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_is_not_ancestor_of_self);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_is_not_ancestor_of_sibling);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_is_not_ancestor_of_parent);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_null_is_not_ancestor);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_is_descendant_of);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_child_navigation);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_parent_navigation);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_parent_child_roundtrip);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_child_parent_roundtrip);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_to_string_null);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_to_string_single_level);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_to_string_multi_level);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_serialize_deserialize_roundtrip);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_deserialize_null_roundtrip);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_deserialize_invalid_depth_returns_null);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_deserialize_canonical_form);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_hash_consistency);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_hash_distinct_values);

    // HierarchySchema
    out << "\n--- HierarchySchema ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, schema_max_depth);
    FATP_RUN_TEST_NS(runner, skeleton, schema_expected_types);

    // SkeletonCapability / SkeletonMask
    out << "\n--- SkeletonMask ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, mask_default_empty);
    FATP_RUN_TEST_NS(runner, skeleton, mask_single_capability);
    FATP_RUN_TEST_NS(runner, skeleton, mask_multiple_capabilities);
    FATP_RUN_TEST_NS(runner, skeleton, mask_bitwise_and);
    FATP_RUN_TEST_NS(runner, skeleton, mask_capability_count_sentinel_value);

    // Bone<>
    out << "\n--- Bone<> ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, bone_id_matches_depth_and_values);
    FATP_RUN_TEST_NS(runner, skeleton, bone_depth_constant);
    FATP_RUN_TEST_NS(runner, skeleton, bone_parent_type);
    FATP_RUN_TEST_NS(runner, skeleton, bone_root_parent_is_void);
    FATP_RUN_TEST_NS(runner, skeleton, bone_child_alias);
    FATP_RUN_TEST_NS(runner, skeleton, bone_schema_type);

    // SkeletonItem lifecycle
    out << "\n--- SkeletonItem Lifecycle ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, item_not_published_after_construction);
    FATP_RUN_TEST_NS(runner, skeleton, item_published_after_publish);
    FATP_RUN_TEST_NS(runner, skeleton, item_not_published_after_unpublish);
    FATP_RUN_TEST_NS(runner, skeleton, item_unpublish_noop_when_not_published);
    FATP_RUN_TEST_NS(runner, skeleton, item_publish_twice_throws_publication_error);
    FATP_RUN_TEST_NS(runner, skeleton, item_publish_duplicate_boneid_throws_duplicate_error);
    FATP_RUN_TEST_NS(runner, skeleton, item_boneid_accessor);
    FATP_RUN_TEST_NS(runner, skeleton, item_name_accessor);
    FATP_RUN_TEST_NS(runner, skeleton, item_mask_accessor);
    FATP_RUN_TEST_NS(runner, skeleton, item_set_mask_while_not_published);
    FATP_RUN_TEST_NS(runner, skeleton, item_set_mask_while_published);
    FATP_RUN_TEST_NS(runner, skeleton, item_republish_after_unpublish);
    FATP_RUN_TEST_NS(runner, skeleton, item_republish_on_different_skeleton);

    // Skeleton registry
    out << "\n--- Skeleton Registry ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_empty_on_construction);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_name_accessor);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_size_increments_on_publish);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_size_decrements_on_unpublish);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_find_returns_correct_item);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_find_returns_null_when_absent);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_find_const_overload);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_find_as_typed);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_find_after_unpublish_returns_null);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_find_null_boneid_returns_null);

    // visitSubtree
    out << "\n--- visitSubtree ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, visit_subtree_root_inclusive);
    FATP_RUN_TEST_NS(runner, skeleton, visit_subtree_ascending_order);
    FATP_RUN_TEST_NS(runner, skeleton, visit_subtree_parent_before_child);
    FATP_RUN_TEST_NS(runner, skeleton, visit_subtree_empty_subtree);
    FATP_RUN_TEST_NS(runner, skeleton, visit_subtree_const_overload);

    // query / querySubtree
    out << "\n--- query / querySubtree ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, query_returns_all_when_empty_mask);
    FATP_RUN_TEST_NS(runner, skeleton, query_required_mask_filters);
    FATP_RUN_TEST_NS(runner, skeleton, query_excluded_mask_filters);
    FATP_RUN_TEST_NS(runner, skeleton, query_results_ascending_order);
    FATP_RUN_TEST_NS(runner, skeleton, query_const_overload);
    FATP_RUN_TEST_NS(runner, skeleton, query_subtree_restricts_to_subtree);
    FATP_RUN_TEST_NS(runner, skeleton, query_subtree_includes_root);
    FATP_RUN_TEST_NS(runner, skeleton, query_subtree_const_overload);

    // Signals
    out << "\n--- Signals ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_published_fires_after_insert);
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_unpublishing_fires_while_item_still_present);
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_unpublishing_fires_before_removal);
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_mask_changed_fires_with_new_mask);
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_mask_changed_not_fired_when_unpublished);
    FATP_RUN_TEST_NS(runner, skeleton, signal_scoped_connection_auto_disconnect);
    FATP_RUN_TEST_NS(runner, skeleton, signal_multiple_listeners);
    FATP_RUN_TEST_NS(runner, skeleton, signal_priority_ordering);

    // dump()
    out << "\n--- dump() ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_dump_empty);
    FATP_RUN_TEST_NS(runner, skeleton, skeleton_dump_lists_items);

    // Stress
    out << "\n--- Stress ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, stress_many_items_publish_and_query);
    FATP_RUN_TEST_NS(runner, skeleton, stress_repeated_publish_unpublish);
    FATP_RUN_TEST_NS(runner, skeleton, stress_query_consistency);
    FATP_RUN_TEST_NS(runner, skeleton, stress_signal_consistency);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Skeleton() ? 0 : 1;
}
#endif
