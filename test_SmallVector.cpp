#include <iostream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <string>
#include <cstring>

#include "SmallVector.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_SmallVector.h"
#endif

namespace fat_p::testing
{

template<typename T>
class TrackingAllocator
{
public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    int id;
    inline static int next_id = 1;
    inline static size_t allocation_count = 0;
    inline static size_t deallocation_count = 0;

    TrackingAllocator() : id(next_id++) {}
    explicit TrackingAllocator(int i) : id(i) {}

    template<typename U>
    TrackingAllocator(const TrackingAllocator<U>& other) : id(other.id) {}

    T* allocate(size_t n)
    {
        ++allocation_count;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t)
    {
        ++deallocation_count;
        ::operator delete(p);
    }

    template<typename U>
    bool operator==(const TrackingAllocator<U>& other) const { return id == other.id; }

    template<typename U>
    bool operator!=(const TrackingAllocator<U>& other) const { return id != other.id; }

    static void reset_counts()
    {
        allocation_count = 0;
        deallocation_count = 0;
    }
};

struct ThrowOnCopy
{
    int value;
    static int operation_count;  // Tracks all potentially-throwing operations (copy AND move)
    static int throw_after;

    ThrowOnCopy(int v = 0) : value(v) {}

    ThrowOnCopy(const ThrowOnCopy& other) : value(other.value)
    {
        if (++operation_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Copy threw");
        }
    }

    ThrowOnCopy& operator=(const ThrowOnCopy& other)
    {
        if (++operation_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Assignment threw");
        }
        value = other.value;
        return *this;
    }

    ThrowOnCopy(ThrowOnCopy&& other) : value(other.value)
    {
        if (++operation_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Move threw");
        }
    }
    
    ThrowOnCopy& operator=(ThrowOnCopy&& other)
    {
        if (++operation_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Move assignment threw");
        }
        value = other.value;
        return *this;
    }

    bool operator==(const ThrowOnCopy& other) const { return value == other.value; }

    static void reset() { operation_count = 0; throw_after = -1; }
};

int ThrowOnCopy::operation_count = 0;
int ThrowOnCopy::throw_after = -1;

// Helper to check if a SmallVector is using inline storage
// NOTE: This is a QoI-style heuristic, not a formal proof of mode.
// It assumes inline_buffer_ lives within the object and that heap
// allocations won't land inside that address range.
template<typename T, size_t N, typename A>
bool is_using_inline_storage(const SmallVector<T, N, A>& v)
{
    const char* obj_start = reinterpret_cast<const char*>(&v);
    const char* obj_end = obj_start + sizeof(v);
    const char* data_ptr = reinterpret_cast<const char*>(v.data());
    return data_ptr >= obj_start && data_ptr < obj_end;
}

bool test_sv_basic_construction()
{
    SmallVector<int, 4> v1;
    SIMPLE_ASSERT(v1.size() == 0, "Default constructor creates empty vector");
    SIMPLE_ASSERT(v1.empty(), "Default vector is empty");
    SIMPLE_ASSERT(v1.capacity() == 4, "Default capacity equals InlineCapacity");

    SmallVector<int, 4> v2(5);
    SIMPLE_ASSERT(v2.size() == 5, "Count constructor creates vector with 5 elements");
    SIMPLE_ASSERT(v2.capacity() >= 5, "Capacity accommodates 5 elements");

    SmallVector<int, 4> v3(3, 42);
    SIMPLE_ASSERT(v3.size() == 3, "Count-value constructor creates 3 elements");
    SIMPLE_ASSERT(v3[0] == 42 && v3[1] == 42 && v3[2] == 42, "All elements equal 42");

    std::vector<int> src = {1, 2, 3, 4, 5};
    SmallVector<int, 4> v4(src.begin(), src.end());
    SIMPLE_ASSERT(v4.size() == 5, "Iterator constructor creates vector with 5 elements");
    SIMPLE_ASSERT(std::equal(v4.begin(), v4.end(), src.begin()), "Elements match source");

    SmallVector<int, 4> v5 = {10, 20, 30};
    SIMPLE_ASSERT(v5.size() == 3, "Initializer list creates 3 elements");
    SIMPLE_ASSERT(v5[0] == 10 && v5[1] == 20 && v5[2] == 30, "Elements match");

    return true;
}

bool test_sv_copy_construction()
{
    SmallVector<int, 4> v1 = {1, 2, 3};
    SmallVector<int, 4> v2(v1);

    SIMPLE_ASSERT(v2.size() == 3, "Copy has same size");
    SIMPLE_ASSERT(v2[0] == 1 && v2[1] == 2 && v2[2] == 3, "Copy has same values");
    SIMPLE_ASSERT(&v1[0] != &v2[0], "Copy is independent");

    SmallVector<int, 2> v3 = {1, 2, 3, 4, 5};
    SmallVector<int, 2> v4(v3);
    SIMPLE_ASSERT(v4.size() == 5, "Heap copy has same size");
    SIMPLE_ASSERT(std::equal(v3.begin(), v3.end(), v4.begin()), "Heap copy has same values");

    return true;
}

bool test_sv_move_construction()
{
    SmallVector<int, 4> v1 = {1, 2, 3};
    SmallVector<int, 4> v2(std::move(v1));

    SIMPLE_ASSERT(v2.size() == 3, "Move target has size 3");
    SIMPLE_ASSERT(v2[0] == 1 && v2[1] == 2 && v2[2] == 3, "Move target has correct values");
    SIMPLE_ASSERT(v1.size() == 0, "Move source is empty");

    SmallVector<int, 2> v3 = {1, 2, 3, 4, 5};
    int* old_data = v3.data();
    SmallVector<int, 2> v4(std::move(v3));

    SIMPLE_ASSERT(v4.size() == 5, "Heap move target has size 5");
    SIMPLE_ASSERT(v4.data() == old_data, "Heap move steals pointer");
    SIMPLE_ASSERT(v3.size() == 0, "Heap move source is empty");

    return true;
}

bool test_sv_copy_assignment()
{
    SmallVector<int, 4> v1 = {1, 2, 3};
    SmallVector<int, 4> v2;
    v2 = v1;

    SIMPLE_ASSERT(v2.size() == 3, "Copy assignment sets size");
    SIMPLE_ASSERT(v2[0] == 1 && v2[1] == 2 && v2[2] == 3, "Copy assignment copies values");

    SmallVector<int, 4> v3 = {10, 20};
    SmallVector<int, 4> v4 = {1, 2, 3, 4, 5};
    v4 = v3;
    SIMPLE_ASSERT(v4.size() == 2, "Copy assignment from smaller vector");
    SIMPLE_ASSERT(v4[0] == 10 && v4[1] == 20, "Values correct after copy assignment");

    SmallVector<int, 4> v5 = {1, 2, 3};
    v5 = v5;
    SIMPLE_ASSERT(v5.size() == 3, "Self-assignment works");
    SIMPLE_ASSERT(v5[0] == 1 && v5[1] == 2 && v5[2] == 3, "Self-assignment preserves values");

    return true;
}

bool test_sv_move_assignment()
{
    SmallVector<int, 4> v1 = {1, 2, 3};
    SmallVector<int, 4> v2;
    v2 = std::move(v1);

    SIMPLE_ASSERT(v2.size() == 3, "Move assignment sets size");
    SIMPLE_ASSERT(v2[0] == 1 && v2[1] == 2 && v2[2] == 3, "Move assignment transfers values");
    SIMPLE_ASSERT(v1.size() == 0, "Move assignment empties source");

    SmallVector<int, 2> v3 = {1, 2, 3, 4, 5};
    int* old_data = v3.data();
    SmallVector<int, 2> v4;
    v4 = std::move(v3);

    SIMPLE_ASSERT(v4.data() == old_data, "Heap move assignment steals pointer");
    SIMPLE_ASSERT(v3.size() == 0, "Heap move assignment empties source");

    SmallVector<int, 4> v5 = {1, 2, 3};
    auto& v5_ref = v5;  // Indirect reference to avoid -Wself-move
    v5 = std::move(v5_ref);
    SIMPLE_ASSERT(v5.size() == 3, "Self-move assignment works");

    return true;
}

bool test_sv_element_access()
{
    SmallVector<int, 4> v = {10, 20, 30, 40};

    SIMPLE_ASSERT(v[0] == 10, "operator[] reads first element");
    SIMPLE_ASSERT(v[3] == 40, "operator[] reads last element");

    v[1] = 25;
    SIMPLE_ASSERT(v[1] == 25, "operator[] writes element");

    SIMPLE_ASSERT(v.at(0) == 10, "at() reads first element");
    SIMPLE_ASSERT(v.at(3) == 40, "at() reads last element");

    bool threw = false;
    try
    {
        (void)v.at(10);
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    SIMPLE_ASSERT(threw, "at() throws on out of bounds");

    SIMPLE_ASSERT(v.front() == 10, "front() returns first element");
    SIMPLE_ASSERT(v.back() == 40, "back() returns last element");

    v.front() = 15;
    SIMPLE_ASSERT(v[0] == 15, "front() is writable");

    SIMPLE_ASSERT(v.data() == &v[0], "data() returns pointer to first element");

    return true;
}

bool test_sv_iterators()
{
    SmallVector<int, 4> v = {1, 2, 3, 4};

    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it)
    {
        sum += *it;
    }
    SIMPLE_ASSERT(sum == 10, "Forward iteration works");

    sum = 0;
    for (auto it = v.rbegin(); it != v.rend(); ++it)
    {
        sum += *it;
    }
    SIMPLE_ASSERT(sum == 10, "Reverse iteration works");

    const SmallVector<int, 4>& cv = v;
    SIMPLE_ASSERT(cv.begin() != nullptr, "Const begin works");
    SIMPLE_ASSERT(cv.cbegin() != nullptr, "cbegin works");

    sum = 0;
    for (int x : v)
    {
        sum += x;
    }
    SIMPLE_ASSERT(sum == 10, "Range-based for works");

    return true;
}

bool test_sv_capacity_operations()
{
    SmallVector<int, 4> v;

    SIMPLE_ASSERT(v.capacity() == 4, "Initial capacity is InlineCapacity");
    SIMPLE_ASSERT(v.empty(), "Empty vector is empty");

    v.reserve(10);
    SIMPLE_ASSERT(v.capacity() >= 10, "Reserve increases capacity");
    SIMPLE_ASSERT(v.size() == 0, "Reserve doesn't change size");

    v = {1, 2};
    SIMPLE_ASSERT(v.size() == 2, "Size is 2");

    v.resize(5);
    SIMPLE_ASSERT(v.size() == 5, "Resize increases size");
    SIMPLE_ASSERT(v[0] == 1 && v[1] == 2, "Existing elements preserved");

    v.resize(3);
    SIMPLE_ASSERT(v.size() == 3, "Resize decreases size");

    v.resize(4, 99);
    SIMPLE_ASSERT(v[3] == 99, "Resize with value works");

    v.clear();
    SIMPLE_ASSERT(v.size() == 0, "Clear empties vector");
    SIMPLE_ASSERT(v.capacity() >= 4, "Clear preserves capacity");

    return true;
}

bool test_sv_push_back_emplace_back()
{
    SmallVector<int, 4> v;

    v.push_back(1);
    SIMPLE_ASSERT(v.size() == 1 && v[0] == 1, "push_back works");

    v.push_back(2);
    v.push_back(3);
    SIMPLE_ASSERT(v.size() == 3, "Multiple push_back works");

    int x = 4;
    v.push_back(x);
    SIMPLE_ASSERT(v.size() == 4 && v[3] == 4, "push_back lvalue works");

    v.push_back(5);
    SIMPLE_ASSERT(v.size() == 5, "push_back triggers growth");
    SIMPLE_ASSERT(v.capacity() > 4, "Capacity increased after growth");

    SmallVector<std::string, 2> sv;
    sv.emplace_back("hello");
    SIMPLE_ASSERT(sv.size() == 1 && sv[0] == "hello", "emplace_back constructs in place");

    sv.emplace_back(5, 'x');
    SIMPLE_ASSERT(sv.size() == 2 && sv[1] == "xxxxx", "emplace_back forwards arguments");

    return true;
}

bool test_sv_pop_back()
{
    SmallVector<int, 4> v = {1, 2, 3, 4};

    v.pop_back();
    SIMPLE_ASSERT(v.size() == 3, "pop_back decreases size");
    SIMPLE_ASSERT(v.back() == 3, "Last element is now 3");

    v.pop_back();
    v.pop_back();
    v.pop_back();
    SIMPLE_ASSERT(v.empty(), "pop_back until empty");

    return true;
}

bool test_sv_insert()
{
    SmallVector<int, 4> v = {1, 2, 5};

    auto it = v.insert(v.begin() + 2, 3);
    SIMPLE_ASSERT(v.size() == 4, "Insert increases size");
    SIMPLE_ASSERT(*it == 3, "Insert returns iterator to new element");
    SIMPLE_ASSERT(v[2] == 3, "Element inserted at correct position");

    v = {1, 2};
    it = v.insert(v.end(), 3);
    SIMPLE_ASSERT(v.size() == 3 && v[2] == 3, "Insert at end works");

    v = {1, 4};
    it = v.insert(v.begin() + 1, 2, 99);
    SIMPLE_ASSERT(v.size() == 4, "Insert count increases size");
    SIMPLE_ASSERT(v[1] == 99 && v[2] == 99, "Multiple inserts work");

    v = {1, 5};
    std::vector<int> to_insert = {2, 3, 4};
    it = v.insert(v.begin() + 1, to_insert.begin(), to_insert.end());
    SIMPLE_ASSERT(v.size() == 5, "Range insert works");
    SIMPLE_ASSERT(v[1] == 2 && v[2] == 3 && v[3] == 4, "Range insert values correct");

    v = {1, 4};
    it = v.insert(v.begin() + 1, {2, 3});
    SIMPLE_ASSERT(v.size() == 4, "Initializer list insert works");
    SIMPLE_ASSERT(v[1] == 2 && v[2] == 3, "Initializer list values correct");

    return true;
}

bool test_sv_emplace()
{
    SmallVector<std::string, 4> v = {"a", "c"};

    auto it = v.emplace(v.begin() + 1, "b");
    SIMPLE_ASSERT(v.size() == 3, "Emplace increases size");
    SIMPLE_ASSERT(*it == "b", "Emplace returns iterator");
    SIMPLE_ASSERT(v[1] == "b", "Emplace constructs at position");

    v.emplace(v.begin(), 3, 'x');
    SIMPLE_ASSERT(v[0] == "xxx", "Emplace forwards constructor arguments");

    return true;
}

bool test_sv_erase()
{
    SmallVector<int, 4> v = {1, 2, 3, 4, 5};

    auto it = v.erase(v.begin() + 2);
    SIMPLE_ASSERT(v.size() == 4, "Erase decreases size");
    SIMPLE_ASSERT(*it == 4, "Erase returns iterator to next element");
    SIMPLE_ASSERT(v[2] == 4, "Elements shifted after erase");

    v = {1, 2, 3, 4, 5};
    it = v.erase(v.begin() + 1, v.begin() + 4);
    SIMPLE_ASSERT(v.size() == 2, "Range erase works");
    SIMPLE_ASSERT(v[0] == 1 && v[1] == 5, "Correct elements remain after range erase");

    v = {1, 2, 3};
    it = v.erase(v.begin() + 2);
    SIMPLE_ASSERT(v.size() == 2 && it == v.end(), "Erase last element");

    return true;
}

bool test_sv_swap()
{
    SmallVector<int, 4> v1 = {1, 2, 3};
    SmallVector<int, 4> v2 = {4, 5};

    v1.swap(v2);

    SIMPLE_ASSERT(v1.size() == 2, "Swap exchanges sizes");
    SIMPLE_ASSERT(v1[0] == 4 && v1[1] == 5, "Swap exchanges elements v1");
    SIMPLE_ASSERT(v2.size() == 3, "Swap exchanges sizes v2");
    SIMPLE_ASSERT(v2[0] == 1 && v2[1] == 2 && v2[2] == 3, "Swap exchanges elements v2");

    SmallVector<int, 2> v3 = {1, 2, 3, 4, 5};
    SmallVector<int, 2> v4 = {6, 7};

    v3.swap(v4);
    SIMPLE_ASSERT(v3.size() == 2 && v4.size() == 5, "Swap heap and inline");

    return true;
}

bool test_sv_comparison_operators()
{
    SmallVector<int, 4> v1 = {1, 2, 3};
    SmallVector<int, 4> v2 = {1, 2, 3};
    SmallVector<int, 4> v3 = {1, 2, 4};
    SmallVector<int, 4> v4 = {1, 2};

    SIMPLE_ASSERT(v1 == v2, "Equal vectors compare equal");
    SIMPLE_ASSERT(v1 != v3, "Different vectors not equal");
    SIMPLE_ASSERT(v1 < v3, "Lexicographical less than");
    SIMPLE_ASSERT(v4 < v1, "Shorter vector less than longer");
    SIMPLE_ASSERT(v1 <= v2, "Less or equal with equal");
    SIMPLE_ASSERT(v3 > v1, "Greater than");
    SIMPLE_ASSERT(v1 >= v2, "Greater or equal with equal");

    return true;
}

bool test_sv_inline_to_heap_transition()
{
    SmallVector<int, 4> v;

    for (int i = 0; i < 4; ++i)
    {
        v.push_back(i);
    }
    SIMPLE_ASSERT(v.capacity() == 4, "Still inline at capacity");

    v.push_back(4);
    SIMPLE_ASSERT(v.capacity() > 4, "Transitioned to heap");
    SIMPLE_ASSERT(v.size() == 5, "Size correct after transition");

    for (int i = 0; i < 5; ++i)
    {
        SIMPLE_ASSERT(v[i] == i, "Values preserved after transition");
    }

    return true;
}

bool test_sv_shrink_to_fit()
{
    SmallVector<int, 4> v;
    for (int i = 0; i < 10; ++i)
    {
        v.push_back(i);
    }

    SIMPLE_ASSERT(v.capacity() > 4, "Grown to heap");

    v.resize(3);
    v.shrink_to_fit();

    SIMPLE_ASSERT(v.capacity() == 4, "Shrunk back to inline");
    SIMPLE_ASSERT(v.size() == 3, "Size preserved");
    SIMPLE_ASSERT(v[0] == 0 && v[1] == 1 && v[2] == 2, "Values preserved");

    return true;
}

bool test_sv_allocator_propagation_pocma()
{
    using Vec = SmallVector<int, 4, TrackingAllocator<int>>;

    TrackingAllocator<int>::reset_counts();

    TrackingAllocator<int> alloc1(1);
    TrackingAllocator<int> alloc2(2);

    Vec v1(alloc1);
    v1 = {1, 2, 3, 4, 5};

    Vec v2(alloc2);
    v2 = {10, 20};

    v2 = std::move(v1);

    SIMPLE_ASSERT(v2.get_allocator().id == 1, "POCMA propagates allocator");
    SIMPLE_ASSERT(v2.size() == 5, "Move assignment transfers elements");

    return true;
}

bool test_sv_allocator_propagation_pocca()
{
    using Vec = SmallVector<int, 4, TrackingAllocator<int>>;

    TrackingAllocator<int> alloc1(1);
    TrackingAllocator<int> alloc2(2);

    Vec v1(alloc1);
    v1 = {1, 2, 3};

    Vec v2(alloc2);
    v2 = {10, 20};

    v2 = v1;

    SIMPLE_ASSERT(v2.get_allocator().id == 1, "POCCA propagates allocator");
    SIMPLE_ASSERT(v2.size() == 3, "Copy assignment copies elements");

    return true;
}

bool test_sv_allocator_propagation_pocs()
{
    using Vec = SmallVector<int, 4, TrackingAllocator<int>>;

    TrackingAllocator<int> alloc1(1);
    TrackingAllocator<int> alloc2(2);

    Vec v1(alloc1);
    v1 = {1, 2, 3};

    Vec v2(alloc2);
    v2 = {10, 20};

    v1.swap(v2);

    SIMPLE_ASSERT(v1.get_allocator().id == 2, "POCS swaps allocators");
    SIMPLE_ASSERT(v2.get_allocator().id == 1, "POCS swaps allocators");
    SIMPLE_ASSERT(v1.size() == 2, "Swap exchanges elements");

    return true;
}

bool test_sv_exception_safety_grow()
{
    SmallVector<ThrowOnCopy, 4> v;

    for (int i = 0; i < 4; ++i)
    {
        v.emplace_back(i);
    }

    ThrowOnCopy::reset();
    ThrowOnCopy::throw_after = 2;

    bool threw = false;
    try
    {
        v.emplace_back(4);
    }
    catch (const std::exception&)
    {
        threw = true;
    }

    SIMPLE_ASSERT(threw, "Exception thrown during grow");
    SIMPLE_ASSERT(v.size() == 4, "Size unchanged after exception");
    SIMPLE_ASSERT(v[0].value == 0, "Original elements intact");

    ThrowOnCopy::reset();

    return true;
}

bool test_sv_exception_safety_insert()
{
    SmallVector<ThrowOnCopy, 4> v;
    v.emplace_back(1);
    v.emplace_back(3);

    ThrowOnCopy::reset();
    ThrowOnCopy::throw_after = 3;

    bool threw = false;
    try
    {
        v.insert(v.begin() + 1, 3, ThrowOnCopy(2));
    }
    catch (const std::exception&)
    {
        threw = true;
    }

    SIMPLE_ASSERT(threw, "Exception thrown during insert");
    SIMPLE_ASSERT(v.size() == 2, "Size unchanged after insert exception");

    ThrowOnCopy::reset();

    return true;
}

bool test_sv_move_only_types()
{
    SmallVector<std::unique_ptr<int>, 4> v;

    v.push_back(std::make_unique<int>(42));
    SIMPLE_ASSERT(*v[0] == 42, "Move-only type stored");

    v.emplace_back(std::make_unique<int>(99));
    SIMPLE_ASSERT(*v[1] == 99, "Move-only emplace works");

    auto v2 = std::move(v);
    SIMPLE_ASSERT(*v2[0] == 42, "Move-only vector moved");

    return true;
}

bool test_sv_large_objects()
{
    struct Large
    {
        int data[100];
        Large()
        {
            for (int i = 0; i < 100; ++i)
            {
                data[i] = i;
            }
        }
    };

    SmallVector<Large, 2> v;
    v.emplace_back();
    v.emplace_back();
    v.emplace_back();

    SIMPLE_ASSERT(v.size() == 3, "Large objects stored");
    SIMPLE_ASSERT(v[0].data[50] == 50, "Large object data intact");

    return true;
}

bool test_sv_non_trivial_types()
{
    SmallVector<std::string, 4> v;

    v.push_back("hello");
    v.push_back("world");
    v.emplace_back(10, 'x');

    SIMPLE_ASSERT(v[0] == "hello", "String stored");
    SIMPLE_ASSERT(v[2] == "xxxxxxxxxx", "String constructed in place");

    v.erase(v.begin() + 1);
    SIMPLE_ASSERT(v.size() == 2, "String erased");

    return true;
}

bool test_sv_edge_case_empty_operations()
{
    SmallVector<int, 4> v;

    SIMPLE_ASSERT(v.begin() == v.end(), "Empty iterators equal");

    v.clear();
    SIMPLE_ASSERT(v.empty(), "Clear on empty works");

    auto it = v.erase(v.begin(), v.begin());
    SIMPLE_ASSERT(it == v.begin(), "Empty range erase works");

    return true;
}

bool test_sv_edge_case_single_element()
{
    SmallVector<int, 4> v;
    v.push_back(42);

    SIMPLE_ASSERT(v.front() == v.back(), "Single element front equals back");
    SIMPLE_ASSERT(v.size() == 1, "Size is 1");

    v.pop_back();
    SIMPLE_ASSERT(v.empty(), "Pop single element empties");

    return true;
}

bool test_sv_edge_case_exact_inline_capacity()
{
    SmallVector<int, 4> v;
    for (int i = 0; i < 4; ++i)
    {
        v.push_back(i);
    }

    SIMPLE_ASSERT(v.size() == 4, "Exactly at inline capacity");
    SIMPLE_ASSERT(v.capacity() == 4, "Capacity equals inline");

    v.resize(3);
    SIMPLE_ASSERT(v.size() == 3, "Resize below capacity");

    v.push_back(99);
    SIMPLE_ASSERT(v.size() == 4, "Push back to capacity");

    return true;
}

bool test_sv_assign_operations()
{
    SmallVector<int, 4> v;

    v.assign(5, 42);
    SIMPLE_ASSERT(v.size() == 5, "Assign count-value works");
    SIMPLE_ASSERT(v[0] == 42 && v[4] == 42, "All values assigned");

    std::vector<int> src = {1, 2, 3};
    v.assign(src.begin(), src.end());
    SIMPLE_ASSERT(v.size() == 3, "Assign range works");
    SIMPLE_ASSERT(v[0] == 1 && v[2] == 3, "Range values assigned");

    v.assign({10, 20});
    SIMPLE_ASSERT(v.size() == 2, "Assign initializer list works");
    SIMPLE_ASSERT(v[0] == 10 && v[1] == 20, "Initializer list values assigned");

    return true;
}

bool test_sv_max_size()
{
    SmallVector<int, 4> v;
    SIMPLE_ASSERT(v.max_size() > 0, "max_size returns positive value");
    SIMPLE_ASSERT(v.max_size() >= v.capacity(), "max_size >= capacity");

    return true;
}

bool test_sv_data_pointer()
{
    SmallVector<int, 4> v = {1, 2, 3};

    int* p = v.data();
    SIMPLE_ASSERT(p == &v[0], "data() points to first element");

    *p = 99;
    SIMPLE_ASSERT(v[0] == 99, "data() pointer is writable");

    const SmallVector<int, 4>& cv = v;
    const int* cp = cv.data();
    SIMPLE_ASSERT(cp == &cv[0], "const data() works");

    return true;
}

bool test_sv_get_allocator()
{
    SmallVector<int, 4, TrackingAllocator<int>> v(TrackingAllocator<int>(42));
    SIMPLE_ASSERT(v.get_allocator().id == 42, "get_allocator returns correct allocator");

    return true;
}

bool test_sv_heterogeneous_inline_capacity()
{
    SmallVector<int, 2> v2;
    SmallVector<int, 8> v8;
    SmallVector<int, 16> v16;

    SIMPLE_ASSERT(v2.capacity() == 2, "InlineCapacity=2");
    SIMPLE_ASSERT(v8.capacity() == 8, "InlineCapacity=8");
    SIMPLE_ASSERT(v16.capacity() == 16, "InlineCapacity=16");

    return true;
}

// ==================================================================================
// NEW TESTS: Pointer-based implementation specific tests
// ==================================================================================

bool test_sv_swap_mixed_mode()
{
    // Test inline-heap swap
    {
        SmallVector<int, 4> inline_vec = {1, 2};              // inline (2 <= 4)
        SmallVector<int, 4> heap_vec = {10, 20, 30, 40, 50};  // heap (5 > 4)
        
        SIMPLE_ASSERT(is_using_inline_storage(inline_vec), "inline_vec starts inline");
        SIMPLE_ASSERT(!is_using_inline_storage(heap_vec), "heap_vec starts on heap");
        
        int* heap_ptr_before = heap_vec.data();
        
        inline_vec.swap(heap_vec);
        
        // After swap: inline_vec should have heap's data, heap_vec should be inline
        SIMPLE_ASSERT(inline_vec.size() == 5, "inline_vec has 5 elements after swap");
        SIMPLE_ASSERT(inline_vec.data() == heap_ptr_before, "inline_vec stole heap pointer");
        SIMPLE_ASSERT(inline_vec[0] == 10 && inline_vec[4] == 50, "inline_vec has heap values");
        
        SIMPLE_ASSERT(heap_vec.size() == 2, "heap_vec has 2 elements after swap");
        SIMPLE_ASSERT(is_using_inline_storage(heap_vec), "heap_vec is now inline");
        SIMPLE_ASSERT(heap_vec[0] == 1 && heap_vec[1] == 2, "heap_vec has inline values");
    }
    
    // Test heap-inline swap (reverse direction)
    {
        SmallVector<int, 4> heap_vec = {10, 20, 30, 40, 50};  // heap
        SmallVector<int, 4> inline_vec = {1, 2};              // inline
        
        int* heap_ptr_before = heap_vec.data();
        
        heap_vec.swap(inline_vec);
        
        SIMPLE_ASSERT(inline_vec.size() == 5, "inline_vec has 5 elements");
        SIMPLE_ASSERT(inline_vec.data() == heap_ptr_before, "inline_vec stole heap pointer");
        SIMPLE_ASSERT(heap_vec.size() == 2, "heap_vec has 2 elements");
        SIMPLE_ASSERT(is_using_inline_storage(heap_vec), "heap_vec is now inline");
    }
    
    return true;
}

bool test_sv_move_pointer_steal()
{
    // Verify move construction steals heap pointer (O(1) operation)
    {
        SmallVector<int, 2> v1 = {1, 2, 3, 4, 5};  // heap
        int* original_ptr = v1.data();
        
        SmallVector<int, 2> v2(std::move(v1));
        
        SIMPLE_ASSERT(v2.data() == original_ptr, "Move construction stole heap pointer");
        SIMPLE_ASSERT(v2.size() == 5, "Moved vector has correct size");
        SIMPLE_ASSERT(v1.size() == 0, "Source vector is empty");
        SIMPLE_ASSERT(is_using_inline_storage(v1), "Source reverted to inline storage");
    }
    
    // Verify move assignment steals heap pointer
    {
        SmallVector<int, 2> v1 = {1, 2, 3, 4, 5};  // heap
        int* original_ptr = v1.data();
        
        SmallVector<int, 2> v2;
        v2 = std::move(v1);
        
        SIMPLE_ASSERT(v2.data() == original_ptr, "Move assignment stole heap pointer");
        SIMPLE_ASSERT(v2.size() == 5, "Moved vector has correct size");
        SIMPLE_ASSERT(v1.size() == 0, "Source vector is empty");
    }
    
    // Verify inline move doesn't steal (copies elements)
    {
        SmallVector<int, 4> v1 = {1, 2, 3};  // inline
        int* original_ptr = v1.data();
        
        SmallVector<int, 4> v2(std::move(v1));
        
        // Inline data is not stolen - it's copied to v2's inline buffer
        SIMPLE_ASSERT(v2.data() != original_ptr, "Inline move did NOT steal pointer");
        SIMPLE_ASSERT(is_using_inline_storage(v2), "v2 uses inline storage");
        SIMPLE_ASSERT(v2.size() == 3, "v2 has correct size");
        SIMPLE_ASSERT(v2[0] == 1 && v2[2] == 3, "v2 has correct values");
    }
    
    return true;
}

bool test_sv_shrink_to_fit_pointer_change()
{
    SmallVector<int, 4> v;
    
    // Fill to heap
    for (int i = 0; i < 10; ++i)
    {
        v.push_back(i);
    }
    
    SIMPLE_ASSERT(!is_using_inline_storage(v), "Vector is on heap");
    int* heap_ptr = v.data();
    
    // Shrink to inline-compatible size
    v.resize(3);
    SIMPLE_ASSERT(v.data() == heap_ptr, "Still on heap after resize");
    
    // shrink_to_fit should move back to inline
    v.shrink_to_fit();
    
    SIMPLE_ASSERT(is_using_inline_storage(v), "Vector is now inline after shrink_to_fit");
    SIMPLE_ASSERT(v.data() != heap_ptr, "Pointer changed to inline buffer");
    SIMPLE_ASSERT(v.capacity() == 4, "Capacity is InlineCapacity");
    SIMPLE_ASSERT(v.size() == 3, "Size preserved");
    SIMPLE_ASSERT(v[0] == 0 && v[1] == 1 && v[2] == 2, "Values preserved");
    
    return true;
}

bool test_sv_iterator_invalidation()
{
    SmallVector<int, 4> v = {1, 2, 3, 4};
    
    // Iterators should remain valid after operations that don't reallocate
    auto it = v.begin() + 2;
    int* ptr = &v[2];
    
    v[0] = 10;  // Modification doesn't invalidate
    SIMPLE_ASSERT(*it == 3, "Iterator valid after element modification");
    SIMPLE_ASSERT(*ptr == 3, "Pointer valid after element modification");
    
    // push_back that triggers reallocation invalidates iterators
    v.push_back(5);  // This grows from inline to heap
    // Note: it and ptr are now invalid - we can't test them
    
    // Get new iterators after reallocation
    auto new_it = v.begin();
    SIMPLE_ASSERT(*new_it == 10, "New iterator works after reallocation");
    SIMPLE_ASSERT(v.size() == 5, "Size correct after growth");
    
    // Reserve doesn't invalidate if no reallocation needed
    v.reserve(v.capacity());  // No-op
    auto it2 = v.begin();
    SIMPLE_ASSERT(*it2 == 10, "Iterator valid after no-op reserve");
    
    // Erase invalidates iterators at and after erased position
    v.erase(v.begin());
    SIMPLE_ASSERT(v[0] == 2, "First element after erase");
    
    return true;
}

bool test_sv_reserve_edge_cases()
{
    // reserve(0) on empty vector
    {
        SmallVector<int, 4> v;
        v.reserve(0);
        SIMPLE_ASSERT(v.capacity() == 4, "reserve(0) keeps InlineCapacity");
        SIMPLE_ASSERT(v.empty(), "reserve(0) keeps empty");
    }
    
    // reserve(0) on non-empty vector
    {
        SmallVector<int, 4> v = {1, 2, 3};
        v.reserve(0);
        SIMPLE_ASSERT(v.capacity() == 4, "reserve(0) doesn't shrink");
        SIMPLE_ASSERT(v.size() == 3, "reserve(0) keeps elements");
    }
    
    // reserve less than current capacity
    {
        SmallVector<int, 4> v;
        v.reserve(10);
        size_t cap = v.capacity();
        v.reserve(5);
        SIMPLE_ASSERT(v.capacity() == cap, "reserve less than capacity is no-op");
    }
    
    // reserve exact InlineCapacity
    {
        SmallVector<int, 4> v;
        v.reserve(4);
        SIMPLE_ASSERT(v.capacity() == 4, "reserve(InlineCapacity) stays inline");
        SIMPLE_ASSERT(is_using_inline_storage(v), "Still inline after reserve(InlineCapacity)");
    }
    
    // reserve InlineCapacity + 1 forces heap
    {
        SmallVector<int, 4> v;
        v.reserve(5);
        SIMPLE_ASSERT(v.capacity() >= 5, "reserve(5) increases capacity");
        SIMPLE_ASSERT(!is_using_inline_storage(v), "Moved to heap after reserve(5)");
    }
    
    return true;
}

bool test_sv_insert_boundaries()
{
    // Insert at begin
    {
        SmallVector<int, 4> v = {2, 3, 4};
        auto it = v.insert(v.begin(), 1);
        SIMPLE_ASSERT(*it == 1, "Insert at begin returns correct iterator");
        SIMPLE_ASSERT(v[0] == 1, "Element at begin");
        SIMPLE_ASSERT(v.size() == 4, "Size increased");
    }
    
    // Insert at end
    {
        SmallVector<int, 4> v = {1, 2, 3};
        auto it = v.insert(v.end(), 4);
        SIMPLE_ASSERT(*it == 4, "Insert at end returns correct iterator");
        SIMPLE_ASSERT(v.back() == 4, "Element at end");
        SIMPLE_ASSERT(v.size() == 4, "Size increased");
    }
    
    // Insert multiple at begin
    {
        SmallVector<int, 8> v = {3, 4};
        auto it = v.insert(v.begin(), {1, 2});
        SIMPLE_ASSERT(*it == 1, "Insert multiple at begin");
        SIMPLE_ASSERT(v[0] == 1 && v[1] == 2 && v[2] == 3, "Elements correct");
    }
    
    // Insert multiple at end
    {
        SmallVector<int, 8> v = {1, 2};
        auto it = v.insert(v.end(), {3, 4});
        SIMPLE_ASSERT(*it == 3, "Insert multiple at end");
        SIMPLE_ASSERT(v[2] == 3 && v[3] == 4, "Elements correct");
    }
    
    // Insert 0 elements (no-op)
    {
        SmallVector<int, 4> v = {1, 2, 3};
        auto it = v.insert(v.begin() + 1, 0, 99);
        SIMPLE_ASSERT(it == v.begin() + 1, "Insert 0 returns position");
        SIMPLE_ASSERT(v.size() == 3, "Size unchanged");
    }
    
    // Insert into empty vector
    {
        SmallVector<int, 4> v;
        auto it = v.insert(v.begin(), 42);
        SIMPLE_ASSERT(*it == 42, "Insert into empty");
        SIMPLE_ASSERT(v.size() == 1, "Size is 1");
    }
    
    return true;
}

// =============================================================================
// Self-Range Insertion Stress Tests
// These tests verify the P0 fix for self-range insertion UB in the
// in-place forward-iterator path.
// =============================================================================

bool test_sv_insert_self_range_stress_int()
{
    // Exhaustive test: all combinations of (initial_size, pos, first, last)
    // Compares against std::vector as reference implementation
    using Vec = SmallVector<int, 8>;

    for (int initial_size = 0; initial_size <= 20; ++initial_size) {
        Vec base;
        for (int i = 0; i < initial_size; ++i)
            base.push_back(i);

        for (int first = 0; first <= initial_size; ++first) {
            for (int last = first; last <= initial_size; ++last) {
                for (int pos = 0; pos <= initial_size; ++pos) {

                    // Reference behavior using std::vector
                    std::vector<int> ref_vec(base.begin(), base.end());
                    ref_vec.insert(ref_vec.begin() + pos,
                                   ref_vec.begin() + first,
                                   ref_vec.begin() + last);

                    // Test SmallVector self-insert
                    Vec v = base;
                    v.insert(v.begin() + pos,
                             v.begin() + first,
                             v.begin() + last);

                    SIMPLE_ASSERT(v.size() == ref_vec.size(), 
                        "Size mismatch in self-range insert");

                    bool values_match = true;
                    for (size_t i = 0; i < v.size() && values_match; ++i) {
                        values_match = (v[i] == ref_vec[i]);
                    }
                    SIMPLE_ASSERT(values_match, "Value mismatch in self-range insert");
                }
            }
        }
    }
    return true;
}

bool test_sv_insert_self_range_stress_move_only()
{
    // Verify move-only types work with insert from external source
    // Note: Self-insert with move iterators is not well-defined (moves invalidate source)
    struct MoveOnly {
        int value;
        explicit MoveOnly(int v) : value(v) {}
        MoveOnly(MoveOnly&& other) noexcept : value(other.value) { other.value = -1; }
        MoveOnly& operator=(MoveOnly&& other) noexcept { 
            value = other.value; 
            other.value = -1; 
            return *this; 
        }

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
    };

    using Vec = SmallVector<MoveOnly, 8>;

    // Test insert from external source with move iterators
    {
        Vec v;
        for (int i = 0; i < 4; ++i)
            v.emplace_back(i);

        std::vector<MoveOnly> src;
        src.emplace_back(10);
        src.emplace_back(20);
        src.emplace_back(30);

        v.insert(v.begin() + 2, 
                 std::make_move_iterator(src.begin()),
                 std::make_move_iterator(src.end()));

        SIMPLE_ASSERT(v.size() == 7, "Move insert: size");
        SIMPLE_ASSERT(v[0].value == 0, "Move insert: original[0]");
        SIMPLE_ASSERT(v[2].value == 10, "Move insert: inserted[0]");
        SIMPLE_ASSERT(v[3].value == 20, "Move insert: inserted[1]");
        SIMPLE_ASSERT(v[4].value == 30, "Move insert: inserted[2]");
        SIMPLE_ASSERT(v[5].value == 2, "Move insert: original[2]");
    }

    // Test insert triggering reallocation with move-only type
    {
        Vec v;
        for (int i = 0; i < 8; ++i)  // Fill to inline capacity
            v.emplace_back(i);

        std::vector<MoveOnly> src;
        src.emplace_back(100);

        v.insert(v.begin() + 4,
                 std::make_move_iterator(src.begin()),
                 std::make_move_iterator(src.end()));

        SIMPLE_ASSERT(v.size() == 9, "Move insert realloc: size");
        SIMPLE_ASSERT(v[4].value == 100, "Move insert realloc: inserted value");
    }

    return true;
}

bool test_sv_insert_self_range_throwing_copy()
{
    // Verify basic exception guarantee during temp materialization
    using Vec = SmallVector<ThrowOnCopy, 8>;

    Vec v;
    for (int i = 0; i < 12; ++i)
        v.emplace_back(i);

    // Save original state
    std::vector<int> original_values;
    for (const auto& e : v)
        original_values.push_back(e.value);

    ThrowOnCopy::reset();
    ThrowOnCopy::throw_after = 3;  // Throw after 3 operations

    bool threw = false;
    try {
        v.insert(v.begin() + 3,
                 v.begin() + 2,
                 v.begin() + 7);
    } catch (const std::runtime_error&) {
        threw = true;
    }

    ThrowOnCopy::reset();

    // Either it succeeded or threw - either way, container should be valid
    // If it threw, we have basic guarantee (valid but unspecified state)
    // Just verify we can still use the container
    SIMPLE_ASSERT(v.size() > 0 || threw, "Container should be usable after exception");
    
    // Verify we can iterate without crashing
    size_t count = 0;
    for (const auto& e : v) {
        (void)e.value;
        ++count;
    }
    SIMPLE_ASSERT(count == v.size(), "Iteration count matches size");

    return true;
}

bool test_sv_insert_self_range_specific_cases()
{
    // Specific regression cases for self-range insertion
    using Vec = SmallVector<int, 16>;

    // Case 1: Insert before source range (in-place, no realloc)
    {
        Vec v = {1, 2, 3, 4, 5, 6};
        v.reserve(16);
        v.insert(v.begin() + 1, v.begin() + 3, v.begin() + 5);  // Insert [4,5] at pos 1
        // Expected: {1, 4, 5, 2, 3, 4, 5, 6}
        SIMPLE_ASSERT(v.size() == 8, "Case 1: size");
        SIMPLE_ASSERT(v[0] == 1 && v[1] == 4 && v[2] == 5 && v[3] == 2, "Case 1: values");
    }

    // Case 2: Insert after source range (in-place, no realloc)
    {
        Vec v = {1, 2, 3, 4, 5, 6};
        v.reserve(16);
        v.insert(v.begin() + 5, v.begin(), v.begin() + 2);  // Insert [1,2] at pos 5
        // Expected: {1, 2, 3, 4, 5, 1, 2, 6}
        SIMPLE_ASSERT(v.size() == 8, "Case 2: size");
        SIMPLE_ASSERT(v[5] == 1 && v[6] == 2 && v[7] == 6, "Case 2: values");
    }

    // Case 3: Overlapping insert (source overlaps insertion point)
    {
        Vec v = {1, 2, 3, 4, 5, 6};
        v.reserve(16);
        v.insert(v.begin() + 2, v.begin() + 1, v.begin() + 4);  // Insert [2,3,4] at pos 2
        // Expected: {1, 2, 2, 3, 4, 3, 4, 5, 6}
        SIMPLE_ASSERT(v.size() == 9, "Case 3: size");
        SIMPLE_ASSERT(v[2] == 2 && v[3] == 3 && v[4] == 4, "Case 3: inserted values");
    }

    // Case 4: Insert with reallocation (already handled correctly)
    {
        Vec v = {1, 2, 3, 4};
        // Don't reserve - force reallocation
        v.insert(v.begin() + 2, v.begin(), v.begin() + 2);  // Insert [1,2] at pos 2
        // Expected: {1, 2, 1, 2, 3, 4}
        SIMPLE_ASSERT(v.size() == 6, "Case 4: size");
        SIMPLE_ASSERT(v[0] == 1 && v[2] == 1 && v[3] == 2 && v[4] == 3, "Case 4: values");
    }

    // Case 5: Insert entire vector into itself
    {
        Vec v = {1, 2, 3};
        v.reserve(16);
        v.insert(v.begin() + 1, v.begin(), v.end());  // Insert [1,2,3] at pos 1
        // Expected: {1, 1, 2, 3, 2, 3}
        SIMPLE_ASSERT(v.size() == 6, "Case 5: size");
        SIMPLE_ASSERT(v[0] == 1 && v[1] == 1 && v[2] == 2 && v[3] == 3, "Case 5: values");
    }

    return true;
}

bool test_sv_swap_edge_cases()
{
    // Self-swap
    {
        SmallVector<int, 4> v = {1, 2, 3};
        v.swap(v);
        SIMPLE_ASSERT(v.size() == 3, "Self-swap preserves size");
        SIMPLE_ASSERT(v[0] == 1 && v[2] == 3, "Self-swap preserves values");
    }
    
    // Swap with empty (inline-inline, one empty)
    {
        SmallVector<int, 4> v1 = {1, 2, 3};
        SmallVector<int, 4> v2;
        
        v1.swap(v2);
        
        SIMPLE_ASSERT(v1.empty(), "v1 is now empty");
        SIMPLE_ASSERT(v2.size() == 3, "v2 has elements");
        SIMPLE_ASSERT(v2[0] == 1, "v2 has correct values");
    }
    
    // Swap two empty vectors
    {
        SmallVector<int, 4> v1;
        SmallVector<int, 4> v2;
        
        v1.swap(v2);
        
        SIMPLE_ASSERT(v1.empty() && v2.empty(), "Both still empty");
    }
    
    // Swap heap with empty
    {
        SmallVector<int, 2> v1 = {1, 2, 3, 4, 5};  // heap
        SmallVector<int, 2> v2;  // inline empty
        
        int* heap_ptr = v1.data();
        
        v1.swap(v2);
        
        SIMPLE_ASSERT(v1.empty(), "v1 is empty");
        SIMPLE_ASSERT(is_using_inline_storage(v1), "v1 is inline");
        SIMPLE_ASSERT(v2.size() == 5, "v2 has 5 elements");
        SIMPLE_ASSERT(v2.data() == heap_ptr, "v2 stole heap");
    }
    
    return true;
}

// ==================================================================================
// Operation-Sequence Fuzzer with std::vector Oracle
// ==================================================================================

/**
 * @brief Minimal deterministic RNG for fuzzing
 */
struct FuzzRNG
{
    uint32_t state;
    explicit FuzzRNG(uint32_t s) : state(s) {}

    uint32_t next()
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    int range(int lo, int hi)
    {
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }
};

/**
 * @brief Fuzzer test: random operations with std::vector as oracle
 * 
 * Runs deterministic random sequences of operations on both SmallVector
 * and std::vector, comparing results after each operation. If they ever
 * diverge, reports the seed for reproduction.
 */
bool test_sv_fuzz_operations()
{
    constexpr int Seeds = 100;
    constexpr int Steps = 100;

    for (int seed = 1; seed <= Seeds; ++seed)
    {
        FuzzRNG rng(static_cast<uint32_t>(seed));

        SmallVector<int, 8> sv;
        std::vector<int> ref;

        for (int step = 0; step < Steps; ++step)
        {
            int op = rng.range(0, 12);

            switch (op)
            {
            case 0: // push_back
            {
                int v = rng.range(0, 1000);
                sv.push_back(v);
                ref.push_back(v);
                break;
            }
            case 1: // emplace_back
            {
                int v = rng.range(0, 1000);
                sv.emplace_back(v);
                ref.emplace_back(v);
                break;
            }
            case 2: // insert single
            {
                // Allow insert into empty (pos=0 is valid for empty vector)
                size_t pos = ref.empty() ? 0 : static_cast<size_t>(rng.range(0, static_cast<int>(ref.size())));
                int v = rng.range(0, 1000);
                sv.insert(sv.begin() + static_cast<ptrdiff_t>(pos), v);
                ref.insert(ref.begin() + static_cast<ptrdiff_t>(pos), v);
                break;
            }
            case 3: // insert count
            {
                if (ref.size() > 20) break; // Limit size growth
                size_t pos = ref.empty() ? 0 : static_cast<size_t>(rng.range(0, static_cast<int>(ref.size())));
                size_t count = static_cast<size_t>(rng.range(1, 3));
                int v = rng.range(0, 1000);
                sv.insert(sv.begin() + static_cast<ptrdiff_t>(pos), count, v);
                ref.insert(ref.begin() + static_cast<ptrdiff_t>(pos), count, v);
                break;
            }
            case 4: // erase single
            {
                if (ref.empty()) break;
                size_t pos = static_cast<size_t>(rng.range(0, static_cast<int>(ref.size()) - 1));
                sv.erase(sv.begin() + static_cast<ptrdiff_t>(pos));
                ref.erase(ref.begin() + static_cast<ptrdiff_t>(pos));
                break;
            }
            case 5: // erase range
            {
                if (ref.size() < 2) break;
                size_t first = static_cast<size_t>(rng.range(0, static_cast<int>(ref.size()) - 1));
                size_t last = static_cast<size_t>(rng.range(static_cast<int>(first), static_cast<int>(ref.size())));
                sv.erase(sv.begin() + static_cast<ptrdiff_t>(first), 
                         sv.begin() + static_cast<ptrdiff_t>(last));
                ref.erase(ref.begin() + static_cast<ptrdiff_t>(first), 
                          ref.begin() + static_cast<ptrdiff_t>(last));
                break;
            }
            case 6: // resize (shrink or grow)
            {
                size_t n = static_cast<size_t>(rng.range(0, 16));
                sv.resize(n);
                ref.resize(n);
                break;
            }
            case 7: // resize with value
            {
                size_t n = static_cast<size_t>(rng.range(0, 16));
                int v = rng.range(0, 1000);
                sv.resize(n, v);
                ref.resize(n, v);
                break;
            }
            case 8: // reserve
            {
                size_t n = static_cast<size_t>(rng.range(0, 32));
                sv.reserve(n);
                ref.reserve(n);
                break;
            }
            case 9: // clear
            {
                sv.clear();
                ref.clear();
                break;
            }
            case 10: // shrink_to_fit
            {
                // NOTE: std::vector::shrink_to_fit() is non-binding (may be ignored).
                // SmallVector::shrink_to_fit() actively demotes to inline when possible.
                // We only compare size/content, not capacity, so this oracle comparison is valid.
                sv.shrink_to_fit();
                ref.shrink_to_fit();
                break;
            }
            case 11: // assign count
            {
                size_t count = static_cast<size_t>(rng.range(0, 10));
                int v = rng.range(0, 1000);
                sv.assign(count, v);
                ref.assign(count, v);
                break;
            }
            case 12: // pop_back
            {
                if (ref.empty()) break;
                sv.pop_back();
                ref.pop_back();
                break;
            }
            }

            // Verify consistency after each operation
            if (sv.size() != ref.size())
            {
                std::cerr << "Fuzzer FAIL at seed=" << seed << " step=" << step 
                          << " op=" << op << ": size mismatch (sv=" << sv.size() 
                          << " ref=" << ref.size() << ")\n";
                return false;
            }
            if (!std::equal(sv.begin(), sv.end(), ref.begin()))
            {
                std::cerr << "Fuzzer FAIL at seed=" << seed << " step=" << step 
                          << " op=" << op << ": content mismatch\n";
                return false;
            }
        }
    }

    return true;
}

// Test over-aligned types to catch subtle inline-storage alignment bugs
bool test_sv_over_aligned_types()
{
    struct alignas(64) Aligned64
    {
        int x;
        Aligned64() : x(0) {}
        explicit Aligned64(int v) : x(v) {}
        bool operator==(const Aligned64& o) const { return x == o.x; }
    };
    
    // Verify alignment requirement
    static_assert(alignof(Aligned64) == 64, "Test type must be 64-byte aligned");
    
    SmallVector<Aligned64, 4> v;
    
    // Test inline storage alignment
    v.push_back(Aligned64(1));
    SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(&v[0]) % 64 == 0, 
                  "Inline storage must respect alignment");
    
    v.push_back(Aligned64(2));
    v.push_back(Aligned64(3));
    v.push_back(Aligned64(4));
    
    // All elements must be properly aligned
    for (size_t i = 0; i < v.size(); ++i)
    {
        SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(&v[i]) % 64 == 0,
                      "Each inline element must be aligned");
    }
    
    // Force transition to heap
    v.push_back(Aligned64(5));
    SIMPLE_ASSERT(v.size() == 5, "Transitioned to heap storage");
    
    // Heap storage must also be aligned
    for (size_t i = 0; i < v.size(); ++i)
    {
        SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(&v[i]) % 64 == 0,
                      "Each heap element must be aligned");
    }
    
    // Test copy
    auto v2 = v;
    SIMPLE_ASSERT(v2.size() == 5, "Copy preserves size");
    for (size_t i = 0; i < v2.size(); ++i)
    {
        SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(&v2[i]) % 64 == 0,
                      "Copied elements must be aligned");
    }
    
    // Test move back to inline via shrink_to_fit
    v.resize(2);
    v.shrink_to_fit();
    SIMPLE_ASSERT(v.capacity() == 4, "Shrunk back to inline");
    for (size_t i = 0; i < v.size(); ++i)
    {
        SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(&v[i]) % 64 == 0,
                      "Shrunk elements must be aligned");
    }
    
    return true;
}

// Test mixed swap (inline <-> heap) with throwing move to verify rollback invariants
bool test_sv_swap_throw_rollback()
{
    // Test 1: Successful mixed swap (baseline)
    {
        ThrowOnCopy::reset();
        
        SmallVector<ThrowOnCopy, 4> inline_vec;
        inline_vec.push_back(ThrowOnCopy(1));
        inline_vec.push_back(ThrowOnCopy(2));
        
        SmallVector<ThrowOnCopy, 4> heap_vec;
        for (int i = 0; i < 8; ++i)
        {
            heap_vec.push_back(ThrowOnCopy(100 + i));
        }
        
        ThrowOnCopy::reset();
        
        // Swap inline <-> heap without throwing
        inline_vec.swap(heap_vec);
        
        SIMPLE_ASSERT(inline_vec.size() == 8, "Inline now has heap contents");
        SIMPLE_ASSERT(heap_vec.size() == 2, "Heap now has inline contents");
        SIMPLE_ASSERT(inline_vec[0].value == 100, "Values swapped correctly");
        SIMPLE_ASSERT(heap_vec[0].value == 1, "Values swapped correctly");
    }
    
    // Test 2: Swap with throw - verify both containers remain valid
    {
        ThrowOnCopy::reset();
        
        SmallVector<ThrowOnCopy, 4> inline_vec;
        inline_vec.push_back(ThrowOnCopy(1));
        inline_vec.push_back(ThrowOnCopy(2));
        inline_vec.push_back(ThrowOnCopy(3));
        
        SmallVector<ThrowOnCopy, 4> heap_vec;
        for (int i = 0; i < 6; ++i)
        {
            heap_vec.push_back(ThrowOnCopy(100 + i));
        }
        
        // Enable throwing after a few moves
        ThrowOnCopy::reset();
        ThrowOnCopy::throw_after = 2;
        
        bool threw = false;
        try
        {
            inline_vec.swap(heap_vec);
        }
        catch (const std::exception&)
        {
            threw = true;
        }
        
        ThrowOnCopy::reset();
        
        // Whether or not it threw, both containers must be valid
        // (Basic guarantee: no leaks, containers usable)
        
        // Can iterate both without crashing
        [[maybe_unused]] int sum1 = 0;
        for (const auto& e : inline_vec)
        {
            sum1 += e.value;
        }
        
        [[maybe_unused]] int sum2 = 0;
        for (const auto& e : heap_vec)
        {
            sum2 += e.value;
        }
        
        // Both should be clearable without crash
        inline_vec.clear();
        heap_vec.clear();
        
        SIMPLE_ASSERT(inline_vec.empty(), "inline_vec cleared");
        SIMPLE_ASSERT(heap_vec.empty(), "heap_vec cleared");
        
        // Note: We don't assert threw==true because move_if_noexcept may use copy
        // The important thing is no crash/leak regardless
        (void)threw;
    }
    
    return true;
}

// ==================================================================================
// C++20 Feature Tests
// ==================================================================================

#if FATP_HAS_CPP20
bool test_sv_contains()
{
    SmallVector<int, 4> v = {1, 2, 3, 4};
    
    SIMPLE_ASSERT(v.contains(1), "contains finds first element");
    SIMPLE_ASSERT(v.contains(4), "contains finds last element");
    SIMPLE_ASSERT(v.contains(2), "contains finds middle element");
    SIMPLE_ASSERT(!v.contains(0), "contains returns false for missing element");
    SIMPLE_ASSERT(!v.contains(5), "contains returns false for value not in range");
    SIMPLE_ASSERT(!v.contains(-1), "contains returns false for negative value");
    
    // Test with empty vector
    SmallVector<int, 4> empty;
    SIMPLE_ASSERT(!empty.contains(1), "contains returns false for empty vector");
    
    // Test with strings
    SmallVector<std::string, 4> sv = {"hello", "world"};
    SIMPLE_ASSERT(sv.contains("hello"), "contains works with strings");
    SIMPLE_ASSERT(!sv.contains("foo"), "contains returns false for missing string");
    
    return true;
}

bool test_sv_spaceship_operator()
{
    SmallVector<int, 4> v1 = {1, 2, 3};
    SmallVector<int, 4> v2 = {1, 2, 3};
    SmallVector<int, 4> v3 = {1, 2, 4};
    SmallVector<int, 4> v4 = {1, 2};
    
    // Test strong ordering results
    SIMPLE_ASSERT((v1 <=> v2) == std::strong_ordering::equal, "Equal vectors compare equal");
    SIMPLE_ASSERT((v1 <=> v3) == std::strong_ordering::less, "Lexicographically less");
    SIMPLE_ASSERT((v3 <=> v1) == std::strong_ordering::greater, "Lexicographically greater");
    SIMPLE_ASSERT((v4 <=> v1) == std::strong_ordering::less, "Shorter vector is less");
    SIMPLE_ASSERT((v1 <=> v4) == std::strong_ordering::greater, "Longer vector is greater");
    
    // Cross-capacity comparison
    SmallVector<int, 8> v5 = {1, 2, 3};
    SIMPLE_ASSERT((v1 <=> v5) == std::strong_ordering::equal, "Cross-capacity equal");
    
    // Empty vectors
    SmallVector<int, 4> empty1, empty2;
    SIMPLE_ASSERT((empty1 <=> empty2) == std::strong_ordering::equal, "Empty vectors equal");
    SIMPLE_ASSERT((empty1 <=> v1) == std::strong_ordering::less, "Empty less than non-empty");
    
    return true;
}
#endif

// ==================================================================================
// Benchmarks
// ==================================================================================

void benchmark_small_vector()
{
    std::cout << "\n" << colors::cyan() << "SmallVector Benchmarks:" << colors::reset() << "\n\n";

    constexpr size_t N = 10000;

    std::cout << colors::yellow() << "1. Construction & Push Back (Small Size)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector<int,8> push 4", [&]()
    {
        SmallVector<int, 8> v;
        for (int i = 0; i < 4; ++i)
        {
            v.push_back(i);
        }
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    benchmark("std::vector<int> push 4", [&]()
    {
        std::vector<int> v;
        for (int i = 0; i < 4; ++i)
        {
            v.push_back(i);
        }
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    std::cout << "\n" << colors::yellow() << "2. Construction & Push Back (Large Size)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector<int,8> push 100", [&]()
    {
        SmallVector<int, 8> v;
        for (int i = 0; i < 100; ++i)
        {
            v.push_back(i);
        }
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    benchmark("std::vector<int> push 100", [&]()
    {
        std::vector<int> v;
        for (int i = 0; i < 100; ++i)
        {
            v.push_back(i);
        }
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    std::cout << "\n" << colors::yellow() << "3. Copy Operations (Inline)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    SmallVector<int, 8> src_small;
    for (int i = 0; i < 4; ++i)
    {
        src_small.push_back(i);
    }

    benchmark("SmallVector copy (inline)", [&]()
    {
        SmallVector<int, 8> v = src_small;
        DoNotOptimize(v.data());
    }, N);

    std::vector<int> src_vec;
    for (int i = 0; i < 4; ++i)
    {
        src_vec.push_back(i);
    }

    benchmark("std::vector copy (4 elem)", [&]()
    {
        std::vector<int> v = src_vec;
        DoNotOptimize(v.data());
    }, N);

    std::cout << "\n" << colors::yellow() << "4. Copy Operations (Heap)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    SmallVector<int, 8> src_large;
    for (int i = 0; i < 100; ++i)
    {
        src_large.push_back(i);
    }

    benchmark("SmallVector copy (heap)", [&]()
    {
        SmallVector<int, 8> v = src_large;
        DoNotOptimize(v.data());
    }, N);

    std::vector<int> src_vec_large;
    for (int i = 0; i < 100; ++i)
    {
        src_vec_large.push_back(i);
    }

    benchmark("std::vector copy (100 elem)", [&]()
    {
        std::vector<int> v = src_vec_large;
        DoNotOptimize(v.data());
    }, N);

    std::cout << "\n" << colors::yellow() << "5. Iteration" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    DoNotOptimize(src_small.data());
    DoNotOptimize(src_vec.data());
    
    benchmark("SmallVector iterate (inline)", [&]()
    {
        int sum = 0;
        for (int x : src_small)
        {
            DoNotOptimize(x);
            sum += x;
        }
        DoNotOptimize(sum);
    }, N);

    benchmark("std::vector iterate (4 elem)", [&]()
    {
        int sum = 0;
        for (int x : src_vec)
        {
            DoNotOptimize(x);
            sum += x;
        }
        DoNotOptimize(sum);
    }, N);

    std::cout << "\n" << colors::yellow() << "6. Insert Operations" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector insert middle", [&]()
    {
        SmallVector<int, 8> v = {1, 2, 3, 4};
        v.insert(v.begin() + 2, 99);
        DoNotOptimize(v.data());
    }, N);

    benchmark("std::vector insert middle", [&]()
    {
        std::vector<int> v = {1, 2, 3, 4};
        v.insert(v.begin() + 2, 99);
        DoNotOptimize(v.data());
    }, N);

    std::cout << "\n" << colors::yellow() << "7. Emplace Back" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector emplace_back", [&]()
    {
        SmallVector<std::string, 8> v;
        for (int i = 0; i < 4; ++i)
        {
            v.emplace_back("test");
        }
        DoNotOptimize(v.data());
    }, N / 10);

    benchmark("std::vector emplace_back", [&]()
    {
        std::vector<std::string> v;
        for (int i = 0; i < 4; ++i)
        {
            v.emplace_back("test");
        }
        DoNotOptimize(v.data());
    }, N / 10);

    std::cout << "\n" << colors::yellow() << "8. Reserve Operations" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector reserve", [&]()
    {
        SmallVector<int, 8> v;
        v.reserve(50);
        DoNotOptimize(v.data());
    }, N);

    benchmark("std::vector reserve", [&]()
    {
        std::vector<int> v;
        v.reserve(50);
        DoNotOptimize(v.data());
    }, N);

    // ==================================================================================
    // NEW BENCHMARKS
    // ==================================================================================

    std::cout << "\n" << colors::yellow() << "9. Move Construction (Heap - should be O(1))" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector move construct (heap)", [&]()
    {
        SmallVector<int, 4> v1;
        for (int i = 0; i < 100; ++i) v1.push_back(i);
        SmallVector<int, 4> v2(std::move(v1));
        DoNotOptimize(v2.data());
    }, N);

    benchmark("std::vector move construct", [&]()
    {
        std::vector<int> v1;
        for (int i = 0; i < 100; ++i) v1.push_back(i);
        std::vector<int> v2(std::move(v1));
        DoNotOptimize(v2.data());
    }, N);

    std::cout << "\n" << colors::yellow() << "10. Move Construction (Inline)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector move construct (inline)", [&]()
    {
        SmallVector<int, 8> v1 = {1, 2, 3, 4};
        SmallVector<int, 8> v2(std::move(v1));
        DoNotOptimize(v2.data());
    }, N);

    benchmark("std::vector move construct (4 elem)", [&]()
    {
        std::vector<int> v1 = {1, 2, 3, 4};
        std::vector<int> v2(std::move(v1));
        DoNotOptimize(v2.data());
    }, N);

    std::cout << "\n" << colors::yellow() << "11. Move Assignment (Heap)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector move assign (heap)", [&]()
    {
        SmallVector<int, 4> v1;
        for (int i = 0; i < 100; ++i) v1.push_back(i);
        SmallVector<int, 4> v2;
        v2 = std::move(v1);
        DoNotOptimize(v2.data());
    }, N);

    benchmark("std::vector move assign", [&]()
    {
        std::vector<int> v1;
        for (int i = 0; i < 100; ++i) v1.push_back(i);
        std::vector<int> v2;
        v2 = std::move(v1);
        DoNotOptimize(v2.data());
    }, N);

    std::cout << "\n" << colors::yellow() << "12. shrink_to_fit (Heap to Inline)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector shrink_to_fit", [&]()
    {
        SmallVector<int, 8> v;
        for (int i = 0; i < 20; ++i) v.push_back(i);
        v.resize(4);
        v.shrink_to_fit();
        DoNotOptimize(v.data());
    }, N);

    benchmark("std::vector shrink_to_fit", [&]()
    {
        std::vector<int> v;
        for (int i = 0; i < 20; ++i) v.push_back(i);
        v.resize(4);
        v.shrink_to_fit();
        DoNotOptimize(v.data());
    }, N);

    std::cout << "\n" << colors::yellow() << "13. Different InlineCapacity Values" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector<int,4> push 4", [&]()
    {
        SmallVector<int, 4> v;
        for (int i = 0; i < 4; ++i) v.push_back(i);
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    benchmark("SmallVector<int,8> push 4", [&]()
    {
        SmallVector<int, 8> v;
        for (int i = 0; i < 4; ++i) v.push_back(i);
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    benchmark("SmallVector<int,16> push 4", [&]()
    {
        SmallVector<int, 16> v;
        for (int i = 0; i < 4; ++i) v.push_back(i);
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    benchmark("SmallVector<int,32> push 4", [&]()
    {
        SmallVector<int, 32> v;
        for (int i = 0; i < 4; ++i) v.push_back(i);
        DoNotOptimize(v.data());
        DoNotOptimize(v.size());
    }, N);

    std::cout << "\n" << colors::yellow() << "14. Non-Trivial Types (std::string)" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector<string,4> push 4", [&]()
    {
        SmallVector<std::string, 4> v;
        v.push_back("one");
        v.push_back("two");
        v.push_back("three");
        v.push_back("four");
        DoNotOptimize(v.data());
    }, N / 10);

    benchmark("std::vector<string> push 4", [&]()
    {
        std::vector<std::string> v;
        v.push_back("one");
        v.push_back("two");
        v.push_back("three");
        v.push_back("four");
        DoNotOptimize(v.data());
    }, N / 10);

    std::cout << "\n" << colors::yellow() << "15. Sequential Access Pattern" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    SmallVector<int, 64> sv_access;
    std::vector<int> vec_access;
    for (int i = 0; i < 64; ++i)
    {
        sv_access.push_back(i);
        vec_access.push_back(i);
    }
    DoNotOptimize(sv_access.data());
    DoNotOptimize(vec_access.data());

    benchmark("SmallVector sequential read", [&]()
    {
        int sum = 0;
        for (size_t i = 0; i < sv_access.size(); ++i)
        {
            DoNotOptimize(sv_access[i]);
            sum += sv_access[i];
        }
        DoNotOptimize(sum);
    }, N);

    benchmark("std::vector sequential read", [&]()
    {
        int sum = 0;
        for (size_t i = 0; i < vec_access.size(); ++i)
        {
            DoNotOptimize(vec_access[i]);
            sum += vec_access[i];
        }
        DoNotOptimize(sum);
    }, N);

    std::cout << "\n" << colors::yellow() << "16. Random Access Pattern" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    // Pre-generate random indices
    std::vector<size_t> indices(64);
    for (size_t i = 0; i < 64; ++i) indices[i] = (i * 37) % 64;  // Pseudo-random
    DoNotOptimize(indices.data());

    benchmark("SmallVector random read", [&]()
    {
        int sum = 0;
        for (size_t idx : indices)
        {
            DoNotOptimize(sv_access[idx]);
            sum += sv_access[idx];
        }
        DoNotOptimize(sum);
    }, N);

    benchmark("std::vector random read", [&]()
    {
        int sum = 0;
        for (size_t idx : indices)
        {
            DoNotOptimize(vec_access[idx]);
            sum += vec_access[idx];
        }
        DoNotOptimize(sum);
    }, N);

    std::cout << "\n" << colors::yellow() << "17. data()[] Access Pattern" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector data()[] sequential", [&]()
    {
        int sum = 0;
        int* p = sv_access.data();
        size_t sz = sv_access.size();
        for (size_t i = 0; i < sz; ++i)
        {
            DoNotOptimize(p[i]);
            sum += p[i];
        }
        DoNotOptimize(sum);
    }, N);

    benchmark("std::vector data()[] sequential", [&]()
    {
        int sum = 0;
        int* p = vec_access.data();
        size_t sz = vec_access.size();
        for (size_t i = 0; i < sz; ++i)
        {
            DoNotOptimize(p[i]);
            sum += p[i];
        }
        DoNotOptimize(sum);
    }, N);

    std::cout << colors::blue() << "  Note: " << colors::reset() 
              << "On MSVC, operator[] benchmarks slower than data()[] for BOTH SmallVector\n"
              << "        AND std::vector, even with all bounds checks removed. This is an MSVC\n"
              << "        codegen artifact, not caused by enforce() or this implementation.\n"
              << "        GCC/Clang on Linux do not exhibit this behavior.\n";

    std::cout << "\n" << colors::yellow() << "18. Swap Operations" << colors::reset() << "\n";
    print_benchmark_context(std::cout);
    benchmark("SmallVector swap (inline-inline)", [&]()
    {
        SmallVector<int, 8> v1 = {1, 2, 3, 4};
        SmallVector<int, 8> v2 = {5, 6, 7, 8};
        v1.swap(v2);
        DoNotOptimize(v1.data());
        DoNotOptimize(v2.data());
    }, N);

    benchmark("std::vector swap", [&]()
    {
        std::vector<int> v1 = {1, 2, 3, 4};
        std::vector<int> v2 = {5, 6, 7, 8};
        v1.swap(v2);
        DoNotOptimize(v1.data());
        DoNotOptimize(v2.data());
    }, N);

    benchmark("SmallVector swap (heap-heap)", [&]()
    {
        SmallVector<int, 2> v1 = {1, 2, 3, 4, 5};
        SmallVector<int, 2> v2 = {6, 7, 8, 9, 10};
        v1.swap(v2);
        DoNotOptimize(v1.data());
        DoNotOptimize(v2.data());
    }, N);
}

bool test_SmallVector()
{
    PRINT_HEADER(SMALL VECTOR)

    TestRunner runner;

    // Original tests
    RUN_TEST(runner, sv_basic_construction);
    RUN_TEST(runner, sv_copy_construction);
    RUN_TEST(runner, sv_move_construction);
    RUN_TEST(runner, sv_copy_assignment);
    RUN_TEST(runner, sv_move_assignment);
    RUN_TEST(runner, sv_element_access);
    RUN_TEST(runner, sv_iterators);
    RUN_TEST(runner, sv_capacity_operations);
    RUN_TEST(runner, sv_push_back_emplace_back);
    RUN_TEST(runner, sv_pop_back);
    RUN_TEST(runner, sv_insert);
    RUN_TEST(runner, sv_emplace);
    RUN_TEST(runner, sv_erase);
    RUN_TEST(runner, sv_swap);
    RUN_TEST(runner, sv_comparison_operators);
    RUN_TEST(runner, sv_inline_to_heap_transition);
    RUN_TEST(runner, sv_shrink_to_fit);
    RUN_TEST(runner, sv_allocator_propagation_pocma);
    RUN_TEST(runner, sv_allocator_propagation_pocca);
    RUN_TEST(runner, sv_allocator_propagation_pocs);
    RUN_TEST(runner, sv_exception_safety_grow);
    RUN_TEST(runner, sv_exception_safety_insert);
    RUN_TEST(runner, sv_move_only_types);
    RUN_TEST(runner, sv_large_objects);
    RUN_TEST(runner, sv_non_trivial_types);
    RUN_TEST(runner, sv_edge_case_empty_operations);
    RUN_TEST(runner, sv_edge_case_single_element);
    RUN_TEST(runner, sv_edge_case_exact_inline_capacity);
    RUN_TEST(runner, sv_assign_operations);
    RUN_TEST(runner, sv_max_size);
    RUN_TEST(runner, sv_data_pointer);
    RUN_TEST(runner, sv_get_allocator);
    RUN_TEST(runner, sv_heterogeneous_inline_capacity);

    // NEW TESTS for pointer-based implementation
    RUN_TEST(runner, sv_swap_mixed_mode);
    RUN_TEST(runner, sv_move_pointer_steal);
    RUN_TEST(runner, sv_shrink_to_fit_pointer_change);
    RUN_TEST(runner, sv_iterator_invalidation);
    RUN_TEST(runner, sv_reserve_edge_cases);
    RUN_TEST(runner, sv_insert_boundaries);
    
    // Self-range insertion stress tests (P0 fix verification)
    RUN_TEST(runner, sv_insert_self_range_stress_int);
    RUN_TEST(runner, sv_insert_self_range_stress_move_only);
    RUN_TEST(runner, sv_insert_self_range_throwing_copy);
    RUN_TEST(runner, sv_insert_self_range_specific_cases);
    
    RUN_TEST(runner, sv_swap_edge_cases);

    // Additional edge case tests
    RUN_TEST(runner, sv_over_aligned_types);
    RUN_TEST(runner, sv_swap_throw_rollback);

    // Fuzzer test - runs deterministic random sequences
    RUN_TEST(runner, sv_fuzz_operations);

    // C++20 feature tests
#if FATP_HAS_CPP20
    RUN_TEST(runner, sv_contains);
    RUN_TEST(runner, sv_spaceship_operator);
#endif

    benchmark_small_vector();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SmallVector() ? 0 : 1;
}
#endif
