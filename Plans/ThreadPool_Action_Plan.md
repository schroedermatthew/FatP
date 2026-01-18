# ThreadPool Action Plan

**Component:** ThreadPool  
**Current Status:** `api_stability: in_work`  
**Target Status:** `api_stability: candidate`  
**Estimated Effort:** ~1.5 hours  
**Risk Level:** Very Low (no breaking changes)

---

## Executive Summary

ThreadPool is already production-quality. Only minor additions needed:
- 5 `[[nodiscard]]` attributes
- 5 edge case tests

No bug fixes required. No API changes.

---

## Phase 1: Code Fixes (15 minutes)

### 1.1 Add `[[nodiscard]]` Attributes

**File:** `fat_p/ThreadPool.h`

```cpp
// Line 449
[[nodiscard]] size_t thread_count() const noexcept
{
    return m_num_threads;
}

// Line 457
[[nodiscard]] size_t pending_tasks() const noexcept
{
    return m_pending_tasks.load(std::memory_order_acquire);
}

// Line 465
[[nodiscard]] size_t active_tasks() const noexcept
{
    return m_active_tasks.load(std::memory_order_acquire);
}

// Line 480
[[nodiscard]] bool is_shutdown() const noexcept
{
    return m_stop.load(std::memory_order_acquire);
}

// Line 488
[[nodiscard]] size_t exception_count() const noexcept
{
    return m_exception_count.load(std::memory_order_acquire);
}
```

---

## Phase 2: Test Additions (1 hour)

### 2.1 Single-Threaded Pool Test

```cpp
FATP_TEST_CASE(single_thread_pool)
{
    ThreadPool pool(1);  // Single worker
    
    FATP_ASSERT_EQ(pool.thread_count(), 1u, "Should have 1 thread");
    
    std::atomic<int> counter{0};
    std::vector<std::future<int>> futures;
    
    // Submit 10 tasks that must execute sequentially
    for (int i = 0; i < 10; ++i)
    {
        futures.push_back(pool.submit([&counter, i]() {
            int expected = i;
            // In single-threaded pool, tasks execute in order
            counter.fetch_add(1, std::memory_order_relaxed);
            return i * 10;
        }));
    }
    
    // Verify all futures
    for (int i = 0; i < 10; ++i)
    {
        FATP_ASSERT_EQ(futures[i].get(), i * 10, "Future value should match");
    }
    
    FATP_ASSERT_EQ(counter.load(), 10, "All tasks should complete");
    
    return true;
}
```

### 2.2 Empty Batch Test

```cpp
FATP_TEST_CASE(empty_batch_submission)
{
    ThreadPool pool(4);
    
    // Submit empty batch - should be no-op
    std::vector<std::function<void()>> empty_tasks;
    pool.submit_batch(empty_tasks);
    
    FATP_ASSERT_EQ(pool.pending_tasks(), 0u, "Empty batch should not add tasks");
    
    // Pool should still work normally
    std::atomic<int> counter{0};
    (void)pool.submit([&counter]() {
        counter.fetch_add(1, std::memory_order_relaxed);
    });
    pool.wait_idle();
    
    FATP_ASSERT_EQ(counter.load(), 1, "Pool should work after empty batch");
    
    return true;
}
```

### 2.3 Batch with Null Functions Test

```cpp
FATP_TEST_CASE(batch_with_null_functions)
{
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    std::vector<std::function<void()>> tasks;
    
    // Mix of valid and null functions
    tasks.push_back([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    tasks.push_back(nullptr);  // Null function
    tasks.push_back([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    tasks.push_back(std::function<void()>{});  // Empty function
    tasks.push_back([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    
    pool.submit_batch(tasks);
    pool.wait_idle();
    
    // Only valid functions should execute
    FATP_ASSERT_EQ(counter.load(), 3, "Only valid functions should execute");
    
    return true;
}
```

### 2.4 Double Shutdown Test

```cpp
FATP_TEST_CASE(double_shutdown)
{
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    for (int i = 0; i < 10; ++i)
    {
        (void)pool.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    // First shutdown
    pool.shutdown();
    FATP_ASSERT_TRUE(pool.is_shutdown(), "Should be shutdown");
    FATP_ASSERT_EQ(counter.load(), 10, "All tasks should complete");
    
    // Second shutdown should be no-op
    pool.shutdown();
    FATP_ASSERT_TRUE(pool.is_shutdown(), "Should still be shutdown");
    
    // Destructor will call shutdown again - should be safe
    return true;
}
```

### 2.5 Long-Running Task During Shutdown Test

```cpp
FATP_TEST_CASE(shutdown_with_long_task)
{
    std::atomic<bool> task_started{false};
    std::atomic<bool> task_completed{false};
    
    {
        ThreadPool pool(2);
        
        // Submit a long-running task
        (void)pool.submit([&task_started, &task_completed]() {
            task_started.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            task_completed.store(true, std::memory_order_release);
        });
        
        // Wait for task to start
        while (!task_started.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        
        // Destructor will call shutdown - should wait for task
    }
    
    FATP_ASSERT_TRUE(task_completed.load(), "Shutdown should wait for running tasks");
    
    return true;
}
```

### 2.6 Multiple Exceptions Test

```cpp
FATP_TEST_CASE(multiple_exceptions)
{
    ThreadPool pool(4);
    
    std::vector<std::future<int>> futures;
    
    // Submit tasks that throw
    for (int i = 0; i < 5; ++i)
    {
        futures.push_back(pool.submit([i]() -> int {
            throw std::runtime_error("Error " + std::to_string(i));
        }));
    }
    
    // Submit tasks that succeed
    for (int i = 0; i < 5; ++i)
    {
        futures.push_back(pool.submit([i]() {
            return i * 100;
        }));
    }
    
    // Verify exceptions propagate correctly
    for (int i = 0; i < 5; ++i)
    {
        bool caught = false;
        try
        {
            futures[i].get();
        }
        catch (const std::runtime_error&)
        {
            caught = true;
        }
        FATP_ASSERT_TRUE(caught, "Exception should propagate");
    }
    
    // Verify successful tasks
    for (int i = 5; i < 10; ++i)
    {
        FATP_ASSERT_EQ(futures[i].get(), (i - 5) * 100, "Successful task should return");
    }
    
    // Pool should report exceptions
    FATP_ASSERT_EQ(pool.exception_count(), 0u, 
                   "Exception count tracks wrapper exceptions, not user exceptions");
    
    return true;
}
```

### 2.7 Update Test Runner

Add to `test_ThreadPool()`:

```cpp
FATP_RUN_TEST_NS(runner, thread_pool, single_thread_pool);
FATP_RUN_TEST_NS(runner, thread_pool, empty_batch_submission);
FATP_RUN_TEST_NS(runner, thread_pool, batch_with_null_functions);
FATP_RUN_TEST_NS(runner, thread_pool, double_shutdown);
FATP_RUN_TEST_NS(runner, thread_pool, shutdown_with_long_task);
FATP_RUN_TEST_NS(runner, thread_pool, multiple_exceptions);
```

---

## Phase 3: Optional Enhancements

### 3.1 Add `submit_detached()` (Optional, 30 minutes)

```cpp
/**
 * @brief Submit a fire-and-forget task (no future returned)
 * 
 * Use when you don't need the result. Avoids the overhead of
 * packaged_task and prevents "unused future" warnings.
 * 
 * @tparam F Callable type
 * @tparam Args Argument types
 * @param f Function to execute
 * @param args Arguments to pass
 */
template <typename F, typename... Args>
void submit_detached(F&& f, Args&&... args)
{
    submit_detached_priority(Priority::Normal, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
void submit_detached_priority(Priority priority, F&& f, Args&&... args)
{
    auto bound_task = [func = std::forward<F>(f),
                       args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        std::apply(std::move(func), std::move(args_tuple));
    };

    ThreadPoolTask wrapped(std::move(bound_task), priority);
    enqueue_task(std::move(wrapped), priority, /*notify=*/true);
}
```

### 3.2 Document Task Cancellation Pattern (Optional, 30 minutes)

Add to User Manual:

```markdown
## Task Cancellation Pattern

ThreadPool does not support built-in task cancellation. Use atomic flags:

```cpp
std::atomic<bool> cancelled{false};

auto future = pool.submit([&cancelled]() {
    for (int i = 0; i < 1000 && !cancelled.load(); ++i) {
        // Work...
    }
});

// To cancel:
cancelled.store(true);
```

For multiple tasks, use a shared cancellation token:

```cpp
struct CancellationToken {
    std::atomic<bool> cancelled{false};
    void cancel() { cancelled.store(true, std::memory_order_release); }
    bool is_cancelled() const { return cancelled.load(std::memory_order_acquire); }
};

auto token = std::make_shared<CancellationToken>();

for (int i = 0; i < 100; ++i) {
    pool.submit([token]() {
        if (token->is_cancelled()) return;
        // Work...
    });
}

// Cancel all:
token->cancel();
```
```

---

## Verification Plan

### Build and Test

```bash
cd /home/claude/FAT-P
g++ -std=c++17 -O2 -pthread -DENABLE_TEST_APPLICATION \
    -I fat_p tests/test_ThreadPool.cpp -o test_threadpool
./test_threadpool
```

### Expected Results

| Metric | Before | After |
|--------|--------|-------|
| Test cases | 12 | 18 |
| Assertions | ~40 | ~60 |
| Code coverage | ~85% | ~95% |

---

## Checklist

### Phase 1: Code Fixes (15m)

| Task | Effort | Status |
|------|--------|--------|
| Add `[[nodiscard]]` to `thread_count()` | 2m | ☐ |
| Add `[[nodiscard]]` to `pending_tasks()` | 2m | ☐ |
| Add `[[nodiscard]]` to `active_tasks()` | 2m | ☐ |
| Add `[[nodiscard]]` to `is_shutdown()` | 2m | ☐ |
| Add `[[nodiscard]]` to `exception_count()` | 2m | ☐ |

### Phase 2: Test Additions (1h)

| Task | Effort | Status |
|------|--------|--------|
| Add `single_thread_pool` test | 10m | ☐ |
| Add `empty_batch_submission` test | 10m | ☐ |
| Add `batch_with_null_functions` test | 10m | ☐ |
| Add `double_shutdown` test | 10m | ☐ |
| Add `shutdown_with_long_task` test | 10m | ☐ |
| Add `multiple_exceptions` test | 10m | ☐ |
| Update test runner | 5m | ☐ |
| Run all tests | 5m | ☐ |

### Phase 3: Optional (1h)

| Task | Effort | Status |
|------|--------|--------|
| Add `submit_detached()` method | 30m | ☐ |
| Document cancellation pattern | 30m | ☐ |

### Final Steps

| Task | Status |
|------|--------|
| Update FATP_META to `candidate` | ☐ |
| Run benchmark suite | ☐ |
| Verify documentation current | ☐ |

---

## Summary

| Phase | Effort | Priority |
|-------|--------|----------|
| Code fixes | 15m | Required |
| Test additions | 1h | Required |
| Optional enhancements | 1h | Nice to have |
| **Total (required)** | **~1.5h** | - |

ThreadPool is already excellent. These changes polish it for candidate status.
