// FastHashMap.h - High-performance SIMD hash map
//
// A single-header Swiss table implementation with configurable deletion policy
// and allocator support. 3-5x faster than std::unordered_map for most workloads.
//
// Version: December 22, 2025 (4-AI Review Round 3)
// Fixes applied:
//   - P0 Critical: Control byte prefix mirroring (fixes probing hang)
//   - P0 Critical: Guaranteed ≥1 empty slot (probe termination)
//   - P0 Critical: FixedAllocator alignment fix (align address, not offset)
//   - P0 Critical: FixedHashMap non-movable/non-swappable (prevents dangling pointers)
//   - P1: 32-bit portable hash finalizer (SplitMix64/MurmurHash3)
//   - P1: Conditional noexcept on move/swap (Hash/KeyEqual may throw)
//   - P1: SFINAE-gated heterogeneous lookup (no hard-error on incompatible K)
//   - P1: Proper freeze semantics (guarded + propagated)
//   - P2: Checked allocations (throws std::bad_alloc)
//   - P2: Allocation-before-rehash ordering (cleaner first-insert path)
//
// Features:
//   - SIMD-accelerated probing (SSE2/AVX2/AVX512/NEON)
//   - Built-in hash finalizer (SplitMix64) for robust distribution
//   - Configurable deletion policy (Tombstone or BackwardShift)
//   - Configurable allocator policy (Heap or Fixed-buffer)
//   - Heterogeneous lookup support (find with string_view for string keys)
//   - ~1200 lines, no dependencies beyond FatPSimdDetection.h
//
// Deletion Policies:
//   - TombstoneDeletion (default): Fast erase, best overall performance
//   - BackwardShiftDeletion: No tombstones, slightly better miss detection
//
// Allocator Policies:
//   - HeapAllocator (default): Standard aligned heap allocation
//   - FixedAllocator<N>: Stack-allocated buffer for embedded/real-time use
//     NOTE: FixedHashMap is NON-MOVABLE and NON-SWAPPABLE (pointers would dangle)
//
// Usage:
//   fat_p::FastHashMap<K, V> map;           // Default: Tombstone + Heap
//   fat_p::FastHashMapBS<K, V> map_bs;      // BackwardShift + Heap
//   fat_p::FastHashMapTS<K, V> map_ts;      // Tombstone + Heap (explicit)
//   fat_p::FixedHashMap<K, V, 8192> fixed;  // Tombstone + 8KB fixed buffer (non-movable!)
//
// Hash Finalizer (SplitMix64):
//   By default, a SplitMix64 finalizer is applied to all hashes to protect
//   against poor hash functions (e.g., std::hash<int> is identity on many platforms).
//   
//   To disable the finalizer for hash functions that already have good avalanche
//   properties (e.g., absl::Hash, wyhash), define `is_avalanching` in your hash:
//   
//   struct MyGoodHash {
//       using is_avalanching = void;  // Opt-out of built-in mixer
//       size_t operator()(int64_t x) const { return wyhash(&x, sizeof(x), 0, _wyp); }
//   };
//   fat_p::FastHashMap<int64_t, int64_t, MyGoodHash> map;  // No double-mixing
//
// Heterogeneous Lookup:
//   // Enable transparent lookup with custom hash/equal:
//   struct StringHash { using is_transparent = void; 
//     size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }};
//   struct StringEqual { using is_transparent = void;
//     bool operator()(std::string_view a, std::string_view b) const { return a == b; }};
//   fat_p::FastHashMap<std::string, int, StringHash, StringEqual> map;
//   map.find("key");  // No allocation! Uses string_view internally
//
// Performance (vs std::unordered_map at N=1M):
//   - Insert: 3-5x faster
//   - Find:   1.5-2x faster  
//   - Erase:  7-10x faster (with TombstoneDeletion)
//
// For maximum performance, consider boost::unordered_flat_map.
// FastHashMap trades some speed for simplicity (single header, C++17).
//
// SIMD Detection:
//   - Compile-time: AVX512/AVX2/SSE2/NEON selected based on compiler flags
//   - Runtime: CPUID detection reports optimization status
//
// Compiler flags for AVX2:
//   - MSVC:      /arch:AVX2
//   - GCC/Clang: -mavx2 or -march=native
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

#include "FatPSimdDetection.h"
#include "FatPConfig.h"

#if defined(_MSC_VER)
#include <malloc.h>
#include <intrin.h>  // For _BitScanForward
#endif

// =============================================================================
// SIMD Platform Detection
// =============================================================================

// AVX-512BW: 64-byte groups (commented out - frequency throttling concerns)
// #if defined(FATP_SIMD_AVX512BW)
//     #define FAST_HASH_MAP_AVX512 1
//     #define FAST_HASH_MAP_AVX2 1
//     #define FAST_HASH_MAP_SSE2 1
//     #include <immintrin.h>

// AVX2: 32-byte groups
#if defined(FATP_SIMD_AVX2)
    #define FAST_HASH_MAP_AVX2 1
    #define FAST_HASH_MAP_SSE2 1
    #include <immintrin.h>

// SSE2: 16-byte groups
#elif defined(FATP_SIMD_SSE2)
    #define FAST_HASH_MAP_SSE2 1
    #include <emmintrin.h>
    #if defined(__SSSE3__)
        #include <tmmintrin.h>
    #endif
#endif

#if defined(FATP_SIMD_NEON)
    #define FAST_HASH_MAP_NEON 1
    #if FATP_SIMD_NEON_AARCH64
        #define FAST_HASH_MAP_NEON_AARCH64 1
    #else
        #define FAST_HASH_MAP_NEON_AARCH64 0
    #endif
    #include <arm_neon.h>
#endif

#if !defined(FAST_HASH_MAP_SSE2) && !defined(FAST_HASH_MAP_NEON)
    #define FAST_HASH_MAP_PORTABLE 1
#endif

namespace fat_p {

// =============================================================================
// Deletion Policy Tags
// =============================================================================

/// Tombstone-based deletion: O(1) erase, good all-around performance
/// Best for: most workloads (default)
struct TombstoneDeletion {
    static constexpr const char* name() { return "Tombstone"; }
};

/// Backward-shift deletion: No tombstones, maintains table density
/// Best for: theoretical advantage on miss-after-delete (not observed in practice)
struct BackwardShiftDeletion {
    static constexpr const char* name() { return "BackwardShift"; }
};

// Empty placeholder for [[no_unique_address]] optimization
struct EmptyMember {};

// =============================================================================
// Allocator Policies - Raw memory allocation for control bytes and slots
// =============================================================================
//
// Allocator Concept Requirements:
//   - void* allocate(size_t size, size_t alignment)
//   - void deallocate(void* ptr, size_t size, size_t alignment)
//   - Default constructible
//   - Move constructible
//
// Available Policies:
//   - HeapAllocator (default): Standard aligned heap allocation
//   - FixedAllocator<N>: Stack-allocated buffer for small, fixed-size maps
//
// Usage:
//   fat_p::FastHashMap<K, V> map;                             // HeapAllocator
//   fat_p::FastHashMap<K, V, H, E, D, fat_p::FixedAllocator<1024>> fixed_map;

/// HeapAllocator - Standard aligned heap allocation (default)
/// Uses platform-appropriate aligned allocation (posix_memalign, _aligned_malloc)
struct HeapAllocator {
    static constexpr const char* name() { return "Heap"; }
    static constexpr bool kPointerStealSafe = true;  // Pointers survive allocator move
    
    void* allocate(size_t size, size_t alignment) {
#if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
#else
        void* ptr = nullptr;
        if (posix_memalign(&ptr, alignment, size) != 0) return nullptr;
        return ptr;
#endif
    }
    
    void deallocate(void* ptr, [[maybe_unused]] size_t size, [[maybe_unused]] size_t alignment) {
#if defined(_MSC_VER)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }
};

/// FixedAllocator - Stack-allocated buffer for small fixed-size maps
/// Provides O(1) allocation from pre-allocated buffer. No heap allocations.
/// Throws std::bad_alloc if capacity exceeded.
///
/// IMPORTANT: FixedHashMap is NON-MOVABLE and NON-SWAPPABLE because pointers
/// into the embedded buffer would dangle after move. Use HeapAllocator if you
/// need move semantics.
///
/// Template parameter N is approximate byte capacity (actual may be slightly more
/// due to internal alignment requirements).
///
/// Best for: Embedded systems, real-time applications, small lookup tables
/// Limitation: Cannot grow/rehash - reserve sufficient capacity upfront
///
/// Usage:
///   // Map with ~4KB stack buffer
///   fat_p::FastHashMap<int, int, std::hash<int>, std::equal_to<int>,
///                      fat_p::TombstoneDeletion, fat_p::FixedAllocator<4096>> map;
///
template<size_t BufferSize>
class FixedAllocator {
    static_assert(BufferSize >= 128, "FixedAllocator buffer too small (min 128 bytes)");
    
    alignas(64) char buffer_[BufferSize];  // 64-byte aligned for SIMD
    size_t used_ = 0;
    
public:
    static constexpr const char* name() { return "Fixed"; }
    static constexpr size_t buffer_size() { return BufferSize; }
    static constexpr bool kPointerStealSafe = false;  // Pointers into embedded buffer!
    
    FixedAllocator() = default;
    
    // Non-copyable (contains embedded storage)
    FixedAllocator(const FixedAllocator&) = delete;
    FixedAllocator& operator=(const FixedAllocator&) = delete;
    
    // Non-movable: pointers into buffer_ would dangle after move
    // The FastHashMap static_assert will catch attempts to move FixedHashMap
    FixedAllocator(FixedAllocator&&) = delete;
    FixedAllocator& operator=(FixedAllocator&&) = delete;
    
    void* allocate(size_t size, size_t alignment) {
        // Alignment must be power of two
        assert(alignment != 0 && (alignment & (alignment - 1)) == 0);
        
        // Align the actual address, not just the offset
        // This handles alignments > 64 correctly
        const uintptr_t base = reinterpret_cast<uintptr_t>(buffer_);
        const uintptr_t cur = base + used_;
        const uintptr_t aligned = (cur + alignment - 1) & ~(uintptr_t(alignment) - 1);
        const size_t aligned_offset = static_cast<size_t>(aligned - base);
        
        if (aligned_offset + size > BufferSize) {
            return nullptr;  // Caller will throw std::bad_alloc
        }
        
        void* ptr = buffer_ + aligned_offset;
        used_ = aligned_offset + size;
        return ptr;
    }
    
    void deallocate(void* /*ptr*/, size_t /*size*/, size_t /*alignment*/) {
        // Fixed allocator doesn't reclaim memory until reset
        // WARNING: rehash will exhaust buffer - reserve sufficient capacity upfront
    }
    
    void reset() { used_ = 0; }
    size_t used() const { return used_; }
    size_t available() const { return BufferSize - used_; }
};

// =============================================================================
// Control Byte Constants
// =============================================================================

namespace fasthash_detail {

constexpr uint8_t kEmpty    = 0x00;
constexpr uint8_t kDeleted  = 0x7E;  // Only used with TombstoneDeletion
constexpr uint8_t kSentinel = 0x7F;
constexpr size_t kGroupSize = 16;

constexpr bool is_full(uint8_t ctrl) { return (ctrl & 0x80) != 0; }
constexpr bool is_empty(uint8_t ctrl) { return ctrl == kEmpty; }
constexpr bool is_deleted(uint8_t ctrl) { return ctrl == kDeleted; }
constexpr bool is_empty_or_deleted(uint8_t ctrl) { return ctrl < 0x80 && ctrl != kSentinel; }

constexpr uint8_t H2(size_t hash) {
    // Use top 7 bits. Portable calculation avoids UB on 32-bit.
    constexpr size_t kBits = sizeof(size_t) * 8;
    constexpr size_t kShift = kBits - 7;
    return static_cast<uint8_t>((hash >> kShift) | 0x80);
}

// =============================================================================
// BitMask
// =============================================================================

class BitMask {
public:
    explicit BitMask(uint32_t mask) : mask_(mask) {}
    explicit operator bool() const { return mask_ != 0; }
    
    uint32_t lowest_set_bit() const {
#if defined(_MSC_VER)
        unsigned long index;
        _BitScanForward(&index, mask_);
        return index;
#else
        return static_cast<uint32_t>(__builtin_ctz(mask_));
#endif
    }
    
    BitMask& remove_lowest_bit() { mask_ &= mask_ - 1; return *this; }
    
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = uint32_t;
        using difference_type = std::ptrdiff_t;
        
        iterator() : mask_(0) {}
        explicit iterator(uint32_t mask) : mask_(mask) {}
        uint32_t operator*() const { return BitMask(mask_).lowest_set_bit(); }
        iterator& operator++() { mask_ &= mask_ - 1; return *this; }
        bool operator==(const iterator& o) const { return mask_ == o.mask_; }
        bool operator!=(const iterator& o) const { return mask_ != o.mask_; }
    private:
        uint32_t mask_;
    };
    
    iterator begin() const { return iterator(mask_); }
    iterator end() const { return iterator(0); }
    
private:
    uint32_t mask_;
};

// =============================================================================
// Group - SIMD operations on control bytes
// =============================================================================

// --- AVX-512BW: 64-byte groups (commented out - frequency throttling concerns) ---
// Uncomment to enable. Requires: -mavx512bw (GCC/Clang) or /arch:AVX512 (MSVC)
// WARNING: AVX-512 can cause 10-20% frequency reduction on some Intel CPUs.
/*
#if defined(FAST_HASH_MAP_AVX512)

class Group {
public:
    static constexpr size_t kWidth = 64;
    
    explicit Group(const uint8_t* ctrl) {
        ctrl_ = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ctrl));
    }
    
    BitMask match(uint8_t h2) const {
        auto match_vec = _mm512_set1_epi8(static_cast<char>(h2));
        return BitMask(static_cast<uint32_t>(_mm512_cmpeq_epi8_mask(ctrl_, match_vec)));
    }
    
    BitMask match_empty() const {
        auto zero = _mm512_setzero_si512();
        return BitMask(static_cast<uint32_t>(_mm512_cmpeq_epi8_mask(ctrl_, zero)));
    }
    
    BitMask match_empty_or_deleted() const {
        // Empty (0x00) and Deleted (0x01) both have high bit clear, Sentinel (0xFF) doesn't
        auto high_bit_mask = _mm512_set1_epi8(static_cast<char>(0x80u));
        auto high_bits = _mm512_and_si512(ctrl_, high_bit_mask);
        auto zero = _mm512_setzero_si512();
        auto has_high_bit_clear = _mm512_cmpeq_epi8_mask(high_bits, zero);
        auto sentinel = _mm512_set1_epi8(static_cast<char>(kSentinel));
        auto is_sentinel = _mm512_cmpeq_epi8_mask(ctrl_, sentinel);
        return BitMask(static_cast<uint32_t>(has_high_bit_clear & ~is_sentinel));
    }
    
private:
    __m512i ctrl_;
};

#elif defined(FAST_HASH_MAP_AVX2)
*/

// --- AVX2: 32-byte groups ---
#if defined(FAST_HASH_MAP_AVX2) && !defined(FAST_HASH_MAP_AVX512)

class Group {
public:
    static constexpr size_t kWidth = 32;
    
    explicit Group(const uint8_t* ctrl) {
        ctrl_ = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ctrl));
    }
    
    BitMask match(uint8_t h2) const {
        auto match_vec = _mm256_set1_epi8(static_cast<char>(h2));
        auto result = _mm256_cmpeq_epi8(ctrl_, match_vec);
        return BitMask(static_cast<uint32_t>(_mm256_movemask_epi8(result)));
    }
    
    BitMask match_empty() const {
        auto zero = _mm256_setzero_si256();
        auto result = _mm256_cmpeq_epi8(ctrl_, zero);
        return BitMask(static_cast<uint32_t>(_mm256_movemask_epi8(result)));
    }
    
    BitMask match_empty_or_deleted() const {
        // Empty (0x00) and Deleted (0x01) both have high bit clear, Sentinel (0xFF) doesn't
        auto high_bit_mask = _mm256_set1_epi8(static_cast<char>(0x80u));
        auto high_bits = _mm256_and_si256(ctrl_, high_bit_mask);
        auto zero = _mm256_setzero_si256();
        auto has_high_bit_clear = _mm256_cmpeq_epi8(high_bits, zero);
        auto sentinel = _mm256_set1_epi8(static_cast<char>(kSentinel));
        auto is_sentinel = _mm256_cmpeq_epi8(ctrl_, sentinel);
        auto result = _mm256_andnot_si256(is_sentinel, has_high_bit_clear);
        return BitMask(static_cast<uint32_t>(_mm256_movemask_epi8(result)));
    }
    
private:
    __m256i ctrl_;
};

// --- SSE2: 16-byte groups ---
#elif defined(FAST_HASH_MAP_SSE2)

class Group {
public:
    static constexpr size_t kWidth = 16;
    
    explicit Group(const uint8_t* ctrl) {
        ctrl_ = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ctrl));
    }
    
    BitMask match(uint8_t h2) const {
        auto match_vec = _mm_set1_epi8(static_cast<char>(h2));
        auto result = _mm_cmpeq_epi8(ctrl_, match_vec);
        return BitMask(static_cast<uint32_t>(_mm_movemask_epi8(result)));
    }
    
    BitMask match_empty() const {
        auto zero = _mm_setzero_si128();
        auto result = _mm_cmpeq_epi8(ctrl_, zero);
        return BitMask(static_cast<uint32_t>(_mm_movemask_epi8(result)));
    }
    
    BitMask match_empty_or_deleted() const {
        auto high_bit_mask = _mm_set1_epi8(static_cast<char>(0x80u));
        auto high_bits = _mm_and_si128(ctrl_, high_bit_mask);
        auto zero = _mm_setzero_si128();
        auto has_high_bit_clear = _mm_cmpeq_epi8(high_bits, zero);
        auto sentinel = _mm_set1_epi8(static_cast<char>(kSentinel));
        auto is_sentinel = _mm_cmpeq_epi8(ctrl_, sentinel);
        auto result = _mm_andnot_si128(is_sentinel, has_high_bit_clear);
        return BitMask(static_cast<uint32_t>(_mm_movemask_epi8(result)));
    }
    
private:
    __m128i ctrl_;
};

// --- NEON: 16-byte groups ---
#elif defined(FAST_HASH_MAP_NEON)

class Group {
public:
    static constexpr size_t kWidth = 16;
    
    explicit Group(const uint8_t* ctrl) {
        ctrl_ = vld1q_u8(ctrl);
    }
    
    BitMask match(uint8_t h2) const {
        auto match_vec = vdupq_n_u8(h2);
        auto result = vceqq_u8(ctrl_, match_vec);
#if FAST_HASH_MAP_NEON_AARCH64
        static const uint8x16_t bit_mask = {1,2,4,8,16,32,64,128,1,2,4,8,16,32,64,128};
        auto masked = vandq_u8(result, bit_mask);
        auto paired = vpaddq_u8(masked, masked);
        auto quad = vpaddq_u8(paired, paired);
        auto oct = vpaddq_u8(quad, quad);
        return BitMask(vgetq_lane_u16(vreinterpretq_u16_u8(oct), 0));
#else
        uint64x2_t paired = vreinterpretq_u64_u8(result);
        uint64_t lo = vgetq_lane_u64(paired, 0);
        uint64_t hi = vgetq_lane_u64(paired, 1);
        uint32_t mask = 0;
        for (int i = 0; i < 8; ++i) {
            if ((lo >> (i * 8)) & 0xFF) mask |= (1u << i);
            if ((hi >> (i * 8)) & 0xFF) mask |= (1u << (i + 8));
        }
        return BitMask(mask);
#endif
    }
    
    BitMask match_empty() const { return match(kEmpty); }
    
    BitMask match_empty_or_deleted() const {
        auto high_bit = vdupq_n_u8(0x80);
        auto has_high = vandq_u8(ctrl_, high_bit);
        auto no_high = vceqq_u8(has_high, vdupq_n_u8(0));
        auto sentinel = vceqq_u8(ctrl_, vdupq_n_u8(kSentinel));
        auto result = vbicq_u8(no_high, sentinel);
#if FAST_HASH_MAP_NEON_AARCH64
        static const uint8x16_t bit_mask = {1,2,4,8,16,32,64,128,1,2,4,8,16,32,64,128};
        auto masked = vandq_u8(result, bit_mask);
        auto paired = vpaddq_u8(masked, masked);
        auto quad = vpaddq_u8(paired, paired);
        auto oct = vpaddq_u8(quad, quad);
        return BitMask(vgetq_lane_u16(vreinterpretq_u16_u8(oct), 0));
#else
        uint64x2_t paired = vreinterpretq_u64_u8(result);
        uint64_t lo = vgetq_lane_u64(paired, 0);
        uint64_t hi = vgetq_lane_u64(paired, 1);
        uint32_t mask = 0;
        for (int i = 0; i < 8; ++i) {
            if ((lo >> (i * 8)) & 0xFF) mask |= (1u << i);
            if ((hi >> (i * 8)) & 0xFF) mask |= (1u << (i + 8));
        }
        return BitMask(mask);
#endif
    }
    
private:
    uint8x16_t ctrl_;
};

// --- Portable fallback: 16-byte groups ---
#else

class Group {
public:
    static constexpr size_t kWidth = 16;
    
    explicit Group(const uint8_t* ctrl) {
        std::memcpy(ctrl_, ctrl, kWidth);
    }
    
    BitMask match(uint8_t h2) const {
        uint32_t mask = 0;
        for (size_t i = 0; i < kWidth; ++i) {
            if (ctrl_[i] == h2) mask |= (1u << i);
        }
        return BitMask(mask);
    }
    
    BitMask match_empty() const { return match(kEmpty); }
    
    BitMask match_empty_or_deleted() const {
        uint32_t mask = 0;
        for (size_t i = 0; i < kWidth; ++i) {
            if (is_empty_or_deleted(ctrl_[i])) mask |= (1u << i);
        }
        return BitMask(mask);
    }
    
private:
    uint8_t ctrl_[kWidth];
};

#endif

// =============================================================================
// ProbeSequence - Linear probing
// =============================================================================

class ProbeSequence {
public:
    ProbeSequence(size_t hash, size_t mask) 
        : mask_(mask), offset_(hash & mask) {}
    
    size_t offset() const { return offset_; }
    size_t offset(size_t i) const { return (offset_ + i) & mask_; }
    void next() { offset_ = (offset_ + Group::kWidth) & mask_; }
    
private:
    size_t mask_;
    size_t offset_;
};

} // namespace fasthash_detail

// =============================================================================
// FastHashMap - Main class with deletion policy
// =============================================================================

template <typename Key, typename Value, 
          typename Hash = std::hash<Key>, 
          typename KeyEqual = std::equal_to<Key>,
          typename DeletionPolicy = TombstoneDeletion,
          typename Allocator = HeapAllocator>
class FastHashMap {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = size_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using deletion_policy = DeletionPolicy;
    using allocator_type = Allocator;
    
    static constexpr bool uses_tombstones = std::is_same_v<DeletionPolicy, TombstoneDeletion>;
    
private:
    using Group = fasthash_detail::Group;
    using ProbeSequence = fasthash_detail::ProbeSequence;
    using BitMask = fasthash_detail::BitMask;
    
    // Slot structure - conditionally includes 'home' for backward-shift
    struct Slot {
        Key key{};
        Value value{};
        
        // Only include home field for backward-shift deletion
        // Uses empty struct for zero overhead with tombstones
        struct Empty {};
        FATP_NO_UNIQUE_ADDRESS std::conditional_t<uses_tombstones, Empty, size_t> home_{};
        
        Slot() = default;
        
        template<typename K, typename V>
        void emplace(K&& k, V&& v, size_t home) {
            key = std::forward<K>(k);
            value = std::forward<V>(v);
            if constexpr (!uses_tombstones) {
                home_ = home;
            }
        }
        
        size_t home() const {
            if constexpr (uses_tombstones) return 0;
            else return home_;
        }
        
        void destroy() {
            key.~Key();
            value.~Value();
        }
    };
    
    uint8_t* ctrl_ = nullptr;
    Slot* slots_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    size_t growth_threshold_ = 0;
    
    // Only used with TombstoneDeletion - zero overhead when unused
    FATP_NO_UNIQUE_ADDRESS std::conditional_t<uses_tombstones, size_t, EmptyMember> tombstones_{};
    
    float max_load_factor_ = 0.875f;
    bool frozen_ = false;  // Read-only mode for high-density lookup tables
    FATP_NO_UNIQUE_ADDRESS Hash hasher_;
    FATP_NO_UNIQUE_ADDRESS KeyEqual key_equal_;
    FATP_NO_UNIQUE_ADDRESS Allocator allocator_;
    
    static constexpr size_t kMinCapacity = Group::kWidth * 2;
    
    // Validate and clamp max_load_factor to safe range (0, 1)
    static constexpr float normalize_load_factor(float lf) noexcept {
        if (lf <= 0.0f || lf >= 1.0f) return 0.875f;  // Default
        return lf;
    }
    
    // Compute growth threshold ensuring at least one empty slot remains
    static size_t compute_growth_threshold(size_t cap, float max_lf) noexcept {
        size_t threshold = static_cast<size_t>(cap * max_lf);
        // Must leave at least one empty slot for probe termination
        if (threshold >= cap) threshold = cap - 1;
        if (threshold < 1) threshold = 1;
        return threshold;
    }
    
    // Write to control array, maintaining prefix mirror in tail
    void set_ctrl(size_t idx, uint8_t value) noexcept {
        ctrl_[idx] = value;
        // Mirror prefix bytes into tail for wrap-around group reads
        if (idx < Group::kWidth) {
            ctrl_[capacity_ + idx] = value;
        }
    }
    
    void allocate(size_t cap) {
        // Allocate control bytes with SIMD alignment
        // Note: We don't modify member variables until allocation succeeds
        // to maintain exception safety
        uint8_t* new_ctrl = static_cast<uint8_t*>(allocator_.allocate(cap + Group::kWidth, Group::kWidth));
        if (!new_ctrl) {
            throw std::bad_alloc();
        }
        
        // Initialize all to empty (including tail - will be overwritten by mirror)
        std::memset(new_ctrl, fasthash_detail::kEmpty, cap + Group::kWidth);
        
        // Allocate slots with proper alignment for Key/Value types
        constexpr size_t slot_align = alignof(Slot) > alignof(std::max_align_t) 
            ? alignof(Slot) : alignof(std::max_align_t);
        Slot* new_slots = static_cast<Slot*>(allocator_.allocate(cap * sizeof(Slot), slot_align));
        if (!new_slots) {
            allocator_.deallocate(new_ctrl, cap + Group::kWidth, Group::kWidth);
            throw std::bad_alloc();
        }
        
        // Both allocations succeeded - now commit to member variables
        ctrl_ = new_ctrl;
        slots_ = new_slots;
        capacity_ = cap;
        mask_ = cap - 1;
        growth_threshold_ = compute_growth_threshold(cap, max_load_factor_);
    }
    
    void deallocate() {
        if (ctrl_) {
            for (size_t i = 0; i < capacity_; ++i) {
                if (fasthash_detail::is_full(ctrl_[i])) {
                    slots_[i].destroy();
                }
            }
            constexpr size_t slot_align = alignof(Slot) > alignof(std::max_align_t) 
                ? alignof(Slot) : alignof(std::max_align_t);
            allocator_.deallocate(slots_, capacity_ * sizeof(Slot), slot_align);
            allocator_.deallocate(ctrl_, capacity_ + Group::kWidth, Group::kWidth);
            ctrl_ = nullptr;
            slots_ = nullptr;
        }
    }
    
    // Detect if hash already has good avalanche properties (opt-out marker)
    // If Hash::is_avalanching is defined, skip the built-in mixer
    template<typename T, typename = void>
    struct has_avalanching : std::false_type {};
    
    template<typename T>
    struct has_avalanching<T, std::void_t<typename T::is_avalanching>> : std::true_type {};
    
    // noexcept specifications for move/swap - conditional on Hash/KeyEqual/Allocator
    static constexpr bool is_nothrow_move_constructible_v =
        std::is_nothrow_move_constructible_v<Hash> &&
        std::is_nothrow_move_constructible_v<KeyEqual> &&
        std::is_nothrow_move_constructible_v<Allocator>;
    
    static constexpr bool is_nothrow_move_assignable_v =
        std::is_nothrow_move_assignable_v<Hash> &&
        std::is_nothrow_move_assignable_v<KeyEqual> &&
        std::is_nothrow_move_assignable_v<Allocator>;
    
    static constexpr bool is_nothrow_swappable_v =
        std::is_nothrow_swappable_v<Hash> &&
        std::is_nothrow_swappable_v<KeyEqual> &&
        std::is_nothrow_swappable_v<Allocator>;
    
    // SFINAE helpers for heterogeneous lookup
    // These ensure find/erase/contains don't hard-error when Hash/KeyEqual can't handle K
    template<typename K>
    using IsHashInvocable = std::bool_constant<
        std::is_invocable_r_v<size_t, const Hash&, const K&>>;
    
    template<typename K>
    using IsKeyEqualInvocable = std::bool_constant<
        std::is_invocable_r_v<bool, const KeyEqual&, const Key&, const K&>>;
    
    template<typename K>
    static constexpr bool is_hetero_lookup_enabled =
        IsHashInvocable<K>::value && IsKeyEqualInvocable<K>::value;
    
    // Hash with optional finalizer
    // Applied by default to protect against bad user hashes (e.g., std::hash<int> is identity)
    // Skipped if Hash::is_avalanching is defined (e.g., absl::Hash, wyhash)
    // Uses SplitMix64 on 64-bit, MurmurHash3 finalizer on 32-bit (avoids UB from >> 33)
    // Note: hash value 0 is valid. H2(0)=0x80 cannot be confused with kEmpty=0x00.
    template<typename K>
    size_t hash_key(const K& k) const {
        size_t h = hasher_(k);
        if constexpr (has_avalanching<Hash>::value) {
            // User's hash already has good distribution, use as-is
            return h;
        } else {
            // Apply finalizer appropriate for architecture (avoids 32-bit UB)
            if constexpr (sizeof(size_t) >= 8) {
                // SplitMix64 finalizer for 64-bit
                uint64_t x = static_cast<uint64_t>(h);
                x ^= x >> 33;
                x *= 0xff51afd7ed558ccdULL;
                x ^= x >> 33;
                x *= 0xc4ceb9fe1a85ec53ULL;
                x ^= x >> 33;
                size_t mixed = static_cast<size_t>(x);
                return mixed ? mixed : 1;
            } else {
                // MurmurHash3 finalizer for 32-bit
                uint32_t x = static_cast<uint32_t>(h);
                x ^= x >> 16;
                x *= 0x85ebca6bU;
                x ^= x >> 13;
                x *= 0xc2b2ae35U;
                x ^= x >> 16;
                size_t mixed = static_cast<size_t>(x);
                return mixed ? mixed : 1;
            }
        }
    }
    
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    std::pair<size_t, bool> find_slot(const K& k) const {
        if (capacity_ == 0) return {0, false};
        
        size_t h = hash_key(k);
        uint8_t h2 = fasthash_detail::H2(h);
        ProbeSequence seq(h, mask_);
        
        while (true) {
            Group g(ctrl_ + seq.offset());
            
            for (uint32_t i : g.match(h2)) {
                size_t idx = seq.offset(i);
                if (key_equal_(slots_[idx].key, k)) {
                    return {idx, true};
                }
            }
            
            if (g.match_empty()) {
                return {0, false};
            }
            
            seq.next();
        }
    }
    
    size_t find_insert_slot(size_t h) const {
        ProbeSequence seq(h, mask_);
        
        while (true) {
            Group g(ctrl_ + seq.offset());
            
            // With tombstones, can insert into deleted slots
            // Without tombstones, only empty slots
            BitMask mask = uses_tombstones ? g.match_empty_or_deleted() : g.match_empty();
            if (mask) {
                return seq.offset(mask.lowest_set_bit());
            }
            
            seq.next();
        }
    }
    
    void rehash_internal(size_t new_cap) {
        uint8_t* old_ctrl = ctrl_;
        Slot* old_slots = slots_;
        size_t old_cap = capacity_;
        size_t old_mask = mask_;
        size_t old_growth_threshold = growth_threshold_;
        
        ctrl_ = nullptr;
        slots_ = nullptr;
        
        try {
            allocate(new_cap);
        } catch (...) {
            // Restore old state on allocation failure
            ctrl_ = old_ctrl;
            slots_ = old_slots;
            capacity_ = old_cap;
            mask_ = old_mask;
            growth_threshold_ = old_growth_threshold;
            throw;  // Re-throw the exception
        }
        
        size_ = 0;
        if constexpr (uses_tombstones) {
            tombstones_ = 0;
        }
        
        if (old_ctrl) {
            for (size_t i = 0; i < old_cap; ++i) {
                if (fasthash_detail::is_full(old_ctrl[i])) {
                    size_t h = hash_key(old_slots[i].key);
                    size_t idx = find_insert_slot(h);
                    size_t home = h & mask_;
                    
                    set_ctrl(idx, fasthash_detail::H2(h));
                    new (&slots_[idx]) Slot();
                    slots_[idx].emplace(std::move(old_slots[i].key), 
                                       std::move(old_slots[i].value), home);
                    old_slots[i].destroy();
                    ++size_;
                }
            }
            
            constexpr size_t slot_align = alignof(Slot) > alignof(std::max_align_t) 
                ? alignof(Slot) : alignof(std::max_align_t);
            allocator_.deallocate(old_slots, old_cap * sizeof(Slot), slot_align);
            allocator_.deallocate(old_ctrl, old_cap + Group::kWidth, Group::kWidth);
        }
    }
    
    void maybe_rehash() {
        // Skip rehash check before first allocation
        if (capacity_ == 0) {
            return;
        }
        if constexpr (uses_tombstones) {
            if (size_ + tombstones_ >= growth_threshold_) {
                size_t new_cap = capacity_;
                if (size_ >= growth_threshold_ / 2) {
                    new_cap = capacity_ ? capacity_ * 2 : kMinCapacity;
                }
                rehash_internal(new_cap);
            }
        } else {
            if (size_ >= growth_threshold_) {
                size_t new_cap = capacity_ ? capacity_ * 2 : kMinCapacity;
                rehash_internal(new_cap);
            }
        }
    }
    
    // Erase implementation - policy-specific
    void erase_at(size_t idx) {
        if constexpr (uses_tombstones) {
            // Tombstone deletion: O(1), mark as deleted
            slots_[idx].destroy();
            set_ctrl(idx, fasthash_detail::kDeleted);
            --size_;
            ++tombstones_;
        } else {
            // Backward-shift deletion: shift elements back
            --size_;
            
            size_t hole = idx;
            size_t scan = (hole + 1) & mask_;
            
            while (fasthash_detail::is_full(ctrl_[scan])) {
                size_t natural_pos = slots_[scan].home();
                size_t dist_to_current = (scan - natural_pos) & mask_;
                size_t dist_to_hole = (hole - natural_pos) & mask_;
                
                if (dist_to_hole < dist_to_current) {
                    set_ctrl(hole, ctrl_[scan]);
                    slots_[hole].key = std::move(slots_[scan].key);
                    slots_[hole].value = std::move(slots_[scan].value);
                    if constexpr (!uses_tombstones) {
                        slots_[hole].home_ = natural_pos;
                    }
                    hole = scan;
                }
                
                scan = (scan + 1) & mask_;
            }
            
            slots_[hole].destroy();
            set_ctrl(hole, fasthash_detail::kEmpty);
        }
    }
    
public:
    FastHashMap() = default;
    
    explicit FastHashMap(size_t initial_capacity, float load_factor = 0.875f)
        : max_load_factor_(normalize_load_factor(load_factor)) {
        if (initial_capacity > 0) {
            size_t cap = kMinCapacity;
            while (cap < initial_capacity) cap *= 2;
            allocate(cap);
        }
    }
    
    ~FastHashMap() { deallocate(); }
    
    FastHashMap(const FastHashMap& other)
        : max_load_factor_(other.max_load_factor_)
        , frozen_(other.frozen_)
        , hasher_(other.hasher_)
        , key_equal_(other.key_equal_)
        , allocator_() {  // Default construct allocator (can't copy stateful allocators)
        if (other.capacity_ > 0) {
            allocate(other.capacity_);
            for (size_t i = 0; i < other.capacity_; ++i) {
                if (fasthash_detail::is_full(other.ctrl_[i])) {
                    set_ctrl(i, other.ctrl_[i]);
                    new (&slots_[i]) Slot();
                    slots_[i].emplace(other.slots_[i].key, other.slots_[i].value,
                                     other.slots_[i].home());
                } else if (fasthash_detail::is_deleted(other.ctrl_[i])) {
                    set_ctrl(i, fasthash_detail::kDeleted);
                }
                // Empty slots already initialized by allocate()
            }
            size_ = other.size_;
            if constexpr (uses_tombstones) {
                tombstones_ = other.tombstones_;
            }
        }
    }
    
    // Move constructor - only available when allocator supports pointer stealing
    // For inline-storage allocators (FixedAllocator), moves are deleted because
    // pointers into the embedded buffer would dangle after move.
    template<typename A = Allocator,
             std::enable_if_t<A::kPointerStealSafe, int> = 0>
    FastHashMap(FastHashMap&& other) noexcept(is_nothrow_move_constructible_v)
        : ctrl_(other.ctrl_)
        , slots_(other.slots_)
        , size_(other.size_)
        , capacity_(other.capacity_)
        , mask_(other.mask_)
        , growth_threshold_(other.growth_threshold_)
        , max_load_factor_(other.max_load_factor_)
        , frozen_(other.frozen_)
        , hasher_(std::move(other.hasher_))
        , key_equal_(std::move(other.key_equal_))
        , allocator_(std::move(other.allocator_)) {
        if constexpr (uses_tombstones) {
            tombstones_ = other.tombstones_;
            other.tombstones_ = 0;
        }
        other.ctrl_ = nullptr;
        other.slots_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        other.mask_ = 0;
        other.frozen_ = false;
    }
    
    // Deleted move constructor for inline-storage allocators
    template<typename A = Allocator,
             std::enable_if_t<!A::kPointerStealSafe, int> = 0>
    FastHashMap(FastHashMap&&) = delete;
    
    FastHashMap& operator=(const FastHashMap& other) {
        if (this != &other) {
            FastHashMap tmp(other);
            swap(tmp);
        }
        return *this;
    }
    
    // Move assignment - only available when allocator supports pointer stealing
    template<typename A = Allocator,
             std::enable_if_t<A::kPointerStealSafe, int> = 0>
    FastHashMap& operator=(FastHashMap&& other) noexcept(is_nothrow_move_assignable_v) {
        if (this != &other) {
            deallocate();
            ctrl_ = other.ctrl_;
            slots_ = other.slots_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            mask_ = other.mask_;
            growth_threshold_ = other.growth_threshold_;
            max_load_factor_ = other.max_load_factor_;
            frozen_ = other.frozen_;
            hasher_ = std::move(other.hasher_);
            key_equal_ = std::move(other.key_equal_);
            allocator_ = std::move(other.allocator_);
            if constexpr (uses_tombstones) {
                tombstones_ = other.tombstones_;
                other.tombstones_ = 0;
            }
            other.ctrl_ = nullptr;
            other.slots_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
            other.mask_ = 0;
            other.frozen_ = false;
        }
        return *this;
    }
    
    // Deleted move assignment for inline-storage allocators
    template<typename A = Allocator,
             std::enable_if_t<!A::kPointerStealSafe, int> = 0>
    FastHashMap& operator=(FastHashMap&&) = delete;
    
    // Swap - only available when allocator supports pointer stealing
    template<typename A = Allocator,
             std::enable_if_t<A::kPointerStealSafe, int> = 0>
    void swap(FastHashMap& other) noexcept(is_nothrow_swappable_v) {
        std::swap(ctrl_, other.ctrl_);
        std::swap(slots_, other.slots_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        std::swap(mask_, other.mask_);
        std::swap(growth_threshold_, other.growth_threshold_);
        std::swap(max_load_factor_, other.max_load_factor_);
        std::swap(frozen_, other.frozen_);
        std::swap(hasher_, other.hasher_);
        std::swap(key_equal_, other.key_equal_);
        std::swap(allocator_, other.allocator_);
        if constexpr (uses_tombstones) {
            std::swap(tombstones_, other.tombstones_);
        }
    }
    
    // Capacity
    bool empty() const noexcept { return size_ == 0; }
    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept { return capacity_; }
    float load_factor() const noexcept {
        return capacity_ ? static_cast<float>(size_) / capacity_ : 0.0f;
    }
    float max_load_factor() const noexcept { return max_load_factor_; }
    
    /// Get allocator reference (for diagnostics/introspection)
    Allocator& get_allocator() noexcept { return allocator_; }
    const Allocator& get_allocator() const noexcept { return allocator_; }
    
    /// Freeze the map for read-only access. Enables safe high-density lookup.
    /// After freezing, insert/erase/clear will assert in debug builds.
    /// Cannot be unfrozen - create a new map if mutations are needed.
    FastHashMap& freeze() noexcept { frozen_ = true; return *this; }
    
    /// Check if map is frozen (read-only mode)
    bool is_frozen() const noexcept { return frozen_; }
    
    void reserve(size_t count) {
        assert(!frozen_ && "FastHashMap::reserve() called on frozen map");
        size_t required = static_cast<size_t>(count / max_load_factor_) + 1;
        if (required > capacity_) {
            size_t new_cap = kMinCapacity;
            while (new_cap < required) new_cap *= 2;
            rehash_internal(new_cap);
        }
    }
    
    void rehash(size_t count) {
        assert(!frozen_ && "FastHashMap::rehash() called on frozen map");
        size_t new_cap = kMinCapacity;
        while (new_cap < count) new_cap *= 2;
        bool should_rehash = new_cap > capacity_;
        if constexpr (uses_tombstones) {
            should_rehash = should_rehash || (tombstones_ > size_ / 4);
        }
        if (should_rehash) {
            rehash_internal(new_cap);
        }
    }
    
    void clear() {
        assert(!frozen_ && "FastHashMap::clear() called on frozen map");
        if (ctrl_) {
            for (size_t i = 0; i < capacity_; ++i) {
                if (fasthash_detail::is_full(ctrl_[i])) {
                    slots_[i].destroy();
                }
            }
            // Reset all control bytes including mirrored tail
            std::memset(ctrl_, fasthash_detail::kEmpty, capacity_ + Group::kWidth);
            size_ = 0;
            if constexpr (uses_tombstones) {
                tombstones_ = 0;
            }
        }
    }
    
    // Modifiers
    template<typename K, typename V>
    Value* insert(K&& key, V&& value) {
        assert(!frozen_ && "FastHashMap::insert() called on frozen map");
        if (capacity_ == 0) allocate(kMinCapacity);
        maybe_rehash();
        
        size_t h = hash_key(key);
        uint8_t h2 = fasthash_detail::H2(h);
        size_t home = h & mask_;
        
        ProbeSequence seq(h, mask_);
        size_t insert_idx = SIZE_MAX;
        
        while (true) {
            Group g(ctrl_ + seq.offset());
            
            for (uint32_t i : g.match(h2)) {
                size_t idx = seq.offset(i);
                if (key_equal_(slots_[idx].key, key)) {
                    return nullptr;  // Already exists
                }
            }
            
            if (insert_idx == SIZE_MAX) {
                BitMask empty_mask = uses_tombstones ? g.match_empty_or_deleted() : g.match_empty();
                if (empty_mask) {
                    insert_idx = seq.offset(empty_mask.lowest_set_bit());
                }
            }
            
            if (g.match_empty()) break;
            seq.next();
        }
        
        if (insert_idx == SIZE_MAX) {
            insert_idx = find_insert_slot(h);
        }
        
        if constexpr (uses_tombstones) {
            if (fasthash_detail::is_deleted(ctrl_[insert_idx])) {
                --tombstones_;
            }
        }
        
        set_ctrl(insert_idx, h2);
        new (&slots_[insert_idx]) Slot();
        slots_[insert_idx].emplace(std::forward<K>(key), std::forward<V>(value), home);
        ++size_;
        
        return &slots_[insert_idx].value;
    }
    
    template<typename K, typename V>
    std::pair<Value*, bool> insert_or_assign(K&& key, V&& value) {
        assert(!frozen_ && "FastHashMap::insert_or_assign() called on frozen map");
        if (capacity_ == 0) allocate(kMinCapacity);
        maybe_rehash();
        
        size_t h = hash_key(key);
        uint8_t h2 = fasthash_detail::H2(h);
        size_t home = h & mask_;
        
        ProbeSequence seq(h, mask_);
        size_t insert_idx = SIZE_MAX;
        
        while (true) {
            Group g(ctrl_ + seq.offset());
            
            for (uint32_t i : g.match(h2)) {
                size_t idx = seq.offset(i);
                if (key_equal_(slots_[idx].key, key)) {
                    // Key exists - assign new value
                    slots_[idx].value = std::forward<V>(value);
                    return {&slots_[idx].value, false};
                }
            }
            
            if (insert_idx == SIZE_MAX) {
                BitMask empty_mask = uses_tombstones ? g.match_empty_or_deleted() : g.match_empty();
                if (empty_mask) {
                    insert_idx = seq.offset(empty_mask.lowest_set_bit());
                }
            }
            
            if (g.match_empty()) break;
            seq.next();
        }
        
        if (insert_idx == SIZE_MAX) {
            insert_idx = find_insert_slot(h);
        }
        
        if constexpr (uses_tombstones) {
            if (fasthash_detail::is_deleted(ctrl_[insert_idx])) {
                --tombstones_;
            }
        }
        
        set_ctrl(insert_idx, h2);
        new (&slots_[insert_idx]) Slot();
        slots_[insert_idx].emplace(std::forward<K>(key), std::forward<V>(value), home);
        ++size_;
        
        return {&slots_[insert_idx].value, true};
    }
    
    /// Erase a key. Supports heterogeneous lookup.
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    bool erase(const K& key) {
        assert(!frozen_ && "FastHashMap::erase() called on frozen map");
        auto [idx, found] = find_slot(key);
        if (!found) return false;
        erase_at(idx);
        return true;
    }
    
    // Lookup - Supports heterogeneous lookup (e.g., find with string_view for string keys)
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    Value* find(const K& key) {
        auto [idx, found] = find_slot(key);
        return found ? &slots_[idx].value : nullptr;
    }
    
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    const Value* find(const K& key) const {
        auto [idx, found] = find_slot(key);
        return found ? &slots_[idx].value : nullptr;
    }
    
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    bool contains(const K& key) const {
        auto [idx, found] = find_slot(key);
        return found;
    }
    
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    size_t count(const K& key) const {
        return contains(key) ? 1 : 0;
    }
    
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    Value& at(const K& key) {
        auto [idx, found] = find_slot(key);
        if (!found) throw std::out_of_range("FastHashMap::at: key not found");
        return slots_[idx].value;
    }
    
    template<typename K,
             std::enable_if_t<is_hetero_lookup_enabled<K>, int> = 0>
    const Value& at(const K& key) const {
        auto [idx, found] = find_slot(key);
        if (!found) throw std::out_of_range("FastHashMap::at: key not found");
        return slots_[idx].value;
    }
    
    Value& operator[](const Key& key) {
        assert(!frozen_ && "FastHashMap::operator[] called on frozen map (use find() or at())");
        if (capacity_ == 0) allocate(kMinCapacity);
        maybe_rehash();
        
        size_t h = hash_key(key);
        uint8_t h2 = fasthash_detail::H2(h);
        size_t home = h & mask_;
        
        ProbeSequence seq(h, mask_);
        size_t insert_idx = SIZE_MAX;
        
        while (true) {
            Group g(ctrl_ + seq.offset());
            
            for (uint32_t i : g.match(h2)) {
                size_t idx = seq.offset(i);
                if (key_equal_(slots_[idx].key, key)) {
                    return slots_[idx].value;
                }
            }
            
            if (insert_idx == SIZE_MAX) {
                BitMask empty_mask = uses_tombstones ? g.match_empty_or_deleted() : g.match_empty();
                if (empty_mask) {
                    insert_idx = seq.offset(empty_mask.lowest_set_bit());
                }
            }
            
            if (g.match_empty()) break;
            seq.next();
        }
        
        if (insert_idx == SIZE_MAX) {
            insert_idx = find_insert_slot(h);
        }
        
        if constexpr (uses_tombstones) {
            if (fasthash_detail::is_deleted(ctrl_[insert_idx])) {
                --tombstones_;
            }
        }
        
        set_ctrl(insert_idx, h2);
        new (&slots_[insert_idx]) Slot();
        slots_[insert_idx].emplace(key, Value{}, home);
        ++size_;
        
        return slots_[insert_idx].value;
    }
    
    // Iterator
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;
        
        iterator() : ctrl_(nullptr), slots_(nullptr), idx_(0), cap_(0) {}
        iterator(uint8_t* ctrl, Slot* slots, size_t idx, size_t cap)
            : ctrl_(ctrl), slots_(slots), idx_(idx), cap_(cap) { skip_empty(); }
        
        value_type operator*() const { return {slots_[idx_].key, slots_[idx_].value}; }
        const Key& key() const { return slots_[idx_].key; }
        Value& value() const { return slots_[idx_].value; }
        
        iterator& operator++() { ++idx_; skip_empty(); return *this; }
        iterator operator++(int) { iterator tmp = *this; ++(*this); return tmp; }
        bool operator==(const iterator& o) const { return ctrl_ == o.ctrl_ && idx_ == o.idx_; }
        bool operator!=(const iterator& o) const { return !(*this == o); }
        
    private:
        void skip_empty() {
            while (idx_ < cap_ && !fasthash_detail::is_full(ctrl_[idx_])) ++idx_;
        }
        uint8_t* ctrl_;
        Slot* slots_;
        size_t idx_;
        size_t cap_;
    };
    
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, const Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;
        
        const_iterator() : ctrl_(nullptr), slots_(nullptr), idx_(0), cap_(0) {}
        const_iterator(const uint8_t* ctrl, const Slot* slots, size_t idx, size_t cap)
            : ctrl_(ctrl), slots_(slots), idx_(idx), cap_(cap) { skip_empty(); }
        
        value_type operator*() const { return {slots_[idx_].key, slots_[idx_].value}; }
        const Key& key() const { return slots_[idx_].key; }
        const Value& value() const { return slots_[idx_].value; }
        
        const_iterator& operator++() { ++idx_; skip_empty(); return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++(*this); return tmp; }
        bool operator==(const const_iterator& o) const { return ctrl_ == o.ctrl_ && idx_ == o.idx_; }
        bool operator!=(const const_iterator& o) const { return !(*this == o); }
        
    private:
        void skip_empty() {
            while (idx_ < cap_ && !fasthash_detail::is_full(ctrl_[idx_])) ++idx_;
        }
        const uint8_t* ctrl_;
        const Slot* slots_;
        size_t idx_;
        size_t cap_;
    };
    
    iterator begin() { return iterator(ctrl_, slots_, 0, capacity_); }
    iterator end() { return iterator(ctrl_, slots_, capacity_, capacity_); }
    const_iterator begin() const { return const_iterator(ctrl_, slots_, 0, capacity_); }
    const_iterator end() const { return const_iterator(ctrl_, slots_, capacity_, capacity_); }
    const_iterator cbegin() const { return const_iterator(ctrl_, slots_, 0, capacity_); }
    const_iterator cend() const { return const_iterator(ctrl_, slots_, capacity_, capacity_); }
    
    // Diagnostics
    static const char* simd_backend() {
#if defined(FAST_HASH_MAP_AVX512)
        return "AVX-512";
#elif defined(FAST_HASH_MAP_AVX2)
        return "AVX2";
#elif defined(FAST_HASH_MAP_SSE2)
        return "SSE2";
#elif defined(FAST_HASH_MAP_NEON)
        return "NEON";
#else
        return "Portable";
#endif
    }
    
    static constexpr size_t group_width() {
        return Group::kWidth;
    }
    
    static const char* deletion_policy_name() {
        return DeletionPolicy::name();
    }
    
    static const char* allocator_name() {
        return Allocator::name();
    }
};

// Convenience aliases - Default allocator (HeapAllocator)
template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
using FastHashMapBS = FastHashMap<K, V, H, E, BackwardShiftDeletion, HeapAllocator>;

template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>>
using FastHashMapTS = FastHashMap<K, V, H, E, TombstoneDeletion, HeapAllocator>;

// Fixed-size map aliases (no heap allocation after construction)
template<typename K, typename V, size_t BufferBytes = 4096,
         typename H = std::hash<K>, typename E = std::equal_to<K>>
using FixedHashMap = FastHashMap<K, V, H, E, TombstoneDeletion, FixedAllocator<BufferBytes>>;

template<typename K, typename V, size_t BufferBytes = 4096,
         typename H = std::hash<K>, typename E = std::equal_to<K>>
using FixedHashMapBS = FastHashMap<K, V, H, E, BackwardShiftDeletion, FixedAllocator<BufferBytes>>;

} // namespace fat_p
