#include <iostream>
#include <string>
#include <map>

#include "FlatMap.h"
#include "test_FlatMap.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

bool test_flat_map_basic_operations() {
    FlatMap<int, std::string> map;
    
    SIMPLE_ASSERT(map.empty(), "New map should be empty");
    SIMPLE_ASSERT(map.size() == 0, "Size should be 0");
    
    map.insert({1, "one"});
    map.insert({2, "two"});
    map.insert({3, "three"});
    
    SIMPLE_ASSERT(map.size() == 3, "Size should be 3");
    SIMPLE_ASSERT(!map.empty(), "Map should not be empty");
    
    return true;
}

bool test_flat_map_find_operations() {
    FlatMap<std::string, int> map{{"one", 1}, {"two", 2}, {"three", 3}};
    
    auto it = map.find("two");
    SIMPLE_ASSERT(it != map.end(), "Should find 'two'");
    SIMPLE_ASSERT(it->second == 2, "Value should be 2");
    
    it = map.find("nonexistent");
    SIMPLE_ASSERT(it == map.end(), "Should not find nonexistent key");
    
    SIMPLE_ASSERT(map.contains("one"), "Should contain 'one'");
    SIMPLE_ASSERT(!map.contains("four"), "Should not contain 'four'");
    
    return true;
}

bool test_flat_map_operator_bracket() {
    FlatMap<int, std::string> map;
    
    map[1] = "one";
    map[2] = "two";
    
    SIMPLE_ASSERT(map[1] == "one", "Value should be 'one'");
    SIMPLE_ASSERT(map[2] == "two", "Value should be 'two'");
    
    map[1] = "ONE";
    SIMPLE_ASSERT(map[1] == "ONE", "Value should be updated");
    
    return true;
}

bool test_flat_map_at_method() {
    FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};
    
    SIMPLE_ASSERT(map.at(1) == "one", "at(1) should return 'one'");
    
    bool caught = false;
    try {
        map.at(999);
    } catch (const std::out_of_range&) {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "at() should throw for nonexistent key");
    
    return true;
}

bool test_flat_map_insert_or_assign() {
    FlatMap<int, std::string> map{{1, "one"}};
    
    auto [it, inserted] = map.insert_or_assign(1, "ONE");
    SIMPLE_ASSERT(!inserted, "Should not insert, key exists");
    SIMPLE_ASSERT(it->second == "ONE", "Value should be updated");
    
    auto [it2, inserted2] = map.insert_or_assign(2, "two");
    SIMPLE_ASSERT(inserted2, "Should insert new key");
    SIMPLE_ASSERT(it2->second == "two", "New value should be correct");
    
    return true;
}

bool test_flat_map_erase() {
    FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};
    
    size_t erased = map.erase(2);
    SIMPLE_ASSERT(erased == 1, "Should erase one element");
    SIMPLE_ASSERT(map.size() == 2, "Size should be 2");
    SIMPLE_ASSERT(map.find(2) == map.end(), "Key 2 should not be found");
    
    return true;
}

bool test_flat_map_sorted_order() {
    FlatMap<int, int> map;
    map.insert({5, 50});
    map.insert({1, 10});
    map.insert({3, 30});
    map.insert({2, 20});
    
    std::vector<int> keys;
    for (const auto& [k, v] : map) {
        keys.push_back(k);
    }
    
    SIMPLE_ASSERT(std::is_sorted(keys.begin(), keys.end()), "Keys should be sorted");
    
    return true;
}

bool test_flat_map_lower_upper_bound() {
    FlatMap<int, int> map{{1, 10}, {3, 30}, {5, 50}, {7, 70}};
    
    auto it = map.lower_bound(3);
    SIMPLE_ASSERT(it != map.end(), "Should find lower bound");
    SIMPLE_ASSERT(it->first == 3, "Lower bound should be 3");
    
    it = map.upper_bound(3);
    SIMPLE_ASSERT(it != map.end(), "Should find upper bound");
    SIMPLE_ASSERT(it->first == 5, "Upper bound should be 5");
    
    return true;
}

bool test_flat_map_equal_range() {
    FlatMap<int, int> map{{1, 10}, {2, 20}, {3, 30}};
    
    auto [first, last] = map.equal_range(2);
    SIMPLE_ASSERT(first != map.end(), "Range should not be empty");
    SIMPLE_ASSERT(first->first == 2, "First should be 2");
    
    size_t count = std::distance(first, last);
    SIMPLE_ASSERT(count == 1, "Should have exactly one element");
    
    return true;
}

bool test_flat_map_clear() {
    FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};
    
    SIMPLE_ASSERT(map.size() == 2, "Size should be 2");
    
    map.clear();
    SIMPLE_ASSERT(map.size() == 0, "Size should be 0 after clear");
    SIMPLE_ASSERT(map.empty(), "Map should be empty after clear");
    
    return true;
}

void benchmark_flatmap() {
    std::cout << "\n" << colors::cyan() << "FlatMap Benchmarks:" << colors::reset() << "\n\n";
    
    FlatMap<int, int> map;
    
    // Benchmark insert
    double insert_time = measure_perf([&map, i=0]() mutable {
        map.insert({i % 1000, i});
        ++i;
    }, 10000, 100);
    std::cout << "Insert: " << format_time(insert_time) << "\n";
    
    // Benchmark find
    double find_time = measure_perf([&map]() {
        auto it = map.find(500);
        DoNotOptimize(it);
    }, 100000, 1000);
    std::cout << "Find: " << format_time(find_time) << "\n";
    
    // Benchmark iteration
    double iter_time = measure_perf([&map]() {
        int sum = 0;
        for (const auto& [k, v] : map) {
            sum += v;
        }
        DoNotOptimize(sum);
    }, 1000, 10);
    std::cout << "Iteration: " << format_time(iter_time) << "\n";
}

bool test_FlatMap() {

    PRINT_HEADER(FLAT MAP)

    TestRunner runner;

    RUN_TEST(runner, flat_map_basic_operations);
    RUN_TEST(runner, flat_map_find_operations);
    RUN_TEST(runner, flat_map_operator_bracket);
    RUN_TEST(runner, flat_map_at_method);
    RUN_TEST(runner, flat_map_insert_or_assign);
    RUN_TEST(runner, flat_map_erase);
    RUN_TEST(runner, flat_map_sorted_order);
    RUN_TEST(runner, flat_map_lower_upper_bound);
    RUN_TEST(runner, flat_map_equal_range);
    RUN_TEST(runner, flat_map_clear);

    benchmark_flatmap();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
