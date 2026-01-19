#pragma once

/*
FATP_META:
  meta_version: 1
  component: BitSet
  file_role: public_header
  path: fat_p/BitSet.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for BitSet."
  api_stability: in_work
  related:
    docs_search: "BitSet"
    tests:
      - tests/test_BitSet.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file BitSet.h
 * @brief High-performance fixed-size bit set with compiler intrinsics
 *
 * Features:
 * - Fixed capacity bit set with O(1) single-bit operations
 * - Bulk operations (set_all, clear_all, flip_all)
 * - Population count using hardware popcnt instruction
 * - Find first/last/next set bit using hardware ctz/clz
 * - Bitwise operations (AND, OR, XOR, NOT)
 * - Range operations (set_range, clear_range, count_range)
 * - Set operations (is_subset_of, intersects)
 * - STL-compatible iterator over set bits
 * - Unchecked accessors for HPC inner loops
 *
 * Compilation: Requires C++17
 * - g++ -std=c++17 -O3 -mpopcnt your_code.cpp
 * - cl /std:c++17 /O2 your_code.cpp
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace fat_p
{

namespace detail
{

// Brian Kernighan's algorithm - O(k) where k = number of set bits
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

inline size_t popcnt64(uint64_t x) noexcept
{
#if defined(_MSC_VER)
#if defined(_M_X64) || defined(_M_AMD64)
    return static_cast<size_t>(__popcnt64(x));
#else
    const unsigned int lo = static_cast<unsigned int>(x);
    const unsigned int hi = static_cast<unsigned int>(x >> 32);
    return static_cast<size_t>(__popcnt(lo) + __popcnt(hi));
#endif
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<size_t>(__builtin_popcountll(x));
#else
    return popcnt64_fallback(x);
#endif
}

inline size_t ctz64(uint64_t x) noexcept
{
    if (x == 0)
    {
        return 64;
    }

#if defined(_MSC_VER)
    unsigned long index = 0;
#if defined(_M_X64) || defined(_M_AMD64)
    _BitScanForward64(&index, x);
    return static_cast<size_t>(index);
#else
    const unsigned int lo = static_cast<unsigned int>(x);
    if (_BitScanForward(&index, lo) != 0)
    {
        return static_cast<size_t>(index);
    }
    const unsigned int hi = static_cast<unsigned int>(x >> 32);
    _BitScanForward(&index, hi);
    return static_cast<size_t>(index) + 32u;
#endif
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<size_t>(__builtin_ctzll(x));
#else
    return ctz64_fallback(x);
#endif
}

inline size_t clz64(uint64_t x) noexcept
{
    if (x == 0)
    {
        return 64;
    }

#if defined(_MSC_VER)
    unsigned long index = 0;
#if defined(_M_X64) || defined(_M_AMD64)
    _BitScanReverse64(&index, x);
    return 63u - static_cast<size_t>(index);
#else
    const unsigned int hi = static_cast<unsigned int>(x >> 32);
    if (hi != 0)
    {
        _BitScanReverse(&index, hi);
        return 31u - static_cast<size_t>(index);
    }
    const unsigned int lo = static_cast<unsigned int>(x);
    _BitScanReverse(&index, lo);
    return 63u - static_cast<size_t>(index);
#endif
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<size_t>(__builtin_clzll(x));
#else
    return clz64_fallback(x);
#endif
}

// Create a mask with bits [0, bit_index] set (inclusive)
// Avoids reliance on unsigned overflow behavior
inline constexpr uint64_t mask_up_to(size_t bit_index) noexcept
{
    return (bit_index >= 63) ? ~0ULL : ((1ULL << (bit_index + 1)) - 1);
}

// Create a mask with bits [bit_index, 63] set (inclusive)
inline constexpr uint64_t mask_from(size_t bit_index) noexcept
{
    return ~0ULL << bit_index;
}

} // namespace detail

// ============================================================================
// BitSet
// ============================================================================

/**
 * @brief Fixed-size bit set with fast operations
 *
 * @tparam N Number of bits (will be rounded up to multiple of 64 internally)
 *
 * Thread-safety: None (each thread needs own instance or external sync)
 * Exception-safety: Strong guarantee for checked operations
 */
template <size_t N>
class BitSet
{
    static_assert(N > 0, "BitSet size must be greater than 0");

    static constexpr size_t BITS_PER_WORD = 64;
    static constexpr size_t NUM_WORDS = (N + BITS_PER_WORD - 1) / BITS_PER_WORD;
    static constexpr size_t CAPACITY = NUM_WORDS * BITS_PER_WORD;
    static constexpr size_t LAST_WORD_BITS = N % BITS_PER_WORD;
    static constexpr uint64_t LAST_WORD_MASK = (LAST_WORD_BITS == 0) ? ~0ULL : ((1ULL << LAST_WORD_BITS) - 1);

public:
    /**
     * @brief Iterator over set bits (indices where bit == 1)
     */
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const size_t*;
        using reference = size_t;

        Iterator(const BitSet* bitset, size_t pos) noexcept
            : m_bitset(bitset)
            , m_pos(pos)
        {
        }

        size_t operator*() const noexcept
        {
            return m_pos;
        }

        Iterator& operator++() noexcept
        {
            m_pos = m_bitset->find_next(m_pos);
            return *this;
        }

        Iterator operator++(int) noexcept
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const noexcept
        {
            return (m_bitset == other.m_bitset) && (m_pos == other.m_pos);
        }

        bool operator!=(const Iterator& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        const BitSet* m_bitset;
        size_t m_pos;
    };

    using iterator = Iterator;
    using const_iterator = Iterator;

    /**
     * @brief Construct bit set with all bits cleared
     */
    constexpr BitSet() noexcept
        : m_words{}
    {
    }

    /**
     * @brief Construct from initializer list of bit indices
     * @param indices Indices of bits to set
     * @throws std::out_of_range if any index >= N
     */
    BitSet(std::initializer_list<size_t> indices)
        : m_words{}
    {
        for (size_t idx : indices)
        {
            set(idx);
        }
    }

    // ========================================================================
    // Single-bit operations (checked)
    // ========================================================================

    /**
     * @brief Set a bit to 1
     * @param index Bit index (0-based)
     * @throws std::out_of_range if index >= N
     */
    void set(size_t index)
    {
        if (index >= N)
        {
            throw std::out_of_range("BitSet::set: index out of range");
        }
        set_unchecked(index);
    }

    /**
     * @brief Set a bit to specified value
     * @param index Bit index
     * @param value Value to set (true=1, false=0)
     * @throws std::out_of_range if index >= N
     */
    void set(size_t index, bool value)
    {
        if (value)
        {
            set(index);
        }
        else
        {
            clear(index);
        }
    }

    /**
     * @brief Clear a bit to 0
     * @param index Bit index
     * @throws std::out_of_range if index >= N
     */
    void clear(size_t index)
    {
        if (index >= N)
        {
            throw std::out_of_range("BitSet::clear: index out of range");
        }
        clear_unchecked(index);
    }

    /**
     * @brief Flip a bit (toggle 0<->1)
     * @param index Bit index
     * @throws std::out_of_range if index >= N
     */
    void flip(size_t index)
    {
        if (index >= N)
        {
            throw std::out_of_range("BitSet::flip: index out of range");
        }
        flip_unchecked(index);
    }

    /**
     * @brief Test if bit is set (checked)
     * @param index Bit index
     * @return true if bit is 1, false if 0
     * @throws std::out_of_range if index >= N
     */
    [[nodiscard]] bool test(size_t index) const
    {
        if (index >= N)
        {
            throw std::out_of_range("BitSet::test: index out of range");
        }
        return test_unchecked(index);
    }

    // ========================================================================
    // Single-bit operations (unchecked - for HPC inner loops)
    // ========================================================================

    /**
     * @brief Set a bit to 1 without bounds checking
     * @param index Bit index (caller must ensure index < N)
     */
    void set_unchecked(size_t index) noexcept
    {
        m_words[index / BITS_PER_WORD] |= (1ULL << (index % BITS_PER_WORD));
    }

    /**
     * @brief Clear a bit to 0 without bounds checking
     * @param index Bit index (caller must ensure index < N)
     */
    void clear_unchecked(size_t index) noexcept
    {
        m_words[index / BITS_PER_WORD] &= ~(1ULL << (index % BITS_PER_WORD));
    }

    /**
     * @brief Flip a bit without bounds checking
     * @param index Bit index (caller must ensure index < N)
     */
    void flip_unchecked(size_t index) noexcept
    {
        m_words[index / BITS_PER_WORD] ^= (1ULL << (index % BITS_PER_WORD));
    }

    /**
     * @brief Test if bit is set without bounds checking
     * @param index Bit index (caller must ensure index < N)
     * @return true if bit is 1
     */
    [[nodiscard]] bool test_unchecked(size_t index) const noexcept
    {
        return (m_words[index / BITS_PER_WORD] & (1ULL << (index % BITS_PER_WORD))) != 0;
    }

    /**
     * @brief Array subscript operator (unchecked for performance)
     * @param index Bit index
     * @return true if bit is 1
     */
    [[nodiscard]] bool operator[](size_t index) const noexcept
    {
        return test_unchecked(index);
    }

    // ========================================================================
    // Bulk operations
    // ========================================================================

    /**
     * @brief Set all bits to 1
     */
    void set_all() noexcept
    {
        for (size_t i = 0; i < NUM_WORDS - 1; ++i)
        {
            m_words[i] = ~0ULL;
        }
        m_words[NUM_WORDS - 1] = LAST_WORD_MASK;
    }

    /**
     * @brief Clear all bits to 0
     */
    void clear_all() noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            m_words[i] = 0;
        }
    }

    /**
     * @brief Flip all bits
     */
    void flip_all() noexcept
    {
        for (size_t i = 0; i < NUM_WORDS - 1; ++i)
        {
            m_words[i] = ~m_words[i];
        }
        m_words[NUM_WORDS - 1] = (~m_words[NUM_WORDS - 1]) & LAST_WORD_MASK;
    }

    /**
     * @brief Alias for clear_all() (std::bitset compatibility)
     */
    void reset() noexcept
    {
        clear_all();
    }

    /**
     * @brief Alias for clear() (std::bitset compatibility)
     */
    void reset(size_t index)
    {
        clear(index);
    }

    // ========================================================================
    // Range operations
    // ========================================================================

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 28020) // SAL annotation false positive on bit shift operations
#endif

    /**
     * @brief Set all bits in range [start, end) to 1
     * @param start First bit index (inclusive)
     * @param end Last bit index (exclusive)
     * @throws std::out_of_range if start > end or end > N
     */
    void set_range(size_t start, size_t end)
    {
        if (start > end || end > N)
        {
            throw std::out_of_range("BitSet::set_range: invalid range");
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
            m_words[start_word] |= mask;
        }
        else
        {
            m_words[start_word] |= detail::mask_from(start_bit);
            for (size_t i = start_word + 1; i < end_word; ++i)
            {
                m_words[i] = ~0ULL;
            }
            m_words[end_word] |= detail::mask_up_to(end_bit);
        }
    }

    /**
     * @brief Clear all bits in range [start, end) to 0
     * @param start First bit index (inclusive)
     * @param end Last bit index (exclusive)
     * @throws std::out_of_range if start > end or end > N
     */
    void clear_range(size_t start, size_t end)
    {
        if (start > end || end > N)
        {
            throw std::out_of_range("BitSet::clear_range: invalid range");
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
            m_words[start_word] &= ~mask;
        }
        else
        {
            m_words[start_word] &= ~detail::mask_from(start_bit);
            for (size_t i = start_word + 1; i < end_word; ++i)
            {
                m_words[i] = 0;
            }
            m_words[end_word] &= ~detail::mask_up_to(end_bit);
        }
    }

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

        if constexpr (LAST_WORD_BITS != 0)
        {
            if (end_word == NUM_WORDS - 1)
            {
                m_words[NUM_WORDS - 1] &= LAST_WORD_MASK;
            }
        }
    }

    /**
     * @brief Count set bits in range [start, end)
     * @param start First bit index (inclusive)
     * @param end Last bit index (exclusive)
     * @return Number of set bits in range
     * @throws std::out_of_range if start > end or end > N
     */
    [[nodiscard]] size_t count_range(size_t start, size_t end) const
    {
        if (start > end || end > N)
        {
            throw std::out_of_range("BitSet::count_range: invalid range");
        }
        if (start == end)
        {
            return 0;
        }

        size_t start_word = start / BITS_PER_WORD;
        size_t end_word = (end - 1) / BITS_PER_WORD;
        size_t start_bit = start % BITS_PER_WORD;
        size_t end_bit = (end - 1) % BITS_PER_WORD;

        if (start_word == end_word)
        {
            uint64_t mask = detail::mask_up_to(end_bit) & detail::mask_from(start_bit);
            return detail::popcnt64(m_words[start_word] & mask);
        }

        size_t cnt = detail::popcnt64(m_words[start_word] & detail::mask_from(start_bit));
        for (size_t i = start_word + 1; i < end_word; ++i)
        {
            cnt += detail::popcnt64(m_words[i]);
        }
        cnt += detail::popcnt64(m_words[end_word] & detail::mask_up_to(end_bit));
        return cnt;
    }

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    // ========================================================================
    // Query operations
    // ========================================================================

    /**
     * @brief Count number of set bits
     * @return Number of 1 bits
     */
    [[nodiscard]] size_t count() const noexcept
    {
        size_t total = 0;
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            total += detail::popcnt64(m_words[i]);
        }
        return total;
    }

    /**
     * @brief Check if any bit is set
     */
    [[nodiscard]] bool any() const noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            if (m_words[i] != 0)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Check if no bits are set
     */
    [[nodiscard]] bool none() const noexcept
    {
        return !any();
    }

    /**
     * @brief Check if all bits are set
     */
    [[nodiscard]] bool all() const noexcept
    {
        for (size_t i = 0; i < NUM_WORDS - 1; ++i)
        {
            if (m_words[i] != ~0ULL)
            {
                return false;
            }
        }
        return m_words[NUM_WORDS - 1] == LAST_WORD_MASK;
    }

    /**
     * @brief Get bit capacity (template parameter N)
     */
    [[nodiscard]] constexpr size_t size() const noexcept
    {
        return N;
    }

    /**
     * @brief Convert to string representation (MSB first, like std::bitset)
     * @param zero Character for 0 bits
     * @param one Character for 1 bits
     * @return String of N characters, MSB first
     */
    [[nodiscard]] std::string to_string(char zero = '0', char one = '1') const
    {
        std::string result(N, zero);
        for (size_t i = 0; i < N; ++i)
        {
            if (test_unchecked(i))
            {
                result[N - 1 - i] = one;
            }
        }
        return result;
    }

    /**
     * @brief Convert to unsigned long
     * @return Value as unsigned long
     * @throws std::overflow_error if the value does not fit
     */
    [[nodiscard]] unsigned long to_ulong() const
    {
        constexpr size_t kUlongBits = sizeof(unsigned long) * 8u;

        if constexpr (N > kUlongBits)
        {
            for (size_t i = kUlongBits; i < N; ++i)
            {
                if (test_unchecked(i))
                {
                    throw std::overflow_error("BitSet::to_ulong: overflow");
                }
            }
        }

        return static_cast<unsigned long>(m_words[0]);
    }

    /**
     * @brief Convert to unsigned long long
     * @return Value as unsigned long long
     * @throws std::overflow_error if the value does not fit
     */
    [[nodiscard]] unsigned long long to_ullong() const
    {
        constexpr size_t kUlongLongBits = sizeof(unsigned long long) * 8u;

        if constexpr (N > kUlongLongBits)
        {
            for (size_t i = kUlongLongBits; i < N; ++i)
            {
                if (test_unchecked(i))
                {
                    throw std::overflow_error("BitSet::to_ullong: overflow");
                }
            }
        }

        return static_cast<unsigned long long>(m_words[0]);
    }

    // ========================================================================
    // Find operations
    // ========================================================================

    /**
     * @brief Find first set bit
     * @return Index of first set bit, or N if none
     */
    [[nodiscard]] size_t find_first() const noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            if (m_words[i] != 0)
            {
                size_t idx = i * BITS_PER_WORD + detail::ctz64(m_words[i]);
                return (idx < N) ? idx : N;
            }
        }
        return N;
    }

    /**
     * @brief Find next set bit after given index
     * @param after Search starts after this index
     * @return Index of next set bit, or N if none
     */
    [[nodiscard]] size_t find_next(size_t after) const noexcept
    {
        ++after;
        if (after >= N)
        {
            return N;
        }

        size_t word_idx = after / BITS_PER_WORD;
        size_t bit_offset = after % BITS_PER_WORD;

        uint64_t word = m_words[word_idx] & detail::mask_from(bit_offset);
        if (word != 0)
        {
            size_t idx = word_idx * BITS_PER_WORD + detail::ctz64(word);
            return (idx < N) ? idx : N;
        }

        for (size_t i = word_idx + 1; i < NUM_WORDS; ++i)
        {
            if (m_words[i] != 0)
            {
                size_t idx = i * BITS_PER_WORD + detail::ctz64(m_words[i]);
                return (idx < N) ? idx : N;
            }
        }

        return N;
    }

    /**
     * @brief Find last set bit
     * @return Index of last set bit, or N if none
     */
    [[nodiscard]] size_t find_last() const noexcept
    {
        for (size_t i = NUM_WORDS; i-- > 0;)
        {
            if (m_words[i] != 0)
            {
                size_t bit_pos = 63 - detail::clz64(m_words[i]);
                size_t idx = i * BITS_PER_WORD + bit_pos;
                return (idx < N) ? idx : N;
            }
        }
        return N;
    }

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

        --before;

        const size_t word_idx = before / BITS_PER_WORD;
        const size_t bit_offset = before % BITS_PER_WORD;

        uint64_t word = m_words[word_idx] & detail::mask_up_to(bit_offset);
        if (word != 0)
        {
            return word_idx * BITS_PER_WORD + (63 - detail::clz64(word));
        }

        for (size_t i = word_idx; i-- > 0;)
        {
            if (m_words[i] != 0)
            {
                return i * BITS_PER_WORD + (63 - detail::clz64(m_words[i]));
            }
        }

        return N;
    }

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
                return i * BITS_PER_WORD + detail::ctz64(~m_words[i]);
            }
        }

        const uint64_t last_inverted = ~m_words[NUM_WORDS - 1] & LAST_WORD_MASK;
        if (last_inverted != 0)
        {
            const size_t idx = (NUM_WORDS - 1) * BITS_PER_WORD + detail::ctz64(last_inverted);
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

        const size_t word_idx = after / BITS_PER_WORD;
        const size_t bit_offset = after % BITS_PER_WORD;

        uint64_t word = ~m_words[word_idx] & detail::mask_from(bit_offset);
        if (word_idx == NUM_WORDS - 1)
        {
            word &= LAST_WORD_MASK;
        }

        if (word != 0)
        {
            const size_t idx = word_idx * BITS_PER_WORD + detail::ctz64(word);
            return (idx < N) ? idx : N;
        }

        for (size_t i = word_idx + 1; i < NUM_WORDS - 1; ++i)
        {
            if (m_words[i] != ~0ULL)
            {
                return i * BITS_PER_WORD + detail::ctz64(~m_words[i]);
            }
        }

        if (word_idx < NUM_WORDS - 1)
        {
            const uint64_t last_inverted = ~m_words[NUM_WORDS - 1] & LAST_WORD_MASK;
            if (last_inverted != 0)
            {
                const size_t idx = (NUM_WORDS - 1) * BITS_PER_WORD + detail::ctz64(last_inverted);
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
        const uint64_t last_inverted = ~m_words[NUM_WORDS - 1] & LAST_WORD_MASK;
        if (last_inverted != 0)
        {
            const size_t bit_pos = 63 - detail::clz64(last_inverted);
            const size_t idx = (NUM_WORDS - 1) * BITS_PER_WORD + bit_pos;
            return (idx < N) ? idx : N;
        }

        for (size_t i = NUM_WORDS - 1; i-- > 0;)
        {
            if (m_words[i] != ~0ULL)
            {
                const size_t bit_pos = 63 - detail::clz64(~m_words[i]);
                return i * BITS_PER_WORD + bit_pos;
            }
        }

        return N;
    }

    // ========================================================================
    // Set operations
    // ========================================================================

    /**
     * @brief Check if this set is a subset of another
     * @param other The potential superset
     * @return true if every bit set in this is also set in other
     */
    [[nodiscard]] bool is_subset_of(const BitSet& other) const noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            if ((m_words[i] & ~other.m_words[i]) != 0)
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Check if this set intersects with another
     * @param other The other set
     * @return true if any bit is set in both sets
     */
    [[nodiscard]] bool intersects(const BitSet& other) const noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            if ((m_words[i] & other.m_words[i]) != 0)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Count differing bits between two sets (Hamming distance)
     */
    [[nodiscard]] size_t hamming_distance(const BitSet& other) const noexcept
    {
        size_t dist = 0;
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            dist += detail::popcnt64(m_words[i] ^ other.m_words[i]);
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
     * @brief Check if this set is a proper subset of another
     */
    [[nodiscard]] bool is_proper_subset_of(const BitSet& other) const noexcept
    {
        return is_subset_of(other) && (*this != other);
    }

    // ========================================================================
    // Bitwise operators
    // ========================================================================

    [[nodiscard]] BitSet operator~() const noexcept
    {
        BitSet result;
        for (size_t i = 0; i < NUM_WORDS - 1; ++i)
        {
            result.m_words[i] = ~m_words[i];
        }
        result.m_words[NUM_WORDS - 1] = (~m_words[NUM_WORDS - 1]) & LAST_WORD_MASK;
        return result;
    }

    [[nodiscard]] BitSet operator&(const BitSet& other) const noexcept
    {
        BitSet result;
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            result.m_words[i] = m_words[i] & other.m_words[i];
        }
        return result;
    }

    [[nodiscard]] BitSet operator|(const BitSet& other) const noexcept
    {
        BitSet result;
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            result.m_words[i] = m_words[i] | other.m_words[i];
        }
        return result;
    }

    [[nodiscard]] BitSet operator^(const BitSet& other) const noexcept
    {
        BitSet result;
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            result.m_words[i] = m_words[i] ^ other.m_words[i];
        }
        return result;
    }

    BitSet& operator&=(const BitSet& other) noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            m_words[i] &= other.m_words[i];
        }
        return *this;
    }

    BitSet& operator|=(const BitSet& other) noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            m_words[i] |= other.m_words[i];
        }
        return *this;
    }

    BitSet& operator^=(const BitSet& other) noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            m_words[i] ^= other.m_words[i];
        }
        return *this;
    }

    // ========================================================================
    // Shift operators
    // ========================================================================

    /**
     * @brief Left-shift all bits by n positions
     * @param n Number of positions to shift (bits shifted out are lost)
     * @return New BitSet with shifted bits
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
            for (size_t i = word_shift; i < NUM_WORDS; ++i)
            {
                result.m_words[i] = m_words[i - word_shift];
            }
        }
        else
        {
            const size_t inv_shift = BITS_PER_WORD - bit_shift;
            for (size_t i = NUM_WORDS - 1; i > word_shift; --i)
            {
                result.m_words[i] = (m_words[i - word_shift] << bit_shift) |
                                    (m_words[i - word_shift - 1] >> inv_shift);
            }
            result.m_words[word_shift] = m_words[0] << bit_shift;
        }

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
            for (size_t i = 0; i < NUM_WORDS - word_shift; ++i)
            {
                result.m_words[i] = m_words[i + word_shift];
            }
        }
        else
        {
            const size_t inv_shift = BITS_PER_WORD - bit_shift;
            for (size_t i = 0; i < NUM_WORDS - word_shift - 1; ++i)
            {
                result.m_words[i] = (m_words[i + word_shift] >> bit_shift) |
                                    (m_words[i + word_shift + 1] << inv_shift);
            }
            result.m_words[NUM_WORDS - word_shift - 1] = m_words[NUM_WORDS - 1] >> bit_shift;
        }

        if constexpr (LAST_WORD_BITS != 0)
        {
            result.m_words[NUM_WORDS - 1] &= LAST_WORD_MASK;
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

    // ========================================================================
    // Comparison operators
    // ========================================================================

    [[nodiscard]] bool operator==(const BitSet& other) const noexcept
    {
        for (size_t i = 0; i < NUM_WORDS; ++i)
        {
            if (m_words[i] != other.m_words[i])
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool operator!=(const BitSet& other) const noexcept
    {
        return !(*this == other);
    }

    // ========================================================================
    // Iteration
    // ========================================================================

    [[nodiscard]] Iterator begin() const noexcept
    {
        return Iterator(this, find_first());
    }

    [[nodiscard]] Iterator end() const noexcept
    {
        return Iterator(this, N);
    }

    // ========================================================================
    // Raw access (for advanced use / serialization)
    // ========================================================================

    /**
     * @brief Get pointer to underlying word array
     */
    [[nodiscard]] const uint64_t* data() const noexcept
    {
        return m_words.data();
    }

    /**
     * @brief Get mutable pointer to underlying word array
     * @warning Caller must maintain invariant: unused bits in last word must be 0
     */
    [[nodiscard]] uint64_t* data() noexcept
    {
        return m_words.data();
    }

    /**
     * @brief Get number of 64-bit words used internally
     */
    [[nodiscard]] static constexpr size_t word_count() noexcept
    {
        return NUM_WORDS;
    }

private:
    std::array<uint64_t, NUM_WORDS> m_words;
};

} // namespace fat_p

// ============================================================================
// std::hash specialization
// ============================================================================

namespace std
{

template <size_t N>
struct hash<fat_p::BitSet<N>>
{
    size_t operator()(const fat_p::BitSet<N>& bs) const noexcept
    {
        size_t h = 0;
        const uint64_t* words = bs.data();
        for (size_t i = 0; i < fat_p::BitSet<N>::word_count(); ++i)
        {
            h ^= hash<uint64_t>{}(words[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

} // namespace std
