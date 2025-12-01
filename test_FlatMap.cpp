#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "FlatMap.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_FlatMap.h"
#endif

namespace fat_p::testing
{

namespace
{

bool test_flat_map_basic_operations()
{
    fat_p::FlatMap<int, std::string> map;

    SIMPLE_ASSERT(map.empty(), "New map should be empty");
    SIMPLE_ASSERT(map.size() == 0, "Size should be 0");

    map.insert({1, "one"});
    map.insert({2, "two"});
    map.insert({3, "three"});

    SIMPLE_ASSERT(map.size() == 3, "Size should be 3");
    SIMPLE_ASSERT(!map.empty(), "Map should not be empty");

    return true;
}

bool test_flat_map_find_operations()
{
    fat_p::FlatMap<std::string, int> map{{"one", 1}, {"two", 2}, {"three", 3}};

    auto it = map.find("two");
    SIMPLE_ASSERT(it != map.end(), "Should find 'two'");
    SIMPLE_ASSERT(it->second == 2, "Value should be 2");

    it = map.find("nonexistent");
    SIMPLE_ASSERT(it == map.end(), "Should not find nonexistent key");

    SIMPLE_ASSERT(map.contains("one"), "Should contain 'one'");
    SIMPLE_ASSERT(!map.contains("four"), "Should not contain 'four'");

    return true;
}

bool test_flat_map_operator_bracket()
{
    fat_p::FlatMap<int, std::string> map;

    map[1] = "one";
    map[2] = "two";

    SIMPLE_ASSERT(map[1] == "one", "Value should be 'one'");
    SIMPLE_ASSERT(map[2] == "two", "Value should be 'two'");

    map[1] = "ONE";
    SIMPLE_ASSERT(map[1] == "ONE", "Value should be updated");

    return true;
}

bool test_flat_map_at_method()
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};

    SIMPLE_ASSERT(map.at(1) == "one", "at(1) should return 'one'");

    bool caught = false;
    try
    {
        (void)map.at(999);
    }
    catch (const std::out_of_range&)
    {
        caught = true;
    }
    SIMPLE_ASSERT(caught, "at() should throw for nonexistent key");

    return true;
}

bool test_flat_map_insert_or_assign()
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}};

    auto [it, inserted] = map.insert_or_assign(1, "ONE");
    SIMPLE_ASSERT(!inserted, "Should not insert, key exists");
    SIMPLE_ASSERT(it->second == "ONE", "Value should be updated");

    auto [it2, inserted2] = map.insert_or_assign(2, "two");
    SIMPLE_ASSERT(inserted2, "Should insert new key");
    SIMPLE_ASSERT(it2->second == "two", "New value should be correct");

    return true;
}

bool test_flat_map_erase()
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    size_t erased = map.erase(2);
    SIMPLE_ASSERT(erased == 1, "Should erase one element");
    SIMPLE_ASSERT(map.size() == 2, "Size should be 2");
    SIMPLE_ASSERT(map.find(2) == map.end(), "Key 2 should not be found");

    return true;
}

bool test_flat_map_sorted_order()
{
    fat_p::FlatMap<int, int> map;
    map.insert({5, 50});
    map.insert({1, 10});
    map.insert({3, 30});
    map.insert({2, 20});

    std::vector<int> keys;
    for (const auto& kv : map)
    {
        keys.push_back(kv.first);
    }

    SIMPLE_ASSERT(std::is_sorted(keys.begin(), keys.end()), "Keys should be sorted");

    return true;
}

bool test_flat_map_lower_upper_bound()
{
    fat_p::FlatMap<int, int> map{{1, 10}, {3, 30}, {5, 50}, {7, 70}};

    auto it = map.lower_bound(3);
    SIMPLE_ASSERT(it != map.end(), "Should find lower bound");
    SIMPLE_ASSERT(it->first == 3, "Lower bound should be 3");

    it = map.upper_bound(3);
    SIMPLE_ASSERT(it != map.end(), "Should find upper bound");
    SIMPLE_ASSERT(it->first == 5, "Upper bound should be 5");

    return true;
}

bool test_flat_map_equal_range()
{
    fat_p::FlatMap<int, int> map{{1, 10}, {2, 20}, {3, 30}};

    auto [first, last] = map.equal_range(2);
    SIMPLE_ASSERT(first != map.end(), "Range should not be empty");
    SIMPLE_ASSERT(first->first == 2, "First should be 2");

    size_t count = std::distance(first, last);
    SIMPLE_ASSERT(count == 1, "Should have exactly one element");

    return true;
}

bool test_flat_map_clear()
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};

    SIMPLE_ASSERT(map.size() == 2, "Size should be 2");

    map.clear();
    SIMPLE_ASSERT(map.size() == 0, "Size should be 0 after clear");
    SIMPLE_ASSERT(map.empty(), "Map should be empty after clear");

    return true;
}

bool test_flat_map_custom_comparator()
{
    fat_p::FlatMap<int, std::string, std::greater<int>> map;

    map.insert({1, "one"});
    map.insert({3, "three"});
    map.insert({2, "two"});

    std::vector<int> keys;
    for (const auto& kv : map)
    {
        keys.push_back(kv.first);
    }

    SIMPLE_ASSERT(keys.size() == 3, "Should have 3 elements");
    SIMPLE_ASSERT(keys[0] == 3, "First should be 3 (descending)");
    SIMPLE_ASSERT(keys[1] == 2, "Second should be 2");
    SIMPLE_ASSERT(keys[2] == 1, "Third should be 1");

    return true;
}

bool test_flat_map_case_insensitive_comparator()
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

    fat_p::FlatMap<std::string, int, CaseInsensitiveCompare> map;

    map.insert({"Hello", 1});
    auto [it, inserted] = map.insert({"HELLO", 2});

    SIMPLE_ASSERT(!inserted, "HELLO should be duplicate of Hello");
    SIMPLE_ASSERT(map.size() == 1, "Should have only 1 element");
    SIMPLE_ASSERT(map.contains("hello"), "Should find 'hello'");
    SIMPLE_ASSERT(map.contains("HELLO"), "Should find 'HELLO'");

    return true;
}

bool test_flat_map_reserve_capacity()
{
    fat_p::FlatMap<int, int> map;

    SIMPLE_ASSERT(map.capacity() == 0, "Initial capacity should be 0");

    map.reserve(100);
    SIMPLE_ASSERT(map.capacity() >= 100, "Capacity should be at least 100");
    SIMPLE_ASSERT(map.size() == 0, "Size should still be 0");

    for (int i = 0; i < 50; ++i)
    {
        map.insert({i, i * 10});
    }

    SIMPLE_ASSERT(map.size() == 50, "Size should be 50");
    SIMPLE_ASSERT(map.capacity() >= 100, "Capacity should still be at least 100");

    map.shrink_to_fit();
    SIMPLE_ASSERT(map.capacity() >= map.size(), "Capacity should be at least size");

    return true;
}

bool test_flat_map_equality_operators()
{
    fat_p::FlatMap<int, std::string> map1{{1, "one"}, {2, "two"}, {3, "three"}};
    fat_p::FlatMap<int, std::string> map2{{1, "one"}, {2, "two"}, {3, "three"}};
    fat_p::FlatMap<int, std::string> map3{{1, "one"}, {2, "TWO"}, {3, "three"}};
    fat_p::FlatMap<int, std::string> map4{{1, "one"}, {2, "two"}};

    SIMPLE_ASSERT(map1 == map2, "Identical maps should be equal");
    SIMPLE_ASSERT(!(map1 != map2), "Identical maps should not be not-equal");

    SIMPLE_ASSERT(map1 != map3, "Maps with different values should not be equal");
    SIMPLE_ASSERT(map1 != map4, "Maps with different sizes should not be equal");

    return true;
}

bool test_flat_map_swap()
{
    fat_p::FlatMap<int, std::string> map1{{1, "one"}, {2, "two"}};
    fat_p::FlatMap<int, std::string> map2{{3, "three"}, {4, "four"}, {5, "five"}};

    map1.swap(map2);

    SIMPLE_ASSERT(map1.size() == 3, "map1 should have 3 elements after swap");
    SIMPLE_ASSERT(map2.size() == 2, "map2 should have 2 elements after swap");
    SIMPLE_ASSERT(map1.contains(3), "map1 should contain 3");
    SIMPLE_ASSERT(map2.contains(1), "map2 should contain 1");

    return true;
}

bool test_flat_map_try_emplace()
{
    fat_p::FlatMap<int, std::string> map;

    auto [it1, inserted1] = map.try_emplace(1, "one");
    SIMPLE_ASSERT(inserted1, "Should insert new key");
    SIMPLE_ASSERT(it1->second == "one", "Value should be 'one'");

    auto [it2, inserted2] = map.try_emplace(1, "ONE");
    SIMPLE_ASSERT(!inserted2, "Should not insert duplicate key");
    SIMPLE_ASSERT(it2->second == "one", "Value should still be 'one'");

    return true;
}

bool test_flat_map_emplace_hint()
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {3, "three"}};

    auto hint = map.find(1);
    auto it = map.emplace_hint(hint, 2, "two");

    SIMPLE_ASSERT(it->first == 2, "Should have inserted key 2");
    SIMPLE_ASSERT(it->second == "two", "Value should be 'two'");
    SIMPLE_ASSERT(map.size() == 3, "Size should be 3");

    return true;
}

bool test_flat_map_range_insert()
{
    std::vector<std::pair<int, std::string>> data = {
        {5, "five"}, {1, "one"}, {3, "three"}, {2, "two"}, {4, "four"}};

    fat_p::FlatMap<int, std::string> map;
    map.insert(data.begin(), data.end());

    SIMPLE_ASSERT(map.size() == 5, "Should have 5 elements");

    std::vector<int> keys;
    for (const auto& kv : map)
    {
        keys.push_back(kv.first);
    }
    SIMPLE_ASSERT(std::is_sorted(keys.begin(), keys.end()), "Keys should be sorted");

    return true;
}

bool test_flat_map_move_semantics()
{
    fat_p::FlatMap<int, std::string> map1{{1, "one"}, {2, "two"}};

    fat_p::FlatMap<int, std::string> map2(std::move(map1));
    SIMPLE_ASSERT(map2.size() == 2, "Moved-to map should have 2 elements");
    SIMPLE_ASSERT(map2.contains(1), "Moved-to map should contain 1");

    fat_p::FlatMap<int, std::string> map3;
    map3 = std::move(map2);
    SIMPLE_ASSERT(map3.size() == 2, "Assigned-to map should have 2 elements");

    return true;
}

bool test_flat_map_iterator_key_immutability()
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};

    auto it = map.begin();

    (*it).second = "ONE";
    SIMPLE_ASSERT(map.at(1) == "ONE", "Value should be mutable");

    return true;
}

bool test_flat_map_extract()
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    auto it = map.find(2);
    SIMPLE_ASSERT(it != map.end(), "Should find key 2");

    auto extracted = map.extract(it);
    SIMPLE_ASSERT(extracted.first == 2, "Extracted key should be 2");
    SIMPLE_ASSERT(extracted.second == "two", "Extracted value should be 'two'");
    SIMPLE_ASSERT(map.size() == 2, "Map should have 2 elements after extract");
    SIMPLE_ASSERT(!map.contains(2), "Map should not contain key 2 after extract");

    return true;
}

bool test_flat_map_empty_operations()
{
    fat_p::FlatMap<int, int> map;

    SIMPLE_ASSERT(map.find(1) == map.end(), "find on empty map should return end");
    SIMPLE_ASSERT(!map.contains(1), "contains on empty map should return false");
    SIMPLE_ASSERT(map.count(1) == 0, "count on empty map should return 0");
    SIMPLE_ASSERT(map.lower_bound(1) == map.end(), "lower_bound on empty map should return end");
    SIMPLE_ASSERT(map.upper_bound(1) == map.end(), "upper_bound on empty map should return end");

    auto [first, last] = map.equal_range(1);
    SIMPLE_ASSERT(first == last, "equal_range on empty map should return empty range");

    SIMPLE_ASSERT(map.erase(1) == 0, "erase nonexistent key should return 0");

    return true;
}

bool test_flat_map_heterogeneous_lookup()
{
    // Use std::less<> for transparent comparison
    fat_p::FlatMap<std::string, int, std::less<>> map;
    
    map.insert({"apple", 1});
    map.insert({"banana", 2});
    map.insert({"cherry", 3});
    
    // These lookups should NOT create temporary std::string objects
    // (verified by the fact that they compile with const char*)
    auto it = map.find("banana");
    SIMPLE_ASSERT(it != map.end(), "find with const char* should work");
    SIMPLE_ASSERT(it->second == 2, "find should return correct value");
    
    SIMPLE_ASSERT(map.contains("apple"), "contains with const char* should work");
    SIMPLE_ASSERT(!map.contains("grape"), "contains should return false for missing key");
    
    SIMPLE_ASSERT(map.count("cherry") == 1, "count with const char* should work");
    SIMPLE_ASSERT(map.count("grape") == 0, "count should return 0 for missing key");
    
    auto lb = map.lower_bound("banana");
    SIMPLE_ASSERT(lb != map.end() && lb->first == "banana", "lower_bound with const char* should work");
    
    auto ub = map.upper_bound("banana");
    SIMPLE_ASSERT(ub != map.end() && ub->first == "cherry", "upper_bound with const char* should work");
    
    auto [first, last] = map.equal_range("banana");
    SIMPLE_ASSERT(first != last, "equal_range should find element");
    SIMPLE_ASSERT(first->first == "banana", "equal_range should return correct element");
    
    return true;
}

bool test_flat_map_merge()
{
    fat_p::FlatMap<int, std::string> map1;
    map1.insert({1, "one"});
    map1.insert({3, "three"});
    map1.insert({5, "five"});
    
    fat_p::FlatMap<int, std::string> map2;
    map2.insert({2, "two"});
    map2.insert({3, "THREE"});  // Duplicate key - should keep map1's value
    map2.insert({4, "four"});
    
    map1.merge(map2);
    
    SIMPLE_ASSERT(map1.size() == 5, "Merged map should have 5 elements");
    SIMPLE_ASSERT(map2.empty(), "Source map should be empty after merge");
    
    SIMPLE_ASSERT(map1.at(1) == "one", "Element 1 should be preserved");
    SIMPLE_ASSERT(map1.at(2) == "two", "Element 2 should be merged");
    SIMPLE_ASSERT(map1.at(3) == "three", "Duplicate key should keep original value");
    SIMPLE_ASSERT(map1.at(4) == "four", "Element 4 should be merged");
    SIMPLE_ASSERT(map1.at(5) == "five", "Element 5 should be preserved");
    
    // Verify sorted order
    int prev = -1;
    for (const auto& [k, v] : map1)
    {
        SIMPLE_ASSERT(k > prev, "Merged map should maintain sorted order");
        prev = k;
    }
    
    // Test merge with empty source
    fat_p::FlatMap<int, std::string> empty;
    size_t size_before = map1.size();
    map1.merge(empty);
    SIMPLE_ASSERT(map1.size() == size_before, "Merging empty map should not change size");
    
    // Test merge into empty target
    fat_p::FlatMap<int, std::string> target;
    fat_p::FlatMap<int, std::string> source;
    source.insert({10, "ten"});
    source.insert({20, "twenty"});
    target.merge(source);
    SIMPLE_ASSERT(target.size() == 2, "Merge into empty should work");
    SIMPLE_ASSERT(source.empty(), "Source should be empty after merge");
    
    return true;
}

void benchmark_flatmap()
{
    std::cout << colors::cyan() << "FlatMap Benchmarks (1k elements):" << colors::reset() << "\n";

    constexpr int N = 1000;
    fat_p::FlatMap<int, int> map;

    double insert_time = measure_perf(
        [&map, i = 0]() mutable {
            map.insert({i % N, i});
            ++i;
        },
        100000,
        1000);
    std::cout << "Insert (random): " << format_time(insert_time) << "\n";

    map.clear();
    map.reserve(N);
    double insert_sorted_time = measure_perf(
        [&map, i = 0]() mutable {
            if (map.size() < N)
            {
                map.insert({static_cast<int>(map.size()), i});
            }
            ++i;
        },
        10000,
        100);
    std::cout << "Insert (sorted, reserved): " << format_time(insert_sorted_time) << "\n";

    map.clear();
    for (int i = 0; i < N; ++i)
    {
        map.insert({i, i * 10});
    }

    // Use varying keys and accumulate results to prevent optimization
    volatile int find_accumulator = 0;
    double find_time = measure_perf(
        [&map, &find_accumulator, i = 0]() mutable {
            auto it = map.find(i % N);
            if (it != map.end())
            {
                find_accumulator += it->second;
            }
            ++i;
        },
        1000000,
        10000);
    std::cout << "Find: " << format_time(find_time) << "\n";
    DoNotOptimize(find_accumulator);

    // Force actual iteration by using volatile accumulator
    double iter_time = measure_perf(
        [&map]() {
            volatile int sum = 0;
            for (const auto& kv : map)
            {
                sum += kv.second;
            }
            DoNotOptimize(sum);
        },
        100000,
        1000);
    std::cout << "Iteration (1000 elements): " << format_time(iter_time) << "\n";

    std::map<int, int> std_map;
    for (int i = 0; i < N; ++i)
    {
        std_map.insert({i, i * 10});
    }

    // Use varying keys and accumulate for std::map find too
    volatile int std_find_accumulator = 0;
    double std_find_time = measure_perf(
        [&std_map, &std_find_accumulator, i = 0]() mutable {
            auto it = std_map.find(i % N);
            if (it != std_map.end())
            {
                std_find_accumulator += it->second;
            }
            ++i;
        },
        1000000,
        10000);
    std::cout << "std::map Find: " << format_time(std_find_time) << "\n";
    DoNotOptimize(std_find_accumulator);

    double std_iter_time = measure_perf(
        [&std_map]() {
            volatile int sum = 0;
            for (const auto& [k, v] : std_map)
            {
                sum += v;
            }
            DoNotOptimize(sum);
        },
        100000,
        1000);
    std::cout << "std::map Iteration (1000 elements): " << format_time(std_iter_time) << "\n";
}

void benchmark_flatmap_large_scale()
{
    std::cout << "\n" << colors::cyan() 
              << "Large-Scale Benchmarks (100k elements, cache stress):" 
              << colors::reset() << "\n";
    
    // 100,000 elements = ~800KB data (exceeds typical L1/L2 cache)
    constexpr int N = 100000;
    
    // Prepare random access pattern to defeat prefetcher
    std::vector<int> random_keys(N);
    std::iota(random_keys.begin(), random_keys.end(), 0);
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::shuffle(random_keys.begin(), random_keys.end(), rng);

    // Build FlatMap
    fat_p::FlatMap<int, int> map;
    map.reserve(N);
    for (int i = 0; i < N; ++i)
    {
        map.insert({i, i * 10});
    }

    // Build std::map
    std::map<int, int> std_map;
    for (int i = 0; i < N; ++i)
    {
        std_map.insert({i, i * 10});
    }

    // FlatMap Find (random access pattern)
    volatile int find_accumulator = 0;
    double find_time = measure_perf(
        [&map, &random_keys, &find_accumulator, i = 0]() mutable {
            auto it = map.find(random_keys[i % N]);
            if (it != map.end())
            {
                find_accumulator += it->second;
            }
            ++i;
        },
        100000,
        1000);
    std::cout << "FlatMap Find (random access): " << format_time(find_time) << "\n";
    DoNotOptimize(find_accumulator);

    // std::map Find (random access pattern)
    volatile int std_find_accumulator = 0;
    double std_find_time = measure_perf(
        [&std_map, &random_keys, &std_find_accumulator, i = 0]() mutable {
            auto it = std_map.find(random_keys[i % N]);
            if (it != std_map.end())
            {
                std_find_accumulator += it->second;
            }
            ++i;
        },
        100000,
        1000);
    std::cout << "std::map Find (random access): " << format_time(std_find_time) << "\n";
    DoNotOptimize(std_find_accumulator);

    // FlatMap Iteration (sequential - prefetcher works)
    double iter_time = measure_perf(
        [&map]() {
            volatile int sum = 0;
            for (const auto& kv : map)
            {
                sum += kv.second;
            }
            DoNotOptimize(sum);
        },
        1000,
        100);
    std::cout << "FlatMap Iteration (100k elements): " << format_time(iter_time) << "\n";

    // std::map Iteration (pointer chasing - prefetcher fails)
    double std_iter_time = measure_perf(
        [&std_map]() {
            volatile int sum = 0;
            for (const auto& [k, v] : std_map)
            {
                sum += v;
            }
            DoNotOptimize(sum);
        },
        1000,
        100);
    std::cout << "std::map Iteration (100k elements): " << format_time(std_iter_time) << "\n";
}

} // anonymous namespace

bool test_FlatMap()
{
    PRINT_HEADER(FLAT MAP)

    // Print system information for benchmark context
    auto sys_info = SystemInfo::capture();
    sys_info.print();

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
    RUN_TEST(runner, flat_map_custom_comparator);
    RUN_TEST(runner, flat_map_case_insensitive_comparator);
    RUN_TEST(runner, flat_map_reserve_capacity);
    RUN_TEST(runner, flat_map_equality_operators);
    RUN_TEST(runner, flat_map_swap);
    RUN_TEST(runner, flat_map_try_emplace);
    RUN_TEST(runner, flat_map_emplace_hint);
    RUN_TEST(runner, flat_map_range_insert);
    RUN_TEST(runner, flat_map_move_semantics);
    RUN_TEST(runner, flat_map_iterator_key_immutability);
    RUN_TEST(runner, flat_map_extract);
    RUN_TEST(runner, flat_map_empty_operations);
    RUN_TEST(runner, flat_map_heterogeneous_lookup);
    RUN_TEST(runner, flat_map_merge);

    benchmark_flatmap();
    benchmark_flatmap_large_scale();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FlatMap() ? 0 : 1;
}
#endif
