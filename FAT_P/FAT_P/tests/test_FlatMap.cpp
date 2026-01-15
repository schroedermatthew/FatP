/**
 * @file test_FlatMap.cpp
 * @brief Comprehensive unit tests for FlatMap.h
 */
/*
FATP_META:
  meta_version: 1
  component: FlatMap
  file_role: test
  path: tests/test_FlatMap.cpp
  namespace: fat_p
  summary: "Unit tests for FlatMap."
  related:
    docs_search: "FlatMap"
    headers:
      - fat_p/FlatMap.h
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

#include <atomic>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "FatPTest.h"
#include "FlatMap.h"

namespace fat_p::testing::flatmap
{

// ============================================================================
// Helper Types
// ============================================================================

/// Tracks constructor/destructor calls for RAII verification
class LifecycleTracker
{
public:
    static inline std::atomic<int> construct_count{0};
    static inline std::atomic<int> destruct_count{0};
    static inline std::atomic<int> copy_count{0};
    static inline std::atomic<int> move_count{0};

    int value;

    explicit LifecycleTracker(int v = 0)
        : value(v)
    {
        ++construct_count;
    }

    LifecycleTracker(const LifecycleTracker& other)
        : value(other.value)
    {
        ++construct_count;
        ++copy_count;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : value(other.value)
    {
        ++construct_count;
        ++move_count;
        other.value = -1;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other)
    {
        value = other.value;
        ++copy_count;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        value = other.value;
        ++move_count;
        other.value = -1;
        return *this;
    }

    ~LifecycleTracker()
    {
        ++destruct_count;
    }

    static void reset()
    {
        construct_count = 0;
        destruct_count = 0;
        copy_count = 0;
        move_count = 0;
    }

    bool operator==(const LifecycleTracker& other) const
    {
        return value == other.value;
    }
};

/// Type that throws on copy after N operations
struct ThrowOnCopy
{
    int value;
    static inline int throw_after = -1;
    static inline int operation_count = 0;

    explicit ThrowOnCopy(int v = 0)
        : value(v)
    {
    }

    ThrowOnCopy(const ThrowOnCopy& other)
        : value(other.value)
    {
        if (throw_after >= 0 && ++operation_count >= throw_after)
        {
            throw std::runtime_error("ThrowOnCopy: copy threw");
        }
    }

    ThrowOnCopy(ThrowOnCopy&& other) noexcept
        : value(other.value)
    {
        other.value = -1;
    }

    ThrowOnCopy& operator=(const ThrowOnCopy& other)
    {
        if (throw_after >= 0 && ++operation_count >= throw_after)
        {
            throw std::runtime_error("ThrowOnCopy: copy assignment threw");
        }
        value = other.value;
        return *this;
    }

    ThrowOnCopy& operator=(ThrowOnCopy&& other) noexcept
    {
        value = other.value;
        other.value = -1;
        return *this;
    }

    static void reset()
    {
        operation_count = 0;
        throw_after = -1;
    }

    bool operator<(const ThrowOnCopy& other) const
    {
        return value < other.value;
    }
    bool operator==(const ThrowOnCopy& other) const
    {
        return value == other.value;
    }
};

/// Move-only type for testing move semantics
struct MoveOnly
{
    std::unique_ptr<int> data;

    explicit MoveOnly(int v = 0)
        : data(std::make_unique<int>(v))
    {
    }
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    int get() const
    {
        return data ? *data : -1;
    }
    bool operator<(const MoveOnly& other) const
    {
        return get() < other.get();
    }
    bool operator==(const MoveOnly& other) const
    {
        return get() == other.get();
    }
};

// ============================================================================
// Constructor Tests
// ============================================================================

FATP_TEST_CASE(default_constructor)
{
    fat_p::FlatMap<int, std::string> map;

    FATP_ASSERT_TRUE(map.empty(), "Default constructed map should be empty");
    FATP_ASSERT_EQ(map.size(), size_t(0), "Size should be 0");
    FATP_ASSERT_TRUE(map.begin() == map.end(), "begin() should equal end()");

    return true;
}

FATP_TEST_CASE(comparator_constructor)
{
    fat_p::FlatMap<int, std::string, std::greater<int>> map(std::greater<int>{});

    map.insert({1, "one"});
    map.insert({2, "two"});
    map.insert({3, "three"});

    std::vector<int> keys;
    for (const auto& kv : map)
    {
        keys.push_back(kv.first);
    }

    FATP_ASSERT_EQ(keys.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_EQ(keys[0], 3, "First should be 3 (descending)");
    FATP_ASSERT_EQ(keys[1], 2, "Second should be 2");
    FATP_ASSERT_EQ(keys[2], 1, "Third should be 1");

    return true;
}

FATP_TEST_CASE(allocator_constructor)
{
    std::allocator<std::pair<const int, std::string>> alloc;
    fat_p::FlatMap<int, std::string> map(alloc);

    FATP_ASSERT_TRUE(map.empty(), "Allocator-constructed map should be empty");

    map.insert({1, "one"});
    FATP_ASSERT_EQ(map.size(), size_t(1), "Should have 1 element after insert");

    return true;
}

FATP_TEST_CASE(comparator_allocator_constructor)
{
    std::allocator<std::pair<const int, std::string>> alloc;
    fat_p::FlatMap<int, std::string, std::greater<int>> map(std::greater<int>{}, alloc);

    map.insert({1, "one"});
    map.insert({2, "two"});

    auto it = map.begin();
    FATP_ASSERT_EQ(it->first, 2, "First element should be 2 (descending order)");

    return true;
}

FATP_TEST_CASE(range_constructor)
{
    std::vector<std::pair<int, std::string>> data = {
        {3, "three"},
        {1, "one"},
        {2, "two"},
        {1, "ONE"} // Note: duplicate key
    };

    fat_p::FlatMap<int, std::string> map(data.begin(), data.end());

    FATP_ASSERT_EQ(map.size(), size_t(3), "Should have 3 unique keys");
    FATP_ASSERT_EQ(map.at(1), std::string("one"), "First occurrence should be kept");
    FATP_ASSERT_EQ(map.at(2), std::string("two"), "Key 2 should exist");
    FATP_ASSERT_EQ(map.at(3), std::string("three"), "Key 3 should exist");

    // Verify sorted order
    std::vector<int> keys;
    for (const auto& kv : map)
    {
        keys.push_back(kv.first);
    }
    FATP_ASSERT_TRUE(std::is_sorted(keys.begin(), keys.end()), "Keys should be sorted");

    return true;
}

FATP_TEST_CASE(initializer_list_constructor)
{
    fat_p::FlatMap<int, std::string> map{{3, "three"}, {1, "one"}, {2, "two"}};

    FATP_ASSERT_EQ(map.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_EQ(map.at(1), std::string("one"), "Key 1 should map to 'one'");

    return true;
}

// ============================================================================
// Copy/Move Semantics Tests
// ============================================================================

FATP_TEST_CASE(copy_constructor)
{
    fat_p::FlatMap<int, std::string> original{{1, "one"}, {2, "two"}, {3, "three"}};

    fat_p::FlatMap<int, std::string> copy(original);

    FATP_ASSERT_EQ(copy.size(), original.size(), "Copy should have same size");
    FATP_ASSERT_EQ(copy.at(1), std::string("one"), "Copy should have same values");
    FATP_ASSERT_EQ(copy.at(2), std::string("two"), "Copy should have same values");
    FATP_ASSERT_EQ(copy.at(3), std::string("three"), "Copy should have same values");

    // Modify copy, original unchanged
    copy[1] = "ONE";
    FATP_ASSERT_EQ(original.at(1), std::string("one"), "Original should be unchanged");
    FATP_ASSERT_EQ(copy.at(1), std::string("ONE"), "Copy should be modified");

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    fat_p::FlatMap<int, std::string> original{{1, "one"}, {2, "two"}};
    fat_p::FlatMap<int, std::string> copy{{99, "ninety-nine"}};

    copy = original;

    FATP_ASSERT_EQ(copy.size(), size_t(2), "Copy should have 2 elements");
    FATP_ASSERT_FALSE(copy.contains(99), "Old content should be gone");
    FATP_ASSERT_EQ(copy.at(1), std::string("one"), "Should have copied content");

    return true;
}

FATP_TEST_CASE(self_assignment)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};

    auto* ptr = &map;
    *ptr = map; // Self-assignment

    FATP_ASSERT_EQ(map.size(), size_t(2), "Size should be unchanged after self-assignment");
    FATP_ASSERT_EQ(map.at(1), std::string("one"), "Content should be unchanged");
    FATP_ASSERT_EQ(map.at(2), std::string("two"), "Content should be unchanged");

    return true;
}

FATP_TEST_CASE(move_constructor)
{
    fat_p::FlatMap<int, std::string> original{{1, "one"}, {2, "two"}};

    fat_p::FlatMap<int, std::string> moved(std::move(original));

    FATP_ASSERT_EQ(moved.size(), size_t(2), "Moved-to map should have 2 elements");
    FATP_ASSERT_EQ(moved.at(1), std::string("one"), "Moved-to map should have correct values");
    FATP_ASSERT_EQ(moved.at(2), std::string("two"), "Moved-to map should have correct values");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    fat_p::FlatMap<int, std::string> original{{1, "one"}, {2, "two"}};
    fat_p::FlatMap<int, std::string> target{{99, "ninety-nine"}};

    target = std::move(original);

    FATP_ASSERT_EQ(target.size(), size_t(2), "Target should have 2 elements");
    FATP_ASSERT_FALSE(target.contains(99), "Old content should be gone");
    FATP_ASSERT_EQ(target.at(1), std::string("one"), "Should have moved content");

    return true;
}

// ============================================================================
// Basic Operations Tests
// ============================================================================

FATP_TEST_CASE(basic_operations)
{
    fat_p::FlatMap<int, std::string> map;

    FATP_ASSERT_TRUE(map.empty(), "New map should be empty");
    FATP_ASSERT_EQ(map.size(), size_t(0), "Size should be 0");

    map.insert({1, "one"});
    map.insert({2, "two"});
    map.insert({3, "three"});

    FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3");
    FATP_ASSERT_FALSE(map.empty(), "Map should not be empty");

    return true;
}

FATP_TEST_CASE(single_element)
{
    fat_p::FlatMap<int, std::string> map;

    map.insert({42, "answer"});

    FATP_ASSERT_EQ(map.size(), size_t(1), "Size should be 1");
    FATP_ASSERT_FALSE(map.empty(), "Map should not be empty");
    FATP_ASSERT_TRUE(map.contains(42), "Should contain the key");
    FATP_ASSERT_EQ(map.at(42), std::string("answer"), "Value should match");

    // Iterator operations on single element
    FATP_ASSERT_EQ(map.begin()->first, 42, "begin() should point to the element");
    FATP_ASSERT_EQ(std::distance(map.begin(), map.end()), 1, "Should have exactly 1 element");

    // Erase single element
    map.erase(42);
    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after erasing only element");

    return true;
}

FATP_TEST_CASE(find_operations)
{
    fat_p::FlatMap<std::string, int> map{{"one", 1}, {"two", 2}, {"three", 3}};

    auto it = map.find("two");
    FATP_ASSERT_TRUE(it != map.end(), "Should find 'two'");
    FATP_ASSERT_EQ(it->second, 2, "Value should be 2");

    it = map.find("nonexistent");
    FATP_ASSERT_TRUE(it == map.end(), "Should not find nonexistent key");

    FATP_ASSERT_TRUE(map.contains("one"), "Should contain 'one'");
    FATP_ASSERT_FALSE(map.contains("four"), "Should not contain 'four'");

    return true;
}

FATP_TEST_CASE(count)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    FATP_ASSERT_EQ(map.count(1), size_t(1), "count(1) should return 1");
    FATP_ASSERT_EQ(map.count(2), size_t(1), "count(2) should return 1");
    FATP_ASSERT_EQ(map.count(99), size_t(0), "count(99) should return 0");

    return true;
}

FATP_TEST_CASE(operator_bracket)
{
    fat_p::FlatMap<int, std::string> map;

    map[1] = "one";
    map[2] = "two";

    FATP_ASSERT_EQ(map[1], std::string("one"), "Value should be 'one'");
    FATP_ASSERT_EQ(map[2], std::string("two"), "Value should be 'two'");

    map[1] = "ONE";
    FATP_ASSERT_EQ(map[1], std::string("ONE"), "Value should be updated");

    // operator[] with non-existent key creates default value
    std::string& ref = map[99];
    FATP_ASSERT_EQ(ref, std::string(""), "New key should have default value");
    FATP_ASSERT_TRUE(map.contains(99), "Key should now exist");

    return true;
}

FATP_TEST_CASE(at_method)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};

    FATP_ASSERT_EQ(map.at(1), std::string("one"), "at(1) should return 'one'");

    // Test const version
    const auto& constMap = map;
    FATP_ASSERT_EQ(constMap.at(2), std::string("two"), "const at(2) should return 'two'");

    FATP_ASSERT_THROWS(map.at(999), std::out_of_range, "at() should throw for nonexistent key");
    FATP_ASSERT_THROWS(constMap.at(999), std::out_of_range, "const at() should throw for nonexistent key");

    return true;
}

FATP_TEST_CASE(insert_or_assign)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}};

    auto [it, inserted] = map.insert_or_assign(1, "ONE");
    FATP_ASSERT_FALSE(inserted, "Should not insert, key exists");
    FATP_ASSERT_EQ(it->second, std::string("ONE"), "Value should be updated");

    auto [it2, inserted2] = map.insert_or_assign(2, "two");
    FATP_ASSERT_TRUE(inserted2, "Should insert new key");
    FATP_ASSERT_EQ(it2->second, std::string("two"), "New value should be correct");

    return true;
}

FATP_TEST_CASE(erase_by_key)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    size_t erased = map.erase(2);
    FATP_ASSERT_EQ(erased, size_t(1), "Should erase one element");
    FATP_ASSERT_EQ(map.size(), size_t(2), "Size should be 2");
    FATP_ASSERT_TRUE(map.find(2) == map.end(), "Key 2 should not be found");

    erased = map.erase(999);
    FATP_ASSERT_EQ(erased, size_t(0), "Should not erase nonexistent key");

    return true;
}

FATP_TEST_CASE(erase_by_iterator)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    auto it = map.find(2);
    FATP_ASSERT_TRUE(it != map.end(), "Should find key 2");

    auto next = map.erase(it);
    FATP_ASSERT_EQ(map.size(), size_t(2), "Size should be 2 after erase");
    FATP_ASSERT_FALSE(map.contains(2), "Key 2 should be gone");
    FATP_ASSERT_TRUE(next == map.find(3) || next == map.end(), "Iterator should point to next element or end");

    return true;
}

FATP_TEST_CASE(erase_range)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}};

    auto first = map.find(2);
    auto last = map.find(4);

    map.erase(first, last); // Erases 2 and 3

    FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3 after range erase");
    FATP_ASSERT_TRUE(map.contains(1), "Key 1 should remain");
    FATP_ASSERT_FALSE(map.contains(2), "Key 2 should be erased");
    FATP_ASSERT_FALSE(map.contains(3), "Key 3 should be erased");
    FATP_ASSERT_TRUE(map.contains(4), "Key 4 should remain");
    FATP_ASSERT_TRUE(map.contains(5), "Key 5 should remain");

    return true;
}

FATP_TEST_CASE(sorted_order)
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

    FATP_ASSERT_TRUE(std::is_sorted(keys.begin(), keys.end()), "Keys should be sorted");

    return true;
}

FATP_TEST_CASE(lower_upper_bound)
{
    fat_p::FlatMap<int, int> map{{1, 10}, {3, 30}, {5, 50}, {7, 70}};

    auto it = map.lower_bound(3);
    FATP_ASSERT_TRUE(it != map.end(), "Should find lower bound");
    FATP_ASSERT_EQ(it->first, 3, "Lower bound should be 3");

    it = map.lower_bound(4); // Key doesn't exist
    FATP_ASSERT_TRUE(it != map.end(), "Should find lower bound for non-existent key");
    FATP_ASSERT_EQ(it->first, 5, "Lower bound of 4 should be 5");

    it = map.upper_bound(3);
    FATP_ASSERT_TRUE(it != map.end(), "Should find upper bound");
    FATP_ASSERT_EQ(it->first, 5, "Upper bound should be 5");

    it = map.upper_bound(7);
    FATP_ASSERT_TRUE(it == map.end(), "Upper bound of max should be end");

    return true;
}

FATP_TEST_CASE(equal_range)
{
    fat_p::FlatMap<int, int> map{{1, 10}, {2, 20}, {3, 30}};

    auto [first, last] = map.equal_range(2);
    FATP_ASSERT_TRUE(first != map.end(), "Range should not be empty");
    FATP_ASSERT_EQ(first->first, 2, "First should be 2");

    size_t count = static_cast<size_t>(std::distance(first, last));
    FATP_ASSERT_EQ(count, size_t(1), "Should have exactly one element");

    // Non-existent key
    auto [first2, last2] = map.equal_range(99);
    FATP_ASSERT_TRUE(first2 == last2, "Range for non-existent key should be empty");

    return true;
}

FATP_TEST_CASE(clear)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};

    FATP_ASSERT_EQ(map.size(), size_t(2), "Size should be 2");

    map.clear();
    FATP_ASSERT_EQ(map.size(), size_t(0), "Size should be 0 after clear");
    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");
    FATP_ASSERT_TRUE(map.begin() == map.end(), "begin() should equal end() after clear");

    return true;
}

// ============================================================================
// Iterator Tests
// ============================================================================

FATP_TEST_CASE(iterator_basics)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    // Forward iteration
    std::vector<int> keys;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        keys.push_back(it->first);
    }
    FATP_ASSERT_EQ(keys.size(), size_t(3), "Should iterate over all elements");
    FATP_ASSERT_TRUE(std::is_sorted(keys.begin(), keys.end()), "Should iterate in sorted order");

    // Range-based for
    keys.clear();
    for (const auto& [k, v] : map)
    {
        keys.push_back(k);
    }
    FATP_ASSERT_EQ(keys.size(), size_t(3), "Range-based for should work");

    return true;
}

FATP_TEST_CASE(const_iterator)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};
    const auto& constMap = map;

    std::vector<int> keys;
    for (auto it = constMap.begin(); it != constMap.end(); ++it)
    {
        keys.push_back(it->first);
    }
    FATP_ASSERT_EQ(keys.size(), size_t(2), "Const iteration should work");

    // cbegin/cend
    keys.clear();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
    {
        keys.push_back(it->first);
    }
    FATP_ASSERT_EQ(keys.size(), size_t(2), "cbegin/cend should work");

    return true;
}

FATP_TEST_CASE(reverse_iterator)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    std::vector<int> keys;
    for (auto it = map.rbegin(); it != map.rend(); ++it)
    {
        keys.push_back(it->first);
    }

    FATP_ASSERT_EQ(keys.size(), size_t(3), "Should iterate over all elements");
    FATP_ASSERT_EQ(keys[0], 3, "First in reverse should be 3");
    FATP_ASSERT_EQ(keys[1], 2, "Second in reverse should be 2");
    FATP_ASSERT_EQ(keys[2], 1, "Third in reverse should be 1");

    // const reverse iterator
    const auto& constMap = map;
    keys.clear();
    for (auto it = constMap.rbegin(); it != constMap.rend(); ++it)
    {
        keys.push_back(it->first);
    }
    FATP_ASSERT_EQ(keys.size(), size_t(3), "Const reverse iteration should work");

    // crbegin/crend
    keys.clear();
    for (auto it = map.crbegin(); it != map.crend(); ++it)
    {
        keys.push_back(it->first);
    }
    FATP_ASSERT_EQ(keys.size(), size_t(3), "crbegin/crend should work");

    return true;
}

FATP_TEST_CASE(iterator_key_immutability)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}};

    auto it = map.begin();

    // Value should be mutable
    (*it).second = "ONE";
    FATP_ASSERT_EQ(map.at(1), std::string("ONE"), "Value should be mutable");

    // Key is const - this should not compile:
    // (*it).first = 99;  // Error: assignment of read-only member

    return true;
}

// ============================================================================
// Capacity Tests
// ============================================================================

FATP_TEST_CASE(reserve_capacity)
{
    fat_p::FlatMap<int, int> map;

    FATP_ASSERT_EQ(map.capacity(), size_t(0), "Initial capacity should be 0");

    map.reserve(100);
    FATP_ASSERT_GE(map.capacity(), size_t(100), "Capacity should be at least 100");
    FATP_ASSERT_EQ(map.size(), size_t(0), "Size should still be 0");

    for (int i = 0; i < 50; ++i)
    {
        map.insert({i, i * 10});
    }

    FATP_ASSERT_EQ(map.size(), size_t(50), "Size should be 50");
    FATP_ASSERT_GE(map.capacity(), size_t(100), "Capacity should still be at least 100");

    map.shrink_to_fit();
    FATP_ASSERT_GE(map.capacity(), map.size(), "Capacity should be at least size");

    return true;
}

FATP_TEST_CASE(max_size)
{
    fat_p::FlatMap<int, int> map;

    FATP_ASSERT_GT(map.max_size(), size_t(0), "max_size should be positive");
    FATP_ASSERT_GT(map.max_size(), size_t(1000000), "max_size should be large");

    return true;
}

// ============================================================================
// Observers Tests
// ============================================================================

FATP_TEST_CASE(key_comp)
{
    fat_p::FlatMap<int, std::string> map;
    auto comp = map.key_comp();

    FATP_ASSERT_TRUE(comp(1, 2), "1 < 2 should be true");
    FATP_ASSERT_FALSE(comp(2, 1), "2 < 1 should be false");
    FATP_ASSERT_FALSE(comp(1, 1), "1 < 1 should be false");

    // With custom comparator
    fat_p::FlatMap<int, std::string, std::greater<int>> descMap;
    auto descComp = descMap.key_comp();

    FATP_ASSERT_FALSE(descComp(1, 2), "1 > 2 should be false");
    FATP_ASSERT_TRUE(descComp(2, 1), "2 > 1 should be true");

    return true;
}

FATP_TEST_CASE(value_comp)
{
    fat_p::FlatMap<int, std::string> map;
    auto comp = map.value_comp();

    std::pair<const int, std::string> a{1, "one"};
    std::pair<const int, std::string> b{2, "two"};

    FATP_ASSERT_TRUE(comp(a, b), "(1, one) < (2, two) should be true");
    FATP_ASSERT_FALSE(comp(b, a), "(2, two) < (1, one) should be false");

    return true;
}

FATP_TEST_CASE(get_allocator)
{
    fat_p::FlatMap<int, std::string> map;
    auto alloc = map.get_allocator();

    // Just verify it compiles and returns something
    using AllocType = decltype(alloc);
    FATP_ASSERT_TRUE((std::is_same_v<AllocType, std::allocator<std::pair<const int, std::string>>>),
                     "Allocator type should match");

    return true;
}

// ============================================================================
// Comparator Tests
// ============================================================================

FATP_TEST_CASE(custom_comparator)
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

    FATP_ASSERT_EQ(keys.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_EQ(keys[0], 3, "First should be 3 (descending)");
    FATP_ASSERT_EQ(keys[1], 2, "Second should be 2");
    FATP_ASSERT_EQ(keys[2], 1, "Third should be 1");

    return true;
}

FATP_TEST_CASE(case_insensitive_comparator)
{
    struct CaseInsensitiveCompare
    {
        bool operator()(const std::string& a, const std::string& b) const
        {
            return std::lexicographical_compare(a.begin(),
                                                a.end(),
                                                b.begin(),
                                                b.end(),
                                                [](unsigned char c1, unsigned char c2)
                                                {
                                                    return std::tolower(c1) < std::tolower(c2);
                                                });
        }
    };

    fat_p::FlatMap<std::string, int, CaseInsensitiveCompare> map;

    map.insert({"Hello", 1});
    auto [it, inserted] = map.insert({"HELLO", 2});

    FATP_ASSERT_FALSE(inserted, "HELLO should be duplicate of Hello");
    FATP_ASSERT_EQ(map.size(), size_t(1), "Should have only 1 element");
    FATP_ASSERT_TRUE(map.contains("hello"), "Should find 'hello'");
    FATP_ASSERT_TRUE(map.contains("HELLO"), "Should find 'HELLO'");

    return true;
}

FATP_TEST_CASE(case_insensitive_constructor_keeps_first)
{
    struct CaseInsensitiveCompare
    {
        bool operator()(const std::string& a, const std::string& b) const
        {
            return std::lexicographical_compare(a.begin(),
                                                a.end(),
                                                b.begin(),
                                                b.end(),
                                                [](char c1, char c2)
                                                {
                                                    return std::tolower(static_cast<unsigned char>(c1)) <
                                                           std::tolower(static_cast<unsigned char>(c2));
                                                });
        }
    };

    std::vector<std::pair<std::string, int>> data = {{"Hello", 1},
                                                     {"HELLO", 2}, // Equivalent key, should be ignored
                                                     {"hello", 3}, // Equivalent key, should be ignored
                                                     {"World", 4}};

    fat_p::FlatMap<std::string, int, CaseInsensitiveCompare> map(data.begin(), data.end());

    FATP_ASSERT_EQ(map.size(), size_t(2), "Should have 2 unique keys under comparator");
    FATP_ASSERT_EQ(map.begin()->first,
                   std::string("Hello"),
                   "Constructor should keep the first inserted representative for equivalent keys");
    FATP_ASSERT_EQ(map.begin()->second, 1, "Should keep value from first equivalent key");

    auto it = map.find(std::string("HELLO"));
    FATP_ASSERT_TRUE(it != map.end(), "Should find equivalent key");
    FATP_ASSERT_EQ(it->second, 1, "Equivalent lookup should see first value");

    return true;
}

// ============================================================================
// Modifier Tests
// ============================================================================

FATP_TEST_CASE(equality_operators)
{
    fat_p::FlatMap<int, std::string> map1{{1, "one"}, {2, "two"}, {3, "three"}};
    fat_p::FlatMap<int, std::string> map2{{1, "one"}, {2, "two"}, {3, "three"}};
    fat_p::FlatMap<int, std::string> map3{{1, "one"}, {2, "TWO"}, {3, "three"}};
    fat_p::FlatMap<int, std::string> map4{{1, "one"}, {2, "two"}};

    FATP_ASSERT_TRUE(map1 == map2, "Identical maps should be equal");
    FATP_ASSERT_FALSE(map1 != map2, "Identical maps should not be not-equal");

    FATP_ASSERT_TRUE(map1 != map3, "Maps with different values should not be equal");
    FATP_ASSERT_TRUE(map1 != map4, "Maps with different sizes should not be equal");

    return true;
}

FATP_TEST_CASE(swap)
{
    fat_p::FlatMap<int, std::string> map1{{1, "one"}, {2, "two"}};
    fat_p::FlatMap<int, std::string> map2{{3, "three"}, {4, "four"}, {5, "five"}};

    map1.swap(map2);

    FATP_ASSERT_EQ(map1.size(), size_t(3), "map1 should have 3 elements after swap");
    FATP_ASSERT_EQ(map2.size(), size_t(2), "map2 should have 2 elements after swap");
    FATP_ASSERT_TRUE(map1.contains(3), "map1 should contain 3");
    FATP_ASSERT_TRUE(map2.contains(1), "map2 should contain 1");

    // ADL swap
    using std::swap;
    swap(map1, map2);
    FATP_ASSERT_EQ(map1.size(), size_t(2), "map1 should have 2 elements after ADL swap");
    FATP_ASSERT_EQ(map2.size(), size_t(3), "map2 should have 3 elements after ADL swap");

    return true;
}

FATP_TEST_CASE(try_emplace)
{
    fat_p::FlatMap<int, std::string> map;

    auto [it1, inserted1] = map.try_emplace(1, "one");
    FATP_ASSERT_TRUE(inserted1, "Should insert new key");
    FATP_ASSERT_EQ(it1->second, std::string("one"), "Value should be 'one'");

    auto [it2, inserted2] = map.try_emplace(1, "ONE");
    FATP_ASSERT_FALSE(inserted2, "Should not insert duplicate key");
    FATP_ASSERT_EQ(it2->second, std::string("one"), "Value should still be 'one'");

    // try_emplace with rvalue key
    auto [it3, inserted3] = map.try_emplace(2, "two");
    FATP_ASSERT_TRUE(inserted3, "Should insert with rvalue key");

    return true;
}

FATP_TEST_CASE(try_emplace_does_not_construct_on_hit)
{
    int constructed = 0;

    struct Counted
    {
        int* constructed = nullptr;
        int value = 0;

        Counted(int v, int* c)
            : constructed(c)
            , value(v)
        {
            ++(*constructed);
        }

        Counted(const Counted& other)
            : constructed(other.constructed)
            , value(other.value)
        {
            ++(*constructed);
        }

        Counted(Counted&& other) noexcept
            : constructed(other.constructed)
            , value(other.value)
        {
            ++(*constructed);
        }

        Counted& operator=(const Counted&) = default;
        Counted& operator=(Counted&&) noexcept = default;
    };

    fat_p::FlatMap<int, Counted> map;
    map.reserve(2);

    auto [it1, inserted1] = map.try_emplace(1, 1, &constructed);
    FATP_ASSERT_TRUE(inserted1, "Should insert new key");
    const int afterFirst = constructed;

    auto [it2, inserted2] = map.try_emplace(1, 2, &constructed);
    FATP_ASSERT_FALSE(inserted2, "Should not insert duplicate key");

    FATP_ASSERT_EQ(constructed, afterFirst, "try_emplace on existing key should not construct mapped_type");
    FATP_ASSERT_EQ(it2->second.value, 1, "Existing value should be preserved");

    return true;
}

FATP_TEST_CASE(emplace)
{
    fat_p::FlatMap<int, std::string> map;

    auto [it1, inserted1] = map.emplace(1, "one");
    FATP_ASSERT_TRUE(inserted1, "Should insert new element");
    FATP_ASSERT_EQ(it1->first, 1, "Key should be 1");
    FATP_ASSERT_EQ(it1->second, std::string("one"), "Value should be 'one'");

    auto [it2, inserted2] = map.emplace(1, "ONE");
    FATP_ASSERT_FALSE(inserted2, "Should not insert duplicate");
    FATP_ASSERT_EQ(it2->second, std::string("one"), "Value should be unchanged");

    return true;
}

FATP_TEST_CASE(emplace_hint)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {3, "three"}};

    auto hint = map.find(1);
    auto it = map.emplace_hint(hint, 2, "two");

    FATP_ASSERT_EQ(it->first, 2, "Should have inserted key 2");
    FATP_ASSERT_EQ(it->second, std::string("two"), "Value should be 'two'");
    FATP_ASSERT_EQ(map.size(), size_t(3), "Size should be 3");

    return true;
}

FATP_TEST_CASE(range_insert)
{
    std::vector<std::pair<int, std::string>> data = {{5, "five"}, {1, "one"}, {3, "three"}, {2, "two"}, {4, "four"}};

    fat_p::FlatMap<int, std::string> map;
    map.insert(data.begin(), data.end());

    FATP_ASSERT_EQ(map.size(), size_t(5), "Should have 5 elements");

    std::vector<int> keys;
    for (const auto& kv : map)
    {
        keys.push_back(kv.first);
    }
    FATP_ASSERT_TRUE(std::is_sorted(keys.begin(), keys.end()), "Keys should be sorted");

    return true;
}

FATP_TEST_CASE(initializer_list_insert)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}};

    map.insert({{2, "two"}, {3, "three"}, {1, "ONE"}}); // 1 is duplicate

    FATP_ASSERT_EQ(map.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_EQ(map.at(1), std::string("one"), "Duplicate should keep original value");
    FATP_ASSERT_EQ(map.at(2), std::string("two"), "New key should be inserted");
    FATP_ASSERT_EQ(map.at(3), std::string("three"), "New key should be inserted");

    return true;
}

FATP_TEST_CASE(extract)
{
    fat_p::FlatMap<int, std::string> map{{1, "one"}, {2, "two"}, {3, "three"}};

    auto it = map.find(2);
    FATP_ASSERT_TRUE(it != map.end(), "Should find key 2");

    auto extracted = map.extract(it);
    FATP_ASSERT_EQ(extracted.first, 2, "Extracted key should be 2");
    FATP_ASSERT_EQ(extracted.second, std::string("two"), "Extracted value should be 'two'");
    FATP_ASSERT_EQ(map.size(), size_t(2), "Map should have 2 elements after extract");
    FATP_ASSERT_FALSE(map.contains(2), "Map should not contain key 2 after extract");

    return true;
}

FATP_TEST_CASE(empty_operations)
{
    fat_p::FlatMap<int, int> map;

    FATP_ASSERT_TRUE(map.find(1) == map.end(), "find on empty map should return end");
    FATP_ASSERT_FALSE(map.contains(1), "contains on empty map should return false");
    FATP_ASSERT_EQ(map.count(1), size_t(0), "count on empty map should return 0");
    FATP_ASSERT_TRUE(map.lower_bound(1) == map.end(), "lower_bound on empty map should return end");
    FATP_ASSERT_TRUE(map.upper_bound(1) == map.end(), "upper_bound on empty map should return end");

    auto [first, last] = map.equal_range(1);
    FATP_ASSERT_TRUE(first == last, "equal_range on empty map should return empty range");

    FATP_ASSERT_EQ(map.erase(1), size_t(0), "erase nonexistent key should return 0");

    // Clear on empty should be safe
    map.clear();
    FATP_ASSERT_TRUE(map.empty(), "clear on empty should leave map empty");

    return true;
}

FATP_TEST_CASE(heterogeneous_lookup)
{
    // Use std::less<> for transparent comparison
    fat_p::FlatMap<std::string, int, std::less<>> map;

    map.insert({"apple", 1});
    map.insert({"banana", 2});
    map.insert({"cherry", 3});

    // These lookups should NOT create temporary std::string objects
    auto it = map.find("banana");
    FATP_ASSERT_TRUE(it != map.end(), "find with const char* should work");
    FATP_ASSERT_EQ(it->second, 2, "find should return correct value");

    FATP_ASSERT_TRUE(map.contains("apple"), "contains with const char* should work");
    FATP_ASSERT_FALSE(map.contains("grape"), "contains should return false for missing key");

    FATP_ASSERT_EQ(map.count("cherry"), size_t(1), "count with const char* should work");
    FATP_ASSERT_EQ(map.count("grape"), size_t(0), "count should return 0 for missing key");

    auto lb = map.lower_bound("banana");
    FATP_ASSERT_TRUE(lb != map.end(), "lower_bound should find element");
    FATP_ASSERT_EQ(lb->first, std::string("banana"), "lower_bound with const char* should work");

    auto ub = map.upper_bound("banana");
    FATP_ASSERT_TRUE(ub != map.end(), "upper_bound should find next element");
    FATP_ASSERT_EQ(ub->first, std::string("cherry"), "upper_bound with const char* should work");

    auto [first, last] = map.equal_range("banana");
    FATP_ASSERT_TRUE(first != last, "equal_range should find element");
    FATP_ASSERT_EQ(first->first, std::string("banana"), "equal_range should return correct element");

    return true;
}

FATP_TEST_CASE(merge)
{
    fat_p::FlatMap<int, std::string> map1;
    map1.insert({1, "one"});
    map1.insert({3, "three"});
    map1.insert({5, "five"});

    fat_p::FlatMap<int, std::string> map2;
    map2.insert({2, "two"});
    map2.insert({3, "THREE"}); // Duplicate key - should keep map1's value
    map2.insert({4, "four"});

    map1.merge(map2);

    FATP_ASSERT_EQ(map1.size(), size_t(5), "Merged map should have 5 elements");

    // Key 3 was duplicate - it should remain in source
    FATP_ASSERT_EQ(map2.size(), size_t(1), "Source should have 1 element (the duplicate)");
    FATP_ASSERT_TRUE(map2.contains(3), "Source should still contain duplicate key 3");
    FATP_ASSERT_EQ(map2.at(3), std::string("THREE"), "Duplicate should retain its value");

    FATP_ASSERT_EQ(map1.at(1), std::string("one"), "Element 1 should be preserved");
    FATP_ASSERT_EQ(map1.at(2), std::string("two"), "Element 2 should be merged");
    FATP_ASSERT_EQ(map1.at(3), std::string("three"), "Duplicate key should keep original value");
    FATP_ASSERT_EQ(map1.at(4), std::string("four"), "Element 4 should be merged");
    FATP_ASSERT_EQ(map1.at(5), std::string("five"), "Element 5 should be preserved");

    // Verify sorted order
    int prev = -1;
    for (const auto& [k, v] : map1)
    {
        FATP_ASSERT_GT(k, prev, "Merged map should maintain sorted order");
        prev = k;
    }

    // Test merge with empty source
    fat_p::FlatMap<int, std::string> empty;
    size_t sizeBefore = map1.size();
    map1.merge(empty);
    FATP_ASSERT_EQ(map1.size(), sizeBefore, "Merging empty map should not change size");

    // Test merge into empty target
    fat_p::FlatMap<int, std::string> target;
    fat_p::FlatMap<int, std::string> source;
    source.insert({10, "ten"});
    source.insert({20, "twenty"});
    target.merge(source);
    FATP_ASSERT_EQ(target.size(), size_t(2), "Merge into empty should work");
    FATP_ASSERT_TRUE(source.empty(), "Source should be empty after merge (no duplicates)");

    // Self-merge should be a no-op (and must not crash)
    const size_t selfSizeBefore = map1.size();
    map1.merge(map1);
    FATP_ASSERT_EQ(map1.size(), selfSizeBefore, "Self-merge should not change size");

    return true;
}

// ============================================================================
// RAII and Lifecycle Tests
// ============================================================================

FATP_TEST_CASE(lifecycle_tracking)
{
    LifecycleTracker::reset();

    {
        fat_p::FlatMap<int, LifecycleTracker> map;
        map.emplace(1, LifecycleTracker(10));
        map.emplace(2, LifecycleTracker(20));
        map.emplace(3, LifecycleTracker(30));

        FATP_ASSERT_EQ(map.size(), size_t(3), "Should have 3 elements");
    }

    // After scope ends, all elements should be destroyed
    FATP_ASSERT_EQ(LifecycleTracker::construct_count,
                   LifecycleTracker::destruct_count,
                   "All constructed objects should be destroyed");

    return true;
}

FATP_TEST_CASE(lifecycle_on_clear)
{
    LifecycleTracker::reset();

    fat_p::FlatMap<int, LifecycleTracker> map;
    map.emplace(1, LifecycleTracker(10));
    map.emplace(2, LifecycleTracker(20));

    int destructedBefore = LifecycleTracker::destruct_count;

    map.clear();

    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");
    FATP_ASSERT_GT(LifecycleTracker::destruct_count, destructedBefore, "Clear should destroy elements");

    return true;
}

FATP_TEST_CASE(lifecycle_on_erase)
{
    LifecycleTracker::reset();

    fat_p::FlatMap<int, LifecycleTracker> map;
    map.emplace(1, LifecycleTracker(10));
    map.emplace(2, LifecycleTracker(20));
    map.emplace(3, LifecycleTracker(30));

    int destructedBefore = LifecycleTracker::destruct_count;

    map.erase(2);

    FATP_ASSERT_EQ(map.size(), size_t(2), "Map should have 2 elements");
    FATP_ASSERT_GT(LifecycleTracker::destruct_count, destructedBefore, "Erase should destroy element");

    return true;
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

FATP_TEST_CASE(exception_safety_insert)
{
    ThrowOnCopy::reset();

    fat_p::FlatMap<int, ThrowOnCopy> map;
    map.emplace(1, ThrowOnCopy(10));
    map.emplace(2, ThrowOnCopy(20));

    size_t sizeBefore = map.size();

    ThrowOnCopy::reset();
    ThrowOnCopy::throw_after = 1; // Throw on first copy

    bool threw = false;
    try
    {
        ThrowOnCopy value(30);
        map.insert({3, value}); // This should throw during copy
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown");
    // Basic guarantee: map is still valid
    FATP_ASSERT_GE(map.size(), sizeBefore - 1, "Map should remain valid after exception");

    ThrowOnCopy::reset();

    return true;
}

// ============================================================================
// Move-Only Type Tests
// ============================================================================

FATP_TEST_CASE(move_only_values)
{
    fat_p::FlatMap<int, MoveOnly> map;

    map.emplace(1, MoveOnly(10));
    map.emplace(2, MoveOnly(20));
    map.emplace(3, MoveOnly(30));

    FATP_ASSERT_EQ(map.size(), size_t(3), "Should have 3 elements");

    auto it = map.find(2);
    FATP_ASSERT_TRUE(it != map.end(), "Should find key 2");
    FATP_ASSERT_EQ(it->second.get(), 20, "Value should be 20");

    // Test move construction
    fat_p::FlatMap<int, MoveOnly> map2(std::move(map));
    FATP_ASSERT_EQ(map2.size(), size_t(3), "Moved map should have 3 elements");

    return true;
}

// ============================================================================
// Stress/Fuzz Tests
// ============================================================================

FATP_TEST_CASE(stress_random_operations)
{
    fat_p::FlatMap<int, int> container;
    std::map<int, int> reference;

    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> keyDist(0, 999);
    std::uniform_int_distribution<int> opDist(0, 2);

    for (int i = 0; i < 5000; ++i)
    {
        int key = keyDist(rng);
        int op = opDist(rng);

        if (op == 0)
        {
            container.insert({key, i});
            reference.insert({key, i});
        }
        else if (op == 1)
        {
            bool ours = container.find(key) != container.end();
            bool theirs = reference.find(key) != reference.end();
            FATP_ASSERT_EQ(ours, theirs, "Find results should match");
        }
        else
        {
            size_t ours = container.erase(key);
            size_t theirs = reference.erase(key);
            FATP_ASSERT_EQ(ours, theirs, "Erase results should match");
        }
    }

    FATP_ASSERT_EQ(container.size(), reference.size(), "Final size should match");

    return true;
}

FATP_TEST_CASE(stress_comprehensive)
{
    fat_p::FlatMap<int, int> container;
    std::map<int, int> reference;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> keyDist(0, 499);
    std::uniform_int_distribution<int> opDist(0, 9);

    for (int i = 0; i < 10000; ++i)
    {
        int key = keyDist(rng);
        int op = opDist(rng);

        switch (op)
        {
            case 0: // insert
            {
                auto [it1, ins1] = container.insert({key, i});
                auto [it2, ins2] = reference.insert({key, i});
                FATP_ASSERT_EQ(ins1, ins2, "insert result mismatch");
                break;
            }

            case 1: // find
            {
                bool ours = container.find(key) != container.end();
                bool theirs = reference.find(key) != reference.end();
                FATP_ASSERT_EQ(ours, theirs, "find mismatch");
                break;
            }

            case 2: // erase
            {
                size_t ours = container.erase(key);
                size_t theirs = reference.erase(key);
                FATP_ASSERT_EQ(ours, theirs, "erase mismatch");
                break;
            }

            case 3: // operator[]
            {
                container[key] = i;
                reference[key] = i;
                break;
            }

            case 4: // contains/count
            {
                bool ours = container.contains(key);
                bool theirs = reference.count(key) > 0;
                FATP_ASSERT_EQ(ours, theirs, "contains mismatch");

                size_t oursCount = container.count(key);
                size_t theirsCount = reference.count(key);
                FATP_ASSERT_EQ(oursCount, theirsCount, "count mismatch");
                break;
            }

            case 5: // insert_or_assign
            {
                auto [it1, ins1] = container.insert_or_assign(key, i);
                auto [it2, ins2] = reference.insert_or_assign(key, i);
                FATP_ASSERT_EQ(ins1, ins2, "insert_or_assign mismatch");
                break;
            }

            case 6: // try_emplace
            {
                auto [it1, ins1] = container.try_emplace(key, i);
                auto [it2, ins2] = reference.try_emplace(key, i);
                FATP_ASSERT_EQ(ins1, ins2, "try_emplace mismatch");
                break;
            }

            case 7: // lower_bound
            {
                auto ours = container.lower_bound(key);
                auto theirs = reference.lower_bound(key);
                bool oursEnd = (ours == container.end());
                bool theirsEnd = (theirs == reference.end());
                FATP_ASSERT_EQ(oursEnd, theirsEnd, "lower_bound end mismatch");
                if (!oursEnd && !theirsEnd)
                {
                    FATP_ASSERT_EQ(ours->first, theirs->first, "lower_bound key mismatch");
                }
                break;
            }

            case 8: // upper_bound
            {
                auto ours = container.upper_bound(key);
                auto theirs = reference.upper_bound(key);
                bool oursEnd = (ours == container.end());
                bool theirsEnd = (theirs == reference.end());
                FATP_ASSERT_EQ(oursEnd, theirsEnd, "upper_bound end mismatch");
                if (!oursEnd && !theirsEnd)
                {
                    FATP_ASSERT_EQ(ours->first, theirs->first, "upper_bound key mismatch");
                }
                break;
            }

            case 9: // at (with exception check)
            {
                bool oursThrew = false, theirsThrew = false;
                int oursVal = 0, theirsVal = 0;
                try
                {
                    oursVal = container.at(key);
                }
                catch (const std::out_of_range&)
                {
                    oursThrew = true;
                }
                try
                {
                    theirsVal = reference.at(key);
                }
                catch (const std::out_of_range&)
                {
                    theirsThrew = true;
                }
                FATP_ASSERT_EQ(oursThrew, theirsThrew, "at throw mismatch");
                if (!oursThrew)
                {
                    FATP_ASSERT_EQ(oursVal, theirsVal, "at value mismatch");
                }
                break;
            }
        }
    }

    // Final state verification
    FATP_ASSERT_EQ(container.size(), reference.size(), "Final size mismatch");

    // Verify all elements match
    for (const auto& [k, v] : reference)
    {
        FATP_ASSERT_TRUE(container.contains(k), "Missing key in container");
        FATP_ASSERT_EQ(container.at(k), v, "Value mismatch");
    }

    // Verify iteration order matches
    auto ourIt = container.begin();
    auto refIt = reference.begin();
    while (ourIt != container.end() && refIt != reference.end())
    {
        FATP_ASSERT_EQ(ourIt->first, refIt->first, "Iteration order key mismatch");
        FATP_ASSERT_EQ(ourIt->second, refIt->second, "Iteration order value mismatch");
        ++ourIt;
        ++refIt;
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void run_benchmarks()
{
    std::cout << colors::cyan() << "FlatMap Benchmarks (1k elements):" << colors::reset() << "\n";

    constexpr int N = 1000;
    fat_p::FlatMap<int, int> map;

    double insertTime = measure_perf(
        [&map, i = 0]() mutable
        {
            map.insert({i % N, i});
            ++i;
        },
        100000,
        1000);
    std::cout << "Insert (random): " << format_time(insertTime) << "\n";

    map.clear();
    map.reserve(N);
    double insertSortedTime = measure_perf(
        [&map, i = 0]() mutable
        {
            if (map.size() < N)
            {
                map.insert({static_cast<int>(map.size()), i});
            }
            ++i;
        },
        10000,
        100);
    std::cout << "Insert (sorted, reserved): " << format_time(insertSortedTime) << "\n";

    map.clear();
    for (int i = 0; i < N; ++i)
    {
        map.insert({i, i * 10});
    }

    volatile int findAccumulator = 0;
    double findTime = measure_perf(
        [&map, &findAccumulator, i = 0]() mutable
        {
            auto it = map.find(i % N);
            if (it != map.end())
            {
                findAccumulator += it->second;
            }
            ++i;
        },
        1000000,
        10000);
    std::cout << "Find: " << format_time(findTime) << "\n";
    DoNotOptimize(findAccumulator);

    double iterTime = measure_perf(
        [&map]()
        {
            volatile int sum = 0;
            for (const auto& kv : map)
            {
                sum += kv.second;
            }
            DoNotOptimize(sum);
        },
        100000,
        1000);
    std::cout << "Iteration (1000 elements): " << format_time(iterTime) << "\n";

    std::map<int, int> stdMap;
    for (int i = 0; i < N; ++i)
    {
        stdMap.insert({i, i * 10});
    }

    volatile int stdFindAccumulator = 0;
    double stdFindTime = measure_perf(
        [&stdMap, &stdFindAccumulator, i = 0]() mutable
        {
            auto it = stdMap.find(i % N);
            if (it != stdMap.end())
            {
                stdFindAccumulator += it->second;
            }
            ++i;
        },
        1000000,
        10000);
    std::cout << "std::map Find: " << format_time(stdFindTime) << "\n";
    DoNotOptimize(stdFindAccumulator);

    double stdIterTime = measure_perf(
        [&stdMap]()
        {
            volatile int sum = 0;
            for (const auto& [k, v] : stdMap)
            {
                sum += v;
            }
            DoNotOptimize(sum);
        },
        100000,
        1000);
    std::cout << "std::map Iteration (1000 elements): " << format_time(stdIterTime) << "\n";
}

void run_large_scale_benchmarks()
{
    std::cout << "\n"
              << colors::cyan() << "Large-Scale Benchmarks (100k elements, cache stress):" << colors::reset() << "\n";

    constexpr int N = 100000;

    std::vector<int> randomKeys(N);
    std::iota(randomKeys.begin(), randomKeys.end(), 0);
    std::mt19937 rng(42);
    std::shuffle(randomKeys.begin(), randomKeys.end(), rng);

    fat_p::FlatMap<int, int> map;
    map.reserve(N);
    for (int i = 0; i < N; ++i)
    {
        map.insert({i, i * 10});
    }

    std::map<int, int> stdMap;
    for (int i = 0; i < N; ++i)
    {
        stdMap.insert({i, i * 10});
    }

    volatile int findAccumulator = 0;
    double findTime = measure_perf(
        [&map, &randomKeys, &findAccumulator, i = 0]() mutable
        {
            auto it = map.find(randomKeys[i % N]);
            if (it != map.end())
            {
                findAccumulator += it->second;
            }
            ++i;
        },
        100000,
        1000);
    std::cout << "FlatMap Find (random access): " << format_time(findTime) << "\n";
    DoNotOptimize(findAccumulator);

    volatile int stdFindAccumulator = 0;
    double stdFindTime = measure_perf(
        [&stdMap, &randomKeys, &stdFindAccumulator, i = 0]() mutable
        {
            auto it = stdMap.find(randomKeys[i % N]);
            if (it != stdMap.end())
            {
                stdFindAccumulator += it->second;
            }
            ++i;
        },
        100000,
        1000);
    std::cout << "std::map Find (random access): " << format_time(stdFindTime) << "\n";
    DoNotOptimize(stdFindAccumulator);

    double iterTime = measure_perf(
        [&map]()
        {
            volatile int sum = 0;
            for (const auto& kv : map)
            {
                sum += kv.second;
            }
            DoNotOptimize(sum);
        },
        1000,
        100);
    std::cout << "FlatMap Iteration (100k elements): " << format_time(iterTime) << "\n";

    double stdIterTime = measure_perf(
        [&stdMap]()
        {
            volatile int sum = 0;
            for (const auto& [k, v] : stdMap)
            {
                sum += v;
            }
            DoNotOptimize(sum);
        },
        1000,
        100);
    std::cout << "std::map Iteration (100k elements): " << format_time(stdIterTime) << "\n";
}

} // namespace fat_p::testing::flatmap

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_FlatMap()
{
    FATP_PRINT_HEADER(FLAT MAP)

    auto sysInfo = SystemInfo::capture();
    sysInfo.print();

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Constructors
    out << colors::blue() << "--- Constructors ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, default_constructor);
    FATP_RUN_TEST_NS(runner, flatmap, comparator_constructor);
    FATP_RUN_TEST_NS(runner, flatmap, allocator_constructor);
    FATP_RUN_TEST_NS(runner, flatmap, comparator_allocator_constructor);
    FATP_RUN_TEST_NS(runner, flatmap, range_constructor);
    FATP_RUN_TEST_NS(runner, flatmap, initializer_list_constructor);

    // Copy/Move Semantics
    out << "\n" << colors::blue() << "--- Copy/Move Semantics ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, copy_constructor);
    FATP_RUN_TEST_NS(runner, flatmap, copy_assignment);
    FATP_RUN_TEST_NS(runner, flatmap, self_assignment);
    FATP_RUN_TEST_NS(runner, flatmap, move_constructor);
    FATP_RUN_TEST_NS(runner, flatmap, move_assignment);

    // Basic Operations
    out << "\n" << colors::blue() << "--- Basic Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, basic_operations);
    FATP_RUN_TEST_NS(runner, flatmap, single_element);
    FATP_RUN_TEST_NS(runner, flatmap, find_operations);
    FATP_RUN_TEST_NS(runner, flatmap, count);
    FATP_RUN_TEST_NS(runner, flatmap, operator_bracket);
    FATP_RUN_TEST_NS(runner, flatmap, at_method);
    FATP_RUN_TEST_NS(runner, flatmap, insert_or_assign);
    FATP_RUN_TEST_NS(runner, flatmap, erase_by_key);
    FATP_RUN_TEST_NS(runner, flatmap, erase_by_iterator);
    FATP_RUN_TEST_NS(runner, flatmap, erase_range);
    FATP_RUN_TEST_NS(runner, flatmap, sorted_order);
    FATP_RUN_TEST_NS(runner, flatmap, lower_upper_bound);
    FATP_RUN_TEST_NS(runner, flatmap, equal_range);
    FATP_RUN_TEST_NS(runner, flatmap, clear);

    // Iterators
    out << "\n" << colors::blue() << "--- Iterators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, iterator_basics);
    FATP_RUN_TEST_NS(runner, flatmap, const_iterator);
    FATP_RUN_TEST_NS(runner, flatmap, reverse_iterator);
    FATP_RUN_TEST_NS(runner, flatmap, iterator_key_immutability);

    // Capacity
    out << "\n" << colors::blue() << "--- Capacity ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, reserve_capacity);
    FATP_RUN_TEST_NS(runner, flatmap, max_size);

    // Observers
    out << "\n" << colors::blue() << "--- Observers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, key_comp);
    FATP_RUN_TEST_NS(runner, flatmap, value_comp);
    FATP_RUN_TEST_NS(runner, flatmap, get_allocator);

    // Comparators
    out << "\n" << colors::blue() << "--- Custom Comparators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, custom_comparator);
    FATP_RUN_TEST_NS(runner, flatmap, case_insensitive_comparator);
    FATP_RUN_TEST_NS(runner, flatmap, case_insensitive_constructor_keeps_first);

    // Modifiers
    out << "\n" << colors::blue() << "--- Modifiers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, equality_operators);
    FATP_RUN_TEST_NS(runner, flatmap, swap);
    FATP_RUN_TEST_NS(runner, flatmap, try_emplace);
    FATP_RUN_TEST_NS(runner, flatmap, try_emplace_does_not_construct_on_hit);
    FATP_RUN_TEST_NS(runner, flatmap, emplace);
    FATP_RUN_TEST_NS(runner, flatmap, emplace_hint);
    FATP_RUN_TEST_NS(runner, flatmap, range_insert);
    FATP_RUN_TEST_NS(runner, flatmap, initializer_list_insert);
    FATP_RUN_TEST_NS(runner, flatmap, extract);
    FATP_RUN_TEST_NS(runner, flatmap, empty_operations);
    FATP_RUN_TEST_NS(runner, flatmap, heterogeneous_lookup);
    FATP_RUN_TEST_NS(runner, flatmap, merge);

    // RAII & Lifecycle
    out << "\n" << colors::blue() << "--- RAII & Lifecycle ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, lifecycle_tracking);
    FATP_RUN_TEST_NS(runner, flatmap, lifecycle_on_clear);
    FATP_RUN_TEST_NS(runner, flatmap, lifecycle_on_erase);

    // Exception Safety
    out << "\n" << colors::blue() << "--- Exception Safety ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, exception_safety_insert);

    // Move-Only Types
    out << "\n" << colors::blue() << "--- Move-Only Types ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, move_only_values);

    // Stress Tests
    out << "\n" << colors::blue() << "--- Stress Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatmap, stress_random_operations);
    FATP_RUN_TEST_NS(runner, flatmap, stress_comprehensive);

    // Benchmarks (skip in debug builds)
#ifdef NDEBUG
    flatmap::run_benchmarks();
    flatmap::run_large_scale_benchmarks();
#else
    out << "\n" << colors::yellow() << "[Debug build - skipping benchmarks]" << colors::reset() << "\n";
#endif

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FlatMap() ? 0 : 1;
}
#endif
