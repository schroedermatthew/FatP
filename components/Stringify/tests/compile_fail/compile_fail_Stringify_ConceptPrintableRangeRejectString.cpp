/**
 * @file compile_fail_Stringify_ConceptPrintableRangeRejectString.cpp
 * @brief Compile-fail test: concepts::printable_range must exclude std::string
 *
 * Expected: static_assert failure - std::string should NOT satisfy printable_range
 */

/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: test
  path: components/Stringify/tests/compile_fail/compile_fail_Stringify_ConceptPrintableRangeRejectString.cpp
  layer: Testing
  namespace: fat_p
  summary: "test file for Stringify"
  api_stability: in_work
  related:
    docs_search: "Stringify"
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/


#include "Stringify.h"
#include <string>

namespace fat_p::testing::compile_fail::stringify_printable_range_reject_string
{

// This static_assert should FAIL - std::string is explicitly excluded from printable_range
// to prevent iterating over characters instead of treating it as a string
static_assert(fat_p::concepts::printable_range<std::string>,
              "This should fail: std::string is excluded from printable_range");

inline void force_instantiate()
{
    std::string s = "test";
    (void)s;
}

} // namespace fat_p::testing::compile_fail::stringify_printable_range_reject_string
