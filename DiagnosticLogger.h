/**
 * @file DiagnosticLogger_optimized.h (v1.0)
 * @brief High-performance diagnostic logging with lock-free fast path
 * 
 * 
 * PERFORMANCE TARGETS:
 * - Disabled logging: <10 ns/call (vs 33 ns before)
 * - Filtered logging: <10 ns/call (vs 29 ns before)
 * - Active logging: <130 ns/call (vs 177 ns before)
 * 
 * Features:
 * - Multiple log levels (Trace, Debug, Info, Warning, Error, Fatal)
 * - Compile-time log level filtering (zero overhead)
 * - Runtime filtering with lock-free atomics
 * - Multiple sinks (Console, File, Custom)
 * - Policy-based formatter design
 * - Thread-safe operations
 * - Lazy evaluation of log messages
 * - Easy integration with external logging frameworks
 * - Header-only, C++17, no dependencies beyond standard library
 */
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>  // For lock-free fast path
#include <chrono>
#include <iomanip>
#include <functional>
#include <type_traits>
#include <thread>

// --- Build Configuration Macros ---

#ifndef CPP_UTIL_MIN_LOG_LEVEL
#define CPP_UTIL_MIN_LOG_LEVEL 0
#endif

// Branch prediction hints for better performance
#ifndef LIKELY
#  if defined(__GNUC__) || defined(__clang__)
#    define LIKELY(x)   __builtin_expect(!!(x), 1)
#    define UNLIKELY(x) __builtin_expect(!!(x), 0)
#  else
#    define LIKELY(x)   (x)
#    define UNLIKELY(x) (x)
#  endif
#endif

// Portable function attributes
#ifndef FORCE_INLINE
#  if defined(_MSC_VER)
#    define FORCE_INLINE __forceinline
#  elif defined(__GNUC__) || defined(__clang__)
#    define FORCE_INLINE inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE inline
#  endif
#endif

#ifndef NO_INLINE
#  if defined(_MSC_VER)
#    define NO_INLINE __declspec(noinline)
#  elif defined(__GNUC__) || defined(__clang__)
#    define NO_INLINE __attribute__((noinline))
#  else
#    define NO_INLINE
#  endif
#endif

namespace fat_p {
namespace diagnostic {

// ============================================================================
// Log Level Enumeration
// ============================================================================

enum class LogLevel : int {
    Trace   = 0,
    Debug   = 1,
    Info    = 2,
    Warning = 3,
    Error   = 4,
    Fatal   = 5,
    Off     = 6
};

constexpr LogLevel gMinLogLevel = static_cast<LogLevel>(CPP_UTIL_MIN_LOG_LEVEL);

constexpr std::string_view logLevelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        case LogLevel::Off:     return "OFF";
    }
    return "UNKNOWN";
}

// ============================================================================
// Source Location
// ============================================================================

struct SourceLocation {
    const char* file;
    int line;
    const char* function;

    constexpr SourceLocation(
        const char* f = __builtin_FILE(),
        int l = __builtin_LINE(),
        const char* fn = __builtin_FUNCTION()
    ) noexcept : file(f), line(l), function(fn) {}
};

#define CPP_UTIL_SOURCE_LOCATION() \
    ::fat_p::diagnostic::SourceLocation(__FILE__, __LINE__, __func__)

// ============================================================================
// Log Record
// ============================================================================

struct LogRecord {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    SourceLocation location;
    std::thread::id threadId;

    LogRecord(LogLevel lvl, std::string msg, SourceLocation loc)
        : level(lvl)
        , timestamp(std::chrono::system_clock::now())
        , message(std::move(msg))
        , location(loc)
        , threadId(std::this_thread::get_id())
    {}
};

// ============================================================================
// Formatter Interface
// ============================================================================

class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual std::string format(const LogRecord& record) const = 0;
};

class DefaultFormatter : public IFormatter {
public:
    std::string format(const LogRecord& record) const override {
        std::ostringstream oss;
        
        // Timestamp
        auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            record.timestamp.time_since_epoch()
        ) % 1000;
        
        std::tm tm_buf;
        #ifdef _WIN32
            localtime_s(&tm_buf, &time_t);
        #else
            localtime_r(&time_t, &tm_buf);
        #endif
        
        oss << '[' << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        
        oss << '[' << logLevelToString(record.level) << "] ";
        oss << "[0x" << std::hex << record.threadId << std::dec << "] ";
        oss << record.message;
        
        if (record.location.file) {
            const char* filename = record.location.file;
            const char* slash = filename;
            while (*slash) {
                if (*slash == '/' || *slash == '\\') {
                    filename = slash + 1;
                }
                ++slash;
            }
            oss << " (" << filename << ':' << record.location.line << ')';
        }
        
        return oss.str();
    }
};

class SimpleFormatter : public IFormatter {
public:
    std::string format(const LogRecord& record) const override {
        std::ostringstream oss;
        oss << '[' << logLevelToString(record.level) << "] " << record.message;
        return oss.str();
    }
};

class JsonFormatter : public IFormatter {
public:
    std::string format(const LogRecord& record) const override {
        std::ostringstream oss;
        
        auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
        std::tm tm_buf;
        #ifdef _WIN32
            localtime_s(&tm_buf, &time_t);
        #else
            localtime_r(&time_t, &tm_buf);
        #endif
        
        oss << "{"
            << R"("timestamp":")" << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S") << "\","
            << R"("level":")" << logLevelToString(record.level) << "\","
            << R"("message":")" << escapeJson(record.message) << "\","
            << R"("file":")" << (record.location.file ? record.location.file : "") << "\","
            << R"("line":)" << record.location.line << ","
            << R"("function":")" << (record.location.function ? record.location.function : "") << "\""
            << "}";
        
        return oss.str();
    }

private:
    static std::string escapeJson(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 32) {
                        result += "\\u00";
                        result += "0123456789abcdef"[c >> 4];
                        result += "0123456789abcdef"[c & 0xF];
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }
};

// ============================================================================
// Sink Interface
// ============================================================================

class ISink {
public:
    virtual ~ISink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};

class ConsoleSink : public ISink {
    std::unique_ptr<IFormatter> formatter_;
    LogLevel minLevel_;
    mutable std::mutex mutex_;

public:
    explicit ConsoleSink(
        std::unique_ptr<IFormatter> formatter = std::make_unique<DefaultFormatter>(),
        LogLevel minLevel = LogLevel::Trace
    ) : formatter_(std::move(formatter)), minLevel_(minLevel) {}

    void write(const LogRecord& record) override {
        if (record.level < minLevel_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stream = (record.level >= LogLevel::Warning) ? std::cerr : std::cout;
        stream << formatter_->format(record) << '\n';
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout.flush();
        std::cerr.flush();
    }
};

class FileSink : public ISink {
    std::unique_ptr<IFormatter> formatter_;
    LogLevel minLevel_;
    std::ofstream file_;
    mutable std::mutex mutex_;
    std::string filename_;

public:
    explicit FileSink(
        const std::string& filename,
        std::unique_ptr<IFormatter> formatter = std::make_unique<DefaultFormatter>(),
        LogLevel minLevel = LogLevel::Trace,
        bool append = true
    ) : formatter_(std::move(formatter))
      , minLevel_(minLevel)
      , filename_(filename)
    {
        auto mode = std::ios_base::out;
        if (append) mode |= std::ios_base::app;
        file_.open(filename_, mode);
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename_);
        }
    }

    ~FileSink() override {
        if (file_.is_open()) {
            file_.close();
        }
    }

    void write(const LogRecord& record) override {
        if (record.level < minLevel_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_ << formatter_->format(record) << '\n';
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
    }
};

class CallbackSink : public ISink {
    std::function<void(const LogRecord&)> callback_;
    LogLevel minLevel_;
    mutable std::mutex mutex_;

public:
    explicit CallbackSink(
        std::function<void(const LogRecord&)> callback,
        LogLevel minLevel = LogLevel::Trace
    ) : callback_(std::move(callback)), minLevel_(minLevel) {}

    void write(const LogRecord& record) override {
        if (record.level < minLevel_) return;
        if (callback_) {
            std::lock_guard<std::mutex> lock(mutex_);
            callback_(record);
        }
    }

    void flush() override {}
};

// ============================================================================
// Logger - OPTIMIZED  with Ultra-Fast Path
// ============================================================================

class Logger {
    std::vector<std::unique_ptr<ISink>> sinks_;
    
    // OPTIMIZATION: Use atomics for lock-free fast path
    // Use unsigned char for enabled to pack better with LogLevel
    std::atomic<unsigned char> enabled_;
    std::atomic<LogLevel> runtimeMinLevel_;
    
    // Mutex only for sink operations (slow path)
    mutable std::mutex sinkMutex_;

public:
    Logger() 
        : enabled_(1)
        , runtimeMinLevel_(LogLevel::Trace)
    {}

    /**
     * @brief Add a sink to the logger
     */
    void addSink(std::unique_ptr<ISink> sink) {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        sinks_.push_back(std::move(sink));
    }

    /**
     * @brief Set the runtime minimum log level (lock-free)
     */
    void setMinLevel(LogLevel level) {
        runtimeMinLevel_.store(level, std::memory_order_relaxed);
    }

    /**
     * @brief Get current runtime minimum log level (lock-free)
     */
    LogLevel getMinLevel() const {
        return runtimeMinLevel_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Enable or disable logging at runtime (lock-free)
     */
    void setEnabled(bool enabled) {
        enabled_.store(enabled ? 1 : 0, std::memory_order_relaxed);
    }

    /**
     * @brief Check if logging is enabled (lock-free)
     */
    bool isEnabled() const {
        return enabled_.load(std::memory_order_relaxed) != 0;
    }

    /**
     * @brief Ultra-fast path check - single atomic load
     */
    FORCE_INLINE
    bool should_log(LogLevel level) const noexcept {
        // Combine checks to minimize atomic loads
        return LIKELY(enabled_.load(std::memory_order_relaxed)) &&
               LIKELY(level >= runtimeMinLevel_.load(std::memory_order_relaxed));
    }

    /**
     * @brief Log a message with lazy evaluation - OPTIMIZED FAST PATH
     * 
     * PERFORMANCE OPTIMIZATION:
     * 1. Compile-time filtering (if constexpr)
     * 2. Lock-free atomic checks (no mutex in hot path)
     * 3. Branch prediction hints (LIKELY/UNLIKELY)
     * 4. Double-checked locking pattern
     * 5. Separate hot path from cold path
     */
    template <typename MessageGenerator>
    FORCE_INLINE
    void log(LogLevel level, MessageGenerator&& messageGen, 
             SourceLocation location = SourceLocation()) {
        // OPTIMIZATION 1: Compile-time early exit (zero overhead)
        if constexpr (gMinLogLevel > LogLevel::Fatal) {
            return; // All logging disabled at compile time
        }
        
        // OPTIMIZATION 2: Compile-time level filtering
        if constexpr (gMinLogLevel > LogLevel::Trace) {
            if (level < gMinLogLevel) {
                return; // This level filtered at compile time
            }
        }
        
        // OPTIMIZATION 3: Combined lock-free check (ULTRA-FAST PATH)
        // Single branch that checks both enabled and level
        if (UNLIKELY(!should_log(level))) {
            return; // <5ns typical case
        }
        
        // SLOW PATH: Only execute if we're definitely logging
        // This keeps the mutex-protected code path out of the hot path
        log_slow_path(level, std::forward<MessageGenerator>(messageGen), location);
    }

    /**
     * @brief Flush all sinks
     */
    void flush() {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        for (auto& sink : sinks_) {
            sink->flush();
        }
    }

    /**
     * @brief Convenience methods for each log level - optimized for string literals
     */
    
    // String literal overloads (zero-overhead for disabled/filtered)
    FORCE_INLINE
    void trace(const char* msg, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Trace) {
            if (UNLIKELY(!should_log(LogLevel::Trace))) return;
            log_slow_path_string(LogLevel::Trace, msg, location);
        }
    }
    
    FORCE_INLINE
    void debug(const char* msg, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Debug) {
            if (UNLIKELY(!should_log(LogLevel::Debug))) return;
            log_slow_path_string(LogLevel::Debug, msg, location);
        }
    }
    
    FORCE_INLINE
    void info(const char* msg, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Info) {
            if (UNLIKELY(!should_log(LogLevel::Info))) return;
            log_slow_path_string(LogLevel::Info, msg, location);
        }
    }
    
    FORCE_INLINE
    void warning(const char* msg, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Warning) {
            if (UNLIKELY(!should_log(LogLevel::Warning))) return;
            log_slow_path_string(LogLevel::Warning, msg, location);
        }
    }
    
    FORCE_INLINE
    void error(const char* msg, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Error) {
            if (UNLIKELY(!should_log(LogLevel::Error))) return;
            log_slow_path_string(LogLevel::Error, msg, location);
        }
    }
    
    FORCE_INLINE
    void fatal(const char* msg, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Fatal) {
            if (UNLIKELY(!should_log(LogLevel::Fatal))) return;
            log_slow_path_string(LogLevel::Fatal, msg, location);
        }
    }
    
    // Lambda/callable overloads (for lazy evaluation)
    template <typename MessageGenerator>
    FORCE_INLINE
    void trace(MessageGenerator&& messageGen, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Trace) {
            log(LogLevel::Trace, std::forward<MessageGenerator>(messageGen), location);
        }
    }

    template <typename MessageGenerator>
    FORCE_INLINE
    void debug(MessageGenerator&& messageGen, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Debug) {
            log(LogLevel::Debug, std::forward<MessageGenerator>(messageGen), location);
        }
    }

    template <typename MessageGenerator>
    FORCE_INLINE
    void info(MessageGenerator&& messageGen, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Info) {
            log(LogLevel::Info, std::forward<MessageGenerator>(messageGen), location);
        }
    }

    template <typename MessageGenerator>
    FORCE_INLINE
    void warning(MessageGenerator&& messageGen, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Warning) {
            log(LogLevel::Warning, std::forward<MessageGenerator>(messageGen), location);
        }
    }

    template <typename MessageGenerator>
    FORCE_INLINE
    void error(MessageGenerator&& messageGen, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Error) {
            log(LogLevel::Error, std::forward<MessageGenerator>(messageGen), location);
        }
    }

    template <typename MessageGenerator>
    FORCE_INLINE
    void fatal(MessageGenerator&& messageGen, SourceLocation location = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Fatal) {
            log(LogLevel::Fatal, std::forward<MessageGenerator>(messageGen), location);
        }
    }

private:
    /**
     * @brief Slow path for string literals
     */
    NO_INLINE
    void log_slow_path_string(LogLevel level, const char* msg, SourceLocation location) {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        
        // Recheck conditions after acquiring lock
        if (!enabled_.load(std::memory_order_relaxed) || 
            level < runtimeMinLevel_.load(std::memory_order_relaxed) || 
            sinks_.empty()) {
            return;
        }
        
        LogRecord record{level, std::string(msg), std::move(location)};
        
        for (auto& sink : sinks_) {
            sink->write(record);
        }
    }
    
    /**
     * @brief Slow path: actual logging with mutex protection
     * 
     * Marked NO_INLINE to keep it out of the hot path,
     * reducing instruction cache pressure.
     */
    template <typename MessageGenerator>
    NO_INLINE
    void log_slow_path(LogLevel level, MessageGenerator&& messageGen, 
                      SourceLocation location) {
        // Double-checked locking: recheck conditions after acquiring lock
        std::lock_guard<std::mutex> lock(sinkMutex_);
        
        // Recheck enabled and level (they could have changed)
        if (!enabled_.load(std::memory_order_relaxed) || 
            level < runtimeMinLevel_.load(std::memory_order_relaxed) || 
            sinks_.empty()) {
            return;
        }
        
        // Generate message ONLY after all checks pass
        // Handle both callables and direct strings
        std::string message;
        using T = std::decay_t<MessageGenerator>;
        if constexpr (std::is_convertible_v<T, std::string> || 
                      std::is_convertible_v<T, const char*> ||
                      std::is_same_v<T, std::string>) {
            message = std::string(std::forward<MessageGenerator>(messageGen));
        } else {
            message = messageGen();
        }
        
        LogRecord record{level, std::move(message), std::move(location)};
        
        for (auto& sink : sinks_) {
            sink->write(record);
        }
    }
};

// ============================================================================
// Global Logger Instance
// ============================================================================

inline Logger& getGlobalLogger() {
    static Logger logger;
    return logger;
}

// ============================================================================
// OPTIMIZED Macros - Avoid Unnecessary stringstream for Literals
// ============================================================================

// Helper to detect if argument is a string literal or needs stringstream
namespace detail {
    template<typename T>
    struct is_string_literal : std::false_type {};
    
    template<size_t N>
    struct is_string_literal<const char(&)[N]> : std::true_type {};
    
    template<>
    struct is_string_literal<const char*> : std::true_type {};
}

// OPTIMIZATION: Smart macro that avoids stringstream for simple cases
#define LOG_TRACE(msg) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::Trace) { \
            ::fat_p::diagnostic::getGlobalLogger().trace([&]() -> std::string { \
                if constexpr (std::is_convertible_v<decltype(msg), std::string>) { \
                    return std::string(msg); \
                } else { \
                    std::ostringstream oss; oss << msg; return oss.str(); \
                } \
            }, CPP_UTIL_SOURCE_LOCATION()); \
        } \
    } while(0)

#define LOG_DEBUG(msg) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::Debug) { \
            ::fat_p::diagnostic::getGlobalLogger().debug([&]() -> std::string { \
                if constexpr (std::is_convertible_v<decltype(msg), std::string>) { \
                    return std::string(msg); \
                } else { \
                    std::ostringstream oss; oss << msg; return oss.str(); \
                } \
            }, CPP_UTIL_SOURCE_LOCATION()); \
        } \
    } while(0)

#define LOG_INFO(msg) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::Info) { \
            ::fat_p::diagnostic::getGlobalLogger().info([&]() -> std::string { \
                if constexpr (std::is_convertible_v<decltype(msg), std::string>) { \
                    return std::string(msg); \
                } else { \
                    std::ostringstream oss; oss << msg; return oss.str(); \
                } \
            }, CPP_UTIL_SOURCE_LOCATION()); \
        } \
    } while(0)

#define LOG_WARNING(msg) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::Warning) { \
            ::fat_p::diagnostic::getGlobalLogger().warning([&]() -> std::string { \
                if constexpr (std::is_convertible_v<decltype(msg), std::string>) { \
                    return std::string(msg); \
                } else { \
                    std::ostringstream oss; oss << msg; return oss.str(); \
                } \
            }, CPP_UTIL_SOURCE_LOCATION()); \
        } \
    } while(0)

#define LOG_ERROR(msg) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::Error) { \
            ::fat_p::diagnostic::getGlobalLogger().error([&]() -> std::string { \
                if constexpr (std::is_convertible_v<decltype(msg), std::string>) { \
                    return std::string(msg); \
                } else { \
                    std::ostringstream oss; oss << msg; return oss.str(); \
                } \
            }, CPP_UTIL_SOURCE_LOCATION()); \
        } \
    } while(0)

#define LOG_FATAL(msg) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::Fatal) { \
            ::fat_p::diagnostic::getGlobalLogger().fatal([&]() -> std::string { \
                if constexpr (std::is_convertible_v<decltype(msg), std::string>) { \
                    return std::string(msg); \
                } else { \
                    std::ostringstream oss; oss << msg; return oss.str(); \
                } \
            }, CPP_UTIL_SOURCE_LOCATION()); \
        } \
    } while(0)

// ============================================================================
// Backward Compatibility
// ============================================================================

#ifndef CXX_ENABLE_ERROR_MESSAGES
#define CXX_ENABLE_ERROR_MESSAGES true
#endif

inline constexpr bool gEnableErrorMessages = CXX_ENABLE_ERROR_MESSAGES;

template <typename F>
void conditionalPrintError(F messageGenerator) {
    if constexpr (gEnableErrorMessages) {
        std::cerr << messageGenerator() << std::endl;
    }
}

template <typename F>
void conditionalPrintErrorViaLogger(F messageGenerator) {
    if constexpr (gEnableErrorMessages) {
        if constexpr (gMinLogLevel <= LogLevel::Error) {
            getGlobalLogger().error(messageGenerator);
        }
    }
}

// ============================================================================
// Initialization Helpers
// ============================================================================

inline void initializeDefaultLogger() {
    auto& logger = getGlobalLogger();
    logger.addSink(std::make_unique<ConsoleSink>());
}

inline void initializeFileLogger(const std::string& filename, bool alsoConsole = true) {
    auto& logger = getGlobalLogger();
    
    if (alsoConsole) {
        logger.addSink(std::make_unique<ConsoleSink>());
    }
    
    logger.addSink(std::make_unique<FileSink>(filename));
}

} // namespace diagnostic
} // namespace fat_p
