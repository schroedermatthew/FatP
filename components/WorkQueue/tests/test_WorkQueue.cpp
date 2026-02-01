/**
 * @file test_WorkQueue.cpp
 * @brief Comprehensive tests for WorkQueue.h
 */
/*
FATP_META:
  meta_version: 1
  component: WorkQueue
  file_role: test
  path: components/WorkQueue/tests/test_WorkQueue.cpp
  layer: Testing
  namespace: fat_p::testing::workqueue
  summary: "Unit tests for WorkQueue."
  api_stability: stable
  related:
    docs_search: "WorkQueue"
    headers:
      - include/fat_p/WorkQueue.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: chatgpt
    mode: authored
*/

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "WorkQueue.h"

namespace fat_p::testing::workqueue
{

// ============================================================================
// Basic Operations
// ============================================================================

FATP_TEST_CASE(work_queue_basic_operations)
{
    using Queue = fat_p::work_queue::WorkQueue<int, 8, 64>;
    Queue queue;

    FATP_ASSERT_TRUE(queue.empty(), "Queue should start empty");
    FATP_ASSERT_TRUE(queue.enqueue(42), "Should enqueue");

    int value = 0;
    FATP_ASSERT_TRUE(queue.dequeue(value), "Should dequeue");
    FATP_ASSERT_TRUE(value == 42, "Value should match");

    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");
    return true;
}

FATP_TEST_CASE(work_queue_capacity)
{
    using Queue = fat_p::work_queue::WorkQueue<int, 16, 32>;
    Queue queue;

    FATP_ASSERT_TRUE(queue.shard_count() == 16, "Shard count should match template parameter");
    FATP_ASSERT_TRUE(queue.shard_capacity() == 32,
                     "Shard capacity should match template parameter");
    FATP_ASSERT_TRUE(queue.capacity() == 16 * 32,
                     "Total capacity should be shard_count * shard_capacity");
    (void)queue;

    return true;
}

FATP_TEST_CASE(work_queue_full_and_empty)
{
    using Queue = fat_p::work_queue::WorkQueue<int, 8, 32>;
    Queue queue;

    const size_t totalCap = queue.capacity();

    for (size_t i = 0; i < totalCap; ++i)
    {
        FATP_ASSERT_TRUE(queue.enqueue(static_cast<int>(i)), "Should enqueue until full");
    }

    FATP_ASSERT_TRUE(!queue.enqueue(999), "Should fail enqueue when full");

    for (size_t i = 0; i < totalCap; ++i)
    {
        int value = -1;
        FATP_ASSERT_TRUE(queue.dequeue(value), "Should dequeue until empty");
        (void)value;
    }

    int value = 0;
    FATP_ASSERT_TRUE(!queue.dequeue(value), "Should fail dequeue when empty");
    return true;
}

// ============================================================================
// Concurrent Operations
// ============================================================================

FATP_TEST_CASE(work_queue_mpmc_no_loss_no_duplicates)
{
    // Small total capacity to force contention and frequent full conditions.
    using Queue = fat_p::work_queue::WorkQueue<uint32_t, 8, 256>;
    Queue queue;

    constexpr int kNumProducers = 8;
    constexpr int kNumConsumers = 8;
    constexpr uint32_t kItemsPerProducer = 50000;
    constexpr uint32_t kTotalItems = kItemsPerProducer * kNumProducers;

    std::unique_ptr<std::atomic<uint32_t>[]> seen(new std::atomic<uint32_t>[kTotalItems]);
    for (uint32_t i = 0; i < kTotalItems; ++i)
    {
        seen[i].store(0, std::memory_order_relaxed);
    }

    std::atomic<uint32_t> produced{0};
    std::atomic<uint32_t> consumed{0};

    std::atomic<bool> failed{false};
    std::atomic<uint32_t> firstBadValue{0};
    std::atomic<uint32_t> firstDuplicateValue{0};

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(kNumProducers + kNumConsumers));

    for (int p = 0; p < kNumProducers; ++p)
    {
        threads.emplace_back([&queue, &produced, &failed, p]() {
            auto tok = queue.makeProducerToken();
            const uint32_t base = static_cast<uint32_t>(p) * kItemsPerProducer;

            for (uint32_t i = 0; i < kItemsPerProducer; ++i)
            {
                if (failed.load(std::memory_order_relaxed))
                {
                    break;
                }

                const uint32_t value = base + i;
                while (!failed.load(std::memory_order_relaxed) &&
                       !queue.enqueue(tok, value))
                {
                    std::this_thread::yield();
                }

                if (!failed.load(std::memory_order_relaxed))
                {
                    produced.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (int c = 0; c < kNumConsumers; ++c)
    {
        threads.emplace_back([&queue,
                              &seen,
                              &consumed,
                              &failed,
                              &firstBadValue,
                              &firstDuplicateValue]() {
            auto tok = queue.makeConsumerToken();
            uint32_t value = 0;

            while (!failed.load(std::memory_order_relaxed) &&
                   consumed.load(std::memory_order_relaxed) < kTotalItems)
            {
                if (queue.dequeue(tok, value))
                {
                    if (value >= kTotalItems)
                    {
                        firstBadValue.store(value, std::memory_order_relaxed);
                        failed.store(true, std::memory_order_relaxed);
                        break;
                    }

                    const uint32_t prev = seen[value].fetch_add(1, std::memory_order_relaxed);
                    if (prev != 0)
                    {
                        firstDuplicateValue.store(value, std::memory_order_relaxed);
                        failed.store(true, std::memory_order_relaxed);
                        break;
                    }

                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(!failed.load(std::memory_order_relaxed),
                     "No out-of-range or duplicate dequeues should occur");

    FATP_ASSERT_TRUE(produced.load(std::memory_order_relaxed) == kTotalItems,
                     "All items should be produced");
    FATP_ASSERT_TRUE(consumed.load(std::memory_order_relaxed) == kTotalItems,
                     "All items should be consumed");

    for (uint32_t i = 0; i < kTotalItems; ++i)
    {
        const uint32_t count = seen[i].load(std::memory_order_relaxed);
        FATP_ASSERT_TRUE(count == 1, "Each value should be seen exactly once");
    }

    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");

    (void)firstBadValue;
    (void)firstDuplicateValue;

    return true;
}

} // namespace fat_p::testing::workqueue

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_WorkQueue()
{
    FATP_PRINT_HEADER(WORK QUEUE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, workqueue, work_queue_basic_operations);
    FATP_RUN_TEST_NS(runner, workqueue, work_queue_capacity);
    FATP_RUN_TEST_NS(runner, workqueue, work_queue_full_and_empty);
    FATP_RUN_TEST_NS(runner, workqueue, work_queue_mpmc_no_loss_no_duplicates);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_WorkQueue() ? 0 : 1;
}
#endif
