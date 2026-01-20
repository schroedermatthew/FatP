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
 * @brief Intrusive doubly-linked list with zero allocation overhead.
 */

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace fat_p
{

namespace intrusive_list
{

struct FastOwnerPolicy
{
    static constexpr bool kHasOwner = false;

    void setOwner(const void*) noexcept
    {
    }

    [[nodiscard]] const void* owner() const noexcept
    {
        return nullptr;
    }
};

struct SafeOwnerPolicy
{
    static constexpr bool kHasOwner = true;

    void setOwner(const void* owner) noexcept
    {
        mOwner = owner;
    }

    [[nodiscard]] const void* owner() const noexcept
    {
        return mOwner;
    }

private:
    const void* mOwner = nullptr;
};

template <typename OwnerPolicy>
struct Hook : private OwnerPolicy
{
    Hook* mPrev = nullptr;
    Hook* mNext = nullptr;

    [[nodiscard]] bool isLinked() const noexcept
    {
        return mPrev != nullptr;
    }

    void clearLinks() noexcept
    {
        mPrev = nullptr;
        mNext = nullptr;
        this->setOwner(nullptr);
    }

    void setOwnerPtr(const void* owner) noexcept
    {
        this->setOwner(owner);
    }

    [[nodiscard]] const void* ownerPtr() const noexcept
    {
        return this->owner();
    }
};

} // namespace intrusive_list

// Forward declarations
template <typename T, typename OwnerPolicy>
class IntrusiveList;

template <typename T, typename OwnerPolicy>
class IntrusiveListIterator;

template <typename T, typename OwnerPolicy>
class IntrusiveListConstIterator;

// ============================================================================
// IntrusiveListNode - Base class for list nodes
// ============================================================================
//
// Contract:
// - Objects must outlive their list membership.
// - A node must be unlinked before destruction (debug-asserted).
// - A node must not be inserted into any list while already linked
//   (debug-asserted).
//
// Policy:
// - FastOwnerPolicy (default): no owner pointer stored; splice/move O(1).
//   Removing a node from the wrong list is UB.
// - SafeOwnerPolicy: stores owner pointer; wrong-list remove is a safe no-op;
//   splice/move are O(N) due to owner updates.
// ============================================================================
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveListNode : public intrusive_list::Hook<OwnerPolicy>
{
public:
    IntrusiveListNode() = default;

    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;

    ~IntrusiveListNode()
    {
        assert(!this->isLinked() && "Destroying a linked IntrusiveListNode");
    }

    /**
     * @brief Check whether the node is currently linked into any list.
     */
    [[nodiscard]] bool isLinked() const noexcept
    {
        return intrusive_list::Hook<OwnerPolicy>::isLinked();
    }

private:
    template <typename U, typename P>
    friend class IntrusiveList;

    template <typename U, typename P>
    friend class IntrusiveListIterator;

    template <typename U, typename P>
    friend class IntrusiveListConstIterator;
};

// ============================================================================
// IntrusiveListIterator
// ============================================================================
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveListIterator
{
private:
    using node_type = IntrusiveListNode<T, OwnerPolicy>;
    using hook_type = intrusive_list::Hook<OwnerPolicy>;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    IntrusiveListIterator() = default;

    IntrusiveListIterator(hook_type* node, hook_type* sentinel)
        : mNode(node)
        , mSentinel(sentinel)
    {
    }

    reference operator*() const
    {
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
        assert(mNode != mSentinel);
        return *static_cast<T*>(static_cast<node_type*>(mNode));
    }

    pointer operator->() const
    {
        return &(**this);
    }

    IntrusiveListIterator& operator++()
    {
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
        mNode = mNode->mNext;
        return *this;
    }

    IntrusiveListIterator operator++(int)
    {
        IntrusiveListIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    IntrusiveListIterator& operator--()
    {
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
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
        return mNode == other.mNode && mSentinel == other.mSentinel;
    }

    bool operator!=(const IntrusiveListIterator& other) const
    {
        return !(*this == other);
    }

private:
    template <typename U, typename P>
    friend class IntrusiveList;

    template <typename U, typename P>
    friend class IntrusiveListConstIterator;

    hook_type* mNode = nullptr;
    hook_type* mSentinel = nullptr;
};

// ============================================================================
// Const iterator
// ============================================================================
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveListConstIterator
{
private:
    using node_type = IntrusiveListNode<T, OwnerPolicy>;
    using hook_type = intrusive_list::Hook<OwnerPolicy>;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = const T*;
    using reference = const T&;

    IntrusiveListConstIterator() = default;

    IntrusiveListConstIterator(const hook_type* node, const hook_type* sentinel)
        : mNode(node)
        , mSentinel(sentinel)
    {
    }

    IntrusiveListConstIterator(const IntrusiveListIterator<T, OwnerPolicy>& it)
        : mNode(it.mNode)
        , mSentinel(it.mSentinel)
    {
    }

    reference operator*() const
    {
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
        assert(mNode != mSentinel);
        return *static_cast<const T*>(static_cast<const node_type*>(mNode));
    }

    pointer operator->() const
    {
        return &(**this);
    }

    IntrusiveListConstIterator& operator++()
    {
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
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
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
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
        return mNode == other.mNode && mSentinel == other.mSentinel;
    }

    bool operator!=(const IntrusiveListConstIterator& other) const
    {
        return !(*this == other);
    }

private:
    template <typename U, typename P>
    friend class IntrusiveList;

    const hook_type* mNode = nullptr;
    const hook_type* mSentinel = nullptr;
};

// ============================================================================
// IntrusiveList
// ============================================================================
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveList
{
private:
    using node_type = IntrusiveListNode<T, OwnerPolicy>;
    using hook_type = intrusive_list::Hook<OwnerPolicy>;

public:
    static_assert(std::is_base_of_v<node_type, T>, "T must inherit from IntrusiveListNode<T, Policy>");

    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using iterator = IntrusiveListIterator<T, OwnerPolicy>;
    using const_iterator = IntrusiveListConstIterator<T, OwnerPolicy>;
    using size_type = std::size_t;

    IntrusiveList()
    {
        initializeEmpty_();
    }

    ~IntrusiveList()
    {
        clear();
    }

    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    IntrusiveList(IntrusiveList&& other) noexcept
    {
        initializeEmpty_();
        moveFrom_(other);
    }

    IntrusiveList& operator=(IntrusiveList&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            moveFrom_(other);
        }
        return *this;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return mSize == 0;
    }

    [[nodiscard]] size_type size() const noexcept
    {
        return mSize;
    }

    [[nodiscard]] reference front()
    {
        assert(!empty() && "front() called on empty list");
        return *static_cast<T*>(static_cast<node_type*>(mSentinel.mNext));
    }

    [[nodiscard]] const_reference front() const
    {
        assert(!empty() && "front() called on empty list");
        return *static_cast<const T*>(static_cast<const node_type*>(mSentinel.mNext));
    }

    [[nodiscard]] reference back()
    {
        assert(!empty() && "back() called on empty list");
        return *static_cast<T*>(static_cast<node_type*>(mSentinel.mPrev));
    }

    [[nodiscard]] const_reference back() const
    {
        assert(!empty() && "back() called on empty list");
        return *static_cast<const T*>(static_cast<const node_type*>(mSentinel.mPrev));
    }

    [[nodiscard]] iterator begin() noexcept
    {
        return iterator(mSentinel.mNext, &mSentinel);
    }

    [[nodiscard]] iterator end() noexcept
    {
        return iterator(&mSentinel, &mSentinel);
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return const_iterator(mSentinel.mNext, &mSentinel);
    }

    [[nodiscard]] const_iterator end() const noexcept
    {
        return const_iterator(&mSentinel, &mSentinel);
    }

    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return begin();
    }

    [[nodiscard]] const_iterator cend() const noexcept
    {
        return end();
    }

    /**
     * @brief Get an iterator to a node already linked in this list.
     *
     * Contract:
     * - If the node is not linked, returns end().
     * - SafeOwnerPolicy: if the node is linked but belongs to a different list,
     *   returns end().
     * - FastOwnerPolicy: passing a node linked into a different list is
     *   undefined behavior.
     */
    [[nodiscard]] iterator iteratorTo(T& node) noexcept
    {
        auto& n = static_cast<node_type&>(node);

        if (!n.isLinked())
        {
            return end();
        }

        if constexpr (OwnerPolicy::kHasOwner)
        {
            if (n.ownerPtr() != this)
            {
                return end();
            }
        }

        return iterator(static_cast<hook_type*>(&n), &mSentinel);
    }

    /**
     * @brief Get a const iterator to a node already linked in this list.
     *
     * See iteratorTo(T&) for the policy-specific contract.
     */
    [[nodiscard]] const_iterator iteratorTo(const T& node) const noexcept
    {
        const auto& n = static_cast<const node_type&>(node);

        if (!n.isLinked())
        {
            return end();
        }

        if constexpr (OwnerPolicy::kHasOwner)
        {
            if (n.ownerPtr() != this)
            {
                return end();
            }
        }

        return const_iterator(static_cast<const hook_type*>(&n), &mSentinel);
    }

    [[nodiscard]] std::reverse_iterator<iterator> rbegin() noexcept
    {
        return std::reverse_iterator<iterator>(end());
    }

    [[nodiscard]] std::reverse_iterator<iterator> rend() noexcept
    {
        return std::reverse_iterator<iterator>(begin());
    }

    [[nodiscard]] std::reverse_iterator<const_iterator> rbegin() const noexcept
    {
        return std::reverse_iterator<const_iterator>(end());
    }

    [[nodiscard]] std::reverse_iterator<const_iterator> rend() const noexcept
    {
        return std::reverse_iterator<const_iterator>(begin());
    }

    [[nodiscard]] std::reverse_iterator<const_iterator> crbegin() const noexcept
    {
        return std::reverse_iterator<const_iterator>(cend());
    }

    [[nodiscard]] std::reverse_iterator<const_iterator> crend() const noexcept
    {
        return std::reverse_iterator<const_iterator>(cbegin());
    }

    void push_front(T& node)
    {
        linkBefore_(mSentinel.mNext, node);
    }

    void push_back(T& node)
    {
        linkBefore_(&mSentinel, node);
    }

    void pop_front()
    {
        if (empty())
        {
            return;
        }

        unlink_(*static_cast<T*>(static_cast<node_type*>(mSentinel.mNext)));
    }

    void pop_back()
    {
        if (empty())
        {
            return;
        }

        unlink_(*static_cast<T*>(static_cast<node_type*>(mSentinel.mPrev)));
    }

    iterator insert(iterator pos, T& node)
    {
        assert(pos.mSentinel == &mSentinel && "insert iterator does not belong to this list");
        linkBefore_(pos.mNode, node);
        return iterator(static_cast<hook_type*>(static_cast<node_type*>(&node)), &mSentinel);
    }

    void remove(T& node)
    {
        auto& n = static_cast<node_type&>(node);

        if (!n.isLinked())
        {
            return;
        }

        if constexpr (OwnerPolicy::kHasOwner)
        {
            if (n.ownerPtr() != this)
            {
                return;
            }
        }

        // Fast policy cannot validate ownership; removing a node from the wrong list is UB.
        unlink_(node);
    }

    iterator erase(iterator pos)
    {
        assert(pos.mSentinel == &mSentinel && "erase iterator does not belong to this list");

        if (pos == end())
        {
            return end();
        }

        hook_type* const next = pos.mNode->mNext;
        unlink_(*static_cast<T*>(static_cast<node_type*>(pos.mNode)));
        return iterator(next, &mSentinel);
    }

    void clear()
    {
        hook_type* node = mSentinel.mNext;
        while (node != &mSentinel)
        {
            hook_type* const next = node->mNext;
            node->clearLinks();
            node = next;
        }

        initializeEmpty_();
    }

    void splice(iterator pos, IntrusiveList& other)
    {
        assert(pos.mSentinel == &mSentinel && "splice iterator does not belong to this list");

        if (other.empty())
        {
            return;
        }

        if (&other == this)
        {
            return;
        }

        const size_type other_size = other.mSize;

        hook_type* const first = other.mSentinel.mNext;
        hook_type* const last = other.mSentinel.mPrev;

        // Detach other's range.
        other.initializeEmpty_();

        // Insert range [first,last] before pos.mNode.
        hook_type* const before = pos.mNode->mPrev;
        before->mNext = first;
        first->mPrev = before;
        last->mNext = pos.mNode;
        pos.mNode->mPrev = last;

        if constexpr (OwnerPolicy::kHasOwner)
        {
            hook_type* cur = first;
            while (cur != pos.mNode)
            {
                cur->setOwnerPtr(this);
                cur = cur->mNext;
            }
        }

        mSize += other_size;
    }

private:
    void initializeEmpty_() noexcept
    {
        mSentinel.mPrev = &mSentinel;
        mSentinel.mNext = &mSentinel;
        mSentinel.setOwnerPtr(nullptr);
        mSize = 0;
    }

    void moveFrom_(IntrusiveList& other) noexcept
    {
        if (other.empty())
        {
            return;
        }

        // Steal the chain.
        mSentinel.mNext = other.mSentinel.mNext;
        mSentinel.mPrev = other.mSentinel.mPrev;
        mSentinel.mNext->mPrev = &mSentinel;
        mSentinel.mPrev->mNext = &mSentinel;

        mSize = other.mSize;

        if constexpr (OwnerPolicy::kHasOwner)
        {
            hook_type* cur = mSentinel.mNext;
            while (cur != &mSentinel)
            {
                cur->setOwnerPtr(this);
                cur = cur->mNext;
            }
        }

        other.initializeEmpty_();
    }

    void linkBefore_(hook_type* before, T& node)
    {
        assert(before != nullptr);

        auto& n = static_cast<node_type&>(node);
        assert(!n.isLinked() && "Inserting an already-linked IntrusiveListNode");

        hook_type* const new_hook = static_cast<hook_type*>(&n);
        hook_type* const prev = before->mPrev;

        new_hook->mNext = before;
        new_hook->mPrev = prev;
        prev->mNext = new_hook;
        before->mPrev = new_hook;

        if constexpr (OwnerPolicy::kHasOwner)
        {
            new_hook->setOwnerPtr(this);
        }

        ++mSize;
    }

    void unlink_(T& node)
    {
        auto& n = static_cast<node_type&>(node);
        auto* hook = static_cast<hook_type*>(&n);

        assert(hook->isLinked() && "Unlinking a node that is not linked");

        hook->mPrev->mNext = hook->mNext;
        hook->mNext->mPrev = hook->mPrev;

        hook->clearLinks();
        --mSize;
    }

    hook_type mSentinel;
    size_type mSize = 0;
};

template <typename T>
using IntrusiveListFast = IntrusiveList<T, intrusive_list::FastOwnerPolicy>;

template <typename T>
using IntrusiveListSafe = IntrusiveList<T, intrusive_list::SafeOwnerPolicy>;

} // namespace fat_p
