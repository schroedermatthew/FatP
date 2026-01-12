/**
 * @file test_EnhancedBoundsChecking.cpp
 * @brief Tests for enhanced bounds checking utilities
 */
/*
FATP_META:
  meta_version: 1
  component: EnhancedBoundsChecking
  file_role: test
  path: tests/test_EnhancedBoundsChecking.cpp
  namespace: fat_p::testing::enhancedboundschecking
  summary: "Unit tests for EnhancedBoundsChecking."
  related:
    docs_search: "EnhancedBoundsChecking"
    headers:
      - fat_p/EnhancedBoundsChecking.h
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

#include <iostream>
#include <vector>
#include <stdexcept>

#include "EnhancedBoundsChecking.h"
#include "FatPTest.h"

namespace fat_p::testing::enhancedboundschecking
{

// =============================================================================
// Test 1: Basic Bounds Checking
// =============================================================================

FATP_TEST_CASE(basic_bounds_checking) {
    std::cout << colors::cyan() << "\n[TEST] Basic Bounds Checking"
              << colors::reset() << std::endl;
    
    // Valid index should not throw
    try {
        bounds_check(5, 0, 10, "Test index");
        std::cout << colors::green() << "[OK] Valid index passed"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid index should not throw");
    }
    
    // Invalid index should throw
    bool threw = false;
    try {
        bounds_check(15, 0, 10, "Test index");
    } catch (const std::out_of_range& e) {
        threw = true;
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Test index") != std::string::npos, 
                     "Error message should contain context");
        FATP_ASSERT_TRUE(msg.find("15") != std::string::npos,
                     "Error message should contain index");
        std::cout << colors::blue() << "  Error message: " << e.what()
                  << colors::reset() << std::endl;
    }
    FATP_ASSERT_TRUE(threw, "Out of bounds index should throw");
    
    std::cout << colors::green() << "[OK] Basic bounds checking passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 2: Debug-Only Bounds Checking
// =============================================================================

FATP_TEST_CASE(debug_only_bounds_checking) {
    std::cout << colors::cyan() << "\n[TEST] Debug-Only Bounds Checking"
              << colors::reset() << std::endl;
    
    #ifdef NDEBUG
    // In release mode, debug_bounds_check should be no-op
    try {
        debug_bounds_check(100, 0, 10, "Debug index");
        std::cout << colors::blue() << "  Release build: check compiled out"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Debug check should be compiled out in release");
    }
    #else
    // In debug mode, should throw
    bool threw = false;
    try {
        debug_bounds_check(100, 0, 10, "Debug index");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "Debug build should throw on invalid index");
    std::cout << colors::blue() << "  Debug build: check active"
              << colors::reset() << std::endl;
    #endif
    
    std::cout << colors::green() << "[OK] Debug-only bounds checking passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 3: Container Bounds Checking
// =============================================================================

FATP_TEST_CASE(container_bounds_checking) {
    std::cout << colors::cyan() << "\n[TEST] Container Bounds Checking"
              << colors::reset() << std::endl;
    
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    // Valid access
    try {
        check_container_bounds(vec, 2, "Vector access");
        std::cout << colors::green() << "[OK] Valid container access passed"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid container access should not throw");
    }
    
    // Invalid access
    bool threw = false;
    try {
        check_container_bounds(vec, 10, "Vector access");
    } catch (const std::out_of_range& e) {
        threw = true;
        std::cout << colors::blue() << "  Error: " << e.what()
                  << colors::reset() << std::endl;
    }
    FATP_ASSERT_TRUE(threw, "Invalid container access should throw");
    
    std::cout << colors::green() << "[OK] Container bounds checking passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 4: 2D Bounds Checking
// =============================================================================

FATP_TEST_CASE(2d_bounds_checking) {
    std::cout << colors::cyan() << "\n[TEST] 2D Bounds Checking"
              << colors::reset() << std::endl;
    
    constexpr int rows = 10;
    constexpr int cols = 20;
    
    // Valid 2D access
    try {
        bounds_check_2d(5, 10, rows, cols, "Matrix access");
        std::cout << colors::green() << "[OK] Valid 2D access passed"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid 2D access should not throw");
    }
    
    // Invalid row
    bool threw = false;
    try {
        bounds_check_2d(15, 10, rows, cols, "Matrix access");
    } catch (const std::out_of_range& e) {
        threw = true;
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("(15, 10)") != std::string::npos,
                     "Error should show indices");
        FATP_ASSERT_TRUE(msg.find("(10, 20)") != std::string::npos,
                     "Error should show shape");
    }
    FATP_ASSERT_TRUE(threw, "Invalid row should throw");
    
    // Invalid column
    threw = false;
    try {
        bounds_check_2d(5, 25, rows, cols, "Matrix access");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "Invalid column should throw");
    
    std::cout << colors::green() << "[OK] 2D bounds checking passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 5: N-Dimensional Bounds Checking
// =============================================================================

FATP_TEST_CASE(nd_bounds_checking) {
    std::cout << colors::cyan() << "\n[TEST] N-Dimensional Bounds Checking"
              << colors::reset() << std::endl;
    
    std::vector<size_t> shape = {10, 20, 30};
    
    // Valid ND access
    try {
        std::vector<size_t> indices = {5, 10, 15};
        bounds_check_nd(indices, shape, "Tensor access");
        std::cout << colors::green() << "[OK] Valid ND access passed"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid ND access should not throw");
    }
    
    // Dimension mismatch
    bool threw = false;
    try {
        std::vector<size_t> indices = {5, 10};  // Only 2 indices for 3D shape
        bounds_check_nd(indices, shape, "Tensor access");
    } catch (const std::invalid_argument& e) {
        threw = true;
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("dimension mismatch") != std::string::npos,
                     "Error should mention dimension mismatch");
    }
    FATP_ASSERT_TRUE(threw, "Dimension mismatch should throw");
    
    // Out of bounds index
    threw = false;
    try {
        std::vector<size_t> indices = {5, 25, 15};  // Second index out of bounds
        bounds_check_nd(indices, shape, "Tensor access");
    } catch (const std::out_of_range& e) {
        threw = true;
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("dimension 1") != std::string::npos,
                     "Error should identify problematic dimension");
    }
    FATP_ASSERT_TRUE(threw, "Out of bounds index should throw");
    
    std::cout << colors::green() << "[OK] ND bounds checking passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 6: Range Validation
// =============================================================================

FATP_TEST_CASE(range_validation) {
    std::cout << colors::cyan() << "\n[TEST] Range Validation"
              << colors::reset() << std::endl;
    
    constexpr int size = 100;
    
    // Valid range
    try {
        validate_range(10, 50, size, "Array slice");
        std::cout << colors::green() << "[OK] Valid range passed"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid range should not throw");
    }
    
    // Invalid: start > end
    bool threw = false;
    try {
        validate_range(50, 10, size, "Array slice");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "start > end should throw");
    
    // Invalid: end > size
    threw = false;
    try {
        validate_range(10, 150, size, "Array slice");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "end > size should throw");
    
    std::cout << colors::green() << "[OK] Range validation passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 7: Slice Validation
// =============================================================================

FATP_TEST_CASE(slice_validation) {
    std::cout << colors::cyan() << "\n[TEST] Slice Validation"
              << colors::reset() << std::endl;
    
    constexpr int size = 100;
    
    // Valid slice
    try {
        validate_slice(10, 50, 2, size, "Array slice");
        std::cout << colors::green() << "[OK] Valid slice passed"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid slice should not throw");
    }
    
    // Invalid: step = 0
    bool threw = false;
    try {
        validate_slice(10, 50, 0, size, "Array slice");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "step = 0 should throw");
    
    // Invalid: start out of bounds
    threw = false;
    try {
        validate_slice(150, 200, 1, size, "Array slice");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "start out of bounds should throw");
    
    std::cout << colors::green() << "[OK] Slice validation passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test 8: Enforce-Based Bounds Checking
// =============================================================================

FATP_TEST_CASE(enforce_based_bounds) {
    std::cout << colors::cyan() << "\n[TEST] Enforce-Based Bounds Checking"
              << colors::reset() << std::endl;
    
    // Valid bounds with enforce
    try {
        enforce_bounds(5, 0, 10, "Enforce test");
        std::cout << colors::green() << "[OK] Valid enforce bounds passed"
                  << colors::reset() << std::endl;
    } catch (...) {
        FATP_ASSERT_TRUE(false, "Valid bounds should not throw");
    }
    
    // Invalid bounds should throw
    bool threw = false;
    try {
        enforce_bounds(15, 0, 10, "Enforce test");
    } catch (const std::exception& e) {
        threw = true;
        std::cout << colors::blue() << "  Enforce error: " << e.what()
                  << colors::reset() << std::endl;
    }
    FATP_ASSERT_TRUE(threw, "Enforce should throw on invalid bounds");
    
    std::cout << colors::green() << "[OK] Enforce-based bounds checking passed"
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Runner
// =============================================================================

} // namespace fat_p::testing::enhancedboundschecking

namespace fat_p::testing
{

bool test_EnhancedBoundsChecking() {
    FATP_PRINT_HEADER(ENHANCED BOUNDS CHECKING)
    
    TestRunner runner;
    
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, basic_bounds_checking);
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, debug_only_bounds_checking);
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, container_bounds_checking);
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, 2d_bounds_checking);
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, nd_bounds_checking);
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, range_validation);
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, slice_validation);
    FATP_RUN_TEST_NS(runner, enhancedboundschecking, enforce_based_bounds);
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_EnhancedBoundsChecking() ? 0 : 1;
}
#endif
