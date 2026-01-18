# Action Plans for Unbenchmarked Containers

---

# 1. CircularBuffer Action Plan

**Target:** `api_stability: candidate`  
**Effort:** ~2.5 hours  
**Risk:** Low

## Phase 1: Test Additions (2h)

### 1.1 Edge Case Tests

```cpp
FATP_TEST_CASE(capacity_two)
{
    CircularBuffer<int, 2> buffer;
    
    FATP_ASSERT_TRUE(buffer.push(1), "First push");
    FATP_ASSERT_TRUE(buffer.push(2), "Second push");
    FATP_ASSERT_TRUE(buffer.full(), "Should be full at capacity 2");
    FATP_ASSERT_TRUE(!buffer.push(3), "Cannot push when full");
    
    int val;
    FATP_ASSERT_TRUE(buffer.pop(val) && val == 1, "FIFO order");
    FATP_ASSERT_TRUE(buffer.push(3), "Can push after pop");
    FATP_ASSERT_TRUE(buffer.pop(val) && val == 2, "FIFO order");
    FATP_ASSERT_TRUE(buffer.pop(val) && val == 3, "FIFO order");
    FATP_ASSERT_TRUE(buffer.empty(), "Should be empty");
    
    return true;
}

FATP_TEST_CASE(non_power_of_two_capacity)
{
    // Capacity 7 should round up to buffer_size 8
    CircularBuffer<int, 7> buffer;
    
    FATP_ASSERT_EQ(buffer.capacity(), 7u, "Capacity should be 7");
    FATP_ASSERT_EQ(buffer.buffer_size(), 8u, "Buffer size should be 8 (next power of 2)");
    
    // Fill to capacity
    for (int i = 0; i < 7; ++i)
    {
        FATP_ASSERT_TRUE(buffer.push(i), "Should push up to capacity");
    }
    FATP_ASSERT_TRUE(buffer.full(), "Should be full at 7");
    FATP_ASSERT_TRUE(!buffer.push(99), "Cannot exceed capacity");
    
    return true;
}

FATP_TEST_CASE(front_stability)
{
    CircularBuffer<int, 8> buffer;
    
    buffer.push(42);
    const int* ptr1 = buffer.front();
    
    buffer.push(99);  // Push more
    buffer.push(100);
    
    const int* ptr2 = buffer.front();
    FATP_ASSERT_EQ(ptr1, ptr2, "front() pointer should remain stable during push");
    FATP_ASSERT_EQ(*ptr2, 42, "front() value should remain 42");
    
    return true;
}
```

### 1.2 Update Test Runner

Add to `test_CircularBuffer()`:
```cpp
FATP_RUN_TEST_NS(runner, circularbuffer, capacity_two);
FATP_RUN_TEST_NS(runner, circularbuffer, non_power_of_two_capacity);
FATP_RUN_TEST_NS(runner, circularbuffer, front_stability);
```

## Phase 2: Optional Enhancements (30m)

### 2.1 Add `try_push()` / `try_pop()` Aliases

```cpp
// Add after pop() definition
[[nodiscard]] bool try_push(const T& value) noexcept(std::is_nothrow_copy_assignable_v<T>)
{
    return push(value);
}

[[nodiscard]] bool try_push(T&& value) noexcept(std::is_nothrow_move_assignable_v<T>)
{
    return push(std::move(value));
}

[[nodiscard]] bool try_pop(T& value) noexcept(std::is_nothrow_move_assignable_v<T>)
{
    return pop(value);
}
```

### 2.2 Add `back()` Method

```cpp
/**
 * @brief Peek at the back element (most recently pushed)
 * @return Pointer to back element, or nullptr if empty
 * @note Only the producer thread should call this
 */
[[nodiscard]] const T* back() const noexcept
{
    size_t write = write_idx_.load(std::memory_order_acquire);
    size_t read = read_idx_.load(std::memory_order_acquire);
    
    if (write == read)
    {
        return nullptr;  // Empty
    }
    
    // Back is at (write - 1) & INDEX_MASK
    size_t back_idx = (write - 1) & INDEX_MASK;
    return &mBuffer[back_idx];
}
```

## Checklist

| Task | Effort | Status |
|------|--------|--------|
| Add `capacity_two` test | 20m | ☐ |
| Add `non_power_of_two_capacity` test | 20m | ☐ |
| Add `front_stability` test | 20m | ☐ |
| Add `try_push()`/`try_pop()` aliases | 15m | ☐ |
| Add `back()` method | 15m | ☐ |
| Run all tests | 10m | ☐ |
| Update FATP_META to `candidate` | 5m | ☐ |

---

# 2. IntrusiveList Action Plan

**Target:** `api_stability: candidate`  
**Effort:** ~7 hours  
**Risk:** Low

## Phase 1: Code Fixes (1h)

### 1.1 Add `[[nodiscard]]` Attributes

```cpp
// Line 316-323: Add [[nodiscard]]
[[nodiscard]] bool empty() const { return size_ == 0; }
[[nodiscard]] size_type size() const { return size_; }
```

### 1.2 Add Assertions to `front()` / `back()`

```cpp
#include <cassert>  // Add to includes

reference front()
{
    assert(mHead != nullptr && "IntrusiveList::front() called on empty list");
    return *static_cast<T*>(mHead);
}
const_reference front() const
{
    assert(mHead != nullptr && "IntrusiveList::front() called on empty list");
    return *static_cast<const T*>(mHead);
}

reference back()
{
    assert(mTail != nullptr && "IntrusiveList::back() called on empty list");
    return *static_cast<T*>(mTail);
}
const_reference back() const
{
    assert(mTail != nullptr && "IntrusiveList::back() called on empty list");
    return *static_cast<const T*>(mTail);
}
```

## Phase 2: Test Additions (3h)

### 2.1 Critical Missing Tests

```cpp
FATP_TEST_CASE(intrusive_list_single_element)
{
    IntrusiveList<TestNode> list;
    TestNode n1(1, "one");
    
    list.push_back(n1);
    
    FATP_ASSERT_EQ(list.size(), 1u, "Size should be 1");
    FATP_ASSERT_TRUE(&list.front() == &n1, "Front should be n1");
    FATP_ASSERT_TRUE(&list.back() == &n1, "Back should be n1");
    FATP_ASSERT_TRUE(n1.is_linked(), "n1 should be linked");
    
    list.remove(n1);
    
    FATP_ASSERT_TRUE(list.empty(), "List should be empty");
    FATP_ASSERT_TRUE(!n1.is_linked(), "n1 should not be linked");
    
    return true;
}

FATP_TEST_CASE(intrusive_list_remove_unlinked)
{
    IntrusiveList<TestNode> list;
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    
    list.push_back(n1);
    
    // n2 is not in the list
    list.remove(n2);  // Should be safe no-op
    
    FATP_ASSERT_EQ(list.size(), 1u, "Size should still be 1");
    FATP_ASSERT_TRUE(list.front().value == 1, "n1 should still be in list");
    
    return true;
}

FATP_TEST_CASE(intrusive_list_is_linked)
{
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    
    FATP_ASSERT_TRUE(!n1.is_linked(), "New node should not be linked");
    
    IntrusiveList<TestNode> list;
    list.push_back(n1);
    
    FATP_ASSERT_TRUE(n1.is_linked(), "Pushed node should be linked");
    FATP_ASSERT_TRUE(!n2.is_linked(), "Unpushed node should not be linked");
    
    list.pop_back();
    
    FATP_ASSERT_TRUE(!n1.is_linked(), "Popped node should not be linked");
    
    return true;
}

FATP_TEST_CASE(intrusive_list_insert_at_begin)
{
    IntrusiveList<TestNode> list;
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    
    list.push_back(n2);
    list.push_back(n3);
    
    // Insert at begin
    list.insert(list.begin(), n1);
    
    FATP_ASSERT_EQ(list.size(), 3u, "Size should be 3");
    FATP_ASSERT_EQ(list.front().value, 1, "Front should be n1");
    
    std::vector<int> values;
    for (const auto& node : list)
    {
        values.push_back(node.value);
    }
    
    FATP_ASSERT_EQ(values[0], 1, "Order: 1");
    FATP_ASSERT_EQ(values[1], 2, "Order: 2");
    FATP_ASSERT_EQ(values[2], 3, "Order: 3");
    
    return true;
}

FATP_TEST_CASE(intrusive_list_splice_at_begin)
{
    IntrusiveList<TestNode> list1;
    IntrusiveList<TestNode> list2;
    
    TestNode n1(1, "one");
    TestNode n2(2, "two");
    TestNode n3(3, "three");
    TestNode n4(4, "four");
    
    list1.push_back(n3);
    list1.push_back(n4);
    
    list2.push_back(n1);
    list2.push_back(n2);
    
    // Splice list2 at beginning of list1
    list1.splice(list1.begin(), list2);
    
    FATP_ASSERT_EQ(list1.size(), 4u, "list1 should have 4 elements");
    FATP_ASSERT_TRUE(list2.empty(), "list2 should be empty");
    
    std::vector<int> values;
    for (const auto& node : list1)
    {
        values.push_back(node.value);
    }
    
    FATP_ASSERT_EQ(values[0], 1, "Order: 1");
    FATP_ASSERT_EQ(values[1], 2, "Order: 2");
    FATP_ASSERT_EQ(values[2], 3, "Order: 3");
    FATP_ASSERT_EQ(values[3], 4, "Order: 4");
    
    return true;
}

FATP_TEST_CASE(intrusive_list_pop_empty)
{
    IntrusiveList<TestNode> list;
    
    // These should be safe no-ops
    list.pop_front();
    list.pop_back();
    
    FATP_ASSERT_TRUE(list.empty(), "List should remain empty");
    FATP_ASSERT_EQ(list.size(), 0u, "Size should be 0");
    
    return true;
}
```

### 2.2 Update Test Runner

```cpp
FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_single_element);
FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_remove_unlinked);
FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_is_linked);
FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_insert_at_begin);
FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_splice_at_begin);
FATP_RUN_TEST_NS(runner, intrusivelist, intrusive_list_pop_empty);
```

## Phase 3: Documentation (3h)

### 3.1 Create Overview Document

Create `/Documentation/IN WORK/IntrusiveList_Overview.md`:

```markdown
# IntrusiveList: Zero-Allocation Linked List

## Executive Summary

IntrusiveList is a doubly-linked list where nodes embed their own links,
eliminating all allocation overhead. Perfect for real-time systems, game
engines, and embedded applications where allocation latency is unacceptable.

## Key Benefits

1. **Zero Allocations** - Nodes contain their own links
2. **O(1) Operations** - Insert, remove, splice in constant time
3. **Cache Friendly** - Nodes are your actual objects
4. **Memory Efficient** - No separate node allocations

## When to Use IntrusiveList

✅ **Good for:**
- Real-time systems with strict latency requirements
- Game entity management
- Embedded systems with limited memory
- Linked structures where objects manage themselves
- Frequent insert/remove operations

❌ **Not ideal for:**
- Random access patterns (use vector)
- Objects that need to be in multiple lists (use SlotMap)
- Scenarios requiring copy semantics
```

### 3.2 Create User Manual

Create `/Documentation/IN WORK/IntrusiveList_User_Manual.md` with full API documentation.

## Checklist

| Task | Effort | Status |
|------|--------|--------|
| Add `[[nodiscard]]` attributes | 10m | ☐ |
| Add assertions to front/back | 15m | ☐ |
| Add `#include <cassert>` | 2m | ☐ |
| Add single_element test | 20m | ☐ |
| Add remove_unlinked test | 15m | ☐ |
| Add is_linked test | 15m | ☐ |
| Add insert_at_begin test | 20m | ☐ |
| Add splice_at_begin test | 20m | ☐ |
| Add pop_empty test | 10m | ☐ |
| Update test runner | 10m | ☐ |
| Create Overview document | 1.5h | ☐ |
| Create User Manual | 1.5h | ☐ |
| Run all tests | 15m | ☐ |
| Update FATP_META to `candidate` | 5m | ☐ |

---

# 3. SparseSet Action Plan

**Target:** `api_stability: candidate`  
**Effort:** ~10 hours  
**Risk:** Medium (significant test gaps)

## Phase 1: Code Fixes (1h)

### 1.1 Add `[[nodiscard]]` Attributes

```cpp
// SparseSet
[[nodiscard]] bool contains(T value) const { ... }
[[nodiscard]] size_type size() const noexcept { ... }
[[nodiscard]] bool empty() const noexcept { ... }
[[nodiscard]] size_type capacity() const noexcept { ... }
[[nodiscard]] T operator[](size_type index) const { ... }
[[nodiscard]] T at(size_type index) const { ... }

// SparseSetWithData
[[nodiscard]] bool contains(T value) const { ... }
[[nodiscard]] Data& get(T value) { ... }
[[nodiscard]] const Data& get(T value) const { ... }
[[nodiscard]] Data& data_at(size_type index) { ... }
[[nodiscard]] const Data& data_at(size_type index) const { ... }
[[nodiscard]] size_type size() const noexcept { ... }
[[nodiscard]] bool empty() const noexcept { ... }
```

### 1.2 Add Debug Assertions

```cpp
#include <cassert>  // Add to includes

T operator[](size_type index) const
{
    assert(index < m_dense.size() && "SparseSet::operator[]: index out of bounds");
    return m_dense[index];
}
```

### 1.3 Add Explicit Move Semantics

```cpp
// After constructors in SparseSet
SparseSet(SparseSet&&) noexcept = default;
SparseSet& operator=(SparseSet&&) noexcept = default;
SparseSet(const SparseSet&) = default;
SparseSet& operator=(const SparseSet&) = default;

// Same for SparseSetWithData
```

## Phase 2: Test Additions (6h)

### 2.1 Critical Missing Tests

```cpp
FATP_TEST_CASE(sparse_set_empty_operations)
{
    SparseSet<uint32_t> set;
    
    FATP_ASSERT_TRUE(set.empty(), "Should start empty");
    FATP_ASSERT_EQ(set.size(), 0u, "Size should be 0");
    FATP_ASSERT_TRUE(!set.contains(0), "Should not contain 0");
    FATP_ASSERT_TRUE(!set.contains(1000), "Should not contain 1000");
    FATP_ASSERT_TRUE(!set.erase(0), "Erase on empty should return false");
    
    return true;
}

FATP_TEST_CASE(sparse_set_single_element)
{
    SparseSet<uint32_t> set;
    
    FATP_ASSERT_TRUE(set.insert(42), "Insert should succeed");
    FATP_ASSERT_EQ(set.size(), 1u, "Size should be 1");
    FATP_ASSERT_TRUE(set.contains(42), "Should contain 42");
    FATP_ASSERT_EQ(set[0], 42u, "Dense[0] should be 42");
    FATP_ASSERT_EQ(set.at(0), 42u, "at(0) should be 42");
    
    FATP_ASSERT_TRUE(set.erase(42), "Erase should succeed");
    FATP_ASSERT_TRUE(set.empty(), "Should be empty after erase");
    
    return true;
}

FATP_TEST_CASE(sparse_set_duplicate_insert)
{
    SparseSet<uint32_t> set;
    
    FATP_ASSERT_TRUE(set.insert(100), "First insert should succeed");
    FATP_ASSERT_TRUE(!set.insert(100), "Duplicate insert should return false");
    FATP_ASSERT_EQ(set.size(), 1u, "Size should still be 1");
    
    return true;
}

FATP_TEST_CASE(sparse_set_at_bounds_check)
{
    SparseSet<uint32_t> set;
    set.insert(1);
    set.insert(2);
    
    FATP_ASSERT_EQ(set.at(0), 1u, "at(0) should work");
    FATP_ASSERT_EQ(set.at(1), 2u, "at(1) should work");
    
    FATP_ASSERT_THROWS(set.at(2), std::out_of_range, "at(2) should throw");
    FATP_ASSERT_THROWS(set.at(100), std::out_of_range, "at(100) should throw");
    
    return true;
}

FATP_TEST_CASE(sparse_set_clear)
{
    SparseSet<uint32_t> set;
    
    set.insert(1);
    set.insert(1000);
    set.insert(100000);
    
    FATP_ASSERT_EQ(set.size(), 3u, "Size should be 3");
    
    set.clear();
    
    FATP_ASSERT_TRUE(set.empty(), "Should be empty after clear");
    FATP_ASSERT_EQ(set.size(), 0u, "Size should be 0");
    FATP_ASSERT_TRUE(!set.contains(1), "Should not contain 1");
    FATP_ASSERT_TRUE(!set.contains(1000), "Should not contain 1000");
    
    // Should be able to reinsert
    FATP_ASSERT_TRUE(set.insert(1), "Reinsert should succeed");
    FATP_ASSERT_EQ(set.size(), 1u, "Size should be 1");
    
    return true;
}

FATP_TEST_CASE(sparse_set_reserve_capacity)
{
    SparseSet<uint32_t> set;
    
    FATP_ASSERT_EQ(set.capacity(), 0u, "Initial capacity should be 0");
    
    set.reserve(1000);
    FATP_ASSERT_TRUE(set.capacity() >= 1001, "Capacity should be at least 1001");
    FATP_ASSERT_TRUE(set.empty(), "Should still be empty");
    
    set.insert(999);
    FATP_ASSERT_TRUE(set.contains(999), "Should contain 999");
    
    return true;
}

FATP_TEST_CASE(sparse_set_dense_sparse_access)
{
    SparseSet<uint32_t> set;
    
    set.insert(100);
    set.insert(200);
    set.insert(300);
    
    const auto& dense = set.dense();
    FATP_ASSERT_EQ(dense.size(), 3u, "Dense should have 3 elements");
    
    const auto& sparse = set.sparse();
    FATP_ASSERT_TRUE(sparse.size() >= 301, "Sparse should accommodate max value");
    
    return true;
}

FATP_TEST_CASE(sparse_set_erase_last_element)
{
    SparseSet<uint32_t> set;
    
    set.insert(1);
    set.insert(2);
    set.insert(3);
    
    // Erase last element (no swap needed)
    FATP_ASSERT_TRUE(set.erase(3), "Erase last should succeed");
    FATP_ASSERT_EQ(set.size(), 2u, "Size should be 2");
    FATP_ASSERT_TRUE(set.contains(1), "Should still contain 1");
    FATP_ASSERT_TRUE(set.contains(2), "Should still contain 2");
    FATP_ASSERT_TRUE(!set.contains(3), "Should not contain 3");
    
    return true;
}

FATP_TEST_CASE(sparse_set_erase_preserves_others)
{
    SparseSet<uint32_t> set;
    
    set.insert(10);
    set.insert(20);
    set.insert(30);
    set.insert(40);
    set.insert(50);
    
    // Erase middle element
    set.erase(30);
    
    // All others should still be accessible
    FATP_ASSERT_TRUE(set.contains(10), "Should contain 10");
    FATP_ASSERT_TRUE(set.contains(20), "Should contain 20");
    FATP_ASSERT_TRUE(set.contains(40), "Should contain 40");
    FATP_ASSERT_TRUE(set.contains(50), "Should contain 50");
    FATP_ASSERT_EQ(set.size(), 4u, "Size should be 4");
    
    return true;
}

FATP_TEST_CASE(sparse_set_large_scale)
{
    SparseSet<uint32_t> set;
    
    // Insert 10000 elements
    for (uint32_t i = 0; i < 10000; ++i)
    {
        FATP_ASSERT_TRUE(set.insert(i * 100), "Insert should succeed");
    }
    
    FATP_ASSERT_EQ(set.size(), 10000u, "Size should be 10000");
    
    // Verify all present
    for (uint32_t i = 0; i < 10000; ++i)
    {
        FATP_ASSERT_TRUE(set.contains(i * 100), "Should contain element");
    }
    
    // Erase half
    for (uint32_t i = 0; i < 5000; ++i)
    {
        FATP_ASSERT_TRUE(set.erase(i * 100), "Erase should succeed");
    }
    
    FATP_ASSERT_EQ(set.size(), 5000u, "Size should be 5000");
    
    return true;
}

// SparseSetWithData tests

FATP_TEST_CASE(sparse_set_with_data_move_semantics)
{
    SparseSetWithData<uint32_t, std::string> set;
    
    std::string val = "test value";
    set.insert(1, std::move(val));
    
    FATP_ASSERT_TRUE(set.get(1) == "test value", "Value should be stored");
    // val is moved-from, don't check its state
    
    return true;
}

FATP_TEST_CASE(sparse_set_with_data_data_at)
{
    SparseSetWithData<uint32_t, std::string> set;
    
    set.insert(100, "first");
    set.insert(200, "second");
    
    FATP_ASSERT_TRUE(set.data_at(0) == "first", "data_at(0) should be first");
    FATP_ASSERT_TRUE(set.data_at(1) == "second", "data_at(1) should be second");
    
    FATP_ASSERT_THROWS(set.data_at(2), std::out_of_range, "data_at(2) should throw");
    
    return true;
}

FATP_TEST_CASE(sparse_set_with_data_get_throws)
{
    SparseSetWithData<uint32_t, std::string> set;
    
    set.insert(1, "one");
    
    FATP_ASSERT_THROWS(set.get(2), std::out_of_range, "get(2) should throw");
    FATP_ASSERT_THROWS(set.get(1000), std::out_of_range, "get(1000) should throw");
    
    return true;
}

FATP_TEST_CASE(sparse_set_type_trait)
{
    static_assert(is_sparse_set<SparseSet<uint32_t>>::value, "Should be sparse set");
    static_assert(!is_sparse_set<std::vector<int>>::value, "vector is not sparse set");
    
    return true;
}
```

### 2.2 Update Test Runner

```cpp
// Add all new tests
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_empty_operations);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_single_element);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_duplicate_insert);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_at_bounds_check);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_clear);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_reserve_capacity);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_dense_sparse_access);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_erase_last_element);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_erase_preserves_others);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_large_scale);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_with_data_move_semantics);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_with_data_data_at);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_with_data_get_throws);
FATP_RUN_TEST_NS(runner, sparseset, sparse_set_type_trait);
```

## Phase 3: Documentation (3h)

### 3.1 Create Overview Document

Create `/Documentation/IN WORK/SparseSet_Overview.md`

### 3.2 Create User Manual

Create `/Documentation/IN WORK/SparseSet_User_Manual.md`

## Checklist

| Task | Effort | Status |
|------|--------|--------|
| Add `[[nodiscard]]` to SparseSet | 15m | ☐ |
| Add `[[nodiscard]]` to SparseSetWithData | 15m | ☐ |
| Add assertion to `operator[]` | 10m | ☐ |
| Add explicit move semantics | 10m | ☐ |
| Add empty_operations test | 15m | ☐ |
| Add single_element test | 15m | ☐ |
| Add duplicate_insert test | 10m | ☐ |
| Add at_bounds_check test | 15m | ☐ |
| Add clear test | 15m | ☐ |
| Add reserve_capacity test | 15m | ☐ |
| Add dense_sparse_access test | 15m | ☐ |
| Add erase_last_element test | 15m | ☐ |
| Add erase_preserves_others test | 15m | ☐ |
| Add large_scale test | 20m | ☐ |
| Add with_data_move_semantics test | 15m | ☐ |
| Add with_data_data_at test | 15m | ☐ |
| Add with_data_get_throws test | 10m | ☐ |
| Add type_trait test | 10m | ☐ |
| Update test runner | 15m | ☐ |
| Create Overview document | 1.5h | ☐ |
| Create User Manual | 1.5h | ☐ |
| Run all tests | 15m | ☐ |
| Update FATP_META to `candidate` | 5m | ☐ |

---

# Summary

| Component | Code Fixes | New Tests | Documentation | Total Effort |
|-----------|------------|-----------|---------------|--------------|
| CircularBuffer | 30m | 1h | - | **2.5h** |
| IntrusiveList | 30m | 2h | 3h | **7h** |
| SparseSet | 1h | 4h | 3h | **10h** |
| **TOTAL** | **2h** | **7h** | **6h** | **~20h** |

## Priority Order

1. **SparseSet** - Most critical due to only 5 existing tests
2. **IntrusiveList** - Needs documentation and more tests
3. **CircularBuffer** - Already high quality, minor additions only
