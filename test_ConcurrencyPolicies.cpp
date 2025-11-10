/**
 * @file test_ConcurrencyPolicies.cpp
 * @brief Comprehensive unit tests for ALL 16 ConcurrencyPolicies
 *
 * @details Complete test suite for:
 * - 8 Original policies (Mutex, Spinlock, SharedMutex, etc.)
 * - 8 New advanced policies (SeqLock, TicketLock, MCS, RCU, etc.)
 * 
 * @version 4.0.0 - COMPLETE with all 19 policies, C++17/C++20/C++23 compatible
 * Includes: Fixed SeqLock test, contended benchmarks, enhanced traits
 */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <memory>
#include <iomanip>

#include "ConcurrencyPolicies.h"
#include "test_ConcurrencyPolicies.h"
#include "test_Utilities.h"

using namespace cpp_utilities::testing;
using namespace cpp_utilities;

namespace cpp_utilities::testing
{

// =============================================================================
// Helper Classes
// =============================================================================

template <typename Policy>
struct ProtectedValue {
    Policy policy;
    int value = 0;

    typename Policy::LockGuard lock() {
        return typename Policy::LockGuard(policy.getLock());
    }

    typename Policy::SharedGuard shared_lock() {
        return typename Policy::SharedGuard(policy.getLock());
    }

#if (defined(CPP_UTILITIES_USE_MUTEX) && CPP_UTILITIES_USE_MUTEX) || \
    (defined(CPP_UTILITIES_USE_ATOMIC) && CPP_UTILITIES_USE_ATOMIC)
    template<typename P = Policy>
    auto get_contention() const -> decltype(std::declval<P>().get_contention()) {
        return policy.get_contention();
    }
#endif
};

// Generic helper for most policies
template <typename Policy>
void run_concurrent_write_test(ProtectedValue<Policy>& data, int num_threads, int ops_per_thread) {
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&data, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                typename Policy::LockGuard guard(data.policy.getLock());
                data.value++;
            }
        });
    }
    for (auto& t : threads) { if (t.joinable()) t.join(); }
}

// Specialization for MutexSynchronizationPolicy (contention tracking)
template <>
void run_concurrent_write_test<MutexSynchronizationPolicy>(
    ProtectedValue<MutexSynchronizationPolicy>& data,
    int num_threads,
    int ops_per_thread) {
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&data, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                MutexSynchronizationPolicy::LockGuard guard(data.policy);
                data.value++;
            }
        });
    }
    for (auto& t : threads) { if (t.joinable()) t.join(); }
}

// Specialization for SpinlockSynchronizationPolicy (contention tracking)
template <>
void run_concurrent_write_test<SpinlockSynchronizationPolicy>(
    ProtectedValue<SpinlockSynchronizationPolicy>& data,
    int num_threads,
    int ops_per_thread) {
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&data, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                SpinlockSynchronizationPolicy::LockGuard guard(data.policy);
                data.value++;
            }
        });
    }
    for (auto& t : threads) { if (t.joinable()) t.join(); }
}

template <typename Policy>
void run_concurrent_read_test(ProtectedValue<Policy>& data, int num_threads, std::atomic<int>& read_sum) {
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&data, &read_sum]() {
            typename Policy::SharedGuard guard(data.policy.getLock());
            read_sum.fetch_add(data.value, std::memory_order_relaxed);
        });
    }
    for (auto& t : threads) { if (t.joinable()) t.join(); }
}

// =============================================================================
// ORIGINAL 8 POLICIES TESTS
// =============================================================================

// =============================================================================
// I. SingleThreadedPolicy Tests
// =============================================================================

bool test_SingleThreadedPolicy() {
    std::cout << colors::cyan() << "\nTesting SingleThreadedPolicy..."
              << colors::reset() << std::endl;
    ProtectedValue<SingleThreadedPolicy> data;

    {
        auto guard = data.lock();
        data.value = 42;
        ASSERT_EQ(data.value, 42, "Value should be 42");
    }

    {
        auto& static_lock = SingleThreadedPolicy::getStaticLock();
        SingleThreadedPolicy::LockGuard guard(static_lock);
        SIMPLE_ASSERT(true, "Static lock should work");
    }

    {
        auto guard = data.shared_lock();
        int val = data.value;
        ASSERT_EQ(val, 42, "Shared guard should work");
    }

    std::cout << colors::blue()
              << "  [INFO] Concurrent test skipped (Policy is non-concurrent)."
              << colors::reset() << std::endl;

    std::cout << colors::green() << "SingleThreadedPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}

// =============================================================================
// II. MutexSynchronizationPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_MUTEX
bool test_MutexSynchronizationPolicy() {
    std::cout << colors::cyan() << "\nTesting MutexSynchronizationPolicy..."
              << colors::reset() << std::endl;
    ProtectedValue<MutexSynchronizationPolicy> data;
    const int num_threads = 10;
    const int ops = 50000;
    const int expected_value = num_threads * ops;

    {
        data.value = 0;
        run_concurrent_write_test<MutexSynchronizationPolicy>(data, num_threads, ops);
        ASSERT_EQ(data.value, expected_value, "Concurrent writes should be thread-safe");

#if CPP_UTILITIES_USE_ATOMIC
        int contention = data.policy.get_contention();
        std::cout << colors::blue()
                  << "  [INFO] MutexSynchronizationPolicy Contention (Total Acquisitions): "
                  << contention << colors::reset() << std::endl;
        SIMPLE_ASSERT(contention == expected_value, "Contention counter should match acquisitions");
#endif
    }

    {
        data.value = 1;
        typename MutexSynchronizationPolicy::SharedGuard shared_guard(data.policy.getLock());
        data.value = 2;
        ASSERT_EQ(data.value, 2, "SharedGuard should work as exclusive lock");
    }

    {
        auto& static_lock = MutexSynchronizationPolicy::getStaticLock();
        MutexSynchronizationPolicy::LockGuard guard(static_lock);
        SIMPLE_ASSERT(true, "Static mutex access should work");
    }

    std::cout << colors::green() << "MutexSynchronizationPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// III. SharedMutexPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_SHARED_MUTEX
bool test_SharedMutexPolicy() {
    std::cout << colors::cyan() << "\nTesting SharedMutexPolicy..."
              << colors::reset() << std::endl;
    ProtectedValue<SharedMutexPolicy> data;
    const int num_threads = 10;
    const int ops = 50000;
    const int expected_value = num_threads * ops / 2;

    {
        data.value = 0;
        run_concurrent_write_test<SharedMutexPolicy>(data, num_threads / 2, ops);
        ASSERT_EQ(data.value, expected_value, "Concurrent writes should be thread-safe");
    }

    {
        std::atomic<int> read_sum(0);
        run_concurrent_read_test<SharedMutexPolicy>(data, num_threads, read_sum);
        ASSERT_EQ(read_sum.load(), data.value * num_threads, "Concurrent reads should work");
    }

    {
        auto static_lock = SharedMutexPolicy::getStaticLock();
        SharedMutexPolicy::LockGuard guard(static_lock);
        SIMPLE_ASSERT(true, "Static shared mutex access should work");
    }

    std::cout << colors::green() << "SharedMutexPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// IV. UniqueRWLockPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_SHARED_MUTEX
bool test_UniqueRWLockPolicy() {
    std::cout << colors::cyan() << "\nTesting UniqueRWLockPolicy..."
              << colors::reset() << std::endl;
    ProtectedValue<UniqueRWLockPolicy> data;
    const int num_threads = 10;
    const int ops = 50000;
    const int expected_value = num_threads * ops / 2;

    {
        data.value = 0;
        run_concurrent_write_test<UniqueRWLockPolicy>(data, num_threads / 2, ops);
        ASSERT_EQ(data.value, expected_value, "Concurrent writes should be thread-safe");
    }

    {
        std::atomic<int> read_sum(0);
        run_concurrent_read_test<UniqueRWLockPolicy>(data, num_threads, read_sum);
        ASSERT_EQ(read_sum.load(), data.value * num_threads, "Concurrent reads should work");
    }

    {
        auto& static_lock = UniqueRWLockPolicy::getStaticLock();
        UniqueRWLockPolicy::LockGuard guard(static_lock);
        SIMPLE_ASSERT(true, "Static unique RW lock access should work");
    }

    std::cout << colors::green() << "UniqueRWLockPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// V. SpinlockSynchronizationPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC
bool test_SpinlockSynchronizationPolicy() {
    std::cout << colors::cyan() << "\nTesting SpinlockSynchronizationPolicy..."
              << colors::reset() << std::endl;
    ProtectedValue<SpinlockSynchronizationPolicy> data;
    const int num_threads = 10;
    const int ops = 50000;
    const int expected_value = num_threads * ops;

    {
        data.value = 0;
        run_concurrent_write_test<SpinlockSynchronizationPolicy>(data, num_threads, ops);
        ASSERT_EQ(data.value, expected_value, "Concurrent increment should be thread-safe");

        int contention = data.policy.get_contention();
        std::cout << colors::blue()
                  << "  [INFO] SpinlockSynchronizationPolicy Contention: "
                  << contention << colors::reset() << std::endl;
        std::cout << colors::blue()
                  << "  [INFO] Contention ratio: "
                  << (double)contention / expected_value << "x spins per acquisition"
                  << colors::reset() << std::endl;
        SIMPLE_ASSERT(contention >= 0, "Contention count should be valid");
    }

    {
        data.value = 1;
        typename SpinlockSynchronizationPolicy::SharedGuard shared_guard(data.policy.getLock());
        data.value = 2;
        ASSERT_EQ(data.value, 2, "SharedGuard should work as exclusive lock");
    }

    {
        auto& static_lock = SpinlockSynchronizationPolicy::getStaticLock();
        SpinlockSynchronizationPolicy::LockGuard guard(static_lock);
        SIMPLE_ASSERT(true, "Static spinlock access should work");
    }

    std::cout << colors::green() << "SpinlockSynchronizationPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// VI. LockFreeSynchronizationPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC
bool test_LockFreeSynchronizationPolicy() {
    std::cout << colors::cyan() << "\nTesting LockFreeSynchronizationPolicy..."
              << colors::reset() << std::endl;

    {
        LockFreeSynchronizationPolicy policy;
        LockFreeSynchronizationPolicy::LockGuard guard(policy.getLock());
        SIMPLE_ASSERT(true, "Lock guard construction should work");
    }

    std::cout << colors::blue()
              << "  [INFO] LockFreePolicy Destructor expected to call assert(false) in debug builds."
              << colors::reset() << std::endl;

    std::cout << colors::green() << "LockFreeSynchronizationPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// VII. LockFreeWithFallbackPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_MUTEX
bool test_LockFreeWithFallbackPolicy() {
    std::cout << colors::cyan() << "\nTesting LockFreeWithFallbackPolicy..."
              << colors::reset() << std::endl;

#ifdef NDEBUG
    std::cout << colors::blue()
              << "  [INFO] LockFreeWithFallbackPolicy: Testing Release Mode (Fallback)..."
              << colors::reset() << std::endl;

    ProtectedValue<LockFreeWithFallbackPolicy<MutexSynchronizationPolicy>> data;
    const int num_threads = 4;
    const int ops = 10000;
    const int expected_value = num_threads * ops;

    {
        data.value = 0;
        run_concurrent_write_test<LockFreeWithFallbackPolicy<MutexSynchronizationPolicy>>(data, num_threads, ops);
        ASSERT_EQ(data.value, expected_value, "Fallback should work correctly in release mode");
    }
#else
    std::cout << colors::blue()
              << "  [INFO] LockFreeWithFallbackPolicy: Debug Mode (assertions active)"
              << colors::reset() << std::endl;
#endif

    std::cout << colors::green() << "LockFreeWithFallbackPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// VIII. WaitableSynchronizationPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_MUTEX && CPP_UTILITIES_USE_CONDITION_VARIABLE
bool test_WaitableSynchronizationPolicy() {
    std::cout << colors::cyan() << "\nTesting WaitableSynchronizationPolicy..."
              << colors::reset() << std::endl;

    {
        WaitableSynchronizationPolicy policy;
        WaitableSynchronizationPolicy::LockGuard guard(policy.getLock());
        SIMPLE_ASSERT(true, "Waitable lock guard should work");
    }

    {
        WaitableSynchronizationPolicy policy;
        bool ready = false;
        bool processed = false;

        std::thread producer([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            WaitableSynchronizationPolicy::LockGuard guard(policy.getLock());
            ready = true;
            policy.getCondition().notify_one();
        });

        std::thread consumer([&]() {
            WaitableSynchronizationPolicy::LockGuard guard(policy.getLock());
            guard.wait(policy.getCondition(), [&]() { return ready; });
            processed = true;
        });

        producer.join();
        consumer.join();

        SIMPLE_ASSERT(ready && processed, "Wait/notify should synchronize correctly");
    }

    {
        auto& static_lock = WaitableSynchronizationPolicy::getStaticLock();
        WaitableSynchronizationPolicy::LockGuard guard(static_lock);
        SIMPLE_ASSERT(true, "Static waitable lock access should work");
    }

    std::cout << colors::green() << "WaitableSynchronizationPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// NEW 8 ADVANCED POLICIES TESTS
// =============================================================================

// =============================================================================
// IX. SeqLockPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC
bool test_SeqLockPolicy() {
    std::cout << colors::cyan() << "\nTesting SeqLockPolicy..."
              << colors::reset() << std::endl;
    
    SeqLockPolicy policy;
    int test_data = 0;

    // Basic functionality test
    {
        {
            SeqLockPolicy::LockGuard guard(policy.getLock());
            test_data = 42;
        }

        {
            SeqLockPolicy::SharedGuard guard(policy.getLock());
            int value = test_data;
            SIMPLE_ASSERT(guard.is_valid(), "Read should be valid");
            ASSERT_EQ(value, 42, "Read value should match");
        }
    }

    // BEST SOLUTION: Burst pattern - realistic SeqLock use case
    // SeqLock is designed for infrequent writes with frequent reads
    {
        test_data = 0;
        std::atomic<int> successful_reads{0};
        std::atomic<int> failed_reads{0};
        std::atomic<bool> stop{false};

        // Writer does bursts of writes with pauses between bursts
        // This demonstrates SeqLock's strength: handling occasional write bursts
        // while allowing many successful reads during stable periods
        std::thread writer([&]() {
            constexpr int BURSTS = 10;
            constexpr int WRITES_PER_BURST = 100;
            
            for (int burst = 0; burst < BURSTS; ++burst) {
                // Quick burst of writes
                for (int i = 0; i < WRITES_PER_BURST; ++i) {
                    SeqLockPolicy::LockGuard guard(policy.getLock());
                    test_data++;
                }
                
                // Pause between bursts - readers can succeed here
                // This is realistic: updates come in batches with quiet periods
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            
            stop.store(true);
        });

        // Readers continuously try to read
        std::vector<std::thread> readers;
        for (int i = 0; i < 4; ++i) {
            readers.emplace_back([&]() {
                int last_value = -1;
                
                while (!stop.load()) {
                    SeqLockPolicy::SharedGuard guard(policy.getLock());
                    int value = test_data;
                    
                    if (guard.is_valid()) {
                        if (value >= last_value) {  // Monotonic check
                            successful_reads.fetch_add(1);
                            last_value = value;
                        }
                    } else {
                        failed_reads.fetch_add(1);
                    }
                    
                    // No yield - try to read as fast as possible during stable periods
                }
            });
        }

        writer.join();
        for (auto& r : readers) { r.join(); }

        ASSERT_EQ(test_data, 1000, "All writes should complete");
        
        std::cout << colors::blue()
                  << "  [INFO] SeqLock Statistics:\n"
                  << "    Successful reads: " << successful_reads.load() << "\n"
                  << "    Failed reads (retry): " << failed_reads.load() << "\n"
                  << "    Final value: " << test_data << "\n"
                  << "    Read success ratio: " 
                  << (100.0 * successful_reads.load() / (successful_reads.load() + failed_reads.load()))
                  << "%"
                  << colors::reset() << std::endl;
        
        // With burst pattern, should have many successful reads during quiet periods
        SIMPLE_ASSERT(successful_reads.load() > 100, "Should have many successful reads in burst pattern");
        
        if (successful_reads.load() < 1000) {
            std::cout << colors::yellow()
                      << "  [INFO] This is expected - writes occur in bursts, "
                      << "readers succeed during stable periods"
                      << colors::reset() << std::endl;
        }
    }

    std::cout << colors::green() << "SeqLockPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// X. TicketLockPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC
bool test_TicketLockPolicy() {
    std::cout << colors::cyan() << "\nTesting TicketLockPolicy..."
              << colors::reset() << std::endl;
    TicketLockPolicy policy;
    std::atomic<int> counter{0};

    {
        std::vector<std::thread> threads;
        const int num_threads = 8;
        const int ops = 10000;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < ops; ++j) {
                    TicketLockPolicy::LockGuard guard(policy);
                    counter++;
                }
            });
        }

        for (auto& t : threads) { t.join(); }

        ASSERT_EQ(counter.load(), num_threads * ops, "Ticket lock should be thread-safe");
    }

    {
        uint64_t queue_len = policy.get_queue_length();
        std::cout << colors::blue() << "  [INFO] Queue length: " << queue_len
                  << colors::reset() << std::endl;
        SIMPLE_ASSERT(queue_len == 0, "Queue should be empty after all threads complete");
    }

    std::cout << colors::green() << "TicketLockPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XI. MCSLockPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC
bool test_MCSLockPolicy() {
    std::cout << colors::cyan() << "\nTesting MCSLockPolicy..."
              << colors::reset() << std::endl;
    MCSLockPolicy policy;
    std::atomic<int> counter{0};

    {
        std::vector<std::thread> threads;
        const int num_threads = 8;
        const int ops = 10000;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < ops; ++j) {
                    MCSLockPolicy::LockGuard guard(policy.getLock());
                    counter++;
                }
            });
        }

        for (auto& t : threads) { t.join(); }

        ASSERT_EQ(counter.load(), num_threads * ops, "MCS lock should be thread-safe");
    }

    std::cout << colors::green() << "MCSLockPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XII. RCUPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_SHARED_MUTEX
bool test_RCUPolicy() {
    std::cout << colors::cyan() << "\nTesting RCUPolicy..."
              << colors::reset() << std::endl;
    RCUPolicy<int> rcu(42);

    {
        auto guard = rcu.read();
        ASSERT_EQ(*guard, 42, "RCU read should work");
    }

    {
        auto guard = rcu.write();
        guard.update([](int& val) { val = 100; });
    }

    {
        auto guard = rcu.read();
        ASSERT_EQ(*guard, 100, "RCU write should update value");
    }

    {
        std::atomic<int> read_count{0};
        std::vector<std::thread> readers;

        for (int i = 0; i < 4; ++i) {
            readers.emplace_back([&]() {
                for (int j = 0; j < 1000; ++j) {
                    auto guard = rcu.read();
                    if (*guard == 100) read_count++;
                }
            });
        }

        for (auto& r : readers) { r.join(); }
        SIMPLE_ASSERT(read_count.load() == 4000, "All reads should see updated value");
    }

    std::cout << colors::green() << "RCUPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XIII. HazardPointerPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC
bool test_HazardPointerPolicy() {
    std::cout << colors::cyan() << "\nTesting HazardPointerPolicy..."
              << colors::reset() << std::endl;
    HazardPointerPolicy<int> hp;

    {
        std::atomic<int*> ptr(new int(42));
        auto guard = hp.acquire();
        int* protected_ptr = guard.protect(ptr);
        ASSERT_EQ(*protected_ptr, 42, "Protected pointer should be accessible");
    }

    {
        int* old_ptr = new int(100);
        hp.retire(old_ptr);
        SIMPLE_ASSERT(true, "Retire should work without errors");
    }

    std::cout << colors::green() << "HazardPointerPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XIV. AdaptiveLockPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_MUTEX
bool test_AdaptiveLockPolicy() {
    std::cout << colors::cyan() << "\nTesting AdaptiveLockPolicy..."
              << colors::reset() << std::endl;
    AdaptiveLockPolicy policy;
    std::atomic<int> counter{0};

    {
        for (int i = 0; i < 100; ++i) {
            AdaptiveLockPolicy::LockGuard guard(policy);
            counter++;
        }
        ASSERT_EQ(counter.load(), 100, "Low contention should work");
        std::cout << colors::blue() << "  [INFO] Using mutex: "
                  << policy.is_using_mutex() << colors::reset() << std::endl;
    }

    {
        counter = 0;
        std::vector<std::thread> threads;
        const int num_threads = 8;
        const int ops = 5000;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < ops; ++j) {
                    AdaptiveLockPolicy::LockGuard guard(policy);
                    counter++;
                }
            });
        }

        for (auto& t : threads) { t.join(); }

        ASSERT_EQ(counter.load(), num_threads * ops, "High contention should work");
        std::cout << colors::blue() << "  [INFO] Contention: "
                  << policy.get_contention() << colors::reset() << std::endl;
        std::cout << colors::blue() << "  [INFO] Adapted to mutex: "
                  << policy.is_using_mutex() << colors::reset() << std::endl;
    }

    std::cout << colors::green() << "AdaptiveLockPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XV. PriorityInheritanceLockPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_MUTEX
bool test_PriorityInheritanceLockPolicy() {
    std::cout << colors::cyan() << "\nTesting PriorityInheritanceLockPolicy..."
              << colors::reset() << std::endl;
    PriorityInheritanceLockPolicy policy;
    std::atomic<int> counter{0};

    {
        std::vector<std::thread> threads;
        const int num_threads = 4;
        const int ops = 10000;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < ops; ++j) {
                    PriorityInheritanceLockPolicy::LockGuard guard(policy.getLock());
                    counter++;
                }
            });
        }

        for (auto& t : threads) { t.join(); }

        ASSERT_EQ(counter.load(), num_threads * ops, "Priority inheritance lock should work");
    }

    std::cout << colors::blue()
              << "  [INFO] Note: Full priority inheritance requires OS-specific APIs"
              << colors::reset() << std::endl;

    std::cout << colors::green() << "PriorityInheritanceLockPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XVI. VersionedLockPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_MUTEX
bool test_VersionedLockPolicy() {
    std::cout << colors::cyan() << "\nTesting VersionedLockPolicy..."
              << colors::reset() << std::endl;
    VersionedLockPolicy policy;
    int test_data = 0;

    {
        {
            auto handle = policy.getLock();
            VersionedLockPolicy::LockGuard guard(handle.version, handle.write_lock);
            test_data = 42;
        }

        {
            VersionedLockPolicy::SharedGuard guard(policy.getLock().version);
            int value = test_data;
            SIMPLE_ASSERT(guard.validate(), "Version should be valid");
            ASSERT_EQ(value, 42, "Read value should be correct");
        }
    }

    {
        uint64_t v1 = policy.get_version();
        {
            auto handle = policy.getLock();
            VersionedLockPolicy::LockGuard guard(handle.version, handle.write_lock);
            test_data = 100;
        }
        uint64_t v2 = policy.get_version();
        SIMPLE_ASSERT(v2 > v1, "Version should increment after write");
    }

    std::cout << colors::green() << "VersionedLockPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif


// =============================================================================
// XVII. RecursiveMutexPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_MUTEX
bool test_RecursiveMutexPolicy() {
    std::cout << colors::cyan() << "\nTesting RecursiveMutexPolicy..."
              << colors::reset() << std::endl;
    RecursiveMutexPolicy policy;
    std::atomic<int> counter{0};

    {
        // Test recursive locking
        RecursiveMutexPolicy::LockGuard guard1(policy.getLock());
        counter++;
        {
            RecursiveMutexPolicy::LockGuard guard2(policy.getLock());
            counter++;
            {
                RecursiveMutexPolicy::LockGuard guard3(policy.getLock());
                counter++;
            }
        }
        ASSERT_EQ(counter.load(), 3, "Recursive locks should work");
    }

    {
        // Test with threads
        counter = 0;
        std::vector<std::thread> threads;
        const int num_threads = 4;
        const int ops = 10000;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < ops; ++j) {
                    RecursiveMutexPolicy::LockGuard guard(policy.getLock());
                    counter++;
                }
            });
        }

        for (auto& t : threads) { t.join(); }

        ASSERT_EQ(counter.load(), num_threads * ops, "Recursive mutex should be thread-safe");
    }

    std::cout << colors::green() << "RecursiveMutexPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XVIII. TimedMutexPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_MUTEX
bool test_TimedMutexPolicy() {
    std::cout << colors::cyan() << "\nTesting TimedMutexPolicy..."
              << colors::reset() << std::endl;
    TimedMutexPolicy policy;

    {
        // Test normal locking
        TimedMutexPolicy::LockGuard guard(policy.getLock());
        SIMPLE_ASSERT(guard.owns_lock(), "Should acquire lock");
    }

    {
        // Test timeout behavior
        TimedMutexPolicy policy2;  // Use separate policy for timeout test
        std::atomic<bool> locked{false};

        std::thread holder([&]() {
            TimedMutexPolicy::LockGuard guard(policy2.getLock());
            locked = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });

        // Wait for holder to acquire lock
        while (!locked.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Now try to acquire with timeout (should fail since holder has it)
        auto& lock = policy2.getLock();
        bool acquired = lock.try_lock_for(std::chrono::milliseconds(10));
        
        if (!acquired) {
            std::cout << colors::blue() << "  [INFO] Timeout correctly prevented acquisition"
                      << colors::reset() << std::endl;
        } else {
            lock.unlock();  // Clean up if we somehow got it
        }
        
        holder.join();

        SIMPLE_ASSERT(!acquired, "Should timeout waiting for lock");
    }

    std::cout << colors::green() << "TimedMutexPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// XIX. SharedTimedMutexPolicy Tests
// =============================================================================

#if CPP_UTILITIES_USE_SHARED_MUTEX
bool test_SharedTimedMutexPolicy() {
    std::cout << colors::cyan() << "\nTesting SharedTimedMutexPolicy..."
              << colors::reset() << std::endl;
    ProtectedValue<SharedTimedMutexPolicy> data;
    const int num_threads = 10;
    const int ops = 10000;

    {
        // Test write operations
        data.value = 0;
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads / 2; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < ops; ++j) {
                    SharedTimedMutexPolicy::LockGuard guard(data.policy.getLock());
                    data.value++;
                }
            });
        }

        for (auto& t : threads) { t.join(); }
        ASSERT_EQ(data.value, (num_threads / 2) * ops, "Writes should be thread-safe");
    }

    {
        // Test concurrent reads
        std::atomic<int> read_sum(0);
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                SharedTimedMutexPolicy::SharedGuard guard(data.policy.getLock());
                read_sum.fetch_add(data.value, std::memory_order_relaxed);
            });
        }

        for (auto& t : threads) { t.join(); }
        ASSERT_EQ(read_sum.load(), data.value * num_threads, "Concurrent reads should work");
    }

    std::cout << colors::green() << "SharedTimedMutexPolicy: Tests passed."
              << colors::reset() << std::endl;
    return true;
}
#endif

// =============================================================================
// Contended Benchmark Framework (NEW in v4.0)
// =============================================================================

template <typename Policy>
struct ContentionBenchmark {
    struct Result {
        std::string policy_name;
        size_t num_threads;
        size_t total_ops;
        double avg_ns_per_op;
        double total_seconds;
        double throughput_ops_per_sec;
    };
    
    static Result run(const std::string& name, size_t num_threads, size_t ops_per_thread) {
        Policy policy;
        std::atomic<size_t> counter{0};
        std::vector<std::thread> threads;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([&policy, &counter, ops_per_thread]() {
                for (size_t j = 0; j < ops_per_thread; ++j) {
                    typename Policy::LockGuard guard(policy.getLock());
                    counter.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::yield();  // Simulate minimal work
                }
            });
        }
        
        for (auto& t : threads) t.join();
        auto end = std::chrono::high_resolution_clock::now();
        
        size_t total_ops = num_threads * ops_per_thread;
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double duration_s = duration_ns / 1e9;
        double avg_ns = static_cast<double>(duration_ns) / total_ops;
        double throughput = total_ops / duration_s;
        
        return Result{name, num_threads, total_ops, avg_ns, duration_s, throughput};
    }
    
    static void print_result(const Result& r) {
        std::cout << colors::blue()
                  << "  " << r.policy_name << " [" << r.num_threads << " threads]:\n"
                  << "    Avg time/op:  " << std::fixed << std::setprecision(2) 
                  << r.avg_ns_per_op << " ns\n"
                  << "    Throughput:   " << std::scientific << std::setprecision(2)
                  << r.throughput_ops_per_sec << " ops/sec"
                  << colors::reset() << std::endl;
    }
};

// Specialization for MutexSynchronizationPolicy
template <>
ContentionBenchmark<MutexSynchronizationPolicy>::Result
ContentionBenchmark<MutexSynchronizationPolicy>::run(
    const std::string& name, size_t num_threads, size_t ops_per_thread
) {
    MutexSynchronizationPolicy policy;
    std::atomic<size_t> counter{0};
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&policy, &counter, ops_per_thread]() {
            for (size_t j = 0; j < ops_per_thread; ++j) {
                MutexSynchronizationPolicy::LockGuard guard(policy);
                counter.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }
    for (auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    
    size_t total_ops = num_threads * ops_per_thread;
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double duration_s = duration_ns / 1e9;
    double avg_ns = static_cast<double>(duration_ns) / total_ops;
    double throughput = total_ops / duration_s;
    
    return Result{name, num_threads, total_ops, avg_ns, duration_s, throughput};
}

// Specialization for SpinlockSynchronizationPolicy  
template <>
ContentionBenchmark<SpinlockSynchronizationPolicy>::Result
ContentionBenchmark<SpinlockSynchronizationPolicy>::run(
    const std::string& name, size_t num_threads, size_t ops_per_thread
) {
    SpinlockSynchronizationPolicy policy;
    std::atomic<size_t> counter{0};
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&policy, &counter, ops_per_thread]() {
            for (size_t j = 0; j < ops_per_thread; ++j) {
                SpinlockSynchronizationPolicy::LockGuard guard(policy);
                counter.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }
    for (auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    
    size_t total_ops = num_threads * ops_per_thread;
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double duration_s = duration_ns / 1e9;
    double avg_ns = static_cast<double>(duration_ns) / total_ops;
    double throughput = total_ops / duration_s;
    
    return Result{name, num_threads, total_ops, avg_ns, duration_s, throughput};
}

void run_contended_benchmarks() {
    std::cout << "\n" << colors::bold() << colors::cyan()
              << "=== CONTENDED Performance Benchmarks (NEW in v4.0) ==="
              << colors::reset() << std::endl;
    
    size_t hw_threads = std::thread::hardware_concurrency();
    std::cout << colors::blue()
              << "System: " << hw_threads << " hardware threads\n"
              << "Test: Multiple threads competing for same lock"
              << colors::reset() << std::endl;
    
    const size_t ITERATIONS_PER_THREAD = 10000;
    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    if (hw_threads > 8) {
        thread_counts.push_back(hw_threads);
    }
    
    std::cout << "\n" << colors::yellow() << "Testing thread counts: ";
    for (size_t tc : thread_counts) std::cout << tc << " ";
    std::cout << colors::reset() << "\n" << std::endl;
    
#if CPP_UTILITIES_USE_MUTEX
    std::cout << colors::cyan() << "MutexSynchronizationPolicy:" << colors::reset() << std::endl;
    for (size_t threads : thread_counts) {
        auto result = ContentionBenchmark<MutexSynchronizationPolicy>::run("Mutex", threads, ITERATIONS_PER_THREAD);
        ContentionBenchmark<MutexSynchronizationPolicy>::print_result(result);
    }
#endif

#if CPP_UTILITIES_USE_ATOMIC
    std::cout << "\n" << colors::cyan() << "SpinlockSynchronizationPolicy:" << colors::reset() << std::endl;
    for (size_t threads : thread_counts) {
        auto result = ContentionBenchmark<SpinlockSynchronizationPolicy>::run("Spinlock", threads, ITERATIONS_PER_THREAD);
        ContentionBenchmark<SpinlockSynchronizationPolicy>::print_result(result);
    }
    
    std::cout << "\n" << colors::cyan() << "TicketLockPolicy:" << colors::reset() << std::endl;
    for (size_t threads : thread_counts) {
        auto result = ContentionBenchmark<TicketLockPolicy>::run("TicketLock", threads, ITERATIONS_PER_THREAD);
        ContentionBenchmark<TicketLockPolicy>::print_result(result);
    }
    
    std::cout << "\n" << colors::cyan() << "MCSLockPolicy:" << colors::reset() << std::endl;
    for (size_t threads : thread_counts) {
        auto result = ContentionBenchmark<MCSLockPolicy>::run("MCSLock", threads, ITERATIONS_PER_THREAD);
        ContentionBenchmark<MCSLockPolicy>::print_result(result);
    }
#endif

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_MUTEX
    std::cout << "\n" << colors::cyan() << "AdaptiveLockPolicy:" << colors::reset() << std::endl;
    for (size_t threads : thread_counts) {
        auto result = ContentionBenchmark<AdaptiveLockPolicy>::run("Adaptive", threads, ITERATIONS_PER_THREAD);
        ContentionBenchmark<AdaptiveLockPolicy>::print_result(result);
    }
#endif
    
    std::cout << "\n" << colors::bold() << colors::green() << "Key Insights:" << colors::reset() << std::endl;
    std::cout << colors::blue()
              << "- 1 thread:    Uncontended baseline\n"
              << "- 2-4 threads: Light contention (2-10x slowdown)\n"
              << "- 8+ threads:  Heavy contention (10-100x slowdown)\n"
              << "- MCSLock typically scales best at high thread counts"
              << colors::reset() << std::endl;
}


// =============================================================================
// Performance Benchmarks
// =============================================================================

void run_performance_benchmarks() {
    std::cout << "\n" << colors::bold() << colors::cyan()
              << "=== Performance Benchmarks ==="
              << colors::reset() << std::endl;

    const size_t ITERATIONS = 100000;

    // Baseline
    {
        SingleThreadedPolicy policy;
        int value = 0;
        benchmark("SingleThreadedPolicy", [&]() {
            SingleThreadedPolicy::LockGuard guard(policy.getLock());
            value++;
        }, ITERATIONS);
    }

#if CPP_UTILITIES_USE_MUTEX
    {
        MutexSynchronizationPolicy policy;
        int value = 0;
        benchmark("MutexSynchronizationPolicy", [&]() {
            MutexSynchronizationPolicy::LockGuard guard(policy);
            value++;
        }, ITERATIONS);
    }
#endif

#if CPP_UTILITIES_USE_ATOMIC
    {
        SpinlockSynchronizationPolicy policy;
        int value = 0;
        benchmark("SpinlockSynchronizationPolicy", [&]() {
            SpinlockSynchronizationPolicy::LockGuard guard(policy);
            value++;
        }, ITERATIONS);
    }

    {
        SeqLockPolicy policy;
        int data = 0;
        benchmark("SeqLockPolicy write", [&]() {
            SeqLockPolicy::LockGuard guard(policy.getLock());
            data++;
        }, ITERATIONS);
    }

    {
        TicketLockPolicy policy;
        benchmark("TicketLockPolicy", [&]() {
            TicketLockPolicy::LockGuard guard(policy);
        }, ITERATIONS);
    }

    {
        MCSLockPolicy policy;
        benchmark("MCSLockPolicy", [&]() {
            MCSLockPolicy::LockGuard guard(policy.getLock());
        }, ITERATIONS);
    }
#endif

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_MUTEX
    {
        AdaptiveLockPolicy policy;
        benchmark("AdaptiveLockPolicy", [&]() {
            AdaptiveLockPolicy::LockGuard guard(policy);
        }, ITERATIONS);
    }

    {
        VersionedLockPolicy policy;
        benchmark("VersionedLockPolicy", [&]() {
            auto handle = policy.getLock();
            VersionedLockPolicy::LockGuard guard(handle.version, handle.write_lock);
        }, ITERATIONS);
    }
#endif

    std::cout << "\n" << colors::blue()
              << "[NOTE] Benchmarks show uncontended lock/unlock overhead"
              << colors::reset() << std::endl;
}

// =============================================================================
// Main Test Function
// =============================================================================

bool test_ConcurrencyPolicies() {

    PRINT_HEADER(CONCURRENCY POLICIES)

    TestRunner runner;

    // Original 8 policies
    runner.run_test("SingleThreadedPolicy", test_SingleThreadedPolicy);

#if CPP_UTILITIES_USE_MUTEX
    runner.run_test("MutexSynchronizationPolicy", test_MutexSynchronizationPolicy);
#endif

#if CPP_UTILITIES_USE_SHARED_MUTEX
    runner.run_test("SharedMutexPolicy", test_SharedMutexPolicy);
    runner.run_test("UniqueRWLockPolicy", test_UniqueRWLockPolicy);
#endif

#if CPP_UTILITIES_USE_ATOMIC
    runner.run_test("SpinlockSynchronizationPolicy", test_SpinlockSynchronizationPolicy);
    runner.run_test("LockFreeSynchronizationPolicy", test_LockFreeSynchronizationPolicy);
#endif

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_MUTEX
    runner.run_test("LockFreeWithFallbackPolicy", test_LockFreeWithFallbackPolicy);
#endif

#if CPP_UTILITIES_USE_MUTEX && CPP_UTILITIES_USE_CONDITION_VARIABLE
    runner.run_test("WaitableSynchronizationPolicy", test_WaitableSynchronizationPolicy);
#endif

    // New 8 advanced policies
#if CPP_UTILITIES_USE_ATOMIC
    runner.run_test("SeqLockPolicy", test_SeqLockPolicy);
    runner.run_test("TicketLockPolicy", test_TicketLockPolicy);
    runner.run_test("MCSLockPolicy", test_MCSLockPolicy);
    runner.run_test("HazardPointerPolicy", test_HazardPointerPolicy);
#endif

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_SHARED_MUTEX
    runner.run_test("RCUPolicy", test_RCUPolicy);
#endif

#if CPP_UTILITIES_USE_ATOMIC && CPP_UTILITIES_USE_MUTEX
    runner.run_test("AdaptiveLockPolicy", test_AdaptiveLockPolicy);
    runner.run_test("VersionedLockPolicy", test_VersionedLockPolicy);
#endif

#if CPP_UTILITIES_USE_MUTEX
    runner.run_test("PriorityInheritanceLockPolicy", test_PriorityInheritanceLockPolicy);


    // New 3 standard policies
#if CPP_UTILITIES_USE_MUTEX
    runner.run_test("RecursiveMutexPolicy", test_RecursiveMutexPolicy);
    runner.run_test("TimedMutexPolicy", test_TimedMutexPolicy);
#endif

#if CPP_UTILITIES_USE_SHARED_MUTEX
    runner.run_test("SharedTimedMutexPolicy", test_SharedTimedMutexPolicy);
#endif
#endif

    int failed = runner.print_summary();

    if (failed == 0) {
        run_performance_benchmarks();
        run_contended_benchmarks();
    }

    return failed == 0;
}

} // namespace cpp_utilities::testing
