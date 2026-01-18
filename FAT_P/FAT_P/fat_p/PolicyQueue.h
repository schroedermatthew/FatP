/**
 * @file PolicyQueue.h
 * @brief Policy-selected lock-free queue variants (single or sharded)
 *
 * @layer Concurrency
 *
 * @details
 * PolicyQueue provides a small policy-based facade selecting between existing
 * Fat-P lock-free queue implementations.
 *
 * Supported topology policies:
 * - policy_queue::SingleTopology: wraps fat_p::LockFreeQueue (bounded MPMC,
 *   strict FIFO).
 * - policy_queue::ShardedTopology: wraps fat_p::work_queue::WorkQueue (bounded MPMC,
 *   work-queue semantics; no global FIFO; sharded for scaling).
 *
 * Semantics:
 * - Exactly-once delivery for successful enqueue/dequeue.
 * - Ordering depends on the topology policy.
 *
 * Progress:
 * - Lock-free (no claim-then-wait phases).
 *
 * Complexity:
 * - SingleTopology: O(1) expected per op.
 * - ShardedTopology: O(1) expected; O(shards) worst-case near full/empty.
 *
 * Thread-safety:
 * - Full MPMC lock-free.
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: PolicyQueue
  file_role: public_header
  path: fat_p/PolicyQueue.h
  namespace: fat_p
  layer: Concurrency
  summary: "Policy-selected lock-free queue variants (single or sharded)."
  api_stability: stable
  related:
    docs_search: "PolicyQueue"
    tests:
      - tests/test_WorkQueue.cpp
      - tests/test_LockFreeQueue.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include "LockFreeQueue.h"
#include "WorkQueue.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace fat_p
{

namespace policy_queue
{

template <size_t Capacity, bool EnableStats = false>
struct SingleTopology
{
    static constexpr size_t kCapacity = Capacity;
    static constexpr bool kEnableStats = EnableStats;
};

template <
    size_t ShardCount,
    size_t ShardCapacity,
    typename RoutingPolicy = work_queue::DefaultRoutingPolicy,
    typename BackoffPolicy = work_queue::DefaultBackoffPolicy>
struct ShardedTopology
{
    static constexpr size_t kShardCount = ShardCount;
    static constexpr size_t kShardCapacity = ShardCapacity;
    static constexpr size_t kCapacity = ShardCapacity * ShardCount;

    using routing_policy = RoutingPolicy;
    using backoff_policy = BackoffPolicy;
};

} // namespace policy_queue

// ============================================================================
// PolicyQueue primary template
// ============================================================================

template <typename T, typename TopologyPolicy>
class PolicyQueue;

// ============================================================================
// Single topology: wraps LockFreeQueue
// ============================================================================

template <typename T, size_t Capacity, bool EnableStats>
class PolicyQueue<T, policy_queue::SingleTopology<Capacity, EnableStats>>
{
public:
    using value_type = T;
    using size_type = size_t;

    static constexpr size_t kCapacity = Capacity;

    using queue_type = LockFreeQueue<T, Capacity, EnableStats>;
    using stats_type = typename queue_type::stats_type;

    struct ProducerToken
    {
    };

    struct ConsumerToken
    {
    };

    PolicyQueue() noexcept = default;
    ~PolicyQueue() = default;

    PolicyQueue(const PolicyQueue&) = delete;
    PolicyQueue& operator=(const PolicyQueue&) = delete;

    PolicyQueue(PolicyQueue&&) = delete;
    PolicyQueue& operator=(PolicyQueue&&) = delete;

    [[nodiscard]] ProducerToken makeProducerToken() const noexcept
    {
        return ProducerToken{};
    }

    [[nodiscard]] ConsumerToken makeConsumerToken() const noexcept
    {
        return ConsumerToken{};
    }

    [[nodiscard]] bool enqueue(const T& item) noexcept
    {
        return mQueue.enqueue(item);
    }

    [[nodiscard]] bool enqueue(T&& item) noexcept
    {
        return mQueue.enqueue(std::move(item));
    }

    [[nodiscard]] bool enqueue(const T& item, ProducerToken&) noexcept
    {
        return mQueue.enqueue(item);
    }

    [[nodiscard]] bool enqueue(T&& item, ProducerToken&) noexcept
    {
        return mQueue.enqueue(std::move(item));
    }

    [[nodiscard]] bool dequeue(T& item) noexcept
    {
        return mQueue.dequeue(item);
    }

    [[nodiscard]] bool dequeue(T& item, ConsumerToken&) noexcept
    {
        return mQueue.dequeue(item);
    }

    [[nodiscard]] bool tryDequeue(T& item, size_t maxAttempts = 100) noexcept
    {
        return mQueue.tryDequeue(item, maxAttempts);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return mQueue.empty();
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return mQueue.size();
    }

    [[nodiscard]] static constexpr size_t capacity() noexcept
    {
        return kCapacity;
    }

    template <bool E = EnableStats>
    [[nodiscard]] std::enable_if_t<E, stats_type> stats() const noexcept
    {
        return mQueue.stats();
    }

    template <bool E = EnableStats>
    std::enable_if_t<E> resetStats() noexcept
    {
        mQueue.resetStats();
    }

private:
    queue_type mQueue;
};

// ============================================================================
// Sharded topology: wraps WorkQueue
// ============================================================================

template <
    typename T,
    size_t ShardCount,
    size_t ShardCapacity,
    typename RoutingPolicy,
    typename BackoffPolicy>
class PolicyQueue<
    T,
    policy_queue::ShardedTopology<ShardCount, ShardCapacity, RoutingPolicy, BackoffPolicy>>
{
public:
    using value_type = T;
    using size_type = size_t;

    static constexpr size_t kShardCount = ShardCount;
    static constexpr size_t kShardCapacity = ShardCapacity;
    static constexpr size_t kCapacity = ShardCapacity * ShardCount;

    using QueueType = work_queue::WorkQueue<T, ShardCount, ShardCapacity, RoutingPolicy, BackoffPolicy>;
    using ProducerToken = typename QueueType::ProducerToken;
    using ConsumerToken = typename QueueType::ConsumerToken;

    PolicyQueue() noexcept = default;
    ~PolicyQueue() = default;

    PolicyQueue(const PolicyQueue&) = delete;
    PolicyQueue& operator=(const PolicyQueue&) = delete;

    PolicyQueue(PolicyQueue&&) = delete;
    PolicyQueue& operator=(PolicyQueue&&) = delete;

    [[nodiscard]] ProducerToken makeProducerToken() const noexcept
    {
        return mQueue.makeProducerToken();
    }

    [[nodiscard]] ConsumerToken makeConsumerToken() const noexcept
    {
        return mQueue.makeConsumerToken();
    }

    [[nodiscard]] bool enqueue(const T& item) noexcept
    {
        return mQueue.enqueue(item);
    }

    [[nodiscard]] bool enqueue(T&& item) noexcept
    {
        return mQueue.enqueue(std::move(item));
    }

    [[nodiscard]] bool enqueue(const T& item, ProducerToken& token) noexcept
    {
        return mQueue.enqueue(token, item);
    }

    [[nodiscard]] bool enqueue(T&& item, ProducerToken& token) noexcept
    {
        return mQueue.enqueue(token, std::move(item));
    }

    [[nodiscard]] bool dequeue(T& item) noexcept
    {
        return mQueue.dequeue(item);
    }

    [[nodiscard]] bool dequeue(T& item, ConsumerToken& token) noexcept
    {
        return mQueue.dequeue(token, item);
    }

    [[nodiscard]] bool tryDequeue(T& item, size_t maxAttempts = 100) noexcept
    {
        return mQueue.tryDequeue(item, maxAttempts);
    }

    [[nodiscard]] bool tryDequeue(T& item, ConsumerToken& token, size_t maxAttempts = 100) noexcept
    {
        return mQueue.tryDequeue(token, item, maxAttempts);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return mQueue.empty();
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return mQueue.size();
    }

    [[nodiscard]] static constexpr size_t shard_count() noexcept
    {
        return kShardCount;
    }

    [[nodiscard]] static constexpr size_t shard_capacity() noexcept
    {
        return kShardCapacity;
    }

    [[nodiscard]] static constexpr size_t capacity() noexcept
    {
        return kCapacity;
    }

private:
    QueueType mQueue;
};

// ============================================================================
// Type Traits
// ============================================================================

template <typename T, typename TopologyPolicy>
struct is_lock_free_queue<PolicyQueue<T, TopologyPolicy>> : std::true_type
{
};

} // namespace fat_p
