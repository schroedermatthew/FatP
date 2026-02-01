#pragma once

/*
FATP_META:
  meta_version: 1
  component: WorkQueue
  file_role: public_header
  path: include/fat_p/WorkQueue.h
  namespace: fat_p::work_queue
  layer: Concurrency
  summary: "Sharded lock-free work queue (MPMC) with relaxed global ordering."
  api_stability: stable
  related:
    docs_search: "WorkQueue"
    tests:
      - components/WorkQueue/tests/test_WorkQueue.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file WorkQueue.h
 * @brief Sharded lock-free work queue (MPMC) with relaxed global ordering
 *
 * @layer Concurrency
 *
 * @details
 * A scalable work-queue optimized for many producers and many consumers.
 *
 * Core idea: remove the single hot enqueue/dequeue counters by sharding.
 * Each shard is a bounded lock-free MPMC queue. Threads prefer a local shard
 * and probe other shards when a shard is empty/full.
 *
 * Semantics:
 * - Exactly-once delivery for successful enqueues
 * - No global FIFO guarantee (work-queue semantics)
 * - Bounded total capacity (ShardCount * ShardCapacity)
 *
 * Thread-safety: Full MPMC lock-free (composed from lock-free shard cores)
 * Exception-safety: Strong guarantee (no-throw for trivially copyable T)
 */

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <utility>

#include "FatPConfig.h"
#include "LockFreeQueue.h"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace fat_p::work_queue
{

// ============================================================================
// Policies
// ============================================================================

/**
 * @brief Routing policy for shard selection and fallbacks
 */
struct DefaultRoutingPolicy
{
    static constexpr size_t kEnqueueProbeCount = 4;
    static constexpr size_t kDequeueProbeCount = 8;

    // If provided, these cap the scan length. 0 means "scan all".
    static constexpr size_t kEnqueueScanCount = 0;
    static constexpr size_t kDequeueScanCount = 0;

    static constexpr bool kScanAllOnEnqueueFail = true;
    static constexpr bool kScanAllOnDequeueFail = true;
};

/**
 * @brief Deterministic round-robin routing (useful for debugging; typically slower)
 *
 * For power-of-2 shard counts, this visits all shards sequentially.
 * Typically slower than random probing under contention due to thundering herd.
 */
struct RoundRobinRoutingPolicy : public DefaultRoutingPolicy
{
    static size_t enqueue_probe(size_t currentShard,
                                size_t attempt,
                                size_t shardCount,
                                uint32_t&) noexcept
    {
        return (currentShard + 1 + attempt) % shardCount;
    }

    static size_t dequeue_probe(size_t currentShard,
                                size_t attempt,
                                size_t shardCount,
                                uint32_t&) noexcept
    {
        return (currentShard + 1 + attempt) % shardCount;
    }

    static size_t enqueue_scan_start(size_t currentShard,
                                     size_t,
                                     uint32_t&) noexcept
    {
        return currentShard;
    }

    static size_t dequeue_scan_start(size_t currentShard,
                                     size_t,
                                     uint32_t&) noexcept
    {
        return currentShard;
    }
};

/**
 * @brief Deterministic stride routing (useful for experiments; typically slower)
 *
 * For power-of-2 shard counts, using an odd stride visits all shards.
 * Even strides with power-of-2 shard counts will only visit a subset.
 *
 * Recommended: Use odd stride values (1, 3, 5, 7, ...) for complete coverage.
 */
template <size_t Stride>
struct StrideRoutingPolicy : public DefaultRoutingPolicy
{
    static_assert(Stride > 0, "Stride must be positive");

    static size_t enqueue_probe(size_t currentShard,
                                size_t attempt,
                                size_t shardCount,
                                uint32_t&) noexcept
    {
        return (currentShard + (Stride * (attempt + 1))) % shardCount;
    }

    static size_t dequeue_probe(size_t currentShard,
                                size_t attempt,
                                size_t shardCount,
                                uint32_t&) noexcept
    {
        return (currentShard + (Stride * (attempt + 1))) % shardCount;
    }

    static size_t enqueue_scan_start(size_t currentShard,
                                     size_t,
                                     uint32_t&) noexcept
    {
        return currentShard;
    }

    static size_t dequeue_scan_start(size_t currentShard,
                                     size_t,
                                     uint32_t&) noexcept
    {
        return currentShard;
    }
};

/**
 * @brief Backoff policy used by tryDequeue helpers
 */
struct DefaultBackoffPolicy
{
    static void pause() noexcept
    {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_pause();
#elif defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
#elif defined(__arm__)
        __asm__ __volatile__("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

    static void yield() noexcept
    {
        std::this_thread::yield();
    }
};

// ============================================================================
// Internal Utilities
// ============================================================================

namespace detail
{

inline uint64_t mix64(uint64_t x) noexcept
{
    // splitmix64 finalizer
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x ^= (x >> 31);
    return x;
}

inline uint32_t xorshift32(uint32_t& state) noexcept
{
    uint32_t x = state;
    x ^= (x << 13);
    x ^= (x >> 17);
    x ^= (x << 5);
    state = x;
    if (state == 0)
    {
        state = 0x9e3779b9U;
    }
    return state;
}

inline uint64_t thread_hash64() noexcept
{
    const auto tid = std::this_thread::get_id();
    const size_t h = std::hash<std::thread::id>{}(tid);
    return mix64(static_cast<uint64_t>(h));
}

template <typename, typename = void>
struct has_enqueue_probe : std::false_type
{
};

template <typename T>
struct has_enqueue_probe<T,
                         std::void_t<decltype(T::enqueue_probe(size_t{},
                                                              size_t{},
                                                              size_t{},
                                                              std::declval<uint32_t&>()))>>
    : std::true_type
{
};

template <typename, typename = void>
struct has_dequeue_probe : std::false_type
{
};

template <typename T>
struct has_dequeue_probe<T,
                         std::void_t<decltype(T::dequeue_probe(size_t{},
                                                              size_t{},
                                                              size_t{},
                                                              std::declval<uint32_t&>()))>>
    : std::true_type
{
};

template <typename, typename = void>
struct has_enqueue_scan_start : std::false_type
{
};

template <typename T>
struct has_enqueue_scan_start<T,
                              std::void_t<decltype(T::enqueue_scan_start(size_t{},
                                                                        size_t{},
                                                                        std::declval<uint32_t&>()))>>
    : std::true_type
{
};

template <typename, typename = void>
struct has_dequeue_scan_start : std::false_type
{
};

template <typename T>
struct has_dequeue_scan_start<T,
                              std::void_t<decltype(T::dequeue_scan_start(size_t{},
                                                                        size_t{},
                                                                        std::declval<uint32_t&>()))>>
    : std::true_type
{
};

} // namespace detail

// ============================================================================
// WorkQueue
// ============================================================================

/**
 * @brief Sharded lock-free work queue (MPMC)
 *
 * @tparam T Element type (must be trivially copyable)
 * @tparam ShardCount Number of shards (independent bounded MPMC queues)
 * @tparam ShardCapacity Capacity per shard (must be power of 2)
 * @tparam RoutingPolicy Shard routing policy
 * @tparam BackoffPolicy Backoff policy for try helpers
 *
 * Complexity:
 * - enqueue/dequeue: O(1) expected, O(ShardCount) in the worst-case scan fallback
 */
template <
    typename T,
    size_t ShardCount = 16,
    size_t ShardCapacity = 1024,
    typename RoutingPolicy = DefaultRoutingPolicy,
    typename BackoffPolicy = DefaultBackoffPolicy>
class WorkQueue
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for lock-free operations");
    static_assert(ShardCount > 0,
                  "ShardCount must be positive");
    static_assert(ShardCapacity > 0,
                  "ShardCapacity must be positive");
    static_assert((ShardCapacity & (ShardCapacity - 1)) == 0,
                  "ShardCapacity must be a power of 2");
    static_assert(ShardCount <= (static_cast<size_t>(-1) / ShardCapacity),
                  "ShardCount * ShardCapacity overflows size_t");

public:
    using value_type = T;
    using size_type = size_t;

    static constexpr size_t kShardCount = ShardCount;
    static constexpr size_t kShardCapacity = ShardCapacity;
    static constexpr size_t kTotalCapacity = ShardCount * ShardCapacity;

    struct ProducerToken
    {
        size_t mShardIndex = 0;
        uint32_t mRngState = 0x9e3779b9U;
    };

    struct ConsumerToken
    {
        size_t mShardIndex = 0;
        uint32_t mRngState = 0x85ebca6bU;
    };

    WorkQueue() noexcept = default;
    ~WorkQueue() = default;

    WorkQueue(const WorkQueue&) = delete;
    WorkQueue& operator=(const WorkQueue&) = delete;
    WorkQueue(WorkQueue&&) = delete;
    WorkQueue& operator=(WorkQueue&&) = delete;

    [[nodiscard]] ProducerToken makeProducerToken() const noexcept
    {
        const uint64_t h = detail::thread_hash64();
        ProducerToken tok;
        tok.mShardIndex = static_cast<size_t>(h % ShardCount);
        tok.mRngState = static_cast<uint32_t>(h) | 1U;
        return tok;
    }

    [[nodiscard]] ConsumerToken makeConsumerToken() const noexcept
    {
        const uint64_t h = detail::thread_hash64();
        ConsumerToken tok;
        tok.mShardIndex = static_cast<size_t>((h >> 32) % ShardCount);
        tok.mRngState = static_cast<uint32_t>(h >> 32) | 1U;
        return tok;
    }

    [[nodiscard]] bool enqueue(const T& value) noexcept
    {
        ProducerToken& tok = producerTokenTls();
        return enqueue(tok, value);
    }

    [[nodiscard]] bool enqueue(T&& value) noexcept
    {
        ProducerToken& tok = producerTokenTls();
        return enqueue(tok, std::move(value));
    }

    [[nodiscard]] bool enqueue(ProducerToken& tok, const T& value) noexcept
    {
        return enqueueImpl(tok, value);
    }

    [[nodiscard]] bool enqueue(ProducerToken& tok, T&& value) noexcept
    {
        return enqueueImpl(tok, std::move(value));
    }

    [[nodiscard]] bool dequeue(T& value) noexcept
    {
        ConsumerToken& tok = consumerTokenTls();
        return dequeue(tok, value);
    }

    [[nodiscard]] bool dequeue(ConsumerToken& tok, T& value) noexcept
    {
        return dequeueImpl(tok, value);
    }

    [[nodiscard]] bool tryDequeue(T& value, size_t maxAttempts = 100) noexcept
    {
        ConsumerToken& tok = consumerTokenTls();
        return tryDequeue(tok, value, maxAttempts);
    }

    [[nodiscard]] bool tryDequeue(ConsumerToken& tok, T& value, size_t maxAttempts = 100) noexcept
    {
        for (size_t i = 0; i < maxAttempts; ++i)
        {
            if (dequeue(tok, value))
            {
                return true;
            }
            BackoffPolicy::pause();
        }
        return false;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        for (const auto& shard : mShards)
        {
            if (!shard.empty())
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] size_t size() const noexcept
    {
        size_t total = 0;
        for (const auto& shard : mShards)
        {
            total += shard.size();
        }
        return total;
    }

    [[nodiscard]] static constexpr size_t shard_count() noexcept
    {
        return ShardCount;
    }

    [[nodiscard]] static constexpr size_t shard_capacity() noexcept
    {
        return ShardCapacity;
    }

    [[nodiscard]] static constexpr size_t capacity() noexcept
    {
        return kTotalCapacity;
    }

private:
    using ShardQueue = fat_p::LockFreeQueue<T, ShardCapacity>;

    static ProducerToken& producerTokenTls() noexcept
    {
        thread_local ProducerToken tok = []() {
            const uint64_t h = detail::thread_hash64();
            ProducerToken t;
            t.mShardIndex = static_cast<size_t>(h % ShardCount);
            t.mRngState = static_cast<uint32_t>(h) | 1U;
            return t;
        }();
        return tok;
    }

    static ConsumerToken& consumerTokenTls() noexcept
    {
        thread_local ConsumerToken tok = []() {
            const uint64_t h = detail::thread_hash64();
            ConsumerToken t;
            t.mShardIndex = static_cast<size_t>((h >> 32) % ShardCount);
            t.mRngState = static_cast<uint32_t>(h >> 32) | 1U;
            return t;
        }();
        return tok;
    }

    [[nodiscard]] static size_t enqueueProbeShard(size_t current,
                                                   size_t attempt,
                                                   uint32_t& rng) noexcept
    {
        if constexpr (detail::has_enqueue_probe<RoutingPolicy>::value)
        {
            return RoutingPolicy::enqueue_probe(current, attempt, ShardCount, rng);
        }
        else
        {
            // Default: random probing via xorshift
            return detail::xorshift32(rng) % ShardCount;
        }
    }

    [[nodiscard]] static size_t dequeueProbeShard(size_t current,
                                                   size_t attempt,
                                                   uint32_t& rng) noexcept
    {
        if constexpr (detail::has_dequeue_probe<RoutingPolicy>::value)
        {
            return RoutingPolicy::dequeue_probe(current, attempt, ShardCount, rng);
        }
        else
        {
            // Default: random probing via xorshift
            return detail::xorshift32(rng) % ShardCount;
        }
    }

    [[nodiscard]] static size_t enqueueScanStart(size_t current, uint32_t& rng) noexcept
    {
        if constexpr (detail::has_enqueue_scan_start<RoutingPolicy>::value)
        {
            return RoutingPolicy::enqueue_scan_start(current, ShardCount, rng);
        }
        else
        {
            // Default: random start
            return detail::xorshift32(rng) % ShardCount;
        }
    }

    [[nodiscard]] static size_t dequeueScanStart(size_t current, uint32_t& rng) noexcept
    {
        if constexpr (detail::has_dequeue_scan_start<RoutingPolicy>::value)
        {
            return RoutingPolicy::dequeue_scan_start(current, ShardCount, rng);
        }
        else
        {
            // Default: random start
            return detail::xorshift32(rng) % ShardCount;
        }
    }

    static constexpr size_t effectiveEnqueueScanCount() noexcept
    {
        const size_t cap = RoutingPolicy::kEnqueueScanCount;
        if (cap == 0)
        {
            return ShardCount;
        }
        return (cap > ShardCount) ? ShardCount : cap;
    }

    static constexpr size_t effectiveDequeueScanCount() noexcept
    {
        const size_t cap = RoutingPolicy::kDequeueScanCount;
        if (cap == 0)
        {
            return ShardCount;
        }
        return (cap > ShardCount) ? ShardCount : cap;
    }

    template <typename U>
    [[nodiscard]] bool enqueueImpl(ProducerToken& tok, U&& value) noexcept
    {
        size_t shardIndex = tok.mShardIndex % ShardCount;

        // Try preferred shard first
        if (mShards[shardIndex].enqueue(std::forward<U>(value)))
        {
            tok.mShardIndex = shardIndex;
            return true;
        }

        // Probe phase: try random shards
        for (size_t i = 0; i < RoutingPolicy::kEnqueueProbeCount; ++i)
        {
            const size_t s = enqueueProbeShard(shardIndex, i, tok.mRngState);
            if (mShards[s].enqueue(std::forward<U>(value)))
            {
                tok.mShardIndex = s;
                return true;
            }
        }

        // Scan phase: full scan as last resort
        if constexpr (RoutingPolicy::kScanAllOnEnqueueFail)
        {
            const size_t start = enqueueScanStart(shardIndex, tok.mRngState);
            const size_t count = effectiveEnqueueScanCount();
            for (size_t i = 0; i < count; ++i)
            {
                const size_t s = (start + i) % ShardCount;
                if (mShards[s].enqueue(std::forward<U>(value)))
                {
                    tok.mShardIndex = s;
                    return true;
                }
            }
        }

        return false;
    }

    [[nodiscard]] bool dequeueImpl(ConsumerToken& tok, T& value) noexcept
    {
        size_t shardIndex = tok.mShardIndex % ShardCount;

        // Try preferred shard first
        if (mShards[shardIndex].dequeue(value))
        {
            tok.mShardIndex = shardIndex;
            return true;
        }

        // Probe phase: try random shards
        for (size_t i = 0; i < RoutingPolicy::kDequeueProbeCount; ++i)
        {
            const size_t s = dequeueProbeShard(shardIndex, i, tok.mRngState);
            if (mShards[s].dequeue(value))
            {
                tok.mShardIndex = s;
                return true;
            }
        }

        // Scan phase: full scan as last resort
        if constexpr (RoutingPolicy::kScanAllOnDequeueFail)
        {
            const size_t start = dequeueScanStart(shardIndex, tok.mRngState);
            const size_t count = effectiveDequeueScanCount();

            for (size_t i = 0; i < count; ++i)
            {
                const size_t s = (start + i) % ShardCount;
                if (mShards[s].dequeue(value))
                {
                    tok.mShardIndex = s;
                    return true;
                }
            }
        }

        return false;
    }

    std::array<ShardQueue, ShardCount> mShards{};
};

} // namespace fat_p::work_queue
