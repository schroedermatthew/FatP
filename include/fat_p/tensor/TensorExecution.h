#pragma once

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: internal_header
  path: include/fat_p/tensor/TensorExecution.h
  namespace: fat_p
  layer: Domain
  summary: "Bounded deterministic output scheduling for Tensor matmul and explicit-axis contractions."
  api_stability: in_work
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  related:
    docs_search: "TensorExecution"
    headers:
      - include/fat_p/TensorExecution.h
      - include/fat_p/ThreadPool.h
      - include/fat_p/tensor/TensorMatmul.h
      - include/fat_p/tensor/TensorContractions.h
    tests:
      - components/Tensor/tests/test_TensorExecution.cpp
      - components/Tensor/tests/test_TensorContractions.cpp
*/

#include "../ThreadPool.h"
#include "TensorMatmul.h"
#include "TensorContractions.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <future>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <stop_token>
#include <vector>

namespace fat_p
{

/** @brief Scheduling metadata only; result elements still use the result allocator. */
struct TensorExecutionOptions
{
    std::size_t grainSize = 32;        ///< Matmul rows or tensorDot output elements per cancellation tile.
    std::size_t minimumWork = 1048576; ///< Scalar products below this count stay serial (zero disables).
    std::size_t maxTasks = 0;          ///< Zero uses pool size; otherwise caps submitted tasks per call.
    std::stop_token cancellation;
    std::pmr::memory_resource* scratch = std::pmr::get_default_resource();
};

/**
 * @brief Explicit synchronous execution policy. Default construction is serial.
 *
 * parallel() borrows a caller-owned pool; no hidden executor is ever created. The pool
 * and scratch resource must outlive each call. Shared contexts require a thread-safe
 * scratch resource (or external serialization). Only the future array uses scratch;
 * ThreadPool task/promise allocations and Tensor layout metadata do not.
 *
 * Parallel work owns disjoint output rows and preserves each increasing-inner fold.
 * Bitwise floating agreement assumes identical build and floating-point environment
 * on caller and workers; caller thread-local state is not propagated to the pool.
 * Calls from ANY Fat-P pool worker fall back to serial, preventing nested pool waits.
 */
class TensorExecutionContext
{
public:
    TensorExecutionContext() = default;

    [[nodiscard]] static TensorExecutionContext serial(TensorExecutionOptions options = {})
    {
        return TensorExecutionContext(nullptr, options);
    }

    [[nodiscard]] static TensorExecutionContext parallel(ThreadPool& pool, TensorExecutionOptions options = {})
    {
        return TensorExecutionContext(&pool, options);
    }

    [[nodiscard]] ThreadPool* pool() const noexcept
    {
        return mPool;
    }
    [[nodiscard]] const TensorExecutionOptions& options() const noexcept
    {
        return mOptions;
    }

private:
    TensorExecutionContext(ThreadPool* pool, TensorExecutionOptions options)
        : mPool(pool)
        , mOptions(options)
    {
        if (options.grainSize == 0 || options.scratch == nullptr)
        {
            throw std::invalid_argument("Tensor execution requires positive grainSize and nonnull scratch");
        }
    }

    ThreadPool* mPool = nullptr;
    TensorExecutionOptions mOptions;
};

/** @brief Cooperative cancellation; no partially computed owner is returned. */
class TensorExecutionCancelled : public std::runtime_error
{
public:
    TensorExecutionCancelled()
        : std::runtime_error("Tensor execution cancelled")
    {
    }
};

namespace tensor_detail
{

// A raw allocator-backed array avoids checked-iterator proxy allocation inside
// noexcept vector constructors. Even in MSVC Debug, scratch failure must throw.
class TensorTaskFutures
{
public:
    TensorTaskFutures(std::pmr::memory_resource* scratch, std::size_t count)
        : mAllocator(scratch)
        , mCount(count)
        , mData(mAllocator.allocate(count))
    {
        static_assert(std::is_nothrow_default_constructible_v<std::future<void>>);
        std::uninitialized_default_construct_n(mData, mCount);
    }
    ~TensorTaskFutures()
    {
        std::destroy_n(mData, mCount);
        mAllocator.deallocate(mData, mCount);
    }
    TensorTaskFutures(const TensorTaskFutures&) = delete;
    TensorTaskFutures& operator=(const TensorTaskFutures&) = delete;
    std::future<void>& operator[](std::size_t index) noexcept
    {
        return mData[index];
    }

private:
    std::pmr::polymorphic_allocator<std::future<void>> mAllocator;
    std::size_t mCount;
    std::future<void>* mData;
};

inline void checkTensorCancellation(const std::stop_token& token)
{
    if (token.stop_requested())
    {
        throw TensorExecutionCancelled();
    }
}

// Compare elements * inner >= threshold without overflowing the product.
[[nodiscard]] inline bool
tensorWorkReachesThreshold(std::size_t elements, std::size_t inner, std::size_t threshold) noexcept
{
    return threshold == 0 || (inner != 0 && elements >= threshold / inner + (threshold % inner != 0));
}

template <typename WriteRows>
void executeTensorRows(const TensorExecutionContext& context,
                       std::size_t rows,
                       std::size_t elements,
                       std::size_t inner,
                       const WriteRows& writeRows)
{
    const auto& options = context.options();
    const auto tileCount = rows / options.grainSize + (rows % options.grainSize != 0);
    auto* pool = context.pool();
    std::size_t taskCount = 1;
    if (pool != nullptr && !ThreadPool::isAnyPoolWorkerThread() &&
        tensorWorkReachesThreshold(elements, inner, options.minimumWork))
    {
        taskCount = std::min(tileCount, pool->thread_count());
        if (options.maxTasks != 0)
        {
            taskCount = std::min(taskCount, options.maxTasks);
        }
    }

    const auto writeTiles = [&](std::size_t firstTile, std::size_t lastTile) {
        // Every nonempty first tile is in range; only the final endpoint can round up.
        auto firstRow = firstTile * options.grainSize;
        const auto lastRow = lastTile == tileCount ? rows : lastTile * options.grainSize;
        if (!options.cancellation.stop_possible())
        {
            writeRows(firstRow, lastRow);
            return;
        }
        while (firstRow < lastRow && !options.cancellation.stop_requested())
        {
            const auto end = firstRow + std::min(options.grainSize, lastRow - firstRow);
            writeRows(firstRow, end);
            firstRow = end;
        }
    };

    checkTensorCancellation(options.cancellation);
    if (taskCount < 2)
    {
        writeTiles(0, tileCount);
        checkTensorCancellation(options.cancellation);
        return;
    }

    // Allocate before submitting anything. Move-assigning a future cannot throw.
    TensorTaskFutures futures(options.scratch, taskCount);
    std::size_t submitted = 0;
    std::exception_ptr submissionError;
    std::size_t firstTile = 0;
    try
    {
        for (std::size_t task = 0; task < taskCount; ++task)
        {
            if (options.cancellation.stop_requested())
            {
                break;
            }
            const auto count = tileCount / taskCount + (task < tileCount % taskCount);
            const auto lastTile = firstTile + count;
            futures[submitted] = pool->submit([&, firstTile, lastTile] {
                writeTiles(firstTile, lastTile);
            });
            ++submitted;
            firstTile = lastTile;
        }
    }
    catch (...)
    {
        submissionError = std::current_exception();
    }

    std::exception_ptr taskError;
    for (std::size_t task = 0; task < submitted; ++task)
    {
        try
        {
            futures[task].get();
        }
        catch (...)
        {
            if (!taskError)
            {
                taskError = std::current_exception();
            }
        }
    }
    // All accepted tasks are drained, even after partial submission or arithmetic failure.
    if (submissionError)
    {
        std::rethrow_exception(submissionError);
    }
    if (taskError)
    {
        std::rethrow_exception(taskError);
    }
    checkTensorCancellation(options.cancellation);
}

} // namespace tensor_detail

/**
 * @brief Matmul with explicit synchronous serial/deterministic-parallel execution.
 * @details Input lifetime/shape is validated before cancellation. Cancellation is observed
 * before element allocation, between row tiles, and after draining; it has no hard latency
 * bound and never interrupts an inner fold. Errors prioritize submission failure, then
 * the lowest-index failed task, then cancellation. All accepted tasks finish before any
 * return or throw. Inputs must not be mutated/resized/destroyed concurrently.
 * Cancellation observed at the final check discards even a fully computed result.
 * A stopped pool throws only when scheduling is attempted; below-threshold, single-tile,
 * capped-to-one, nested, and explicit-serial calls do not submit to it.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto
matmul(const Left& left, const Right& right, const TensorExecutionContext& context, const Allocator& allocator)
    -> Tensor<TensorMatmulType<typename Left::value_type>, Allocator,
              tensor_detail::matmulStaticRank<Left, Right>>
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto shape = tensor_detail::makeMatmulShape(left.layout(), right.layout());
    tensor_detail::checkTensorCancellation(context.options().cancellation);
    constexpr auto OutputRank = tensor_detail::matmulStaticRank<Left, Right>;
    using OutputExtents = tensor_detail::TensorExtentsFor<OutputRank>;
    Tensor<result_type, Allocator, OutputRank> result(
        std::allocator_arg, allocator,
        OutputExtents(shape.outputExtents.begin(), shape.outputExtents.end()), result_type{0});
    if (!result.empty() && shape.inner != 0)
    {
        tensor_detail::executeTensorRows(context,
                                         result.size() / shape.columns,
                                         result.size(),
                                         shape.inner,
                                         [&](std::size_t first, std::size_t last) {
                                             tensor_detail::matmulRows(left, right, shape, result, first, last);
                                         });
    }
    tensor_detail::checkTensorCancellation(context.options().cancellation);
    return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
}

/** @brief Context matmul with unchanged first-owner SOCCC/default result allocator selection. */
template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto matmul(const Left& left, const Right& right, const TensorExecutionContext& context)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return matmul(left, right, context, tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

/** @brief Dot with context cancellation; its single output fold always stays serial. */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto
dot(const Left& left, const Right& right, const TensorExecutionContext& context, const Allocator& allocator)
{
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    if (left.rank() != 1 || right.rank() != 1)
    {
        throw std::invalid_argument("dot requires two rank-one operands");
    }
    if (left.extents()[0] != right.extents()[0])
    {
        throw std::invalid_argument("dot vector lengths must match");
    }
    return matmul(left, right, context, allocator);
}

template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto dot(const Left& left, const Right& right, const TensorExecutionContext& context)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return dot(left, right, context, tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

/**
 * @brief tensorDot with explicit synchronous execution; default fold order is unchanged.
 * @details Scheduling partitions flattened output elements, never an inner fold.
 * grainSize counts output elements here (flattened rows for matmul). The existing
 * minimumWork threshold counts output-size times contracted-size scalar products.
 * Cancellation, draining, nested fallback, scratch, and error precedence match context matmul.
 */
template <ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto tensorDot(const Left& left,
                             const Right& right,
                             const std::vector<TensorAxis>& leftAxes,
                             const std::vector<TensorAxis>& rightAxes,
                             const TensorExecutionContext& context,
                             const Allocator& allocator)
    -> Tensor<TensorMatmulType<typename Left::value_type>, Allocator>
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto shape = tensor_detail::makeContractionShape(left.layout(), right.layout(), leftAxes, rightAxes);
    tensor_detail::checkTensorCancellation(context.options().cancellation);
    Tensor<result_type, Allocator> result(std::allocator_arg, allocator, shape.outputExtents, result_type{0});
    if (!result.empty() && shape.inner != 0)
    {
        tensor_detail::executeTensorRows(context,
                                         result.size(),
                                         result.size(),
                                         shape.inner,
                                         [&](std::size_t first, std::size_t last) {
                                             tensor_detail::contractionRows(left, right, shape, result, first, last);
                                         });
    }
    tensor_detail::checkTensorCancellation(context.options().cancellation);
    return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
}

/** @brief Context tensorDot with the same allocator selection as the serial overload. */
template <ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto tensorDot(const Left& left,
                             const Right& right,
                             const std::vector<TensorAxis>& leftAxes,
                             const std::vector<TensorAxis>& rightAxes,
                             const TensorExecutionContext& context)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return tensorDot(left,
                     right,
                     leftAxes,
                     rightAxes,
                     context,
                     tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

template <std::size_t PairCount, ReadableTensor Left, ReadableTensor Right, typename Allocator>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type> &&
             tensor_detail::AllocatorFor<Allocator, TensorMatmulType<typename Left::value_type>>
[[nodiscard]] auto tensorDot(const Left& left, const Right& right,
                             const std::array<TensorAxis, PairCount>& leftAxes,
                             const std::array<TensorAxis, PairCount>& rightAxes,
                             const TensorExecutionContext& context, const Allocator& allocator)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    constexpr auto OutputRank = tensor_detail::contractionStaticRank<PairCount, Left, Right>;
    tensor_detail::TensorAccess::validate(left);
    tensor_detail::TensorAccess::validate(right);
    const auto shape = [&] {
        if constexpr (OutputRank == tensor_detail::kDynamicTensorRank)
        {
            const std::vector<TensorAxis> dynamicLeft(leftAxes.begin(), leftAxes.end());
            const std::vector<TensorAxis> dynamicRight(rightAxes.begin(), rightAxes.end());
            return tensor_detail::makeContractionShape(
                left.layout(), right.layout(), dynamicLeft, dynamicRight);
        }
        else
        {
            return tensor_detail::makeFixedContractionShape(
                left.layout(), right.layout(), leftAxes, rightAxes);
        }
    }();
    tensor_detail::checkTensorCancellation(context.options().cancellation);
    Tensor<result_type, Allocator, OutputRank> result(
        std::allocator_arg, allocator, shape.outputExtents, result_type{0});
    if (!result.empty() && shape.inner != 0)
    {
        tensor_detail::executeTensorRows(
            context, result.size(), result.size(), shape.inner,
            [&](std::size_t first, std::size_t last) {
                tensor_detail::contractionRows(left, right, shape, result, first, last);
            });
    }
    tensor_detail::checkTensorCancellation(context.options().cancellation);
    return tensor_detail::TensorAccess::finishMaterialization(std::move(result));
}

template <std::size_t PairCount, ReadableTensor Left, ReadableTensor Right>
    requires SameTensorValue<Left, Right> && std::is_arithmetic_v<typename Left::value_type>
[[nodiscard]] auto tensorDot(const Left& left, const Right& right,
                             const std::array<TensorAxis, PairCount>& leftAxes,
                             const std::array<TensorAxis, PairCount>& rightAxes,
                             const TensorExecutionContext& context)
{
    using result_type = TensorMatmulType<typename Left::value_type>;
    return tensorDot(left, right, leftAxes, rightAxes, context,
                     tensor_detail::selectBinaryResultAllocator<result_type>(left, right));
}

} // namespace fat_p
