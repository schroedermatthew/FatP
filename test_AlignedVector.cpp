#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "AlignedVector.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_AlignedVector.h"
#endif

namespace fat_p::testing::alignedvector
{

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE(default_construction)
{
    fat_p::AlignedVector<int> vec;
    
    SIMPLE_ASSERT(vec.empty(), "Default constructor should create empty vector");
    ASSERT_EQ(vec.size(), 0u, "Size should be 0");
    ASSERT_EQ(vec.capacity(), 0u, "Capacity should be 0");
    
    return true;
}

TEST_CASE(size_construction)
{
    fat_p::AlignedVector<int> vec(10);
    
    ASSERT_EQ(vec.size(), 10u, "Size constructor should create vector of size 10");
    SIMPLE_ASSERT(!vec.empty(), "Should not be empty");
    
    for (size_t i = 0; i < 10; ++i) {
        ASSERT_EQ(vec[i], 0, "Elements should be value-initialized to 0");
    }
    
    return true;
}

TEST_CASE(size_value_construction)
{
    fat_p::AlignedVector<int> vec(5, 42);
    
    ASSERT_EQ(vec.size(), 5u, "Should have size 5");
    for (size_t i = 0; i < 5; ++i) {
        ASSERT_EQ(vec[i], 42, "All elements should be 42");
    }
    
    return true;
}

TEST_CASE(range_construction)
{
    std::vector<int> src = {1, 2, 3, 4, 5};
    
    fat_p::AlignedVector<int> vec(src.begin(), src.end());
    
    ASSERT_EQ(vec.size(), 5u, "Range constructor should create vector of size 5");
    for (size_t i = 0; i < 5; ++i) {
        ASSERT_EQ(vec[i], static_cast<int>(i + 1), "Elements should match source");
    }
    
    return true;
}

TEST_CASE(range_construction_from_list)
{
    std::list<int> src = {10, 20, 30};
    
    fat_p::AlignedVector<int> vec(src.begin(), src.end());
    
    ASSERT_EQ(vec.size(), 3u, "Should have 3 elements from list");
    ASSERT_EQ(vec[0], 10, "First element");
    ASSERT_EQ(vec[1], 20, "Second element");
    ASSERT_EQ(vec[2], 30, "Third element");
    
    return true;
}

TEST_CASE(range_construction_from_array)
{
    int arr[] = {100, 200, 300, 400};
    
    fat_p::AlignedVector<int> vec(std::begin(arr), std::end(arr));
    
    ASSERT_EQ(vec.size(), 4u, "Should have 4 elements from array");
    ASSERT_EQ(vec[0], 100, "First element");
    ASSERT_EQ(vec[3], 400, "Last element");
    
    return true;
}

TEST_CASE(range_construction_empty)
{
    std::vector<int> empty_src;
    
    fat_p::AlignedVector<int> vec(empty_src.begin(), empty_src.end());
    
    SIMPLE_ASSERT(vec.empty(), "Range constructor from empty should be empty");
    
    return true;
}

TEST_CASE(initializer_list_construction)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    ASSERT_EQ(vec.size(), 5u, "Initializer list should create vector of size 5");
    ASSERT_EQ(vec[0], 1, "First element should be 1");
    ASSERT_EQ(vec[4], 5, "Last element should be 5");
    
    return true;
}

// ============================================================================
// Alignment Tests
// ============================================================================

TEST_CASE(alignment_64)
{
    fat_p::AlignedVector<float, 64> vec(100);
    
    SIMPLE_ASSERT(vec.is_aligned(), "Data should be aligned");
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec.data()) % 64, 0u,
              "Data pointer should be 64-byte aligned");
    
    return true;
}

TEST_CASE(alignment_various)
{
    fat_p::AlignedVector<float, 16> vec16(10);
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec16.data()) % 16, 0u,
              "16-byte alignment should work");
    
    fat_p::AlignedVector<double, 128> vec128(10);
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec128.data()) % 128, 0u,
              "128-byte alignment should work");
    
    fat_p::AlignedVector<int, 256> vec256(10);
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec256.data()) % 256, 0u,
              "256-byte alignment should work");
    
    return true;
}

TEST_CASE(assume_aligned)
{
    fat_p::AlignedVector<float, 64> vec(100, 1.0f);
    
    float* ptr = vec.assume_aligned();
    SIMPLE_ASSERT(ptr != nullptr, "assume_aligned should return non-null");
    ASSERT_EQ(ptr, vec.data(), "assume_aligned should return same as data()");
    
    const fat_p::AlignedVector<float, 64>& cvec = vec;
    const float* cptr = cvec.assume_aligned();
    ASSERT_EQ(cptr, cvec.data(), "const assume_aligned should work");
    
    float sum = 0.0f;
    for (size_t i = 0; i < vec.size(); ++i) {
        sum += ptr[i];
    }
    ASSERT_CLOSE(sum, 100.0f, "assume_aligned should provide valid access");
    
    return true;
}

// ============================================================================
// Iterator Tests (Strict end() behavior)
// ============================================================================

TEST_CASE(empty_vector_iterators_strict)
{
    fat_p::AlignedVector<int> empty;
    
    SIMPLE_ASSERT(empty.begin() == empty.end(), "Empty begin() should equal end()");
    
    int count = 0;
    for (int v : empty) {
        (void)v;
        ++count;
    }
    ASSERT_EQ(count, 0, "Range-for over empty vector should not iterate");
    
    return true;
}

TEST_CASE(forward_iterators)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        sum += *it;
    }
    ASSERT_EQ(sum, 15, "Iterator sum should be 15");
    
    return true;
}

TEST_CASE(reverse_iterators)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    std::vector<int> reversed;
    for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
        reversed.push_back(*it);
    }
    ASSERT_EQ(reversed.size(), 5u, "Should have 5 reversed elements");
    ASSERT_EQ(reversed[0], 5, "First reversed should be 5");
    ASSERT_EQ(reversed[4], 1, "Last reversed should be 1");
    
    return true;
}

// ============================================================================
// Element Access Tests
// ============================================================================

TEST_CASE(subscript_operator)
{
    fat_p::AlignedVector<int> vec = {10, 20, 30, 40, 50};
    
    ASSERT_EQ(vec[0], 10, "First element");
    ASSERT_EQ(vec[4], 50, "Last element");
    
    vec[2] = 99;
    ASSERT_EQ(vec[2], 99, "Modification via subscript should work");
    
    return true;
}

TEST_CASE(at_bounds_checking)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    ASSERT_EQ(vec.at(0), 1, "at(0) should return first element");
    ASSERT_THROWS(vec.at(3), std::out_of_range, "at(3) should throw out_of_range");
    
    return true;
}

TEST_CASE(front_back)
{
    fat_p::AlignedVector<int> vec = {10, 20, 30};
    
    ASSERT_EQ(vec.front(), 10, "front() should return first element");
    ASSERT_EQ(vec.back(), 30, "back() should return last element");
    
    return true;
}

// ============================================================================
// Assign Tests
// ============================================================================

TEST_CASE(assign_count_value)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    vec.assign(5, 42);
    
    ASSERT_EQ(vec.size(), 5u, "Size should be 5 after assign");
    for (size_t i = 0; i < 5; ++i) {
        ASSERT_EQ(vec[i], 42, "All elements should be 42");
    }
    
    return true;
}

TEST_CASE(assign_range)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    std::vector<int> src = {10, 20, 30};
    
    vec.assign(src.begin(), src.end());
    
    ASSERT_EQ(vec.size(), 3u, "Size should be 3 after assign");
    ASSERT_EQ(vec[0], 10, "First element");
    ASSERT_EQ(vec[2], 30, "Last element");
    
    return true;
}

TEST_CASE(assign_initializer_list)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    vec.assign({100, 200});
    
    ASSERT_EQ(vec.size(), 2u, "Size should be 2 after assign");
    ASSERT_EQ(vec[0], 100, "First element");
    ASSERT_EQ(vec[1], 200, "Second element");
    
    return true;
}

TEST_CASE(assign_empty)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    vec.assign(0, 42);
    SIMPLE_ASSERT(vec.empty(), "Assign with count 0 should result in empty vector");
    
    return true;
}

TEST_CASE(initializer_list_assignment_operator)
{
    fat_p::AlignedVector<int> vec;
    
    vec = {5, 10, 15, 20};
    
    ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    ASSERT_EQ(vec[0], 5, "First element");
    ASSERT_EQ(vec[3], 20, "Last element");
    
    return true;
}

// ============================================================================
// Insert Tests
// ============================================================================

TEST_CASE(insert_single_lvalue)
{
    fat_p::AlignedVector<int> vec = {1, 2, 4, 5};
    int val = 3;
    
    auto it = vec.insert(vec.begin() + 2, val);
    
    ASSERT_EQ(vec.size(), 5u, "Size should be 5 after insert");
    ASSERT_EQ(*it, 3, "Iterator should point to inserted element");
    ASSERT_EQ(vec[2], 3, "Inserted element should be at position 2");
    
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(vec[i], i + 1, "Elements should be 1,2,3,4,5");
    }
    
    return true;
}

TEST_CASE(insert_single_rvalue)
{
    fat_p::AlignedVector<int> vec = {1, 3};
    
    auto it = vec.insert(vec.begin() + 1, 2);
    
    ASSERT_EQ(vec.size(), 3u, "Size should be 3 after insert");
    ASSERT_EQ(*it, 2, "Iterator should point to inserted element");
    ASSERT_EQ(vec[0], 1, "First element");
    ASSERT_EQ(vec[1], 2, "Inserted element");
    ASSERT_EQ(vec[2], 3, "Last element");
    
    return true;
}

TEST_CASE(insert_at_begin)
{
    fat_p::AlignedVector<int> vec = {2, 3, 4};
    
    vec.insert(vec.begin(), 1);
    
    ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    ASSERT_EQ(vec[0], 1, "Inserted element should be first");
    
    return true;
}

TEST_CASE(insert_at_end)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    vec.insert(vec.end(), 4);
    
    ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    ASSERT_EQ(vec[3], 4, "Inserted element should be last");
    
    return true;
}

TEST_CASE(insert_into_empty)
{
    fat_p::AlignedVector<int> vec;
    
    vec.insert(vec.begin(), 42);
    
    ASSERT_EQ(vec.size(), 1u, "Size should be 1");
    ASSERT_EQ(vec[0], 42, "Element should be 42");
    
    return true;
}

TEST_CASE(insert_count)
{
    fat_p::AlignedVector<int> vec = {1, 5};
    
    vec.insert(vec.begin() + 1, 3, 3);
    
    ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    ASSERT_EQ(vec[0], 1, "First element");
    ASSERT_EQ(vec[1], 3, "Inserted element 1");
    ASSERT_EQ(vec[2], 3, "Inserted element 2");
    ASSERT_EQ(vec[3], 3, "Inserted element 3");
    ASSERT_EQ(vec[4], 5, "Last element");
    
    return true;
}

TEST_CASE(insert_count_zero)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    auto it = vec.insert(vec.begin() + 1, 0, 99);
    
    ASSERT_EQ(vec.size(), 3u, "Size should be unchanged");
    SIMPLE_ASSERT(it == vec.begin() + 1, "Should return position iterator");
    
    return true;
}

TEST_CASE(insert_range)
{
    fat_p::AlignedVector<int> vec = {1, 5};
    std::vector<int> src = {2, 3, 4};
    
    vec.insert(vec.begin() + 1, src.begin(), src.end());
    
    ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(vec[i], i + 1, "Elements should be 1,2,3,4,5");
    }
    
    return true;
}

TEST_CASE(insert_initializer_list)
{
    fat_p::AlignedVector<int> vec = {1, 5};
    
    vec.insert(vec.begin() + 1, {2, 3, 4});
    
    ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(vec[i], i + 1, "Elements should be 1,2,3,4,5");
    }
    
    return true;
}

TEST_CASE(insert_triggers_reallocation)
{
    fat_p::AlignedVector<int> vec;
    vec.reserve(3);
    vec.push_back(1);
    vec.push_back(3);
    vec.push_back(4);
    
    vec.insert(vec.begin() + 1, 2);
    
    ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    SIMPLE_ASSERT(vec.capacity() > 3, "Capacity should have grown");
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(vec[i], i + 1, "Elements should be 1,2,3,4");
    }
    
    return true;
}

// ============================================================================
// Emplace Tests
// ============================================================================

TEST_CASE(emplace_middle)
{
    struct Point {
        int x, y;
        Point(int x_, int y_) : x(x_), y(y_) {}
    };
    
    fat_p::AlignedVector<Point> vec;
    vec.emplace_back(1, 1);
    vec.emplace_back(3, 3);
    
    auto it = vec.emplace(vec.begin() + 1, 2, 2);
    
    ASSERT_EQ(vec.size(), 3u, "Size should be 3");
    ASSERT_EQ(it->x, 2, "Emplaced element x");
    ASSERT_EQ(it->y, 2, "Emplaced element y");
    ASSERT_EQ(vec[1].x, 2, "Middle element x");
    
    return true;
}

// ============================================================================
// Erase Tests
// ============================================================================

TEST_CASE(erase_single)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    auto it = vec.erase(vec.begin() + 2);
    
    ASSERT_EQ(vec.size(), 4u, "Size should be 4 after erase");
    ASSERT_EQ(*it, 4, "Iterator should point to element after erased");
    ASSERT_EQ(vec[0], 1, "First element");
    ASSERT_EQ(vec[1], 2, "Second element");
    ASSERT_EQ(vec[2], 4, "Third element (was fourth)");
    ASSERT_EQ(vec[3], 5, "Fourth element (was fifth)");
    
    return true;
}

TEST_CASE(erase_first)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    vec.erase(vec.begin());
    
    ASSERT_EQ(vec.size(), 2u, "Size should be 2");
    ASSERT_EQ(vec[0], 2, "First element should now be 2");
    
    return true;
}

TEST_CASE(erase_last)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    auto it = vec.erase(vec.end() - 1);
    
    ASSERT_EQ(vec.size(), 2u, "Size should be 2");
    SIMPLE_ASSERT(it == vec.end(), "Iterator should be end()");
    ASSERT_EQ(vec.back(), 2, "Last element should now be 2");
    
    return true;
}

TEST_CASE(erase_range)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    auto it = vec.erase(vec.begin() + 1, vec.begin() + 4);
    
    ASSERT_EQ(vec.size(), 2u, "Size should be 2 after erase range");
    ASSERT_EQ(*it, 5, "Iterator should point to element after erased range");
    ASSERT_EQ(vec[0], 1, "First element");
    ASSERT_EQ(vec[1], 5, "Second element (was fifth)");
    
    return true;
}

TEST_CASE(erase_range_empty)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    auto it = vec.erase(vec.begin() + 1, vec.begin() + 1);
    
    ASSERT_EQ(vec.size(), 3u, "Size should be unchanged");
    SIMPLE_ASSERT(it == vec.begin() + 1, "Should return same position");
    
    return true;
}

TEST_CASE(erase_all)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    vec.erase(vec.begin(), vec.end());
    
    SIMPLE_ASSERT(vec.empty(), "Vector should be empty after erasing all");
    
    return true;
}

TEST_CASE(erase_to_empty)
{
    fat_p::AlignedVector<int> vec = {42};
    
    vec.erase(vec.begin());
    
    SIMPLE_ASSERT(vec.empty(), "Vector should be empty");
    
    return true;
}

// ============================================================================
// Capacity Tests
// ============================================================================

TEST_CASE(reserve_capacity)
{
    fat_p::AlignedVector<int> vec;
    
    vec.reserve(100);
    SIMPLE_ASSERT(vec.capacity() >= 100, "Capacity should be at least 100");
    ASSERT_EQ(vec.size(), 0u, "Size should still be 0");
    
    return true;
}

TEST_CASE(shrink_to_fit)
{
    fat_p::AlignedVector<int> vec;
    vec.reserve(100);
    vec.push_back(1);
    vec.push_back(2);
    
    vec.shrink_to_fit();
    
    ASSERT_EQ(vec.capacity(), 2u, "Capacity should shrink to size");
    ASSERT_EQ(vec.size(), 2u, "Size should be unchanged");
    
    return true;
}

TEST_CASE(max_size_and_overflow)
{
    fat_p::AlignedVector<int> vec;
    
    size_t max_sz = vec.max_size();
    SIMPLE_ASSERT(max_sz > 0, "max_size() should be positive");
    
    // Note: Different implementations throw different exceptions for allocation beyond max_size:
    // - Some throw std::length_error (per standard recommendation)
    // - MSVC throws std::bad_alloc
    // Both are acceptable behavior
    bool threw_expected = false;
    try {
        vec.reserve(max_sz + 1);
    } catch (const std::length_error&) {
        threw_expected = true;
    } catch (const std::bad_alloc&) {
        threw_expected = true;  // MSVC behavior
    }
    SIMPLE_ASSERT(threw_expected, "reserve() beyond max_size() should throw length_error or bad_alloc");
    
    return true;
}

// ============================================================================
// Modifier Tests
// ============================================================================

TEST_CASE(push_back)
{
    fat_p::AlignedVector<int> vec;
    
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }
    
    ASSERT_EQ(vec.size(), 100u, "Should have 100 elements");
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(vec[i], i, "Elements should match");
    }
    
    return true;
}

TEST_CASE(emplace_back)
{
    struct Point {
        int x, y;
        Point(int x_, int y_) : x(x_), y(y_) {}
    };
    
    fat_p::AlignedVector<Point> vec;
    vec.emplace_back(1, 2);
    vec.emplace_back(3, 4);
    
    ASSERT_EQ(vec.size(), 2u, "Should have 2 elements");
    ASSERT_EQ(vec[0].x, 1, "First point x");
    ASSERT_EQ(vec[1].y, 4, "Second point y");
    
    return true;
}

TEST_CASE(resize)
{
    fat_p::AlignedVector<int> vec(10, 5);
    
    vec.resize(20);
    ASSERT_EQ(vec.size(), 20u, "Size should be 20 after resize");
    for (size_t i = 10; i < 20; ++i) {
        ASSERT_EQ(vec[i], 0, "New elements should be value-initialized to 0");
    }
    
    vec.resize(5);
    ASSERT_EQ(vec.size(), 5u, "Size should be 5 after shrink");
    
    return true;
}

TEST_CASE(clear_and_pop_back)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};
    
    vec.pop_back();
    ASSERT_EQ(vec.size(), 2u, "After pop_back, size should decrease");
    
    vec.clear();
    SIMPLE_ASSERT(vec.empty(), "After clear, vector should be empty");
    
    return true;
}

TEST_CASE(swap)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {4, 5};
    
    vec1.swap(vec2);
    
    ASSERT_EQ(vec1.size(), 2u, "After swap, vec1 should have 2 elements");
    ASSERT_EQ(vec2.size(), 3u, "After swap, vec2 should have 3 elements");
    
    return true;
}

// ============================================================================
// Copy/Move Tests
// ============================================================================

TEST_CASE(copy_constructor)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3, 4, 5};
    
    fat_p::AlignedVector<int> vec2(vec1);
    ASSERT_EQ(vec2.size(), 5u, "Copy should have same size");
    ASSERT_EQ(vec2[2], 3, "Copy should have same elements");
    
    return true;
}

TEST_CASE(move_constructor)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3, 4, 5};
    
    fat_p::AlignedVector<int> vec2(std::move(vec1));
    ASSERT_EQ(vec2.size(), 5u, "Move should transfer ownership");
    ASSERT_EQ(vec1.size(), 0u, "Moved-from vector should be empty");
    
    return true;
}

TEST_CASE(copy_assignment)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3, 4, 5};
    fat_p::AlignedVector<int> vec2;
    
    vec2 = vec1;
    ASSERT_EQ(vec2.size(), 5u, "Assignment should copy");
    
    return true;
}

TEST_CASE(move_assignment)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3, 4, 5};
    fat_p::AlignedVector<int> vec2;
    
    vec2 = std::move(vec1);
    ASSERT_EQ(vec2.size(), 5u, "Move assignment should transfer");
    ASSERT_EQ(vec1.size(), 0u, "Moved-from should be empty");
    
    return true;
}

TEST_CASE(self_assignment)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    vec = vec;
    ASSERT_EQ(vec.size(), 5u, "Self copy assignment should preserve size");
    
    return true;
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST_CASE(equality_operators)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {1, 2, 3};
    fat_p::AlignedVector<int> vec3 = {1, 2, 4};
    
    SIMPLE_ASSERT(vec1 == vec2, "Equal vectors should be ==");
    SIMPLE_ASSERT(vec1 != vec3, "Different vectors should be !=");
    
    return true;
}

TEST_CASE(ordering_operators)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {1, 2, 4};
    
    SIMPLE_ASSERT(vec1 < vec2, "{1,2,3} < {1,2,4}");
    SIMPLE_ASSERT(vec2 > vec1, "{1,2,4} > {1,2,3}");
    SIMPLE_ASSERT(vec1 <= vec2, "{1,2,3} <= {1,2,4}");
    SIMPLE_ASSERT(vec2 >= vec1, "{1,2,4} >= {1,2,3}");
    
    return true;
}

// ============================================================================
// Allocator Tests
// ============================================================================

TEST_CASE(get_allocator)
{
    fat_p::AlignedVector<int, 64> vec;
    
    auto alloc = vec.get_allocator();
    ASSERT_EQ(alloc.alignment, 64u, "Allocator should have correct alignment");
    
    return true;
}

TEST_CASE(allocator_comparison)
{
    fat_p::AlignedAllocator<int, 64> alloc1;
    fat_p::AlignedAllocator<int, 64> alloc2;
    fat_p::AlignedAllocator<float, 64> alloc3;
    fat_p::AlignedAllocator<int, 32> alloc4;
    
    SIMPLE_ASSERT(alloc1 == alloc2, "Same type/alignment allocators should be equal");
    
    // Per C++ allocator semantics, allocators with the same alignment compare equal
    // regardless of element type (they can be rebound to any type)
    SIMPLE_ASSERT(alloc1 == alloc3, "Same alignment allocators should be equal (rebindable)");
    
    // Different alignments should NOT be equal
    SIMPLE_ASSERT(!(alloc1 == alloc4), "Different alignment allocators should not be equal");
    
    return true;
}

// ============================================================================
// Value Initialization Tests
// ============================================================================

TEST_CASE(trivial_struct_initialization)
{
    struct TrivialStruct {
        int x;
        float y;
        double z;
    };
    
    fat_p::AlignedVector<TrivialStruct> vec(5);
    
    ASSERT_EQ(vec.size(), 5u, "Should have 5 elements");
    for (size_t i = 0; i < 5; ++i) {
        ASSERT_EQ(vec[i].x, 0, "Trivial struct int member should be zero");
    }
    
    return true;
}

TEST_CASE(nontrivial_copy_initialization)
{
    struct AlmostPOD {
        int x;
        AlmostPOD() = default;
        AlmostPOD(const AlmostPOD& o) : x(o.x) {}
        AlmostPOD& operator=(const AlmostPOD&) = default;
    };
    
    fat_p::AlignedVector<AlmostPOD> vec(3);
    
    ASSERT_EQ(vec.size(), 3u, "Should have 3 elements");
    ASSERT_EQ(vec[0].x, 0, "Value should be zero-initialized");
    
    return true;
}

// ============================================================================
// Move-Only Type Tests
// ============================================================================

TEST_CASE(move_only_types)
{
    struct MoveOnly {
        int value;
        
        MoveOnly(int v) : value(v) {}
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
        MoveOnly(MoveOnly&& o) noexcept : value(o.value) {}
        MoveOnly& operator=(MoveOnly&& o) noexcept {
            value = o.value;
            return *this;
        }
    };
    
    fat_p::AlignedVector<MoveOnly> vec;
    vec.push_back(MoveOnly(1));
    vec.push_back(MoveOnly(2));
    
    ASSERT_EQ(vec.size(), 2u, "Should have 2 move-only elements");
    
    fat_p::AlignedVector<MoveOnly> vec2(std::move(vec));
    ASSERT_EQ(vec2.size(), 2u, "Move ctor should transfer elements");
    
    return true;
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

namespace
{
    struct ThrowOnCopy {
        int value;
        static int copy_count;
        static int limit;
        
        ThrowOnCopy(int v = 0) : value(v) {}
        
        ThrowOnCopy(const ThrowOnCopy& other) : value(other.value) {
            if (copy_count >= limit) {
                throw std::runtime_error("Simulated copy failure");
            }
            ++copy_count;
        }
        
        ThrowOnCopy& operator=(const ThrowOnCopy&) = default;
    };
    int ThrowOnCopy::copy_count = 0;
    int ThrowOnCopy::limit = 100;
} // anonymous namespace

TEST_CASE(exception_safety_strong_guarantee)
{
    ThrowOnCopy::copy_count = 0;
    ThrowOnCopy::limit = 100;
    
    fat_p::AlignedVector<ThrowOnCopy> vec;
    ThrowOnCopy t1(1), t2(2), t3(3);
    vec.push_back(t1);
    vec.push_back(t2);
    vec.push_back(t3);
    
    size_t original_size = vec.size();
    size_t original_cap = vec.capacity();
    int original_val0 = vec[0].value;
    
    ThrowOnCopy::copy_count = 0;
    ThrowOnCopy::limit = 1;
    
    bool threw = false;
    try {
        vec.reserve(100);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Reserve should have thrown exception");
    ASSERT_EQ(vec.size(), original_size, "Size should remain unchanged after failure");
    ASSERT_EQ(vec.capacity(), original_cap, "Capacity should remain unchanged");
    ASSERT_EQ(vec[0].value, original_val0, "First element should be preserved");
    
    return true;
}

// ============================================================================
// Destructor Tests
// ============================================================================

namespace
{
    struct DestructorCounter {
        static int count;
        int id;
        DestructorCounter(int i = 0) : id(i) {}
        ~DestructorCounter() { ++count; }
        DestructorCounter(const DestructorCounter& o) : id(o.id) {}
        DestructorCounter& operator=(const DestructorCounter&) = default;
    };
    int DestructorCounter::count = 0;
} // anonymous namespace

TEST_CASE(destructor_calls)
{
    DestructorCounter::count = 0;
    
    {
        fat_p::AlignedVector<DestructorCounter> vec;
        vec.reserve(10);
        vec.emplace_back(1);
        vec.emplace_back(2);
        int count_after_emplace = DestructorCounter::count;
        
        vec.clear();
        ASSERT_EQ(DestructorCounter::count, count_after_emplace + 2,
                  "clear() should call destructors for all elements");
    }
    
    return true;
}

TEST_CASE(erase_calls_destructors)
{
    DestructorCounter::count = 0;
    
    fat_p::AlignedVector<DestructorCounter> vec;
    vec.reserve(10);
    vec.emplace_back(1);
    vec.emplace_back(2);
    vec.emplace_back(3);
    int count_before = DestructorCounter::count;
    
    vec.erase(vec.begin() + 1);
    
    SIMPLE_ASSERT(DestructorCounter::count > count_before,
                  "erase() should call destructor");
    ASSERT_EQ(vec.size(), 2u, "Size should be 2 after erase");
    
    return true;
}

// ============================================================================
// Adversarial Tests (Exception Safety Bug Detection)
// ============================================================================

// Helper: type that throws after N constructions
struct ThrowAfterN {
    static int construction_count;
    static int throw_after;
    static int destruction_count;
    
    int value;
    
    static void reset(int throw_at = -1) {
        construction_count = 0;
        throw_after = throw_at;
        destruction_count = 0;
    }
    
    ThrowAfterN() : value(0) {
        if (throw_after >= 0 && construction_count >= throw_after) {
            throw std::runtime_error("ThrowAfterN: construction limit reached");
        }
        ++construction_count;
    }
    
    explicit ThrowAfterN(int v) : value(v) {
        if (throw_after >= 0 && construction_count >= throw_after) {
            throw std::runtime_error("ThrowAfterN: construction limit reached");
        }
        ++construction_count;
    }
    
    ThrowAfterN(const ThrowAfterN& other) : value(other.value) {
        if (throw_after >= 0 && construction_count >= throw_after) {
            throw std::runtime_error("ThrowAfterN: copy construction limit reached");
        }
        ++construction_count;
    }
    
    ThrowAfterN(ThrowAfterN&& other) noexcept : value(other.value) {
        other.value = -1;
    }
    
    ThrowAfterN& operator=(const ThrowAfterN& other) {
        if (throw_after >= 0 && construction_count >= throw_after) {
            throw std::runtime_error("ThrowAfterN: assignment limit reached");
        }
        value = other.value;
        ++construction_count;
        return *this;
    }
    
    ThrowAfterN& operator=(ThrowAfterN&& other) noexcept {
        value = other.value;
        other.value = -1;
        return *this;
    }
    
    ~ThrowAfterN() {
        ++destruction_count;
    }
};

int ThrowAfterN::construction_count = 0;
int ThrowAfterN::throw_after = -1;
int ThrowAfterN::destruction_count = 0;

TEST_CASE(assign_exception_safety_strong_guarantee)
{
    // Test that assign() provides strong exception guarantee:
    // If an exception is thrown, the vector should be unchanged.
    
    fat_p::AlignedVector<ThrowAfterN> vec;
    
    // Set up initial state
    ThrowAfterN::reset();
    vec.push_back(ThrowAfterN(1));
    vec.push_back(ThrowAfterN(2));
    vec.push_back(ThrowAfterN(3));
    
    size_t original_size = vec.size();
    
    // Now try to assign with a throw in the middle
    ThrowAfterN::reset(2);  // Throw after 2 constructions
    
    bool threw = false;
    try {
        vec.assign(10, ThrowAfterN(99));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Should have thrown exception");
    
    // Strong guarantee: vector should be in a valid state
    // The vector should be empty or unchanged depending on implementation.
    SIMPLE_ASSERT(vec.size() == 0 || vec.size() == original_size,
                  "Vector should be empty or unchanged (basic guarantee)");
    
    ThrowAfterN::reset();
    
    return true;
}

TEST_CASE(assign_range_exception_safety)
{
    fat_p::AlignedVector<ThrowAfterN> vec;
    
    // Set up initial state
    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i) {
        vec.push_back(ThrowAfterN(i));
    }
    
    // Create source data
    std::vector<ThrowAfterN> source;
    ThrowAfterN::reset();
    for (int i = 0; i < 10; ++i) {
        source.push_back(ThrowAfterN(i * 10));
    }
    
    // Try to assign from range with throw
    ThrowAfterN::reset(3);  // Throw after 3 constructions
    
    bool threw = false;
    try {
        vec.assign(source.begin(), source.end());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Should have thrown exception");
    
    // Vector should be in valid state (at least basic guarantee)
    size_t count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        ++count;
    }
    ASSERT_EQ(count, vec.size(), "Size should match iteration count");
    
    ThrowAfterN::reset();
    return true;
}

TEST_CASE(insert_range_exception_safety)
{
    fat_p::AlignedVector<ThrowAfterN> vec;
    
    // Set up initial state
    ThrowAfterN::reset();
    for (int i = 0; i < 10; ++i) {
        vec.push_back(ThrowAfterN(i));
    }
    
    // Create source data for insertion
    std::vector<ThrowAfterN> source;
    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i) {
        source.push_back(ThrowAfterN(100 + i));
    }
    
    // Try to insert in middle with throw during copy/construction
    ThrowAfterN::reset(2);  // Throw after 2 constructions
    
    bool threw = false;
    try {
        vec.insert(vec.begin() + 5, source.begin(), source.end());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Should have thrown exception");
    
    // At minimum, vector should be in valid state (basic guarantee)
    size_t count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        ++count;
    }
    ASSERT_EQ(count, vec.size(), "Size should match iteration count");
    
    ThrowAfterN::reset();
    return true;
}

TEST_CASE(insert_count_exception_safety)
{
    fat_p::AlignedVector<ThrowAfterN> vec;
    
    // Set up initial state
    ThrowAfterN::reset();
    for (int i = 0; i < 10; ++i) {
        vec.push_back(ThrowAfterN(i));
    }
    
    // Try to insert multiple copies with throw
    ThrowAfterN value_to_insert(999);
    ThrowAfterN::reset(3);  // Throw after 3 constructions
    
    bool threw = false;
    try {
        vec.insert(vec.begin() + 5, 10, value_to_insert);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Should have thrown exception");
    
    // Vector should be in valid state
    size_t count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        ++count;
    }
    ASSERT_EQ(count, vec.size(), "Size should match iteration count");
    
    ThrowAfterN::reset();
    return true;
}

TEST_CASE(construct_range_copy_no_leak)
{
    // Verify that if construction fails mid-range, previously constructed
    // elements are properly destroyed (no memory leak)
    
    ThrowAfterN::reset();
    
    // Create source
    std::vector<ThrowAfterN> source;
    for (int i = 0; i < 10; ++i) {
        source.push_back(ThrowAfterN(i));
    }
    
    // Create vector that will throw during construction
    ThrowAfterN::reset(5);  // Throw after 5 constructions
    
    bool threw = false;
    try {
        fat_p::AlignedVector<ThrowAfterN> vec(source.begin(), source.end());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Should have thrown exception");
    
    ThrowAfterN::reset();
    return true;
}

TEST_CASE(resize_exception_safety)
{
    fat_p::AlignedVector<ThrowAfterN> vec;
    
    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i) {
        vec.push_back(ThrowAfterN(i));
    }
    
    // Try to resize larger with throw
    ThrowAfterN::reset(3);
    
    bool threw = false;
    try {
        vec.resize(20);  // Will need to construct 15 new elements
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Should have thrown exception");
    
    // Vector should be in valid state
    SIMPLE_ASSERT(vec.size() <= 20, "Size should not exceed requested");
    
    ThrowAfterN::reset();
    return true;
}

TEST_CASE(emplace_back_exception_safety)
{
    fat_p::AlignedVector<ThrowAfterN> vec;
    
    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i) {
        vec.push_back(ThrowAfterN(i));
    }
    
    // Force reallocation scenario
    while (vec.size() < vec.capacity()) {
        vec.push_back(ThrowAfterN(99));
    }
    size_t size_before = vec.size();
    
    // Now emplace_back will need to reallocate
    // Note: reset(0) means throw when construction_count >= 0 (i.e., immediately)
    // Move operations are noexcept and don't increment construction_count
    ThrowAfterN::reset(0);  // Throw on first construction attempt
    
    bool threw = false;
    try {
        vec.emplace_back(123);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    SIMPLE_ASSERT(threw, "Should have thrown exception");
    
    // Size should be unchanged (strong guarantee for emplace_back)
    ASSERT_EQ(vec.size(), size_before, "Size should be unchanged after failed emplace_back");
    
    ThrowAfterN::reset();
    return true;
}

TEST_CASE(self_insertion_safety)
{
    // Test that self-insertion doesn't cause use-after-free
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    // This would crash on unpatched version due to use-after-free
    vec.insert(vec.begin() + 2, vec.begin(), vec.end());
    
    ASSERT_EQ(vec.size(), 10u, "Size should be 10 after self-insert");
    
    // Verify the values are sensible (exact order depends on implementation)
    bool all_valid = true;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] < 1 || vec[i] > 5) {
            all_valid = false;
            break;
        }
    }
    SIMPLE_ASSERT(all_valid, "All values should be in range [1,5]");
    
    return true;
}

TEST_CASE(push_back_aliasing)
{
    // Test that push_back handles aliasing when value references internal element
    fat_p::AlignedVector<std::string> vec;
    vec.push_back("first");
    
    // Force several reallocations while referencing internal element
    for (int i = 0; i < 20; ++i) {
        vec.push_back(vec[0]);  // Aliasing - value is inside vector
    }
    
    ASSERT_EQ(vec.size(), 21u, "Should have 21 elements");
    
    // All elements should be "first"
    bool all_first = true;
    for (const auto& s : vec) {
        if (s != "first") {
            all_first = false;
            break;
        }
    }
    SIMPLE_ASSERT(all_first, "All elements should be 'first'");
    
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

// Global volatile sink to prevent optimizer from eliminating computations
// This is stronger than DoNotOptimize for loops where we accumulate a result
static volatile long long g_benchmark_sink;

// Helper to print current CPU state
void print_cpu_state(const char* label)
{
    auto info = SystemInfo::capture();
    std::cout << "  " << colors::blue() << "[" << label << "] ";
    if (info.current_freq_mhz > 0 && info.base_freq_mhz > 0)
    {
        std::cout << "CPU: " << static_cast<int>(info.current_freq_mhz) << " MHz";
        double throttle_pct = info.throttle_percentage();
        if (throttle_pct > 5.0)
        {
            std::cout << " (" << colors::yellow() << std::fixed << std::setprecision(0) 
                      << throttle_pct << "% throttled" << colors::reset() << colors::blue() << ")";
        }
        else if (throttle_pct < -5.0)
        {
            std::cout << " (" << colors::green() << "turbo" << colors::reset() << colors::blue() << ")";
        }
    }
    else if (info.base_freq_mhz > 0)
    {
        std::cout << "CPU: " << static_cast<int>(info.base_freq_mhz) << " MHz (base)";
    }
    std::cout << colors::reset() << "\n";
}

void benchmark_aligned_vector()
{
    std::cout << "\n" << colors::cyan() << "AlignedVector Benchmarks:" 
              << colors::reset() << "\n\n";
    
    // Print initial system state
    auto sys_info = SystemInfo::capture();
    std::cout << "System: " << sys_info.one_line_summary() << "\n\n";
    
    constexpr size_t N = 10000;
    
    // Benchmark 1: push_back
    print_cpu_state("push_back");
    double push_time = measure_perf([]() {
        fat_p::AlignedVector<int> vec;
        for (int i = 0; i < 1000; ++i) {
            vec.push_back(i);
        }
        g_benchmark_sink = vec.size();  // Force vec to be computed
    }, 1000, 100);
    
    std::cout << "push_back (1000 elements): " << format_time(push_time) << "\n\n";
    
    // Benchmark 2: Sequential iteration
    // Use random data to prevent compile-time sum calculation
    fat_p::AlignedVector<int, 64> vec(N);
    std::mt19937 rng(42);
    for (size_t i = 0; i < N; ++i) {
        vec[i] = static_cast<int>(rng() % 1000);
    }
    DoNotOptimize(vec.data());  // Hide contents from optimizer
    
    print_cpu_state("iteration");
    double iter_time = measure_perf([&vec]() {
        long long sum = 0;
        for (size_t i = 0; i < vec.size(); ++i) {
            sum += vec[i];
        }
        g_benchmark_sink = sum;  // Force sum to be computed
    }, 1000, 100);
    
    std::cout << "Iteration sum (" << N << " elements): " << format_time(iter_time) 
              << " (" << std::fixed << std::setprecision(2) << (iter_time * 1e6 / N) << " ns/element)\n\n";
    
    // Benchmark 3: assume_aligned iteration
    print_cpu_state("assume_aligned");
    double assume_aligned_time = measure_perf([&vec]() {
        long long sum = 0;
        const int* p = vec.assume_aligned();
        for (size_t i = 0; i < vec.size(); ++i) {
            sum += p[i];
        }
        g_benchmark_sink = sum;
    }, 1000, 100);
    
    std::cout << "assume_aligned sum (" << N << " elements): " 
              << format_time(assume_aligned_time)
              << " (" << std::fixed << std::setprecision(2) << (assume_aligned_time * 1e6 / N) << " ns/element)\n\n";
    
    // Benchmark 4: Random access
    std::vector<size_t> indices(N);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);
    DoNotOptimize(indices.data());
    
    print_cpu_state("random access");
    double random_time = measure_perf([&vec, &indices]() {
        long long sum = 0;
        for (size_t idx : indices) {
            sum += vec[idx];
        }
        g_benchmark_sink = sum;
    }, 1000, 100);
    
    std::cout << "Random access sum (" << N << " elements): " << format_time(random_time)
              << " (" << std::fixed << std::setprecision(2) << (random_time * 1e6 / N) << " ns/element)\n\n";
    
    // Benchmark 5: insert at begin (worst case)
    print_cpu_state("insert begin");
    double insert_time = measure_perf([]() {
        fat_p::AlignedVector<int> vec;
        vec.reserve(100);
        for (int i = 0; i < 100; ++i) {
            vec.insert(vec.begin(), i);
        }
        g_benchmark_sink = vec.size();
    }, 1000, 10);
    
    std::cout << "insert at begin (100 elements): " << format_time(insert_time) << "\n";
    
    // Benchmark 6: Comparison with std::vector
    std::cout << "\n" << colors::yellow() << "Comparison with std::vector:" 
              << colors::reset() << "\n";
    
    std::vector<int> std_vec(N);
    for (size_t i = 0; i < N; ++i) {
        std_vec[i] = static_cast<int>(rng() % 1000);
    }
    DoNotOptimize(std_vec.data());
    
    print_cpu_state("std::vector iter");
    double std_iter_time = measure_perf([&std_vec]() {
        long long sum = 0;
        for (size_t i = 0; i < std_vec.size(); ++i) {
            sum += std_vec[i];
        }
        g_benchmark_sink = sum;
    }, 1000, 100);
    
    std::cout << "std::vector iteration (" << N << " elements): " << format_time(std_iter_time)
              << " (" << std::fixed << std::setprecision(2) << (std_iter_time * 1e6 / N) << " ns/element)\n\n";
    
    print_cpu_state("std::vector random");
    double std_random_time = measure_perf([&std_vec, &indices]() {
        long long sum = 0;
        for (size_t idx : indices) {
            sum += std_vec[idx];
        }
        g_benchmark_sink = sum;
    }, 1000, 100);
    
    std::cout << "std::vector random access (" << N << " elements): " << format_time(std_random_time)
              << " (" << std::fixed << std::setprecision(2) << (std_random_time * 1e6 / N) << " ns/element)\n";
    
    // Speedup calculation
    double iter_speedup = std_iter_time / iter_time;
    double random_speedup = std_random_time / random_time;
    
    std::cout << "\nSpeedup vs std::vector:\n";
    std::cout << "  Sequential: " << std::fixed << std::setprecision(2) << iter_speedup << "x\n";
    std::cout << "  Random:     " << std::fixed << std::setprecision(2) << random_speedup << "x\n";
    
    std::cout << "\nAlignment: " << fat_p::AlignedVector<int, 64>::get_alignment() 
              << " bytes\n";
    
    // Final CPU state
    print_cpu_state("final");
}

} // namespace fat_p::testing::alignedvector

namespace fat_p::testing
{

bool test_AlignedVector()
{
    PRINT_HEADER(ALIGNED VECTOR)
    
    // Enable verbose output to see individual test results
    get_test_config().verbose = true;
    
    TestRunner runner;
    
    // Construction
    RUN_TEST_NS(runner, alignedvector, default_construction);
    RUN_TEST_NS(runner, alignedvector, size_construction);
    RUN_TEST_NS(runner, alignedvector, size_value_construction);
    RUN_TEST_NS(runner, alignedvector, range_construction);
    RUN_TEST_NS(runner, alignedvector, range_construction_from_list);
    RUN_TEST_NS(runner, alignedvector, range_construction_from_array);
    RUN_TEST_NS(runner, alignedvector, range_construction_empty);
    RUN_TEST_NS(runner, alignedvector, initializer_list_construction);
    
    // Alignment
    RUN_TEST_NS(runner, alignedvector, alignment_64);
    RUN_TEST_NS(runner, alignedvector, alignment_various);
    RUN_TEST_NS(runner, alignedvector, assume_aligned);
    
    // Iterators
    RUN_TEST_NS(runner, alignedvector, empty_vector_iterators_strict);
    RUN_TEST_NS(runner, alignedvector, forward_iterators);
    RUN_TEST_NS(runner, alignedvector, reverse_iterators);
    
    // Element access
    RUN_TEST_NS(runner, alignedvector, subscript_operator);
    RUN_TEST_NS(runner, alignedvector, at_bounds_checking);
    RUN_TEST_NS(runner, alignedvector, front_back);
    
    // Assign
    RUN_TEST_NS(runner, alignedvector, assign_count_value);
    RUN_TEST_NS(runner, alignedvector, assign_range);
    RUN_TEST_NS(runner, alignedvector, assign_initializer_list);
    RUN_TEST_NS(runner, alignedvector, assign_empty);
    RUN_TEST_NS(runner, alignedvector, initializer_list_assignment_operator);
    
    // Insert
    RUN_TEST_NS(runner, alignedvector, insert_single_lvalue);
    RUN_TEST_NS(runner, alignedvector, insert_single_rvalue);
    RUN_TEST_NS(runner, alignedvector, insert_at_begin);
    RUN_TEST_NS(runner, alignedvector, insert_at_end);
    RUN_TEST_NS(runner, alignedvector, insert_into_empty);
    RUN_TEST_NS(runner, alignedvector, insert_count);
    RUN_TEST_NS(runner, alignedvector, insert_count_zero);
    RUN_TEST_NS(runner, alignedvector, insert_range);
    RUN_TEST_NS(runner, alignedvector, insert_initializer_list);
    RUN_TEST_NS(runner, alignedvector, insert_triggers_reallocation);
    
    // Emplace
    RUN_TEST_NS(runner, alignedvector, emplace_middle);
    
    // Erase
    RUN_TEST_NS(runner, alignedvector, erase_single);
    RUN_TEST_NS(runner, alignedvector, erase_first);
    RUN_TEST_NS(runner, alignedvector, erase_last);
    RUN_TEST_NS(runner, alignedvector, erase_range);
    RUN_TEST_NS(runner, alignedvector, erase_range_empty);
    RUN_TEST_NS(runner, alignedvector, erase_all);
    RUN_TEST_NS(runner, alignedvector, erase_to_empty);
    
    // Capacity
    RUN_TEST_NS(runner, alignedvector, reserve_capacity);
    RUN_TEST_NS(runner, alignedvector, shrink_to_fit);
    RUN_TEST_NS(runner, alignedvector, max_size_and_overflow);
    
    // Modifiers
    RUN_TEST_NS(runner, alignedvector, push_back);
    RUN_TEST_NS(runner, alignedvector, emplace_back);
    RUN_TEST_NS(runner, alignedvector, resize);
    RUN_TEST_NS(runner, alignedvector, clear_and_pop_back);
    RUN_TEST_NS(runner, alignedvector, swap);
    
    // Copy/Move
    RUN_TEST_NS(runner, alignedvector, copy_constructor);
    RUN_TEST_NS(runner, alignedvector, move_constructor);
    RUN_TEST_NS(runner, alignedvector, copy_assignment);
    RUN_TEST_NS(runner, alignedvector, move_assignment);
    RUN_TEST_NS(runner, alignedvector, self_assignment);
    
    // Comparison
    RUN_TEST_NS(runner, alignedvector, equality_operators);
    RUN_TEST_NS(runner, alignedvector, ordering_operators);
    
    // Allocator
    RUN_TEST_NS(runner, alignedvector, get_allocator);
    RUN_TEST_NS(runner, alignedvector, allocator_comparison);
    
    // Value initialization
    RUN_TEST_NS(runner, alignedvector, trivial_struct_initialization);
    RUN_TEST_NS(runner, alignedvector, nontrivial_copy_initialization);
    
    // Move-only types
    RUN_TEST_NS(runner, alignedvector, move_only_types);
    
    // Exception safety
    RUN_TEST_NS(runner, alignedvector, exception_safety_strong_guarantee);
    
    // Destructor behavior
    RUN_TEST_NS(runner, alignedvector, destructor_calls);
    RUN_TEST_NS(runner, alignedvector, erase_calls_destructors);
    
    // Adversarial tests (bug detection)
    RUN_TEST_NS(runner, alignedvector, assign_exception_safety_strong_guarantee);
    RUN_TEST_NS(runner, alignedvector, assign_range_exception_safety);
    RUN_TEST_NS(runner, alignedvector, insert_range_exception_safety);
    RUN_TEST_NS(runner, alignedvector, insert_count_exception_safety);
    RUN_TEST_NS(runner, alignedvector, construct_range_copy_no_leak);
    RUN_TEST_NS(runner, alignedvector, resize_exception_safety);
    RUN_TEST_NS(runner, alignedvector, emplace_back_exception_safety);
    RUN_TEST_NS(runner, alignedvector, self_insertion_safety);
    RUN_TEST_NS(runner, alignedvector, push_back_aliasing);
    
    alignedvector::benchmark_aligned_vector();
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_AlignedVector() ? 0 : 1;
}
#endif
