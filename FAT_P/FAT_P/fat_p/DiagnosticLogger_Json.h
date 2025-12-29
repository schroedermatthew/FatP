/**
 * @file DiagnosticLogger_Json.h
 * @brief Extension for Structured JSON Logging.
 * @dependencies DiagnosticLogger_Core.h, JsonLite.h
 * 
 * FIXES APPLIED (v2.1):
 * - ADL Support for user-defined types (using-declaration idiom)
 * - Compile-time filtering via if constexpr (zero-overhead guarantee)
 */
#pragma once

#include "DiagnosticLogger_Core.h"
#include "JsonLite.h"

namespace fat_p
{
namespace diagnostic
{

class JsonFormatter : public IFormatter
{
public:
    std::string format(const LogRecord& record) const override
    {
        fat_p::JsonObject json;
        
        auto time_t = std::chrono::system_clock::to_time_t(record.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(record.timestamp.time_since_epoch()) % 1000;
        std::tm tm_buf;
        #ifdef _WIN32
            localtime_s(&tm_buf, &time_t);
        #else
            localtime_r(&time_t, &tm_buf);
        #endif
        
        std::ostringstream ts;
        ts << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
        
        json["timestamp"] = ts.str();
        json["level"] = std::string(logLevelToString(record.level));
        json["message"] = record.message;
        
        std::ostringstream tid; 
        tid << std::hex << record.threadId;
        json["thread_id"] = tid.str();

        if (!record.metadata.empty())
        {
            try
            {
                auto parsed = fat_p::parse_json(record.metadata);
                json["data"] = parsed;
            }
            catch (...)
            {
                json["data_payload"] = record.metadata;
            }
        }

        if (record.location.file)
        {
            json["file"] = record.location.file;
            json["line"] = static_cast<int64_t>(record.location.line);
        }

        return fat_p::to_json_string(json);
    }
};

template <typename T>
inline void logJsonHelper(LogLevel level, const T& data, SourceLocation loc)
{
    auto& logger = getGlobalLogger();
    if (!logger.shouldLog(level)) return;

    fat_p::JsonValue j;
    using fat_p::to_json;  // Bring library to_json into scope for fallback
    to_json(j, data);      // Unqualified call enables ADL for user types
    std::string jsonStr = fat_p::to_json_string(j);

    logger.log(level, "", loc, std::move(jsonStr));
}

template <typename T>
inline void logWithDataHelper(LogLevel level, std::string_view msg, const T& data, SourceLocation loc)
{
    auto& logger = getGlobalLogger();
    if (!logger.shouldLog(level)) return;

    fat_p::JsonValue j;
    using fat_p::to_json;  // Bring library to_json into scope for fallback
    to_json(j, data);      // Unqualified call enables ADL for user types
    std::string jsonStr = fat_p::to_json_string(j);

    logger.log(level, std::string(msg), loc, std::move(jsonStr));
}

} // namespace diagnostic
} // namespace fat_p

// HELPER MACROS TO ENSURE COMPILE-TIME REMOVAL
#define LOG_JSON_MACRO_IMPL(func, obj) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::func) { \
            if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::func)) { \
                ::fat_p::diagnostic::logJsonHelper(::fat_p::diagnostic::LogLevel::func, obj, CPP_UTIL_SOURCE_LOCATION()); \
            } \
        } \
    } while (0)

#define LOG_WITH_DATA_MACRO_IMPL(func, msg, obj) \
    do { \
        if constexpr (::fat_p::diagnostic::gMinLogLevel <= ::fat_p::diagnostic::LogLevel::func) { \
            if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::func)) { \
                ::fat_p::diagnostic::logWithDataHelper(::fat_p::diagnostic::LogLevel::func, msg, obj, CPP_UTIL_SOURCE_LOCATION()); \
            } \
        } \
    } while (0)

// PUBLIC MACROS
#define LOG_TRACE_JSON(obj) LOG_JSON_MACRO_IMPL(Trace, obj)
#define LOG_DEBUG_JSON(obj) LOG_JSON_MACRO_IMPL(Debug, obj)
#define LOG_INFO_JSON(obj) LOG_JSON_MACRO_IMPL(Info, obj)
#define LOG_WARNING_JSON(obj) LOG_JSON_MACRO_IMPL(Warning, obj)
#define LOG_ERROR_JSON(obj) LOG_JSON_MACRO_IMPL(Error, obj)
#define LOG_FATAL_JSON(obj) LOG_JSON_MACRO_IMPL(Fatal, obj)

#define LOG_TRACE_WITH_DATA(msg, obj) LOG_WITH_DATA_MACRO_IMPL(Trace, msg, obj)
#define LOG_DEBUG_WITH_DATA(msg, obj) LOG_WITH_DATA_MACRO_IMPL(Debug, msg, obj)
#define LOG_INFO_WITH_DATA(msg, obj) LOG_WITH_DATA_MACRO_IMPL(Info, msg, obj)
#define LOG_WARNING_WITH_DATA(msg, obj) LOG_WITH_DATA_MACRO_IMPL(Warning, msg, obj)
#define LOG_ERROR_WITH_DATA(msg, obj) LOG_WITH_DATA_MACRO_IMPL(Error, msg, obj)
#define LOG_FATAL_WITH_DATA(msg, obj) LOG_WITH_DATA_MACRO_IMPL(Fatal, msg, obj)
