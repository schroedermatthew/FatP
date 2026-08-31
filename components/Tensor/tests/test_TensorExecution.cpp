/*
FATP_META:
  meta_version: 1
  component: TensorExecution
  file_role: test
  path: components/Tensor/tests/test_TensorExecution.cpp
  namespace: fat_p::testing::tensor_execution
  layer: Testing
  summary: "Execution defaults, determinism, cancellation, draining, nesting, and allocator contracts."
  api_stability: in_work
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  related:
    headers:
      - include/fat_p/TensorExecution.h
*/

#include "FatPTest.h"
#include "TensorExecution.h"

#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory_resource>
#include <new>
#include <string>
#include <thread>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
// Caller-thread ordinary-new fault injection only. Do not interpose global new in the
// aggregate test executable, checked-iterator builds, or unmodified-new sanitizer runs.
namespace fat_p::testing::tensor_execution::allocation_probe
{
thread_local std::ptrdiff_t failIndex = -1;
void* allocate(std::size_t bytes)
{
    if (failIndex >= 0 && failIndex-- == 0)
    {
        failIndex = -1;
        throw std::bad_alloc();
    }
    if (auto* storage = std::malloc(bytes == 0 ? 1 : bytes))
    {
        return storage;
    }
    throw std::bad_alloc();
}
class Failure
{
public:
    explicit Failure(std::ptrdiff_t index) noexcept
    {
        failIndex = index;
    }
    ~Failure()
    {
        failIndex = -1;
    }
    Failure(const Failure&) = delete;
    Failure& operator=(const Failure&) = delete;
};
} // namespace fat_p::testing::tensor_execution::allocation_probe
void* operator new(std::size_t bytes)
{
    return fat_p::testing::tensor_execution::allocation_probe::allocate(bytes);
}
void* operator new[](std::size_t bytes)
{
    return fat_p::testing::tensor_execution::allocation_probe::allocate(bytes);
}
void operator delete(void* storage) noexcept
{
    std::free(storage);
}
void operator delete[](void* storage) noexcept
{
    std::free(storage);
}
void operator delete(void* storage, std::size_t) noexcept
{
    std::free(storage);
}
void operator delete[](void* storage, std::size_t) noexcept
{
    std::free(storage);
}
#endif

namespace fat_p::testing::tensor_execution
{

template <typename Exception, typename F>
bool throwsAs(F&& action)
{
    try
    {
        action();
    }
    catch (const Exception&)
    {
        return true;
    }
    return false;
}

class ScratchProbe : public std::pmr::memory_resource
{
public:
    std::size_t allocations = 0;
    std::size_t live = 0;
    bool fail = false;
    bool workerAllocation = false;
    std::stop_source* stopOnAllocate = nullptr;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        workerAllocation |= ThreadPool::isAnyPoolWorkerThread();
        ++allocations;
        if (fail)
        {
            throw std::bad_alloc();
        }
        auto* memory = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++live;
        if (stopOnAllocate)
        {
            stopOnAllocate->request_stop();
        }
        return memory;
    }
    void do_deallocate(void* memory, std::size_t bytes, std::size_t alignment) override
    {
        --live;
        std::pmr::new_delete_resource()->deallocate(memory, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
        return this == &other;
    }
};

TensorExecutionOptions forcedOptions()
{
    TensorExecutionOptions options;
    options.grainSize = 1;
    options.minimumWork = 0;
    return options;
}

template <ReadableTensor A, ReadableTensor B>
bool exactValues(const A& a, const B& b)
{
    if (a.extents() != b.extents())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if constexpr (std::same_as<typename A::value_type, double>)
        {
            if (std::bit_cast<std::uint64_t>(a[i]) != std::bit_cast<std::uint64_t>(b[i]))
            {
                return false;
            }
        }
        else if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}

FATP_TEST_CASE(serial_defaults_and_thresholds)
{
    ThreadPool pool(4, 0);
    ScratchProbe scratch;
    auto options = forcedOptions();
    options.scratch = &scratch;
    Tensor<int> left({9, 7}, 2), right({7, 5}, 3);
    const auto expected = matmul(left, right);
    FATP_ASSERT_TRUE(TensorExecutionContext{}.pool() == nullptr, "Default context must be serial");
    FATP_ASSERT_TRUE(exactValues(expected, matmul(left, right, TensorExecutionContext::serial(options))),
                     "Explicit serial matches no-context call");
    options.minimumWork = std::numeric_limits<std::size_t>::max();
    FATP_ASSERT_TRUE(exactValues(expected, matmul(left, right, TensorExecutionContext::parallel(pool, options))),
                     "Small work falls back to serial");
    options.minimumWork = 0;
    options.maxTasks = 1;
    (void)matmul(left, right, TensorExecutionContext::parallel(pool, options));
    FATP_ASSERT_EQ(scratch.allocations, std::size_t{0}, "Serial fallbacks allocate no scheduler scratch");
    options.maxTasks = 2;
    FATP_ASSERT_TRUE(exactValues(expected, matmul(left, right, TensorExecutionContext::parallel(pool, options))),
                     "Opt-in pool produces the same owner");
    FATP_ASSERT_EQ(scratch.allocations, std::size_t{1}, "One bounded future array per parallel call");
    FATP_ASSERT_EQ(scratch.live, std::size_t{0}, "Scheduler storage is released before return");
    FATP_ASSERT_TRUE(!scratch.workerAllocation, "Scratch allocations happen on the calling thread");
    using tensor_detail::tensorWorkReachesThreshold;
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    FATP_ASSERT_TRUE(tensorWorkReachesThreshold(maximum, maximum, maximum), "Work estimate cannot overflow");
    FATP_ASSERT_TRUE(!tensorWorkReachesThreshold(2, 3, 7), "Ceiling threshold comparison");
    FATP_ASSERT_TRUE(tensorWorkReachesThreshold(2, 3, 6), "Exact threshold schedules");
    return true;
}

FATP_TEST_CASE(layouts_and_deterministic_folds)
{
    ThreadPool pool(4, 0);
    auto options = forcedOptions();
    for (std::size_t grain : {std::size_t{1}, std::size_t{3}, std::size_t{32}})
    {
        options.grainSize = grain;
        auto context = TensorExecutionContext::parallel(pool, options);
        std::stop_source liveStop;
        auto cancellableOptions = options;
        cancellableOptions.cancellation = liveStop.get_token();
        const auto cancellable = TensorExecutionContext::parallel(pool, cancellableOptions);
        Tensor<double> left({35, 37}), right({37, 11});
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            left[i] = static_cast<double>(static_cast<int>(i % 19) - 9) / 7.0;
        }
        for (std::size_t i = 0; i < right.size(); ++i)
        {
            right[i] = static_cast<double>(static_cast<int>(i % 13) - 6) / 11.0;
        }
        for (int repeat = 0; repeat < 4; ++repeat)
        {
            FATP_ASSERT_TRUE(exactValues(matmul(left, right), matmul(left, right, context)),
                             "Parallel contiguous fold matches every result bit");
            FATP_ASSERT_TRUE(exactValues(matmul(left, right), matmul(left, right, cancellable)),
                             "Cancellation tile boundaries do not change floating folds");
            const auto reversed = left.sliceView({Slice{34, -36, -1}, All});
            FATP_ASSERT_TRUE(exactValues(matmul(reversed, right), matmul(reversed, right, context)),
                             "Negative-stride row ranges preserve logical order");
            FATP_ASSERT_TRUE(
                exactValues(matmul(left.transposeView(), left), matmul(left.transposeView(), left, context)),
                "Transposed inputs use the signed-stride kernel");
        }
        Tensor<int> batches({17, 1, 3}, 2), broadcast({1, 3, 4}, 3);
        FATP_ASSERT_TRUE(exactValues(matmul(batches, broadcast), matmul(batches, broadcast, context)),
                         "Batches with one row partition over batch-times-rows");
        Tensor<int> vector({3}, 4);
        FATP_ASSERT_TRUE(exactValues(matmul(vector, broadcast), matmul(vector, broadcast, context)),
                         "Vector/matrix shape squeeze stays intact");
        FATP_ASSERT_TRUE(exactValues(matmul(batches, vector), matmul(batches, vector, context)),
                         "Batched matrix/vector form stays intact");
        const auto scalar = dot(vector, vector, context);
        FATP_ASSERT_EQ(scalar(), std::int64_t{48}, "Dot's single fold is serial");
    }
    return true;
}

FATP_TEST_CASE(validation_empty_and_overflow)
{
    ThreadPool pool(4, 0);
    auto options = forcedOptions();
    options.grainSize = 0;
    FATP_ASSERT_TRUE(throwsAs<std::invalid_argument>([&] {
                         (void)TensorExecutionContext::parallel(pool, options);
                     }),
                     "Zero grain rejected");
    options = forcedOptions();
    options.scratch = nullptr;
    FATP_ASSERT_TRUE(throwsAs<std::invalid_argument>([&] {
                         (void)TensorExecutionContext::serial(options);
                     }),
                     "Null scratch rejected");
    auto context = TensorExecutionContext::parallel(pool, forcedOptions());
    Tensor<int> empty({0, 3}), right({3, 5}, 2), zeroInner({17, 0}), zeroRight({0, 5});
    FATP_ASSERT_TRUE(matmul(empty, right, context).empty(), "Empty outputs do not schedule");
    const auto zeros = matmul(zeroInner, zeroRight, context);
    FATP_ASSERT_TRUE(zeros.size() == 85 && zeros[84] == 0, "Zero inner dimensions sum to zero");
    FATP_ASSERT_TRUE(throwsAs<std::invalid_argument>([&] {
                         (void)dot(right, right, context);
                     }),
                     "Dot validates rank");
    Tensor<std::int64_t> huge({17, 2}, std::numeric_limits<std::int64_t>::max());
    Tensor<std::int64_t> multiplier({2, 2}, 2);
    FATP_ASSERT_TRUE(throwsAs<std::overflow_error>([&] {
                         (void)matmul(huge, multiplier, context);
                     }),
                     "Parallel checked products retain overflow errors");
    pool.wait_idle();
    FATP_ASSERT_EQ(pool.pending_tasks(), std::size_t{0}, "Overflow leaves no queued work");
    return true;
}

FATP_TEST_CASE(cancellation_and_scratch_failure)
{
    ThreadPool pool(4, 0);
    Tensor<int> left({17, 3}, 2), right({3, 4}, 3);
    std::stop_source stop;
    auto options = forcedOptions();
    options.cancellation = stop.get_token();
    ScratchProbe scratch;
    options.scratch = &scratch;
    stop.request_stop();
    auto context = TensorExecutionContext::parallel(pool, options);
    FATP_ASSERT_TRUE(throwsAs<TensorExecutionCancelled>([&] {
                         (void)matmul(left, right, context);
                     }),
                     "Pre-cancelled call is rejected before scheduler allocation");
    FATP_ASSERT_EQ(scratch.allocations, std::size_t{0}, "No scratch for pre-cancellation");
    FATP_ASSERT_TRUE(throwsAs<std::invalid_argument>([&] {
                         (void)matmul(left, left, context);
                     }),
                     "Invalid shape takes precedence over cancellation");
    stop = std::stop_source{};
    options.cancellation = stop.get_token();
    scratch.stopOnAllocate = &stop;
    context = TensorExecutionContext::parallel(pool, options);
    FATP_ASSERT_TRUE(throwsAs<TensorExecutionCancelled>([&] {
                         (void)matmul(left, right, context);
                     }),
                     "Cancellation during scratch preparation publishes no result");
    scratch.stopOnAllocate = nullptr;
    scratch.fail = true;
    options.cancellation = {};
    context = TensorExecutionContext::parallel(pool, options);
    FATP_ASSERT_TRUE(throwsAs<std::bad_alloc>([&] {
                         (void)matmul(left, right, context);
                     }),
                     "Scratch failure occurs before any submission");
    FATP_ASSERT_EQ(scratch.live, std::size_t{0}, "Scratch always released");
    return true;
}

FATP_TEST_CASE(scheduler_draining_and_task_bounds)
{
    ThreadPool pool(4, 0);
    auto options = forcedOptions();
    options.maxTasks = 3;
    auto context = TensorExecutionContext::parallel(pool, options);
    std::array<int, 17> visits{};
    std::atomic<int> calls{0};
    tensor_detail::executeTensorRows(context,
                                     visits.size(),
                                     visits.size(),
                                     1,
                                     [&](std::size_t first, std::size_t last) {
                                         ++calls;
                                         for (auto row = first; row < last; ++row)
                                         {
                                             ++visits[row];
                                         }
                                     });
    FATP_ASSERT_EQ(calls.load(), 3, "maxTasks bounds actual submissions");
    FATP_ASSERT_TRUE(std::all_of(visits.begin(),
                                 visits.end(),
                                 [](int n) {
                                     return n == 1;
                                 }),
                     "Static ranges cover each logical row exactly once");
    std::atomic<int> finished{0};
    std::string error;
    try
    {
        tensor_detail::executeTensorRows(context, 17, 17, 1, [&](std::size_t first, std::size_t) {
            ++finished;
            throw std::runtime_error(std::to_string(first));
        });
    }
    catch (const std::runtime_error& exception)
    {
        error = exception.what();
    }
    FATP_ASSERT_EQ(error, std::string("0"), "Lowest submitted task wins independent of completion order");
    FATP_ASSERT_EQ(finished.load(), 3, "Every accepted task drained before propagation");
    std::stop_source stop;
    options.cancellation = stop.get_token();
    context = TensorExecutionContext::parallel(pool, options);
    FATP_ASSERT_TRUE(throwsAs<TensorExecutionCancelled>([&] {
                         tensor_detail::executeTensorRows(context, 17, 17, 1, [&](std::size_t, std::size_t) {
                             stop.request_stop();
                         });
                     }),
                     "Mid-execution cancellation drains accepted work");
    stop = std::stop_source{};
    options.cancellation = stop.get_token();
    context = TensorExecutionContext::parallel(pool, options);
    error.clear();
    try
    {
        tensor_detail::executeTensorRows(context, 17, 17, 1, [&](std::size_t, std::size_t) {
            stop.request_stop();
            throw std::overflow_error("arithmetic wins");
        });
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
    }
    FATP_ASSERT_EQ(error, std::string("arithmetic wins"), "Task failure precedes simultaneous cancellation");
    pool.shutdown();
    options.cancellation = {};
    context = TensorExecutionContext::parallel(pool, options);
    error.clear();
    try
    {
        tensor_detail::executeTensorRows(context, 17, 17, 1, [](std::size_t, std::size_t) {});
    }
    catch (const std::runtime_error& exception)
    {
        error = exception.what();
    }
    FATP_ASSERT_EQ(error, std::string("ThreadPool is shut down"), "Closed executor rejects, not cancels");
    Tensor<int> oneRow({1, 3}, 2), right({3, 4}, 3);
    const auto singleRow = matmul(oneRow, right, context);
    FATP_ASSERT_EQ(singleRow[0], std::int64_t{18}, "One-row matmul stays serial even with a stopped pool");
    options.minimumWork = std::numeric_limits<std::size_t>::max();
    Tensor<int> manyRows({17, 3}, 2);
    const auto small = matmul(manyRows, right, TensorExecutionContext::parallel(pool, options));
    FATP_ASSERT_EQ(small[0], std::int64_t{18}, "Below-threshold calls do not consult a stopped pool");
    return true;
}

FATP_TEST_CASE(nested_and_concurrent_callers)
{
    ThreadPool pool(4, 0), otherPool(2, 0);
    const auto context = TensorExecutionContext::parallel(pool, forcedOptions());
    const auto otherContext = TensorExecutionContext::parallel(otherPool, forcedOptions());
    Tensor<int> left({17, 7}, 2), right({7, 9}, 3);
    const auto expected = matmul(left, right);
    std::barrier start(4);
    std::vector<std::future<bool>> futures;
    for (int i = 0; i < 4; ++i)
    {
        futures.emplace_back(pool.submit([&] {
            start.arrive_and_wait();
            return ThreadPool::isAnyPoolWorkerThread() && exactValues(expected, matmul(left, right, context)) &&
                   exactValues(expected, matmul(left, right, otherContext));
        }));
    }
    for (auto& future : futures)
    {
        FATP_ASSERT_TRUE(future.get(), "Saturated nested calls must not deadlock");
    }
    FATP_ASSERT_TRUE(!ThreadPool::isAnyPoolWorkerThread(), "Worker identity is thread-local");
    std::atomic<bool> correct{true};
    std::vector<std::thread> callers;
    for (int i = 0; i < 4; ++i)
    {
        callers.emplace_back([&] {
            for (int repeat = 0; repeat < 20; ++repeat)
            {
                if (!exactValues(expected, matmul(left, right, context)))
                {
                    correct = false;
                }
            }
        });
    }
    for (auto& caller : callers)
    {
        caller.join();
    }
    FATP_ASSERT_TRUE(correct.load(), "Concurrent callers can share default thread-safe scratch");
    return true;
}

FATP_TEST_CASE(result_allocator_contract)
{
    ThreadPool pool(4, 0);
    auto context = TensorExecutionContext::parallel(pool, forcedOptions());
    ScratchProbe elements;
    using Allocator = std::pmr::polymorphic_allocator<double>;
    Tensor<double, Allocator> left(std::allocator_arg, Allocator(&elements), DynamicExtents{17, 3}, 2.0);
    Tensor<double> right({3, 4}, 3.0);
    const auto before = elements.allocations;
    {
        const auto result = matmul(left, right, context, Allocator(&elements));
        FATP_ASSERT_TRUE(result.get_allocator().resource() == &elements, "Explicit allocator preserved");
        FATP_ASSERT_EQ(elements.allocations, before + 1, "One result element allocation");
        FATP_ASSERT_TRUE(!elements.workerAllocation, "Result allocator never runs on workers");
        const auto selected = matmul(left, right, context);
        FATP_ASSERT_TRUE(selected.get_allocator().resource() == std::pmr::get_default_resource(),
                         "PMR SOCCC selection remains unchanged");
    }
    elements.fail = true;
    FATP_ASSERT_TRUE(throwsAs<std::bad_alloc>([&] {
                         (void)matmul(left, right, context, Allocator(&elements));
                     }),
                     "Result allocation fails before submission");
    FATP_ASSERT_EQ(elements.live, std::size_t{1}, "Only input storage survives failure");
    return true;
}

FATP_TEST_CASE(partial_submission_allocation_failures)
{
#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
    int partialFailures = 0;
    for (std::ptrdiff_t failAt = 0; failAt < 48; ++failAt)
    {
        ThreadPool pool(4, 0);
        auto context = TensorExecutionContext::parallel(pool, forcedOptions());
        std::atomic<int> finished{0};
        bool failed = false;
        {
            allocation_probe::Failure injection(failAt);
            try
            {
                tensor_detail::executeTensorRows(context, 32, 32, 1, [&](std::size_t, std::size_t) {
                    ++finished;
                });
            }
            catch (const std::bad_alloc&)
            {
                failed = true;
            }
        }
        const auto completedOnReturn = finished.load();
        pool.wait_idle();
        FATP_ASSERT_EQ(finished.load(), completedOnReturn, "No task may execute after scheduler throws");
        FATP_ASSERT_EQ(pool.pending_tasks(), std::size_t{0}, "Failed submission leaves no phantom tasks");
        if (failed && completedOnReturn > 0)
        {
            ++partialFailures;
        }
        if (!failed)
        {
            FATP_ASSERT_EQ(completedOnReturn, 4, "Successful sweep executes all tasks");
        }
    }
    FATP_ASSERT_TRUE(partialFailures > 0, "Sweep must exercise failure after accepting a task prefix");

    // Grow both queue kinds with workers blocked: inject every ordinary-new point
    // around queue capacity boundaries, including enqueue after accounting increments.
    for (Priority priority : {Priority::Normal, Priority::High})
    {
        for (std::size_t queued = 0; queued < 34; ++queued)
        {
            for (std::ptrdiff_t failAt = 0; failAt < 8; ++failAt)
            {
                ThreadPool pool(1, 0);
                std::promise<void> release, entered;
                auto gate = release.get_future().share();
                auto blocker = pool.submit([&] {
                    entered.set_value();
                    gate.wait();
                });
                entered.get_future().wait();
                for (std::size_t i = 0; i < queued; ++i)
                {
                    (void)pool.submit_priority(priority, [] {});
                }
                const auto pendingBefore = pool.pending_tasks();
                bool failed = false;
                {
                    allocation_probe::Failure injection(failAt);
                    try
                    {
                        (void)pool.submit_priority(priority, [] {});
                    }
                    catch (const std::bad_alloc&)
                    {
                        failed = true;
                    }
                }
                const auto pendingAfter = pool.pending_tasks();
                release.set_value();
                blocker.get();
                pool.shutdown();
                FATP_ASSERT_EQ(pendingAfter,
                               pendingBefore + (failed ? 0U : 1U),
                               "Queue allocation failure rolls back exactly one pending task");
                FATP_ASSERT_EQ(pool.pending_tasks(), std::size_t{0}, "Shutdown drains after allocation failure");
            }
        }
    }
#else
    std::cout << "  [SKIP] Ordinary-new fault sweep requires standalone, non-checked-iterator build\n";
#endif
    return true;
}

} // namespace fat_p::testing::tensor_execution

namespace fat_p::testing
{
bool test_TensorExecution()
{
    FATP_PRINT_HEADER(TENSOR EXECUTION)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_execution, serial_defaults_and_thresholds);
    FATP_RUN_TEST_NS(runner, tensor_execution, layouts_and_deterministic_folds);
    FATP_RUN_TEST_NS(runner, tensor_execution, validation_empty_and_overflow);
    FATP_RUN_TEST_NS(runner, tensor_execution, cancellation_and_scratch_failure);
    FATP_RUN_TEST_NS(runner, tensor_execution, scheduler_draining_and_task_bounds);
    FATP_RUN_TEST_NS(runner, tensor_execution, nested_and_concurrent_callers);
    FATP_RUN_TEST_NS(runner, tensor_execution, result_allocator_contract);
    FATP_RUN_TEST_NS(runner, tensor_execution, partial_submission_allocation_failures);
    return runner.print_summary() == 0;
}
} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
    return fat_p::testing::test_TensorExecution() ? 0 : 1;
}
#endif
