/**
 * @file test_CircularBuffer.cpp
 * @brief Comprehensive unit tests for CircularBuffer.h
 */
/*
FATP_META:
  meta_version: 1
  component: CircularBuffer
  file_role: test
  path: tests/test_CircularBuffer.cpp
  namespace: fat_p::testing::circularbuffer
  summary: "Unit tests for CircularBuffer."
  related:
    docs_search: "CircularBuffer"
    headers:
      - fat_p/CircularBuffer.h
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
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "CircularBuffer.h"
#include "FatPTest.h"

namespace fat_p::testing::circularbuffer
{

FATP_TEST_CASE(basic_construction)
{
    CircularBuffer<int, 16> buffer;

    FATP_ASSERT_TRUE(buffer.empty(), "New buffer should be empty");
    FATP_ASSERT_TRUE(!buffer.full(), "New buffer should not be full");
    FATP_ASSERT_TRUE(buffer.size() == 0, "Size should be 0");
    FATP_ASSERT_TRUE(buffer.capacity() == 16, "Capacity should be 16");

    return true;
}

FATP_TEST_CASE(capacity_one)
{
    CircularBuffer<int, 1> buffer;

    FATP_ASSERT_TRUE(buffer.capacity() == 1, "Capacity should be 1");
    FATP_ASSERT_TRUE(buffer.empty(), "Should be empty");

    FATP_ASSERT_TRUE(buffer.push(42), "Push to capacity-1 buffer should succeed");
    FATP_ASSERT_TRUE(buffer.full(), "Should be full after one push");
    FATP_ASSERT_TRUE(!buffer.push(99), "Push to full capacity-1 buffer should fail");

    int val = 0;
    FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
    FATP_ASSERT_TRUE(val == 42, "Popped value should be 42");
    FATP_ASSERT_TRUE(buffer.empty(), "Should be empty after pop");

    return true;
}

FATP_TEST_CASE(push_pop_basic)
{
    CircularBuffer<int, 16> buffer;

    FATP_ASSERT_TRUE(buffer.push(1), "First push should succeed");
    FATP_ASSERT_TRUE(buffer.push(2), "Second push should succeed");
    FATP_ASSERT_TRUE(buffer.push(3), "Third push should succeed");

    FATP_ASSERT_TRUE(buffer.size() == 3, "Size should be 3 after 3 pushes");

    int val = 0;
    FATP_ASSERT_TRUE(buffer.pop(val), "First pop should succeed");
    FATP_ASSERT_TRUE(val == 1, "First pop should return 1");

    FATP_ASSERT_TRUE(buffer.pop(val), "Second pop should succeed");
    FATP_ASSERT_TRUE(val == 2, "Second pop should return 2");

    FATP_ASSERT_TRUE(buffer.pop(val), "Third pop should succeed");
    FATP_ASSERT_TRUE(val == 3, "Third pop should return 3");

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty");
    FATP_ASSERT_TRUE(!buffer.pop(val), "Pop from empty buffer should fail");

    return true;
}

FATP_TEST_CASE(full_condition)
{
    CircularBuffer<int, 4> buffer;

    FATP_ASSERT_TRUE(buffer.push(1), "Push 1 should succeed");
    FATP_ASSERT_TRUE(buffer.push(2), "Push 2 should succeed");
    FATP_ASSERT_TRUE(buffer.push(3), "Push 3 should succeed");
    FATP_ASSERT_TRUE(buffer.push(4), "Push 4 should succeed");

    FATP_ASSERT_TRUE(buffer.full(), "Buffer should be full");
    FATP_ASSERT_TRUE(!buffer.push(5), "Push to full buffer should fail");

    int val = 0;
    FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
    FATP_ASSERT_TRUE(!buffer.full(), "Buffer should not be full after pop");
    FATP_ASSERT_TRUE(buffer.push(5), "Push after pop should succeed");

    return true;
}

FATP_TEST_CASE(empty_condition)
{
    CircularBuffer<int, 4> buffer;

    FATP_ASSERT_TRUE(buffer.empty(), "New buffer should be empty");

    int val = 0;
    FATP_ASSERT_TRUE(!buffer.pop(val), "Pop from empty buffer should fail");

    FATP_ASSERT_TRUE(buffer.push(1), "Push should succeed");
    FATP_ASSERT_TRUE(!buffer.empty(), "Buffer should not be empty");

    FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after pop");

    return true;
}

FATP_TEST_CASE(fifo_order)
{
    CircularBuffer<int, 8> buffer;

    for (int i = 0; i < 8; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(i * 10), "Push should succeed");
    }

    for (int i = 0; i < 8; ++i)
    {
        int val = 0;
        FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
        FATP_ASSERT_TRUE(val == i * 10, "Values should be in FIFO order");
    }

    return true;
}

FATP_TEST_CASE(wraparound)
{
    CircularBuffer<int, 4> buffer;

    // Fill and empty multiple times to test wraparound
    for (int round = 0; round < 5; ++round)
    {
        for (int i = 0; i < 4; ++i)
        {
            FATP_ASSERT_TRUE(buffer.push(round * 100 + i), "Push should succeed");
        }

        for (int i = 0; i < 4; ++i)
        {
            int val = 0;
            FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
            FATP_ASSERT_TRUE(val == round * 100 + i, "Value should match");
        }
    }

    return true;
}

FATP_TEST_CASE(move_semantics)
{
    CircularBuffer<std::string, 4> buffer;

    std::string original = "test string";
    FATP_ASSERT_TRUE(buffer.push(std::move(original)), "Push rvalue should succeed");

    std::string result;
    FATP_ASSERT_TRUE(buffer.pop(result), "Pop should succeed");
    FATP_ASSERT_TRUE(result == "test string", "String content should match");

    return true;
}

FATP_TEST_CASE(emplace)
{
    CircularBuffer<std::string, 4> buffer;

    FATP_ASSERT_TRUE(buffer.emplace(5, 'x'), "Emplace should succeed");

    std::string result;
    FATP_ASSERT_TRUE(buffer.pop(result), "Pop should succeed");
    FATP_ASSERT_TRUE(result == "xxxxx", "Emplaced string should be correct");

    return true;
}

FATP_TEST_CASE(front)
{
    CircularBuffer<int, 4> buffer;

    FATP_ASSERT_TRUE(buffer.front() == nullptr, "Front of empty buffer should be nullptr");

    FATP_ASSERT_TRUE(buffer.push(42), "Push should succeed");
    const int* frontPtr = buffer.front();
    FATP_ASSERT_TRUE(frontPtr != nullptr, "Front should not be nullptr");
    FATP_ASSERT_TRUE(*frontPtr == 42, "Front should point to first element");

    FATP_ASSERT_TRUE(buffer.push(99), "Second push should succeed");
    FATP_ASSERT_TRUE(*buffer.front() == 42, "Front should still be first element");

    int val = 0;
    FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
    FATP_ASSERT_TRUE(*buffer.front() == 99, "Front should now be second element");

    return true;
}

FATP_TEST_CASE(clear)
{
    CircularBuffer<int, 8> buffer;

    for (int i = 0; i < 5; ++i)
    {
        (void)buffer.push(i);
    }

    FATP_ASSERT_TRUE(buffer.size() == 5, "Size should be 5 before clear");

    buffer.clear();

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after clear");
    FATP_ASSERT_TRUE(buffer.size() == 0, "Size should be 0 after clear");

    FATP_ASSERT_TRUE(buffer.push(100), "Push after clear should succeed");

    int val = 0;
    FATP_ASSERT_TRUE(buffer.pop(val), "Pop after clear should succeed");
    FATP_ASSERT_TRUE(val == 100, "Popped value should be correct");

    return true;
}

FATP_TEST_CASE(clear_auto_destruct)
{
    // Test that clear() auto-destructs non-trivial types
    static int destruct_count = 0;

    struct NonTrivial
    {
        int value;
        NonTrivial()
            : value(0)
        {
        }
        NonTrivial(int v)
            : value(v)
        {
        }
        NonTrivial(const NonTrivial& other)
            : value(other.value)
        {
        }
        NonTrivial(NonTrivial&& other) noexcept
            : value(other.value)
        {
            other.value = -1;
        }
        NonTrivial& operator=(const NonTrivial& other)
        {
            value = other.value;
            return *this;
        }
        NonTrivial& operator=(NonTrivial&& other) noexcept
        {
            value = other.value;
            other.value = -1;
            return *this;
        }
        ~NonTrivial()
        {
            if (value >= 0)
            {
                ++destruct_count;
            }
        }
    };

    static_assert(!std::is_trivially_destructible_v<NonTrivial>, "NonTrivial should not be trivially destructible");

    destruct_count = 0;

    {
        CircularBuffer<NonTrivial, 8> buffer;
        for (int i = 0; i < 5; ++i)
        {
            (void)buffer.push(NonTrivial(i));
        }

        int before = destruct_count;
        buffer.clear(); // Should auto-destruct for non-trivial types

        FATP_ASSERT_TRUE(destruct_count > before, "clear() should destruct non-trivial elements");
        FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after clear");
    }

    return true;
}

FATP_TEST_CASE(size_tracking)
{
    CircularBuffer<int, 8> buffer;

    FATP_ASSERT_TRUE(buffer.size() == 0, "Initial size should be 0");

    for (int i = 1; i <= 5; ++i)
    {
        (void)buffer.push(i);
        FATP_ASSERT_TRUE(buffer.size() == static_cast<size_t>(i), "Size should increment after push");
    }

    for (int i = 4; i >= 0; --i)
    {
        int val = 0;
        (void)buffer.pop(val);
        FATP_ASSERT_TRUE(buffer.size() == static_cast<size_t>(i), "Size should decrement after pop");
    }

    return true;
}

FATP_TEST_CASE(type_trait)
{
    static_assert(is_circular_buffer_v<CircularBuffer<int, 8>>,
                  "CircularBuffer should satisfy is_circular_buffer trait");
    static_assert(!is_circular_buffer_v<std::vector<int>>, "std::vector should not satisfy is_circular_buffer trait");
    return true;
}

FATP_TEST_CASE(static_capacity)
{
    using Buffer16 = CircularBuffer<int, 16>;
    using Buffer1 = CircularBuffer<int, 1>;
    using Buffer1024 = CircularBuffer<int, 1024>;
    using Buffer32 = CircularBuffer<int, 32>;

    FATP_ASSERT_TRUE(Buffer16::capacity() == 16, "Static capacity should be 16");
    FATP_ASSERT_TRUE(Buffer1::capacity() == 1, "Static capacity should be 1");
    FATP_ASSERT_TRUE(Buffer1024::capacity() == 1024, "Static capacity should be 1024");

    static_assert(Buffer32::capacity() == 32, "Capacity should be constexpr");

    return true;
}

FATP_TEST_CASE(buffer_size_power_of_two)
{
    // Test various capacities and verify internal buffer is power of 2
    using Buffer7 = CircularBuffer<int, 7>;
    using Buffer100 = CircularBuffer<int, 100>;

    // Buffer size should be next power of 2 after Capacity + 1
    // Capacity 7: needs 8 slots minimum, next power of 2 is 8
    FATP_ASSERT_TRUE(Buffer7::buffer_size() == 8, "Buffer size for capacity 7 should be 8");

    // Capacity 100: needs 101 slots minimum, next power of 2 is 128
    FATP_ASSERT_TRUE(Buffer100::buffer_size() == 128, "Buffer size for capacity 100 should be 128");

    // Verify at compile time that buffer sizes are powers of 2
    static_assert((Buffer7::buffer_size() & (Buffer7::buffer_size() - 1)) == 0, "Must be power of 2");
    static_assert((Buffer100::buffer_size() & (Buffer100::buffer_size() - 1)) == 0, "Must be power of 2");

    return true;
}

FATP_TEST_CASE(clear_and_destruct)
{
    static int destruct_count = 0;

    struct Tracked
    {
        int value;
        Tracked()
            : value(0)
        {
        }
        Tracked(int v)
            : value(v)
        {
        }
        Tracked(const Tracked& other)
            : value(other.value)
        {
        }
        Tracked(Tracked&& other) noexcept
            : value(other.value)
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
        FATP_ASSERT_TRUE(destruct_count > before, "clear_and_destruct should destruct elements");
        FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after clear_and_destruct");
    }

    return true;
}

FATP_TEST_CASE(thread_safety_spsc)
{
    constexpr int NUM_ITEMS = 100000;
    CircularBuffer<int, 1024> buffer;

    std::atomic<bool> start{false};
    std::atomic<int> consumer_received{0};
    std::atomic<bool> order_error{false};

    std::thread producer(
        [&]()
        {
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

    std::thread consumer(
        [&]()
        {
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

    FATP_ASSERT_TRUE(!order_error.load(), "SPSC values must be in order");
    FATP_ASSERT_TRUE(consumer_received.load() == NUM_ITEMS, "All items should be received");
    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after test");

    return true;
}

FATP_TEST_CASE(stress_wraparound)
{
    constexpr int ITERATIONS = 10000;
    CircularBuffer<int, 7> buffer; // Small odd capacity to stress power-of-2 logic

    for (int i = 0; i < ITERATIONS; ++i)
    {
        // Push 5 items
        for (int j = 0; j < 5; ++j)
        {
            FATP_ASSERT_TRUE(buffer.push(i * 10 + j), "Push should succeed");
        }

        // Pop 3 items
        for (int j = 0; j < 3; ++j)
        {
            int val = 0;
            FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
            FATP_ASSERT_TRUE(val == i * 10 + j, "Value should match");
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
        [&buffer]()
        {
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
        [&buffer]()
        {
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
        [&buffer]()
        {
            size_t s = buffer.size();
            DoNotOptimize(s);
        },
        100000,
        1000);
    std::cout << "Size query: " << format_time(size_time) << "\n";

    double empty_time = measure_perf(
        [&buffer]()
        {
            bool e = buffer.empty();
            DoNotOptimize(e);
        },
        100000,
        1000);
    std::cout << "Empty check: " << format_time(empty_time) << "\n";

    double full_time = measure_perf(
        [&buffer]()
        {
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

        std::thread producer(
            [&]()
            {
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

        std::thread consumer(
            [&]()
            {
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
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) << (ops_per_sec / 1000000.0)
                  << " million ops/sec\n";
        std::cout << "Time for " << THROUGHPUT_ITEMS << " items: " << duration.count() << " us\n";
    }
}

} // namespace fat_p::testing::circularbuffer

namespace fat_p::testing
{

bool test_CircularBuffer()
{
    FATP_PRINT_HEADER(CIRCULAR BUFFER)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, circularbuffer, basic_construction);
    FATP_RUN_TEST_NS(runner, circularbuffer, capacity_one);
    FATP_RUN_TEST_NS(runner, circularbuffer, push_pop_basic);
    FATP_RUN_TEST_NS(runner, circularbuffer, full_condition);
    FATP_RUN_TEST_NS(runner, circularbuffer, empty_condition);
    FATP_RUN_TEST_NS(runner, circularbuffer, fifo_order);
    FATP_RUN_TEST_NS(runner, circularbuffer, wraparound);
    FATP_RUN_TEST_NS(runner, circularbuffer, move_semantics);
    FATP_RUN_TEST_NS(runner, circularbuffer, emplace);
    FATP_RUN_TEST_NS(runner, circularbuffer, front);
    FATP_RUN_TEST_NS(runner, circularbuffer, clear);
    FATP_RUN_TEST_NS(runner, circularbuffer, clear_auto_destruct);
    FATP_RUN_TEST_NS(runner, circularbuffer, size_tracking);
    FATP_RUN_TEST_NS(runner, circularbuffer, type_trait);
    FATP_RUN_TEST_NS(runner, circularbuffer, static_capacity);
    FATP_RUN_TEST_NS(runner, circularbuffer, buffer_size_power_of_two);
    FATP_RUN_TEST_NS(runner, circularbuffer, clear_and_destruct);
    FATP_RUN_TEST_NS(runner, circularbuffer, thread_safety_spsc);
    FATP_RUN_TEST_NS(runner, circularbuffer, stress_wraparound);

    circularbuffer::benchmark_circularbuffer();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_CircularBuffer() ? 0 : 1;
}
#endif
