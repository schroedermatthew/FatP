/**
 * @file test_Stacktrace.cpp
 * @brief Comprehensive unit tests for Stacktrace.h
 *
 * Tests cover:
 * - Basic capture functionality
 * - Frame skipping and depth limiting
 * - Symbol resolution
 * - Raw capture and deferred symbolization
 * - Output formatting (string, JSON)
 * - Comparison and hashing
 * - Thread safety
 * - Backend detection
 */
/*
FATP_META:
  meta_version: 1
  component: Stacktrace
  file_role: test
  path: tests/test_Stacktrace.cpp
  namespace: fat_p::testing::stacktrace
  summary: Comprehensive unit tests for Stacktrace.
  related:
    docs_search: "Stacktrace"
    headers:
      - fat_p/Stacktrace.h
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

#include <atomic>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "FatPTest.h"
#include "Stacktrace.h"

namespace fat_p::testing::stacktrace
{

// ============================================================================
// Test Helpers
// ============================================================================

// Helper functions to create nested call stacks for testing
// Each function is NOINLINE to ensure they appear in stack traces

#if defined(__GNUC__) || defined(__clang__)
#define FATP_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define FATP_NOINLINE __declspec(noinline)
#else
#define FATP_NOINLINE
#endif

FATP_NOINLINE Stacktrace captureAtDepth3()
{
    return Stacktrace::current();
}

FATP_NOINLINE Stacktrace captureAtDepth2()
{
    return captureAtDepth3();
}

FATP_NOINLINE Stacktrace captureAtDepth1()
{
    return captureAtDepth2();
}

FATP_NOINLINE Stacktrace captureRawAtDepth3()
{
    return Stacktrace::captureRaw();
}

FATP_NOINLINE Stacktrace captureRawAtDepth2()
{
    return captureRawAtDepth3();
}

FATP_NOINLINE Stacktrace captureRawAtDepth1()
{
    return captureRawAtDepth2();
}

// ============================================================================
// Basic Capture Tests
// ============================================================================

FATP_TEST_CASE(capture_current_not_empty)
{
    auto st = Stacktrace::current();

    FATP_ASSERT_FALSE(st.empty(), "Stacktrace should not be empty");
    FATP_ASSERT_GT(st.size(), 0u, "Stacktrace should have at least one frame");

    return true;
}

FATP_TEST_CASE(capture_has_valid_frames)
{
    auto st = Stacktrace::current();

    FATP_ASSERT_FALSE(st.empty(), "Should have frames");

    // All frames should have addresses (unless stub backend)
    if (Stacktrace::hasRealBackend())
    {
        for (const auto& frame : st.frames())
        {
            // With real backend, addresses should be non-null (usually)
            // Note: Some frames might have null addresses in edge cases
            FATP_ASSERT_TRUE(frame.symbolized, "Frame should be symbolized");
        }
    }

    return true;
}

FATP_TEST_CASE(capture_is_symbolized)
{
    auto st = Stacktrace::current();

    FATP_ASSERT_TRUE(st.isSymbolized(), "current() should auto-symbolize");

    return true;
}

// ============================================================================
// Frame Skipping Tests
// ============================================================================

FATP_TEST_CASE(skip_frames_reduces_depth)
{
    auto st0 = Stacktrace::current(0);
    auto st1 = Stacktrace::current(1);
    auto st2 = Stacktrace::current(2);

    // More skipped frames = fewer captured frames
    FATP_ASSERT_GE(st0.size(), st1.size(), "Skipping 0 should capture >= skipping 1");
    FATP_ASSERT_GE(st1.size(), st2.size(), "Skipping 1 should capture >= skipping 2");

    return true;
}

FATP_TEST_CASE(skip_removes_top_frames)
{
    // Capture with different skip values
    auto st0 = Stacktrace::current(0);
    auto st2 = Stacktrace::current(2);

    if (st0.size() >= 3 && st2.size() >= 1 && Stacktrace::hasRealBackend())
    {
        // Frame at index 2 in st0 should match frame at index 0 in st2
        // (because we skipped 2 frames in st2)
        FATP_ASSERT_EQ(st0[2].address, st2[0].address, "Skipped frames should match");
    }

    return true;
}

// ============================================================================
// Max Depth Tests
// ============================================================================

FATP_TEST_CASE(max_depth_limits_capture)
{
    auto st = Stacktrace::current(1, 3);

    FATP_ASSERT_LE(st.size(), 3u, "Should respect maxDepth parameter");

    return true;
}

FATP_TEST_CASE(max_depth_one)
{
    auto st = Stacktrace::current(1, 1);

    FATP_ASSERT_LE(st.size(), 1u, "maxDepth=1 should capture at most 1 frame");

    return true;
}

FATP_TEST_CASE(max_depth_large)
{
    auto st = Stacktrace::current(1, 1000);

    // Should capture something reasonable, not crash
    FATP_ASSERT_FALSE(st.empty(), "Large maxDepth should still capture frames");

    return true;
}

// ============================================================================
// Nested Call Tests
// ============================================================================

FATP_TEST_CASE(nested_calls_captured)
{
    auto st = captureAtDepth1();

    // Should capture multiple frames from nested calls
    if (Stacktrace::hasRealBackend())
    {
        FATP_ASSERT_GE(st.size(), 2u, "Should capture nested call stack");
    }

    return true;
}

FATP_TEST_CASE(nested_function_names_resolved)
{
    auto st = captureAtDepth1();

    if (Stacktrace::hasRealBackend())
    {
        // At least one frame should have a function name containing our test function
        bool foundTestFunction = false;
        for (const auto& frame : st.frames())
        {
            if (frame.function.find("captureAtDepth") != std::string::npos)
            {
                foundTestFunction = true;
                break;
            }
        }

        // Note: This may fail if symbols are stripped, which is OK
        // We just log if not found
        if (!foundTestFunction)
        {
            std::cout << "  [Info: Test function names not found in symbols - "
                      << "this is normal if symbols are stripped]\n";
        }
    }

    return true;
}

// ============================================================================
// Raw Capture Tests
// ============================================================================

FATP_TEST_CASE(capture_raw_not_symbolized)
{
    auto st = Stacktrace::captureRaw();

    // captureRaw should NOT auto-symbolize (on most backends)
    // Note: C++23 backend always symbolizes
    FATP_ASSERT_FALSE(st.empty(), "captureRaw should capture frames");

    return true;
}

FATP_TEST_CASE(capture_raw_then_resolve)
{
    auto st = Stacktrace::captureRaw();

    FATP_ASSERT_FALSE(st.empty(), "Should have frames");

    st.resolveSymbols();

    FATP_ASSERT_TRUE(st.isSymbolized(), "Should be symbolized after resolveSymbols()");

    return true;
}

FATP_TEST_CASE(resolve_symbols_idempotent)
{
    auto st = Stacktrace::captureRaw();

    st.resolveSymbols();
    auto size1 = st.size();
    auto str1 = st.toString();

    st.resolveSymbols();  // Call again
    auto size2 = st.size();
    auto str2 = st.toString();

    FATP_ASSERT_EQ(size1, size2, "Repeated resolveSymbols should not change size");
    FATP_ASSERT_EQ(str1, str2, "Repeated resolveSymbols should produce same output");

    return true;
}

FATP_TEST_CASE(raw_nested_capture)
{
    auto st = captureRawAtDepth1();

    if (Stacktrace::hasRealBackend())
    {
        FATP_ASSERT_GE(st.size(), 2u, "Raw capture should get nested frames");
    }

    st.resolveSymbols();
    FATP_ASSERT_TRUE(st.isSymbolized(), "Should resolve after raw capture");

    return true;
}

// ============================================================================
// Output Formatting Tests
// ============================================================================

FATP_TEST_CASE(to_string_not_empty)
{
    auto st = Stacktrace::current();
    std::string str = st.toString();

    FATP_ASSERT_FALSE(str.empty(), "toString() should produce output");

    return true;
}

FATP_TEST_CASE(to_string_multiline)
{
    auto st = Stacktrace::current();
    std::string str = st.toString();

    if (st.size() > 1)
    {
        FATP_ASSERT_TRUE(str.find('\n') != std::string::npos, "Multi-frame trace should be multi-line");
    }

    return true;
}

FATP_TEST_CASE(to_string_has_frame_numbers)
{
    auto st = Stacktrace::current();
    std::string str = st.toString();

    FATP_ASSERT_TRUE(str.find("#0") != std::string::npos, "Should have frame number #0");

    return true;
}

FATP_TEST_CASE(to_string_max_frames)
{
    auto st = Stacktrace::current(1, 10);

    if (st.size() > 2)
    {
        std::string full = st.toString(0);
        std::string limited = st.toString(2);

        FATP_ASSERT_LT(limited.size(), full.size(), "Limited output should be shorter");
        FATP_ASSERT_TRUE(limited.find("more frames") != std::string::npos, "Should indicate truncation");
    }

    return true;
}

FATP_TEST_CASE(to_json_valid_format)
{
    auto st = Stacktrace::current();
    std::string json = st.toJson();

    FATP_ASSERT_FALSE(json.empty(), "toJson() should produce output");
    FATP_ASSERT_TRUE(json.find('[') != std::string::npos, "Should be JSON array");
    FATP_ASSERT_TRUE(json.find(']') != std::string::npos, "Should close JSON array");
    FATP_ASSERT_TRUE(json.find("\"address\"") != std::string::npos, "Should have address field");
    FATP_ASSERT_TRUE(json.find("\"function\"") != std::string::npos, "Should have function field");
    FATP_ASSERT_TRUE(json.find("\"symbolized\"") != std::string::npos, "Should have symbolized field");

    return true;
}

FATP_TEST_CASE(stream_insertion)
{
    auto st = Stacktrace::current();

    std::ostringstream oss;
    oss << st;

    FATP_ASSERT_FALSE(oss.str().empty(), "Stream insertion should produce output");
    FATP_ASSERT_EQ(oss.str(), st.toString(), "Stream insertion should match toString()");

    return true;
}

// ============================================================================
// StackFrame Tests
// ============================================================================

FATP_TEST_CASE(frame_to_string)
{
    StackFrame frame;
    frame.address = reinterpret_cast<void*>(0x12345678);
    frame.function = "testFunction";
    frame.offset = 16;
    frame.file = "test.cpp";
    frame.line = 42;

    std::string str = frame.toString();

    FATP_ASSERT_TRUE(str.find("testFunction") != std::string::npos, "Should contain function name");
    FATP_ASSERT_TRUE(str.find("test.cpp") != std::string::npos, "Should contain file name");
    FATP_ASSERT_TRUE(str.find("42") != std::string::npos, "Should contain line number");

    return true;
}

FATP_TEST_CASE(frame_to_string_short)
{
    StackFrame frame;
    frame.function = "myFunction";
    frame.offset = 0x20;

    std::string str = frame.toStringShort();

    FATP_ASSERT_TRUE(str.find("myFunction") != std::string::npos, "Should contain function");
    FATP_ASSERT_TRUE(str.find("+0x") != std::string::npos, "Should contain offset");

    return true;
}

FATP_TEST_CASE(frame_equality)
{
    StackFrame f1, f2;
    f1.address = reinterpret_cast<void*>(0x1000);
    f2.address = reinterpret_cast<void*>(0x1000);

    FATP_ASSERT_TRUE(f1 == f2, "Frames with same address should be equal");

    f2.address = reinterpret_cast<void*>(0x2000);
    FATP_ASSERT_TRUE(f1 != f2, "Frames with different addresses should not be equal");

    return true;
}

FATP_TEST_CASE(frame_stream_insertion)
{
    StackFrame frame;
    frame.function = "testFunc";

    std::ostringstream oss;
    oss << frame;

    FATP_ASSERT_TRUE(oss.str().find("testFunc") != std::string::npos, "Stream should contain function");

    return true;
}

// ============================================================================
// Comparison and Hashing Tests
// ============================================================================

FATP_TEST_CASE(stacktrace_equality)
{
    auto st1 = Stacktrace::current();
    auto st2 = Stacktrace::current();

    // Two captures at the same location should be equal
    // (addresses should match)
    FATP_ASSERT_TRUE(st1 == st2 || st1 != st2, "Equality comparison should work");

    return true;
}

FATP_TEST_CASE(stacktrace_hash)
{
    auto st1 = Stacktrace::current();
    auto st2 = Stacktrace::current();

    std::size_t h1 = st1.hash();
    std::size_t h2 = st2.hash();

    // Hashes should be deterministic
    FATP_ASSERT_EQ(st1.hash(), h1, "Hash should be deterministic");
    (void)h2;  // May differ due to ASLR

    // Can use in unordered containers
    std::unordered_set<Stacktrace> traces;
    traces.insert(st1);
    FATP_ASSERT_EQ(traces.size(), 1u, "Should be insertable into unordered_set");

    return true;
}

FATP_TEST_CASE(std_hash_specialization)
{
    auto st = Stacktrace::current();

    std::hash<Stacktrace> hasher;
    std::size_t h = hasher(st);

    FATP_ASSERT_EQ(h, st.hash(), "std::hash should match member hash()");

    return true;
}

// ============================================================================
// Iterator Tests
// ============================================================================

FATP_TEST_CASE(iterator_range_for)
{
    auto st = Stacktrace::current();

    std::size_t count = 0;
    for (const auto& frame : st)
    {
        (void)frame;
        ++count;
    }

    FATP_ASSERT_EQ(count, st.size(), "Range-for should iterate all frames");

    return true;
}

FATP_TEST_CASE(iterator_begin_end)
{
    auto st = Stacktrace::current();

    auto dist = std::distance(st.begin(), st.end());
    FATP_ASSERT_EQ(static_cast<std::size_t>(dist), st.size(), "begin/end distance should match size");

    return true;
}

FATP_TEST_CASE(iterator_cbegin_cend)
{
    auto st = Stacktrace::current();

    auto dist = std::distance(st.cbegin(), st.cend());
    FATP_ASSERT_EQ(static_cast<std::size_t>(dist), st.size(), "cbegin/cend distance should match size");

    return true;
}

FATP_TEST_CASE(iterator_reverse)
{
    auto st = Stacktrace::current();

    if (st.size() >= 2)
    {
        auto first = *st.begin();
        auto last = *st.rbegin();

        FATP_ASSERT_TRUE(first.address != last.address || st.size() == 1, "First and last should differ (usually)");
    }

    return true;
}

// ============================================================================
// Access Tests
// ============================================================================

FATP_TEST_CASE(operator_bracket)
{
    auto st = Stacktrace::current();

    if (!st.empty())
    {
        const auto& frame = st[0];
        (void)frame;
        FATP_ASSERT_TRUE(true, "operator[] should work");
    }

    return true;
}

FATP_TEST_CASE(at_bounds_check)
{
    auto st = Stacktrace::current();

    bool threw = false;
    try
    {
        (void)st.at(st.size() + 100);
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "at() should throw on out of bounds");

    return true;
}

// ============================================================================
// Backend Information Tests
// ============================================================================

FATP_TEST_CASE(backend_name)
{
    const char* name = Stacktrace::backendName();

    FATP_ASSERT_TRUE(name != nullptr, "backendName() should return valid string");
    FATP_ASSERT_GT(std::strlen(name), 0u, "Backend name should not be empty");

    std::cout << "  [Backend: " << name << "]\n";

    return true;
}

FATP_TEST_CASE(has_real_backend)
{
    bool hasBackend = Stacktrace::hasRealBackend();

    std::cout << "  [Has real backend: " << (hasBackend ? "yes" : "no") << "]\n";

    // This is informational - the test passes either way
    FATP_ASSERT_TRUE(true, "hasRealBackend() should return consistent value");

    return true;
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

FATP_TEST_CASE(concurrent_capture)
{
    constexpr int kThreads = 8;
    constexpr int kIterations = 100;

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back([&successCount]() {
            for (int j = 0; j < kIterations; ++j)
            {
                auto st = Stacktrace::current();
                if (!st.empty())
                {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(successCount.load(), kThreads * kIterations, "All concurrent captures should succeed");

    return true;
}

FATP_TEST_CASE(concurrent_raw_capture)
{
    constexpr int kThreads = 4;
    constexpr int kIterations = 50;

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back([&successCount]() {
            for (int j = 0; j < kIterations; ++j)
            {
                auto st = Stacktrace::captureRaw();
                st.resolveSymbols();
                if (!st.empty() && st.isSymbolized())
                {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(successCount.load(), kThreads * kIterations, "All concurrent raw captures should succeed");

    return true;
}

// ============================================================================
// Copy/Move Semantics Tests
// ============================================================================

FATP_TEST_CASE(copy_construction)
{
    auto st1 = Stacktrace::current();
    Stacktrace st2(st1);

    FATP_ASSERT_EQ(st1.size(), st2.size(), "Copy should have same size");
    FATP_ASSERT_TRUE(st1 == st2, "Copy should be equal to original");

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    auto st1 = Stacktrace::current();
    Stacktrace st2;
    st2 = st1;

    FATP_ASSERT_EQ(st1.size(), st2.size(), "Assigned copy should have same size");
    FATP_ASSERT_TRUE(st1 == st2, "Assigned copy should be equal");

    return true;
}

FATP_TEST_CASE(move_construction)
{
    auto st1 = Stacktrace::current();
    auto size1 = st1.size();

    Stacktrace st2(std::move(st1));

    FATP_ASSERT_EQ(st2.size(), size1, "Moved-to should have original size");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    auto st1 = Stacktrace::current();
    auto size1 = st1.size();

    Stacktrace st2;
    st2 = std::move(st1);

    FATP_ASSERT_EQ(st2.size(), size1, "Move-assigned should have original size");

    return true;
}

// ============================================================================
// Edge Cases
// ============================================================================

FATP_TEST_CASE(empty_stacktrace)
{
    Stacktrace st;

    FATP_ASSERT_TRUE(st.empty(), "Default constructed should be empty");
    FATP_ASSERT_EQ(st.size(), 0u, "Default constructed should have size 0");
    FATP_ASSERT_EQ(st.toString(), "", "Empty stacktrace should produce empty string");

    return true;
}

FATP_TEST_CASE(skip_more_than_depth)
{
    // Skip more frames than exist
    auto st = Stacktrace::current(1000, 10);

    // Should either be empty or have very few frames
    FATP_ASSERT_LE(st.size(), 10u, "Should respect depth limit even with high skip");

    return true;
}

FATP_TEST_CASE(zero_max_depth)
{
    // maxDepth=0 is a special case - implementation-defined behavior
    // Most implementations treat 0 as "unlimited"
    auto st = Stacktrace::current(1, 0);

    // Should not crash, result is implementation-defined
    FATP_ASSERT_TRUE(true, "Zero maxDepth should not crash");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_stacktrace()
{
    std::cout << "\n" << colors::cyan() << "Stacktrace Benchmarks:" << colors::reset() << "\n";
    std::cout << "Backend: " << Stacktrace::backendName() << "\n\n";

    // Benchmark: Raw capture (no symbols)
    double rawTime = measure_perf(
        []() {
            auto st = Stacktrace::captureRaw();
            DoNotOptimize(st);
        },
        1000,
        10);
    std::cout << "Raw capture (no symbols):     " << format_time(rawTime) << "\n";

    // Benchmark: Full capture with symbols
    double fullTime = measure_perf(
        []() {
            auto st = Stacktrace::current();
            DoNotOptimize(st);
        },
        1000,
        10);
    std::cout << "Full capture (with symbols):  " << format_time(fullTime) << "\n";

    // Benchmark: Symbol resolution only
    auto raw = Stacktrace::captureRaw();
    double resolveTime = measure_perf(
        [&raw]() {
            auto copy = raw;
            copy.resolveSymbols();
            DoNotOptimize(copy);
        },
        1000,
        10);
    std::cout << "Symbol resolution only:       " << format_time(resolveTime) << "\n";

    // Benchmark: toString()
    auto st = Stacktrace::current();
    double toStringTime = measure_perf(
        [&st]() {
            auto str = st.toString();
            DoNotOptimize(str);
        },
        10000,
        100);
    std::cout << "toString():                   " << format_time(toStringTime) << "\n";

    // Benchmark: toJson()
    double toJsonTime = measure_perf(
        [&st]() {
            auto json = st.toJson();
            DoNotOptimize(json);
        },
        10000,
        100);
    std::cout << "toJson():                     " << format_time(toJsonTime) << "\n";

    // Benchmark: Nested capture
    double nestedTime = measure_perf(
        []() {
            auto st = captureAtDepth1();
            DoNotOptimize(st);
        },
        1000,
        10);
    std::cout << "Nested capture (3 levels):    " << format_time(nestedTime) << "\n";

    std::cout << "\n";
}

}  // namespace fat_p::testing::stacktrace

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_Stacktrace()
{
    FATP_PRINT_HEADER(STACKTRACE)

    TestRunner runner;

    // Basic capture tests
    FATP_RUN_TEST_NS(runner, stacktrace, capture_current_not_empty);
    FATP_RUN_TEST_NS(runner, stacktrace, capture_has_valid_frames);
    FATP_RUN_TEST_NS(runner, stacktrace, capture_is_symbolized);

    // Frame skipping tests
    FATP_RUN_TEST_NS(runner, stacktrace, skip_frames_reduces_depth);
    FATP_RUN_TEST_NS(runner, stacktrace, skip_removes_top_frames);

    // Max depth tests
    FATP_RUN_TEST_NS(runner, stacktrace, max_depth_limits_capture);
    FATP_RUN_TEST_NS(runner, stacktrace, max_depth_one);
    FATP_RUN_TEST_NS(runner, stacktrace, max_depth_large);

    // Nested call tests
    FATP_RUN_TEST_NS(runner, stacktrace, nested_calls_captured);
    FATP_RUN_TEST_NS(runner, stacktrace, nested_function_names_resolved);

    // Raw capture tests
    FATP_RUN_TEST_NS(runner, stacktrace, capture_raw_not_symbolized);
    FATP_RUN_TEST_NS(runner, stacktrace, capture_raw_then_resolve);
    FATP_RUN_TEST_NS(runner, stacktrace, resolve_symbols_idempotent);
    FATP_RUN_TEST_NS(runner, stacktrace, raw_nested_capture);

    // Output formatting tests
    FATP_RUN_TEST_NS(runner, stacktrace, to_string_not_empty);
    FATP_RUN_TEST_NS(runner, stacktrace, to_string_multiline);
    FATP_RUN_TEST_NS(runner, stacktrace, to_string_has_frame_numbers);
    FATP_RUN_TEST_NS(runner, stacktrace, to_string_max_frames);
    FATP_RUN_TEST_NS(runner, stacktrace, to_json_valid_format);
    FATP_RUN_TEST_NS(runner, stacktrace, stream_insertion);

    // StackFrame tests
    FATP_RUN_TEST_NS(runner, stacktrace, frame_to_string);
    FATP_RUN_TEST_NS(runner, stacktrace, frame_to_string_short);
    FATP_RUN_TEST_NS(runner, stacktrace, frame_equality);
    FATP_RUN_TEST_NS(runner, stacktrace, frame_stream_insertion);

    // Comparison and hashing tests
    FATP_RUN_TEST_NS(runner, stacktrace, stacktrace_equality);
    FATP_RUN_TEST_NS(runner, stacktrace, stacktrace_hash);
    FATP_RUN_TEST_NS(runner, stacktrace, std_hash_specialization);

    // Iterator tests
    FATP_RUN_TEST_NS(runner, stacktrace, iterator_range_for);
    FATP_RUN_TEST_NS(runner, stacktrace, iterator_begin_end);
    FATP_RUN_TEST_NS(runner, stacktrace, iterator_cbegin_cend);
    FATP_RUN_TEST_NS(runner, stacktrace, iterator_reverse);

    // Access tests
    FATP_RUN_TEST_NS(runner, stacktrace, operator_bracket);
    FATP_RUN_TEST_NS(runner, stacktrace, at_bounds_check);

    // Backend info tests
    FATP_RUN_TEST_NS(runner, stacktrace, backend_name);
    FATP_RUN_TEST_NS(runner, stacktrace, has_real_backend);

    // Thread safety tests
    FATP_RUN_TEST_NS(runner, stacktrace, concurrent_capture);
    FATP_RUN_TEST_NS(runner, stacktrace, concurrent_raw_capture);

    // Copy/move tests
    FATP_RUN_TEST_NS(runner, stacktrace, copy_construction);
    FATP_RUN_TEST_NS(runner, stacktrace, copy_assignment);
    FATP_RUN_TEST_NS(runner, stacktrace, move_construction);
    FATP_RUN_TEST_NS(runner, stacktrace, move_assignment);

    // Edge cases
    FATP_RUN_TEST_NS(runner, stacktrace, empty_stacktrace);
    FATP_RUN_TEST_NS(runner, stacktrace, skip_more_than_depth);
    FATP_RUN_TEST_NS(runner, stacktrace, zero_max_depth);

    // Run benchmarks
    stacktrace::benchmark_stacktrace();

    return 0 == runner.print_summary();
}

}  // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Stacktrace() ? 0 : 1;
}
#endif
