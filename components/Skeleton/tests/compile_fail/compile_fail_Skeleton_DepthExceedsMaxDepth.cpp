/**
 * @file compile_fail_Skeleton_DepthExceedsMaxDepth.cpp
 * @brief Expected-fail: Bone<> with more levels than kMaxDepth must be rejected.
 *
 * Contract under test:
 *   The requires clause on Bone<Schema, Levels...> enforces:
 *     sizeof...(Levels) <= Schema::kMaxDepth
 *   A schema with kMaxDepth == 2 must reject any Bone instantiation that
 *   supplies 3 or more level values.
 *
 * Schema declares: depth 0 = Sys, depth 1 = Sub. kMaxDepth == 2.
 * Violation:       Bone<Schema, Sys::Root, Sub::Sensors, Sub::Sensors>
 *                  -- 3 levels provided, but kMaxDepth is 2.
 *
 * Expected failure:
 *   Constraint violation on Bone<Schema, Sys::Root, Sub::Sensors, Sub::Sensors>:
 *   sizeof...(Levels) <= Schema::kMaxDepth  -- FAILS (3 > 2)
 *
 * The compiler must emit a constraint-not-satisfied error.
 * If this file compiles, the depth enforcement contract of Bone<> has regressed.
 */
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: test
  path: components/Skeleton/tests/compile_fail/compile_fail_Skeleton_DepthExceedsMaxDepth.cpp
  layer: Testing
  namespace: fat_p
  summary: "Compile-fail test: Bone depth must not exceed Schema::kMaxDepth"
  api_stability: in_work
  related:
    docs_search: "Skeleton"
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include "Skeleton.h"

namespace
{

enum class Sys : uint8_t { Root = 1 };
enum class Sub : uint8_t { Sensors = 1 };

// Two-level schema -- kMaxDepth == 2.
using Schema = fat_p::skeleton::HierarchySchema<Sys, Sub>;

// Intentionally invalid: 3 levels supplied to a schema with kMaxDepth == 2.
using Bad = fat_p::skeleton::Bone<Schema, Sys::Root, Sub::Sensors, Sub::Sensors>;

// Force instantiation so the constraint failure triggers deterministically.
static_assert(Bad::kDepth > 0, "Force instantiation");

} // namespace
