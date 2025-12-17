#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "SlidingFileWindow.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_SlidingFileWindow.h"
#endif

namespace fat_p::testing::slidingfilewindow
{

namespace fs = std::filesystem;

// =============================================================================
// Test Data Structures
// =============================================================================

struct DataPoint
{
    double value;
    uint64_t timestamp;
    uint32_t id;

    DataPoint() : value(0.0), timestamp(0), id(0) {}
    DataPoint(double v, uint64_t t, uint32_t i) : value(v), timestamp(t), id(i) {}

    void Read(std::istream& is)
    {
        is.read(reinterpret_cast<char*>(&value), sizeof(value));
        is.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
        is.read(reinterpret_cast<char*>(&id), sizeof(id));
    }

    void Write(std::ostream& os) const
    {
        os.write(reinterpret_cast<const char*>(&value), sizeof(value));
        os.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        os.write(reinterpret_cast<const char*>(&id), sizeof(id));
    }

    bool operator==(const DataPoint& other) const
    {
        return value == other.value && timestamp == other.timestamp && id == other.id;
    }
    
    static constexpr size_t serialized_size() 
    {
        return sizeof(double) + sizeof(uint64_t) + sizeof(uint32_t);
    }
};

struct SimpleInt
{
    int32_t value;

    SimpleInt() : value(0) {}
    explicit SimpleInt(int32_t v) : value(v) {}

    bool operator==(const SimpleInt& other) const { return value == other.value; }
};

// =============================================================================
// Test Helpers
// =============================================================================

class TempFile
{
public:
    explicit TempFile(const std::string& name) : path_(name) {}

    ~TempFile()
    {
        std::error_code ec;
        fs::remove(path_, ec);
    }

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

void create_test_file(const std::string& filename, size_t count)
{
    std::ofstream file(filename, std::ios::binary);
    for (size_t i = 0; i < count; ++i)
    {
        DataPoint dp(static_cast<double>(i) * 1.5, i * 1000, static_cast<uint32_t>(i));
        dp.Write(file);
    }
    file.close();
}

void create_large_test_file(const std::string& filename, size_t count)
{
    std::ofstream file(filename, std::ios::binary);
    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> dist(0.0, 1000.0);

    for (size_t i = 0; i < count; ++i)
    {
        DataPoint dp(dist(rng), i, static_cast<uint32_t>(i));
        dp.Write(file);
    }
    file.close();
}

// =============================================================================
// I. Basic Functionality Tests
// =============================================================================

TEST_CASE(basic_open_close)
{
    const std::string filename = "test_basic.bin";
    const size_t element_count = 100;
    create_test_file(filename, element_count);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;

    auto open_result = window.open(filename, DataPoint::serialized_size(), 10);
    SIMPLE_ASSERT(open_result.has_value(), "File open failed");
    SIMPLE_ASSERT(window.is_open(), "Window should be open");
    ASSERT_EQ(window.size(), element_count, "Incorrect file size");
    ASSERT_EQ(window.window_size(), 10u, "Incorrect window size");

    window.close();
    SIMPLE_ASSERT(!window.is_open(), "Window should be closed");

    return true;
}

TEST_CASE(element_access)
{
    const std::string filename = "test_access.bin";
    const size_t element_count = 50;
    create_test_file(filename, element_count);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, DataPoint::serialized_size(), 10);
    SIMPLE_ASSERT(open_result.has_value(), "Failed to open file");

    auto elem0 = window[0];
    SIMPLE_ASSERT(elem0.has_value(), "Failed to access element 0");
    ASSERT_CLOSE(elem0->get().value, 0.0, "Incorrect element 0 value");
    ASSERT_EQ(elem0->get().id, 0u, "Incorrect element 0 id");

    auto elem5 = window[5];
    SIMPLE_ASSERT(elem5.has_value(), "Failed to access element 5");
    ASSERT_CLOSE(elem5->get().value, 7.5, "Incorrect element 5 value");
    ASSERT_EQ(elem5->get().id, 5u, "Incorrect element 5 id");

    elem5->get().value = 999.0;
    auto elem5_again = window[5];
    ASSERT_CLOSE(elem5_again->get().value, 999.0, "Element modification failed");

    window.close();
    return true;
}

TEST_CASE(window_shifting)
{
    const std::string filename = "test_shift.bin";
    const size_t element_count = 100;
    create_test_file(filename, element_count);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, DataPoint::serialized_size(), 10);
    SIMPLE_ASSERT(open_result.has_value(), "Failed to open file");

    size_t initial_begin = window.begin_index();
    ASSERT_EQ(initial_begin, 0u, "Initial begin should be 0");

    auto shift_result = window.shift_to_index(50);
    SIMPLE_ASSERT(shift_result.has_value(), "Window shift failed");

    SIMPLE_ASSERT(window.begin_index() <= 50, "Window should include index 50");
    SIMPLE_ASSERT(window.end_index() > 50, "Window should include index 50");

    auto elem50 = window[50];
    SIMPLE_ASSERT(elem50.has_value(), "Failed to access element after shift");
    ASSERT_EQ(elem50->get().id, 50u, "Incorrect element after shift");

    window.close();
    return true;
}

// =============================================================================
// II. Serialization Policy Tests
// =============================================================================

TEST_CASE(binary_serialization)
{
    const std::string filename = "test_binary.bin";
    const size_t element_count = 50;

    {
        std::ofstream file(filename, std::ios::binary);
        for (size_t i = 0; i < element_count; ++i)
        {
            int32_t value = static_cast<int32_t>(i * 10);
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    }
    TempFile cleanup(filename);

    fat_p::BinarySlidingWindow<SimpleInt> window;
    auto open_result = window.open(filename, sizeof(SimpleInt), 10);
    SIMPLE_ASSERT(open_result.has_value(), "Binary window open failed");

    auto elem0 = window[0];
    SIMPLE_ASSERT(elem0.has_value(), "Failed to access binary element");
    ASSERT_EQ(elem0->get().value, 0, "Incorrect binary element value");

    auto elem25 = window[25];
    SIMPLE_ASSERT(elem25.has_value(), "Failed to access element 25");
    ASSERT_EQ(elem25->get().value, 250, "Incorrect binary element at 25");

    window.close();
    return true;
}

// =============================================================================
// III. Large File Handling
// =============================================================================

TEST_CASE(large_file)
{
    const std::string filename = "test_large.bin";
    const size_t element_count = 10000;
    const size_t window_size = 100;

    create_large_test_file(filename, element_count);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, DataPoint::serialized_size(), window_size);
    SIMPLE_ASSERT(open_result.has_value(), "Large file open failed");

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<size_t> dist(0, element_count - 1);

    const size_t test_accesses = 100;
    for (size_t i = 0; i < test_accesses; ++i)
    {
        size_t index = dist(rng);
        auto elem = window[index];
        SIMPLE_ASSERT(elem.has_value(), "Failed random access in large file");
        ASSERT_EQ(elem->get().id, static_cast<uint32_t>(index), "Incorrect element in large file");
    }

    window.close();
    return true;
}

// =============================================================================
// IV. Error Handling Tests
// =============================================================================

TEST_CASE(error_handling)
{
    fat_p::SlidingFileWindow<DataPoint> window;

    auto elem = window[0];
    SIMPLE_ASSERT(!elem.has_value(), "Should fail to access closed window");
    SIMPLE_ASSERT(elem.error() == fat_p::FileError::FileNotOpen, "Should report FileNotOpen error");

    const std::string filename = "test_error.bin";
    create_test_file(filename, 10);
    TempFile cleanup(filename);

    auto open_result = window.open(filename, DataPoint::serialized_size(), 5);
    SIMPLE_ASSERT(open_result.has_value(), "Failed to open file");

    auto out_of_bounds = window[1000];
    SIMPLE_ASSERT(!out_of_bounds.has_value(), "Should fail out-of-bounds access");
    SIMPLE_ASSERT(out_of_bounds.error() == fat_p::FileError::InvalidIndex,
                  "Should report InvalidIndex");

    window.close();
    return true;
}

// =============================================================================
// V. Thread Safety Tests
// =============================================================================

TEST_CASE(thread_safety)
{
    const std::string filename = "test_threadsafe.bin";
    const size_t element_count = 1000;
    create_test_file(filename, element_count);
    TempFile cleanup(filename);

    fat_p::ThreadSafeSlidingWindow<DataPoint> window;
    auto open_result = window.open(filename, DataPoint::serialized_size(), 100);
    SIMPLE_ASSERT(open_result.has_value(), "Failed to open file");

    const size_t num_threads = 4;
    const size_t reads_per_thread = 100;
    std::atomic<size_t> error_count{0};

    auto reader = [&](size_t thread_id) {
        std::mt19937_64 rng(thread_id);
        std::uniform_int_distribution<size_t> dist(0, element_count - 1);

        for (size_t i = 0; i < reads_per_thread; ++i)
        {
            size_t index = dist(rng);
            auto elem = window[index];
            if (!elem || elem->get().id != static_cast<uint32_t>(index))
            {
                ++error_count;
            }
        }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(reader, i);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    ASSERT_EQ(error_count.load(), 0u, "Thread-safety violations detected");

    window.close();
    return true;
}

// =============================================================================
// VI. Lag Offset Tests
// =============================================================================

TEST_CASE(lag_offset)
{
    const std::string filename = "test_lag.bin";
    const size_t element_count = 100;
    create_test_file(filename, element_count);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, DataPoint::serialized_size(), 10, 20);

    SIMPLE_ASSERT(open_result.has_value(), "Lag offset open failed");
    ASSERT_EQ(window.begin_index(), 80u, "Incorrect lag offset start position");

    auto elem85 = window[85];
    SIMPLE_ASSERT(elem85.has_value(), "Failed to access lagged element");
    ASSERT_EQ(elem85->get().id, 85u, "Incorrect lagged element");

    window.close();
    return true;
}

// =============================================================================
// VII. Persistence Tests
// =============================================================================

TEST_CASE(persistence)
{
    const std::string filename = "test_persist.bin";
    const size_t element_count = 50;
    create_test_file(filename, element_count);
    TempFile cleanup(filename);

    {
        fat_p::SlidingFileWindow<DataPoint> window;
        auto open_result1 = window.open(filename, DataPoint::serialized_size(), 10);
        SIMPLE_ASSERT(open_result1.has_value(), "Failed to open file for writing");

        auto elem5 = window[5];
        elem5->get().value = 12345.67;
        elem5->get().timestamp = 999999;

        window.close();
    }

    {
        fat_p::SlidingFileWindow<DataPoint> window;
        auto open_result2 = window.open(filename, DataPoint::serialized_size(), 10);
        SIMPLE_ASSERT(open_result2.has_value(), "Failed to open file for reading");

        auto elem5 = window[5];
        ASSERT_CLOSE(elem5->get().value, 12345.67, "Modified value not persisted");
        ASSERT_EQ(elem5->get().timestamp, 999999u, "Modified timestamp not persisted");

        window.close();
    }

    return true;
}

// =============================================================================
// VIII. Edge Cases
// =============================================================================

TEST_CASE(empty_file)
{
    const std::string filename = "test_empty.bin";
    {
        std::ofstream empty_file(filename, std::ios::binary);
    }
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    auto result = window.open(filename, DataPoint::serialized_size(), 10);

    SIMPLE_ASSERT(result.has_value(), "Should handle empty file");
    ASSERT_EQ(window.size(), 0u, "Empty file should have size 0");
    SIMPLE_ASSERT(window.empty(), "Empty file should report empty");

    window.close();
    return true;
}

TEST_CASE(single_element)
{
    const std::string filename = "test_single.bin";
    create_test_file(filename, 1);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, DataPoint::serialized_size(), 10);
    SIMPLE_ASSERT(open_result.has_value(), "Failed to open file");

    ASSERT_EQ(window.size(), 1u, "Single element file size incorrect");

    auto elem = window[0];
    SIMPLE_ASSERT(elem.has_value(), "Should access single element");
    ASSERT_EQ(elem->get().id, 0u, "Single element has wrong data");

    window.close();
    return true;
}

TEST_CASE(window_larger_than_file)
{
    const std::string filename = "test_small.bin";
    create_test_file(filename, 5);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    auto open_result = window.open(filename, DataPoint::serialized_size(), 100);
    SIMPLE_ASSERT(open_result.has_value(), "Failed to open file");

    ASSERT_EQ(window.window_size(), 5u, "Window should be clamped to file size");

    window.close();
    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

void benchmark_slidingfilewindow()
{
    std::cout << "\n" << colors::cyan() << "SlidingFileWindow Benchmarks:" << colors::reset()
              << "\n\n";

    const std::string filename = "bench_sliding.bin";
    const size_t element_count = 100000;
    const size_t window_size = 1000;

    create_large_test_file(filename, element_count);
    TempFile cleanup(filename);

    fat_p::SlidingFileWindow<DataPoint> window;
    (void)window.open(filename, DataPoint::serialized_size(), window_size);

    const size_t iterations = 10000;

    double seq_time = measure_perf(
        [&]() {
            for (size_t i = 0; i < window_size; ++i)
            {
                auto elem = window[i];
                DoNotOptimize(elem);
            }
        },
        100,
        10);

    std::cout << "  Sequential in-window access: " << format_time(seq_time / window_size) << "/elem"
              << "\n";

    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<size_t> dist(0, element_count - 1);

    double rand_time = measure_perf(
        [&]() {
            size_t index = dist(rng);
            auto elem = window[index];
            DoNotOptimize(elem);
        },
        iterations,
        100);

    std::cout << "  Random access (with I/O):    " << format_time(rand_time) << "/access"
              << "\n";

    window.close();
}

} // namespace fat_p::testing::slidingfilewindow

namespace fat_p::testing
{

bool test_SlidingFileWindow()
{
    PRINT_HEADER(SLIDING FILE WINDOW)

    TestRunner runner;

    RUN_TEST_NS(runner, slidingfilewindow, basic_open_close);
    RUN_TEST_NS(runner, slidingfilewindow, element_access);
    RUN_TEST_NS(runner, slidingfilewindow, window_shifting);
    RUN_TEST_NS(runner, slidingfilewindow, binary_serialization);
    RUN_TEST_NS(runner, slidingfilewindow, large_file);
    RUN_TEST_NS(runner, slidingfilewindow, error_handling);
    RUN_TEST_NS(runner, slidingfilewindow, thread_safety);
    RUN_TEST_NS(runner, slidingfilewindow, lag_offset);
    RUN_TEST_NS(runner, slidingfilewindow, persistence);
    RUN_TEST_NS(runner, slidingfilewindow, empty_file);
    RUN_TEST_NS(runner, slidingfilewindow, single_element);
    RUN_TEST_NS(runner, slidingfilewindow, window_larger_than_file);

    slidingfilewindow::benchmark_slidingfilewindow();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SlidingFileWindow() ? 0 : 1;
}
#endif
