#pragma once

#include <vector>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace cpp_utilities {

// ============================================================================
// SlotMap - Generational Index Container
// ============================================================================
// 
// A container that provides stable indices even after element removal.
// Uses generational counters to detect dangling/stale handles.
//
// Perfect for:
// - Entity systems (game engines)
// - Resource management without shared_ptr overhead
// - Safe handles that detect use-after-free
// - Cache-friendly dense storage
//
// Performance:
// - Insert: O(1)
// - Remove: O(1)
// - Access: O(1)
// - Dense storage for iteration
// - No allocations after reserve()
//
// Example:
//   SlotMap<Enemy> enemies;
//   auto handle = enemies.insert(Enemy{100, 50});
//   Enemy* ptr = enemies.get(handle);
//   enemies.erase(handle);
//   ptr = enemies.get(handle);  // Returns nullptr (safe!)
// ============================================================================

template<typename T>
class SlotMap {
public:
    using value_type = T;
    using size_type = uint32_t;
    using generation_type = uint32_t;
    
    // Handle contains index + generation for safe access
    struct Handle {
        size_type index{0};
        generation_type generation{0};
        
        constexpr Handle() = default;
        constexpr Handle(size_type idx, generation_type gen) 
            : index(idx), generation(gen) {}
        
        constexpr bool operator==(const Handle& other) const {
            return index == other.index && generation == other.generation;
        }
        
        constexpr bool operator!=(const Handle& other) const {
            return !(*this == other);
        }
        
        constexpr bool is_valid() const {
            return generation != 0;
        }
    };
    
    // Iterator for dense data iteration
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        
        Iterator(std::vector<T>* data, size_type idx) 
            : data_(data), index_(idx) {}
        
        reference operator*() const { return (*data_)[index_]; }
        pointer operator->() const { return &(*data_)[index_]; }
        
        Iterator& operator++() { ++index_; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        
        bool operator==(const Iterator& other) const { 
            return index_ == other.index_; 
        }
        bool operator!=(const Iterator& other) const { 
            return !(*this == other); 
        }
        
    private:
        std::vector<T>* data_;
        size_type index_;
    };
    
    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = const T*;
        using reference = const T&;
        
        ConstIterator(const std::vector<T>* data, size_type idx) 
            : data_(data), index_(idx) {}
        
        reference operator*() const { return (*data_)[index_]; }
        pointer operator->() const { return &(*data_)[index_]; }
        
        ConstIterator& operator++() { ++index_; return *this; }
        ConstIterator operator++(int) { ConstIterator tmp = *this; ++(*this); return tmp; }
        
        bool operator==(const ConstIterator& other) const { 
            return index_ == other.index_; 
        }
        bool operator!=(const ConstIterator& other) const { 
            return !(*this == other); 
        }
        
    private:
        const std::vector<T>* data_;
        size_type index_;
    };
    
    SlotMap() = default;
    ~SlotMap() = default;
    
    SlotMap(const SlotMap&) = default;
    SlotMap& operator=(const SlotMap&) = default;
    
    SlotMap(SlotMap&&) noexcept = default;
    SlotMap& operator=(SlotMap&&) noexcept = default;
    
    // Insert element and return handle
    template<typename... Args>
    Handle insert(Args&&... args) {
        size_type slot_index;
        
        if (!free_list_.empty()) {
            // Reuse a free slot
            slot_index = free_list_.back();
            free_list_.pop_back();
        } else {
            // Allocate new slot
            slot_index = static_cast<size_type>(slots_.size());
            slots_.emplace_back();
        }
        
        Slot& slot = slots_[slot_index];
        ++slot.generation;  // Increment generation
        
        // Add to dense data array
        size_type data_index = static_cast<size_type>(data_.size());
        data_.emplace_back(std::forward<Args>(args)...);
        erase_map_.push_back(slot_index);
        
        // Update slot to point to data
        slot.data_index = data_index;
        
        return Handle{slot_index, slot.generation};
    }
    
    // Erase element by handle
    bool erase(Handle handle) {
        if (!is_valid(handle)) {
            return false;
        }
        
        Slot& slot = slots_[handle.index];
        size_type data_index = slot.data_index;
        
        // Swap-and-pop from dense array
        if (data_index < data_.size() - 1) {
            std::swap(data_[data_index], data_.back());
            std::swap(erase_map_[data_index], erase_map_.back());
            
            // Update the swapped element's slot
            slots_[erase_map_[data_index]].data_index = data_index;
        }
        
        data_.pop_back();
        erase_map_.pop_back();
        
        // Invalidate slot and add to free list
        ++slot.generation;  // Invalidate old handles
        slot.data_index = std::numeric_limits<size_type>::max();
        free_list_.push_back(handle.index);
        
        return true;
    }
    
    // Get pointer to element (nullptr if invalid)
    T* get(Handle handle) {
        if (!is_valid(handle)) {
            return nullptr;
        }
        return &data_[slots_[handle.index].data_index];
    }
    
    const T* get(Handle handle) const {
        if (!is_valid(handle)) {
            return nullptr;
        }
        return &data_[slots_[handle.index].data_index];
    }
    
    // Check if handle is valid
    bool is_valid(Handle handle) const {
        if (handle.index >= slots_.size()) {
            return false;
        }
        return slots_[handle.index].generation == handle.generation;
    }
    
    // Clear all elements
    void clear() {
        data_.clear();
        erase_map_.clear();
        slots_.clear();
        free_list_.clear();
    }
    
    // Reserve capacity
    void reserve(size_type capacity) {
        data_.reserve(capacity);
        erase_map_.reserve(capacity);
        slots_.reserve(capacity);
    }
    
    // Size queries
    size_type size() const { return static_cast<size_type>(data_.size()); }
    size_type capacity() const { return static_cast<size_type>(data_.capacity()); }
    bool empty() const { return data_.empty(); }
    
    // Iteration over dense data
    Iterator begin() { return Iterator(&data_, 0); }
    Iterator end() { return Iterator(&data_, static_cast<size_type>(data_.size())); }
    
    ConstIterator begin() const { return ConstIterator(&data_, 0); }
    ConstIterator end() const { return ConstIterator(&data_, static_cast<size_type>(data_.size())); }
    
    ConstIterator cbegin() const { return begin(); }
    ConstIterator cend() const { return end(); }
    
private:
    struct Slot {
        generation_type generation{0};  // 0 = never used
        size_type data_index{0};        // Index into dense data array
    };
    
    std::vector<T> data_;              // Dense array of actual data
    std::vector<size_type> erase_map_; // Maps data index -> slot index
    std::vector<Slot> slots_;          // Sparse array of slots
    std::vector<size_type> free_list_; // Free slot indices
};

} // namespace cpp_utilities
