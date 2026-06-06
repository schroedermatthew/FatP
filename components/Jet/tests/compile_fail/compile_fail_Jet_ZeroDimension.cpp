/**
 * @file compile_fail_Jet_ZeroDimension.cpp
 * @brief Expected-fail: Jet<0> must be rejected by the requires(N >= 1) clause.
 *
 * A Jet must carry at least one partial-derivative direction. This translation
 * unit instantiates Jet<0> and is expected to FAIL compilation because the
 * primary template is constrained with requires(N >= 1). CI compiles this with
 * -c and asserts a non-zero exit status.
 */
/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: test
  path: components/Jet/tests/compile_fail/compile_fail_Jet_ZeroDimension.cpp
  namespace: fat_p::autodiff
  layer: Testing
  summary: Expected-fail compile test that Jet<0> violates requires(N >= 1).
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
// Force instantiation of the constrained primary template with N == 0 so the
// requires(N >= 1) constraint is checked and the compile reliably fails.
using Bad = fat_p::autodiff::Jet<0>;
static_assert(sizeof(Bad) > 0, "Force instantiation of Jet<0>");
} // namespace

int main()
{
    return 0;
}
