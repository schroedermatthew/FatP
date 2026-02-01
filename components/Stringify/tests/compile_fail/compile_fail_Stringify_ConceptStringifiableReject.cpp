/**
 * @file compile_fail_Stringify_ConceptStringifiableReject.cpp
 * @brief Compile-fail test: concepts::stringifiable must reject truly non-stringifiable types
 *
 * Expected: static_assert failure
 */

/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: test
  path: components/Stringify/tests/compile_fail/compile_fail_Stringify_ConceptStringifiableReject.cpp
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

namespace fat_p::testing::compile_fail::stringify_stringifiable_reject
{

struct TrulyNonStringifiable
{
    int value;
    // No toString(), no to_string(), no operator<<, not arithmetic, not enum, not container
};

// This static_assert should FAIL - TrulyNonStringifiable has no way to be stringified
static_assert(fat_p::concepts::stringifiable<TrulyNonStringifiable>,
              "This should fail: TrulyNonStringifiable cannot be stringified");

inline void force_instantiate()
{
    TrulyNonStringifiable tns{42};
    (void)tns;
}

} // namespace fat_p::testing::compile_fail::stringify_stringifiable_reject
