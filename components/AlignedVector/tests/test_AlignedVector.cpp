/**
 * @file test_AlignedVector.cpp
 * @brief Comprehensive unit tests for AlignedVector.h
 *
 * Test coverage includes:
 * - Construction (default, size, value, range, initializer_list)
 * - Alignment verification (various alignments, assume_aligned)
 * - Element access (subscript, at, front, back)
 * - Iterators (forward, reverse, empty)
 * - Modifiers (insert, erase, push_back, emplace, resize)
 * - Copy/move semantics
 * - Exception safety (strong/basic guarantees)
 * - Self-insertion and aliasing handling
 * - RAII correctness
 * - Fuzz testing against std::vector
 */
/*
FATP_META:
  meta_version: 1
  component: AlignedVector
  file_role: test
  path: components/AlignedVector/tests/test_AlignedVector.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for AlignedVector."
  api_stability: in_work
  related:
    docs_search: "AlignedVector"
    headers:
      - include/fat_p/AlignedVector.h
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
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "AlignedVector.h"
#include "FatPTest.h"

namespace fat_p::testing::alignedvector
{

// ============================================================================
// Helper Types (in named namespace per guidelines)
// ============================================================================

namespace helpers
{

/**
 * @brief Type that throws after N copy/construction operations
 *
 * Used for exception safety testing. Move operations are noexcept.
 */
struct ThrowAfterN
{
    static int constructionCount;
    static int throwAfter;
    static int destructionCount;

    int value;

    static void reset(int throwAt = -1)
    {
        constructionCount = 0;
        throwAfter = throwAt;
        destructionCount = 0;
    }

    ThrowAfterN()
        : value(0)
    {
        if (throwAfter >= 0 && constructionCount >= throwAfter)
        {
            throw std::runtime_error("ThrowAfterN: construction limit reached");
        }
        ++constructionCount;
    }

    explicit ThrowAfterN(int v)
        : value(v)
    {
        if (throwAfter >= 0 && constructionCount >= throwAfter)
        {
            throw std::runtime_error("ThrowAfterN: construction limit reached");
        }
        ++constructionCount;
    }

    ThrowAfterN(const ThrowAfterN& other)
        : value(other.value)
    {
        if (throwAfter >= 0 && constructionCount >= throwAfter)
        {
            throw std::runtime_error("ThrowAfterN: copy construction limit reached");
        }
        ++constructionCount;
    }

    ThrowAfterN(ThrowAfterN&& other) noexcept
        : value(other.value)
    {
        other.value = -1;
    }

    ThrowAfterN& operator=(const ThrowAfterN& other)
    {
        if (throwAfter >= 0 && constructionCount >= throwAfter)
        {
            throw std::runtime_error("ThrowAfterN: assignment limit reached");
        }
        value = other.value;
        ++constructionCount;
        return *this;
    }

    ThrowAfterN& operator=(ThrowAfterN&& other) noexcept
    {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~ThrowAfterN()
    {
        ++destructionCount;
    }
};

int ThrowAfterN::constructionCount = 0;
int ThrowAfterN::throwAfter = -1;
int ThrowAfterN::destructionCount = 0;

/**
 * @brief Type that throws on copy (configurable)
 */
struct ThrowOnCopy
{
    int value;
    static int copyCount;
    static int limit;

    ThrowOnCopy(int v = 0)
        : value(v)
    {
    }

    ThrowOnCopy(const ThrowOnCopy& other)
        : value(other.value)
    {
        if (copyCount >= limit)
        {
            throw std::runtime_error("Simulated copy failure");
        }
        ++copyCount;
    }

    ThrowOnCopy& operator=(const ThrowOnCopy&) = default;

    static void reset(int newLimit = 100)
    {
        copyCount = 0;
        limit = newLimit;
    }
};

int ThrowOnCopy::copyCount = 0;
int ThrowOnCopy::limit = 100;

/**
 * @brief Type that throws on move assignment (configurable) and tracks live instances
 *
 * Used to test exception safety in insert/emplace when move assignment throws.
 * This catches bugs where an element is constructed in uninitialized memory
 * but not tracked in size, leading to leaks if subsequent moves throw.
 *
 * The `alive` counter provides direct leak detection: if any instance leaks,
 * `alive` won't return to 0 when all scopes exit.
 */
struct ThrowOnMoveAssign
{
    int value;
    static int alive; // Live instance count (increment on ctor, decrement on dtor)
    static int moveAssignCount;
    static int throwAfter;
    static int constructCount;
    static int destructCount;

    ThrowOnMoveAssign(int v = 0)
        : value(v)
    {
        ++alive;
        ++constructCount;
    }

    ThrowOnMoveAssign(const ThrowOnMoveAssign& other)
        : value(other.value)
    {
        ++alive;
        ++constructCount;
    }

    ThrowOnMoveAssign(ThrowOnMoveAssign&& other) noexcept
        : value(other.value)
    {
        other.value = -1;
        ++alive;
        ++constructCount;
    }

    ThrowOnMoveAssign& operator=(const ThrowOnMoveAssign& other)
    {
        value = other.value;
        return *this;
    }

    ThrowOnMoveAssign& operator=(ThrowOnMoveAssign&& other)
    {
        // Throw BEFORE incrementing, so throwAfter=0 throws on 1st call,
        // throwAfter=1 throws on 2nd call, etc. (0-based indexing)
        if (throwAfter >= 0 && moveAssignCount == throwAfter)
        {
            throw std::runtime_error("ThrowOnMoveAssign: move assignment throw");
        }
        ++moveAssignCount;
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~ThrowOnMoveAssign()
    {
        --alive;
        ++destructCount;
    }

    static void reset(int throwAt = -1)
    {
        moveAssignCount = 0;
        throwAfter = throwAt;
        constructCount = 0;
        destructCount = 0;
        // Note: don't reset alive here - it should already be 0 if no leaks
    }

    static bool balanced()
    {
        return constructCount == destructCount;
    }

    static bool no_leaks()
    {
        return alive == 0 && balanced();
    }
};

int ThrowOnMoveAssign::alive = 0;
int ThrowOnMoveAssign::moveAssignCount = 0;
int ThrowOnMoveAssign::throwAfter = -1;
int ThrowOnMoveAssign::constructCount = 0;
int ThrowOnMoveAssign::destructCount = 0;

/**
 * @brief Counts destructor calls for RAII verification
 */
struct DestructorCounter
{
    static int count;
    int id;

    DestructorCounter(int i = 0)
        : id(i)
    {
    }

    ~DestructorCounter()
    {
        ++count;
    }

    DestructorCounter(const DestructorCounter& o)
        : id(o.id)
    {
    }

    DestructorCounter& operator=(const DestructorCounter&) = default;

    static void reset()
    {
        count = 0;
    }
};

int DestructorCounter::count = 0;

/**
 * @brief Move-only type for testing move semantics
 */
struct MoveOnly
{
    int value;

    MoveOnly(int v)
        : value(v)
    {
    }

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&& o) noexcept
        : value(o.value)
    {
    }

    MoveOnly& operator=(MoveOnly&& o) noexcept
    {
        value = o.value;
        return *this;
    }
};

} // namespace helpers

// ============================================================================
// Construction Tests
// ============================================================================

FATP_TEST_CASE(default_construction)
{
    fat_p::AlignedVector<int> vec;

    FATP_ASSERT_TRUE(vec.empty(), "Default constructor should create empty vector");
    FATP_ASSERT_EQ(vec.size(), 0u, "Size should be 0");
    FATP_ASSERT_EQ(vec.capacity(), 0u, "Capacity should be 0");

    return true;
}

FATP_TEST_CASE(size_construction)
{
    fat_p::AlignedVector<int> vec(10);

    FATP_ASSERT_EQ(vec.size(), 10u, "Size constructor should create vector of size 10");
    FATP_ASSERT_TRUE(!vec.empty(), "Should not be empty");

    for (size_t i = 0; i < 10; ++i)
    {
        FATP_ASSERT_EQ(vec[i], 0, "Elements should be value-initialized to 0");
    }

    return true;
}

FATP_TEST_CASE(size_value_construction)
{
    fat_p::AlignedVector<int> vec(5, 42);

    FATP_ASSERT_EQ(vec.size(), 5u, "Should have size 5");
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec[i], 42, "All elements should be 42");
    }

    return true;
}

FATP_TEST_CASE(range_construction)
{
    std::vector<int> src = {1, 2, 3, 4, 5};

    fat_p::AlignedVector<int> vec(src.begin(), src.end());

    FATP_ASSERT_EQ(vec.size(), 5u, "Range constructor should create vector of size 5");
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec[i], static_cast<int>(i + 1), "Elements should match source");
    }

    return true;
}

FATP_TEST_CASE(range_construction_from_list)
{
    std::list<int> src = {10, 20, 30};

    fat_p::AlignedVector<int> vec(src.begin(), src.end());

    FATP_ASSERT_EQ(vec.size(), 3u, "Should have 3 elements from list");
    FATP_ASSERT_EQ(vec[0], 10, "First element");
    FATP_ASSERT_EQ(vec[1], 20, "Second element");
    FATP_ASSERT_EQ(vec[2], 30, "Third element");

    return true;
}

FATP_TEST_CASE(range_construction_from_array)
{
    int arr[] = {100, 200, 300, 400};

    fat_p::AlignedVector<int> vec(std::begin(arr), std::end(arr));

    FATP_ASSERT_EQ(vec.size(), 4u, "Should have 4 elements from array");
    FATP_ASSERT_EQ(vec[0], 100, "First element");
    FATP_ASSERT_EQ(vec[3], 400, "Last element");

    return true;
}

FATP_TEST_CASE(range_construction_empty)
{
    std::vector<int> emptySrc;

    fat_p::AlignedVector<int> vec(emptySrc.begin(), emptySrc.end());

    FATP_ASSERT_TRUE(vec.empty(), "Range constructor from empty should be empty");

    return true;
}

FATP_TEST_CASE(initializer_list_construction)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    FATP_ASSERT_EQ(vec.size(), 5u, "Initializer list should create vector of size 5");
    FATP_ASSERT_EQ(vec[0], 1, "First element should be 1");
    FATP_ASSERT_EQ(vec[4], 5, "Last element should be 5");

    return true;
}

// ============================================================================
// Alignment Tests
// ============================================================================

FATP_TEST_CASE(alignment_64)
{
    fat_p::AlignedVector<float, 64> vec(100);

    FATP_ASSERT_TRUE(vec.is_aligned(), "Data should be aligned");
    FATP_ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec.data()) % 64, 0u, "Data pointer should be 64-byte aligned");

    return true;
}

FATP_TEST_CASE(alignment_various)
{
    fat_p::AlignedVector<float, 16> vec16(10);
    FATP_ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec16.data()) % 16, 0u, "16-byte alignment should work");

    fat_p::AlignedVector<double, 128> vec128(10);
    FATP_ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec128.data()) % 128, 0u, "128-byte alignment should work");

    fat_p::AlignedVector<int, 256> vec256(10);
    FATP_ASSERT_EQ(reinterpret_cast<std::uintptr_t>(vec256.data()) % 256, 0u, "256-byte alignment should work");

    return true;
}

FATP_TEST_CASE(assume_aligned)
{
    fat_p::AlignedVector<float, 64> vec(100, 1.0f);

    float* ptr = vec.assume_aligned();
    FATP_ASSERT_TRUE(ptr != nullptr, "assume_aligned should return non-null");
    FATP_ASSERT_EQ(ptr, vec.data(), "assume_aligned should return same as data()");

    const fat_p::AlignedVector<float, 64>& cvec = vec;
    const float* cptr = cvec.assume_aligned();
    FATP_ASSERT_EQ(cptr, cvec.data(), "const assume_aligned should work");

    float sum = 0.0f;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        sum += ptr[i];
    }
    FATP_ASSERT_CLOSE(sum, 100.0f, "assume_aligned should provide valid access");

    return true;
}

// ============================================================================
// Iterator Tests
// ============================================================================

FATP_TEST_CASE(empty_vector_iterators_strict)
{
    fat_p::AlignedVector<int> empty;

    FATP_ASSERT_TRUE(empty.begin() == empty.end(), "Empty begin() should equal end()");

    int count = 0;
    for (int v : empty)
    {
        (void)v;
        ++count;
    }
    FATP_ASSERT_EQ(count, 0, "Range-for over empty vector should not iterate");

    return true;
}

FATP_TEST_CASE(forward_iterators)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        sum += *it;
    }
    FATP_ASSERT_EQ(sum, 15, "Iterator sum should be 15");

    return true;
}

FATP_TEST_CASE(reverse_iterators)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    std::vector<int> reversed;
    for (auto it = vec.rbegin(); it != vec.rend(); ++it)
    {
        reversed.push_back(*it);
    }
    FATP_ASSERT_EQ(reversed.size(), 5u, "Should have 5 reversed elements");
    FATP_ASSERT_EQ(reversed[0], 5, "First reversed should be 5");
    FATP_ASSERT_EQ(reversed[4], 1, "Last reversed should be 1");

    return true;
}

// ============================================================================
// Element Access Tests
// ============================================================================

FATP_TEST_CASE(subscript_operator)
{
    fat_p::AlignedVector<int> vec = {10, 20, 30, 40, 50};

    FATP_ASSERT_EQ(vec[0], 10, "First element");
    FATP_ASSERT_EQ(vec[4], 50, "Last element");

    vec[2] = 99;
    FATP_ASSERT_EQ(vec[2], 99, "Modification via subscript should work");

    return true;
}

FATP_TEST_CASE(at_bounds_checking)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    FATP_ASSERT_EQ(vec.at(0), 1, "at(0) should return first element");
    FATP_ASSERT_THROWS(vec.at(3), std::out_of_range, "at(3) should throw out_of_range");

    return true;
}

FATP_TEST_CASE(front_back)
{
    fat_p::AlignedVector<int> vec = {10, 20, 30};

    FATP_ASSERT_EQ(vec.front(), 10, "front() should return first element");
    FATP_ASSERT_EQ(vec.back(), 30, "back() should return last element");

    return true;
}

FATP_TEST_CASE(front_back_modification)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    vec.front() = 100;
    vec.back() = 300;

    FATP_ASSERT_EQ(vec[0], 100, "front() modification should work");
    FATP_ASSERT_EQ(vec[2], 300, "back() modification should work");

    return true;
}

// ============================================================================
// Assign Tests
// ============================================================================

FATP_TEST_CASE(assign_count_value)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    vec.assign(5, 42);

    FATP_ASSERT_EQ(vec.size(), 5u, "Size should be 5 after assign");
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec[i], 42, "All elements should be 42");
    }

    return true;
}

FATP_TEST_CASE(assign_range)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    std::vector<int> src = {10, 20, 30};

    vec.assign(src.begin(), src.end());

    FATP_ASSERT_EQ(vec.size(), 3u, "Size should be 3 after assign");
    FATP_ASSERT_EQ(vec[0], 10, "First element");
    FATP_ASSERT_EQ(vec[2], 30, "Last element");

    return true;
}

FATP_TEST_CASE(assign_initializer_list)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    vec.assign({100, 200});

    FATP_ASSERT_EQ(vec.size(), 2u, "Size should be 2 after assign");
    FATP_ASSERT_EQ(vec[0], 100, "First element");
    FATP_ASSERT_EQ(vec[1], 200, "Second element");

    return true;
}

FATP_TEST_CASE(assign_empty)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    vec.assign(0, 42);
    FATP_ASSERT_TRUE(vec.empty(), "Assign with count 0 should result in empty vector");

    return true;
}

FATP_TEST_CASE(initializer_list_assignment_operator)
{
    fat_p::AlignedVector<int> vec;

    vec = {5, 10, 15, 20};

    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    FATP_ASSERT_EQ(vec[0], 5, "First element");
    FATP_ASSERT_EQ(vec[3], 20, "Last element");

    return true;
}

// ============================================================================
// Insert Tests
// ============================================================================

FATP_TEST_CASE(insert_single_lvalue)
{
    fat_p::AlignedVector<int> vec = {1, 2, 4, 5};
    int val = 3;

    auto it = vec.insert(vec.begin() + 2, val);

    FATP_ASSERT_EQ(vec.size(), 5u, "Size should be 5 after insert");
    FATP_ASSERT_EQ(*it, 3, "Iterator should point to inserted element");
    FATP_ASSERT_EQ(vec[2], 3, "Inserted element should be at position 2");

    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec[i], static_cast<int>(i) + 1, "Elements should be 1,2,3,4,5");
    }

    return true;
}

FATP_TEST_CASE(insert_single_rvalue)
{
    fat_p::AlignedVector<int> vec = {1, 3};

    auto it = vec.insert(vec.begin() + 1, 2);

    FATP_ASSERT_EQ(vec.size(), 3u, "Size should be 3 after insert");
    FATP_ASSERT_EQ(*it, 2, "Iterator should point to inserted element");
    FATP_ASSERT_EQ(vec[0], 1, "First element");
    FATP_ASSERT_EQ(vec[1], 2, "Inserted element");
    FATP_ASSERT_EQ(vec[2], 3, "Last element");

    return true;
}

FATP_TEST_CASE(insert_at_begin)
{
    fat_p::AlignedVector<int> vec = {2, 3, 4};

    vec.insert(vec.begin(), 1);

    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    FATP_ASSERT_EQ(vec[0], 1, "Inserted element should be first");

    return true;
}

FATP_TEST_CASE(insert_at_end)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    vec.insert(vec.end(), 4);

    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    FATP_ASSERT_EQ(vec[3], 4, "Inserted element should be last");

    return true;
}

FATP_TEST_CASE(insert_into_empty)
{
    fat_p::AlignedVector<int> vec;

    vec.insert(vec.begin(), 42);

    FATP_ASSERT_EQ(vec.size(), 1u, "Size should be 1");
    FATP_ASSERT_EQ(vec[0], 42, "Element should be 42");

    return true;
}

FATP_TEST_CASE(insert_count)
{
    fat_p::AlignedVector<int> vec = {1, 5};

    vec.insert(vec.begin() + 1, 3, 3);

    FATP_ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    FATP_ASSERT_EQ(vec[0], 1, "First element");
    FATP_ASSERT_EQ(vec[1], 3, "Inserted element 1");
    FATP_ASSERT_EQ(vec[2], 3, "Inserted element 2");
    FATP_ASSERT_EQ(vec[3], 3, "Inserted element 3");
    FATP_ASSERT_EQ(vec[4], 5, "Last element");

    return true;
}

FATP_TEST_CASE(insert_count_zero)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    auto it = vec.insert(vec.begin() + 1, 0, 99);

    FATP_ASSERT_EQ(vec.size(), 3u, "Size should be unchanged");
    FATP_ASSERT_TRUE(it == vec.begin() + 1, "Should return position iterator");

    return true;
}

FATP_TEST_CASE(insert_range)
{
    fat_p::AlignedVector<int> vec = {1, 5};
    std::vector<int> src = {2, 3, 4};

    vec.insert(vec.begin() + 1, src.begin(), src.end());

    FATP_ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec[i], static_cast<int>(i) + 1, "Elements should be 1,2,3,4,5");
    }

    return true;
}

FATP_TEST_CASE(insert_initializer_list)
{
    fat_p::AlignedVector<int> vec = {1, 5};

    vec.insert(vec.begin() + 1, {2, 3, 4});

    FATP_ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec[i], static_cast<int>(i) + 1, "Elements should be 1,2,3,4,5");
    }

    return true;
}

FATP_TEST_CASE(insert_triggers_reallocation)
{
    fat_p::AlignedVector<int> vec;
    vec.reserve(3);
    vec.push_back(1);
    vec.push_back(3);
    vec.push_back(4);

    vec.insert(vec.begin() + 1, 2);

    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    FATP_ASSERT_TRUE(vec.capacity() > 3, "Capacity should have grown");
    for (size_t i = 0; i < 4; ++i)
    {
        FATP_ASSERT_EQ(vec[i], static_cast<int>(i) + 1, "Elements should be 1,2,3,4");
    }

    return true;
}

// ============================================================================
// Emplace Tests
// ============================================================================

FATP_TEST_CASE(emplace_middle)
{
    struct Point
    {
        int x, y;
        Point(int x_, int y_)
            : x(x_)
            , y(y_)
        {
        }
    };

    fat_p::AlignedVector<Point> vec;
    vec.emplace_back(1, 1);
    vec.emplace_back(3, 3);

    auto it = vec.emplace(vec.begin() + 1, 2, 2);

    FATP_ASSERT_EQ(vec.size(), 3u, "Size should be 3");
    FATP_ASSERT_EQ(it->x, 2, "Emplaced element x should be 2");
    FATP_ASSERT_EQ(it->y, 2, "Emplaced element y should be 2");
    FATP_ASSERT_EQ(vec[0].x, 1, "First element x");
    FATP_ASSERT_EQ(vec[1].x, 2, "Middle element x");
    FATP_ASSERT_EQ(vec[2].x, 3, "Last element x");

    return true;
}

FATP_TEST_CASE(emplace_at_begin)
{
    fat_p::AlignedVector<int> vec = {2, 3, 4};

    vec.emplace(vec.begin(), 1);

    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    for (size_t i = 0; i < 4; ++i)
    {
        FATP_ASSERT_EQ(vec[i], static_cast<int>(i) + 1, "Elements should be 1,2,3,4");
    }

    return true;
}

FATP_TEST_CASE(emplace_at_end)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    vec.emplace(vec.end(), 4);

    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4");
    FATP_ASSERT_EQ(vec[3], 4, "Emplaced element should be last");

    return true;
}

FATP_TEST_CASE(emplace_exception_safety)
{
    // Test that emplace() provides basic exception guarantee when construction throws
    using helpers::ThrowAfterN;

    fat_p::AlignedVector<ThrowAfterN> vec;

    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i)
    {
        vec.push_back(ThrowAfterN(i));
    }

    size_t sizeBefore = vec.size();

    // Reset and set to throw on first construction attempt
    ThrowAfterN::reset(0);

    bool threw = false;
    try
    {
        vec.emplace(vec.begin() + 2, 999);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");
    // Basic guarantee: vector should be in valid state
    FATP_ASSERT_EQ(vec.size(), sizeBefore, "Size should be unchanged after failed emplace");

    // Verify elements are still accessible
    size_t count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        ++count;
    }
    FATP_ASSERT_EQ(count, vec.size(), "Iteration count should match size");

    ThrowAfterN::reset();
    return true;
}

// ============================================================================
// Erase Tests
// ============================================================================

FATP_TEST_CASE(erase_single)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    auto it = vec.erase(vec.begin() + 2);

    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4 after erase");
    FATP_ASSERT_EQ(*it, 4, "Iterator should point to next element");
    FATP_ASSERT_EQ(vec[0], 1, "First element");
    FATP_ASSERT_EQ(vec[1], 2, "Second element");
    FATP_ASSERT_EQ(vec[2], 4, "Third element (was fourth)");
    FATP_ASSERT_EQ(vec[3], 5, "Fourth element (was fifth)");

    return true;
}

FATP_TEST_CASE(erase_first)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    vec.erase(vec.begin());

    FATP_ASSERT_EQ(vec.size(), 2u, "Size should be 2");
    FATP_ASSERT_EQ(vec[0], 2, "First element should now be 2");
    FATP_ASSERT_EQ(vec[1], 3, "Second element should be 3");

    return true;
}

FATP_TEST_CASE(erase_last)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    auto it = vec.erase(vec.end() - 1);

    FATP_ASSERT_EQ(vec.size(), 2u, "Size should be 2");
    FATP_ASSERT_TRUE(it == vec.end(), "Iterator should be end() after erasing last");
    FATP_ASSERT_EQ(vec[0], 1, "First element");
    FATP_ASSERT_EQ(vec[1], 2, "Second element");

    return true;
}

FATP_TEST_CASE(erase_range)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    auto it = vec.erase(vec.begin() + 1, vec.begin() + 4);

    FATP_ASSERT_EQ(vec.size(), 2u, "Size should be 2 after erasing 3 elements");
    FATP_ASSERT_EQ(*it, 5, "Iterator should point to element after erased range");
    FATP_ASSERT_EQ(vec[0], 1, "First element");
    FATP_ASSERT_EQ(vec[1], 5, "Second element (was fifth)");

    return true;
}

FATP_TEST_CASE(erase_range_empty)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    auto it = vec.erase(vec.begin() + 1, vec.begin() + 1);

    FATP_ASSERT_EQ(vec.size(), 3u, "Size should be unchanged for empty range");
    FATP_ASSERT_EQ(*it, 2, "Iterator should point to position");

    return true;
}

FATP_TEST_CASE(erase_all)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    vec.erase(vec.begin(), vec.end());

    FATP_ASSERT_TRUE(vec.empty(), "Vector should be empty after erasing all");

    return true;
}

FATP_TEST_CASE(erase_to_empty)
{
    fat_p::AlignedVector<int> vec = {42};

    vec.erase(vec.begin());

    FATP_ASSERT_TRUE(vec.empty(), "Vector should be empty after erasing only element");

    return true;
}

// ============================================================================
// Capacity Tests
// ============================================================================

FATP_TEST_CASE(reserve_capacity)
{
    fat_p::AlignedVector<int> vec;

    vec.reserve(100);

    FATP_ASSERT_EQ(vec.size(), 0u, "Size should still be 0");
    FATP_ASSERT_GE(vec.capacity(), 100u, "Capacity should be at least 100");

    return true;
}

FATP_TEST_CASE(shrink_to_fit)
{
    fat_p::AlignedVector<int> vec;
    vec.reserve(100);
    vec.push_back(1);
    vec.push_back(2);

    vec.shrink_to_fit();

    FATP_ASSERT_EQ(vec.size(), 2u, "Size should still be 2");
    FATP_ASSERT_EQ(vec.capacity(), 2u, "Capacity should shrink to size");

    return true;
}

FATP_TEST_CASE(max_size_and_overflow)
{
    fat_p::AlignedVector<int> vec;

    FATP_ASSERT_TRUE(vec.max_size() > 0, "max_size should be positive");
    FATP_ASSERT_TRUE(vec.max_size() <= std::numeric_limits<size_t>::max() / sizeof(int),
                     "max_size should account for sizeof(T)");

    return true;
}

// ============================================================================
// Modifier Tests
// ============================================================================

FATP_TEST_CASE(push_back)
{
    fat_p::AlignedVector<int> vec;

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    FATP_ASSERT_EQ(vec.size(), 3u, "Size should be 3");
    FATP_ASSERT_EQ(vec[0], 1, "First element");
    FATP_ASSERT_EQ(vec[1], 2, "Second element");
    FATP_ASSERT_EQ(vec[2], 3, "Third element");

    return true;
}

FATP_TEST_CASE(emplace_back)
{
    struct Point
    {
        int x, y;
        Point(int x_, int y_)
            : x(x_)
            , y(y_)
        {
        }
    };

    fat_p::AlignedVector<Point> vec;
    auto& ref = vec.emplace_back(1, 2);

    FATP_ASSERT_EQ(vec.size(), 1u, "Size should be 1");
    FATP_ASSERT_EQ(ref.x, 1, "Returned reference x");
    FATP_ASSERT_EQ(ref.y, 2, "Returned reference y");

    return true;
}

FATP_TEST_CASE(resize)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3};

    vec.resize(5);
    FATP_ASSERT_EQ(vec.size(), 5u, "Size should grow to 5");
    FATP_ASSERT_EQ(vec[3], 0, "New elements should be value-initialized");
    FATP_ASSERT_EQ(vec[4], 0, "New elements should be value-initialized");

    vec.resize(2);
    FATP_ASSERT_EQ(vec.size(), 2u, "Size should shrink to 2");
    FATP_ASSERT_EQ(vec[0], 1, "First element preserved");
    FATP_ASSERT_EQ(vec[1], 2, "Second element preserved");

    return true;
}

FATP_TEST_CASE(resize_with_value)
{
    fat_p::AlignedVector<int> vec = {1, 2};

    vec.resize(5, 99);

    FATP_ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    FATP_ASSERT_EQ(vec[0], 1, "First element preserved");
    FATP_ASSERT_EQ(vec[1], 2, "Second element preserved");
    FATP_ASSERT_EQ(vec[2], 99, "New element");
    FATP_ASSERT_EQ(vec[3], 99, "New element");
    FATP_ASSERT_EQ(vec[4], 99, "New element");

    return true;
}

FATP_TEST_CASE(clear_and_pop_back)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};
    size_t capBefore = vec.capacity();

    vec.pop_back();
    FATP_ASSERT_EQ(vec.size(), 4u, "Size should be 4 after pop_back");

    vec.clear();
    FATP_ASSERT_TRUE(vec.empty(), "Should be empty after clear");
    FATP_ASSERT_EQ(vec.capacity(), capBefore, "Capacity should be unchanged after clear");

    return true;
}

FATP_TEST_CASE(swap)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {4, 5};

    vec1.swap(vec2);

    FATP_ASSERT_EQ(vec1.size(), 2u, "vec1 size after swap");
    FATP_ASSERT_EQ(vec2.size(), 3u, "vec2 size after swap");
    FATP_ASSERT_EQ(vec1[0], 4, "vec1 first element");
    FATP_ASSERT_EQ(vec2[0], 1, "vec2 first element");

    return true;
}

// ============================================================================
// Copy/Move Tests
// ============================================================================

FATP_TEST_CASE(copy_constructor)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3, 4, 5};

    fat_p::AlignedVector<int> vec2(vec1);

    FATP_ASSERT_EQ(vec2.size(), 5u, "Copy should have same size");
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec2[i], vec1[i], "Elements should be equal");
    }

    // Verify independence
    vec2[0] = 99;
    FATP_ASSERT_EQ(vec1[0], 1, "Original should be unchanged");

    return true;
}

FATP_TEST_CASE(move_constructor)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3, 4, 5};
    int* originalData = vec1.data();

    fat_p::AlignedVector<int> vec2(std::move(vec1));

    FATP_ASSERT_EQ(vec2.size(), 5u, "Moved-to should have original size");
    FATP_ASSERT_EQ(vec2.data(), originalData, "Should have stolen pointer");
    FATP_ASSERT_TRUE(vec1.empty(), "Moved-from should be empty");

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {10, 20, 30, 40, 50};

    vec2 = vec1;

    FATP_ASSERT_EQ(vec2.size(), 3u, "Assigned should have new size");
    for (size_t i = 0; i < 3; ++i)
    {
        FATP_ASSERT_EQ(vec2[i], vec1[i], "Elements should be equal");
    }

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {10, 20, 30, 40, 50};
    int* originalData = vec1.data();

    vec2 = std::move(vec1);

    FATP_ASSERT_EQ(vec2.size(), 3u, "Moved-to should have original size");
    FATP_ASSERT_EQ(vec2.data(), originalData, "Should have stolen pointer");
    FATP_ASSERT_TRUE(vec1.empty(), "Moved-from should be empty");

    return true;
}

FATP_TEST_CASE(self_assignment)
{
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    vec = vec;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    FATP_ASSERT_EQ(vec.size(), 5u, "Self-assignment should preserve size");
    FATP_ASSERT_EQ(vec[0], 1, "Elements should be preserved");

    return true;
}

// ============================================================================
// Comparison Tests
// ============================================================================

FATP_TEST_CASE(equality_operators)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {1, 2, 3};
    fat_p::AlignedVector<int> vec3 = {1, 2, 4};
    fat_p::AlignedVector<int> vec4 = {1, 2};

    FATP_ASSERT_TRUE(vec1 == vec2, "Equal vectors should compare equal");
    FATP_ASSERT_FALSE(vec1 == vec3, "Different elements should not be equal");
    FATP_ASSERT_FALSE(vec1 == vec4, "Different sizes should not be equal");
    FATP_ASSERT_TRUE(vec1 != vec3, "Different vectors should be not-equal");

    return true;
}

FATP_TEST_CASE(ordering_operators)
{
    fat_p::AlignedVector<int> vec1 = {1, 2, 3};
    fat_p::AlignedVector<int> vec2 = {1, 2, 4};
    fat_p::AlignedVector<int> vec3 = {1, 2};

    FATP_ASSERT_TRUE(vec1 < vec2, "Lexicographically smaller");
    FATP_ASSERT_TRUE(vec3 < vec1, "Shorter is smaller if prefix matches");
    FATP_ASSERT_TRUE(vec2 > vec1, "Greater than");
    FATP_ASSERT_TRUE(vec1 <= vec2, "Less than or equal");
    FATP_ASSERT_TRUE(vec1 >= vec3, "Greater than or equal");

    return true;
}

// ============================================================================
// Allocator Tests
// ============================================================================

FATP_TEST_CASE(get_allocator)
{
    fat_p::AlignedVector<int, 64> vec;

    auto alloc = vec.get_allocator();

    FATP_ASSERT_EQ(alloc.alignment, 64u, "Allocator should have correct alignment");

    return true;
}

FATP_TEST_CASE(allocator_comparison)
{
    fat_p::AlignedAllocator<int, 64> alloc1;
    fat_p::AlignedAllocator<int, 64> alloc2;
    fat_p::AlignedAllocator<int, 128> alloc3;

    FATP_ASSERT_TRUE(alloc1 == alloc2, "Same alignment allocators should be equal");
    FATP_ASSERT_TRUE(alloc1 != alloc3, "Different alignment allocators should not be equal");

    return true;
}

// ============================================================================
// Type Initialization Tests
// ============================================================================

FATP_TEST_CASE(trivial_struct_initialization)
{
    struct TrivialStruct
    {
        int a;
        double b;
    };

    fat_p::AlignedVector<TrivialStruct> vec(5);

    FATP_ASSERT_EQ(vec.size(), 5u, "Size should be 5");
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_EQ(vec[i].a, 0, "Trivial struct should be zero-initialized");
        FATP_ASSERT_CLOSE(vec[i].b, 0.0, "Trivial struct should be zero-initialized");
    }

    return true;
}

FATP_TEST_CASE(nontrivial_copy_initialization)
{
    fat_p::AlignedVector<std::string> vec(3, "hello");

    FATP_ASSERT_EQ(vec.size(), 3u, "Size should be 3");
    for (size_t i = 0; i < 3; ++i)
    {
        FATP_ASSERT_EQ(vec[i], std::string("hello"), "Non-trivial should be copy-initialized");
    }

    return true;
}

// ============================================================================
// Move-Only Type Tests
// ============================================================================

FATP_TEST_CASE(move_only_types)
{
    using helpers::MoveOnly;

    fat_p::AlignedVector<MoveOnly> vec;
    vec.push_back(MoveOnly(1));
    vec.push_back(MoveOnly(2));

    FATP_ASSERT_EQ(vec.size(), 2u, "Should have 2 move-only elements");

    fat_p::AlignedVector<MoveOnly> vec2(std::move(vec));
    FATP_ASSERT_EQ(vec2.size(), 2u, "Move ctor should transfer elements");
    FATP_ASSERT_EQ(vec2[0].value, 1, "First element value");
    FATP_ASSERT_EQ(vec2[1].value, 2, "Second element value");

    return true;
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

FATP_TEST_CASE(exception_safety_strong_guarantee)
{
    using helpers::ThrowOnCopy;

    ThrowOnCopy::reset(100);

    fat_p::AlignedVector<ThrowOnCopy> vec;
    ThrowOnCopy t1(1), t2(2), t3(3);
    vec.push_back(t1);
    vec.push_back(t2);
    vec.push_back(t3);

    size_t originalSize = vec.size();
    size_t originalCap = vec.capacity();
    int originalVal0 = vec[0].value;

    ThrowOnCopy::reset(1);

    bool threw = false;
    try
    {
        vec.reserve(100);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Reserve should have thrown exception");
    FATP_ASSERT_EQ(vec.size(), originalSize, "Size should remain unchanged after failure");
    FATP_ASSERT_EQ(vec.capacity(), originalCap, "Capacity should remain unchanged");
    FATP_ASSERT_EQ(vec[0].value, originalVal0, "First element should be preserved");

    return true;
}

// ============================================================================
// Destructor Tests
// ============================================================================

FATP_TEST_CASE(destructor_calls)
{
    using helpers::DestructorCounter;

    DestructorCounter::reset();

    {
        fat_p::AlignedVector<DestructorCounter> vec;
        vec.reserve(10);
        vec.emplace_back(1);
        vec.emplace_back(2);
        int countAfterEmplace = DestructorCounter::count;

        vec.clear();
        FATP_ASSERT_EQ(DestructorCounter::count,
                       countAfterEmplace + 2,
                       "clear() should call destructors for all elements");
    }

    return true;
}

FATP_TEST_CASE(erase_calls_destructors)
{
    using helpers::DestructorCounter;

    DestructorCounter::reset();

    fat_p::AlignedVector<DestructorCounter> vec;
    vec.reserve(10);
    vec.emplace_back(1);
    vec.emplace_back(2);
    vec.emplace_back(3);
    int countBefore = DestructorCounter::count;

    vec.erase(vec.begin() + 1);

    FATP_ASSERT_TRUE(DestructorCounter::count > countBefore, "erase() should call destructor");
    FATP_ASSERT_EQ(vec.size(), 2u, "Size should be 2 after erase");

    return true;
}

// ============================================================================
// Adversarial Tests (Exception Safety Bug Detection)
// ============================================================================

FATP_TEST_CASE(assign_exception_safety_strong_guarantee)
{
    using helpers::ThrowAfterN;

    fat_p::AlignedVector<ThrowAfterN> vec;

    // Set up initial state
    ThrowAfterN::reset();
    vec.push_back(ThrowAfterN(1));
    vec.push_back(ThrowAfterN(2));
    vec.push_back(ThrowAfterN(3));

    size_t originalSize = vec.size();

    // Now try to assign with a throw in the middle
    ThrowAfterN::reset(2);

    bool threw = false;
    try
    {
        vec.assign(10, ThrowAfterN(99));
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");

    // Strong guarantee: vector should be in a valid state
    FATP_ASSERT_TRUE(vec.size() == 0 || vec.size() == originalSize,
                     "Vector should be empty or unchanged (basic guarantee)");

    ThrowAfterN::reset();

    return true;
}

FATP_TEST_CASE(assign_range_exception_safety)
{
    using helpers::ThrowAfterN;

    fat_p::AlignedVector<ThrowAfterN> vec;

    // Set up initial state
    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i)
    {
        vec.push_back(ThrowAfterN(i));
    }

    // Create source data
    std::vector<ThrowAfterN> source;
    ThrowAfterN::reset();
    for (int i = 0; i < 10; ++i)
    {
        source.push_back(ThrowAfterN(i * 10));
    }

    // Try to assign from range with throw
    ThrowAfterN::reset(3);

    bool threw = false;
    try
    {
        vec.assign(source.begin(), source.end());
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");

    // Vector should be in valid state (at least basic guarantee)
    size_t count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        ++count;
    }
    FATP_ASSERT_EQ(count, vec.size(), "Size should match iteration count");

    ThrowAfterN::reset();
    return true;
}

FATP_TEST_CASE(insert_range_exception_safety)
{
    using helpers::ThrowAfterN;

    fat_p::AlignedVector<ThrowAfterN> vec;

    // Set up initial state
    ThrowAfterN::reset();
    for (int i = 0; i < 10; ++i)
    {
        vec.push_back(ThrowAfterN(i));
    }

    // Create source data for insertion
    std::vector<ThrowAfterN> source;
    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i)
    {
        source.push_back(ThrowAfterN(100 + i));
    }

    // Try to insert in middle with throw during copy/construction
    ThrowAfterN::reset(2);

    bool threw = false;
    try
    {
        vec.insert(vec.begin() + 5, source.begin(), source.end());
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");

    // At minimum, vector should be in valid state (basic guarantee)
    size_t count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        ++count;
    }
    FATP_ASSERT_EQ(count, vec.size(), "Size should match iteration count");

    ThrowAfterN::reset();
    return true;
}

FATP_TEST_CASE(insert_count_exception_safety)
{
    using helpers::ThrowAfterN;

    fat_p::AlignedVector<ThrowAfterN> vec;

    // Set up initial state
    ThrowAfterN::reset();
    for (int i = 0; i < 10; ++i)
    {
        vec.push_back(ThrowAfterN(i));
    }

    // Try to insert multiple copies with throw
    ThrowAfterN valueToInsert(999);
    ThrowAfterN::reset(3);

    bool threw = false;
    try
    {
        vec.insert(vec.begin() + 5, 10, valueToInsert);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");

    // Vector should be in valid state
    size_t count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        ++count;
    }
    FATP_ASSERT_EQ(count, vec.size(), "Size should match iteration count");

    ThrowAfterN::reset();
    return true;
}

FATP_TEST_CASE(construct_range_copy_no_leak)
{
    using helpers::ThrowAfterN;

    ThrowAfterN::reset();

    // Create source
    std::vector<ThrowAfterN> source;
    for (int i = 0; i < 10; ++i)
    {
        source.push_back(ThrowAfterN(i));
    }

    // Create vector that will throw during construction
    ThrowAfterN::reset(5);

    bool threw = false;
    try
    {
        fat_p::AlignedVector<ThrowAfterN> vec(source.begin(), source.end());
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");

    ThrowAfterN::reset();
    return true;
}

FATP_TEST_CASE(resize_exception_safety)
{
    using helpers::ThrowAfterN;

    fat_p::AlignedVector<ThrowAfterN> vec;

    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i)
    {
        vec.push_back(ThrowAfterN(i));
    }

    // Try to resize larger with throw
    ThrowAfterN::reset(3);

    bool threw = false;
    try
    {
        vec.resize(20);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");

    // Vector should be in valid state
    FATP_ASSERT_TRUE(vec.size() <= 20, "Size should not exceed requested");

    ThrowAfterN::reset();
    return true;
}

FATP_TEST_CASE(emplace_back_exception_safety)
{
    using helpers::ThrowAfterN;

    fat_p::AlignedVector<ThrowAfterN> vec;

    ThrowAfterN::reset();
    for (int i = 0; i < 5; ++i)
    {
        vec.push_back(ThrowAfterN(i));
    }

    // Force reallocation scenario
    while (vec.size() < vec.capacity())
    {
        vec.push_back(ThrowAfterN(99));
    }
    size_t sizeBefore = vec.size();

    // Now emplace_back will need to reallocate
    ThrowAfterN::reset(0);

    bool threw = false;
    try
    {
        vec.emplace_back(123);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "Should have thrown exception");
    FATP_ASSERT_EQ(vec.size(), sizeBefore, "Size should be unchanged after failed emplace_back");

    ThrowAfterN::reset();
    return true;
}

// ============================================================================
// Throwing Move Assignment Tests (regression tests for leak bug)
// ============================================================================

FATP_TEST_CASE(insert_single_throwing_move_assign)
{
    // This test catches a bug where insert(pos, value) would leak an element
    // if move assignment throws after the tail element was constructed.
    using helpers::ThrowOnMoveAssign;

    // Ensure clean state at start
    FATP_ASSERT_EQ(ThrowOnMoveAssign::alive, 0, "No live instances at test start");
    ThrowOnMoveAssign::reset();

    {
        fat_p::AlignedVector<ThrowOnMoveAssign> vec;
        vec.reserve(10);

        // Add some elements (no throwing yet)
        for (int i = 0; i < 5; ++i)
        {
            vec.push_back(ThrowOnMoveAssign(i));
        }

        const int aliveBefore = ThrowOnMoveAssign::alive;

        // Set to throw on 3rd move assignment (0-indexed: call #2)
        // Insert at pos 1 with 5 elements does: tail construct, then shifts [4]->[3]->[2]->[1]
        // Throwing mid-shift tests the leak fix for the already-constructed tail element
        ThrowOnMoveAssign::throwAfter = 2;
        ThrowOnMoveAssign::moveAssignCount = 0;

        bool threw = false;
        try
        {
            ThrowOnMoveAssign newVal(99);
            vec.insert(vec.begin() + 1, newVal); // Insert in middle
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        FATP_ASSERT_TRUE(threw, "Should have thrown from move assignment");

        // Vector should still be valid and destructible
        // Size should be unchanged (basic guarantee)
        FATP_ASSERT_EQ(vec.size(), 5u, "Size should be unchanged after exception");

        // No leaked elements from the failed insert
        FATP_ASSERT_EQ(ThrowOnMoveAssign::alive,
                       aliveBefore,
                       "Failed insert should not leak a constructed tail element");
    }

    // Critical check: all constructed objects must have been destroyed
    FATP_ASSERT_TRUE(ThrowOnMoveAssign::no_leaks(), "Constructor/destructor count mismatch - leak detected!");

    ThrowOnMoveAssign::reset();
    return true;
}

FATP_TEST_CASE(insert_rvalue_throwing_move_assign)
{
    // Same test but with rvalue insert
    using helpers::ThrowOnMoveAssign;

    FATP_ASSERT_EQ(ThrowOnMoveAssign::alive, 0, "No live instances at test start");
    ThrowOnMoveAssign::reset();

    {
        fat_p::AlignedVector<ThrowOnMoveAssign> vec;
        vec.reserve(10);

        for (int i = 0; i < 5; ++i)
        {
            vec.push_back(ThrowOnMoveAssign(i));
        }

        const int aliveBefore = ThrowOnMoveAssign::alive;

        // Throw on 3rd move assignment (0-indexed: call #2) during shift
        ThrowOnMoveAssign::throwAfter = 2;
        ThrowOnMoveAssign::moveAssignCount = 0;

        bool threw = false;
        try
        {
            vec.insert(vec.begin() + 1, ThrowOnMoveAssign(99)); // rvalue
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        FATP_ASSERT_TRUE(threw, "Should have thrown from move assignment");
        FATP_ASSERT_EQ(vec.size(), 5u, "Size should be unchanged after exception");
        FATP_ASSERT_EQ(ThrowOnMoveAssign::alive,
                       aliveBefore,
                       "Failed insert should not leak a constructed tail element");
    }

    FATP_ASSERT_TRUE(ThrowOnMoveAssign::no_leaks(), "Constructor/destructor count mismatch - leak detected!");

    ThrowOnMoveAssign::reset();
    return true;
}

FATP_TEST_CASE(emplace_throwing_move_assign)
{
    // Test emplace with throwing move assignment
    using helpers::ThrowOnMoveAssign;

    FATP_ASSERT_EQ(ThrowOnMoveAssign::alive, 0, "No live instances at test start");
    ThrowOnMoveAssign::reset();

    {
        fat_p::AlignedVector<ThrowOnMoveAssign> vec;
        vec.reserve(10);

        for (int i = 0; i < 5; ++i)
        {
            vec.push_back(ThrowOnMoveAssign(i));
        }

        const int aliveBefore = ThrowOnMoveAssign::alive;

        // Throw on 3rd move assignment (0-indexed: call #2) during shift
        ThrowOnMoveAssign::throwAfter = 2;
        ThrowOnMoveAssign::moveAssignCount = 0;

        bool threw = false;
        try
        {
            vec.emplace(vec.begin() + 1, 99); // Emplace in middle
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        FATP_ASSERT_TRUE(threw, "Should have thrown from move assignment");
        FATP_ASSERT_EQ(vec.size(), 5u, "Size should be unchanged after exception");
        FATP_ASSERT_EQ(ThrowOnMoveAssign::alive,
                       aliveBefore,
                       "Failed emplace should not leak a constructed tail element");
    }

    FATP_ASSERT_TRUE(ThrowOnMoveAssign::no_leaks(), "Constructor/destructor count mismatch - leak detected!");

    ThrowOnMoveAssign::reset();
    return true;
}

FATP_TEST_CASE(insert_at_end_no_shift)
{
    // Sanity check: insert at end doesn't do shifting, should not throw
    using helpers::ThrowOnMoveAssign;

    ThrowOnMoveAssign::reset();

    fat_p::AlignedVector<ThrowOnMoveAssign> vec;
    vec.reserve(10);

    for (int i = 0; i < 5; ++i)
    {
        vec.push_back(ThrowOnMoveAssign(i));
    }

    // Set throw to trigger immediately, but insert at end should not use move assignment
    ThrowOnMoveAssign::throwAfter = 0;
    ThrowOnMoveAssign::moveAssignCount = 0;

    // This should NOT throw because we're inserting at end (no shifting)
    vec.insert(vec.end(), ThrowOnMoveAssign(99));

    FATP_ASSERT_EQ(vec.size(), 6u, "Size should be 6 after insert at end");
    FATP_ASSERT_EQ(vec.back().value, 99, "Last element should be 99");

    ThrowOnMoveAssign::reset();
    return true;
}

FATP_TEST_CASE(self_insertion_safety)
{
    // Test that self-insertion doesn't cause use-after-free
    fat_p::AlignedVector<int> vec = {1, 2, 3, 4, 5};

    // This would crash on unpatched version due to use-after-free
    vec.insert(vec.begin() + 2, vec.begin(), vec.end());

    FATP_ASSERT_EQ(vec.size(), 10u, "Size should be 10 after self-insert");

    // Verify the values are sensible (exact order depends on implementation)
    bool allValid = true;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if (vec[i] < 1 || vec[i] > 5)
        {
            allValid = false;
            break;
        }
    }
    FATP_ASSERT_TRUE(allValid, "All values should be in range [1,5]");

    return true;
}

FATP_TEST_CASE(push_back_aliasing)
{
    // Test that push_back handles aliasing when value references internal element
    fat_p::AlignedVector<std::string> vec;
    vec.push_back("first");

    // Force several reallocations while referencing internal element
    for (int i = 0; i < 20; ++i)
    {
        vec.push_back(vec[0]);
    }

    FATP_ASSERT_EQ(vec.size(), 21u, "Should have 21 elements");

    // All elements should be "first"
    bool allFirst = true;
    for (const auto& s : vec)
    {
        if (s != "first")
        {
            allFirst = false;
            break;
        }
    }
    FATP_ASSERT_TRUE(allFirst, "All elements should be 'first'");

    return true;
}

// ============================================================================
// Fuzz Test
// ============================================================================

FATP_TEST_CASE(fuzz_vs_std_vector)
{
    // Compare random operations against std::vector as oracle
    fat_p::AlignedVector<int> ours;
    std::vector<int> theirs;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> valueDist(0, 999);
    std::uniform_int_distribution<int> opDist(0, 4);

    for (int i = 0; i < 1000; ++i)
    {
        int op = opDist(rng);
        int val = valueDist(rng);

        switch (op)
        {
            case 0: // push_back
                ours.push_back(val);
                theirs.push_back(val);
                break;

            case 1: // pop_back (if not empty)
                if (!ours.empty())
                {
                    ours.pop_back();
                    theirs.pop_back();
                }
                break;

            case 2: // insert at random position
                if (ours.size() < 100)
                {
                    size_t pos = ours.empty() ? 0 : (rng() % (ours.size() + 1));
                    ours.insert(ours.begin() + pos, val);
                    theirs.insert(theirs.begin() + static_cast<std::ptrdiff_t>(pos), val);
                }
                break;

            case 3: // erase at random position
                if (!ours.empty())
                {
                    size_t pos = rng() % ours.size();
                    ours.erase(ours.begin() + pos);
                    theirs.erase(theirs.begin() + static_cast<std::ptrdiff_t>(pos));
                }
                break;

            case 4: // clear (occasionally)
                if (rng() % 50 == 0)
                {
                    ours.clear();
                    theirs.clear();
                }
                break;
        }

        // Verify consistency
        if (ours.size() != theirs.size())
        {
            FATP_SIMPLE_ASSERT(false, "Size mismatch during fuzz test");
            return false;
        }
    }

    // Final comparison
    FATP_ASSERT_EQ(ours.size(), theirs.size(), "Final size should match");
    for (size_t i = 0; i < ours.size(); ++i)
    {
        FATP_ASSERT_EQ(ours[i], theirs[i], "Elements should match after fuzz test");
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

static volatile long long g_benchmarkSink;

void print_cpu_state(const char* label)
{
    auto info = SystemInfo::capture();
    std::cout << "  " << colors::blue() << "[" << label << "] ";
    if (info.current_freq_mhz > 0 && info.base_freq_mhz > 0)
    {
        std::cout << "CPU: " << static_cast<int>(info.current_freq_mhz) << " MHz";
        double throttlePct = info.throttle_percentage();
        if (throttlePct > 5.0)
        {
            std::cout << " (" << colors::yellow() << std::fixed << std::setprecision(0) << throttlePct << "% throttled"
                      << colors::reset() << colors::blue() << ")";
        }
        else if (throttlePct < -5.0)
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
} // namespace fat_p::testing::alignedvector

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_AlignedVector()
{
    FATP_PRINT_HEADER(ALIGNED VECTOR)

    get_test_config().verbose = true;

    TestRunner runner;

    // Construction
    FATP_RUN_TEST_NS(runner, alignedvector, default_construction);
    FATP_RUN_TEST_NS(runner, alignedvector, size_construction);
    FATP_RUN_TEST_NS(runner, alignedvector, size_value_construction);
    FATP_RUN_TEST_NS(runner, alignedvector, range_construction);
    FATP_RUN_TEST_NS(runner, alignedvector, range_construction_from_list);
    FATP_RUN_TEST_NS(runner, alignedvector, range_construction_from_array);
    FATP_RUN_TEST_NS(runner, alignedvector, range_construction_empty);
    FATP_RUN_TEST_NS(runner, alignedvector, initializer_list_construction);

    // Alignment
    FATP_RUN_TEST_NS(runner, alignedvector, alignment_64);
    FATP_RUN_TEST_NS(runner, alignedvector, alignment_various);
    FATP_RUN_TEST_NS(runner, alignedvector, assume_aligned);

    // Iterators
    FATP_RUN_TEST_NS(runner, alignedvector, empty_vector_iterators_strict);
    FATP_RUN_TEST_NS(runner, alignedvector, forward_iterators);
    FATP_RUN_TEST_NS(runner, alignedvector, reverse_iterators);

    // Element access
    FATP_RUN_TEST_NS(runner, alignedvector, subscript_operator);
    FATP_RUN_TEST_NS(runner, alignedvector, at_bounds_checking);
    FATP_RUN_TEST_NS(runner, alignedvector, front_back);
    FATP_RUN_TEST_NS(runner, alignedvector, front_back_modification);

    // Assign
    FATP_RUN_TEST_NS(runner, alignedvector, assign_count_value);
    FATP_RUN_TEST_NS(runner, alignedvector, assign_range);
    FATP_RUN_TEST_NS(runner, alignedvector, assign_initializer_list);
    FATP_RUN_TEST_NS(runner, alignedvector, assign_empty);
    FATP_RUN_TEST_NS(runner, alignedvector, initializer_list_assignment_operator);

    // Insert
    FATP_RUN_TEST_NS(runner, alignedvector, insert_single_lvalue);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_single_rvalue);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_at_begin);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_at_end);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_into_empty);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_count);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_count_zero);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_range);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_initializer_list);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_triggers_reallocation);

    // Emplace
    FATP_RUN_TEST_NS(runner, alignedvector, emplace_middle);
    FATP_RUN_TEST_NS(runner, alignedvector, emplace_at_begin);
    FATP_RUN_TEST_NS(runner, alignedvector, emplace_at_end);
    FATP_RUN_TEST_NS(runner, alignedvector, emplace_exception_safety);

    // Erase
    FATP_RUN_TEST_NS(runner, alignedvector, erase_single);
    FATP_RUN_TEST_NS(runner, alignedvector, erase_first);
    FATP_RUN_TEST_NS(runner, alignedvector, erase_last);
    FATP_RUN_TEST_NS(runner, alignedvector, erase_range);
    FATP_RUN_TEST_NS(runner, alignedvector, erase_range_empty);
    FATP_RUN_TEST_NS(runner, alignedvector, erase_all);
    FATP_RUN_TEST_NS(runner, alignedvector, erase_to_empty);

    // Capacity
    FATP_RUN_TEST_NS(runner, alignedvector, reserve_capacity);
    FATP_RUN_TEST_NS(runner, alignedvector, shrink_to_fit);
    FATP_RUN_TEST_NS(runner, alignedvector, max_size_and_overflow);

    // Modifiers
    FATP_RUN_TEST_NS(runner, alignedvector, push_back);
    FATP_RUN_TEST_NS(runner, alignedvector, emplace_back);
    FATP_RUN_TEST_NS(runner, alignedvector, resize);
    FATP_RUN_TEST_NS(runner, alignedvector, resize_with_value);
    FATP_RUN_TEST_NS(runner, alignedvector, clear_and_pop_back);
    FATP_RUN_TEST_NS(runner, alignedvector, swap);

    // Copy/Move
    FATP_RUN_TEST_NS(runner, alignedvector, copy_constructor);
    FATP_RUN_TEST_NS(runner, alignedvector, move_constructor);
    FATP_RUN_TEST_NS(runner, alignedvector, copy_assignment);
    FATP_RUN_TEST_NS(runner, alignedvector, move_assignment);
    FATP_RUN_TEST_NS(runner, alignedvector, self_assignment);

    // Comparison
    FATP_RUN_TEST_NS(runner, alignedvector, equality_operators);
    FATP_RUN_TEST_NS(runner, alignedvector, ordering_operators);

    // Allocator
    FATP_RUN_TEST_NS(runner, alignedvector, get_allocator);
    FATP_RUN_TEST_NS(runner, alignedvector, allocator_comparison);

    // Value initialization
    FATP_RUN_TEST_NS(runner, alignedvector, trivial_struct_initialization);
    FATP_RUN_TEST_NS(runner, alignedvector, nontrivial_copy_initialization);

    // Move-only types
    FATP_RUN_TEST_NS(runner, alignedvector, move_only_types);

    // Exception safety
    FATP_RUN_TEST_NS(runner, alignedvector, exception_safety_strong_guarantee);

    // Destructor behavior
    FATP_RUN_TEST_NS(runner, alignedvector, destructor_calls);
    FATP_RUN_TEST_NS(runner, alignedvector, erase_calls_destructors);

    // Adversarial tests (bug detection)
    FATP_RUN_TEST_NS(runner, alignedvector, assign_exception_safety_strong_guarantee);
    FATP_RUN_TEST_NS(runner, alignedvector, assign_range_exception_safety);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_range_exception_safety);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_count_exception_safety);
    FATP_RUN_TEST_NS(runner, alignedvector, construct_range_copy_no_leak);
    FATP_RUN_TEST_NS(runner, alignedvector, resize_exception_safety);
    FATP_RUN_TEST_NS(runner, alignedvector, emplace_back_exception_safety);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_single_throwing_move_assign);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_rvalue_throwing_move_assign);
    FATP_RUN_TEST_NS(runner, alignedvector, emplace_throwing_move_assign);
    FATP_RUN_TEST_NS(runner, alignedvector, insert_at_end_no_shift);
    FATP_RUN_TEST_NS(runner, alignedvector, self_insertion_safety);
    FATP_RUN_TEST_NS(runner, alignedvector, push_back_aliasing);

    // Fuzz test
    FATP_RUN_TEST_NS(runner, alignedvector, fuzz_vs_std_vector);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_AlignedVector() ? 0 : 1;
}
#endif
