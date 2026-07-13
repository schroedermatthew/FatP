/**
 * @file compile_fail_Skeleton_EnumValueExceedsLevelWidth.cpp
 * @brief Expected-fail: Bone<> with an enum value > 65535 must be rejected.
 *
 * Contract under test:
 *   Bone<> contains a static_assert that verifies each level enum value fits
 *   in one 16-bit level slot (range 0..65535). This prevents silent data
 *   truncation: the BoneId packs each level into 16 bits, so a value > 65535
 *   would silently corrupt the packed address and break equality, ordering,
 *   and duplicate detection in Skeleton::publish().
 *
 * Violation:
 *   An enum with uint32_t underlying type and a value of 65536 (just over the
 *   16-bit limit) is used as a level value. The schema is constructed to accept
 *   this enum type at depth 0, so the kLevelsMatchSchema constraint passes.
 *   The static_assert inside Bone<> must then fire.
 *
 * Expected failure:
 *   static_assert in Bone<>: "Each Bone level enum value must fit in one 16-bit level (0..65535)"
 *
 * The compiler must emit the static_assert diagnostic.
 * If this file compiles, the level-range safety contract of Bone<> has regressed.
 */
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: test
  path: components/Skeleton/tests/compile_fail/compile_fail_Skeleton_EnumValueExceedsLevelWidth.cpp
  layer: Testing
  namespace: fat_p
  summary: "Compile-fail test: Bone level enum values must fit in one 16-bit level"
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

// An enum whose underlying type allows values > 65535.
enum class WideEnum : uint32_t
{
    Valid    = 1,
    TooLarge = 65536  // Just over the 16-bit limit.
};

// Schema that accepts WideEnum at depth 0.
using Schema = fat_p::skeleton::HierarchySchema<WideEnum>;

// Intentionally invalid: WideEnum::TooLarge has value 65536, which does not
// fit in the 16-bit slot allocated for a BoneId level.
using Bad = fat_p::skeleton::Bone<Schema, WideEnum::TooLarge>;

// Force instantiation so the static_assert fires deterministically.
static_assert(Bad::kDepth > 0, "Force instantiation");

} // namespace
