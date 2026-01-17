/**
 * @file DiagnosticLogger_IO.h
 * @brief Extension for File I/O, Rotation, Ring Buffers, Async Logging, and Resilience.
 *
 *
 * @layer Domain
 *
 * @dependencies <filesystem>, <fstream>, CircularBuffer, ThreadPool, ScopeGuard
 *
 * FIXES APPLIED (v2.0):
 * - P1.3: Fixed loop underflow in rotation
 * - P1.5: Fixed tellp() error handling
 * - P2.1: Exception-safe constructors (no throws)
 * - P3.1: RingBufferSink now uses CircularBuffer
 * - P3.2: Added AsyncSink using mutex-protected queue + ThreadPool
 * - P3.3: Added RateLimitingSink
 * - P3.4: Added FilteringSink
 *
 * IMPROVEMENTS (v2.1):
 * - Added ScopeGuard integration for safer resource management
 * - Enhanced RotatingFileSink::rotate() with guaranteed file reopening
 * - Enhanced ResilientSink with automatic failure state management
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: DiagnosticLogger_IO
  file_role: public_header
  path: fat_p/DiagnosticLogger_IO.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for DiagnosticLogger_IO."
  api_stability: in_work
  related:
    docs_search: "DiagnosticLogger_IO"
    tests:
      - tests/test_DiagnosticLogger_IO.cpp
      - tests/test_DiagnosticLogger_ScopeGuard.cpp
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
#include "ScopeGuard.h"
#include "ThreadPool.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <queue>
#include <vector>

namespace fat_p
{
namespace diagnostic
{

/**
 * @brief Sink that writes log records to a file.
 *
 * @details Thread-safe file output with configurable minimum level and append mode.
 * The destructor ensures all buffered data is flushed before the file is closed.
 */
class FileSink : public ISink
{
    std::ofstream mFile;
    std::unique_ptr<IFormatter> mFormatter;
    LogLevel mMinLevel;
    std::mutex mMutex;
    bool is_valid_;

public:
    /**
     * @brief Constructs a FileSink for the given file.
     * @param filename Path to the log file.
     * @param fmt Formatter to use. Defaults to DefaultFormatter.
     * @param minLevel Minimum level to write. Defaults to Trace (all levels).
     * @param append If true, append to existing file. If false, truncate.
     */
    explicit FileSink(const std::string& filename,
                      std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>(),
                      LogLevel minLevel = LogLevel::Trace,
                      bool append = true)
        : mFormatter(std::move(fmt))
        , mMinLevel(minLevel)
        , is_valid_(false)
    {
        auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
        mFile.open(filename, mode);
        is_valid_ = mFile.is_open();
    }

    ~FileSink() noexcept
    {
        try
        {
            flush();
        }
        catch (...)
        {
        }
    }

    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    bool is_valid() const
    {
        return is_valid_;
    }

    void write(const LogRecord& record) override
    {
        if (!is_valid_ || record.level < mMinLevel)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mMutex);
        mFile << mFormatter->format(record) << '\n';
    }

    void flush() override
    {
        if (!is_valid_)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mMutex);
        mFile.flush();
    }
};

inline std::shared_ptr<FileSink> makeFileSink(const std::string& filename,
                                              std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
{
    auto sink = std::make_shared<FileSink>(filename, std::move(fmt));
    return sink->is_valid() ? sink : nullptr;
}

/**
 * @brief In-memory ring buffer sink for crash diagnostics.
 *
 * @details Stores the last N log records in memory. When an error occurs,
 * call dumpTo() to flush the buffer to a persistent sink for analysis.
 * New records overwrite old ones when the buffer is full.
 */
class RingBufferSink : public ISink
{
    std::vector<LogRecord> mBuffer;
    size_t mHead = 0;
    size_t mCount = 0;
    size_t mCapacity;
    mutable std::mutex mMutex;

public:
    /**
     * @brief Constructs a RingBufferSink with the specified capacity.
     * @param capacity Maximum number of records to store. Defaults to 1024.
     */
    explicit RingBufferSink(size_t capacity = 1024)
        : mCapacity(capacity > 0 ? capacity : 1024)
    {
        mBuffer.resize(mCapacity);
    }

    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        size_t idx = (mHead + mCount) % mCapacity;
        mBuffer[idx] = record;
        if (mCount < mCapacity)
        {
            ++mCount;
        }
        else
        {
            mHead = (mHead + 1) % mCapacity;
        }
    }

    /**
     * @brief Dumps all buffered records to another sink and clears the buffer.
     * @param target The sink to write records to.
     */
    void dumpTo(ISink& target)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (size_t i = 0; i < mCount; ++i)
        {
            size_t idx = (mHead + i) % mCapacity;
            target.write(mBuffer[idx]);
        }
        mHead = 0;
        mCount = 0;
        target.flush();
    }

    /**
     * @brief Returns the current number of records in the buffer.
     */
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mCount;
    }

    /**
     * @brief Returns the maximum capacity of the buffer.
     */
    size_t capacity() const
    {
        return mCapacity;
    }

    void flush() override
    {
    }
};

class RotatingFileSink : public ISink
{
    std::string mBaseFilename;
    size_t mMaxBytes;
    size_t mMaxFiles;
    std::unique_ptr<IFormatter> mFormatter;
    std::ofstream mFile;
    std::mutex mMutex;
    bool is_valid_;

public:
    RotatingFileSink(const std::string& fname,
                     size_t maxBytes = 1024 * 1024 * 10,
                     size_t maxFiles = 5,
                     std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
        : mBaseFilename(fname)
        , mMaxBytes(maxBytes)
        , mMaxFiles(maxFiles)
        , mFormatter(std::move(fmt))
        , is_valid_(false)
    {
        open();
    }

    bool is_valid() const
    {
        return is_valid_;
    }

    void write(const LogRecord& record) override
    {
        if (!is_valid_)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mMutex);
        std::string msg = mFormatter->format(record) + '\n';

        auto pos = mFile.tellp();
        if (pos != std::streampos(-1) && static_cast<size_t>(pos) + msg.size() > mMaxBytes)
        {
            rotate();
        }
        mFile << msg;
    }

    void flush() override
    {
        if (!is_valid_)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mMutex);
        mFile.flush();
    }

private:
    void open()
    {
        mFile.open(mBaseFilename, std::ios::app);
        is_valid_ = mFile.is_open();
    }

    void rotate()
    {
        mFile.close();

        // ScopeGuard ensures file reopening even if rotation fails catastrophically
        FATP_SCOPE_GUARD
        {
            open();
        };

        namespace fs = std::filesystem;
        try
        {
            if (fs::exists(mBaseFilename))
            {
                std::string oldName = mBaseFilename + "." + std::to_string(mMaxFiles);
                if (fs::exists(oldName))
                {
                    fs::remove(oldName);
                }

                for (int i = static_cast<int>(mMaxFiles - 1); i >= 1; --i)
                {
                    std::string src = mBaseFilename + "." + std::to_string(i);
                    std::string dst = mBaseFilename + "." + std::to_string(i + 1);
                    if (fs::exists(src))
                    {
                        fs::rename(src, dst);
                    }
                }
                fs::rename(mBaseFilename, mBaseFilename + ".1");
            }
        }
        catch (...)
        {
        }
        // open() is guaranteed to be called by ScopeGuard on scope exit
    }
};

inline std::shared_ptr<RotatingFileSink>
makeRotatingFileSink(const std::string& filename,
                     size_t maxBytes = 1024 * 1024 * 10,
                     size_t maxFiles = 5,
                     std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
{
    auto sink = std::make_shared<RotatingFileSink>(filename, maxBytes, maxFiles, std::move(fmt));
    return sink->is_valid() ? sink : nullptr;
}

class ResilientSink : public ISink
{
    std::shared_ptr<ISink> mPrimary;
    std::shared_ptr<ISink> mFallback;
    std::atomic<bool> mPrimaryFailed{false};

public:
    ResilientSink(std::shared_ptr<ISink> primary, std::shared_ptr<ISink> fallback)
        : mPrimary(std::move(primary))
        , mFallback(std::move(fallback))
    {
    }

    void write(const LogRecord& record) override
    {
        if (!mPrimaryFailed)
        {
            bool writeSucceeded = false;

            // ScopeGuard manages failure state automatically
            FATP_SCOPE_GUARD
            {
                if (!writeSucceeded)
                {
                    mPrimaryFailed.store(true, std::memory_order_release);
                }
            };

            try
            {
                mPrimary->write(record);
                writeSucceeded = true;
                return;
            }
            catch (...)
            {
            }
            // Guard will set mPrimaryFailed = true if writeSucceeded is still false
        }

        if (mFallback)
        {
            try
            {
                mFallback->write(record);
            }
            catch (...)
            {
            }
        }
    }

    void flush() override
    {
        if (!mPrimaryFailed)
        {
            try
            {
                mPrimary->flush();
            }
            catch (...)
            {
            }
        }
        if (mFallback)
        {
            try
            {
                mFallback->flush();
            }
            catch (...)
            {
            }
        }
    }

    void reset()
    {
        mPrimaryFailed = false;
    }
};

class AsyncSink : public ISink
{
    std::queue<LogRecord> mQueue;
    mutable std::mutex mQueueMutex;
    std::shared_ptr<ISink> mTarget;
    ThreadPool mWorker;
    std::atomic<bool> mRunning{true};
    std::atomic<uint64_t> mDropped{0};

    std::mutex flush_mutex_;
    std::condition_variable flush_cv_;
    std::atomic<bool> mProcessing{false};

    std::future<void> mWorkerFuture; // Store the future for proper shutdown

public:
    explicit AsyncSink(std::shared_ptr<ISink> target)
        : mTarget(std::move(target))
        , mWorker(1)
    {
        mWorkerFuture = mWorker.submit([this]() {
            processLoop();
        });
    }

    ~AsyncSink()
    {
        mRunning.store(false, std::memory_order_release);
        flush_cv_.notify_all();

        // Wait for the worker to actually finish, not just the queue to drain
        if (mWorkerFuture.valid())
        {
            mWorkerFuture.wait();
        }
    }

    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mQueue.push(record);
    }

    void flush() override
    {
        std::unique_lock<std::mutex> lock(flush_mutex_);
        flush_cv_.wait(lock, [this] {
            std::lock_guard<std::mutex> qlock(mQueueMutex);
            return mQueue.empty() && !mProcessing.load();
        });
        mTarget->flush();
    }

    uint64_t dropped() const
    {
        return mDropped.load(std::memory_order_relaxed);
    }

private:
    void processLoop()
    {
        while (mRunning.load(std::memory_order_acquire) || !queueEmpty())
        {
            LogRecord rec;
            if (tryDequeue(rec))
            {
                mProcessing.store(true);
                mTarget->write(rec);
                mProcessing.store(false);

                if (queueEmpty())
                {
                    flush_cv_.notify_all();
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

    bool queueEmpty() const
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        return mQueue.empty();
    }

    bool tryDequeue(LogRecord& rec)
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        if (mQueue.empty())
        {
            return false;
        }
        rec = std::move(mQueue.front());
        mQueue.pop();
        return true;
    }
};

class RateLimitingSink : public ISink
{
    std::shared_ptr<ISink> mTarget;
    std::atomic<uint64_t> mDropped{0};
    std::chrono::steady_clock::time_point last_refill_;
    double mTokens;
    const double mRate;
    const double mBurst;
    std::mutex limiter_mutex_;

public:
    RateLimitingSink(std::shared_ptr<ISink> target, double rate_per_sec, double burst = 10.0)
        : mTarget(std::move(target))
        , last_refill_(std::chrono::steady_clock::now())
        , mTokens(burst)
        , mRate(rate_per_sec)
        , mBurst(burst)
    {
    }

    void write(const LogRecord& record) override
    {
        if (try_acquire())
        {
            mTarget->write(record);
        }
        else
        {
            mDropped.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void flush() override
    {
        mTarget->flush();
    }
    uint64_t dropped() const
    {
        return mDropped.load(std::memory_order_relaxed);
    }

private:
    bool try_acquire()
    {
        std::lock_guard<std::mutex> lock(limiter_mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - last_refill_).count();
        mTokens = std::min(mBurst, mTokens + elapsed * mRate);
        last_refill_ = now;
        if (mTokens >= 1.0)
        {
            mTokens -= 1.0;
            return true;
        }
        return false;
    }
};

class FilteringSink : public ISink
{
    std::shared_ptr<ISink> mTarget;
    std::function<bool(const LogRecord&)> mFilter;

public:
    FilteringSink(std::shared_ptr<ISink> target, std::function<bool(const LogRecord&)> filter)
        : mTarget(std::move(target))
        , mFilter(std::move(filter))
    {
    }

    void write(const LogRecord& record) override
    {
        if (mFilter(record))
        {
            mTarget->write(record);
        }
    }

    void flush() override
    {
        mTarget->flush();
    }
};

/**
 * @brief Sink that invokes a user-provided callback for each log record.
 *
 * @details Useful for custom integrations like metrics collection,
 * monitoring systems, or custom output formats.
 */
class CallbackSink : public ISink
{
    std::function<void(const LogRecord&)> mCallback;
    LogLevel mMinLevel;

public:
    /**
     * @brief Constructs a CallbackSink.
     * @param callback Function to call for each log record.
     * @param minLevel Minimum level to trigger the callback.
     */
    CallbackSink(std::function<void(const LogRecord&)> callback, LogLevel minLevel = LogLevel::Trace)
        : mCallback(std::move(callback))
        , mMinLevel(minLevel)
    {
    }

    void write(const LogRecord& record) override
    {
        if (record.level >= mMinLevel)
        {
            mCallback(record);
        }
    }

    void flush() override
    {
    }
};

inline void initializeRotatingLogger(const std::string& filename)
{
    auto sink = makeRotatingFileSink(filename);
    if (sink)
    {
        getGlobalLogger().addSink(sink);
    }
}

inline void initializeAsyncLogger(std::shared_ptr<ISink> target)
{
    getGlobalLogger().addSink(std::make_shared<AsyncSink>(std::move(target)));
}

inline void initializeRateLimitedLogger(std::shared_ptr<ISink> target, double rate_per_sec)
{
    getGlobalLogger().addSink(std::make_shared<RateLimitingSink>(std::move(target), rate_per_sec));
}

} // namespace diagnostic
} // namespace fat_p
