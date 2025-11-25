/**
 * @file test_SlidingFileWindow.h
 * @brief Comprehensive unit tests for SlidingFileWindow.h
 *
 * @details Complete test suite for:
 * - Binary file I/O with sliding window
 * - Window shifting and paging
 * - Out-of-window fail-safe access
 * - Thread-safe concurrent access
 * - Different serialization policies
 * - Expected error handling
 * - Large file handling
 * - RAII file management
 */

#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <random>
#include <mutex>

#include "SlidingFileWindow.h"
#include "test_SlidingFileWindow.h"
#include "FatPTest.h"

// Mock implementations for standalone compilation
namespace fat_p {

template <typename T, typename E>
class Expected {
public:
    Expected(T value) : has_value_(true), value_(std::move(value)) {}
    Expected(const E& error) : has_value_(false), error_(error) {}
    
    bool has_value() const noexcept { return has_value_; }
    explicit operator bool() const noexcept { return has_value_; }
    
    T& value() { return value_; }
    const T& value() const { return value_; }
    T& operator*() { return value_; }
    const T& operator*() const { return value_; }
    T* operator->() { return &value_; }
    
    E& error() { return error_; }
    const E& error() const { return error_; }
    
private:
    bool has_value_;
    T value_;
    E error_;
};

template <typename E>
class Expected<void, E> {
public:
    Expected() : has_value_(true) {}
    Expected(const E& error) : has_value_(false), error_(error) {}
    
    bool has_value() const noexcept { return has_value_; }
    explicit operator bool() const noexcept { return has_value_; }
    
    E& error() { return error_; }
    const E& error() const { return error_; }
    
private:
    bool has_value_;
    E error_;
};

template <typename E>
Expected<void, E> make_unexpected(E error) {
    return Expected<void, E>(error);
}

class SingleThreadedPolicy {
public:
    class mutex_type {};
    class lock_type {
    public:
        explicit lock_type(mutex_type&) {}
    };
};

class MutexSynchronizationPolicy {
public:
    class mutex_type {
    public:
        void lock() { mtx_.lock(); }
        void unlock() { mtx_.unlock(); }
    private:
        mutable std::mutex mtx_;
    };
    class lock_type {
    public:
        explicit lock_type(mutex_type& m) : m_(m) { m_.lock(); }
        ~lock_type() { m_.unlock(); }
    private:
        mutex_type& m_;
    };
};

} // namespace fat_p

namespace fat_p::testing {

using namespace fat_p;
namespace fs = std::filesystem;

// =============================================================================
// Test Data Structures
// =============================================================================

/**
 * @brief Simple POD structure for binary serialization testing
 */
struct DataPoint {
    double value;
    uint64_t timestamp;
    uint32_t id;
    
    DataPoint() : value(0.0), timestamp(0), id(0) {}
    DataPoint(double v, uint64_t t, uint32_t i) 
        : value(v), timestamp(t), id(i) {}
    
    // Custom serialization
    void Read(std::istream& is) {
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
        is.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
        is.read(reinterpret_cast<char*>(&id), sizeof(id));
    }
    
    void Write(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
        os.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        os.write(reinterpret_cast<const char*>(&id), sizeof(id));
    }
    
    bool operator==(const DataPoint& other) const {
        return value == other.value && 
               timestamp == other.timestamp && 
               id == other.id;
    }
};

/**
 * @brief Simple integer structure for binary policy testing
 */
struct SimpleInt {
    int32_t value;
    
    SimpleInt() : value(0) {}
    explicit SimpleInt(int32_t v) : value(v) {}
    
    bool operator==(const SimpleInt& other) const {
        return value == other.value;
    }
};

// =============================================================================
// Test Helpers
// =============================================================================

/**
 * @brief Create a test file with known data
 */
std::string create_test_file(const std::string& filename, size_t count) {
    std::ofstream file(filename, std::ios::binary);
    
    for (size_t i = 0; i < count; ++i) {
        DataPoint dp(static_cast<double>(i) * 1.5, i * 1000, static_cast<uint32_t>(i));
        dp.Write(file);
    }
    
    file.close();
    return filename;
}

/**
 * @brief Create a large test file for performance testing
 */
std::string create_large_test_file(const std::string& filename, size_t count) {
    std::ofstream file(filename, std::ios::binary);
    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> dist(0.0, 1000.0);
    
    for (size_t i = 0; i < count; ++i) {
        DataPoint dp(dist(rng), i, static_cast<uint32_t>(i));
        dp.Write(file);
    }
    
    file.close();
    return filename;
}

// =============================================================================
// I. Basic Functionality Tests
// =============================================================================

bool test_sliding_window_basic_open_close() {
    
    const std::string filename = "test_basic.bin";
    const size_t element_count = 100;
    create_test_file(filename, element_count);
    
    {
        SlidingFileWindow<DataPoint> window;
        
        auto open_result = window.open(filename, sizeof(DataPoint), 10);
        SIMPLE_ASSERT(open_result.has_value(), "File open failed");
        SIMPLE_ASSERT(window.is_open(), "Window should be open");
        SIMPLE_ASSERT(window.size() == element_count, "Incorrect file size");
        SIMPLE_ASSERT(window.window_size() == 10, "Incorrect window size");
        
        window.close();
        SIMPLE_ASSERT(!window.is_open(), "Window should be closed");
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

bool test_sliding_window_element_access() {
    
    const std::string filename = "test_access.bin";
    const size_t element_count = 50;
    create_test_file(filename, element_count);
    
    SlidingFileWindow<DataPoint> window;
    window.open(filename, sizeof(DataPoint), 10);
    
    // Access first element in window
    auto elem0 = window[0];
    SIMPLE_ASSERT(elem0.has_value(), "Failed to access element 0");
    SIMPLE_ASSERT(elem0->value == 0.0, "Incorrect element 0 value");
    SIMPLE_ASSERT(elem0->id == 0, "Incorrect element 0 id");
    
    // Access middle element in window
    auto elem5 = window[5];
    SIMPLE_ASSERT(elem5.has_value(), "Failed to access element 5");
    SIMPLE_ASSERT(elem5->value == 7.5, "Incorrect element 5 value"); // 5 * 1.5
    SIMPLE_ASSERT(elem5->id == 5, "Incorrect element 5 id");
    
    elem5->value = 999.0;
    auto elem5_again = window[5];
    SIMPLE_ASSERT(elem5_again->value == 999.0, "Element modification failed");
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

bool test_sliding_window_shifting() {
    
    const std::string filename = "test_shift.bin";
    const size_t element_count = 100;
    create_test_file(filename, element_count);
    
    SlidingFileWindow<DataPoint> window;
    window.open(filename, sizeof(DataPoint), 10); // Window: [0, 10)
    
    size_t initial_begin = window.begin_index();
    SIMPLE_ASSERT(initial_begin == 0, "Initial begin should be 0");
    
    // Shift window to index 50
    auto shift_result = window.shift_to_index(50);
    SIMPLE_ASSERT(shift_result.has_value(), "Window shift failed");
    
    // Verify window moved
    SIMPLE_ASSERT(window.begin_index() >= 40, "Window should have shifted forward");
    
    auto elem50 = window[50];
    SIMPLE_ASSERT(elem50.has_value(), "Failed to access element after shift");
    SIMPLE_ASSERT(elem50->id == 50, "Incorrect element after shift");
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// II. Serialization Policy Tests
// =============================================================================

bool test_sliding_window_binary_serialization() {
    
    const std::string filename = "test_binary.bin";
    const size_t element_count = 50;
    
    // Create file with SimpleInt elements
    {
        std::ofstream file(filename, std::ios::binary);
        for (size_t i = 0; i < element_count; ++i) {
            int32_t value = static_cast<int32_t>(i * 10);
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    }
    
    // Open with binary serialization policy
    BinarySlidingWindow<SimpleInt> window;
    auto open_result = window.open(filename, sizeof(SimpleInt), 10);
    SIMPLE_ASSERT(open_result.has_value(), "Binary window open failed");
    
    // Verify data
    auto elem0 = window[0];
    SIMPLE_ASSERT(elem0.has_value(), "Failed to access binary element");
    SIMPLE_ASSERT(elem0->value == 0, "Incorrect binary element value");
    
    auto elem25 = window[25];
    SIMPLE_ASSERT(elem25->value == 250, "Incorrect binary element at 25");
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// III. Large File Handling
// =============================================================================

bool test_sliding_window_large_file() {
    
    const std::string filename = "test_large.bin";
    const size_t element_count = 10000;
    const size_t window_size = 100;
    
    create_large_test_file(filename, element_count);
    
    SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, sizeof(DataPoint), window_size);
    SIMPLE_ASSERT(open_result.has_value(), "Large file open failed");
    
    // Random access across entire file
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> dist(0, element_count - 1);
    
    const size_t test_accesses = 100;
    for (size_t i = 0; i < test_accesses; ++i) {
        size_t index = dist(rng);
        auto elem = window[index];
        SIMPLE_ASSERT(elem.has_value(), "Failed random access in large file");
        SIMPLE_ASSERT(elem->id == index, "Incorrect element in large file");
    }
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// IV. Error Handling Tests
// =============================================================================

bool test_sliding_window_error_handling() {
    
    SlidingFileWindow<DataPoint> window;
    
    // Try to access before opening
    auto elem = window[0];
    SIMPLE_ASSERT(!elem.has_value(), "Should fail to access closed window");
    SIMPLE_ASSERT(elem.error() == FileError::FileNotOpen, 
                  "Should report FileNotOpen error");
    
    // Open valid file
    const std::string filename = "test_error.bin";
    create_test_file(filename, 10);
    window.open(filename, sizeof(DataPoint), 5);
    
    auto out_of_bounds = window[1000];
    SIMPLE_ASSERT(!out_of_bounds.has_value(), "Should fail out-of-bounds access");
    SIMPLE_ASSERT(out_of_bounds.error() == FileError::InvalidIndex,
                  "Should report InvalidIndex error");
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// V. Thread Safety Tests
// =============================================================================

bool test_sliding_window_thread_safety() {
    
    const std::string filename = "test_threadsafe.bin";
    const size_t element_count = 1000;
    create_test_file(filename, element_count);
    
    ThreadSafeSlidingWindow<DataPoint> window;
    window.open(filename, sizeof(DataPoint), 100);
    
    const size_t num_threads = 4;
    const size_t reads_per_thread = 100;
    std::atomic<size_t> error_count{0};
    
    auto reader = [&](size_t thread_id) {
        std::mt19937_64 rng(thread_id);
        std::uniform_int_distribution<size_t> dist(0, element_count - 1);
        
        for (size_t i = 0; i < reads_per_thread; ++i) {
            size_t index = dist(rng);
            auto elem = window[index];
            if (!elem || elem->id != index) {
                ++error_count;
            }
        }
    };
    
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(reader, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    SIMPLE_ASSERT(error_count == 0, "Thread-safety violations detected");
    
    std::cout << "    âœ“ " << (num_threads * reads_per_thread) 
              << " concurrent reads completed in " << duration.count() << " ms\n";
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// VI. Lag Offset Tests
// =============================================================================

bool test_sliding_window_lag_offset() {
    
    const std::string filename = "test_lag.bin";
    const size_t element_count = 100;
    create_test_file(filename, element_count);
    
    // Open with lag offset (start near end of file)
    SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, sizeof(DataPoint), 10, 20); // Start at element 80
    
    SIMPLE_ASSERT(open_result.has_value(), "Lag offset open failed");
    SIMPLE_ASSERT(window.begin_index() == 80, "Incorrect lag offset start position");
    
    auto elem85 = window[85];
    SIMPLE_ASSERT(elem85.has_value(), "Failed to access lagged element");
    SIMPLE_ASSERT(elem85->id == 85, "Incorrect lagged element");
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// VII. Performance Tests
// =============================================================================

bool test_sliding_window_performance() {
    
    const std::string filename = "test_perf.bin";
    const size_t element_count = 100000;
    const size_t window_size = 1000;
    
    std::cout << "    Creating large test file...\n";
    create_large_test_file(filename, element_count);
    
    SlidingFileWindow<DataPoint> window;
    window.open(filename, sizeof(DataPoint), window_size);
    
    // Sequential access (in-window, should be fast)
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < window_size && i < element_count; ++i) {
        auto elem = window[i];
        (void)elem; // Suppress unused warning
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    // Random access (requires shifting)
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<size_t> dist(0, element_count - 1);
    const size_t random_accesses = 1000;
    
    for (size_t i = 0; i < random_accesses; ++i) {
        size_t index = dist(rng);
        auto elem = window[index];
        (void)elem;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto seq_time = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto rand_time = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    
    double seq_us_per_access = static_cast<double>(seq_time.count()) / window_size;
    double rand_us_per_access = static_cast<double>(rand_time.count()) / random_accesses;
    
    std::cout << "    âœ“ Sequential access: " << seq_us_per_access << " Âµs/element\n";
    std::cout << "    âœ“ Random access:     " << rand_us_per_access << " Âµs/element\n";
    
    window.close();
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// VIII. Persistence Tests
// =============================================================================

bool test_sliding_window_persistence() {
    
    const std::string filename = "test_persist.bin";
    const size_t element_count = 50;
    create_test_file(filename, element_count);
    
    // Modify data and close
    {
        SlidingFileWindow<DataPoint> window;
        window.open(filename, sizeof(DataPoint), 10);
        
        auto elem5 = window[5];
        elem5->value = 12345.67;
        elem5->timestamp = 999999;
        
        window.close(); // Should flush changes
    }
    
    // Reopen and verify changes persisted
    {
        SlidingFileWindow<DataPoint> window;
        window.open(filename, sizeof(DataPoint), 10);
        
        auto elem5 = window[5];
        SIMPLE_ASSERT(elem5->value == 12345.67, "Modified value not persisted");
        SIMPLE_ASSERT(elem5->timestamp == 999999, "Modified timestamp not persisted");
        
        window.close();
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// IX. Edge Cases
// =============================================================================

bool test_sliding_window_edge_cases() {
    
    const std::string filename = "test_edge.bin";
    
    // Empty file
    {
        std::ofstream empty_file(filename, std::ios::binary);
        empty_file.close();
        
        SlidingFileWindow<DataPoint> window;
        auto result = window.open(filename, sizeof(DataPoint), 10);
        
        SIMPLE_ASSERT(result.has_value(), "Should handle empty file");
        SIMPLE_ASSERT(window.size() == 0, "Empty file should have size 0");
        SIMPLE_ASSERT(window.empty(), "Empty file should report empty");
        
        window.close();
    }
    
    // Single element file
    {
        create_test_file(filename, 1);
        
        SlidingFileWindow<DataPoint> window;
        window.open(filename, sizeof(DataPoint), 10);
        
        SIMPLE_ASSERT(window.size() == 1, "Single element file size incorrect");
        
        auto elem = window[0];
        SIMPLE_ASSERT(elem.has_value(), "Should access single element");
        SIMPLE_ASSERT(elem->id == 0, "Single element has wrong data");
        
        window.close();
    }
    
    // Window size larger than file
    {
        create_test_file(filename, 5);
        
        SlidingFileWindow<DataPoint> window;
        window.open(filename, sizeof(DataPoint), 100); // Window larger than file
        
        SIMPLE_ASSERT(window.window_size() == 5, "Window should be clamped to file size");
        
        window.close();
    }
    
    std::error_code ec;
    fs::remove(filename, ec);
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_SlidingFileWindow() {

    std::cout << "==========================================================\n";
    std::cout << "SLIDING FILE WINDOW UNIT TESTS\n";
    std::cout << "==========================================================\n";

    TestRunner runner;

    RUN_TEST(runner, sliding_window_basic_open_close);
    RUN_TEST(runner, sliding_window_element_access);
    RUN_TEST(runner, sliding_window_shifting);
    RUN_TEST(runner, sliding_window_binary_serialization);
    RUN_TEST(runner, sliding_window_large_file);
    RUN_TEST(runner, sliding_window_error_handling);
    RUN_TEST(runner, sliding_window_thread_safety);
    RUN_TEST(runner, sliding_window_lag_offset);
    RUN_TEST(runner, sliding_window_persistence);
    RUN_TEST(runner, sliding_window_performance);
    RUN_TEST(runner, sliding_window_edge_cases);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
