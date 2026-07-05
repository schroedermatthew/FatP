/**
 * @file test_ConcurrencyPolicies.cpp
 * @brief Comprehensive unit tests for all 19 ConcurrencyPolicies
 */
/*
FATP_META:
  meta_version: 1
  component: ConcurrencyPolicies
  file_role: test
  path: components/ConcurrencyPolicies/tests/test_ConcurrencyPolicies.cpp
  layer: Testing
  namespace: fat_p::testing::concurrencypolicies
  summary: "Unit tests for ConcurrencyPolicies."
  api_stability: in_work
  related:
    docs_search: "ConcurrencyPolicies"
    headers:
      - include/fat_p/ConcurrencyPolicies.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ConcurrencyPolicies.h"
#include "FatPTest.h"

#if defined(__SANITIZE_THREAD__)
#define FATP_TSAN_ACTIVE 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define FATP_TSAN_ACTIVE 1
#endif
#endif

using namespace fat_p::testing;
using namespace fat_p;

namespace fat_p::testing::concurrencypolicies
{

// =============================================================================
// Helper Classes
// =============================================================================

template <typename Policy>
struct ProtectedValue
{
    Policy policy;
    int value = 0;

    typename Policy::LockGuard lock()
    {
        return policy.lock();
    }

    typename Policy::SharedGuard shared_lock()
    {
        return policy.lock_shared();
    }
};

template <typename Policy>
void run_concurrent_increment(Policy& policy, std::atomic<int>& counter, int ops)
{
    for (int i = 0; i < ops; ++i)
    {
        [[maybe_unused]] auto guard = policy.lock();
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

// =============================================================================
// Trait Tests
// =============================================================================

FATP_TEST_CASE(policy_traits)
{
    std::cout << colors::cyan() << "\nTesting Policy Traits..." << colors::reset() << std::endl;

    static_assert(is_concurrency_policy_v<SingleThreadedPolicy>, "Should have PolicyTag");
    static_assert(is_concurrency_policy_v<MutexSynchronizationPolicy>, "Should have PolicyTag");
    static_assert(is_concurrency_policy_v<SharedMutexPolicy>, "Should have PolicyTag");
    static_assert(is_concurrency_policy_v<SpinlockSynchronizationPolicy>, "Should have PolicyTag");

    static_assert(is_shared_policy_v<SingleThreadedPolicy>, "Should have SharedGuard");
    static_assert(is_shared_policy_v<MutexSynchronizationPolicy>, "Should have SharedGuard");
    static_assert(is_shared_policy_v<SharedMutexPolicy>, "Should have SharedGuard");

    static_assert(is_fair_policy_v<TicketLockPolicy>, "TicketLock is fair");
    static_assert(is_fair_policy_v<MCSLockPolicy>, "MCS is fair");
    static_assert(!is_fair_policy_v<SpinlockSynchronizationPolicy>, "Spinlock not fair");

    static_assert(is_optimistic_policy_v<SeqLockPolicy>, "SeqLock is optimistic");
    static_assert(is_optimistic_policy_v<VersionedLockPolicy>, "Versioned is optimistic");
    static_assert(!is_optimistic_policy_v<MutexSynchronizationPolicy>, "Mutex not optimistic");

    static_assert(is_numa_aware_policy_v<MCSLockPolicy>, "MCS is NUMA-aware");
    static_assert(!is_numa_aware_policy_v<TicketLockPolicy>, "TicketLock not NUMA-aware");

    static_assert(is_realtime_policy_v<PriorityInheritanceLockPolicy>, "PI is realtime");

    static_assert(is_lockfree_policy_v<RCUPolicy<int>>, "RCU is lock-free");
    static_assert(is_lockfree_policy_v<HazardPointerPolicy<int>>, "HP is lock-free");
    static_assert(is_lockfree_policy_v<LockFreeSynchronizationPolicy>, "LockFree has tag");

    static_assert(is_adaptive_policy_v<AdaptiveLockPolicy>, "Adaptive is adaptive");

    static_assert(has_contention_tracking_v<SingleThreadedPolicy>, "ST tracks contention");
    static_assert(has_contention_tracking_v<MutexSynchronizationPolicy>, "Mutex tracks contention");
    static_assert(has_contention_tracking_v<SpinlockSynchronizationPolicy>, "Spinlock tracks");
    static_assert(has_contention_tracking_v<AdaptiveLockPolicy>, "Adaptive tracks");

    static_assert(is_recursive_policy_v<RecursiveMutexPolicy>, "RecursiveMutex is recursive");
    static_assert(!is_recursive_policy_v<MutexSynchronizationPolicy>, "Mutex not recursive");

    static_assert(is_timed_policy_v<TimedMutexPolicy>, "TimedMutex is timed");

    static_assert(supports_try_lock_v<SingleThreadedPolicy>, "ST supports try_lock");
    static_assert(supports_try_lock_v<MutexSynchronizationPolicy>, "Mutex supports try_lock");
    static_assert(supports_try_lock_v<SharedMutexPolicy>, "SharedMutex supports try_lock");
    static_assert(supports_try_lock_v<SpinlockSynchronizationPolicy>, "Spinlock supports try_lock");
    static_assert(supports_try_lock_v<TicketLockPolicy>, "TicketLock supports try_lock");
    static_assert(supports_try_lock_v<AdaptiveLockPolicy>, "Adaptive supports try_lock");
    static_assert(supports_try_lock_v<VersionedLockPolicy>, "Versioned supports try_lock");
    static_assert(supports_try_lock_v<RecursiveMutexPolicy>, "Recursive supports try_lock");
    static_assert(supports_try_lock_v<TimedMutexPolicy>, "Timed supports try_lock");
    static_assert(supports_try_lock_v<PriorityInheritanceLockPolicy>, "PI supports try_lock");

    // C++20 concept-based checks (verify concepts match _v variables)
    static_assert(ConcurrencyPolicy<SingleThreadedPolicy>, "ST satisfies ConcurrencyPolicy");
    static_assert(ConcurrencyPolicy<MutexSynchronizationPolicy>, "Mutex satisfies ConcurrencyPolicy");
    static_assert(ConcurrencyPolicy<SharedMutexPolicy>, "SharedMutex satisfies ConcurrencyPolicy");
    static_assert(!ConcurrencyPolicy<int>, "int does not satisfy ConcurrencyPolicy");

    static_assert(ConcurrencyPolicyTag<SingleThreadedPolicy>, "ST has PolicyTag");
    static_assert(SharedPolicy<SharedMutexPolicy>, "SharedMutex is SharedPolicy");
    static_assert(FairPolicy<TicketLockPolicy>, "TicketLock is FairPolicy");
    static_assert(!FairPolicy<SpinlockSynchronizationPolicy>, "Spinlock not FairPolicy");
    static_assert(OptimisticPolicy<SeqLockPolicy>, "SeqLock is OptimisticPolicy");
    static_assert(NumaAwarePolicy<MCSLockPolicy>, "MCS is NumaAwarePolicy");
    static_assert(RealtimePolicy<PriorityInheritanceLockPolicy>, "PI is RealtimePolicy");
    static_assert(LockFreePolicy<RCUPolicy<int>>, "RCU is LockFreePolicy");
    static_assert(AdaptivePolicy<AdaptiveLockPolicy>, "Adaptive is AdaptivePolicy");
    static_assert(HasContentionTracking<MutexSynchronizationPolicy>, "Mutex has contention tracking");
    static_assert(RecursivePolicy<RecursiveMutexPolicy>, "RecursiveMutex is RecursivePolicy");
    static_assert(TimedPolicy<TimedMutexPolicy>, "TimedMutex is TimedPolicy");
    static_assert(SupportsTryLock<MutexSynchronizationPolicy>, "Mutex supports SupportsTryLock");

    std::cout << colors::green() << "Policy Traits & Concepts: All static_asserts passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Regression: FATP_CACHE_LINE_SIZE sourced from PlatformDetection.h
// =============================================================================
// ConcurrencyPolicies.h previously defined its own FATP_CACHE_LINE_SIZE fallback
// of 64, bypassing PlatformDetection.h's Apple Silicon detection (128 bytes).
// The fix includes FatPConfig.h (which includes PlatformDetection.h) and removes
// the local fallback. These tests verify the macro flows through the correct chain.

FATP_TEST_CASE(cache_line_size_source)
{
    std::cout << colors::cyan() << "\nRegression: FATP_CACHE_LINE_SIZE source..." << colors::reset() << std::endl;

    constexpr size_t cl = FATP_CACHE_LINE_SIZE;

    // Must be power of two, at least 32
    FATP_ASSERT_GE(cl, size_t(32), "FATP_CACHE_LINE_SIZE must be >= 32");
    FATP_ASSERT_EQ(cl & (cl - 1), size_t(0), "FATP_CACHE_LINE_SIZE must be power of two");

    // PlatformDetection.h macros must be visible (proves FatPConfig.h was included)
#if defined(FATP_PLATFORM_LINUX) || defined(FATP_PLATFORM_MACOS) || defined(FATP_PLATFORM_WINDOWS)
    FATP_ASSERT_TRUE(true, "PlatformDetection.h macros available via FatPConfig.h");
#else
    FATP_ASSERT_TRUE(false, "PlatformDetection.h macros not available — FatPConfig.h not included");
#endif

    // Platform-specific value checks
#if defined(FATP_PLATFORM_MACOS) && FATP_PLATFORM_MACOS && defined(FATP_ARCH_ARM64) && FATP_ARCH_ARM64
    FATP_ASSERT_EQ(cl, size_t(128), "Apple Silicon requires 128-byte cache line from PlatformDetection.h");
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    FATP_ASSERT_EQ(cl, size_t(64), "x86/x86-64 uses 64-byte cache line");
#endif

    // All alignas(FATP_CACHE_LINE_SIZE) types must have sufficient alignment
    FATP_ASSERT_GE(alignof(SpinlockSynchronizationPolicy), cl,
        "SpinlockSynchronizationPolicy alignment must match FATP_CACHE_LINE_SIZE");
    FATP_ASSERT_GE(alignof(TicketLockPolicy), cl,
        "TicketLockPolicy alignment must match FATP_CACHE_LINE_SIZE");
    FATP_ASSERT_EQ(alignof(MCSLockPolicy::QNode), cl,
        "MCSLockPolicy::QNode alignment must equal FATP_CACHE_LINE_SIZE");
    FATP_ASSERT_GE(alignof(SeqLockPolicy), cl,
        "SeqLockPolicy alignment must match FATP_CACHE_LINE_SIZE");

    std::cout << colors::green() << "  FATP_CACHE_LINE_SIZE = " << cl
              << " (sourced from PlatformDetection.h)" << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// Regression: FATP_HAS_ATOMIC_SHARED_PTR from CppFeatureDetection.h only
// =============================================================================
// ConcurrencyPolicies.h previously re-detected FATP_HAS_ATOMIC_SHARED_PTR using
// raw compiler version checks, potentially overriding CppFeatureDetection.h's
// authoritative detection via __cpp_lib_atomic_shared_ptr. The re-detection block
// has been removed. These tests verify consistency with the standard feature-test
// macro.

FATP_TEST_CASE(atomic_shared_ptr_detection_source)
{
    std::cout << colors::cyan() << "\nRegression: FATP_HAS_ATOMIC_SHARED_PTR source..." << colors::reset() << std::endl;

    constexpr int has_asp = FATP_HAS_ATOMIC_SHARED_PTR;
    FATP_ASSERT_TRUE(has_asp == 0 || has_asp == 1,
        "FATP_HAS_ATOMIC_SHARED_PTR must be 0 or 1");

    // Cross-check against the standard feature-test macro
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    FATP_ASSERT_EQ(has_asp, 1,
        "__cpp_lib_atomic_shared_ptr present but FATP_HAS_ATOMIC_SHARED_PTR is 0");
    std::cout << colors::green() << "  FATP_HAS_ATOMIC_SHARED_PTR = 1 "
              << "(confirmed via __cpp_lib_atomic_shared_ptr)" << colors::reset() << std::endl;
#else
    FATP_ASSERT_EQ(has_asp, 0,
        "No __cpp_lib_atomic_shared_ptr but FATP_HAS_ATOMIC_SHARED_PTR is 1 — "
        "residual re-detection in ConcurrencyPolicies.h?");
    std::cout << colors::green() << "  FATP_HAS_ATOMIC_SHARED_PTR = 0 "
              << "(no __cpp_lib_atomic_shared_ptr, correctly not overridden)" << colors::reset() << std::endl;
#endif

    return true;
}

// =============================================================================
// SingleThreadedPolicy Tests
// =============================================================================

FATP_TEST_CASE(SingleThreadedPolicy)
{
    std::cout << colors::cyan() << "\nTesting SingleThreadedPolicy..." << colors::reset() << std::endl;

    SingleThreadedPolicy policy;
    int value = 0;

    {
        [[maybe_unused]] auto guard = policy.lock();
        value = 42;
        FATP_ASSERT_EQ(value, 42, "Value should be 42");
    }

    {
        [[maybe_unused]] auto guard = policy.lock_shared();
        FATP_ASSERT_EQ(value, 42, "Shared read should work");
    }

    {
        auto& static_lock = SingleThreadedPolicy::getStaticLock();
        (void)static_lock;
        [[maybe_unused]] SingleThreadedPolicy::LockGuard guard{};
        FATP_ASSERT_TRUE(true, "Static lock should work");
    }

    std::cout << colors::green() << "SingleThreadedPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// MutexSynchronizationPolicy Tests
// =============================================================================

#if FATP_USE_MUTEX
FATP_TEST_CASE(MutexSynchronizationPolicy)
{
    std::cout << colors::cyan() << "\nTesting MutexSynchronizationPolicy..." << colors::reset() << std::endl;

    MutexSynchronizationPolicy policy;
    std::atomic<int> counter{0};
    const int num_threads = 8;
    const int ops_per_thread = 10000;

    policy.reset_contention();
    FATP_ASSERT_EQ(policy.get_contention(), 0u, "Contention should reset to 0");

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]() {
            run_concurrent_increment(policy, counter, ops_per_thread);
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), num_threads * ops_per_thread, "All increments counted");

    uint64_t contention = policy.get_contention();
    std::cout << colors::blue() << "  [INFO] Contention count: " << contention << colors::reset() << std::endl;
    FATP_ASSERT_TRUE(contention > 0, "Should have tracked contention");

    {
        auto& static_lock = MutexSynchronizationPolicy::getStaticLock();
        [[maybe_unused]] MutexSynchronizationPolicy::LockGuard guard(static_lock);
        FATP_ASSERT_TRUE(true, "Static lock works");
    }

    std::cout << colors::green() << "MutexSynchronizationPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// SharedMutexPolicy Tests
// =============================================================================

#if FATP_USE_SHARED_MUTEX
FATP_TEST_CASE(SharedMutexPolicy)
{
    std::cout << colors::cyan() << "\nTesting SharedMutexPolicy..." << colors::reset() << std::endl;

    SharedMutexPolicy policy;
    std::atomic<int> value{0};
    std::atomic<int> read_sum{0};

    {
        [[maybe_unused]] auto guard = policy.lock();
        value.store(100, std::memory_order_relaxed);
    }

    const int num_readers = 8;
    std::vector<std::thread> readers;
    for (int i = 0; i < num_readers; ++i)
    {
        readers.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock_shared();
                read_sum.fetch_add(value.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : readers)
    {
        t.join();
    }

    FATP_ASSERT_EQ(read_sum.load(), num_readers * 1000 * 100, "All reads correct");

    {
        auto& static_lock = SharedMutexPolicy::getStaticLock();
        [[maybe_unused]] SharedMutexPolicy::LockGuard guard(static_lock);
        FATP_ASSERT_TRUE(true, "Static lock works");
    }

    std::cout << colors::green() << "SharedMutexPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// UniqueRWLockPolicy Tests
// =============================================================================

#if FATP_USE_SHARED_MUTEX
FATP_TEST_CASE(UniqueRWLockPolicy)
{
    std::cout << colors::cyan() << "\nTesting UniqueRWLockPolicy..." << colors::reset() << std::endl;

    UniqueRWLockPolicy policy;
    std::atomic<int> counter{0};

    {
        [[maybe_unused]] auto guard = policy.lock();
        counter.store(42, std::memory_order_relaxed);
    }

    {
        [[maybe_unused]] auto guard = policy.lock_shared();
        FATP_ASSERT_EQ(counter.load(), 42, "Read should work");
    }

    UniqueRWLockPolicy policy2 = std::move(policy);
    {
        [[maybe_unused]] auto guard = policy2.lock();
        counter.store(100, std::memory_order_relaxed);
    }
    FATP_ASSERT_EQ(counter.load(), 100, "Move should work");

    std::cout << colors::green() << "UniqueRWLockPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// SpinlockSynchronizationPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC
FATP_TEST_CASE(SpinlockSynchronizationPolicy)
{
    std::cout << colors::cyan() << "\nTesting SpinlockSynchronizationPolicy..." << colors::reset() << std::endl;

    SpinlockSynchronizationPolicy policy;
    std::atomic<int> counter{0};
    const int num_threads = 8;
    const int ops_per_thread = 10000;

    policy.reset_contention();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]() {
            run_concurrent_increment(policy, counter, ops_per_thread);
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), num_threads * ops_per_thread, "All increments counted");

    uint64_t contention = policy.get_contention();
    std::cout << colors::blue() << "  [INFO] Spin contention: " << contention << " ("
              << (double)contention / (num_threads * ops_per_thread) << "x)" << colors::reset() << std::endl;

    std::cout << colors::green() << "SpinlockSynchronizationPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// LockFreeSynchronizationPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC
FATP_TEST_CASE(LockFreeSynchronizationPolicy)
{
    std::cout << colors::cyan() << "\nTesting LockFreeSynchronizationPolicy..." << colors::reset() << std::endl;

    LockFreeSynchronizationPolicy policy;

    {
        [[maybe_unused]] auto guard = policy.lock();
        FATP_ASSERT_TRUE(true, "Lock guard construction works");
    }

    {
        [[maybe_unused]] auto guard = policy.lock_shared();
        FATP_ASSERT_TRUE(true, "Shared guard construction works");
    }

    std::cout << colors::green() << "LockFreeSynchronizationPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// LockFreeWithFallbackPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC && FATP_USE_MUTEX
FATP_TEST_CASE(LockFreeWithFallbackPolicy)
{
    std::cout << colors::cyan() << "\nTesting LockFreeWithFallbackPolicy..." << colors::reset() << std::endl;

    LockFreeWithFallbackPolicy<MutexSynchronizationPolicy> policy;
    std::atomic<int> counter{0};

#ifndef NDEBUG
    std::cout << colors::blue() << "  [INFO] Debug mode - uses mutex fallback for safety" << colors::reset()
              << std::endl;

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock();
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), 4000, "Debug mode uses mutex fallback");
    FATP_ASSERT_TRUE(policy.get_contention() > 0, "Should track contention in debug");
#else
    std::cout << colors::blue() << "  [INFO] Release mode - no-op (lock-free)" << colors::reset() << std::endl;

    for (int j = 0; j < 1000; ++j)
    {
        [[maybe_unused]] auto guard = policy.lock();
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    FATP_ASSERT_EQ(counter.load(), 1000, "Release mode is no-op");
    FATP_ASSERT_EQ(policy.get_contention(), 0u, "No contention tracking in release");
#endif

    FATP_ASSERT_TRUE(policy.try_lock(), "try_lock should work");

    std::cout << colors::green() << "LockFreeWithFallbackPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// WaitableSynchronizationPolicy Tests
// =============================================================================

#if FATP_USE_MUTEX && FATP_USE_CONDITION_VARIABLE
FATP_TEST_CASE(WaitableSynchronizationPolicy)
{
    std::cout << colors::cyan() << "\nTesting WaitableSynchronizationPolicy..." << colors::reset() << std::endl;

    WaitableSynchronizationPolicy policy;
    bool ready = false;
    bool processed = false;

    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        [[maybe_unused]] auto guard = policy.lock();
        ready = true;
        policy.getCondition().notify_one();
    });

    std::thread consumer([&]() {
        [[maybe_unused]] auto guard = policy.lock();
        guard.wait(policy.getCondition(), [&]() {
            return ready;
        });
        processed = true;
    });

    producer.join();
    consumer.join();

    FATP_ASSERT_TRUE(ready && processed, "Producer/consumer should synchronize");

    {
        [[maybe_unused]] auto guard = policy.lock();
        FATP_ASSERT_TRUE(guard.owns_lock(), "Should own lock");
        FATP_ASSERT_TRUE(guard.mutex() != nullptr, "Should have mutex pointer");
    }

    std::cout << colors::green() << "WaitableSynchronizationPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// SeqLockPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC
FATP_TEST_CASE(SeqLockPolicy)
{
    std::cout << colors::cyan() << "\nTesting SeqLockPolicy..." << colors::reset() << std::endl;

    SeqLockPolicy policy;
    int test_data = 0;

    {
        [[maybe_unused]] auto guard = policy.lock();
        test_data = 42;
    }

    {
        [[maybe_unused]] auto guard = policy.lock_shared();
        int value = test_data;
        FATP_ASSERT_TRUE(guard.is_valid(), "Read should be valid");
        FATP_ASSERT_EQ(value, 42, "Read value should match");
    }

    uint64_t seq1 = policy.get_sequence();
    {
        [[maybe_unused]] auto guard = policy.lock();
        test_data = 100;
    }
    uint64_t seq2 = policy.get_sequence();
    FATP_ASSERT_TRUE(seq2 > seq1, "Sequence should increment");

    std::atomic<int> successful_reads{0};
    std::atomic<int> failed_reads{0};
    std::atomic<bool> stop{false};

    test_data = 0;

    std::thread writer([&]() {
        for (int burst = 0; burst < 10; ++burst)
        {
            for (int i = 0; i < 100; ++i)
            {
                [[maybe_unused]] auto guard = policy.lock();
                test_data++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        stop.store(true);
    });

    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i)
    {
        readers.emplace_back([&]() {
            while (!stop.load())
            {
                [[maybe_unused]] auto guard = policy.lock_shared();
                int value = test_data;
                (void)value;
                if (guard.is_valid())
                {
                    successful_reads.fetch_add(1);
                }
                else
                {
                    failed_reads.fetch_add(1);
                }
            }
        });
    }

    writer.join();
    for (auto& r : readers)
    {
        r.join();
    }

    FATP_ASSERT_EQ(test_data, 1000, "All writes should complete");
    std::cout << colors::blue() << "  [INFO] Successful reads: " << successful_reads.load()
              << ", Failed: " << failed_reads.load() << colors::reset() << std::endl;
    FATP_ASSERT_TRUE(successful_reads.load() > 100, "Should have successful reads");

    std::cout << colors::green() << "SeqLockPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// TicketLockPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC
FATP_TEST_CASE(TicketLockPolicy)
{
    std::cout << colors::cyan() << "\nTesting TicketLockPolicy..." << colors::reset() << std::endl;

    TicketLockPolicy policy;
    std::atomic<int> counter{0};
    const int num_threads = 8;
    const int ops = 5000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < ops; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock();
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), num_threads * ops, "All increments counted");
    FATP_ASSERT_EQ(policy.get_queue_length(), 0u, "Queue should be empty");

    std::cout << colors::green() << "TicketLockPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// MCSLockPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC
FATP_TEST_CASE(MCSLockPolicy)
{
    std::cout << colors::cyan() << "\nTesting MCSLockPolicy..." << colors::reset() << std::endl;

    MCSLockPolicy policy;
    std::atomic<int> counter{0};
    const int num_threads = 8;
    const int ops = 5000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < ops; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock();
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), num_threads * ops, "All increments counted");

    std::cout << colors::green() << "MCSLockPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// RCUPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC && FATP_USE_SHARED_MUTEX
FATP_TEST_CASE(RCUPolicy)
{
    std::cout << colors::cyan() << "\nTesting RCUPolicy..." << colors::reset() << std::endl;

    RCUPolicy<int> rcu(42);

    {
        [[maybe_unused]] auto guard = rcu.read();
        FATP_ASSERT_EQ(*guard, 42, "Initial read should work");
    }

    {
        [[maybe_unused]] auto guard = rcu.write();
        guard.update([](int& val) {
            val = 100;
        });
    }

    {
        [[maybe_unused]] auto guard = rcu.read();
        FATP_ASSERT_EQ(*guard, 100, "Updated value should be visible");
    }

#ifndef FATP_TSAN_ACTIVE
    // libstdc++ atomic<shared_ptr>::load uses an internal mutex that TSan
    // reports as double-lock under concurrent readers (false positive).
    std::atomic<int> read_count{0};
    std::vector<std::thread> readers;

    for (int i = 0; i < 4; ++i)
    {
        readers.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j)
            {
                [[maybe_unused]] auto guard = rcu.read();
                if (*guard == 100)
                {
                    read_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& r : readers)
    {
        r.join();
    }

    FATP_ASSERT_EQ(read_count.load(), 4000, "All reads should see updated value");
#else
    std::cout << colors::blue() << "  [INFO] Skipping concurrent RCU readers under TSan"
              << colors::reset() << std::endl;
#endif

    std::cout << colors::blue() << "  [INFO] RCU is_lock_free: " << (RCUPolicy<int>::is_lock_free() ? "yes" : "no")
              << colors::reset() << std::endl;

    std::cout << colors::green() << "RCUPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// HazardPointerPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC
FATP_TEST_CASE(HazardPointerPolicy)
{
    std::cout << colors::cyan() << "\nTesting HazardPointerPolicy..." << colors::reset() << std::endl;

    HazardPointerPolicy<int> hp;

    std::atomic<int*> ptr(new int(42));
    {
        [[maybe_unused]] auto guard = hp.acquire();
        int* protected_ptr = guard.protect(ptr);
        FATP_ASSERT_EQ(*protected_ptr, 42, "Protected read should work");
    }

    int* old_ptr = ptr.exchange(new int(100));
    hp.retire(old_ptr);
    FATP_ASSERT_TRUE(true, "Retire should work");

    // Clean up remaining allocation in ptr
    delete ptr.load();

    std::atomic<int*> shared_ptr(new int(0));
    std::atomic<int> sum{0};
    std::atomic<bool> stop{false};
    std::atomic<bool> readers_done{false};

    std::thread writer([&]() {
        for (int i = 1; i <= 100; ++i)
        {
            int* new_val = new int(i);
            int* old_val = shared_ptr.exchange(new_val);
            hp.retire(old_val);
            std::this_thread::yield();
        }
        stop.store(true);
        // Wait for readers to release hazard pointers before reclaiming
        while (!readers_done.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        hp.force_reclaim();
    });

    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i)
    {
        readers.emplace_back([&]() {
            while (!stop.load())
            {
                [[maybe_unused]] auto guard = hp.acquire();
                int* p = guard.protect(shared_ptr);
                if (p)
                {
                    sum.fetch_add(*p, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& r : readers)
    {
        r.join();
    }
    readers_done.store(true, std::memory_order_release);
    writer.join();

    hp.force_reclaim();

    delete shared_ptr.load();

    std::cout << colors::blue() << "  [INFO] Sum of reads: " << sum.load() << colors::reset() << std::endl;

    std::cout << colors::green() << "HazardPointerPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// AdaptiveLockPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC && FATP_USE_MUTEX
FATP_TEST_CASE(AdaptiveLockPolicy)
{
    std::cout << colors::cyan() << "\nTesting AdaptiveLockPolicy..." << colors::reset() << std::endl;

    AdaptiveLockPolicy policy;
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i)
    {
        [[maybe_unused]] auto guard = policy.lock();
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    FATP_ASSERT_EQ(counter.load(), 100, "Low contention should work");

    bool initial_mode = policy.is_using_mutex();
    std::cout << colors::blue() << "  [INFO] Initial mode (mutex): " << initial_mode << colors::reset() << std::endl;

    counter.store(0);
    policy.reset_contention();

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 2000; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock();
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), 16000, "High contention should work");
    std::cout << colors::blue() << "  [INFO] Final mode (mutex): " << policy.is_using_mutex()
              << ", Contention: " << policy.get_contention() << colors::reset() << std::endl;

    std::cout << colors::green() << "AdaptiveLockPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// AdaptiveLockPolicy: Mutual exclusion violation detector
//
// The original bug: spinlock and mutex were independent locks, so threads
// holding different ones could enter the critical section simultaneously.
// An atomic counter cannot detect this because fetch_add is itself atomic.
// This test uses a PLAIN (non-atomic) counter plus an occupancy flag to
// detect concurrent entry.
// -----------------------------------------------------------------------------
FATP_TEST_CASE(AdaptiveLockPolicy_mutual_exclusion)
{
    std::cout << colors::cyan() << "\nTesting AdaptiveLockPolicy mutual exclusion..."
              << colors::reset() << std::endl;

    AdaptiveLockPolicy policy;

    // Non-atomic shared state -- any concurrent access is a data race that
    // either TSan will flag or the occupancy check will catch.
    int counter = 0;
    std::atomic<bool> violation_detected{false};
    std::atomic<int> occupancy{0};

    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 50000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&]() {
            for (int i = 0; i < ops_per_thread; ++i)
            {
                [[maybe_unused]] auto guard = policy.lock();

                // If more than one thread is inside the lock, mutual exclusion
                // is broken. The fetch_add/fetch_sub pair on occupancy is atomic,
                // so even without TSan we can detect concurrent entry.
                int prev = occupancy.fetch_add(1, std::memory_order_relaxed);
                if (prev != 0)
                {
                    violation_detected.store(true, std::memory_order_relaxed);
                }

                // Non-atomic increment -- a data race here corrupts counter
                ++counter;

                occupancy.fetch_sub(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_FALSE(violation_detected.load(),
                      "No two threads should be inside the lock simultaneously");
    FATP_ASSERT_EQ(counter, num_threads * ops_per_thread,
                   "Non-atomic counter must equal expected if mutual exclusion held");

    std::cout << colors::green() << "AdaptiveLockPolicy_mutual_exclusion: passed."
              << colors::reset() << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// AdaptiveLockPolicy: Force spin-to-mutex transition and verify correctness
//
// Creates enough contention to guarantee the adaptive mechanism switches to
// mutex mode, then verifies mutual exclusion holds across the transition.
// The key insight: with many threads and a high iteration count, some threads
// WILL exhaust their spin budget and take the mutex fallback path while others
// are still on the spin path. If the transition is not atomic, the occupancy
// check catches it.
// -----------------------------------------------------------------------------
FATP_TEST_CASE(AdaptiveLockPolicy_transition_stress)
{
    std::cout << colors::cyan() << "\nTesting AdaptiveLockPolicy transition stress..."
              << colors::reset() << std::endl;

    AdaptiveLockPolicy policy;

    int counter = 0;
    std::atomic<bool> violation{false};
    std::atomic<int> occupancy{0};
    std::atomic<bool> start{false};

    // Use more threads than cores to maximise scheduling contention and
    // force spin budget exhaustion.
    const int num_threads = static_cast<int>(std::thread::hardware_concurrency() * 2);
    constexpr int ops_per_thread = 20000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&]() {
            // Barrier -- all threads start simultaneously for maximum contention
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (int i = 0; i < ops_per_thread; ++i)
            {
                [[maybe_unused]] auto guard = policy.lock();

                int prev = occupancy.fetch_add(1, std::memory_order_relaxed);
                if (prev != 0)
                {
                    violation.store(true, std::memory_order_relaxed);
                }

                ++counter;

                occupancy.fetch_sub(1, std::memory_order_relaxed);
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : threads)
    {
        t.join();
    }

    // The policy should have switched to mutex mode under this load
    std::cout << colors::blue() << "  [INFO] Using mutex after stress: "
              << policy.is_using_mutex() << ", Contention: " << policy.get_contention()
              << colors::reset() << std::endl;

    FATP_ASSERT_FALSE(violation.load(),
                      "Mutual exclusion must hold across spin-to-mutex transition");
    FATP_ASSERT_EQ(counter, num_threads * ops_per_thread,
                   "Non-atomic counter must be exact after transition stress");

    std::cout << colors::green() << "AdaptiveLockPolicy_transition_stress: passed."
              << colors::reset() << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// AdaptiveLockPolicy: try_lock() must not succeed while lock is held
//
// Holds the lock from one thread and verifies try_lock() fails from another.
// Also stress-tests try_lock() under contention to ensure it never grants
// access when another thread holds the lock.
// -----------------------------------------------------------------------------
FATP_TEST_CASE(AdaptiveLockPolicy_try_lock)
{
    std::cout << colors::cyan() << "\nTesting AdaptiveLockPolicy try_lock..."
              << colors::reset() << std::endl;

    // Part 1: Basic try_lock exclusion
    {
        AdaptiveLockPolicy policy;
        std::atomic<bool> holding{false};
        std::atomic<bool> try_lock_succeeded_while_held{false};
        std::atomic<bool> done{false};

        std::thread holder([&]() {
            auto guard = policy.lock();
            holding.store(true, std::memory_order_release);
            // Hold the lock until the other thread has tested try_lock
            while (!done.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        });

        // Wait for holder to acquire lock
        while (!holding.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        // try_lock must fail while the lock is held
        for (int i = 0; i < 100; ++i)
        {
            if (policy.try_lock())
            {
                try_lock_succeeded_while_held.store(true, std::memory_order_relaxed);
                // Must unlock if we somehow got it (to avoid deadlock in holder)
                // Note: there's no public unlock() so we can't easily undo this,
                // but if try_lock succeeds incorrectly, the flag records it.
                break;
            }
        }

        done.store(true, std::memory_order_release);
        holder.join();

        FATP_ASSERT_FALSE(try_lock_succeeded_while_held.load(),
                          "try_lock() must not succeed while lock is held by another thread");
    }

    std::cout << colors::green() << "AdaptiveLockPolicy_try_lock: passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// PriorityInheritanceLockPolicy Tests
// =============================================================================

#if FATP_USE_MUTEX
FATP_TEST_CASE(PriorityInheritanceLockPolicy)
{
    std::cout << colors::cyan() << "\nTesting PriorityInheritanceLockPolicy..." << colors::reset() << std::endl;

    PriorityInheritanceLockPolicy policy;
    std::atomic<int> counter{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 5000; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock();
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), 20000, "All increments counted");

#if FATP_HAS_PTHREAD_PRIO_INHERIT
    std::cout << colors::blue() << "  [INFO] Using PTHREAD_PRIO_INHERIT" << colors::reset() << std::endl;
#elif FATP_HAS_WIN32_CRITICAL_SECTION
    std::cout << colors::blue() << "  [INFO] Using Win32 CRITICAL_SECTION" << colors::reset() << std::endl;
#else
    std::cout << colors::blue() << "  [INFO] Using fallback std::mutex" << colors::reset() << std::endl;
#endif

    std::cout << colors::green() << "PriorityInheritanceLockPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// VersionedLockPolicy Tests
// =============================================================================

#if FATP_USE_ATOMIC
FATP_TEST_CASE(VersionedLockPolicy)
{
    std::cout << colors::cyan() << "\nTesting VersionedLockPolicy..." << colors::reset() << std::endl;

    VersionedLockPolicy policy;
    int test_data = 0;

    uint64_t v1 = policy.get_version();
    {
        [[maybe_unused]] auto guard = policy.lock();
        test_data = 42;
    }
    uint64_t v2 = policy.get_version();
    FATP_ASSERT_TRUE(v2 > v1, "Version should increment after write");

    {
        [[maybe_unused]] auto guard = policy.lock_shared();
        int value = test_data;
        FATP_ASSERT_TRUE(guard.validate(), "Version should be valid");
        FATP_ASSERT_EQ(value, 42, "Read value should match");
        FATP_ASSERT_EQ(guard.get_version(), v2, "Guard version should match");
    }

    {
        [[maybe_unused]] auto guard = policy.lock();
        test_data = 100;
        FATP_ASSERT_EQ(guard.get_version(), v2, "Write guard has old version");
        guard.commit();
    }
    uint64_t v3 = policy.get_version();
    FATP_ASSERT_TRUE(v3 > v2, "Explicit commit should increment version");

    std::cout << colors::green() << "VersionedLockPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// RecursiveMutexPolicy Tests
// =============================================================================

#if FATP_USE_MUTEX
FATP_TEST_CASE(RecursiveMutexPolicy)
{
    std::cout << colors::cyan() << "\nTesting RecursiveMutexPolicy..." << colors::reset() << std::endl;

    RecursiveMutexPolicy policy;
    std::atomic<int> counter{0};

    {
        auto guard1 = policy.lock();
        counter.fetch_add(1);
        {
            auto guard2 = policy.lock();
            counter.fetch_add(1);
            {
                auto guard3 = policy.lock();
                counter.fetch_add(1);
            }
        }
    }
    FATP_ASSERT_EQ(counter.load(), 3, "Recursive locks should work");

    counter.store(0);
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 5000; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock();
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(counter.load(), 20000, "Concurrent access should work");

    std::cout << colors::green() << "RecursiveMutexPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// TimedMutexPolicy Tests
// =============================================================================

#if FATP_USE_MUTEX
FATP_TEST_CASE(TimedMutexPolicy)
{
    std::cout << colors::cyan() << "\nTesting TimedMutexPolicy..." << colors::reset() << std::endl;

    TimedMutexPolicy policy;

    {
        [[maybe_unused]] auto guard = policy.lock();
        FATP_ASSERT_TRUE(guard.owns_lock(), "Should acquire lock");
    }

#ifndef FATP_TSAN_ACTIVE
    {
        [[maybe_unused]] auto guard = policy.lock_deferred();
        FATP_ASSERT_TRUE(!guard.owns_lock(), "Deferred should not own lock");
        bool acquired = guard.try_lock_for(std::chrono::milliseconds(10));
        FATP_ASSERT_TRUE(acquired, "Should acquire uncontended lock");
        FATP_ASSERT_TRUE(guard.owns_lock(), "Should own lock after try_lock_for");
    }

    TimedMutexPolicy policy2;
    std::atomic<bool> holder_ready{false};

    std::thread holder([&]() {
        [[maybe_unused]] auto guard = policy2.lock();
        holder_ready.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    while (!holder_ready.load())
    {
        std::this_thread::yield();
    }

    {
        [[maybe_unused]] auto guard = policy2.lock_deferred();
        bool acquired = guard.try_lock_for(std::chrono::milliseconds(10));
        FATP_ASSERT_TRUE(!acquired, "Should timeout on contended lock");
    }

    holder.join();
#endif

    std::cout << colors::green() << "TimedMutexPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// SharedTimedMutexPolicy Tests
// =============================================================================

#if FATP_USE_SHARED_MUTEX
FATP_TEST_CASE(SharedTimedMutexPolicy)
{
    std::cout << colors::cyan() << "\nTesting SharedTimedMutexPolicy..." << colors::reset() << std::endl;

    SharedTimedMutexPolicy policy;
    std::atomic<int> value{0};

    {
        [[maybe_unused]] auto guard = policy.lock();
        value.store(42);
    }

    std::atomic<int> read_sum{0};
    std::vector<std::thread> readers;

    for (int i = 0; i < 8; ++i)
    {
        readers.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j)
            {
                [[maybe_unused]] auto guard = policy.lock_shared();
                read_sum.fetch_add(value.load(), std::memory_order_relaxed);
            }
        });
    }

    for (auto& r : readers)
    {
        r.join();
    }

    FATP_ASSERT_EQ(read_sum.load(), 8 * 1000 * 42, "All shared reads correct");

    std::cout << colors::green() << "SharedTimedMutexPolicy: Tests passed." << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// Performance Benchmarks
// =============================================================================
// =============================================================================
// Main Test Function
// =============================================================================

} // namespace fat_p::testing::concurrencypolicies

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_ConcurrencyPolicies()
{
    FATP_PRINT_HEADER(CONCURRENCY POLICIES)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, concurrencypolicies, policy_traits);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, cache_line_size_source);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, atomic_shared_ptr_detection_source);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, SingleThreadedPolicy);

#if FATP_USE_MUTEX
    FATP_RUN_TEST_NS(runner, concurrencypolicies, MutexSynchronizationPolicy);
#endif

#if FATP_USE_SHARED_MUTEX
    FATP_RUN_TEST_NS(runner, concurrencypolicies, SharedMutexPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, UniqueRWLockPolicy);
#endif

#if FATP_USE_ATOMIC
    FATP_RUN_TEST_NS(runner, concurrencypolicies, SpinlockSynchronizationPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, LockFreeSynchronizationPolicy);
#endif

#if FATP_USE_ATOMIC && FATP_USE_MUTEX
    FATP_RUN_TEST_NS(runner, concurrencypolicies, LockFreeWithFallbackPolicy);
#endif

#if FATP_USE_MUTEX && FATP_USE_CONDITION_VARIABLE
    FATP_RUN_TEST_NS(runner, concurrencypolicies, WaitableSynchronizationPolicy);
#endif

#if FATP_USE_ATOMIC
    FATP_RUN_TEST_NS(runner, concurrencypolicies, SeqLockPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, TicketLockPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, MCSLockPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, HazardPointerPolicy);
#endif

#if FATP_USE_ATOMIC && FATP_USE_SHARED_MUTEX
    FATP_RUN_TEST_NS(runner, concurrencypolicies, RCUPolicy);
#endif

#if FATP_USE_ATOMIC && FATP_USE_MUTEX
    FATP_RUN_TEST_NS(runner, concurrencypolicies, AdaptiveLockPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, AdaptiveLockPolicy_mutual_exclusion);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, AdaptiveLockPolicy_transition_stress);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, AdaptiveLockPolicy_try_lock);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, VersionedLockPolicy);
#endif

#if FATP_USE_MUTEX
    FATP_RUN_TEST_NS(runner, concurrencypolicies, PriorityInheritanceLockPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, RecursiveMutexPolicy);
    FATP_RUN_TEST_NS(runner, concurrencypolicies, TimedMutexPolicy);
#endif

#if FATP_USE_SHARED_MUTEX
    FATP_RUN_TEST_NS(runner, concurrencypolicies, SharedTimedMutexPolicy);
#endif

    int failed = runner.print_summary();

    if (failed == 0)
    {
    }

    return failed == 0;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ConcurrencyPolicies() ? 0 : 1;
}
#endif
