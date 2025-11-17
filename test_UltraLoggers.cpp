// test_UltraLoggers.cpp - Comprehensive Tests for ALL Ultra Logger Variants
// Tests SyncLogger, AsyncLogger, and HybridLogger from LoggerPolicy.h

#include "test_UltraLoggers.h"
#include "FatPTest.h"
#include "LoggerPolicy.h"

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

namespace fat_p {
namespace testing {

using namespace std::chrono;
using namespace fat_p::diagnostic::ultra;

// Type aliases for the three logger variants
using SyncLoggerType = SyncLogger;
using AsyncLoggerType = AsyncLogger;
using HybridLoggerType = HybridLogger;

// =============================================================================
// SFINAE Helper - Detect async mode support
// =============================================================================

template<typename T>
struct has_startAsyncMode {
    template <typename U>
    static auto test(int) -> decltype(std::declval<U>().startAsyncMode(), std::true_type{});
    
    template <typename>
    static std::false_type test(...);
    
    static constexpr bool value = decltype(test<T>(0))::value;
};

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
// Test Suite 1: Functional Correctness (All Logger Types)
// =============================================================================

template<typename LoggerType>
bool test_basic_logging_impl(const char* logger_name) {
    std::cout << colors::cyan() << "  [" << logger_name << "] Basic logging..." 
              << colors::reset() << std::endl;
    
    LoggerType logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    bool droppedCalled = false;
    logger.setOnDropCallback([&droppedCalled](const LogRecord&) { droppedCalled = true; });
    
    logger.info("Test message");
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 1, "Should have 1 message");
    SIMPLE_ASSERT(testSinkPtr->getLastMessage() == "Test message", "Message should match");
    SIMPLE_ASSERT(!droppedCalled, "No drop callback should be called");
    
    std::cout << colors::green() << "    ✓ Basic logging works" 
              << colors::reset() << std::endl;
    return true;
}

bool test_basic_logging() {
    std::cout << colors::cyan() << "Testing basic logging (all variants)..." 
              << colors::reset() << std::endl;
    
    bool passed = true;
    passed &= test_basic_logging_impl<SyncLoggerType>("SyncLogger");
    passed &= test_basic_logging_impl<AsyncLoggerType>("AsyncLogger");
    passed &= test_basic_logging_impl<HybridLoggerType>("HybridLogger");
    return passed;
}

template<typename LoggerType>
bool test_level_filtering_impl(const char* logger_name) {
    std::cout << colors::cyan() << "  [" << logger_name << "] Level filtering..." 
              << colors::reset() << std::endl;
    
    LoggerType logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.setMinLevel(LogLevel::Warning);
    
    logger.debug("Debug msg");
    logger.info("Info msg");
    logger.warning("Warning msg");
    logger.error("Error msg");
    
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 2, "Should have 2 messages (warn+error)");
    
    std::cout << colors::green() << "    ✓ Level filtering works" 
              << colors::reset() << std::endl;
    return true;
}

bool test_level_filtering() {
    std::cout << colors::cyan() << "Testing level filtering (all variants)..." 
              << colors::reset() << std::endl;
    
    bool passed = true;
    passed &= test_level_filtering_impl<SyncLoggerType>("SyncLogger");
    passed &= test_level_filtering_impl<AsyncLoggerType>("AsyncLogger");
    passed &= test_level_filtering_impl<HybridLoggerType>("HybridLogger");
    return passed;
}

template<typename LoggerType>
bool test_enable_disable_impl(const char* logger_name) {
    std::cout << colors::cyan() << "  [" << logger_name << "] Enable/disable..." 
              << colors::reset() << std::endl;
    
    LoggerType logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.info("Message 1");
    logger.setEnabled(false);
    logger.info("Message 2");
    logger.setEnabled(true);
    logger.info("Message 3");
    
    logger.flush();
    
    SIMPLE_ASSERT(testSinkPtr->count() == 2, "Should have 2 messages");
    
    std::cout << colors::green() << "    ✓ Enable/disable works" 
              << colors::reset() << std::endl;
    return true;
}

bool test_enable_disable() {
    std::cout << colors::cyan() << "Testing enable/disable (all variants)..." 
              << colors::reset() << std::endl;
    
    bool passed = true;
    passed &= test_enable_disable_impl<SyncLoggerType>("SyncLogger");
    passed &= test_enable_disable_impl<AsyncLoggerType>("AsyncLogger");
    passed &= test_enable_disable_impl<HybridLoggerType>("HybridLogger");
    return passed;
}

// =============================================================================
// Test Suite 2: Performance Benchmarks (AsyncLogger only for best performance)
// =============================================================================

bool test_disabled_performance() {
    std::cout << colors::cyan() << "Testing disabled performance..." 
              << colors::reset() << std::endl;
    
    AsyncLoggerType logger;
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
    
    SIMPLE_ASSERT(ns < 15.0, "Disabled overhead should be <15ns");
    
    std::cout << colors::green() << "  ✓ Disabled performance good" 
              << colors::reset() << std::endl;
    return true;
}

bool test_filtered_performance() {
    std::cout << colors::cyan() << "Testing filtered performance..." 
              << colors::reset() << std::endl;
    
    AsyncLoggerType logger;
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
    
    SIMPLE_ASSERT(ns < 15.0, "Filtered overhead should be <15ns");
    
    std::cout << colors::green() << "  Filtered performance good" 
              << colors::reset() << std::endl;
    return true;
}

bool test_active_performance() {
    std::cout << colors::cyan() << "Testing active performance..." 
              << colors::reset() << std::endl;
    
    AsyncLoggerType logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.startAsyncMode();
    
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
    
    AsyncLoggerType logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.startAsyncMode();
    
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
// Test Suite 3: Mode Switching (HybridLogger specific)
// =============================================================================

bool test_mode_switching() {
    std::cout << colors::cyan() << "Testing mode switching (HybridLogger)..." 
              << colors::reset() << std::endl;
    
    HybridLoggerType logger;
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
}

// =============================================================================
// Test Suite 4: Policy Constraints (Compile-time verification)
// =============================================================================

bool test_policy_constraints() {
    std::cout << colors::cyan() << "Testing policy constraints (compile-time)..." 
              << colors::reset() << std::endl;
    
    // Verify compile-time interface differences
    static_assert(!has_startAsyncMode<SyncLoggerType>::value, 
                  "SyncLogger should not have startAsyncMode");
    static_assert(has_startAsyncMode<HybridLoggerType>::value, 
                  "HybridLogger should have startAsyncMode");
    static_assert(has_startAsyncMode<AsyncLoggerType>::value, 
                  "AsyncLogger should have startAsyncMode");
    
    std::cout << colors::green() << "  ✓ Policy constraints enforced at compile-time" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Test Suite 5: Concurrency (AsyncLogger)
// =============================================================================

bool test_concurrency() {
    std::cout << colors::cyan() << "Testing concurrency (AsyncLogger)..." 
              << colors::reset() << std::endl;
    
    AsyncLoggerType logger;
    auto testSinkPtr = new TestLoggerSink();
    logger.addSink(std::unique_ptr<ISink>(testSinkPtr));
    
    logger.startAsyncMode();
    
    constexpr size_t NUM_THREADS = 4;
    constexpr size_t MSGS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    
    for (size_t t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&logger]() {
            for (size_t i = 0; i < MSGS_PER_THREAD; ++i) {
                logger.info("Concurrent test");
            }
        });
    }
    
    for (auto& thr : threads) {
        thr.join();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    logger.flush();
    
    size_t received = testSinkPtr->count();
    uint64_t logged = logger.getMessagesLogged();
    uint64_t dropped = logger.getMessagesDropped();
    
    constexpr size_t EXPECTED_TOTAL = NUM_THREADS * MSGS_PER_THREAD;
    
    std::cout << "  Expected: " << EXPECTED_TOTAL 
              << ", Logged: " << logged
              << ", Received: " << received
              << ", Dropped: " << dropped << std::endl;
    
    SIMPLE_ASSERT(logged + dropped == EXPECTED_TOTAL, "All attempts accounted for");
    
    std::cout << colors::green() << "  ✓ Concurrency safe" 
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_UltraLoggers() {

    PRINT_HEADER(ULTRA LOGGERS)

    TestRunner runner;
    
    // Suite 1: Functional (all variants)
    runner.run_test("Basic Logging (all)", test_basic_logging);
    runner.run_test("Level Filtering (all)", test_level_filtering);
    runner.run_test("Enable/Disable (all)", test_enable_disable);
    
    // Suite 2: Performance (AsyncLogger)
    runner.run_test("Disabled Perf", test_disabled_performance);
    runner.run_test("Filtered Perf", test_filtered_performance);
    runner.run_test("Active Perf", test_active_performance);
    runner.run_test("Throughput", test_throughput);
    
    // Suite 3: Mode Switching (HybridLogger)
    runner.run_test("Mode Switching", test_mode_switching);
    
    // Suite 4: Policy Constraints (compile-time)
    runner.run_test("Policy Constraints", test_policy_constraints);
    
    // Suite 5: Concurrency (AsyncLogger)
    runner.run_test("Concurrency", test_concurrency);
        
    return runner.print_summary() == 0;
}

} // namespace testing
} // namespace fat_p
