/**
 * @file test_StableHashMap.cpp
 * @brief Comprehensive test suite for fat_p::StableHashMap
 * 
 * Tests all features including:
 * - Robin Hood hashing collision resolution
 * - Insert, find, erase operations
 * - Load factor management (0.75)
 * - Power-of-two sizing
 * - Backward shift deletion
 * - Heterogeneous lookup (transparent hash/equal)
 * - RAII correctness (from Gemini review)
 * - Sanitizer stress tests (from Gemini review)
 * - Performance vs std::unordered_map
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "StableHashMap.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_StableHashMap.h"
#endif

namespace fat_p::testing::stablehashmap
{

// Import StableHashMap from parent fat_p namespace
// (Using targeted declaration instead of namespace-scope 'using namespace')
using fat_p::StableHashMap;

// ============================================================================
// Test Constants
// ============================================================================

constexpr int LARGE_SIZE = 10000;


// ============================================================================
// RAII Tracking Type (from Gemini review)
// ============================================================================

struct TrackedRAII
{
    static inline int live_count = 0;
    static inline int ctor_count = 0;
    static inline int dtor_count = 0;

    int value;

    TrackedRAII() noexcept : value(0)
    {
        ++live_count;
        ++ctor_count;
    }

    explicit TrackedRAII(int v) noexcept : value(v)
    {
        ++live_count;
        ++ctor_count;
    }

    TrackedRAII(const TrackedRAII& other) noexcept : value(other.value)
    {
        ++live_count;
        ++ctor_count;
    }

    TrackedRAII(TrackedRAII&& other) noexcept : value(other.value)
    {
        other.value = -1;
        ++live_count;
        ++ctor_count;
    }

    TrackedRAII& operator=(const TrackedRAII& other) noexcept
    {
        value = other.value;
        return *this;
    }

    TrackedRAII& operator=(TrackedRAII&& other) noexcept
    {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~TrackedRAII()
    {
        --live_count;
        ++dtor_count;
    }

    static void reset()
    {
        live_count = 0;
        ctor_count = 0;
        dtor_count = 0;
    }
};

// ============================================================================
// Heap-owning payload for sanitizer tests (from Gemini review)
// ============================================================================

struct HeapBox
{
    int* p = nullptr;

    HeapBox() noexcept : p(nullptr) {}

    explicit HeapBox(int v) : p(new int(v)) {}

    HeapBox(const HeapBox& o) : p(o.p ? new int(*o.p) : nullptr) {}

    HeapBox(HeapBox&& o) noexcept : p(o.p)
    {
        o.p = nullptr;
    }

    HeapBox& operator=(const HeapBox& o)
    {
        if (this != &o)
        {
            delete p;
            p = o.p ? new int(*o.p) : nullptr;
        }
        return *this;
    }

    HeapBox& operator=(HeapBox&& o) noexcept
    {
        delete p;
        p = o.p;
        o.p = nullptr;
        return *this;
    }

    ~HeapBox()
    {
        delete p;
    }

    int value() const
    {
        return p ? *p : -1;
    }
};

// ============================================================================
// Zero hash for collision testing
// ============================================================================

struct ZeroHash
{
    size_t operator()(int) const noexcept
    {
        return 1;  // Force all keys to same bucket (not 0, since 0 = empty)
    }
};

// ============================================================================
// White-Box Tester for Churn Stability Tests
// ============================================================================
// This class provides internal inspection of StableHashMap to verify
// structural invariants (no tombstones, stable probe distances).
// Requires friend declaration in StableHashMap.h:
//   template <typename K, typename V, typename P>
//   friend class fat_p::testing::stablehashmap::StableHashMapTester;

template <typename MapType>
class StableHashMapTester
{
public:
    // Count physically occupied slots (hash != 0)
    static size_t count_occupied_slots(const MapType& map)
    {
        size_t count = 0;
        for (const auto& entry : map.buckets_)
        {
            if (entry.hash != 0) ++count;
        }
        return count;
    }

    // Check for ghost slots (tombstones): occupied_slots != size()
    static bool has_ghost_slots(const MapType& map)
    {
        return count_occupied_slots(map) != map.size();
    }

    // Compute average probe distance across all occupied slots
    static double average_probe_distance(const MapType& map)
    {
        size_t total_dist = 0;
        size_t count = 0;

        for (size_t i = 0; i < map.buckets_.size(); ++i)
        {
            if (map.buckets_[i].hash != 0)
            {
                size_t ideal = map.buckets_[i].hash & map.mask_;
                size_t dist = (i >= ideal) ? (i - ideal)
                                           : (i + map.buckets_.size() - ideal);
                total_dist += dist;
                ++count;
            }
        }
        return count > 0 ? static_cast<double>(total_dist) / count : 0.0;
    }

    // Get maximum probe distance
    static size_t max_probe_distance(const MapType& map)
    {
        size_t max_dist = 0;
        for (size_t i = 0; i < map.buckets_.size(); ++i)
        {
            if (map.buckets_[i].hash != 0)
            {
                size_t ideal = map.buckets_[i].hash & map.mask_;
                size_t dist = (i >= ideal) ? (i - ideal)
                                           : (i + map.buckets_.size() - ideal);
                if (dist > max_dist) max_dist = dist;
            }
        }
        return max_dist;
    }
};

// ============================================================================
// Test 1: Basic Construction
// ============================================================================

TEST_CASE(basic_construction)
{
    StableHashMap<int, std::string> map;

    ASSERT_TRUE(map.empty(), "New map should be empty");
    ASSERT_EQ(map.size(), size_t(0), "Size should be 0");

    return true;
}

// ============================================================================
// Test 2: Insert and Find
// ============================================================================

TEST_CASE(insert_find)
{
    StableHashMap<std::string, int> map;

    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);

    ASSERT_EQ(map.size(), size_t(3), "Size should be 3");

    int* val = map.find("one");
    ASSERT_TRUE(val != nullptr, "Should find 'one'");
    ASSERT_EQ(*val, 1, "Value should be 1");

    val = map.find("two");
    ASSERT_TRUE(val != nullptr, "Should find 'two'");
    ASSERT_EQ(*val, 2, "Value should be 2");

    val = map.find("nonexistent");
    ASSERT_TRUE(val == nullptr, "Should not find nonexistent key");

    return true;
}

// ============================================================================
// Test 3: Erase
// ============================================================================

TEST_CASE(erase)
{
    StableHashMap<int, std::string> map;

    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    ASSERT_EQ(map.size(), size_t(3), "Size should be 3");

    bool erased = map.erase(2);
    ASSERT_TRUE(erased, "Should successfully erase key 2");
    ASSERT_EQ(map.size(), size_t(2), "Size should be 2 after erase");

    auto* val = map.find(2);
    ASSERT_TRUE(val == nullptr, "Should not find erased key");

    erased = map.erase(999);
    ASSERT_FALSE(erased, "Should fail to erase nonexistent key");

    return true;
}

// ============================================================================
// Test 4: Update Existing Value
// ============================================================================

TEST_CASE(update_value)
{
    StableHashMap<std::string, int> map;

    map.insert("key", 10);

    int* val = map.find("key");
    ASSERT_TRUE(val != nullptr, "Should find key");
    ASSERT_EQ(*val, 10, "Initial value should be 10");

    *val = 20;

    val = map.find("key");
    ASSERT_EQ(*val, 20, "Updated value should be 20");

    return true;
}

// ============================================================================
// Test 5: Clear
// ============================================================================

TEST_CASE(clear)
{
    StableHashMap<int, std::string> map;

    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, "value");
    }

    ASSERT_EQ(map.size(), size_t(100), "Size should be 100");

    map.clear();
    ASSERT_EQ(map.size(), size_t(0), "Size should be 0 after clear");
    ASSERT_TRUE(map.empty(), "Map should be empty after clear");

    map.insert(1, "one");
    ASSERT_EQ(map.size(), size_t(1), "Should be able to insert after clear");

    return true;
}

// ============================================================================
// Test 6: Load Factor
// ============================================================================

TEST_CASE(load_factor)
{
    StableHashMap<int, int> map;

    for (int i = 0; i < 1000; ++i)
    {
        map.insert(i, i * 2);
    }

    float load = map.load_factor();
    ASSERT_TRUE(load >= 0.0f && load <= 1.0f, "Load factor should be between 0 and 1");
    ASSERT_TRUE(load <= 0.75f, "Load factor should not exceed 0.75");

    return true;
}

// ============================================================================
// Test 7: Collision Handling
// ============================================================================

TEST_CASE(collision_handling)
{
    StableHashMap<int, int> map;

    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, i * 10);
    }

    for (int i = 0; i < 100; ++i)
    {
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr, "Should find all keys despite collisions");
        ASSERT_EQ(*val, i * 10, "Values should be correct");
    }

    return true;
}

// ============================================================================
// Test 8: Large Dataset
// ============================================================================

TEST_CASE(large_dataset)
{
    StableHashMap<int, int> map;

    for (int i = 0; i < LARGE_SIZE; ++i)
    {
        map.insert(i, i * 2);
    }

    ASSERT_EQ(map.size(), size_t(LARGE_SIZE), "Size should match inserted count");

    int* val = map.find(LARGE_SIZE / 2);
    ASSERT_TRUE(val != nullptr, "Should find middle element");
    ASSERT_EQ(*val, (LARGE_SIZE / 2) * 2, "Value should be correct");

    return true;
}

// ============================================================================
// Test 9: String Keys
// ============================================================================

TEST_CASE(string_keys)
{
    StableHashMap<std::string, int> map;

    map.insert("apple", 1);
    map.insert("banana", 2);
    map.insert("cherry", 3);
    map.insert("date", 4);
    map.insert("elderberry", 5);

    ASSERT_EQ(map.size(), size_t(5), "Size should be 5");

    int* val = map.find("cherry");
    ASSERT_TRUE(val != nullptr, "Should find 'cherry'");
    ASSERT_EQ(*val, 3, "Value should be 3");

    return true;
}

// ============================================================================
// Test 10: Erase and Reinsert
// ============================================================================

TEST_CASE(erase_reinsert)
{
    StableHashMap<int, std::string> map;

    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    map.erase(2);
    ASSERT_TRUE(map.find(2) == nullptr, "Key 2 should not be found after erase");

    map.insert(2, "TWO");
    auto* val = map.find(2);
    ASSERT_TRUE(val != nullptr, "Key 2 should be found after reinsertion");
    ASSERT_TRUE(*val == "TWO", "Value should be updated");

    return true;
}

// ============================================================================
// Test 11: Empty Key/Value Edge Cases
// ============================================================================

TEST_CASE(empty_values)
{
    StableHashMap<std::string, std::string> map;

    map.insert("empty", "");
    map.insert("", "empty_key");

    auto* val1 = map.find("empty");
    ASSERT_TRUE(val1 != nullptr, "Should find key with empty value");
    ASSERT_TRUE(val1->empty(), "Value should be empty string");

    auto* val2 = map.find("");
    ASSERT_TRUE(val2 != nullptr, "Should find empty key");
    ASSERT_TRUE(*val2 == "empty_key", "Value for empty key should be correct");

    return true;
}

// ============================================================================
// Test 12: Const Correctness
// ============================================================================

TEST_CASE(const_correctness)
{
    StableHashMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");

    const StableHashMap<int, std::string>& cmap = map;

    const std::string* val = cmap.find(1);
    ASSERT_TRUE(val != nullptr, "Const find should work");
    ASSERT_TRUE(*val == "one", "Const value should be correct");

    return true;
}

// ============================================================================
// Test 13: Bracket Operator
// ============================================================================

TEST_CASE(bracket_operator)
{
    StableHashMap<std::string, int> map;

    map["key1"] = 100;
    map["key2"] = 200;

    ASSERT_EQ(map.size(), size_t(2), "Size should be 2");
    ASSERT_EQ(map["key1"], 100, "Should get correct value");

    map["key1"] = 150;
    ASSERT_EQ(map["key1"], 150, "Should update via bracket");

    return true;
}

// ============================================================================
// Test 14: Move Semantics
// ============================================================================

TEST_CASE(move_semantics)
{
    // Test move construction
    StableHashMap<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    StableHashMap<int, std::string> map2 = std::move(map1);

    ASSERT_EQ(map2.size(), size_t(2), "Moved-to map should have elements");
    ASSERT_TRUE(*map2.find(1) == "one", "Moved-to map should have values");

    // Test move assignment
    StableHashMap<int, std::string> map3;
    map3.insert(10, "ten");
    map3.insert(20, "twenty");
    map3.insert(30, "thirty");
    
    StableHashMap<int, std::string> map4;
    map4.insert(100, "hundred");
    
    map4 = std::move(map3);
    
    ASSERT_EQ(map4.size(), size_t(3), "Move-assigned map should have source elements");
    ASSERT_TRUE(*map4.find(10) == "ten", "Move-assigned map should have correct values");
    ASSERT_TRUE(*map4.find(20) == "twenty", "Move-assigned map should have correct values");
    ASSERT_TRUE(map4.find(100) == nullptr, "Move-assigned map should not have old elements");

    // Test that moved-from map can be safely reused
    // (insert triggers rehash which resets internal state)
    map3.insert(999, "reused");
    ASSERT_TRUE(map3.find(999) != nullptr, "Moved-from map should be reusable after insert");
    ASSERT_TRUE(*map3.find(999) == "reused", "Reused map should have correct value");

    return true;
}

// ============================================================================
// Test 15: Copy Semantics
// ============================================================================

TEST_CASE(copy_semantics)
{
    StableHashMap<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    StableHashMap<int, std::string> map2 = map1;

    ASSERT_EQ(map2.size(), size_t(2), "Copied map should have same size");
    ASSERT_TRUE(*map2.find(1) == "one", "Copied map should have same values");

    map1.insert(3, "three");
    ASSERT_EQ(map1.size(), size_t(3), "Original should be modified");
    ASSERT_EQ(map2.size(), size_t(2), "Copy should be independent");

    return true;
}

// ============================================================================
// Test 16: Backward Shift Deletion
// ============================================================================

TEST_CASE(backward_shift_deletion)
{
    StableHashMap<int, int, CustomHashPolicy<int, int, ZeroHash>> map(16, 0.9f);

    for (int i = 0; i < 10; ++i)
    {
        map.insert(i, i * 100);
    }

    map.erase(3);
    map.erase(5);
    map.erase(7);

    for (int i = 0; i < 10; ++i)
    {
        int* val = map.find(i);
        if (i == 3 || i == 5 || i == 7)
        {
            ASSERT_TRUE(val == nullptr, "Erased key should not be found");
        }
        else
        {
            ASSERT_TRUE(val != nullptr, "Non-erased key should be found");
            ASSERT_EQ(*val, i * 100, "Value should be correct");
        }
    }

    return true;
}

// ============================================================================
// Test 17: Equality Operators
// ============================================================================

TEST_CASE(equality_operators)
{
    // Empty maps are equal
    StableHashMap<int, int> empty1;
    StableHashMap<int, int> empty2;
    ASSERT_TRUE(empty1 == empty2, "Empty maps should be equal");
    ASSERT_FALSE(empty1 != empty2, "Empty maps should not be unequal");

    // Map equals itself
    StableHashMap<int, int> map1;
    map1.insert(1, 100);
    map1.insert(2, 200);
    map1.insert(3, 300);
    ASSERT_TRUE(map1 == map1, "Map should equal itself");

    // Maps with same content are equal
    StableHashMap<int, int> map2;
    map2.insert(1, 100);
    map2.insert(2, 200);
    map2.insert(3, 300);
    ASSERT_TRUE(map1 == map2, "Maps with same content should be equal");
    ASSERT_FALSE(map1 != map2, "Maps with same content should not be unequal");

    // Order of insertion doesn't matter
    StableHashMap<int, int> map3;
    map3.insert(3, 300);
    map3.insert(1, 100);
    map3.insert(2, 200);
    ASSERT_TRUE(map1 == map3, "Maps should be equal regardless of insertion order");

    // Different sizes are unequal
    StableHashMap<int, int> map4;
    map4.insert(1, 100);
    map4.insert(2, 200);
    ASSERT_FALSE(map1 == map4, "Maps with different sizes should not be equal");
    ASSERT_TRUE(map1 != map4, "Maps with different sizes should be unequal");

    // Same keys, different values are unequal
    StableHashMap<int, int> map5;
    map5.insert(1, 100);
    map5.insert(2, 200);
    map5.insert(3, 999);  // Different value
    ASSERT_FALSE(map1 == map5, "Maps with different values should not be equal");
    ASSERT_TRUE(map1 != map5, "Maps with different values should be unequal");

    // Different keys are unequal
    StableHashMap<int, int> map6;
    map6.insert(1, 100);
    map6.insert(2, 200);
    map6.insert(999, 300);  // Different key
    ASSERT_FALSE(map1 == map6, "Maps with different keys should not be equal");

    // Empty vs non-empty
    ASSERT_FALSE(empty1 == map1, "Empty map should not equal non-empty map");
    ASSERT_TRUE(empty1 != map1, "Empty map should be unequal to non-empty map");

    return true;
}

// ============================================================================
// Test 18: Stress Test Random Operations
// ============================================================================

TEST_CASE(stress_random)
{
    StableHashMap<int, int> map;
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
            reference.insert({key, i});  // Must use insert, not operator[] (which overwrites)
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
// Test 19: RAII Erase Correctness (from Gemini review)
// ============================================================================

TEST_CASE(raii_erase_correctness)
{
    TrackedRAII::reset();

    {
        StableHashMap<int, TrackedRAII> map;
        constexpr int N = 128;

        for (int i = 0; i < N; ++i)
        {
            map.insert(i, TrackedRAII{i});
        }

        map.erase(0);
        map.erase(N / 2);
        map.erase(N - 1);

        ASSERT_EQ(map.size(), size_t(N - 3), "Size should decrease by 3");
    }

    ASSERT_EQ(TrackedRAII::live_count, 0, "All RAII objects should be destroyed");
    ASSERT_EQ(TrackedRAII::ctor_count, TrackedRAII::dtor_count, 
              "Ctor/dtor count should match");

    return true;
}

// ============================================================================
// Test 20: HeapBox Sanitizer Stress (from Gemini review)
// ============================================================================

TEST_CASE(heapbox_stress)
{
    StableHashMap<int, HeapBox, CustomHashPolicy<int, HeapBox, ZeroHash>> map(8, 0.9f);

    for (int i = 0; i < 200; ++i)
    {
        map.insert(i, HeapBox{i * 11});
    }

    for (int i = 0; i < 200; i += 4)
    {
        map.erase(i);
    }

    for (int i = 1; i < 200; i += 4)
    {
        HeapBox* val = map.find(i);
        ASSERT_TRUE(val != nullptr, "Non-erased key should be found");
        ASSERT_EQ(val->value(), i * 11, "Value should be correct");
    }

    map.clear();
    ASSERT_TRUE(map.empty(), "Map should be empty after clear");

    return true;
}

// ============================================================================
// Test 21: Rehash Stress (from Gemini review)
// ============================================================================

TEST_CASE(rehash_stress)
{
    StableHashMap<int, HeapBox, CustomHashPolicy<int, HeapBox, ZeroHash>> map(4, 0.5f);

    for (int round = 0; round < 5; ++round)
    {
        for (int i = 0; i < 100; ++i)
        {
            map.insert(i, HeapBox{round * 1000 + i});
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
// Test 22: Heavy Collision Chain
// ============================================================================

TEST_CASE(heavy_collision_chain)
{
    StableHashMap<int, int, CustomHashPolicy<int, int, ZeroHash>> map(16, 0.9f);

    for (int i = 0; i < 12; ++i)
    {
        map.insert(i, i * 100);
    }

    for (int i = 0; i < 12; ++i)
    {
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr, "Should find all keys in collision chain");
        ASSERT_EQ(*val, i * 100, "Value should be correct");
    }

    for (int i = 5; i >= 0; --i)
    {
        map.erase(i);
    }

    for (int i = 6; i < 12; ++i)
    {
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr, "Should find remaining keys");
        ASSERT_EQ(*val, i * 100, "Value should be correct");
    }

    return true;
}

// ============================================================================
// Test 23: Insert Duplicate Key
// ============================================================================

TEST_CASE(insert_duplicate_key)
{
    StableHashMap<int, std::string> map;

    // Test insert() - does NOT overwrite (matches std::unordered_map)
    bool inserted1 = map.insert(1, "first");
    bool inserted2 = map.insert(1, "second");  // Should fail, key exists

    ASSERT_EQ(map.size(), size_t(1), "Size should be 1 (no duplicate)");
    ASSERT_TRUE(inserted1, "First insert should succeed");
    ASSERT_TRUE(!inserted2, "Second insert should return false (key exists)");

    auto* val = map.find(1);
    ASSERT_TRUE(val != nullptr, "Should find key");
    ASSERT_TRUE(*val == "first", "Value should remain 'first' (insert doesn't overwrite)");

    // Test insert_or_assign() - DOES overwrite
    auto [ptr, was_inserted] = map.insert_or_assign(1, "third");
    ASSERT_TRUE(!was_inserted, "insert_or_assign should return false (updated existing)");
    ASSERT_TRUE(*ptr == "third", "Value should be updated to 'third'");
    
    val = map.find(1);
    ASSERT_TRUE(*val == "third", "Value should be 'third' after insert_or_assign");

    return true;
}

// ============================================================================
// Test 24: Move-Only Values
// ============================================================================

TEST_CASE(move_only_values)
{
    StableHashMap<int, std::unique_ptr<int>> map;

    auto p1 = std::make_unique<int>(100);
    auto p2 = std::make_unique<int>(200);

    map.insert(1, std::move(p1));
    map.insert(2, std::move(p2));

    auto* val1 = map.find(1);
    ASSERT_TRUE(val1 != nullptr, "Should find key 1");
    ASSERT_EQ(**val1, 100, "Value should be 100");

    map.erase(1);
    ASSERT_TRUE(map.find(1) == nullptr, "Erased key should not be found");

    auto* val2 = map.find(2);
    ASSERT_TRUE(val2 != nullptr, "Should find key 2");
    ASSERT_EQ(**val2, 200, "Value should be 200");

    return true;
}

TEST_CASE(freeze_and_fluent_api)
{
    // Test freeze() alias
    StableHashMap<int, int> map1;
    map1.insert(1, 100);
    map1.freeze();
    
    ASSERT_TRUE(map1.is_read_only(), "Should be read-only after freeze()");
    ASSERT_TRUE(map1.mutation_policy() == MutationPolicy::ReadOnly, 
                "Policy should be ReadOnly");
    
    // Verify find still works
    int* val = map1.find(1);
    ASSERT_TRUE(val != nullptr, "find() should work in read-only mode");
    ASSERT_EQ(*val, 100, "Value should be 100");
    
    // Test fluent chaining - make_read_only() returns reference
    StableHashMap<int, int> map2;
    map2.insert(1, 100);
    map2.insert(2, 200);
    
    StableHashMap<int, int>& ref = map2.make_read_only();
    ASSERT_TRUE(&ref == &map2, "make_read_only() should return *this");
    ASSERT_TRUE(map2.is_read_only(), "Should be read-only");
    
    // Test freeze() also returns reference
    StableHashMap<int, int> map3;
    map3.insert(3, 300);
    StableHashMap<int, int>& ref3 = map3.freeze();
    ASSERT_TRUE(&ref3 == &map3, "freeze() should return *this");
    ASSERT_TRUE(map3.is_read_only(), "Should be read-only after freeze()");

    return true;
}

// ============================================================================
// Heterogeneous Lookup Tests
// ============================================================================

// Transparent hash for std::string that accepts const char* and string_view
// Uses string_view consistently to ensure identical hashes for identical content
struct TransparentStringHash
{
    using is_transparent = void;
    
    size_t operator()(std::string_view sv) const noexcept
    {
        return std::hash<std::string_view>{}(sv);
    }
    
    size_t operator()(const std::string& s) const noexcept
    {
        return operator()(std::string_view(s));
    }
    
    size_t operator()(const char* s) const noexcept
    {
        return operator()(std::string_view(s));
    }
};

// Transparent equality for std::string
// Uses string_view comparison for all combinations to ensure consistency
struct TransparentStringEqual
{
    using is_transparent = void;
    
    // All comparisons go through string_view for consistency
    bool operator()(std::string_view a, std::string_view b) const noexcept
    {
        return a == b;
    }
    
    bool operator()(const std::string& a, const std::string& b) const noexcept
    {
        return a == b;
    }
    
    bool operator()(const std::string& a, std::string_view b) const noexcept
    {
        return std::string_view(a) == b;
    }
    
    bool operator()(std::string_view a, const std::string& b) const noexcept
    {
        return a == std::string_view(b);
    }
    
    bool operator()(const std::string& a, const char* b) const noexcept
    {
        return a == b;
    }
    
    bool operator()(const char* a, const std::string& b) const noexcept
    {
        return a == b;
    }
    
    bool operator()(const char* a, const char* b) const noexcept
    {
        return std::string_view(a) == std::string_view(b);
    }
    
    bool operator()(const char* a, std::string_view b) const noexcept
    {
        return std::string_view(a) == b;
    }
    
    bool operator()(std::string_view a, const char* b) const noexcept
    {
        return a == std::string_view(b);
    }
};

// ============================================================================
// SplitMix64 Hash for Hash Quality Benchmarks
// ============================================================================
// 
// SplitMix64 is a well-known, public-domain mixing function used in:
// - Java's SplittableRandom
// - Research papers on hash quality
// - Competitive programming and HPC
// 
// It provides excellent avalanche properties without SIMD or complexity.
// NOT borrowed from any hash map library.
// ============================================================================

struct SplitMix64Hash
{
    size_t operator()(uint64_t x) const noexcept
    {
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }
    
    size_t operator()(int64_t x) const noexcept
    {
        return operator()(static_cast<uint64_t>(x));
    }
    
    size_t operator()(int x) const noexcept
    {
        return operator()(static_cast<uint64_t>(static_cast<uint32_t>(x)));
    }
};

TEST_CASE(heterogeneous_lookup)
{
    // Test with transparent hash/equal using CustomHashPolicy
    using TransparentPolicy = CustomHashPolicy<std::string, int, 
                                               TransparentStringHash,
                                               TransparentStringEqual>;
    using TransparentMap = StableHashMap<std::string, int, TransparentPolicy>;
    
    TransparentMap map;
    map.insert("hello", 1);
    map.insert("world", 2);
    map.insert("test", 3);
    
    // find() with const char*
    int* val = map.find("hello");
    ASSERT_TRUE(val != nullptr, "find(const char*) should work");
    ASSERT_EQ(*val, 1, "Value should be 1");
    
    val = map.find("missing");
    ASSERT_TRUE(val == nullptr, "find() should return nullptr for missing key");
    
    // find() with string_view
    std::string_view sv = "world";
    val = map.find(sv);
    ASSERT_TRUE(val != nullptr, "find(string_view) should work");
    ASSERT_EQ(*val, 2, "Value should be 2");
    
    // contains() with const char*
    ASSERT_TRUE(map.contains("hello"), "contains(const char*) should work");
    ASSERT_TRUE(!map.contains("missing"), "contains() should return false for missing");
    
    // contains() with string_view
    ASSERT_TRUE(map.contains(sv), "contains(string_view) should work");
    
    // erase() with const char*
    ASSERT_TRUE(map.erase("test"), "erase(const char*) should work");
    ASSERT_TRUE(!map.contains("test"), "Key should be erased");
    ASSERT_EQ(map.size(), 2u, "Size should be 2 after erase");
    
    // erase() with string_view
    std::string_view sv_hello = "hello";
    ASSERT_TRUE(map.erase(sv_hello), "erase(string_view) should work");
    ASSERT_TRUE(!map.contains("hello"), "Key should be erased");
    
    // const correctness
    const TransparentMap& cmap = map;
    const int* cval = cmap.find("world");
    ASSERT_TRUE(cval != nullptr, "const find() should work");
    ASSERT_EQ(*cval, 2, "Value should be 2");
    
    return true;
}

TEST_CASE(heterogeneous_lookup_non_transparent)
{
    // Without transparent hash/equal, heterogeneous lookup is disabled
    StableHashMap<std::string, int> map;
    map.insert(std::string("hello"), 1);
    map.insert(std::string("world"), 2);
    
    // Must use std::string for lookup (const char* would create temporary)
    int* val = map.find(std::string("hello"));
    ASSERT_TRUE(val != nullptr, "Regular find should work");
    ASSERT_EQ(*val, 1, "Value should be 1");
    
    ASSERT_TRUE(map.contains(std::string("world")), "Regular contains should work");
    ASSERT_TRUE(map.erase(std::string("hello")), "Regular erase should work");
    
    return true;
}

TEST_CASE(heterogeneous_try_emplace_rvalue)
{
    // Test heterogeneous try_emplace with rvalue keys
    // This tests the fix for the use-after-move bug
    using TransparentPolicy = CustomHashPolicy<std::string, int, 
                                               TransparentStringHash,
                                               TransparentStringEqual>;
    using TransparentMap = StableHashMap<std::string, int, TransparentPolicy>;
    
    TransparentMap map;
    
    // Test with rvalue string - this would trigger the use-after-move bug
    // if not properly fixed
    auto make_key = []() { return std::string("rvalue_key"); };
    auto [ptr1, inserted1] = map.try_emplace(make_key(), 42);
    
    ASSERT_TRUE(inserted1, "Should insert new key");
    ASSERT_TRUE(ptr1 != nullptr, "Should return valid pointer");
    ASSERT_EQ(*ptr1, 42, "Value should be 42");
    ASSERT_TRUE(map.contains("rvalue_key"), "Key should exist in map");
    
    // Try again with same key (should not insert)
    auto [ptr2, inserted2] = map.try_emplace(make_key(), 100);
    ASSERT_FALSE(inserted2, "Should not insert duplicate");
    ASSERT_EQ(*ptr2, 42, "Value should still be 42");
    
    // Test with longer rvalue string to ensure no SSO interference
    auto make_long_key = []() { 
        return std::string("this_is_a_very_long_key_that_exceeds_sso_buffer_size_on_most_implementations"); 
    };
    auto [ptr3, inserted3] = map.try_emplace(make_long_key(), 999);
    
    ASSERT_TRUE(inserted3, "Should insert long key");
    ASSERT_TRUE(ptr3 != nullptr, "Should return valid pointer for long key");
    ASSERT_EQ(*ptr3, 999, "Value should be 999");
    ASSERT_TRUE(map.contains("this_is_a_very_long_key_that_exceeds_sso_buffer_size_on_most_implementations"), 
                "Long key should exist");
    
    return true;
}

// ============================================================================
// Test: SafePolicy erase behavior
// Documents the BASIC exception guarantee behavior with SafePolicy. When
// Value default assignment could throw during erase, the slot is marked empty
// (hash=0) BEFORE the assignment attempt. This ensures map invariants are
// preserved even if the assignment throws. The unreset values are inert
// until map destruction.
// ============================================================================

TEST_CASE(safepolicy_erase_basic_guarantee)
{
    // This test documents the BASIC exception guarantee for erase() with SafePolicy:
    // 
    // erase_slot() sequence:
    //   1. e.hash = 0;           // Mark empty FIRST (invariant preserved)
    //   2. --num_elements_;      // Update count
    //   3. e.key = Key{};        // Best-effort cleanup (may throw with SafePolicy)
    //   4. e.value = Value{};    // Best-effort cleanup (may throw with SafePolicy)
    //
    // If step 3 or 4 throws, the slot is already logically empty (hash==0),
    // so lookups will skip it. The old key/value remain in memory but are
    // inaccessible. They will be destroyed when the map is destroyed or the
    // slot is reused.
    
    using SafeMap = fat_p::StableHashMap<int, int, fat_p::SafePolicy<int, int>>;
    
    SafeMap map;
    
    // Insert several entries
    for (int i = 0; i < 10; ++i)
    {
        map.insert(i, i * 100);
    }
    
    ASSERT_EQ(map.size(), 10u, "Should have 10 entries");
    
    // Erase entries - exercises SafePolicy code path
    bool erased = map.erase(5);
    ASSERT_TRUE(erased, "Should erase key 5");
    ASSERT_EQ(map.size(), 9u, "Size should be 9 after erase");
    
    // Verify the erased key is not findable
    ASSERT_TRUE(map.find(5) == nullptr, "Erased key should not be found");
    
    // Verify other keys are still accessible
    for (int i = 0; i < 10; ++i)
    {
        if (i == 5) continue;
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr, "Non-erased key should be found");
        ASSERT_EQ(*val, i * 100, "Value should be correct");
    }
    
    // Verify we can reinsert into the erased slot
    map.insert(5, 555);
    ASSERT_EQ(map.size(), 10u, "Size should be 10 after reinsert");
    int* reinserted = map.find(5);
    ASSERT_TRUE(reinserted != nullptr, "Reinserted key should be found");
    ASSERT_EQ(*reinserted, 555, "Reinserted value should be correct");
    
    // Test clear() with SafePolicy as well
    map.clear();
    ASSERT_TRUE(map.empty(), "Map should be empty after clear");
    ASSERT_EQ(map.size(), 0u, "Size should be 0 after clear");
    
    // Verify map is still usable after clear
    map.insert(42, 4200);
    ASSERT_EQ(map.size(), 1u, "Should have 1 entry after reinsert");
    ASSERT_TRUE(map.find(42) != nullptr, "Should find reinserted key");
    
    return true;
}

// ============================================================================
// Churn Stability Test 1: Probe Distance Does Not Degrade
// ============================================================================
// Objective: Prove that after 50x churn (insert/erase), the internal
// probe distance structure is statistically identical to a fresh map.
// In tombstone-based maps, aged_avg would be 5x-10x higher.

TEST_CASE(churn_probe_distance_stability)
{
    using Map = StableHashMap<int, int>;
    using Tester = StableHashMapTester<Map>;

    const int TARGET_SIZE = 10000;
    const int CHURN_OPS = 500000;  // 50x the size

    Map map;
    std::mt19937 rng(42);  // Fixed seed for reproducibility

    // 1. Fill map and establish baseline
    std::vector<int> keys;
    keys.reserve(TARGET_SIZE);
    for (int i = 0; i < TARGET_SIZE; ++i)
    {
        int k = static_cast<int>(rng());
        if (map.try_emplace(k, k).second)
        {
            keys.push_back(k);
        }
        else
        {
            --i;  // Retry on collision
        }
    }

    double baseline_avg = Tester::average_probe_distance(map);
    size_t baseline_max = Tester::max_probe_distance(map);

    // 2. Perform sustained churn (maintain constant size)
    // Use random keys (not sequential) to avoid clustering artifacts
    std::uniform_int_distribution<int> key_dist(0, std::numeric_limits<int>::max());
    for (int i = 0; i < CHURN_OPS; ++i)
    {
        // Erase random existing key
        std::uniform_int_distribution<size_t> idx_dist(0, keys.size() - 1);
        size_t idx = idx_dist(rng);
        int key_to_erase = keys[idx];

        map.erase(key_to_erase);

        // Swap-remove from tracking vector
        keys[idx] = keys.back();
        keys.pop_back();

        // Insert new RANDOM key (not sequential)
        int new_key;
        do {
            new_key = key_dist(rng);
        } while (map.find(new_key) != nullptr);  // Ensure unique
        
        map.insert(new_key, new_key);
        keys.push_back(new_key);
    }

    // 3. Measure aged state
    double aged_avg = Tester::average_probe_distance(map);
    size_t aged_max = Tester::max_probe_distance(map);

    // Diagnostic output
    std::cout << "  Probe distance: baseline_avg=" << baseline_avg 
              << " aged_avg=" << aged_avg
              << " baseline_max=" << baseline_max
              << " aged_max=" << aged_max << "\n";

    // 4. Assertions - probe distances should remain stable
    // In tombstone maps, aged_avg typically grows 3-10x after heavy churn
    // Allow up to 1.0 delta for variance in RNG-based key distribution
    double avg_delta = std::abs(aged_avg - baseline_avg);
    ASSERT_TRUE(avg_delta < 1.0,
        "Average probe distance degraded significantly");

    // Max probe can vary more due to key distribution luck
    ASSERT_TRUE(aged_max < baseline_max + 15,
        "Maximum probe distance grew excessively");

    return true;
}

// ============================================================================
// Churn Stability Test 2: No Ghost Slots (Tombstones) Accumulate
// ============================================================================
// Objective: Verify that physically occupied slots == logical size.
// If tombstones existed, occupied_slots would be > size().
// This is the definitive structural proof of "no tombstones."

TEST_CASE(churn_no_ghost_slots)
{
    using Map = StableHashMap<int, int>;
    using Tester = StableHashMapTester<Map>;

    const int TARGET_SIZE = 5000;
    const int CHURN_OPS = 100000;  // 20x the size

    Map map;
    std::mt19937 rng(42);

    // Fill map with tracked keys
    std::vector<int> keys;
    keys.reserve(TARGET_SIZE);
    for (int i = 0; i < TARGET_SIZE; ++i)
    {
        int k = static_cast<int>(rng());
        if (map.try_emplace(k, k).second)
        {
            keys.push_back(k);
        }
        else
        {
            --i;
        }
    }

    // Perform churn with REAL erases (not random misses)
    // This is the reviewer-recommended improvement: track actual keys
    // Use random keys (not sequential) to avoid clustering artifacts
    std::uniform_int_distribution<int> key_dist(0, std::numeric_limits<int>::max());
    for (int i = 0; i < CHURN_OPS; ++i)
    {
        // Erase actual existing key (triggers backward-shift)
        std::uniform_int_distribution<size_t> idx_dist(0, keys.size() - 1);
        size_t idx = idx_dist(rng);
        int key_to_erase = keys[idx];

        bool erased = map.erase(key_to_erase);
        ASSERT_TRUE(erased, "Should successfully erase tracked key");

        keys[idx] = keys.back();
        keys.pop_back();

        // Insert new RANDOM key
        int new_key;
        do {
            new_key = key_dist(rng);
        } while (map.find(new_key) != nullptr);
        
        map.insert(new_key, new_key);
        keys.push_back(new_key);
    }

    // Verify: no ghost slots
    bool has_ghosts = Tester::has_ghost_slots(map);
    ASSERT_FALSE(has_ghosts, "Map contains ghost slots (tombstones) after churn");

    // Double-check: count via iteration matches size()
    size_t manual_count = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        ++manual_count;
    }
    ASSERT_EQ(manual_count, map.size(),
        "Iteration count should match size()");

    // Triple-check: occupied slots == size()
    size_t occupied = Tester::count_occupied_slots(map);
    ASSERT_EQ(occupied, map.size(),
        "Occupied slots should equal size()");

    return true;
}

// ============================================================================
// Churn Stability Test 3: Latency Does Not Degrade
// ============================================================================
// Objective: Wall-clock performance benchmark. Aged lookup speed should
// be within margin of error of fresh lookup speed.
// In tombstone-based maps, this ratio is typically 1.5-3.0x (degraded).

TEST_CASE(churn_latency_stability)
{
    using Map = StableHashMap<int, int>;

    const int TARGET_SIZE = 50000;
    const int LOOKUPS = 1000000;
    const int CHURN_OPS = 200000;

    Map map;
    std::mt19937 rng(42);

    // 1. Fill map with tracked keys
    std::vector<int> keys;
    keys.reserve(TARGET_SIZE);
    for (int i = 0; i < TARGET_SIZE; ++i)
    {
        int k = static_cast<int>(rng());
        if (map.try_emplace(k, k).second)
        {
            keys.push_back(k);
        }
        else
        {
            --i;
        }
    }

    // Generate lookup keys (mix of hits and misses)
    std::vector<int> lookup_keys;
    lookup_keys.reserve(LOOKUPS);
    std::uniform_int_distribution<int> lookup_dist(0, TARGET_SIZE * 20);
    for (int i = 0; i < LOOKUPS; ++i)
    {
        lookup_keys.push_back(lookup_dist(rng));
    }

    // 2. Measure fresh lookup performance
    volatile int sink = 0;
    auto start_fresh = std::chrono::high_resolution_clock::now();
    for (int k : lookup_keys)
    {
        if (auto* v = map.find(k)) sink += *v;
    }
    auto end_fresh = std::chrono::high_resolution_clock::now();
    auto fresh_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_fresh - start_fresh).count();

    // 3. Perform churn (using tracked keys for real erases)
    // Use random keys (not sequential) to avoid clustering artifacts
    std::uniform_int_distribution<int> key_dist(0, std::numeric_limits<int>::max());
    for (int i = 0; i < CHURN_OPS; ++i)
    {
        std::uniform_int_distribution<size_t> idx_dist(0, keys.size() - 1);
        size_t idx = idx_dist(rng);

        map.erase(keys[idx]);
        keys[idx] = keys.back();
        keys.pop_back();

        // Insert new RANDOM key
        int new_key;
        do {
            new_key = key_dist(rng);
        } while (map.find(new_key) != nullptr);
        
        map.insert(new_key, new_key);
        keys.push_back(new_key);
    }

    // 4. Measure aged lookup performance
    auto start_aged = std::chrono::high_resolution_clock::now();
    for (int k : lookup_keys)
    {
        if (auto* v = map.find(k)) sink += *v;
    }
    auto end_aged = std::chrono::high_resolution_clock::now();
    auto aged_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_aged - start_aged).count();

    // 5. Assert latency stability
    // Allow 25% tolerance for system noise (scheduler, cache effects, CI variability)
    // In tombstone-based maps, ratio is typically 1.5-3.0x
    double ratio = static_cast<double>(aged_us) / fresh_us;

    // Diagnostic output
    std::cout << "  Latency: fresh=" << fresh_us << "us aged=" << aged_us 
              << "us ratio=" << ratio << "\n";

    // Use sink to prevent optimization
    (void)sink;

    ASSERT_TRUE(ratio < 1.25,
        "Lookup performance degraded after churn");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================
// ==========================================================
// Benchmark infrastructure (shared, Windows-safe)
// ==========================================================

static constexpr int    BENCH_SAMPLES = 5;
static constexpr int    BENCH_WARMUP = 2;
static constexpr size_t BENCH_FIND_ITERS = 1'000'000;
static constexpr size_t BENCH_OP_ITERS = 200'000;

// MSVC requires a hard observable side effect
static volatile std::uintptr_t benchmark_sink = 0;

// ==========================================================
// Helpers
// ==========================================================

inline size_t bucket_count_for(size_t n)
{
    size_t cap = 1;
    while (cap < n * 2) cap <<= 1;
    return cap;
}

// ==========================================================
// Benchmark helper
// ==========================================================
template <size_t ITERS, typename ResetFn, typename Fn>
double bench_ns_per_op(ResetFn&& reset_fn, Fn&& fn)
{
    std::vector<double> samples;
    samples.reserve(BENCH_SAMPLES);

    reset_fn();
    (void)measure_perf(fn, ITERS, BENCH_WARMUP);

    for (int i = 0; i < BENCH_SAMPLES; ++i)
    {
        reset_fn();
        double ms_per_call = measure_perf(fn, ITERS, 0); // <-- ms per call
        samples.push_back(ms_per_call * 1e6);            // <-- ns per call
    }

    std::sort(samples.begin(), samples.end());
    double med = samples[samples.size() / 2];

    if (med <= 0.0) {
        std::cerr
            << "[bench] WARNING: invalid measurement ("
            << med << " ns/op). Skipping sample.\n";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return med;
}


template <size_t ITERS, typename Fn>
double bench_ns_per_op(Fn&& fn)
{
    return bench_ns_per_op<ITERS>([]() {}, std::forward<Fn>(fn));
}

// ==========================================================
// FIND (steady-state, marginal)
// ==========================================================

void benchmark_find(size_t N)
{
    std::mt19937 rng(123456);

    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), rng);

    fat_p::StableHashMap<int, int> fmap(N * 2, 0.99f);
    std::unordered_map<int, int> umap;
    umap.reserve(N * 2);

    for (int k : keys) {
        fmap.insert(k, k);
        umap.emplace(k, k);
    }

    size_t idx = 0;

    auto next_key = [&]() {
        int k = keys[idx++];
        if (idx == keys.size()) idx = 0;
        return k;
        };

    double fmap_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
        int* v = fmap.find(next_key());
        benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
        });

    idx = 0;

    double umap_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
        auto it = umap.find(next_key());
        benchmark_sink += reinterpret_cast<std::uintptr_t>(
            it == umap.end() ? nullptr : &it->second);
        });

    std::cout << std::setw(8) << N
        << " | FIND   | StableHashMap " << fmap_ns
        << " ns | unordered_map " << umap_ns
        << " ns | Speedup " << (umap_ns / fmap_ns) << "x\n";
}

// ==========================================================
// INSERT (marginal, no rehash noise)
// ==========================================================

void benchmark_insert(size_t N)
{
    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);

    fat_p::StableHashMap<int, int> fmap(N * 2, 0.99f);
    std::unordered_map<int, int> umap;
    umap.reserve(N * 2);

    auto reset = [&]() {
        fmap.clear();
        umap.clear();
        umap.reserve(N * 2);
        };

    size_t idx = 0;

    double fmap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        int k = keys[idx++ % keys.size()];
        fmap.insert(k, k);
        benchmark_sink += k;
        });

    idx = 0;

    double umap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        int k = keys[idx++ % keys.size()];
        umap.emplace(k, k);
        benchmark_sink += k;
        });

    std::cout << std::setw(8) << N
        << " | INSERT | StableHashMap " << fmap_ns
        << " ns | unordered_map " << umap_ns
        << " ns | Speedup " << (umap_ns / fmap_ns) << "x\n";
}

// ==========================================================
// ERASE (marginal)
// ==========================================================

void benchmark_erase(size_t N)
{
    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);

    fat_p::StableHashMap<int, int> fmap(N * 2, 0.99f);
    std::unordered_map<int, int> umap;
    umap.reserve(N * 2);

    auto reset = [&]() {
        fmap.clear();
        umap.clear();
        umap.reserve(N * 2);
        for (int k : keys) {
            fmap.insert(k, k);
            umap.emplace(k, k);
        }
        };

    size_t idx = 0;

    double fmap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        bool ok = fmap.erase(keys[idx++ % keys.size()]);
        benchmark_sink += static_cast<std::size_t>(ok);
        });

    idx = 0;

    double umap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        std::size_t erased = umap.erase(keys[idx++ % keys.size()]);
        benchmark_sink += erased;
        });

    std::cout << std::setw(8) << N
        << " | ERASE  | StableHashMap " << fmap_ns
        << " ns | unordered_map " << umap_ns
        << " ns | Speedup " << (umap_ns / fmap_ns) << "x\n";
}

// ==========================================================
// BUILD FROM EMPTY (amortized)
// ==========================================================

void benchmark_build_from_empty(size_t N)
{
    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);

    constexpr size_t BUILD_ITERS = 2000;

    auto measure = [&](auto&& build_fn) {
        std::vector<double> s;
        s.reserve(BENCH_SAMPLES);

        (void)measure_perf(build_fn, BUILD_ITERS, BENCH_WARMUP);

        for (int i = 0; i < BENCH_SAMPLES; ++i) {
            double us = measure_perf(build_fn, BUILD_ITERS, 0);
            s.push_back(us);
        }

        std::sort(s.begin(), s.end());
        return s[s.size() / 2];
        };

    double fmap_us = measure([&]() {
        fat_p::StableHashMap<int, int> map;
        for (int k : keys) map.insert(k, k);
        benchmark_sink += map.size();
        });

    double umap_us = measure([&]() {
        std::unordered_map<int, int> map;
        for (int k : keys) map.emplace(k, k);
        benchmark_sink += map.size();
        });

    double fmap_ns_per_elem = (fmap_us * 1e6) / double(N); // ms -> ns
    double umap_ns_per_elem = (umap_us * 1e6) / double(N);

    std::cout << std::setw(8) << N
        << " | BUILD  | StableHashMap " << fmap_ns_per_elem
        << " ns/elem | unordered_map " << umap_ns_per_elem
        << " ns/elem | Speedup " << (umap_ns_per_elem / fmap_ns_per_elem) << "x\n";
}

// ==========================================================
// REHASH-INCLUSIVE INSERT
// ==========================================================

void benchmark_rehash_inclusive(size_t N)
{
    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);

    fat_p::StableHashMap<int, int> fmap;
    std::unordered_map<int, int> umap;

    auto reset = [&]() {
        fmap.clear();
        umap.clear();
        };

    size_t idx = 0;

    double fmap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        int k = keys[idx++ % keys.size()];
        fmap.insert(k, k);
        if (fmap.size() >= keys.size()) fmap.clear();
        benchmark_sink += fmap.size();
        });

    idx = 0;

    double umap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        int k = keys[idx++ % keys.size()];
        umap.emplace(k, k);
        if (umap.size() >= keys.size()) umap.clear();
        benchmark_sink += umap.size();
        });

    std::cout << std::setw(8) << N
        << " | REHASH | StableHashMap " << fmap_ns
        << " ns/op | unordered_map " << umap_ns
        << " ns/op | Speedup " << (umap_ns / fmap_ns) << "x\n";
}

// ==========================================================
// MIXED WORKLOAD
// ==========================================================

void benchmark_mixed(size_t N)
{
    std::mt19937 rng(24680);

    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), rng);

    const size_t buckets = bucket_count_for(N);

    fat_p::StableHashMap<int, int> fmap(buckets, 0.75f);
    std::unordered_map<int, int> umap;
    umap.reserve(buckets);

    for (int k : keys) {
        fmap.insert(k, k);
        umap.emplace(k, k);
    }

    std::uniform_int_distribution<int> op_dist(0, 3);

    auto reset = [&]() {
        fmap.clear();
        umap.clear();
        umap.reserve(buckets);
        for (int k : keys) {
            fmap.insert(k, k);
            umap.emplace(k, k);
        }
        };

    size_t idx = 0;

    double fmap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        int k = keys[idx++ % keys.size()];
        int op = op_dist(rng);

        if (op <= 1) {
            benchmark_sink += reinterpret_cast<std::uintptr_t>(fmap.find(k));
        }
        else if (op == 2) {
            fmap.insert(k + 1'000'000, k);
            benchmark_sink += k;
        }
        else {
            benchmark_sink += fmap.erase(k);
        }
        });

    idx = 0;

    double umap_ns = bench_ns_per_op<BENCH_OP_ITERS>(reset, [&]() {
        int k = keys[idx++ % keys.size()];
        int op = op_dist(rng);

        if (op <= 1) {
            auto it = umap.find(k);
            benchmark_sink += reinterpret_cast<std::uintptr_t>(
                it == umap.end() ? nullptr : &it->second);
        }
        else if (op == 2) {
            umap.emplace(k + 1'000'000, k);
            benchmark_sink += k;
        }
        else {
            benchmark_sink += umap.erase(k);
        }
        });

    std::cout << std::setw(8) << N
        << " | MIXED  | StableHashMap " << fmap_ns
        << " ns/op | unordered_map " << umap_ns
        << " ns/op | Speedup " << (umap_ns / fmap_ns) << "x\n";
}

// ==========================================================
// Hash Quality Impact Benchmark
// ==========================================================
// Tests whether a "better" hash (SplitMix64) improves performance
// compared to std::hash. Answers: "Is StableHashMap hash-limited?"

void benchmark_hash_quality(size_t N)
{
    std::mt19937_64 rng(123456);
    
    // Generate random keys (positive range)
    std::vector<int64_t> keys(N);
    std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    for (size_t i = 0; i < N; ++i) keys[i] = dist(rng);
    
    // Generate missing keys (negative range - guaranteed miss)
    std::vector<int64_t> missing(N);
    std::uniform_int_distribution<int64_t> miss_dist(INT64_MIN, -1);
    for (size_t i = 0; i < N; ++i) missing[i] = miss_dist(rng);
    
    // Map with std::hash (default)
    fat_p::StableHashMap<int64_t, int64_t> fmap_std(N * 2, 0.75f);
    for (const auto& k : keys) fmap_std.insert(k, k);
    
    // Map with SplitMix64
    fat_p::StableHashMap<int64_t, int64_t, CustomHashPolicy<int64_t, int64_t, SplitMix64Hash>> fmap_sm(N * 2, 0.75f);
    for (const auto& k : keys) fmap_sm.insert(k, k);
    
    size_t idx = 0;
    
    // Find HIT - std::hash
    auto next_key = [&]() {
        int64_t k = keys[idx++];
        if (idx == keys.size()) idx = 0;
        return k;
    };
    
    double std_hit_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
        int64_t* v = fmap_std.find(next_key());
        benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
    });
    
    idx = 0;
    
    // Find HIT - SplitMix64
    double sm_hit_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
        int64_t* v = fmap_sm.find(next_key());
        benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
    });
    
    // Find MISS - std::hash
    auto next_missing = [&]() {
        int64_t k = missing[idx++];
        if (idx == missing.size()) idx = 0;
        return k;
    };
    
    idx = 0;
    
    double std_miss_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
        int64_t* v = fmap_std.find(next_missing());
        benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
    });
    
    idx = 0;
    
    // Find MISS - SplitMix64
    double sm_miss_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
        int64_t* v = fmap_sm.find(next_missing());
        benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
    });
    
    // Print results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(8) << N << " | ";
    std::cout << "Hit: SHM(std)=" << std::setw(7) << std_hit_ns 
              << " SHM(SM64)=" << std::setw(7) << sm_hit_ns
              << " (" << (std_hit_ns < sm_hit_ns ? "std wins" : "SM64 wins") << ") | ";
    std::cout << "Miss: SHM(std)=" << std::setw(7) << std_miss_ns 
              << " SHM(SM64)=" << std::setw(7) << sm_miss_ns
              << " (" << (std_miss_ns < sm_miss_ns ? "std wins" : "SM64 wins") << ")\n";
}

// ==========================================================
// Driver
// ==========================================================

void benchmark_stablehashmap()
{
    std::cout << "\nStableHashMap Benchmarks\n";

    std::cout << "\n=== FIND ===\n";
    for (size_t N : {1'000, 10'000, 50'000, 100'000, 500'000, 1'000'000, 2'000'000})
    {
        fat_p::testing::print_benchmark_context(std::cout);
        benchmark_find(N);
    }   

    std::cout << "\n=== INSERT ===\n";
    for (size_t N : {1'000, 10'000, 50'000, 100'000, 500'000, 1'000'000, 2'000'000})
    {
        fat_p::testing::print_benchmark_context(std::cout);
        benchmark_insert(N);
    }

    std::cout << "\n=== ERASE ===\n";
    for (size_t N : {1'000, 10'000, 50'000, 100'000, 500'000, 1'000'000, 2'000'000})
    {
        fat_p::testing::print_benchmark_context(std::cout);
        benchmark_erase(N);
    }

    std::cout << "\n=== BUILD FROM EMPTY ===\n";
    std::cout << "Rebuild from empty is prohibitively slow for a large number of values.\n";
    std::cout << "Large maps sizes are not appropriate for these benchmark sanity checks.\n";
    for (size_t N : {1'000, 10'000, 50'000})
    {
        fat_p::testing::print_benchmark_context(std::cout);
        benchmark_build_from_empty(N);
    }

    std::cout << "\n=== REHASH INCLUSIVE ===\n";
    for (size_t N : {1'000, 10'000, 50'000, 100'000, 500'000, 1'000'000, 2'000'000})
    {
        fat_p::testing::print_benchmark_context(std::cout);
        benchmark_rehash_inclusive(N);
    }

    std::cout << "\n=== MIXED WORKLOAD ===\n";
    for (size_t N : {10'000, 50'000, 100'000, 500'000, 1'000'000, 2'000'000})
    {
        fat_p::testing::print_benchmark_context(std::cout);
        benchmark_mixed(N);
    }

    std::cout << "\n=== HASH QUALITY IMPACT (StableHashMap: std::hash vs SplitMix64) ===\n";
    std::cout << "Compares StableHashMap<K,V> vs StableHashMap<K,V,SplitMix64Hash>.\n";
    std::cout << "SplitMix64: high-quality 64-bit mixer (not borrowed from any library).\n";
    std::cout << "SHM(std) = StableHashMap with std::hash, SHM(SM64) = StableHashMap with SplitMix64.\n\n";
    for (size_t N : {10'000, 100'000, 500'000, 1'000'000})
    {
        fat_p::testing::print_benchmark_context(std::cout);
        benchmark_hash_quality(N);
    }

    // -------------------------------------------------------------------------
    // String heterogeneous lookup benchmark
    // -------------------------------------------------------------------------
    
    std::cout << "\n=== STRING HETEROGENEOUS LOOKUP ===\n";
    std::cout << "Benefit of find(string_view) vs find(temp std::string).\n";
    std::cout << "Real workloads often have string keys but view-based lookups.\n\n";
    
    for (size_t N : {1'000, 10'000, 100'000})
    {
        // Generate string keys
        std::vector<std::string> str_keys;
        str_keys.reserve(N);
        for (size_t i = 0; i < N; ++i)
        {
            str_keys.emplace_back("config.section.subsection.item." + std::to_string(i));
        }
        
        // Build map with transparent hash/equal
        using HeteroPolicy = CustomHashPolicy<std::string, size_t, 
                                              TransparentStringHash, 
                                              TransparentStringEqual>;
        using HeteroMap = StableHashMap<std::string, size_t, HeteroPolicy>;
        HeteroMap map;
        map.reserve(N);
        for (size_t i = 0; i < N; ++i)
        {
            map.insert(str_keys[i], i);
        }
        
        size_t idx = 0;
        
        // Benchmark: find with string_view (heterogeneous - no allocation)
        auto next_view = [&]() -> std::string_view {
            const auto& s = str_keys[idx++];
            if (idx == str_keys.size()) idx = 0;
            return std::string_view{s};
        };
        
        double hetero_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
            auto* v = map.find(next_view());
            benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
        });
        
        idx = 0;
        
        // Benchmark: find with temp string (simulates no heterogeneous lookup)
        auto next_temp = [&]() -> std::string {
            const auto& s = str_keys[idx++];
            if (idx == str_keys.size()) idx = 0;
            return std::string{std::string_view{s}};  // Force temp construction
        };
        
        double temp_ns = bench_ns_per_op<BENCH_FIND_ITERS>([&]() {
            auto* v = map.find(next_temp());
            benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
        });
        
        double speedup = temp_ns / hetero_ns;
        double savings = temp_ns - hetero_ns;
        
        std::cout << std::setw(8) << N << " | ";
        std::cout << "string_view=" << std::setw(7) << hetero_ns << " ns | ";
        std::cout << "temp_string=" << std::setw(7) << temp_ns << " ns | ";
        std::cout << "Speedup: " << std::setprecision(2) << speedup << "x ";
        std::cout << "(saves " << std::setprecision(1) << savings << " ns)\n";
    }


    // -------------------------------------------------------------------------
    // Load factor sensitivity benchmark
    // -------------------------------------------------------------------------

    std::cout << "\n=== LOAD FACTOR SENSITIVITY ===\n\n";

    constexpr size_t LF_BUCKETS = 65536;
    constexpr size_t BATCH_SIZE = 1000;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Load Factor |   Find (ns) |  Insert (ns) |  Erase (ns)\n";
    std::cout << "------------|-------------|--------------|------------\n";

    for (float target_load : {0.50f, 0.60f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f})
    {
        const size_t base_elements =
            static_cast<size_t>(LF_BUCKETS * target_load);

        std::vector<int> base_keys(base_elements);
        std::iota(base_keys.begin(), base_keys.end(), 0);
        std::shuffle(base_keys.begin(), base_keys.end(), std::mt19937(12345));

        std::vector<int> insert_batch(BATCH_SIZE);
        std::iota(insert_batch.begin(), insert_batch.end(),
            static_cast<int>(base_elements));

        std::vector<int> find_batch(
            base_keys.begin(), base_keys.begin() + BATCH_SIZE);
        std::shuffle(find_batch.begin(), find_batch.end(), std::mt19937(999));

        std::vector<int> erase_batch(
            base_keys.begin(), base_keys.begin() + BATCH_SIZE);

        // -----------------------------------------------------
        // FIND (read-only, frozen)
        // -----------------------------------------------------

        fat_p::StableHashMap<int, int> find_map(LF_BUCKETS, 0.99f);
        for (int k : base_keys)
            find_map.insert(k, k * 10);
        find_map.freeze();

        double find_ns = bench_ns_per_op<BATCH_SIZE>([&]() {
            long long sum = 0;
            for (int k : find_batch) {
                int* v = find_map.find(k);
                if (v) sum += *v;
            }
            benchmark_sink += static_cast<size_t>(sum);
            });

        // -----------------------------------------------------
        // INSERT (restore via reset)
        // -----------------------------------------------------

        fat_p::StableHashMap<int, int> insert_map(LF_BUCKETS, 0.99f);

        auto insert_reset = [&]() {
            insert_map.clear();
            for (int k : base_keys)
                insert_map.insert(k, k);
            };

        double insert_ns = bench_ns_per_op<BATCH_SIZE>(
            insert_reset,
            [&]() {
                for (int k : insert_batch)
                    insert_map.insert(k, k);
                benchmark_sink += insert_map.size();
            }
        );

        // -----------------------------------------------------
        // ERASE (restore via reset)
        // -----------------------------------------------------

        fat_p::StableHashMap<int, int> erase_map(LF_BUCKETS, 0.99f);

        auto erase_reset = [&]() {
            erase_map.clear();
            for (int k : base_keys)
                erase_map.insert(k, k);
            };

        double erase_ns = bench_ns_per_op<BATCH_SIZE>(
            erase_reset,
            [&]() {
                for (int k : erase_batch)
                    erase_map.erase(k);
                benchmark_sink += erase_map.size();
            }
        );

        std::cout << "    "
            << std::setw(5) << (target_load * 100) << "% |"
            << std::setw(11) << find_ns << "  |"
            << std::setw(12) << insert_ns << "  |"
            << std::setw(11) << erase_ns << "\n";
    }

    std::cout << "\n";
}

}  // namespace fat_p::testing::stablehashmap

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_StableHashMap()
{
    PRINT_HEADER(STABLE HASH MAP)

    TestRunner runner;

    // Core functionality tests (1-18)
    RUN_TEST_NS(runner, stablehashmap, basic_construction);
    RUN_TEST_NS(runner, stablehashmap, insert_find);
    RUN_TEST_NS(runner, stablehashmap, erase);
    RUN_TEST_NS(runner, stablehashmap, update_value);
    RUN_TEST_NS(runner, stablehashmap, clear);
    RUN_TEST_NS(runner, stablehashmap, load_factor);
    RUN_TEST_NS(runner, stablehashmap, collision_handling);
    RUN_TEST_NS(runner, stablehashmap, large_dataset);
    RUN_TEST_NS(runner, stablehashmap, string_keys);
    RUN_TEST_NS(runner, stablehashmap, erase_reinsert);
    RUN_TEST_NS(runner, stablehashmap, empty_values);
    RUN_TEST_NS(runner, stablehashmap, const_correctness);
    RUN_TEST_NS(runner, stablehashmap, bracket_operator);
    RUN_TEST_NS(runner, stablehashmap, move_semantics);
    RUN_TEST_NS(runner, stablehashmap, copy_semantics);
    RUN_TEST_NS(runner, stablehashmap, backward_shift_deletion);
    RUN_TEST_NS(runner, stablehashmap, equality_operators);
    RUN_TEST_NS(runner, stablehashmap, stress_random);

    // RAII and memory safety tests (19-24)
    RUN_TEST_NS(runner, stablehashmap, raii_erase_correctness);
    RUN_TEST_NS(runner, stablehashmap, heapbox_stress);
    RUN_TEST_NS(runner, stablehashmap, rehash_stress);
    RUN_TEST_NS(runner, stablehashmap, heavy_collision_chain);
    RUN_TEST_NS(runner, stablehashmap, insert_duplicate_key);
    RUN_TEST_NS(runner, stablehashmap, move_only_values);
    RUN_TEST_NS(runner, stablehashmap, freeze_and_fluent_api);
    RUN_TEST_NS(runner, stablehashmap, heterogeneous_lookup);
    RUN_TEST_NS(runner, stablehashmap, heterogeneous_lookup_non_transparent);
    RUN_TEST_NS(runner, stablehashmap, heterogeneous_try_emplace_rvalue);
    RUN_TEST_NS(runner, stablehashmap, safepolicy_erase_basic_guarantee);

    // Churn stability tests (verify no-tombstone invariant)
    RUN_TEST_NS(runner, stablehashmap, churn_probe_distance_stability);
    RUN_TEST_NS(runner, stablehashmap, churn_no_ghost_slots);
    RUN_TEST_NS(runner, stablehashmap, churn_latency_stability);

    stablehashmap::benchmark_stablehashmap();

    return 0 == runner.print_summary();
}

}  // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StableHashMap() ? 0 : 1;
}
#endif
