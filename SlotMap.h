#pragma once

#include <vector>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include "FatPTypeTraits.h"

namespace fat_p {

// =============================================================================
// SlotMap - Generational Index Container
// =============================================================================
//
// A container that provides stable handles even after element removal.
// Uses generational counters to detect stale/dangling handles (ABA safety).
//
// Architecture:
//   Handle = { index, generation }
//      │
//      ▼
//   slots_[index] = { generation, data_index }
//      │                   │
//      │ generation match? │
//      ▼                   ▼
//   Valid access       data_[data_index] = actual value
//
// Key properties:
// - Insert: O(1) amortized
// - Remove: O(1) via swap-and-pop
// - Access: O(1) with two array lookups
// - Iteration: O(n) over dense storage (cache-friendly)
// - ABA-safe: Stale handles return nullptr, never wrong data
//
// Use cases:
// - Entity-Component Systems (game engines)
// - Resource pools without shared_ptr overhead
// - Any scenario requiring safe handles to deletable objects
//
// Example:
//   SlotMap<Enemy> enemies;
//   auto handle = enemies.insert(Enemy{100, 50});
//   Enemy* ptr = enemies.get(handle);       // Valid access
//   enemies.erase(handle);
//   ptr = enemies.get(handle);              // Returns nullptr (safe!)
//   auto new_handle = enemies.insert(...);  // May reuse same slot
//   enemies.get(handle);                    // Still nullptr (generation changed)
//
// =============================================================================

template<typename T>
class SlotMap
{
public:
    using value_type = T;
    using size_type = uint32_t;
    using generation_type = uint32_t;

    // =========================================================================
    // Handle - Opaque reference to a slot
    // =========================================================================
    //
    // Encodes both the slot index and generation counter. Two handles to the
    // same index but different generations refer to different objects.
    //
    // Note: Handle validity can only be determined by querying the SlotMap.
    // The is_null() method only checks if the handle was default-constructed.
    //
    struct Handle
    {
        size_type index{0};
        generation_type generation{0};

        constexpr Handle() = default;

        constexpr Handle(size_type idx, generation_type gen)
            : index(idx)
            , generation(gen)
        {
        }

        constexpr bool operator==(const Handle& other) const
        {
            return index == other.index && generation == other.generation;
        }

        constexpr bool operator!=(const Handle& other) const
        {
            return !(*this == other);
        }

        /// Returns true if this handle was default-constructed (never assigned).
        /// Note: A non-null handle may still be invalid if the slot was erased.
        /// Use SlotMap::is_valid(handle) to check actual validity.
        constexpr bool is_null() const
        {
            return generation == 0;
        }

        /// Returns true if this handle has been assigned (not default-constructed).
        /// Note: This does NOT guarantee the handle is valid in any SlotMap.
        constexpr explicit operator bool() const
        {
            return !is_null();
        }
    };

    // =========================================================================
    // Entry - Handle + reference pair for entries() iteration
    // =========================================================================

    struct Entry
    {
        Handle handle;
        T& value;

        Entry(Handle h, T& v)
            : handle(h)
            , value(v)
        {
        }
    };

    struct ConstEntry
    {
        Handle handle;
        const T& value;

        ConstEntry(Handle h, const T& v)
            : handle(h)
            , value(v)
        {
        }
    };

    // =========================================================================
    // Iterators
    // =========================================================================

    /// Iterator over dense data array (values only)
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        Iterator(std::vector<T>* data, size_type idx)
            : data_(data)
            , index_(idx)
        {
        }

        reference operator*() const { return (*data_)[index_]; }
        pointer operator->() const { return &(*data_)[index_]; }

        Iterator& operator++()
        {
            ++index_;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const { return index_ == other.index_; }
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        std::vector<T>* data_;
        size_type index_;
    };

    /// Const iterator over dense data array (values only)
    class ConstIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = const T*;
        using reference = const T&;

        ConstIterator(const std::vector<T>* data, size_type idx)
            : data_(data)
            , index_(idx)
        {
        }

        reference operator*() const { return (*data_)[index_]; }
        pointer operator->() const { return &(*data_)[index_]; }

        ConstIterator& operator++()
        {
            ++index_;
            return *this;
        }

        ConstIterator operator++(int)
        {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const ConstIterator& other) const { return index_ == other.index_; }
        bool operator!=(const ConstIterator& other) const { return !(*this == other); }

    private:
        const std::vector<T>* data_;
        size_type index_;
    };

    /// Iterator over entries (handle + value pairs)
    class EntryIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Entry;
        using pointer = Entry*;
        using reference = Entry;

        EntryIterator(SlotMap* map, size_type idx)
            : map_(map)
            , index_(idx)
        {
        }

        Entry operator*() const
        {
            size_type slot_index = map_->erase_map_[index_];
            const Slot& slot = map_->slots_[slot_index];
            return Entry{Handle{slot_index, slot.generation}, map_->data_[index_]};
        }

        EntryIterator& operator++()
        {
            ++index_;
            return *this;
        }

        EntryIterator operator++(int)
        {
            EntryIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const EntryIterator& other) const { return index_ == other.index_; }
        bool operator!=(const EntryIterator& other) const { return !(*this == other); }

    private:
        SlotMap* map_;
        size_type index_;
    };

    /// Const iterator over entries (handle + value pairs)
    class ConstEntryIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = ConstEntry;
        using pointer = ConstEntry*;
        using reference = ConstEntry;

        ConstEntryIterator(const SlotMap* map, size_type idx)
            : map_(map)
            , index_(idx)
        {
        }

        ConstEntry operator*() const
        {
            size_type slot_index = map_->erase_map_[index_];
            const Slot& slot = map_->slots_[slot_index];
            return ConstEntry{Handle{slot_index, slot.generation}, map_->data_[index_]};
        }

        ConstEntryIterator& operator++()
        {
            ++index_;
            return *this;
        }

        ConstEntryIterator operator++(int)
        {
            ConstEntryIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const ConstEntryIterator& other) const { return index_ == other.index_; }
        bool operator!=(const ConstEntryIterator& other) const { return !(*this == other); }

    private:
        const SlotMap* map_;
        size_type index_;
    };

    /// Range wrapper for entries() iteration
    class EntryRange
    {
    public:
        EntryRange(SlotMap* map)
            : map_(map)
        {
        }

        EntryIterator begin() { return EntryIterator(map_, 0); }
        EntryIterator end()
        {
            return EntryIterator(map_, static_cast<size_type>(map_->data_.size()));
        }

    private:
        SlotMap* map_;
    };

    /// Const range wrapper for entries() iteration
    class ConstEntryRange
    {
    public:
        ConstEntryRange(const SlotMap* map)
            : map_(map)
        {
        }

        ConstEntryIterator begin() const { return ConstEntryIterator(map_, 0); }
        ConstEntryIterator end() const
        {
            return ConstEntryIterator(map_, static_cast<size_type>(map_->data_.size()));
        }

    private:
        const SlotMap* map_;
    };

    // =========================================================================
    // Construction / Assignment
    // =========================================================================

    SlotMap() = default;
    ~SlotMap() = default;

    SlotMap(const SlotMap&) = default;
    SlotMap& operator=(const SlotMap&) = default;

    SlotMap(SlotMap&&) noexcept = default;
    SlotMap& operator=(SlotMap&&) noexcept = default;

    // =========================================================================
    // Modifiers
    // =========================================================================

    /// Insert element and return handle.
    /// Supports in-place construction via forwarding.
    template<typename... Args>
    Handle insert(Args&&... args)
    {
        size_type slot_index;

        if (!free_list_.empty())
        {
            // Reuse a free slot
            slot_index = free_list_.back();
            free_list_.pop_back();
        }
        else
        {
            // Allocate new slot
            slot_index = static_cast<size_type>(slots_.size());
            slots_.emplace_back();
        }

        Slot& slot = slots_[slot_index];
        ++slot.generation; // Increment generation (wraps at max, see notes)

        // Add to dense data array
        size_type data_index = static_cast<size_type>(data_.size());
        data_.emplace_back(std::forward<Args>(args)...);
        erase_map_.push_back(slot_index);

        // Update slot to point to data
        slot.data_index = data_index;

        return Handle{slot_index, slot.generation};
    }

    /// Erase element by handle.
    /// Returns true if element was erased, false if handle was invalid.
    bool erase(Handle handle)
    {
        if (!is_valid(handle))
        {
            return false;
        }

        Slot& slot = slots_[handle.index];
        size_type data_index = slot.data_index;

        // Swap-and-pop from dense array
        if (data_index < data_.size() - 1)
        {
            std::swap(data_[data_index], data_.back());
            std::swap(erase_map_[data_index], erase_map_.back());

            // Update the swapped element's slot
            slots_[erase_map_[data_index]].data_index = data_index;
        }

        data_.pop_back();
        erase_map_.pop_back();

        // Invalidate slot and add to free list
        ++slot.generation; // Invalidate old handles
        slot.data_index = std::numeric_limits<size_type>::max();
        free_list_.push_back(handle.index);

        return true;
    }

    /// Clear all elements. Handles become invalid.
    void clear()
    {
        data_.clear();
        erase_map_.clear();
        slots_.clear();
        free_list_.clear();
    }

    /// Reserve capacity for n elements (reduces allocations).
    void reserve(size_type capacity)
    {
        data_.reserve(capacity);
        erase_map_.reserve(capacity);
        slots_.reserve(capacity);
        free_list_.reserve(capacity);
    }

    // =========================================================================
    // Access
    // =========================================================================

    /// Get pointer to element (nullptr if handle is invalid).
    [[nodiscard]] T* get(Handle handle)
    {
        if (!is_valid(handle))
        {
            return nullptr;
        }
        return &data_[slots_[handle.index].data_index];
    }

    /// Get const pointer to element (nullptr if handle is invalid).
    [[nodiscard]] const T* get(Handle handle) const
    {
        if (!is_valid(handle))
        {
            return nullptr;
        }
        return &data_[slots_[handle.index].data_index];
    }

    /// Get reference to element WITHOUT validity check.
    /// Undefined behavior if handle is invalid. Use only when validity is
    /// guaranteed by surrounding logic (e.g., iterating known-valid handles).
    /// For HPC tight loops where the ~3ns validation overhead matters.
    [[nodiscard]] T& get_unchecked(Handle handle)
    {
        return data_[slots_[handle.index].data_index];
    }

    /// Get const reference to element WITHOUT validity check.
    /// Undefined behavior if handle is invalid.
    [[nodiscard]] const T& get_unchecked(Handle handle) const
    {
        return data_[slots_[handle.index].data_index];
    }

    /// Check if handle refers to a valid element.
    /// This is the authoritative validity check.
    [[nodiscard]] bool is_valid(Handle handle) const
    {
        if (handle.index >= slots_.size())
        {
            return false;
        }
        return slots_[handle.index].generation == handle.generation;
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Number of elements currently stored.
    [[nodiscard]] size_type size() const
    {
        return static_cast<size_type>(data_.size());
    }

    /// Current capacity (elements before reallocation).
    [[nodiscard]] size_type capacity() const
    {
        return static_cast<size_type>(data_.capacity());
    }

    /// True if container is empty.
    [[nodiscard]] bool empty() const { return data_.empty(); }

    /// Number of slots ever allocated (including free slots).
    [[nodiscard]] size_type slot_count() const
    {
        return static_cast<size_type>(slots_.size());
    }

    /// Number of free slots available for reuse.
    [[nodiscard]] size_type free_slot_count() const
    {
        return static_cast<size_type>(free_list_.size());
    }

    // =========================================================================
    // Iteration
    // =========================================================================

    /// Iterate over values only (dense, cache-friendly).
    Iterator begin() { return Iterator(&data_, 0); }
    Iterator end() { return Iterator(&data_, static_cast<size_type>(data_.size())); }

    ConstIterator begin() const { return ConstIterator(&data_, 0); }
    ConstIterator end() const
    {
        return ConstIterator(&data_, static_cast<size_type>(data_.size()));
    }

    ConstIterator cbegin() const { return begin(); }
    ConstIterator cend() const { return end(); }

    /// Iterate over (handle, value) pairs.
    /// Use when you need to pass handles to other systems.
    ///
    /// Example:
    ///   for (auto entry : map.entries()) {
    ///       other_system.register(entry.handle);
    ///       process(entry.value);
    ///   }
    EntryRange entries() { return EntryRange(this); }
    ConstEntryRange entries() const { return ConstEntryRange(this); }

private:
    struct Slot
    {
        generation_type generation{0}; // 0 = never used, incremented on insert/erase
        size_type data_index{0};       // Index into dense data array
    };

    std::vector<T> data_;              // Dense array of actual data
    std::vector<size_type> erase_map_; // Maps data index -> slot index
    std::vector<Slot> slots_;          // Sparse array of slots
    std::vector<size_type> free_list_; // Free slot indices for reuse

    // EntryIterator needs access to internals
    friend class EntryIterator;
    friend class ConstEntryIterator;
};

template<typename T>
struct is_slot_map<SlotMap<T>> : std::true_type
{
};

} // namespace fat_p
