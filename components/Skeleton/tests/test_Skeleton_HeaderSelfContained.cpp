/**
 * @file test_Skeleton_HeaderSelfContained.cpp
 * @brief Compile-only header self-contained test for Skeleton.h.
 *
 * Verifies that Skeleton.h:
 * - Compiles when included first in an otherwise empty translation unit.
 * - Does not rely on transitive includes from other Fat-P headers.
 * - Is idempotent (double-include via pragma once produces no errors).
 *
 * No test framework is used. Compilation is the pass criterion; execution
 * only verifies the include guard by defining a trivial main().
 *
 * If this file fails to compile, the fix belongs in Skeleton.h (missing
 * standard includes, missing forward declarations, or include-order
 * dependency) -- never in this test file.
 */

// First include: target header must stand alone.
#include "Skeleton.h"

// Second include: validates pragma once idempotence.
#include "Skeleton.h"

int main()
{
    // Exercise a minimal instantiation so the compiler cannot elide all
    // template definitions. This catches headers that appear self-contained
    // but defer errors to template instantiation.
    fat_p::skeleton::Skeleton sk("self-contained-check");

    enum class Sys : uint8_t { Root = 1 };
    enum class Sub : uint8_t { Sensors = 1 };
    using Schema = fat_p::skeleton::HierarchySchema<Sys, Sub>;

    using RootBone = fat_p::skeleton::Bone<Schema, Sys::Root>;
    constexpr fat_p::skeleton::BoneId id = RootBone::id();
    (void)id;

    return 0;
}
