/**
 * @file DiagnosticLogger_Core.h
 * @brief Core high-performance logging infrastructure.
 * @layer CoreUtility
 *
 * PHILOSOPHY: "The Race Car"
 * - Zero overhead when disabled (<5ns).
 * - Minimal compile time impact.
 * - Strict C++17 compliance.
 *
 * This header provides the core Logger class, interfaces (ISink, IFormatter),
 * logging macros, LoggerRegistry for named loggers, and SinkFactory for lazy
 * initialization. Concrete sink implementations like ConsoleSink are provided
 * in DiagnosticLogger_Sinks.h to avoid pulling in <iostream>.
 *
 * FEATURES:
 * - Named loggers via LoggerRegistry
 * - Lazy initialization via SinkFactory
 * - Compile-time log level filtering
 * - Lock-free fast path checks
 * - Thread-safe sink management
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: DiagnosticLogger_Core
  file_role: public_header
  path: fat_p/DiagnosticLogger_Core.h
  namespace: fat_p
  summary: "Public header for DiagnosticLogger_Core."
  api_stability: in_work
  related:
    docs_search: "DiagnosticLogger_Core"
    tests:
      - tests/test_DiagnosticLogger_Core.cpp
  hygiene:
    pragma_once: true
    include_guard: true
    defines_total: 26
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef FATP_MIN_LOG_LEVEL
#define FATP_MIN_LOG_LEVEL 0
#endif

#ifndef FATP_LIKELY
#if defined(__GNUC__) || defined(__clang__)
#define FATP_LIKELY(x) __builtin_expect(!!(x), 1)
#define FATP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define FATP_LIKELY(x) (x)
#define FATP_UNLIKELY(x) (x)
#endif
#endif

#ifndef FATP_FORCE_INLINE
#if defined(_MSC_VER)
#define FATP_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FATP_FORCE_INLINE inline __attribute__((always_inline))
#else
#define FATP_FORCE_INLINE inline
#endif
#endif

#ifndef FATP_NO_INLINE
#if defined(_MSC_VER)
#define FATP_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define FATP_NO_INLINE __attribute__((noinline))
#else
#define FATP_NO_INLINE
#endif
#endif

namespace fat_p
{
namespace diagnostic
{

/**
 * @brief Enumeration of available log levels.
 */
enum class LogLevel : int
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5,
    Off = 6
};

/**
 * @brief Compile-time minimum log level threshold.
 */
constexpr LogLevel gMinLogLevel = static_cast<LogLevel>(FATP_MIN_LOG_LEVEL);

/**
 * @brief Converts a LogLevel to its string representation.
 * @param level The log level to convert.
 * @return A string_view containing the level name.
 */
constexpr std::string_view logLevelToString(LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
        default:
            return "OFF";
    }
}

/**
 * @brief Captures source code location information.
 */
struct SourceLocation
{
    const char* file;
    int line;
    const char* function;

    constexpr SourceLocation(const char* f, int l, const char* fn)
        : file(f)
        , line(l)
        , function(fn)
    {
    }
};

#define FATP_SOURCE_LOCATION()                                                                 \
    ::fat_p::diagnostic::SourceLocation(__FILE__, __LINE__, __func__)

/**
 * @brief Contains all information about a single log event.
 */
struct LogRecord
{
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    std::string metadata;
    SourceLocation location;
    std::thread::id threadId;

    LogRecord()
        : level(LogLevel::Info)
        , timestamp(std::chrono::system_clock::now())
        , location("", 0, "")
        , threadId(std::this_thread::get_id())
    {
    }

    LogRecord(LogLevel lvl, std::string msg, SourceLocation loc, std::string meta = "")
        : level(lvl)
        , timestamp(std::chrono::system_clock::now())
        , message(std::move(msg))
        , metadata(std::move(meta))
        , location(loc)
        , threadId(std::this_thread::get_id())
    {
    }
};

/**
 * @brief Interface for log record formatters.
 */
class IFormatter
{
public:
    virtual ~IFormatter() = default;

    /**
     * @brief Formats a log record into a string.
     * @param record The log record to format.
     * @return The formatted string representation.
     */
    virtual std::string format(const LogRecord& record) const = 0;
};

/**
 * @brief Default formatter producing human-readable log output.
 *
 * @details Output format:
 * [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [0xTHREADID] message | metadata (file:line)
 */
class DefaultFormatter : public IFormatter
{
public:
    std::string format(const LogRecord& record) const override
    {
        std::ostringstream oss;

        auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      record.timestamp.time_since_epoch()) %
                  1000;
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        oss << '[' << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
            << std::setw(3) << ms.count() << "] ";
        oss << '[' << logLevelToString(record.level) << "] ";

        std::ostringstream tid_oss;
        tid_oss << std::hex << record.threadId;
        oss << "[0x" << tid_oss.str() << "] ";

        oss << record.message;
        if (!record.metadata.empty())
        {
            oss << " | " << record.metadata;
        }

        if (record.location.file)
        {
            const char* fn = record.location.file;
            const char* slash = fn;
            while (*slash)
            {
                if (*slash == '/' || *slash == '\\')
                {
                    fn = slash + 1;
                }
                ++slash;
            }
            oss << " (" << fn << ':' << record.location.line << ')';
        }

        return oss.str();
    }
};

/**
 * @brief Simple formatter producing minimal output: [LEVEL] message
 */
class SimpleFormatter : public IFormatter
{
public:
    std::string format(const LogRecord& record) const override
    {
        std::ostringstream oss;
        oss << "[" << logLevelToString(record.level) << "] " << record.message;
        return oss.str();
    }
};

/**
 * @brief Interface for log output destinations.
 */
class ISink
{
public:
    virtual ~ISink() = default;

    /**
     * @brief Writes a log record to the sink.
     * @param record The log record to write.
     */
    virtual void write(const LogRecord& record) = 0;

    /**
     * @brief Flushes any buffered output.
     */
    virtual void flush()
    {
    }
};

// Forward declarations
class Logger;
class LoggerRegistry;

/**
 * @brief Function pointer type for creating default sinks.
 *
 * @details Used by the lazy initialization system. When a logger has no sinks
 * and a factory is registered, the factory is called to create a default sink.
 */
using SinkFactory = std::shared_ptr<ISink> (*)();

namespace detail
{

/**
 * @brief Global sink factory for lazy initialization.
 */
inline std::atomic<SinkFactory> gDefaultSinkFactory{nullptr};

} // namespace detail

/**
 * @brief Registers a factory function for creating default sinks.
 *
 * @details Called automatically by DiagnosticLogger_Sinks.h to register
 * ConsoleSink as the default. Users can call this to override with a
 * custom default sink.
 *
 * @param factory Function that creates a sink, or nullptr to disable.
 */
inline void registerDefaultSinkFactory(SinkFactory factory) noexcept
{
    detail::gDefaultSinkFactory.store(factory, std::memory_order_release);
}

/**
 * @brief Creates a sink using the registered factory.
 * @return A new sink instance, or nullptr if no factory is registered.
 */
inline std::shared_ptr<ISink> createDefaultSink()
{
    auto factory = detail::gDefaultSinkFactory.load(std::memory_order_acquire);
    return factory ? factory() : nullptr;
}

/**
 * @brief Thread-safe logger with configurable sinks.
 *
 * @details The Logger class provides high-performance logging with:
 * - Compile-time log level filtering via FATP_MIN_LOG_LEVEL
 * - Runtime log level filtering via setLevel()
 * - Multiple output sinks with thread-safe management
 * - Lazy message evaluation to avoid formatting overhead when disabled
 * - Automatic initialization with default sink on first use
 */
class Logger
{
    std::shared_ptr<std::vector<std::shared_ptr<ISink>>> sinks_;
    mutable std::mutex sinkMutex_;
    std::atomic<bool> enabled_{true};
    std::atomic<LogLevel> runtimeMinLevel_{LogLevel::Trace};
    std::once_flag autoInitFlag_;
    std::atomic<bool> autoInitDisabled_{false};

public:
    Logger()
        : sinks_(std::make_shared<std::vector<std::shared_ptr<ISink>>>())
    {
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief Adds a sink to receive log output.
     * @param sink The sink to add.
     *
     * @details Adding a sink disables auto-initialization, giving the user
     * full control over sink configuration.
     */
    void addSink(std::shared_ptr<ISink> sink)
    {
        autoInitDisabled_.store(true, std::memory_order_release);
        if (!sink)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(sinkMutex_);
        auto new_sinks = std::make_shared<std::vector<std::shared_ptr<ISink>>>(*sinks_);
        new_sinks->push_back(std::move(sink));
        sinks_ = new_sinks;
    }

    /**
     * @brief Removes all sinks from the logger.
     *
     * @details Also disables auto-initialization.
     */
    void clearSinks()
    {
        autoInitDisabled_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(sinkMutex_);
        sinks_ = std::make_shared<std::vector<std::shared_ptr<ISink>>>();
    }

    /**
     * @brief Replaces all sinks with the given vector of sinks.
     * @param newSinks The new sinks to use.
     */
    void setSinks(std::vector<std::shared_ptr<ISink>> newSinks)
    {
        autoInitDisabled_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(sinkMutex_);
        sinks_ = std::make_shared<std::vector<std::shared_ptr<ISink>>>(std::move(newSinks));
    }

    /**
     * @brief Returns a copy of all currently configured sinks.
     * @return Vector of sink shared pointers.
     */
    std::vector<std::shared_ptr<ISink>> getSinks() const
    {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        return sinks_ ? *sinks_ : std::vector<std::shared_ptr<ISink>>{};
    }

    /**
     * @brief Checks if the logger has any sinks configured.
     * @return True if at least one sink is present.
     */
    bool hasSinks() const
    {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        return sinks_ && !sinks_->empty();
    }

    /**
     * @brief Returns the number of sinks currently configured.
     * @return Number of sinks.
     */
    size_t sinkCount() const
    {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        return sinks_ ? sinks_->size() : 0;
    }

    /**
     * @brief Disables automatic sink initialization.
     *
     * @details Call this if you want to use the logger without any sinks,
     * preventing the auto-initialization from adding a default sink.
     */
    void disableAutoInit() noexcept
    {
        autoInitDisabled_.store(true, std::memory_order_release);
    }

    /**
     * @brief Sets the runtime minimum log level.
     * @param level The minimum level to log.
     */
    void setLevel(LogLevel level) noexcept
    {
        runtimeMinLevel_.store(level, std::memory_order_release);
    }

    /**
     * @brief Alias for setLevel() to match manual documentation.
     * @param level The minimum level to log.
     */
    void setMinLevel(LogLevel level) noexcept
    {
        setLevel(level);
    }

    /**
     * @brief Gets the current runtime minimum log level.
     * @return The current minimum log level.
     */
    LogLevel getLevel() const noexcept
    {
        return runtimeMinLevel_.load(std::memory_order_acquire);
    }

    /**
     * @brief Alias for getLevel() to match manual documentation.
     * @return The current minimum log level.
     */
    LogLevel getMinLevel() const noexcept
    {
        return getLevel();
    }

    /**
     * @brief Enables or disables the logger.
     * @param e True to enable, false to disable.
     */
    void setEnabled(bool e) noexcept
    {
        enabled_.store(e, std::memory_order_release);
    }

    /**
     * @brief Checks if the logger is enabled.
     * @return True if enabled.
     */
    bool isEnabled() const noexcept
    {
        return enabled_.load(std::memory_order_acquire);
    }

    /**
     * @brief Checks if a message at the given level would be logged.
     * @param level The log level to check.
     * @return True if the message would be logged.
     */
    FATP_FORCE_INLINE bool shouldLog(LogLevel level) const noexcept
    {
        return FATP_LIKELY(enabled_.load(std::memory_order_relaxed)) &&
               FATP_LIKELY(level >= runtimeMinLevel_.load(std::memory_order_relaxed));
    }

    /**
     * @brief Logs a message with lazy evaluation.
     * @tparam MessageGenerator Callable or convertible-to-string type.
     * @param level The log level.
     * @param messageGen The message generator (lambda, string, etc.).
     * @param location Source code location.
     * @param metadata Optional structured metadata.
     */
    template <typename MessageGenerator>
    FATP_FORCE_INLINE void log(LogLevel level,
                          MessageGenerator&& messageGen,
                          SourceLocation location,
                          std::string metadata = "")
    {
        // Optimization: Logging is usually the cold path.
        if (FATP_UNLIKELY(shouldLog(level)))
        {
            log_slow_path(level, std::forward<MessageGenerator>(messageGen), location,
                          std::move(metadata));
        }
    }

    template <typename T>
    FATP_FORCE_INLINE void trace(T&& msg, SourceLocation loc)
    {
        log(LogLevel::Trace, std::forward<T>(msg), loc);
    }

    template <typename T>
    FATP_FORCE_INLINE void debug(T&& msg, SourceLocation loc)
    {
        log(LogLevel::Debug, std::forward<T>(msg), loc);
    }

    template <typename T>
    FATP_FORCE_INLINE void info(T&& msg, SourceLocation loc)
    {
        log(LogLevel::Info, std::forward<T>(msg), loc);
    }

    template <typename T>
    FATP_FORCE_INLINE void warning(T&& msg, SourceLocation loc)
    {
        log(LogLevel::Warning, std::forward<T>(msg), loc);
    }

    template <typename T>
    FATP_FORCE_INLINE void error(T&& msg, SourceLocation loc)
    {
        log(LogLevel::Error, std::forward<T>(msg), loc);
    }

    template <typename T>
    FATP_FORCE_INLINE void fatal(T&& msg, SourceLocation loc)
    {
        log(LogLevel::Fatal, std::forward<T>(msg), loc);
    }

    /**
     * @brief Flushes all sinks.
     */
    void flush()
    {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        if (sinks_)
        {
            for (auto& sink : *sinks_)
            {
                sink->flush();
            }
        }
    }

private:
    void tryAutoInit()
    {
        if (autoInitDisabled_.load(std::memory_order_acquire))
        {
            return;
        }

        std::call_once(autoInitFlag_, [this]() {
            if (autoInitDisabled_.load(std::memory_order_acquire))
            {
                return;
            }

            std::lock_guard<std::mutex> lock(sinkMutex_);
            if (sinks_->empty())
            {
                auto sink = createDefaultSink();
                if (sink)
                {
                    auto newSinks =
                        std::make_shared<std::vector<std::shared_ptr<ISink>>>(*sinks_);
                    newSinks->push_back(std::move(sink));
                    sinks_ = newSinks;
                }
            }
        });
    }

    template <typename MessageGenerator>
    FATP_NO_INLINE void log_slow_path(LogLevel level,
                                 MessageGenerator&& messageGen,
                                 SourceLocation location,
                                 std::string metadata)
    {
        tryAutoInit();

        std::shared_ptr<std::vector<std::shared_ptr<ISink>>> local_sinks;
        {
            std::lock_guard<std::mutex> lock(sinkMutex_);
            local_sinks = sinks_;
        }

        if (!shouldLog(level) || !local_sinks || local_sinks->empty())
        {
            return;
        }

        std::string message;
        using T = std::decay_t<MessageGenerator>;

        if constexpr (std::is_convertible_v<T, std::string> || std::is_same_v<T, const char*>)
        {
            message = std::string(std::forward<MessageGenerator>(messageGen));
        }
        else if constexpr (std::is_invocable_v<T>)
        {
            message = messageGen();
        }
        else
        {
            std::ostringstream oss;
            oss << messageGen;
            message = oss.str();
        }

        LogRecord record{level, std::move(message), location, std::move(metadata)};

        for (auto& sink : *local_sinks)
        {
            sink->write(record);
        }

        if (level >= LogLevel::Error)
        {
            for (auto& sink : *local_sinks)
            {
                sink->flush();
            }
        }
    }
};

/**
 * @brief Registry for named loggers with thread-safe access.
 *
 * @details Provides a centralized registry for managing multiple loggers
 * by name. Loggers are created on first access and can be configured
 * with default sinks and levels.
 *
 * The registry uses a read-optimized locking strategy (shared_mutex)
 * since logger lookups vastly outnumber logger creation.
 *
 * Usage:
 * @code
 * auto& logger = LoggerRegistry::instance().get("network");
 * logger.info("Connected", FATP_SOURCE_LOCATION());
 *
 * // Or use the convenience function
 * getLogger("network").info("Connected", FATP_SOURCE_LOCATION());
 * @endcode
 */
class LoggerRegistry
{
public:
    /**
     * @brief Returns the singleton registry instance.
     * @return Reference to the global LoggerRegistry.
     */
    static LoggerRegistry& instance() noexcept
    {
        static LoggerRegistry registry;
        return registry;
    }

    /**
     * @brief Gets a logger by name, creating it if necessary.
     *
     * @param name Logger name. Empty string returns the global logger.
     * @return Reference to the named logger.
     *
     * @details New loggers inherit default sinks and level from the registry.
     * If no default sinks are configured but a SinkFactory is registered,
     * the factory will be used for auto-initialization on first log.
     */
    Logger& get(std::string_view name)
    {
        std::string nameStr(name);

        // Fast path: read lock for existing logger
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = loggers_.find(nameStr);
            if (it != loggers_.end())
            {
                return *it->second;
            }
        }

        // Slow path: write lock for creation
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Double-check after acquiring write lock
        auto it = loggers_.find(nameStr);
        if (it != loggers_.end())
        {
            return *it->second;
        }

        // Create new logger
        auto logger = std::make_shared<Logger>();
        logger->setLevel(defaultLevel_);

        // Apply default sinks if configured
        for (const auto& sink : defaultSinks_)
        {
            logger->addSink(sink);
        }

        // If user configured default sinks, disable auto-init for this logger
        // (they've taken control). Otherwise, leave auto-init enabled.
        if (!defaultSinks_.empty())
        {
            logger->disableAutoInit();
        }

        auto [inserted_it, success] = loggers_.emplace(std::move(nameStr), std::move(logger));
        return *inserted_it->second;
    }

    /**
     * @brief Gets a logger as a shared_ptr for caching.
     * @param name Logger name.
     * @return Shared pointer to the named logger.
     */
    std::shared_ptr<Logger> getShared(std::string_view name)
    {
        // Ensure logger exists
        get(name);

        std::string nameStr(name);
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = loggers_.find(nameStr);
        return it != loggers_.end() ? it->second : nullptr;
    }

    /**
     * @brief Checks if a logger with the given name exists.
     * @param name Logger name to check.
     * @return True if the logger exists.
     */
    bool exists(std::string_view name) const
    {
        std::string nameStr(name);
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return loggers_.find(nameStr) != loggers_.end();
    }

    /**
     * @brief Removes a logger by name.
     * @param name Logger name to remove.
     * @return True if a logger was removed.
     */
    bool drop(std::string_view name)
    {
        std::string nameStr(name);
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = loggers_.find(nameStr);
        if (it != loggers_.end())
        {
            loggers_.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief Removes all loggers from the registry.
     */
    void dropAll()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        loggers_.clear();
    }

    /**
     * @brief Sets the default log level for newly created loggers.
     * @param level The default level.
     */
    void setDefaultLevel(LogLevel level)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        defaultLevel_ = level;
    }

    /**
     * @brief Gets the default log level for new loggers.
     * @return The default level.
     */
    LogLevel getDefaultLevel() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return defaultLevel_;
    }

    /**
     * @brief Adds a sink to be added to all newly created loggers.
     * @param sink The sink to add as a default.
     */
    void addDefaultSink(std::shared_ptr<ISink> sink)
    {
        if (!sink)
        {
            return;
        }
        std::unique_lock<std::shared_mutex> lock(mutex_);
        defaultSinks_.push_back(std::move(sink));
    }

    /**
     * @brief Clears all default sinks.
     */
    void clearDefaultSinks()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        defaultSinks_.clear();
    }

    /**
     * @brief Returns the names of all registered loggers.
     * @return Vector of logger names.
     */
    std::vector<std::string> names() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::string> result;
        result.reserve(loggers_.size());
        for (const auto& [name, logger] : loggers_)
        {
            result.push_back(name);
        }
        return result;
    }

    /**
     * @brief Returns the number of registered loggers.
     * @return Logger count.
     */
    size_t count() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return loggers_.size();
    }

    /**
     * @brief Sets the log level on all registered loggers.
     * @param level The level to set.
     */
    void setAllLevels(LogLevel level)
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (auto& [name, logger] : loggers_)
        {
            logger->setLevel(level);
        }
    }

    /**
     * @brief Adds a sink to all registered loggers.
     * @param sink The sink to add.
     */
    void addSinkToAll(std::shared_ptr<ISink> sink)
    {
        if (!sink)
        {
            return;
        }
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (auto& [name, logger] : loggers_)
        {
            logger->addSink(sink);
        }
    }

private:
    LoggerRegistry() = default;
    LoggerRegistry(const LoggerRegistry&) = delete;
    LoggerRegistry& operator=(const LoggerRegistry&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    std::vector<std::shared_ptr<ISink>> defaultSinks_;
    LogLevel defaultLevel_{LogLevel::Trace};
};

/**
 * @brief Gets a named logger from the global registry.
 * @param name Logger name.
 * @return Reference to the named logger.
 */
inline Logger& getLogger(std::string_view name)
{
    return LoggerRegistry::instance().get(name);
}

/**
 * @brief Returns the global logger instance (named "").
 * @return Reference to the global Logger.
 *
 * @details This is equivalent to getLogger("") and provides backward
 * compatibility with code that uses the global logger directly.
 */
inline Logger& getGlobalLogger()
{
    return LoggerRegistry::instance().get("");
}

// Global logger macros

#define FATP_LOG_MACRO_IMPL(func, msg)                                                                  \
    do                                                                                             \
    {                                                                                              \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::func)    \
        {                                                                                          \
            ::fat_p::diagnostic::getGlobalLogger().log(                                            \
                ::fat_p::diagnostic::LogLevel::func,                                               \
                [&]() {                                                                            \
                    std::ostringstream _oss_;                                                      \
                    _oss_ << msg;                                                                  \
                    return _oss_.str();                                                            \
                },                                                                                 \
                FATP_SOURCE_LOCATION());                                                       \
        }                                                                                          \
    } while (0)

#define FATP_LOG_TRACE(msg) FATP_LOG_MACRO_IMPL(Trace, msg)
#define FATP_LOG_DEBUG(msg) FATP_LOG_MACRO_IMPL(Debug, msg)
#define FATP_LOG_INFO(msg) FATP_LOG_MACRO_IMPL(Info, msg)
#define FATP_LOG_WARNING(msg) FATP_LOG_MACRO_IMPL(Warning, msg)
#define FATP_LOG_ERROR(msg) FATP_LOG_MACRO_IMPL(Error, msg)
#define FATP_LOG_FATAL(msg) FATP_LOG_MACRO_IMPL(Fatal, msg)

// Named logger macros with static caching for zero lookup overhead after first call

#define FATP_LOG_TO_IMPL(logger_name, func, msg)                                                        \
    do                                                                                             \
    {                                                                                              \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::func)    \
        {                                                                                          \
            static ::fat_p::diagnostic::Logger& _cached_logger_ =                                  \
                ::fat_p::diagnostic::getLogger(logger_name);                                       \
            _cached_logger_.log(::fat_p::diagnostic::LogLevel::func,                               \
                                [&]() {                                                            \
                                    std::ostringstream _oss_;                                      \
                                    _oss_ << msg;                                                  \
                                    return _oss_.str();                                            \
                                },                                                                 \
                                FATP_SOURCE_LOCATION());                                       \
        }                                                                                          \
    } while (0)

#define FATP_LOG_TRACE_TO(name, msg) FATP_LOG_TO_IMPL(name, Trace, msg)
#define FATP_LOG_DEBUG_TO(name, msg) FATP_LOG_TO_IMPL(name, Debug, msg)
#define FATP_LOG_INFO_TO(name, msg) FATP_LOG_TO_IMPL(name, Info, msg)
#define FATP_LOG_WARNING_TO(name, msg) FATP_LOG_TO_IMPL(name, Warning, msg)
#define FATP_LOG_ERROR_TO(name, msg) FATP_LOG_TO_IMPL(name, Error, msg)
#define FATP_LOG_FATAL_TO(name, msg) FATP_LOG_TO_IMPL(name, Fatal, msg)

} // namespace diagnostic
} // namespace fat_p
