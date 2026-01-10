/**
 * @file test_SortedContainer.cpp
 * @brief Comprehensive test suite for fat_p::SortedContainer
 * 
 * Tests cover:
 * - Uniqueness policies (AllowDuplicates, OnlyUnique, Fuzzy, Logging)
 * - Fuzzy chain handling and default epsilon
 * - Backend policies (Vector, Deque)
 * - Concurrency policies (SingleThreaded, Mutex, SharedMutex, Spinlock)
 * - Binary search methods (lower_bound, upper_bound, count, find)
 * - Container access methods (toVector, withInternalContainer)
 * - Erase support and iteration
 * - Thread-safety verification
 */
/*
FATP_META:
  meta_version: 1
  component: SortedContainer
  file_role: test
  path: tests/test_SortedContainer.cpp
  namespace: fat_p
  summary: "Unit tests for SortedContainer."
  related:
    docs_search: "SortedContainer"
    headers:
      - fat_p/SortedContainer.h
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
#include <algorithm>
#include <numeric>
#include <random>
#include <sstream>

#include "SortedContainer.h"
#include "FatPTest.h"

namespace fat_p::testing::sortedcontainer
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

class TestObject
{
public:
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> destruct_count{0};
    static inline std::atomic<int> copy_count{0};
    static inline std::atomic<int> move_count{0};
    
    int value;
    
    explicit TestObject(int v = 0) : value(v)
    { 
        construct_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    TestObject(const TestObject& other) : value(other.value)
    { 
        copy_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    TestObject(TestObject&& other) noexcept : value(other.value)
    { 
        move_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    TestObject& operator=(const TestObject& other)
    {
        if (this != &other)
        {
            value = other.value;
            copy_count.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }
    
    TestObject& operator=(TestObject&& other) noexcept
    {
        if (this != &other)
        {
            value = other.value;
            move_count.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }
    
    ~TestObject()
    { 
        destruct_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    bool operator<(const TestObject& other) const { return value < other.value; }
    bool operator==(const TestObject& other) const { return value == other.value; }
    
    static void reset_counts() noexcept
    {
        construct_count.store(0, std::memory_order_relaxed);
        destruct_count.store(0, std::memory_order_relaxed);
        copy_count.store(0, std::memory_order_relaxed);
        move_count.store(0, std::memory_order_relaxed);
    }
};

inline std::ostream& operator<<(std::ostream& os, const TestObject& obj)
{
    os << obj.value;
    return os;
}

// ============================================================================
// Bug Fix #1: Stability Test for AllowDuplicatesPolicy
// ============================================================================

FATP_TEST_CASE(allow_duplicates_stability)
{
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy> sv;
    
    (void)sv.insert(5);
    (void)sv.insert(5);
    (void)sv.insert(5);
    
    auto vec = sv.toVector();
    FATP_ASSERT_EQ(vec.size(), 3, "Should have 3 elements");
    FATP_ASSERT_TRUE(std::all_of(vec.begin(), vec.end(), [](int v) { return v == 5; }),
               "All elements should be 5");
    
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy> sv2;
    (void)sv2.insert(1);
    (void)sv2.insert(3);
    (void)sv2.insert(2);
    (void)sv2.insert(3);
    (void)sv2.insert(2);
    
    vec = sv2.toVector();
    FATP_ASSERT_EQ(vec.size(), 5, "Should have 5 elements");
    FATP_ASSERT_TRUE(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
    FATP_ASSERT_EQ(sv2.count(2), 2, "Should have 2 occurrences of 2");
    FATP_ASSERT_EQ(sv2.count(3), 2, "Should have 2 occurrences of 3");
    
    return true;
}

// ============================================================================
// Bug Fix #2 & #3: FuzzyUniquePolicy Detection and Improved Fuzzy Detection
// ============================================================================

FATP_TEST_CASE(fuzzy_unique_policy)
{
    using FuzzySV = fat_p::SortedContainer<double, 
                                           fat_p::FuzzyUniquePolicy<fat_p::HybridComparisonPolicy, double>>;
    FuzzySV sv;
    
    FATP_ASSERT_TRUE(fat_p::is_fuzzy_unique_policy<fat_p::FuzzyUniquePolicy<>>::value,
               "Trait should detect FuzzyUniquePolicy");
    FATP_ASSERT_FALSE(fat_p::is_fuzzy_unique_policy<fat_p::OnlyUniquePolicy>::value,
               "Trait should not detect OnlyUniquePolicy as fuzzy");
    
    auto result1 = sv.insert(1.0, FUZZY_EPSILON, FUZZY_EPSILON);
    FATP_ASSERT_TRUE(result1.has_value() && result1.value(), "First insert should succeed");
    
    auto result2 = sv.insert(1.005, FUZZY_EPSILON, FUZZY_EPSILON);
    FATP_ASSERT_TRUE(result2.has_value() && !result2.value(), "Fuzzy duplicate should be rejected");
    
    auto result3 = sv.insert(1.02, FUZZY_EPSILON, FUZZY_EPSILON);
    FATP_ASSERT_TRUE(result3.has_value() && result3.value(), "Non-duplicate should succeed");
    
    FATP_ASSERT_EQ(sv.size(), 2, "Should have 2 elements after fuzzy filtering");
    
    auto result4 = sv.insert(0.995, FUZZY_EPSILON, FUZZY_EPSILON);
    FATP_ASSERT_TRUE(result4.has_value() && !result4.value(), 
                  "Fuzzy duplicate before should be rejected");
    
    return true;
}

// ============================================================================
// Bug Fix #4: Epsilon Parameters in insertRange for Fuzzy
// ============================================================================

FATP_TEST_CASE(fuzzy_insert_range)
{
    using FuzzySV = fat_p::SortedContainer<double, 
                                           fat_p::FuzzyUniquePolicy<fat_p::HybridComparisonPolicy, double>>;
    FuzzySV sv;
    
    std::vector<double> values = {1.0, 1.005, 1.02, 1.025, 2.0, 2.005};
    auto result = sv.insertRange(values.begin(), values.end(), FUZZY_EPSILON, FUZZY_EPSILON);
    FATP_ASSERT_TRUE(result.has_value(), "insertRange should succeed");
    
    FATP_ASSERT_LT(sv.size(), values.size(), "Fuzzy duplicates should be filtered");
    
    auto vec = sv.toVector();
    for (size_t i = 0; i < vec.size() - 1; ++i)
    {
        FATP_ASSERT_GT(std::abs(vec[i+1] - vec[i]), FUZZY_EPSILON,
                   "No two elements should be within epsilon");
    }
    
    return true;
}

// ============================================================================
// Bug Fix #10: Non-Transitive Chain Handling in Fuzzy Uniqueness
// ============================================================================

FATP_TEST_CASE(fuzzy_chain_handling)
{
    using FuzzySV = fat_p::SortedContainer<double, 
                                           fat_p::FuzzyUniquePolicy<fat_p::HybridComparisonPolicy, double>>;
    FuzzySV sv;
    
    constexpr double chain_eps = 0.004;
    std::vector<double> chain = {1.0, 1.003, 1.006, 1.009};
    
    auto result = sv.insertRange(chain.begin(), chain.end(), chain_eps, 0.0);
    FATP_ASSERT_TRUE(result.has_value(), "insertRange should succeed");
    
    auto vec = sv.toVector();
    
    FATP_ASSERT_GE(vec.size(), 1, "Should keep at least one element");
    FATP_ASSERT_LE(vec.size(), 3, "Should collapse some elements in chain");
    
    for (size_t i = 0; i + 1 < vec.size(); ++i)
    {
        double diff = std::abs(vec[i+1] - vec[i]);
        FATP_ASSERT_GE(diff, chain_eps,
                   "Consecutive elements must be >= epsilon apart after dedup");
    }
    
    FATP_ASSERT_LT(std::abs(vec[0] - 1.0), 1e-10, "First element should be 1.0");
    
    return true;
}

// ============================================================================
// Improvement: Default Epsilon for FuzzyUniquePolicy
// ============================================================================

FATP_TEST_CASE(fuzzy_default_epsilon)
{
    using FuzzySV = fat_p::SortedContainer<double, fat_p::FuzzyUniquePolicy<fat_p::HybridComparisonPolicy>>;
    FuzzySV sv;
    
    auto result1 = sv.insert(1.0);
    FATP_ASSERT_TRUE(result1.has_value() && result1.value(), "First insert should succeed");
    
    auto result2 = sv.insert(1.0 + 1e-15);
    FATP_ASSERT_TRUE(result2.has_value() && !result2.value(), 
               "Very close value should be rejected with default epsilon");
    
    auto result3 = sv.insert(2.0);
    FATP_ASSERT_TRUE(result3.has_value() && result3.value(), "Different value should be accepted");
    
    FATP_ASSERT_EQ(sv.size(), 2, "Should have 2 elements with default epsilon");
    
    FuzzySV sv2;
    std::vector<double> values = {1.0, 1.0 + 1e-15, 2.0, 2.0 + 1e-15};
    auto range_result = sv2.insertRange(values.begin(), values.end());
    FATP_ASSERT_TRUE(range_result.has_value(), "insertRange should succeed");
    FATP_ASSERT_EQ(sv2.size(), 2, "Should have 2 elements after insertRange with default epsilon");
    
    return true;
}

// ============================================================================
// Bug Fix #5: Forwarding in LoggingUniquePolicy
// ============================================================================

FATP_TEST_CASE(logging_unique_policy_forwarding)
{
    TestObject::reset_counts();
    
    using LoggingSV = fat_p::SortedContainer<TestObject, 
                                              fat_p::LoggingUniquePolicy<fat_p::OnlyUniquePolicy>>;
    LoggingSV sv;
    
    TestObject obj(42);
    auto initial_moves = TestObject::move_count.load();
    (void)sv.insert(std::move(obj));
    
    FATP_ASSERT_GT(TestObject::move_count.load(), initial_moves, 
               "Move insert should trigger move operations");
    
    return true;
}

// ============================================================================
// Bug Fix #6: std::unique Equivalence for OnlyUniquePolicy
// ============================================================================

FATP_TEST_CASE(unique_equivalence)
{
    fat_p::SortedContainer<int, fat_p::OnlyUniquePolicy> sv;
    
    std::vector<int> values = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5};
    (void)sv.insertRange(values.begin(), values.end());
    
    auto vec = sv.toVector();
    FATP_ASSERT_EQ(vec.size(), 5, "Should have 5 unique elements");
    
    std::vector<int> expected = {1, 2, 3, 4, 5};
    FATP_ASSERT_TRUE(vec == expected, "Should match expected unique sequence");
    
    return true;
}

// ============================================================================
// Improvement #7: Stable Sort for Large Batches
// ============================================================================

FATP_TEST_CASE(stable_sort_large_batch)
{
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy> sv;
    
    std::vector<int> large_batch;
    large_batch.reserve(100);
    for (int i = 99; i >= 0; --i)
    {
        large_batch.push_back(i);
    }
    
    (void)sv.insertRange(large_batch.begin(), large_batch.end());
    
    auto vec = sv.toVector();
    FATP_ASSERT_EQ(vec.size(), 100, "Should have 100 elements");
    FATP_ASSERT_TRUE(std::is_sorted(vec.begin(), vec.end()), "Should be sorted after large batch");
    
    return true;
}

// ============================================================================
// Improvement #8: Reserve and Capacity Methods
// ============================================================================

FATP_TEST_CASE(reserve_and_capacity)
{
    fat_p::SortedContainer<int> sv;
    
    sv.reserve(100);
    FATP_ASSERT_GE(sv.capacity(), 100, "Capacity should be at least 100 after reserve");
    
    (void)sv.insert(1);
    (void)sv.insert(2);
    (void)sv.insert(3);
    
    auto cap_before = sv.capacity();
    FATP_ASSERT_GE(cap_before, 100, "Capacity should persist after inserts");
    
    return true;
}

// ============================================================================
// Improvement #9: Clear Method
// ============================================================================

FATP_TEST_CASE(clear_method)
{
    fat_p::SortedContainer<int> sv;
    
    (void)sv.insert(1);
    (void)sv.insert(2);
    (void)sv.insert(3);
    
    FATP_ASSERT_EQ(sv.size(), 3, "Should have 3 elements before clear");
    
    sv.clear();
    
    FATP_ASSERT_EQ(sv.size(), 0, "Should have 0 elements after clear");
    FATP_ASSERT_TRUE(sv.empty(), "Should be empty after clear");
    
    (void)sv.insert(5);
    FATP_ASSERT_EQ(sv.size(), 1, "Should be able to insert after clear");
    
    return true;
}

// ============================================================================
// Basic Functionality Tests
// ============================================================================

FATP_TEST_CASE(basic_insert_and_find)
{
    fat_p::SortedContainer<int> sv;
    
    (void)sv.insert(5);
    (void)sv.insert(3);
    (void)sv.insert(7);
    (void)sv.insert(1);
    (void)sv.insert(9);
    
    auto vec = sv.toVector();
    FATP_ASSERT_TRUE(std::is_sorted(vec.begin(), vec.end()), "Container should maintain sorted order");
    
    std::vector<int> expected = {1, 3, 5, 7, 9};
    FATP_ASSERT_TRUE(vec == expected, "Elements should be in sorted order");
    
    auto it = sv.find(5);
    FATP_ASSERT_TRUE(it != sv.end(), "Should find existing element");
    FATP_ASSERT_EQ(*it, 5, "Found element should be correct");
    
    auto it_not_found = sv.find(6);
    FATP_ASSERT_TRUE(it_not_found == sv.end(), "Should not find non-existing element");
    
    return true;
}

FATP_TEST_CASE(only_unique_policy_basic)
{
    fat_p::SortedContainer<int, fat_p::OnlyUniquePolicy> sv;
    
    auto r1 = sv.insert(5);
    FATP_ASSERT_TRUE(r1.has_value() && r1.value(), "First insert should succeed");
    
    auto r2 = sv.insert(5);
    FATP_ASSERT_TRUE(r2.has_value() && !r2.value(), "Duplicate insert should be rejected");
    
    auto r3 = sv.insert(3);
    FATP_ASSERT_TRUE(r3.has_value() && r3.value(), "Different value should succeed");
    
    FATP_ASSERT_EQ(sv.size(), 2, "Should have 2 unique elements");
    
    return true;
}

FATP_TEST_CASE(range_constructor)
{
    std::vector<int> source = {5, 3, 7, 1, 9};
    fat_p::SortedContainer<int> sv(source.begin(), source.end());
    
    FATP_ASSERT_EQ(sv.size(), 5, "Should contain all source elements");
    
    auto vec = sv.toVector();
    FATP_ASSERT_TRUE(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
    
    return true;
}

// ============================================================================
// Binary Search Methods
// ============================================================================

FATP_TEST_CASE(binary_search_methods)
{
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy> sv;
    
    (void)sv.insert(1);
    (void)sv.insert(3);
    (void)sv.insert(3);
    (void)sv.insert(5);
    (void)sv.insert(7);
    
    auto lower = sv.lower_bound(3);
    FATP_ASSERT_TRUE(lower != sv.end() && *lower == 3, "lower_bound should find first 3");
    
    auto upper = sv.upper_bound(3);
    FATP_ASSERT_TRUE(upper != sv.end() && *upper == 5, "upper_bound should find element after 3s");
    
    FATP_ASSERT_EQ(sv.count(3), 2, "Should have 2 occurrences of 3");
    FATP_ASSERT_EQ(sv.count(4), 0, "Should have 0 occurrences of 4");
    
    return true;
}

// ============================================================================
// Erase Support
// ============================================================================

FATP_TEST_CASE(erase_basic)
{
    fat_p::SortedContainer<int> sv;
    
    (void)sv.insert(1);
    (void)sv.insert(3);
    (void)sv.insert(5);
    
    auto result = sv.erase(3);
    FATP_ASSERT_TRUE(result.has_value() && result.value(), "Erase should succeed for existing element");
    FATP_ASSERT_EQ(sv.size(), 2, "Size should decrease after erase");
    FATP_ASSERT_TRUE(sv.find(3) == sv.end(), "Element should no longer exist");
    
    auto result2 = sv.erase(99);
    FATP_ASSERT_TRUE(result2.has_value() && !result2.value(), 
                  "Erase should return false for non-existing element");
    
    return true;
}

// ============================================================================
// Iterator Tests
// ============================================================================

FATP_TEST_CASE(iterators)
{
    fat_p::SortedContainer<int> sv;
    
    (void)sv.insert(1);
    (void)sv.insert(3);
    (void)sv.insert(5);
    
    std::vector<int> forward_order;
    for (auto it = sv.begin(); it != sv.end(); ++it)
    {
        forward_order.push_back(*it);
    }
    
    std::vector<int> expected_forward = {1, 3, 5};
    FATP_ASSERT_TRUE(forward_order == expected_forward, "Forward iteration should work");
    
    std::vector<int> reverse_order;
    for (auto it = sv.rbegin(); it != sv.rend(); ++it)
    {
        reverse_order.push_back(*it);
    }
    
    std::vector<int> expected_reverse = {5, 3, 1};
    FATP_ASSERT_TRUE(reverse_order == expected_reverse, "Reverse iteration should work");
    
    return true;
}

// ============================================================================
// Thread-Safety Tests
// ============================================================================

FATP_TEST_CASE(concurrent_inserts)
{
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy, std::less<int>, 
                          std::allocator<int>, fat_p::MutexSynchronizationPolicy> sv;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < CONCURRENT_THREAD_COUNT; ++t)
    {
        threads.emplace_back([&sv, &success_count, t]() {
            for (int i = 0; i < CONCURRENT_ITERATIONS; ++i)
            {
                auto result = sv.insert(t * 1000 + i);
                if (result.has_value() && result.value())
                {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    int expected_total = CONCURRENT_THREAD_COUNT * CONCURRENT_ITERATIONS;
    FATP_ASSERT_EQ(success_count.load(), expected_total, 
               "All inserts should succeed under concurrency");
    FATP_ASSERT_EQ(sv.size(), static_cast<size_t>(expected_total), 
               "Size should match insert count");
    
    auto vec = sv.toVector();
    FATP_ASSERT_TRUE(std::is_sorted(vec.begin(), vec.end()), 
               "Should remain sorted after concurrent inserts");
    
    return true;
}

// ============================================================================
// Large Dataset Test
// ============================================================================

FATP_TEST_CASE(large_dataset)
{
    fat_p::SortedContainer<int> sv;
    
    std::vector<int> large_data;
    large_data.reserve(TEST_RANGE_SIZE);
    for (int i = TEST_RANGE_SIZE - 1; i >= 0; --i)
    {
        large_data.push_back(i);
    }
    
    (void)sv.insertRange(large_data.begin(), large_data.end());
    
    FATP_ASSERT_EQ(sv.size(), TEST_RANGE_SIZE, "Should contain all elements");
    
    auto vec = sv.toVector();
    FATP_ASSERT_TRUE(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
    
    return true;
}

// ============================================================================
// WithInternalContainer Tests
// ============================================================================

FATP_TEST_CASE(with_internal_container_basic)
{
    fat_p::SortedContainer<int> sv;
    (void)sv.insert(1);
    (void)sv.insert(2);
    (void)sv.insert(3);
    
    auto result = sv.withInternalContainer([](const auto& container) {
        return container.size();
    });
    
    FATP_ASSERT_EQ(result, 3u, "withInternalContainer should return correct size");
    
    auto sum = sv.withInternalContainer([](const auto& container) {
        return std::accumulate(container.begin(), container.end(), 0);
    });
    
    FATP_ASSERT_EQ(sum, 6, "Sum should be 1+2+3=6");
    
    return true;
}

FATP_TEST_CASE(with_internal_container_thread_safety)
{
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy, std::less<int>,
                          std::allocator<int>, fat_p::MutexSynchronizationPolicy> sv;
    
    for (int i = 0; i < 100; ++i)
    {
        (void)sv.insert(i);
    }
    
    std::atomic<int> read_count{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&sv, &read_count]() {
            for (int i = 0; i < 100; ++i)
            {
                sv.withInternalContainer([&read_count](const auto& container) {
                    (void)std::accumulate(container.begin(), container.end(), 0);
                    read_count.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }
    
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    FATP_ASSERT_EQ(read_count.load(), 400, "All reads should complete");
    
    return true;
}

FATP_TEST_CASE(with_internal_container_complex_return)
{
    fat_p::SortedContainer<int> sv;
    (void)sv.insert(10);
    (void)sv.insert(20);
    (void)sv.insert(30);
    
    auto stats = sv.withInternalContainer([](const auto& container) {
        struct Stats { int min; int max; int sum; size_t count; };
        if (container.empty())
        {
            return Stats{0, 0, 0, 0};
        }
        int sum = std::accumulate(container.begin(), container.end(), 0);
        return Stats{container.front(), container.back(), sum, container.size()};
    });
    
    FATP_ASSERT_EQ(stats.min, 10, "Min should be 10");
    FATP_ASSERT_EQ(stats.max, 30, "Max should be 30");
    FATP_ASSERT_EQ(stats.sum, 60, "Sum should be 60");
    FATP_ASSERT_EQ(stats.count, 3u, "Count should be 3");
    
    return true;
}

FATP_TEST_CASE(with_internal_container_backend_agnostic)
{
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy, std::less<int>,
                          std::allocator<int>, fat_p::SingleThreadedPolicy, 
                          DequeBackendPolicy> deque_sv;
    
    (void)deque_sv.insert(1);
    (void)deque_sv.insert(2);
    (void)deque_sv.insert(3);
    
    auto sum = deque_sv.withInternalContainer([](const auto& container) {
        return std::accumulate(container.begin(), container.end(), 0);
    });
    
    FATP_ASSERT_EQ(sum, 6, "Deque backend should also work with withInternalContainer");
    
    return true;
}

FATP_TEST_CASE(vector_access_methods)
{
    fat_p::SortedContainer<int> sv;
    (void)sv.insert(1);
    (void)sv.insert(2);
    (void)sv.insert(3);
    
    auto vec_copy = sv.toVector();
    FATP_ASSERT_EQ(vec_copy.size(), 3u, "toVector should return copy with correct size");
    
    vec_copy[0] = 999;
    auto original_first = sv.withInternalContainer([](const auto& c) { return c.front(); });
    FATP_ASSERT_EQ(original_first, 1, "Modifying copy should not affect original");
    
    int sum = sv.withInternalContainer([](const auto& container) {
        return std::accumulate(container.begin(), container.end(), 0);
    });
    
    FATP_ASSERT_EQ(sum, 6, "Sum should be 1+2+3=6");
    
    return true;
}

// ============================================================================
// Backend Policy Tests
// ============================================================================

FATP_TEST_CASE(deque_backend_policy)
{
    fat_p::SortedContainer<int, fat_p::AllowDuplicatesPolicy, std::less<int>,
                          std::allocator<int>, fat_p::SingleThreadedPolicy,
                          DequeBackendPolicy> sv;
    
    (void)sv.insert(3);
    (void)sv.insert(1);
    (void)sv.insert(2);
    
    FATP_ASSERT_EQ(sv.size(), 3, "Deque backend should work");
    
    auto cap = sv.capacity();
    FATP_ASSERT_EQ(cap, sv.size(), "Deque capacity() returns size()");
    
    sv.reserve(100);
    
    auto vec = sv.toVector();
    FATP_ASSERT_TRUE(std::is_sorted(vec.begin(), vec.end()), "Should be sorted");
    
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_component()
{
    std::cout << "\n" << colors::cyan() << "SortedContainer Benchmarks:" 
              << colors::reset() << "\n\n";
    
    fat_p::SortedContainer<int> sv;
    
    double insert_time = measure_perf([&sv]() {
        static int i = 0;
        auto result = sv.insert(i++);
        DoNotOptimize(result);
    }, 10000, 100);
    std::cout << "Single insert: " << format_time(insert_time) << "\n";
    
    sv.clear();
    
    std::vector<int> batch;
    batch.reserve(1000);
    for (int i = 0; i < 1000; ++i)
    {
        batch.push_back(999 - i);
    }
    
    double batch_time = measure_perf([&sv, &batch]() {
        sv.clear();
        auto result = sv.insertRange(batch.begin(), batch.end());
        DoNotOptimize(result);
    }, 100, 10);
    std::cout << "Batch insert (1000 elements): " << format_time(batch_time) << "\n";
    
    sv.clear();
    (void)sv.insertRange(batch.begin(), batch.end());
    
    double find_time = measure_perf([&sv]() {
        auto it = sv.find(500);
        DoNotOptimize(it);
    }, 100000, 1000);
    std::cout << "Find (binary search): " << format_time(find_time) << "\n";
    
    double lower_bound_time = measure_perf([&sv]() {
        auto it = sv.lower_bound(500);
        DoNotOptimize(it);
    }, 100000, 1000);
    std::cout << "Lower bound: " << format_time(lower_bound_time) << "\n";
    
    using FuzzySV = fat_p::SortedContainer<double, 
                                           fat_p::FuzzyUniquePolicy<fat_p::HybridComparisonPolicy>>;
    FuzzySV fuzzy_sv;
    
    std::vector<double> fuzzy_batch;
    fuzzy_batch.reserve(1000);
    for (int i = 0; i < 1000; ++i)
    {
        fuzzy_batch.push_back(i * 0.1);
    }
    
    double fuzzy_time = measure_perf([&fuzzy_sv, &fuzzy_batch]() {
        fuzzy_sv.clear();
        auto result = fuzzy_sv.insertRange(fuzzy_batch.begin(), fuzzy_batch.end());
        DoNotOptimize(result);
    }, 100, 10);
    std::cout << "Fuzzy batch insert (1000 elements): " << format_time(fuzzy_time) << "\n";
}

} // namespace fat_p::testing::sortedcontainer

namespace fat_p::testing
{

bool test_SortedContainer()
{
    FATP_PRINT_HEADER(SORTED CONTAINER)
    
    TestRunner runner;
    
    FATP_RUN_TEST_NS(runner, sortedcontainer, allow_duplicates_stability);
    FATP_RUN_TEST_NS(runner, sortedcontainer, fuzzy_unique_policy);
    FATP_RUN_TEST_NS(runner, sortedcontainer, fuzzy_insert_range);
    FATP_RUN_TEST_NS(runner, sortedcontainer, fuzzy_chain_handling);
    FATP_RUN_TEST_NS(runner, sortedcontainer, fuzzy_default_epsilon);
    FATP_RUN_TEST_NS(runner, sortedcontainer, logging_unique_policy_forwarding);
    FATP_RUN_TEST_NS(runner, sortedcontainer, unique_equivalence);
    FATP_RUN_TEST_NS(runner, sortedcontainer, stable_sort_large_batch);
    FATP_RUN_TEST_NS(runner, sortedcontainer, reserve_and_capacity);
    FATP_RUN_TEST_NS(runner, sortedcontainer, clear_method);
    FATP_RUN_TEST_NS(runner, sortedcontainer, basic_insert_and_find);
    FATP_RUN_TEST_NS(runner, sortedcontainer, only_unique_policy_basic);
    FATP_RUN_TEST_NS(runner, sortedcontainer, range_constructor);
    FATP_RUN_TEST_NS(runner, sortedcontainer, binary_search_methods);
    FATP_RUN_TEST_NS(runner, sortedcontainer, erase_basic);
    FATP_RUN_TEST_NS(runner, sortedcontainer, iterators);
    FATP_RUN_TEST_NS(runner, sortedcontainer, concurrent_inserts);
    FATP_RUN_TEST_NS(runner, sortedcontainer, large_dataset);
    FATP_RUN_TEST_NS(runner, sortedcontainer, with_internal_container_basic);
    FATP_RUN_TEST_NS(runner, sortedcontainer, with_internal_container_thread_safety);
    FATP_RUN_TEST_NS(runner, sortedcontainer, with_internal_container_complex_return);
    FATP_RUN_TEST_NS(runner, sortedcontainer, with_internal_container_backend_agnostic);
    FATP_RUN_TEST_NS(runner, sortedcontainer, vector_access_methods);
    FATP_RUN_TEST_NS(runner, sortedcontainer, deque_backend_policy);
    
    sortedcontainer::benchmark_component();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SortedContainer() ? 0 : 1;
}
#endif
