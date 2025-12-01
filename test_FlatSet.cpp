#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "FlatSet.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_FlatSet.h"
#endif

namespace fat_p::testing
{

namespace
{

bool test_flat_set_basic_operations()
{
    fat_p::FlatSet<int> set;

    SIMPLE_ASSERT(set.empty(), "New set should be empty");
    SIMPLE_ASSERT(set.size() == 0, "Size should be 0");

    set.insert(1);
    set.insert(2);
    set.insert(3);

    SIMPLE_ASSERT(set.size() == 3, "Size should be 3");
    SIMPLE_ASSERT(!set.empty(), "Set should not be empty");

    return true;
}

bool test_flat_set_insert_duplicate()
{
    fat_p::FlatSet<int> set;

    auto [it1, inserted1] = set.insert(5);
    SIMPLE_ASSERT(inserted1, "First insert should succeed");
    SIMPLE_ASSERT(*it1 == 5, "Value should be 5");

    auto [it2, inserted2] = set.insert(5);
    SIMPLE_ASSERT(!inserted2, "Duplicate insert should fail");
    SIMPLE_ASSERT(*it2 == 5, "Value should still be 5");

    SIMPLE_ASSERT(set.size() == 1, "Size should still be 1");

    return true;
}

bool test_flat_set_find_operations()
{
    fat_p::FlatSet<int> set{10, 20, 30, 40};

    auto it = set.find(20);
    SIMPLE_ASSERT(it != set.end(), "Should find 20");
    SIMPLE_ASSERT(*it == 20, "Value should be 20");

    it = set.find(999);
    SIMPLE_ASSERT(it == set.end(), "Should not find nonexistent value");

    SIMPLE_ASSERT(set.contains(30), "Should contain 30");
    SIMPLE_ASSERT(!set.contains(50), "Should not contain 50");

    return true;
}

bool test_flat_set_erase()
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    size_t erased = set.erase(3);
    SIMPLE_ASSERT(erased == 1, "Should erase one element");
    SIMPLE_ASSERT(set.size() == 4, "Size should be 4");
    SIMPLE_ASSERT(!set.contains(3), "Should not contain 3");

    erased = set.erase(999);
    SIMPLE_ASSERT(erased == 0, "Should not erase nonexistent element");

    return true;
}

bool test_flat_set_sorted_order()
{
    fat_p::FlatSet<int> set;
    set.insert(5);
    set.insert(1);
    set.insert(3);
    set.insert(2);
    set.insert(4);

    std::vector<int> values(set.begin(), set.end());
    SIMPLE_ASSERT(std::is_sorted(values.begin(), values.end()), "Values should be sorted");

    return true;
}

bool test_flat_set_lower_upper_bound()
{
    fat_p::FlatSet<int> set{10, 20, 30, 40, 50};

    auto it = set.lower_bound(30);
    SIMPLE_ASSERT(it != set.end(), "Should find lower bound");
    SIMPLE_ASSERT(*it == 30, "Lower bound should be 30");

    it = set.upper_bound(30);
    SIMPLE_ASSERT(it != set.end(), "Should find upper bound");
    SIMPLE_ASSERT(*it == 40, "Upper bound should be 40");

    return true;
}

bool test_flat_set_equal_range()
{
    fat_p::FlatSet<int> set{10, 20, 30};

    auto [first, last] = set.equal_range(20);
    SIMPLE_ASSERT(first != set.end(), "Range should not be empty");
    SIMPLE_ASSERT(*first == 20, "First should be 20");

    size_t count = std::distance(first, last);
    SIMPLE_ASSERT(count == 1, "Should have exactly one element");

    return true;
}

bool test_flat_set_clear()
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    SIMPLE_ASSERT(set.size() == 5, "Size should be 5");

    set.clear();
    SIMPLE_ASSERT(set.size() == 0, "Size should be 0 after clear");
    SIMPLE_ASSERT(set.empty(), "Set should be empty after clear");

    return true;
}

bool test_flat_set_custom_comparator()
{
    fat_p::FlatSet<int, std::greater<int>> set;

    set.insert(1);
    set.insert(3);
    set.insert(2);

    std::vector<int> values(set.begin(), set.end());

    SIMPLE_ASSERT(values.size() == 3, "Should have 3 elements");
    SIMPLE_ASSERT(values[0] == 3, "First should be 3 (descending)");
    SIMPLE_ASSERT(values[1] == 2, "Second should be 2");
    SIMPLE_ASSERT(values[2] == 1, "Third should be 1");

    return true;
}

bool test_flat_set_case_insensitive_comparator()
{
    struct CaseInsensitiveCompare
    {
        bool operator()(const std::string& a, const std::string& b) const
        {
            return std::lexicographical_compare(
                a.begin(),
                a.end(),
                b.begin(),
                b.end(),
                [](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); });
        }
    };

    fat_p::FlatSet<std::string, CaseInsensitiveCompare> set;

    set.insert("Hello");
    auto [it, inserted] = set.insert("HELLO");

    SIMPLE_ASSERT(!inserted, "HELLO should be duplicate of Hello");
    SIMPLE_ASSERT(set.size() == 1, "Should have only 1 element");
    SIMPLE_ASSERT(set.contains("hello"), "Should find 'hello'");
    SIMPLE_ASSERT(set.contains("HELLO"), "Should find 'HELLO'");

    return true;
}

bool test_flat_set_reserve_capacity()
{
    fat_p::FlatSet<int> set;

    SIMPLE_ASSERT(set.capacity() == 0, "Initial capacity should be 0");

    set.reserve(100);
    SIMPLE_ASSERT(set.capacity() >= 100, "Capacity should be at least 100");
    SIMPLE_ASSERT(set.size() == 0, "Size should still be 0");

    for (int i = 0; i < 50; ++i)
    {
        set.insert(i);
    }

    SIMPLE_ASSERT(set.size() == 50, "Size should be 50");
    SIMPLE_ASSERT(set.capacity() >= 100, "Capacity should still be at least 100");

    set.shrink_to_fit();
    SIMPLE_ASSERT(set.capacity() >= set.size(), "Capacity should be at least size");

    return true;
}

bool test_flat_set_equality_operators()
{
    fat_p::FlatSet<int> set1{1, 2, 3};
    fat_p::FlatSet<int> set2{1, 2, 3};
    fat_p::FlatSet<int> set3{1, 2, 4};
    fat_p::FlatSet<int> set4{1, 2};

    SIMPLE_ASSERT(set1 == set2, "Identical sets should be equal");
    SIMPLE_ASSERT(!(set1 != set2), "Identical sets should not be not-equal");

    SIMPLE_ASSERT(set1 != set3, "Sets with different values should not be equal");
    SIMPLE_ASSERT(set1 != set4, "Sets with different sizes should not be equal");

    return true;
}

bool test_flat_set_swap()
{
    fat_p::FlatSet<int> set1{1, 2};
    fat_p::FlatSet<int> set2{3, 4, 5};

    set1.swap(set2);

    SIMPLE_ASSERT(set1.size() == 3, "set1 should have 3 elements after swap");
    SIMPLE_ASSERT(set2.size() == 2, "set2 should have 2 elements after swap");
    SIMPLE_ASSERT(set1.contains(3), "set1 should contain 3");
    SIMPLE_ASSERT(set2.contains(1), "set2 should contain 1");

    return true;
}

bool test_flat_set_emplace()
{
    fat_p::FlatSet<std::string> set;

    auto [it1, inserted1] = set.emplace("hello");
    SIMPLE_ASSERT(inserted1, "Should insert new value");
    SIMPLE_ASSERT(*it1 == "hello", "Value should be 'hello'");

    auto [it2, inserted2] = set.emplace("hello");
    SIMPLE_ASSERT(!inserted2, "Should not insert duplicate");

    return true;
}

bool test_flat_set_emplace_hint()
{
    fat_p::FlatSet<int> set{1, 3};

    auto hint = set.find(1);
    auto it = set.emplace_hint(hint, 2);

    SIMPLE_ASSERT(*it == 2, "Should have inserted 2");
    SIMPLE_ASSERT(set.size() == 3, "Size should be 3");

    return true;
}

bool test_flat_set_range_insert()
{
    std::vector<int> data = {5, 1, 3, 2, 4, 3, 1};

    fat_p::FlatSet<int> set;
    set.insert(data.begin(), data.end());

    SIMPLE_ASSERT(set.size() == 5, "Should have 5 unique elements");

    std::vector<int> values(set.begin(), set.end());
    SIMPLE_ASSERT(std::is_sorted(values.begin(), values.end()), "Values should be sorted");

    return true;
}

bool test_flat_set_move_semantics()
{
    fat_p::FlatSet<int> set1{1, 2, 3};

    fat_p::FlatSet<int> set2(std::move(set1));
    SIMPLE_ASSERT(set2.size() == 3, "Moved-to set should have 3 elements");
    SIMPLE_ASSERT(set2.contains(1), "Moved-to set should contain 1");

    fat_p::FlatSet<int> set3;
    set3 = std::move(set2);
    SIMPLE_ASSERT(set3.size() == 3, "Assigned-to set should have 3 elements");

    return true;
}

bool test_flat_set_extract()
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    auto it = set.find(3);
    SIMPLE_ASSERT(it != set.end(), "Should find value 3");

    auto extracted = set.extract(it);
    SIMPLE_ASSERT(extracted == 3, "Extracted value should be 3");
    SIMPLE_ASSERT(set.size() == 4, "Set should have 4 elements after extract");
    SIMPLE_ASSERT(!set.contains(3), "Set should not contain 3 after extract");

    return true;
}

bool test_flat_set_empty_operations()
{
    fat_p::FlatSet<int> set;

    SIMPLE_ASSERT(set.find(1) == set.end(), "find on empty set should return end");
    SIMPLE_ASSERT(!set.contains(1), "contains on empty set should return false");
    SIMPLE_ASSERT(set.count(1) == 0, "count on empty set should return 0");
    SIMPLE_ASSERT(set.lower_bound(1) == set.end(), "lower_bound on empty set should return end");
    SIMPLE_ASSERT(set.upper_bound(1) == set.end(), "upper_bound on empty set should return end");

    auto [first, last] = set.equal_range(1);
    SIMPLE_ASSERT(first == last, "equal_range on empty set should return empty range");

    SIMPLE_ASSERT(set.erase(1) == 0, "erase nonexistent value should return 0");

    return true;
}

bool test_flat_set_initializer_list_insert()
{
    fat_p::FlatSet<int> set{1, 2};

    set.insert({3, 4, 5, 3, 4});

    SIMPLE_ASSERT(set.size() == 5, "Should have 5 unique elements");
    SIMPLE_ASSERT(set.contains(5), "Should contain 5");

    return true;
}

bool test_flat_set_heterogeneous_lookup()
{
    // Use std::less<> for transparent comparison
    fat_p::FlatSet<std::string, std::less<>> set;
    
    set.insert("apple");
    set.insert("banana");
    set.insert("cherry");
    
    // These lookups should NOT create temporary std::string objects
    auto it = set.find("banana");
    SIMPLE_ASSERT(it != set.end(), "find with const char* should work");
    SIMPLE_ASSERT(*it == "banana", "find should return correct value");
    
    SIMPLE_ASSERT(set.contains("apple"), "contains with const char* should work");
    SIMPLE_ASSERT(!set.contains("grape"), "contains should return false for missing key");
    
    SIMPLE_ASSERT(set.count("cherry") == 1, "count with const char* should work");
    SIMPLE_ASSERT(set.count("grape") == 0, "count should return 0 for missing key");
    
    auto lb = set.lower_bound("banana");
    SIMPLE_ASSERT(lb != set.end() && *lb == "banana", "lower_bound with const char* should work");
    
    auto ub = set.upper_bound("banana");
    SIMPLE_ASSERT(ub != set.end() && *ub == "cherry", "upper_bound with const char* should work");
    
    auto [first, last] = set.equal_range("banana");
    SIMPLE_ASSERT(first != last, "equal_range should find element");
    SIMPLE_ASSERT(*first == "banana", "equal_range should return correct element");
    
    return true;
}

bool test_flat_set_merge()
{
    fat_p::FlatSet<int> set1{1, 3, 5};
    fat_p::FlatSet<int> set2{2, 3, 4};  // 3 is duplicate
    
    set1.merge(set2);
    
    SIMPLE_ASSERT(set1.size() == 5, "Merged set should have 5 elements");
    SIMPLE_ASSERT(set2.empty(), "Source set should be empty after merge");
    
    SIMPLE_ASSERT(set1.contains(1), "Element 1 should be preserved");
    SIMPLE_ASSERT(set1.contains(2), "Element 2 should be merged");
    SIMPLE_ASSERT(set1.contains(3), "Element 3 should be present");
    SIMPLE_ASSERT(set1.contains(4), "Element 4 should be merged");
    SIMPLE_ASSERT(set1.contains(5), "Element 5 should be preserved");
    
    // Verify sorted order
    int prev = -1;
    for (int v : set1)
    {
        SIMPLE_ASSERT(v > prev, "Merged set should maintain sorted order");
        prev = v;
    }
    
    // Test merge with empty source
    fat_p::FlatSet<int> empty;
    size_t size_before = set1.size();
    set1.merge(empty);
    SIMPLE_ASSERT(set1.size() == size_before, "Merging empty set should not change size");
    
    // Test merge into empty target
    fat_p::FlatSet<int> target;
    fat_p::FlatSet<int> source{10, 20};
    target.merge(source);
    SIMPLE_ASSERT(target.size() == 2, "Merge into empty should work");
    SIMPLE_ASSERT(source.empty(), "Source should be empty after merge");
    
    return true;
}

void benchmark_flatset()
{
    std::cout << colors::cyan() << "FlatSet Benchmarks (1k elements):" << colors::reset() << "\n";

    constexpr int N = 1000;
    fat_p::FlatSet<int> set;

    double insert_time = measure_perf(
        [&set, i = 0]() mutable {
            set.insert(i % N);
            ++i;
        },
        100000,
        1000);
    std::cout << "Insert (random): " << format_time(insert_time) << "\n";

    set.clear();
    set.reserve(N);
    double insert_sorted_time = measure_perf(
        [&set, i = 0]() mutable {
            if (set.size() < N)
            {
                set.insert(static_cast<int>(set.size()));
            }
            ++i;
        },
        10000,
        100);
    std::cout << "Insert (sorted, reserved): " << format_time(insert_sorted_time) << "\n";

    set.clear();
    for (int i = 0; i < N; ++i)
    {
        set.insert(i);
    }

    // Use varying keys and accumulate results to prevent optimization
    volatile int find_accumulator = 0;
    double find_time = measure_perf(
        [&set, &find_accumulator, i = 0]() mutable {
            auto it = set.find(i % N);
            if (it != set.end())
            {
                find_accumulator += *it;
            }
            ++i;
        },
        1000000,
        10000);
    std::cout << "Find: " << format_time(find_time) << "\n";
    DoNotOptimize(find_accumulator);

    // Force actual iteration by using volatile accumulator
    double iter_time = measure_perf(
        [&set]() {
            volatile int sum = 0;
            for (int val : set)
            {
                sum += val;
            }
            DoNotOptimize(sum);
        },
        100000,
        1000);
    std::cout << "Iteration (1000 elements): " << format_time(iter_time) << "\n";

    std::set<int> std_set;
    for (int i = 0; i < N; ++i)
    {
        std_set.insert(i);
    }

    // Use varying keys and accumulate for std::set find too
    volatile int std_find_accumulator = 0;
    double std_find_time = measure_perf(
        [&std_set, &std_find_accumulator, i = 0]() mutable {
            auto it = std_set.find(i % N);
            if (it != std_set.end())
            {
                std_find_accumulator += *it;
            }
            ++i;
        },
        1000000,
        10000);
    std::cout << "std::set Find: " << format_time(std_find_time) << "\n";
    DoNotOptimize(std_find_accumulator);

    double std_iter_time = measure_perf(
        [&std_set]() {
            volatile int sum = 0;
            for (int val : std_set)
            {
                sum += val;
            }
            DoNotOptimize(sum);
        },
        100000,
        1000);
    std::cout << "std::set Iteration (1000 elements): " << format_time(std_iter_time) << "\n";
}

void benchmark_flatset_large_scale()
{
    std::cout << "\n" << colors::cyan() 
              << "Large-Scale Benchmarks (100k elements, cache stress):" 
              << colors::reset() << "\n";
    
    // 100,000 elements = ~400KB data (exceeds typical L1/L2 cache)
    constexpr int N = 100000;
    
    // Prepare random access pattern to defeat prefetcher
    std::vector<int> random_keys(N);
    std::iota(random_keys.begin(), random_keys.end(), 0);
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::shuffle(random_keys.begin(), random_keys.end(), rng);

    // Build FlatSet
    fat_p::FlatSet<int> set;
    set.reserve(N);
    for (int i = 0; i < N; ++i)
    {
        set.insert(i);
    }

    // Build std::set
    std::set<int> std_set;
    for (int i = 0; i < N; ++i)
    {
        std_set.insert(i);
    }

    // FlatSet Find (random access pattern)
    volatile int find_accumulator = 0;
    double find_time = measure_perf(
        [&set, &random_keys, &find_accumulator, i = 0]() mutable {
            auto it = set.find(random_keys[i % N]);
            if (it != set.end())
            {
                find_accumulator += *it;
            }
            ++i;
        },
        100000,
        1000);
    std::cout << "FlatSet Find (random access): " << format_time(find_time) << "\n";
    DoNotOptimize(find_accumulator);

    // std::set Find (random access pattern)
    volatile int std_find_accumulator = 0;
    double std_find_time = measure_perf(
        [&std_set, &random_keys, &std_find_accumulator, i = 0]() mutable {
            auto it = std_set.find(random_keys[i % N]);
            if (it != std_set.end())
            {
                std_find_accumulator += *it;
            }
            ++i;
        },
        100000,
        1000);
    std::cout << "std::set Find (random access): " << format_time(std_find_time) << "\n";
    DoNotOptimize(std_find_accumulator);

    // FlatSet Iteration (sequential - prefetcher works)
    double iter_time = measure_perf(
        [&set]() {
            volatile int sum = 0;
            for (int val : set)
            {
                sum += val;
            }
            DoNotOptimize(sum);
        },
        1000,
        100);
    std::cout << "FlatSet Iteration (100k elements): " << format_time(iter_time) << "\n";

    // std::set Iteration (pointer chasing - prefetcher fails)
    double std_iter_time = measure_perf(
        [&std_set]() {
            volatile int sum = 0;
            for (int val : std_set)
            {
                sum += val;
            }
            DoNotOptimize(sum);
        },
        1000,
        100);
    std::cout << "std::set Iteration (100k elements): " << format_time(std_iter_time) << "\n";
}

} // anonymous namespace

bool test_FlatSet()
{
    PRINT_HEADER(FLAT SET)

    // Print system information for benchmark context
    auto sys_info = SystemInfo::capture();
    sys_info.print();

    TestRunner runner;

    RUN_TEST(runner, flat_set_basic_operations);
    RUN_TEST(runner, flat_set_insert_duplicate);
    RUN_TEST(runner, flat_set_find_operations);
    RUN_TEST(runner, flat_set_erase);
    RUN_TEST(runner, flat_set_sorted_order);
    RUN_TEST(runner, flat_set_lower_upper_bound);
    RUN_TEST(runner, flat_set_equal_range);
    RUN_TEST(runner, flat_set_clear);
    RUN_TEST(runner, flat_set_custom_comparator);
    RUN_TEST(runner, flat_set_case_insensitive_comparator);
    RUN_TEST(runner, flat_set_reserve_capacity);
    RUN_TEST(runner, flat_set_equality_operators);
    RUN_TEST(runner, flat_set_swap);
    RUN_TEST(runner, flat_set_emplace);
    RUN_TEST(runner, flat_set_emplace_hint);
    RUN_TEST(runner, flat_set_range_insert);
    RUN_TEST(runner, flat_set_move_semantics);
    RUN_TEST(runner, flat_set_extract);
    RUN_TEST(runner, flat_set_empty_operations);
    RUN_TEST(runner, flat_set_initializer_list_insert);
    RUN_TEST(runner, flat_set_heterogeneous_lookup);
    RUN_TEST(runner, flat_set_merge);

    benchmark_flatset();
    benchmark_flatset_large_scale();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FlatSet() ? 0 : 1;
}
#endif
