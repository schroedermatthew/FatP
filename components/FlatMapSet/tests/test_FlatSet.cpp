/**
 * @file test_FlatSet.cpp
 * @brief Comprehensive unit tests for FlatSet.h
 */
/*
FATP_META:
  meta_version: 1
  component: FlatSet
  file_role: test
  path: components/FlatMapSet/tests/test_FlatSet.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for FlatSet."
  api_stability: in_work
  related:
    docs_search: "FlatSet"
    headers:
      - include/fat_p/FlatSet.h
      - include/fat_p/FatPTest.h
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
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "FatPTest.h"
#include "FlatSet.h"

namespace fat_p::testing::flatset
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

    bool operator<(const LifecycleTracker& other) const
    {
        return value < other.value;
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
    fat_p::FlatSet<int> set;

    FATP_ASSERT_TRUE(set.empty(), "Default constructed set should be empty");
    FATP_ASSERT_EQ(set.size(), size_t(0), "Size should be 0");
    FATP_ASSERT_TRUE(set.begin() == set.end(), "begin() should equal end()");

    return true;
}

FATP_TEST_CASE(comparator_constructor)
{
    fat_p::FlatSet<int, std::greater<int>> set(std::greater<int>{});

    set.insert(1);
    set.insert(2);
    set.insert(3);

    std::vector<int> values(set.begin(), set.end());

    FATP_ASSERT_EQ(values.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_EQ(values[0], 3, "First should be 3 (descending)");
    FATP_ASSERT_EQ(values[1], 2, "Second should be 2");
    FATP_ASSERT_EQ(values[2], 1, "Third should be 1");

    return true;
}

FATP_TEST_CASE(allocator_constructor)
{
    std::allocator<int> alloc;
    fat_p::FlatSet<int> set(alloc);

    FATP_ASSERT_TRUE(set.empty(), "Allocator-constructed set should be empty");

    set.insert(1);
    FATP_ASSERT_EQ(set.size(), size_t(1), "Should have 1 element after insert");

    return true;
}

FATP_TEST_CASE(comparator_allocator_constructor)
{
    std::allocator<int> alloc;
    fat_p::FlatSet<int, std::greater<int>> set(std::greater<int>{}, alloc);

    set.insert(1);
    set.insert(2);

    auto it = set.begin();
    FATP_ASSERT_EQ(*it, 2, "First element should be 2 (descending order)");

    return true;
}

FATP_TEST_CASE(range_constructor)
{
    std::vector<int> data = {3, 1, 2, 1, 3}; // Note: duplicates

    fat_p::FlatSet<int> set(data.begin(), data.end());

    FATP_ASSERT_EQ(set.size(), size_t(3), "Should have 3 unique values");
    FATP_ASSERT_TRUE(set.contains(1), "Should contain 1");
    FATP_ASSERT_TRUE(set.contains(2), "Should contain 2");
    FATP_ASSERT_TRUE(set.contains(3), "Should contain 3");

    // Verify sorted order
    FATP_ASSERT_TRUE(std::is_sorted(set.begin(), set.end()), "Values should be sorted");

    return true;
}

FATP_TEST_CASE(initializer_list_constructor)
{
    fat_p::FlatSet<int> set{3, 1, 2};

    FATP_ASSERT_EQ(set.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_TRUE(set.contains(1), "Should contain 1");
    FATP_ASSERT_TRUE(set.contains(2), "Should contain 2");
    FATP_ASSERT_TRUE(set.contains(3), "Should contain 3");

    return true;
}

// ============================================================================
// Copy/Move Semantics Tests
// ============================================================================

FATP_TEST_CASE(copy_constructor)
{
    fat_p::FlatSet<int> original{1, 2, 3};

    fat_p::FlatSet<int> copy(original);

    FATP_ASSERT_EQ(copy.size(), original.size(), "Copy should have same size");
    FATP_ASSERT_TRUE(copy.contains(1), "Copy should contain 1");
    FATP_ASSERT_TRUE(copy.contains(2), "Copy should contain 2");
    FATP_ASSERT_TRUE(copy.contains(3), "Copy should contain 3");

    // Modify copy by adding element, original unchanged
    copy.insert(4);
    FATP_ASSERT_FALSE(original.contains(4), "Original should be unchanged");
    FATP_ASSERT_TRUE(copy.contains(4), "Copy should have new element");

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    fat_p::FlatSet<int> original{1, 2};
    fat_p::FlatSet<int> copy{99};

    copy = original;

    FATP_ASSERT_EQ(copy.size(), size_t(2), "Copy should have 2 elements");
    FATP_ASSERT_FALSE(copy.contains(99), "Old content should be gone");
    FATP_ASSERT_TRUE(copy.contains(1), "Should have copied content");

    return true;
}

FATP_TEST_CASE(self_assignment)
{
    fat_p::FlatSet<int> set{1, 2};

    auto* ptr = &set;
    *ptr = set; // Self-assignment

    FATP_ASSERT_EQ(set.size(), size_t(2), "Size should be unchanged after self-assignment");
    FATP_ASSERT_TRUE(set.contains(1), "Content should be unchanged");
    FATP_ASSERT_TRUE(set.contains(2), "Content should be unchanged");

    return true;
}

FATP_TEST_CASE(move_constructor)
{
    fat_p::FlatSet<int> original{1, 2, 3};

    fat_p::FlatSet<int> moved(std::move(original));

    FATP_ASSERT_EQ(moved.size(), size_t(3), "Moved-to set should have 3 elements");
    FATP_ASSERT_TRUE(moved.contains(1), "Moved-to set should contain 1");
    FATP_ASSERT_TRUE(moved.contains(2), "Moved-to set should contain 2");
    FATP_ASSERT_TRUE(moved.contains(3), "Moved-to set should contain 3");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    fat_p::FlatSet<int> original{1, 2};
    fat_p::FlatSet<int> target{99};

    target = std::move(original);

    FATP_ASSERT_EQ(target.size(), size_t(2), "Target should have 2 elements");
    FATP_ASSERT_FALSE(target.contains(99), "Old content should be gone");
    FATP_ASSERT_TRUE(target.contains(1), "Should have moved content");

    return true;
}

// ============================================================================
// Basic Operations Tests
// ============================================================================

FATP_TEST_CASE(basic_operations)
{
    fat_p::FlatSet<int> set;

    FATP_ASSERT_TRUE(set.empty(), "New set should be empty");
    FATP_ASSERT_EQ(set.size(), size_t(0), "Size should be 0");

    set.insert(1);
    set.insert(2);
    set.insert(3);

    FATP_ASSERT_EQ(set.size(), size_t(3), "Size should be 3");
    FATP_ASSERT_FALSE(set.empty(), "Set should not be empty");

    return true;
}

FATP_TEST_CASE(single_element)
{
    fat_p::FlatSet<int> set;

    set.insert(42);

    FATP_ASSERT_EQ(set.size(), size_t(1), "Size should be 1");
    FATP_ASSERT_FALSE(set.empty(), "Set should not be empty");
    FATP_ASSERT_TRUE(set.contains(42), "Should contain the value");

    // Iterator operations on single element
    FATP_ASSERT_EQ(*set.begin(), 42, "begin() should point to the element");
    FATP_ASSERT_EQ(std::distance(set.begin(), set.end()), 1, "Should have exactly 1 element");

    // Erase single element
    set.erase(42);
    FATP_ASSERT_TRUE(set.empty(), "Set should be empty after erasing only element");

    return true;
}

FATP_TEST_CASE(insert_duplicate)
{
    fat_p::FlatSet<int> set;

    auto [it1, inserted1] = set.insert(5);
    FATP_ASSERT_TRUE(inserted1, "First insert should succeed");
    FATP_ASSERT_EQ(*it1, 5, "Value should be 5");

    auto [it2, inserted2] = set.insert(5);
    FATP_ASSERT_FALSE(inserted2, "Duplicate insert should fail");
    FATP_ASSERT_EQ(*it2, 5, "Value should still be 5");

    FATP_ASSERT_EQ(set.size(), size_t(1), "Size should still be 1");

    return true;
}

FATP_TEST_CASE(find_operations)
{
    fat_p::FlatSet<int> set{10, 20, 30, 40};

    auto it = set.find(20);
    FATP_ASSERT_TRUE(it != set.end(), "Should find 20");
    FATP_ASSERT_EQ(*it, 20, "Value should be 20");

    it = set.find(999);
    FATP_ASSERT_TRUE(it == set.end(), "Should not find nonexistent value");

    FATP_ASSERT_TRUE(set.contains(30), "Should contain 30");
    FATP_ASSERT_FALSE(set.contains(50), "Should not contain 50");

    return true;
}

FATP_TEST_CASE(count)
{
    fat_p::FlatSet<int> set{1, 2, 3};

    FATP_ASSERT_EQ(set.count(1), size_t(1), "count(1) should return 1");
    FATP_ASSERT_EQ(set.count(2), size_t(1), "count(2) should return 1");
    FATP_ASSERT_EQ(set.count(99), size_t(0), "count(99) should return 0");

    return true;
}

FATP_TEST_CASE(erase_by_value)
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    size_t erased = set.erase(3);
    FATP_ASSERT_EQ(erased, size_t(1), "Should erase one element");
    FATP_ASSERT_EQ(set.size(), size_t(4), "Size should be 4");
    FATP_ASSERT_FALSE(set.contains(3), "Should not contain 3");

    erased = set.erase(999);
    FATP_ASSERT_EQ(erased, size_t(0), "Should not erase nonexistent element");

    return true;
}

FATP_TEST_CASE(erase_by_iterator)
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    auto it = set.find(3);
    FATP_ASSERT_TRUE(it != set.end(), "Should find value 3");

    auto next = set.erase(it);
    FATP_ASSERT_EQ(set.size(), size_t(4), "Size should be 4 after erase");
    FATP_ASSERT_FALSE(set.contains(3), "Value 3 should be gone");
    FATP_ASSERT_TRUE(next == set.find(4) || next == set.end(), "Iterator should point to next element or end");

    return true;
}

FATP_TEST_CASE(erase_range)
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    auto first = set.find(2);
    auto last = set.find(4);

    set.erase(first, last); // Erases 2 and 3

    FATP_ASSERT_EQ(set.size(), size_t(3), "Size should be 3 after range erase");
    FATP_ASSERT_TRUE(set.contains(1), "Value 1 should remain");
    FATP_ASSERT_FALSE(set.contains(2), "Value 2 should be erased");
    FATP_ASSERT_FALSE(set.contains(3), "Value 3 should be erased");
    FATP_ASSERT_TRUE(set.contains(4), "Value 4 should remain");
    FATP_ASSERT_TRUE(set.contains(5), "Value 5 should remain");

    return true;
}

FATP_TEST_CASE(sorted_order)
{
    fat_p::FlatSet<int> set;
    set.insert(5);
    set.insert(1);
    set.insert(3);
    set.insert(2);
    set.insert(4);

    FATP_ASSERT_TRUE(std::is_sorted(set.begin(), set.end()), "Values should be sorted");

    return true;
}

FATP_TEST_CASE(lower_upper_bound)
{
    fat_p::FlatSet<int> set{10, 20, 30, 40, 50};

    auto it = set.lower_bound(30);
    FATP_ASSERT_TRUE(it != set.end(), "Should find lower bound");
    FATP_ASSERT_EQ(*it, 30, "Lower bound should be 30");

    it = set.lower_bound(25); // Value doesn't exist
    FATP_ASSERT_TRUE(it != set.end(), "Should find lower bound for non-existent value");
    FATP_ASSERT_EQ(*it, 30, "Lower bound of 25 should be 30");

    it = set.upper_bound(30);
    FATP_ASSERT_TRUE(it != set.end(), "Should find upper bound");
    FATP_ASSERT_EQ(*it, 40, "Upper bound should be 40");

    it = set.upper_bound(50);
    FATP_ASSERT_TRUE(it == set.end(), "Upper bound of max should be end");

    return true;
}

FATP_TEST_CASE(equal_range)
{
    fat_p::FlatSet<int> set{10, 20, 30};

    auto [first, last] = set.equal_range(20);
    FATP_ASSERT_TRUE(first != set.end(), "Range should not be empty");
    FATP_ASSERT_EQ(*first, 20, "First should be 20");

    size_t count = static_cast<size_t>(std::distance(first, last));
    FATP_ASSERT_EQ(count, size_t(1), "Should have exactly one element");

    // Non-existent value
    auto [first2, last2] = set.equal_range(99);
    FATP_ASSERT_TRUE(first2 == last2, "Range for non-existent value should be empty");

    return true;
}

FATP_TEST_CASE(clear)
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    FATP_ASSERT_EQ(set.size(), size_t(5), "Size should be 5");

    set.clear();
    FATP_ASSERT_EQ(set.size(), size_t(0), "Size should be 0 after clear");
    FATP_ASSERT_TRUE(set.empty(), "Set should be empty after clear");
    FATP_ASSERT_TRUE(set.begin() == set.end(), "begin() should equal end() after clear");

    return true;
}

// ============================================================================
// Iterator Tests
// ============================================================================

FATP_TEST_CASE(iterator_basics)
{
    fat_p::FlatSet<int> set{1, 2, 3};

    // Forward iteration
    std::vector<int> values;
    for (auto it = set.begin(); it != set.end(); ++it)
    {
        values.push_back(*it);
    }
    FATP_ASSERT_EQ(values.size(), size_t(3), "Should iterate over all elements");
    FATP_ASSERT_TRUE(std::is_sorted(values.begin(), values.end()), "Should iterate in sorted order");

    // Range-based for
    values.clear();
    for (int v : set)
    {
        values.push_back(v);
    }
    FATP_ASSERT_EQ(values.size(), size_t(3), "Range-based for should work");

    return true;
}

FATP_TEST_CASE(const_iterator)
{
    fat_p::FlatSet<int> set{1, 2};
    const auto& constSet = set;

    std::vector<int> values;
    for (auto it = constSet.begin(); it != constSet.end(); ++it)
    {
        values.push_back(*it);
    }
    FATP_ASSERT_EQ(values.size(), size_t(2), "Const iteration should work");

    // cbegin/cend
    values.clear();
    for (auto it = set.cbegin(); it != set.cend(); ++it)
    {
        values.push_back(*it);
    }
    FATP_ASSERT_EQ(values.size(), size_t(2), "cbegin/cend should work");

    return true;
}

FATP_TEST_CASE(reverse_iterator)
{
    fat_p::FlatSet<int> set{1, 2, 3};

    std::vector<int> values;
    for (auto it = set.rbegin(); it != set.rend(); ++it)
    {
        values.push_back(*it);
    }

    FATP_ASSERT_EQ(values.size(), size_t(3), "Should iterate over all elements");
    FATP_ASSERT_EQ(values[0], 3, "First in reverse should be 3");
    FATP_ASSERT_EQ(values[1], 2, "Second in reverse should be 2");
    FATP_ASSERT_EQ(values[2], 1, "Third in reverse should be 1");

    // const reverse iterator
    const auto& constSet = set;
    values.clear();
    for (auto it = constSet.rbegin(); it != constSet.rend(); ++it)
    {
        values.push_back(*it);
    }
    FATP_ASSERT_EQ(values.size(), size_t(3), "Const reverse iteration should work");

    // crbegin/crend
    values.clear();
    for (auto it = set.crbegin(); it != set.crend(); ++it)
    {
        values.push_back(*it);
    }
    FATP_ASSERT_EQ(values.size(), size_t(3), "crbegin/crend should work");

    return true;
}

// ============================================================================
// Capacity Tests
// ============================================================================

FATP_TEST_CASE(reserve_capacity)
{
    fat_p::FlatSet<int> set;

    FATP_ASSERT_EQ(set.capacity(), size_t(0), "Initial capacity should be 0");

    set.reserve(100);
    FATP_ASSERT_GE(set.capacity(), size_t(100), "Capacity should be at least 100");
    FATP_ASSERT_EQ(set.size(), size_t(0), "Size should still be 0");

    for (int i = 0; i < 50; ++i)
    {
        set.insert(i);
    }

    FATP_ASSERT_EQ(set.size(), size_t(50), "Size should be 50");
    FATP_ASSERT_GE(set.capacity(), size_t(100), "Capacity should still be at least 100");

    set.shrink_to_fit();
    FATP_ASSERT_GE(set.capacity(), set.size(), "Capacity should be at least size");

    return true;
}

FATP_TEST_CASE(max_size)
{
    fat_p::FlatSet<int> set;

    FATP_ASSERT_GT(set.max_size(), size_t(0), "max_size should be positive");
    FATP_ASSERT_GT(set.max_size(), size_t(1000000), "max_size should be large");

    return true;
}

// ============================================================================
// Observers Tests
// ============================================================================

FATP_TEST_CASE(key_comp)
{
    fat_p::FlatSet<int> set;
    auto comp = set.key_comp();

    FATP_ASSERT_TRUE(comp(1, 2), "1 < 2 should be true");
    FATP_ASSERT_FALSE(comp(2, 1), "2 < 1 should be false");
    FATP_ASSERT_FALSE(comp(1, 1), "1 < 1 should be false");

    // With custom comparator
    fat_p::FlatSet<int, std::greater<int>> descSet;
    auto descComp = descSet.key_comp();

    FATP_ASSERT_FALSE(descComp(1, 2), "1 > 2 should be false");
    FATP_ASSERT_TRUE(descComp(2, 1), "2 > 1 should be true");

    return true;
}

FATP_TEST_CASE(value_comp)
{
    fat_p::FlatSet<int> set;
    auto comp = set.value_comp();

    FATP_ASSERT_TRUE(comp(1, 2), "1 < 2 should be true");
    FATP_ASSERT_FALSE(comp(2, 1), "2 < 1 should be false");

    return true;
}

FATP_TEST_CASE(get_allocator)
{
    fat_p::FlatSet<int> set;
    auto alloc = set.get_allocator();

    // Just verify it compiles and returns something
    using AllocType = decltype(alloc);
    FATP_ASSERT_TRUE((std::is_same_v<AllocType, std::allocator<int>>), "Allocator type should match");

    return true;
}

// ============================================================================
// Comparator Tests
// ============================================================================

FATP_TEST_CASE(custom_comparator)
{
    fat_p::FlatSet<int, std::greater<int>> set;

    set.insert(1);
    set.insert(3);
    set.insert(2);

    std::vector<int> values(set.begin(), set.end());

    FATP_ASSERT_EQ(values.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_EQ(values[0], 3, "First should be 3 (descending)");
    FATP_ASSERT_EQ(values[1], 2, "Second should be 2");
    FATP_ASSERT_EQ(values[2], 1, "Third should be 1");

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
                                                [](unsigned char c1, unsigned char c2) {
                                                    return std::tolower(c1) < std::tolower(c2);
                                                });
        }
    };

    fat_p::FlatSet<std::string, CaseInsensitiveCompare> set;

    set.insert("Hello");
    auto [it, inserted] = set.insert("HELLO");

    FATP_ASSERT_FALSE(inserted, "HELLO should be duplicate of Hello");
    FATP_ASSERT_EQ(set.size(), size_t(1), "Should have only 1 element");
    FATP_ASSERT_TRUE(set.contains("hello"), "Should find 'hello'");
    FATP_ASSERT_TRUE(set.contains("HELLO"), "Should find 'HELLO'");

    return true;
}

FATP_TEST_CASE(case_insensitive_constructor_keeps_first)
{
    struct CaseInsensitiveCompare
    {
        bool operator()(const std::string& a, const std::string& b) const
        {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) < std::tolower(static_cast<unsigned char>(c2));
            });
        }
    };

    std::vector<std::string> data = {"Hello", "HELLO", "hello", "World"};

    fat_p::FlatSet<std::string, CaseInsensitiveCompare> set(data.begin(), data.end());

    FATP_ASSERT_EQ(set.size(), size_t(2), "Should have 2 unique values under comparator");
    FATP_ASSERT_EQ(*set.begin(),
                   std::string("Hello"),
                   "Constructor should keep the first inserted representative for equivalent values");

    FATP_ASSERT_TRUE(set.contains("HELLO"), "Equivalent lookup should succeed");
    FATP_ASSERT_TRUE(set.contains("hello"), "Equivalent lookup should succeed");

    return true;
}

// ============================================================================
// Modifier Tests
// ============================================================================

FATP_TEST_CASE(equality_operators)
{
    fat_p::FlatSet<int> set1{1, 2, 3};
    fat_p::FlatSet<int> set2{1, 2, 3};
    fat_p::FlatSet<int> set3{1, 2, 4};
    fat_p::FlatSet<int> set4{1, 2};

    FATP_ASSERT_TRUE(set1 == set2, "Identical sets should be equal");
    FATP_ASSERT_FALSE(set1 != set2, "Identical sets should not be not-equal");

    FATP_ASSERT_TRUE(set1 != set3, "Sets with different values should not be equal");
    FATP_ASSERT_TRUE(set1 != set4, "Sets with different sizes should not be equal");

    return true;
}

FATP_TEST_CASE(swap)
{
    fat_p::FlatSet<int> set1{1, 2};
    fat_p::FlatSet<int> set2{3, 4, 5};

    set1.swap(set2);

    FATP_ASSERT_EQ(set1.size(), size_t(3), "set1 should have 3 elements after swap");
    FATP_ASSERT_EQ(set2.size(), size_t(2), "set2 should have 2 elements after swap");
    FATP_ASSERT_TRUE(set1.contains(3), "set1 should contain 3");
    FATP_ASSERT_TRUE(set2.contains(1), "set2 should contain 1");

    // ADL swap
    using std::swap;
    swap(set1, set2);
    FATP_ASSERT_EQ(set1.size(), size_t(2), "set1 should have 2 elements after ADL swap");
    FATP_ASSERT_EQ(set2.size(), size_t(3), "set2 should have 3 elements after ADL swap");

    return true;
}

FATP_TEST_CASE(emplace)
{
    fat_p::FlatSet<std::string> set;

    auto [it1, inserted1] = set.emplace("hello");
    FATP_ASSERT_TRUE(inserted1, "Should insert new value");
    FATP_ASSERT_EQ(*it1, std::string("hello"), "Value should be 'hello'");

    auto [it2, inserted2] = set.emplace("hello");
    FATP_ASSERT_FALSE(inserted2, "Should not insert duplicate");

    return true;
}

FATP_TEST_CASE(emplace_hint)
{
    fat_p::FlatSet<int> set{1, 3};

    auto hint = set.find(1);
    auto it = set.emplace_hint(hint, 2);

    FATP_ASSERT_EQ(*it, 2, "Should have inserted 2");
    FATP_ASSERT_EQ(set.size(), size_t(3), "Size should be 3");

    return true;
}

FATP_TEST_CASE(range_insert)
{
    std::vector<int> data = {5, 1, 3, 2, 4, 3, 1};

    fat_p::FlatSet<int> set;
    set.insert(data.begin(), data.end());

    FATP_ASSERT_EQ(set.size(), size_t(5), "Should have 5 unique elements");

    std::vector<int> values(set.begin(), set.end());
    FATP_ASSERT_TRUE(std::is_sorted(values.begin(), values.end()), "Values should be sorted");

    return true;
}

FATP_TEST_CASE(initializer_list_insert)
{
    fat_p::FlatSet<int> set{1, 2};

    set.insert({3, 4, 5, 3, 4});

    FATP_ASSERT_EQ(set.size(), size_t(5), "Should have 5 unique elements");
    FATP_ASSERT_TRUE(set.contains(5), "Should contain 5");

    return true;
}

FATP_TEST_CASE(extract)
{
    fat_p::FlatSet<int> set{1, 2, 3, 4, 5};

    auto it = set.find(3);
    FATP_ASSERT_TRUE(it != set.end(), "Should find value 3");

    auto extracted = set.extract(it);
    FATP_ASSERT_EQ(extracted, 3, "Extracted value should be 3");
    FATP_ASSERT_EQ(set.size(), size_t(4), "Set should have 4 elements after extract");
    FATP_ASSERT_FALSE(set.contains(3), "Set should not contain 3 after extract");

    return true;
}

FATP_TEST_CASE(empty_operations)
{
    fat_p::FlatSet<int> set;

    FATP_ASSERT_TRUE(set.find(1) == set.end(), "find on empty set should return end");
    FATP_ASSERT_FALSE(set.contains(1), "contains on empty set should return false");
    FATP_ASSERT_EQ(set.count(1), size_t(0), "count on empty set should return 0");
    FATP_ASSERT_TRUE(set.lower_bound(1) == set.end(), "lower_bound on empty set should return end");
    FATP_ASSERT_TRUE(set.upper_bound(1) == set.end(), "upper_bound on empty set should return end");

    auto [first, last] = set.equal_range(1);
    FATP_ASSERT_TRUE(first == last, "equal_range on empty set should return empty range");

    FATP_ASSERT_EQ(set.erase(1), size_t(0), "erase nonexistent value should return 0");

    // Clear on empty should be safe
    set.clear();
    FATP_ASSERT_TRUE(set.empty(), "clear on empty should leave set empty");

    return true;
}

FATP_TEST_CASE(heterogeneous_lookup)
{
    // Use std::less<> for transparent comparison
    fat_p::FlatSet<std::string, std::less<>> set;

    set.insert("apple");
    set.insert("banana");
    set.insert("cherry");

    // These lookups should NOT create temporary std::string objects
    auto it = set.find("banana");
    FATP_ASSERT_TRUE(it != set.end(), "find with const char* should work");
    FATP_ASSERT_EQ(*it, std::string("banana"), "find should return correct value");

    FATP_ASSERT_TRUE(set.contains("apple"), "contains with const char* should work");
    FATP_ASSERT_FALSE(set.contains("grape"), "contains should return false for missing value");

    FATP_ASSERT_EQ(set.count("cherry"), size_t(1), "count with const char* should work");
    FATP_ASSERT_EQ(set.count("grape"), size_t(0), "count should return 0 for missing value");

    auto lb = set.lower_bound("banana");
    FATP_ASSERT_TRUE(lb != set.end(), "lower_bound should find element");
    FATP_ASSERT_EQ(*lb, std::string("banana"), "lower_bound with const char* should work");

    auto ub = set.upper_bound("banana");
    FATP_ASSERT_TRUE(ub != set.end(), "upper_bound should find next element");
    FATP_ASSERT_EQ(*ub, std::string("cherry"), "upper_bound with const char* should work");

    auto [first, last] = set.equal_range("banana");
    FATP_ASSERT_TRUE(first != last, "equal_range should find element");
    FATP_ASSERT_EQ(*first, std::string("banana"), "equal_range should return correct element");

    return true;
}

FATP_TEST_CASE(merge)
{
    fat_p::FlatSet<int> set1{1, 3, 5};
    fat_p::FlatSet<int> set2{2, 3, 4}; // 3 is duplicate

    set1.merge(set2);

    FATP_ASSERT_EQ(set1.size(), size_t(5), "Merged set should have 5 elements");

    // Element 3 was duplicate - it should remain in source
    FATP_ASSERT_EQ(set2.size(), size_t(1), "Source should have 1 element (the duplicate)");
    FATP_ASSERT_TRUE(set2.contains(3), "Source should still contain duplicate element 3");

    FATP_ASSERT_TRUE(set1.contains(1), "Element 1 should be preserved");
    FATP_ASSERT_TRUE(set1.contains(2), "Element 2 should be merged");
    FATP_ASSERT_TRUE(set1.contains(3), "Element 3 should be present");
    FATP_ASSERT_TRUE(set1.contains(4), "Element 4 should be merged");
    FATP_ASSERT_TRUE(set1.contains(5), "Element 5 should be preserved");

    // Verify sorted order
    int prev = -1;
    for (int v : set1)
    {
        FATP_ASSERT_GT(v, prev, "Merged set should maintain sorted order");
        prev = v;
    }

    // Test merge with empty source
    fat_p::FlatSet<int> empty;
    size_t sizeBefore = set1.size();
    set1.merge(empty);
    FATP_ASSERT_EQ(set1.size(), sizeBefore, "Merging empty set should not change size");

    // Test merge into empty target
    fat_p::FlatSet<int> target;
    fat_p::FlatSet<int> source{10, 20};
    target.merge(source);
    FATP_ASSERT_EQ(target.size(), size_t(2), "Merge into empty should work");
    FATP_ASSERT_TRUE(source.empty(), "Source should be empty after merge (no duplicates)");

    // Self-merge should be a no-op (and must not crash)
    const size_t selfSizeBefore = set1.size();
    set1.merge(set1);
    FATP_ASSERT_EQ(set1.size(), selfSizeBefore, "Self-merge should not change size");

    return true;
}

// ============================================================================
// RAII and Lifecycle Tests
// ============================================================================

FATP_TEST_CASE(lifecycle_tracking)
{
    LifecycleTracker::reset();

    {
        fat_p::FlatSet<LifecycleTracker> set;
        set.emplace(LifecycleTracker(10));
        set.emplace(LifecycleTracker(20));
        set.emplace(LifecycleTracker(30));

        FATP_ASSERT_EQ(set.size(), size_t(3), "Should have 3 elements");
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

    fat_p::FlatSet<LifecycleTracker> set;
    set.emplace(LifecycleTracker(10));
    set.emplace(LifecycleTracker(20));

    int destructedBefore = LifecycleTracker::destruct_count;

    set.clear();

    FATP_ASSERT_TRUE(set.empty(), "Set should be empty after clear");
    FATP_ASSERT_GT(LifecycleTracker::destruct_count, destructedBefore, "Clear should destroy elements");

    return true;
}

FATP_TEST_CASE(lifecycle_on_erase)
{
    LifecycleTracker::reset();

    fat_p::FlatSet<LifecycleTracker> set;
    set.emplace(LifecycleTracker(10));
    set.emplace(LifecycleTracker(20));
    set.emplace(LifecycleTracker(30));

    int destructedBefore = LifecycleTracker::destruct_count;

    auto it = set.find(LifecycleTracker(20));
    if (it != set.end())
    {
        set.erase(it);
    }

    FATP_ASSERT_EQ(set.size(), size_t(2), "Set should have 2 elements");
    FATP_ASSERT_GT(LifecycleTracker::destruct_count, destructedBefore, "Erase should destroy element");

    return true;
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

FATP_TEST_CASE(exception_safety_insert)
{
    ThrowOnCopy::reset();

    fat_p::FlatSet<ThrowOnCopy> set;
    set.emplace(ThrowOnCopy(10));
    set.emplace(ThrowOnCopy(20));

    size_t sizeBefore = set.size();

    ThrowOnCopy::reset();
    ThrowOnCopy::throw_after = 1; // Throw on first copy

    bool threw = false;
    try
    {
        ThrowOnCopy value(30);
        set.insert(value); // This should throw during copy
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown");
    // Basic guarantee: set is still valid
    FATP_ASSERT_GE(set.size(), sizeBefore - 1, "Set should remain valid after exception");

    ThrowOnCopy::reset();

    return true;
}

// ============================================================================
// Move-Only Type Tests
// ============================================================================

FATP_TEST_CASE(move_only_values)
{
    fat_p::FlatSet<MoveOnly> set;

    set.emplace(MoveOnly(10));
    set.emplace(MoveOnly(20));
    set.emplace(MoveOnly(30));

    FATP_ASSERT_EQ(set.size(), size_t(3), "Should have 3 elements");

    // Test move construction
    fat_p::FlatSet<MoveOnly> set2(std::move(set));
    FATP_ASSERT_EQ(set2.size(), size_t(3), "Moved set should have 3 elements");

    return true;
}

// ============================================================================
// Stress/Fuzz Tests
// ============================================================================

FATP_TEST_CASE(stress_random_operations)
{
    fat_p::FlatSet<int> container;
    std::set<int> reference;

    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> keyDist(0, 999);
    std::uniform_int_distribution<int> opDist(0, 2);

    for (int i = 0; i < 5000; ++i)
    {
        int key = keyDist(rng);
        int op = opDist(rng);

        if (op == 0)
        {
            container.insert(key);
            reference.insert(key);
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
    fat_p::FlatSet<int> container;
    std::set<int> reference;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> keyDist(0, 499);
    std::uniform_int_distribution<int> opDist(0, 7);

    for (int i = 0; i < 10000; ++i)
    {
        int key = keyDist(rng);
        int op = opDist(rng);

        switch (op)
        {
            case 0: // insert
            {
                auto [it1, ins1] = container.insert(key);
                auto [it2, ins2] = reference.insert(key);
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

            case 3: // contains/count
            {
                bool ours = container.contains(key);
                bool theirs = reference.count(key) > 0;
                FATP_ASSERT_EQ(ours, theirs, "contains mismatch");

                size_t oursCount = container.count(key);
                size_t theirsCount = reference.count(key);
                FATP_ASSERT_EQ(oursCount, theirsCount, "count mismatch");
                break;
            }

            case 4: // emplace
            {
                auto [it1, ins1] = container.emplace(key);
                auto [it2, ins2] = reference.emplace(key);
                FATP_ASSERT_EQ(ins1, ins2, "emplace mismatch");
                break;
            }

            case 5: // lower_bound
            {
                auto ours = container.lower_bound(key);
                auto theirs = reference.lower_bound(key);
                bool oursEnd = (ours == container.end());
                bool theirsEnd = (theirs == reference.end());
                FATP_ASSERT_EQ(oursEnd, theirsEnd, "lower_bound end mismatch");
                if (!oursEnd && !theirsEnd)
                {
                    FATP_ASSERT_EQ(*ours, *theirs, "lower_bound value mismatch");
                }
                break;
            }

            case 6: // upper_bound
            {
                auto ours = container.upper_bound(key);
                auto theirs = reference.upper_bound(key);
                bool oursEnd = (ours == container.end());
                bool theirsEnd = (theirs == reference.end());
                FATP_ASSERT_EQ(oursEnd, theirsEnd, "upper_bound end mismatch");
                if (!oursEnd && !theirsEnd)
                {
                    FATP_ASSERT_EQ(*ours, *theirs, "upper_bound value mismatch");
                }
                break;
            }

            case 7: // equal_range
            {
                auto [f1, l1] = container.equal_range(key);
                auto [f2, l2] = reference.equal_range(key);
                size_t ourDist = static_cast<size_t>(std::distance(f1, l1));
                size_t refDist = static_cast<size_t>(std::distance(f2, l2));
                FATP_ASSERT_EQ(ourDist, refDist, "equal_range distance mismatch");
                break;
            }
        }
    }

    // Final state verification
    FATP_ASSERT_EQ(container.size(), reference.size(), "Final size mismatch");

    // Verify all elements match
    for (int v : reference)
    {
        FATP_ASSERT_TRUE(container.contains(v), "Missing value in container");
    }

    // Verify iteration order matches
    auto ourIt = container.begin();
    auto refIt = reference.begin();
    while (ourIt != container.end() && refIt != reference.end())
    {
        FATP_ASSERT_EQ(*ourIt, *refIt, "Iteration order mismatch");
        ++ourIt;
        ++refIt;
    }

    return true;
}

} // namespace fat_p::testing::flatset

// ============================================================================

namespace fat_p::testing
{

bool test_FlatSet()
{
    FATP_PRINT_HEADER(FLAT SET)

    auto sysInfo = SystemInfo::capture();
    sysInfo.print();

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Constructors
    out << colors::blue() << "--- Constructors ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, default_constructor);
    FATP_RUN_TEST_NS(runner, flatset, comparator_constructor);
    FATP_RUN_TEST_NS(runner, flatset, allocator_constructor);
    FATP_RUN_TEST_NS(runner, flatset, comparator_allocator_constructor);
    FATP_RUN_TEST_NS(runner, flatset, range_constructor);
    FATP_RUN_TEST_NS(runner, flatset, initializer_list_constructor);

    // Copy/Move Semantics
    out << "\n" << colors::blue() << "--- Copy/Move Semantics ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, copy_constructor);
    FATP_RUN_TEST_NS(runner, flatset, copy_assignment);
    FATP_RUN_TEST_NS(runner, flatset, self_assignment);
    FATP_RUN_TEST_NS(runner, flatset, move_constructor);
    FATP_RUN_TEST_NS(runner, flatset, move_assignment);

    // Basic Operations
    out << "\n" << colors::blue() << "--- Basic Operations ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, basic_operations);
    FATP_RUN_TEST_NS(runner, flatset, single_element);
    FATP_RUN_TEST_NS(runner, flatset, insert_duplicate);
    FATP_RUN_TEST_NS(runner, flatset, find_operations);
    FATP_RUN_TEST_NS(runner, flatset, count);
    FATP_RUN_TEST_NS(runner, flatset, erase_by_value);
    FATP_RUN_TEST_NS(runner, flatset, erase_by_iterator);
    FATP_RUN_TEST_NS(runner, flatset, erase_range);
    FATP_RUN_TEST_NS(runner, flatset, sorted_order);
    FATP_RUN_TEST_NS(runner, flatset, lower_upper_bound);
    FATP_RUN_TEST_NS(runner, flatset, equal_range);
    FATP_RUN_TEST_NS(runner, flatset, clear);

    // Iterators
    out << "\n" << colors::blue() << "--- Iterators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, iterator_basics);
    FATP_RUN_TEST_NS(runner, flatset, const_iterator);
    FATP_RUN_TEST_NS(runner, flatset, reverse_iterator);

    // Capacity
    out << "\n" << colors::blue() << "--- Capacity ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, reserve_capacity);
    FATP_RUN_TEST_NS(runner, flatset, max_size);

    // Observers
    out << "\n" << colors::blue() << "--- Observers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, key_comp);
    FATP_RUN_TEST_NS(runner, flatset, value_comp);
    FATP_RUN_TEST_NS(runner, flatset, get_allocator);

    // Comparators
    out << "\n" << colors::blue() << "--- Custom Comparators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, custom_comparator);
    FATP_RUN_TEST_NS(runner, flatset, case_insensitive_comparator);
    FATP_RUN_TEST_NS(runner, flatset, case_insensitive_constructor_keeps_first);

    // Modifiers
    out << "\n" << colors::blue() << "--- Modifiers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, equality_operators);
    FATP_RUN_TEST_NS(runner, flatset, swap);
    FATP_RUN_TEST_NS(runner, flatset, emplace);
    FATP_RUN_TEST_NS(runner, flatset, emplace_hint);
    FATP_RUN_TEST_NS(runner, flatset, range_insert);
    FATP_RUN_TEST_NS(runner, flatset, initializer_list_insert);
    FATP_RUN_TEST_NS(runner, flatset, extract);
    FATP_RUN_TEST_NS(runner, flatset, empty_operations);
    FATP_RUN_TEST_NS(runner, flatset, heterogeneous_lookup);
    FATP_RUN_TEST_NS(runner, flatset, merge);

    // RAII & Lifecycle
    out << "\n" << colors::blue() << "--- RAII & Lifecycle ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, lifecycle_tracking);
    FATP_RUN_TEST_NS(runner, flatset, lifecycle_on_clear);
    FATP_RUN_TEST_NS(runner, flatset, lifecycle_on_erase);

    // Exception Safety
    out << "\n" << colors::blue() << "--- Exception Safety ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, exception_safety_insert);

    // Move-Only Types
    out << "\n" << colors::blue() << "--- Move-Only Types ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, move_only_values);

    // Stress Tests
    out << "\n" << colors::blue() << "--- Stress Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, flatset, stress_random_operations);
    FATP_RUN_TEST_NS(runner, flatset, stress_comprehensive);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FlatSet() ? 0 : 1;
}
#endif
