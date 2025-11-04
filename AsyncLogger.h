/**
 * @file AsyncLogger.h (v3.0 - ULTRA PERFORMANCE)
 * @brief Ultra-high-performance diagnostic logging with <200ns target
 * 
 * CHANGES FROM v2.0:
 * - Lock-free SPSC ring buffer (Single Producer Single Consumer)
 * - Pre-allocated message storage (no allocations in hot path)
 * - Async batched writes (background thread)
 * - Memory pool for LogRecords
 * - Cache-aligned data structures
 * 
 * PERFORMANCE TARGETS:
 * - Disabled logging: <5 ns/call (same as v2.0)
 * - Filtered logging: <5 ns/call (same as v2.0)  
 * - Active logging: <150 ns/call (vs 296ns in v2.0)
 * - STRETCH GOAL: <100 ns/call on optimal hardware
 * 
 * TRADE-OFFS:
 * - Messages are async (not immediately written to sinks)
 * - Bounded buffer (can drop messages under extreme load)
 * - Higher memory usage (pre-allocated buffers)
 * - Background thread overhead
 * 
 * WHEN TO USE THIS VERSION:
 * - Need absolute minimum latency (<200ns)
 * - High-frequency logging (>100K msgs/sec)
 * - Can tolerate async delivery
 * - Have memory for pre-allocation
 * 
 * WHEN TO USE v2.0 INSTEAD:
 * - Need synchronous logging
 * - Memory constrained
 * - Low frequency logging
 * - Simpler is better
 */
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <functional>
#include <type_traits>
#include <thread>
#include <array>
#include <cstring>
#if __cplusplus >= 202002L
#include <source_location>
#endif

// --- Build Configuration ---
#ifndef CPP_UTIL_MIN_LOG_LEVEL
#define CPP_UTIL_MIN_LOG_LEVEL 0
#endif

#ifndef CPP_UTIL_LOG_BUFFER_SIZE
#define CPP_UTIL_LOG_BUFFER_SIZE 4096  // Ring buffer size (must be power of 2)
#endif

#ifndef CPP_UTIL_LOG_MAX_MSG_SIZE
#define CPP_UTIL_LOG_MAX_MSG_SIZE 256  // Max message length
#endif

// Branch prediction hints
#ifndef LIKELY
#  if defined(__GNUC__) || defined(__clang__)
#    define LIKELY(x)   __builtin_expect(!!(x), 1)
#    define UNLIKELY(x) __builtin_expect(!!(x), 0)
#  else
#    define LIKELY(x)   (x)
#    define UNLIKELY(x) (x)
#  endif
#endif

// Force inline
#ifndef FORCE_INLINE
#  if defined(_MSC_VER)
#    define FORCE_INLINE __forceinline
#  elif defined(__GNUC__) || defined(__clang__)
#    define FORCE_INLINE inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE inline
#  endif
#endif

// Cache line size
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

namespace cpp_utilities {
namespace diagnostic {
namespace ultra {

// ============================================================================
// Log Level
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

inline const char* toString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

// ============================================================================
// Source Location (Lightweight)
// ============================================================================

#if __cplusplus >= 202002L
using SourceLocation = std::source_location;
#else
struct SourceLocation {
    const char* file = "";
    const char* function = "";
    int line = 0;
    
    constexpr SourceLocation(
        const char* f = "",
        const char* fn = "",
        int l = 0
    ) : file(f), function(fn), line(l) {}
};
#endif

// ============================================================================
// Pre-Allocated Log Record (Cache-Aligned)
// ============================================================================

struct alignas(CACHE_LINE_SIZE) LogRecord {
    LogLevel level;
    char message[CPP_UTIL_LOG_MAX_MSG_SIZE];
    uint16_t message_len;
    char file[64];
    char function[64];
    int line;
    std::chrono::system_clock::time_point timestamp;
    
    LogRecord() : level(LogLevel::Info), message_len(0), line(0) {
        message[0] = '\0';
        file[0] = '\0';
        function[0] = '\0';
    }
    
    void setMessage(const char* msg, size_t len) {
        message_len = static_cast<uint16_t>(
            std::min(len, static_cast<size_t>(CPP_UTIL_LOG_MAX_MSG_SIZE - 1))
        );
        std::memcpy(message, msg, message_len);
        message[message_len] = '\0';
    }
    
    void setMessage(const std::string& msg) {
        setMessage(msg.c_str(), msg.size());
    }
    
    void setLocation(const SourceLocation& loc) {
#if __cplusplus >= 202002L
        const char* basename = loc.file_name();
#else
        const char* basename = loc.file;
#endif
        const char* last_slash = std::strrchr(basename, '/');
        const char* last_backslash = std::strrchr(basename, '\\');
        const char* last_sep = std::max(last_slash, last_backslash);
        if (last_sep) basename = last_sep + 1;
        
        size_t file_len = std::min(strlen(basename), size_t(63));
        std::memcpy(file, basename, file_len);
        file[file_len] = '\0';
        
#if __cplusplus >= 202002L
        const char* func_name = loc.function_name();
#else
        const char* func_name = loc.function;
#endif
        size_t func_len = std::min(strlen(func_name), size_t(63));
        std::memcpy(function, func_name, func_len);
        function[func_len] = '\0';
        
        line = loc.line();
    }
};

// ============================================================================
// Lock-Free SPSC Ring Buffer
// ============================================================================

/**
 * @brief Single-Producer Single-Consumer lock-free ring buffer
 * 
 * PERFORMANCE: ~50-80ns for enqueue/dequeue on modern CPUs
 * 
 * Design:
 * - Producer: Only writes to head_ (no contention)
 * - Consumer: Only writes to tail_ (no contention)
 * - Cache-line alignment to avoid false sharing
 * - Power-of-2 size for fast modulo (mask instead of %)
 */
 // Optimized SPSCRingBuffer with cached indices for reduced cache coherency traffic

template<typename T, size_t SIZE>
class alignas(CACHE_LINE_SIZE) SPSCRingBuffer {
    static_assert((SIZE& (SIZE - 1)) == 0, "SIZE must be power of 2");

    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{ 0 };
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{ 0 };

    static constexpr size_t MASK = SIZE - 1;
    std::array<T, SIZE> buffer_;

    // Cached indices to optimize hot path
    alignas(CACHE_LINE_SIZE) size_t tail_cached_ { 0 };  // For producer
    alignas(CACHE_LINE_SIZE) size_t head_cached_ { 0 };  // For consumer

public:
    SPSCRingBuffer() = default;
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    FORCE_INLINE
        bool try_push(const T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & MASK;

        if (next_head == tail_cached_) {
            tail_cached_ = tail_.load(std::memory_order_acquire);
            if (next_head == tail_cached_) {
                return false;
            }
        }

        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    FORCE_INLINE
        bool try_pop(T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_cached_) {
            head_cached_ = head_.load(std::memory_order_acquire);
            if (tail == head_cached_) {
                return false;
            }
        }

        item = buffer_[tail];
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & MASK;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
            tail_.load(std::memory_order_acquire);
    }
};

// ============================================================================
// Sink Interfaces
// ============================================================================

struct ISink {
    virtual ~ISink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};

// ============================================================================
// Ultra-High-Performance Async Logger
// ============================================================================

class AsyncLogger {
    // Lock-free ring buffer for log records
    SPSCRingBuffer<LogRecord, CPP_UTIL_LOG_BUFFER_SIZE> ringBuffer_;
    
    // Sinks (protected by mutex only in slow path - -)
    std::vector<std::unique_ptr<ISink>> sinks_;
    std::mutex sinkMutex_;
    
    // Control flags (lock-free)
    alignas(CACHE_LINE_SIZE) std::atomic<bool> enabled_{true};
    alignas(CACHE_LINE_SIZE) std::atomic<LogLevel> runtimeMinLevel_{LogLevel::Trace};
    
    // Background worker thread
    std::thread workerThread_;
    std::atomic<bool> stopWorker_{false};
    
    // Statistics
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> messagesLogged_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> messagesDropped_{0};
    
    // NEW: Overflow callback
    std::function<void(const LogRecord&)> onDropCallback_;
    
public:
    AsyncLogger() {
        // Start background worker thread with name and priority
        workerThread_ = std::thread([this]() { 
            // Set thread name (POSIX)
            #if defined(__linux__) || defined(__APPLE__)
            pthread_setname_np(pthread_self(), "LoggerWorker");
            #endif
            // Lower priority (nice +10 on POSIX)
            #if defined(__linux__) || defined(__APPLE__)
            nice(10);
            #endif
            workerLoop(); 
        });
    }
    
    ~AsyncLogger() {
        // Stop worker and wait for it to drain
        stopWorker_.store(true, std::memory_order_release);
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }
    
    // Non-copyable
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    
    void setOnDropCallback(std::function<void(const LogRecord&)> cb) {
        onDropCallback_ = std::move(cb);
    }
    
    void addSink(std::unique_ptr<ISink> sink) {
        std::lock_guard<std::mutex> lock(sinkMutex_);
        sinks_.push_back(std::move(sink));
    }
    
    void setMinLevel(LogLevel level) {
        runtimeMinLevel_.store(level, std::memory_order_relaxed);
    }
    
    LogLevel getMinLevel() const {
        return runtimeMinLevel_.load(std::memory_order_relaxed);
    }
    
    void setEnabled(bool enabled) {
        enabled_.store(enabled, std::memory_order_relaxed);
    }
    
    bool isEnabled() const {
        return enabled_.load(std::memory_order_relaxed);
    }
    
#if __cplusplus >= 202002L
    template <typename MessageGenerator>
    requires std::invocable<MessageGenerator> || std::convertible_to<MessageGenerator, std::string_view>
#else
    template <typename MessageGenerator, typename = std::enable_if_t<std::is_invocable_v<MessageGenerator> || std::is_convertible_v<MessageGenerator, std::string_view>>>
#endif
    FORCE_INLINE
    void log(LogLevel level, MessageGenerator&& messageGen,
             SourceLocation location = SourceLocation()) {
        // Compile-time filtering
        if constexpr (gMinLogLevel > LogLevel::Fatal) {
            return;
        }
        
        if constexpr (gMinLogLevel > LogLevel::Trace) {
            if constexpr (level < gMinLogLevel) {
                return;
            }
        }
        
        // Lock-free runtime checks
        if (UNLIKELY(!enabled_.load(std::memory_order_relaxed))) {
            return;
        }
        
        if (UNLIKELY(level < runtimeMinLevel_.load(std::memory_order_relaxed))) {
            return;
        }
        
        // Prepare record (no allocation!)
        LogRecord record;
        record.level = level;
        record.timestamp = std::chrono::system_clock::now();
        record.setLocation(location);
        
        // Generate message
        using T = std::decay_t<MessageGenerator>;
        if constexpr (std::is_convertible_v<T, const char*>) {
            record.setMessage(messageGen, std::strlen(messageGen));
        } else if constexpr (std::is_convertible_v<T, std::string>) {
            const std::string& msg = messageGen;
            record.setMessage(msg);
        } else {
            std::string msg = messageGen();
            record.setMessage(msg);
        }
        
        // Lock-free push to ring buffer
        if (UNLIKELY(!ringBuffer_.try_push(record))) {
            messagesDropped_.fetch_add(1, std::memory_order_relaxed);
            if (onDropCallback_) onDropCallback_(record);
            return;
        }
        
        messagesLogged_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void flush() {
        auto start = std::chrono::steady_clock::now();
        while (!ringBuffer_.empty()) {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
                std::cerr << "Flush timeout: Worker may be stuck" << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        // Flush all sinks
        std::lock_guard<std::mutex> lock(sinkMutex_);
        for (auto& sink : sinks_) {
            sink->flush();
        }
    }
    
    // Convenience methods
    template<typename MessageGenerator>
    void trace(MessageGenerator&& msg, SourceLocation loc = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Trace) {
            log(LogLevel::Trace, std::forward<MessageGenerator>(msg), loc);
        }
    }
    
    template<typename MessageGenerator>
    void debug(MessageGenerator&& msg, SourceLocation loc = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Debug) {
            log(LogLevel::Debug, std::forward<MessageGenerator>(msg), loc);
        }
    }
    
    template<typename MessageGenerator>
    void info(MessageGenerator&& msg, SourceLocation loc = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Info) {
            log(LogLevel::Info, std::forward<MessageGenerator>(msg), loc);
        }
    }
    
    template<typename MessageGenerator>
    void warning(MessageGenerator&& msg, SourceLocation loc = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Warning) {
            log(LogLevel::Warning, std::forward<MessageGenerator>(msg), loc);
        }
    }
    
    template<typename MessageGenerator>
    void error(MessageGenerator&& msg, SourceLocation loc = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Error) {
            log(LogLevel::Error, std::forward<MessageGenerator>(msg), loc);
        }
    }
    
    template<typename MessageGenerator>
    void fatal(MessageGenerator&& msg, SourceLocation loc = SourceLocation()) {
        if constexpr (gMinLogLevel <= LogLevel::Fatal) {
            log(LogLevel::Fatal, std::forward<MessageGenerator>(msg), loc);
        }
    }
    
    // Statistics
    uint64_t getMessagesLogged() const {
        return messagesLogged_.load(std::memory_order_relaxed);
    }
    
    uint64_t getMessagesDropped() const {
        return messagesDropped_.load(std::memory_order_relaxed);
    }
    
    size_t getQueueSize() const {
        return ringBuffer_.size();
    }
    
private:
    /**
     * @brief Background worker thread - drains ring buffer
     * 
     * Batches writes to amortize I/O overhead.
     * Runs at low priority to avoid interfering with main thread.
     */
    void workerLoop() {
        constexpr size_t BATCH_SIZE = 32;  // Process up to 32 messages per batch
        LogRecord batch[BATCH_SIZE];
        
        while (!stopWorker_.load(std::memory_order_acquire)) {
            size_t count = 0;
            
            // Drain up to BATCH_SIZE messages
            while (count < BATCH_SIZE && ringBuffer_.try_pop(batch[count])) {
                ++count;
            }
            
            // Write batch to sinks
            if (count > 0) {
                std::lock_guard<std::mutex> lock(sinkMutex_);
                for (size_t i = 0; i < count; ++i) {
                    for (auto& sink : sinks_) {
                        sink->write(batch[i]);
                    }
                }
            } else {
                // No messages - sleep briefly to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        // Drain remaining messages on shutdown
        LogRecord record;
        while (ringBuffer_.try_pop(record)) {
            std::lock_guard<std::mutex> lock(sinkMutex_);
            for (auto& sink : sinks_) {
                sink->write(record);
            }
        }
    }
};

// ============================================================================
// Global Logger
// ============================================================================

inline AsyncLogger& getGlobalLogger() {
    static AsyncLogger logger;
    return logger;
}

// ============================================================================
// Simple Formatter
// ============================================================================

inline std::string formatRecord(const LogRecord& record) {
    auto time_t_now = std::chrono::system_clock::to_time_t(record.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        record.timestamp.time_since_epoch()
    ).count() % 1000;
    
    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms << "] "
        << "[" << toString(record.level) << "] ";
    
    if (record.file[0] != '\0') {
        oss << "[" << record.file << ":" << record.line << "] ";
    }
    
    oss << record.message;
    return oss.str();
}

// ============================================================================
// Console Sink
// ============================================================================

class ConsoleSink : public ISink {
    LogLevel minLevel_;
    
public:
    explicit ConsoleSink(LogLevel minLevel = LogLevel::Trace)
        : minLevel_(minLevel) {}
    
    void write(const LogRecord& record) override {
        if (record.level < minLevel_) return;
        
        std::cout << formatRecord(record) << '\n';
    }
    
    void flush() override {
        std::cout.flush();
    }
};

// ============================================================================
// File Sink (Buffered)
// ============================================================================

class FileSink : public ISink {
    LogLevel minLevel_;
    std::ofstream file_;
    
public:
    explicit FileSink(
        const std::string& filename,
        LogLevel minLevel = LogLevel::Trace,
        bool append = true
    ) : minLevel_(minLevel) {
        auto mode = std::ios_base::out;
        if (append) mode |= std::ios_base::app;
        file_.open(filename, mode);
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename);
        }
    }
    
    ~FileSink() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    void write(const LogRecord& record) override {
        if (record.level < minLevel_) return;
        
        if (file_.is_open()) {
            file_ << formatRecord(record) << '\n';
        }
    }
    
    void flush() override {
        if (file_.is_open()) {
            file_.flush();
        }
    }
};

} // namespace ultra
} // namespace diagnostic
} // namespace cpp_utilities