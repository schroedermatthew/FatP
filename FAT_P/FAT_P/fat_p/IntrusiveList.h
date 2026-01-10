#pragma once

/*
FATP_META:
  meta_version: 1
  component: IntrusiveList
  file_role: public_header
  path: fat_p/IntrusiveList.h
  namespace: fat_p
  summary: "Public header for IntrusiveList."
  api_stability: in_work
  related:
    docs_search: "IntrusiveList"
    tests:
      - tests/test_IntrusiveList.cpp
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
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace fat_p {

// ============================================================================
// IntrusiveList - Zero-Allocation Linked List
// ============================================================================
//
// An intrusive linked list where objects contain their own links.
// No separate node allocations - objects manage themselves!
//
// Benefits:
// - Zero allocations for list management
// - Cache-friendly (nodes are your actual objects)
// - O(1) insert/remove with iterator
// - O(1) splice operations
// - Perfect for embedded systems and real-time code
//
// Usage:
//   struct MyObject : IntrusiveListNode<MyObject> {
//       int data;
//   };
//   
//   IntrusiveList<MyObject> list;
//   MyObject obj;
//   list.push_back(obj);
//   list.remove(obj);
//
// Important:
// - Objects must inherit from IntrusiveListNode<T>
// - Objects can only be in ONE list at a time
// - Object lifetime must exceed list operations
// ============================================================================

// Forward declarations
template<typename T>
class IntrusiveList;

template<typename T>
class IntrusiveListIterator;

// ============================================================================
// IntrusiveListNode - Base class for list nodes
// ============================================================================
template<typename T>
class IntrusiveListNode {
public:
    IntrusiveListNode() : prev_(nullptr), next_(nullptr) {}
    
    // Non-copyable (nodes are unique in list structure)
    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;
    
    // Check if node is linked in a list
    bool is_linked() const { return prev_ != nullptr || next_ != nullptr; }
    
protected:
    friend class IntrusiveList<T>;
    friend class IntrusiveListIterator<T>;
    
    IntrusiveListNode* prev_;
    IntrusiveListNode* next_;
};

// ============================================================================
// IntrusiveListIterator
// ============================================================================
template<typename T>
class IntrusiveListIterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    
    IntrusiveListIterator() : node_(nullptr) {}
    explicit IntrusiveListIterator(IntrusiveListNode<T>* node) : node_(node) {}
    
    reference operator*() const { return *static_cast<T*>(node_); }
    pointer operator->() const { return static_cast<T*>(node_); }
    
    IntrusiveListIterator& operator++() {
        node_ = node_->next_;
        return *this;
    }
    
    IntrusiveListIterator operator++(int) {
        IntrusiveListIterator tmp = *this;
        ++(*this);
        return tmp;
    }
    
    IntrusiveListIterator& operator--() {
        node_ = node_->prev_;
        return *this;
    }
    
    IntrusiveListIterator operator--(int) {
        IntrusiveListIterator tmp = *this;
        --(*this);
        return tmp;
    }
    
    bool operator==(const IntrusiveListIterator& other) const {
        return node_ == other.node_;
    }
    
    bool operator!=(const IntrusiveListIterator& other) const {
        return !(*this == other);
    }
    
private:
    friend class IntrusiveList<T>;
    IntrusiveListNode<T>* node_;
};

// ============================================================================
// Const iterator
// ============================================================================
template<typename T>
class IntrusiveListConstIterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = const T*;
    using reference = const T&;
    
    IntrusiveListConstIterator() : node_(nullptr) {}
    explicit IntrusiveListConstIterator(const IntrusiveListNode<T>* node) : node_(node) {}
    IntrusiveListConstIterator(const IntrusiveListIterator<T>& it) : node_(it.node_) {}
    
    reference operator*() const { return *static_cast<const T*>(node_); }
    pointer operator->() const { return static_cast<const T*>(node_); }
    
    IntrusiveListConstIterator& operator++() {
        node_ = node_->next_;
        return *this;
    }
    
    IntrusiveListConstIterator operator++(int) {
        IntrusiveListConstIterator tmp = *this;
        ++(*this);
        return tmp;
    }
    
    IntrusiveListConstIterator& operator--() {
        node_ = node_->prev_;
        return *this;
    }
    
    IntrusiveListConstIterator operator--(int) {
        IntrusiveListConstIterator tmp = *this;
        --(*this);
        return tmp;
    }
    
    bool operator==(const IntrusiveListConstIterator& other) const {
        return node_ == other.node_;
    }
    
    bool operator!=(const IntrusiveListConstIterator& other) const {
        return !(*this == other);
    }
    
private:
    friend class IntrusiveList<T>;
    const IntrusiveListNode<T>* node_;
};

// ============================================================================
// IntrusiveList
// ============================================================================
template<typename T>
class IntrusiveList {
public:
    static_assert(std::is_base_of_v<IntrusiveListNode<T>, T>,
                  "T must inherit from IntrusiveListNode<T>");
    
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using iterator = IntrusiveListIterator<T>;
    using const_iterator = IntrusiveListConstIterator<T>;
    using size_type = std::size_t;
    
    IntrusiveList() : head_(nullptr), tail_(nullptr), size_(0) {}
    
    ~IntrusiveList() { clear(); }
    
    // Non-copyable (would require cloning objects)
    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;
    
    // Moveable
    IntrusiveList(IntrusiveList&& other) noexcept 
        : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }
    
    IntrusiveList& operator=(IntrusiveList&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            other.head_ = nullptr;
            other.tail_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    
    // Size queries
    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }
    
    // Element access
    reference front() { return *static_cast<T*>(head_); }
    const_reference front() const { return *static_cast<const T*>(head_); }
    
    reference back() { return *static_cast<T*>(tail_); }
    const_reference back() const { return *static_cast<const T*>(tail_); }
    
    // Iterators
    iterator begin() { return iterator(head_); }
    iterator end() { return iterator(nullptr); }
    
    const_iterator begin() const { return const_iterator(head_); }
    const_iterator end() const { return const_iterator(nullptr); }
    
    const_iterator cbegin() const { return const_iterator(head_); }
    const_iterator cend() const { return const_iterator(nullptr); }
    
    // Modifiers
    void push_front(T& node) {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);
        
        n->prev_ = nullptr;
        n->next_ = head_;
        
        if (head_) {
            head_->prev_ = n;
        } else {
            tail_ = n;
        }
        
        head_ = n;
        ++size_;
    }
    
    void push_back(T& node) {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);
        
        n->prev_ = tail_;
        n->next_ = nullptr;
        
        if (tail_) {
            tail_->next_ = n;
        } else {
            head_ = n;
        }
        
        tail_ = n;
        ++size_;
    }
    
    void pop_front() {
        if (!head_) return;
        
        auto* node = head_;
        head_ = head_->next_;
        
        if (head_) {
            head_->prev_ = nullptr;
        } else {
            tail_ = nullptr;
        }
        
        node->prev_ = nullptr;
        node->next_ = nullptr;
        --size_;
    }
    
    void pop_back() {
        if (!tail_) return;
        
        auto* node = tail_;
        tail_ = tail_->prev_;
        
        if (tail_) {
            tail_->next_ = nullptr;
        } else {
            head_ = nullptr;
        }
        
        node->prev_ = nullptr;
        node->next_ = nullptr;
        --size_;
    }
    
    // Insert before position
    iterator insert(iterator pos, T& node) {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);
        auto* pos_node = pos.node_;
        
        if (!pos_node) {
            // Insert at end
            push_back(node);
            return iterator(n);
        }
        
        n->next_ = pos_node;
        n->prev_ = pos_node->prev_;
        
        if (pos_node->prev_) {
            pos_node->prev_->next_ = n;
        } else {
            head_ = n;
        }
        
        pos_node->prev_ = n;
        ++size_;
        
        return iterator(n);
    }
    
    // Remove specific node
    void remove(T& node) {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);
        
        if (!n->is_linked() && n != head_ && n != tail_) {
            return;  // Not in list
        }
        
        if (n->prev_) {
            n->prev_->next_ = n->next_;
        } else {
            head_ = n->next_;
        }
        
        if (n->next_) {
            n->next_->prev_ = n->prev_;
        } else {
            tail_ = n->prev_;
        }
        
        n->prev_ = nullptr;
        n->next_ = nullptr;
        --size_;
    }
    
    // Erase at iterator
    iterator erase(iterator pos) {
        if (pos == end()) return end();
        
        auto* node = pos.node_;
        iterator next(node->next_);
        
        remove(*static_cast<T*>(node));
        
        return next;
    }
    
    // Clear list (unlinks all nodes)
    void clear() {
        auto* node = head_;
        while (node) {
            auto* next = node->next_;
            node->prev_ = nullptr;
            node->next_ = nullptr;
            node = next;
        }
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }
    
    // Splice - move elements from other list
    void splice(iterator pos, IntrusiveList& other) {
        if (other.empty()) return;
        
        if (pos.node_ == nullptr) {
            // Splice at end
            if (tail_) {
                tail_->next_ = other.head_;
                other.head_->prev_ = tail_;
            } else {
                head_ = other.head_;
            }
            tail_ = other.tail_;
        } else {
            // Splice before pos
            auto* pos_node = pos.node_;
            
            other.tail_->next_ = pos_node;
            other.head_->prev_ = pos_node->prev_;
            
            if (pos_node->prev_) {
                pos_node->prev_->next_ = other.head_;
            } else {
                head_ = other.head_;
            }
            
            pos_node->prev_ = other.tail_;
        }
        
        size_ += other.size_;
        
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }
    
private:
    IntrusiveListNode<T>* head_;
    IntrusiveListNode<T>* tail_;
    size_type size_;
};

} // namespace fat_p
