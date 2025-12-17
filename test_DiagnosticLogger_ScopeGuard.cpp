/**
 * @file test_DiagnosticLogger_ScopeGuard.cpp
 * @brief Tests verifying ScopeGuard integration in DiagnosticLogger
 * 
 * These tests verify that:
 * 1. RotatingFileSink guarantees file reopening with ScopeGuard
 * 2. ResilientSink manages failure state correctly with ScopeGuard
 * 3. Test utilities using ScopeGuard work correctly
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

#include "DiagnosticLogger_IO.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_DiagnosticLogger_ScopeGuard.h"
#endif

namespace fat_p::testing
{

using namespace fat_p::diagnostic;
namespace fs = std::filesystem;

// anonymous
namespace
{

// ============================================================================
// Helper Functions
// ============================================================================

std::string readFileContents(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)), 
                       std::istreambuf_iterator<char>());
}

void cleanupTestFiles(const std::string& baseName, int maxIndex = 5)
{
    for (int i = 0; i <= maxIndex; ++i)
    {
        std::string file = baseName + (i == 0 ? "" : "." + std::to_string(i));
        if (fs::exists(file))
        {
            std::error_code ec;
            fs::remove(file, ec);
        }
    }
}

    // ============================================================================
    // ScopeGuard Safety Tests for RotatingFileSink
    // ============================================================================

    /**
     * @brief Test that file is guaranteed to reopen after rotation
     *
     * This test verifies that the SCOPE_GUARD in rotate() ensures the file
     * is always reopened, even if rotation fails.
     */
    bool test_rotating_file_guaranteed_reopen()
    {
        std::string filename = "test_guard_reopen.log";
        cleanupTestFiles(filename);

        auto sink = makeRotatingFileSink(filename, 200, 3);
        SIMPLE_ASSERT(sink != nullptr, "RotatingFileSink created");
        SIMPLE_ASSERT(sink->is_valid(), "Initial validity");

        auto loc = CPP_UTIL_SOURCE_LOCATION();

        // Write enough to trigger rotation
        for (int i = 0; i < 10; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(50, 'X'), loc);
            sink->write(record);
        }

        // Critical test: sink must still be valid after rotation
        SIMPLE_ASSERT(sink->is_valid(), "Valid after rotation - ScopeGuard worked");

        // Verify we can continue writing
        LogRecord postRotation(LogLevel::Info, "Post-rotation", loc);
        sink->write(postRotation);
        sink->flush();

        std::string contents = readFileContents(filename);
        SIMPLE_ASSERT(contents.find("Post-rotation") != std::string::npos,
            "Post-rotation write succeeded");

        cleanupTestFiles(filename);
        return true;
    }

    /**
     * @brief Test multiple rotation cycles to ensure consistent behavior
     */
    bool test_rotating_file_multiple_rotations_stability()
    {
        std::string filename = "test_guard_multi_rotate.log";
        cleanupTestFiles(filename);

        auto sink = makeRotatingFileSink(filename, 150, 3);
        auto loc = CPP_UTIL_SOURCE_LOCATION();

        // Trigger multiple rotations
        for (int i = 0; i < 50; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(40, 'A') + std::to_string(i), loc);
            sink->write(record);

            // Verify validity after each write
            if (!sink->is_valid())
            {
                SIMPLE_ASSERT(false, "Sink became invalid during rotations");
                cleanupTestFiles(filename);
                return false;
            }
        }

        // Final verification
        SIMPLE_ASSERT(sink->is_valid(), "Valid after all rotations");

        LogRecord final(LogLevel::Info, "FINAL_MESSAGE", loc);
        sink->write(final);
        sink->flush();

        std::string contents = readFileContents(filename);
        SIMPLE_ASSERT(contents.find("FINAL_MESSAGE") != std::string::npos,
            "Final message written successfully");

        cleanupTestFiles(filename);
        return true;
    }

    /**
     * @brief Stress test: rapid writes that trigger many rotations
     */
    bool test_rotating_file_rapid_rotation_stress()
    {
        std::string filename = "test_guard_stress.log";
        cleanupTestFiles(filename);

        // Very small file size to force frequent rotations
        auto sink = makeRotatingFileSink(filename, 100, 5);
        auto loc = CPP_UTIL_SOURCE_LOCATION();

        // Rapid writes
        for (int i = 0; i < 100; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(30, 'S'), loc);
            sink->write(record);
        }

        // Sink should still be operational
        SIMPLE_ASSERT(sink->is_valid(), "Sink survived stress test");

        sink->flush();

        // Verify at least one backup file exists (rotation occurred)
        bool rotationOccurred = false;
        for (int i = 1; i <= 5; ++i)
        {
            if (fs::exists(filename + "." + std::to_string(i)))
            {
                rotationOccurred = true;
                break;
            }
        }
        SIMPLE_ASSERT(rotationOccurred, "Rotation occurred during stress test");

        cleanupTestFiles(filename);
        return true;
    }

    // ============================================================================
    // ScopeGuard State Management Tests for ResilientSink
    // ============================================================================

    /**
     * @brief Test that failure state is correctly managed with ScopeGuard
     */
    bool test_resilient_sink_automatic_failure_marking()
    {
        // Custom sink that fails after N writes
        class ConditionallyFailingSink : public ISink
        {
        public:
            int writeCount = 0;
            int failAfter = 1;

            void write(const LogRecord&) override
            {
                ++writeCount;
                if (writeCount > failAfter)
                {
                    throw std::runtime_error("Intentional failure");
                }
            }

            void flush() override {}
        };

        std::string fallbackFile = "test_resilient_auto_fail.log";
        if (fs::exists(fallbackFile))
        {
            std::error_code ec;
            fs::remove(fallbackFile, ec);
        }

        auto primary = std::make_shared<ConditionallyFailingSink>();
        auto fallback = makeFileSink(fallbackFile);
        auto resilient = std::make_shared<ResilientSink>(primary, fallback);

        auto loc = CPP_UTIL_SOURCE_LOCATION();

        // First write succeeds
        LogRecord record1(LogLevel::Info, "Message1", loc);
        resilient->write(record1);
        SIMPLE_ASSERT(primary->writeCount == 1, "Primary succeeded first time");

        // Second write fails, ScopeGuard marks primary as failed
        LogRecord record2(LogLevel::Info, "Message2", loc);
        resilient->write(record2);
        SIMPLE_ASSERT(primary->writeCount == 2, "Primary attempted second time");

        // Third write should skip primary (it's marked as failed)
        LogRecord record3(LogLevel::Info, "Message3", loc);
        resilient->write(record3);
        SIMPLE_ASSERT(primary->writeCount == 2, "Primary not attempted after failure");

        resilient->flush();

        std::string contents = readFileContents(fallbackFile);
        SIMPLE_ASSERT(contents.find("Message2") != std::string::npos, "Fallback has Message2");
        SIMPLE_ASSERT(contents.find("Message3") != std::string::npos, "Fallback has Message3");

        if (fs::exists(fallbackFile))
        {
            std::error_code ec;
            fs::remove(fallbackFile, ec);
        }
        return true;
    }

    /**
     * @brief Test that ResilientSink handles primary returning early correctly
     */
    bool test_resilient_sink_early_return_path()
    {
        std::string primaryFile = "test_resilient_primary_early.log";
        std::string fallbackFile = "test_resilient_fallback_early.log";

        if (fs::exists(primaryFile))
        {
            std::error_code ec;
            fs::remove(primaryFile, ec);
        }
        if (fs::exists(fallbackFile))
        {
            std::error_code ec;
            fs::remove(fallbackFile, ec);
        }

        auto primary = makeFileSink(primaryFile);
        auto fallback = makeFileSink(fallbackFile);
        auto resilient = std::make_shared<ResilientSink>(primary, fallback);

        auto loc = CPP_UTIL_SOURCE_LOCATION();

        // Write succeeds on primary - should return early
        LogRecord record(LogLevel::Info, "Primary success", loc);
        resilient->write(record);
        resilient->flush();

        // Verify primary has the message
        std::string primaryContents = readFileContents(primaryFile);
        SIMPLE_ASSERT(primaryContents.find("Primary success") != std::string::npos,
            "Primary received message");

        std::string fallbackContents = readFileContents(fallbackFile);
        SIMPLE_ASSERT(fallbackContents.empty(), "Fallback not used on success");

        if (fs::exists(primaryFile))
        {
            std::error_code ec;
            fs::remove(primaryFile, ec);
        }
        if (fs::exists(fallbackFile))
        {
            std::error_code ec;
            fs::remove(fallbackFile, ec);
        }
        return true;
    }

    // ============================================================================
    // Test Utilities Using ScopeGuard
    // ============================================================================

    /**
     * @brief RAII guard for temporarily changing log level
     */
    class LogLevelGuard
    {
        Logger& logger_;
        LogLevel originalLevel_;

    public:
        LogLevelGuard(Logger& logger, LogLevel tempLevel)
            : logger_(logger)
            , originalLevel_(logger.getLevel())
        {
            logger_.setLevel(tempLevel);
        }

        ~LogLevelGuard()
        {
            logger_.setLevel(originalLevel_);
        }

        LogLevelGuard(const LogLevelGuard&) = delete;
        LogLevelGuard& operator=(const LogLevelGuard&) = delete;
    };

    /**
     * @brief Test that LogLevelGuard restores level correctly
     */
    bool test_log_level_guard_basic()
    {
        auto& logger = getGlobalLogger();
        LogLevel original = logger.getLevel();

        {
            LogLevelGuard guard(logger, LogLevel::Trace);
            SIMPLE_ASSERT(logger.getLevel() == LogLevel::Trace, "Level changed to Trace");
        }

        SIMPLE_ASSERT(logger.getLevel() == original, "Level restored after guard");

        return true;
    }

    /**
     * @brief Test that LogLevelGuard restores even with exceptions
     */
    bool test_log_level_guard_exception_safety()
    {
        auto& logger = getGlobalLogger();
        LogLevel original = logger.getLevel();

        try
        {
            LogLevelGuard guard(logger, LogLevel::Fatal);
            SIMPLE_ASSERT(logger.getLevel() == LogLevel::Fatal, "Level changed");

            // Simulate exception in test code
            throw std::runtime_error("Test exception");
        }
        catch (...)
        {
            // Guard destructor should have run
        }

        SIMPLE_ASSERT(logger.getLevel() == original, "Level restored despite exception");

        return true;
    }

    /**
     * @brief Test nested log level guards
     */
    bool test_log_level_guard_nesting()
    {
        auto& logger = getGlobalLogger();
        LogLevel original = logger.getLevel();

        {
            LogLevelGuard guard1(logger, LogLevel::Debug);
            SIMPLE_ASSERT(logger.getLevel() == LogLevel::Debug, "First guard applied");

            {
                LogLevelGuard guard2(logger, LogLevel::Error);
                SIMPLE_ASSERT(logger.getLevel() == LogLevel::Error, "Second guard applied");
            }

            SIMPLE_ASSERT(logger.getLevel() == LogLevel::Debug, "First guard restored");
        }

        SIMPLE_ASSERT(logger.getLevel() == original, "Original level restored");

        return true;
    }

    // ============================================================================
    // Integration Tests
    // ============================================================================

    /**
     * @brief Integration test: ResilientSink with RotatingFileSink
     */
    bool test_resilient_with_rotating_integration()
    {
        std::string primaryFile = "test_integ_primary.log";
        std::string fallbackFile = "test_integ_fallback.log";

        cleanupTestFiles(primaryFile);
        cleanupTestFiles(fallbackFile);

        auto primary = makeRotatingFileSink(primaryFile, 200, 3);
        auto fallback = makeRotatingFileSink(fallbackFile, 200, 3);
        auto resilient = std::make_shared<ResilientSink>(primary, fallback);

        auto loc = CPP_UTIL_SOURCE_LOCATION();

        // Write enough to trigger rotations
        for (int i = 0; i < 20; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(30, 'I') + std::to_string(i), loc);
            resilient->write(record);
        }

        resilient->flush();

        // Both sinks should be valid after rotations
        SIMPLE_ASSERT(primary->is_valid(), "Primary valid after rotations");
        SIMPLE_ASSERT(fallback->is_valid(), "Fallback valid after rotations");

        cleanupTestFiles(primaryFile);
        cleanupTestFiles(fallbackFile);
        return true;
    }

    /**
     * @brief Test that demonstrates all ScopeGuard benefits together
     */
    bool test_scope_guard_comprehensive_demo()
    {
        auto& logger = getGlobalLogger();
        std::string filename = "test_comprehensive.log";

        if (fs::exists(filename))
        {
            std::error_code ec;
            fs::remove(filename, ec);
        }

        // Use LogLevelGuard for temporary level change
        LogLevel originalLevel = logger.getLevel();
        {
            LogLevelGuard guard(logger, LogLevel::Trace);

            // Use RotatingFileSink with ScopeGuard-protected rotation
            auto sink = makeRotatingFileSink(filename, 150, 2);
            logger.clearSinks();
            logger.addSink(sink);

            auto cleanupLogger = fat_p::makeScopeGuard([&logger]() {
                logger.clearSinks();
                });

            // Generate enough logs to trigger rotation
            for (int i = 0; i < 30; ++i)
            {
                LOG_TRACE("Comprehensive test message " + std::to_string(i));
            }

            // Sink should still be valid
            SIMPLE_ASSERT(sink->is_valid(), "Sink valid throughout");

            // cleanupLogger will execute, logger will restore
        }

        // Level should be restored
        SIMPLE_ASSERT(logger.getLevel() == originalLevel, "Level restored");

        cleanupTestFiles(filename);
        return true;
    }

} // anonymous namespace

// ============================================================================
// Test Runner
// ============================================================================

bool test_DiagnosticLogger_ScopeGuard()
{
    PRINT_HEADER(DIAGNOSTIC LOGGER SCOPEGUARD)
    
    TestRunner runner;
    
    // RotatingFileSink ScopeGuard tests
    RUN_TEST(runner, rotating_file_guaranteed_reopen);
    RUN_TEST(runner, rotating_file_multiple_rotations_stability);
    RUN_TEST(runner, rotating_file_rapid_rotation_stress);
    
    // ResilientSink ScopeGuard tests
    RUN_TEST(runner, resilient_sink_automatic_failure_marking);
    RUN_TEST(runner, resilient_sink_early_return_path);
    
    // Test utility tests
    RUN_TEST(runner, log_level_guard_basic);
    RUN_TEST(runner, log_level_guard_exception_safety);
    RUN_TEST(runner, log_level_guard_nesting);
    
    // Integration tests
    RUN_TEST(runner, resilient_with_rotating_integration);
    RUN_TEST(runner, scope_guard_comprehensive_demo);
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_DiagnosticLogger_ScopeGuard() ? 0 : 1;
}
#endif
