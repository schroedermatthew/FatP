/**
 * @file test_IntrusiveList.cpp
 * @brief Comprehensive unit tests for IntrusiveList.h
 */

#include <iostream>
#include <vector>
#include <string>

#include "IntrusiveList.h"
#include "FatPTest.h"

using namespace fat_p;

namespace fat_p::testing
{

// Test node type
struct TestNode : IntrusiveListNode<TestNode> {
    int value;
    std::string name;
    
    TestNode(int v = 0, std::string n = "") : value(v), name(std::move(n)) {}
};

bool test_intrusive_list_empty() {
    IntrusiveList<TestNode> list;
    
    ASSERT_TRUE(list.empty(), "List should be empty");
    ASSERT_TRUE(list.size() == 0, "List should have size 0");
    
    return true;
}

bool test_intrusive_list_push_front() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_front(n1);
    ASSERT_TRUE(list.size() == 1, "Size should be 1");
    ASSERT_TRUE(list.front().value == 1, "Front should be n1");
    
    list.push_front(n2);
    ASSERT_TRUE(list.size() == 2, "Size should be 2");
    ASSERT_TRUE(list.front().value == 2, "Front should be n2");
    
    list.push_front(n3);
    ASSERT_TRUE(list.size() == 3, "Size should be 3");
    ASSERT_TRUE(list.front().value == 3, "Front should be n3");
    
    return true;
}

bool test_intrusive_list_push_back() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n1);
    ASSERT_TRUE(list.size() == 1, "Size should be 1");
    ASSERT_TRUE(list.back().value == 1, "Back should be n1");
    
    list.push_back(n2);
    ASSERT_TRUE(list.size() == 2, "Size should be 2");
    ASSERT_TRUE(list.back().value == 2, "Back should be n2");
    
    list.push_back(n3);
    ASSERT_TRUE(list.size() == 3, "Size should be 3");
    ASSERT_TRUE(list.back().value == 3, "Back should be n3");
    
    return true;
}

bool test_intrusive_list_iteration() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n1);
    list.push_back(n2);
    list.push_back(n3);
    
    std::vector<int> values;
    for (const auto& node : list) {
        values.push_back(node.value);
    }
    
    ASSERT_TRUE(values.size() == 3, "Should iterate 3 times");
    ASSERT_TRUE(values[0] == 1, "First value should be 1");
    ASSERT_TRUE(values[1] == 2, "Second value should be 2");
    ASSERT_TRUE(values[2] == 3, "Third value should be 3");
    
    return true;
}

bool test_intrusive_list_pop_front() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n1);
    list.push_back(n2);
    list.push_back(n3);
    
    list.pop_front();
    ASSERT_TRUE(list.size() == 2, "Size should be 2");
    ASSERT_TRUE(list.front().value == 2, "Front should be n2");
    
    list.pop_front();
    ASSERT_TRUE(list.size() == 1, "Size should be 1");
    ASSERT_TRUE(list.front().value == 3, "Front should be n3");
    
    list.pop_front();
    ASSERT_TRUE(list.empty(), "List should be empty");
    
    return true;
}

bool test_intrusive_list_pop_back() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n1);
    list.push_back(n2);
    list.push_back(n3);
    
    list.pop_back();
    ASSERT_TRUE(list.size() == 2, "Size should be 2");
    ASSERT_TRUE(list.back().value == 2, "Back should be n2");
    
    list.pop_back();
    ASSERT_TRUE(list.size() == 1, "Size should be 1");
    ASSERT_TRUE(list.back().value == 1, "Back should be n1");
    
    list.pop_back();
    ASSERT_TRUE(list.empty(), "List should be empty");
    
    return true;
}

bool test_intrusive_list_remove() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n1);
    list.push_back(n2);
    list.push_back(n3);
    
    // Remove middle
    list.remove(n2);
    ASSERT_TRUE(list.size() == 2, "Size should be 2");
    
    std::vector<int> values;
    for (const auto& node : list) {
        values.push_back(node.value);
    }
    
    ASSERT_TRUE(values.size() == 2, "Should have 2 elements");
    ASSERT_TRUE(values[0] == 1, "First should be 1");
    ASSERT_TRUE(values[1] == 3, "Second should be 3");
    
    // Remove front
    list.remove(n1);
    ASSERT_TRUE(list.size() == 1, "Size should be 1");
    ASSERT_TRUE(list.front().value == 3, "Front should be 3");
    
    // Remove last
    list.remove(n3);
    ASSERT_TRUE(list.empty(), "List should be empty");
    
    return true;
}

bool test_intrusive_list_insert() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");
    
    list.push_back(n1);
    list.push_back(n3);
    
    // Insert in middle
    auto it = list.begin();
    ++it;
    list.insert(it, n2);
    
    ASSERT_TRUE(list.size() == 3, "Size should be 3");
    
    std::vector<int> values;
    for (const auto& node : list) {
        values.push_back(node.value);
    }
    
    ASSERT_TRUE(values[0] == 1, "First should be 1");
    ASSERT_TRUE(values[1] == 2, "Second should be 2");
    ASSERT_TRUE(values[2] == 3, "Third should be 3");
    
    // Insert at end
    list.insert(list.end(), n4);
    ASSERT_TRUE(list.size() == 4, "Size should be 4");
    ASSERT_TRUE(list.back().value == 4, "Back should be 4");
    
    return true;
}

bool test_intrusive_list_erase() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n1);
    list.push_back(n2);
    list.push_back(n3);
    
    auto it = list.begin();
    ++it;  // Point to n2
    
    auto next = list.erase(it);
    
    ASSERT_TRUE(list.size() == 2, "Size should be 2");
    ASSERT_TRUE(next->value == 3, "Next should point to n3");
    
    return true;
}

bool test_intrusive_list_clear() {
    IntrusiveList<TestNode> list;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n1);
    list.push_back(n2);
    list.push_back(n3);
    
    ASSERT_TRUE(list.size() == 3, "Size should be 3");
    
    list.clear();
    
    ASSERT_TRUE(list.empty(), "List should be empty");
    ASSERT_TRUE(list.size() == 0, "Size should be 0");
    
    // Nodes should be unlinked
    ASSERT_TRUE(!n1.is_linked(), "n1 should not be linked");
    ASSERT_TRUE(!n2.is_linked(), "n2 should not be linked");
    ASSERT_TRUE(!n3.is_linked(), "n3 should not be linked");
    
    return true;
}

bool test_intrusive_list_splice() {
    IntrusiveList<TestNode> list1;
    IntrusiveList<TestNode> list2;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");
    
    list1.push_back(n1);
    list1.push_back(n2);
    
    list2.push_back(n3);
    list2.push_back(n4);
    
    // Splice list2 at end of list1
    list1.splice(list1.end(), list2);
    
    ASSERT_TRUE(list1.size() == 4, "list1 should have 4 elements");
    ASSERT_TRUE(list2.empty(), "list2 should be empty");
    
    std::vector<int> values;
    for (const auto& node : list1) {
        values.push_back(node.value);
    }
    
    ASSERT_TRUE(values[0] == 1, "First should be 1");
    ASSERT_TRUE(values[1] == 2, "Second should be 2");
    ASSERT_TRUE(values[2] == 3, "Third should be 3");
    ASSERT_TRUE(values[3] == 4, "Fourth should be 4");
    
    return true;
}

bool test_intrusive_list_move() {
    IntrusiveList<TestNode> list1;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    
    list1.push_back(n1);
    list1.push_back(n2);
    
    // Move constructor
    IntrusiveList<TestNode> list2(std::move(list1));
    
    ASSERT_TRUE(list1.empty(), "list1 should be empty after move");
    ASSERT_TRUE(list2.size() == 2, "list2 should have 2 elements");
    ASSERT_TRUE(list2.front().value == 1, "Front should be 1");
    
    // Move assignment
    IntrusiveList<TestNode> list3;
    list3 = std::move(list2);
    
    ASSERT_TRUE(list2.empty(), "list2 should be empty after move");
    ASSERT_TRUE(list3.size() == 2, "list3 should have 2 elements");
    
    return true;
}

void benchmark_intrusive_list() {
    std::cout << "\n" << colors::cyan() << "IntrusiveList Benchmarks:" << colors::reset() << "\n\n";
    
    // Benchmark push_back
    std::vector<TestNode> nodes(10000);
    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i].value = static_cast<int>(i);
    }
    
    IntrusiveList<TestNode> list;
    double push_time = measure_perf([&list, &nodes, i=0]() mutable {
        list.push_back(nodes[i % 10000]);
        ++i;
    }, 10000, 0);
    std::cout << "Push back: " << format_time(push_time) << "\n";
    
    // Benchmark iteration
    list.clear();
    for (size_t i = 0; i < 10000; ++i) {
        list.push_back(nodes[i]);
    }
    
    double iter_time = measure_perf([&list]() {
        int sum = 0;
        for (const auto& node : list) {
            sum += node.value;
        }
        DoNotOptimize(sum);
    }, 1000, 10);
    std::cout << "Iteration (10k elements): " << format_time(iter_time) << "\n";
    
    // Benchmark remove
    double remove_time = measure_perf([&list, &nodes, i=0]() mutable {
        if (i < 10000) {
            list.remove(nodes[i]);
        }
        ++i;
    }, 10000, 0);
    std::cout << "Remove: " << format_time(remove_time) << "\n";
    
    // Benchmark insert
    IntrusiveList<TestNode> list2;
    list2.push_back(nodes[0]);
    double insert_time = measure_perf([&list2, &nodes, i=1]() mutable {
        if (i < 10000) {
            list2.insert(list2.end(), nodes[i]);
        }
        ++i;
    }, 9999, 0);
    std::cout << "Insert at end: " << format_time(insert_time) << "\n";
}

bool test_IntrusiveList() {

    PRINT_HEADER(INTRUSIVE LIST)

    TestRunner runner;

    RUN_TEST(runner, intrusive_list_empty);
    RUN_TEST(runner, intrusive_list_push_front);
    RUN_TEST(runner, intrusive_list_push_back);
    RUN_TEST(runner, intrusive_list_iteration);
    RUN_TEST(runner, intrusive_list_pop_front);
    RUN_TEST(runner, intrusive_list_pop_back);
    RUN_TEST(runner, intrusive_list_remove);
    RUN_TEST(runner, intrusive_list_insert);
    RUN_TEST(runner, intrusive_list_erase);
    RUN_TEST(runner, intrusive_list_clear);
    RUN_TEST(runner, intrusive_list_splice);
    RUN_TEST(runner, intrusive_list_move);

    benchmark_intrusive_list();

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
