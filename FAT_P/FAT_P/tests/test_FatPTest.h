#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPTest
  file_role: internal_header
  path: tests/test_FatPTest.h
  namespace: fat_p
  summary: "Test support header for FatPTest."
  related:
    docs_search: "FatPTest"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#ifndef ENABLE_TEST_APPLICATION
namespace fat_p::testing
{

bool test_FatPTest();

} // namespace fat_p::testing
#endif // #ifndef ENABLE_TEST_APPLICATION
