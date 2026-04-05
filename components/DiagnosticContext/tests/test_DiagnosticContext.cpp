/**
 * @file test_DiagnosticContext.cpp
 * @brief Comprehensive unit tests for DiagnosticContext.h
 *
 * Tests: empty context, single/nested frames, multi-key frames,
 * string values, loop context, cross-function visibility, thread
 * isolation, debug-only variant, and enforce integration.
 */
/*
FATP_META:
  meta_version: 1
  component: DiagnosticContext
  file_role: test
  path: components/DiagnosticContext/tests/test_DiagnosticContext.cpp
  namespace: [fat_p::testing::diagnosticcontext, fat_p::testing]
  layer: Testing
  summary: "Unit tests for DiagnosticContext."
  api_stability: in_work
  related:
    docs_search: "DiagnosticContext"
    headers:
      - include/fat_p/DiagnosticContext.h
      - include/fat_p/enforce.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include <string>
#include <thread>

#include "DiagnosticContext.h"
#include "enforce.h"
#include "FatPTest.h"

namespace fat_p::testing::diagnosticcontext
{

// ============================================================================
// Helper
// ============================================================================

/// @brief Returns true if @p haystack contains @p needle.
static bool contains(const std::string& haystack, const char* needle)
{
    return haystack.find(needle) != std::string::npos;
}

// ============================================================================
// Test Suite 1: Basic Stack Operations
// ============================================================================

FATP_TEST_CASE(empty_context)
{
    FATP_ASSERT_FALSE(DiagnosticContext::hasContext(), "Empty stack has no context");
    FATP_ASSERT_TRUE(DiagnosticContext::formatCurrent().empty(), "Empty stack formats to empty");
    FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(0), "Empty stack depth is 0");
    return true;
}

FATP_TEST_CASE(single_frame)
{
    {
        FATP_DIAG_CONTEXT("cell", 42);
        FATP_ASSERT_TRUE(DiagnosticContext::hasContext(), "Has context after push");
        FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(1), "Depth is 1");

        auto ctx = DiagnosticContext::formatCurrent();
        FATP_ASSERT_TRUE(contains(ctx, "cell=42"), "Contains cell=42");
    }
    FATP_ASSERT_FALSE(DiagnosticContext::hasContext(), "Empty after pop");
    return true;
}

FATP_TEST_CASE(nested_frames)
{
    {
        FATP_DIAG_CONTEXT("timestep", 100);
        FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(1), "Depth 1");
        {
            FATP_DIAG_CONTEXT("cell", 37);
            FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(2), "Depth 2");
            {
                FATP_DIAG_CONTEXT("face", 2);
                FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(3), "Depth 3");

                auto ctx = DiagnosticContext::formatCurrent();
                FATP_ASSERT_TRUE(contains(ctx, "timestep=100"), "Has timestep");
                FATP_ASSERT_TRUE(contains(ctx, "cell=37"), "Has cell");
                FATP_ASSERT_TRUE(contains(ctx, "face=2"), "Has face");

                // Verify outer-to-inner order
                auto tsPos = ctx.find("timestep");
                auto cellPos = ctx.find("cell");
                auto facePos = ctx.find("face");
                FATP_ASSERT_TRUE(tsPos < cellPos, "timestep before cell");
                FATP_ASSERT_TRUE(cellPos < facePos, "cell before face");
            }
            FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(2), "Face popped");
            auto ctx = DiagnosticContext::formatCurrent();
            FATP_ASSERT_TRUE(ctx.find("face") == std::string::npos, "No face after pop");
        }
        FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(1), "Cell popped");
    }
    FATP_ASSERT_EQ(DiagnosticContext::depth(), std::size_t(0), "All popped");
    return true;
}

// ============================================================================
// Test Suite 2: Multi-Key Frames
// ============================================================================

FATP_TEST_CASE(two_key_frame)
{
    {
        FATP_DIAG_CONTEXT("cell", 10, "face", 3);
        auto ctx = DiagnosticContext::formatCurrent();
        FATP_ASSERT_TRUE(contains(ctx, "cell=10"), "Has cell=10");
        FATP_ASSERT_TRUE(contains(ctx, "face=3"), "Has face=3");
    }
    FATP_ASSERT_FALSE(DiagnosticContext::hasContext(), "Popped");
    return true;
}

FATP_TEST_CASE(three_key_frame)
{
    {
        FATP_DIAG_CONTEXT("i", 1, "j", 2, "k", 3);
        auto ctx = DiagnosticContext::formatCurrent();
        FATP_ASSERT_TRUE(contains(ctx, "i=1"), "Has i=1");
        FATP_ASSERT_TRUE(contains(ctx, "j=2"), "Has j=2");
        FATP_ASSERT_TRUE(contains(ctx, "k=3"), "Has k=3");
    }
    return true;
}

// ============================================================================
// Test Suite 3: Value Types
// ============================================================================

FATP_TEST_CASE(string_value)
{
    {
        std::string material = "steel";
        FATP_DIAG_CONTEXT("material", material);
        auto ctx = DiagnosticContext::formatCurrent();
        FATP_ASSERT_TRUE(contains(ctx, "material=steel"), "Has material=steel");
    }
    return true;
}

FATP_TEST_CASE(cstring_value)
{
    {
        FATP_DIAG_CONTEXT("phase", "liquid");
        auto ctx = DiagnosticContext::formatCurrent();
        FATP_ASSERT_TRUE(contains(ctx, "phase=liquid"), "Has phase=liquid");
    }
    return true;
}

// ============================================================================
// Test Suite 4: Loop and Cross-Function Context
// ============================================================================

FATP_TEST_CASE(loop_context)
{
    for (int i = 0; i < 3; ++i)
    {
        FATP_DIAG_CONTEXT("iteration", i);
        auto ctx = DiagnosticContext::formatCurrent();
        std::string expected = "iteration=" + std::to_string(i);
        FATP_ASSERT_TRUE(contains(ctx, expected.c_str()), "Correct iteration");
    }
    FATP_ASSERT_FALSE(DiagnosticContext::hasContext(), "Clean after loop");
    return true;
}

static std::string captureContextFromInnerFunction()
{
    return DiagnosticContext::formatCurrent();
}

FATP_TEST_CASE(cross_function)
{
    {
        FATP_DIAG_CONTEXT("timestep", 500);
        {
            FATP_DIAG_CONTEXT("cell", 99);
            auto ctx = captureContextFromInnerFunction();
            FATP_ASSERT_TRUE(contains(ctx, "timestep=500"), "Inner sees timestep");
            FATP_ASSERT_TRUE(contains(ctx, "cell=99"), "Inner sees cell");
        }
    }
    return true;
}

// ============================================================================
// Test Suite 5: Thread Isolation
// ============================================================================

FATP_TEST_CASE(thread_isolation)
{
    FATP_DIAG_CONTEXT("thread", "main");

    std::string otherThreadCtx;
    std::string workerCtx;
    std::thread t([&] {
        otherThreadCtx = DiagnosticContext::formatCurrent();
        FATP_DIAG_CONTEXT("thread", "worker");
        workerCtx = DiagnosticContext::formatCurrent();
    });
    t.join();

    FATP_ASSERT_TRUE(otherThreadCtx.empty(), "Worker starts empty");
    FATP_ASSERT_TRUE(contains(workerCtx, "thread=worker"), "Worker sees own context");
    FATP_ASSERT_TRUE(workerCtx.find("main") == std::string::npos,
                     "Worker does not see main context");

    auto mainCtx = DiagnosticContext::formatCurrent();
    FATP_ASSERT_TRUE(contains(mainCtx, "thread=main"), "Main still has own context");
    return true;
}

// ============================================================================
// Test Suite 6: Debug-Only Variant
// ============================================================================

FATP_TEST_CASE(debug_variant)
{
    {
        FATP_DEBUG_DIAG_CONTEXT("debugKey", 42);
#ifdef NDEBUG
        FATP_ASSERT_FALSE(DiagnosticContext::hasContext(), "No context in Release");
#else
        FATP_ASSERT_TRUE(DiagnosticContext::hasContext(), "Has context in Debug");
        auto ctx = DiagnosticContext::formatCurrent();
        FATP_ASSERT_TRUE(contains(ctx, "debugKey=42"), "Has debugKey=42");
#endif
    }
    return true;
}

// ============================================================================
// Test Suite 7: Enforce Integration
// ============================================================================

FATP_TEST_CASE(enforce_without_context)
{
    try
    {
        FATP_ALWAYS_ENFORCE(1 == 2, "math is broken");
        FATP_ASSERT_TRUE(false, "Should have thrown");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(contains(msg, "1 == 2"), "Has condition");
        FATP_ASSERT_TRUE(contains(msg, "math is broken"), "Has message");
        FATP_ASSERT_TRUE(msg.find("Context:") == std::string::npos,
                         "No context line");
    }
    return true;
}

FATP_TEST_CASE(enforce_with_context)
{
    try
    {
        FATP_DIAG_CONTEXT("timestep", 4200);
        FATP_DIAG_CONTEXT("cell", 1037);
        FATP_DIAG_CONTEXT("material", "steel");

        FATP_ALWAYS_ENFORCE(false, "negative pressure");
        FATP_ASSERT_TRUE(false, "Should have thrown");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(contains(msg, "Context:"), "Has Context line");
        FATP_ASSERT_TRUE(contains(msg, "timestep=4200"), "Has timestep");
        FATP_ASSERT_TRUE(contains(msg, "cell=1037"), "Has cell");
        FATP_ASSERT_TRUE(contains(msg, "material=steel"), "Has material");
    }
    return true;
}

static void innerEnforce(double p)
{
    FATP_ALWAYS_ENFORCE(p > 0, "pressure must be positive, got ", p);
}

FATP_TEST_CASE(enforce_cross_function)
{
    try
    {
        FATP_DIAG_CONTEXT("simulation", "CFD_run_7");
        for (int cell = 0; cell < 5; ++cell)
        {
            FATP_DIAG_CONTEXT("cell", cell);
            double pressure = (cell == 3) ? -0.5 : 1.0;
            innerEnforce(pressure);
        }
        FATP_ASSERT_TRUE(false, "Should have thrown at cell 3");
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(contains(msg, "pressure must be positive"), "Has message");
        FATP_ASSERT_TRUE(contains(msg, "Context:"), "Has Context line");
        FATP_ASSERT_TRUE(contains(msg, "simulation=CFD_run_7"), "Has simulation");
        FATP_ASSERT_TRUE(contains(msg, "cell=3"), "Has cell=3");
    }
    return true;
}

FATP_TEST_CASE(context_cleanup_after_exception)
{
    try
    {
        FATP_DIAG_CONTEXT("outer", 1);
        {
            FATP_DIAG_CONTEXT("inner", 2);
            FATP_ALWAYS_ENFORCE(false, "boom");
        }
    }
    catch (...) {}

    FATP_ASSERT_FALSE(DiagnosticContext::hasContext(),
                      "Stack clean after unwinding");
    return true;
}

} // namespace fat_p::testing::diagnosticcontext

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_DiagnosticContext()
{
    FATP_PRINT_HEADER(DIAGNOSTIC CONTEXT)

    TestRunner runner;

    // Test Suite 1: Basic Stack Operations
    std::cout << colors::cyan() << "Test Suite 1: Basic Stack Operations"
              << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, diagnosticcontext, empty_context);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, single_frame);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, nested_frames);

    // Test Suite 2: Multi-Key Frames
    std::cout << "\n" << colors::cyan() << "Test Suite 2: Multi-Key Frames"
              << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, diagnosticcontext, two_key_frame);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, three_key_frame);

    // Test Suite 3: Value Types
    std::cout << "\n" << colors::cyan() << "Test Suite 3: Value Types"
              << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, diagnosticcontext, string_value);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, cstring_value);

    // Test Suite 4: Loop and Cross-Function
    std::cout << "\n" << colors::cyan() << "Test Suite 4: Loop and Cross-Function"
              << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, diagnosticcontext, loop_context);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, cross_function);

    // Test Suite 5: Thread Isolation
    std::cout << "\n" << colors::cyan() << "Test Suite 5: Thread Isolation"
              << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, diagnosticcontext, thread_isolation);

    // Test Suite 6: Debug-Only
    std::cout << "\n" << colors::cyan() << "Test Suite 6: Debug-Only"
              << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, diagnosticcontext, debug_variant);

    // Test Suite 7: Enforce Integration
    std::cout << "\n" << colors::cyan() << "Test Suite 7: Enforce Integration"
              << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, diagnosticcontext, enforce_without_context);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, enforce_with_context);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, enforce_cross_function);
    FATP_RUN_TEST_NS(runner, diagnosticcontext, context_cleanup_after_exception);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// ============================================================================
// Standalone Entry Point
// ============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_DiagnosticContext() ? 0 : 1;
}
#endif
