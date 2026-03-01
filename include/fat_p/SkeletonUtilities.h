#pragma once
/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: public_header
  path: include/fat_p/SkeletonUtilities.h
  namespace: fat_p::skeleton
  layer: Domain
  summary: >
    Runtime utilities for fat_p::Skeleton: index2BoneId for generating unique
    BoneId descendants from a flat integer index without a compile-time schema.
  api_stability: in_work
  related:
    headers:
      - include/fat_p/SkeletonFwd.h
    docs_search: "Skeleton"
    tests:
      - components/Skeleton/tests/test_Skeleton.cpp
    benchmarks:
      - components/Skeleton/benchmarks/benchmark_Skeleton.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file SkeletonUtilities.h
 * @brief Runtime utilities for the Skeleton component.
 *
 * @details
 * Provides helpers for scenarios that cannot use the typed Bone<Schema> system,
 * such as plugin systems registering items with dynamic keys, data-driven
 * hierarchies loaded from configuration, and test or benchmark fixtures that
 * need to generate large populations of valid BoneIds without a fixed schema.
 *
 * Key functions:
 * - index2BoneId(prefix, index) -- unique BoneId descendant from a flat index
 *
 * Requirements:
 * - C++20
 * - fat_p headers: SkeletonFwd.h
 */

#include <cstddef>
#include <cstdint>

#include "SkeletonFwd.h"

namespace fat_p::skeleton
{

/**
 * @brief Maps a flat zero-based index to a unique BoneId descendant of @p prefix.
 *
 * @details
 * Encodes @p index as a base-256 number and appends one level per digit
 * (least-significant digit first) via successive BoneId::child() calls.
 * Every distinct @p index produces a distinct BoneId, and no result is an
 * ancestor of any other result for the same @p prefix.
 *
 * Capacity per prefix depth:
 * | prefix.depth() | remaining levels | max index (exclusive) |
 * |:--------------:|:----------------:|----------------------:|
 * | 0              | 8                | 256^8  (~1.8e19)      |
 * | 1              | 7                | 256^7  (~7.2e16)      |
 * | 2              | 6                | 256^6  (~2.8e14)      |
 * | 3              | 5                | 256^5  (~1.1e12)      |
 * | 4              | 4                | 256^4  (4,294,967,296)|
 * | 5              | 3                | 256^3  (16,777,216)   |
 * | 6              | 2                | 256^2  (65,536)       |
 * | 7              | 1                | 256^1  (256)          |
 *
 * @par Encoding
 * The encoding is little-endian in the BoneId path: the least-significant byte
 * of @p index occupies the first appended level. This means indices 0..255 each
 * occupy exactly one additional level, indices 256..65535 occupy two, and so on.
 * The resulting depth is @p prefix.depth() + ceil(log256(index + 1)), with a
 * minimum of @p prefix.depth() + 1.
 *
 * @par Preconditions
 * - @p prefix must not be null (prefix.depth() > 0 is not required, but
 *   prefix.isNull() == false is required to prevent publishing a null BoneId).
 * - @p index must be small enough that the encoded form fits within the 8-level
 *   BoneId maximum. Violating this terminates the process in all build
 *   configurations (FATP_ALWAYS_ENFORCE in BoneId::child()).
 * - Callers sharing a @p prefix between two independent index sequences
 *   are responsible for ensuring the sequences do not overlap. Use distinct
 *   top-level roots (e.g. BoneId{}.child(1), BoneId{}.child(2)) to partition.
 *
 * @par Example
 * @code
 * // Generate 10,000 unique BoneIds under root [1].
 * BoneId prefix = BoneId{}.child(1);
 * for (size_t i = 0; i < 10'000; ++i)
 * {
 *     BoneId id = fat_p::skeleton::index2BoneId(prefix, i);
 *     // id is unique for each i
 * }
 *
 * // Partition two independent populations under roots [1] and [2].
 * BoneId hits   = fat_p::skeleton::index2BoneId(BoneId{}.child(1), 42);
 * BoneId misses = fat_p::skeleton::index2BoneId(BoneId{}.child(2), 42);
 * // hits != misses even for the same index
 * @endcode
 *
 * @param prefix The BoneId to append levels to. Must not be null.
 * @param index  Zero-based integer uniquely identifying the desired descendant.
 * @return A BoneId descended from @p prefix that is unique for each @p index.
 *
 * @note Complexity: O(depth consumed), which is at most 8 iterations total.
 * @note Thread-safety: stateless; safe to call concurrently on distinct threads.
 */
[[nodiscard]] inline BoneId index2BoneId(BoneId prefix, std::size_t index) noexcept
{
    BoneId id = prefix;
    do
    {
        id = id.child(static_cast<uint8_t>(index & 0xFF));
        index >>= 8;
    }
    while (index > 0);
    return id;
}

} // namespace fat_p::skeleton
