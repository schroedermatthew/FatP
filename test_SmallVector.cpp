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
    static int copy_count;
    static int throw_after;

    ThrowOnCopy(int v = 0) : value(v) {}

    ThrowOnCopy(const ThrowOnCopy& other) : value(other.value)
    {
        if (++copy_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Copy threw");
        }
    }

    ThrowOnCopy& operator=(const ThrowOnCopy& other)
    {
        if (++copy_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Assignment threw");
        }
        value = other.value;
        return *this;
    }

    ThrowOnCopy(ThrowOnCopy&& other) : value(other.value)
    {
        if (++copy_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Move threw");
        }
    }
    
    ThrowOnCopy& operator=(ThrowOnCopy&& other)
    {
        if (++copy_count >= throw_after && throw_after > 0)
        {
            throw std::runtime_error("Move assignment threw");
        }
        value = other.value;
        return *this;
    }

    bool operator==(const ThrowOnCopy& other) const { return value == other.value; }

    static void reset() { copy_count = 0; throw_after = -1; }
};

int ThrowOnCopy::copy_count = 0;
int ThrowOnCopy::throw_after = -1;

// Helper to check if a SmallVector is using inline storage
template<typename T, size_t N, typename A>
bool is_using_inline_storage(const SmallVector<T, N, A>& v)
{
    // The data pointer should be within the object's memory range for inline storage
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
    v5 = std::move(v5);
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
        v.at(10);
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
    auto it3 = v.begin() + 1;
    v.erase(v.begin());
    // it3 now points to what was v[2], which is now v[1]
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
// Benchmarks
// ==================================================================================

void benchmark_small_vector()
{
    std::cout << "\n" << colors::cyan() << "SmallVector Benchmarks:" << colors::reset() << "\n\n";

    constexpr size_t N = 10000;

    std::cout << colors::yellow() << "1. Construction & Push Back (Small Size)" << colors::reset() << "\n";
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

    std::cout << "\n" << colors::yellow() << "17. Swap Operations" << colors::reset() << "\n";
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
    RUN_TEST(runner, sv_swap_edge_cases);

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
