// StableHashMap.h - High-performance reference-stable hash map
//
// Features:
//   - Reference stability: pointers/references valid across insert/reserve/rehash
//   - SIMD-accelerated probing (SSE2/AVX2/NEON)
//   - Configurable allocator policy (NewDelete, Block, Pool)
//   - Fused find-or-insert optimization
//   - Built-in hash finalizer (robust against bad user hashes)
//   - Heterogeneous lookup support
//   - Single header, C++17, minimal dependencies
//
// Usage:
//   fat_p::StableHashMap<std::string, MyObject> map;
//   auto [ptr, inserted] = map.insert("key", MyObject{});
//   MyObject* stable_ptr = ptr;  // ptr remains valid forever (until erase)
//
//   map.insert("other", MyObject{});  // stable_ptr still valid!
//   map.reserve(1000000);             // stable_ptr still valid!
//   map.erase("other");               // stable_ptr still valid!
//
//   *stable_ptr = MyObject{...};      // OK - can mutate through pointer
//   map.erase("key");                 // NOW stable_ptr is invalid
//
// Allocator Policies (see NodeAllocators.h):
//   NewDeleteAllocator (default) - Best for lookup-heavy workloads
//   BlockAllocator               - Best for insert/erase-heavy workloads
//   PoolAllocator<N>             - Best for fixed-size, max performance
//
// Example with BlockAllocator for insert-heavy workload:
//   fat_p::StableHashMap<K, V, std::hash<K>, std::equal_to<K>,
//                        fat_p::BlockAllocator> map;
//
// Performance vs std::unordered_map (N=1M, AVX2, NewDeleteAllocator):
//   Insert: 2.3x faster
//   Find:   1.8x faster
//   Miss:   11x faster
//   Erase:  4.6x faster
//
// Compiler flags for AVX2: -mavx2 or -march=native (GCC/Clang), /arch:AVX2 (MSVC)
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "AllocationStrategies.h"
#include "FatPSimdDetection.h"

#if defined(_MSC_VER)
#include <malloc.h>
#endif

// SIMD headers
#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(FATP_SIMD_AVX2) || defined(__AVX2__)
#include <immintrin.h>
#else
#include <emmintrin.h>
#endif
#endif

namespace fat_p
{

// Forward declaration for test framework friend access
namespace testing
{
namespace stablehashmap
{
template <typename M>
class StableHashMapTester;
} // namespace stablehashmap
} // namespace testing

namespace stablehash_detail
{

// Control byte values
static constexpr uint8_t kEmpty = 0b10000000;
static constexpr uint8_t kDeleted = 0b11111110;
static constexpr uint8_t kSentinel = 0b11111111;

inline bool is_full(uint8_t ctrl)
{
    return ctrl < 0x80;
}
inline bool is_empty(uint8_t ctrl)
{
    return ctrl == kEmpty;
}
inline bool is_deleted(uint8_t ctrl)
{
    return ctrl == kDeleted;
}
inline bool is_empty_or_deleted(uint8_t ctrl)
{
    return ctrl >= 0x80 && ctrl != kSentinel;
}

// H2: Extract 7 bits from HIGH bits of hash for control byte matching.
// CRITICAL: Using low bits would correlate with bucket index (hash & mask_),
// causing false-positive SIMD matches and degraded miss performance.
// Fix identified by ChatGPT code review.
inline uint8_t H2(size_t hash)
{
    // Use top 7 bits (shift by 57 on 64-bit, 25 on 32-bit)
    constexpr size_t shift = sizeof(size_t) > 4 ? 57 : 25;
    return static_cast<uint8_t>((hash >> shift) & 0x7F);
}

// Hash finalizer - makes the map robust against bad user hashes
// (e.g., std::hash<int> is identity on many platforms)
inline size_t mix_hash(size_t h)
{
    // SplitMix64 finalizer - excellent avalanche properties
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

// Portable aligned allocation
inline void* aligned_alloc(size_t alignment, size_t size)
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0)
    {
        return nullptr;
    }
    return ptr;
#endif
}

inline void aligned_free(void* ptr)
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// BitMask for iterating set bits
struct BitMask
{
    uint32_t mask;

    explicit BitMask(uint32_t m)
        : mask(m)
    {
    }
    explicit operator bool() const
    {
        return mask != 0;
    }

    uint32_t lowest_set_bit() const
    {
#if defined(_MSC_VER)
        unsigned long idx;
        _BitScanForward(&idx, mask);
        return idx;
#else
        return static_cast<uint32_t>(__builtin_ctz(mask));
#endif
    }

    BitMask& operator++()
    {
        mask &= (mask - 1);
        return *this;
    }

    uint32_t operator*() const
    {
        return lowest_set_bit();
    }

    struct Iterator
    {
        uint32_t mask;
        uint32_t operator*() const
        {
#if defined(_MSC_VER)
            unsigned long idx;
            _BitScanForward(&idx, mask);
            return idx;
#else
            return static_cast<uint32_t>(__builtin_ctz(mask));
#endif
        }
        Iterator& operator++()
        {
            mask &= (mask - 1);
            return *this;
        }
        bool operator!=(const Iterator& o) const
        {
            return mask != o.mask;
        }
    };

    Iterator begin() const
    {
        return {mask};
    }
    Iterator end() const
    {
        return {0};
    }
};

#if defined(FATP_STABLEHASHMAP_DIAGNOSTICS)
inline uint32_t popcount32(uint32_t x) noexcept
{
    uint32_t c = 0;
    while (x)
    {
        x &= (x - 1);
        ++c;
    }
    return c;
}
#endif

// SIMD Group operations
#if defined(__aarch64__) || defined(_M_ARM64)
// ARM NEON implementation
struct Group
{
    static constexpr size_t kWidth = 16;
    uint8x16_t ctrl;

    explicit Group(const uint8_t* pos)
        : ctrl(vld1q_u8(pos))
    {
    }

    BitMask match(uint8_t h2) const
    {
        uint8x16_t dup = vdupq_n_u8(h2);
        uint8x16_t cmp = vceqq_u8(ctrl, dup);

        static const uint8_t kShift[] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
        uint8x16_t shifts = vld1q_u8(kShift);
        uint8x16_t bits = vandq_u8(cmp, shifts);

        uint8x8_t lo = vget_low_u8(bits);
        uint8x8_t hi = vget_high_u8(bits);
        uint8x8_t sum_lo = vpadd_u8(lo, lo);
        uint8x8_t sum_hi = vpadd_u8(hi, hi);
        sum_lo = vpadd_u8(sum_lo, sum_lo);
        sum_hi = vpadd_u8(sum_hi, sum_hi);
        sum_lo = vpadd_u8(sum_lo, sum_lo);
        sum_hi = vpadd_u8(sum_hi, sum_hi);

        uint32_t mask = (vget_lane_u8(sum_hi, 0) << 8) | vget_lane_u8(sum_lo, 0);
        return BitMask(mask);
    }

    BitMask match_empty() const
    {
        return match(kEmpty);
    }

    BitMask match_empty_or_deleted() const
    {
        uint8x16_t threshold = vdupq_n_u8(0x80);
        uint8x16_t cmp = vcgeq_u8(ctrl, threshold);

        static const uint8_t kShift[] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
        uint8x16_t shifts = vld1q_u8(kShift);
        uint8x16_t bits = vandq_u8(cmp, shifts);

        uint8x8_t lo = vget_low_u8(bits);
        uint8x8_t hi = vget_high_u8(bits);
        uint8x8_t sum_lo = vpadd_u8(lo, lo);
        uint8x8_t sum_hi = vpadd_u8(hi, hi);
        sum_lo = vpadd_u8(sum_lo, sum_lo);
        sum_hi = vpadd_u8(sum_hi, sum_hi);
        sum_lo = vpadd_u8(sum_lo, sum_lo);
        sum_hi = vpadd_u8(sum_hi, sum_hi);

        uint32_t mask = (vget_lane_u8(sum_hi, 0) << 8) | vget_lane_u8(sum_lo, 0);
        uint8x16_t sentinel_mask = vceqq_u8(ctrl, vdupq_n_u8(kSentinel));
        uint64_t sentinel_any = vgetq_lane_u64(vreinterpretq_u64_u8(sentinel_mask), 0) |
                                vgetq_lane_u64(vreinterpretq_u64_u8(sentinel_mask), 1);
        if (sentinel_any)
        {
            for (int i = 0; i < 16; ++i)
            {
                if (vgetq_lane_u8(ctrl, i) == kSentinel)
                {
                    mask &= ~(1u << i);
                }
            }
        }
        return BitMask(mask);
    }

    static const char* simd_name()
    {
        return "NEON";
    }
};

#elif (defined(FATP_SIMD_AVX2) || defined(__AVX2__)) && (defined(__x86_64__) || defined(_M_X64))
// AVX2 implementation
struct Group
{
    static constexpr size_t kWidth = 32;
    __m256i ctrl;

    explicit Group(const uint8_t* pos)
        : ctrl(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(pos)))
    {
    }

    BitMask match(uint8_t h2) const
    {
        __m256i needle = _mm256_set1_epi8(static_cast<char>(h2));
        return BitMask(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(ctrl, needle))));
    }

    BitMask match_empty() const
    {
        return match(kEmpty);
    }

    BitMask match_empty_or_deleted() const
    {
        // Abseil scheme: Full slots have h2 = 0-127 (bit 7 clear)
        // Empty (0x80) and Deleted (0xFE) have bit 7 set
        // Just extract high bits directly
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(ctrl));

        // Exclude sentinel (0xFF)
        __m256i sentinel = _mm256_set1_epi8(static_cast<char>(kSentinel));
        __m256i is_sentinel = _mm256_cmpeq_epi8(ctrl, sentinel);
        uint32_t sentinel_mask = static_cast<uint32_t>(_mm256_movemask_epi8(is_sentinel));

        return BitMask(mask & ~sentinel_mask);
    }

    static const char* simd_name()
    {
        return "AVX2";
    }
};

#else
// SSE2 implementation (fallback)
struct Group
{
    static constexpr size_t kWidth = 16;
    __m128i ctrl;

    explicit Group(const uint8_t* pos)
        : ctrl(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pos)))
    {
    }

    BitMask match(uint8_t h2) const
    {
        __m128i needle = _mm_set1_epi8(static_cast<char>(h2));
        return BitMask(static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, needle))));
    }

    BitMask match_empty() const
    {
        return match(kEmpty);
    }

    BitMask match_empty_or_deleted() const
    {
        // Abseil scheme: Full slots have h2 = 0-127 (bit 7 clear)
        // Empty (0x80) and Deleted (0xFE) have bit 7 set
        uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(ctrl)) & 0xFFFF;

        // Exclude sentinel (0xFF)
        __m128i sentinel = _mm_set1_epi8(static_cast<char>(kSentinel));
        __m128i is_sentinel = _mm_cmpeq_epi8(ctrl, sentinel);
        uint32_t sentinel_mask = static_cast<uint32_t>(_mm_movemask_epi8(is_sentinel));

        return BitMask(mask & ~sentinel_mask);
    }

    static const char* simd_name()
    {
        return "SSE2";
    }
};
#endif

// Probe sequence using triangular numbers
struct ProbeSequence
{
    size_t pos_;
    size_t stride_;
    size_t mask_;

    ProbeSequence(size_t hash, size_t mask)
        : pos_(hash & mask)
        , stride_(0)
        , mask_(mask)
    {
    }

    size_t offset() const
    {
        return pos_;
    }
    size_t offset(size_t i) const
    {
        return (pos_ + i) & mask_;
    }

    void next()
    {
        stride_ += Group::kWidth;
        pos_ = (pos_ + stride_) & mask_;
    }
};

} // namespace stablehash_detail

// ============================================================================
// StableHashMap - Reference-stable hash map with SIMD acceleration
// ============================================================================
//
// Template parameters:
//   Key       - Key type
//   Value     - Mapped value type
//   Hash      - Hash function (default: std::hash<Key>)
//   KeyEqual  - Key equality comparator (default: std::equal_to<Key>)
//   NodeAllocator - Allocator template for nodes (default: NewDeleteAllocator)
//
// Available allocators (see AllocationStrategies.h):
//   NewDeleteAllocator (default) - Best for lookup-heavy workloads
//   BlockAllocator               - Best for insert/erase-heavy workloads
//   PoolAllocator<N>::Allocator  - Best for fixed-size, max performance
//
// Custom allocators must satisfy:
//   - NodeAllocator<Node> where Node has (Key key, Value value)
//   - Node* allocate(Args&&...) - allocate and construct in-place
//   - void deallocate(Node* ptr) - destroy and recycle
//   - Move constructible and move assignable
//   - Default constructible
//
template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          template <typename> class NodeAllocator = NewDeleteAllocator>
class StableHashMap
{
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = size_t;
    using hasher = Hash;
    using key_equal = KeyEqual;

    static const char* simd_backend()
    {
        return stablehash_detail::Group::simd_name();
    }

#if defined(FATP_STABLEHASHMAP_DIAGNOSTICS)
    /**
     * @brief Probe diagnostics counters for unsuccessful lookups.
     *
     * @details
     * These counters are intended for benchmarks and investigations. They are not part
     * of the StableHashMap contract and are compiled only when FATP_STABLEHASHMAP_DIAGNOSTICS
     * is defined.
     */
    struct ProbeCounters
    {
        uint64_t mGroupsVisited = 0;     ///< Number of control groups visited
        uint64_t mFullGroupsVisited = 0; ///< Groups that contained no empty slot (miss continues)
        uint64_t mFullSlotsVisited = 0;  ///< Sum of full slots across visited groups
        uint64_t mTagMatches = 0;        ///< Sum of H2 tag matches across visited groups
    };

    /**
     * @brief Diagnostic find that accumulates probe counters.
     *
     * @param key Key to find
     * @param out Accumulated counters (incremented; not reset)
     * @return true if found, false otherwise
     *
     * @note Intended for miss analysis. It performs additional work to compute occupancy
     *       and tag match counts. Do not use for normal lookups.
     */
    bool diagnostic_find(const Key& key, ProbeCounters& out) const
    {
        if (capacity_ == 0)
        {
            return false;
        }

        const size_t h = hash_key(key);
        const uint8_t h2 = stablehash_detail::H2(h);
        stablehash_detail::ProbeSequence seq(h, mask_);

        while (true)
        {
            stablehash_detail::Group g(ctrl_ + seq.offset());
            out.mGroupsVisited++;

            const stablehash_detail::BitMask empty = g.match_empty();

            const uint32_t available_mask = g.match_empty_or_deleted().mask;
            const uint32_t full =
                static_cast<uint32_t>(stablehash_detail::Group::kWidth) - stablehash_detail::popcount32(available_mask);
            out.mFullSlotsVisited += full;

            stablehash_detail::BitMask matches = g.match(h2);
            if (empty)
            {
                // Only consider tag matches strictly before the first empty slot.
                const uint32_t first_empty_bit = empty.mask & (~empty.mask + 1u); // lowest set bit
                matches.mask &= (first_empty_bit - 1u);
            }
            out.mTagMatches += stablehash_detail::popcount32(matches.mask);

            for (uint32_t i : matches)
            {
                size_t idx = seq.offset(i);
                if (stablehash_detail::is_full(ctrl_[idx]) && key_equal_(nodes_[idx]->key, key))
                {
                    return true;
                }
            }

            if (empty)
            {
                return false;
            }
            ++out.mFullGroupsVisited;
            seq.next();
        }
    }
#endif


    // Friend declaration for test framework access to internals
    template <typename M>
    friend class fat_p::testing::stablehashmap::StableHashMapTester;

private:
    using Group = stablehash_detail::Group;
    using BitMask = stablehash_detail::BitMask;
    using ProbeSequence = stablehash_detail::ProbeSequence;

    // Node: separately allocated in blocks, never moves once created
    struct Node
    {
        Key key;
        Value value;

        template <typename K, typename V>
        Node(K&& k, V&& v)
            : key(std::forward<K>(k))
            , value(std::forward<V>(v))
        {
        }
    };

public:
    // Expose Node type for custom allocator implementations
    using node_type = Node;
    using allocator_type = NodeAllocator<Node>;

private:
    uint8_t* ctrl_ = nullptr;
    Node** nodes_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    size_t growth_threshold_ = 0;
    size_t tombstones_ = 0;

    float max_load_factor_ = 0.8f; // Lower than 0.875 for faster miss detection
    bool frozen_ = false;
    Hash hasher_;
    KeyEqual key_equal_;
    allocator_type allocator_;

    static constexpr size_t kMinCapacity = Group::kWidth * 2;

    // Set control byte with mirroring for group reads that wrap
    void set_ctrl(size_t idx, uint8_t value)
    {
        ctrl_[idx] = value;
        // Mirror first Group::kWidth slots to the end for wrap-around reads
        if (idx < Group::kWidth)
        {
            ctrl_[capacity_ + idx] = value;
        }
    }

    // Detect if hash already has good avalanche properties (opt-out marker)
    template <typename T, typename = void>
    struct has_avalanching : std::false_type
    {
    };

    template <typename T>
    struct has_avalanching<T, std::void_t<typename T::is_avalanching>> : std::true_type
    {
    };

    // SFINAE traits for heterogeneous lookup (prevents hard errors)
    // Checks if Hash can be invoked with K, and KeyEqual can compare Key with K
    template <typename K>
    static constexpr bool is_hetero_lookup_enabled =
        std::is_invocable_r_v<size_t, const Hash&, const K&> &&
        (std::is_invocable_r_v<bool, const KeyEqual&, const Key&, const K&> ||
         std::is_invocable_r_v<bool, const KeyEqual&, const K&, const Key&>);

    // Hash with optional finalizer for robustness
    // Skipped if Hash::is_avalanching is defined (user claims good avalanche)
    // Note: hash value 0 is valid. With kEmpty=0x80 and H2 using high bits,
    // H2(0)=0 cannot be confused with empty slots.
    template <typename K>
    size_t hash_key(const K& key) const
    {
        size_t h = hasher_(key);
        if constexpr (has_avalanching<Hash>::value)
        {
            return h; // Trust user's hash as-is
        }
        else
        {
            return stablehash_detail::mix_hash(h);
        }
    }

    void allocate(size_t cap)
    {
        capacity_ = cap;
        mask_ = cap - 1;

        size_t ctrl_size = cap + Group::kWidth;
        ctrl_ = static_cast<uint8_t*>(stablehash_detail::aligned_alloc(Group::kWidth, ctrl_size));
        if (!ctrl_)
        {
            throw std::bad_alloc();
        }
        std::memset(ctrl_, stablehash_detail::kEmpty, ctrl_size);
        ctrl_[cap] = stablehash_detail::kSentinel;

        nodes_ = static_cast<Node**>(stablehash_detail::aligned_alloc(alignof(Node*), cap * sizeof(Node*)));
        if (!nodes_)
        {
            stablehash_detail::aligned_free(ctrl_);
            ctrl_ = nullptr;
            throw std::bad_alloc();
        }
        std::memset(nodes_, 0, cap * sizeof(Node*));

        // CRITICAL: Always keep at least 1 empty slot to prevent infinite loops
        // in find_slot() and find_or_prepare_insert(). Fix identified by ChatGPT.
        size_t threshold = static_cast<size_t>(cap * max_load_factor_);
        growth_threshold_ = (threshold >= cap) ? cap - 1 : threshold;
        if (growth_threshold_ == 0 && cap > 0)
        {
            growth_threshold_ = 1;
        }
    }

    void deallocate()
    {
        if (ctrl_)
        {
            // Nodes are owned by allocator_, which handles destruction
            for (size_t i = 0; i < capacity_; ++i)
            {
                if (stablehash_detail::is_full(ctrl_[i]))
                {
                    allocator_.deallocate(nodes_[i]);
                }
            }
            stablehash_detail::aligned_free(nodes_);
            stablehash_detail::aligned_free(ctrl_);
            ctrl_ = nullptr;
            nodes_ = nullptr;
        }
    }

    // Fused find-or-prepare-insert: single pass for both find and insert slot
    // Returns: {index, found, insert_slot}
    // - If found: index is the existing key's slot
    // - If not found: insert_slot is where to put the new node
    template <typename K>
    std::tuple<size_t, bool, size_t> find_or_prepare_insert(const K& key, size_t h) const
    {
        uint8_t h2 = stablehash_detail::H2(h);
        stablehash_detail::ProbeSequence seq(h, mask_);
        size_t first_available = SIZE_MAX;

        while (true)
        {
            stablehash_detail::Group g(ctrl_ + seq.offset());

            const auto empty = g.match_empty();

            // Check for tag matches, but ignore candidates after the first empty slot
            // in this group. Once we hit an empty, the probe sequence terminates.
            auto matches = g.match(h2);
            if (empty)
            {
                // Mask matches to bits strictly before the first empty slot.
                // Equivalent to:
                //   first = ctz(empty.mask);
                //   matches.mask &= ((1u << first) - 1);
                const uint32_t first_empty_bit = empty.mask & (~empty.mask + 1u); // lowest set bit
                matches.mask &= (first_empty_bit - 1u);
            }

            for (uint32_t i : matches)
            {
                size_t idx = seq.offset(i);
                if (key_equal_(nodes_[idx]->key, key))
                {
                    return {idx, true, SIZE_MAX}; // Found
                }
            }

            // Track first available slot (empty or deleted)
            if (first_available == SIZE_MAX)
            {
                auto available = g.match_empty_or_deleted();
                if (available)
                {
                    first_available = seq.offset(available.lowest_set_bit());
                }
            }

            // Stop on empty (definitive miss)
            if (empty)
            {
                return {SIZE_MAX, false, first_available};
            }

            seq.next();
        }
    }

    // Find slot only (for find/contains/erase)
    template <typename K>
    std::pair<size_t, bool> find_slot(const K& key, size_t h) const
    {
        if (capacity_ == 0)
        {
            return {SIZE_MAX, false};
        }

        uint8_t h2 = stablehash_detail::H2(h);
        stablehash_detail::ProbeSequence seq(h, mask_);

        while (true)
        {
            stablehash_detail::Group g(ctrl_ + seq.offset());

            const auto empty = g.match_empty();
            auto matches = g.match(h2);
            if (empty)
            {
                const uint32_t first_empty_bit = empty.mask & (~empty.mask + 1u); // lowest set bit
                matches.mask &= (first_empty_bit - 1u);
            }

            for (uint32_t i : matches)
            {
                size_t idx = seq.offset(i);
                if (key_equal_(nodes_[idx]->key, key))
                {
                    return {idx, true};
                }
            }

            if (empty)
            {
                return {SIZE_MAX, false};
            }
            seq.next();
        }
    }

    void rehash_internal(size_t new_cap)
    {
        uint8_t* old_ctrl = ctrl_;
        Node** old_nodes = nodes_;
        size_t old_cap = capacity_;

        ctrl_ = nullptr;
        nodes_ = nullptr;
        allocate(new_cap);
        size_ = 0;
        tombstones_ = 0;

        if (old_ctrl)
        {
            for (size_t i = 0; i < old_cap; ++i)
            {
                if (stablehash_detail::is_full(old_ctrl[i]))
                {
                    Node* node = old_nodes[i];
                    size_t h = hash_key(node->key);

                    // Find empty slot (no deleted slots in fresh table)
                    stablehash_detail::ProbeSequence seq(h, mask_);
                    while (true)
                    {
                        stablehash_detail::Group g(ctrl_ + seq.offset());
                        auto empty = g.match_empty();
                        if (empty)
                        {
                            size_t idx = seq.offset(empty.lowest_set_bit());
                            set_ctrl(idx, stablehash_detail::H2(h));
                            nodes_[idx] = node;
                            ++size_;
                            break;
                        }
                        seq.next();
                    }
                }
            }
            stablehash_detail::aligned_free(old_nodes);
            stablehash_detail::aligned_free(old_ctrl);
        }
    }

    void maybe_rehash()
    {
        if (size_ + tombstones_ >= growth_threshold_)
        {
            size_t new_cap = capacity_;
            if (tombstones_ > size_ / 4)
            {
                // Just clear tombstones by rehashing to same size
            }
            else
            {
                new_cap = std::max(capacity_ * 2, kMinCapacity);
            }
            rehash_internal(new_cap);
        }
    }

public:
    StableHashMap() = default;

    explicit StableHashMap(size_t initial_capacity)
    {
        if (initial_capacity > 0)
        {
            size_t cap = kMinCapacity;
            while (cap < initial_capacity)
            {
                cap *= 2;
            }
            allocate(cap);
        }
    }

    // Constructor with capacity and max load factor
    StableHashMap(size_t initial_capacity, float load_factor)
        : max_load_factor_(load_factor)
    {
        if (initial_capacity > 0)
        {
            size_t cap = kMinCapacity;
            while (cap < initial_capacity)
            {
                cap *= 2;
            }
            allocate(cap);
        }
    }

    // Constructor with capacity, load factor, and custom hash
    StableHashMap(size_t initial_capacity, float load_factor, const Hash& hash)
        : max_load_factor_(load_factor)
        , hasher_(hash)
    {
        if (initial_capacity > 0)
        {
            size_t cap = kMinCapacity;
            while (cap < initial_capacity)
            {
                cap *= 2;
            }
            allocate(cap);
        }
    }

    // Constructor with capacity, load factor, hash, and key_equal
    StableHashMap(size_t initial_capacity, float load_factor, const Hash& hash, const KeyEqual& equal)
        : max_load_factor_(load_factor)
        , hasher_(hash)
        , key_equal_(equal)
    {
        if (initial_capacity > 0)
        {
            size_t cap = kMinCapacity;
            while (cap < initial_capacity)
            {
                cap *= 2;
            }
            allocate(cap);
        }
    }

    ~StableHashMap()
    {
        deallocate();
    }

    // Move noexcept traits - conditional on Hash, KeyEqual, Allocator
    static constexpr bool is_nothrow_move_constructible = std::is_nothrow_move_constructible_v<Hash> &&
                                                          std::is_nothrow_move_constructible_v<KeyEqual> &&
                                                          std::is_nothrow_move_constructible_v<allocator_type>;

    static constexpr bool is_nothrow_move_assignable = std::is_nothrow_move_assignable_v<Hash> &&
                                                       std::is_nothrow_move_assignable_v<KeyEqual> &&
                                                       std::is_nothrow_move_assignable_v<allocator_type>;

    // Move constructor (conditional noexcept based on member types)
    StableHashMap(StableHashMap&& other) noexcept(is_nothrow_move_constructible)
        : ctrl_(other.ctrl_)
        , nodes_(other.nodes_)
        , size_(other.size_)
        , capacity_(other.capacity_)
        , mask_(other.mask_)
        , growth_threshold_(other.growth_threshold_)
        , tombstones_(other.tombstones_)
        , max_load_factor_(other.max_load_factor_)
        , frozen_(other.frozen_)
        , hasher_(std::move(other.hasher_))
        , key_equal_(std::move(other.key_equal_))
        , allocator_(std::move(other.allocator_))
    {
        other.ctrl_ = nullptr;
        other.nodes_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    // Move assignment (conditional noexcept based on member types)
    StableHashMap& operator=(StableHashMap&& other) noexcept(is_nothrow_move_assignable)
    {
        if (this != &other)
        {
            deallocate();
            ctrl_ = other.ctrl_;
            nodes_ = other.nodes_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            mask_ = other.mask_;
            growth_threshold_ = other.growth_threshold_;
            tombstones_ = other.tombstones_;
            max_load_factor_ = other.max_load_factor_;
            frozen_ = other.frozen_;
            hasher_ = std::move(other.hasher_);
            key_equal_ = std::move(other.key_equal_);
            allocator_ = std::move(other.allocator_);

            other.ctrl_ = nullptr;
            other.nodes_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    // Copy constructor - preserves configuration and functor state
    StableHashMap(const StableHashMap& other)
        : max_load_factor_(other.max_load_factor_)
        , frozen_(false)
        , // Copies start unfrozen (intentional)
        hasher_(other.hasher_)
        , key_equal_(other.key_equal_)
    {
        if (other.size_ > 0)
        {
            allocate(other.capacity_);
            for (size_t i = 0; i < other.capacity_; ++i)
            {
                if (stablehash_detail::is_full(other.ctrl_[i]))
                {
                    Node* new_node = allocator_.allocate(other.nodes_[i]->key, other.nodes_[i]->value);
                    size_t h = hash_key(new_node->key);
                    auto [idx, found, insert_slot] = find_or_prepare_insert(new_node->key, h);
                    set_ctrl(insert_slot, stablehash_detail::H2(h));
                    nodes_[insert_slot] = new_node;
                    ++size_;
                }
            }
        }
    }

    // Copy assignment
    StableHashMap& operator=(const StableHashMap& other)
    {
        if (this != &other)
        {
            StableHashMap tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

    // === Core Operations ===

    /// Insert a key-value pair. Returns pointer to value and whether insertion occurred.
    /// The returned pointer remains valid until erase(key) is called.
    template <typename K, typename V>
    std::pair<Value*, bool> insert(K&& key, V&& value)
    {
        assert(!frozen_ && "StableHashMap::insert() called on frozen map");
        if (capacity_ == 0)
        {
            allocate(kMinCapacity);
        }
        maybe_rehash();

        size_t h = hash_key(key);
        auto [idx, found, insert_slot] = find_or_prepare_insert(key, h);

        if (found)
        {
            return {&nodes_[idx]->value, false};
        }

        // Allocate new node using block allocator
        Node* node = allocator_.allocate(std::forward<K>(key), std::forward<V>(value));

        uint8_t old_ctrl = ctrl_[insert_slot];
        set_ctrl(insert_slot, stablehash_detail::H2(h));
        nodes_[insert_slot] = node;

        if (stablehash_detail::is_deleted(old_ctrl))
        {
            --tombstones_;
        }
        ++size_;

        return {&node->value, true};
    }

    /// Insert or assign - insert if key doesn't exist, otherwise assign new value.
    /// Returns pair of (pointer to value, true if inserted / false if assigned).
    template <typename K, typename V>
    std::pair<Value*, bool> insert_or_assign(K&& key, V&& value)
    {
        assert(!frozen_ && "StableHashMap::insert_or_assign() called on frozen map");
        if (capacity_ == 0)
        {
            allocate(kMinCapacity);
        }
        maybe_rehash();

        size_t h = hash_key(key);
        auto [idx, found, insert_slot] = find_or_prepare_insert(key, h);

        if (found)
        {
            // Key exists - assign new value
            nodes_[idx]->value = std::forward<V>(value);
            return {&nodes_[idx]->value, false};
        }

        // Key doesn't exist - insert
        Node* node = allocator_.allocate(std::forward<K>(key), std::forward<V>(value));

        uint8_t old_ctrl = ctrl_[insert_slot];
        set_ctrl(insert_slot, stablehash_detail::H2(h));
        nodes_[insert_slot] = node;

        if (stablehash_detail::is_deleted(old_ctrl))
        {
            --tombstones_;
        }
        ++size_;

        return {&node->value, true};
    }

    /// Try emplace - insert if key doesn't exist, constructing value in-place.
    /// Returns pair of (pointer to value, true if inserted / false if already present).
    template <typename K, typename... Args>
    std::pair<Value*, bool> try_emplace(K&& key, Args&&... args)
    {
        assert(!frozen_ && "StableHashMap::try_emplace() called on frozen map");
        if (capacity_ == 0)
        {
            allocate(kMinCapacity);
        }
        maybe_rehash();

        size_t h = hash_key(key);
        auto [idx, found, insert_slot] = find_or_prepare_insert(key, h);

        if (found)
        {
            // Key already exists - don't construct value
            return {&nodes_[idx]->value, false};
        }

        // Key doesn't exist - construct value in-place
        Node* node = allocator_.allocate(std::forward<K>(key), Value(std::forward<Args>(args)...));

        uint8_t old_ctrl = ctrl_[insert_slot];
        set_ctrl(insert_slot, stablehash_detail::H2(h));
        nodes_[insert_slot] = node;

        if (stablehash_detail::is_deleted(old_ctrl))
        {
            --tombstones_;
        }
        ++size_;

        return {&node->value, true};
    }

    /// Subscript operator - returns reference to value, inserting default if not present.
    /// WARNING: Invalidates existing pointers if rehash occurs!
    Value& operator[](const Key& key)
    {
        auto [ptr, inserted] = try_emplace(key);
        return *ptr;
    }

    Value& operator[](Key&& key)
    {
        auto [ptr, inserted] = try_emplace(std::move(key));
        return *ptr;
    }

    /// Find a key. Returns pointer to value or nullptr.
    /// SFINAE-gated to prevent hard errors with incompatible key types.
    template <typename K, std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    Value* find(const K& key)
    {
        size_t h = hash_key(key);
        auto [idx, found] = find_slot(key, h);
        return found ? &nodes_[idx]->value : nullptr;
    }

    template <typename K, std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    const Value* find(const K& key) const
    {
        size_t h = hash_key(key);
        auto [idx, found] = find_slot(key, h);
        return found ? &nodes_[idx]->value : nullptr;
    }

    template <typename K, std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    bool contains(const K& key) const
    {
        size_t h = hash_key(key);
        auto [idx, found] = find_slot(key, h);
        return found;
    }

    template <typename K, std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    size_t count(const K& key) const
    {
        return contains(key) ? 1 : 0;
    }

    /// Erase a key. After this call, any pointers to the erased value are INVALID.
    template <typename K, std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    bool erase(const K& key)
    {
        assert(!frozen_ && "StableHashMap::erase() called on frozen map");
        size_t h = hash_key(key);
        auto [idx, found] = find_slot(key, h);

        if (found)
        {
            allocator_.deallocate(nodes_[idx]);
            nodes_[idx] = nullptr;
            set_ctrl(idx, stablehash_detail::kDeleted);
            --size_;
            ++tombstones_;
            return true;
        }
        return false;
    }

    void clear()
    {
        assert(!frozen_ && "StableHashMap::clear() called on frozen map");
        if (ctrl_)
        {
            for (size_t i = 0; i < capacity_; ++i)
            {
                if (stablehash_detail::is_full(ctrl_[i]))
                {
                    allocator_.deallocate(nodes_[i]);
                    nodes_[i] = nullptr;
                }
            }
            std::memset(ctrl_, stablehash_detail::kEmpty, capacity_ + Group::kWidth);
            ctrl_[capacity_] = stablehash_detail::kSentinel;
        }
        size_ = 0;
        tombstones_ = 0;
    }

    void reserve(size_t count)
    {
        size_t required = static_cast<size_t>(count / max_load_factor_) + 1;
        if (required > capacity_)
        {
            size_t new_cap = kMinCapacity;
            while (new_cap < required)
            {
                new_cap *= 2;
            }
            rehash_internal(new_cap);
        }
    }

    // === Accessors ===

    size_t size() const
    {
        return size_;
    }
    bool empty() const
    {
        return size_ == 0;
    }
    size_t capacity() const
    {
        return capacity_;
    }
    float load_factor() const
    {
        return capacity_ > 0 ? static_cast<float>(size_) / capacity_ : 0.0f;
    }

    /// Freeze the map for read-only access.
    StableHashMap& freeze() noexcept
    {
        frozen_ = true;
        return *this;
    }
    bool is_frozen() const noexcept
    {
        return frozen_;
    }

    /// Get the allocator (for custom allocator inspection)
    allocator_type& get_allocator() noexcept
    {
        return allocator_;
    }
    const allocator_type& get_allocator() const noexcept
    {
        return allocator_;
    }

    // === Iterator ===

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        iterator()
            : ctrl_(nullptr)
            , nodes_(nullptr)
            , idx_(0)
            , cap_(0)
        {
        }
        iterator(const uint8_t* ctrl, Node** nodes, size_t idx, size_t cap)
            : ctrl_(ctrl)
            , nodes_(nodes)
            , idx_(idx)
            , cap_(cap)
        {
            skip_empty();
        }

        value_type operator*() const
        {
            return {nodes_[idx_]->key, nodes_[idx_]->value};
        }

        const Key& key() const
        {
            return nodes_[idx_]->key;
        }
        Value& value() const
        {
            return nodes_[idx_]->value;
        }

        iterator& operator++()
        {
            ++idx_;
            skip_empty();
            return *this;
        }
        iterator operator++(int)
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator& o) const
        {
            return idx_ == o.idx_;
        }
        bool operator!=(const iterator& o) const
        {
            return idx_ != o.idx_;
        }

    private:
        const uint8_t* ctrl_;
        Node** nodes_;
        size_t idx_;
        size_t cap_;

        void skip_empty()
        {
            while (idx_ < cap_ && !stablehash_detail::is_full(ctrl_[idx_]))
            {
                ++idx_;
            }
        }
    };

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, const Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        const_iterator()
            : ctrl_(nullptr)
            , nodes_(nullptr)
            , idx_(0)
            , cap_(0)
        {
        }
        const_iterator(const uint8_t* ctrl, Node* const* nodes, size_t idx, size_t cap)
            : ctrl_(ctrl)
            , nodes_(nodes)
            , idx_(idx)
            , cap_(cap)
        {
            skip_empty();
        }
        const_iterator(const iterator& it)
            : ctrl_(it.ctrl_)
            , nodes_(it.nodes_)
            , idx_(it.idx_)
            , cap_(it.cap_)
        {
        }

        value_type operator*() const
        {
            return {nodes_[idx_]->key, nodes_[idx_]->value};
        }

        const Key& key() const
        {
            return nodes_[idx_]->key;
        }
        const Value& value() const
        {
            return nodes_[idx_]->value;
        }

        const_iterator& operator++()
        {
            ++idx_;
            skip_empty();
            return *this;
        }
        const_iterator operator++(int)
        {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const const_iterator& o) const
        {
            return idx_ == o.idx_;
        }
        bool operator!=(const const_iterator& o) const
        {
            return idx_ != o.idx_;
        }

    private:
        const uint8_t* ctrl_;
        Node* const* nodes_;
        size_t idx_;
        size_t cap_;

        void skip_empty()
        {
            while (idx_ < cap_ && !stablehash_detail::is_full(ctrl_[idx_]))
            {
                ++idx_;
            }
        }
    };

    iterator begin()
    {
        return iterator(ctrl_, nodes_, 0, capacity_);
    }
    iterator end()
    {
        return iterator(ctrl_, nodes_, capacity_, capacity_);
    }
    const_iterator begin() const
    {
        return const_iterator(ctrl_, nodes_, 0, capacity_);
    }
    const_iterator end() const
    {
        return const_iterator(ctrl_, nodes_, capacity_, capacity_);
    }
    const_iterator cbegin() const
    {
        return begin();
    }
    const_iterator cend() const
    {
        return end();
    }

    // === Comparison Operators ===

    friend bool operator==(const StableHashMap& lhs, const StableHashMap& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return false;
        }
        for (auto it = lhs.begin(); it != lhs.end(); ++it)
        {
            auto [key, value] = *it;
            const Value* rhs_val = rhs.find(key);
            if (!rhs_val || *rhs_val != value)
            {
                return false;
            }
        }
        return true;
    }

    friend bool operator!=(const StableHashMap& lhs, const StableHashMap& rhs)
    {
        return !(lhs == rhs);
    }
};

} // namespace fat_p
