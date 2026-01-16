/**
 * @file ThreadPool.h
 * @brief Production-ready thread pool with work stealing, priority queues, and hybrid idle strategy
 *
 *
 *
 * @layer Concurrency
 *
 * @details High-performance thread pool implementation featuring:
 * - Work stealing for load balancing (mutex-protected deques)
 * - Priority-based task scheduling (global priority queue + local queues)
 * - Exception-safe task execution with diagnostics
 * - Graceful shutdown with pending task completion
 * - Low contention design with per-thread queues
 * - Hybrid idle strategy: spin then sleep
 * - Randomized victim selection (Fisher-Yates) for fair stealing
 *
 * @section performance Performance Characteristics
 * - Task submission: ~100-200ns (lock acquisition)
 * - Work stealing: O(N) worst case, amortized O(1) with shuffle
 * - Spin-wait reduces latency for bursty workloads
 * - Cache-line aligned queues prevent false sharing
 *
 * @section thread_safety Thread Safety
 * - All public methods are thread-safe
 * - Multiple threads may submit concurrently
 * - Internal synchronization via mutexes and atomics
 *
 * @version 2.0
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: ThreadPool
  file_role: public_header
  path: fat_p/ThreadPool.h
  namespace: fat_p
  layer: Concurrency
  summary: "Public header for ThreadPool."
  api_stability: in_work
  related:
    docs_search: "ThreadPool"
    tests:
      - tests/test_ThreadPool.cpp
  hygiene:
    pragma_once: true
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
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include "FatPConfig.h"
#include "FatPTypeTraits.h"

namespace fat_p
{

// ============================================================================
// Priority Enum
// ============================================================================

/**
 * @brief Task priority levels for scheduling
 *
 * Higher values execute before lower values.
 * High and Critical priority tasks go to global queue for immediate visibility.
 * Normal and Low priority tasks go to per-thread local queues.
 */
enum class Priority : int
{
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// ============================================================================
// Task Wrapper
// ============================================================================

/**
 * @brief Type-erased task wrapper with priority and ordering
 *
 * Tasks are ordered by priority (higher first), then by submission order (FIFO).
 * The ordering is designed for use with std::priority_queue (max-heap).
 */
class ThreadPoolTask
{
public:
    using TaskFunction = std::function<void()>;

    ThreadPoolTask() = default;

    ThreadPoolTask(TaskFunction func, Priority pri = Priority::Normal)
        : m_func(std::move(func))
        , m_priority(pri)
        , m_id(s_next_id.fetch_add(1, std::memory_order_relaxed))
    {
    }

    void execute() const
    {
        if (m_func)
        {
            m_func();
        }
    }

    Priority priority() const noexcept
    {
        return m_priority;
    }
    uint64_t id() const noexcept
    {
        return m_id;
    }

    /**
     * @brief Comparison for std::priority_queue (max-heap)
     *
     * Returns true if 'this' has LOWER effective priority than 'other'.
     * std::priority_queue places the "largest" element at top, so:
     * - Higher Priority enum value = higher priority = should be at top
     * - For equal priority: lower ID (older) = higher priority = should be at top
     */
    bool operator<(const ThreadPoolTask& other) const noexcept
    {
        if (m_priority != other.m_priority)
        {
            // Lower enum value means lower priority
            return static_cast<int>(m_priority) < static_cast<int>(other.m_priority);
        }
        // FIFO: older tasks (smaller ID) should come first
        // So newer tasks (larger ID) are "less than" older tasks
        return m_id > other.m_id;
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(m_func);
    }

private:
    TaskFunction m_func;
    Priority m_priority = Priority::Normal;
    uint64_t m_id = 0;

    static std::atomic<uint64_t> s_next_id;
};

inline std::atomic<uint64_t> ThreadPoolTask::s_next_id{0};

// ============================================================================
// Work Stealing Queue
// ============================================================================

/**
 * @brief Work stealing deque for per-thread task storage
 *
 * Uses mutex-based synchronization for correctness and simplicity.
 * Owner thread pushes/pops from bottom (LIFO for cache locality).
 * Thieves steal from top (FIFO to reduce contention with owner).
 *
 * @note Mutex-based design chosen over lock-free Chase-Lev for:
 * - Simplicity and debuggability
 * - Correctness guarantees
 * - Acceptable performance for most workloads
 */
class WorkStealingQueue
{
public:
    WorkStealingQueue() = default;

    // Non-copyable
    WorkStealingQueue(const WorkStealingQueue&) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

    // Movable (mutex requires explicit handling)
    WorkStealingQueue(WorkStealingQueue&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_tasks = std::move(other.m_tasks);
    }

    WorkStealingQueue& operator=(WorkStealingQueue&& other) noexcept
    {
        if (this != &other)
        {
            std::scoped_lock lock(m_mutex, other.m_mutex);
            m_tasks = std::move(other.m_tasks);
        }
        return *this;
    }

    /**
     * @brief Push task to bottom (owner operation, LIFO)
     */
    void push(ThreadPoolTask task)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push_back(std::move(task));
    }

    /**
     * @brief Pop task from bottom (owner operation, LIFO)
     * @return true if task was retrieved, false if queue empty
     */
    bool pop(ThreadPoolTask& task)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_tasks.empty())
        {
            return false;
        }
        task = std::move(m_tasks.back());
        m_tasks.pop_back();
        return true;
    }

    /**
     * @brief Steal task from top (thief operation, FIFO)
     *
     * Uses try_to_lock to avoid blocking if owner is active.
     * @return true if task was stolen, false if queue empty or locked
     */
    bool steal(ThreadPoolTask& task)
    {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock() || m_tasks.empty())
        {
            return false;
        }
        task = std::move(m_tasks.front());
        m_tasks.pop_front();
        return true;
    }

    /**
     * @brief Check if queue is empty
     * @note Requires lock, use sparingly in hot paths
     */
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.empty();
    }

    /**
     * @brief Get queue size
     * @note Requires lock, use sparingly
     */
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

private:
    std::deque<ThreadPoolTask> m_tasks;
    mutable std::mutex m_mutex;
};

// ============================================================================
// Cache-Line Alignment
// ============================================================================

/**
 * @brief Cache-line aligned wrapper for WorkStealingQueue
 * @details Prevents false sharing between adjacent queues in the vector
 */
struct alignas(FATP_CACHE_LINE_SIZE) AlignedQueue
{
    WorkStealingQueue queue;
};

// ============================================================================
// Thread Pool
// ============================================================================

/**
 * @brief Production-ready thread pool with work stealing and priority scheduling
 *
 * @section design Design Decisions
 *
 * **Priority Model:** Hybrid approach where High/Critical tasks go to a global
 * priority queue (immediate visibility to all workers), while Normal/Low tasks
 * go to per-thread local queues (better cache locality, work stealing balances).
 *
 * **Idle Strategy:** Two-phase approach:
 * 1. Spin-wait for configurable duration (low latency for bursty work)
 * 2. OS wait on condition variable (CPU-friendly for extended idle)
 *
 * **Work Stealing:** Fisher-Yates shuffle ensures every queue is checked exactly
 * once per steal attempt, preventing starvation of any queue.
 */
class ThreadPool
{
public:
    /**
     * @brief Construct thread pool
     * @param num_threads Number of worker threads (0 = hardware_concurrency)
     * @param spin_us Microseconds to spin before sleeping (default: 2000)
     */
    explicit ThreadPool(size_t num_threads = 0, size_t spin_us = 2000)
        : m_stop(false)
        , m_spin_duration(std::chrono::microseconds(spin_us))
    {
        if (num_threads == 0)
        {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0)
            {
                num_threads = 2;
            }
        }

        m_num_threads = num_threads;
        m_worker_queues.resize(num_threads);

        for (size_t i = 0; i < num_threads; ++i)
        {
            m_workers.emplace_back([this, i]() {
                worker_thread(i);
            });
        }
    }

    /**
     * @brief Destructor - initiates graceful shutdown
     */
    ~ThreadPool()
    {
        shutdown();
    }

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Submit a task and get a future for the result
     * @tparam F Callable type
     * @tparam Args Argument types
     * @param f Function to execute
     * @param args Arguments to pass to function
     * @return std::future with the result
     *
     * @note Uses lambda capture instead of std::bind to preserve reference semantics
     */
    template <typename F, typename... Args>
    [[nodiscard]] auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
    {
        return submit_priority(Priority::Normal, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief Submit a task with specific priority
     * @tparam F Callable type
     * @tparam Args Argument types
     * @param priority Task priority level
     * @param f Function to execute
     * @param args Arguments to pass to function
     * @return std::future with the result
     */
    template <typename F, typename... Args>
    [[nodiscard]] auto submit_priority(Priority priority, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        // Lambda capture preserves reference semantics (unlike std::bind)
        auto bound_task = [func = std::forward<F>(f),
                           args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable -> ReturnType {
            return std::apply(std::move(func), std::move(args_tuple));
        };

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::move(bound_task));
        std::future<ReturnType> result = task->get_future();

        ThreadPoolTask wrapped(
            [task]() {
                (*task)();
            },
            priority);

        enqueue_task(std::move(wrapped), priority, /*notify=*/true);

        return result;
    }

    /**
     * @brief Submit multiple tasks efficiently (single notification)
     * @param tasks Vector of functions to execute
     *
     * @note All tasks are submitted with Normal priority to global queue
     * @note Single notify_all at end avoids thundering herd
     */
    void submit_batch(const std::vector<std::function<void()>>& tasks)
    {
        if (tasks.empty())
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_global_mutex);
            for (const auto& func : tasks)
            {
                if (func)
                {
                    m_global_queue.emplace(func, Priority::Normal);
                    m_pending_tasks.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        m_global_cv.notify_all();
    }

    /**
     * @brief Get number of worker threads
     */
    size_t thread_count() const noexcept
    {
        return m_num_threads;
    }

    /**
     * @brief Get number of pending tasks (O(1) via atomic counter)
     */
    size_t pending_tasks() const noexcept
    {
        return m_pending_tasks.load(std::memory_order_acquire);
    }

    /**
     * @brief Get number of currently executing tasks
     */
    size_t active_tasks() const noexcept
    {
        return m_active_tasks.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if shutdown has been initiated
     *
     * Can be used to reject submissions during shutdown if desired:
     * @code
     * if (!pool.is_shutdown()) {
     *     pool.submit(...);
     * }
     * @endcode
     */
    bool is_shutdown() const noexcept
    {
        return m_stop.load(std::memory_order_acquire);
    }

    /**
     * @brief Get total number of unhandled task exceptions (diagnostics)
     */
    size_t exception_count() const noexcept
    {
        return m_exception_count.load(std::memory_order_acquire);
    }

    /**
     * @brief Wait until all tasks are finished and no tasks are running
     *
     * Uses dedicated condition variable to avoid TOCTOU race.
     * Does not prevent new submissions during wait.
     */
    void wait_idle()
    {
        std::unique_lock<std::mutex> lock(m_idle_mutex);
        m_idle_cv.wait(lock, [this]() {
            return m_pending_tasks.load(std::memory_order_acquire) == 0 &&
                   m_active_tasks.load(std::memory_order_acquire) == 0;
        });
    }

    /**
     * @brief Initiate graceful shutdown (idempotent)
     *
     * Waits for all currently executing tasks to complete.
     * Tasks in queues at shutdown time will be executed.
     *
     * @note Submissions during shutdown are NOT rejected - they will execute
     *       if workers haven't drained yet. For immediate rejection semantics,
     *       check is_shutdown() before submitting.
     */
    void shutdown()
    {
        bool expected = false;
        if (m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            // Wake all workers to check stop flag
            m_global_cv.notify_all();

            for (auto& worker : m_workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }

            // Notify any threads waiting on idle
            {
                std::lock_guard<std::mutex> lock(m_idle_mutex);
                m_idle_cv.notify_all();
            }
        }
    }

private:
    /**
     * @brief Internal task enqueue with optional notification
     */
    void enqueue_task(ThreadPoolTask task, Priority priority, bool notify)
    {
        m_pending_tasks.fetch_add(1, std::memory_order_relaxed);

        if (priority >= Priority::High)
        {
            // High/Critical goes to global queue for immediate visibility
            std::lock_guard<std::mutex> lock(m_global_mutex);
            m_global_queue.push(std::move(task));
            if (notify)
            {
                m_global_cv.notify_one();
            }
        }
        else
        {
            // Normal/Low goes to per-thread queue (round-robin)
            size_t idx = m_next_queue.fetch_add(1, std::memory_order_relaxed) % m_num_threads;
            m_worker_queues[idx].queue.push(std::move(task));
            if (notify)
            {
                std::lock_guard<std::mutex> lock(m_global_mutex);
                m_global_cv.notify_one();
            }
        }
    }

    /**
     * @brief Worker thread main loop
     *
     * Priority order:
     * 1. Local queue (best cache locality)
     * 2. Global priority queue (high-priority tasks)
     * 3. Steal from other threads (load balancing)
     *
     * Idle strategy:
     * 1. Spin for m_spin_duration (low latency)
     * 2. Sleep on condition variable (CPU-friendly)
     */
    void worker_thread(size_t thread_idx)
    {
        ThreadPoolTask task;

        while (true)
        {
            bool got_task = false;

            // 1. Try local queue first (best cache locality)
            if (m_worker_queues[thread_idx].queue.pop(task))
            {
                got_task = true;
            }
            // 2. Try global priority queue
            else if (try_pop_global(task))
            {
                got_task = true;
            }
            // 3. Try stealing from other threads
            else
            {
                got_task = try_steal(task, thread_idx);
            }

            if (got_task)
            {
                // Transition: pending -> active
                // CRITICAL: Increment active BEFORE decrementing pending to prevent
                // wait_idle() from seeing (pending==0 && active==0) while task is in-flight
                m_active_tasks.fetch_add(1, std::memory_order_relaxed);
                m_pending_tasks.fetch_sub(1, std::memory_order_relaxed);

                try
                {
                    task.execute();
                }
                catch (...)
                {
                    // Count exceptions for diagnostics; keep worker alive
                    // Note: User exceptions are captured by packaged_task,
                    // this catches wrapper/infrastructure exceptions
                    m_exception_count.fetch_add(1, std::memory_order_relaxed);
                }

                m_active_tasks.fetch_sub(1, std::memory_order_relaxed);

                // Signal idle waiters if pool may be idle
                if (m_pending_tasks.load(std::memory_order_acquire) == 0 &&
                    m_active_tasks.load(std::memory_order_acquire) == 0)
                {
                    std::lock_guard<std::mutex> lock(m_idle_mutex);
                    m_idle_cv.notify_all();
                }
            }
            else
            {
                // No work available
                if (m_stop.load(std::memory_order_acquire))
                {
                    break;
                }

                // PHASE 1: Spin-wait for low latency
                if (m_spin_duration.count() > 0)
                {
                    auto spin_start = std::chrono::steady_clock::now();
                    bool found_work = false;

                    while (std::chrono::steady_clock::now() - spin_start < m_spin_duration)
                    {
                        // Check without locking: local queue + atomic pending count
                        if (m_pending_tasks.load(std::memory_order_acquire) > 0 ||
                            m_stop.load(std::memory_order_acquire))
                        {
                            found_work = true;
                            break;
                        }
                        std::this_thread::yield();
                    }

                    if (found_work)
                    {
                        continue;
                    }
                }

                // PHASE 2: OS wait (CPU-friendly for extended idle)
                std::unique_lock<std::mutex> lock(m_global_mutex);
                m_global_cv.wait_for(lock, std::chrono::milliseconds(10), [this]() {
                    return m_stop.load(std::memory_order_acquire) ||
                           m_pending_tasks.load(std::memory_order_acquire) > 0;
                });
            }
        }
    }

    /**
     * @brief Try to pop from global priority queue
     *
     * Uses const_cast move pattern to avoid copying ThreadPoolTask.
     * Safe because we immediately pop() after moving.
     */
    bool try_pop_global(ThreadPoolTask& task)
    {
        std::lock_guard<std::mutex> lock(m_global_mutex);
        if (m_global_queue.empty())
        {
            return false;
        }

        // Move from top() - safe because we pop immediately
        // std::priority_queue::top() returns const&, but we own the container
        task = std::move(const_cast<ThreadPoolTask&>(m_global_queue.top()));
        m_global_queue.pop();
        return true;
    }

    /**
     * @brief Try to steal work from another thread
     *
     * Uses Fisher-Yates shuffle to ensure every queue is checked exactly once,
     * preventing starvation of any particular queue.
     */
    bool try_steal(ThreadPoolTask& task, size_t my_idx)
    {
        // Thread-local state for victim selection (no static - supports multiple pools)
        thread_local std::vector<size_t> victims;
        thread_local std::mt19937 rng(std::random_device{}() + static_cast<unsigned int>(std::hash<std::thread::id>{}(
                                                                   std::this_thread::get_id())));

        // Resize if thread pool size changed (supports multiple pools per thread)
        if (victims.size() != m_num_threads)
        {
            victims.resize(m_num_threads);
            std::iota(victims.begin(), victims.end(), 0);
        }

        // Fisher-Yates shuffle for random but exhaustive search
        std::shuffle(victims.begin(), victims.end(), rng);

        for (size_t victim_idx : victims)
        {
            if (victim_idx != my_idx && m_worker_queues[victim_idx].queue.steal(task))
            {
                return true;
            }
        }
        return false;
    }

    // ========================================================================
    // Member Variables
    // ========================================================================

    // Shutdown flag
    std::atomic<bool> m_stop;

    // Thread count (immutable after construction)
    size_t m_num_threads{0};

    // Round-robin counter for local queue assignment
    std::atomic<size_t> m_next_queue{0};

    // Task accounting (for wait_idle and diagnostics)
    std::atomic<size_t> m_pending_tasks{0};   // Tasks in queues
    std::atomic<size_t> m_active_tasks{0};    // Tasks executing
    std::atomic<size_t> m_exception_count{0}; // Unhandled exceptions

    // Spin duration before sleeping
    std::chrono::microseconds m_spin_duration;

    // Worker threads
    std::vector<std::thread> m_workers;

    // Per-thread work queues (cache-line aligned)
    std::vector<AlignedQueue> m_worker_queues;

    // Global priority queue for High/Critical tasks
    std::priority_queue<ThreadPoolTask> m_global_queue;
    mutable std::mutex m_global_mutex;
    std::condition_variable m_global_cv;

    // Dedicated CV for wait_idle (prevents TOCTOU race)
    std::mutex m_idle_mutex;
    std::condition_variable m_idle_cv;
};

// Type trait for generic thread pool detection
template <>
struct is_thread_pool<ThreadPool> : std::true_type
{
};

} // namespace fat_p
