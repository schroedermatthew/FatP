/**
 * @file DiagnosticLogger_Json.h
 * @brief Extension for Structured JSON Logging.
 * @dependencies DiagnosticLogger_Core.h, JsonLite.h
 * 
 * FIXES APPLIED (v2.0):
 * - P4.1: Fixed JSON-in-JSON escaping - now properly embeds parsed JSON objects
 * - P4.2: Added missing log levels (ERROR, WARNING, FATAL, TRACE)
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
    fat_p::to_json(j, data);
    std::string jsonStr = fat_p::to_json_string(j);

    logger.log(level, "", loc, std::move(jsonStr));
}

template <typename T>
inline void logWithDataHelper(LogLevel level, std::string_view msg, const T& data, SourceLocation loc)
{
    auto& logger = getGlobalLogger();
    if (!logger.shouldLog(level)) return;

    fat_p::JsonValue j;
    fat_p::to_json(j, data);
    std::string jsonStr = fat_p::to_json_string(j);

    logger.log(level, std::string(msg), loc, std::move(jsonStr));
}

} // namespace diagnostic
} // namespace fat_p

#define LOG_TRACE_JSON(obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Trace)) { \
        ::fat_p::diagnostic::logJsonHelper(::fat_p::diagnostic::LogLevel::Trace, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_DEBUG_JSON(obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Debug)) { \
        ::fat_p::diagnostic::logJsonHelper(::fat_p::diagnostic::LogLevel::Debug, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_INFO_JSON(obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Info)) { \
        ::fat_p::diagnostic::logJsonHelper(::fat_p::diagnostic::LogLevel::Info, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_WARNING_JSON(obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Warning)) { \
        ::fat_p::diagnostic::logJsonHelper(::fat_p::diagnostic::LogLevel::Warning, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_ERROR_JSON(obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Error)) { \
        ::fat_p::diagnostic::logJsonHelper(::fat_p::diagnostic::LogLevel::Error, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_FATAL_JSON(obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Fatal)) { \
        ::fat_p::diagnostic::logJsonHelper(::fat_p::diagnostic::LogLevel::Fatal, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_TRACE_WITH_DATA(msg, obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Trace)) { \
        ::fat_p::diagnostic::logWithDataHelper(::fat_p::diagnostic::LogLevel::Trace, msg, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_DEBUG_WITH_DATA(msg, obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Debug)) { \
        ::fat_p::diagnostic::logWithDataHelper(::fat_p::diagnostic::LogLevel::Debug, msg, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_INFO_WITH_DATA(msg, obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Info)) { \
        ::fat_p::diagnostic::logWithDataHelper(::fat_p::diagnostic::LogLevel::Info, msg, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_WARNING_WITH_DATA(msg, obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Warning)) { \
        ::fat_p::diagnostic::logWithDataHelper(::fat_p::diagnostic::LogLevel::Warning, msg, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_ERROR_WITH_DATA(msg, obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Error)) { \
        ::fat_p::diagnostic::logWithDataHelper(::fat_p::diagnostic::LogLevel::Error, msg, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)

#define LOG_FATAL_WITH_DATA(msg, obj) \
    do { if (::fat_p::diagnostic::getGlobalLogger().shouldLog(::fat_p::diagnostic::LogLevel::Fatal)) { \
        ::fat_p::diagnostic::logWithDataHelper(::fat_p::diagnostic::LogLevel::Fatal, msg, obj, CPP_UTIL_SOURCE_LOCATION()); \
    }} while(0)
