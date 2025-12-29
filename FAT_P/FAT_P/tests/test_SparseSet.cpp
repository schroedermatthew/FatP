/**
 * @file test_SparseSet.cpp
 * @brief Comprehensive unit tests for SparseSet.h
 */

#include <iostream>

#include "SparseSet.h"
#include "FatPTest.h"

namespace fat_p::testing
{

bool test_sparse_set_basic_operations() {
    SparseSet<uint32_t> set;
    
    ASSERT_TRUE(set.empty(), "Should start empty");
    ASSERT_TRUE(set.insert(100), "Should insert");
    ASSERT_TRUE(!set.empty(), "Should not be empty");
    ASSERT_TRUE(set.contains(100), "Should contain inserted element");
    ASSERT_TRUE(!set.contains(99), "Should not contain non-inserted element");
    
    return true;
}

bool test_sparse_set_sparse_indices() {
    SparseSet<uint32_t> set;
    
    // Insert sparse indices
    set.insert(10);
    set.insert(1000);
    set.insert(100000);
    
    ASSERT_TRUE(set.size() == 3, "Should have 3 elements");
    ASSERT_TRUE(set.contains(10), "Should contain 10");
    ASSERT_TRUE(set.contains(1000), "Should contain 1000");
    ASSERT_TRUE(set.contains(100000), "Should contain 100000");
    
    return true;
}

bool test_sparse_set_erase() {
    SparseSet<uint32_t> set;
    
    set.insert(1);
    set.insert(2);
    set.insert(3);
    
    ASSERT_TRUE(set.erase(2), "Should erase existing element");
    ASSERT_TRUE(!set.contains(2), "Should not contain erased element");
    ASSERT_TRUE(set.size() == 2, "Size should decrease");
    ASSERT_TRUE(!set.erase(2), "Should not erase non-existent element");
    
    return true;
}

bool test_sparse_set_iteration() {
    SparseSet<uint32_t> set;
    
    set.insert(100);
    set.insert(200);
    set.insert(300);
    
    std::vector<uint32_t> values;
    for (uint32_t val : set) {
        values.push_back(val);
    }
    
    ASSERT_TRUE(values.size() == 3, "Should iterate over all elements");
    ASSERT_TRUE(std::find(values.begin(), values.end(), 100) != values.end(), "Should find 100");
    ASSERT_TRUE(std::find(values.begin(), values.end(), 200) != values.end(), "Should find 200");
    ASSERT_TRUE(std::find(values.begin(), values.end(), 300) != values.end(), "Should find 300");
    
    return true;
}

bool test_sparse_set_with_data() {
    SparseSetWithData<uint32_t, std::string> set;
    
    set.insert(1, "one");
    set.insert(2, "two");
    set.insert(3, "three");
    
    ASSERT_TRUE(set.get(1) == "one", "Should retrieve correct data");
    ASSERT_TRUE(set.get(2) == "two", "Should retrieve correct data");
    ASSERT_TRUE(set.get(3) == "three", "Should retrieve correct data");
    
    set.erase(2);
    ASSERT_TRUE(!set.contains(2), "Should not contain erased element");
    ASSERT_TRUE(set.get(1) == "one", "Other data should remain valid");
    ASSERT_TRUE(set.get(3) == "three", "Other data should remain valid");
    
    return true;
}

void benchmark_sparse_set() {
    std::cout << "\n" << colors::cyan() << "SparseSet Benchmarks:" << colors::reset() << "\n\n";
    
    SparseSet<uint32_t> set;
    set.reserve(100000);
    
    // Benchmark insert
    double insert_time = measure_perf([&set, i=0]() mutable {
        set.insert(i * 100);
        ++i;
    }, 10000, 100);
    std::cout << "Insert: " << format_time(insert_time) << "\n";
    
    // Benchmark contains
    double contains_time = measure_perf([&set]() {
        set.contains(50000);
    }, 100000, 1000);
    std::cout << "Contains: " << format_time(contains_time) << "\n";
    
    // Benchmark erase
    double erase_time = measure_perf([&set, i=0]() mutable {
        set.erase(i * 100);
        ++i;
    }, 1000, 10);
    std::cout << "Erase: " << format_time(erase_time) << "\n";
}

bool test_SparseSet() {

    PRINT_HEADER(SPARSE SET)

    TestRunner runner;

    RUN_TEST(runner, sparse_set_basic_operations);
    RUN_TEST(runner, sparse_set_sparse_indices);
    RUN_TEST(runner, sparse_set_erase);
    RUN_TEST(runner, sparse_set_iteration);
    RUN_TEST(runner, sparse_set_with_data);

    benchmark_sparse_set();

    return 0 == runner.print_summary();

}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SparseSet() ? 0 : 1;
}
#endif
