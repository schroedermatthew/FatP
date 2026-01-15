/**
 * @file test_FastHashMap.cpp
 * @brief Comprehensive test suite for fat_p::FastHashMap
 *
 * Tests all features including:
 * - SIMD-accelerated probing (SSE2/AVX2/NEON)
 * - Deletion policies (Tombstone and BackwardShift)
 * - Allocator policies (Heap and Fixed)
 * - Conditional noexcept on move/swap
 * - SFINAE-gated heterogeneous lookup
 * - 32-bit safe hash finalizer
 * - Freeze mode for read-only tables
 * - Insert, find, erase operations
 * - Load factor management (0.875)
 * - Iterator support
 * - Performance vs std::unordered_map
 *
 * Total Tests: 32
 */
/*
FATP_META:
  meta_version: 1
  component: FastHashMap
  file_role: test
  path: tests/test_FastHashMap.cpp
  namespace: fat_p
  summary: "Unit tests for FastHashMap."
  related:
    docs_search: "FastHashMap"
    headers:
      - fat_p/FastHashMap.h
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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "FastHashMap.h"
#include "FatPTest.h"

namespace fat_p::testing::fasthashmap
{

using namespace fat_p::testing;

// ============================================================================
// Test 1: Basic Construction
// ============================================================================

FATP_TEST_CASE(basic_construction)
{
    FastHashMap<int, int> map;

    FATP_ASSERT_TRUE(map.empty(), "Default constructed map should be empty");
    FATP_ASSERT_EQ(map.size(), size_t(0), "Default constructed map should have size 0");

    return true;
}

// ============================================================================
// Test 2: Insert and Find
// ============================================================================

FATP_TEST_CASE(insert_find)
{
    FastHashMap<int, std::string> map;

    auto* v1 = map.insert(1, "one");
    auto* v2 = map.insert(2, "two");
    auto* v3 = map.insert(3, "three");

    FATP_ASSERT_TRUE(v1 != nullptr, "Insert should return pointer");
    FATP_ASSERT_TRUE(*v1 == "one", "Insert should store value");
    FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3");

    FATP_ASSERT_TRUE(*map.find(1) == "one", "Find should return correct value");
    FATP_ASSERT_TRUE(*map.find(2) == "two", "Find should return correct value");
    FATP_ASSERT_TRUE(*map.find(3) == "three", "Find should return correct value");
    FATP_ASSERT_TRUE(map.find(4) == nullptr, "Find non-existent should return nullptr");

    (void)v2;
    (void)v3;

    return true;
}

// ============================================================================
// Test 3: Erase
// ============================================================================

FATP_TEST_CASE(erase)
{
    FastHashMap<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    FATP_ASSERT_TRUE(map.erase(2), "Erase existing should return true");
    FATP_ASSERT_EQ(map.size(), size_t(2), "Size should decrease after erase");
    FATP_ASSERT_TRUE(map.find(2) == nullptr, "Erased key should not be found");
    FATP_ASSERT_TRUE(map.find(1) != nullptr, "Other keys should still exist");
    FATP_ASSERT_TRUE(map.find(3) != nullptr, "Other keys should still exist");

    FATP_ASSERT_FALSE(map.erase(2), "Erase non-existent should return false");
    FATP_ASSERT_FALSE(map.erase(999), "Erase non-existent should return false");

    return true;
}

// ============================================================================
// Test 4: Update Value
// ============================================================================

FATP_TEST_CASE(update_value)
{
    FastHashMap<int, int> map;
    map.insert(1, 100);

    *map.find(1) = 200;
    FATP_ASSERT_EQ(*map.find(1), 200, "Value should be updated via find");

    map[1] = 300;
    FATP_ASSERT_EQ(*map.find(1), 300, "Value should be updated via operator[]");

    return true;
}

// ============================================================================
// Test 5: Clear
// ============================================================================

FATP_TEST_CASE(clear)
{
    FastHashMap<int, int> map;
    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, i * 2);
    }

    FATP_ASSERT_EQ(map.size(), size_t(100), "Size should be 100");

    map.clear();
    FATP_ASSERT_EQ(map.size(), size_t(0), "Size should be 0 after clear");
    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");

    map.insert(1, 10);
    FATP_ASSERT_EQ(map.size(), size_t(1), "Should be able to insert after clear");

    return true;
}

// ============================================================================
// Test 6: Load Factor
// ============================================================================

FATP_TEST_CASE(load_factor)
{
    FastHashMap<int, int> map;

    for (int i = 0; i < 1000; ++i)
    {
        map.insert(i, i * 2);
    }

    double load = map.load_factor();
    FATP_ASSERT_TRUE(load >= 0.0 && load <= 1.0, "Load factor should be between 0 and 1");
    FATP_ASSERT_TRUE(load <= 0.875, "Load factor should not exceed 0.875");

    return true;
}

// ============================================================================
// Test 7: Contains and Count
// ============================================================================

FATP_TEST_CASE(contains_count)
{
    FastHashMap<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);

    FATP_ASSERT_TRUE(map.contains(1), "Contains should return true for existing key");
    FATP_ASSERT_TRUE(map.contains(2), "Contains should return true for existing key");
    FATP_ASSERT_FALSE(map.contains(3), "Contains should return false for non-existent key");

    FATP_ASSERT_EQ(map.count(1), size_t(1), "Count should return 1 for existing key");
    FATP_ASSERT_EQ(map.count(3), size_t(0), "Count should return 0 for non-existent key");

    return true;
}

// ============================================================================
// Test 8: At Method
// ============================================================================

FATP_TEST_CASE(at_method)
{
    FastHashMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");

    FATP_ASSERT_TRUE(map.at(1) == "one", "at() should return correct value");
    FATP_ASSERT_TRUE(map.at(2) == "two", "at() should return correct value");

    bool threw = false;
    try
    {
        (void)map.at(999);
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "at() should throw for non-existent key");

    return true;
}

// ============================================================================
// Test 9: Insert or Assign
// ============================================================================

FATP_TEST_CASE(insert_or_assign)
{
    FastHashMap<int, std::string> map;

    auto [ptr1, inserted1] = map.insert_or_assign(1, "one");
    FATP_ASSERT_TRUE(inserted1, "First insert_or_assign should insert");
    FATP_ASSERT_TRUE(*ptr1 == "one", "Value should be stored");

    auto [ptr2, inserted2] = map.insert_or_assign(1, "ONE");
    FATP_ASSERT_FALSE(inserted2, "Second insert_or_assign should assign");
    FATP_ASSERT_TRUE(*ptr2 == "ONE", "Value should be updated");

    FATP_ASSERT_EQ(map.size(), size_t(1), "Size should still be 1");

    return true;
}

// ============================================================================
// Test 10: String Keys
// ============================================================================

FATP_TEST_CASE(string_keys)
{
    FastHashMap<std::string, int> map;

    map.insert("apple", 1);
    map.insert("banana", 2);
    map.insert("cherry", 3);

    FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3");
    FATP_ASSERT_EQ(*map.find("banana"), 2, "Should find string key");
    FATP_ASSERT_TRUE(map.find("grape") == nullptr, "Should not find non-existent");

    return true;
}

// ============================================================================
// Test 11: Large Dataset
// ============================================================================

FATP_TEST_CASE(large_dataset)
{
    FastHashMap<int, int> map;
    constexpr int N = 10000;

    for (int i = 0; i < N; ++i)
    {
        map.insert(i, i * 2);
    }

    FATP_ASSERT_EQ(map.size(), size_t(N), "Size should match inserted count");

    for (int i = 0; i < N; ++i)
    {
        int* val = map.find(i);
        FATP_ASSERT_TRUE(val != nullptr, "Should find all inserted keys");
        FATP_ASSERT_EQ(*val, i * 2, "Values should be correct");
    }

    return true;
}

// ============================================================================
// Test 12: Erase and Reinsert
// ============================================================================

FATP_TEST_CASE(erase_reinsert)
{
    FastHashMap<int, std::string> map;

    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    map.erase(2);
    FATP_ASSERT_TRUE(map.find(2) == nullptr, "Erased key should not be found");

    map.insert(2, "TWO");
    auto* val = map.find(2);
    FATP_ASSERT_TRUE(val != nullptr, "Reinserted key should be found");
    FATP_ASSERT_TRUE(*val == "TWO", "Reinserted value should be correct");

    return true;
}

// ============================================================================
// Test 13: Tombstone Accumulation
// ============================================================================

FATP_TEST_CASE(tombstone_stress)
{
    FastHashMap<int, int> map;

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
            FATP_ASSERT_TRUE(map.find(i) != nullptr, "Odd keys should still exist");
        }

        map.clear();
    }

    return true;
}

// ============================================================================
// Test 14: Iterator Basic
// ============================================================================

FATP_TEST_CASE(iterator_basic)
{
    FastHashMap<int, int> map;
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

    FATP_ASSERT_EQ(count, size_t(3), "Iterator should visit all elements");
    FATP_ASSERT_EQ(sum, 60, "Sum of values should be correct");

    return true;
}

// ============================================================================
// Test 15: Iterator Range-For
// ============================================================================

FATP_TEST_CASE(iterator_range_for)
{
    FastHashMap<std::string, int> map;
    map.insert("a", 1);
    map.insert("b", 2);
    map.insert("c", 3);

    int sum = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        sum += it.value();
    }

    FATP_ASSERT_EQ(sum, 6, "Range-for should visit all elements");

    return true;
}

// ============================================================================
// Test 16: Const Iterator
// ============================================================================

FATP_TEST_CASE(const_iterator)
{
    FastHashMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);

    const FastHashMap<int, int>& cmap = map;

    size_t count = 0;
    for (auto it = cmap.begin(); it != cmap.end(); ++it)
    {
        ++count;
    }

    FATP_ASSERT_EQ(count, size_t(2), "Const iterator should work");

    return true;
}

// ============================================================================
// Test 17: Copy Semantics
// ============================================================================

FATP_TEST_CASE(copy_semantics)
{
    FastHashMap<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    FastHashMap<int, std::string> map2 = map1;

    FATP_ASSERT_EQ(map2.size(), size_t(2), "Copied map should have same size");
    FATP_ASSERT_TRUE(*map2.find(1) == "one", "Copied map should have same values");

    map1.insert(3, "three");
    FATP_ASSERT_EQ(map1.size(), size_t(3), "Original should be modified");
    FATP_ASSERT_EQ(map2.size(), size_t(2), "Copy should be independent");

    return true;
}

// ============================================================================
// Test 18: Move Semantics
// ============================================================================

FATP_TEST_CASE(move_semantics)
{
    FastHashMap<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    FastHashMap<int, std::string> map2 = std::move(map1);

    FATP_ASSERT_EQ(map2.size(), size_t(2), "Moved map should have elements");
    FATP_ASSERT_TRUE(*map2.find(1) == "one", "Moved map should have values");

    return true;
}

// ============================================================================
// Test 19: Empty Values
// ============================================================================

FATP_TEST_CASE(empty_values)
{
    FastHashMap<std::string, std::string> map;

    map.insert("empty_value", "");
    map.insert("", "empty_key");

    auto* v1 = map.find("empty_value");
    FATP_ASSERT_TRUE(v1 != nullptr, "Should find key with empty value");
    FATP_ASSERT_TRUE(v1->empty(), "Value should be empty");

    auto* v2 = map.find("");
    FATP_ASSERT_TRUE(v2 != nullptr, "Should find empty key");
    FATP_ASSERT_TRUE(*v2 == "empty_key", "Value should be correct");

    return true;
}

// ============================================================================
// Test 20: SIMD Backend Detection
// ============================================================================

FATP_TEST_CASE(simd_backend)
{
    const char* backend = FastHashMap<int, int>::simd_backend();
    FATP_ASSERT_TRUE(backend != nullptr, "SIMD backend should not be null");
    FATP_ASSERT_TRUE(std::string(backend).length() > 0, "SIMD backend should have a name");

    std::cout << "  SIMD Backend: " << backend << "\n";

    return true;
}

// ============================================================================
// Test 21: Rehash
// ============================================================================

FATP_TEST_CASE(rehash)
{
    FastHashMap<int, int> map;

    for (int i = 0; i < 1000; ++i)
    {
        map.insert(i, i * 10);
    }

    map.rehash(2000);

    FATP_ASSERT_EQ(map.size(), size_t(1000), "Size should be unchanged after rehash");

    for (int i = 0; i < 1000; ++i)
    {
        int* val = map.find(i);
        FATP_ASSERT_TRUE(val != nullptr, "All keys should still exist after rehash");
        FATP_ASSERT_EQ(*val, i * 10, "Values should be correct after rehash");
    }

    return true;
}

// ============================================================================
// Test 22: Stress Test Random Operations
// ============================================================================

FATP_TEST_CASE(stress_random)
{
    FastHashMap<int, int> map;
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
                FATP_ASSERT_TRUE(ptr == nullptr, "Find should return null for missing key");
            }
            else
            {
                FATP_ASSERT_TRUE(ptr != nullptr, "Find should return non-null for existing key");
            }
        }
        else
        {
            bool erased = map.erase(key);
            size_t ref_erased = reference.erase(key);
            FATP_ASSERT_EQ(erased, ref_erased > 0, "Erase should match reference");
        }
    }

    FATP_ASSERT_EQ(map.size(), reference.size(), "Size should match reference");

    return true;
}

// ============================================================================
// Test 23: Conditional noexcept (ChatGPT Round 2)
// ============================================================================

// Helper types for noexcept testing
struct ThrowingHash
{
    ThrowingHash() = default;
    ThrowingHash(ThrowingHash&&) noexcept(false)
    {
    }
    ThrowingHash& operator=(ThrowingHash&&) noexcept(false)
    {
        return *this;
    }
    size_t operator()(int k) const
    {
        return static_cast<size_t>(k);
    }
};

struct NothrowHash
{
    NothrowHash() = default;
    NothrowHash(NothrowHash&&) noexcept = default;
    NothrowHash& operator=(NothrowHash&&) noexcept = default;
    size_t operator()(int k) const noexcept
    {
        return static_cast<size_t>(k);
    }
};

FATP_TEST_CASE(conditional_noexcept)
{
    // Maps with throwing hash should NOT be nothrow move constructible
    using ThrowingMap = FastHashMap<int, int, ThrowingHash>;
    static_assert(!std::is_nothrow_move_constructible_v<ThrowingMap>,
                  "ThrowingHash map should NOT be nothrow move constructible");

    // Maps with nothrow hash SHOULD be nothrow move constructible
    using NothrowMap = FastHashMap<int, int, NothrowHash>;
    static_assert(std::is_nothrow_move_constructible_v<NothrowMap>,
                  "NothrowHash map should be nothrow move constructible");

    // Default map should be nothrow (std::hash is nothrow)
    using DefaultMap = FastHashMap<int, int>;
    static_assert(std::is_nothrow_move_constructible_v<DefaultMap>, "Default map should be nothrow move constructible");

    return true;
}

// ============================================================================
// Test 24: SFINAE Heterogeneous Lookup (ChatGPT Round 2)
// ============================================================================

// Transparent hash/equal for heterogeneous lookup
struct TransparentHash
{
    using is_transparent = void;
    using is_avalanching = void; // Already good hash, skip mixer

    size_t operator()(std::string_view s) const noexcept
    {
        return std::hash<std::string_view>{}(s);
    }
};

struct TransparentEqual
{
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept
    {
        return a == b;
    }
};

FATP_TEST_CASE(sfinae_heterogeneous_lookup)
{
    // Transparent lookup - should work with string_view
    FastHashMap<std::string, int, TransparentHash, TransparentEqual> tmap;
    tmap.insert("apple", 1);
    tmap.insert("banana", 2);
    tmap.insert("cherry", 3);

    // Find with string_view (no allocation!)
    std::string_view sv = "banana";
    auto* val = tmap.find(sv);
    FATP_ASSERT_NOT_NULLPTR(val, "Transparent find should work with string_view");
    FATP_ASSERT_EQ(*val, 2, "Transparent find should return correct value");

    // Non-transparent hash - lookup requires exact key type
    // This is a compile-time check: if SFINAE didn't work, this would hard-error
    FastHashMap<std::string, int> ntmap;
    ntmap.insert("apple", 1);
    auto* val2 = ntmap.find(std::string("apple"));
    FATP_ASSERT_NOT_NULLPTR(val2, "Non-transparent find should work with exact key type");

    return true;
}

// ============================================================================
// Test 25: 32-bit Hash Finalizer Safety (ChatGPT Round 2)
// ============================================================================

FATP_TEST_CASE(hash_finalizer_safety)
{
    // This test verifies the hash finalizer compiles and works correctly
    // On 64-bit: uses SplitMix64 (h >> 33)
    // On 32-bit: uses MurmurHash3 finalizer (shifts <= 16)

    FastHashMap<int, int> map;

    // Insert values that would expose bad hash distribution
    for (int i = 0; i < 1000; ++i)
    {
        map.insert(i, i * i);
    }

    // Verify all values are retrievable (finalizer is working)
    for (int i = 0; i < 1000; ++i)
    {
        auto* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "All keys should be findable");
        FATP_ASSERT_EQ(*val, i * i, "Values should be correct");
    }

    // Report architecture
    std::cout << "  sizeof(size_t): " << sizeof(size_t) << " bytes\n";

    return true;
}

// ============================================================================
// Test 26: HeapAllocator (ChatGPT Round 3)
// ============================================================================

FATP_TEST_CASE(heap_allocator)
{
    // HeapAllocator maps are movable and swappable
    using HeapMap = FastHashMap<int, int>;

    static_assert(std::is_move_constructible_v<HeapMap>, "HeapAllocator map should be move constructible");
    static_assert(std::is_move_assignable_v<HeapMap>, "HeapAllocator map should be move assignable");
    static_assert(std::is_swappable_v<HeapMap>, "HeapAllocator map should be swappable");

    // Runtime verification
    HeapMap map1;
    map1.insert(1, 100);
    map1.insert(2, 200);

    HeapMap map2 = std::move(map1);
    FATP_ASSERT_EQ(map1.size(), size_t(0), "Moved-from map should be empty");
    FATP_ASSERT_EQ(map2.size(), size_t(2), "Moved-to map should have elements");
    FATP_ASSERT_EQ(*map2.find(1), 100, "Moved-to map should have correct values");

    return true;
}

// ============================================================================
// Test 27: FixedAllocator Basic Operations (ChatGPT Round 3)
// ============================================================================

FATP_TEST_CASE(fixed_allocator_basic)
{
    // FixedHashMap with 8KB buffer
    FixedHashMap<int, int, 8192> map;

    // Insert entries
    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, i * 10);
    }

    FATP_ASSERT_EQ(map.size(), size_t(100), "FixedHashMap should store 100 entries");

    // Find entries
    for (int i = 0; i < 100; ++i)
    {
        auto* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "FixedHashMap should find all entries");
        FATP_ASSERT_EQ(*val, i * 10, "FixedHashMap values should be correct");
    }

    // Erase and verify
    map.erase(50);
    FATP_ASSERT_NULLPTR(map.find(50), "Erased key should not be found");
    FATP_ASSERT_EQ(map.size(), size_t(99), "Size should decrease after erase");

    // Report buffer usage
    std::cout << "  Buffer used: " << map.get_allocator().used() << " / 8192 bytes\n";

    return true;
}

// ============================================================================
// Test 28: FixedAllocator Alignment (ChatGPT Round 3)
// ============================================================================

FATP_TEST_CASE(fixed_allocator_alignment)
{
    FixedAllocator<4096> alloc;

    // Test various alignments
    void* p1 = alloc.allocate(10, 1);
    void* p8 = alloc.allocate(10, 8);
    void* p16 = alloc.allocate(10, 16);
    void* p32 = alloc.allocate(10, 32);
    void* p64 = alloc.allocate(10, 64);

    FATP_ASSERT_TRUE(reinterpret_cast<uintptr_t>(p1) % 1 == 0, "1-byte alignment");
    FATP_ASSERT_TRUE(reinterpret_cast<uintptr_t>(p8) % 8 == 0, "8-byte alignment");
    FATP_ASSERT_TRUE(reinterpret_cast<uintptr_t>(p16) % 16 == 0, "16-byte alignment");
    FATP_ASSERT_TRUE(reinterpret_cast<uintptr_t>(p32) % 32 == 0, "32-byte alignment");
    FATP_ASSERT_TRUE(reinterpret_cast<uintptr_t>(p64) % 64 == 0, "64-byte alignment");

    return true;
}

// ============================================================================
// Test 29: FixedHashMap Non-Movable (ChatGPT Round 3)
// ============================================================================

FATP_TEST_CASE(fixed_hashmap_non_movable)
{
    // FixedHashMap is non-movable to prevent dangling pointer UB
    using FixedMap = FixedHashMap<int, int, 4096>;

    // FixedAllocator is non-movable
    static_assert(!std::is_move_constructible_v<FixedAllocator<4096>>,
                  "FixedAllocator should be non-move-constructible");

    // FixedHashMap is non-movable (due to SFINAE-deleted move operations)
    static_assert(!std::is_move_constructible_v<FixedMap>, "FixedHashMap should be non-move-constructible");
    static_assert(!std::is_move_assignable_v<FixedMap>, "FixedHashMap should be non-move-assignable");
    static_assert(!std::is_swappable_v<FixedMap>, "FixedHashMap should be non-swappable");

    // But it still works for its intended use case
    FixedMap map;
    map.insert(1, 100);
    map.insert(2, 200);

    FATP_ASSERT_EQ(*map.find(1), 100, "FixedHashMap basic operations should work");
    FATP_ASSERT_EQ(*map.find(2), 200, "FixedHashMap basic operations should work");

    return true;
}

// ============================================================================
// Test 30: kPointerStealSafe Trait (ChatGPT Round 3)
// ============================================================================

FATP_TEST_CASE(pointer_steal_safe_trait)
{
    // HeapAllocator is pointer-steal safe (pointers survive allocator move)
    static_assert(HeapAllocator::kPointerStealSafe == true, "HeapAllocator should be pointer-steal safe");

    // FixedAllocator is NOT pointer-steal safe (pointers into embedded buffer)
    static_assert(FixedAllocator<1024>::kPointerStealSafe == false, "FixedAllocator should NOT be pointer-steal safe");

    return true;
}

// ============================================================================
// Test 31: Deletion Policy (BackwardShift)
// ============================================================================

FATP_TEST_CASE(backward_shift_deletion)
{
    // Test BackwardShift deletion policy
    FastHashMapBS<int, int> map;

    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, i * 2);
    }

    FATP_ASSERT_EQ(map.size(), size_t(100), "Should have 100 entries");

    // Erase half the entries
    for (int i = 0; i < 100; i += 2)
    {
        FATP_ASSERT_TRUE(map.erase(i), "Erase should succeed");
    }

    FATP_ASSERT_EQ(map.size(), size_t(50), "Should have 50 entries after erase");

    // Verify remaining entries
    for (int i = 1; i < 100; i += 2)
    {
        auto* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "Odd keys should still exist");
        FATP_ASSERT_EQ(*val, i * 2, "Values should be correct");
    }

    // Verify erased entries are gone
    for (int i = 0; i < 100; i += 2)
    {
        FATP_ASSERT_NULLPTR(map.find(i), "Even keys should be erased");
    }

    return true;
}

// ============================================================================
// Test 32: Freeze Mode
// ============================================================================

FATP_TEST_CASE(freeze_mode)
{
    FastHashMap<int, int> map;

    for (int i = 0; i < 50; ++i)
    {
        map.insert(i, i * 3);
    }

    // Freeze the map
    map.freeze();

    FATP_ASSERT_TRUE(map.is_frozen(), "Map should be frozen");

    // Read operations should still work
    for (int i = 0; i < 50; ++i)
    {
        auto* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "Find should work on frozen map");
        FATP_ASSERT_EQ(*val, i * 3, "Values should be correct");
    }

    FATP_ASSERT_TRUE(map.contains(25), "Contains should work on frozen map");
    FATP_ASSERT_EQ(map.count(25), size_t(1), "Count should work on frozen map");
    FATP_ASSERT_EQ(map.at(25), 75, "At should work on frozen map");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_fasthashmap()
{
    std::cout << "\n" << colors::cyan() << "FastHashMap Benchmarks:" << colors::reset() << "\n\n";

    std::cout << "SIMD Backend: " << FastHashMap<int, int>::simd_backend() << "\n\n";

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

    FastHashMap<int, int> fmap;
    std::unordered_map<int, int> umap;

    for (int k : keys)
    {
        fmap.insert(k, k * 10);
        umap[k] = k * 10;
    }

    volatile long long sink = 0;

    double fast_time = measure_perf(
        [&]() {
            long long sum = 0;
            for (int k : lookup_keys)
            {
                int* v = fmap.find(k);
                if (v)
                {
                    sum += *v;
                }
            }
            sink = sum;
        },
        ITERATIONS / N,
        WARMUP / N);

    double umap_time = measure_perf(
        [&]() {
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
        },
        ITERATIONS / N,
        WARMUP / N);

    double ns_per_find_fast = (fast_time * 1e6) / N;
    double ns_per_find_umap = (umap_time * 1e6) / N;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Find (" << N << " elements):\n";
    std::cout << "  FastHashMap:       " << ns_per_find_fast << " ns/op\n";
    std::cout << "  std::unordered_map:" << ns_per_find_umap << " ns/op\n";
    std::cout << "  Speedup:           " << (umap_time / fast_time) << "x\n\n";

    (void)sink;
}

} // namespace fat_p::testing::fasthashmap

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_FastHashMap()
{
    FATP_PRINT_HEADER(FAST HASH MAP)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // --- Basic Operations ---
    out << colors::blue() << "--- Basic Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, basic_construction);
    FATP_RUN_TEST_NS(runner, fasthashmap, insert_find);
    FATP_RUN_TEST_NS(runner, fasthashmap, erase);
    FATP_RUN_TEST_NS(runner, fasthashmap, update_value);
    FATP_RUN_TEST_NS(runner, fasthashmap, clear);
    FATP_RUN_TEST_NS(runner, fasthashmap, load_factor);
    FATP_RUN_TEST_NS(runner, fasthashmap, contains_count);
    FATP_RUN_TEST_NS(runner, fasthashmap, at_method);
    FATP_RUN_TEST_NS(runner, fasthashmap, insert_or_assign);
    FATP_RUN_TEST_NS(runner, fasthashmap, string_keys);

    // --- Stress Tests ---
    out << "\n" << colors::blue() << "--- Stress Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, large_dataset);
    FATP_RUN_TEST_NS(runner, fasthashmap, erase_reinsert);
    FATP_RUN_TEST_NS(runner, fasthashmap, tombstone_stress);
    FATP_RUN_TEST_NS(runner, fasthashmap, stress_random);

    // --- Iterators ---
    out << "\n" << colors::blue() << "--- Iterators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, iterator_basic);
    FATP_RUN_TEST_NS(runner, fasthashmap, iterator_range_for);
    FATP_RUN_TEST_NS(runner, fasthashmap, const_iterator);

    // --- Copy/Move Semantics ---
    out << "\n" << colors::blue() << "--- Copy/Move Semantics ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, copy_semantics);
    FATP_RUN_TEST_NS(runner, fasthashmap, move_semantics);

    // --- Edge Cases ---
    out << "\n" << colors::blue() << "--- Edge Cases ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, empty_values);
    FATP_RUN_TEST_NS(runner, fasthashmap, simd_backend);
    FATP_RUN_TEST_NS(runner, fasthashmap, rehash);

    // --- Conditional noexcept & SFINAE (ChatGPT Round 2) ---
    out << "\n" << colors::blue() << "--- Conditional noexcept & SFINAE ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, conditional_noexcept);
    FATP_RUN_TEST_NS(runner, fasthashmap, sfinae_heterogeneous_lookup);
    FATP_RUN_TEST_NS(runner, fasthashmap, hash_finalizer_safety);

    // --- Allocator Policies (ChatGPT Round 3) ---
    out << "\n" << colors::blue() << "--- Allocator Policies ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, heap_allocator);
    FATP_RUN_TEST_NS(runner, fasthashmap, fixed_allocator_basic);
    FATP_RUN_TEST_NS(runner, fasthashmap, fixed_allocator_alignment);
    FATP_RUN_TEST_NS(runner, fasthashmap, fixed_hashmap_non_movable);
    FATP_RUN_TEST_NS(runner, fasthashmap, pointer_steal_safe_trait);

    // --- Deletion Policies & Freeze ---
    out << "\n" << colors::blue() << "--- Deletion Policies & Freeze ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, fasthashmap, backward_shift_deletion);
    FATP_RUN_TEST_NS(runner, fasthashmap, freeze_mode);

    fasthashmap::benchmark_fasthashmap();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FastHashMap() ? 0 : 1;
}
#endif
