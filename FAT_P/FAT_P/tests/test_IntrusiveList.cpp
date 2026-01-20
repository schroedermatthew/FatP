/**
 * @file test_IntrusiveList.cpp
 * @brief Comprehensive unit tests for IntrusiveList.h
 */
/*
FATP_META:
  meta_version: 1
  component: IntrusiveList
  file_role: test
  path: tests/test_IntrusiveList.cpp
  namespace: fat_p::testing::intrusivelist
  summary: "Unit tests for IntrusiveList."
  related:
    docs_search: "IntrusiveList"
    headers:
      - fat_p/IntrusiveList.h
      - fat_p/FatPTest.h
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

#include <iostream>
#include <string>
#include <vector>

#include "FatPTest.h"
#include "IntrusiveList.h"

using namespace fat_p;

namespace fat_p::testing::intrusivelist
{

// Test node type (default policy: fast / no owner pointer)
struct TestNode : IntrusiveListNode<TestNode>
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
struct TestNodeWithOwner : IntrusiveListNode<TestNodeWithOwner, intrusive_list::SafeOwnerPolicy>
{
    int value;

    explicit TestNodeWithOwner(int v = 0)
        : value(v)
    {
    }
};

FATP_TEST_CASE(intrusive_list_empty)
{
    IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list1;
        IntrusiveList<TestNode> list2;

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
        IntrusiveList<TestNode> list1;

        list1.push_back(n1);
        list1.push_back(n2);

        // Move constructor
        IntrusiveList<TestNode> list2(std::move(list1));

        FATP_ASSERT_TRUE(list1.empty(), "list1 should be empty after move");
        FATP_ASSERT_TRUE(list2.size() == 2, "list2 should have 2 elements");
        FATP_ASSERT_TRUE(list2.front().value == 1, "Front should be 1");

        // Move assignment
        IntrusiveList<TestNode> list3;
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
        IntrusiveList<TestNode> list;

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

FATP_TEST_CASE(intrusive_list_iterator_conversion)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");

    {
        IntrusiveList<TestNode> list;

        list.push_back(n1);
        list.push_back(n2);

        // Get mutable iterator
        IntrusiveList<TestNode>::iterator it = list.begin();

        // Convert to const_iterator (this was broken before fix)
        IntrusiveList<TestNode>::const_iterator cit = it;

        FATP_ASSERT_EQ(cit->value, 1, "const_iterator should point to first element");

        ++cit;
        FATP_ASSERT_EQ(cit->value, 2, "const_iterator should advance");

        // Verify const correctness - this should compile
        const IntrusiveList<TestNode>& const_list = list;
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
        IntrusiveList<TestNode> list;

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

        const IntrusiveList<TestNode>& const_list = list;
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
        IntrusiveList<TestNode> list;
        list.push_back(n1);

        auto it = list.end();
        --it;
        FATP_ASSERT_EQ(it->value, 42, "--end() should point to last element");

        const IntrusiveList<TestNode>& const_list = list;
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
        IntrusiveList<TestNode> list;

        // Unlinked node: iteratorTo returns end().
        FATP_ASSERT_TRUE(list.iteratorTo(n3) == list.end(), "iteratorTo(unlinked) should be end()");

        list.push_back(n1);
        list.push_back(n2);

        auto it = list.iteratorTo(n2);
        FATP_ASSERT_TRUE(it != list.end(), "iteratorTo(linked) should not be end()");
        FATP_ASSERT_EQ(it->value, 2, "iteratorTo should point to the requested node");

        const IntrusiveList<TestNode>& const_list = list;
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
        IntrusiveListSafe<TestNodeWithOwner> listA;
        IntrusiveListSafe<TestNodeWithOwner> listB;

        listB.push_back(node);

        // Wrong-list: safe policy should return end().
        FATP_ASSERT_TRUE(listA.iteratorTo(node) == listA.end(), "Safe iteratorTo(wrong list) should be end()");

        auto it = listB.iteratorTo(node);
        FATP_ASSERT_TRUE(it != listB.end(), "Safe iteratorTo(correct list) should not be end()");
        FATP_ASSERT_EQ(it->value, 7, "Safe iteratorTo should point to the requested node");

        const IntrusiveListSafe<TestNodeWithOwner>& const_listB = listB;
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
        IntrusiveList<TestNode> list;

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
        IntrusiveList<TestNode> list;

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
        IntrusiveListSafe<TestNodeWithOwner> listA;
        IntrusiveListSafe<TestNodeWithOwner> listB;

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
        IntrusiveList<TestNode> list1;
        IntrusiveList<TestNode> list2;

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
        IntrusiveList<TestNode> list1;

        list1.push_back(n1);
        list1.push_back(n2);

        FATP_ASSERT_TRUE(n1.isLinked(), "n1 should be linked before move");
        FATP_ASSERT_TRUE(n2.isLinked(), "n2 should be linked before move");

        // Move to a new list
        IntrusiveList<TestNode> list2(std::move(list1));

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


void benchmark_intrusive_list()
{
    std::cout << "\n" << colors::cyan() << "IntrusiveList Benchmarks:" << colors::reset() << "\n\n";

    // Benchmark push_back
    std::vector<TestNode> nodes(10000);
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i].value = static_cast<int>(i);
    }

    IntrusiveList<TestNode> list;
    double push_time = measure_perf(
        [&list, &nodes, i = 0]() mutable {
            list.push_back(nodes[i % 10000]);
            ++i;
        },
        10000,
        0);
    std::cout << "Push back: " << format_time(push_time) << "\n";

    // Benchmark iteration
    list.clear();
    for (size_t i = 0; i < 10000; ++i)
    {
        list.push_back(nodes[i]);
    }

    double iter_time = measure_perf(
        [&list]() {
            int sum = 0;
            for (const auto& node : list)
            {
                sum += node.value;
            }
            DoNotOptimize(sum);
        },
        1000,
        10);
    std::cout << "Iteration (10k elements): " << format_time(iter_time) << "\n";

    // Benchmark remove
    double remove_time = measure_perf(
        [&list, &nodes, i = 0]() mutable {
            if (i < 10000)
            {
                list.remove(nodes[i]);
            }
            ++i;
        },
        10000,
        0);
    std::cout << "Remove: " << format_time(remove_time) << "\n";

    // Benchmark insert
    IntrusiveList<TestNode> list2;
    list2.push_back(nodes[0]);
    double insert_time = measure_perf(
        [&list2, &nodes, i = 1]() mutable {
            if (i < 10000)
            {
                list2.insert(list2.end(), nodes[i]);
            }
            ++i;
        },
        9999,
        0);
    std::cout << "Insert at end: " << format_time(insert_time) << "\n";
}

} // namespace fat_p::testing::intrusivelist

namespace fat_p::testing
{

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

    intrusivelist::benchmark_intrusive_list();

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
