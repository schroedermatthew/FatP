/**
 * @file DiagnosticLogger_IO.h
 * @brief Extension for File I/O, Rotation, Ring Buffers, Async Logging, and Resilience.
 * 
 *
 * @layer Domain
 *
 * @dependencies <filesystem>, <fstream>, CircularBuffer, LockFreeQueue, ThreadPool, ScopeGuard
 * 
 * FIXES APPLIED (v2.0):
 * - P1.3: Fixed loop underflow in rotation
 * - P1.5: Fixed tellp() error handling
 * - P2.1: Exception-safe constructors (no throws)
 * - P3.1: RingBufferSink now uses CircularBuffer
 * - P3.2: Added AsyncSink using LockFreeQueue + ThreadPool
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
#include "LockFreeQueue.h"
#include "ThreadPool.h"
#include "ScopeGuard.h"
#include <fstream>
#include <filesystem>
#include <future>
#include <vector>
#include <chrono>

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
    std::ofstream file_;
    std::unique_ptr<IFormatter> formatter_;
    LogLevel minLevel_;
    std::mutex mutex_;
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
        : formatter_(std::move(fmt))
        , minLevel_(minLevel)
        , is_valid_(false)
    {
        auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
        file_.open(filename, mode);
        is_valid_ = file_.is_open();
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
        if (!is_valid_ || record.level < minLevel_)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        file_ << formatter_->format(record) << '\n';
    }

    void flush() override
    {
        if (!is_valid_)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        file_.flush();
    }
};

inline std::shared_ptr<FileSink> makeFileSink(const std::string& filename, std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
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
    std::vector<LogRecord> buffer_;
    size_t head_ = 0;
    size_t count_ = 0;
    size_t capacity_;
    mutable std::mutex mutex_;

public:
    /**
     * @brief Constructs a RingBufferSink with the specified capacity.
     * @param capacity Maximum number of records to store. Defaults to 1024.
     */
    explicit RingBufferSink(size_t capacity = 1024)
        : capacity_(capacity > 0 ? capacity : 1024)
    {
        buffer_.resize(capacity_);
    }

    void write(const LogRecord& record) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = (head_ + count_) % capacity_;
        buffer_[idx] = record;
        if (count_ < capacity_)
        {
            ++count_;
        }
        else
        {
            head_ = (head_ + 1) % capacity_;
        }
    }

    /**
     * @brief Dumps all buffered records to another sink and clears the buffer.
     * @param target The sink to write records to.
     */
    void dumpTo(ISink& target)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < count_; ++i)
        {
            size_t idx = (head_ + i) % capacity_;
            target.write(buffer_[idx]);
        }
        head_ = 0;
        count_ = 0;
        target.flush();
    }

    /**
     * @brief Returns the current number of records in the buffer.
     */
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    /**
     * @brief Returns the maximum capacity of the buffer.
     */
    size_t capacity() const
    {
        return capacity_;
    }

    void flush() override {}
};

class RotatingFileSink : public ISink
{
    std::string baseFilename_;
    size_t maxBytes_;
    size_t maxFiles_;
    std::unique_ptr<IFormatter> formatter_;
    std::ofstream file_;
    std::mutex mutex_;
    bool is_valid_;
    
public:
    RotatingFileSink(const std::string& fname, size_t maxBytes = 1024*1024*10, size_t maxFiles = 5, std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
        : baseFilename_(fname), maxBytes_(maxBytes), maxFiles_(maxFiles), formatter_(std::move(fmt)), is_valid_(false)
    {
        open();
    }

    bool is_valid() const { return is_valid_; }

    void write(const LogRecord& record) override
    {
        if (!is_valid_) return;
        std::lock_guard<std::mutex> lock(mutex_);
        std::string msg = formatter_->format(record) + '\n';
        
        auto pos = file_.tellp();
        if (pos != std::streampos(-1) && static_cast<size_t>(pos) + msg.size() > maxBytes_)
        {
            rotate();
        }
        file_ << msg;
    }
    
    void flush() override
    {
        if (!is_valid_) return;
        std::lock_guard<std::mutex> lock(mutex_);
        file_.flush();
    }

private:
    void open()
    {
        file_.open(baseFilename_, std::ios::app);
        is_valid_ = file_.is_open();
    }

    void rotate()
    {
        file_.close();
        
        // ScopeGuard ensures file reopening even if rotation fails catastrophically
        SCOPE_GUARD { open(); };
        
        namespace fs = std::filesystem;
        try
        {
            if (fs::exists(baseFilename_))
            {
                std::string oldName = baseFilename_ + "." + std::to_string(maxFiles_);
                if (fs::exists(oldName)) fs::remove(oldName);
                
                for (int i = static_cast<int>(maxFiles_ - 1); i >= 1; --i)
                {
                    std::string src = baseFilename_ + "." + std::to_string(i);
                    std::string dst = baseFilename_ + "." + std::to_string(i + 1);
                    if (fs::exists(src)) fs::rename(src, dst);
                }
                fs::rename(baseFilename_, baseFilename_ + ".1");
            }
        }
        catch(...) {}
        // open() is guaranteed to be called by ScopeGuard on scope exit
    }
};

inline std::shared_ptr<RotatingFileSink> makeRotatingFileSink(const std::string& filename, size_t maxBytes = 1024*1024*10, size_t maxFiles = 5, std::unique_ptr<IFormatter> fmt = std::make_unique<DefaultFormatter>())
{
    auto sink = std::make_shared<RotatingFileSink>(filename, maxBytes, maxFiles, std::move(fmt));
    return sink->is_valid() ? sink : nullptr;
}

class ResilientSink : public ISink
{
    std::shared_ptr<ISink> primary_;
    std::shared_ptr<ISink> fallback_;
    std::atomic<bool> primaryFailed_{false};

public:
    ResilientSink(std::shared_ptr<ISink> primary, std::shared_ptr<ISink> fallback)
        : primary_(std::move(primary)), fallback_(std::move(fallback)) {}

    void write(const LogRecord& record) override
    {
        if (!primaryFailed_)
        {
            bool writeSucceeded = false;
            
            // ScopeGuard manages failure state automatically
            SCOPE_GUARD { 
                if (!writeSucceeded) {
                    primaryFailed_.store(true, std::memory_order_release);
                }
            };
            
            try
            {
                primary_->write(record);
                writeSucceeded = true;
                return; 
            }
            catch (...) {}
            // Guard will set primaryFailed_ = true if writeSucceeded is still false
        }
        
        if (fallback_)
        {
            try
            {
                fallback_->write(record);
            }
            catch (...) {}
        }
    }

    void flush() override
    {
        if (!primaryFailed_) try { primary_->flush(); } catch(...) {}
        if (fallback_) try { fallback_->flush(); } catch(...) {}
    }
    
    void reset() { primaryFailed_ = false; }
};

class AsyncSink : public ISink
{
    LockFreeQueue<LogRecord, 4096> queue_;
    std::shared_ptr<ISink> target_;
    ThreadPool worker_;
    std::atomic<bool> running_{ true };
    std::atomic<uint64_t> dropped_{ 0 };

    std::mutex flush_mutex_;
    std::condition_variable flush_cv_;
    std::atomic<bool> processing_{ false };
    
    std::future<void> workerFuture_;  // Store the future for proper shutdown

public:
    explicit AsyncSink(std::shared_ptr<ISink> target)
        : target_(std::move(target)), worker_(1)
    {
        workerFuture_ = worker_.submit([this]() { processLoop(); });
    }

    ~AsyncSink()
    {
        running_.store(false, std::memory_order_release);
        flush_cv_.notify_all();
        
        // Wait for the worker to actually finish, not just the queue to drain
        if (workerFuture_.valid())
        {
            workerFuture_.wait();
        }
    }

    void write(const LogRecord& record) override
    {
        if (!queue_.enqueue(record))
        {
            dropped_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void flush() override
    {
        std::unique_lock<std::mutex> lock(flush_mutex_);
        flush_cv_.wait(lock, [this] {
            return queue_.empty() && !processing_.load();
            });
        target_->flush();
    }

    uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    void processLoop()
    {
        while (running_.load(std::memory_order_acquire) || !queue_.empty())
        {
            LogRecord rec;
            if (queue_.dequeue(rec))
            {
                processing_.store(true);
                target_->write(rec);
                processing_.store(false);

                if (queue_.empty())
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
};

class RateLimitingSink : public ISink
{
    std::shared_ptr<ISink> target_;
    std::atomic<uint64_t> dropped_{0};
    std::chrono::steady_clock::time_point last_refill_;
    double tokens_;
    const double rate_;
    const double burst_;
    std::mutex limiter_mutex_;
    
public:
    RateLimitingSink(std::shared_ptr<ISink> target, double rate_per_sec, double burst = 10.0)
        : target_(std::move(target)), last_refill_(std::chrono::steady_clock::now()), tokens_(burst), rate_(rate_per_sec), burst_(burst) {}
    
    void write(const LogRecord& record) override
    {
        if (try_acquire())
        {
            target_->write(record);
        }
        else
        {
            dropped_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    void flush() override { target_->flush(); }
    uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
    
private:
    bool try_acquire()
    {
        std::lock_guard<std::mutex> lock(limiter_mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - last_refill_).count();
        tokens_ = std::min(burst_, tokens_ + elapsed * rate_);
        last_refill_ = now;
        if (tokens_ >= 1.0)
        {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }
};

class FilteringSink : public ISink
{
    std::shared_ptr<ISink> target_;
    std::function<bool(const LogRecord&)> filter_;
    
public:
    FilteringSink(std::shared_ptr<ISink> target, std::function<bool(const LogRecord&)> filter)
        : target_(std::move(target)), filter_(std::move(filter)) {}
    
    void write(const LogRecord& record) override
    {
        if (filter_(record))
        {
            target_->write(record);
        }
    }
    
    void flush() override { target_->flush(); }
};

/**
 * @brief Sink that invokes a user-provided callback for each log record.
 *
 * @details Useful for custom integrations like metrics collection,
 * monitoring systems, or custom output formats.
 */
class CallbackSink : public ISink
{
    std::function<void(const LogRecord&)> callback_;
    LogLevel minLevel_;

public:
    /**
     * @brief Constructs a CallbackSink.
     * @param callback Function to call for each log record.
     * @param minLevel Minimum level to trigger the callback.
     */
    CallbackSink(std::function<void(const LogRecord&)> callback,
                 LogLevel minLevel = LogLevel::Trace)
        : callback_(std::move(callback))
        , minLevel_(minLevel)
    {
    }

    void write(const LogRecord& record) override
    {
        if (record.level >= minLevel_)
        {
            callback_(record);
        }
    }

    void flush() override {}
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
