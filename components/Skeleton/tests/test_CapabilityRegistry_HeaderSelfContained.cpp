/**
 * @file test_CapabilityRegistry_HeaderSelfContained.cpp
 * @brief Compile-only header self-contained test for CapabilityRegistry.h.
 *
 * Verifies that CapabilityRegistry.h:
 * - Compiles when included first in an otherwise empty translation unit.
 * - Does not rely on Skeleton.h or any other Fat-P header being included first.
 * - Is idempotent (double-include via pragma once produces no errors).
 *
 * CapabilityRegistry.h is the name -> capability-index registry that opens the
 * capability vocabulary: the framework band (SkeletonCapability, indices 0-31)
 * is pre-registered; applications register names and receive indices from 32
 * upward. It must be usable with only SkeletonFwd.h in its include graph.
 *
 * No test framework is used. Compilation is the pass criterion (main also
 * exercises the API minimally).
 *
 * If this file fails to compile, the fix belongs in CapabilityRegistry.h --
 * never in this test file.
 */
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: header_self_contained_test
  path: components/Skeleton/tests/test_CapabilityRegistry_HeaderSelfContained.cpp
  layer: Testing
  namespace: fat_p
  summary: "Compile-only self-containment check for CapabilityRegistry.h"
  api_stability: in_work
  related:
    headers:
      - include/fat_p/CapabilityRegistry.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

// First include: target header must stand alone.
#include "CapabilityRegistry.h"

// Second include: validates pragma once idempotence.
#include "CapabilityRegistry.h"

int main()
{
    using namespace fat_p::skeleton;

    auto& reg = CapabilityRegistry::instance();

    // Framework names are pre-registered at their enum indices.
    const auto sensor = reg.find("Sensor");
    if (!sensor.has_value() ||
        *sensor != static_cast<std::size_t>(SkeletonCapability::Sensor))
    {
        return 1;
    }

    // Application registration allocates above the framework band and is
    // idempotent by name.
    const std::size_t a = reg.registerCapability("SelfContained.A");
    if (a < kFrameworkCapabilityBand ||
        reg.registerCapability("SelfContained.A") != a)
    {
        return 1;
    }

    // Reverse lookup and high-water mark.
    if (reg.name(a) != "SelfContained.A" || reg.highWater() <= a)
    {
        return 1;
    }

    // Registered indices compose into masks alongside framework bits.
    const SkeletonMask m = makeMask(SkeletonCapability::ProvidesValue, a);
    return (m.test(a) && m.count() == 2) ? 0 : 1;
}
