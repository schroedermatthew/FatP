/**
 * @file compile_fail_Stringify_ConceptStreamableReject.cpp
 * @brief Compile-fail test: concepts::streamable must reject types without operator<<
 *
 * Expected: static_assert failure or concept constraint failure
 */

/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: test
  path: components/Stringify/tests/compile_fail/compile_fail_Stringify_ConceptStreamableReject.cpp
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

namespace fat_p::testing::compile_fail::stringify_streamable_reject
{

struct NotStreamable
{
    int value;
    // No operator<< defined
};

// This static_assert should FAIL - NotStreamable is not streamable
static_assert(fat_p::concepts::streamable<NotStreamable>,
              "This should fail: NotStreamable has no operator<<");

inline void force_instantiate()
{
    NotStreamable ns{42};
    (void)ns;
}

} // namespace fat_p::testing::compile_fail::stringify_streamable_reject
