/**
 * @file test_BitSet.cpp
 * @brief Comprehensive unit tests for BitSet.h
 */
/*
FATP_META:
  meta_version: 1
  component: BitSet
  file_role: test
  path: components/BitSet/tests/test_BitSet.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for BitSet."
  api_stability: in_work
  related:
    docs_search: "BitSet"
    headers:
      - include/fat_p/BitSet.h
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

#include <iostream>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "BitSet.h"
#include "FatPTest.h"

namespace fat_p::testing::bitset
{

// ============================================================================
// Basic Operations
// ============================================================================

FATP_TEST_CASE(basic_operations)
{
    fat_p::BitSet<64> bits;

    FATP_ASSERT_TRUE(bits.none(), "Should start with no bits set");
    FATP_ASSERT_TRUE(!bits.any(), "any() should return false for empty set");
    FATP_ASSERT_EQ(bits.count(), 0u, "Count should be 0");

    bits.set(5);
    FATP_ASSERT_TRUE(bits.test(5), "Bit 5 should be set");
    FATP_ASSERT_TRUE(!bits.test(6), "Bit 6 should not be set");
    FATP_ASSERT_EQ(bits.count(), 1u, "Count should be 1");

    bits.clear(5);
    FATP_ASSERT_TRUE(!bits.test(5), "Bit 5 should be cleared");
    FATP_ASSERT_EQ(bits.count(), 0u, "Count should be 0");

    bits.set(10);
    bits.flip(10);
    FATP_ASSERT_TRUE(!bits.test(10), "Bit 10 should be flipped to 0");

    bits.flip(20);
    FATP_ASSERT_TRUE(bits.test(20), "Bit 20 should be flipped to 1");

    return true;
}

FATP_TEST_CASE(set_with_value)
{
    fat_p::BitSet<64> bits;

    bits.set(10, true);
    FATP_ASSERT_TRUE(bits.test(10), "Bit 10 should be set");

    bits.set(10, false);
    FATP_ASSERT_TRUE(!bits.test(10), "Bit 10 should be cleared");

    bits.set(20, true);
    bits.set(20, true);
    FATP_ASSERT_TRUE(bits.test(20), "Bit 20 should remain set");

    return true;
}

FATP_TEST_CASE(subscript_operator)
{
    fat_p::BitSet<128> bits;

    bits.set(0);
    bits.set(63);
    bits.set(64);
    bits.set(127);

    FATP_ASSERT_TRUE(bits[0], "bits[0] should be true");
    FATP_ASSERT_TRUE(bits[63], "bits[63] should be true");
    FATP_ASSERT_TRUE(bits[64], "bits[64] should be true");
    FATP_ASSERT_TRUE(bits[127], "bits[127] should be true");
    FATP_ASSERT_TRUE(!bits[1], "bits[1] should be false");
    FATP_ASSERT_TRUE(!bits[100], "bits[100] should be false");

    return true;
}

FATP_TEST_CASE(unchecked_operations)
{
    fat_p::BitSet<256> bits;

    bits.setUnchecked(0);
    bits.setUnchecked(100);
    bits.setUnchecked(255);

    FATP_ASSERT_TRUE(bits.test_unchecked(0), "Bit 0 should be set");
    FATP_ASSERT_TRUE(bits.test_unchecked(100), "Bit 100 should be set");
    FATP_ASSERT_TRUE(bits.test_unchecked(255), "Bit 255 should be set");
    FATP_ASSERT_TRUE(!bits.test_unchecked(50), "Bit 50 should not be set");

    bits.flipUnchecked(100);
    FATP_ASSERT_TRUE(!bits.test_unchecked(100), "Bit 100 should be flipped");

    bits.clearUnchecked(0);
    FATP_ASSERT_TRUE(!bits.test_unchecked(0), "Bit 0 should be cleared");

    return true;
}

// ============================================================================
// Boundary Tests
// ============================================================================

FATP_TEST_CASE(size_one)
{
    fat_p::BitSet<1> bits;

    FATP_ASSERT_TRUE(bits.none(), "Should start empty");
    FATP_ASSERT_EQ(bits.size(), 1u, "Size should be 1");

    bits.set(0);
    FATP_ASSERT_TRUE(bits.test(0), "Bit 0 should be set");
    FATP_ASSERT_TRUE(bits.all(), "all() should return true");
    FATP_ASSERT_EQ(bits.count(), 1u, "Count should be 1");

    bits.flipAll();
    FATP_ASSERT_TRUE(bits.none(), "Should be empty after flip");

    return true;
}

FATP_TEST_CASE(size_63)
{
    fat_p::BitSet<63> bits;

    FATP_ASSERT_EQ(bits.size(), 63u, "Size should be 63");

    bits.setAll();
    FATP_ASSERT_EQ(bits.count(), 63u, "Count should be 63");
    FATP_ASSERT_TRUE(bits.all(), "all() should return true");

    bits.clear(62);
    FATP_ASSERT_TRUE(!bits.all(), "all() should return false after clearing one bit");
    FATP_ASSERT_EQ(bits.count(), 62u, "Count should be 62");

    return true;
}

FATP_TEST_CASE(size_64)
{
    fat_p::BitSet<64> bits;

    FATP_ASSERT_EQ(bits.size(), 64u, "Size should be 64");

    bits.setAll();
    FATP_ASSERT_EQ(bits.count(), 64u, "Count should be 64");
    FATP_ASSERT_TRUE(bits.all(), "all() should return true");

    return true;
}

FATP_TEST_CASE(size_65)
{
    fat_p::BitSet<65> bits;

    FATP_ASSERT_EQ(bits.size(), 65u, "Size should be 65");

    bits.setAll();
    FATP_ASSERT_EQ(bits.count(), 65u, "Count should be 65");
    FATP_ASSERT_TRUE(bits.all(), "all() should return true");

    bits.clear(64);
    FATP_ASSERT_EQ(bits.count(), 64u, "Count should be 64");
    FATP_ASSERT_TRUE(!bits.all(), "all() should return false");

    return true;
}

FATP_TEST_CASE(size_large)
{
    fat_p::BitSet<1000> bits;

    FATP_ASSERT_EQ(bits.size(), 1000u, "Size should be 1000");

    bits.set(0);
    bits.set(499);
    bits.set(999);
    FATP_ASSERT_EQ(bits.count(), 3u, "Count should be 3");

    bits.setAll();
    FATP_ASSERT_EQ(bits.count(), 1000u, "Count should be 1000");

    return true;
}

// ============================================================================
// Bulk Operations
// ============================================================================

FATP_TEST_CASE(bulk_operations)
{
    fat_p::BitSet<128> bits;

    bits.setAll();
    FATP_ASSERT_TRUE(bits.all(), "All bits should be set");
    FATP_ASSERT_EQ(bits.count(), 128u, "Should have 128 bits set");

    bits.clearAll();
    FATP_ASSERT_TRUE(bits.none(), "No bits should be set");
    FATP_ASSERT_EQ(bits.count(), 0u, "Should have 0 bits set");

    bits.set(10);
    bits.set(20);
    bits.set(30);
    FATP_ASSERT_EQ(bits.count(), 3u, "Should have 3 bits set");

    bits.flipAll();
    FATP_ASSERT_EQ(bits.count(), 125u, "Should have 125 bits set");
    FATP_ASSERT_TRUE(!bits.test(10), "Bit 10 should be flipped");
    FATP_ASSERT_TRUE(!bits.test(20), "Bit 20 should be flipped");
    FATP_ASSERT_TRUE(!bits.test(30), "Bit 30 should be flipped");

    return true;
}

FATP_TEST_CASE(reset_alias)
{
    fat_p::BitSet<64> bits;

    bits.setAll();
    bits.reset();
    FATP_ASSERT_TRUE(bits.none(), "reset() should clear all bits");

    bits.set(10);
    bits.reset(10);
    FATP_ASSERT_TRUE(!bits.test(10), "reset(index) should clear that bit");

    return true;
}

// ============================================================================
// Range Operations
// ============================================================================

FATP_TEST_CASE(setRange)
{
    fat_p::BitSet<256> bits;

    bits.setRange(10, 20);
    FATP_ASSERT_EQ(bits.count(), 10u, "Should have 10 bits set");
    FATP_ASSERT_TRUE(bits.test(10), "Bit 10 should be set");
    FATP_ASSERT_TRUE(bits.test(19), "Bit 19 should be set");
    FATP_ASSERT_TRUE(!bits.test(9), "Bit 9 should not be set");
    FATP_ASSERT_TRUE(!bits.test(20), "Bit 20 should not be set");

    bits.clearAll();
    bits.setRange(60, 70);
    FATP_ASSERT_EQ(bits.count(), 10u, "Should have 10 bits spanning word boundary");
    FATP_ASSERT_TRUE(bits.test(63), "Bit 63 should be set");
    FATP_ASSERT_TRUE(bits.test(64), "Bit 64 should be set");

    bits.clearAll();
    bits.setRange(0, 256);
    FATP_ASSERT_EQ(bits.count(), 256u, "Full range should set all bits");

    bits.clearAll();
    bits.setRange(100, 100);
    FATP_ASSERT_EQ(bits.count(), 0u, "Empty range should set no bits");

    return true;
}

FATP_TEST_CASE(clearRange)
{
    fat_p::BitSet<256> bits;

    bits.setAll();
    bits.clearRange(10, 20);
    FATP_ASSERT_EQ(bits.count(), 246u, "Should have 246 bits set");
    FATP_ASSERT_TRUE(!bits.test(10), "Bit 10 should be cleared");
    FATP_ASSERT_TRUE(!bits.test(19), "Bit 19 should be cleared");
    FATP_ASSERT_TRUE(bits.test(9), "Bit 9 should still be set");
    FATP_ASSERT_TRUE(bits.test(20), "Bit 20 should still be set");

    bits.setAll();
    bits.clearRange(60, 70);
    FATP_ASSERT_EQ(bits.count(), 246u, "Should clear bits spanning word boundary");

    return true;
}

FATP_TEST_CASE(flipRange)
{
    fat_p::BitSet<256> bits;

    bits.flipRange(10, 20);
    FATP_ASSERT_EQ(bits.count(), 10u, "Should have 10 bits set");
    FATP_ASSERT_TRUE(bits.test(10), "Bit 10 should be set");
    FATP_ASSERT_TRUE(bits.test(19), "Bit 19 should be set");

    bits.flipRange(10, 20);
    FATP_ASSERT_TRUE(bits.none(), "Double flip should clear all");

    bits.flipRange(60, 70);
    FATP_ASSERT_EQ(bits.count(), 10u, "Should span word boundary");
    FATP_ASSERT_TRUE(bits.test(63), "Bit 63 set");
    FATP_ASSERT_TRUE(bits.test(64), "Bit 64 set");

    FATP_ASSERT_THROWS(bits.flipRange(20, 10), std::out_of_range, "Invalid range");
    FATP_ASSERT_THROWS(bits.flipRange(0, 257), std::out_of_range, "Out of bounds");

    return true;
}

FATP_TEST_CASE(count_range)
{
    fat_p::BitSet<256> bits;

    bits.setRange(0, 100);
    FATP_ASSERT_EQ(bits.count_range(0, 100), 100u, "Should count 100 bits");
    FATP_ASSERT_EQ(bits.count_range(0, 50), 50u, "Should count 50 bits");
    FATP_ASSERT_EQ(bits.count_range(50, 100), 50u, "Should count 50 bits");
    FATP_ASSERT_EQ(bits.count_range(100, 200), 0u, "Should count 0 bits outside range");

    bits.clearAll();
    bits.set(63);
    bits.set(64);
    FATP_ASSERT_EQ(bits.count_range(60, 70), 2u, "Should count bits at word boundary");

    return true;
}

// ============================================================================
// Find Operations
// ============================================================================

FATP_TEST_CASE(find_operations)
{
    fat_p::BitSet<256> bits;

    bits.set(10);
    bits.set(50);
    bits.set(200);

    FATP_ASSERT_EQ(bits.find_first(), 10u, "First bit should be 10");
    FATP_ASSERT_EQ(bits.find_next(10), 50u, "Next after 10 should be 50");
    FATP_ASSERT_EQ(bits.find_next(50), 200u, "Next after 50 should be 200");
    FATP_ASSERT_EQ(bits.find_next(200), 256u, "No next after 200");
    FATP_ASSERT_EQ(bits.find_last(), 200u, "Last bit should be 200");

    bits.clearAll();
    FATP_ASSERT_EQ(bits.find_first(), 256u, "find_first on empty should return N");
    FATP_ASSERT_EQ(bits.find_last(), 256u, "find_last on empty should return N");

    return true;
}

FATP_TEST_CASE(find_at_boundaries)
{
    fat_p::BitSet<128> bits;

    bits.set(0);
    FATP_ASSERT_EQ(bits.find_first(), 0u, "Should find bit 0");
    FATP_ASSERT_EQ(bits.find_last(), 0u, "Last should also be 0");

    bits.clearAll();
    bits.set(127);
    FATP_ASSERT_EQ(bits.find_first(), 127u, "Should find bit 127");
    FATP_ASSERT_EQ(bits.find_last(), 127u, "Last should also be 127");

    bits.set(63);
    bits.set(64);
    FATP_ASSERT_EQ(bits.find_next(63), 64u, "Should find bit across word boundary");

    return true;
}

FATP_TEST_CASE(find_next_last_index)
{
    fat_p::BitSet<64> bits;
    bits.set(63);

    FATP_ASSERT_EQ(bits.find_next(62), 63u, "Should find bit 63");
    FATP_ASSERT_EQ(bits.find_next(63), 64u, "Nothing after last index");

    bits.clearAll();
    FATP_ASSERT_EQ(bits.find_next(62), 64u, "Empty set returns N");
    FATP_ASSERT_EQ(bits.find_next(63), 64u, "find_next(N-1) returns N");

    return true;
}

FATP_TEST_CASE(find_next_at_word_boundary)
{
    fat_p::BitSet<128> bits;
    bits.set(64);

    FATP_ASSERT_EQ(bits.find_next(63), 64u, "Should cross word boundary");
    FATP_ASSERT_EQ(bits.find_next(64), 128u, "Nothing after 64");

    bits.set(127);
    FATP_ASSERT_EQ(bits.find_next(64), 127u, "Should find 127");
    FATP_ASSERT_EQ(bits.find_next(127), 128u, "Nothing after last");

    return true;
}

FATP_TEST_CASE(find_prev_operations)
{
    fat_p::BitSet<256> bits;
    bits.set(10);
    bits.set(50);
    bits.set(200);

    FATP_ASSERT_EQ(bits.find_prev(200), 50u, "Prev before 200 should be 50");
    FATP_ASSERT_EQ(bits.find_prev(201), 200u, "Prev before 201 should be 200");
    FATP_ASSERT_EQ(bits.find_prev(11), 10u, "Prev before 11 should be 10");
    FATP_ASSERT_EQ(bits.find_prev(10), 256u, "Prev before 10 should be none");
    FATP_ASSERT_EQ(bits.find_prev(0), 256u, "Prev before 0 should be none");
    FATP_ASSERT_EQ(bits.find_prev(256), 200u, "Prev before N should be last");

    return true;
}

FATP_TEST_CASE(find_zero_operations)
{
    fat_p::BitSet<256> bits;
    bits.setAll();

    FATP_ASSERT_EQ(bits.find_first_zero(), 256u, "No zeros when all set");
    FATP_ASSERT_EQ(bits.find_last_zero(), 256u, "No zeros when all set");

    bits.clear(10);
    bits.clear(100);
    bits.clear(200);

    FATP_ASSERT_EQ(bits.find_first_zero(), 10u, "First zero at 10");
    FATP_ASSERT_EQ(bits.find_next_zero(10), 100u, "Next zero after 10 is 100");
    FATP_ASSERT_EQ(bits.find_next_zero(100), 200u, "Next zero after 100 is 200");
    FATP_ASSERT_EQ(bits.find_next_zero(200), 256u, "No zero after 200");
    FATP_ASSERT_EQ(bits.find_last_zero(), 200u, "Last zero at 200");

    bits.clearAll();
    FATP_ASSERT_EQ(bits.find_first_zero(), 0u, "First zero at 0");
    FATP_ASSERT_EQ(bits.find_last_zero(), 255u, "Last zero at 255");

    return true;
}

FATP_TEST_CASE(find_zero_boundaries)
{
    fat_p::BitSet<65> bits;
    bits.setAll();

    bits.clear(0);
    FATP_ASSERT_EQ(bits.find_first_zero(), 0u, "Zero at position 0");

    bits.set(0);
    bits.clear(64);
    FATP_ASSERT_EQ(bits.find_first_zero(), 64u, "Zero at word boundary");
    FATP_ASSERT_EQ(bits.find_last_zero(), 64u, "Last zero at 64");

    bits.set(64);
    bits.clear(63);
    FATP_ASSERT_EQ(bits.find_first_zero(), 63u, "Zero at 63");

    return true;
}

// ============================================================================
// Iteration
// ============================================================================

FATP_TEST_CASE(iteration)
{
    fat_p::BitSet<128> bits;

    bits.set(5);
    bits.set(15);
    bits.set(25);
    bits.set(100);

    std::vector<size_t> indices;
    for (size_t idx : bits)
    {
        indices.push_back(idx);
    }

    FATP_ASSERT_EQ(indices.size(), 4u, "Should iterate over 4 bits");
    FATP_ASSERT_EQ(indices[0], 5u, "First should be 5");
    FATP_ASSERT_EQ(indices[1], 15u, "Second should be 15");
    FATP_ASSERT_EQ(indices[2], 25u, "Third should be 25");
    FATP_ASSERT_EQ(indices[3], 100u, "Fourth should be 100");

    return true;
}

FATP_TEST_CASE(iteration_empty)
{
    fat_p::BitSet<64> bits;

    std::vector<size_t> indices;
    for (size_t idx : bits)
    {
        indices.push_back(idx);
    }

    FATP_ASSERT_TRUE(indices.empty(), "Empty bitset should yield no iterations");

    return true;
}

FATP_TEST_CASE(iterator_traits)
{
    using Iterator = fat_p::BitSet<64>::Iterator;

    static_assert(std::is_same_v<Iterator::iterator_category, std::forward_iterator_tag>, "Should be forward iterator");
    static_assert(std::is_same_v<Iterator::value_type, size_t>, "Value type should be size_t");
    static_assert(std::is_same_v<Iterator::difference_type, std::ptrdiff_t>, "Difference type should be ptrdiff_t");

    return true;
}

// ============================================================================
// Bitwise Operations
// ============================================================================

FATP_TEST_CASE(bitwise_and)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);
    a.set(3);

    b.set(2);
    b.set(3);
    b.set(4);

    auto result = a & b;
    FATP_ASSERT_EQ(result.count(), 2u, "AND should have 2 bits (2,3)");
    FATP_ASSERT_TRUE(result.test(2), "Bit 2 should be set");
    FATP_ASSERT_TRUE(result.test(3), "Bit 3 should be set");
    FATP_ASSERT_TRUE(!result.test(1), "Bit 1 should not be set");
    FATP_ASSERT_TRUE(!result.test(4), "Bit 4 should not be set");

    return true;
}

FATP_TEST_CASE(bitwise_or)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);

    b.set(3);
    b.set(4);

    auto result = a | b;
    FATP_ASSERT_EQ(result.count(), 4u, "OR should have 4 bits");
    FATP_ASSERT_TRUE(result.test(1), "Bit 1 should be set");
    FATP_ASSERT_TRUE(result.test(2), "Bit 2 should be set");
    FATP_ASSERT_TRUE(result.test(3), "Bit 3 should be set");
    FATP_ASSERT_TRUE(result.test(4), "Bit 4 should be set");

    return true;
}

FATP_TEST_CASE(bitwise_xor)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);
    a.set(3);

    b.set(2);
    b.set(3);
    b.set(4);

    auto result = a ^ b;
    FATP_ASSERT_EQ(result.count(), 2u, "XOR should have 2 bits (1,4)");
    FATP_ASSERT_TRUE(result.test(1), "Bit 1 should be set");
    FATP_ASSERT_TRUE(result.test(4), "Bit 4 should be set");
    FATP_ASSERT_TRUE(!result.test(2), "Bit 2 should not be set");
    FATP_ASSERT_TRUE(!result.test(3), "Bit 3 should not be set");

    return true;
}

FATP_TEST_CASE(bitwise_not)
{
    fat_p::BitSet<64> bits;

    bits.set(0);
    bits.set(63);

    auto result = ~bits;
    FATP_ASSERT_EQ(result.count(), 62u, "NOT should have 62 bits");
    FATP_ASSERT_TRUE(!result.test(0), "Bit 0 should not be set");
    FATP_ASSERT_TRUE(!result.test(63), "Bit 63 should not be set");
    FATP_ASSERT_TRUE(result.test(1), "Bit 1 should be set");
    FATP_ASSERT_TRUE(result.test(32), "Bit 32 should be set");

    return true;
}

FATP_TEST_CASE(compound_assignment)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);
    b.set(2);
    b.set(3);

    fat_p::BitSet<64> test_and = a;
    test_and &= b;
    FATP_ASSERT_EQ(test_and.count(), 1u, "&= should leave bit 2");
    FATP_ASSERT_TRUE(test_and.test(2), "Bit 2 should be set");

    fat_p::BitSet<64> test_or = a;
    test_or |= b;
    FATP_ASSERT_EQ(test_or.count(), 3u, "|= should have 3 bits");

    fat_p::BitSet<64> test_xor = a;
    test_xor ^= b;
    FATP_ASSERT_EQ(test_xor.count(), 2u, "^= should have 2 bits");

    return true;
}

// ============================================================================
// Shift Operations
// ============================================================================

FATP_TEST_CASE(left_shift)
{
    fat_p::BitSet<128> bits;
    bits.set(0);
    bits.set(1);
    bits.set(63);

    auto shifted = bits << 1;
    FATP_ASSERT_TRUE(!shifted.test(0), "Bit 0 should be empty");
    FATP_ASSERT_TRUE(shifted.test(1), "Bit 1 should be set");
    FATP_ASSERT_TRUE(shifted.test(2), "Bit 2 should be set");
    FATP_ASSERT_TRUE(shifted.test(64), "Bit 64 should be set (was 63)");

    auto word_shift = bits << 64;
    FATP_ASSERT_TRUE(word_shift.test(64), "Bit 64 should be set (was 0)");
    FATP_ASSERT_TRUE(word_shift.test(65), "Bit 65 should be set (was 1)");
    FATP_ASSERT_TRUE(word_shift.test(127), "Bit 127 should be set (was 63)");

    auto empty = bits << 128;
    FATP_ASSERT_TRUE(empty.none(), "Shift by N should empty the set");

    auto same = bits << 0;
    FATP_ASSERT_TRUE(same == bits, "Shift by 0 should be identity");

    return true;
}

FATP_TEST_CASE(right_shift)
{
    fat_p::BitSet<128> bits;
    bits.set(1);
    bits.set(64);
    bits.set(127);

    auto shifted = bits >> 1;
    FATP_ASSERT_TRUE(shifted.test(0), "Bit 0 should be set (was 1)");
    FATP_ASSERT_TRUE(shifted.test(63), "Bit 63 should be set (was 64)");
    FATP_ASSERT_TRUE(shifted.test(126), "Bit 126 should be set (was 127)");
    FATP_ASSERT_TRUE(!shifted.test(127), "Bit 127 should be empty");

    return true;
}

FATP_TEST_CASE(shift_assignment)
{
    fat_p::BitSet<64> bits;
    bits.set(0);
    bits.set(10);

    bits <<= 5;
    FATP_ASSERT_TRUE(bits.test(5), "Bit 5 should be set");
    FATP_ASSERT_TRUE(bits.test(15), "Bit 15 should be set");
    FATP_ASSERT_TRUE(!bits.test(0), "Bit 0 should be empty");

    bits >>= 5;
    FATP_ASSERT_TRUE(bits.test(0), "Back to bit 0");
    FATP_ASSERT_TRUE(bits.test(10), "Back to bit 10");

    return true;
}

// ============================================================================
// Set Operations
// ============================================================================

FATP_TEST_CASE(isSubsetOf)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);

    b.set(1);
    b.set(2);
    b.set(3);

    FATP_ASSERT_TRUE(a.isSubsetOf(b), "a should be subset of b");
    FATP_ASSERT_TRUE(!b.isSubsetOf(a), "b should not be subset of a");

    fat_p::BitSet<64> empty;
    FATP_ASSERT_TRUE(empty.isSubsetOf(a), "Empty set is subset of any set");
    FATP_ASSERT_TRUE(a.isSubsetOf(a), "Set is subset of itself");

    return true;
}

FATP_TEST_CASE(intersects)
{
    fat_p::BitSet<64> a, b, c;

    a.set(1);
    a.set(2);

    b.set(2);
    b.set(3);

    c.set(10);
    c.set(20);

    FATP_ASSERT_TRUE(a.intersects(b), "a and b should intersect at bit 2");
    FATP_ASSERT_TRUE(!a.intersects(c), "a and c should not intersect");

    fat_p::BitSet<64> empty;
    FATP_ASSERT_TRUE(!a.intersects(empty), "No set intersects empty set");

    return true;
}

// ============================================================================
// Comparison Operations
// ============================================================================

FATP_TEST_CASE(equality)
{
    fat_p::BitSet<64> a, b, c;

    a.set(1);
    a.set(10);
    a.set(50);

    b.set(1);
    b.set(10);
    b.set(50);

    c.set(1);
    c.set(10);

    FATP_ASSERT_TRUE(a == b, "a and b should be equal");
    FATP_ASSERT_TRUE(!(a != b), "a and b should not be not-equal");
    FATP_ASSERT_TRUE(a != c, "a and c should not be equal");
    FATP_ASSERT_TRUE(!(a == c), "a and c should not be equal");

    fat_p::BitSet<64> empty1, empty2;
    FATP_ASSERT_TRUE(empty1 == empty2, "Empty sets should be equal");

    return true;
}

// ============================================================================
// Constructor Tests
// ============================================================================

FATP_TEST_CASE(initializer_list_constructor)
{
    fat_p::BitSet<100> bits{5, 10, 15, 99};

    FATP_ASSERT_EQ(bits.count(), 4u, "Should have 4 bits set");
    FATP_ASSERT_TRUE(bits.test(5), "Bit 5 should be set");
    FATP_ASSERT_TRUE(bits.test(10), "Bit 10 should be set");
    FATP_ASSERT_TRUE(bits.test(15), "Bit 15 should be set");
    FATP_ASSERT_TRUE(bits.test(99), "Bit 99 should be set");
    FATP_ASSERT_TRUE(!bits.test(0), "Bit 0 should not be set");

    fat_p::BitSet<64> empty{};
    FATP_ASSERT_TRUE(empty.none(), "Empty initializer list should yield empty set");

    return true;
}

FATP_TEST_CASE(copy_operations)
{
    fat_p::BitSet<128> original;
    original.set(10);
    original.set(100);

    fat_p::BitSet<128> copied = original;
    FATP_ASSERT_EQ(copied.count(), 2u, "Copied should have 2 bits");
    FATP_ASSERT_TRUE(copied.test(10), "Copied bit 10");
    FATP_ASSERT_TRUE(copied.test(100), "Copied bit 100");

    copied.set(50);
    FATP_ASSERT_TRUE(!original.test(50), "Original should not be affected");

    fat_p::BitSet<128> assigned;
    assigned = original;
    FATP_ASSERT_EQ(assigned.count(), 2u, "Assigned should have 2 bits");

    return true;
}

FATP_TEST_CASE(move_construction)
{
    fat_p::BitSet<128> original;
    original.set(10);
    original.set(100);

    fat_p::BitSet<128> moved = std::move(original);
    FATP_ASSERT_EQ(moved.count(), 2u, "Moved should have 2 bits");
    FATP_ASSERT_TRUE(moved.test(10), "Bit 10 preserved");
    FATP_ASSERT_TRUE(moved.test(100), "Bit 100 preserved");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    fat_p::BitSet<128> original;
    original.set(10);
    original.set(100);

    fat_p::BitSet<128> target;
    target.set(50);

    target = std::move(original);
    FATP_ASSERT_EQ(target.count(), 2u, "Target should have 2 bits");
    FATP_ASSERT_TRUE(target.test(10), "Bit 10 present");
    FATP_ASSERT_TRUE(target.test(100), "Bit 100 present");
    FATP_ASSERT_TRUE(!target.test(50), "Old bit 50 gone");

    return true;
}

FATP_TEST_CASE(self_assignment)
{
    fat_p::BitSet<64> bits;
    bits.set(10);
    bits.set(20);
    bits.set(30);

    // Intentional self-assignment to test operator= safety
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    bits = bits;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    FATP_ASSERT_EQ(bits.count(), 3u, "Self-assignment preserves count");
    FATP_ASSERT_TRUE(bits.test(10), "Bit 10 preserved");
    FATP_ASSERT_TRUE(bits.test(20), "Bit 20 preserved");
    FATP_ASSERT_TRUE(bits.test(30), "Bit 30 preserved");

    return true;
}

FATP_TEST_CASE(to_string_basic)
{
    fat_p::BitSet<8> bits;
    bits.set(0);
    bits.set(2);
    bits.set(7);

    FATP_ASSERT_EQ(bits.to_string(), std::string("10000101"), "MSB-first string");
    FATP_ASSERT_EQ(bits.to_string('.', '#'), std::string("#....#.#"), "Custom chars");

    return true;
}

FATP_TEST_CASE(to_integral)
{
    fat_p::BitSet<64> bits;
    bits.set(0);
    bits.set(5);
    bits.set(20);

    const unsigned long expected_ul = (1UL << 0) | (1UL << 5) | (1UL << 20);
    const unsigned long long expected_ull = (1ULL << 0) | (1ULL << 5) | (1ULL << 20);

    FATP_ASSERT_EQ(bits.to_ulong(), expected_ul, "to_ulong matches low bits");
    FATP_ASSERT_EQ(bits.to_ullong(), expected_ull, "to_ullong matches low bits");

    fat_p::BitSet<128> too_big;
    too_big.set(70);
    FATP_ASSERT_THROWS(too_big.to_ulong(), std::overflow_error, "to_ulong overflow");

    fat_p::BitSet<65> too_big_ull;
    too_big_ull.set(64);
    FATP_ASSERT_THROWS(too_big_ull.to_ullong(), std::overflow_error, "to_ullong overflow");

    return true;
}

FATP_TEST_CASE(hamming_distance_and_disjoint)
{
    fat_p::BitSet<64> a;
    fat_p::BitSet<64> b;

    a.set(1);
    a.set(2);
    a.set(10);

    b.set(2);
    b.set(3);

    FATP_ASSERT_EQ(a.hamming_distance(b), 3u, "Hamming distance");
    FATP_ASSERT_TRUE(!a.isDisjoint(b), "Shared bit means not disjoint");

    fat_p::BitSet<64> c = a;
    c.set(20);
    FATP_ASSERT_TRUE(a.isProperSubsetOf(c), "Proper subset");
    FATP_ASSERT_TRUE(!c.isProperSubsetOf(a), "Not a proper subset");

    fat_p::BitSet<64> d;
    d.set(50);
    FATP_ASSERT_TRUE(a.isDisjoint(d), "Disjoint sets");

    return true;
}

// ============================================================================
// Exception Tests
// ============================================================================

FATP_TEST_CASE(out_of_range_exceptions)
{
    fat_p::BitSet<64> bits;

    FATP_ASSERT_THROWS(bits.set(64), std::out_of_range, "set(64) should throw");
    FATP_ASSERT_THROWS(bits.clear(100), std::out_of_range, "clear(100) should throw");
    FATP_ASSERT_THROWS(bits.flip(64), std::out_of_range, "flip(64) should throw");
    FATP_ASSERT_THROWS(bits.test(1000), std::out_of_range, "test(1000) should throw");
    FATP_ASSERT_THROWS(bits.reset(64), std::out_of_range, "reset(64) should throw");

    return true;
}

FATP_TEST_CASE(initializer_list_out_of_range)
{
    FATP_ASSERT_THROWS((fat_p::BitSet<64>{10, 64}), std::out_of_range, "Initializer list out of range");
    FATP_ASSERT_THROWS((fat_p::BitSet<64>{0, 100}), std::out_of_range, "Initializer list out of range");

    return true;
}

FATP_TEST_CASE(range_exceptions)
{
    fat_p::BitSet<64> bits;

    FATP_ASSERT_THROWS(bits.setRange(10, 5), std::out_of_range, "Invalid range should throw");
    FATP_ASSERT_THROWS(bits.setRange(0, 65), std::out_of_range, "Out of bounds range should throw");
    FATP_ASSERT_THROWS(bits.clearRange(100, 200), std::out_of_range, "Out of bounds should throw");
    FATP_ASSERT_THROWS(bits.count_range(50, 100), std::out_of_range, "Out of bounds should throw");

    return true;
}

FATP_TEST_CASE(range_word_boundaries)
{
    fat_p::BitSet<128> bits;

    bits.setRange(0, 64);
    FATP_ASSERT_EQ(bits.count(), 64u, "Should have exactly 64 bits set");
    FATP_ASSERT_TRUE(bits.test(0), "Bit 0 should be set");
    FATP_ASSERT_TRUE(bits.test(63), "Bit 63 should be set");
    FATP_ASSERT_TRUE(!bits.test(64), "Bit 64 should not be set");

    bits.clearAll();
    bits.setRange(64, 128);
    FATP_ASSERT_EQ(bits.count(), 64u, "Should have exactly 64 bits set");
    FATP_ASSERT_TRUE(!bits.test(63), "Bit 63 should not be set");
    FATP_ASSERT_TRUE(bits.test(64), "Bit 64 should be set");
    FATP_ASSERT_TRUE(bits.test(127), "Bit 127 should be set");

    bits.clearAll();
    bits.setRange(0, 128);
    FATP_ASSERT_EQ(bits.count(), 128u, "Full range should set all bits");
    FATP_ASSERT_TRUE(bits.all(), "all() should return true");

    return true;
}

// ============================================================================
// Hash Tests
// ============================================================================

FATP_TEST_CASE(std_hash)
{
    fat_p::BitSet<64> a, b, c;

    a.set(1);
    a.set(10);

    b.set(1);
    b.set(10);

    c.set(1);
    c.set(20);

    std::hash<fat_p::BitSet<64>> hasher;

    FATP_ASSERT_EQ(hasher(a), hasher(b), "Equal sets should have equal hashes");
    FATP_ASSERT_TRUE(hasher(a) != hasher(c), "Different sets should likely have different hashes");

    std::unordered_set<fat_p::BitSet<64>> set;
    set.insert(a);
    set.insert(c);
    FATP_ASSERT_EQ(set.size(), 2u, "Should have 2 distinct sets");
    FATP_ASSERT_EQ(set.count(b), 1u, "Should find b (equal to a)");

    return true;
}

// ============================================================================
// Data Access Tests
// ============================================================================

FATP_TEST_CASE(data_access)
{
    fat_p::BitSet<128> bits;

    bits.set(0);
    bits.set(64);

    const uint64_t* data = bits.data();
    FATP_ASSERT_EQ(data[0], 1ULL, "First word should have bit 0 set");
    FATP_ASSERT_EQ(data[1], 1ULL, "Second word should have bit 0 set (bit 64 overall)");

    FATP_ASSERT_EQ(fat_p::BitSet<128>::wordCount(), 2u, "128-bit set should have 2 words");
    FATP_ASSERT_EQ(fat_p::BitSet<65>::wordCount(), 2u, "65-bit set should have 2 words");
    FATP_ASSERT_EQ(fat_p::BitSet<64>::wordCount(), 1u, "64-bit set should have 1 word");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================
} // namespace fat_p::testing::bitset

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_BitSet()
{
    FATP_PRINT_HEADER(BIT SET)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, bitset, basic_operations);
    FATP_RUN_TEST_NS(runner, bitset, set_with_value);
    FATP_RUN_TEST_NS(runner, bitset, subscript_operator);
    FATP_RUN_TEST_NS(runner, bitset, unchecked_operations);

    FATP_RUN_TEST_NS(runner, bitset, size_one);
    FATP_RUN_TEST_NS(runner, bitset, size_63);
    FATP_RUN_TEST_NS(runner, bitset, size_64);
    FATP_RUN_TEST_NS(runner, bitset, size_65);
    FATP_RUN_TEST_NS(runner, bitset, size_large);

    FATP_RUN_TEST_NS(runner, bitset, bulk_operations);
    FATP_RUN_TEST_NS(runner, bitset, reset_alias);

    FATP_RUN_TEST_NS(runner, bitset, setRange);
    FATP_RUN_TEST_NS(runner, bitset, clearRange);
    FATP_RUN_TEST_NS(runner, bitset, flipRange);
    FATP_RUN_TEST_NS(runner, bitset, count_range);

    FATP_RUN_TEST_NS(runner, bitset, find_operations);
    FATP_RUN_TEST_NS(runner, bitset, find_at_boundaries);
    FATP_RUN_TEST_NS(runner, bitset, find_next_last_index);
    FATP_RUN_TEST_NS(runner, bitset, find_next_at_word_boundary);
    FATP_RUN_TEST_NS(runner, bitset, find_prev_operations);
    FATP_RUN_TEST_NS(runner, bitset, find_zero_operations);
    FATP_RUN_TEST_NS(runner, bitset, find_zero_boundaries);

    FATP_RUN_TEST_NS(runner, bitset, iteration);
    FATP_RUN_TEST_NS(runner, bitset, iteration_empty);
    FATP_RUN_TEST_NS(runner, bitset, iterator_traits);

    FATP_RUN_TEST_NS(runner, bitset, bitwise_and);
    FATP_RUN_TEST_NS(runner, bitset, bitwise_or);
    FATP_RUN_TEST_NS(runner, bitset, bitwise_xor);
    FATP_RUN_TEST_NS(runner, bitset, bitwise_not);
    FATP_RUN_TEST_NS(runner, bitset, compound_assignment);

    FATP_RUN_TEST_NS(runner, bitset, left_shift);
    FATP_RUN_TEST_NS(runner, bitset, right_shift);
    FATP_RUN_TEST_NS(runner, bitset, shift_assignment);

    FATP_RUN_TEST_NS(runner, bitset, isSubsetOf);
    FATP_RUN_TEST_NS(runner, bitset, intersects);
    FATP_RUN_TEST_NS(runner, bitset, hamming_distance_and_disjoint);

    FATP_RUN_TEST_NS(runner, bitset, equality);

    FATP_RUN_TEST_NS(runner, bitset, initializer_list_constructor);
    FATP_RUN_TEST_NS(runner, bitset, copy_operations);
    FATP_RUN_TEST_NS(runner, bitset, move_construction);
    FATP_RUN_TEST_NS(runner, bitset, move_assignment);
    FATP_RUN_TEST_NS(runner, bitset, self_assignment);

    FATP_RUN_TEST_NS(runner, bitset, to_string_basic);
    FATP_RUN_TEST_NS(runner, bitset, to_integral);

    FATP_RUN_TEST_NS(runner, bitset, out_of_range_exceptions);
    FATP_RUN_TEST_NS(runner, bitset, initializer_list_out_of_range);
    FATP_RUN_TEST_NS(runner, bitset, range_exceptions);
    FATP_RUN_TEST_NS(runner, bitset, range_word_boundaries);

    FATP_RUN_TEST_NS(runner, bitset, std_hash);
    FATP_RUN_TEST_NS(runner, bitset, data_access);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_BitSet() ? 0 : 1;
}
#endif
