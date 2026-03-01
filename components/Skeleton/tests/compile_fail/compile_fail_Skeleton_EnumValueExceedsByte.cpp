/**
 * @file compile_fail_Skeleton_EnumValueExceedsByte.cpp
 * @brief Expected-fail: Bone<> with an enum value > 255 must be rejected.
 *
 * Contract under test:
 *   Bone<> contains a static_assert that verifies each level enum value fits
 *   in one byte (range 0..255). This prevents silent data truncation: the
 *   BoneId packs each level into 8 bits, so a value > 255 would silently
 *   corrupt the packed address and break equality, ordering, and duplicate
 *   detection in Skeleton::publish().
 *
 * Violation:
 *   An enum with uint16_t underlying type and a value of 256 (just over the
 *   byte limit) is used as a level value. The schema is constructed to accept
 *   this enum type at depth 0, so the kLevelsMatchSchema constraint passes.
 *   The static_assert inside Bone<> must then fire.
 *
 * Expected failure:
 *   static_assert in Bone<>: "Each Bone level enum value must fit in one byte (0..255)"
 *
 * The compiler must emit the static_assert diagnostic.
 * If this file compiles, the byte-range safety contract of Bone<> has regressed.
 */

#include "Skeleton.h"

namespace
{

// An enum whose underlying type allows values > 255.
enum class WideEnum : uint16_t
{
    Valid   = 1,
    TooLarge = 256  // Just over the 8-bit limit.
};

// Schema that accepts WideEnum at depth 0.
using Schema = fat_p::skeleton::HierarchySchema<WideEnum>;

// Intentionally invalid: WideEnum::TooLarge has value 256, which does not fit
// in the 8-bit slot allocated for a BoneId level.
using Bad = fat_p::skeleton::Bone<Schema, WideEnum::TooLarge>;

// Force instantiation so the static_assert fires deterministically.
static_assert(Bad::kDepth > 0, "Force instantiation");

} // namespace
