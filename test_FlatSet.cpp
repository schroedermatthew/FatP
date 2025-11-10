#include <iostream>
#include <set>

#include "FlatSet.h"
#include "test_FlatSet.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

bool test_flat_set_basic_operations() {
    FlatSet<int> set;
    
    SIMPLE_ASSERT(set.empty(), "New set should be empty");
    SIMPLE_ASSERT(set.size() == 0, "Size should be 0");
    
    set.insert(1);
    set.insert(2);
    set.insert(3);
    
    SIMPLE_ASSERT(set.size() == 3, "Size should be 3");
    SIMPLE_ASSERT(!set.empty(), "Set should not be empty");
    
    return true;
}

bool test_flat_set_insert_duplicate() {
    FlatSet<int> set;
    
    auto [it1, inserted1] = set.insert(5);
    SIMPLE_ASSERT(inserted1, "First insert should succeed");
    SIMPLE_ASSERT(*it1 == 5, "Value should be 5");
    
    auto [it2, inserted2] = set.insert(5);
    SIMPLE_ASSERT(!inserted2, "Duplicate insert should fail");
    SIMPLE_ASSERT(*it2 == 5, "Value should still be 5");
    
    SIMPLE_ASSERT(set.size() == 1, "Size should still be 1");
    
    return true;
}

bool test_flat_set_find_operations() {
    FlatSet<int> set{10, 20, 30, 40};
    
    auto it = set.find(20);
    SIMPLE_ASSERT(it != set.end(), "Should find 20");
    SIMPLE_ASSERT(*it == 20, "Value should be 20");
    
    it = set.find(999);
    SIMPLE_ASSERT(it == set.end(), "Should not find nonexistent value");
    
    SIMPLE_ASSERT(set.contains(30), "Should contain 30");
    SIMPLE_ASSERT(!set.contains(50), "Should not contain 50");
    
    return true;
}

bool test_flat_set_erase() {
    FlatSet<int> set{1, 2, 3, 4, 5};
    
    size_t erased = set.erase(3);
    SIMPLE_ASSERT(erased == 1, "Should erase one element");
    SIMPLE_ASSERT(set.size() == 4, "Size should be 4");
    SIMPLE_ASSERT(!set.contains(3), "Should not contain 3");
    
    erased = set.erase(999);
    SIMPLE_ASSERT(erased == 0, "Should not erase nonexistent element");
    
    return true;
}

bool test_flat_set_sorted_order() {
    FlatSet<int> set;
    set.insert(5);
    set.insert(1);
    set.insert(3);
    set.insert(2);
    set.insert(4);
    
    std::vector<int> values(set.begin(), set.end());
    SIMPLE_ASSERT(std::is_sorted(values.begin(), values.end()), "Values should be sorted");
    
    return true;
}

bool test_flat_set_lower_upper_bound() {
    FlatSet<int> set{10, 20, 30, 40, 50};
    
    auto it = set.lower_bound(30);
    SIMPLE_ASSERT(it != set.end(), "Should find lower bound");
    SIMPLE_ASSERT(*it == 30, "Lower bound should be 30");
    
    it = set.upper_bound(30);
    SIMPLE_ASSERT(it != set.end(), "Should find upper bound");
    SIMPLE_ASSERT(*it == 40, "Upper bound should be 40");
    
    return true;
}

bool test_flat_set_equal_range() {
    FlatSet<int> set{10, 20, 30};
    
    auto [first, last] = set.equal_range(20);
    SIMPLE_ASSERT(first != set.end(), "Range should not be empty");
    SIMPLE_ASSERT(*first == 20, "First should be 20");
    
    size_t count = std::distance(first, last);
    SIMPLE_ASSERT(count == 1, "Should have exactly one element");
    
    return true;
}

bool test_flat_set_clear() {
    FlatSet<int> set{1, 2, 3, 4, 5};
    
    SIMPLE_ASSERT(set.size() == 5, "Size should be 5");
    
    set.clear();
    SIMPLE_ASSERT(set.size() == 0, "Size should be 0 after clear");
    SIMPLE_ASSERT(set.empty(), "Set should be empty after clear");
    
    return true;
}

void benchmark_flatset() {
    std::cout << "\n" << colors::cyan() << "FlatSet Benchmarks:" << colors::reset() << "\n\n";
    
    FlatSet<int> set;
    
    // Benchmark insert
    double insert_time = measure_perf([&set, i=0]() mutable {
        set.insert(i % 1000);
        ++i;
    }, 10000, 100);
    std::cout << "Insert: " << format_time(insert_time) << "\n";
    
    // Benchmark find
    double find_time = measure_perf([&set]() {
        auto it = set.find(500);
        DoNotOptimize(it);
    }, 100000, 1000);
    std::cout << "Find: " << format_time(find_time) << "\n";
    
    // Benchmark iteration
    double iter_time = measure_perf([&set]() {
        int sum = 0;
        for (int val : set) {
            sum += val;
        }
        DoNotOptimize(sum);
    }, 1000, 10);
    std::cout << "Iteration: " << format_time(iter_time) << "\n";
}

bool test_FlatSet() {

    PRINT_HEADER(FLAT SET)

    TestRunner runner;

    RUN_TEST(runner, flat_set_basic_operations);
    RUN_TEST(runner, flat_set_insert_duplicate);
    RUN_TEST(runner, flat_set_find_operations);
    RUN_TEST(runner, flat_set_erase);
    RUN_TEST(runner, flat_set_sorted_order);
    RUN_TEST(runner, flat_set_lower_upper_bound);
    RUN_TEST(runner, flat_set_equal_range);
    RUN_TEST(runner, flat_set_clear);

    benchmark_flatset();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
