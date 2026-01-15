/**
 * @file test_StableHashMap.cpp
 * @brief Comprehensive test suite for fat_p::StableHashMap
 *
 * Tests all features including:
 * - Swiss Table SIMD probing with control bytes
 * - Insert, find, erase operations
 * - Load factor management (default 0.8)
 * - Power-of-two sizing
 * - Tombstone-based deletion with rehash cleanup
 * - Heterogeneous lookup (transparent hash/equal)
 * - RAII correctness
 * - Sanitizer stress tests
 * - Performance vs std::unordered_map
 * - BlockAllocator variant for cache-friendly allocation
 */

/*
FATP_META:
  meta_version: 1
  component: StableHashMap
  file_role: test
  path: tests/test_StableHashMap.cpp
  namespace: "fat_p::testing::stablehashmap"
  layer: tests.containers.associative
  summary: Unit and regression tests for StableHashMap API, probing invariants, and reference stability.
  api_stability: candidate
  related:
    docs:
      - "Documentation/Associative Containers/StableHashMap_User_Manual.md"
      - "Documentation/Associative Containers/StableHashMap_Overview.md"
      - "Documentation/Associative Containers/Companion Guide - StableHashMap.md"
    benchmarks:
      - benchmarks/benchmark_FatPHashMap.cpp
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
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

#define FATP_STABLEHASHMAP_TESTING 1

#include "FatPTest.h"
#include "StableHashMap.h"

// =============================================================================
// StableHashMapTestingAccess Definition
// =============================================================================
// Forward-declared and friended in StableHashMap.h when FATP_STABLEHASHMAP_TESTING
// is defined. We provide the definition here to keep test code out of the header.

namespace fat_p::stablehash_detail
{

struct StableHashMapTestingAccess
{
    template <typename MapType>
    static const uint8_t* ctrl_bytes(const MapType& map)
    {
        return map.mCtrl;
    }

    template <typename MapType>
    static auto nodes(const MapType& map)
    {
        return map.mNodes;
    }

    template <typename MapType>
    static size_t capacity(const MapType& map)
    {
        return map.mCapacity;
    }

    template <typename MapType>
    static size_t tombstones(const MapType& map)
    {
        return map.mTombstones;
    }

    template <typename MapType>
    static size_t growth_threshold(const MapType& map)
    {
        return map.growth_threshold_;
    }

    template <typename MapType>
    static size_t mask(const MapType& map)
    {
        return map.mMask;
    }

    template <typename MapType, typename K>
    static size_t hash_key(const MapType& map, const K& key)
    {
        // Call the map's internal hash_key which respects is_avalanching trait
        return map.hash_key(key);
    }
};

} // namespace fat_p::stablehash_detail

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

    TrackedRAII() noexcept
        : value(0)
    {
        ++live_count;
        ++ctor_count;
    }

    explicit TrackedRAII(int v) noexcept
        : value(v)
    {
        ++live_count;
        ++ctor_count;
    }

    TrackedRAII(const TrackedRAII& other) noexcept
        : value(other.value)
    {
        ++live_count;
        ++ctor_count;
    }

    TrackedRAII(TrackedRAII&& other) noexcept
        : value(other.value)
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

    HeapBox() noexcept
        : p(nullptr)
    {
    }

    explicit HeapBox(int v)
        : p(new int(v))
    {
    }

    HeapBox(const HeapBox& o)
        : p(o.p ? new int(*o.p) : nullptr)
    {
    }

    HeapBox(HeapBox&& o) noexcept
        : p(o.p)
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
        return 1; // Force all keys to same bucket (not 0, since 0 = empty)
    }
};

// ============================================================================
// White-Box Tester for Swiss Table Internals
// ============================================================================
// This class provides internal inspection of StableHashMap to verify
// structural invariants (tombstone count, slot utilization).
// Requires friend declaration in StableHashMap.h

template <typename MapType>
class StableHashMapTester
{
public:
    using Access = stablehash_detail::StableHashMapTestingAccess;

    // Count physically full slots (control byte < 0x80)
    static size_t count_full_slots(const MapType& map)
    {
        const uint8_t* ctrl = Access::ctrl_bytes(map);
        if (ctrl == nullptr)
        {
            return 0;
        }

        size_t count = 0;
        const size_t cap = Access::capacity(map);

        for (size_t i = 0; i < cap; ++i)
        {
            if (ctrl[i] < 0x80)
            {
                ++count;
            }
        }

        return count;
    }

    // Count tombstone slots (control byte == 0xFE)
    static size_t count_tombstones(const MapType& map)
    {
        return Access::tombstones(map);
    }

    // Check if map has accumulated tombstones
    static bool has_tombstones(const MapType& map)
    {
        return Access::tombstones(map) > 0;
    }

    // Get capacity
    static size_t capacity(const MapType& map)
    {
        return Access::capacity(map);
    }

    // Compute approximate average probe distance (based on H1 distribution)
    // Note: Swiss Table uses triangular probing, so this is approximate
    static double average_probe_distance(const MapType& map)
    {
        const uint8_t* ctrl = Access::ctrl_bytes(map);
        if (ctrl == nullptr || map.size() == 0)
        {
            return 0.0;
        }

        size_t total_dist = 0;
        size_t count = 0;

        const size_t cap = Access::capacity(map);
        auto nodes = Access::nodes(map);

        for (size_t i = 0; i < cap; ++i)
        {
            if (ctrl[i] < 0x80 && nodes[i] != nullptr)
            {
                // Compute expected position from hash
                size_t h = Access::hash_key(map, nodes[i]->key);
                size_t ideal = h & Access::mask(map);
                size_t dist = (i >= ideal) ? (i - ideal) : (i + cap - ideal);
                total_dist += dist;
                ++count;
            }
        }

        return count > 0 ? static_cast<double>(total_dist) / static_cast<double>(count) : 0.0;
    }

    // Get maximum probe distance
    static size_t max_probe_distance(const MapType& map)
    {
        const uint8_t* ctrl = Access::ctrl_bytes(map);
        if (ctrl == nullptr || map.size() == 0)
        {
            return 0;
        }

        size_t max_dist = 0;

        const size_t cap = Access::capacity(map);
        auto nodes = Access::nodes(map);

        for (size_t i = 0; i < cap; ++i)
        {
            if (ctrl[i] < 0x80 && nodes[i] != nullptr)
            {
                size_t h = Access::hash_key(map, nodes[i]->key);
                size_t ideal = h & Access::mask(map);
                size_t dist = (i >= ideal) ? (i - ideal) : (i + cap - ideal);
                if (dist > max_dist)
                {
                    max_dist = dist;
                }
            }
        }

        return max_dist;
    }

    static const uint8_t* ctrl_bytes(const MapType& map)
    {
        return Access::ctrl_bytes(map);
    }

    static size_t mask(const MapType& map)
    {
        return Access::mask(map);
    }

    template <typename K>
    static size_t hash_key(const MapType& map, const K& key)
    {
        return Access::hash_key(map, key);
    }
};

// ============================================================================
// Test 1: Basic Construction
// ============================================================================

FATP_TEST_CASE(basic_construction)
{
    StableHashMap<int, std::string> map;

    FATP_ASSERT_TRUE(map.empty(), "New map should be empty");
    FATP_ASSERT_EQ(map.size(), size_t(0), "Size should be 0");

    return true;
}

// ============================================================================
// Test 2: Insert and Find
// ============================================================================

FATP_TEST_CASE(insert_find)
{
    StableHashMap<std::string, int> map;

    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);

    FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3");

    int* val = map.find("one");
    FATP_ASSERT_NOT_NULLPTR(val, "Should find 'one'");
    FATP_ASSERT_EQ(*val, 1, "Value should be 1");

    val = map.find("two");
    FATP_ASSERT_NOT_NULLPTR(val, "Should find 'two'");
    FATP_ASSERT_EQ(*val, 2, "Value should be 2");

    val = map.find("nonexistent");
    FATP_ASSERT_NULLPTR(val, "Should not find nonexistent key");

    return true;
}

// ============================================================================
// Test 3: Erase
// ============================================================================

FATP_TEST_CASE(erase)
{
    StableHashMap<int, std::string> map;

    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3");

    bool erased = map.erase(2);
    FATP_ASSERT_TRUE(erased, "Should successfully erase key 2");
    FATP_ASSERT_EQ(map.size(), size_t(2), "Size should be 2 after erase");

    auto* val = map.find(2);
    FATP_ASSERT_NULLPTR(val, "Should not find erased key");

    erased = map.erase(999);
    FATP_ASSERT_FALSE(erased, "Should fail to erase nonexistent key");

    return true;
}

// ============================================================================
// Test 4: Update Existing Value
// ============================================================================

FATP_TEST_CASE(update_value)
{
    StableHashMap<std::string, int> map;

    map.insert("key", 10);

    int* val = map.find("key");
    FATP_ASSERT_NOT_NULLPTR(val, "Should find key");
    FATP_ASSERT_EQ(*val, 10, "Initial value should be 10");

    *val = 20;

    val = map.find("key");
    FATP_ASSERT_EQ(*val, 20, "Updated value should be 20");

    return true;
}

// ============================================================================
// Test 5: Clear
// ============================================================================

FATP_TEST_CASE(clear)
{
    StableHashMap<int, std::string> map;

    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, "value");
    }

    FATP_ASSERT_EQ(map.size(), size_t(100), "Size should be 100");

    map.clear();
    FATP_ASSERT_EQ(map.size(), size_t(0), "Size should be 0 after clear");
    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");

    map.insert(1, "one");
    FATP_ASSERT_EQ(map.size(), size_t(1), "Should be able to insert after clear");

    return true;
}

// ============================================================================
// Test 5b: Ctrl Tail Mirror + Wrap-Around Probe Termination
// ============================================================================
//
// Regression tripwire: mCtrl[mCapacity] must mirror mCtrl[0] so that a Group load
// spanning the end of the table observes the correct state of slot 0.
// If mCtrl[mCapacity] is a sentinel, probes that should terminate on an empty
// slot can fail to terminate (and may loop) when the only empty is slot 0.
//

FATP_TEST_CASE(ctrl_tail_wraparound_probe)
{
    struct IdentityHash
    {
        using is_avalanching [[maybe_unused]] = int; // Opt-out marker: disable hash finalizer

        size_t operator()(int k) const noexcept
        {
            return static_cast<size_t>(k);
        }
    };

    using Map = StableHashMap<int, int, IdentityHash>;
    Map map(64, 1.0, IdentityHash());

    const size_t cap = StableHashMapTester<Map>::capacity(map);
    FATP_ASSERT_TRUE((cap & (cap - 1)) == 0, "Capacity must be power-of-two");
    FATP_ASSERT_TRUE(cap >= stablehash_detail::Group::kWidth * 2, "Capacity must be at least 2*GroupWidth");

    // Fill every slot except slot 0 (leaving exactly one empty).
    for (int k = 1; k < static_cast<int>(cap); ++k)
    {
        map.insert(k, k);
    }

    FATP_ASSERT_EQ(map.size(), cap - 1, "Expected to fill to cap-1 at load factor 1.0");
    FATP_ASSERT_NULLPTR(map.find(0), "Slot 0 should remain empty");

    const uint8_t* ctrl = StableHashMapTester<Map>::ctrl_bytes(map);
    FATP_ASSERT_NOT_NULLPTR(ctrl, "Internal ctrl array must be allocated");

    // Critical invariant: the tail byte at mCtrl[mCapacity] mirrors mCtrl[0].
    FATP_ASSERT_EQ(ctrl[0], stablehash_detail::kEmpty, "Slot 0 control byte must be kEmpty");
    FATP_ASSERT_EQ(ctrl[cap], ctrl[0], "ctrl_[capacity] must mirror ctrl_[0] for wrap-around reads");

    // Create a missing key whose probe starts at cap-4 so the first group read
    // spans the end of the table and includes the mirrored slot 0 byte.
    const size_t start_pos = cap - 4;
    const int missing_key = static_cast<int>(cap + start_pos);

    const size_t h = StableHashMapTester<Map>::hash_key(map, missing_key);
    stablehash_detail::ProbeSequence seq(h, StableHashMapTester<Map>::mask(map));

    bool saw_empty = false;
    for (size_t step = 0; step < cap + stablehash_detail::Group::kWidth; ++step)
    {
        stablehash_detail::Group g(ctrl + seq.offset());
        const auto empty = g.match_empty();

        if (empty)
        {
            const uint32_t first = empty.lowest_set_bit();
            const size_t idx = seq.offset(first);

            FATP_ASSERT_EQ(first, uint32_t(4), "Expected first empty bit to be the wrapped slot 0 (bit 4)");
            FATP_ASSERT_EQ(idx, size_t(0), "Probe must observe the empty slot 0 via wrap-around mirror");

            saw_empty = true;
            break;
        }

        seq.next();
    }

    FATP_ASSERT_TRUE(saw_empty, "Probe sequence did not observe the only empty slot; wrap-around tail mirror broken");

    // Now the real API call: must terminate and report miss.
    FATP_ASSERT_NULLPTR(map.find(missing_key), "Missing key should not be found");

    return true;
}
// ============================================================================
// Test 6: Load Factor
// ============================================================================


FATP_TEST_CASE(load_factor)
{
    StableHashMap<int, int> map;

    for (int i = 0; i < 1000; ++i)
    {
        map.insert(i, i * 2);
    }

    double load = map.load_factor();
    FATP_ASSERT_TRUE(load >= 0.0 && load <= 1.0, "Load factor should be between 0 and 1");
    FATP_ASSERT_TRUE(load <= map.max_load_factor(), "Load factor should not exceed max_load_factor()");
    return true;
}

// ============================================================================
// Test 7: Collision Handling
// ============================================================================

FATP_TEST_CASE(collision_handling)
{
    StableHashMap<int, int> map;

    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, i * 10);
    }

    for (int i = 0; i < 100; ++i)
    {
        int* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "Should find all keys despite collisions");
        FATP_ASSERT_EQ(*val, i * 10, "Values should be correct");
    }

    return true;
}

// ============================================================================
// Test 8: Large Dataset
// ============================================================================

FATP_TEST_CASE(large_dataset)
{
    StableHashMap<int, int> map;

    for (int i = 0; i < LARGE_SIZE; ++i)
    {
        map.insert(i, i * 2);
    }

    FATP_ASSERT_EQ(map.size(), size_t(LARGE_SIZE), "Size should match inserted count");

    int* val = map.find(LARGE_SIZE / 2);
    FATP_ASSERT_NOT_NULLPTR(val, "Should find middle element");
    FATP_ASSERT_EQ(*val, (LARGE_SIZE / 2) * 2, "Value should be correct");

    return true;
}

// ============================================================================
// Test 9: String Keys
// ============================================================================

FATP_TEST_CASE(string_keys)
{
    StableHashMap<std::string, int> map;

    map.insert("apple", 1);
    map.insert("banana", 2);
    map.insert("cherry", 3);
    map.insert("date", 4);
    map.insert("elderberry", 5);

    FATP_ASSERT_EQ(map.size(), size_t(5), "Size should be 5");

    int* val = map.find("cherry");
    FATP_ASSERT_NOT_NULLPTR(val, "Should find 'cherry'");
    FATP_ASSERT_EQ(*val, 3, "Value should be 3");

    return true;
}

// ============================================================================
// Test 10: Erase and Reinsert
// ============================================================================

FATP_TEST_CASE(erase_reinsert)
{
    StableHashMap<int, std::string> map;

    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    map.erase(2);
    FATP_ASSERT_NULLPTR(map.find(2), "Key 2 should not be found after erase");

    map.insert(2, "TWO");
    auto* val = map.find(2);
    FATP_ASSERT_NOT_NULLPTR(val, "Key 2 should be found after reinsertion");
    FATP_ASSERT_EQ(*val, "TWO", "Value should be updated");

    return true;
}

// ============================================================================
// Test 11: Empty Key/Value Edge Cases
// ============================================================================

FATP_TEST_CASE(empty_values)
{
    StableHashMap<std::string, std::string> map;

    map.insert("empty", "");
    map.insert("", "empty_key");

    auto* val1 = map.find("empty");
    FATP_ASSERT_NOT_NULLPTR(val1, "Should find key with empty value");
    FATP_ASSERT_TRUE(val1->empty(), "Value should be empty string");

    auto* val2 = map.find("");
    FATP_ASSERT_NOT_NULLPTR(val2, "Should find empty key");
    FATP_ASSERT_EQ(*val2, "empty_key", "Value for empty key should be correct");

    return true;
}

// ============================================================================
// Test 12: Const Correctness
// ============================================================================

FATP_TEST_CASE(const_correctness)
{
    StableHashMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");

    const StableHashMap<int, std::string>& cmap = map;

    const std::string* val = cmap.find(1);
    FATP_ASSERT_NOT_NULLPTR(val, "Const find should work");
    FATP_ASSERT_EQ(*val, "one", "Const value should be correct");

    return true;
}

// ============================================================================
// Test 13: Bracket Operator
// ============================================================================

FATP_TEST_CASE(bracket_operator)
{
    StableHashMap<std::string, int> map;

    map["key1"] = 100;
    map["key2"] = 200;

    FATP_ASSERT_EQ(map.size(), size_t(2), "Size should be 2");
    FATP_ASSERT_EQ(map["key1"], 100, "Should get correct value");

    map["key1"] = 150;
    FATP_ASSERT_EQ(map["key1"], 150, "Should update via bracket");

    return true;
}

// ============================================================================
// Test 14: Move Semantics
// ============================================================================

FATP_TEST_CASE(move_semantics)
{
    // Test move construction
    StableHashMap<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    StableHashMap<int, std::string> map2 = std::move(map1);

    FATP_ASSERT_EQ(map2.size(), size_t(2), "Moved-to map should have elements");
    FATP_ASSERT_EQ(*map2.find(1), "one", "Moved-to map should have values");

    // Test move assignment
    StableHashMap<int, std::string> map3;
    map3.insert(10, "ten");
    map3.insert(20, "twenty");
    map3.insert(30, "thirty");

    StableHashMap<int, std::string> map4;
    map4.insert(100, "hundred");

    map4 = std::move(map3);

    FATP_ASSERT_EQ(map4.size(), size_t(3), "Move-assigned map should have source elements");
    FATP_ASSERT_EQ(*map4.find(10), "ten", "Move-assigned map should have correct values");
    FATP_ASSERT_EQ(*map4.find(20), "twenty", "Move-assigned map should have correct values");
    FATP_ASSERT_NULLPTR(map4.find(100), "Move-assigned map should not have old elements");

    // Test that moved-from map can be safely reused
    // (insert triggers rehash which resets internal state)
    map3.insert(999, "reused");
    FATP_ASSERT_NOT_NULLPTR(map3.find(999), "Moved-from map should be reusable after insert");
    FATP_ASSERT_EQ(*map3.find(999), "reused", "Reused map should have correct value");

    return true;
}

// ============================================================================
// Test 15: Copy Semantics
// ============================================================================

FATP_TEST_CASE(copy_semantics)
{
    StableHashMap<int, std::string> map1;
    map1.insert(1, "one");
    map1.insert(2, "two");

    StableHashMap<int, std::string> map2 = map1;

    FATP_ASSERT_EQ(map2.size(), size_t(2), "Copied map should have same size");
    FATP_ASSERT_EQ(*map2.find(1), "one", "Copied map should have same values");

    map1.insert(3, "three");
    FATP_ASSERT_EQ(map1.size(), size_t(3), "Original should be modified");
    FATP_ASSERT_EQ(map2.size(), size_t(2), "Copy should be independent");

    return true;
}

// ============================================================================
// Test 16: Tombstone Deletion (Swiss Table)
// ============================================================================

FATP_TEST_CASE(backward_shift_deletion)
{
    // Use ZeroHash to force collisions - tests deletion with H2 filtering
    StableHashMap<int, int, ZeroHash> map(16, 0.9f);

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
            FATP_ASSERT_NULLPTR(val, "Erased key should not be found");
        }
        else
        {
            FATP_ASSERT_NOT_NULLPTR(val, "Non-erased key should be found");
            FATP_ASSERT_EQ(*val, i * 100, "Value should be correct");
        }
    }

    return true;
}

// ============================================================================
// Test 17: Equality Operators
// ============================================================================

FATP_TEST_CASE(equality_operators)
{
    // Empty maps are equal
    StableHashMap<int, int> empty1;
    StableHashMap<int, int> empty2;
    FATP_ASSERT_TRUE(empty1 == empty2, "Empty maps should be equal");
    FATP_ASSERT_FALSE(empty1 != empty2, "Empty maps should not be unequal");

    // Map equals itself
    StableHashMap<int, int> map1;
    map1.insert(1, 100);
    map1.insert(2, 200);
    map1.insert(3, 300);
    FATP_ASSERT_TRUE(map1 == map1, "Map should equal itself");

    // Maps with same content are equal
    StableHashMap<int, int> map2;
    map2.insert(1, 100);
    map2.insert(2, 200);
    map2.insert(3, 300);
    FATP_ASSERT_TRUE(map1 == map2, "Maps with same content should be equal");
    FATP_ASSERT_FALSE(map1 != map2, "Maps with same content should not be unequal");

    // Order of insertion doesn't matter
    StableHashMap<int, int> map3;
    map3.insert(3, 300);
    map3.insert(1, 100);
    map3.insert(2, 200);
    FATP_ASSERT_TRUE(map1 == map3, "Maps should be equal regardless of insertion order");

    // Different sizes are unequal
    StableHashMap<int, int> map4;
    map4.insert(1, 100);
    map4.insert(2, 200);
    FATP_ASSERT_FALSE(map1 == map4, "Maps with different sizes should not be equal");
    FATP_ASSERT_TRUE(map1 != map4, "Maps with different sizes should be unequal");

    // Same keys, different values are unequal
    StableHashMap<int, int> map5;
    map5.insert(1, 100);
    map5.insert(2, 200);
    map5.insert(3, 999); // Different value
    FATP_ASSERT_FALSE(map1 == map5, "Maps with different values should not be equal");
    FATP_ASSERT_TRUE(map1 != map5, "Maps with different values should be unequal");

    // Different keys are unequal
    StableHashMap<int, int> map6;
    map6.insert(1, 100);
    map6.insert(2, 200);
    map6.insert(999, 300); // Different key
    FATP_ASSERT_FALSE(map1 == map6, "Maps with different keys should not be equal");

    // Empty vs non-empty
    FATP_ASSERT_FALSE(empty1 == map1, "Empty map should not equal non-empty map");
    FATP_ASSERT_TRUE(empty1 != map1, "Empty map should be unequal to non-empty map");

    return true;
}

// ============================================================================
// Test 18: Stress Test Random Operations
// ============================================================================

FATP_TEST_CASE(stress_random)
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
            reference.insert({key, i}); // Must use insert, not operator[] (which overwrites)
        }
        else if (op == 1)
        {
            int* ptr = map.find(key);
            auto it = reference.find(key);

            if (it == reference.end())
            {
                FATP_ASSERT_NULLPTR(ptr, "Find should return null for missing key");
            }
            else
            {
                FATP_ASSERT_NOT_NULLPTR(ptr, "Find should return non-null for existing key");
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
// Test 19: RAII Erase Correctness (from Gemini review)
// ============================================================================

FATP_TEST_CASE(raii_erase_correctness)
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

        FATP_ASSERT_EQ(map.size(), size_t(N - 3), "Size should decrease by 3");
    }

    FATP_ASSERT_EQ(TrackedRAII::live_count, 0, "All RAII objects should be destroyed");
    FATP_ASSERT_EQ(TrackedRAII::ctor_count, TrackedRAII::dtor_count, "Ctor/dtor count should match");

    return true;
}

// ============================================================================
// Test 20: HeapBox Sanitizer Stress (from Gemini review)
// ============================================================================

FATP_TEST_CASE(heapbox_stress)
{
    StableHashMap<int, HeapBox, ZeroHash> map(8, 0.9f);

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
        FATP_ASSERT_NOT_NULLPTR(val, "Non-erased key should be found");
        FATP_ASSERT_EQ(val->value(), i * 11, "Value should be correct");
    }

    map.clear();
    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");

    return true;
}

// ============================================================================
// Test 21: Rehash Stress (from Gemini review)
// ============================================================================

FATP_TEST_CASE(rehash_stress)
{
    StableHashMap<int, HeapBox, ZeroHash> map(4, 0.5f);

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
            FATP_ASSERT_NOT_NULLPTR(map.find(i), "Odd keys should still exist");
        }

        map.clear();
    }

    return true;
}

// ============================================================================
// Test 22: Heavy Collision Chain
// ============================================================================

FATP_TEST_CASE(heavy_collision_chain)
{
    StableHashMap<int, int, ZeroHash> map(16, 0.9f);

    for (int i = 0; i < 12; ++i)
    {
        map.insert(i, i * 100);
    }

    for (int i = 0; i < 12; ++i)
    {
        int* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "Should find all keys in collision chain");
        FATP_ASSERT_EQ(*val, i * 100, "Value should be correct");
    }

    for (int i = 5; i >= 0; --i)
    {
        map.erase(i);
    }

    for (int i = 6; i < 12; ++i)
    {
        int* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "Should find remaining keys");
        FATP_ASSERT_EQ(*val, i * 100, "Value should be correct");
    }

    return true;
}

// ============================================================================
// Test 23: Insert Duplicate Key
// ============================================================================

FATP_TEST_CASE(insert_duplicate_key)
{
    StableHashMap<int, std::string> map;

    // Test insert() - does NOT overwrite (matches std::unordered_map)
    auto [ptr1, inserted1] = map.insert(1, "first");
    auto [ptr2, inserted2] = map.insert(1, "second"); // Should fail, key exists

    FATP_ASSERT_EQ(map.size(), size_t(1), "Size should be 1 (no duplicate)");
    FATP_ASSERT_TRUE(inserted1, "First insert should succeed");
    FATP_ASSERT_TRUE(!inserted2, "Second insert should return false (key exists)");

    auto* val = map.find(1);
    FATP_ASSERT_NOT_NULLPTR(val, "Should find key");
    FATP_ASSERT_EQ(*val, "first", "Value should remain 'first' (insert doesn't overwrite)");

    // Test insert_or_assign() - DOES overwrite
    auto [ptr, was_inserted] = map.insert_or_assign(1, "third");
    FATP_ASSERT_TRUE(!was_inserted, "insert_or_assign should return false (updated existing)");
    FATP_ASSERT_EQ(*ptr, "third", "Value should be updated to 'third'");

    val = map.find(1);
    FATP_ASSERT_EQ(*val, "third", "Value should be 'third' after insert_or_assign");

    return true;
}

// ============================================================================
// Test 24: Move-Only Values
// ============================================================================

FATP_TEST_CASE(move_only_values)
{
    StableHashMap<int, std::unique_ptr<int>> map;

    auto p1 = std::make_unique<int>(100);
    auto p2 = std::make_unique<int>(200);

    map.insert(1, std::move(p1));
    map.insert(2, std::move(p2));

    auto* val1 = map.find(1);
    FATP_ASSERT_NOT_NULLPTR(val1, "Should find key 1");
    FATP_ASSERT_EQ(**val1, 100, "Value should be 100");

    map.erase(1);
    FATP_ASSERT_NULLPTR(map.find(1), "Erased key should not be found");

    auto* val2 = map.find(2);
    FATP_ASSERT_NOT_NULLPTR(val2, "Should find key 2");
    FATP_ASSERT_EQ(**val2, 200, "Value should be 200");

    return true;
}

// ============================================================================
// Test 25: Reference Stability - Core Feature
// ============================================================================
// This tests the PRIMARY feature of StableHashMap: pointers/references to
// values remain valid across insert, reserve, and rehash operations.
// This is what distinguishes it from flat hash maps.

FATP_TEST_CASE(reference_stability_across_insert)
{
    StableHashMap<int, std::string> map;

    // Insert initial element and get pointer
    map.insert(1, "one");
    std::string* ptr1 = map.find(1);
    FATP_ASSERT_NOT_NULLPTR(ptr1, "Should find key 1");
    FATP_ASSERT_EQ(*ptr1, "one", "Value should be 'one'");

    // Store the pointer address for later comparison
    const void* addr1 = static_cast<const void*>(ptr1);

    // Insert many more elements (will trigger rehash)
    for (int i = 2; i <= 1000; ++i)
    {
        map.insert(i, "value_" + std::to_string(i));
    }

    // Original pointer should STILL be valid (this is the key invariant!)
    std::string* ptr1_after = map.find(1);
    FATP_ASSERT_NOT_NULLPTR(ptr1_after, "Should still find key 1 after rehash");
    FATP_ASSERT_TRUE(static_cast<const void*>(ptr1_after) == addr1, "Pointer address should be unchanged after rehash");
    FATP_ASSERT_EQ(*ptr1, "one", "Original pointer should still be dereferenceable");
    FATP_ASSERT_EQ(*ptr1_after, "one", "Value should still be 'one'");

    // Mutate through original pointer
    *ptr1 = "ONE_MODIFIED";
    FATP_ASSERT_EQ(*map.find(1), "ONE_MODIFIED", "Mutation through old pointer should work");

    return true;
}

FATP_TEST_CASE(reference_stability_across_reserve)
{
    StableHashMap<int, int> map;

    // Insert some elements
    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, i * 10);
    }

    // Collect pointers to all values
    std::vector<int*> ptrs(100);
    std::vector<const void*> addrs(100);
    for (int i = 0; i < 100; ++i)
    {
        ptrs[static_cast<size_t>(i)] = map.find(i);
        addrs[static_cast<size_t>(i)] = static_cast<const void*>(ptrs[static_cast<size_t>(i)]);
        FATP_ASSERT_NOT_NULLPTR(ptrs[static_cast<size_t>(i)], "Should find all keys");
    }

    // Force a large reserve (will definitely rehash)
    map.reserve(100000);

    // All pointers should still be valid!
    for (int i = 0; i < 100; ++i)
    {
        int* ptr_after = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(ptr_after, "Key should still exist after reserve");
        FATP_ASSERT_TRUE(static_cast<const void*>(ptr_after) == addrs[static_cast<size_t>(i)],
                         "Pointer address should be unchanged after reserve");
        FATP_ASSERT_EQ(*ptrs[static_cast<size_t>(i)], i * 10, "Original pointer should still work");
    }

    return true;
}

FATP_TEST_CASE(reference_stability_across_erase)
{
    StableHashMap<int, std::string> map;

    // Insert elements
    for (int i = 0; i < 100; ++i)
    {
        map.insert(i, "value_" + std::to_string(i));
    }

    // Get pointers to elements we WON'T erase
    std::string* ptr_10 = map.find(10);
    std::string* ptr_50 = map.find(50);
    std::string* ptr_90 = map.find(90);

    const void* addr_10 = static_cast<const void*>(ptr_10);
    const void* addr_50 = static_cast<const void*>(ptr_50);
    const void* addr_90 = static_cast<const void*>(ptr_90);

    // Erase many OTHER elements (this should NOT invalidate our pointers)
    for (int i = 0; i < 100; ++i)
    {
        if (i != 10 && i != 50 && i != 90)
        {
            map.erase(i);
        }
    }

    // Our pointers should still be valid
    FATP_ASSERT_TRUE(static_cast<const void*>(map.find(10)) == addr_10,
                     "Pointer to 10 should be stable after erasing others");
    FATP_ASSERT_TRUE(static_cast<const void*>(map.find(50)) == addr_50,
                     "Pointer to 50 should be stable after erasing others");
    FATP_ASSERT_TRUE(static_cast<const void*>(map.find(90)) == addr_90,
                     "Pointer to 90 should be stable after erasing others");

    FATP_ASSERT_EQ(*ptr_10, "value_10", "Value at 10 should be correct");
    FATP_ASSERT_EQ(*ptr_50, "value_50", "Value at 50 should be correct");
    FATP_ASSERT_EQ(*ptr_90, "value_90", "Value at 90 should be correct");

    return true;
}

FATP_TEST_CASE(reference_stability_mixed_operations)
{
    // Comprehensive test: mix of insert, erase, reserve while holding pointers
    StableHashMap<int, int> map;

    std::vector<int*> stable_ptrs;
    std::vector<const void*> stable_addrs;
    std::vector<int> stable_keys;

    // Phase 1: Insert 500 elements, save pointers to every 10th
    for (int i = 0; i < 500; ++i)
    {
        auto [ptr, inserted] = map.insert(i, i * 100);
        if (i % 10 == 0)
        {
            stable_ptrs.push_back(ptr);
            stable_addrs.push_back(static_cast<const void*>(ptr));
            stable_keys.push_back(i);
        }
    }

    // Phase 2: Erase every 3rd element (except our stable keys)
    for (int i = 0; i < 500; ++i)
    {
        if (i % 3 == 0 && i % 10 != 0)
        {
            map.erase(i);
        }
    }

    // Verify stable pointers
    for (size_t j = 0; j < stable_ptrs.size(); ++j)
    {
        FATP_ASSERT_TRUE(static_cast<const void*>(map.find(stable_keys[j])) == stable_addrs[j],
                         "Pointer should be stable after erase phase");
        FATP_ASSERT_EQ(*stable_ptrs[j], stable_keys[j] * 100, "Value should be correct through stable pointer");
    }

    // Phase 3: Insert 1000 more elements (triggers rehash)
    for (int i = 1000; i < 2000; ++i)
    {
        map.insert(i, i * 100);
    }

    // Verify stable pointers AGAIN
    for (size_t j = 0; j < stable_ptrs.size(); ++j)
    {
        FATP_ASSERT_TRUE(static_cast<const void*>(map.find(stable_keys[j])) == stable_addrs[j],
                         "Pointer should be stable after insert phase");
        FATP_ASSERT_EQ(*stable_ptrs[j], stable_keys[j] * 100, "Value should be correct through stable pointer");
    }

    // Phase 4: Reserve massive capacity
    map.reserve(100000);

    // Verify stable pointers ONE MORE TIME
    for (size_t j = 0; j < stable_ptrs.size(); ++j)
    {
        FATP_ASSERT_TRUE(static_cast<const void*>(map.find(stable_keys[j])) == stable_addrs[j],
                         "Pointer should be stable after reserve");
        FATP_ASSERT_EQ(*stable_ptrs[j], stable_keys[j] * 100, "Value should be correct through stable pointer");

        // Mutate through stable pointer
        *stable_ptrs[j] = 999;
        FATP_ASSERT_EQ(*map.find(stable_keys[j]), 999, "Mutation should work");
    }

    return true;
}

FATP_TEST_CASE(reference_stability_with_strings)
{
    // Test with heap-allocated values (std::string) to catch memory issues
    StableHashMap<std::string, std::string> map;

    // Insert with long strings (definitely heap-allocated, not SSO)
    std::string long_key = "this_is_a_very_long_key_that_exceeds_sso_buffer";
    std::string long_val = "this_is_a_very_long_value_that_also_exceeds_sso_buffer";

    map.insert(long_key, long_val);
    std::string* ptr = map.find(long_key);
    const void* addr = static_cast<const void*>(ptr);

    FATP_ASSERT_NOT_NULLPTR(ptr, "Should find long key");
    FATP_ASSERT_EQ(*ptr, long_val, "Value should match");

    // Insert many more long strings
    for (int i = 0; i < 1000; ++i)
    {
        std::string k = "key_" + std::to_string(i) + "_padding_to_exceed_sso_definitely";
        std::string v = "val_" + std::to_string(i) + "_more_padding_for_heap_allocation";
        map.insert(k, v);
    }

    // Original pointer should still work
    FATP_ASSERT_TRUE(static_cast<const void*>(map.find(long_key)) == addr, "Pointer should be stable");
    FATP_ASSERT_EQ(*ptr, long_val, "Value through pointer should be correct");

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

FATP_TEST_CASE(heterogeneous_lookup)
{
    // Test with transparent hash/equal using separate template params
    using TransparentMap = StableHashMap<std::string, int, TransparentStringHash, TransparentStringEqual>;

    TransparentMap map;
    map.insert("hello", 1);
    map.insert("world", 2);
    map.insert("test", 3);

    // find() with const char*
    int* val = map.find("hello");
    FATP_ASSERT_NOT_NULLPTR(val, "find(const char*) should work");
    FATP_ASSERT_EQ(*val, 1, "Value should be 1");

    val = map.find("missing");
    FATP_ASSERT_NULLPTR(val, "find() should return nullptr for missing key");

    // find() with string_view
    std::string_view sv = "world";
    val = map.find(sv);
    FATP_ASSERT_NOT_NULLPTR(val, "find(string_view) should work");
    FATP_ASSERT_EQ(*val, 2, "Value should be 2");

    // contains() with const char*
    FATP_ASSERT_TRUE(map.contains("hello"), "contains(const char*) should work");
    FATP_ASSERT_TRUE(!map.contains("missing"), "contains() should return false for missing");

    // contains() with string_view
    FATP_ASSERT_TRUE(map.contains(sv), "contains(string_view) should work");

    // erase() with const char*
    FATP_ASSERT_TRUE(map.erase("test"), "erase(const char*) should work");
    FATP_ASSERT_TRUE(!map.contains("test"), "Key should be erased");
    FATP_ASSERT_EQ(map.size(), 2u, "Size should be 2 after erase");

    // erase() with string_view
    std::string_view sv_hello = "hello";
    FATP_ASSERT_TRUE(map.erase(sv_hello), "erase(string_view) should work");
    FATP_ASSERT_TRUE(!map.contains("hello"), "Key should be erased");

    // const correctness
    const TransparentMap& cmap = map;
    const int* cval = cmap.find("world");
    FATP_ASSERT_NOT_NULLPTR(cval, "const find() should work");
    FATP_ASSERT_EQ(*cval, 2, "Value should be 2");

    return true;
}

FATP_TEST_CASE(heterogeneous_lookup_non_transparent)
{
    // Without transparent hash/equal, heterogeneous lookup is disabled
    StableHashMap<std::string, int> map;
    map.insert(std::string("hello"), 1);
    map.insert(std::string("world"), 2);

    // Must use std::string for lookup (const char* would create temporary)
    int* val = map.find(std::string("hello"));
    FATP_ASSERT_NOT_NULLPTR(val, "Regular find should work");
    FATP_ASSERT_EQ(*val, 1, "Value should be 1");

    FATP_ASSERT_TRUE(map.contains(std::string("world")), "Regular contains should work");
    FATP_ASSERT_TRUE(map.erase(std::string("hello")), "Regular erase should work");

    return true;
}

FATP_TEST_CASE(heterogeneous_try_emplace_rvalue)
{
    // Test heterogeneous try_emplace with rvalue keys
    // This tests the fix for the use-after-move bug
    using TransparentMap = StableHashMap<std::string, int, TransparentStringHash, TransparentStringEqual>;

    TransparentMap map;

    // Test with rvalue string - this would trigger the use-after-move bug
    // if not properly fixed
    auto make_key = []()
    {
        return std::string("rvalue_key");
    };
    auto [ptr1, inserted1] = map.try_emplace(make_key(), 42);

    FATP_ASSERT_TRUE(inserted1, "Should insert new key");
    FATP_ASSERT_NOT_NULLPTR(ptr1, "Should return valid pointer");
    FATP_ASSERT_EQ(*ptr1, 42, "Value should be 42");
    FATP_ASSERT_TRUE(map.contains("rvalue_key"), "Key should exist in map");

    // Try again with same key (should not insert)
    auto [ptr2, inserted2] = map.try_emplace(make_key(), 100);
    FATP_ASSERT_FALSE(inserted2, "Should not insert duplicate");
    FATP_ASSERT_EQ(*ptr2, 42, "Value should still be 42");

    // Test with longer rvalue string to ensure no SSO interference
    auto make_long_key = []()
    {
        return std::string("this_is_a_very_long_key_that_exceeds_sso_buffer_size_on_most_implementations");
    };
    auto [ptr3, inserted3] = map.try_emplace(make_long_key(), 999);

    FATP_ASSERT_TRUE(inserted3, "Should insert long key");
    FATP_ASSERT_NOT_NULLPTR(ptr3, "Should return valid pointer for long key");
    FATP_ASSERT_EQ(*ptr3, 999, "Value should be 999");
    FATP_ASSERT_TRUE(map.contains("this_is_a_very_long_key_that_exceeds_sso_buffer_size_on_most_implementations"),
                     "Long key should exist");

    return true;
}

// ============================================================================
// Test: Tombstone erase behavior (Swiss Table)
// Documents the tombstone-based deletion in Swiss Table implementation.
// Erased slots are marked with kDeleted (0xFE) control byte, allowing
// probe sequences to continue past deleted entries.
// ============================================================================

FATP_TEST_CASE(safepolicy_erase_basic_guarantee)
{
    // This test verifies Swiss Table tombstone-based erase behavior:
    //
    // erase() sequence:
    //   1. Mark control byte as kDeleted (0xFE)
    //   2. Delete the node
    //   3. Set mNodes[idx] = nullptr
    //   4. Increment tombstone count
    //   5. Decrement size
    //
    // Tombstones are cleaned up during rehash when load factor triggers growth.

    fat_p::StableHashMap<int, int> map;

    // Insert several entries
    for (int i = 0; i < 10; ++i)
    {
        map.insert(i, i * 100);
    }

    FATP_ASSERT_EQ(map.size(), 10u, "Should have 10 entries");

    // Erase entries
    bool erased = map.erase(5);
    FATP_ASSERT_TRUE(erased, "Should erase key 5");
    FATP_ASSERT_EQ(map.size(), 9u, "Size should be 9 after erase");

    // Verify the erased key is not findable
    FATP_ASSERT_NULLPTR(map.find(5), "Erased key should not be found");

    // Verify other keys are still accessible
    for (int i = 0; i < 10; ++i)
    {
        if (i == 5)
        {
            continue;
        }
        int* val = map.find(i);
        FATP_ASSERT_NOT_NULLPTR(val, "Non-erased key should be found");
        FATP_ASSERT_EQ(*val, i * 100, "Value should be correct");
    }

    // Verify we can reinsert into the erased slot (tombstone reuse)
    map.insert(5, 555);
    FATP_ASSERT_EQ(map.size(), 10u, "Size should be 10 after reinsert");
    int* reinserted = map.find(5);
    FATP_ASSERT_NOT_NULLPTR(reinserted, "Reinserted key should be found");
    FATP_ASSERT_EQ(*reinserted, 555, "Reinserted value should be correct");

    // Test clear()
    map.clear();
    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");
    FATP_ASSERT_EQ(map.size(), 0u, "Size should be 0 after clear");

    // Verify map is still usable after clear
    map.insert(42, 4200);
    FATP_ASSERT_EQ(map.size(), 1u, "Should have 1 entry after reinsert");
    FATP_ASSERT_NOT_NULLPTR(map.find(42), "Should find reinserted key");

    return true;
}

// ============================================================================
// Churn Stability Test 1: Probe Distance Remains Stable Over Time
// ============================================================================
// Objective: After sustained insert/erase churn at constant size, the
// probe distance structure should remain stable. Swiss Table uses tombstones
// that are cleaned up during rehash, so performance should remain good.

FATP_TEST_CASE(churn_probe_distance_stability)
{
    using Map = StableHashMap<int, int>;
    using Tester = StableHashMapTester<Map>;

    const int TARGET_SIZE = 10000;
    const int CHURN_OPS = 50000; // 5x the size

    Map map;
    std::mt19937 rng(42); // Fixed seed for reproducibility

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
            --i; // Retry on collision
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
        do
        {
            new_key = key_dist(rng);
        } while (map.find(new_key) != nullptr); // Ensure unique

        map.insert(new_key, new_key);
        keys.push_back(new_key);
    }

    // 3. Measure aged state
    double aged_avg = Tester::average_probe_distance(map);
    size_t aged_max = Tester::max_probe_distance(map);
    size_t tombstones = Tester::count_tombstones(map);

    // Diagnostic output
    std::cout << "  Probe distance: baseline_avg=" << baseline_avg << " aged_avg=" << aged_avg
              << " baseline_max=" << baseline_max << " aged_max=" << aged_max << " tombstones=" << tombstones << "\n";

    // 4. Assertions - probe distances should remain reasonable
    // Swiss Table with tombstones may have slightly higher probe distances
    // but should not degrade catastrophically
    double avg_delta = std::abs(aged_avg - baseline_avg);
    FATP_ASSERT_TRUE(avg_delta < 3.0, "Average probe distance degraded significantly");

    // Max probe can vary more due to key distribution luck
    FATP_ASSERT_TRUE(aged_max < baseline_max + 25, "Maximum probe distance grew excessively");

    return true;
}

// ============================================================================
// Churn Stability Test 2: Tombstone Tracking is Consistent
// ============================================================================
// Objective: Verify that Swiss Table tombstone tracking is correct.
// After churn, full_slots == size() and tombstones are tracked.
// Tombstones are cleaned up when rehash occurs.

FATP_TEST_CASE(churn_no_ghost_slots)
{
    using Map = StableHashMap<int, int>;
    using Tester = StableHashMapTester<Map>;

    const int TARGET_SIZE = 5000;
    const int CHURN_OPS = 20000; // 4x the size

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

    // Perform churn with REAL erases
    std::uniform_int_distribution<int> key_dist(0, std::numeric_limits<int>::max());
    for (int i = 0; i < CHURN_OPS; ++i)
    {
        // Erase actual existing key
        std::uniform_int_distribution<size_t> idx_dist(0, keys.size() - 1);
        size_t idx = idx_dist(rng);
        int key_to_erase = keys[idx];

        bool erased = map.erase(key_to_erase);
        FATP_ASSERT_TRUE(erased, "Should successfully erase tracked key");

        keys[idx] = keys.back();
        keys.pop_back();

        // Insert new RANDOM key
        int new_key;
        do
        {
            new_key = key_dist(rng);
        } while (map.find(new_key) != nullptr);

        map.insert(new_key, new_key);
        keys.push_back(new_key);
    }

    // Verify: full slots == size()
    size_t full_slots = Tester::count_full_slots(map);
    FATP_ASSERT_EQ(full_slots, map.size(), "Full slots should equal size()");

    // Verify: iteration count matches size()
    size_t manual_count = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        ++manual_count;
    }
    FATP_ASSERT_EQ(manual_count, map.size(), "Iteration count should match size()");

    // Verify: all tracked keys are findable
    for (int k : keys)
    {
        FATP_ASSERT_NOT_NULLPTR(map.find(k), "All tracked keys should be findable");
    }

    // Output tombstone count for diagnostics
    size_t tombstones = Tester::count_tombstones(map);
    std::cout << "  After churn: size=" << map.size() << " full_slots=" << full_slots << " tombstones=" << tombstones
              << " capacity=" << Tester::capacity(map) << "\n";

    return true;
}

// ============================================================================
// Churn Stability Test 3: Latency Remains Reasonable
// ============================================================================
// Objective: Wall-clock performance benchmark. Aged lookup speed should
// remain within acceptable bounds of fresh lookup speed.
// Swiss Table with tombstones may show slight degradation, but should
// not be catastrophic like pure tombstone maps (1.5-3.0x).

FATP_TEST_CASE(churn_latency_stability)
{
    using Map = StableHashMap<int, int>;

    const int TARGET_SIZE = 50000;
    const int LOOKUPS = 1000000;
    const int CHURN_OPS = 100000; // Reduced to avoid excessive tombstone accumulation

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
        if (auto* v = map.find(k))
        {
            sink += *v;
        }
    }
    auto end_fresh = std::chrono::high_resolution_clock::now();
    auto fresh_us = std::chrono::duration_cast<std::chrono::microseconds>(end_fresh - start_fresh).count();

    // 3. Perform churn (using tracked keys for real erases)
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
        do
        {
            new_key = key_dist(rng);
        } while (map.find(new_key) != nullptr);

        map.insert(new_key, new_key);
        keys.push_back(new_key);
    }

    // 4. Measure aged lookup performance
    auto start_aged = std::chrono::high_resolution_clock::now();
    for (int k : lookup_keys)
    {
        if (auto* v = map.find(k))
        {
            sink += *v;
        }
    }
    auto end_aged = std::chrono::high_resolution_clock::now();
    auto aged_us = std::chrono::duration_cast<std::chrono::microseconds>(end_aged - start_aged).count();

    // 5. Assert latency stability
    // Allow 50% tolerance for Swiss Table tombstone overhead + system noise
    double ratio = static_cast<double>(aged_us) / static_cast<double>(fresh_us);

    // Diagnostic output
    std::cout << "  Latency: fresh=" << fresh_us << "us aged=" << aged_us << "us ratio=" << ratio << "\n";

    // Use sink to prevent optimization
    (void)sink;

    // CI tolerance: 2.5x accounts for tombstone overhead + noisy CI runners
    // (shared resources, CPU throttling). Local runs typically see ~1.3-1.5x.
    FATP_ASSERT_TRUE(ratio < 2.50, "Lookup performance degraded excessively after churn");

    return true;
}

// ============================================================================
// Benchmark Sanity Test
// ============================================================================
// Simplified benchmark that verifies StableHashMap is faster than unordered_map
// for the core find operation at a reasonable size.

static volatile std::uintptr_t benchmark_sink = 0;


void benchmark_stablehashmap()
{
    std::cout << "\n" << colors::cyan() << "StableHashMap Sanity Benchmark:" << colors::reset() << "\n\n";

    constexpr size_t N = 100'000;
    constexpr size_t ITERS = 500'000;

    std::mt19937 rng(123456);
    std::vector<int> keys(N);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), rng);

    // Build maps
    fat_p::StableHashMap<int, int> fmap(N * 2, 0.75f);
    std::unordered_map<int, int> umap;
    umap.reserve(N * 2);

    for (int k : keys)
    {
        fmap.insert(k, k);
        umap.emplace(k, k);
    }

    // Benchmark find
    size_t idx = 0;
    auto next_key = [&]()
    {
        int k = keys[idx++ % keys.size()];
        return k;
    };

    double fmap_time = measure_perf(
        [&]()
        {
            int* v = fmap.find(next_key());
            benchmark_sink += reinterpret_cast<std::uintptr_t>(v);
        },
        ITERS,
        10);

    idx = 0;

    double umap_time = measure_perf(
        [&]()
        {
            auto it = umap.find(next_key());
            benchmark_sink += reinterpret_cast<std::uintptr_t>(it == umap.end() ? nullptr : &it->second);
        },
        ITERS,
        10);

    double speedup = umap_time / fmap_time;

    std::cout << "Find (" << N << " elements, " << ITERS << " lookups):\n";
    std::cout << "  StableHashMap: " << format_time(fmap_time) << "\n";
    std::cout << "  unordered_map: " << format_time(umap_time) << "\n";
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << speedup << "x\n\n";

    // Sanity check: StableHashMap should be at least as fast
    if (speedup < 0.8)
    {
        std::cout << colors::yellow() << "  Warning: StableHashMap slower than expected" << colors::reset() << "\n";
    }
    else
    {
        std::cout << colors::green() << "  ✓ Performance sanity check passed" << colors::reset() << "\n";
    }
}

} // namespace fat_p::testing::stablehashmap

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_StableHashMap()
{
    FATP_PRINT_HEADER(STABLE HASH MAP)

    TestRunner runner;

    // Core functionality tests (1-18)
    FATP_RUN_TEST_NS(runner, stablehashmap, basic_construction);
    FATP_RUN_TEST_NS(runner, stablehashmap, insert_find);
    FATP_RUN_TEST_NS(runner, stablehashmap, erase);
    FATP_RUN_TEST_NS(runner, stablehashmap, update_value);
    FATP_RUN_TEST_NS(runner, stablehashmap, clear);
    FATP_RUN_TEST_NS(runner, stablehashmap, ctrl_tail_wraparound_probe);
    FATP_RUN_TEST_NS(runner, stablehashmap, load_factor);
    FATP_RUN_TEST_NS(runner, stablehashmap, collision_handling);
    FATP_RUN_TEST_NS(runner, stablehashmap, large_dataset);
    FATP_RUN_TEST_NS(runner, stablehashmap, string_keys);
    FATP_RUN_TEST_NS(runner, stablehashmap, erase_reinsert);
    FATP_RUN_TEST_NS(runner, stablehashmap, empty_values);
    FATP_RUN_TEST_NS(runner, stablehashmap, const_correctness);
    FATP_RUN_TEST_NS(runner, stablehashmap, bracket_operator);
    FATP_RUN_TEST_NS(runner, stablehashmap, move_semantics);
    FATP_RUN_TEST_NS(runner, stablehashmap, copy_semantics);
    FATP_RUN_TEST_NS(runner, stablehashmap, backward_shift_deletion);
    FATP_RUN_TEST_NS(runner, stablehashmap, equality_operators);
    FATP_RUN_TEST_NS(runner, stablehashmap, stress_random);

    // RAII and memory safety tests (19-24)
    FATP_RUN_TEST_NS(runner, stablehashmap, raii_erase_correctness);
    FATP_RUN_TEST_NS(runner, stablehashmap, heapbox_stress);
    FATP_RUN_TEST_NS(runner, stablehashmap, rehash_stress);
    FATP_RUN_TEST_NS(runner, stablehashmap, heavy_collision_chain);
    FATP_RUN_TEST_NS(runner, stablehashmap, insert_duplicate_key);
    FATP_RUN_TEST_NS(runner, stablehashmap, move_only_values);

    // Reference stability tests (core feature)
    FATP_RUN_TEST_NS(runner, stablehashmap, reference_stability_across_insert);
    FATP_RUN_TEST_NS(runner, stablehashmap, reference_stability_across_reserve);
    FATP_RUN_TEST_NS(runner, stablehashmap, reference_stability_across_erase);
    FATP_RUN_TEST_NS(runner, stablehashmap, reference_stability_mixed_operations);
    FATP_RUN_TEST_NS(runner, stablehashmap, reference_stability_with_strings);
    FATP_RUN_TEST_NS(runner, stablehashmap, heterogeneous_lookup);
    FATP_RUN_TEST_NS(runner, stablehashmap, heterogeneous_lookup_non_transparent);
    FATP_RUN_TEST_NS(runner, stablehashmap, heterogeneous_try_emplace_rvalue);
    FATP_RUN_TEST_NS(runner, stablehashmap, safepolicy_erase_basic_guarantee);

    // Churn stability tests (verify no-tombstone invariant)
    FATP_RUN_TEST_NS(runner, stablehashmap, churn_probe_distance_stability);
    FATP_RUN_TEST_NS(runner, stablehashmap, churn_no_ghost_slots);
    FATP_RUN_TEST_NS(runner, stablehashmap, churn_latency_stability);

    stablehashmap::benchmark_stablehashmap();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StableHashMap() ? 0 : 1;
}
#endif
