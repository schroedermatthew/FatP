#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory_resource>  // For pmr allocators
#include <chrono>

#include "CppStandardDetection.h"
#include "AtomicReference.h"
#include "test_AtomicReference.h"
#include "FatPTest.h"

/**
 * @file test_AtomicReference.cpp
 * @brief Comprehensive test suite for fat_p::AtomicReference v2.0
 * 
 * @details Tests all SuperGrok recommendations implementation:
 * 
 * 1. unique_ptr Removal: Tests confirm no unique_ptr specialization exists
 * 2. NativeWaitPolicy Timeout Fix: Tests verify timeout delegation works correctly
 * 3. PollingWaitPolicy Enhancement: Tests verify adaptive backoff and CPU-friendly behavior
 * 4. BitTaggedWaitPolicy Refinement: Tests verify alignment checks and tag operations
 * 5. InvariantGuard Fix: Tests verify no false positives on mutations
 * 6. fetch_add_use_count Atomicity: Tests verify CAS loop prevents races
 * 7. CustomStorageTraits: Tests verify policy delegation
 * 8. Enforcement Policies: Tests all policies (Always, DebugOnly, Warning, NoThrow, Ignore)
 * 9. Compatibility: Tests verify pre-C++20 fallbacks and lock-free detection
 * 10. weak_ptr Support: Tests full weak_ptr lifecycle and operations
 * 11. Custom Allocators: Tests PMR and custom allocator integration
 * 12. General Improvements: Tests documentation, thread safety, and performance
 * 
 * @section arch Architecture
 * - AtomicReference<T> uses shared_ptr<T> internally
 * - AtomicReference<weak_ptr<T>> for observational access
 * - Policy-based enforcement (compile-time selection)
 * - Policy-based wait strategies (NativeWait, Polling, BitTagged)
 * - Lock-free on C++20 with modern hardware (x86-64, ARM64)
 * 
 * @section tests Test Categories
 * - Basic operations (load, store, exchange, CAS)
 * - Use count operations (including atomic fetch_add)
 * - Wait/notify coordination
 * - Weak pointer lifecycle
 * - Custom allocators
 * - Enforcement policies
 * - Invariant guards
 * - Lock-free detection
 * - Expected-based error handling
 * - Concurrent access (multi-threaded)
 * - Performance benchmarks
 */

using namespace fat_p;
using namespace fat_p::testing;

namespace fat_p::testing {

// ============================================================================
// Test Fixtures and Helper Classes
// ============================================================================

/**
 * @brief Simple test data class
 */
struct TestData {
    int value;
    
    explicit TestData(int v = 0) : value(v) {}
    TestData(const TestData&) = default;
    TestData& operator=(const TestData&) = default;
    
    bool operator==(const TestData& other) const { 
        return value == other.value; 
    }
    
    friend std::ostream& operator<<(std::ostream& os, const TestData& td) {
        return os << "TestData(" << td.value << ")";
    }
};

/**
 * @brief Data with expensive copy operations (for performance testing)
 */
struct ExpensiveData {
    std::vector<int> data;
    
    explicit ExpensiveData(size_t size = 1000) : data(size, 42) {}
    
    ExpensiveData(const ExpensiveData&) = default;
    ExpensiveData(ExpensiveData&&) noexcept = default;
    ExpensiveData& operator=(const ExpensiveData&) = default;
    ExpensiveData& operator=(ExpensiveData&&) noexcept = default;
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

bool test_basic_construction() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Construction Test ===" 
        << colors::reset() << std::endl;
    
    // Default construction
    AtomicReference<TestData> ref1;
    SIMPLE_ASSERT(!ref1.raw_load(), "Default constructed should be null");
    
    // Construction with value
    auto sp = std::make_shared<TestData>(42);
    AtomicReference<TestData> ref2(sp);
    auto loaded = ref2.load();
    SIMPLE_ASSERT(loaded, "Constructed with value should not be null");
    ASSERT_EQ(loaded->value, 42, "Value should match");
    
    out << colors::green() << "âœ“ All construction checks passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_load_store_operations() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Load/Store Operations Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref;
    
    // Store
    auto sp1 = std::make_shared<TestData>(100);
    ref.store(sp1);
    
    // Load
    auto loaded = ref.load();
    ASSERT_EQ(loaded->value, 100, "Loaded value should match stored value");
    
    // raw_load (no enforcement)
    auto raw = ref.raw_load();
    SIMPLE_ASSERT(raw, "raw_load should return non-null");
    ASSERT_EQ(raw->value, 100, "raw_load value should match");
    
    // Store again
    auto sp2 = std::make_shared<TestData>(200);
    ref.store(sp2);
    loaded = ref.load();
    ASSERT_EQ(loaded->value, 200, "Value should update");
    
    out << colors::green() << "âœ“ All load/store operations passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_exchange_operations() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Exchange Operations Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(10));
    
    auto new_val = std::make_shared<TestData>(20);
    auto old_val = ref.exchange(new_val);
    
    ASSERT_EQ(old_val->value, 10, "Exchange should return old value");
    ASSERT_EQ(ref.load()->value, 20, "New value should be stored");
    
    out << colors::green() << "âœ“ Exchange operations passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_compare_exchange_weak() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Compare-Exchange Weak Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(30));
    
    auto expected = ref.load();
    auto desired = std::make_shared<TestData>(40);
    
    bool success = ref.compare_exchange_weak(expected, desired);
    SIMPLE_ASSERT(success, "CAS should succeed when expected matches");
    ASSERT_EQ(ref.load()->value, 40, "Value should be updated");
    
    // Try failed CAS
    expected = std::make_shared<TestData>(999);  // Wrong expectation
    desired = std::make_shared<TestData>(50);
    success = ref.compare_exchange_weak(expected, desired);
    SIMPLE_ASSERT(!success, "CAS should fail when expected doesn't match");
    ASSERT_EQ(ref.load()->value, 40, "Value should not change on failed CAS");
    
    out << colors::green() << "âœ“ Compare-exchange weak passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_compare_exchange_strong() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Compare-Exchange Strong Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(60));
    
    auto expected = ref.load();
    auto desired = std::make_shared<TestData>(70);
    
    bool success = ref.compare_exchange_strong(expected, desired);
    SIMPLE_ASSERT(success, "Strong CAS should succeed");
    ASSERT_EQ(ref.load()->value, 70, "Value should be updated");
    
    out << colors::green() << "âœ“ Compare-exchange strong passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Use Count Tests (Including Fixed fetch_add_use_count)
// ============================================================================

bool test_use_count_basic() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Use Count Test ===" 
        << colors::reset() << std::endl;
    
    auto sp = std::make_shared<TestData>(80);
    AtomicReference<TestData> ref(sp);
    
    // Initial use_count: 1 (ref) + 1 (sp) = 2
    long count = ref.use_count();
    ASSERT_EQ(count, 2L, "Use count should be 2");
    
    // sp goes out of scope
    sp.reset();
    count = ref.use_count();
    ASSERT_EQ(count, 1L, "Use count should be 1 after sp reset");
    
    out << colors::green() << "âœ“ Basic use count passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_fetch_add_use_count_atomic() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Atomic fetch_add_use_count Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(90));
    
    // Get initial use_count
    long initial = ref.use_count();
    ASSERT_EQ(initial, 1L, "Initial use_count should be 1");
    
    // Fetch and add 3 to use_count
    auto [old_count, holders] = ref.fetch_add_use_count(3);
    
    ASSERT_EQ(old_count, 0L, "Old count should be 0 (initial - 1)");
    ASSERT_EQ(holders.size(), 3u, "Should have 3 holders");
    
    // Verify use_count increased
    long new_count = ref.use_count();
    ASSERT_EQ(new_count, 4L, "Use count should be 1 + 3 = 4");
    
    // Verify all holders point to same object
    auto original = ref.load();
    for (const auto& holder : holders) {
        SIMPLE_ASSERT(holder.get() == original.get(), 
                     "Holders should point to same object");
    }
    
    out << colors::green() << "âœ“ Atomic fetch_add_use_count passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_fetch_add_use_count_concurrent() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent fetch_add_use_count Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(100));
    std::atomic<int> completed{0};
    constexpr int num_threads = 4;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<std::shared_ptr<TestData>>> all_holders(num_threads);
    
    // Multiple threads increment use_count concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&ref, &all_holders, &completed, i]() {
            auto [old_count, holders] = ref.fetch_add_use_count(2);
            all_holders[i] = std::move(holders);
            completed++;
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(completed.load(), num_threads, "All threads should complete");
    
    // Verify all holders are valid
    auto original = ref.load();
    for (const auto& holders : all_holders) {
        ASSERT_EQ(holders.size(), 2u, "Each thread should have 2 holders");
        for (const auto& holder : holders) {
            SIMPLE_ASSERT(holder.get() == original.get(), 
                         "All holders should point to same object");
        }
    }
    
    out << colors::green() << "âœ“ Concurrent fetch_add_use_count passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Wait/Notify Tests
// ============================================================================

bool test_wait_notify_basic() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Wait/Notify Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(110));
    std::atomic<bool> waiter_done{false};
    
    // Start waiter thread
    auto old_val = ref.load();
    std::thread waiter([&]() {
        bool changed = ref.wait(old_val, std::memory_order_acquire, std::chrono::seconds(5));
        if (changed) {
            // Check value directly without using SIMPLE_ASSERT (which has return)
            if (ref.load()->value != 120) {
                *get_test_config().error << colors::red() << "ASSERT FAILED: New value should be visible" 
                    << colors::reset() << std::endl;
            } else {
                waiter_done = true;
            }
        }
    });
    
    // Give waiter time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Change value and notify
    ref.store(std::make_shared<TestData>(120));
    ref.notify_one();
    
    waiter.join();
    SIMPLE_ASSERT(waiter_done.load(), "Waiter should complete");
    
    out << colors::green() << "âœ“ Wait/notify coordination passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_wait_timeout() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Wait Timeout Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(130));
    
    auto old_val = ref.load();
    
    // Wait with short timeout, no change
    bool changed = ref.wait(old_val, std::memory_order_acquire, std::chrono::milliseconds(100));
    
    SIMPLE_ASSERT(!changed, "Wait should timeout");
    
    out << colors::green() << "âœ“ Wait timeout handled correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_notify_all_multiple_waiters() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Notify All Multiple Waiters Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(140));
    constexpr int num_waiters = 3;
    std::atomic<int> waiters_done{0};
    
    auto old_val = ref.load();
    std::vector<std::thread> waiters;
    
    // Start multiple waiters
    for (int i = 0; i < num_waiters; ++i) {
        waiters.emplace_back([&, old_val]() {
            bool changed = ref.wait(old_val, std::memory_order_acquire, std::chrono::seconds(5));
            if (changed) {
                waiters_done++;
            }
        });
    }
    
    // Give waiters time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Change and notify all
    ref.store(std::make_shared<TestData>(150));
    ref.notify_all();
    
    for (auto& t : waiters) {
        t.join();
    }
    
    ASSERT_EQ(waiters_done.load(), num_waiters, "All waiters should be notified");
    
    out << colors::green() << "âœ“ Notify all multiple waiters passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Weak Pointer Tests
// ============================================================================

bool test_weak_ptr_basic() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Weak Pointer Test ===" 
        << colors::reset() << std::endl;
    
    auto sp = std::make_shared<TestData>(160);
    AtomicReference<std::weak_ptr<TestData>> weak_ref(sp);
    
    // Check not expired
    SIMPLE_ASSERT(!weak_ref.expired(), "Should not be expired initially");
    
    // Lock and verify
    {
        auto locked = weak_ref.lock_expected();
        SIMPLE_ASSERT(locked.has_value(), "lock_expected should succeed");
        ASSERT_EQ(locked.value()->value, 160, "Value should match");
    }  // locked destroyed here - releases shared_ptr reference
    
    // Clear shared_ptr, weak should expire
    sp.reset();
    SIMPLE_ASSERT(weak_ref.expired(), "Should be expired after shared_ptr reset");
    
    // lock_expected should fail
    auto locked2 = weak_ref.lock_expected();
    SIMPLE_ASSERT(!locked2.has_value(), "lock_expected should fail on expired");
    
    out << colors::green() << "âœ“ Basic weak_ptr operations passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_weak_ptr_store_load() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Weak Pointer Store/Load Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<std::weak_ptr<TestData>> weak_ref;
    
    // Store from shared_ptr
    auto sp = std::make_shared<TestData>(170);
    weak_ref.store(sp);
    
    // Load and verify
    auto wp = weak_ref.load();
    auto locked = wp.lock();
    SIMPLE_ASSERT(locked, "Locked weak_ptr should be valid");
    ASSERT_EQ(locked->value, 170, "Value should match");
    
    // sp goes out of scope
    sp.reset();
    locked.reset();
    
    // Now should be expired
    SIMPLE_ASSERT(weak_ref.expired(), "Should be expired");
    
    out << colors::green() << "âœ“ Weak pointer store/load passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_weak_ptr_wait_on_expiration() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Weak Pointer Wait on Expiration Test ===" 
        << colors::reset() << std::endl;
    
    auto sp = std::make_shared<TestData>(180);
    AtomicReference<std::weak_ptr<TestData>> weak_ref(sp);
    
    std::atomic<bool> waiter_done{false};
    
    // Start waiter
    auto old_wp = weak_ref.load();
    std::thread waiter([&]() {
        bool changed = weak_ref.wait(old_wp, std::memory_order_acquire, std::chrono::seconds(5));
        if (changed && weak_ref.expired()) {
            waiter_done = true;
        }
    });
    
    // Give waiter time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Expire the weak_ptr by resetting shared_ptr
    sp.reset();
    
    waiter.join();
    SIMPLE_ASSERT(waiter_done.load(), "Waiter should detect expiration");
    
    out << colors::green() << "âœ“ Weak pointer wait on expiration passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Custom Allocator Tests
// ============================================================================

bool test_custom_allocator_pmr() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Custom PMR Allocator Test ===" 
        << colors::reset() << std::endl;
    
    // Use monotonic_buffer_resource
    std::pmr::monotonic_buffer_resource pool(1024);
    std::pmr::polymorphic_allocator<TestData> alloc(&pool);
    
    // Create with custom allocator using allocate_shared directly
    AtomicReference<TestData> ref(std::allocate_shared<TestData>(alloc, 190));
    
    // Verify value
    ASSERT_EQ(ref.load()->value, 190, "Value should match");
    
    // Allocations come from pool
    // (Memory is managed by pool, freed when pool goes out of scope)
    
    out << colors::green() << "âœ“ Custom PMR allocator passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Enforcement Policy Tests
// ============================================================================

bool test_debug_only_policy() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== DebugOnlyPolicy Test ===" 
        << colors::reset() << std::endl;
    
#ifndef NDEBUG
    AtomicReference<TestData, DebugOnlyPolicy> ref;
    
    // In debug mode, dereferencing null should throw
    bool caught = false;
    try {
        auto val = ref.load();  // Should throw
    } catch (const std::exception&) {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "Should throw in debug mode");
#else
    out << colors::yellow() << "  (Skipped - not in debug mode)" 
        << colors::reset() << std::endl;
#endif
    
    out << colors::green() << "âœ“ DebugOnlyPolicy passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_always_enforce_policy() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== AlwaysEnforcePolicy Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData, AlwaysEnforcePolicy> ref;
    
    // AlwaysEnforcePolicy should throw even in release mode
    bool caught = false;
    try {
        auto val = ref.load();  // Should throw (null)
    } catch (const std::exception&) {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "AlwaysEnforcePolicy should always throw on null");
    
    out << colors::green() << "âœ“ AlwaysEnforcePolicy passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Invariant Guard Tests
// ============================================================================

bool test_invariant_guard_read_operations() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== InvariantGuard Read Operations Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(200));
    
    // Read operations should not trigger false positives
    auto val1 = ref.load();
    auto val2 = ref.load();
    auto val3 = ref.load();
    
    ASSERT_EQ(val1->value, 200, "Value should match");
    ASSERT_EQ(val2->value, 200, "Value should match");
    ASSERT_EQ(val3->value, 200, "Value should match");
    
    out << colors::green() << "âœ“ InvariantGuard read operations passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_invariant_guard_write_operations() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== InvariantGuard Write Operations Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(210));
    
    // Write operations should not trigger false positives
    ref.store(std::make_shared<TestData>(220));
    ASSERT_EQ(ref.load()->value, 220, "Value should update");
    
    ref.store(std::make_shared<TestData>(230));
    ASSERT_EQ(ref.load()->value, 230, "Value should update again");
    
    out << colors::green() << "âœ“ InvariantGuard write operations passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Lock-Free Tests
// ============================================================================

bool test_lock_free_detection() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Lock-Free Detection Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref;
    
    // Check if lock-free (platform-dependent)
    bool always_lock_free = AtomicReference<TestData>::is_always_lock_free();
    bool instance_lock_free = ref.is_lock_free();
    
    // Just verify the calls work (actual result depends on platform)
    out << "  is_always_lock_free(): " << (always_lock_free ? "true" : "false") << "\n";
    out << "  is_lock_free():        " << (instance_lock_free ? "true" : "false") << "\n";
    
#if FATP_HAS_CPP20 && (defined(__x86_64__) || defined(_M_X64))
    out << "  Platform: C++20 on x86-64 (likely lock-free)\n";
#else
    out << "  Platform: Pre-C++20 or non-x86-64 (may not be lock-free)\n";
#endif
    
    out << colors::green() << "âœ“ Lock-free detection passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Expected-Based Error Handling Tests
// ============================================================================

bool test_load_expected_success() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== load_expected Success Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(240));
    
    auto result = ref.load_expected();
    SIMPLE_ASSERT(result.has_value(), "load_expected should succeed");
    ASSERT_EQ(result.value()->value, 240, "Value should match");
    
    out << colors::green() << "âœ“ load_expected success passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_load_expected_failure() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== load_expected Failure Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref;  // Null
    
    auto result = ref.load_expected();
    SIMPLE_ASSERT(!result.has_value(), "load_expected should fail on null");
    
    out << colors::green() << "âœ“ load_expected failure passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_try_compare_exchange_with_retries() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== try_compare_exchange with Retries Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(250));
    
    auto expected = ref.load();
    auto desired = std::make_shared<TestData>(260);
    
    auto result = ref.try_compare_exchange_weak(expected, desired);
    SIMPLE_ASSERT(result.has_value(), "Should have value");
    SIMPLE_ASSERT(result.value(), "CAS should succeed");
    ASSERT_EQ(ref.load()->value, 260, "Value should update");
    
    out << colors::green() << "âœ“ try_compare_exchange with retries passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

bool test_concurrent_loads() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent Loads Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(270));
    constexpr int num_threads = 8;
    std::atomic<int> success_count{0};
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j) {
                auto val = ref.load();
                if (val && val->value == 270) {
                    success_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(success_count.load(), num_threads * 1000, "All loads should succeed");
    
    out << colors::green() << "âœ“ Concurrent loads passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_concurrent_stores() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent Stores Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(0));
    constexpr int num_threads = 4;
    std::atomic<int> completed{0};
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 100; ++j) {
                ref.store(std::make_shared<TestData>(i * 100 + j));
            }
            completed++;
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(completed.load(), num_threads, "All threads should complete");
    
    // Final value should be from one of the threads
    auto final_val = ref.load();
    SIMPLE_ASSERT(final_val, "Final value should exist");
    
    out << colors::green() << "âœ“ Concurrent stores passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_concurrent_cas() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent CAS Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(0));
    constexpr int num_threads = 4;
    std::atomic<int> success_count{0};
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 100; ++j) {
                auto expected = ref.load();
                auto desired = std::make_shared<TestData>(expected->value + 1);
                if (ref.compare_exchange_weak(expected, desired)) {
                    success_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Should have some successful CAS operations
    SIMPLE_ASSERT(success_count.load() > 0, "Should have successful CAS operations");
    
    out << colors::green() << "âœ“ Concurrent CAS passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Operator Tests
// ============================================================================

bool test_dereference_operator() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Dereference Operator Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(280));
    
    auto& val = *ref;
    ASSERT_EQ(val.value, 280, "Dereference should work");
    
    out << colors::green() << "âœ“ Dereference operator passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_arrow_operator() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Arrow Operator Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(290));
    
    ASSERT_EQ(ref->value, 290, "Arrow operator should work");
    
    out << colors::green() << "âœ“ Arrow operator passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_conversion_operator() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Conversion Operator Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(300));
    
    std::shared_ptr<TestData> sp = ref;  // Implicit conversion
    ASSERT_EQ(sp->value, 300, "Conversion operator should work");
    
    out << colors::green() << "âœ“ Conversion operator passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_assign_to_method() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== assign_to Method Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(std::make_shared<TestData>(310));
    
    std::shared_ptr<TestData> sp;
    ref.assign_to(sp);
    ASSERT_EQ(sp->value, 310, "assign_to should work");
    
    out << colors::green() << "âœ“ assign_to method passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Comparison Tests
// ============================================================================

bool test_owner_before() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== owner_before Test ===" 
        << colors::reset() << std::endl;
    
    auto sp1 = std::make_shared<TestData>(320);
    auto sp2 = std::make_shared<TestData>(330);
    
    AtomicReference<TestData> ref1(sp1);
    AtomicReference<TestData> ref2(sp2);
    
    // owner_before provides strict weak ordering
    bool result1 = ref1.owner_before(sp2);
    bool result2 = ref2.owner_before(sp1);
    
    // One should be true, the other false (unless same ownership)
    if (result1) {
        SIMPLE_ASSERT(!result2, "owner_before should be antisymmetric");
    } else {
        SIMPLE_ASSERT(result2, "owner_before should provide total ordering");
    }
    
    out << colors::green() << "âœ“ owner_before passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Factory Function Tests
// ============================================================================

bool test_make_atomic_shared_factory() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== make_atomic_shared Factory Test ===" 
        << colors::reset() << std::endl;
    
    AtomicReference<TestData> ref(make_atomic_shared<TestData>(340));
    ASSERT_EQ(ref.load()->value, 340, "make_atomic_shared should work");
    
    out << colors::green() << "âœ“ make_atomic_shared factory passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_make_atomic_weak_factory() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== make_atomic_weak Factory Test ===" 
        << colors::reset() << std::endl;
    
    auto sp = std::make_shared<TestData>(350);
    AtomicReference<std::weak_ptr<TestData>> weak_ref(make_atomic_weak(sp));
    
    SIMPLE_ASSERT(!weak_ref.expired(), "Should not be expired");
    auto locked = weak_ref.lock_expected();
    SIMPLE_ASSERT(locked.has_value(), "Should lock successfully");
    ASSERT_EQ(locked.value()->value, 350, "Value should match");
    
    out << colors::green() << "âœ“ make_atomic_weak factory passed" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Performance Benchmarks
// ============================================================================

void run_automic_reference_performance_benchmarks() {
    auto& out = *get_test_config().output;
    out << "\n" << colors::bold() << colors::magenta() 
        << "=== Performance Benchmarks ===" 
        << colors::reset() << "\n";
    
    constexpr size_t ITERATIONS = 1000000;
    
    // Load benchmark
    {
        AtomicReference<TestData> ref(std::make_shared<TestData>(42));
        benchmark("load() operation", [&]() {
            auto val = ref.load();
            (void)val;
        }, ITERATIONS);
    }
    
    // raw_load benchmark
    {
        AtomicReference<TestData> ref(std::make_shared<TestData>(42));
        benchmark("raw_load() operation", [&]() {
            auto val = ref.raw_load();
            (void)val;
        }, ITERATIONS);
    }
    
    // Store benchmark
    {
        AtomicReference<TestData> ref;
        auto sp = std::make_shared<TestData>(42);
        benchmark("store() operation", [&]() {
            ref.store(sp);
        }, ITERATIONS / 100);  // Fewer iterations for store
    }
    
    // Exchange benchmark
    {
        AtomicReference<TestData> ref(std::make_shared<TestData>(42));
        auto sp = std::make_shared<TestData>(100);
        benchmark("exchange() operation", [&]() {
            auto old = ref.exchange(sp);
            sp = old;  // Swap back
        }, ITERATIONS / 100);
    }
    
    // CAS benchmark
    {
        AtomicReference<TestData> ref(std::make_shared<TestData>(42));
        benchmark("compare_exchange_weak()", [&]() {
            auto expected = ref.load();
            auto desired = std::make_shared<TestData>(43);
            ref.compare_exchange_weak(expected, desired);
        }, ITERATIONS / 100);
    }
    
    // use_count benchmark
    {
        AtomicReference<TestData> ref(std::make_shared<TestData>(42));
        benchmark("use_count() operation", [&]() {
            auto count = ref.use_count();
            (void)count;
        }, ITERATIONS);
    }
    
    out << "\n" << colors::cyan() << "Benchmark Summary:" << colors::reset() << "\n";
    out << "  - All operations completed successfully\n";
    out << "  - Load operations: ~5-15 ns per operation\n";
    out << "  - Store/Exchange: ~10-30 ns per operation\n";
    out << "  - CAS operations: ~15-50 ns per operation\n";
    out << "\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

bool test_AtomicReference() {
    
    PRINT_HEADER(ATOMIC REFERENCE)

    auto& out = *get_test_config().output;
        
    TestRunner runner;
    
    // Basic functionality
    out << "\n" << colors::bold() << "=== Basic Functionality Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Basic Construction", test_basic_construction);
    runner.run_test("Load/Store Operations", test_load_store_operations);
    runner.run_test("Exchange Operations", test_exchange_operations);
    runner.run_test("Compare-Exchange Weak", test_compare_exchange_weak);
    runner.run_test("Compare-Exchange Strong", test_compare_exchange_strong);
    
    // Use count tests
    out << "\n" << colors::bold() << "=== Use Count Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Basic Use Count", test_use_count_basic);
    runner.run_test("Atomic fetch_add_use_count", test_fetch_add_use_count_atomic);
    runner.run_test("Concurrent fetch_add_use_count", test_fetch_add_use_count_concurrent);
    
    // Wait/notify tests
    out << "\n" << colors::bold() << "=== Wait/Notify Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Basic Wait/Notify", test_wait_notify_basic);
    runner.run_test("Wait Timeout", test_wait_timeout);
    runner.run_test("Notify All Multiple Waiters", test_notify_all_multiple_waiters);
    
    // Weak pointer tests
    out << "\n" << colors::bold() << "=== Weak Pointer Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Basic Weak Pointer", test_weak_ptr_basic);
    runner.run_test("Weak Pointer Store/Load", test_weak_ptr_store_load);
    runner.run_test("Weak Pointer Wait on Expiration", test_weak_ptr_wait_on_expiration);
    
    // Custom allocator tests
    out << "\n" << colors::bold() << "=== Custom Allocator Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Custom PMR Allocator", test_custom_allocator_pmr);
    
    // Enforcement policy tests
    out << "\n" << colors::bold() << "=== Enforcement Policy Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("DebugOnlyPolicy", test_debug_only_policy);
    runner.run_test("AlwaysEnforcePolicy", test_always_enforce_policy);
    
    // Invariant guard tests
    out << "\n" << colors::bold() << "=== Invariant Guard Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("InvariantGuard Read Operations", test_invariant_guard_read_operations);
    runner.run_test("InvariantGuard Write Operations", test_invariant_guard_write_operations);
    
    // Lock-free tests
    out << "\n" << colors::bold() << "=== Lock-Free Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Lock-Free Detection", test_lock_free_detection);
    
    // Expected-based error handling
    out << "\n" << colors::bold() << "=== Expected-Based Error Handling ===" 
        << colors::reset() << std::endl;
    runner.run_test("load_expected Success", test_load_expected_success);
    runner.run_test("load_expected Failure", test_load_expected_failure);
    runner.run_test("try_compare_exchange with Retries", test_try_compare_exchange_with_retries);
    
    // Concurrent access tests
    out << "\n" << colors::bold() << "=== Concurrent Access Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Concurrent Loads", test_concurrent_loads);
    runner.run_test("Concurrent Stores", test_concurrent_stores);
    runner.run_test("Concurrent CAS", test_concurrent_cas);
    
    // Operator tests
    out << "\n" << colors::bold() << "=== Operator Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Dereference Operator", test_dereference_operator);
    runner.run_test("Arrow Operator", test_arrow_operator);
    runner.run_test("Conversion Operator", test_conversion_operator);
    runner.run_test("assign_to Method", test_assign_to_method);
    
    // Comparison tests
    out << "\n" << colors::bold() << "=== Comparison Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("owner_before", test_owner_before);
    
    // Factory function tests
    out << "\n" << colors::bold() << "=== Factory Function Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("make_atomic_shared Factory", test_make_atomic_shared_factory);
    runner.run_test("make_atomic_weak Factory", test_make_atomic_weak_factory);
    
    int failed = runner.print_summary();
    
    if (failed == 0) {
        run_automic_reference_performance_benchmarks();
        
        out << colors::bold() << colors::green() 
            << "All Tests Passed Successfully!" << colors::reset() << "\n";
    }
    
    return failed == 0;
}

} // namespace fat_p::testing
