/**
 * @file compile_fail_Stringify_ConceptHasToStringMethodReject.cpp
 * @brief Compile-fail test: concepts::has_to_string_method must reject wrong return type
 *
 * Expected: static_assert failure or concept constraint failure
 */

/*
FATP_META:
  meta_version: 1
  component: Stringify
  file_role: test
  path: components/Stringify/tests/compile_fail/compile_fail_Stringify_ConceptHasToStringMethodReject.cpp
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

namespace fat_p::testing::compile_fail::stringify_has_to_string_reject
{

struct BadReturnType
{
    int value;
    // toString() returns int, not std::string
    int toString() const { return value; }
};

// This static_assert should FAIL - BadReturnType::toString() returns int, not std::string
static_assert(fat_p::concepts::has_to_string_method<BadReturnType>,
              "This should fail: toString() returns int, not std::string");

inline void force_instantiate()
{
    BadReturnType brt{42};
    (void)brt;
}

} // namespace fat_p::testing::compile_fail::stringify_has_to_string_reject
