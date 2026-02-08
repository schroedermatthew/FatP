#pragma once

/*
FATP_META:
  meta_version: 1
  component: DiagnosticLogger_TestUtilities
  file_role: public_header
  path: include/fat_p/DiagnosticLogger_TestUtilities.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for DiagnosticLogger_TestUtilities."
  api_stability: in_work
  related:
    docs_search: "DiagnosticLogger_TestUtilities"
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

/**
 * @file DiagnosticLogger_TestUtilities.h
 * @brief Test utilities for DiagnosticLogger using ScopeGuard for safe state management
 *
 * This file provides RAII-based utilities for temporarily changing logger state
 * during tests, ensuring automatic restoration even in the presence of exceptions.
 *
 */

#include "DiagnosticLogger_Core.h"
#include "ScopeGuard.h"

namespace fat_p
{
namespace diagnostic
{
namespace test_util
{

/**
 * @brief RAII guard for temporarily changing log level
 *
 * Automatically restores the original log level when the guard goes out of scope,
 * even if exceptions are thrown.
 *
 * Example:
 * @code
 * {
 *     LogLevelGuard guard(logger, LogLevel::Debug);
 *     // ... test code with debug logging enabled ...
 * } // Original level automatically restored
 * @endcode
 */
class LogLevelGuard
{
    Logger& mLogger;
    LogLevel mOriginalLevel;

public:
    explicit LogLevelGuard(Logger& logger, LogLevel tempLevel)
        : mLogger(logger)
        , mOriginalLevel(logger.getLevel())
    {
        mLogger.setLevel(tempLevel);
    }

    ~LogLevelGuard()
    {
        mLogger.setLevel(mOriginalLevel);
    }

    LogLevelGuard(const LogLevelGuard&) = delete;
    LogLevelGuard& operator=(const LogLevelGuard&) = delete;
    LogLevelGuard(LogLevelGuard&&) = delete;
    LogLevelGuard& operator=(LogLevelGuard&&) = delete;

    /**
     * @brief Get the original log level that will be restored
     */
    LogLevel originalLevel() const noexcept
    {
        return mOriginalLevel;
    }

    /**
     * @brief Create a ScopeGuard-based log level guard
     *
     * Alternative factory method that returns a ScopeGuard directly.
     */
    static auto makeScopeGuard(Logger& logger, LogLevel tempLevel)
    {
        auto originalLevel = logger.getLevel();
        logger.setLevel(tempLevel);

        return fat_p::makeScopeGuard([&logger, originalLevel]() {
            logger.setLevel(originalLevel);
        });
    }
};

/**
 * @brief RAII guard for temporarily enabling/disabling the logger
 *
 * Example:
 * @code
 * {
 *     LoggerEnabledGuard guard(logger, false); // Disable logging
 *     // ... test code with logging disabled ...
 * } // Original state automatically restored
 * @endcode
 */
class LoggerEnabledGuard
{
    Logger& mLogger;
    bool mOriginalState;

public:
    explicit LoggerEnabledGuard(Logger& logger, bool enabled)
        : mLogger(logger)
        , mOriginalState(logger.shouldLog(LogLevel::Trace)) // Check if currently enabled
    {
        mLogger.setEnabled(enabled);
    }

    ~LoggerEnabledGuard()
    {
        mLogger.setEnabled(mOriginalState);
    }

    LoggerEnabledGuard(const LoggerEnabledGuard&) = delete;
    LoggerEnabledGuard& operator=(const LoggerEnabledGuard&) = delete;
};

/**
 * @brief RAII guard for temporarily managing logger sinks
 *
 * Saves all current sinks on construction and restores them on destruction.
 * Optionally clears sinks after saving for a clean test configuration.
 *
 * Example:
 * @code
 * {
 *     SinkGuard guard(logger);
 *     logger.addSink(testSink); // Add test-specific sink
 *     // ... test code ...
 * } // Original sinks automatically restored
 * @endcode
 */
class SinkGuard
{
    Logger& mLogger;
    std::vector<std::shared_ptr<ISink>> mOriginalSinks;

public:
    explicit SinkGuard(Logger& logger, bool clearSinks = true)
        : mLogger(logger)
        , mOriginalSinks(logger.getSinks())
    {
        if (clearSinks)
        {
            mLogger.clearSinks();
        }
    }

    ~SinkGuard()
    {
        // Restore original sinks
        mLogger.setSinks(std::move(mOriginalSinks));
    }

    SinkGuard(const SinkGuard&) = delete;
    SinkGuard& operator=(const SinkGuard&) = delete;
};

/**
 * @brief Create a temporary test sink that captures log records
 *
 * Useful for verifying that specific log messages were generated during a test.
 */
class CapturingSink : public ISink
{
public:
    std::vector<LogRecord> records;
    mutable std::mutex mMutex;

    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        records.push_back(record);
    }

    void flush() override
    {
    }

    /**
     * @brief Check if any captured record contains the given message substring
     */
    bool containsMessage(const std::string& substr) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto& rec : records)
        {
            if (rec.message.find(substr) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Count records at a specific log level
     */
    size_t countLevel(LogLevel level) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return static_cast<size_t>(std::count_if(records.begin(), records.end(), [level](const LogRecord& rec) {
            return rec.level == level;
        }));
    }

    /**
     * @brief Clear all captured records
     */
    void clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        records.clear();
    }

    /**
     * @brief Get the number of captured records
     */
    size_t count() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return records.size();
    }
};

/**
 * @brief RAII guard that adds a sink temporarily and restores original sinks on destruction
 *
 * Saves current sinks, clears them, adds the temporary sink, then restores
 * original sinks when the guard goes out of scope.
 *
 * Example:
 * @code
 * auto capturingSink = std::make_shared<CapturingSink>();
 * {
 *     TemporarySinkGuard guard(logger, capturingSink);
 *     LOG_INFO("Test message");
 * } // Original sinks automatically restored
 *
 * assert(capturingSink->containsMessage("Test message"));
 * @endcode
 */
class TemporarySinkGuard
{
    Logger& mLogger;
    std::vector<std::shared_ptr<ISink>> mOriginalSinks;

public:
    TemporarySinkGuard(Logger& logger, std::shared_ptr<ISink> sink)
        : mLogger(logger)
        , mOriginalSinks(logger.getSinks())
    {
        mLogger.clearSinks();
        mLogger.addSink(std::move(sink));
    }

    ~TemporarySinkGuard()
    {
        mLogger.setSinks(std::move(mOriginalSinks));
    }

    TemporarySinkGuard(const TemporarySinkGuard&) = delete;
    TemporarySinkGuard& operator=(const TemporarySinkGuard&) = delete;
};

/**
 * @brief Convenience function to temporarily change log level for a code block
 *
 * Example:
 * @code
 * withLogLevel(logger, LogLevel::Trace, []() {
 *     // Code that needs trace logging
 * });
 * // Original level automatically restored
 * @endcode
 */
template <typename Func>
void withLogLevel(Logger& logger, LogLevel level, Func&& func)
{
    LogLevelGuard guard(logger, level);
    std::forward<Func>(func)();
}

/**
 * @brief Convenience function to capture logs during a code block
 *
 * Saves current sinks, replaces them with a capturing sink, executes the
 * function, then restores original sinks and returns captured records.
 *
 * Example:
 * @code
 * auto records = captureLogsFrom(logger, []() {
 *     LOG_INFO("Test message");
 * });
 *
 * assert(!records.empty());
 * @endcode
 */
template <typename Func>
std::vector<LogRecord> captureLogsFrom(Logger& logger, Func&& func)
{
    auto originalSinks = logger.getSinks();
    auto sink = std::make_shared<CapturingSink>();

    logger.clearSinks();
    logger.addSink(sink);

    auto guard = fat_p::makeScopeGuard([&logger, &originalSinks]() {
        logger.setSinks(std::move(originalSinks));
    });

    std::forward<Func>(func)();

    return sink->records;
}

} // namespace test_util
} // namespace diagnostic
} // namespace fat_p
