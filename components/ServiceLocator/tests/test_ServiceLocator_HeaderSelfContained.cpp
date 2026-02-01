/**
 * @file test_ServiceLocator_HeaderSelfContained.cpp
 * @brief Header self-containment test for ServiceLocator.h
 *
 * @details
 * Verifies that ServiceLocator.h is self-contained: it compiles when included
 * first (and only) in an otherwise empty TU. Double-include validates
 * #pragma once / idempotence.
 *
 * This file exists primarily to COMPILE. Runtime checks are minimal.
 */
/*
FATP_META:
  meta_version: 1
  component: ServiceLocator
  file_role: test
  path: components/ServiceLocator/tests/test_ServiceLocator_HeaderSelfContained.cpp
  layer: Testing
  namespace: fat_p::testing
  summary: "Compile-only self-containment check for ServiceLocator.h"
  api_stability: in_work
  related:
    headers:
      - include/fat_p/ServiceLocator.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: chatgpt
    mode: manual
*/

// CRITICAL: ServiceLocator.h MUST be the first include (no FatPTest.h!)
#include "ServiceLocator.h"
#include "ServiceLocator.h"  // Validate idempotence

#include <iostream>
#include <memory>

namespace fat_p::testing::service_locator_header_self_contained
{

struct Foo
{
    int mValue = 0;
};

struct Bar
{
    int mValue = 0;
};

} // namespace fat_p::testing::service_locator_header_self_contained

namespace fat_p::testing
{

bool test_ServiceLocator_HeaderSelfContained()
{
    std::cout << "==========================================================\n";
    std::cout << "SERVICE LOCATOR HEADER SELF-CONTAINMENT TEST\n";
    std::cout << "==========================================================\n\n";

    using namespace service_locator_header_self_contained;

    fat_p::DefaultServiceLocator locator;

    Foo foo;
    foo.mValue = 123;

    auto reg = locator.registerInstance<Foo>(foo);
    (void)reg;

    Foo* resolved = locator.tryResolve<Foo>();
    (void)resolved;

    auto shared = std::make_shared<Bar>();
    (void)locator.registerShared<Bar>(shared);

    return true;
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ServiceLocator_HeaderSelfContained() ? 0 : 1;
}
#endif
