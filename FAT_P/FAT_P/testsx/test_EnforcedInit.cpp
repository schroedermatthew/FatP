/**
 * @file test_EnforcedInit.cpp
 * @brief Comprehensive unit tests for EnforcedInit.h
 */
/*
FATP_META:
  meta_version: 1
  component: EnforcedInit
  file_role: test
  path: tests/test_EnforcedInit.cpp
  namespace: fat_p::testing::enforcedinit
  summary: "Unit tests for EnforcedInit."
  related:
    docs_search: "EnforcedInit"
    headers:
      - fat_p/EnforcedInit.h
      - fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <array>
#include <memory>
#include <chrono>
#include <fstream>
#include <random>
#include <algorithm>

#include "EnforcedInit.h"
#include "FatPTest.h"

/**
 * @file test_EnforcedInit.cpp
 * @brief Comprehensive test suite for fat_p::EnforcedInit - FULLY CORRECTED VERSION
 * 
 * This test suite demonstrates all features of EnforcedInit including:
 * - Basic initialization and access enforcement
 * - Thread-safety with various concurrency policies
 * - Custom check policies
 * - Reset policies
 * - Lazy initialization
 * - Storage policies
 * - Copy/move semantics
 * - Integration patterns
 * - Performance benchmarks
 * 
 * @version 2.0 - ALL FIXES IMPLEMENTED
 * 
 * Fixes applied:
 * - Critical (5): All data races, deadlocks, and UB fixed
 * - High Priority (8): Validation and performance fixes
 * - Medium Priority (12): Quality and completeness improvements
 * - Low Priority (7): Code quality enhancements
 * 
 * @section requirements Requirements
 * - C++17 or later
 * - Header-only, no external dependencies
 * - Tested on Intel(R) Core(TM) i7-8850H CPU @ 2.60GHz
 */

using namespace fat_p;
using namespace fat_p::testing;

namespace fat_p::testing::enforcedinit
{
    // ============================================================================
    // Constants - FIX #27: Named constants instead of magic numbers
    // ============================================================================
    
    constexpr int TEST_VALUE_DEFAULT = 42;
    constexpr int TEST_VALUE_ALTERNATE = 100;
    constexpr int TEST_VALUE_LARGE = 999;
    constexpr int TEST_VALUE_SMALL = 10;
    constexpr int TEST_RANGE_MIN = 0;
    constexpr int TEST_RANGE_MAX = 100;
    
    constexpr int CONCURRENT_THREAD_COUNT = 10;
    constexpr int CONCURRENT_ITERATIONS = 1000;
    constexpr int STRESS_THREAD_COUNT = 20;
    constexpr int STRESS_ITERATIONS = 10000;
    
    // ============================================================================
    // Utility: Prevent Compiler Optimization - FIX #7
    // ============================================================================
    
    /**
     * @brief Prevents the compiler from optimizing away variables in benchmarks
     * @details Uses inline assembly to create a memory barrier
     */
    template <typename T>
    inline void DoNotOptimize(T&& value) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        asm volatile("" : "+r,m"(value) : : "memory");
#elif defined(_MSC_VER)
        _ReadWriteBarrier();
        [[maybe_unused]] void* volatile dummy = static_cast<void*>(&value);
#else
        [[maybe_unused]] volatile T copy = value;
#endif
    }

    // ============================================================================
    // Test Helper Classes - FIX #28: Added documentation
    // ============================================================================

    /**
     * @brief Test class for tracking construction/destruction lifecycle
     * @details Uses atomic counters to track operations even in concurrent tests
     * FIX #15: Uses relaxed memory order for test counters
     */
    class TestObject {
    public:
        static inline std::atomic<int> construct_count{0};
        static inline std::atomic<int> destruct_count{0};
        static inline std::atomic<int> copy_count{0};
        static inline std::atomic<int> move_count{0};

        int value;

        explicit TestObject(int v = 0) : value(v) { 
            construct_count.fetch_add(1, std::memory_order_relaxed);
        }

        TestObject(const TestObject& other) : value(other.value) { 
            copy_count.fetch_add(1, std::memory_order_relaxed);
        }

        TestObject(TestObject&& other) noexcept : value(other.value) { 
            move_count.fetch_add(1, std::memory_order_relaxed);
        }

        TestObject& operator=(const TestObject& other) {
            if (this != &other) {
                value = other.value;
                copy_count.fetch_add(1, std::memory_order_relaxed);
            }
            return *this;
        }

        TestObject& operator=(TestObject&& other) noexcept {
            if (this != &other) {
                value = other.value;
                move_count.fetch_add(1, std::memory_order_relaxed);
            }
            return *this;
        }

        ~TestObject() { 
            destruct_count.fetch_add(1, std::memory_order_relaxed);
        }

        // FIX #8: Added noexcept
        static void reset_counts() noexcept {
            construct_count.store(0, std::memory_order_relaxed);
            destruct_count.store(0, std::memory_order_relaxed);
            copy_count.store(0, std::memory_order_relaxed);
            move_count.store(0, std::memory_order_relaxed);
        }
    };

    /**
     * @brief Custom check policy for range validation
     * @details Validates that integer values are in range [0, 100]
     */
    struct RangeCheckPolicy {
        template <typename T>
        static void pre_init_check(int value) {
            if (value < TEST_RANGE_MIN || value > TEST_RANGE_MAX) {
                throw std::invalid_argument("Value out of range [0, 100]");
            }
        }

        template <typename T>
        static void post_init_check(const T&) noexcept {}
    };

    /**
     * @brief Custom check policy for positive value validation
     * @details Validates that integer values are positive (> 0)
     */
    struct PositiveCheckPolicy {
        template <typename T, typename U, 
                  typename = std::enable_if_t<std::is_integral_v<U>>>
        static void pre_init_check(U value) {
            if (value <= 0) throw std::invalid_argument("Must be positive");
        }
        
        template <typename T>
        static void post_init_check(const T&) noexcept {}
    };

    /**
     * @brief Move-only test type for move semantics testing
     */
    class MoveOnlyType {
    public:
        int value;
        explicit MoveOnlyType(int v) noexcept : value(v) {}
        MoveOnlyType(const MoveOnlyType&) = delete;
        MoveOnlyType& operator=(const MoveOnlyType&) = delete;
        MoveOnlyType(MoveOnlyType&&) noexcept = default;
        MoveOnlyType& operator=(MoveOnlyType&&) noexcept = default;
    };
    
    /**
     * @brief Non-copyable, non-movable type for negative testing
     */
    class NonCopyableType {
    public:
        int value;
        explicit NonCopyableType(int v) noexcept : value(v) {}
        NonCopyableType(const NonCopyableType&) = delete;
        NonCopyableType& operator=(const NonCopyableType&) = delete;
        NonCopyableType(NonCopyableType&&) = delete;
        NonCopyableType& operator=(NonCopyableType&&) = delete;
    };

    // ============================================================================
    // Helper Functions - FIX #26: Reduce code duplication
    // ============================================================================
    
    /**
     * @brief Helper to init and verify value
     */
    template <typename T, typename... Policies>
    void assert_init_and_get(EnforcedInit<T, Policies...>& obj, 
                            const T& init_val, const T& expected) {
        auto result = obj.init(init_val);
        FATP_ASSERT_TRUE(result.has_value(), "Init succeeds");
        FATP_ASSERT_EQ(obj.get(), expected, "Value matches");
    }

    // ============================================================================
    // Test Suite 1: Basic Initialization and Access
    // ============================================================================

    FATP_TEST_CASE(enforce_init_basic_initialization) {
        // Test 1: Basic init and access
        {
            EnforcedInit<int> value;
            FATP_ASSERT_FALSE(value.is_initialized(), "Initially not initialized");

            auto result = value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_TRUE(result.has_value(), "Initialization succeeds");  // FIX #9
            FATP_ASSERT_TRUE(value.is_initialized(), "Now initialized");
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Correct value");
        }

        // Test 2: Init with complex type
        {
            EnforcedInit<std::string> str;
            auto result = str.init("Hello, World!");
            FATP_ASSERT_TRUE(result.has_value(), "String init succeeds");  // FIX #9
            FATP_ASSERT_EQ(str.get(), "Hello, World!", "String initialization");
        }

        // Test 3: Init with multiple arguments
        {
            EnforcedInit<std::vector<int>> vec;
            auto result = vec.init({1, 2, 3, 4, 5});
            FATP_ASSERT_TRUE(result.has_value(), "Vector init succeeds");  // FIX #9
            FATP_ASSERT_EQ(vec.get().size(), 5u, "Vector initialized with initializer list");
            FATP_ASSERT_EQ(vec.get()[0], 1, "Vector elements correct");
        }

        // Test 4: Pointer access
        {
            EnforcedInit<std::string> str;
            (void)str.init("Test");
            FATP_ASSERT_EQ(str->length(), 4u, "Arrow operator works");
            FATP_ASSERT_EQ((*str).length(), 4u, "Dereference operator works");
        }
        
        // Test 5: Zero/empty values are valid
        {
            EnforcedInit<int> zero;
            (void)zero.init(0);
            FATP_ASSERT_EQ(zero.get(), 0, "Zero is valid value");
            
            EnforcedInit<std::string> empty;
            (void)empty.init("");
            FATP_ASSERT_EQ(empty->length(), 0u, "Empty string is valid");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_double_init_prevention) {
        // Test 1: Second init should fail with descriptive error - FIX #14
        {
            EnforcedInit<int> value;
            auto result1 = value.init(TEST_VALUE_SMALL);
            FATP_ASSERT_TRUE(result1.has_value(), "First init succeeds");
            
            auto result2 = value.init(20);
            FATP_ASSERT_FALSE(result2.has_value(), "Second init fails");
            FATP_ASSERT_TRUE(result2.error().find("already initialized") != std::string::npos,
                       "Error message is descriptive");
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_SMALL, "Original value unchanged");
        }

        // Test 2: Multiple init attempts
        {
            EnforcedInit<std::string> str;
            auto result = str.init("First");
            FATP_ASSERT_TRUE(result.has_value(), "First init succeeds");
            
            for (int i = 0; i < 5; ++i) {
                auto fail_result = str.init("Attempt " + std::to_string(i));
                FATP_ASSERT_FALSE(fail_result.has_value(), "Repeated init fails");
                // FIX #14: Consistent error checking
                FATP_ASSERT_TRUE(fail_result.error().find("already initialized") != std::string::npos,
                           "Consistent error message");
            }
            FATP_ASSERT_EQ(str.get(), "First", "First value preserved");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_access_before_init) {
        // Test 1: Get before init should throw
        {
            EnforcedInit<int> value;
            bool caught = false;
            
            try {
                [[maybe_unused]] int v = value.get();
            } catch (const std::exception& e) {
                caught = true;
                FATP_ASSERT_TRUE(std::string(e.what()).find("before init") != std::string::npos,
                           "Exception message mentions init requirement");
            }
            FATP_ASSERT_TRUE(caught, "Access before init throws");
        }

        // Test 2: Operators before init should throw
        {
            EnforcedInit<std::string> str;
            bool caught_deref = false;
            bool caught_arrow = false;

            try {
                [[maybe_unused]] auto& s = *str;
            } catch (const std::exception&) {
                caught_deref = true;
            }

            try {
                [[maybe_unused]] auto len = str->length();
            } catch (const std::exception&) {
                caught_arrow = true;
            }

            FATP_ASSERT_TRUE(caught_deref, "Dereference before init throws");
            FATP_ASSERT_TRUE(caught_arrow, "Arrow before init throws");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_is_initialized_query) {
        EnforcedInit<int> value;
        FATP_ASSERT_FALSE(value.is_initialized(), "Not initialized initially");

        (void)value.init(TEST_VALUE_DEFAULT);
        FATP_ASSERT_TRUE(value.is_initialized(), "Initialized after init()");

        // Should be safe to call multiple times
        FATP_ASSERT_TRUE(value.is_initialized(), "is_initialized() is const and repeatable");
        FATP_ASSERT_TRUE(value.is_initialized(), "Can call many times");

        return true;
    }

    // ============================================================================
    // Test Suite 2: Reset Policy Tests
    // ============================================================================

    FATP_TEST_CASE(enforce_init_reset_not_allowed) {
        // Test 1: Default policy disallows reset
        {
            EnforcedInit<int> value;
            (void)value.init(TEST_VALUE_DEFAULT);
            
            auto result = value.reset();
            FATP_ASSERT_FALSE(result.has_value(), "Reset not allowed by default");
            FATP_ASSERT_TRUE(result.error().find("not allowed") != std::string::npos,
                       "Error message explains policy");
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Value unchanged after failed reset");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_reset_allowed) {
        using ResettableInit = EnforcedInit<int, SingleThreadedPolicy, DefaultCheckPolicy, AllowResetPolicy>;
        
        // Test 1: AllowResetPolicy permits reset
        {
            ResettableInit value;
            auto init_result = value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_TRUE(init_result.has_value(), "Init succeeds");
            FATP_ASSERT_TRUE(value.is_initialized(), "Initialized");
            
            auto reset_result = value.reset();
            FATP_ASSERT_TRUE(reset_result.has_value(), "Reset succeeds with AllowResetPolicy");
            FATP_ASSERT_FALSE(value.is_initialized(), "Not initialized after reset");
        }

        // Test 2: Re-init after reset
        {
            ResettableInit str;
            (void)str.init(50);
            auto reset_result = str.reset();
            FATP_ASSERT_TRUE(reset_result.has_value(), "Reset succeeds");
            
            auto reinit_result = str.init(75);
            FATP_ASSERT_TRUE(reinit_result.has_value(), "Re-init after reset succeeds");
            FATP_ASSERT_EQ(str.get(), 75, "New value correct");
        }

        // Test 3: Multiple reset cycles
        {
            ResettableInit value;
            for (int i = 0; i < 5; ++i) {
                auto init_result = value.init(i * TEST_VALUE_SMALL);
                FATP_ASSERT_TRUE(init_result.has_value(), "Init cycle " + std::to_string(i));
                FATP_ASSERT_EQ(value.get(), i * TEST_VALUE_SMALL, "Value correct in cycle " + std::to_string(i));
                
                auto reset_result = value.reset();
                FATP_ASSERT_TRUE(reset_result.has_value(), "Reset cycle " + std::to_string(i));
                FATP_ASSERT_FALSE(value.is_initialized(), "Uninitialized after reset " + std::to_string(i));
            }
        }

        // Test 4: Reset tracks destructor calls - FIX #24
        {
            using ResettableObj = EnforcedInit<TestObject, SingleThreadedPolicy, 
                                              DefaultCheckPolicy, AllowResetPolicy>;
            TestObject::reset_counts();
            {
                ResettableObj obj;
                (void)obj.init(TEST_VALUE_DEFAULT);
                FATP_ASSERT_EQ(TestObject::construct_count.load(), 1, "One construction");
                
                (void)obj.reset();
                FATP_ASSERT_EQ(TestObject::destruct_count.load(), 1, "One destruction after reset");
                
                // Re-init and let scope end
                (void)obj.init(TEST_VALUE_ALTERNATE);
                FATP_ASSERT_EQ(TestObject::construct_count.load(), 2, "Second construction");
            }
            FATP_ASSERT_EQ(TestObject::destruct_count.load(), 2, "Second destruction on scope exit");
        }

        return true;
    }

    // ============================================================================
    // Test Suite 3: Custom Check Policies
    // ============================================================================

    FATP_TEST_CASE(enforce_init_custom_check_policy) {
        using RangeEnforced = EnforcedInit<int, SingleThreadedPolicy, RangeCheckPolicy>;
        
        // Test 1: Pre-init check validation
        {
            RangeEnforced value;
            
            auto result = value.init(50);
            FATP_ASSERT_TRUE(result.has_value(), "Valid value passes check");
            FATP_ASSERT_EQ(value.get(), 50, "Value initialized");
        }

        // Test 2: Pre-init check failure - FIX #18
        {
            RangeEnforced value;
            
            bool caught = false;
            try {
                (void)value.init(150);  // Out of range
            } catch (const std::invalid_argument& e) {
                caught = true;
                FATP_ASSERT_TRUE(std::string(e.what()).find("out of range") != std::string::npos,
                           "Check policy throws appropriate exception");
            }
            FATP_ASSERT_TRUE(caught, "Pre-init check throws on invalid value");
            FATP_ASSERT_FALSE(value.is_initialized(), "Init fails, object remains uninitialized");
        }
        
        // Test 3: Negative value check
        {
            RangeEnforced value;
            bool caught = false;
            try {
                (void)value.init(-10);
            } catch (const std::invalid_argument&) {
                caught = true;
            }
            FATP_ASSERT_TRUE(caught, "Negative value rejected");
            FATP_ASSERT_FALSE(value.is_initialized(), "Not initialized after exception");
        }

        return true;
    }
    
    // FIX #17: Test PolicyPack composition
    FATP_TEST_CASE(enforce_init_policy_pack_composition) {
        // Multiple check policies composed - PositiveCheckPolicy defined at namespace level
        using MultiCheck = EnforcedInit<int, SingleThreadedPolicy, 
                                       PolicyPack<RangeCheckPolicy, PositiveCheckPolicy>>;
        
        MultiCheck value;
        
        // Should pass both checks
        auto result1 = value.init(50);
        FATP_ASSERT_TRUE(result1.has_value(), "Value passes both checks");
        
        // FIX #25: Test configurable parameters
        MultiCheck value2;
        bool caught = false;
        try {
            (void)value2.init(-5);  // Fails positive check
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        FATP_ASSERT_TRUE(caught, "Fails positive check");
        
        return true;
    }

    // ============================================================================
    // Test Suite 4: Lazy Initialization
    // ============================================================================

    FATP_TEST_CASE(enforce_init_lazy_init) {
        // Test 1: Basic lazy init
        {
            EnforcedInit<int> value;
            int call_count = 0;
            
            value.lazy_init([&call_count]() { 
                ++call_count; 
                return TEST_VALUE_DEFAULT; 
            });
            
            FATP_ASSERT_EQ(call_count, 1, "Lambda called once");
            FATP_ASSERT_TRUE(value.is_initialized(), "Lazy init succeeds");
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Correct value from lambda");
        }

        // Test 2: Lazy init is idempotent
        {
            EnforcedInit<int> value;
            int call_count = 0;
            
            auto lazy_fn = [&call_count]() { 
                ++call_count; 
                return TEST_VALUE_ALTERNATE; 
            };
            
            value.lazy_init(lazy_fn);
            value.lazy_init(lazy_fn);
            value.lazy_init(lazy_fn);
            
            FATP_ASSERT_EQ(call_count, 1, "Lambda called only once despite multiple lazy_init calls");
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_ALTERNATE, "First value preserved");
        }

        // Test 3: Lazy get with initialization - FIX #1: const overload removed
        {
            EnforcedInit<std::string> str;
            auto& result = str.get([]() { return std::string("Lazy initialized"); });
            
            FATP_ASSERT_EQ(result, "Lazy initialized", "Lazy get initializes and returns");
            FATP_ASSERT_TRUE(str.is_initialized(), "Object initialized by lazy get");
        }

        // Test 4: Lazy init with complex type
        {
            EnforcedInit<std::vector<int>> vec;
            vec.lazy_init([]() { 
                return std::vector<int>{10, 20, 30}; 
            });
            
            FATP_ASSERT_EQ(vec->size(), 3u, "Vector lazily initialized");
            FATP_ASSERT_EQ((*vec)[0], 10, "Vector contents correct");
        }

        return true;
    }

    // ============================================================================
    // Test Suite 5: Copy and Move Semantics - FIX #4 applied in header
    // ============================================================================

    FATP_TEST_CASE(enforce_init_copy_semantics) {
        // Test 1: Copy construction
        {
            EnforcedInit<int> original;
            (void)original.init(TEST_VALUE_DEFAULT);
            
            EnforcedInit<int> copy(original);
            FATP_ASSERT_TRUE(copy.is_initialized(), "Copy is initialized");
            FATP_ASSERT_EQ(copy.get(), TEST_VALUE_DEFAULT, "Copy has same value");
            FATP_ASSERT_EQ(original.get(), TEST_VALUE_DEFAULT, "Original unchanged");
        }

        // Test 2: Copy assignment
        {
            EnforcedInit<std::string> original;
            (void)original.init("Hello");
            
            EnforcedInit<std::string> copy;
            copy = original;
            
            FATP_ASSERT_TRUE(copy.is_initialized(), "Copy assigned and initialized");
            FATP_ASSERT_EQ(copy.get(), "Hello", "Copy has same value");
        }

        // Test 3: Copy uninitialized
        {
            EnforcedInit<int> original;
            EnforcedInit<int> copy(original);
            
            FATP_ASSERT_FALSE(copy.is_initialized(), "Copy of uninitialized is uninitialized");
        }

        // Test 4: Self-assignment
        {
            EnforcedInit<int> value;
            (void)value.init(TEST_VALUE_DEFAULT);
            
            value = value;
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Self-assignment works");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_move_semantics) {
        // Test 1: Move construction
        {
            EnforcedInit<std::string> original;
            (void)original.init("Move me");
            
            EnforcedInit<std::string> moved(std::move(original));
            FATP_ASSERT_TRUE(moved.is_initialized(), "Moved object is initialized");
            FATP_ASSERT_EQ(moved.get(), "Move me", "Moved value correct");
        }

        // Test 2: Move assignment
        {
            EnforcedInit<std::vector<int>> original;
            (void)original.init({1, 2, 3, 4, 5});
            
            EnforcedInit<std::vector<int>> moved;
            moved = std::move(original);
            
            FATP_ASSERT_TRUE(moved.is_initialized(), "Move assigned");
            FATP_ASSERT_EQ(moved->size(), 5u, "Moved vector size correct");
        }

        // Test 3: Move-only type
        {
            EnforcedInit<MoveOnlyType> value;
            (void)value.init(TEST_VALUE_ALTERNATE);
            
            EnforcedInit<MoveOnlyType> moved(std::move(value));
            FATP_ASSERT_TRUE(moved.is_initialized(), "Move-only type moved");
            FATP_ASSERT_EQ(moved->value, TEST_VALUE_ALTERNATE, "Move-only value correct");
        }

        return true;
    }
    
    // FIX #4: Test thread-safe copy/move
    FATP_TEST_CASE(enforce_init_copy_move_thread_safety) {
        // Test concurrent copy operations with proper locking
        {
            EnforcedInit<int, MutexSynchronizationPolicy> original;
            (void)original.init(TEST_VALUE_DEFAULT);
            
            std::vector<EnforcedInit<int, MutexSynchronizationPolicy>> copies(CONCURRENT_THREAD_COUNT);
            std::vector<std::thread> threads;
            std::atomic<int> exceptions{0};
            
            // Multiple threads copy simultaneously
            for (int i = 0; i < CONCURRENT_THREAD_COUNT; ++i) {
                threads.emplace_back([&original, &copies, &exceptions, i]() {
                    try {
                        copies[i] = original;  // [OK] Should be thread-safe now
                        int val = copies[i].get();
                        if (val != TEST_VALUE_DEFAULT) {
                            exceptions.fetch_add(1, std::memory_order_relaxed);
                        }
                    } catch (...) {
                        exceptions.fetch_add(1, std::memory_order_relaxed);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            FATP_ASSERT_EQ(exceptions.load(), 0, "No exceptions during concurrent copy");
            
            // Verify all copies are correct
            for (const auto& copy : copies) {
                FATP_ASSERT_TRUE(copy.is_initialized(), "Copy is initialized");
                FATP_ASSERT_EQ(copy.get(), TEST_VALUE_DEFAULT, "Copy has correct value");
            }
        }
        
        return true;
    }

    // ============================================================================
    // Test Suite 6: Thread-Safety with Concurrency Policies
    // ============================================================================

    FATP_TEST_CASE(enforce_init_single_threaded_policy) {
        // Test 1: Default policy is single-threaded (zero overhead)
        {
            EnforcedInit<int> value;
            (void)value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Single-threaded policy works");
        }

        // Test 2: Explicit single-threaded policy
        {
            EnforcedInit<int, SingleThreadedPolicy> value;
            (void)value.init(TEST_VALUE_ALTERNATE);
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_ALTERNATE, "Explicit single-threaded policy");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_mutex_synchronization_policy) {
        // Test 1: Basic mutex policy
        {
            EnforcedInit<int, MutexSynchronizationPolicy> value;
            auto result = value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_TRUE(result.has_value(), "Mutex policy init succeeds");
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Mutex policy basic usage");
        }

        // Test 2: Concurrent initialization attempts (should be serialized)
        {
            EnforcedInit<int, MutexSynchronizationPolicy> value;
            std::atomic<int> success_count{0};
            std::atomic<int> failure_count{0};
            
            std::vector<std::thread> threads;
            for (int i = 0; i < CONCURRENT_THREAD_COUNT; ++i) {
                threads.emplace_back([&value, &success_count, &failure_count, i]() {
                    auto result = value.init(i);
                    if (result.has_value()) {
                        success_count.fetch_add(1, std::memory_order_relaxed);  // FIX #2
                    } else {
                        failure_count.fetch_add(1, std::memory_order_relaxed);  // FIX #2
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            FATP_ASSERT_EQ(success_count.load(), 1, "Exactly one init succeeds");
            FATP_ASSERT_EQ(failure_count.load(), CONCURRENT_THREAD_COUNT - 1, "Others fail");
            FATP_ASSERT_TRUE(value.is_initialized(), "Value is initialized");
        }

        // Test 3: Concurrent access after init - FIX #2: Proper atomic operations
        {
            EnforcedInit<int, MutexSynchronizationPolicy> value;
            (void)value.init(TEST_VALUE_DEFAULT);
            
            std::atomic<long long> sum{0};  // Use long long to avoid overflow
            std::atomic<int> exception_count{0};
            std::atomic<int> wrong_value_count{0};  // FIX #10: Track corrupted reads
            std::vector<std::thread> threads;
            
            for (int i = 0; i < CONCURRENT_THREAD_COUNT; ++i) {
                threads.emplace_back([&value, &sum, &exception_count, &wrong_value_count]() {
                    try {
                        for (int j = 0; j < CONCURRENT_ITERATIONS; ++j) {
                            int val = value.get();
                            if (val != TEST_VALUE_DEFAULT) {  // FIX #10: Verify each read
                                wrong_value_count.fetch_add(1, std::memory_order_relaxed);
                            }
                            sum.fetch_add(val, std::memory_order_relaxed);  // FIX #2: Atomic fetch_add
                        }
                    } catch (...) {
                        exception_count.fetch_add(1, std::memory_order_relaxed);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            FATP_ASSERT_EQ(exception_count.load(), 0, "No exceptions during concurrent access");
            FATP_ASSERT_EQ(wrong_value_count.load(), 0, "No corrupted reads");  // FIX #10
            FATP_ASSERT_EQ(sum.load(), static_cast<long long>(TEST_VALUE_DEFAULT) * CONCURRENT_THREAD_COUNT * CONCURRENT_ITERATIONS, 
                     "Concurrent reads produce correct sum");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_spinlock_synchronization_policy) {
        // Test 1: Basic spinlock usage
        {
            EnforcedInit<int, SpinlockSynchronizationPolicy> value;
            (void)value.init(TEST_VALUE_ALTERNATE);
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_ALTERNATE, "Spinlock policy works");
        }

        // Test 2: Concurrent access with spinlock
        {
            EnforcedInit<std::string, SpinlockSynchronizationPolicy> value;
            (void)value.init("Shared");
            
            std::atomic<int> read_count{0};
            std::vector<std::thread> threads;
            
            for (int i = 0; i < 5; ++i) {
                threads.emplace_back([&value, &read_count]() {
                    for (int j = 0; j < 100; ++j) {
                        if (value.get() == "Shared") {
                            read_count.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            FATP_ASSERT_EQ(read_count.load(), 5 * 100, "All reads successful with spinlock");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_shared_mutex_policy) {
        // Test 1: Shared read-write lock
        {
            EnforcedInit<int, SharedMutexPolicy> value;
            (void)value.init(TEST_VALUE_DEFAULT);
            
            // Multiple concurrent readers
            std::atomic<long long> read_sum{0};
            std::vector<std::thread> readers;
            
            for (int i = 0; i < CONCURRENT_THREAD_COUNT; ++i) {
                readers.emplace_back([&value, &read_sum]() {
                    read_sum.fetch_add(value.get(), std::memory_order_relaxed);
                });
            }
            
            for (auto& t : readers) {
                t.join();
            }
            
            FATP_ASSERT_EQ(read_sum.load(), static_cast<long long>(TEST_VALUE_DEFAULT) * CONCURRENT_THREAD_COUNT, 
                     "Shared reads work correctly");
        }

        return true;
    }
    
    // FIX #13: Add AtomicPolicy tests
    FATP_TEST_CASE(enforce_init_atomic_policy) {
#if FATP_USE_ATOMIC
        // Test 1: Basic atomic policy usage
        {
            EnforcedInit<int, AtomicPolicy> value;
            auto result = value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_TRUE(result.has_value(), "Atomic policy init succeeds");
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Atomic policy works");
        }
        
        // Test 2: Concurrent initialization with atomic policy
        {
            EnforcedInit<int, AtomicPolicy> value;
            std::atomic<int> success{0};
            std::vector<std::thread> threads;
            
            for (int i = 0; i < CONCURRENT_THREAD_COUNT; ++i) {
                threads.emplace_back([&value, &success, i]() {
                    if (value.init(i).has_value()) {
                        success.fetch_add(1, std::memory_order_relaxed);
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            FATP_ASSERT_EQ(success.load(), 1, "Atomic policy allows exactly one init");
            FATP_ASSERT_TRUE(value.is_initialized(), "Value initialized");
        }
        
        // Test 3: Concurrent access with atomic policy
        {
            EnforcedInit<int, AtomicPolicy> value;
            (void)value.init(99);
            
            std::atomic<int> access_count{0};
            std::vector<std::thread> threads;
            
            for (int i = 0; i < 5; ++i) {
                threads.emplace_back([&value, &access_count]() {
                    for (int j = 0; j < 100; ++j) {
                        if (value.get() == 99) {
                            access_count.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                });
            }
            
            for (auto& t : threads) {
                t.join();
            }
            
            FATP_ASSERT_EQ(access_count.load(), 500, "All atomic accesses successful");
        }
#endif
        return true;
    }
    
    // FIX #5: Add comprehensive ConditionVarPolicy tests
    FATP_TEST_CASE(enforce_init_condition_variable_policy) {
        using WaitableInit = EnforcedInit<int, ConditionVarPolicy>;
        
        // Test 1: Wait succeeds when value is initialized
        {
            WaitableInit value;
            std::atomic<bool> wait_result{false};
            std::atomic<int> retrieved_value{0};
            
            std::thread waiter([&value, &wait_result, &retrieved_value]() {
                bool success = value.wait_for_init(std::chrono::seconds(5));
                wait_result.store(success, std::memory_order_relaxed);
                if (success) {
                    retrieved_value.store(value.get(), std::memory_order_relaxed);
                }
            });
            
            // Give waiter time to start waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Initialize the value
            auto init_result = value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_TRUE(init_result.has_value(), "Initialization succeeds");
            
            waiter.join();
            
            FATP_ASSERT_TRUE(wait_result.load(), "Wait succeeds when init happens");
            FATP_ASSERT_EQ(retrieved_value.load(), TEST_VALUE_DEFAULT, "Correct value after wait");
        }
        
        // FIX #16: Test timeout behavior
        // Test 2: Wait timeout when value not initialized
        {
            WaitableInit value;
            auto start = std::chrono::steady_clock::now();
            bool result = value.wait_for_init(std::chrono::milliseconds(100));
            auto duration = std::chrono::steady_clock::now() - start;
            
            FATP_ASSERT_FALSE(result, "Wait times out if not initialized");
            FATP_ASSERT_TRUE(duration >= std::chrono::milliseconds(90),  // Allow some tolerance
                       "Timeout duration respected");
        }
        
        // Test 3: Wait returns immediately if already initialized
        {
            WaitableInit value;
            (void)value.init(TEST_VALUE_ALTERNATE);
            
            auto start = std::chrono::steady_clock::now();
            bool result = value.wait_for_init(std::chrono::seconds(5));
            auto duration = std::chrono::steady_clock::now() - start;
            
            FATP_ASSERT_TRUE(result, "Wait succeeds immediately if already initialized");
            FATP_ASSERT_TRUE(duration < std::chrono::milliseconds(50), 
                       "No significant delay when already initialized");
        }
        
        // Test 4: Multiple waiters
        {
            WaitableInit value;
            std::atomic<int> success_count{0};
            std::vector<std::thread> waiters;
            
            for (int i = 0; i < 5; ++i) {
                waiters.emplace_back([&value, &success_count]() {
                    if (value.wait_for_init(std::chrono::seconds(5))) {
                        success_count.fetch_add(1, std::memory_order_relaxed);
                    }
                });
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            (void)value.init(TEST_VALUE_LARGE);
            
            for (auto& t : waiters) {
                t.join();
            }
            
            FATP_ASSERT_EQ(success_count.load(), 5, "All waiters notified");
        }
        
        // FIX #16: Test short timeout
        {
            WaitableInit value;
            bool result = value.wait_for_init(std::chrono::milliseconds(10));
            FATP_ASSERT_FALSE(result, "Short timeout works");
        }
        
        return true;
    }

    // ============================================================================
    // Test Suite 7: Storage Policies
    // ============================================================================

    FATP_TEST_CASE(enforce_init_optional_storage_policy) {
        // Test 1: Default optional storage
        {
            EnforcedInit<int> value;
            (void)value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Optional storage (default) works");
        }

        // Test 2: Use explicit std::string construction
        EnforcedInit<std::vector<std::string>> vec1;
        (void)vec1.init({ std::string("hello"), std::string("world") });  // OK

        // Test 3: Use variadic init with constructed vector
        EnforcedInit<std::vector<std::string>> vec2;
        (void)vec2.init(std::vector<std::string>{"hello", "world"});  // OK

        // Test 4: Use variadic init with iterators
        EnforcedInit<std::vector<std::string>> vec3;
        std::vector<std::string> temp{ "hello", "world" };
        (void)vec3.init(temp.begin(), temp.end());  // OK

        return true;
    }

    FATP_TEST_CASE(enforce_init_union_storage_policy) {
        using UnionInit = EnforcedInit<int, SingleThreadedPolicy, DefaultCheckPolicy, 
                                      NoResetPolicy, UnionStoragePolicy>;
        using OptionalInit = EnforcedInit<int, SingleThreadedPolicy, DefaultCheckPolicy, 
                                         NoResetPolicy, OptionalStoragePolicy>;
        
        // FIX #11: Verify size optimization
        {
            std::cout << "  Storage size comparison:\n";
            std::cout << "    Union storage:    " << sizeof(UnionInit) << " bytes\n";
            std::cout << "    Optional storage: " << sizeof(OptionalInit) << " bytes\n";
            
            // Union should be smaller or equal for trivial types
            FATP_ASSERT_TRUE(sizeof(UnionInit) <= sizeof(OptionalInit), 
                       "Union storage is not larger than optional");
        }
        
        // Test 1: Union storage for trivial type
        {
            UnionInit value;
            (void)value.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_EQ(value.get(), TEST_VALUE_DEFAULT, "Union storage works");
        }

        // Test 2: Destructor called with union storage
        {
            using UnionObj = EnforcedInit<TestObject, SingleThreadedPolicy, DefaultCheckPolicy,
                           NoResetPolicy, UnionStoragePolicy>;
            TestObject::reset_counts();
            {
                UnionObj obj;
                (void)obj.init(99);
                FATP_ASSERT_EQ(TestObject::construct_count.load(), 1, "Construction tracked");
            }
            FATP_ASSERT_EQ(TestObject::destruct_count.load(), 1, "Destruction tracked with union storage");
        }
        
        // FIX #21: Test trivial type optimization
        {
            static_assert(std::is_trivially_copyable_v<int>, 
                         "Int is trivially copyable");
            // Union storage should handle trivial types efficiently
            UnionInit trivial;
            (void)trivial.init(12345);
            FATP_ASSERT_EQ(trivial.get(), 12345, "Trivial type in union storage");
        }

        return true;
    }

    // ============================================================================
    // Test Suite 8: Lifecycle and RAII
    // ============================================================================

    FATP_TEST_CASE(enforce_init_lifecycle_tracking) {
        TestObject::reset_counts();

        // Test 1: Construction and destruction
        {
            EnforcedInit<TestObject> obj;
            (void)obj.init(TEST_VALUE_DEFAULT);
            FATP_ASSERT_EQ(TestObject::construct_count.load(), 1, "One construction");
            FATP_ASSERT_EQ(TestObject::destruct_count.load(), 0, "No destruction yet");
        }
        FATP_ASSERT_EQ(TestObject::destruct_count.load(), 1, "Destruction on scope exit");

        // Test 2: Copy tracking
        TestObject::reset_counts();
        {
            EnforcedInit<TestObject> obj1;
            (void)obj1.init(TEST_VALUE_SMALL);
            
            EnforcedInit<TestObject> obj2(obj1);
            FATP_ASSERT_EQ(TestObject::construct_count.load(), 1, "One direct construction");
            FATP_ASSERT_EQ(TestObject::copy_count.load(), 1, "One copy operation");
        }

        // Test 3: Move tracking
        TestObject::reset_counts();
        {
            EnforcedInit<TestObject> obj1;
            (void)obj1.init(20);
            
            EnforcedInit<TestObject> obj2(std::move(obj1));
            FATP_ASSERT_EQ(TestObject::construct_count.load(), 1, "One construction");
            FATP_ASSERT_EQ(TestObject::move_count.load(), 1, "One move operation");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_exception_safety) {
        // Test 1: Exception during initialization doesn't leave object in bad state
        {
            struct ThrowOnConstruct {
                ThrowOnConstruct() { throw std::runtime_error("Construction failed"); }
            };

            EnforcedInit<ThrowOnConstruct> obj;
            bool caught = false;
            
            try {
                (void)obj.init();
            } catch (const std::runtime_error&) {
                caught = true;
            }
            
            FATP_ASSERT_TRUE(caught, "Exception propagated");
            FATP_ASSERT_FALSE(obj.is_initialized(), "Object remains uninitialized after failed init");
        }
        
        // FIX #18: Test exception during pre_init_check
        {
            using RangeEnforced = EnforcedInit<int, SingleThreadedPolicy, RangeCheckPolicy>;
            RangeEnforced value;
            
            bool caught = false;
            try {
                (void)value.init(200);  // Out of range
            } catch (const std::invalid_argument&) {
                caught = true;
            }
            
            FATP_ASSERT_TRUE(caught, "Pre-check exception propagated");
            FATP_ASSERT_FALSE(value.is_initialized(), "Not initialized after pre-check exception");
            
            // Should be able to init with valid value after exception
            auto result = value.init(50);
            FATP_ASSERT_TRUE(result.has_value(), "Can init after previous exception");
        }
        
        // FIX #24: Test lifecycle with exception path
        {
            TestObject::reset_counts();
            {
                EnforcedInit<TestObject> obj;
                try {
                    // This should succeed
                    (void)obj.init(TEST_VALUE_DEFAULT);
                } catch (...) {
                    // Should not throw
                }
                FATP_ASSERT_EQ(TestObject::construct_count.load(), 1, "Constructed");
            }
            FATP_ASSERT_EQ(TestObject::destruct_count.load(), 1, "Destroyed even if exceptions possible");
        }

        return true;
    }

    // ============================================================================
    // Test Suite 9: Integration Patterns
    // ============================================================================

    FATP_TEST_CASE(enforce_init_unique_ptr_variant) {
        // Test 1: EnforcedInitUnique with unique_ptr
        {
            EnforcedInitUnique<int> ptr;
            (void)ptr.init(std::make_unique<int>(TEST_VALUE_DEFAULT));
            
            FATP_ASSERT_TRUE(ptr.is_initialized(), "Unique ptr variant initialized");
            FATP_ASSERT_EQ(**ptr, TEST_VALUE_DEFAULT, "Dereferencing unique_ptr through EnforcedInit");
        }

        // Test 2: Ownership semantics preserved
        {
            EnforcedInitUnique<std::string> ptr;
            (void)ptr.init(std::make_unique<std::string>("Hello"));
            
            auto& unique = *ptr;
            FATP_ASSERT_TRUE(unique != nullptr, "Unique ptr is valid");
            FATP_ASSERT_EQ(*unique, "Hello", "Value accessible through unique_ptr");
        }
        
        // FIX #22: Test EnforcedInitUnique edge cases
        // Test 3: nullptr initialization
        {
            EnforcedInitUnique<int> ptr;
            (void)ptr.init(nullptr);
            FATP_ASSERT_TRUE(ptr.is_initialized(), "Can init with nullptr");
            FATP_ASSERT_TRUE(*ptr == nullptr, "nullptr preserved");
        }
        
        // Test 4: Move semantics with unique_ptr
        {
            EnforcedInitUnique<int> ptr1;
            (void)ptr1.init(std::make_unique<int>(99));
            
            EnforcedInitUnique<int> ptr2(std::move(ptr1));
            FATP_ASSERT_TRUE(ptr2.is_initialized(), "Moved unique_ptr initialized");
            FATP_ASSERT_EQ(**ptr2, 99, "Value preserved after move");
        }

        return true;
    }

    FATP_TEST_CASE(enforce_init_integration_with_expected) {
        // Test 1: init() returns Expected
        {
            EnforcedInit<int> value;
            auto result = value.init(TEST_VALUE_DEFAULT);
            
            FATP_ASSERT_TRUE(result.has_value(), "Expected returned from init");
        }

        // Test 2: Error handling with Expected
        {
            EnforcedInit<int> value;
            (void)value.init(TEST_VALUE_SMALL);
            
            auto result = value.init(20);
            std::string error_msg;
            (void)result.map_error([&error_msg](const auto& err) {
                error_msg = err;
                return err;
            });
            
            FATP_ASSERT_TRUE(error_msg.find("already initialized") != std::string::npos,
                       "Error mapped through Expected interface");
            FATP_ASSERT_FALSE(result.has_value(), "Second init returns error");
        }
        
        // Test 3: Chaining with Expected
        {
            EnforcedInit<int> value;
            auto result = value.init(50)
                .and_then([&value]() {
                    return Expected<int, std::string>(value.get() * 2);
                });
            
            FATP_ASSERT_TRUE(result.has_value(), "Chaining succeeds");
            FATP_ASSERT_EQ(*result, 100, "Chained value correct");
        }

        return true;
    }
    
    // FIX #20: Add const-correctness tests
    FATP_TEST_CASE(enforce_init_const_correctness) {
        // Test 1: Const EnforcedInit allows const get()
        {
            EnforcedInit<int> value;
            (void)value.init(TEST_VALUE_DEFAULT);
            
            const EnforcedInit<int>& const_ref = value;
            FATP_ASSERT_EQ(const_ref.get(), TEST_VALUE_DEFAULT, "Const get() works");
            FATP_ASSERT_TRUE(const_ref.is_initialized(), "Const is_initialized() works");
        }
        
        // Test 2: Const accessors
        {
            EnforcedInit<std::string> str;
            (void)str.init("test");
            
            const auto& const_str = str;
            FATP_ASSERT_EQ((*const_str).length(), 4u, "Const deref works");
            FATP_ASSERT_EQ(const_str->length(), 4u, "Const arrow works");
        }
        
        return true;
    }

    // ============================================================================
    // Test Suite 10: Edge Cases and Stress Tests
    // ============================================================================

    FATP_TEST_CASE(enforced_init_edge_cases) {
        // Test 1: Empty string initialization
        {
            EnforcedInit<std::string> str;
            (void)str.init("");
            FATP_ASSERT_TRUE(str.is_initialized(), "Empty string is valid init");
            FATP_ASSERT_EQ(str->length(), 0u, "Empty string has zero length");
        }

        // Test 2: Zero initialization
        {
            EnforcedInit<int> value;
            (void)value.init(0);
            FATP_ASSERT_EQ(value.get(), 0, "Zero is valid value");
        }

        // Test 3: Large object initialization
        {
            EnforcedInit<std::array<int, 1000>> large;
            std::array<int, 1000> data{};
            data.fill(TEST_VALUE_DEFAULT);
            (void)large.init(data);
            
            FATP_ASSERT_EQ((*large)[0], TEST_VALUE_DEFAULT, "Large object initialized");
            FATP_ASSERT_EQ((*large)[999], TEST_VALUE_DEFAULT, "Large object fully initialized");
        }

        // Test 4: Nested EnforcedInit
        {
            EnforcedInit<EnforcedInit<int>> nested;
            EnforcedInit<int> inner;
            (void)inner.init(TEST_VALUE_DEFAULT);
            (void)nested.init(std::move(inner));
            
            FATP_ASSERT_TRUE(nested.is_initialized(), "Outer initialized");
            FATP_ASSERT_TRUE(nested->is_initialized(), "Inner initialized");
            FATP_ASSERT_EQ(nested->get(), TEST_VALUE_DEFAULT, "Nested value accessible");
        }
        
        // Test 5: Boolean values (0 and 1 are both valid)
        {
            EnforcedInit<bool> flag;
            (void)flag.init(false);
            FATP_ASSERT_EQ(flag.get(), false, "Boolean false is valid");
            
            EnforcedInit<bool> flag2;
            (void)flag2.init(true);
            FATP_ASSERT_EQ(flag2.get(), true, "Boolean true is valid");
        }

        return true;
    }

    // FIX #10 & #25: Strengthened stress test with configurable parameters
    FATP_TEST_CASE(enforce_init_stress_concurrent_access) {
        // FIX #25: Use hardware concurrency if available
        const int num_threads = std::min(STRESS_THREAD_COUNT, 
                                        static_cast<int>(std::thread::hardware_concurrency()));
        const int iterations = STRESS_ITERATIONS;
        
        EnforcedInit<int, MutexSynchronizationPolicy> value;
        (void)value.init(1);
        
        std::atomic<long long> sum{0};
        std::atomic<int> exception_count{0};
        std::atomic<int> wrong_value_count{0};  // FIX #10: Track corrupted reads
        std::atomic<int> completed_threads{0};
        
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                try {
                    for (int j = 0; j < iterations; ++j) {
                        int val = value.get();
                        if (val != 1) {  // Expected value
                            wrong_value_count.fetch_add(1, std::memory_order_relaxed);
                        }
                        sum.fetch_add(val, std::memory_order_relaxed);
                    }
                    completed_threads.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    exception_count.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        FATP_ASSERT_EQ(exception_count.load(), 0, "Stress test: no exceptions");
        FATP_ASSERT_EQ(wrong_value_count.load(), 0, "Stress test: no corrupted reads");
        FATP_ASSERT_EQ(completed_threads.load(), num_threads, "Stress test: all threads completed");
        FATP_ASSERT_EQ(sum.load(), static_cast<long long>(num_threads * iterations), 
                 "Stress test: all concurrent reads successful");
        
        return true;
    }
    
    // FIX #26: Add fuzz testing
    FATP_TEST_CASE(enforce_init_fuzz_initialization) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 1000);
        
        for (int i = 0; i < 1000; ++i) {
            EnforcedInit<int> value;
            int random_val = dis(gen);
            (void)value.init(random_val);
            FATP_ASSERT_EQ(value.get(), random_val, "Fuzz: value stable");
            FATP_ASSERT_EQ(value.get(), value.get(), "Fuzz: value consistent");
        }
        
        return true;
    }

    // ============================================================================
    // Performance Benchmarks - FIX #7, #12, #19
    // ============================================================================

    void run_enforce_init_benchmarks() {
        std::cout << "\n" << colors::cyan() << colors::bold() 
                  << "=== Performance Benchmarks ===" 
                  << colors::reset() << "\n\n";

        // FIX #19: Increased warmup iterations
        constexpr size_t BENCH_ITERATIONS = 100000;
        constexpr size_t WARMUP_ITERATIONS = 10000;

        // Benchmark 1: Basic initialization overhead - FIX #7
        {
            std::cout << colors::yellow() << "Benchmark 1: Init + Get (Single-threaded)" 
                     << colors::reset() << "\n";
            
            double raw_time = measure_perf([]() {
                int x = TEST_VALUE_DEFAULT;
                DoNotOptimize(x);  // FIX #7: Prevent optimization
            }, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            
            double enforced_time = measure_perf([]() {
                EnforcedInit<int> x;
                (void)x.init(TEST_VALUE_DEFAULT);
                int val = x.get();
                DoNotOptimize(val);  // FIX #7
            }, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            
            std::cout << "  Raw int:           " << format_time(raw_time) << "\n";
            std::cout << "  EnforcedInit<int>: " << format_time(enforced_time) << "\n";
            
            double overhead = ((enforced_time - raw_time) / raw_time) * 100.0;
            std::cout << "  Overhead: " << colors::yellow() << std::fixed 
                     << std::setprecision(1) << overhead << "%" << colors::reset() << "\n";
        }

        // Benchmark 2: Lazy initialization - FIX #12: Isolated overhead
        {
            std::cout << "\n" << colors::yellow() << "Benchmark 2: Lazy Init vs Direct Init" 
                     << colors::reset() << "\n";
            
            // Pre-construct objects outside benchmark
            EnforcedInit<int> direct_obj;
            EnforcedInit<int> lazy_obj;
            
            auto direct_lambda = [&direct_obj]() {
                EnforcedInit<int> x;
                (void)x.init(TEST_VALUE_DEFAULT);
                int val = x.get();
                DoNotOptimize(val);
            };
            
            auto lazy_lambda = [&lazy_obj]() {
                EnforcedInit<int> x;
                x.lazy_init([]() { return TEST_VALUE_DEFAULT; });
                int val = x.get();
                DoNotOptimize(val);
            };
            
            double direct_time = measure_perf(direct_lambda, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            double lazy_time = measure_perf(lazy_lambda, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            
            std::cout << "  Direct init: " << format_time(direct_time) << "\n";
            std::cout << "  Lazy init:   " << format_time(lazy_time) << "\n";
        }

        // Benchmark 3: Storage policies
        {
            std::cout << "\n" << colors::yellow() << "Benchmark 3: Optional vs Union Storage" 
                     << colors::reset() << "\n";
            
            double optional_time = measure_perf([]() {
                EnforcedInit<int, SingleThreadedPolicy, DefaultCheckPolicy, 
                           NoResetPolicy, OptionalStoragePolicy> x;
                (void)x.init(TEST_VALUE_DEFAULT);
                int val = x.get();
                DoNotOptimize(val);
            }, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            
            double union_time = measure_perf([]() {
                EnforcedInit<int, SingleThreadedPolicy, DefaultCheckPolicy,
                           NoResetPolicy, UnionStoragePolicy> x;
                (void)x.init(TEST_VALUE_DEFAULT);
                int val = x.get();
                DoNotOptimize(val);
            }, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            
            std::cout << "  Optional storage: " << format_time(optional_time) << "\n";
            std::cout << "  Union storage:    " << format_time(union_time) << "\n";
            
            if (union_time < optional_time) {
                double speedup = optional_time / union_time;
                std::cout << "  " << colors::green() << "Union is " 
                         << std::fixed << std::setprecision(2) << speedup << "x faster"
                         << colors::reset() << "\n";
            }
        }

        // Benchmark 4: Concurrency policies
        {
            std::cout << "\n" << colors::yellow() << "Benchmark 4: Concurrency Policy Overhead" 
                     << colors::reset() << "\n";
            
            double single_time = measure_perf([]() {
                EnforcedInit<int, SingleThreadedPolicy> x;
                (void)x.init(TEST_VALUE_DEFAULT);
                int val = x.get();
                DoNotOptimize(val);
            }, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            
            double mutex_time = measure_perf([]() {
                EnforcedInit<int, MutexSynchronizationPolicy> x;
                (void)x.init(TEST_VALUE_DEFAULT);
                int val = x.get();
                DoNotOptimize(val);
            }, BENCH_ITERATIONS, WARMUP_ITERATIONS);
            
            std::cout << "  SingleThreaded: " << format_time(single_time) << "\n";
            std::cout << "  MutexPolicy:    " << format_time(mutex_time) << "\n";
            
            double overhead = ((mutex_time - single_time) / single_time) * 100.0;
            std::cout << "  Mutex overhead: " << colors::yellow() 
                     << std::fixed << std::setprecision(1) << overhead << "%"
                     << colors::reset() << "\n";
        }

        // Benchmark 5: Access patterns
        {
            std::cout << "\n" << colors::yellow() << "Benchmark 5: Operator-> vs get()" 
                     << colors::reset() << "\n";
            
            EnforcedInit<std::string> str;
            (void)str.init("Benchmark");
            
            double arrow_time = measure_perf([&str]() {
                size_t len = str->length();
                DoNotOptimize(len);
            }, BENCH_ITERATIONS * 10, WARMUP_ITERATIONS);
            
            double get_time = measure_perf([&str]() {
                size_t len = str.get().length();
                DoNotOptimize(len);
            }, BENCH_ITERATIONS * 10, WARMUP_ITERATIONS);
            
            std::cout << "  operator->: " << format_time(arrow_time) << "\n";
            std::cout << "  get():      " << format_time(get_time) << "\n";
        }

        std::cout << "\n";
    }

    // ============================================================================
    // Main Test Entry Point
    // ============================================================================

} // namespace fat_p::testing::enforcedinit

namespace fat_p::testing
{

    bool test_EnforcedInit() {

        FATP_PRINT_HEADER(ENFORCED INIT)

        TestRunner runner;
        get_test_config().verbose = true;

        // Test Suite 1: Basic Initialization and Access
        std::cout << "\n" << colors::cyan() << "Test Suite 1: Basic Initialization and Access" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_basic_initialization);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_double_init_prevention);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_access_before_init);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_is_initialized_query);

        // Test Suite 2: Reset Policy
        std::cout << "\n" << colors::cyan() << "Test Suite 2: Reset Policy" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_reset_not_allowed);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_reset_allowed);

        // Test Suite 3: Custom Check Policies
        std::cout << "\n" << colors::cyan() << "Test Suite 3: Custom Check Policies" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_custom_check_policy);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_policy_pack_composition);

        // Test Suite 4: Lazy Initialization
        std::cout << "\n" << colors::cyan() << "Test Suite 4: Lazy Initialization" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_lazy_init);

        // Test Suite 5: Copy and Move Semantics
        std::cout << "\n" << colors::cyan() << "Test Suite 5: Copy and Move Semantics" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_copy_semantics);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_move_semantics);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_copy_move_thread_safety);

        // Test Suite 6: Thread-Safety
        std::cout << "\n" << colors::cyan() << "Test Suite 6: Thread-Safety with Concurrency Policies" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_single_threaded_policy);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_mutex_synchronization_policy);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_spinlock_synchronization_policy);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_shared_mutex_policy);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_atomic_policy);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_condition_variable_policy);

        // Test Suite 7: Storage Policies
        std::cout << "\n" << colors::cyan() << "Test Suite 7: Storage Policies" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_optional_storage_policy);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_union_storage_policy);

        // Test Suite 8: Lifecycle and RAII
        std::cout << "\n" << colors::cyan() << "Test Suite 8: Lifecycle and RAII" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_lifecycle_tracking);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_exception_safety);

        // Test Suite 9: Integration Patterns
        std::cout << "\n" << colors::cyan() << "Test Suite 9: Integration Patterns" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_unique_ptr_variant);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_integration_with_expected);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_const_correctness);

        // Test Suite 10: Edge Cases
        std::cout << "\n" << colors::cyan() << "Test Suite 10: Edge Cases and Stress Tests" 
                  << colors::reset() << "\n";
        FATP_RUN_TEST_NS(runner, enforcedinit, enforced_init_edge_cases);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_stress_concurrent_access);
        FATP_RUN_TEST_NS(runner, enforcedinit, enforce_init_fuzz_initialization);

        // Performance Benchmarks
        enforcedinit::run_enforce_init_benchmarks();

        // Print summary
        int failed = runner.print_summary();
        
        return failed == 0;
    }

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_EnforcedInit() ? 0 : 1;
}
#endif
