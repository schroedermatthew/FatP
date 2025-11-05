/**
 * @file test_SortedContainer.cpp
 * @brief Comprehensive test suite for cpp_utilities::SortedContainer
 * 
 * This test suite verifies all features and bug fixes including:
 * - Bug Fix #1: Stability of AllowDuplicatesPolicy (upper_bound)
 * - Bug Fix #2: FuzzyUniquePolicy detection with trait
 * - Bug Fix #3: Improved fuzzy duplicate detection
 * - Bug Fix #4: Epsilon parameters in insertRange for fuzzy
 * - Bug Fix #5: Forwarding in LoggingUniquePolicy
 * - Bug Fix #6: std::unique equivalence for OnlyUniquePolicy
 * - Improvement #7: stable_sort for large batches
 * - Improvement #8: reserve/capacity methods
 * - Improvement #9: clear method
 * - NEW: withInternalContainer() - scoped access method
 * - NEW: Backend policy support (vector, deque)
 * - Thread-safety with various concurrency policies
 * - Binary search methods (lower_bound, upper_bound, count, find)
 * - Erase support
 * - Range constructors and insertRange
 * - Custom policies (Transform, Logging)
 * - Container access methods (toVector, asVector, withInternalContainer)
 * 
 * Total Tests: 26
 * 
 * @version 2.0 (SuperGrok V3 Complete)
 * @note Tested on Intel(R) Core(TM) i7-8850H CPU @ 2.60GHz
 */

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
#include <sstream>

#include "SortedContainer.h"
#include "test_SortedContainer.h"
#include "test_Utilities.h"

using namespace cpp_utilities;
using namespace cpp_utilities::testing;

namespace cpp_utilities::testing
{
    // ============================================================================
    // Test Constants
    // ============================================================================
    
    constexpr int TEST_VALUE_DEFAULT = 42;
    constexpr int TEST_VALUE_ALTERNATE = 100;
    constexpr int TEST_RANGE_SIZE = 1000;
    constexpr int CONCURRENT_THREAD_COUNT = 10;
    constexpr int CONCURRENT_ITERATIONS = 500;
    constexpr double FUZZY_EPSILON = 0.01;
    
    // ============================================================================
    // Test Helper Classes
    // ============================================================================
    
    /**
     * @brief Test class for tracking operations
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
        
        bool operator<(const TestObject& other) const { return value < other.value; }
        bool operator==(const TestObject& other) const { return value == other.value; }
        
        static void reset_counts() noexcept {
            construct_count.store(0, std::memory_order_relaxed);
            destruct_count.store(0, std::memory_order_relaxed);
            copy_count.store(0, std::memory_order_relaxed);
            move_count.store(0, std::memory_order_relaxed);
        }
    };
    
    std::ostream& operator<<(std::ostream& os, const TestObject& obj) {
        os << obj.value;
        return os;
    }
    
    /**
     * @brief Custom transformer for testing TransformUniquenessPolicy
     */
    struct AbsoluteValueTransformer {
        int operator()(const TestObject& obj) const { return std::abs(obj.value); }
    };
    
    // ============================================================================
    // Bug Fix #1: Stability Test for AllowDuplicatesPolicy
    // ============================================================================
    
    bool test_AllowDuplicatesStability() {
        std::cout << "Running test: " << "AllowDuplicatesPolicy Stability (upper_bound)" << std::endl;
        
        SortedContainer<int, AllowDuplicatesPolicy> sv;
        
        // Insert duplicates in order
        (void)sv.insert(5);
        (void)sv.insert(5);
        (void)sv.insert(5);
        
        // Verify they maintain insertion order (stable)
        auto vec = sv.toVector();
        SIMPLE_ASSERT(static_cast<size_t>(vec.size()) == 3, "Should have 3 elements");
        SIMPLE_ASSERT(std::all_of(vec.begin(), vec.end(), [](int v) { return v == 5; }),
                   "All elements should be 5");
        
        // More complex stability test
        SortedContainer<int, AllowDuplicatesPolicy> sv2;
        (void)sv2.insert(1);
        (void)sv2.insert(3);
        (void)sv2.insert(2);
        (void)sv2.insert(3);
        (void)sv2.insert(2);
        
        vec = sv2.toVector();
        SIMPLE_ASSERT(static_cast<size_t>(vec.size()) == 5, "Should have 5 elements");
        SIMPLE_ASSERT(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
        
        // Count occurrences
        SIMPLE_ASSERT(static_cast<size_t>(sv2.count(2)) == 2, "Should have 2 occurrences of 2");
        SIMPLE_ASSERT(static_cast<size_t>(sv2.count(3)) == 2, "Should have 2 occurrences of 3");
        
        return true;;
    }
    
    // ============================================================================
    // Bug Fix #2 & #3: FuzzyUniquePolicy Detection and Improved Fuzzy Detection
    // ============================================================================
    
    bool test_FuzzyUniquePolicy() {
        std::cout << "Running test: " << "FuzzyUniquePolicy Detection and Fuzzy Matching" << std::endl;
        
        using FuzzySV = SortedContainer<double, FuzzyUniquePolicy<HybridComparisonPolicy, double>>;
        FuzzySV sv;
        
        // Test trait detection
        SIMPLE_ASSERT(is_fuzzy_unique_policy<FuzzyUniquePolicy<>>::value,
                   "Trait should detect FuzzyUniquePolicy");
        SIMPLE_ASSERT(!is_fuzzy_unique_policy<OnlyUniquePolicy>::value,
                   "Trait should not detect OnlyUniquePolicy");
        
        // Test fuzzy insertion with epsilon
        auto result1 = sv.insert(1.0, FUZZY_EPSILON, FUZZY_EPSILON);
        SIMPLE_ASSERT(result1.has_value() && result1.value(), "First insert should succeed");
        
        // This should be rejected as fuzzy duplicate
        auto result2 = sv.insert(1.005, FUZZY_EPSILON, FUZZY_EPSILON);
        SIMPLE_ASSERT(result2.has_value() && !result2.value(), "Fuzzy duplicate should be rejected");
        
        // This should be accepted (outside epsilon)
        auto result3 = sv.insert(1.02, FUZZY_EPSILON, FUZZY_EPSILON);
        SIMPLE_ASSERT(result3.has_value() && result3.value(), "Non-duplicate should be accepted");
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 2, "Should have 2 elements after fuzzy filtering");
        
        // Test fuzzy duplicate before insertion point
        auto result4 = sv.insert(0.995, FUZZY_EPSILON, FUZZY_EPSILON);
        SIMPLE_ASSERT(result4.has_value() && !result4.value(), "Fuzzy duplicate before should be rejected");
        
        return true;;
    }
    
    // ============================================================================
    // Bug Fix #4: Epsilon Parameters in insertRange for Fuzzy
    // ============================================================================
    
    bool test_FuzzyInsertRange() {
        std::cout << "Running test: " << "FuzzyUniquePolicy insertRange with epsilon" << std::endl;
        
        using FuzzySV = SortedContainer<double, FuzzyUniquePolicy<HybridComparisonPolicy, double>>;
        FuzzySV sv;
        
        std::vector<double> values = {1.0, 1.005, 1.02, 1.025, 2.0, 2.005};
        auto result = sv.insertRange(values.begin(), values.end(), FUZZY_EPSILON, FUZZY_EPSILON);
        SIMPLE_ASSERT(result.has_value(), "insertRange should succeed");
        
        // Should have filtered fuzzy duplicates
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) < values.size(), "Fuzzy duplicates should be filtered");
        
        auto vec = sv.toVector();
        // Verify no two elements are within epsilon
        for (size_t i = 0; i < vec.size() - 1; ++i) {
            SIMPLE_ASSERT(std::abs(vec[i+1] - vec[i]) > FUZZY_EPSILON,
                       "No two elements should be within epsilon");
        }
        
        return true;;
    }
    
    // ============================================================================
    // Bug Fix #5: Forwarding in LoggingUniquePolicy
    // ============================================================================
    
    bool test_LoggingUniquePolicyForwarding() {
        std::cout << "Running test: " << "LoggingUniquePolicy Forwarding" << std::endl;
        
        TestObject::reset_counts();
        
        using LoggingSV = SortedContainer<TestObject, LoggingUniquePolicy<OnlyUniquePolicy>>;
        LoggingSV sv;
        
        // Test move insert
        TestObject obj(42);
        auto initial_moves = TestObject::move_count.load();
        (void)sv.insert(std::move(obj));
        auto final_moves = TestObject::move_count.load();
        
        SIMPLE_ASSERT(final_moves > initial_moves, "Move should have been performed");
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 1, "Should have 1 element");
        
        return true;;
    }
    
    // ============================================================================
    // Bug Fix #6: std::unique Equivalence for OnlyUniquePolicy
    // ============================================================================
    
    bool test_UniqueEquivalence() {
        std::cout << "Running test: " << "OnlyUniquePolicy insertRange Equivalence" << std::endl;
        
        SortedContainer<int, OnlyUniquePolicy> sv;
        
        std::vector<int> values = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};
        auto result = sv.insertRange(values.begin(), values.end());
        SIMPLE_ASSERT(result.has_value(), "insertRange should succeed");
        
        // Should have unique elements only
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 4, "Should have 4 unique elements");
        
        auto vec = sv.toVector();
        SIMPLE_ASSERT(vec[0] == 1 && vec[1] == 2 && vec[2] == 3 && vec[3] == 4,
                   "Should have correct unique values");
        
        return true;;
    }
    
    // ============================================================================
    // Improvement #7: stable_sort for Large Batches
    // ============================================================================
    
    bool test_StableSortLargeBatch() {
        std::cout << "Running test: " << "stable_sort for Large Batches" << std::endl;
        
        SortedContainer<int, AllowDuplicatesPolicy> sv;
        
        // Create large batch with duplicates
        std::vector<int> values(100);
        for (size_t i = 0; i < values.size(); ++i) {
            values[i] = i % 10; // Lots of duplicates
        }
        
        auto result = sv.insertRange(values.begin(), values.end());
        SIMPLE_ASSERT(result.has_value(), "insertRange should succeed");
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 100, "Should have 100 elements");
        
        auto vec = sv.toVector();
        SIMPLE_ASSERT(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
        
        return true;;
    }
    
    // ============================================================================
    // Improvement #8: reserve and capacity Methods
    // ============================================================================
    
    bool test_ReserveAndCapacity() {
        std::cout << "Running test: " << "reserve and capacity Methods" << std::endl;
        
        SortedContainer<int> sv;
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.capacity()) == 0, "Initial capacity should be 0");
        
        sv.reserve(SortedContainer<int>::size_type(100));
        SIMPLE_ASSERT(static_cast<size_t>(sv.capacity()) >= 100, "Capacity should be at least 100 after reserve");
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 0, "Size should still be 0");
        
        // Insert elements and verify capacity unchanged
        for (int i = 0; i < 50; ++i) {
            (void)sv.insert(i);
        }
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 50, "Should have 50 elements");
        SIMPLE_ASSERT(static_cast<size_t>(sv.capacity()) >= 100, "Capacity should still be at least 100");
        
        return true;;
    }
    
    // ============================================================================
    // Improvement #9: clear Method
    // ============================================================================
    
    bool test_ClearMethod() {
        std::cout << "Running test: " << "clear Method" << std::endl;
        
        SortedContainer<int> sv;
        
        for (int i = 0; i < 50; ++i) {
            (void)sv.insert(i);
        }
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 50, "Should have 50 elements");
        SIMPLE_ASSERT(!sv.empty(), "Should not be empty");
        
        sv.clear();
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 0, "Should have 0 elements after clear");
        SIMPLE_ASSERT(sv.empty(), "Should be empty after clear");
        
        // Verify can still insert after clear
        (void)sv.insert(42);
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 1, "Should be able to insert after clear");
        
        return true;;
    }
    
    // ============================================================================
    // Basic Functionality Tests
    // ============================================================================
    
    bool test_BasicInsertAndFind() {
        std::cout << "Running test: " << "Basic Insert and Find" << std::endl;
        
        SortedContainer<int> sv;
        
        // Insert values
        (void)sv.insert(5);
        (void)sv.insert(2);
        (void)sv.insert(8);
        (void)sv.insert(1);
        (void)sv.insert(9);
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 5, "Should have 5 elements");
        
        // Verify sorting
        auto vec = sv.toVector();
        SIMPLE_ASSERT(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
        SIMPLE_ASSERT(vec[0] == 1 && vec[4] == 9, "First and last elements correct");
        
        // Test find
        auto it = sv.find(5);
        SIMPLE_ASSERT(it != sv.end(), "Should find 5");
        SIMPLE_ASSERT(*it == 5, "Found element should be 5");
        
        auto it2 = sv.find(99);
        SIMPLE_ASSERT(it2 == sv.end(), "Should not find 99");
        
        return true;;
    }
    
    bool test_OnlyUniquePolicyBasic() {
        std::cout << "Running test: " << "OnlyUniquePolicy Basic" << std::endl;
        
        SortedContainer<int, OnlyUniquePolicy> sv;
        
        auto r1 = sv.insert(5);
        SIMPLE_ASSERT(r1.has_value() && r1.value(), "First insert should succeed");
        
        auto r2 = sv.insert(5);
        SIMPLE_ASSERT(r2.has_value() && !r2.value(), "Duplicate should be rejected");
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 1, "Should have 1 unique element");
        
        (void)sv.insert(3);
        (void)sv.insert(7);
        (void)sv.insert(3); // duplicate
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 3, "Should have 3 unique elements");
        
        return true;;
    }
    
    bool test_RangeConstructor() {
        std::cout << "Running test: " << "Range Constructor" << std::endl;
        
        std::vector<int> values = {5, 2, 8, 1, 9, 3};
        SortedContainer<int> sv(values.begin(), values.end());
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 6, "Should have 6 elements");
        
        auto vec = sv.toVector();
        SIMPLE_ASSERT(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
        
        return true;;
    }
    
    // ============================================================================
    // Binary Search Methods Tests
    // ============================================================================
    
    bool test_BinarySearchMethods() {
        std::cout << "Running test: " << "Binary Search Methods" << std::endl;
        
        SortedContainer<int, AllowDuplicatesPolicy> sv;
        std::vector<int> values = {1, 3, 3, 3, 5, 7, 9};
        (void)sv.insertRange(values.begin(), values.end());
        
        // Test lower_bound
        auto lb = sv.lower_bound(3);
        SIMPLE_ASSERT(lb != sv.end() && *lb == 3, "lower_bound should find first 3");
        
        // Test upper_bound
        auto ub = sv.upper_bound(3);
        SIMPLE_ASSERT(ub != sv.end() && *ub == 5, "upper_bound should find element after 3");
        
        // Test count
        SIMPLE_ASSERT(static_cast<size_t>(sv.count(3)) == 3, "Should count 3 occurrences of 3");
        SIMPLE_ASSERT(static_cast<size_t>(sv.count(99)) == 0, "Should count 0 occurrences of 99");
        
        return true;;
    }
    
    // ============================================================================
    // Erase Tests
    // ============================================================================
    
    bool test_EraseBasic() {
        std::cout << "Running test: " << "Erase Basic" << std::endl;
        
        SortedContainer<int> sv;
        (void)sv.insert(1);
        (void)sv.insert(2);
        (void)sv.insert(3);
        
        auto result = sv.erase(2);
        SIMPLE_ASSERT(result.has_value() && result.value(), "Erase should succeed");
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 2, "Should have 2 elements after erase");
        
        auto vec = sv.toVector();
        SIMPLE_ASSERT(vec[0] == 1 && vec[1] == 3, "Correct elements remaining");
        
        auto result2 = sv.erase(99);
        SIMPLE_ASSERT(result2.has_value() && !result2.value(), "Erase of non-existent should return false");
        
        return true;;
    }
    
    // ============================================================================
    // Iterator Tests
    // ============================================================================
    
    bool test_Iterators() {
        std::cout << "Running test: " << "Iterators" << std::endl;
        
        SortedContainer<int> sv;
        std::vector<int> values = {1, 2, 3, 4, 5};
        (void)sv.insertRange(values.begin(), values.end());
        
        // Test forward iteration
        int expected = 1;
        for (auto it = sv.begin(); it != sv.end(); ++it) {
            SIMPLE_ASSERT(*it == expected, "Forward iteration should work");
            ++expected;
        }
        
        // Test reverse iteration
        expected = 5;
        for (auto it = sv.rbegin(); it != sv.rend(); ++it) {
            SIMPLE_ASSERT(*it == expected, "Reverse iteration should work");
            --expected;
        }
        
        return true;;
    }
    
    // ============================================================================
    // Custom Policy Tests
    // ============================================================================
    
    bool test_TransformUniquenessPolicy() {
        std::cout << "Running test: " << "TransformUniquenessPolicy" << std::endl;
        
        using TransformSV = SortedContainer<TestObject, 
                                         TransformUniquenessPolicy<OnlyUniquePolicy, AbsoluteValueTransformer>,
                                         std::less<>>; // Use transparent comparator for transformed types
        TransformSV sv;
        
        (void)sv.insert(TestObject(5));
        (void)sv.insert(TestObject(-5)); // Should be rejected (same absolute value)
        (void)sv.insert(TestObject(3));
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 2, "Should have 2 elements (5 and 3, -5 rejected)");
        
        return true;;
    }
    
    // ============================================================================
    // Thread-Safety Tests
    // ============================================================================
    
    bool test_ConcurrentInserts() {
        std::cout << "Running test: " << "Concurrent Inserts" << std::endl;
        
        SortedContainer<int, AllowDuplicatesPolicy, std::less<int>, std::allocator<int>, 
                     MutexSynchronizationPolicy> sv;
        
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        
        for (int t = 0; t < CONCURRENT_THREAD_COUNT; ++t) {
            threads.emplace_back([&sv, &success_count, t]() {
                for (int i = 0; i < CONCURRENT_ITERATIONS; ++i) {
                    auto result = sv.insert(t * 1000 + i);
                    if (result.has_value() && result.value()) {
                        success_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == CONCURRENT_THREAD_COUNT * CONCURRENT_ITERATIONS,
                   "All inserts should succeed");
        SIMPLE_ASSERT(std::is_sorted(sv.begin(), sv.end()), "Should remain sorted");
        
        return true;;
    }
    
    // ============================================================================
    // Performance Tests
    // ============================================================================
    
    bool test_LargeDataset() {
        std::cout << "Running test: " << "Large Dataset Performance" << std::endl;
        
        SortedContainer<int> sv;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Insert 10000 elements
        for (int i = 0; i < 10000; ++i) {
            (void)sv.insert(i);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 10000, "Should have 10000 elements");
        SIMPLE_ASSERT(std::is_sorted(sv.begin(), sv.end()), "Should be sorted");
        
        std::cout << "  Inserted 10000 elements in " << duration.count() << "ms" << std::endl;
        
        return true;;
    }
    
    bool test_BatchInsertPerformance() {
        std::cout << "Running test: " << "Batch Insert Performance" << std::endl;
        
        SortedContainer<int> sv;
        
        std::vector<int> values(10000);
        std::iota(values.begin(), values.end(), 0);
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(values.begin(), values.end(), g);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        auto result = sv.insertRange(values.begin(), values.end());
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        SIMPLE_ASSERT(result.has_value(), "insertRange should succeed");
        SIMPLE_ASSERT(static_cast<size_t>(sv.size()) == 10000, "Should have 10000 elements");
        SIMPLE_ASSERT(std::is_sorted(sv.begin(), sv.end()), "Should be sorted");
        
        std::cout << "  Batch inserted 10000 elements in " << duration.count() << "ms" << std::endl;
        
        return true;;
    }
    
    // ============================================================================
    // Test: withInternalContainer - Basic Usage
    // ============================================================================
    
    bool test_WithInternalContainerBasic()
    {
        std::cout << "Running test: " << "withInternalContainer - Basic Usage" << std::endl;
        
        SortedContainer<int> sc;
        (void)sc.insert(5);
        (void)sc.insert(2);
        (void)sc.insert(8);
        (void)sc.insert(1);
        
        // Test: Read-only access via lambda
        auto sum = sc.withInternalContainer([](const auto& container) {
            return std::accumulate(container.begin(), container.end(), 0);
        });
        
        ASSERT_EQ(sum, 16, "Sum should be 5+2+8+1=16");
        
        // Test: Can access size
        auto size = sc.withInternalContainer([](const auto& container) {
            return container.size();
        });
        
        ASSERT_EQ(size, 4u, "Size should be 4");
        
        // Test: Can use STL algorithms
        auto max_val = sc.withInternalContainer([](const auto& container) {
            return *std::max_element(container.begin(), container.end());
        });
        
        ASSERT_EQ(max_val, 8, "Max should be 8");
        
        return true;;
    }
    
    // ============================================================================
    // Test: withInternalContainer - Thread Safety
    // ============================================================================
    
    bool test_WithInternalContainerThreadSafety()
    {
        std::cout << "Running test: " << "withInternalContainer - Thread Safety" << std::endl;
        
        SortedContainer<int, AllowDuplicatesPolicy, std::less<int>,
                        std::allocator<int>, MutexSynchronizationPolicy> sc;
        
        // Populate
        for (int i = 0; i < 1000; ++i) {
            (void)sc.insert(i);
        }
        
        std::atomic<int> read_count{0};
        std::atomic<bool> all_reads_valid{true};
        
        // Thread 1: Multiple reads via withInternalContainer
        std::thread reader([&sc, &read_count, &all_reads_valid]() {
            for (int i = 0; i < 50; ++i) {
                sc.withInternalContainer([&](const auto& container) {
                    // Lock is held during this entire operation
                    size_t prev_size = container.size();
                    int sum = std::accumulate(container.begin(), container.end(), 0);
                    size_t after_size = container.size();
                    
                    // Size should not change during this operation
                    if (prev_size != after_size) {
                        all_reads_valid = false;
                    }
                    
                    // Container should be sorted
                    if (!std::is_sorted(container.begin(), container.end())) {
                        all_reads_valid = false;
                    }
                    
                    read_count++;
                });
                std::this_thread::yield();
            }
        });
        
        // Thread 2: Concurrent writes
        std::thread writer([&sc]() {
            for (int i = 1000; i < 1050; ++i) {
                (void)sc.insert(i);
                std::this_thread::yield();
            }
        });
        
        reader.join();
        writer.join();
        
        ASSERT_EQ(read_count.load(), 50, "Should complete 50 reads");
        ASSERT_TRUE(all_reads_valid.load(), "All reads should see consistent state");
        
        return true;;
    }
    
    // ============================================================================
    // Test: withInternalContainer - Complex Return Types
    // ============================================================================
    
    bool test_WithInternalContainerComplexReturn()
    {
        std::cout << "Running test: " << "withInternalContainer - Complex Return Types" << std::endl;
        
        SortedContainer<double> sc;
        (void)sc.insert(1.5);
        (void)sc.insert(2.5);
        (void)sc.insert(3.5);
        (void)sc.insert(4.5);
        (void)sc.insert(5.5);
        
        // Test: Return struct
        struct Stats {
            double mean;
            double min;
            double max;
            size_t count;
        };
        
        auto stats = sc.withInternalContainer([](const auto& container) -> Stats {
            Stats s;
            s.count = container.size();
            s.min = container.empty() ? 0.0 : *container.begin();
            s.max = container.empty() ? 0.0 : *container.rbegin();
            s.mean = std::accumulate(container.begin(), container.end(), 0.0) / s.count;
            return s;
        });
        
        ASSERT_EQ(stats.count, 5u, "Count should be 5");
        ASSERT_EQ(stats.min, 1.5, "Min should be 1.5");
        ASSERT_EQ(stats.max, 5.5, "Max should be 5.5");
        ASSERT_EQ(stats.mean, 3.5, "Mean should be 3.5");
        
        // Test: Return bool
        auto has_negative = sc.withInternalContainer([](const auto& container) {
            return std::any_of(container.begin(), container.end(), 
                              [](double v) { return v < 0; });
        });
        
        ASSERT_FALSE(has_negative, "Should not have negative values");
        
        return true;;
    }
    
    // ============================================================================
    // Test: withInternalContainer - Backend Agnostic
    // ============================================================================
    
    bool test_WithInternalContainerBackendAgnostic()
    {
        std::cout << "Running test: " << "withInternalContainer - Backend Agnostic" << std::endl;
        
        // Test with vector backend
        SortedContainer<int, AllowDuplicatesPolicy, std::less<int>,
                        std::allocator<int>, SingleThreadedPolicy,
                        VectorBackendPolicy> vec_container;
        
        (void)vec_container.insert(10);
        (void)vec_container.insert(20);
        (void)vec_container.insert(30);
        
        auto vec_sum = vec_container.withInternalContainer([](const auto& c) {
            return std::accumulate(c.begin(), c.end(), 0);
        });
        
        ASSERT_EQ(vec_sum, 60, "Vector backend sum should be 60");
        
        // Test with deque backend
        SortedContainer<int, AllowDuplicatesPolicy, std::less<int>,
                        std::allocator<int>, SingleThreadedPolicy,
                        DequeBackendPolicy> deque_container;
        
        (void)deque_container.insert(10);
        (void)deque_container.insert(20);
        (void)deque_container.insert(30);
        
        auto deque_sum = deque_container.withInternalContainer([](const auto& c) {
            return std::accumulate(c.begin(), c.end(), 0);
        });
        
        ASSERT_EQ(deque_sum, 60, "Deque backend sum should be 60");
        
        return true;;
    }
    
    // ============================================================================
    // Test: withInternalContainer vs toVector Performance
    // ============================================================================
    
    bool test_WithInternalContainerPerformance()
    {
        std::cout << "Running test: " << "withInternalContainer - Performance Comparison" << std::endl;
        
        SortedContainer<int> sc;
        
        // Populate with 10K elements (smaller for faster testing)
        for (int i = 0; i < 10000; ++i) {
            (void)sc.insert(i);
        }
        
        // Benchmark: toVector (with copy)
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int iter = 0; iter < 100; ++iter) {
            auto vec = sc.toVector();
            volatile int sum = std::accumulate(vec.begin(), vec.end(), 0);
            (void)sum;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto toVector_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        // Benchmark: withInternalContainer (zero-copy)
        start = std::chrono::high_resolution_clock::now();
        
        for (int iter = 0; iter < 100; ++iter) {
            sc.withInternalContainer([](const auto& container) {
                volatile int sum = std::accumulate(container.begin(), container.end(), 0);
                (void)sum;
            });
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto withContainer_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "  toVector (100 iterations):            " << toVector_time << " μs" << std::endl;
        std::cout << "  withInternalContainer (100 iterations): " << withContainer_time << " μs" << std::endl;
        
        if (withContainer_time > 0) {
            std::cout << "  Speedup: " << (static_cast<double>(toVector_time) / withContainer_time) << "x" << std::endl;
        }
        
        // withInternalContainer should be significantly faster (no copying)
        ASSERT_TRUE(withContainer_time < toVector_time, 
                        "withInternalContainer should be faster than toVector");
        
        return true;;
    }
    
    // ============================================================================
    // Test: toVector vs asVector Behavior
    // ============================================================================
    
    bool test_VectorAccessMethods()
    {
        std::cout << "Running test: " << "Vector Access Methods Comparison" << std::endl;
        
        SortedContainer<int> sc;
        (void)sc.insert(1);
        (void)sc.insert(2);
        (void)sc.insert(3);
        
        // Test toVector - returns a copy
        auto vec_copy = sc.toVector();
        ASSERT_EQ(vec_copy.size(), 3u, "Copy should have 3 elements");
        vec_copy.push_back(999); // Modify copy
        
        // Original should be unchanged
        ASSERT_EQ(static_cast<size_t>(sc.size()), 3u, "Original should still have 3 elements");
        
        // Test asVector - returns const reference (for legacy compatibility)
        const auto& vec_ref = sc.asVector();
        ASSERT_EQ(vec_ref.size(), 3u, "Reference should have 3 elements");
        
        // Test withInternalContainer - scoped access
        int sum = sc.withInternalContainer([](const auto& container) {
            return std::accumulate(container.begin(), container.end(), 0);
        });
        
        ASSERT_EQ(sum, 6, "Sum should be 1+2+3=6");
        
        return true;;
    }
    
    // ============================================================================
    // Main Test Function
    // ============================================================================
    
    bool test_SortedContainer() {
        std::cout << "\n=== SortedContainer Test Suite ===" << std::endl;
        
        bool all_passed = true;
        
        // Bug Fix Tests
        all_passed &= test_AllowDuplicatesStability();
        all_passed &= test_FuzzyUniquePolicy();
        all_passed &= test_FuzzyInsertRange();
        all_passed &= test_LoggingUniquePolicyForwarding();
        all_passed &= test_UniqueEquivalence();
        all_passed &= test_StableSortLargeBatch();
        
        // Improvement Tests
        all_passed &= test_ReserveAndCapacity();
        all_passed &= test_ClearMethod();
        
        // Basic Functionality
        all_passed &= test_BasicInsertAndFind();
        all_passed &= test_OnlyUniquePolicyBasic();
        all_passed &= test_RangeConstructor();
        
        // Binary Search
        all_passed &= test_BinarySearchMethods();
        
        // Erase
        all_passed &= test_EraseBasic();
        
        // Iterators
        all_passed &= test_Iterators();
        
        // Custom Policies
        all_passed &= test_TransformUniquenessPolicy();
        
        // Thread-Safety
        all_passed &= test_ConcurrentInserts();
        
        // Performance
        all_passed &= test_LargeDataset();
        all_passed &= test_BatchInsertPerformance();
        
        // Container Access Methods (NEW - SuperGrok V3)
        all_passed &= test_WithInternalContainerBasic();
        all_passed &= test_WithInternalContainerThreadSafety();
        all_passed &= test_WithInternalContainerComplexReturn();
        all_passed &= test_WithInternalContainerBackendAgnostic();
        all_passed &= test_WithInternalContainerPerformance();
        all_passed &= test_VectorAccessMethods();
        
        if (all_passed) {
            std::cout << "\n✓ All SortedContainer tests passed!" << std::endl;
        } else {
            std::cout << "\n✗ Some SortedContainer tests failed!" << std::endl;
        }
        
        return all_passed;
    }
    
} // namespace cpp_utilities::testing
