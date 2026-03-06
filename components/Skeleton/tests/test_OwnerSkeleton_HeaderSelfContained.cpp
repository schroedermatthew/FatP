/**
 * @file test_OwnerSkeleton_HeaderSelfContained.cpp
 * @brief Compile-only header self-contained test for OwnerSkeleton.h.
 *
 * Verifies that OwnerSkeleton.h:
 *   1. Compiles when included first in an otherwise empty translation unit
 *      (no transitive include luck from other Fat-P headers).
 *   2. Is idempotent (safe to include twice).
 *   3. Is include-order independent (standard library headers before or after
 *      make no difference -- validated by CI via separate TUs in the
 *      header-check job).
 */

/*
FATP_META:
  meta_version: 1
  component: OwnerSkeleton
  file_role: test
  path: components/OwnerSkeleton/tests/test_OwnerSkeleton_HeaderSelfContained.cpp
  namespace:
  layer: Testing
  summary: Compile-only self-contained and idempotence test for OwnerSkeleton.h.
  api_stability: in_work
  related:
    docs_search: "OwnerSkeleton"
  hygiene:
    pragma_once: false
    includes_windows_h: false
    defines_total: 0
*/

#include "OwnerSkeleton.h"
#include "OwnerSkeleton.h"  // idempotence

int main()
{
    return 0;
}
