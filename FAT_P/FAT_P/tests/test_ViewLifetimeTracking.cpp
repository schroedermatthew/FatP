/**
 * @file test_ViewLifetimeTracking.cpp
 * @brief Tests for view lifetime tracking utilities
 */
/*
FATP_META:
  meta_version: 1
  component: ViewLifetimeTracking
  file_role: test
  path: tests/test_ViewLifetimeTracking.cpp
  namespace: fat_p
  summary: "Unit tests for ViewLifetimeTracking."
  related:
    docs_search: "ViewLifetimeTracking"
    headers:
      - fat_p/ViewLifetimeTracking.h
      - fat_p/FatPTest.h
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

#include "FatPTest.h"
#include "ViewLifetimeTracking.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace fat_p::testing::viewlifetimetracking
{

// =============================================================================
// Test 1: Basic Lifetime Tracking
// =============================================================================

FATP_TEST_CASE(basic_lifetime_tracking)
{
    std::cout << colors::cyan() << "\n[TEST] Basic Lifetime Tracking" << colors::reset() << std::endl;

#ifndef NDEBUG
    {
        std::vector<int> data = {1, 2, 3, 4, 5};
        LifetimeTracker<std::vector<int>> tracker(data, "test_vector");

        auto view = tracker.create_view();
        FATP_ASSERT_TRUE(view.is_valid(), "View should be valid");
        FATP_ASSERT_EQ(view->size(), 5, "View should access data");

        std::cout << colors::green() << "[OK] Basic tracking works in debug mode" << colors::reset() << std::endl;
    }
#else
    std::cout << colors::blue() << "  Release build: tracking compiled out" << colors::reset() << std::endl;
#endif

    return true;
}

// =============================================================================
// Test 2: Dangling Reference Detection
// =============================================================================

FATP_TEST_CASE(dangling_reference_detection)
{
    std::cout << colors::cyan() << "\n[TEST] Dangling Reference Detection" << colors::reset() << std::endl;

#ifndef NDEBUG
    using ViewType = LifetimeTracker<std::vector<int>>::TrackedView;
    std::optional<ViewType> view;

    {
        std::vector<int> data = {1, 2, 3};
        LifetimeTracker<std::vector<int>> tracker(data, "temporary_vector");
        view = tracker.create_view();

        FATP_ASSERT_TRUE(view->is_valid(), "View should be valid while object exists");
    }
    // data and tracker destroyed here

    FATP_ASSERT_FALSE(view->is_valid(), "View should be invalid after object destroyed");

    // Attempt to use invalid view
    bool threw = false;
    try
    {
        view->check_valid();
    }
    catch (const DanglingReferenceError& e)
    {
        threw = true;
        std::string msg = e.what();
        FATP_ASSERT_NE(msg.find("Dangling reference"), std::string::npos, "Error should mention dangling reference");
        std::cout << colors::blue() << "  Caught error: " << e.what() << colors::reset() << std::endl;
    }
    FATP_ASSERT_TRUE(threw, "Should throw on dangling reference access");

    std::cout << colors::green() << "[OK] Dangling reference detection works" << colors::reset() << std::endl;
#else
    std::cout << colors::blue() << "  Release build: detection compiled out" << colors::reset() << std::endl;
#endif

    return true;
}

// =============================================================================
// Test 3: Multiple Views
// =============================================================================

FATP_TEST_CASE(multiple_views)
{
    std::cout << colors::cyan() << "\n[TEST] Multiple Views" << colors::reset() << std::endl;

#ifndef NDEBUG
    std::vector<int> data = {10, 20, 30};
    LifetimeTracker<std::vector<int>> tracker(data, "shared_vector");

    auto view1 = tracker.create_view();
    auto view2 = tracker.create_view();
    auto view3 = tracker.create_view();

    FATP_ASSERT_TRUE(view1.is_valid(), "View 1 should be valid");
    FATP_ASSERT_TRUE(view2.is_valid(), "View 2 should be valid");
    FATP_ASSERT_TRUE(view3.is_valid(), "View 3 should be valid");

    FATP_ASSERT_EQ((*view1)[0], 10, "View 1 should access data");
    FATP_ASSERT_EQ((*view2)[1], 20, "View 2 should access data");
    FATP_ASSERT_EQ((*view3)[2], 30, "View 3 should access data");

    std::cout << colors::green() << "[OK] Multiple views work correctly" << colors::reset() << std::endl;
#else
    std::cout << colors::blue() << "  Release build: tracking compiled out" << colors::reset() << std::endl;
#endif

    return true;
}

// =============================================================================
// Test 4: Weak Pointer Utilities
// =============================================================================

FATP_TEST_CASE(weak_pointer_utilities)
{
    std::cout << colors::cyan() << "\n[TEST] Weak Pointer Utilities" << colors::reset() << std::endl;

    std::weak_ptr<int> weak;

    {
        auto shared = std::make_shared<int>(42);
        weak = shared;

        // Valid weak pointer
        auto locked = checked_lock(weak, "test_int");
        FATP_ASSERT_EQ(*locked, 42, "Locked pointer should have correct value");
    }
    // shared destroyed here

    // Expired weak pointer
    bool threw = false;
    try
    {
        auto locked = checked_lock(weak, "test_int");
    }
    catch (const DanglingReferenceError& e)
    {
        threw = true;
        std::cout << colors::blue() << "  Caught error: " << e.what() << colors::reset() << std::endl;
    }
    catch (const std::runtime_error&)
    {
        threw = true;
        std::cout << colors::blue() << "  Caught runtime_error (release build)" << colors::reset() << std::endl;
    }
    FATP_ASSERT_TRUE(threw, "Expired weak pointer should throw");

    // safe_lock should not throw
    auto result = safe_lock(weak);
    FATP_ASSERT_EQ(result, nullptr, "safe_lock should return nullptr");

    std::cout << colors::green() << "[OK] Weak pointer utilities work" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 5: View Guard
// =============================================================================

FATP_TEST_CASE(view_guard)
{
    std::cout << colors::cyan() << "\n[TEST] View Guard" << colors::reset() << std::endl;

#ifndef NDEBUG
    std::vector<int> data = {1, 2, 3};
    LifetimeTracker<std::vector<int>> tracker(data, "guarded_vector");
    auto view = tracker.create_view();

    {
        ViewGuard guard(view, "test_scope");
        FATP_ASSERT_TRUE(view.is_valid(), "View should be valid in guard scope");
    }
    // Guard destroyed, but view still valid since data still exists

    FATP_ASSERT_TRUE(view.is_valid(), "View should still be valid");

    std::cout << colors::green() << "[OK] View guard works" << colors::reset() << std::endl;
#else
    std::cout << colors::blue() << "  Release build: guard is no-op" << colors::reset() << std::endl;
#endif

    return true;
}

// =============================================================================
// Test 6: Macro Convenience
// =============================================================================

FATP_TEST_CASE(macro_convenience)
{
    std::cout << colors::cyan() << "\n[TEST] Macro Convenience" << colors::reset() << std::endl;

#ifndef NDEBUG
    std::vector<int> my_data = {100, 200, 300};
    auto tracker = FATP_TRACKED_VIEW(my_data);
    auto view = tracker.create_view();

    FATP_ASSERT_TRUE(view.is_valid(), "Macro-created tracker should work");
    FATP_ASSERT_EQ((*view)[1], 200, "Should access correct data");

    std::cout << colors::green() << "[OK] Macro convenience works" << colors::reset() << std::endl;
#else
    std::cout << colors::blue() << "  Release build: macros are pass-through" << colors::reset() << std::endl;
#endif

    return true;
}

// =============================================================================
// Test 7: Thread Safety
// =============================================================================

FATP_TEST_CASE(thread_safety)
{
    std::cout << colors::cyan() << "\n[TEST] Thread Safety" << colors::reset() << std::endl;

#ifndef NDEBUG
    std::vector<int> data(1000, 42);
    LifetimeTracker<std::vector<int>> tracker(data, "threaded_vector");

    // Create views in multiple threads
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(
            [&]()
            {
                auto view = tracker.create_view();
                if (view.is_valid())
                {
                    int sum = 0;
                    for (size_t j = 0; j < 100; ++j)
                    {
                        sum += (*view)[j];
                    }
                    if (sum == 4200)
                    { // 100 * 42
                        success_count++;
                    }
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(success_count, 10, "All threads should succeed");

    std::cout << colors::green() << "[OK] Thread safety works" << colors::reset() << std::endl;
#else
    std::cout << colors::blue() << "  Release build: no thread-safety overhead" << colors::reset() << std::endl;
#endif

    return true;
}

// =============================================================================
// Test 8: Zero Overhead in Release
// =============================================================================

FATP_TEST_CASE(zero_overhead_release)
{
    std::cout << colors::cyan() << "\n[TEST] Zero Overhead in Release" << colors::reset() << std::endl;

#ifdef NDEBUG
    // In release mode, sizeof should be minimal
    using TrackerType = LifetimeTracker<std::vector<int>>;
    std::cout << colors::blue() << "  Tracker size: " << sizeof(TrackerType) << " bytes (should be just a pointer)"
              << colors::reset() << std::endl;

    FATP_ASSERT_EQ(sizeof(TrackerType), sizeof(void*), "Release tracker should be pointer-sized");

    std::cout << colors::green() << "[OK] Release build has zero overhead" << colors::reset() << std::endl;
#else
    std::cout << colors::blue() << "  Debug build: tracking active" << colors::reset() << std::endl;
#endif

    return true;
}

} // namespace fat_p::testing::viewlifetimetracking

namespace fat_p::testing
{

// =============================================================================
// Test Runner
// =============================================================================

bool test_ViewLifetimeTracking()
{
    FATP_PRINT_HEADER(VIEW LIFETIME TRACKING)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, viewlifetimetracking, basic_lifetime_tracking);
    FATP_RUN_TEST_NS(runner, viewlifetimetracking, dangling_reference_detection);
    FATP_RUN_TEST_NS(runner, viewlifetimetracking, multiple_views);
    FATP_RUN_TEST_NS(runner, viewlifetimetracking, weak_pointer_utilities);
    FATP_RUN_TEST_NS(runner, viewlifetimetracking, view_guard);
    FATP_RUN_TEST_NS(runner, viewlifetimetracking, macro_convenience);
    FATP_RUN_TEST_NS(runner, viewlifetimetracking, thread_safety);
    FATP_RUN_TEST_NS(runner, viewlifetimetracking, zero_overhead_release);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ViewLifetimeTracking() ? 0 : 1;
}
#endif
