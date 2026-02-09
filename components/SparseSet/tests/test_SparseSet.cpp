/**
 * @file test_SparseSet.cpp
 * @brief Comprehensive unit tests for SparseSet.h
 */
/*
FATP_META:
  meta_version: 1
  component: SparseSet
  file_role: test
  path: components/SparseSet/tests/test_SparseSet.cpp
  layer: Testing
  namespace: fat_p::testing::sparseset
  summary: Unit tests for SparseSet.
  api_stability: in_work
  related:
    headers:
      - include/fat_p/SparseSet.h
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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "SparseSet.h"
#include "FatPTest.h"

namespace fat_p::testing::sparseset
{

// ============================================================================
// Helper Types
// ============================================================================

class LifecycleTracker
{
public:
    inline static std::atomic<int> sConstructCount{0};
    inline static std::atomic<int> sDestructCount{0};
    inline static std::atomic<int> sCopyCount{0};
    inline static std::atomic<int> sMoveCount{0};

    explicit LifecycleTracker(int value = 0)
        : mValue(value)
    {
        ++sConstructCount;
    }

    LifecycleTracker(const LifecycleTracker& other)
        : mValue(other.mValue)
    {
        ++sConstructCount;
        ++sCopyCount;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : mValue(other.mValue)
    {
        ++sConstructCount;
        ++sMoveCount;
        other.mValue = -1;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other)
    {
        mValue = other.mValue;
        ++sCopyCount;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        mValue = other.mValue;
        ++sMoveCount;
        other.mValue = -1;
        return *this;
    }

    ~LifecycleTracker()
    {
        ++sDestructCount;
    }

    static void reset() noexcept
    {
        sConstructCount = 0;
        sDestructCount = 0;
        sCopyCount = 0;
        sMoveCount = 0;
    }

    int mValue;
};

struct MoveOnly
{
    explicit MoveOnly(int value = 0)
        : mValue(value)
    {
    }

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&& other) noexcept
        : mValue(other.mValue)
    {
        other.mValue = -1;
    }

    MoveOnly& operator=(MoveOnly&& other) noexcept
    {
        mValue = other.mValue;
        other.mValue = -1;
        return *this;
    }

    int mValue;
};

struct ThrowOnCopy
{
    explicit ThrowOnCopy(int value = 0) noexcept
        : mValue(value)
    {
    }

    ThrowOnCopy(const ThrowOnCopy&)
    {
        throw std::runtime_error("ThrowOnCopy: copy");
    }

    ThrowOnCopy(ThrowOnCopy&& other) noexcept
        : mValue(other.mValue)
    {
        other.mValue = -1;
    }

    ThrowOnCopy& operator=(const ThrowOnCopy&) = delete;
    ThrowOnCopy& operator=(ThrowOnCopy&&) noexcept = default;

    int mValue;
};

struct ThrowOnMoveAssign
{
    explicit ThrowOnMoveAssign(int value = 0) noexcept
        : mValue(value)
    {
    }

    ThrowOnMoveAssign(const ThrowOnMoveAssign&) = default;

    ThrowOnMoveAssign(ThrowOnMoveAssign&& other) noexcept
        : mValue(other.mValue)
    {
        other.mValue = -1;
    }

    ThrowOnMoveAssign& operator=(const ThrowOnMoveAssign&) = default;

    ThrowOnMoveAssign& operator=(ThrowOnMoveAssign&& other)
    {
        if (sThrowOnNextMoveAssign)
        {
            sThrowOnNextMoveAssign = false;
            throw std::runtime_error("ThrowOnMoveAssign: move assign");
        }

        mValue = other.mValue;
        other.mValue = -1;
        return *this;
    }

    int mValue;
    inline static bool sThrowOnNextMoveAssign = false;
};

// ============================================================================
// SparseSet Basic Operations
// ============================================================================

FATP_TEST_CASE(basic_operations)
{
    SparseSet<uint32_t> set;

    FATP_ASSERT_TRUE(set.empty(), "Should start empty");
    FATP_ASSERT_EQ(set.size(), size_t(0), "Size should be 0");

    FATP_ASSERT_TRUE(set.insert(100), "Should insert new element");
    FATP_ASSERT_FALSE(set.empty(), "Should not be empty after insert");
    FATP_ASSERT_EQ(set.size(), size_t(1), "Size should be 1");
    FATP_ASSERT_TRUE(set.contains(100), "Should contain inserted element");
    FATP_ASSERT_FALSE(set.contains(99), "Should not contain non-inserted element");

    return true;
}

FATP_TEST_CASE(duplicate_insert)
{
    SparseSet<uint32_t> set;

    FATP_ASSERT_TRUE(set.insert(42), "First insert should succeed");
    FATP_ASSERT_FALSE(set.insert(42), "Duplicate insert should return false");
    FATP_ASSERT_EQ(set.size(), size_t(1), "Size should still be 1");
    FATP_ASSERT_TRUE(set.contains(42), "Should still contain element");

    return true;
}

FATP_TEST_CASE(sparse_indices)
{
    SparseSet<uint32_t> set;

    set.insert(10);
    set.insert(1000);
    set.insert(100000);

    FATP_ASSERT_EQ(set.size(), size_t(3), "Should have 3 elements");
    FATP_ASSERT_TRUE(set.contains(10), "Should contain 10");
    FATP_ASSERT_TRUE(set.contains(1000), "Should contain 1000");
    FATP_ASSERT_TRUE(set.contains(100000), "Should contain 100000");

    return true;
}

FATP_TEST_CASE(erase)
{
    SparseSet<uint32_t> set;

    set.insert(1);
    set.insert(2);
    set.insert(3);

    FATP_ASSERT_TRUE(set.erase(2), "Should erase existing element");
    FATP_ASSERT_FALSE(set.contains(2), "Should not contain erased element");
    FATP_ASSERT_EQ(set.size(), size_t(2), "Size should decrease");
    FATP_ASSERT_FALSE(set.erase(2), "Should not erase non-existent element");
    FATP_ASSERT_FALSE(set.erase(999), "Should not erase never-inserted element");

    return true;
}

FATP_TEST_CASE(erase_single_element)
{
    SparseSet<uint32_t> set;

    set.insert(42);
    FATP_ASSERT_EQ(set.size(), size_t(1), "Should have 1 element");

    FATP_ASSERT_TRUE(set.erase(42), "Should erase the only element");
    FATP_ASSERT_TRUE(set.empty(), "Should be empty after erase");
    FATP_ASSERT_FALSE(set.contains(42), "Should not contain erased element");

    return true;
}

FATP_TEST_CASE(erase_last_inserted)
{
    SparseSet<uint32_t> set;

    set.insert(1);
    set.insert(2);
    set.insert(3);

    // Erase the last inserted element (tests fast-path for last element)
    FATP_ASSERT_TRUE(set.erase(3), "Should erase last inserted element");
    FATP_ASSERT_EQ(set.size(), size_t(2), "Size should be 2");
    FATP_ASSERT_TRUE(set.contains(1), "Should still contain 1");
    FATP_ASSERT_TRUE(set.contains(2), "Should still contain 2");
    FATP_ASSERT_FALSE(set.contains(3), "Should not contain 3");

    return true;
}

FATP_TEST_CASE(iteration)
{
    SparseSet<uint32_t> set;

    set.insert(100);
    set.insert(200);
    set.insert(300);

    std::vector<uint32_t> values;
    for (uint32_t value : set)
    {
        values.push_back(value);
    }

    FATP_ASSERT_EQ(values.size(), size_t(3), "Should iterate over all elements");
    FATP_ASSERT_TRUE(std::find(values.begin(), values.end(), 100) != values.end(), "Should find 100");
    FATP_ASSERT_TRUE(std::find(values.begin(), values.end(), 200) != values.end(), "Should find 200");
    FATP_ASSERT_TRUE(std::find(values.begin(), values.end(), 300) != values.end(), "Should find 300");

    return true;
}

FATP_TEST_CASE(reserve)
{
    SparseSet<uint32_t> set;

    FATP_ASSERT_EQ(set.capacity(), size_t(0), "Initial capacity should be 0");

    set.reserve(1000);
    FATP_ASSERT_TRUE(set.capacity() >= 1001, "Capacity should be at least 1001");
    FATP_ASSERT_TRUE(set.empty(), "Should still be empty after reserve");

    set.insert(999);
    FATP_ASSERT_TRUE(set.contains(999), "Should be able to insert up to reserved value");

    return true;
}

FATP_TEST_CASE(operator_bracket)
{
    SparseSet<uint32_t> set;

    set.insert(100);
    set.insert(200);
    set.insert(300);

    std::set<uint32_t> found;
    for (size_t i = 0; i < set.size(); ++i)
    {
        found.insert(set[i]);
    }

    FATP_ASSERT_TRUE(found.count(100) == 1, "Should find 100 via operator[]");
    FATP_ASSERT_TRUE(found.count(200) == 1, "Should find 200 via operator[]");
    FATP_ASSERT_TRUE(found.count(300) == 1, "Should find 300 via operator[]");

    return true;
}

FATP_TEST_CASE(at_bounds_checking)
{
    SparseSet<uint32_t> set;

    set.insert(10);
    set.insert(20);

    FATP_ASSERT_NO_THROW((void)set.at(0), "at(0) should not throw");
    FATP_ASSERT_NO_THROW((void)set.at(1), "at(1) should not throw");
    FATP_ASSERT_THROWS((void)set.at(2), std::out_of_range, "at() should throw out_of_range");

    return true;
}

FATP_TEST_CASE(clear)
{
    SparseSet<uint32_t> set;

    set.insert(1);
    set.insert(2);
    set.insert(3);

    set.clear();

    FATP_ASSERT_TRUE(set.empty(), "Should be empty after clear");
    FATP_ASSERT_EQ(set.size(), size_t(0), "Size should be 0 after clear");
    FATP_ASSERT_FALSE(set.contains(1), "Should not contain 1 after clear");

    FATP_ASSERT_TRUE(set.insert(1), "Should be able to insert after clear");
    FATP_ASSERT_TRUE(set.contains(1), "Should contain 1 after re-insert");

    return true;
}

FATP_TEST_CASE(dense_sparse_accessors)
{
    SparseSet<uint32_t> set;

    set.insert(5);
    set.insert(10);

    const auto& dense = set.dense();
    const auto& sparse = set.sparse();

    FATP_ASSERT_EQ(dense.size(), size_t(2), "Dense should have 2 elements");
    FATP_ASSERT_TRUE(sparse.size() >= 11, "Sparse should be large enough for index 10");

    return true;
}

FATP_TEST_CASE(swap)
{
    SparseSet<uint32_t> a;
    SparseSet<uint32_t> b;

    a.insert(1);
    a.insert(2);

    b.insert(100);

    a.swap(b);

    FATP_ASSERT_EQ(a.size(), size_t(1), "a should have 1 element after swap");
    FATP_ASSERT_TRUE(a.contains(100), "a should contain 100 after swap");

    FATP_ASSERT_EQ(b.size(), size_t(2), "b should have 2 elements after swap");
    FATP_ASSERT_TRUE(b.contains(1), "b should contain 1 after swap");
    FATP_ASSERT_TRUE(b.contains(2), "b should contain 2 after swap");

    return true;
}

// ============================================================================
// SparseSet Copy/Move Semantics
// ============================================================================

FATP_TEST_CASE(copy_constructor)
{
    SparseSet<uint32_t> original;
    original.insert(1);
    original.insert(100);
    original.insert(1000);

    SparseSet<uint32_t> copy(original);

    FATP_ASSERT_EQ(copy.size(), original.size(), "Copy should have same size");
    FATP_ASSERT_TRUE(copy.contains(1), "Copy should contain 1");
    FATP_ASSERT_TRUE(copy.contains(100), "Copy should contain 100");
    FATP_ASSERT_TRUE(copy.contains(1000), "Copy should contain 1000");

    original.erase(1);
    FATP_ASSERT_TRUE(copy.contains(1), "Copy should be independent of original");

    return true;
}

FATP_TEST_CASE(move_constructor)
{
    SparseSet<uint32_t> original;
    original.insert(1);
    original.insert(100);

    SparseSet<uint32_t> moved(std::move(original));

    FATP_ASSERT_EQ(moved.size(), size_t(2), "Moved-to should have 2 elements");
    FATP_ASSERT_TRUE(moved.contains(1), "Moved-to should contain 1");
    FATP_ASSERT_TRUE(moved.contains(100), "Moved-to should contain 100");

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    SparseSet<uint32_t> original;
    original.insert(1);
    original.insert(2);

    SparseSet<uint32_t> assigned;
    assigned.insert(999);

    assigned = original;

    FATP_ASSERT_EQ(assigned.size(), size_t(2), "Assigned should have 2 elements");
    FATP_ASSERT_TRUE(assigned.contains(1), "Assigned should contain 1");
    FATP_ASSERT_TRUE(assigned.contains(2), "Assigned should contain 2");
    FATP_ASSERT_FALSE(assigned.contains(999), "Assigned should not contain old value");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    SparseSet<uint32_t> original;
    original.insert(42);

    SparseSet<uint32_t> assigned;
    assigned.insert(999);

    assigned = std::move(original);

    FATP_ASSERT_EQ(assigned.size(), size_t(1), "Assigned should have 1 element");
    FATP_ASSERT_TRUE(assigned.contains(42), "Assigned should contain 42");
    FATP_ASSERT_FALSE(assigned.contains(999), "Assigned should not contain old value");

    return true;
}

FATP_TEST_CASE(self_assignment)
{
    SparseSet<uint32_t> set;
    set.insert(1);
    set.insert(2);
    set.insert(3);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    set = set;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    FATP_ASSERT_EQ(set.size(), size_t(3), "Self-assignment should preserve size");
    FATP_ASSERT_TRUE(set.contains(1), "Self-assignment should preserve elements");
    FATP_ASSERT_TRUE(set.contains(2), "Self-assignment should preserve elements");
    FATP_ASSERT_TRUE(set.contains(3), "Self-assignment should preserve elements");

    return true;
}

// ============================================================================
// SparseSetWithData Tests
// ============================================================================

FATP_TEST_CASE(with_data_basic)
{
    SparseSetWithData<uint32_t, std::string> set;

    set.insert(1, "one");
    set.insert(2, "two");
    set.insert(3, "three");

    FATP_ASSERT_EQ(set.get(1), std::string("one"), "Should retrieve correct data for 1");
    FATP_ASSERT_EQ(set.get(2), std::string("two"), "Should retrieve correct data for 2");
    FATP_ASSERT_EQ(set.get(3), std::string("three"), "Should retrieve correct data for 3");

    set.erase(2);
    FATP_ASSERT_FALSE(set.contains(2), "Should not contain erased element");
    FATP_ASSERT_EQ(set.get(1), std::string("one"), "Other data should remain valid");
    FATP_ASSERT_EQ(set.get(3), std::string("three"), "Other data should remain valid");

    FATP_ASSERT_EQ(set.dense().size(), set.data().size(), "Dense/data sizes must match");

    return true;
}

FATP_TEST_CASE(with_data_get_throws)
{
    SparseSetWithData<uint32_t, int> set;

    set.insert(1, 100);
    FATP_ASSERT_THROWS((void)set.get(999), std::out_of_range, "get() should throw for missing");

    return true;
}

FATP_TEST_CASE(with_data_try_get)
{
    SparseSetWithData<uint32_t, int> set;

    set.insert(1, 100);

    int* ptr = set.tryGet(1);
    FATP_ASSERT_NOT_NULLPTR(ptr, "tryGet should return valid pointer for existing element");
    FATP_ASSERT_EQ(*ptr, 100, "tryGet should return correct value");

    int* nullPtr = set.tryGet(999);
    FATP_ASSERT_TRUE(nullPtr == nullptr, "tryGet should return nullptr for missing element");

    return true;
}

FATP_TEST_CASE(with_data_move_insert)
{
    SparseSetWithData<uint32_t, MoveOnly> set;

    MoveOnly value(42);
    set.insert(1, std::move(value));

    FATP_ASSERT_TRUE(set.contains(1), "Should contain inserted element");
    FATP_ASSERT_EQ(set.get(1).mValue, 42, "Should have correct value");
    FATP_ASSERT_EQ(value.mValue, -1, "Original should be moved-from");

    return true;
}

FATP_TEST_CASE(with_data_emplace)
{
    SparseSetWithData<uint32_t, std::string> set;

    FATP_ASSERT_TRUE(set.emplace(10, 3, 'x'), "emplace should insert new element");
    FATP_ASSERT_FALSE(set.emplace(10, 3, 'y'), "emplace duplicate should return false");
    FATP_ASSERT_EQ(set.get(10), std::string("xxx"), "emplace should construct correct value");

    return true;
}

FATP_TEST_CASE(with_data_data_at)
{
    SparseSetWithData<uint32_t, std::string> set;

    set.insert(10, "alpha");
    set.insert(20, "beta");

    FATP_ASSERT_NO_THROW((void)set.dataAt(0), "dataAt(0) should not throw");
    FATP_ASSERT_NO_THROW((void)set.dataAt(1), "dataAt(1) should not throw");
    FATP_ASSERT_THROWS((void)set.dataAt(2), std::out_of_range, "dataAt() should throw out_of_range");

    return true;
}

FATP_TEST_CASE(with_data_lifecycle)
{
    LifecycleTracker::reset();

    {
        SparseSetWithData<uint32_t, LifecycleTracker> set;
        set.insert(1, LifecycleTracker(10));
        set.insert(2, LifecycleTracker(20));
        set.erase(1);
    }

    FATP_ASSERT_EQ(LifecycleTracker::sConstructCount.load(), LifecycleTracker::sDestructCount.load(),
                   "All constructed objects should be destructed");

    return true;
}

FATP_TEST_CASE(with_data_iteration)
{
    SparseSetWithData<uint32_t, std::string> set;

    set.insert(100, "hundred");
    set.insert(200, "two-hundred");
    set.insert(300, "three-hundred");

    std::vector<uint32_t> indices;
    for (uint32_t idx : set)
    {
        indices.push_back(idx);
    }

    FATP_ASSERT_EQ(indices.size(), size_t(3), "Should iterate over all indices");

    return true;
}

FATP_TEST_CASE(with_data_swap)
{
    SparseSetWithData<uint32_t, std::string> a;
    SparseSetWithData<uint32_t, std::string> b;

    a.insert(1, "one");
    a.insert(2, "two");

    b.insert(10, "ten");

    a.swap(b);

    FATP_ASSERT_EQ(a.size(), size_t(1), "a should have 1 element after swap");
    FATP_ASSERT_EQ(a.get(10), std::string("ten"), "a should contain 10 after swap");

    FATP_ASSERT_EQ(b.size(), size_t(2), "b should have 2 elements after swap");
    FATP_ASSERT_EQ(b.get(1), std::string("one"), "b should contain 1 after swap");
    FATP_ASSERT_EQ(b.get(2), std::string("two"), "b should contain 2 after swap");

    return true;
}

FATP_TEST_CASE(with_data_insert_exception_safety)
{
    SparseSetWithData<uint32_t, ThrowOnCopy> set;

    const ThrowOnCopy data(123);

    FATP_ASSERT_THROWS(set.insert(42, data), std::runtime_error, "Insert should propagate copy throw");
    FATP_ASSERT_EQ(set.size(), size_t(0), "Failed insert should not change size");
    FATP_ASSERT_FALSE(set.contains(42), "Failed insert should not add element");
    FATP_ASSERT_EQ(set.dense().size(), set.data().size(), "Dense/data sizes must match");

    FATP_ASSERT_TRUE(set.insert(42, ThrowOnCopy(7)), "Move insert should succeed");
    FATP_ASSERT_TRUE(set.contains(42), "Should contain inserted element");
    FATP_ASSERT_EQ(set.size(), size_t(1), "Size should be 1 after successful insert");

    return true;
}

FATP_TEST_CASE(with_data_erase_exception_safety)
{
    SparseSetWithData<uint32_t, ThrowOnMoveAssign> set;

    set.insert(1, ThrowOnMoveAssign(1));
    set.insert(2, ThrowOnMoveAssign(2));

    ThrowOnMoveAssign::sThrowOnNextMoveAssign = true;
    FATP_ASSERT_THROWS(set.erase(1), std::runtime_error, "Erase should propagate move-assign throw");

    // Basic guarantee: container is still valid
    FATP_ASSERT_EQ(set.dense().size(), set.data().size(), "Dense/data sizes must match after failed erase");

    return true;
}

// ============================================================================
// find() Method Tests
// ============================================================================

FATP_TEST_CASE(find_existing)
{
    SparseSet<uint32_t> set;

    set.insert(100);
    set.insert(200);
    set.insert(300);

    auto it = set.find(200);
    FATP_ASSERT_TRUE(it != set.end(), "find should return valid iterator for existing element");
    FATP_ASSERT_EQ(*it, uint32_t(200), "find should return iterator to correct element");

    return true;
}

FATP_TEST_CASE(find_nonexistent)
{
    SparseSet<uint32_t> set;

    set.insert(100);
    set.insert(200);

    auto it = set.find(999);
    FATP_ASSERT_TRUE(it == set.end(), "find should return end() for nonexistent element");

    auto it2 = set.find(150);
    FATP_ASSERT_TRUE(it2 == set.end(), "find should return end() for value in sparse range but not present");

    return true;
}

FATP_TEST_CASE(find_empty_set)
{
    SparseSet<uint32_t> set;

    auto it = set.find(42);
    FATP_ASSERT_TRUE(it == set.end(), "find on empty set should return end()");

    return true;
}

FATP_TEST_CASE(find_const)
{
    SparseSet<uint32_t> set;
    set.insert(42);

    const SparseSet<uint32_t>& constSet = set;
    auto it = constSet.find(42);
    FATP_ASSERT_TRUE(it != constSet.end(), "const find should work");
    FATP_ASSERT_EQ(*it, uint32_t(42), "const find should return correct value");

    return true;
}

FATP_TEST_CASE(with_data_find)
{
    SparseSetWithData<uint32_t, std::string> set;

    set.insert(10, "ten");
    set.insert(20, "twenty");
    set.insert(30, "thirty");

    auto it = set.find(20);
    FATP_ASSERT_TRUE(it != set.end(), "find should return valid iterator");
    FATP_ASSERT_EQ(*it, uint32_t(20), "find should return correct index");

    auto missing = set.find(999);
    FATP_ASSERT_TRUE(missing == set.end(), "find should return end() for missing");

    return true;
}

// ============================================================================
// shrink_to_fit() Method Tests
// ============================================================================

FATP_TEST_CASE(shrink_to_fit_empty)
{
    SparseSet<uint32_t> set;

    set.reserve(10000);
    FATP_ASSERT_TRUE(set.capacity() >= 10001, "Capacity should be at least 10001 after reserve");

    set.shrink_to_fit();
    FATP_ASSERT_EQ(set.capacity(), size_t(0), "Capacity should be 0 after shrink_to_fit on empty set");

    return true;
}

FATP_TEST_CASE(shrink_to_fit_with_elements)
{
    SparseSet<uint32_t> set;

    set.reserve(10000);
    set.insert(5);
    set.insert(50);
    set.insert(100);

    const size_t capacityBefore = set.capacity();
    FATP_ASSERT_TRUE(capacityBefore >= 10001, "Capacity should be large before shrink");

    set.shrink_to_fit();

    // After shrink, capacity should be at least 101 (to hold index 100)
    FATP_ASSERT_TRUE(set.capacity() >= 101, "Capacity should be at least 101 after shrink");
    FATP_ASSERT_TRUE(set.capacity() <= capacityBefore, "Capacity should not increase");

    // Verify all elements still present
    FATP_ASSERT_TRUE(set.contains(5), "Should still contain 5");
    FATP_ASSERT_TRUE(set.contains(50), "Should still contain 50");
    FATP_ASSERT_TRUE(set.contains(100), "Should still contain 100");
    FATP_ASSERT_EQ(set.size(), size_t(3), "Size should still be 3");

    return true;
}

FATP_TEST_CASE(shrink_to_fit_after_erase)
{
    SparseSet<uint32_t> set;

    set.insert(10);
    set.insert(1000);
    set.insert(100);

    FATP_ASSERT_TRUE(set.capacity() >= 1001, "Capacity should cover max value");

    set.erase(1000);
    set.shrink_to_fit();

    // Max value is now 100, so capacity should be around 101
    FATP_ASSERT_TRUE(set.capacity() >= 101, "Capacity should be at least 101");
    FATP_ASSERT_TRUE(set.capacity() < 1001, "Capacity should have shrunk");

    FATP_ASSERT_TRUE(set.contains(10), "Should still contain 10");
    FATP_ASSERT_TRUE(set.contains(100), "Should still contain 100");

    return true;
}

FATP_TEST_CASE(with_data_shrink_to_fit)
{
    SparseSetWithData<uint32_t, std::string> set;

    set.reserve(5000);
    set.insert(10, "ten");
    set.insert(100, "hundred");

    set.shrink_to_fit();

    FATP_ASSERT_TRUE(set.capacity() >= 101, "Capacity should be at least 101");
    FATP_ASSERT_TRUE(set.capacity() < 5001, "Capacity should have shrunk");
    FATP_ASSERT_EQ(set.get(10), std::string("ten"), "Data should be preserved");
    FATP_ASSERT_EQ(set.get(100), std::string("hundred"), "Data should be preserved");

    return true;
}

// ============================================================================
// Edge Cases and Boundary Tests
// ============================================================================

FATP_TEST_CASE(edge_case_uint8_max)
{
    SparseSet<uint8_t> set;

    constexpr uint8_t nearMax = std::numeric_limits<uint8_t>::max() - 1;
    constexpr uint8_t maxVal = std::numeric_limits<uint8_t>::max();

    FATP_ASSERT_TRUE(set.insert(nearMax), "Should insert near-max uint8_t value");
    FATP_ASSERT_TRUE(set.insert(maxVal), "Should insert max uint8_t value");
    FATP_ASSERT_TRUE(set.insert(0), "Should insert 0");

    FATP_ASSERT_TRUE(set.contains(nearMax), "Should contain near-max");
    FATP_ASSERT_TRUE(set.contains(maxVal), "Should contain max");
    FATP_ASSERT_TRUE(set.contains(0), "Should contain 0");
    FATP_ASSERT_EQ(set.size(), size_t(3), "Size should be 3");

    FATP_ASSERT_TRUE(set.erase(maxVal), "Should erase max value");
    FATP_ASSERT_FALSE(set.contains(maxVal), "Should not contain erased max");

    return true;
}

FATP_TEST_CASE(edge_case_uint16_max)
{
    SparseSet<uint16_t> set;

    constexpr uint16_t nearMax = std::numeric_limits<uint16_t>::max() - 1;
    constexpr uint16_t maxVal = std::numeric_limits<uint16_t>::max();

    FATP_ASSERT_TRUE(set.insert(nearMax), "Should insert near-max uint16_t value");
    FATP_ASSERT_TRUE(set.insert(maxVal), "Should insert max uint16_t value");
    FATP_ASSERT_TRUE(set.insert(0), "Should insert 0");

    FATP_ASSERT_TRUE(set.contains(nearMax), "Should contain near-max");
    FATP_ASSERT_TRUE(set.contains(maxVal), "Should contain max");
    FATP_ASSERT_EQ(set.size(), size_t(3), "Size should be 3");

    return true;
}

FATP_TEST_CASE(edge_case_uint64)
{
    SparseSet<uint64_t> set;

    // Test with moderately large values (not max, to avoid memory issues)
    constexpr uint64_t largeVal1 = 1000000ULL;
    constexpr uint64_t largeVal2 = 2000000ULL;

    FATP_ASSERT_TRUE(set.insert(largeVal1), "Should insert large uint64_t value");
    FATP_ASSERT_TRUE(set.insert(largeVal2), "Should insert another large value");
    FATP_ASSERT_TRUE(set.insert(0), "Should insert 0");

    FATP_ASSERT_TRUE(set.contains(largeVal1), "Should contain large value 1");
    FATP_ASSERT_TRUE(set.contains(largeVal2), "Should contain large value 2");
    FATP_ASSERT_TRUE(set.contains(0), "Should contain 0");
    FATP_ASSERT_FALSE(set.contains(500000ULL), "Should not contain non-inserted value");

    FATP_ASSERT_EQ(set.size(), size_t(3), "Size should be 3");

    return true;
}

FATP_TEST_CASE(reserve_overflow)
{
    SparseSet<uint32_t> set;

    FATP_ASSERT_THROWS(
        set.reserve(std::numeric_limits<size_t>::max()),
        std::length_error,
        "reserve(max) should throw length_error"
    );

    // Set should still be usable after failed reserve
    FATP_ASSERT_TRUE(set.insert(42), "Should still be able to insert after failed reserve");
    FATP_ASSERT_TRUE(set.contains(42), "Should contain inserted element");

    return true;
}

FATP_TEST_CASE(with_data_edge_case_uint8)
{
    SparseSetWithData<uint8_t, int> set;

    constexpr uint8_t maxVal = std::numeric_limits<uint8_t>::max();

    FATP_ASSERT_TRUE(set.insert(maxVal, 255), "Should insert max uint8_t with data");
    FATP_ASSERT_TRUE(set.insert(0, 0), "Should insert 0 with data");

    FATP_ASSERT_EQ(set.get(maxVal), 255, "Should retrieve correct data for max");
    FATP_ASSERT_EQ(set.get(0), 0, "Should retrieve correct data for 0");

    return true;
}

// ============================================================================
// Stress/Fuzz Tests
// ============================================================================

FATP_TEST_CASE(stress_random)
{
    SparseSet<uint32_t> set;
    std::set<uint32_t> reference;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<uint32_t> valueDist(0, 9999);
    std::uniform_int_distribution<int> opDist(0, 2);

    for (int i = 0; i < 5000; ++i)
    {
        const uint32_t value = valueDist(rng);
        const int op = opDist(rng);

        if (op == 0)
        {
            const bool setResult = set.insert(value);
            const bool refResult = reference.insert(value).second;
            FATP_ASSERT_EQ(setResult, refResult, "Insert result mismatch");
        }
        else if (op == 1)
        {
            const bool setResult = set.erase(value);
            const bool refResult = reference.erase(value) > 0;
            FATP_ASSERT_EQ(setResult, refResult, "Erase result mismatch");
        }
        else
        {
            const bool setResult = set.contains(value);
            const bool refResult = reference.count(value) > 0;
            FATP_ASSERT_EQ(setResult, refResult, "Contains result mismatch");
        }

        FATP_ASSERT_EQ(set.size(), reference.size(), "Size mismatch after operation");
    }

    for (uint32_t value : set)
    {
        FATP_ASSERT_TRUE(reference.count(value) > 0, "Set contains value not in reference");
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}


} // namespace fat_p::testing::sparseset

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_SparseSet()
{
    FATP_PRINT_HEADER(SPARSE SET)

    TestRunner runner;

    // SparseSet Basic Operations
    FATP_RUN_TEST_NS(runner, sparseset, basic_operations);
    FATP_RUN_TEST_NS(runner, sparseset, duplicate_insert);
    FATP_RUN_TEST_NS(runner, sparseset, sparse_indices);
    FATP_RUN_TEST_NS(runner, sparseset, erase);
    FATP_RUN_TEST_NS(runner, sparseset, erase_single_element);
    FATP_RUN_TEST_NS(runner, sparseset, erase_last_inserted);
    FATP_RUN_TEST_NS(runner, sparseset, iteration);
    FATP_RUN_TEST_NS(runner, sparseset, reserve);
    FATP_RUN_TEST_NS(runner, sparseset, operator_bracket);
    FATP_RUN_TEST_NS(runner, sparseset, at_bounds_checking);
    FATP_RUN_TEST_NS(runner, sparseset, clear);
    FATP_RUN_TEST_NS(runner, sparseset, dense_sparse_accessors);
    FATP_RUN_TEST_NS(runner, sparseset, swap);

    // SparseSet find() Method
    FATP_RUN_TEST_NS(runner, sparseset, find_existing);
    FATP_RUN_TEST_NS(runner, sparseset, find_nonexistent);
    FATP_RUN_TEST_NS(runner, sparseset, find_empty_set);
    FATP_RUN_TEST_NS(runner, sparseset, find_const);

    // SparseSet shrink_to_fit() Method
    FATP_RUN_TEST_NS(runner, sparseset, shrink_to_fit_empty);
    FATP_RUN_TEST_NS(runner, sparseset, shrink_to_fit_with_elements);
    FATP_RUN_TEST_NS(runner, sparseset, shrink_to_fit_after_erase);

    // SparseSet Copy/Move Semantics
    FATP_RUN_TEST_NS(runner, sparseset, copy_constructor);
    FATP_RUN_TEST_NS(runner, sparseset, move_constructor);
    FATP_RUN_TEST_NS(runner, sparseset, copy_assignment);
    FATP_RUN_TEST_NS(runner, sparseset, move_assignment);
    FATP_RUN_TEST_NS(runner, sparseset, self_assignment);

    // SparseSetWithData
    FATP_RUN_TEST_NS(runner, sparseset, with_data_basic);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_get_throws);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_try_get);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_move_insert);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_emplace);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_data_at);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_lifecycle);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_iteration);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_swap);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_insert_exception_safety);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_erase_exception_safety);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_find);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_shrink_to_fit);

    // Edge Cases and Boundary Tests
    FATP_RUN_TEST_NS(runner, sparseset, edge_case_uint8_max);
    FATP_RUN_TEST_NS(runner, sparseset, edge_case_uint16_max);
    FATP_RUN_TEST_NS(runner, sparseset, edge_case_uint64);
    FATP_RUN_TEST_NS(runner, sparseset, reserve_overflow);
    FATP_RUN_TEST_NS(runner, sparseset, with_data_edge_case_uint8);

    // Stress Tests
    FATP_RUN_TEST_NS(runner, sparseset, stress_random);

#ifndef NDEBUG
    std::cout << "\n[Debug build - skipping benchmarks]\n";
#else
    sparseset::run_benchmarks();
#endif

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SparseSet() ? 0 : 1;
}
#endif
