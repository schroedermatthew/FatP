#pragma once

/*
FATP_META:
  meta_version: 1
  component: SlotMap
  file_role: public_header
  path: include/fat_p/SlotMap.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for SlotMap."
  api_stability: in_work
  related:
    docs_search: "SlotMap"
    tests:
      - components/SlotMap/tests/test_SlotMap.cpp
    benchmarks:
      - components/SlotMap/benchmarks/benchmark_SlotMap.cpp
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
 * @file SlotMap.h
 * @brief Generational index container providing stable handles and O(1) access.
 *
 * @details A container that provides stable handles even after element removal.
 * Uses generational counters to detect stale/dangling handles (ABA safety).
 * Data is stored contiguously in memory (dense array) for cache efficiency.
 *
 * Architecture:
 * @code
 *   Handle = { index, generation }
 *      Ã¢â€â€š
 *      Ã¢â€“Â¼
 *   mSlots[index] = { generation, data_index }
 *      Ã¢â€â€š                   Ã¢â€â€š
 *      Ã¢â€â€š generation match? Ã¢â€â€š
 *      Ã¢â€“Â¼                   Ã¢â€“Â¼
 *   Valid access       mData[data_index] = actual value
 * @endcode
 *
 * Key properties:
 * - Insert: O(1) amortized
 * - Remove: O(1) via swap-and-pop
 * - Access: O(1) with two array lookups
 * - Iteration: O(n) over dense storage (cache-friendly)
 * - ABA-safe: Stale handles return nullptr, never wrong data
 *
 * Use cases:
 * - Entity-Component Systems (game engines)
 * - Resource pools without shared_ptr overhead
 * - Any scenario requiring safe handles to deletable objects
 *
 * @note Thread Safety: Not thread-safe. External synchronization required.
 *
 * Example:
 * @code
 *   SlotMap<Enemy> enemies;
 *   auto handle = enemies.insert(Enemy{100, 50});
 *   Enemy* ptr = enemies.get(handle);       // Valid access
 *   enemies.erase(handle);
 *   ptr = enemies.get(handle);              // Returns nullptr (safe!)
 *   auto new_handle = enemies.insert(...);  // May reuse same slot
 *   enemies.get(handle);                    // Still nullptr (generation changed)
 * @endcode
 */

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

// =============================================================================
// SlotMapHandle - Standalone Handle Type
// =============================================================================
//
// Extracted from SlotMap to enable std::hash specialization.
// Template-dependent nested types cannot be specialized in namespace std.
//

/**
 * @brief Opaque handle to a slot in a SlotMap.
 *
 * @tparam GenerationType Unsigned integer type for the generation counter.
 *         Default uint32_t provides ~4 billion generations per slot.
 *         Use uint64_t for long-running systems with high slot churn.
 *
 * @details Encodes both the slot index and generation counter. Two handles to
 * the same index but different generations refer to different objects.
 *
 * @note Handle validity can only be determined by querying the SlotMap.
 * The is_null() method only checks if the handle was default-constructed.
 */
template <typename GenerationType = uint32_t>
struct SlotMapHandleT
{
    static_assert(std::is_unsigned_v<GenerationType>,
                  "GenerationType must be an unsigned integer type");

    uint32_t index{0};
    GenerationType generation{0};

    constexpr SlotMapHandleT() noexcept = default;

    constexpr SlotMapHandleT(uint32_t idx, GenerationType gen) noexcept
        : index(idx)
        , generation(gen)
    {
    }

    [[nodiscard]] constexpr bool operator==(const SlotMapHandleT& other) const noexcept
    {
        return index == other.index && generation == other.generation;
    }

    [[nodiscard]] constexpr bool operator!=(const SlotMapHandleT& other) const noexcept
    {
        return !(*this == other);
    }

    /// @brief Lexicographic comparison for use in std::set/std::map.
    [[nodiscard]] constexpr bool operator<(const SlotMapHandleT& other) const noexcept
    {
        if (index != other.index)
        {
            return index < other.index;
        }
        return generation < other.generation;
    }

    [[nodiscard]] constexpr bool operator<=(const SlotMapHandleT& other) const noexcept
    {
        return !(other < *this);
    }

    [[nodiscard]] constexpr bool operator>(const SlotMapHandleT& other) const noexcept
    {
        return other < *this;
    }

    [[nodiscard]] constexpr bool operator>=(const SlotMapHandleT& other) const noexcept
    {
        return !(*this < other);
    }

    /// @brief Returns true if this handle was default-constructed (never assigned).
    /// @note A non-null handle may still be invalid if the slot was erased.
    /// Use SlotMap::is_valid(handle) to check actual validity.
    [[nodiscard]] constexpr bool is_null() const noexcept
    {
        return generation == 0;
    }

    /// @brief Returns true if this handle has been assigned (not default-constructed).
    /// @note This does NOT guarantee the handle is valid in any SlotMap.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return !is_null();
    }
};

/// @brief Default 32-bit generation handle (backward compatible).
using SlotMapHandle = SlotMapHandleT<uint32_t>;

/// @brief 64-bit generation handle for long-running systems with high slot churn.
/// Wrap time at 1M insert/erase cycles per second per slot: ~584 million years.
using SlotMapHandle64 = SlotMapHandleT<uint64_t>;

// =============================================================================
// SlotMap - Generational Index Container
// =============================================================================

/**
 * @brief A generational index container providing stable handles to elements.
 *
 * @tparam T The type of element to store.
 * @tparam GenerationType Unsigned integer type for generation counters.
 *         uint32_t (default): wraps after ~4 billion insert/erase cycles per slot.
 *         uint64_t: effectively never wraps (~584 million years at 1M ops/sec/slot).
 * @tparam Allocator The allocator for internal storage (default: std::allocator<T>).
 *
 * @note Complexity:
 * - insert(): O(1) amortized
 * - erase(): O(1)
 * - get(): O(1)
 * - is_valid(): O(1)
 * - Iteration: O(n)
 *
 * @note Thread Safety: Not thread-safe. External synchronization required.
 */
template <typename T, typename GenerationType = uint32_t, typename Allocator = std::allocator<T>>
class SlotMap
{
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    using value_type = T;
    using size_type = uint32_t;
    using generation_type = GenerationType;
    using allocator_type = Allocator;
    using Handle = SlotMapHandleT<GenerationType>;

    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    // Direct mapping to std::vector iterators for maximum performance
    using iterator = typename std::vector<T, Allocator>::iterator;
    using const_iterator = typename std::vector<T, Allocator>::const_iterator;

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct Slot
    {
        generation_type generation{0}; // 0 = never used or wrapped, incremented on insert/erase
        size_type data_index{0};       // Index into dense data array
    };

    // Allocator rebinding for internal vectors
    using IndexAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<size_type>;
    using SlotAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Slot>;

public:
    // =========================================================================
    // Entry Types for Handle+Value Iteration
    // =========================================================================

    /**
     * @brief Handle + reference pair for entries() iteration.
     * @warning The reference is only valid during iteration. Do not store Entry
     * objects; extract the handle if persistence is needed.
     */
    struct Entry
    {
        Handle handle;
        T& value;

        Entry(Handle h, T& v) noexcept
            : handle(h)
            , value(v)
        {
        }
    };

    /**
     * @brief Const version of Entry for const iteration.
     */
    struct ConstEntry
    {
        Handle handle;
        const T& value;

        ConstEntry(Handle h, const T& v) noexcept
            : handle(h)
            , value(v)
        {
        }
    };

    // =========================================================================
    // Entry Iterators (Custom - Required for Handle Reconstruction)
    // =========================================================================

    /**
     * @brief Iterator over (handle, value) pairs.
     */
    class EntryIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Entry;
        using pointer = Entry*;
        using reference = Entry;

        EntryIterator(SlotMap* map, size_type idx) noexcept
            : mMap(map)
            , mIndex(idx)
        {
        }

        [[nodiscard]] Entry operator*() const
        {
            size_type slot_index = mMap->mEraseMap[mIndex];
            const Slot& slot = mMap->mSlots[slot_index];
            return Entry{Handle{slot_index, slot.generation}, mMap->mData[mIndex]};
        }

        EntryIterator& operator++() noexcept
        {
            ++mIndex;
            return *this;
        }

        EntryIterator operator++(int) noexcept
        {
            EntryIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const EntryIterator& other) const noexcept
        {
            return mIndex == other.mIndex;
        }

        [[nodiscard]] bool operator!=(const EntryIterator& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        SlotMap* mMap;
        size_type mIndex;
    };

    /**
     * @brief Const iterator over (handle, value) pairs.
     */
    class ConstEntryIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = ConstEntry;
        using pointer = const ConstEntry*;
        using reference = ConstEntry;

        ConstEntryIterator(const SlotMap* map, size_type idx) noexcept
            : mMap(map)
            , mIndex(idx)
        {
        }

        [[nodiscard]] ConstEntry operator*() const
        {
            size_type slot_index = mMap->mEraseMap[mIndex];
            const Slot& slot = mMap->mSlots[slot_index];
            return ConstEntry{Handle{slot_index, slot.generation}, mMap->mData[mIndex]};
        }

        ConstEntryIterator& operator++() noexcept
        {
            ++mIndex;
            return *this;
        }

        ConstEntryIterator operator++(int) noexcept
        {
            ConstEntryIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const ConstEntryIterator& other) const noexcept
        {
            return mIndex == other.mIndex;
        }

        [[nodiscard]] bool operator!=(const ConstEntryIterator& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        const SlotMap* mMap;
        size_type mIndex;
    };

    /**
     * @brief Range wrapper for entries() iteration.
     */
    class EntryRange
    {
    public:
        explicit EntryRange(SlotMap* map) noexcept
            : mMap(map)
        {
        }

        [[nodiscard]] EntryIterator begin() noexcept
        {
            return EntryIterator(mMap, 0);
        }

        [[nodiscard]] EntryIterator end() noexcept
        {
            return EntryIterator(mMap, static_cast<size_type>(mMap->mData.size()));
        }

    private:
        SlotMap* mMap;
    };

    /**
     * @brief Const range wrapper for entries() iteration.
     */
    class ConstEntryRange
    {
    public:
        explicit ConstEntryRange(const SlotMap* map) noexcept
            : mMap(map)
        {
        }

        [[nodiscard]] ConstEntryIterator begin() const noexcept
        {
            return ConstEntryIterator(mMap, 0);
        }

        [[nodiscard]] ConstEntryIterator end() const noexcept
        {
            return ConstEntryIterator(mMap, static_cast<size_type>(mMap->mData.size()));
        }

    private:
        const SlotMap* mMap;
    };

    // =========================================================================
    // Construction / Assignment
    // =========================================================================

    /**
     * @brief Construct an empty SlotMap with optional allocator.
     * @param alloc The allocator to use for internal storage.
     */
    explicit SlotMap(const Allocator& alloc = Allocator())
        : mData(alloc)
        , mEraseMap(IndexAllocator(alloc))
        , mSlots(SlotAllocator(alloc))
        , mFreeList(IndexAllocator(alloc))
    {
    }

    ~SlotMap() = default;

    SlotMap(const SlotMap&) = default;
    SlotMap& operator=(const SlotMap&) = default;

    SlotMap(SlotMap&&) noexcept(std::is_nothrow_move_constructible_v<std::vector<T, Allocator>> &&
                                std::is_nothrow_move_constructible_v<std::vector<size_type, IndexAllocator>> &&
                                std::is_nothrow_move_constructible_v<std::vector<Slot, SlotAllocator>>) = default;

    SlotMap&
    operator=(SlotMap&&) noexcept(std::is_nothrow_move_assignable_v<std::vector<T, Allocator>> &&
                                  std::is_nothrow_move_assignable_v<std::vector<size_type, IndexAllocator>> &&
                                  std::is_nothrow_move_assignable_v<std::vector<Slot, SlotAllocator>>) = default;

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Insert element and return handle.
     *
     * @tparam Args Constructor arguments for T.
     * @param args Arguments forwarded to T's constructor.
     * @return Handle to the inserted element.
     *
     * @note Complexity: O(1) amortized.
     * @note Exception Safety: Strong guarantee. If an exception is thrown,
     *       the SlotMap remains in its original state.
     *
     * @warning Pointers/references to existing elements may be invalidated
     *          if reallocation occurs.
     */
    template <typename... Args>
    [[nodiscard]] Handle insert(Args&&... args)
    {
        // 1. Determine source BEFORE modifying any state (for exception safety)
        const bool came_from_free_list = !mFreeList.empty();
        size_type slot_index;

        if (came_from_free_list)
        {
            slot_index = mFreeList.back();
            mFreeList.pop_back();
        }
        else
        {
            slot_index = static_cast<size_type>(mSlots.size());
            mSlots.emplace_back();
        }

        Slot& slot = mSlots[slot_index];

        // Wrap to 1 (not 0): generation 0 is reserved as "never used" sentinel.
        // This creates a theoretical ABA window after 2^32 cycles on one slot.
        // See class documentation for details.
        if (++slot.generation == 0)
        {
            slot.generation = 1;
        }

        // 2. Try data insertion with proper rollback on exception
        try
        {
            mData.emplace_back(std::forward<Args>(args)...);
        }
        catch (...)
        {
            // Rollback slot allocation
            if (came_from_free_list)
            {
                mFreeList.push_back(slot_index);
            }
            else
            {
                mSlots.pop_back();
            }
            throw;
        }

        // 3. Update erase map (can also throw on allocation)
        try
        {
            mEraseMap.push_back(slot_index);
        }
        catch (...)
        {
            // Rollback data insertion
            mData.pop_back();
            if (came_from_free_list)
            {
                mFreeList.push_back(slot_index);
            }
            else
            {
                mSlots.pop_back();
            }
            throw;
        }

        // 4. Update slot to point to data (no-throw)
        slot.data_index = static_cast<size_type>(mData.size() - 1);

        return Handle{slot_index, slot.generation};
    }

    /**
     * @brief Alias for insert() - constructs element in-place.
     * @tparam Args Constructor arguments for T.
     * @param args Arguments forwarded to T's constructor.
     * @return Handle to the emplaced element.
     */
    template <typename... Args>
    [[nodiscard]] Handle emplace(Args&&... args)
    {
        return insert(std::forward<Args>(args)...);
    }

    /**
     * @brief Erase element by handle.
     *
     * @param handle The handle to the element to erase.
     * @return true if element was erased, false if handle was invalid.
     *
     * @note Complexity: O(1).
     * @note The last element in the dense array is moved to fill the hole.
     */
    bool erase(Handle handle)
    {
        if (!is_valid(handle))
        {
            return false;
        }

        Slot& slot = mSlots[handle.index];
        const size_type data_index = slot.data_index;
        const size_type last_data_index = static_cast<size_type>(mData.size() - 1);

        // Swap-and-pop from dense array
        if (data_index < last_data_index)
        {
            using std::swap;
            swap(mData[data_index], mData.back());
            swap(mEraseMap[data_index], mEraseMap.back());

            // Update the swapped element's slot to point to new location
            mSlots[mEraseMap[data_index]].data_index = data_index;
        }

        mData.pop_back();
        mEraseMap.pop_back();

        // Invalidate slot and add to free list
        if (++slot.generation == 0)
        {
            slot.generation = 1; // Skip 0 to preserve is_null() semantics
        }
        slot.data_index = std::numeric_limits<size_type>::max();
        mFreeList.push_back(handle.index);

        return true;
    }

    /**
     * @brief Clear all elements.
     *
     * @note All existing handles become invalid.
     * @note Complexity: O(slot_count).
     * @note Memory: Does not deallocate. Use shrink_to_fit() after clear()
     *       if memory reclamation is needed.
     *
     * @warning This invalidates ALL handles, including those held externally.
     */
    void clear()
    {
        mData.clear();
        mEraseMap.clear();

        // CRITICAL: Do NOT clear mSlots!
        // We must increment generations to invalidate existing external handles.
        // Otherwise, old handles could match new insertions (ABA violation).
        mFreeList.clear();
        mFreeList.reserve(mSlots.size());

        for (size_type i = 0; i < static_cast<size_type>(mSlots.size()); ++i)
        {
            if (++mSlots[i].generation == 0)
            {
                mSlots[i].generation = 1; // Skip 0
            }
            mSlots[i].data_index = std::numeric_limits<size_type>::max();
            mFreeList.push_back(i);
        }
    }

    /**
     * @brief Reserve capacity for n elements (reduces allocations).
     * @param capacity The number of elements to reserve space for.
     */
    void reserve(size_type capacity)
    {
        mData.reserve(capacity);
        mEraseMap.reserve(capacity);
        mSlots.reserve(capacity);
        mFreeList.reserve(capacity);
    }

    /**
     * @brief Reduce memory usage by freeing unused capacity.
     */
    void shrink_to_fit()
    {
        mData.shrink_to_fit();
        mEraseMap.shrink_to_fit();
        // Note: mSlots and mFreeList are not shrunk to preserve slot stability
    }

    /**
     * @brief Swap contents with another SlotMap.
     * @param other The SlotMap to swap with.
     */
    void swap(SlotMap& other) noexcept(std::is_nothrow_swappable_v<std::vector<T, Allocator>> &&
                                       std::is_nothrow_swappable_v<std::vector<size_type, IndexAllocator>> &&
                                       std::is_nothrow_swappable_v<std::vector<Slot, SlotAllocator>>)
    {
        using std::swap;
        swap(mData, other.mData);
        swap(mEraseMap, other.mEraseMap);
        swap(mSlots, other.mSlots);
        swap(mFreeList, other.mFreeList);
    }

    // =========================================================================
    // Access
    // =========================================================================

    /**
     * @brief Get pointer to element (nullptr if handle is invalid).
     *
     * @param handle The handle to look up.
     * @return Pointer to element, or nullptr if invalid.
     *
     * @note Complexity: O(1).
     * @warning Returned pointer is invalidated by any insert() that causes
     *          reallocation, or by erase() of any element.
     */
    [[nodiscard]] T* get(Handle handle) noexcept
    {
        if (!is_valid(handle))
        {
            return nullptr;
        }
        return &mData[mSlots[handle.index].data_index];
    }

    /**
     * @brief Get const pointer to element (nullptr if handle is invalid).
     */
    [[nodiscard]] const T* get(Handle handle) const noexcept
    {
        if (!is_valid(handle))
        {
            return nullptr;
        }
        return &mData[mSlots[handle.index].data_index];
    }

    /**
     * @brief Get reference to element, throws if handle is invalid.
     *
     * @param handle The handle to look up.
     * @return Reference to element.
     * @throws std::out_of_range if handle is invalid.
     */
    [[nodiscard]] T& at(Handle handle)
    {
        if (!is_valid(handle))
        {
            throw std::out_of_range("SlotMap::at: invalid handle");
        }
        return mData[mSlots[handle.index].data_index];
    }

    /**
     * @brief Get const reference to element, throws if handle is invalid.
     */
    [[nodiscard]] const T& at(Handle handle) const
    {
        if (!is_valid(handle))
        {
            throw std::out_of_range("SlotMap::at: invalid handle");
        }
        return mData[mSlots[handle.index].data_index];
    }

    /**
     * @brief Get reference to element WITHOUT validity check.
     *
     * @param handle The handle (must be valid!).
     * @return Reference to element.
     *
     * @warning Undefined behavior if handle is invalid.
     * @note Use only when validity is guaranteed by surrounding logic.
     * @note For HPC tight loops where the ~3ns validation overhead matters.
     */
    [[nodiscard]] T& get_unchecked(Handle handle) noexcept
    {
        assert(is_valid(handle) && "SlotMap::get_unchecked: invalid handle");
        return mData[mSlots[handle.index].data_index];
    }

    /**
     * @brief Get const reference to element WITHOUT validity check.
     */
    [[nodiscard]] const T& get_unchecked(Handle handle) const noexcept
    {
        assert(is_valid(handle) && "SlotMap::get_unchecked: invalid handle");
        return mData[mSlots[handle.index].data_index];
    }

    /**
     * @brief Check if handle refers to a valid element.
     *
     * @param handle The handle to check.
     * @return true if handle is valid and refers to an existing element.
     *
     * @note Complexity: O(1).
     * @note This is the authoritative validity check.
     */
    [[nodiscard]] bool is_valid(Handle handle) const noexcept
    {
        if (handle.index >= static_cast<size_type>(mSlots.size()))
        {
            return false;
        }
        return mSlots[handle.index].generation == handle.generation;
    }

    /**
     * @brief Alias for is_valid() - STL-style name.
     */
    [[nodiscard]] bool contains(Handle handle) const noexcept
    {
        return is_valid(handle);
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /**
     * @brief Number of elements currently stored.
     */
    [[nodiscard]] size_type size() const noexcept
    {
        return static_cast<size_type>(mData.size());
    }

    /**
     * @brief Current capacity (elements before reallocation).
     */
    [[nodiscard]] size_type capacity() const noexcept
    {
        return static_cast<size_type>(mData.capacity());
    }

    /**
     * @brief True if container is empty.
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return mData.empty();
    }

    /**
     * @brief Number of slots ever allocated (including free slots).
     */
    [[nodiscard]] size_type slot_count() const noexcept
    {
        return static_cast<size_type>(mSlots.size());
    }

    /**
     * @brief Number of free slots available for reuse.
     */
    [[nodiscard]] size_type free_slot_count() const noexcept
    {
        return static_cast<size_type>(mFreeList.size());
    }

    /**
     * @brief Get the allocator.
     */
    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return allocator_type(mData.get_allocator());
    }

    // =========================================================================
    // Iteration
    // =========================================================================

    /**
     * @brief Iterator to the first element (dense array).
     * @note Iteration is O(n) and cache-friendly.
     */
    [[nodiscard]] iterator begin() noexcept
    {
        return mData.begin();
    }
    [[nodiscard]] iterator end() noexcept
    {
        return mData.end();
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return mData.begin();
    }
    [[nodiscard]] const_iterator end() const noexcept
    {
        return mData.end();
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return mData.cbegin();
    }
    [[nodiscard]] const_iterator cend() const noexcept
    {
        return mData.cend();
    }

    /**
     * @brief Iterate over (handle, value) pairs.
     *
     * @return Range object for use in range-based for loops.
     *
     * @note Use when you need to pass handles to other systems.
     *
     * Example:
     * @code
     *   for (auto entry : map.entries()) {
     *       other_system.register(entry.handle);
     *       process(entry.value);
     *   }
     * @endcode
     */
    [[nodiscard]] EntryRange entries() noexcept
    {
        return EntryRange(this);
    }
    [[nodiscard]] ConstEntryRange entries() const noexcept
    {
        return ConstEntryRange(this);
    }

private:
    // =========================================================================
    // Data Members (Fat-P naming: mPascalCase)
    // =========================================================================

    std::vector<T, Allocator> mData;                  ///< Dense array of actual data
    std::vector<size_type, IndexAllocator> mEraseMap; ///< Maps data index -> slot index
    std::vector<Slot, SlotAllocator> mSlots;          ///< Sparse array of slots
    std::vector<size_type, IndexAllocator> mFreeList; ///< Free slot indices for reuse

    // EntryIterator needs access to internals
    friend class EntryIterator;
    friend class ConstEntryIterator;
};

// =============================================================================
// Non-Member Functions
// =============================================================================

/**
 * @brief Swap two SlotMaps.
 */
template <typename T, typename GenerationType, typename Allocator>
void swap(SlotMap<T, GenerationType, Allocator>& lhs,
          SlotMap<T, GenerationType, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

} // namespace fat_p

// =============================================================================
// std::hash Specialization for SlotMapHandleT
// =============================================================================

template <typename GenerationType>
struct std::hash<fat_p::SlotMapHandleT<GenerationType>>
{
    [[nodiscard]] std::size_t operator()(const fat_p::SlotMapHandleT<GenerationType>& h) const noexcept
    {
        // Hash combine: mix index and generation
        std::size_t seed = std::hash<std::uint32_t>{}(h.index);
        seed ^= std::hash<GenerationType>{}(h.generation) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
