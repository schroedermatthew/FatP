/**
 * @file test_IntrusiveList.cpp
 * @brief Comprehensive unit tests for IntrusiveList.h
 */
/*
FATP_META:
  meta_version: 1
  component: IntrusiveList
  file_role: test
  path: components/IntrusiveList/tests/test_IntrusiveList.cpp
  layer: Testing
  namespace: fat_p::testing::intrusivelist
  summary: "Unit tests for IntrusiveList."
  api_stability: in_work
  related:
    docs_search: "IntrusiveList"
    headers:
      - include/fat_p/IntrusiveList.h
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <atomic>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "FatPTest.h"
#include "IntrusiveList.h"

namespace fat_p::testing::intrusivelist
{

// ============================================================================
// Helper Types
// ============================================================================

// Test node type (default policy: fast / no owner pointer)
struct TestNode : fat_p::IntrusiveListNode<TestNode>
{
    int value;
    std::string name;

    TestNode(int v = 0, std::string n = "")
        : value(v)
        , name(std::move(n))
    {
    }
};

// Test node type (safe policy: owner pointer enabled)
struct TestNodeWithOwner : fat_p::IntrusiveListNode<TestNodeWithOwner, fat_p::intrusive_list::SafeOwnerPolicy>
{
    int value;

    explicit TestNodeWithOwner(int v = 0)
        : value(v)
    {
    }
};

// Lifecycle tracking node - verifies RAII correctness
struct LifecycleNode : fat_p::IntrusiveListNode<LifecycleNode>
{
    static inline std::atomic<int> constructCount{0};
    static inline std::atomic<int> destructCount{0};

    int value;

    explicit LifecycleNode(int v = 0)
        : value(v)
    {
        ++constructCount;
    }

    ~LifecycleNode()
    {
        ++destructCount;
    }

    LifecycleNode(const LifecycleNode& other)
        : value(other.value)
    {
        ++constructCount;
    }

    // Copy assignment is implicitly deleted (IntrusiveListNode base is non-copyable)
    // Move is implicitly deleted (user-defined copy ctor + destructor)

    static void reset()
    {
        constructCount = 0;
        destructCount = 0;
    }
};

// Move-only node - verifies move-only type support
struct MoveOnlyNode : fat_p::IntrusiveListNode<MoveOnlyNode>
{
    std::unique_ptr<int> data;

    explicit MoveOnlyNode(int v = 0)
        : data(std::make_unique<int>(v))
    {
    }

    // IntrusiveListNode base has no move ctor/assign (deleted copy + user dtor),
    // so default move would be implicitly deleted. Provide explicit moves that
    // transfer the data while leaving the base in default (unlinked) state.
    MoveOnlyNode(MoveOnlyNode&& other) noexcept
        : data(std::move(other.data))
    {
    }

    MoveOnlyNode& operator=(MoveOnlyNode&& other) noexcept
    {
        if (this != &other)
        {
            data = std::move(other.data);
        }
        return *this;
    }

    MoveOnlyNode(const MoveOnlyNode&) = delete;
    MoveOnlyNode& operator=(const MoveOnlyNode&) = delete;

    int getValue() const
    {
        return data ? *data : -1;
    }
};

// Node with throwing operations - for exception safety testing
struct ThrowingNode : fat_p::IntrusiveListNode<ThrowingNode>
{
    static inline int throwAfter{-1};
    static inline int operationCount{0};

    int value;

    explicit ThrowingNode(int v = 0)
        : value(v)
    {
    }

    // Simulate a method that might throw
    void riskyOperation() const
    {
        ++operationCount;
        if (throwAfter >= 0 && operationCount >= throwAfter)
        {
            throw std::runtime_error("ThrowingNode::riskyOperation threw");
        }
    }

    static void reset()
    {
        operationCount = 0;
        throwAfter = -1;
    }
};

FATP_TEST_CASE(intrusive_list_empty)
{
    fat_p::IntrusiveList<TestNode> list;

    FATP_ASSERT_TRUE(list.empty(), "List should be empty");
    FATP_ASSERT_TRUE(list.size() == 0, "List should have size 0");

    return true;
}

FATP_TEST_CASE(intrusive_list_push_front)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_front(n1);
        FATP_ASSERT_TRUE(list.size() == 1, "Size should be 1");
        FATP_ASSERT_TRUE(list.front().value == 1, "Front should be n1");

        list.push_front(n2);
        FATP_ASSERT_TRUE(list.size() == 2, "Size should be 2");
        FATP_ASSERT_TRUE(list.front().value == 2, "Front should be n2");

        list.push_front(n3);
        FATP_ASSERT_TRUE(list.size() == 3, "Size should be 3");
        FATP_ASSERT_TRUE(list.front().value == 3, "Front should be n3");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_push_back)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        FATP_ASSERT_TRUE(list.size() == 1, "Size should be 1");
        FATP_ASSERT_TRUE(list.back().value == 1, "Back should be n1");

        list.push_back(n2);
        FATP_ASSERT_TRUE(list.size() == 2, "Size should be 2");
        FATP_ASSERT_TRUE(list.back().value == 2, "Back should be n2");

        list.push_back(n3);
        FATP_ASSERT_TRUE(list.size() == 3, "Size should be 3");
        FATP_ASSERT_TRUE(list.back().value == 3, "Back should be n3");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_iteration)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        std::vector<int> values;
        for (const auto& node : list)
        {
            values.push_back(node.value);
        }

        FATP_ASSERT_TRUE(values.size() == 3, "Should iterate 3 times");
        FATP_ASSERT_TRUE(values[0] == 1, "First value should be 1");
        FATP_ASSERT_TRUE(values[1] == 2, "Second value should be 2");
        FATP_ASSERT_TRUE(values[2] == 3, "Third value should be 3");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_pop_front)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        list.pop_front();
        FATP_ASSERT_TRUE(list.size() == 2, "Size should be 2");
        FATP_ASSERT_TRUE(list.front().value == 2, "Front should be n2");

        list.pop_front();
        FATP_ASSERT_TRUE(list.size() == 1, "Size should be 1");
        FATP_ASSERT_TRUE(list.front().value == 3, "Front should be n3");

        list.pop_front();
        FATP_ASSERT_TRUE(list.empty(), "List should be empty");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_pop_back)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        list.pop_back();
        FATP_ASSERT_TRUE(list.size() == 2, "Size should be 2");
        FATP_ASSERT_TRUE(list.back().value == 2, "Back should be n2");

        list.pop_back();
        FATP_ASSERT_TRUE(list.size() == 1, "Size should be 1");
        FATP_ASSERT_TRUE(list.back().value == 1, "Back should be n1");

        list.pop_back();
        FATP_ASSERT_TRUE(list.empty(), "List should be empty");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_remove)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        // Remove middle
        list.remove(n2);
        FATP_ASSERT_TRUE(list.size() == 2, "Size should be 2");

        std::vector<int> values;
        for (const auto& node : list)
        {
            values.push_back(node.value);
        }

        FATP_ASSERT_TRUE(values.size() == 2, "Should have 2 elements");
        FATP_ASSERT_TRUE(values[0] == 1, "First should be 1");
        FATP_ASSERT_TRUE(values[1] == 3, "Second should be 3");

        // Remove front
        list.remove(n1);
        FATP_ASSERT_TRUE(list.size() == 1, "Size should be 1");
        FATP_ASSERT_TRUE(list.front().value == 3, "Front should be 3");

        // Remove last
        list.remove(n3);
        FATP_ASSERT_TRUE(list.empty(), "List should be empty");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_insert)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n3);

        // Insert in middle
        auto it = list.begin();
        ++it;
        list.insert(it, n2);

        FATP_ASSERT_TRUE(list.size() == 3, "Size should be 3");

        std::vector<int> values;
        for (const auto& node : list)
        {
            values.push_back(node.value);
        }

        FATP_ASSERT_TRUE(values[0] == 1, "First should be 1");
        FATP_ASSERT_TRUE(values[1] == 2, "Second should be 2");
        FATP_ASSERT_TRUE(values[2] == 3, "Third should be 3");

        // Insert at end
        list.insert(list.end(), n4);
        FATP_ASSERT_TRUE(list.size() == 4, "Size should be 4");
        FATP_ASSERT_TRUE(list.back().value == 4, "Back should be 4");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_erase)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        auto it = list.begin();
        ++it; // Point to n2

        auto next = list.erase(it);

        FATP_ASSERT_TRUE(list.size() == 2, "Size should be 2");
        FATP_ASSERT_TRUE(next->value == 3, "Next should point to n3");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_clear)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        FATP_ASSERT_TRUE(list.size() == 3, "Size should be 3");

        list.clear();

        FATP_ASSERT_TRUE(list.empty(), "List should be empty");
        FATP_ASSERT_TRUE(list.size() == 0, "Size should be 0");

        // Nodes should be unlinked
        FATP_ASSERT_TRUE(!n1.isLinked(), "n1 should not be linked");
        FATP_ASSERT_TRUE(!n2.isLinked(), "n2 should not be linked");
        FATP_ASSERT_TRUE(!n3.isLinked(), "n3 should not be linked");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_splice)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");

    {
        fat_p::IntrusiveList<TestNode> list1;
        fat_p::IntrusiveList<TestNode> list2;

        list1.push_back(n1);
        list1.push_back(n2);

        list2.push_back(n3);
        list2.push_back(n4);

        // Splice list2 at end of list1
        list1.splice(list1.end(), list2);

        FATP_ASSERT_TRUE(list1.size() == 4, "list1 should have 4 elements");
        FATP_ASSERT_TRUE(list2.empty(), "list2 should be empty");

        std::vector<int> values;
        for (const auto& node : list1)
        {
            values.push_back(node.value);
        }

        FATP_ASSERT_TRUE(values[0] == 1, "First should be 1");
        FATP_ASSERT_TRUE(values[1] == 2, "Second should be 2");
        FATP_ASSERT_TRUE(values[2] == 3, "Third should be 3");
        FATP_ASSERT_TRUE(values[3] == 4, "Fourth should be 4");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_move)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list1;

        list1.push_back(n1);
        list1.push_back(n2);

        // Move constructor
        fat_p::IntrusiveList<TestNode> list2(std::move(list1));

        FATP_ASSERT_TRUE(list1.empty(), "list1 should be empty after move");
        FATP_ASSERT_TRUE(list2.size() == 2, "list2 should have 2 elements");
        FATP_ASSERT_TRUE(list2.front().value == 1, "Front should be 1");

        // Move assignment
        fat_p::IntrusiveList<TestNode> list3;
        list3 = std::move(list2);

        FATP_ASSERT_TRUE(list2.empty(), "list2 should be empty after move");
        FATP_ASSERT_TRUE(list3.size() == 2, "list3 should have 2 elements");
    }

    return true;
}

// ============================================================================
// New tests for bug fixes
// ============================================================================

FATP_TEST_CASE(intrusive_list_single_element)
{
    TestNode n1(42, "single");

    FATP_ASSERT_FALSE(n1.isLinked(), "Node should not be linked initially");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);

        FATP_ASSERT_EQ(list.size(), 1u, "Size should be 1");
        FATP_ASSERT_TRUE(n1.isLinked(), "Node should be linked after push");
        FATP_ASSERT_EQ(&list.front(), &n1, "Front should be the node");
        FATP_ASSERT_EQ(&list.back(), &n1, "Back should be the node");

        // Iterate over single element
        int count = 0;
        for (auto& node : list)
        {
            FATP_ASSERT_EQ(node.value, 42, "Should iterate the single node");
            ++count;
        }
        FATP_ASSERT_EQ(count, 1, "Should iterate exactly once");

        list.remove(n1);

        FATP_ASSERT_TRUE(list.empty(), "List should be empty after remove");
        FATP_ASSERT_FALSE(n1.isLinked(), "Node should not be linked after remove");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_cross_iterator_comparison)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);

        // Get both iterator types
        fat_p::IntrusiveList<TestNode>::iterator it = list.begin();
        fat_p::IntrusiveList<TestNode>::const_iterator cit = list.cbegin();

        // Test iterator == const_iterator (via implicit conversion)
        FATP_ASSERT_TRUE(it == cit, "iterator should equal const_iterator at same position");
        FATP_ASSERT_FALSE(it != cit, "iterator should not be unequal to const_iterator at same position");

        // Test const_iterator == iterator (via explicit comparison operator)
        FATP_ASSERT_TRUE(cit == it, "const_iterator should equal iterator at same position");
        FATP_ASSERT_FALSE(cit != it, "const_iterator should not be unequal to iterator at same position");

        // Advance one iterator
        ++it;
        FATP_ASSERT_FALSE(it == cit, "iterator should not equal const_iterator at different positions");
        FATP_ASSERT_TRUE(it != cit, "iterator should be unequal to const_iterator at different positions");

        FATP_ASSERT_FALSE(cit == it, "const_iterator should not equal iterator at different positions");
        FATP_ASSERT_TRUE(cit != it, "const_iterator should be unequal to iterator at different positions");

        // Both at end
        fat_p::IntrusiveList<TestNode>::iterator itEnd = list.end();
        fat_p::IntrusiveList<TestNode>::const_iterator citEnd = list.cend();
        FATP_ASSERT_TRUE(itEnd == citEnd, "end iterators should be equal");
        FATP_ASSERT_TRUE(citEnd == itEnd, "end iterators should be equal (reverse)");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_max_size)
{
    fat_p::IntrusiveList<TestNode> list;

    // max_size should return a very large value
    auto maxSz = list.max_size();
    FATP_ASSERT_TRUE(maxSz > 0, "max_size should be positive");
    FATP_ASSERT_TRUE(maxSz >= std::numeric_limits<size_t>::max() / 2, "max_size should be very large");

    // max_size should be constexpr-friendly (compile-time constant expression)
    // This is tested implicitly by the constexpr declaration in the header

    return true;
}

FATP_TEST_CASE(intrusive_list_type_aliases)
{
    // Verify type aliases exist and are correct
    using List = fat_p::IntrusiveList<TestNode>;

    // Basic type aliases
    static_assert(std::is_same_v<List::value_type, TestNode>, "value_type should be TestNode");
    static_assert(std::is_same_v<List::reference, TestNode&>, "reference should be TestNode&");
    static_assert(std::is_same_v<List::const_reference, const TestNode&>, "const_reference should be const TestNode&");
    static_assert(std::is_same_v<List::size_type, std::size_t>, "size_type should be std::size_t");
    static_assert(std::is_same_v<List::difference_type, std::ptrdiff_t>, "difference_type should be std::ptrdiff_t");

    // Reverse iterator type aliases
    static_assert(std::is_same_v<List::reverse_iterator, std::reverse_iterator<List::iterator>>,
                  "reverse_iterator should be std::reverse_iterator<iterator>");
    static_assert(std::is_same_v<List::const_reverse_iterator, std::reverse_iterator<List::const_iterator>>,
                  "const_reverse_iterator should be std::reverse_iterator<const_iterator>");

    // Verify they can be used
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    List list;
    list.push_back(n1);
    list.push_back(n2);

    List::reverse_iterator rit = list.rbegin();
    FATP_ASSERT_EQ(rit->value, 2, "reverse_iterator should point to last element");

    List::const_reverse_iterator crit = list.crbegin();
    FATP_ASSERT_EQ(crit->value, 2, "const_reverse_iterator should point to last element");

    list.clear();
    return true;
}

FATP_TEST_CASE(intrusive_list_iterator_conversion)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);

        // Get mutable iterator
        fat_p::IntrusiveList<TestNode>::iterator it = list.begin();

        // Convert to const_iterator (this was broken before fix)
        fat_p::IntrusiveList<TestNode>::const_iterator cit = it;

        FATP_ASSERT_EQ(cit->value, 1, "const_iterator should point to first element");

        ++cit;
        FATP_ASSERT_EQ(cit->value, 2, "const_iterator should advance");

        // Verify const correctness - this should compile
        const fat_p::IntrusiveList<TestNode>& const_list = list;
        for (const auto& node : const_list)
        {
            (void)node.value;  // Should work with const iteration
        }
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_reverse_iteration)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        std::vector<int> values;
        for (auto it = list.rbegin(); it != list.rend(); ++it)
        {
            values.push_back(it->value);
        }

        FATP_ASSERT_EQ(values.size(), 3u, "Reverse iteration should visit 3 elements");
        FATP_ASSERT_EQ(values[0], 3, "First reverse should be 3");
        FATP_ASSERT_EQ(values[1], 2, "Second reverse should be 2");
        FATP_ASSERT_EQ(values[2], 1, "Third reverse should be 1");

        const fat_p::IntrusiveList<TestNode>& const_list = list;
        std::vector<int> const_values;
        for (auto it = const_list.crbegin(); it != const_list.crend(); ++it)
        {
            const_values.push_back(it->value);
        }

        FATP_ASSERT_EQ(const_values.size(), 3u, "Const reverse iteration should visit 3 elements");
        FATP_ASSERT_EQ(const_values[0], 3, "First const reverse should be 3");
        FATP_ASSERT_EQ(const_values[1], 2, "Second const reverse should be 2");
        FATP_ASSERT_EQ(const_values[2], 1, "Third const reverse should be 1");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_decrement_end)
{
    TestNode n1(42, "single");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);

        auto it = list.end();
        --it;
        FATP_ASSERT_EQ(it->value, 42, "--end() should point to last element");

        const fat_p::IntrusiveList<TestNode>& const_list = list;
        auto cit = const_list.end();
        --cit;
        FATP_ASSERT_EQ(cit->value, 42, "--const end() should point to last element");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_iterator_to_fast)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;

        // Unlinked node: iteratorTo returns end().
        FATP_ASSERT_TRUE(list.iteratorTo(n3) == list.end(), "iteratorTo(unlinked) should be end()");

        list.push_back(n1);
        list.push_back(n2);

        auto it = list.iteratorTo(n2);
        FATP_ASSERT_TRUE(it != list.end(), "iteratorTo(linked) should not be end()");
        FATP_ASSERT_EQ(it->value, 2, "iteratorTo should point to the requested node");

        const fat_p::IntrusiveList<TestNode>& const_list = list;
        auto cit = const_list.iteratorTo(n1);
        FATP_ASSERT_TRUE(cit != const_list.end(), "const iteratorTo(linked) should not be end()");
        FATP_ASSERT_EQ(cit->value, 1, "const iteratorTo should point to the requested node");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_iterator_to_safe)
{
    TestNodeWithOwner node(7);

    {
        fat_p::IntrusiveListSafe<TestNodeWithOwner> listA;
        fat_p::IntrusiveListSafe<TestNodeWithOwner> listB;

        listB.push_back(node);

        // Wrong-list: safe policy should return end().
        FATP_ASSERT_TRUE(listA.iteratorTo(node) == listA.end(), "Safe iteratorTo(wrong list) should be end()");

        auto it = listB.iteratorTo(node);
        FATP_ASSERT_TRUE(it != listB.end(), "Safe iteratorTo(correct list) should not be end()");
        FATP_ASSERT_EQ(it->value, 7, "Safe iteratorTo should point to the requested node");

        const fat_p::IntrusiveListSafe<TestNodeWithOwner>& const_listB = listB;
        auto cit = const_listB.iteratorTo(node);
        FATP_ASSERT_TRUE(cit != const_listB.end(), "const safe iteratorTo(correct list) should not be end()");
        FATP_ASSERT_EQ(cit->value, 7, "const safe iteratorTo should point to the requested node");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_is_linked)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    // Initially not linked
    FATP_ASSERT_FALSE(n1.isLinked(), "n1 should not be linked initially");
    FATP_ASSERT_FALSE(n2.isLinked(), "n2 should not be linked initially");
    FATP_ASSERT_FALSE(n3.isLinked(), "n3 should not be linked initially");


    {
        fat_p::IntrusiveList<TestNode> list;

        // After push
        list.push_back(n1);
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked after push");

        list.push_back(n2);
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should still be linked");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should be linked after push");

        list.push_back(n3);
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should still be linked");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should still be linked");
        FATP_ASSERT_TRUE(n3.isLinked(), "n3 should be linked after push");

        // After remove
        list.remove(n2);
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should still be linked");
        FATP_ASSERT_FALSE(n2.isLinked(), "n2 should not be linked after remove");
        FATP_ASSERT_TRUE(n3.isLinked(), "n3 should still be linked");

        // After clear
        list.clear();
        FATP_ASSERT_FALSE(n1.isLinked(), "n1 should not be linked after clear");
        FATP_ASSERT_FALSE(n3.isLinked(), "n3 should not be linked after clear");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_remove_unlinked)
{
    TestNode n1(1, "one");
    TestNode unlinked(99, "never added");

    {
        fat_p::IntrusiveList<TestNode> list;

        list.push_back(n1);

        // Remove a node that was never added - should be safe no-op
        list.remove(unlinked);

        FATP_ASSERT_EQ(list.size(), 1u, "Size should be unchanged");
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should still be linked");
        FATP_ASSERT_FALSE(unlinked.isLinked(), "unlinked should still not be linked");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_cross_list_remove)
{
    // Cross-list remove is only well-defined when ownership tracking is enabled.
    TestNodeWithOwner node(42);

    {
        fat_p::IntrusiveListSafe<TestNodeWithOwner> listA;
        fat_p::IntrusiveListSafe<TestNodeWithOwner> listB;

        listB.push_back(node);

        FATP_ASSERT_EQ(listB.size(), 1u, "listB should have 1 element");
        FATP_ASSERT_TRUE(node.isLinked(), "node should be linked");

        // Try to remove from wrong list - should be safe no-op
        listA.remove(node);

        // listB should be unchanged
        FATP_ASSERT_EQ(listB.size(), 1u, "listB size should be unchanged");
        FATP_ASSERT_TRUE(node.isLinked(), "node should still be linked to listB");
        FATP_ASSERT_EQ(&listB.front(), &node, "listB front should still be node");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_splice_at_begin)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");

    {
        fat_p::IntrusiveList<TestNode> list1;
        fat_p::IntrusiveList<TestNode> list2;

        list1.push_back(n1);
        list1.push_back(n2);

        list2.push_back(n3);
        list2.push_back(n4);

        // Splice list2 at the beginning of list1
        list1.splice(list1.begin(), list2);

        FATP_ASSERT_EQ(list1.size(), 4u, "list1 should have 4 elements");
        FATP_ASSERT_TRUE(list2.empty(), "list2 should be empty");

        std::vector<int> values;
        for (const auto& node : list1)
        {
            values.push_back(node.value);
        }

        FATP_ASSERT_EQ(values[0], 3, "First should be 3");
        FATP_ASSERT_EQ(values[1], 4, "Second should be 4");
        FATP_ASSERT_EQ(values[2], 1, "Third should be 1");
        FATP_ASSERT_EQ(values[3], 2, "Fourth should be 2");

        // Verify nodes remain linked after transfer.
        FATP_ASSERT_TRUE(n3.isLinked(), "n3 should be linked");
        FATP_ASSERT_TRUE(n4.isLinked(), "n4 should be linked");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_move_ownership)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list1;

        list1.push_back(n1);
        list1.push_back(n2);

        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked before move");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should be linked before move");

        // Move to a new list
        fat_p::IntrusiveList<TestNode> list2(std::move(list1));

        // Nodes should still be linked (ownership transferred)
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked after move");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should be linked after move");

        // Removing from list2 should work
        list2.remove(n1);
        FATP_ASSERT_FALSE(n1.isLinked(), "n1 should not be linked after remove from list2");
        FATP_ASSERT_EQ(list2.size(), 1u, "list2 should have 1 element");
    }

    return true;
}

// ============================================================================
// SafeOwnerPolicy owner-update tests (P2-9)
// Verify iteratorTo works on destination and fails on source after move/splice
// ============================================================================

FATP_TEST_CASE(intrusive_list_safe_owner_move_construction)
{
    // Verify owner pointers are updated after move construction
    TestNodeWithOwner n1(1);
    TestNodeWithOwner n2(2);

    {
        fat_p::IntrusiveListSafe<TestNodeWithOwner> source;
        source.push_back(n1);
        source.push_back(n2);

        // Before move: iteratorTo works on source
        FATP_ASSERT_TRUE(source.iteratorTo(n1) != source.end(), "iteratorTo should work on source before move");
        FATP_ASSERT_TRUE(source.iteratorTo(n2) != source.end(), "iteratorTo should work on source before move");

        // Move construction
        fat_p::IntrusiveListSafe<TestNodeWithOwner> dest(std::move(source));

        // After move: iteratorTo should work on dest
        FATP_ASSERT_TRUE(dest.iteratorTo(n1) != dest.end(), "iteratorTo should work on dest after move");
        FATP_ASSERT_TRUE(dest.iteratorTo(n2) != dest.end(), "iteratorTo should work on dest after move");
        FATP_ASSERT_EQ(dest.iteratorTo(n1)->value, 1, "iteratorTo should return correct node");
        FATP_ASSERT_EQ(dest.iteratorTo(n2)->value, 2, "iteratorTo should return correct node");

        // After move: iteratorTo should fail on source (nodes no longer owned by source)
        FATP_ASSERT_TRUE(source.iteratorTo(n1) == source.end(), "iteratorTo should fail on source after move");
        FATP_ASSERT_TRUE(source.iteratorTo(n2) == source.end(), "iteratorTo should fail on source after move");

        // Verify source is empty
        FATP_ASSERT_TRUE(source.empty(), "source should be empty after move");
        FATP_ASSERT_EQ(dest.size(), 2u, "dest should have 2 elements");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_safe_owner_move_assignment)
{
    // Verify owner pointers are updated after move assignment
    TestNodeWithOwner n1(1);
    TestNodeWithOwner n2(2);
    TestNodeWithOwner n3(3);

    {
        fat_p::IntrusiveListSafe<TestNodeWithOwner> source;
        fat_p::IntrusiveListSafe<TestNodeWithOwner> dest;

        source.push_back(n1);
        source.push_back(n2);
        dest.push_back(n3);

        // Before move: verify ownership
        FATP_ASSERT_TRUE(source.iteratorTo(n1) != source.end(), "n1 owned by source");
        FATP_ASSERT_TRUE(dest.iteratorTo(n3) != dest.end(), "n3 owned by dest");

        // Move assignment (n3 will be cleared from dest)
        dest = std::move(source);

        // After move: iteratorTo should work on dest for n1, n2
        FATP_ASSERT_TRUE(dest.iteratorTo(n1) != dest.end(), "n1 should be owned by dest");
        FATP_ASSERT_TRUE(dest.iteratorTo(n2) != dest.end(), "n2 should be owned by dest");

        // After move: iteratorTo should fail on source
        FATP_ASSERT_TRUE(source.iteratorTo(n1) == source.end(), "n1 should not be owned by source");
        FATP_ASSERT_TRUE(source.iteratorTo(n2) == source.end(), "n2 should not be owned by source");

        // n3 should have been cleared and is no longer linked
        FATP_ASSERT_FALSE(n3.isLinked(), "n3 should be unlinked after dest was overwritten");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_safe_owner_splice)
{
    // Verify owner pointers are updated after splice
    TestNodeWithOwner n1(1);
    TestNodeWithOwner n2(2);
    TestNodeWithOwner n3(3);
    TestNodeWithOwner n4(4);

    {
        fat_p::IntrusiveListSafe<TestNodeWithOwner> dest;
        fat_p::IntrusiveListSafe<TestNodeWithOwner> source;

        dest.push_back(n1);
        dest.push_back(n2);
        source.push_back(n3);
        source.push_back(n4);

        // Before splice: verify ownership
        FATP_ASSERT_TRUE(dest.iteratorTo(n1) != dest.end(), "n1 owned by dest");
        FATP_ASSERT_TRUE(dest.iteratorTo(n2) != dest.end(), "n2 owned by dest");
        FATP_ASSERT_TRUE(source.iteratorTo(n3) != source.end(), "n3 owned by source");
        FATP_ASSERT_TRUE(source.iteratorTo(n4) != source.end(), "n4 owned by source");

        // Splice source into dest
        dest.splice(dest.end(), source);

        // After splice: all nodes should be owned by dest
        FATP_ASSERT_TRUE(dest.iteratorTo(n1) != dest.end(), "n1 should be owned by dest");
        FATP_ASSERT_TRUE(dest.iteratorTo(n2) != dest.end(), "n2 should be owned by dest");
        FATP_ASSERT_TRUE(dest.iteratorTo(n3) != dest.end(), "n3 should be owned by dest after splice");
        FATP_ASSERT_TRUE(dest.iteratorTo(n4) != dest.end(), "n4 should be owned by dest after splice");

        // After splice: source should not own n3, n4
        FATP_ASSERT_TRUE(source.iteratorTo(n3) == source.end(), "n3 should not be owned by source");
        FATP_ASSERT_TRUE(source.iteratorTo(n4) == source.end(), "n4 should not be owned by source");

        // Source should be empty
        FATP_ASSERT_TRUE(source.empty(), "source should be empty after splice");
        FATP_ASSERT_EQ(dest.size(), 4u, "dest should have 4 elements");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_splice_empty_source)
{
    // Verify splice with empty source is a no-op
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> dest;
        fat_p::IntrusiveList<TestNode> emptySource;

        dest.push_back(n1);
        dest.push_back(n2);

        FATP_ASSERT_EQ(dest.size(), 2u, "dest should have 2 elements before splice");
        FATP_ASSERT_TRUE(emptySource.empty(), "source should be empty");

        // Splice empty list - should be no-op
        dest.splice(dest.end(), emptySource);

        FATP_ASSERT_EQ(dest.size(), 2u, "dest should still have 2 elements");
        FATP_ASSERT_TRUE(emptySource.empty(), "source should still be empty");
    }

    return true;
}

FATP_TEST_CASE(intrusive_list_move_assignment_to_nonempty)
{
    // Verify move assignment clears destination first
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> source;
        fat_p::IntrusiveList<TestNode> dest;

        source.push_back(n1);
        dest.push_back(n2);
        dest.push_back(n3);

        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should be linked before move");
        FATP_ASSERT_TRUE(n3.isLinked(), "n3 should be linked before move");
        FATP_ASSERT_EQ(dest.size(), 2u, "dest should have 2 elements");

        // Move assignment to non-empty list
        dest = std::move(source);

        // Dest's old nodes should be unlinked
        FATP_ASSERT_FALSE(n2.isLinked(), "n2 should be unlinked after dest overwritten");
        FATP_ASSERT_FALSE(n3.isLinked(), "n3 should be unlinked after dest overwritten");

        // Dest should now contain source's nodes
        FATP_ASSERT_EQ(dest.size(), 1u, "dest should have 1 element");
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked");
        FATP_ASSERT_EQ(&dest.front(), &n1, "dest front should be n1");
    }

    return true;
}

// ============================================================================
// Corner Case / Edge Case Tests
// ============================================================================

FATP_TEST_CASE(corner_empty_list_operations)
{
    // Verify all operations on empty list are safe
    fat_p::IntrusiveList<TestNode> list;

    // Size/empty checks
    FATP_ASSERT_TRUE(list.empty(), "New list should be empty");
    FATP_ASSERT_EQ(list.size(), 0u, "New list should have size 0");

    // Iteration on empty list
    FATP_ASSERT_TRUE(list.begin() == list.end(), "begin == end for empty list");
    FATP_ASSERT_TRUE(list.cbegin() == list.cend(), "cbegin == cend for empty list");
    FATP_ASSERT_TRUE(list.rbegin() == list.rend(), "rbegin == rend for empty list");

    int count = 0;
    for ([[maybe_unused]] auto& node : list)
    {
        ++count;
    }
    FATP_ASSERT_EQ(count, 0, "Should not iterate over empty list");

    // pop on empty - should be no-op
    list.pop_front();
    FATP_ASSERT_TRUE(list.empty(), "pop_front on empty should be no-op");
    FATP_ASSERT_EQ(list.size(), 0u, "Size should remain 0");

    list.pop_back();
    FATP_ASSERT_TRUE(list.empty(), "pop_back on empty should be no-op");
    FATP_ASSERT_EQ(list.size(), 0u, "Size should remain 0");

    // clear on empty - should be no-op
    list.clear();
    FATP_ASSERT_TRUE(list.empty(), "clear on empty should be no-op");

    // erase end() - should return end()
    auto it = list.erase(list.end());
    FATP_ASSERT_TRUE(it == list.end(), "erase(end()) should return end()");

    return true;
}

FATP_TEST_CASE(corner_two_element_list)
{
    // Two-element list is a boundary case for many operations
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);

        // Verify structure
        FATP_ASSERT_EQ(list.size(), 2u, "Size should be 2");
        FATP_ASSERT_EQ(&list.front(), &n1, "Front should be n1");
        FATP_ASSERT_EQ(&list.back(), &n2, "Back should be n2");

        // Forward iteration
        auto it = list.begin();
        FATP_ASSERT_EQ(&(*it), &n1, "First element");
        ++it;
        FATP_ASSERT_EQ(&(*it), &n2, "Second element");
        ++it;
        FATP_ASSERT_TRUE(it == list.end(), "Should reach end");

        // Reverse iteration
        auto rit = list.rbegin();
        FATP_ASSERT_EQ(&(*rit), &n2, "First reverse element");
        ++rit;
        FATP_ASSERT_EQ(&(*rit), &n1, "Second reverse element");
        ++rit;
        FATP_ASSERT_TRUE(rit == list.rend(), "Should reach rend");

        // Remove first, leaving single element
        list.remove(n1);
        FATP_ASSERT_EQ(list.size(), 1u, "Size should be 1");
        FATP_ASSERT_EQ(&list.front(), &n2, "Front should be n2");
        FATP_ASSERT_EQ(&list.back(), &n2, "Back should also be n2");
    }

    return true;
}

FATP_TEST_CASE(corner_insert_at_begin)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n2);
        list.push_back(n3);

        // Insert at begin
        auto it = list.insert(list.begin(), n1);

        FATP_ASSERT_EQ(&(*it), &n1, "insert should return iterator to inserted element");
        FATP_ASSERT_EQ(list.size(), 3u, "Size should be 3");
        FATP_ASSERT_EQ(&list.front(), &n1, "Front should be n1");

        // Verify order: 1, 2, 3
        std::vector<int> values;
        for (auto& node : list)
        {
            values.push_back(node.value);
        }
        FATP_ASSERT_EQ(values[0], 1, "First should be 1");
        FATP_ASSERT_EQ(values[1], 2, "Second should be 2");
        FATP_ASSERT_EQ(values[2], 3, "Third should be 3");
    }

    return true;
}

FATP_TEST_CASE(corner_erase_first_and_last)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        // Erase first element
        auto it = list.erase(list.begin());
        FATP_ASSERT_EQ(&(*it), &n2, "erase(begin) should return iterator to new first");
        FATP_ASSERT_EQ(list.size(), 2u, "Size should be 2");
        FATP_ASSERT_EQ(&list.front(), &n2, "Front should be n2");
        FATP_ASSERT_FALSE(n1.isLinked(), "n1 should be unlinked");

        // Erase last element (n3)
        auto lastIt = list.begin();
        ++lastIt;  // Points to n3
        it = list.erase(lastIt);
        FATP_ASSERT_TRUE(it == list.end(), "erase(last) should return end()");
        FATP_ASSERT_EQ(list.size(), 1u, "Size should be 1");
        FATP_ASSERT_EQ(&list.back(), &n2, "Back should be n2");
        FATP_ASSERT_FALSE(n3.isLinked(), "n3 should be unlinked");
    }

    return true;
}

FATP_TEST_CASE(corner_self_splice)
{
    // Splice list into itself should be no-op
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);

        size_t sizeBefore = list.size();

        // Self-splice should be no-op
        list.splice(list.end(), list);

        FATP_ASSERT_EQ(list.size(), sizeBefore, "Self-splice should not change size");
        FATP_ASSERT_EQ(&list.front(), &n1, "Front should still be n1");
        FATP_ASSERT_EQ(&list.back(), &n2, "Back should still be n2");
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should still be linked");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should still be linked");
    }

    return true;
}

FATP_TEST_CASE(corner_move_from_empty)
{
    // Move from empty list
    {
        fat_p::IntrusiveList<TestNode> empty;
        fat_p::IntrusiveList<TestNode> dest(std::move(empty));

        FATP_ASSERT_TRUE(dest.empty(), "Move from empty should result in empty");
        FATP_ASSERT_TRUE(empty.empty(), "Source should remain empty");
    }

    // Move assign from empty to non-empty
    TestNode n1(1, "one");
    {
        fat_p::IntrusiveList<TestNode> empty;
        fat_p::IntrusiveList<TestNode> dest;
        dest.push_back(n1);

        dest = std::move(empty);

        FATP_ASSERT_TRUE(dest.empty(), "Dest should be empty after move from empty");
        FATP_ASSERT_FALSE(n1.isLinked(), "n1 should be unlinked");
    }

    return true;
}

FATP_TEST_CASE(corner_double_remove)
{
    // Remove same node twice should be safe (second is no-op)
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);

        list.remove(n1);
        FATP_ASSERT_FALSE(n1.isLinked(), "n1 should be unlinked after first remove");
        FATP_ASSERT_EQ(list.size(), 1u, "Size should be 1");

        // Second remove should be no-op
        list.remove(n1);
        FATP_ASSERT_FALSE(n1.isLinked(), "n1 should still be unlinked");
        FATP_ASSERT_EQ(list.size(), 1u, "Size should still be 1");
    }

    return true;
}

FATP_TEST_CASE(corner_splice_in_middle)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");

    {
        fat_p::IntrusiveList<TestNode> dest;
        fat_p::IntrusiveList<TestNode> src;

        dest.push_back(n1);
        dest.push_back(n4);

        src.push_back(n2);
        src.push_back(n3);

        // Splice in middle (before n4)
        auto it = dest.begin();
        ++it;  // Points to n4
        dest.splice(it, src);

        FATP_ASSERT_EQ(dest.size(), 4u, "Dest should have 4 elements");
        FATP_ASSERT_TRUE(src.empty(), "Src should be empty");

        // Verify order: 1, 2, 3, 4
        std::vector<int> values;
        for (auto& node : dest)
        {
            values.push_back(node.value);
        }
        FATP_ASSERT_EQ(values[0], 1, "First should be 1");
        FATP_ASSERT_EQ(values[1], 2, "Second should be 2");
        FATP_ASSERT_EQ(values[2], 3, "Third should be 3");
        FATP_ASSERT_EQ(values[3], 4, "Fourth should be 4");
    }

    return true;
}

FATP_TEST_CASE(corner_const_iteration)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);

        const fat_p::IntrusiveList<TestNode>& constList = list;

        // cbegin/cend
        std::vector<int> values;
        for (auto it = constList.cbegin(); it != constList.cend(); ++it)
        {
            values.push_back(it->value);
        }
        FATP_ASSERT_EQ(values.size(), 2u, "Should iterate 2 elements via cbegin/cend");

        // crbegin/crend
        std::vector<int> rvalues;
        for (auto it = constList.crbegin(); it != constList.crend(); ++it)
        {
            rvalues.push_back(it->value);
        }
        FATP_ASSERT_EQ(rvalues.size(), 2u, "Should iterate 2 elements via crbegin/crend");
        FATP_ASSERT_EQ(rvalues[0], 2, "First reverse should be 2");
        FATP_ASSERT_EQ(rvalues[1], 1, "Second reverse should be 1");

        // Range-based for on const
        int sum = 0;
        for (const auto& node : constList)
        {
            sum += node.value;
        }
        FATP_ASSERT_EQ(sum, 3, "Sum should be 3");
    }

    return true;
}

FATP_TEST_CASE(corner_front_back_same_element)
{
    // Single element: front and back are the same
    TestNode n1(42, "single");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);

        FATP_ASSERT_EQ(&list.front(), &list.back(), "front == back for single element");
        FATP_ASSERT_EQ(&list.front(), &n1, "front should be n1");

        // Const versions
        const fat_p::IntrusiveList<TestNode>& constList = list;
        FATP_ASSERT_EQ(&constList.front(), &constList.back(), "const front == back");
    }

    return true;
}

FATP_TEST_CASE(corner_iterator_equality)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);

        auto it1 = list.begin();
        auto it2 = list.begin();
        auto it3 = list.end();

        // Same position
        FATP_ASSERT_TRUE(it1 == it2, "begin() == begin()");
        FATP_ASSERT_FALSE(it1 != it2, "!(begin() != begin())");

        // Different positions
        ++it2;
        FATP_ASSERT_FALSE(it1 == it2, "begin() != second element");
        FATP_ASSERT_TRUE(it1 != it2, "begin() != second element");

        // End
        FATP_ASSERT_TRUE(it3 == list.end(), "end() == end()");
        FATP_ASSERT_FALSE(it1 == it3, "begin() != end()");
    }

    return true;
}

FATP_TEST_CASE(corner_post_increment_decrement)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);

        // Post-increment returns old value
        auto it = list.begin();
        auto old = it++;
        FATP_ASSERT_EQ(old->value, 1, "Post-increment should return old position");
        FATP_ASSERT_EQ(it->value, 2, "Iterator should advance");

        // Post-decrement
        auto it2 = list.end();
        --it2;  // Now at n2
        auto old2 = it2--;
        FATP_ASSERT_EQ(old2->value, 2, "Post-decrement should return old position");
        FATP_ASSERT_EQ(it2->value, 1, "Iterator should go back");
    }

    return true;
}

FATP_TEST_CASE(corner_safe_policy_unlinked_iteratorTo)
{
    // SafeOwnerPolicy: iteratorTo on unlinked node should return end()
    TestNodeWithOwner node(42);

    {
        fat_p::IntrusiveListSafe<TestNodeWithOwner> list;

        // Node never added
        FATP_ASSERT_TRUE(list.iteratorTo(node) == list.end(),
                         "iteratorTo(unlinked) should return end()");

        // Add then remove
        list.push_back(node);
        FATP_ASSERT_TRUE(list.iteratorTo(node) != list.end(),
                         "iteratorTo(linked) should not return end()");

        list.remove(node);
        FATP_ASSERT_TRUE(list.iteratorTo(node) == list.end(),
                         "iteratorTo(removed) should return end()");
    }

    return true;
}

// ============================================================================
// Swap Tests
// ============================================================================

FATP_TEST_CASE(swap_basic)
{
    // Basic swap between two non-empty lists
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");

    {
        fat_p::IntrusiveList<TestNode> list1;
        fat_p::IntrusiveList<TestNode> list2;

        list1.push_back(n1);
        list1.push_back(n2);
        list2.push_back(n3);
        list2.push_back(n4);

        FATP_ASSERT_EQ(list1.size(), 2u, "list1 should have 2 elements");
        FATP_ASSERT_EQ(list2.size(), 2u, "list2 should have 2 elements");

        list1.swap(list2);

        // After swap: list1 should have n3, n4; list2 should have n1, n2
        FATP_ASSERT_EQ(list1.size(), 2u, "list1 should still have 2 elements");
        FATP_ASSERT_EQ(list2.size(), 2u, "list2 should still have 2 elements");

        FATP_ASSERT_EQ(&list1.front(), &n3, "list1 front should be n3");
        FATP_ASSERT_EQ(&list1.back(), &n4, "list1 back should be n4");
        FATP_ASSERT_EQ(&list2.front(), &n1, "list2 front should be n1");
        FATP_ASSERT_EQ(&list2.back(), &n2, "list2 back should be n2");

        // Verify iteration
        std::vector<int> list1Values;
        for (auto& node : list1)
        {
            list1Values.push_back(node.value);
        }
        FATP_ASSERT_EQ(list1Values.size(), 2u, "list1 should iterate 2 elements");
        FATP_ASSERT_EQ(list1Values[0], 3, "list1 first should be 3");
        FATP_ASSERT_EQ(list1Values[1], 4, "list1 second should be 4");
    }

    return true;
}

FATP_TEST_CASE(swap_empty_with_nonempty)
{
    // Swap empty list with non-empty list
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> empty;
        fat_p::IntrusiveList<TestNode> nonempty;

        nonempty.push_back(n1);
        nonempty.push_back(n2);

        FATP_ASSERT_TRUE(empty.empty(), "empty should be empty");
        FATP_ASSERT_EQ(nonempty.size(), 2u, "nonempty should have 2 elements");

        empty.swap(nonempty);

        FATP_ASSERT_EQ(empty.size(), 2u, "empty should now have 2 elements");
        FATP_ASSERT_TRUE(nonempty.empty(), "nonempty should now be empty");
        FATP_ASSERT_EQ(&empty.front(), &n1, "empty front should be n1");
        FATP_ASSERT_EQ(&empty.back(), &n2, "empty back should be n2");

        // Swap back
        empty.swap(nonempty);

        FATP_ASSERT_TRUE(empty.empty(), "empty should be empty again");
        FATP_ASSERT_EQ(nonempty.size(), 2u, "nonempty should have 2 again");
    }

    return true;
}

FATP_TEST_CASE(swap_both_empty)
{
    // Swap two empty lists - should be no-op
    fat_p::IntrusiveList<TestNode> list1;
    fat_p::IntrusiveList<TestNode> list2;

    FATP_ASSERT_TRUE(list1.empty(), "list1 should be empty");
    FATP_ASSERT_TRUE(list2.empty(), "list2 should be empty");

    list1.swap(list2);

    FATP_ASSERT_TRUE(list1.empty(), "list1 should still be empty");
    FATP_ASSERT_TRUE(list2.empty(), "list2 should still be empty");

    return true;
}

FATP_TEST_CASE(swap_self)
{
    // Self-swap should be no-op
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        fat_p::IntrusiveList<TestNode> list;
        list.push_back(n1);
        list.push_back(n2);

        size_t sizeBefore = list.size();

        list.swap(list);

        FATP_ASSERT_EQ(list.size(), sizeBefore, "Self-swap should not change size");
        FATP_ASSERT_EQ(&list.front(), &n1, "Front should still be n1");
        FATP_ASSERT_EQ(&list.back(), &n2, "Back should still be n2");
    }

    return true;
}

FATP_TEST_CASE(swap_safe_policy)
{
    // Swap with SafeOwnerPolicy should update owner pointers
    TestNodeWithOwner n1(1);
    TestNodeWithOwner n2(2);
    TestNodeWithOwner n3(3);
    TestNodeWithOwner n4(4);

    {
        fat_p::IntrusiveListSafe<TestNodeWithOwner> list1;
        fat_p::IntrusiveListSafe<TestNodeWithOwner> list2;

        list1.push_back(n1);
        list1.push_back(n2);
        list2.push_back(n3);
        list2.push_back(n4);

        // Before swap: verify ownership
        FATP_ASSERT_TRUE(list1.iteratorTo(n1) != list1.end(), "n1 owned by list1");
        FATP_ASSERT_TRUE(list1.iteratorTo(n2) != list1.end(), "n2 owned by list1");
        FATP_ASSERT_TRUE(list2.iteratorTo(n3) != list2.end(), "n3 owned by list2");
        FATP_ASSERT_TRUE(list2.iteratorTo(n4) != list2.end(), "n4 owned by list2");

        list1.swap(list2);

        // After swap: ownership should be updated
        FATP_ASSERT_TRUE(list1.iteratorTo(n3) != list1.end(), "n3 should now be owned by list1");
        FATP_ASSERT_TRUE(list1.iteratorTo(n4) != list1.end(), "n4 should now be owned by list1");
        FATP_ASSERT_TRUE(list2.iteratorTo(n1) != list2.end(), "n1 should now be owned by list2");
        FATP_ASSERT_TRUE(list2.iteratorTo(n2) != list2.end(), "n2 should now be owned by list2");

        // Verify old ownership is invalid
        FATP_ASSERT_TRUE(list1.iteratorTo(n1) == list1.end(), "n1 should not be owned by list1");
        FATP_ASSERT_TRUE(list2.iteratorTo(n3) == list2.end(), "n3 should not be owned by list2");
    }

    return true;
}

FATP_TEST_CASE(swap_adl)
{
    // Test ADL-based swap (free function)
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");

    {
        fat_p::IntrusiveList<TestNode> list1;
        fat_p::IntrusiveList<TestNode> list2;

        list1.push_back(n1);
        list2.push_back(n2);
        list2.push_back(n3);

        using std::swap;  // Enable ADL
        swap(list1, list2);

        FATP_ASSERT_EQ(list1.size(), 2u, "list1 should have 2 elements after ADL swap");
        FATP_ASSERT_EQ(list2.size(), 1u, "list2 should have 1 element after ADL swap");
        FATP_ASSERT_EQ(&list1.front(), &n2, "list1 front should be n2");
        FATP_ASSERT_EQ(&list2.front(), &n1, "list2 front should be n1");
    }

    return true;
}

// ============================================================================
// RAII Correctness Tests
// ============================================================================

FATP_TEST_CASE(raii_lifecycle_clear)
{
    // Verify clear() properly unlinks nodes (nodes have external lifetime)
    LifecycleNode::reset();

    {
        LifecycleNode n1(1);
        LifecycleNode n2(2);
        LifecycleNode n3(3);

        FATP_ASSERT_EQ(LifecycleNode::constructCount.load(), 3, "Should construct 3 nodes");

        {
            fat_p::IntrusiveList<LifecycleNode> list;
            list.push_back(n1);
            list.push_back(n2);
            list.push_back(n3);

            FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked");
            FATP_ASSERT_TRUE(n2.isLinked(), "n2 should be linked");
            FATP_ASSERT_TRUE(n3.isLinked(), "n3 should be linked");

            list.clear();

            FATP_ASSERT_FALSE(n1.isLinked(), "n1 should be unlinked after clear");
            FATP_ASSERT_FALSE(n2.isLinked(), "n2 should be unlinked after clear");
            FATP_ASSERT_FALSE(n3.isLinked(), "n3 should be unlinked after clear");
        }

        // Nodes still exist (external lifetime)
        FATP_ASSERT_EQ(LifecycleNode::destructCount.load(), 0, "Nodes not destroyed yet");
    }

    // Now nodes go out of scope
    FATP_ASSERT_EQ(LifecycleNode::destructCount.load(), 3, "All nodes should be destroyed");

    return true;
}

FATP_TEST_CASE(raii_lifecycle_list_destruction)
{
    // Verify list destructor properly unlinks all nodes
    LifecycleNode::reset();

    LifecycleNode n1(1);
    LifecycleNode n2(2);

    {
        fat_p::IntrusiveList<LifecycleNode> list;
        list.push_back(n1);
        list.push_back(n2);

        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should be linked");
    }  // list destroyed here

    // Nodes should be unlinked after list destruction
    FATP_ASSERT_FALSE(n1.isLinked(), "n1 should be unlinked after list destruction");
    FATP_ASSERT_FALSE(n2.isLinked(), "n2 should be unlinked after list destruction");

    return true;
}

FATP_TEST_CASE(raii_no_double_unlink)
{
    // Verify no issues with nodes that outlive the list
    LifecycleNode::reset();

    LifecycleNode n1(1);

    {
        fat_p::IntrusiveList<LifecycleNode> list;
        list.push_back(n1);
    }

    // Node is unlinked, can be safely added to another list
    FATP_ASSERT_FALSE(n1.isLinked(), "n1 should be unlinked");

    {
        fat_p::IntrusiveList<LifecycleNode> list2;
        list2.push_back(n1);
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked to list2");
    }

    FATP_ASSERT_FALSE(n1.isLinked(), "n1 should be unlinked after list2 destruction");

    return true;
}

// ============================================================================
// Move-Only Type Tests
// ============================================================================

FATP_TEST_CASE(move_only_basic_operations)
{
    // Verify list works with move-only node types
    MoveOnlyNode n1(10);
    MoveOnlyNode n2(20);
    MoveOnlyNode n3(30);

    {
        fat_p::IntrusiveList<MoveOnlyNode> list;

        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        FATP_ASSERT_EQ(list.size(), 3u, "Size should be 3");

        // Access through iteration
        int sum = 0;
        for (const auto& node : list)
        {
            sum += node.getValue();
        }
        FATP_ASSERT_EQ(sum, 60, "Sum should be 60");

        // Remove
        list.remove(n2);
        FATP_ASSERT_EQ(list.size(), 2u, "Size should be 2");
        FATP_ASSERT_FALSE(n2.isLinked(), "n2 should be unlinked");

        list.clear();
    }

    return true;
}

FATP_TEST_CASE(move_only_splice)
{
    MoveOnlyNode n1(1);
    MoveOnlyNode n2(2);
    MoveOnlyNode n3(3);
    MoveOnlyNode n4(4);

    {
        fat_p::IntrusiveList<MoveOnlyNode> list1;
        fat_p::IntrusiveList<MoveOnlyNode> list2;

        list1.push_back(n1);
        list1.push_back(n2);
        list2.push_back(n3);
        list2.push_back(n4);

        list1.splice(list1.end(), list2);

        FATP_ASSERT_EQ(list1.size(), 4u, "list1 should have 4 elements");
        FATP_ASSERT_TRUE(list2.empty(), "list2 should be empty");

        // Verify order
        auto it = list1.begin();
        FATP_ASSERT_EQ(it->getValue(), 1, "First should be 1");
        ++it;
        FATP_ASSERT_EQ(it->getValue(), 2, "Second should be 2");
        ++it;
        FATP_ASSERT_EQ(it->getValue(), 3, "Third should be 3");
        ++it;
        FATP_ASSERT_EQ(it->getValue(), 4, "Fourth should be 4");

        list1.clear();
    }

    return true;
}

FATP_TEST_CASE(move_only_list_move)
{
    MoveOnlyNode n1(100);
    MoveOnlyNode n2(200);

    {
        fat_p::IntrusiveList<MoveOnlyNode> list1;
        list1.push_back(n1);
        list1.push_back(n2);

        // Move construct
        fat_p::IntrusiveList<MoveOnlyNode> list2(std::move(list1));

        FATP_ASSERT_TRUE(list1.empty(), "list1 should be empty after move");
        FATP_ASSERT_EQ(list2.size(), 2u, "list2 should have 2 elements");
        FATP_ASSERT_EQ(list2.front().getValue(), 100, "Front should be 100");
        FATP_ASSERT_EQ(list2.back().getValue(), 200, "Back should be 200");

        // Move assign
        fat_p::IntrusiveList<MoveOnlyNode> list3;
        list3 = std::move(list2);

        FATP_ASSERT_TRUE(list2.empty(), "list2 should be empty after move");
        FATP_ASSERT_EQ(list3.size(), 2u, "list3 should have 2 elements");

        list3.clear();
    }

    return true;
}

// ============================================================================
// Exception Safety Tests
// ============================================================================

FATP_TEST_CASE(exception_safe_iteration)
{
    // Verify list remains consistent even if user code throws during iteration
    ThrowingNode::reset();
    ThrowingNode n1(1);
    ThrowingNode n2(2);
    ThrowingNode n3(3);

    {
        fat_p::IntrusiveList<ThrowingNode> list;
        list.push_back(n1);
        list.push_back(n2);
        list.push_back(n3);

        ThrowingNode::throwAfter = 2;

        bool threw = false;
        int processedCount = 0;

        try
        {
            for (auto& node : list)
            {
                node.riskyOperation();
                ++processedCount;
            }
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        FATP_ASSERT_TRUE(threw, "Should have thrown");
        FATP_ASSERT_EQ(processedCount, 1, "Should have processed 1 node before throw");

        // List should still be consistent
        FATP_ASSERT_EQ(list.size(), 3u, "List size unchanged");
        FATP_ASSERT_TRUE(n1.isLinked(), "n1 still linked");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 still linked");
        FATP_ASSERT_TRUE(n3.isLinked(), "n3 still linked");

        list.clear();
    }

    return true;
}

FATP_TEST_CASE(exception_safe_partial_operation)
{
    // Verify list can continue to be used after exception in user code
    ThrowingNode::reset();
    ThrowingNode n1(1);
    ThrowingNode n2(2);
    ThrowingNode n3(3);
    ThrowingNode n4(4);

    {
        fat_p::IntrusiveList<ThrowingNode> list;
        list.push_back(n1);
        list.push_back(n2);

        ThrowingNode::throwAfter = 1;

        bool threw = false;
        try
        {
            // User code that throws
            for (auto& node : list)
            {
                node.riskyOperation();
            }
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        FATP_ASSERT_TRUE(threw, "Should have thrown");

        // List operations should still work
        ThrowingNode::reset();  // Reset so no more throws

        list.push_back(n3);
        FATP_ASSERT_EQ(list.size(), 3u, "Can still push after exception");

        list.remove(n2);
        FATP_ASSERT_EQ(list.size(), 2u, "Can still remove after exception");

        list.push_front(n4);
        FATP_ASSERT_EQ(list.size(), 3u, "Can still push_front after exception");

        // Verify final state
        std::vector<int> values;
        for (const auto& node : list)
        {
            values.push_back(node.value);
        }
        FATP_ASSERT_EQ(values.size(), 3u, "Should have 3 elements");
        FATP_ASSERT_EQ(values[0], 4, "First should be 4");
        FATP_ASSERT_EQ(values[1], 1, "Second should be 1");
        FATP_ASSERT_EQ(values[2], 3, "Third should be 3");

        list.clear();
    }

    return true;
}

// ============================================================================
// Fuzz / Stress Tests
// ============================================================================

FATP_TEST_CASE(stress_random_operations)
{
    // Stress test: random push/pop/remove operations with reference oracle
    constexpr size_t kPoolSize = 200;
    constexpr size_t kIterations = 5000;
    constexpr unsigned kSeed = 42;

    // Pre-allocate node pool (intrusive nodes have external lifetime)
    std::vector<TestNode> pool(kPoolSize);
    for (size_t i = 0; i < kPoolSize; ++i)
    {
        pool[i].value = static_cast<int>(i);
    }

    fat_p::IntrusiveList<TestNode> list;
    std::list<TestNode*> reference;  // Oracle: std::list of pointers

    std::mt19937 rng(kSeed);
    std::uniform_int_distribution<size_t> indexDist(0, kPoolSize - 1);
    std::uniform_int_distribution<int> opDist(0, 5);

    for (size_t iter = 0; iter < kIterations; ++iter)
    {
        int op = opDist(rng);

        switch (op)
        {
        case 0: // push_back
        {
            size_t idx = indexDist(rng);
            if (!pool[idx].isLinked())
            {
                list.push_back(pool[idx]);
                reference.push_back(&pool[idx]);
            }
            break;
        }
        case 1: // push_front
        {
            size_t idx = indexDist(rng);
            if (!pool[idx].isLinked())
            {
                list.push_front(pool[idx]);
                reference.push_front(&pool[idx]);
            }
            break;
        }
        case 2: // pop_back
        {
            if (!list.empty())
            {
                list.pop_back();
                reference.pop_back();
            }
            break;
        }
        case 3: // pop_front
        {
            if (!list.empty())
            {
                list.pop_front();
                reference.pop_front();
            }
            break;
        }
        case 4: // remove random linked node
        {
            size_t idx = indexDist(rng);
            if (pool[idx].isLinked())
            {
                list.remove(pool[idx]);
                reference.remove(&pool[idx]);
            }
            break;
        }
        case 5: // verify iteration matches
        {
            auto it = list.begin();
            auto refIt = reference.begin();
            while (it != list.end() && refIt != reference.end())
            {
                FATP_ASSERT_EQ(&(*it), *refIt, "Iteration mismatch at step");
                ++it;
                ++refIt;
            }
            FATP_ASSERT_TRUE(it == list.end() && refIt == reference.end(),
                             "Iteration length mismatch");
            break;
        }
        }

        // Invariant: sizes must match
        FATP_ASSERT_EQ(list.size(), reference.size(), "Size mismatch after operation");
    }

    // Final verification: full iteration
    {
        auto it = list.begin();
        auto refIt = reference.begin();
        while (it != list.end())
        {
            FATP_ASSERT_EQ(&(*it), *refIt, "Final iteration mismatch");
            ++it;
            ++refIt;
        }
    }

    // Cleanup
    list.clear();

    return true;
}

FATP_TEST_CASE(stress_splice_multiple_lists)
{
    // Stress test: random splice operations between multiple lists
    constexpr size_t kPoolSize = 100;
    constexpr size_t kNumLists = 4;
    constexpr size_t kIterations = 2000;
    constexpr unsigned kSeed = 12345;

    std::vector<TestNode> pool(kPoolSize);
    for (size_t i = 0; i < kPoolSize; ++i)
    {
        pool[i].value = static_cast<int>(i);
    }

    std::vector<fat_p::IntrusiveList<TestNode>> lists(kNumLists);

    // Initial distribution: spread nodes across lists
    for (size_t i = 0; i < kPoolSize; ++i)
    {
        lists[i % kNumLists].push_back(pool[i]);
    }

    std::mt19937 rng(kSeed);
    std::uniform_int_distribution<size_t> listDist(0, kNumLists - 1);
    std::uniform_int_distribution<int> opDist(0, 3);

    size_t totalNodes = kPoolSize;

    for (size_t iter = 0; iter < kIterations; ++iter)
    {
        int op = opDist(rng);
        size_t srcIdx = listDist(rng);
        size_t dstIdx = listDist(rng);

        switch (op)
        {
        case 0: // splice all from src to dst (at end)
        {
            if (srcIdx != dstIdx && !lists[srcIdx].empty())
            {
                lists[dstIdx].splice(lists[dstIdx].end(), lists[srcIdx]);
            }
            break;
        }
        case 1: // splice all from src to dst (at begin)
        {
            if (srcIdx != dstIdx && !lists[srcIdx].empty())
            {
                lists[dstIdx].splice(lists[dstIdx].begin(), lists[srcIdx]);
            }
            break;
        }
        case 2: // move one node between lists via remove + push
        {
            if (!lists[srcIdx].empty())
            {
                TestNode& node = lists[srcIdx].front();
                lists[srcIdx].remove(node);
                lists[dstIdx].push_back(node);
            }
            break;
        }
        case 3: // verify total count
        {
            size_t count = 0;
            for (const auto& list : lists)
            {
                count += list.size();
            }
            FATP_ASSERT_EQ(count, totalNodes, "Total node count invariant violated");
            break;
        }
        }

        // Invariant: sum of all list sizes == totalNodes
        size_t checkSum = 0;
        for (const auto& list : lists)
        {
            checkSum += list.size();
        }
        FATP_ASSERT_EQ(checkSum, totalNodes, "Node count invariant violated");
    }

    // Cleanup
    for (auto& list : lists)
    {
        list.clear();
    }

    return true;
}

FATP_TEST_CASE(stress_safe_policy_ownership)
{
    // Stress test SafeOwnerPolicy: verify ownership tracking under random operations
    constexpr size_t kPoolSize = 50;
    constexpr size_t kNumLists = 3;
    constexpr size_t kIterations = 1000;
    constexpr unsigned kSeed = 98765;

    std::vector<TestNodeWithOwner> pool(kPoolSize);
    for (size_t i = 0; i < kPoolSize; ++i)
    {
        pool[i].value = static_cast<int>(i);
    }

    std::vector<fat_p::IntrusiveListSafe<TestNodeWithOwner>> lists(kNumLists);

    std::mt19937 rng(kSeed);
    std::uniform_int_distribution<size_t> nodeDist(0, kPoolSize - 1);
    std::uniform_int_distribution<size_t> listDist(0, kNumLists - 1);
    std::uniform_int_distribution<int> opDist(0, 4);

    for (size_t iter = 0; iter < kIterations; ++iter)
    {
        int op = opDist(rng);
        size_t nodeIdx = nodeDist(rng);
        size_t listIdx = listDist(rng);

        switch (op)
        {
        case 0: // push to random list (if unlinked)
        {
            if (!pool[nodeIdx].isLinked())
            {
                lists[listIdx].push_back(pool[nodeIdx]);
            }
            break;
        }
        case 1: // remove from correct list
        {
            if (pool[nodeIdx].isLinked())
            {
                // Find the owning list
                for (auto& list : lists)
                {
                    if (list.iteratorTo(pool[nodeIdx]) != list.end())
                    {
                        list.remove(pool[nodeIdx]);
                        break;
                    }
                }
            }
            break;
        }
        case 2: // try remove from wrong list (should be safe no-op)
        {
            if (pool[nodeIdx].isLinked())
            {
                // Find which list owns the node
                size_t owningList = kNumLists;  // Invalid index
                for (size_t j = 0; j < kNumLists; ++j)
                {
                    if (lists[j].iteratorTo(pool[nodeIdx]) != lists[j].end())
                    {
                        owningList = j;
                        break;
                    }
                }

                // Try to remove from a different list - should be no-op
                if (owningList < kNumLists)
                {
                    size_t wrongList = (owningList + 1) % kNumLists;
                    size_t sizeBefore = lists[wrongList].size();
                    lists[wrongList].remove(pool[nodeIdx]);
                    FATP_ASSERT_EQ(lists[wrongList].size(), sizeBefore,
                                   "Wrong-list remove should be no-op");
                }
            }
            break;
        }
        case 3: // verify iteratorTo consistency
        {
            int foundCount = 0;
            for (auto& list : lists)
            {
                if (list.iteratorTo(pool[nodeIdx]) != list.end())
                {
                    ++foundCount;
                }
            }
            if (pool[nodeIdx].isLinked())
            {
                FATP_ASSERT_EQ(foundCount, 1, "Linked node should be found in exactly one list");
            }
            else
            {
                FATP_ASSERT_EQ(foundCount, 0, "Unlinked node should not be found in any list");
            }
            break;
        }
        case 4: // splice between lists
        {
            size_t dstIdx = (listIdx + 1) % kNumLists;
            if (!lists[listIdx].empty())
            {
                lists[dstIdx].splice(lists[dstIdx].end(), lists[listIdx]);
            }
            break;
        }
        }
    }

    // Final verification: each linked node is in exactly one list
    for (size_t i = 0; i < kPoolSize; ++i)
    {
        int foundCount = 0;
        for (auto& list : lists)
        {
            if (list.iteratorTo(pool[i]) != list.end())
            {
                ++foundCount;
            }
        }
        if (pool[i].isLinked())
        {
            FATP_ASSERT_EQ(foundCount, 1, "Final: linked node in exactly one list");
        }
        else
        {
            FATP_ASSERT_EQ(foundCount, 0, "Final: unlinked node in no list");
        }
    }

    // Cleanup
    for (auto& list : lists)
    {
        list.clear();
    }

    return true;
}

FATP_TEST_CASE(stress_insert_erase_iteration)
{
    // Stress test: insert/erase with iterator validation
    constexpr size_t kPoolSize = 100;
    constexpr size_t kIterations = 2000;
    constexpr unsigned kSeed = 55555;

    std::vector<TestNode> pool(kPoolSize);
    for (size_t i = 0; i < kPoolSize; ++i)
    {
        pool[i].value = static_cast<int>(i);
    }

    fat_p::IntrusiveList<TestNode> list;
    std::vector<TestNode*> activeNodes;  // Track which nodes are in the list

    std::mt19937 rng(kSeed);
    std::uniform_int_distribution<size_t> poolDist(0, kPoolSize - 1);
    std::uniform_int_distribution<int> opDist(0, 3);

    for (size_t iter = 0; iter < kIterations; ++iter)
    {
        int op = opDist(rng);

        switch (op)
        {
        case 0: // insert at random position
        {
            size_t idx = poolDist(rng);
            if (!pool[idx].isLinked() && !list.empty())
            {
                std::uniform_int_distribution<size_t> posDist(0, activeNodes.size());
                size_t pos = posDist(rng);

                auto it = list.begin();
                std::advance(it, static_cast<std::ptrdiff_t>(pos > activeNodes.size() ? activeNodes.size() : pos));

                list.insert(it, pool[idx]);
                auto insertPos = static_cast<std::ptrdiff_t>(
                    pos > activeNodes.size() ? activeNodes.size() : pos);
                activeNodes.insert(activeNodes.begin() + insertPos, &pool[idx]);
            }
            else if (!pool[idx].isLinked())
            {
                list.push_back(pool[idx]);
                activeNodes.push_back(&pool[idx]);
            }
            break;
        }
        case 1: // erase at random position
        {
            if (!list.empty())
            {
                std::uniform_int_distribution<size_t> posDist(0, activeNodes.size() - 1);
                size_t pos = posDist(rng);

                auto it = list.begin();
                std::advance(it, static_cast<std::ptrdiff_t>(pos));

                list.erase(it);
                activeNodes.erase(activeNodes.begin() + static_cast<std::ptrdiff_t>(pos));
            }
            break;
        }
        case 2: // push_back
        {
            size_t idx = poolDist(rng);
            if (!pool[idx].isLinked())
            {
                list.push_back(pool[idx]);
                activeNodes.push_back(&pool[idx]);
            }
            break;
        }
        case 3: // verify order matches
        {
            FATP_ASSERT_EQ(list.size(), activeNodes.size(), "Size mismatch");
            size_t i = 0;
            for (auto& node : list)
            {
                FATP_ASSERT_EQ(&node, activeNodes[i], "Order mismatch");
                ++i;
            }
            break;
        }
        }

        FATP_ASSERT_EQ(list.size(), activeNodes.size(), "Size invariant");
    }

    list.clear();
    return true;
}
} // namespace fat_p::testing::intrusivelist

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_IntrusiveList()
{
    FATP_PRINT_HEADER(INTRUSIVE LIST)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_empty);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_push_front);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_push_back);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_iteration);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_pop_front);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_pop_back);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_remove);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_insert);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_erase);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_clear);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_splice);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_move);

    // New tests for bug fixes
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_single_element);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_cross_iterator_comparison);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_max_size);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_type_aliases);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_iterator_conversion);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_reverse_iteration);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_decrement_end);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_is_linked);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_iterator_to_fast);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_iterator_to_safe);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_remove_unlinked);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_cross_list_remove);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_splice_at_begin);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_move_ownership);

    // SafeOwnerPolicy owner-update tests (P2-9)
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_safe_owner_move_construction);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_safe_owner_move_assignment);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_safe_owner_splice);

    // Additional coverage tests
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_splice_empty_source);
    FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_move_assignment_to_nonempty);

    // Corner case / Edge case tests
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_empty_list_operations);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_two_element_list);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_insert_at_begin);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_erase_first_and_last);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_self_splice);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_move_from_empty);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_double_remove);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_splice_in_middle);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_const_iteration);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_front_back_same_element);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_iterator_equality);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_post_increment_decrement);
    FATP_RUN_TEST_NS(runner, intrusivelist, corner_safe_policy_unlinked_iteratorTo);

    // Swap tests
    FATP_RUN_TEST_NS(runner, intrusivelist, swap_basic);
    FATP_RUN_TEST_NS(runner, intrusivelist, swap_empty_with_nonempty);
    FATP_RUN_TEST_NS(runner, intrusivelist, swap_both_empty);
    FATP_RUN_TEST_NS(runner, intrusivelist, swap_self);
    FATP_RUN_TEST_NS(runner, intrusivelist, swap_safe_policy);
    FATP_RUN_TEST_NS(runner, intrusivelist, swap_adl);

    // RAII correctness tests
    FATP_RUN_TEST_NS(runner, intrusivelist, raii_lifecycle_clear);
    FATP_RUN_TEST_NS(runner, intrusivelist, raii_lifecycle_list_destruction);
    FATP_RUN_TEST_NS(runner, intrusivelist, raii_no_double_unlink);

    // Move-only type tests
    FATP_RUN_TEST_NS(runner, intrusivelist, move_only_basic_operations);
    FATP_RUN_TEST_NS(runner, intrusivelist, move_only_splice);
    FATP_RUN_TEST_NS(runner, intrusivelist, move_only_list_move);

    // Exception safety tests
    FATP_RUN_TEST_NS(runner, intrusivelist, exception_safe_iteration);
    FATP_RUN_TEST_NS(runner, intrusivelist, exception_safe_partial_operation);

    // Fuzz / Stress tests
    FATP_RUN_TEST_NS(runner, intrusivelist, stress_random_operations);
    FATP_RUN_TEST_NS(runner, intrusivelist, stress_splice_multiple_lists);
    FATP_RUN_TEST_NS(runner, intrusivelist, stress_safe_policy_ownership);
    FATP_RUN_TEST_NS(runner, intrusivelist, stress_insert_erase_iteration);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_IntrusiveList() ? 0 : 1;
}
#endif
