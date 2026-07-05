/**
 * @file test_SkeletonFwd_HeaderSelfContained.cpp
 * @brief Compile-only header self-contained test for SkeletonFwd.h.
 *
 * Verifies that SkeletonFwd.h:
 * - Compiles when included first in an otherwise empty translation unit.
 * - Does not rely on Skeleton.h or any other Fat-P header being included first.
 * - Is idempotent (double-include via pragma once produces no errors).
 *
 * SkeletonFwd.h is the lightweight header for sharing BoneId values,
 * HierarchySchema definitions, and capability masks across translation units
 * without pulling in the full Skeleton machinery. It must be usable without
 * Skeleton.h in the include graph.
 *
 * No test framework is used. Compilation is the pass criterion.
 *
 * If this file fails to compile, the fix belongs in SkeletonFwd.h -- never
 * in this test file.
 */
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: header_self_contained_test
  path: components/Skeleton/tests/test_SkeletonFwd_HeaderSelfContained.cpp
  layer: Testing
  namespace: fat_p
  summary: "Compile-only self-containment check for SkeletonFwd.h"
  api_stability: in_work
  related:
    headers:
      - include/fat_p/SkeletonFwd.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

// First include: target header must stand alone -- no Skeleton.h before it.
#include "SkeletonFwd.h"

// Second include: validates pragma once idempotence.
#include "SkeletonFwd.h"

int main()
{
    // Exercise each type exported by SkeletonFwd.h to ensure they are
    // complete and usable without Skeleton.h.

    // BoneId construction and basic operations.
    fat_p::skeleton::BoneId null;
    (void)null.isNull();
    (void)null.depth();
    (void)null.value();

    // HierarchySchema instantiation.
    enum class Sys : uint8_t { Root = 1 };
    enum class Sub : uint8_t { Sensors = 1 };
    using Schema = fat_p::skeleton::HierarchySchema<Sys, Sub>;
    static_assert(Schema::kMaxDepth == 2, "kMaxDepth must be 2 for a 2-level schema");

    // SkeletonMask construction and makeMask.
    fat_p::skeleton::SkeletonMask m = fat_p::skeleton::makeMask(
        fat_p::skeleton::SkeletonCapability::Sensor
    );
    (void)m.count();

    // BoneId navigation (child/parent).
    fat_p::skeleton::BoneId d1 = null.child(1);
    fat_p::skeleton::BoneId d0 = d1.parent();
    (void)d0;

    // BoneId hierarchy queries.
    fat_p::skeleton::BoneId d2 = d1.child(2);
    (void)d1.isAncestorOf(d2);
    (void)d2.isDescendantOf(d1);

    // BoneId serialization.
    std::array<std::byte, 9> buf{};
    d2.serialize(buf);
    fat_p::skeleton::BoneId restored = fat_p::skeleton::BoneId::deserialize(buf);
    (void)restored;

    // BoneId toString (allocates -- just verify it compiles).
    std::string s = d2.toString();
    (void)s;

    // std::hash specialization.
    std::hash<fat_p::skeleton::BoneId> h;
    (void)h(d2);

    return 0;
}
