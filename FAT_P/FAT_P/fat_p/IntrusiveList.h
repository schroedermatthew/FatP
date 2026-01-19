#pragma once

/*
FATP_META:
  meta_version: 1
  component: IntrusiveList
  file_role: public_header
  path: fat_p/IntrusiveList.h
  namespace: fat_p
  layer: Containers
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

/**
 * @file IntrusiveList.h
 * @brief Intrusive doubly-linked list with zero allocation overhead
 */

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <cassert>

// Debug assertion macro - enabled only in debug builds
#ifdef NDEBUG
    #define FATP_DEBUG_ASSERT(condition, message) ((void)0)
#else
    #define FATP_DEBUG_ASSERT(condition, message) assert((condition) && (message))
#endif

namespace fat_p
{

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
template <typename T>
class IntrusiveList;

template <typename T>
class IntrusiveListIterator;

template <typename T>
class IntrusiveListConstIterator;

// ============================================================================
// IntrusiveListNode - Base class for list nodes
// ============================================================================
template <typename T>
class IntrusiveListNode
{
public:
    IntrusiveListNode()
        : mPrev(nullptr)
        , mNext(nullptr)
        , mOwner(nullptr)
    {
    }

    // Non-copyable (nodes are unique in list structure)
    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;

    /**
     * @brief Check if node is in a list
     * @return true if node belongs to any IntrusiveList
     */
    [[nodiscard]] bool is_linked() const noexcept
    {
        return mOwner != nullptr;
    }

protected:
    friend class IntrusiveList<T>;
    friend class IntrusiveListIterator<T>;
    friend class IntrusiveListConstIterator<T>;

    IntrusiveListNode* mPrev;
    IntrusiveListNode* mNext;
    IntrusiveList<T>* mOwner;  // Tracks which list owns this node
};

// ============================================================================
// IntrusiveListIterator
// ============================================================================
template <typename T>
class IntrusiveListIterator
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    IntrusiveListIterator()
        : mNode(nullptr)
    {
    }
    explicit IntrusiveListIterator(IntrusiveListNode<T>* node)
        : mNode(node)
    {
    }

    reference operator*() const
    {
        return *static_cast<T*>(mNode);
    }
    pointer operator->() const
    {
        return static_cast<T*>(mNode);
    }

    IntrusiveListIterator& operator++()
    {
        mNode = mNode->mNext;
        return *this;
    }

    IntrusiveListIterator operator++(int)
    {
        IntrusiveListIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    /**
     * @brief Decrement iterator
     * 
     * @warning Decrementing end() is UNDEFINED BEHAVIOR.
     *          This iterator does NOT fully satisfy BidirectionalIterator
     *          requirements when used with end().
     */
    IntrusiveListIterator& operator--()
    {
        mNode = mNode->mPrev;
        return *this;
    }

    IntrusiveListIterator operator--(int)
    {
        IntrusiveListIterator tmp = *this;
        --(*this);
        return tmp;
    }

    bool operator==(const IntrusiveListIterator& other) const
    {
        return mNode == other.mNode;
    }

    bool operator!=(const IntrusiveListIterator& other) const
    {
        return !(*this == other);
    }

private:
    friend class IntrusiveList<T>;
    friend class IntrusiveListConstIterator<T>;  // Allow const_iterator conversion
    IntrusiveListNode<T>* mNode;
};

// ============================================================================
// Const iterator
// ============================================================================
template <typename T>
class IntrusiveListConstIterator
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = const T*;
    using reference = const T&;

    IntrusiveListConstIterator()
        : mNode(nullptr)
    {
    }
    explicit IntrusiveListConstIterator(const IntrusiveListNode<T>* node)
        : mNode(node)
    {
    }
    IntrusiveListConstIterator(const IntrusiveListIterator<T>& it)
        : mNode(it.mNode)
    {
    }

    reference operator*() const
    {
        return *static_cast<const T*>(mNode);
    }
    pointer operator->() const
    {
        return static_cast<const T*>(mNode);
    }

    IntrusiveListConstIterator& operator++()
    {
        mNode = mNode->mNext;
        return *this;
    }

    IntrusiveListConstIterator operator++(int)
    {
        IntrusiveListConstIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    IntrusiveListConstIterator& operator--()
    {
        mNode = mNode->mPrev;
        return *this;
    }

    IntrusiveListConstIterator operator--(int)
    {
        IntrusiveListConstIterator tmp = *this;
        --(*this);
        return tmp;
    }

    bool operator==(const IntrusiveListConstIterator& other) const
    {
        return mNode == other.mNode;
    }

    bool operator!=(const IntrusiveListConstIterator& other) const
    {
        return !(*this == other);
    }

private:
    friend class IntrusiveList<T>;
    const IntrusiveListNode<T>* mNode;
};

// ============================================================================
// IntrusiveList
// ============================================================================
template <typename T>
class IntrusiveList
{
public:
    static_assert(std::is_base_of_v<IntrusiveListNode<T>, T>, "T must inherit from IntrusiveListNode<T>");

    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using iterator = IntrusiveListIterator<T>;
    using const_iterator = IntrusiveListConstIterator<T>;
    using size_type = std::size_t;

    IntrusiveList()
        : mHead(nullptr)
        , mTail(nullptr)
        , size_(0)
    {
    }

    ~IntrusiveList()
    {
        clear();
    }

    // Non-copyable (would require cloning objects)
    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    // Moveable
    IntrusiveList(IntrusiveList&& other) noexcept
        : mHead(other.mHead)
        , mTail(other.mTail)
        , size_(other.size_)
    {
        // Update ownership for all moved nodes
        auto* node = mHead;
        while (node)
        {
            node->mOwner = this;
            node = node->mNext;
        }
        
        other.mHead = nullptr;
        other.mTail = nullptr;
        other.size_ = 0;
    }

    IntrusiveList& operator=(IntrusiveList&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            mHead = other.mHead;
            mTail = other.mTail;
            size_ = other.size_;
            
            // Update ownership for all moved nodes
            auto* node = mHead;
            while (node)
            {
                node->mOwner = this;
                node = node->mNext;
            }
            
            other.mHead = nullptr;
            other.mTail = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // Size queries
    [[nodiscard]] bool empty() const noexcept
    {
        return size_ == 0;
    }
    [[nodiscard]] size_type size() const noexcept
    {
        return size_;
    }

    // Element access
    [[nodiscard]] reference front()
    {
        FATP_DEBUG_ASSERT(mHead != nullptr, "front() called on empty list");
        return *static_cast<T*>(mHead);
    }
    [[nodiscard]] const_reference front() const
    {
        FATP_DEBUG_ASSERT(mHead != nullptr, "front() called on empty list");
        return *static_cast<const T*>(mHead);
    }

    [[nodiscard]] reference back()
    {
        FATP_DEBUG_ASSERT(mTail != nullptr, "back() called on empty list");
        return *static_cast<T*>(mTail);
    }
    [[nodiscard]] const_reference back() const
    {
        FATP_DEBUG_ASSERT(mTail != nullptr, "back() called on empty list");
        return *static_cast<const T*>(mTail);
    }

    // Iterators
    [[nodiscard]] iterator begin() noexcept
    {
        return iterator(mHead);
    }
    [[nodiscard]] iterator end() noexcept
    {
        return iterator(nullptr);
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return const_iterator(mHead);
    }
    [[nodiscard]] const_iterator end() const noexcept
    {
        return const_iterator(nullptr);
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return const_iterator(mHead);
    }
    [[nodiscard]] const_iterator cend() const noexcept
    {
        return const_iterator(nullptr);
    }

    // Modifiers
    void push_front(T& node)
    {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);

        n->mOwner = this;  // Set ownership
        n->mPrev = nullptr;
        n->mNext = mHead;

        if (mHead)
        {
            mHead->mPrev = n;
        }
        else
        {
            mTail = n;
        }

        mHead = n;
        ++size_;
    }

    void push_back(T& node)
    {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);

        n->mOwner = this;  // Set ownership
        n->mPrev = mTail;
        n->mNext = nullptr;

        if (mTail)
        {
            mTail->mNext = n;
        }
        else
        {
            mHead = n;
        }

        mTail = n;
        ++size_;
    }

    void pop_front()
    {
        if (!mHead)
        {
            return;
        }

        auto* node = mHead;
        mHead = mHead->mNext;

        if (mHead)
        {
            mHead->mPrev = nullptr;
        }
        else
        {
            mTail = nullptr;
        }

        node->mOwner = nullptr;  // Clear ownership
        node->mPrev = nullptr;
        node->mNext = nullptr;
        --size_;
    }

    void pop_back()
    {
        if (!mTail)
        {
            return;
        }

        auto* node = mTail;
        mTail = mTail->mPrev;

        if (mTail)
        {
            mTail->mNext = nullptr;
        }
        else
        {
            mHead = nullptr;
        }

        node->mOwner = nullptr;  // Clear ownership
        node->mPrev = nullptr;
        node->mNext = nullptr;
        --size_;
    }

    // Insert before position
    iterator insert(iterator pos, T& node)
    {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);
        auto* pos_node = pos.mNode;

        if (!pos_node)
        {
            // Insert at end
            push_back(node);
            return iterator(n);
        }

        n->mOwner = this;  // Set ownership
        n->mNext = pos_node;
        n->mPrev = pos_node->mPrev;

        if (pos_node->mPrev)
        {
            pos_node->mPrev->mNext = n;
        }
        else
        {
            mHead = n;
        }

        pos_node->mPrev = n;
        ++size_;

        return iterator(n);
    }

    // Remove specific node
    void remove(T& node)
    {
        auto* n = static_cast<IntrusiveListNode<T>*>(&node);

        // Cross-list protection: only remove if node belongs to THIS list
        if (n->mOwner != this)
        {
            return;  // Not in this list - safe no-op
        }

        if (n->mPrev)
        {
            n->mPrev->mNext = n->mNext;
        }
        else
        {
            mHead = n->mNext;
        }

        if (n->mNext)
        {
            n->mNext->mPrev = n->mPrev;
        }
        else
        {
            mTail = n->mPrev;
        }

        n->mOwner = nullptr;  // Clear ownership
        n->mPrev = nullptr;
        n->mNext = nullptr;
        --size_;
    }

    // Erase at iterator
    iterator erase(iterator pos)
    {
        if (pos == end())
        {
            return end();
        }

        auto* node = pos.mNode;
        iterator next(node->mNext);

        remove(*static_cast<T*>(node));

        return next;
    }

    // Clear list (unlinks all nodes)
    void clear()
    {
        auto* node = mHead;
        while (node)
        {
            auto* next = node->mNext;
            node->mOwner = nullptr;  // Clear ownership
            node->mPrev = nullptr;
            node->mNext = nullptr;
            node = next;
        }
        mHead = nullptr;
        mTail = nullptr;
        size_ = 0;
    }

    // Splice - move elements from other list
    void splice(iterator pos, IntrusiveList& other)
    {
        if (other.empty())
        {
            return;
        }

        // Update ownership for all nodes being transferred
        auto* node = other.mHead;
        while (node)
        {
            node->mOwner = this;
            node = node->mNext;
        }

        if (pos.mNode == nullptr)
        {
            // Splice at end
            if (mTail)
            {
                mTail->mNext = other.mHead;
                other.mHead->mPrev = mTail;
            }
            else
            {
                mHead = other.mHead;
            }
            mTail = other.mTail;
        }
        else
        {
            // Splice before pos
            auto* pos_node = pos.mNode;

            // Static analysis note: other.mHead/mTail are guaranteed non-null here
            // because we returned early if other.empty() above
            assert(other.mHead != nullptr && other.mTail != nullptr);

            other.mTail->mNext = pos_node;
            other.mHead->mPrev = pos_node->mPrev;

            if (pos_node->mPrev)
            {
                pos_node->mPrev->mNext = other.mHead;
            }
            else
            {
                mHead = other.mHead;
            }

            pos_node->mPrev = other.mTail;
        }

        size_ += other.size_;

        other.mHead = nullptr;
        other.mTail = nullptr;
        other.size_ = 0;
    }

private:
    IntrusiveListNode<T>* mHead;
    IntrusiveListNode<T>* mTail;
    size_type size_;
};

} // namespace fat_p
