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
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
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
// BoneId stream operator (required by FATP_ASSERT_EQ for failure diagnostics)
// =============================================================================

inline std::ostream& operator<<(std::ostream& os, const BoneId& id)
{
    return os << id.toString();
}

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
    std::size_t slashes = static_cast<std::size_t>(std::count(s.begin(), s.end(), '/'));
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
        [&sk]() { SysItem b(sk, {}, "b"); (void)b; }(),
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

    bool foundInsideCallback = false;
    auto conn = sk.onPublished([&](SkeletonItem& item)
    {
        ++callCount;
        capturedId = item.boneId();
        // Item must be findable inside the callback -- capture result for assertion outside lambda
        foundInsideCallback = (sk.find(item.boneId()) != nullptr);
    });

    {
        SensorItem sensor(sk, {}, "sensor");
        FATP_ASSERT_EQ(callCount, 1, "onPublished must fire exactly once after publish");
        constexpr BoneId expectedId = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
        FATP_ASSERT_EQ(capturedId, expectedId,
            "onPublished must deliver the correct BoneId");
        FATP_ASSERT_TRUE(foundInsideCallback,
            "Item must be findable in registry inside onPublished callback");
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

    // All Sensor items: sys, sensor, load, temp = 4
    SkeletonMask sensorReq = makeMask(SkeletonCapability::Sensor);
    auto sensors = sk.query(sensorReq);
    FATP_ASSERT_EQ(sensors.size(), std::size_t(4),
        "Stress: query(Sensor) must return sys, sensor, load, and temp (4 items)");

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

// =============================================================================
// Suite 12: BoneId boundary and structural properties
// =============================================================================

FATP_TEST_CASE(boneid_max_depth_chain)
{
    // Build a depth-8 BoneId via child() starting from a depth-1 root.
    // Each call to child() must increment depth by exactly 1.
    constexpr BoneId d1 = Bone<TestSchema, Sys::Root>::id();

    BoneId cur = d1;
    for (uint8_t level = 1; level < 8; ++level)
    {
        cur = cur.child(level); // use level value as the index
        FATP_ASSERT_EQ(cur.depth(), static_cast<uint8_t>(level + 1),
            "Each child() must increment depth by exactly 1");
        FATP_ASSERT_FALSE(cur.isNull(), "Intermediate BoneId must not be null");
    }
    FATP_ASSERT_EQ(cur.depth(), uint8_t(8), "Maximum depth must be exactly 8");
    return true;
}

FATP_TEST_CASE(boneid_max_depth_parent_chain)
{
    // Build depth-8 via child() then walk all the way back with parent().
    constexpr BoneId d1 = Bone<TestSchema, Sys::Root>::id();
    BoneId cur = d1;
    for (uint8_t i = 0; i < 7; ++i)
    {
        cur = cur.child(static_cast<uint8_t>(i + 1));
    }
    FATP_ASSERT_EQ(cur.depth(), uint8_t(8), "Must have reached depth 8");

    // Walk back to depth 1 via parent()
    for (uint8_t expectedDepth = 7; expectedDepth >= 1; --expectedDepth)
    {
        cur = cur.parent();
        FATP_ASSERT_EQ(cur.depth(), expectedDepth,
            "parent() must decrement depth by exactly 1");
    }
    // cur must now equal d1
    FATP_ASSERT_EQ(cur, d1, "Walking back via parent() must recover the original BoneId");
    return true;
}

FATP_TEST_CASE(boneid_child_with_zero_index)
{
    // Level value 0 is valid (Chan::Load has underlying value 0).
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
    BoneId child = root.child(0);
    FATP_ASSERT_EQ(child.depth(), uint8_t(2), "child(0) must produce depth 2");
    FATP_ASSERT_FALSE(child.isNull(), "child(0) must not be null");
    // Round-trip
    BoneId back = child.parent();
    FATP_ASSERT_EQ(back, root, "parent() of child(0) must recover root");
    return true;
}

FATP_TEST_CASE(boneid_child_with_max_index)
{
    // Level value 255 is the maximum valid index.
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
    BoneId child = root.child(255);
    FATP_ASSERT_EQ(child.depth(), uint8_t(2), "child(255) must produce depth 2");
    FATP_ASSERT_FALSE(child.isNull(), "child(255) must not be null");
    // Round-trip
    BoneId back = child.parent();
    FATP_ASSERT_EQ(back, root, "parent() of child(255) must recover root");
    return true;
}

FATP_TEST_CASE(boneid_child_zero_vs_child_one_are_distinct)
{
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
    BoneId c0 = root.child(0);
    BoneId c1 = root.child(1);
    FATP_ASSERT_NE(c0, c1, "child(0) and child(1) must be distinct BoneIds");
    FATP_ASSERT_EQ(c0.depth(), c1.depth(), "Siblings must have the same depth");
    return true;
}

FATP_TEST_CASE(boneid_child_255_vs_other_distinct)
{
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
    BoneId c0   = root.child(0);
    BoneId c255 = root.child(255);
    FATP_ASSERT_NE(c0, c255, "child(0) and child(255) must be distinct");
    bool ordered = (c0 < c255) || (c255 < c0);
    FATP_ASSERT_TRUE(ordered, "child(0) and child(255) must be totally ordered");
    // Both must be children of root
    FATP_ASSERT_TRUE(root.isAncestorOf(c0),   "root must be ancestor of child(0)");
    FATP_ASSERT_TRUE(root.isAncestorOf(c255), "root must be ancestor of child(255)");
    return true;
}

FATP_TEST_CASE(boneid_value_bit_layout)
{
    // Verify the packing: level 0 occupies bits [63:56], level 1 [55:48], etc.
    // child(1) on a depth-1 bone with level value 1 places 1 at bits [55:48].
    constexpr BoneId d1 = Bone<TestSchema, Sys::Root>::id(); // level 0 = Sys::Root = 1
    BoneId d2 = d1.child(2); // level 1 = 2

    // Level 0 value must be 1 in bits [63:56]
    uint8_t level0 = static_cast<uint8_t>((d2.value() >> 56u) & 0xFFu);
    uint8_t level1 = static_cast<uint8_t>((d2.value() >> 48u) & 0xFFu);
    FATP_ASSERT_EQ(level0, uint8_t(1), "Level 0 must encode Sys::Root (value 1)");
    FATP_ASSERT_EQ(level1, uint8_t(2), "Level 1 must encode the child index 2");
    return true;
}

FATP_TEST_CASE(boneid_inactive_bytes_are_zero)
{
    // After a child() call, bytes at inactive levels must be zero.
    constexpr BoneId d2 = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    uint64_t v = d2.value();
    // Bytes at levels 2-7 (bits [47:0]) must all be zero.
    FATP_ASSERT_EQ(v & uint64_t(0x0000FFFFFFFFFFFFull), uint64_t(0),
        "Inactive level bytes (levels 2-7) must be zero for a depth-2 BoneId");
    return true;
}

FATP_TEST_CASE(boneid_ordering_strict_weak_order_reflexivity)
{
    // Irreflexivity: a BoneId must not be less than itself.
    constexpr BoneId id = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_FALSE(id < id, "A BoneId must not be less than itself");
    return true;
}

FATP_TEST_CASE(boneid_ordering_strict_weak_order_asymmetry)
{
    // Asymmetry: if a < b then !(b < a).
    constexpr BoneId a = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId b = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    FATP_ASSERT_TRUE(a < b, "Parent must be less than child");
    FATP_ASSERT_FALSE(b < a, "Child must not be less than parent (asymmetry)");
    return true;
}

FATP_TEST_CASE(boneid_ordering_strict_weak_order_transitivity)
{
    // Transitivity: if a < b and b < c then a < c.
    constexpr BoneId a = Bone<TestSchema, Sys::Root>::id();
    constexpr BoneId b = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    constexpr BoneId c = Bone<TestSchema, Sys::Root, Sub::Sensors, Chan::Load>::id();
    FATP_ASSERT_TRUE(a < b, "a < b");
    FATP_ASSERT_TRUE(b < c, "b < c");
    FATP_ASSERT_TRUE(a < c, "a < c (transitivity must hold)");
    return true;
}

FATP_TEST_CASE(boneid_serialize_all_depths)
{
    // Serialize and deserialize BoneIds at every valid depth (0-8).
    // Build depth-N by chaining child() calls from a depth-1 root.
    constexpr BoneId d1 = Bone<TestSchema, Sys::Root>::id();

    // depth 0: null
    {
        BoneId null;
        std::array<std::byte, 9> buf{};
        null.serialize(buf);
        BoneId restored = BoneId::deserialize(buf);
        FATP_ASSERT_EQ(restored, null, "Depth-0 (null) roundtrip must survive");
    }

    // depths 1-8
    BoneId cur = d1;
    for (uint8_t depth = 1; depth <= 8; ++depth)
    {
        if (depth > 1)
        {
            cur = cur.child(static_cast<uint8_t>(depth)); // distinct per level
        }
        std::array<std::byte, 9> buf{};
        cur.serialize(buf);
        BoneId restored = BoneId::deserialize(buf);
        FATP_ASSERT_EQ(restored, cur, "Serialize/deserialize roundtrip must hold at all depths");
        FATP_ASSERT_EQ(restored.depth(), depth, "Depth must survive roundtrip");
    }
    return true;
}

FATP_TEST_CASE(boneid_ancestor_chain_all_depths)
{
    // A depth-N BoneId must be an ancestor of every strictly deeper BoneId
    // that shares its prefix.
    constexpr BoneId d1 = Bone<TestSchema, Sys::Root>::id();
    BoneId chain[8];
    chain[0] = d1;
    for (int i = 1; i < 8; ++i)
    {
        chain[i] = chain[i - 1].child(static_cast<uint8_t>(i + 1));
    }

    for (int i = 0; i < 7; ++i)
    {
        for (int j = i + 1; j < 8; ++j)
        {
            FATP_ASSERT_TRUE(chain[i].isAncestorOf(chain[j]),
                "Each ancestor in the chain must be an ancestor of all deeper members");
            FATP_ASSERT_FALSE(chain[j].isAncestorOf(chain[i]),
                "Deeper member must not be ancestor of shallower member");
        }
    }
    return true;
}

FATP_TEST_CASE(boneid_non_prefix_is_not_ancestor)
{
    // A BoneId at the same depth with different last byte is NOT an ancestor.
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
    BoneId branchA = root.child(10);
    BoneId branchB = root.child(20);
    BoneId deepA   = branchA.child(5);

    FATP_ASSERT_FALSE(branchB.isAncestorOf(deepA),
        "A sibling branch must not be an ancestor of the other branch's child");
    FATP_ASSERT_FALSE(branchA.isAncestorOf(branchB),
        "A sibling must not be an ancestor of its sibling");
    return true;
}

FATP_TEST_CASE(boneid_hash_null_is_stable)
{
    // The null BoneId must always hash to the same value.
    std::hash<BoneId> h;
    BoneId null1;
    BoneId null2;
    FATP_ASSERT_EQ(h(null1), h(null2), "Null BoneIds must hash equally");
    return true;
}

FATP_TEST_CASE(boneid_to_string_each_depth)
{
    // toString must produce the right number of slash separators for each depth.
    constexpr BoneId d1 = Bone<TestSchema, Sys::Root>::id();
    BoneId cur = d1;
    for (uint8_t depth = 1; depth <= 8; ++depth)
    {
        if (depth > 1)
        {
            cur = cur.child(static_cast<uint8_t>(depth));
        }
        std::string s = cur.toString();
        std::size_t slashes = static_cast<std::size_t>(std::count(s.begin(), s.end(), '/'));
        FATP_ASSERT_EQ(slashes, static_cast<std::size_t>(depth - 1u),
            "toString must have exactly depth-1 slash separators");
        FATP_ASSERT_STARTS_WITH(s, std::string("["), "toString must start with '['");
        FATP_ASSERT_ENDS_WITH(s, std::string("]"), "toString must end with ']'");
    }
    return true;
}

// =============================================================================
// Suite 13: Mask edge cases
// =============================================================================

FATP_TEST_CASE(mask_all_32_bits_set)
{
    // Build a mask with all 32 capability bits set.
    SkeletonMask full;
    full.set(); // std::bitset::set() sets all bits
    FATP_ASSERT_EQ(full.count(), std::size_t(32), "Full mask must have 32 bits set");
    FATP_ASSERT_TRUE(full.all(), "Full mask must report all() == true");
    return true;
}

FATP_TEST_CASE(mask_required_and_excluded_overlap_matches_nothing)
{
    // If a bit appears in both required and excluded, no item can satisfy both.
    Skeleton sk;
    SkeletonMask sensorMask = makeMask(SkeletonCapability::Sensor);

    SensorItem sensor(sk, sensorMask, "sensor");

    // Ask for Sensor required AND Sensor excluded simultaneously -- impossible to satisfy.
    auto results = sk.query(sensorMask, sensorMask);
    FATP_ASSERT_EQ(results.size(), std::size_t(0),
        "Query with same bit in both required and excluded must match nothing");
    return true;
}

FATP_TEST_CASE(mask_excluded_superset_of_item_mask_matches_nothing)
{
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor, SkeletonCapability::Readable);
    SensorItem sensor(sk, m, "sensor");

    // Exclude more bits than the item has -- still excludes it.
    SkeletonMask superExclude;
    superExclude.set(); // all bits excluded
    auto results = sk.query({}, superExclude);
    FATP_ASSERT_EQ(results.size(), std::size_t(0),
        "Full exclusion mask must match nothing");
    return true;
}

FATP_TEST_CASE(mask_required_superset_of_any_item_matches_nothing)
{
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);
    SensorItem sensor(sk, m, "sensor");

    // Require all 32 bits -- item only has 1.
    SkeletonMask fullRequired;
    fullRequired.set();
    auto results = sk.query(fullRequired);
    FATP_ASSERT_EQ(results.size(), std::size_t(0),
        "Query requiring all 32 bits must not match an item with only 1 bit set");
    return true;
}

FATP_TEST_CASE(mask_item_with_all_bits_matches_any_required)
{
    Skeleton sk;
    SkeletonMask fullMask;
    fullMask.set();
    SensorItem sensor(sk, fullMask, "omnipotent");

    // Any non-empty required mask must match an item with all bits.
    SkeletonMask someRequired = makeMask(SkeletonCapability::Sensor, SkeletonCapability::Controller);
    auto results = sk.query(someRequired);
    FATP_ASSERT_EQ(results.size(), std::size_t(1),
        "An item with all 32 bits must match any required mask");
    return true;
}

FATP_TEST_CASE(mask_set_same_value_fires_signal)
{
    // setMask unconditionally replaces; signal must fire even if the mask is identical.
    Skeleton sk;
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);
    int callCount = 0;
    auto conn = sk.onMaskChanged([&](SkeletonItem&, SkeletonMask) { ++callCount; });

    ManualItem item(m, "item");
    item.doPublish(sk);
    item.doSetMask(m); // same value -- must still fire
    FATP_ASSERT_EQ(callCount, 1,
        "setMask to same value must still fire onMaskChanged");
    return true;
}

FATP_TEST_CASE(mask_rapid_repeated_changes)
{
    // Rapid setMask sequence: each call fires onMaskChanged exactly once.
    Skeleton sk;
    int callCount = 0;
    std::vector<SkeletonMask> capturedOldMasks;
    SkeletonMask capturedNewMaskInCallback;

    auto conn = sk.onMaskChanged([&](SkeletonItem& item, SkeletonMask oldMask)
    {
        ++callCount;
        capturedOldMasks.push_back(oldMask);
        capturedNewMaskInCallback = item.mask();
    });

    SkeletonMask m0 = makeMask(SkeletonCapability::Sensor);
    SkeletonMask m1 = makeMask(SkeletonCapability::Controller);
    SkeletonMask m2 = makeMask(SkeletonCapability::Readable);
    SkeletonMask m3 = makeMask(SkeletonCapability::Writable);

    ManualItem item(m0, "item");
    item.doPublish(sk);

    item.doSetMask(m1);
    item.doSetMask(m2);
    item.doSetMask(m3);

    FATP_ASSERT_EQ(callCount, 3, "Three setMask calls must fire signal exactly 3 times");
    FATP_ASSERT_EQ(capturedOldMasks[0], m0, "First old mask must be m0");
    FATP_ASSERT_EQ(capturedOldMasks[1], m1, "Second old mask must be m1");
    FATP_ASSERT_EQ(capturedOldMasks[2], m2, "Third old mask must be m2");
    FATP_ASSERT_EQ(item.mask(), m3, "Final mask must be m3");
    return true;
}

FATP_TEST_CASE(mask_empty_required_and_empty_excluded_matches_all)
{
    Skeleton sk;
    SysItem    sys(sk);
    SensorItem sensor(sk);
    LoadItem   load(sk);

    // Empty required and empty excluded: everything matches
    auto results = sk.query({}, {});
    FATP_ASSERT_EQ(results.size(), std::size_t(3),
        "Empty required + empty excluded must match all 3 items");
    return true;
}

// =============================================================================
// Suite 14: Multi-skeleton isolation
// =============================================================================

FATP_TEST_CASE(isolation_two_skeletons_independent)
{
    Skeleton sk1("sk1");
    Skeleton sk2("sk2");

    SysItem sys1(sk1, {}, "on_sk1");

    // Item on sk1 must not appear in sk2
    constexpr BoneId sysId = Bone<TestSchema, Sys::Root>::id();
    FATP_ASSERT_NOT_NULLPTR(sk1.find(sysId), "Item must be findable on its own skeleton");
    FATP_ASSERT_NULLPTR(sk2.find(sysId),    "Item must not appear on a different skeleton");
    FATP_ASSERT_EQ(sk1.size(), std::size_t(1), "sk1 must have 1 item");
    FATP_ASSERT_EQ(sk2.size(), std::size_t(0), "sk2 must be empty");
    return true;
}

FATP_TEST_CASE(isolation_same_boneid_on_two_skeletons_allowed)
{
    Skeleton sk1("sk1");
    Skeleton sk2("sk2");

    // Same BoneId on two different skeletons is allowed (not a duplicate).
    SysItem sys1(sk1, {}, "on_sk1");
    FATP_ASSERT_NO_THROW(
        [&]() { SysItem sys2(sk2, {}, "on_sk2"); (void)sys2; }(),
        "Same BoneId on a different skeleton must not throw"
    );
    return true;
}

FATP_TEST_CASE(isolation_signals_do_not_cross_skeletons)
{
    Skeleton sk1("sk1");
    Skeleton sk2("sk2");

    int count1 = 0;
    int count2 = 0;
    auto conn1 = sk1.onPublished([&](SkeletonItem&) { ++count1; });
    auto conn2 = sk2.onPublished([&](SkeletonItem&) { ++count2; });

    SysItem sys1(sk1);
    FATP_ASSERT_EQ(count1, 1, "sk1.onPublished must fire for item on sk1");
    FATP_ASSERT_EQ(count2, 0, "sk2.onPublished must NOT fire for item on sk1");
    return true;
}

FATP_TEST_CASE(isolation_query_scoped_to_own_skeleton)
{
    Skeleton sk1("sk1");
    Skeleton sk2("sk2");
    SkeletonMask m = makeMask(SkeletonCapability::Sensor);

    SensorItem s1(sk1, m, "s1");
    SensorItem s2(sk2, m, "s2"); // same BoneId, different skeleton

    auto r1 = sk1.query(m);
    auto r2 = sk2.query(m);
    FATP_ASSERT_EQ(r1.size(), std::size_t(1), "sk1.query must return only sk1's item");
    FATP_ASSERT_EQ(r2.size(), std::size_t(1), "sk2.query must return only sk2's item");
    FATP_ASSERT_NE(r1[0], r2[0], "Results must point to different item instances");
    return true;
}

// =============================================================================
// Suite 15: Signal edge cases
// =============================================================================

FATP_TEST_CASE(signal_reentrant_publish_from_on_published)
{
    // The API docs explicitly permit reentrant publish() from an onPublished callback.
    // Strategy: the outer item's onPublished callback publishes the ManualItem (Sys::Aux).
    // ManualItem has a distinct BoneId from the outer item (SensorItem = Sys::Root/Sensors).
    Skeleton sk;

    ManualItem inner;
    bool innerPublishedInsideCallback = false;

    auto conn = sk.onPublished([&](SkeletonItem& item)
    {
        // Fire only on the outer item to avoid infinite recursion.
        if (item.name() == std::string_view("outer") && !inner.isPublished())
        {
            try
            {
                inner.doPublish(sk);
                innerPublishedInsideCallback = true;
            }
            catch (...)
            {
                innerPublishedInsideCallback = false;
            }
        }
    });

    {
        SensorItem outer(sk, {}, "outer");
        FATP_ASSERT_TRUE(innerPublishedInsideCallback,
            "Reentrant publish from onPublished callback must succeed without throwing");
        FATP_ASSERT_EQ(sk.size(), std::size_t(2),
            "Both outer and inner must be in the registry after reentrant publish");

        constexpr BoneId auxId = Bone<TestSchema, Sys::Aux>::id();
        FATP_ASSERT_NOT_NULLPTR(sk.find(auxId),
            "Inner item must be findable by BoneId after reentrant publish");
    }
    // outer destroyed and unpublished; inner is still published (ManualItem needs explicit unpublish)
    FATP_ASSERT_EQ(sk.size(), std::size_t(1), "Only inner must remain after outer destruction");
    inner.doUnpublish();
    FATP_ASSERT_EQ(sk.size(), std::size_t(0), "Skeleton must be empty after cleanup");
    return true;
}

FATP_TEST_CASE(signal_on_unpublishing_item_still_findable)
{
    // During onUnpublishing, the item must still be findable.
    Skeleton sk;
    bool foundDuringUnpublish = false;
    BoneId capturedId;

    auto conn = sk.onUnpublishing([&](SkeletonItem& item)
    {
        capturedId = item.boneId();
        foundDuringUnpublish = (sk.find(item.boneId()) == &item);
    });

    {
        SensorItem sensor(sk);
    } // destroy triggers unpublish

    FATP_ASSERT_TRUE(foundDuringUnpublish,
        "Item must be findable in registry during onUnpublishing callback");
    return true;
}

FATP_TEST_CASE(signal_on_unpublishing_item_gone_after_callback)
{
    Skeleton sk;
    constexpr BoneId sensorId = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    {
        SensorItem sensor(sk);
        FATP_ASSERT_NOT_NULLPTR(sk.find(sensorId), "Item must be found before destruction");
    }
    FATP_ASSERT_NULLPTR(sk.find(sensorId),
        "Item must be gone from registry after onUnpublishing callbacks complete");
    return true;
}

FATP_TEST_CASE(signal_connect_then_immediately_disconnect)
{
    Skeleton sk;
    int callCount = 0;

    {
        auto conn = sk.onPublished([&](SkeletonItem&) { ++callCount; });
        // conn immediately destroyed -- auto-disconnect
    }

    SensorItem sensor(sk);
    FATP_ASSERT_EQ(callCount, 0,
        "Immediately-disconnected callback must never fire");
    return true;
}

FATP_TEST_CASE(signal_many_listeners_all_fire)
{
    Skeleton sk;
    constexpr int kListeners = 20;
    int counts[kListeners]{};
    std::vector<ScopedConnection> conns;
    conns.reserve(kListeners);

    for (int i = 0; i < kListeners; ++i)
    {
        int* cptr = &counts[i];
        conns.push_back(sk.onPublished([cptr](SkeletonItem&) { ++(*cptr); }));
    }

    SensorItem sensor(sk);

    for (int i = 0; i < kListeners; ++i)
    {
        FATP_ASSERT_EQ(counts[i], 1, "Every listener must fire exactly once");
    }
    return true;
}

FATP_TEST_CASE(signal_listener_removed_while_others_remain)
{
    Skeleton sk;
    int countA = 0;
    int countB = 0;
    int countC = 0;

    auto connA = sk.onPublished([&](SkeletonItem&) { ++countA; });
    {
        auto connB = sk.onPublished([&](SkeletonItem&) { ++countB; });
        (void)connB;
        // connB destroyed -- only A and C remain
    }
    auto connC = sk.onPublished([&](SkeletonItem&) { ++countC; });

    SensorItem sensor(sk);

    FATP_ASSERT_EQ(countA, 1, "Listener A must fire");
    FATP_ASSERT_EQ(countB, 0, "Listener B (disconnected) must not fire");
    FATP_ASSERT_EQ(countC, 1, "Listener C must fire");
    return true;
}

FATP_TEST_CASE(signal_on_mask_changed_sequence_old_masks_correct)
{
    // Verify the old mask delivered to each callback is exactly the mask before that call.
    Skeleton sk;
    std::vector<std::pair<SkeletonMask, SkeletonMask>> changes; // {old, new}

    auto conn = sk.onMaskChanged([&](SkeletonItem& item, SkeletonMask oldMask)
    {
        changes.emplace_back(oldMask, item.mask());
    });

    SkeletonMask masks[5];
    masks[0] = makeMask(SkeletonCapability::Sensor);
    masks[1] = makeMask(SkeletonCapability::Controller);
    masks[2] = {};
    masks[3] = makeMask(SkeletonCapability::Readable, SkeletonCapability::Writable);
    masks[4] = makeMask(SkeletonCapability::ProvidesValue);

    ManualItem item(masks[0], "item");
    item.doPublish(sk);

    for (int i = 1; i < 5; ++i)
    {
        item.doSetMask(masks[i]);
    }

    FATP_ASSERT_EQ(changes.size(), std::size_t(4), "Must have 4 mask change events");
    for (int i = 0; i < 4; ++i)
    {
        FATP_ASSERT_EQ(changes[static_cast<std::size_t>(i)].first,  masks[i],
            "Old mask must be the mask before this change");
        FATP_ASSERT_EQ(changes[static_cast<std::size_t>(i)].second, masks[i + 1],
            "New mask must be the mask after this change");
    }
    return true;
}

FATP_TEST_CASE(signal_priority_high_fires_before_default)
{
    Skeleton sk;
    std::vector<std::string> order;

    auto connDefault = sk.onPublished([&](SkeletonItem&) { order.push_back("default"); }, 0);
    auto connHigh    = sk.onPublished([&](SkeletonItem&) { order.push_back("high"); },    100);
    auto connLow     = sk.onPublished([&](SkeletonItem&) { order.push_back("low"); },    -100);

    SensorItem sensor(sk);

    FATP_ASSERT_EQ(order.size(), std::size_t(3), "All 3 listeners must fire");
    FATP_ASSERT_EQ(order[0], std::string("high"),    "High priority fires first");
    FATP_ASSERT_EQ(order[1], std::string("default"), "Default fires second");
    FATP_ASSERT_EQ(order[2], std::string("low"),     "Low fires last");
    return true;
}

FATP_TEST_CASE(signal_publish_count_matches_items)
{
    // Total signal firings must equal total publish/unpublish calls.
    // Use a single ManualItem cycled 30 times.
    Skeleton sk;
    int publishCount   = 0;
    int unpublishCount = 0;
    auto connP = sk.onPublished([&](SkeletonItem&) { ++publishCount; });
    auto connU = sk.onUnpublishing([&](SkeletonItem&) { ++unpublishCount; });

    const int kN = 30;
    ManualItem item;
    for (int i = 0; i < kN; ++i)
    {
        item.doPublish(sk);
        item.doUnpublish();
    }

    FATP_ASSERT_EQ(publishCount,   kN, "onPublished must fire exactly once per publish");
    FATP_ASSERT_EQ(unpublishCount, kN, "onUnpublishing must fire exactly once per unpublish");
    FATP_ASSERT_EQ(sk.size(), std::size_t(0), "Skeleton must be empty after all cycles");
    return true;
}

// =============================================================================
// Suite 16: visitSubtree and query corner cases
// =============================================================================

FATP_TEST_CASE(visit_subtree_null_root_visits_nothing)
{
    // visitSubtree with a null BoneId should visit nothing:
    // null.isAncestorOf(x) is false for all x, and null != any non-null id.
    Skeleton sk;
    SensorItem sensor(sk);
    LoadItem   load(sk);

    int count = 0;
    sk.visitSubtree(BoneId{}, [&](SkeletonItem&) { ++count; });
    FATP_ASSERT_EQ(count, 0, "visitSubtree with null root must visit 0 items");
    return true;
}

FATP_TEST_CASE(visit_subtree_sibling_not_visited)
{
    Skeleton sk;
    SysItem      sys(sk);
    SensorItem   sensors(sk);
    ActuatorItem actuators(sk);
    LoadItem     load(sk);

    // Visit only the Sensors subtree -- Actuators branch must not appear.
    constexpr BoneId sensorsRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();

    std::vector<BoneId> visited;
    sk.visitSubtree(sensorsRoot, [&](SkeletonItem& item)
    {
        visited.push_back(item.boneId());
    });

    bool actuatorsVisited = std::any_of(visited.begin(), visited.end(),
        [](const BoneId& id) { return id == Bone<TestSchema, Sys::Root, Sub::Actuators>::id(); });
    FATP_ASSERT_FALSE(actuatorsVisited, "Sibling branch must not appear in subtree visit");
    return true;
}

FATP_TEST_CASE(query_subtree_null_root_returns_nothing)
{
    Skeleton sk;
    SensorItem sensor(sk);

    auto results = sk.querySubtree(BoneId{}, {});
    FATP_ASSERT_EQ(results.size(), std::size_t(0),
        "querySubtree with null root must return empty result");
    return true;
}

FATP_TEST_CASE(query_subtree_absent_root_still_returns_descendants_if_any)
{
    // If the subtree root itself is not published, descendants that exist must still be returned.
    Skeleton sk;
    // Publish Load but NOT Sensors (the parent).
    LoadItem load(sk, {}, "load");
    TempItem temp(sk, {}, "temp");

    constexpr BoneId sensorsRoot = Bone<TestSchema, Sys::Root, Sub::Sensors>::id();
    auto results = sk.querySubtree(sensorsRoot, {});

    // Should include load and temp (they are descendants of sensors root), but NOT sensors itself.
    FATP_ASSERT_EQ(results.size(), std::size_t(2),
        "querySubtree must return descendants even when root itself is not published");
    for (auto* item : results)
    {
        FATP_ASSERT_TRUE(
            item->boneId() == sensorsRoot || sensorsRoot.isAncestorOf(item->boneId()),
            "All results must be within the queried subtree"
        );
    }
    return true;
}

FATP_TEST_CASE(query_reverse_order_child_before_parent)
{
    // Callers needing child-before-parent order must iterate in reverse.
    Skeleton sk;
    SysItem    sys(sk);
    SensorItem sensor(sk);
    LoadItem   load(sk);

    auto results = sk.query({});
    // Forward: parent before child
    FATP_ASSERT_TRUE(std::is_sorted(results.begin(), results.end(),
        [](SkeletonItem* a, SkeletonItem* b) { return a->boneId() < b->boneId(); }),
        "query() results must be in ascending BoneId order");

    // Reverse: child before parent
    bool reverseIsSorted = std::is_sorted(results.rbegin(), results.rend(),
        [](SkeletonItem* a, SkeletonItem* b) { return a->boneId() < b->boneId(); });
    FATP_ASSERT_FALSE(reverseIsSorted,
        "Reversing must yield descending (child-before-parent) order when multiple items present");
    return true;
}

FATP_TEST_CASE(query_mixed_required_and_excluded_precise)
{
    // Items: A has {Sensor, Readable}, B has {Sensor}, C has {Readable}, D has {}
    Skeleton sk;
    SkeletonMask sensorAndReadable = makeMask(SkeletonCapability::Sensor, SkeletonCapability::Readable);
    SkeletonMask sensorOnly        = makeMask(SkeletonCapability::Sensor);
    SkeletonMask readableOnly      = makeMask(SkeletonCapability::Readable);
    SkeletonMask none              = {};

    SysItem      a(sk, sensorAndReadable, "a");
    SensorItem   b(sk, sensorOnly,        "b");
    ActuatorItem c(sk, readableOnly,      "c");
    LoadItem     d(sk, none,              "d");

    // Required: Sensor. Excluded: Readable.
    // A has Sensor but also Readable (excluded) -> no
    // B has Sensor, no Readable -> yes
    // C has no Sensor -> no
    // D has nothing -> no
    SkeletonMask reqSensor = makeMask(SkeletonCapability::Sensor);
    SkeletonMask excReadable = makeMask(SkeletonCapability::Readable);
    auto results = sk.query(reqSensor, excReadable);
    FATP_ASSERT_EQ(results.size(), std::size_t(1),
        "Only item B (Sensor only, no Readable) must match");
    FATP_ASSERT_EQ(results[0]->name(), std::string_view("b"),
        "The matching item must be item B");
    return true;
}

// =============================================================================
// Suite 17: Fuzz / stress
// =============================================================================

FATP_TEST_CASE(fuzz_boneid_child_parent_roundtrip_random)
{
    // For random depths and index values, child()->parent() must always recover the original.
    std::mt19937 rng(0xDEADBEEFu);
    std::uniform_int_distribution<uint32_t> idxDist(0, 255);

    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();

    for (int iteration = 0; iteration < 500; ++iteration)
    {
        BoneId cur = root;
        // Build up to depth 7 (max = 8, root is 1, so 7 more steps)
        int steps = static_cast<int>(rng() % 7 + 1);
        std::vector<uint8_t> indices;
        indices.reserve(static_cast<std::size_t>(steps));

        for (int s = 0; s < steps; ++s)
        {
            uint8_t idx = static_cast<uint8_t>(idxDist(rng));
            indices.push_back(idx);
            cur = cur.child(idx);
        }

        // Walk back
        BoneId walking = cur;
        for (int s = steps - 1; s >= 0; --s)
        {
            BoneId up = walking.parent();
            // Verify child() in the forward direction from up gives walking
            BoneId rebuilt = up.child(indices[static_cast<std::size_t>(s)]);
            FATP_ASSERT_EQ(rebuilt, walking, "child(parent()->index) roundtrip must hold");
            walking = up;
        }
        FATP_ASSERT_EQ(walking, root, "Full walk back must recover root BoneId");
    }
    return true;
}

FATP_TEST_CASE(fuzz_boneid_ancestor_structural_invariant)
{
    // For any chain A -> B -> C (A is parent of B, B is parent of C):
    // - A.isAncestorOf(B) == true
    // - A.isAncestorOf(C) == true
    // - B.isAncestorOf(A) == false
    // - C.isAncestorOf(A) == false
    // - C.isAncestorOf(B) == false
    std::mt19937 rng(0xCAFEBABEu);
    std::uniform_int_distribution<uint32_t> idxDist(0, 255);

    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();

    for (int i = 0; i < 300; ++i)
    {
        uint8_t idxB = static_cast<uint8_t>(idxDist(rng));
        uint8_t idxC = static_cast<uint8_t>(idxDist(rng));

        BoneId A = root;
        BoneId B = A.child(idxB);
        BoneId C = B.child(idxC);

        FATP_ASSERT_TRUE(A.isAncestorOf(B),  "A must be ancestor of B");
        FATP_ASSERT_TRUE(A.isAncestorOf(C),  "A must be ancestor of C");
        FATP_ASSERT_FALSE(B.isAncestorOf(A), "B must not be ancestor of A");
        FATP_ASSERT_FALSE(C.isAncestorOf(A), "C must not be ancestor of A");
        FATP_ASSERT_FALSE(C.isAncestorOf(B), "C must not be ancestor of B");
        FATP_ASSERT_FALSE(A.isAncestorOf(A), "A must not be ancestor of itself");
    }
    return true;
}

FATP_TEST_CASE(fuzz_boneid_sibling_never_ancestor)
{
    // Two children of the same parent with different indices are never ancestors of each other.
    std::mt19937 rng(0xFACEFEEDu);
    std::uniform_int_distribution<uint32_t> idxDist(0, 254); // 0-254 so +1 is always distinct

    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();

    for (int i = 0; i < 300; ++i)
    {
        uint8_t idxA = static_cast<uint8_t>(idxDist(rng));
        uint8_t idxB = static_cast<uint8_t>(idxA + 1u); // guaranteed distinct

        BoneId sibA = root.child(idxA);
        BoneId sibB = root.child(idxB);

        FATP_ASSERT_FALSE(sibA.isAncestorOf(sibB), "Sibling A must not be ancestor of B");
        FATP_ASSERT_FALSE(sibB.isAncestorOf(sibA), "Sibling B must not be ancestor of A");
        FATP_ASSERT_NE(sibA, sibB, "Siblings with different indices must be unequal");
    }
    return true;
}

FATP_TEST_CASE(fuzz_serialize_deserialize_all_depths_random_values)
{
    // For each depth 0-8, for many random index combinations, serialize and deserialize
    // must produce an equal BoneId.
    std::mt19937 rng(0xBEEFC0DEu);
    std::uniform_int_distribution<uint32_t> idxDist(0, 255);

    for (int depth = 0; depth <= 8; ++depth)
    {
        for (int trial = 0; trial < 100; ++trial)
        {
            // Build a BoneId of the target depth.
            BoneId cur; // depth 0 = null
            if (depth > 0)
            {
                constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();
                cur = root;
                for (int d = 1; d < depth; ++d)
                {
                    cur = cur.child(static_cast<uint8_t>(idxDist(rng)));
                }
            }

            std::array<std::byte, 9> buf{};
            cur.serialize(buf);
            BoneId restored = BoneId::deserialize(buf);

            FATP_ASSERT_EQ(restored, cur,
                "Serialize/deserialize roundtrip must be lossless at every depth");
            FATP_ASSERT_EQ(restored.depth(), cur.depth(),
                "Depth must survive serialize/deserialize roundtrip");
        }
    }
    return true;
}

FATP_TEST_CASE(fuzz_query_matches_reference_oracle)
{
    // Publish N items with random masks. For random required/excluded pairs,
    // verify that query() results match a brute-force reference implementation.
    std::mt19937 rng(0xABCDEF01u);
    std::uniform_int_distribution<uint32_t> maskDist(0, 0xFFFFFFFFu);

    Skeleton sk;

    // Build 6 items (max we have unique BoneIds for in this schema)
    struct ItemDesc { SkeletonMask mask; std::string name; };
    ItemDesc descs[6];
    descs[0] = { SkeletonMask(static_cast<unsigned long long>(maskDist(rng))), "sys" };
    descs[1] = { SkeletonMask(static_cast<unsigned long long>(maskDist(rng))), "sensor" };
    descs[2] = { SkeletonMask(static_cast<unsigned long long>(maskDist(rng))), "actuator" };
    descs[3] = { SkeletonMask(static_cast<unsigned long long>(maskDist(rng))), "load" };
    descs[4] = { SkeletonMask(static_cast<unsigned long long>(maskDist(rng))), "temp" };
    descs[5] = { SkeletonMask(static_cast<unsigned long long>(maskDist(rng))), "pressure" };

    SysItem      i0(sk, descs[0].mask, descs[0].name);
    SensorItem   i1(sk, descs[1].mask, descs[1].name);
    ActuatorItem i2(sk, descs[2].mask, descs[2].name);
    LoadItem     i3(sk, descs[3].mask, descs[3].name);
    TempItem     i4(sk, descs[4].mask, descs[4].name);
    PressureItem i5(sk, descs[5].mask, descs[5].name);

    SkeletonItem* allItems[6] = { &i0, &i1, &i2, &i3, &i4, &i5 };

    for (int trial = 0; trial < 200; ++trial)
    {
        SkeletonMask required(maskDist(rng));
        SkeletonMask excluded(maskDist(rng));

        // Reference: brute-force O(N) with same predicate
        std::vector<SkeletonItem*> reference;
        for (auto* item : allItems)
        {
            if ((item->mask() & required) == required &&
                (item->mask() & excluded).none())
            {
                reference.push_back(item);
            }
        }
        std::sort(reference.begin(), reference.end(),
            [](SkeletonItem* a, SkeletonItem* b) { return a->boneId() < b->boneId(); });

        auto results = sk.query(required, excluded);
        FATP_ASSERT_EQ(results.size(), reference.size(),
            "query() result count must match brute-force reference");
        for (std::size_t i = 0; i < results.size(); ++i)
        {
            FATP_ASSERT_EQ(results[i], reference[i],
                "query() result items must match brute-force reference in order");
        }
    }
    return true;
}

FATP_TEST_CASE(fuzz_publish_unpublish_size_invariant)
{
    // Interleave publish and unpublish of a single item randomly many times.
    // At every step, sk.size() must be either 0 or 1, never anything else.
    Skeleton sk;
    std::mt19937 rng(0x12345678u);
    std::uniform_int_distribution<int> coinFlip(0, 1);

    ManualItem item;
    bool published = false;

    for (int i = 0; i < 500; ++i)
    {
        if (coinFlip(rng) == 0)
        {
            if (!published)
            {
                item.doPublish(sk);
                published = true;
            }
        }
        else
        {
            if (published)
            {
                item.doUnpublish();
                published = false;
            }
        }

        std::size_t expected = published ? 1u : 0u;
        FATP_ASSERT_EQ(sk.size(), expected,
            "Skeleton size must be 0 or 1 matching publish state");
        FATP_ASSERT_EQ(item.isPublished(), published,
            "isPublished() must match actual publish state");
    }

    // Clean up
    if (published)
    {
        item.doUnpublish();
    }
    return true;
}

FATP_TEST_CASE(fuzz_signal_counts_match_publish_unpublish_operations)
{
    // Random sequence of publish/unpublish operations on multiple items.
    // Total onPublished fires must equal total publish calls;
    // total onUnpublishing fires must equal total unpublish calls.
    Skeleton sk;
    int publishFires   = 0;
    int unpublishFires = 0;
    auto connP = sk.onPublished([&](SkeletonItem&) { ++publishFires; });
    auto connU = sk.onUnpublishing([&](SkeletonItem&) { ++unpublishFires; });

    std::mt19937 rng(0xDEADC0DEu);
    std::uniform_int_distribution<int> coinFlip(0, 1);

    // Use a single unique-BoneId item (ManualItem = Sys::Aux) to avoid duplicates
    ManualItem item;
    bool published = false;
    int expectedPublish   = 0;
    int expectedUnpublish = 0;

    for (int i = 0; i < 300; ++i)
    {
        if (coinFlip(rng) == 0)
        {
            if (!published)
            {
                item.doPublish(sk);
                published = true;
                ++expectedPublish;
            }
        }
        else
        {
            if (published)
            {
                item.doUnpublish();
                published = false;
                ++expectedUnpublish;
            }
        }
    }
    if (published) { item.doUnpublish(); ++expectedUnpublish; }

    FATP_ASSERT_EQ(publishFires, expectedPublish,
        "onPublished fire count must match total publish operations");
    FATP_ASSERT_EQ(unpublishFires, expectedUnpublish,
        "onUnpublishing fire count must match total unpublish operations");
    return true;
}

FATP_TEST_CASE(fuzz_query_subtree_subset_of_global_query)
{
    // Property: for any subtree root R and any mask (req, excl),
    // querySubtree(R, req, excl) must be a subset of query(req, excl).
    Skeleton sk;
    std::mt19937 rng(0x0FACADE0u);
    std::uniform_int_distribution<uint32_t> maskDist(0, 0xFFFFFFFFu);

    SkeletonMask masks[6];
    for (auto& m : masks) { m = SkeletonMask(static_cast<unsigned long long>(maskDist(rng))); }

    SysItem      i0(sk, masks[0]);
    SensorItem   i1(sk, masks[1]);
    ActuatorItem i2(sk, masks[2]);
    LoadItem     i3(sk, masks[3]);
    TempItem     i4(sk, masks[4]);
    PressureItem i5(sk, masks[5]);

    // Candidate subtree roots to test
    BoneId roots[] = {
        Bone<TestSchema, Sys::Root>::id(),
        Bone<TestSchema, Sys::Root, Sub::Sensors>::id(),
        Bone<TestSchema, Sys::Root, Sub::Actuators>::id(),
        BoneId{}, // null
    };

    for (int trial = 0; trial < 100; ++trial)
    {
        SkeletonMask required(maskDist(rng));
        SkeletonMask excluded(maskDist(rng));

        auto global = sk.query(required, excluded);

        for (auto root : roots)
        {
            auto subtree = sk.querySubtree(root, required, excluded);

            // Every subtree result must appear in the global result.
            for (auto* item : subtree)
            {
                bool inGlobal = std::any_of(global.begin(), global.end(),
                    [item](SkeletonItem* g) { return g == item; });
                FATP_ASSERT_TRUE(inGlobal,
                    "Every querySubtree result must appear in query() result");
            }

            FATP_ASSERT_LE(subtree.size(), global.size(),
                "querySubtree result must be no larger than global query");
        }
    }
    return true;
}

FATP_TEST_CASE(fuzz_hash_no_collision_for_distinct_simple_ids)
{
    // For every pair of distinct BoneIds constructible from a small value space,
    // check that they are not equal (they could hash-collide, but they must not compare equal).
    // This validates the canonical-form invariant used by Skeleton::publish().
    std::hash<BoneId> h;
    constexpr BoneId root = Bone<TestSchema, Sys::Root>::id();

    // Build 50 BoneIds at depth 2 with indices 0-49
    std::vector<BoneId> ids;
    ids.reserve(50);
    for (uint8_t i = 0; i < 50; ++i)
    {
        ids.push_back(root.child(i));
    }

    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        for (std::size_t j = i + 1; j < ids.size(); ++j)
        {
            FATP_ASSERT_NE(ids[i], ids[j],
                "Distinct child BoneIds must not compare equal");
            // Also verify they don't produce the same hash (if they did, the hash function
            // would be catastrophically broken for this small value space).
            FATP_ASSERT_NE(h(ids[i]), h(ids[j]),
                "Distinct simple BoneIds must have distinct hashes");
        }
    }
    return true;
}

FATP_TEST_CASE(fuzz_mask_query_all_combinations_exhaustive)
{
    // Exhaustively test all (required, excluded) combinations for a small 4-bit mask space.
    // Use only the first 4 SkeletonCapability bits to keep the space manageable.
    // 4 items, each with a distinct combination of the 4 bits -> 16 items needed,
    // but we only have 6 unique BoneIds. Use 4.

    Skeleton sk;
    // 4 items with all 4-bit combinations: 0b0000=0, 0b0001=1, 0b0011=3, 0b1111=15
    // Using SkeletonCapability bits 0-3 (Sensor=0, Controller=1, Readable=2, Writable=3)
    auto bit = [](SkeletonCapability c) { return static_cast<std::size_t>(c); };

    SkeletonMask m0; // 0b0000
    SkeletonMask m1; m1.set(bit(SkeletonCapability::Sensor));
    SkeletonMask m2; m2.set(bit(SkeletonCapability::Sensor)); m2.set(bit(SkeletonCapability::Controller));
    SkeletonMask m3; m3.set(bit(SkeletonCapability::Sensor)); m3.set(bit(SkeletonCapability::Controller));
                     m3.set(bit(SkeletonCapability::Readable)); m3.set(bit(SkeletonCapability::Writable));

    SysItem      i0(sk, m0, "i0");
    SensorItem   i1(sk, m1, "i1");
    ActuatorItem i2(sk, m2, "i2");
    LoadItem     i3(sk, m3, "i3");

    SkeletonItem* items[4] = { &i0, &i1, &i2, &i3 };
    SkeletonMask  masks[4] = { m0, m1, m2, m3 };

    // Test all 16 required x 16 excluded combinations (using only 4-bit variants)
    for (uint32_t reqBits = 0; reqBits < 16u; ++reqBits)
    {
        for (uint32_t exclBits = 0; exclBits < 16u; ++exclBits)
        {
            SkeletonMask required;
            SkeletonMask excluded;
            for (uint32_t b = 0; b < 4; ++b)
            {
                if ((reqBits  >> b) & 1u) { required.set(b); }
                if ((exclBits >> b) & 1u) { excluded.set(b); }
            }

            // Reference: brute-force
            std::vector<SkeletonItem*> reference;
            for (int k = 0; k < 4; ++k)
            {
                if ((masks[k] & required) == required && (masks[k] & excluded).none())
                {
                    reference.push_back(items[k]);
                }
            }
            std::sort(reference.begin(), reference.end(),
                [](SkeletonItem* a, SkeletonItem* b) { return a->boneId() < b->boneId(); });

            auto results = sk.query(required, excluded);
            FATP_ASSERT_EQ(results.size(), reference.size(),
                "Exhaustive mask test: result count must match reference");
            for (std::size_t idx = 0; idx < results.size(); ++idx)
            {
                FATP_ASSERT_EQ(results[idx], reference[idx],
                    "Exhaustive mask test: items must match reference in order");
            }
        }
    }
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

    // BoneId boundary and structural properties
    out << "\n--- BoneId boundary ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, boneid_max_depth_chain);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_max_depth_parent_chain);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_child_with_zero_index);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_child_with_max_index);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_child_zero_vs_child_one_are_distinct);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_child_255_vs_other_distinct);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_value_bit_layout);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_inactive_bytes_are_zero);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_ordering_strict_weak_order_reflexivity);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_ordering_strict_weak_order_asymmetry);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_ordering_strict_weak_order_transitivity);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_serialize_all_depths);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_ancestor_chain_all_depths);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_non_prefix_is_not_ancestor);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_hash_null_is_stable);
    FATP_RUN_TEST_NS(runner, skeleton, boneid_to_string_each_depth);

    // Mask edge cases
    out << "\n--- Mask edge cases ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, mask_all_32_bits_set);
    FATP_RUN_TEST_NS(runner, skeleton, mask_required_and_excluded_overlap_matches_nothing);
    FATP_RUN_TEST_NS(runner, skeleton, mask_excluded_superset_of_item_mask_matches_nothing);
    FATP_RUN_TEST_NS(runner, skeleton, mask_required_superset_of_any_item_matches_nothing);
    FATP_RUN_TEST_NS(runner, skeleton, mask_item_with_all_bits_matches_any_required);
    FATP_RUN_TEST_NS(runner, skeleton, mask_set_same_value_fires_signal);
    FATP_RUN_TEST_NS(runner, skeleton, mask_rapid_repeated_changes);
    FATP_RUN_TEST_NS(runner, skeleton, mask_empty_required_and_empty_excluded_matches_all);

    // Multi-skeleton isolation
    out << "\n--- Multi-skeleton isolation ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, isolation_two_skeletons_independent);
    FATP_RUN_TEST_NS(runner, skeleton, isolation_same_boneid_on_two_skeletons_allowed);
    FATP_RUN_TEST_NS(runner, skeleton, isolation_signals_do_not_cross_skeletons);
    FATP_RUN_TEST_NS(runner, skeleton, isolation_query_scoped_to_own_skeleton);

    // Signal edge cases
    out << "\n--- Signal edge cases ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, signal_reentrant_publish_from_on_published);
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_unpublishing_item_still_findable);
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_unpublishing_item_gone_after_callback);
    FATP_RUN_TEST_NS(runner, skeleton, signal_connect_then_immediately_disconnect);
    FATP_RUN_TEST_NS(runner, skeleton, signal_many_listeners_all_fire);
    FATP_RUN_TEST_NS(runner, skeleton, signal_listener_removed_while_others_remain);
    FATP_RUN_TEST_NS(runner, skeleton, signal_on_mask_changed_sequence_old_masks_correct);
    FATP_RUN_TEST_NS(runner, skeleton, signal_priority_high_fires_before_default);
    FATP_RUN_TEST_NS(runner, skeleton, signal_publish_count_matches_items);

    // visitSubtree / query corner cases
    out << "\n--- Traversal / query corner cases ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, visit_subtree_null_root_visits_nothing);
    FATP_RUN_TEST_NS(runner, skeleton, visit_subtree_sibling_not_visited);
    FATP_RUN_TEST_NS(runner, skeleton, query_subtree_null_root_returns_nothing);
    FATP_RUN_TEST_NS(runner, skeleton, query_subtree_absent_root_still_returns_descendants_if_any);
    FATP_RUN_TEST_NS(runner, skeleton, query_reverse_order_child_before_parent);
    FATP_RUN_TEST_NS(runner, skeleton, query_mixed_required_and_excluded_precise);

    // Fuzz / stress
    out << "\n--- Fuzz / stress ---\n";
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_boneid_child_parent_roundtrip_random);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_boneid_ancestor_structural_invariant);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_boneid_sibling_never_ancestor);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_serialize_deserialize_all_depths_random_values);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_query_matches_reference_oracle);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_publish_unpublish_size_invariant);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_signal_counts_match_publish_unpublish_operations);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_query_subtree_subset_of_global_query);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_hash_no_collision_for_distinct_simple_ids);
    FATP_RUN_TEST_NS(runner, skeleton, fuzz_mask_query_all_combinations_exhaustive);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Skeleton() ? 0 : 1;
}
#endif
