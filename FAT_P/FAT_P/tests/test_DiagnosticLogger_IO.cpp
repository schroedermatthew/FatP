/**
 * @file test_DiagnosticLogger_IO.cpp
 * @brief Comprehensive unit tests for DiagnosticLogger_IO.h
 */
/*
FATP_META:
  meta_version: 1
  component: DiagnosticLogger_IO
  file_role: test
  path: tests/test_DiagnosticLogger_IO.cpp
  namespace: fat_p::testing::diagnosticlogger_io
  summary: "Unit tests for DiagnosticLogger_IO."
  related:
    docs_search: "DiagnosticLogger_IO"
    headers:
      - fat_p/DiagnosticLogger_IO.h
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

#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

#include "DiagnosticLogger_IO.h"
#include "FatPTest.h"

namespace fat_p::testing::diagnosticlogger_io
{

using namespace fat_p::diagnostic;
namespace fs = std::filesystem;

namespace
{

std::string readFileContents(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

size_t countLines(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        return 0;
    }
    return std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
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

FATP_TEST_CASE(file_sink_basic)
{
    std::string filename = "test_file_sink.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto sink = makeFileSink(filename);
        FATP_ASSERT_TRUE(sink != nullptr, "FileSink created successfully");
        FATP_ASSERT_TRUE(sink->is_valid(), "FileSink is valid");
        
        auto loc = FATP_SOURCE_LOCATION();
        LogRecord record(LogLevel::Info, "Test file sink message", loc);
        sink->write(record);
        sink->flush();
        
        std::string contents = readFileContents(filename);
        FATP_ASSERT_TRUE(!contents.empty(), "File has content");
        FATP_ASSERT_TRUE(contents.find("Test file sink message") != std::string::npos, "Message in file");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(file_sink_multiple_writes)
{
    std::string filename = "test_file_sink_multi.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto sink = makeFileSink(filename);
        auto loc = FATP_SOURCE_LOCATION();
        
        for (int i = 0; i < 10; ++i)
        {
            LogRecord record(LogLevel::Info, "Message " + std::to_string(i), loc);
            sink->write(record);
        }
        sink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 10, "10 lines written");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(file_sink_append_mode)
{
    std::string filename = "test_file_sink_append.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    auto loc = FATP_SOURCE_LOCATION();
    
    {
        auto sink1 = makeFileSink(filename);
        LogRecord record(LogLevel::Info, "First", loc);
        sink1->write(record);
    }
    
    {
        auto sink2 = makeFileSink(filename);
        LogRecord record(LogLevel::Info, "Second", loc);
        sink2->write(record);
    }
    
    size_t lines = countLines(filename);
    FATP_ASSERT_TRUE(lines == 2, "Both messages appended");
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(file_sink_invalid_path)
{
    std::string invalid_path = "/invalid/path/that/does/not/exist/test.log";
    auto sink = makeFileSink(invalid_path);
    
    FATP_ASSERT_TRUE(sink == nullptr, "Invalid path returns nullptr");
    
    return true;
}

FATP_TEST_CASE(ring_buffer_sink)
{
    RingBufferSink rbSink;
    auto loc = FATP_SOURCE_LOCATION();
    
    for (int i = 0; i < 10; ++i)
    {
        LogRecord record(LogLevel::Info, "Ring buffer " + std::to_string(i), loc);
        rbSink.write(record);
    }
    
    std::string filename = "test_ring_buffer_dump.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        rbSink.dumpTo(*fileSink);
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 10, "All ring buffer messages dumped");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(ring_buffer_overflow)
{
    RingBufferSink rbSink;
    auto loc = FATP_SOURCE_LOCATION();
    
    for (int i = 0; i < 2000; ++i)
    {
        LogRecord record(LogLevel::Info, "Overflow " + std::to_string(i), loc);
        rbSink.write(record);
    }
    
    std::string filename = "test_ring_buffer_overflow.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        rbSink.dumpTo(*fileSink);
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 1024, "Ring buffer capacity is 1024");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(rotating_file_sink_basic)
{
    std::string filename = "test_rotate.log";
    cleanupTestFiles(filename);
    
    {
        auto sink = makeRotatingFileSink(filename, 1024, 3);
        FATP_ASSERT_TRUE(sink != nullptr, "RotatingFileSink created");
        FATP_ASSERT_TRUE(sink->is_valid(), "RotatingFileSink is valid");
        
        auto loc = FATP_SOURCE_LOCATION();
        for (int i = 0; i < 100; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(50, 'X') + std::to_string(i), loc);
            sink->write(record);
        }
        sink->flush();
        
        FATP_ASSERT_TRUE(fs::exists(filename), "Base file exists");
    }
    
    cleanupTestFiles(filename);
    return true;
}

FATP_TEST_CASE(rotating_file_sink_rotation)
{
    std::string filename = "test_rotate_check.log";
    cleanupTestFiles(filename);
    
    {
        auto sink = makeRotatingFileSink(filename, 500, 3);
        auto loc = FATP_SOURCE_LOCATION();
        
        for (int i = 0; i < 50; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(100, 'A'), loc);
            sink->write(record);
            sink->flush();
        }
        
        bool rotated = false;
        for (int i = 1; i <= 3; ++i)
        {
            if (fs::exists(filename + "." + std::to_string(i)))
            {
                rotated = true;
                break;
            }
        }
        
        FATP_ASSERT_TRUE(rotated, "Files were rotated");
    }
    
    cleanupTestFiles(filename);
    return true;
}

FATP_TEST_CASE(resilient_sink_primary_works)
{
    std::string filename = "test_resilient_primary.log";
    std::string fallbackFile = "test_resilient_fallback.log";
    
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    if (fs::exists(fallbackFile))
    {
        std::error_code ec;
        fs::remove(fallbackFile, ec);
    }
    
    {
        auto primary = makeFileSink(filename);
        auto fallback = makeFileSink(fallbackFile);
        auto resilient = std::make_shared<ResilientSink>(primary, fallback);
        
        auto loc = FATP_SOURCE_LOCATION();
        LogRecord record(LogLevel::Info, "Primary test", loc);
        resilient->write(record);
        resilient->flush();
        
        std::string contents = readFileContents(filename);
        FATP_ASSERT_TRUE(contents.find("Primary test") != std::string::npos, "Primary sink used");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    fs::remove(fallbackFile, ec);
    return true;
}

FATP_TEST_CASE(async_sink)
{
    std::string filename = "test_async.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto asyncSink = std::make_shared<AsyncSink>(fileSink);
        
        auto loc = FATP_SOURCE_LOCATION();
        for (int i = 0; i < 100; ++i)
        {
            LogRecord record(LogLevel::Info, "Async message " + std::to_string(i), loc);
            asyncSink->write(record);
        }
        
        asyncSink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 100, "All async messages written");
        FATP_ASSERT_TRUE(asyncSink->dropped() == 0, "No messages dropped");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(async_sink_high_load)
{
    std::string filename = "test_async_load.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto asyncSink = std::make_shared<AsyncSink>(fileSink);
        
        auto loc = FATP_SOURCE_LOCATION();
        std::atomic<int> counter{0};
        
        std::vector<std::thread> threads;
        for (int t = 0; t < 5; ++t)
        {
            threads.emplace_back([&asyncSink, &counter, loc]() {
                for (int i = 0; i < 100; ++i)
                {
                    LogRecord record(LogLevel::Info, "Thread message " + std::to_string(counter.fetch_add(1)), loc);
                    asyncSink->write(record);
                }
            });
        }
        
        for (auto& t : threads)
        {
            t.join();
        }
        
        asyncSink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines >= 400, "Most messages written under load");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(rate_limiting_sink)
{
    std::string filename = "test_rate_limit.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto rateLimitSink = std::make_shared<RateLimitingSink>(fileSink, 10.0);
        
        auto loc = FATP_SOURCE_LOCATION();
        for (int i = 0; i < 100; ++i)
        {
            LogRecord record(LogLevel::Info, "Rate limited " + std::to_string(i), loc);
            rateLimitSink->write(record);
        }
        rateLimitSink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines < 100, "Some messages were rate limited");
        FATP_ASSERT_TRUE(rateLimitSink->dropped() > 0, "Some messages dropped");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(rate_limiting_burst)
{
    std::string filename = "test_rate_burst.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto rateLimitSink = std::make_shared<RateLimitingSink>(fileSink, 10.0, 20.0);
        
        auto loc = FATP_SOURCE_LOCATION();
        for (int i = 0; i < 20; ++i)
        {
            LogRecord record(LogLevel::Info, "Burst " + std::to_string(i), loc);
            rateLimitSink->write(record);
        }
        rateLimitSink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines >= 10, "Burst allows initial messages");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(filtering_sink)
{
    std::string filename = "test_filter.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto filterSink = std::make_shared<FilteringSink>(fileSink, [](const LogRecord& rec) { return rec.level >= LogLevel::Warning; });
        
        auto loc = FATP_SOURCE_LOCATION();
        
        LogRecord trace(LogLevel::Trace, "Trace", loc);
        LogRecord debug(LogLevel::Debug, "Debug", loc);
        LogRecord info(LogLevel::Info, "Info", loc);
        LogRecord warning(LogLevel::Warning, "Warning", loc);
        LogRecord error(LogLevel::Error, "Error", loc);
        
        filterSink->write(trace);
        filterSink->write(debug);
        filterSink->write(info);
        filterSink->write(warning);
        filterSink->write(error);
        filterSink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 2, "Only Warning and Error logged");
        
        std::string contents = readFileContents(filename);
        FATP_ASSERT_TRUE(contents.find("Warning") != std::string::npos, "Warning present");
        FATP_ASSERT_TRUE(contents.find("Error") != std::string::npos, "Error present");
        FATP_ASSERT_TRUE(contents.find("Trace") == std::string::npos, "Trace filtered");
        FATP_ASSERT_TRUE(contents.find("Debug") == std::string::npos, "Debug filtered");
        FATP_ASSERT_TRUE(contents.find("Info") == std::string::npos, "Info filtered");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(filtering_sink_custom_filter)
{
    std::string filename = "test_filter_custom.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto filterSink = std::make_shared<FilteringSink>(fileSink, [](const LogRecord& rec) { return rec.message.find("important") != std::string::npos; });
        
        auto loc = FATP_SOURCE_LOCATION();
        
        LogRecord r1(LogLevel::Info, "Normal message", loc);
        LogRecord r2(LogLevel::Info, "This is important", loc);
        LogRecord r3(LogLevel::Info, "Another normal one", loc);
        LogRecord r4(LogLevel::Info, "Also important", loc);
        
        filterSink->write(r1);
        filterSink->write(r2);
        filterSink->write(r3);
        filterSink->write(r4);
        filterSink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 2, "Only important messages logged");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

FATP_TEST_CASE(initialize_rotating_logger)
{
    std::string filename = "test_init_rotate.log";
    cleanupTestFiles(filename);
    
    getGlobalLogger().clearSinks();
    initializeRotatingLogger(filename);
    
    FATP_LOG_INFO("Test rotating logger initialization");
    
    FATP_ASSERT_TRUE(fs::exists(filename), "Rotating log file created");
    
    getGlobalLogger().clearSinks();
    cleanupTestFiles(filename);
    
    return true;
}

FATP_TEST_CASE(combined_sinks)
{
    std::string file1 = "test_combined1.log";
    std::string file2 = "test_combined2.log";
    
    if (fs::exists(file1))
    {
        std::error_code ec;
        fs::remove(file1, ec);
    }
    if (fs::exists(file2))
    {
        std::error_code ec;
        fs::remove(file2, ec);
    }
    
    {
        getGlobalLogger().clearSinks();
        
        auto sink1 = makeFileSink(file1);
        auto sink2 = makeFileSink(file2);
        
        getGlobalLogger().addSink(sink1);
        getGlobalLogger().addSink(sink2);
        
        FATP_LOG_INFO("Combined sink test");
        
        sink1->flush();
        sink2->flush();
        
        FATP_ASSERT_TRUE(fs::exists(file1), "First file created");
        FATP_ASSERT_TRUE(fs::exists(file2), "Second file created");
        
        std::string contents1 = readFileContents(file1);
        std::string contents2 = readFileContents(file2);
        
        FATP_ASSERT_TRUE(contents1.find("Combined sink test") != std::string::npos, "Message in file1");
        FATP_ASSERT_TRUE(contents2.find("Combined sink test") != std::string::npos, "Message in file2");
        
        getGlobalLogger().clearSinks();
    }
    
    std::error_code ec;
    fs::remove(file1, ec);
    fs::remove(file2, ec);
    
    return true;
}

FATP_TEST_CASE(async_sink_flush_consistency)
{
    std::string filename = "test_async_flush.log";
    cleanupTestFiles(filename);

    {
        auto fileSink = makeFileSink(filename);
        auto asyncSink = std::make_shared<AsyncSink>(fileSink);
        auto loc = FATP_SOURCE_LOCATION();

        for (int i = 0; i < 50; ++i)
        {
            LogRecord record(LogLevel::Info, "Consistency Check", loc);
            asyncSink->write(record);
        }

        asyncSink->flush();

        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 50, "Flush ensured exactly 50 lines were written");
    }

    cleanupTestFiles(filename);
    return true;
}

FATP_TEST_CASE(async_with_filtering)
{
    std::string filename = "test_async_filter.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto filterSink = std::make_shared<FilteringSink>(fileSink, [](const LogRecord& rec) { return rec.level >= LogLevel::Error; });
        auto asyncSink = std::make_shared<AsyncSink>(filterSink);
        
        auto loc = FATP_SOURCE_LOCATION();
        
        for (int i = 0; i < 20; ++i)
        {
            LogLevel level = (i % 2 == 0) ? LogLevel::Info : LogLevel::Error;
            LogRecord record(level, "Message " + std::to_string(i), loc);
            asyncSink->write(record);
        }
        
        asyncSink->flush();
        
        size_t lines = countLines(filename);
        FATP_ASSERT_TRUE(lines == 10, "Only errors logged through combined async+filter");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

void benchmark_io_sinks()
{
    std::cout << "\n" << colors::cyan() << "DiagnosticLogger IO Benchmarks:" << colors::reset() << "\n\n";
    
    std::string filename = "benchmark.log";
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
    
    {
        auto fileSink = makeFileSink(filename);
        auto loc = FATP_SOURCE_LOCATION();
        
        double file_time = measure_perf([&fileSink, loc]() {
            LogRecord record(LogLevel::Info, "Benchmark message", loc);
            fileSink->write(record);
        }, 1000, 10);
        std::cout << "File sink write: " << format_time(file_time) << "\n";
        
        RingBufferSink rbSink;
        double ring_time = measure_perf([&rbSink, loc]() {
            LogRecord record(LogLevel::Info, "Ring buffer benchmark", loc);
            rbSink.write(record);
        }, 10000, 100);
        std::cout << "Ring buffer write: " << format_time(ring_time) << "\n";
        
        auto asyncSink = std::make_shared<AsyncSink>(fileSink);
        double async_time = measure_perf([&asyncSink, loc]() {
            LogRecord record(LogLevel::Info, "Async benchmark", loc);
            asyncSink->write(record);
        }, 10000, 100);
        std::cout << "Async sink write: " << format_time(async_time) << "\n";
        
        asyncSink->flush();
    }
    
    if (fs::exists(filename))
    {
        std::error_code ec;
        fs::remove(filename, ec);
    }
}

FATP_TEST_CASE(rotating_file_sink_scope_guard_safety)
{
    std::string filename = "test_rotate_guard.log";
    cleanupTestFiles(filename);
    
    {
        auto sink = makeRotatingFileSink(filename, 200, 3);
        FATP_ASSERT_TRUE(sink != nullptr, "RotatingFileSink created");
        FATP_ASSERT_TRUE(sink->is_valid(), "RotatingFileSink is valid initially");
        
        auto loc = FATP_SOURCE_LOCATION();
        
        for (int i = 0; i < 10; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(50, 'X'), loc);
            sink->write(record);
        }
        
        FATP_ASSERT_TRUE(sink->is_valid(), "Sink still valid after rotation");
        
        LogRecord record(LogLevel::Info, "Post-rotation test", loc);
        sink->write(record);
        sink->flush();
        
        std::string contents = readFileContents(filename);
        FATP_ASSERT_TRUE(contents.find("Post-rotation test") != std::string::npos, "Post-rotation write succeeded");
    }
    
    cleanupTestFiles(filename);
    return true;
}

class LogLevelTestGuard
{
    Logger& logger_;
    LogLevel originalLevel_;
    
public:
    LogLevelTestGuard(Logger& logger, LogLevel tempLevel)
        : logger_(logger)
        , originalLevel_(logger.getLevel())
    {
        logger_.setLevel(tempLevel);
    }
    
    ~LogLevelTestGuard()
    {
        logger_.setLevel(originalLevel_);
    }
    
    LogLevelTestGuard(const LogLevelTestGuard&) = delete;
    LogLevelTestGuard& operator=(const LogLevelTestGuard&) = delete;
};

FATP_TEST_CASE(log_level_guard_restoration)
{
    auto& logger = getGlobalLogger();
    LogLevel original = logger.getLevel();
    
    {
        LogLevelTestGuard guard(logger, LogLevel::Trace);
        FATP_ASSERT_TRUE(logger.getLevel() == LogLevel::Trace, "Level changed to Trace");
    }
    
    FATP_ASSERT_TRUE(logger.getLevel() == original, "Level restored after guard destruction");
    
    {
        LogLevelTestGuard guard(logger, LogLevel::Fatal);
        FATP_ASSERT_TRUE(logger.getLevel() == LogLevel::Fatal, "Level changed to Fatal");
    }
    
    FATP_ASSERT_TRUE(logger.getLevel() == original, "Level restored after second guard");
    
    return true;
}

FATP_TEST_CASE(resilient_sink_scope_guard_state_management)
{
    class FailingSink : public ISink
    {
    public:
        mutable int writeCount = 0;
        
        void write(const LogRecord&) override
        {
            ++writeCount;
            throw std::runtime_error("Intentional failure");
        }
        
        void flush() override {}
    };
    
    std::string fallbackFile = "test_resilient_fallback_guard.log";
    if (fs::exists(fallbackFile))
    {
        std::error_code ec;
        fs::remove(fallbackFile, ec);
    }
    
    {
        auto primary = std::make_shared<FailingSink>();
        auto fallback = makeFileSink(fallbackFile);
        auto resilient = std::make_shared<ResilientSink>(primary, fallback);
        
        auto loc = FATP_SOURCE_LOCATION();
        
        LogRecord record1(LogLevel::Info, "First message", loc);
        resilient->write(record1);
        
        FATP_ASSERT_TRUE(primary->writeCount == 1, "Primary attempted once");
        
        LogRecord record2(LogLevel::Info, "Second message", loc);
        resilient->write(record2);
        
        FATP_ASSERT_TRUE(primary->writeCount == 1, "Primary not attempted after failure");
        
        resilient->flush();
        
        std::string contents = readFileContents(fallbackFile);
        FATP_ASSERT_TRUE(contents.find("First message") != std::string::npos, "Fallback has first message");
        FATP_ASSERT_TRUE(contents.find("Second message") != std::string::npos, "Fallback has second message");
    }
    
    if (fs::exists(fallbackFile))
    {
        std::error_code ec;
        fs::remove(fallbackFile, ec);
    }
    
    return true;
}

FATP_TEST_CASE(rotating_file_sink_multiple_rotations_with_guard)
{
    std::string filename = "test_rotate_multi_guard.log";
    cleanupTestFiles(filename);
    
    {
        auto sink = makeRotatingFileSink(filename, 150, 3);
        auto loc = FATP_SOURCE_LOCATION();
        
        for (int i = 0; i < 30; ++i)
        {
            LogRecord record(LogLevel::Info, std::string(40, 'A'), loc);
            sink->write(record);
            sink->flush();
        }
        
        FATP_ASSERT_TRUE(sink->is_valid(), "Sink valid after multiple rotations");
        
        LogRecord finalRecord(LogLevel::Info, "Final message after rotations", loc);
        sink->write(finalRecord);
        sink->flush();
        
        std::string contents = readFileContents(filename);
        FATP_ASSERT_TRUE(contents.find("Final message") != std::string::npos, "Final message written successfully");
    }
    
    cleanupTestFiles(filename);
    return true;
}

}

} // namespace fat_p::testing::diagnosticlogger_io

namespace fat_p::testing
{

bool test_DiagnosticLogger_IO()
{
    FATP_PRINT_HEADER(DIAGNOSTIC LOGGER IO)
    
    TestRunner runner;
    
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, file_sink_basic);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, file_sink_multiple_writes);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, file_sink_append_mode);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, file_sink_invalid_path);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, ring_buffer_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, ring_buffer_overflow);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, rotating_file_sink_basic);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, rotating_file_sink_rotation);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, rotating_file_sink_scope_guard_safety);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, rotating_file_sink_multiple_rotations_with_guard);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, resilient_sink_primary_works);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, resilient_sink_scope_guard_state_management);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, log_level_guard_restoration);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, async_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, async_sink_flush_consistency);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, async_sink_high_load);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, rate_limiting_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, rate_limiting_burst);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, filtering_sink);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, filtering_sink_custom_filter);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, initialize_rotating_logger);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, combined_sinks);
    FATP_RUN_TEST_NS(runner, diagnosticlogger_io, async_with_filtering);
    
    diagnosticlogger_io::benchmark_io_sinks();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_DiagnosticLogger_IO() ? 0 : 1;
}
#endif
