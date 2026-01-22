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
    docs:
      - Documentation/IntrusiveList/Overview - IntrusiveList.md
      - Documentation/IntrusiveList/User Manual - IntrusiveList.md
    tests:
      - tests/test_IntrusiveList.cpp
    benchmarks:
      - benchmarks/benchmark_IntrusiveList.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  backlog:
    - id: BL-1
      title: "Single-element splice(pos, other, it)"
      rationale: "STL parity with std::list"
      status: deferred
    - id: BL-2
      title: "merge() and sort() member functions"
      rationale: "STL parity; low demand for intrusive list use cases"
      status: deferred
    - id: BL-3
      title: "constexpr support (C++20)"
      rationale: "Compile-time list operations; requires feature gating"
      status: deferred
    - id: BL-4
      title: "Sharpen splice iterator invalidation docs"
      rationale: "Maximum clarity on poisoned iterators post-splice"
      status: deferred
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
#include <limits>
#include <type_traits>
#include <utility>

namespace fat_p
{

// Forward declarations (needed for Hook friend access)
template <typename T, typename OwnerPolicy>
class IntrusiveList;

template <typename T, typename OwnerPolicy>
class IntrusiveListNode;

template <typename T, typename OwnerPolicy>
class IntrusiveListIterator;

template <typename T, typename OwnerPolicy>
class IntrusiveListConstIterator;

namespace intrusive_list
{

/**
 * @brief Ownership policy that stores no owner pointer.
 *
 * Provides O(1) splice and move operations. Removing a node from the wrong
 * list is undefined behavior.
 */
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

/**
 * @brief Ownership policy that tracks which list owns each node.
 *
 * Provides safe wrong-list removal (returns silently). Splice and move
 * operations are O(N) due to owner pointer updates.
 */
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

/**
 * @brief Internal hook structure for intrusive list linkage.
 * @tparam OwnerPolicy The ownership tracking policy.
 */
template <typename OwnerPolicy>
struct Hook : private OwnerPolicy
{
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

private:
    // Link pointers for circular doubly-linked list.
    // Accessible only via friend classes (IntrusiveList, iterators).
    // mPrev == nullptr indicates an unlinked node.
    Hook* mPrev = nullptr;
    Hook* mNext = nullptr;

    template <typename T, typename P>
    friend class fat_p::IntrusiveList;

    template <typename T, typename P>
    friend class fat_p::IntrusiveListNode;

    template <typename T, typename P>
    friend class fat_p::IntrusiveListIterator;

    template <typename T, typename P>
    friend class fat_p::IntrusiveListConstIterator;
};

} // namespace intrusive_list

// ============================================================================
// IntrusiveListNode - Base class for list nodes
// ============================================================================

/**
 * @brief Base class for nodes that can be inserted into an IntrusiveList.
 *
 * User types must publicly inherit from this class to be usable with
 * IntrusiveList. The node tracks its linkage state and, depending on
 * the policy, which list owns it.
 *
 * @tparam T The derived node type (CRTP pattern).
 * @tparam OwnerPolicy FastOwnerPolicy (default) for O(1) operations, or
 *         SafeOwnerPolicy for ownership tracking.
 *
 * Contract:
 * - Objects must outlive their list membership.
 * - A node must be unlinked before destruction (debug-asserted).
 * - A node must not be inserted into any list while already linked
 *   (debug-asserted).
 *
 * Policy behavior:
 * - FastOwnerPolicy (default): no owner pointer stored; splice/move O(1).
 *   Removing a node from the wrong list is UB.
 * - SafeOwnerPolicy: stores owner pointer; wrong-list remove is a safe no-op;
 *   splice/move are O(N) due to owner updates.
 *
 * @note Thread-safety: Not thread-safe.
 */
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveListNode : public intrusive_list::Hook<OwnerPolicy>
{
public:
    IntrusiveListNode() = default;

    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;

    ~IntrusiveListNode()
    {
        assert(!this->isLinked() &&
               "Destroying a linked IntrusiveListNode - call list.remove(node) or list.clear() first");
    }

    /**
     * @brief Check whether the node is currently linked into any list.
     * @return true if linked, false otherwise.
     * @note Complexity: O(1)
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

/**
 * @brief Bidirectional iterator for IntrusiveList.
 *
 * Models the BidirectionalIterator concept. Iterators remain valid as long as
 * the referenced node remains in the list.
 *
 * @tparam T The node type.
 * @tparam OwnerPolicy The ownership tracking policy.
 *
 * @note Thread-safety: Not thread-safe.
 */
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

    /** @brief Default constructor. Creates a singular iterator. */
    IntrusiveListIterator() = default;

    /**
     * @brief Construct an iterator pointing to a specific node.
     * @param node Pointer to the current node's hook.
     * @param sentinel Pointer to the list's sentinel hook.
     */
    IntrusiveListIterator(hook_type* node, hook_type* sentinel)
        : mNode(node)
        , mSentinel(sentinel)
    {
    }

    /**
     * @brief Dereference the iterator.
     * @return Reference to the current node.
     * @pre Iterator must not be at end().
     */
    reference operator*() const noexcept
    {
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
        assert(mNode != mSentinel);
        return *static_cast<T*>(static_cast<node_type*>(mNode));
    }

    /**
     * @brief Member access through the iterator.
     * @return Pointer to the current node.
     */
    pointer operator->() const noexcept
    {
        return &(**this);
    }

    /**
     * @brief Pre-increment. Advances to the next node.
     * @return Reference to this iterator after advancement.
     * @pre Iterator must not be end().
     * @note Complexity: O(1)
     */
    IntrusiveListIterator& operator++() noexcept
    {
        assert(mNode != nullptr && "Uninitialized iterator");
        assert(mSentinel != nullptr && "Uninitialized iterator");
        assert(mNode != mSentinel && "Cannot increment end() iterator");
        mNode = mNode->mNext;
        return *this;
    }

    /**
     * @brief Post-increment. Advances to the next node.
     * @return Copy of iterator before advancement.
     * @note Complexity: O(1)
     */
    IntrusiveListIterator operator++(int) noexcept
    {
        IntrusiveListIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    /**
     * @brief Pre-decrement. Moves to the previous node.
     * @return Reference to this iterator after movement.
     * @pre Iterator must not be begin() (unless at end() of non-empty list).
     * @note Complexity: O(1)
     */
    IntrusiveListIterator& operator--() noexcept
    {
        assert(mNode != nullptr && "Uninitialized iterator");
        assert(mSentinel != nullptr && "Uninitialized iterator");
        // Prevent --begin(): at first element, mNode->mPrev is sentinel
        assert((mNode == mSentinel || mNode->mPrev != mSentinel) &&
               "Cannot decrement begin() iterator");
        // Prevent --end() on empty list
        assert((mNode != mSentinel || mSentinel->mPrev != mSentinel) &&
               "Cannot decrement end() on empty list");
        mNode = mNode->mPrev;
        return *this;
    }

    /**
     * @brief Post-decrement. Moves to the previous node.
     * @return Copy of iterator before movement.
     * @note Complexity: O(1)
     */
    IntrusiveListIterator operator--(int) noexcept
    {
        IntrusiveListIterator tmp = *this;
        --(*this);
        return tmp;
    }

    /**
     * @brief Equality comparison.
     * @param other Iterator to compare with.
     * @return true if both iterators point to the same position in the same list.
     */
    bool operator==(const IntrusiveListIterator& other) const noexcept
    {
        return mNode == other.mNode && mSentinel == other.mSentinel;
    }

    /**
     * @brief Inequality comparison.
     * @param other Iterator to compare with.
     * @return true if iterators point to different positions or different lists.
     */
    bool operator!=(const IntrusiveListIterator& other) const noexcept
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
// IntrusiveListConstIterator
// ============================================================================

/**
 * @brief Const bidirectional iterator for IntrusiveList.
 *
 * Models the BidirectionalIterator concept with const access. Implicitly
 * convertible from IntrusiveListIterator.
 *
 * @tparam T The node type.
 * @tparam OwnerPolicy The ownership tracking policy.
 *
 * @note Thread-safety: Not thread-safe.
 */
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

    /** @brief Default constructor. Creates a singular iterator. */
    IntrusiveListConstIterator() = default;

    /**
     * @brief Construct a const iterator pointing to a specific node.
     * @param node Pointer to the current node's hook.
     * @param sentinel Pointer to the list's sentinel hook.
     */
    IntrusiveListConstIterator(const hook_type* node, const hook_type* sentinel)
        : mNode(node)
        , mSentinel(sentinel)
    {
    }

    /**
     * @brief Converting constructor from mutable iterator.
     * @param it The mutable iterator to convert.
     */
    IntrusiveListConstIterator(const IntrusiveListIterator<T, OwnerPolicy>& it)
        : mNode(it.mNode)
        , mSentinel(it.mSentinel)
    {
    }

    /**
     * @brief Dereference the iterator.
     * @return Const reference to the current node.
     * @pre Iterator must not be at end().
     */
    reference operator*() const noexcept
    {
        assert(mNode != nullptr);
        assert(mSentinel != nullptr);
        assert(mNode != mSentinel);
        return *static_cast<const T*>(static_cast<const node_type*>(mNode));
    }

    /**
     * @brief Member access through the iterator.
     * @return Const pointer to the current node.
     */
    pointer operator->() const noexcept
    {
        return &(**this);
    }

    /**
     * @brief Pre-increment. Advances to the next node.
     * @return Reference to this iterator after advancement.
     * @pre Iterator must not be end().
     * @note Complexity: O(1)
     */
    IntrusiveListConstIterator& operator++() noexcept
    {
        assert(mNode != nullptr && "Uninitialized iterator");
        assert(mSentinel != nullptr && "Uninitialized iterator");
        assert(mNode != mSentinel && "Cannot increment end() iterator");
        mNode = mNode->mNext;
        return *this;
    }

    /**
     * @brief Post-increment. Advances to the next node.
     * @return Copy of iterator before advancement.
     * @note Complexity: O(1)
     */
    IntrusiveListConstIterator operator++(int) noexcept
    {
        IntrusiveListConstIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    /**
     * @brief Pre-decrement. Moves to the previous node.
     * @return Reference to this iterator after movement.
     * @pre Iterator must not be begin() (unless at end() of non-empty list).
     * @note Complexity: O(1)
     */
    IntrusiveListConstIterator& operator--() noexcept
    {
        assert(mNode != nullptr && "Uninitialized iterator");
        assert(mSentinel != nullptr && "Uninitialized iterator");
        // Prevent --begin(): at first element, mNode->mPrev is sentinel
        assert((mNode == mSentinel || mNode->mPrev != mSentinel) &&
               "Cannot decrement begin() iterator");
        // Prevent --end() on empty list
        assert((mNode != mSentinel || mSentinel->mPrev != mSentinel) &&
               "Cannot decrement end() on empty list");
        mNode = mNode->mPrev;
        return *this;
    }

    /**
     * @brief Post-decrement. Moves to the previous node.
     * @return Copy of iterator before movement.
     * @note Complexity: O(1)
     */
    IntrusiveListConstIterator operator--(int) noexcept
    {
        IntrusiveListConstIterator tmp = *this;
        --(*this);
        return tmp;
    }

    /**
     * @brief Equality comparison.
     * @param other Iterator to compare with.
     * @return true if both iterators point to the same position in the same list.
     */
    bool operator==(const IntrusiveListConstIterator& other) const noexcept
    {
        return mNode == other.mNode && mSentinel == other.mSentinel;
    }

    /**
     * @brief Inequality comparison.
     * @param other Iterator to compare with.
     * @return true if iterators point to different positions or different lists.
     */
    bool operator!=(const IntrusiveListConstIterator& other) const noexcept
    {
        return !(*this == other);
    }

    /**
     * @brief Equality comparison with mutable iterator.
     * @param other Mutable iterator to compare with.
     * @return true if both iterators point to the same position in the same list.
     */
    bool operator==(const IntrusiveListIterator<T, OwnerPolicy>& other) const noexcept
    {
        return mNode == other.mNode && mSentinel == other.mSentinel;
    }

    /**
     * @brief Inequality comparison with mutable iterator.
     * @param other Mutable iterator to compare with.
     * @return true if iterators point to different positions or different lists.
     */
    bool operator!=(const IntrusiveListIterator<T, OwnerPolicy>& other) const noexcept
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
// Cross-Iterator Comparison Operators (Non-member)
// ============================================================================

/**
 * @brief Equality comparison between iterator and const_iterator.
 * @tparam T The node type.
 * @tparam OwnerPolicy The ownership tracking policy.
 * @param lhs Mutable iterator.
 * @param rhs Const iterator.
 * @return true if both iterators point to the same position in the same list.
 */
template <typename T, typename OwnerPolicy>
bool operator==(const IntrusiveListIterator<T, OwnerPolicy>& lhs,
                const IntrusiveListConstIterator<T, OwnerPolicy>& rhs)
{
    // Convert iterator to const_iterator for comparison
    return IntrusiveListConstIterator<T, OwnerPolicy>(lhs) == rhs;
}

/**
 * @brief Inequality comparison between iterator and const_iterator.
 * @tparam T The node type.
 * @tparam OwnerPolicy The ownership tracking policy.
 * @param lhs Mutable iterator.
 * @param rhs Const iterator.
 * @return true if iterators point to different positions or different lists.
 */
template <typename T, typename OwnerPolicy>
bool operator!=(const IntrusiveListIterator<T, OwnerPolicy>& lhs,
                const IntrusiveListConstIterator<T, OwnerPolicy>& rhs)
{
    return !(lhs == rhs);
}

// ============================================================================
// IntrusiveList
// ============================================================================

/**
 * @brief Zero-allocation intrusive doubly-linked list.
 *
 * An intrusive list where nodes contain the link pointers themselves, avoiding
 * per-node memory allocation. Nodes must inherit from IntrusiveListNode<T, Policy>.
 * The list does not own the nodes; users are responsible for node lifetime.
 *
 * @tparam T The node type. Must publicly inherit from IntrusiveListNode<T, OwnerPolicy>.
 * @tparam OwnerPolicy FastOwnerPolicy (default) for O(1) splice/move operations, or
 *         SafeOwnerPolicy for ownership tracking (wrong-list remove is safe no-op,
 *         but splice/move become O(N)).
 *
 * Key properties:
 * - O(1) insertion and removal given an iterator or node reference
 * - O(1) splice (FastOwnerPolicy) or O(N) splice (SafeOwnerPolicy)
 * - O(1) size() query (cached count, not computed per call)
 * - Bidirectional iteration
 * - No memory allocation
 *
 * @note Thread-safety: Not thread-safe. External synchronization required for
 *       concurrent access.
 *
 * @see IntrusiveListNode
 * @see IntrusiveListFast (alias for FastOwnerPolicy)
 * @see IntrusiveListSafe (alias for SafeOwnerPolicy)
 */
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveList
{
private:
    using node_type = IntrusiveListNode<T, OwnerPolicy>;
    using hook_type = intrusive_list::Hook<OwnerPolicy>;

public:
    static_assert(std::is_base_of_v<node_type, T>,
                  "T must inherit from IntrusiveListNode<T, OwnerPolicy>. "
                  "Ensure your node class uses public inheritance: "
                  "struct MyNode : public IntrusiveListNode<MyNode, Policy> { ... };");

    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using iterator = IntrusiveListIterator<T, OwnerPolicy>;
    using const_iterator = IntrusiveListConstIterator<T, OwnerPolicy>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    /**
     * @brief Default constructor. Creates an empty list.
     * @note Complexity: O(1)
     */
    IntrusiveList()
    {
        initializeEmpty();
    }

    /**
     * @brief Destructor. Unlinks all nodes.
     *
     * Calls clear() to unlink all nodes. Does not destroy the nodes themselves
     * (the list does not own the nodes).
     *
     * @note Complexity: O(N)
     */
    ~IntrusiveList()
    {
        clear();
    }

    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    /**
     * @brief Move constructor. Transfers ownership of all nodes from other.
     * @param other The list to move from. Will be empty after the move.
     * @note Complexity: O(1) for FastOwnerPolicy, O(N) for SafeOwnerPolicy.
     */
    IntrusiveList(IntrusiveList&& other) noexcept
    {
        initializeEmpty();
        moveFrom(other);
    }

    /**
     * @brief Move assignment. Clears this list and transfers nodes from other.
     * @param other The list to move from. Will be empty after the move.
     * @return Reference to this list.
     * @note Complexity: O(N) where N is size() + other.size() for SafeOwnerPolicy,
     *       O(size()) for FastOwnerPolicy (clearing this list).
     */
    IntrusiveList& operator=(IntrusiveList&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            moveFrom(other);
        }
        return *this;
    }

    // ========================================================================
    // Capacity
    // ========================================================================

    /**
     * @brief Check if the list is empty.
     * @return true if the list contains no nodes.
     * @note Complexity: O(1)
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return mSize == 0;
    }

    /**
     * @brief Get the number of nodes in the list.
     * @return The number of nodes.
     * @note Complexity: O(1)
     */
    [[nodiscard]] size_type size() const noexcept
    {
        return mSize;
    }

    /**
     * @brief Get the theoretical maximum number of elements.
     * @return The maximum possible number of elements.
     * @note Complexity: O(1)
     */
    [[nodiscard]] constexpr size_type max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max();
    }

    // ========================================================================
    // Element Access
    // ========================================================================

    /**
     * @brief Access the first node.
     * @return Reference to the first node.
     * @pre The list must not be empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] reference front()
    {
        assert(!empty() && "front() called on empty list");
        return *static_cast<T*>(static_cast<node_type*>(mSentinel.mNext));
    }

    /**
     * @brief Access the first node (const version).
     * @return Const reference to the first node.
     * @pre The list must not be empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] const_reference front() const
    {
        assert(!empty() && "front() called on empty list");
        return *static_cast<const T*>(static_cast<const node_type*>(mSentinel.mNext));
    }

    /**
     * @brief Access the last node.
     * @return Reference to the last node.
     * @pre The list must not be empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] reference back()
    {
        assert(!empty() && "back() called on empty list");
        return *static_cast<T*>(static_cast<node_type*>(mSentinel.mPrev));
    }

    /**
     * @brief Access the last node (const version).
     * @return Const reference to the last node.
     * @pre The list must not be empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] const_reference back() const
    {
        assert(!empty() && "back() called on empty list");
        return *static_cast<const T*>(static_cast<const node_type*>(mSentinel.mPrev));
    }

    // ========================================================================
    // Iterators
    // ========================================================================

    /**
     * @brief Get iterator to the first node.
     * @return Iterator to the first node, or end() if empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] iterator begin() noexcept
    {
        return iterator(mSentinel.mNext, &mSentinel);
    }

    /**
     * @brief Get iterator to one past the last node.
     * @return Iterator to the sentinel (past-the-end).
     * @note Complexity: O(1)
     */
    [[nodiscard]] iterator end() noexcept
    {
        return iterator(&mSentinel, &mSentinel);
    }

    /**
     * @brief Get const iterator to the first node.
     * @return Const iterator to the first node, or end() if empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] const_iterator begin() const noexcept
    {
        return const_iterator(mSentinel.mNext, &mSentinel);
    }

    /**
     * @brief Get const iterator to one past the last node.
     * @return Const iterator to the sentinel (past-the-end).
     * @note Complexity: O(1)
     */
    [[nodiscard]] const_iterator end() const noexcept
    {
        return const_iterator(&mSentinel, &mSentinel);
    }

    /**
     * @brief Get const iterator to the first node.
     * @return Const iterator to the first node, or cend() if empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] const_iterator cbegin() const noexcept
    {
        return begin();
    }

    /**
     * @brief Get const iterator to one past the last node.
     * @return Const iterator to the sentinel (past-the-end).
     * @note Complexity: O(1)
     */
    [[nodiscard]] const_iterator cend() const noexcept
    {
        return end();
    }

    /**
     * @brief Get an iterator to a node already linked in this list.
     *
     * @param node Reference to the node to find.
     * @return Iterator to the node, or end() if not linked or not in this list.
     *
     * Contract:
     * - If the node is not linked, returns end().
     * - SafeOwnerPolicy: if the node is linked but belongs to a different list,
     *   returns end().
     * - FastOwnerPolicy: passing a node linked into a different list is
     *   undefined behavior.
     *
     * @note Complexity: O(1)
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
        else
        {
            // FastOwnerPolicy: debug-only O(N) membership validation
            assert(debugContainsNode(node) &&
                   "iteratorTo() called on node not in this list (UB for FastOwnerPolicy)");
        }

        return iterator(static_cast<hook_type*>(&n), &mSentinel);
    }

    /**
     * @brief Get a const iterator to a node already linked in this list.
     *
     * @param node Const reference to the node to find.
     * @return Const iterator to the node, or end() if not linked or not in this list.
     *
     * See iteratorTo(T&) for the policy-specific contract.
     *
     * @note Complexity: O(1)
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
        else
        {
            // FastOwnerPolicy: debug-only O(N) membership validation
            assert(debugContainsNode(node) &&
                   "iteratorTo() called on node not in this list (UB for FastOwnerPolicy)");
        }

        return const_iterator(static_cast<const hook_type*>(&n), &mSentinel);
    }

    /**
     * @brief Check if a node is in this list.
     *
     * @param node The node to check.
     * @return true if node is linked into this list; false otherwise.
     *
     * @note SafeOwnerPolicy: O(1) via owner pointer comparison.
     * @note FastOwnerPolicy: O(N) linear scan in debug builds;
     *       always returns node.isLinked() in release (cannot distinguish lists).
     */
    [[nodiscard]] bool contains(const T& node) const noexcept
    {
        const auto& n = static_cast<const node_type&>(node);

        if (!n.isLinked())
        {
            return false;
        }

        if constexpr (OwnerPolicy::kHasOwner)
        {
            return n.ownerPtr() == this;
        }
        else
        {
            return debugContainsNode(node);
        }
    }

    /**
     * @brief Get reverse iterator to the last node.
     * @return Reverse iterator to the last node, or rend() if empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] std::reverse_iterator<iterator> rbegin() noexcept
    {
        return std::reverse_iterator<iterator>(end());
    }

    /**
     * @brief Get reverse iterator to one before the first node.
     * @return Reverse iterator to before-the-beginning.
     * @note Complexity: O(1)
     */
    [[nodiscard]] std::reverse_iterator<iterator> rend() noexcept
    {
        return std::reverse_iterator<iterator>(begin());
    }

    /**
     * @brief Get const reverse iterator to the last node.
     * @return Const reverse iterator to the last node, or rend() if empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] std::reverse_iterator<const_iterator> rbegin() const noexcept
    {
        return std::reverse_iterator<const_iterator>(end());
    }

    /**
     * @brief Get const reverse iterator to one before the first node.
     * @return Const reverse iterator to before-the-beginning.
     * @note Complexity: O(1)
     */
    [[nodiscard]] std::reverse_iterator<const_iterator> rend() const noexcept
    {
        return std::reverse_iterator<const_iterator>(begin());
    }

    /**
     * @brief Get const reverse iterator to the last node.
     * @return Const reverse iterator to the last node, or crend() if empty.
     * @note Complexity: O(1)
     */
    [[nodiscard]] std::reverse_iterator<const_iterator> crbegin() const noexcept
    {
        return std::reverse_iterator<const_iterator>(cend());
    }

    /**
     * @brief Get const reverse iterator to one before the first node.
     * @return Const reverse iterator to before-the-beginning.
     * @note Complexity: O(1)
     */
    [[nodiscard]] std::reverse_iterator<const_iterator> crend() const noexcept
    {
        return std::reverse_iterator<const_iterator>(cbegin());
    }

    // ========================================================================
    // Modifiers
    // ========================================================================

    /**
     * @brief Insert a node at the front of the list.
     * @param node Reference to the node to insert. Must not be currently linked.
     * @pre node.isLinked() == false
     * @note Complexity: O(1)
     */
    void push_front(T& node)
    {
        linkBefore(mSentinel.mNext, node);
    }

    /**
     * @brief Insert a node at the back of the list.
     * @param node Reference to the node to insert. Must not be currently linked.
     * @pre node.isLinked() == false
     * @note Complexity: O(1)
     */
    void push_back(T& node)
    {
        linkBefore(&mSentinel, node);
    }

    /**
     * @brief Remove and unlink the first node.
     *
     * If the list is empty, this is a no-op. The removed node's isLinked()
     * will return false after this call.
     *
     * @note Complexity: O(1)
     */
    void pop_front()
    {
        if (empty())
        {
            return;
        }

        unlinkNode(*static_cast<T*>(static_cast<node_type*>(mSentinel.mNext)));
    }

    /**
     * @brief Remove and unlink the last node.
     *
     * If the list is empty, this is a no-op. The removed node's isLinked()
     * will return false after this call.
     *
     * @note Complexity: O(1)
     */
    void pop_back()
    {
        if (empty())
        {
            return;
        }

        unlinkNode(*static_cast<T*>(static_cast<node_type*>(mSentinel.mPrev)));
    }

    /**
     * @brief Insert a node before the specified position.
     * @param pos Iterator to the position before which to insert.
     * @param node Reference to the node to insert. Must not be currently linked.
     * @return Iterator to the inserted node.
     * @pre pos must be a valid iterator into this list.
     * @pre node.isLinked() == false
     * @note Complexity: O(1)
     */
    iterator insert(iterator pos, T& node)
    {
        assert(pos.mSentinel == &mSentinel && "insert iterator does not belong to this list");
        linkBefore(pos.mNode, node);
        return iterator(static_cast<hook_type*>(static_cast<node_type*>(&node)), &mSentinel);
    }

    /**
     * @brief Remove a node from the list by reference.
     *
     * If the node is not linked, this is a no-op. With SafeOwnerPolicy,
     * if the node belongs to a different list, this is also a no-op.
     * With FastOwnerPolicy, removing a node from the wrong list is UB.
     *
     * @param node Reference to the node to remove.
     * @note Complexity: O(1)
     */
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
        else
        {
            // FastOwnerPolicy: debug-only O(N) membership validation
            assert(debugContainsNode(node) &&
                   "remove() called on node not in this list (UB for FastOwnerPolicy)");
        }

        unlinkNode(node);
    }

    /**
     * @brief Erase the node at the specified position.
     * @param pos Iterator to the node to erase.
     * @return Iterator to the node following the erased node, or end().
     * @pre pos must be a valid iterator into this list (not end()).
     * @note If pos == end(), returns end() without modification (no-op).
     *       This differs from std::list where erasing end() is undefined behavior.
     * @note Complexity: O(1)
     */
    iterator erase(iterator pos)
    {
        assert(pos.mSentinel == &mSentinel && "erase iterator does not belong to this list");

        if (pos == end())
        {
            return end();
        }

        hook_type* const next = pos.mNode->mNext;
        unlinkNode(*static_cast<T*>(static_cast<node_type*>(pos.mNode)));
        return iterator(next, &mSentinel);
    }

    /**
     * @brief Remove and unlink all nodes from the list.
     *
     * After this call, all previously-linked nodes will have isLinked() == false.
     * The list will be empty.
     *
     * @note Complexity: O(N)
     */
    void clear()
    {
        hook_type* node = mSentinel.mNext;
        while (node != &mSentinel)
        {
            hook_type* const next = node->mNext;
            node->clearLinks();
            node = next;
        }

        initializeEmpty();
    }

    /**
     * @brief Transfer all nodes from another list into this list.
     *
     * Inserts all nodes from other before the position specified by pos.
     * After the splice, other will be empty. Self-splice is a no-op.
     *
     * @param pos Iterator to the position before which to insert.
     * @param other The list to splice from. Will be empty after the splice.
     * @pre pos must be a valid iterator into this list.
     *
     * @warning All iterators into `other` are invalidated after splice,
     *          including iterators to transferred nodes. Use iteratorTo()
     *          or begin()/end() on the destination to obtain new iterators.
     *
     * @note Complexity: O(1) for FastOwnerPolicy, O(other.size()) for SafeOwnerPolicy.
     */
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
        other.initializeEmpty();

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

    /**
     * @brief Swap contents with another list.
     *
     * Exchanges all nodes between this list and other. After the swap,
     * nodes that were in this list will be in other and vice versa.
     *
     * @param other The list to swap with.
     * @note Complexity: O(1) for FastOwnerPolicy, O(size() + other.size()) for SafeOwnerPolicy.
     */
    void swap(IntrusiveList& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        // Handle empty cases specially to avoid sentinel confusion
        const bool thisEmpty = empty();
        const bool otherEmpty = other.empty();

        if (thisEmpty && otherEmpty)
        {
            return;
        }

        if (thisEmpty)
        {
            // Move other to this
            moveFrom(other);
            return;
        }

        if (otherEmpty)
        {
            // Move this to other
            other.moveFrom(*this);
            return;
        }

        // Both non-empty: swap the chains
        std::swap(mSentinel.mNext, other.mSentinel.mNext);
        std::swap(mSentinel.mPrev, other.mSentinel.mPrev);

        // Fix the back-pointers
        mSentinel.mNext->mPrev = &mSentinel;
        mSentinel.mPrev->mNext = &mSentinel;
        other.mSentinel.mNext->mPrev = &other.mSentinel;
        other.mSentinel.mPrev->mNext = &other.mSentinel;

        std::swap(mSize, other.mSize);

        // Update owner pointers if using SafeOwnerPolicy
        if constexpr (OwnerPolicy::kHasOwner)
        {
            hook_type* cur = mSentinel.mNext;
            while (cur != &mSentinel)
            {
                cur->setOwnerPtr(this);
                cur = cur->mNext;
            }

            cur = other.mSentinel.mNext;
            while (cur != &other.mSentinel)
            {
                cur->setOwnerPtr(&other);
                cur = cur->mNext;
            }
        }
    }

private:
    /**
     * @brief Debug-only: verify node belongs to this list.
     * @param node Node to check.
     * @return true if node is in this list.
     * @note O(N) scan, compiled out in release builds.
     */
    bool debugContainsNode([[maybe_unused]] const T& node) const
    {
#ifdef NDEBUG
        return true;
#else
        const hook_type* target = static_cast<const hook_type*>(
            static_cast<const node_type*>(&node));
        const hook_type* cur = mSentinel.mNext;
        while (cur != &mSentinel)
        {
            if (cur == target)
            {
                return true;
            }
            cur = cur->mNext;
        }
        return false;
#endif
    }

    void initializeEmpty() noexcept
    {
        mSentinel.mPrev = &mSentinel;
        mSentinel.mNext = &mSentinel;
        mSentinel.setOwnerPtr(nullptr);
        mSize = 0;
    }

    void moveFrom(IntrusiveList& other) noexcept
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

        other.initializeEmpty();
    }

    void linkBefore(hook_type* before, T& node)
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

    void unlinkNode(T& node)
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

/**
 * @brief Swap two IntrusiveLists.
 * @tparam T The node type.
 * @tparam OwnerPolicy The ownership tracking policy.
 * @param lhs First list.
 * @param rhs Second list.
 * @note Enables ADL-based swap for STL compatibility.
 */
template <typename T, typename OwnerPolicy>
void swap(IntrusiveList<T, OwnerPolicy>& lhs, IntrusiveList<T, OwnerPolicy>& rhs) noexcept
{
    lhs.swap(rhs);
}

/** @brief Alias for IntrusiveList with FastOwnerPolicy (O(1) operations, no ownership tracking). */
template <typename T>
using IntrusiveListFast = IntrusiveList<T, intrusive_list::FastOwnerPolicy>;

/** @brief Alias for IntrusiveList with SafeOwnerPolicy (ownership tracking, safe wrong-list remove). */
template <typename T>
using IntrusiveListSafe = IntrusiveList<T, intrusive_list::SafeOwnerPolicy>;

} // namespace fat_p
