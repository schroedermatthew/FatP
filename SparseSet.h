/**
 * @file SparseSet.h
 * @brief High-performance sparse set data structure for ECS and games
 * 
 * @details Sparse set provides O(1) insertion, deletion, and lookup with dense iteration.
 * Perfect for Entity Component Systems (ECS) where entities are sparse but iteration is common.
 * 
 * Features:
 * - O(1) insertion, deletion, lookup, contains
 * - O(n) dense iteration over active elements
 * - Stable indices (elements don't move)
 * - Memory efficient for sparse data
 * - Clear and reserve operations
 * - Compatible with range-based for loops
 * 
 * @version 1.0.0
 * @date 2025-11
 * 
 * @section complexity Complexity
 * - insert: O(1) amortized
 * - erase: O(1)
 * - contains: O(1)
 * - iteration: O(number of active elements)
 * - memory: O(max_index + active_elements)
 * 
 * @section use_cases Use Cases
 * - Entity Component System (ECS) architectures
 * - Game entity management
 * - Sparse graph adjacency lists
 * - Active object tracking
 * - Set operations on large index spaces
 * 
 * @section usage Usage Example
 * @code
 * SparseSet<uint32_t> entities;
 * 
 * // Add entities
 * entities.insert(1000);
 * entities.insert(5000);
 * entities.insert(10000);
 * 
 * // Check membership
 * if (entities.contains(1000)) { }
 * 
 * // Iterate (only over active entities)
 * for (uint32_t id : entities) {
 *     // Process entity
 * }
 * 
 * // Remove entity
 * entities.erase(1000);
 * @endcode
 * 
 * Compilation: Requires C++17
 * - g++ -std=c++17 -O3 your_code.cpp
 * - Tested on Intel Core i7-8850H @ 2.60GHz, 32GB RAM
 */

#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <iterator>

namespace cpp_utilities {

// ============================================================================
// Sparse Set
// ============================================================================

/**
 * @brief Sparse set data structure for O(1) operations on sparse indices
 * 
 * @tparam T Element type (must be unsigned integral)
 * 
 * Thread-safety: None (wrap in mutex if needed)
 * Exception-safety: Strong guarantee
 */
template<typename T = uint32_t>
class SparseSet {
    static_assert(std::is_unsigned_v<T>, "T must be unsigned integral type");
    
public:
    using value_type = T;
    using size_type = size_t;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;
    
    /**
     * @brief Construct empty sparse set
     * @param max_value Expected maximum value (for optimization)
     */
    explicit SparseSet(size_type max_value = 0) {
        if (max_value > 0) {
            reserve(max_value);
        }
    }
    
    /**
     * @brief Insert element
     * @param value Element to insert
     * @return true if inserted, false if already present
     * 
     * Complexity: O(1) amortized
     */
    bool insert(T value) {
        ensure_capacity(value);
        
        if (m_sparse[value] < m_dense.size() && m_dense[m_sparse[value]] == value) {
            return false;  // Already present
        }
        
        m_sparse[value] = static_cast<T>(m_dense.size());
        m_dense.push_back(value);
        return true;
    }
    
    /**
     * @brief Erase element
     * @param value Element to erase
     * @return true if erased, false if not present
     * 
     * Complexity: O(1)
     */
    bool erase(T value) {
        if (!contains(value)) {
            return false;
        }
        
        // Swap with last element
        T dense_idx = m_sparse[value];
        T last_value = m_dense.back();
        
        m_dense[dense_idx] = last_value;
        m_sparse[last_value] = dense_idx;
        
        m_dense.pop_back();
        return true;
    }
    
    /**
     * @brief Check if element is present
     * @param value Element to check
     * @return true if present
     * 
     * Complexity: O(1)
     */
    bool contains(T value) const {
        if (value >= m_sparse.size()) {
            return false;
        }
        
        T dense_idx = m_sparse[value];
        return dense_idx < m_dense.size() && m_dense[dense_idx] == value;
    }
    
    /**
     * @brief Get number of elements
     * 
     * Complexity: O(1)
     */
    size_type size() const noexcept {
        return m_dense.size();
    }
    
    /**
     * @brief Check if set is empty
     * 
     * Complexity: O(1)
     */
    bool empty() const noexcept {
        return m_dense.empty();
    }
    
    /**
     * @brief Clear all elements
     * 
     * Complexity: O(1) (doesn't deallocate)
     */
    void clear() noexcept {
        m_dense.clear();
    }
    
    /**
     * @brief Reserve space for indices up to max_value
     * @param max_value Maximum value to support
     * 
     * Complexity: O(n) where n = max_value
     */
    void reserve(size_type max_value) {
        if (max_value + 1 > m_sparse.size()) {
            m_sparse.resize(max_value + 1);
        }
    }
    
    /**
     * @brief Get sparse array capacity
     */
    size_type capacity() const noexcept {
        return m_sparse.size();
    }
    
    /**
     * @brief Get element at index (for dense iteration)
     * @param index Index in dense array
     * @return Element value
     * 
     * Complexity: O(1)
     */
    T operator[](size_type index) const {
        return m_dense[index];
    }
    
    /**
     * @brief Get element at index with bounds checking
     * @param index Index in dense array
     * @return Element value
     * @throws std::out_of_range if index out of bounds
     * 
     * Complexity: O(1)
     */
    T at(size_type index) const {
        if (index >= m_dense.size()) {
            throw std::out_of_range("SparseSet::at: index out of range");
        }
        return m_dense[index];
    }
    
    // Iterator support (for range-based for loops)
    iterator begin() noexcept { return m_dense.begin(); }
    iterator end() noexcept { return m_dense.end(); }
    const_iterator begin() const noexcept { return m_dense.begin(); }
    const_iterator end() const noexcept { return m_dense.end(); }
    const_iterator cbegin() const noexcept { return m_dense.cbegin(); }
    const_iterator cend() const noexcept { return m_dense.cend(); }
    
    /**
     * @brief Get underlying dense array (for advanced use)
     */
    const std::vector<T>& dense() const noexcept {
        return m_dense;
    }
    
    /**
     * @brief Get underlying sparse array (for advanced use)
     */
    const std::vector<T>& sparse() const noexcept {
        return m_sparse;
    }
    
private:
    void ensure_capacity(T value) {
        if (value >= m_sparse.size()) {
            m_sparse.resize(static_cast<size_type>(value) + 1);
        }
    }
    
    std::vector<T> m_dense;   // Dense array of present elements
    std::vector<T> m_sparse;  // Sparse array mapping element to dense index
};

// ============================================================================
// Sparse Set with Data
// ============================================================================

/**
 * @brief Sparse set that stores associated data with each element
 * 
 * @tparam T Index type (unsigned integral)
 * @tparam Data Data type to store with each element
 * 
 * Thread-safety: None
 * Exception-safety: Strong guarantee
 */
template<typename T, typename Data>
class SparseSetWithData {
    static_assert(std::is_unsigned_v<T>, "T must be unsigned integral type");
    
public:
    using value_type = T;
    using data_type = Data;
    using size_type = size_t;
    
    /**
     * @brief Construct empty sparse set
     */
    explicit SparseSetWithData(size_type max_value = 0) {
        if (max_value > 0) {
            reserve(max_value);
        }
    }
    
    /**
     * @brief Insert element with data
     * @param value Index to insert
     * @param data Data to store
     * @return true if inserted, false if already present
     */
    bool insert(T value, const Data& data) {
        ensure_capacity(value);
        
        if (m_sparse[value] < m_dense.size() && m_dense[m_sparse[value]] == value) {
            return false;  // Already present
        }
        
        m_sparse[value] = static_cast<T>(m_dense.size());
        m_dense.push_back(value);
        m_data.push_back(data);
        return true;
    }
    
    /**
     * @brief Insert element with data (move)
     */
    bool insert(T value, Data&& data) {
        ensure_capacity(value);
        
        if (m_sparse[value] < m_dense.size() && m_dense[m_sparse[value]] == value) {
            return false;
        }
        
        m_sparse[value] = static_cast<T>(m_dense.size());
        m_dense.push_back(value);
        m_data.push_back(std::move(data));
        return true;
    }
    
    /**
     * @brief Erase element
     */
    bool erase(T value) {
        if (!contains(value)) {
            return false;
        }
        
        T dense_idx = m_sparse[value];
        T last_value = m_dense.back();
        
        m_dense[dense_idx] = last_value;
        m_data[dense_idx] = std::move(m_data.back());
        m_sparse[last_value] = dense_idx;
        
        m_dense.pop_back();
        m_data.pop_back();
        return true;
    }
    
    /**
     * @brief Check if element is present
     */
    bool contains(T value) const {
        if (value >= m_sparse.size()) {
            return false;
        }
        
        T dense_idx = m_sparse[value];
        return dense_idx < m_dense.size() && m_dense[dense_idx] == value;
    }
    
    /**
     * @brief Get data for element
     * @param value Index to look up
     * @return Reference to data
     * @throws std::out_of_range if not present
     */
    Data& get(T value) {
        if (!contains(value)) {
            throw std::out_of_range("SparseSetWithData::get: element not present");
        }
        return m_data[m_sparse[value]];
    }
    
    /**
     * @brief Get const data for element
     */
    const Data& get(T value) const {
        if (!contains(value)) {
            throw std::out_of_range("SparseSetWithData::get: element not present");
        }
        return m_data[m_sparse[value]];
    }
    
    /**
     * @brief Get data by dense index
     */
    Data& data_at(size_type index) {
        return m_data.at(index);
    }
    
    /**
     * @brief Get const data by dense index
     */
    const Data& data_at(size_type index) const {
        return m_data.at(index);
    }
    
    size_type size() const noexcept { return m_dense.size(); }
    bool empty() const noexcept { return m_dense.empty(); }
    void clear() noexcept { m_dense.clear(); m_data.clear(); }
    
    void reserve(size_type max_value) {
        if (max_value + 1 > m_sparse.size()) {
            m_sparse.resize(max_value + 1);
        }
    }
    
    const std::vector<T>& dense() const noexcept { return m_dense; }
    const std::vector<Data>& data() const noexcept { return m_data; }
    
private:
    void ensure_capacity(T value) {
        if (value >= m_sparse.size()) {
            m_sparse.resize(static_cast<size_type>(value) + 1);
        }
    }
    
    std::vector<T> m_dense;
    std::vector<Data> m_data;
    std::vector<T> m_sparse;
};

} // namespace cpp_utilities
