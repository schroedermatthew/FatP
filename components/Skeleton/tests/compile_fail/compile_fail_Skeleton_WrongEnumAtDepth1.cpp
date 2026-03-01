/**
 * @file compile_fail_Skeleton_WrongEnumAtDepth1.cpp
 * @brief Expected-fail: Bone<> with wrong enum type at depth 1 must be rejected.
 *
 * Contract under test:
 *   detail::kLevelsMatchSchema<Schema, Levels...> requires that each value in
 *   Levels... has the type that the Schema declares for that position.
 *   Passing an enum of the wrong type at any depth is a hard constraint
 *   violation that must be caught at instantiation time.
 *
 * Schema declares: depth 0 = Sys, depth 1 = Sub.
 * Violation:       Bone<Schema, Sys::Root, Sys::Root>
 *                  -- Sys::Root has type Sys, but depth 1 requires Sub.
 *
 * Expected failure:
 *   Constraint violation on Bone<Schema, Sys::Root, Sys::Root>:
 *   sizeof...(Levels) <= Schema::kMaxDepth  -- satisfies (depth 2 <= 2)
 *   kLevelsMatchSchema<Schema, Levels...>   -- FAILS (Sys at position 1, Sub expected)
 *
 * The compiler must emit a constraint-not-satisfied error, not compile silently.
 * If this file compiles, the type-safety contract of Bone<> has regressed.
 */

#include "Skeleton.h"

namespace
{

enum class Sys : uint8_t { Root = 1 };
enum class Sub : uint8_t { Sensors = 1 };

using Schema = fat_p::skeleton::HierarchySchema<Sys, Sub>;

// Intentionally invalid: position 1 must be Sub, but Sys is passed instead.
using Bad = fat_p::skeleton::Bone<Schema, Sys::Root, Sys::Root>;

// Force instantiation so the constraint failure triggers deterministically.
static_assert(Bad::kDepth > 0, "Force instantiation");

} // namespace
