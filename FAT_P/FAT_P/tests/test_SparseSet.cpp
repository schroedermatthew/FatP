/**
 * @file test_SparseSet.cpp
 * @brief Comprehensive unit tests for SparseSet.h
 */
/*
FATP_META:
  meta_version: 1
  component: SparseSet
  file_role: test
  path: tests/test_SparseSet.cpp
  namespace: fat_p::testing::sparseset
  summary: "Unit tests for SparseSet."
  related:
    docs_search: "SparseSet"
    headers:
      - fat_p/SparseSet.h
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

#include "FatPTest.h"
#include "SparseSet.h"

namespace fat_p::testing::sparseset
{

FATP_TEST_CASE(sparse_set_basic_operations)
{
    SparseSet<uint32_t> set;

    FATP_ASSERT_TRUE(set.empty(), "Should start empty");
    FATP_ASSERT_TRUE(set.insert(100), "Should insert");
    FATP_ASSERT_TRUE(!set.empty(), "Should not be empty");
    FATP_ASSERT_TRUE(set.contains(100), "Should contain inserted element");
    FATP_ASSERT_TRUE(!set.contains(99), "Should not contain non-inserted element");

    return true;
}

FATP_TEST_CASE(sparse_set_sparse_indices)
{
    SparseSet<uint32_t> set;

    // Insert sparse indices
    set.insert(10);
    set.insert(1000);
    set.insert(100000);

    FATP_ASSERT_TRUE(set.size() == 3, "Should have 3 elements");
    FATP_ASSERT_TRUE(set.contains(10), "Should contain 10");
    FATP_ASSERT_TRUE(set.contains(1000), "Should contain 1000");
    FATP_ASSERT_TRUE(set.contains(100000), "Should contain 100000");

    return true;
}

FATP_TEST_CASE(sparse_set_erase)
{
    SparseSet<uint32_t> set;

    set.insert(1);
    set.insert(2);
    set.insert(3);

    FATP_ASSERT_TRUE(set.erase(2), "Should erase existing element");
    FATP_ASSERT_TRUE(!set.contains(2), "Should not contain erased element");
    FATP_ASSERT_TRUE(set.size() == 2, "Size should decrease");
    FATP_ASSERT_TRUE(!set.erase(2), "Should not erase non-existent element");

    return true;
}

FATP_TEST_CASE(sparse_set_iteration)
{
    SparseSet<uint32_t> set;

    set.insert(100);
    set.insert(200);
    set.insert(300);

    std::vector<uint32_t> values;
    for (uint32_t val : set)
    {
        values.push_back(val);
    }

    FATP_ASSERT_TRUE(values.size() == 3, "Should iterate over all elements");
    FATP_ASSERT_TRUE(std::find(values.begin(), values.end(), 100) != values.end(), "Should find 100");
    FATP_ASSERT_TRUE(std::find(values.begin(), values.end(), 200) != values.end(), "Should find 200");
    FATP_ASSERT_TRUE(std::find(values.begin(), values.end(), 300) != values.end(), "Should find 300");

    return true;
}

FATP_TEST_CASE(sparse_set_with_data)
{
    SparseSetWithData<uint32_t, std::string> set;

    set.insert(1, "one");
    set.insert(2, "two");
    set.insert(3, "three");

    FATP_ASSERT_TRUE(set.get(1) == "one", "Should retrieve correct data");
    FATP_ASSERT_TRUE(set.get(2) == "two", "Should retrieve correct data");
    FATP_ASSERT_TRUE(set.get(3) == "three", "Should retrieve correct data");

    set.erase(2);
    FATP_ASSERT_TRUE(!set.contains(2), "Should not contain erased element");
    FATP_ASSERT_TRUE(set.get(1) == "one", "Other data should remain valid");
    FATP_ASSERT_TRUE(set.get(3) == "three", "Other data should remain valid");

    return true;
}

void benchmark_sparse_set()
{
    std::cout << "\n" << colors::cyan() << "SparseSet Benchmarks:" << colors::reset() << "\n\n";

    SparseSet<uint32_t> set;
    set.reserve(100000);

    // Benchmark insert
    double insert_time = measure_perf(
        [&set, i = 0]() mutable {
            set.insert(i * 100);
            ++i;
        },
        10000,
        100);
    std::cout << "Insert: " << format_time(insert_time) << "\n";

    // Benchmark contains
    double contains_time = measure_perf(
        [&set]() {
            set.contains(50000);
        },
        100000,
        1000);
    std::cout << "Contains: " << format_time(contains_time) << "\n";

    // Benchmark erase
    double erase_time = measure_perf(
        [&set, i = 0]() mutable {
            set.erase(i * 100);
            ++i;
        },
        1000,
        10);
    std::cout << "Erase: " << format_time(erase_time) << "\n";
}

} // namespace fat_p::testing::sparseset

namespace fat_p::testing
{

bool test_SparseSet()
{
    FATP_PRINT_HEADER(SPARSE SET)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, sparseset, sparse_set_basic_operations);
    FATP_RUN_TEST_NS(runner, sparseset, sparse_set_sparse_indices);
    FATP_RUN_TEST_NS(runner, sparseset, sparse_set_erase);
    FATP_RUN_TEST_NS(runner, sparseset, sparse_set_iteration);
    FATP_RUN_TEST_NS(runner, sparseset, sparse_set_with_data);

    sparseset::benchmark_sparse_set();

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
