// test_UltraLoggers.cpp - Comprehensive Tests for ULTRA Loggers (v3.x)
// ABSOLUTE FINAL VERSION - ALL MSVC ISSUES RESOLVED

#include "test_UltraLoggers.h"
#include "test_Utilities.h"

// Select version via define (change as needed)
#define VERSION_3_2  // or VERSION_3_1 or VERSION_3_0

#if defined(VERSION_3_0)
#include "AsyncLogger.h"
typedef cpp_utilities::diagnostic::ultra::AsyncLogger TestLogger;
#elif defined(VERSION_3_1)
#include "HybridLogger.h"
typedef cpp_utilities::diagnostic::ultra::UltraLogger TestLogger;
#elif defined(VERSION_3_2)
#include "PolicyLogger.h"
typedef cpp_utilities::diagnostic::ultra::Logger TestLogger;
#else
#error "Define VERSION_3_0, 3_1, or 3_2"
#endif

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <mutex>
#include <ctime>
#include <utility>

// MSVC-specific suppression
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable: 4996)
#endif

namespace cpp_utilities {
namespace testing {

using namespace std::chrono;
using namespace cpp_utilities::diagnostic::ultra;

// =============================================================================
// SFINAE Helper - MUST BE AT NAMESPACE SCOPE (NOT IN FUNCTION!)
// =============================================================================

#if defined(VERSION_3_2)
// Supergrok's superior pattern - encapsulated SFINAE detection
template<typename T>
struct has_startAsyncMode {
    template <typename U>
    static auto test(int) -> decltype(std::declval<U>().startAsyncMode(), std::true_type{});
    
    template <typename>
    static std::false_type test(...);
    
    static constexpr bool value = decltype(test<T>(0))::value;
};
#endif

// =============================================================================
// Helper Classes (Shared Test Sink)
// =============================================================================

class TestLoggerSink : public ISink {
public:
    std::vector<std::string> messages;
    mutable std::mutex mutex;
    
    void write(const LogRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex);
        messages.push_back(std::string(record.message, record.message_len));
    }
    
    void flush() override {}
    
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex);
        return messages.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        messages.clear();
    }
    
    std::string getLastMessage() const {
        std::lock_guard<std::mutex> lock(mutex);
        return messages.empty() ? "" : messages.back();
    }
};

// =============================================================================
// Test Suite 1: Functional Correctness
// =============================================================================

bool test_basic_logging() {
    std::cout << colors::cyan() << "Testing basic logging functionality..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    #if defined(VERSION_3_0) || defined(VERSION_3_1) || defined(VERSION_3_2)
    bool droppedCalled = false;
    logger.setOnDropCallback([&droppedCalled](const LogRecord&) { droppedCalled = true; });
    #endif
    
    logger.info("Test message");
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 1, "Should have 1 message");
    SIMPLE_ASSERT(testSinkPtr->getLastMessage() == "Test message", "Message should match");
    SIMPLE_ASSERT(!droppedCalled, "No drop callback should be called");
    
    std::cout << colors::green() << "  ✓ Basic logging works" 
              << colors::reset() << std::endl;
    return true;
}

bool test_level_filtering() {
    std::cout << colors::cyan() << "Testing level filtering..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.setMinLevel(LogLevel::Warning);
    
    logger.debug("Debug msg");
    logger.info("Info msg");
    logger.warning("Warning msg");
    logger.error("Error msg");
    
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 2, "Should have 2 messages (warn+error)");
    
    std::cout << colors::green() << "  ✓ Level filtering works" 
              << colors::reset() << std::endl;
    return true;
}

bool test_enable_disable() {
    std::cout << colors::cyan() << "Testing enable/disable..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.info("Message 1");
    logger.setEnabled(false);
    logger.info("Message 2");
    logger.setEnabled(true);
    logger.info("Message 3");
    
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 2, "Should have 2 messages");
    
    std::cout << colors::green() << "  ✓ Enable/disable works" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 2: Performance Benchmarks
// =============================================================================

bool test_disabled_performance() {
    std::cout << colors::cyan() << "Testing disabled performance..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    logger.setEnabled(false);
    
    constexpr size_t ITERATIONS = 1000000;
    
    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        logger.info("Test");
    }
    auto end = high_resolution_clock::now();
    
    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    
    std::cout << "  Disabled overhead: " << colors::bold() << ns << " ns/call" 
              << colors::reset() << std::endl;
    
    SIMPLE_ASSERT(ns < 10.0, "Disabled overhead should be <10ns");
    
    std::cout << colors::green() << "  ✓ Disabled performance good" 
              << colors::reset() << std::endl;
    return true;
}

bool test_filtered_performance() {
    std::cout << colors::cyan() << "Testing filtered performance..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    logger.setMinLevel(LogLevel::Off);
    
    constexpr size_t ITERATIONS = 1000000;
    
    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        logger.info("Test");
    }
    auto end = high_resolution_clock::now();
    
    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    
    std::cout << "  Filtered overhead: " << colors::bold() << ns << " ns/call" 
              << colors::reset() << std::endl;
    
    SIMPLE_ASSERT(ns < 10.0, "Filtered overhead should be <10ns");
    
    std::cout << colors::green() << "  ✓ Filtered performance good" 
              << colors::reset() << std::endl;
    return true;
}

bool test_active_performance() {
    std::cout << colors::cyan() << "Testing active performance..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    #if defined(VERSION_3_1) || defined(VERSION_3_2)
    logger.startAsyncMode();
    #endif
    
    constexpr size_t ITERATIONS = 100000;
    
    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        logger.info("Active test");
    }
    auto end = high_resolution_clock::now();
    
    double ns = std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
    
    logger.flush();
    
    std::cout << "  Active overhead: " << colors::bold() << ns << " ns/call" 
              << colors::reset() << std::endl;
    
    SIMPLE_ASSERT(ns < 200.0, "Active should be <200ns");
    
    std::cout << colors::green() << "  ✓ Active performance good" 
              << colors::reset() << std::endl;
    return true;
}

bool test_throughput() {
    std::cout << colors::cyan() << "Testing throughput..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    #if defined(VERSION_3_1) || defined(VERSION_3_2)
    logger.startAsyncMode();
    #endif
    
    constexpr size_t DURATION_MS = 1000;
    
    std::atomic<size_t> count{0};
    
    auto start = high_resolution_clock::now();
    auto deadline = start + std::chrono::milliseconds(DURATION_MS);
    
    while (high_resolution_clock::now() < deadline) {
        logger.info("Throughput test");
        ++count;
    }
    
    auto end = high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    
    size_t messages_per_sec = static_cast<size_t>(count / elapsed_sec);
    
    std::cout << "  Maximum throughput: " << colors::bold() 
              << messages_per_sec << " msg/sec" << colors::reset() << std::endl;
    
    logger.flush();
    
    std::cout << colors::green() << "  ✓ Throughput measured" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 3: Overflow Handling
// =============================================================================

bool test_overflow_handling() {
    std::cout << colors::cyan() << "Testing overflow handling..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    #if defined(VERSION_3_1) || defined(VERSION_3_2)
    logger.startAsyncMode();
    #endif
    
    bool droppedCalled = false;
    logger.setOnDropCallback([&droppedCalled](const LogRecord&) { droppedCalled = true; });
    
    constexpr size_t OVERFLOW_COUNT = CPP_UTIL_LOG_BUFFER_SIZE * 2;
    
    for (size_t i = 0; i < OVERFLOW_COUNT; ++i) {
        logger.info("Overflow test");
    }
    
    logger.flush();
    
    uint64_t logged = logger.getMessagesLogged();
    uint64_t dropped = logger.getMessagesDropped();
    
    std::cout << "  Logged: " << logged << ", Dropped: " << dropped << std::endl;
    
    SIMPLE_ASSERT(dropped > 0, "Should have dropped messages");
    SIMPLE_ASSERT(logged + dropped == OVERFLOW_COUNT, "Total should match");
    SIMPLE_ASSERT(droppedCalled, "Drop callback should be called");
    
    std::cout << colors::green() << "  ✓ Overflow handling works" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 4: Mode Switching (v3.1+)
// =============================================================================

bool test_mode_switching() {
    #if defined(VERSION_3_0)
    std::cout << colors::yellow() << "  ⚠ Skipping mode switching test (v3.0 always async)" 
              << colors::reset() << std::endl;
    return true;
    #else
    std::cout << colors::cyan() << "Testing mode switching..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.info("Sync message");
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 1, "Sync log should work");
    SIMPLE_ASSERT(!logger.isAsyncRunning(), "No worker in sync");
    
    logger.startAsyncMode();
    logger.info("Async message");
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 2, "Async log should work");
    SIMPLE_ASSERT(logger.isAsyncRunning(), "Worker should run in async");
    
    logger.stopAsyncMode();
    logger.info("Back to sync");
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 3, "Switch back should work");
    SIMPLE_ASSERT(!logger.isAsyncRunning(), "Worker stopped");
    
    std::cout << colors::green() << "  ✓ Mode switching works" 
              << colors::reset() << std::endl;
    return true;
    #endif
}

// =============================================================================
// Test Suite 5: Policy Constraints (v3.2 Only)
// =============================================================================

bool test_policy_constraints() {
    #if !defined(VERSION_3_2)
    std::cout << colors::yellow() << "  ⚠ Skipping policy constraints test (v3.2 only)" 
              << colors::reset() << std::endl;
    return true;
    #else
    std::cout << colors::cyan() << "Testing policy constraints (compile-time)..." 
              << colors::reset() << std::endl;
    
    // ✅ USE THE NAMESPACE-SCOPE TRAIT (defined at top of file)
    // NO TEMPLATES INSIDE THIS FUNCTION!
    static_assert(!has_startAsyncMode<SyncLogger>::value, 
                  "SyncLogger should not have startAsyncMode");
    static_assert(has_startAsyncMode<HybridLogger>::value, 
                  "HybridLogger should have startAsyncMode");
    static_assert(has_startAsyncMode<AsyncLogger>::value, 
                  "AsyncLogger should have startAsyncMode");
    
    std::cout << colors::green() << "  ✓ Policy constraints enforced at compile-time" 
              << colors::reset() << std::endl;
    return true;
    #endif
}

// =============================================================================
// Test Suite 6: Concurrency and Thread Safety
// =============================================================================

bool test_concurrency() {
    std::cout << colors::cyan() << "Testing concurrency..." 
              << colors::reset() << std::endl;
    
    TestLogger logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    #if defined(VERSION_3_1) || defined(VERSION_3_2)
    logger.startAsyncMode();
    #endif
    
    constexpr size_t NUM_THREADS = 4;
    constexpr size_t MSGS_PER_THREAD = 10000;
    
    std::vector<std::thread> threads;
    std::atomic<size_t> total_sent{0};
    
    for (size_t t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&logger, &total_sent]() {
            for (size_t i = 0; i < MSGS_PER_THREAD; ++i) {
                logger.info("Concurrent test");
                ++total_sent;
            }
        });
    }
    
    for (auto& thr : threads) {
        thr.join();
    }
    
    logger.flush();
    
    size_t received = testSinkPtr->count();
    uint64_t dropped = logger.getMessagesDropped();
    
    SIMPLE_ASSERT(received + dropped == total_sent.load(), "All messages accounted for");
    
    std::cout << colors::green() << "  ✓ Concurrency safe" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_UltraLoggers() {
    std::cout << "\n" << colors::bold() << colors::blue()
              << "═══════════════════════════════════════════════════════════\n"
              << "  ULTRA LOGGERS UNIT TESTS (v3.x)\n"
              << "  Covering Async (v3.0), Hybrid (v3.1), Policy-Based (v3.2)\n"
              << "═══════════════════════════════════════════════════════════"
              << colors::reset() << "\n\n";
    
    TestRunner runner;
    
    // Suite 1: Functional
    runner.run_test("Basic Logging", test_basic_logging);
    runner.run_test("Level Filtering", test_level_filtering);
    runner.run_test("Enable/Disable", test_enable_disable);
    
    // Suite 2: Performance
    runner.run_test("Disabled Perf", test_disabled_performance);
    runner.run_test("Filtered Perf", test_filtered_performance);
    runner.run_test("Active Perf", test_active_performance);
    runner.run_test("Throughput", test_throughput);
    
    // Suite 3: Overflow
    runner.run_test("Overflow Handling", test_overflow_handling);
    
    // Suite 4: Mode Switching (v3.1+)
    runner.run_test("Mode Switching", test_mode_switching);
    
    // Suite 5: Policies (v3.2)
    runner.run_test("Policy Constraints", test_policy_constraints);
    
    // Suite 6: Concurrency
    runner.run_test("Concurrency", test_concurrency);
    
    int failed = runner.print_summary();
    
    if (failed == 0) {
        std::cout << colors::green() << colors::bold()
                  << "✓ ALL ULTRA LOGGER TESTS PASSED!"
                  << colors::reset() << std::endl;
    } else {
        std::cout << colors::red() << colors::bold()
                  << "✗ SOME ULTRA LOGGER TESTS FAILED"
                  << colors::reset() << std::endl;
    }
    
    return failed == 0;
}

} // namespace testing
} // namespace cpp_utilities
