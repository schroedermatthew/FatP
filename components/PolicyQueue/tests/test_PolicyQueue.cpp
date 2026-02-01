/**
 * @file test_PolicyQueue.cpp
 * @brief Tests for PolicyQueue.h
 */
/*
FATP_META:
  meta_version: 1
  component: PolicyQueue
  file_role: test
  path: components/PolicyQueue/tests/test_PolicyQueue.cpp
  layer: Testing
  namespace: fat_p::testing::policyqueue
  summary: "Unit tests for PolicyQueue wrapper (single + sharded topologies)."
  api_stability: stable
  related:
    docs_search: "PolicyQueue"
    headers:
      - include/fat_p/PolicyQueue.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "PolicyQueue.h"

namespace fat_p::testing::policyqueue
{

// ============================================================================
// SingleTopology: strict FIFO (wrapping LockFreeQueue)
// ============================================================================

FATP_TEST_CASE(policy_queue_single_fifo_order)
{
    using Queue = fat_p::PolicyQueue<uint32_t, fat_p::policy_queue::SingleTopology<256>>;
    Queue queue;

    for (uint32_t i = 0; i < 100; ++i)
    {
        FATP_ASSERT_TRUE(queue.enqueue(i), "enqueue should succeed within capacity");
    }

    for (uint32_t i = 0; i < 100; ++i)
    {
        uint32_t value = 0;
        FATP_ASSERT_TRUE(queue.dequeue(value), "dequeue should succeed");
        FATP_ASSERT_TRUE(value == i, "FIFO ordering should hold for SingleTopology");
    }

    FATP_ASSERT_TRUE(queue.empty(), "Queue should be empty");
    return true;
}

// ============================================================================
// ShardedTopology: work-queue semantics (wrapping work_queue::WorkQueue)
// ============================================================================

FATP_TEST_CASE(policy_queue_sharded_mpmc_no_loss_no_duplicates)
{
    using Queue = fat_p::PolicyQueue<uint32_t, fat_p::policy_queue::ShardedTopology<8, 256>>;
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
                while (!failed.load(std::memory_order_relaxed) && !queue.enqueue(value, tok))
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
        threads.emplace_back([&queue, &seen, &consumed, &failed, &firstBadValue, &firstDuplicateValue]() {
            auto tok = queue.makeConsumerToken();
            uint32_t value = 0;

            while (!failed.load(std::memory_order_relaxed) && consumed.load(std::memory_order_relaxed) < kTotalItems)
            {
                if (queue.dequeue(value, tok))
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

    FATP_ASSERT_TRUE(!failed.load(std::memory_order_relaxed), "No out-of-range or duplicate dequeues should occur");

    FATP_ASSERT_TRUE(produced.load(std::memory_order_relaxed) == kTotalItems, "All items should be produced");
    FATP_ASSERT_TRUE(consumed.load(std::memory_order_relaxed) == kTotalItems, "All items should be consumed");

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

// ============================================================================
// Custom Routing Policies: Smoke Tests
// ============================================================================

FATP_TEST_CASE(policy_queue_roundrobin_routing_no_loss)
{
    using Queue =
        fat_p::PolicyQueue<uint32_t,
                           fat_p::policy_queue::ShardedTopology<4, 64, fat_p::work_queue::RoundRobinRoutingPolicy>>;

    Queue queue;
    constexpr uint32_t kCount = 200;

    for (uint32_t i = 0; i < kCount; ++i)
    {
        FATP_ASSERT_TRUE(queue.enqueue(i), "enqueue should succeed");
    }

    std::vector<uint32_t> seen(kCount, 0);
    uint32_t value = 0;
    uint32_t dequeued = 0;

    while (queue.dequeue(value))
    {
        FATP_ASSERT_TRUE(value < kCount, "value in range");
        seen[value]++;
        dequeued++;
    }

    FATP_ASSERT_TRUE(dequeued == kCount, "all items dequeued");

    for (uint32_t i = 0; i < kCount; ++i)
    {
        FATP_ASSERT_TRUE(seen[i] == 1, "each value seen exactly once");
    }

    FATP_ASSERT_TRUE(queue.empty(), "queue should be empty");
    return true;
}

FATP_TEST_CASE(policy_queue_stride3_routing_no_loss)
{
    using Queue =
        fat_p::PolicyQueue<uint32_t,
                           fat_p::policy_queue::ShardedTopology<4, 64, fat_p::work_queue::StrideRoutingPolicy<3>>>;

    Queue queue;
    constexpr uint32_t kCount = 200;

    for (uint32_t i = 0; i < kCount; ++i)
    {
        FATP_ASSERT_TRUE(queue.enqueue(i), "enqueue should succeed");
    }

    std::vector<uint32_t> seen(kCount, 0);
    uint32_t value = 0;
    uint32_t dequeued = 0;

    while (queue.dequeue(value))
    {
        FATP_ASSERT_TRUE(value < kCount, "value in range");
        seen[value]++;
        dequeued++;
    }

    FATP_ASSERT_TRUE(dequeued == kCount, "all items dequeued");

    for (uint32_t i = 0; i < kCount; ++i)
    {
        FATP_ASSERT_TRUE(seen[i] == 1, "each value seen exactly once");
    }

    FATP_ASSERT_TRUE(queue.empty(), "queue should be empty");
    return true;
}

} // namespace fat_p::testing::policyqueue

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_PolicyQueue()
{
    FATP_PRINT_HEADER(POLICY QUEUE)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, policyqueue, policy_queue_single_fifo_order);
    FATP_RUN_TEST_NS(runner, policyqueue, policy_queue_sharded_mpmc_no_loss_no_duplicates);
    FATP_RUN_TEST_NS(runner, policyqueue, policy_queue_roundrobin_routing_no_loss);
    FATP_RUN_TEST_NS(runner, policyqueue, policy_queue_stride3_routing_no_loss);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_PolicyQueue() ? 0 : 1;
}
#endif
