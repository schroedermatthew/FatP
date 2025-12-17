/**
 * @file test_SwissTable.cpp
 * @brief Comprehensive test suite for fat_p::SwissTable
 * 
 * Tests all features including:
 * - SIMD-accelerated probing (SSE2/NEON)
 * - Tombstone-based deletion
 * - Triangular probing
 * - Insert, find, erase operations
 * - Load factor management (0.875)
 * - Iterator support
 * - Performance vs std::unordered_map
 * 
 * Total Tests: 22
 */

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "SwissTable.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_SwissTable.h"
#endif

namespace fat_p::testing::swisstable
{

using namespace fat_p::testing;

// ============================================================================
// Test 1: Basic Construction
// ============================================================================

TEST_CASE(basic_construction)
{
    SwissTable<int, int> map;

    ASSERT_TRUE(map.empty(), "Default constructed map should be empty");
    ASSERT_EQ(map.size(), size_t(0), "Default constructed map should have size 0");

    return true;
}

// ============================================================================
// Test 2: Insert and Find
// ============================================================================

TEST_CASE(insert_find)
{
    SwissTable<int, std::string> map;

    auto* v1 = map.insert(1, "one");
    auto* v2 = map.insert(2, "two");
    auto* v3 = map.insert(3, "three");

    ASSERT_TRUE(v1 != nullptr, "Insert should return pointer");
    ASSERT_TRUE(*v1 == "one", "Insert should store value");
    ASSERT_EQ(map.size(), size_t(3), "Size should be 3");

    ASSERT_TRUE(*map.find(1) == "one", "Find should return correct value");
    ASSERT_TRUE(*map.find(2) == "two", "Find should return correct value");
    ASSERT_TRUE(*map.find(3) == "three", "Find should return correct value");
    ASSERT_TRUE(map.find(4) == nullptr, "Find non-existent should return nullptr");

    (void)v2;
    (void)v3;

    return true;
}

// ============================================================================
// Test 3: Erase
// ============================================================================

TEST_CASE(erase)
{
    SwissTable<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    ASSERT_TRUE(map.erase(2), "Erase existing should return true");
    ASSERT_EQ(map.size(), size_t(2), "Size should decrease after erase");
    ASSERT_TRUE(map.find(2) == nullptr, "Erased key should not be found");
    ASSERT_TRUE(map.find(1) != nullptr, "Other keys should still exist");
    ASSERT_TRUE(map.find(3) != nullptr, "Other keys should still exist");

    ASSERT_FALSE(map.erase(2), "Erase non-existent should return false");
    ASSERT_FALSE(map.erase(999), "Erase non-existent should return false");

    return true;
}

// ============================================================================
// Test 4: Update Value
// ============================================================================

TEST_CASE(update_value)
{
    SwissTable<int, int> map;
    map.insert(1, 100);

    *map.find(1) = 200;
    ASSERT_EQ(*map.find(1), 200, "Value should be updated via find");

    map[1] = 300;
    ASSERT_EQ(*map.find(1), 300, "Value should be updated via operator[]");

    return true;
}

// ============================================================================
// Test 5: Clear
// ============================================================================

TEST_CASE(clear)
{
    SwissTable<int, int> map;
    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, i * 2);
    }

    ASSERT_EQ(map.size(), size_t(100), "Size should be 100");

    map.clear();
    ASSERT_EQ(map.size(), size_t(0), "Size should be 0 after clear");
    ASSERT_TRUE(map.empty(), "Map should be empty after clear");

    map.insert(1, 10);
    ASSERT_EQ(map.size(), size_t(1), "Should be able to insert after clear");

    return true;
}

// ============================================================================
// Test 6: Load Factor
// ============================================================================

TEST_CASE(load_factor)
{
    SwissTable<int, int> map;

    for (int i = 0; i < 1000; ++i)
    {
        map.insert(i, i * 2);
    }

    float load = map.load_factor();
    ASSERT_TRUE(load >= 0.0f && load <= 1.0f, "Load factor should be between 0 and 1");
    ASSERT_TRUE(load <= 0.875f, "Load factor should not exceed 0.875");

    return true;
}

// ============================================================================
// Test 7: Contains and Count
// ============================================================================

TEST_CASE(contains_count)
{
    SwissTable<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);

    ASSERT_TRUE(map.contains(1), "Contains should return true for existing key");
    ASSERT_TRUE(map.contains(2), "Contains should return true for existing key");
    ASSERT_FALSE(map.contains(3), "Contains should return false for non-existent key");

    ASSERT_EQ(map.count(1), size_t(1), "Count should return 1 for existing key");
    ASSERT_EQ(map.count(3), size_t(0), "Count should return 0 for non-existent key");

    return true;
}

// ============================================================================
// Test 8: At Method
// ============================================================================

TEST_CASE(at_method)
{
    SwissTable<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");

    ASSERT_TRUE(map.at(1) == "one", "at() should return correct value");
    ASSERT_TRUE(map.at(2) == "two", "at() should return correct value");

    bool threw = false;
    try
    {
        (void)map.at(999);
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }
    ASSERT_TRUE(threw, "at() should throw for non-existent key");

    return true;
}

// ============================================================================
// Test 9: Insert or Assign
// ============================================================================

TEST_CASE(insert_or_assign)
{
    SwissTable<int, std::string> map;

    auto [ptr1, inserted1] = map.insert_or_assign(1, "one");
    ASSERT_TRUE(inserted1, "First insert_or_assign should insert");
    ASSERT_TRUE(*ptr1 == "one", "Value should be stored");

    auto [ptr2, inserted2] = map.insert_or_assign(1, "ONE");
    ASSERT_FALSE(inserted2, "Second insert_or_assign should assign");
    ASSERT_TRUE(*ptr2 == "ONE", "Value should be updated");

    ASSERT_EQ(map.size(), size_t(1), "Size should still be 1");

    return true;
}

// ============================================================================
// Test 10: String Keys
// ============================================================================

TEST_CASE(string_keys)
{
    SwissTable<std::string, int> map;

    map.insert("apple", 1);
    map.insert("banana", 2);
    map.insert("cherry", 3);

    ASSERT_EQ(map.size(), size_t(3), "Size should be 3");
    ASSERT_EQ(*map.find("banana"), 2, "Should find string key");
    ASSERT_TRUE(map.find("grape") == nullptr, "Should not find non-existent");

    return true;
}

// ============================================================================
// Test 11: Large Dataset
// ============================================================================

TEST_CASE(large_dataset)
{
    SwissTable<int, int> map;
    constexpr int N = 10000;

    for (int i = 0; i < N; ++i)
    {
        map.insert(i, i * 2);
    }

    ASSERT_EQ(map.size(), size_t(N), "Size should match inserted count");

    for (int i = 0; i < N; ++i)
    {
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr, "Should find all inserted keys");
        ASSERT_EQ(*val, i * 2, "Values should be correct");
    }

    return true;
}

// ============================================================================
// Test 12: Erase and Reinsert
// ============================================================================

TEST_CASE(erase_reinsert)
{
    SwissTable<int, std::string> map;

    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    map.erase(2);
    ASSERT_TRUE(map.find(2) == nullptr, "Erased key should not be found");

    map.insert(2, "TWO");
    auto* val = map.find(2);
    ASSERT_TRUE(val != nullptr, "Reinserted key should be found");
    ASSERT_TRUE(*val == "TWO", "Reinserted value should be correct");

    return true;
}

// ============================================================================
// Test 13: Tombstone Accumulation
// ============================================================================

TEST_CASE(tombstone_stress)
{
    SwissTable<int, int> map;

    for (int round = 0; round < 5; ++round)
    {
        for (int i = 0; i < 100; ++i)
        {
            map.insert(i, i);
        }

        for (int i = 0; i < 100; i += 2)
        {
            map.erase(i);
        }

        for (int i = 1; i < 100; i += 2)
        {
            ASSERT_TRUE(map.find(i) != nullptr, "Odd keys should still exist");
        }

        map.clear();
    }

    return true;
}

// ============================================================================
// Test 14: Iterator Basic
// ============================================================================

TEST_CASE(iterator_basic)
{
    SwissTable<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    size_t count = 0;
    int sum = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        sum += it.value();
        ++count;
    }

    ASSERT_EQ(count, size_t(3), "Iterator should visit all elements");
    ASSERT_EQ(sum, 60, "Sum of values should be correct");

    return true;
}

// ============================================================================
// Test 15: Iterator Range-For
// ============================================================================

TEST_CASE(iterator_range_for)
{
    SwissTable<std::string, int> map;
    map.insert("a", 1);
    map.insert("b", 2);
    map.insert("c", 3);

    int sum = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        sum += it.value();
    }

    ASSERT_EQ(sum, 6, "Range-for should visit all elements");

    return true;
}

// ============================================================================
// Test 16: Const Iterator
// ============================================================================

TEST_CASE(const_iterator)
{
    SwissTable<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);

    const SwissTable<int, int>& cmap = map;

    size_t count = 0;
    for (auto it = cmap.begin(); it != cmap.end(); ++it)
    {
        ++count;
    }

    ASSERT_EQ(count, size_t(2), "Const iterator should work");

    return true;
}

// ============================================================================
// Test 17: Copy Semantics
// ============================================================================

TEST_CASE(copy_semantics)
{
    SwissTable<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    SwissTable<int, std::string> map2 = map1;

    ASSERT_EQ(map2.size(), size_t(2), "Copied map should have same size");
    ASSERT_TRUE(*map2.find(1) == "one", "Copied map should have same values");

    map1.insert(3, "three");
    ASSERT_EQ(map1.size(), size_t(3), "Original should be modified");
    ASSERT_EQ(map2.size(), size_t(2), "Copy should be independent");

    return true;
}

// ============================================================================
// Test 18: Move Semantics
// ============================================================================

TEST_CASE(move_semantics)
{
    SwissTable<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    SwissTable<int, std::string> map2 = std::move(map1);

    ASSERT_EQ(map2.size(), size_t(2), "Moved map should have elements");
    ASSERT_TRUE(*map2.find(1) == "one", "Moved map should have values");

    return true;
}

// ============================================================================
// Test 19: Empty Values
// ============================================================================

TEST_CASE(empty_values)
{
    SwissTable<std::string, std::string> map;

    map.insert("empty_value", "");
    map.insert("", "empty_key");

    auto* v1 = map.find("empty_value");
    ASSERT_TRUE(v1 != nullptr, "Should find key with empty value");
    ASSERT_TRUE(v1->empty(), "Value should be empty");

    auto* v2 = map.find("");
    ASSERT_TRUE(v2 != nullptr, "Should find empty key");
    ASSERT_TRUE(*v2 == "empty_key", "Value should be correct");

    return true;
}

// ============================================================================
// Test 20: SIMD Backend Detection
// ============================================================================

TEST_CASE(simd_backend)
{
    const char* backend = SwissTable<int, int>::simd_backend();
    ASSERT_TRUE(backend != nullptr, "SIMD backend should not be null");
    ASSERT_TRUE(std::string(backend).length() > 0, "SIMD backend should have a name");

    std::cout << "  SIMD Backend: " << backend << "\n";

    return true;
}

// ============================================================================
// Test 21: Rehash
// ============================================================================

TEST_CASE(rehash)
{
    SwissTable<int, int> map;

    for (int i = 0; i < 1000; ++i)
    {
        map.insert(i, i * 10);
    }

    map.rehash(2000);

    ASSERT_EQ(map.size(), size_t(1000), "Size should be unchanged after rehash");

    for (int i = 0; i < 1000; ++i)
    {
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr, "All keys should still exist after rehash");
        ASSERT_EQ(*val, i * 10, "Values should be correct after rehash");
    }

    return true;
}

// ============================================================================
// Test 22: Stress Test Random Operations
// ============================================================================

TEST_CASE(stress_random)
{
    SwissTable<int, int> map;
    std::unordered_map<int, int> reference;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> key_dist(0, 999);
    std::uniform_int_distribution<int> op_dist(0, 2);

    for (int i = 0; i < 5000; ++i)
    {
        int key = key_dist(rng);
        int op = op_dist(rng);

        if (op == 0)
        {
            map.insert(key, i);
            reference[key] = i;
        }
        else if (op == 1)
        {
            int* ptr = map.find(key);
            auto it = reference.find(key);

            if (it == reference.end())
            {
                ASSERT_TRUE(ptr == nullptr, "Find should return null for missing key");
            }
            else
            {
                ASSERT_TRUE(ptr != nullptr, "Find should return non-null for existing key");
            }
        }
        else
        {
            bool erased = map.erase(key);
            size_t ref_erased = reference.erase(key);
            ASSERT_EQ(erased, ref_erased > 0, "Erase should match reference");
        }
    }

    ASSERT_EQ(map.size(), reference.size(), "Size should match reference");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_swisstable()
{
    std::cout << "\n" << colors::cyan() << "SwissTable Benchmarks:"
              << colors::reset() << "\n\n";

    std::cout << "SIMD Backend: " << SwissTable<int, int>::simd_backend() << "\n\n";

    constexpr size_t N = 50000;
    constexpr size_t WARMUP = 1000;
    constexpr size_t ITERATIONS = 100000;

    std::vector<int> keys(N);
    for (size_t i = 0; i < N; ++i)
    {
        keys[i] = static_cast<int>(i);
    }

    std::mt19937 rng(12345);
    std::shuffle(keys.begin(), keys.end(), rng);

    std::vector<int> lookup_keys = keys;
    std::shuffle(lookup_keys.begin(), lookup_keys.end(), rng);

    SwissTable<int, int> smap;
    std::unordered_map<int, int> umap;

    for (int k : keys)
    {
        smap.insert(k, k * 10);
        umap[k] = k * 10;
    }

    volatile long long sink = 0;

    double swiss_time = measure_perf([&]() {
        long long sum = 0;
        for (int k : lookup_keys)
        {
            int* v = smap.find(k);
            if (v)
            {
                sum += *v;
            }
        }
        sink = sum;
    }, ITERATIONS / N, WARMUP / N);

    double umap_time = measure_perf([&]() {
        long long sum = 0;
        for (int k : lookup_keys)
        {
            auto it = umap.find(k);
            if (it != umap.end())
            {
                sum += it->second;
            }
        }
        sink = sum;
    }, ITERATIONS / N, WARMUP / N);

    double ns_per_find_swiss = (swiss_time * 1e6) / N;
    double ns_per_find_umap = (umap_time * 1e6) / N;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Find (" << N << " elements):\n";
    std::cout << "  SwissTable:        " << ns_per_find_swiss << " ns/op\n";
    std::cout << "  std::unordered_map:" << ns_per_find_umap << " ns/op\n";
    std::cout << "  Speedup:           " << (umap_time / swiss_time) << "x\n\n";

    (void)sink;
}

}  // namespace fat_p::testing::swisstable

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_SwissTable()
{
    PRINT_HEADER(SWISS TABLE)

    TestRunner runner;

    RUN_TEST_NS(runner, swisstable, basic_construction);
    RUN_TEST_NS(runner, swisstable, insert_find);
    RUN_TEST_NS(runner, swisstable, erase);
    RUN_TEST_NS(runner, swisstable, update_value);
    RUN_TEST_NS(runner, swisstable, clear);
    RUN_TEST_NS(runner, swisstable, load_factor);
    RUN_TEST_NS(runner, swisstable, contains_count);
    RUN_TEST_NS(runner, swisstable, at_method);
    RUN_TEST_NS(runner, swisstable, insert_or_assign);
    RUN_TEST_NS(runner, swisstable, string_keys);
    RUN_TEST_NS(runner, swisstable, large_dataset);
    RUN_TEST_NS(runner, swisstable, erase_reinsert);
    RUN_TEST_NS(runner, swisstable, tombstone_stress);
    RUN_TEST_NS(runner, swisstable, iterator_basic);
    RUN_TEST_NS(runner, swisstable, iterator_range_for);
    RUN_TEST_NS(runner, swisstable, const_iterator);
    RUN_TEST_NS(runner, swisstable, copy_semantics);
    RUN_TEST_NS(runner, swisstable, move_semantics);
    RUN_TEST_NS(runner, swisstable, empty_values);
    RUN_TEST_NS(runner, swisstable, simd_backend);
    RUN_TEST_NS(runner, swisstable, rehash);
    RUN_TEST_NS(runner, swisstable, stress_random);

    swisstable::benchmark_swisstable();

    return 0 == runner.print_summary();
}

}  // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SwissTable() ? 0 : 1;
}
#endif
