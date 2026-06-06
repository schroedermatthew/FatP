/**
 * @file compile_fail_Jet_SeedDirection.cpp
 * @brief Expected-fail: seed<K>() must reject a direction K >= N.
 *
 * The compile-time seed overload is constrained with requires(K < N). This
 * translation unit calls Jet<2>::seed<2>(...) and is expected to FAIL
 * compilation because direction 2 is out of range for a Jet<2>. CI compiles
 * this with -c and asserts a non-zero exit status.
 */
/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: test
  path: components/Jet/tests/compile_fail/compile_fail_Jet_SeedDirection.cpp
  namespace: fat_p::autodiff
  layer: Testing
  summary: Expected-fail compile test that seed<K>() violates requires(K < N).
  api_stability: in_work
  related:
    docs_search: "Jet"
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include "Jet.h"

namespace
{
// Direction 2 is out of range for a Jet<2>; the requires(K < N) constraint on
// the compile-time seed overload must reject this and fail the compile.
constexpr auto kBad = fat_p::autodiff::Jet<2>::seed<2>(1.0);
} // namespace

int main()
{
    return 0;
}
