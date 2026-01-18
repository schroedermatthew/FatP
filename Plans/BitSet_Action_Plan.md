# BitSet Enhancement Action Plan

**Component:** BitSet  
**Current Status:** `api_stability: in_work`  
**Target Status:** `api_stability: candidate`  
**Estimated Effort:** 3-4 days  
**Risk Level:** Low (no breaking changes, additive only)

---

## Executive Summary

BitSet is a well-implemented, bug-free component. This plan adds missing features to achieve feature parity with std::bitset while maintaining FAT-P's performance advantages. All changes are additive—no breaking changes to existing API.

**Priority Breakdown:**
- P0 (Blockers): 0 items
- P1 (Required for candidate): 6 items (~2 days)
- P2 (Should have): 5 items (~1 day)
- P3 (Nice to have): 4 items (future)

---

## Phase 1: Critical API Additions (P1)

### 1.1 Bit Shift Operators

**File:** `fat_p/BitSet.h`  
**Effort:** 4 hours  
**Why:** Essential for sliding window algorithms, bloom filters, bit manipulation

#### Implementation

```cpp
// Add after line 783 (after operator^=)

// ========================================================================
// Shift operators
// ========================================================================

/**
 * @brief Left-shift all bits by n positions
 * @param n Number of positions to shift (bits shifted out are lost)
 * @return New BitSet with shifted bits
 * 
 * Example: BitSet<8> with bits 0,1,2 set, shifted left by 2:
 *   Before: 00000111
 *   After:  00011100
 */
[[nodiscard]] BitSet operator<<(size_t n) const noexcept
{
    if (n == 0)
    {
        return *this;
    }
    if (n >= N)
    {
        return BitSet{};
    }

    BitSet result;
    const size_t word_shift = n / BITS_PER_WORD;
    const size_t bit_shift = n % BITS_PER_WORD;

    if (bit_shift == 0)
    {
        // Word-aligned shift
        for (size_t i = word_shift; i < NUM_WORDS; ++i)
        {
            result.m_words[i] = m_words[i - word_shift];
        }
    }
    else
    {
        // Cross-word shift
        const size_t inv_shift = BITS_PER_WORD - bit_shift;
        for (size_t i = NUM_WORDS - 1; i > word_shift; --i)
        {
            result.m_words[i] = (m_words[i - word_shift] << bit_shift) |
                                (m_words[i - word_shift - 1] >> inv_shift);
        }
        result.m_words[word_shift] = m_words[0] << bit_shift;
    }

    // Mask out bits beyond N
    if constexpr (LAST_WORD_BITS != 0)
    {
        result.m_words[NUM_WORDS - 1] &= LAST_WORD_MASK;
    }

    return result;
}

/**
 * @brief Right-shift all bits by n positions
 * @param n Number of positions to shift (bits shifted out are lost)
 * @return New BitSet with shifted bits
 */
[[nodiscard]] BitSet operator>>(size_t n) const noexcept
{
    if (n == 0)
    {
        return *this;
    }
    if (n >= N)
    {
        return BitSet{};
    }

    BitSet result;
    const size_t word_shift = n / BITS_PER_WORD;
    const size_t bit_shift = n % BITS_PER_WORD;

    if (bit_shift == 0)
    {
        // Word-aligned shift
        for (size_t i = 0; i < NUM_WORDS - word_shift; ++i)
        {
            result.m_words[i] = m_words[i + word_shift];
        }
    }
    else
    {
        // Cross-word shift
        const size_t inv_shift = BITS_PER_WORD - bit_shift;
        for (size_t i = 0; i < NUM_WORDS - word_shift - 1; ++i)
        {
            result.m_words[i] = (m_words[i + word_shift] >> bit_shift) |
                                (m_words[i + word_shift + 1] << inv_shift);
        }
        result.m_words[NUM_WORDS - word_shift - 1] = m_words[NUM_WORDS - 1] >> bit_shift;
    }

    return result;
}

BitSet& operator<<=(size_t n) noexcept
{
    *this = *this << n;
    return *this;
}

BitSet& operator>>=(size_t n) noexcept
{
    *this = *this >> n;
    return *this;
}
```

#### Test Cases

```cpp
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
    
    // Word-aligned shift
    auto word_shift = bits << 64;
    FATP_ASSERT_TRUE(word_shift.test(64), "Bit 64 should be set (was 0)");
    FATP_ASSERT_TRUE(word_shift.test(65), "Bit 65 should be set (was 1)");
    FATP_ASSERT_TRUE(word_shift.test(127), "Bit 127 should be set (was 63)");
    
    // Shift beyond size
    auto empty = bits << 128;
    FATP_ASSERT_TRUE(empty.none(), "Shift by N should empty the set");
    
    // Shift by zero
    auto same = bits << 0;
    FATP_ASSERT_EQ(same, bits, "Shift by 0 should be identity");
    
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
```

---

### 1.2 `flip_range()` for API Consistency

**File:** `fat_p/BitSet.h`  
**Effort:** 1 hour  
**Why:** API consistency with `set_range()` and `clear_range()`

#### Implementation

```cpp
// Add after clear_range() (around line 497)

/**
 * @brief Flip all bits in range [start, end)
 * @param start First bit index (inclusive)
 * @param end Last bit index (exclusive)
 * @throws std::out_of_range if start > end or end > N
 */
void flip_range(size_t start, size_t end)
{
    if (start > end || end > N)
    {
        throw std::out_of_range("BitSet::flip_range: invalid range");
    }
    if (start == end)
    {
        return;
    }

    size_t start_word = start / BITS_PER_WORD;
    size_t end_word = (end - 1) / BITS_PER_WORD;
    size_t start_bit = start % BITS_PER_WORD;
    size_t end_bit = (end - 1) % BITS_PER_WORD;

    if (start_word == end_word)
    {
        uint64_t mask = detail::mask_up_to(end_bit) & detail::mask_from(start_bit);
        m_words[start_word] ^= mask;
    }
    else
    {
        m_words[start_word] ^= detail::mask_from(start_bit);
        for (size_t i = start_word + 1; i < end_word; ++i)
        {
            m_words[i] = ~m_words[i];
        }
        m_words[end_word] ^= detail::mask_up_to(end_bit);
    }

    // Maintain last word invariant
    if constexpr (LAST_WORD_BITS != 0)
    {
        if (end_word == NUM_WORDS - 1)
        {
            m_words[NUM_WORDS - 1] &= LAST_WORD_MASK;
        }
    }
}
```

#### Test Cases

```cpp
FATP_TEST_CASE(flip_range)
{
    fat_p::BitSet<256> bits;
    
    bits.flip_range(10, 20);
    FATP_ASSERT_EQ(bits.count(), 10u, "Should have 10 bits set");
    FATP_ASSERT_TRUE(bits.test(10), "Bit 10 should be set");
    FATP_ASSERT_TRUE(bits.test(19), "Bit 19 should be set");
    
    bits.flip_range(10, 20);
    FATP_ASSERT_TRUE(bits.none(), "Double flip should clear all");
    
    // Word boundary
    bits.flip_range(60, 70);
    FATP_ASSERT_EQ(bits.count(), 10u, "Should span word boundary");
    FATP_ASSERT_TRUE(bits.test(63), "Bit 63 set");
    FATP_ASSERT_TRUE(bits.test(64), "Bit 64 set");
    
    // Exception test
    FATP_ASSERT_THROWS(bits.flip_range(20, 10), std::out_of_range, "Invalid range");
    FATP_ASSERT_THROWS(bits.flip_range(0, 257), std::out_of_range, "Out of bounds");
    
    return true;
}
```

---

### 1.3 Zero-Finding Operations

**File:** `fat_p/BitSet.h`  
**Effort:** 2 hours  
**Why:** Essential for memory allocators, sparse matrix operations

#### Implementation

```cpp
// Add after find_last() (around line 673)

/**
 * @brief Find first cleared bit (first 0)
 * @return Index of first cleared bit, or N if all bits are set
 */
[[nodiscard]] size_t find_first_zero() const noexcept
{
    for (size_t i = 0; i < NUM_WORDS - 1; ++i)
    {
        if (m_words[i] != ~0ULL)
        {
            return i * BITS_PER_WORD + FATP_CTZ64(~m_words[i]);
        }
    }
    
    // Last word: check against mask
    uint64_t last_inverted = ~m_words[NUM_WORDS - 1] & LAST_WORD_MASK;
    if (last_inverted != 0)
    {
        size_t idx = (NUM_WORDS - 1) * BITS_PER_WORD + FATP_CTZ64(last_inverted);
        return (idx < N) ? idx : N;
    }
    
    return N;
}

/**
 * @brief Find next cleared bit after given index
 * @param after Search starts after this index
 * @return Index of next cleared bit, or N if none
 */
[[nodiscard]] size_t find_next_zero(size_t after) const noexcept
{
    ++after;
    if (after >= N)
    {
        return N;
    }

    size_t word_idx = after / BITS_PER_WORD;
    size_t bit_offset = after % BITS_PER_WORD;

    // Check current word (masked to only look at bits >= bit_offset)
    uint64_t word = ~m_words[word_idx] & detail::mask_from(bit_offset);
    
    // For last word, also apply the last word mask
    if (word_idx == NUM_WORDS - 1)
    {
        word &= LAST_WORD_MASK;
    }
    
    if (word != 0)
    {
        size_t idx = word_idx * BITS_PER_WORD + FATP_CTZ64(word);
        return (idx < N) ? idx : N;
    }

    // Check remaining words
    for (size_t i = word_idx + 1; i < NUM_WORDS - 1; ++i)
    {
        if (m_words[i] != ~0ULL)
        {
            return i * BITS_PER_WORD + FATP_CTZ64(~m_words[i]);
        }
    }
    
    // Check last word
    if (word_idx < NUM_WORDS - 1)
    {
        uint64_t last_inverted = ~m_words[NUM_WORDS - 1] & LAST_WORD_MASK;
        if (last_inverted != 0)
        {
            size_t idx = (NUM_WORDS - 1) * BITS_PER_WORD + FATP_CTZ64(last_inverted);
            return (idx < N) ? idx : N;
        }
    }

    return N;
}

/**
 * @brief Find last cleared bit (last 0)
 * @return Index of last cleared bit, or N if all bits are set
 */
[[nodiscard]] size_t find_last_zero() const noexcept
{
    // Check last word first (with mask)
    uint64_t last_inverted = ~m_words[NUM_WORDS - 1] & LAST_WORD_MASK;
    if (last_inverted != 0)
    {
        size_t bit_pos = 63 - FATP_CLZ64(last_inverted);
        size_t idx = (NUM_WORDS - 1) * BITS_PER_WORD + bit_pos;
        return (idx < N) ? idx : N;
    }
    
    // Check remaining words
    for (size_t i = NUM_WORDS - 1; i-- > 0;)
    {
        if (m_words[i] != ~0ULL)
        {
            size_t bit_pos = 63 - FATP_CLZ64(~m_words[i]);
            return i * BITS_PER_WORD + bit_pos;
        }
    }
    
    return N;
}
```

#### Test Cases

```cpp
FATP_TEST_CASE(find_zero_operations)
{
    fat_p::BitSet<256> bits;
    bits.set_all();
    
    // All set - no zeros
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
    
    // Empty set - all zeros
    bits.clear_all();
    FATP_ASSERT_EQ(bits.find_first_zero(), 0u, "First zero at 0");
    FATP_ASSERT_EQ(bits.find_last_zero(), 255u, "Last zero at 255");
    
    return true;
}

FATP_TEST_CASE(find_zero_boundaries)
{
    fat_p::BitSet<65> bits;
    bits.set_all();
    
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
```

---

### 1.4 Missing Test Cases (Critical)

**File:** `tests/test_BitSet.cpp`  
**Effort:** 2 hours  
**Why:** Required for candidate status

#### Add These Tests

```cpp
// ============================================================================
// Move Semantics (NEW)
// ============================================================================

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
    FATP_ASSERT_TRUE(!target.test(50), "Old bit 50 gone");
    
    return true;
}

// ============================================================================
// Self-Assignment (NEW)
// ============================================================================

FATP_TEST_CASE(self_assignment)
{
    fat_p::BitSet<64> bits;
    bits.set(10);
    bits.set(20);
    bits.set(30);
    
    bits = bits;  // Self-assignment
    
    FATP_ASSERT_EQ(bits.count(), 3u, "Self-assignment preserves count");
    FATP_ASSERT_TRUE(bits.test(10), "Bit 10 preserved");
    FATP_ASSERT_TRUE(bits.test(20), "Bit 20 preserved");
    FATP_ASSERT_TRUE(bits.test(30), "Bit 30 preserved");
    
    return true;
}

// ============================================================================
// Initializer List Edge Cases (NEW)
// ============================================================================

FATP_TEST_CASE(initializer_list_out_of_range)
{
    // This should throw because 64 is out of range for BitSet<64>
    FATP_ASSERT_THROWS(
        fat_p::BitSet<64> bits({10, 64}),
        std::out_of_range,
        "Out-of-range index in initializer list should throw"
    );
    
    // This should throw because 100 is out of range
    FATP_ASSERT_THROWS(
        fat_p::BitSet<64> bits({0, 100}),
        std::out_of_range,
        "Large out-of-range index should throw"
    );
    
    return true;
}

// ============================================================================
// find_next Edge Cases (NEW)
// ============================================================================

FATP_TEST_CASE(find_next_last_index)
{
    fat_p::BitSet<64> bits;
    bits.set(63);  // Last valid index
    
    // find_next from 62 should find 63
    FATP_ASSERT_EQ(bits.find_next(62), 63u, "Should find bit 63");
    
    // find_next from 63 should return N (nothing after last bit)
    FATP_ASSERT_EQ(bits.find_next(63), 64u, "Nothing after last index");
    
    // find_next from N-1 on empty should return N
    bits.clear_all();
    FATP_ASSERT_EQ(bits.find_next(62), 64u, "Empty set returns N");
    FATP_ASSERT_EQ(bits.find_next(63), 64u, "find_next(N-1) returns N");
    
    return true;
}

FATP_TEST_CASE(find_next_at_word_boundary)
{
    fat_p::BitSet<128> bits;
    bits.set(64);  // First bit of second word
    
    FATP_ASSERT_EQ(bits.find_next(63), 64u, "Should cross word boundary");
    FATP_ASSERT_EQ(bits.find_next(64), 128u, "Nothing after 64");
    
    bits.set(127);
    FATP_ASSERT_EQ(bits.find_next(64), 127u, "Should find 127");
    FATP_ASSERT_EQ(bits.find_next(127), 128u, "Nothing after last");
    
    return true;
}
```

#### Update Test Runner

```cpp
// Add to test_BitSet() function:
FATP_RUN_TEST_NS(runner, bitset, move_construction);
FATP_RUN_TEST_NS(runner, bitset, move_assignment);
FATP_RUN_TEST_NS(runner, bitset, self_assignment);
FATP_RUN_TEST_NS(runner, bitset, initializer_list_out_of_range);
FATP_RUN_TEST_NS(runner, bitset, find_next_last_index);
FATP_RUN_TEST_NS(runner, bitset, find_next_at_word_boundary);

// Also add tests for new features:
FATP_RUN_TEST_NS(runner, bitset, left_shift);
FATP_RUN_TEST_NS(runner, bitset, right_shift);
FATP_RUN_TEST_NS(runner, bitset, shift_assignment);
FATP_RUN_TEST_NS(runner, bitset, flip_range);
FATP_RUN_TEST_NS(runner, bitset, find_zero_operations);
FATP_RUN_TEST_NS(runner, bitset, find_zero_boundaries);
```

---

## Phase 2: std::bitset Compatibility (P2)

### 2.1 Conversion Methods

**File:** `fat_p/BitSet.h`  
**Effort:** 2 hours

```cpp
// Add in Query operations section

/**
 * @brief Convert to string representation
 * @param zero Character for 0 bits (default '0')
 * @param one Character for 1 bits (default '1')
 * @return String of N characters, MSB first (like std::bitset)
 */
[[nodiscard]] std::string to_string(char zero = '0', char one = '1') const
{
    std::string result(N, zero);
    for (size_t i = 0; i < N; ++i)
    {
        if (test_unchecked(i))
        {
            result[N - 1 - i] = one;  // MSB first
        }
    }
    return result;
}

/**
 * @brief Convert to unsigned long
 * @return Value as unsigned long
 * @throws std::overflow_error if N > bits in unsigned long or value overflows
 */
[[nodiscard]] unsigned long to_ulong() const
{
    constexpr size_t ulong_bits = sizeof(unsigned long) * 8;
    
    if constexpr (N > ulong_bits)
    {
        // Check if any bits beyond ulong_bits are set
        for (size_t i = ulong_bits; i < N; ++i)
        {
            if (test_unchecked(i))
            {
                throw std::overflow_error("BitSet::to_ulong: value overflows unsigned long");
            }
        }
    }
    
    return static_cast<unsigned long>(m_words[0]);
}

/**
 * @brief Convert to unsigned long long
 * @return Value as unsigned long long
 * @throws std::overflow_error if value overflows
 */
[[nodiscard]] unsigned long long to_ullong() const
{
    constexpr size_t ullong_bits = sizeof(unsigned long long) * 8;
    
    if constexpr (N > ullong_bits)
    {
        for (size_t i = ullong_bits; i < N; ++i)
        {
            if (test_unchecked(i))
            {
                throw std::overflow_error("BitSet::to_ullong: value overflows unsigned long long");
            }
        }
    }
    
    return static_cast<unsigned long long>(m_words[0]);
}
```

**Required Include:** Add `#include <string>` at top of file.

### 2.2 `find_prev()` for Backward Iteration

**Effort:** 1 hour

```cpp
/**
 * @brief Find previous set bit before given index
 * @param before Search ends before this index (exclusive)
 * @return Index of previous set bit, or N if none
 */
[[nodiscard]] size_t find_prev(size_t before) const noexcept
{
    if (before == 0 || before > N)
    {
        return N;
    }
    
    --before;  // Convert to inclusive
    
    size_t word_idx = before / BITS_PER_WORD;
    size_t bit_offset = before % BITS_PER_WORD;
    
    // Check current word (masked to only look at bits <= bit_offset)
    uint64_t word = m_words[word_idx] & detail::mask_up_to(bit_offset);
    if (word != 0)
    {
        return word_idx * BITS_PER_WORD + (63 - FATP_CLZ64(word));
    }
    
    // Check previous words
    for (size_t i = word_idx; i-- > 0;)
    {
        if (m_words[i] != 0)
        {
            return i * BITS_PER_WORD + (63 - FATP_CLZ64(m_words[i]));
        }
    }
    
    return N;
}
```

### 2.3 Utility Methods

**Effort:** 1 hour

```cpp
/**
 * @brief Count differing bits between two sets (Hamming distance)
 */
[[nodiscard]] size_t hamming_distance(const BitSet& other) const noexcept
{
    size_t dist = 0;
    for (size_t i = 0; i < NUM_WORDS; ++i)
    {
        dist += FATP_POPCNT64(m_words[i] ^ other.m_words[i]);
    }
    return dist;
}

/**
 * @brief Check if this set has no bits in common with another
 */
[[nodiscard]] bool is_disjoint(const BitSet& other) const noexcept
{
    return !intersects(other);
}

/**
 * @brief Check if this is a proper subset (subset but not equal)
 */
[[nodiscard]] bool is_proper_subset_of(const BitSet& other) const noexcept
{
    return is_subset_of(other) && (*this != other);
}
```

### 2.4 Make Fallback Functions `constexpr`

**File:** `fat_p/BitSet.h`, lines 78-117  
**Effort:** 15 minutes

```cpp
// Change 'inline' to 'inline constexpr' for all three fallback functions:

inline constexpr size_t popcnt64_fallback(uint64_t x) noexcept
{
    size_t count = 0;
    while (x)
    {
        x &= x - 1;
        ++count;
    }
    return count;
}

inline constexpr size_t ctz64_fallback(uint64_t x) noexcept
{
    if (x == 0)
    {
        return 64;
    }
    size_t count = 0;
    while ((x & 1) == 0)
    {
        ++count;
        x >>= 1;
    }
    return count;
}

inline constexpr size_t clz64_fallback(uint64_t x) noexcept
{
    if (x == 0)
    {
        return 64;
    }
    size_t count = 0;
    while ((x & (1ULL << 63)) == 0)
    {
        ++count;
        x <<= 1;
    }
    return count;
}
```

---

## Phase 3: Performance Enhancements (P3 - Future)

### 3.1 SIMD Optimization for Large BitSets

**When:** After Phase 1 & 2 complete, if benchmarks show need  
**Effort:** 4-6 hours

**Target Operations:**
- `operator&`, `operator|`, `operator^` - 4x speedup for N >= 256
- `set_all()`, `clear_all()`, `flip_all()` - 4x speedup
- `count()` - Minor improvement (POPCNT already fast)

**Implementation Strategy:**

```cpp
#ifdef __AVX2__
// For BitSet<N> where N >= 256 (4+ words)
BitSet operator&(const BitSet& other) const noexcept
{
    BitSet result;
    size_t i = 0;
    
    // AVX2: Process 4 words (256 bits) at a time
    for (; i + 4 <= NUM_WORDS; i += 4)
    {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&m_words[i]));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&other.m_words[i]));
        __m256i r = _mm256_and_si256(a, b);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result.m_words[i]), r);
    }
    
    // Scalar remainder
    for (; i < NUM_WORDS; ++i)
    {
        result.m_words[i] = m_words[i] & other.m_words[i];
    }
    
    return result;
}
#endif
```

**Decision Criteria:**
- Only add if benchmarks show >20% improvement for target sizes
- Must maintain scalar fallback for portability
- Consider compile-time size threshold (e.g., only for N >= 512)

### 3.2 C++20 `constexpr` Support

**When:** C++20 becomes minimum requirement  
**Effort:** 2 hours

Make these operations `constexpr`:
- All single-bit operations
- Bulk operations (`set_all`, `clear_all`, `flip_all`)
- Bitwise operators
- Comparison operators

---

## Implementation Checklist

### Phase 1 (Required for Candidate)

| Task | File | Lines | Est. Time | Status |
|------|------|-------|-----------|--------|
| Add `operator<<` / `operator>>` | BitSet.h | ~80 | 3h | ☐ |
| Add `operator<<=` / `operator>>=` | BitSet.h | ~10 | 30m | ☐ |
| Add `flip_range()` | BitSet.h | ~30 | 1h | ☐ |
| Add `find_first_zero()` | BitSet.h | ~20 | 45m | ☐ |
| Add `find_next_zero()` | BitSet.h | ~35 | 45m | ☐ |
| Add `find_last_zero()` | BitSet.h | ~20 | 30m | ☐ |
| Add move semantics tests | test_BitSet.cpp | ~25 | 30m | ☐ |
| Add self-assignment test | test_BitSet.cpp | ~15 | 15m | ☐ |
| Add initializer_list exception test | test_BitSet.cpp | ~15 | 15m | ☐ |
| Add find_next edge case tests | test_BitSet.cpp | ~30 | 30m | ☐ |
| Add shift operator tests | test_BitSet.cpp | ~60 | 45m | ☐ |
| Add flip_range tests | test_BitSet.cpp | ~25 | 20m | ☐ |
| Add find_zero tests | test_BitSet.cpp | ~40 | 30m | ☐ |
| Update test runner | test_BitSet.cpp | ~15 | 10m | ☐ |

**Phase 1 Total:** ~10 hours

### Phase 2 (Should Have)

| Task | File | Lines | Est. Time | Status |
|------|------|-------|-----------|--------|
| Add `to_string()` | BitSet.h | ~15 | 30m | ☐ |
| Add `to_ulong()` / `to_ullong()` | BitSet.h | ~30 | 45m | ☐ |
| Add `find_prev()` | BitSet.h | ~25 | 45m | ☐ |
| Add `hamming_distance()` | BitSet.h | ~10 | 15m | ☐ |
| Add `is_disjoint()` | BitSet.h | ~5 | 10m | ☐ |
| Add `is_proper_subset_of()` | BitSet.h | ~5 | 10m | ☐ |
| Make fallbacks `constexpr` | BitSet.h | ~3 | 10m | ☐ |
| Add `#include <string>` | BitSet.h | 1 | 1m | ☐ |
| Add tests for all P2 features | test_BitSet.cpp | ~100 | 1.5h | ☐ |

**Phase 2 Total:** ~5 hours

### Documentation Updates

| Task | File | Est. Time | Status |
|------|------|-----------|--------|
| Update BitSet_User_Manual.md | Documentation | 2h | ☐ |
| Add shift operators section | Documentation | 30m | ☐ |
| Add zero-finding section | Documentation | 30m | ☐ |
| Add conversion methods section | Documentation | 30m | ☐ |
| Update API reference table | Documentation | 30m | ☐ |

---

## Verification Plan

### Unit Tests

```bash
# Build and run tests
cd /home/claude/FAT-P
g++ -std=c++17 -O2 -mpopcnt -DENABLE_TEST_APPLICATION \
    -I fat_p tests/test_BitSet.cpp -o test_bitset
./test_bitset
```

### Expected Test Count After Phase 1

| Category | Before | After |
|----------|--------|-------|
| Total Test Cases | 34 | 46 |
| Total Assertions | 173 | ~230 |

### Benchmark Verification

Add to benchmark section:

```cpp
// Shift operations benchmark
bits.set_all();
double shift_time = measure_perf(
    [&bits]() {
        auto r = bits << 7;
        DoNotOptimize(*r.data());
    },
    100000,
    1000);
std::cout << "Left shift by 7: " << format_time(shift_time) << "\n";

// Zero-finding benchmark
bits.set_all();
for (size_t i = 0; i < 1024; i += 10) bits.clear(i);
double find_zero_time = measure_perf(
    [&bits]() {
        size_t pos = bits.find_first_zero();
        while (pos != 1024) {
            DoNotOptimize(pos);
            pos = bits.find_next_zero(pos);
        }
    },
    10000,
    100);
std::cout << "Find zero traversal: " << format_time(find_zero_time) << "\n";
```

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Shift operator edge cases | Medium | Medium | Extensive boundary tests |
| find_zero off-by-one errors | Medium | High | Test all word boundaries |
| LAST_WORD_MASK bugs in new code | Low | High | Verify with non-64-multiple sizes |
| Performance regression | Low | Medium | Benchmark before/after |

---

## Success Criteria

### Phase 1 Complete When:
- [ ] All 46+ tests pass
- [ ] Shift operators work for all sizes (1, 63, 64, 65, 128, 1000)
- [ ] Zero-finding works correctly at word boundaries
- [ ] No performance regression in existing operations
- [ ] Code compiles with `-Wall -Wextra -Werror`

### Ready for Candidate Status When:
- [ ] Phase 1 complete
- [ ] Phase 2 complete (optional but recommended)
- [ ] Documentation updated
- [ ] Benchmark results documented
- [ ] Code review passed
