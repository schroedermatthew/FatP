/**
 * @file test_CircularBuffer.cpp
 * @brief Comprehensive unit tests for CircularBuffer.h
 */
/*
FATP_META:
  meta_version: 1
  component: CircularBuffer
  file_role: test
  path: components/CircularBuffer/tests/test_CircularBuffer.cpp
  layer: Testing
  namespace: fat_p::testing::circularbuffer
  summary: "Unit tests for CircularBuffer."
  api_stability: candidate
  related:
    docs:
      - Documentation/IN WORK/CircularBuffer_Overview.md
      - Documentation/IN WORK/CircularBuffer_User_Manual.md
    headers:
      - include/fat_p/CircularBuffer.h
      - include/fat_p/FatPTest.h
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

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "CircularBuffer.h"
#include "FatPConcepts.h"
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

FATP_TEST_CASE(capacity_two)
{
    CircularBuffer<int, 2> buffer;

    FATP_ASSERT_TRUE(buffer.capacity() == 2, "Capacity should be 2");
    FATP_ASSERT_TRUE(buffer.empty(), "Should be empty");

    FATP_ASSERT_TRUE(buffer.push(11), "First push should succeed");
    FATP_ASSERT_TRUE(buffer.push(22), "Second push should succeed");
    FATP_ASSERT_TRUE(buffer.full(), "Should be full after two pushes");
    FATP_ASSERT_TRUE(!buffer.push(33), "Push to full capacity-2 buffer should fail");

    int val = 0;
    FATP_ASSERT_TRUE(buffer.pop(val), "First pop should succeed");
    FATP_ASSERT_TRUE(val == 11, "First pop should return 11");

    FATP_ASSERT_TRUE(buffer.pop(val), "Second pop should succeed");
    FATP_ASSERT_TRUE(val == 22, "Second pop should return 22");

    FATP_ASSERT_TRUE(buffer.empty(), "Should be empty after draining");

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

    FATP_ASSERT_TRUE(buffer.emplace(std::size_t(5), 'x'), "Emplace should succeed");

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

FATP_TEST_CASE(front_pointer_stability_until_pop)
{
    CircularBuffer<int, 8> buffer;
    FATP_ASSERT_TRUE(buffer.push(100), "Push should succeed");

    const int* first_ptr = buffer.front();
    FATP_ASSERT_TRUE(first_ptr != nullptr, "Front should not be nullptr");
    FATP_ASSERT_TRUE(*first_ptr == 100, "Front value should match");

    // Pushing additional items must not invalidate the consumer's front pointer.
    for (int i = 1; i < 8; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(100 + i), "Push should succeed");

        const int* p = buffer.front();
        FATP_ASSERT_TRUE(p == first_ptr, "Front pointer should remain stable until pop()");
        FATP_ASSERT_TRUE(*p == 100, "Front value should remain stable until pop()");
    }

    int val = 0;
    FATP_ASSERT_TRUE(buffer.pop(val), "Pop should succeed");
    FATP_ASSERT_TRUE(val == 100, "Popped value should match first element");

    const int* new_ptr = buffer.front();
    FATP_ASSERT_TRUE(new_ptr != nullptr, "Front should not be nullptr after pop");
    FATP_ASSERT_TRUE(*new_ptr == 101, "Front should advance after pop()");

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
    static_assert(concepts::circular_buffer_type<CircularBuffer<int, 8>>,
                  "CircularBuffer should satisfy circular_buffer_type concept");
    static_assert(!concepts::circular_buffer_type<std::vector<int>>,
                  "std::vector should not satisfy circular_buffer_type concept");
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
    FATP_ASSERT_TRUE(Buffer7::bufferSize() == 8, "Buffer size for capacity 7 should be 8");

    // Capacity 100: needs 101 slots minimum, next power of 2 is 128
    FATP_ASSERT_TRUE(Buffer100::bufferSize() == 128, "Buffer size for capacity 100 should be 128");

    // Verify at compile time that buffer sizes are powers of 2
    static_assert((Buffer7::bufferSize() & (Buffer7::bufferSize() - 1)) == 0, "Must be power of 2");
    static_assert((Buffer100::bufferSize() & (Buffer100::bufferSize() - 1)) == 0, "Must be power of 2");

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
        buffer.clearAndDestruct();
        FATP_ASSERT_TRUE(destruct_count > before, "clear_and_destruct should destruct elements");
        FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after clear_and_destruct");
    }

    return true;
}

FATP_TEST_CASE(clear_and_destruct_allows_reuse)
{
    CircularBuffer<std::string, 8> buffer;

    for (int i = 0; i < 5; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(std::to_string(i)), "Push should succeed");
    }

    buffer.clearAndDestruct();

    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after clear_and_destruct");
    FATP_ASSERT_TRUE(buffer.size() == 0, "Size should be 0 after clear_and_destruct");
    FATP_ASSERT_TRUE(buffer.front() == nullptr, "Front should be nullptr after clear_and_destruct");

    FATP_ASSERT_TRUE(buffer.push("ok"), "Push after clear_and_destruct should succeed");
    std::string s;
    FATP_ASSERT_TRUE(buffer.pop(s), "Pop after clear_and_destruct should succeed");
    FATP_ASSERT_TRUE(s == "ok", "Popped value should match");

    return true;
}

FATP_TEST_CASE(thread_safety_spsc)
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

    FATP_ASSERT_TRUE(!order_error.load(), "SPSC values must be in order");
    FATP_ASSERT_TRUE(consumer_received.load() == NUM_ITEMS, "All items should be received");
    FATP_ASSERT_TRUE(buffer.empty(), "Buffer should be empty after test");

    return true;
}

FATP_TEST_CASE(size_bounded_under_contention)
{
    constexpr int NUM_ITEMS = 200000;
    CircularBuffer<int, 1024> buffer;

    std::atomic<bool> start{false};
    std::atomic<bool> done{false};

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

        int received = 0;
        while (received < NUM_ITEMS)
        {
            int val = 0;
            if (buffer.pop(val))
            {
                ++received;
            }
        }

        done.store(true, std::memory_order_release);
    });

    start.store(true, std::memory_order_release);

    size_t max_size_seen = 0;
    size_t out_of_range_samples = 0;
    while (!done.load(std::memory_order_acquire))
    {
        const size_t s = buffer.size();
        if (s > buffer.capacity())
        {
            ++out_of_range_samples;
            break;
        }

        if (s > max_size_seen)
        {
            max_size_seen = s;
        }
    }

    producer.join();
    consumer.join();

    FATP_ASSERT_TRUE(out_of_range_samples == 0, "size() must be bounded by capacity()");
    FATP_ASSERT_TRUE(max_size_seen > 0, "Test should observe non-zero size during activity");

    return true;
}

// -----------------------------------------------------------------------------
// Test: size() returns semantically correct values under contention
//
// The original bug: the fallback path in size() computed
//     index_distance(read, write)  ==  (read - write) & MASK
// which is the FREE SPACE, not the element count. The value was in-range
// (0..Capacity), so the bounds-only check in size_bounded_under_contention
// could not catch it.
//
// This test maintains a separate atomic reference counter that tracks the
// true number of elements. The observer thread compares size() against
// the reference. Because both are approximate under contention, we allow
// a tolerance window. But the free-space bug produces errors proportional
// to Capacity (e.g., reporting 1020 when true size is 4), which easily
// exceeds any reasonable tolerance.
// -----------------------------------------------------------------------------
FATP_TEST_CASE(size_semantic_correctness)
{
    constexpr size_t CAP = 1024;
    constexpr int NUM_ITEMS = 500000;
    CircularBuffer<int, CAP> buffer;

    // Reference counter: incremented on push, decremented on pop.
    // This gives us the "true" size within a small race window.
    std::atomic<int> ref_count{0};

    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
    std::atomic<size_t> max_error_seen{0};
    std::atomic<size_t> semantic_violations{0};

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
            ref_count.fetch_add(1, std::memory_order_release);
        }
    });

    std::thread consumer([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        int received = 0;
        while (received < NUM_ITEMS)
        {
            int val = 0;
            if (buffer.pop(val))
            {
                ref_count.fetch_sub(1, std::memory_order_release);
                ++received;
            }
        }
        done.store(true, std::memory_order_release);
    });

    // Observer thread: repeatedly samples size() and ref_count, checking
    // that they are reasonably close.
    //
    // Methodology: bracket size() with two ref_count reads (before and
    // after) and take whichever is closer to the reported value. This
    // eliminates false positives from preemption between size() and a
    // single ref_count read — on constrained CI runners, hundreds of
    // items can flow through the queue during a scheduling gap, producing
    // measurement skew that exceeds CAP/2 even when size() is correct.
    //
    // The free-space bug produces errors proportional to Capacity
    // regardless of observation timing, so bracketing does not mask it.
    start.store(true, std::memory_order_release);

    size_t samples = 0;
    while (!done.load(std::memory_order_acquire))
    {
        int ref_before = ref_count.load(std::memory_order_acquire);
        size_t reported_size = buffer.size();
        int ref_after = ref_count.load(std::memory_order_acquire);

        // Use whichever ref_count snapshot is temporally closer to the
        // size() call, minimizing observation skew.
        size_t rb = static_cast<size_t>(std::max(ref_before, 0));
        size_t ra = static_cast<size_t>(std::max(ref_after, 0));

        size_t err_b = (reported_size > rb) ? (reported_size - rb) : (rb - reported_size);
        size_t err_a = (reported_size > ra) ? (reported_size - ra) : (ra - reported_size);
        size_t error = (err_b < err_a) ? err_b : err_a;

        // The free-space bug produces errors near Capacity (~1020 for a
        // 1024-element buffer). CAP/2 catches it with wide margin.
        if (error > CAP / 2)
        {
            semantic_violations.fetch_add(1, std::memory_order_relaxed);
        }

        // Track max error for diagnostics
        size_t prev_max = max_error_seen.load(std::memory_order_relaxed);
        while (error > prev_max &&
               !max_error_seen.compare_exchange_weak(prev_max, error,
                                                     std::memory_order_relaxed))
        {
        }

        ++samples;
    }

    producer.join();
    consumer.join();

    std::cout << colors::blue() << "  [INFO] Observer took " << samples << " samples, "
              << "max error: " << max_error_seen.load() << ", "
              << "violations: " << semantic_violations.load()
              << colors::reset() << std::endl;

    FATP_ASSERT_EQ(semantic_violations.load(), size_t(0),
                   "size() must not return free-space count (catastrophic semantic error)");
    FATP_ASSERT_TRUE(samples > 100,
                     "Observer should have taken enough samples to be meaningful");

    return true;
}

// -----------------------------------------------------------------------------
// Test: size() consistency during rapid fill/drain cycles
//
// Rapidly fills and drains the buffer while an observer checks size().
// This targets the specific scenario where write and read indices are
// sampled from different "moments" by size(), producing an inconsistent
// snapshot that triggers the fallback path.
// -----------------------------------------------------------------------------
FATP_TEST_CASE(size_fill_drain_consistency)
{
    constexpr size_t CAP = 64; // Small capacity = more wraparounds per second
    constexpr int CYCLES = 5000;
    CircularBuffer<int, CAP> buffer;

    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
    std::atomic<size_t> violations{0};

    // Producer/consumer in one thread: fill then drain repeatedly.
    // This creates maximum index churn.
    std::thread churner([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        for (int cycle = 0; cycle < CYCLES; ++cycle)
        {
            // Fill to capacity
            for (size_t i = 0; i < CAP; ++i)
            {
                (void)buffer.push(static_cast<int>(i));
            }
            // Drain completely
            int val = 0;
            while (buffer.pop(val))
            {
            }
        }
        done.store(true, std::memory_order_release);
    });

    // Observer: check size() bounds and sanity
    start.store(true, std::memory_order_release);

    size_t samples = 0;
    while (!done.load(std::memory_order_acquire))
    {
        size_t s = buffer.size();
        if (s > CAP)
        {
            violations.fetch_add(1, std::memory_order_relaxed);
        }
        ++samples;
    }

    churner.join();

    std::cout << colors::blue() << "  [INFO] Fill/drain observer: " << samples
              << " samples, violations: " << violations.load()
              << colors::reset() << std::endl;

    FATP_ASSERT_EQ(violations.load(), size_t(0),
                   "size() must never exceed capacity during fill/drain cycles");

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
} // namespace fat_p::testing::circularbuffer

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_CircularBuffer()
{
    FATP_PRINT_HEADER(CIRCULAR BUFFER)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, circularbuffer, basic_construction);
    FATP_RUN_TEST_NS(runner, circularbuffer, capacity_one);
    FATP_RUN_TEST_NS(runner, circularbuffer, capacity_two);
    FATP_RUN_TEST_NS(runner, circularbuffer, push_pop_basic);
    FATP_RUN_TEST_NS(runner, circularbuffer, full_condition);
    FATP_RUN_TEST_NS(runner, circularbuffer, empty_condition);
    FATP_RUN_TEST_NS(runner, circularbuffer, fifo_order);
    FATP_RUN_TEST_NS(runner, circularbuffer, wraparound);
    FATP_RUN_TEST_NS(runner, circularbuffer, move_semantics);
    FATP_RUN_TEST_NS(runner, circularbuffer, emplace);
    FATP_RUN_TEST_NS(runner, circularbuffer, front);
    FATP_RUN_TEST_NS(runner, circularbuffer, front_pointer_stability_until_pop);
    FATP_RUN_TEST_NS(runner, circularbuffer, clear);
    FATP_RUN_TEST_NS(runner, circularbuffer, clear_auto_destruct);
    FATP_RUN_TEST_NS(runner, circularbuffer, size_tracking);
    FATP_RUN_TEST_NS(runner, circularbuffer, type_trait);
    FATP_RUN_TEST_NS(runner, circularbuffer, static_capacity);
    FATP_RUN_TEST_NS(runner, circularbuffer, buffer_size_power_of_two);
    FATP_RUN_TEST_NS(runner, circularbuffer, clear_and_destruct);
    FATP_RUN_TEST_NS(runner, circularbuffer, clear_and_destruct_allows_reuse);
    FATP_RUN_TEST_NS(runner, circularbuffer, thread_safety_spsc);
    FATP_RUN_TEST_NS(runner, circularbuffer, size_bounded_under_contention);
    FATP_RUN_TEST_NS(runner, circularbuffer, size_semantic_correctness);
    FATP_RUN_TEST_NS(runner, circularbuffer, size_fill_drain_consistency);
    FATP_RUN_TEST_NS(runner, circularbuffer, stress_wraparound);


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
