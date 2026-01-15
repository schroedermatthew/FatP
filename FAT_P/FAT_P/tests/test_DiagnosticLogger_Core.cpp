/**
 * @file test_DiagnosticLogger_Core.cpp
 * @brief Comprehensive unit tests for DiagnosticLogger_Core.h
 */
/*
FATP_META:
  meta_version: 1
  component: DiagnosticLogger_Core
  file_role: test
  path: tests/test_DiagnosticLogger_Core.cpp
  namespace: fat_p::testing::diagnosticlogger_core
  summary: "Unit tests for DiagnosticLogger_Core."
  related:
    docs_search: "DiagnosticLogger_Core"
    headers:
      - fat_p/DiagnosticLogger_Core.h
      - fat_p/DiagnosticLogger_Sinks.h
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
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "DiagnosticLogger_Core.h"
#include "DiagnosticLogger_Sinks.h"
#include "FatPTest.h"

namespace fat_p::testing::diagnosticlogger_core
{

using namespace fat_p::diagnostic;

class TestSink : public ISink
{
    std::vector<LogRecord> mRecords;
    mutable std::mutex mMutex;

public:
    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRecords.push_back(record);
    }

    void flush() override
    {
    }

    std::vector<LogRecord> getRecords() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mRecords;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRecords.clear();
    }

    size_t count() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mRecords.size();
    }

    bool containsMessage(const std::string& substr) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto& rec : mRecords)
        {
            if (rec.message.find(substr) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    size_t countLevel(LogLevel level) const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        size_t count = 0;
        for (const auto& rec : mRecords)
        {
            if (rec.level == level)
            {
                ++count;
            }
        }
        return count;
    }
};

namespace
{

FATP_TEST_CASE(log_level_enum)
{
    FATP_ASSERT_TRUE(LogLevel::Trace < LogLevel::Debug, "Trace < Debug");
    FATP_ASSERT_TRUE(LogLevel::Debug < LogLevel::Info, "Debug < Info");
    FATP_ASSERT_TRUE(LogLevel::Info < LogLevel::Warning, "Info < Warning");
    FATP_ASSERT_TRUE(LogLevel::Warning < LogLevel::Error, "Warning < Error");
    FATP_ASSERT_TRUE(LogLevel::Error < LogLevel::Fatal, "Error < Fatal");
    FATP_ASSERT_TRUE(LogLevel::Fatal < LogLevel::Off, "Fatal < Off");

    FATP_ASSERT_TRUE(logLevelToString(LogLevel::Trace) == "TRACE", "Trace string");
    FATP_ASSERT_TRUE(logLevelToString(LogLevel::Debug) == "DEBUG", "Debug string");
    FATP_ASSERT_TRUE(logLevelToString(LogLevel::Info) == "INFO", "Info string");
    FATP_ASSERT_TRUE(logLevelToString(LogLevel::Warning) == "WARN", "Warning string");
    FATP_ASSERT_TRUE(logLevelToString(LogLevel::Error) == "ERROR", "Error string");
    FATP_ASSERT_TRUE(logLevelToString(LogLevel::Fatal) == "FATAL", "Fatal string");
    FATP_ASSERT_TRUE(logLevelToString(LogLevel::Off) == "OFF", "Off string");

    return true;
}

FATP_TEST_CASE(source_location)
{
    auto loc = FATP_SOURCE_LOCATION();

    FATP_ASSERT_TRUE(loc.file != nullptr, "File is not null");
    FATP_ASSERT_TRUE(loc.line > 0, "Line is positive");
    FATP_ASSERT_TRUE(loc.function != nullptr, "Function is not null");
    FATP_ASSERT_TRUE(std::string(loc.function).find("test_source_location") != std::string::npos,
                     "Function name captured");

    return true;
}

FATP_TEST_CASE(log_record_construction)
{
    auto loc = FATP_SOURCE_LOCATION();
    LogRecord record(LogLevel::Info, "Test message", loc, "metadata");

    FATP_ASSERT_TRUE(record.level == LogLevel::Info, "Level set correctly");
    FATP_ASSERT_TRUE(record.message == "Test message", "Message set correctly");
    FATP_ASSERT_TRUE(record.metadata == "metadata", "Metadata set correctly");
    FATP_ASSERT_TRUE(record.location.file != nullptr, "Location file set");
    FATP_ASSERT_TRUE(record.location.line > 0, "Location line set");

    LogRecord default_record;
    FATP_ASSERT_TRUE(default_record.level == LogLevel::Info, "Default level is Info");
    FATP_ASSERT_TRUE(default_record.message.empty(), "Default message is empty");
    FATP_ASSERT_TRUE(default_record.metadata.empty(), "Default metadata is empty");

    return true;
}

FATP_TEST_CASE(default_formatter)
{
    DefaultFormatter formatter;
    auto loc = FATP_SOURCE_LOCATION();
    LogRecord record(LogLevel::Warning, "Test warning", loc, "extra data");

    std::string formatted = formatter.format(record);

    FATP_ASSERT_TRUE(!formatted.empty(), "Formatted string not empty");
    FATP_ASSERT_TRUE(formatted.find("WARN") != std::string::npos, "Contains log level");
    FATP_ASSERT_TRUE(formatted.find("Test warning") != std::string::npos, "Contains message");
    FATP_ASSERT_TRUE(formatted.find("extra data") != std::string::npos, "Contains metadata");
    FATP_ASSERT_TRUE(formatted.find("0x") != std::string::npos, "Contains thread ID");

    return true;
}

FATP_TEST_CASE(console_sink)
{
    std::ostringstream capturedOutput;
    std::streambuf* oldCoutBuf = std::cout.rdbuf();
    std::cout.rdbuf(capturedOutput.rdbuf());

    ConsoleSink sink;
    auto loc = FATP_SOURCE_LOCATION();
    LogRecord record(LogLevel::Info, "Console test", loc);

    sink.write(record);

    std::cout.rdbuf(oldCoutBuf);

    std::string output = capturedOutput.str();
    FATP_ASSERT_TRUE(output.find("Console test") != std::string::npos, "Message in console output");

    return true;
}

FATP_TEST_CASE(stderr_sink)
{
    std::ostringstream capturedOutput;
    std::streambuf* oldCerrBuf = std::cerr.rdbuf();
    std::cerr.rdbuf(capturedOutput.rdbuf());

    StderrSink sink;
    auto loc = FATP_SOURCE_LOCATION();
    LogRecord record(LogLevel::Error, "Stderr test", loc);

    sink.write(record);

    std::cerr.rdbuf(oldCerrBuf);

    std::string output = capturedOutput.str();
    FATP_ASSERT_TRUE(output.find("Stderr test") != std::string::npos, "Message in stderr output");

    return true;
}

FATP_TEST_CASE(logger_enable_disable)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    logger.setEnabled(true);
    FATP_ASSERT_TRUE(logger.isEnabled(), "Logger reports enabled");

    logger.log(LogLevel::Info, "Enabled message", FATP_SOURCE_LOCATION());
    FATP_ASSERT_TRUE(testSink->count() == 1, "Message logged when enabled");

    testSink->clear();
    logger.setEnabled(false);
    FATP_ASSERT_TRUE(!logger.isEnabled(), "Logger reports disabled");

    logger.log(LogLevel::Info, "Disabled message", FATP_SOURCE_LOCATION());
    FATP_ASSERT_TRUE(testSink->count() == 0, "No message logged when disabled");

    logger.setEnabled(true);

    return true;
}

FATP_TEST_CASE(logger_level_filtering)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    logger.setLevel(LogLevel::Warning);

    logger.log(LogLevel::Trace, "Trace msg", FATP_SOURCE_LOCATION());
    logger.log(LogLevel::Debug, "Debug msg", FATP_SOURCE_LOCATION());
    logger.log(LogLevel::Info, "Info msg", FATP_SOURCE_LOCATION());
    FATP_ASSERT_TRUE(testSink->count() == 0, "Lower levels filtered out");

    logger.log(LogLevel::Warning, "Warning msg", FATP_SOURCE_LOCATION());
    logger.log(LogLevel::Error, "Error msg", FATP_SOURCE_LOCATION());
    logger.log(LogLevel::Fatal, "Fatal msg", FATP_SOURCE_LOCATION());
    FATP_ASSERT_TRUE(testSink->count() == 3, "Higher or equal levels logged");

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records[0].message == "Warning msg", "Warning message correct");
    FATP_ASSERT_TRUE(records[1].message == "Error msg", "Error message correct");
    FATP_ASSERT_TRUE(records[2].message == "Fatal msg", "Fatal message correct");

    return true;
}

FATP_TEST_CASE(logger_multiple_sinks)
{
    Logger logger;
    auto sink1 = std::make_shared<TestSink>();
    auto sink2 = std::make_shared<TestSink>();
    auto sink3 = std::make_shared<TestSink>();

    logger.addSink(sink1);
    logger.addSink(sink2);
    logger.addSink(sink3);

    logger.log(LogLevel::Info, "Multi-sink test", FATP_SOURCE_LOCATION());

    FATP_ASSERT_TRUE(sink1->count() == 1, "First sink received message");
    FATP_ASSERT_TRUE(sink2->count() == 1, "Second sink received message");
    FATP_ASSERT_TRUE(sink3->count() == 1, "Third sink received message");

    return true;
}

FATP_TEST_CASE(logger_has_sinks)
{
    Logger logger;
    FATP_ASSERT_TRUE(!logger.hasSinks(), "No sinks initially");
    FATP_ASSERT_TRUE(logger.sinkCount() == 0, "Sink count is 0");

    auto sink1 = std::make_shared<TestSink>();
    logger.addSink(sink1);
    FATP_ASSERT_TRUE(logger.hasSinks(), "Has sinks after add");
    FATP_ASSERT_TRUE(logger.sinkCount() == 1, "Sink count is 1");

    auto sink2 = std::make_shared<TestSink>();
    logger.addSink(sink2);
    FATP_ASSERT_TRUE(logger.sinkCount() == 2, "Sink count is 2");

    logger.clearSinks();
    FATP_ASSERT_TRUE(!logger.hasSinks(), "No sinks after clear");
    FATP_ASSERT_TRUE(logger.sinkCount() == 0, "Sink count is 0 after clear");

    return true;
}

FATP_TEST_CASE(logger_clear_sinks)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    logger.log(LogLevel::Info, "Before clear", FATP_SOURCE_LOCATION());
    FATP_ASSERT_TRUE(testSink->count() == 1, "Sink receives message before clear");

    logger.clearSinks();
    testSink->clear();

    logger.log(LogLevel::Info, "After clear", FATP_SOURCE_LOCATION());
    FATP_ASSERT_TRUE(testSink->count() == 0, "Sink doesn't receive message after clear");

    return true;
}

FATP_TEST_CASE(logger_string_message)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    std::string msg = "String message";
    logger.log(LogLevel::Info, msg, FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "One record logged");
    FATP_ASSERT_TRUE(records[0].message == "String message", "String message logged correctly");

    return true;
}

FATP_TEST_CASE(logger_lambda_message)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    int value = 42;
    logger.log(
        LogLevel::Info,
        [&]() {
            return "Value is " + std::to_string(value);
        },
        FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "One record logged");
    FATP_ASSERT_TRUE(records[0].message == "Value is 42", "Lambda message evaluated correctly");

    return true;
}

FATP_TEST_CASE(logger_stream_message)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    int num = 123;
    logger.log(
        LogLevel::Info,
        [&]() {
            std::ostringstream oss;
            oss << "Number: " << num;
            return oss.str();
        },
        FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "One record logged");
    FATP_ASSERT_TRUE(records[0].message == "Number: 123", "Stream message correct");

    return true;
}

FATP_TEST_CASE(logger_metadata)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    logger.log(LogLevel::Info, "Test message", FATP_SOURCE_LOCATION(), "key=value");

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "One record logged");
    FATP_ASSERT_TRUE(records[0].metadata == "key=value", "Metadata set correctly");

    return true;
}

FATP_TEST_CASE(logger_convenience_methods)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    logger.trace("Trace", FATP_SOURCE_LOCATION());
    logger.debug("Debug", FATP_SOURCE_LOCATION());
    logger.info("Info", FATP_SOURCE_LOCATION());
    logger.warning("Warning", FATP_SOURCE_LOCATION());
    logger.error("Error", FATP_SOURCE_LOCATION());
    logger.fatal("Fatal", FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 6, "Six records logged");

    FATP_ASSERT_TRUE(records[0].level == LogLevel::Trace, "Trace level");
    FATP_ASSERT_TRUE(records[1].level == LogLevel::Debug, "Debug level");
    FATP_ASSERT_TRUE(records[2].level == LogLevel::Info, "Info level");
    FATP_ASSERT_TRUE(records[3].level == LogLevel::Warning, "Warning level");
    FATP_ASSERT_TRUE(records[4].level == LogLevel::Error, "Error level");
    FATP_ASSERT_TRUE(records[5].level == LogLevel::Fatal, "Fatal level");

    return true;
}

FATP_TEST_CASE(logger_thread_safety)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    const int numThreads = 4;
    const int messagesPerThread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&logger, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; ++i)
            {
                logger.log(LogLevel::Info,
                           "Thread " + std::to_string(t) + " msg " + std::to_string(i),
                           FATP_SOURCE_LOCATION());
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(testSink->count() == static_cast<size_t>(numThreads * messagesPerThread),
                     "All messages from all threads logged");

    return true;
}

FATP_TEST_CASE(logger_sink_copy_on_write)
{
    Logger logger;
    auto sink1 = std::make_shared<TestSink>();
    logger.addSink(sink1);

    logger.log(LogLevel::Info, "Message 1", FATP_SOURCE_LOCATION());

    auto sink2 = std::make_shared<TestSink>();
    logger.addSink(sink2);

    logger.log(LogLevel::Info, "Message 2", FATP_SOURCE_LOCATION());

    FATP_ASSERT_TRUE(sink1->count() == 2, "First sink received both messages");
    FATP_ASSERT_TRUE(sink2->count() == 1, "Second sink only received message after being added");

    return true;
}

FATP_TEST_CASE(logger_error_auto_flush)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    logger.log(LogLevel::Info, "Info message", FATP_SOURCE_LOCATION());
    logger.log(LogLevel::Error, "Error message", FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 2, "Both messages logged");

    return true;
}

FATP_TEST_CASE(log_macros)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto testSink = std::make_shared<TestSink>();
    getGlobalLogger().clearSinks();
    getGlobalLogger().addSink(testSink);
    getGlobalLogger().setLevel(LogLevel::Trace);

    FATP_LOG_TRACE("Trace macro");
    FATP_LOG_DEBUG("Debug macro");
    FATP_LOG_INFO("Info macro");
    FATP_LOG_WARNING("Warning macro");
    FATP_LOG_ERROR("Error macro");
    FATP_LOG_FATAL("Fatal macro");

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 6, "All macro messages logged");

    FATP_ASSERT_TRUE(records[0].message == "Trace macro", "Trace macro message");
    FATP_ASSERT_TRUE(records[1].message == "Debug macro", "Debug macro message");
    FATP_ASSERT_TRUE(records[2].message == "Info macro", "Info macro message");
    FATP_ASSERT_TRUE(records[3].message == "Warning macro", "Warning macro message");
    FATP_ASSERT_TRUE(records[4].message == "Error macro", "Error macro message");
    FATP_ASSERT_TRUE(records[5].message == "Fatal macro", "Fatal macro message");

    return true;
}

FATP_TEST_CASE(log_macro_with_stream)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto testSink = std::make_shared<TestSink>();
    getGlobalLogger().clearSinks();
    getGlobalLogger().addSink(testSink);
    getGlobalLogger().setLevel(LogLevel::Trace);

    int value = 42;
    std::string text = "test";

    FATP_LOG_INFO("Value: " << value << ", Text: " << text);

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "One log record");
    FATP_ASSERT_TRUE(records[0].message == "Value: 42, Text: test", "Stream operator works in macro");

    return true;
}

FATP_TEST_CASE(should_log_performance)
{
    Logger logger;
    logger.setLevel(LogLevel::Error);

    bool shouldLog = logger.shouldLog(LogLevel::Info);
    FATP_ASSERT_TRUE(!shouldLog, "Should not log Info when level is Error");

    shouldLog = logger.shouldLog(LogLevel::Error);
    FATP_ASSERT_TRUE(shouldLog, "Should log Error when level is Error");

    shouldLog = logger.shouldLog(LogLevel::Fatal);
    FATP_ASSERT_TRUE(shouldLog, "Should log Fatal when level is Error");

    return true;
}

FATP_TEST_CASE(empty_message)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    logger.log(LogLevel::Info, "", FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "Empty message logged");
    FATP_ASSERT_TRUE(records[0].message.empty(), "Message is empty");

    return true;
}

FATP_TEST_CASE(long_message)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    std::string longMsg(10000, 'X');
    logger.log(LogLevel::Info, longMsg, FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "Long message logged");
    FATP_ASSERT_TRUE(records[0].message.size() == 10000, "Long message size correct");

    return true;
}

FATP_TEST_CASE(special_characters)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    std::string specialChars = "Special: \n\t\r\\\"'";
    logger.log(LogLevel::Info, specialChars, FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "Special chars message logged");
    FATP_ASSERT_TRUE(records[0].message == specialChars, "Special chars preserved");

    return true;
}

FATP_TEST_CASE(unicode_characters)
{
    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    // Use raw UTF-8 byte sequences to avoid C4566 warnings on Windows
    // \u00E9 (e-acute) = 0xC3 0xA9, \u00F1 (n-tilde) = 0xC3 0xB1, \u00FC (u-umlaut) = 0xC3 0xBC
    // \u4E2D\u6587 ("Chinese: Zhong Wen") = 0xE4 0xB8 0xAD 0xE6 0x96 0x87
    // \U0001F600 ("grinning face") = 0xF0 0x9F 0x98 0x80
    std::string unicode = "Unicode: \xC3\xA9\xC3\xB1\xC3\xBC \xE4\xB8\xAD\xE6\x96\x87 \xF0\x9F\x98\x80";
    logger.log(LogLevel::Info, unicode, FATP_SOURCE_LOCATION());

    auto records = testSink->getRecords();
    FATP_ASSERT_TRUE(records.size() == 1, "Unicode message logged");
    FATP_ASSERT_TRUE(records[0].message == unicode, "Unicode preserved");

    return true;
}

// ============================================================================
// Named Loggers Tests
// ============================================================================

FATP_TEST_CASE(logger_registry_singleton)
{
    LoggerRegistry& reg1 = LoggerRegistry::instance();
    LoggerRegistry& reg2 = LoggerRegistry::instance();

    FATP_ASSERT_TRUE(&reg1 == &reg2, "Registry is a singleton");

    return true;
}

FATP_TEST_CASE(logger_registry_get_creates)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 0, "Empty after dropAll");

    Logger& log = LoggerRegistry::instance().get("test1");
    (void)log;

    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 1, "One logger created");
    FATP_ASSERT_TRUE(LoggerRegistry::instance().exists("test1"), "test1 exists");

    return true;
}

FATP_TEST_CASE(logger_registry_get_returns_same)
{
    LoggerRegistry::instance().dropAll();

    Logger& log1 = LoggerRegistry::instance().get("same");
    Logger& log2 = LoggerRegistry::instance().get("same");

    FATP_ASSERT_TRUE(&log1 == &log2, "Same logger returned");

    return true;
}

FATP_TEST_CASE(logger_registry_multiple_loggers)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    getLogger("network");
    getLogger("database");
    getLogger("ui");

    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 3, "Three loggers created");
    FATP_ASSERT_TRUE(LoggerRegistry::instance().exists("network"), "network exists");
    FATP_ASSERT_TRUE(LoggerRegistry::instance().exists("database"), "database exists");
    FATP_ASSERT_TRUE(LoggerRegistry::instance().exists("ui"), "ui exists");
    FATP_ASSERT_TRUE(!LoggerRegistry::instance().exists("audio"), "audio does not exist");

    return true;
}

FATP_TEST_CASE(logger_registry_names)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    getLogger("alpha");
    getLogger("beta");
    getLogger("gamma");

    auto names = LoggerRegistry::instance().names();
    FATP_ASSERT_TRUE(names.size() == 3, "Three names returned");

    bool hasAlpha = false, hasBeta = false, hasGamma = false;
    for (const auto& name : names)
    {
        if (name == "alpha")
        {
            hasAlpha = true;
        }
        if (name == "beta")
        {
            hasBeta = true;
        }
        if (name == "gamma")
        {
            hasGamma = true;
        }
    }

    FATP_ASSERT_TRUE(hasAlpha && hasBeta && hasGamma, "All names present");

    return true;
}

FATP_TEST_CASE(logger_registry_drop)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    getLogger("keep");
    getLogger("remove");

    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 2, "Two loggers");

    bool dropped = LoggerRegistry::instance().drop("remove");
    FATP_ASSERT_TRUE(dropped, "drop returned true");
    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 1, "One logger after drop");
    FATP_ASSERT_TRUE(LoggerRegistry::instance().exists("keep"), "keep still exists");
    FATP_ASSERT_TRUE(!LoggerRegistry::instance().exists("remove"), "remove does not exist");

    dropped = LoggerRegistry::instance().drop("nonexistent");
    FATP_ASSERT_TRUE(!dropped, "drop returned false for nonexistent");

    return true;
}

FATP_TEST_CASE(logger_registry_drop_all)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    getLogger("one");
    getLogger("two");
    getLogger("three");

    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 3, "Three loggers");

    LoggerRegistry::instance().dropAll();
    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 0, "Zero loggers after dropAll");

    return true;
}

FATP_TEST_CASE(logger_registry_default_level)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Warning);

    Logger& log = getLogger("with_default_level");

    FATP_ASSERT_TRUE(log.getLevel() == LogLevel::Warning, "Inherits default level");

    return true;
}

FATP_TEST_CASE(logger_registry_default_sinks)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().clearDefaultSinks();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto defaultSink = std::make_shared<TestSink>();
    LoggerRegistry::instance().addDefaultSink(defaultSink);

    Logger& log = getLogger("with_default_sink");

    log.info("Test message", FATP_SOURCE_LOCATION());

    FATP_ASSERT_TRUE(defaultSink->count() == 1, "Default sink received message");
    FATP_ASSERT_TRUE(defaultSink->containsMessage("Test message"), "Correct message");

    LoggerRegistry::instance().clearDefaultSinks();

    return true;
}

FATP_TEST_CASE(logger_registry_set_all_levels)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    getLogger("a").setLevel(LogLevel::Debug);
    getLogger("b").setLevel(LogLevel::Info);
    getLogger("c").setLevel(LogLevel::Warning);

    LoggerRegistry::instance().setAllLevels(LogLevel::Error);

    FATP_ASSERT_TRUE(getLogger("a").getLevel() == LogLevel::Error, "a level set");
    FATP_ASSERT_TRUE(getLogger("b").getLevel() == LogLevel::Error, "b level set");
    FATP_ASSERT_TRUE(getLogger("c").getLevel() == LogLevel::Error, "c level set");

    return true;
}

FATP_TEST_CASE(logger_registry_add_sink_to_all)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().clearDefaultSinks();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto sink1 = std::make_shared<TestSink>();
    auto sink2 = std::make_shared<TestSink>();

    getLogger("x").addSink(sink1);
    getLogger("y").addSink(sink1);

    LoggerRegistry::instance().addSinkToAll(sink2);

    getLogger("x").info("Message from x", FATP_SOURCE_LOCATION());
    getLogger("y").info("Message from y", FATP_SOURCE_LOCATION());

    FATP_ASSERT_TRUE(sink1->count() == 2, "sink1 received both messages");
    FATP_ASSERT_TRUE(sink2->count() == 2, "sink2 received both messages");

    return true;
}

FATP_TEST_CASE(logger_registry_get_shared)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto ptr1 = LoggerRegistry::instance().getShared("shared_test");
    auto ptr2 = LoggerRegistry::instance().getShared("shared_test");

    FATP_ASSERT_TRUE(ptr1 != nullptr, "First pointer not null");
    FATP_ASSERT_TRUE(ptr2 != nullptr, "Second pointer not null");
    FATP_ASSERT_TRUE(ptr1.get() == ptr2.get(), "Same logger instance");

    return true;
}

FATP_TEST_CASE(get_logger_convenience)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    Logger& log = getLogger("convenient");
    auto testSink = std::make_shared<TestSink>();
    log.addSink(testSink);

    log.info("Convenience test", FATP_SOURCE_LOCATION());

    FATP_ASSERT_TRUE(testSink->count() == 1, "Message logged via getLogger");

    return true;
}

FATP_TEST_CASE(get_global_logger_is_empty_name)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    Logger& global1 = getGlobalLogger();
    Logger& global2 = getLogger("");

    FATP_ASSERT_TRUE(&global1 == &global2, "getGlobalLogger() == getLogger(\"\")");

    return true;
}

FATP_TEST_CASE(named_logger_independent_levels)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto networkSink = std::make_shared<TestSink>();
    auto dbSink = std::make_shared<TestSink>();

    Logger& networkLog = getLogger("network");
    Logger& dbLog = getLogger("database");

    networkLog.addSink(networkSink);
    networkLog.setLevel(LogLevel::Debug);

    dbLog.addSink(dbSink);
    dbLog.setLevel(LogLevel::Error);

    networkLog.debug("Network debug", FATP_SOURCE_LOCATION());
    networkLog.info("Network info", FATP_SOURCE_LOCATION());

    dbLog.debug("DB debug", FATP_SOURCE_LOCATION());
    dbLog.info("DB info", FATP_SOURCE_LOCATION());
    dbLog.error("DB error", FATP_SOURCE_LOCATION());

    FATP_ASSERT_TRUE(networkSink->count() == 2, "Network logged debug and info");
    FATP_ASSERT_TRUE(dbSink->count() == 1, "DB only logged error");

    return true;
}

// ============================================================================
// Named Logger Macros Tests
// ============================================================================

FATP_TEST_CASE(log_to_macros)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto sink = std::make_shared<TestSink>();
    getLogger("macro_test").addSink(sink);

    FATP_LOG_TRACE_TO("macro_test", "Trace to named");
    FATP_LOG_DEBUG_TO("macro_test", "Debug to named");
    FATP_LOG_INFO_TO("macro_test", "Info to named");
    FATP_LOG_WARNING_TO("macro_test", "Warning to named");
    FATP_LOG_ERROR_TO("macro_test", "Error to named");
    FATP_LOG_FATAL_TO("macro_test", "Fatal to named");

    FATP_ASSERT_TRUE(sink->count() == 6, "All six messages logged");
    FATP_ASSERT_TRUE(sink->countLevel(LogLevel::Trace) == 1, "One trace");
    FATP_ASSERT_TRUE(sink->countLevel(LogLevel::Debug) == 1, "One debug");
    FATP_ASSERT_TRUE(sink->countLevel(LogLevel::Info) == 1, "One info");
    FATP_ASSERT_TRUE(sink->countLevel(LogLevel::Warning) == 1, "One warning");
    FATP_ASSERT_TRUE(sink->countLevel(LogLevel::Error) == 1, "One error");
    FATP_ASSERT_TRUE(sink->countLevel(LogLevel::Fatal) == 1, "One fatal");

    return true;
}

FATP_TEST_CASE(log_to_macro_with_stream)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto sink = std::make_shared<TestSink>();
    getLogger("stream_test").addSink(sink);

    int value = 99;
    std::string name = "Alice";

    FATP_LOG_INFO_TO("stream_test", "User " << name << " has value " << value);

    FATP_ASSERT_TRUE(sink->count() == 1, "One message logged");
    FATP_ASSERT_TRUE(sink->containsMessage("User Alice has value 99"), "Stream formatting correct");

    return true;
}

FATP_TEST_CASE(log_to_macro_static_caching)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto sink = std::make_shared<TestSink>();
    getLogger("cached").addSink(sink);

    for (int i = 0; i < 100; ++i)
    {
        FATP_LOG_INFO_TO("cached", "Iteration " << i);
    }

    FATP_ASSERT_TRUE(sink->count() == 100, "All 100 messages logged");

    return true;
}

// ============================================================================
// Lazy Initialization Tests
// ============================================================================

FATP_TEST_CASE(lazy_init_auto_creates_sink)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    std::ostringstream capturedOutput;
    std::streambuf* oldCoutBuf = std::cout.rdbuf();
    std::cout.rdbuf(capturedOutput.rdbuf());

    FATP_LOG_INFO("Auto-init test message");

    std::cout.rdbuf(oldCoutBuf);

    std::string output = capturedOutput.str();
    FATP_ASSERT_TRUE(output.find("Auto-init test message") != std::string::npos,
                     "Message appeared in console (auto-initialized)");

    FATP_ASSERT_TRUE(getGlobalLogger().hasSinks(), "Global logger has sinks after auto-init");

    return true;
}

FATP_TEST_CASE(lazy_init_disabled_by_add_sink)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto customSink = std::make_shared<TestSink>();
    getGlobalLogger().clearSinks();
    getGlobalLogger().addSink(customSink);

    FATP_LOG_INFO("Custom sink message");

    FATP_ASSERT_TRUE(customSink->count() == 1, "Custom sink received message");
    FATP_ASSERT_TRUE(getGlobalLogger().sinkCount() == 1, "Only one sink (no auto-init)");

    return true;
}

FATP_TEST_CASE(lazy_init_disabled_by_clear_sinks)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    getGlobalLogger().clearSinks();

    auto testSink = std::make_shared<TestSink>();

    std::ostringstream capturedOutput;
    std::streambuf* oldCoutBuf = std::cout.rdbuf();
    std::cout.rdbuf(capturedOutput.rdbuf());

    FATP_LOG_INFO("Should not appear");

    std::cout.rdbuf(oldCoutBuf);

    FATP_ASSERT_TRUE(!getGlobalLogger().hasSinks(), "No sinks after clearSinks");

    return true;
}

FATP_TEST_CASE(lazy_init_disable_auto_init)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    Logger& log = getLogger("no_auto");
    log.disableAutoInit();

    std::ostringstream capturedOutput;
    std::streambuf* oldCoutBuf = std::cout.rdbuf();
    std::cout.rdbuf(capturedOutput.rdbuf());

    log.info("Should not appear", FATP_SOURCE_LOCATION());

    std::cout.rdbuf(oldCoutBuf);

    FATP_ASSERT_TRUE(!log.hasSinks(), "No sinks after disableAutoInit");

    return true;
}

// ============================================================================
// Initialization Function Tests
// ============================================================================

FATP_TEST_CASE(initialize_default_logger)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    getGlobalLogger().clearSinks();
    FATP_ASSERT_TRUE(!getGlobalLogger().hasSinks(), "No sinks before init");

    initializeDefaultLogger();
    FATP_ASSERT_TRUE(getGlobalLogger().hasSinks(), "Has sinks after initializeDefaultLogger");

    return true;
}

FATP_TEST_CASE(initialize_logger)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    initializeLogger("custom_init");

    FATP_ASSERT_TRUE(getLogger("custom_init").hasSinks(), "Named logger has sink");

    return true;
}

FATP_TEST_CASE(initialize_default_sinks)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().clearDefaultSinks();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    initializeDefaultSinks();

    Logger& newLogger = getLogger("after_default_sinks");
    FATP_ASSERT_TRUE(newLogger.hasSinks(), "New logger inherits default sink");

    LoggerRegistry::instance().clearDefaultSinks();

    return true;
}

// ============================================================================
// Thread Safety Tests for Registry
// ============================================================================

FATP_TEST_CASE(registry_thread_safety)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    const int numThreads = 8;
    const int loggersPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([t, loggersPerThread, &successCount]() {
            for (int i = 0; i < loggersPerThread; ++i)
            {
                std::string name = "thread" + std::to_string(t) + "_logger" + std::to_string(i);
                Logger& log = getLogger(name);
                if (log.getLevel() == LogLevel::Trace)
                {
                    successCount++;
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(successCount == numThreads * loggersPerThread, "All loggers created successfully");

    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == static_cast<size_t>(numThreads * loggersPerThread),
                     "Correct total count");

    return true;
}

FATP_TEST_CASE(registry_concurrent_get_same_logger)
{
    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    const int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<Logger*> loggerPtrs(numThreads, nullptr);

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([t, &loggerPtrs]() {
            loggerPtrs[t] = &getLogger("shared");
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    for (int i = 1; i < numThreads; ++i)
    {
        FATP_ASSERT_TRUE(loggerPtrs[i] == loggerPtrs[0], "All threads got same logger instance");
    }

    FATP_ASSERT_TRUE(LoggerRegistry::instance().count() == 1, "Only one logger created");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_logger()
{
    std::cout << "\n" << colors::cyan() << "DiagnosticLogger Core Benchmarks:" << colors::reset() << "\n\n";

    Logger logger;
    auto testSink = std::make_shared<TestSink>();
    logger.addSink(testSink);

    double enabled_time = measure_perf(
        [&logger]() {
            logger.log(LogLevel::Info, "Benchmark message", FATP_SOURCE_LOCATION());
        },
        10000,
        100);
    std::cout << "Enabled logging: " << format_time(enabled_time) << "\n";

    logger.setEnabled(false);
    double disabled_time = measure_perf(
        [&logger]() {
            logger.log(LogLevel::Info, "Disabled message", FATP_SOURCE_LOCATION());
        },
        100000,
        1000);
    std::cout << "Disabled logging: " << format_time(disabled_time) << "\n";

    logger.setEnabled(true);
    logger.setLevel(LogLevel::Error);
    double filtered_time = measure_perf(
        [&logger]() {
            logger.log(LogLevel::Info, "Filtered message", FATP_SOURCE_LOCATION());
        },
        100000,
        1000);
    std::cout << "Filtered logging: " << format_time(filtered_time) << "\n";

    logger.setLevel(LogLevel::Trace);
    double with_lambda_time = measure_perf(
        [&logger]() {
            logger.log(
                LogLevel::Info,
                []() {
                    return "Lambda message";
                },
                FATP_SOURCE_LOCATION());
        },
        10000,
        100);
    std::cout << "With lambda: " << format_time(with_lambda_time) << "\n";
}

void benchmark_named_loggers()
{
    std::cout << "\n" << colors::cyan() << "Named Loggers Benchmarks:" << colors::reset() << "\n\n";

    LoggerRegistry::instance().dropAll();
    LoggerRegistry::instance().setDefaultLevel(LogLevel::Trace);

    auto sink = std::make_shared<TestSink>();
    getLogger("bench").addSink(sink);

    double lookup_time = measure_perf(
        []() {
            getLogger("bench");
        },
        100000,
        1000);
    std::cout << "Logger lookup (existing): " << format_time(lookup_time) << "\n";

    double log_to_time = measure_perf(
        []() {
            FATP_LOG_INFO_TO("bench", "Benchmark");
        },
        10000,
        100);
    std::cout << "LOG_INFO_TO (cached): " << format_time(log_to_time) << "\n";

    LoggerRegistry::instance().dropAll();
    double create_time = measure_perf(
        []() {
            static int counter = 0;
            getLogger("new_" + std::to_string(counter++));
        },
        1000,
        10);
    std::cout << "Logger creation: " << format_time(create_time) << "\n";
}

} // anonymous namespace

} // namespace fat_p::testing::diagnosticlogger_core

namespace fat_p::testing
{

bool test_DiagnosticLogger_Core()
{
    FATP_PRINT_HEADER(DIAGNOSTIC LOGGER CORE)

    TestRunner runner;

    // Basic tests
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, log_level_enum);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, source_location);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, log_record_construction);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, default_formatter);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, console_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, stderr_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_enable_disable);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_level_filtering);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_multiple_sinks);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_has_sinks);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_clear_sinks);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_string_message);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_lambda_message);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_stream_message);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_metadata);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_convenience_methods);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_thread_safety);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_sink_copy_on_write);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_error_auto_flush);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, log_macros);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, log_macro_with_stream);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, should_log_performance);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, empty_message);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, long_message);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, special_characters);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, unicode_characters);

    // Named loggers tests
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_singleton);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_get_creates);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_get_returns_same);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_multiple_loggers);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_names);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_drop);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_drop_all);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_default_level);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_default_sinks);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_set_all_levels);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_add_sink_to_all);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, logger_registry_get_shared);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, get_logger_convenience);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, get_global_logger_is_empty_name);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, named_logger_independent_levels);

    // Named logger macros tests
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, log_to_macros);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, log_to_macro_with_stream);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, log_to_macro_static_caching);

    // Lazy initialization tests
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, lazy_init_auto_creates_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, lazy_init_disabled_by_add_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, lazy_init_disabled_by_clear_sinks);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, lazy_init_disable_auto_init);

    // Initialization function tests
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, initialize_default_logger);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, initialize_logger);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, initialize_default_sinks);

    // Thread safety tests
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, registry_thread_safety);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_core, registry_concurrent_get_same_logger);

    diagnosticlogger_core::benchmark_logger();
    diagnosticlogger_core::benchmark_named_loggers();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_DiagnosticLogger_Core() ? 0 : 1;
}
#endif
