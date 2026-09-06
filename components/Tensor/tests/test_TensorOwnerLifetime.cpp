/** @file test_TensorOwnerLifetime.cpp @brief Lazy owner tracking and storage-only relocation regressions. */
/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: test
  path: components/Tensor/tests/test_TensorOwnerLifetime.cpp
  namespace: fat_p::testing::tensor_owner_lifetime
  layer: Testing
  summary: "Allocation, invalidation, concurrent borrowing, and owner relocation regressions."
  api_stability: in_work
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include "FatPTest.h"
#include "TensorInterop.h"

#include <array>
#include <barrier>
#include <cstdlib>
#include <memory>
#include <new>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
namespace fat_p::testing::tensor_owner_lifetime::allocation_probe
{
thread_local bool armed = false;
thread_local bool fail = false;
thread_local std::size_t calls = 0;

void* allocate(std::size_t bytes)
{
    if (armed)
    {
        ++calls;
        if (fail)
        {
            throw std::bad_alloc();
        }
    }
    if (void* storage = std::malloc(bytes == 0 ? 1 : bytes))
    {
        return storage;
    }
    throw std::bad_alloc();
}

struct Guard
{
    explicit Guard(bool reject = false) noexcept { calls = 0; fail = reject; armed = true; }
    ~Guard() { armed = false; fail = false; }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};
} // namespace fat_p::testing::tensor_owner_lifetime::allocation_probe

void* operator new(std::size_t n) { return fat_p::testing::tensor_owner_lifetime::allocation_probe::allocate(n); }
void* operator new[](std::size_t n) { return fat_p::testing::tensor_owner_lifetime::allocation_probe::allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#endif

namespace fat_p::testing::tensor_owner_lifetime
{
struct Counted
{
    static inline std::size_t copies = 0;
    static inline std::size_t moves = 0;
    int value = 17;
    Counted() = default;
    Counted(const Counted& other) : value(other.value) { ++copies; }
    Counted(Counted&& other) noexcept : value(other.value) { ++moves; }
    Counted& operator=(const Counted& other) { value = other.value; ++copies; return *this; }
    Counted& operator=(Counted&& other) noexcept { value = other.value; ++moves; return *this; }
};

template <typename T>
struct ThrowingMoveAllocator : std::allocator<T>
{
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    template <typename U> struct rebind { using other = ThrowingMoveAllocator<U>; };
    ThrowingMoveAllocator() = default;
    ThrowingMoveAllocator(const ThrowingMoveAllocator&) = default;
    ThrowingMoveAllocator(ThrowingMoveAllocator&&) noexcept(false) {}
    ThrowingMoveAllocator& operator=(const ThrowingMoveAllocator&) = default;
    ThrowingMoveAllocator& operator=(ThrowingMoveAllocator&&) noexcept(false) { return *this; }
};

static_assert(std::is_nothrow_move_constructible_v<Tensor<int>>);
static_assert(std::is_nothrow_move_assignable_v<Tensor<int>>);
static_assert(std::is_nothrow_swappable_v<Tensor<int>>);
static_assert(std::is_nothrow_move_constructible_v<RankedTensor<int, 6>>);
static_assert(!std::is_nothrow_move_constructible_v<RankedTensor<int, 0>>);
static_assert(!std::is_nothrow_move_assignable_v<RankedTensor<int, 0>>);
static_assert(!std::is_nothrow_move_constructible_v<Tensor<int, ThrowingMoveAllocator<int>>>);
static_assert(!std::is_nothrow_move_assignable_v<Tensor<int, ThrowingMoveAllocator<int>>>);

FATP_TEST_CASE(vector_growth_transfers_storage)
{
    std::vector<Tensor<Counted>> owners;
    owners.reserve(1);
    owners.emplace_back(DynamicExtents{10000});
    const auto* storage = owners[0].data();
    const auto borrowed = owners[0].asConstView();
    const auto shared = owners[0].asSharedView();
    Counted::copies = Counted::moves = 0;
    owners.reserve(2);
    FATP_ASSERT_EQ(Counted::copies, std::size_t{0}, "Container growth must not copy tensor elements");
    FATP_ASSERT_EQ(Counted::moves, std::size_t{0}, "Container growth transfers the storage handle");
    FATP_ASSERT_TRUE(owners[0].data() == storage, "Storage address survives owner relocation");
    FATP_ASSERT_EQ(shared[9999].value, 17, "Shared aliases survive relocation");
    FATP_ASSERT_EQ(owners[0].asConstView()[0].value, 17, "Relocated owner can publish a fresh borrow");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(borrowed[0], std::runtime_error, "Old owner borrows are invalidated on relocation");
#else
    (void)borrowed;
#endif
    return true;
}

FATP_TEST_CASE(allocation_free_empty_and_storage_transfers)
{
#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
    Tensor<int> highRank(DynamicExtents{2, 1, 1, 1, 1, 1}, 7);
    const auto previous = highRank.asConstView();
    Tensor<int> destination({1}, 9);
    bool valid = false;
    {
        allocation_probe::Guard guard(true);
        Tensor<int> empty;
        Tensor<int> moved(std::move(highRank));
        destination = std::move(moved);
        swap(destination, empty);
        valid = empty.size() == 2 && empty[0] == 7 && highRank.empty() && moved.empty();
    }
    FATP_ASSERT_TRUE(valid, "Default empty owners and ordinary transfers work with global new disabled");
    FATP_ASSERT_EQ(allocation_probe::calls, std::size_t{0}, "Transfers allocate no lifetime state");
#ifndef NDEBUG
    FATP_ASSERT_THROWS(previous[0], std::runtime_error, "Nonallocating move still invalidates borrows");
#else
    (void)previous;
#endif
#endif
    return true;
}

FATP_TEST_CASE(first_borrow_failure_is_recoverable)
{
    Tensor<int> owner({2}, 23);
#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
    bool failed = false;
    try
    {
        allocation_probe::Guard guard(true);
        (void)owner.asConstView();
    }
    catch (const std::bad_alloc&) { failed = true; }
#ifndef NDEBUG
    FATP_ASSERT_TRUE(failed, "First Debug borrow reports tracking allocation failure");
#else
    FATP_ASSERT_FALSE(failed, "Release borrows need no tracking allocation");
#endif
    FATP_ASSERT_EQ(owner[1], 23, "Tracking allocation failure preserves storage");
#endif
    const auto first = owner.asConstView();
    auto descriptor = describeTensor(owner);
    FATP_ASSERT_EQ(descriptor.borrow()[1], 23, "Descriptors reuse the published tracking state");
#if defined(ENABLE_TEST_APPLICATION) && !defined(FATP_TENSOR_DISABLE_ALLOCATION_PROBE) && \
    (!defined(_MSC_VER) || _ITERATOR_DEBUG_LEVEL == 0)
    bool readable = false;
    {
        allocation_probe::Guard guard(true);
        const auto second = owner.asConstView();
        readable = second[0] == 23;
    }
    FATP_ASSERT_TRUE(readable, "Subsequent low-rank borrows reuse tracking without allocation");
#endif
    FATP_ASSERT_EQ(first[0], 23, "First successful borrow remains valid");
    return true;
}

FATP_TEST_CASE(concurrent_const_borrows_share_tracking)
{
    constexpr std::size_t count = 12;
    Tensor<int> owner({4}, 31);
    std::array<TensorView<const int>, count> views;
    std::array<std::thread, count> workers;
    std::barrier start(static_cast<std::ptrdiff_t>(count + 1));
    for (std::size_t i = 0; i < count; ++i)
    {
        workers[i] = std::thread([&, i] { start.arrive_and_wait(); views[i] = std::as_const(owner).asConstView(); });
    }
    start.arrive_and_wait();
    for (auto& worker : workers) { worker.join(); }
    for (const auto& view : views) { FATP_ASSERT_EQ(view[3], 31, "Every concurrent borrow is live"); }
    Tensor<int> relocated(std::move(owner));
#ifndef NDEBUG
    for (const auto& view : views)
    {
        FATP_ASSERT_THROWS(view[0], std::runtime_error, "One owner move invalidates every concurrent borrow");
    }
#endif
    const auto relocatedView = relocated.asConstView();
    FATP_ASSERT_EQ(relocatedView[0], 31, "Moved owner publishes a new tracking generation");
    FATP_ASSERT_TRUE(owner.asConstView().empty(), "Moved-from owner can be borrowed again");
    return true;
}

FATP_TEST_CASE(empty_shared_views_retain_a_handle)
{
    SharedTensorView<int> shared;
    {
        Tensor<int> empty;
        shared = empty.asSharedView();
        const auto descriptor = describeTensor(shared);
        FATP_ASSERT_TRUE(descriptor.sharedStorageLifetime.use_count() > 0,
                         "Empty shared mappings still carry a real ownership handle");
        Tensor<int> moved(std::move(empty));
        FATP_ASSERT_TRUE(moved.empty(), "Moving an empty shared owner retains empty shape");
    }
    FATP_ASSERT_TRUE(shared.empty(), "Empty shared mapping survives owner destruction");
    FATP_ASSERT_TRUE(shared.begin() == shared.end(), "Empty shared mapping remains iterable");
    return true;
}
} // namespace fat_p::testing::tensor_owner_lifetime

namespace fat_p::testing
{
bool test_TensorOwnerLifetime()
{
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensor_owner_lifetime, vector_growth_transfers_storage);
    FATP_RUN_TEST_NS(runner, tensor_owner_lifetime, allocation_free_empty_and_storage_transfers);
    FATP_RUN_TEST_NS(runner, tensor_owner_lifetime, first_borrow_failure_is_recoverable);
    FATP_RUN_TEST_NS(runner, tensor_owner_lifetime, concurrent_const_borrows_share_tracking);
    FATP_RUN_TEST_NS(runner, tensor_owner_lifetime, empty_shared_views_retain_a_handle);
    return runner.print_summary() == 0;
}
}
#ifdef ENABLE_TEST_APPLICATION
int main() { return fat_p::testing::test_TensorOwnerLifetime() ? 0 : 1; }
#endif
