/**
 * @file test_FeatureManager_HeaderSelfContained.cpp
 * @brief Compile-only test: verifies FeatureManager.h compiles in isolation.
 *
 * Fat-P hygiene policy requires that every public header compiles standalone
 * without depending on prior includes from a unity build or PCH. This file
 * has no other includes. If it compiles, the header is self-contained.
 */
/*
FATP_META:
  meta_version: 1
  component: FeatureManager
  file_role: test
  path: components/FeatureManager/tests/test_FeatureManager_HeaderSelfContained.cpp
  layer: Testing
  namespace: fat_p
  summary: "Compile-only hygiene test for FeatureManager.h self-containment."
  api_stability: in_work
  related:
    headers:
      - include/fat_p/FeatureManager.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include "FeatureManager.h"

// Instantiate the primary template to force full compilation of the class body.
template class fat_p::feature::FeatureManager<fat_p::SingleThreadedPolicy>;

int main()
{
    return 0;
}
