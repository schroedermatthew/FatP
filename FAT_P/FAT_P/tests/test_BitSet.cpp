/**
 * @file test_BitSet.cpp
 * @brief Comprehensive unit tests for BitSet.h
 */
/*
FATP_META:
  meta_version: 1
  component: BitSet
  file_role: test
  path: tests/test_BitSet.cpp
  namespace: fat_p
  summary: "Unit tests for BitSet."
  related:
    docs_search: "BitSet"
    headers:
      - fat_p/BitSet.h
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

#include <iostream>
#include <unordered_set>
#include <vector>

#include "BitSet.h"
#include "FatPTest.h"

namespace fat_p::testing::bitset
{

// ============================================================================
// Basic Operations
// ============================================================================

TEST_CASE(basic_operations)
{
    fat_p::BitSet<64> bits;

    ASSERT_TRUE(bits.none(), "Should start with no bits set");
    ASSERT_TRUE(!bits.any(), "any() should return false for empty set");
    ASSERT_EQ(bits.count(), 0u, "Count should be 0");

    bits.set(5);
    ASSERT_TRUE(bits.test(5), "Bit 5 should be set");
    ASSERT_TRUE(!bits.test(6), "Bit 6 should not be set");
    ASSERT_EQ(bits.count(), 1u, "Count should be 1");

    bits.clear(5);
    ASSERT_TRUE(!bits.test(5), "Bit 5 should be cleared");
    ASSERT_EQ(bits.count(), 0u, "Count should be 0");

    bits.set(10);
    bits.flip(10);
    ASSERT_TRUE(!bits.test(10), "Bit 10 should be flipped to 0");

    bits.flip(20);
    ASSERT_TRUE(bits.test(20), "Bit 20 should be flipped to 1");

    return true;
}

TEST_CASE(set_with_value)
{
    fat_p::BitSet<64> bits;

    bits.set(10, true);
    ASSERT_TRUE(bits.test(10), "Bit 10 should be set");

    bits.set(10, false);
    ASSERT_TRUE(!bits.test(10), "Bit 10 should be cleared");

    bits.set(20, true);
    bits.set(20, true);
    ASSERT_TRUE(bits.test(20), "Bit 20 should remain set");

    return true;
}

TEST_CASE(subscript_operator)
{
    fat_p::BitSet<128> bits;

    bits.set(0);
    bits.set(63);
    bits.set(64);
    bits.set(127);

    ASSERT_TRUE(bits[0], "bits[0] should be true");
    ASSERT_TRUE(bits[63], "bits[63] should be true");
    ASSERT_TRUE(bits[64], "bits[64] should be true");
    ASSERT_TRUE(bits[127], "bits[127] should be true");
    ASSERT_TRUE(!bits[1], "bits[1] should be false");
    ASSERT_TRUE(!bits[100], "bits[100] should be false");

    return true;
}

TEST_CASE(unchecked_operations)
{
    fat_p::BitSet<256> bits;

    bits.set_unchecked(0);
    bits.set_unchecked(100);
    bits.set_unchecked(255);

    ASSERT_TRUE(bits.test_unchecked(0), "Bit 0 should be set");
    ASSERT_TRUE(bits.test_unchecked(100), "Bit 100 should be set");
    ASSERT_TRUE(bits.test_unchecked(255), "Bit 255 should be set");
    ASSERT_TRUE(!bits.test_unchecked(50), "Bit 50 should not be set");

    bits.flip_unchecked(100);
    ASSERT_TRUE(!bits.test_unchecked(100), "Bit 100 should be flipped");

    bits.clear_unchecked(0);
    ASSERT_TRUE(!bits.test_unchecked(0), "Bit 0 should be cleared");

    return true;
}

// ============================================================================
// Boundary Tests
// ============================================================================

TEST_CASE(size_one)
{
    fat_p::BitSet<1> bits;

    ASSERT_TRUE(bits.none(), "Should start empty");
    ASSERT_EQ(bits.size(), 1u, "Size should be 1");

    bits.set(0);
    ASSERT_TRUE(bits.test(0), "Bit 0 should be set");
    ASSERT_TRUE(bits.all(), "all() should return true");
    ASSERT_EQ(bits.count(), 1u, "Count should be 1");

    bits.flip_all();
    ASSERT_TRUE(bits.none(), "Should be empty after flip");

    return true;
}

TEST_CASE(size_63)
{
    fat_p::BitSet<63> bits;

    ASSERT_EQ(bits.size(), 63u, "Size should be 63");

    bits.set_all();
    ASSERT_EQ(bits.count(), 63u, "Count should be 63");
    ASSERT_TRUE(bits.all(), "all() should return true");

    bits.clear(62);
    ASSERT_TRUE(!bits.all(), "all() should return false after clearing one bit");
    ASSERT_EQ(bits.count(), 62u, "Count should be 62");

    return true;
}

TEST_CASE(size_64)
{
    fat_p::BitSet<64> bits;

    ASSERT_EQ(bits.size(), 64u, "Size should be 64");

    bits.set_all();
    ASSERT_EQ(bits.count(), 64u, "Count should be 64");
    ASSERT_TRUE(bits.all(), "all() should return true");

    return true;
}

TEST_CASE(size_65)
{
    fat_p::BitSet<65> bits;

    ASSERT_EQ(bits.size(), 65u, "Size should be 65");

    bits.set_all();
    ASSERT_EQ(bits.count(), 65u, "Count should be 65");
    ASSERT_TRUE(bits.all(), "all() should return true");

    bits.clear(64);
    ASSERT_EQ(bits.count(), 64u, "Count should be 64");
    ASSERT_TRUE(!bits.all(), "all() should return false");

    return true;
}

TEST_CASE(size_large)
{
    fat_p::BitSet<1000> bits;

    ASSERT_EQ(bits.size(), 1000u, "Size should be 1000");

    bits.set(0);
    bits.set(499);
    bits.set(999);
    ASSERT_EQ(bits.count(), 3u, "Count should be 3");

    bits.set_all();
    ASSERT_EQ(bits.count(), 1000u, "Count should be 1000");

    return true;
}

// ============================================================================
// Bulk Operations
// ============================================================================

TEST_CASE(bulk_operations)
{
    fat_p::BitSet<128> bits;

    bits.set_all();
    ASSERT_TRUE(bits.all(), "All bits should be set");
    ASSERT_EQ(bits.count(), 128u, "Should have 128 bits set");

    bits.clear_all();
    ASSERT_TRUE(bits.none(), "No bits should be set");
    ASSERT_EQ(bits.count(), 0u, "Should have 0 bits set");

    bits.set(10);
    bits.set(20);
    bits.set(30);
    ASSERT_EQ(bits.count(), 3u, "Should have 3 bits set");

    bits.flip_all();
    ASSERT_EQ(bits.count(), 125u, "Should have 125 bits set");
    ASSERT_TRUE(!bits.test(10), "Bit 10 should be flipped");
    ASSERT_TRUE(!bits.test(20), "Bit 20 should be flipped");
    ASSERT_TRUE(!bits.test(30), "Bit 30 should be flipped");

    return true;
}

TEST_CASE(reset_alias)
{
    fat_p::BitSet<64> bits;

    bits.set_all();
    bits.reset();
    ASSERT_TRUE(bits.none(), "reset() should clear all bits");

    bits.set(10);
    bits.reset(10);
    ASSERT_TRUE(!bits.test(10), "reset(index) should clear that bit");

    return true;
}

// ============================================================================
// Range Operations
// ============================================================================

TEST_CASE(set_range)
{
    fat_p::BitSet<256> bits;

    bits.set_range(10, 20);
    ASSERT_EQ(bits.count(), 10u, "Should have 10 bits set");
    ASSERT_TRUE(bits.test(10), "Bit 10 should be set");
    ASSERT_TRUE(bits.test(19), "Bit 19 should be set");
    ASSERT_TRUE(!bits.test(9), "Bit 9 should not be set");
    ASSERT_TRUE(!bits.test(20), "Bit 20 should not be set");

    bits.clear_all();
    bits.set_range(60, 70);
    ASSERT_EQ(bits.count(), 10u, "Should have 10 bits spanning word boundary");
    ASSERT_TRUE(bits.test(63), "Bit 63 should be set");
    ASSERT_TRUE(bits.test(64), "Bit 64 should be set");

    bits.clear_all();
    bits.set_range(0, 256);
    ASSERT_EQ(bits.count(), 256u, "Full range should set all bits");

    bits.clear_all();
    bits.set_range(100, 100);
    ASSERT_EQ(bits.count(), 0u, "Empty range should set no bits");

    return true;
}

TEST_CASE(clear_range)
{
    fat_p::BitSet<256> bits;

    bits.set_all();
    bits.clear_range(10, 20);
    ASSERT_EQ(bits.count(), 246u, "Should have 246 bits set");
    ASSERT_TRUE(!bits.test(10), "Bit 10 should be cleared");
    ASSERT_TRUE(!bits.test(19), "Bit 19 should be cleared");
    ASSERT_TRUE(bits.test(9), "Bit 9 should still be set");
    ASSERT_TRUE(bits.test(20), "Bit 20 should still be set");

    bits.set_all();
    bits.clear_range(60, 70);
    ASSERT_EQ(bits.count(), 246u, "Should clear bits spanning word boundary");

    return true;
}

TEST_CASE(count_range)
{
    fat_p::BitSet<256> bits;

    bits.set_range(0, 100);
    ASSERT_EQ(bits.count_range(0, 100), 100u, "Should count 100 bits");
    ASSERT_EQ(bits.count_range(0, 50), 50u, "Should count 50 bits");
    ASSERT_EQ(bits.count_range(50, 100), 50u, "Should count 50 bits");
    ASSERT_EQ(bits.count_range(100, 200), 0u, "Should count 0 bits outside range");

    bits.clear_all();
    bits.set(63);
    bits.set(64);
    ASSERT_EQ(bits.count_range(60, 70), 2u, "Should count bits at word boundary");

    return true;
}

// ============================================================================
// Find Operations
// ============================================================================

TEST_CASE(find_operations)
{
    fat_p::BitSet<256> bits;

    bits.set(10);
    bits.set(50);
    bits.set(200);

    ASSERT_EQ(bits.find_first(), 10u, "First bit should be 10");
    ASSERT_EQ(bits.find_next(10), 50u, "Next after 10 should be 50");
    ASSERT_EQ(bits.find_next(50), 200u, "Next after 50 should be 200");
    ASSERT_EQ(bits.find_next(200), 256u, "No next after 200");
    ASSERT_EQ(bits.find_last(), 200u, "Last bit should be 200");

    bits.clear_all();
    ASSERT_EQ(bits.find_first(), 256u, "find_first on empty should return N");
    ASSERT_EQ(bits.find_last(), 256u, "find_last on empty should return N");

    return true;
}

TEST_CASE(find_at_boundaries)
{
    fat_p::BitSet<128> bits;

    bits.set(0);
    ASSERT_EQ(bits.find_first(), 0u, "Should find bit 0");
    ASSERT_EQ(bits.find_last(), 0u, "Last should also be 0");

    bits.clear_all();
    bits.set(127);
    ASSERT_EQ(bits.find_first(), 127u, "Should find bit 127");
    ASSERT_EQ(bits.find_last(), 127u, "Last should also be 127");

    bits.set(63);
    bits.set(64);
    ASSERT_EQ(bits.find_next(63), 64u, "Should find bit across word boundary");

    return true;
}

// ============================================================================
// Iteration
// ============================================================================

TEST_CASE(iteration)
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

    ASSERT_EQ(indices.size(), 4u, "Should iterate over 4 bits");
    ASSERT_EQ(indices[0], 5u, "First should be 5");
    ASSERT_EQ(indices[1], 15u, "Second should be 15");
    ASSERT_EQ(indices[2], 25u, "Third should be 25");
    ASSERT_EQ(indices[3], 100u, "Fourth should be 100");

    return true;
}

TEST_CASE(iteration_empty)
{
    fat_p::BitSet<64> bits;

    std::vector<size_t> indices;
    for (size_t idx : bits)
    {
        indices.push_back(idx);
    }

    ASSERT_TRUE(indices.empty(), "Empty bitset should yield no iterations");

    return true;
}

TEST_CASE(iterator_traits)
{
    using Iterator = fat_p::BitSet<64>::Iterator;

    static_assert(std::is_same_v<Iterator::iterator_category, std::forward_iterator_tag>,
                  "Should be forward iterator");
    static_assert(std::is_same_v<Iterator::value_type, size_t>,
                  "Value type should be size_t");
    static_assert(std::is_same_v<Iterator::difference_type, std::ptrdiff_t>,
                  "Difference type should be ptrdiff_t");

    return true;
}

// ============================================================================
// Bitwise Operations
// ============================================================================

TEST_CASE(bitwise_and)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);
    a.set(3);

    b.set(2);
    b.set(3);
    b.set(4);

    auto result = a & b;
    ASSERT_EQ(result.count(), 2u, "AND should have 2 bits (2,3)");
    ASSERT_TRUE(result.test(2), "Bit 2 should be set");
    ASSERT_TRUE(result.test(3), "Bit 3 should be set");
    ASSERT_TRUE(!result.test(1), "Bit 1 should not be set");
    ASSERT_TRUE(!result.test(4), "Bit 4 should not be set");

    return true;
}

TEST_CASE(bitwise_or)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);

    b.set(3);
    b.set(4);

    auto result = a | b;
    ASSERT_EQ(result.count(), 4u, "OR should have 4 bits");
    ASSERT_TRUE(result.test(1), "Bit 1 should be set");
    ASSERT_TRUE(result.test(2), "Bit 2 should be set");
    ASSERT_TRUE(result.test(3), "Bit 3 should be set");
    ASSERT_TRUE(result.test(4), "Bit 4 should be set");

    return true;
}

TEST_CASE(bitwise_xor)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);
    a.set(3);

    b.set(2);
    b.set(3);
    b.set(4);

    auto result = a ^ b;
    ASSERT_EQ(result.count(), 2u, "XOR should have 2 bits (1,4)");
    ASSERT_TRUE(result.test(1), "Bit 1 should be set");
    ASSERT_TRUE(result.test(4), "Bit 4 should be set");
    ASSERT_TRUE(!result.test(2), "Bit 2 should not be set");
    ASSERT_TRUE(!result.test(3), "Bit 3 should not be set");

    return true;
}

TEST_CASE(bitwise_not)
{
    fat_p::BitSet<64> bits;

    bits.set(0);
    bits.set(63);

    auto result = ~bits;
    ASSERT_EQ(result.count(), 62u, "NOT should have 62 bits");
    ASSERT_TRUE(!result.test(0), "Bit 0 should not be set");
    ASSERT_TRUE(!result.test(63), "Bit 63 should not be set");
    ASSERT_TRUE(result.test(1), "Bit 1 should be set");
    ASSERT_TRUE(result.test(32), "Bit 32 should be set");

    return true;
}

TEST_CASE(compound_assignment)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);
    b.set(2);
    b.set(3);

    fat_p::BitSet<64> test_and = a;
    test_and &= b;
    ASSERT_EQ(test_and.count(), 1u, "&= should leave bit 2");
    ASSERT_TRUE(test_and.test(2), "Bit 2 should be set");

    fat_p::BitSet<64> test_or = a;
    test_or |= b;
    ASSERT_EQ(test_or.count(), 3u, "|= should have 3 bits");

    fat_p::BitSet<64> test_xor = a;
    test_xor ^= b;
    ASSERT_EQ(test_xor.count(), 2u, "^= should have 2 bits");

    return true;
}

// ============================================================================
// Set Operations
// ============================================================================

TEST_CASE(is_subset_of)
{
    fat_p::BitSet<64> a, b;

    a.set(1);
    a.set(2);

    b.set(1);
    b.set(2);
    b.set(3);

    ASSERT_TRUE(a.is_subset_of(b), "a should be subset of b");
    ASSERT_TRUE(!b.is_subset_of(a), "b should not be subset of a");

    fat_p::BitSet<64> empty;
    ASSERT_TRUE(empty.is_subset_of(a), "Empty set is subset of any set");
    ASSERT_TRUE(a.is_subset_of(a), "Set is subset of itself");

    return true;
}

TEST_CASE(intersects)
{
    fat_p::BitSet<64> a, b, c;

    a.set(1);
    a.set(2);

    b.set(2);
    b.set(3);

    c.set(10);
    c.set(20);

    ASSERT_TRUE(a.intersects(b), "a and b should intersect at bit 2");
    ASSERT_TRUE(!a.intersects(c), "a and c should not intersect");

    fat_p::BitSet<64> empty;
    ASSERT_TRUE(!a.intersects(empty), "No set intersects empty set");

    return true;
}

// ============================================================================
// Comparison Operations
// ============================================================================

TEST_CASE(equality)
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

    ASSERT_TRUE(a == b, "a and b should be equal");
    ASSERT_TRUE(!(a != b), "a and b should not be not-equal");
    ASSERT_TRUE(a != c, "a and c should not be equal");
    ASSERT_TRUE(!(a == c), "a and c should not be equal");

    fat_p::BitSet<64> empty1, empty2;
    ASSERT_TRUE(empty1 == empty2, "Empty sets should be equal");

    return true;
}

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_CASE(initializer_list_constructor)
{
    fat_p::BitSet<100> bits{5, 10, 15, 99};

    ASSERT_EQ(bits.count(), 4u, "Should have 4 bits set");
    ASSERT_TRUE(bits.test(5), "Bit 5 should be set");
    ASSERT_TRUE(bits.test(10), "Bit 10 should be set");
    ASSERT_TRUE(bits.test(15), "Bit 15 should be set");
    ASSERT_TRUE(bits.test(99), "Bit 99 should be set");
    ASSERT_TRUE(!bits.test(0), "Bit 0 should not be set");

    fat_p::BitSet<64> empty{};
    ASSERT_TRUE(empty.none(), "Empty initializer list should yield empty set");

    return true;
}

TEST_CASE(copy_operations)
{
    fat_p::BitSet<128> original;
    original.set(10);
    original.set(100);

    fat_p::BitSet<128> copied = original;
    ASSERT_EQ(copied.count(), 2u, "Copied should have 2 bits");
    ASSERT_TRUE(copied.test(10), "Copied bit 10");
    ASSERT_TRUE(copied.test(100), "Copied bit 100");

    copied.set(50);
    ASSERT_TRUE(!original.test(50), "Original should not be affected");

    fat_p::BitSet<128> assigned;
    assigned = original;
    ASSERT_EQ(assigned.count(), 2u, "Assigned should have 2 bits");

    return true;
}

// ============================================================================
// Exception Tests
// ============================================================================

TEST_CASE(out_of_range_exceptions)
{
    fat_p::BitSet<64> bits;

    ASSERT_THROWS(bits.set(64), std::out_of_range, "set(64) should throw");
    ASSERT_THROWS(bits.clear(100), std::out_of_range, "clear(100) should throw");
    ASSERT_THROWS(bits.flip(64), std::out_of_range, "flip(64) should throw");
    ASSERT_THROWS(bits.test(1000), std::out_of_range, "test(1000) should throw");
    ASSERT_THROWS(bits.reset(64), std::out_of_range, "reset(64) should throw");

    return true;
}

TEST_CASE(range_exceptions)
{
    fat_p::BitSet<64> bits;

    ASSERT_THROWS(bits.set_range(10, 5), std::out_of_range, "Invalid range should throw");
    ASSERT_THROWS(bits.set_range(0, 65), std::out_of_range, "Out of bounds range should throw");
    ASSERT_THROWS(bits.clear_range(100, 200), std::out_of_range, "Out of bounds should throw");
    ASSERT_THROWS(bits.count_range(50, 100), std::out_of_range, "Out of bounds should throw");

    return true;
}

TEST_CASE(range_word_boundaries)
{
    fat_p::BitSet<128> bits;

    bits.set_range(0, 64);
    ASSERT_EQ(bits.count(), 64u, "Should have exactly 64 bits set");
    ASSERT_TRUE(bits.test(0), "Bit 0 should be set");
    ASSERT_TRUE(bits.test(63), "Bit 63 should be set");
    ASSERT_TRUE(!bits.test(64), "Bit 64 should not be set");

    bits.clear_all();
    bits.set_range(64, 128);
    ASSERT_EQ(bits.count(), 64u, "Should have exactly 64 bits set");
    ASSERT_TRUE(!bits.test(63), "Bit 63 should not be set");
    ASSERT_TRUE(bits.test(64), "Bit 64 should be set");
    ASSERT_TRUE(bits.test(127), "Bit 127 should be set");

    bits.clear_all();
    bits.set_range(0, 128);
    ASSERT_EQ(bits.count(), 128u, "Full range should set all bits");
    ASSERT_TRUE(bits.all(), "all() should return true");

    return true;
}

// ============================================================================
// Hash Tests
// ============================================================================

TEST_CASE(std_hash)
{
    fat_p::BitSet<64> a, b, c;

    a.set(1);
    a.set(10);

    b.set(1);
    b.set(10);

    c.set(1);
    c.set(20);

    std::hash<fat_p::BitSet<64>> hasher;

    ASSERT_EQ(hasher(a), hasher(b), "Equal sets should have equal hashes");
    ASSERT_TRUE(hasher(a) != hasher(c), "Different sets should likely have different hashes");

    std::unordered_set<fat_p::BitSet<64>> set;
    set.insert(a);
    set.insert(c);
    ASSERT_EQ(set.size(), 2u, "Should have 2 distinct sets");
    ASSERT_EQ(set.count(b), 1u, "Should find b (equal to a)");

    return true;
}

// ============================================================================
// Data Access Tests
// ============================================================================

TEST_CASE(data_access)
{
    fat_p::BitSet<128> bits;

    bits.set(0);
    bits.set(64);

    const uint64_t* data = bits.data();
    ASSERT_EQ(data[0], 1ULL, "First word should have bit 0 set");
    ASSERT_EQ(data[1], 1ULL, "Second word should have bit 0 set (bit 64 overall)");

    ASSERT_EQ(fat_p::BitSet<128>::word_count(), 2u, "128-bit set should have 2 words");
    ASSERT_EQ(fat_p::BitSet<65>::word_count(), 2u, "65-bit set should have 2 words");
    ASSERT_EQ(fat_p::BitSet<64>::word_count(), 1u, "64-bit set should have 1 word");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_bitset()
{
    std::cout << "\n" << colors::cyan() << "BitSet Benchmarks:" << colors::reset() << "\n\n";

    fat_p::BitSet<1024> bits;
    volatile uint64_t* ptr = bits.data();

    double set_time = measure_perf([&bits, ptr]()
    {
        for (size_t i = 0; i < 1024; ++i)
        {
            bits.set_unchecked(i);
        }
        DoNotOptimize(*ptr);
    }, 10000, 100);
    std::cout << "Set 1024 bits (unchecked): " << format_time(set_time) << "\n";

    double test_time = measure_perf([&bits]()
    {
        bool result = false;
        for (size_t i = 0; i < 1024; ++i)
        {
            result ^= bits.test_unchecked(i);
        }
        DoNotOptimize(result);
    }, 10000, 100);
    std::cout << "Test 1024 bits (unchecked): " << format_time(test_time) << "\n";

    bits.set_all();
    double count_time = measure_perf([&bits]()
    {
        size_t c = bits.count();
        DoNotOptimize(c);
    }, 100000, 1000);
    std::cout << "Count bits (popcnt): " << format_time(count_time) << "\n";

    fat_p::BitSet<1024> other;
    other.set_all();
    fat_p::BitSet<1024> result;
    double and_time = measure_perf([&bits, &other, &result]()
    {
        result = bits & other;
        DoNotOptimize(*result.data());
    }, 100000, 1000);
    std::cout << "Bitwise AND (1024 bits): " << format_time(and_time) << "\n";

    bits.clear_all();
    for (size_t i = 0; i < 1024; i += 10)
    {
        bits.set(i);
    }
    double iterate_time = measure_perf([&bits]()
    {
        size_t sum = 0;
        for (size_t idx : bits)
        {
            sum += idx;
        }
        DoNotOptimize(sum);
    }, 10000, 100);
    std::cout << "Iterate ~100 set bits: " << format_time(iterate_time) << "\n";

    double find_time = measure_perf([&bits]()
    {
        size_t pos = bits.find_first();
        while (pos != 1024)
        {
            DoNotOptimize(pos);
            pos = bits.find_next(pos);
        }
    }, 10000, 100);
    std::cout << "Find traversal ~100 bits: " << format_time(find_time) << "\n";
}

} // namespace fat_p::testing::bitset

namespace fat_p::testing
{

bool test_BitSet()
{
    PRINT_HEADER(BIT SET)

    TestRunner runner;

    RUN_TEST_NS(runner, bitset, basic_operations);
    RUN_TEST_NS(runner, bitset, set_with_value);
    RUN_TEST_NS(runner, bitset, subscript_operator);
    RUN_TEST_NS(runner, bitset, unchecked_operations);

    RUN_TEST_NS(runner, bitset, size_one);
    RUN_TEST_NS(runner, bitset, size_63);
    RUN_TEST_NS(runner, bitset, size_64);
    RUN_TEST_NS(runner, bitset, size_65);
    RUN_TEST_NS(runner, bitset, size_large);

    RUN_TEST_NS(runner, bitset, bulk_operations);
    RUN_TEST_NS(runner, bitset, reset_alias);

    RUN_TEST_NS(runner, bitset, set_range);
    RUN_TEST_NS(runner, bitset, clear_range);
    RUN_TEST_NS(runner, bitset, count_range);

    RUN_TEST_NS(runner, bitset, find_operations);
    RUN_TEST_NS(runner, bitset, find_at_boundaries);

    RUN_TEST_NS(runner, bitset, iteration);
    RUN_TEST_NS(runner, bitset, iteration_empty);
    RUN_TEST_NS(runner, bitset, iterator_traits);

    RUN_TEST_NS(runner, bitset, bitwise_and);
    RUN_TEST_NS(runner, bitset, bitwise_or);
    RUN_TEST_NS(runner, bitset, bitwise_xor);
    RUN_TEST_NS(runner, bitset, bitwise_not);
    RUN_TEST_NS(runner, bitset, compound_assignment);

    RUN_TEST_NS(runner, bitset, is_subset_of);
    RUN_TEST_NS(runner, bitset, intersects);

    RUN_TEST_NS(runner, bitset, equality);

    RUN_TEST_NS(runner, bitset, initializer_list_constructor);
    RUN_TEST_NS(runner, bitset, copy_operations);

    RUN_TEST_NS(runner, bitset, out_of_range_exceptions);
    RUN_TEST_NS(runner, bitset, range_exceptions);
    RUN_TEST_NS(runner, bitset, range_word_boundaries);

    RUN_TEST_NS(runner, bitset, std_hash);
    RUN_TEST_NS(runner, bitset, data_access);

    bitset::benchmark_bitset();

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
