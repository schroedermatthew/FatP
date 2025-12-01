#include <atomic>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "CircularBuffer.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_CircularBuffer.h"
#endif

namespace fat_p::testing
{

namespace
{

bool test_basic_construction()
{
    CircularBuffer<int, 16> buffer;

    SIMPLE_ASSERT(buffer.empty(), "New buffer should be empty");
    SIMPLE_ASSERT(!buffer.full(), "New buffer should not be full");
    SIMPLE_ASSERT(buffer.size() == 0, "Size should be 0");
    SIMPLE_ASSERT(buffer.capacity() == 16, "Capacity should be 16");

    return true;
}

bool test_capacity_one()
{
    CircularBuffer<int, 1> buffer;

    SIMPLE_ASSERT(buffer.capacity() == 1, "Capacity should be 1");
    SIMPLE_ASSERT(buffer.empty(), "Should be empty");

    SIMPLE_ASSERT(buffer.push(42), "Push to capacity-1 buffer should succeed");
    SIMPLE_ASSERT(buffer.full(), "Should be full after one push");
    SIMPLE_ASSERT(!buffer.push(99), "Push to full capacity-1 buffer should fail");

    int val = 0;
    SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
    SIMPLE_ASSERT(val == 42, "Popped value should be 42");
    SIMPLE_ASSERT(buffer.empty(), "Should be empty after pop");

    return true;
}

bool test_push_pop_basic()
{
    CircularBuffer<int, 16> buffer;

    SIMPLE_ASSERT(buffer.push(1), "First push should succeed");
    SIMPLE_ASSERT(buffer.push(2), "Second push should succeed");
    SIMPLE_ASSERT(buffer.push(3), "Third push should succeed");

    SIMPLE_ASSERT(buffer.size() == 3, "Size should be 3 after 3 pushes");

    int val = 0;
    SIMPLE_ASSERT(buffer.pop(val), "First pop should succeed");
    SIMPLE_ASSERT(val == 1, "First pop should return 1");

    SIMPLE_ASSERT(buffer.pop(val), "Second pop should succeed");
    SIMPLE_ASSERT(val == 2, "Second pop should return 2");

    SIMPLE_ASSERT(buffer.pop(val), "Third pop should succeed");
    SIMPLE_ASSERT(val == 3, "Third pop should return 3");

    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty");
    SIMPLE_ASSERT(!buffer.pop(val), "Pop from empty buffer should fail");

    return true;
}

bool test_full_condition()
{
    CircularBuffer<int, 4> buffer;

    SIMPLE_ASSERT(buffer.push(1), "Push 1 should succeed");
    SIMPLE_ASSERT(buffer.push(2), "Push 2 should succeed");
    SIMPLE_ASSERT(buffer.push(3), "Push 3 should succeed");
    SIMPLE_ASSERT(buffer.push(4), "Push 4 should succeed");

    SIMPLE_ASSERT(buffer.full(), "Buffer should be full");
    SIMPLE_ASSERT(!buffer.push(5), "Push to full buffer should fail");

    int val = 0;
    SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
    SIMPLE_ASSERT(!buffer.full(), "Buffer should not be full after pop");
    SIMPLE_ASSERT(buffer.push(5), "Push after pop should succeed");

    return true;
}

bool test_empty_condition()
{
    CircularBuffer<int, 4> buffer;

    SIMPLE_ASSERT(buffer.empty(), "New buffer should be empty");

    int val = 0;
    SIMPLE_ASSERT(!buffer.pop(val), "Pop from empty buffer should fail");

    SIMPLE_ASSERT(buffer.push(1), "Push should succeed");
    SIMPLE_ASSERT(!buffer.empty(), "Buffer should not be empty");

    SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after pop");

    return true;
}

bool test_fifo_order()
{
    CircularBuffer<int, 8> buffer;

    for (int i = 0; i < 8; ++i)
    {
        SIMPLE_ASSERT(buffer.push(i * 10), "Push should succeed");
    }

    for (int i = 0; i < 8; ++i)
    {
        int val = 0;
        SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
        SIMPLE_ASSERT(val == i * 10, "Values should be in FIFO order");
    }

    return true;
}

bool test_wraparound()
{
    CircularBuffer<int, 4> buffer;

    // Fill and empty multiple times to test wraparound
    for (int round = 0; round < 5; ++round)
    {
        for (int i = 0; i < 4; ++i)
        {
            SIMPLE_ASSERT(buffer.push(round * 100 + i), "Push should succeed");
        }

        for (int i = 0; i < 4; ++i)
        {
            int val = 0;
            SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
            SIMPLE_ASSERT(val == round * 100 + i, "Value should match");
        }
    }

    return true;
}

bool test_move_semantics()
{
    CircularBuffer<std::string, 4> buffer;

    std::string original = "test string";
    SIMPLE_ASSERT(buffer.push(std::move(original)), "Push rvalue should succeed");

    std::string result;
    SIMPLE_ASSERT(buffer.pop(result), "Pop should succeed");
    SIMPLE_ASSERT(result == "test string", "String content should match");

    return true;
}

bool test_emplace()
{
    CircularBuffer<std::string, 4> buffer;

    SIMPLE_ASSERT(buffer.emplace(5, 'x'), "Emplace should succeed");

    std::string result;
    SIMPLE_ASSERT(buffer.pop(result), "Pop should succeed");
    SIMPLE_ASSERT(result == "xxxxx", "Emplaced string should be correct");

    return true;
}

bool test_front()
{
    CircularBuffer<int, 4> buffer;

    SIMPLE_ASSERT(buffer.front() == nullptr, "Front of empty buffer should be nullptr");

    SIMPLE_ASSERT(buffer.push(42), "Push should succeed");
    const int* frontPtr = buffer.front();
    SIMPLE_ASSERT(frontPtr != nullptr, "Front should not be nullptr");
    SIMPLE_ASSERT(*frontPtr == 42, "Front should point to first element");

    SIMPLE_ASSERT(buffer.push(99), "Second push should succeed");
    SIMPLE_ASSERT(*buffer.front() == 42, "Front should still be first element");

    int val = 0;
    SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
    SIMPLE_ASSERT(*buffer.front() == 99, "Front should now be second element");

    return true;
}

bool test_clear()
{
    CircularBuffer<int, 8> buffer;

    for (int i = 0; i < 5; ++i)
    {
        (void)buffer.push(i);
    }

    SIMPLE_ASSERT(buffer.size() == 5, "Size should be 5 before clear");

    buffer.clear();

    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after clear");
    SIMPLE_ASSERT(buffer.size() == 0, "Size should be 0 after clear");

    SIMPLE_ASSERT(buffer.push(100), "Push after clear should succeed");

    int val = 0;
    SIMPLE_ASSERT(buffer.pop(val), "Pop after clear should succeed");
    SIMPLE_ASSERT(val == 100, "Popped value should be correct");

    return true;
}

bool test_clear_auto_destruct()
{
    // Test that clear() auto-destructs non-trivial types
    static int destruct_count = 0;

    struct NonTrivial
    {
        int value;
        NonTrivial() : value(0) {}
        NonTrivial(int v) : value(v) {}
        NonTrivial(const NonTrivial& other) : value(other.value) {}
        NonTrivial(NonTrivial&& other) noexcept : value(other.value) { other.value = -1; }
        NonTrivial& operator=(const NonTrivial& other) { value = other.value; return *this; }
        NonTrivial& operator=(NonTrivial&& other) noexcept { value = other.value; other.value = -1; return *this; }
        ~NonTrivial() { if (value >= 0) ++destruct_count; }
    };

    static_assert(!std::is_trivially_destructible_v<NonTrivial>,
                  "NonTrivial should not be trivially destructible");

    destruct_count = 0;

    {
        CircularBuffer<NonTrivial, 8> buffer;
        for (int i = 0; i < 5; ++i)
        {
            (void)buffer.push(NonTrivial(i));
        }

        int before = destruct_count;
        buffer.clear();  // Should auto-destruct for non-trivial types

        SIMPLE_ASSERT(destruct_count > before, "clear() should destruct non-trivial elements");
        SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after clear");
    }

    return true;
}

bool test_size_tracking()
{
    CircularBuffer<int, 8> buffer;

    SIMPLE_ASSERT(buffer.size() == 0, "Initial size should be 0");

    for (int i = 1; i <= 5; ++i)
    {
        (void)buffer.push(i);
        SIMPLE_ASSERT(buffer.size() == static_cast<size_t>(i),
                      "Size should increment after push");
    }

    for (int i = 4; i >= 0; --i)
    {
        int val = 0;
        (void)buffer.pop(val);
        SIMPLE_ASSERT(buffer.size() == static_cast<size_t>(i),
                      "Size should decrement after pop");
    }

    return true;
}

bool test_type_trait()
{
    static_assert(is_circular_buffer_v<CircularBuffer<int, 8>>,
                  "CircularBuffer should satisfy is_circular_buffer trait");
    static_assert(!is_circular_buffer_v<std::vector<int>>,
                  "std::vector should not satisfy is_circular_buffer trait");
    return true;
}

bool test_static_capacity()
{
    using Buffer16 = CircularBuffer<int, 16>;
    using Buffer1 = CircularBuffer<int, 1>;
    using Buffer1024 = CircularBuffer<int, 1024>;
    using Buffer32 = CircularBuffer<int, 32>;

    SIMPLE_ASSERT(Buffer16::capacity() == 16, "Static capacity should be 16");
    SIMPLE_ASSERT(Buffer1::capacity() == 1, "Static capacity should be 1");
    SIMPLE_ASSERT(Buffer1024::capacity() == 1024, "Static capacity should be 1024");

    static_assert(Buffer32::capacity() == 32, "Capacity should be constexpr");

    return true;
}

bool test_buffer_size_power_of_two()
{
    // Test various capacities and verify internal buffer is power of 2
    using Buffer7 = CircularBuffer<int, 7>;
    using Buffer100 = CircularBuffer<int, 100>;

    // Buffer size should be next power of 2 after Capacity + 1
    // Capacity 7: needs 8 slots minimum, next power of 2 is 8
    SIMPLE_ASSERT(Buffer7::buffer_size() == 8, "Buffer size for capacity 7 should be 8");

    // Capacity 100: needs 101 slots minimum, next power of 2 is 128
    SIMPLE_ASSERT(Buffer100::buffer_size() == 128, "Buffer size for capacity 100 should be 128");

    // Verify at compile time that buffer sizes are powers of 2
    static_assert((Buffer7::buffer_size() & (Buffer7::buffer_size() - 1)) == 0, "Must be power of 2");
    static_assert((Buffer100::buffer_size() & (Buffer100::buffer_size() - 1)) == 0, "Must be power of 2");

    return true;
}

bool test_clear_and_destruct()
{
    static int destruct_count = 0;

    struct Tracked
    {
        int value;
        Tracked() : value(0)
        {
        }
        Tracked(int v) : value(v)
        {
        }
        Tracked(const Tracked& other) : value(other.value)
        {
        }
        Tracked(Tracked&& other) noexcept : value(other.value)
        {
            other.value = -1;
        }
        Tracked& operator=(const Tracked& other)
        {
            value = other.value;
            return *this;
        }
        Tracked& operator=(Tracked&& other) noexcept
        {
            value = other.value;
            other.value = -1;
            return *this;
        }
        ~Tracked()
        {
            if (value >= 0)
            {
                ++destruct_count;
            }
        }
    };

    destruct_count = 0;

    {
        CircularBuffer<Tracked, 8> buffer;
        for (int i = 0; i < 5; ++i)
        {
            (void)buffer.push(Tracked(i));
        }

        int before = destruct_count;
        buffer.clear_and_destruct();
        SIMPLE_ASSERT(destruct_count > before, "clear_and_destruct should destruct elements");
        SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after clear_and_destruct");
    }

    return true;
}

bool test_thread_safety_spsc()
{
    constexpr int NUM_ITEMS = 100000;
    CircularBuffer<int, 1024> buffer;

    std::atomic<bool> start{false};
    std::atomic<int> consumer_received{0};
    std::atomic<bool> order_error{false};

    std::thread producer([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        for (int i = 0; i < NUM_ITEMS; ++i)
        {
            while (!buffer.push(i))
            {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        int expected = 0;
        while (expected < NUM_ITEMS)
        {
            int val = 0;
            if (buffer.pop(val))
            {
                if (val != expected)
                {
                    order_error.store(true, std::memory_order_release);
                }
                ++expected;
            }
        }
        consumer_received.store(expected, std::memory_order_release);
    });

    start.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    SIMPLE_ASSERT(!order_error.load(), "SPSC values must be in order");
    SIMPLE_ASSERT(consumer_received.load() == NUM_ITEMS, "All items should be received");
    SIMPLE_ASSERT(buffer.empty(), "Buffer should be empty after test");

    return true;
}

bool test_stress_wraparound()
{
    constexpr int ITERATIONS = 10000;
    CircularBuffer<int, 7> buffer;  // Small odd capacity to stress power-of-2 logic

    for (int i = 0; i < ITERATIONS; ++i)
    {
        // Push 5 items
        for (int j = 0; j < 5; ++j)
        {
            SIMPLE_ASSERT(buffer.push(i * 10 + j), "Push should succeed");
        }

        // Pop 3 items
        for (int j = 0; j < 3; ++j)
        {
            int val = 0;
            SIMPLE_ASSERT(buffer.pop(val), "Pop should succeed");
            SIMPLE_ASSERT(val == i * 10 + j, "Value should match");
        }

        // Push 2 more (should hit capacity limit around here)
        (void)buffer.push(i * 10 + 5);
        (void)buffer.push(i * 10 + 6);

        // Drain remaining
        while (!buffer.empty())
        {
            int val = 0;
            (void)buffer.pop(val);
        }
    }

    return true;
}

void benchmark_circularbuffer()
{
    std::cout << "\n" << colors::cyan() << "CircularBuffer Benchmarks:" << colors::reset() << "\n";
    std::cout << "(Single-threaded, uncontended operations)\n";
    std::cout << "Note: Run multiple times for stable results; variance is normal.\n\n";

    constexpr size_t BUFFER_SIZE = 1024;
    CircularBuffer<int, BUFFER_SIZE> buffer;

    double push_time = measure_perf(
        [&buffer]() {
            static int i = 0;
            if (buffer.full())
            {
                int val = 0;
                (void)buffer.pop(val);
            }
            (void)buffer.push(i++);
        },
        100000,
        1000);
    std::cout << "Push (with overflow handling): " << format_time(push_time) << "\n";

    buffer.clear();
    for (size_t i = 0; i < BUFFER_SIZE / 2; ++i)
    {
        (void)buffer.push(static_cast<int>(i));
    }

    double pop_time = measure_perf(
        [&buffer]() {
            static int refill = 0;
            int val = 0;
            if (buffer.pop(val))
            {
                (void)buffer.push(refill++);
            }
        },
        100000,
        1000);
    std::cout << "Pop + Push (steady state): " << format_time(pop_time) << "\n";

    buffer.clear();
    for (size_t i = 0; i < BUFFER_SIZE / 2; ++i)
    {
        (void)buffer.push(static_cast<int>(i));
    }

    double size_time = measure_perf(
        [&buffer]() {
            size_t s = buffer.size();
            DoNotOptimize(s);
        },
        100000,
        1000);
    std::cout << "Size query: " << format_time(size_time) << "\n";

    double empty_time = measure_perf(
        [&buffer]() {
            bool e = buffer.empty();
            DoNotOptimize(e);
        },
        100000,
        1000);
    std::cout << "Empty check: " << format_time(empty_time) << "\n";

    double full_time = measure_perf(
        [&buffer]() {
            bool f = buffer.full();
            DoNotOptimize(f);
        },
        100000,
        1000);
    std::cout << "Full check: " << format_time(full_time) << "\n";

    // SPSC Throughput test
    std::cout << "\n" << colors::cyan() << "SPSC Throughput Test:" << colors::reset() << "\n";

    constexpr int THROUGHPUT_ITEMS = 1000000;

    {
        CircularBuffer<int, 4096> throughput_buffer;
        std::atomic<bool> start{false};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (int i = 0; i < THROUGHPUT_ITEMS; ++i)
            {
                while (!throughput_buffer.push(i))
                {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            int received = 0;
            while (received < THROUGHPUT_ITEMS)
            {
                int val = 0;
                if (throughput_buffer.pop(val))
                {
                    ++received;
                }
            }
        });

        start.store(true, std::memory_order_release);
        auto start_time = std::chrono::high_resolution_clock::now();

        producer.join();
        consumer.join();

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        double ops_per_sec = (THROUGHPUT_ITEMS * 1000000.0) / duration.count();
        std::cout << "Throughput: " << std::fixed << std::setprecision(2)
                  << (ops_per_sec / 1000000.0) << " million ops/sec\n";
        std::cout << "Time for " << THROUGHPUT_ITEMS << " items: " << duration.count() << " us\n";
    }
}

} // anonymous namespace

bool test_CircularBuffer()
{
    PRINT_HEADER(CIRCULAR BUFFER)

    TestRunner runner;

    RUN_TEST(runner, basic_construction);
    RUN_TEST(runner, capacity_one);
    RUN_TEST(runner, push_pop_basic);
    RUN_TEST(runner, full_condition);
    RUN_TEST(runner, empty_condition);
    RUN_TEST(runner, fifo_order);
    RUN_TEST(runner, wraparound);
    RUN_TEST(runner, move_semantics);
    RUN_TEST(runner, emplace);
    RUN_TEST(runner, front);
    RUN_TEST(runner, clear);
    RUN_TEST(runner, clear_auto_destruct);
    RUN_TEST(runner, size_tracking);
    RUN_TEST(runner, type_trait);
    RUN_TEST(runner, static_capacity);
    RUN_TEST(runner, buffer_size_power_of_two);
    RUN_TEST(runner, clear_and_destruct);
    RUN_TEST(runner, thread_safety_spsc);
    RUN_TEST(runner, stress_wraparound);

    benchmark_circularbuffer();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CircularBuffer() ? 0 : 1;
}
#endif
