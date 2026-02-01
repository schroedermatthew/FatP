/**
 * @file DiagnosticLogger_Sinks.h
 * @brief Standard sink implementations for the diagnostic logging system.
 *
 *
 *
 * @layer Domain
 *
 * @details This header provides concrete sink implementations including
 * ConsoleSink for stdout output and initialization utilities. Separated
 * from DiagnosticLogger_Core.h to allow lightweight inclusion of the
 * core logging infrastructure without pulling in <iostream>.
 *
 * LAZY INITIALIZATION:
 * Including this header automatically registers ConsoleSink as the default
 * sink factory. When you use LOG_INFO() etc. without explicitly adding sinks,
 * a ConsoleSink is automatically created on first use.
 *
 * To disable auto-initialization:
 * - Call logger.addSink(...) with your own sinks, OR
 * - Call logger.clearSinks() to explicitly use no sinks, OR
 * - Call logger.disableAutoInit() before any logging
 *
 * @dependencies DiagnosticLogger_Core.h
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: DiagnosticLogger_Sinks
  file_role: public_header
  path: include/fat_p/DiagnosticLogger_Sinks.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for DiagnosticLogger_Sinks."
  api_stability: in_work
  related:
    docs_search: "DiagnosticLogger_Sinks"
    tests:
      - components/DiagnosticLogger/tests/test_DiagnosticLogger_Core.cpp
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
#include "DiagnosticLogger_Core.h"

#include <iostream>

namespace fat_p
{
namespace diagnostic
{

/**
 * @brief A sink that writes formatted log records to stdout.
 *
 * @details Thread-safe sink implementation that outputs log messages
 * to the console. Each write operation is protected by a mutex to
 * ensure atomic output in multi-threaded environments.
 */
class ConsoleSink : public ISink
{
    std::unique_ptr<IFormatter> mFormatter;
    mutable std::mutex mMutex;

public:
    /**
     * @brief Constructs a ConsoleSink with an optional custom formatter.
     * @param fmt The formatter to use. Defaults to DefaultFormatter.
     */
    explicit ConsoleSink(std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
        : mFormatter(std::move(fmt))
    {
    }

    /**
     * @brief Writes a log record to stdout or stderr based on level.
     * @param record The log record to write.
     *
     * @details Warning, Error, and Fatal go to stderr; others go to stdout.
     */
    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (record.level >= LogLevel::Warning)
        {
            std::cerr << mFormatter->format(record) << '\n';
        }
        else
        {
            std::cout << mFormatter->format(record) << '\n';
        }
    }

    /**
     * @brief Flushes both stdout and stderr streams.
     */
    void flush() override
    {
        std::cout.flush();
        std::cerr.flush();
    }
};

/**
 * @brief A sink that writes formatted log records to stderr.
 *
 * @details Similar to ConsoleSink but writes to stderr instead of stdout.
 * Useful for error-only logging or when stdout is being used for data output.
 */
class StderrSink : public ISink
{
    std::unique_ptr<IFormatter> mFormatter;
    mutable std::mutex mMutex;

public:
    /**
     * @brief Constructs a StderrSink with an optional custom formatter.
     * @param fmt The formatter to use. Defaults to DefaultFormatter.
     */
    explicit StderrSink(std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
        : mFormatter(std::move(fmt))
    {
    }

    /**
     * @brief Writes a log record to stderr.
     * @param record The log record to write.
     */
    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        std::cerr << mFormatter->format(record) << '\n';
    }

    /**
     * @brief Flushes the stderr stream.
     */
    void flush() override
    {
        std::cerr.flush();
    }
};

namespace detail
{

/**
 * @brief Factory function that creates a ConsoleSink.
 * @return Shared pointer to a new ConsoleSink.
 */
inline std::shared_ptr<ISink> createConsoleSinkFactory()
{
    return std::make_shared<ConsoleSink>();
}

/**
 * @brief Self-registering static that registers ConsoleSink as the default.
 *
 * @details This static variable's initializer runs when the header is included,
 * registering createConsoleSinkFactory with the global SinkFactory. This enables
 * lazy initialization: when LOG_INFO() is called without any sinks configured,
 * a ConsoleSink is automatically created.
 */
inline const bool gConsoleSinkFactoryRegistered = []() {
    registerDefaultSinkFactory(&createConsoleSinkFactory);
    return true;
}();

// Prevent unused variable warning
inline void ensureFactoryRegistered()
{
    (void)gConsoleSinkFactoryRegistered;
}

} // namespace detail

/**
 * @brief Initializes the global logger with a default ConsoleSink.
 *
 * @details Convenience function that adds a ConsoleSink to the global
 * logger. Typically called once at application startup.
 *
 * @note With lazy initialization, this function is optional. Simply
 * using LOG_INFO() etc. will auto-initialize with ConsoleSink if no
 * sinks are configured. This function is provided for:
 * - Explicit initialization for clarity
 * - Backward compatibility
 * - Cases where you want to ensure initialization happens at a specific time
 */
inline void initializeDefaultLogger()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        getGlobalLogger().addSink(std::make_shared<ConsoleSink>());
    });
}

/**
 * @brief Initializes a named logger with a default ConsoleSink.
 * @param name The logger name.
 */
inline void initializeLogger(std::string_view name)
{
    getLogger(name).addSink(std::make_shared<ConsoleSink>());
}

/**
 * @brief Initializes all default sinks in the registry.
 *
 * @details Adds a ConsoleSink to the registry's default sinks, so all
 * newly created named loggers will automatically have console output.
 */
inline void initializeDefaultSinks()
{
    LoggerRegistry::instance().addDefaultSink(std::make_shared<ConsoleSink>());
}

} // namespace diagnostic
} // namespace fat_p
